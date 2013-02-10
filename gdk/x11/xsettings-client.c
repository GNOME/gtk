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
#include <gdk/x11/gdkx11screen.h>
#include <gdk/x11/gdkx11window.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD16 */

struct _XSettingsClient
{
  GdkScreen *screen;
  Display *display;
  XSettingsNotifyFunc notify;
  XSettingsWatchFunc watch;
  void *cb_data;

  XSettingsGrabFunc grab;
  XSettingsGrabFunc ungrab;

  Window manager_window;
  Atom manager_atom;
  Atom selection_atom;
  Atom xsettings_atom;

  GHashTable *settings; /* string => XSettingsSetting */
};

static void
notify_changes (XSettingsClient *client,
		GHashTable      *old_list)
{
  GHashTableIter iter;
  XSettingsSetting *setting, *old_setting;

  if (!client->notify)
    return;

  if (client->settings != NULL)
    {
      g_hash_table_iter_init (&iter, client->settings);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &setting))
	{
	  old_setting = old_list ? g_hash_table_lookup (old_list, setting->name) : NULL;

	  if (old_setting == NULL)
	    client->notify (setting->name, XSETTINGS_ACTION_NEW, setting, client->cb_data);
	  else if (!xsettings_setting_equal (setting, old_setting))
	    client->notify (setting->name, XSETTINGS_ACTION_CHANGED, setting, client->cb_data);
	    
	  /* remove setting from old_list */
	  if (old_setting != NULL)
	    g_hash_table_remove (old_list, setting->name);
	}
    }

  if (old_list != NULL)
    {
      /* old_list now contains only deleted settings */
      g_hash_table_iter_init (&iter, old_list);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &old_setting))
	client->notify (old_setting->name, XSETTINGS_ACTION_DELETED, NULL, client->cb_data);
    }
}

static int
ignore_errors (Display *display, XErrorEvent *event)
{
  return True;
}

#define BYTES_LEFT(buffer) ((buffer)->data + (buffer)->len - (buffer)->pos)

static XSettingsResult
fetch_card16 (XSettingsBuffer *buffer,
	      CARD16          *result)
{
  CARD16 x;

  if (BYTES_LEFT (buffer) < 2)
    return XSETTINGS_ACCESS;

  x = *(CARD16 *)buffer->pos;
  buffer->pos += 2;
  
  if (buffer->byte_order == MSBFirst)
    *result = GUINT16_FROM_BE (x);
  else
    *result = GUINT16_FROM_LE (x);

  return XSETTINGS_SUCCESS;
}

static XSettingsResult
fetch_ushort (XSettingsBuffer *buffer,
	      unsigned short  *result) 
{
  CARD16 x;
  XSettingsResult r;  

  r = fetch_card16 (buffer, &x);
  if (r == XSETTINGS_SUCCESS)
    *result = x;

  return r;
}

static XSettingsResult
fetch_card32 (XSettingsBuffer *buffer,
	      CARD32          *result)
{
  CARD32 x;

  if (BYTES_LEFT (buffer) < 4)
    return XSETTINGS_ACCESS;

  x = *(CARD32 *)buffer->pos;
  buffer->pos += 4;
  
  if (buffer->byte_order == MSBFirst)
    *result = GUINT32_FROM_BE (x);
  else
    *result = GUINT32_FROM_LE (x);
  
  return XSETTINGS_SUCCESS;
}

static XSettingsResult
fetch_card8 (XSettingsBuffer *buffer,
	     CARD8           *result)
{
  if (BYTES_LEFT (buffer) < 1)
    return XSETTINGS_ACCESS;

  *result = *(CARD8 *)buffer->pos;
  buffer->pos += 1;

  return XSETTINGS_SUCCESS;
}

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

