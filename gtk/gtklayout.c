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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * GtkLayout: Widget for scrolling of arbitrary-sized areas.
 *
 * Copyright Owen Taylor, 1998
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdkconfig.h"

#include "gtklayout.h"
#include "gtksignal.h"
#include "gtkprivate.h"

typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
  GtkWidget *widget;
  gint x;
  gint y;
};

static void     gtk_layout_class_init         (GtkLayoutClass *class);
static void     gtk_layout_init               (GtkLayout      *layout);

static void     gtk_layout_finalize           (GtkObject      *object);
static void     gtk_layout_realize            (GtkWidget      *widget);
static void     gtk_layout_unrealize          (GtkWidget      *widget);
static void     gtk_layout_map                (GtkWidget      *widget);
static void     gtk_layout_size_request       (GtkWidget      *widget,
					       GtkRequisition *requisition);
static void     gtk_layout_size_allocate      (GtkWidget      *widget,
					       GtkAllocation  *allocation);
static void     gtk_layout_draw               (GtkWidget      *widget, 
					       GdkRectangle   *area);
static gint     gtk_layout_expose             (GtkWidget      *widget, 
					       GdkEventExpose *event);

static void     gtk_layout_remove             (GtkContainer *container, 
					       GtkWidget    *widget);
static void     gtk_layout_forall             (GtkContainer *container,
					       gboolean      include_internals,
					       GtkCallback   callback,
					       gpointer      callback_data);
static void     gtk_layout_set_adjustments    (GtkLayout     *layout,
					       GtkAdjustment *hadj,
					       GtkAdjustment *vadj);

static void     gtk_layout_allocate_child     (GtkLayout      *layout,
					       GtkLayoutChild *child);


static void     gtk_layout_adjustment_changed (GtkAdjustment  *adjustment,
					       GtkLayout      *layout);

static GtkWidgetClass *parent_class = NULL;

/* Public interface
 */
  
GtkWidget*    
gtk_layout_new (GtkAdjustment *hadjustment,
		GtkAdjustment *vadjustment)
{
  GtkLayout *layout;

  layout = gtk_type_new (GTK_TYPE_LAYOUT);

  gtk_layout_set_adjustments (layout, hadjustment, vadjustment);

  return GTK_WIDGET (layout);
}

GtkAdjustment* 
gtk_layout_get_hadjustment (GtkLayout     *layout)
{
  g_return_val_if_fail (layout != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->hadjustment;
}
GtkAdjustment* 
gtk_layout_get_vadjustment (GtkLayout     *layout)
{
  g_return_val_if_fail (layout != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->vadjustment;
}

static void           
gtk_layout_set_adjustments (GtkLayout     *layout,
			    GtkAdjustment *hadj,
			    GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  
  if (layout->hadjustment && (layout->hadjustment != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (layout->hadjustment), layout);
      gtk_object_unref (GTK_OBJECT (layout->hadjustment));
    }
  
  if (layout->vadjustment && (layout->vadjustment != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (layout->vadjustment), layout);
      gtk_object_unref (GTK_OBJECT (layout->vadjustment));
    }
  
  if (layout->hadjustment != hadj)
    {
      layout->hadjustment = hadj;
      gtk_object_ref (GTK_OBJECT (layout->hadjustment));
      gtk_object_sink (GTK_OBJECT (layout->hadjustment));
      
      gtk_signal_connect (GTK_OBJECT (layout->hadjustment), "value_changed",
			  (GtkSignalFunc) gtk_layout_adjustment_changed,
			  layout);
      need_adjust = TRUE;
    }
  
  if (layout->vadjustment != vadj)
    {
      layout->vadjustment = vadj;
      gtk_object_ref (GTK_OBJECT (layout->vadjustment));
      gtk_object_sink (GTK_OBJECT (layout->vadjustment));
      
      gtk_signal_connect (GTK_OBJECT (layout->vadjustment), "value_changed",
			  (GtkSignalFunc) gtk_layout_adjustment_changed,
			  layout);
      need_adjust = TRUE;
    }

  if (need_adjust)
    gtk_layout_adjustment_changed (NULL, layout);
}

static void
gtk_layout_finalize (GtkObject *object)
{
  GtkLayout *layout = GTK_LAYOUT (object);

  gtk_object_unref (GTK_OBJECT (layout->hadjustment));
  gtk_object_unref (GTK_OBJECT (layout->vadjustment));

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

void           
gtk_layout_set_hadjustment (GtkLayout     *layout,
			    GtkAdjustment *adjustment)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  gtk_layout_set_adjustments (layout, adjustment, layout->vadjustment);
}
 

void           
gtk_layout_set_vadjustment (GtkLayout     *layout,
			    GtkAdjustment *adjustment)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  
  gtk_layout_set_adjustments (layout, layout->hadjustment, adjustment);
}


void           
gtk_layout_put (GtkLayout     *layout, 
		GtkWidget     *child_widget, 
		gint           x, 
		gint           y)
{
  GtkLayoutChild *child;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (child_widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child_widget));
  
  child = g_new (GtkLayoutChild, 1);

  child->widget = child_widget;
  child->x = x;
  child->y = y;

  layout->children = g_list_append (layout->children, child);
  
  gtk_widget_set_parent (child_widget, GTK_WIDGET (layout));
  if (GTK_WIDGET_REALIZED (layout))
    gtk_widget_set_parent_window (child->widget, layout->bin_window);

  if (GTK_WIDGET_REALIZED (layout))
    gtk_widget_realize (child_widget);
    
  if (GTK_WIDGET_VISIBLE (layout) && GTK_WIDGET_VISIBLE (child_widget))
    {
      if (GTK_WIDGET_MAPPED (layout))
	gtk_widget_map (child_widget);

      gtk_widget_queue_resize (child_widget);
    }
}

