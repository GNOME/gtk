/* GTK - The GIMP Toolkit
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

/**
 * SECTION:gtkeditable
 * @Short_description: Interface for text-editing widgets
 * @Title: GtkEditable
 *
 * The #GtkEditable interface is an interface which should be implemented by
 * text editing widgets, such as #GtkEntry and #GtkSpinButton. It contains functions
 * for generically manipulating an editable widget, a large number of action
 * signals used for key bindings, and several signals that an application can
 * connect to to modify the behavior of a widget.
 *
 * As an example of the latter usage, by connecting
 * the following handler to #GtkEditable::insert-text, an application
 * can convert all entry into a widget into uppercase.
 *
 * ## Forcing entry to uppercase.
 *
 * |[<!-- language="C" -->
 * #include <ctype.h>;
 *
 * void
 * insert_text_handler (GtkEditable *editable,
 *                      const char  *text,
 *                      int          length,
 *                      int         *position,
 *                      gpointer     data)
 * {
 *   char *result = g_utf8_strup (text, length);
 *
 *   g_signal_handlers_block_by_func (editable,
 *                                (gpointer) insert_text_handler, data);
 *   gtk_editable_insert_text (editable, result, length, position);
 *   g_signal_handlers_unblock_by_func (editable,
 *                                      (gpointer) insert_text_handler, data);
 *
 *   g_signal_stop_emission_by_name (editable, "insert_text");
 *
 *   g_free (result);
 * }
 * ]|
 */

#include "config.h"
#include <string.h>

#include "gtkeditable.h"
#include "gtkentrybuffer.h"
#include "gtkeditableprivate.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkprivate.h"

G_DEFINE_INTERFACE (GtkEditable, gtk_editable, GTK_TYPE_WIDGET)

