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

#include "gtkgstmediafileprivate.h"
#include "gtkgstpaintableprivate.h"
#include "gtkmodulesprivate.h"

#include <gst/play/play.h>

struct _GtkGstMediaFile
{
  GtkMediaFile parent_instance;

  GstPlay *play;
  GstPlaySignalAdapter *play_adapter;
  GdkPaintable *paintable;
};

struct _GtkGstMediaFileClass
{
  GtkMediaFileClass parent_class;
};

#define TO_GST_TIME(ts) ((ts) * (GST_SECOND / G_USEC_PER_SEC))
#define FROM_GST_TIME(ts) ((ts) / (GST_SECOND / G_USEC_PER_SEC))

static void
gtk_gst_media_file_paintable_snapshot (GdkPaintable *paintable,
                                       GdkSnapshot  *snapshot,
                                       double        width,
                                       double        height)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (paintable);

  gdk_paintable_snapshot (self->paintable, snapshot, width, height);
}

static GdkPaintable *
gtk_gst_media_file_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (paintable);

  return gdk_paintable_get_current_image (self->paintable);
}

static int
gtk_gst_media_file_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (paintable);

  return gdk_paintable_get_intrinsic_width (self->paintable);
}

static int
gtk_gst_media_file_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (paintable);

  return gdk_paintable_get_intrinsic_height (self->paintable);
}

static double gtk_gst_media_file_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (paintable);

  return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
};

static void
gtk_gst_media_file_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_gst_media_file_paintable_snapshot;
  iface->get_current_image = gtk_gst_media_file_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_gst_media_file_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_gst_media_file_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_gst_media_file_paintable_get_intrinsic_aspect_ratio;
}

GTK_DEFINE_BUILTIN_MODULE_TYPE_WITH_CODE (GtkGstMediaFile, gtk_gst_media_file, GTK_TYPE_MEDIA_FILE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_gst_media_file_paintable_init);
                         g_io_extension_point_implement (GTK_MEDIA_FILE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "gstreamer",
                                                         20))

static void
gtk_gst_media_file_ensure_prepared (GtkGstMediaFile *self)
{
  GstPlayMediaInfo *media_info;

  if (gtk_media_stream_is_prepared (GTK_MEDIA_STREAM (self)))
    return;

  media_info = gst_play_get_media_info (self->play);
  if (media_info)
    {
      GstClockTime duration = gst_play_media_info_get_duration (media_info);

      gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (self),
                                        gst_play_media_info_get_number_of_audio_streams (media_info) > 0,
                                        gst_play_media_info_get_number_of_video_streams (media_info) > 0,
                                        gst_play_media_info_is_seekable (media_info),
                                        duration == GST_CLOCK_TIME_NONE ? 0 : FROM_GST_TIME (duration));

      g_object_unref (media_info);
    }
  else
    {
      /* Assuming everything exists is better for the user than pretending it doesn't exist.
       * Better to be able to control non-existing audio than not be able to control existing audio.
       *
       * Only for seeking we can't do a thing, because with 0 duration we can't seek anywhere.
       */
      gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (self),
                                        TRUE,
                                        TRUE,
                                        FALSE,
                                        0);
    }
}

static void
gtk_gst_media_file_position_updated_cb (GstPlaySignalAdapter *adapter,
                                        GstClockTime          time,
                                        GtkGstMediaFile      *self)
{
  gtk_gst_media_file_ensure_prepared (self);

  gtk_media_stream_update (GTK_MEDIA_STREAM (self), FROM_GST_TIME (time));
}

static void
gtk_gst_media_file_media_info_updated_cb (GstPlaySignalAdapter *adapter,
                                          GstPlayMediaInfo     *media_info,
                                          GtkGstMediaFile      *self)
{
  /* clock_time == 0: https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad/-/issues/1588
   * GstPlayer's first media-info-updated comes with 0 duration
   *
   * clock_time == -1: Seen on loading an audio-only ogg
   */
  GstClockTime clock_time = gst_play_media_info_get_duration (media_info);
  if (clock_time == 0 || clock_time == -1)
    return;

  gtk_gst_media_file_ensure_prepared (self);
}

static void
gtk_gst_media_file_seek_done_cb (GstPlaySignalAdapter *adapter,
                                 GstClockTime          time,
                                 GtkGstMediaFile      *self)
{
  /* if we're not seeking, we're doing the loop seek-back after EOS */
  if (gtk_media_stream_is_seeking (GTK_MEDIA_STREAM (self)))
    gtk_media_stream_seek_success (GTK_MEDIA_STREAM (self));
  gtk_media_stream_update (GTK_MEDIA_STREAM (self), FROM_GST_TIME (time));
}

static void
gtk_gst_media_file_error_cb (GstPlaySignalAdapter *adapter,
                             GError               *error,
                             GstStructure         *details,
                             GtkGstMediaFile      *self)
{
  if (gtk_media_stream_get_error (GTK_MEDIA_STREAM (self)))
    return;

  gtk_media_stream_gerror (GTK_MEDIA_STREAM (self),
                           g_error_copy (error));
}

static void
gtk_gst_media_file_end_of_stream_cb (GstPlaySignalAdapter *adapter,
                                     GtkGstMediaFile      *self)
{
  gtk_gst_media_file_ensure_prepared (self);

  if (gtk_media_stream_get_ended (GTK_MEDIA_STREAM (self)))
    return;

  if (gtk_media_stream_get_loop (GTK_MEDIA_STREAM (self)))
    {
      gst_play_seek (self->play, 0);
      return;
    }

  gtk_media_stream_stream_ended (GTK_MEDIA_STREAM (self));
}

