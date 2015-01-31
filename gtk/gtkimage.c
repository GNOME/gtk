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

#include <math.h>
#include <string.h>
#include <cairo-gobject.h>

#include "gtkcontainer.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkiconhelperprivate.h"
#include "gtkimageprivate.h"
#include "deprecated/gtkiconfactory.h"
#include "deprecated/gtkstock.h"
#include "gtkicontheme.h"
#include "gtksizerequest.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkimageaccessible.h"

/**
 * SECTION:gtkimage
 * @Short_description: A widget displaying an image
 * @Title: GtkImage
 * @See_also:#GdkPixbuf
 *
 * The #GtkImage widget displays an image. Various kinds of object
 * can be displayed as an image; most typically, you would load a
 * #GdkPixbuf ("pixel buffer") from a file, and then display that.
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
 * gdk_pixbuf_new_from_file(), then create the #GtkImage with
 * gtk_image_new_from_pixbuf().
 *
 * The image file may contain an animation, if so the #GtkImage will
 * display an animation (#GdkPixbufAnimation) instead of a static image.
 *
 * #GtkImage is a subclass of #GtkMisc, which implies that you can
 * align it (center, left, right) and add padding to it, using
 * #GtkMisc methods.
 *
 * #GtkImage is a “no window” widget (has no #GdkWindow of its own),
 * so by default does not receive events. If you want to receive events
 * on the image, such as button clicks, place the image inside a
 * #GtkEventBox, then connect to the event signals on the event box.
 *
 * ## Handling button press events on a #GtkImage.
 *
 * |[<!-- language="C" -->
 *   static gboolean
 *   button_press_callback (GtkWidget      *event_box,
 *                          GdkEventButton *event,
 *                          gpointer        data)
 *   {
 *     g_print ("Event box clicked at coordinates %f,%f\n",
 *              event->x, event->y);
 *
 *     // Returning TRUE means we handled the event, so the signal
 *     // emission should be stopped (don’t call any further callbacks
 *     // that may be connected). Return FALSE to continue invoking callbacks.
 *     return TRUE;
 *   }
 *
 *   static GtkWidget*
 *   create_image (void)
 *   {
 *     GtkWidget *image;
 *     GtkWidget *event_box;
 *
 *     image = gtk_image_new_from_file ("myfile.png");
 *
 *     event_box = gtk_event_box_new ();
 *
 *     gtk_container_add (GTK_CONTAINER (event_box), image);
 *
 *     g_signal_connect (G_OBJECT (event_box),
 *                       "button_press_event",
 *                       G_CALLBACK (button_press_callback),
 *                       image);
 *
 *     return image;
 *   }
 * ]|
 *
 * When handling events on the event box, keep in mind that coordinates
 * in the image may be different from event box coordinates due to
 * the alignment and padding settings on the image (see #GtkMisc).
 * The simplest way to solve this is to set the alignment to 0.0
 * (left/top), and set the padding to zero. Then the origin of
 * the image will be the same as the origin of the event box.
 *
 * Sometimes an application will want to avoid depending on external data
 * files, such as image files. GTK+ comes with a program to avoid this,
 * called “gdk-pixbuf-csource”. This library
 * allows you to convert an image into a C variable declaration, which
 * can then be loaded into a #GdkPixbuf using
 * gdk_pixbuf_new_from_inline().
 */


struct _GtkImagePrivate
{
  GtkIconHelper *icon_helper;

  GdkPixbufAnimationIter *animation_iter;
  gint animation_timeout;

  float baseline_align;

  gchar                *filename;       /* Only used with GTK_IMAGE_ANIMATION, GTK_IMAGE_PIXBUF */
  gchar                *resource_path;  /* Only used with GTK_IMAGE_PIXBUF */
};


#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_BUTTON
static gint gtk_image_draw                 (GtkWidget    *widget,
                                            cairo_t      *cr);
static void gtk_image_size_allocate        (GtkWidget    *widget,
                                            GtkAllocation*allocation);
static void gtk_image_unmap                (GtkWidget    *widget);
static void gtk_image_realize              (GtkWidget    *widget);
static void gtk_image_unrealize            (GtkWidget    *widget);
static void gtk_image_get_preferred_size   (GtkImage     *image,
                                            gint         *width_out,
                                            gint         *height_out);
static void gtk_image_get_preferred_width  (GtkWidget    *widget,
                                            gint         *minimum,
                                            gint         *natural);
static void gtk_image_get_preferred_height (GtkWidget    *widget,
                                            gint         *minimum,
                                            gint         *natural);
static void gtk_image_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
								   gint       width,
								   gint      *minimum,
								   gint      *natural,
								   gint      *minimum_baseline,
								   gint      *natural_baseline);

static void gtk_image_style_updated        (GtkWidget    *widget);
static void gtk_image_screen_changed       (GtkWidget    *widget,
                                            GdkScreen    *prev_screen);
static void gtk_image_finalize             (GObject      *object);
static void gtk_image_reset                (GtkImage     *image);

