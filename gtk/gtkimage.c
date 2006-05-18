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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <math.h>
#include <string.h>

#include "gtkcontainer.h"
#include "gtkimage.h"
#include "gtkiconfactory.h"
#include "gtkstock.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkalias.h"

typedef struct _GtkImagePrivate GtkImagePrivate;

struct _GtkImagePrivate
{
  /* Only used with GTK_IMAGE_ANIMATION, GTK_IMAGE_PIXBUF */
  gchar *filename;

  gint pixel_size;
};

#define GTK_IMAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_IMAGE, GtkImagePrivate))


#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_BUTTON
static gint gtk_image_expose       (GtkWidget      *widget,
                                    GdkEventExpose *event);
static void gtk_image_unmap        (GtkWidget      *widget);
static void gtk_image_unrealize    (GtkWidget      *widget);
static void gtk_image_size_request (GtkWidget      *widget,
                                    GtkRequisition *requisition);
static void gtk_image_style_set    (GtkWidget      *widget,
				    GtkStyle       *prev_style);
static void gtk_image_screen_changed (GtkWidget    *widget,
				      GdkScreen    *prev_screen);
static void gtk_image_destroy      (GtkObject      *object);
static void gtk_image_reset        (GtkImage       *image);
static void gtk_image_calc_size    (GtkImage       *image);

static void gtk_image_update_size  (GtkImage       *image,
                                    gint            image_width,
                                    gint            image_height);

static void gtk_image_set_property      (GObject          *object,
					 guint             prop_id,
					 const GValue     *value,
					 GParamSpec       *pspec);
static void gtk_image_get_property      (GObject          *object,
					 guint             prop_id,
					 GValue           *value,
					 GParamSpec       *pspec);

static void icon_theme_changed          (GtkImage         *image);

enum
{
  PROP_0,
  PROP_PIXBUF,
  PROP_PIXMAP,
  PROP_IMAGE,
  PROP_MASK,
  PROP_FILE,
  PROP_STOCK,
  PROP_ICON_SET,
  PROP_ICON_SIZE,
  PROP_PIXEL_SIZE,
  PROP_PIXBUF_ANIMATION,
  PROP_ICON_NAME,
  PROP_STORAGE_TYPE
};

G_DEFINE_TYPE (GtkImage, gtk_image, GTK_TYPE_MISC)

