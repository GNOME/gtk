/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechooserdialog.c: File selector dialog
 * Copyright (C) 2003, Red Hat, Inc.
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

#include <config.h>
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserwidget.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserembed.h"
#include "gtkfilesystem.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtkalias.h"

#include <stdarg.h>

#define GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE(o)  (GTK_FILE_CHOOSER_DIALOG (o)->priv)

static void gtk_file_chooser_dialog_finalize   (GObject                   *object);

static GObject* gtk_file_chooser_dialog_constructor  (GType                  type,
						      guint                  n_construct_properties,
						      GObjectConstructParam *construct_params);
static void     gtk_file_chooser_dialog_set_property (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_get_property (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

static void     gtk_file_chooser_dialog_map          (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_unmap        (GtkWidget             *widget);
static void     gtk_file_chooser_dialog_style_set    (GtkWidget             *widget,
						      GtkStyle              *previous_style);

static void response_cb (GtkDialog *dialog,
			 gint       response_id);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserDialog, gtk_file_chooser_dialog, GTK_TYPE_DIALOG,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
						_gtk_file_chooser_delegate_iface_init))

static void
gtk_file_chooser_dialog_class_init (GtkFileChooserDialogClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->constructor = gtk_file_chooser_dialog_constructor;
  gobject_class->set_property = gtk_file_chooser_dialog_set_property;
  gobject_class->get_property = gtk_file_chooser_dialog_get_property;
  gobject_class->finalize = gtk_file_chooser_dialog_finalize;

  widget_class->map       = gtk_file_chooser_dialog_map;
  widget_class->unmap     = gtk_file_chooser_dialog_unmap;
  widget_class->style_set = gtk_file_chooser_dialog_style_set;

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserDialogPrivate));
}

static void
gtk_file_chooser_dialog_init (GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
								   GTK_TYPE_FILE_CHOOSER_DIALOG,
								   GtkFileChooserDialogPrivate);
  dialog->priv = priv;
  dialog->priv->response_requested = FALSE;

  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  /* We do a signal connection here rather than overriding the method in
   * class_init because GtkDialog::response is a RUN_LAST signal.  We want *our*
   * handler to be run *first*, regardless of whether the user installs response
   * handlers of his own.
   */
  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb), NULL);
}

static void
gtk_file_chooser_dialog_finalize (GObject *object)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (object);

  g_free (dialog->priv->file_system);

  G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->finalize (object);  
}

/* Callback used when the user activates a file in the file chooser widget */
static void
file_chooser_widget_file_activated (GtkFileChooser       *chooser,
				    GtkFileChooserDialog *dialog)
{
  GList *children, *l;

  if (gtk_window_activate_default (GTK_WINDOW (dialog)))
    return;

  /* There probably isn't a default widget, so make things easier for the
   * programmer by looking for a reasonable button on our own.
   */

  children = gtk_container_get_children (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area));

  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      int response_id;

      widget = GTK_WIDGET (l->data);
      response_id = gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog), widget);
      if (response_id == GTK_RESPONSE_ACCEPT
	  || response_id == GTK_RESPONSE_OK
	  || response_id == GTK_RESPONSE_YES
	  || response_id == GTK_RESPONSE_APPLY)
	{
	  gtk_widget_activate (widget); /* Should we gtk_dialog_response (dialog, response_id) instead? */
	  break;
	}
    }

  g_list_free (children);
}

static void
clamp_to_screen (GtkWidget *widget,
		 gint      *width,
		 gint      *height)
{
  GdkScreen *screen;
  int monitor_num;
  GdkRectangle monitor;

  g_return_if_fail (GTK_WIDGET_REALIZED (widget));
  
  screen = gtk_widget_get_screen (widget);
  monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  if (width)
    *width = MIN (*width, (monitor.width * 3) / 4);

  if (height)
    *height = MIN (*height, (monitor.height * 3) / 4);
}

