/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <fcntl.h>
#include <unistd.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>

#include "gdkwayland.h"
#include "gdkprivate-wayland.h"
#include "gdkdisplay-wayland.h"
#include "gdkcontentformatsprivate.h"
#include "gdkproperty.h"

#include <string.h>

gint
_gdk_wayland_display_text_property_to_utf8_list (GdkDisplay    *display,
                                                 GdkAtom        encoding,
                                                 gint           format,
                                                 const guchar  *text,
                                                 gint           length,
                                                 gchar       ***list)
{
  GPtrArray *array;
  const gchar *ptr;
  gsize chunk_len;
  gchar *copy;
  guint nitems;

  ptr = (const gchar *) text;
  array = g_ptr_array_new ();

  while (ptr < (const gchar *) &text[length])
    {
      chunk_len = strlen (ptr);

      if (g_utf8_validate (ptr, chunk_len, NULL))
        {
          copy = g_strndup (ptr, chunk_len);
          g_ptr_array_add (array, copy);
        }

      ptr = &ptr[chunk_len + 1];
    }

  nitems = array->len;
  g_ptr_array_add (array, NULL);

  if (list)
    *list = (gchar **) g_ptr_array_free (array, FALSE);
  else
    g_ptr_array_free (array, TRUE);

  return nitems;
}

