/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkimageprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkiconhelperprivate.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkscalerprivate.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkimageaccessible.h"

#include <math.h>
#include <string.h>
#include <cairo-gobject.h>

/**
 * SECTION:gtkimage
 * @Short_description: A widget displaying an image
 * @Title: GtkImage
 * @SeeAlso: #GdkTexture
 *
 * The #GtkImage widget displays an image. Various kinds of object
 * can be displayed as an image; most typically, you would load a
 * #GdkTexture from a file, and then display that.
 * There’s a convenience function to do this, gtk_image_new_from_file(),
 * used as follows:
 * |[<!-- language="C" -->
 *   GtkWidget *image;
 *   image = gtk_image_new_from_file ("myfile.png");
 * ]|
 * If the file isn’t loaded successfully, the image will contain a
 * “broken image” icon similar to that used in many web browsers.
 * If you want to handle errors in loading the file yourself,
 * for example by displaying an error message, then load the image with
 * gdk_texture_new_from_file(), then create the #GtkImage with
 * gtk_image_new_from_paintable().
 *
 * Sometimes an application will want to avoid depending on external data
 * files, such as image files. See the documentation of #GResource for details.
 * In this case, the #GtkImage:resource, gtk_image_new_from_resource() and
 * gtk_image_set_from_resource() should be used.
 *
 * # CSS nodes
 *
 * GtkImage has a single CSS node with the name image. The style classes
 * .normal-icons or .large-icons may appear, depending on the #GtkImage:icon-size
 * property.
 */

typedef struct _GtkImageClass GtkImageClass;

struct _GtkImage
{
  GtkWidget parent_instance;
};

struct _GtkImageClass
{
  GtkWidgetClass parent_class;
};


typedef struct
{
  GtkIconHelper *icon_helper;
  GtkIconSize icon_size;

  float baseline_align;

  char *filename;
  char *resource_path;
} GtkImagePrivate;

static void gtk_image_snapshot             (GtkWidget    *widget,
                                            GtkSnapshot  *snapshot);
static void gtk_image_unrealize            (GtkWidget    *widget);
static void gtk_image_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int            for_size,
                               int           *minimum,
                               int           *natural,
                               int           *minimum_baseline,
                               int           *natural_baseline);

static void gtk_image_style_updated        (GtkWidget    *widget);
static void gtk_image_finalize             (GObject      *object);

static void gtk_image_set_property         (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void gtk_image_get_property         (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_FILE,
  PROP_ICON_SIZE,
  PROP_PIXEL_SIZE,
  PROP_ICON_NAME,
  PROP_STORAGE_TYPE,
  PROP_GICON,
  PROP_RESOURCE,
  PROP_USE_FALLBACK,
  NUM_PROPERTIES
};

static GParamSpec *image_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkImage, gtk_image, GTK_TYPE_WIDGET)

