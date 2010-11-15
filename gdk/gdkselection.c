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
#include "gdkdisplay.h"


/**
 * SECTION:selections
 * @Short_description: Functions for transfering data via the X selection mechanism
 * @Title: Selections
 *
 * The X selection mechanism provides a way to transfer arbitrary chunks of
 * data between programs. A <firstterm>selection</firstterm> is a essentially
 * a named clipboard, identified by a string interned as a #GdkAtom. By
 * claiming ownership of a selection, an application indicates that it will
 * be responsible for supplying its contents. The most common selections are
 * <literal>PRIMARY</literal> and <literal>CLIPBOARD</literal>.
 *
 * The contents of a selection can be represented in a number of formats,
 * called <firstterm>targets</firstterm>. Each target is identified by an atom.
 * A list of all possible targets supported by the selection owner can be
 * retrieved by requesting the special target <literal>TARGETS</literal>. When
 * a selection is retrieved, the data is accompanied by a type (an atom), and
 * a format (an integer, representing the number of bits per item).
 * See <link linkend="gdk-Properties-and-Atoms">Properties and Atoms</link>
 * for more information.
 *
 * The functions in this section only contain the lowlevel parts of the
 * selection protocol. A considerably more complicated implementation is needed
 * on top of this. GTK+ contains such an implementation in the functions in
 * <literal>gtkselection.h</literal> and programmers should use those functions
 * instead of the ones presented here. If you plan to implement selection
 * handling directly on top of the functions here, you should refer to the
 * X Inter-client Communication Conventions Manual (ICCCM).
 */

/**
 * gdk_selection_owner_set:
 * @owner: a #GdkWindow or %NULL to indicate that the
 *   the owner for the given should be unset.
 * @selection: an atom identifying a selection.
 * @time_: timestamp to use when setting the selection.
 *   If this is older than the timestamp given last
 *   time the owner was set for the given selection, the
 *   request will be ignored.
 * @send_event: if %TRUE, and the new owner is different
 *   from the current owner, the current owner
 *   will be sent a SelectionClear event.
 *
 * Sets the owner of the given selection.
 *
 * Returns: %TRUE if the selection owner was successfully
 *   changed to @owner, otherwise %FALSE.
 */
gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
  return gdk_selection_owner_set_for_display (gdk_display_get_default (),
					      owner, selection, 
					      time, send_event);
}

/**
 * gdk_selection_owner_get:
 * @selection: an atom indentifying a selection.
 *
 * Determines the owner of the given selection.
 *
 * Returns: if there is a selection owner for this window,
 *   and it is a window known to the current process,
 *   the #GdkWindow that owns the selection, otherwise
 *   %NULL. Note that the return value may be owned
 *   by a different process if a foreign window
 *   was previously created for that window, but
 *   a new foreign window will never be created by
 *   this call.
 */
GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  return gdk_selection_owner_get_for_display (gdk_display_get_default (), 
					      selection);
}

/**
 * gdk_selection_send_notify:
 * @requestor: window to which to deliver response.
 * @selection: selection that was requested.
 * @target: target that was selected.
 * @property: property in which the selection owner stored the
 *   data, or %GDK_NONE to indicate that the request
 *   was rejected.
 * @time_: timestamp.
 *
 * Sends a response to SelectionRequest event.
 */
void
gdk_selection_send_notify (GdkNativeWindow requestor,
			   GdkAtom         selection,
			   GdkAtom         target,
			   GdkAtom         property,
			   guint32         time)
{
  gdk_selection_send_notify_for_display (gdk_display_get_default (), 
					 requestor, selection, 
					 target, property, time);
}

/**
 * gdk_text_property_to_text_list:
 * @encoding: an atom representing the encoding. The most common
 *   values for this are <literal>STRING</literal>,
 *   or <literal>COMPOUND_TEXT</literal>. This is
 *   value used as the type for the property.
 * @format: the format of the property.
 * @text: the text data.
 * @length: the length of the property, in items.
 * @list: location to store a terminated array of strings
 *   in the encoding of the current locale. This
 *   array should be freed using gdk_free_text_list().
 *
 * Converts a text string from the encoding as it is stored in
 * a property into an array of strings in the encoding of
 * the current local. (The elements of the array represent
 * the nul-separated elements of the original text string.)
 *
 * Returns: the number of strings stored in @list, or 0,
 *   if the conversion failed.
 */
gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
  return gdk_text_property_to_text_list_for_display (gdk_display_get_default (),
						     encoding, format, text, length, list);
}

/**
 * gdk_text_property_to_utf8_list:
 * @encoding: an atom representing the encoding of the text
 * @format:   the format of the property
 * @text:     the text to convert
 * @length:   the length of @text, in bytes
 * @list: (allow-none):     location to store the list of strings or %NULL. The
 *            list should be freed with g_strfreev().
 * 
 * Convert a text property in the giving encoding to
 * a list of UTF-8 strings. 
 * 
 * Return value: the number of strings in the resulting
 *               list.
 **/
gint 
gdk_text_property_to_utf8_list (GdkAtom        encoding,
				gint           format,
				const guchar  *text,
				gint           length,
				gchar       ***list)
{
  return gdk_text_property_to_utf8_list_for_display (gdk_display_get_default (),
						     encoding, format, text, length, list);
}

/**
 * gdk_string_to_compound_text:
 * @str: a nul-terminated string.
 * @encoding: location to store the encoding atom (to be used as
 *   the type for the property).
 * @format: location to store the format for the property.
 * @ctext: location to store newly allocated data for the property.
 * @length: location to store the length of @ctext in items.
 *
 * Converts a string from the encoding of the current locale
 * into a form suitable for storing in a window property.
 *
 * Returns: 0 upon sucess, non-zero upon failure.
 */
gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
  return gdk_string_to_compound_text_for_display (gdk_display_get_default (),
						  str, encoding, format, 
						  ctext, length);
}

/**
 * gdk_utf8_to_compound_text:
 * @str:      a UTF-8 string
 * @encoding: location to store resulting encoding
 * @format:   location to store format of the result
 * @ctext:    location to store the data of the result
 * @length:   location to store the length of the data
 *            stored in @ctext
 * 
 * Convert from UTF-8 to compound text. 
 * 
 * Return value: %TRUE if the conversion succeeded, otherwise
 *               false.
 **/
gboolean
gdk_utf8_to_compound_text (const gchar *str,
			   GdkAtom     *encoding,
			   gint        *format,
			   guchar     **ctext,
			   gint        *length)
{
  return gdk_utf8_to_compound_text_for_display (gdk_display_get_default (),
						str, encoding, format, 
						ctext, length);
}
