/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

GtkType
gtk_im_context_get_type (void)
{
  static GtkType im_context_type = 0;

  if (!im_context_type)
    {
      static const GtkTypeInfo im_context_info =
      {
	"GtkIMContext",
	sizeof (GtkIMContext),
	sizeof (GtkIMContextClass),
	(GtkClassInitFunc) gtk_im_context_class_init,
	(GtkObjectInitFunc) gtk_im_context_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      im_context_type = gtk_type_unique (GTK_TYPE_OBJECT, &im_context_info);
    }

  return im_context_type;
}

static void
gtk_im_context_class_init (GtkIMContextClass *class)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) class;

  im_context_signals[PREEDIT_START] =
    gtk_signal_new ("preedit_start",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_start),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[PREEDIT_END] =
    gtk_signal_new ("preedit_end",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_end),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[PREEDIT_CHANGED] =
    gtk_signal_new ("preedit_changed",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, preedit_changed),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  
  im_context_signals[COMMIT] =
    gtk_signal_new ("commit",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkIMContextClass, commit),
		    gtk_marshal_NONE__STRING,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_STRING);
  
  gtk_object_class_add_signals (object_class, im_context_signals, LAST_SIGNAL);
}

static void
gtk_im_context_init (GtkIMContext *im_context)
{
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
  
  g_return_if_fail (context != NULL);
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = (GtkIMContextClass *)((GtkObject *)(context))->klass;
  if (klass->set_client_window)
    klass->set_client_window (context, window);
}

/**
 * gtk_im_context_get_preedit_string:
 * @context: a #GtkIMContext
 * @str:     location to store the retrieved string. The
 *           string retrieved must be freed with g_free ().
 * @attrs:   location to store the retrieved attribute list.
 *           When you are done with this list, you must
 *           unreference it with pango_attr_list_unref().
 * 
 * Retrieve the current preedit string for the input context,
 * and a list of attributes to apply to the string.
 * This string should be displayed inserted at the insertion
 * point.
 **/
void
gtk_im_context_get_preedit_string (GtkIMContext   *context,
				   gchar         **str,
				   PangoAttrList **attrs)
{
  GtkIMContextClass *klass;
  
  g_return_if_fail (context != NULL);
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  
  klass = (GtkIMContextClass *)((GtkObject *)(context))->klass;
  if (klass->get_preedit_string)
    klass->get_preedit_string (context, str, attrs);
  else
    {
      if (str)
	*str = g_strdup ("");
      if (attrs)
	*attrs = pango_attr_list_new ();
    }
}

/**
 * gtk_im_context_filter_keypress:
 * @context: a #GtkIMContext
 * @key: the key event
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
  
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_IM_CONTEXT (context), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  klass = (GtkIMContextClass *)((GtkObject *)(context))->klass;
  if (klass->filter_keypress)
    return klass->filter_keypress (context, key);
  else
    return FALSE;
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
  
  g_return_if_fail (context != NULL);
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));
  
  klass = (GtkIMContextClass *)((GtkObject *)(context))->klass;
  if (klass->focus_in)
    klass->focus_in (context);
}

/**
 * gtk_im_context_focus_in:
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
  
  g_return_if_fail (context != NULL);
  g_return_if_fail (GTK_IS_IM_CONTEXT (context));

  klass = (GtkIMContextClass *)((GtkObject *)(context))->klass;
  if (klass->focus_out)
    klass->focus_out (context);
}


