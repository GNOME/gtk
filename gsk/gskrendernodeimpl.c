/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#include "gskrendernodeprivate.h"

#include "gskcairoblurprivate.h"
#include "gskcairoshadowprivate.h"
#include "gskcairogradientprivate.h"
#include "gskcairorenderer.h"
#include "gskcolormatrixnodeprivate.h"
#include "gskcolornode.h"
#include "gskcontainernode.h"
#include "gskdebugprivate.h"
#include "gskfillnode.h"
#include "gskrectprivate.h"
#include "gskrendererprivate.h"
#include "gskrenderreplay.h"
#include "gskrepeatnode.h"
#include "gskroundedrectprivate.h"
#include "gskstrokenode.h"
#include "gsksubsurfacenode.h"
#include "gsktransformprivate.h"
#include "gskcomponenttransferprivate.h"
#include "gskprivate.h"
#include "gpu/gskglrenderer.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkprivate.h"
#include "gdk/gdkrectangleprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include <cairo.h>
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#include <hb-ot.h>

/* This lock protects all on-demand created legacy rgba data of
 * render nodes.
 */
G_LOCK_DEFINE_STATIC (rgba);

/* {{{ Utilities */

static GskRenderNode *
gsk_render_node_replay_as_self (GskRenderNode   *node,
                                GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

/* }}} */
/* {{{ GSK_CONIC_GRADIENT_NODE */

/**
 * GskConicGradientNode:
 *
 * A render node for a conic gradient.
 */

struct _GskConicGradientNode
{
  GskRenderNode render_node;
  GskGradient gradient;

  graphene_point_t center;
  float rotation;
  float angle;
};

static void
gsk_conic_gradient_node_finalize (GskRenderNode *node)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CONIC_GRADIENT_NODE));

  gsk_gradient_clear (&self->gradient);

  parent_class->finalize (node);
}

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))

static void
_cairo_mesh_pattern_set_corner_rgba (cairo_pattern_t *pattern,
                                     guint            corner_num,
                                     const float      color[4])
{
  cairo_mesh_pattern_set_corner_color_rgba (pattern, corner_num, color[0], color[1], color[2], color[3]);
}

static void
project (double  angle,
         double  radius,
         double *x_out,
         double *y_out)
{
  double x, y;

#ifdef HAVE_SINCOS
  sincos (angle, &y, &x);
#else
  x = cos (angle);
  y = sin (angle);
#endif
  *x_out = radius * x;
  *y_out = radius * y;
}

static void
gsk_conic_gradient_node_add_patch (cairo_pattern_t *pattern,
                                   float            radius,
                                   float            start_angle,
                                   const float      start_color[4],
                                   float            end_angle,
                                   const float      end_color[4])
{
  double x, y;

  cairo_mesh_pattern_begin_patch (pattern);

  cairo_mesh_pattern_move_to  (pattern, 0, 0);
  project (start_angle, radius, &x, &y);
  cairo_mesh_pattern_line_to  (pattern, x, y);
  project (end_angle, radius, &x, &y);
  cairo_mesh_pattern_line_to  (pattern, x, y);
  cairo_mesh_pattern_line_to  (pattern, 0, 0);

  _cairo_mesh_pattern_set_corner_rgba (pattern, 0, start_color);
  _cairo_mesh_pattern_set_corner_rgba (pattern, 1, start_color);
  _cairo_mesh_pattern_set_corner_rgba (pattern, 2, end_color);
  _cairo_mesh_pattern_set_corner_rgba (pattern, 3, end_color);

  cairo_mesh_pattern_end_patch (pattern);
}

static void
gdk_rgba_color_interpolate (GdkRGBA       *dest,
                            const GdkRGBA *src1,
                            const GdkRGBA *src2,
                            double         progress)
{
  double alpha = src1->alpha * (1.0 - progress) + src2->alpha * progress;

  dest->alpha = alpha;
  if (alpha == 0)
    {
      dest->red = src1->red * (1.0 - progress) + src2->red * progress;
      dest->green = src1->green * (1.0 - progress) + src2->green * progress;
      dest->blue = src1->blue * (1.0 - progress) + src2->blue * progress;
    }
  else
    {
      dest->red = (src1->red * src1->alpha * (1.0 - progress) + src2->red * src2->alpha * progress) / alpha;
      dest->green = (src1->green * src1->alpha * (1.0 - progress) + src2->green * src2->alpha * progress) / alpha;
      dest->blue = (src1->blue * src1->alpha * (1.0 - progress) + src2->blue * src2->alpha * progress) / alpha;
    }
}

