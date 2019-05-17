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

#include "gtkscrolledwindow.h"

#include "gtkadjustment.h"
#include "gtkadjustmentprivate.h"
#include "gtkbindings.h"
#include "gtkdnd.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkgesturedrag.h"
#include "gtkgesturelongpress.h"
#include "gtkgesturepan.h"
#include "gtkgesturesingle.h"
#include "gtkgestureswipe.h"
#include "gtkintl.h"
#include "gtkkineticscrollingprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtkscrollable.h"
#include "gtkscrollbar.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkviewport.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkscrolledwindowaccessible.h"

#include <math.h>

/**
 * SECTION:gtkscrolledwindow
 * @Short_description: Adds scrollbars to its child widget
 * @Title: GtkScrolledWindow
 * @See_also: #GtkScrollable, #GtkViewport, #GtkAdjustment
 *
 * GtkScrolledWindow is a container that accepts a single child widget, makes
 * that child scrollable using either internally added scrollbars or externally
 * associated adjustments, and optionally draws a frame around the child.
 *
 * Widgets with native scrolling support, i.e. those whose classes implement the
 * #GtkScrollable interface, are added directly. For other types of widget, the
 * class #GtkViewport acts as an adaptor, giving scrollability to other widgets.
 * GtkScrolledWindow’s implementation of gtk_container_add() intelligently
 * accounts for whether or not the added child is a #GtkScrollable. If it isn’t,
 * #GtkScrolledWindow wraps the child in a #GtkViewport and adds that for you.
 * Therefore, you can just add any child widget and not worry about the details.
 *
 * If gtk_container_add() has added a #GtkViewport for you, you can remove
 * both your added child widget from the #GtkViewport, and the #GtkViewport
 * from the GtkScrolledWindow, like this:
 *
 * |[<!-- language="C" -->
 * GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
 * GtkWidget *child_widget = gtk_button_new ();
 *
 * // GtkButton is not a GtkScrollable, so GtkScrolledWindow will automatically
 * // add a GtkViewport.
 * gtk_container_add (GTK_CONTAINER (scrolled_window),
 *                    child_widget);
 *
 * // Either of these will result in child_widget being unparented:
 * gtk_container_remove (GTK_CONTAINER (scrolled_window),
 *                       child_widget);
 * // or
 * gtk_container_remove (GTK_CONTAINER (scrolled_window),
 *                       gtk_bin_get_child (GTK_BIN (scrolled_window)));
 * ]|
 *
 * Unless #GtkScrolledWindow:hscrolbar-policy and #GtkScrolledWindow:vscrollbar-policy
 * are GTK_POLICY_NEVER or GTK_POLICY_EXTERNAL,
 * GtkScrolledWindow adds internal #GtkScrollbar widgets around its child. The
 * scroll position of the child, and if applicable the scrollbars, is controlled
 * by the #GtkScrolledWindow:hadjustment and #GtkScrolledWindow:vadjustment
 * that are associated with the GtkScrolledWindow. See the docs on #GtkScrollbar
 * for the details, but note that the “step_increment” and “page_increment”
 * fields are only effective if the policy causes scrollbars to be present.
 *
 * If a GtkScrolledWindow doesn’t behave quite as you would like, or
 * doesn’t have exactly the right layout, it’s very possible to set up
 * your own scrolling with #GtkScrollbar and for example a #GtkGrid.
 *
 * # Touch support
 *
 * GtkScrolledWindow has built-in support for touch devices. When a
 * touchscreen is used, swiping will move the scrolled window, and will
 * expose 'kinetic' behavior. This can be turned off with the
 * #GtkScrolledWindow:kinetic-scrolling property if it is undesired.
 *
 * GtkScrolledWindow also displays visual 'overshoot' indication when
 * the content is pulled beyond the end, and this situation can be
 * captured with the #GtkScrolledWindow::edge-overshot signal.
 *
 * If no mouse device is present, the scrollbars will overlayed as
 * narrow, auto-hiding indicators over the content. If traditional
 * scrollbars are desired although no mouse is present, this behaviour
 * can be turned off with the #GtkScrolledWindow:overlay-scrolling
 * property.
 *
 * # CSS nodes
 *
 * GtkScrolledWindow has a main CSS node with name scrolledwindow.
 *
 * It uses subnodes with names overshoot and undershoot to
 * draw the overflow and underflow indications. These nodes get
 * the .left, .right, .top or .bottom style class added depending
 * on where the indication is drawn.
 *
 * GtkScrolledWindow also sets the positional style classes (.left,
 * .right, .top, .bottom) and style classes related to overlay
 * scrolling (.overlay-indicator, .dragging, .hovering) on its scrollbars.
 *
 * If both scrollbars are visible, the area where they meet is drawn
 * with a subnode named junction.
 */


/* scrolled window policy and size requisition handling:
 *
 * gtk size requisition works as follows:
 *   a widget upon size-request reports the width and height that it finds
 *   to be best suited to display its contents, including children.
 *   the width and/or height reported from a widget upon size requisition
 *   may be overidden by the user by specifying a width and/or height
 *   other than 0 through gtk_widget_set_size_request().
 *
 * a scrolled window needs (for implementing all three policy types) to
 * request its width and height based on two different rationales.
 * 1)   the user wants the scrolled window to just fit into the space
 *      that it gets allocated for a specifc dimension.
 * 1.1) this does not apply if the user specified a concrete value
 *      value for that specific dimension by either specifying usize for the
 *      scrolled window or for its child.
 * 2)   the user wants the scrolled window to take as much space up as
 *      is desired by the child for a specifc dimension (i.e. POLICY_NEVER).
 *
 * also, kinda obvious:
 * 3)   a user would certainly not have choosen a scrolled window as a container
 *      for the child, if the resulting allocation takes up more space than the
 *      child would have allocated without the scrolled window.
 *
 * conclusions:
 * A) from 1) follows: the scrolled window shouldn’t request more space for a
 *    specifc dimension than is required at minimum.
 * B) from 1.1) follows: the requisition may be overidden by usize of the scrolled
 *    window (done automatically) or by usize of the child (needs to be checked).
 * C) from 2) follows: for POLICY_NEVER, the scrolled window simply reports the
 *    child’s dimension.
 * D) from 3) follows: the scrolled window child’s minimum width and minimum height
 *    under A) at least correspond to the space taken up by its scrollbars.
 */

/* Kinetic scrolling */
#define MAX_OVERSHOOT_DISTANCE 100
#define DECELERATION_FRICTION 4
#define OVERSHOOT_FRICTION 20

/* Animated scrolling */
#define ANIMATION_DURATION 200

/* Overlay scrollbars */
#define INDICATOR_FADE_OUT_DELAY 2000
#define INDICATOR_FADE_OUT_DURATION 1000
#define INDICATOR_FADE_OUT_TIME 500
#define INDICATOR_CLOSE_DISTANCE 5
#define INDICATOR_FAR_DISTANCE 10

/* Scrolled off indication */
#define UNDERSHOOT_SIZE 40

typedef struct
{
  GtkWidget *scrollbar;
  gboolean   over; /* either mouse over, or while dragging */
  gint64     last_scroll_time;
  guint      conceil_timer;

  gdouble    current_pos;
  gdouble    source_pos;
  gdouble    target_pos;
  GtkProgressTracker tracker;
  guint      tick_id;
  guint      over_timeout_id;
} Indicator;

typedef struct
{
  GtkWidget     *hscrollbar;
  GtkWidget     *vscrollbar;

  GtkCssNode    *overshoot_node[4];
  GtkCssNode    *undershoot_node[4];

  Indicator hindicator;
  Indicator vindicator;

  GtkCornerType  window_placement;
  guint16  shadow_type;

  guint    hscrollbar_policy        : 2;
  guint    vscrollbar_policy        : 2;
  guint    hscrollbar_visible       : 1;
  guint    vscrollbar_visible       : 1;
  guint    focus_out                : 1; /* used by ::move-focus-out implementation */
  guint    overlay_scrolling        : 1;
  guint    use_indicators           : 1;
  guint    auto_added_viewport      : 1;
  guint    propagate_natural_width  : 1;
  guint    propagate_natural_height : 1;
  guint    smooth_scroll            : 1;

  gint     min_content_width;
  gint     min_content_height;
  gint     max_content_width;
  gint     max_content_height;

  guint scroll_events_overshoot_id;

  /* Kinetic scrolling */
  GtkGesture *long_press_gesture;
  GtkGesture *swipe_gesture;

  /* These two gestures are mutually exclusive */
  GtkGesture *drag_gesture;
  GtkGesture *pan_gesture;

  gdouble drag_start_x;
  gdouble drag_start_y;

  guint                  kinetic_scrolling         : 1;
  guint                  capture_button_press      : 1;
  guint                  in_drag                   : 1;

  guint                  deceleration_id;

  gdouble                x_velocity;
  gdouble                y_velocity;

  gdouble                unclamped_hadj_value;
  gdouble                unclamped_vadj_value;
} GtkScrolledWindowPrivate;

typedef struct
{
  GtkScrolledWindow     *scrolled_window;
  gint64                 last_deceleration_time;

  GtkKineticScrolling   *hscrolling;
  GtkKineticScrolling   *vscrolling;
} KineticScrollData;

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_WINDOW_PLACEMENT,
  PROP_SHADOW_TYPE,
  PROP_MIN_CONTENT_WIDTH,
  PROP_MIN_CONTENT_HEIGHT,
  PROP_KINETIC_SCROLLING,
  PROP_OVERLAY_SCROLLING,
  PROP_MAX_CONTENT_WIDTH,
  PROP_MAX_CONTENT_HEIGHT,
  PROP_PROPAGATE_NATURAL_WIDTH,
  PROP_PROPAGATE_NATURAL_HEIGHT,
  NUM_PROPERTIES
};

/* Signals */
enum
{
  SCROLL_CHILD,
  MOVE_FOCUS_OUT,
  EDGE_OVERSHOT,
  EDGE_REACHED,
  LAST_SIGNAL
};

static void     gtk_scrolled_window_set_property       (GObject           *object,
                                                        guint              prop_id,
                                                        const GValue      *value,
                                                        GParamSpec        *pspec);
static void     gtk_scrolled_window_get_property       (GObject           *object,
                                                        guint              prop_id,
                                                        GValue            *value,
                                                        GParamSpec        *pspec);

static void     gtk_scrolled_window_destroy            (GtkWidget         *widget);
static void     gtk_scrolled_window_snapshot           (GtkWidget         *widget,
                                                        GtkSnapshot       *snapshot);
static void     gtk_scrolled_window_size_allocate      (GtkWidget         *widget,
                                                        int                width,
                                                        int                height,
                                                        int                baseline);
static gboolean gtk_scrolled_window_focus              (GtkWidget         *widget,
                                                        GtkDirectionType   direction);
static void     gtk_scrolled_window_add                (GtkContainer      *container,
                                                        GtkWidget         *widget);
static void     gtk_scrolled_window_remove             (GtkContainer      *container,
                                                        GtkWidget         *widget);
static gboolean gtk_scrolled_window_scroll_child       (GtkScrolledWindow *scrolled_window,
                                                        GtkScrollType      scroll,
                                                        gboolean           horizontal);
static void     gtk_scrolled_window_move_focus_out     (GtkScrolledWindow *scrolled_window,
                                                        GtkDirectionType   direction_type);

static void     gtk_scrolled_window_relative_allocation(GtkScrolledWindow *scrolled_window,
                                                        GtkAllocation     *allocation);
static void     gtk_scrolled_window_inner_allocation   (GtkScrolledWindow *scrolled_window,
                                                        GtkAllocation     *rect);
static void     gtk_scrolled_window_allocate_scrollbar (GtkScrolledWindow *scrolled_window,
                                                        GtkWidget         *scrollbar,
                                                        GtkAllocation     *allocation);
static void     gtk_scrolled_window_allocate_child     (GtkScrolledWindow   *swindow,
                                                        int                  width,
                                                        int                  height);
static void     gtk_scrolled_window_adjustment_changed (GtkAdjustment     *adjustment,
                                                        gpointer           data);
static void     gtk_scrolled_window_adjustment_value_changed (GtkAdjustment     *adjustment,
                                                              gpointer           data);
static gboolean gtk_widget_should_animate              (GtkWidget           *widget);
static void     gtk_scrolled_window_measure (GtkWidget      *widget,
                                             GtkOrientation  orientation,
                                             int             for_size,
                                             int            *minimum_size,
                                             int            *natural_size,
                                             int            *minimum_baseline,
                                             int            *natural_baseline);
static void  gtk_scrolled_window_map                   (GtkWidget           *widget);
static void  gtk_scrolled_window_unmap                 (GtkWidget           *widget);
static void  gtk_scrolled_window_realize               (GtkWidget           *widget);
static void _gtk_scrolled_window_set_adjustment_value  (GtkScrolledWindow *scrolled_window,
                                                        GtkAdjustment     *adjustment,
                                                        gdouble            value);

static void gtk_scrolled_window_cancel_deceleration (GtkScrolledWindow *scrolled_window);

static gboolean _gtk_scrolled_window_get_overshoot (GtkScrolledWindow *scrolled_window,
                                                    gint              *overshoot_x,
                                                    gint              *overshoot_y);

static void     gtk_scrolled_window_start_deceleration (GtkScrolledWindow *scrolled_window);

static void     gtk_scrolled_window_update_use_indicators (GtkScrolledWindow *scrolled_window);
static void     remove_indicator     (GtkScrolledWindow *sw,
                                      Indicator         *indicator);
static gboolean maybe_hide_indicator (gpointer data);

static void     indicator_start_fade (Indicator *indicator,
                                      gdouble    pos);
static void     indicator_set_over   (Indicator *indicator,
                                      gboolean   over);

static void     install_scroll_cursor (GtkScrolledWindow *scrolled_window);
static void     uninstall_scroll_cursor (GtkScrolledWindow *scrolled_window);

static guint signals[LAST_SIGNAL] = {0};
static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE (GtkScrolledWindow, gtk_scrolled_window, GTK_TYPE_BIN)

static void
add_scroll_binding (GtkBindingSet  *binding_set,
		    guint           keyval,
		    GdkModifierType mask,
		    GtkScrollType   scroll,
		    gboolean        horizontal)
{
  guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keyval, mask,
                                "scroll-child", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
  gtk_binding_entry_add_signal (binding_set, keypad_keyval, mask,
                                "scroll-child", 2,
                                GTK_TYPE_SCROLL_TYPE, scroll,
				G_TYPE_BOOLEAN, horizontal);
}

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
                                "move-focus-out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
                                "move-focus-out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
motion_controller_leave (GtkEventController *controller,
                         GdkCrossingMode     mode,
                         GdkNotifyType       detail,
                         GtkScrolledWindow  *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->use_indicators)
    {
      indicator_set_over (&priv->hindicator, FALSE);
      indicator_set_over (&priv->vindicator, FALSE);
    }
}

