/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

#include "gtkbutton.h"
#include "gtktogglebutton.h"
#include "gtkradiobutton.h"
#include "gtklabel.h"
#include "gtkvbox.h"
#include "gtktoolbar.h"


#define DEFAULT_SPACE_SIZE 5


enum {
  ORIENTATION_CHANGED,
  STYLE_CHANGED,
  LAST_SIGNAL
};

static void gtk_toolbar_class_init               (GtkToolbarClass *class);
static void gtk_toolbar_init                     (GtkToolbar      *toolbar);
static void gtk_toolbar_destroy                  (GtkObject       *object);
static void gtk_toolbar_map                      (GtkWidget       *widget);
static void gtk_toolbar_unmap                    (GtkWidget       *widget);
static void gtk_toolbar_draw                     (GtkWidget       *widget,
				                  GdkRectangle    *area);
static gint gtk_toolbar_expose                   (GtkWidget       *widget,
						  GdkEventExpose  *event);
static void gtk_toolbar_size_request             (GtkWidget       *widget,
				                  GtkRequisition  *requisition);
static void gtk_toolbar_size_allocate            (GtkWidget       *widget,
				                  GtkAllocation   *allocation);
static void gtk_toolbar_add                      (GtkContainer    *container,
				                  GtkWidget       *widget);
static void gtk_toolbar_remove                   (GtkContainer    *container,
						  GtkWidget       *widget);
static void gtk_toolbar_foreach                  (GtkContainer    *container,
				                  GtkCallback      callback,
				                  gpointer         callback_data);
static void gtk_real_toolbar_orientation_changed (GtkToolbar      *toolbar,
						  GtkOrientation   orientation);
static void gtk_real_toolbar_style_changed       (GtkToolbar      *toolbar,
						  GtkToolbarStyle  style);


static GtkContainerClass *parent_class;

static guint toolbar_signals[LAST_SIGNAL] = { 0 };