static void
add_color_stop_to_array (float    offset,
                         GdkColorState *ccs,
                         float    values[4],
                         gpointer data)
{
  GArray *stops = data;
  GskGradientStop stop;

  stop.offset = offset;
  gdk_color_init (&stop.color, ccs, values);

  g_array_append_val (stops, stop);
}

static void
gsk_conic_gradient_node_draw (GskRenderNode *node,
                              cairo_t       *cr,
                              GskCairoData  *data)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  cairo_pattern_t *pattern;
  graphene_point_t corner;
  float radius;
  gsize i;
  GArray *stops;
  GskGradient *gradient = &self->gradient;

  pattern = cairo_pattern_create_mesh ();
  graphene_rect_get_top_right (&node->bounds, &corner);
  radius = graphene_point_distance (&self->center, &corner, NULL, NULL);
  graphene_rect_get_bottom_right (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));
  graphene_rect_get_bottom_left (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));
  graphene_rect_get_top_left (&node->bounds, &corner);
  radius = MAX (radius, graphene_point_distance (&self->center, &corner, NULL, NULL));

  gsize n_stops = gsk_gradient_get_n_stops (gradient);
  const GskGradientStop *orig_stops = gsk_gradient_get_stops (gradient);

  stops = g_array_new (FALSE, TRUE, sizeof (GskGradientStop));
  g_array_set_clear_func (stops, (GDestroyNotify) clear_stop);

  if (gdk_color_state_equal (gsk_gradient_get_interpolation (gradient), data->ccs))
    {
      for (i = 0; i < n_stops; i++)
        {
          g_array_append_val (stops, orig_stops[i]);
          /* take a ref, since clear_stop removes one */
          gdk_color_state_ref (orig_stops[i].color.color_state);
        }
    }
  else
    {
      g_array_append_val (stops, orig_stops[0]);

      for (i = 1; i < n_stops; i++)
        {
          gsk_cairo_interpolate_color_stops (data->ccs,
                                             gsk_gradient_get_interpolation (gradient),
                                             gsk_gradient_get_hue_interpolation (gradient),
                                             orig_stops[i-1].offset, &orig_stops[i-1].color,
                                             orig_stops[i].offset, &orig_stops[i].color,
                                             orig_stops[i].transition_hint,
                                             add_color_stop_to_array,
                                             stops);
          g_array_append_val (stops, orig_stops[i]);
          /* take a ref, since clear_stop removes one */
          gdk_color_state_ref (orig_stops[i].color.color_state);
        }
    }

  for (i = 0; i <= stops->len; i++)
    {
      GskGradientStop *stop1 = &g_array_index (stops, GskGradientStop, MAX (i, 1) - 1);
      GskGradientStop *stop2 = &g_array_index (stops, GskGradientStop, MIN (i, stops->len - 1));
      double offset1 = i > 0 ? stop1->offset : 0;
      double offset2 = i < n_stops ? stop2->offset : 1;
      double transition_hint = i > 0 && i < n_stops ? stop2->transition_hint : 0.5;
      double start_angle, end_angle;
      float color1[4];
      float color2[4];
      double exp;

      offset1 = offset1 * 360 + self->rotation - 90;
      offset2 = offset2 * 360 + self->rotation - 90;

      gdk_color_to_float (&stop1->color, data->ccs, color1);
      gdk_color_to_float (&stop2->color, data->ccs, color2);

      if (transition_hint <= 0)
        exp = 0;
      else if (transition_hint >= 1)
        exp = INFINITY;
      else if (transition_hint == 0.5)
        exp = 1;
      else
        exp = - M_LN2 / logf (transition_hint);

      for (start_angle = offset1; start_angle < offset2; start_angle = end_angle)
        {
          float f, C;
          float start_color[4], end_color[4];

          end_angle = (floor (start_angle / 45) + 1) * 45;
          end_angle = MIN (end_angle, offset2);

          f = (start_angle - offset1) / (offset2 - offset1);
          if (transition_hint <= 0)
            C = 1;
          else if (transition_hint >= 1)
            C = 0;
          else if (transition_hint == 0.5)
            C = f;
          else
            C = powf (f, exp);

          gdk_rgba_color_interpolate ((GdkRGBA *) &start_color,
                                      (const GdkRGBA *) &color1,
                                      (const GdkRGBA *) &color2,
                                      C);

          f = (end_angle - offset1) / (offset2 - offset1);
          if (transition_hint <= 0)
            C = 1;
          else if (transition_hint >= 1)
            C = 0;
          else if (transition_hint == 0.5)
            C = f;
          else
            C = powf (f, exp);

          gdk_rgba_color_interpolate ((GdkRGBA *) &end_color,
                                      (const GdkRGBA *) &color1,
                                      (const GdkRGBA *) &color2,
                                      C);

          gsk_conic_gradient_node_add_patch (pattern,
                                             radius,
                                             DEG_TO_RAD (start_angle),
                                             start_color,
                                             DEG_TO_RAD (end_angle),
                                             end_color);
        }
    }

  g_array_unref (stops);

  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_translate (cr, self->center.x, self->center.y);
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);
}

