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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * GtkLayout: Widget for scrolling of arbitrary-sized areas.
 *
 * Copyright Owen Taylor, 1998
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtklayout.h"

#include "gdk/gdk.h"

#include "gtkadjustment.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkscrollable.h"


/**
 * SECTION:gtklayout
 * @Short_description: Infinite scrollable area containing child widgets
 *   and/or custom drawing
 * @Title: GtkLayout
 * @See_also: #GtkDrawingArea, #GtkScrolledWindow
 *
 * #GtkLayout is similar to #GtkDrawingArea in that it's a "blank slate"
 * and doesn't do anything but paint a blank background by default. It's
 * different in that it supports scrolling natively (you can add it to a
 * #GtkScrolledWindow), and it can contain child widgets, since it's a
 * #GtkContainer. However if you're just going to draw, a #GtkDrawingArea
 * is a better choice since it has lower overhead.
 *
 * When handling expose events on a #GtkLayout, you must draw to
 * GTK_LAYOUT (layout)->bin_window, rather than to
 * GTK_WIDGET (layout)->window, as you would for a drawing
 * area.
 */


typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutPrivate
{
  /* Properties */
  guint width;
  guint height;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;

  /* Properties */

  GdkVisibilityState visibility;
  GdkWindow *bin_window;

  GList *children;

  gint scroll_x;
  gint scroll_y;

  guint freeze_count;
};

struct _GtkLayoutChild {
  GtkWidget *widget;
  gint x;
  gint y;
};

enum {
   PROP_0,
   PROP_HADJUSTMENT,
   PROP_VADJUSTMENT,
   PROP_HSCROLL_POLICY,
   PROP_VSCROLL_POLICY,
   PROP_WIDTH,
   PROP_HEIGHT
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_X,
  CHILD_PROP_Y
};

