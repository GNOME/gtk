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
#include <gdk/x11/gdkscreen-x11.h>

#include <gdkinternals.h>

#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD16 */

#include "gdksettings.c"

typedef struct _XSettingsBuffer  XSettingsBuffer;

struct _XSettingsBuffer
{
  char byte_order;
  size_t len;
  unsigned char *data;
  unsigned char *pos;
};

struct _XSettingsClient
{
  GdkScreen *screen;
  Display *display;

  Window manager_window;
  Atom selection_atom;

  GHashTable *settings; /* string of GDK settings name => XSettingsSetting */
};

static void
gdk_xsettings_notify (const char       *name,
		      GdkSettingAction  action,
		      XSettingsSetting *setting,
		      GdkScreen        *screen)
{
  GdkEvent new_event;
  GdkX11Screen *x11_screen = GDK_X11_SCREEN (screen);

  if (x11_screen->xsettings_in_init)
    return;
  
  new_event.type = GDK_SETTING;
  new_event.setting.window = gdk_screen_get_root_window (screen);
  new_event.setting.send_event = FALSE;
  new_event.setting.action = action;
  new_event.setting.name = (char*) name;

  gdk_event_put (&new_event);
}

static void
notify_changes (XSettingsClient *client,
		GHashTable      *old_list)
{
  GHashTableIter iter;
  XSettingsSetting *setting, *old_setting;
  const char *name;

  if (client->settings != NULL)
    {
      g_hash_table_iter_init (&iter, client->settings);
      while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer*) &setting))
	{
	  old_setting = old_list ? g_hash_table_lookup (old_list, name) : NULL;

	  if (old_setting == NULL)
	    gdk_xsettings_notify (name, GDK_SETTING_ACTION_NEW, setting, client->screen);
	  else if (!xsettings_setting_equal (setting, old_setting))
	    gdk_xsettings_notify (name, GDK_SETTING_ACTION_CHANGED, setting, client->screen);
	    
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
	gdk_xsettings_notify (name, GDK_SETTING_ACTION_DELETED, NULL, client->screen);
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

static GHashTable *
parse_settings (unsigned char *data,
		size_t         len)
{
  XSettingsBuffer buffer;
  GHashTable *settings = NULL;
  CARD32 serial;
  CARD32 n_entries;
  CARD32 i;
  XSettingsSetting *setting = NULL;
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

  GDK_NOTE(SETTINGS, g_print("reading %u settings (serial %u byte order %u)\n", n_entries, serial, buffer.byte_order));

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

      setting = g_new (XSettingsSetting, 1);
      setting->type = XSETTINGS_TYPE_INT; /* No allocated memory */

      if (!fetch_string (&buffer, name_len, &x_name) ||
          /* last change serial (we ignore it) */
          !fetch_card32 (&buffer, &v_int))
	goto out;

      switch (type)
	{
	case XSETTINGS_TYPE_INT:
	  if (!fetch_card32 (&buffer, &v_int))
	    goto out;

	  setting->data.v_int = (INT32)v_int;
          GDK_NOTE(SETTINGS, g_print("  %s = %d\n", x_name, (gint) setting->data.v_int));
	  break;
	case XSETTINGS_TYPE_STRING:
	  if (!fetch_card32 (&buffer, &v_int) ||
              !fetch_string (&buffer, v_int, &setting->data.v_string))
	    goto out;
	  
          GDK_NOTE(SETTINGS, g_print("  %s = \"%s\"\n", x_name, setting->data.v_string));
	  break;
	case XSETTINGS_TYPE_COLOR:
	  if (!fetch_ushort (&buffer, &setting->data.v_color.red) ||
	      !fetch_ushort (&buffer, &setting->data.v_color.green) ||
	      !fetch_ushort (&buffer, &setting->data.v_color.blue) ||
	      !fetch_ushort (&buffer, &setting->data.v_color.alpha))
	    goto out;

          GDK_NOTE(SETTINGS, g_print("  %s = #%02X%02X%02X%02X\n", x_name, 
                                 setting->data.v_color.alpha, setting->data.v_color.red,
                                 setting->data.v_color.green, setting->data.v_color.blue));
	  break;
	default:
	  /* Quietly ignore unknown types */
          GDK_NOTE(SETTINGS, g_print("  %s = ignored (unknown type %u)\n", x_name, type));
	  break;
	}

      setting->type = type;

      gdk_name = gdk_from_xsettings_name (x_name);
      g_free (x_name);
      x_name = NULL;

      if (gdk_name == NULL)
        {
          GDK_NOTE(SETTINGS, g_print("    ==> unknown to GTK\n"));
        }
      else
        {
          GDK_NOTE(SETTINGS, g_print("    ==> storing as '%s'\n", gdk_name));

          if (settings == NULL)
            settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL,
                                              (GDestroyNotify) xsettings_setting_free);

          if (g_hash_table_lookup (settings, gdk_name) != NULL)
            {
              g_warning ("Invalid XSETTINGS property (Duplicate entry for '%s')", gdk_name);
              goto out;
            }

          g_hash_table_insert (settings, (gpointer) gdk_name, setting);
        }

      setting = NULL;
    }

  return settings;

 out:

  if (setting)
    xsettings_setting_free (setting);

  if (settings)
    g_hash_table_unref (settings);

  g_free (x_name);

  return NULL;
}

