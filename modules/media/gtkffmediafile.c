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
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

typedef struct _GtkVideoFrameFFMpeg GtkVideoFrameFFMpeg;

struct _GtkVideoFrameFFMpeg
{
  GdkTexture *texture;
  gint64 timestamp;
};

typedef struct _GtkFStream GtkFfStream;

struct _GtkFStream
{
  AVCodecContext *codec_ctx;
  AVStream *stream;
  int stream_id;
  int type;
};

struct _GtkFfMediaFile
{
  GtkMediaFile parent_instance;

  GFile *file;
  GInputStream *input_stream;

  AVFormatContext *device_ctx; /* used for avdevice audio playback */
  AVFormatContext *format_ctx;

  GtkFfStream *input_audio_stream;
  GtkFfStream *input_video_stream;

  GtkFfStream *output_audio_stream;

  gint64 audio_samples_count;

  // Resampling
  struct SwrContext *swr_ctx;
  AVFrame* audio_frame;

  // Rescaling
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
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (paintable);

  if (!gtk_video_frame_ffmpeg_is_empty (&self->current_frame))
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (self->current_frame.texture), snapshot, width, height);
    }
}

static GdkPaintable *
gtk_ff_media_file_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (paintable);

  if (gtk_video_frame_ffmpeg_is_empty (&self->current_frame))
    {
      if (self->input_video_stream->codec_ctx)
        return gdk_paintable_new_empty (self->input_video_stream->codec_ctx->width, self->input_video_stream->codec_ctx->height);
      else
        return gdk_paintable_new_empty (0, 0);
    }

  return GDK_PAINTABLE (g_object_ref (self->current_frame.texture));
}

static int
gtk_ff_media_file_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (paintable);

  if (self->input_video_stream->codec_ctx)
    return self->input_video_stream->codec_ctx->width;

  return 0;
}

static int
gtk_ff_media_file_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (paintable);

  if (self->input_video_stream->codec_ctx)
    return self->input_video_stream->codec_ctx->height;

  return 0;
}

static double gtk_ff_media_file_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (paintable);

  if (self->input_video_stream->codec_ctx)
    return (double) self->input_video_stream->codec_ctx->width / self->input_video_stream->codec_ctx->height;

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
gtk_ff_stream_close (GtkFfStream *stream)
{
  stream->stream_id = -1;
  g_clear_pointer (&stream->codec_ctx, avcodec_close);
  g_free (stream);
}

static void
gtk_ff_media_file_set_ffmpeg_error (GtkFfMediaFile *self,
                                    int           av_errnum)
{
  char s[AV_ERROR_MAX_STRING_SIZE];

  if (gtk_media_stream_get_error (GTK_MEDIA_STREAM (self)))
    return;

  if (av_strerror (av_errnum, s, sizeof (s) != 0))
    g_snprintf (s, sizeof (s), _("Unspecified error decoding media"));

  gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                          G_IO_ERROR,
                          G_IO_ERROR_FAILED,
                          "%s",
                          s);
}

static GtkFfStream *
gtk_ff_media_file_find_input_stream (GtkFfMediaFile *self,
                                     int type)
{
  GtkFfStream *ff_stream;
  const AVCodec *codec;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  int stream_id;
  int errnum;

  stream_id = av_find_best_stream (self->format_ctx, type, -1, -1, NULL, 0);
  if (stream_id < 0)
    {
      return NULL;
    }

  stream = self->format_ctx->streams[stream_id];
  codec = avcodec_find_decoder (stream->codecpar->codec_id);
  if (codec == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Cannot find decoder: %s"),
                              avcodec_get_name (stream->codecpar->codec_id));
      return NULL;
    }
  codec_ctx = avcodec_alloc_context3 (codec);
  if (codec_ctx == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Failed to allocate a codec context"));
      return NULL;
    }
  errnum = avcodec_parameters_to_context (codec_ctx, stream->codecpar);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      avcodec_close (codec_ctx);
      return NULL;
    }
  errnum = avcodec_open2 (codec_ctx, codec, &stream->metadata);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      avcodec_close (codec_ctx);
      return NULL;
    }

  ff_stream = g_new (GtkFfStream, 1);
  ff_stream->codec_ctx = codec_ctx;
  ff_stream->stream = stream;
  ff_stream->stream_id = stream_id;
  ff_stream->type = type;
  return ff_stream;
}