static void
gtk_gst_media_file_destroy_play (GtkGstMediaFile *self)
{
  if (self->play == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->play_adapter, gtk_gst_media_file_media_info_updated_cb, self);
  g_signal_handlers_disconnect_by_func (self->play_adapter, gtk_gst_media_file_position_updated_cb, self);
  g_signal_handlers_disconnect_by_func (self->play_adapter, gtk_gst_media_file_end_of_stream_cb, self);
  g_signal_handlers_disconnect_by_func (self->play_adapter, gtk_gst_media_file_seek_done_cb, self);
  g_signal_handlers_disconnect_by_func (self->play_adapter, gtk_gst_media_file_error_cb, self);
  g_object_unref (self->play_adapter);
  gst_play_stop (self->play);
  g_object_unref (self->play);
  self->play = NULL;
}

static void
gtk_gst_media_file_create_play (GtkGstMediaFile *file)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (file);

  if (self->play != NULL)
    return;

  self->play = gst_play_new (GST_PLAY_VIDEO_RENDERER (g_object_ref (self->paintable)));
  self->play_adapter = gst_play_signal_adapter_new (self->play);
  g_signal_connect (self->play_adapter, "media-info-updated", G_CALLBACK (gtk_gst_media_file_media_info_updated_cb), self);
  g_signal_connect (self->play_adapter, "position-updated", G_CALLBACK (gtk_gst_media_file_position_updated_cb), self);
  g_signal_connect (self->play_adapter, "end-of-stream", G_CALLBACK (gtk_gst_media_file_end_of_stream_cb), self);
  g_signal_connect (self->play_adapter, "seek-done", G_CALLBACK (gtk_gst_media_file_seek_done_cb), self);
  g_signal_connect (self->play_adapter, "error", G_CALLBACK (gtk_gst_media_file_error_cb), self);
}

static void
gtk_gst_media_file_open (GtkMediaFile *media_file)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (media_file);
  GFile *file;

  gtk_gst_media_file_create_play (self);

  file = gtk_media_file_get_file (media_file);

  if (file)
    {
      /* XXX: This is technically incorrect because GFile uris aren't real uris */
      char *uri = g_file_get_uri (file);

      gst_play_set_uri (self->play, uri);

      g_free (uri);
    }
  else
    {
      /* It's an input stream */
      g_error ("Input Streams are currently not supported. Please pass a File based MediaFile.");
      g_assert_not_reached ();
    }

  gst_play_pause (self->play);
}

static void
gtk_gst_media_file_close (GtkMediaFile *file)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (file);

  gtk_gst_media_file_destroy_play (self);
}

static gboolean
gtk_gst_media_file_play (GtkMediaStream *stream)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  if (self->play == NULL)
    return FALSE;

  gst_play_play (self->play);

  return TRUE;
}

static void
gtk_gst_media_file_pause (GtkMediaStream *stream)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gst_play_pause (self->play);
}

static void
gtk_gst_media_file_seek (GtkMediaStream *stream,
                         gint64          timestamp)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gst_play_seek (self->play, TO_GST_TIME (timestamp));
}

static void
gtk_gst_media_file_update_audio (GtkMediaStream *stream,
                                 gboolean        muted,
                                 double          volume)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gst_play_set_mute (self->play, muted);
  gst_play_set_volume (self->play, volume * volume * volume);
}

static void
gtk_gst_media_file_realize (GtkMediaStream *stream,
                            GdkSurface     *surface)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gtk_gst_paintable_realize (GTK_GST_PAINTABLE (self->paintable), surface);
}

static void
gtk_gst_media_file_unrealize (GtkMediaStream *stream,
                              GdkSurface     *surface)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gtk_gst_paintable_unrealize (GTK_GST_PAINTABLE (self->paintable), surface);
}

static void
gtk_gst_media_file_dispose (GObject *object)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (object);

  gtk_gst_media_file_destroy_play (self);
  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);
      g_clear_object (&self->paintable);
    }

  G_OBJECT_CLASS (gtk_gst_media_file_parent_class)->dispose (object);
}

static void
gtk_gst_media_file_class_init (GtkGstMediaFileClass *klass)
{
  GtkMediaFileClass *file_class = GTK_MEDIA_FILE_CLASS (klass);
  GtkMediaStreamClass *stream_class = GTK_MEDIA_STREAM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  file_class->open = gtk_gst_media_file_open;
  file_class->close = gtk_gst_media_file_close;

  stream_class->play = gtk_gst_media_file_play;
  stream_class->pause = gtk_gst_media_file_pause;
  stream_class->seek = gtk_gst_media_file_seek;
  stream_class->update_audio = gtk_gst_media_file_update_audio;
  stream_class->realize = gtk_gst_media_file_realize;
  stream_class->unrealize = gtk_gst_media_file_unrealize;

  gobject_class->dispose = gtk_gst_media_file_dispose;
}

static void
gtk_gst_media_file_init (GtkGstMediaFile *self)
{
  self->paintable = gtk_gst_paintable_new ();
  g_signal_connect_swapped (self->paintable, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);
  g_signal_connect_swapped (self->paintable, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);
}

