/* gtkcellrendererpixbuf.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include "gtkcellrendererpixbuf.h"
#include "gtkintl.h"

static void gtk_cell_renderer_pixbuf_get_property  (GObject                    *object,
						    guint                       param_id,
						    GValue                     *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_pixbuf_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_pixbuf_init       (GtkCellRendererPixbuf      *celltext);
static void gtk_cell_renderer_pixbuf_class_init (GtkCellRendererPixbufClass *class);
static void gtk_cell_renderer_pixbuf_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
						 gint                       *width,
						 gint                       *height);
static void gtk_cell_renderer_pixbuf_render     (GtkCellRenderer            *cell,
						 GdkWindow                  *window,
						 GtkWidget                  *widget,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 GdkRectangle               *expose_area,
						 guint                       flags);


enum {
	PROP_ZERO,
	PROP_PIXBUF
};


GtkType
gtk_cell_renderer_pixbuf_get_type (void)
{
	static GtkType cell_pixbuf_type = 0;

	if (!cell_pixbuf_type)
	{
		static const GTypeInfo cell_pixbuf_info =
		{
			sizeof (GtkCellRendererPixbufClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gtk_cell_renderer_pixbuf_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GtkCellRendererPixbuf),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gtk_cell_renderer_pixbuf_init,
		};

		cell_pixbuf_type = g_type_register_static (GTK_TYPE_CELL_RENDERER, "GtkCellRendererPixbuf", &cell_pixbuf_info, 0);
	}

	return cell_pixbuf_type;
}

static void
gtk_cell_renderer_pixbuf_init (GtkCellRendererPixbuf *cellpixbuf)
{
}

static void
gtk_cell_renderer_pixbuf_class_init (GtkCellRendererPixbufClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	object_class->get_property = gtk_cell_renderer_pixbuf_get_property;
	object_class->set_property = gtk_cell_renderer_pixbuf_set_property;

	cell_class->get_size = gtk_cell_renderer_pixbuf_get_size;
	cell_class->render = gtk_cell_renderer_pixbuf_render;

	g_object_class_install_property (object_class,
					 PROP_PIXBUF,
					 g_param_spec_object ("pixbuf",
							      _("Pixbuf Object"),
							      _("The pixbuf to render."),
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READABLE |
							      G_PARAM_WRITABLE));
}

static void
gtk_cell_renderer_pixbuf_get_property (GObject        *object,
				       guint           param_id,
				       GValue         *value,
				       GParamSpec     *pspec)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  
  switch (param_id)
    {
    case PROP_PIXBUF:
      g_value_set_object (value,
                          cellpixbuf->pixbuf ? G_OBJECT (cellpixbuf->pixbuf) : NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
gtk_cell_renderer_pixbuf_set_property (GObject      *object,
				       guint         param_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GdkPixbuf *pixbuf;
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);
  
  switch (param_id)
    {
    case PROP_PIXBUF:
      pixbuf = (GdkPixbuf*) g_value_get_object (value);
      if (pixbuf)
        g_object_ref (G_OBJECT (pixbuf));
      if (cellpixbuf->pixbuf)
	g_object_unref (G_OBJECT (cellpixbuf->pixbuf));
      cellpixbuf->pixbuf = pixbuf;
      g_object_notify (object, "pixbuf");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_pixbuf_new:
 * 
 * Creates a new #GtkCellRendererPixbuf. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "pixbuf" property on the cell renderer to a pixbuf value
 * in the model, thus rendering a different image in each row of the
 * #GtkTreeView.
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_pixbuf_new (void)
{
  return GTK_CELL_RENDERER (gtk_type_new (gtk_cell_renderer_pixbuf_get_type ()));
}

static void
gtk_cell_renderer_pixbuf_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   gint            *width,
				   gint            *height)
{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
  
  if (width)
    *width = (gint) GTK_CELL_RENDERER (cellpixbuf)->xpad * 2 +
      (cellpixbuf->pixbuf ? gdk_pixbuf_get_width (cellpixbuf->pixbuf) : 0);
  
  if (height)
    *height = (gint) GTK_CELL_RENDERER (cellpixbuf)->ypad * 2 +
      (cellpixbuf->pixbuf ? gdk_pixbuf_get_height (cellpixbuf->pixbuf) : 0);
}

static void
gtk_cell_renderer_pixbuf_render (GtkCellRenderer    *cell,
				 GdkWindow          *window,
				 GtkWidget          *widget,
				 GdkRectangle       *background_area,
				 GdkRectangle       *cell_area,
				 GdkRectangle       *expose_area,
				 guint               flags)

{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
  GdkPixbuf *pixbuf;
  guchar *pixels;
  gint rowstride;
  gint real_xoffset;
  gint real_yoffset;
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;

  pixbuf = cellpixbuf->pixbuf;

  if (!pixbuf)
    return;

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  real_xoffset = GTK_CELL_RENDERER (cellpixbuf)->xalign * (cell_area->width - gdk_pixbuf_get_width (pixbuf) - (2 * GTK_CELL_RENDERER (cellpixbuf)->xpad));
  real_xoffset = MAX (real_xoffset, 0) + GTK_CELL_RENDERER (cellpixbuf)->xpad;
  real_yoffset = GTK_CELL_RENDERER (cellpixbuf)->yalign * (cell_area->height - gdk_pixbuf_get_height (pixbuf) - (2 * GTK_CELL_RENDERER (cellpixbuf)->ypad));
  real_yoffset = MAX (real_yoffset, 0) + GTK_CELL_RENDERER (cellpixbuf)->ypad;

  pix_rect.x = cell_area->x + real_xoffset;
  pix_rect.y = cell_area->y + real_yoffset;
  pix_rect.width = gdk_pixbuf_get_width (pixbuf);
  pix_rect.height = gdk_pixbuf_get_height (pixbuf);

  if (gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect))
    gdk_pixbuf_render_to_drawable_alpha (pixbuf,
                                         window,
                                         /* pixbuf 0, 0 is at pix_rect.x, pix_rect.y */
                                         draw_rect.x - pix_rect.x,
                                         draw_rect.y - pix_rect.y,
                                         draw_rect.x,
                                         draw_rect.y,
                                         draw_rect.width,
                                         draw_rect.height,
                                         GDK_PIXBUF_ALPHA_FULL,
                                         0,
                                         GDK_RGB_DITHER_NORMAL,
                                         0, 0);
}
