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
#include "gtklist.h"
#include "gtklistitem.h"
#include "gtkmain.h"
#include "gtksignal.h"


enum {
  SELECTION_CHANGED,
  SELECT_CHILD,
  UNSELECT_CHILD,
  LAST_SIGNAL
};


typedef void (*GtkListSignal) (GtkObject *object,
			       gpointer   arg1,
			       gpointer   data);


static void gtk_list_class_init      (GtkListClass   *klass);
static void gtk_list_init            (GtkList        *list);
static void gtk_list_destroy         (GtkObject      *object);
static void gtk_list_map             (GtkWidget      *widget);
static void gtk_list_unmap           (GtkWidget      *widget);
static void gtk_list_realize         (GtkWidget      *widget);
static void gtk_list_draw            (GtkWidget      *widget,
				      GdkRectangle   *area);
static gint gtk_list_expose          (GtkWidget      *widget,
				      GdkEventExpose *event);
static gint gtk_list_motion_notify   (GtkWidget      *widget,
				      GdkEventMotion *event);
static gint gtk_list_button_press    (GtkWidget      *widget,
				      GdkEventButton *event);
static gint gtk_list_button_release  (GtkWidget      *widget,
				      GdkEventButton *event);
static void gtk_list_size_request    (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void gtk_list_size_allocate   (GtkWidget      *widget,
				      GtkAllocation  *allocation);
static void gtk_list_add             (GtkContainer   *container,
				      GtkWidget      *widget);
static void gtk_list_remove          (GtkContainer   *container,
				      GtkWidget      *widget);
static void gtk_list_foreach         (GtkContainer   *container,
				      GtkCallback     callback,
				      gpointer        callback_data);

static void gtk_real_list_select_child   (GtkList       *list,
					  GtkWidget     *child);
static void gtk_real_list_unselect_child (GtkList       *list,
					  GtkWidget     *child);

static void gtk_list_marshal_signal (GtkObject      *object,
				     GtkSignalFunc   func,
				     gpointer        func_data,
				     GtkArg         *args);


static GtkContainerClass *parent_class = NULL;
static guint list_signals[LAST_SIGNAL] = { 0 };


guint
gtk_list_get_type ()
{
  static guint list_type = 0;

  if (!list_type)
    {
      GtkTypeInfo list_info =
      {
	"GtkList",
	sizeof (GtkList),
	sizeof (GtkListClass),
	(GtkClassInitFunc) gtk_list_class_init,
	(GtkObjectInitFunc) gtk_list_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      list_type = gtk_type_unique (gtk_container_get_type (), &list_info);
    }

  return list_type;
}

static void
gtk_list_class_init (GtkListClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  list_signals[SELECTION_CHANGED] =
    gtk_signal_new ("selection_changed",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkListClass, selection_changed),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  list_signals[SELECT_CHILD] =
    gtk_signal_new ("select_child",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkListClass, select_child),
                    gtk_list_marshal_signal,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);
  list_signals[UNSELECT_CHILD] =
    gtk_signal_new ("unselect_child",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkListClass, unselect_child),
                    gtk_list_marshal_signal,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_WIDGET);

  gtk_object_class_add_signals (object_class, list_signals, LAST_SIGNAL);

  object_class->destroy = gtk_list_destroy;

  widget_class->map = gtk_list_map;
  widget_class->unmap = gtk_list_unmap;
  widget_class->realize = gtk_list_realize;
  widget_class->draw = gtk_list_draw;
  widget_class->expose_event = gtk_list_expose;
  widget_class->motion_notify_event = gtk_list_motion_notify;
  widget_class->button_press_event = gtk_list_button_press;
  widget_class->button_release_event = gtk_list_button_release;
  widget_class->size_request = gtk_list_size_request;
  widget_class->size_allocate = gtk_list_size_allocate;

  container_class->add = gtk_list_add;
  container_class->remove = gtk_list_remove;
  container_class->foreach = gtk_list_foreach;

  class->selection_changed = NULL;
  class->select_child = gtk_real_list_select_child;
  class->unselect_child = gtk_real_list_unselect_child;
}

