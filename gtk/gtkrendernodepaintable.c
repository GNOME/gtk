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

#include "config.h"

#include "gtkrendernodepaintableprivate.h"

#include "gtksnapshot.h"

struct _GtkRenderNodePaintable
{
  GObject parent_instance;

  GskRenderNode *node;
};

struct _GtkRenderNodePaintableClass
{
  GObjectClass parent_class;
};

static void
gtk_render_node_paintable_paintable_snapshot (GdkPaintable *paintable,
                                              GdkSnapshot  *snapshot,
                                              double        width,
                                              double        height)
{
  GtkRenderNodePaintable *self = GTK_RENDER_NODE_PAINTABLE (paintable);
  graphene_rect_t node_bounds;

  gsk_render_node_get_bounds (self->node, &node_bounds);

  if (node_bounds.origin.x + node_bounds.size.width != width ||
      node_bounds.origin.y + node_bounds.size.height != height)
    {
      graphene_matrix_t transform;

      graphene_matrix_init_scale (&transform,
                                  width / (node_bounds.origin.x + node_bounds.size.width),
                                  height / (node_bounds.origin.y + node_bounds.size.height),
                                  1.0);
      gtk_snapshot_push_transform (snapshot,
                                   &transform,
                                   "RenderNodeScaleToFit");
      gtk_snapshot_append_node (snapshot, self->node);
      gtk_snapshot_pop (snapshot);
    }
  else
    {
      gtk_snapshot_append_node (snapshot, self->node);
    }
}

static GdkPaintableFlags
gtk_render_node_paintable_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_CONTENTS | GDK_PAINTABLE_STATIC_SIZE;
}

static int
gtk_render_node_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkRenderNodePaintable *self = GTK_RENDER_NODE_PAINTABLE (paintable);
  graphene_rect_t node_bounds;

  gsk_render_node_get_bounds (self->node, &node_bounds);

  return node_bounds.origin.x + node_bounds.size.width;
}

static int
gtk_render_node_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkRenderNodePaintable *self = GTK_RENDER_NODE_PAINTABLE (paintable);
  graphene_rect_t node_bounds;

  gsk_render_node_get_bounds (self->node, &node_bounds);

  return node_bounds.origin.y + node_bounds.size.height;
}

static void
gtk_render_node_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_render_node_paintable_paintable_snapshot;
  iface->get_flags = gtk_render_node_paintable_paintable_get_flags;
  iface->get_intrinsic_width = gtk_render_node_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_render_node_paintable_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (GtkRenderNodePaintable, gtk_render_node_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_render_node_paintable_paintable_init))

static void
gtk_render_node_paintable_dispose (GObject *object)
{
  GtkRenderNodePaintable *self = GTK_RENDER_NODE_PAINTABLE (object);

  g_clear_pointer (&self->node, gsk_render_node_unref);

  G_OBJECT_CLASS (gtk_render_node_paintable_parent_class)->dispose (object);
}

static void
gtk_render_node_paintable_class_init (GtkRenderNodePaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_render_node_paintable_dispose;
}

static void
gtk_render_node_paintable_init (GtkRenderNodePaintable *self)
{
}

GdkPaintable *
gtk_render_node_paintable_new (GskRenderNode *node)
{
  GtkRenderNodePaintable *self;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  self = g_object_new (GTK_TYPE_RENDER_NODE_PAINTABLE, NULL);

  self->node = gsk_render_node_ref (node);

  return GDK_PAINTABLE (self);
}
