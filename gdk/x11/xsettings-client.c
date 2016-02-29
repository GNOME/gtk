/*
 * Copyright © 2001, 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */

#include "config.h"

#include "xsettings-client.h"

#include <gdk/x11/gdkx11display.h>
#include <gdk/x11/gdkx11property.h>
#include <gdk/x11/gdkx11screen.h>
#include <gdk/x11/gdkx11window.h>
#include <gdk/x11/gdkprivate-x11.h>
#include <gdk/x11/gdkscreen-x11.h>

#include <gdkinternals.h>

#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD16 */

#include "gdksettings.c"

/* Types of settings possible. Enum values correspond to
 * protocol values.
 */
typedef enum 
{
  XSETTINGS_TYPE_INT     = 0,
  XSETTINGS_TYPE_STRING  = 1,
  XSETTINGS_TYPE_COLOR   = 2
} XSettingsType;

typedef struct _XSettingsBuffer  XSettingsBuffer;

struct _XSettingsBuffer
{
  char byte_order;
  size_t len;
  unsigned char *data;
  unsigned char *pos;
};

static void
gdk_xsettings_notify (GdkX11Screen     *x11_screen,
                      const char       *name,
		      GdkSettingAction  action)
{
  GdkEvent new_event;

  if (!g_str_has_prefix (name, "gtk-"))
    return;

  new_event.type = GDK_SETTING;
  new_event.setting.window = gdk_screen_get_root_window (GDK_SCREEN (x11_screen));
  new_event.setting.send_event = FALSE;
  new_event.setting.action = action;
  new_event.setting.name = (char*) name;

  gdk_event_put (&new_event);
}

static gboolean
value_equal (const GValue *value_a,
             const GValue *value_b)
{
  if (G_VALUE_TYPE (value_a) != G_VALUE_TYPE (value_b))
    return FALSE;

  switch (G_VALUE_TYPE (value_a))
    {
    case G_TYPE_INT:
      return g_value_get_int (value_a) == g_value_get_int (value_b);
    case XSETTINGS_TYPE_COLOR:
      return gdk_rgba_equal (g_value_get_boxed (value_a), g_value_get_boxed (value_b));
    case G_TYPE_STRING:
      return g_str_equal (g_value_get_string (value_a), g_value_get_string (value_b));
    default:
      g_warning ("unable to compare values of type %s", g_type_name (G_VALUE_TYPE (value_a)));
      return FALSE;
    }
}

static void
notify_changes (GdkX11Screen *x11_screen,
		GHashTable   *old_list)
{
  GHashTableIter iter;
  GValue *setting, *old_setting;
  const char *name;

  if (x11_screen->xsettings != NULL)
    {
      g_hash_table_iter_init (&iter, x11_screen->xsettings);
      while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer*) &setting))
	{
	  old_setting = old_list ? g_hash_table_lookup (old_list, name) : NULL;

	  if (old_setting == NULL)
	    gdk_xsettings_notify (x11_screen, name, GDK_SETTING_ACTION_NEW);
	  else if (!value_equal (setting, old_setting))
	    gdk_xsettings_notify (x11_screen, name, GDK_SETTING_ACTION_CHANGED);
	    
	  /* remove setting from old_list */
	  if (old_setting != NULL)
	    g_hash_table_remove (old_list, name);
	}
    }

  if (old_list != NULL)
    {
      /* old_list now contains only deleted settings */
      g_hash_table_iter_init (&iter, old_list);
      while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer*) &old_setting))
	gdk_xsettings_notify (x11_screen, name, GDK_SETTING_ACTION_DELETED);
    }
}

#define BYTES_LEFT(buffer) ((buffer)->data + (buffer)->len - (buffer)->pos)

#define return_if_fail_bytes(buffer, n_bytes) G_STMT_START{ \
  if (BYTES_LEFT (buffer) < (n_bytes)) \
    { \
      g_warning ("Invalid XSETTINGS property (read off end: Expected %u bytes, only %ld left", \
                 (n_bytes), BYTES_LEFT (buffer)); \
      return FALSE; \
    } \
}G_STMT_END

static gboolean
fetch_card16 (XSettingsBuffer *buffer,
	      CARD16          *result)
{
  CARD16 x;

  return_if_fail_bytes (buffer, 2);

  x = *(CARD16 *)buffer->pos;
  buffer->pos += 2;
  
  if (buffer->byte_order == MSBFirst)
    *result = GUINT16_FROM_BE (x);
  else
    *result = GUINT16_FROM_LE (x);

  return TRUE;
}

