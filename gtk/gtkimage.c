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

#include <math.h>
#include "gtkcontainer.h"
#include "gtkimage.h"
#include "gtkiconfactory.h"
#include "gtkstock.h"
#include "gtkintl.h"
#include <string.h>

#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_BUTTON

static void gtk_image_class_init   (GtkImageClass  *klass);
static void gtk_image_init         (GtkImage       *image);
static gint gtk_image_expose       (GtkWidget      *widget,
                                    GdkEventExpose *event);
static void gtk_image_unmap        (GtkWidget      *widget);
static void gtk_image_size_request (GtkWidget      *widget,
                                    GtkRequisition *requisition);
static void gtk_image_destroy      (GtkObject      *object);
static void gtk_image_clear        (GtkImage       *image);
static void gtk_image_reset        (GtkImage       *image);
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

static gpointer parent_class;

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
  PROP_PIXBUF_ANIMATION,
  PROP_STORAGE_TYPE
};

GtkType
gtk_image_get_type (void)
{
  static GtkType image_type = 0;

  if (!image_type)
    {
      static const GtkTypeInfo image_info =
      {
	"GtkImage",
	sizeof (GtkImage),
	sizeof (GtkImageClass),
	(GtkClassInitFunc) gtk_image_class_init,
	(GtkObjectInitFunc) gtk_image_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      image_type = gtk_type_unique (GTK_TYPE_MISC, &image_info);
    }

  return image_type;
}

static void
gtk_image_class_init (GtkImageClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (class);

  gobject_class = G_OBJECT_CLASS (class);
  
  gobject_class->set_property = gtk_image_set_property;
  gobject_class->get_property = gtk_image_get_property;
  
  object_class = GTK_OBJECT_CLASS (class);
  
  object_class->destroy = gtk_image_destroy;

  widget_class = GTK_WIDGET_CLASS (class);
  
  widget_class->expose_event = gtk_image_expose;
  widget_class->size_request = gtk_image_size_request;
  widget_class->unmap = gtk_image_unmap;
  
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF,
                                   g_param_spec_object ("pixbuf",
                                                        _("Pixbuf"),
                                                        _("A GdkPixbuf to display."),
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PIXMAP,
                                   g_param_spec_object ("pixmap",
                                                        _("Pixmap"),
                                                        _("A GdkPixmap to display."),
                                                        GDK_TYPE_PIXMAP,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        _("Image"),
                                                        _("A GdkImage to display."),
                                                        GDK_TYPE_IMAGE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MASK,
                                   g_param_spec_object ("mask",
                                                        _("Mask"),
                                                        _("Mask bitmap to use with GdkImage or GdkPixmap"),
                                                        GDK_TYPE_PIXMAP,
                                                        G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_string ("file",
                                                        _("Filename"),
                                                        _("Filename to load and siplay."),
                                                        NULL,
                                                        G_PARAM_WRITABLE));
  

  g_object_class_install_property (gobject_class,
                                   PROP_STOCK,
                                   g_param_spec_string ("stock",
                                                        _("Stock ID"),
                                                        _("Stock ID for a stock image to display."),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SET,
                                   g_param_spec_boxed ("icon_set",
                                                       _("Icon set"),
                                                       _("Icon set to display."),
                                                       GTK_TYPE_ICON_SET,
                                                       G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_SIZE,
                                   g_param_spec_int ("icon_size",
                                                     _("Icon size"),
                                                     _("Size to use for stock icon or icon set."),
                                                     0, G_MAXINT,
                                                     DEFAULT_ICON_SIZE,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF_ANIMATION,
                                   g_param_spec_object ("pixbuf_animation",
                                                        _("Animation"),
                                                        _("GdkPixbufAnimation to display."),
                                                        GDK_TYPE_PIXBUF_ANIMATION,
                                                        G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_STORAGE_TYPE,
                                   g_param_spec_enum ("storage_type",
                                                      _("Storage type"),
                                                      _("The representation being used for image data."),
                                                      GTK_TYPE_IMAGE_TYPE,
                                                      GTK_IMAGE_EMPTY,
                                                      G_PARAM_READABLE));
}

static void
gtk_image_init (GtkImage *image)
{
  GTK_WIDGET_SET_FLAGS (image, GTK_NO_WINDOW);

  image->storage_type = GTK_IMAGE_EMPTY;
  image->icon_size = DEFAULT_ICON_SIZE;
  image->mask = NULL;
}

static void
gtk_image_destroy (GtkObject *object)
{
  GtkImage *image = GTK_IMAGE (object);

  gtk_image_clear (image);
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
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
            g_object_ref (G_OBJECT (mask));
          
          gtk_image_reset (image);

          image->mask = mask;
        }
      break;
    case PROP_FILE:
      gtk_image_set_from_file (image,
                               g_value_get_string (value));
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
      else
        /* Save to be used when STOCK or ICON_SET property comes in */
        image->icon_size = g_value_get_int (value);
      break;
    case PROP_PIXBUF_ANIMATION:
      gtk_image_set_from_animation (image,
                                    g_value_get_object (value));
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

  image = GTK_IMAGE (object);

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
    case PROP_PIXBUF_ANIMATION:
      if (image->storage_type != GTK_IMAGE_ANIMATION)
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value,
                            image->data.anim.anim);
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
 * A #GdkImage is a server-side image buffer in the pixel format of the
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

  image = gtk_type_new (GTK_TYPE_IMAGE);

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

  image = gtk_type_new (GTK_TYPE_IMAGE);

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

  image = gtk_type_new (GTK_TYPE_IMAGE);

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

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_pixbuf (image, pixbuf);

  return GTK_WIDGET (image);  
}

/**
 * gtk_image_new_from_stock:
 * @stock_id: a stock icon name
 * @size: a stock icon size
 * 
 * Creates a #GtkImage displaying a stock icon. Sample stock icon
 * names are #GTK_STOCK_OPEN, #GTK_STOCK_EXIT. Sample stock sizes
 * are #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_SMALL_TOOLBAR. If the stock
 * icon name isn't known, a "broken image" icon will be displayed instead.
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

  image = gtk_type_new (GTK_TYPE_IMAGE);

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

  image = gtk_type_new (GTK_TYPE_IMAGE);

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
 * Return value: a new #GtkImage widget
 **/
GtkWidget*
gtk_image_new_from_animation (GdkPixbufAnimation *animation)
{
  GtkImage *image;

  g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION (animation), NULL);
  
  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_animation (image, animation);

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
    g_object_ref (G_OBJECT (pixmap));

  if (mask)
    g_object_ref (G_OBJECT (mask));

  gtk_image_reset (image);

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
    g_object_ref (G_OBJECT (gdk_image));

  if (mask)
    g_object_ref (G_OBJECT (mask));

  gtk_image_reset (image);

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
        g_object_unref (G_OBJECT (mask));
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
  GdkPixbufAnimation *anim;
  
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (filename != NULL);

  g_object_freeze_notify (G_OBJECT (image));
  
  gtk_image_reset (image);

  if (filename == NULL)
    {
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
    {
      gtk_image_set_from_pixbuf (image,
                                 gdk_pixbuf_animation_get_static_image (anim));
    }
  else
    {
      gtk_image_set_from_animation (image, anim);
    }

  g_object_unref (G_OBJECT (anim));

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
    g_object_ref (G_OBJECT (pixbuf));

  gtk_image_reset (image);

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
  
  gtk_image_reset (image);

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
  g_object_notify (G_OBJECT (image), "icon_size");
  
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
  
  gtk_image_reset (image);

  if (icon_set)
    {      
      image->storage_type = GTK_IMAGE_ICON_SET;
      
      image->data.icon_set.icon_set = icon_set;
      image->icon_size = size;

      /* Size is demand-computed in size request method
       * if we're an icon set
       */
    }
  
  g_object_notify (G_OBJECT (image), "icon_set");
  g_object_notify (G_OBJECT (image), "icon_size");
  
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
    g_object_ref (G_OBJECT (animation));

  gtk_image_reset (image);

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

  g_object_notify (G_OBJECT (image), "pixbuf_animation");
  
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
gtk_image_unmap (GtkWidget *widget)
{
  GtkImage *image;

  image = GTK_IMAGE (widget);

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
          g_object_unref (G_OBJECT (image->data.anim.iter));
          image->data.anim.iter = NULL;
        }
    }

  if (GTK_WIDGET_CLASS (parent_class)->unmap)
    GTK_WIDGET_CLASS (parent_class)->unmap (widget);
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

  GDK_THREADS_LEAVE ();

  return FALSE;
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
      gint x, y;
      GdkBitmap *mask;
      GdkPixbuf *pixbuf;
      gboolean needs_state_transform;
      
      image = GTK_IMAGE (widget);
      misc = GTK_MISC (widget);

      area = event->area;
      
      if (!gdk_rectangle_intersect (&area, &widget->allocation, &area))
	return FALSE;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	xalign = 1.0 - misc->xalign;
  
      x = floor (widget->allocation.x + misc->xpad
		 + ((widget->allocation.width - widget->requisition.width) * xalign)
		 + 0.5);
      y = floor (widget->allocation.y + misc->ypad 
		 + ((widget->allocation.height - widget->requisition.height) * misc->yalign)
		 + 0.5);
      
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

	  if (gdk_rectangle_intersect (&image_bound, &area, &image_bound) &&
	      needs_state_transform)
            {
              pixbuf = gdk_pixbuf_get_from_drawable (NULL,
                                                     image->data.pixmap.pixmap,
                                                     gtk_widget_get_colormap (widget),
                                                     image_bound.x, image_bound.y,
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

	  if (gdk_rectangle_intersect (&image_bound, &area, &image_bound) &&
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

	  if (gdk_rectangle_intersect (&image_bound, &area, &image_bound) &&
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
	      g_object_ref (G_OBJECT (pixbuf));
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
            g_object_ref (G_OBJECT (pixbuf));
          }
          break;

        case GTK_IMAGE_EMPTY:
          g_assert_not_reached ();
          break;
        }

      if (mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, mask);
	  gdk_gc_set_clip_origin (widget->style->black_gc, x, y);
	}

      if (gdk_rectangle_intersect (&image_bound, &area, &image_bound))
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

                  g_object_unref (G_OBJECT (pixbuf));
                  pixbuf = rendered;
                }

              if (pixbuf)
                {
                  gdk_pixbuf_render_to_drawable (pixbuf,
						 widget->window,
						 widget->style->black_gc,						 
						 image_bound.x - x,
						 image_bound.y - y,
						 image_bound.x,
						 image_bound.y,
						 image_bound.width,
						 image_bound.height,
						 GDK_RGB_DITHER_NORMAL,
						 0, 0);

                  g_object_unref (G_OBJECT (pixbuf));
                  pixbuf = NULL;
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
      
    } /* if widget is drawable */

  return FALSE;
}

static void
gtk_image_clear (GtkImage *image)
{
  g_object_freeze_notify (G_OBJECT (image));
  
  if (image->storage_type != GTK_IMAGE_EMPTY)
    g_object_notify (G_OBJECT (image), "storage_type");

  if (image->mask)
    {
      g_object_unref (G_OBJECT (image->mask));
      image->mask = NULL;
      g_object_notify (G_OBJECT (image), "mask");
    }

  if (image->icon_size != DEFAULT_ICON_SIZE)
    {
      image->icon_size = DEFAULT_ICON_SIZE;
      g_object_notify (G_OBJECT (image), "icon_size");
    }
  
  switch (image->storage_type)
    {
    case GTK_IMAGE_PIXMAP:

      if (image->data.pixmap.pixmap)
        g_object_unref (G_OBJECT (image->data.pixmap.pixmap));
      image->data.pixmap.pixmap = NULL;
      
      g_object_notify (G_OBJECT (image), "pixmap");
      
      break;

    case GTK_IMAGE_IMAGE:

      if (image->data.image.image)
        g_object_unref (G_OBJECT (image->data.image.image));
      image->data.image.image = NULL;
      
      g_object_notify (G_OBJECT (image), "image");
      
      break;

    case GTK_IMAGE_PIXBUF:

      if (image->data.pixbuf.pixbuf)
        g_object_unref (G_OBJECT (image->data.pixbuf.pixbuf));

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
      
      g_object_notify (G_OBJECT (image), "icon_set");      
      break;

    case GTK_IMAGE_ANIMATION:
      if (image->data.anim.frame_timeout)
        g_source_remove (image->data.anim.frame_timeout);
      
      if (image->data.anim.anim)
        g_object_unref (G_OBJECT (image->data.anim.anim));

      image->data.anim.frame_timeout = 0;
      image->data.anim.anim = NULL;
      
      g_object_notify (G_OBJECT (image), "pixbuf_animation");
      
      break;
      
    case GTK_IMAGE_EMPTY:
    default:
      break;
      
    }

  image->storage_type = GTK_IMAGE_EMPTY;

  memset (&image->data, '\0', sizeof (image->data));

  g_object_thaw_notify (G_OBJECT (image));
}

static void
gtk_image_reset (GtkImage *image)
{
  gtk_image_clear (image);

  gtk_image_update_size (image, 0, 0);
}

static void
gtk_image_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
  GtkImage *image;
  GdkPixbuf *pixbuf = NULL;
  
  image = GTK_IMAGE (widget);

  /* We update stock/icon set on every size request, because
   * the theme could have affected the size; for other kinds of
   * image, we just update the requisition when the image data
   * is set.
   */
  
  switch (image->storage_type)
    {
    case GTK_IMAGE_STOCK:
      pixbuf = gtk_widget_render_icon (GTK_WIDGET (image),
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
      
    default:
      break;
    }

  if (pixbuf)
    {
      GTK_WIDGET (image)->requisition.width = gdk_pixbuf_get_width (pixbuf) + GTK_MISC (image)->xpad * 2;
      GTK_WIDGET (image)->requisition.height = gdk_pixbuf_get_height (pixbuf) + GTK_MISC (image)->ypad * 2;

      g_object_unref (G_OBJECT (pixbuf));
    }

  /* Chain up to default that simply reads current requisition */
  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
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


