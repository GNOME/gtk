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

#include "gdkproperty.h"

#include "gdkdisplayprivate.h"


/**
 * gdk_text_property_to_utf8_list_for_display:
 * @display:  a #GdkDisplay
 * @encoding: an atom representing the encoding of the text
 * @format:   the format of the property
 * @text:     (array length=length): the text to convert
 * @length:   the length of @text, in bytes
 * @list:     (out) (array zero-terminated=1): location to store the list
 *            of strings or %NULL. The list should be freed with
 *            g_strfreev().
 *
 * Converts a text property in the given encoding to
 * a list of UTF-8 strings.
 *
 * Returns: the number of strings in the resulting list
 */
gint
gdk_text_property_to_utf8_list_for_display (GdkDisplay     *display,
                                            GdkAtom         encoding,
                                            gint            format,
                                            const guchar   *text,
                                            gint            length,
                                            gchar        ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  return GDK_DISPLAY_GET_CLASS (display)
           ->text_property_to_utf8_list (display, encoding, format, text, length, list);
}

/**
 * gdk_utf8_to_string_target:
 * @str: a UTF-8 string
 *
 * Converts a UTF-8 string into the best possible representation
 * as a STRING. The representation of characters not in STRING
 * is not specified; it may be as pseudo-escape sequences
 * \x{ABCD}, or it may be in some other form of approximation.
 *
 * Returns: (nullable): the newly-allocated string, or %NULL if the
 *          conversion failed. (It should not fail for any properly
 *          formed UTF-8 string unless system limits like memory or
 *          file descriptors are exceeded.)
 **/
gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  GdkDisplay *display = gdk_display_get_default ();

  return GDK_DISPLAY_GET_CLASS (display)->utf8_to_string_target (display, str);
}

