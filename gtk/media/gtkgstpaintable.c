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

#include <gtk/gtk.h>
#include <gst/play/play.h>
#include <graphene.h>

#include <math.h>

#ifdef GDK_WINDOWING_WIN32
#include <gdk/win32/gdkwin32.h>
#endif

struct _GtkGstPaintable
{
  GObject parent_instance;

  GdkPaintable *image;
  double pixel_aspect_ratio;
  graphene_rect_t viewport;
  int orientation;

  GdkSurface *surface;
};

struct _GtkGstPaintableClass
{
  GObjectClass parent_class;
};

static gboolean
is_orientation_rotated (int orientation)
{
  switch (orientation)
    {
    case GST_VIDEO_ORIENTATION_90R:
    case GST_VIDEO_ORIENTATION_90L:
    case GST_VIDEO_ORIENTATION_UL_LR:
    case GST_VIDEO_ORIENTATION_UR_LL:
      return TRUE;
    default:
      return FALSE;
    }
}

static void
gtk_gst_paintable_paintable_snapshot (GdkPaintable *paintable,
                                      GdkSnapshot  *snapshot,
                                      double        width,
                                      double        height)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);
  float sx, sy;

  if (!self->image)
    return;

  gtk_snapshot_save (snapshot);

  sx = gdk_paintable_get_intrinsic_width (self->image) / self->viewport.size.width;
  sy = gdk_paintable_get_intrinsic_height (self->image) / self->viewport.size.height;

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (-self->viewport.origin.x * width / self->viewport.size.width,
                                                -self->viewport.origin.y * height / self->viewport.size.height));

  if (self->orientation != GST_VIDEO_ORIENTATION_IDENTITY)
    {
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2, height / 2));

      switch (self->orientation)
        {
        case GST_VIDEO_ORIENTATION_90R:
          gtk_snapshot_rotate (snapshot, 90.0);
          break;
        case GST_VIDEO_ORIENTATION_180:
          gtk_snapshot_scale (snapshot, -1.0, -1.0);
          break;
        case GST_VIDEO_ORIENTATION_90L:
          gtk_snapshot_rotate (snapshot, 270.0);
          break;
        case GST_VIDEO_ORIENTATION_HORIZ:
          gtk_snapshot_scale (snapshot, -1.0, 1.0);
          break;
        case GST_VIDEO_ORIENTATION_VERT:
          gtk_snapshot_scale (snapshot, 1.0, -1.0);
          break;
        case GST_VIDEO_ORIENTATION_UL_LR:
          gtk_snapshot_rotate (snapshot, 90.0);
          gtk_snapshot_scale (snapshot, 1.0, -1.0);
          break;
        case GST_VIDEO_ORIENTATION_UR_LL:
          gtk_snapshot_rotate (snapshot, 270.0);
          gtk_snapshot_scale (snapshot, 1.0, -1.0);
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      if (is_orientation_rotated (self->orientation))
        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-height / 2, -width / 2));
      else
        gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-width / 2, -height / 2));
    }

  if (is_orientation_rotated (self->orientation))
    gdk_paintable_snapshot (self->image, snapshot, height * sy, width * sx);
  else
    gdk_paintable_snapshot (self->image, snapshot, width * sx, height *sy);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
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
    {
      if (is_orientation_rotated (self->orientation))
        return round (self->viewport.size.height);
      else
        return round (self->viewport.size.width);
    }

  return 0;
}

static int
gtk_gst_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    {
      if (is_orientation_rotated (self->orientation))
        return ceil (self->viewport.size.width);
      else
        return ceil (self->viewport.size.height);
    }

  return 0;
}

static double
gtk_gst_paintable_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (paintable);

  if (self->image)
    {
      if (is_orientation_rotated (self->orientation))
        return self->viewport.size.height / self->viewport.size.width;
      else
        return self->viewport.size.width / self->viewport.size.height;
    }

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
gtk_gst_paintable_video_renderer_create_video_sink (GstPlayVideoRenderer *renderer,
                                                    GstPlay              *play)
{
  GtkGstPaintable *self = GTK_GST_PAINTABLE (renderer);
  GstElement *sink;
  gboolean uses_gl;
  GdkDisplay *display;
  GdkGLContext *context;
  GError *error = NULL;

  if (self->surface)
    display = gdk_surface_get_display (self->surface);
  else
    display = gdk_display_get_default ();

  context = gdk_display_create_gl_context (display, &error);
  if (context == NULL)
    {
      GST_INFO ("failed to create GDK GL context: %s", error->message);
      g_error_free (error);
    }
  else if (!gdk_gl_context_realize (context, &error))
    {
      GST_INFO ("failed to realize GDK GL context: %s", error->message);
      g_clear_object (&context);
      g_error_free (error);
    }

  sink = g_object_new (GTK_TYPE_GST_SINK,
                       "paintable", self,
                       "gl-context", context,
                       "display", display,
                       NULL);

  g_object_get (GTK_GST_SINK (sink), "uses-gl", &uses_gl, NULL);

#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    {
      GstElement *bin, *convert, *upload;
      GstPad *pad, *ghostpad;
      gboolean res = TRUE;

      convert = gst_element_factory_make ("d3d12convert", NULL);
      if (convert)
      {
        bin = gst_bin_new ("d3d12sinkbin");
        res &= gst_bin_add (GST_BIN (bin), convert);
        res &= gst_bin_add (GST_BIN (bin), sink);
        res &= gst_element_link_pads (convert, "src", sink, "sink");
        upload = gst_element_factory_make ("d3d12upload", NULL);
        if (upload)
          {
            res &= gst_bin_add (GST_BIN (bin), upload);
            res &= gst_element_link_pads (upload, "src", convert, "sink");
            pad = gst_element_get_static_pad (upload, "sink");
          }
        else
          {
            pad = gst_element_get_static_pad (convert, "sink");
          }
        ghostpad = gst_ghost_pad_new ("sink", pad);
        gst_element_add_pad (bin, ghostpad);
        gst_object_unref (pad);
        g_assert (res);
        sink = bin;
      }
    }
  else