static void
file_chooser_widget_default_size_changed (GtkWidget            *widget,
					  GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv;
  gint width, height;
  gint default_width, default_height;
  GtkRequisition req, widget_req;
  gboolean resizable;

  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  /* Unset any previously set size */
  gtk_widget_set_size_request (GTK_WIDGET (dialog), -1, -1);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      /* Force a size request of everything before we start.  This will make sure
       * that widget->requisition is meaningful. */
      gtk_widget_size_request (GTK_WIDGET (dialog), &req);
      gtk_widget_size_request (widget, &widget_req);

      width = req.width - widget_req.width;
      height = req.height - widget_req.height;
    }
  else
    {
      width = GTK_WIDGET (dialog)->allocation.width - widget->allocation.width;
      height = GTK_WIDGET (dialog)->allocation.height - widget->allocation.height;
    }

  resizable = _gtk_file_chooser_embed_get_resizable (GTK_FILE_CHOOSER_EMBED (priv->widget));
  _gtk_file_chooser_embed_get_default_size (GTK_FILE_CHOOSER_EMBED (priv->widget),
					    &default_width, &default_height);

  /* Ideal target size plus any extra size */
  width = default_width + width + (2 * GTK_CONTAINER (dialog)->border_width);
  height = default_height + height + (2 * GTK_CONTAINER (dialog)->border_width);

  if (GTK_WIDGET_REALIZED (dialog))
    clamp_to_screen (GTK_WIDGET (dialog), &width, &height);

  if (resizable)
    {
      gtk_window_set_resizable (GTK_WINDOW (dialog), resizable);
      gtk_window_resize (GTK_WINDOW (dialog), width, height);
    }
  else
    {
      gtk_widget_set_size_request (GTK_WIDGET (dialog), width, -1);
      gtk_window_set_resizable (GTK_WINDOW (dialog), resizable);
    }
}

static void
file_chooser_widget_response_requested (GtkWidget            *widget,
					GtkFileChooserDialog *dialog)
{
  GList *children, *l;

  /* There probably isn't a default widget, so make things easier for the
   * programmer by looking for a reasonable button on our own.
   */

  children = gtk_container_get_children (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area));

  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      int response_id;

      widget = GTK_WIDGET (l->data);
      response_id = gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog), widget);
      if (response_id == GTK_RESPONSE_ACCEPT
	  || response_id == GTK_RESPONSE_OK
	  || response_id == GTK_RESPONSE_YES
	  || response_id == GTK_RESPONSE_APPLY)
	{
	  dialog->priv->response_requested = TRUE;
	  gtk_widget_activate (widget); /* Should we gtk_dialog_response (dialog, response_id) instead? */
	  break;
	}
    }

  g_list_free (children);
}
  
static GObject*
gtk_file_chooser_dialog_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkFileChooserDialogPrivate *priv;
  GObject *object;

  object = G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->constructor (type,
									       n_construct_properties,
									       construct_params);
  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  gtk_widget_push_composite_child ();

  if (priv->file_system)
    priv->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
				 "file-system-backend", priv->file_system,
				 NULL);
  else
    priv->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET, NULL);

  g_signal_connect (priv->widget, "file-activated",
		    G_CALLBACK (file_chooser_widget_file_activated), object);
  g_signal_connect (priv->widget, "default-size-changed",
		    G_CALLBACK (file_chooser_widget_default_size_changed), object);
  g_signal_connect (priv->widget, "response-requested",
		    G_CALLBACK (file_chooser_widget_response_requested), object);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox), priv->widget, TRUE, TRUE, 0);

  gtk_widget_show (priv->widget);

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (object),
				  GTK_FILE_CHOOSER (priv->widget));

  gtk_widget_pop_composite_child ();

  return object;
}

static void
gtk_file_chooser_dialog_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec)

