/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkintl.h"
#include "gtkpaned.h"

enum {
  PROP_0,
  PROP_POSITION,
  PROP_POSITION_SET
};

static void    gtk_paned_class_init   (GtkPanedClass  *klass);
static void    gtk_paned_init         (GtkPaned       *paned);
static void    gtk_paned_set_property (GObject        *object,
				       guint           prop_id,
				       const GValue   *value,
				       GParamSpec     *pspec);
static void    gtk_paned_get_property (GObject        *object,
				       guint           prop_id,
				       GValue         *value,
				       GParamSpec     *pspec);
static void    gtk_paned_realize      (GtkWidget      *widget);
static void    gtk_paned_unrealize    (GtkWidget      *widget);
static void    gtk_paned_map          (GtkWidget      *widget);
static void    gtk_paned_unmap        (GtkWidget      *widget);
static gint    gtk_paned_expose       (GtkWidget      *widget,
				       GdkEventExpose *event);
static void    gtk_paned_add          (GtkContainer   *container,
				       GtkWidget      *widget);
static void    gtk_paned_remove       (GtkContainer   *container,
				       GtkWidget      *widget);
static void    gtk_paned_forall       (GtkContainer   *container,
				       gboolean        include_internals,
				       GtkCallback     callback,
				       gpointer        callback_data);
static GtkType gtk_paned_child_type   (GtkContainer   *container);

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
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  object_class->set_property = gtk_paned_set_property;
  object_class->get_property = gtk_paned_get_property;

  widget_class->realize = gtk_paned_realize;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->map = gtk_paned_map;
  widget_class->unmap = gtk_paned_unmap;
  widget_class->expose_event = gtk_paned_expose;
  
  container_class->add = gtk_paned_add;
  container_class->remove = gtk_paned_remove;
  container_class->forall = gtk_paned_forall;
  container_class->child_type = gtk_paned_child_type;

  g_object_class_install_property (object_class,
				   PROP_POSITION,
				   g_param_spec_int ("position",
						     _("Position"),
						     _("Position of paned separator in pixels (0 means all the way to the left/top)"),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (object_class,
				   PROP_POSITION_SET,
				   g_param_spec_boolean ("position_set",
							 _("Position Set"),
							 _("TRUE if the Position property should be used"),
							 FALSE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));
				   
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("handle_size",
							     _("Handle Size"),
							     _("Width of handle"),
							     0,
							     G_MAXINT,
							     5,
							     G_PARAM_READABLE));
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
  GTK_WIDGET_SET_FLAGS (paned, GTK_NO_WINDOW);
  
  paned->child1 = NULL;
  paned->child2 = NULL;
  paned->handle = NULL;
  paned->xor_gc = NULL;
  paned->cursor_type = GDK_CROSS;
  
  paned->handle_pos.width = 5;
  paned->handle_pos.height = 5;
  paned->position_set = FALSE;
  paned->last_allocation = -1;
  paned->in_drag = FALSE;
  
  paned->handle_pos.x = -1;
  paned->handle_pos.y = -1;
}