static void gtk_layout_get_property       (GObject        *object,
                                           guint           prop_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void gtk_layout_set_property       (GObject        *object,
                                           guint           prop_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static void gtk_layout_finalize           (GObject        *object);
static void gtk_layout_realize            (GtkWidget      *widget);
static void gtk_layout_unrealize          (GtkWidget      *widget);
static void gtk_layout_map                (GtkWidget      *widget);
static void gtk_layout_get_preferred_width  (GtkWidget     *widget,
                                             gint          *minimum,
                                             gint          *natural);
static void gtk_layout_get_preferred_height (GtkWidget     *widget,
                                             gint          *minimum,
                                             gint          *natural);
static void gtk_layout_size_allocate      (GtkWidget      *widget,
                                           GtkAllocation  *allocation);
static gint gtk_layout_draw               (GtkWidget      *widget,
                                           cairo_t        *cr);
static void gtk_layout_add                (GtkContainer   *container,
					   GtkWidget      *widget);
static void gtk_layout_remove             (GtkContainer   *container,
                                           GtkWidget      *widget);
static void gtk_layout_forall             (GtkContainer   *container,
                                           gboolean        include_internals,
                                           GtkCallback     callback,
                                           gpointer        callback_data);
static void gtk_layout_set_child_property (GtkContainer   *container,
                                           GtkWidget      *child,
                                           guint           property_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static void gtk_layout_get_child_property (GtkContainer   *container,
                                           GtkWidget      *child,
                                           guint           property_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void gtk_layout_allocate_child     (GtkLayout      *layout,
                                           GtkLayoutChild *child);
static void gtk_layout_adjustment_changed (GtkAdjustment  *adjustment,
                                           GtkLayout      *layout);
static void gtk_layout_style_updated      (GtkWidget      *widget);

static void gtk_layout_set_hadjustment_values (GtkLayout      *layout);
static void gtk_layout_set_vadjustment_values (GtkLayout      *layout);

G_DEFINE_TYPE_WITH_CODE (GtkLayout, gtk_layout, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkLayout)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

/* Public interface
 */
/**
 * gtk_layout_new:
 * @hadjustment: (allow-none): horizontal scroll adjustment, or %NULL
 * @vadjustment: (allow-none): vertical scroll adjustment, or %NULL
 * 
 * Creates a new #GtkLayout. Unless you have a specific adjustment
 * you'd like the layout to use for scrolling, pass %NULL for
 * @hadjustment and @vadjustment.
 * 
 * Return value: a new #GtkLayout
 **/
  
GtkWidget*    
gtk_layout_new (GtkAdjustment *hadjustment,
		GtkAdjustment *vadjustment)
{
  GtkLayout *layout;

  layout = g_object_new (GTK_TYPE_LAYOUT,
			 "hadjustment", hadjustment,
			 "vadjustment", vadjustment,
			 NULL);

  return GTK_WIDGET (layout);
}

/**
 * gtk_layout_get_bin_window:
 * @layout: a #GtkLayout
 *
 * Retrieve the bin window of the layout used for drawing operations.
 *
 * Return value: (transfer none): a #GdkWindow
 *
 * Since: 2.14
 **/
GdkWindow*
gtk_layout_get_bin_window (GtkLayout *layout)
{
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->priv->bin_window;
}

/**
 * gtk_layout_get_hadjustment:
 * @layout: a #GtkLayout
 *
 * This function should only be called after the layout has been
 * placed in a #GtkScrolledWindow or otherwise configured for
 * scrolling. It returns the #GtkAdjustment used for communication
 * between the horizontal scrollbar and @layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 *
 * Return value: (transfer none): horizontal scroll adjustment
 *
 * Deprecated: 3.0: Use gtk_scrollable_get_hadjustment()
 **/
GtkAdjustment*
gtk_layout_get_hadjustment (GtkLayout *layout)
{
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->priv->hadjustment;
}
/**
 * gtk_layout_get_vadjustment:
 * @layout: a #GtkLayout
 *
 * This function should only be called after the layout has been
 * placed in a #GtkScrolledWindow or otherwise configured for
 * scrolling. It returns the #GtkAdjustment used for communication
 * between the vertical scrollbar and @layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 *
 * Return value: (transfer none): vertical scroll adjustment
 *
 * Deprecated: 3.0: Use gtk_scrollable_get_vadjustment()
 **/
GtkAdjustment*
gtk_layout_get_vadjustment (GtkLayout *layout)
{
  g_return_val_if_fail (GTK_IS_LAYOUT (layout), NULL);

  return layout->priv->vadjustment;
}

static void
gtk_layout_set_hadjustment_values (GtkLayout *layout)
{
  GtkLayoutPrivate *priv = layout->priv;
  GtkAllocation  allocation;
  GtkAdjustment *adj = priv->hadjustment;
  gdouble old_value;
  gdouble new_value;
  gdouble new_upper;

  gtk_widget_get_allocation (GTK_WIDGET (layout), &allocation);

  old_value = gtk_adjustment_get_value (adj);
  new_upper = MAX (allocation.width, priv->width);

  g_object_set (adj,
                "lower", 0.0,
                "upper", new_upper,
                "page-size", (gdouble)allocation.width,
                "step-increment", allocation.width * 0.1,
                "page-increment", allocation.width * 0.9,
                NULL);

  new_value = CLAMP (old_value, 0, new_upper - allocation.width);
  if (new_value != old_value)
    gtk_adjustment_set_value (adj, new_value);
}

static void
gtk_layout_set_vadjustment_values (GtkLayout *layout)
{
  GtkAllocation  allocation;
  GtkAdjustment *adj = layout->priv->vadjustment;
  gdouble old_value;
  gdouble new_value;
  gdouble new_upper;

  gtk_widget_get_allocation (GTK_WIDGET (layout), &allocation);

  old_value = gtk_adjustment_get_value (adj);
  new_upper = MAX (allocation.height, layout->priv->height);

  g_object_set (adj,
                "lower", 0.0,
                "upper", new_upper,
                "page-size", (gdouble)allocation.height,
                "step-increment", allocation.height * 0.1,
                "page-increment", allocation.height * 0.9,
                NULL);

  new_value = CLAMP (old_value, 0, new_upper - allocation.height);
  if (new_value != old_value)
    gtk_adjustment_set_value (adj, new_value);
}

static void
gtk_layout_finalize (GObject *object)
{
  GtkLayout *layout = GTK_LAYOUT (object);
  GtkLayoutPrivate *priv = layout->priv;

  g_object_unref (priv->hadjustment);
  g_object_unref (priv->vadjustment);

  G_OBJECT_CLASS (gtk_layout_parent_class)->finalize (object);
}

static void
gtk_layout_do_set_hadjustment (GtkLayout     *layout,
                               GtkAdjustment *adjustment)
{
  GtkLayoutPrivate *priv;

  priv = layout->priv;

  if (adjustment && priv->hadjustment == adjustment)
        return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment,
                                            gtk_layout_adjustment_changed,
                                            layout);
      g_object_unref (priv->hadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_layout_adjustment_changed), layout);
  priv->hadjustment = g_object_ref_sink (adjustment);
  gtk_layout_set_hadjustment_values (layout);

  g_object_notify (G_OBJECT (layout), "hadjustment");
}

/**
 * gtk_layout_set_hadjustment:
 * @layout: a #GtkLayout
 * @adjustment: (allow-none): new scroll adjustment
 *
 * Sets the horizontal scroll adjustment for the layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 *
 * Deprecated: 3.0: Use gtk_scrollable_set_hadjustment()
 **/
void
gtk_layout_set_hadjustment (GtkLayout     *layout,
                            GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment));

  gtk_layout_do_set_hadjustment (layout, adjustment);
}

