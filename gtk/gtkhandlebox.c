/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
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


#include <stdlib.h>
#include "gdk/gdkx.h"
#include "gtkhandlebox.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkwindow.h"


#define DRAG_HANDLE_SIZE 10
#define CHILDLESS_SIZE	25
#define GHOST_HEIGHT 3

enum
{
  SIGNAL_CHILD_ATTACHED,
  SIGNAL_CHILD_DETACHED,
  SIGNAL_LAST
};

typedef void    (*SignalChildAttached)          (GtkObject      *object,
						 GtkWidget      *widget,
						 gpointer        func_data);

static void gtk_handle_box_class_init     (GtkHandleBoxClass *klass);
static void gtk_handle_box_init           (GtkHandleBox      *handle_box);
static void gtk_handle_box_destroy        (GtkObject         *object);
static void gtk_handle_box_map            (GtkWidget         *widget);
static void gtk_handle_box_unmap          (GtkWidget         *widget);
static void gtk_handle_box_realize        (GtkWidget         *widget);
static void gtk_handle_box_unrealize      (GtkWidget         *widget);
static void gtk_handle_box_size_request   (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_handle_box_size_allocate  (GtkWidget         *widget,
					   GtkAllocation     *real_allocation);
static void gtk_handle_box_add            (GtkContainer      *container,
					   GtkWidget         *widget);
static void gtk_handle_box_remove         (GtkContainer      *container,
					   GtkWidget         *widget);
static void gtk_handle_box_draw_ghost     (GtkHandleBox      *hb);
static void gtk_handle_box_paint          (GtkWidget         *widget,
					   GdkEventExpose    *event,
					   GdkRectangle      *area);
static void gtk_handle_box_draw           (GtkWidget         *widget,
					   GdkRectangle      *area);
static gint gtk_handle_box_expose         (GtkWidget         *widget,
					   GdkEventExpose    *event);
static gint gtk_handle_box_button_changed (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gtk_handle_box_motion         (GtkWidget         *widget,
					   GdkEventMotion    *event);
static gint gtk_handle_box_delete_event   (GtkWidget         *widget,
					   GdkEventAny       *event);


static GtkBinClass *parent_class;
static guint        handle_box_signals[SIGNAL_LAST] = { 0 };


guint
gtk_handle_box_get_type (void)
{
  static guint handle_box_type = 0;

  if (!handle_box_type)
    {
      GtkTypeInfo handle_box_info =
      {
	"GtkHandleBox",
	sizeof (GtkHandleBox),
	sizeof (GtkHandleBoxClass),
	(GtkClassInitFunc) gtk_handle_box_class_init,
	(GtkObjectInitFunc) gtk_handle_box_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      handle_box_type = gtk_type_unique (gtk_bin_get_type (), &handle_box_info);
    }

  return handle_box_type;
}

static void
gtk_handle_box_marshal_child_attached (GtkObject      *object,
				       GtkSignalFunc  func,
				       gpointer       func_data,
				       GtkArg         *args)
{
  SignalChildAttached sfunc = (SignalChildAttached) func;

  (* sfunc) (object,
	     (GtkWidget*) GTK_VALUE_OBJECT (args[0]),
	     func_data);
}

static void
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (gtk_bin_get_type ());

  handle_box_signals[SIGNAL_CHILD_ATTACHED] =
    gtk_signal_new ("child_attached",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHandleBoxClass, child_attached),
		    gtk_handle_box_marshal_child_attached,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_WIDGET);
  handle_box_signals[SIGNAL_CHILD_DETACHED] =
    gtk_signal_new ("child_detached",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHandleBoxClass, child_detached),
		    gtk_handle_box_marshal_child_attached,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_WIDGET);
  gtk_object_class_add_signals (object_class, handle_box_signals, SIGNAL_LAST);
  
  object_class->destroy = gtk_handle_box_destroy;

  widget_class->map = gtk_handle_box_map;
  widget_class->unmap = gtk_handle_box_unmap;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->unrealize = gtk_handle_box_unrealize;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->draw = gtk_handle_box_draw;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_changed;
  widget_class->button_release_event = gtk_handle_box_button_changed;
  widget_class->motion_notify_event = gtk_handle_box_motion;
  widget_class->delete_event = gtk_handle_box_delete_event;

  container_class->add = gtk_handle_box_add;
  container_class->remove = gtk_handle_box_remove;

  class->child_attached = NULL;
  class->child_detached = NULL;
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  GTK_WIDGET_UNSET_FLAGS (handle_box, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (handle_box, GTK_BASIC); /* FIXME: are we really a basic widget? */

  handle_box->bin_window = NULL;
  handle_box->float_window = NULL;
  handle_box->handle_position = GTK_POS_LEFT;
  handle_box->float_window_mapped = FALSE;
  handle_box->child_detached = FALSE;
  handle_box->in_drag = FALSE;
  handle_box->fleur_cursor = gdk_cursor_new (GDK_FLEUR);
  handle_box->dragoff_x = 0;
  handle_box->dragoff_y = 0;
}

GtkWidget*
gtk_handle_box_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_handle_box_get_type ()));
}

