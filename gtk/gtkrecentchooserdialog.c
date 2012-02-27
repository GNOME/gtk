/* GTK - The GIMP Toolkit
 * gtkrecentchooserdialog.c: Recent files selector dialog
 * Copyright (C) 2006 Emmanuele Bassi
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

#include "gtkrecentchooserdialog.h"
#include "gtkrecentchooserwidget.h"
#include "gtkrecentchooserutils.h"
#include "gtkrecentmanager.h"
#include "gtktypebuiltins.h"

#include <stdarg.h>


/**
 * SECTION:gtkrecentchooserdialog
 * @Short_description: Displays recently used files in a dialog
 * @Title: GtkRecentChooserDialog
 * @See_also:#GtkRecentChooser, #GtkDialog
 *
 * #GtkRecentChooserDialog is a dialog box suitable for displaying the recently
 * used documents.  This widgets works by putting a #GtkRecentChooserWidget inside
 * a #GtkDialog.  It exposes the #GtkRecentChooserIface interface, so you can use
 * all the #GtkRecentChooser functions on the recent chooser dialog as well as
 * those for #GtkDialog.
 *
 * Note that #GtkRecentChooserDialog does not have any methods of its own.
 * Instead, you should use the functions that work on a #GtkRecentChooser.
 *
 * <example id="gtkrecentchooser-typical-usage">
 * <title>Typical usage</title>
 * In the simplest of cases, you can use the following code to use
 * a #GtkRecentChooserDialog to select a recently used file:
 * <programlisting>
 * GtkWidget *dialog;
 *
 * dialog = gtk_recent_chooser_dialog_new ("Recent Documents",
 *                                         parent_window,
 *                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
 *                                         NULL);
 *
 * if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 *   {
 *     GtkRecentInfo *info;
 *
 *     info = gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER (dialog));
 *     open_file (gtk_recent_info_get_uri (info));
 *     gtk_recent_info_unref (info);
 *   }
 *
 * gtk_widget_destroy (dialog);
 * </programlisting>
 * </example>
 *
 * Recently used files are supported since GTK+ 2.10.
 */


struct _GtkRecentChooserDialogPrivate
{
  GtkRecentManager *manager;
  
  GtkWidget *chooser;
};

#define GTK_RECENT_CHOOSER_DIALOG_GET_PRIVATE(obj)	(GTK_RECENT_CHOOSER_DIALOG (obj)->priv)

static void gtk_recent_chooser_dialog_class_init (GtkRecentChooserDialogClass *klass);
static void gtk_recent_chooser_dialog_init       (GtkRecentChooserDialog      *dialog);
static void gtk_recent_chooser_dialog_finalize   (GObject                     *object);

static GObject *gtk_recent_chooser_dialog_constructor (GType                  type,
						       guint                  n_construct_properties,
						       GObjectConstructParam *construct_params);

static void gtk_recent_chooser_dialog_set_property (GObject      *object,
						    guint         prop_id,
						    const GValue *value,
						    GParamSpec   *pspec);
static void gtk_recent_chooser_dialog_get_property (GObject      *object,
						    guint         prop_id,
						    GValue       *value,
						    GParamSpec   *pspec);

G_DEFINE_TYPE_WITH_CODE (GtkRecentChooserDialog,
			 gtk_recent_chooser_dialog,
			 GTK_TYPE_DIALOG,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
		       				_gtk_recent_chooser_delegate_iface_init))

static void
gtk_recent_chooser_dialog_class_init (GtkRecentChooserDialogClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->set_property = gtk_recent_chooser_dialog_set_property;
  gobject_class->get_property = gtk_recent_chooser_dialog_get_property;
  gobject_class->constructor = gtk_recent_chooser_dialog_constructor;
  gobject_class->finalize = gtk_recent_chooser_dialog_finalize;
  
  _gtk_recent_chooser_install_properties (gobject_class);
  
  g_type_class_add_private (klass, sizeof (GtkRecentChooserDialogPrivate));
}

static void
gtk_recent_chooser_dialog_init (GtkRecentChooserDialog *dialog)
{
  GtkWidget *content_area, *action_area;

  GtkRecentChooserDialogPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
  								     GTK_TYPE_RECENT_CHOOSER_DIALOG,
  								     GtkRecentChooserDialogPrivate);
  GtkDialog *rc_dialog = GTK_DIALOG (dialog);
  
  dialog->priv = priv;

  content_area = gtk_dialog_get_content_area (rc_dialog);
  action_area = gtk_dialog_get_action_area (rc_dialog);

  gtk_container_set_border_width (GTK_CONTAINER (rc_dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);

}

/* we intercept the GtkRecentChooser::item_activated signal and try to
 * make the dialog emit a valid response signal
 */
