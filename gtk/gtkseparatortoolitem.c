/* gtkseparatortoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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

#undef GTK_DISABLE_DEPRECATED

#include "gtkseparatormenuitem.h"
#include "gtkseparatortoolitem.h"
#include "gtkintl.h"
#include "gtktoolbar.h"

/* note: keep in sync with DEFAULT_SPACE_SIZE and DEFAULT_SPACE_STYLE in gtktoolbar.c */
#define DEFAULT_SPACE_SIZE 4
#define DEFAULT_SPACE_STYLE GTK_TOOLBAR_SPACE_LINE

#define SPACE_LINE_DIVISION 10
#define SPACE_LINE_START    3
#define SPACE_LINE_END      7

#define MENU_ID "gtk-separator-tool-item-menu-id"

enum {
  PROP_0,
  PROP_DRAW
};

static void     gtk_separator_tool_item_class_init        (GtkSeparatorToolItemClass *class);
static void	gtk_separator_tool_item_init		  (GtkSeparatorToolItem      *separator_item,
							   GtkSeparatorToolItemClass *class);
static gboolean gtk_separator_tool_item_create_menu_proxy (GtkToolItem               *item);
static void     gtk_separator_tool_item_set_property      (GObject                   *object,
							   guint                      prop_id,
							   const GValue              *value,
							   GParamSpec                *pspec);
static void     gtk_separator_tool_item_get_property       (GObject                   *object,
							   guint                      prop_id,
							   GValue                    *value,
							   GParamSpec                *pspec);
static void     gtk_separator_tool_item_size_request      (GtkWidget                 *widget,
							   GtkRequisition            *requisition);
static gboolean gtk_separator_tool_item_expose            (GtkWidget                 *widget,
							   GdkEventExpose            *event);
static void     gtk_separator_tool_item_add               (GtkContainer              *container,
							   GtkWidget                 *child);
static GtkToolbarSpaceStyle get_space_style               (GtkToolItem               *tool_item);
static gint                 get_space_size                (GtkToolItem               *tool_item);



static GObjectClass *parent_class = NULL;

#define GTK_SEPARATOR_TOOL_ITEM_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_SEPARATOR_TOOL_ITEM, GtkSeparatorToolItemPrivate))

struct _GtkSeparatorToolItemPrivate
{
  guint draw : 1;
};

GType
gtk_separator_tool_item_get_type (void)
{
  static GType type = 0;
  
  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (GtkSeparatorToolItemClass),
	  (GBaseInitFunc) 0,
	  (GBaseFinalizeFunc) 0,
	  (GClassInitFunc) gtk_separator_tool_item_class_init,
	  (GClassFinalizeFunc) 0,
	  NULL,
	  sizeof (GtkSeparatorToolItem),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) gtk_separator_tool_item_init,
	};
      
      type = g_type_register_static (GTK_TYPE_TOOL_ITEM,
				     "GtkSeparatorToolItem", &type_info, 0);
    }
  return type;
}

static GtkToolbarSpaceStyle
get_space_style (GtkToolItem *tool_item)
{
  GtkToolbarSpaceStyle space_style = DEFAULT_SPACE_STYLE;
  GtkWidget *parent = GTK_WIDGET (tool_item)->parent;
  
  if (GTK_IS_TOOLBAR (parent))
    {
      gtk_widget_style_get (parent,
			    "space_style", &space_style,
			    NULL);
    }
  
  return space_style;  
}

static gint
get_space_size (GtkToolItem *tool_item)
{
  gint space_size = DEFAULT_SPACE_SIZE;
  GtkWidget *parent = GTK_WIDGET (tool_item)->parent;
  
  if (GTK_IS_TOOLBAR (parent))
    {
      gtk_widget_style_get (parent,
			    "space_size", &space_size,
			    NULL);
    }
  
  return space_size;
}

static void
gtk_separator_tool_item_class_init (GtkSeparatorToolItemClass *class)
{
  GObjectClass *object_class;
  GtkContainerClass *container_class;
  GtkToolItemClass *toolitem_class;
  GtkWidgetClass *widget_class;
  
  parent_class = g_type_class_peek_parent (class);
  object_class = (GObjectClass *)class;
  container_class = (GtkContainerClass *)class;
  toolitem_class = (GtkToolItemClass *)class;
  widget_class = (GtkWidgetClass *)class;

  object_class->set_property = gtk_separator_tool_item_set_property;
  object_class->get_property = gtk_separator_tool_item_get_property;
  widget_class->size_request = gtk_separator_tool_item_size_request;
  widget_class->expose_event = gtk_separator_tool_item_expose;
  toolitem_class->create_menu_proxy = gtk_separator_tool_item_create_menu_proxy;
  
  container_class->add = gtk_separator_tool_item_add;
  
  g_object_class_install_property (object_class,
				   PROP_DRAW,
				   g_param_spec_boolean ("draw",
							 _("Draw"),
							 _("Whether the separator is drawn, or just blank"),
							 TRUE,
							 G_PARAM_READWRITE));
  
  g_type_class_add_private (object_class, sizeof (GtkSeparatorToolItemPrivate));
}

