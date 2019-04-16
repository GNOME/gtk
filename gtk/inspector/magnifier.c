/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "magnifier.h"

#include "gtkmagnifierprivate.h"

#include "gtkadjustment.h"
#include "gtkstack.h"
#include "gtkdiffpaintableprivate.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkpicture.h"
#include "gtkrendererpaintableprivate.h"
#include "gtkwidgetpaintable.h"

#ifdef GDK_WINDOWING_BROADWAY
#include "gsk/gskbroadwayrendererprivate.h"
#endif
#include "gsk/gskcairorendererprivate.h"
#include "gsk/gl/gskglrendererprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gsk/vulkan/gskvulkanrendererprivate.h"
#endif

enum
{
  PROP_0,
  PROP_ADJUSTMENT
};

struct _GtkInspectorMagnifierPrivate
{
  GtkWidget *object;
  GtkWidget *magnifier;
  GtkWidget *renderer_listbox;
  GtkAdjustment *adjustment;
  GListStore *renderers;
  GdkPaintable *paintable;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorMagnifier, gtk_inspector_magnifier, GTK_TYPE_BOX)

static void
gtk_inspector_magnifier_add_renderer (GtkInspectorMagnifier *self,
                                      GType                  renderer_type,
                                      const char            *description)
{
  GtkInspectorMagnifierPrivate *priv = gtk_inspector_magnifier_get_instance_private (self);
  GskRenderer *renderer;
  GdkSurface *surface;
  GdkPaintable *paintable;

  surface = gtk_widget_get_surface (GTK_WIDGET (self));
  g_assert (surface != NULL);

  if (renderer_type == G_TYPE_NONE)
    renderer = NULL;
  else
    {
      renderer = g_object_new (renderer_type, NULL);

      if (!gsk_renderer_realize (renderer, surface, NULL))
        {
          g_object_unref (renderer);
          return;
        }
    }

  paintable = gtk_renderer_paintable_new (renderer, priv->paintable);
  g_object_set_data_full (G_OBJECT (paintable), "description", g_strdup (description), g_free);
  g_clear_object (&renderer);

  g_list_store_append (priv->renderers, paintable);
  g_object_unref (paintable);
}

static void
gtk_inspector_magnifier_realize (GtkWidget *widget)
{
  GtkInspectorMagnifier *self = GTK_INSPECTOR_MAGNIFIER (widget);

  GTK_WIDGET_CLASS (gtk_inspector_magnifier_parent_class)->realize (widget);

  gtk_inspector_magnifier_add_renderer (self,
                                        G_TYPE_NONE,
                                        "Default");
  gtk_inspector_magnifier_add_renderer (self,
                                        GSK_TYPE_GL_RENDERER,
                                        "OpenGL");
#ifdef GDK_RENDERING_VULKAN
  gtk_inspector_magnifier_add_renderer (self,
                                        GSK_TYPE_VULKAN_RENDERER,
                                        "Vulkan");
#endif
#ifdef GDK_WINDOWING_BROADWAY
  gtk_inspector_magnifier_add_renderer (self,
                                        GSK_TYPE_BROADWAY_RENDERER,
                                        "Broadway");
#endif
  gtk_inspector_magnifier_add_renderer (self,
                                        GSK_TYPE_CAIRO_RENDERER,
                                        "Cairo");
}

static void
gtk_inspector_magnifier_unrealize (GtkWidget *widget)
{
  GtkInspectorMagnifier *self = GTK_INSPECTOR_MAGNIFIER (widget);
  GtkInspectorMagnifierPrivate *priv = gtk_inspector_magnifier_get_instance_private (self);

  g_list_store_remove_all (priv->renderers);

  GTK_WIDGET_CLASS (gtk_inspector_magnifier_parent_class)->unrealize (widget);
}