static void
gtk_layout_do_set_vadjustment (GtkLayout     *layout,
                               GtkAdjustment *adjustment)
{
  GtkLayoutPrivate *priv;

  priv = layout->priv;

  if (adjustment && priv->vadjustment == adjustment)
        return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment,
                                            gtk_layout_adjustment_changed,
                                            layout);
      g_object_unref (priv->vadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_layout_adjustment_changed), layout);
  priv->vadjustment = g_object_ref_sink (adjustment);
  gtk_layout_set_vadjustment_values (layout);

  g_object_notify (G_OBJECT (layout), "vadjustment");
}

/**
 * gtk_layout_set_vadjustment:
 * @layout: a #GtkLayout
 * @adjustment: (allow-none): new scroll adjustment
 *
 * Sets the vertical scroll adjustment for the layout.
 *
 * See #GtkScrolledWindow, #GtkScrollbar, #GtkAdjustment for details.
 *
 * Deprecated: 3.0: Use gtk_scrollable_set_vadjustment()
 **/
void
gtk_layout_set_vadjustment (GtkLayout     *layout,
                            GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment));

  gtk_layout_do_set_vadjustment (layout, adjustment);
}

static GtkLayoutChild*
get_child (GtkLayout  *layout,
           GtkWidget  *widget)
{
  GtkLayoutPrivate *priv = layout->priv;
  GList *children;

  children = priv->children;
  while (children)
    {
      GtkLayoutChild *child;
      
      child = children->data;
      children = children->next;

      if (child->widget == widget)
        return child;
    }

  return NULL;
}

/**
 * gtk_layout_put:
 * @layout: a #GtkLayout
 * @child_widget: child widget
 * @x: X position of child widget
 * @y: Y position of child widget
 *
 * Adds @child_widget to @layout, at position (@x,@y).
 * @layout becomes the new parent container of @child_widget.
 * 
 **/
void           
gtk_layout_put (GtkLayout     *layout, 
		GtkWidget     *child_widget, 
		gint           x, 
		gint           y)
{
  GtkLayoutPrivate *priv;
  GtkLayoutChild *child;

  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));

  priv = layout->priv;

  child = g_new (GtkLayoutChild, 1);

  child->widget = child_widget;
  child->x = x;
  child->y = y;

  priv->children = g_list_append (priv->children, child);

  if (gtk_widget_get_realized (GTK_WIDGET (layout)))
    gtk_widget_set_parent_window (child->widget, priv->bin_window);

  gtk_widget_set_parent (child_widget, GTK_WIDGET (layout));
}

