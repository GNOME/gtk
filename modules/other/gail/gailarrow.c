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

#include <gtk/gtk.h>
#include "gailarrow.h"

static void gail_arrow_class_init	(GailArrowClass *klass);
static void gail_arrow_init		(GailArrow	*arrow);
static void gail_arrow_initialize       (AtkObject      *accessible,
                                         gpointer        data);

/* AtkImage */
static void  atk_image_interface_init   (AtkImageIface  *iface);
static const gchar* gail_arrow_get_image_description
                                        (AtkImage       *obj);
static gboolean gail_arrow_set_image_description 
                                        (AtkImage       *obj,
                                        const gchar    *description);
static void  gail_arrow_finalize       (GObject         *object);

G_DEFINE_TYPE_WITH_CODE (GailArrow, gail_arrow, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void	 
gail_arrow_class_init		(GailArrowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

  atk_object_class->initialize = gail_arrow_initialize;

  gobject_class->finalize = gail_arrow_finalize;
}

static void
gail_arrow_init (GailArrow *arrow)
{
  arrow->image_description = NULL;
}

static void
gail_arrow_initialize (AtkObject *accessible,
                       gpointer data)
{
  ATK_OBJECT_CLASS (gail_arrow_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_ICON;
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gail_arrow_get_image_description;
  iface->set_image_description = gail_arrow_set_image_description;
}

static const gchar*
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
  G_OBJECT_CLASS (gail_arrow_parent_class)->finalize (object);
}