static gboolean
fetch_ushort (XSettingsBuffer *buffer,
	      unsigned short  *result) 
{
  CARD16 x;
  gboolean r;  

  r = fetch_card16 (buffer, &x);
  if (r)
    *result = x;

  return r;
}

static gboolean
fetch_card32 (XSettingsBuffer *buffer,
	      CARD32          *result)
{
  CARD32 x;

  return_if_fail_bytes (buffer, 4);

  x = *(CARD32 *)buffer->pos;
  buffer->pos += 4;
  
  if (buffer->byte_order == MSBFirst)
    *result = GUINT32_FROM_BE (x);
  else
    *result = GUINT32_FROM_LE (x);
  
  return TRUE;
}

static gboolean
fetch_card8 (XSettingsBuffer *buffer,
	     CARD8           *result)
{
  return_if_fail_bytes (buffer, 1);

  *result = *(CARD8 *)buffer->pos;
  buffer->pos += 1;

  return TRUE;
}

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

static gboolean
fetch_string (XSettingsBuffer  *buffer,
              guint             length,
              char            **result)
{
  guint pad_len;

  pad_len = XSETTINGS_PAD (length, 4);
  if (pad_len < length) /* guard against overflow */
    {
      g_warning ("Invalid XSETTINGS property (overflow in string length)");
      return FALSE;
    }

  return_if_fail_bytes (buffer, pad_len);

  *result = g_strndup ((char *) buffer->pos, length);
  buffer->pos += pad_len;

  return TRUE;
}

static void
free_value (gpointer data)
{
  GValue *value = data;

  g_value_unset (value);
  g_free (value);
}

static GHashTable *
parse_settings (unsigned char *data,
		size_t         len)
{
  XSettingsBuffer buffer;
  GHashTable *settings = NULL;
  CARD32 serial;
  CARD32 n_entries;
  CARD32 i;
  GValue *value = NULL;
  char *x_name = NULL;
  const char *gdk_name;
  
  buffer.pos = buffer.data = data;
  buffer.len = len;
  
  if (!fetch_card8 (&buffer, (unsigned char *)&buffer.byte_order))
    goto out;

  if (buffer.byte_order != MSBFirst &&
      buffer.byte_order != LSBFirst)
    {
      g_warning ("Invalid XSETTINGS property (unknown byte order %u)", buffer.byte_order);
      goto out;
    }

  buffer.pos += 3;

  if (!fetch_card32 (&buffer, &serial) ||
      !fetch_card32 (&buffer, &n_entries))
    goto out;

  GDK_NOTE(SETTINGS, g_message ("reading %u settings (serial %u byte order %u)", n_entries, serial, buffer.byte_order));

  for (i = 0; i < n_entries; i++)
    {
      CARD8 type;
      CARD16 name_len;
      CARD32 v_int;
      
      if (!fetch_card8 (&buffer, &type))
	goto out;

      buffer.pos += 1;

      if (!fetch_card16 (&buffer, &name_len))
	goto out;

      if (!fetch_string (&buffer, name_len, &x_name) ||
          /* last change serial (we ignore it) */
          !fetch_card32 (&buffer, &v_int))
	goto out;

      switch (type)
	{
	case XSETTINGS_TYPE_INT:
	  if (!fetch_card32 (&buffer, &v_int))
	    goto out;

          value = g_new0 (GValue, 1);
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, (gint32) v_int);

          GDK_NOTE(SETTINGS, g_message ("  %s = %d", x_name, (gint32) v_int));
	  break;
	case XSETTINGS_TYPE_STRING:
          {
            char *s;

            if (!fetch_card32 (&buffer, &v_int) ||
                !fetch_string (&buffer, v_int, &s))
              goto out;
            
            value = g_new0 (GValue, 1);
            g_value_init (value, G_TYPE_STRING);
            g_value_take_string (value, s);

            GDK_NOTE(SETTINGS, g_message ("  %s = \"%s\"", x_name, s));
          }
	  break;
	case XSETTINGS_TYPE_COLOR:
          {
            unsigned short red, green, blue, alpha;
            GdkRGBA rgba;

            if (!fetch_ushort (&buffer, &red) ||
                !fetch_ushort (&buffer, &green) ||
                !fetch_ushort (&buffer, &blue) ||
                !fetch_ushort (&buffer, &alpha))
              goto out;

            rgba.red = red / 65535.0;
            rgba.green = green / 65535.0;
            rgba.blue = blue / 65535.0;
            rgba.alpha = alpha / 65535.0;

            value = g_new0 (GValue, 1);
            g_value_init (value, G_TYPE_STRING);
            g_value_set_boxed (value, &rgba);

            GDK_NOTE(SETTINGS, g_message ("  %s = #%02X%02X%02X%02X", x_name, alpha,red, green, blue));
          }
	  break;
	default:
	  /* Quietly ignore unknown types */
          GDK_NOTE(SETTINGS, g_message ("  %s = ignored (unknown type %u)", x_name, type));
	  break;
	}

      gdk_name = gdk_from_xsettings_name (x_name);
      g_free (x_name);
      x_name = NULL;

      if (gdk_name == NULL)
        {
          GDK_NOTE(SETTINGS, g_message ("    ==> unknown to GTK"));
          free_value (value);
        }
      else
        {
          GDK_NOTE(SETTINGS, g_message ("    ==> storing as '%s'", gdk_name));

          if (settings == NULL)
            settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL,
                                              free_value);

          if (g_hash_table_lookup (settings, gdk_name) != NULL)
            {
              g_warning ("Invalid XSETTINGS property (Duplicate entry for '%s')", gdk_name);
              goto out;
            }

          g_hash_table_insert (settings, (gpointer) gdk_name, value);
        }

      value = NULL;
    }

  return settings;

 out:

  if (value)
    free_value (value);

  if (settings)
    g_hash_table_unref (settings);

  g_free (x_name);

  return NULL;
}

