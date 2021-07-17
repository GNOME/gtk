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

#include <gst/gst.h>
#include <gst/player/gstplayer.h>
#include <gst/player/gstplayer-g-main-context-signal-dispatcher.h>
#include <gst/app/gstappsrc.h>

struct _GtkGstMediaFile
{
  GtkMediaFile parent_instance;

  GstPlayer *player;
  GdkPaintable *paintable;
  GstElement *playbin;
  GstElement *appsrc;
  GInputStream *stream;
  guint64 offset;
  guint64 length;
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

G_DEFINE_TYPE_EXTENDED (GtkGstMediaFile, gtk_gst_media_file, GTK_TYPE_MEDIA_FILE, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_gst_media_file_paintable_init))

void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));

  g_io_extension_point_implement (GTK_MEDIA_FILE_EXTENSION_POINT_NAME,
                                  GTK_TYPE_GST_MEDIA_FILE,
                                  "gstreamer",
                                  10);
}

G_GNUC_NORETURN
void
g_io_module_unload (GIOModule *module)
{
  g_assert_not_reached ();
}

char **
g_io_module_query (void)
{
  char *eps[] = {
    (char *) GTK_MEDIA_FILE_EXTENSION_POINT_NAME,
    NULL
  };

  return g_strdupv (eps);
}

static void
gtk_gst_media_file_ensure_prepared (GtkGstMediaFile *self)
{
  GstPlayerMediaInfo *media_info;

  if (gtk_media_stream_is_prepared (GTK_MEDIA_STREAM (self)))
    return;

  media_info = gst_player_get_media_info (self->player);
  if (media_info)
    {
      gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (self),
                                        gst_player_media_info_get_audio_streams (media_info) != NULL,
                                        gst_player_media_info_get_video_streams (media_info) != NULL,
                                        gst_player_media_info_is_seekable (media_info),
                                        FROM_GST_TIME (gst_player_media_info_get_duration (media_info)));

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
gtk_gst_media_file_position_updated_cb (GstPlayer       *player,
                                        GstClockTime     time,
                                        GtkGstMediaFile *self)
{
  gtk_gst_media_file_ensure_prepared (self);

  gtk_media_stream_update (GTK_MEDIA_STREAM (self), FROM_GST_TIME (time));
}

static void
gtk_gst_media_file_seek_done_cb (GstPlayer       *player,
                                 GstClockTime     time,
                                 GtkGstMediaFile *self)
{
  /* if we're not seeking, we're doing the loop seek-back after EOS */
  if (gtk_media_stream_is_seeking (GTK_MEDIA_STREAM (self)))
    gtk_media_stream_seek_success (GTK_MEDIA_STREAM (self));
  gtk_media_stream_update (GTK_MEDIA_STREAM (self), FROM_GST_TIME (time));
}

static void
gtk_gst_media_file_error_cb (GstPlayer       *player,
                             GError          *error,
                             GtkGstMediaFile *self)
{
  if (gtk_media_stream_get_error (GTK_MEDIA_STREAM (self)))
    return;

  gtk_media_stream_gerror (GTK_MEDIA_STREAM (self),
                           g_error_copy (error));
}

static void
gtk_gst_media_file_end_of_stream_cb (GstPlayer       *player,
                                     GtkGstMediaFile *self)
{
  gtk_gst_media_file_ensure_prepared (self);

  if (gtk_media_stream_get_ended (GTK_MEDIA_STREAM (self)))
    return;

  if (gtk_media_stream_get_loop (GTK_MEDIA_STREAM (self)))
    {
      gst_player_seek (self->player, 0);
      return;
    }

  gtk_media_stream_stream_ended (GTK_MEDIA_STREAM (self));
}

static void
gtk_gst_media_file_feed_data (GstElement      *appsrc,
                              guint            size,
                              GtkGstMediaFile *self)
{
  GstBuffer *buffer;
  GstFlowReturn ret;
  char *buf;
  gssize read;

  if (self->offset >= self->length)
    {
      /* we are EOS, send end-of-stream */
      g_signal_emit_by_name (self->appsrc, "end-of-stream", &ret);
      return;
    }

  /* read the amount of data, we are allowed to return less if we are EOS */
  buffer = gst_buffer_new ();

  if (self->offset + size > self->length)
    size = self->length - self->offset;

  buf = g_malloc (size);
  read = g_input_stream_read (G_INPUT_STREAM (self->stream), buf, size, NULL, NULL);

  gst_buffer_append_memory (buffer,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
                              buf, size, 0, read, NULL, NULL));

  /* we need to set an offset for random access */
  GST_BUFFER_OFFSET (buffer) = self->offset;
  GST_BUFFER_OFFSET_END (buffer) = self->offset + size;

  g_signal_emit_by_name (self->appsrc, "push-buffer", buffer, &ret);
  gst_buffer_unref (buffer);

  self->offset += read;
}