static GtkWidget *
gtk_inspector_magnifier_create_renderer_widget (gpointer item,
                                                gpointer user_data)
{
  GtkInspectorMagnifier *self = user_data;
  GtkInspectorMagnifierPrivate *priv = gtk_inspector_magnifier_get_instance_private (self);
  GdkPaintable *paintable = item;
  GdkPaintable *diff;
  GtkWidget *vbox, *hbox, *label, *picture;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (vbox, 160, 60);

  label = gtk_label_new (g_object_get_data (G_OBJECT (paintable), "description"));
  gtk_container_add (GTK_CONTAINER (vbox), label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  picture = gtk_picture_new_for_paintable (paintable);
  gtk_container_add (GTK_CONTAINER (hbox), picture);

  diff = gtk_diff_paintable_new (paintable, priv->paintable);
  picture = gtk_picture_new_for_paintable (diff);
  gtk_container_add (GTK_CONTAINER (hbox), picture);
  g_object_unref (diff);

  return vbox;
}

static void
gtk_inspector_magnifier_init (GtkInspectorMagnifier *self)
{
  self->priv = gtk_inspector_magnifier_get_instance_private (self);
  gtk_widget_init_template (GTK_WIDGET (self));

  self->priv->renderers = g_list_store_new (GDK_TYPE_PAINTABLE);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->priv->renderer_listbox),
                           G_LIST_MODEL (self->priv->renderers),
                           gtk_inspector_magnifier_create_renderer_widget,
                           self,
                           NULL);
  self->priv->paintable = gtk_widget_paintable_new (NULL);
}

void
gtk_inspector_magnifier_set_object (GtkInspectorMagnifier *self,
                                    GObject               *object)
{
  GtkInspectorMagnifierPrivate *priv = gtk_inspector_magnifier_get_instance_private (self);
  GtkWidget *stack;
  GtkStackPage *page;

  stack = gtk_widget_get_parent (GTK_WIDGET (self));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (self));

  priv->object = NULL;

  if (!GTK_IS_WIDGET (object) || !gtk_widget_is_visible (GTK_WIDGET (object)))
    {
      g_object_set (page, "visible", FALSE, NULL);
      _gtk_magnifier_set_inspected (GTK_MAGNIFIER (priv->magnifier), NULL);
      gtk_widget_paintable_set_widget (GTK_WIDGET_PAINTABLE (priv->paintable), NULL);
      return;
    }

  g_object_set (page, "visible", TRUE, NULL);

  priv->object = GTK_WIDGET (object);

  _gtk_magnifier_set_inspected (GTK_MAGNIFIER (priv->magnifier), GTK_WIDGET (object));
  gtk_widget_paintable_set_widget (GTK_WIDGET_PAINTABLE (priv->paintable), GTK_WIDGET (object));
  _gtk_magnifier_set_coords (GTK_MAGNIFIER (priv->magnifier), 0, 0);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorMagnifier *sl = GTK_INSPECTOR_MAGNIFIER (object);

  switch (param_id)
    {
    case PROP_ADJUSTMENT:
      g_value_take_object (value, sl->priv->adjustment);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorMagnifier *sl = GTK_INSPECTOR_MAGNIFIER (object);

  switch (param_id)
    {
    case PROP_ADJUSTMENT:
      sl->priv->adjustment = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_magnifier_dispose (GObject *object)
{
  GtkInspectorMagnifier *self = GTK_INSPECTOR_MAGNIFIER (object);
  GtkInspectorMagnifierPrivate *priv = gtk_inspector_magnifier_get_instance_private (self);

  g_clear_object (&priv->renderers);

  G_OBJECT_CLASS (gtk_inspector_magnifier_parent_class)->dispose (object);
}

static void
constructed (GObject *object)
{
  GtkInspectorMagnifier *sl = GTK_INSPECTOR_MAGNIFIER (object);

  g_object_bind_property (sl->priv->adjustment, "value",
                          sl->priv->magnifier, "magnification",
                          G_BINDING_SYNC_CREATE);

  G_OBJECT_CLASS (gtk_inspector_magnifier_parent_class)->constructed (object);
}

static void
gtk_inspector_magnifier_class_init (GtkInspectorMagnifierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->dispose = gtk_inspector_magnifier_dispose;
  object_class->constructed = constructed;

  g_object_class_install_property (object_class, PROP_ADJUSTMENT,
      g_param_spec_object ("adjustment", NULL, NULL,
                           GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  widget_class->realize = gtk_inspector_magnifier_realize;
  widget_class->unrealize = gtk_inspector_magnifier_unrealize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/magnifier.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMagnifier, magnifier);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMagnifier, renderer_listbox);
}

// vim: set et sw=2 ts=2:
