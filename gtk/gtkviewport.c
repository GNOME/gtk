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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkviewport.h"

#include "gtkadjustment.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkscrollable.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"


/**
 * SECTION:gtkviewport
 * @Short_description: An adapter which makes widgets scrollable
 * @Title: GtkViewport
 * @See_also:#GtkScrolledWindow, #GtkAdjustment
 *
 * The #GtkViewport widget acts as an adaptor class, implementing
 * scrollability for child widgets that lack their own scrolling
 * capabilities. Use GtkViewport to scroll child widgets such as
 * #GtkGrid, #GtkBox, and so on.
 *
 * If a widget has native scrolling abilities, such as #GtkTextView,
 * #GtkTreeView or #GtkIconView, it can be added to a #GtkScrolledWindow
 * with gtk_container_add(). If a widget does not, you must first add the
 * widget to a #GtkViewport, then add the viewport to the scrolled window.
 * gtk_container_add() does this automatically if a child that does not
 * implement #GtkScrollable is added to a #GtkScrolledWindow, so you can
 * ignore the presence of the viewport.
 *
 * The GtkViewport will start scrolling content only if allocated less
 * than the child widget’s minimum size in a given orientation.
 *
 * # CSS nodes
 *
 * GtkViewport has a single CSS node with name viewport.
 */

struct _GtkViewportPrivate
{
  GtkAdjustment  *hadjustment;
  GtkAdjustment  *vadjustment;
  GtkShadowType   shadow_type;

  GdkWindow      *bin_window;
  GdkWindow      *view_window;

  GtkCssGadget *gadget;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
};

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_SHADOW_TYPE
};


static void gtk_viewport_set_property             (GObject         *object,
						   guint            prop_id,
						   const GValue    *value,
						   GParamSpec      *pspec);
static void gtk_viewport_get_property             (GObject         *object,
						   guint            prop_id,
						   GValue          *value,
						   GParamSpec      *pspec);
static void gtk_viewport_finalize                 (GObject         *object);
static void gtk_viewport_destroy                  (GtkWidget        *widget);
static void gtk_viewport_realize                  (GtkWidget        *widget);
static void gtk_viewport_unrealize                (GtkWidget        *widget);
static void gtk_viewport_snapshot                 (GtkWidget        *widget,
						   GtkSnapshot      *snapshot);
static void gtk_viewport_add                      (GtkContainer     *container,
						   GtkWidget        *widget);
static void gtk_viewport_size_allocate            (GtkWidget        *widget,
						   GtkAllocation    *allocation);
static void gtk_viewport_adjustment_value_changed (GtkAdjustment    *adjustment,
						   gpointer          data);
static void viewport_set_adjustment               (GtkViewport      *viewport,
                                                   GtkOrientation    orientation,
                                                   GtkAdjustment    *adjustment);

G_DEFINE_TYPE_WITH_CODE (GtkViewport, gtk_viewport, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkViewport)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
gtk_viewport_measure (GtkCssGadget   *gadget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline,
                      gpointer        data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkWidget *child;

  *minimum = *natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_measure (child,
                        orientation,
                        for_size,
                        minimum, natural,
                        NULL, NULL);
}

