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

#include "gtksignal.h"
#include "gtkviewport.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_SHADOW_TYPE
};


static void gtk_viewport_class_init               (GtkViewportClass *klass);
static void gtk_viewport_init                     (GtkViewport      *viewport);
static void gtk_viewport_destroy                  (GtkObject        *object);
static void gtk_viewport_set_property             (GObject         *object,
						   guint            prop_id,
						   const GValue    *value,
						   GParamSpec      *pspec);
static void gtk_viewport_get_property             (GObject         *object,
						   guint            prop_id,
						   GValue          *value,
						   GParamSpec      *pspec);
static void gtk_viewport_set_scroll_adjustments	  (GtkViewport	    *viewport,
						   GtkAdjustment    *hadjustment,
						   GtkAdjustment    *vadjustment);
static void gtk_viewport_realize                  (GtkWidget        *widget);
static void gtk_viewport_unrealize                (GtkWidget        *widget);
static void gtk_viewport_paint                    (GtkWidget        *widget,
						   GdkRectangle     *area);
static gint gtk_viewport_expose                   (GtkWidget        *widget,
						   GdkEventExpose   *event);
static void gtk_viewport_add                      (GtkContainer     *container,
						   GtkWidget        *widget);
static void gtk_viewport_size_request             (GtkWidget        *widget,
						   GtkRequisition   *requisition);
static void gtk_viewport_size_allocate            (GtkWidget        *widget,
						   GtkAllocation    *allocation);
static void gtk_viewport_adjustment_changed       (GtkAdjustment    *adjustment,
						   gpointer          data);
static void gtk_viewport_adjustment_value_changed (GtkAdjustment    *adjustment,
						   gpointer          data);
static void gtk_viewport_style_set                (GtkWidget *widget,
			                           GtkStyle  *previous_style);

static GtkBinClass *parent_class;