static void
gtk_handle_box_destroy (GtkObject *object)
{
  GtkHandleBox *hb;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (object));

  hb = GTK_HANDLE_BOX (object);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_handle_box_map (GtkWidget *widget)
{
  GtkBin *bin;
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  gdk_window_show (hb->bin_window);
  gdk_window_show (widget->window);

  if (hb->child_detached && !hb->float_window_mapped)
    {
      gdk_window_show (hb->float_window);
      hb->float_window_mapped = TRUE;
    }

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_map (bin->child);
}

static void
gtk_handle_box_unmap (GtkWidget *widget)
{
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  hb = GTK_HANDLE_BOX (widget);

  gdk_window_hide (widget->window);
  if (hb->float_window_mapped)
    {
      gdk_window_hide (hb->float_window);
      hb->float_window_mapped = FALSE;
    }
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  hb = GTK_HANDLE_BOX (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget)
			   | GDK_EXPOSURE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask |= (gtk_widget_get_events (widget) |
			    GDK_EXPOSURE_MASK |
			    GDK_BUTTON1_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  hb->bin_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (hb->bin_window, widget);
  if (GTK_BIN (hb)->child)
    gtk_widget_set_parent_window (GTK_BIN (hb)->child, hb->bin_window);
  
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->requisition.width;
  attributes.height = widget->requisition.height;
  attributes.window_type = GDK_WINDOW_TOPLEVEL;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_KEY_PRESS_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_FOCUS_CHANGE_MASK |
			   GDK_STRUCTURE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  hb->float_window = gdk_window_new (NULL, &attributes, attributes_mask);
  gdk_window_set_user_data (hb->float_window, widget);
  gdk_window_set_decorations (hb->float_window, 0);
  

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_WIDGET_STATE (hb));
  gtk_style_set_background (widget->style, hb->bin_window, GTK_WIDGET_STATE (hb));
  gtk_style_set_background (widget->style, hb->float_window, GTK_WIDGET_STATE (hb));
}