static void
gtk_editable_default_init (GtkEditableInterface *iface)
{
  /**
   * GtkEditable::insert-text:
   * @editable: the object which received the signal
   * @text: the new text to insert
   * @length: the length of the new text, in bytes,
   *     or -1 if new_text is nul-terminated
   * @position: (inout) (type int): the position, in characters,
   *     at which to insert the new text. this is an in-out
   *     parameter.  After the signal emission is finished, it
   *     should point after the newly inserted text.
   *
   * This signal is emitted when text is inserted into
   * the widget by the user. The default handler for
   * this signal will normally be responsible for inserting
   * the text, so by connecting to this signal and then
   * stopping the signal with g_signal_stop_emission(), it
   * is possible to modify the inserted text, or prevent
   * it from being inserted entirely.
   */
  g_signal_new (I_("insert-text"),
                GTK_TYPE_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkEditableInterface, insert_text),
                NULL, NULL,
                _gtk_marshal_VOID__STRING_INT_POINTER,
                G_TYPE_NONE, 3,
                G_TYPE_STRING,
                G_TYPE_INT,
                G_TYPE_POINTER);

  /**
   * GtkEditable::delete-text:
   * @editable: the object which received the signal
   * @start_pos: the starting position
   * @end_pos: the end position
   * 
   * This signal is emitted when text is deleted from
   * the widget by the user. The default handler for
   * this signal will normally be responsible for deleting
   * the text, so by connecting to this signal and then
   * stopping the signal with g_signal_stop_emission(), it
   * is possible to modify the range of deleted text, or
   * prevent it from being deleted entirely. The @start_pos
   * and @end_pos parameters are interpreted as for
   * gtk_editable_delete_text().
   */
  g_signal_new (I_("delete-text"),
                GTK_TYPE_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkEditableInterface, delete_text),
                NULL, NULL,
                _gtk_marshal_VOID__INT_INT,
                G_TYPE_NONE, 2,
                G_TYPE_INT,
                G_TYPE_INT);

  /**
   * GtkEditable::changed:
   * @editable: the object which received the signal
   *
   * The ::changed signal is emitted at the end of a single
   * user-visible operation on the contents of the #GtkEditable.
   *
   * E.g., a paste operation that replaces the contents of the
   * selection will cause only one signal emission (even though it
   * is implemented by first deleting the selection, then inserting
   * the new content, and may cause multiple ::notify::text signals
   * to be emitted).
   */ 
  g_signal_new (I_("changed"),
                GTK_TYPE_EDITABLE,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (GtkEditableInterface, changed),
                NULL, NULL,
                NULL,
                G_TYPE_NONE, 0);

  g_object_interface_install_property (iface,
      g_param_spec_string ("text",
                           P_("Text"),
                           P_("The contents of the entry"),
                           "",
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property (iface,
      g_param_spec_int ("cursor-position",
                        P_("Cursor Position"),
                        P_("The current position of the insertion cursor in chars"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READABLE));

  g_object_interface_install_property (iface,
      g_param_spec_int ("selection-bound",
                        P_("Selection Bound"),
                        P_("The position of the opposite end of the selection from the cursor in chars"),
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READABLE));

  g_object_interface_install_property (iface,
      g_param_spec_boolean ("editable",
                            P_("Editable"),
                            P_("Whether the entry contents can be edited"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property (iface,
      g_param_spec_int ("width-chars",
                        P_("Width in chars"),
                        P_("Number of characters to leave space for in the entry"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property (iface,
      g_param_spec_int ("max-width-chars",
                        P_("Maximum width in characters"),
                        P_("The desired maximum width of the entry, in characters"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property (iface,
      g_param_spec_float ("xalign",
                          P_("X align"),
                          P_("The horizontal alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
                          0.0, 1.0,
                          0.0,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

/**
 * gtk_editable_insert_text: (virtual do_insert_text)
 * @editable: a #GtkEditable
 * @text: the text to append
 * @length: the length of the text in bytes, or -1
 * @position: (inout): location of the position text will be inserted at
 *
 * Inserts @length bytes of @text into the contents of the
 * widget, at position @position.
 *
 * Note that the position is in characters, not in bytes. 
 * The function updates @position to point after the newly inserted text.
 */
void
gtk_editable_insert_text (GtkEditable *editable,
                          const char  *text,
                          int          length,
                          int         *position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (position != NULL);

  if (length < 0)
    length = strlen (text);
  
  GTK_EDITABLE_GET_IFACE (editable)->do_insert_text (editable, text, length, position);
}

/**
 * gtk_editable_delete_text: (virtual do_delete_text)
 * @editable: a #GtkEditable
 * @start_pos: start position
 * @end_pos: end position
 *
 * Deletes a sequence of characters. The characters that are deleted are 
 * those characters at positions from @start_pos up to, but not including 
 * @end_pos. If @end_pos is negative, then the characters deleted
 * are those from @start_pos to the end of the text.
 *
 * Note that the positions are specified in characters, not bytes.
 */
void
gtk_editable_delete_text (GtkEditable *editable,
                          int          start_pos,
                          int          end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_IFACE (editable)->do_delete_text (editable, start_pos, end_pos);
}

/**
 * gtk_editable_get_chars:
 * @editable: a #GtkEditable
 * @start_pos: start of text
 * @end_pos: end of text
 *
 * Retrieves a sequence of characters. The characters that are retrieved 
 * are those characters at positions from @start_pos up to, but not 
 * including @end_pos. If @end_pos is negative, then the characters
 * retrieved are those characters from @start_pos to the end of the text.
 * 
 * Note that positions are specified in characters, not bytes.
 *
 * Returns: (transfer full): a pointer to the contents of the widget as a
 *      string. This string is allocated by the #GtkEditable
 *      implementation and should be freed by the caller.
 */
char *    
gtk_editable_get_chars (GtkEditable *editable,
                        int          start_pos,
                        int          end_pos)
{
  const char *text;
  int length;
  int start_index,end_index;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  text = GTK_EDITABLE_GET_IFACE (editable)->get_text (editable);
  length = g_utf8_strlen (text, -1);

  if (end_pos < 0)
    end_pos = length;

  start_pos = MIN (length, start_pos);
  end_pos = MIN (length, end_pos);

  start_index = g_utf8_offset_to_pointer (text, start_pos) - text;
  end_index = g_utf8_offset_to_pointer (text, end_pos) - text;

  return g_strndup (text + start_index, end_index - start_index);
}

/**
 * gtk_editable_get_text:
 * @editable: a #GtkEditable
 *
 * Retrieves the contents of @editable. The returned string is
 * owned by GTK and must not be modified or freed.
 *
 * Returns: (transfer none): a pointer to the contents of the editable.
 */
const char *
gtk_editable_get_text (GtkEditable *editable)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  return GTK_EDITABLE_GET_IFACE (editable)->get_text (editable);
}

/**
 * gtk_editable_set_text:
 * @editable: a #GtkEditable
 *
 * Sets the text in the editable to the given value,
 * replacing the current contents.
 */
void
gtk_editable_set_text (GtkEditable *editable,
                       const char  *text)
{
  int pos;

  g_return_if_fail (GTK_IS_EDITABLE (editable));

  gtk_editable_delete_text (editable, 0, -1);
  pos = 0;
  gtk_editable_insert_text (editable, text, -1, &pos);
}

/**
 * gtk_editable_set_position:
 * @editable: a #GtkEditable
 * @position: the position of the cursor 
 *
 * Sets the cursor position in the editable to the given value.
 *
 * The cursor is displayed before the character with the given (base 0) 
 * index in the contents of the editable. The value must be less than or 
 * equal to the number of characters in the editable. A value of -1 
 * indicates that the position should be set after the last character 
 * of the editable. Note that @position is in characters, not in bytes.
 */
void
gtk_editable_set_position (GtkEditable *editable,
                           int          position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_IFACE (editable)->set_position (editable, position);
}

/**
 * gtk_editable_get_position:
 * @editable: a #GtkEditable
 *
 * Retrieves the current position of the cursor relative to the start
 * of the content of the editable. 
 * 
 * Note that this position is in characters, not in bytes.
 *
 * Returns: the cursor position
 */
int
gtk_editable_get_position (GtkEditable *editable)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  return GTK_EDITABLE_GET_IFACE (editable)->get_position (editable);
}

/**
 * gtk_editable_get_selection_bounds:
 * @editable: a #GtkEditable
 * @start_pos: (out) (allow-none): location to store the starting position, or %NULL
 * @end_pos: (out) (allow-none): location to store the end position, or %NULL
 *
 * Retrieves the selection bound of the editable.
 * 
 * @start_pos will be filled with the start of the selection and
 * @end_pos with end. If no text was selected both will be identical
 * and %FALSE will be returned.
 *
 * Note that positions are specified in characters, not bytes.
 *
 * Returns: %TRUE if there is a non-empty selection, %FALSE otherwise
 */
gboolean
gtk_editable_get_selection_bounds (GtkEditable *editable,
                                   int         *start_pos,
                                   int         *end_pos)
{
  int tmp_start, tmp_end;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  result = GTK_EDITABLE_GET_IFACE (editable)->get_selection_bounds (editable, &tmp_start, &tmp_end);

  if (start_pos)
    *start_pos = MIN (tmp_start, tmp_end);
  if (end_pos)
    *end_pos = MAX (tmp_start, tmp_end);

  return result;
}

/**
 * gtk_editable_delete_selection:
 * @editable: a #GtkEditable
 *
 * Deletes the currently selected text of the editable.
 * This call doesn’t do anything if there is no selected text.
 */
void
gtk_editable_delete_selection (GtkEditable *editable)
{
  int start, end;

  g_return_if_fail (GTK_IS_EDITABLE (editable));

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);
}

/**
 * gtk_editable_select_region: (virtual set_selection_bounds)
 * @editable: a #GtkEditable
 * @start_pos: start of region
 * @end_pos: end of region
 *
 * Selects a region of text.
 * 
 * The characters that are selected are those characters at positions
 * from @start_pos up to, but not including @end_pos. If @end_pos is
 * negative, then the characters selected are those characters from
 * @start_pos to  the end of the text.
 * 
 * Note that positions are specified in characters, not bytes.
 */
void
gtk_editable_select_region (GtkEditable *editable,
                            int          start_pos,
                            int          end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  GTK_EDITABLE_GET_IFACE (editable)->set_selection_bounds (editable, start_pos, end_pos);
}

/**
 * gtk_editable_set_editable:
 * @editable: a #GtkEditable
 * @is_editable: %TRUE if the user is allowed to edit the text
 *   in the widget
 *
 * Determines if the user can edit the text
 * in the editable widget or not. 
 */
void
gtk_editable_set_editable (GtkEditable *editable,
                           gboolean     is_editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "editable", is_editable, NULL);
}

/**
 * gtk_editable_get_editable:
 * @editable: a #GtkEditable
 *
 * Retrieves whether @editable is editable.
 * See gtk_editable_set_editable().
 *
 * Returns: %TRUE if @editable is editable.
 */
gboolean
gtk_editable_get_editable (GtkEditable *editable)
{
  gboolean is_editable;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  g_object_get (editable, "editable", &is_editable, NULL);

  return is_editable;
}


/**
 * gtk_editable_get_alignment:
 * @editable: a #GtkEditable
 *
 * Gets the value set by gtk_editable_set_alignment().
 *
 * Returns: the alignment
 **/
float
gtk_editable_get_alignment (GtkEditable *editable)
{
  float xalign;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "xalign", &xalign, NULL);

  return xalign;
}

/**
 * gtk_editable_set_alignment:
 * @editable: a #GtkEditable
 * @xalign: The horizontal alignment, from 0 (left) to 1 (right).
 *          Reversed for RTL layouts
 *
 * Sets the alignment for the contents of the editable.
 *
 * This controls the horizontal positioning of the contents when
 * the displayed text is shorter than the width of the editable.
 */
void
gtk_editable_set_alignment (GtkEditable *editable,
                            float        xalign)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "xalign", xalign, NULL);
}

/**
 * gtk_editable_get_width_chars:
 * @editable: a #GtkEditable
 *
 * Gets the value set by gtk_editable_set_width_chars().
 *
 * Returns: number of chars to request space for, or negative if unset
 **/
int
gtk_editable_get_width_chars (GtkEditable *editable)
{
  int width_chars;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "width-chars", &width_chars, NULL);

  return width_chars;
}

/**
 * gtk_editable_set_width_chars:
 * @editable: a #GtkEditable
 * @n_chars: width in chars
 *
 * Changes the size request of the editable to be about the
 * right size for @n_chars characters.
 * 
 * Note that it changes the size request, the size can still
 * be affected by how you pack the widget into containers.
 * If @n_chars is -1, the size reverts to the default size.
 **/
void
gtk_editable_set_width_chars (GtkEditable *editable,
                              int          n_chars)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "width-chars", n_chars, NULL);
}

/**
 * gtk_editable_get_max_width_chars:
 * @editable: a #GtkEditable
 *
 * Retrieves the desired maximum width of @editable, in characters.
 * See gtk_editable_set_max_width_chars().
 *
 * Returns: the maximum width of the entry, in characters
 */
int
gtk_editable_get_max_width_chars (GtkEditable *editable)
{
  int max_width_chars;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  g_object_get (editable, "max-width-chars", &max_width_chars, NULL);

  return max_width_chars;
}


/**
 * gtk_editable_set_max_width_chars:
 * @editable: a #GtkEditable
 * @n_chars: the new desired maximum width, in characters
 *
 * Sets the desired maximum width in characters of @editable.
 */
void
gtk_editable_set_max_width_chars (GtkEditable *editable,
                                  int          n_chars)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable, "max-width-chars", n_chars, NULL);
}

/* Delegate setup helpers
 */
void
gtk_editable_install_properties (GObjectClass *class)
{
  g_object_class_override_property (class, GTK_EDITABLE_PROP_TEXT, "text");
  g_object_class_override_property (class, GTK_EDITABLE_PROP_CURSOR_POSITION, "cursor-position");
  g_object_class_override_property (class, GTK_EDITABLE_PROP_SELECTION_BOUND, "selection-bound");
  g_object_class_override_property (class, GTK_EDITABLE_PROP_EDITABLE, "editable");
  g_object_class_override_property (class, GTK_EDITABLE_PROP_WIDTH_CHARS, "width-chars");
  g_object_class_override_property (class, GTK_EDITABLE_PROP_MAX_WIDTH_CHARS, "max-width-chars");
  g_object_class_override_property (class, GTK_EDITABLE_PROP_XALIGN, "xalign");
}

static void
delegate_changed (GtkEditable *delegate,
                  gpointer     editable)
{
  g_signal_emit_by_name (editable, "changed");
}

static void
delegate_notify (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)), gtk_editable_get_type ());
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify (data, pspec->name);
}

