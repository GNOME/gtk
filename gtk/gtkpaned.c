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

#include "gtkpaned.h"
#include "gtkhpaned.h"

enum {
  ARG_0,
  ARG_HANDLE_SIZE,
  ARG_GUTTER_SIZE
};

static void gtk_paned_class_init (GtkPanedClass    *klass);
static void gtk_paned_init       (GtkPaned         *paned);
static void gtk_paned_set_arg	 (GtkObject        *object,
				  GtkArg           *arg,
				  guint             arg_id);
static void gtk_paned_get_arg	 (GtkObject        *object,
				  GtkArg           *arg,
				  guint             arg_id);
static void gtk_paned_realize    (GtkWidget *widget);
static void gtk_paned_map        (GtkWidget      *widget);
static void gtk_paned_unmap      (GtkWidget      *widget);
static void gtk_paned_unrealize  (GtkWidget *widget);
static gint gtk_paned_expose     (GtkWidget      *widget,
				  GdkEventExpose *event);
static void gtk_paned_add        (GtkContainer   *container,
				  GtkWidget      *widget);
static void gtk_paned_remove     (GtkContainer   *container,
				  GtkWidget      *widget);
static void gtk_paned_forall     (GtkContainer   *container,
			   	  gboolean	  include_internals,
			          GtkCallback     callback,
			          gpointer        callback_data);
static GtkType gtk_paned_child_type (GtkContainer *container);


static GtkContainerClass *parent_class = NULL;


GtkType
gtk_paned_get_type (void)
{
  static GtkType paned_type = 0;
  
  if (!paned_type)
    {
      static const GtkTypeInfo paned_info =
      {
	"GtkPaned",
	sizeof (GtkPaned),
	sizeof (GtkPanedClass),
	(GtkClassInitFunc) gtk_paned_class_init,
	(GtkObjectInitFunc) gtk_paned_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      paned_type = gtk_type_unique (GTK_TYPE_CONTAINER, &paned_info);
    }
  
  return paned_type;
}

static void
gtk_paned_class_init (GtkPanedClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);
  
  object_class->set_arg = gtk_paned_set_arg;
  object_class->get_arg = gtk_paned_get_arg;

  widget_class->realize = gtk_paned_realize;
  widget_class->map = gtk_paned_map;
  widget_class->unmap = gtk_paned_unmap;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->expose_event = gtk_paned_expose;
  
  container_class->add = gtk_paned_add;
  container_class->remove = gtk_paned_remove;
  container_class->forall = gtk_paned_forall;
  container_class->child_type = gtk_paned_child_type;

  gtk_object_add_arg_type ("GtkPaned::handle_size", GTK_TYPE_UINT,
			   GTK_ARG_READWRITE, ARG_HANDLE_SIZE);
  gtk_object_add_arg_type ("GtkPaned::gutter_size", GTK_TYPE_UINT,
			   GTK_ARG_READWRITE, ARG_GUTTER_SIZE);
}