GtkType
gtk_viewport_get_type (void)
{
  static GtkType viewport_type = 0;

  if (!viewport_type)
    {
      static const GtkTypeInfo viewport_info =
      {
	"GtkViewport",
	sizeof (GtkViewport),
	sizeof (GtkViewportClass),
	(GtkClassInitFunc) gtk_viewport_class_init,
	(GtkObjectInitFunc) gtk_viewport_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      viewport_type = gtk_type_unique (GTK_TYPE_BIN, &viewport_info);
    }

  return viewport_type;
}

static void
gtk_viewport_class_init (GtkViewportClass *class)
{
  GtkObjectClass *object_class;
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  parent_class = (GtkBinClass*) gtk_type_class (GTK_TYPE_BIN);

  gobject_class->set_property = gtk_viewport_set_property;
  gobject_class->get_property = gtk_viewport_get_property;
  object_class->destroy = gtk_viewport_destroy;
  
  widget_class->realize = gtk_viewport_realize;
  widget_class->unrealize = gtk_viewport_unrealize;
  widget_class->expose_event = gtk_viewport_expose;
  widget_class->size_request = gtk_viewport_size_request;
  widget_class->size_allocate = gtk_viewport_size_allocate;
  widget_class->style_set = gtk_viewport_style_set;
  
  container_class->add = gtk_viewport_add;

  class->set_scroll_adjustments = gtk_viewport_set_scroll_adjustments;

  g_object_class_install_property (gobject_class,
                                   PROP_HADJUSTMENT,
                                   g_param_spec_object ("hadjustment",
							_("Horizontal adjustment"),
							_("The GtkAdjustment that determines the values of the horizontal position for this viewport."),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VADJUSTMENT,
                                   g_param_spec_object ("vadjustment",
							_("Vertical adjustment"),
							_("The GtkAdjustment that determines the values of the vertical position for this viewport."),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow_type",
						      _("Shadow type"),
						      _("Determines how the shadowed box around the viewport is drawn."),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_IN,
						      G_PARAM_READWRITE));

  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkViewportClass, set_scroll_adjustments),
		    _gtk_marshal_VOID__OBJECT_OBJECT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
}

static void
gtk_viewport_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
  GtkViewport *viewport;

  viewport = GTK_VIEWPORT (object);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_viewport_set_hadjustment (viewport, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_viewport_set_vadjustment (viewport, g_value_get_object (value));
      break;
    case PROP_SHADOW_TYPE:
      gtk_viewport_set_shadow_type (viewport, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_get_property (GObject         *object,
			   guint            prop_id,
			   GValue          *value,
			   GParamSpec      *pspec)
{
  GtkViewport *viewport;

  viewport = GTK_VIEWPORT (object);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, viewport->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, viewport->vadjustment);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, viewport->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_init (GtkViewport *viewport)
{
  GTK_WIDGET_UNSET_FLAGS (viewport, GTK_NO_WINDOW);

  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (viewport), FALSE);
  gtk_container_set_resize_mode (GTK_CONTAINER (viewport), GTK_RESIZE_QUEUE);
  
  viewport->shadow_type = GTK_SHADOW_IN;
  viewport->view_window = NULL;
  viewport->bin_window = NULL;
  viewport->hadjustment = NULL;
  viewport->vadjustment = NULL;
}

/**
 * gtk_viewport_new:
 * @hadjustment: horizontal adjustment.
 * @vadjustment: vertical adjustment.
 * @returns: a new #GtkViewport.
 *
 * Creates a new #GtkViewport with the given adjustments.
 *
 **/
GtkWidget*
gtk_viewport_new (GtkAdjustment *hadjustment,
		  GtkAdjustment *vadjustment)
{
  GtkWidget *viewport;

  viewport = gtk_widget_new (GTK_TYPE_VIEWPORT,
			     "hadjustment", hadjustment,
			     "vadjustment", vadjustment,
			     NULL);

  return viewport;
}

static void
gtk_viewport_destroy (GtkObject *object)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);

  if (viewport->hadjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (viewport->hadjustment), viewport);
      gtk_object_unref (GTK_OBJECT (viewport->hadjustment));
      viewport->hadjustment = NULL;
    }
  if (viewport->vadjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (viewport->vadjustment), viewport);
      gtk_object_unref (GTK_OBJECT (viewport->vadjustment));
      viewport->vadjustment = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/**
 * gtk_viewport_get_hadjustment:
 * @viewport: a #GtkViewport.
 * 
 * Returns the horizontal adjustment of the viewport.
 *
 * Return value: the horizontal adjustment of @viewport.
 **/
GtkAdjustment*
gtk_viewport_get_hadjustment (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  if (!viewport->hadjustment)
    gtk_viewport_set_hadjustment (viewport, NULL);

  return viewport->hadjustment;
}

/**
 * gtk_viewport_get_vadjustment:
 * @viewport: a #GtkViewport.
 * 
 * Returns the vertical adjustment of the viewport.
 *
 * Return value: the vertical adjustment of @viewport.
 **/
GtkAdjustment*
gtk_viewport_get_vadjustment (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  if (!viewport->vadjustment)
    gtk_viewport_set_vadjustment (viewport, NULL);

  return viewport->vadjustment;
}

/**
 * gtk_viewport_set_hadjustment:
 * @viewport: a #GtkViewport.
 * @adjustment: a #GtkAdjustment.
 * 
 * Sets the horizontal adjustment of the viewport.
 **/
void
gtk_viewport_set_hadjustment (GtkViewport   *viewport,
			      GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (viewport->hadjustment && viewport->hadjustment != adjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (viewport->hadjustment), viewport);
      gtk_object_unref (GTK_OBJECT (viewport->hadjustment));
      viewport->hadjustment = NULL;
    }

  if (!adjustment)
    adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0,
						     0.0, 0.0, 0.0));

  if (viewport->hadjustment != adjustment)
    {
      viewport->hadjustment = adjustment;
      gtk_object_ref (GTK_OBJECT (viewport->hadjustment));
      gtk_object_sink (GTK_OBJECT (viewport->hadjustment));
      
      gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			  (GtkSignalFunc) gtk_viewport_adjustment_changed,
			  (gpointer) viewport);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  (GtkSignalFunc) gtk_viewport_adjustment_value_changed,
			  (gpointer) viewport);

      gtk_viewport_adjustment_changed (adjustment, viewport);
    }

  g_object_notify (G_OBJECT (viewport), "hadjustment");
}

