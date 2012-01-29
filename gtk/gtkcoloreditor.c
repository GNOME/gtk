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

#include "gtkintl.h"
#include "gtkcolorchooserprivate.h"
#include "gtkcoloreditor.h"
#include "gtkhsv.h"


struct _GtkColorEditorPrivate
{
  GtkWidget *hsv;
  GdkRGBA color;
};

enum
{
  PROP_ZERO,
  PROP_COLOR
};

static void gtk_color_editor_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorEditor, gtk_color_editor, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_editor_iface_init))

static void
hsv_changed (GtkHSV         *hsv,
             GtkColorEditor *editor)
{
  gdouble h, s, v;

  gtk_hsv_get_color (hsv, &h, &s, &v);
  gtk_hsv_to_rgb (h, s, v,
                  &editor->priv->color.red,
                  &editor->priv->color.green,
                  &editor->priv->color.blue);
  editor->priv->color.alpha = 1;
}

static void
gtk_color_editor_init (GtkColorEditor *editor)
{
  GtkWidget *hsv;

  editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (editor,
                                              GTK_TYPE_COLOR_EDITOR,
                                              GtkColorEditorPrivate);
  gtk_widget_push_composite_child ();

  editor->priv->hsv = hsv = gtk_hsv_new ();
  gtk_hsv_set_metrics (GTK_HSV (hsv), 300, 20);
  g_signal_connect (hsv, "changed", G_CALLBACK (hsv_changed), editor);
  gtk_widget_show (hsv);
  gtk_container_add (GTK_CONTAINER (editor), hsv);
  gtk_widget_pop_composite_child ();
}

static void
gtk_color_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      gtk_color_chooser_set_color (cc, g_value_get_boxed (value));
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_class_init (GtkColorEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_color_editor_get_property;
  object_class->set_property = gtk_color_editor_set_property;

  g_object_class_override_property (object_class, PROP_COLOR, "color");

  g_type_class_add_private (class, sizeof (GtkColorEditorPrivate));
}

static void
gtk_color_editor_get_color (GtkColorChooser *chooser,
                            GdkRGBA         *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);

  color->red = editor->priv->color.red;
  color->green = editor->priv->color.green;
  color->blue = editor->priv->color.blue;
  color->alpha = editor->priv->color.alpha;
}

static void
gtk_color_editor_set_color (GtkColorChooser *chooser,
                            const GdkRGBA   *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);
  gdouble h, s, v;

  editor->priv->color.red = color->red;
  editor->priv->color.green = color->green;
  editor->priv->color.blue = color->blue;
  editor->priv->color.alpha = color->alpha;
  gtk_rgb_to_hsv (color->red, color->green, color->blue, &h, &s, &v);
  gtk_hsv_set_color (GTK_HSV (editor->priv->hsv), h, s, v);
  g_object_notify (G_OBJECT (editor), "color");
}

static void
gtk_color_editor_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_color = gtk_color_editor_get_color;
  iface->set_color = gtk_color_editor_set_color;
}

GtkWidget *
gtk_color_editor_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_EDITOR, NULL);
}
