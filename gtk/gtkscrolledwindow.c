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

enum {
  ARG_0,
  ARG_HADJUSTMENT,
  ARG_VADJUSTMENT,
  ARG_HSCROLLBAR_POLICY,
  ARG_VSCROLLBAR_POLICY,
  ARG_WINDOW_PLACEMENT
};


static void gtk_scrolled_window_class_init         (GtkScrolledWindowClass *klass);
static void gtk_scrolled_window_init               (GtkScrolledWindow      *scrolled_window);
static void gtk_scrolled_window_set_arg		   (GtkObject              *object,
						    GtkArg                 *arg,
						    guint                   arg_id);
static void gtk_scrolled_window_get_arg		   (GtkObject              *object,
						    GtkArg                 *arg,
						    guint                   arg_id);
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
static void gtk_scrolled_window_forall             (GtkContainer           *container,
						    gboolean		    include_internals,
						    GtkCallback             callback,
						    gpointer                callback_data);
static void gtk_scrolled_window_relative_allocation(GtkWidget              *widget,
						    GtkAllocation          *allocation);
static void gtk_scrolled_window_adjustment_changed (GtkAdjustment          *adjustment,
						    gpointer                data);


static GtkContainerClass *parent_class = NULL;


GtkType
gtk_scrolled_window_get_type (void)
{
  static GtkType scrolled_window_type = 0;

  if (!scrolled_window_type)
    {
      GtkTypeInfo scrolled_window_info =
      {
	"GtkScrolledWindow",
	sizeof (GtkScrolledWindow),
	sizeof (GtkScrolledWindowClass),
	(GtkClassInitFunc) gtk_scrolled_window_class_init,
	(GtkObjectInitFunc) gtk_scrolled_window_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      scrolled_window_type = gtk_type_unique (GTK_TYPE_CONTAINER, &scrolled_window_info);
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
  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  gtk_object_add_arg_type ("GtkScrolledWindow::hadjustment",
			   GTK_TYPE_ADJUSTMENT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
			   ARG_HADJUSTMENT);
  gtk_object_add_arg_type ("GtkScrolledWindow::vadjustment",
			   GTK_TYPE_ADJUSTMENT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
			   ARG_VADJUSTMENT);
  gtk_object_add_arg_type ("GtkScrolledWindow::hscrollbar_policy",
			   GTK_TYPE_POLICY_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_HSCROLLBAR_POLICY);
  gtk_object_add_arg_type ("GtkScrolledWindow::vscrollbar_policy",
			   GTK_TYPE_POLICY_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_VSCROLLBAR_POLICY);
  gtk_object_add_arg_type ("GtkScrolledWindow::window_placement",
			   GTK_TYPE_CORNER_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_WINDOW_PLACEMENT);

  object_class->set_arg = gtk_scrolled_window_set_arg;
  object_class->get_arg = gtk_scrolled_window_get_arg;
  object_class->destroy = gtk_scrolled_window_destroy;
  object_class->finalize = gtk_scrolled_window_finalize;

  widget_class->map = gtk_scrolled_window_map;
  widget_class->unmap = gtk_scrolled_window_unmap;
  widget_class->draw = gtk_scrolled_window_draw;
  widget_class->size_request = gtk_scrolled_window_size_request;
  widget_class->size_allocate = gtk_scrolled_window_size_allocate;

  container_class->add = gtk_scrolled_window_add;
  container_class->remove = gtk_scrolled_window_remove;
  container_class->forall = gtk_scrolled_window_forall;

  class->scrollbar_spacing = 5;
}

static void
gtk_scrolled_window_set_arg (GtkObject        *object,
			     GtkArg           *arg,
			     guint             arg_id)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  switch (arg_id)
    {
    case ARG_HADJUSTMENT:
      gtk_scrolled_window_set_hadjustment (scrolled_window, GTK_VALUE_POINTER (*arg));
      break;
    case ARG_VADJUSTMENT:
      gtk_scrolled_window_set_vadjustment (scrolled_window, GTK_VALUE_POINTER (*arg));
      break;
    case ARG_HSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      GTK_VALUE_ENUM (*arg),
				      scrolled_window->vscrollbar_policy);
      break;
    case ARG_VSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      scrolled_window->hscrollbar_policy,
				      GTK_VALUE_ENUM (*arg));
      break;
    case ARG_WINDOW_PLACEMENT:
      gtk_scrolled_window_set_placement (scrolled_window,
					 GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_scrolled_window_get_arg (GtkObject        *object,
			     GtkArg           *arg,
			     guint             arg_id)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  switch (arg_id)
    {
    case ARG_HADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = gtk_scrolled_window_get_hadjustment (scrolled_window);
      break;
    case ARG_VADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = gtk_scrolled_window_get_vadjustment (scrolled_window);
      break;
    case ARG_HSCROLLBAR_POLICY:
      GTK_VALUE_ENUM (*arg) = scrolled_window->hscrollbar_policy;
      break;
    case ARG_VSCROLLBAR_POLICY:
      GTK_VALUE_ENUM (*arg) = scrolled_window->vscrollbar_policy;
      break;
    case ARG_WINDOW_PLACEMENT:
      GTK_VALUE_ENUM (*arg) = scrolled_window->window_placement;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_scrolled_window_init (GtkScrolledWindow *scrolled_window)
{
  GTK_WIDGET_SET_FLAGS (scrolled_window, GTK_NO_WINDOW);

  gtk_container_set_resize_mode (GTK_CONTAINER (scrolled_window), GTK_RESIZE_QUEUE);

  scrolled_window->child = NULL;
  scrolled_window->hscrollbar = NULL;
  scrolled_window->vscrollbar = NULL;
  scrolled_window->hscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->vscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->hscrollbar_visible = FALSE;
  scrolled_window->vscrollbar_visible = FALSE;
  scrolled_window->window_placement = GTK_CORNER_TOP_LEFT;
}

GtkWidget*
gtk_scrolled_window_new (GtkAdjustment *hadjustment,
			 GtkAdjustment *vadjustment)
{
  GtkWidget *scrolled_window;

  if (hadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), NULL);

  if (vadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), NULL);

  scrolled_window = gtk_widget_new (GTK_TYPE_SCROLLED_WINDOW,
				    "hadjustment", hadjustment,
				    "vadjustment", vadjustment,
				    NULL);

  return scrolled_window;
}

void
gtk_scrolled_window_construct (GtkScrolledWindow *scrolled_window,
			       GtkAdjustment     *hadjustment,
			       GtkAdjustment     *vadjustment)
{
  g_message ("gtk_scrolled_window_construct() is deprecated");
  gtk_scrolled_window_set_hadjustment (scrolled_window, hadjustment);
  gtk_scrolled_window_set_vadjustment (scrolled_window, vadjustment);
  gtk_object_default_construct (GTK_OBJECT (scrolled_window));
}

void
gtk_scrolled_window_set_hadjustment (GtkScrolledWindow *scrolled_window,
				     GtkAdjustment     *hadjustment)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment = (GtkAdjustment*) gtk_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  if (!scrolled_window->hscrollbar)
    {
      gtk_widget_push_composite_child ();
      scrolled_window->hscrollbar = gtk_hscrollbar_new (hadjustment);
      gtk_widget_pop_composite_child ();

      gtk_widget_set_parent (scrolled_window->hscrollbar, GTK_WIDGET (scrolled_window));
      gtk_widget_ref (scrolled_window->hscrollbar);
      gtk_widget_show (scrolled_window->hscrollbar);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
      if (old_adjustment == hadjustment)
	return;

      gtk_signal_disconnect_by_func (GTK_OBJECT (old_adjustment),
				     GTK_SIGNAL_FUNC (gtk_scrolled_window_adjustment_changed),
				     scrolled_window);
      gtk_range_set_adjustment (GTK_RANGE (scrolled_window->hscrollbar),
				hadjustment);
    }
  hadjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
  gtk_signal_connect (GTK_OBJECT (hadjustment),
		      "changed",
		      GTK_SIGNAL_FUNC (gtk_scrolled_window_adjustment_changed),
		      scrolled_window);
  gtk_scrolled_window_adjustment_changed (hadjustment, scrolled_window);
  
  if (scrolled_window->child)
    gtk_widget_scroll_adjustements (scrolled_window->child,
				    gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
				    gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)));
}

