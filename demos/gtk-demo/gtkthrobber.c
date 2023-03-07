/*
 * Copyright © 2018 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "gtkthrobber.h"

#include "gtk.h"

struct _GtkThrobber
{
  GObject parent_instance;

  GdkFrameClock *clock;
  GdkTexture *texture;
  guint clock_tick_id;

  float min, max, scale, delta;
  float delta2, hot;

  GdkDrag *drag;
};

struct _GtkThrobberClass
{
  GObjectClass parent_class;
};

static void
gtk_throbber_paintable_snapshot (GdkPaintable *paintable,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height)
{
  GtkThrobber *self = GTK_THROBBER (paintable);

  gtk_snapshot_append_texture (snapshot, self->texture, &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static int
gtk_throbber_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkThrobber *self = GTK_THROBBER (paintable);

  return gdk_texture_get_width (self->texture) * self->scale;
}

static int
gtk_throbber_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkThrobber *self = GTK_THROBBER (paintable);

  return gdk_texture_get_height (self->texture) * self->scale;
}

static void
gtk_throbber_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_throbber_paintable_snapshot;
  iface->get_intrinsic_width = gtk_throbber_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_throbber_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (GtkThrobber, gtk_throbber, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_throbber_paintable_init))

static void
gtk_throbber_dispose (GObject *object)
{
  GtkThrobber *self = GTK_THROBBER (object);

  gdk_frame_clock_end_updating (self->clock);
  g_signal_handler_disconnect (self->clock, self->clock_tick_id);
  self->clock_tick_id = 0;

  g_clear_object (&self->clock);
  g_clear_object (&self->texture);
  g_clear_object (&self->drag);

  G_OBJECT_CLASS (gtk_throbber_parent_class)->dispose (object);
}

static void
gtk_throbber_class_init (GtkThrobberClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_throbber_dispose;
}

static void
gtk_throbber_init (GtkThrobber *self)
{
}

static void
on_frame_clock_update (GdkFrameClock *clock,
                       GtkThrobber   *self)
{
  self->scale += self->delta;
  if (self->scale >= self->max)
    {
      self->scale = self->max;
      self->delta = - self->delta;
    }
  else if (self->scale <= self->min)
    {
      self->scale = self->min;
      self->delta = - self->delta;
    }

  self->hot += self->delta2;
  if (self->hot >= 1.)
    {
      self->hot = 1.;
      self->delta2 = - self->delta2;
    }
  else if (self->hot <= 0.)
    {
      self->hot = 0.;
      self->delta2 = - self->delta2;
    }

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  gdk_drag_set_hotspot (self->drag,
                        gdk_texture_get_width (self->texture) * self->scale * self->hot,
                        gdk_texture_get_height (self->texture) * self->scale * self->hot);
}


GdkPaintable *
gtk_throbber_new (GdkFrameClock *clock,
                  const char    *resource_path,
                  float          min,
                  float          max,
                  GdkDrag       *drag)
{
  GtkThrobber *self;

  self = g_object_new (GTK_TYPE_THROBBER, NULL);

  self->clock = g_object_ref (clock);
  self->texture = gdk_texture_new_from_resource (resource_path);
  self->drag = g_object_ref (drag);
  self->min = min;
  self->max = max;
  self->scale = min;
  self->delta = (max - min) / (15 * 16);
  self->delta2 = 1. / (10 * 16);
  self->hot = 0.;

  self->clock_tick_id = g_signal_connect (self->clock, "update",
                                          G_CALLBACK (on_frame_clock_update), self);

  gdk_frame_clock_begin_updating (self->clock);

  return GDK_PAINTABLE (self);
}
