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
