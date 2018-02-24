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

#include "gtkmediastream.h"

#include "gtkintl.h"
#include "gtkvideotrackprivate.h"

/**
 * SECTION:gtkmediastream
 * @Short_description: Display media in GTK
 * @Title: GtkMediaStream
 * @See_also: #GdkPaintable
 *
 * #GtkMediaStream is the integration point for media playback inside GTK.
 *
 * FIXME: Write more about how frameworks should implement this thing and how
 * GTK widgets exist (once they do) that consume it.
 */

typedef struct _GtkMediaStreamPrivate GtkMediaStreamPrivate;

struct _GtkMediaStreamPrivate
{
  gint64 timestamp;
  gint64 duration;

  guint has_audio : 1;
  guint has_video : 1;
  guint playing : 1;
  guint ended : 1;
  guint seekable : 1;
  guint seeking : 1;
  guint loop : 1;
};

enum {
  PROP_0,
  PROP_HAS_AUDIO,
  PROP_HAS_VIDEO,
  PROP_PLAYING,
  PROP_ENDED,
  PROP_TIMESTAMP,
  PROP_DURATION,
  PROP_SEEKABLE,
  PROP_SEEKING,
  PROP_LOOP,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_media_stream_paintable_snapshot (GdkPaintable *paintable,
                                     GdkSnapshot  *snapshot,
                                     double        width,
                                     double        height)
{
}

static void
gtk_media_stream_paintable_init (GdkPaintableInterface *iface)
{
  /* We implement the behavior for "no video stream" here */
  iface->snapshot = gtk_media_stream_paintable_snapshot;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkMediaStream, gtk_media_stream, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                         gtk_media_stream_paintable_init)
                                  G_ADD_PRIVATE (GtkMediaStream))

#define GTK_MEDIA_STREAM_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Renderer of type '%s' does not implement GskRenderer::" # method, G_OBJECT_TYPE_NAME (obj))

static gboolean
gtk_media_stream_default_play (GtkMediaStream *self)
{
  GTK_MEDIA_STREAM_WARN_NOT_IMPLEMENTED_METHOD (self, play);

  return FALSE;
}

static void
gtk_media_stream_default_pause (GtkMediaStream *self)
{
  GTK_MEDIA_STREAM_WARN_NOT_IMPLEMENTED_METHOD (self, pause);
}

static void
gtk_media_stream_default_seek (GtkMediaStream *self,
                               gint64          timestamp)
{
  gtk_media_stream_seek_failed (self);
}

static void
gtk_media_stream_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)

{
  GtkMediaStream *self = GTK_MEDIA_STREAM (object);

  switch (prop_id)
    {
      case PROP_PLAYING:
        gtk_media_stream_set_playing (self, g_value_get_boolean (value));
        break;

      case PROP_LOOP:
        gtk_media_stream_set_loop (self, g_value_get_boolean (value));
        break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_media_stream_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkMediaStream *self = GTK_MEDIA_STREAM (object);
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HAS_AUDIO:
      g_value_set_boolean (value, priv->has_audio);
      break;

    case PROP_HAS_VIDEO:
      g_value_set_boolean (value, priv->has_video);
      break;

    case PROP_PLAYING:
      g_value_set_boolean (value, priv->playing);
      break;

    case PROP_ENDED:
      g_value_set_boolean (value, priv->ended);
      break;

    case PROP_TIMESTAMP:
      g_value_set_int64 (value, priv->timestamp);
      break;

    case PROP_DURATION:
      g_value_set_int64 (value, priv->duration);
      break;

    case PROP_SEEKABLE:
      g_value_set_boolean (value, priv->seekable);
      break;

    case PROP_SEEKING:
      g_value_set_boolean (value, priv->seeking);
      break;

    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_media_stream_dispose (GObject *object)
{
#if 0
  GtkMediaStream *self = GTK_MEDIA_STREAM (object);
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);
#endif

  G_OBJECT_CLASS (gtk_media_stream_parent_class)->dispose (object);
}

static void
gtk_media_stream_finalize (GObject *object)
{
#if 0
  GtkMediaStream *self = GTK_MEDIA_STREAM (object);
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);
#endif

  G_OBJECT_CLASS (gtk_media_stream_parent_class)->finalize (object);
}

