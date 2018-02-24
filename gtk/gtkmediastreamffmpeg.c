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

#include "gtkmediastreamffmpegprivate.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct _GtkVideoFrameFFMpeg GtkVideoFrameFFMpeg;

struct _GtkVideoFrameFFMpeg
{
  GdkTexture *texture;
  gint64 timestamp;
};

struct _GtkMediaStreamFFMpeg
{
  GtkMediaStream parent_instance;

  char *filename;

  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  int stream_id;
  struct SwsContext *sws_ctx;

  GtkVideoFrameFFMpeg current_frame;
  GtkVideoFrameFFMpeg next_frame;

  gint64 start_time; /* monotonic time when we displayed the last frame */
  guint next_frame_cb; /* Source ID of next frame callback */
};

struct _GtkMediaStreamFFMpegClass
{
  GtkMediaStreamClass parent_class;
};

static void
gtk_video_frame_ffmpeg_init (GtkVideoFrameFFMpeg *frame,
                             GdkTexture          *texture,
                             gint64               timestamp)
{
  frame->texture = texture;
  frame->timestamp = timestamp;
}

static void
gtk_video_frame_ffmpeg_clear (GtkVideoFrameFFMpeg *frame)
{
  g_clear_object (&frame->texture);
  frame->timestamp = 0;
}

static gboolean
gtk_video_frame_ffmpeg_is_empty (GtkVideoFrameFFMpeg *frame)
{
  return frame->texture == NULL;
}

static void
gtk_video_frame_ffmpeg_move (GtkVideoFrameFFMpeg *dest,
                             GtkVideoFrameFFMpeg *src)
{
  *dest = *src;
  src->texture = NULL;
  src->timestamp = 0;
}

static void
gtk_media_stream_ffmpeg_paintable_snapshot (GdkPaintable *paintable,
                                            GdkSnapshot  *snapshot,
                                            double        width,
                                            double        height)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (paintable);

  if (!gtk_video_frame_ffmpeg_is_empty (&video->current_frame))
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (video->current_frame.texture), snapshot, width, height);
    }
  else
    {
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA) { 1.0, 0.1, 0.6, 1.0 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height),
                                 "Video Fallback image");
    }
}

static GdkPaintable *
gtk_media_stream_ffmpeg_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (paintable);

  return GDK_PAINTABLE (g_object_ref (video->current_frame.texture));
}

static int
gtk_media_stream_ffmpeg_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->width;

  return 0;
}

static int
gtk_media_stream_ffmpeg_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->height;

  return 0;
}

static double gtk_media_stream_ffmpeg_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (paintable);

  if (video->codec_ctx)
    return (double) video->codec_ctx->width / video->codec_ctx->height;

  return 0.0;
};

static void
gtk_media_stream_ffmpeg_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_media_stream_ffmpeg_paintable_snapshot;
  iface->get_current_image = gtk_media_stream_ffmpeg_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_media_stream_ffmpeg_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_media_stream_ffmpeg_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_media_stream_ffmpeg_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_WITH_CODE (GtkMediaStreamFFMpeg, gtk_media_stream_ffmpeg, GTK_TYPE_MEDIA_STREAM,
                         av_register_all ();
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_media_stream_ffmpeg_paintable_init))

static void
gtk_media_stream_ffmpeg_set_error (GtkMediaStreamFFMpeg *video,
                          const char   *error_message)
{
}

static void
gtk_media_stream_ffmpeg_set_ffmpeg_error (GtkMediaStreamFFMpeg *video,
                                          int           av_errnum)
{
  char s[AV_ERROR_MAX_STRING_SIZE];

  if (av_strerror (av_errnum, s, sizeof (s) != 0))
    snprintf (s, sizeof (s), "Unspecified error decoding video");

  gtk_media_stream_ffmpeg_set_error (video, s);
}

