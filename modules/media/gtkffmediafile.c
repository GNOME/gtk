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

#include "gtkffmediafileprivate.h"

#include "gtkintl.h"

#include "gdk/gdkmemorytextureprivate.h"

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

struct _GtkFfMediaFile
{
  GtkMediaFile parent_instance;

  GFile *file;
  GInputStream *input_stream;

  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  int stream_id;
  struct SwsContext *sws_ctx;
  enum AVPixelFormat sws_pix_fmt;
  GdkMemoryFormat memory_format;

  GtkVideoFrameFFMpeg current_frame;
  GtkVideoFrameFFMpeg next_frame;

  gint64 start_time; /* monotonic time when we displayed the last frame */
  guint next_frame_cb; /* Source ID of next frame callback */
};

struct _GtkFfMediaFileClass
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
gtk_ff_media_file_paintable_snapshot (GdkPaintable *paintable,
                                      GdkSnapshot  *snapshot,
                                      double        width,
                                      double        height)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (paintable);

  if (!gtk_video_frame_ffmpeg_is_empty (&video->current_frame))
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (video->current_frame.texture), snapshot, width, height);
    }
}

static GdkPaintable *
gtk_ff_media_file_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (paintable);

  if (gtk_video_frame_ffmpeg_is_empty (&video->current_frame))
    {
      if (video->codec_ctx)
        return gdk_paintable_new_empty (video->codec_ctx->width, video->codec_ctx->height);
      else
        return gdk_paintable_new_empty (0, 0);
    }

  return GDK_PAINTABLE (g_object_ref (video->current_frame.texture));
}

static int
gtk_ff_media_file_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->width;

  return 0;
}

static int
gtk_ff_media_file_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->height;

  return 0;
}

static double gtk_ff_media_file_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (paintable);

  if (video->codec_ctx)
    return (double) video->codec_ctx->width / video->codec_ctx->height;

  return 0.0;
};

static void
gtk_ff_media_file_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_ff_media_file_paintable_snapshot;
  iface->get_current_image = gtk_ff_media_file_paintable_get_current_image;
  iface->get_intrinsic_width = gtk_ff_media_file_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_ff_media_file_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_ff_media_file_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (GtkFfMediaFile, gtk_ff_media_file, GTK_TYPE_MEDIA_FILE, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_ff_media_file_paintable_init))

G_MODULE_EXPORT
void
g_io_module_load (GIOModule *module)
{
  g_type_module_use (G_TYPE_MODULE (module));

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT (58, 9, 100)
  av_register_all ();
#endif

  g_io_extension_point_implement (GTK_MEDIA_FILE_EXTENSION_POINT_NAME,
                                  GTK_TYPE_FF_MEDIA_FILE,
                                  "ffmpeg",
                                  0);
}

G_MODULE_EXPORT
G_GNUC_NORETURN
void
g_io_module_unload (GIOModule *module)
{
  g_assert_not_reached ();
}

G_MODULE_EXPORT
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
gtk_ff_media_file_set_ffmpeg_error (GtkFfMediaFile *video,
                                    int           av_errnum)
{
  char s[AV_ERROR_MAX_STRING_SIZE];

  if (gtk_media_stream_get_error (GTK_MEDIA_STREAM (video)))
    return;

  if (av_strerror (av_errnum, s, sizeof (s) != 0))
    g_snprintf (s, sizeof (s), _("Unspecified error decoding video"));

  gtk_media_stream_error (GTK_MEDIA_STREAM (video),
                          G_IO_ERROR,
                          G_IO_ERROR_FAILED,
                          "%s",
                          s);
}

static int
gtk_ff_media_file_read_packet_cb (void    *data,
                                  uint8_t *buf,
                                  int      buf_size)
{
  GtkFfMediaFile *video = data;
  GError *error = NULL;
  gssize n_read;

  n_read = g_input_stream_read (video->input_stream,
                                buf,
                                buf_size,
                                NULL,
                                &error);
  if (n_read < 0)
    {
      gtk_media_stream_gerror (GTK_MEDIA_STREAM (video), error);
    }
  else if (n_read == 0)
    {
      n_read = AVERROR_EOF;
    }

  return n_read;
}

