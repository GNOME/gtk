/* gdkselection-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2005 Imendio AB
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

#include "config.h"

#include "gdkselection.h"
#include "gdkproperty.h"
#include "gdkquartz.h"

gboolean
_gdk_quartz_display_set_selection_owner (GdkDisplay *display,
                                         GdkWindow  *owner,
                                         GdkAtom     selection,
                                         guint32     time,
                                         gint        send_event)
{
  /* FIXME: Implement */
  return TRUE;
}

GdkWindow*
_gdk_quartz_display_get_selection_owner (GdkDisplay *display,
                                         GdkAtom     selection)
{
  /* FIXME: Implement */
  return NULL;
}

void
_gdk_quartz_display_convert_selection (GdkDisplay *display,
                                       GdkWindow  *requestor,
                                       GdkAtom     selection,
                                       GdkAtom     target,
                                       guint32     time)
{
  /* FIXME: Implement */
}

gint
_gdk_quartz_display_get_selection_property (GdkDisplay *display,
                                            GdkWindow  *requestor,
                                            guchar    **data,
                                            GdkAtom    *ret_type,
                                            gint       *ret_format)
{
  /* FIXME: Implement */
  return 0;
}

gchar *
_gdk_quartz_display_utf8_to_string_target (GdkDisplay  *display,
                                           const gchar *str)
{
  /* FIXME: Implement */
  return NULL;
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
	str = g_strndup (p, q - p);

      if (str)
	{
	  strings = g_slist_prepend (strings, str);
	  n_strings++;
	}

      p = q + 1;
    }

  if (list)
    *list = g_new0 (gchar *, n_strings + 1);

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
_gdk_quartz_display_text_property_to_utf8_list (GdkDisplay    *display,
                                                GdkAtom        encoding,
                                                gint           format,
                                                const guchar  *text,
                                                gint           length,
                                                gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);

  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == gdk_atom_intern_static_string ("UTF8_STRING"))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar *enc_name = gdk_atom_name (encoding);

      g_warning ("gdk_text_property_to_utf8_list_for_display: encoding %s not handled", enc_name);
      g_free (enc_name);

      if (list)
	*list = NULL;

      return 0;
    }
}

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400
#define GDK_QUARTZ_URL_PBOARD_TYPE     NSURLPboardType
#define GDK_QUARTZ_COLOR_PBOARD_TYPE   NSColorPboardType
#define GDK_QUARTZ_STRING_PBOARD_TYPE  NSStringPboardType
#define GDK_QUARTZ_TIFF_PBOARD_TYPE    NSTIFFPboardType
#else
#define GDK_QUARTZ_URL_PBOARD_TYPE     NSPasteboardTypeURL
#define GDK_QUARTZ_COLOR_PBOARD_TYPE   NSPasteboardTypeColor
#define GDK_QUARTZ_STRING_PBOARD_TYPE  NSPasteboardTypeString
#define GDK_QUARTZ_TIFF_PBOARD_TYPE    NSPasteboardTypeTIFF
#endif

GdkAtom
gdk_quartz_pasteboard_type_to_atom_libgtk_only (NSString *type)
{
  if ([type isEqualToString:GDK_QUARTZ_STRING_PBOARD_TYPE])
    return gdk_atom_intern_static_string ("UTF8_STRING");
  else if ([type isEqualToString:GDK_QUARTZ_TIFF_PBOARD_TYPE])
    return gdk_atom_intern_static_string ("image/tiff");
  else if ([type isEqualToString:GDK_QUARTZ_COLOR_PBOARD_TYPE])
    return gdk_atom_intern_static_string ("application/x-color");
  else if ([type isEqualToString:GDK_QUARTZ_URL_PBOARD_TYPE])
    return gdk_atom_intern_static_string ("text/uri-list");
  else
    return gdk_atom_intern ([type UTF8String], FALSE);
}

NSString *
gdk_quartz_target_to_pasteboard_type_libgtk_only (const char *target)
{
  if (strcmp (target, "UTF8_STRING") == 0)
    return GDK_QUARTZ_STRING_PBOARD_TYPE;
  else if (strcmp (target, "image/tiff") == 0)
    return GDK_QUARTZ_TIFF_PBOARD_TYPE;
  else if (strcmp (target, "application/x-color") == 0)
    return GDK_QUARTZ_COLOR_PBOARD_TYPE;
  else if (strcmp (target, "text/uri-list") == 0)
    return GDK_QUARTZ_URL_PBOARD_TYPE;
  else
    return [NSString stringWithUTF8String:target];
}

NSString *
gdk_quartz_atom_to_pasteboard_type_libgtk_only (GdkAtom atom)
{
  gchar *target = gdk_atom_name (atom);
  NSString *ret = gdk_quartz_target_to_pasteboard_type_libgtk_only (target);
  g_free (target);

  return ret;
}