void           
gtk_layout_move (GtkLayout     *layout, 
		 GtkWidget     *child_widget, 
		 gint           x, 
		 gint           y)
{
  GList *tmp_list;
  GtkLayoutChild *child;
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (child->widget == child_widget)
	{
	  child->x = x;
	  child->y = y;

	  if (GTK_WIDGET_VISIBLE (child_widget) && GTK_WIDGET_VISIBLE (layout))
	    gtk_widget_queue_resize (child_widget);

	  return;
	}
    }
}

void
gtk_layout_set_size (GtkLayout     *layout, 
		     guint          width,
		     guint          height)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  layout->width = width;
  layout->height = height;

  layout->hadjustment->upper = layout->width;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->hadjustment), "changed");

  layout->vadjustment->upper = layout->height;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->vadjustment), "changed");

  if (GTK_WIDGET_REALIZED (layout))
    gdk_window_resize (layout->bin_window, width, height);
}

void
gtk_layout_freeze (GtkLayout *layout)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  layout->freeze_count++;
}

void
gtk_layout_thaw (GtkLayout *layout)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (layout));

  if (layout->freeze_count)
    if (!(--layout->freeze_count))
      gtk_widget_draw (GTK_WIDGET (layout), NULL);
}

/* Basic Object handling procedures
 */
GtkType
gtk_layout_get_type (void)
{
  static GtkType layout_type = 0;

  if (!layout_type)
    {
      static const GtkTypeInfo layout_info =
      {
	"GtkLayout",
	sizeof (GtkLayout),
	sizeof (GtkLayoutClass),
	(GtkClassInitFunc) gtk_layout_class_init,
	(GtkObjectInitFunc) gtk_layout_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      layout_type = gtk_type_unique (GTK_TYPE_CONTAINER, &layout_info);
    }

  return layout_type;
}

static void
gtk_layout_class_init (GtkLayoutClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  object_class->finalize = gtk_layout_finalize;

  widget_class->realize = gtk_layout_realize;
  widget_class->unrealize = gtk_layout_unrealize;
  widget_class->map = gtk_layout_map;
  widget_class->size_request = gtk_layout_size_request;
  widget_class->size_allocate = gtk_layout_size_allocate;
  widget_class->draw = gtk_layout_draw;
  widget_class->expose_event = gtk_layout_expose;

  container_class->remove = gtk_layout_remove;
  container_class->forall = gtk_layout_forall;

  class->set_scroll_adjustments = gtk_layout_set_adjustments;

  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkLayoutClass, set_scroll_adjustments),
		    gtk_marshal_NONE__POINTER_POINTER,
		    GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
}

static void
gtk_layout_init (GtkLayout *layout)
{
  layout->children = NULL;

  layout->width = 100;
  layout->height = 100;

  layout->hadjustment = NULL;
  layout->vadjustment = NULL;

  layout->bin_window = NULL;

  layout->configure_serial = 0;
  layout->scroll_x = 0;
  layout->scroll_y = 0;
  layout->visibility = GDK_VISIBILITY_PARTIAL;

  layout->freeze_count = 0;
}

/* Widget methods
 */

static void 
gtk_layout_realize (GtkWidget *widget)
{
  GList *tmp_list;
  GtkLayout *layout;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);
  GTK_WIDGET_SET_FLAGS (layout, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = layout->width;
  attributes.height = layout->height;
  attributes.event_mask = GDK_EXPOSURE_MASK | GDK_SCROLL_MASK | 
                          gtk_widget_get_events (widget);

  layout->bin_window = gdk_window_new (widget->window,
					&attributes, attributes_mask);
  gdk_window_set_user_data (layout->bin_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, layout->bin_window, GTK_STATE_NORMAL);

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_widget_set_parent_window (child->widget, layout->bin_window);
    }
}