static void
gtk_layout_move_internal (GtkLayout       *layout,
                          GtkWidget       *widget,
                          gboolean         change_x,
                          gint             x,
                          gboolean         change_y,
                          gint             y)
{
  GtkLayoutChild *child;

  child = get_child (layout, widget);

  g_assert (child);

  gtk_widget_freeze_child_notify (widget);
  
  if (change_x)
    {
      child->x = x;
      gtk_widget_child_notify (widget, "x");
    }

  if (change_y)
    {
      child->y = y;
      gtk_widget_child_notify (widget, "y");
    }

  gtk_widget_thaw_child_notify (widget);
  
  if (gtk_widget_get_visible (widget) &&
      gtk_widget_get_visible (GTK_WIDGET (layout)))
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_layout_move:
 * @layout: a #GtkLayout
 * @child_widget: a current child of @layout
 * @x: X position to move to
 * @y: Y position to move to
 *
 * Moves a current child of @layout to a new position.
 * 
 **/
void           
gtk_layout_move (GtkLayout     *layout, 
		 GtkWidget     *child_widget, 
		 gint           x, 
		 gint           y)
{
  g_return_if_fail (GTK_IS_LAYOUT (layout));
  g_return_if_fail (GTK_IS_WIDGET (child_widget));
  g_return_if_fail (gtk_widget_get_parent (child_widget) == GTK_WIDGET (layout));

  gtk_layout_move_internal (layout, child_widget, TRUE, x, TRUE, y);
}

/**
 * gtk_layout_set_size:
 * @layout: a #GtkLayout
 * @width: width of entire scrollable area
 * @height: height of entire scrollable area
 *
 * Sets the size of the scrollable area of the layout.
 * 
 **/
void
gtk_layout_set_size (GtkLayout     *layout, 
		     guint          width,
		     guint          height)
{
  GtkLayoutPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_LAYOUT (layout));

  priv = layout->priv;
  widget = GTK_WIDGET (layout);

  g_object_freeze_notify (G_OBJECT (layout));
  if (width != priv->width)
     {
	priv->width = width;
	g_object_notify (G_OBJECT (layout), "width");
     }
  if (height != priv->height)
     {
	priv->height = height;
	g_object_notify (G_OBJECT (layout), "height");
     }
  g_object_thaw_notify (G_OBJECT (layout));

  if (gtk_widget_get_realized (widget))
    {
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      width = MAX (width, allocation.width);
      height = MAX (height, allocation.height);
      gdk_window_resize (priv->bin_window, width, height);
    }

  gtk_layout_set_hadjustment_values (layout);
  gtk_layout_set_vadjustment_values (layout);
}

/**
 * gtk_layout_get_size:
 * @layout: a #GtkLayout
 * @width: (out) (allow-none): location to store the width set on
 *     @layout, or %NULL
 * @height: (out) (allow-none): location to store the height set on
 *     @layout, or %NULL
 *
 * Gets the size that has been set on the layout, and that determines
 * the total extents of the layout's scrollbar area. See
 * gtk_layout_set_size ().
 **/
void
gtk_layout_get_size (GtkLayout *layout,
		     guint     *width,
		     guint     *height)
{
  GtkLayoutPrivate *priv;

  g_return_if_fail (GTK_IS_LAYOUT (layout));

  priv = layout->priv;

  if (width)
    *width = priv->width;
  if (height)
    *height = priv->height;
}

/* Basic Object handling procedures
 */
