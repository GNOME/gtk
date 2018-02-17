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

#include "gtkdemovideo.h"

#ifdef HAVE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

enum {
  PROP_0,
  PROP_FILENAME,
  PROP_PLAYING,

  N_PROPS
};

struct _GtkDemoVideo {
  GObject parent_instance;

  char *filename;

#ifdef HAVE_FFMPEG
  AVFormatContext *format_ctx;
  AVCodecContext *codec_ctx;
  int stream_id;
  struct SwsContext *sws_ctx;
#endif
  GdkTexture *current_texture;
  gint64 current_texture_duration;

  gint64 timestamp; /* monotonic time when we displayed the last frame */
  guint next_frame_cb; /* Source ID of next frame callback */
  gboolean playing;
};

struct _GtkDemoVideoClass {
  GObjectClass parent_class;
};

static GParamSpec *properties[N_PROPS];

static void
gtk_demo_video_paintable_snapshot (GdkPaintable *paintable,
                                   GdkSnapshot  *snapshot,
                                   double        width,
                                   double        height)
{
  GtkDemoVideo *video = GTK_DEMO_VIDEO (paintable);

  if (video->current_texture)
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (video->current_texture), snapshot, width, height);
    }
  else
    {
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA) { 1.0, 0.1, 0.6, 1.0 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height),
                                 "Video Fallback image");
    }
}

static int
gtk_demo_video_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
#ifdef HAVE_FFMPEG
  GtkDemoVideo *video = GTK_DEMO_VIDEO (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->width;
#endif

  return 0;
}

static int
gtk_demo_video_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
#ifdef HAVE_FFMPEG
  GtkDemoVideo *video = GTK_DEMO_VIDEO (paintable);

  if (video->codec_ctx)
    return video->codec_ctx->height;
#endif

  return 0;
}

static double gtk_demo_video_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
#ifdef HAVE_FFMPEG
  GtkDemoVideo *video = GTK_DEMO_VIDEO (paintable);

  if (video->codec_ctx)
    return (double) video->codec_ctx->width / video->codec_ctx->height;
#endif

  return 0.0;
};

static void
gtk_demo_video_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_demo_video_paintable_snapshot;
  iface->get_intrinsic_width = gtk_demo_video_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_demo_video_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_demo_video_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_WITH_CODE (GtkDemoVideo, gtk_demo_video, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_demo_video_paintable_init))

static void
gtk_demo_video_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkDemoVideo *video = GTK_DEMO_VIDEO (gobject);

  switch (prop_id)
    {
    case PROP_FILENAME:
      gtk_demo_video_set_filename (video, g_value_get_string (value));
      break;

    case PROP_PLAYING:
      gtk_demo_video_set_playing (video, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_demo_video_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkDemoVideo *video = GTK_DEMO_VIDEO (gobject);

  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, video->filename);
      break;

    case PROP_PLAYING:
      g_value_set_boolean (value, video->playing);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_demo_video_stop (GtkDemoVideo *video)
{
  if (!video->playing)
    return;

  if (video->next_frame_cb)
    {
      g_source_remove (video->next_frame_cb);
      video->next_frame_cb = 0;
    }

  video->timestamp = FALSE;
  video->playing = FALSE;
  g_object_notify_by_pspec (G_OBJECT (video), properties[PROP_PLAYING]);
}

static void 
gtk_demo_video_clear (GtkDemoVideo *video)
{
  g_clear_pointer (&video->filename, g_free);

#ifdef HAVE_FFMPEG
  g_clear_pointer (&video->sws_ctx, sws_freeContext);
  g_clear_pointer (&video->codec_ctx, avcodec_close);
  avformat_close_input (&video->format_ctx);
  video->stream_id = -1;
#endif
  g_clear_object (&video->current_texture);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
}

static void
gtk_demo_video_dispose (GObject *object)
{
  GtkDemoVideo *video = GTK_DEMO_VIDEO (object);

  gtk_demo_video_stop (video);
  gtk_demo_video_clear (video);

  G_OBJECT_CLASS (gtk_demo_video_parent_class)->dispose (object);
}

static void
gtk_demo_video_class_init (GtkDemoVideoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_demo_video_set_property;
  gobject_class->get_property = gtk_demo_video_get_property;
  gobject_class->dispose = gtk_demo_video_dispose;

  properties[PROP_FILENAME] =
    g_param_spec_string ("filename",
                         "Filename",
                         "The file to play",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_PLAYING] =
    g_param_spec_string ("playing",
                         "Playing",
                         "TRUE if the file is playing",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

#ifdef HAVE_FFMPEG
  av_register_all ();
#endif
}

static void
gtk_demo_video_init (GtkDemoVideo *video)
{
  video->stream_id = -1;
}

GtkDemoVideo *
gtk_demo_video_new (void)
{
  return g_object_new (GTK_TYPE_DEMO_VIDEO, NULL);
}

GtkDemoVideo *
gtk_demo_video_new_for_filename (const char *filename)
{
  GtkDemoVideo *video;

  g_return_val_if_fail (filename != NULL, NULL);

  video = gtk_demo_video_new ();
  gtk_demo_video_set_filename (video, filename);
  gtk_demo_video_set_playing (video, TRUE);

  return video;
}

static void
gtk_demo_video_set_error (GtkDemoVideo *video,
                          const char   *error_message)
{
}

#ifdef HAVE_FFMPEG
static void
gtk_demo_video_set_ffmpeg_error (GtkDemoVideo *video,
                                 int           av_errnum)
{
  char s[AV_ERROR_MAX_STRING_SIZE];

  if (av_strerror (av_errnum, s, sizeof (s) != 0))
    snprintf (s, sizeof (s), "Unspecified error decoding video");

  gtk_demo_video_set_error (video, s);
}

static GdkTexture *
gtk_demo_video_decode_frame (GtkDemoVideo *video,
                             gint64       *duration)
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
      gtk_demo_video_set_ffmpeg_error (video, errnum);
      av_frame_free (&frame);
      *duration = 0;
      return NULL;
    }
  
  data = g_try_malloc0 (video->codec_ctx->width * video->codec_ctx->height * 4);
  if (data == NULL)
    {
      gtk_demo_video_set_error (video, "Not enough memory");
      av_frame_free (&frame);
      *duration = 0;
      return NULL;
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

  if (av_frame_get_pkt_duration (frame))
    *duration = av_rescale_q (av_frame_get_pkt_duration (frame),
                              video->format_ctx->streams[video->stream_id]->time_base,
                              (AVRational) { 1, G_USEC_PER_SEC });
  else
    *duration = av_rescale_q (1,
                              video->format_ctx->streams[video->stream_id]->time_base,
                              (AVRational) { 1, G_USEC_PER_SEC });
  av_frame_free (&frame);

  return texture;
}

static void
gtk_demo_video_open_ffmpeg (GtkDemoVideo *video)
{
  AVStream *stream;
  AVCodec *codec;
  int errnum;

  errnum = avformat_open_input (&video->format_ctx, video->filename, NULL, NULL);
  if (errnum != 0)
    {
      gtk_demo_video_set_ffmpeg_error (video, errnum);
      return;
    }
  
  errnum = avformat_find_stream_info (video->format_ctx, NULL);
  if (errnum < 0)
    {
      gtk_demo_video_set_ffmpeg_error (video, errnum);
      return;
    }
  
  video->stream_id = av_find_best_stream (video->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video->stream_id < 0)
    {
      gtk_demo_video_set_error (video, "File contains no video");
      return;
    }
  
  stream = video->format_ctx->streams[video->stream_id];
  codec = avcodec_find_decoder (stream->codecpar->codec_id);
  if (codec == NULL)
    {
      gtk_demo_video_set_error (video, "Unsupported video codec");
      return;
    }

  video->codec_ctx = avcodec_alloc_context3 (codec);
  errnum = avcodec_parameters_to_context (video->codec_ctx, stream->codecpar);
  if (errnum < 0)
    {
      gtk_demo_video_set_ffmpeg_error (video, errnum);
      return;
    }
  errnum = avcodec_open2(video->codec_ctx, codec, NULL);
  if (errnum < 0)
    {
      gtk_demo_video_set_ffmpeg_error (video, errnum);
      return;
    }

  video->sws_ctx = sws_getContext(video->codec_ctx->width,
                                  video->codec_ctx->height,
                                  video->codec_ctx->pix_fmt,
                                  video->codec_ctx->width,
                                  video->codec_ctx->height,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                  AV_PIX_FMT_BGRA,
#else
                                  AV_PIX_FMT_ARGB,
#endif
                                  SWS_BILINEAR,
                                  NULL,
                                  NULL,
                                  NULL);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (video));

  video->current_texture = gtk_demo_video_decode_frame (video, &video->current_texture_duration);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
}

