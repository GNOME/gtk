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

#include <stdlib.h>
#include "gdk/gdkx.h"
#include "gtkhandlebox.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkwindow.h"

enum {
  ARG_0,
  ARG_SHADOW,
  ARG_HANDLE_POSITION,
  ARG_SNAP_EDGE
};

#define DRAG_HANDLE_SIZE 10
#define CHILDLESS_SIZE	25
#define GHOST_HEIGHT 3
#define TOLERANCE 5

enum {
  SIGNAL_CHILD_ATTACHED,
  SIGNAL_CHILD_DETACHED,
  SIGNAL_LAST
};

/* The algorithm for docking and redocking implemented here
 * has a couple of nice properties:
 *
 * 1) During a single drag, docking always occurs at the
 *    the same cursor position. This means that the users
 *    motions are reversible, and that you won't
 *    undock/dock oscillations.
 *
 * 2) Docking generally occurs at user-visible features.
 *    The user, once they figure out to redock, will
 *    have useful information about doing it again in
 *    the future.
 *
 * Please try to preserve these properties if you
 * change the algorithm. (And the current algorithm
 * is far from ideal). Briefly, the current algorithm
 * for deciding whether the handlebox is docked or not:
 *
 * 1) The decision is done by comparing two rectangles - the
 *    allocation if the widget at the start of the drag,
 *    and the boundary of hb->bin_window at the start of
 *    of the drag offset by the distance that the cursor
 *    has moved.
 *
 * 2) These rectangles must have one edge, the "snap_edge"
 *    of the handlebox, aligned within TOLERANCE.
 * 
 * 3) On the other dimension, the extents of one rectangle
 *    must be contained in the extents of the other,
 *    extended by tolerance. That is, either we can have:
 *
 * <-TOLERANCE-|--------bin_window--------------|-TOLERANCE->
 *         <--------float_window-------------------->
 *
 * or we can have:
 *
 * <-TOLERANCE-|------float_window--------------|-TOLERANCE->
 *          <--------bin_window-------------------->
 */

static void gtk_handle_box_class_init     (GtkHandleBoxClass *klass);
static void gtk_handle_box_init           (GtkHandleBox      *handle_box);
static void gtk_handle_box_set_arg        (GtkObject         *object,
					   GtkArg            *arg,
					   guint              arg_id);
static void gtk_handle_box_get_arg        (GtkObject         *object,
					   GtkArg            *arg,
					   guint              arg_id);
static void gtk_handle_box_destroy        (GtkObject         *object);
static void gtk_handle_box_map            (GtkWidget         *widget);
static void gtk_handle_box_unmap          (GtkWidget         *widget);
static void gtk_handle_box_realize        (GtkWidget         *widget);
static void gtk_handle_box_unrealize      (GtkWidget         *widget);
static void gtk_handle_box_style_set      (GtkWidget         *widget,
					   GtkStyle          *previous_style);
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
static void gtk_handle_box_reattach       (GtkHandleBox      *hb);
static void gtk_handle_box_end_drag       (GtkHandleBox      *hb,
					   guint32            time);

static GtkBinClass *parent_class;
static guint        handle_box_signals[SIGNAL_LAST] = { 0 };