static void
gtk_list_init (GtkList *list)
{
  list->children = NULL;
  list->selection = NULL;
  list->timer = 0;
  list->selection_start_pos = 0;
  list->selection_end_pos = 0;
  list->selection_mode = GTK_SELECTION_SINGLE;
  list->scroll_direction = 0;
  list->have_grab = FALSE;
}

GtkWidget*
gtk_list_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_list_get_type ()));
}

static void
gtk_list_destroy (GtkObject *object)
{
  GList *node;

  GtkList *list = GTK_LIST (object);

  for (node = list->children; node; node = node->next)
    {
      GtkWidget *child;

      child = (GtkWidget *)node->data;
      gtk_widget_ref (child);
      gtk_widget_unparent (child);
      gtk_widget_destroy (child);
      gtk_widget_unref (child);
    }
  g_list_free (list->children);
  list->children = NULL;

  for (node = list->selection; node; node = node->next)
    {
      GtkWidget *child;

      child = (GtkWidget *)node->data;
      gtk_widget_unref (child);
    }
  g_list_free (list->selection);
  list->selection = NULL;

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

void
gtk_list_insert_items (GtkList *list,
		       GList   *items,
		       gint     position)
{
  GtkWidget *widget;
  GList *tmp_list;
  GList *last;
  gint nchildren;

  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  if (!items)
    return;

  tmp_list = items;
  while (tmp_list)
    {
      widget = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_widget_set_parent (widget, GTK_WIDGET (list));

      if (GTK_WIDGET_VISIBLE (widget->parent))
	{
	  if (GTK_WIDGET_REALIZED (widget->parent) &&
	      !GTK_WIDGET_REALIZED (widget))
	    gtk_widget_realize (widget);

	  if (GTK_WIDGET_MAPPED (widget->parent) &&
	      !GTK_WIDGET_MAPPED (widget))
	    gtk_widget_map (widget);
	}
    }

  nchildren = g_list_length (list->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  if (position == nchildren)
    {
      if (list->children)
	{
	  tmp_list = g_list_last (list->children);
	  tmp_list->next = items;
	  items->prev = tmp_list;
	}
      else
	{
	  list->children = items;
	}
    }
  else
    {
      tmp_list = g_list_nth (list->children, position);
      last = g_list_last (items);

      if (tmp_list->prev)
	tmp_list->prev->next = items;
      last->next = tmp_list;
      items->prev = tmp_list->prev;
      tmp_list->prev = last;

      if (tmp_list == list->children)
	list->children = items;
    }

  if (list->children && !list->selection &&
      (list->selection_mode == GTK_SELECTION_BROWSE))
    {
      widget = list->children->data;
      gtk_list_select_child (list, widget);
    }

  if (GTK_WIDGET_VISIBLE (list))
    gtk_widget_queue_resize (GTK_WIDGET (list));
}

void
gtk_list_append_items (GtkList *list,
		       GList   *items)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  gtk_list_insert_items (list, items, -1);
}

void
gtk_list_prepend_items (GtkList *list,
			GList   *items)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  gtk_list_insert_items (list, items, 0);
}

static void
gtk_list_remove_items_internal (GtkList  *list,
				GList    *items,
				gboolean no_unref)
{
  GtkWidget *widget;
  GList *selected_widgets;
  GList *tmp_list;
  
  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));
  
  tmp_list = items;
  selected_widgets = NULL;
  widget = NULL;
  
  while (tmp_list)
    {
      widget = tmp_list->data;
      tmp_list = tmp_list->next;
      
      if (widget->state == GTK_STATE_SELECTED)
	selected_widgets = g_list_prepend (selected_widgets, widget);
      
      list->children = g_list_remove (list->children, widget);
      
      if (GTK_WIDGET_MAPPED (widget))
	gtk_widget_unmap (widget);
      
      if (no_unref)
	gtk_widget_ref (widget);
      gtk_widget_unparent (widget);
    }
  
  if (selected_widgets)
    {
      tmp_list = selected_widgets;
      while (tmp_list)
	{
	  widget = tmp_list->data;
	  tmp_list = tmp_list->next;
	  
	  gtk_list_unselect_child (list, widget);
	}
      
      gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
    }
  
  g_list_free (selected_widgets);
  
  if (list->children && !list->selection &&
      (list->selection_mode == GTK_SELECTION_BROWSE))
    {
      widget = list->children->data;
      gtk_list_select_child (list, widget);
    }
  
  if (GTK_WIDGET_VISIBLE (list))
    gtk_widget_queue_resize (GTK_WIDGET (list));
}