static void
gtk_separator_tool_item_init (GtkSeparatorToolItem      *separator_item,
			      GtkSeparatorToolItemClass *class)
{
  separator_item->priv = GTK_SEPARATOR_TOOL_ITEM_GET_PRIVATE (separator_item);
  separator_item->priv->draw = TRUE;
}

static void
gtk_separator_tool_item_add (GtkContainer *container,
			     GtkWidget    *child)
{
  g_warning("attempt to add a child to an GtkSeparatorToolItem");
}

static gboolean
gtk_separator_tool_item_create_menu_proxy (GtkToolItem *item)
{
  GtkWidget *menu_item = NULL;
  
  menu_item = gtk_separator_menu_item_new();
  
  gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
  
  return TRUE;
}

static void
gtk_separator_tool_item_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      gtk_separator_tool_item_set_draw (item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      g_value_set_boolean (value, gtk_separator_tool_item_get_draw (item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_size_request (GtkWidget      *widget,
				      GtkRequisition *requisition)
{
  GtkToolItem *item = GTK_TOOL_ITEM (widget);
  GtkOrientation orientation = gtk_tool_item_get_orientation (item);
  
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width = get_space_size (item);
      requisition->height = 1;
    }
  else
    {
      requisition->height = get_space_size (item);
      requisition->width = 1;
    }
}

static gboolean
gtk_separator_tool_item_expose (GtkWidget      *widget,
				GdkEventExpose *event)
{
  GtkToolItem *tool_item = GTK_TOOL_ITEM (widget);
  GtkSeparatorToolItem *separator_tool_item = GTK_SEPARATOR_TOOL_ITEM (widget);
  gint space_size;
  GtkAllocation *allocation;
  GtkOrientation orientation;
  GdkRectangle *area;

  if (separator_tool_item->priv->draw &&
      get_space_style (tool_item) == GTK_TOOLBAR_SPACE_LINE)
    {
      space_size = get_space_size (tool_item);
      allocation = &(widget->allocation);
      orientation = gtk_tool_item_get_orientation (tool_item);
      area = &(event->area);
      
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  gtk_paint_vline (widget->style, widget->window,
			   GTK_WIDGET_STATE (widget), area, widget,
			   "separator_tool_item",
			   allocation->y + allocation->height *
			   SPACE_LINE_START / SPACE_LINE_DIVISION,
			   allocation->y + allocation->height *
			   SPACE_LINE_END / SPACE_LINE_DIVISION,
			   allocation->x + (space_size - widget->style->xthickness) / 2);
	}
      else if (orientation == GTK_ORIENTATION_VERTICAL)
	{
	  gtk_paint_hline (widget->style, widget->window,
			   GTK_WIDGET_STATE (widget), area, widget,
			   "separator_tool_item",
			   allocation->x + allocation->width *
			   SPACE_LINE_START / SPACE_LINE_DIVISION,
			   allocation->x + allocation->width *
			   SPACE_LINE_END / SPACE_LINE_DIVISION,
			   allocation->y + (space_size - widget->style->ythickness) / 2);
	}
    }
  
  return FALSE;
}

/**
 * gtk_separator_tool_item_new:
 * 
 * Create a new #GtkSeparatorToolItem
 * 
 * Return value: the new #GtkSeparatorToolItem
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_separator_tool_item_new (void)
{
  GtkToolItem *self;
  
  self = g_object_new (GTK_TYPE_SEPARATOR_TOOL_ITEM,
		       NULL);
  
  return self;
}

/**
 * gtk_separator_tool_item_get_draw:
 * @separator_tool_item: a #GtkSeparatorToolItem 
 * 
 * Returns whether @separator_tool_item is drawn as a
 * line, or just blank. See gtk_separator_tool_item_set_draw().
 * 
 * Return value: #TRUE if @separator_tool_item is drawn as a line, or just blank.
 * 
 * Since: 2.4
 **/
gboolean
gtk_separator_tool_item_get_draw (GtkSeparatorToolItem *separator_tool_item)
{
  g_return_val_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (separator_tool_item), FALSE);
  
  return separator_tool_item->priv->draw;
}

/**
 * gtk_separator_tool_item_set_draw:
 * @separator_tool_item: a #GtkSeparatorToolItem
 * @draw: whether @separator_tool_item is drawn as a vertical iln
 * 
 * When @separator_tool_items is drawn as a vertical line, or just blank.
 * Setting this #FALSE along with gtk_tool_item_set_expand() is useful
 * to create an item that forces following items to the end of the toolbar.
 * 
 * Since: 2.4
 **/
void
gtk_separator_tool_item_set_draw (GtkSeparatorToolItem *separator_tool_item,
				  gboolean              draw)
{
  g_return_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (separator_tool_item));

  draw = draw != FALSE;

  if (draw != separator_tool_item->priv->draw)
    {
      separator_tool_item->priv->draw = draw;

      gtk_widget_queue_draw (GTK_WIDGET (separator_tool_item));

      g_object_notify (G_OBJECT (separator_tool_item), "draw");
    }
}