static void
gsk_conic_gradient_node_diff (GskRenderNode *node1,
                              GskRenderNode *node2,
                              GskDiffData   *data)
{
  GskConicGradientNode *self1 = (GskConicGradientNode *) node1;
  GskConicGradientNode *self2 = (GskConicGradientNode *) node2;

  if (!gsk_rect_equal (&node1->bounds, &node2->bounds) ||
      !graphene_point_equal (&self1->center, &self2->center) ||
      self1->rotation != self2->rotation ||
      !gsk_gradient_equal (&self1->gradient, &self2->gradient))
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static void
gsk_conic_gradient_node_class_init (gpointer g_class,
                                    gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_CONIC_GRADIENT_NODE;

  node_class->finalize = gsk_conic_gradient_node_finalize;
  node_class->draw = gsk_conic_gradient_node_draw;
  node_class->diff = gsk_conic_gradient_node_diff;
  node_class->replay = gsk_render_node_replay_as_self;
}

/**
 * gsk_conic_gradient_node_new:
 * @bounds: the bounds of the node
 * @center: the center of the gradient
 * @rotation: the rotation of the gradient in degrees
 * @color_stops: (array length=n_color_stops): a pointer to an array of
 *   `GskColorStop` defining the gradient. The offsets of all color stops
 *   must be increasing. The first stop's offset must be >= 0 and the last
 *   stop's offset must be <= 1.
 * @n_color_stops: the number of elements in @color_stops
 *
 * Creates a `GskRenderNode` that draws a conic gradient.
 *
 * The conic gradient
 * starts around @center in the direction of @rotation. A rotation of 0 means
 * that the gradient points up. Color stops are then added clockwise.
 *
 * Returns: (transfer full) (type GskConicGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_conic_gradient_node_new (const graphene_rect_t  *bounds,
                             const graphene_point_t *center,
                             float                   rotation,
                             const GskColorStop     *color_stops,
                             gsize                   n_color_stops)
{
  GskGradient *gradient;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);
  g_return_val_if_fail (color_stops != NULL, NULL);
  g_return_val_if_fail (n_color_stops >= 2, NULL);
  g_return_val_if_fail (color_stops[0].offset >= 0, NULL);

  gradient = gsk_gradient_new ();
  gsk_gradient_add_color_stops (gradient, color_stops, n_color_stops);

  node = gsk_conic_gradient_node_new2 (bounds,
                                       center, rotation,
                                       gradient);

  gsk_gradient_free (gradient);

  return node;
}

/*< private >
 * gsk_conic_gradient_node_new2:
 * @bounds: the bounds of the node
 * @center: the center of the gradient
 * @rotation: the rotation of the gradient in degrees
 * @gradient: the gradient specification
 *
 * Creates a `GskRenderNode` that draws a conic gradient.
 *
 * The conic gradient
 * starts around @center in the direction of @rotation. A rotation of 0 means
 * that the gradient points up. Color stops are then added clockwise.
 *
 * Returns: (transfer full) (type GskConicGradientNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_conic_gradient_node_new2 (const graphene_rect_t   *bounds,
                              const graphene_point_t  *center,
                              float                    rotation,
                              const GskGradient       *gradient)
{
  GskConicGradientNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (center != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_CONIC_GRADIENT_NODE);
  node = (GskRenderNode *) self;

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);
  graphene_point_init_from_point (&self->center, center);

  self->rotation = rotation;

  gsk_gradient_init_copy (&self->gradient, gradient);

  node->fully_opaque = gsk_gradient_is_opaque (gradient);
  node->preferred_depth = gdk_color_state_get_depth (gsk_gradient_get_interpolation (gradient));
  node->is_hdr = gdk_color_state_is_hdr (gsk_gradient_get_interpolation (gradient));

  self->angle = 90.f - self->rotation;
  self->angle = G_PI * self->angle / 180.f;
  self->angle = fmodf (self->angle, 2.f * G_PI);
  if (self->angle < 0.f)
    self->angle += 2.f * G_PI;

  return node;
}

/**
 * gsk_conic_gradient_node_get_n_color_stops:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the number of color stops in the gradient.
 *
 * Returns: the number of color stops
 */
gsize
gsk_conic_gradient_node_get_n_color_stops (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return gsk_gradient_get_n_stops (&self->gradient);
}

/**
 * gsk_conic_gradient_node_get_color_stops:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 * @n_stops: (out) (optional): the number of color stops in the returned array
 *
 * Retrieves the color stops in the gradient.
 *
 * Returns: (array length=n_stops): the color stops in the gradient
 */
const GskColorStop *
gsk_conic_gradient_node_get_color_stops (const GskRenderNode *node,
                                         gsize               *n_stops)
{
  GskConicGradientNode *self = (GskConicGradientNode *) node;
  const GskColorStop *stops;

  G_LOCK (rgba);

  stops = gsk_gradient_get_color_stops (&self->gradient, n_stops);

  G_UNLOCK (rgba);

  return stops;
}

/**
 * gsk_conic_gradient_node_get_center:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the center pointer for the gradient.
 *
 * Returns: the center point for the gradient
 */
const graphene_point_t *
gsk_conic_gradient_node_get_center (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return &self->center;
}

/**
 * gsk_conic_gradient_node_get_rotation:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the rotation for the gradient in degrees.
 *
 * Returns: the rotation for the gradient
 */
float
gsk_conic_gradient_node_get_rotation (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return self->rotation;
}

/**
 * gsk_conic_gradient_node_get_angle:
 * @node: (type GskConicGradientNode): a `GskRenderNode` for a conic gradient
 *
 * Retrieves the angle for the gradient in radians, normalized in [0, 2 * PI].
 *
 * The angle is starting at the top and going clockwise, as expressed
 * in the css specification:
 *
 *     angle = 90 - gsk_conic_gradient_node_get_rotation()
 *
 * Returns: the angle for the gradient
 *
 * Since: 4.2
 */
float
gsk_conic_gradient_node_get_angle (const GskRenderNode *node)
{
  const GskConicGradientNode *self = (const GskConicGradientNode *) node;

  return self->angle;
}

/* }}} */
/* {{{ Serialization */

static void
gsk_render_node_serialize_bytes_finish (GObject      *source,
                                        GAsyncResult *result,
                                        gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (g_output_stream_splice_finish (stream, result, &error) < 0)
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
gsk_render_node_serialize_bytes (GdkContentSerializer *serializer,
                                 GBytes               *bytes)
{
  GInputStream *input;

  input = g_memory_input_stream_new_from_bytes (bytes);

  g_output_stream_splice_async (gdk_content_serializer_get_output_stream (serializer),
                                input,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                gdk_content_serializer_get_priority (serializer),
                                gdk_content_serializer_get_cancellable (serializer),
                                gsk_render_node_serialize_bytes_finish,
                                serializer);
  g_object_unref (input);
  g_bytes_unref (bytes);
}

#ifdef CAIRO_HAS_SVG_SURFACE
static cairo_status_t
gsk_render_node_cairo_serializer_write (gpointer             user_data,
                                        const unsigned char *data,
                                        unsigned int         length)
{
  g_byte_array_append (user_data, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
gsk_render_node_svg_serializer (GdkContentSerializer *serializer)
{
  GskRenderNode *node;
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GByteArray *array;

  node = gsk_value_get_render_node (gdk_content_serializer_get_value (serializer));
  gsk_render_node_get_bounds (node, &bounds);
  array = g_byte_array_new ();

  surface = cairo_svg_surface_create_for_stream (gsk_render_node_cairo_serializer_write,
                                                 array,
                                                 bounds.size.width,
                                                 bounds.size.height);
  cairo_svg_surface_set_document_unit (surface, CAIRO_SVG_UNIT_PX);
  cairo_surface_set_device_offset (surface, -bounds.origin.x, -bounds.origin.y);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  cairo_surface_finish (surface);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      gsk_render_node_serialize_bytes (serializer, g_byte_array_free_to_bytes (array));
    }
  else
    {
      GError *error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                                           cairo_status_to_string (cairo_surface_status (surface)));
      gdk_content_serializer_return_error (serializer, error);
      g_byte_array_unref (array);
    }
  cairo_surface_destroy (surface);
}
#endif

static void
gsk_render_node_png_serializer (GdkContentSerializer *serializer)
{
  GskRenderNode *node;
  GdkTexture *texture;
  GskRenderer *renderer;
  GBytes *bytes;

  node = gsk_value_get_render_node (gdk_content_serializer_get_value (serializer));

  renderer = gsk_gl_renderer_new ();
  if (!gsk_renderer_realize (renderer, NULL, NULL))
    {
      g_object_unref (renderer);
      renderer = gsk_cairo_renderer_new ();
      if (!gsk_renderer_realize (renderer, NULL, NULL))
        {
          g_assert_not_reached ();
        }
    }
  texture = gsk_renderer_render_texture (renderer, node, NULL);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);

  bytes = gdk_texture_save_to_png_bytes (texture);
  g_object_unref (texture);

  gsk_render_node_serialize_bytes (serializer, bytes);
}

static void
gsk_render_node_content_serializer (GdkContentSerializer *serializer)
{
  const GValue *value;
  GskRenderNode *node;
  GBytes *bytes;

  value = gdk_content_serializer_get_value (serializer);
  node = gsk_value_get_render_node (value);
  bytes = gsk_render_node_serialize (node);

  gsk_render_node_serialize_bytes (serializer, bytes);
}

static void
gsk_render_node_content_deserializer_finish (GObject      *source,
                                             GAsyncResult *result,
                                             gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;
  GValue *value;
  GskRenderNode *node;
  GBytes *bytes;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (stream));

  /* For now, we ignore any parsing errors. We might want to revisit that if it turns
   * out copy/paste leads to too many errors */
  node = gsk_render_node_deserialize (bytes, NULL, NULL);

  value = gdk_content_deserializer_get_value (deserializer);
  gsk_value_take_render_node (value, node);

  gdk_content_deserializer_return_success (deserializer);
}