static void
update_scrollbar_positions (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkStyleContext *context;
  gboolean is_rtl;

  if (priv->hscrollbar != NULL)
    {
      context = gtk_widget_get_style_context (priv->hscrollbar);
      if (priv->window_placement == GTK_CORNER_TOP_LEFT ||
          priv->window_placement == GTK_CORNER_TOP_RIGHT)
        {
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
          gtk_style_context_remove_class (context, GTK_STYLE_CLASS_TOP);
        }
      else
        {
          gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BOTTOM);
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);
        }
    }

  if (priv->vscrollbar != NULL)
    {
      context = gtk_widget_get_style_context (priv->vscrollbar);
      is_rtl = _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL;
      if ((is_rtl &&
          (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
           priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
         (!is_rtl &&
          (priv->window_placement == GTK_CORNER_TOP_LEFT ||
           priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        {
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);
          gtk_style_context_remove_class (context, GTK_STYLE_CLASS_LEFT);
        }
      else
        {
          gtk_style_context_remove_class (context, GTK_STYLE_CLASS_RIGHT);
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);
        }
    }
}

static void
gtk_scrolled_window_direction_changed (GtkWidget        *widget,
                                       GtkTextDirection  previous_dir)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);

  update_scrollbar_positions (scrolled_window);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_scrolled_window_class_init (GtkScrolledWindowClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_scrolled_window_set_property;
  gobject_class->get_property = gtk_scrolled_window_get_property;

  widget_class->destroy = gtk_scrolled_window_destroy;
  widget_class->snapshot = gtk_scrolled_window_snapshot;
  widget_class->size_allocate = gtk_scrolled_window_size_allocate;
  widget_class->focus = gtk_scrolled_window_focus;
  widget_class->measure = gtk_scrolled_window_measure;
  widget_class->map = gtk_scrolled_window_map;
  widget_class->unmap = gtk_scrolled_window_unmap;
  widget_class->realize = gtk_scrolled_window_realize;
  widget_class->direction_changed = gtk_scrolled_window_direction_changed;

  container_class->add = gtk_scrolled_window_add;
  container_class->remove = gtk_scrolled_window_remove;

  class->scroll_child = gtk_scrolled_window_scroll_child;
  class->move_focus_out = gtk_scrolled_window_move_focus_out;

  properties[PROP_HADJUSTMENT] =
      g_param_spec_object ("hadjustment",
                           P_("Horizontal Adjustment"),
                           P_("The GtkAdjustment for the horizontal position"),
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT);

  properties[PROP_VADJUSTMENT] =
      g_param_spec_object ("vadjustment",
                           P_("Vertical Adjustment"),
                           P_("The GtkAdjustment for the vertical position"),
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT);

  properties[PROP_HSCROLLBAR_POLICY] =
      g_param_spec_enum ("hscrollbar-policy",
                         P_("Horizontal Scrollbar Policy"),
                         P_("When the horizontal scrollbar is displayed"),
                         GTK_TYPE_POLICY_TYPE,
                         GTK_POLICY_AUTOMATIC,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_VSCROLLBAR_POLICY] =
      g_param_spec_enum ("vscrollbar-policy",
                         P_("Vertical Scrollbar Policy"),
                         P_("When the vertical scrollbar is displayed"),
			GTK_TYPE_POLICY_TYPE,
			GTK_POLICY_AUTOMATIC,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_WINDOW_PLACEMENT] =
      g_param_spec_enum ("window-placement",
                         P_("Window Placement"),
                         P_("Where the contents are located with respect to the scrollbars."),
			GTK_TYPE_CORNER_TYPE,
			GTK_CORNER_TOP_LEFT,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHADOW_TYPE] =
      g_param_spec_enum ("shadow-type",
                         P_("Shadow Type"),
                         P_("Style of bevel around the contents"),
			GTK_TYPE_SHADOW_TYPE,
			GTK_SHADOW_NONE,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:min-content-width:
   *
   * The minimum content width of @scrolled_window, or -1 if not set.
   */
  properties[PROP_MIN_CONTENT_WIDTH] =
      g_param_spec_int ("min-content-width",
                        P_("Minimum Content Width"),
                        P_("The minimum width that the scrolled window will allocate to its content"),
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:min-content-height:
   *
   * The minimum content height of @scrolled_window, or -1 if not set.
   */
  properties[PROP_MIN_CONTENT_HEIGHT] =
      g_param_spec_int ("min-content-height",
                        P_("Minimum Content Height"),
                        P_("The minimum height that the scrolled window will allocate to its content"),
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:kinetic-scrolling:
   *
   * Whether kinetic scrolling is enabled or not. Kinetic scrolling
   * only applies to devices with source %GDK_SOURCE_TOUCHSCREEN.
   */
  properties[PROP_KINETIC_SCROLLING] =
      g_param_spec_boolean ("kinetic-scrolling",
                            P_("Kinetic Scrolling"),
                            P_("Kinetic scrolling mode."),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:overlay-scrolling:
   *
   * Whether overlay scrolling is enabled or not. If it is, the
   * scrollbars are only added as traditional widgets when a mouse
   * is present. Otherwise, they are overlayed on top of the content,
   * as narrow indicators.
   */
  properties[PROP_OVERLAY_SCROLLING] =
      g_param_spec_boolean ("overlay-scrolling",
                            P_("Overlay Scrolling"),
                            P_("Overlay scrolling mode"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:max-content-width:
   *
   * The maximum content width of @scrolled_window, or -1 if not set.
   */
  properties[PROP_MAX_CONTENT_WIDTH] =
      g_param_spec_int ("max-content-width",
                        P_("Maximum Content Width"),
                        P_("The maximum width that the scrolled window will allocate to its content"),
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:max-content-height:
   *
   * The maximum content height of @scrolled_window, or -1 if not set.
   */
  properties[PROP_MAX_CONTENT_HEIGHT] =
      g_param_spec_int ("max-content-height",
                        P_("Maximum Content Height"),
                        P_("The maximum height that the scrolled window will allocate to its content"),
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:propagate-natural-width:
   *
   * Whether the natural width of the child should be calculated and propagated
   * through the scrolled window’s requested natural width.
   *
   * This is useful in cases where an attempt should be made to allocate exactly
   * enough space for the natural size of the child.
   */
  properties[PROP_PROPAGATE_NATURAL_WIDTH] =
      g_param_spec_boolean ("propagate-natural-width",
                            P_("Propagate Natural Width"),
                            P_("Propagate Natural Width"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScrolledWindow:propagate-natural-height:
   *
   * Whether the natural height of the child should be calculated and propagated
   * through the scrolled window’s requested natural height.
   *
   * This is useful in cases where an attempt should be made to allocate exactly
   * enough space for the natural size of the child.
   */
  properties[PROP_PROPAGATE_NATURAL_HEIGHT] =
      g_param_spec_boolean ("propagate-natural-height",
                            P_("Propagate Natural Height"),
                            P_("Propagate Natural Height"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  /**
   * GtkScrolledWindow::scroll-child:
   * @scrolled_window: a #GtkScrolledWindow
   * @scroll: a #GtkScrollType describing how much to scroll
   * @horizontal: whether the keybinding scrolls the child
   *   horizontally or not
   *
   * The ::scroll-child signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when a keybinding that scrolls is pressed.
   * The horizontal or vertical adjustment is updated which triggers a
   * signal that the scrolled window’s child may listen to and scroll itself.
   */
  signals[SCROLL_CHILD] =
    g_signal_new (I_("scroll-child"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkScrolledWindowClass, scroll_child),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_BOOLEAN,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_SCROLL_TYPE,
		  G_TYPE_BOOLEAN);

  /**
   * GtkScrolledWindow::move-focus-out:
   * @scrolled_window: a #GtkScrolledWindow
   * @direction_type: either %GTK_DIR_TAB_FORWARD or
   *   %GTK_DIR_TAB_BACKWARD
   *
   * The ::move-focus-out signal is a
   * [keybinding signal][GtkBindingSignal] which gets
   * emitted when focus is moved away from the scrolled window by a
   * keybinding. The #GtkWidget::move-focus signal is emitted with
   * @direction_type on this scrolled window’s toplevel parent in the
   * container hierarchy. The default bindings for this signal are
   * `Ctrl + Tab` to move forward and `Ctrl + Shift + Tab` to move backward.
   */
  signals[MOVE_FOCUS_OUT] =
    g_signal_new (I_("move-focus-out"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkScrolledWindowClass, move_focus_out),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkScrolledWindow::edge-overshot:
   * @scrolled_window: a #GtkScrolledWindow
   * @pos: edge side that was hit
   *
   * The ::edge-overshot signal is emitted whenever user initiated scrolling
   * makes the scrolled window firmly surpass (i.e. with some edge resistance)
   * the lower or upper limits defined by the adjustment in that orientation.
   *
   * A similar behavior without edge resistance is provided by the
   * #GtkScrolledWindow::edge-reached signal.
   *
   * Note: The @pos argument is LTR/RTL aware, so callers should be aware too
   * if intending to provide behavior on horizontal edges.
   */
  signals[EDGE_OVERSHOT] =
    g_signal_new (I_("edge-overshot"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 1, GTK_TYPE_POSITION_TYPE);

  /**
   * GtkScrolledWindow::edge-reached:
   * @scrolled_window: a #GtkScrolledWindow
   * @pos: edge side that was reached
   *
   * The ::edge-reached signal is emitted whenever user-initiated scrolling
   * makes the scrolled window exactly reach the lower or upper limits
   * defined by the adjustment in that orientation.
   *
   * A similar behavior with edge resistance is provided by the
   * #GtkScrolledWindow::edge-overshot signal.
   *
   * Note: The @pos argument is LTR/RTL aware, so callers should be aware too
   * if intending to provide behavior on horizontal edges.
   */
  signals[EDGE_REACHED] =
    g_signal_new (I_("edge-reached"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 1, GTK_TYPE_POSITION_TYPE);

  binding_set = gtk_binding_set_by_class (class);

  add_scroll_binding (binding_set, GDK_KEY_Left,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, TRUE);
  add_scroll_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  TRUE);
  add_scroll_binding (binding_set, GDK_KEY_Up,    GDK_CONTROL_MASK, GTK_SCROLL_STEP_BACKWARD, FALSE);
  add_scroll_binding (binding_set, GDK_KEY_Down,  GDK_CONTROL_MASK, GTK_SCROLL_STEP_FORWARD,  FALSE);

  add_scroll_binding (binding_set, GDK_KEY_Page_Up,   GDK_CONTROL_MASK, GTK_SCROLL_PAGE_BACKWARD, TRUE);
  add_scroll_binding (binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_PAGE_FORWARD,  TRUE);
  add_scroll_binding (binding_set, GDK_KEY_Page_Up,   0,                GTK_SCROLL_PAGE_BACKWARD, FALSE);
  add_scroll_binding (binding_set, GDK_KEY_Page_Down, 0,                GTK_SCROLL_PAGE_FORWARD,  FALSE);

  add_scroll_binding (binding_set, GDK_KEY_Home, GDK_CONTROL_MASK, GTK_SCROLL_START, TRUE);
  add_scroll_binding (binding_set, GDK_KEY_End,  GDK_CONTROL_MASK, GTK_SCROLL_END,   TRUE);
  add_scroll_binding (binding_set, GDK_KEY_Home, 0,                GTK_SCROLL_START, FALSE);
  add_scroll_binding (binding_set, GDK_KEY_End,  0,                GTK_SCROLL_END,   FALSE);

  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SCROLLED_WINDOW_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("scrolledwindow"));
}

static gboolean
may_hscroll (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  return priv->hscrollbar_visible || priv->hscrollbar_policy == GTK_POLICY_EXTERNAL;
}

static gboolean
may_vscroll (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  return priv->vscrollbar_visible || priv->vscrollbar_policy == GTK_POLICY_EXTERNAL;
}

static inline gboolean
policy_may_be_visible (GtkPolicyType policy)
{
  return policy == GTK_POLICY_ALWAYS || policy == GTK_POLICY_AUTOMATIC;
}

static void
scrolled_window_drag_begin_cb (GtkScrolledWindow *scrolled_window,
                               gdouble            start_x,
                               gdouble            start_y,
                               GtkGesture        *gesture)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkEventSequenceState state;
  GdkEventSequence *sequence;
  GtkWidget *event_widget;
  const GdkEvent *event;

  priv->in_drag = FALSE;
  priv->drag_start_x = priv->unclamped_hadj_value;
  priv->drag_start_y = priv->unclamped_vadj_value;
  gtk_scrolled_window_cancel_deceleration (scrolled_window);
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (gesture, sequence);
  event_widget = gtk_get_event_target ((GdkEvent *) event);

  if (event_widget == priv->vscrollbar || event_widget == priv->hscrollbar ||
      (!may_hscroll (scrolled_window) && !may_vscroll (scrolled_window)))
    state = GTK_EVENT_SEQUENCE_DENIED;
  else if (priv->capture_button_press)
    state = GTK_EVENT_SEQUENCE_CLAIMED;
  else
    return;

  gtk_gesture_set_sequence_state (gesture, sequence, state);
}

static void
gtk_scrolled_window_invalidate_overshoot (GtkScrolledWindow *scrolled_window)
{
  GtkAllocation child_allocation;
  gint overshoot_x, overshoot_y;

  if (!_gtk_scrolled_window_get_overshoot (scrolled_window, &overshoot_x, &overshoot_y))
    return;

  gtk_scrolled_window_relative_allocation (scrolled_window,
                                           &child_allocation);
  if (overshoot_x != 0)
    gtk_widget_queue_draw (GTK_WIDGET (scrolled_window));

  if (overshoot_y != 0)
    gtk_widget_queue_draw (GTK_WIDGET (scrolled_window));
}

static void
scrolled_window_drag_update_cb (GtkScrolledWindow *scrolled_window,
                                gdouble            offset_x,
                                gdouble            offset_y,
                                GtkGesture        *gesture)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  gdouble dx, dy;

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  if (!priv->capture_button_press)
    {
      GdkEventSequence *sequence;

      sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
      gtk_gesture_set_sequence_state (gesture, sequence,
                                      GTK_EVENT_SEQUENCE_CLAIMED);
    }

  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  if (hadjustment && may_hscroll (scrolled_window))
    {
      dx = priv->drag_start_x - offset_x;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window,
                                                 hadjustment, dx);
    }

  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  if (vadjustment && may_vscroll (scrolled_window))
    {
      dy = priv->drag_start_y - offset_y;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window,
                                                 vadjustment, dy);
    }

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);
}

static void
scrolled_window_drag_end_cb (GtkScrolledWindow *scrolled_window,
                             GdkEventSequence  *sequence,
                             GtkGesture        *gesture)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (!priv->in_drag || !gtk_gesture_handles_sequence (gesture, sequence))
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_scrolled_window_decelerate (GtkScrolledWindow *scrolled_window,
                                gdouble            x_velocity,
                                gdouble            y_velocity)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean overshoot;

  overshoot = _gtk_scrolled_window_get_overshoot (scrolled_window, NULL, NULL);
  priv->x_velocity = x_velocity;
  priv->y_velocity = y_velocity;

  /* Zero out vector components for which we don't scroll */
  if (!may_hscroll (scrolled_window))
    priv->x_velocity = 0;
  if (!may_vscroll (scrolled_window))
    priv->y_velocity = 0;

  if (priv->x_velocity != 0 || priv->y_velocity != 0 || overshoot)
    {
      gtk_scrolled_window_start_deceleration (scrolled_window);
      priv->x_velocity = priv->y_velocity = 0;
    }
}

static void
scrolled_window_swipe_cb (GtkScrolledWindow *scrolled_window,
                          gdouble            x_velocity,
                          gdouble            y_velocity)
{
  gtk_scrolled_window_decelerate (scrolled_window, -x_velocity, -y_velocity);
}

static void
scrolled_window_long_press_cb (GtkScrolledWindow *scrolled_window,
                               gdouble            x,
                               gdouble            y,
                               GtkGesture        *gesture)
{
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_set_sequence_state (gesture, sequence,
                                  GTK_EVENT_SEQUENCE_DENIED);
}

static void
scrolled_window_long_press_cancelled_cb (GtkScrolledWindow *scrolled_window,
                                         GtkGesture        *gesture)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GdkEventType event_type;

  sequence = gtk_gesture_get_last_updated_sequence (gesture);
  event = gtk_gesture_get_last_event (gesture, sequence);
  event_type = gdk_event_get_event_type (event);

  if (event_type == GDK_TOUCH_BEGIN ||
      event_type == GDK_BUTTON_PRESS)
    gtk_gesture_set_sequence_state (gesture, sequence,
                                    GTK_EVENT_SEQUENCE_DENIED);
  else if (event_type != GDK_TOUCH_END &&
           event_type != GDK_BUTTON_RELEASE)
    priv->in_drag = TRUE;
}

static void
gtk_scrolled_window_check_attach_pan_gesture (GtkScrolledWindow *sw)
{
  GtkPropagationPhase phase = GTK_PHASE_NONE;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);

  if (priv->kinetic_scrolling &&
      ((may_hscroll (sw) && !may_vscroll (sw)) ||
       (!may_hscroll (sw) && may_vscroll (sw))))
    {
      GtkOrientation orientation;

      if (may_hscroll (sw))
        orientation = GTK_ORIENTATION_HORIZONTAL;
      else
        orientation = GTK_ORIENTATION_VERTICAL;

      gtk_gesture_pan_set_orientation (GTK_GESTURE_PAN (priv->pan_gesture),
                                       orientation);
      phase = GTK_PHASE_CAPTURE;
    }

  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan_gesture), phase);
}

static void
indicator_set_over (Indicator *indicator,
                    gboolean   over)
{
  GtkStyleContext *context;

  if (indicator->over_timeout_id)
    {
      g_source_remove (indicator->over_timeout_id);
      indicator->over_timeout_id = 0;
    }

  if (indicator->over == over)
    return;

  context = gtk_widget_get_style_context (indicator->scrollbar);
  indicator->over = over;

  if (indicator->over)
    gtk_style_context_add_class (context, "hovering");
  else
    gtk_style_context_remove_class (context, "hovering");

  gtk_widget_queue_resize (indicator->scrollbar);
}

static gboolean
coords_close_to_indicator (GtkScrolledWindow *sw,
                           Indicator         *indicator,
                           gdouble            x,
                           gdouble            y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  graphene_rect_t indicator_bounds;
  gint distance;

  if (!gtk_widget_compute_bounds (indicator->scrollbar, GTK_WIDGET (sw), &indicator_bounds))
    return FALSE;

  if (indicator->over)
    distance = INDICATOR_FAR_DISTANCE;
  else
    distance = INDICATOR_CLOSE_DISTANCE;

  graphene_rect_inset (&indicator_bounds, - distance, - distance);

  if (indicator == &priv->hindicator)
    {
      if (y >= indicator_bounds.origin.y &&
          y < indicator_bounds.origin.y + indicator_bounds.size.height)
         return TRUE;
    }
  else if (indicator == &priv->vindicator)
    {
      if (x >= indicator_bounds.origin.x &&
          x < indicator_bounds.origin.x + indicator_bounds.size.width)
        return TRUE;
    }

  return FALSE;
}

static gboolean
enable_over_timeout_cb (gpointer user_data)
{
  Indicator *indicator = user_data;

  indicator_set_over (indicator, TRUE);
  return G_SOURCE_REMOVE;
}

static gboolean
check_update_scrollbar_proximity (GtkScrolledWindow *sw,
                                  Indicator         *indicator,
                                  GtkWidget         *target,
                                  gdouble            x,
                                  gdouble            y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  gboolean indicator_close, on_scrollbar, on_other_scrollbar;

  indicator_close = coords_close_to_indicator (sw, indicator, x, y);
  on_scrollbar = (target == indicator->scrollbar);
  on_other_scrollbar = (!on_scrollbar &&
                        (target == priv->hindicator.scrollbar ||
                         target == priv->vindicator.scrollbar));

  if (indicator->over_timeout_id)
    {
      g_source_remove (indicator->over_timeout_id);
      indicator->over_timeout_id = 0;
    }

  if (on_scrollbar)
    indicator_set_over (indicator, TRUE);
  else if (indicator_close && !on_other_scrollbar)
    {
      indicator->over_timeout_id = g_timeout_add (30, enable_over_timeout_cb, indicator);
      g_source_set_name_by_id (indicator->over_timeout_id, "[gtk] enable_over_timeout_cb");
    }
  else
    indicator_set_over (indicator, FALSE);

  return indicator_close;
}

static gdouble
get_scroll_unit (GtkScrolledWindow *sw,
                 GtkOrientation     orientation)
{
  gdouble scroll_unit;

#ifndef GDK_WINDOWING_QUARTZ
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  GtkScrollbar *scrollbar;
  GtkAdjustment *adj;
  gdouble page_size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    scrollbar = GTK_SCROLLBAR (priv->hscrollbar);
  else
    scrollbar = GTK_SCROLLBAR (priv->vscrollbar);

  if (!scrollbar)
    return 0;

  adj = gtk_scrollbar_get_adjustment (scrollbar);
  page_size = gtk_adjustment_get_page_size (adj);
  scroll_unit = pow (page_size, 2.0 / 3.0);
#else
  scroll_unit = 1;
#endif

  return scroll_unit;
}

static gboolean
captured_event_cb (GtkWidget *widget,
                   GdkEvent  *event)
{
  GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW (widget);

  if (gdk_event_get_event_type (event) == GDK_SCROLL)
    gtk_scrolled_window_cancel_deceleration (sw);

  return GDK_EVENT_PROPAGATE;
}

static void
captured_motion (GtkScrolledWindow *sw,
                 gdouble            x,
                 gdouble            y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  GdkDevice *source_device;
  GdkInputSource input_source;
  GdkModifierType state;
  GdkEvent *event;

  if (!priv->use_indicators)
    return;

  event = gtk_get_current_event ();
  source_device = gdk_event_get_source_device (event);
  input_source = gdk_device_get_source (source_device);

  if (priv->hscrollbar_visible)
    indicator_start_fade (&priv->hindicator, 1.0);
  if (priv->vscrollbar_visible)
    indicator_start_fade (&priv->vindicator, 1.0);

  gdk_event_get_state (event, &state);

  if (!gtk_get_event_target_with_type (event, GTK_TYPE_SCROLLBAR) &&
      (state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) != 0)
    {
      indicator_set_over (&priv->hindicator, FALSE);
      indicator_set_over (&priv->vindicator, FALSE);
    }
  else if (input_source == GDK_SOURCE_PEN ||
           input_source == GDK_SOURCE_ERASER ||
           input_source == GDK_SOURCE_TRACKPOINT)
    {
      indicator_set_over (&priv->hindicator, TRUE);
      indicator_set_over (&priv->vindicator, TRUE);
    }
  else
    {
      GtkWidget *target;

      target = gtk_get_event_target_with_type (event, GTK_TYPE_SCROLLBAR);

      if (!check_update_scrollbar_proximity (sw, &priv->vindicator, target, x, y))
        check_update_scrollbar_proximity (sw, &priv->hindicator, target, x, y);
      else
        indicator_set_over (&priv->hindicator, FALSE);
    }

  g_object_unref (event);
}

static gboolean
start_scroll_deceleration_cb (gpointer user_data)
{
  GtkScrolledWindow *scrolled_window = user_data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  priv->scroll_events_overshoot_id = 0;

  if (!priv->deceleration_id)
    {
      uninstall_scroll_cursor (scrolled_window);
      gtk_scrolled_window_start_deceleration (scrolled_window);
    }

  return FALSE;
}

static void
scroll_controller_scroll_begin (GtkEventControllerScroll *scroll,
                                GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  install_scroll_cursor (scrolled_window);
  priv->smooth_scroll = TRUE;
}

static gboolean
scroll_controller_scroll (GtkEventControllerScroll *scroll,
                          gdouble                   delta_x,
                          gdouble                   delta_y,
                          GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean shifted;
  GdkModifierType state;

  if (!gtk_get_current_event_state (&state))
    return GDK_EVENT_PROPAGATE;

  shifted = (state & GDK_SHIFT_MASK) != 0;

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  if (shifted)
    {
      gdouble delta;

      delta = delta_x;
      delta_x = delta_y;
      delta_y = delta;
    }

  if (delta_x != 0.0 &&
      may_hscroll (scrolled_window))
    {
      GtkAdjustment *adj;
      gdouble new_value;
      gdouble scroll_unit;

      adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      scroll_unit = get_scroll_unit (scrolled_window, GTK_ORIENTATION_HORIZONTAL);

      new_value = priv->unclamped_hadj_value + delta_x * scroll_unit;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window, adj,
                                                 new_value);
    }

  if (delta_y != 0.0 &&
      may_vscroll (scrolled_window))
    {
      GtkAdjustment *adj;
      gdouble new_value;
      gdouble scroll_unit;

      adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      scroll_unit = get_scroll_unit (scrolled_window, GTK_ORIENTATION_VERTICAL);

      new_value = priv->unclamped_vadj_value + delta_y * scroll_unit;
      _gtk_scrolled_window_set_adjustment_value (scrolled_window, adj,
                                                 new_value);
    }

  if (priv->scroll_events_overshoot_id)
    {
      g_source_remove (priv->scroll_events_overshoot_id);
      priv->scroll_events_overshoot_id = 0;
    }

  if (!priv->smooth_scroll &&
      _gtk_scrolled_window_get_overshoot (scrolled_window, NULL, NULL))
    {
      priv->scroll_events_overshoot_id =
        g_timeout_add (50, start_scroll_deceleration_cb, scrolled_window);
      g_source_set_name_by_id (priv->scroll_events_overshoot_id,
                               "[gtk] start_scroll_deceleration_cb");
    }

  return GDK_EVENT_STOP;
}

static void
scroll_controller_scroll_end (GtkEventControllerScroll *scroll,
                              GtkScrolledWindow        *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  priv->smooth_scroll = FALSE;
  uninstall_scroll_cursor (scrolled_window);
}

static void
scroll_controller_decelerate (GtkEventControllerScroll *scroll,
                              gdouble                   initial_vel_x,
                              gdouble                   initial_vel_y,
                              GtkScrolledWindow        *scrolled_window)
{
  gdouble unit_x, unit_y;
  gboolean shifted;
  GdkModifierType state;

  if (!gtk_get_current_event_state (&state))
    return;

  shifted = (state & GDK_SHIFT_MASK) != 0;

  unit_x = get_scroll_unit (scrolled_window, GTK_ORIENTATION_HORIZONTAL);
  unit_y = get_scroll_unit (scrolled_window, GTK_ORIENTATION_VERTICAL);

  if (shifted)
    {
      gtk_scrolled_window_decelerate (scrolled_window,
                                      initial_vel_y * unit_x,
                                      initial_vel_x * unit_y);
    }
  else
    {
      gtk_scrolled_window_decelerate (scrolled_window,
                                      initial_vel_x * unit_x,
                                      initial_vel_y * unit_y);
    }
}

static void
gtk_scrolled_window_update_scrollbar_visibility_flags (GtkScrolledWindow *scrolled_window,
                                                       GtkWidget         *scrollbar)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *adjustment;

  if (scrollbar == NULL)
    return;

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));

  if (scrollbar == priv->hscrollbar)
    {
      if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          priv->hscrollbar_visible = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) >
                                      gtk_adjustment_get_page_size (adjustment));
        }
    }
  else if (scrollbar == priv->vscrollbar)
    {
      if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
        {
          priv->vscrollbar_visible = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment) >
                                      gtk_adjustment_get_page_size (adjustment));
        }
    }
}