static void
gtk_handle_box_unrealize (GtkWidget *widget)
{
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  hb = GTK_HANDLE_BOX (widget);

  gdk_window_set_user_data (hb->bin_window, NULL);
  gdk_window_destroy (hb->bin_window);
  hb->bin_window = NULL;
  gdk_window_set_user_data (hb->float_window, NULL);
  gdk_window_destroy (hb->float_window);
  hb->float_window = NULL;

  gdk_cursor_destroy (hb->fleur_cursor);
  hb->fleur_cursor = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  if (hb->handle_position == GTK_POS_LEFT ||
      hb->handle_position == GTK_POS_RIGHT)
    {
      requisition->width = DRAG_HANDLE_SIZE;
      requisition->height = 0;
    }
  else
    {
      requisition->width = 0;
      requisition->height = DRAG_HANDLE_SIZE;
    }
  requisition->width += GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2;

  /* if our child is not visible, we still request its size, since we
   * won't have any usefull hint for our size otherwise.
   */
  if (bin->child)
    gtk_widget_size_request (bin->child, &bin->child->requisition);

  if (hb->child_detached)
    {
      if (hb->handle_position == GTK_POS_LEFT ||
	  hb->handle_position == GTK_POS_RIGHT)
	requisition->height += bin->child->requisition.height;
      else
	requisition->width += bin->child->requisition.width;
    }
  else if (bin->child)
    {
      requisition->width += bin->child->requisition.width;
      requisition->height += bin->child->requisition.height;
    }
  else
    {
      requisition->width += CHILDLESS_SIZE;
      requisition->height += CHILDLESS_SIZE;
    }
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (allocation != NULL);
  
  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);
  
  widget->allocation.x = allocation->x;

  if (hb->child_detached)
    {
      if (allocation->height > widget->requisition.height)
	widget->allocation.y = allocation->y +
	  (allocation->height - widget->requisition.height) / 2;
      else
	widget->allocation.y = allocation->y;
      
      widget->allocation.height = MIN (allocation->height, widget->requisition.height);
      widget->allocation.width = MIN (allocation->width, widget->requisition.width);
    }
  else
    {
      widget->allocation = *allocation;
    }  

  if (GTK_WIDGET_REALIZED (hb))
    gdk_window_move_resize (widget->window,
			    widget->allocation.x,
			    widget->allocation.y,
			    widget->allocation.width,
			    widget->allocation.height);


  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkWidget *child;
      GtkAllocation child_allocation;
      guint border_width;

      child = bin->child;
      border_width = GTK_CONTAINER (widget)->border_width;

      child_allocation.x = border_width;
      child_allocation.y = border_width;
      if (hb->handle_position == GTK_POS_LEFT)
	child_allocation.x += DRAG_HANDLE_SIZE;
      else if (hb->handle_position == GTK_POS_TOP)
	child_allocation.y += DRAG_HANDLE_SIZE;

      if (hb->child_detached)
	{
	  guint float_width;
	  guint float_height;
	  
	  child_allocation.width = child->requisition.width;
	  child_allocation.height = child->requisition.height;
	  
	  float_width = child_allocation.width + 2 * border_width;
	  float_height = child_allocation.height + 2 * border_width;
	  
	  if (hb->handle_position == GTK_POS_LEFT ||
	      hb->handle_position == GTK_POS_RIGHT)
	    float_width += DRAG_HANDLE_SIZE;
	  else
	    float_height += DRAG_HANDLE_SIZE;

	  if (GTK_WIDGET_REALIZED (hb))
	    {
	      gdk_window_resize (hb->float_window,
				 float_width,
				 float_height);
	      gdk_window_move_resize (hb->bin_window,
				      0,
				      0,
				      float_width,
				      float_height);
	    }
	}
      else
	{
	  child_allocation.width = widget->allocation.width - 2 * border_width;
	  child_allocation.height = widget->allocation.height - 2 * border_width;

	  if (hb->handle_position == GTK_POS_LEFT ||
	      hb->handle_position == GTK_POS_RIGHT)
	    child_allocation.width -= DRAG_HANDLE_SIZE;
	  else
	    child_allocation.height -= DRAG_HANDLE_SIZE;
	  
	  if (GTK_WIDGET_REALIZED (hb))
	    gdk_window_move_resize (hb->bin_window,
				    0,
				    0,
				    widget->allocation.width,
				    widget->allocation.height);
	}

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
gtk_handle_box_draw_ghost (GtkHandleBox *hb)
{
  GtkWidget *widget;
  guint x;
  guint y;
  guint width;
  guint height;

  widget = GTK_WIDGET (hb);

  if (hb->handle_position == GTK_POS_LEFT ||
      hb->handle_position == GTK_POS_RIGHT)
    {
      x = hb->handle_position == GTK_POS_LEFT ? 0 : widget->allocation.width;
      y = 0;
      width = DRAG_HANDLE_SIZE;
      height = widget->allocation.height;
    }
  else
    {
      x = 0;
      y = hb->handle_position == GTK_POS_TOP ? 0 : widget->allocation.height;
      width = widget->allocation.width;
      height = DRAG_HANDLE_SIZE;
    }
  gtk_draw_shadow (widget->style,
		   widget->window,
		   GTK_WIDGET_STATE (widget),
		   GTK_SHADOW_ETCHED_IN,
		   x,
		   y,
		   width,
		   height);
  /*
  if (hb->handle_position == GTK_POS_LEFT ||
      hb->handle_position == GTK_POS_RIGHT)
    gtk_draw_hline (widget->style,
		    widget->window,
		    GTK_WIDGET_STATE (widget),
		    hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : widget->allocation.width - DRAG_HANDLE_SIZE,
		    widget->allocation.width - DRAG_HANDLE_SIZE,
		    widget->allocation.height / 2);
  else
    gtk_draw_vline (widget->style,
		    widget->window,
		    GTK_WIDGET_STATE (widget),
		    hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : widget->allocation.height - DRAG_HANDLE_SIZE,
		    widget->allocation.height - DRAG_HANDLE_SIZE,
		    widget->allocation.width / 2);
		    */
}