static void
read_settings (GdkX11Screen *x11_screen,
               gboolean      do_notify)
{
  GdkScreen *screen = GDK_SCREEN (x11_screen);

  Atom type;
  int format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *data;
  int result;

  GHashTable *old_list = x11_screen->xsettings;
  GValue value = G_VALUE_INIT;
  GValue *setting, *copy;

  x11_screen->xsettings = NULL;

  if (x11_screen->xsettings_manager_window)
    {
      GdkDisplay *display = x11_screen->display;
      Atom xsettings_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XSETTINGS_SETTINGS");

      gdk_x11_display_error_trap_push (display);
      result = XGetWindowProperty (gdk_x11_display_get_xdisplay (display),
                                   gdk_x11_window_get_xid (x11_screen->xsettings_manager_window),
				   xsettings_atom, 0, LONG_MAX,
				   False, xsettings_atom,
				   &type, &format, &n_items, &bytes_after, &data);
      gdk_x11_display_error_trap_pop_ignored (display);
      
      if (result == Success && type != None)
	{
	  if (type != xsettings_atom)
	    {
	      g_warning ("Invalid type for XSETTINGS property: %s", gdk_x11_get_xatom_name_for_display (display, type));
	    }
	  else if (format != 8)
	    {
	      g_warning ("Invalid format for XSETTINGS property: %d", format);
	    }
	  else
	    x11_screen->xsettings = parse_settings (data, n_items);
	  
	  XFree (data);
	}
    }

  /* Since we support scaling we look at the specific Gdk/UnscaledDPI
     setting if it exists and use that instead of Xft/DPI if it is set */
  if (x11_screen->xsettings && !x11_screen->fixed_window_scale)
    {
      setting = g_hash_table_lookup (x11_screen->xsettings, "gdk-unscaled-dpi");
      if (setting)
	{
	  copy = g_new0 (GValue, 1);
	  g_value_init (copy, G_VALUE_TYPE (setting));
	  g_value_copy (setting, copy);
	  g_hash_table_insert (x11_screen->xsettings, 
			       "gtk-xft-dpi", copy);
	}
    }

  if (do_notify)
    notify_changes (x11_screen, old_list);
  if (old_list)
    g_hash_table_unref (old_list);

  g_value_init (&value, G_TYPE_INT);

  if (!screen->resolution_set)
    {
      /* This code is duplicated with gtksettings.c:settings_update_resolution().
       * The update of the screen resolution needs to happen immediately when
       * gdk_x11_display_set_window_scale() is called, and not wait for events
       * to be processed, so we can't always handling it in gtksettings.c.
       * But we can't always handle it here because the DPI can be set through
       * GtkSettings, which we don't have access to.
       */
      int dpi_int = 0;
      double dpi;
      const char *scale_env;
      double scale;

      if (gdk_screen_get_setting (GDK_SCREEN (x11_screen),
                                  "gtk-xft-dpi", &value))
        dpi_int = g_value_get_int (&value);

      if (dpi_int > 0)
        dpi = dpi_int / 1024.;
      else
        dpi = -1.;

      scale_env = g_getenv ("GDK_DPI_SCALE");
      if (scale_env)
        {
          scale = g_ascii_strtod (scale_env, NULL);
          if (scale != 0 && dpi > 0)
            dpi *= scale;
        }

      _gdk_screen_set_resolution (screen, dpi);
    }

  if (!x11_screen->fixed_window_scale &&
      gdk_screen_get_setting (GDK_SCREEN (x11_screen),
			      "gdk-window-scaling-factor", &value))
    _gdk_x11_screen_set_window_scale (x11_screen,
				      g_value_get_int (&value));
}