static void
gtk_image_class_init (GtkImageClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_image_set_property;
  gobject_class->get_property = gtk_image_get_property;
  gobject_class->finalize = gtk_image_finalize;

  widget_class = GTK_WIDGET_CLASS (class);
  widget_class->snapshot = gtk_image_snapshot;
  widget_class->measure = gtk_image_measure;
  widget_class->unrealize = gtk_image_unrealize;
  widget_class->style_updated = gtk_image_style_updated;

  image_props[PROP_PAINTABLE] =
      g_param_spec_object ("paintable",
                           P_("Paintable"),
                           P_("A GdkPaintable to display"),
                           GDK_TYPE_PAINTABLE,
                           GTK_PARAM_READWRITE);

  image_props[PROP_FILE] =
      g_param_spec_string ("file",
                           P_("Filename"),
                           P_("Filename to load and display"),
                           NULL,
                           GTK_PARAM_READWRITE);

  image_props[PROP_ICON_SIZE] =
      g_param_spec_enum ("icon-size",
                         P_("Icon size"),
                         P_("Symbolic size to use for icon set or named icon"),
                         GTK_TYPE_ICON_SIZE,
                         GTK_ICON_SIZE_INHERIT,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkImage:pixel-size:
   *
   * The "pixel-size" property can be used to specify a fixed size
   * overriding the #GtkImage:icon-size property for images of type
   * %GTK_IMAGE_ICON_NAME.
   */
  image_props[PROP_PIXEL_SIZE] =
      g_param_spec_int ("pixel-size",
                        P_("Pixel size"),
                        P_("Pixel size to use for named icon"),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkImage:icon-name:
   *
   * The name of the icon in the icon theme. If the icon theme is
   * changed, the image will be updated automatically.
   */
  image_props[PROP_ICON_NAME] =
      g_param_spec_string ("icon-name",
                           P_("Icon Name"),
                           P_("The name of the icon from the icon theme"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:gicon:
   *
   * The GIcon displayed in the GtkImage. For themed icons,
   * If the icon theme is changed, the image will be updated
   * automatically.
   */
  image_props[PROP_GICON] =
      g_param_spec_object ("gicon",
                           P_("Icon"),
                           P_("The GIcon being displayed"),
                           G_TYPE_ICON,
                           GTK_PARAM_READWRITE);

  /**
   * GtkImage:resource:
   *
   * A path to a resource file to display.
   */
  image_props[PROP_RESOURCE] =
      g_param_spec_string ("resource",
                           P_("Resource"),
                           P_("The resource path being displayed"),
                           NULL,
                           GTK_PARAM_READWRITE);

  image_props[PROP_STORAGE_TYPE] =
      g_param_spec_enum ("storage-type",
                         P_("Storage type"),
                         P_("The representation being used for image data"),
                         GTK_TYPE_IMAGE_TYPE,
                         GTK_IMAGE_EMPTY,
                         GTK_PARAM_READABLE);

  /**
   * GtkImage:use-fallback:
   *
   * Whether the icon displayed in the GtkImage will use
   * standard icon names fallback. The value of this property
   * is only relevant for images of type %GTK_IMAGE_ICON_NAME
   * and %GTK_IMAGE_GICON.
   */
  image_props[PROP_USE_FALLBACK] =
      g_param_spec_boolean ("use-fallback",
                            P_("Use Fallback"),
                            P_("Whether to use icon names fallback"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, image_props);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_IMAGE_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("image"));
}

static void
gtk_image_init (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkCssNode *widget_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (image));

  priv->icon_helper = gtk_icon_helper_new (widget_node, GTK_WIDGET (image));
}

static void
gtk_image_finalize (GObject *object)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  gtk_image_clear (image);

  g_clear_object (&priv->icon_helper);

  g_free (priv->filename);
  g_free (priv->resource_path);

  G_OBJECT_CLASS (gtk_image_parent_class)->finalize (object);
};

static void
gtk_image_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      gtk_image_set_from_paintable (image, g_value_get_object (value));
      break;
    case PROP_FILE:
      gtk_image_set_from_file (image, g_value_get_string (value));
      break;
    case PROP_ICON_SIZE:
      gtk_image_set_icon_size (image, g_value_get_enum (value));
      break;
    case PROP_PIXEL_SIZE:
      gtk_image_set_pixel_size (image, g_value_get_int (value));
      break;
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (image, g_value_get_string (value));
      break;
    case PROP_GICON:
      gtk_image_set_from_gicon (image, g_value_get_object (value));
      break;
    case PROP_RESOURCE:
      gtk_image_set_from_resource (image, g_value_get_string (value));
      break;

    case PROP_USE_FALLBACK:
      if (_gtk_icon_helper_set_use_fallback (priv->icon_helper, g_value_get_boolean (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_image_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, _gtk_icon_helper_peek_paintable (priv->icon_helper));
      break;
    case PROP_FILE:
      g_value_set_string (value, priv->filename);
      break;
    case PROP_ICON_SIZE:
      g_value_set_enum (value, priv->icon_size);
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_int (value, _gtk_icon_helper_get_pixel_size (priv->icon_helper));
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, _gtk_icon_helper_get_icon_name (priv->icon_helper));
      break;
    case PROP_GICON:
      g_value_set_object (value, _gtk_icon_helper_peek_gicon (priv->icon_helper));
      break;
    case PROP_RESOURCE:
      g_value_set_string (value, priv->resource_path);
      break;
    case PROP_USE_FALLBACK:
      g_value_set_boolean (value, _gtk_icon_helper_get_use_fallback (priv->icon_helper));
      break;
    case PROP_STORAGE_TYPE:
      g_value_set_enum (value, _gtk_icon_helper_get_storage_type (priv->icon_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * gtk_image_new_from_file:
 * @filename: (type filename): a filename
 * 
 * Creates a new #GtkImage displaying the file @filename. If the file
 * isn’t found or can’t be loaded, the resulting #GtkImage will
 * display a “broken image” icon. This function never returns %NULL,
 * it always returns a valid #GtkImage widget.
 *
 * If you need to detect failures to load the file, use
 * gdk_texture_new_from_file() to load the file yourself, then create
 * the #GtkImage from the texture.
 *
 * The storage type (gtk_image_get_storage_type()) of the returned
 * image is not defined, it will be whatever is appropriate for
 * displaying the file.
 * 
 * Returns: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_file   (const gchar *filename)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_file (image, filename);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_resource:
 * @resource_path: a resource path
 *
 * Creates a new #GtkImage displaying the resource file @resource_path. If the file
 * isn’t found or can’t be loaded, the resulting #GtkImage will
 * display a “broken image” icon. This function never returns %NULL,
 * it always returns a valid #GtkImage widget.
 *
 * If you need to detect failures to load the file, use
 * gdk_pixbuf_new_from_file() to load the file yourself, then create
 * the #GtkImage from the pixbuf.
 *
 * The storage type (gtk_image_get_storage_type()) of the returned
 * image is not defined, it will be whatever is appropriate for
 * displaying the file.
 *
 * Returns: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_resource (const gchar *resource_path)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_resource (image, resource_path);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_pixbuf:
 * @pixbuf: (allow-none): a #GdkPixbuf, or %NULL
 *
 * Creates a new #GtkImage displaying @pixbuf.
 * The #GtkImage does not assume a reference to the
 * pixbuf; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 *
 * This is a helper for gtk_image_new_from_texture(), and you can't
 * get back the exact pixbuf once this is called, only a texture.
 *
 * Note that this function just creates an #GtkImage from the pixbuf. The
 * #GtkImage created will not react to state changes. Should you want that, 
 * you should use gtk_image_new_from_icon_name().
 * 
 * Returns: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_pixbuf (image, pixbuf);

  return GTK_WIDGET (image);  
}

/**
 * gtk_image_new_from_paintable:
 * @paintable: (allow-none): a #GdkPaintable, or %NULL
 *
 * Creates a new #GtkImage displaying @paintable.
 * The #GtkImage does not assume a reference to the
 * paintable; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 *
 * The #GtkImage will track changes to the @paintable and update
 * its size and contents in response to it.
 *
 * Returns: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_paintable (GdkPaintable *paintable)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_paintable (image, paintable);

  return GTK_WIDGET (image);  
}

/**
 * gtk_image_new_from_icon_name:
 * @icon_name: (nullable): an icon name or %NULL
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Note: Before 3.94, this function was taking an extra icon size
 * argument. See gtk_image_set_icon_size() for another way to set
 * the icon size.
 *
 * Returns: a new #GtkImage displaying the themed icon
 **/
GtkWidget*
gtk_image_new_from_icon_name (const gchar *icon_name)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_icon_name (image, icon_name);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_gicon:
 * @icon: an icon
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Note: Before 3.94, this function was taking an extra icon size
 * argument. See gtk_image_set_icon_size() for another way to set
 * the icon size.
 *
 * Returns: a new #GtkImage displaying the themed icon
 **/
GtkWidget*
gtk_image_new_from_gicon (GIcon *icon)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_gicon (image, icon);

  return GTK_WIDGET (image);
}

typedef struct {
  GtkImage *image;
  gint scale_factor;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
			 gint             width,
			 gint             height,
			 gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  gint scale_factor;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable images */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale_factor = 1;
      return;
    }

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (loader_data->image));
  gdk_pixbuf_loader_set_size (loader, width * scale_factor, height * scale_factor);
  loader_data->scale_factor = scale_factor;
}

static GdkPixbufAnimation *
load_scalable_with_loader (GtkImage    *image,
			   const gchar *file_path,
			   const gchar *resource_path,
			   gint        *scale_factor_out)
{
  GdkPixbufLoader *loader;
  GBytes *bytes;
  char *contents;
  gsize length;
  gboolean res;
  GdkPixbufAnimation *animation;
  LoaderData loader_data;

  animation = NULL;
  bytes = NULL;

  loader = gdk_pixbuf_loader_new ();
  loader_data.image = image;

  g_signal_connect (loader, "size-prepared", G_CALLBACK (on_loader_size_prepared), &loader_data);

  if (resource_path != NULL)
    {
      bytes = g_resources_lookup_data (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    }
  else if (file_path != NULL)
    {
      res = g_file_get_contents (file_path, &contents, &length, NULL);
      if (res)
	bytes = g_bytes_new_take (contents, length);
    }
  else
    {
      g_assert_not_reached ();
    }

  if (!bytes)
    goto out;

  if (!gdk_pixbuf_loader_write_bytes (loader, bytes, NULL))
    goto out;

  if (!gdk_pixbuf_loader_close (loader, NULL))
    goto out;

  animation = gdk_pixbuf_loader_get_animation (loader);
  if (animation != NULL)
    {
      g_object_ref (animation);
      if (scale_factor_out != NULL)
	*scale_factor_out = loader_data.scale_factor;
    }

 out:
  gdk_pixbuf_loader_close (loader, NULL);
  g_object_unref (loader);
  g_bytes_unref (bytes);

  return animation;
}

/**
 * gtk_image_set_from_file:
 * @image: a #GtkImage
 * @filename: (type filename) (allow-none): a filename or %NULL
 *
 * See gtk_image_new_from_file() for details.
 **/
void
gtk_image_set_from_file   (GtkImage    *image,
                           const gchar *filename)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GdkPixbufAnimation *anim;
  gint scale_factor;
  GdkTexture *texture;
  GdkPaintable *scaler;

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  
  gtk_image_clear (image);

  if (filename == NULL)
    {
      priv->filename = NULL;
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  anim = load_scalable_with_loader (image, filename, NULL, &scale_factor);

  if (anim == NULL)
    {
      gtk_image_set_from_icon_name (image, "image-missing");
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_animation_get_static_image (anim));
  scaler = gtk_scaler_new (GDK_PAINTABLE (texture), scale_factor);

  gtk_image_set_from_paintable (image, scaler);

  g_object_unref (scaler);
  g_object_unref (texture);
  g_object_unref (anim);

  priv->filename = g_strdup (filename);

  g_object_thaw_notify (G_OBJECT (image));
}

#ifndef GDK_PIXBUF_MAGIC_NUMBER
#define GDK_PIXBUF_MAGIC_NUMBER (0x47646b50)    /* 'GdkP' */
#endif

static gboolean
resource_is_pixdata (const gchar *resource_path)
{
  const guint8 *stream;
  guint32 magic;
  gsize data_size;
  GBytes *bytes;
  gboolean ret = FALSE;

  bytes = g_resources_lookup_data (resource_path, 0, NULL);
  if (bytes == NULL)
    return FALSE;

  stream = g_bytes_get_data (bytes, &data_size);
  if (data_size < sizeof(guint32))
    goto out;

  magic = (stream[0] << 24) + (stream[1] << 16) + (stream[2] << 8) + stream[3];
  if (magic == GDK_PIXBUF_MAGIC_NUMBER)
    ret = TRUE;

out:
  g_bytes_unref (bytes);
  return ret;
}

/**
 * gtk_image_set_from_resource:
 * @image: a #GtkImage
 * @resource_path: (allow-none): a resource path or %NULL
 *
 * See gtk_image_new_from_resource() for details.
 **/
void
gtk_image_set_from_resource (GtkImage    *image,
			     const gchar *resource_path)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GdkPixbufAnimation *animation;
  gint scale_factor = 1;
  GdkTexture *texture;
  GdkPaintable *scaler;

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (resource_path == NULL)
    {
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  if (resource_is_pixdata (resource_path))
    {
      g_warning ("GdkPixdata format images are not supported, remove the \"to-pixdata\" option from your GResource files");
      animation = NULL;
    }
  else
    {
      animation = load_scalable_with_loader (image, NULL, resource_path, &scale_factor);
    }

  if (animation == NULL)
    {
      gtk_image_set_from_icon_name (image, "image-missing");
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_animation_get_static_image (animation));
  scaler = gtk_scaler_new (GDK_PAINTABLE (texture), scale_factor);

  gtk_image_set_from_paintable (image, scaler);

  g_object_unref (scaler);
  g_object_unref (texture);

  priv->resource_path = g_strdup (resource_path);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_RESOURCE]);

  g_object_unref (animation);

  g_object_thaw_notify (G_OBJECT (image));
}


