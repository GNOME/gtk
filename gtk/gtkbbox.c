/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkbbox.h"


static void gtk_button_box_class_init    (GtkButtonBoxClass   *klass);
static void gtk_button_box_init          (GtkButtonBox        *box);


static gint default_child_min_width = 85;
static gint default_child_min_height = 27;
static gint default_child_ipad_x = 7;
static gint default_child_ipad_y = 0;


GtkType
gtk_button_box_get_type (void)
{
  static GtkType button_box_type = 0;

  if (!button_box_type)
    {
      static const GtkTypeInfo button_box_info =
      {
	"GtkButtonBox",
	sizeof (GtkButtonBox),
	sizeof (GtkButtonBoxClass),
	(GtkClassInitFunc) gtk_button_box_class_init,
	(GtkObjectInitFunc) gtk_button_box_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      button_box_type = gtk_type_unique (gtk_box_get_type (), &button_box_info);
    }

  return button_box_type;
}

static void
gtk_button_box_class_init (GtkButtonBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
}

static void
gtk_button_box_init (GtkButtonBox *button_box)
{
  button_box->spacing = GTK_BUTTONBOX_DEFAULT;
  button_box->child_min_width = GTK_BUTTONBOX_DEFAULT;
  button_box->child_min_height = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_x = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_y = GTK_BUTTONBOX_DEFAULT;
  button_box->layout_style = GTK_BUTTONBOX_DEFAULT_STYLE;
}


/* set default values for child size and child internal padding */
/* default spacing is in defined in subclasses */

void gtk_button_box_set_child_size_default (gint width, gint height)
{
  default_child_min_width = width;
  default_child_min_height = height;
}

void gtk_button_box_set_child_ipadding_default (gint ipad_x, gint ipad_y)
{
  default_child_ipad_x = ipad_x;
  default_child_ipad_y = ipad_y;
}

/* get default values for child size and child internal padding */

void gtk_button_box_get_child_size_default (gint *width, gint *height)
{
  *width = default_child_min_width;
  *height = default_child_min_height;
}

void gtk_button_box_get_child_ipadding_default (gint *ipad_x, gint *ipad_y)
{
  *ipad_x = default_child_ipad_x;
  *ipad_y = default_child_ipad_y;
}

/* set per widget values for spacing, child size and child internal padding */

void gtk_button_box_set_spacing (GtkButtonBox *widget, gint spacing)
{
  widget->spacing = spacing;
}

void gtk_button_box_set_child_size (GtkButtonBox *widget, gint width, gint height)
{
  widget->child_min_width = width;
  widget->child_min_height = height;
}

void gtk_button_box_set_child_ipadding (GtkButtonBox *widget,
					gint ipad_x, gint ipad_y)
{
  widget->child_ipad_x = ipad_x;
  widget->child_ipad_y = ipad_y;
}

void gtk_button_box_set_layout (GtkButtonBox *widget, 
				GtkButtonBoxStyle layout_style)
{
  g_return_if_fail (layout_style >= GTK_BUTTONBOX_DEFAULT_STYLE &&
		    layout_style <= GTK_BUTTONBOX_END);

  widget->layout_style = layout_style;
}


/* get per widget values for spacing, child size and child internal padding */

gint gtk_button_box_get_spacing (GtkButtonBox *widget)
{
  return widget->spacing;
}

void gtk_button_box_get_child_size (GtkButtonBox *widget,
				     gint *width, gint *height)
{
  *width  = widget->child_min_width;
  *height = widget->child_min_height;
}

void gtk_button_box_get_child_ipadding (GtkButtonBox *widget,
					 gint* ipad_x, gint *ipad_y)
{
  *ipad_x = widget->child_ipad_x;
  *ipad_y = widget->child_ipad_y;
}

GtkButtonBoxStyle gtk_button_box_get_layout (GtkButtonBox *widget)
{
  return widget->layout_style;
}



/* Ask children how much space they require and round up 
   to match minimum size and internal padding.
   Returns the size each single child should have. */
void
gtk_button_box_child_requisition (GtkWidget *widget,
				  int *nvis_children,
				  int *width,
				  int *height)
{
  GtkButtonBox *bbox;
  GtkBoxChild *child;
  GList *children;
  gint nchildren;
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
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  bbox = GTK_BUTTON_BOX (widget);

  gtk_button_box_get_child_size_default (&width_default, &height_default);
  gtk_button_box_get_child_ipadding_default (&ipad_x_default, &ipad_y_default);
  
  child_min_width = bbox->child_min_width   != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_width : width_default;
  child_min_height = bbox->child_min_height !=GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_height : height_default;
  ipad_x = bbox->child_ipad_x != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_x : ipad_x_default;
  ipad_y = bbox->child_ipad_y != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_y : ipad_y_default;

  nchildren = 0;
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
	}
    }
  
  *nvis_children = nchildren;
  *width = needed_width;
  *height = needed_height;
}
