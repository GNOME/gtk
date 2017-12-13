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
#include "gdkdisplayprivate.h"


/**
 * SECTION:selections
 * @Short_description: Functions for transfering data via the X selection mechanism
 * @Title: Selections
 *
 * The X selection mechanism provides a way to transfer arbitrary chunks of
 * data between programs. A “selection” is a essentially
 * a named clipboard, identified by a string interned as a #GdkAtom. By
 * claiming ownership of a selection, an application indicates that it will
 * be responsible for supplying its contents. The most common selections are
 * `PRIMARY` and `CLIPBOARD`.
 *
 * The contents of a selection can be represented in a number of formats,
 * called “targets”. Each target is identified by an atom.
 * A list of all possible targets supported by the selection owner can be
 * retrieved by requesting the special target `TARGETS`. When
 * a selection is retrieved, the data is accompanied by a type (an atom), and
 * a format (an integer, representing the number of bits per item).
 * See [Properties and Atoms][gdk3-Properties-and-Atoms]
 * for more information.
 *
 * The functions in this section only contain the lowlevel parts of the
 * selection protocol. A considerably more complicated implementation is needed
 * on top of this. GTK+ contains such an implementation in the functions in
 * `gtkselection.h` and programmers should use those functions
 * instead of the ones presented here. If you plan to implement selection
 * handling directly on top of the functions here, you should refer to the
 * X Inter-client Communication Conventions Manual (ICCCM).
 */

/**
 * gdk_selection_send_notify:
 * @requestor: window to which to deliver response.
 * @selection: selection that was requested.
 * @target: target that was selected.
 * @property: property in which the selection owner stored the
 *   data, or %NULL to indicate that the request
 *   was rejected.
 * @time_: timestamp.
 *
 * Sends a response to SelectionRequest event.
 */
void
gdk_selection_send_notify (GdkWindow      *requestor,
			   GdkAtom         selection,
			   GdkAtom         target,
			   GdkAtom         property,
			   guint32         time)
{
  gdk_selection_send_notify_for_display (gdk_window_get_display (requestor),
					 requestor, selection, 
					 target, property, time);
}

/**
 * gdk_selection_send_notify_for_display:
 * @display: the #GdkDisplay where @requestor is realized
 * @requestor: window to which to deliver response
 * @selection: selection that was requested
 * @target: target that was selected
 * @property: property in which the selection owner stored the data,
 *            or %NULL to indicate that the request was rejected
 * @time_: timestamp
 *
 * Send a response to SelectionRequest event.
 *
 * Since: 2.2
 */
void
gdk_selection_send_notify_for_display (GdkDisplay       *display,
                                       GdkWindow        *requestor,
                                       GdkAtom          selection,
                                       GdkAtom          target,
                                       GdkAtom          property,
                                       guint32          time_)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  GDK_DISPLAY_GET_CLASS (display)
    ->send_selection_notify (display, requestor, selection,target, property, time_);
}

/**
 * gdk_selection_property_get: (skip)
 * @requestor: the window on which the data is stored
 * @data: location to store a pointer to the retrieved data.
       If the retrieval failed, %NULL we be stored here, otherwise, it
       will be non-%NULL and the returned data should be freed with g_free()
       when you are finished using it. The length of the
       allocated memory is one more than the length
       of the returned data, and the final byte will always
       be zero, to ensure nul-termination of strings
 * @prop_type: location to store the type of the property
 * @prop_format: location to store the format of the property
 *
 * Retrieves selection data that was stored by the selection
 * data in response to a call to gdk_selection_convert(). This function
 * will not be used by applications, who should use the #GtkClipboard
 * API instead.
 *
 * Returns: the length of the retrieved data.
 */
gint
gdk_selection_property_get (GdkWindow  *requestor,
                            guchar    **data,
                            GdkAtom    *ret_type,
                            gint       *ret_format)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  display = gdk_window_get_display (requestor);

  return GDK_DISPLAY_GET_CLASS (display)
           ->get_selection_property (display, requestor, data, ret_type, ret_format);
}

void
gdk_selection_convert (GdkWindow *requestor,
                       GdkAtom    selection,
                       GdkAtom    target,
                       guint32    time)
{
  GdkDisplay *display;

  g_return_if_fail (selection != NULL);

  display = gdk_window_get_display (requestor);

  GDK_DISPLAY_GET_CLASS (display)
    ->convert_selection (display, requestor, selection, target, time);
}

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
 *
 * Since: 2.2
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
 * Converts an UTF-8 string into the best possible representation
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

