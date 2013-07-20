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
#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
#include <glib.h>
#include "gtkcolorseldialog.h"
#include "gtkframe.h"
#include "gtkbutton.h"
#include "gtkstock.h"
#include "gtkintl.h"
#include "gtkbuildable.h"


/**
 * SECTION:gtkcolorseldlg
 * @Short_description: Deprecated dialog box for selecting a color
 * @Title: GtkColorSelectionDialog
 *
 * The #GtkColorSelectionDialog provides a standard dialog which
 * allows the user to select a color much like the #GtkFileChooserDialog
 * provides a standard dialog for file selection.
 *
 * Use gtk_color_selection_dialog_get_color_selection() to get the
 * #GtkColorSelection widget contained within the dialog. Use this widget
 * and its gtk_color_selection_get_current_color()
 * function to gain access to the selected color.  Connect a handler
 * for this widget's #GtkColorSelection::color-changed signal to be notified
 * when the color changes.
 *
 * <refsect2 id="GtkColorSelectionDialog-BUILDER-UI">
 * <title>GtkColorSelectionDialog as GtkBuildable</title>
 * The GtkColorSelectionDialog implementation of the GtkBuildable interface
 * exposes the embedded #GtkColorSelection as internal child with the
 * name "color_selection". It also exposes the buttons with the names
 * "ok_button", "cancel_button" and "help_button".
 * </refsect2>
 */


struct _GtkColorSelectionDialogPrivate
{
  GtkWidget *colorsel;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;
  GtkWidget *help_button;
};

enum {
  PROP_0,
  PROP_COLOR_SELECTION,
  PROP_OK_BUTTON,
  PROP_CANCEL_BUTTON,
  PROP_HELP_BUTTON
};


/***************************/
/* GtkColorSelectionDialog */
/***************************/

static void gtk_color_selection_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static GObject * gtk_color_selection_dialog_buildable_get_internal_child (GtkBuildable *buildable,
									  GtkBuilder   *builder,
									  const gchar  *childname);

G_DEFINE_TYPE_WITH_CODE (GtkColorSelectionDialog, gtk_color_selection_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (GtkColorSelectionDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_color_selection_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_color_selection_dialog_get_property (GObject         *object,
					 guint            prop_id,
					 GValue          *value,
					 GParamSpec      *pspec)
{
  GtkColorSelectionDialog *colorsel = GTK_COLOR_SELECTION_DIALOG (object);
  GtkColorSelectionDialogPrivate *priv = colorsel->priv;

  switch (prop_id)
    {
    case PROP_COLOR_SELECTION:
      g_value_set_object (value, priv->colorsel);
      break;
    case PROP_OK_BUTTON:
      g_value_set_object (value, priv->ok_button);
      break;
    case PROP_CANCEL_BUTTON:
      g_value_set_object (value, priv->cancel_button);
      break;
    case PROP_HELP_BUTTON:
      g_value_set_object (value, priv->help_button);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->get_property = gtk_color_selection_dialog_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_COLOR_SELECTION,
				   g_param_spec_object ("color-selection",
						     P_("Color Selection"),
						     P_("The color selection embedded in the dialog."),
						     GTK_TYPE_WIDGET,
						     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
				   PROP_OK_BUTTON,
				   g_param_spec_object ("ok-button",
						     P_("OK Button"),
						     P_("The OK button of the dialog."),
						     GTK_TYPE_WIDGET,
						     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
				   PROP_CANCEL_BUTTON,
				   g_param_spec_object ("cancel-button",
						     P_("Cancel Button"),
						     P_("The cancel button of the dialog."),
						     GTK_TYPE_WIDGET,
						     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
				   PROP_HELP_BUTTON,
				   g_param_spec_object ("help-button",
						     P_("Help Button"),
						     P_("The help button of the dialog."),
						     GTK_TYPE_WIDGET,
						     G_PARAM_READABLE));

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_COLOR_CHOOSER);
}

static void
gtk_color_selection_dialog_init (GtkColorSelectionDialog *colorseldiag)
{
  GtkColorSelectionDialogPrivate *priv;
  GtkDialog *dialog = GTK_DIALOG (colorseldiag);
  GtkWidget *action_area, *content_area;

  colorseldiag->priv = gtk_color_selection_dialog_get_instance_private (colorseldiag);
  priv = colorseldiag->priv;

  content_area = gtk_dialog_get_content_area (dialog);
  action_area = gtk_dialog_get_action_area (dialog);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  priv->colorsel = gtk_color_selection_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->colorsel), 5);
  gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION (priv->colorsel), FALSE);
  gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION (priv->colorsel), FALSE);
  gtk_container_add (GTK_CONTAINER (content_area), priv->colorsel);
  gtk_widget_show (priv->colorsel);

  priv->cancel_button = gtk_dialog_add_button (dialog,
                                               _("_Cancel"),
                                               GTK_RESPONSE_CANCEL);

  priv->ok_button = gtk_dialog_add_button (dialog,
                                           _("_Select"),
                                           GTK_RESPONSE_OK);

  gtk_widget_grab_default (priv->ok_button);

  priv->help_button = gtk_dialog_add_button (dialog,
                                             _("_Help"),
                                             GTK_RESPONSE_HELP);

  gtk_widget_hide (priv->help_button);

  gtk_dialog_set_alternative_button_order (dialog,
					   GTK_RESPONSE_OK,
					   GTK_RESPONSE_CANCEL,
					   GTK_RESPONSE_HELP,
					   -1);

  gtk_window_set_title (GTK_WINDOW (colorseldiag),
                        _("Color Selection"));
}

