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
#include "gtksignal.h"

enum {
  PREEDIT_START,
  PREEDIT_END,
  PREEDIT_CHANGED,
  COMMIT,
  LAST_SIGNAL
};

static guint im_context_signals[LAST_SIGNAL] = { 0 };

static void gtk_im_context_class_init (GtkIMContextClass *class);
static void gtk_im_context_init (GtkIMContext *im_context);

static void     gtk_im_context_real_get_preedit_string (GtkIMContext       *context,
							gchar             **str,
							PangoAttrList     **attrs,
							gint               *cursor_pos);
static gboolean gtk_im_context_real_filter_keypress    (GtkIMContext       *context,
							GdkEventKey        *event);

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

  im_context_signals[PREEDIT_START] =
    gtk_signal_new ("preedit_start",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_start),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[PREEDIT_END] =
    gtk_signal_new ("preedit_end",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_end),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[PREEDIT_CHANGED] =
    gtk_signal_new ("preedit_changed",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_changed),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[COMMIT] =
    gtk_signal_new ("commit",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, commit),
		    gtk_marshal_VOID__STRING,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STRING);
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