static void
read_settings (XSettingsClient *client)
{
  Atom type;
  int format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *data;
  int result;

  GHashTable *old_list = client->settings;

  client->settings = NULL;

  if (client->manager_window)
    {
      GdkDisplay *display = gdk_screen_get_display (client->screen);
      Atom xsettings_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XSETTINGS_SETTINGS");

      gdk_x11_display_error_trap_push (display);
      result = XGetWindowProperty (client->display, client->manager_window,
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
	    client->settings = parse_settings (data, n_items);
	  
	  XFree (data);
	}
    }

  notify_changes (client, old_list);
  if (old_list)
    g_hash_table_unref (old_list);
}

static Bool
gdk_xsettings_watch (Window     window,
		     Bool       is_start,
		     GdkScreen *screen);

static void
check_manager_window (XSettingsClient *client)
{
  if (client->manager_window)
    gdk_xsettings_watch (client->manager_window, False, client->screen);

  gdk_x11_display_grab (gdk_screen_get_display (client->screen));

  client->manager_window = XGetSelectionOwner (client->display,
					       client->selection_atom);
  if (client->manager_window)
    XSelectInput (client->display, client->manager_window,
		  PropertyChangeMask | StructureNotifyMask);

  gdk_x11_display_ungrab (gdk_screen_get_display (client->screen));
  
  XFlush (client->display);

  if (client->manager_window)
    {
      if (!gdk_xsettings_watch (client->manager_window, True,  client->screen))
	{
	  /* Inability to watch the window probably means that it was destroyed
	   * after we ungrabbed
	   */
	  client->manager_window = None;
	  return;
	}
    }
      
  read_settings (client);
}

static GdkFilterReturn
gdk_xsettings_client_event_filter (GdkXEvent *xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  GdkScreen *screen = data;
  GdkDisplay *display = gdk_screen_get_display (screen);
  XSettingsClient *client = GDK_X11_SCREEN (screen)->xsettings_client;
  XEvent *xev = xevent;

  /* The checks here will not unlikely cause us to reread
   * the properties from the manager window a number of
   * times when the manager changes from A->B. But manager changes
   * are going to be pretty rare.
   */
  if (xev->xany.window == gdk_x11_window_get_xid (gdk_screen_get_root_window (screen)))
    {
      if (xev->xany.type == ClientMessage &&
	  xev->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "MANAGER") &&
	  xev->xclient.data.l[1] == client->selection_atom)
	{
	  check_manager_window (client);
	  return GDK_FILTER_REMOVE;
	}
    }
  else if (xev->xany.window == client->manager_window)
    {
      if (xev->xany.type == DestroyNotify)
	{
	  check_manager_window (client);
          /* let GDK do its cleanup */
	  return GDK_FILTER_CONTINUE; 
	}
      else if (xev->xany.type == PropertyNotify)
	{
	  read_settings (client);
	  return GDK_FILTER_REMOVE;
	}
    }
  
  return GDK_FILTER_CONTINUE;;
}