static void
gsk_render_node_content_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                gsk_render_node_content_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static void
gsk_render_node_init_content_serializers (void)
{
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "application/x-gtk-render-node",
                                   gsk_render_node_content_serializer,
                                   NULL,
                                   NULL);
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "text/plain;charset=utf-8",
                                   gsk_render_node_content_serializer,
                                   NULL,
                                   NULL);
  /* The serialization format only outputs ASCII, so we can do this */
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "text/plain",
                                   gsk_render_node_content_serializer,
                                   NULL,
                                   NULL);
#ifdef CAIRO_HAS_SVG_SURFACE
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "image/svg+xml",
                                   gsk_render_node_svg_serializer,
                                   NULL,
                                   NULL);
#endif
  gdk_content_register_serializer (GSK_TYPE_RENDER_NODE,
                                   "image/png",
                                   gsk_render_node_png_serializer,
                                   NULL,
                                   NULL);

  gdk_content_register_deserializer ("application/x-gtk-render-node",
                                     GSK_TYPE_RENDER_NODE,
                                     gsk_render_node_content_deserializer,
                                     NULL,
                                     NULL);
}

/* }}} */
/* {{{ Type registration */

GSK_DEFINE_RENDER_NODE_TYPE (GskConicGradientNode, gsk_conic_gradient_node)

/*< private >
 * gsk_render_node_init_types:
 *
 * Initialize all the `GskRenderNode` types provided by GSK.
 */
void
gsk_render_node_init_types (void)
{
  static gsize register_types__volatile;

  if (g_once_init_enter (&register_types__volatile))
    {
      gboolean initialized = TRUE;
      gsk_render_node_init_content_serializers ();
      g_once_init_leave (&register_types__volatile, initialized);
    }
}

/* vim:set foldmethod=marker: */
