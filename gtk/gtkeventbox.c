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

#include "gtkeventbox.h"

#include "gtksizerequest.h"

#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkeventbox
 * @Short_description: A widget used to catch events for widgets which
 *     do not have their own window
 * @Title: GtkEventBox
 *
 * The #GtkEventBox widget is a subclass of #GtkBin which also has its
 * own window. It is useful since it allows you to catch events for widgets
 * which do not have their own window.
 */

struct _GtkEventBoxPrivate
{
  gboolean above_child;
  GdkWindow *event_window;
};

enum {
  PROP_0,
  PROP_VISIBLE_WINDOW,
  PROP_ABOVE_CHILD
};

static void     gtk_event_box_realize       (GtkWidget        *widget);
static void     gtk_event_box_unrealize     (GtkWidget        *widget);
static void     gtk_event_box_map           (GtkWidget        *widget);
static void     gtk_event_box_unmap         (GtkWidget        *widget);
static void     gtk_event_box_get_preferred_width  (GtkWidget *widget,
                                                    gint      *minimum,
                                                    gint      *natural);
static void     gtk_event_box_get_preferred_height (GtkWidget *widget,
                                                    gint      *minimum,
                                                    gint      *natural);
static void     gtk_event_box_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
									   gint       width,
									   gint      *minimum,
									   gint      *natural,
									   gint      *minimum_baseline,
									   gint      *natural_baseline);
static void     gtk_event_box_size_allocate (GtkWidget        *widget,
                                             GtkAllocation    *allocation);
static gboolean gtk_event_box_draw          (GtkWidget        *widget,
                                             cairo_t          *cr);
static void     gtk_event_box_set_property  (GObject          *object,
                                             guint             prop_id,
                                             const GValue     *value,
                                             GParamSpec       *pspec);
static void     gtk_event_box_get_property  (GObject          *object,
                                             guint             prop_id,
                                             GValue           *value,
                                             GParamSpec       *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (GtkEventBox, gtk_event_box, GTK_TYPE_BIN)

static void
gtk_event_box_class_init (GtkEventBoxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  gobject_class->set_property = gtk_event_box_set_property;
  gobject_class->get_property = gtk_event_box_get_property;

  widget_class->realize = gtk_event_box_realize;
  widget_class->unrealize = gtk_event_box_unrealize;
  widget_class->map = gtk_event_box_map;
  widget_class->unmap = gtk_event_box_unmap;
  widget_class->get_preferred_width = gtk_event_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_event_box_get_preferred_height;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_event_box_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_event_box_size_allocate;
  widget_class->draw = gtk_event_box_draw;

  gtk_container_class_handle_border_width (container_class);

  g_object_class_install_property (gobject_class,
                                   PROP_VISIBLE_WINDOW,
                                   g_param_spec_boolean ("visible-window",
                                                        P_("Visible Window"),
                                                        P_("Whether the event box is visible, as opposed to invisible and only used to trap events."),
                                                        TRUE,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ABOVE_CHILD,
                                   g_param_spec_boolean ("above-child",
                                                        P_("Above child"),
                                                        P_("Whether the event-trapping window of the eventbox is above the window of the child widget as opposed to below it."),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE));
}

static void
gtk_event_box_init (GtkEventBox *event_box)
{
  gtk_widget_set_has_window (GTK_WIDGET (event_box), TRUE);

  event_box->priv = gtk_event_box_get_instance_private (event_box);
  event_box->priv->above_child = FALSE;
}

/**
 * gtk_event_box_new:
 *
 * Creates a new #GtkEventBox.
 *
 * Returns: a new #GtkEventBox
 */
GtkWidget*
gtk_event_box_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_BOX, NULL);
}

