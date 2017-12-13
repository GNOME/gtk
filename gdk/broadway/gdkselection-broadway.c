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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"

#include <string.h>


void
_gdk_broadway_display_convert_selection (GdkDisplay *display,
					 GdkWindow *requestor,
					 GdkAtom    selection,
					 GdkAtom    target,
					 guint32    time)
{
  g_warning ("convert_selection not implemented");
}

gint
_gdk_broadway_display_get_selection_property (GdkDisplay *display,
					      GdkWindow  *requestor,
					      guchar    **data,
					      GdkAtom    *ret_type,
					      gint       *ret_format)
{
  if (ret_type)
    *ret_type = NULL;
  if (ret_format)
    *ret_format = 0;
  if (data)
    *data = NULL;

  g_warning ("get_selection_property not implemented");

  return 0;
}

void
_gdk_broadway_display_send_selection_notify (GdkDisplay      *display,
					     GdkWindow       *requestor,
					     GdkAtom          selection,
					     GdkAtom          target,
					     GdkAtom          property, 
					     guint32          time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  g_warning ("send_selection_notify not implemented");
}


static gint
make_list (const gchar  *text,
           gint          length,
           gboolean      latin1,
           gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;

      q = p;
      while (*q && q < text + length)
        q++;

      if (latin1)
        {
          str = g_convert (p, q - p,
                           "UTF-8", "ISO-8859-1",
                           NULL, NULL, &error);

          if (!str)
            {
              g_warning ("Error converting selection from STRING: %s",
                         error->message);
              g_error_free (error);
            }
        }
      else
        {
          str = g_strndup (p, q - p);
          if (!g_utf8_validate (str, -1, NULL))
            {
              g_warning ("Error converting selection from UTF8_STRING");
              g_free (str);
              str = NULL;
            }
        }

      if (str)
        {
          strings = g_slist_prepend (strings, str);
          n_strings++;
        }

      p = q + 1;
    }

  if (list)
    {
      *list = g_new (gchar *, n_strings + 1);
      (*list)[n_strings] = NULL;
    }

  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
        (*list)[--i] = tmp_list->data;
      else
        g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint 
_gdk_broadway_display_text_property_to_utf8_list (GdkDisplay    *display,
						  GdkAtom        encoding,
						  gint           format,
						  const guchar  *text,
						  gint           length,
						  gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == gdk_atom_intern_static_string ("UTF8_STRING"))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  
  if (list)
    *list = NULL;
  return 0;
}

gchar *
_gdk_broadway_display_utf8_to_string_target (GdkDisplay  *display,
					     const gchar *str)
{
  return g_strdup (str);
}
