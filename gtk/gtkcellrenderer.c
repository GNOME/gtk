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
  PROP_CAN_ACTIVATE,
  PROP_CAN_EDIT,
  PROP_VISIBLE,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_IS_EXPANDER,
  PROP_IS_EXPANDED,
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
  cell->can_activate = FALSE;
  cell->can_edit = FALSE;
  cell->visible = TRUE;
  cell->width = -1;
  cell->height = -1;
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
				   PROP_CAN_ACTIVATE,
				   g_param_spec_boolean ("can_activate",
							 _("can_activate"),
							 _("Cell can get activate events."),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_CAN_EDIT,
				   g_param_spec_boolean ("can_edit",
							 _("can_edit"),
							 _("Cell supports CellEditable interface."),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
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

  g_object_class_install_property (object_class,
				   PROP_WIDTH,
				   g_param_spec_int ("width",
						     _("width"),
						     _("The fixed width."),
						     -1,
						     100,
						     -1,
						     G_PARAM_READABLE |
						     G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_HEIGHT,
				   g_param_spec_int ("height",
						     _("height"),
						     _("The fixed height."),
						     -1,
						     100,
						     -1,
						     G_PARAM_READABLE |
						     G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_IS_EXPANDER,
				   g_param_spec_boolean ("is_expander",
							 _("Is Expander"),
							 _("Row has children."),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));


  g_object_class_install_property (object_class,
				   PROP_IS_EXPANDED,
				   g_param_spec_boolean ("is_expanded",
							 _("Is Expanded"),
							 _("Row is an expander row, and is expanded"),
							 FALSE,
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
    case PROP_CAN_ACTIVATE:
      g_value_set_boolean (value, cell->can_activate);
      break;
    case PROP_CAN_EDIT:
      g_value_set_boolean (value, cell->can_edit);
      break;
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
      g_value_set_uint (value, cell->xpad);
      break;
    case PROP_YPAD:
      g_value_set_uint (value, cell->ypad);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, cell->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, cell->height);
      break;
    case PROP_IS_EXPANDER:
      g_value_set_int (value, cell->is_expander);
      break;
    case PROP_IS_EXPANDED:
      g_value_set_int (value, cell->is_expanded);
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
    case PROP_CAN_ACTIVATE:
      cell->can_activate = g_value_get_boolean (value);
      g_object_notify (object, "can_activate");
      /* can_activate and can_edit are mutually exclusive */
      if (cell->can_activate && cell->can_edit)
	{
	  cell->can_edit = FALSE;
	  g_object_notify (object, "can_edit");
	}
      break;
    case PROP_CAN_EDIT:
      cell->can_edit = g_value_get_boolean (value);
      g_object_notify (object, "can_edit");
      /* can_activate and can_edit are mutually exclusive */
      if (cell->can_activate && cell->can_edit)
	{
	  cell->can_activate = FALSE;
	  g_object_notify (object, "can_activate");
	}
      break;
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
      cell->xpad = g_value_get_uint (value);
      g_object_notify (object, "xpad");
      break;
    case PROP_YPAD:
      cell->ypad = g_value_get_uint (value);
      g_object_notify (object, "ypad");
      break;
    case PROP_WIDTH:
      cell->width = g_value_get_int (value);
      g_object_notify (object, "width");
      break;
    case PROP_HEIGHT:
      cell->height = g_value_get_int (value);
      g_object_notify (object, "height");
      break;
    case PROP_IS_EXPANDER:
      cell->is_expander = g_value_get_boolean (value);
      g_object_notify (object, "is_expander");
      break;
    case PROP_IS_EXPANDED:
      cell->is_expanded = g_value_get_boolean (value);
      g_object_notify (object, "is_expanded");
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
 * @cell_area: The area a cell will be allocated, or %NULL
 * @x_offset: location to return x offset of cell relative to @cell_area, or %NULL
 * @y_offset: location to return y offset of cell relative to @cell_area, or %NULL
 * @width: location to return width needed to render a cell, or %NULL
 * @height: location to return height needed to render a cell, or %NULL
 *
 * Obtains the width and height needed to render the cell. Used by view widgets
 * to determine the appropriate size for the cell_area passed to
 * gtk_cell_renderer_render().  If @cell_area is not %NULL, fills in the x and y
 * offsets (if set) of the cell relative to this location.
 **/
void
gtk_cell_renderer_get_size (GtkCellRenderer *cell,
			    GtkWidget       *widget,
			    GdkRectangle    *cell_area,
			    gint            *x_offset,
			    gint            *y_offset,
			    gint            *width,
			    gint            *height)
{
  gint *real_width = NULL;
  gint *real_height = NULL;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->get_size != NULL);

  if (width)
    {
      if (cell->width == -1)
	real_width = width;
      else
	*width = cell->width;
    }
  if (height)
    {
      if (cell->height == -1)
	real_height = height;
      else
	*height = cell->height;
    }

  if (real_width || real_height)
    GTK_CELL_RENDERER_GET_CLASS (cell)->get_size (cell, widget, cell_area, x_offset, y_offset, real_width, real_height);
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
 * gtk_cell_renderer_activate:
 * @cell: a #GtkCellRenderer
 * @event: a #GdkEvent
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location; e.g. for #GtkTreeView, a string representation of #GtkTreePath
 * @background_area: background area as passed to gtk_cell_renderer_render()
 * @cell_area: cell area as passed to gtk_cell_renderer_render()
 * @flags: render flags
 *
 * Passes an activate event to the cell renderer for possible processing.  Some
 * cell renderers may use events; for example, #GtkCellRendererToggle toggles
 * when it gets a mouse click.
 *
 * Return value: %TRUE if the event was consumed/handled
 **/
gboolean
gtk_cell_renderer_activate (GtkCellRenderer      *cell,
			    GdkEvent             *event,
			    GtkWidget            *widget,
			    gchar                *path,
			    GdkRectangle         *background_area,
			    GdkRectangle         *cell_area,
			    GtkCellRendererState  flags)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  if (! cell->can_activate)
    return FALSE;

  if (GTK_CELL_RENDERER_GET_CLASS (cell)->activate == NULL)
    return FALSE;

  return GTK_CELL_RENDERER_GET_CLASS (cell)->activate (cell,
						       event,
						       widget,
						       path,
						       background_area,
						       cell_area,
						       flags);
}


/**
 * gtk_cell_renderer_start_editing:
 * @cell: a #GtkCellRenderer
 * @event: a #GdkEvent
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location; e.g. for #GtkTreeView, a string representation of #GtkTreePath
 * @background_area: background area as passed to gtk_cell_renderer_render()
 * @cell_area: cell area as passed to gtk_cell_renderer_render()
 * @flags: render flags
 * 
 * Passes an activate event to the cell renderer for possible processing.
 * 
 * Return value: A new #GtkCellEditable, or %NULL
 **/
GtkCellEditable *
gtk_cell_renderer_start_editing (GtkCellRenderer      *cell,
				 GdkEvent             *event,
				 GtkWidget            *widget,
				 gchar                *path,
				 GdkRectangle         *background_area,
				 GdkRectangle         *cell_area,
				 GtkCellRendererState  flags)

{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), NULL);

  if (! cell->can_edit)
    return NULL;

  if (GTK_CELL_RENDERER_GET_CLASS (cell)->start_editing == NULL)
    return NULL;

  
  return GTK_CELL_RENDERER_GET_CLASS (cell)->start_editing (cell,
							    event,
							    widget,
							    path,
							    background_area,
							    cell_area,
							    flags);
}

/**
 * gtk_cell_renderer_set_fixed_size:
 * @cell: A #GtkCellRenderer
 * @width: the width of the cell renderer, or -1
 * @height: the height of the cell renderer, or -1
 * 
 * Sets the renderer size to be explicit, independent of the properties set.
 **/
void
gtk_cell_renderer_set_fixed_size (GtkCellRenderer *cell,
				  gint             width,
				  gint             height)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (width >= -1 && height >= -1);

  if ((width != cell->width) || (height != cell->height))
    {
      g_object_freeze_notify (G_OBJECT (cell));

      if (width != cell->width)
        {
          cell->width = width;
          g_object_notify (G_OBJECT (cell), "width");
        }

      if (height != cell->height)
        {
          cell->height = height;
          g_object_notify (G_OBJECT (cell), "height");
        }

      g_object_thaw_notify (G_OBJECT (cell));
    }
}

/**
 * gtk_cell_renderer_get_fixed_size:
 * @cell: A #GtkCellRenderer
 * @width: location to fill in with the fixed width of the widget, or %NULL
 * @height: location to fill in with the fixed height of the widget, or %NULL
 * 
 * Fills in @width and @height with the appropriate size of @cell.
 **/
void
gtk_cell_renderer_get_fixed_size (GtkCellRenderer *cell,
				  gint            *width,
				  gint            *height)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  if (width)
    (* width) = cell->width;
  if (height)
    (* height) = cell->height;
}
