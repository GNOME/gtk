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

#include "gtklayout.h"
#include "gtksignal.h"
#include "gtkprivate.h"
#include "gdk/gdkx.h"

typedef struct _GtkLayoutAdjData GtkLayoutAdjData;
typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutAdjData {
  gint dx;
  gint dy;
};

struct _GtkLayoutChild {
  GtkWidget *widget;
  gint x;
  gint y;
};

#define IS_ONSCREEN(x,y) ((x >= G_MINSHORT) && (x <= G_MAXSHORT) && \
                          (y >= G_MINSHORT) && (y <= G_MAXSHORT))

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

static void     gtk_layout_position_child     (GtkLayout      *layout,
					       GtkLayoutChild *child);
static void     gtk_layout_allocate_child     (GtkLayout      *layout,
					       GtkLayoutChild *child);
static void     gtk_layout_position_children  (GtkLayout      *layout);

static void     gtk_layout_adjust_allocations_recurse (GtkWidget *widget,
						       gpointer   cb_data);
static void     gtk_layout_adjust_allocations         (GtkLayout *layout,
					               gint       dx,
						       gint       dy);



static void     gtk_layout_expose_area        (GtkLayout      *layout,
					       gint            x, 
					       gint            y, 
					       gint            width, 
					       gint            height);
static void     gtk_layout_adjustment_changed (GtkAdjustment  *adjustment,
					       GtkLayout      *layout);
static GdkFilterReturn gtk_layout_main_filter (GdkXEvent      *gdk_xevent,
					       GdkEvent       *event,
					       gpointer        data);

static GtkWidgetClass *parent_class = NULL;
static gboolean gravity_works;

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

  if (!IS_ONSCREEN (x, y))
    GTK_PRIVATE_SET_FLAG (child_widget, GTK_IS_OFFSCREEN);

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
      {
	gtk_layout_position_children (layout);
	gtk_widget_draw (GTK_WIDGET (layout), NULL);
      }
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
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
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
  attributes.event_mask = GDK_EXPOSURE_MASK | 
                          gtk_widget_get_events (widget);

  layout->bin_window = gdk_window_new (widget->window,
					&attributes, attributes_mask);
  gdk_window_set_user_data (layout->bin_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, layout->bin_window, GTK_STATE_NORMAL);

  gdk_window_add_filter (widget->window, gtk_layout_main_filter, layout);
  /*   gdk_window_add_filter (layout->bin_window, gtk_layout_filter, layout);*/

  /* XXX: If we ever get multiple displays for GTK+, then gravity_works
   *      will have to become a widget member. Right now we just
   *      keep it as a global
   */
  gravity_works = gdk_window_set_static_gravities (layout->bin_window, TRUE);

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

      gtk_layout_position_child (layout, child);
      gtk_layout_allocate_child (layout, child);
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      gdk_window_move_resize (GTK_LAYOUT(widget)->bin_window,
			      0, 0,
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
gtk_layout_position_child (GtkLayout      *layout,
			   GtkLayoutChild *child)
{
  gint x;
  gint y;

  x = child->x - layout->xoffset;
  y = child->y - layout->yoffset;

  if (IS_ONSCREEN (x,y))
    {
      if (GTK_WIDGET_MAPPED (layout) &&
	  GTK_WIDGET_VISIBLE (child->widget))
	{
	  if (!GTK_WIDGET_MAPPED (child->widget))
	    gtk_widget_map (child->widget);
	}

      if (GTK_WIDGET_IS_OFFSCREEN (child->widget))
	GTK_PRIVATE_UNSET_FLAG (child->widget, GTK_IS_OFFSCREEN);
    }
  else
    {
      if (!GTK_WIDGET_IS_OFFSCREEN (child->widget))
	GTK_PRIVATE_SET_FLAG (child->widget, GTK_IS_OFFSCREEN);

      if (GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_unmap (child->widget);
    }
}

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

static void
gtk_layout_position_children (GtkLayout *layout)
{
  GList *tmp_list;

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;
      
      gtk_layout_position_child (layout, child);
    }
}

static void
gtk_layout_adjust_allocations_recurse (GtkWidget *widget,
				       gpointer   cb_data)
{
  GtkLayoutAdjData *data = cb_data;

  widget->allocation.x += data->dx;
  widget->allocation.y += data->dy;

  if (GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), 
			  gtk_layout_adjust_allocations_recurse,
			  cb_data);
}

