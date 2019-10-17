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

#include "gtkvideo.h"

#include "gtkbinlayout.h"
#include "gtkeventcontrollermotion.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkmediacontrols.h"
#include "gtkmediafile.h"
#include "gtknative.h"
#include "gtkpicture.h"
#include "gtkrevealer.h"

/**
 * SECTION:gtkvideo
 * @title: GtkVideo
 * @short_description: A widget for displaying video
 * @see_also: #GtkMediaControls
 *
 * GtkVideo is a widget to show a #GtkMediaStream.
 *
 * It is commonly combined with #GtkMediaControls to give the
 * user a way to control the playback.
 */

struct _GtkVideo
{
  GtkWidget parent_instance;

  GFile *file;
  GtkMediaStream *media_stream;

  GtkWidget *box;
  GtkWidget *video_picture;
  GtkWidget *overlay_icon;
  GtkWidget *controls_revealer;
  GtkWidget *controls;
  guint controls_hide_source;

  guint autoplay : 1;
  guint loop : 1;
};

enum
{
  PROP_0,
  PROP_AUTOPLAY,
  PROP_FILE,
  PROP_LOOP,
  PROP_MEDIA_STREAM,

  N_PROPS
};

G_DEFINE_TYPE (GtkVideo, gtk_video, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };

static gboolean
gtk_video_hide_controls (gpointer data)
{
  GtkVideo *self = data;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->controls_revealer), FALSE);

  self->controls_hide_source = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_video_motion (GtkEventControllerMotion *motion,
                  double                    x,
                  double                    y,
                  GtkVideo                 *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->controls_revealer), TRUE);
  if (self->controls_hide_source)
    g_source_remove (self->controls_hide_source);
  self->controls_hide_source = g_timeout_add (5 * 1000,
                                              gtk_video_hide_controls,
                                              self);
}

static void
gtk_video_realize (GtkWidget *widget)
{
  GtkVideo *self = GTK_VIDEO (widget);

  GTK_WIDGET_CLASS (gtk_video_parent_class)->realize (widget);

  if (self->media_stream)
    {
      GdkSurface *surface;

      surface = gtk_native_get_surface (gtk_widget_get_native (widget));
      gtk_media_stream_realize (self->media_stream, surface);
    }

  if (self->file)
    gtk_media_file_set_file (GTK_MEDIA_FILE (self->media_stream), self->file);
}

static void
gtk_video_unrealize (GtkWidget *widget)
{
  GtkVideo *self = GTK_VIDEO (widget);

  if (self->media_stream)
    {
      GdkSurface *surface;

      surface = gtk_native_get_surface (gtk_widget_get_native (widget));
      gtk_media_stream_unrealize (self->media_stream, surface);
    }

  GTK_WIDGET_CLASS (gtk_video_parent_class)->unrealize (widget);
}

static void
gtk_video_map (GtkWidget *widget)
{
  GtkVideo *self = GTK_VIDEO (widget);

  GTK_WIDGET_CLASS (gtk_video_parent_class)->map (widget);

  if (self->autoplay && self->media_stream)
    gtk_media_stream_play (self->media_stream);
}

static void
gtk_video_unmap (GtkWidget *widget)
{
  GtkVideo *self = GTK_VIDEO (widget);

  if (self->controls_hide_source)
    {
      g_source_remove (self->controls_hide_source);
      self->controls_hide_source = 0;
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->controls_revealer), FALSE);
    }

  /* XXX: pause video here? */

  GTK_WIDGET_CLASS (gtk_video_parent_class)->unmap (widget);
}

static void
gtk_video_dispose (GObject *object)
{
  GtkVideo *self = GTK_VIDEO (object);

  gtk_video_set_media_stream (self, NULL);

  g_clear_pointer (&self->box, gtk_widget_unparent);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (gtk_video_parent_class)->dispose (object);
}

