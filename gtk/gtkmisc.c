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

#include "gtkcontainer.h"
#include "gtkmisc.h"


enum {
  ARG_0,
  ARG_XALIGN,
  ARG_YALIGN,
  ARG_XPAD,
  ARG_YPAD
};

static void gtk_misc_class_init (GtkMiscClass *klass);
static void gtk_misc_init       (GtkMisc      *misc);
static void gtk_misc_realize    (GtkWidget    *widget);
static void gtk_misc_set_arg    (GtkObject    *object,
				 GtkArg       *arg,
				 guint         arg_id);
static void gtk_misc_get_arg    (GtkObject    *object,
				 GtkArg       *arg,
				 guint         arg_id);


GtkType
gtk_misc_get_type (void)
{
  static GtkType misc_type = 0;

  if (!misc_type)
    {
      static const GtkTypeInfo misc_info =
      {
	"GtkMisc",
	sizeof (GtkMisc),
	sizeof (GtkMiscClass),
	(GtkClassInitFunc) gtk_misc_class_init,
	(GtkObjectInitFunc) gtk_misc_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      misc_type = gtk_type_unique (GTK_TYPE_WIDGET, &misc_info);
    }

  return misc_type;
}

static void
gtk_misc_class_init (GtkMiscClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gtk_object_add_arg_type ("GtkMisc::xalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_XALIGN);
  gtk_object_add_arg_type ("GtkMisc::yalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_YALIGN);
  gtk_object_add_arg_type ("GtkMisc::xpad", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_XPAD);
  gtk_object_add_arg_type ("GtkMisc::ypad", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_YPAD);

  object_class->set_arg = gtk_misc_set_arg;
  object_class->get_arg = gtk_misc_get_arg;
  
  widget_class->realize = gtk_misc_realize;
}

static void
gtk_misc_init (GtkMisc *misc)
{
  misc->xalign = 0.5;
  misc->yalign = 0.5;
  misc->xpad = 0;
  misc->ypad = 0;
}

static void
gtk_misc_set_arg (GtkObject      *object,
		  GtkArg         *arg,
		  guint           arg_id)
{
  GtkMisc *misc;

  misc = GTK_MISC (object);

  switch (arg_id)
    {
    case ARG_XALIGN:
      gtk_misc_set_alignment (misc, GTK_VALUE_FLOAT (*arg), misc->yalign);
      break;
    case ARG_YALIGN:
      gtk_misc_set_alignment (misc, misc->xalign, GTK_VALUE_FLOAT (*arg));
      break;
    case ARG_XPAD:
      gtk_misc_set_padding (misc, GTK_VALUE_INT (*arg), misc->ypad);
      break;
    case ARG_YPAD:
      gtk_misc_set_padding (misc, misc->xpad, GTK_VALUE_INT (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_misc_get_arg (GtkObject	 *object,
		  GtkArg         *arg,
		  guint           arg_id)
{
  GtkMisc *misc;

  misc = GTK_MISC (object);

  switch (arg_id)
    {
    case ARG_XALIGN:
      GTK_VALUE_FLOAT (*arg) = misc->xalign;
      break;
    case ARG_YALIGN:
      GTK_VALUE_FLOAT (*arg) = misc->yalign;
      break;
    case ARG_XPAD:
      GTK_VALUE_INT (*arg) = misc->xpad;
      break;
    case ARG_YPAD:
      GTK_VALUE_INT (*arg) = misc->ypad;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_misc_set_alignment (GtkMisc *misc,
			gfloat   xalign,
			gfloat   yalign)
{
  g_return_if_fail (misc != NULL);
  g_return_if_fail (GTK_IS_MISC (misc));

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (yalign < 0.0)
    yalign = 0.0;
  else if (yalign > 1.0)
    yalign = 1.0;

  if ((xalign != misc->xalign) || (yalign != misc->yalign))
    {
      misc->xalign = xalign;
      misc->yalign = yalign;
      
      /* clear the area that was allocated before the change
       */
      if (GTK_WIDGET_DRAWABLE (misc))
        {
          GtkWidget *widget;
	  
          widget = GTK_WIDGET (misc);
          gtk_widget_queue_clear (widget);
        }
    }
}

void
gtk_misc_set_padding (GtkMisc *misc,
		      gint     xpad,
		      gint     ypad)
{
  GtkRequisition *requisition;
  
  g_return_if_fail (misc != NULL);
  g_return_if_fail (GTK_IS_MISC (misc));
  
  if (xpad < 0)
    xpad = 0;
  if (ypad < 0)
    ypad = 0;
  
  if ((xpad != misc->xpad) || (ypad != misc->ypad))
    {
      requisition = &(GTK_WIDGET (misc)->requisition);
      requisition->width -= misc->xpad * 2;
      requisition->height -= misc->ypad * 2;
      
      misc->xpad = xpad;
      misc->ypad = ypad;
      
      requisition->width += misc->xpad * 2;
      requisition->height += misc->ypad * 2;
      
      if (GTK_WIDGET_DRAWABLE (misc))
	gtk_widget_queue_resize (GTK_WIDGET (misc));
    }
}

static void
gtk_misc_realize (GtkWidget *widget)
{
  GtkMisc *misc;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MISC (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  misc = GTK_MISC (widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      widget->window = gtk_widget_get_parent_window (widget);
      gdk_window_ref (widget->window);
      widget->style = gtk_style_attach (widget->style, widget->window);
    }
  else
    {
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = widget->allocation.x;
      attributes.y = widget->allocation.y;
      attributes.width = widget->allocation.width;
      attributes.height = widget->allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
      gdk_window_set_user_data (widget->window, widget);

      widget->style = gtk_style_attach (widget->style, widget->window);
      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
    }
}
