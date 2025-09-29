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

#include "border-paintable.h"
#include "path-paintable.h"

struct _BorderPaintable
{
  GObject parent_instance;

  gboolean show_bounds;
  gboolean show_spines;

  PathPaintable *paintable;
};

/* {{{ Utilities */

static GskPath *
circle_path_new (float cx,
                 float cy,
                 float radius)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), radius);
  return gsk_path_builder_free_to_path (builder);
}

static void
get_origin_location (GskPath          *path,
                     float             origin,
                     graphene_point_t *pos)
{
  g_autoptr (GskPathMeasure) measure = NULL;
  float length;
  GskPathPoint point;

  measure = gsk_path_measure_new (path);
  length = gsk_path_measure_get_length (measure);
  gsk_path_measure_get_point (measure, origin * length, &point);
  gsk_path_point_get_position (&point, path, pos);
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static void
border_paintable_snapshot_with_weight (GtkSymbolicPaintable  *paintable,
                                       GtkSnapshot           *snapshot,
                                       double                 width,
                                       double                 height,
                                       const GdkRGBA         *colors,
                                       size_t                 n_colors,
                                       double                 weight)
{
  BorderPaintable *self = BORDER_PAINTABLE (paintable);
  double w, h;
  float scale;

  if (!self->paintable)
    return;

  w = path_paintable_get_width (self->paintable);
  h = path_paintable_get_height (self->paintable);

  if (w == 0 || h == 0)
    return;

  scale = MIN (width / w, height / h);

  if (self->show_bounds)
    {
      float border_width[4];
      GdkRGBA border_color[4];

      for (int i = 0; i < 4; i++)
        {
          border_width[i] = 1;
          border_color[i] = colors[GTK_SYMBOLIC_COLOR_FOREGROUND];
        }

      gtk_snapshot_append_border (snapshot,
                                  &GSK_ROUNDED_RECT_INIT (0, 0, scale * w, scale * h),
                                  border_width, border_color);
    }

  gtk_symbolic_paintable_snapshot_with_weight (GTK_SYMBOLIC_PAINTABLE (self->paintable),
                                               snapshot,
                                               width, height,
                                               colors, n_colors,
                                               weight);

  if (self->show_spines)
    {
      graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, width, height);
      GdkRGBA c = (GdkRGBA) { 1, 0, 0, 1 };
      g_autoptr (GskStroke) stroke = NULL;
      unsigned int state;

      stroke = gsk_stroke_new (1.f/scale);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_scale (snapshot, scale, scale);

      state = path_paintable_get_state (self->paintable);

      if (state != STATE_UNSET)
        {
          for (unsigned int i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
            {
              uint64_t states = path_paintable_get_path_states (self->paintable, i);

              if (states & (G_GUINT64_CONSTANT (1) << state))
                {
                  GskPath *path = path_paintable_get_path (self->paintable, i);
                  float origin = path_paintable_get_path_origin (self->paintable, i);
                  size_t attach_to;
                  float attach_pos;

                  graphene_point_t pos;
                  g_autoptr (GskPath) dot = NULL;

                  gtk_snapshot_push_stroke (snapshot, path, stroke);
                  gtk_snapshot_append_color (snapshot, &c, &bounds);
                  gtk_snapshot_pop (snapshot);

                  get_origin_location (path, origin, &pos);

                  dot = circle_path_new (pos.x, pos.y, 4.f/scale);
                  gtk_snapshot_push_fill (snapshot, dot, GSK_FILL_RULE_WINDING);
                  gtk_snapshot_append_color (snapshot, &c, &bounds);
                  gtk_snapshot_pop (snapshot);

                  path_paintable_get_attach_path (self->paintable, i, &attach_to, &attach_pos);

                  if (attach_to != (size_t) -1)
                    {
                      GskPathBuilder *builder;
                      GskPath *arrow;

                      builder = gsk_path_builder_new ();
                      gsk_path_builder_move_to (builder, pos.x, pos.y);
                      gsk_path_builder_rel_line_to (builder, 20.f/scale, 0);
                      gsk_path_builder_rel_move_to (builder, -4.f/scale, -3.f/scale);
                      gsk_path_builder_rel_line_to (builder, 4.f/scale, 3.f/scale);
                      gsk_path_builder_rel_line_to (builder, -4.f/scale, 3.f/scale);
                      arrow = gsk_path_builder_free_to_path (builder);
                      gtk_snapshot_push_stroke (snapshot, arrow, stroke);
                      gtk_snapshot_append_color (snapshot, &c, &bounds);
                      gtk_snapshot_pop (snapshot);

                      gsk_path_unref (arrow);
                    }
                }
            }
        }

      gtk_snapshot_restore (snapshot);
    }
}

static void
border_paintable_snapshot_symbolic (GtkSymbolicPaintable  *paintable,
                                    GtkSnapshot           *snapshot,
                                    double                 width,
                                    double                 height,
                                    const GdkRGBA         *colors,
                                    size_t                 n_colors)
{
  border_paintable_snapshot_with_weight (paintable,
                                         snapshot,
                                         width, height,
                                         colors, n_colors,
                                         400);
}

static void
border_paintable_init_symbolic_paintable_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = border_paintable_snapshot_symbolic;
  iface->snapshot_with_weight = border_paintable_snapshot_with_weight;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
border_paintable_snapshot (GdkPaintable  *paintable,
                           GtkSnapshot   *snapshot,
                           double         width,
                           double         height)
{
  border_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                      snapshot,
                                      width, height,
                                      NULL, 0);
}

