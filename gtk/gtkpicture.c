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

#include "gtkpicture.h"

#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkscalerprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkpictureaccessibleprivate.h"

/**
 * SECTION:gtkpicture
 * @Short_description: A widget displaying a #GdkPaintable
 * @Title: GtkPicture
 * @SeeAlso: #GdkPaintable, #GtkImage
 *
 * The #GtkPicture widget displays a #GdkPaintable. Many convenience functions
 * are provided to make pictures simple to use. For example, if you want to load
 * an image from a file, and then display that, there’s a convenience function
 * to do this:
 * |[<!-- language="C" -->
 *   GtkWidget *widget;
 *   widget = gtk_picture_new_for_filename ("myfile.png");
 * ]|
 * If the file isn’t loaded successfully, the picture will contain a
 * “broken image” icon similar to that used in many web browsers.
 * If you want to handle errors in loading the file yourself,
 * for example by displaying an error message, then load the image with
 * gdk_texture_new_from_file(), then create the #GtkPicture with
 * gtk_picture_new_for_paintable().
 *
 * Sometimes an application will want to avoid depending on external data
 * files, such as image files. See the documentation of #GResource for details.
 * In this case, gtk_picture_new_for_resource() and gtk_picture_set_resource()
 * should be used.
 *
 * # CSS nodes
 *
 * GtkPicture has a single CSS node with the name picture.
 */

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_FILE,
  PROP_ALTERNATIVE_TEXT,
  PROP_KEEP_ASPECT_RATIO,
  PROP_CAN_SHRINK,
  NUM_PROPERTIES
};

struct _GtkPicture
{
  GtkWidget parent_instance;

  GdkPaintable *paintable;
  GFile *file;

  char *alternative_text;
  guint keep_aspect_ratio : 1;
  guint can_shrink : 1;
};

struct _GtkPictureClass
{
  GtkWidgetClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkPicture, gtk_picture, GTK_TYPE_WIDGET)

static void
gtk_picture_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkPicture *self = GTK_PICTURE (widget);
  double ratio;
  int x, y, width, height;
  double w, h;

  if (self->paintable == NULL)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  ratio = gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);

  if (!self->keep_aspect_ratio || ratio == 0)
    {
      gdk_paintable_snapshot (self->paintable, snapshot, width, height);
    }
  else
    {
      double picture_ratio = (double) width / height;

      if (ratio > picture_ratio)
        {
          w = width;
          h = width / ratio;
        }
      else
        {
          w = height * ratio;
          h = height;
        }

      x = (width - ceil (w)) / 2;
      y = floor(height - ceil (h)) / 2;

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gdk_paintable_snapshot (self->paintable, snapshot, w, h);
      gtk_snapshot_restore (snapshot);
    }
}

static GtkSizeRequestMode
gtk_picture_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_picture_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int            for_size,
                     int           *minimum,
                     int           *natural,
                     int           *minimum_baseline,
                     int           *natural_baseline)
{
  GtkPicture *self = GTK_PICTURE (widget);
  double min_width, min_height, nat_width, nat_height;
  double default_size;

  if (self->paintable == NULL)
    {
      *minimum = 0;
      *natural = 0;
      return;
    }

  default_size = _gtk_css_number_value_get (gtk_css_style_get_value (gtk_css_node_get_style (gtk_widget_get_css_node (widget)), GTK_CSS_PROPERTY_ICON_SIZE), 100);

  if (self->can_shrink)
    {
      min_width = min_height = 0;
    }
  else
    {
      gdk_paintable_compute_concrete_size (self->paintable,
                                           0, 0,
                                           default_size, default_size,
                                           &min_width, &min_height);
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gdk_paintable_compute_concrete_size (self->paintable,
                                           0,
                                           for_size < 0 ? 0 : for_size,
                                           default_size, default_size,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_width);
      *natural = ceil (nat_width);
    }
  else
    {
      gdk_paintable_compute_concrete_size (self->paintable,
                                           for_size < 0 ? 0 : for_size,
                                           0,
                                           default_size, default_size,
                                           &nat_width, &nat_height);
      *minimum = ceil (min_height);
      *natural = ceil (nat_height);
    }
}

