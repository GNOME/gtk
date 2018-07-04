/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "gtkgstsinkprivate.h"

#include "gtkgstpaintableprivate.h"
#include "gtkintl.h"

enum {
  PROP_0,
  PROP_PAINTABLE,

  N_PROPS,
};

GST_DEBUG_CATEGORY (gtk_debug_gst_sink);
#define GST_CAT_DEFAULT gtk_debug_gst_sink

#define FORMATS "{ BGRA, ARGB, RGBA, ABGR, RGB, BGR }"

static GstStaticPadTemplate gtk_gst_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (FORMATS))
    );

G_DEFINE_TYPE_WITH_CODE (GtkGstSink, gtk_gst_sink,
    GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gtk_debug_gst_sink,
        "gtkgstsink", 0, "GtkGstMediaFile Video Sink"));

static GParamSpec *properties[N_PROPS] = { NULL, };


static void
gtk_gst_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GtkGstSink *gtk_sink;

  gtk_sink = GTK_GST_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info) > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND,
            GST_VIDEO_INFO_FPS_D (&gtk_sink->v_info),
            GST_VIDEO_INFO_FPS_N (&gtk_sink->v_info));
      }
    }
  }
}

static gboolean
gtk_gst_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GtkGstSink *self = GTK_GST_SINK (bsink);

  GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&self->v_info, caps))
    return FALSE;

  return TRUE;
}

static GdkMemoryFormat
gtk_gst_memory_format_from_video (GstVideoFormat format)
{
  switch ((guint) format)
  {
    case GST_VIDEO_FORMAT_BGRA:
      return GDK_MEMORY_B8G8R8A8;
    case GST_VIDEO_FORMAT_ARGB:
      return GDK_MEMORY_A8R8G8B8;
    case GST_VIDEO_FORMAT_RGBA:
      return GDK_MEMORY_R8G8B8A8;
    case GST_VIDEO_FORMAT_ABGR:
      return GDK_MEMORY_A8B8G8R8;
    case GST_VIDEO_FORMAT_RGB:
      return GDK_MEMORY_R8G8B8;
    case GST_VIDEO_FORMAT_BGR:
      return GDK_MEMORY_B8G8R8;
    default:
      g_assert_not_reached ();
      return GDK_MEMORY_A8R8G8B8;
  }
}

static GdkTexture *
gtk_gst_sink_texture_from_buffer (GtkGstSink *self,
                                  GstBuffer  *buffer)
{
  GstVideoFrame frame;
  GdkTexture *texture;
  GBytes *bytes;

  if (!gst_video_frame_map (&frame, &self->v_info, buffer, GST_MAP_READ))
    return NULL;

  bytes = g_bytes_new_with_free_func (frame.data[0],
                                      frame.info.width * frame.info.stride[0],
                                      (GDestroyNotify) gst_buffer_unref,
                                      gst_buffer_ref (buffer));
  texture = gdk_memory_texture_new (frame.info.width,
                                    frame.info.height,
                                    gtk_gst_memory_format_from_video (GST_VIDEO_FRAME_FORMAT (&frame)),
                                    bytes,
                                    frame.info.stride[0]);
  g_bytes_unref (bytes);
  gst_video_frame_unmap (&frame);

  return texture;
}

static GstFlowReturn
gtk_gst_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GtkGstSink *self;
  GdkTexture *texture;

  GST_TRACE ("rendering buffer:%p", buf);

  self = GTK_GST_SINK (vsink);

  GST_OBJECT_LOCK (self);

  texture = gtk_gst_sink_texture_from_buffer (self, buf);
  if (texture)
    {
      gtk_gst_paintable_queue_set_texture (self->paintable, texture);
      g_object_unref (texture);
    }

  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;
}

static void
gtk_gst_sink_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)

{
  GtkGstSink *self = GTK_GST_SINK (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      self->paintable = g_value_dup_object (value);
      if (self->paintable == NULL)
        self->paintable = GTK_GST_PAINTABLE (gtk_gst_paintable_new ());
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gst_sink_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkGstSink *self = GTK_GST_SINK (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gst_sink_dispose (GObject *object)
{
  GtkGstSink *self = GTK_GST_SINK (object);

  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (gtk_gst_sink_parent_class)->dispose (object);
}

static void
gtk_gst_sink_class_init (GtkGstSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *gstvideosink_class = GST_VIDEO_SINK_CLASS (klass);

  gobject_class->set_property = gtk_gst_sink_set_property;
  gobject_class->get_property = gtk_gst_sink_get_property;
  gobject_class->dispose = gtk_gst_sink_dispose;

  gstbasesink_class->set_caps = gtk_gst_sink_set_caps;
  gstbasesink_class->get_times = gtk_gst_sink_get_times;

  gstvideosink_class->show_frame = gtk_gst_sink_show_frame;

  /**
   * GtkGstSink:paintable:
   *
   * The paintable that provides the picture for this sink.
   */
  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable",
                         P_("paintable"),
                         P_("Paintable providing the picture"),
                         GTK_TYPE_GST_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gst_element_class_set_metadata (gstelement_class,
      "GtkMediaStream Video Sink",
      "Sink/Video", "The video sink used by GtkMediaStream",
      "Matthew Waters <matthew@centricular.com>, "
      "Benjamin Otte <otte@gnome.org>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gtk_gst_sink_template);
}

static void
gtk_gst_sink_init (GtkGstSink * gtk_sink)
{
}