void
gtk_list_remove_items (GtkList  *list,
		       GList    *items)
{
  gtk_list_remove_items_internal (list, items, FALSE);
}

void
gtk_list_remove_items_no_unref (GtkList  *list,
				GList    *items)
{
  gtk_list_remove_items_internal (list, items, TRUE);
}

void
gtk_list_clear_items (GtkList *list,
		      gint     start,
		      gint     end)
{
  GtkWidget *widget;
  GList *start_list;
  GList *end_list;
  GList *tmp_list;
  guint nchildren;
  gboolean selection_changed;

  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  nchildren = g_list_length (list->children);

  if (nchildren > 0)
    {
      if ((end < 0) || (end > nchildren))
	end = nchildren;

      if (start >= end)
	return;

      start_list = g_list_nth (list->children, start);
      end_list = g_list_nth (list->children, end);

      if (start_list->prev)
	start_list->prev->next = end_list;
      if (end_list && end_list->prev)
        end_list->prev->next = NULL;
      if (end_list)
        end_list->prev = start_list->prev;
      if (start_list == list->children)
        list->children = end_list;

      selection_changed = FALSE;
      widget = NULL;
      tmp_list = start_list;

      while (tmp_list)
	{
	  widget = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (widget->state == GTK_STATE_SELECTED)
	    {
	      selection_changed = TRUE;
	      list->selection = g_list_remove (list->selection, widget);
	      gtk_widget_unref (widget);
	    }

	  gtk_widget_unparent (widget);
	}

      g_list_free (start_list);

      if (list->children && !list->selection &&
	  (list->selection_mode == GTK_SELECTION_BROWSE))
	{
	  widget = list->children->data;
	  gtk_list_select_child (list, widget);
	}

      if (selection_changed)
	gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);

      gtk_widget_queue_resize (GTK_WIDGET (list));
    }
}

void
gtk_list_select_item (GtkList *list,
		      gint     item)
{
  GList *tmp_list;

  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  tmp_list = g_list_nth (list->children, item);
  if (tmp_list)
    gtk_list_select_child (list, GTK_WIDGET (tmp_list->data));
}

void
gtk_list_unselect_item (GtkList *list,
			gint     item)
{
  GList *tmp_list;

  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  tmp_list = g_list_nth (list->children, item);
  if (tmp_list)
    gtk_list_unselect_child (list, GTK_WIDGET (tmp_list->data));
}

void
gtk_list_select_child (GtkList   *list,
		       GtkWidget *child)
{
  gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECT_CHILD], child);
}

void
gtk_list_unselect_child (GtkList   *list,
			 GtkWidget *child)
{
  gtk_signal_emit (GTK_OBJECT (list), list_signals[UNSELECT_CHILD], child);
}

gint
gtk_list_child_position (GtkList   *list,
			 GtkWidget *child)
{
  GList *children;
  gint pos;

  g_return_val_if_fail (list != NULL, -1);
  g_return_val_if_fail (GTK_IS_LIST (list), -1);
  g_return_val_if_fail (child != NULL, -1);

  pos = 0;
  children = list->children;

  while (children)
    {
      if (child == GTK_WIDGET (children->data))
	return pos;

      pos += 1;
      children = children->next;
    }

  return -1;
}

void
gtk_list_set_selection_mode (GtkList          *list,
			     GtkSelectionMode  mode)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));

  list->selection_mode = mode;
}


static void
gtk_list_map (GtkWidget *widget)
{
  GtkList *list;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  list = GTK_LIST (widget);

  gdk_window_show (widget->window);

  children = list->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child) &&
	  !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }
}

