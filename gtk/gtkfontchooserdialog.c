/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gtkfontchooserdialog.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserwidget.h"
#include "gtkfontchooserutils.h"
#include "gtkbox.h"
#include "gtkstock.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkbuildable.h"
#include "gtkprivate.h"
#include "gtkwidget.h"

struct _GtkFontChooserDialogPrivate
{
  GtkWidget *fontchooser;

  GtkWidget *select_button;
  GtkWidget *cancel_button;
};

/**
 * SECTION:gtkfontchooserdialog
 * @Short_description: A dialog for selecting fonts
 * @Title: GtkFontChooserDialog
 * @See_also: #GtkFontChooser, #GtkDialog
 *
 * The #GtkFontChooserDialog widget is a dialog for selecting a font.
 * It implements the #GtkFontChooser interface.
 *
 * <refsect2 id="GtkFontChooserDialog-BUILDER-UI">
 * <title>GtkFontChooserDialog as GtkBuildable</title>
 * The GtkFontChooserDialog implementation of the GtkBuildable interface
 * exposes the buttons with the names
 * "select_button" and "cancel_button.
 * </refsect2>
 *
 * Since: 3.2
 */

static void     gtk_font_chooser_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static GObject *gtk_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                                      GtkBuilder   *builder,
                                                                      const gchar  *childname);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserDialog, gtk_font_chooser_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                _gtk_font_chooser_delegate_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_font_chooser_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_font_chooser_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (object);
  GtkFontChooserDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    default:
      g_object_set_property (G_OBJECT (priv->fontchooser), pspec->name, value);
      break;
    }
}

static void
gtk_font_chooser_dialog_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (object);
  GtkFontChooserDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    default:
      g_object_get_property (G_OBJECT (priv->fontchooser), pspec->name, value);
      break;
    }
}

static void
gtk_font_chooser_dialog_class_init (GtkFontChooserDialogClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_font_chooser_dialog_get_property;
  gobject_class->set_property = gtk_font_chooser_dialog_set_property;

  _gtk_font_chooser_install_properties (gobject_class);

  g_type_class_add_private (klass, sizeof (GtkFontChooserDialogPrivate));
}

static void
font_activated_cb (GtkFontChooser *fontchooser,
                   const gchar    *fontname,
                   gpointer        user_data)
{
  GtkDialog *dialog = user_data;

  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
gtk_font_chooser_dialog_init (GtkFontChooserDialog *fontchooserdiag)
{
  GtkFontChooserDialogPrivate *priv;
  GtkDialog *dialog = GTK_DIALOG (fontchooserdiag);
  GtkWidget *action_area, *content_area;

  fontchooserdiag->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontchooserdiag,
                                                       GTK_TYPE_FONT_CHOOSER_DIALOG,
                                                       GtkFontChooserDialogPrivate);
  priv = fontchooserdiag->priv;

  content_area = gtk_dialog_get_content_area (dialog);
  action_area = gtk_dialog_get_action_area (dialog);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  gtk_widget_push_composite_child ();

  gtk_window_set_resizable (GTK_WINDOW (fontchooserdiag), TRUE);

  /* Create the content area */
  priv->fontchooser = gtk_font_chooser_widget_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->fontchooser), 5);
  gtk_widget_show (priv->fontchooser);
  gtk_box_pack_start (GTK_BOX (content_area),
                      priv->fontchooser, TRUE, TRUE, 0);

  g_signal_connect (priv->fontchooser, "font-activated",
                    G_CALLBACK (font_activated_cb), dialog);

  /* Create the action area */
  priv->cancel_button = gtk_dialog_add_button (dialog,
                                               GTK_STOCK_CANCEL,
                                               GTK_RESPONSE_CANCEL);
  priv->select_button = gtk_dialog_add_button (dialog,
                                               _("_Select"),
                                               GTK_RESPONSE_OK);
  gtk_widget_grab_default (priv->select_button);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (fontchooserdiag),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gtk_window_set_title (GTK_WINDOW (fontchooserdiag), _("Font Selection"));

  gtk_widget_pop_composite_child ();

  _gtk_font_chooser_set_delegate (GTK_FONT_CHOOSER (fontchooserdiag),
                                  GTK_FONT_CHOOSER (priv->fontchooser));
}

/**
 * gtk_font_chooser_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 *
 * Creates a new #GtkFontChooserDialog.
 *
 * Return value: a new #GtkFontChooserDialog
 *
 * Since: 3.2
 */
GtkWidget*
gtk_font_chooser_dialog_new (const gchar *title,
                             GtkWindow   *parent)
{
  GtkFontChooserDialog *dialog;

  dialog = g_object_new (GTK_TYPE_FONT_CHOOSER_DIALOG,
                         "title", title,
                         "transient-for", parent,
                         NULL);

  return GTK_WIDGET (dialog);
}

static void
gtk_font_chooser_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_font_chooser_dialog_buildable_get_internal_child;
}

static GObject *
gtk_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                      GtkBuilder   *builder,
                                                      const gchar  *childname)
{
  GtkFontChooserDialogPrivate *priv;

  priv = GTK_FONT_CHOOSER_DIALOG (buildable)->priv;

  if (g_strcmp0 (childname, "select_button") == 0)
    return G_OBJECT (priv->select_button);
  else if (g_strcmp0 (childname, "cancel_button") == 0)
    return G_OBJECT (priv->cancel_button);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}