static void
gtk_image_class_init (GtkImageClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  
  gobject_class->set_property = gtk_image_set_property;
  gobject_class->get_property = gtk_image_get_property;
  
  object_class = GTK_OBJECT_CLASS (class);
  
  object_class->destroy = gtk_image_destroy;

  widget_class = GTK_WIDGET_CLASS (class);
  
  widget_class->expose_event = gtk_image_expose;
  widget_class->size_request = gtk_image_size_request;
  widget_class->unmap = gtk_image_unmap;
  widget_class->unrealize = gtk_image_unrealize;
  widget_class->style_set = gtk_image_style_set;
  widget_class->screen_changed = gtk_image_screen_changed;
  
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF,
                                   g_param_spec_object ("pixbuf",
                                                        P_("Pixbuf"),
                                                        P_("A GdkPixbuf to display"),
                                                        GDK_TYPE_PIXBUF,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PIXMAP,
                                   g_param_spec_object ("pixmap",
                                                        P_("Pixmap"),
                                                        P_("A GdkPixmap to display"),
                                                        GDK_TYPE_PIXMAP,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        P_("Image"),
                                                        P_("A GdkImage to display"),
                                                        GDK_TYPE_IMAGE,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MASK,
                                   g_param_spec_object ("mask",
                                                        P_("Mask"),
                                                        P_("Mask bitmap to use with GdkImage or GdkPixmap"),
                                                        GDK_TYPE_PIXMAP,
                                                        GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_string ("file",
                                                        P_("Filename"),
                                                        P_("Filename to load and display"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  

  g_object_class_install_property (gobject_class,
                                   PROP_STOCK,
                                   g_param_spec_string ("stock",
                                                        P_("Stock ID"),
                                                        P_("Stock ID for a stock image to display"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SET,
                                   g_param_spec_boxed ("icon-set",
                                                       P_("Icon set"),
                                                       P_("Icon set to display"),
                                                       GTK_TYPE_ICON_SET,
                                                       GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SIZE,
                                   g_param_spec_int ("icon-size",
                                                     P_("Icon size"),
                                                     P_("Symbolic size to use for stock icon, icon set or named icon"),
                                                     0, G_MAXINT,
                                                     DEFAULT_ICON_SIZE,
                                                     GTK_PARAM_READWRITE));
  /**
   * GtkImage:pixel-size:
   *
   * The :pixel-size property can be used to specify a fixed size
   * overriding the :icon-size property for images of type 
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
						     GTK_PARAM_READWRITE));
  
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
   * The name of the icon in the icon theme.  If the icon theme is
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
  
  g_object_class_install_property (gobject_class,
                                   PROP_STORAGE_TYPE,
                                   g_param_spec_enum ("storage-type",
                                                      P_("Storage type"),
                                                      P_("The representation being used for image data"),
                                                      GTK_TYPE_IMAGE_TYPE,
                                                      GTK_IMAGE_EMPTY,
                                                      GTK_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (GtkImagePrivate));
}

static void
gtk_image_init (GtkImage *image)
{
  GtkImagePrivate *priv = GTK_IMAGE_GET_PRIVATE (image);

  GTK_WIDGET_SET_FLAGS (image, GTK_NO_WINDOW);

  image->storage_type = GTK_IMAGE_EMPTY;
  image->icon_size = DEFAULT_ICON_SIZE;
  image->mask = NULL;

  priv->pixel_size = -1;

  priv->filename = NULL;
}

static void
gtk_image_destroy (GtkObject *object)
{
  GtkImage *image = GTK_IMAGE (object);

  gtk_image_reset (image);
  
  GTK_OBJECT_CLASS (gtk_image_parent_class)->destroy (object);
}

static void 
gtk_image_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkImage *image;

  image = GTK_IMAGE (object);
  
  switch (prop_id)
    {
    case PROP_PIXBUF:
      gtk_image_set_from_pixbuf (image,
                                 g_value_get_object (value));
      break;
    case PROP_PIXMAP:
      gtk_image_set_from_pixmap (image,
                                 g_value_get_object (value),
                                 image->mask);
      break;
    case PROP_IMAGE:
      gtk_image_set_from_image (image,
                                g_value_get_object (value),
                                image->mask);
      break;
    case PROP_MASK:
      if (image->storage_type == GTK_IMAGE_PIXMAP)
        gtk_image_set_from_pixmap (image,
                                   image->data.pixmap.pixmap,
                                   g_value_get_object (value));
      else if (image->storage_type == GTK_IMAGE_IMAGE)
        gtk_image_set_from_image (image,
                                  image->data.image.image,
                                  g_value_get_object (value));
      else
        {
          GdkBitmap *mask;

          mask = g_value_get_object (value);

          if (mask)
            g_object_ref (mask);
          
          gtk_image_clear (image);

          image->mask = mask;
        }
      break;
    case PROP_FILE:
      gtk_image_set_from_file (image, g_value_get_string (value));
      break;
    case PROP_STOCK:
      gtk_image_set_from_stock (image, g_value_get_string (value),
                                image->icon_size);
      break;
    case PROP_ICON_SET:
      gtk_image_set_from_icon_set (image, g_value_get_boxed (value),
                                   image->icon_size);
      break;
    case PROP_ICON_SIZE:
      if (image->storage_type == GTK_IMAGE_STOCK)
        gtk_image_set_from_stock (image,
                                  image->data.stock.stock_id,
                                  g_value_get_int (value));
      else if (image->storage_type == GTK_IMAGE_ICON_SET)
        gtk_image_set_from_icon_set (image,
                                     image->data.icon_set.icon_set,
                                     g_value_get_int (value));
      else if (image->storage_type == GTK_IMAGE_ICON_NAME)
        gtk_image_set_from_icon_name (image,
				      image->data.name.icon_name,
				      g_value_get_int (value));
      else
        /* Save to be used when STOCK or ICON_SET property comes in */
        image->icon_size = g_value_get_int (value);
      break;
    case PROP_PIXEL_SIZE:
      gtk_image_set_pixel_size (image, g_value_get_int (value));
      break;
    case PROP_PIXBUF_ANIMATION:
      gtk_image_set_from_animation (image,
                                    g_value_get_object (value));
      break;
    case PROP_ICON_NAME:
      gtk_image_set_from_icon_name (image, g_value_get_string (value),
				    image->icon_size);
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
  GtkImage *image;
  GtkImagePrivate *priv;

  image = GTK_IMAGE (object);
  priv = GTK_IMAGE_GET_PRIVATE (image);

  /* The "getter" functions whine if you try to get the wrong
   * storage type. This function is instead robust against that,
   * so that GUI builders don't have to jump through hoops
   * to avoid g_warning
   */
  
  switch (prop_id)
    {
    case PROP_PIXBUF:
      if (image->storage_type != GTK_IMAGE_PIXBUF)
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value,
                            gtk_image_get_pixbuf (image));
      break;
    case PROP_PIXMAP:
      if (image->storage_type != GTK_IMAGE_PIXMAP)
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value,
                            image->data.pixmap.pixmap);
      break;
    case PROP_MASK:
      g_value_set_object (value, image->mask);
      break;
    case PROP_IMAGE:
      if (image->storage_type != GTK_IMAGE_IMAGE)
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value,
                            image->data.image.image);
      break;
    case PROP_FILE:
      g_value_set_string (value, priv->filename);
      break;
    case PROP_STOCK:
      if (image->storage_type != GTK_IMAGE_STOCK)
        g_value_set_string (value, NULL);
      else
        g_value_set_string (value,
                            image->data.stock.stock_id);
      break;
    case PROP_ICON_SET:
      if (image->storage_type != GTK_IMAGE_ICON_SET)
        g_value_set_boxed (value, NULL);
      else
        g_value_set_boxed (value,
                           image->data.icon_set.icon_set);
      break;      
    case PROP_ICON_SIZE:
      g_value_set_int (value, image->icon_size);
      break;
    case PROP_PIXEL_SIZE:
      g_value_set_int (value, priv->pixel_size);
      break;
    case PROP_PIXBUF_ANIMATION:
      if (image->storage_type != GTK_IMAGE_ANIMATION)
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value,
                            image->data.anim.anim);
      break;
    case PROP_ICON_NAME:
      if (image->storage_type != GTK_IMAGE_ICON_NAME)
	g_value_set_string (value, NULL);
      else
	g_value_set_string (value,
			    image->data.name.icon_name);
      break;
    case PROP_STORAGE_TYPE:
      g_value_set_enum (value, image->storage_type);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * gtk_image_new_from_pixmap:
 * @pixmap: a #GdkPixmap, or %NULL
 * @mask: a #GdkBitmap, or %NULL
 * 
 * Creates a #GtkImage widget displaying @pixmap with a @mask.
 * A #GdkPixmap is a server-side image buffer in the pixel format of the
 * current display. The #GtkImage does not assume a reference to the
 * pixmap or mask; you still need to unref them if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Return value: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_pixmap (GdkPixmap *pixmap,
                           GdkBitmap *mask)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_pixmap (image, pixmap, mask);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_image:
 * @image: a #GdkImage, or %NULL
 * @mask: a #GdkBitmap, or %NULL 
 * 
 * Creates a #GtkImage widget displaying a @image with a @mask.
 * A #GdkImage is a client-side image buffer in the pixel format of the
 * current display.
 * The #GtkImage does not assume a reference to the
 * image or mask; you still need to unref them if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Return value: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_image  (GdkImage  *gdk_image,
                           GdkBitmap *mask)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_image (image, gdk_image, mask);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_file:
 * @filename: a filename
 * 
 * Creates a new #GtkImage displaying the file @filename. If the file
 * isn't found or can't be loaded, the resulting #GtkImage will
 * display a "broken image" icon. This function never returns %NULL,
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
 * Return value: a new #GtkImage
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
 * gtk_image_new_from_pixbuf:
 * @pixbuf: a #GdkPixbuf, or %NULL
 * 
 * Creates a new #GtkImage displaying @pixbuf.
 * The #GtkImage does not assume a reference to the
 * pixbuf; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * Note that this function just creates an #GtkImage from the pixbuf.  The
 * #GtkImage created will not react to state changes.  Should you want that, you
 * should use gtk_image_new_from_icon_set().
 * 
 * Return value: a new #GtkImage
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
 * gtk_image_new_from_stock:
 * @stock_id: a stock icon name
 * @size: a stock icon size
 * 
 * Creates a #GtkImage displaying a stock icon. Sample stock icon
 * names are #GTK_STOCK_OPEN, #GTK_STOCK_QUIT. Sample stock sizes
 * are #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_SMALL_TOOLBAR. If the stock
 * icon name isn't known, the image will be empty.
 * You can register your own stock icon names, see
 * gtk_icon_factory_add_default() and gtk_icon_factory_add().
 * 
 * Return value: a new #GtkImage displaying the stock icon
 **/
GtkWidget*
gtk_image_new_from_stock (const gchar    *stock_id,
                          GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_stock (image, stock_id, size);

  return GTK_WIDGET (image);
}

/**
 * gtk_image_new_from_icon_set:
 * @icon_set: a #GtkIconSet
 * @size: a stock icon size
 *
 * Creates a #GtkImage displaying an icon set. Sample stock sizes are
 * #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_SMALL_TOOLBAR. Instead of using
 * this function, usually it's better to create a #GtkIconFactory, put
 * your icon sets in the icon factory, add the icon factory to the
 * list of default factories with gtk_icon_factory_add_default(), and
 * then use gtk_image_new_from_stock(). This will allow themes to
 * override the icon you ship with your application.
 *
 * The #GtkImage does not assume a reference to the
 * icon set; you still need to unref it if you own references.
 * #GtkImage will add its own reference rather than adopting yours.
 * 
 * 
 * Return value: a new #GtkImage
 **/
GtkWidget*
gtk_image_new_from_icon_set (GtkIconSet     *icon_set,
                             GtkIconSize     size)
{
  GtkImage *image;

  image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_image_set_from_icon_set (image, icon_set, size);

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
 * Return value: a new #GtkImage widget
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
 * @size: a stock icon size
 * 
 * Creates a #GtkImage displaying an icon from the current icon theme.
 * If the icon name isn't known, a "broken image" icon will be
 * displayed instead.  If the current icon theme is changed, the icon
 * will be updated appropriately.
 * 
 * Return value: a new #GtkImage displaying the themed icon
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
 * gtk_image_set_from_pixmap:
 * @image: a #GtkImage
 * @pixmap: a #GdkPixmap or %NULL
 * @mask: a #GdkBitmap or %NULL
 *
 * See gtk_image_new_from_pixmap() for details.
 * 
 **/
void
gtk_image_set_from_pixmap (GtkImage  *image,
                           GdkPixmap *pixmap,
                           GdkBitmap *mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixmap == NULL ||
                    GDK_IS_PIXMAP (pixmap));
  g_return_if_fail (mask == NULL ||
                    GDK_IS_PIXMAP (mask));

  g_object_freeze_notify (G_OBJECT (image));
  
  if (pixmap)
    g_object_ref (pixmap);

  if (mask)
    g_object_ref (mask);

  gtk_image_clear (image);

  image->mask = mask;
  
  if (pixmap)
    {
      int width;
      int height;
      
      image->storage_type = GTK_IMAGE_PIXMAP;

      image->data.pixmap.pixmap = pixmap;

      gdk_drawable_get_size (GDK_DRAWABLE (pixmap), &width, &height);

      gtk_image_update_size (image, width, height);
    }

  g_object_notify (G_OBJECT (image), "pixmap");
  g_object_notify (G_OBJECT (image), "mask");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_image:
 * @image: a #GtkImage
 * @gdk_image: a #GdkImage or %NULL
 * @mask: a #GdkBitmap or %NULL
 *
 * See gtk_image_new_from_image() for details.
 * 
 **/
void
gtk_image_set_from_image  (GtkImage  *image,
                           GdkImage  *gdk_image,
                           GdkBitmap *mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (gdk_image == NULL ||
                    GDK_IS_IMAGE (gdk_image));
  g_return_if_fail (mask == NULL ||
                    GDK_IS_PIXMAP (mask));

  g_object_freeze_notify (G_OBJECT (image));
  
  if (gdk_image)
    g_object_ref (gdk_image);

  if (mask)
    g_object_ref (mask);

  gtk_image_clear (image);

  if (gdk_image)
    {
      image->storage_type = GTK_IMAGE_IMAGE;

      image->data.image.image = gdk_image;
      image->mask = mask;

      gtk_image_update_size (image, gdk_image->width, gdk_image->height);
    }
  else
    {
      /* Clean up the mask if gdk_image was NULL */
      if (mask)
        g_object_unref (mask);
    }

  g_object_notify (G_OBJECT (image), "image");
  g_object_notify (G_OBJECT (image), "mask");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_file:
 * @image: a #GtkImage
 * @filename: a filename or %NULL
 *
 * See gtk_image_new_from_file() for details.
 * 
 **/
void
gtk_image_set_from_file   (GtkImage    *image,
                           const gchar *filename)
{
  GtkImagePrivate *priv = GTK_IMAGE_GET_PRIVATE (image);
  GdkPixbufAnimation *anim;
  
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  
  gtk_image_clear (image);

  if (filename == NULL)
    {
      priv->filename = NULL;
      g_object_thaw_notify (G_OBJECT (image));
      return;
    }

  anim = gdk_pixbuf_animation_new_from_file (filename, NULL);

  if (anim == NULL)
    {
      gtk_image_set_from_stock (image,
                                GTK_STOCK_MISSING_IMAGE,
                                GTK_ICON_SIZE_BUTTON);
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

  g_object_unref (anim);

  priv->filename = g_strdup (filename);
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_pixbuf:
 * @image: a #GtkImage
 * @pixbuf: a #GdkPixbuf or %NULL
 *
 * See gtk_image_new_from_pixbuf() for details. 
 * 
 **/
void
gtk_image_set_from_pixbuf (GtkImage  *image,
                           GdkPixbuf *pixbuf)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixbuf == NULL ||
                    GDK_IS_PIXBUF (pixbuf));

  g_object_freeze_notify (G_OBJECT (image));
  
  if (pixbuf)
    g_object_ref (pixbuf);

  gtk_image_clear (image);

  if (pixbuf != NULL)
    {
      image->storage_type = GTK_IMAGE_PIXBUF;

      image->data.pixbuf.pixbuf = pixbuf;

      gtk_image_update_size (image,
                             gdk_pixbuf_get_width (pixbuf),
                             gdk_pixbuf_get_height (pixbuf));
    }

  g_object_notify (G_OBJECT (image), "pixbuf");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_stock:
 * @image: a #GtkImage
 * @stock_id: a stock icon name
 * @size: a stock icon size
 *
 * See gtk_image_new_from_stock() for details.
 * 
 **/
void
gtk_image_set_from_stock  (GtkImage       *image,
                           const gchar    *stock_id,
                           GtkIconSize     size)
{
  gchar *new_id;
  
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  /* in case stock_id == image->data.stock.stock_id */
  new_id = g_strdup (stock_id);
  
  gtk_image_clear (image);

  if (new_id)
    {
      image->storage_type = GTK_IMAGE_STOCK;
      
      image->data.stock.stock_id = new_id;
      image->icon_size = size;

      /* Size is demand-computed in size request method
       * if we're a stock image, since changing the
       * style impacts the size request
       */
    }

  g_object_notify (G_OBJECT (image), "stock");
  g_object_notify (G_OBJECT (image), "icon-size");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_icon_set:
 * @image: a #GtkImage
 * @icon_set: a #GtkIconSet
 * @size: a stock icon size
 *
 * See gtk_image_new_from_icon_set() for details.
 * 
 **/
void
gtk_image_set_from_icon_set  (GtkImage       *image,
                              GtkIconSet     *icon_set,
                              GtkIconSize     size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));
  
  if (icon_set)
    gtk_icon_set_ref (icon_set);
  
  gtk_image_clear (image);

  if (icon_set)
    {      
      image->storage_type = GTK_IMAGE_ICON_SET;
      
      image->data.icon_set.icon_set = icon_set;
      image->icon_size = size;

      /* Size is demand-computed in size request method
       * if we're an icon set
       */
    }
  
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
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (animation == NULL ||
                    GDK_IS_PIXBUF_ANIMATION (animation));

  g_object_freeze_notify (G_OBJECT (image));
  
  if (animation)
    g_object_ref (animation);

  gtk_image_clear (image);

  if (animation != NULL)
    {
      image->storage_type = GTK_IMAGE_ANIMATION;

      image->data.anim.anim = animation;
      image->data.anim.frame_timeout = 0;
      image->data.anim.iter = NULL;
      
      gtk_image_update_size (image,
                             gdk_pixbuf_animation_get_width (animation),
                             gdk_pixbuf_animation_get_height (animation));
    }

  g_object_notify (G_OBJECT (image), "pixbuf-animation");
  
  g_object_thaw_notify (G_OBJECT (image));
}

/**
 * gtk_image_set_from_icon_name:
 * @image: a #GtkImage
 * @icon_name: an icon name
 * @size: an icon size
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
  gchar *new_name;
  
  g_return_if_fail (GTK_IS_IMAGE (image));

  g_object_freeze_notify (G_OBJECT (image));

  /* in case icon_name == image->data.name.icon_name */
  new_name = g_strdup (icon_name);
  
  gtk_image_clear (image);

  if (new_name)
    {
      image->storage_type = GTK_IMAGE_ICON_NAME;
      
      image->data.name.icon_name = new_name;
      image->icon_size = size;

      /* Size is demand-computed in size request method
       * if we're a icon theme image, since changing the
       * style impacts the size request
       */
    }

  g_object_notify (G_OBJECT (image), "icon-name");
  g_object_notify (G_OBJECT (image), "icon-size");
  
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
 * Return value: image representation being used
 **/
GtkImageType
gtk_image_get_storage_type (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_IMAGE_EMPTY);

  return image->storage_type;
}

/**
 * gtk_image_get_pixmap:
 * @image: a #GtkImage
 * @pixmap: location to store the pixmap, or %NULL
 * @mask: location to store the mask, or %NULL
 *
 * Gets the pixmap and mask being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PIXMAP (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned pixmap and mask.
 * 
 **/
void
gtk_image_get_pixmap (GtkImage   *image,
                      GdkPixmap **pixmap,
                      GdkBitmap **mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image)); 
  g_return_if_fail (image->storage_type == GTK_IMAGE_PIXMAP ||
                    image->storage_type == GTK_IMAGE_EMPTY);
  
  if (pixmap)
    *pixmap = image->data.pixmap.pixmap;
  
  if (mask)
    *mask = image->mask;
}

/**
 * gtk_image_get_image:
 * @image: a #GtkImage
 * @gdk_image: return location for a #GtkImage
 * @mask: return location for a #GdkBitmap
 * 
 * Gets the #GdkImage and mask being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_IMAGE (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned image and mask.
 **/
void
gtk_image_get_image  (GtkImage   *image,
                      GdkImage  **gdk_image,
                      GdkBitmap **mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_IMAGE ||
                    image->storage_type == GTK_IMAGE_EMPTY);

  if (gdk_image)
    *gdk_image = image->data.image.image;
  
  if (mask)
    *mask = image->mask;
}

/**
 * gtk_image_get_pixbuf:
 * @image: a #GtkImage
 *
 *
 * Gets the #GdkPixbuf being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_PIXBUF (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned pixbuf.
 * 
 * Return value: the displayed pixbuf, or %NULL if the image is empty
 **/
GdkPixbuf*
gtk_image_get_pixbuf (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);
  g_return_val_if_fail (image->storage_type == GTK_IMAGE_PIXBUF ||
                        image->storage_type == GTK_IMAGE_EMPTY, NULL);

  if (image->storage_type == GTK_IMAGE_EMPTY)
    image->data.pixbuf.pixbuf = NULL;
  
  return image->data.pixbuf.pixbuf;
}

/**
 * gtk_image_get_stock:
 * @image: a #GtkImage
 * @stock_id: place to store a stock icon name
 * @size: place to store a stock icon size
 *
 * Gets the stock icon name and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_STOCK (see gtk_image_get_storage_type()).
 * The returned string is owned by the #GtkImage and should not
 * be freed.
 * 
 **/
void
gtk_image_get_stock  (GtkImage        *image,
                      gchar          **stock_id,
                      GtkIconSize     *size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_STOCK ||
                    image->storage_type == GTK_IMAGE_EMPTY);

  if (image->storage_type == GTK_IMAGE_EMPTY)
    image->data.stock.stock_id = NULL;
  
  if (stock_id)
    *stock_id = image->data.stock.stock_id;

  if (size)
    *size = image->icon_size;
}

/**
 * gtk_image_get_icon_set:
 * @image: a #GtkImage
 * @icon_set: location to store a #GtkIconSet
 * @size: location to store a stock icon size
 *
 * Gets the icon set and size being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ICON_SET (see gtk_image_get_storage_type()).
 * 
 **/
void
gtk_image_get_icon_set  (GtkImage        *image,
                         GtkIconSet     **icon_set,
                         GtkIconSize     *size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_ICON_SET ||
                    image->storage_type == GTK_IMAGE_EMPTY);
      
  if (icon_set)    
    *icon_set = image->data.icon_set.icon_set;

  if (size)
    *size = image->icon_size;
}

/**
 * gtk_image_get_animation:
 * @image: a #GtkImage
 *
 *
 * Gets the #GdkPixbufAnimation being displayed by the #GtkImage.
 * The storage type of the image must be %GTK_IMAGE_EMPTY or
 * %GTK_IMAGE_ANIMATION (see gtk_image_get_storage_type()).
 * The caller of this function does not own a reference to the
 * returned animation.
 * 
 * Return value: the displayed animation, or %NULL if the image is empty
 **/
GdkPixbufAnimation*
gtk_image_get_animation (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);
  g_return_val_if_fail (image->storage_type == GTK_IMAGE_ANIMATION ||
                        image->storage_type == GTK_IMAGE_EMPTY,
                        NULL);

  if (image->storage_type == GTK_IMAGE_EMPTY)
    image->data.anim.anim = NULL;
  
  return image->data.anim.anim;
}