static GtkType
gtk_paned_child_type (GtkContainer *container)
{
  if (!GTK_PANED (container)->child1 || !GTK_PANED (container)->child2)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
gtk_paned_init (GtkPaned *paned)
{
  GTK_WIDGET_UNSET_FLAGS (paned, GTK_NO_WINDOW);
  
  paned->child1 = NULL;
  paned->child2 = NULL;
  paned->handle = NULL;
  paned->xor_gc = NULL;
  
  paned->handle_size = 10;
  paned->gutter_size = 6;
  paned->position_set = FALSE;
  paned->last_allocation = -1;
  paned->in_drag = FALSE;
  
  paned->handle_xpos = -1;
  paned->handle_ypos = -1;
}

static void
gtk_paned_set_arg (GtkObject *object,
		   GtkArg    *arg,
		   guint      arg_id)
{
  GtkPaned *paned = GTK_PANED (object);
  
  switch (arg_id)
    {
    case ARG_HANDLE_SIZE:
      gtk_paned_set_handle_size (paned, GTK_VALUE_UINT (*arg));
      break;
    case ARG_GUTTER_SIZE:
      gtk_paned_set_gutter_size (paned, GTK_VALUE_UINT (*arg));
      break;
    }
}

static void
gtk_paned_get_arg (GtkObject *object,
		   GtkArg    *arg,
		   guint      arg_id)
{
  GtkPaned *paned = GTK_PANED (object);
  
  switch (arg_id)
    {
    case ARG_HANDLE_SIZE:
      GTK_VALUE_UINT (*arg) = paned->handle_size;
      break;
    case ARG_GUTTER_SIZE:
      GTK_VALUE_UINT (*arg) = paned->gutter_size;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_paned_realize (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;
  gboolean handle_full_size = _gtk_paned_is_handle_full_size (paned);
 
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, paned);

  if (handle_full_size)
    {
      GdkRectangle rect;

      _gtk_paned_get_handle_rect (paned, &rect);

      attributes.x = rect.x;
      attributes.y = rect.y;
      attributes.width = rect.width;
      attributes.height = rect.height;
    }
  else
    {
      attributes.x = paned->handle_xpos;
      attributes.y = paned->handle_ypos;
      attributes.width = paned->handle_size;
      attributes.height = paned->handle_size;
    }

  attributes.cursor = gdk_cursor_new (GTK_IS_HPANED (paned) ?
				      GDK_SB_H_DOUBLE_ARROW :
				      GDK_SB_V_DOUBLE_ARROW);
				      
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);
  attributes_mask |= GDK_WA_CURSOR;
  
  paned->handle = gdk_window_new (widget->window,
				  &attributes, attributes_mask);
  gdk_window_set_user_data (paned->handle, paned);
  gdk_cursor_destroy (attributes.cursor);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, paned->handle, GTK_STATE_NORMAL);

  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
  
  gdk_window_show (paned->handle);
}

static void
gtk_paned_map (GtkWidget *widget)
{
  GtkPaned *paned;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  paned = GTK_PANED (widget);
  
  if (paned->child1 &&
      GTK_WIDGET_VISIBLE (paned->child1) &&
      !GTK_WIDGET_MAPPED (paned->child1))
    gtk_widget_map (paned->child1);
  if (paned->child2 &&
      GTK_WIDGET_VISIBLE (paned->child2) &&
      !GTK_WIDGET_MAPPED (paned->child2))
    gtk_widget_map (paned->child2);

  gdk_window_show (widget->window);
}

static void
gtk_paned_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  
  gdk_window_hide (widget->window);
}

static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));
  
  paned = GTK_PANED (widget);
  
  if (paned->xor_gc)
    {
      gdk_gc_destroy (paned->xor_gc);
      paned->xor_gc = NULL;
    }
  
  if (paned->handle)
    {
      gdk_window_set_user_data (paned->handle, NULL);
      gdk_window_destroy (paned->handle);
      paned->handle = NULL;
    }
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gint
gtk_paned_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  gboolean handle_full_size = _gtk_paned_is_handle_full_size (paned);
  GdkEventExpose child_event;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      /* An expose event for the handle */
      if (event->window == paned->handle)
	{
	  gint width, height;
	  gchar *detail;
	  
	  gdk_window_get_size (paned->handle, &width, &height);

	  if (handle_full_size)
	    detail = GTK_IS_HPANED (widget) ? "hpaned" : "vpaned";
	  else
	    detail = "paned";
	  
	  gtk_paint_box (widget->style, paned->handle,
			 GTK_WIDGET_STATE(widget),
			 GTK_SHADOW_OUT, 
			 &event->area, widget, detail,
			 0, 0,
			 width, height);
	}
      else
	{
	  child_event = *event;
	  if (paned->child1 &&
	      GTK_WIDGET_NO_WINDOW (paned->child1) &&
	      gtk_widget_intersect (paned->child1, &event->area, &child_event.area))
	    gtk_widget_event (paned->child1, (GdkEvent*) &child_event);
	  
	  if (paned->child2 &&
	      GTK_WIDGET_NO_WINDOW (paned->child2) &&
	      gtk_widget_intersect (paned->child2, &event->area, &child_event.area))
	    gtk_widget_event (paned->child2, (GdkEvent*) &child_event);
	  
	  /* redraw the groove if necessary */
	  if (gdk_rectangle_intersect (&paned->groove_rectangle, 
				       &event->area, 
				       &child_event.area))
	    gtk_widget_draw (widget, &child_event.area);
	}
    }
  return FALSE;
}