static void
gtk_picture_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkPicture *self = GTK_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      gtk_picture_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      gtk_picture_set_file (self, g_value_get_object (value));
      break;

    case PROP_ALTERNATIVE_TEXT:
      gtk_picture_set_alternative_text (self, g_value_get_string (value));
      break;

    case PROP_KEEP_ASPECT_RATIO:
      gtk_picture_set_keep_aspect_ratio (self, g_value_get_boolean (value));
      break;

    case PROP_CAN_SHRINK:
      gtk_picture_set_can_shrink (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_picture_get_property (GObject     *object,
                          guint        prop_id,
                          GValue      *value,
                          GParamSpec  *pspec)
{
  GtkPicture *self = GTK_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_ALTERNATIVE_TEXT:
      g_value_set_string (value, self->alternative_text);
      break;

    case PROP_KEEP_ASPECT_RATIO:
      g_value_set_boolean (value, self->keep_aspect_ratio);
      break;

    case PROP_CAN_SHRINK:
      g_value_set_boolean (value, self->can_shrink);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_picture_dispose (GObject *object)
{
  GtkPicture *self = GTK_PICTURE (object);

  gtk_picture_set_paintable (self, NULL);

  g_clear_object (&self->file);

  G_OBJECT_CLASS (gtk_picture_parent_class)->dispose (object);
};

static void
gtk_picture_class_init (GtkPictureClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->set_property = gtk_picture_set_property;
  gobject_class->get_property = gtk_picture_get_property;
  gobject_class->dispose = gtk_picture_dispose;

  widget_class->snapshot = gtk_picture_snapshot;
  widget_class->get_request_mode = gtk_picture_get_request_mode;
  widget_class->measure = gtk_picture_measure;

  /**
   * GtkPicture:paintable:
   *
   * The #GdkPaintable to be displayed by this #GtkPicture.
   */
  properties[PROP_PAINTABLE] =
      g_param_spec_object ("paintable",
                           P_("Paintable"),
                           P_("The GdkPaintable to display"),
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:file:
   *
   * The #GFile that is displayed or %NULL if none.
   */
  properties[PROP_FILE] =
      g_param_spec_object ("file",
                           P_("File"),
                           P_("File to load and display"),
                           G_TYPE_FILE,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:alternative-text:
   *
   * The alternative textual description for the picture.
   */
  properties[PROP_ALTERNATIVE_TEXT] =
      g_param_spec_string ("alternative-text",
                           P_("Alternative text"),
                           P_("The alternative textual description"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:keep-aspect-ratio:
   *
   * Whether the GtkPicture will render its contents trying to preserve the aspect
   * ratio of the contents.
   */
  properties[PROP_KEEP_ASPECT_RATIO] =
      g_param_spec_boolean ("keep-aspect-ratio",
                            P_("Keep aspect ratio"),
                            P_("Render contents respecting the aspect ratio"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPicture:can-shrink
   *
   * If the #GtkPicture can be made smaller than the self it contains.
   */
  properties[PROP_CAN_SHRINK] =
      g_param_spec_boolean ("can-shrink",
                            P_("Can shrink"),
                            P_("Allow self to be smaller than contents"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_PICTURE_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("picture"));
}

static void
gtk_picture_init (GtkPicture *self)
{
  self->can_shrink = TRUE;
  self->keep_aspect_ratio = TRUE;
}

/**
 * gtk_picture_new:
 *
 * Creates a new empty #GtkPicture widget.
 *
 * Returns: a newly created #GtkPicture widget.
 **/
GtkWidget*
gtk_picture_new (void)
{
  return g_object_new (GTK_TYPE_PICTURE, NULL);
}

/**
 * gtk_picture_new_for_paintable:
 * @paintable: (nullable): a #GdkPaintable, or %NULL
 *
 * Creates a new #GtkPicture displaying @paintable.
 *
 * The #GtkPicture will track changes to the @paintable and update
 * its size and contents in response to it.
 *
 * Returns: a new #GtkPicture
 **/
GtkWidget*
gtk_picture_new_for_paintable (GdkPaintable *paintable)
{
  g_return_val_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable), NULL);

  return g_object_new (GTK_TYPE_PICTURE,
                       "paintable", paintable,
                       NULL);
}

/**
 * gtk_picture_new_for_pixbuf:
 * @pixbuf: (nullable): a #GdkPixbuf, or %NULL
 *
 * Creates a new #GtkPicture displaying @pixbuf.
 *
 * This is a utility function that calls gtk_picture_new_for_paintable(),
 * See that function for details.
 *
 * The pixbuf must not be modified after passing it to this function.
 *
 * Returns: a new #GtkPicture
 **/
GtkWidget*
gtk_picture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GtkWidget *result;
  GdkPaintable *paintable;

  g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), NULL);

  if (pixbuf)
    paintable = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf));
  else
    paintable = NULL;

  result = gtk_picture_new_for_paintable (paintable);

  if (paintable)
    g_object_unref (paintable);

  return result;
}

/**
 * gtk_picture_new_for_file:
 * @file: (nullable): a #GFile
 * 
 * Creates a new #GtkPicture displaying the given @file. If the file
 * isn’t found or can’t be loaded, the resulting #GtkPicture be empty.
 *
 * If you need to detect failures to load the file, use
 * gdk_texture_new_for_file() to load the file yourself, then create
 * the #GtkPicture from the texture.
 *
 * Returns: a new #GtkPicture
 **/
GtkWidget*
gtk_picture_new_for_file (GFile *file)
{
  g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);

  return g_object_new (GTK_TYPE_PICTURE,
                       "file", file,
                       NULL);
}

/**
 * gtk_picture_new_for_filename:
 * @filename: (type filename) (nullable): a filename
 *
 * Creates a new #GtkPicture displaying the file @filename.
 *
 * This is a utility function that calls gtk_picture_new_for_file().
 * See that function for details.
 *
 * Returns: a new #GtkPicture
 **/
GtkWidget*
gtk_picture_new_for_filename (const char *filename)
{
  GtkWidget *result;
  GFile *file;

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  result = gtk_picture_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

/**
 * gtk_picture_new_for_resource:
 * @resource_path: (nullable): resource path to play back
 *
 * Creates a new #GtkPicture displaying the file @filename.
 *
 * This is a utility function that calls gtk_picture_new_for_file().
 * See that function for details.
 *
 * Returns: a new #GtkPicture
 **/
GtkWidget *
gtk_picture_new_for_resource (const char *resource_path)
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

  result = gtk_picture_new_for_file (file);

  if (file)
    g_object_unref (file);

  return result;
}

typedef struct {
  gint scale_factor;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
			 gint             width,
			 gint             height,
			 gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable pictures */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale_factor = 1;
      return;
    }

  gdk_pixbuf_loader_set_size (loader,
                              width * loader_data->scale_factor,
                              height * loader_data->scale_factor);
}

