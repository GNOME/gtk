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

#include "gtkgstpaintableprivate.h"

#include "gtkgstsinkprivate.h"

#include <gst/player/gstplayer-video-renderer.h>

struct _GtkGstPaintable
{
  GObject parent_instance;

  GdkPaintable *image;
};

struct _GtkGstPaintableClass
{
  GObjectClass parent_class;
};

static void
gtk_gst_paintable_paintable_snapshot (GdkPaintable *paintable,
                                      GdkSnapshot  *snapshot,
                                      double        width,
                                      double        height)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    gdk_paintable_snapshot (self->image, snapshot, width, height);
}

static GdkPaintable *
gtk_gst_paintable_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    return GDK_PAINTABLE (g_object_ref (self->image));

  return gdk_paintable_new_empty (0, 0);
}

static int
gtk_gst_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    return gdk_paintable_get_intrinsic_width (self->image);

  return 0;
}

static int
gtk_gst_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    return gdk_paintable_get_intrinsic_height (self->image);

  return 0;
}

static double
gtk_gst_paintable_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    return gdk_paintable_get_intrinsic_aspect_ratio (self->image);

  return 0.0;
};

static void
gtk_gst_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_gst_paintable_paintable_snapshot;
  iface->get_current_image = gtk_gst_paintable_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_gst_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_gst_paintable_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_gst_paintable_paintable_get_intrinsic_aspect_ratio;
}

static GstElement *
gtk_gst_paintable_video_renderer_create_video_sink (GstPlayerVideoRenderer *renderer,
                                                    GstPlayer              *player)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (renderer);

  return g_object_new (GTK_TYPE_GST_SINK,
                       "paintable", self,
                       NULL);
}

static void
gtk_gst_paintable_video_renderer_init (GstPlayerVideoRendererInterface *iface)
{
  iface->create_video_sink = gtk_gst_paintable_video_renderer_create_video_sink;
}

G_DEFINE_TYPE_WITH_CODE (GtkGstPaintable, gtk_gst_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_gst_paintable_paintable_init)
                         G_IMPLEMENT_INTERFACE (GST_TYPE_PLAYER_VIDEO_RENDERER,
                                                gtk_gst_paintable_video_renderer_init));

static void
gtk_gst_paintable_dispose (GObject *object)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (object);
  
  g_clear_object (&self->image);

  G_OBJECT_CLASS (gtk_gst_paintable_parent_class)->dispose (object);
}

static void
gtk_gst_paintable_class_init (GtkGstPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_gst_paintable_dispose;
}

static void
gtk_gst_paintable_init (GtkGstPaintable *self)
{
}

GdkPaintable *
gtk_gst_paintable_new (void)
{
  return g_object_new (GTK_TYPE_GST_PAINTABLE, NULL);
}

static void
gtk_gst_paintable_set_paintable (GtkGstPaintable *self,
                                 GdkPaintable    *paintable)
{
  gboolean size_changed;

  if (self->image == paintable)
    return;

  if (self->image == NULL ||
      gdk_paintable_get_intrinsic_width (self->image) != gdk_paintable_get_intrinsic_width (paintable) ||
      gdk_paintable_get_intrinsic_height (self->image) != gdk_paintable_get_intrinsic_height (paintable) ||
      gdk_paintable_get_intrinsic_aspect_ratio (self->image) != gdk_paintable_get_intrinsic_aspect_ratio (paintable))
    size_changed = TRUE;
  else
    size_changed = FALSE;

  g_set_object (&self->image, paintable);

  if (size_changed)
    gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

typedef struct _SetTextureInvocation SetTextureInvocation;

struct _SetTextureInvocation {
  GtkGstPaintable *paintable;
  GdkTexture      *texture;
};

static void
set_texture_invocation_free (SetTextureInvocation *invoke)
{
  g_object_unref (invoke->paintable);
  g_object_unref (invoke->texture);

  g_slice_free (SetTextureInvocation, invoke);
}

static gboolean
gtk_gst_paintable_set_texture_invoke (gpointer data)
{
  SetTextureInvocation *invoke = data;

  gtk_gst_paintable_set_paintable (invoke->paintable,
                                   GDK_PAINTABLE (invoke->texture));

  return G_SOURCE_REMOVE;
}

void
gtk_gst_paintable_queue_set_texture (GtkGstPaintable *self,
                                     GdkTexture      *texture)
{
  SetTextureInvocation *invoke;

  invoke = g_slice_new0 (SetTextureInvocation);
  invoke->paintable = g_object_ref (self);
  invoke->texture = g_object_ref (texture);

  g_main_context_invoke_full (NULL,
                              G_PRIORITY_DEFAULT,
                              gtk_gst_paintable_set_texture_invoke,
                              invoke,
                              (GDestroyNotify) set_texture_invocation_free);
}

