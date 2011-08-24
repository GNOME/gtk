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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gtkfontchooserdialog.h"
#include "gtkfontchooser.h"
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
 * SECTION:gtkfontchooserdlg
 * @Short_description: A dialog box for selecting fonts
 * @Title: GtkFontChooserDialog
 * @See_also: #GtkFontChooser, #GtkDialog
 *
 * The #GtkFontChooserDialog widget is a dialog box for selecting a font.
 *
 * To set the font which is initially selected, use
 * gtk_font_chooser_dialog_set_font_name().
 *
 * To get the selected font use gtk_font_chooser_dialog_get_font_name().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_chooser_dialog_set_preview_text().
 *
 * <refsect2 id="GtkFontChooserDialog-BUILDER-UI">
 * <title>GtkFontChooserDialog as GtkBuildable</title>
 * The GtkFontChooserDialog implementation of the GtkBuildable interface
 * exposes the embedded #GtkFontChooser as internal child with the
 * name "font_chooser". It also exposes the buttons with the names
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
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                         gtk_font_chooser_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_font_chooser_dialog_class_init (GtkFontChooserDialogClass *klass)
{
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
  priv->fontchooser = gtk_font_chooser_new ();
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
}

/** gtk_font_chooser_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Trasient parent of the dialog, or %NULL
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

/**
 * gtk_font_chooser_dialog_get_font_chooser:
 * @fcd: a #GtkFontChooserDialog
 *
 * Retrieves the #GtkFontChooser widget embedded in the dialog.
 *
 * Returns: (transfer none): the embedded #GtkFontChooser
 *
 * Since: 3.2
 */
GtkWidget*
gtk_font_chooser_dialog_get_font_chooser (GtkFontChooserDialog *fcd)
{
  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), NULL);

  return fcd->priv->fontchooser;
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
  else if (g_strcmp0 (childname, "font_chooser") == 0)
    return G_OBJECT (priv->fontchooser);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/**
 * gtk_font_chooser_dialog_get_font_name:
 * @fcd: a #GtkFontChooserDialog
 *
 * Gets the currently-selected font name.
 *
 * Note that this can be a different string than what you set with
 * gtk_font_chooser_dialog_set_font_name(), as the font chooser widget
 * may normalize font names and thus return a string with a different
 * structure. For example, "Helvetica Italic Bold 12" could be normalized
 * to "Helvetica Bold Italic 12".
 *
 * Use pango_font_description_equal() if you want to compare two
 * font descriptions.
 *
 * Return value: A string with the name of the current font, or %NULL
 *     if no font is selected. You must free this string with g_free().
 *
 * Since: 3.2
 */
gchar*
gtk_font_chooser_dialog_get_font_name (GtkFontChooserDialog *fcd)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), NULL);

  priv = fcd->priv;

  return gtk_font_chooser_get_font_name (GTK_FONT_CHOOSER (priv->fontchooser));
}

/**
 * gtk_font_chooser_dialog_set_font_name:
 * @fcd: a #GtkFontChooserDialog
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 *
 * Sets the currently selected font.
 *
 * Return value: %TRUE if the font selected in @fcd is now the
 *     @fontname specified, %FALSE otherwise.
 *
 * Since: 3.2
 */
gboolean
gtk_font_chooser_dialog_set_font_name (GtkFontChooserDialog *fcd,
                                       const gchar          *fontname)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), FALSE);
  g_return_val_if_fail (fontname, FALSE);

  priv = fcd->priv;

  return gtk_font_chooser_set_font_name (GTK_FONT_CHOOSER (priv->fontchooser), fontname);
}

/**
 * gtk_font_chooser_dialog_get_preview_text:
 * @fcd: a #GtkFontChooserDialog
 *
 * Gets the text displayed in the preview area.
 *
 * Return value: the text displayed in the preview area.
 *     This string is owned by the widget and should not be
 *     modified or freed
 *
 * Since: 3.2
 */
const gchar*
gtk_font_chooser_dialog_get_preview_text (GtkFontChooserDialog *fcd)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd), NULL);

  priv = fcd->priv;

  return gtk_font_chooser_get_preview_text (GTK_FONT_CHOOSER (priv->fontchooser));
}

/**
 * gtk_font_chooser_dialog_set_preview_text:
 * @fcd: a #GtkFontChooserDialog
 * @text: the text to display in the preview area
 *
 * Sets the text displayed in the preview area.
 *
 * Since: 3.2
 */
void
gtk_font_chooser_dialog_set_preview_text (GtkFontChooserDialog *fcd,
                                          const gchar          *text)
{
  GtkFontChooserDialogPrivate *priv;

  g_return_if_fail (GTK_IS_FONT_CHOOSER_DIALOG (fcd));
  g_return_if_fail (text != NULL);

  priv = fcd->priv;

  gtk_font_chooser_set_preview_text (GTK_FONT_CHOOSER (priv->fontchooser), text);
}