static GtkFfStream *
gtk_ff_media_file_add_output_stream (GtkFfMediaFile *self,
                                     AVFormatContext *fmt_ctx,
                                     enum AVCodecID codec_id)
{
  GtkFfStream *ff_media_stream;
  const AVCodec *codec;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  int stream_id;
  int errnum;

  // find the encoder
  codec = avcodec_find_encoder (codec_id);
  if (codec == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Cannot find encoder: %s"),
                              avcodec_get_name (codec_id));
      return NULL;
    }

  stream = avformat_new_stream (fmt_ctx, NULL);
  if (stream == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Cannot add new stream"));
      return NULL;
    }
  stream_id = fmt_ctx->nb_streams - 1;

  codec_ctx = avcodec_alloc_context3 (codec);
  if (codec_ctx == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Failed to allocate a codec context"));
      return NULL;
    }

  // set the encoder options
  codec_ctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
  codec_ctx->sample_rate = codec->supported_samplerates ? codec->supported_samplerates[0] : 48000;
  codec_ctx->channel_layout = codec->channel_layouts ? codec->channel_layouts[0] : AV_CH_LAYOUT_STEREO;
  codec_ctx->channels = av_get_channel_layout_nb_channels (codec_ctx->channel_layout);

  stream->time_base = (AVRational){ 1, codec_ctx->sample_rate };

  // open the codec
  errnum = avcodec_open2 (codec_ctx, codec, NULL);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      avcodec_close (codec_ctx);
      return NULL;
    }

  errnum = avcodec_parameters_from_context (stream->codecpar, codec_ctx);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      avcodec_close (codec_ctx);
      return NULL;
    }

  ff_media_stream = g_new (GtkFfStream, 1);
  ff_media_stream->codec_ctx = codec_ctx;
  ff_media_stream->stream = stream;
  ff_media_stream->stream_id = stream_id;
  ff_media_stream->type = AVMEDIA_TYPE_AUDIO;
  return ff_media_stream;
}

static gboolean
gtk_ff_media_file_seek_stream (GtkFfMediaFile *self, GtkFfStream *stream, int64_t timestamp)
{
  int errnum;

  if (!stream)
    return TRUE;

  errnum = av_seek_frame (self->format_ctx,
                          stream->stream_id,
                          av_rescale_q (timestamp,
                                        (AVRational){ 1, G_USEC_PER_SEC },
                                        stream->stream->time_base),
                          AVSEEK_FLAG_BACKWARD);

  if (errnum < 0)
    {
      gtk_media_stream_seek_failed (GTK_MEDIA_STREAM (self));
      return FALSE;
    }

  return TRUE;
}

static AVFrame *
gtk_ff_media_file_alloc_audio_frame (enum AVSampleFormat sample_fmt,
                                     uint64_t channel_layout,
                                     int sample_rate,
                                     int nb_samples)
{
  AVFrame *frame = av_frame_alloc ();
  int ret;

  if (!frame)
    {
      return NULL;
    }

  frame->format = sample_fmt;
  frame->channel_layout = channel_layout;
  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;

  if (nb_samples)
    {
      ret = av_frame_get_buffer (frame, 0);
      if (ret < 0)
        {
          return NULL;
        }
    }

  return frame;
}

