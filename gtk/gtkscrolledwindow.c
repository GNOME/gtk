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
#include "gtkscrolledwindow.h"
#include "gtksignal.h"


#define SCROLLBAR_SPACING(w) (GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT (w)->klass)->scrollbar_spacing)


static void gtk_scrolled_window_class_init         (GtkScrolledWindowClass *klass);
static void gtk_scrolled_window_init               (GtkScrolledWindow      *scrolled_window);
static void gtk_scrolled_window_destroy            (GtkObject              *object);
static void gtk_scrolled_window_finalize           (GtkObject              *object);
static void gtk_scrolled_window_map                (GtkWidget              *widget);
static void gtk_scrolled_window_unmap              (GtkWidget              *widget);
static void gtk_scrolled_window_draw               (GtkWidget              *widget,
						    GdkRectangle           *area);
static void gtk_scrolled_window_size_request       (GtkWidget              *widget,
						    GtkRequisition         *requisition);
static void gtk_scrolled_window_size_allocate      (GtkWidget              *widget,
						    GtkAllocation          *allocation);
static void gtk_scrolled_window_add                (GtkContainer           *container,
						    GtkWidget              *widget);
static void gtk_scrolled_window_remove             (GtkContainer           *container,
						    GtkWidget              *widget);
static void gtk_scrolled_window_foreach            (GtkContainer           *container,
						    GtkCallback             callback,
						    gpointer                callback_data);
static void gtk_scrolled_window_viewport_allocate  (GtkWidget              *widget,
						    GtkAllocation          *allocation);
static void gtk_scrolled_window_adjustment_changed (GtkAdjustment          *adjustment,
						    gpointer                data);


static GtkContainerClass *parent_class = NULL;


guint
gtk_scrolled_window_get_type ()
{
  static guint scrolled_window_type = 0;

  if (!scrolled_window_type)
    {
      GtkTypeInfo scrolled_window_info =
      {
	"GtkScrolledWindow",
	sizeof (GtkScrolledWindow),
	sizeof (GtkScrolledWindowClass),
	(GtkClassInitFunc) gtk_scrolled_window_class_init,
	(GtkObjectInitFunc) gtk_scrolled_window_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      scrolled_window_type = gtk_type_unique (gtk_container_get_type (), &scrolled_window_info);
    }

  return scrolled_window_type;
}

static void
gtk_scrolled_window_class_init (GtkScrolledWindowClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  object_class->destroy = gtk_scrolled_window_destroy;
  object_class->finalize = gtk_scrolled_window_finalize;

  widget_class->map = gtk_scrolled_window_map;
  widget_class->unmap = gtk_scrolled_window_unmap;
  widget_class->draw = gtk_scrolled_window_draw;
  widget_class->size_request = gtk_scrolled_window_size_request;
  widget_class->size_allocate = gtk_scrolled_window_size_allocate;

  container_class->add = gtk_scrolled_window_add;
  container_class->remove = gtk_scrolled_window_remove;
  container_class->foreach = gtk_scrolled_window_foreach;

  class->scrollbar_spacing = 5;
}

static void
gtk_scrolled_window_init (GtkScrolledWindow *scrolled_window)
{
  GTK_WIDGET_SET_FLAGS (scrolled_window, GTK_NO_WINDOW);

  scrolled_window->hscrollbar = NULL;
  scrolled_window->vscrollbar = NULL;
  scrolled_window->hscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->vscrollbar_policy = GTK_POLICY_ALWAYS;
}

GtkWidget*
gtk_scrolled_window_new (GtkAdjustment *hadjustment,
			 GtkAdjustment *vadjustment)
{
  GtkWidget *scrolled_window;

  scrolled_window = gtk_type_new (gtk_scrolled_window_get_type ());

  gtk_scrolled_window_construct (GTK_SCROLLED_WINDOW (scrolled_window), hadjustment, vadjustment);
  
  return scrolled_window;
}

void
gtk_scrolled_window_construct (GtkScrolledWindow *scrolled_window,
			       GtkAdjustment     *hadjustment,
			       GtkAdjustment     *vadjustment)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (scrolled_window->viewport == NULL);

  scrolled_window->viewport = gtk_viewport_new (hadjustment, vadjustment);
  hadjustment = gtk_viewport_get_hadjustment (GTK_VIEWPORT (scrolled_window->viewport));
  vadjustment = gtk_viewport_get_vadjustment (GTK_VIEWPORT (scrolled_window->viewport));

  gtk_signal_connect (GTK_OBJECT (hadjustment), "changed",
		      (GtkSignalFunc) gtk_scrolled_window_adjustment_changed,
		      (gpointer) scrolled_window);
  gtk_signal_connect (GTK_OBJECT (vadjustment), "changed",
		      (GtkSignalFunc) gtk_scrolled_window_adjustment_changed,
		      (gpointer) scrolled_window);

  scrolled_window->hscrollbar = gtk_hscrollbar_new (hadjustment);
  scrolled_window->vscrollbar = gtk_vscrollbar_new (vadjustment);

  gtk_widget_set_parent (scrolled_window->viewport, GTK_WIDGET (scrolled_window));
  gtk_widget_set_parent (scrolled_window->hscrollbar, GTK_WIDGET (scrolled_window));
  gtk_widget_set_parent (scrolled_window->vscrollbar, GTK_WIDGET (scrolled_window));

  gtk_widget_show (scrolled_window->viewport);
  gtk_widget_show (scrolled_window->hscrollbar);
  gtk_widget_show (scrolled_window->vscrollbar);
  
  gtk_widget_ref (scrolled_window->viewport);
  gtk_widget_ref (scrolled_window->hscrollbar);
  gtk_widget_ref (scrolled_window->vscrollbar);
}