static void
gtk_handle_box_paint (GtkWidget      *widget,
		      GdkEventExpose *event,
		      GdkRectangle   *area)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  guint width;
  guint height;
  guint border_width;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  border_width = GTK_CONTAINER (hb)->border_width;

  if (hb->child_detached)
    {
      width = bin->child->allocation.width + 2 * border_width;
      height = bin->child->allocation.height + 2 * border_width;
    }
  else if (hb->handle_position == GTK_POS_LEFT ||
	   hb->handle_position == GTK_POS_RIGHT)
    {
      width = widget->allocation.width - DRAG_HANDLE_SIZE;
      height = widget->allocation.height;
    }
  else
    {
      width = widget->allocation.width;
      height = widget->allocation.height - DRAG_HANDLE_SIZE;
    }

  gdk_window_clear (hb->bin_window);
  gtk_draw_shadow (widget->style,
		   hb->bin_window,
		   GTK_WIDGET_STATE (widget),
		   GTK_SHADOW_OUT,
		   hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		   hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		   width,
		   height);

  if (hb->handle_position == GTK_POS_LEFT ||
      hb->handle_position == GTK_POS_RIGHT)
    {
      guint x;

      for (x = 1; x < DRAG_HANDLE_SIZE; x += 3)
	
	gtk_draw_vline (widget->style,
			hb->bin_window,
			GTK_WIDGET_STATE (widget),
			widget->style->klass->ythickness,
			height + DRAG_HANDLE_SIZE - widget->style->klass->ythickness,
			hb->handle_position == GTK_POS_LEFT ? x : width + x);
      gtk_draw_hline (widget->style,
		      hb->bin_window,
		      GTK_WIDGET_STATE (widget),
		      hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : width,
		      hb->handle_position == GTK_POS_LEFT ? 0 : width + DRAG_HANDLE_SIZE,
		      0);
      gtk_draw_hline (widget->style,
		      hb->bin_window,
		      GTK_WIDGET_STATE (widget),
		      hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : width,
		      hb->handle_position == GTK_POS_LEFT ? 0 : width + DRAG_HANDLE_SIZE,
		      height - widget->style->klass->ythickness);
    }
  else
    {
      guint y;

      for (y = 1; y < DRAG_HANDLE_SIZE; y += 3)
	
	gtk_draw_hline (widget->style,
			hb->bin_window,
			GTK_WIDGET_STATE (widget),
			widget->style->klass->xthickness,
			width + DRAG_HANDLE_SIZE - widget->style->klass->xthickness,
			hb->handle_position == GTK_POS_TOP ? y : height + y);
      gtk_draw_vline (widget->style,
		      hb->bin_window,
		      GTK_WIDGET_STATE (widget),
		      hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : height,
		      hb->handle_position == GTK_POS_TOP ? 0 : height + DRAG_HANDLE_SIZE,
		      0);
      gtk_draw_vline (widget->style,
		      hb->bin_window,
		      GTK_WIDGET_STATE (widget),
		      hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : height,
		      hb->handle_position == GTK_POS_TOP ? 0 : height + DRAG_HANDLE_SIZE,
		      width - widget->style->klass->xthickness);
    }
  

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GdkRectangle child_area;
      GdkEventExpose child_event;

      if (!event) /* we were called from draw() */
	{
	  if (gtk_widget_intersect (bin->child, area, &child_area))
	    gtk_widget_draw (bin->child, &child_area);
	}
      else /* we were called from expose() */
	{
	  child_event = *event;
	  
	  if (GTK_WIDGET_NO_WINDOW (bin->child) &&
	      gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	    gtk_widget_event (bin->child, (GdkEvent *) &child_event);
	}
    }
}

static void
gtk_handle_box_draw (GtkWidget    *widget,
		     GdkRectangle *area)
{
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (area != NULL);

  hb = GTK_HANDLE_BOX (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (hb->child_detached)
	{
	  /* The area parameter does not make sense in this case, so we
	   * repaint everything.
	   */

	  gtk_handle_box_draw_ghost (hb);

	  area->x = 0;
	  area->y = 0;
	  area->width = 2 * GTK_CONTAINER (hb)->border_width + DRAG_HANDLE_SIZE;
	  area->height = area->width + GTK_BIN (hb)->child->allocation.height;
	  area->width += GTK_BIN (hb)->child->allocation.width;

	  gtk_handle_box_paint (widget, NULL, area);
	}
      else
	gtk_handle_box_paint (widget, NULL, area);
    }
}