static int
border_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  BorderPaintable *self = BORDER_PAINTABLE (paintable);

  if (self->paintable)
    return gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (self->paintable));

  return 0;
}

static int
border_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  BorderPaintable *self = BORDER_PAINTABLE (paintable);

  if (self->paintable)
    return gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (self->paintable));

  return 0;
}

static void
border_paintable_init_paintable_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = border_paintable_snapshot;
  iface->get_intrinsic_width = border_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = border_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GObject boilerplate */

struct _BorderPaintableClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_PAINTABLE = 1,
  PROP_SHOW_BOUNDS,
  PROP_SHOW_SPINES,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE_WITH_CODE (BorderPaintable, border_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                border_paintable_init_paintable_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                border_paintable_init_symbolic_paintable_interface))

static void
border_paintable_init (BorderPaintable *self)
{
}

static void
border_paintable_dispose (GObject *object)
{
  BorderPaintable *self = BORDER_PAINTABLE (object);

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);
    }

  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (border_paintable_parent_class)->dispose (object);
}

static void
border_paintable_get_property (GObject      *object,
                               unsigned int  property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  BorderPaintable *self = BORDER_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_SHOW_BOUNDS:
      g_value_set_boolean (value, self->show_bounds);
      break;

    case PROP_SHOW_SPINES:
      g_value_set_boolean (value, self->show_spines);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
border_paintable_set_property (GObject      *object,
                               unsigned int  property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BorderPaintable *self = BORDER_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_PAINTABLE:
      border_paintable_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_SHOW_BOUNDS:
      border_paintable_set_show_bounds (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SPINES:
      border_paintable_set_show_spines (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
border_paintable_class_init (BorderPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = border_paintable_dispose;
  object_class->get_property = border_paintable_get_property;
  object_class->set_property = border_paintable_set_property;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         PATH_PAINTABLE_TYPE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_BOUNDS] =
    g_param_spec_boolean ("show-bounds", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_SPINES] =
    g_param_spec_boolean ("show-spines", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Public API */

BorderPaintable *
border_paintable_new (void)
{
  return g_object_new (border_paintable_get_type (), NULL);
}

void
border_paintable_set_paintable (BorderPaintable *self,
                                PathPaintable   *paintable)
{
  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);
    }

  if (!g_set_object (&self->paintable, paintable))
    return;

  if (self->paintable)
    {
      g_signal_connect_swapped (self->paintable, "invalidate-contents",
                                G_CALLBACK (gdk_paintable_invalidate_contents), self);
      g_signal_connect_swapped (self->paintable, "invalidate-size",
                                G_CALLBACK (gdk_paintable_invalidate_size), self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

PathPaintable *
border_paintable_get_paintable (BorderPaintable *self)
{
  return self->paintable;
}

void
border_paintable_set_show_bounds (BorderPaintable *self,
                                  gboolean         show_bounds)
{
  if (self->show_bounds == show_bounds)
    return;

  self->show_bounds = show_bounds;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_BOUNDS]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

gboolean
border_paintable_get_show_bounds (BorderPaintable *self)
{
  return self->show_bounds;
}

void
border_paintable_set_show_spines (BorderPaintable *self,
                                  gboolean         show_spines)
{
  if (self->show_spines == show_spines)
    return;

  self->show_spines = show_spines;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SPINES]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

gboolean
border_paintable_get_show_spines (BorderPaintable *self)
{
  return self->show_spines;
}

/* }}} */

/* vim:set foldmethod=marker: */