static GdkPaintable *
load_scalable_with_loader (GFile *file,
                           gint   scale_factor)
{
  GdkPixbufLoader *loader;
  GBytes *bytes;
  GdkPixbufAnimation *animation;
  GdkPaintable *result, *scaler;
  LoaderData loader_data;

  result = NULL;

  loader = gdk_pixbuf_loader_new ();
  loader_data.scale_factor = scale_factor;

  g_signal_connect (loader, "size-prepared", G_CALLBACK (on_loader_size_prepared), &loader_data);

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (bytes == NULL)
    goto out1;

  if (!gdk_pixbuf_loader_write_bytes (loader, bytes, NULL))
    goto out2;

  if (!gdk_pixbuf_loader_close (loader, NULL))
    goto out2;

  animation = gdk_pixbuf_loader_get_animation (loader);
  if (animation == NULL)
    goto out2;

  result = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (gdk_pixbuf_animation_get_static_image (animation)));
  scaler = gtk_scaler_new (result, loader_data.scale_factor);
  g_object_unref (result);
  result = scaler;

out2:
  g_bytes_unref (bytes);
out1:
  gdk_pixbuf_loader_close (loader, NULL);
  g_object_unref (loader);

  return result;
}

/**
 * gtk_picture_set_file:
 * @self: a #GtkPicture
 * @file: (nullable): a %GFile or %NULL
 *
 * Makes @self load and display @file.
 *
 * See gtk_picture_new_for_file() for details.
 **/
