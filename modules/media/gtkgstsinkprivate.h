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

#ifndef __GTK_GST_SINK_PRIVATE_H__
#define __GTK_GST_SINK_PRIVATE_H__

#include "gtkgstpaintableprivate.h"

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#define GTK_TYPE_GST_SINK            (gtk_gst_sink_get_type())
#define GTK_GST_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GTK_TYPE_GST_SINK,GtkGstSink))
#define GTK_GST_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GTK_TYPE_GST_SINK,GtkGstSinkClass))
#define GTK_GST_SINK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GST_SINK, GtkGstSinkClass))
#define GST_IS_GTK_BASE_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GTK_TYPE_GST_SINK))
#define GST_IS_GTK_BASE_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GTK_TYPE_GST_SINK))
#define GTK_GST_SINK_CAST(obj)       ((GtkGstSink*)(obj))

G_BEGIN_DECLS

typedef struct _GtkGstSink GtkGstSink;
typedef struct _GtkGstSinkClass GtkGstSinkClass;

struct _GtkGstSink
{
  /* <private> */
  GstVideoSink         parent;

  GstVideoInfo         v_info;
  GtkGstPaintable *    paintable;
};

struct _GtkGstSinkClass
{
  GstVideoSinkClass object_class;
};

GType gtk_gst_sink_get_type (void);

G_END_DECLS

#endif /* __GTK_GST_SINK_PRIVATE_H__ */