void
gtk_paned_add1 (GtkPaned     *paned,
		GtkWidget    *widget)
{
  gtk_paned_pack1 (paned, widget, FALSE, TRUE);
}

void
gtk_paned_add2 (GtkPaned  *paned,
		GtkWidget *widget)
{
  gtk_paned_pack2 (paned, widget, TRUE, TRUE);
}

void
gtk_paned_pack1 (GtkPaned     *paned,
		 GtkWidget    *child,
		 gboolean      resize,
		 gboolean      shrink)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));
  
  if (!paned->child1)
    {
      paned->child1 = child;
      paned->child1_resize = resize;
      paned->child1_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));

      if (GTK_WIDGET_REALIZED (child->parent))
	gtk_widget_realize (child);

      if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child))
	{
	  if (GTK_WIDGET_MAPPED (child->parent))
	    gtk_widget_map (child);

	  gtk_widget_queue_resize (child);
	}
    }
}

void
gtk_paned_pack2 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));
  
  if (!paned->child2)
    {
      paned->child2 = child;
      paned->child2_resize = resize;
      paned->child2_shrink = shrink;
      
      gtk_widget_set_parent (child, GTK_WIDGET (paned));

      if (GTK_WIDGET_REALIZED (child->parent))
	gtk_widget_realize (child);

      if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child))
	{
	  if (GTK_WIDGET_MAPPED (child->parent))
	    gtk_widget_map (child);

	  gtk_widget_queue_resize (child);
	}
    }
}

static void
gtk_paned_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  GtkPaned *paned;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (widget != NULL);
  
  paned = GTK_PANED (container);
  
  if (!paned->child1)
    gtk_paned_add1 (GTK_PANED (container),widget);
  else if (!paned->child2)
    gtk_paned_add2 (GTK_PANED (container),widget);
}

static void
gtk_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkPaned *paned;
  gboolean was_visible;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (widget != NULL);
  
  paned = GTK_PANED (container);
  was_visible = GTK_WIDGET_VISIBLE (widget);
  
  if (paned->child1 == widget)
    {
      gtk_widget_unparent (widget);
      
      paned->child1 = NULL;
      
      if (was_visible && GTK_WIDGET_VISIBLE (container))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
  else if (paned->child2 == widget)
    {
      gtk_widget_unparent (widget);
      
      paned->child2 = NULL;
      
      if (was_visible && GTK_WIDGET_VISIBLE (container))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_paned_forall (GtkContainer *container,
		  gboolean	include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkPaned *paned;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (callback != NULL);
  
  paned = GTK_PANED (container);
  
  if (paned->child1)
    (* callback) (paned->child1, callback_data);
  if (paned->child2)
    (* callback) (paned->child2, callback_data);
}

void
gtk_paned_set_position    (GtkPaned  *paned,
			   gint       position)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (GTK_IS_PANED (paned));

  if (position >= 0)
    {
      /* We don't clamp here - the assumption is that
       * if the total allocation changes at the same time
       * as the position, the position set is with reference
       * to the new total size. If only the position changes,
       * then clamping will occur in gtk_paned_compute_position()
       */
      paned->child1_size = position;
      paned->position_set = TRUE;
    }
  else
    paned->position_set = FALSE;

  gtk_widget_queue_resize (GTK_WIDGET (paned));
}

void
gtk_paned_set_handle_size (GtkPaned *paned,
			   guint16   size)
{
  gint x,y;
  
  g_return_if_fail (paned != NULL);
  g_return_if_fail (GTK_IS_PANED (paned));

  if (paned->handle)
    {
      gdk_window_get_geometry (paned->handle, &x, &y, NULL, NULL, NULL);
      gdk_window_move_resize (paned->handle,
                              x + paned->handle_size / 2 - size / 2,
                              y + paned->handle_size / 2 - size / 2,
                              size, size);
    }
  paned->handle_size = size;
}

void
gtk_paned_set_gutter_size (GtkPaned *paned,
			   guint16   size)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (GTK_IS_PANED (paned));

  paned->gutter_size = size;
  
  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (paned)))
    gtk_widget_queue_resize (GTK_WIDGET (paned));
}

