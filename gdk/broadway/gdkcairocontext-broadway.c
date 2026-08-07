/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkconfig.h"

#include "gdkcairocontext-broadway.h"
#include "gdktextureprivate.h"
#include "gdkprivate-broadway.h"

G_DEFINE_TYPE (GdkBroadwayCairoContext, gdk_broadway_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_broadway_cairo_context_dispose (GObject *object)
{
  G_OBJECT_CLASS (gdk_broadway_cairo_context_parent_class)->dispose (object);
}

static void
gdk_broadway_cairo_context_begin_frame (GdkDrawContext      *draw_context,
                                        GdkDrawContextFrame *frame,
                                        gpointer             context_data,
                                        GdkColorState      **out_color_state,
                                        GdkMemoryDepth      *out_depth)
{
  cairo_t *cr;
  guint width, height;
  cairo_region_t *region;
  cairo_surface_t *cairo_surface;

  gdk_draw_context_get_buffer_size (draw_context, &width, &height);
  cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                              0, 0,
                                              width, height
                                          });
  gdk_draw_context_frame_add_damage (frame, region);
  cairo_region_destroy (region);

  /* clear the repaint area */
  cr = cairo_create (cairo_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_fill (cr);
  cairo_destroy (cr);

  gdk_cairo_context_frame_set_surface ((GdkCairoContextFrame *) frame, cairo_surface);

  *out_color_state = GDK_COLOR_STATE_SRGB;
  *out_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);
}

static void
add_uint32 (GArray *nodes, guint32 v)
{
  g_array_append_val (nodes, v);
}

static void
add_float (GArray *nodes, float f)
{
  gint32 i = (gint32) (f * 256.0f);
  guint u = (guint32) i;
  g_array_append_val (nodes, u);
}

static void
gdk_broadway_cairo_context_end_frame (GdkDrawContext      *draw_context,
                                      GdkDrawContextFrame *frame,
                                      gpointer             context_data)
{
  GdkCairoContextFrame *cframe = (GdkCairoContextFrame *) frame;
  GdkDisplay *display = gdk_draw_context_get_display (draw_context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  cairo_surface_t *cairo_surface = gdk_cairo_context_frame_get_surface (cframe);
  GdkTexture *texture;
  GPtrArray *node_textures;
  GArray *nodes;
  guint32 texture_id;

  nodes = g_array_new (FALSE, FALSE, sizeof(guint32));
  node_textures = g_ptr_array_new_with_free_func (g_object_unref);

  texture = gdk_texture_new_for_surface (cairo_surface);
  g_ptr_array_add (node_textures, g_object_ref (texture)); /* Transfers ownership to node_textures */
  texture_id = gdk_broadway_display_ensure_texture (display, texture);

  add_uint32 (nodes, BROADWAY_NODE_TEXTURE);
  add_float (nodes, 0);
  add_float (nodes, 0);
  add_float (nodes, cairo_image_surface_get_width (cairo_surface));
  add_float (nodes, cairo_image_surface_get_height (cairo_surface));
  add_uint32 (nodes, texture_id);

  gdk_broadway_surface_set_nodes (surface, nodes, node_textures);
  g_array_unref (nodes);
  g_ptr_array_unref (node_textures);

  gdk_cairo_context_frame_set_surface (cframe, NULL);
}

static void
gdk_broadway_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
}

static void
gdk_broadway_cairo_context_class_init (GdkBroadwayCairoContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_broadway_cairo_context_dispose;

  draw_context_class->begin_frame = gdk_broadway_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_broadway_cairo_context_end_frame;
  draw_context_class->surface_resized = gdk_broadway_cairo_context_surface_resized;
}

static void
gdk_broadway_cairo_context_init (GdkBroadwayCairoContext *self)
{
}