static gint
gtk_handle_box_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  GtkHandleBox *hb;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      hb = GTK_HANDLE_BOX (widget);

      if (event->window == widget->window)
	{
	  if (hb->child_detached)
	    gtk_handle_box_draw_ghost (hb);
	}
      else
	gtk_handle_box_paint (widget, event, NULL);
    }
  
  return FALSE;
}

static gint
gtk_handle_box_button_changed (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GtkHandleBox *hb;
  gboolean event_handled;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  event_handled = FALSE;
  if (event->button == 1 &&
      event->type == GDK_BUTTON_PRESS)
    {
      GtkWidget *child;
      gboolean in_handle;
      
      child = GTK_BIN (hb)->child;
      
      switch (hb->handle_position)
	{
	case GTK_POS_LEFT:
	  in_handle = event->x < DRAG_HANDLE_SIZE;
	  break;
	case GTK_POS_TOP:
	  in_handle = event->y < DRAG_HANDLE_SIZE;
	  break;
	case GTK_POS_RIGHT:
	  in_handle = event->x > 2 * GTK_CONTAINER (hb)->border_width + child->allocation.width;
	  break;
	case GTK_POS_BOTTOM:
	  in_handle = event->y > 2 * GTK_CONTAINER (hb)->border_width + child->allocation.height;
	  break;
	default:
	  in_handle = FALSE;
	  break;
	}

      if (!child)
	{
	  in_handle = FALSE;
	  event_handled = TRUE;
	}
      
      if (in_handle)
	{
	  hb->dragoff_x = event->x;
	  hb->dragoff_y = event->y;

	  gtk_grab_add (widget);
	  hb->in_drag = TRUE;
	  while (gdk_pointer_grab (hb->bin_window,
				   FALSE,
				   (GDK_BUTTON1_MOTION_MASK |
				    GDK_POINTER_MOTION_HINT_MASK |
				    GDK_BUTTON_RELEASE_MASK),
				   NULL,
				   hb->fleur_cursor,
				   GDK_CURRENT_TIME) != 0); /* wait for success */
	  event_handled = TRUE;
	}
    }
  else if (event->type == GDK_BUTTON_RELEASE &&
	   hb->in_drag)
    {
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gtk_grab_remove (widget);
      hb->in_drag = FALSE;
      event_handled = TRUE;
    }
  
  return event_handled;
}