void
gtk_scrolled_window_set_vadjustment (GtkScrolledWindow *scrolled_window,
				     GtkAdjustment     *vadjustment)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment = (GtkAdjustment*) gtk_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  if (!scrolled_window->vscrollbar)
    {
      gtk_widget_push_composite_child ();
      scrolled_window->vscrollbar = gtk_vscrollbar_new (vadjustment);
      gtk_widget_pop_composite_child ();

      gtk_widget_set_parent (scrolled_window->vscrollbar, GTK_WIDGET (scrolled_window));
      gtk_widget_ref (scrolled_window->vscrollbar);
      gtk_widget_show (scrolled_window->vscrollbar);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
      if (old_adjustment == vadjustment)
	return;

      gtk_signal_disconnect_by_func (GTK_OBJECT (old_adjustment),
				     GTK_SIGNAL_FUNC (gtk_scrolled_window_adjustment_changed),
				     scrolled_window);
      gtk_range_set_adjustment (GTK_RANGE (scrolled_window->vscrollbar),
				vadjustment);
    }
  vadjustment = gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
  gtk_signal_connect (GTK_OBJECT (vadjustment),
		      "changed",
		      GTK_SIGNAL_FUNC (gtk_scrolled_window_adjustment_changed),
		      scrolled_window);
  gtk_scrolled_window_adjustment_changed (vadjustment, scrolled_window);

  if (scrolled_window->child)
    gtk_widget_scroll_adjustements (scrolled_window->child,
				    gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
				    gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)));
}