GtkAdjustment*
gtk_scrolled_window_get_hadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (scrolled_window != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
}

GtkAdjustment*
gtk_scrolled_window_get_vadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (scrolled_window != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
}

void
gtk_scrolled_window_set_policy (GtkScrolledWindow *scrolled_window,
				GtkPolicyType      hscrollbar_policy,
				GtkPolicyType      vscrollbar_policy)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if ((scrolled_window->hscrollbar_policy != hscrollbar_policy) ||
      (scrolled_window->vscrollbar_policy != vscrollbar_policy))
    {
      scrolled_window->hscrollbar_policy = hscrollbar_policy;
      scrolled_window->vscrollbar_policy = vscrollbar_policy;

      if (GTK_WIDGET (scrolled_window)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}


static void
gtk_scrolled_window_destroy (GtkObject *object)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (object));

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  gtk_widget_destroy (scrolled_window->viewport);
  gtk_widget_destroy (scrolled_window->hscrollbar);
  gtk_widget_destroy (scrolled_window->vscrollbar);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_scrolled_window_finalize (GtkObject *object)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (object);
  gtk_widget_unref (scrolled_window->viewport);
  gtk_widget_unref (scrolled_window->hscrollbar);
  gtk_widget_unref (scrolled_window->vscrollbar);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_scrolled_window_map (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));

  if (!GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
      scrolled_window = GTK_SCROLLED_WINDOW (widget);

      if (GTK_WIDGET_VISIBLE (scrolled_window->viewport) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->viewport))
	gtk_widget_map (scrolled_window->viewport);

      if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->hscrollbar))
	gtk_widget_map (scrolled_window->hscrollbar);

      if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->vscrollbar))
	gtk_widget_map (scrolled_window->vscrollbar);
    }
}

static void
gtk_scrolled_window_unmap (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));

  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
      scrolled_window = GTK_SCROLLED_WINDOW (widget);

      if (GTK_WIDGET_MAPPED (scrolled_window->viewport))
	gtk_widget_unmap (scrolled_window->viewport);

      if (GTK_WIDGET_MAPPED (scrolled_window->hscrollbar))
	gtk_widget_unmap (scrolled_window->hscrollbar);

      if (GTK_WIDGET_MAPPED (scrolled_window->vscrollbar))
	gtk_widget_unmap (scrolled_window->vscrollbar);
    }
}

static void
gtk_scrolled_window_draw (GtkWidget    *widget,
			  GdkRectangle *area)
{
  GtkScrolledWindow *scrolled_window;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      scrolled_window = GTK_SCROLLED_WINDOW (widget);

      if (gtk_widget_intersect (scrolled_window->viewport, area, &child_area))
	gtk_widget_draw (scrolled_window->viewport, &child_area);

      if (gtk_widget_intersect (scrolled_window->hscrollbar, area, &child_area))
	gtk_widget_draw (scrolled_window->hscrollbar, &child_area);

      if (gtk_widget_intersect (scrolled_window->vscrollbar, area, &child_area))
	gtk_widget_draw (scrolled_window->vscrollbar, &child_area);
    }
}