static void
gtk_ff_media_file_write_audio_frame (GtkFfMediaFile *self, AVFrame* frame)
{
  AVFormatContext *device_ctx;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  AVFrame *resampled_frame;
  int errnum;
  int dst_nb_samples;

  device_ctx = self->device_ctx;
  codec_ctx = self->output_audio_stream->codec_ctx;
  stream = self->output_audio_stream->stream;

  if (frame)
    {
      dst_nb_samples = av_rescale_rnd (swr_get_delay (self->swr_ctx, codec_ctx->sample_rate) + frame->nb_samples,
                                       codec_ctx->sample_rate,
                                       codec_ctx->sample_rate,
                                       AV_ROUND_UP);

      resampled_frame = gtk_ff_media_file_alloc_audio_frame (codec_ctx->sample_fmt,
                                                             codec_ctx->channel_layout,
                                                             codec_ctx->sample_rate,
                                                             dst_nb_samples);
      if (resampled_frame == NULL)
        {
          gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                                  G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  _("Failed to allocate an audio frame"));
          return;
        }

      errnum = swr_convert (self->swr_ctx,
                            resampled_frame->data, dst_nb_samples,
                            (const uint8_t **) frame->data, frame->nb_samples);
      if (errnum < 0)
        {
          gtk_ff_media_file_set_ffmpeg_error (self, errnum);
          return;
        }
      frame = resampled_frame;


      frame->pts = av_rescale_q (self->audio_samples_count,
                                 (AVRational){ 1, codec_ctx->sample_rate },
                                 codec_ctx->time_base);

      errnum = av_write_uncoded_frame (device_ctx, stream->index, frame);
      if (errnum < 0)
        {
          gtk_ff_media_file_set_ffmpeg_error (self, errnum);
          return;
        }

      self->audio_samples_count += frame->nb_samples;
    }
}