static void
viewport_set_adjustment_values (GtkViewport    *viewport,
                                GtkOrientation  orientation)
{
  GtkBin *bin = GTK_BIN (viewport);
  GtkViewportPrivate *priv = gtk_viewport_get_instance_private (viewport);
  GtkAdjustment *adjustment;
  GtkScrollablePolicy scroll_policy;
  GtkScrollablePolicy other_scroll_policy;
  GtkOrientation other_orientation;
  GtkAllocation view_allocation;
  GtkWidget *child;
  gdouble upper, value;
  int viewport_size, other_viewport_size;

  gtk_css_gadget_get_content_allocation (viewport->priv->gadget,
                                         &view_allocation, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      adjustment = priv->hadjustment;
      other_orientation = GTK_ORIENTATION_VERTICAL;
      viewport_size = view_allocation.width;
      other_viewport_size = view_allocation.height;
      scroll_policy = priv->hscroll_policy;
      other_scroll_policy = priv->vscroll_policy;
    }
  else /* VERTICAL */
    {
      adjustment = priv->vadjustment;
      other_orientation = GTK_ORIENTATION_HORIZONTAL;
      viewport_size = view_allocation.height;
      other_viewport_size = view_allocation.width;
      scroll_policy = priv->vscroll_policy;
      other_scroll_policy = priv->hscroll_policy;
    }


  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      int min_size, nat_size;
      int scroll_size;

      if (other_scroll_policy == GTK_SCROLL_MINIMUM)
        gtk_widget_measure (child, other_orientation, -1,
                            &scroll_size, NULL, NULL, NULL);
      else
        gtk_widget_measure (child, other_orientation, -1,
                            NULL, &scroll_size, NULL, NULL);

      gtk_widget_measure (child, orientation,
                          MAX (other_viewport_size, scroll_size),
                          &min_size, &nat_size, NULL, NULL);

      if (scroll_policy == GTK_SCROLL_MINIMUM)
        upper = MAX (min_size, viewport_size);
      else
        upper = MAX (nat_size, viewport_size);

    }
  else
    {
      upper = viewport_size;
    }

  value = gtk_adjustment_get_value (adjustment);

  /* We clamp to the left in RTL mode */
  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      _gtk_widget_get_direction (GTK_WIDGET (viewport)) == GTK_TEXT_DIR_RTL)
    {
      gdouble dist = gtk_adjustment_get_upper (adjustment)
                     - value
                     - gtk_adjustment_get_page_size (adjustment);
      value = upper - dist - viewport_size;
    }

  gtk_adjustment_configure (adjustment,
                            value,
                            0,
                            upper,
                            viewport_size * 0.1,
                            viewport_size * 0.9,
                            viewport_size);
}

static void
gtk_viewport_allocate (GtkCssGadget        *gadget,
                       const GtkAllocation *allocation,
                       int                  baseline,
                       GtkAllocation       *out_clip,
                       gpointer             data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkAdjustment *hadjustment = priv->hadjustment;
  GtkAdjustment *vadjustment = priv->vadjustment;
  GtkWidget *child;

  g_object_freeze_notify (G_OBJECT (hadjustment));
  g_object_freeze_notify (G_OBJECT (vadjustment));

  viewport_set_adjustment_values (viewport, GTK_ORIENTATION_HORIZONTAL);
  viewport_set_adjustment_values (viewport, GTK_ORIENTATION_VERTICAL);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->view_window,
			      allocation->x,
			      allocation->y,
			      allocation->width,
			      allocation->height);
      gdk_window_move_resize (priv->bin_window,
                              - gtk_adjustment_get_value (hadjustment),
                              - gtk_adjustment_get_value (vadjustment),
                              gtk_adjustment_get_upper (hadjustment),
                              gtk_adjustment_get_upper (vadjustment));
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation child_allocation;

      child_allocation.x = 0;
      child_allocation.y = 0;
      child_allocation.width = gtk_adjustment_get_upper (hadjustment);
      child_allocation.height = gtk_adjustment_get_upper (vadjustment);

      gtk_widget_size_allocate (child, &child_allocation);
    }

  g_object_thaw_notify (G_OBJECT (hadjustment));
  g_object_thaw_notify (G_OBJECT (vadjustment));
}

static gboolean
gtk_viewport_render (GtkCssGadget *gadget,
                     GtkSnapshot  *snapshot,
                     int           x,
                     int           y,
                     int           width,
                     int           height,
                     gpointer      data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT(x, y, width, height), "Viewport");

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->snapshot (widget, snapshot);

  gtk_snapshot_pop (snapshot);

  return FALSE;
}