GtkAdjustment*
gtk_scrolled_window_get_hadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (scrolled_window != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return (scrolled_window->hscrollbar ?
	  gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)) :
	  NULL);
}

GtkAdjustment*
gtk_scrolled_window_get_vadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (scrolled_window != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return (scrolled_window->vscrollbar ?
	  gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar)) :
	  NULL);
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

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

void
gtk_scrolled_window_set_placement (GtkScrolledWindow *scrolled_window,
				   GtkCornerType      window_placement)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (scrolled_window->window_placement != window_placement)
    {
      scrolled_window->window_placement = window_placement;

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

  gtk_widget_unparent (scrolled_window->hscrollbar);
  gtk_widget_unparent (scrolled_window->vscrollbar);
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

  gtk_widget_unref (scrolled_window->hscrollbar);
  gtk_widget_unref (scrolled_window->vscrollbar);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_scrolled_window_map (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));

  if (!GTK_WIDGET_MAPPED (widget))
    {
      GtkScrolledWindow *scrolled_window;

      scrolled_window = GTK_SCROLLED_WINDOW (widget);
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

      if (scrolled_window->child &&
	  GTK_WIDGET_VISIBLE (scrolled_window->child) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->child))
	gtk_widget_map (scrolled_window->child);

      if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->hscrollbar))
	gtk_widget_map (scrolled_window->hscrollbar);

      if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->vscrollbar))
	gtk_widget_map (scrolled_window->vscrollbar);

      gtk_widget_queue_draw (widget);
    }
}

static void
gtk_scrolled_window_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));

  if (GTK_WIDGET_MAPPED (widget))
    {
      GtkScrolledWindow *scrolled_window;

      scrolled_window = GTK_SCROLLED_WINDOW (widget);
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

      if (scrolled_window->child &&
	  GTK_WIDGET_VISIBLE (scrolled_window->child) &&
	  GTK_WIDGET_MAPPED (scrolled_window->child))
	gtk_widget_unmap (scrolled_window->child);

      if (GTK_WIDGET_MAPPED (scrolled_window->hscrollbar))
	gtk_widget_unmap (scrolled_window->hscrollbar);

      if (GTK_WIDGET_MAPPED (scrolled_window->vscrollbar))
	gtk_widget_unmap (scrolled_window->vscrollbar);

      gtk_widget_queue_clear (widget);
    }
}

