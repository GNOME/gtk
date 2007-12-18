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
#include "gailarrow.h"

static void	 gail_arrow_class_init		(GailArrowClass *klass);
static void  gail_arrow_object_init		(GailArrow		*arrow);

/* AtkImage */
static void  atk_image_interface_init   (AtkImageIface  *iface);
static G_CONST_RETURN gchar* gail_arrow_get_image_description 
                                        (AtkImage       *obj);
static gboolean gail_arrow_set_image_description 
                                        (AtkImage       *obj,
                                        const gchar    *description);
static void  gail_arrow_finalize       (GObject         *object);

static GailWidgetClass* parent_class = NULL;

GType
gail_arrow_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailArrowClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_arrow_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailArrow), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) gail_arrow_object_init, /* instance init */
      NULL /* value table */
    };

    static const GInterfaceInfo atk_image_info =
    {
        (GInterfaceInitFunc) atk_image_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    type = g_type_register_static (GAIL_TYPE_WIDGET,
                                   "GailArrow", &tinfo, 0);

    g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                 &atk_image_info);
  }
  return type;
}

static void	 
gail_arrow_class_init		(GailArrowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gail_arrow_finalize;

  parent_class = g_type_class_peek_parent (klass);
}

static void
gail_arrow_object_init (GailArrow *arrow)
{
  arrow->image_description = NULL;
}

AtkObject* 
gail_arrow_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_ARROW (widget), NULL);

  object = g_object_new (GAIL_TYPE_ARROW, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);
  accessible->role = ATK_ROLE_ICON;

  return accessible;
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_image_description = gail_arrow_get_image_description;
  iface->set_image_description = gail_arrow_set_image_description;
}

static G_CONST_RETURN gchar* 
gail_arrow_get_image_description (AtkImage       *obj)
{
  GailArrow* arrow;

  g_return_val_if_fail(GAIL_IS_ARROW(obj), NULL);

  arrow = GAIL_ARROW (obj);

  return arrow->image_description;

}


static gboolean 
gail_arrow_set_image_description (AtkImage       *obj,
                                  const gchar    *description)
{
  GailArrow* arrow;

  g_return_val_if_fail(GAIL_IS_ARROW(obj), FALSE);

  arrow = GAIL_ARROW (obj);
  g_free (arrow->image_description);

  arrow->image_description = g_strdup (description);

  return TRUE;

}

/*
 * static void  
 * gail_arrow_get_image_size (AtkImage       *obj,
 *                          gint           *height,
 *                          gint           *width)
 *
 * We dont implement this function for GailArrow as gtk hardcodes the size 
 * of the arrow to be 7x5 and it is not possible to query this.
 */

static void
gail_arrow_finalize (GObject      *object)
{
  GailArrow *arrow = GAIL_ARROW (object);

  g_free (arrow->image_description);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