static void
gtk_scrolled_window_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkWidget *child;
  gint sb_width;
  gint sb_height;

  bin = GTK_BIN (scrolled_window);

  /* Get possible scrollbar dimensions */
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  if (priv->hscrollbar_policy == GTK_POLICY_ALWAYS)
    priv->hscrollbar_visible = TRUE;
  else if (priv->hscrollbar_policy == GTK_POLICY_NEVER ||
           priv->hscrollbar_policy == GTK_POLICY_EXTERNAL)
    priv->hscrollbar_visible = FALSE;

  if (priv->vscrollbar_policy == GTK_POLICY_ALWAYS)
    priv->vscrollbar_visible = TRUE;
  else if (priv->vscrollbar_policy == GTK_POLICY_NEVER ||
           priv->vscrollbar_policy == GTK_POLICY_EXTERNAL)
    priv->vscrollbar_visible = FALSE;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      gint child_scroll_width;
      gint child_scroll_height;
      gboolean previous_hvis;
      gboolean previous_vvis;
      guint count = 0;
      GtkScrollable *scrollable_child = GTK_SCROLLABLE (child);
      GtkScrollablePolicy hscroll_policy = gtk_scrollable_get_hscroll_policy (scrollable_child);
      GtkScrollablePolicy vscroll_policy = gtk_scrollable_get_vscroll_policy (scrollable_child);

      /* Determine scrollbar visibility first via hfw apis */
      if (gtk_widget_get_request_mode (child) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
	{
          if (hscroll_policy == GTK_SCROLL_MINIMUM)
            gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1,
                                &child_scroll_width, NULL, NULL, NULL);
          else
            gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1,
                                NULL, &child_scroll_width, NULL, NULL);

	  if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	    {
	      /* First try without a vertical scrollbar if the content will fit the height
	       * given the extra width of the scrollbar */
	      if (vscroll_policy == GTK_SCROLL_MINIMUM)
                gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL,
                                    MAX (width, child_scroll_width),
                                    &child_scroll_height, NULL, NULL, NULL);
              else
                gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL,
                                    MAX (width, child_scroll_width),
                                    NULL, &child_scroll_height, NULL, NULL);

	      if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
		{
		  /* Does the content height fit the allocation height ? */
                  priv->vscrollbar_visible = child_scroll_height > height;

		  /* Does the content width fit the allocation with minus a possible scrollbar ? */
                  priv->hscrollbar_visible = child_scroll_width > width -
		    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);

		  /* Now that we've guessed the hscrollbar, does the content height fit
		   * the possible new allocation height ?
		   */
                  priv->vscrollbar_visible = child_scroll_height > height -
		    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);

		  /* Now that we've guessed the vscrollbar, does the content width fit
		   * the possible new allocation width ?
		   */
                  priv->hscrollbar_visible = child_scroll_width > width -
		    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);
		}
	      else /* priv->hscrollbar_policy != GTK_POLICY_AUTOMATIC */
		{
		  priv->hscrollbar_visible = policy_may_be_visible (priv->hscrollbar_policy);
		  priv->vscrollbar_visible = child_scroll_height > height -
		    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);
		}
	    }
	  else /* priv->vscrollbar_policy != GTK_POLICY_AUTOMATIC */
	    {
	      priv->vscrollbar_visible = policy_may_be_visible (priv->vscrollbar_policy);

	      if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
                priv->hscrollbar_visible = child_scroll_width > width -
		  (priv->vscrollbar_visible && !priv->use_indicators ? 0 : sb_width);
	      else
		priv->hscrollbar_visible = policy_may_be_visible (priv->hscrollbar_policy);
	    }
	}
      else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT */
	{
          if (vscroll_policy == GTK_SCROLL_MINIMUM)
            gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1,
                                &child_scroll_height, NULL, NULL, NULL);
          else
            gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1,
                                NULL, &child_scroll_height, NULL, NULL);

	  if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	    {
	      /* First try without a horizontal scrollbar if the content will fit the width
	       * given the extra height of the scrollbar */
	      if (hscroll_policy == GTK_SCROLL_MINIMUM)
                gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL,
                                    MAX (height, child_scroll_height),
                                    &child_scroll_width, NULL, NULL, NULL);
              else
                gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL,
                                    MAX (height, child_scroll_height),
                                    NULL, &child_scroll_width, NULL, NULL);

	      if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
		{
		  /* Does the content width fit the allocation width ? */
                  priv->hscrollbar_visible = child_scroll_width > width;

		  /* Does the content height fit the allocation with minus a possible scrollbar ? */
                  priv->vscrollbar_visible = child_scroll_height > height -
		    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);

		  /* Now that we've guessed the vscrollbar, does the content width fit
		   * the possible new allocation width ?
		   */
                  priv->hscrollbar_visible = child_scroll_width > width -
		    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);

		  /* Now that we've guessed the hscrollbar, does the content height fit
		   * the possible new allocation height ?
		   */
                  priv->vscrollbar_visible = child_scroll_height > height -
		    (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);
		}
	      else /* priv->vscrollbar_policy != GTK_POLICY_AUTOMATIC */
		{
		  priv->vscrollbar_visible = policy_may_be_visible (priv->vscrollbar_policy);
                  priv->hscrollbar_visible = child_scroll_width > width -
		    (priv->vscrollbar_visible && !priv->use_indicators ? sb_width : 0);
		}
	    }
	  else /* priv->hscrollbar_policy != GTK_POLICY_AUTOMATIC */
	    {
	      priv->hscrollbar_visible = policy_may_be_visible (priv->hscrollbar_policy);

	      if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
                priv->vscrollbar_visible = child_scroll_height > height -
		  (priv->hscrollbar_visible && !priv->use_indicators ? sb_height : 0);
	      else
		priv->vscrollbar_visible = policy_may_be_visible (priv->vscrollbar_policy);
	    }
	}

      /* Now after guessing scrollbar visibility; fall back on the allocation loop which
       * observes the adjustments to detect scrollbar visibility and also avoids
       * infinite recursion
       */
      do
	{
	  previous_hvis = priv->hscrollbar_visible;
	  previous_vvis = priv->vscrollbar_visible;

	  gtk_scrolled_window_allocate_child (scrolled_window, width, height);

          /* Explicitly force scrollbar visibility checks.
           *
           * Since we make a guess above, the child might not decide to update the adjustments
           * if they logically did not change since the last configuration
           *
           * These will update priv->hscrollbar_visible and priv->vscrollbar_visible.
           */
          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window,
                                                                 priv->hscrollbar);

          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window,
                                                                 priv->vscrollbar);

	  /* If, after the first iteration, the hscrollbar and the
	   * vscrollbar flip visiblity... or if one of the scrollbars flip
	   * on each itteration indefinitly/infinitely, then we just need both
	   * at this size.
	   */
	  if ((count &&
	       previous_hvis != priv->hscrollbar_visible &&
	       previous_vvis != priv->vscrollbar_visible) || count > 3)
	    {
	      priv->hscrollbar_visible = TRUE;
	      priv->vscrollbar_visible = TRUE;

	      gtk_scrolled_window_allocate_child (scrolled_window, width, height);

	      break;
	    }

	  count++;
	}
      while (previous_hvis != priv->hscrollbar_visible ||
	     previous_vvis != priv->vscrollbar_visible);
    }
  else
    {
      priv->hscrollbar_visible = priv->hscrollbar_policy == GTK_POLICY_ALWAYS;
      priv->vscrollbar_visible = priv->vscrollbar_policy == GTK_POLICY_ALWAYS;
    }

  gtk_widget_set_child_visible (priv->hscrollbar, priv->hscrollbar_visible);
  if (priv->hscrollbar_visible)
    {
      gtk_scrolled_window_allocate_scrollbar (scrolled_window,
                                              priv->hscrollbar,
                                              &child_allocation);
      gtk_widget_size_allocate (priv->hscrollbar, &child_allocation, -1);
    }

  gtk_widget_set_child_visible (priv->vscrollbar, priv->vscrollbar_visible);
  if (priv->vscrollbar_visible)
    {
      gtk_scrolled_window_allocate_scrollbar (scrolled_window,
                                              priv->vscrollbar,
                                              &child_allocation);
      gtk_widget_size_allocate (priv->vscrollbar, &child_allocation, -1);
    }

  gtk_scrolled_window_check_attach_pan_gesture (scrolled_window);
}