static gboolean
gtk_gst_media_file_seek_data (GstElement      *appsrc,
                              guint64          position,
                              GtkGstMediaFile *self)
{
  if (g_seekable_seek (G_SEEKABLE (self->stream), position, G_SEEK_CUR, NULL, NULL))
    {
      self->offset = position;
      return TRUE;
    }

  return FALSE;
}

static void
gtk_gst_media_file_found_source (GObject         *object,
                                 GObject         *orig,
                                 GParamSpec      *pspec,
                                 GtkGstMediaFile *self)
{
  g_object_get (orig, "source", &self->appsrc, NULL);

  g_print ("got appsrc\n");

  g_return_if_fail (GST_IS_APP_SRC (self->appsrc));

  if (G_IS_SEEKABLE (self->stream))
    gst_app_src_set_stream_type (GST_APP_SRC (self->appsrc), GST_APP_STREAM_TYPE_SEEKABLE);
  else
    gst_app_src_set_stream_type (GST_APP_SRC (self->appsrc), GST_APP_STREAM_TYPE_STREAM);

  g_signal_connect (self->appsrc, "need-data", G_CALLBACK (gtk_gst_media_file_feed_data), self);
  g_signal_connect (self->appsrc, "seek-data", G_CALLBACK (gtk_gst_media_file_seek_data), self);
}

static void
gtk_gst_media_file_destroy_player (GtkGstMediaFile *self)
{
  if (self->player == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->player, gtk_gst_media_file_position_updated_cb, self);
  g_signal_handlers_disconnect_by_func (self->player, gtk_gst_media_file_end_of_stream_cb, self);
  g_signal_handlers_disconnect_by_func (self->player, gtk_gst_media_file_seek_done_cb, self);
  g_signal_handlers_disconnect_by_func (self->player, gtk_gst_media_file_error_cb, self);
  g_signal_handlers_disconnect_by_func (self->playbin, gtk_gst_media_file_found_source, self);
  g_clear_object (&self->player);
  g_clear_object (&self->appsrc);
  g_clear_object (&self->playbin);
}

static void
gtk_gst_media_file_create_player (GtkGstMediaFile *file)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (file);

  if (self->player != NULL)
    return;

  self->player = gst_player_new (GST_PLAYER_VIDEO_RENDERER (g_object_ref (self->paintable)),
                                 gst_player_g_main_context_signal_dispatcher_new (NULL));
  g_signal_connect (self->player, "position-updated", G_CALLBACK (gtk_gst_media_file_position_updated_cb), self);
  g_signal_connect (self->player, "end-of-stream", G_CALLBACK (gtk_gst_media_file_end_of_stream_cb), self);
  g_signal_connect (self->player, "seek-done", G_CALLBACK (gtk_gst_media_file_seek_done_cb), self);
  g_signal_connect (self->player, "error", G_CALLBACK (gtk_gst_media_file_error_cb), self);

  self->playbin = gst_player_get_pipeline (self->player);
  g_signal_connect (self->playbin, "deep-notify::source", G_CALLBACK (gtk_gst_media_file_found_source), self);
}

static void
gtk_gst_media_file_open (GtkMediaFile *media_file)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (media_file);
  GInputStream *stream;
  GFile *file;

  stream = gtk_media_file_get_input_stream (media_file);
  file = gtk_media_file_get_file (media_file);

  if (stream)
    self->stream = g_object_ref (stream);
  else if (file)
    self->stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
  else
    self->stream = NULL;

  self->offset = 0;
  self->length = G_MAXUINT64;

  if (G_IS_FILE_INPUT_STREAM (self->stream))
    {
      GFileInfo *info;

      info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (self->stream),
                                             G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                             NULL, NULL);
      self->length = g_file_info_get_size (info);
      g_object_unref (info);
    }

  if (G_IS_SEEKABLE (self->stream))
    self->offset = g_seekable_tell (G_SEEKABLE (self->stream));

  gtk_gst_media_file_create_player (self);

  gst_player_set_uri (self->player, "appsrc://");
  gst_player_pause (self->player);
}

static void
gtk_gst_media_file_close (GtkMediaFile *file)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (file);

  g_clear_object (&self->stream);

  gtk_gst_media_file_destroy_player (self);
}

static gboolean
gtk_gst_media_file_play (GtkMediaStream *stream)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  if (self->player == NULL)
    return FALSE;

  gst_player_play (self->player);

  return TRUE;
}

static void
gtk_gst_media_file_pause (GtkMediaStream *stream)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gst_player_pause (self->player);
}

static void
gtk_gst_media_file_seek (GtkMediaStream *stream,
                         gint64          timestamp)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gst_player_seek (self->player, TO_GST_TIME (timestamp));
}

static void
gtk_gst_media_file_update_audio (GtkMediaStream *stream,
                                 gboolean        muted,
                                 double          volume)
{
  GtkGstMediaFile *self = GTK_GST_MEDIA_FILE (stream);

  gst_player_set_mute (self->player, muted);
  gst_player_set_volume (self->player, volume * volume * volume);
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

  gtk_gst_media_file_destroy_player (self);
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