static void
gtk_list_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_hide (widget->window);
}

static void
gtk_list_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

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
  gdk_window_set_background (widget->window, 
			     &widget->style->base[GTK_STATE_NORMAL]);
}

static void
gtk_list_draw (GtkWidget    *widget,
	       GdkRectangle *area)
{
  GtkList *list;
  GtkWidget *child;
  GdkRectangle child_area;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      list = GTK_LIST (widget);

      children = list->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_intersect (child, area, &child_area))
	    gtk_widget_draw (child, &child_area);
	}
    }
}

static gint
gtk_list_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  GtkList *list;
  GtkWidget *child;
  GdkEventExpose child_event;
  GList *children;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      list = GTK_LIST (widget);

      child_event = *event;

      children = list->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_NO_WINDOW (child) &&
	      gtk_widget_intersect (child, &event->area, &child_event.area))
	    gtk_widget_event (child, (GdkEvent*) &child_event);
	}
    }

  return FALSE;
}

static gint
gtk_list_motion_notify (GtkWidget      *widget,
			GdkEventMotion *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  g_print ("gtk_list_motion_notify\n");

  return FALSE;
}

static gint
gtk_list_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GtkList *list;
  GtkWidget *item;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  list = GTK_LIST (widget);
  item = gtk_get_event_widget ((GdkEvent*) event);
  
  while (item && !GTK_IS_LIST_ITEM (item))
    item = item->parent;

  if (!item || (item->parent != widget))
    return FALSE;
  
  gtk_list_select_child (list, item);

  return FALSE;
}

static gint
gtk_list_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkList *list;
  GtkWidget *item;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  list = GTK_LIST (widget);
  item = gtk_get_event_widget ((GdkEvent*) event);

  return FALSE;
}

static void
gtk_list_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkList *list;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST (widget));
  g_return_if_fail (requisition != NULL);

  list = GTK_LIST (widget);
  requisition->width = 0;
  requisition->height = 0;

  children = list->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_size_request (child, &child->requisition);

	  requisition->width = MAX (requisition->width, child->requisition.width);
	  requisition->height += child->requisition.height;
	}
    }

  requisition->width += GTK_CONTAINER (list)->border_width * 2;
  requisition->height += GTK_CONTAINER (list)->border_width * 2;

  requisition->width = MAX (requisition->width, 1);
  requisition->height = MAX (requisition->height, 1);
}

static void
gtk_list_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkList *list;
  GtkWidget *child;
  GtkAllocation child_allocation;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST (widget));
  g_return_if_fail (allocation != NULL);

  list = GTK_LIST (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  if (list->children)
    {
      child_allocation.x = GTK_CONTAINER (list)->border_width;
      child_allocation.y = GTK_CONTAINER (list)->border_width;
      child_allocation.width = allocation->width - child_allocation.x * 2;

      children = list->children;

      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      child_allocation.height = child->requisition.height;

	      gtk_widget_size_allocate (child, &child_allocation);

	      child_allocation.y += child_allocation.height;
	    }
	}
    }
}

static void
gtk_list_add (GtkContainer *container,
	      GtkWidget    *widget)
{
  GtkList *list;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LIST (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (widget));

  list = GTK_LIST (container);

  gtk_widget_set_parent (widget, GTK_WIDGET (container));
  if (GTK_WIDGET_VISIBLE (widget->parent))
    {
      if (GTK_WIDGET_REALIZED (widget->parent) &&
	  !GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);

      if (GTK_WIDGET_MAPPED (widget->parent) &&
	  !GTK_WIDGET_MAPPED (widget))
	gtk_widget_map (widget);
    }

  list->children = g_list_append (list->children, widget);

  if (!list->selection && (list->selection_mode == GTK_SELECTION_BROWSE))
    gtk_list_select_child (list, widget);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
    gtk_widget_queue_resize (widget);
}

