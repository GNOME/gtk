/*
 * Copyright © 2025 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "color-paintable.h"

struct _ColorPaintable
{
  GtkWidget parent_instance;

  GtkSymbolicColor symbolic;
  float alpha;
};

/* {{{ Utilities */

static void
snapshot_checkered_pattern (GtkSnapshot *snapshot,
                            int          width,
                            int          height)
{
  const GdkRGBA color1 = (GdkRGBA) { 0.603, 0.603, 0.603, 1 };
  const GdkRGBA color2 = (GdkRGBA) { 0.329, 0.329, 0.329, 1 };

  gtk_snapshot_push_repeat (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height), NULL);
  gtk_snapshot_append_color (snapshot, &color1, &GRAPHENE_RECT_INIT (0,  0,  10, 10));
  gtk_snapshot_append_color (snapshot, &color2, &GRAPHENE_RECT_INIT (10, 0,  10, 10));
  gtk_snapshot_append_color (snapshot, &color2, &GRAPHENE_RECT_INIT (0,  10, 10, 10));
  gtk_snapshot_append_color (snapshot, &color1, &GRAPHENE_RECT_INIT (10, 10, 10, 10));
  gtk_snapshot_pop (snapshot);
}

/* }}} */
/* {{{ GtkWidget implementation */

static void
color_paintable_snapshot_with_weight (GtkSymbolicPaintable  *paintable,
                                      GtkSnapshot           *snapshot,
                                      double                 width,
                                      double                 height,
                                      const GdkRGBA         *colors,
                                      size_t                 n_colors,
                                      double                 weight)
{
  ColorPaintable *self = COLOR_PAINTABLE (paintable);
  GdkRGBA color;

  color = colors[self->symbolic];
  color.alpha *= self->alpha;

  snapshot_checkered_pattern (snapshot, width, height);
  gtk_snapshot_append_color (snapshot,
                             &color,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static void
color_paintable_snapshot_symbolic (GtkSymbolicPaintable  *paintable,
                                   GtkSnapshot           *snapshot,
                                   double                 width,
                                   double                 height,
                                   const GdkRGBA         *colors,
                                   size_t                 n_colors)
{
  color_paintable_snapshot_with_weight (paintable,
                                        snapshot,
                                        width, height,
                                        colors, n_colors,
                                        400);
}

static void
color_paintable_init_symbolic_paintable_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = color_paintable_snapshot_symbolic;
  iface->snapshot_with_weight = color_paintable_snapshot_with_weight;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
color_paintable_snapshot (GdkPaintable  *paintable,
                           GtkSnapshot   *snapshot,
                           double         width,
                           double         height)
{
  color_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                      snapshot,
                                      width, height,
                                      NULL, 0);
}

static int
color_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return 0;
}

static int
color_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return 0;
}

static void
color_paintable_init_paintable_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = color_paintable_snapshot;
  iface->get_intrinsic_width = color_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = color_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GObject boilerplate */

struct _ColorPaintableClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_SYMBOLIC = 1,
  PROP_ALPHA,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE_WITH_CODE (ColorPaintable, color_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                color_paintable_init_paintable_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                color_paintable_init_symbolic_paintable_interface))

static void
color_paintable_init (ColorPaintable *self)
{
}

static void
color_paintable_get_property (GObject      *object,
                              unsigned int  property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  ColorPaintable *self = COLOR_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_SYMBOLIC:
      g_value_set_enum (value, self->symbolic);
      break;

    case PROP_ALPHA:
      g_value_set_float (value, self->alpha);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
color_paintable_set_property (GObject      *object,
                              unsigned int  property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ColorPaintable *self = COLOR_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_SYMBOLIC:
      color_paintable_set_symbolic (self, g_value_get_enum (value));
      break;

    case PROP_ALPHA:
      color_paintable_set_alpha (self, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
color_paintable_class_init (ColorPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = color_paintable_get_property;
  object_class->set_property = color_paintable_set_property;

  properties[PROP_SYMBOLIC] =
    g_param_spec_enum ("symbolic", NULL, NULL,
                       GTK_TYPE_SYMBOLIC_COLOR, GTK_SYMBOLIC_COLOR_FOREGROUND,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ALPHA] =
    g_param_spec_float ("alpha", NULL, NULL,
                        0, 1, 1,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Public API */

ColorPaintable *
color_paintable_new (void)
{
  return g_object_new (color_paintable_get_type (), NULL);
}

void
color_paintable_set_symbolic (ColorPaintable   *self,
                              GtkSymbolicColor  symbolic)
{
  g_return_if_fail (COLOR_IS_PAINTABLE (self));

  if (self->symbolic == symbolic)
    return;

  self->symbolic = symbolic;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYMBOLIC]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

GtkSymbolicColor
color_paintable_get_symbolic (ColorPaintable *self)
{
  g_return_val_if_fail (COLOR_IS_PAINTABLE (self), GTK_SYMBOLIC_COLOR_FOREGROUND);

  return self->symbolic;
}

void
color_paintable_set_alpha (ColorPaintable *self,
                           float           alpha)
{
  g_return_if_fail (COLOR_IS_PAINTABLE (self));

  if (self->alpha == alpha)
    return;

  self->alpha = alpha;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALPHA]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

float
color_paintable_get_alpha (ColorPaintable *self)
{
  g_return_val_if_fail (COLOR_IS_PAINTABLE (self), 1.f);

  return self->alpha;
}

/* }}} */

/* vim:set foldmethod=marker: */