static gboolean
gtk_media_stream_ffmpeg_decode_frame (GtkMediaStreamFFMpeg *video,
                                      GtkVideoFrameFFMpeg  *result)
{
  GdkTexture *texture;
  AVPacket packet;
  AVFrame *frame;
  int errnum;
  guchar *data;

  frame = av_frame_alloc ();

  for (errnum = av_read_frame (video->format_ctx, &packet);
       errnum >= 0;
       errnum = av_read_frame (video->format_ctx, &packet))
    {
      if (packet.stream_index == video->stream_id)
        {
          errnum = avcodec_send_packet (video->codec_ctx, &packet);
          if (errnum >= 0)
            {
              errnum = avcodec_receive_frame (video->codec_ctx, frame);
              if (errnum >= 0) 
                {
                  av_packet_unref (&packet);
                  break;
                }
            }
        }

      av_packet_unref (&packet);
    }

  if (errnum < 0)
    {
      gtk_media_stream_ffmpeg_set_ffmpeg_error (video, errnum);
      av_frame_free (&frame);
      return FALSE;
    }
  
  data = g_try_malloc0 (video->codec_ctx->width * video->codec_ctx->height * 4);
  if (data == NULL)
    {
      gtk_media_stream_ffmpeg_set_error (video, "Not enough memory");
      av_frame_free (&frame);
      return FALSE;
    }

  sws_scale(video->sws_ctx,
            (const uint8_t * const *) frame->data, frame->linesize,
            0, video->codec_ctx->height,
            (uint8_t *[1]) { data }, (int[1]) { video->codec_ctx->width * 4 });
  texture = gdk_texture_new_for_data (data,
                                      video->codec_ctx->width,
                                      video->codec_ctx->height,
                                      video->codec_ctx->width * 4);
  g_free (data);

  gtk_video_frame_ffmpeg_init (result,
                               texture,
                               av_rescale_q (av_frame_get_best_effort_timestamp (frame),
                                             video->format_ctx->streams[video->stream_id]->time_base,
                                             (AVRational) { 1, G_USEC_PER_SEC })); 

  av_frame_free (&frame);

  return TRUE;
}

static gboolean
gtk_media_stream_ffmpeg_next_frame_cb (gpointer data);
static void
gtk_media_stream_ffmpeg_queue_frame (GtkMediaStreamFFMpeg *video)
{
  gint64 time, frame_time;
  guint delay;
  
  time = g_get_monotonic_time ();
  frame_time = video->start_time + video->next_frame.timestamp;
  delay = time > frame_time ? 0 : (frame_time - time) / 1000;

  video->next_frame_cb = g_timeout_add (delay, gtk_media_stream_ffmpeg_next_frame_cb, video);
}

static gboolean
gtk_media_stream_ffmpeg_next_frame_cb (gpointer data)
{
  GtkMediaStreamFFMpeg *video = data;
  
  video->next_frame_cb = 0;

  if (gtk_video_frame_ffmpeg_is_empty (&video->next_frame))
    {
      gtk_media_stream_ended (GTK_MEDIA_STREAM (video));
    }
  else
    {
      gtk_video_frame_ffmpeg_clear (&video->current_frame);
      gtk_video_frame_ffmpeg_move (&video->current_frame,
                                   &video->next_frame);

      gtk_media_stream_update (GTK_MEDIA_STREAM (video),
                               video->current_frame.timestamp);
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));

      if (gtk_media_stream_ffmpeg_decode_frame (video, &video->next_frame))
        {
          gtk_media_stream_ffmpeg_queue_frame (video);
        }
      else
        {
          gtk_media_stream_ended (GTK_MEDIA_STREAM (video));
        }
    }

  return G_SOURCE_REMOVE;
}

static gboolean
gtk_media_stream_ffmpeg_play (GtkMediaStream *stream)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (stream);

  if (gtk_video_frame_ffmpeg_is_empty (&video->next_frame) &&
      !gtk_media_stream_ffmpeg_decode_frame (video, &video->next_frame))
    return FALSE;

  video->start_time = g_get_monotonic_time () - video->current_frame.timestamp;

  gtk_media_stream_ffmpeg_queue_frame (video);

  return TRUE;
}

static void
gtk_media_stream_ffmpeg_pause (GtkMediaStream *stream)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (stream);

  if (video->next_frame_cb)
    {
      g_source_remove (video->next_frame_cb);
      video->next_frame_cb = 0;
    }

  video->start_time = 0;
}

static void
gtk_media_stream_ffmpeg_seek (GtkMediaStream *stream,
                              gint64          timestamp)                           
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (stream);
  int errnum;

  errnum = av_seek_frame (video->format_ctx,
                          video->stream_id,
                          av_rescale_q (timestamp,
                                        (AVRational) { 1, G_USEC_PER_SEC },
                                        video->format_ctx->streams[video->stream_id]->time_base),
                                        AVSEEK_FLAG_BACKWARD);
  if (errnum < 0)
    {
      gtk_media_stream_seek_failed (stream);
      return;
    }
  
  gtk_media_stream_seek_success (stream);

  gtk_video_frame_ffmpeg_clear (&video->next_frame);
  gtk_video_frame_ffmpeg_clear (&video->current_frame);
  if (gtk_media_stream_ffmpeg_decode_frame (video, &video->current_frame))
    gtk_media_stream_update (stream, video->current_frame.timestamp);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));

  if (gtk_media_stream_get_playing (stream))
    {
      gtk_media_stream_ffmpeg_pause (stream);
      if (!gtk_media_stream_ffmpeg_play (stream))
        gtk_media_stream_ended (stream);
    }
}