guint
gtk_toolbar_get_type (void)
{
  static guint toolbar_type = 0;

  if (!toolbar_type)
    {
      GtkTypeInfo toolbar_info =
      {
	"GtkToolbar",
	sizeof (GtkToolbar),
	sizeof (GtkToolbarClass),
	(GtkClassInitFunc) gtk_toolbar_class_init,
	(GtkObjectInitFunc) gtk_toolbar_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      toolbar_type = gtk_type_unique (gtk_container_get_type (), &toolbar_info);
    }

  return toolbar_type;
}

static void
gtk_toolbar_class_init (GtkToolbarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  toolbar_signals[ORIENTATION_CHANGED] =
    gtk_signal_new ("orientation_changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkToolbarClass, orientation_changed),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);
  toolbar_signals[STYLE_CHANGED] =
    gtk_signal_new ("style_changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkToolbarClass, style_changed),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, toolbar_signals, LAST_SIGNAL);

  object_class->destroy = gtk_toolbar_destroy;

  widget_class->map = gtk_toolbar_map;
  widget_class->unmap = gtk_toolbar_unmap;
  widget_class->draw = gtk_toolbar_draw;
  widget_class->expose_event = gtk_toolbar_expose;
  widget_class->size_request = gtk_toolbar_size_request;
  widget_class->size_allocate = gtk_toolbar_size_allocate;

  container_class->add = gtk_toolbar_add;
  container_class->remove = gtk_toolbar_remove;
  container_class->foreach = gtk_toolbar_foreach;
  container_class->focus = NULL;

  class->orientation_changed = gtk_real_toolbar_orientation_changed;
  class->style_changed = gtk_real_toolbar_style_changed;
}

static void
gtk_toolbar_init (GtkToolbar *toolbar)
{
  GTK_WIDGET_SET_FLAGS (toolbar, GTK_NO_WINDOW);
  GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_CAN_FOCUS);

  toolbar->num_children = 0;
  toolbar->children     = NULL;
  toolbar->orientation  = GTK_ORIENTATION_HORIZONTAL;
  toolbar->style        = GTK_TOOLBAR_ICONS;
  toolbar->relief       = GTK_RELIEF_NORMAL;
  toolbar->space_size   = DEFAULT_SPACE_SIZE;
  toolbar->tooltips     = gtk_tooltips_new ();
  toolbar->button_maxw  = 0;
  toolbar->button_maxh  = 0;
}

GtkWidget *
gtk_toolbar_new (GtkOrientation  orientation,
		 GtkToolbarStyle style)
{
  GtkToolbar *toolbar;

  toolbar = gtk_type_new (gtk_toolbar_get_type ());

  toolbar->orientation = orientation;
  toolbar->style = style;

  return GTK_WIDGET (toolbar);
}

static void
gtk_toolbar_destroy (GtkObject *object)
{
  GtkToolbar *toolbar;
  GList *children;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (object));

  toolbar = GTK_TOOLBAR (object);

  gtk_object_unref (GTK_OBJECT (toolbar->tooltips));
  toolbar->tooltips = NULL;

  for (children = toolbar->children; children; children = children->next)
    {
      GtkToolbarChild *child;

      child = children->data;

      if (child->type != GTK_TOOLBAR_CHILD_SPACE)
	{
	  gtk_widget_ref (child->widget);
	  gtk_widget_unparent (child->widget);
	  gtk_widget_destroy (child->widget);
	  gtk_widget_unref (child->widget);
	}

      g_free (child);
    }

  g_list_free (toolbar->children);
  toolbar->children = NULL;
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_toolbar_map (GtkWidget *widget)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (widget));

  toolbar = GTK_TOOLBAR (widget);
  GTK_WIDGET_SET_FLAGS (toolbar, GTK_MAPPED);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if ((child->type != GTK_TOOLBAR_CHILD_SPACE)
	  && GTK_WIDGET_VISIBLE (child->widget) && !GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_map (child->widget);
    }
}

static void
gtk_toolbar_unmap (GtkWidget *widget)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (widget));

  toolbar = GTK_TOOLBAR (widget);
  GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_MAPPED);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if ((child->type != GTK_TOOLBAR_CHILD_SPACE)
	  && GTK_WIDGET_VISIBLE (child->widget) && GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_unmap (child->widget);
    }
}

static void
gtk_toolbar_draw (GtkWidget    *widget,
		  GdkRectangle *area)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      toolbar = GTK_TOOLBAR (widget);

      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if ((child->type != GTK_TOOLBAR_CHILD_SPACE)
	      && gtk_widget_intersect (child->widget, area, &child_area))
	    gtk_widget_draw (child->widget, &child_area);
	}
    }
}

static gint
gtk_toolbar_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TOOLBAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      toolbar = GTK_TOOLBAR (widget);

      child_event = *event;

      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if ((child->type != GTK_TOOLBAR_CHILD_SPACE)
	      && GTK_WIDGET_NO_WINDOW (child->widget)
	      && gtk_widget_intersect (child->widget, &event->area, &child_event.area))
	    gtk_widget_event (child->widget, (GdkEvent *) &child_event);
	}
    }

  return FALSE;
}