static GdkMemoryFormat
memory_format_from_pix_fmt (enum AVPixelFormat pix_fmt)
{
  switch ((int) pix_fmt)
    {
    case AV_PIX_FMT_RGBA:
      return GDK_MEMORY_R8G8B8A8;
    case AV_PIX_FMT_RGB24:
      return GDK_MEMORY_R8G8B8;
    default:
      g_assert_not_reached ();
      return GDK_MEMORY_R8G8B8A8;
    }
}

static gboolean
gtk_ff_media_file_decode_frame (GtkFfMediaFile      *video,
                                GtkVideoFrameFFMpeg *result)
{
  GdkTexture *texture;
  AVPacket packet;
  AVFrame *frame;
  int errnum;
  GBytes *bytes;
  guchar *data;

  frame = av_frame_alloc ();

  for (errnum = av_read_frame (video->format_ctx, &packet);
       errnum >= 0;
       errnum = av_read_frame (video->format_ctx, &packet))
    {
      if (packet.stream_index == video->stream_id)
        {
          errnum = avcodec_send_packet (video->codec_ctx, &packet);
              if (errnum < 0)
                G_BREAKPOINT();
          if (errnum >= 0)
            {
              errnum = avcodec_receive_frame (video->codec_ctx, frame);
              if (errnum < 0)
                G_BREAKPOINT();
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
      if (errnum != AVERROR_EOF)
        gtk_ff_media_file_set_ffmpeg_error (video, errnum);
      av_frame_free (&frame);
      return FALSE;
    }

  data = g_try_malloc0 (video->codec_ctx->width * video->codec_ctx->height * 4);
  if (data == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (video),
                              G_IO_ERROR,
                              G_IO_ERROR_FAILED,
                              _("Not enough memory"));
      av_frame_free (&frame);
      return FALSE;
    }

  if (video->sws_ctx == NULL ||
      video->sws_pix_fmt != frame->format)
    {
      const AVPixFmtDescriptor *desc;
      enum AVPixelFormat gdk_pix_fmt;

      g_clear_pointer (&video->sws_ctx, sws_freeContext);
      video->sws_pix_fmt = frame->format;
      desc = av_pix_fmt_desc_get (video->sws_pix_fmt);
      /* Use gdk-pixbuf formats because ffmpeg can't premultiply */
      if (desc != NULL && (desc->flags & AV_PIX_FMT_FLAG_ALPHA))
        gdk_pix_fmt = AV_PIX_FMT_RGBA;
      else
        gdk_pix_fmt = AV_PIX_FMT_RGB24;

      video->sws_ctx = sws_getContext (video->codec_ctx->width,
                                       video->codec_ctx->height,
                                       frame->format,
                                       video->codec_ctx->width,
                                       video->codec_ctx->height,
                                       gdk_pix_fmt,
                                       0,
                                       NULL,
                                       NULL,
                                       NULL);

      video->memory_format = memory_format_from_pix_fmt (gdk_pix_fmt);
    }

  sws_scale(video->sws_ctx,
            (const uint8_t * const *) frame->data, frame->linesize,
            0, video->codec_ctx->height,
            (uint8_t *[1]) { data }, (int[1]) { video->codec_ctx->width * 4 });

  bytes = g_bytes_new_take (data, video->codec_ctx->width * video->codec_ctx->height * 4);
  texture = gdk_memory_texture_new (video->codec_ctx->width,
                                    video->codec_ctx->height,
                                    video->memory_format,
                                    bytes,
                                    video->codec_ctx->width * 4);

  g_bytes_unref (bytes);

  gtk_video_frame_ffmpeg_init (result,
                               texture,
                               av_rescale_q (frame->best_effort_timestamp,
                                             video->format_ctx->streams[video->stream_id]->time_base,
                                             (AVRational) { 1, G_USEC_PER_SEC }));

  av_frame_free (&frame);

  return TRUE;
}

static int64_t
gtk_ff_media_file_seek_cb (void    *data,
                           int64_t  offset,
                           int      whence)
{
  GtkFfMediaFile *video = data;
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
gtk_ff_media_file_create_input_stream (GtkFfMediaFile *video)
{
  GError *error = NULL;
  GFile *file;

  file = gtk_media_file_get_file (GTK_MEDIA_FILE (video));
  if (file)
    {
      video->input_stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
      if (video->input_stream == NULL)
        {
          gtk_media_stream_gerror (GTK_MEDIA_STREAM (video), error);
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
gtk_ff_media_file_create_io_context (GtkFfMediaFile *video)
{
  AVIOContext *result;
  int buffer_size = 4096; /* it's what everybody else uses... */
  unsigned char *buffer;

  if (!gtk_ff_media_file_create_input_stream (video))
    return NULL;

  buffer = av_malloc (buffer_size);
  if (buffer == NULL)
    return NULL;

  result = avio_alloc_context (buffer,
                               buffer_size,
                               AVIO_FLAG_READ,
                               video,
                               gtk_ff_media_file_read_packet_cb,
                               NULL,
                               G_IS_SEEKABLE (video->input_stream)
                               ? gtk_ff_media_file_seek_cb
                               : NULL);

  result->buf_ptr = result->buf_end;
  result->write_flag = 0;

  return result;
}

static gboolean gtk_ff_media_file_play (GtkMediaStream *stream);

static void
gtk_ff_media_file_open (GtkMediaFile *file)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (file);
  AVStream *stream;
  const AVCodec *codec;
  int errnum;

  video->format_ctx = avformat_alloc_context ();
  video->format_ctx->pb = gtk_ff_media_file_create_io_context (video);
  if (video->format_ctx->pb == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (video),
                              G_IO_ERROR,
                              G_IO_ERROR_FAILED,
                              _("Not enough memory"));
      return;
    }
  errnum = avformat_open_input (&video->format_ctx, NULL, NULL, NULL);
  if (errnum != 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (video, errnum);
      return;
    }

  errnum = avformat_find_stream_info (video->format_ctx, NULL);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (video, errnum);
      return;
    }

  video->stream_id = av_find_best_stream (video->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video->stream_id < 0)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (video),
                              G_IO_ERROR,
                              G_IO_ERROR_INVALID_DATA,
                              _("Not a video file"));
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
      gtk_media_stream_error (GTK_MEDIA_STREAM (video),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Unsupported video codec"));
      return;
    }

  video->codec_ctx = avcodec_alloc_context3 (codec);
  errnum = avcodec_parameters_to_context (video->codec_ctx, stream->codecpar);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (video, errnum);
      return;
    }
  errnum = avcodec_open2 (video->codec_ctx, codec, &stream->metadata);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (video, errnum);
      return;
    }

  gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (video),
                                    FALSE,
                                    video->codec_ctx != NULL,
                                    TRUE,
                                    video->format_ctx->duration != AV_NOPTS_VALUE
                                      ? av_rescale (video->format_ctx->duration, G_USEC_PER_SEC, AV_TIME_BASE)
                                      : 0);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));

  if (gtk_ff_media_file_decode_frame (video, &video->current_frame))
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));

  if (gtk_media_stream_get_playing (GTK_MEDIA_STREAM (video)))
    gtk_ff_media_file_play (GTK_MEDIA_STREAM (video));
}

