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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gtkthemes.h"
#include "gtkprivate.h"
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"

#define CHILD_SPACING     1
#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

/* Theme functions to export */
void button_border             (GtkWidget        *widget);
void button_init               (GtkWidget        *widget);
void button_draw               (GtkWidget        *widget,
				       GdkRectangle     *area);
void button_exit               (GtkWidget        *widget);

/* internals */

void 
button_border (GtkWidget        *widget)
{
   if (GTK_WIDGET_CAN_DEFAULT (widget))
     {
	GTK_CONTAINER(widget)->internal_border_left=10;
	GTK_CONTAINER(widget)->internal_border_right=10;
	GTK_CONTAINER(widget)->internal_border_top=10;
	GTK_CONTAINER(widget)->internal_border_bottom=10;
     }
   else
     {
	GTK_CONTAINER(widget)->internal_border_left=2;
	GTK_CONTAINER(widget)->internal_border_right=2;
	GTK_CONTAINER(widget)->internal_border_top=2;
	GTK_CONTAINER(widget)->internal_border_bottom=2;
     }
}

void 
button_init (GtkWidget        *widget)
{
}

void 
button_draw               (GtkWidget        *widget,
			   GdkRectangle     *area)
{
   GdkRectangle restrict_area;
   GdkRectangle new_area;
   GtkShadowType shadow_type;
   gint width, height;
   gint x, y;

   /*
    * State
    * 
    * GTK_STATE_NORMAL,
    * GTK_STATE_ACTIVE,
    * GTK_STATE_PRELIGHT,
    * GTK_STATE_SELECTED,
    * GTK_STATE_INSENSITIVE
    */
   
   restrict_area.x = GTK_WIDGET (widget)->style->klass->xthickness;
   restrict_area.y = GTK_WIDGET (widget)->style->klass->ythickness;
   restrict_area.width = (GTK_WIDGET (widget)->allocation.width - restrict_area.x * 2 -
			  GTK_CONTAINER (widget)->border_width * 2);
   restrict_area.height = (GTK_WIDGET (widget)->allocation.height - restrict_area.y * 2 -
			   GTK_CONTAINER (widget)->border_width * 2);
   if (GTK_WIDGET_CAN_DEFAULT (widget))
     {
	restrict_area.x += DEFAULT_LEFT_POS;
	restrict_area.y += DEFAULT_TOP_POS;
	restrict_area.width -= DEFAULT_SPACING;
	restrict_area.height -= DEFAULT_SPACING;
     }
   if (gdk_rectangle_intersect (area, &restrict_area, &new_area))
     {
	gtk_style_set_background (widget->style, widget->window, GTK_WIDGET_STATE (widget));
	gdk_window_clear_area (widget->window,new_area.x, new_area.y,
			       new_area.width, new_area.height);
     }
   x = 0;
   y = 0;
   width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
   height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;
   if (GTK_WIDGET_HAS_DEFAULT (widget))
     {
	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, GTK_SHADOW_IN,
			 x, y, width, height);
     }
   else
     {
	gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_NORMAL],
			    FALSE, x, y, width - 1, height - 1);
	gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_NORMAL],
			    FALSE, x + 1, y + 1, width - 3, height - 3);
     }
   x = 0;y = 0;
   width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
   height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;
   if (GTK_WIDGET_CAN_DEFAULT (widget))
     {
	x += widget->style->klass->xthickness;
	y += widget->style->klass->ythickness;
	width -= 2 * x + DEFAULT_SPACING;
	height -= 2 * y + DEFAULT_SPACING;
	x += DEFAULT_LEFT_POS;
	y += DEFAULT_TOP_POS;
     }
   if (GTK_WIDGET_HAS_FOCUS (widget))
     {
	x += 1;y += 1;width -= 2;height -= 2;
     }
   else
     {
	if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
	  gdk_draw_rectangle (widget->window,
			      widget->style->bg_gc[GTK_WIDGET_STATE (widget)], 
			      FALSE, x + 1, y + 1, width - 4, height - 4);
	else
	  gdk_draw_rectangle (widget->window,
			      widget->style->bg_gc[GTK_WIDGET_STATE (widget)], 
			      FALSE, x + 2, y + 2, width - 5, height - 5);
     }
   if (GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE)
     shadow_type = GTK_SHADOW_IN;
   else
     shadow_type = GTK_SHADOW_OUT;
   gtk_draw_shadow (widget->style, widget->window,GTK_WIDGET_STATE (widget), 
		    shadow_type,x, y, width, height);
   if (GTK_WIDGET_HAS_FOCUS (widget))
     {
	x -= 1;y -= 1;width += 2;height += 2;
	gdk_draw_rectangle (widget->window,widget->style->black_gc, FALSE,
			    x, y, width - 1, height - 1);
     }
}

void 
button_exit               (GtkWidget        *widget)
{
}

