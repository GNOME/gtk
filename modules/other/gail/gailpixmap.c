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

#include <gtk/gtk.h>
#include "gailpixmap.h"

static void	 gail_pixmap_class_init		(GailPixmapClass *klass);
static void  gail_pixmap_object_init    (GailPixmap      *pixmap);

/* AtkImage */
static void  atk_image_interface_init   (AtkImageIface  *iface);
static G_CONST_RETURN gchar* gail_pixmap_get_image_description 
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

static GailWidgetClass* parent_class = NULL;

GType
gail_pixmap_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (GailPixmapClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gail_pixmap_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GailPixmap), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) gail_pixmap_object_init, /* instance init */
        NULL /* value table */
      };

      static const GInterfaceInfo atk_image_info =
      {
        (GInterfaceInitFunc) atk_image_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (GAIL_TYPE_WIDGET,
                                     "GailPixmap", &tinfo, 0);

      g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                   &atk_image_info);
    }
  return type;
}

static void	 
gail_pixmap_class_init (GailPixmapClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
 
  gobject_class->finalize = gail_pixmap_finalize;

  parent_class = g_type_class_peek_parent (klass);
}

static void
gail_pixmap_object_init (GailPixmap *pixmap)
{
  pixmap->image_description = NULL;
}

AtkObject* 
gail_pixmap_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_assert (GTK_IS_PIXMAP (widget));
  g_return_val_if_fail (GTK_IS_PIXMAP (widget), NULL);

  object = g_object_new (GAIL_TYPE_PIXMAP, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  accessible->role = ATK_ROLE_ICON;

  return accessible;
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_image_description = gail_pixmap_get_image_description;
  iface->get_image_position = gail_pixmap_get_image_position;
  iface->get_image_size = gail_pixmap_get_image_size;
  iface->set_image_description = gail_pixmap_set_image_description;
}

static G_CONST_RETURN gchar* 
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
    gdk_window_get_size (pixmap->pixmap, width, height);
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
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