static void
gtk_scrolled_window_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *minimum_size,
                             int            *natural_size,
                             int            *minimum_baseline,
                             int            *natural_baseline)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkBin *bin = GTK_BIN (scrolled_window);
  int minimum_req = 0, natural_req = 0;
  GtkWidget *child;
  GtkBorder sborder = { 0 };

  child = gtk_bin_get_child (bin);

  if (child)
    gtk_scrollable_get_border (GTK_SCROLLABLE (child), &sborder);

  /*
   * First collect the child requisition
   */
  if (child && gtk_widget_get_visible (child))
    {
      int min_child_size, nat_child_size;

      gtk_widget_measure (child, orientation, -1,
                          &min_child_size, &nat_child_size,
                          NULL, NULL);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (priv->propagate_natural_width)
            natural_req += nat_child_size;

	  if (priv->hscrollbar_policy == GTK_POLICY_NEVER)
	    {
              minimum_req += min_child_size;
	    }
	  else
	    {
	      gint min = priv->min_content_width >= 0 ? priv->min_content_width : 0;
	      gint max = priv->max_content_width >= 0 ? priv->max_content_width : G_MAXINT;

              minimum_req = CLAMP (minimum_req, min, max);
              natural_req = CLAMP (natural_req, min, max);
	    }
	}
      else /* GTK_ORIENTATION_VERTICAL */
	{
	  if (priv->propagate_natural_height)
            natural_req += nat_child_size;

	  if (priv->vscrollbar_policy == GTK_POLICY_NEVER)
	    {
              minimum_req += min_child_size;
	    }
	  else
	    {
	      gint min = priv->min_content_height >= 0 ? priv->min_content_height : 0;
	      gint max = priv->max_content_height >= 0 ? priv->max_content_height : G_MAXINT;

              minimum_req = CLAMP (minimum_req, min, max);
              natural_req = CLAMP (natural_req, min, max);
	    }
	}
    }

  /* Ensure we make requests with natural size >= minimum size */
  natural_req = MAX (minimum_req, natural_req);

  /*
   * Now add to the requisition any additional space for surrounding scrollbars
   * and the special scrollable border.
   */
  if (policy_may_be_visible (priv->hscrollbar_policy))
    {
      GtkRequisition hscrollbar_requisition;
      gtk_widget_get_preferred_size (priv->hscrollbar, &hscrollbar_requisition, NULL);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          minimum_req = MAX (minimum_req, hscrollbar_requisition.width + sborder.left + sborder.right);
          natural_req = MAX (natural_req, hscrollbar_requisition.width + sborder.left + sborder.right);
        }
      else if (!priv->use_indicators && priv->hscrollbar_policy == GTK_POLICY_ALWAYS)
        {
          minimum_req += hscrollbar_requisition.height;
          natural_req += hscrollbar_requisition.height;
        }
    }

  if (policy_may_be_visible (priv->vscrollbar_policy))
    {
      GtkRequisition vscrollbar_requisition;
      gtk_widget_get_preferred_size (priv->vscrollbar, &vscrollbar_requisition, NULL);

      if (orientation == GTK_ORIENTATION_VERTICAL)
        {
          minimum_req = MAX (minimum_req, vscrollbar_requisition.height + sborder.top + sborder.bottom);
          natural_req = MAX (natural_req, vscrollbar_requisition.height + sborder.top + sborder.bottom);
        }
      else if (!priv->use_indicators && priv->vscrollbar_policy == GTK_POLICY_ALWAYS)
        {
          minimum_req += vscrollbar_requisition.width;
          natural_req += vscrollbar_requisition.width;
        }
    }

  *minimum_size = minimum_req;
  *natural_size = natural_req;
}

static void
gtk_scrolled_window_snapshot_scrollbars_junction (GtkScrolledWindow *scrolled_window,
                                                  GtkSnapshot       *snapshot)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  GtkAllocation hscr_allocation, vscr_allocation;
  GtkStyleContext *context;
  GdkRectangle junction_rect;

  gtk_widget_get_allocation (GTK_WIDGET (priv->hscrollbar), &hscr_allocation);
  gtk_widget_get_allocation (GTK_WIDGET (priv->vscrollbar), &vscr_allocation);

  junction_rect.x = vscr_allocation.x;
  junction_rect.y = hscr_allocation.y;
  junction_rect.width = vscr_allocation.width;
  junction_rect.height = hscr_allocation.height;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save_named (context, "junction");

  gtk_snapshot_render_background (snapshot, context,
                                  junction_rect.x, junction_rect.y,
                                  junction_rect.width, junction_rect.height);
  gtk_snapshot_render_frame (snapshot, context,
                             junction_rect.x, junction_rect.y,
                             junction_rect.width, junction_rect.height);

  gtk_style_context_restore (context);
}

static void
gtk_scrolled_window_snapshot_overshoot (GtkScrolledWindow *scrolled_window,
                                        GtkSnapshot       *snapshot)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  gint overshoot_x, overshoot_y;
  GtkStyleContext *context;
  GdkRectangle rect;

  if (!_gtk_scrolled_window_get_overshoot (scrolled_window, &overshoot_x, &overshoot_y))
    return;

  context = gtk_widget_get_style_context (widget);
  gtk_scrolled_window_inner_allocation (scrolled_window, &rect);

  overshoot_x = CLAMP (overshoot_x, - MAX_OVERSHOOT_DISTANCE, MAX_OVERSHOOT_DISTANCE);
  overshoot_y = CLAMP (overshoot_y, - MAX_OVERSHOOT_DISTANCE, MAX_OVERSHOOT_DISTANCE);

  if (overshoot_x > 0)
    {
      gtk_style_context_save_to_node (context, priv->overshoot_node[GTK_POS_RIGHT]);
      gtk_snapshot_render_background (snapshot, context, rect.x + rect.width - overshoot_x, rect.y, overshoot_x, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x + rect.width - overshoot_x, rect.y, overshoot_x, rect.height);
      gtk_style_context_restore (context);
    }
  else if (overshoot_x < 0)
    {
      gtk_style_context_save_to_node (context, priv->overshoot_node[GTK_POS_LEFT]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y, -overshoot_x, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y, -overshoot_x, rect.height);
      gtk_style_context_restore (context);
    }

  if (overshoot_y > 0)
    {
      gtk_style_context_save_to_node (context, priv->overshoot_node[GTK_POS_BOTTOM]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y + rect.height - overshoot_y, rect.width, overshoot_y);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y + rect.height - overshoot_y, rect.width, overshoot_y);
      gtk_style_context_restore (context);
    }
  else if (overshoot_y < 0)
    {
      gtk_style_context_save_to_node (context, priv->overshoot_node[GTK_POS_TOP]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y, rect.width, -overshoot_y);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y, rect.width, -overshoot_y);
      gtk_style_context_restore (context);
    }
}

static void
gtk_scrolled_window_snapshot_undershoot (GtkScrolledWindow *scrolled_window,
                                         GtkSnapshot       *snapshot)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  GtkStyleContext *context;
  GdkRectangle rect;
  GtkAdjustment *adj;

  context = gtk_widget_get_style_context (widget);
  gtk_scrolled_window_inner_allocation (scrolled_window, &rect);

  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  if (gtk_adjustment_get_value (adj) < gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj))
    {
      gtk_style_context_save_to_node (context, priv->undershoot_node[GTK_POS_RIGHT]);
      gtk_snapshot_render_background (snapshot, context, rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x + rect.width - UNDERSHOOT_SIZE, rect.y, UNDERSHOOT_SIZE, rect.height);

      gtk_style_context_restore (context);
    }
  if (gtk_adjustment_get_value (adj) > gtk_adjustment_get_lower (adj))
    {
      gtk_style_context_save_to_node (context, priv->undershoot_node[GTK_POS_LEFT]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y, UNDERSHOOT_SIZE, rect.height);
      gtk_style_context_restore (context);
    }

  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  if (gtk_adjustment_get_value (adj) < gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj))
    {
      gtk_style_context_save_to_node (context, priv->undershoot_node[GTK_POS_BOTTOM]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y + rect.height - UNDERSHOOT_SIZE, rect.width, UNDERSHOOT_SIZE);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y + rect.height - UNDERSHOOT_SIZE, rect.width, UNDERSHOOT_SIZE);
      gtk_style_context_restore (context);
    }
  if (gtk_adjustment_get_value (adj) > gtk_adjustment_get_lower (adj))
    {
      gtk_style_context_save_to_node (context, priv->undershoot_node[GTK_POS_TOP]);
      gtk_snapshot_render_background (snapshot, context, rect.x, rect.y, rect.width, UNDERSHOOT_SIZE);
      gtk_snapshot_render_frame (snapshot, context, rect.x, rect.y, rect.width, UNDERSHOOT_SIZE);
      gtk_style_context_restore (context);
    }
}

