/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gtkimcontext.h"
#include "gtkmain.h"		/* For _gtk_boolean_handled_accumulator */
#include "gtkmarshalers.h"
#include "gtksignal.h"
#include "string.h"

enum {
  PREEDIT_START,
  PREEDIT_END,
  PREEDIT_CHANGED,
  COMMIT,
  RETRIEVE_SURROUNDING,
  DELETE_SURROUNDING,
  LAST_SIGNAL
};

static guint im_context_signals[LAST_SIGNAL] = { 0 };

static void gtk_im_context_class_init (GtkIMContextClass *class);
static void gtk_im_context_init (GtkIMContext *im_context);

static void     gtk_im_context_real_get_preedit_string (GtkIMContext   *context,
							gchar         **str,
							PangoAttrList **attrs,
							gint           *cursor_pos);
static gboolean gtk_im_context_real_filter_keypress    (GtkIMContext   *context,
							GdkEventKey    *event);
static gboolean gtk_im_context_real_get_surrounding    (GtkIMContext   *context,
							gchar         **text,
							gint           *cursor_index);
static void     gtk_im_context_real_set_surrounding    (GtkIMContext   *context,
							const char     *text,
							gint            len,
							gint            cursor_index);

GtkType
gtk_im_context_get_type (void)
{
  static GtkType im_context_type = 0;

  if (!im_context_type)
    {
      static const GTypeInfo im_context_info =
      {
        sizeof (GtkIMContextClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_im_context_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkIMContext),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_im_context_init,
      };
      
      im_context_type = g_type_register_static (G_TYPE_OBJECT,
						"GtkIMContext",
						&im_context_info, 0);
    }

  return im_context_type;
}

static void
gtk_im_context_class_init (GtkIMContextClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;

  klass->get_preedit_string = gtk_im_context_real_get_preedit_string;
  klass->filter_keypress = gtk_im_context_real_filter_keypress;
  klass->get_surrounding = gtk_im_context_real_get_surrounding;
  klass->set_surrounding = gtk_im_context_real_set_surrounding;

  im_context_signals[PREEDIT_START] =
    gtk_signal_new ("preedit_start",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_start),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[PREEDIT_END] =
    gtk_signal_new ("preedit_end",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_end),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[PREEDIT_CHANGED] =
    gtk_signal_new ("preedit_changed",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_changed),
		    _gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[COMMIT] =
    gtk_signal_new ("commit",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, commit),
		    _gtk_marshal_VOID__STRING,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STRING);

  im_context_signals[RETRIEVE_SURROUNDING] =
    g_signal_new ("retrieve_surrounding",
                  GTK_CLASS_TYPE (object_class),
                  GTK_RUN_LAST,
                  GTK_SIGNAL_OFFSET (GtkIMContextClass, retrieve_surrounding),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  GTK_TYPE_BOOL, 0);
  im_context_signals[DELETE_SURROUNDING] =
    g_signal_new ("delete_surrounding",
                  GTK_CLASS_TYPE (object_class),
                  GTK_RUN_LAST,
                  GTK_SIGNAL_OFFSET (GtkIMContextClass, delete_surrounding),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__INT_INT,
                  GTK_TYPE_BOOL, 2,
                  GTK_TYPE_INT,
		  GTK_TYPE_INT);
}

static void
gtk_im_context_init (GtkIMContext *im_context)
{
}

static void
gtk_im_context_real_get_preedit_string (GtkIMContext       *context,
					gchar             **str,
					PangoAttrList     **attrs,
					gint               *cursor_pos)
{
  if (str)
    *str = g_strdup ("");
  if (attrs)
    *attrs = pango_attr_list_new ();
  if (cursor_pos)
    *cursor_pos = 0;
}

static gboolean
gtk_im_context_real_filter_keypress (GtkIMContext       *context,
				     GdkEventKey        *event)
{
  return FALSE;
}

typedef struct
{
  gchar *text;
  gint cursor_index;
} SurroundingInfo;

static void
gtk_im_context_real_set_surrounding (GtkIMContext  *context,
				     const gchar   *text,
				     gint           len,
				     gint           cursor_index)
{
  SurroundingInfo *info = g_object_get_data (G_OBJECT (context), "gtk-im-surrounding-info");

  if (info)
    {
      g_free (info->text);
      info->text = g_strndup (text, len);
      info->cursor_index = cursor_index;
    }
}