static void
gtk_recent_chooser_item_activated_cb (GtkRecentChooser *chooser,
				      gpointer          user_data)
{
  GtkDialog *rc_dialog;
  GtkRecentChooserDialog *dialog;
  GtkWidget *action_area;
  GList *children, *l;

  dialog = GTK_RECENT_CHOOSER_DIALOG (user_data);
  rc_dialog = GTK_DIALOG (dialog);

  if (gtk_window_activate_default (GTK_WINDOW (dialog)))
    return;

  action_area = gtk_dialog_get_action_area (rc_dialog);
  children = gtk_container_get_children (GTK_CONTAINER (action_area));
  
  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      gint response_id;
      
      widget = GTK_WIDGET (l->data);
      response_id = gtk_dialog_get_response_for_widget (rc_dialog, widget);
      
      if (response_id == GTK_RESPONSE_ACCEPT ||
          response_id == GTK_RESPONSE_OK     ||
          response_id == GTK_RESPONSE_YES    ||
          response_id == GTK_RESPONSE_APPLY)
        {
          g_list_free (children);
	  
          gtk_dialog_response (GTK_DIALOG (dialog), response_id);

          return;
        }
    }
  
  g_list_free (children);
}

static GObject *
gtk_recent_chooser_dialog_constructor (GType                  type,
				       guint                  n_construct_properties,
				       GObjectConstructParam *construct_params)
{
  GtkRecentChooserDialogPrivate *priv;
  GtkWidget *content_area;
  GObject *object;

  object = G_OBJECT_CLASS (gtk_recent_chooser_dialog_parent_class)->constructor (type,
		  							         n_construct_properties,
										 construct_params);
  priv = GTK_RECENT_CHOOSER_DIALOG_GET_PRIVATE (object);
  
  gtk_widget_push_composite_child ();
  
  if (priv->manager)
    priv->chooser = g_object_new (GTK_TYPE_RECENT_CHOOSER_WIDGET,
  				  "recent-manager", priv->manager,
  				  NULL);
  else
    priv->chooser = g_object_new (GTK_TYPE_RECENT_CHOOSER_WIDGET, NULL);
  
  g_signal_connect (priv->chooser, "item-activated",
  		    G_CALLBACK (gtk_recent_chooser_item_activated_cb),
  		    object);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (object));

  gtk_container_set_border_width (GTK_CONTAINER (priv->chooser), 5);
  gtk_box_pack_start (GTK_BOX (content_area),
                      priv->chooser, TRUE, TRUE, 0);
  gtk_widget_show (priv->chooser);
  
  _gtk_recent_chooser_set_delegate (GTK_RECENT_CHOOSER (object),
  				    GTK_RECENT_CHOOSER (priv->chooser));
  
  gtk_widget_pop_composite_child ();
  
  return object;
}

static void
gtk_recent_chooser_dialog_set_property (GObject      *object,
					guint         prop_id,
					const GValue *value,
					GParamSpec   *pspec)
{
  GtkRecentChooserDialogPrivate *priv;
  
  priv = GTK_RECENT_CHOOSER_DIALOG_GET_PRIVATE (object);
  
  switch (prop_id)
    {
    case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
      priv->manager = g_value_get_object (value);
      break;
    default:
      g_object_set_property (G_OBJECT (priv->chooser), pspec->name, value);
      break;
    }
}

static void
gtk_recent_chooser_dialog_get_property (GObject      *object,
					guint         prop_id,
					GValue       *value,
					GParamSpec   *pspec)
{
  GtkRecentChooserDialogPrivate *priv;
  
  priv = GTK_RECENT_CHOOSER_DIALOG_GET_PRIVATE (object);
  
  g_object_get_property (G_OBJECT (priv->chooser), pspec->name, value);
}

static void
gtk_recent_chooser_dialog_finalize (GObject *object)
{
  GtkRecentChooserDialog *dialog = GTK_RECENT_CHOOSER_DIALOG (object);
 
  dialog->priv->manager = NULL;
  
  G_OBJECT_CLASS (gtk_recent_chooser_dialog_parent_class)->finalize (object);
}

static GtkWidget *
gtk_recent_chooser_dialog_new_valist (const gchar      *title,
				      GtkWindow        *parent,
				      GtkRecentManager *manager,
				      const gchar      *first_button_text,
				      va_list           varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  gint response_id;
  
  result = g_object_new (GTK_TYPE_RECENT_CHOOSER_DIALOG,
                         "title", title,
                         "recent-manager", manager,
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
 * gtk_recent_chooser_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL,
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id)
 *   pairs, ending with %NULL
 *
 * Creates a new #GtkRecentChooserDialog.  This function is analogous to
 * gtk_dialog_new_with_buttons().
 *
 * Return value: a new #GtkRecentChooserDialog
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_dialog_new (const gchar *title,
			       GtkWindow   *parent,
			       const gchar *first_button_text,
			       ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_recent_chooser_dialog_new_valist (title,
  						 parent,
  						 NULL,
  						 first_button_text,
  						 varargs);
  va_end (varargs);
  
  return result;
}

/**
 * gtk_recent_chooser_dialog_new_for_manager:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL,
 * @manager: a #GtkRecentManager
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id)
 *   pairs, ending with %NULL
 *
 * Creates a new #GtkRecentChooserDialog with a specified recent manager.
 *
 * This is useful if you have implemented your own recent manager, or if you
 * have a customized instance of a #GtkRecentManager object.
 *
 * Return value: a new #GtkRecentChooserDialog
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_dialog_new_for_manager (const gchar      *title,
			                   GtkWindow        *parent,
			                   GtkRecentManager *manager,
			                   const gchar      *first_button_text,
			                   ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_recent_chooser_dialog_new_valist (title,
  						 parent,
  						 manager,
  						 first_button_text,
  						 varargs);
  va_end (varargs);
  
  return result;
}