void
gtk_picture_set_file (GtkPicture *self,
                      GFile      *file)
{
  GdkPaintable *paintable;

  g_return_if_fail (GTK_IS_PICTURE (self));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (self->file == file)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_set_object (&self->file, file);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);

  paintable = load_scalable_with_loader (file, gtk_widget_get_scale_factor (GTK_WIDGET (self)));
  gtk_picture_set_paintable (self, paintable);
  g_clear_object (&paintable);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_picture_get_file:
 * @self: a #GtkPicture
 *
 * Gets the #GFile currently displayed if @self is displaying a file.
 * If @self is not displaying a file, for example when gtk_picture_set_paintable()
 * was used, then %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The #GFile displayed by @self.
 **/
GFile *
gtk_picture_get_file (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), FALSE);

  return self->file;
}

/**
 * gtk_picture_set_filename:
 * @self: a #GtkPicture
 * @filename: (nullable): the filename to play
 *
 * Makes @self load and display the given @filename.
 *
 * This is a utility function that calls gtk_picture_set_file().
 **/
void
gtk_picture_set_filename (GtkPicture *self,
                          const char *filename)
{
  GFile *file;

  g_return_if_fail (GTK_IS_PICTURE (self));

  if (filename)
    file = g_file_new_for_path (filename);
  else
    file = NULL;

  gtk_picture_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_picture_set_resource:
 * @self: a #GtkPicture
 * @resource_path: (nullable): the resource to set
 *
 * Makes @self load and display the resource at the given
 * @resource_path.
 *
 * This is a utility function that calls gtk_picture_set_file(),
 **/
void
gtk_picture_set_resource (GtkPicture *self,
                          const char *resource_path)
{
  GFile *file;

  g_return_if_fail (GTK_IS_PICTURE (self));

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

  gtk_picture_set_file (self, file);

  if (file)
    g_object_unref (file);
}

/**
 * gtk_picture_set_pixbuf:
 * @self: a #GtkPicture
 * @pixbuf: (nullable): a #GdkPixbuf or %NULL
 *
 * See gtk_picture_new_for_pixbuf() for details.
 *
 * This is a utility function that calls gtk_picture_set_paintable(),
 **/
void
gtk_picture_set_pixbuf (GtkPicture *self,
                        GdkPixbuf  *pixbuf)
{
  GdkTexture *texture;

  g_return_if_fail (GTK_IS_PICTURE (self));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf)
    texture = gdk_texture_new_for_pixbuf (pixbuf);
  else
    texture = NULL;

  gtk_picture_set_paintable (self, GDK_PAINTABLE (texture));

  if (texture)
    g_object_unref (texture);
}