{
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      g_free (priv->file_system);
      priv->file_system = g_value_dup_string (value);
      break;
    default:
      g_object_set_property (G_OBJECT (priv->widget), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_dialog_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  g_object_get_property (G_OBJECT (priv->widget), pspec->name, value);
}

#if 0
static void
set_default_size (GtkFileChooserDialog *dialog)
{
  GtkWidget *widget;
  GtkWindow *window;
  int default_width, default_height;
  int width, height;
  int font_size;
  GdkScreen *screen;
  int monitor_num;
  GtkRequisition req;
  GdkRectangle monitor;

  widget = GTK_WIDGET (dialog);
  window = GTK_WINDOW (dialog);

  /* Size based on characters */

  font_size = pango_font_description_get_size (widget->style->font_desc);
  font_size = PANGO_PIXELS (font_size);

  width = font_size * NUM_CHARS;
  height = font_size * NUM_LINES;

  /* Use at least the requisition size... */

  gtk_widget_size_request (widget, &req);
  width = MAX (width, req.width);
  height = MAX (height, req.height);

  /* ... but no larger than the monitor */

  screen = gtk_widget_get_screen (widget);
  monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  width = MIN (width, monitor.width * 3 / 4);
  height = MIN (height, monitor.height * 3 / 4);

  /* Set size */

  gtk_window_get_default_size (window, &default_width, &default_height);

  gtk_window_set_default_size (window,
			       (default_width == -1) ? width : default_width,
			       (default_height == -1) ? height : default_height);
}
#endif

/* GtkWidget::map handler */
static void
gtk_file_chooser_dialog_map (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  if (!GTK_WIDGET_MAPPED (priv->widget))
    gtk_widget_map (priv->widget);

  file_chooser_widget_default_size_changed (priv->widget, dialog);
  _gtk_file_chooser_embed_initial_focus (GTK_FILE_CHOOSER_EMBED (priv->widget));

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->map (widget);
}

/* GtkWidget::unmap handler */
static void
gtk_file_chooser_dialog_unmap (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->unmap (widget);

  /* See bug #145470.  We unmap the GtkFileChooserWidget so that if the dialog
   * is remapped, the widget will be remapped as well.  Implementations should
   * refresh their contents when this happens, as some applications keep a
   * single file chooser alive and map/unmap it as needed, rather than creating
   * a new file chooser every time they need one.
   */
  gtk_widget_unmap (priv->widget);
}

static void
gtk_file_chooser_dialog_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
  GtkDialog *dialog;

  if (GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->style_set)
    GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->style_set (widget, previous_style);

  dialog = GTK_DIALOG (widget);

  /* Override the style properties with HIG-compliant spacings.  Ugh.
   * http://developer.gnome.org/projects/gup/hig/1.0/layout.html#layout-dialogs
   * http://developer.gnome.org/projects/gup/hig/1.0/windows.html#alert-spacing
   */

  gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox), 12);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 24);

  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 0);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);
}

/* GtkDialog::response handler */
static void
response_cb (GtkDialog *dialog,
	     gint       response_id)
{
  GtkFileChooserDialogPrivate *priv;
  gboolean response_matters;

  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  /* Act only on response IDs we recognize */
  response_matters = 
       response_id == GTK_RESPONSE_ACCEPT
    || response_id == GTK_RESPONSE_OK
    || response_id == GTK_RESPONSE_YES
    || response_id == GTK_RESPONSE_APPLY;

  if (response_matters && !priv->response_requested &&
      !_gtk_file_chooser_embed_should_respond (GTK_FILE_CHOOSER_EMBED (priv->widget)))
    {
      g_signal_stop_emission_by_name (dialog, "response");
    }

  priv->response_requested = FALSE;
}

static GtkWidget *
gtk_file_chooser_dialog_new_valist (const gchar          *title,
				    GtkWindow            *parent,
				    GtkFileChooserAction  action,
				    const gchar          *backend,
				    const gchar          *first_button_text,
				    va_list               varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  gint response_id;

  result = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			 "title", title,
			 "action", action,
			 "file-system-backend", backend,
			 NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  while (button_text)
    {
      response_id = va_arg (varargs, gint);
      gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
      button_text = va_arg (varargs, const gchar *);
    }

  return result;
}

/**
 * gtk_file_chooser_dialog_new:
 * @title: Title of the dialog, or %NULL
 * @parent: Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog
 * @first_button_text: stock ID or text to go in the first button, or %NULL
 * @Varargs: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #GtkFileChooserDialog.  This function is analogous to
 * gtk_dialog_new_with_buttons().
 *
 * Return value: a new #GtkFileChooserDialog
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_dialog_new (const gchar         *title,
			     GtkWindow           *parent,
			     GtkFileChooserAction action,
			     const gchar         *first_button_text,
			     ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
					       NULL, first_button_text,
					       varargs);
  va_end (varargs);

  return result;
}

/**
 * gtk_file_chooser_dialog_new_with_backend:
 * @title: Title of the dialog, or %NULL
 * @parent: Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog
 * @backend: The name of the specific filesystem backend to use.
 * @first_button_text: stock ID or text to go in the first button, or %NULL
 * @Varargs: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #GtkFileChooserDialog with a specified backend. This is
 * especially useful if you use gtk_file_chooser_set_local_only() to allow
 * non-local files and you use a more expressive vfs, such as gnome-vfs,
 * to load files.
 *
 * Return value: a new #GtkFileChooserDialog
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_dialog_new_with_backend (const gchar          *title,
					  GtkWindow            *parent,
					  GtkFileChooserAction  action,
					  const gchar          *backend,
					  const gchar          *first_button_text,
					  ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
					       backend, first_button_text,
					       varargs);
  va_end (varargs);

  return result;
}

#define __GTK_FILE_CHOOSER_DIALOG_C__
#include "gtkaliasdef.c"