static void
gtk_scrolled_window_draw (GtkWidget    *widget,
			  GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (area != NULL);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GtkScrolledWindow *scrolled_window;
      GdkRectangle child_area;
      
      scrolled_window = GTK_SCROLLED_WINDOW (widget);
      
      if (scrolled_window->child && GTK_WIDGET_VISIBLE (scrolled_window->child) &&
	  gtk_widget_intersect (scrolled_window->child, area, &child_area))
	gtk_widget_draw (scrolled_window->child, &child_area);

      if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar) &&
	  gtk_widget_intersect (scrolled_window->hscrollbar, area, &child_area))
	gtk_widget_draw (scrolled_window->hscrollbar, &child_area);

      if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar) &&
	  gtk_widget_intersect (scrolled_window->vscrollbar, area, &child_area))
	gtk_widget_draw (scrolled_window->vscrollbar, &child_area);
    }
}

static void
gtk_scrolled_window_forall (GtkContainer *container,
			    gboolean	  include_internals,
			    GtkCallback   callback,
			    gpointer      callback_data)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (callback != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);

  if (scrolled_window->child)
    callback (scrolled_window->child, callback_data);
  if (include_internals)
    {
      callback (scrolled_window->vscrollbar, callback_data);
      callback (scrolled_window->hscrollbar, callback_data);
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

  if (scrolled_window->child && GTK_WIDGET_VISIBLE (scrolled_window->child))
    {
      gtk_widget_size_request (scrolled_window->child, &scrolled_window->child->requisition);

      requisition->width += scrolled_window->child->requisition.width;
      requisition->height += scrolled_window->child->requisition.height;
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
gtk_scrolled_window_relative_allocation (GtkWidget     *widget,
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

  if (scrolled_window->vscrollbar_visible)
    {
      if (scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)
	allocation->x += (scrolled_window->vscrollbar->requisition.width +
			  SCROLLBAR_SPACING (scrolled_window));

      allocation->width = MAX (1, allocation->width -
			       (scrolled_window->vscrollbar->requisition.width +
				SCROLLBAR_SPACING (scrolled_window)));
    }
  if (scrolled_window->hscrollbar_visible)
    {
      if (scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)
	allocation->y += (scrolled_window->hscrollbar->requisition.height +
			  SCROLLBAR_SPACING (scrolled_window));

      allocation->height = MAX (1, allocation->height -
				(scrolled_window->hscrollbar->requisition.height +
				 SCROLLBAR_SPACING (scrolled_window)));
    }
}

static void
gtk_scrolled_window_size_allocate (GtkWidget     *widget,
				   GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;
  GtkAllocation relative_allocation;
  GtkAllocation child_allocation;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  widget->allocation = *allocation;

  if (scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->hscrollbar_visible = TRUE;
  if (scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->vscrollbar_visible = TRUE;

  if (scrolled_window->child && GTK_WIDGET_VISIBLE (scrolled_window->child))
    {
      gboolean previous_hvis;
      gboolean previous_vvis;
      guint count = 0;
      
      do
	{
	  gtk_scrolled_window_relative_allocation (widget, &relative_allocation);
	  
	  child_allocation.x = relative_allocation.x + allocation->x;
	  child_allocation.y = relative_allocation.y + allocation->y;
	  child_allocation.width = relative_allocation.width;
	  child_allocation.height = relative_allocation.height;
	  
	  previous_hvis = scrolled_window->hscrollbar_visible;
	  previous_vvis = scrolled_window->vscrollbar_visible;
	  
	  gtk_widget_size_allocate (scrolled_window->child, &child_allocation);

	  /* If, after the first iteration, the hscrollbar and the
	   * vscrollbar flip visiblity, then we need both.
	   */
	  if (count &&
	      previous_hvis != scrolled_window->hscrollbar_visible &&
	      previous_vvis != scrolled_window->vscrollbar_visible)
	    {
	      scrolled_window->hscrollbar_visible = TRUE;
	      scrolled_window->vscrollbar_visible = TRUE;

	      /* a new resize is already queued at this point,
	       * so we will immediatedly get reinvoked
	       */
	      return;
	    }
	  
	  count++;
	}
      while (previous_hvis != scrolled_window->hscrollbar_visible ||
	     previous_vvis != scrolled_window->vscrollbar_visible);
    }
  else
    gtk_scrolled_window_relative_allocation (widget, &relative_allocation);
  
  if (scrolled_window->hscrollbar_visible)
    {
      if (!GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
	gtk_widget_show (scrolled_window->hscrollbar);

      child_allocation.x = relative_allocation.x;
      if (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT)
	child_allocation.y = (relative_allocation.y +
			      relative_allocation.height +
			      SCROLLBAR_SPACING (scrolled_window));
      else
	child_allocation.y = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.width = relative_allocation.width;
      child_allocation.height = scrolled_window->hscrollbar->requisition.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (scrolled_window->hscrollbar, &child_allocation);
    }
  else if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    gtk_widget_hide (scrolled_window->hscrollbar);

  if (scrolled_window->vscrollbar_visible)
    {
      if (!GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
	gtk_widget_show (scrolled_window->vscrollbar);

      if (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT)
	child_allocation.x = (relative_allocation.x +
			      relative_allocation.width +
			      SCROLLBAR_SPACING (scrolled_window));
      else
	child_allocation.x = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.y = relative_allocation.y;
      child_allocation.width = scrolled_window->vscrollbar->requisition.width;
      child_allocation.height = relative_allocation.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (scrolled_window->vscrollbar, &child_allocation);
    }
  else if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    gtk_widget_hide (scrolled_window->vscrollbar);
}


static void
gtk_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
					gpointer       data)
{
  GtkScrolledWindow *scrolled_win;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  scrolled_win = GTK_SCROLLED_WINDOW (data);

  if (adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->hscrollbar)))
    {
      if (scrolled_win->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  gboolean visible;
	  
	  visible = scrolled_win->hscrollbar_visible;
	  scrolled_win->hscrollbar_visible = (adjustment->upper - adjustment->lower >
					      adjustment->page_size);
	  if (scrolled_win->hscrollbar_visible != visible)
	    gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
	}
    }
  else if (adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->vscrollbar)))
    {
      if (scrolled_win->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  gboolean visible;

	  visible = scrolled_win->vscrollbar_visible;
	  scrolled_win->vscrollbar_visible = (adjustment->upper - adjustment->lower >
					      adjustment->page_size);
	  if (scrolled_win->vscrollbar_visible != visible)
	    gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
	}
    }
}

static void
gtk_scrolled_window_add (GtkContainer *container,
			 GtkWidget    *child)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (container);
  g_return_if_fail (scrolled_window->child == NULL);

  gtk_widget_set_parent (child, GTK_WIDGET (container));
  scrolled_window->child = child;

  /* this is a temporary message */
  if (!gtk_widget_scroll_adjustements (child,
				       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar)),
				       gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar))))
    g_message ("gtk_scrolled_window_add(): cannot add non scrollable widget "
	       "use gtk_scrolled_window_add_with_viewport() instead");

  if (GTK_WIDGET_VISIBLE (scrolled_window))
    {
      if (GTK_WIDGET_REALIZED (scrolled_window) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);

      if (GTK_WIDGET_MAPPED (scrolled_window) &&
	  !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }
  
  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (scrolled_window))
    gtk_widget_queue_resize (child);
}

static void
gtk_scrolled_window_remove (GtkContainer *container,
			    GtkWidget    *child)
{
  GtkScrolledWindow *scrolled_window;
  gboolean widget_was_visible;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (child != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);
  g_return_if_fail (scrolled_window->child == child);
  
  widget_was_visible = GTK_WIDGET_VISIBLE (child);
  
  gtk_widget_scroll_adjustements (child, NULL, NULL);
  gtk_widget_unparent (child);
  scrolled_window->child = NULL;
  
  if (widget_was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

void
gtk_scrolled_window_add_with_viewport (GtkScrolledWindow *scrolled_window,
				       GtkWidget         *child)
{
  GtkWidget *viewport;

  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (scrolled_window->child == NULL);
  g_return_if_fail (child->parent == NULL);

  viewport =
    gtk_viewport_new (gtk_scrolled_window_get_hadjustment (scrolled_window),
		      gtk_scrolled_window_get_vadjustment (scrolled_window));
  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), child);
  gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);
}