static void
gtk_picture_paintable_invalidate_contents (GdkPaintable *paintable,
                                           GtkPicture   *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_picture_paintable_invalidate_size (GdkPaintable *paintable,
                                       GtkPicture   *self)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_picture_set_paintable:
 * @self: a #GtkPicture
 * @paintable: (nullable): a #GdkPaintable or %NULL
 *
 * Makes @self display the given @paintable. If @paintable is %NULL,
 * nothing will be displayed.
 *
 * See gtk_picture_new_for_paintable() for details.
 **/
void
gtk_picture_set_paintable (GtkPicture   *self,
                           GdkPaintable *paintable)
{
  g_return_if_fail (GTK_IS_PICTURE (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (self->paintable == paintable)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (paintable)
    g_object_ref (paintable);

  if (self->paintable)
    {
      const guint flags = gdk_paintable_get_flags (self->paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_handlers_disconnect_by_func (self->paintable,
                                              gtk_picture_paintable_invalidate_contents,
                                              self);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_handlers_disconnect_by_func (self->paintable,
                                              gtk_picture_paintable_invalidate_size,
                                              self);
    }

  self->paintable = paintable;

  if (paintable)
    {
      const guint flags = gdk_paintable_get_flags (paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_connect (paintable,
                          "invalidate-contents",
                          G_CALLBACK (gtk_picture_paintable_invalidate_contents),
                          self);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_connect (paintable,
                          "invalidate-size",
                          G_CALLBACK (gtk_picture_paintable_invalidate_size),
                          self);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_picture_get_paintable:
 * @self: a #GtkPicture
 *
 * Gets the #GdkPaintable being displayed by the #GtkPicture.
 *
 * Returns: (nullable) (transfer none): the displayed paintable, or %NULL if
 *   the picture is empty
 **/
GdkPaintable *
gtk_picture_get_paintable (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), NULL);

  return self->paintable;
}

/**
 * gtk_picture_set_keep_aspect_ratio:
 * @self: a #GtkPicture
 * @keep_aspect_ratio: whether to keep aspect ratio
 *
 * If set to %TRUE, the @self will render its contents according to
 * their aspect ratio. That means that empty space may show up at the
 * top/bottom or left/right of @self.
 *
 * If set to %FALSE or if the contents provide no aspect ratio, the
 * contents will be stretched over the picture's whole area.
 */
void
gtk_picture_set_keep_aspect_ratio (GtkPicture *self,
                                   gboolean    keep_aspect_ratio)
{
  g_return_if_fail (GTK_IS_PICTURE (self));

  if (self->keep_aspect_ratio == keep_aspect_ratio)
    return;

  self->keep_aspect_ratio = keep_aspect_ratio;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEEP_ASPECT_RATIO]);
}

/**
 * gtk_picture_get_keep_aspect_ratio:
 * @self: a #GtkPicture
 *
 * Gets the value set via gtk_picture_set_keep_aspect_ratio().
 *
 * Returns: %TRUE if the self tries to keep the contents' aspect ratio
 **/
gboolean
gtk_picture_get_keep_aspect_ratio (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), TRUE);

  return self->keep_aspect_ratio;
}

/**
 * gtk_picture_set_can_shrink:
 * @self: a #GtkPicture
 * @can_shrink: if @self can be made smaller than its contents
 *
 * If set to %TRUE, the @self can be made smaller than its contents.
 * The contents will then be scaled down when rendering.
 *
 * If you want to still force a minimum size manually, consider using
 * gtk_widget_set_size_request().
 *
 * Also of note is that a similar function for growing does not exist
 * because the grow behavior can be controlled via
 * gtk_widget_set_halign() and gtk_widget_set_valign().
 */
void
gtk_picture_set_can_shrink (GtkPicture *self,
                            gboolean    can_shrink)
{
  g_return_if_fail (GTK_IS_PICTURE (self));

  if (self->can_shrink == can_shrink)
    return;

  self->can_shrink = can_shrink;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_SHRINK]);
}

/**
 * gtk_picture_get_can_shrink:
 * @self: a #GtkPicture
 *
 * Gets the value set via gtk_picture_set_can_shrink().
 *
 * Returns: %TRUE if the picture can be made smaller than its contents
 **/
gboolean
gtk_picture_get_can_shrink (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), FALSE);

  return self->can_shrink;
}

/**
 * gtk_picture_set_alternative_text:
 * @self: a #GtkPicture
 * @alternative_text: (nullable): a textual description of the contents
 *
 * Sets an alternative textual description for the picture contents.
 * It is equivalent to the "alt" attribute for images on websites.
 *
 * This text will be made available to accessibility tools.
 *
 * If the picture cannot be described textually, set this property to %NULL.
 */
void
gtk_picture_set_alternative_text (GtkPicture *self,
                                  const char *alternative_text)
{
  g_return_if_fail (GTK_IS_PICTURE (self));

  if (g_strcmp0 (self->alternative_text, alternative_text) == 0)
    return;

  g_free (self->alternative_text);
  self->alternative_text = g_strdup (alternative_text);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALTERNATIVE_TEXT]);
}

/**
 * gtk_picture_get_alternative_text:
 * @self: a #GtkPicture
 *
 * Gets the alternative textual description of the picture or returns %NULL if
 * the picture cannot be described textually.
 *
 * Returns: (nullable) (transfer none): the alternative textual description
 *     of @self.
 **/
const char *
gtk_picture_get_alternative_text (GtkPicture *self)
{
  g_return_val_if_fail (GTK_IS_PICTURE (self), NULL);

  return self->alternative_text;
}

