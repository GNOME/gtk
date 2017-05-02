/* GAIL - The GNOME Accessibility Enabling Library
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
#include "gailimagecell.h"

static void      gail_image_cell_class_init          (GailImageCellClass *klass);
static void      gail_image_cell_init                (GailImageCell      *image_cell);

static void      gail_image_cell_finalize            (GObject            *object);

/* AtkImage */
static void      atk_image_interface_init              (AtkImageIface  *iface);
static const gchar *
                 gail_image_cell_get_image_description (AtkImage       *image);
static gboolean  gail_image_cell_set_image_description (AtkImage       *image,
                                                        const gchar    *description);
static void      gail_image_cell_get_image_position    (AtkImage       *image,
                                                        gint           *x,
                                                        gint           *y,
                                                        AtkCoordType   coord_type);
static void      gail_image_cell_get_image_size        (AtkImage       *image,
                                                        gint           *width,
                                                        gint           *height);

/* Misc */

static gboolean  gail_image_cell_update_cache          (GailRendererCell *cell,
                                                        gboolean         emit_change_signal);

// FIXMEchpe static!!!
gchar *gail_image_cell_property_list[] = {
  "pixbuf",
  NULL
};

G_DEFINE_TYPE_WITH_CODE (GailImageCell, gail_image_cell, GAIL_TYPE_RENDERER_CELL,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void 
gail_image_cell_class_init (GailImageCellClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GailRendererCellClass *renderer_cell_class = GAIL_RENDERER_CELL_CLASS(klass);

  gobject_class->finalize = gail_image_cell_finalize;

  renderer_cell_class->update_cache = gail_image_cell_update_cache;
  renderer_cell_class->property_list = gail_image_cell_property_list;
}

AtkObject* 
gail_image_cell_new (void)
{
  GObject *object;
  AtkObject *atk_object;
  GailRendererCell *cell;

  object = g_object_new (GAIL_TYPE_IMAGE_CELL, NULL);

  g_return_val_if_fail (object != NULL, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object->role = ATK_ROLE_TABLE_CELL;

  cell = GAIL_RENDERER_CELL(object);

  cell->renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_ref_sink (cell->renderer);
  return atk_object;
}

static void
gail_image_cell_init (GailImageCell *image_cell)
{
  image_cell->image_description = NULL;
}


static void
gail_image_cell_finalize (GObject *object)
{
  GailImageCell *image_cell = GAIL_IMAGE_CELL (object);

  g_free (image_cell->image_description);
  G_OBJECT_CLASS (gail_image_cell_parent_class)->finalize (object);
}

static gboolean
gail_image_cell_update_cache (GailRendererCell *cell, 
                              gboolean         emit_change_signal)
{
  return FALSE;
}

static void
atk_image_interface_init (AtkImageIface  *iface)
{
  iface->get_image_description = gail_image_cell_get_image_description;
  iface->set_image_description = gail_image_cell_set_image_description;
  iface->get_image_position = gail_image_cell_get_image_position;
  iface->get_image_size = gail_image_cell_get_image_size;
}

static const gchar *
gail_image_cell_get_image_description (AtkImage     *image)
{
  GailImageCell *image_cell;

  image_cell = GAIL_IMAGE_CELL (image);
  return image_cell->image_description;
}

static gboolean
gail_image_cell_set_image_description (AtkImage     *image,
                                       const gchar  *description)
{
  GailImageCell *image_cell;

  image_cell = GAIL_IMAGE_CELL (image);
  g_free (image_cell->image_description);
  image_cell->image_description = g_strdup (description);
  if (image_cell->image_description)
    return TRUE;
  else
    return FALSE;
}

static void
gail_image_cell_get_image_position (AtkImage     *image,
                                    gint         *x,
                                    gint         *y,
                                    AtkCoordType coord_type)
{
  atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
}

static void
gail_image_cell_get_image_size (AtkImage *image,
                                gint     *width,
                                gint     *height)
{
  GailImageCell *cell = GAIL_IMAGE_CELL (image);
  GtkCellRenderer *cell_renderer;
  GdkPixbuf *pixbuf;

  cell_renderer  = GAIL_RENDERER_CELL (cell)->renderer;
  pixbuf = GTK_CELL_RENDERER_PIXBUF (cell_renderer)->pixbuf;

  *width = gdk_pixbuf_get_width (pixbuf);
  *height = gdk_pixbuf_get_height (pixbuf);
}