static void
gtk_media_stream_class_init (GtkMediaStreamClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  class->play = gtk_media_stream_default_play;
  class->pause = gtk_media_stream_default_pause;
  class->seek = gtk_media_stream_default_seek;

  gobject_class->set_property = gtk_media_stream_set_property;
  gobject_class->get_property = gtk_media_stream_get_property;
  gobject_class->finalize = gtk_media_stream_finalize;
  gobject_class->dispose = gtk_media_stream_dispose;

  /**
   * GtkMediaStream:has-audio:
   *
   * Whether the stream contains audio
   */
  properties[PROP_HAS_AUDIO] =
    g_param_spec_boolean ("has-audio",
                          P_("Has audio"),
                          P_("Whether the stream contains audio"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:has-video:
   *
   * Whether the stream contains video
   */
  properties[PROP_HAS_VIDEO] =
    g_param_spec_boolean ("has-video",
                          P_("Has video"),
                          P_("Whether the stream contains video"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:playing:
   *
   * Whether the stream is currently playing.
   */
  properties[PROP_PLAYING] =
    g_param_spec_boolean ("playing",
                          P_("Playing"),
                          P_("Whether the stream is playing"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:ended:
   *
   * Set when playback has finished.
   */
  properties[PROP_ENDED] =
    g_param_spec_boolean ("ended",
                          P_("Ended"),
                          P_("Set when playback has finished"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:timestamp:
   *
   * The current presentation timestamp in microseconds.
   */
  properties[PROP_TIMESTAMP] =
    g_param_spec_int64 ("timestamp",
                        P_("Timestamp"),
                        P_("Timestamp in microseconds"),
                        0, G_MAXINT64, 0,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:duration:
   *
   * The stream's duration in microseconds or 0 if unknown.
   */
  properties[PROP_DURATION] =
    g_param_spec_int64 ("duration",
                        P_("Duration"),
                        P_("Timestamp in microseconds"),
                        0, G_MAXINT64, 0,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:seekable:
   *
   * Set unless the stream is known to not support seeking.
   */
  properties[PROP_SEEKABLE] =
    g_param_spec_boolean ("seekable",
                          P_("Seekable"),
                          P_("Set unless seeking is not supported"),
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:seeking:
   *
   * Set while a seek is in progress.
   */
  properties[PROP_SEEKING] =
    g_param_spec_boolean ("seeking",
                          P_("Seeking"),
                          P_("Set while a seek is in progress"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkMediaStream:loop:
   *
   * Try to restart the media from the beginning once it ended.
   */
  properties[PROP_LOOP] =
    g_param_spec_boolean ("loop",
                          P_("Loop"),
                          P_("Try to restart the media from the beginning once it ended."),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_media_stream_init (GtkMediaStream *self)
{
}

gboolean
gtk_media_stream_has_audio (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->has_audio;
}

gboolean
gtk_media_stream_has_video (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->has_video;
}

void
gtk_media_stream_play (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));

  if (priv->playing)
    return;

  if (GTK_MEDIA_STREAM_GET_CLASS (self)->play (self))
    {
      priv->playing = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLAYING]);
    }
}

void
gtk_media_stream_pause (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));

  if (!priv->playing)
    return;

  GTK_MEDIA_STREAM_GET_CLASS (self)->pause (self);

  priv->playing = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLAYING]);
}

gboolean
gtk_media_stream_get_playing (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->playing;
}

void
gtk_media_stream_set_playing (GtkMediaStream *self,
                              gboolean        playing)
{
  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));

  if (playing)
    gtk_media_stream_play (self);
  else
    gtk_media_stream_pause (self);
}

gboolean
gtk_media_stream_get_ended (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->ended;
}

gint64
gtk_media_stream_get_timestamp (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->timestamp;
}

gint64
gtk_media_stream_get_duration (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->duration;
}

gboolean
gtk_media_stream_is_seekable (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->seekable;
}

gboolean
gtk_media_stream_is_seeking (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->seeking;
}

void
gtk_media_stream_seek (GtkMediaStream *self,
                       gint64          timestamp)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);
  gboolean was_seeking;

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));
  g_return_if_fail (timestamp >= 0);

  if (!priv->seekable)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  was_seeking = priv->seeking;
  priv->seeking = TRUE;

  GTK_MEDIA_STREAM_GET_CLASS (self)->seek (self, timestamp);

  if (was_seeking != priv->seeking)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEEKING]);

  g_object_thaw_notify (G_OBJECT (self));
}

gboolean
gtk_media_stream_get_loop (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_MEDIA_STREAM (self), FALSE);

  return priv->loop;
}

void
gtk_media_stream_set_loop (GtkMediaStream *self,
                           gboolean        loop)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));

  if (priv->loop == loop)
    return;

  priv->loop = loop;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOOP]);
}

void
gtk_media_stream_initialized (GtkMediaStream *self,
                              gboolean        has_audio,
                              gboolean        has_video,
                              gboolean        seekable,
                              gint64          duration)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (priv->has_audio != has_audio)
    {
      priv->has_audio = has_audio;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_AUDIO]);
    }
  if (priv->has_video != has_video)
    {
      priv->has_video = has_video;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_VIDEO]);
    }
  if (priv->seekable != seekable)
    {
      priv->seekable = seekable;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEEKABLE]);
    }
  if (priv->seekable != seekable)
    {
      priv->seekable = seekable;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEEKABLE]);
    }
  if (priv->duration != duration)
    {
      priv->duration = duration;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

void
gtk_media_stream_update (GtkMediaStream *self,
                         gint64          timestamp)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));

  if (priv->timestamp != timestamp)
    {
      priv->timestamp = timestamp;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMESTAMP]);
    }
}

void
gtk_media_stream_ended (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));
  g_return_if_fail (!gtk_media_stream_get_ended (self));

  g_object_freeze_notify (G_OBJECT (self));

  gtk_media_stream_pause (self);

  priv->ended = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);

  g_object_thaw_notify (G_OBJECT (self));
}

void
gtk_media_stream_seek_success (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));
  g_return_if_fail (gtk_media_stream_is_seeking (self));

  g_object_freeze_notify (G_OBJECT (self));

  priv->seeking = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEEKING]);

  if (priv->ended)
    {
      priv->ended = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENDED]);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

void
gtk_media_stream_seek_failed (GtkMediaStream *self)
{
  GtkMediaStreamPrivate *priv = gtk_media_stream_get_instance_private (self);

  g_return_if_fail (GTK_IS_MEDIA_STREAM (self));
  g_return_if_fail (gtk_media_stream_is_seeking (self));

  priv->seeking = FALSE;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEEKING]);
}

#include "gtkmediastreamffmpegprivate.h"

GtkMediaStream *
gtk_media_stream_new_for_filename (const char *filename)
{
  g_return_val_if_fail (filename != NULL, NULL);

  return gtk_media_stream_ffmpeg_new_for_filename (filename);
}