static void
gtk_ff_media_file_close (GtkMediaFile *file)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (file);

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
gtk_ff_media_file_next_frame_cb (gpointer data);
static void
gtk_ff_media_file_queue_frame (GtkFfMediaFile *video)
{
  gint64 time, frame_time;
  guint delay;

  time = g_get_monotonic_time ();
  frame_time = video->start_time + video->next_frame.timestamp;
  delay = time > frame_time ? 0 : (frame_time - time) / 1000;

  video->next_frame_cb = g_timeout_add (delay, gtk_ff_media_file_next_frame_cb, video);
}

static gboolean
gtk_ff_media_file_restart (GtkFfMediaFile *video)
{
  if (av_seek_frame (video->format_ctx,
                     video->stream_id,
                     av_rescale_q (0,
                                   (AVRational) { 1, G_USEC_PER_SEC },
                                   video->format_ctx->streams[video->stream_id]->time_base),
                     AVSEEK_FLAG_BACKWARD) < 0)
    return FALSE;

  if (!gtk_ff_media_file_decode_frame (video, &video->next_frame))
    return FALSE;

  return TRUE;
}

static gboolean
gtk_ff_media_file_next_frame_cb (gpointer data)
{
  GtkFfMediaFile *video = data;

  video->next_frame_cb = 0;

  if (gtk_video_frame_ffmpeg_is_empty (&video->next_frame))
    {
      if (!gtk_media_stream_get_loop (GTK_MEDIA_STREAM (video)) ||
          !gtk_ff_media_file_restart (video))
        {
          gtk_media_stream_stream_ended (GTK_MEDIA_STREAM (video));
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
  gtk_ff_media_file_decode_frame (video, &video->next_frame);
  gtk_ff_media_file_queue_frame (video);

  return G_SOURCE_REMOVE;
}

static gboolean
gtk_ff_media_file_play (GtkMediaStream *stream)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (stream);

  if (video->format_ctx == NULL)
    return FALSE;

  if (!gtk_media_stream_is_prepared (stream))
    return TRUE;

  if (gtk_video_frame_ffmpeg_is_empty (&video->next_frame) &&
      !gtk_ff_media_file_decode_frame (video, &video->next_frame))
    {
      if (gtk_ff_media_file_restart (video))
        {
          video->start_time = g_get_monotonic_time () - video->next_frame.timestamp;
        }
      else
        {
          return FALSE;
        }
    }
  else
    {
      video->start_time = g_get_monotonic_time () - video->current_frame.timestamp;
    }

  gtk_ff_media_file_queue_frame (video);

  return TRUE;
}

static void
gtk_ff_media_file_pause (GtkMediaStream *stream)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (stream);

  if (video->next_frame_cb)
    {
      g_source_remove (video->next_frame_cb);
      video->next_frame_cb = 0;
    }

  video->start_time = 0;
}

