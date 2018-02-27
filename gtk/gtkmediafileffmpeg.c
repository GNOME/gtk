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

#include "gtkmediafileffmpegprivate.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

typedef struct _GtkVideoFrameFFMpeg GtkVideoFrameFFMpeg;

struct _GtkVideoFrameFFMpeg
{
  GdkTexture *texture;
  gint64 timestamp;
};

struct _GtkMediaFileFFMpeg
{
  GtkMediaFile parent_instance;

  GFile *file;
  GInputStream *input_stream;

  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  int stream_id;
  struct SwsContext *sws_ctx;
  enum AVPixelFormat sws_pix_fmt;
  gboolean sws_premultiply;

  GtkVideoFrameFFMpeg current_frame;
  GtkVideoFrameFFMpeg next_frame;

  gint64 start_time; /* monotonic time when we displayed the last frame */
  guint next_frame_cb; /* Source ID of next frame callback */
};

struct _GtkMediaFileFFMpegClass
{
  GtkMediaFileClass parent_class;
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
gtk_media_file_ffmpeg_paintable_snapshot (GdkPaintable *paintable,
                                            GdkSnapshot  *snapshot,
                                            double        width,
                                            double        height)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (paintable);

  if (!gtk_video_frame_ffmpeg_is_empty (&video->current_frame))
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (video->current_frame.texture), snapshot, width, height);
    }
}

static GdkPaintable *
gtk_media_file_ffmpeg_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (paintable);

  return GDK_PAINTABLE (g_object_ref (video->current_frame.texture));
}

static int
gtk_media_file_ffmpeg_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->width;

  return 0;
}

static int
gtk_media_file_ffmpeg_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->height;

  return 0;
}

static double gtk_media_file_ffmpeg_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (paintable);

  if (video->codec_ctx)
    return (double) video->codec_ctx->width / video->codec_ctx->height;

  return 0.0;
};

static void
gtk_media_file_ffmpeg_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_media_file_ffmpeg_paintable_snapshot;
  iface->get_current_image = gtk_media_file_ffmpeg_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_media_file_ffmpeg_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_media_file_ffmpeg_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_media_file_ffmpeg_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_WITH_CODE (GtkMediaFileFFMpeg, gtk_media_file_ffmpeg, GTK_TYPE_MEDIA_FILE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_media_file_ffmpeg_paintable_init)
                         av_register_all ();
                         g_io_extension_point_implement (GTK_MEDIA_FILE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "ffmpeg",
                                                         0);)

static void
gtk_media_file_ffmpeg_set_error (GtkMediaFileFFMpeg *video,
                          const char   *error_message)
{
}

static void
gtk_media_file_ffmpeg_set_ffmpeg_error (GtkMediaFileFFMpeg *video,
                                          int           av_errnum)
{
  char s[AV_ERROR_MAX_STRING_SIZE];

  if (av_strerror (av_errnum, s, sizeof (s) != 0))
    snprintf (s, sizeof (s), "Unspecified error decoding video");

  gtk_media_file_ffmpeg_set_error (video, s);
}

static int
gtk_media_file_ffmpeg_read_packet_cb (void    *data,
                                      uint8_t *buf,
                                      int      buf_size)
{
  GtkMediaFileFFMpeg *video = data;
  gssize n_read;

  n_read = g_input_stream_read (video->input_stream,
                                buf,
                                buf_size,
                                NULL,
                                NULL);

  if (n_read == 0)
    return AVERROR_EOF;

  return n_read;
}