/**
 * gtk_color_selection_dialog_new:
 * @title: a string containing the title text for the dialog.
 *
 * Creates a new #GtkColorSelectionDialog.
 *
 * Returns: a #GtkColorSelectionDialog.
 */
GtkWidget*
gtk_color_selection_dialog_new (const gchar *title)
{
  GtkColorSelectionDialog *colorseldiag;
  
  colorseldiag = g_object_new (GTK_TYPE_COLOR_SELECTION_DIALOG, NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (colorseldiag), title);

  gtk_window_set_resizable (GTK_WINDOW (colorseldiag), FALSE);
  
  return GTK_WIDGET (colorseldiag);
}

/**
 * gtk_color_selection_dialog_get_color_selection:
 * @colorsel: a #GtkColorSelectionDialog
 *
 * Retrieves the #GtkColorSelection widget embedded in the dialog.
 *
 * Returns: (transfer none): the embedded #GtkColorSelection
 *
 * Since: 2.14
 **/
GtkWidget*
gtk_color_selection_dialog_get_color_selection (GtkColorSelectionDialog *colorsel)
{
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION_DIALOG (colorsel), NULL);

  return colorsel->priv->colorsel;
}

static void
gtk_color_selection_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_color_selection_dialog_buildable_get_internal_child;
}

static GObject *
gtk_color_selection_dialog_buildable_get_internal_child (GtkBuildable *buildable,
							 GtkBuilder   *builder,
							 const gchar  *childname)
{
  GtkColorSelectionDialog *selection_dialog = GTK_COLOR_SELECTION_DIALOG (buildable);
  GtkColorSelectionDialogPrivate *priv = selection_dialog->priv;

  if (g_strcmp0 (childname, "ok_button") == 0)
    return G_OBJECT (priv->ok_button);
  else if (g_strcmp0 (childname, "cancel_button") == 0)
    return G_OBJECT (priv->cancel_button);
  else if (g_strcmp0 (childname, "help_button") == 0)
    return G_OBJECT (priv->help_button);
  else if (g_strcmp0 (childname, "color_selection") == 0)
    return G_OBJECT (priv->colorsel);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}