static gboolean
gtk_im_context_real_get_surrounding (GtkIMContext *context,
				     gchar       **text,
				     gint         *cursor_index)
{
  gboolean result;
  gboolean info_is_local = FALSE;
  SurroundingInfo local_info = { NULL, 0 };
  SurroundingInfo *info;
  
  info = g_object_get_data (G_OBJECT (context), "gtk-im-surrounding-info");
  if (!info)
    {
      info = &local_info;
      g_object_set_data (G_OBJECT (context), "gtk-im-surrounding-info", info);
      info_is_local = TRUE;
    }
  
  g_signal_emit (context,
		 im_context_signals[RETRIEVE_SURROUNDING], 0,
		 &result);

  if (result)
    {
      *text = g_strdup (info->text ? info->text : "");
      *cursor_index = info->cursor_index;
    }
  else
    {
      *text = NULL;
      *cursor_index = 0;
    }

  if (info_is_local)
    g_free (info->text);
  
  return result;
}

/**
 * gtk_im_context_set_client_window:
 * @context: a #GtkIMContext
 * @window:  the client window. This may be %NULL to indicate
 *           that the previous client window no longer exists.
 * 
 * Set the client window for the input context; this is the
 * #GdkWindow in which the input appears. This window is
 * used in order to correctly position status windows, and may
 * also be used for purposes internal to the input method.
 **/
void
gtk_im_context_set_client_window (GtkIMContext *context,
				  GdkWindow    *window)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_client_window)
    klass->set_client_window (context, window);
}

/**
 * gtk_im_context_get_preedit_string:
 * @context:    a #GtkIMContext
 * @str:        location to store the retrieved string. The
 *              string retrieved must be freed with g_free ().
 * @attrs:      location to store the retrieved attribute list.
 *              When you are done with this list, you must
 *              unreference it with pango_attr_list_unref().
 * @cursor_pos: location to store position of cursor (in bytes)
 *              within the preedit string.  
 * 
 * Retrieve the current preedit string for the input context,
 * and a list of attributes to apply to the string.
 * This string should be displayed inserted at the insertion
 * point.
 **/
void
gtk_im_context_get_preedit_string (GtkIMContext   *context,
				   gchar         **str,
				   PangoAttrList **attrs,
				   gint           *cursor_pos)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  
  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  klass->get_preedit_string (context, str, attrs, cursor_pos);
  g_return_if_fail (str == NULL || g_utf8_validate (*str, -1, NULL));
}

/**
 * gtk_im_context_filter_keypress:
 * @context: a #GtkIMContext
 * @event: the key event
 * 
 * Allow an input method to internally handle a key press event.
 * if this function returns %TRUE, then no further processing
 * should be done for this keystroke.
 * 
 * Return value: %TRUE if the input method handled the keystroke.
 *
 **/
gboolean
gtk_im_context_filter_keypress (GtkIMContext *context,
				GdkEventKey  *key)
{
  GtkIMContextClass *klass;
  
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  return klass->filter_keypress (context, key);
}

/**
 * gtk_im_context_focus_in:
 * @context: a #GtkIMContext
 *
 * Notify the input method that the widget to which this
 * input context corresponds has lost gained. The input method
 * may, for example, change the displayed feedback to reflect
 * this change.
 **/
void
gtk_im_context_focus_in (GtkIMContext   *context)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  
  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->focus_in)
    klass->focus_in (context);
}

/**
 * gtk_im_context_focus_out:
 * @context: a #GtkIMContext
 *
 * Notify the input method that the widget to which this
 * input context corresponds has lost focus. The input method
 * may, for example, change the displayed feedback or reset the contexts
 * state to reflect this change.
 **/
void
gtk_im_context_focus_out (GtkIMContext   *context)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->focus_out)
    klass->focus_out (context);
}

/**
 * gtk_im_context_reset:
 * @context: a #GtkIMContext
 *
 * Notify the input method that a change such as a change in cursor
 * position has been made. This will typically cause the input
 * method to clear the preedit state.
 **/
void
gtk_im_context_reset (GtkIMContext   *context)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->reset)
    klass->reset (context);
}


/**
 * gtk_im_context_set_cursor_location:
 * @context: a #GtkIMContext
 * @area: new location
 *
 * Notify the input method that a change in cursor 
 * position has been made.
 **/
void
gtk_im_context_set_cursor_location (GtkIMContext *context,
				    GdkRectangle *area)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_cursor_location)
    klass->set_cursor_location (context, area);
}