static void
gtk_video_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkVideo *self = GTK_VIDEO (object);

  switch (property_id)
    {
    case PROP_AUTOPLAY:
      g_value_set_boolean (value, self->autoplay);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_LOOP:
      g_value_set_boolean (value, self->loop);
      break;

    case PROP_MEDIA_STREAM:
      g_value_set_object (value, self->media_stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_video_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkVideo *self = GTK_VIDEO (object);

  switch (property_id)
    {
    case PROP_AUTOPLAY:
      gtk_video_set_autoplay (self, g_value_get_boolean (value));
      break;

    case PROP_FILE:
      gtk_video_set_file (self, g_value_get_object (value));
      break;

    case PROP_LOOP:
      gtk_video_set_loop (self, g_value_get_boolean (value));
      break;

    case PROP_MEDIA_STREAM:
      gtk_video_set_media_stream (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_video_class_init (GtkVideoClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->realize = gtk_video_realize;
  widget_class->unrealize = gtk_video_unrealize;
  widget_class->map = gtk_video_map;
  widget_class->unmap = gtk_video_unmap;

  gobject_class->dispose = gtk_video_dispose;
  gobject_class->get_property = gtk_video_get_property;
  gobject_class->set_property = gtk_video_set_property;

  /**
   * GtkVideo:autoplay:
   *
   * If the video should automatically begin playing.
   */
  properties[PROP_AUTOPLAY] =
    g_param_spec_boolean ("autoplay",
                          P_("Autoplay"),
                          P_("If playback should begin automatically"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkVideo:file:
   *
   * The file played by this video if the video is playing a file.
   */
  properties[PROP_FILE] =
    g_param_spec_object ("file",
                         P_("File"),
                         P_("The video file played back"),
                         G_TYPE_FILE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkVideo:loop:
   *
   * If new media files should be set to loop.
   */
  properties[PROP_LOOP] =
    g_param_spec_boolean ("loop",
                          P_("Loop"),
                          P_("If new media streams should be set to loop"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkVideo:media-stream:
   *
   * The media-stream played
   */
  properties[PROP_MEDIA_STREAM] =
    g_param_spec_object ("media-stream",
                         P_("Media Stream"),
                         P_("The media stream played"),
                         GTK_TYPE_MEDIA_STREAM,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkvideo.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkVideo, box);
  gtk_widget_class_bind_template_child (widget_class, GtkVideo, video_picture);
  gtk_widget_class_bind_template_child (widget_class, GtkVideo, overlay_icon);
  gtk_widget_class_bind_template_child (widget_class, GtkVideo, controls);
  gtk_widget_class_bind_template_child (widget_class, GtkVideo, controls_revealer);
  gtk_widget_class_bind_template_callback (widget_class, gtk_video_motion);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("video"));
}

static void
gtk_video_init (GtkVideo *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtk_video_new:
 *
 * Creates a new empty #GtkVideo.
 *
 * Returns: a new #GtkVideo
 **/
GtkWidget *
gtk_video_new (void)
{
  return g_object_new (GTK_TYPE_VIDEO, NULL);
}

/**
 * gtk_video_new_for_media_stream:
 * @stream: (allow-none): a #GtkMediaStream
 *
 * Creates a #GtkVideo to play back the given @stream.
 *
 * Returns: a new #GtkVideo
 **/
GtkWidget *
gtk_video_new_for_media_stream (GtkMediaStream *stream)
{
  g_return_val_if_fail (stream == NULL || GTK_IS_MEDIA_STREAM (stream), NULL);

  return g_object_new (GTK_TYPE_VIDEO,
                       "media-stream", stream,
                       NULL);
}

/**
 * gtk_video_new_for_file:
 * @file: (allow-none): a #GFile
 *
 * Creates a #GtkVideo to play back the given @file.
 *
 * Returns: a new #GtkVideo
 **/
GtkWidget *
gtk_video_new_for_file (GFile *file)
{
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);

  return g_object_new (GTK_TYPE_VIDEO,
                       "file", file,
                       NULL);
}

/**
 * gtk_video_new_for_filename:
 * @filename: (allow-none) (type filename): filename to play back
 *
 * Creates a #GtkVideo to play back the given @filename.
 *
 * This is a utility function that calls gtk_video_new_for_file(),
 * See that function for details.
 *
 * Returns: a new #GtkVideo
 **/
GtkWidget *
gtk_video_new_for_filename (const char *filename)
{
  GtkWidget *result;
  GFile *file;

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  result = gtk_video_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_video_new_for_resource:
 * @resource_path: (allow-none): resource path to play back
 *
 * Creates a #GtkVideo to play back the resource at the
 * given @resource_path.
 *
 * This is a utility function that calls gtk_video_new_for_file(),
 *
 * Returns: a new #GtkVideo
 **/
GtkWidget *
gtk_video_new_for_resource (const char *resource_path)
{
  GtkWidget *result;
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

  result = gtk_video_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_video_get_media_stream:
 * @self: a #GtkVideo
 *
 * Gets the media stream managed by @self or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The media stream managed by @self
 **/
GtkMediaStream *
gtk_video_get_media_stream (GtkVideo *self)
{
  g_return_val_if_fail (GTK_IS_VIDEO (self), NULL);

  return self->media_stream;
}

static void
gtk_video_update_overlay_icon (GtkVideo *self)
{
  const char *icon_name;
  const GError *error = NULL;

  if (self->media_stream == NULL)
    icon_name = "media-eject-symbolic";
  else if ((error = gtk_media_stream_get_error (self->media_stream)))
    icon_name = "dialog-error-symbolic";
  else if (gtk_media_stream_get_ended (self->media_stream))
    icon_name = "media-playlist-repeat-symbolic";
  else
    icon_name = "media-playback-start-symbolic";

  gtk_image_set_from_icon_name (GTK_IMAGE (self->overlay_icon), icon_name);
  if (error)
    gtk_widget_set_tooltip_text (self->overlay_icon, error->message);
  else
    gtk_widget_set_tooltip_text (self->overlay_icon, NULL);
}

static void
gtk_video_update_ended (GtkVideo *self)
{
  gtk_video_update_overlay_icon (self);
}

static void
gtk_video_update_error (GtkVideo *self)
{
  gtk_video_update_overlay_icon (self);
}

static void
gtk_video_update_playing (GtkVideo *self)
{
  gboolean playing;

  if (self->media_stream != NULL)
    playing = gtk_media_stream_get_playing (self->media_stream);
  else
    playing = FALSE;

  gtk_widget_set_visible (self->overlay_icon, !playing);
}

static void
gtk_video_update_all (GtkVideo *self)
{
  gtk_video_update_ended (self);
  gtk_video_update_error (self);
  gtk_video_update_playing (self);
}

static void
gtk_video_notify_cb (GtkMediaStream *stream,
                     GParamSpec     *pspec,
                     GtkVideo       *self)
{
  if (g_str_equal (pspec->name, "ended"))
    gtk_video_update_ended (self);
  if (g_str_equal (pspec->name, "error"))
    gtk_video_update_error (self);
  if (g_str_equal (pspec->name, "playing"))
    gtk_video_update_playing (self);
}

/**
 * gtk_video_set_media_stream:
 * @self: a #GtkVideo
 * @stream: (allow-none): The media stream to play or %NULL to unset
 *
 * Sets the media stream to be played back. @self will take full control
 * of managing the media stream. If you want to manage a media stream
 * yourself, consider using a #GtkImage for display.
 *
 * If you want to display a file, consider using gtk_video_set_file()
 * instead.
 **/
void
gtk_video_set_media_stream (GtkVideo       *self,
                            GtkMediaStream *stream)
{
  g_return_if_fail (GTK_IS_VIDEO (self));
  g_return_if_fail (stream == NULL || GTK_IS_MEDIA_STREAM (stream));

  if (self->media_stream == stream)
    return;

  if (self->media_stream)
    {
      g_signal_handlers_disconnect_by_func (self->media_stream,
                                            gtk_video_notify_cb,
                                            self);
      if (gtk_widget_get_realized (GTK_WIDGET (self)))
        {
          GdkSurface *surface;

          surface = gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (self)));
          gtk_media_stream_unrealize (self->media_stream, surface);
        }
      g_object_unref (self->media_stream);
      self->media_stream = NULL;
    }

  if (stream)
    {
      self->media_stream = g_object_ref (stream);
      gtk_media_stream_set_loop (stream, self->loop);
      if (gtk_widget_get_realized (GTK_WIDGET (self)))
        {
          GdkSurface *surface;

          surface = gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (self)));
          gtk_media_stream_realize (stream, surface);
        }
      g_signal_connect (self->media_stream,
                        "notify",
                        G_CALLBACK (gtk_video_notify_cb),
                        self);
      if (self->autoplay)
        gtk_media_stream_play (stream);
    }

  gtk_media_controls_set_media_stream (GTK_MEDIA_CONTROLS (self->controls), stream);
  gtk_picture_set_paintable (GTK_PICTURE (self->video_picture), GDK_PAINTABLE (stream));

  gtk_video_update_all (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MEDIA_STREAM]);
}

/**
 * gtk_video_get_file:
 * @self: a #GtkVideo
 *
 * Gets the file played by @self or %NULL if not playing back
 * a file.
 *
 * Returns: (nullable) (transfer none): The file played by @self
 **/
GFile *
gtk_video_get_file (GtkVideo *self)
{
  g_return_val_if_fail (GTK_IS_VIDEO (self), NULL);

  return self->file;
}

/**
 * gtk_video_set_file:
 * @self: a #GtkVideo
 * @file: (allow-none): the file to play
 *
 * Makes @self play the given @file.
 **/
void
gtk_video_set_file (GtkVideo *self,
                    GFile    *file)
{
  g_return_if_fail (GTK_IS_VIDEO (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (!g_set_object (&self->file, file))
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (file)
    {
      GtkMediaStream *stream;

      stream = gtk_media_file_new ();

      gtk_video_set_media_stream (self, stream);
      if (gtk_widget_get_realized (GTK_WIDGET (self)))
        gtk_media_file_set_file (GTK_MEDIA_FILE (stream), file);

      g_object_unref (stream);
    }
  else
    {
      gtk_video_set_media_stream (self, NULL);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_video_set_filename:
 * @self: a #GtkVideo
 * @filename: (allow-none): the filename to play
 *
 * Makes @self play the given @filename.
 *
 * This is a utility function that calls gtk_video_set_file(),
 **/
void
gtk_video_set_filename (GtkVideo   *self,
                        const char *filename)
{
  GFile *file;

  g_return_if_fail (GTK_IS_VIDEO (self));

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  gtk_video_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_video_set_resource:
 * @self: a #GtkVideo
 * @resource_path: (allow-none): the resource to set
 *
 * Makes @self play the resource at the given @resource_path.
 *
 * This is a utility function that calls gtk_video_set_file(),
 **/
void
gtk_video_set_resource (GtkVideo   *self,
                        const char *resource_path)
{
  GFile *file;

  g_return_if_fail (GTK_IS_VIDEO (self));

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

  gtk_video_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_video_get_autoplay:
 * @self: a #GtkVideo
 *
 * Returns %TRUE if videos have been set to loop via gtk_video_set_loop().
 *
 * Returns: %TRUE if streams should autoplay
 **/
gboolean
gtk_video_get_autoplay (GtkVideo *self)
{
  g_return_val_if_fail (GTK_IS_VIDEO (self), FALSE);

  return self->autoplay;
}

/**
 * gtk_video_set_autoplay:
 * @self: a #GtkVideo
 * @autoplay: whether media streams should autoplay
 *
 * Sets whether @self automatically starts playback when it becomes visible
 * or when a new file gets loaded.
 **/
void
gtk_video_set_autoplay (GtkVideo *self,
                        gboolean  autoplay)
{
  g_return_if_fail (GTK_IS_VIDEO (self));

  if (self->autoplay == autoplay)
    return;

  self->autoplay = autoplay;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTOPLAY]);
}

/**
 * gtk_video_get_loop:
 * @self: a #GtkVideo
 *
 * Returns %TRUE if videos have been set to loop via gtk_video_set_loop().
 *
 * Returns: %TRUE if streams should loop
 **/
gboolean
gtk_video_get_loop (GtkVideo *self)
{
  g_return_val_if_fail (GTK_IS_VIDEO (self), FALSE);

  return self->loop;
}

/**
 * gtk_video_set_loop:
 * @self: a #GtkVideo
 * @loop: whether media streams should loop
 *
 * Sets whether new files loaded by @self should be set to loop.
 **/
void
gtk_video_set_loop (GtkVideo *self,
                    gboolean  loop)
{
  g_return_if_fail (GTK_IS_VIDEO (self));

  if (self->loop == loop)
    return;

  self->loop = loop;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOOP]);
}