static void
gtk_scrolled_window_init (GtkScrolledWindow *scrolled_window)
{
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkEventController *controller;
  GtkCssNode *widget_node;
  GQuark classes[4] = {
    g_quark_from_static_string (GTK_STYLE_CLASS_LEFT),
    g_quark_from_static_string (GTK_STYLE_CLASS_RIGHT),
    g_quark_from_static_string (GTK_STYLE_CLASS_TOP),
    g_quark_from_static_string (GTK_STYLE_CLASS_BOTTOM),
  };
  gint i;

  gtk_widget_set_can_focus (widget, TRUE);

  /* Instantiated by gtk_scrolled_window_set_[hv]adjustment
   * which are both construct properties
   */
  priv->hscrollbar = NULL;
  priv->vscrollbar = NULL;
  priv->hscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->vscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->hscrollbar_visible = FALSE;
  priv->vscrollbar_visible = FALSE;
  priv->focus_out = FALSE;
  priv->auto_added_viewport = FALSE;
  priv->window_placement = GTK_CORNER_TOP_LEFT;
  priv->min_content_width = -1;
  priv->min_content_height = -1;
  priv->max_content_width = -1;
  priv->max_content_height = -1;

  priv->overlay_scrolling = TRUE;

  priv->drag_gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);
  g_signal_connect_swapped (priv->drag_gesture, "drag-begin",
                            G_CALLBACK (scrolled_window_drag_begin_cb),
                            scrolled_window);
  g_signal_connect_swapped (priv->drag_gesture, "drag-update",
                            G_CALLBACK (scrolled_window_drag_update_cb),
                            scrolled_window);
  g_signal_connect_swapped (priv->drag_gesture, "end",
                            G_CALLBACK (scrolled_window_drag_end_cb),
                            scrolled_window);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->drag_gesture));

  priv->pan_gesture = gtk_gesture_pan_new (GTK_ORIENTATION_VERTICAL);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->pan_gesture), TRUE);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->pan_gesture));
  gtk_gesture_group (priv->pan_gesture, priv->drag_gesture);

  priv->swipe_gesture = gtk_gesture_swipe_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->swipe_gesture), TRUE);
  g_signal_connect_swapped (priv->swipe_gesture, "swipe",
                            G_CALLBACK (scrolled_window_swipe_cb),
                            scrolled_window);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->swipe_gesture));
  gtk_gesture_group (priv->swipe_gesture, priv->drag_gesture);

  priv->long_press_gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->long_press_gesture), TRUE);
  g_signal_connect_swapped (priv->long_press_gesture, "pressed",
                            G_CALLBACK (scrolled_window_long_press_cb),
                            scrolled_window);
  g_signal_connect_swapped (priv->long_press_gesture, "cancelled",
                            G_CALLBACK (scrolled_window_long_press_cancelled_cb),
                            scrolled_window);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->long_press_gesture));
  gtk_gesture_group (priv->long_press_gesture, priv->drag_gesture);

  gtk_scrolled_window_set_kinetic_scrolling (scrolled_window, TRUE);
  gtk_scrolled_window_set_capture_button_press (scrolled_window, TRUE);

  _gtk_widget_set_captured_event_handler (widget, captured_event_cb);

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (controller, "motion",
                            G_CALLBACK (captured_motion), scrolled_window);
  gtk_widget_add_controller (widget, controller);

  widget_node = gtk_widget_get_css_node (widget);
  for (i = 0; i < 4; i++)
    {
      priv->overshoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->overshoot_node[i], I_("overshoot"));
      gtk_css_node_add_class (priv->overshoot_node[i], classes[i]);
      gtk_css_node_set_parent (priv->overshoot_node[i], widget_node);
      gtk_css_node_set_state (priv->overshoot_node[i], gtk_css_node_get_state (widget_node));
      g_object_unref (priv->overshoot_node[i]);

      priv->undershoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->undershoot_node[i], I_("undershoot"));
      gtk_css_node_add_class (priv->undershoot_node[i], classes[i]);
      gtk_css_node_set_parent (priv->undershoot_node[i], widget_node);
      gtk_css_node_set_state (priv->undershoot_node[i], gtk_css_node_get_state (widget_node));
      g_object_unref (priv->undershoot_node[i]);
    }

  gtk_scrolled_window_update_use_indicators (scrolled_window);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES |
                                                GTK_EVENT_CONTROLLER_SCROLL_KINETIC);
  g_signal_connect (controller, "scroll-begin",
                    G_CALLBACK (scroll_controller_scroll_begin), scrolled_window);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (scroll_controller_scroll), scrolled_window);
  g_signal_connect (controller, "scroll-end",
                    G_CALLBACK (scroll_controller_scroll_end), scrolled_window);
  g_signal_connect (controller, "decelerate",
                    G_CALLBACK (scroll_controller_decelerate), scrolled_window);
  gtk_widget_add_controller (widget, controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "leave",
                    G_CALLBACK (motion_controller_leave), scrolled_window);
  gtk_widget_add_controller (widget, controller);
}

/**
 * gtk_scrolled_window_new:
 * @hadjustment: (allow-none): horizontal adjustment
 * @vadjustment: (allow-none): vertical adjustment
 *
 * Creates a new scrolled window.
 *
 * The two arguments are the scrolled window’s adjustments; these will be
 * shared with the scrollbars and the child widget to keep the bars in sync
 * with the child. Usually you want to pass %NULL for the adjustments, which
 * will cause the scrolled window to create them for you.
 *
 * Returns: a new scrolled window
 */
GtkWidget*
gtk_scrolled_window_new (GtkAdjustment *hadjustment,
			 GtkAdjustment *vadjustment)
{
  GtkWidget *scrolled_window;

  if (hadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadjustment), NULL);

  if (vadjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadjustment), NULL);

  scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
				    "hadjustment", hadjustment,
				    "vadjustment", vadjustment,
				    NULL);

  return scrolled_window;
}

/**
 * gtk_scrolled_window_set_hadjustment:
 * @scrolled_window: a #GtkScrolledWindow
 * @hadjustment: horizontal scroll adjustment
 *
 * Sets the #GtkAdjustment for the horizontal scrollbar.
 */
void
gtk_scrolled_window_set_hadjustment (GtkScrolledWindow *scrolled_window,
                                     GtkAdjustment     *hadjustment)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkBin *bin;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (hadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadjustment));
  else
    hadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  bin = GTK_BIN (scrolled_window);

  if (!priv->hscrollbar)
    {
      priv->hscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadjustment);

      gtk_widget_insert_before (priv->hscrollbar, GTK_WIDGET (scrolled_window), priv->vscrollbar);
      update_scrollbar_positions (scrolled_window);
    }
  else
    {
      GtkAdjustment *old_adjustment;

      old_adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      if (old_adjustment == hadjustment)
	return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_changed,
                                            scrolled_window);
      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_value_changed,
                                            scrolled_window);

      gtk_adjustment_enable_animation (old_adjustment, NULL, 0);
      gtk_scrollbar_set_adjustment (GTK_SCROLLBAR (priv->hscrollbar), hadjustment);
    }

  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));

  g_signal_connect (hadjustment,
                    "changed",
		    G_CALLBACK (gtk_scrolled_window_adjustment_changed),
		    scrolled_window);
  g_signal_connect (hadjustment,
                    "value-changed",
		    G_CALLBACK (gtk_scrolled_window_adjustment_value_changed),
		    scrolled_window);

  gtk_scrolled_window_adjustment_changed (hadjustment, scrolled_window);
  gtk_scrolled_window_adjustment_value_changed (hadjustment, scrolled_window);

  child = gtk_bin_get_child (bin);
  if (child)
    gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (child), hadjustment);

  if (gtk_widget_should_animate (GTK_WIDGET (scrolled_window)))
    gtk_adjustment_enable_animation (hadjustment, gtk_widget_get_frame_clock (GTK_WIDGET (scrolled_window)), ANIMATION_DURATION);

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_HADJUSTMENT]);
}

/**
 * gtk_scrolled_window_set_vadjustment:
 * @scrolled_window: a #GtkScrolledWindow
 * @vadjustment: vertical scroll adjustment
 *
 * Sets the #GtkAdjustment for the vertical scrollbar.
 */
void
gtk_scrolled_window_set_vadjustment (GtkScrolledWindow *scrolled_window,
                                     GtkAdjustment     *vadjustment)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkBin *bin;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (vadjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadjustment));
  else
    vadjustment = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  bin = GTK_BIN (scrolled_window);

  if (!priv->vscrollbar)
    {
      priv->vscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, vadjustment);

      gtk_widget_insert_after (priv->vscrollbar, GTK_WIDGET (scrolled_window), priv->hscrollbar);
      update_scrollbar_positions (scrolled_window);
    }
  else
    {
      GtkAdjustment *old_adjustment;
      
      old_adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      if (old_adjustment == vadjustment)
	return;

      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_changed,
                                            scrolled_window);
      g_signal_handlers_disconnect_by_func (old_adjustment,
                                            gtk_scrolled_window_adjustment_value_changed,
                                            scrolled_window);

      gtk_adjustment_enable_animation (old_adjustment, NULL, 0);
      gtk_scrollbar_set_adjustment (GTK_SCROLLBAR (priv->vscrollbar), vadjustment);
    }

  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

  g_signal_connect (vadjustment,
                    "changed",
		    G_CALLBACK (gtk_scrolled_window_adjustment_changed),
		    scrolled_window);
  g_signal_connect (vadjustment,
                    "value-changed",
		    G_CALLBACK (gtk_scrolled_window_adjustment_value_changed),
		    scrolled_window);

  gtk_scrolled_window_adjustment_changed (vadjustment, scrolled_window);
  gtk_scrolled_window_adjustment_value_changed (vadjustment, scrolled_window);

  child = gtk_bin_get_child (bin);
  if (child)
    gtk_scrollable_set_vadjustment (GTK_SCROLLABLE (child), vadjustment);

  if (gtk_widget_should_animate (GTK_WIDGET (scrolled_window)))
    gtk_adjustment_enable_animation (vadjustment, gtk_widget_get_frame_clock (GTK_WIDGET (scrolled_window)), ANIMATION_DURATION);

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_VADJUSTMENT]);
}

/**
 * gtk_scrolled_window_get_hadjustment:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Returns the horizontal scrollbar’s adjustment, used to connect the
 * horizontal scrollbar to the child widget’s horizontal scroll
 * functionality.
 *
 * Returns: (transfer none): the horizontal #GtkAdjustment
 */
GtkAdjustment*
gtk_scrolled_window_get_hadjustment (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
}

/**
 * gtk_scrolled_window_get_vadjustment:
 * @scrolled_window: a #GtkScrolledWindow
 * 
 * Returns the vertical scrollbar’s adjustment, used to connect the
 * vertical scrollbar to the child widget’s vertical scroll functionality.
 * 
 * Returns: (transfer none): the vertical #GtkAdjustment
 */
GtkAdjustment*
gtk_scrolled_window_get_vadjustment (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
}

/**
 * gtk_scrolled_window_get_hscrollbar:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Returns the horizontal scrollbar of @scrolled_window.
 *
 * Returns: (transfer none): the horizontal scrollbar of the scrolled window.
 */
GtkWidget*
gtk_scrolled_window_get_hscrollbar (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return priv->hscrollbar;
}

/**
 * gtk_scrolled_window_get_vscrollbar:
 * @scrolled_window: a #GtkScrolledWindow
 * 
 * Returns the vertical scrollbar of @scrolled_window.
 *
 * Returns: (transfer none): the vertical scrollbar of the scrolled window.
 */
GtkWidget*
gtk_scrolled_window_get_vscrollbar (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return priv->vscrollbar;
}

/**
 * gtk_scrolled_window_set_policy:
 * @scrolled_window: a #GtkScrolledWindow
 * @hscrollbar_policy: policy for horizontal bar
 * @vscrollbar_policy: policy for vertical bar
 * 
 * Sets the scrollbar policy for the horizontal and vertical scrollbars.
 *
 * The policy determines when the scrollbar should appear; it is a value
 * from the #GtkPolicyType enumeration. If %GTK_POLICY_ALWAYS, the
 * scrollbar is always present; if %GTK_POLICY_NEVER, the scrollbar is
 * never present; if %GTK_POLICY_AUTOMATIC, the scrollbar is present only
 * if needed (that is, if the slider part of the bar would be smaller
 * than the trough — the display is larger than the page size).
 */
void
gtk_scrolled_window_set_policy (GtkScrolledWindow *scrolled_window,
				GtkPolicyType      hscrollbar_policy,
				GtkPolicyType      vscrollbar_policy)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GObject *object = G_OBJECT (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if ((priv->hscrollbar_policy != hscrollbar_policy) ||
      (priv->vscrollbar_policy != vscrollbar_policy))
    {
      priv->hscrollbar_policy = hscrollbar_policy;
      priv->vscrollbar_policy = vscrollbar_policy;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (object, properties[PROP_HSCROLLBAR_POLICY]);
      g_object_notify_by_pspec (object, properties[PROP_VSCROLLBAR_POLICY]);
    }
}

/**
 * gtk_scrolled_window_get_policy:
 * @scrolled_window: a #GtkScrolledWindow
 * @hscrollbar_policy: (out) (allow-none): location to store the policy 
 *     for the horizontal scrollbar, or %NULL
 * @vscrollbar_policy: (out) (allow-none): location to store the policy
 *     for the vertical scrollbar, or %NULL
 * 
 * Retrieves the current policy values for the horizontal and vertical
 * scrollbars. See gtk_scrolled_window_set_policy().
 */
void
gtk_scrolled_window_get_policy (GtkScrolledWindow *scrolled_window,
				GtkPolicyType     *hscrollbar_policy,
				GtkPolicyType     *vscrollbar_policy)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (hscrollbar_policy)
    *hscrollbar_policy = priv->hscrollbar_policy;
  if (vscrollbar_policy)
    *vscrollbar_policy = priv->vscrollbar_policy;
}

static void
gtk_scrolled_window_set_placement_internal (GtkScrolledWindow *scrolled_window,
					    GtkCornerType      window_placement)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->window_placement != window_placement)
    {
      priv->window_placement = window_placement;
      update_scrollbar_positions (scrolled_window);

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_WINDOW_PLACEMENT]);
    }
}

/**
 * gtk_scrolled_window_set_placement:
 * @scrolled_window: a #GtkScrolledWindow
 * @window_placement: position of the child window
 *
 * Sets the placement of the contents with respect to the scrollbars
 * for the scrolled window.
 * 
 * The default is %GTK_CORNER_TOP_LEFT, meaning the child is
 * in the top left, with the scrollbars underneath and to the right.
 * Other values in #GtkCornerType are %GTK_CORNER_TOP_RIGHT,
 * %GTK_CORNER_BOTTOM_LEFT, and %GTK_CORNER_BOTTOM_RIGHT.
 *
 * See also gtk_scrolled_window_get_placement() and
 * gtk_scrolled_window_unset_placement().
 */
void
gtk_scrolled_window_set_placement (GtkScrolledWindow *scrolled_window,
				   GtkCornerType      window_placement)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  gtk_scrolled_window_set_placement_internal (scrolled_window, window_placement);
}

/**
 * gtk_scrolled_window_get_placement:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Gets the placement of the contents with respect to the scrollbars
 * for the scrolled window. See gtk_scrolled_window_set_placement().
 *
 * Returns: the current placement value.
 *
 * See also gtk_scrolled_window_set_placement() and
 * gtk_scrolled_window_unset_placement().
 **/
GtkCornerType
gtk_scrolled_window_get_placement (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), GTK_CORNER_TOP_LEFT);

  return priv->window_placement;
}

/**
 * gtk_scrolled_window_unset_placement:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Unsets the placement of the contents with respect to the scrollbars
 * for the scrolled window. If no window placement is set for a scrolled
 * window, it defaults to %GTK_CORNER_TOP_LEFT.
 *
 * See also gtk_scrolled_window_set_placement() and
 * gtk_scrolled_window_get_placement().
 **/