static void gtk_image_set_property         (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void gtk_image_get_property         (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

static void icon_theme_changed             (GtkImage     *image);

enum
{
  PROP_0,
  PROP_PIXBUF,
  PROP_SURFACE,
  PROP_FILE,
  PROP_STOCK,
  PROP_ICON_SET,
  PROP_ICON_SIZE,
  PROP_PIXEL_SIZE,
  PROP_PIXBUF_ANIMATION,
  PROP_ICON_NAME,
  PROP_STORAGE_TYPE,
  PROP_GICON,
  PROP_RESOURCE,
  PROP_USE_FALLBACK
};

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
G_DEFINE_TYPE_WITH_PRIVATE (GtkImage, gtk_image, GTK_TYPE_MISC)
G_GNUC_END_IGNORE_DEPRECATIONS

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
  widget_class->draw = gtk_image_draw;
  widget_class->get_preferred_width = gtk_image_get_preferred_width;
  widget_class->get_preferred_height = gtk_image_get_preferred_height;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_image_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_image_size_allocate;
  widget_class->unmap = gtk_image_unmap;
  widget_class->realize = gtk_image_realize;
  widget_class->unrealize = gtk_image_unrealize;
  widget_class->style_updated = gtk_image_style_updated;
  widget_class->screen_changed = gtk_image_screen_changed;
  
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF,
                                   g_param_spec_object ("pixbuf",
                                                        P_("Pixbuf"),
                                                        P_("A GdkPixbuf to display"),
                                                        GDK_TYPE_PIXBUF,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SURFACE,
                                   g_param_spec_boxed ("surface",
						       P_("Surface"),
						       P_("A cairo_surface_t to display"),
						       CAIRO_GOBJECT_TYPE_SURFACE,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_string ("file",
                                                        P_("Filename"),
                                                        P_("Filename to load and display"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  

  /**
   * GtkImage:stock:
   *
   * Deprecated: 3.10: Use #GtkImage:icon-name instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STOCK,
                                   g_param_spec_string ("stock",
                                                        P_("Stock ID"),
                                                        P_("Stock ID for a stock image to display"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));
  
  /**
   * GtkImage:icon-set:
   *
   * Deprecated: 3.10: Use #GtkImage:icon-name instead.
   */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SET,
                                   g_param_spec_boxed ("icon-set",
                                                       P_("Icon set"),
                                                       P_("Icon set to display"),
                                                       GTK_TYPE_ICON_SET,
                                                       GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SIZE,
                                   g_param_spec_int ("icon-size",
                                                     P_("Icon size"),
                                                     P_("Symbolic size to use for stock icon, icon set or named icon"),
                                                     0, G_MAXINT,
                                                     DEFAULT_ICON_SIZE,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkImage:pixel-size:
   *
   * The "pixel-size" property can be used to specify a fixed size
   * overriding the #GtkImage:icon-size property for images of type 
   * %GTK_IMAGE_ICON_NAME. 
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_PIXEL_SIZE,
				   g_param_spec_int ("pixel-size",
						     P_("Pixel size"),
						     P_("Pixel size to use for named icon"),
						     -1, G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF_ANIMATION,
                                   g_param_spec_object ("pixbuf-animation",
                                                        P_("Animation"),
                                                        P_("GdkPixbufAnimation to display"),
                                                        GDK_TYPE_PIXBUF_ANIMATION,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkImage:icon-name:
   *
   * The name of the icon in the icon theme. If the icon theme is
   * changed, the image will be updated automatically.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        P_("Icon Name"),
                                                        P_("The name of the icon from the icon theme"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkImage:gicon:
   *
   * The GIcon displayed in the GtkImage. For themed icons,
   * If the icon theme is changed, the image will be updated
   * automatically.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GICON,
                                   g_param_spec_object ("gicon",
                                                        P_("Icon"),
                                                        P_("The GIcon being displayed"),
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkImage:resource:
   *
   * A path to a resource file to display.
   *
   * Since: 3.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RESOURCE,
                                   g_param_spec_string ("resource",
                                                        P_("Resource"),
                                                        P_("The resource path being displayed"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_STORAGE_TYPE,
                                   g_param_spec_enum ("storage-type",
                                                      P_("Storage type"),
                                                      P_("The representation being used for image data"),
                                                      GTK_TYPE_IMAGE_TYPE,
                                                      GTK_IMAGE_EMPTY,
                                                      GTK_PARAM_READABLE));

  /**
   * GtkImage:use-fallback:
   *
   * Whether the icon displayed in the GtkImage will use
   * standard icon names fallback. The value of this property
   * is only relevant for images of type %GTK_IMAGE_ICON_NAME
   * and %GTK_IMAGE_GICON.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_FALLBACK,
                                   g_param_spec_boolean ("use-fallback",
                                                         P_("Use Fallback"),
                                                         P_("Whether to use icon names fallback"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_IMAGE_ACCESSIBLE);
}

static void
gtk_image_init (GtkImage *image)
{
  GtkImagePrivate *priv;

  image->priv = gtk_image_get_instance_private (image);
  priv = image->priv;

  gtk_widget_set_has_window (GTK_WIDGET (image), FALSE);
  priv->icon_helper = _gtk_icon_helper_new ();
  _gtk_icon_helper_set_icon_size (priv->icon_helper, DEFAULT_ICON_SIZE);

  priv->filename = NULL;
}

static void
gtk_image_finalize (GObject *object)
{
  GtkImage *image = GTK_IMAGE (object);

  g_clear_object (&image->priv->icon_helper);
  
  g_free (image->priv->filename);

  G_OBJECT_CLASS (gtk_image_parent_class)->finalize (object);
};

static void 
gtk_image_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkImage *image = GTK_IMAGE (object);
  GtkImagePrivate *priv = image->priv;
  GtkIconSize icon_size = _gtk_icon_helper_get_icon_size (priv->icon_helper);

  if (icon_size == GTK_ICON_SIZE_INVALID)
    icon_size = DEFAULT_ICON_SIZE;

  switch (prop_id)
    {
    case PROP_PIXBUF:
      gtk_image_set_from_pixbuf (image, g_value_get_object (value));
      break;
    case PROP_SURFACE:
      gtk_image_set_from_surface (image, g_value_get_boxed (value));
      break;
    case PROP_FILE:
      gtk_image_set_from_file (image, g_value_get_string (value));
      break;
    case PROP_STOCK:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_image_set_from_stock (image, g_value_get_string (value), icon_size);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_ICON_SET:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_image_set_from_icon_set (image, g_value_get_boxed (value), icon_size);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_ICON_SIZE:
      if (_gtk_icon_helper_set_icon_size (priv->icon_helper, g_value_get_int (value)))
        g_object_notify_by_pspec (object, pspec);
      break;
    case PROP_PIXEL_SIZE:
      gtk_image_set_pixel_size (image, g_value_get_int (value));
      break;
    case PROP_PIXBUF_ANIMATION:
      gtk_image_set_from_animation (image, g_value_get_object (value));
      break;
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (image, g_value_get_string (value), icon_size);
      break;
    case PROP_GICON:
      gtk_image_set_from_gicon (image, g_value_get_object (value), icon_size);
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
  GtkImagePrivate *priv = image->priv;

  switch (prop_id)
    {
    case PROP_PIXBUF:
      g_value_set_object (value, _gtk_icon_helper_peek_pixbuf (priv->icon_helper));
      break;
    case PROP_SURFACE:
      g_value_set_boxed (value, _gtk_icon_helper_peek_surface (priv->icon_helper));
      break;
    case PROP_FILE:
      g_value_set_string (value, priv->filename);
      break;
    case PROP_STOCK:
      g_value_set_string (value, _gtk_icon_helper_get_stock_id (priv->icon_helper));
      break;
    case PROP_ICON_SET:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      g_value_set_boxed (value, _gtk_icon_helper_peek_icon_set (priv->icon_helper));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_ICON_SIZE:
      g_value_set_int (value, _gtk_icon_helper_get_icon_size (priv->icon_helper));
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_int (value, _gtk_icon_helper_get_pixel_size (priv->icon_helper));
      break;
    case PROP_PIXBUF_ANIMATION:
      g_value_set_object (value, _gtk_icon_helper_peek_animation (priv->icon_helper));
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
 * If the file contains an animation, the image will contain an
 * animation.
 *
 * If you need to detect failures to load the file, use
 * gdk_pixbuf_new_from_file() to load the file yourself, then create
 * the #GtkImage from the pixbuf. (Or for animations, use
 * gdk_pixbuf_animation_new_from_file()).
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
 * If the file contains an animation, the image will contain an
 * animation.
 *
 * If you need to detect failures to load the file, use
 * gdk_pixbuf_new_from_file() to load the file yourself, then create
 * the #GtkImage from the pixbuf. (Or for animations, use
 * gdk_pixbuf_animation_new_from_file()).
 *
 * The storage type (gtk_image_get_storage_type()) of the returned
 * image is not defined, it will be whatever is appropriate for
 * displaying the file.
 *
 * Returns: a new #GtkImage
 *
 * Since: 3.4
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
 * gtk_image_new_from_surface:
 * @surface: (allow-none): a #cairo_surface_t, or %NULL
 *
 * Creates a new #GtkImage displaying @surface.
 * The #GtkImage does not assume a reference to the
 * surface; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Returns: a new #GtkImage
 *
 * Since: 3.10
 **/
GtkWidget*
gtk_image_new_from_surface (cairo_surface_t *surface)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_surface (image, surface);

  return GTK_WIDGET (image);  
}

/**
 * gtk_image_new_from_stock:
 * @stock_id: a stock icon name
 * @size: (type int): a stock icon size
 * 
 * Creates a #GtkImage displaying a stock icon. Sample stock icon
 * names are #GTK_STOCK_OPEN, #GTK_STOCK_QUIT. Sample stock sizes
 * are #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_SMALL_TOOLBAR. If the stock
 * icon name isn’t known, the image will be empty.
 * You can register your own stock icon names, see
 * gtk_icon_factory_add_default() and gtk_icon_factory_add().
 * 
 * Returns: a new #GtkImage displaying the stock icon
 *
 * Deprecated: 3.10: Use gtk_image_new_from_icon_name() instead.
 **/
GtkWidget*
gtk_image_new_from_stock (const gchar    *stock_id,
                          GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_image_set_from_stock (image, stock_id, size);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_icon_set:
 * @icon_set: a #GtkIconSet
 * @size: (type int): a stock icon size
 *
 * Creates a #GtkImage displaying an icon set. Sample stock sizes are
 * #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_SMALL_TOOLBAR. Instead of using
 * this function, usually it’s better to create a #GtkIconFactory, put
 * your icon sets in the icon factory, add the icon factory to the
 * list of default factories with gtk_icon_factory_add_default(), and
 * then use gtk_image_new_from_stock(). This will allow themes to
 * override the icon you ship with your application.
 *
 * The #GtkImage does not assume a reference to the
 * icon set; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Returns: a new #GtkImage
 *
 * Deprecated: 3.10: Use gtk_image_new_from_icon_name() instead.
 **/
GtkWidget*
gtk_image_new_from_icon_set (GtkIconSet     *icon_set,
                             GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  gtk_image_set_from_icon_set (image, icon_set, size);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_animation:
 * @animation: an animation
 * 
 * Creates a #GtkImage displaying the given animation.
 * The #GtkImage does not assume a reference to the
 * animation; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 *
 * Note that the animation frames are shown using a timeout with
 * #G_PRIORITY_DEFAULT. When using animations to indicate busyness,
 * keep in mind that the animation will only be shown if the main loop
 * is not busy with something that has a higher priority.
 *
 * Returns: a new #GtkImage widget
 **/
GtkWidget*
gtk_image_new_from_animation (GdkPixbufAnimation *animation)
{
  GtkImage *image;

  g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), NULL);
  
  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_animation (image, animation);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_icon_name:
 * @icon_name: an icon name
 * @size: (type int): a stock icon size
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 * 
 * Returns: a new #GtkImage displaying the themed icon
 *
 * Since: 2.6
 **/
GtkWidget*
gtk_image_new_from_icon_name (const gchar    *icon_name,
			      GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_icon_name (image, icon_name, size);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_gicon:
 * @icon: an icon
 * @size: (type int): a stock icon size
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 * 
 * Returns: a new #GtkImage displaying the themed icon
 *
 * Since: 2.14
 **/
GtkWidget*
gtk_image_new_from_gicon (GIcon *icon,
			  GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_gicon (image, icon, size);

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
  GtkImagePrivate *priv;
  GdkPixbufAnimation *anim;
  gint scale_factor;
  
  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_reset (image);

  if (filename == NULL)
    {
      priv->filename = NULL;
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  anim = load_scalable_with_loader (image, filename, NULL, &scale_factor);

  if (anim == NULL)
    {
      gtk_image_set_from_icon_name (image, "image-missing", DEFAULT_ICON_SIZE);
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  /* We could just unconditionally set_from_animation,
   * but it's nicer for memory if we toss the animation
   * if it's just a single pixbuf
   */

  if (gdk_pixbuf_animation_is_static_image (anim))
    gtk_image_set_from_pixbuf (image,
			       gdk_pixbuf_animation_get_static_image (anim));
  else
    gtk_image_set_from_animation (image, anim);

  _gtk_icon_helper_set_pixbuf_scale (priv->icon_helper, scale_factor);

  g_object_unref (anim);

  priv->filename = g_strdup (filename);

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
    {
      gint width, height;

      gtk_image_get_preferred_size (image, &width, &height);
      if (width != gtk_widget_get_allocated_width (GTK_WIDGET (image)) ||
          height != gtk_widget_get_allocated_height (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      else
        gtk_widget_queue_draw (GTK_WIDGET (image));
    }

  g_object_notify (G_OBJECT (image), "file");

  g_object_thaw_notify (G_OBJECT (image));
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
  GtkImagePrivate *priv;
  GdkPixbufAnimation *animation;
  gint scale_factor;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_clear (image);

  if (resource_path == NULL)
    {
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  animation = load_scalable_with_loader (image, NULL, resource_path, &scale_factor);

  if (animation == NULL)
    {
      gtk_image_set_from_icon_name (image, "image-missing", DEFAULT_ICON_SIZE);
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  priv->resource_path = g_strdup (resource_path);

  if (gdk_pixbuf_animation_is_static_image (animation))
    gtk_image_set_from_pixbuf (image, gdk_pixbuf_animation_get_static_image (animation));
  else
    gtk_image_set_from_animation (image, animation);

  _gtk_icon_helper_set_pixbuf_scale (priv->icon_helper, scale_factor);

  g_object_notify (G_OBJECT (image), "resource");

  g_object_unref (animation);

  g_object_thaw_notify (G_OBJECT (image));
}


/**
 * gtk_image_set_from_pixbuf:
 * @image: a #GtkImage
 * @pixbuf: (allow-none): a #GdkPixbuf or %NULL
 *
 * See gtk_image_new_from_pixbuf() for details.
 **/
void
gtk_image_set_from_pixbuf (GtkImage  *image,
                           GdkPixbuf *pixbuf)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  gtk_image_reset (image);

  if (pixbuf != NULL)
    _gtk_icon_helper_set_pixbuf (priv->icon_helper, pixbuf);

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
    {
      gint width, height;

      gtk_image_get_preferred_size (image, &width, &height);
      if (width != gtk_widget_get_allocated_width (GTK_WIDGET (image)) ||
          height != gtk_widget_get_allocated_height (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      else
        gtk_widget_queue_draw (GTK_WIDGET (image));
    }

  g_object_notify (G_OBJECT (image), "pixbuf");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_stock:
 * @image: a #GtkImage
 * @stock_id: a stock icon name
 * @size: (type int): a stock icon size
 *
 * See gtk_image_new_from_stock() for details.
 *
 * Deprecated: 3.10: Use gtk_image_set_from_icon_name() instead.
 **/
void
gtk_image_set_from_stock  (GtkImage       *image,
                           const gchar    *stock_id,
                           GtkIconSize     size)
{
  GtkImagePrivate *priv;
  gchar *new_id;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  new_id = g_strdup (stock_id);
  gtk_image_clear (image);

  if (new_id)
    {
      _gtk_icon_helper_set_stock_id (priv->icon_helper, new_id, size);
      g_free (new_id);
    }

  g_object_notify (G_OBJECT (image), "stock");
  g_object_notify (G_OBJECT (image), "icon-size");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_icon_set:
 * @image: a #GtkImage
 * @icon_set: a #GtkIconSet
 * @size: (type int): a stock icon size
 *
 * See gtk_image_new_from_icon_set() for details.
 *
 * Deprecated: 3.10: Use gtk_image_set_from_icon_name() instead.
 **/
void
gtk_image_set_from_icon_set  (GtkImage       *image,
                              GtkIconSet     *icon_set,
                              GtkIconSize     size)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (icon_set)
    gtk_icon_set_ref (icon_set);
  
  gtk_image_clear (image);

  if (icon_set)
    {      
      _gtk_icon_helper_set_icon_set (priv->icon_helper, icon_set, size);
      gtk_icon_set_unref (icon_set);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;

  g_object_notify (G_OBJECT (image), "icon-set");
  g_object_notify (G_OBJECT (image), "icon-size");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_animation:
 * @image: a #GtkImage
 * @animation: the #GdkPixbufAnimation
 * 
 * Causes the #GtkImage to display the given animation (or display
 * nothing, if you set the animation to %NULL).
 **/
void
gtk_image_set_from_animation (GtkImage           *image,
                              GdkPixbufAnimation *animation)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (animation == NULL ||
                    GDK_IS_PIXBUF_ANIMATION (animation));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));
  
  if (animation)
    g_object_ref (animation);

  gtk_image_reset (image);

  if (animation != NULL)
    {
      _gtk_icon_helper_set_animation (priv->icon_helper, animation);
      g_object_unref (animation);
    }

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
    {
      gint width, height;

      gtk_image_get_preferred_size (image, &width, &height);
      if (width != gtk_widget_get_allocated_width (GTK_WIDGET (image)) ||
          height != gtk_widget_get_allocated_height (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      else
        gtk_widget_queue_draw (GTK_WIDGET (image));
    }

  g_object_notify (G_OBJECT (image), "pixbuf-animation");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_icon_name:
 * @image: a #GtkImage
 * @icon_name: an icon name
 * @size: (type int): an icon size
 *
 * See gtk_image_new_from_icon_name() for details.
 * 
 * Since: 2.6
 **/
void
gtk_image_set_from_icon_name  (GtkImage       *image,
			       const gchar    *icon_name,
			       GtkIconSize     size)
{
  GtkImagePrivate *priv;
  gchar *new_name;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  new_name = g_strdup (icon_name);

  gtk_image_reset (image);

  if (new_name)
    {
      _gtk_icon_helper_set_icon_name (priv->icon_helper, new_name, size);
      g_free (new_name);
    }

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
    {
      gint width, height;

      gtk_image_get_preferred_size (image, &width, &height);
      if (width != gtk_widget_get_allocated_width (GTK_WIDGET (image)) ||
          height != gtk_widget_get_allocated_height (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      else
        gtk_widget_queue_draw (GTK_WIDGET (image));
    }

  g_object_notify (G_OBJECT (image), "icon-name");
  g_object_notify (G_OBJECT (image), "icon-size");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_gicon:
 * @image: a #GtkImage
 * @icon: an icon
 * @size: (type int): an icon size
 *
 * See gtk_image_new_from_gicon() for details.
 * 
 * Since: 2.14
 **/
void
gtk_image_set_from_gicon  (GtkImage       *image,
			   GIcon          *icon,
			   GtkIconSize     size)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  if (icon)
    g_object_ref (icon);

  gtk_image_reset (image);

  if (icon)
    {
      _gtk_icon_helper_set_gicon (priv->icon_helper, icon, size);
      g_object_unref (icon);
    }

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
    {
      gint width, height;

      gtk_image_get_preferred_size (image, &width, &height);
      if (width != gtk_widget_get_allocated_width (GTK_WIDGET (image)) ||
          height != gtk_widget_get_allocated_height (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      else
        gtk_widget_queue_draw (GTK_WIDGET (image));
    }

  g_object_notify (G_OBJECT (image), "gicon");
  g_object_notify (G_OBJECT (image), "icon-size");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_surface:
 * @image: a #GtkImage
 * @surface: a cairo_surface_t
 *
 * See gtk_image_new_from_surface() for details.
 * 
 * Since: 3.10
 **/
void
gtk_image_set_from_surface (GtkImage       *image,
			    cairo_surface_t *surface)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  g_object_freeze_notify (G_OBJECT (image));

  if (surface)
    cairo_surface_reference (surface);

  gtk_image_reset (image);

  if (surface)
    {
      _gtk_icon_helper_set_surface (priv->icon_helper, surface);
      cairo_surface_destroy (surface);
    }

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
    {
      gint width, height;

      gtk_image_get_preferred_size (image, &width, &height);
      if (width != gtk_widget_get_allocated_width (GTK_WIDGET (image)) ||
          height != gtk_widget_get_allocated_height (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      else
        gtk_widget_queue_draw (GTK_WIDGET (image));
    }

  g_object_notify (G_OBJECT (image), "surface");
  
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
  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_IMAGE_EMPTY);

  return _gtk_icon_helper_get_storage_type (image->priv->icon_helper);
}

/**
 * gtk_image_get_pixbuf:
 * @image: a #GtkImage
 *
 * Gets the #GdkPixbuf being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PIXBUF (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned pixbuf.
 * 
 * Returns: (transfer none): the displayed pixbuf, or %NULL if
 * the image is empty
 **/
GdkPixbuf*
gtk_image_get_pixbuf (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  return _gtk_icon_helper_peek_pixbuf (image->priv->icon_helper);
}

/**
 * gtk_image_get_stock:
 * @image: a #GtkImage
 * @stock_id: (out) (transfer none) (allow-none): place to store a
 *     stock icon name, or %NULL
 * @size: (out) (allow-none) (type int): place to store a stock icon
 *     size, or %NULL
 *
 * Gets the stock icon name and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_STOCK (see gtk_image_get_storage_type()).
 * The returned string is owned by the #GtkImage and should not
 * be freed.
 *
 * Deprecated: 3.10: Use gtk_image_get_icon_name() instead.
 **/
void
gtk_image_get_stock  (GtkImage        *image,
                      gchar          **stock_id,
                      GtkIconSize     *size)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  if (stock_id)
    *stock_id = (gchar *) _gtk_icon_helper_get_stock_id (priv->icon_helper);

  if (size)
    *size = _gtk_icon_helper_get_icon_size (priv->icon_helper);
}

/**
 * gtk_image_get_icon_set:
 * @image: a #GtkImage
 * @icon_set: (out) (transfer none) (allow-none): location to store a
 *     #GtkIconSet, or %NULL
 * @size: (out) (allow-none) (type int): location to store a stock
 *     icon size, or %NULL
 *
 * Gets the icon set and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_SET (see gtk_image_get_storage_type()).
 *
 * Deprecated: 3.10: Use gtk_image_get_icon_name() instead.
 **/
void
gtk_image_get_icon_set  (GtkImage        *image,
                         GtkIconSet     **icon_set,
                         GtkIconSize     *size)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  if (icon_set)
    *icon_set = _gtk_icon_helper_peek_icon_set (priv->icon_helper);

  if (size)
    *size = _gtk_icon_helper_get_icon_size (priv->icon_helper);
}

/**
 * gtk_image_get_animation:
 * @image: a #GtkImage
 *
 * Gets the #GdkPixbufAnimation being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ANIMATION (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned animation.
 * 
 * Returns: (transfer none): the displayed animation, or %NULL if
 * the image is empty
 **/
GdkPixbufAnimation*
gtk_image_get_animation (GtkImage *image)
{
  GtkImagePrivate *priv;

  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);

  priv = image->priv;

  return _gtk_icon_helper_peek_animation (priv->icon_helper);
}

/**
 * gtk_image_get_icon_name:
 * @image: a #GtkImage
 * @icon_name: (out) (transfer none) (allow-none): place to store an
 *     icon name, or %NULL
 * @size: (out) (allow-none) (type int): place to store an icon size,
 *     or %NULL
 *
 * Gets the icon name and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_NAME (see gtk_image_get_storage_type()).
 * The returned string is owned by the #GtkImage and should not
 * be freed.
 * 
 * Since: 2.6
 **/
void
gtk_image_get_icon_name  (GtkImage     *image,
			  const gchar **icon_name,
			  GtkIconSize  *size)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  if (icon_name)
    *icon_name = _gtk_icon_helper_get_icon_name (priv->icon_helper);

  if (size)
    *size = _gtk_icon_helper_get_icon_size (priv->icon_helper);
}

/**
 * gtk_image_get_gicon:
 * @image: a #GtkImage
 * @gicon: (out) (transfer none) (allow-none): place to store a
 *     #GIcon, or %NULL
 * @size: (out) (allow-none) (type int): place to store an icon size,
 *     or %NULL
 *
 * Gets the #GIcon and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_GICON (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned #GIcon.
 * 
 * Since: 2.14
 **/
void
gtk_image_get_gicon (GtkImage     *image,
		     GIcon       **gicon,
		     GtkIconSize  *size)
{
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));

  priv = image->priv;

  if (gicon)
    *gicon = _gtk_icon_helper_peek_gicon (priv->icon_helper);

  if (size)
    *size = _gtk_icon_helper_get_icon_size (priv->icon_helper);
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
gtk_image_reset_anim_iter (GtkImage *image)
{
  GtkImagePrivate *priv = image->priv;

  if (gtk_image_get_storage_type (image) == GTK_IMAGE_ANIMATION)
    {
      /* Reset the animation */
      if (priv->animation_timeout)
        {
          g_source_remove (priv->animation_timeout);
          priv->animation_timeout = 0;
        }

      g_clear_object (&priv->animation_iter);
    }
}

static void
gtk_image_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkAllocation clip;

  GTK_WIDGET_CLASS (gtk_image_parent_class)->size_allocate (widget, allocation);

  /* XXX: This is not strictly correct, we could compute the area
   * actually occupied by the image, but I'm lazy...
   */
  _gtk_style_context_get_icon_extents (gtk_widget_get_style_context (widget),
                                       &clip,
                                       allocation->x,
                                       allocation->y,
                                       allocation->width,
                                       allocation->height);

  _gtk_widget_set_simple_clip (widget, &clip);
}

static void
gtk_image_unmap (GtkWidget *widget)
{
  gtk_image_reset_anim_iter (GTK_IMAGE (widget));

  GTK_WIDGET_CLASS (gtk_image_parent_class)->unmap (widget);
}

static void
gtk_image_realize (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = image->priv;

  GTK_WIDGET_CLASS (gtk_image_parent_class)->realize (widget);

  _gtk_icon_helper_set_window (priv->icon_helper,
			       gtk_widget_get_window (widget));
}

static void
gtk_image_unrealize (GtkWidget *widget)
{
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = image->priv;

  _gtk_icon_helper_set_window (priv->icon_helper, NULL);
  gtk_image_reset_anim_iter (GTK_IMAGE (widget));

  GTK_WIDGET_CLASS (gtk_image_parent_class)->unrealize (widget);
}

static gint
animation_timeout (gpointer data)
{
  GtkImage *image = GTK_IMAGE (data);
  GtkImagePrivate *priv = image->priv;
  int delay;

  priv->animation_timeout = 0;

  gdk_pixbuf_animation_iter_advance (priv->animation_iter, NULL);

  delay = gdk_pixbuf_animation_iter_get_delay_time (priv->animation_iter);
  if (delay >= 0)
    {
      GtkWidget *widget = GTK_WIDGET (image);

      priv->animation_timeout =
        gdk_threads_add_timeout (delay, animation_timeout, image);
      g_source_set_name_by_id (priv->animation_timeout, "[gtk+] animation_timeout");

      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static GdkPixbuf *
get_animation_frame (GtkImage *image)
{
  GtkImagePrivate *priv = image->priv;

  if (priv->animation_iter == NULL)
    {
      int delay;

      priv->animation_iter = 
        gdk_pixbuf_animation_get_iter (_gtk_icon_helper_peek_animation (priv->icon_helper), NULL);

      delay = gdk_pixbuf_animation_iter_get_delay_time (priv->animation_iter);
      if (delay >= 0) {
        priv->animation_timeout =
          gdk_threads_add_timeout (delay, animation_timeout, image);
        g_source_set_name_by_id (priv->animation_timeout, "[gtk+] animation_timeout");
      }
    }

  /* don't advance the anim iter here, or we could get frame changes between two
   * exposes of different areas.
   */
  return g_object_ref (gdk_pixbuf_animation_iter_get_pixbuf (priv->animation_iter));
}

static void
gtk_image_get_preferred_size (GtkImage *image,
                              gint     *width_out,
                              gint     *height_out)
{
  GtkImagePrivate *priv = image->priv;
  gint width, height;
  GtkBorder border;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (image));
  _gtk_icon_helper_get_size (priv->icon_helper, context, &width, &height);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _gtk_misc_get_padding_and_border (GTK_MISC (image), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

  width += border.left + border.right;
  height += border.top + border.bottom;

  if (width_out)
    *width_out = width;
  if (height_out)
    *height_out = height;
}

static float
gtk_image_get_baseline_align (GtkImage *image)
{
  PangoContext *pango_context;
  PangoFontMetrics *metrics;

  if (image->priv->baseline_align == 0.0)
    {
      pango_context = gtk_widget_get_pango_context (GTK_WIDGET (image));
      metrics = pango_context_get_metrics (pango_context,
					   pango_context_get_font_description (pango_context),
					   pango_context_get_language (pango_context));
      image->priv->baseline_align =
	(float)pango_font_metrics_get_ascent (metrics) /
	(pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));

      pango_font_metrics_unref (metrics);
    }

  return image->priv->baseline_align;
}

static gint
gtk_image_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkImage *image;
  GtkImagePrivate *priv;
  GtkStyleContext *context;
  gint x, y, width, height, baseline;
  gfloat xalign, yalign;
  GtkBorder border;

  g_return_val_if_fail (GTK_IS_IMAGE (widget), FALSE);

  image = GTK_IMAGE (widget);
  priv = image->priv;

  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr,
                         0, 0,
                         gtk_widget_get_allocated_width (widget), gtk_widget_get_allocated_height (widget));
  gtk_render_frame (context, cr,
                    0, 0,
                    gtk_widget_get_allocated_width (widget), gtk_widget_get_allocated_height (widget));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_misc_get_alignment (GTK_MISC (image), &xalign, &yalign);
  _gtk_misc_get_padding_and_border (GTK_MISC (image), &border);
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_image_get_preferred_size (image, &width, &height);

  if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    xalign = 1.0 - xalign;

  baseline = gtk_widget_get_allocated_baseline (widget);

  x = floor ((gtk_widget_get_allocated_width (widget) - width) * xalign) + border.left;
  if (baseline == -1)
    y = floor ((gtk_widget_get_allocated_height (widget) - height) * yalign) + border.top;
  else
    y = CLAMP (baseline - height * gtk_image_get_baseline_align (image),
	       border.top, gtk_widget_get_allocated_height (widget) - height);

  if (gtk_image_get_storage_type (image) == GTK_IMAGE_ANIMATION)
    {
      GdkPixbuf *pixbuf = get_animation_frame (image);
      gtk_render_icon (context, cr,
                       pixbuf,
                       x, y);
      g_object_unref (pixbuf);
    }
  else
    {
      _gtk_icon_helper_draw (priv->icon_helper, 
                             context, cr,
                             x, y);
    }

  return FALSE;
}

static void
gtk_image_reset (GtkImage *image)
{
  GtkImagePrivate *priv = image->priv;
  GtkImageType storage_type;

  g_object_freeze_notify (G_OBJECT (image));
  storage_type = gtk_image_get_storage_type (image);
  
  if (storage_type != GTK_IMAGE_EMPTY)
    g_object_notify (G_OBJECT (image), "storage-type");

  g_object_notify (G_OBJECT (image), "icon-size");
  
  switch (storage_type)
    {
    case GTK_IMAGE_PIXBUF:
      g_object_notify (G_OBJECT (image), "pixbuf");
      break;
    case GTK_IMAGE_STOCK:
      g_object_notify (G_OBJECT (image), "stock");      
      break;
    case GTK_IMAGE_ICON_SET:
      g_object_notify (G_OBJECT (image), "icon-set");      
      break;
    case GTK_IMAGE_ANIMATION:
      gtk_image_reset_anim_iter (image);      
      g_object_notify (G_OBJECT (image), "pixbuf-animation");
      break;
    case GTK_IMAGE_ICON_NAME:
      g_object_notify (G_OBJECT (image), "icon-name");
      break;
    case GTK_IMAGE_GICON:
      g_object_notify (G_OBJECT (image), "gicon");
      break;
    case GTK_IMAGE_EMPTY:
    default:
      break;
    }

  if (priv->filename)
    {
      g_free (priv->filename);
      priv->filename = NULL;
      g_object_notify (G_OBJECT (image), "file");
    }

  if (priv->resource_path)
    {
      g_free (priv->resource_path);
      priv->resource_path = NULL;
      g_object_notify (G_OBJECT (image), "resource");
    }

  _gtk_icon_helper_clear (priv->icon_helper);

  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_clear:
 * @image: a #GtkImage
 *
 * Resets the image to be empty.
 *
 * Since: 2.8
 */
void
gtk_image_clear (GtkImage *image)
{
  gtk_image_reset (image);

  if (gtk_widget_get_visible (GTK_WIDGET (image)))
      gtk_widget_queue_resize (GTK_WIDGET (image));
}

static void
gtk_image_get_preferred_width (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  gint width;

  gtk_image_get_preferred_size (GTK_IMAGE (widget), &width, NULL);
  *minimum = *natural = width;
} 

static void
gtk_image_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
						       gint       width,
						       gint      *minimum,
						       gint      *natural,
						       gint      *minimum_baseline,
						       gint      *natural_baseline)
{
  gint height;
  float baseline_align;

  gtk_image_get_preferred_size (GTK_IMAGE (widget), NULL, &height);
  *minimum = *natural = height;

  if (minimum_baseline || natural_baseline)
    {
      baseline_align = gtk_image_get_baseline_align (GTK_IMAGE (widget));
      if (minimum_baseline)
	*minimum_baseline = height * baseline_align;
      if (natural_baseline)
	*natural_baseline = height * baseline_align;
    }
}

static void
gtk_image_get_preferred_height (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  gtk_image_get_preferred_height_and_baseline_for_width (widget, -1, minimum, natural,
							 NULL, NULL);
}

static void
icon_theme_changed (GtkImage *image)
{
  GtkImagePrivate *priv = image->priv;

  _gtk_icon_helper_invalidate (priv->icon_helper);
  gtk_widget_queue_draw (GTK_WIDGET (image));
}

static void
gtk_image_style_updated (GtkWidget *widget)
{
  static GtkBitmask *affects_icon = NULL;
  GtkImage *image = GTK_IMAGE (widget);
  GtkImagePrivate *priv = image->priv;
  const GtkBitmask *changes;

  GTK_WIDGET_CLASS (gtk_image_parent_class)->style_updated (widget);

  if (G_UNLIKELY (affects_icon == NULL))
    affects_icon = _gtk_css_style_property_get_mask_affecting (GTK_CSS_AFFECTS_ICON);

  changes = _gtk_style_context_get_changes (gtk_widget_get_style_context (widget));
  if (changes == NULL || _gtk_bitmask_intersects (changes, affects_icon))
    icon_theme_changed (image);
  priv->baseline_align = 0.0;
}

static void
gtk_image_screen_changed (GtkWidget *widget,
			  GdkScreen *prev_screen)
{
  GtkImage *image;

  image = GTK_IMAGE (widget);

  if (GTK_WIDGET_CLASS (gtk_image_parent_class)->screen_changed)
    GTK_WIDGET_CLASS (gtk_image_parent_class)->screen_changed (widget, prev_screen);

  icon_theme_changed (image);
}

void
_gtk_image_gicon_data_clear (GtkImageGIconData *data)
{
  if (data->pixbuf)
    {
      g_object_unref (data->pixbuf);
      data->pixbuf = NULL;
    }
  if (data->icon)
    {
      g_object_unref (data->icon);
      data->icon = NULL;
    }
}

/**
 * gtk_image_set_pixel_size:
 * @image: a #GtkImage
 * @pixel_size: the new pixel size
 * 
 * Sets the pixel size to use for named icons. If the pixel size is set
 * to a value != -1, it is used instead of the icon size set by
 * gtk_image_set_from_icon_name().
 *
 * Since: 2.6
 */
void 
gtk_image_set_pixel_size (GtkImage *image,
			  gint      pixel_size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  if (_gtk_icon_helper_set_pixel_size (image->priv->icon_helper, pixel_size)) 
    {
      if (gtk_widget_get_visible (GTK_WIDGET (image)))
        gtk_widget_queue_resize (GTK_WIDGET (image));
      g_object_notify (G_OBJECT (image), "pixel-size");
    }
}

/**
 * gtk_image_get_pixel_size:
 * @image: a #GtkImage
 * 
 * Gets the pixel size used for named icons.
 *
 * Returns: the pixel size used for named icons.
 *
 * Since: 2.6
 */
gint
gtk_image_get_pixel_size (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), -1);

  return _gtk_icon_helper_get_pixel_size (image->priv->icon_helper);
}
