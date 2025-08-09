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
#include "path-paintable-private.h"

struct _BorderPaintable
{
  GObject parent_instance;

  gboolean show_bounds;
  PathPaintable *paintable;
};

/* {{{ GtkSymbolicPaintable implementation */

static void
border_paintable_snapshot_with_weight (GtkSymbolicPaintable  *paintable,
                                       GtkSnapshot           *snapshot,
                                       double                 width,
                                       double                 height,
                                       const GdkRGBA         *colors,
                                       gsize                  n_colors,
                                       double                 weight)
{
  BorderPaintable *self = BORDER_PAINTABLE (paintable);
  double w, h;

  if (!self->paintable)
    return;

  w = path_paintable_get_width (self->paintable);
  h = path_paintable_get_height (self->paintable);

  if (w == 0 || h == 0)
    return;

  if (self->show_bounds)
    {
      float scale;
      float border_width[4];
      GdkRGBA border_color[4];

      scale = MIN (width / w, height / h);

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
}

static void
border_paintable_snapshot_symbolic (GtkSymbolicPaintable  *paintable,
                                    GtkSnapshot           *snapshot,
                                    double                 width,
                                    double                 height,
                                    const GdkRGBA         *colors,
                                    gsize                  n_colors)
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
border_paintable_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
border_paintable_set_property (GObject      *object,
                               guint         property_id,
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

/* }}} */

/* vim:set foldmethod=marker: */