/**
 * gtk_im_context_set_use_preedit:
 * @context: a #GtkIMContext
 * @use_preedit: whether the IM context should use the preedit string.
 * 
 * Sets whether the IM context should use the preedit string
 * to display feedback. If @use_preedit is FALSE (default
 * is TRUE), then the IM context may use some other method to display
 * feedback, such as displaying it in a child of the root window.
 **/
void
gtk_im_context_set_use_preedit (GtkIMContext *context,
				gboolean      use_preedit)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_use_preedit)
    klass->set_use_preedit (context, use_preedit);
}

/**
 * gtk_im_context_set_surrounding:
 * @context: a #GtkIMContext 
 * @text: text surrounding the insertion point, as UTF-8.
 *        the preedit string should not be included within
 *        @text.
 * @len: the length of @text, or -1 if @text is nul-terminated
 * @cursor_index: the byte index of the insertion cursor within @text.
 * 
 * Sets surrounding context around the insertion point and preedit
 * string. This function is expected to be called in response to the
 * GtkIMContext::retrieve_context signal, and will likely have no
 * effect if called at other times.
 **/
void
gtk_im_context_set_surrounding (GtkIMContext  *context,
				const gchar   *text,
				gint           len,
				gint           cursor_index)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  g_return_if_fail (text != NULL || len == 0);

  if (text == NULL && len == 0)
    text = "";
  if (len < 0)
    len = strlen (text);

  g_return_if_fail (cursor_index >= 0 && cursor_index <= len);

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->set_surrounding)
    klass->set_surrounding (context, text, len, cursor_index);
}

/**
 * gtk_im_context_get_surrounding:
 * @context: a #GtkIMContext
 * @text: location to store a UTF-8 encoded string of text
 *        holding context around the insertion point.
 *        If the function returns %TRUE, then you must free
 *        the result stored in this location with g_free().
 * @cursor_index: location to store byte index of the insertion cursor
 *        within @text.
 * 
 * Retrieves context around the insertion point. Input methods
 * typically want context in order to constrain input text based on
 * existing text; this is important for languages such as Thai where
 * only some sequences of characters are allowed.
 *
 * This function is implemented by emitting the
 * GtkIMContext::retrieve_context signal on the input method; in
 * response to this signal, a widget should provide as much context as
 * is available, up to an entire paragraph, by calling
 * gtk_im_context_set_surrounding. Note that there is no obligation
 * for a widget to respond to the ::retrieve_context signal, so input
 * methods must be prepared to function without context.
 *
 * Return value: %TRUE if surrounding text was provided; in this case
 *    you must free the result stored in *text.
 **/
gboolean
gtk_im_context_get_surrounding (GtkIMContext *context,
				gchar       **text,
				gint         *cursor_index)
{
  GtkIMContextClass *klass;
  gchar *local_text = NULL;
  gint local_index;
  gboolean result = FALSE;
  
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);

  klass = GTK_IM_CONTEXT_GET_CLASS (context);
  if (klass->get_surrounding)
    result = klass->get_surrounding (context,
				     text ? text : &local_text,
				     cursor_index ? cursor_index : &local_index);

  if (result)
    g_free (local_text);

  return result;
}

/**
 * gtk_im_context_delete_surrounding:
 * @context: a #GtkIMContext
 * @offset: offset from cursor position in chars;
 *    a negative value means start before the cursor.
 * @n_chars: number of characters to delete.
 * 
 * Asks the widget that the input context is attached to to delete
 * characters around the cursor position by emitting the
 * GtkIMContext::delete_context signal. Note that @offset and @n_chars
 * are in characters not in bytes, which differs from the usage other
 * places in #GtkIMContext.
 *
 * In order to use this function, you should first call
 * gtk_im_context_get_surrounding() to get the current context, and
 * call this function immediately afterwards to make sure that you
 * know what you are deleting. You should also account for the fact
 * that even if the signal was handled, the input context might not
 * have deleted all the characters that were requested to be deleted.
 *
 * This function is used by an input method that wants to make
 * subsitutions in the existing text in response to new input. It is
 * not useful for applications.
 * 
 * Return value: %TRUE if the signal was handled.
 **/
gboolean
gtk_im_context_delete_surrounding (GtkIMContext *context,
				   gint          offset,
				   gint          n_chars)
{
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);

  g_signal_emit (context,
		 im_context_signals[DELETE_SURROUNDING], 0,
		 offset, n_chars, &result);

  return result;
}