static void
gtk_event_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkEventBox *event_box;

  event_box = GTK_EVENT_BOX (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_WINDOW:
      gtk_event_box_set_visible_window (event_box, g_value_get_boolean (value));
      break;

    case PROP_ABOVE_CHILD:
      gtk_event_box_set_above_child (event_box, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_event_box_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
  GtkEventBox *event_box;

  event_box = GTK_EVENT_BOX (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_WINDOW:
      g_value_set_boolean (value, gtk_event_box_get_visible_window (event_box));
      break;

    case PROP_ABOVE_CHILD:
      g_value_set_boolean (value, gtk_event_box_get_above_child (event_box));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_event_box_get_visible_window:
 * @event_box: a #GtkEventBox
 *
 * Returns whether the event box has a visible window.
 * See gtk_event_box_set_visible_window() for details.
 *
 * Return value: %TRUE if the event box window is visible
 *
 * Since: 2.4
 */
gboolean
gtk_event_box_get_visible_window (GtkEventBox *event_box)
{
  g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), FALSE);

  return gtk_widget_get_has_window (GTK_WIDGET (event_box));
}

/**
 * gtk_event_box_set_visible_window:
 * @event_box: a #GtkEventBox
 * @visible_window: %TRUE to make the event box have a visible window
 *
 * Set whether the event box uses a visible or invisible child
 * window. The default is to use visible windows.
 *
 * In an invisible window event box, the window that the
 * event box creates is a %GDK_INPUT_ONLY window, which
 * means that it is invisible and only serves to receive
 * events.
 *
 * A visible window event box creates a visible (%GDK_INPUT_OUTPUT)
 * window that acts as the parent window for all the widgets
 * contained in the event box.
 *
 * You should generally make your event box invisible if
 * you just want to trap events. Creating a visible window
 * may cause artifacts that are visible to the user, especially
 * if the user is using a theme with gradients or pixmaps.
 *
 * The main reason to create a non input-only event box is if
 * you want to set the background to a different color or
 * draw on it.
 *
 * <note><para>
 * There is one unexpected issue for an invisible event box that has its
 * window below the child. (See gtk_event_box_set_above_child().)
 * Since the input-only window is not an ancestor window of any windows
 * that descendent widgets of the event box create, events on these
 * windows aren't propagated up by the windowing system, but only by GTK+.
 * The practical effect of this is if an event isn't in the event
 * mask for the descendant window (see gtk_widget_add_events()),
 * it won't be received by the event box.
 * </para><para>
 * This problem doesn't occur for visible event boxes, because in
 * that case, the event box window is actually the ancestor of the
 * descendant windows, not just at the same place on the screen.
 * </para></note>
 *
 * Since: 2.4
 */
void
gtk_event_box_set_visible_window (GtkEventBox *event_box,
                                  gboolean     visible_window)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_EVENT_BOX (event_box));

  widget = GTK_WIDGET (event_box);

  visible_window = visible_window != FALSE;

  if (visible_window != gtk_widget_get_has_window (widget))
    {
      if (gtk_widget_get_realized (widget))
        {
          gboolean visible = gtk_widget_get_visible (widget);

          if (visible)
            gtk_widget_hide (widget);

          gtk_widget_unrealize (widget);

          gtk_widget_set_has_window (widget, visible_window);

          gtk_widget_realize (widget);

          if (visible)
            gtk_widget_show (widget);
        }
      else
        {
          gtk_widget_set_has_window (widget, visible_window);
        }

      if (gtk_widget_get_visible (widget))
        gtk_widget_queue_resize (widget);

      g_object_notify (G_OBJECT (event_box), "visible-window");
    }
}

/**
 * gtk_event_box_get_above_child:
 * @event_box: a #GtkEventBox
 *
 * Returns whether the event box window is above or below the
 * windows of its child. See gtk_event_box_set_above_child()
 * for details.
 *
 * Return value: %TRUE if the event box window is above the
 *     window of its child
 *
 * Since: 2.4
 */
gboolean
gtk_event_box_get_above_child (GtkEventBox *event_box)
{
  GtkEventBoxPrivate *priv = event_box->priv;

  g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), FALSE);

  return priv->above_child;
}

/**
 * gtk_event_box_set_above_child:
 * @event_box: a #GtkEventBox
 * @above_child: %TRUE if the event box window is above its child
 *
 * Set whether the event box window is positioned above the windows
 * of its child, as opposed to below it. If the window is above, all
 * events inside the event box will go to the event box. If the window
 * is below, events in windows of child widgets will first got to that
 * widget, and then to its parents.
 *
 * The default is to keep the window below the child.
 *
 * Since: 2.4
 */
void
gtk_event_box_set_above_child (GtkEventBox *event_box,
                               gboolean     above_child)
{
  GtkEventBoxPrivate *priv = event_box->priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_EVENT_BOX (event_box));

  widget = GTK_WIDGET (event_box);

  above_child = above_child != FALSE;

  if (priv->above_child != above_child)
    {
      priv->above_child = above_child;

      if (gtk_widget_get_realized (widget))
        {
          if (!gtk_widget_get_has_window (widget))
            {
              if (above_child)
                gdk_window_raise (priv->event_window);
              else
                gdk_window_lower (priv->event_window);
            }
          else
            {
              gboolean visible = gtk_widget_get_visible (widget);

              if (visible)
                gtk_widget_hide (widget);

              gtk_widget_unrealize (widget);
              gtk_widget_realize (widget);

              if (visible)
                gtk_widget_show (widget);
            }
        }

      if (gtk_widget_get_visible (widget))
        gtk_widget_queue_resize (widget);

      g_object_notify (G_OBJECT (event_box), "above-child");
    }
}