static inline int
multiply_alpha (int alpha, int color)
{
    int temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

static gboolean
gtk_media_file_ffmpeg_decode_frame (GtkMediaFileFFMpeg  *video,
                                    GtkVideoFrameFFMpeg *result)
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
      gtk_media_file_ffmpeg_set_ffmpeg_error (video, errnum);
      av_frame_free (&frame);
      return FALSE;
    }
  
  data = g_try_malloc0 (video->codec_ctx->width * video->codec_ctx->height * 4);
  if (data == NULL)
    {
      gtk_media_file_ffmpeg_set_error (video, "Not enough memory");
      av_frame_free (&frame);
      return FALSE;
    }

  if (video->sws_ctx == NULL ||
      video->sws_pix_fmt != frame->format)
    {
      const AVPixFmtDescriptor *desc;

      g_clear_pointer (&video->sws_ctx, sws_freeContext);

      video->sws_ctx = sws_getContext (video->codec_ctx->width,
                                       video->codec_ctx->height,
                                       frame->format,
                                       video->codec_ctx->width,
                                       video->codec_ctx->height,
                                       AV_PIX_FMT_RGB32,
                                       0,
                                       NULL,
                                       NULL,
                                       NULL);
      video->sws_pix_fmt = frame->format;
      desc = av_pix_fmt_desc_get (video->sws_pix_fmt);
      video->sws_premultiply = desc != NULL && (desc->flags & AV_PIX_FMT_FLAG_ALPHA);
    }

  sws_scale(video->sws_ctx,
            (const uint8_t * const *) frame->data, frame->linesize,
            0, video->codec_ctx->height,
            (uint8_t *[1]) { data }, (int[1]) { video->codec_ctx->width * 4 });
  if (video->sws_premultiply)
    {
      gsize i;

      for (i = 0; i < video->codec_ctx->width * video->codec_ctx->height; i++)
        {
          data[4 * i + 0] = multiply_alpha (data[4 * i + 3], data[4 * i + 0]);
          data[4 * i + 1] = multiply_alpha (data[4 * i + 3], data[4 * i + 1]);
          data[4 * i + 2] = multiply_alpha (data[4 * i + 3], data[4 * i + 2]);
        }
    }
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

static int64_t
gtk_media_file_ffmpeg_seek_cb (void    *data,
                               int64_t  offset,
                               int      whence)
{
  GtkMediaFileFFMpeg *video = data;
  GSeekType seek_type;
  gboolean result;

  switch (whence)
    {
    case SEEK_SET:
      seek_type = G_SEEK_SET;
      break;

    case SEEK_CUR:
      seek_type = G_SEEK_CUR;
      break;

    case SEEK_END:
      seek_type = G_SEEK_END;
      break;

    case AVSEEK_SIZE:
      /* FIXME: Handle size querying */
      return -1;

    default:
      g_assert_not_reached ();
      return -1;
    }

  result = g_seekable_seek (G_SEEKABLE (video->input_stream),
                            offset,
                            seek_type,
                            NULL,
                            NULL);
  if (!result)
    return -1;

  return g_seekable_tell (G_SEEKABLE (video->input_stream));
}

static gboolean
gtk_media_file_ffmpeg_create_input_stream (GtkMediaFileFFMpeg *video)
{
  GError *error = NULL;
  GFile *file;

  file = gtk_media_file_get_file (GTK_MEDIA_FILE (video));
  if (file)
    {
      video->input_stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
      if (video->input_stream == NULL)
        {
          gtk_media_file_ffmpeg_set_error (video, error->message);
          g_error_free (error);
          return FALSE;
        }
    }
  else
    {
      video->input_stream = g_object_ref (gtk_media_file_get_input_stream (GTK_MEDIA_FILE (video)));
    }

  return TRUE;
}

static AVIOContext *
gtk_media_file_ffmpeg_create_io_context (GtkMediaFileFFMpeg *video)
{
  AVIOContext *result;
  int buffer_size = 4096; /* it's what everybody else uses... */
  unsigned char *buffer;

  if (!gtk_media_file_ffmpeg_create_input_stream (video))
    return NULL;

  buffer = av_malloc (buffer_size);
  if (buffer == NULL)
    return NULL;

  result = avio_alloc_context (buffer,
                               buffer_size,
                               AVIO_FLAG_READ,
                               video,
                               gtk_media_file_ffmpeg_read_packet_cb,
                               NULL,
                               G_IS_SEEKABLE (video->input_stream)
                               ? gtk_media_file_ffmpeg_seek_cb
                               : NULL);

  result->buf_ptr = result->buf_end;
  result->write_flag = 0;

  return result;
}

