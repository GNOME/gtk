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
#include <gdk_imlib.h>

#define CHILD_SPACING     1
#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

struct _imgs
{
   GdkImlibImage *im1;
   GdkImlibImage *im2;
   GdkImlibImage *im3;
   GdkImlibImage *im4;
   GdkImlibImage *im5;
   GdkImlibImage *im6;
   GdkImlibImage *im7;
};

struct _butinfo
{
   int state;
   char has_focus;
   char has_default;
   int w,h;
};

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
	GTK_CONTAINER(widget)->internal_border_left=26;
	GTK_CONTAINER(widget)->internal_border_right=6;
	GTK_CONTAINER(widget)->internal_border_top=6;
	GTK_CONTAINER(widget)->internal_border_bottom=6;
     }
   else
     {
	GTK_CONTAINER(widget)->internal_border_left=4;
	GTK_CONTAINER(widget)->internal_border_right=4;
	GTK_CONTAINER(widget)->internal_border_top=4;
	GTK_CONTAINER(widget)->internal_border_bottom=4;
     }
}

void 
button_init               (GtkWidget        *widget)
{
   struct _butinfo *bi;

   bi=malloc(sizeof(struct _butinfo));
   GTK_CONTAINER(widget)->border_width=0;
   gtk_object_set_data(GTK_OBJECT(widget),"gtk-widget-theme-data",bi);
   bi->w=-1;bi->h=-1;bi->state=-1;bi->has_focus=-1;bi->has_default=1;
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
   struct _imgs *imgs;
   struct _butinfo *bi;
   
   /*
    * State
    * 
    * GTK_STATE_NORMAL,
    * GTK_STATE_ACTIVE,
    * GTK_STATE_PRELIGHT,
    * GTK_STATE_SELECTED,
    * GTK_STATE_INSENSITIVE
    */
   
   bi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   imgs=th_dat.data;
   if ((bi->w!=widget->allocation.width)||(bi->h!=widget->allocation.height)||
       (bi->state!=GTK_WIDGET_STATE(widget))||
       (bi->has_focus!=GTK_WIDGET_HAS_FOCUS(widget))||
       (bi->has_default!=GTK_WIDGET_CAN_DEFAULT(widget)))
     {
	if (GTK_WIDGET_STATE(widget)==GTK_STATE_ACTIVE)
	  gdk_imlib_apply_image(imgs->im3,widget->window);
	else if (GTK_WIDGET_STATE(widget)==GTK_STATE_PRELIGHT)
	  gdk_imlib_apply_image(imgs->im1,widget->window);
	else
	  gdk_imlib_apply_image(imgs->im2,widget->window);
	bi->w=widget->allocation.width;
	bi->h=widget->allocation.height;
	bi->state=GTK_WIDGET_STATE(widget);
	bi->has_focus=GTK_WIDGET_HAS_FOCUS(widget);
	bi->has_default=GTK_WIDGET_CAN_DEFAULT(widget);
     }
   if (GTK_WIDGET_HAS_DEFAULT(widget))
     {
	if (GTK_WIDGET_STATE(widget)==GTK_STATE_ACTIVE)
	  gdk_imlib_paste_image(imgs->im6,widget->window,6,
				(widget->allocation.height>>1)-6,
				12,12);
	else if (GTK_WIDGET_STATE(widget)==GTK_STATE_PRELIGHT)
	  gdk_imlib_paste_image(imgs->im6,widget->window,6,
				(widget->allocation.height>>1)-6,
				12,12);
	else
	  gdk_imlib_paste_image(imgs->im7,widget->window,6,
				(widget->allocation.height>>1)-6,
				12,12);
     }  
   else if (GTK_WIDGET_CAN_DEFAULT(widget))
     {
	if (GTK_WIDGET_STATE(widget)==GTK_STATE_ACTIVE)
	  gdk_imlib_paste_image(imgs->im4,widget->window,6,
				(widget->allocation.height>>1)-6,
				12,12);
	else if (GTK_WIDGET_STATE(widget)==GTK_STATE_PRELIGHT)
	  gdk_imlib_paste_image(imgs->im4,widget->window,6,
				(widget->allocation.height>>1)-6,
				12,12);
	else
	  gdk_imlib_paste_image(imgs->im5,widget->window,6,
				(widget->allocation.height>>1)-6,
				12,12);
     }
   return;
   
}

void 
button_exit               (GtkWidget        *widget)
{
  struct _butinfo *bi;

   bi=gtk_object_get_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
   free(bi);
   gtk_object_remove_data(GTK_OBJECT(widget),"gtk-widget-theme-data");
}

