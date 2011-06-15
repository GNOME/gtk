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

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#include "gailpixmap.h"

static void	 gail_pixmap_class_init		(GailPixmapClass *klass);
static void      gail_pixmap_init               (GailPixmap      *pixmap);
static void      gail_pixmap_initialize         (AtkObject       *accessible,
                                                 gpointer         data);

/* AtkImage */
static void  atk_image_interface_init   (AtkImageIface  *iface);
static const gchar* gail_pixmap_get_image_description
                                        (AtkImage       *obj);
static void  gail_pixmap_get_image_position    
                                        (AtkImage       *obj,
                                         gint           *x,
                                         gint           *y,
                                         AtkCoordType   coord_type);
static void  gail_pixmap_get_image_size (AtkImage       *obj,
                                         gint           *width,
                                         gint           *height);
static gboolean gail_pixmap_set_image_description 
                                        (AtkImage       *obj,
                                        const gchar    *description);
static void  gail_pixmap_finalize       (GObject         *object);

G_DEFINE_TYPE_WITH_CODE (GailPixmap, gail_pixmap, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void	 
gail_pixmap_class_init (GailPixmapClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
 
  atk_object_class->initialize = gail_pixmap_initialize;

  gobject_class->finalize = gail_pixmap_finalize;
}

static void
gail_pixmap_init (GailPixmap *pixmap)
{
  pixmap->image_description = NULL;
}

static void
gail_pixmap_initialize (AtkObject *accessible,
                        gpointer  data)
{
  ATK_OBJECT_CLASS (gail_pixmap_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_ICON;
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gail_pixmap_get_image_description;
  iface->get_image_position = gail_pixmap_get_image_position;
  iface->get_image_size = gail_pixmap_get_image_size;
  iface->set_image_description = gail_pixmap_set_image_description;
}

static const gchar*
gail_pixmap_get_image_description (AtkImage       *obj)
{
  GailPixmap* pixmap;

  g_return_val_if_fail (GAIL_IS_PIXMAP (obj), NULL);

  pixmap = GAIL_PIXMAP (obj);

  return pixmap->image_description;
}

static void
gail_pixmap_get_image_position (AtkImage       *obj,
                                gint           *x,
                                gint           *y,
                                AtkCoordType   coord_type)
{
  atk_component_get_position (ATK_COMPONENT (obj), x, y, coord_type);
}

static void  
gail_pixmap_get_image_size (AtkImage       *obj,
                            gint           *width,
                            gint           *height)
{
  GtkWidget *widget;
  GtkPixmap *pixmap;
 
  *width = -1;
  *height = -1;

  g_return_if_fail (GAIL_IS_PIXMAP (obj));

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == 0)
    /* State is defunct */
    return;

  g_return_if_fail (GTK_IS_PIXMAP (widget));

  pixmap = GTK_PIXMAP (widget);

  if (pixmap->pixmap)
    gdk_pixmap_get_size (pixmap->pixmap, width, height);
}

static gboolean 
gail_pixmap_set_image_description (AtkImage       *obj,
                                   const gchar    *description)
{ 
  GailPixmap* pixmap;

  g_return_val_if_fail (GAIL_IS_PIXMAP (obj), FALSE);

  pixmap = GAIL_PIXMAP (obj);
  g_free (pixmap->image_description);

  pixmap->image_description = g_strdup (description);

  return TRUE;
}

static void
gail_pixmap_finalize (GObject      *object)
{
  GailPixmap *pixmap = GAIL_PIXMAP (object);

  g_free (pixmap->image_description);
  G_OBJECT_CLASS (gail_pixmap_parent_class)->finalize (object);
}