static int
gtk_ff_media_file_read_packet_cb (void    *data,
                                  uint8_t *buf,
                                  int      buf_size)
{
  GtkFfMediaFile *self = data;
  GError *error = NULL;
  gssize n_read;

  n_read = g_input_stream_read (self->input_stream,
                                buf,
                                buf_size,
                                NULL,
                                &error);
  if (n_read < 0)
    {
      gtk_media_stream_gerror (GTK_MEDIA_STREAM (self), error);
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
gtk_ff_media_file_decode_frame (GtkFfMediaFile      *self,
                                GtkVideoFrameFFMpeg *result)
{
  GdkTexture *texture;
  AVPacket packet;
  AVFrame *frame;
  int errnum;
  GBytes *bytes;
  guchar *data;

  frame = av_frame_alloc ();

  for (errnum = av_read_frame (self->format_ctx, &packet);
       errnum >= 0;
       errnum = av_read_frame (self->format_ctx, &packet))
    {
      if (self->input_audio_stream && packet.stream_index == self->input_audio_stream->stream_id)
        {
          errnum = avcodec_send_packet (self->input_audio_stream->codec_ctx, &packet);
              if (errnum < 0)
              {
                gtk_ff_media_file_set_ffmpeg_error (self, errnum);
                return FALSE;
              }
          if (errnum >= 0)
            {
              errnum = avcodec_receive_frame (self->input_audio_stream->codec_ctx, self->audio_frame);
              if (errnum == AVERROR (EAGAIN))
                {
                  // Just retry with the next packet
                  errnum = 0;
                  continue;
                }
              if (errnum < 0)
                {
                  gtk_ff_media_file_set_ffmpeg_error (self, errnum);
                  return FALSE;
                }
              else
                {
                  av_packet_unref (&packet);
                }
            }

            gtk_ff_media_file_write_audio_frame(self, self->audio_frame);
        }
      else if (self->input_video_stream && packet.stream_index == self->input_video_stream->stream_id)
        {
          errnum = avcodec_send_packet (self->input_video_stream->codec_ctx, &packet);
          if (errnum < 0)
            {
              gtk_ff_media_file_set_ffmpeg_error (self, errnum);
              return FALSE;
            }
          if (errnum >= 0)
            {
              errnum = avcodec_receive_frame (self->input_video_stream->codec_ctx, frame);
              if (errnum == AVERROR (EAGAIN))
                {
                  // Just retry with the next packet
                  errnum = 0;
                  continue;
                }
              if (errnum < 0)
                {
                  gtk_ff_media_file_set_ffmpeg_error (self, errnum);
                  return FALSE;
                }
              else
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
        gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      av_frame_free (&frame);
      return FALSE;
    }

  data = g_try_malloc0 (self->input_video_stream->codec_ctx->width * self->input_video_stream->codec_ctx->height * 4);
  if (data == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_FAILED,
                              _("Not enough memory"));
      av_frame_free (&frame);
      return FALSE;
    }

  if (self->sws_ctx == NULL ||
      self->sws_pix_fmt != frame->format)
    {
      const AVPixFmtDescriptor *desc;
      enum AVPixelFormat gdk_pix_fmt;

      g_clear_pointer (&self->sws_ctx, sws_freeContext);
      self->sws_pix_fmt = frame->format;
      desc = av_pix_fmt_desc_get (self->sws_pix_fmt);
      /* Use gdk-pixbuf formats because ffmpeg can't premultiply */
      if (desc != NULL && (desc->flags & AV_PIX_FMT_FLAG_ALPHA))
        gdk_pix_fmt = AV_PIX_FMT_RGBA;
      else
        gdk_pix_fmt = AV_PIX_FMT_RGB24;

      self->sws_ctx = sws_getContext (self->input_video_stream->codec_ctx->width,
                                       self->input_video_stream->codec_ctx->height,
                                       frame->format,
                                       self->input_video_stream->codec_ctx->width,
                                       self->input_video_stream->codec_ctx->height,
                                       gdk_pix_fmt,
                                       0,
                                       NULL,
                                       NULL,
                                       NULL);

      self->memory_format = memory_format_from_pix_fmt (gdk_pix_fmt);
    }

  sws_scale(self->sws_ctx,
            (const uint8_t * const *) frame->data, frame->linesize,
            0, self->input_video_stream->codec_ctx->height,
            (uint8_t *[1]) { data }, (int[1]) { self->input_video_stream->codec_ctx->width * 4 });

  bytes = g_bytes_new_take (data, self->input_video_stream->codec_ctx->width * self->input_video_stream->codec_ctx->height * 4);
  texture = gdk_memory_texture_new (self->input_video_stream->codec_ctx->width,
                                    self->input_video_stream->codec_ctx->height,
                                    self->memory_format,
                                    bytes,
                                    self->input_video_stream->codec_ctx->width * 4);

  g_bytes_unref (bytes);

  gtk_video_frame_ffmpeg_init (result,
                               texture,
                               av_rescale_q (frame->best_effort_timestamp,
                                             self->input_video_stream->stream->time_base,
                                             (AVRational) { 1, G_USEC_PER_SEC }));

  av_frame_free (&frame);

  return TRUE;
}

static int64_t
gtk_ff_media_file_seek_cb (void    *data,
                           int64_t  offset,
                           int      whence)
{
  GtkFfMediaFile *self = data;
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

  result = g_seekable_seek (G_SEEKABLE (self->input_stream),
                            offset,
                            seek_type,
                            NULL,
                            NULL);
  if (!result)
    return -1;

  return g_seekable_tell (G_SEEKABLE (self->input_stream));
}

static gboolean
gtk_ff_media_file_create_input_stream (GtkFfMediaFile *self)
{
  GError *error = NULL;
  GFile *file;

  file = gtk_media_file_get_file (GTK_MEDIA_FILE (self));
  if (file)
    {
      self->input_stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
      if (self->input_stream == NULL)
        {
          gtk_media_stream_gerror (GTK_MEDIA_STREAM (self), error);
          g_error_free (error);
          return FALSE;
        }
    }
  else
    {
      self->input_stream = g_object_ref (gtk_media_file_get_input_stream (GTK_MEDIA_FILE (self)));
    }

  return TRUE;
}

static AVIOContext *
gtk_ff_media_file_create_io_context (GtkFfMediaFile *self)
{
  AVIOContext *result;
  int buffer_size = 4096; /* it's what everybody else uses... */
  unsigned char *buffer;

  if (!gtk_ff_media_file_create_input_stream (self))
    return NULL;

  buffer = av_malloc (buffer_size);
  if (buffer == NULL)
    return NULL;

  result = avio_alloc_context (buffer,
                               buffer_size,
                               AVIO_FLAG_READ,
                               self,
                               gtk_ff_media_file_read_packet_cb,
                               NULL,
                               G_IS_SEEKABLE (self->input_stream)
                               ? gtk_ff_media_file_seek_cb
                               : NULL);

  result->buf_ptr = result->buf_end;
  result->write_flag = 0;

  return result;
}

static gboolean
gtk_ff_media_file_init_audio_resampler (GtkFfMediaFile *self)
{
  AVCodecContext *in_codec_ctx = self->input_audio_stream->codec_ctx;
  AVCodecContext *out_codec_ctx = self->output_audio_stream->codec_ctx;
  int errnum;

  // create resampler context
  self->swr_ctx = swr_alloc ();
  if (!self->swr_ctx)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _("Could not allocate resampler context"));
      return FALSE;
    }

  // set resampler option
  av_opt_set_int (self->swr_ctx, "in_channel_count", in_codec_ctx->channels, 0);
  av_opt_set_int (self->swr_ctx, "in_sample_rate", in_codec_ctx->sample_rate, 0);
  av_opt_set_sample_fmt (self->swr_ctx, "in_sample_fmt", in_codec_ctx->sample_fmt, 0);
  av_opt_set_int (self->swr_ctx, "out_channel_count", out_codec_ctx->channels, 0);
  av_opt_set_int (self->swr_ctx, "out_sample_rate", out_codec_ctx->sample_rate, 0);
  av_opt_set_sample_fmt (self->swr_ctx, "out_sample_fmt", out_codec_ctx->sample_fmt, 0);

  // initialize the resampling context
  errnum = swr_init (self->swr_ctx);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gtk_ff_media_file_open_audio_device (GtkFfMediaFile *self)
{
  const AVOutputFormat *candidate;
  int errnum;

  /* Try finding an audio device that supports setting the volume */
  for (candidate = av_output_audio_device_next (NULL);
       candidate != NULL;
       candidate = av_output_audio_device_next (candidate))
    {
      if (candidate->control_message)
        break;
    }

  /* fallback to the first format available */
  if (candidate == NULL)
    candidate = av_output_audio_device_next (NULL);

  if (candidate == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_NOT_SUPPORTED,
                              _ ("No audio output found"));
      return FALSE;
    }

  errnum = avformat_alloc_output_context2 (&self->device_ctx, candidate, NULL, NULL);
  if (errnum != 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      return FALSE;
    }

  return TRUE;
}

static gboolean gtk_ff_media_file_play (GtkMediaStream *stream);

static void
gtk_ff_media_file_open (GtkMediaFile *file)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (file);
  int errnum;
  int nb_samples;

  self->format_ctx = avformat_alloc_context ();
  self->format_ctx->pb = gtk_ff_media_file_create_io_context (self);
  if (self->format_ctx->pb == NULL)
    {
      gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                              G_IO_ERROR,
                              G_IO_ERROR_FAILED,
                              _("Not enough memory"));
      return;
    }
  errnum = avformat_open_input (&self->format_ctx, NULL, NULL, NULL);
  if (errnum != 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      return;
    }

  errnum = avformat_find_stream_info (self->format_ctx, NULL);
  if (errnum < 0)
    {
      gtk_ff_media_file_set_ffmpeg_error (self, errnum);
      return;
    }

  self->input_audio_stream = gtk_ff_media_file_find_input_stream (self, AVMEDIA_TYPE_AUDIO);
  self->input_video_stream = gtk_ff_media_file_find_input_stream (self, AVMEDIA_TYPE_VIDEO);

  // open an audio device when we have an audio stream
  if (self->input_audio_stream && gtk_ff_media_file_open_audio_device (self))
    {
      self->output_audio_stream = gtk_ff_media_file_add_output_stream (self,
                                                                       self->device_ctx,
                                                                       self->device_ctx->oformat->audio_codec);

      gtk_ff_media_file_init_audio_resampler (self);

      if (self->output_audio_stream->codec_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000; // just taken from the ffmpeg muxing example
      else
        nb_samples = self->output_audio_stream->codec_ctx->frame_size;

      self->audio_frame = gtk_ff_media_file_alloc_audio_frame (self->output_audio_stream->codec_ctx->sample_fmt,
                                                               self->output_audio_stream->codec_ctx->channel_layout,
                                                               self->output_audio_stream->codec_ctx->sample_rate,
                                                               nb_samples);

      if (self->audio_frame == NULL)
        {
          gtk_media_stream_error (GTK_MEDIA_STREAM (self),
                                  G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  _("Failed to allocate an audio frame"));
          return;
        }

      errnum = avformat_write_header (self->device_ctx, NULL);
      if (errnum != 0)
        {
          gtk_ff_media_file_set_ffmpeg_error (self, errnum);
          return;
        }
    }

  gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (self),
                                    self->output_audio_stream != NULL,
                                    self->input_video_stream != NULL,
                                    TRUE,
                                    self->format_ctx->duration != AV_NOPTS_VALUE
                                        ? av_rescale (self->format_ctx->duration, G_USEC_PER_SEC, AV_TIME_BASE)
                                        : 0);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  if (gtk_ff_media_file_decode_frame (self, &self->current_frame))
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  if (gtk_media_stream_get_playing (GTK_MEDIA_STREAM (self)))
    gtk_ff_media_file_play (GTK_MEDIA_STREAM (self));
}