/**
 * gtk_image_get_icon_name:
 * @image: a #GtkImage
 * @icon_name: place to store an icon name
 * @size: place to store an icon size
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
gtk_image_get_icon_name  (GtkImage              *image,
			  G_CONST_RETURN gchar **icon_name,
			  GtkIconSize           *size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_ICON_NAME ||
                    image->storage_type == GTK_IMAGE_EMPTY);

  if (image->storage_type == GTK_IMAGE_EMPTY)
    image->data.name.icon_name = NULL;
  
  if (icon_name)
    *icon_name = image->data.name.icon_name;

  if (size)
    *size = image->icon_size;
}

/**
 * gtk_image_new:
 * 
 * Creates a new empty #GtkImage widget.
 * 
 * Return value: a newly created #GtkImage widget. 
 **/
GtkWidget*
gtk_image_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE, NULL);
}

void
gtk_image_set (GtkImage  *image,
	       GdkImage  *val,
	       GdkBitmap *mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  gtk_image_set_from_image (image, val, mask);
}

void
gtk_image_get (GtkImage   *image,
	       GdkImage  **val,
	       GdkBitmap **mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  gtk_image_get_image (image, val, mask);
}

static void
gtk_image_reset_anim_iter (GtkImage *image)
{
  if (image->storage_type == GTK_IMAGE_ANIMATION)
    {
      /* Reset the animation */
      
      if (image->data.anim.frame_timeout)
        {
          g_source_remove (image->data.anim.frame_timeout);
          image->data.anim.frame_timeout = 0;
        }

      if (image->data.anim.iter)
        {
          g_object_unref (image->data.anim.iter);
          image->data.anim.iter = NULL;
        }
    }
}