void
gtk_scrolled_window_unset_placement (GtkScrolledWindow *scrolled_window)
{
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  gtk_scrolled_window_set_placement_internal (scrolled_window, GTK_CORNER_TOP_LEFT);
}

/**
 * gtk_scrolled_window_set_shadow_type:
 * @scrolled_window: a #GtkScrolledWindow
 * @type: kind of shadow to draw around scrolled window contents
 *
 * Changes the type of shadow drawn around the contents of
 * @scrolled_window.
 **/
void
gtk_scrolled_window_set_shadow_type (GtkScrolledWindow *scrolled_window,
				     GtkShadowType      type)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (type >= GTK_SHADOW_NONE && type <= GTK_SHADOW_ETCHED_OUT);

  if (priv->shadow_type != type)
    {
      priv->shadow_type = type;

      context = gtk_widget_get_style_context (GTK_WIDGET (scrolled_window));
      if (type != GTK_SHADOW_NONE)
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
      else
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_FRAME);

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_SHADOW_TYPE]);
    }
}

/**
 * gtk_scrolled_window_get_shadow_type:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Gets the shadow type of the scrolled window. See 
 * gtk_scrolled_window_set_shadow_type().
 *
 * Returns: the current shadow type
 **/
GtkShadowType
gtk_scrolled_window_get_shadow_type (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_NONE);

  return priv->shadow_type;
}

/**
 * gtk_scrolled_window_set_kinetic_scrolling:
 * @scrolled_window: a #GtkScrolledWindow
 * @kinetic_scrolling: %TRUE to enable kinetic scrolling
 *
 * Turns kinetic scrolling on or off.
 * Kinetic scrolling only applies to devices with source
 * %GDK_SOURCE_TOUCHSCREEN.
 **/
void
gtk_scrolled_window_set_kinetic_scrolling (GtkScrolledWindow *scrolled_window,
                                           gboolean           kinetic_scrolling)
{
  GtkPropagationPhase phase = GTK_PHASE_NONE;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (priv->kinetic_scrolling == kinetic_scrolling)
    return;

  priv->kinetic_scrolling = kinetic_scrolling;
  gtk_scrolled_window_check_attach_pan_gesture (scrolled_window);

  if (priv->kinetic_scrolling)
    phase = GTK_PHASE_CAPTURE;
  else
    gtk_scrolled_window_cancel_deceleration (scrolled_window);

  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->drag_gesture), phase);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->swipe_gesture), phase);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->long_press_gesture), phase);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan_gesture), phase);

  g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_KINETIC_SCROLLING]);
}

/**
 * gtk_scrolled_window_get_kinetic_scrolling:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Returns the specified kinetic scrolling behavior.
 *
 * Returns: the scrolling behavior flags.
 */
gboolean
gtk_scrolled_window_get_kinetic_scrolling (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), FALSE);

  return priv->kinetic_scrolling;
}

/**
 * gtk_scrolled_window_set_capture_button_press:
 * @scrolled_window: a #GtkScrolledWindow
 * @capture_button_press: %TRUE to capture button presses
 *
 * Changes the behaviour of @scrolled_window with regard to the initial
 * event that possibly starts kinetic scrolling. When @capture_button_press
 * is set to %TRUE, the event is captured by the scrolled window, and
 * then later replayed if it is meant to go to the child widget.
 *
 * This should be enabled if any child widgets perform non-reversible
 * actions on button press events. If they don't, and additionally handle
 * #GtkWidget::grab-broken-event, it might be better to set @capture_button_press
 * to %FALSE.
 *
 * This setting only has an effect if kinetic scrolling is enabled.
 */
void
gtk_scrolled_window_set_capture_button_press (GtkScrolledWindow *scrolled_window,
                                              gboolean           capture_button_press)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  priv->capture_button_press = capture_button_press;
}

/**
 * gtk_scrolled_window_get_capture_button_press:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Return whether button presses are captured during kinetic
 * scrolling. See gtk_scrolled_window_set_capture_button_press().
 *
 * Returns: %TRUE if button presses are captured during kinetic scrolling
 */
gboolean
gtk_scrolled_window_get_capture_button_press (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), FALSE);

  return priv->capture_button_press;
}

static void
gtk_scrolled_window_destroy (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    gtk_widget_destroy (child);

  remove_indicator (scrolled_window, &priv->hindicator);
  remove_indicator (scrolled_window, &priv->vindicator);

  if (priv->hscrollbar)
    {
      GtkAdjustment *hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));

      g_signal_handlers_disconnect_by_data (hadjustment, scrolled_window);
      g_signal_handlers_disconnect_by_data (hadjustment, &priv->hindicator);

      gtk_widget_unparent (priv->hscrollbar);
      priv->hscrollbar = NULL;
    }

  if (priv->vscrollbar)
    {
      GtkAdjustment *vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

      g_signal_handlers_disconnect_by_data (vadjustment, scrolled_window);
      g_signal_handlers_disconnect_by_data (vadjustment, &priv->vindicator);

      gtk_widget_unparent (priv->vscrollbar);
      priv->vscrollbar = NULL;
    }

  if (priv->deceleration_id)
    {
      gtk_widget_remove_tick_callback (widget, priv->deceleration_id);
      priv->deceleration_id = 0;
    }

  if (priv->scroll_events_overshoot_id)
    {
      g_source_remove (priv->scroll_events_overshoot_id);
      priv->scroll_events_overshoot_id = 0;
    }

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->destroy (widget);
}

static void
gtk_scrolled_window_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_scrolled_window_set_hadjustment (scrolled_window,
					   g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_scrolled_window_set_vadjustment (scrolled_window,
					   g_value_get_object (value));
      break;
    case PROP_HSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      g_value_get_enum (value),
				      priv->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      priv->hscrollbar_policy,
				      g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT:
      gtk_scrolled_window_set_placement_internal (scrolled_window,
		      				  g_value_get_enum (value));
      break;
    case PROP_SHADOW_TYPE:
      gtk_scrolled_window_set_shadow_type (scrolled_window,
					   g_value_get_enum (value));
      break;
    case PROP_MIN_CONTENT_WIDTH:
      gtk_scrolled_window_set_min_content_width (scrolled_window,
                                                 g_value_get_int (value));
      break;
    case PROP_MIN_CONTENT_HEIGHT:
      gtk_scrolled_window_set_min_content_height (scrolled_window,
                                                  g_value_get_int (value));
      break;
    case PROP_KINETIC_SCROLLING:
      gtk_scrolled_window_set_kinetic_scrolling (scrolled_window,
                                                 g_value_get_boolean (value));
      break;
    case PROP_OVERLAY_SCROLLING:
      gtk_scrolled_window_set_overlay_scrolling (scrolled_window,
                                                 g_value_get_boolean (value));
      break;
    case PROP_MAX_CONTENT_WIDTH:
      gtk_scrolled_window_set_max_content_width (scrolled_window,
                                                 g_value_get_int (value));
      break;
    case PROP_MAX_CONTENT_HEIGHT:
      gtk_scrolled_window_set_max_content_height (scrolled_window,
                                                  g_value_get_int (value));
      break;
    case PROP_PROPAGATE_NATURAL_WIDTH:
      gtk_scrolled_window_set_propagate_natural_width (scrolled_window,
						       g_value_get_boolean (value));
      break;
    case PROP_PROPAGATE_NATURAL_HEIGHT:
      gtk_scrolled_window_set_propagate_natural_height (scrolled_window,
						       g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrolled_window_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (object);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value,
			  G_OBJECT (gtk_scrolled_window_get_hadjustment (scrolled_window)));
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value,
			  G_OBJECT (gtk_scrolled_window_get_vadjustment (scrolled_window)));
      break;
    case PROP_WINDOW_PLACEMENT:
      g_value_set_enum (value, priv->window_placement);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->vscrollbar_policy);
      break;
    case PROP_MIN_CONTENT_WIDTH:
      g_value_set_int (value, priv->min_content_width);
      break;
    case PROP_MIN_CONTENT_HEIGHT:
      g_value_set_int (value, priv->min_content_height);
      break;
    case PROP_KINETIC_SCROLLING:
      g_value_set_boolean (value, priv->kinetic_scrolling);
      break;
    case PROP_OVERLAY_SCROLLING:
      g_value_set_boolean (value, priv->overlay_scrolling);
      break;
    case PROP_MAX_CONTENT_WIDTH:
      g_value_set_int (value, priv->max_content_width);
      break;
    case PROP_MAX_CONTENT_HEIGHT:
      g_value_set_int (value, priv->max_content_height);
      break;
    case PROP_PROPAGATE_NATURAL_WIDTH:
      g_value_set_boolean (value, priv->propagate_natural_width);
      break;
    case PROP_PROPAGATE_NATURAL_HEIGHT:
      g_value_set_boolean (value, priv->propagate_natural_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scrolled_window_inner_allocation (GtkScrolledWindow *scrolled_window,
                                      GtkAllocation     *rect)
{
  GtkWidget *child;
  GtkBorder border = { 0 };

  gtk_scrolled_window_relative_allocation (scrolled_window, rect);
  rect->x = 0;
  rect->y = 0;
  child = gtk_bin_get_child (GTK_BIN (scrolled_window));
  if (child && gtk_scrollable_get_border (GTK_SCROLLABLE (child), &border))
    {
      rect->x += border.left;
      rect->y += border.top;
      rect->width -= border.left + border.right;
      rect->height -= border.top + border.bottom;
    }
}

static void
gtk_scrolled_window_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->hscrollbar_visible &&
      priv->vscrollbar_visible)
    gtk_scrolled_window_snapshot_scrollbars_junction (scrolled_window, snapshot);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->snapshot (widget, snapshot);

  gtk_scrolled_window_snapshot_undershoot (scrolled_window, snapshot);
  gtk_scrolled_window_snapshot_overshoot (scrolled_window, snapshot);
}