static void
gtk_ff_media_file_close (GtkMediaFile *file)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (file);

  g_clear_object (&self->input_stream);

  g_clear_pointer (&self->swr_ctx, swr_close);
  g_clear_pointer (&self->sws_ctx, sws_freeContext);
  g_clear_pointer (&self->input_audio_stream, gtk_ff_stream_close);
  g_clear_pointer (&self->input_video_stream, gtk_ff_stream_close);
  g_clear_pointer (&self->output_audio_stream, gtk_ff_stream_close);
  av_frame_free (&self->audio_frame);
  avformat_free_context(self->device_ctx);
  avformat_close_input (&self->format_ctx);
  gtk_video_frame_ffmpeg_clear (&self->next_frame);
  gtk_video_frame_ffmpeg_clear (&self->current_frame);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static gboolean
gtk_ff_media_file_next_frame_cb (gpointer data);
static void
gtk_ff_media_file_queue_frame (GtkFfMediaFile *self)
{
  gint64 time, frame_time;
  guint delay;

  time = g_get_monotonic_time ();
  frame_time = self->start_time + self->next_frame.timestamp;
  delay = time > frame_time ? 0 : (frame_time - time) / 1000;

  self->next_frame_cb = g_timeout_add (delay, gtk_ff_media_file_next_frame_cb, self);
}