static void
gtk_viewport_measure_ (GtkWidget     *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  gtk_css_gadget_get_preferred_size (GTK_VIEWPORT (widget)->priv->gadget,
                                     orientation,
                                     for_size,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_viewport_class_init (GtkViewportClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_viewport_set_property;
  gobject_class->get_property = gtk_viewport_get_property;
  gobject_class->finalize = gtk_viewport_finalize;

  widget_class->destroy = gtk_viewport_destroy;
  widget_class->realize = gtk_viewport_realize;
  widget_class->unrealize = gtk_viewport_unrealize;
  widget_class->snapshot = gtk_viewport_snapshot;
  widget_class->size_allocate = gtk_viewport_size_allocate;
  widget_class->measure = gtk_viewport_measure_;
  
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_VIEWPORT);

  container_class->add = gtk_viewport_add;

  /* GtkScrollable implementation */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
						      P_("Shadow type"),
						      P_("Determines how the shadowed box around the viewport is drawn"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_IN,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, "viewport");
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
      viewport_set_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      viewport_set_adjustment (viewport, GTK_ORIENTATION_VERTICAL, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      if (viewport->priv->hscroll_policy != g_value_get_enum (value))
        {
          viewport->priv->hscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (viewport));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_VSCROLL_POLICY:
      if (viewport->priv->vscroll_policy != g_value_get_enum (value))
        {
          viewport->priv->vscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (viewport));
          g_object_notify_by_pspec (object, pspec);
        }
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
  GtkViewport *viewport = GTK_VIEWPORT (object);
  GtkViewportPrivate *priv = viewport->priv;

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->vadjustment);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->hscroll_policy);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->vscroll_policy);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_init (GtkViewport *viewport)
{
  GtkWidget *widget;
  GtkViewportPrivate *priv;
  GtkCssNode *widget_node;

  viewport->priv = gtk_viewport_get_instance_private (viewport);
  priv = viewport->priv;
  widget = GTK_WIDGET (viewport);

  gtk_widget_set_has_window (widget, FALSE);
  gtk_widget_set_redraw_on_allocate (widget, FALSE);

  priv->shadow_type = GTK_SHADOW_IN;
  priv->view_window = NULL;
  priv->bin_window = NULL;
  priv->hadjustment = NULL;
  priv->vadjustment = NULL;

  widget_node = gtk_widget_get_css_node (widget);
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     widget,
                                                     gtk_viewport_measure,
                                                     gtk_viewport_allocate,
                                                     gtk_viewport_render,
                                                     NULL, NULL);

  gtk_css_gadget_add_class (priv->gadget, GTK_STYLE_CLASS_FRAME);
  viewport_set_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL, NULL);
  viewport_set_adjustment (viewport, GTK_ORIENTATION_VERTICAL, NULL);
}

/**
 * gtk_viewport_new:
 * @hadjustment: (allow-none): horizontal adjustment
 * @vadjustment: (allow-none): vertical adjustment
 *
 * Creates a new #GtkViewport with the given adjustments, or with default
 * adjustments if none are given.
 *
 * Returns: a new #GtkViewport
 */
GtkWidget*
gtk_viewport_new (GtkAdjustment *hadjustment,
		  GtkAdjustment *vadjustment)
{
  GtkWidget *viewport;

  viewport = g_object_new (GTK_TYPE_VIEWPORT,
                           "hadjustment", hadjustment,
                           "vadjustment", vadjustment,
                           NULL);

  return viewport;
}

#define ADJUSTMENT_POINTER(viewport, orientation)         \
  (((orientation) == GTK_ORIENTATION_HORIZONTAL) ?        \
     &(viewport)->priv->hadjustment : &(viewport)->priv->vadjustment)

static void
viewport_disconnect_adjustment (GtkViewport    *viewport,
				GtkOrientation  orientation)
{
  GtkAdjustment **adjustmentp = ADJUSTMENT_POINTER (viewport, orientation);

  if (*adjustmentp)
    {
      g_signal_handlers_disconnect_by_func (*adjustmentp,
					    gtk_viewport_adjustment_value_changed,
					    viewport);
      g_object_unref (*adjustmentp);
      *adjustmentp = NULL;
    }
}

static void
gtk_viewport_destroy (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL);
  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_VERTICAL);

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->destroy (widget);
}

static void
gtk_viewport_finalize (GObject *object)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);
  GtkViewportPrivate *priv = viewport->priv;

  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_viewport_parent_class)->finalize (object);
}

static void
viewport_set_adjustment (GtkViewport    *viewport,
			 GtkOrientation  orientation,
			 GtkAdjustment  *adjustment)
{
  GtkAdjustment **adjustmentp = ADJUSTMENT_POINTER (viewport, orientation);

  if (adjustment && adjustment == *adjustmentp)
    return;

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  viewport_disconnect_adjustment (viewport, orientation);
  *adjustmentp = adjustment;
  g_object_ref_sink (adjustment);

  viewport_set_adjustment_values (viewport, orientation);

  g_signal_connect (adjustment, "value-changed",
		    G_CALLBACK (gtk_viewport_adjustment_value_changed),
		    viewport);

  gtk_viewport_adjustment_value_changed (adjustment, viewport);
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
  GtkViewportPrivate *priv;
  GtkWidget *widget;
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_VIEWPORT (viewport));

  widget = GTK_WIDGET (viewport);
  priv = viewport->priv;

  if ((GtkShadowType) priv->shadow_type != type)
    {
      priv->shadow_type = type;

      context = gtk_widget_get_style_context (widget);
      if (type != GTK_SHADOW_NONE)
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
      else
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_FRAME);
 
      gtk_widget_queue_resize (widget);

      g_object_notify (G_OBJECT (viewport), "shadow-type");
    }
}