static gboolean
gtk_scrolled_window_scroll_child (GtkScrolledWindow *scrolled_window,
				  GtkScrollType      scroll,
				  gboolean           horizontal)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *adjustment = NULL;

  switch (scroll)
    {
    case GTK_SCROLL_STEP_UP:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_DOWN:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_STEP_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_UP:
      scroll = GTK_SCROLL_PAGE_BACKWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_DOWN:
      scroll = GTK_SCROLL_PAGE_FORWARD;
      horizontal = FALSE;
      break;
    case GTK_SCROLL_PAGE_LEFT:
      scroll = GTK_SCROLL_STEP_BACKWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_PAGE_RIGHT:
      scroll = GTK_SCROLL_STEP_FORWARD;
      horizontal = TRUE;
      break;
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_START:
    case GTK_SCROLL_END:
      break;
    case GTK_SCROLL_NONE:
    case GTK_SCROLL_JUMP:
    default:
      g_warning ("Invalid scroll type %u for GtkScrolledWindow::scroll-child", scroll);
      return FALSE;
    }

  if (horizontal)
    {
      if (may_hscroll (scrolled_window))
        adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      else
        return FALSE;
    }
  else
    {
      if (may_vscroll (scrolled_window))
        adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      else
        return FALSE;
    }

  if (adjustment)
    {
      gdouble value = gtk_adjustment_get_value (adjustment);
      
      switch (scroll)
	{
	case GTK_SCROLL_STEP_FORWARD:
	  value += gtk_adjustment_get_step_increment (adjustment);
	  break;
	case GTK_SCROLL_STEP_BACKWARD:
	  value -= gtk_adjustment_get_step_increment (adjustment);
	  break;
	case GTK_SCROLL_PAGE_FORWARD:
	  value += gtk_adjustment_get_page_increment (adjustment);
	  break;
	case GTK_SCROLL_PAGE_BACKWARD:
	  value -= gtk_adjustment_get_page_increment (adjustment);
	  break;
	case GTK_SCROLL_START:
	  value = gtk_adjustment_get_lower (adjustment);
	  break;
	case GTK_SCROLL_END:
	  value = gtk_adjustment_get_upper (adjustment);
	  break;
        case GTK_SCROLL_STEP_UP:
        case GTK_SCROLL_STEP_DOWN:
        case GTK_SCROLL_STEP_LEFT:
        case GTK_SCROLL_STEP_RIGHT:
        case GTK_SCROLL_PAGE_UP:
        case GTK_SCROLL_PAGE_DOWN:
        case GTK_SCROLL_PAGE_LEFT:
        case GTK_SCROLL_PAGE_RIGHT:
        case GTK_SCROLL_NONE:
        case GTK_SCROLL_JUMP:
	default:
	  g_assert_not_reached ();
	  break;
	}

      gtk_adjustment_animate_to_value (adjustment, value);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_scrolled_window_move_focus_out (GtkScrolledWindow *scrolled_window,
				    GtkDirectionType   direction_type)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *toplevel;

  /* Focus out of the scrolled window entirely. We do this by setting
   * a flag, then propagating the focus motion to the notebook.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (scrolled_window));
  if (!gtk_widget_is_toplevel (toplevel))
    return;

  g_object_ref (scrolled_window);

  priv->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move-focus", direction_type);
  priv->focus_out = FALSE;

  g_object_unref (scrolled_window);
}

static void
gtk_scrolled_window_relative_allocation (GtkScrolledWindow *scrolled_window,
                                         GtkAllocation     *allocation)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gint sb_width;
  gint sb_height;
  int width, height;

  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (allocation != NULL);

  /* Get possible scrollbar dimensions */
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  width = gtk_widget_get_width (GTK_WIDGET (scrolled_window));
  height = gtk_widget_get_height (GTK_WIDGET (scrolled_window));

  allocation->x = 0;
  allocation->y = 0;
  allocation->width = width;
  allocation->height = height;

  /* Subtract some things from our available allocation size */
  if (priv->vscrollbar_visible && !priv->use_indicators)
    {
      gboolean is_rtl;

      is_rtl = _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL;

      if ((!is_rtl &&
	   (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
	    priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
	  (is_rtl &&
	   (priv->window_placement == GTK_CORNER_TOP_LEFT ||
	    priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        allocation->x += sb_width;

      allocation->width = MAX (1, width - sb_width);
    }

  if (priv->hscrollbar_visible && !priv->use_indicators)
    {

      if (priv->window_placement == GTK_CORNER_BOTTOM_LEFT ||
	  priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)
	allocation->y += (sb_height);

      allocation->height = MAX (1, height - sb_height);
    }
}

static gboolean
_gtk_scrolled_window_get_overshoot (GtkScrolledWindow *scrolled_window,
                                    gint              *overshoot_x,
                                    gint              *overshoot_y)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *vadjustment, *hadjustment;
  gdouble lower, upper, x, y;

  /* Vertical overshoot */
  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  lower = gtk_adjustment_get_lower (vadjustment);
  upper = gtk_adjustment_get_upper (vadjustment) -
    gtk_adjustment_get_page_size (vadjustment);

  if (priv->unclamped_vadj_value < lower)
    y = priv->unclamped_vadj_value - lower;
  else if (priv->unclamped_vadj_value > upper)
    y = priv->unclamped_vadj_value - upper;
  else
    y = 0;

  /* Horizontal overshoot */
  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  lower = gtk_adjustment_get_lower (hadjustment);
  upper = gtk_adjustment_get_upper (hadjustment) -
    gtk_adjustment_get_page_size (hadjustment);

  if (priv->unclamped_hadj_value < lower)
    x = priv->unclamped_hadj_value - lower;
  else if (priv->unclamped_hadj_value > upper)
    x = priv->unclamped_hadj_value - upper;
  else
    x = 0;

  if (overshoot_x)
    *overshoot_x = x;

  if (overshoot_y)
    *overshoot_y = y;

  return (x != 0 || y != 0);
}

static void
gtk_scrolled_window_allocate_child (GtkScrolledWindow   *swindow,
                                    int                  width,
                                    int                  height)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (swindow);
  GtkWidget     *widget = GTK_WIDGET (swindow), *child;
  GtkAllocation  child_allocation;
  int sb_width;
  int sb_height;

  child = gtk_bin_get_child (GTK_BIN (widget));

  child_allocation = (GtkAllocation) {0, 0, width, height};

  /* Get possible scrollbar dimensions */
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  /* Subtract some things from our available allocation size */
  if (priv->vscrollbar_visible && !priv->use_indicators)
    {
      gboolean is_rtl;

      is_rtl = _gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

      if ((!is_rtl &&
           (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
            priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
          (is_rtl &&
           (priv->window_placement == GTK_CORNER_TOP_LEFT ||
            priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        child_allocation.x += sb_width;

      child_allocation.width = MAX (1, child_allocation.width - sb_width);
    }

  if (priv->hscrollbar_visible && !priv->use_indicators)
    {

      if (priv->window_placement == GTK_CORNER_BOTTOM_LEFT ||
          priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)
        child_allocation.y += (sb_height);

      child_allocation.height = MAX (1, child_allocation.height - sb_height);
    }

  gtk_widget_size_allocate (child, &child_allocation, -1);
}

static void
gtk_scrolled_window_allocate_scrollbar (GtkScrolledWindow *scrolled_window,
                                        GtkWidget         *scrollbar,
                                        GtkAllocation     *allocation)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAllocation child_allocation, content_allocation;
  GtkWidget *widget = GTK_WIDGET (scrolled_window);
  gint sb_height, sb_width;

  gtk_scrolled_window_inner_allocation (scrolled_window, &content_allocation);
  gtk_widget_measure (priv->vscrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                      &sb_width, NULL, NULL, NULL);
  gtk_widget_measure (priv->hscrollbar, GTK_ORIENTATION_VERTICAL, -1,
                      &sb_height, NULL, NULL, NULL);

  if (scrollbar == priv->hscrollbar)
    {
      child_allocation.x = content_allocation.x;

      if (priv->window_placement == GTK_CORNER_TOP_LEFT ||
	  priv->window_placement == GTK_CORNER_TOP_RIGHT)
        {
          if (priv->use_indicators)
	    child_allocation.y = content_allocation.y + content_allocation.height - sb_height;
          else
	    child_allocation.y = content_allocation.y + content_allocation.height;
        }
      else
        {
          if (priv->use_indicators)
	    child_allocation.y = content_allocation.y;
          else
	    child_allocation.y = content_allocation.y - sb_height;
        }

      child_allocation.width = content_allocation.width;
      child_allocation.height = sb_height;
    }
  else
    {
      g_assert (scrollbar == priv->vscrollbar);

      if ((_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL &&
	   (priv->window_placement == GTK_CORNER_TOP_RIGHT ||
	    priv->window_placement == GTK_CORNER_BOTTOM_RIGHT)) ||
	  (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR &&
	   (priv->window_placement == GTK_CORNER_TOP_LEFT ||
	    priv->window_placement == GTK_CORNER_BOTTOM_LEFT)))
        {
          if (priv->use_indicators)
	    child_allocation.x = content_allocation.x + content_allocation.width - sb_width;
          else
	    child_allocation.x = content_allocation.x + content_allocation.width;
        }
      else
        {
          if (priv->use_indicators)
	    child_allocation.x = content_allocation.x;
          else
	    child_allocation.x = content_allocation.x - sb_width;
        }

      child_allocation.y = content_allocation.y;
      child_allocation.width = sb_width;
      child_allocation.height = content_allocation.height;
    }

  *allocation = child_allocation;
}

static void
install_scroll_cursor (GtkScrolledWindow *scrolled_window)
{
  gtk_widget_set_cursor_from_name (GTK_WIDGET (scrolled_window), "all-scroll");
}

static void
uninstall_scroll_cursor (GtkScrolledWindow *scrolled_window)
{
  gtk_widget_set_cursor (GTK_WIDGET (scrolled_window), NULL);
}

static void
_gtk_scrolled_window_set_adjustment_value (GtkScrolledWindow *scrolled_window,
                                           GtkAdjustment     *adjustment,
                                           gdouble            value)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gdouble lower, upper, *prev_value;
  GtkPositionType edge_pos;
  gboolean vertical;

  lower = gtk_adjustment_get_lower (adjustment) - MAX_OVERSHOOT_DISTANCE;
  upper = gtk_adjustment_get_upper (adjustment) -
    gtk_adjustment_get_page_size (adjustment) + MAX_OVERSHOOT_DISTANCE;

  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    vertical = FALSE;
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    vertical = TRUE;
  else
    return;

  if (vertical)
    prev_value = &priv->unclamped_vadj_value;
  else
    prev_value = &priv->unclamped_hadj_value;

  value = CLAMP (value, lower, upper);

  if (*prev_value == value)
    return;

  *prev_value = value;
  gtk_adjustment_set_value (adjustment, value);

  if (value == lower)
    edge_pos = vertical ? GTK_POS_TOP : GTK_POS_LEFT;
  else if (value == upper)
    edge_pos = vertical ? GTK_POS_BOTTOM : GTK_POS_RIGHT;
  else
    return;

  /* Invert horizontal edge position on RTL */
  if (!vertical &&
      _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL)
    edge_pos = (edge_pos == GTK_POS_LEFT) ? GTK_POS_RIGHT : GTK_POS_LEFT;

  g_signal_emit (scrolled_window, signals[EDGE_OVERSHOT], 0, edge_pos);
}

static gboolean
scrolled_window_deceleration_cb (GtkWidget         *widget,
                                 GdkFrameClock     *frame_clock,
                                 gpointer           user_data)
{
  KineticScrollData *data = user_data;
  GtkScrolledWindow *scrolled_window = data->scrolled_window;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkAdjustment *hadjustment, *vadjustment;
  gint64 current_time;
  gdouble position, elapsed;

  current_time = gdk_frame_clock_get_frame_time (frame_clock);
  elapsed = (current_time - data->last_deceleration_time) / 1000000.0;
  data->last_deceleration_time = current_time;

  hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  if (data->hscrolling &&
      gtk_kinetic_scrolling_tick (data->hscrolling, elapsed, &position))
    {
      priv->unclamped_hadj_value = position;
      gtk_adjustment_set_value (hadjustment, position);
    }
  else if (data->hscrolling)
    g_clear_pointer (&data->hscrolling, gtk_kinetic_scrolling_free);

  if (data->vscrolling &&
      gtk_kinetic_scrolling_tick (data->vscrolling, elapsed, &position))
    {
      priv->unclamped_vadj_value = position;
      gtk_adjustment_set_value (vadjustment, position);
    }
  else if (data->vscrolling)
    g_clear_pointer (&data->vscrolling, gtk_kinetic_scrolling_free);

  if (!data->hscrolling && !data->vscrolling)
    {
      gtk_scrolled_window_cancel_deceleration (scrolled_window);
      return G_SOURCE_REMOVE;
    }

  gtk_scrolled_window_invalidate_overshoot (scrolled_window);

  return G_SOURCE_CONTINUE;
}

static void
gtk_scrolled_window_cancel_deceleration (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->deceleration_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (scrolled_window),
                                       priv->deceleration_id);
      priv->deceleration_id = 0;
    }
}

static void
kinetic_scroll_data_free (KineticScrollData *data)
{
  if (data->hscrolling)
    gtk_kinetic_scrolling_free (data->hscrolling);
  if (data->vscrolling)
    gtk_kinetic_scrolling_free (data->vscrolling);

  g_free (data);
}

static void
gtk_scrolled_window_start_deceleration (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GdkFrameClock *frame_clock;
  KineticScrollData *data;

  g_return_if_fail (priv->deceleration_id == 0);

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (scrolled_window));

  data = g_new0 (KineticScrollData, 1);
  data->scrolled_window = scrolled_window;
  data->last_deceleration_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (may_hscroll (scrolled_window))
    {
      gdouble lower,upper;
      GtkAdjustment *hadjustment;

      hadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
      lower = gtk_adjustment_get_lower (hadjustment);
      upper = gtk_adjustment_get_upper (hadjustment);
      upper -= gtk_adjustment_get_page_size (hadjustment);
      data->hscrolling =
        gtk_kinetic_scrolling_new (lower,
                                   upper,
                                   MAX_OVERSHOOT_DISTANCE,
                                   DECELERATION_FRICTION,
                                   OVERSHOOT_FRICTION,
                                   priv->unclamped_hadj_value,
                                   priv->x_velocity);
    }

  if (may_vscroll (scrolled_window))
    {
      gdouble lower,upper;
      GtkAdjustment *vadjustment;

      vadjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
      lower = gtk_adjustment_get_lower(vadjustment);
      upper = gtk_adjustment_get_upper(vadjustment);
      upper -= gtk_adjustment_get_page_size(vadjustment);
      data->vscrolling =
        gtk_kinetic_scrolling_new (lower,
                                   upper,
                                   MAX_OVERSHOOT_DISTANCE,
                                   DECELERATION_FRICTION,
                                   OVERSHOOT_FRICTION,
                                   priv->unclamped_vadj_value,
                                   priv->y_velocity);
    }

  priv->deceleration_id = gtk_widget_add_tick_callback (GTK_WIDGET (scrolled_window),
                                                        scrolled_window_deceleration_cb, data,
                                                        (GDestroyNotify) kinetic_scroll_data_free);
}

static gboolean
gtk_scrolled_window_focus (GtkWidget        *widget,
			   GtkDirectionType  direction)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *child;
  gboolean had_focus_child;

  had_focus_child = gtk_widget_get_focus_child (widget) != NULL;

  if (priv->focus_out)
    {
      priv->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }
  
  if (gtk_widget_is_focus (widget))
    return FALSE;

  /* We only put the scrolled window itself in the focus chain if it
   * isn't possible to focus any children.
   */
  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    {
      if (gtk_widget_child_focus (child, direction))
	return TRUE;
    }

  if (!had_focus_child && gtk_widget_get_can_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
					gpointer       data)
{
  GtkScrolledWindow *scrolled_window = data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    {
      if (priv->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  gboolean visible;

          visible = priv->hscrollbar_visible;
          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window, priv->hscrollbar);

          if (priv->hscrollbar_visible != visible)
            gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
	}
    }
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    {
      if (priv->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  gboolean visible;

          visible = priv->vscrollbar_visible;
          gtk_scrolled_window_update_scrollbar_visibility_flags (scrolled_window, priv->vscrollbar);

          if (priv->vscrollbar_visible != visible)
            gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
	}
    }
}

static void
maybe_emit_edge_reached (GtkScrolledWindow *scrolled_window,
			 GtkAdjustment *adjustment)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gdouble value, lower, upper, page_size;
  GtkPositionType edge_pos;
  gboolean vertical;

  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    vertical = FALSE;
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    vertical = TRUE;
  else
    return;

  value = gtk_adjustment_get_value (adjustment);
  lower = gtk_adjustment_get_lower (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);

  if (value == lower)
    edge_pos = vertical ? GTK_POS_TOP: GTK_POS_LEFT;
  else if (value == upper - page_size)
    edge_pos = vertical ? GTK_POS_BOTTOM : GTK_POS_RIGHT;
  else
    return;

  if (!vertical &&
      _gtk_widget_get_direction (GTK_WIDGET (scrolled_window)) == GTK_TEXT_DIR_RTL)
    edge_pos = (edge_pos == GTK_POS_LEFT) ? GTK_POS_RIGHT : GTK_POS_LEFT;

  g_signal_emit (scrolled_window, signals[EDGE_REACHED], 0, edge_pos);
}

static void
gtk_scrolled_window_adjustment_value_changed (GtkAdjustment *adjustment,
                                              gpointer       user_data)
{
  GtkScrolledWindow *scrolled_window = user_data;
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  maybe_emit_edge_reached (scrolled_window, adjustment);

  /* Allow overshooting for kinetic scrolling operations */
  if (priv->deceleration_id)
    return;

  /* Ensure GtkAdjustment and unclamped values are in sync */
  if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar)))
    priv->unclamped_hadj_value = gtk_adjustment_get_value (adjustment);
  else if (adjustment == gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar)))
    priv->unclamped_vadj_value = gtk_adjustment_get_value (adjustment);
}

static void
gtk_scrolled_window_add (GtkContainer *container,
                         GtkWidget    *child)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (container);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkBin *bin;
  GtkWidget *child_widget, *scrollable_child;
  GtkAdjustment *hadj, *vadj;

  bin = GTK_BIN (container);
  child_widget = gtk_bin_get_child (bin);
  g_return_if_fail (child_widget == NULL);

  /* gtk_scrolled_window_set_[hv]adjustment have the side-effect
   * of creating the scrollbars
   */
  if (!priv->hscrollbar)
    gtk_scrolled_window_set_hadjustment (scrolled_window, NULL);

  if (!priv->vscrollbar)
    gtk_scrolled_window_set_vadjustment (scrolled_window, NULL);

  hadj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  vadj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));

  if (GTK_IS_SCROLLABLE (child))
    {
      scrollable_child = child;
    }
  else
    {
      scrollable_child = gtk_viewport_new (hadj, vadj);
      gtk_container_set_focus_hadjustment (GTK_CONTAINER (scrollable_child),
                                           gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_container_set_focus_vadjustment (GTK_CONTAINER (scrollable_child),
                                           gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_container_add (GTK_CONTAINER (scrollable_child), child);
      priv->auto_added_viewport = TRUE;
    }

  _gtk_bin_set_child (bin, scrollable_child);
  gtk_widget_insert_after (scrollable_child, GTK_WIDGET (bin), NULL);

  g_object_set (scrollable_child, "hadjustment", hadj, "vadjustment", vadj, NULL);
}

static void
gtk_scrolled_window_remove (GtkContainer *container,
			    GtkWidget    *child)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (container);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  GtkWidget *scrollable_child;

  if (!priv->auto_added_viewport)
    {
      scrollable_child = child;
    }
  else
    {
      scrollable_child = gtk_bin_get_child (GTK_BIN (container));
      if (scrollable_child == child)
        {
          /* @child is the automatically added viewport. */
          GtkWidget *grandchild = gtk_bin_get_child (GTK_BIN (child));

          /* Remove the viewport's child, if any. */
          if (grandchild)
            gtk_container_remove (GTK_CONTAINER (child), grandchild);
        }
      else
        {
          /* @child is (assumed to be) the viewport's child. */
          gtk_container_remove (GTK_CONTAINER (scrollable_child), child);
        }
    }

  g_object_set (scrollable_child, "hadjustment", NULL, "vadjustment", NULL, NULL);

  GTK_CONTAINER_CLASS (gtk_scrolled_window_parent_class)->remove (container, scrollable_child);

  priv->auto_added_viewport = FALSE;
}