static void
gtk_media_file_ffmpeg_open (GtkMediaFile *file)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (file);
  AVStream *stream;
  AVCodec *codec;
  int errnum;

  video->format_ctx = avformat_alloc_context ();
  video->format_ctx->pb = gtk_media_file_ffmpeg_create_io_context (video);
  if (video->format_ctx->pb == NULL)
    {
      gtk_media_file_ffmpeg_set_error (video, "Not enough memory");
      return;
    }
  errnum = avformat_open_input (&video->format_ctx, NULL, NULL, NULL);
  if (errnum != 0)
    {
      gtk_media_file_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }
  
  errnum = avformat_find_stream_info (video->format_ctx, NULL);
  if (errnum < 0)
    {
      gtk_media_file_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }
  
  video->stream_id = av_find_best_stream (video->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video->stream_id < 0)
    {
      gtk_media_file_ffmpeg_set_error (video, "File contains no video");
      return;
    }
  
  stream = video->format_ctx->streams[video->stream_id];
  /* alpha transparency requires the libvpx codecs, not the ffmpeg builtin ones */
  if (stream->codecpar->codec_id == AV_CODEC_ID_VP8)
    codec = avcodec_find_decoder_by_name ("libvpx");
  else if (stream->codecpar->codec_id == AV_CODEC_ID_VP9)
    codec = avcodec_find_decoder_by_name ("libvpx-vp9");
  else
    codec = NULL;
  if (codec == NULL)
    codec = avcodec_find_decoder (stream->codecpar->codec_id);
  if (codec == NULL)
    {
      gtk_media_file_ffmpeg_set_error (video, "Unsupported video codec");
      return;
    }

  video->codec_ctx = avcodec_alloc_context3 (codec);
  errnum = avcodec_parameters_to_context (video->codec_ctx, stream->codecpar);
  if (errnum < 0)
    {
      gtk_media_file_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }
  errnum = avcodec_open2 (video->codec_ctx, codec, &stream->metadata);
  if (errnum < 0)
    {
      gtk_media_file_ffmpeg_set_ffmpeg_error (video, errnum);
      return;
    }

  gtk_media_stream_prepared (GTK_MEDIA_STREAM (video),
                             FALSE,
                             video->codec_ctx != NULL,
                             TRUE,
                             video->format_ctx->duration != AV_NOPTS_VALUE
                             ? av_rescale (video->format_ctx->duration, G_USEC_PER_SEC, AV_TIME_BASE)
                             : 0);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));

  if (gtk_media_file_ffmpeg_decode_frame (video, &video->current_frame))
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
}

static void 
gtk_media_file_ffmpeg_close (GtkMediaFile *file)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (file);

  g_clear_object (&video->input_stream);

  g_clear_pointer (&video->sws_ctx, sws_freeContext);
  g_clear_pointer (&video->codec_ctx, avcodec_close);
  avformat_close_input (&video->format_ctx);
  video->stream_id = -1;
  gtk_video_frame_ffmpeg_clear (&video->next_frame);
  gtk_video_frame_ffmpeg_clear (&video->current_frame);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
}

static gboolean
gtk_media_file_ffmpeg_next_frame_cb (gpointer data);
static void
gtk_media_file_ffmpeg_queue_frame (GtkMediaFileFFMpeg *video)
{
  gint64 time, frame_time;
  guint delay;
  
  time = g_get_monotonic_time ();
  frame_time = video->start_time + video->next_frame.timestamp;
  delay = time > frame_time ? 0 : (frame_time - time) / 1000;

  video->next_frame_cb = g_timeout_add (delay, gtk_media_file_ffmpeg_next_frame_cb, video);
}

