/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include "gailimage.h"

static void      gail_image_class_init         (GailImageClass *klass);
static void      gail_image_init               (GailImage      *image);
static void      gail_image_initialize         (AtkObject       *accessible,
                                                gpointer        data);
static const gchar* gail_image_get_name  (AtkObject     *accessible);


static void      atk_image_interface_init      (AtkImageIface  *iface);

static const gchar *
                 gail_image_get_image_description (AtkImage     *image);
static void	 gail_image_get_image_position    (AtkImage     *image,
                                                   gint         *x,
                                                   gint         *y,
                                                   AtkCoordType coord_type);
static void      gail_image_get_image_size     (AtkImage        *image,
                                                gint            *width,
                                                gint            *height);
static gboolean  gail_image_set_image_description (AtkImage     *image,
                                                const gchar     *description);
static void      gail_image_finalize           (GObject         *object);

G_DEFINE_TYPE_WITH_CODE (GailImage, gail_image, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void
gail_image_class_init (GailImageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gail_image_finalize;
  class->initialize = gail_image_initialize;
  class->get_name = gail_image_get_name;
}

static void
gail_image_init (GailImage *image)
{
  image->image_description = NULL;
}

static void
gail_image_initialize (AtkObject *accessible,
                       gpointer data)
{
  ATK_OBJECT_CLASS (gail_image_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_ICON;
}

/* Copied from gtktoolbar.c, keep in sync */
static gchar *
elide_underscores (const gchar *original)
{
  gchar *q, *result;
  const gchar *p, *end;
  gsize len;
  gboolean last_underscore;
  
  if (!original)
    return NULL;

  len = strlen (original);
  q = result = g_malloc (len + 1);
  last_underscore = FALSE;
  
  end = original + len;
  for (p = original; p < end; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  if (original + 2 <= p && p + 1 <= end && 
              p[-2] == '(' && p[-1] == '_' && p[0] != '_' && p[1] == ')')
	    {
	      q--;
	      *q = '\0';
	      p++;
	    }
	  else
	    *q++ = *p;
	}
    }

  if (last_underscore)
    *q++ = '_';
  
  *q = '\0';
  
  return result;
}

static const gchar*
gail_image_get_name (AtkObject *accessible)
{
  GtkWidget* widget;
  GtkImage *image;
  GailImage *image_accessible;
  GtkStockItem stock_item;
  const gchar *name;

  name = ATK_OBJECT_CLASS (gail_image_parent_class)->get_name (accessible);
  if (name)
    return name;

  widget = GTK_ACCESSIBLE (accessible)->widget;
  /*
   * State is defunct
   */
  if (widget == NULL)
    return NULL;

  g_return_val_if_fail (GTK_IS_IMAGE (widget), NULL);
  image = GTK_IMAGE (widget);
  image_accessible = GAIL_IMAGE (accessible);

  g_free (image_accessible->stock_name);
  image_accessible->stock_name = NULL;

  if (image->storage_type != GTK_IMAGE_STOCK ||
      image->data.stock.stock_id == NULL)
    return NULL;

  if (!gtk_stock_lookup (image->data.stock.stock_id, &stock_item))
    return NULL;

  image_accessible->stock_name = elide_underscores (stock_item.label);
  return image_accessible->stock_name;
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gail_image_get_image_description;
  iface->get_image_position = gail_image_get_image_position;
  iface->get_image_size = gail_image_get_image_size;
  iface->set_image_description = gail_image_set_image_description;
}

static const gchar *
gail_image_get_image_description (AtkImage     *image)
{
  GailImage* aimage = GAIL_IMAGE (image);

  return aimage->image_description;
}

static void
gail_image_get_image_position (AtkImage     *image,
                               gint         *x,
                               gint         *y,
                               AtkCoordType coord_type)
{
  atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
}

static void
gail_image_get_image_size (AtkImage *image, 
                           gint     *width,
                           gint     *height)
{
  GtkWidget* widget;
  GtkImage *gtk_image;
  GtkImageType image_type;

  widget = GTK_ACCESSIBLE (image)->widget;
  if (widget == 0)
  {
    /* State is defunct */
    *height = -1;
    *width = -1;
    return;
  }

  gtk_image = GTK_IMAGE(widget);

  image_type = gtk_image_get_storage_type(gtk_image);
 
  switch(image_type) {
    case GTK_IMAGE_PIXMAP:
    {	
      GdkPixmap *pixmap;
      gtk_image_get_pixmap(gtk_image, &pixmap, NULL);
      gdk_pixmap_get_size (pixmap, width, height);
      break;
    }
    case GTK_IMAGE_PIXBUF:
    {
      GdkPixbuf *pixbuf;
      pixbuf = gtk_image_get_pixbuf(gtk_image);
      *height = gdk_pixbuf_get_height(pixbuf);
      *width = gdk_pixbuf_get_width(pixbuf);
      break;
    }
    case GTK_IMAGE_IMAGE:
    {
      GdkImage *gdk_image;
      gtk_image_get_image(gtk_image, &gdk_image, NULL);
      *height = gdk_image->height;
      *width = gdk_image->width;
      break;
    }
    case GTK_IMAGE_STOCK:
    case GTK_IMAGE_ICON_SET:
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
    {
      GtkIconSize size;
      GtkSettings *settings;

      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (widget));

      g_object_get (gtk_image, "icon-size", &size, NULL);
      gtk_icon_size_lookup_for_settings (settings, size, width, height);
      break;
    }
    case GTK_IMAGE_ANIMATION:
    {
      GdkPixbufAnimation *animation;
      animation = gtk_image_get_animation(gtk_image);
      *height = gdk_pixbuf_animation_get_height(animation);
      *width = gdk_pixbuf_animation_get_width(animation);
      break;
    }
    default:
    {
      *height = -1;
      *width = -1;
      break;
    }
  }
}

static gboolean
gail_image_set_image_description (AtkImage     *image,
                                  const gchar  *description)
{
  GailImage* aimage = GAIL_IMAGE (image);

  g_free (aimage->image_description);
  aimage->image_description = g_strdup (description);
  return TRUE;
}

static void
gail_image_finalize (GObject      *object)
{
  GailImage *aimage = GAIL_IMAGE (object);

  g_free (aimage->image_description);
  g_free (aimage->stock_name);

  G_OBJECT_CLASS (gail_image_parent_class)->finalize (object);
}