void
gtk_editable_set_delegate (GtkEditable *editable,
                           GtkEditable *delegate)
{
  g_object_set_data (G_OBJECT (editable), "gtk-editable-delegate", delegate);
  g_signal_connect (delegate, "notify", G_CALLBACK (delegate_notify), editable);
  g_signal_connect (delegate, "changed", G_CALLBACK (delegate_changed), editable);
}

static GtkEditable *
get_delegate (GtkEditable *editable)
{
  return GTK_EDITABLE (g_object_get_data (G_OBJECT (editable), "gtk-editable-delegate"));
}

static void
delegate_do_insert_text (GtkEditable *editable,
                         const char  *text,
                         int          length,
                         int         *position)
{
  gtk_editable_insert_text (get_delegate (editable), text, length, position);
}

static void
delegate_do_delete_text (GtkEditable *editable,
                         int          start_pos,
                         int          end_pos)
{
  gtk_editable_delete_text (get_delegate (editable), start_pos, end_pos);
}

static const char *
delegate_get_text (GtkEditable *editable)
{
  return gtk_editable_get_text (get_delegate (editable));
}

static void
delegate_select_region (GtkEditable *editable,
                        int          start_pos,
                        int          end_pos)
{
  gtk_editable_select_region (get_delegate (editable), start_pos, end_pos);
}