static gboolean
gtk_media_file_ffmpeg_next_frame_cb (gpointer data)
{
  GtkMediaFileFFMpeg *video = data;
  
  video->next_frame_cb = 0;

  if (gtk_video_frame_ffmpeg_is_empty (&video->next_frame))
    {
      if (!gtk_media_stream_get_loop (GTK_MEDIA_STREAM (video)) ||
          av_seek_frame (video->format_ctx,
                         video->stream_id,
                         av_rescale_q (0,
                                       (AVRational) { 1, G_USEC_PER_SEC },
                                       video->format_ctx->streams[video->stream_id]->time_base),
                         AVSEEK_FLAG_BACKWARD) < 0 ||
          !gtk_media_file_ffmpeg_decode_frame (video, &video->next_frame))
        {
          gtk_media_stream_ended (GTK_MEDIA_STREAM (video));
          return G_SOURCE_REMOVE;
        }

      video->start_time += video->current_frame.timestamp - video->next_frame.timestamp;
    }

  gtk_video_frame_ffmpeg_clear (&video->current_frame);
  gtk_video_frame_ffmpeg_move (&video->current_frame,
                               &video->next_frame);

  gtk_media_stream_update (GTK_MEDIA_STREAM (video),
                           video->current_frame.timestamp);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));

  /* ignore failure here, we'll handle the empty frame case above
   * the next time we're called. */
  gtk_media_file_ffmpeg_decode_frame (video, &video->next_frame);
  gtk_media_file_ffmpeg_queue_frame (video);

  return G_SOURCE_REMOVE;
}

static gboolean
gtk_media_file_ffmpeg_play (GtkMediaStream *stream)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (stream);

  if (gtk_video_frame_ffmpeg_is_empty (&video->next_frame) &&
      !gtk_media_file_ffmpeg_decode_frame (video, &video->next_frame))
    return FALSE;

  video->start_time = g_get_monotonic_time () - video->current_frame.timestamp;

  gtk_media_file_ffmpeg_queue_frame (video);

  return TRUE;
}

static void
gtk_media_file_ffmpeg_pause (GtkMediaStream *stream)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (stream);

  if (video->next_frame_cb)
    {
      g_source_remove (video->next_frame_cb);
      video->next_frame_cb = 0;
    }

  video->start_time = 0;
}

static void
gtk_media_file_ffmpeg_seek (GtkMediaStream *stream,
                            gint64          timestamp)                           
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (stream);
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
  if (gtk_media_file_ffmpeg_decode_frame (video, &video->current_frame))
    gtk_media_stream_update (stream, video->current_frame.timestamp);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));

  if (gtk_media_stream_get_playing (stream))
    {
      gtk_media_file_ffmpeg_pause (stream);
      if (!gtk_media_file_ffmpeg_play (stream))
        gtk_media_stream_ended (stream);
    }
}

static void
gtk_media_file_ffmpeg_dispose (GObject *object)
{
  GtkMediaFileFFMpeg *video = GTK_MEDIA_FILE_FFMPEG (object);

  gtk_media_file_ffmpeg_pause (GTK_MEDIA_STREAM (video));
  gtk_media_file_ffmpeg_close (GTK_MEDIA_FILE (video));

  G_OBJECT_CLASS (gtk_media_file_ffmpeg_parent_class)->dispose (object);
}

static void
gtk_media_file_ffmpeg_class_init (GtkMediaFileFFMpegClass *klass)
{
  GtkMediaFileClass *file_class = GTK_MEDIA_FILE_CLASS (klass);
  GtkMediaStreamClass *stream_class = GTK_MEDIA_STREAM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  file_class->open = gtk_media_file_ffmpeg_open;
  file_class->close = gtk_media_file_ffmpeg_close;
  stream_class->play = gtk_media_file_ffmpeg_play;
  stream_class->pause = gtk_media_file_ffmpeg_pause;
  stream_class->seek = gtk_media_file_ffmpeg_seek;

  gobject_class->dispose = gtk_media_file_ffmpeg_dispose;
}

static void
gtk_media_file_ffmpeg_init (GtkMediaFileFFMpeg *video)
{
  video->stream_id = -1;
}