#endif /* HAVE_FFMPEG */

static void
gtk_demo_video_open (GtkDemoVideo *video)
{
  if (video->filename == NULL)
    return;

#ifdef HAVE_FFMPEG
  gtk_demo_video_open_ffmpeg (video);
#else
  gtk_demo_video_set_error (video, "Video support not enabled at build time.");
#endif
}

void
gtk_demo_video_set_filename (GtkDemoVideo *video,
                             const char   *filename)
{
  gtk_demo_video_clear (video);

  video->filename = g_strdup (filename);

  gtk_demo_video_open (video);

  g_object_notify_by_pspec (G_OBJECT (video), properties[PROP_FILENAME]);
}

static gboolean
gtk_demo_video_next_frame_cb (gpointer data);
static void
gtk_demo_video_queue_frame (GtkDemoVideo *video)
{
  gint64 time, frame_time;
  guint delay;
  
  time = g_get_monotonic_time ();
  frame_time = video->timestamp + video->current_texture_duration;
  delay = time > frame_time ? 0 : (frame_time - time) / 1000;

  video->next_frame_cb = g_timeout_add (delay, gtk_demo_video_next_frame_cb, video);
}

static gboolean
gtk_demo_video_next_frame_cb (gpointer data)
{
  GtkDemoVideo *video = data;
  GdkTexture *next_texture;
  gint64 next_duration;
  
  video->next_frame_cb = 0;
  next_texture = gtk_demo_video_decode_frame (video, &next_duration);
  if (next_texture)
    {
      video->timestamp += video->current_texture_duration;
      g_clear_object (&video->current_texture);
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (video));
      video->current_texture_duration = next_duration;
      video->current_texture = next_texture;

      gtk_demo_video_queue_frame (video);
    }
  else
    {
      video->current_texture_duration = 0;
      gtk_demo_video_stop (video);
    }

  return G_SOURCE_REMOVE;
}

static void
gtk_demo_video_play (GtkDemoVideo *video)
{
  if (video->playing)
    return;

  if (video->current_texture_duration == 0)
    return;

  video->timestamp = g_get_monotonic_time ();
  video->playing = TRUE;
  g_object_notify_by_pspec (G_OBJECT (video), properties[PROP_PLAYING]);

  gtk_demo_video_queue_frame (video);
}

void
gtk_demo_video_set_playing (GtkDemoVideo *video,
                            gboolean      playing)
{
  if (video->playing == playing)
    return;

  if (playing)
    gtk_demo_video_play (video);
  else
    gtk_demo_video_stop (video);

  video->playing = playing;
  g_object_notify_by_pspec (G_OBJECT (video), properties[PROP_PLAYING]);
}