static void 
gtk_layout_map (GtkWidget *widget)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  if (!GTK_WIDGET_MAPPED (child->widget) && 
	      !GTK_WIDGET_IS_OFFSCREEN (child->widget))
	    gtk_widget_map (child->widget);
	}
    }

  gdk_window_show (layout->bin_window);
  gdk_window_show (widget->window);
}

static void 
gtk_layout_unrealize (GtkWidget *widget)
{
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  gdk_window_set_user_data (layout->bin_window, NULL);
  gdk_window_destroy (layout->bin_window);
  layout->bin_window = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void     
gtk_layout_size_request (GtkWidget     *widget,
			 GtkRequisition *requisition)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  requisition->width = 0;
  requisition->height = 0;

  tmp_list = layout->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      GtkRequisition child_requisition;
      
      tmp_list = tmp_list->next;

      gtk_widget_size_request (child->widget, &child_requisition);
    }
}

static void     
gtk_layout_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GList *tmp_list;
  GtkLayout *layout;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  widget->allocation = *allocation;
  
  layout = GTK_LAYOUT (widget);

  tmp_list = layout->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_layout_allocate_child (layout, child);
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
    }

  layout->hadjustment->page_size = allocation->width;
  layout->hadjustment->page_increment = allocation->width / 2;
  layout->hadjustment->lower = 0;
  layout->hadjustment->upper = layout->width;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->hadjustment), "changed");

  layout->vadjustment->page_size = allocation->height;
  layout->vadjustment->page_increment = allocation->height / 2;
  layout->vadjustment->lower = 0;
  layout->vadjustment->upper = layout->height;
  gtk_signal_emit_by_name (GTK_OBJECT (layout->vadjustment), "changed");
}

static void 
gtk_layout_draw (GtkWidget *widget, GdkRectangle *area)
{
  GList *tmp_list;
  GtkLayout *layout;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (widget));

  layout = GTK_LAYOUT (widget);

  /* We don't have any way of telling themes about this properly,
   * so we just assume a background pixmap
   */
  if (!GTK_WIDGET_APP_PAINTABLE (widget))
    gdk_window_clear_area (layout->bin_window,
			   area->x, area->y, area->width, area->height);
  
  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (gtk_widget_intersect (child->widget, area, &child_area))
	gtk_widget_draw (child->widget, &child_area);
    }
}

static gint 
gtk_layout_expose (GtkWidget *widget, GdkEventExpose *event)
{
  GList *tmp_list;
  GtkLayout *layout;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LAYOUT (widget), FALSE);

  layout = GTK_LAYOUT (widget);

  if (event->window != layout->bin_window)
    return FALSE;
  
  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      child_event = *event;
      if (GTK_WIDGET_DRAWABLE (child->widget) &&
	  GTK_WIDGET_NO_WINDOW (child->widget) &&
	  gtk_widget_intersect (child->widget, &event->area, &child_event.area))
	gtk_widget_event (child->widget, (GdkEvent*) &child_event);
    }

  return FALSE;
}

/* Container method
 */
static void
gtk_layout_remove (GtkContainer *container, 
		   GtkWidget    *widget)
{
  GList *tmp_list;
  GtkLayout *layout;
  GtkLayoutChild *child = NULL;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (container));
  
  layout = GTK_LAYOUT (container);

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)
    {
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_IS_OFFSCREEN);

      gtk_widget_unparent (widget);

      layout->children = g_list_remove_link (layout->children, tmp_list);
      g_list_free_1 (tmp_list);
      g_free (child);
    }
}

static void
gtk_layout_forall (GtkContainer *container,
		   gboolean      include_internals,
		   GtkCallback   callback,
		   gpointer      callback_data)
{
  GtkLayout *layout;
  GtkLayoutChild *child;
  GList *tmp_list;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LAYOUT (container));
  g_return_if_fail (callback != NULL);

  layout = GTK_LAYOUT (container);

  tmp_list = layout->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
}

/* Operations on children
 */

static void
gtk_layout_allocate_child (GtkLayout      *layout,
			   GtkLayoutChild *child)
{
  GtkAllocation allocation;
  GtkRequisition requisition;

  allocation.x = child->x - layout->xoffset;
  allocation.y = child->y - layout->yoffset;
  gtk_widget_get_child_requisition (child->widget, &requisition);
  allocation.width = requisition.width;
  allocation.height = requisition.height;
  
  gtk_widget_size_allocate (child->widget, &allocation);
}

/* Callbacks */

static void
gtk_layout_adjustment_changed (GtkAdjustment *adjustment,
			       GtkLayout     *layout)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (layout);

  if (layout->freeze_count)
    return;

  if (GTK_WIDGET_REALIZED (layout))
    {
      gdk_window_move (layout->bin_window,
		       - layout->hadjustment->value,
		       - layout->vadjustment->value);
      
      gdk_window_process_updates (layout->bin_window, TRUE);
    }
}