static void
gtk_toolbar_size_request (GtkWidget      *widget,
			  GtkRequisition *requisition)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  gint nbuttons;
  gint button_maxw, button_maxh;
  gint widget_maxw, widget_maxh;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (requisition != NULL);

  toolbar = GTK_TOOLBAR (widget);

  requisition->width = GTK_CONTAINER (toolbar)->border_width * 2;
  requisition->height = GTK_CONTAINER (toolbar)->border_width * 2;
  nbuttons = 0;
  button_maxw = 0;
  button_maxh = 0;
  widget_maxw = 0;
  widget_maxh = 0;

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    requisition->width += toolbar->space_size;
	  else
	    requisition->height += toolbar->space_size;

	  break;

	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {
	      gtk_widget_size_request (child->widget, &child->widget->requisition);

	      nbuttons++;
	      button_maxw = MAX (button_maxw, child->widget->requisition.width);
	      button_maxh = MAX (button_maxh, child->widget->requisition.height);
	    }

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {
	      gtk_widget_size_request (child->widget, &child->widget->requisition);

	      widget_maxw = MAX (widget_maxw, child->widget->requisition.width);
	      widget_maxh = MAX (widget_maxh, child->widget->requisition.height);

	      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		requisition->width += child->widget->requisition.width;
	      else
		requisition->height += child->widget->requisition.height;
	    }

	  break;

	default:
	  g_assert_not_reached ();
	}
    }

  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width += nbuttons * button_maxw;
      requisition->height += MAX (button_maxh, widget_maxh);
    }
  else
    {
      requisition->width += MAX (button_maxw, widget_maxw);
      requisition->height += nbuttons * button_maxh;
    }

  toolbar->button_maxw = button_maxw;
  toolbar->button_maxh = button_maxh;
}

static void
gtk_toolbar_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  GtkAllocation alloc;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (allocation != NULL);

  toolbar = GTK_TOOLBAR (widget);
  widget->allocation = *allocation;

  border_width = GTK_CONTAINER (toolbar)->border_width;

  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    alloc.x = allocation->x + border_width;
  else
    alloc.y = allocation->y + border_width;

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += toolbar->space_size;
	  else
	    alloc.y += toolbar->space_size;

	  break;

	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  alloc.width = toolbar->button_maxw;
	  alloc.height = toolbar->button_maxh;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.y = allocation->y + (allocation->height - toolbar->button_maxh) / 2;
	  else
	    alloc.x = allocation->x + (allocation->width - toolbar->button_maxw) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += toolbar->button_maxw;
	  else
	    alloc.y += toolbar->button_maxh;

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  alloc.width = child->widget->requisition.width;
	  alloc.height = child->widget->requisition.height;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.y = allocation->y + (allocation->height - child->widget->requisition.height) / 2;
	  else
	    alloc.x = allocation->x + (allocation->width - child->widget->requisition.width) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    alloc.x += child->widget->requisition.width;
	  else
	    alloc.y += child->widget->requisition.height;

	  break;

	default:
	  g_assert_not_reached ();
	}
    }
}

static void
gtk_toolbar_add (GtkContainer *container,
		 GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  gtk_toolbar_append_widget (GTK_TOOLBAR (container), widget, NULL, NULL);
}

static void
gtk_toolbar_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  toolbar = GTK_TOOLBAR (container);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if ((child->type != GTK_TOOLBAR_CHILD_SPACE) && (child->widget == widget))
	{
	  gboolean was_visible;

	  was_visible = GTK_WIDGET_VISIBLE (widget);
	  gtk_widget_unparent (widget);

	  toolbar->children = g_list_remove_link (toolbar->children, children);
	  g_free (child);
	  g_list_free (children);
	  toolbar->num_children--;

	  if (was_visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}
    }
}

static void
gtk_toolbar_foreach (GtkContainer *container,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (callback != NULL);

  toolbar = GTK_TOOLBAR (container);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if (child->type != GTK_TOOLBAR_CHILD_SPACE)
	(*callback) (child->widget, callback_data);
    }
}

GtkWidget *
gtk_toolbar_append_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
gtk_toolbar_prepend_item (GtkToolbar    *toolbar,
			  const char    *text,
			  const char    *tooltip_text,
			  const char    *tooltip_private_text,
			  GtkWidget     *icon,
			  GtkSignalFunc  callback,
			  gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     0);
}

GtkWidget *
gtk_toolbar_insert_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data,
			 gint           position)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     position);
}

void
gtk_toolbar_append_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
gtk_toolbar_prepend_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      0);
}