GtkType
gtk_handle_box_get_type (void)
{
  static GtkType handle_box_type = 0;

  if (!handle_box_type)
    {
      static const GtkTypeInfo handle_box_info =
      {
	"GtkHandleBox",
	sizeof (GtkHandleBox),
	sizeof (GtkHandleBoxClass),
	(GtkClassInitFunc) gtk_handle_box_class_init,
	(GtkObjectInitFunc) gtk_handle_box_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      handle_box_type = gtk_type_unique (GTK_TYPE_BIN, &handle_box_info);
    }

  return handle_box_type;
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

  parent_class = gtk_type_class (GTK_TYPE_BIN);
  
  gtk_object_add_arg_type ("GtkHandleBox::shadow", GTK_TYPE_SHADOW_TYPE, GTK_ARG_READWRITE, ARG_SHADOW);
  gtk_object_add_arg_type ("GtkHandleBox::handle_position", GTK_TYPE_POSITION_TYPE, GTK_ARG_READWRITE, ARG_HANDLE_POSITION);
  gtk_object_add_arg_type ("GtkHandleBox::snap_edge", GTK_TYPE_POSITION_TYPE, GTK_ARG_READWRITE, ARG_SNAP_EDGE);

  object_class->set_arg = gtk_handle_box_set_arg;
  object_class->get_arg = gtk_handle_box_get_arg;
  
  handle_box_signals[SIGNAL_CHILD_ATTACHED] =
    gtk_signal_new ("child_attached",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHandleBoxClass, child_attached),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_WIDGET);
  handle_box_signals[SIGNAL_CHILD_DETACHED] =
    gtk_signal_new ("child_detached",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHandleBoxClass, child_detached),
		    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_WIDGET);
  gtk_object_class_add_signals (object_class, handle_box_signals, SIGNAL_LAST);
  
  object_class->destroy = gtk_handle_box_destroy;

  widget_class->map = gtk_handle_box_map;
  widget_class->unmap = gtk_handle_box_unmap;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->unrealize = gtk_handle_box_unrealize;
  widget_class->style_set = gtk_handle_box_style_set;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->draw = gtk_handle_box_draw;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_changed;
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

  handle_box->bin_window = NULL;
  handle_box->float_window = NULL;
  handle_box->shadow_type = GTK_SHADOW_OUT;
  handle_box->handle_position = GTK_POS_LEFT;
  handle_box->float_window_mapped = FALSE;
  handle_box->child_detached = FALSE;
  handle_box->in_drag = FALSE;
  handle_box->shrink_on_detach = TRUE;
  handle_box->snap_edge = -1;
}

