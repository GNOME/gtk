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
#include "gtkintl.h"

static void gtk_cell_renderer_init       (GtkCellRenderer      *cell);
static void gtk_cell_renderer_class_init (GtkCellRendererClass *class);
static void gtk_cell_renderer_get_property  (GObject              *object,
					     guint                 param_id,
					     GValue               *value,
					     GParamSpec           *pspec);
static void gtk_cell_renderer_set_property  (GObject              *object,
					     guint                 param_id,
					     const GValue         *value,
					     GParamSpec           *pspec);


enum {
  PROP_ZERO,
  PROP_VISIBLE,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD,
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

      cell_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkCellRenderer", &cell_info, 0);
    }

  return cell_type;
}

static void
gtk_cell_renderer_init (GtkCellRenderer *cell)
{
  /* FIXME remove on port to GtkObject */
  gtk_object_ref (GTK_OBJECT (cell));
  gtk_object_sink (GTK_OBJECT (cell));

  cell->visible = TRUE;
  cell->xalign = 0.5;
  cell->yalign = 0.5;
  cell->xpad = 0;
  cell->ypad = 0;
}

static void
gtk_cell_renderer_class_init (GtkCellRendererClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_cell_renderer_get_property;
  object_class->set_property = gtk_cell_renderer_set_property;

  class->render = NULL;
  class->get_size = NULL;

  g_object_class_install_property (object_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
							 _("visible"),
							 _("Display the cell"),
							 TRUE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_XALIGN,
				   g_param_spec_float ("xalign",
						       _("xalign"),
						       _("The x-align."),
						       0.0,
						       1.0,
						       0.0,
						       G_PARAM_READABLE |
						       G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_YALIGN,
				   g_param_spec_float ("yalign",
						       _("yalign"),
						       _("The y-align."),
						       0.0,
						       1.0,
						       0.5,
						       G_PARAM_READABLE |
						       G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_XPAD,
				   g_param_spec_uint ("xpad",
						      _("xpad"),
						      _("The xpad."),
						      0,
						      100,
						      2,
						      G_PARAM_READABLE |
						      G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
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
gtk_cell_renderer_get_property (GObject     *object,
				guint        param_id,
				GValue      *value,
				GParamSpec  *pspec)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);

  switch (param_id)
    {
    case PROP_VISIBLE:
      g_value_set_boolean (value, cell->visible);
      break;
    case PROP_XALIGN:
      g_value_set_float (value, cell->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, cell->yalign);
      break;
    case PROP_XPAD:
      g_value_set_float (value, cell->xpad);
      break;
    case PROP_YPAD:
      g_value_set_float (value, cell->ypad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

}

static void
gtk_cell_renderer_set_property (GObject      *object,
				guint         param_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);

  switch (param_id)
    {
    case PROP_VISIBLE:
      cell->visible = g_value_get_boolean (value);
      g_object_notify (object, "visible");
      break;
    case PROP_XALIGN:
      cell->xalign = g_value_get_float (value);
      g_object_notify (object, "xalign");
      break;
    case PROP_YALIGN:
      cell->yalign = g_value_get_float (value);
      g_object_notify (object, "yalign");
      break;
    case PROP_XPAD:
      cell->xpad = g_value_get_int (value);
      g_object_notify (object, "xpad");
      break;
    case PROP_YPAD:
      cell->ypad = g_value_get_int (value);
      g_object_notify (object, "ypad");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_get_size:
 * @cell: a #GtkCellRenderer
 * @widget: the widget the renderer is rendering to
 * @width: location to return width needed to render a cell, or %NULL
 * @height: location to return height needed to render a cell, or %NULL
 * 
 * Obtains the width and height needed to render the cell. Used by
 * view widgets to determine the appropriate size for the cell_area
 * passed to gtk_cell_renderer_render().
 **/
void
gtk_cell_renderer_get_size (GtkCellRenderer *cell,
			    GtkWidget *widget,
			    gint      *width,
			    gint      *height)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->get_size != NULL);

  GTK_CELL_RENDERER_GET_CLASS (cell)->get_size (cell, widget, width, height);
}

/**
 * gtk_cell_renderer_render:
 * @cell: a #GtkCellRenderer
 * @window: a #GdkDrawable to draw to
 * @widget: the widget owning @window
 * @background_area: entire cell area (including tree expanders and maybe padding on the sides)
 * @cell_area: area normally rendered by a cell renderer
 * @expose_area: area that actually needs updating
 * @flags: flags that affect rendering
 *
 * Invokes the virtual render function of the #GtkCellRenderer. The
 * three passed-in rectangles are areas of @window. Most renderers
 * will draw to @cell_area; the xalign, yalign, xpad, and ypad fields
 * of the #GtkCellRenderer should be honored with respect to
 * @cell_area. @background_area includes the blank space around the
 * cell, and also the area containing the tree expander; so the
 * @background_area rectangles for all cells tile to cover the entire
 * @window. Cell renderers can use the @background_area to draw custom expanders, for
 * example. @expose_area is a clip rectangle.
 * 
 **/
void
gtk_cell_renderer_render (GtkCellRenderer     *cell,
			  GdkWindow           *window,
			  GtkWidget           *widget,
			  GdkRectangle        *background_area,
			  GdkRectangle        *cell_area,
			  GdkRectangle        *expose_area,
			  GtkCellRendererState flags)
{
  /* It's actually okay to pass in a NULL cell, as we run into that
   * a lot
   */
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

/**
 * gtk_cell_renderer_event:
 * @cell: a #GtkCellRenderer
 * @event: a #GdkEvent
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location; e.g. for #GtkTreeView, a string representation of #GtkTreePath
 * @background_area: background area as passed to gtk_cell_renderer_render()
 * @cell_area: cell area as passed to gtk_cell_renderer_render()
 * @flags: render flags
 * 
 * Passes an event to the cell renderer for possible processing.  Some
 * cell renderers may use events; for example, #GtkCellRendererToggle
 * toggles when it gets a mouse click.
 * 
 * Return value: %TRUE if the event was consumed/handled
 **/
gint
gtk_cell_renderer_event (GtkCellRenderer     *cell,
			 GdkEvent            *event,
			 GtkWidget           *widget,
			 gchar               *path,
			 GdkRectangle        *background_area,
			 GdkRectangle        *cell_area,
			 GtkCellRendererState flags)
{
  /* It's actually okay to pass in a NULL cell, as we run into that
   * a lot
   */
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