static void
gtk_image_unmap (GtkWidget *widget)
{
  gtk_image_reset_anim_iter (GTK_IMAGE (widget));

  if (GTK_WIDGET_CLASS (gtk_image_parent_class)->unmap)
    GTK_WIDGET_CLASS (gtk_image_parent_class)->unmap (widget);
}

static void
gtk_image_unrealize (GtkWidget *widget)
{
  gtk_image_reset_anim_iter (GTK_IMAGE (widget));

  if (GTK_WIDGET_CLASS (gtk_image_parent_class)->unrealize)
    GTK_WIDGET_CLASS (gtk_image_parent_class)->unrealize (widget);
}

static gint
animation_timeout (gpointer data)
{
  GtkImage *image;

  GDK_THREADS_ENTER ();

  image = GTK_IMAGE (data);
  
  image->data.anim.frame_timeout = 0;

  gdk_pixbuf_animation_iter_advance (image->data.anim.iter, NULL);

  if (gdk_pixbuf_animation_iter_get_delay_time (image->data.anim.iter) >= 0)
    image->data.anim.frame_timeout =
      g_timeout_add (gdk_pixbuf_animation_iter_get_delay_time (image->data.anim.iter),
                     animation_timeout,
                     image);

  gtk_widget_queue_draw (GTK_WIDGET (image));

  if (GTK_WIDGET_DRAWABLE (image))
    gdk_window_process_updates (GTK_WIDGET (image)->window, TRUE);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
icon_theme_changed (GtkImage *image)
{
  if (image->storage_type == GTK_IMAGE_ICON_NAME) 
    {
      if (image->data.name.pixbuf)
	g_object_unref (image->data.name.pixbuf);
      image->data.name.pixbuf = NULL;

      gtk_widget_queue_draw (GTK_WIDGET (image));
    }
}

static void
ensure_pixbuf_for_icon_name (GtkImage *image)
{
  GtkImagePrivate *priv;
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  gint width, height;
  gint *sizes, *s, dist;
  GError *error = NULL;

  g_return_if_fail (image->storage_type == GTK_IMAGE_ICON_NAME);

  priv = GTK_IMAGE_GET_PRIVATE (image);
  screen = gtk_widget_get_screen (GTK_WIDGET (image));
  icon_theme = gtk_icon_theme_get_for_screen (screen);
  settings = gtk_settings_get_for_screen (screen);
  if (image->data.name.pixbuf == NULL)
    {
      if (priv->pixel_size != -1)
	{
	  width = height = priv->pixel_size;
	}
      else if (!gtk_icon_size_lookup_for_settings (settings,
						   image->icon_size,
						   &width, &height))
	{
	  if (image->icon_size == -1)
	    {
	      /* Find an available size close to 48 */
	      sizes = gtk_icon_theme_get_icon_sizes (icon_theme, image->data.name.icon_name);
	      dist = 100;
	      width = height = 48;
	      for (s = sizes; *s; s++)
		{
		  if (*s == -1)
		    {
		      width = height = 48;
		      break;
		    }
		  if (*s < 48)
		    {
		      if (48 - *s < dist)
			{
			  width = height = *s;
			  dist = 48 - *s;
			}
		    }
		  else
		    {
		      if (*s - 48 < dist)
			{
			  width = height = *s;
			  dist = *s - 48;
			}
		    }
		}
	      g_free (sizes);
	    }
	  else
	    {
	      g_warning ("Invalid icon size %d\n", image->icon_size);
	      width = height = 24;
	    }
	}
      image->data.name.pixbuf =
	gtk_icon_theme_load_icon (icon_theme,
				  image->data.name.icon_name,
				  MIN (width, height), 0, &error);
      if (image->data.name.pixbuf == NULL)
	{
	  g_error_free (error);
	  image->data.name.pixbuf =
	    gtk_widget_render_icon (GTK_WIDGET (image),
				    GTK_STOCK_MISSING_IMAGE,
				    image->icon_size,
				    NULL);
	}
    }
}

/*
 * Like gdk_rectangle_intersect (dest, src, dest), but make 
 * sure that the origin of dest is moved by an "even" offset. 
 * If necessary grow the intersection by one row or column 
 * to achieve this.
 *
 * This is necessary since we can't pass alignment information
 * for the pixelation pattern down to gdk_pixbuf_saturate_and_pixelate(), 
 * thus we have to makesure that the subimages are properly aligned.
 */
static gboolean
rectangle_intersect_even (GdkRectangle *src, 
			  GdkRectangle *dest)
{
  gboolean isect;
  gint x, y;

  x = dest->x;
  y = dest->y;
  isect = gdk_rectangle_intersect (dest, src, dest);

  if ((dest->x - x + dest->y - y) % 2 != 0)
    {
      if (dest->x > x)
	{
	  dest->x--;
	  dest->width++;
	}
      else
	{
	  dest->y--;
	  dest->height++;
	}
    }
  
  return isect;
}

static gint
gtk_image_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_IMAGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (GTK_WIDGET_MAPPED (widget) &&
      GTK_IMAGE (widget)->storage_type != GTK_IMAGE_EMPTY)
    {
      GtkImage *image;
      GtkMisc *misc;
      GdkRectangle area, image_bound;
      gfloat xalign;
      gint x, y, mask_x, mask_y;
      GdkBitmap *mask;
      GdkPixbuf *pixbuf;
      gboolean needs_state_transform;

      image = GTK_IMAGE (widget);
      misc = GTK_MISC (widget);

      area = event->area;

      /* For stock items and icon sets, we lazily calculate
       * the size; we might get here between a queue_resize()
       * and size_request() if something explicitely forces
       * a redraw.
       */
      if (widget->requisition.width == 0 && widget->requisition.height == 0)
	gtk_image_calc_size (image);
      
      if (!gdk_rectangle_intersect (&area, &widget->allocation, &area))
	return FALSE;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	xalign = 1.0 - misc->xalign;
  
      x = floor (widget->allocation.x + misc->xpad
		 + ((widget->allocation.width - widget->requisition.width) * xalign));
      y = floor (widget->allocation.y + misc->ypad 
		 + ((widget->allocation.height - widget->requisition.height) * misc->yalign));
      mask_x = x;
      mask_y = y;
      
      image_bound.x = x;
      image_bound.y = y;      
      image_bound.width = 0;
      image_bound.height = 0;      

      mask = NULL;
      pixbuf = NULL;
      needs_state_transform = GTK_WIDGET_STATE (widget) != GTK_STATE_NORMAL;
      
      switch (image->storage_type)
        {
        case GTK_IMAGE_PIXMAP:
          mask = image->mask;
          gdk_drawable_get_size (image->data.pixmap.pixmap,
                                 &image_bound.width,
                                 &image_bound.height);
	  if (rectangle_intersect_even (&area, &image_bound) &&
	      needs_state_transform)
            {
              pixbuf = gdk_pixbuf_get_from_drawable (NULL,
                                                     image->data.pixmap.pixmap,
                                                     gtk_widget_get_colormap (widget),
                                                     image_bound.x - x, image_bound.y - y,
						     0, 0,
                                                     image_bound.width,
                                                     image_bound.height);

	      x = image_bound.x;
	      y = image_bound.y;
            }
	  
          break;

        case GTK_IMAGE_IMAGE:
          mask = image->mask;
          image_bound.width = image->data.image.image->width;
          image_bound.height = image->data.image.image->height;

	  if (rectangle_intersect_even (&area, &image_bound) &&
	      needs_state_transform)
            {
              pixbuf = gdk_pixbuf_get_from_image (NULL,
                                                  image->data.image.image,
                                                  gtk_widget_get_colormap (widget),
						  image_bound.x - x, image_bound.y - y,
                                                  0, 0,
                                                  image_bound.width,
                                                  image_bound.height);

	      x = image_bound.x;
	      y = image_bound.y;
            }
          break;

        case GTK_IMAGE_PIXBUF:
          image_bound.width = gdk_pixbuf_get_width (image->data.pixbuf.pixbuf);
          image_bound.height = gdk_pixbuf_get_height (image->data.pixbuf.pixbuf);            

	  if (rectangle_intersect_even (&area, &image_bound) &&
	      needs_state_transform)
	    {
	      pixbuf = gdk_pixbuf_new_subpixbuf (image->data.pixbuf.pixbuf,
						 image_bound.x - x, image_bound.y - y,
						 image_bound.width, image_bound.height);

	      x = image_bound.x;
	      y = image_bound.y;
	    }
	  else
	    {
	      pixbuf = image->data.pixbuf.pixbuf;
	      g_object_ref (pixbuf);
	    }
          break;

        case GTK_IMAGE_STOCK:
          pixbuf = gtk_widget_render_icon (widget,
                                           image->data.stock.stock_id,
                                           image->icon_size,
                                           NULL);
          if (pixbuf)
            {              
              image_bound.width = gdk_pixbuf_get_width (pixbuf);
              image_bound.height = gdk_pixbuf_get_height (pixbuf);
            }

          /* already done */
          needs_state_transform = FALSE;
          break;

        case GTK_IMAGE_ICON_SET:
          pixbuf =
            gtk_icon_set_render_icon (image->data.icon_set.icon_set,
                                      widget->style,
                                      gtk_widget_get_direction (widget),
                                      GTK_WIDGET_STATE (widget),
                                      image->icon_size,
                                      widget,
                                      NULL);

          if (pixbuf)
            {
              image_bound.width = gdk_pixbuf_get_width (pixbuf);
              image_bound.height = gdk_pixbuf_get_height (pixbuf);
            }

          /* already done */
          needs_state_transform = FALSE;
          break;

        case GTK_IMAGE_ANIMATION:
          {
            if (image->data.anim.iter == NULL)
              {
                image->data.anim.iter = gdk_pixbuf_animation_get_iter (image->data.anim.anim, NULL);
                
                if (gdk_pixbuf_animation_iter_get_delay_time (image->data.anim.iter) >= 0)
                  image->data.anim.frame_timeout =
                    g_timeout_add (gdk_pixbuf_animation_iter_get_delay_time (image->data.anim.iter),
                                   animation_timeout,
                                   image);
              }

            image_bound.width = gdk_pixbuf_animation_get_width (image->data.anim.anim);
            image_bound.height = gdk_pixbuf_animation_get_height (image->data.anim.anim);
                  
            /* don't advance the anim iter here, or we could get frame changes between two
             * exposes of different areas.
             */
            
            pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (image->data.anim.iter);
            g_object_ref (pixbuf);
          }
          break;

	case GTK_IMAGE_ICON_NAME:
	  ensure_pixbuf_for_icon_name (image);
	  pixbuf = image->data.name.pixbuf;
	  if (pixbuf)
	    {
	      g_object_ref (pixbuf);
	      image_bound.width = gdk_pixbuf_get_width (pixbuf);
	      image_bound.height = gdk_pixbuf_get_height (pixbuf);
	    }
	  break;

        case GTK_IMAGE_EMPTY:
          g_assert_not_reached ();
          break;
        }

      if (mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, mask);
	  gdk_gc_set_clip_origin (widget->style->black_gc, mask_x, mask_y);
	}

      if (rectangle_intersect_even (&area, &image_bound))
        {
          if (pixbuf)
            {
              if (needs_state_transform)
                {
                  GtkIconSource *source;
                  GdkPixbuf *rendered;

                  source = gtk_icon_source_new ();
                  gtk_icon_source_set_pixbuf (source, pixbuf);
                  /* The size here is arbitrary; since size isn't
                   * wildcarded in the souce, it isn't supposed to be
                   * scaled by the engine function
                   */
                  gtk_icon_source_set_size (source,
                                            GTK_ICON_SIZE_SMALL_TOOLBAR);
                  gtk_icon_source_set_size_wildcarded (source, FALSE);
                  
                  rendered = gtk_style_render_icon (widget->style,
                                                    source,
                                                    gtk_widget_get_direction (widget),
                                                    GTK_WIDGET_STATE (widget),
                                                    /* arbitrary */
                                                    (GtkIconSize)-1,
                                                    widget,
                                                    "gtk-image");

                  gtk_icon_source_free (source);

                  g_object_unref (pixbuf);
                  pixbuf = rendered;
                }

              if (pixbuf)
                {
                  gdk_draw_pixbuf (widget->window,
				   widget->style->black_gc,
				   pixbuf,
				   image_bound.x - x,
				   image_bound.y - y,
				   image_bound.x,
				   image_bound.y,
				   image_bound.width,
				   image_bound.height,
				   GDK_RGB_DITHER_NORMAL,
				   0, 0);
                }
            }
          else
            {
              switch (image->storage_type)
                {
                case GTK_IMAGE_PIXMAP:
                  gdk_draw_drawable (widget->window,
                                     widget->style->black_gc,
                                     image->data.pixmap.pixmap,
                                     image_bound.x - x, image_bound.y - y,
                                     image_bound.x, image_bound.y,
                                     image_bound.width, image_bound.height);
                  break;
              
                case GTK_IMAGE_IMAGE:
                  gdk_draw_image (widget->window,
                                  widget->style->black_gc,
                                  image->data.image.image,
                                  image_bound.x - x, image_bound.y - y,
                                  image_bound.x, image_bound.y,
                                  image_bound.width, image_bound.height);
                  break;

                case GTK_IMAGE_PIXBUF:
                case GTK_IMAGE_STOCK:
                case GTK_IMAGE_ICON_SET:
                case GTK_IMAGE_ANIMATION:
		case GTK_IMAGE_ICON_NAME:
                case GTK_IMAGE_EMPTY:
                  g_assert_not_reached ();
                  break;
                }
            }
        } /* if rectangle intersects */      

      if (mask)
        {
          gdk_gc_set_clip_mask (widget->style->black_gc, NULL);
          gdk_gc_set_clip_origin (widget->style->black_gc, 0, 0);
        }
      
      if (pixbuf)
	g_object_unref (pixbuf);

    } /* if widget is drawable */

  return FALSE;
}

