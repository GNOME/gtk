/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkselection.h"

#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"

#include <string.h>


typedef struct _OwnerInfo OwnerInfo;

struct _OwnerInfo
{
  GdkAtom    selection;
  GdkWindow *owner;
  gulong     serial;
};

static GSList *owner_list;

/* When a window is destroyed we check if it is the owner
 * of any selections. This is somewhat inefficient, but
 * owner_list is typically short, and it is a low memory,
 * low code solution
 */
void
_gdk_selection_window_destroyed (GdkWindow *window)
{
  GSList *tmp_list = owner_list;
  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;
      tmp_list = tmp_list->next;

      if (info->owner == window)
	{
	  owner_list = g_slist_remove (owner_list, info);
	  g_free (info);
	}
    }
}

gboolean
gdk_selection_owner_set_for_display (GdkDisplay *display,
				     GdkWindow  *owner,
				     GdkAtom     selection,
				     guint32     time,
				     gboolean    send_event)
{
  return FALSE;
}

GdkWindow *
gdk_selection_owner_get_for_display (GdkDisplay *display,
				     GdkAtom     selection)
{
  return NULL;
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  if (ret_type)
    *ret_type = GDK_NONE;
  if (ret_format)
    *ret_format = 0;
  if (data)
    *data = NULL;

  return 0;
}

void
gdk_selection_send_notify_for_display (GdkDisplay       *display,
				       GdkNativeWindow  requestor,
				       GdkAtom          selection,
				       GdkAtom          target,
				       GdkAtom          property, 
				       guint32          time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

gint
gdk_text_property_to_text_list_for_display (GdkDisplay   *display,
					    GdkAtom       encoding,
					    gint          format, 
					    const guchar *text,
					    gint          length,
					    gchar      ***list)
{
  return 0;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  g_strfreev (list);
}

gint 
gdk_text_property_to_utf8_list_for_display (GdkDisplay    *display,
					    GdkAtom        encoding,
					    gint           format,
					    const guchar  *text,
					    gint           length,
					    gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 0;
}

gint
gdk_string_to_compound_text_for_display (GdkDisplay  *display,
					 const gchar *str,
					 GdkAtom     *encoding,
					 gint        *format,
					 guchar     **ctext,
					 gint        *length)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return 1;
}

gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  return g_strdup (str);
}

gboolean
gdk_utf8_to_compound_text_for_display (GdkDisplay  *display,
				       const gchar *str,
				       GdkAtom     *encoding,
				       gint        *format,
				       guchar     **ctext,
				       gint        *length)
{
  return FALSE;
}

void gdk_free_compound_text (guchar *ctext)
{
  g_free (ctext);
}
