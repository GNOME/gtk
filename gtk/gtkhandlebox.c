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


#define DRAG_HANDLE_SIZE 10
#define BORDER_SIZE 5
#define GHOST_HEIGHT 3
#define SNAP_TOLERANCE 10


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
					   GtkAllocation     *allocation);
static void gtk_handle_box_draw_ghost     (GtkWidget         *widget);
static void gtk_handle_box_paint          (GtkWidget         *widget,
					   GdkEventExpose    *event,
					   GdkRectangle      *area);
static void gtk_handle_box_draw           (GtkWidget         *widget,
					   GdkRectangle      *area);
static gint gtk_handle_box_delete         (GtkWidget         *widget,
					   GdkEventAny       *event);
static gint gtk_handle_box_expose         (GtkWidget         *widget,
					   GdkEventExpose    *event);
static gint gtk_handle_box_button_changed (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gtk_handle_box_motion         (GtkWidget         *widget,
					   GdkEventMotion    *event);


static GtkBinClass *parent_class;


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
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GtkWidgetClass *widget_class;
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  parent_class = gtk_type_class (gtk_bin_get_type ());

  object_class->destroy = gtk_handle_box_destroy;

  widget_class->map = gtk_handle_box_map;
  widget_class->unmap = gtk_handle_box_unmap;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->unrealize = gtk_handle_box_unrealize;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->draw = gtk_handle_box_draw;
  widget_class->delete_event = gtk_handle_box_delete;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_changed;
  widget_class->button_release_event = gtk_handle_box_button_changed;
  widget_class->motion_notify_event = gtk_handle_box_motion;
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  GTK_WIDGET_UNSET_FLAGS (handle_box, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (handle_box, GTK_BASIC); /* FIXME: are we really a basic widget? */

  handle_box->steady_window = NULL;
  handle_box->is_being_dragged = FALSE;
  handle_box->is_onroot = FALSE;
  handle_box->real_parent = NULL;
  handle_box->fleur_cursor = gdk_cursor_new (GDK_FLEUR);
  handle_box->dragoff_x = 0;
  handle_box->dragoff_y = 0;
  handle_box->steady_x = 0;
  handle_box->steady_x = 0;
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

  gdk_cursor_destroy (hb->fleur_cursor);

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

  gdk_window_show (hb->steady_window);
  gdk_window_show (widget->window);

  if (bin->child
      && GTK_WIDGET_VISIBLE (bin->child)
      && !GTK_WIDGET_MAPPED (bin->child))
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
  gdk_window_hide (hb->steady_window);
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

  hb->steady_window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data (hb->steady_window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.event_mask |= (GDK_BUTTON1_MOTION_MASK
			    | GDK_BUTTON_PRESS_MASK
			    | GDK_BUTTON_RELEASE_MASK);

  widget->window = gdk_window_new (hb->steady_window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, hb->steady_window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_handle_box_unrealize (GtkWidget *widget)
{
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED | GTK_MAPPED);

  gtk_style_detach (widget->style);

  hb = GTK_HANDLE_BOX (widget);

  if (widget->window)
    gdk_window_destroy (widget->window);

  if (hb->steady_window)
    gdk_window_destroy (hb->steady_window);

  hb->steady_window = NULL;
  widget->window = NULL;
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

  requisition->width = DRAG_HANDLE_SIZE + GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;
      requisition->height += bin->child->requisition.height;
    }

  hb->real_requisition = *requisition;
  if (hb->is_onroot)
      requisition->height = GHOST_HEIGHT;
  /* Should also set requisition->width to a small value? */
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkHandleBox *hb;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  border_width = GTK_CONTAINER (widget)->border_width;

  if (GTK_WIDGET_REALIZED (widget))
    {
      if (!hb->is_onroot)
	{
	  gdk_window_move_resize (hb->steady_window,
				  allocation->x + border_width,
				  allocation->y + border_width,
				  allocation->width - border_width * 2,
				  allocation->height - border_width * 2);
	  gdk_window_move_resize (widget->window,
				  0,
				  0,
				  allocation->width - border_width * 2,
				  allocation->height - border_width * 2);
	}
      else
	{
	  gdk_window_resize (widget->window,
			     hb->real_requisition.width,
			     hb->real_requisition.height);
	  gdk_window_move_resize (hb->steady_window,
				  allocation->x + border_width,
				  allocation->y + border_width,
				  allocation->width - border_width * 2,
				  GHOST_HEIGHT);
	}
    }

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = DRAG_HANDLE_SIZE;
      child_allocation.y = 0;

      if (hb->is_onroot)
	{
	  child_allocation.width = hb->real_requisition.width - DRAG_HANDLE_SIZE;
	  child_allocation.height = hb->real_requisition.height;
	}
      else
	{
	  child_allocation.width = allocation->width - DRAG_HANDLE_SIZE - border_width * 2;
	  child_allocation.height = allocation->height - border_width * 2;
	}

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
gtk_handle_box_draw_ghost (GtkWidget *widget)
{
  gtk_draw_hline (widget->style,
		  GTK_HANDLE_BOX (widget)->steady_window,
		  GTK_WIDGET_STATE (widget),
		  0,
		  widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2,
		  0);
}

static void
gtk_handle_box_paint (GtkWidget      *widget,
		      GdkEventExpose *event,
		      GdkRectangle   *area)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GdkRectangle child_area;
  GdkEventExpose child_event;
  gint x;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  if (event != NULL)
    area = &event->area;

  for (x = 1; x < DRAG_HANDLE_SIZE; x += 3)
    gtk_draw_vline (widget->style,
		    widget->window,
		    GTK_WIDGET_STATE (widget),
		    0, hb->is_onroot ? hb->real_requisition.height : widget->allocation.height,
		    x);

  if (hb->is_onroot)
    gtk_draw_shadow (widget->style,
		     widget->window,
		     GTK_WIDGET_STATE (widget),
		     GTK_SHADOW_OUT,
		     0, 0,
		     hb->real_requisition.width,
		     hb->real_requisition.height);
  else
    gtk_draw_shadow (widget->style,
		     widget->window,
		     GTK_WIDGET_STATE (widget),
		     GTK_SHADOW_OUT,
		     0, 0,
		     widget->allocation.width,
		     widget->allocation.height);

  if (bin->child)
    {
      if (event == NULL) /* we were called from draw() */
	{
	  if (gtk_widget_intersect (bin->child, area, &child_area))
	    gtk_widget_draw (bin->child, &child_area);
	}
      else /* we were called from expose() */
	{
	  child_event = *event;
	  
	  if (GTK_WIDGET_NO_WINDOW (bin->child)
	      && gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	    gtk_widget_event (bin->child, (GdkEvent *) &child_event);
	}
    }
}

static void
gtk_handle_box_draw (GtkWidget    *widget,
		     GdkRectangle *area)
{
  GdkRectangle child_area;
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (area != NULL);

  hb = GTK_HANDLE_BOX (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (hb->is_onroot)
	{
	  /* The area parameter does not make sense in this case, so we
	   * repaint everything.
	   */

	  gtk_handle_box_draw_ghost (widget);

	  child_area.x = 0;
	  child_area.y = 0;
	  child_area.width = hb->real_requisition.width;
	  child_area.height = hb->real_requisition.height;

	  gtk_handle_box_paint (widget, NULL, &child_area);
	}
      else
	gtk_handle_box_paint (widget, NULL, area);
    }
}

static gint
gtk_handle_box_delete (GtkWidget   *widget,
		       GdkEventAny *event)
{
  GtkHandleBox *hb;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  if (event->window == widget->window)
    {
      hb->is_onroot = FALSE;
      
      gdk_window_reparent (widget->window, hb->steady_window, 0, 0);
      gtk_widget_queue_resize (widget);
    }

  return FALSE;
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

      if (event->window == hb->steady_window)
	gtk_handle_box_draw_ghost (widget);
      else if (event->window == widget->window)
	gtk_handle_box_paint (widget, event, NULL);
    }

  return FALSE;
}

static gint
gtk_handle_box_button_changed (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GtkHandleBox *hb;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  if (event->button == 1)
    {
      if ((event->type == GDK_BUTTON_PRESS) && (event->x < DRAG_HANDLE_SIZE))
	{
	  hb->dragoff_x = event->x;
	  hb->dragoff_y = event->y;

	  gdk_window_get_origin (hb->steady_window, &hb->steady_x, &hb->steady_y);

	  hb->is_being_dragged = TRUE;

	  gdk_flush();
	  gtk_grab_add (widget);
	  while (gdk_pointer_grab (widget->window,
				   FALSE,
				   GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
				   NULL,
				   hb->fleur_cursor,
				   GDK_CURRENT_TIME) != 0); /* wait for success */
	}
      else if ((event->type == GDK_BUTTON_RELEASE) && hb->is_being_dragged)
	{
	  gdk_pointer_ungrab (GDK_CURRENT_TIME);
	  gtk_grab_remove (widget);
	  hb->is_being_dragged = FALSE;
	}
    }
  return TRUE;
}

static gint
gtk_handle_box_motion (GtkWidget      *widget,
		       GdkEventMotion *event)
{
  GtkHandleBox *hb;
  gint newx, newy;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  if (hb->is_being_dragged)
    {
      newx = event->x_root - hb->dragoff_x;
      newy = event->y_root - hb->dragoff_y;

      if ((abs (hb->steady_x - newx) < SNAP_TOLERANCE)
	  && (abs (hb->steady_y - newy) < SNAP_TOLERANCE))
	{
	  if (hb->is_onroot)
	    {
	      hb->is_onroot = FALSE;

	      gdk_pointer_ungrab (GDK_CURRENT_TIME);

	      gdk_window_reparent (widget->window, hb->steady_window, 0, 0);

	      while (gdk_pointer_grab (widget->window,
				       FALSE,
				       GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
				       NULL,
				       hb->fleur_cursor,
				       GDK_CURRENT_TIME) != 0); /* wait for success */

	      gtk_widget_queue_resize (widget);
	    }
	}
      else
	{
	  if (hb->is_onroot)
	    gdk_window_move (widget->window, newx, newy);
	  else
	    {
	      hb->is_onroot = TRUE;

	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	      gdk_window_reparent (widget->window, GDK_ROOT_PARENT (), newx, newy);

	      /* We move the window explicitly because the window manager may
	       * have set the position itself, ignoring what we asked in the
	       * reparent call.
	       */
	      gdk_window_move (widget->window, newx, newy);

	      /* FIXME: are X calls Gtk-kosher? */
	      
	      XSetTransientForHint (GDK_DISPLAY(),
				    GDK_WINDOW_XWINDOW (widget->window),
				    GDK_WINDOW_XWINDOW (gtk_widget_get_toplevel (widget)->window));

	      XSetWMProtocols (GDK_DISPLAY (),
			       GDK_WINDOW_XWINDOW (widget->window),
			       &gdk_wm_delete_window,
			       1);

	      /* FIXME: we need a property that would tell the window
	       * manager not to put decoration on this window.  This
	       * is not part of the ICCCM, so we'll have to define our
	       * own (a la KWM) and hack some window managers to
	       * support it.
	       */

	      while (gdk_pointer_grab (widget->window,
				       FALSE,
				       GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
				       NULL,
				       hb->fleur_cursor,
				       GDK_CURRENT_TIME) != 0); /* wait for success */

	      gtk_widget_queue_resize (widget);
	    }
	}
    }

  return TRUE;
}