/**
 * gtk_image_set_from_pixbuf:
 * @image: a #GtkImage
 * @pixbuf: (allow-none): a #GdkPixbuf or %NULL
 *
 * See gtk_image_new_from_pixbuf() for details.
 *
 * Note: This is a helper for gtk_image_set_from_paintable(), and you can't
 * get back the exact pixbuf once this is called, only a paintable.
 **/
void
gtk_image_set_from_pixbuf (GtkImage  *image,
                           GdkPixbuf *pixbuf)
{
  GdkTexture *texture;

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  if (pixbuf)
    texture = gdk_texture_new_for_pixbuf (pixbuf);
  else
    texture = NULL;

  gtk_image_set_from_paintable (image, GDK_PAINTABLE (texture));

  if (texture)
    g_object_unref (texture);
}

/**
 * gtk_image_set_from_icon_name:
 * @image: a #GtkImage
 * @icon_name: (nullable): an icon name or %NULL
 *
 * See gtk_image_new_from_icon_name() for details.
 *
 * Note: Before 3.94, this function was taking an extra icon size
 * argument. See gtk_image_set_icon_size() for another way to set
 * the icon size.
 **/
void
gtk_image_set_from_icon_name  (GtkImage    *image,
			       const gchar *icon_name)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (icon_name)
    _gtk_icon_helper_set_icon_name (priv->icon_helper, icon_name);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_NAME]);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_gicon:
 * @image: a #GtkImage
 * @icon: an icon
 *
 * See gtk_image_new_from_gicon() for details.
 *
 * Note: Before 3.94, this function was taking an extra icon size
 * argument. See gtk_image_set_icon_size() for another way to set
 * the icon size.
 **/