void
gtk_toolbar_insert_space (GtkToolbar *toolbar,
			  gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      position);
}

void
gtk_toolbar_append_widget (GtkToolbar  *toolbar,
			   GtkWidget   *widget,
			   const gchar *tooltip_text,
			   const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
gtk_toolbar_prepend_widget (GtkToolbar  *toolbar,
			    GtkWidget   *widget,
			    const gchar *tooltip_text,
			    const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
gtk_toolbar_insert_widget (GtkToolbar *toolbar,
			   GtkWidget  *widget,
			   const char *tooltip_text,
			   const char *tooltip_private_text,
			   gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      position);
}

GtkWidget *
gtk_toolbar_append_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
gtk_toolbar_prepend_element (GtkToolbar          *toolbar,
			     GtkToolbarChildType  type,
			     GtkWidget           *widget,
			     const char          *text,
			     const char          *tooltip_text,
			     const char          *tooltip_private_text,
			     GtkWidget           *icon,
			     GtkSignalFunc        callback,
			     gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data, 0);
}

GtkWidget *
gtk_toolbar_insert_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data,
			    gint                 position)
{
  GtkToolbarChild *child;
  GtkWidget *vbox;

  g_return_val_if_fail (toolbar != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);
  g_return_val_if_fail ((type != GTK_TOOLBAR_CHILD_WIDGET) || (widget != NULL), NULL);

  child = g_new (GtkToolbarChild, 1);
  child->type = type;
  child->icon = NULL;
  child->label = NULL;

  switch (type)
    {
    case GTK_TOOLBAR_CHILD_SPACE:
      child->widget = NULL;
      break;

    case GTK_TOOLBAR_CHILD_WIDGET:
      child->widget = widget;
      break;

    case GTK_TOOLBAR_CHILD_BUTTON:
    case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
    case GTK_TOOLBAR_CHILD_RADIOBUTTON:
      if (type == GTK_TOOLBAR_CHILD_BUTTON)
	{
	  child->widget = gtk_button_new ();
	  gtk_button_set_relief (GTK_BUTTON (child->widget), toolbar->relief);
	}
      else if (type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
	{
	  child->widget = gtk_toggle_button_new ();
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (child->widget),
				      FALSE);
	}
      else
	{
	  child->widget = gtk_radio_button_new (widget
						? gtk_radio_button_group (GTK_RADIO_BUTTON (widget))
						: NULL);
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (child->widget), FALSE);
	}

      if (callback)
	gtk_signal_connect (GTK_OBJECT (child->widget), "clicked",
			    callback, user_data);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (child->widget), vbox);
      gtk_widget_show (vbox);

      if (text)
	{
	  child->label = gtk_label_new (text);
	  gtk_box_pack_end (GTK_BOX (vbox), child->label, FALSE, FALSE, 0);
	  if (toolbar->style != GTK_TOOLBAR_ICONS)
	    gtk_widget_show (child->label);
	}

      if (icon)
	{
	  child->icon = GTK_WIDGET (icon);
	  gtk_box_pack_end (GTK_BOX (vbox), child->icon, FALSE, FALSE, 0);
	  if (toolbar->style != GTK_TOOLBAR_TEXT)
	    gtk_widget_show (child->icon);
	}

      gtk_widget_show (child->widget);
      break;

    default:
      g_assert_not_reached ();
    }

  if ((type != GTK_TOOLBAR_CHILD_SPACE) && tooltip_text)
    gtk_tooltips_set_tip (toolbar->tooltips, child->widget,
			  tooltip_text, tooltip_private_text);

  toolbar->children = g_list_insert (toolbar->children, child, position);
  toolbar->num_children++;

  if (type != GTK_TOOLBAR_CHILD_SPACE)
    gtk_widget_set_parent (child->widget, GTK_WIDGET (toolbar));

  if ((type != GTK_TOOLBAR_CHILD_SPACE) && GTK_WIDGET_VISIBLE (toolbar))
    {
      if (GTK_WIDGET_REALIZED (toolbar)
	  && !GTK_WIDGET_REALIZED (child->widget))
	gtk_widget_realize (child->widget);
	
      if (GTK_WIDGET_MAPPED (toolbar)
	  && !GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_map (child->widget);
    }

  if (GTK_WIDGET_VISIBLE (toolbar) &&
      ((type == GTK_TOOLBAR_CHILD_SPACE) ||
       GTK_WIDGET_VISIBLE (child->widget)))
    gtk_widget_queue_resize (GTK_WIDGET (toolbar));

  return child->widget;
}