static void
gtk_layout_class_init (GtkLayoutClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_layout_set_property;
  gobject_class->get_property = gtk_layout_get_property;
  gobject_class->finalize = gtk_layout_finalize;

  container_class->set_child_property = gtk_layout_set_child_property;
  container_class->get_child_property = gtk_layout_get_child_property;

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_X,
					      g_param_spec_int ("x",
                                                                P_("X position"),
                                                                P_("X position of child widget"),
                                                                G_MININT,
                                                                G_MAXINT,
                                                                0,
                                                                GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_Y,
					      g_param_spec_int ("y",
                                                                P_("Y position"),
                                                                P_("Y position of child widget"),
                                                                G_MININT,
                                                                G_MAXINT,
                                                                0,
                                                                GTK_PARAM_READWRITE));
  
  /* Scrollable interface */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  g_object_class_install_property (gobject_class,
				   PROP_WIDTH,
				   g_param_spec_uint ("width",
						     P_("Width"),
						     P_("The width of the layout"),
						     0,
						     G_MAXINT,
						     100,
						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HEIGHT,
				   g_param_spec_uint ("height",
						     P_("Height"),
						     P_("The height of the layout"),
						     0,
						     G_MAXINT,
						     100,
						     GTK_PARAM_READWRITE));
  widget_class->realize = gtk_layout_realize;
  widget_class->unrealize = gtk_layout_unrealize;
  widget_class->map = gtk_layout_map;
  widget_class->get_preferred_width = gtk_layout_get_preferred_width;
  widget_class->get_preferred_height = gtk_layout_get_preferred_height;
  widget_class->size_allocate = gtk_layout_size_allocate;
  widget_class->draw = gtk_layout_draw;
  widget_class->style_updated = gtk_layout_style_updated;

  container_class->add = gtk_layout_add;
  container_class->remove = gtk_layout_remove;
  container_class->forall = gtk_layout_forall;
}

static void
gtk_layout_get_property (GObject     *object,
			 guint        prop_id,
			 GValue      *value,
			 GParamSpec  *pspec)
{
  GtkLayout *layout = GTK_LAYOUT (object);
  GtkLayoutPrivate *priv = layout->priv;

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
    case PROP_WIDTH:
      g_value_set_uint (value, priv->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, priv->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_layout_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
  GtkLayout *layout = GTK_LAYOUT (object);
  GtkLayoutPrivate *priv = layout->priv;

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_layout_do_set_hadjustment (layout, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_layout_do_set_vadjustment (layout, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      priv->hscroll_policy = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (layout));
      break;
    case PROP_VSCROLL_POLICY:
      priv->vscroll_policy = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (layout));
      break;
    case PROP_WIDTH:
      gtk_layout_set_size (layout, g_value_get_uint (value),
			   priv->height);
      break;
    case PROP_HEIGHT:
      gtk_layout_set_size (layout, priv->width,
			   g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_layout_set_child_property (GtkContainer    *container,
                               GtkWidget       *child,
                               guint            property_id,
                               const GValue    *value,
                               GParamSpec      *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_X:
      gtk_layout_move_internal (GTK_LAYOUT (container),
                                child,
                                TRUE, g_value_get_int (value),
                                FALSE, 0);
      break;
    case CHILD_PROP_Y:
      gtk_layout_move_internal (GTK_LAYOUT (container),
                                child,
                                FALSE, 0,
                                TRUE, g_value_get_int (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_layout_get_child_property (GtkContainer *container,
                               GtkWidget    *child,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkLayoutChild *layout_child;

  layout_child = get_child (GTK_LAYOUT (container), child);
  
  switch (property_id)
    {
    case CHILD_PROP_X:
      g_value_set_int (value, layout_child->x);
      break;
    case CHILD_PROP_Y:
      g_value_set_int (value, layout_child->y);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_layout_init (GtkLayout *layout)
{
  GtkLayoutPrivate *priv;

  layout->priv = gtk_layout_get_instance_private (layout);
  priv = layout->priv;

  priv->children = NULL;

  priv->width = 100;
  priv->height = 100;

  priv->hadjustment = NULL;
  priv->vadjustment = NULL;

  priv->bin_window = NULL;

  priv->scroll_x = 0;
  priv->scroll_y = 0;
  priv->visibility = GDK_VISIBILITY_PARTIAL;

  priv->freeze_count = 0;
}

/* Widget methods
 */

static void 
gtk_layout_realize (GtkWidget *widget)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GtkLayoutPrivate *priv = layout->priv;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  GList *tmp_list;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = - gtk_adjustment_get_value (priv->hadjustment),
  attributes.y = - gtk_adjustment_get_value (priv->vadjustment);
  attributes.width = MAX (priv->width, allocation.width);
  attributes.height = MAX (priv->height, allocation.height);
  attributes.event_mask = GDK_EXPOSURE_MASK | GDK_SCROLL_MASK |
                          GDK_SMOOTH_SCROLL_MASK | 
                          gtk_widget_get_events (widget);

  priv->bin_window = gdk_window_new (window,
                                     &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->bin_window);
  gtk_style_context_set_background (gtk_widget_get_style_context (widget), priv->bin_window);

  tmp_list = priv->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_widget_set_parent_window (child->widget, priv->bin_window);
    }
}

static void
gtk_layout_style_updated (GtkWidget *widget)
{
  GtkLayoutPrivate *priv;

  GTK_WIDGET_CLASS (gtk_layout_parent_class)->style_updated (widget);

  if (gtk_widget_get_realized (widget))
    {
      priv = GTK_LAYOUT (widget)->priv;
      gtk_style_context_set_background (gtk_widget_get_style_context (widget), priv->bin_window);
    }
}

static void
gtk_layout_map (GtkWidget *widget)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GtkLayoutPrivate *priv = layout->priv;
  GList *tmp_list;

  gtk_widget_set_mapped (widget, TRUE);

  tmp_list = priv->children;
  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (gtk_widget_get_visible (child->widget))
	{
	  if (!gtk_widget_get_mapped (child->widget))
	    gtk_widget_map (child->widget);
	}
    }

  gdk_window_show (priv->bin_window);
  gdk_window_show (gtk_widget_get_window (widget));
}

static void 
gtk_layout_unrealize (GtkWidget *widget)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GtkLayoutPrivate *priv = layout->priv;

  gtk_widget_unregister_window (widget, priv->bin_window);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;

  GTK_WIDGET_CLASS (gtk_layout_parent_class)->unrealize (widget);
}

static void
gtk_layout_get_preferred_width (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  *minimum = *natural = 0;
}

static void
gtk_layout_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
  *minimum = *natural = 0;
}

static void
gtk_layout_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GtkLayoutPrivate *priv = layout->priv;
  GList *tmp_list;

  gtk_widget_set_allocation (widget, allocation);

  tmp_list = priv->children;

  while (tmp_list)
    {
      GtkLayoutChild *child = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_layout_allocate_child (layout, child);
    }

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_resize (priv->bin_window,
			 MAX (priv->width, allocation->width),
			 MAX (priv->height, allocation->height));
    }

  gtk_layout_set_hadjustment_values (layout);
  gtk_layout_set_vadjustment_values (layout);
}

static gboolean
gtk_layout_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  GtkLayout *layout = GTK_LAYOUT (widget);
  GtkLayoutPrivate *priv = layout->priv;

  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    GTK_WIDGET_CLASS (gtk_layout_parent_class)->draw (widget, cr);

  return FALSE;
}

