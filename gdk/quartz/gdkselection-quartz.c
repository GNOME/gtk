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

#include "gdkproperty.h"
#include "gdkquartz.h"

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

  if (encoding == g_intern_static_string ("STRING"))
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == g_intern_static_string ("UTF8_STRING"))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      g_warning ("gdk_text_property_to_utf8_list_for_display: encoding %s not handled", (const char *)encoding);

      if (list)
	*list = NULL;

      return 0;
    }
}

GdkAtom
gdk_quartz_pasteboard_type_to_atom_libgtk_only (NSString *type)
{
  if ([type isEqualToString:NSStringPboardType])
    return g_intern_static_string ("UTF8_STRING");
  else if ([type isEqualToString:NSTIFFPboardType])
    return g_intern_static_string ("image/tiff");
  else if ([type isEqualToString:NSColorPboardType])
    return g_intern_static_string ("application/x-color");
  else if ([type isEqualToString:NSURLPboardType])
    return g_intern_static_string ("text/uri-list");
  else
    return g_intern_string ([type UTF8String]);
}

NSString *
gdk_quartz_target_to_pasteboard_type_libgtk_only (const char *target)
{
  if (strcmp (target, "UTF8_STRING") == 0)
    return NSStringPboardType;
  else if (strcmp (target, "image/tiff") == 0)
    return NSTIFFPboardType;
  else if (strcmp (target, "application/x-color") == 0)
    return NSColorPboardType;
  else if (strcmp (target, "text/uri-list") == 0)
    return NSURLPboardType;
  else
    return [NSString stringWithUTF8String:target];
}

NSString *
gdk_quartz_atom_to_pasteboard_type_libgtk_only (GdkAtom atom)
{
  const char *target = (const char *)atom;
  NSString *ret = gdk_quartz_target_to_pasteboard_type_libgtk_only (target);

  return ret;
}