/**
 * gtk_viewport_set_vadjustment:
 * @viewport: a #GtkViewport.
 * @adjustment: a #GtkAdjustment.
 * 
 * Sets the vertical adjustment of the viewport.
 **/
void
gtk_viewport_set_vadjustment (GtkViewport   *viewport,
			      GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (viewport->vadjustment && viewport->vadjustment != adjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (viewport->vadjustment), viewport);
      gtk_object_unref (GTK_OBJECT (viewport->vadjustment));
      viewport->vadjustment = NULL;
    }

  if (!adjustment)
    adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0,
						     0.0, 0.0, 0.0));

  if (viewport->vadjustment != adjustment)
    {
      viewport->vadjustment = adjustment;
      gtk_object_ref (GTK_OBJECT (viewport->vadjustment));
      gtk_object_sink (GTK_OBJECT (viewport->vadjustment));
      
      gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			  (GtkSignalFunc) gtk_viewport_adjustment_changed,
			  (gpointer) viewport);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  (GtkSignalFunc) gtk_viewport_adjustment_value_changed,
			  (gpointer) viewport);

      gtk_viewport_adjustment_changed (adjustment, viewport);
    }

    g_object_notify (G_OBJECT (viewport), "vadjustment");
}

static void
gtk_viewport_set_scroll_adjustments (GtkViewport      *viewport,
				     GtkAdjustment    *hadjustment,
				     GtkAdjustment    *vadjustment)
{
  if (viewport->hadjustment != hadjustment)
    gtk_viewport_set_hadjustment (viewport, hadjustment);
  if (viewport->vadjustment != vadjustment)
    gtk_viewport_set_vadjustment (viewport, vadjustment);
}

/** 
 * gtk_viewport_set_shadow_type:
 * @viewport: a #GtkViewport.
 * @type: the new shadow type.
 *
 * Sets the shadow type of the viewport.
 **/ 
void
gtk_viewport_set_shadow_type (GtkViewport   *viewport,
			      GtkShadowType  type)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));

  if ((GtkShadowType) viewport->shadow_type != type)
    {
      viewport->shadow_type = type;

      if (GTK_WIDGET_VISIBLE (viewport))
	{
	  gtk_widget_size_allocate (GTK_WIDGET (viewport), &(GTK_WIDGET (viewport)->allocation));
	  gtk_widget_queue_draw (GTK_WIDGET (viewport));
	}

      g_object_notify (G_OBJECT (viewport), "shadow_type");
    }
}

/**
 * gtk_viewport_get_shadow_type:
 * @viewport: a #GtkViewport
 *
 * Gets the shadow type of the #GtkViewport. See
 * gtk_viewport_set_shadow_type().
 
 * Return value: the shadow type 
 **/
GtkShadowType
gtk_viewport_get_shadow_type (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), GTK_SHADOW_NONE);

  return viewport->shadow_type;
}

static void
gtk_viewport_realize (GtkWidget *widget)
{
  GtkBin *bin;
  GtkViewport *viewport;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint event_mask;
  gint border_width;

  border_width = GTK_CONTAINER (widget)->border_width;

  bin = GTK_BIN (widget);
  viewport = GTK_VIEWPORT (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  /* We select on button_press_mask so that button 4-5 scrolls are trapped.
   */
  attributes.event_mask = event_mask | GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, viewport);

  if (viewport->shadow_type != GTK_SHADOW_NONE)
    {
      attributes.x = widget->style->xthickness;
      attributes.y = widget->style->ythickness;
    }
  else
    {
      attributes.x = 0;
      attributes.y = 0;
    }

  attributes.width = MAX (1, (gint)widget->allocation.width - attributes.x * 2 - border_width * 2);
  attributes.height = MAX (1, (gint)widget->allocation.height - attributes.y * 2 - border_width * 2);
  attributes.event_mask = 0;

  viewport->view_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (viewport->view_window, viewport);

  attributes.x = 0;
  attributes.y = 0;

  if (bin->child)
    {
      attributes.width = viewport->hadjustment->upper;
      attributes.height = viewport->vadjustment->upper;
    }
  
  attributes.event_mask = event_mask;

  viewport->bin_window = gdk_window_new (viewport->view_window, &attributes, attributes_mask);
  gdk_window_set_user_data (viewport->bin_window, viewport);

  if (bin->child)
    gtk_widget_set_parent_window (bin->child, viewport->bin_window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, viewport->bin_window, GTK_STATE_NORMAL);

   gtk_paint_flat_box(widget->style, viewport->bin_window, GTK_STATE_NORMAL,
		      GTK_SHADOW_NONE,
		      NULL, widget, "viewportbin",
		      0, 0, -1, -1);
   
  gdk_window_show (viewport->bin_window);
  gdk_window_show (viewport->view_window);
}