void
gtk_image_set_from_gicon  (GtkImage       *image,
			   GIcon          *icon)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  if (icon)
    g_object_ref (icon);

  gtk_image_clear (image);

  if (icon)
    {
      _gtk_icon_helper_set_gicon (priv->icon_helper, icon);
      g_object_unref (icon);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_GICON]);
  
  g_object_thaw_notify (G_OBJECT (image));
}

static void
gtk_image_paintable_invalidate_contents (GdkPaintable *paintable,
                                         GtkImage     *image)
{
  gtk_widget_queue_draw (GTK_WIDGET (image));
}

static void
gtk_image_paintable_invalidate_size (GdkPaintable *paintable,
                                     GtkImage     *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  gtk_icon_helper_invalidate (priv->icon_helper);
}

/**
 * gtk_image_set_from_paintable:
 * @image: a #GtkImage
 * @paintable: (nullable): a #GdkPaintable or %NULL
 *
 * See gtk_image_new_from_paintable() for details.
 **/
void
gtk_image_set_from_paintable (GtkImage     *image,
			      GdkPaintable *paintable)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  g_object_freeze_notify (G_OBJECT (image));

  if (paintable)
    g_object_ref (paintable);

  gtk_image_clear (image);

  if (paintable)
    {
      const guint flags = gdk_paintable_get_flags (paintable);

      _gtk_icon_helper_set_paintable (priv->icon_helper, paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_connect (paintable,
                          "invalidate-contents",
                          G_CALLBACK (gtk_image_paintable_invalidate_contents),
                          image);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_connect (paintable,
                          "invalidate-size",
                          G_CALLBACK (gtk_image_paintable_invalidate_size),
                          image);
      g_object_unref (paintable);
    }

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PAINTABLE]);
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_get_storage_type:
 * @image: a #GtkImage
 * 
 * Gets the type of representation being used by the #GtkImage
 * to store image data. If the #GtkImage has no image data,
 * the return value will be %GTK_IMAGE_EMPTY.
 * 
 * Returns: image representation being used
 **/