static void
gtk_event_box_realize (GtkWidget *widget)
{
  GtkEventBoxPrivate *priv;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gboolean visible_window;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget)
                        | GDK_BUTTON_MOTION_MASK
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_BUTTON_RELEASE_MASK
                        | GDK_EXPOSURE_MASK
                        | GDK_ENTER_NOTIFY_MASK
                        | GDK_LEAVE_NOTIFY_MASK;

  priv = GTK_EVENT_BOX (widget)->priv;

  visible_window = gtk_widget_get_has_window (widget);
  if (visible_window)
    {
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.wclass = GDK_INPUT_OUTPUT;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

      window = gdk_window_new (gtk_widget_get_parent_window (widget),
                               &attributes, attributes_mask);
      gtk_widget_set_window (widget, window);
      gtk_widget_register_window (widget, window);
    }
  else
    {
      window = gtk_widget_get_parent_window (widget);
      gtk_widget_set_window (widget, window);
      g_object_ref (window);
    }

  if (!visible_window || priv->above_child)
    {
      attributes.wclass = GDK_INPUT_ONLY;
      if (!visible_window)
        attributes_mask = GDK_WA_X | GDK_WA_Y;
      else
        attributes_mask = 0;

      priv->event_window = gdk_window_new (window,
                                           &attributes, attributes_mask);
      gtk_widget_register_window (widget, priv->event_window);
    }

  if (visible_window)
    gtk_style_context_set_background (gtk_widget_get_style_context (widget), window);
}

static void
gtk_event_box_unrealize (GtkWidget *widget)
{
  GtkEventBoxPrivate *priv = GTK_EVENT_BOX (widget)->priv;

  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_event_box_parent_class)->unrealize (widget);
}

static void
gtk_event_box_map (GtkWidget *widget)
{
  GtkEventBoxPrivate *priv = GTK_EVENT_BOX (widget)->priv;

  if (priv->event_window != NULL && !priv->above_child)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gtk_event_box_parent_class)->map (widget);

  if (priv->event_window != NULL && priv->above_child)
    gdk_window_show (priv->event_window);
}

static void
gtk_event_box_unmap (GtkWidget *widget)
{
  GtkEventBoxPrivate *priv = GTK_EVENT_BOX (widget)->priv;

  if (priv->event_window != NULL)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_event_box_parent_class)->unmap (widget);
}

static void
gtk_event_box_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum)
    *minimum = 0;

  if (natural)
    *natural = 0;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_width (child, minimum, natural);
}

static void
gtk_event_box_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
							   gint       width,
							   gint      *minimum,
							   gint      *natural,
							   gint      *minimum_baseline,
							   gint      *natural_baseline)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum)
    *minimum = 0;

  if (natural)
    *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_height_and_baseline_for_width (child,
							    width,
							    minimum,
							    natural,
							    minimum_baseline,
							    natural_baseline);
}

static void
gtk_event_box_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  gtk_event_box_get_preferred_height_and_baseline_for_width (widget, -1,
							     minimum, natural,
							     NULL, NULL);
}

static void
gtk_event_box_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;
  gint baseline;
  GtkEventBoxPrivate *priv;
  GtkWidget *child;

  bin = GTK_BIN (widget);

  gtk_widget_set_allocation (widget, allocation);

  if (!gtk_widget_get_has_window (widget))
    {
      child_allocation.x = allocation->x;
      child_allocation.y = allocation->y;
    }
  else
    {
      child_allocation.x = 0;
      child_allocation.y = 0;
    }
  child_allocation.width = allocation->width;
  child_allocation.height = allocation->height;

  if (gtk_widget_get_realized (widget))
    {
      priv = GTK_EVENT_BOX (widget)->priv;

      if (priv->event_window != NULL)
        gdk_window_move_resize (priv->event_window,
                                child_allocation.x,
                                child_allocation.y,
                                child_allocation.width,
                                child_allocation.height);

      if (gtk_widget_get_has_window (widget))
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x,
                                allocation->y,
                                child_allocation.width,
                                child_allocation.height);
    }

  baseline = gtk_widget_get_allocated_baseline (widget);
  child = gtk_bin_get_child (bin);
  if (child)
    gtk_widget_size_allocate_with_baseline (child, &child_allocation, baseline);
}

static gboolean
gtk_event_box_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  if (gtk_widget_get_has_window (widget) &&
      !gtk_widget_get_app_paintable (widget))
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (widget);

      gtk_render_background (context, cr, 0, 0,
                             gtk_widget_get_allocated_width (widget),
                             gtk_widget_get_allocated_height (widget));
    }

  GTK_WIDGET_CLASS (gtk_event_box_parent_class)->draw (widget, cr);

  return FALSE;
}