static Atom
get_selection_atom (GdkX11Screen *x11_screen)
{
  return _gdk_x11_get_xatom_for_display_printf (x11_screen->display, "_XSETTINGS_S%d", x11_screen->screen_num);
}

static GdkFilterReturn
gdk_xsettings_manager_window_filter (GdkXEvent *xevent,
                                     GdkEvent  *event,
                                     gpointer   data);

static void
check_manager_window (GdkX11Screen *x11_screen,
                      gboolean      notify_changes)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window manager_window_xid;

  display = x11_screen->display;
  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (x11_screen->xsettings_manager_window)
    {
      gdk_window_remove_filter (x11_screen->xsettings_manager_window, gdk_xsettings_manager_window_filter, x11_screen);
      g_object_unref (x11_screen->xsettings_manager_window);
    }

  gdk_x11_display_grab (display);

  manager_window_xid = XGetSelectionOwner (xdisplay, get_selection_atom (x11_screen));
  x11_screen->xsettings_manager_window = gdk_x11_window_foreign_new_for_display (display,
                                                                   manager_window_xid);
  /* XXX: Can't use gdk_window_set_events() here because the first call to this
   * function happens too early in gdk_init() */
  if (x11_screen->xsettings_manager_window)
    XSelectInput (xdisplay,
                  gdk_x11_window_get_xid (x11_screen->xsettings_manager_window),
                  PropertyChangeMask | StructureNotifyMask);

  gdk_x11_display_ungrab (display);
  
  gdk_display_flush (display);

  if (x11_screen->xsettings_manager_window)
    {
      gdk_window_add_filter (x11_screen->xsettings_manager_window, gdk_xsettings_manager_window_filter, x11_screen);
    }
      
  read_settings (x11_screen, notify_changes);
}

static GdkFilterReturn
gdk_xsettings_root_window_filter (GdkXEvent *xevent,
                                  GdkEvent  *event,
                                  gpointer   data)
{
  GdkX11Screen *x11_screen = data;
  GdkDisplay *display = x11_screen->display;
  XEvent *xev = xevent;

  /* The checks here will not unlikely cause us to reread
   * the properties from the manager window a number of
   * times when the manager changes from A->B. But manager changes
   * are going to be pretty rare.
   */
  if (xev->xany.type == ClientMessage &&
      xev->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "MANAGER") &&
      xev->xclient.data.l[1] == get_selection_atom (x11_screen))
    {
      check_manager_window (x11_screen, TRUE);
      return GDK_FILTER_REMOVE;
    }
  
  return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
gdk_xsettings_manager_window_filter (GdkXEvent *xevent,
                                     GdkEvent  *event,
                                     gpointer   data)
{
  GdkX11Screen *x11_screen = data;
  XEvent *xev = xevent;

  if (xev->xany.type == DestroyNotify)
    {
      check_manager_window (x11_screen, TRUE);
      /* let GDK do its cleanup */
      return GDK_FILTER_CONTINUE; 
    }
  else if (xev->xany.type == PropertyNotify)
    {
      read_settings (x11_screen, TRUE);
      return GDK_FILTER_REMOVE;
    }
  
  return GDK_FILTER_CONTINUE;;
}

void
_gdk_x11_xsettings_init (GdkX11Screen *x11_screen)
{
  gdk_window_add_filter (gdk_screen_get_root_window (GDK_SCREEN (x11_screen)), gdk_xsettings_root_window_filter, x11_screen);

  check_manager_window (x11_screen, FALSE);
}

void
_gdk_x11_settings_force_reread (GdkX11Screen *x11_screen)
{
  read_settings (x11_screen, TRUE);
}

void
_gdk_x11_xsettings_finish (GdkX11Screen *x11_screen)
{
  gdk_window_remove_filter (gdk_screen_get_root_window (GDK_SCREEN (x11_screen)), gdk_xsettings_root_window_filter, x11_screen);
  if (x11_screen->xsettings_manager_window)
    {
      gdk_window_remove_filter (x11_screen->xsettings_manager_window, gdk_xsettings_manager_window_filter, x11_screen);
      g_object_unref (x11_screen->xsettings_manager_window);
      x11_screen->xsettings_manager_window = NULL;
    }
  
  if (x11_screen->xsettings)
    {
      g_hash_table_unref (x11_screen->xsettings);
      x11_screen->xsettings = NULL;
    }
}