static void
gtk_list_remove (GtkContainer *container,
		 GtkWidget    *widget)
{
  GList *item_list;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LIST (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (container == GTK_CONTAINER (widget->parent));
  
  
  item_list = g_list_alloc ();
  item_list->data = widget;
  
  gtk_list_remove_items (GTK_LIST (container), item_list);
  
  g_list_free (item_list);
}

static void
gtk_list_foreach (GtkContainer *container,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkList *list;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_LIST (container));
  g_return_if_fail (callback != NULL);

  list = GTK_LIST (container);
  children = list->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child, callback_data);
    }
}


static void
gtk_real_list_select_child (GtkList   *list,
			    GtkWidget *child)
{
  GList *selection;
  GList *tmp_list;
  GtkWidget *tmp_item;

  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (child));

  switch (list->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
      selection = list->selection;

      while (selection)
	{
	  tmp_item = selection->data;

	  if (tmp_item != child)
	    {
	      gtk_list_item_deselect (GTK_LIST_ITEM (tmp_item));
	      
	      tmp_list = selection;
	      selection = selection->next;

	      list->selection = g_list_remove_link (list->selection, tmp_list);
	      gtk_widget_unref (GTK_WIDGET (tmp_item));

	      g_list_free (tmp_list);
	    }
	  else
	    selection = selection->next;
	}

      if (child->state == GTK_STATE_NORMAL)
	{
	  gtk_list_item_select (GTK_LIST_ITEM (child));
	  list->selection = g_list_prepend (list->selection, child);
	  gtk_widget_ref (child);
	}
      else if (child->state == GTK_STATE_SELECTED)
	{
	  gtk_list_item_deselect (GTK_LIST_ITEM (child));
	  list->selection = g_list_remove (list->selection, child);
	  gtk_widget_unref (child);
	}

      gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
      break;

    case GTK_SELECTION_BROWSE:
      selection = list->selection;

      while (selection)
	{
	  tmp_item = selection->data;

	  if (tmp_item != child)
	    {
	      gtk_list_item_deselect (GTK_LIST_ITEM (tmp_item));
	      
	      tmp_list = selection;
	      selection = selection->next;

	      list->selection = g_list_remove_link (list->selection, tmp_list);
	      gtk_widget_unref (GTK_WIDGET (tmp_item));

	      g_list_free (tmp_list);
	    }
	  else
	    selection = selection->next;
	}

      if (child->state == GTK_STATE_NORMAL)
	{
	  gtk_list_item_select (GTK_LIST_ITEM (child));
	  list->selection = g_list_prepend (list->selection, child);
	  gtk_widget_ref (child);
	  gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
	}
      break;

    case GTK_SELECTION_MULTIPLE:
      if (child->state == GTK_STATE_NORMAL)
	{
	  gtk_list_item_select (GTK_LIST_ITEM (child));
	  list->selection = g_list_prepend (list->selection, child);
	  gtk_widget_ref (child);
	  gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
	}
      else if (child->state == GTK_STATE_SELECTED)
	{
	  gtk_list_item_deselect (GTK_LIST_ITEM (child));
	  list->selection = g_list_remove (list->selection, child);
	  gtk_widget_unref (child);
	  gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
	}
      break;

    case GTK_SELECTION_EXTENDED:
      break;
    }
}

static void
gtk_real_list_unselect_child (GtkList   *list,
			      GtkWidget *child)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (GTK_IS_LIST (list));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (child));

  switch (list->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_MULTIPLE:
    case GTK_SELECTION_BROWSE:
      if (child->state == GTK_STATE_SELECTED)
	{
	  gtk_list_item_deselect (GTK_LIST_ITEM (child));
	  list->selection = g_list_remove (list->selection, child);
	  gtk_widget_unref (child);
	  gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
	}
      break;

    case GTK_SELECTION_EXTENDED:
      break;
    }
}


static void
gtk_list_marshal_signal (GtkObject      *object,
			 GtkSignalFunc   func,
			 gpointer        func_data,
			 GtkArg         *args)
{
  GtkListSignal rfunc;

  rfunc = (GtkListSignal) func;

  (* rfunc) (object, GTK_VALUE_OBJECT (args[0]), func_data);
}