static Bool
gdk_xsettings_watch (Window     window,
		     Bool       is_start,
		     GdkScreen *screen)
{
  GdkWindow *gdkwin;

  gdkwin = gdk_x11_window_lookup_for_display (gdk_screen_get_display (screen), window);

  if (is_start)
    {
      if (gdkwin)
	g_object_ref (gdkwin);
      else
	{
	  gdkwin = gdk_x11_window_foreign_new_for_display (gdk_screen_get_display (screen), window);
	  
	  /* gdk_window_foreign_new_for_display() can fail and return NULL if the
	   * window has already been destroyed.
	   */
	  if (!gdkwin)
	    return False;
	}

      gdk_window_add_filter (gdkwin, gdk_xsettings_client_event_filter, screen);
    }
  else
    {
      if (!gdkwin)
	{
	  /* gdkwin should not be NULL here, since if starting the watch succeeded
	   * we have a reference on the window. It might mean that the caller didn't
	   * remove the watch when it got a DestroyNotify event. Or maybe the
	   * caller ignored the return value when starting the watch failed.
	   */
	  g_warning ("gdk_xsettings_watch_cb(): Couldn't find window to unwatch");
	  return False;
	}
      
      gdk_window_remove_filter (gdkwin, gdk_xsettings_client_event_filter, screen);
      g_object_unref (gdkwin);
    }

  return True;
}

XSettingsClient *
xsettings_client_new (GdkScreen *screen)
{
  XSettingsClient *client;
  char *selection_atom_name;
  
  client = g_new (XSettingsClient, 1);
  if (!client)
    return NULL;

  client->screen = screen;
  client->display = gdk_x11_display_get_xdisplay (gdk_screen_get_display (screen));
  client->screen = screen;
  client->manager_window = None;
  client->settings = NULL;

  selection_atom_name = g_strdup_printf ("_XSETTINGS_S%d", gdk_x11_screen_get_screen_number (screen));
  client->selection_atom = gdk_x11_get_xatom_by_name_for_display (gdk_screen_get_display (screen), selection_atom_name);
  g_free (selection_atom_name);

  gdk_xsettings_watch (gdk_x11_window_get_xid (gdk_screen_get_root_window (screen)), True, client->screen);

  check_manager_window (client);

  return client;
}

void
xsettings_client_destroy (XSettingsClient *client)
{
  gdk_xsettings_watch (gdk_x11_window_get_xid (gdk_screen_get_root_window (client->screen)), False, client->screen);
  if (client->manager_window)
    gdk_xsettings_watch (client->manager_window, False, client->screen);
  
  if (client->settings)
    g_hash_table_unref (client->settings);
  g_free (client);
}

const XSettingsSetting *
xsettings_client_get_setting (XSettingsClient   *client,
			      const char        *name)
{
  return g_hash_table_lookup (client->settings, name);
}

int
xsettings_setting_equal (XSettingsSetting *setting_a,
			 XSettingsSetting *setting_b)
{
  if (setting_a->type != setting_b->type)
    return 0;

  switch (setting_a->type)
    {
    case XSETTINGS_TYPE_INT:
      return setting_a->data.v_int == setting_b->data.v_int;
    case XSETTINGS_TYPE_COLOR:
      return (setting_a->data.v_color.red == setting_b->data.v_color.red &&
	      setting_a->data.v_color.green == setting_b->data.v_color.green &&
	      setting_a->data.v_color.blue == setting_b->data.v_color.blue &&
	      setting_a->data.v_color.alpha == setting_b->data.v_color.alpha);
    case XSETTINGS_TYPE_STRING:
      return strcmp (setting_a->data.v_string, setting_b->data.v_string) == 0;
    }

  return 0;
}

void
xsettings_setting_free (XSettingsSetting *setting)
{
  if (setting->type == XSETTINGS_TYPE_STRING)
    g_free (setting->data.v_string);

  g_free (setting);
}

