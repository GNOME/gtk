/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "gtkdialog.h"
#include "gtkstock.h"
#include "gtkbox.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserdialog.h"
#include "gtkcolorchooserwidget.h"


struct _GtkColorChooserDialogPrivate
{
  GtkWidget *color_chooser;

  GtkWidget *select_button;
  GtkWidget *cancel_button;
};

enum
{
  PROP_ZERO,
  PROP_COLOR,
  PROP_SHOW_ALPHA,
  PROP_SHOW_EDITOR
};

static void gtk_color_chooser_dialog_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorChooserDialog, gtk_color_chooser_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_chooser_dialog_iface_init))

static void
propagate_notify (GObject               *o,
                  GParamSpec            *pspec,
                  GtkColorChooserDialog *cc)
{
  g_object_notify (G_OBJECT (cc), "color");
}

static void
color_activated_cb (GtkColorChooser *chooser,
                    GdkRGBA         *color,
                    GtkDialog       *dialog)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
gtk_color_chooser_dialog_init (GtkColorChooserDialog *cc)
{
  GtkColorChooserDialogPrivate *priv;
  GtkDialog *dialog = GTK_DIALOG (cc);
  GtkWidget *action_area, *content_area;

  cc->priv = G_TYPE_INSTANCE_GET_PRIVATE (cc,
                                          GTK_TYPE_COLOR_CHOOSER_DIALOG,
                                          GtkColorChooserDialogPrivate);
  priv = cc->priv;

  content_area = gtk_dialog_get_content_area (dialog);
  action_area = gtk_dialog_get_action_area (dialog);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  gtk_widget_push_composite_child ();

  gtk_window_set_resizable (GTK_WINDOW (cc), FALSE);

  /* Create the content area */
  priv->color_chooser = gtk_color_chooser_widget_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->color_chooser), 5);
  gtk_widget_show (priv->color_chooser);
  gtk_box_pack_start (GTK_BOX (content_area),
                      priv->color_chooser, TRUE, TRUE, 0);

  g_signal_connect (priv->color_chooser, "notify::color",
                    G_CALLBACK (propagate_notify), cc);

  g_signal_connect (priv->color_chooser, "color-activated",
                    G_CALLBACK (color_activated_cb), cc);

  /* Create the action area */
  priv->cancel_button = gtk_dialog_add_button (dialog,
                                               GTK_STOCK_CANCEL,
                                               GTK_RESPONSE_CANCEL);
  priv->select_button = gtk_dialog_add_button (dialog,
                                               _("_Select"),
                                               GTK_RESPONSE_OK);
  gtk_widget_grab_default (priv->select_button);

  gtk_dialog_set_alternative_button_order (dialog,
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gtk_window_set_title (GTK_WINDOW (cc), _("Select a Color"));

  gtk_widget_pop_composite_child ();
}

static void
gtk_color_chooser_dialog_response (GtkDialog *dialog,
                                   gint       response_id)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      GdkRGBA color;

      gtk_color_chooser_get_color (GTK_COLOR_CHOOSER (dialog), &color);
      gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (dialog), &color);
    }

  g_object_set (GTK_COLOR_CHOOSER_DIALOG (dialog)->priv->color_chooser,
                "show-editor", FALSE, NULL);
}

static void
gtk_color_chooser_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkColorChooserDialog *cd = GTK_COLOR_CHOOSER_DIALOG (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      {
        GdkRGBA color;

        gtk_color_chooser_get_color (cc, &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_SHOW_ALPHA:
      g_value_set_boolean (value, gtk_color_chooser_get_show_alpha (GTK_COLOR_CHOOSER (cd->priv->color_chooser)));
      break;
    case PROP_SHOW_EDITOR:
      {
        gboolean show_editor;
        g_object_get (cd->priv->color_chooser, "show-editor", &show_editor, NULL);
        g_value_set_boolean (value, show_editor);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserDialog *cd = GTK_COLOR_CHOOSER_DIALOG (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      gtk_color_chooser_set_color (cc, g_value_get_boxed (value));
      break;
    case PROP_SHOW_ALPHA:
      gtk_color_chooser_set_show_alpha (GTK_COLOR_CHOOSER (cd->priv->color_chooser), g_value_get_boolean (value));
      break;
    case PROP_SHOW_EDITOR:
      g_object_set (cd->priv->color_chooser,
                    "show-editor", g_value_get_boolean (value),
                    NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_dialog_class_init (GtkColorChooserDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (class);

  object_class->get_property = gtk_color_chooser_dialog_get_property;
  object_class->set_property = gtk_color_chooser_dialog_set_property;

  dialog_class->response = gtk_color_chooser_dialog_response;

  g_object_class_override_property (object_class, PROP_COLOR, "color");
  g_object_class_override_property (object_class, PROP_SHOW_ALPHA, "show-alpha");
  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", P_("Show editor"), P_("Show editor"),
                            FALSE, GTK_PARAM_READWRITE));


  g_type_class_add_private (class, sizeof (GtkColorChooserDialogPrivate));
}

static void
gtk_color_chooser_dialog_get_color (GtkColorChooser *chooser,
                                    GdkRGBA         *color)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (chooser);

  gtk_color_chooser_get_color (GTK_COLOR_CHOOSER (cc->priv->color_chooser), color);
}

static void
gtk_color_chooser_dialog_set_color (GtkColorChooser *chooser,
                                    const GdkRGBA   *color)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (chooser);

  gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (cc->priv->color_chooser), color);
}

static void
gtk_color_chooser_dialog_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_color = gtk_color_chooser_dialog_get_color;
  iface->set_color = gtk_color_chooser_dialog_set_color;
}

GtkWidget *
gtk_color_chooser_dialog_new (const gchar *title,
                              GtkWindow   *parent)
{
  GtkColorChooserDialog *dialog;

  dialog = g_object_new (GTK_TYPE_COLOR_CHOOSER_DIALOG,
                         "title", title,
                         "transient-for", parent,
                         NULL);

  return GTK_WIDGET (dialog);
}