static void
gtk_layout_adjust_allocations (GtkLayout *layout,
			       gint       dx,
			       gint       dy)
{
  GList *tmp_list;
  GtkLayoutAdjData data;

  data.dx = dx;
  data.dy = dy;

  tmp_list = layout->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;
      
      child->widget->allocation.x += dx;
      child->widget->allocation.y += dy;

      if (GTK_WIDGET_NO_WINDOW (child->widget) &&
	  GTK_IS_CONTAINER (child->widget))
	gtk_container_forall (GTK_CONTAINER (child->widget), 
			      gtk_layout_adjust_allocations_recurse,
			      &data);
    }
}
  
/* Callbacks */

/* Send a synthetic expose event to the widget
 */
static void
gtk_layout_expose_area (GtkLayout    *layout,
			gint x, gint y, gint width, gint height)
{
  if (layout->visibility == GDK_VISIBILITY_UNOBSCURED)
    {
      GdkEventExpose event;
      
      event.type = GDK_EXPOSE;
      event.send_event = TRUE;
      event.window = layout->bin_window;
      event.count = 0;
      
      event.area.x = x;
      event.area.y = y;
      event.area.width = width;
      event.area.height = height;
      
      gdk_window_ref (event.window);
      gtk_widget_event (GTK_WIDGET (layout), (GdkEvent *)&event);
      gdk_window_unref (event.window);
    }
}

/* This function is used to find events to process while scrolling
 */

static Bool 
gtk_layout_expose_predicate (Display *display, 
		  XEvent  *xevent, 
		  XPointer arg)
{
  if ((xevent->type == Expose) || 
      ((xevent->xany.window == *(Window *)arg) &&
       (xevent->type == ConfigureNotify)))
    return True;
  else
    return False;
}

/* This is the main routine to do the scrolling. Scrolling is
 * done by "Guffaw" scrolling, as in the Mozilla XFE, with
 * a few modifications.
 * 
 * The main improvement is that we keep track of whether we
 * are obscured or not. If not, we ignore the generated expose
 * events and instead do the exposes ourself, without having
 * to wait for a roundtrip to the server. This also provides
 * a limited form of expose-event compression, since we do
 * the affected area as one big chunk.
 *
 * Real expose event compression, as in the XFE, could be added
 * here. It would help opaque drags over the region, and the
 * obscured case.
 *
 * Code needs to be added here to do the scrolling on machines
 * that don't have working WindowGravity. That could be done
 *
 *  - XCopyArea and move the windows, and accept trailing the
 *    background color. (Since it is only a fallback method)
 *  - XmHTML style. As above, but turn off expose events when
 *    not obscured and do the exposures ourself.
 *  - gzilla-style. Move the window continuously, and reset
 *    every 32768 pixels
 */