static void
gtk_ff_media_file_seek (GtkMediaStream *stream,
                        gint64          timestamp)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (stream);
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
  if (gtk_ff_media_file_decode_frame (video, &video->current_frame))
    gtk_media_stream_update (stream, video->current_frame.timestamp);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));

  if (gtk_media_stream_get_playing (stream))
    {
      gtk_ff_media_file_pause (stream);
      if (!gtk_ff_media_file_play (stream))
        gtk_media_stream_stream_ended (stream);
    }
}

static void
gtk_ff_media_file_dispose (GObject *object)
{
  GtkFfMediaFile *video = GTK_FF_MEDIA_FILE (object);

  gtk_ff_media_file_pause (GTK_MEDIA_STREAM (video));
  gtk_ff_media_file_close (GTK_MEDIA_FILE (video));

  G_OBJECT_CLASS (gtk_ff_media_file_parent_class)->dispose (object);
}

static void
gtk_ff_media_file_class_init (GtkFfMediaFileClass *klass)
{
  GtkMediaFileClass *file_class = GTK_MEDIA_FILE_CLASS (klass);
  GtkMediaStreamClass *stream_class = GTK_MEDIA_STREAM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  file_class->open = gtk_ff_media_file_open;
  file_class->close = gtk_ff_media_file_close;
  stream_class->play = gtk_ff_media_file_play;
  stream_class->pause = gtk_ff_media_file_pause;
  stream_class->seek = gtk_ff_media_file_seek;

  gobject_class->dispose = gtk_ff_media_file_dispose;
}

static void
gtk_ff_media_file_init (GtkFfMediaFile *video)
{
  video->stream_id = -1;
}