void
gtk_paned_compute_position (GtkPaned *paned,
			    gint      allocation,
			    gint      child1_req,
			    gint      child2_req)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (GTK_IS_PANED (paned));

  paned->min_position = paned->child1_shrink ? 0 : child1_req;

  paned->max_position = allocation;
  if (!paned->child2_shrink)
    paned->max_position = MAX (1, paned->max_position - child2_req);

  if (!paned->position_set)
    {
      if (paned->child1_resize && !paned->child2_resize)
	paned->child1_size = MAX (1, allocation - child2_req);
      else if (!paned->child1_resize && paned->child2_resize)
	paned->child1_size = child1_req;
      else if (child1_req + child2_req != 0)
	paned->child1_size = allocation * ((gdouble)child1_req / (child1_req + child2_req));
      else
	paned->child1_size = allocation * 0.5;
    }
  else
    {
      /* If the position was set before the initial allocation.
       * (paned->last_allocation <= 0) just clamp it and leave it.
       */
      if (paned->last_allocation > 0)
	{
	  if (paned->child1_resize && !paned->child2_resize)
	    paned->child1_size += (allocation - paned->last_allocation);
	  else if (!(!paned->child1_resize && paned->child2_resize))
	    paned->child1_size = allocation * ((gdouble)paned->child1_size / (paned->last_allocation));
	}
    }

  paned->child1_size = CLAMP (paned->child1_size,
			      paned->min_position,
			      paned->max_position);

  paned->last_allocation = allocation;
}

gboolean
_gtk_paned_is_handle_full_size (GtkPaned *paned)
{
  return gtk_style_get_prop_experimental (GTK_WIDGET (paned)->style,
					  "GtkPaned::handle_full_size",
					  FALSE);
}

void 
_gtk_paned_get_handle_rect (GtkPaned     *paned,
			    GdkRectangle *rect)
{
  gint border_width = GTK_CONTAINER (paned)->border_width;
  GtkAllocation *allocation = &GTK_WIDGET (paned)->allocation;
  gint gutter_size = _gtk_paned_get_gutter_size (paned);
      
  if (GTK_IS_HPANED (paned))
    {
      rect->x = border_width + paned->child1_size;
      rect->y = border_width;
      rect->width = gutter_size;
      rect->height = MAX (1, (gint)allocation->height - 2 * border_width);
    }
  else
    {
      rect->x = border_width;
      rect->y = border_width + paned->child1_size;
      rect->width = MAX (1, (gint)allocation->width - 2 * border_width);
      rect->height = gutter_size;
    }
}

gint
_gtk_paned_get_gutter_size (GtkPaned *paned)
{
  gint default_size = _gtk_paned_is_handle_full_size (paned) ? 5 : paned->gutter_size;

  return gtk_style_get_prop_experimental (GTK_WIDGET (paned)->style,
					  "GtkPaned::handle_width",
					  default_size);
}
