/*
 * Copyright © 2020 Benjamin Otte
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

#include "ottieplayer.h"

#include "ottiecreation.h"
#include "ottiepaintable.h"

#include <glib/gi18n.h>

struct _OttiePlayer
{
  GObject parent_instance;

  GFile *file;

  OttieCreation *creation;
  OttiePaintable *paintable;
  gint64 time_offset;
  guint timer_cb;
};

struct _OttiePlayerClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_FILE,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
ottie_player_paintable_snapshot (GdkPaintable *paintable,
                                 GdkSnapshot  *snapshot,
                                 double        width,
                                 double        height)
{
  OttiePlayer *self = OTTIE_PLAYER (paintable);

  gdk_paintable_snapshot (GDK_PAINTABLE (self->paintable), snapshot, width, height);
}

static int
ottie_player_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  OttiePlayer *self = OTTIE_PLAYER (paintable);

  return gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (self->paintable));
}

static int
ottie_player_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  OttiePlayer *self = OTTIE_PLAYER (paintable);

  return gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (self->paintable));
}

static void
ottie_player_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = ottie_player_paintable_snapshot;
  iface->get_intrinsic_width = ottie_player_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = ottie_player_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (OttiePlayer, ottie_player, GTK_TYPE_MEDIA_STREAM, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               ottie_player_paintable_init))

static gboolean
ottie_player_timer_cb (gpointer data)
{
  OttiePlayer *self = OTTIE_PLAYER (data);
  gint64 timestamp;

  timestamp = g_get_monotonic_time () - self->time_offset;
  if (timestamp > ottie_paintable_get_duration (self->paintable))
    {
      if (gtk_media_stream_get_loop (GTK_MEDIA_STREAM (self)))
        {
          timestamp %= ottie_paintable_get_duration (self->paintable);
        }
      else
        {
          timestamp = ottie_paintable_get_duration (self->paintable);
          gtk_media_stream_stream_ended (GTK_MEDIA_STREAM (self));
        }
    }
  ottie_paintable_set_timestamp (self->paintable, timestamp);
  gtk_media_stream_update (GTK_MEDIA_STREAM (self), timestamp);

  return G_SOURCE_CONTINUE;
}

static gboolean
ottie_player_play (GtkMediaStream *stream)
{
  OttiePlayer *self = OTTIE_PLAYER (stream);
  double frame_rate;

  frame_rate = ottie_creation_get_frame_rate (self->creation);
  if (frame_rate <= 0)
    return FALSE;

  self->time_offset = g_get_monotonic_time () - ottie_paintable_get_timestamp (self->paintable);
  self->timer_cb = g_timeout_add (1000 / frame_rate, ottie_player_timer_cb, self);

  return TRUE;
}

static void
ottie_player_pause (GtkMediaStream *stream)
{
  OttiePlayer *self = OTTIE_PLAYER (stream);

  g_clear_handle_id (&self->timer_cb, g_source_remove);
}

static void
ottie_player_seek (GtkMediaStream *stream,
                   gint64          timestamp)
{
  OttiePlayer *self = OTTIE_PLAYER (stream);

  if (!ottie_creation_is_prepared (self->creation))
    gtk_media_stream_seek_failed (stream);

  ottie_paintable_set_timestamp (self->paintable, timestamp);
  self->time_offset = g_get_monotonic_time () - timestamp;

  gtk_media_stream_seek_success (stream);
  gtk_media_stream_update (stream, timestamp);
}

static void
ottie_player_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)

{
  OttiePlayer *self = OTTIE_PLAYER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ottie_player_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_player_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  OttiePlayer *self = OTTIE_PLAYER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_player_dispose (GObject *object)
{
  OttiePlayer *self = OTTIE_PLAYER (object);

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);
      g_clear_object (&self->paintable);
    }
  g_clear_object (&self->creation);

  G_OBJECT_CLASS (ottie_player_parent_class)->dispose (object);
}

static void
ottie_player_class_init (OttiePlayerClass *klass)
{
  GtkMediaStreamClass *stream_class = GTK_MEDIA_STREAM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  stream_class->play = ottie_player_play;
  stream_class->pause = ottie_player_pause;
  stream_class->seek = ottie_player_seek;

  gobject_class->get_property = ottie_player_get_property;
  gobject_class->set_property = ottie_player_set_property;
  gobject_class->dispose = ottie_player_dispose;

  /**
   * OttiePlayer:file
   *
   * The played file or %NULL.
   */
  properties[PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The played file"),
                         G_TYPE_FILE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
ottie_player_prepared_cb (OttieCreation *creation,
                          GParamSpec    *pspec,
                          OttiePlayer   *self)
{
  if (ottie_creation_is_prepared (creation))
    gtk_media_stream_stream_prepared (GTK_MEDIA_STREAM (self),
                                      FALSE,
                                      TRUE,
                                      TRUE,
                                      ottie_paintable_get_duration (self->paintable));
  else
    gtk_media_stream_stream_unprepared (GTK_MEDIA_STREAM (self));

  ottie_paintable_set_timestamp (self->paintable, 0);
}

static void
ottie_player_init (OttiePlayer *self)
{
  self->creation = ottie_creation_new ();
  g_signal_connect (self->creation, "notify::prepared", G_CALLBACK (ottie_player_prepared_cb), self);
  self->paintable = ottie_paintable_new (self->creation);
  g_signal_connect_swapped (self->paintable, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);
  g_signal_connect_swapped (self->paintable, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);
}

/**
 * ottie_player_new:
 *
 * Creates a new Ottie player.
 *
 * Returns: (transfer full): a new #OttiePlayer
 **/
OttiePlayer *
ottie_player_new (void)
{
  return g_object_new (OTTIE_TYPE_PLAYER, NULL);
}

/**
 * ottie_player_new_for_file:
 * @file: (nullable): a #GFile
 *
 * Creates a new #OttiePlayer playing the given @file. If the file
 * isn’t found or can’t be loaded, the resulting #OttiePlayer be empty.
 *
 * Returns: a new #OttiePlayer
 **/
OttiePlayer*
ottie_player_new_for_file (GFile *file)
{
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);

  return g_object_new (OTTIE_TYPE_PLAYER,
                       "file", file,
                       NULL);
}