static gboolean
gtk_widget_should_animate (GtkWidget *widget)
{
  if (!gtk_widget_get_mapped (widget))
    return FALSE;

  return gtk_settings_get_enable_animations (gtk_widget_get_settings (widget));
}

static void
gtk_scrolled_window_update_animating (GtkScrolledWindow *sw)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (sw);
  GtkAdjustment *adjustment;
  GdkFrameClock *clock = NULL;
  guint duration = 0;

  if (gtk_widget_should_animate (GTK_WIDGET (sw)))
    {
      clock = gtk_widget_get_frame_clock (GTK_WIDGET (sw)),
      duration = ANIMATION_DURATION;
    }

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->hscrollbar));
  gtk_adjustment_enable_animation (adjustment, clock, duration);

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (priv->vscrollbar));
  gtk_adjustment_enable_animation (adjustment, clock, duration);
}

static void
gtk_scrolled_window_map (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->map (widget);

  gtk_scrolled_window_update_animating (scrolled_window);
}

static void
indicator_reset (Indicator *indicator)
{
  if (indicator->conceil_timer)
    {
      g_source_remove (indicator->conceil_timer);
      indicator->conceil_timer = 0;
    }

  if (indicator->over_timeout_id)
    {
      g_source_remove (indicator->over_timeout_id);
      indicator->over_timeout_id = 0;
    }

  if (indicator->scrollbar && indicator->tick_id)
    {
      gtk_widget_remove_tick_callback (indicator->scrollbar,
                                       indicator->tick_id);
      indicator->tick_id = 0;
    }

  indicator->over = FALSE;
  gtk_progress_tracker_finish (&indicator->tracker);
  indicator->current_pos = indicator->source_pos = indicator->target_pos = 0;
  indicator->last_scroll_time = 0;
}

static void
gtk_scrolled_window_unmap (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->unmap (widget);

  gtk_scrolled_window_update_animating (scrolled_window);

  indicator_reset (&priv->hindicator);
  indicator_reset (&priv->vindicator);
}

static void
indicator_set_fade (Indicator *indicator,
                    gdouble    pos)
{
  gboolean visible, changed;

  changed = indicator->current_pos != pos;
  indicator->current_pos = pos;

  visible = indicator->current_pos != 0.0 || indicator->target_pos != 0.0;

  if (visible && indicator->conceil_timer == 0)
    {
      indicator->conceil_timer = g_timeout_add (INDICATOR_FADE_OUT_TIME, maybe_hide_indicator, indicator);
      g_source_set_name_by_id (indicator->conceil_timer, "[gtk] maybe_hide_indicator");
    }
  if (!visible && indicator->conceil_timer != 0)
    {
      g_source_remove (indicator->conceil_timer);
      indicator->conceil_timer = 0;
    }

  if (changed)
    {
      gtk_widget_set_opacity (indicator->scrollbar, indicator->current_pos);
    }
}

static gboolean
indicator_fade_cb (GtkWidget     *widget,
                   GdkFrameClock *frame_clock,
                   gpointer       user_data)
{
  Indicator *indicator = user_data;
  gdouble t;

  gtk_progress_tracker_advance_frame (&indicator->tracker,
                                      gdk_frame_clock_get_frame_time (frame_clock));
  t = gtk_progress_tracker_get_ease_out_cubic (&indicator->tracker, FALSE);

  indicator_set_fade (indicator,
                      indicator->source_pos + (t * (indicator->target_pos - indicator->source_pos)));

  if (gtk_progress_tracker_get_state (&indicator->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      indicator->tick_id = 0;
      return FALSE;
    }

  return TRUE;
}

static void
indicator_start_fade (Indicator *indicator,
                      gdouble    target)
{
  if (indicator->target_pos == target)
    return;

  indicator->target_pos = target;

  if (target != 0.0)
    indicator->last_scroll_time = g_get_monotonic_time ();

  if (gtk_widget_should_animate (indicator->scrollbar))
    {
      indicator->source_pos = indicator->current_pos;
      gtk_progress_tracker_start (&indicator->tracker, INDICATOR_FADE_OUT_DURATION * 1000, 0, 1.0);
      if (indicator->tick_id == 0)
        indicator->tick_id = gtk_widget_add_tick_callback (indicator->scrollbar, indicator_fade_cb, indicator, NULL);
    }
  else
    indicator_set_fade (indicator, target);
}

static gboolean
maybe_hide_indicator (gpointer data)
{
  Indicator *indicator = data;

  if (g_get_monotonic_time () - indicator->last_scroll_time >= INDICATOR_FADE_OUT_DELAY * 1000 &&
      !indicator->over)
    indicator_start_fade (indicator, 0.0);

  return G_SOURCE_CONTINUE;
}

static void
indicator_value_changed (GtkAdjustment *adjustment,
                         Indicator     *indicator)
{
  indicator->last_scroll_time = g_get_monotonic_time ();
  indicator_start_fade (indicator, 1.0);
}

static void
setup_indicator (GtkScrolledWindow *scrolled_window,
                 Indicator         *indicator,
                 GtkWidget         *scrollbar)
{
  GtkStyleContext *context;
  GtkAdjustment *adjustment;

  if (scrollbar == NULL)
    return;

  context = gtk_widget_get_style_context (scrollbar);
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));

  indicator->scrollbar = scrollbar;

  gtk_style_context_add_class (context, "overlay-indicator");
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (indicator_value_changed), indicator);

  gtk_widget_set_opacity (scrollbar, 0.0);
  indicator->current_pos = 0.0;
}

static void
remove_indicator (GtkScrolledWindow *scrolled_window,
                  Indicator         *indicator)
{
  GtkWidget *scrollbar;
  GtkStyleContext *context;
  GtkAdjustment *adjustment;

  if (indicator->scrollbar == NULL)
    return;

  scrollbar = indicator->scrollbar;
  indicator->scrollbar = NULL;

  context = gtk_widget_get_style_context (scrollbar);
  gtk_style_context_remove_class (context, "overlay-indicator");

  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));
  g_signal_handlers_disconnect_by_data (adjustment, indicator);

  if (indicator->conceil_timer)
    {
      g_source_remove (indicator->conceil_timer);
      indicator->conceil_timer = 0;
    }

  if (indicator->over_timeout_id)
    {
      g_source_remove (indicator->over_timeout_id);
      indicator->over_timeout_id = 0;
    }

  if (indicator->tick_id)
    {
      gtk_widget_remove_tick_callback (scrollbar, indicator->tick_id);
      indicator->tick_id = 0;
    }

  gtk_widget_set_opacity (scrollbar, 1.0);
  indicator->current_pos = 1.0;
}

static void
gtk_scrolled_window_sync_use_indicators (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  if (priv->use_indicators)
    {
      setup_indicator (scrolled_window, &priv->hindicator, priv->hscrollbar);
      setup_indicator (scrolled_window, &priv->vindicator, priv->vscrollbar);
    }
  else
    {
      remove_indicator (scrolled_window, &priv->hindicator);
      remove_indicator (scrolled_window, &priv->vindicator);
    }
}

static void
gtk_scrolled_window_update_use_indicators (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);
  gboolean use_indicators;

  use_indicators = priv->overlay_scrolling;

  if (priv->use_indicators != use_indicators)
    {
      priv->use_indicators = use_indicators;

      if (gtk_widget_get_realized (GTK_WIDGET (scrolled_window)))
        gtk_scrolled_window_sync_use_indicators (scrolled_window);

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

static void
gtk_scrolled_window_realize (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  priv->hindicator.scrollbar = priv->hscrollbar;
  priv->vindicator.scrollbar = priv->vscrollbar;

  gtk_scrolled_window_sync_use_indicators (scrolled_window);

  GTK_WIDGET_CLASS (gtk_scrolled_window_parent_class)->realize (widget);
}

/**
 * gtk_scrolled_window_get_min_content_width:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Gets the minimum content width of @scrolled_window, or -1 if not set.
 *
 * Returns: the minimum content width
 */
gint
gtk_scrolled_window_get_min_content_width (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), 0);

  return priv->min_content_width;
}

/**
 * gtk_scrolled_window_set_min_content_width:
 * @scrolled_window: a #GtkScrolledWindow
 * @width: the minimal content width
 *
 * Sets the minimum width that @scrolled_window should keep visible.
 * Note that this can and (usually will) be smaller than the minimum
 * size of the content.
 *
 * It is a programming error to set the minimum content width to a
 * value greater than #GtkScrolledWindow:max-content-width.
 */
void
gtk_scrolled_window_set_min_content_width (GtkScrolledWindow *scrolled_window,
                                           gint               width)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (width == -1 || priv->max_content_width == -1 || width <= priv->max_content_width);

  if (priv->min_content_width != width)
    {
      priv->min_content_width = width;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_MIN_CONTENT_WIDTH]);
    }
}

/**
 * gtk_scrolled_window_get_min_content_height:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Gets the minimal content height of @scrolled_window, or -1 if not set.
 *
 * Returns: the minimal content height
 */
gint
gtk_scrolled_window_get_min_content_height (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), 0);

  return priv->min_content_height;
}

/**
 * gtk_scrolled_window_set_min_content_height:
 * @scrolled_window: a #GtkScrolledWindow
 * @height: the minimal content height
 *
 * Sets the minimum height that @scrolled_window should keep visible.
 * Note that this can and (usually will) be smaller than the minimum
 * size of the content.
 *
 * It is a programming error to set the minimum content height to a
 * value greater than #GtkScrolledWindow:max-content-height.
 */
void
gtk_scrolled_window_set_min_content_height (GtkScrolledWindow *scrolled_window,
                                            gint               height)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (height == -1 || priv->max_content_height == -1 || height <= priv->max_content_height);

  if (priv->min_content_height != height)
    {
      priv->min_content_height = height;

      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_MIN_CONTENT_HEIGHT]);
    }
}

/**
 * gtk_scrolled_window_set_overlay_scrolling:
 * @scrolled_window: a #GtkScrolledWindow
 * @overlay_scrolling: whether to enable overlay scrolling
 *
 * Enables or disables overlay scrolling for this scrolled window.
 */
void
gtk_scrolled_window_set_overlay_scrolling (GtkScrolledWindow *scrolled_window,
                                           gboolean           overlay_scrolling)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (priv->overlay_scrolling != overlay_scrolling)
    {
      priv->overlay_scrolling = overlay_scrolling;

      gtk_scrolled_window_update_use_indicators (scrolled_window);

      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties[PROP_OVERLAY_SCROLLING]);
    }
}

/**
 * gtk_scrolled_window_get_overlay_scrolling:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Returns whether overlay scrolling is enabled for this scrolled window.
 *
 * Returns: %TRUE if overlay scrolling is enabled
 */
gboolean
gtk_scrolled_window_get_overlay_scrolling (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), TRUE);

  return priv->overlay_scrolling;
}

/**
 * gtk_scrolled_window_set_max_content_width:
 * @scrolled_window: a #GtkScrolledWindow
 * @width: the maximum content width
 *
 * Sets the maximum width that @scrolled_window should keep visible. The
 * @scrolled_window will grow up to this width before it starts scrolling
 * the content.
 *
 * It is a programming error to set the maximum content width to a value
 * smaller than #GtkScrolledWindow:min-content-width.
 */
void
gtk_scrolled_window_set_max_content_width (GtkScrolledWindow *scrolled_window,
                                           gint               width)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (width == -1 || priv->min_content_width == -1 || width >= priv->min_content_width);

  if (width != priv->max_content_width)
    {
      priv->max_content_width = width;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_MAX_CONTENT_WIDTH]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_max_content_width:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Returns the maximum content width set.
 *
 * Returns: the maximum content width, or -1
 */
gint
gtk_scrolled_window_get_max_content_width (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->max_content_width;
}

/**
 * gtk_scrolled_window_set_max_content_height:
 * @scrolled_window: a #GtkScrolledWindow
 * @height: the maximum content height
 *
 * Sets the maximum height that @scrolled_window should keep visible. The
 * @scrolled_window will grow up to this height before it starts scrolling
 * the content.
 *
 * It is a programming error to set the maximum content height to a value
 * smaller than #GtkScrolledWindow:min-content-height.
 */
void
gtk_scrolled_window_set_max_content_height (GtkScrolledWindow *scrolled_window,
                                            gint               height)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  g_return_if_fail (height == -1 || priv->min_content_height == -1 || height >= priv->min_content_height);

  if (height != priv->max_content_height)
    {
      priv->max_content_height = height;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_MAX_CONTENT_HEIGHT]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_max_content_height:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Returns the maximum content height set.
 *
 * Returns: the maximum content height, or -1
 */
gint
gtk_scrolled_window_get_max_content_height (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->max_content_height;
}

/**
 * gtk_scrolled_window_set_propagate_natural_width:
 * @scrolled_window: a #GtkScrolledWindow
 * @propagate: whether to propagate natural width
 *
 * Sets whether the natural width of the child should be calculated and propagated
 * through the scrolled window’s requested natural width.
 */
void
gtk_scrolled_window_set_propagate_natural_width (GtkScrolledWindow *scrolled_window,
                                                 gboolean           propagate)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  propagate = !!propagate;

  if (priv->propagate_natural_width != propagate)
    {
      priv->propagate_natural_width = propagate;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_PROPAGATE_NATURAL_WIDTH]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_propagate_natural_width:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Reports whether the natural width of the child will be calculated and propagated
 * through the scrolled window’s requested natural width.
 *
 * Returns: whether natural width propagation is enabled.
 */
gboolean
gtk_scrolled_window_get_propagate_natural_width (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->propagate_natural_width;
}

/**
 * gtk_scrolled_window_set_propagate_natural_height:
 * @scrolled_window: a #GtkScrolledWindow
 * @propagate: whether to propagate natural height
 *
 * Sets whether the natural height of the child should be calculated and propagated
 * through the scrolled window’s requested natural height.
 */
void
gtk_scrolled_window_set_propagate_natural_height (GtkScrolledWindow *scrolled_window,
                                                  gboolean           propagate)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  propagate = !!propagate;

  if (priv->propagate_natural_height != propagate)
    {
      priv->propagate_natural_height = propagate;
      g_object_notify_by_pspec (G_OBJECT (scrolled_window), properties [PROP_PROPAGATE_NATURAL_HEIGHT]);
      gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

/**
 * gtk_scrolled_window_get_propagate_natural_height:
 * @scrolled_window: a #GtkScrolledWindow
 *
 * Reports whether the natural height of the child will be calculated and propagated
 * through the scrolled window’s requested natural height.
 *
 * Returns: whether natural height propagation is enabled.
 */
gboolean
gtk_scrolled_window_get_propagate_natural_height (GtkScrolledWindow *scrolled_window)
{
  GtkScrolledWindowPrivate *priv = gtk_scrolled_window_get_instance_private (scrolled_window);

  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), -1);

  return priv->propagate_natural_height;
}