/**
 * gtk_viewport_get_shadow_type:
 * @viewport: a #GtkViewport
 *
 * Gets the shadow type of the #GtkViewport. See
 * gtk_viewport_set_shadow_type().
 *
 * Returns: the shadow type 
 **/
GtkShadowType
gtk_viewport_get_shadow_type (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), GTK_SHADOW_NONE);

  return viewport->priv->shadow_type;
}

static void
gtk_viewport_realize (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkAdjustment *hadjustment = priv->hadjustment;
  GtkAdjustment *vadjustment = priv->vadjustment;
  GtkAllocation view_allocation;
  GtkWidget *child;
  gint event_mask;

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->realize (widget);

  event_mask = gtk_widget_get_events (widget);
  gtk_css_gadget_get_content_allocation (priv->gadget,
                                         &view_allocation, NULL);

  priv->view_window = gdk_window_new_child (gtk_widget_get_window (widget),
                                            event_mask | GDK_SCROLL_MASK | GDK_TOUCH_MASK | GDK_SMOOTH_SCROLL_MASK,
                                            &view_allocation);
  gtk_widget_register_window (widget, priv->view_window);

  priv->bin_window = gdk_window_new_child (priv->view_window,
                                           event_mask,
                                           &(GdkRectangle) {
                                             - gtk_adjustment_get_value (hadjustment),
                                             - gtk_adjustment_get_value (vadjustment),
                                             gtk_adjustment_get_upper (hadjustment),
                                             gtk_adjustment_get_upper (vadjustment)});
  gtk_widget_register_window (widget, priv->bin_window);

  child = gtk_bin_get_child (bin);
  if (child)
    gtk_widget_set_parent_window (child, priv->bin_window);

  gdk_window_show (priv->bin_window);
  gdk_window_show (priv->view_window);
}

static void
gtk_viewport_unrealize (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;

  gtk_widget_unregister_window (widget, priv->view_window);
  gdk_window_destroy (priv->view_window);
  priv->view_window = NULL;

  gtk_widget_unregister_window (widget, priv->bin_window);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->unrealize (widget);
}

static void
gtk_viewport_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;

  gtk_css_gadget_snapshot (priv->gadget, snapshot);
}

static void
gtk_viewport_add (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  GtkViewport *viewport = GTK_VIEWPORT (bin);
  GtkViewportPrivate *priv = viewport->priv;

  g_return_if_fail (gtk_bin_get_child (bin) == NULL);

  gtk_widget_set_parent_window (child, priv->bin_window);
  
  GTK_CONTAINER_CLASS (gtk_viewport_parent_class)->add (container, child);
}

static void
gtk_viewport_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkAllocation clip, content_allocation, widget_allocation;

  /* If our size changed, and we have a shadow, queue a redraw on widget->window to
   * redraw the shadow correctly.
   */
  gtk_widget_get_allocation (widget, &widget_allocation);
  if (gtk_widget_get_mapped (widget) &&
      priv->shadow_type != GTK_SHADOW_NONE &&
      (widget_allocation.width != allocation->width ||
       widget_allocation.height != allocation->height))
    gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

  gtk_widget_set_allocation (widget, allocation);

  content_allocation = *allocation;
  gtk_css_gadget_allocate (priv->gadget,
                           &content_allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_viewport_adjustment_value_changed (GtkAdjustment *adjustment,
				       gpointer       data)
{
  GtkViewport *viewport = GTK_VIEWPORT (data);
  GtkViewportPrivate *priv = viewport->priv;
  GtkBin *bin = GTK_BIN (data);
  GtkWidget *child;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child) &&
      gtk_widget_get_realized (GTK_WIDGET (viewport)))
    {
      GtkAdjustment *hadjustment = priv->hadjustment;
      GtkAdjustment *vadjustment = priv->vadjustment;
      gint old_x, old_y;
      gint new_x, new_y;

      gdk_window_get_position (priv->bin_window, &old_x, &old_y);
      new_x = - gtk_adjustment_get_value (hadjustment);
      new_y = - gtk_adjustment_get_value (vadjustment);

      if (new_x != old_x || new_y != old_y)
	gdk_window_move (priv->bin_window, new_x, new_y);
    }
}