static gboolean
gtk_ff_media_file_restart (GtkFfMediaFile *self)
{
  if (!gtk_ff_media_file_seek_stream (self, self->input_audio_stream, 0))
    return FALSE;
  if (!gtk_ff_media_file_seek_stream (self, self->input_video_stream, 0))
    return FALSE;

  if (!gtk_ff_media_file_decode_frame (self, &self->next_frame))
    return FALSE;

  return TRUE;
}

static gboolean
gtk_ff_media_file_next_frame_cb (gpointer data)
{
  GtkFfMediaFile *self = data;

  self->next_frame_cb = 0;

  if (gtk_video_frame_ffmpeg_is_empty (&self->next_frame))
    {
      if (!gtk_media_stream_get_loop (GTK_MEDIA_STREAM (self)) ||
          !gtk_ff_media_file_restart (self))
        {
          gtk_media_stream_stream_ended (GTK_MEDIA_STREAM (self));
          return G_SOURCE_REMOVE;
        }

      self->start_time += self->current_frame.timestamp - self->next_frame.timestamp;
    }

  gtk_video_frame_ffmpeg_clear (&self->current_frame);
  gtk_video_frame_ffmpeg_move (&self->current_frame,
                               &self->next_frame);

  gtk_media_stream_update (GTK_MEDIA_STREAM (self),
                           self->current_frame.timestamp);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  /* ignore failure here, we'll handle the empty frame case above
   * the next time we're called. */
  gtk_ff_media_file_decode_frame (self, &self->next_frame);
  gtk_ff_media_file_queue_frame (self);

  return G_SOURCE_REMOVE;
}

