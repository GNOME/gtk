/* GTK - The GIMP Toolkit
 * gtkcellrendererseptext.h: Cell renderer for text or a separator
 * Copyright (C) 2003, Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#include "gtkcellrendererseptext.h"

static void gtk_cell_renderer_sep_text_render (GtkCellRenderer      *cell,
					       GdkWindow            *window,
					       GtkWidget            *widget,
					       GdkRectangle         *background_area,
					       GdkRectangle         *cell_area,
					       GdkRectangle         *expose_area,
					       GtkCellRendererState  flags);

static GtkCellRendererTextClass *parent_class;

static void
gtk_cell_renderer_sep_text_class_init (GtkCellRendererSepTextClass *class)
{
  GtkCellRendererClass *cell_renderer_class;

  cell_renderer_class = GTK_CELL_RENDERER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  cell_renderer_class->render = gtk_cell_renderer_sep_text_render;
}

GType
_gtk_cell_renderer_sep_text_get_type (void)
{
  static GType cell_type = 0;

  if (!cell_type)
    {
      static const GTypeInfo cell_info =
      {
        sizeof (GtkCellRendererSepTextClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_sep_text_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRendererSepText),
	0,		/* n_preallocs */
        NULL,		/* instance_init */
	NULL,		/* value_table */
      };

      cell_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "GtkCellRendererSepText",
					  &cell_info, 0);
    }

  return cell_type;
}

static void
gtk_cell_renderer_sep_text_render (GtkCellRenderer      *cell,
				   GdkWindow            *window,
				   GtkWidget            *widget,
				   GdkRectangle         *background_area,
				   GdkRectangle         *cell_area,
				   GdkRectangle         *expose_area,
				   GtkCellRendererState  flags)
{
  GtkCellRendererSepText *st;
  const char *text;

  st = GTK_CELL_RENDERER_SEP_TEXT (cell);

  text = st->renderer_text.text;

  if (!text || !text[0])
    gtk_paint_hline (gtk_widget_get_style (widget),
		     window,
		     GTK_WIDGET_STATE (widget),
		     expose_area,
		     widget,
		     NULL,
		     cell_area->x,
		     cell_area->x + cell_area->width,
		     cell_area->y + cell_area->height / 2);
  else
    GTK_CELL_RENDERER_CLASS (parent_class)->render (cell, window, widget, background_area, cell_area, expose_area, flags);
}

GtkCellRenderer *
_gtk_cell_renderer_sep_text_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_SEP_TEXT, NULL);
}