GtkImageType
gtk_image_get_storage_type (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_IMAGE_EMPTY);

  return _gtk_icon_helper_get_storage_type (priv->icon_helper);
}

/**
 * gtk_image_get_paintable:
 * @image: a #GtkImage
 *
 * Gets the image #GdkPaintable being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PAINTABLE (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned paintable.
 * 
 * Returns: (nullable) (transfer none): the displayed paintable, or %NULL if
 *   the image is empty
 **/
GdkPaintable *
gtk_image_get_paintable (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_paintable (priv->icon_helper);
}

/**
 * gtk_image_get_icon_name:
 * @image: a #GtkImage
 *
 * Gets the icon name and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_NAME (see gtk_image_get_storage_type()).
 * The returned string is owned by the #GtkImage and should not
 * be freed.
 *
 * Note: This function was changed in 3.94 not to use out parameters
 * anymore, but return the icon name directly. See gtk_image_get_icon_size()
 * for a way to get the icon size.
 *
 * Returns: (transfer none) (allow-none): the icon name, or %NULL
 **/
const gchar *
gtk_image_get_icon_name (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_get_icon_name (priv->icon_helper);
}

/**
 * gtk_image_get_gicon:
 * @image: a #GtkImage
 *
 * Gets the #GIcon and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_GICON (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned #GIcon.
 *
 * Note: This function was changed in 3.94 not to use out parameters
 * anymore, but return the GIcon directly. See gtk_image_get_icon_size()
 * for a way to get the icon size.
 *
 * Returns: (transfer none) (allow-none): a #GIcon, or %NULL
 **/
