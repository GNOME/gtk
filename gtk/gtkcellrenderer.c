/* gtkcellrenderer.c
 * Copyright (C) 2000  Red Hat, Inc. Jonathan Blandford
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

#include "gtkcellrenderer.h"

#ifndef _
#define _(x) x
#endif

static void gtk_cell_renderer_init       (GtkCellRenderer      *cell);
static void gtk_cell_renderer_class_init (GtkCellRendererClass *class);
static void gtk_cell_renderer_get_param  (GObject              *object,
					  guint                 param_id,
					  GValue               *value,
					  GParamSpec           *pspec,
					  const gchar          *trailer);
static void gtk_cell_renderer_set_param  (GObject              *object,
					  guint                 param_id,
					  GValue               *value,
					  GParamSpec           *pspec,
					  const gchar          *trailer);


enum {
  PROP_ZERO,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD
};


GtkType
gtk_cell_renderer_get_type (void)
{
  static GtkType cell_type = 0;

  if (!cell_type)
    {
      static const GTypeInfo cell_info =
      {
        sizeof (GtkCellRendererClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRenderer),
	0,
        (GInstanceInitFunc) gtk_cell_renderer_init,
      };

      cell_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkCellRenderer", &cell_info);
    }

  return cell_type;
}

static void
gtk_cell_renderer_init (GtkCellRenderer *cell)
{
  cell->xpad = 0;
  cell->ypad = 0;
  cell->xalign = 0.5;
  cell->yalign = 0.5;
}

static void
gtk_cell_renderer_class_init (GtkCellRendererClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_param = gtk_cell_renderer_get_param;
  object_class->set_param = gtk_cell_renderer_set_param;

  class->render = NULL;
  class->get_size = NULL;


  g_object_class_install_param (object_class,
				PROP_XALIGN,
				g_param_spec_float ("xalign",
						    _("xalign"),
						    _("The x-align."),
						    0.0,
						    1.0,
						    0.0,
						    G_PARAM_READABLE |
						    G_PARAM_WRITABLE));

  g_object_class_install_param (object_class,
				PROP_YALIGN,
				g_param_spec_float ("yalign",
						    _("yalign"),
						    _("The y-align."),
						    0.0,
						    1.0,
						    0.5,
						    G_PARAM_READABLE |
						    G_PARAM_WRITABLE));

  g_object_class_install_param (object_class,
				PROP_XPAD,
				g_param_spec_uint ("xpad",
						   _("xpad"),
						   _("The xpad."),
						   0,
						   100,
						   2,
						   G_PARAM_READABLE |
						   G_PARAM_WRITABLE));

  g_object_class_install_param (object_class,
				PROP_YPAD,
				g_param_spec_uint ("ypad",
						   _("ypad"),
						   _("The ypad."),
						   0,
						   100,
						   2,
						   G_PARAM_READABLE |
						   G_PARAM_WRITABLE));
}

static void
gtk_cell_renderer_get_param (GObject     *object,
			     guint        param_id,
			     GValue      *value,
			     GParamSpec  *pspec,
			     const gchar *trailer)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);

  switch (param_id)
    {
    case PROP_XALIGN:
      g_value_init (value, G_TYPE_FLOAT);
      g_value_set_float (value, cell->xalign);
      break;
    case PROP_YALIGN:
      g_value_init (value, G_TYPE_FLOAT);
      g_value_set_float (value, cell->yalign);
      break;
    case PROP_XPAD:
      g_value_init (value, G_TYPE_INT);
      g_value_set_float (value, cell->xpad);
      break;
    case PROP_YPAD:
      g_value_init (value, G_TYPE_INT);
      g_value_set_float (value, cell->ypad);
      break;
    default:
      break;
    }

}

static void
gtk_cell_renderer_set_param (GObject     *object,
			     guint        param_id,
			     GValue      *value,
			     GParamSpec  *pspec,
			     const gchar *trailer)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);

  switch (param_id)
    {
    case PROP_XALIGN:
      cell->xalign = g_value_get_float (value);
      break;
    case PROP_YALIGN:
      cell->yalign = g_value_get_float (value);
      break;
    case PROP_XPAD:
      cell->xpad = g_value_get_int (value);
      break;
    case PROP_YPAD:
      cell->ypad = g_value_get_int (value);
      break;
    default:
      break;
    }
}

void
gtk_cell_renderer_get_size (GtkCellRenderer *cell,
			    GtkWidget *widget,
			    gint      *width,
			    gint      *height)
{
  /* It's actually okay to pass in a NULL cell, as we run into that
   * a lot */
  if (cell == NULL)
    return;
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->get_size != NULL);

  GTK_CELL_RENDERER_GET_CLASS (cell)->get_size (cell, widget, width, height);
}

void
gtk_cell_renderer_render (GtkCellRenderer *cell,
			  GdkWindow       *window,
			  GtkWidget       *widget,
			  GdkRectangle    *background_area,
			  GdkRectangle    *cell_area,
			  GdkRectangle    *expose_area,
			  guint            flags)
{
  /* It's actually okay to pass in a NULL cell, as we run into that
   * a lot */
  if (cell == NULL)
    return;
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->render != NULL);

  GTK_CELL_RENDERER_GET_CLASS (cell)->render (cell,
					      window,
					      widget,
					      background_area,
					      cell_area,
					      expose_area,
					      flags);
}

gint
gtk_cell_renderer_event (GtkCellRenderer *cell,
			 GdkEvent        *event,
			 GtkWidget       *widget,
			 gchar           *path,
			 GdkRectangle    *background_area,
			 GdkRectangle    *cell_area,
			 guint            flags)
{
  /* It's actually okay to pass in a NULL cell, as we run into that
   * a lot */
  if (cell == NULL)
    return FALSE;
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);
  if (GTK_CELL_RENDERER_GET_CLASS (cell)->event == NULL)
    return FALSE;

  return GTK_CELL_RENDERER_GET_CLASS (cell)->event (cell,
						    event,
						    widget,
						    path,
						    background_area,
						    cell_area,
						    flags);
}