/* Container methods
 */
static void
gtk_layout_add (GtkContainer *container,
		GtkWidget    *widget)
{
  gtk_layout_put (GTK_LAYOUT (container), widget, 0, 0);
}

static void
gtk_layout_remove (GtkContainer *container, 
		   GtkWidget    *widget)
{
  GtkLayout *layout = GTK_LAYOUT (container);
  GtkLayoutPrivate *priv = layout->priv;
  GList *tmp_list;
  GtkLayoutChild *child = NULL;

  tmp_list = priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)
    {
      gtk_widget_unparent (widget);

      priv->children = g_list_remove_link (priv->children, tmp_list);
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
  GtkLayout *layout = GTK_LAYOUT (container);
  GtkLayoutPrivate *priv = layout->priv;
  GtkLayoutChild *child;
  GList *tmp_list;

  tmp_list = priv->children;
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

  allocation.x = child->x;
  allocation.y = child->y;

  gtk_widget_get_preferred_size (child->widget, &requisition, NULL);
  allocation.width = requisition.width;
  allocation.height = requisition.height;
  
  gtk_widget_size_allocate (child->widget, &allocation);
}

/* Callbacks */

static void
gtk_layout_adjustment_changed (GtkAdjustment *adjustment,
			       GtkLayout     *layout)
{
  GtkLayoutPrivate *priv = layout->priv;

  if (priv->freeze_count)
    return;

  if (gtk_widget_get_realized (GTK_WIDGET (layout)))
    {
      gdk_window_move (priv->bin_window,
		       - gtk_adjustment_get_value (priv->hadjustment),
		       - gtk_adjustment_get_value (priv->vadjustment));
    }
}