static GHashTable *
parse_settings (unsigned char *data,
		size_t         len)
{
  XSettingsBuffer buffer;
  XSettingsResult result = XSETTINGS_SUCCESS;
  GHashTable *settings = NULL;
  CARD32 serial;
  CARD32 n_entries;
  CARD32 i;
  XSettingsSetting *setting = NULL;
  
  buffer.pos = buffer.data = data;
  buffer.len = len;
  
  result = fetch_card8 (&buffer, (unsigned char *)&buffer.byte_order);
  if (buffer.byte_order != MSBFirst &&
      buffer.byte_order != LSBFirst)
    {
      fprintf (stderr, "Invalid byte order in XSETTINGS property\n");
      result = XSETTINGS_FAILED;
      goto out;
    }

  buffer.pos += 3;

  result = fetch_card32 (&buffer, &serial);
  if (result != XSETTINGS_SUCCESS)
    goto out;

  result = fetch_card32 (&buffer, &n_entries);
  if (result != XSETTINGS_SUCCESS)
    goto out;

  for (i = 0; i < n_entries; i++)
    {
      CARD8 type;
      CARD16 name_len;
      CARD32 v_int;
      size_t pad_len;
      
      result = fetch_card8 (&buffer, &type);
      if (result != XSETTINGS_SUCCESS)
	goto out;

      buffer.pos += 1;

      result = fetch_card16 (&buffer, &name_len);
      if (result != XSETTINGS_SUCCESS)
	goto out;

      pad_len = XSETTINGS_PAD(name_len, 4);
      if (BYTES_LEFT (&buffer) < pad_len)
	{
	  result = XSETTINGS_ACCESS;
	  goto out;
	}

      setting = malloc (sizeof *setting);
      if (!setting)
	{
	  result = XSETTINGS_NO_MEM;
	  goto out;
	}
      setting->type = XSETTINGS_TYPE_INT; /* No allocated memory */

      setting->name = malloc (name_len + 1);
      if (!setting->name)
	{
	  result = XSETTINGS_NO_MEM;
	  goto out;
	}

      memcpy (setting->name, buffer.pos, name_len);
      setting->name[name_len] = '\0';
      buffer.pos += pad_len;

      /* last change serial (we ignore it) */
      result = fetch_card32 (&buffer, &v_int);
      if (result != XSETTINGS_SUCCESS)
	goto out;

      switch (type)
	{
	case XSETTINGS_TYPE_INT:
	  result = fetch_card32 (&buffer, &v_int);
	  if (result != XSETTINGS_SUCCESS)
	    goto out;

	  setting->data.v_int = (INT32)v_int;
	  break;
	case XSETTINGS_TYPE_STRING:
	  result = fetch_card32 (&buffer, &v_int);
	  if (result != XSETTINGS_SUCCESS)
	    goto out;

	  pad_len = XSETTINGS_PAD (v_int, 4);
	  if (v_int + 1 == 0 || /* Guard against wrap-around */
	      BYTES_LEFT (&buffer) < pad_len)
	    {
	      result = XSETTINGS_ACCESS;
	      goto out;
	    }

	  setting->data.v_string = malloc (v_int + 1);
	  if (!setting->data.v_string)
	    {
	      result = XSETTINGS_NO_MEM;
	      goto out;
	    }
	  
	  memcpy (setting->data.v_string, buffer.pos, v_int);
	  setting->data.v_string[v_int] = '\0';
	  buffer.pos += pad_len;

	  break;
	case XSETTINGS_TYPE_COLOR:
	  result = fetch_ushort (&buffer, &setting->data.v_color.red);
	  if (result != XSETTINGS_SUCCESS)
	    goto out;
	  result = fetch_ushort (&buffer, &setting->data.v_color.green);
	  if (result != XSETTINGS_SUCCESS)
	    goto out;
	  result = fetch_ushort (&buffer, &setting->data.v_color.blue);
	  if (result != XSETTINGS_SUCCESS)
	    goto out;
	  result = fetch_ushort (&buffer, &setting->data.v_color.alpha);
	  if (result != XSETTINGS_SUCCESS)
	    goto out;

	  break;
	default:
	  /* Quietly ignore unknown types */
	  break;
	}

      setting->type = type;

      if (settings == NULL)
        settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          NULL,
                                          (GDestroyNotify) xsettings_setting_free);

      if (g_hash_table_lookup (settings, setting->name) != NULL)
        {
          result = XSETTINGS_DUPLICATE_ENTRY;
          goto out;
        }

      g_hash_table_insert (settings, setting->name, setting);
      setting = NULL;
    }

 out:

  if (result != XSETTINGS_SUCCESS)
    {
      switch (result)
	{
	case XSETTINGS_NO_MEM:
	  fprintf(stderr, "Out of memory reading XSETTINGS property\n");
	  break;
	case XSETTINGS_ACCESS:
	  fprintf(stderr, "Invalid XSETTINGS property (read off end)\n");
	  break;
	case XSETTINGS_DUPLICATE_ENTRY:
	  fprintf (stderr, "Duplicate XSETTINGS entry for '%s'\n", setting->name);
	case XSETTINGS_FAILED:
	case XSETTINGS_SUCCESS:
	case XSETTINGS_NO_ENTRY:
	  break;
	}

      if (setting)
	xsettings_setting_free (setting);

      if (settings)
        g_hash_table_unref (settings);
      settings = NULL;
    }

  return settings;
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

  int (*old_handler) (Display *, XErrorEvent *);
  
  GHashTable *old_list = client->settings;

  client->settings = NULL;

  if (client->manager_window)
    {
      old_handler = XSetErrorHandler (ignore_errors);
      result = XGetWindowProperty (client->display, client->manager_window,
				   client->xsettings_atom, 0, LONG_MAX,
				   False, client->xsettings_atom,
				   &type, &format, &n_items, &bytes_after, &data);
      XSetErrorHandler (old_handler);
      
      if (result == Success && type != None)
	{
	  if (type != client->xsettings_atom)
	    {
	      fprintf (stderr, "Invalid type for XSETTINGS property");
	    }
	  else if (format != 8)
	    {
	      fprintf (stderr, "Invalid format for XSETTINGS property %d", format);
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

static void
add_events (Display *display,
	    Window   window,
	    long     mask)
{
  XWindowAttributes attr;

  XGetWindowAttributes (display, window, &attr);
  XSelectInput (display, window, attr.your_event_mask | mask);
}

static void
check_manager_window (XSettingsClient *client)
{
  if (client->manager_window && client->watch)
    client->watch (client->manager_window, False, 0, client->cb_data);

  if (client->grab)
    client->grab (client->display);
  else
    XGrabServer (client->display);

  client->manager_window = XGetSelectionOwner (client->display,
					       client->selection_atom);
  if (client->manager_window)
    XSelectInput (client->display, client->manager_window,
		  PropertyChangeMask | StructureNotifyMask);

  if (client->ungrab)
    client->ungrab (client->display);
  else
    XUngrabServer (client->display);
  
  XFlush (client->display);

  if (client->manager_window && client->watch)
    {
      if (!client->watch (client->manager_window, True, 
			  PropertyChangeMask | StructureNotifyMask,
			  client->cb_data))
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

XSettingsClient *
xsettings_client_new (GdkScreen           *screen,
		      XSettingsNotifyFunc  notify,
		      XSettingsWatchFunc   watch,
		      void                *cb_data,
		      XSettingsGrabFunc    grab,
		      XSettingsGrabFunc    ungrab)
{
  XSettingsClient *client;
  char buffer[256];
  char *atom_names[3];
  Atom atoms[3];
  
  client = malloc (sizeof *client);
  if (!client)
    return NULL;

  client->screen = screen;
  client->display = gdk_x11_display_get_xdisplay (gdk_screen_get_display (screen));
  client->notify = notify;
  client->watch = watch;
  client->cb_data = cb_data;
  client->grab = grab;
  client->ungrab = ungrab;
  client->manager_window = None;
  client->settings = NULL;

  sprintf(buffer, "_XSETTINGS_S%d", gdk_x11_screen_get_screen_number (screen));
  atom_names[0] = buffer;
  atom_names[1] = "_XSETTINGS_SETTINGS";
  atom_names[2] = "MANAGER";

  XInternAtoms (client->display, atom_names, 3, False, atoms);

  client->selection_atom = atoms[0];
  client->xsettings_atom = atoms[1];
  client->manager_atom = atoms[2];

  /* Select on StructureNotify so we get MANAGER events
   */
  add_events (client->display, gdk_x11_window_get_xid (gdk_screen_get_root_window (screen)), StructureNotifyMask);

  if (client->watch)
    client->watch (gdk_x11_window_get_xid (gdk_screen_get_root_window (screen)), True, StructureNotifyMask,
		   client->cb_data);

  check_manager_window (client);

  return client;
}


void
xsettings_client_set_grab_func   (XSettingsClient      *client,
				  XSettingsGrabFunc     grab)
{
  client->grab = grab;
}

void
xsettings_client_set_ungrab_func (XSettingsClient      *client,
				  XSettingsGrabFunc     ungrab)
{
  client->ungrab = ungrab;
}

void
xsettings_client_destroy (XSettingsClient *client)
{
  if (client->watch)
    client->watch (gdk_x11_window_get_xid (gdk_screen_get_root_window (client->screen)),
		   False, 0, client->cb_data);
  if (client->manager_window && client->watch)
    client->watch (client->manager_window, False, 0, client->cb_data);
  
  if (client->settings)
    g_hash_table_unref (client->settings);
  free (client);
}

const XSettingsSetting *
xsettings_client_get_setting (XSettingsClient   *client,
			      const char        *name)
{
  return g_hash_table_lookup (client->settings, name);
}

Bool
xsettings_client_process_event (XSettingsClient *client,
				XEvent          *xev)
{
  /* The checks here will not unlikely cause us to reread
   * the properties from the manager window a number of
   * times when the manager changes from A->B. But manager changes
   * are going to be pretty rare.
   */
  if (xev->xany.window == gdk_x11_window_get_xid (gdk_screen_get_root_window (client->screen)))
    {
      if (xev->xany.type == ClientMessage &&
	  xev->xclient.message_type == client->manager_atom &&
	  xev->xclient.data.l[1] == client->selection_atom)
	{
	  check_manager_window (client);
	  return True;
	}
    }
  else if (xev->xany.window == client->manager_window)
    {
      if (xev->xany.type == DestroyNotify)
	{
	  check_manager_window (client);
          /* let GDK do its cleanup */
	  return False; 
	}
      else if (xev->xany.type == PropertyNotify)
	{
	  read_settings (client);
	  return True;
	}
    }
  
  return False;
}

int
xsettings_setting_equal (XSettingsSetting *setting_a,
			 XSettingsSetting *setting_b)
{
  if (setting_a->type != setting_b->type)
    return 0;

  if (strcmp (setting_a->name, setting_b->name) != 0)
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
    free (setting->data.v_string);

  if (setting->name)
    free (setting->name);
  
  free (setting);
}