static void
gtk_layout_adjustment_changed (GtkAdjustment *adjustment,
			       GtkLayout     *layout)
{
  GtkWidget *widget;
  XEvent xevent;
  
  gint dx, dy;

  widget = GTK_WIDGET (layout);

  dx = (gint)layout->hadjustment->value - layout->xoffset;
  dy = (gint)layout->vadjustment->value - layout->yoffset;

  layout->xoffset = (gint)layout->hadjustment->value;
  layout->yoffset = (gint)layout->vadjustment->value;

  if (layout->freeze_count)
    return;

  if (!GTK_WIDGET_MAPPED (layout))
    {
      gtk_layout_position_children (layout);
      return;
    }

  gtk_layout_adjust_allocations (layout, -dx, -dy);

  if (dx > 0)
    {
      if (gravity_works)
	{
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width + dx,
			     widget->allocation.height);
	  gdk_window_move   (layout->bin_window, -dx, 0);
	  gdk_window_move_resize (layout->bin_window,
				  0, 0,
				  widget->allocation.width,
				  widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}

      gtk_layout_expose_area (layout, 
			      MAX ((gint)widget->allocation.width - dx, 0),
			      0,
			      MIN (dx, widget->allocation.width),
			      widget->allocation.height);
    }
  else if (dx < 0)
    {
      if (gravity_works)
	{
	  gdk_window_move_resize (layout->bin_window,
				  dx, 0,
				  widget->allocation.width - dx,
				  widget->allocation.height);
	  gdk_window_move   (layout->bin_window, 0, 0);
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width,
			     widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}

      gtk_layout_expose_area (layout,
			      0,
			      0,
			      MIN (-dx, widget->allocation.width),
			      widget->allocation.height);
    }

  if (dy > 0)
    {
      if (gravity_works)
	{
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width,
			     widget->allocation.height + dy);
	  gdk_window_move   (layout->bin_window, 0, -dy);
	  gdk_window_move_resize (layout->bin_window,
				  0, 0,
				  widget->allocation.width,
				  widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}

      gtk_layout_expose_area (layout, 
			      0,
			      MAX ((gint)widget->allocation.height - dy, 0),
			      widget->allocation.width,
			      MIN (dy, widget->allocation.height));
    }
  else if (dy < 0)
    {
      if (gravity_works)
	{
	  gdk_window_move_resize (layout->bin_window,
				  0, dy,
				  widget->allocation.width,
				  widget->allocation.height - dy);
	  gdk_window_move   (layout->bin_window, 0, 0);
	  gdk_window_resize (layout->bin_window,
			     widget->allocation.width,
			     widget->allocation.height);
	}
      else
	{
	  /* FIXME */
	}
      gtk_layout_expose_area (layout, 
			      0,
			      0,
			      widget->allocation.width,
			      MIN (-dy, (gint)widget->allocation.height));
    }

  gtk_layout_position_children (layout);

  /* We have to make sure that all exposes from this scroll get
   * processed before we scroll again, or the expose events will
   * have invalid coordinates.
   *
   * We also do expose events for other windows, since otherwise
   * their updating will fall behind the scrolling 
   *
   * This also avoids a problem in pre-1.0 GTK where filters don't
   * have access to configure events that were compressed.
   */

  gdk_flush();
  while (XCheckIfEvent(GDK_WINDOW_XDISPLAY (layout->bin_window),
		       &xevent,
		       gtk_layout_expose_predicate,
		       (XPointer)&GDK_WINDOW_XWINDOW (layout->bin_window)))
    {
      GdkEvent event;
      GtkWidget *event_widget;

      switch (xevent.type)
	{
	case Expose:

	  if (xevent.xany.window == GDK_WINDOW_XWINDOW (layout->bin_window))
	    {
	      /* If the window is unobscured, then we've exposed the
	       * regions with the following serials already, so we
	       * can throw out the expose events.
	       */
	      if (layout->visibility == GDK_VISIBILITY_UNOBSCURED &&
		  (((dx > 0 || dy > 0) && 
		    xevent.xexpose.serial == layout->configure_serial) ||
		   ((dx < 0 || dy < 0) && 
		    xevent.xexpose.serial == layout->configure_serial + 1)))
		continue;
	      /* The following expose was generated while the origin was
	       * different from the current origin, so we need to offset it.
	       */
	      else if (xevent.xexpose.serial == layout->configure_serial)
		{
		  xevent.xexpose.x += layout->scroll_x;
		  xevent.xexpose.y += layout->scroll_y;
		}
	      event.expose.window = layout->bin_window;
	      event_widget = widget;
	    }
	  else
	    {
	      event.expose.window = gdk_window_lookup (xevent.xany.window);
	      gdk_window_get_user_data (event.expose.window, 
					(gpointer *)&event_widget);
	    }

	  if (event_widget)
	    {
	      event.expose.type = GDK_EXPOSE;
	      event.expose.area.x = xevent.xexpose.x;
	      event.expose.area.y = xevent.xexpose.y;
	      event.expose.area.width = xevent.xexpose.width;
	      event.expose.area.height = xevent.xexpose.height;
	      event.expose.count = xevent.xexpose.count;
	      
	      gdk_window_ref (event.expose.window);
	      gtk_widget_event (event_widget, &event);
	      gdk_window_unref (event.expose.window);
	    }
	  break;
      
	case ConfigureNotify:
	  if (xevent.xany.window == GDK_WINDOW_XWINDOW (layout->bin_window) &&
	      (xevent.xconfigure.x != 0 || xevent.xconfigure.y != 0))
	    {
	      layout->configure_serial = xevent.xconfigure.serial;
	      layout->scroll_x = xevent.xconfigure.x;
	      layout->scroll_y = xevent.xconfigure.y;
	    }
	  break;
	}
    }
}

