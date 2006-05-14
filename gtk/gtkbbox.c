/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gtkbbox.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum {
  PROP_0,
  PROP_LAYOUT_STYLE,
  PROP_LAST
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_SECONDARY
};

static void gtk_button_box_set_property       (GObject           *object,
					       guint              prop_id,
					       const GValue      *value,
					       GParamSpec        *pspec);
static void gtk_button_box_get_property       (GObject           *object,
					       guint              prop_id,
					       GValue            *value,
					       GParamSpec        *pspec);
static void gtk_button_box_set_child_property (GtkContainer      *container,
					       GtkWidget         *child,
					       guint              property_id,
					       const GValue      *value,
					       GParamSpec        *pspec);
static void gtk_button_box_get_child_property (GtkContainer      *container,
					       GtkWidget         *child,
					       guint              property_id,
					       GValue            *value,
					       GParamSpec        *pspec);

#define DEFAULT_CHILD_MIN_WIDTH 85
#define DEFAULT_CHILD_MIN_HEIGHT 27
#define DEFAULT_CHILD_IPAD_X 4
#define DEFAULT_CHILD_IPAD_Y 0

G_DEFINE_ABSTRACT_TYPE (GtkButtonBox, gtk_button_box, GTK_TYPE_BOX)

static void
gtk_button_box_class_init (GtkButtonBoxClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_button_box_set_property;
  gobject_class->get_property = gtk_button_box_get_property;

  container_class->set_child_property = gtk_button_box_set_child_property;
  container_class->get_child_property = gtk_button_box_get_child_property;
  
  /* FIXME we need to override the "spacing" property on GtkBox once
   * libgobject allows that.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-min-width",
							     P_("Minimum child width"),
							     P_("Minimum width of buttons inside the box"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_MIN_WIDTH,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-min-height",
							     P_("Minimum child height"),
							     P_("Minimum height of buttons inside the box"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_MIN_HEIGHT,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-internal-pad-x",
							     P_("Child internal width padding"),
							     P_("Amount to increase child's size on either side"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_X,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-internal-pad-y",
							     P_("Child internal height padding"),
							     P_("Amount to increase child's size on the top and bottom"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_Y,
							     GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_STYLE,
                                   g_param_spec_enum ("layout-style",
                                                      P_("Layout style"),
                                                      P_("How to layout the buttons in the box. Possible values are default, spread, edge, start and end"),
						      GTK_TYPE_BUTTON_BOX_STYLE,
						      GTK_BUTTONBOX_DEFAULT_STYLE,
                                                      GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_SECONDARY,
					      g_param_spec_boolean ("secondary", 
								    P_("Secondary"),
								    P_("If TRUE, the child appears in a secondary group of children, suitable for, e.g., help buttons"),
								    FALSE,
								    GTK_PARAM_READWRITE));
}

static void
gtk_button_box_init (GtkButtonBox *button_box)
{
  GTK_BOX (button_box)->spacing = 0;
  button_box->child_min_width = GTK_BUTTONBOX_DEFAULT;
  button_box->child_min_height = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_x = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_y = GTK_BUTTONBOX_DEFAULT;
  button_box->layout_style = GTK_BUTTONBOX_DEFAULT_STYLE;
}

static void
gtk_button_box_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  switch (prop_id) 
    {
    case PROP_LAYOUT_STYLE:
      gtk_button_box_set_layout (GTK_BUTTON_BOX (object),
				 g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_STYLE:
      g_value_set_enum (value, GTK_BUTTON_BOX (object)->layout_style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_set_child_property (GtkContainer    *container,
				   GtkWidget       *child,
				   guint            property_id,
				   const GValue    *value,
				   GParamSpec      *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_SECONDARY:
      gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (container), child,
					  g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_child_property (GtkContainer *container,
				   GtkWidget    *child,
				   guint         property_id,
				   GValue       *value,
				   GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_SECONDARY:
      g_value_set_boolean (value, 
			   gtk_button_box_get_child_secondary (GTK_BUTTON_BOX (container), 
							       child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

/* set per widget values for spacing, child size and child internal padding */

void 
gtk_button_box_set_child_size (GtkButtonBox *widget, 
                               gint width, gint height)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  widget->child_min_width = width;
  widget->child_min_height = height;
}

void 
gtk_button_box_set_child_ipadding (GtkButtonBox *widget,
                                   gint ipad_x, gint ipad_y)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  widget->child_ipad_x = ipad_x;
  widget->child_ipad_y = ipad_y;
}

void
gtk_button_box_set_layout (GtkButtonBox      *widget, 
                           GtkButtonBoxStyle  layout_style)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (layout_style >= GTK_BUTTONBOX_DEFAULT_STYLE &&
		    layout_style <= GTK_BUTTONBOX_END);

  if (widget->layout_style != layout_style)
    {
      widget->layout_style = layout_style;
      g_object_notify (G_OBJECT (widget), "layout-style");
      gtk_widget_queue_resize (GTK_WIDGET (widget));
    }
}


/* get per widget values for spacing, child size and child internal padding */

void 
gtk_button_box_get_child_size (GtkButtonBox *widget,
                               gint *width, gint *height)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (width != NULL);
  g_return_if_fail (height != NULL);

  *width  = widget->child_min_width;
  *height = widget->child_min_height;
}