static void
gtk_scrolled_window_size_request (GtkWidget      *widget,
				  GtkRequisition *requisition)
{
  GtkScrolledWindow *scrolled_window;
  gint extra_height;
  gint extra_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);

  requisition->width = 0;
  requisition->height = 0;

  if (GTK_WIDGET_VISIBLE (scrolled_window->viewport))
    {
      gtk_widget_size_request (scrolled_window->viewport, &scrolled_window->viewport->requisition);

      requisition->width += scrolled_window->viewport->requisition.width;
      requisition->height += scrolled_window->viewport->requisition.height;
    }

  extra_width = 0;
  extra_height = 0;

  if ((scrolled_window->hscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    {
      gtk_widget_size_request (scrolled_window->hscrollbar,
			       &scrolled_window->hscrollbar->requisition);

      requisition->width = MAX (requisition->width, scrolled_window->hscrollbar->requisition.width);
      extra_height = SCROLLBAR_SPACING (scrolled_window) + scrolled_window->hscrollbar->requisition.height;
    }

  if ((scrolled_window->vscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    {
      gtk_widget_size_request (scrolled_window->vscrollbar,
			       &scrolled_window->vscrollbar->requisition);

      requisition->height = MAX (requisition->height, scrolled_window->vscrollbar->requisition.height);
      extra_width = SCROLLBAR_SPACING (scrolled_window) + scrolled_window->vscrollbar->requisition.width;
    }

  requisition->width += GTK_CONTAINER (widget)->border_width * 2 + extra_width;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2 + extra_height;
}

static void
gtk_scrolled_window_size_allocate (GtkWidget     *widget,
				   GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;
  GtkAllocation viewport_allocation;
  GtkAllocation child_allocation;
  guint previous_hvis;
  guint previous_vvis;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  widget->allocation = *allocation;

  gtk_scrolled_window_viewport_allocate (widget, &viewport_allocation);

  gtk_container_disable_resize (GTK_CONTAINER (scrolled_window));

  if (GTK_WIDGET_VISIBLE (scrolled_window->viewport))
    {
      do {
	gtk_scrolled_window_viewport_allocate (widget, &viewport_allocation);

	child_allocation.x = viewport_allocation.x + allocation->x;
	child_allocation.y = viewport_allocation.y + allocation->y;
	child_allocation.width = viewport_allocation.width;
	child_allocation.height = viewport_allocation.height;

	previous_hvis = GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar);
	previous_vvis = GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar);

	gtk_widget_size_allocate (scrolled_window->viewport, &child_allocation);
      } while ((previous_hvis != GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar)) ||
	       (previous_vvis != GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar)));
    }

  if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    {
      child_allocation.x = viewport_allocation.x;
      child_allocation.y = viewport_allocation.y + viewport_allocation.height + SCROLLBAR_SPACING (scrolled_window);
      child_allocation.width = viewport_allocation.width;
      child_allocation.height = scrolled_window->hscrollbar->requisition.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (scrolled_window->hscrollbar, &child_allocation);
    }

  if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    {
      child_allocation.x = viewport_allocation.x + viewport_allocation.width + SCROLLBAR_SPACING (scrolled_window);
      child_allocation.y = viewport_allocation.y;
      child_allocation.width = scrolled_window->vscrollbar->requisition.width;
      child_allocation.height = viewport_allocation.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (scrolled_window->vscrollbar, &child_allocation);
    }

  gtk_container_enable_resize (GTK_CONTAINER (scrolled_window));
}

static void
gtk_scrolled_window_add (GtkContainer *container,
			 GtkWidget    *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (widget != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);
  gtk_container_add (GTK_CONTAINER (scrolled_window->viewport), widget);
}

static void
gtk_scrolled_window_remove (GtkContainer *container,
			    GtkWidget    *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (widget != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);

  if (scrolled_window->viewport == widget ||
      scrolled_window->hscrollbar == widget ||
      scrolled_window->vscrollbar == widget)
    {
      /* this happens during destroy */
      gtk_widget_unparent (widget);
    }
  else
    gtk_container_remove (GTK_CONTAINER (scrolled_window->viewport), widget);
}

static void
gtk_scrolled_window_foreach (GtkContainer *container,
			     GtkCallback   callback,
			     gpointer      callback_data)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (callback != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);

  (* callback) (scrolled_window->viewport, callback_data);
}

static void
gtk_scrolled_window_viewport_allocate (GtkWidget     *widget,
				       GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);

  allocation->x = GTK_CONTAINER (widget)->border_width;
  allocation->y = GTK_CONTAINER (widget)->border_width;
  allocation->width = MAX (1, widget->allocation.width - allocation->x * 2);
  allocation->height = MAX (1, widget->allocation.height - allocation->y * 2);

  if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    allocation->width = MAX (1,
      allocation->width - (scrolled_window->vscrollbar->requisition.width + SCROLLBAR_SPACING (scrolled_window)));
  if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    allocation->height = MAX (1, 
      allocation->height - (scrolled_window->hscrollbar->requisition.height + SCROLLBAR_SPACING (scrolled_window)));
}

static void
gtk_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
					gpointer       data)
{
  GtkScrolledWindow *scrolled_win;
  GtkWidget *scrollbar;
  gint hide_scrollbar;
  gint policy;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  scrolled_win = GTK_SCROLLED_WINDOW (data);

  if (adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->hscrollbar)))
    {
      scrollbar = scrolled_win->hscrollbar;
      policy = scrolled_win->hscrollbar_policy;
    }
  else if (adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->vscrollbar)))
    {
      scrollbar = scrolled_win->vscrollbar;
      policy = scrolled_win->vscrollbar_policy;
    }
  else
    {
      g_warning ("could not determine which adjustment scrollbar received change signal for");
      return;
    }

  if (policy == GTK_POLICY_AUTOMATIC)
    {
      hide_scrollbar = FALSE;

      if ((adjustment->upper - adjustment->lower) <= adjustment->page_size)
	hide_scrollbar = TRUE;

      if (hide_scrollbar)
	{
	  if (GTK_WIDGET_VISIBLE (scrollbar))
	    gtk_widget_hide (scrollbar);
	}
      else
	{
	  if (!GTK_WIDGET_VISIBLE (scrollbar))
	    gtk_widget_show (scrollbar);
	}
    }
}