#if 0
/* The main event filter. Actually, we probably don't really need
 * thisfilter at all, since we are calling it directly above in the
 * expose-handling hack. But in case scrollbars
 * are fixed up in some manner...
 *
 * This routine identifies expose events that are generated when
 * we've temporarily moved the bin_window_origin, and translates
 * them or discards them, depending on whether we are obscured
 * or not.
 */
static GdkFilterReturn 
gtk_layout_filter (GdkXEvent *gdk_xevent,
		   GdkEvent  *event,
		   gpointer   data)
{
  XEvent *xevent;
  GtkLayout *layout;

  xevent = (XEvent *)gdk_xevent;
  layout = GTK_LAYOUT (data);

  switch (xevent->type)
    {
    case Expose:
      if (xevent->xexpose.serial == layout->configure_serial)
	{
	  if (layout->visibility == GDK_VISIBILITY_UNOBSCURED)
	    return GDK_FILTER_REMOVE;
	  else
	    {
	      xevent->xexpose.x += layout->scroll_x;
	      xevent->xexpose.y += layout->scroll_y;
	      
	      break;
	    }
	}
      break;
      
    case ConfigureNotify:
       if ((xevent->xconfigure.x != 0) || (xevent->xconfigure.y != 0))
	{
	  layout->configure_serial = xevent->xconfigure.serial;
	  layout->scroll_x = xevent->xconfigure.x;
	  layout->scroll_y = xevent->xconfigure.y;
	}
      break;
    }
  
  return GDK_FILTER_CONTINUE;
}
#endif

/* Although GDK does have a GDK_VISIBILITY_NOTIFY event,
 * there is no corresponding event in GTK, so we have
 * to get the events from a filter
 */
static GdkFilterReturn 
gtk_layout_main_filter (GdkXEvent *gdk_xevent,
			GdkEvent  *event,
			gpointer   data)
{
  XEvent *xevent;
  GtkLayout *layout;

  xevent = (XEvent *)gdk_xevent;
  layout = GTK_LAYOUT (data);

  if (xevent->type == VisibilityNotify)
    {
      switch (xevent->xvisibility.state)
	{
	case VisibilityFullyObscured:
	  layout->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
	  break;

	case VisibilityPartiallyObscured:
	  layout->visibility = GDK_VISIBILITY_PARTIAL;
	  break;

	case VisibilityUnobscured:
	  layout->visibility = GDK_VISIBILITY_UNOBSCURED;
	  break;
	}

      return GDK_FILTER_REMOVE;
    }

  
  return GDK_FILTER_CONTINUE;
}