static void
gtk_image_reset (GtkImage *image)
{
  GtkImagePrivate *priv;

  priv = GTK_IMAGE_GET_PRIVATE (image);

  g_object_freeze_notify (G_OBJECT (image));
  
  if (image->storage_type != GTK_IMAGE_EMPTY)
    g_object_notify (G_OBJECT (image), "storage-type");

  if (image->mask)
    {
      g_object_unref (image->mask);
      image->mask = NULL;
      g_object_notify (G_OBJECT (image), "mask");
    }

  if (image->icon_size != DEFAULT_ICON_SIZE)
    {
      image->icon_size = DEFAULT_ICON_SIZE;
      g_object_notify (G_OBJECT (image), "icon-size");
    }
  
  switch (image->storage_type)
    {
    case GTK_IMAGE_PIXMAP:

      if (image->data.pixmap.pixmap)
        g_object_unref (image->data.pixmap.pixmap);
      image->data.pixmap.pixmap = NULL;
      
      g_object_notify (G_OBJECT (image), "pixmap");
      
      break;

    case GTK_IMAGE_IMAGE:

      if (image->data.image.image)
        g_object_unref (image->data.image.image);
      image->data.image.image = NULL;
      
      g_object_notify (G_OBJECT (image), "image");
      
      break;

    case GTK_IMAGE_PIXBUF:

      if (image->data.pixbuf.pixbuf)
        g_object_unref (image->data.pixbuf.pixbuf);

      g_object_notify (G_OBJECT (image), "pixbuf");
      
      break;

    case GTK_IMAGE_STOCK:

      g_free (image->data.stock.stock_id);

      image->data.stock.stock_id = NULL;
      
      g_object_notify (G_OBJECT (image), "stock");      
      break;

    case GTK_IMAGE_ICON_SET:
      if (image->data.icon_set.icon_set)
        gtk_icon_set_unref (image->data.icon_set.icon_set);
      image->data.icon_set.icon_set = NULL;
      
      g_object_notify (G_OBJECT (image), "icon-set");      
      break;

    case GTK_IMAGE_ANIMATION:
      gtk_image_reset_anim_iter (image);
      
      if (image->data.anim.anim)
        g_object_unref (image->data.anim.anim);
      image->data.anim.anim = NULL;
      
      g_object_notify (G_OBJECT (image), "pixbuf-animation");
      
      break;

    case GTK_IMAGE_ICON_NAME:
      if (image->data.name.icon_name)
	g_free (image->data.name.icon_name);
      image->data.name.icon_name = NULL;
      if (image->data.name.pixbuf)
	g_object_unref (image->data.name.pixbuf);
      image->data.name.pixbuf = NULL;

      g_object_notify (G_OBJECT (image), "icon-name");

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

  image->storage_type = GTK_IMAGE_EMPTY;

  memset (&image->data, '\0', sizeof (image->data));

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

  gtk_image_update_size (image, 0, 0);
}

static void
gtk_image_calc_size (GtkImage *image)
{
  GtkWidget *widget = GTK_WIDGET (image);
  GdkPixbuf *pixbuf = NULL;
  
  /* We update stock/icon set on every size request, because
   * the theme could have affected the size; for other kinds of
   * image, we just update the requisition when the image data
   * is set.
   */
  switch (image->storage_type)
    {
    case GTK_IMAGE_STOCK:
      pixbuf = gtk_widget_render_icon (widget,
				       image->data.stock.stock_id,
                                       image->icon_size,
                                       NULL);
      break;
      
    case GTK_IMAGE_ICON_SET:
      pixbuf = gtk_icon_set_render_icon (image->data.icon_set.icon_set,
                                         widget->style,
                                         gtk_widget_get_direction (widget),
                                         GTK_WIDGET_STATE (widget),
                                         image->icon_size,
                                         widget,
                                         NULL);
      break;
    case GTK_IMAGE_ICON_NAME:
      ensure_pixbuf_for_icon_name (image);
      pixbuf = image->data.name.pixbuf;
      if (pixbuf) g_object_ref (pixbuf);
      break;
    default:
      break;
    }

  if (pixbuf)
    {
      widget->requisition.width = gdk_pixbuf_get_width (pixbuf) + GTK_MISC (image)->xpad * 2;
      widget->requisition.height = gdk_pixbuf_get_height (pixbuf) + GTK_MISC (image)->ypad * 2;

      g_object_unref (pixbuf);
    }
}

static void
gtk_image_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
  GtkImage *image;
  
  image = GTK_IMAGE (widget);

  gtk_image_calc_size (image);

  /* Chain up to default that simply reads current requisition */
  GTK_WIDGET_CLASS (gtk_image_parent_class)->size_request (widget, requisition);
}

