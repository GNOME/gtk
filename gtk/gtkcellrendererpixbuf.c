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
static void gtk_cell_renderer_pixbuf_finalize   (GObject                    *object);
static void gtk_cell_renderer_pixbuf_create_stock_pixbuf (GtkCellRendererPixbuf *cellpixbuf,
							  GtkWidget             *widget);
static void gtk_cell_renderer_pixbuf_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
						 GdkRectangle               *rectangle,
						 gint                       *x_offset,
						 gint                       *y_offset,
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
	PROP_PIXBUF,
	PROP_PIXBUF_EXPANDER_OPEN,
	PROP_PIXBUF_EXPANDER_CLOSED,
	PROP_STOCK_ID,
	PROP_STOCK_SIZE,
	PROP_STOCK_DETAIL
};

static gpointer parent_class;


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
	cellpixbuf->stock_size = GTK_ICON_SIZE_MENU;
}

static void
gtk_cell_renderer_pixbuf_class_init (GtkCellRendererPixbufClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = gtk_cell_renderer_pixbuf_finalize;

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

	g_object_class_install_property (object_class,
					 PROP_PIXBUF_EXPANDER_OPEN,
					 g_param_spec_object ("pixbuf_expander_open",
							      _("Pixbuf Expander Open"),
							      _("Pixbuf for open expander."),
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READABLE |
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_PIXBUF_EXPANDER_CLOSED,
					 g_param_spec_object ("pixbuf_expander_closed",
							      _("Pixbuf Expander Closed"),
							      _("Pixbuf for closed expander."),
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READABLE |
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
                                         PROP_STOCK_ID,
                                         g_param_spec_string ("stock_id",
                                                              _("Stock ID"),
                                                              _("The stock ID of the stock icon to render"),
                                                              NULL,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_STOCK_SIZE,
                                         g_param_spec_enum ("stock_size",
                                                            _("Size"),
                                                            _("The size of the rendered icon"),
                                                            GTK_TYPE_ICON_SIZE,
                                                            GTK_ICON_SIZE_MENU,
                                                            G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_STOCK_DETAIL,
                                         g_param_spec_string ("stock_detail",
                                                              _("Detail"),
                                                              _("Render detail to pass to the theme engine"),
                                                              NULL,
                                                              G_PARAM_READWRITE));

}

static void
gtk_cell_renderer_pixbuf_finalize (GObject *object)
{
  GtkCellRendererPixbuf *cellpixbuf = GTK_CELL_RENDERER_PIXBUF (object);

  if (cellpixbuf->pixbuf && cellpixbuf->stock_id)
    g_object_unref (cellpixbuf->pixbuf);

  if (cellpixbuf->stock_id)
    g_free (cellpixbuf->stock_id);

  if (cellpixbuf->stock_detail)
    g_free (cellpixbuf->stock_detail);

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
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
    case PROP_PIXBUF_EXPANDER_OPEN:
      g_value_set_object (value,
                          cellpixbuf->pixbuf_expander_open ? G_OBJECT (cellpixbuf->pixbuf_expander_open) : NULL);
      break;
    case PROP_PIXBUF_EXPANDER_CLOSED:
      g_value_set_object (value,
                          cellpixbuf->pixbuf_expander_closed ? G_OBJECT (cellpixbuf->pixbuf_expander_closed) : NULL);
      break;
    case PROP_STOCK_ID:
      g_value_set_string (value, cellpixbuf->stock_id);
      break;
    case PROP_STOCK_SIZE:
      g_value_set_enum (value, cellpixbuf->stock_size);
      break;
    case PROP_STOCK_DETAIL:
      g_value_set_string (value, cellpixbuf->stock_detail);
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
      break;
    case PROP_PIXBUF_EXPANDER_OPEN:
      pixbuf = (GdkPixbuf*) g_value_get_object (value);
      if (pixbuf)
        g_object_ref (G_OBJECT (pixbuf));
      if (cellpixbuf->pixbuf_expander_open)
	g_object_unref (G_OBJECT (cellpixbuf->pixbuf_expander_open));
      cellpixbuf->pixbuf_expander_open = pixbuf;
      break;
    case PROP_PIXBUF_EXPANDER_CLOSED:
      pixbuf = (GdkPixbuf*) g_value_get_object (value);
      if (pixbuf)
        g_object_ref (G_OBJECT (pixbuf));
      if (cellpixbuf->pixbuf_expander_closed)
	g_object_unref (G_OBJECT (cellpixbuf->pixbuf_expander_closed));
      cellpixbuf->pixbuf_expander_closed = pixbuf;
      break;
    case PROP_STOCK_ID:
      if (cellpixbuf->stock_id)
        g_free (cellpixbuf->stock_id);
      cellpixbuf->stock_id = g_strdup (g_value_get_string (value));
      g_object_notify (G_OBJECT (object), "stock_id");
      break;
    case PROP_STOCK_SIZE:
      cellpixbuf->stock_size = g_value_get_enum (value);
      g_object_notify (G_OBJECT (object), "stock_size");
      break;
    case PROP_STOCK_DETAIL:
      if (cellpixbuf->stock_detail)
        g_free (cellpixbuf->stock_detail);
      cellpixbuf->stock_detail = g_strdup (g_value_get_string (value));
      g_object_notify (G_OBJECT (object), "stock_detail");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

  if (cellpixbuf->pixbuf && cellpixbuf->stock_id)
    {
      g_object_unref (cellpixbuf->pixbuf);
      cellpixbuf->pixbuf = NULL;
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
gtk_cell_renderer_pixbuf_create_stock_pixbuf (GtkCellRendererPixbuf *cellpixbuf,
					      GtkWidget             *widget)
{
  if (cellpixbuf->pixbuf)
    g_object_unref (G_OBJECT (cellpixbuf->pixbuf));

  cellpixbuf->pixbuf = gtk_widget_render_icon (widget,
					       cellpixbuf->stock_id,
					       cellpixbuf->stock_size,
					       cellpixbuf->stock_detail);
}

static void
gtk_cell_renderer_pixbuf_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   GdkRectangle    *cell_area,
				   gint            *x_offset,
				   gint            *y_offset,
				   gint            *width,
				   gint            *height)
{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;
  gint pixbuf_width = 0;
  gint pixbuf_height = 0;
  gint calc_width;
  gint calc_height;

  if (!cellpixbuf->pixbuf && cellpixbuf->stock_id)
    gtk_cell_renderer_pixbuf_create_stock_pixbuf (cellpixbuf, widget);

  if (cellpixbuf->pixbuf)
    {
      pixbuf_width = gdk_pixbuf_get_width (cellpixbuf->pixbuf);
      pixbuf_height = gdk_pixbuf_get_height (cellpixbuf->pixbuf);
    }
  if (cellpixbuf->pixbuf_expander_open)
    {
      pixbuf_width = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_open));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_open));
    }
  if (cellpixbuf->pixbuf_expander_closed)
    {
      pixbuf_width = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_closed));
      pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_closed));
    }
  
  calc_width = (gint) GTK_CELL_RENDERER (cellpixbuf)->xpad * 2 + pixbuf_width;
  calc_height = (gint) GTK_CELL_RENDERER (cellpixbuf)->ypad * 2 + pixbuf_height;
  
  if (x_offset) *x_offset = 0;
  if (y_offset) *y_offset = 0;

  if (cell_area && pixbuf_width > 0 && pixbuf_height > 0)
    {
      if (x_offset)
	{
	  *x_offset = GTK_CELL_RENDERER (cellpixbuf)->xalign * (cell_area->width - calc_width - (2 * GTK_CELL_RENDERER (cellpixbuf)->xpad));
	  *x_offset = MAX (*x_offset, 0) + GTK_CELL_RENDERER (cellpixbuf)->xpad;
	}
      if (y_offset)
	{
	  *y_offset = GTK_CELL_RENDERER (cellpixbuf)->yalign * (cell_area->height - calc_height - (2 * GTK_CELL_RENDERER (cellpixbuf)->ypad));
	  *y_offset = MAX (*y_offset, 0) + GTK_CELL_RENDERER (cellpixbuf)->ypad;
	}
    }

  if (calc_width)
    *width = calc_width;
  
  if (height)
    *height = calc_height;
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
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;
  gboolean stock_pixbuf = FALSE;

  pixbuf = cellpixbuf->pixbuf;
  if (cell->is_expander)
    {
      if (cell->is_expanded &&
	  cellpixbuf->pixbuf_expander_open != NULL)
	pixbuf = cellpixbuf->pixbuf_expander_open;
      else if (! cell->is_expanded &&
	       cellpixbuf->pixbuf_expander_closed != NULL)
	pixbuf = cellpixbuf->pixbuf_expander_closed;
    }

  if (!pixbuf && !cellpixbuf->stock_id)
    return;
  else if (!pixbuf && cellpixbuf->stock_id)
    stock_pixbuf = TRUE;

  gtk_cell_renderer_pixbuf_get_size (cell, widget, cell_area,
				     &pix_rect.x,
				     &pix_rect.y,
				     &pix_rect.width,
				     &pix_rect.height);

  if (stock_pixbuf)
    pixbuf = cellpixbuf->pixbuf;
  
  pix_rect.x += cell_area->x;
  pix_rect.y += cell_area->y;
  pix_rect.width -= cell->xpad * 2;
  pix_rect.height -= cell->ypad * 2;
  
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