static void
gtk_viewport_unrealize (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  gdk_window_set_user_data (viewport->view_window, NULL);
  gdk_window_destroy (viewport->view_window);
  viewport->view_window = NULL;

  gdk_window_set_user_data (viewport->bin_window, NULL);
  gdk_window_destroy (viewport->bin_window);
  viewport->bin_window = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_viewport_paint (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkViewport *viewport;

  g_return_if_fail (GTK_IS_VIEWPORT (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      viewport = GTK_VIEWPORT (widget);

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, viewport->shadow_type,
		       0, 0, -1, -1);
    }
}

static gint
gtk_viewport_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GdkEventExpose child_event;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      viewport = GTK_VIEWPORT (widget);
      bin = GTK_BIN (widget);

      if (event->window == widget->window)
	gtk_viewport_paint (widget, &event->area);
      else if (event->window == viewport->bin_window)
	{
	  child_event = *event;

	  gtk_paint_flat_box(widget->style, viewport->bin_window, 
			     GTK_STATE_NORMAL, GTK_SHADOW_NONE,
			     &event->area, widget, "viewportbin",
			     0, 0, -1, -1);
	  
	  (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
	}
	

    }

  return FALSE;
}

static void
gtk_viewport_add (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkBin *bin;

  g_return_if_fail (GTK_IS_WIDGET (child));

  bin = GTK_BIN (container);
  g_return_if_fail (bin->child == NULL);

  gtk_widget_set_parent_window (child, GTK_VIEWPORT (bin)->bin_window);

  GTK_CONTAINER_CLASS (parent_class)->add (container, child);
}