GIcon *
gtk_image_get_gicon (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_gicon (priv->icon_helper);
}

/**
 * gtk_image_new:
 * 
 * Creates a new empty #GtkImage widget.
 * 
 * Returns: a newly created #GtkImage widget. 
 **/
GtkWidget*
gtk_image_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE, NULL);
}

static void
gtk_image_unrealize (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  gtk_icon_helper_invalidate (priv->icon_helper);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->unrealize (widget);
}

static float
gtk_image_get_baseline_align (GtkImage *image)
{
  PangoContext *pango_context;
  PangoFontMetrics *metrics;
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);


  if (priv->baseline_align == 0.0)
    {
      pango_context = gtk_widget_get_pango_context (GTK_WIDGET (image));
      metrics = pango_context_get_metrics (pango_context,
					   pango_context_get_font_description (pango_context),
					   pango_context_get_language (pango_context));
      priv->baseline_align =
                (float)pango_font_metrics_get_ascent (metrics) /
                (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));

      pango_font_metrics_unref (metrics);
    }

  return priv->baseline_align;
}

static void
gtk_image_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  double ratio;
  int x, y, width, height, baseline;
  double w, h;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  ratio = gdk_paintable_get_intrinsic_aspect_ratio (GDK_PAINTABLE (priv->icon_helper));

  if (ratio == 0)
    {
      gdk_paintable_snapshot (GDK_PAINTABLE (priv->icon_helper), snapshot, width, height);
    }
  else
    {
      double image_ratio = (double) width / height;

      if (ratio > image_ratio)
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

      baseline = gtk_widget_get_allocated_baseline (widget);
      if (baseline == -1)
        y = floor(height - ceil (h)) / 2;
      else
        y = CLAMP (baseline - h * gtk_image_get_baseline_align (image), 0, height - ceil (h));

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gdk_paintable_snapshot (GDK_PAINTABLE (priv->icon_helper), snapshot, w, h);
      gtk_snapshot_restore (snapshot);
    }
}

static void
gtk_image_notify_for_storage_type (GtkImage     *image,
                                   GtkImageType  storage_type)
{
  switch (storage_type)
    {
    case GTK_IMAGE_ICON_NAME:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_NAME]);
      break;
    case GTK_IMAGE_GICON:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_GICON]);
      break;
    case GTK_IMAGE_PAINTABLE:
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PAINTABLE]);
      break;
    case GTK_IMAGE_EMPTY:
    default:
      break;
    }
}

void
gtk_image_set_from_definition (GtkImage           *image,
                               GtkImageDefinition *def)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  
  gtk_image_clear (image);

  if (def != NULL)
    {
      _gtk_icon_helper_set_definition (priv->icon_helper, def);

      gtk_image_notify_for_storage_type (image, gtk_image_definition_get_storage_type (def));
    }

  g_object_thaw_notify (G_OBJECT (image));
}

GtkImageDefinition *
gtk_image_get_definition (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  return gtk_icon_helper_get_definition (priv->icon_helper);
}

/**
 * gtk_image_clear:
 * @image: a #GtkImage
 *
 * Resets the image to be empty.
 */
