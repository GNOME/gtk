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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <math.h>
#include "gtkarrow.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define MIN_ARROW_SIZE  15

enum {
  PROP_0,

  PROP_ARROW_TYPE,
  PROP_SHADOW_TYPE,
  
  PROP_LAST
};


static gint gtk_arrow_expose     (GtkWidget      *widget,
				  GdkEventExpose *event);
static void gtk_arrow_set_property (GObject         *object,
				    guint            prop_id,
				    const GValue    *value,
				    GParamSpec      *pspec);
static void gtk_arrow_get_property (GObject         *object,
				    guint            prop_id,
				    GValue          *value,
				    GParamSpec      *pspec);


G_DEFINE_TYPE (GtkArrow, gtk_arrow, GTK_TYPE_MISC)


static void
gtk_arrow_class_init (GtkArrowClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_arrow_set_property;
  gobject_class->get_property = gtk_arrow_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_ARROW_TYPE,
                                   g_param_spec_enum ("arrow-type",
                                                      P_("Arrow direction"),
                                                      P_("The direction the arrow should point"),
						      GTK_TYPE_ARROW_TYPE,
						      GTK_ARROW_RIGHT,
                                                      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Arrow shadow"),
                                                      P_("Appearance of the shadow surrounding the arrow"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));
  
  widget_class->expose_event = gtk_arrow_expose;
}

static void
gtk_arrow_set_property (GObject         *object,
			guint            prop_id,
			const GValue    *value,
			GParamSpec      *pspec)
{
  GtkArrow *arrow;
  
  arrow = GTK_ARROW (object);

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      gtk_arrow_set (arrow,
		     g_value_get_enum (value),
		     arrow->shadow_type);
      break;
    case PROP_SHADOW_TYPE:
      gtk_arrow_set (arrow,
		     arrow->arrow_type,
		     g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_arrow_get_property (GObject         *object,
			guint            prop_id,
			GValue          *value,
			GParamSpec      *pspec)
{
  GtkArrow *arrow;
  
  arrow = GTK_ARROW (object);
  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      g_value_set_enum (value, arrow->arrow_type);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, arrow->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_arrow_init (GtkArrow *arrow)
{
  GTK_WIDGET_SET_FLAGS (arrow, GTK_NO_WINDOW);

  GTK_WIDGET (arrow)->requisition.width = MIN_ARROW_SIZE + GTK_MISC (arrow)->xpad * 2;
  GTK_WIDGET (arrow)->requisition.height = MIN_ARROW_SIZE + GTK_MISC (arrow)->ypad * 2;

  arrow->arrow_type = GTK_ARROW_RIGHT;
  arrow->shadow_type = GTK_SHADOW_OUT;
}

GtkWidget*
gtk_arrow_new (GtkArrowType  arrow_type,
	       GtkShadowType shadow_type)
{
  GtkArrow *arrow;

  arrow = g_object_new (GTK_TYPE_ARROW, NULL);

  arrow->arrow_type = arrow_type;
  arrow->shadow_type = shadow_type;

  return GTK_WIDGET (arrow);
}

void
gtk_arrow_set (GtkArrow      *arrow,
	       GtkArrowType   arrow_type,
	       GtkShadowType  shadow_type)
{
  g_return_if_fail (GTK_IS_ARROW (arrow));

  if (   ((GtkArrowType) arrow->arrow_type != arrow_type)
      || ((GtkShadowType) arrow->shadow_type != shadow_type))
    {
      g_object_freeze_notify (G_OBJECT (arrow));

      if ((GtkArrowType) arrow->arrow_type != arrow_type)
        {
          arrow->arrow_type = arrow_type;
          g_object_notify (G_OBJECT (arrow), "arrow-type");
        }

      if ((GtkShadowType) arrow->shadow_type != shadow_type)
        {
          arrow->shadow_type = shadow_type;
          g_object_notify (G_OBJECT (arrow), "shadow-type");
        }

      g_object_thaw_notify (G_OBJECT (arrow));

      if (GTK_WIDGET_DRAWABLE (arrow))
	gtk_widget_queue_draw (GTK_WIDGET (arrow));
    }
}


static gboolean 
gtk_arrow_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkArrow *arrow;
  GtkMisc *misc;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint extent;
  gfloat xalign;
  GtkArrowType effective_arrow_type;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      arrow = GTK_ARROW (widget);
      misc = GTK_MISC (widget);

      width = widget->allocation.width - misc->xpad * 2;
      height = widget->allocation.height - misc->ypad * 2;
      extent = MIN (width, height) * 0.7;
      effective_arrow_type = arrow->arrow_type;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	{
	  xalign = 1.0 - misc->xalign;
	  if (arrow->arrow_type == GTK_ARROW_LEFT)
	    effective_arrow_type = GTK_ARROW_RIGHT;
	  else if (arrow->arrow_type == GTK_ARROW_RIGHT)
	    effective_arrow_type = GTK_ARROW_LEFT;
	}

      x = floor (widget->allocation.x + misc->xpad
		 + ((widget->allocation.width - extent) * xalign));
      y = floor (widget->allocation.y + misc->ypad 
		 + ((widget->allocation.height - extent) * misc->yalign));
      
      shadow_type = arrow->shadow_type;

      if (widget->state == GTK_STATE_ACTIVE)
	{
          if (shadow_type == GTK_SHADOW_IN)
            shadow_type = GTK_SHADOW_OUT;
          else if (shadow_type == GTK_SHADOW_OUT)
            shadow_type = GTK_SHADOW_IN;
          else if (shadow_type == GTK_SHADOW_ETCHED_IN)
            shadow_type = GTK_SHADOW_ETCHED_OUT;
          else if (shadow_type == GTK_SHADOW_ETCHED_OUT)
            shadow_type = GTK_SHADOW_ETCHED_IN;
	}

      gtk_paint_arrow (widget->style, widget->window,
		       widget->state, shadow_type,
		       &event->area, widget, "arrow",
		       effective_arrow_type, TRUE,
		       x, y, extent, extent);
    }

  return FALSE;
}

#define __GTK_ARROW_C__
#include "gtkaliasdef.c"