static void 
gtk_media_stream_ffmpeg_clear (GtkMediaStreamFFMpeg *video)
{
  g_clear_pointer (&video->filename, g_free);

  g_clear_pointer (&video->sws_ctx, sws_freeContext);
  g_clear_pointer (&video->codec_ctx, avcodec_close);
  avformat_close_input (&video->format_ctx);
  video->stream_id = -1;
  gtk_video_frame_ffmpeg_clear (&video->next_frame);
  gtk_video_frame_ffmpeg_clear (&video->current_frame);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
}

static void
gtk_media_stream_ffmpeg_dispose (GObject *object)
{
  GtkMediaStreamFFMpeg *video = GTK_MEDIA_STREAM_FFMPEG (object);

  gtk_media_stream_ffmpeg_pause (GTK_MEDIA_STREAM (video));
  gtk_media_stream_ffmpeg_clear (video);

  G_OBJECT_CLASS (gtk_media_stream_ffmpeg_parent_class)->dispose (object);
}

static void
gtk_media_stream_ffmpeg_class_init (GtkMediaStreamFFMpegClass *klass)
{
  GtkMediaStreamClass *stream_class = GTK_MEDIA_STREAM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  stream_class->play = gtk_media_stream_ffmpeg_play;
  stream_class->pause = gtk_media_stream_ffmpeg_pause;
  stream_class->seek = gtk_media_stream_ffmpeg_seek;

  gobject_class->dispose = gtk_media_stream_ffmpeg_dispose;
}

static void
gtk_media_stream_ffmpeg_init (GtkMediaStreamFFMpeg *video)
{
  video->stream_id = -1;
}

static void
gtk_media_stream_ffmpeg_open_ffmpeg (GtkMediaStreamFFMpeg *video)
{
  AVStream *stream;
  AVCodec *codec;
  int errnum;

  errnum = avformat_open_input (&video->format_ctx, video->filename, NULL, NULL);
  if (errnum != 0)
    {
      gtk_media_stream_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }
  
  errnum = avformat_find_stream_info (video->format_ctx, NULL);
  if (errnum < 0)
    {
      gtk_media_stream_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }
  
  video->stream_id = av_find_best_stream (video->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video->stream_id < 0)
    {
      gtk_media_stream_ffmpeg_set_error (video, "File contains no video");
      return;
    }
  
  stream = video->format_ctx->streams[video->stream_id];
  codec = avcodec_find_decoder (stream->codecpar->codec_id);
  if (codec == NULL)
    {
      gtk_media_stream_ffmpeg_set_error (video, "Unsupported video codec");
      return;
    }

  video->codec_ctx = avcodec_alloc_context3 (codec);
  errnum = avcodec_parameters_to_context (video->codec_ctx, stream->codecpar);
  if (errnum < 0)
    {
      gtk_media_stream_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }
  errnum = avcodec_open2(video->codec_ctx, codec, NULL);
  if (errnum < 0)
    {
      gtk_media_stream_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }

  video->sws_ctx = sws_getContext (video->codec_ctx->width,
                                   video->codec_ctx->height,
                                   video->codec_ctx->pix_fmt,
                                   video->codec_ctx->width,
                                   video->codec_ctx->height,
                                   AV_PIX_FMT_RGB32,
                                   SWS_BILINEAR,
                                   NULL,
                                   NULL,
                                   NULL);

  gtk_media_stream_initialized (GTK_MEDIA_STREAM (video),
                                FALSE,
                                video->codec_ctx != NULL,
                                TRUE,
                                video->format_ctx->duration != AV_NOPTS_VALUE
                                ? av_rescale (video->format_ctx->duration, G_USEC_PER_SEC, AV_TIME_BASE)
                                : 0);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));

  if (gtk_media_stream_ffmpeg_decode_frame (video, &video->current_frame))
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
}

static void
gtk_media_stream_ffmpeg_open (GtkMediaStreamFFMpeg *video)
{
  if (video->filename == NULL)
    return;

  gtk_media_stream_ffmpeg_open_ffmpeg (video);
}

static void
gtk_media_stream_ffmpeg_set_filename (GtkMediaStreamFFMpeg *video,
                                      const char           *filename)
{
  gtk_media_stream_ffmpeg_clear (video);

  video->filename = g_strdup (filename);

  gtk_media_stream_ffmpeg_open (video);
}

GtkMediaStream *
gtk_media_stream_ffmpeg_new_for_filename (const char *filename)
{
  GtkMediaStreamFFMpeg *video;

  g_return_val_if_fail (filename != NULL, NULL);

  video = g_object_new (GTK_TYPE_MEDIA_STREAM_FFMPEG, NULL);

  gtk_media_stream_ffmpeg_set_filename (video, filename);

  return GTK_MEDIA_STREAM (video);
}