static gboolean
delegate_get_selection_bounds (GtkEditable *editable,
                               int         *start_pos,
                               int         *end_pos)
{
  return gtk_editable_get_selection_bounds (get_delegate (editable), start_pos, end_pos);
}

static void
delegate_set_position (GtkEditable *editable,
                       int          position)
{
  gtk_editable_set_position (get_delegate (editable), position);
}

static int
delegate_get_position (GtkEditable *editable)
{
  return gtk_editable_get_position (get_delegate (editable));
}

void
gtk_editable_delegate_iface_init (GtkEditableInterface *iface)
{
  iface->do_insert_text = delegate_do_insert_text;
  iface->do_delete_text = delegate_do_delete_text;
  iface->get_text = delegate_get_text;
  iface->set_selection_bounds = delegate_select_region;
  iface->get_selection_bounds = delegate_get_selection_bounds;
  iface->set_position = delegate_set_position;
  iface->get_position = delegate_get_position;
}

gboolean
gtk_editable_delegate_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkEditable *delegate = get_delegate (GTK_EDITABLE (object));

  switch (prop_id)
    {
    case GTK_EDITABLE_PROP_TEXT:
      gtk_editable_set_text (delegate, g_value_get_string (value));
      break;

    case GTK_EDITABLE_PROP_EDITABLE:
      gtk_editable_set_editable (delegate, g_value_get_boolean (value));
      break;

    case GTK_EDITABLE_PROP_WIDTH_CHARS:
      gtk_editable_set_width_chars (delegate, g_value_get_int (value));
      break;

    case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      gtk_editable_set_max_width_chars (delegate, g_value_get_int (value));
      break;

    case GTK_EDITABLE_PROP_XALIGN:
      gtk_editable_set_alignment (delegate, g_value_get_float (value));
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

gboolean
gtk_editable_delegate_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkEditable *delegate = get_delegate (GTK_EDITABLE (object));
  int cursor_position, selection_bound;

  switch (prop_id)
    {
    case GTK_EDITABLE_PROP_TEXT:
      g_value_set_string (value, gtk_editable_get_text (delegate));
      break;

    case GTK_EDITABLE_PROP_CURSOR_POSITION:
      gtk_editable_get_selection_bounds (delegate, &cursor_position, &selection_bound);
      g_value_set_int (value, cursor_position);
      break;

    case GTK_EDITABLE_PROP_SELECTION_BOUND:
      gtk_editable_get_selection_bounds (delegate, &cursor_position, &selection_bound);
      g_value_set_int (value, selection_bound);
      break;

    case GTK_EDITABLE_PROP_EDITABLE:
      g_value_set_boolean (value, gtk_editable_get_editable (delegate));
      break;

    case GTK_EDITABLE_PROP_WIDTH_CHARS:
      g_value_set_int (value, gtk_editable_get_width_chars (delegate));
      break;

    case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, gtk_editable_get_max_width_chars (delegate));
      break;

    case GTK_EDITABLE_PROP_XALIGN:
      g_value_set_float (value, gtk_editable_get_alignment (delegate));
      break;

    default:
      return FALSE;
    }

  return TRUE;
}