static gint
gtk_handle_box_motion (GtkWidget      *widget,
		       GdkEventMotion *event)
{
  GtkHandleBox *hb;
  gint new_x, new_y;
  gint ox, oy;
  gint snap_x, snap_y;
  gboolean in_handle;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);
  if (!hb->in_drag)
    return FALSE;

  ox = 0;
  oy = 0;
  gdk_window_get_origin (hb->float_window, &ox, &oy);
  new_x = 0;
  new_y = 0;
  gdk_window_get_pointer (hb->float_window, &new_x, &new_y, NULL);
  new_x += ox;
  new_y += oy;
  new_x -= hb->dragoff_x;
  new_y -= hb->dragoff_y;
  snap_x = 0;
  snap_y = 0;
  gdk_window_get_pointer (widget->window, &snap_x, &snap_y, NULL);
  
  switch (hb->handle_position)
    {
    case GTK_POS_LEFT:
      in_handle = (snap_x >= 0 && snap_x <= DRAG_HANDLE_SIZE &&
		   snap_y >= 0 && snap_y <= widget->allocation.height);
      break;
    case GTK_POS_TOP:
      in_handle = (snap_x >= 0 && snap_x <= widget->allocation.width &&
		   snap_y >= 0 && snap_y <= DRAG_HANDLE_SIZE);
      break;
    case GTK_POS_RIGHT:
      in_handle = (snap_x >= widget->allocation.width - DRAG_HANDLE_SIZE && snap_x <= widget->allocation.width &&
		   snap_y >= 0 && snap_y <= widget->allocation.height);
      break;
    case GTK_POS_BOTTOM:
      in_handle = (snap_x >= 0 && snap_x <= widget->allocation.width &&
		   snap_y >= widget->allocation.height - DRAG_HANDLE_SIZE && snap_y <= widget->allocation.height);
      break;
    default:
      in_handle = FALSE;
      break;
    }

  if (in_handle)
    {
      if (hb->child_detached)
	{
	  gdk_pointer_ungrab (GDK_CURRENT_TIME);
	  
	  hb->child_detached = FALSE;
	  gdk_window_hide (hb->float_window);
	  gdk_window_reparent (hb->bin_window, widget->window, 0, 0);
	  hb->float_window_mapped = FALSE;
	  gtk_signal_emit (GTK_OBJECT (hb),
			   handle_box_signals[SIGNAL_CHILD_ATTACHED],
			   GTK_BIN (hb)->child);
	  
	  while (gdk_pointer_grab (hb->bin_window,
				   FALSE,
				   (GDK_BUTTON1_MOTION_MASK |
				    GDK_POINTER_MOTION_HINT_MASK |
				    GDK_BUTTON_RELEASE_MASK),
				   NULL,
				   hb->fleur_cursor,
				   GDK_CURRENT_TIME) != 0); /* wait for success */
	  
	  gtk_widget_queue_resize (widget);
	}
    }
  else
    {
      if (hb->child_detached)
	{
	  gdk_window_move (hb->float_window, new_x, new_y);
	  gdk_window_raise (hb->float_window);
	}
      else
	{
	  gint width;
	  gint height;

	  gdk_pointer_ungrab (GDK_CURRENT_TIME);

	  hb->child_detached = TRUE;
	  width = 0;
	  height = 0;
	  gdk_window_get_size (hb->bin_window, &width, &height);
	  gdk_window_move_resize (hb->float_window, new_x, new_y, width, height);
	  gdk_window_reparent (hb->bin_window, hb->float_window, 0, 0);
	  gdk_window_set_hints (hb->float_window, new_x, new_y, 0, 0, 0, 0, GDK_HINT_POS);
	  gdk_window_show (hb->float_window);
	  hb->float_window_mapped = TRUE;
#if	0
	  /* this extra move is neccessary if we use decorations, or our
	   * window manager insists on decorations.
	   */
	  gdk_flush ();
	  gdk_window_move (hb->float_window, new_x, new_y);
	  gdk_flush ();
#endif	/* 0 */
	  gtk_signal_emit (GTK_OBJECT (hb),
			   handle_box_signals[SIGNAL_CHILD_DETACHED],
			   GTK_BIN (hb)->child);
	  gtk_handle_box_draw_ghost (hb);
	  
	  while (gdk_pointer_grab (hb->bin_window,
				   FALSE,
				   (GDK_BUTTON1_MOTION_MASK |
				    GDK_POINTER_MOTION_HINT_MASK |
				    GDK_BUTTON_RELEASE_MASK),
				   NULL,
				   hb->fleur_cursor,
				   GDK_CURRENT_TIME) != 0); /* wait for success */
	  
	  gtk_widget_queue_resize (widget);
	}
    }

  return TRUE;
}

static void
gtk_handle_box_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (container));
  g_return_if_fail (GTK_BIN (container)->child == NULL);
  g_return_if_fail (widget->parent == NULL);

  gtk_widget_set_parent_window (widget, GTK_HANDLE_BOX (container)->bin_window);
  GTK_CONTAINER_CLASS (parent_class)->add (container, widget);
}

static void
gtk_handle_box_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GtkHandleBox *hb;

  g_return_if_fail (GTK_IS_HANDLE_BOX (container));
  g_return_if_fail (GTK_BIN (container)->child == widget);

  hb = GTK_HANDLE_BOX (container);

  if (hb->child_detached)
    {
      hb->child_detached = FALSE;
      if (GTK_WIDGET_REALIZED (hb))
	{
	  gdk_window_hide (hb->float_window);
	  gdk_window_reparent (hb->bin_window, GTK_WIDGET (hb)->window, 0, 0);
	}
      hb->float_window_mapped = FALSE;
    }
  if (hb->in_drag)
    {
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gtk_grab_remove (GTK_WIDGET (container));
      hb->in_drag = FALSE;
    }
  
  GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static gint
gtk_handle_box_delete_event (GtkWidget *widget,
			     GdkEventAny  *event)
{
  GtkHandleBox *hb;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);
  g_return_val_if_fail (event->window == hb->float_window, FALSE);
  
  hb->child_detached = FALSE;
  gdk_window_hide (hb->float_window);
  gdk_window_reparent (hb->bin_window, widget->window, 0, 0);
  hb->float_window_mapped = FALSE;
  gtk_signal_emit (GTK_OBJECT (hb),
		   handle_box_signals[SIGNAL_CHILD_ATTACHED],
		   GTK_BIN (hb)->child);
  if (hb->in_drag)
    {
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gtk_grab_remove (widget);
      hb->in_drag = FALSE;
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (hb));

  return TRUE;
}