/**
 * ottie_player_new_for_filename:
 * @filename: (type filename) (nullable): a filename
 *
 * Creates a new #OttiePlayer displaying the file @filename.
 *
 * This is a utility function that calls ottie_player_new_for_file().
 * See that function for details.
 *
 * Returns: a new #OttiePlayer
 **/
OttiePlayer*
ottie_player_new_for_filename (const char *filename)
{
  OttiePlayer *result;
  GFile *file;

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  result = ottie_player_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * ottie_player_new_for_resource:
 * @resource_path: (nullable): resource path to play back
 *
 * Creates a new #OttiePlayer displaying the file @resource_path.
 *
 * This is a utility function that calls ottie_player_new_for_file().
 * See that function for details.
 *
 * Returns: a new #OttiePlayer
 **/
OttiePlayer *
ottie_player_new_for_resource (const char *resource_path)
{
  OttiePlayer *result;
  GFile *file;

  if (resource_path)
    {
      char *uri, *escaped;

      escaped = g_uri_escape_string (resource_path,
                                     G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
      uri = g_strconcat ("resource://", escaped, NULL);
      g_free (escaped);

      file = g_file_new_for_uri (uri);
      g_free (uri);
    }
  else
    {
      file = NULL;
    }

  result = ottie_player_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * ottie_player_set_file:
 * @self: a #OttiePlayer
 * @file: (nullable): a %GFile or %NULL
 *
 * Makes @self load and display @file.
 *
 * See ottie_player_new_for_file() for details.
 **/
void
ottie_player_set_file (OttiePlayer *self,
                       GFile      *file)
{
  g_return_if_fail (OTTIE_IS_PLAYER (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (self->file == file)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_set_object (&self->file, file);
  ottie_creation_load_file (self->creation, file);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * ottie_player_get_file:
 * @self: a #OttiePlayer
 *
 * Gets the #GFile currently displayed if @self is displaying a file.
 * If @self is not displaying a file, then %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The #GFile displayed by @self.
 **/
GFile *
ottie_player_get_file (OttiePlayer *self)
{
  g_return_val_if_fail (OTTIE_IS_PLAYER (self), FALSE);

  return self->file;
}

/**
 * ottie_player_set_filename:
 * @self: a #OttiePlayer
 * @filename: (nullable): the filename to play
 *
 * Makes @self load and display the given @filename.
 *
 * This is a utility function that calls ottie_player_set_file().
 **/
void
ottie_player_set_filename (OttiePlayer *self,
                          const char *filename)
{
  GFile *file;

  g_return_if_fail (OTTIE_IS_PLAYER (self));

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  ottie_player_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * ottie_player_set_resource:
 * @self: a #OttiePlayer
 * @resource_path: (nullable): the resource to set
 *
 * Makes @self load and display the resource at the given
 * @resource_path.
 *
 * This is a utility function that calls ottie_player_set_file(),
 **/
void
ottie_player_set_resource (OttiePlayer *self,
                          const char *resource_path)
{
  GFile *file;

  g_return_if_fail (OTTIE_IS_PLAYER (self));

  if (resource_path)
    {
      char *uri, *escaped;

      escaped = g_uri_escape_string (resource_path,
                                     G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
      uri = g_strconcat ("resource://", escaped, NULL);
      g_free (escaped);

      file = g_file_new_for_uri (uri);
      g_free (uri);
    }
  else
    {
      file = NULL;
    }

  ottie_player_set_file (self, file);

  if (file)
    g_object_unref (file);
}