static void
gtk_paned_set_property (GObject        *object,
			guint           prop_id,
			const GValue   *value,
			GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);
  
  switch (prop_id)
    {
    case PROP_POSITION:
      gtk_paned_set_position (paned, g_value_get_int (value));
      break;
    case PROP_POSITION_SET:
      paned->position_set = g_value_get_boolean (value);
      gtk_widget_queue_resize (GTK_WIDGET (paned));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_get_property (GObject        *object,
			guint           prop_id,
			GValue         *value,
			GParamSpec     *pspec)
{
  GtkPaned *paned = GTK_PANED (object);
  
  switch (prop_id)
    {
    case PROP_POSITION:
      g_value_set_int (value, paned->child1_size);
      break;
    case PROP_POSITION_SET:
      g_value_set_boolean (value, paned->position_set);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_paned_realize (GtkWidget *widget)
{
  GtkPaned *paned;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (GTK_IS_PANED (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  paned = GTK_PANED (widget);

  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = paned->handle_pos.x;
  attributes.y = paned->handle_pos.y;
  attributes.width = paned->handle_pos.width;
  attributes.height = paned->handle_pos.height;
  attributes.cursor = gdk_cursor_new (paned->cursor_type);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_CURSOR;

  paned->handle = gdk_window_new (widget->window,
				  &attributes, attributes_mask);
  gdk_window_set_user_data (paned->handle, paned);
  gdk_cursor_destroy (attributes.cursor);

  widget->style = gtk_style_attach (widget->style, widget->window);

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    gdk_window_show (paned->handle);
}

static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned;

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

static void
gtk_paned_map (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
  
  g_return_if_fail (GTK_IS_PANED (widget));

  gdk_window_show (paned->handle);

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
gtk_paned_unmap (GtkWidget *widget)
{
  GtkPaned *paned = GTK_PANED (widget);
    
  g_return_if_fail (GTK_IS_PANED (widget));

  gdk_window_hide (paned->handle);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static gint
gtk_paned_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkPaned *paned = GTK_PANED (widget);

  g_return_val_if_fail (GTK_IS_PANED (widget), FALSE);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      GdkRegion *region;

      region = gdk_region_rectangle (&paned->handle_pos);
      gdk_region_intersect (region, event->region);

      if (!gdk_region_empty (region))
	{
	  GdkRectangle clip;

	  gdk_region_get_clipbox (region, &clip);
	  
	  gtk_paint_handle (widget->style, widget->window,
			    GTK_STATE_NORMAL, GTK_SHADOW_NONE,
			    &clip, widget, "paned",
			    paned->handle_pos.x, paned->handle_pos.y,
			    paned->handle_pos.width, paned->handle_pos.height,
			    paned->orientation);
	}

      gdk_region_destroy (region);
      
      /* Chain up to draw children */
      GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

void
gtk_paned_add1 (GtkPaned  *paned,
		GtkWidget *widget)
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
gtk_paned_pack1 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!paned->child1)
    {
      paned->child1 = child;
      paned->child1_resize = resize;
      paned->child1_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));
    }
}

void
gtk_paned_pack2 (GtkPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (GTK_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!paned->child2)
    {
      paned->child2 = child;
      paned->child2_resize = resize;
      paned->child2_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));
    }
}


static void
gtk_paned_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  GtkPaned *paned;

  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (widget != NULL);

  paned = GTK_PANED (container);

  if (!paned->child1)
    gtk_paned_add1 (GTK_PANED (container), widget);
  else if (!paned->child2)
    gtk_paned_add2 (GTK_PANED (container), widget);
}

static void
gtk_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkPaned *paned;
  gboolean was_visible;

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
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkPaned *paned;

  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (callback != NULL);

  paned = GTK_PANED (container);

  if (paned->child1)
    (*callback) (paned->child1, callback_data);
  if (paned->child2)
    (*callback) (paned->child2, callback_data);
}

/**
 * gtk_paned_get_position:
 * @paned: a #GtkPaned widget
 * 
 * Obtains the position of the divider between the two panes.
 * 
 * Return value: position of the divider
 **/
gint
gtk_paned_get_position (GtkPaned  *paned)
{
  g_return_val_if_fail (GTK_IS_PANED (paned), 0);

  return paned->child1_size;
}

/**
 * gtk_paned_set_position:
 * @paned: a #GtkPaned widget
 * @position: pixel position of divider, a negative value means that the position
 *            is unset.
 * 
 * Sets the position of the divider between the two panes.
 **/
void
gtk_paned_set_position (GtkPaned *paned,
			gint      position)
{
  GObject *object;
  
  g_return_if_fail (GTK_IS_PANED (paned));

  object = G_OBJECT (paned);
  
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
    {
      paned->position_set = FALSE;
    }

  g_object_freeze_notify (object);
  g_object_notify (object, "position");
  g_object_notify (object, "position_set");
  g_object_thaw_notify (object);

  gtk_widget_queue_resize (GTK_WIDGET (paned));
}

void
gtk_paned_compute_position (GtkPaned *paned,
			    gint      allocation,
			    gint      child1_req,
			    gint      child2_req)
{
  gint old_position;
  
  g_return_if_fail (GTK_IS_PANED (paned));

  old_position = paned->child1_size;

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
	    paned->child1_size += allocation - paned->last_allocation;
	  else if (!(!paned->child1_resize && paned->child2_resize))
	    paned->child1_size = allocation * ((gdouble) paned->child1_size / (paned->last_allocation));
	}
    }

  paned->child1_size = CLAMP (paned->child1_size,
			      paned->min_position,
			      paned->max_position);

  if (paned->child1_size != old_position)
    g_object_notify (G_OBJECT (paned), "position");

  paned->last_allocation = allocation;
}