static void
gtk_handle_box_set_arg (GtkObject *object,
			GtkArg    *arg,
			guint      arg_id)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);

  switch (arg_id)
    {
    case ARG_SHADOW:
      gtk_handle_box_set_shadow_type (handle_box, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_HANDLE_POSITION:
      gtk_handle_box_set_handle_position (handle_box, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_SNAP_EDGE:
      gtk_handle_box_set_snap_edge (handle_box, GTK_VALUE_ENUM (*arg));
      break;
    }
}

static void
gtk_handle_box_get_arg (GtkObject *object,
			GtkArg    *arg,
			guint      arg_id)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);
  
  switch (arg_id)
    {
    case ARG_SHADOW:
      GTK_VALUE_ENUM (*arg) = handle_box->shadow_type;
      break;
    case ARG_HANDLE_POSITION:
      GTK_VALUE_ENUM (*arg) = handle_box->handle_position;
      break;
    case ARG_SNAP_EDGE:
      GTK_VALUE_ENUM (*arg) = handle_box->snap_edge;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
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

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_map (bin->child);

  if (hb->child_detached && !hb->float_window_mapped)
    {
      gdk_window_show (hb->float_window);
      hb->float_window_mapped = TRUE;
    }

  gdk_window_show (hb->bin_window);
  gdk_window_show (widget->window);
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
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
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

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_handle_box_style_set (GtkWidget *widget,
			  GtkStyle  *previous_style)
{
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  hb = GTK_HANDLE_BOX (widget);

  if (GTK_WIDGET_REALIZED (widget) &&
      !GTK_WIDGET_NO_WINDOW (widget))
    {
      gtk_style_set_background (widget->style, widget->window,
widget->state);
      gtk_style_set_background (widget->style, hb->bin_window, widget->state);
      gtk_style_set_background (widget->style, hb->float_window, widget->state);
    }
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GtkRequisition child_requisition;

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

  /* if our child is not visible, we still request its size, since we
   * won't have any usefull hint for our size otherwise.
   */
  if (bin->child)
    gtk_widget_size_request (bin->child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      

  if (hb->child_detached)
    {
      /* FIXME: This doesn't work currently */
      if (!hb->shrink_on_detach)
	{
	  if (hb->handle_position == GTK_POS_LEFT ||
	      hb->handle_position == GTK_POS_RIGHT)
	    requisition->height += child_requisition.height;
	  else
	    requisition->width += child_requisition.width;
	}
      else
	{
	  if (hb->handle_position == GTK_POS_LEFT ||
	      hb->handle_position == GTK_POS_RIGHT)
	    requisition->height += widget->style->klass->ythickness;
	  else
	    requisition->width += widget->style->klass->xthickness;
	}
    }
  else
    {
      requisition->width += GTK_CONTAINER (widget)->border_width * 2;
      requisition->height += GTK_CONTAINER (widget)->border_width * 2;
      
      if (bin->child)
	{
	  requisition->width += child_requisition.width;
	  requisition->height += child_requisition.height;
	}
      else
	{
	  requisition->width += CHILDLESS_SIZE;
	  requisition->height += CHILDLESS_SIZE;
	}
    }
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GtkRequisition child_requisition;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (allocation != NULL);
  
  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);
  
  if (bin->child)
    gtk_widget_get_child_requisition (bin->child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      
      
  widget->allocation = *allocation;

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
	  
	  child_allocation.width = child_requisition.width;
	  child_allocation.height = child_requisition.height;
	  
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
	  child_allocation.width = MAX (1, (gint)widget->allocation.width - 2 * border_width);
	  child_allocation.height = MAX (1, (gint)widget->allocation.height - 2 * border_width);

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
      x = hb->handle_position == GTK_POS_LEFT ? 0 : widget->allocation.width - DRAG_HANDLE_SIZE;
      y = 0;
      width = DRAG_HANDLE_SIZE;
      height = widget->allocation.height;
    }
  else
    {
      x = 0;
      y = hb->handle_position == GTK_POS_TOP ? 0 : widget->allocation.height - DRAG_HANDLE_SIZE;
      width = widget->allocation.width;
      height = DRAG_HANDLE_SIZE;
    }
  gtk_paint_shadow (widget->style,
		    widget->window,
		    GTK_WIDGET_STATE (widget),
		    GTK_SHADOW_ETCHED_IN,
		    NULL, widget, "handle",
		    x,
		    y,
		    width,
		    height);
   if (hb->handle_position == GTK_POS_LEFT ||
       hb->handle_position == GTK_POS_RIGHT)
     gtk_paint_hline (widget->style,
		      widget->window,
		      GTK_WIDGET_STATE (widget),
		      NULL, widget, "handlebox",
		      hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		      hb->handle_position == GTK_POS_LEFT ? widget->allocation.width : widget->allocation.width - DRAG_HANDLE_SIZE,
		      widget->allocation.height / 2);
   else
     gtk_paint_vline (widget->style,
		      widget->window,
		      GTK_WIDGET_STATE (widget),
		      NULL, widget, "handlebox",
		      hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		      hb->handle_position == GTK_POS_TOP ? widget->allocation.height : widget->allocation.height - DRAG_HANDLE_SIZE,
		      widget->allocation.width / 2);
}

static void
draw_textured_frame (GtkWidget *widget, GdkWindow *window, GdkRectangle *rect, GtkShadowType shadow,
		     GdkRectangle *clip)
{
   gtk_paint_handle(widget->style, window, GTK_STATE_NORMAL, shadow,
		    clip, widget, "handlebox",
		    rect->x, rect->y, rect->width, rect->height, 
		    GTK_ORIENTATION_VERTICAL);
}

void
gtk_handle_box_set_shadow_type (GtkHandleBox  *handle_box,
				GtkShadowType  type)
{
  g_return_if_fail (handle_box != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if ((GtkShadowType) handle_box->shadow_type != type)
    {
      handle_box->shadow_type = type;
      gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

void        
gtk_handle_box_set_handle_position  (GtkHandleBox    *handle_box,
				     GtkPositionType  position)
{
  if ((GtkPositionType) handle_box->handle_position != position)
    {
      handle_box->handle_position = position;
      gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

void        
gtk_handle_box_set_snap_edge        (GtkHandleBox    *handle_box,
				     GtkPositionType  edge)
{
  g_return_if_fail (handle_box != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  handle_box->snap_edge = edge;
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
  GdkRectangle rect;
  GdkRectangle dest;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  gdk_window_get_size (hb->bin_window, &width, &height);
  
  if (!event)
   gtk_paint_box(widget->style,
		 hb->bin_window,
		 GTK_WIDGET_STATE (widget),
		 hb->shadow_type,
		 area, widget, "handlebox_bin",
		 0, 0, -1, -1);
  else
   gtk_paint_box(widget->style,
		 hb->bin_window,
		 GTK_WIDGET_STATE (widget),
		 hb->shadow_type,
		 &event->area, widget, "handlebox_bin",
		 0, 0, -1, -1);

/* We currently draw the handle _above_ the relief of the handlebox.
 * it could also be drawn on the same level...

		 hb->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		 hb->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		 width,
		 height);*/

  switch (hb->handle_position)
    {
    case GTK_POS_LEFT:
      rect.x = 0;
      rect.y = 0; 
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      break;
    case GTK_POS_RIGHT:
      rect.x = width - DRAG_HANDLE_SIZE; 
      rect.y = 0;
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      break;
    case GTK_POS_TOP:
      rect.x = 0;
      rect.y = 0; 
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      break;
    case GTK_POS_BOTTOM:
      rect.x = 0;
      rect.y = height - DRAG_HANDLE_SIZE;
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      break;
    }

  if (gdk_rectangle_intersect (event ? &event->area : area, &rect, &dest))
    draw_textured_frame (widget, hb->bin_window, &rect,
			 GTK_SHADOW_OUT,
			 event ? &event->area : area);

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
  GdkRectangle new_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (area != NULL);

  hb = GTK_HANDLE_BOX (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (hb->child_detached)
	{
	  gint width, height;
	  
	  /* The area parameter does not make sense in this case, so we
	   * repaint everything.
	   */

	  gtk_handle_box_draw_ghost (hb);

	  gdk_window_get_size (hb->bin_window, &width, &height);

	  new_area.x = 0;
	  new_area.y = 0;
	  new_area.width = width;
	  new_area.height = height;

	  gtk_handle_box_paint (widget, NULL, &new_area);
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

static GtkWidget *
gtk_handle_box_get_invisible (void)
{
  static GtkWidget *handle_box_invisible = NULL;

  if (!handle_box_invisible)
    {
      handle_box_invisible = gtk_invisible_new ();
      gtk_widget_show (handle_box_invisible);
    }
  
  return handle_box_invisible;
}

static gboolean
window_is_viewable (GdkWindow *window)
{
  GdkWindowPrivate *window_private = (GdkWindowPrivate *)window;
  XWindowAttributes xwa;

  while (window_private->parent)
    {
      GdkWindowPrivate *parent_private = (GdkWindowPrivate *)window_private->parent;

      if (parent_private->window_type == GDK_WINDOW_ROOT ||
	  parent_private->window_type == GDK_WINDOW_FOREIGN)
	break;

      window_private = parent_private;
    }

  XGetWindowAttributes (window_private->xdisplay, window_private->xwindow, &xwa);

  return xwa.map_state == IsViewable;
}

static gboolean
gtk_handle_box_grab_event (GtkWidget    *widget,
			   GdkEvent     *event,
			   GtkHandleBox *hb)
{
  switch (event->type)
    {
    case GDK_BUTTON_RELEASE:
      if (hb->in_drag)		/* sanity check */
	{
	  gtk_handle_box_end_drag (hb, event->button.time);
	  return TRUE;
	}
      break;

    case GDK_MOTION_NOTIFY:
      return gtk_handle_box_motion (GTK_WIDGET (hb), (GdkEventMotion *)event);
      break;

    default:
      break;
    }

  return FALSE;
}

static gint
gtk_handle_box_button_changed (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GtkHandleBox *hb;
  gboolean event_handled;
  GdkCursor *fleur;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  event_handled = FALSE;
  if ((event->button == 1) && 
      (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS))
    {
      GtkWidget *child;
      gboolean in_handle;
      
      if (event->window != hb->bin_window)
	return FALSE;

      child = GTK_BIN (hb)->child;

      if (child)
	{
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
	}
      else
	{
	  in_handle = FALSE;
	  event_handled = TRUE;
	}
      
      if (in_handle)
	{
	  if (event->type == GDK_BUTTON_PRESS) /* Start a drag */
	    {
	      GtkWidget *invisible = gtk_handle_box_get_invisible ();
		
	      gint desk_x, desk_y;
	      gint root_x, root_y;
	      gint width, height;
	      
	      gdk_window_get_deskrelative_origin (hb->bin_window, &desk_x, &desk_y);
	      gdk_window_get_origin (hb->bin_window, &root_x, &root_y);
	      gdk_window_get_size (hb->bin_window, &width, &height);
	      
	      hb->float_allocation.x = root_x - event->x_root;
	      hb->float_allocation.y = root_y - event->y_root;
	      hb->float_allocation.width = width;
	      hb->float_allocation.height = height;
	      
	      hb->deskoff_x = desk_x - root_x;
	      hb->deskoff_y = desk_y - root_y;

	      if (window_is_viewable (widget->window))
		{
		  gdk_window_get_origin (widget->window, &root_x, &root_y);
		  gdk_window_get_size (widget->window, &width, &height);

		  hb->attach_allocation.x = root_x;
		  hb->attach_allocation.y = root_y;
		  hb->attach_allocation.width = width;
		  hb->attach_allocation.height = height;
		}
	      else
		{
		  hb->attach_allocation.x = -1;
		  hb->attach_allocation.y = -1;
		  hb->attach_allocation.width = 0;
		  hb->attach_allocation.height = 0;
		}

	      hb->in_drag = TRUE;
	      fleur = gdk_cursor_new (GDK_FLEUR);
	      if (gdk_pointer_grab (invisible->window,
				    FALSE,
				    (GDK_BUTTON1_MOTION_MASK |
				     GDK_POINTER_MOTION_HINT_MASK |
				     GDK_BUTTON_RELEASE_MASK),
				    NULL,
				    fleur,
				    event->time) != 0)
		{
		  hb->in_drag = FALSE;
		}
	      else
		gtk_signal_connect (GTK_OBJECT (invisible), "event",
				    GTK_SIGNAL_FUNC (gtk_handle_box_grab_event), hb);
	      
	      gdk_cursor_destroy (fleur);
	      event_handled = TRUE;
	    }
	  else if (hb->child_detached) /* Double click */
	    {
	      gtk_handle_box_reattach (hb);
	    }
	}
    }
  
  return event_handled;
}

static gint
gtk_handle_box_motion (GtkWidget      *widget,
		       GdkEventMotion *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  gint new_x, new_y;
  gint snap_edge;
  gboolean is_snapped = FALSE;

  if (!hb->in_drag)
    return FALSE;
  
  /* Calculate the attachment point on the float, if the float
   * were detached
   */
  new_x = 0;
  new_y = 0;
  gdk_window_get_pointer (GDK_ROOT_PARENT(), &new_x, &new_y, NULL);
  new_x += hb->float_allocation.x;
  new_y += hb->float_allocation.y;

  snap_edge = hb->snap_edge;
  if (snap_edge == -1)
    snap_edge = (hb->handle_position == GTK_POS_LEFT ||
		  hb->handle_position == GTK_POS_RIGHT) ?
      GTK_POS_TOP : GTK_POS_LEFT;

  /* First, check if the snapped edge is aligned
   */
  switch (snap_edge)
    {
    case GTK_POS_TOP:
      is_snapped = abs (hb->attach_allocation.y - new_y) < TOLERANCE;
      break;
    case GTK_POS_BOTTOM:
      is_snapped = abs (hb->attach_allocation.y + (gint)hb->attach_allocation.height -
			new_y - (gint)hb->float_allocation.height) < TOLERANCE;
      break;
    case GTK_POS_LEFT:
      is_snapped = abs (hb->attach_allocation.x - new_x) < TOLERANCE;
      break;
    case GTK_POS_RIGHT:
      is_snapped = abs (hb->attach_allocation.x + (gint)hb->attach_allocation.width -
			new_x - (gint)hb->float_allocation.width) < TOLERANCE;
      break;
    }

  /* Next, check if coordinates in the other direction are sufficiently
   * aligned
   */
  if (is_snapped)
    {
      gint float_pos1 = 0;	/* Initialize to suppress warnings */
      gint float_pos2 = 0;
      gint attach_pos1 = 0;
      gint attach_pos2 = 0;
      
      switch (snap_edge)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  attach_pos1 = hb->attach_allocation.x;
	  attach_pos2 = hb->attach_allocation.x + hb->attach_allocation.width;
	  float_pos1 = new_x;
	  float_pos2 = new_x + hb->float_allocation.width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  attach_pos1 = hb->attach_allocation.y;
	  attach_pos2 = hb->attach_allocation.y + hb->attach_allocation.height;
	  float_pos1 = new_y;
	  float_pos2 = new_y + hb->float_allocation.height;
	  break;
	}

      is_snapped = ((attach_pos1 - TOLERANCE < float_pos1) && 
		    (attach_pos2 + TOLERANCE > float_pos2)) ||
	           ((float_pos1 - TOLERANCE < attach_pos1) &&
		    (float_pos2 + TOLERANCE > attach_pos2));
    }

  if (is_snapped)
    {
      if (hb->child_detached)
	{
	  hb->child_detached = FALSE;
	  gdk_window_hide (hb->float_window);
	  gdk_window_reparent (hb->bin_window, widget->window, 0, 0);
	  hb->float_window_mapped = FALSE;
	  gtk_signal_emit (GTK_OBJECT (hb),
			   handle_box_signals[SIGNAL_CHILD_ATTACHED],
			   GTK_BIN (hb)->child);
	  
	  gtk_widget_queue_resize (widget);
	}
    }
  else
    {
      gint width, height;

      gdk_window_get_size (hb->float_window, &width, &height);
      new_x += hb->deskoff_x;
      new_y += hb->deskoff_y;

      switch (hb->handle_position)
	{
	case GTK_POS_LEFT:
	  new_y += ((gint)hb->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_RIGHT:
	  new_x += (gint)hb->float_allocation.width - width;
	  new_y += ((gint)hb->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_TOP:
	  new_x += ((gint)hb->float_allocation.width - width) / 2;
	  break;
	case GTK_POS_BOTTOM:
	  new_x += ((gint)hb->float_allocation.width - width) / 2;
	  new_y += (gint)hb->float_allocation.height - height;
	  break;
	}

      if (hb->child_detached)
	{
	  gdk_window_move (hb->float_window, new_x, new_y);
	  gdk_window_raise (hb->float_window);
	}
      else
	{
	  gint width;
	  gint height;
	  GtkRequisition child_requisition;

	  hb->child_detached = TRUE;

	  if (GTK_BIN (hb)->child)
	    gtk_widget_get_child_requisition (GTK_BIN (hb)->child, &child_requisition);
	  else
	    {
	      child_requisition.width = 0;
	      child_requisition.height = 0;
	    }      

	  width = child_requisition.width + 2 * GTK_CONTAINER (hb)->border_width;
	  height = child_requisition.height + 2 * GTK_CONTAINER (hb)->border_width;
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
  g_return_if_fail (GTK_IS_HANDLE_BOX (container));
  g_return_if_fail (GTK_BIN (container)->child == widget);

  GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);

  gtk_handle_box_reattach (GTK_HANDLE_BOX (container));
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

  if (event->window == hb->float_window)
    {
      gtk_handle_box_reattach (hb);
      
      return TRUE;
    }

  return FALSE;
}

static void
gtk_handle_box_reattach (GtkHandleBox *hb)
{
  if (hb->child_detached)
    {
      hb->child_detached = FALSE;
      if (GTK_WIDGET_REALIZED (hb))
	{
	  gdk_window_hide (hb->float_window);
	  gdk_window_reparent (hb->bin_window, GTK_WIDGET (hb)->window, 0, 0);

	  if (GTK_BIN (hb)->child)
	    gtk_signal_emit (GTK_OBJECT (hb),
			     handle_box_signals[SIGNAL_CHILD_ATTACHED],
			     GTK_BIN (hb)->child);

	}
      hb->float_window_mapped = FALSE;
    }
  
  if (hb->in_drag)
    gtk_handle_box_end_drag (hb, GDK_CURRENT_TIME);

  gtk_widget_queue_resize (GTK_WIDGET (hb));
}

static void
gtk_handle_box_end_drag (GtkHandleBox *hb,
			 guint32       time)
{
  GtkWidget *invisible = gtk_handle_box_get_invisible ();
		
  hb->in_drag = FALSE;

  gdk_pointer_ungrab (time);
  gtk_signal_disconnect_by_func (GTK_OBJECT (invisible),
				 GTK_SIGNAL_FUNC (gtk_handle_box_grab_event),
				 hb);
}