static gboolean
gtk_ff_media_file_play (GtkMediaStream *stream)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (stream);

  if (self->format_ctx == NULL)
    return FALSE;

  if (!gtk_media_stream_is_prepared (stream))
    return TRUE;

  if (gtk_video_frame_ffmpeg_is_empty (&self->next_frame) &&
      !gtk_ff_media_file_decode_frame (self, &self->next_frame))
    {
      if (gtk_ff_media_file_restart (self))
        {
          self->start_time = g_get_monotonic_time () - self->next_frame.timestamp;
        }
      else
        {
          return FALSE;
        }
    }
  else
    {
      self->start_time = g_get_monotonic_time () - self->current_frame.timestamp;
    }

  gtk_ff_media_file_queue_frame (self);

  return TRUE;
}

static void
gtk_ff_media_file_pause (GtkMediaStream *stream)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (stream);

  if (self->next_frame_cb)
    {
      g_source_remove (self->next_frame_cb);
      self->next_frame_cb = 0;
    }

  self->start_time = 0;
}

static void
gtk_ff_media_file_seek (GtkMediaStream *stream,
                        gint64          timestamp)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (stream);

  if (!gtk_ff_media_file_seek_stream (self, self->input_audio_stream, timestamp))
    return;
  if (!gtk_ff_media_file_seek_stream (self, self->input_video_stream, timestamp))
    return;

  gtk_media_stream_seek_success (stream);

  gtk_video_frame_ffmpeg_clear (&self->next_frame);
  gtk_video_frame_ffmpeg_clear (&self->current_frame);
  if (gtk_ff_media_file_decode_frame (self, &self->current_frame))
    gtk_media_stream_update (stream, self->current_frame.timestamp);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  if (gtk_media_stream_get_playing (stream))
    {
      gtk_ff_media_file_pause (stream);
      if (!gtk_ff_media_file_play (stream))
        gtk_media_stream_stream_ended (stream);
    }
}

static void
gtk_ff_media_file_update_audio (GtkMediaStream *stream,
                                gboolean muted,
                                double volume)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (stream);
  int errnum;

  errnum = avdevice_app_to_dev_control_message (self->device_ctx, muted ? AV_APP_TO_DEV_MUTE : AV_APP_TO_DEV_UNMUTE, NULL, 0);
  if (errnum < 0)
    {
      g_warning ("Cannot set audio mute state");
    }

  errnum = avdevice_app_to_dev_control_message (self->device_ctx, AV_APP_TO_DEV_SET_VOLUME, &volume, sizeof (volume));
  if (errnum < 0)
    {
      g_warning ("Cannot set audio volume");
    }
}

static void
gtk_ff_media_file_dispose (GObject *object)
{
  GtkFfMediaFile *self = GTK_FF_MEDIA_FILE (object);

  gtk_ff_media_file_pause (GTK_MEDIA_STREAM (self));
  gtk_ff_media_file_close (GTK_MEDIA_FILE (self));

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
  stream_class->update_audio = gtk_ff_media_file_update_audio;

  gobject_class->dispose = gtk_ff_media_file_dispose;
}

static void
gtk_ff_media_file_init (GtkFfMediaFile *self)
{
}