static void
gtk_image_style_set (GtkWidget      *widget,
		     GtkStyle       *prev_style)
{
  GtkImage *image;

  image = GTK_IMAGE (widget);

  if (GTK_WIDGET_CLASS (gtk_image_parent_class)->style_set)
    GTK_WIDGET_CLASS (gtk_image_parent_class)->style_set (widget, prev_style);
  
  icon_theme_changed (image);
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


static void
gtk_image_update_size (GtkImage *image,
                       gint      image_width,
                       gint      image_height)
{
  GTK_WIDGET (image)->requisition.width = image_width + GTK_MISC (image)->xpad * 2;
  GTK_WIDGET (image)->requisition.height = image_height + GTK_MISC (image)->ypad * 2;

  if (GTK_WIDGET_VISIBLE (image))
    gtk_widget_queue_resize (GTK_WIDGET (image));
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
  GtkImagePrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE (image));
  
  priv = GTK_IMAGE_GET_PRIVATE (image);

  if (priv->pixel_size != pixel_size)
    {
      priv->pixel_size = pixel_size;
      
      if (image->storage_type == GTK_IMAGE_ICON_NAME)
	{
	  if (image->data.name.pixbuf)
	    {
	      g_object_unref (image->data.name.pixbuf);
	      image->data.name.pixbuf = NULL;
	    }
	  
	  gtk_image_update_size (image, pixel_size, pixel_size);
	}
      
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
  GtkImagePrivate *priv;

  g_return_val_if_fail (GTK_IS_IMAGE (image), -1);
  
  priv = GTK_IMAGE_GET_PRIVATE (image);

  return priv->pixel_size;
}

#ifdef G_OS_WIN32

#undef gtk_image_new_from_file

GtkWidget*
gtk_image_new_from_file   (const gchar *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
  GtkWidget *retval;

  retval = gtk_image_new_from_file_utf8 (utf8_filename);

  g_free (utf8_filename);

  return retval;
}

#undef gtk_image_set_from_file

void
gtk_image_set_from_file   (GtkImage    *image,
                           const gchar *filename)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);

  gtk_image_set_from_file_utf8 (image, utf8_filename);

  g_free (utf8_filename);
}

#endif

#define __GTK_IMAGE_C__
#include "gtkaliasdef.c"