static void
gtk_viewport_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GtkRequisition child_requisition;

  viewport = GTK_VIEWPORT (widget);
  bin = GTK_BIN (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			GTK_WIDGET (widget)->style->xthickness) * 2;

  requisition->height = (GTK_CONTAINER (widget)->border_width * 2 +
			 GTK_WIDGET (widget)->style->ythickness) * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);
      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_viewport_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation child_allocation;
  gint hval, vval;
  gint border_width = GTK_CONTAINER (widget)->border_width;

  /* demand creation */
  if (!viewport->hadjustment)
    gtk_viewport_set_hadjustment (viewport, NULL);
  if (!viewport->vadjustment)
    gtk_viewport_set_vadjustment (viewport, NULL);

  /* If our size changed, and we have a shadow, queue a redraw on widget->window to
   * redraw the shadow correctly.
   */
  if (GTK_WIDGET_MAPPED (widget) &&
      viewport->shadow_type != GTK_SHADOW_NONE &&
      (widget->allocation.width != allocation->width ||
       widget->allocation.height != allocation->height))
    gdk_window_invalidate_rect (widget->window, NULL, FALSE);
  
  widget->allocation = *allocation;

  child_allocation.x = 0;
  child_allocation.y = 0;

  if (viewport->shadow_type != GTK_SHADOW_NONE)
    {
      child_allocation.x = widget->style->xthickness;
      child_allocation.y = widget->style->ythickness;
    }

  child_allocation.width = MAX (1, allocation->width - child_allocation.x * 2 - border_width * 2);
  child_allocation.height = MAX (1, allocation->height - child_allocation.y * 2 - border_width * 2);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + border_width,
			      allocation->y + border_width,
			      allocation->width - border_width * 2,
			      allocation->height - border_width * 2);

      gdk_window_move_resize (viewport->view_window,
			      child_allocation.x,
			      child_allocation.y,
			      child_allocation.width,
			      child_allocation.height);
    }

  viewport->hadjustment->page_size = child_allocation.width;
  viewport->hadjustment->page_increment = viewport->hadjustment->page_size / 2;
  viewport->hadjustment->step_increment = 10;

  viewport->vadjustment->page_size = child_allocation.height;
  viewport->vadjustment->page_increment = viewport->vadjustment->page_size / 2;
  viewport->vadjustment->step_increment = 10;

  hval = viewport->hadjustment->value;
  vval = viewport->vadjustment->value;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      gtk_widget_get_child_requisition (bin->child, &child_requisition);
      
      viewport->hadjustment->lower = 0;
      viewport->hadjustment->upper = MAX (child_requisition.width,
					  child_allocation.width);

      hval = CLAMP (hval, 0,
		    viewport->hadjustment->upper -
		    viewport->hadjustment->page_size);

      viewport->vadjustment->lower = 0;
      viewport->vadjustment->upper = MAX (child_requisition.height,
					  child_allocation.height);

      vval = CLAMP (vval, 0,
		    viewport->vadjustment->upper -
		    viewport->vadjustment->page_size);
    }

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = 0;
      child_allocation.y = 0;

      child_allocation.width = viewport->hadjustment->upper;
      child_allocation.height = viewport->vadjustment->upper;

      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_resize (viewport->bin_window,
			   child_allocation.width,
			   child_allocation.height);

      child_allocation.x = 0;
      child_allocation.y = 0;
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }

  gtk_signal_emit_by_name (GTK_OBJECT (viewport->hadjustment), "changed");
  gtk_signal_emit_by_name (GTK_OBJECT (viewport->vadjustment), "changed");
  if (viewport->hadjustment->value != hval)
    {
      viewport->hadjustment->value = hval;
      gtk_signal_emit_by_name (GTK_OBJECT (viewport->hadjustment), "value_changed");
    }
  if (viewport->vadjustment->value != vval)
    {
      viewport->vadjustment->value = vval;
      gtk_signal_emit_by_name (GTK_OBJECT (viewport->vadjustment), "value_changed");
    }
}

static void
gtk_viewport_adjustment_changed (GtkAdjustment *adjustment,
				 gpointer       data)
{
  GtkViewport *viewport;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (GTK_IS_VIEWPORT (data));

  viewport = GTK_VIEWPORT (data);
}

static void
gtk_viewport_adjustment_value_changed (GtkAdjustment *adjustment,
				       gpointer       data)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (GTK_IS_VIEWPORT (data));

  viewport = GTK_VIEWPORT (data);
  bin = GTK_BIN (data);

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = 0;
      child_allocation.y = 0;

      if (viewport->hadjustment->lower != (viewport->hadjustment->upper -
					   viewport->hadjustment->page_size))
	child_allocation.x =  viewport->hadjustment->lower - viewport->hadjustment->value;

      if (viewport->vadjustment->lower != (viewport->vadjustment->upper -
					   viewport->vadjustment->page_size))
	child_allocation.y = viewport->vadjustment->lower - viewport->vadjustment->value;

      if (GTK_WIDGET_REALIZED (viewport))
	{
	  gdk_window_move (viewport->bin_window,
			   child_allocation.x,
			   child_allocation.y);
      
	  gdk_window_process_updates (viewport->bin_window, TRUE);
	}
    }
}

static void
gtk_viewport_style_set (GtkWidget *widget,
			GtkStyle  *previous_style)
{
   GtkViewport *viewport;
   
   if (GTK_WIDGET_REALIZED (widget) &&
       !GTK_WIDGET_NO_WINDOW (widget))
     {
	viewport = GTK_VIEWPORT (widget);
	
	gtk_style_set_background (widget->style, viewport->bin_window, GTK_STATE_NORMAL);
	gtk_style_set_background (widget->style, widget->window, widget->state);
     }
}