void
gtk_button_box_get_child_ipadding (GtkButtonBox *widget,
                                   gint* ipad_x, gint *ipad_y)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (ipad_x != NULL);
  g_return_if_fail (ipad_y != NULL);

  *ipad_x = widget->child_ipad_x;
  *ipad_y = widget->child_ipad_y;
}

GtkButtonBoxStyle 
gtk_button_box_get_layout (GtkButtonBox *widget)
{
  g_return_val_if_fail (GTK_IS_BUTTON_BOX (widget), GTK_BUTTONBOX_SPREAD);
  
  return widget->layout_style;
}

/**
 * gtk_button_box_get_child_secondary:
 * @widget: a #GtkButtonBox
 * @child: a child of @widget 
 * 
 * Returns whether @child should appear in a secondary group of children.
 *
 * Return value: whether @child should appear in a secondary group of children.
 *
 * Since: 2.4
 **/
gboolean 
gtk_button_box_get_child_secondary (GtkButtonBox *widget,
				    GtkWidget    *child)
{
  GList *list;
  GtkBoxChild *child_info;

  g_return_val_if_fail (GTK_IS_BUTTON_BOX (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  child_info = NULL;
  list = GTK_BOX (widget)->children;
  while (list)
    {
      child_info = list->data;
      if (child_info->widget == child)
	break;

      list = list->next;
    }

  g_return_val_if_fail (list != NULL, FALSE);

  return child_info->is_secondary;
}

/**
 * gtk_button_box_set_child_secondary
 * @widget: a #GtkButtonBox
 * @child: a child of @widget
 * @is_secondary: if %TRUE, the @child appears in a secondary group of the
 *                button box.
 *
 * Sets whether @child should appear in a secondary group of children.
 * A typical use of a secondary child is the help button in a dialog.
 *
 * This group appears after the other children if the style
 * is %GTK_BUTTONBOX_START, %GTK_BUTTONBOX_SPREAD or
 * %GTK_BUTTONBOX_EDGE, and before the other children if the style
 * is %GTK_BUTTONBOX_END. For horizontal button boxes, the definition
 * of before/after depends on direction of the widget (see
 * gtk_widget_set_direction()). If the style is %GTK_BUTTONBOX_START
 * or %GTK_BUTTONBOX_END, then the secondary children are aligned at
 * the other end of the button box from the main children. For the
 * other styles, they appear immediately next to the main children.
 **/
void 
gtk_button_box_set_child_secondary (GtkButtonBox *widget, 
				    GtkWidget    *child,
				    gboolean      is_secondary)
{
  GList *list;
  
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (widget));

  list = GTK_BOX (widget)->children;
  while (list)
    {
      GtkBoxChild *child_info = list->data;
      if (child_info->widget == child)
	{
	  child_info->is_secondary = is_secondary;
	  break;
	}

      list = list->next;
    }

  gtk_widget_child_notify (child, "secondary");

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (child))
    gtk_widget_queue_resize (child);
}

/* Ask children how much space they require and round up 
   to match minimum size and internal padding.
   Returns the size each single child should have. */
void
_gtk_button_box_child_requisition (GtkWidget *widget,
                                   int       *nvis_children,
				   int       *nvis_secondaries,
                                   int       *width,
                                   int       *height)
{
  GtkButtonBox *bbox;
  GtkBoxChild *child;
  GList *children;
  gint nchildren;
  gint nsecondaries;
  gint needed_width;
  gint needed_height;
  GtkRequisition child_requisition;
  gint ipad_w;
  gint ipad_h;
  gint width_default;
  gint height_default;
  gint ipad_x_default;
  gint ipad_y_default;
  
  gint child_min_width;
  gint child_min_height;
  gint ipad_x;
  gint ipad_y;
  
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  bbox = GTK_BUTTON_BOX (widget);

  gtk_widget_style_get (widget,
                        "child-min-width", &width_default,
                        "child-min-height", &height_default,
                        "child-internal-pad-x", &ipad_x_default,
                        "child-internal-pad-y", &ipad_y_default, 
			NULL);
  
  child_min_width = bbox->child_min_width   != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_width : width_default;
  child_min_height = bbox->child_min_height !=GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_height : height_default;
  ipad_x = bbox->child_ipad_x != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_x : ipad_x_default;
  ipad_y = bbox->child_ipad_y != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_y : ipad_y_default;

  nchildren = 0;
  nsecondaries = 0;
  children = GTK_BOX(bbox)->children;
  needed_width = child_min_width;
  needed_height = child_min_height;  
  ipad_w = ipad_x * 2;
  ipad_h = ipad_y * 2;
  
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  nchildren += 1;
	  gtk_widget_size_request (child->widget, &child_requisition);
	  
	  if (child_requisition.width + ipad_w > needed_width)
	    needed_width = child_requisition.width + ipad_w;
	  if (child_requisition.height + ipad_h > needed_height)
	    needed_height = child_requisition.height + ipad_h;
	  if (child->is_secondary)
	    nsecondaries++;
	}
    }

  if (nvis_children)
    *nvis_children = nchildren;
  if (nvis_secondaries)
    *nvis_secondaries = nsecondaries;
  if (width)
    *width = needed_width;
  if (height)
    *height = needed_height;
}

#define __GTK_BUTTON_BOX_C__
#include "gtkaliasdef.c"
