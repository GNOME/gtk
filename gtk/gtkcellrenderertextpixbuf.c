/* gtkcellrenderertextpixbuf.c
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
#include "gtkcellrenderertextpixbuf.h"
#include "gtkintl.h"

enum {
  PROP_ZERO,
  PROP_PIXBUF_POS,
  PROP_PIXBUF,
  PROP_PIXBUF_XALIGN,
  PROP_PIXBUF_YALIGN,
  PROP_PIXBUF_XPAD,
  PROP_PIXBUF_YPAD
};


static void gtk_cell_renderer_text_pixbuf_get_property  (GObject                        *object,
							 guint                           param_id,
							 GValue                         *value,
							 GParamSpec                     *pspec);
static void gtk_cell_renderer_text_pixbuf_set_property  (GObject                        *object,
							 guint                           param_id,
							 const GValue                   *value,
							 GParamSpec                     *pspec);
static void gtk_cell_renderer_text_pixbuf_init       (GtkCellRendererTextPixbuf      *celltextpixbuf);
static void gtk_cell_renderer_text_pixbuf_class_init (GtkCellRendererTextPixbufClass *class);
static void gtk_cell_renderer_text_pixbuf_get_size   (GtkCellRenderer                *cell,
						      GtkWidget                      *view,
						      GdkRectangle                   *cell_area,
						      gint                           *x_offset,
						      gint                           *y_offset,
						      gint                           *width,
						      gint                           *height);
static void gtk_cell_renderer_text_pixbuf_render     (GtkCellRenderer                *cell,
						      GdkWindow                      *window,
						      GtkWidget                      *view,
						      GdkRectangle                   *background_area,
						      GdkRectangle                   *cell_area,
						      GdkRectangle                   *expose_area,
						      guint                           flags);


static GtkCellRendererTextClass *parent_class = NULL;


GtkType
gtk_cell_renderer_text_pixbuf_get_type (void)
{
  static GtkType cell_text_pixbuf_type = 0;

  if (!cell_text_pixbuf_type)
    {
      static const GTypeInfo cell_text_pixbuf_info =
      {
        sizeof (GtkCellRendererTextPixbufClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_text_pixbuf_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRendererTextPixbuf),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_cell_renderer_text_pixbuf_init,
      };

      cell_text_pixbuf_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "GtkCellRendererTextPixbuf", &cell_text_pixbuf_info, 0);
    }

  return cell_text_pixbuf_type;
}

static void
gtk_cell_renderer_text_pixbuf_init (GtkCellRendererTextPixbuf *celltextpixbuf)
{
  celltextpixbuf->pixbuf = GTK_CELL_RENDERER_PIXBUF (gtk_cell_renderer_pixbuf_new ());
  celltextpixbuf->pixbuf_pos = GTK_POS_LEFT;
}

static void
gtk_cell_renderer_text_pixbuf_class_init (GtkCellRendererTextPixbufClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  object_class->get_property = gtk_cell_renderer_text_pixbuf_get_property;
  object_class->set_property = gtk_cell_renderer_text_pixbuf_set_property;

  cell_class->get_size = gtk_cell_renderer_text_pixbuf_get_size;
  cell_class->render = gtk_cell_renderer_text_pixbuf_render;
  
  g_object_class_install_property (object_class,
				   PROP_PIXBUF_POS,
				   g_param_spec_int ("pixbufpos",
						     _("Pixbuf location"),
						     _("The relative location of the pixbuf to the text."),
						     GTK_POS_LEFT,
						     GTK_POS_BOTTOM,
						     GTK_POS_LEFT,
						     G_PARAM_READABLE |
						     G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_PIXBUF,
				   g_param_spec_object ("pixbuf",
							_("Pixbuf Object"),
							_("The pixbuf to render."),
							GDK_TYPE_PIXBUF,
							G_PARAM_READABLE |
							G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_PIXBUF_XALIGN,
				   g_param_spec_float ("pixbuf_xalign",
						       _("pixbuf xalign"),
						       _("The x-align of the pixbuf."),
						       0.0,
						       1.0,
						       0.0,
						       G_PARAM_READABLE |
						       G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_PIXBUF_YALIGN,
				   g_param_spec_float ("pixbuf_yalign",
						       _("pixbuf yalign"),
						       _("The y-align of the pixbuf."),
						       0.0,
						       1.0,
						       0.5,
						       G_PARAM_READABLE |
						       G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_PIXBUF_XPAD,
				   g_param_spec_uint ("pixbuf_xpad",
						      _("pixbuf xpad"),
						      _("The xpad of the pixbuf."),
						      0,
						      100,
						      2,
						      G_PARAM_READABLE |
						      G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_PIXBUF_YPAD,
				   g_param_spec_uint ("pixbuf_ypad",
						      _("pixbuf ypad"),
						      _("The ypad of the pixbuf."),
						      0,
						      100,
						      2,
						      G_PARAM_READABLE |
						      G_PARAM_WRITABLE));
}

static void
gtk_cell_renderer_text_pixbuf_get_property (GObject     *object,
					    guint        param_id,
					    GValue      *value,
					    GParamSpec  *pspec)
{
  GtkCellRendererTextPixbuf *celltextpixbuf = GTK_CELL_RENDERER_TEXT_PIXBUF (object);
  
  switch (param_id)
    {
    case PROP_PIXBUF_POS:
      g_value_set_int (value, celltextpixbuf->pixbuf_pos);
      break;
    case PROP_PIXBUF:
      g_object_get_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "pixbuf",
			     value);
      break;
    case PROP_PIXBUF_XALIGN:
      g_object_get_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "xalign",
			     value);
      break;
    case PROP_PIXBUF_YALIGN:
      g_object_get_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "yalign",
			     value);
      break;
    case PROP_PIXBUF_XPAD:
      g_object_get_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "xpad",
			     value);
      break;
    case PROP_PIXBUF_YPAD:
      g_object_get_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "ypad",
			     value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_text_pixbuf_set_property (GObject      *object,
					    guint         param_id,
					    const GValue *value,
					    GParamSpec   *pspec)
{
  GtkCellRendererTextPixbuf *celltextpixbuf = GTK_CELL_RENDERER_TEXT_PIXBUF (object);
  
  switch (param_id)
    {
    case PROP_PIXBUF:
      g_object_set_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "pixbuf",
			     value);
      g_object_notify (G_OBJECT(object), "pixbuf");
      break;
    case PROP_PIXBUF_POS:
      celltextpixbuf->pixbuf_pos = g_value_get_int (value);
      g_object_notify (G_OBJECT(object), "pixbuf_pos");
      break;
    case PROP_PIXBUF_XALIGN:
      g_object_set_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "xalign",
			     value);
      g_object_notify (G_OBJECT(object), "pixbuf_xalign");
      break;
    case PROP_PIXBUF_YALIGN:
      g_object_set_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "yalign",
			     value);
      g_object_notify (G_OBJECT(object), "pixbuf_yalign");
      break;
    case PROP_PIXBUF_XPAD:
      g_object_set_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "xpad",
			     value);
      g_object_notify (G_OBJECT(object), "pixbuf_xpad");
      break;
    case PROP_PIXBUF_YPAD:
      g_object_set_property (G_OBJECT (celltextpixbuf->pixbuf),
			     "ypad",
			     value);
      g_object_notify (G_OBJECT(object), "pixbuf_ypad");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_text_pixbuf_new:
 * 
 * Creates a new #GtkCellRendererTextPixbuf. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "text" property on the cell renderer to a string value
 * in the model, thus rendering a different string in each row of the
 * #GtkTreeView
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_text_pixbuf_new (void)
{
  return GTK_CELL_RENDERER (gtk_type_new (gtk_cell_renderer_text_pixbuf_get_type ()));
}

typedef void (* CellSizeFunc) (GtkCellRenderer    *cell,
			       GtkWidget          *widget,
			       GdkRectangle       *rectangle,
			       gint               *x_offset,
			       gint               *y_offset,
			       gint               *width,
			       gint               *height);
typedef void (* CellRenderFunc) (GtkCellRenderer *cell,
				 GdkWindow       *window,
				 GtkWidget       *widget,
				 GdkRectangle    *background_area,
				 GdkRectangle    *cell_area,
				 GdkRectangle    *expose_area,
				 guint            flags);

static void
gtk_cell_renderer_text_pixbuf_get_size (GtkCellRenderer *cell,
					GtkWidget       *widget,
					GdkRectangle    *cell_area,
					gint            *x_offset,
					gint            *y_offset,
					gint            *width,
					gint            *height)
{
  GtkCellRendererTextPixbuf *celltextpixbuf = (GtkCellRendererTextPixbuf *)cell;
  gint pixbuf_width;
  gint pixbuf_height;
  gint text_width;
  gint text_height;
  gint calc_width;
  gint calc_height;
  
  (* GTK_CELL_RENDERER_CLASS (parent_class)->get_size) (cell, widget, NULL, NULL, NULL, &text_width, &text_height);
  (* GTK_CELL_RENDERER_CLASS (G_OBJECT_GET_CLASS (celltextpixbuf->pixbuf))->get_size) (GTK_CELL_RENDERER (celltextpixbuf->pixbuf),
										       widget,
										       NULL, NULL, NULL,
										       &pixbuf_width,
										       &pixbuf_height);
  if (celltextpixbuf->pixbuf_pos == GTK_POS_LEFT ||
      celltextpixbuf->pixbuf_pos == GTK_POS_RIGHT)
    {
      calc_width = pixbuf_width + text_width;
      calc_height = MAX (pixbuf_height, text_height);
    }
  else
    {
      calc_width = MAX (pixbuf_width, text_width);
      calc_height = pixbuf_height + text_height;
    }

  if (width)
    *width = calc_width;
  if (height)
    *height = calc_height;

  if (cell_area)
    {
      if (x_offset)
	{
	  *x_offset = cell->xalign * (cell_area->width - calc_width - (2 * cell->xpad));
	  *x_offset = MAX (*x_offset, 0) + cell->xpad;
	}
      if (y_offset)
	{
	  *y_offset = cell->yalign * (cell_area->height - calc_height - (2 * cell->ypad));
	  *y_offset = MAX (*y_offset, 0) + cell->ypad;
	}
    }
}

static void
gtk_cell_renderer_text_pixbuf_render (GtkCellRenderer *cell,
				      GdkWindow       *window,
				      GtkWidget       *widget,
				      GdkRectangle    *background_area,
				      GdkRectangle    *cell_area,
				      GdkRectangle    *expose_area,
				      guint            flags)

{
  GtkCellRendererTextPixbuf *celltextpixbuf = (GtkCellRendererTextPixbuf *) cell;
  CellSizeFunc size_func1, size_func2;
  CellRenderFunc render_func1, render_func2;
  GtkCellRenderer *cell1, *cell2;
  gint tmp_width;
  gint tmp_height;
  GdkRectangle real_cell_area;

  if (celltextpixbuf->pixbuf_pos == GTK_POS_LEFT ||
      celltextpixbuf->pixbuf_pos == GTK_POS_TOP)
    {
      size_func1 = GTK_CELL_RENDERER_CLASS (G_OBJECT_GET_CLASS (celltextpixbuf->pixbuf))->get_size;
      render_func1 = GTK_CELL_RENDERER_CLASS (G_OBJECT_GET_CLASS (celltextpixbuf->pixbuf))->render;
      cell1 = GTK_CELL_RENDERER (celltextpixbuf->pixbuf);

      size_func2 = GTK_CELL_RENDERER_CLASS (parent_class)->get_size;
      render_func2 = GTK_CELL_RENDERER_CLASS (parent_class)->render;
      cell2 = cell;
    }
  else
    {
      size_func1 = GTK_CELL_RENDERER_CLASS (parent_class)->get_size;
      render_func1 = GTK_CELL_RENDERER_CLASS (parent_class)->render;
      cell1 = cell;

      size_func2 = GTK_CELL_RENDERER_CLASS (G_OBJECT_GET_CLASS (celltextpixbuf->pixbuf))->get_size;
      render_func2 = GTK_CELL_RENDERER_CLASS (G_OBJECT_GET_CLASS (celltextpixbuf->pixbuf))->render;
      cell2 = GTK_CELL_RENDERER (celltextpixbuf->pixbuf);
    }

  (size_func1) (cell1, widget, NULL, NULL, NULL, &tmp_width, &tmp_height);

  real_cell_area.x = cell_area->x;
  real_cell_area.y = cell_area->y;

  if (celltextpixbuf->pixbuf_pos == GTK_POS_LEFT ||
      celltextpixbuf->pixbuf_pos == GTK_POS_RIGHT)
    {
      real_cell_area.width = MIN (tmp_width, cell_area->width);
      real_cell_area.height = cell_area->height;
    }
  else
    {
      real_cell_area.height = MIN (tmp_height, cell_area->height);
      real_cell_area.width = cell_area->width;
    }

  (render_func1) (cell1,
		  window,
		  widget,
		  background_area,
		  &real_cell_area,
		  expose_area,
		  flags);

  if (celltextpixbuf->pixbuf_pos == GTK_POS_LEFT ||
      celltextpixbuf->pixbuf_pos == GTK_POS_RIGHT)
    {
      real_cell_area.x = real_cell_area.x + real_cell_area.width;
      real_cell_area.width = cell_area->width - real_cell_area.width;
    }
  else
    {
      real_cell_area.y = real_cell_area.y + real_cell_area.height;
      real_cell_area.height = cell_area->height - real_cell_area.height;
    }

  (render_func2 ) (cell2,
		   window,
		   widget,
		   background_area,
		   &real_cell_area,
		   expose_area,
		   flags);
}