#endif
  if (uses_gl)
    {
      GstElement *glsinkbin;

      glsinkbin = gst_element_factory_make ("glsinkbin", NULL);

      if (glsinkbin)
        {
          g_object_set (glsinkbin, "sink", sink, NULL);
          sink = glsinkbin;
        }
    }
  else
    {
      if (context != NULL)
        {
          g_warning ("GstGL context creation failed, falling back to non-GL playback");

          g_object_unref (sink);
          sink = g_object_new (GTK_TYPE_GST_SINK,
                               "paintable", self,
                               "display", display,
                               NULL);
        }
    }

  g_clear_object (&context);

  return sink;
}

static void
gtk_gst_paintable_video_renderer_init (GstPlayVideoRendererInterface *iface)
{
  iface->create_video_sink = gtk_gst_paintable_video_renderer_create_video_sink;
}

G_DEFINE_TYPE_WITH_CODE (GtkGstPaintable, gtk_gst_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_gst_paintable_paintable_init)
                         G_IMPLEMENT_INTERFACE (GST_TYPE_PLAY_VIDEO_RENDERER,
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

void
gtk_gst_paintable_realize (GtkGstPaintable *self,
                           GdkSurface      *surface)
{
  if (self->surface)
    return;

  self->surface = surface;
}

void
gtk_gst_paintable_unrealize (GtkGstPaintable *self,
                             GdkSurface      *surface)
{
  /* XXX: We could be smarter here and:
   * - track how often we were realized with that surface
   * - track alternate surfaces
   */

  if (self->surface == surface)
    self->surface = NULL;
}

static void
gtk_gst_paintable_set_paintable (GtkGstPaintable       *self,
                                 GdkPaintable          *paintable,
                                 double                 pixel_aspect_ratio,
                                 const graphene_rect_t *viewport,
                                 int                    orientation)
{
  gboolean size_changed;

  if (self->image == paintable)
    return;

  if (self->image == NULL ||
      is_orientation_rotated (self->orientation) != is_orientation_rotated (orientation) ||
      gdk_paintable_get_intrinsic_height (self->image) != gdk_paintable_get_intrinsic_height (paintable) ||
      !G_APPROX_VALUE (self->pixel_aspect_ratio * gdk_paintable_get_intrinsic_width (self->image),
                       pixel_aspect_ratio * gdk_paintable_get_intrinsic_width (paintable),
                       FLT_EPSILON) ||
      !G_APPROX_VALUE (gdk_paintable_get_intrinsic_aspect_ratio (self->image),
                       gdk_paintable_get_intrinsic_aspect_ratio (paintable),
                       FLT_EPSILON) ||
      !graphene_rect_equal (viewport, &self->viewport))
    size_changed = TRUE;
  else
    size_changed = FALSE;

  g_set_object (&self->image, paintable);
  self->pixel_aspect_ratio = pixel_aspect_ratio;
  self->viewport = *viewport;
  self->orientation = orientation;

  if (size_changed)
    gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

typedef struct _SetTextureInvocation SetTextureInvocation;

struct _SetTextureInvocation {
  GtkGstPaintable *paintable;
  GdkTexture      *texture;
  double           pixel_aspect_ratio;
  graphene_rect_t  viewport;
  int              orientation;
};

static void
set_texture_invocation_free (SetTextureInvocation *invoke)
{
  g_object_unref (invoke->paintable);
  g_object_unref (invoke->texture);

  g_free (invoke);
}

static gboolean
gtk_gst_paintable_set_texture_invoke (gpointer data)
{
  SetTextureInvocation *invoke = data;

  gtk_gst_paintable_set_paintable (invoke->paintable,
                                   GDK_PAINTABLE (invoke->texture),
                                   invoke->pixel_aspect_ratio,
                                   &invoke->viewport,
                                   invoke->orientation);

  return G_SOURCE_REMOVE;
}

void
gtk_gst_paintable_queue_set_texture (GtkGstPaintable       *self,
                                     GdkTexture            *texture,
                                     double                 pixel_aspect_ratio,
                                     const graphene_rect_t *viewport,
                                     int                    orientation)
{
  SetTextureInvocation *invoke;

  invoke = g_new0 (SetTextureInvocation, 1);
  invoke->paintable = g_object_ref (self);
  invoke->texture = g_object_ref (texture);
  invoke->pixel_aspect_ratio = pixel_aspect_ratio;
  invoke->viewport = *viewport;
  invoke->orientation = orientation;

  g_main_context_invoke_full (NULL,
                              G_PRIORITY_DEFAULT,
                              gtk_gst_paintable_set_texture_invoke,
                              invoke,
                              (GDestroyNotify) set_texture_invocation_free);
}