void
gtk_toolbar_set_orientation (GtkToolbar     *toolbar,
			     GtkOrientation  orientation)
{
  gtk_signal_emit (GTK_OBJECT (toolbar), toolbar_signals[ORIENTATION_CHANGED], orientation);
}

void
gtk_toolbar_set_style (GtkToolbar      *toolbar,
		       GtkToolbarStyle  style)
{
  gtk_signal_emit (GTK_OBJECT (toolbar), toolbar_signals[STYLE_CHANGED], style);
}

void
gtk_toolbar_set_space_size (GtkToolbar *toolbar,
			    gint        space_size)
{
  g_return_if_fail (toolbar != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->space_size != space_size)
    {
      toolbar->space_size = space_size;
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
    }
}

void
gtk_toolbar_set_tooltips (GtkToolbar *toolbar,
			  gint        enable)
{
  g_return_if_fail (toolbar != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (enable)
    gtk_tooltips_enable (toolbar->tooltips);
  else
    gtk_tooltips_disable (toolbar->tooltips);
}

void
gtk_toolbar_set_button_relief (GtkToolbar *toolbar,
			       GtkReliefStyle relief)
{
  GList *children;
  GtkToolbarChild *child;
  
  g_return_if_fail (toolbar != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->relief != relief)
    {
      toolbar->relief = relief;
      
      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;
	  if (child->type == GTK_TOOLBAR_CHILD_BUTTON)
	    gtk_button_set_relief (GTK_BUTTON (child->widget), relief);
	}
      
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
    }
}

GtkReliefStyle
gtk_toolbar_get_button_relief (GtkToolbar      *toolbar)
{
  g_return_val_if_fail (toolbar != NULL, GTK_RELIEF_NORMAL);
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_RELIEF_NORMAL);

  return toolbar->relief;
}

static void
gtk_real_toolbar_orientation_changed (GtkToolbar     *toolbar,
				      GtkOrientation  orientation)
{
  g_return_if_fail (toolbar != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->orientation != orientation)
    {
      toolbar->orientation = orientation;
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
    }
}

static void
gtk_real_toolbar_style_changed (GtkToolbar      *toolbar,
				GtkToolbarStyle  style)
{
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (toolbar != NULL);
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->style != style)
    {
      toolbar->style = style;

      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if (child->type == GTK_TOOLBAR_CHILD_BUTTON ||
	      child->type == GTK_TOOLBAR_CHILD_RADIOBUTTON ||
	      child->type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
	    switch (style)
	      {
	      case GTK_TOOLBAR_ICONS:
		if (child->icon && !GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_show (child->icon);

		if (child->label && GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_hide (child->label);

		break;

	      case GTK_TOOLBAR_TEXT:
		if (child->icon && GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_hide (child->icon);

		if (child->label && !GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_show (child->label);

		break;

	      case GTK_TOOLBAR_BOTH:
		if (child->icon && !GTK_WIDGET_VISIBLE (child->icon))
		  gtk_widget_show (child->icon);

		if (child->label && !GTK_WIDGET_VISIBLE (child->label))
		  gtk_widget_show (child->label);

		break;

	      default:
		g_assert_not_reached ();
	      }
	}
		
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
    }
}