void
gtk_image_clear (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkImageType storage_type;

  g_object_freeze_notify (G_OBJECT (image));
  storage_type = gtk_image_get_storage_type (image);

  if (storage_type != GTK_IMAGE_EMPTY)
    g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_STORAGE_TYPE]);

  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_SIZE]);

  gtk_image_notify_for_storage_type (image, storage_type);

  if (priv->filename)
    {
      g_free (priv->filename);
      priv->filename = NULL;
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_FILE]);
    }

  if (priv->resource_path)
    {
      g_free (priv->resource_path);
      priv->resource_path = NULL;
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_RESOURCE]);
    }

  if (storage_type == GTK_IMAGE_PAINTABLE)
    {
      GdkPaintable *paintable = _gtk_icon_helper_peek_paintable (priv->icon_helper);
      const guint flags = gdk_paintable_get_flags (paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_handlers_disconnect_by_func (paintable,
                                              gtk_image_paintable_invalidate_contents,
                                              image);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_handlers_disconnect_by_func (paintable,
                                              gtk_image_paintable_invalidate_size,
                                              image);
    }

  _gtk_icon_helper_clear (priv->icon_helper);

  g_object_thaw_notify (G_OBJECT (image));
}

static void
gtk_image_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int            for_size,
                   int           *minimum,
                   int           *natural,
                   int           *minimum_baseline,
                   int           *natural_baseline)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (GTK_IMAGE (widget));
  float baseline_align;

  *minimum = *natural = gtk_icon_helper_get_size (priv->icon_helper);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      baseline_align = gtk_image_get_baseline_align (GTK_IMAGE (widget));
      if (minimum_baseline)
        *minimum_baseline = *minimum * baseline_align;
      if (natural_baseline)
        *natural_baseline = *natural * baseline_align;
    }
}

static void
gtk_image_style_updated (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkCssStyleChange *change = gtk_style_context_get_change (context);

  gtk_icon_helper_invalidate_for_change (priv->icon_helper, change);

  GTK_WIDGET_CLASS (gtk_image_parent_class)->style_updated (widget);

  priv->baseline_align = 0.0;
}

/**
 * gtk_image_set_pixel_size:
 * @image: a #GtkImage
 * @pixel_size: the new pixel size
 * 
 * Sets the pixel size to use for named icons. If the pixel size is set
 * to a value != -1, it is used instead of the icon size set by
 * gtk_image_set_from_icon_name().
 */
void 
gtk_image_set_pixel_size (GtkImage *image,
			  gint      pixel_size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  if (_gtk_icon_helper_set_pixel_size (priv->icon_helper, pixel_size))
    {
      if (gtk_widget_get_visible (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_PIXEL_SIZE]);
    }
}

/**
 * gtk_image_get_pixel_size:
 * @image: a #GtkImage
 * 
 * Gets the pixel size used for named icons.
 *
 * Returns: the pixel size used for named icons.
 */
gint
gtk_image_get_pixel_size (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), -1);

  return _gtk_icon_helper_get_pixel_size (priv->icon_helper);
}

/**
 * gtk_image_set_icon_size:
 * @image: a #GtkImage
 * @icon_size: the new icon size
 *
 * Suggests an icon size to the theme for named icons.
 */
void
gtk_image_set_icon_size (GtkImage    *image,
			 GtkIconSize  icon_size)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_if_fail (GTK_IS_IMAGE (image));

  if (priv->icon_size == icon_size)
    return;

  priv->icon_size = icon_size;
  gtk_icon_size_set_style_classes (gtk_widget_get_css_node (GTK_WIDGET (image)), icon_size);
  g_object_notify_by_pspec (G_OBJECT (image), image_props[PROP_ICON_SIZE]);
}

/**
 * gtk_image_get_icon_size:
 * @image: a #GtkImage
 *
 * Gets the icon size used by the @image when rendering icons.
 *
 * Returns: the image size used by icons
 **/
GtkIconSize
gtk_image_get_icon_size (GtkImage *image)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_ICON_SIZE_INHERIT);

  return priv->icon_size;
}

void
gtk_image_get_image_size (GtkImage *image,
                          int      *width,
                          int      *height)
{
  GtkImagePrivate *priv = gtk_image_get_instance_private (image);

  *width = *height = gtk_icon_helper_get_size (priv->icon_helper);
}
