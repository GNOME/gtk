/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkrangeprivate.h"

#include "gtkadjustmentprivate.h"
#include "gtkcolorscaleprivate.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkgesturedrag.h"
#include "gtkgesturelongpressprivate.h"
#include "gtkgestureclick.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtktypebuiltins.h"
#include "gtkeventcontrollerkey.h"

#include "a11y/gtkrangeaccessible.h"

#include <stdio.h>
#include <math.h>

/**
 * SECTION:gtkrange
 * @Short_description: Base class for widgets which visualize an adjustment
 * @Title: GtkRange
 *
 * #GtkRange is the common base class for widgets which visualize an
 * adjustment, e.g #GtkScale or #GtkScrollbar.
 *
 * Apart from signals for monitoring the parameters of the adjustment,
 * #GtkRange provides properties and methods for setting a
 * “fill level” on range widgets. See gtk_range_set_fill_level().
 */


#define TIMEOUT_INITIAL     500
#define TIMEOUT_REPEAT      250
#define AUTOSCROLL_FACTOR   20
#define SCROLL_EDGE_SIZE    15
#define MARK_SNAP_LENGTH    12

typedef struct _GtkRangeStepTimer GtkRangeStepTimer;

typedef struct _GtkRangePrivate       GtkRangePrivate;
struct _GtkRangePrivate
{
  GtkWidget *grab_location;   /* "grabbed" mouse location, NULL for no grab */

  GtkRangeStepTimer *timer;

  GtkAdjustment     *adjustment;

  int slider_x;
  int slider_y;

  GtkWidget    *trough_widget;
  GtkWidget    *fill_widget;
  GtkWidget    *highlight_widget;
  GtkWidget    *slider_widget;

  GtkOrientation     orientation;

  gdouble  fill_level;
  gdouble *marks;

  gint *mark_pos;
  gint  n_marks;
  gint  round_digits;                /* Round off value to this many digits, -1 for no rounding */
  gint  slide_initial_slider_position;
  gint  slide_initial_coordinate_delta;

  guint flippable              : 1;
  guint inverted               : 1;
  guint slider_size_fixed      : 1;
  guint trough_click_forward   : 1;  /* trough click was on the forward side of slider */

  /* Whether we're doing fine adjustment */
  guint zoom                   : 1;

  /* Fill level */
  guint show_fill_level        : 1;
  guint restrict_to_fill_level : 1;

  /* Whether dragging is ongoing */
  guint in_drag                : 1;

  GtkGesture *drag_gesture;

  GtkScrollType autoscroll_mode;
  guint autoscroll_id;
};


enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_INVERTED,
  PROP_SHOW_FILL_LEVEL,
  PROP_RESTRICT_TO_FILL_LEVEL,
  PROP_FILL_LEVEL,
  PROP_ROUND_DIGITS,
  PROP_ORIENTATION,
  LAST_PROP = PROP_ORIENTATION
};

enum {
  VALUE_CHANGED,
  ADJUST_BOUNDS,
  MOVE_SLIDER,
  CHANGE_VALUE,
  LAST_SIGNAL
};

static void gtk_range_set_property   (GObject          *object,
                                      guint             prop_id,
                                      const GValue     *value,
                                      GParamSpec       *pspec);
static void gtk_range_get_property   (GObject          *object,
                                      guint             prop_id,
                                      GValue           *value,
                                      GParamSpec       *pspec);
static void gtk_range_finalize       (GObject          *object);
static void gtk_range_destroy        (GtkWidget        *widget);
static void gtk_range_measure        (GtkWidget      *widget,
                                      GtkOrientation  orientation,
                                      int             for_size,
                                      int            *minimum,
                                      int            *natural,
                                      int            *minimum_baseline,
                                      int            *natural_baseline);
static void gtk_range_size_allocate  (GtkWidget      *widget,
                                      int             width,
                                      int             height,
                                      int             baseline);
static void gtk_range_unmap          (GtkWidget        *widget);

static void gtk_range_click_gesture_pressed  (GtkGestureClick *gesture,
                                                   guint                 n_press,
                                                   gdouble               x,
                                                   gdouble               y,
                                                   GtkRange             *range);
static void gtk_range_click_gesture_released (GtkGestureClick *gesture,
                                                   guint                 n_press,
                                                   gdouble               x,
                                                   gdouble               y,
                                                   GtkRange             *range);
static void gtk_range_drag_gesture_begin          (GtkGestureDrag       *gesture,
                                                   gdouble               offset_x,
                                                   gdouble               offset_y,
                                                   GtkRange             *range);
static void gtk_range_drag_gesture_update         (GtkGestureDrag       *gesture,
                                                   gdouble               offset_x,
                                                   gdouble               offset_y,
                                                   GtkRange             *range);
static void gtk_range_long_press_gesture_pressed  (GtkGestureLongPress  *gesture,
                                                   gdouble               x,
                                                   gdouble               y,
                                                   GtkRange             *range);


static void update_slider_position   (GtkRange	       *range,
                                      int               mouse_x,
                                      int               mouse_y);
static void stop_scrolling           (GtkRange         *range);
static void add_autoscroll           (GtkRange         *range);
static void remove_autoscroll        (GtkRange         *range);

/* Range methods */

static void gtk_range_move_slider              (GtkRange         *range,
                                                GtkScrollType     scroll);

/* Internals */
static void          gtk_range_compute_slider_position  (GtkRange      *range,
                                                         gdouble        adjustment_value,
                                                         GdkRectangle  *slider_rect);
static gboolean      gtk_range_scroll                   (GtkRange      *range,
                                                         GtkScrollType  scroll);
static void          gtk_range_calc_slider              (GtkRange      *range);
static void          gtk_range_calc_marks               (GtkRange      *range);
static void          gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_adjustment_changed       (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_add_step_timer           (GtkRange      *range,
                                                         GtkScrollType  step);
static void          gtk_range_remove_step_timer        (GtkRange      *range);
static gboolean      gtk_range_real_change_value        (GtkRange      *range,
                                                         GtkScrollType  scroll,
                                                         gdouble        value);
static gboolean      gtk_range_key_controller_key_pressed (GtkEventControllerKey *controller,
                                                           guint                  keyval,
                                                           guint                  keycode,
                                                           GdkModifierType        state,
                                                           GtkWidget             *widget);
static void          gtk_range_direction_changed        (GtkWidget     *widget,
                                                         GtkTextDirection  previous_direction);
static void          gtk_range_measure_trough           (GtkGizmo       *gizmo,
                                                         GtkOrientation  orientation,
                                                         gint            for_size,
                                                         gint           *minimum,
                                                         gint           *natural,
                                                         gint           *minimum_baseline,
                                                         gint           *natural_baseline);
static void          gtk_range_allocate_trough          (GtkGizmo            *gizmo,
                                                         int                  width,
                                                         int                  height,
                                                         int                  baseline);
static void          gtk_range_render_trough            (GtkGizmo     *gizmo,
                                                         GtkSnapshot  *snapshot);

static gboolean      gtk_range_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                         gdouble                   dx,
                                                         gdouble                   dy,
                                                         GtkRange                 *range);

G_DEFINE_TYPE_WITH_CODE (GtkRange, gtk_range, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkRange)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))

static guint signals[LAST_SIGNAL];
static GParamSpec *properties[LAST_PROP];

static void
gtk_range_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  gtk_widget_snapshot_child (widget, priv->trough_widget, snapshot);
}

static void
gtk_range_class_init (GtkRangeClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_range_set_property;
  gobject_class->get_property = gtk_range_get_property;
  gobject_class->finalize = gtk_range_finalize;

  widget_class->destroy = gtk_range_destroy;
  widget_class->measure = gtk_range_measure;
  widget_class->snapshot = gtk_range_snapshot;
  widget_class->size_allocate = gtk_range_size_allocate;
  widget_class->unmap = gtk_range_unmap;
  widget_class->direction_changed = gtk_range_direction_changed;

  class->move_slider = gtk_range_move_slider;
  class->change_value = gtk_range_real_change_value;

  /**
   * GtkRange::value-changed:
   * @range: the #GtkRange that received the signal
   *
   * Emitted when the range value changes.
   */
  signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, value_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkRange::adjust-bounds:
   * @range: the #GtkRange that received the signal
   * @value: the value before we clamp
   *
   * Emitted before clamping a value, to give the application a
   * chance to adjust the bounds.
   */
  signals[ADJUST_BOUNDS] =
    g_signal_new (I_("adjust-bounds"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, adjust_bounds),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_DOUBLE);

  /**
   * GtkRange::move-slider:
   * @range: the #GtkRange that received the signal
   * @step: how to move the slider
   *
   * Virtual function that moves the slider. Used for keybindings.
   */
  signals[MOVE_SLIDER] =
    g_signal_new (I_("move-slider"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkRangeClass, move_slider),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkRange::change-value:
   * @range: the #GtkRange that received the signal
   * @scroll: the type of scroll action that was performed
   * @value: the new value resulting from the scroll action
   *
   * The #GtkRange::change-value signal is emitted when a scroll action is
   * performed on a range.  It allows an application to determine the
   * type of scroll event that occurred and the resultant new value.
   * The application can handle the event itself and return %TRUE to
   * prevent further processing.  Or, by returning %FALSE, it can pass
   * the event to other handlers until the default GTK+ handler is
   * reached.
   *
   * The value parameter is unrounded.  An application that overrides
   * the GtkRange::change-value signal is responsible for clamping the
   * value to the desired number of decimal digits; the default GTK+
   * handler clamps the value based on #GtkRange:round-digits.
   *
   * Returns: %TRUE to prevent other handlers from being invoked for
   *     the signal, %FALSE to propagate the signal further
   */
  signals[CHANGE_VALUE] =
    g_signal_new (I_("change-value"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, change_value),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_DOUBLE,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_SCROLL_TYPE,
                  G_TYPE_DOUBLE);

  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  properties[PROP_ADJUSTMENT] =
      g_param_spec_object ("adjustment",
                           P_("Adjustment"),
                           P_("The GtkAdjustment that contains the current value of this range object"),
                           GTK_TYPE_ADJUSTMENT,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT);

  properties[PROP_INVERTED] =
      g_param_spec_boolean ("inverted",
                            P_("Inverted"),
                            P_("Invert direction slider moves to increase range value"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRange:show-fill-level:
   *
   * The show-fill-level property controls whether fill level indicator
   * graphics are displayed on the trough. See
   * gtk_range_set_show_fill_level().
   **/
  properties[PROP_SHOW_FILL_LEVEL] =
      g_param_spec_boolean ("show-fill-level",
                            P_("Show Fill Level"),
                            P_("Whether to display a fill level indicator graphics on trough."),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRange:restrict-to-fill-level:
   *
   * The restrict-to-fill-level property controls whether slider
   * movement is restricted to an upper boundary set by the
   * fill level. See gtk_range_set_restrict_to_fill_level().
   **/
  properties[PROP_RESTRICT_TO_FILL_LEVEL] =
      g_param_spec_boolean ("restrict-to-fill-level",
                            P_("Restrict to Fill Level"),
                            P_("Whether to restrict the upper boundary to the fill level."),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRange:fill-level:
   *
   * The fill level (e.g. prebuffering of a network stream).
   * See gtk_range_set_fill_level().
   **/
  properties[PROP_FILL_LEVEL] =
      g_param_spec_double ("fill-level",
                           P_("Fill Level"),
                           P_("The fill level."),
                           -G_MAXDOUBLE, G_MAXDOUBLE,
                           G_MAXDOUBLE,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRange:round-digits:
   *
   * The number of digits to round the value to when
   * it changes, or -1. See #GtkRange::change-value.
   */
  properties[PROP_ROUND_DIGITS] =
      g_param_spec_int ("round-digits",
                        P_("Round Digits"),
                        P_("The number of digits to round the value to."),
                        -1, G_MAXINT,
                        -1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, properties);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_RANGE_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("range"));
}

static void
gtk_range_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkRange *range = GTK_RANGE (object);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      if (priv->orientation != g_value_get_enum (value))
        {
          priv->orientation = g_value_get_enum (value);
          _gtk_orientable_set_style_classes (GTK_ORIENTABLE (range));
          gtk_widget_queue_resize (GTK_WIDGET (range));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_ADJUSTMENT:
      gtk_range_set_adjustment (range, g_value_get_object (value));
      break;
    case PROP_INVERTED:
      gtk_range_set_inverted (range, g_value_get_boolean (value));
      break;
    case PROP_SHOW_FILL_LEVEL:
      gtk_range_set_show_fill_level (range, g_value_get_boolean (value));
      break;
    case PROP_RESTRICT_TO_FILL_LEVEL:
      gtk_range_set_restrict_to_fill_level (range, g_value_get_boolean (value));
      break;
    case PROP_FILL_LEVEL:
      gtk_range_set_fill_level (range, g_value_get_double (value));
      break;
    case PROP_ROUND_DIGITS:
      gtk_range_set_round_digits (range, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_range_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkRange *range = GTK_RANGE (object);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    case PROP_SHOW_FILL_LEVEL:
      g_value_set_boolean (value, gtk_range_get_show_fill_level (range));
      break;
    case PROP_RESTRICT_TO_FILL_LEVEL:
      g_value_set_boolean (value, gtk_range_get_restrict_to_fill_level (range));
      break;
    case PROP_FILL_LEVEL:
      g_value_set_double (value, gtk_range_get_fill_level (range));
      break;
    case PROP_ROUND_DIGITS:
      g_value_set_int (value, gtk_range_get_round_digits (range));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_range_init (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkGesture *gesture;
  GtkEventController *controller;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->adjustment = NULL;
  priv->inverted = FALSE;
  priv->flippable = FALSE;
  priv->round_digits = -1;
  priv->show_fill_level = FALSE;
  priv->restrict_to_fill_level = TRUE;
  priv->fill_level = G_MAXDOUBLE;
  priv->timer = NULL;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (range));

  priv->trough_widget = gtk_gizmo_new ("trough",
                                       gtk_range_measure_trough,
                                       gtk_range_allocate_trough,
                                       gtk_range_render_trough,
                                       NULL);

  gtk_widget_set_parent (priv->trough_widget, GTK_WIDGET (range));

  priv->slider_widget = gtk_gizmo_new ("slider", NULL, NULL, NULL, NULL);
  gtk_widget_set_parent (priv->slider_widget, priv->trough_widget);

  /* Note: Order is important here.
   * The ::drag-begin handler relies on the state set up by the
   * click ::pressed handler. Gestures are handling events
   * in the oppposite order in which they are added to their
   * widget.
   */
  priv->drag_gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->drag_gesture), 0);
  g_signal_connect (priv->drag_gesture, "drag-begin",
                    G_CALLBACK (gtk_range_drag_gesture_begin), range);
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_range_drag_gesture_update), range);
  gtk_widget_add_controller (GTK_WIDGET (range), GTK_EVENT_CONTROLLER (priv->drag_gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_range_click_gesture_pressed), range);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_range_click_gesture_released), range);
  gtk_widget_add_controller (GTK_WIDGET (range), GTK_EVENT_CONTROLLER (gesture));
  gtk_gesture_group (priv->drag_gesture, gesture);

  gesture = gtk_gesture_long_press_new ();
  gtk_gesture_long_press_set_delay_factor (GTK_GESTURE_LONG_PRESS (gesture), 2.0);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_range_long_press_gesture_pressed), range);
  gtk_widget_add_controller (GTK_WIDGET (range), GTK_EVENT_CONTROLLER (gesture));
  gtk_gesture_group (priv->drag_gesture, gesture);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_range_scroll_controller_scroll), range);
  gtk_widget_add_controller (GTK_WIDGET (range), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_range_key_controller_key_pressed), range);
  gtk_widget_add_controller (GTK_WIDGET (range), controller);
}

/**
 * gtk_range_get_adjustment:
 * @range: a #GtkRange
 * 
 * Get the #GtkAdjustment which is the “model” object for #GtkRange.
 * See gtk_range_set_adjustment() for details.
 * The return value does not have a reference added, so should not
 * be unreferenced.
 * 
 * Returns: (transfer none): a #GtkAdjustment
 **/
GtkAdjustment*
gtk_range_get_adjustment (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), NULL);

  if (!priv->adjustment)
    gtk_range_set_adjustment (range, NULL);

  return priv->adjustment;
}

/**
 * gtk_range_set_adjustment:
 * @range: a #GtkRange
 * @adjustment: a #GtkAdjustment
 *
 * Sets the adjustment to be used as the “model” object for this range
 * widget. The adjustment indicates the current range value, the
 * minimum and maximum range values, the step/page increments used
 * for keybindings and scrolling, and the page size. The page size
 * is normally 0 for #GtkScale and nonzero for #GtkScrollbar, and
 * indicates the size of the visible area of the widget being scrolled.
 * The page size affects the size of the scrollbar slider.
 **/
void
gtk_range_set_adjustment (GtkRange      *range,
			  GtkAdjustment *adjustment)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (priv->adjustment != adjustment)
    {
      if (priv->adjustment)
	{
	  g_signal_handlers_disconnect_by_func (priv->adjustment,
						gtk_range_adjustment_changed,
						range);
	  g_signal_handlers_disconnect_by_func (priv->adjustment,
						gtk_range_adjustment_value_changed,
						range);
	  g_object_unref (priv->adjustment);
	}

      priv->adjustment = adjustment;
      g_object_ref_sink (adjustment);
      
      g_signal_connect (adjustment, "changed",
			G_CALLBACK (gtk_range_adjustment_changed),
			range);
      g_signal_connect (adjustment, "value-changed",
			G_CALLBACK (gtk_range_adjustment_value_changed),
			range);

      gtk_range_adjustment_changed (adjustment, range);
      gtk_range_adjustment_value_changed (adjustment, range);
      g_object_notify_by_pspec (G_OBJECT (range), properties[PROP_ADJUSTMENT]);
    }
}

static gboolean
should_invert (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    return
      (priv->inverted && !priv->flippable) ||
      (priv->inverted && priv->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_LTR) ||
      (!priv->inverted && priv->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_RTL);
  else
    return priv->inverted;
}

static gboolean
should_invert_move (GtkRange       *range,
                    GtkOrientation  move_orientation)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  /* If the move is parallel to the range, use general check for inversion */
  if (move_orientation == priv->orientation)
    return should_invert (range);

  /* H scale/V move: Always invert, so down/up always dec/increase the value */
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL && GTK_IS_SCALE (range))
    return TRUE;

  /* V range/H move: Left/right always dec/increase the value */
  return FALSE;
}

static void
update_highlight_position (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkStyleContext *context;

  if (!priv->highlight_widget)
    return;

  context = gtk_widget_get_style_context (priv->highlight_widget);

  if (should_invert (range))
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_TOP);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
    }
  else
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BOTTOM);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);
    }
}

static void
update_fill_position (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkStyleContext *context;

  if (!priv->fill_widget)
    return;

  context = gtk_widget_get_style_context (priv->fill_widget);

  if (should_invert (range))
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_TOP);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
    }
  else
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BOTTOM);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);
    }
}

/**
 * gtk_range_set_inverted:
 * @range: a #GtkRange
 * @setting: %TRUE to invert the range
 *
 * Ranges normally move from lower to higher values as the
 * slider moves from top to bottom or left to right. Inverted
 * ranges have higher values at the top or on the right rather than
 * on the bottom or left.
 **/
void
gtk_range_set_inverted (GtkRange *range,
                        gboolean  setting)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  setting = setting != FALSE;

  if (setting != priv->inverted)
    {
      priv->inverted = setting;

      update_fill_position (range);
      update_highlight_position (range);

      gtk_widget_queue_resize (priv->trough_widget);

      g_object_notify_by_pspec (G_OBJECT (range), properties[PROP_INVERTED]);
    }
}

/**
 * gtk_range_get_inverted:
 * @range: a #GtkRange
 * 
 * Gets the value set by gtk_range_set_inverted().
 * 
 * Returns: %TRUE if the range is inverted
 **/
gboolean
gtk_range_get_inverted (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return priv->inverted;
}

/**
 * gtk_range_set_flippable:
 * @range: a #GtkRange
 * @flippable: %TRUE to make the range flippable
 *
 * If a range is flippable, it will switch its direction if it is
 * horizontal and its direction is %GTK_TEXT_DIR_RTL.
 *
 * See gtk_widget_get_direction().
 **/
void
gtk_range_set_flippable (GtkRange *range,
                         gboolean  flippable)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  flippable = flippable ? TRUE : FALSE;

  if (flippable != priv->flippable)
    {
      priv->flippable = flippable;
      update_fill_position (range);
      update_highlight_position (range);

      gtk_widget_queue_allocate (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_flippable:
 * @range: a #GtkRange
 *
 * Gets the value set by gtk_range_set_flippable().
 *
 * Returns: %TRUE if the range is flippable
 **/
gboolean
gtk_range_get_flippable (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return priv->flippable;
}

/**
 * gtk_range_set_slider_size_fixed:
 * @range: a #GtkRange
 * @size_fixed: %TRUE to make the slider size constant
 *
 * Sets whether the range’s slider has a fixed size, or a size that
 * depends on its adjustment’s page size.
 *
 * This function is useful mainly for #GtkRange subclasses.
 **/
void
gtk_range_set_slider_size_fixed (GtkRange *range,
                                 gboolean  size_fixed)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  if (size_fixed != priv->slider_size_fixed)
    {
      priv->slider_size_fixed = size_fixed ? TRUE : FALSE;

      if (priv->adjustment && gtk_widget_get_mapped (GTK_WIDGET (range)))
        gtk_widget_queue_allocate (priv->trough_widget);
    }
}

/**
 * gtk_range_get_slider_size_fixed:
 * @range: a #GtkRange
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * See gtk_range_set_slider_size_fixed().
 *
 * Returns: whether the range’s slider has a fixed size.
 **/
gboolean
gtk_range_get_slider_size_fixed (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return priv->slider_size_fixed;
}

/**
 * gtk_range_get_range_rect:
 * @range: a #GtkRange
 * @range_rect: (out): return location for the range rectangle
 *
 * This function returns the area that contains the range’s trough,
 * in coordinates relative to @range's origin.
 *
 * This function is useful mainly for #GtkRange subclasses.
 **/
void
gtk_range_get_range_rect (GtkRange     *range,
                          GdkRectangle *range_rect)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  graphene_rect_t r;

  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (range_rect != NULL);

  if (!gtk_widget_compute_bounds (priv->trough_widget, GTK_WIDGET (range), &r))
    {
      *range_rect = (GdkRectangle) { 0, 0, 0, 0 };
    }
  else
    {
      *range_rect = (GdkRectangle) {
        floorf (r.origin.x),
        floorf (r.origin.y),
        ceilf (r.size.width),
        ceilf (r.size.height),
      };
    }
}

/**
 * gtk_range_get_slider_range:
 * @range: a #GtkRange
 * @slider_start: (out) (allow-none): return location for the slider's
 *     start, or %NULL
 * @slider_end: (out) (allow-none): return location for the slider's
 *     end, or %NULL
 *
 * This function returns sliders range along the long dimension,
 * in widget->window coordinates.
 *
 * This function is useful mainly for #GtkRange subclasses.
 **/
void
gtk_range_get_slider_range (GtkRange *range,
                            gint     *slider_start,
                            gint     *slider_end)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  graphene_rect_t slider_bounds;

  g_return_if_fail (GTK_IS_RANGE (range));

  if (!gtk_widget_compute_bounds (priv->slider_widget, GTK_WIDGET (range), &slider_bounds))
    {
      if (slider_start)
        *slider_start = 0;
      if (slider_end)
        *slider_end = 0;
      return;
    }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (slider_start)
        *slider_start = slider_bounds.origin.y;
      if (slider_end)
        *slider_end = slider_bounds.origin.y + slider_bounds.size.height;
    }
  else
    {
      if (slider_start)
        *slider_start = slider_bounds.origin.y;
      if (slider_end)
        *slider_end = slider_bounds.origin.x + slider_bounds.size.width;
    }
}

/**
 * gtk_range_set_increments:
 * @range: a #GtkRange
 * @step: step size
 * @page: page size
 *
 * Sets the step and page sizes for the range.
 * The step size is used when the user clicks the #GtkScrollbar
 * arrows or moves #GtkScale via arrow keys. The page size
 * is used for example when moving via Page Up or Page Down keys.
 **/
void
gtk_range_set_increments (GtkRange *range,
                          gdouble   step,
                          gdouble   page)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkAdjustment *adjustment;

  g_return_if_fail (GTK_IS_RANGE (range));

  adjustment = priv->adjustment;

  gtk_adjustment_configure (adjustment,
                            gtk_adjustment_get_value (adjustment),
                            gtk_adjustment_get_lower (adjustment),
                            gtk_adjustment_get_upper (adjustment),
                            step,
                            page,
                            gtk_adjustment_get_page_size (adjustment));
}

/**
 * gtk_range_set_range:
 * @range: a #GtkRange
 * @min: minimum range value
 * @max: maximum range value
 * 
 * Sets the allowable values in the #GtkRange, and clamps the range
 * value to be between @min and @max. (If the range has a non-zero
 * page size, it is clamped between @min and @max - page-size.)
 **/
void
gtk_range_set_range (GtkRange *range,
                     gdouble   min,
                     gdouble   max)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkAdjustment *adjustment;
  gdouble value;
  
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (min <= max);

  adjustment = priv->adjustment;

  value = gtk_adjustment_get_value (adjustment);
  if (priv->restrict_to_fill_level)
    value = MIN (value, MAX (gtk_adjustment_get_lower (adjustment),
                             priv->fill_level));

  gtk_adjustment_configure (adjustment,
                            value,
                            min,
                            max,
                            gtk_adjustment_get_step_increment (adjustment),
                            gtk_adjustment_get_page_increment (adjustment),
                            gtk_adjustment_get_page_size (adjustment));
}

/**
 * gtk_range_set_value:
 * @range: a #GtkRange
 * @value: new value of the range
 *
 * Sets the current value of the range; if the value is outside the
 * minimum or maximum range values, it will be clamped to fit inside
 * them. The range emits the #GtkRange::value-changed signal if the 
 * value changes.
 **/
void
gtk_range_set_value (GtkRange *range,
                     gdouble   value)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  if (priv->restrict_to_fill_level)
    value = MIN (value, MAX (gtk_adjustment_get_lower (priv->adjustment),
                             priv->fill_level));

  gtk_adjustment_set_value (priv->adjustment, value);
}

/**
 * gtk_range_get_value:
 * @range: a #GtkRange
 * 
 * Gets the current value of the range.
 * 
 * Returns: current value of the range.
 **/
gdouble
gtk_range_get_value (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return gtk_adjustment_get_value (priv->adjustment);
}

/**
 * gtk_range_set_show_fill_level:
 * @range:           A #GtkRange
 * @show_fill_level: Whether a fill level indicator graphics is shown.
 *
 * Sets whether a graphical fill level is show on the trough. See
 * gtk_range_set_fill_level() for a general description of the fill
 * level concept.
 **/
void
gtk_range_set_show_fill_level (GtkRange *range,
                               gboolean  show_fill_level)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  show_fill_level = show_fill_level ? TRUE : FALSE;

  if (show_fill_level == priv->show_fill_level)
    return;

  priv->show_fill_level = show_fill_level;

  if (show_fill_level)
    {
      priv->fill_widget = gtk_gizmo_new ("fill", NULL, NULL, NULL, NULL);
      gtk_widget_insert_after (priv->fill_widget, priv->trough_widget, NULL);
      update_fill_position (range);
    }
  else
    {
      g_clear_pointer (&priv->fill_widget, gtk_widget_unparent);
    }

  g_object_notify_by_pspec (G_OBJECT (range), properties[PROP_SHOW_FILL_LEVEL]);
  gtk_widget_queue_allocate (GTK_WIDGET (range));
}

/**
 * gtk_range_get_show_fill_level:
 * @range: A #GtkRange
 *
 * Gets whether the range displays the fill level graphically.
 *
 * Returns: %TRUE if @range shows the fill level.
 **/
gboolean
gtk_range_get_show_fill_level (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return priv->show_fill_level;
}

/**
 * gtk_range_set_restrict_to_fill_level:
 * @range:                  A #GtkRange
 * @restrict_to_fill_level: Whether the fill level restricts slider movement.
 *
 * Sets whether the slider is restricted to the fill level. See
 * gtk_range_set_fill_level() for a general description of the fill
 * level concept.
 **/
void
gtk_range_set_restrict_to_fill_level (GtkRange *range,
                                      gboolean  restrict_to_fill_level)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  restrict_to_fill_level = restrict_to_fill_level ? TRUE : FALSE;

  if (restrict_to_fill_level != priv->restrict_to_fill_level)
    {
      priv->restrict_to_fill_level = restrict_to_fill_level;
      g_object_notify_by_pspec (G_OBJECT (range), properties[PROP_RESTRICT_TO_FILL_LEVEL]);

      gtk_range_set_value (range, gtk_range_get_value (range));
    }
}

/**
 * gtk_range_get_restrict_to_fill_level:
 * @range: A #GtkRange
 *
 * Gets whether the range is restricted to the fill level.
 *
 * Returns: %TRUE if @range is restricted to the fill level.
 **/
gboolean
gtk_range_get_restrict_to_fill_level (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return priv->restrict_to_fill_level;
}

/**
 * gtk_range_set_fill_level:
 * @range: a #GtkRange
 * @fill_level: the new position of the fill level indicator
 *
 * Set the new position of the fill level indicator.
 *
 * The “fill level” is probably best described by its most prominent
 * use case, which is an indicator for the amount of pre-buffering in
 * a streaming media player. In that use case, the value of the range
 * would indicate the current play position, and the fill level would
 * be the position up to which the file/stream has been downloaded.
 *
 * This amount of prebuffering can be displayed on the range’s trough
 * and is themeable separately from the trough. To enable fill level
 * display, use gtk_range_set_show_fill_level(). The range defaults
 * to not showing the fill level.
 *
 * Additionally, it’s possible to restrict the range’s slider position
 * to values which are smaller than the fill level. This is controller
 * by gtk_range_set_restrict_to_fill_level() and is by default
 * enabled.
 **/
void
gtk_range_set_fill_level (GtkRange *range,
                          gdouble   fill_level)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));

  if (fill_level != priv->fill_level)
    {
      priv->fill_level = fill_level;
      g_object_notify_by_pspec (G_OBJECT (range), properties[PROP_FILL_LEVEL]);

      if (priv->show_fill_level)
        gtk_widget_queue_allocate (GTK_WIDGET (range));

      if (priv->restrict_to_fill_level)
        gtk_range_set_value (range, gtk_range_get_value (range));
    }
}

/**
 * gtk_range_get_fill_level:
 * @range: A #GtkRange
 *
 * Gets the current position of the fill level indicator.
 *
 * Returns: The current fill level
 **/
gdouble
gtk_range_get_fill_level (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return priv->fill_level;
}

static void
gtk_range_destroy (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  gtk_range_remove_step_timer (range);

  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
					    gtk_range_adjustment_changed,
					    range);
      g_signal_handlers_disconnect_by_func (priv->adjustment,
					    gtk_range_adjustment_value_changed,
					    range);
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }

  if (priv->n_marks)
    {
      g_free (priv->marks);
      priv->marks = NULL;
      g_free (priv->mark_pos);
      priv->mark_pos = NULL;
      priv->n_marks = 0;
    }

  GTK_WIDGET_CLASS (gtk_range_parent_class)->destroy (widget);
}

static void
gtk_range_finalize (GObject *object)
{
  GtkRange *range = GTK_RANGE (object);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_clear_pointer (&priv->slider_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->fill_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->highlight_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->trough_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_range_parent_class)->finalize (object);
}

static void
gtk_range_measure_trough (GtkGizmo       *gizmo,
                          GtkOrientation  orientation,
                          gint            for_size,
                          gint           *minimum,
                          gint           *natural,
                          gint           *minimum_baseline,
                          gint           *natural_baseline)
{
  GtkWidget *widget = gtk_widget_get_parent (GTK_WIDGET (gizmo));
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gint min, nat;

  gtk_widget_measure (priv->slider_widget,
                      orientation, -1,
                      minimum, natural,
                      NULL, NULL);

  if (priv->fill_widget)
    {
      gtk_widget_measure (priv->fill_widget,
                          orientation, for_size,
                          &min, &nat,
                          NULL, NULL);
      *minimum = MAX (*minimum, min);
      *natural = MAX (*natural, nat);
    }

  if (priv->highlight_widget)
    {
      gtk_widget_measure (priv->highlight_widget,
                          orientation, for_size,
                          &min, &nat,
                          NULL, NULL);
      *minimum = MAX (*minimum, min);
      *natural = MAX (*natural, nat);
    }
}

static void
gtk_range_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkBorder border = { 0 };

  /* Measure the main box */
  gtk_widget_measure (priv->trough_widget,
                      orientation,
                      -1,
                      minimum, natural,
                      NULL, NULL);

  if (GTK_RANGE_GET_CLASS (range)->get_range_border)
    GTK_RANGE_GET_CLASS (range)->get_range_border (range, &border);

  /* Add the border */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum += border.left + border.right;
      *natural += border.left + border.right;
    }
  else
    {
      *minimum += border.top + border.bottom;
      *natural += border.top + border.bottom;
    }
}

static void
gtk_range_allocate_trough (GtkGizmo *gizmo,
                           int       width,
                           int       height,
                           int       baseline)
{
  GtkWidget *widget = gtk_widget_get_parent (GTK_WIDGET (gizmo));
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkAllocation slider_alloc;
  const double lower = gtk_adjustment_get_lower (priv->adjustment);
  const double upper = gtk_adjustment_get_upper (priv->adjustment);
  const double page_size = gtk_adjustment_get_page_size (priv->adjustment);
  double value;

  /* Slider */
  gtk_range_calc_marks (range);

  gtk_range_compute_slider_position (range,
                                     gtk_adjustment_get_value (priv->adjustment),
                                     &slider_alloc);

  gtk_widget_size_allocate (priv->slider_widget, &slider_alloc, -1);
  priv->slider_x = slider_alloc.x;
  priv->slider_y = slider_alloc.y;

  if (lower == upper)
    value = 0;
  else
    value = (gtk_adjustment_get_value (priv->adjustment) - lower) / (upper - lower);

  if (priv->show_fill_level &&
      upper - page_size - lower != 0)
    {
      double level, fill;
      GtkAllocation fill_alloc;

      fill_alloc = (GtkAllocation) {0, 0, width, height};

      level = CLAMP (priv->fill_level, lower, upper - page_size);

      fill = (level - lower) / (upper - lower - page_size);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          fill_alloc.width *= fill;

          if (should_invert (range))
            fill_alloc.x += width - fill_alloc.width;
        }
      else
        {
          fill_alloc.height *= fill;

          if (should_invert (range))
            fill_alloc.y += height - fill_alloc.height;
        }

      gtk_widget_size_allocate (priv->fill_widget, &fill_alloc, -1);
    }

  if (priv->highlight_widget)
    {
      GtkAllocation highlight_alloc;
      int min, nat;

      gtk_widget_measure (priv->highlight_widget,
                          priv->orientation, -1,
                          &min, &nat,
                          NULL, NULL);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          highlight_alloc.y = 0;
          highlight_alloc.height = height;

          if (!should_invert (range))
            highlight_alloc.x = 0;
          else
            highlight_alloc.x = (width * (1 - value));

          highlight_alloc.width = MAX (min, width * value);
        }
      else
        {
          highlight_alloc.x = 0;
          highlight_alloc.width = width;

          if (!should_invert (range))
            highlight_alloc.y = 0;
          else
            highlight_alloc.y = (height * (1 - value));

          highlight_alloc.height = MAX (min, height * value);
        }

      gtk_widget_size_allocate (priv->highlight_widget, &highlight_alloc, -1);
    }
}

/* Clamp dimensions and border inside allocation, such that we prefer
 * to take space from border not dimensions in all directions, and prefer to
 * give space to border over dimensions in one direction.
 */
static void
clamp_dimensions (int        range_width,
                  int        range_height,
                  int       *width,
                  int       *height,
                  GtkBorder *border,
                  gboolean   border_expands_horizontally)
{
  gint extra, shortage;

  /* Width */
  extra = range_width - border->left - border->right - *width;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          border->left += extra / 2;
          border->right += extra / 2 + extra % 2;
        }
      else
        {
          *width += extra;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = *width - range_width;
  if (shortage > 0)
    {
      *width = range_width;
      /* lose the border */
      border->left = 0;
      border->right = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = *width + border->left + border->right - range_width;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->left -= shortage / 2;
          border->right -= shortage / 2 + shortage % 2;
        }
    }

  /* Height */
  extra = range_height - border->top - border->bottom - *height;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          /* don't expand border vertically */
          *height += extra;
        }
      else
        {
          border->top += extra / 2;
          border->bottom += extra / 2 + extra % 2;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = *height - range_height;
  if (shortage > 0)
    {
      *height = range_height;
      /* lose the border */
      border->top = 0;
      border->bottom = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = *height + border->top + border->bottom - range_height;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->top -= shortage / 2;
          border->bottom -= shortage / 2 + shortage % 2;
        }
    }
}

static void
gtk_range_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkBorder border = { 0 };
  GtkAllocation box_alloc;
  int box_min_width, box_min_height;

  if (GTK_RANGE_GET_CLASS (range)->get_range_border)
    GTK_RANGE_GET_CLASS (range)->get_range_border (range, &border);

  gtk_widget_measure (priv->trough_widget,
                      GTK_ORIENTATION_HORIZONTAL, -1,
                      &box_min_width, NULL,
                      NULL, NULL);
  gtk_widget_measure (priv->trough_widget,
                      GTK_ORIENTATION_VERTICAL, -1,
                      &box_min_height, NULL,
                      NULL, NULL);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    clamp_dimensions (width, height, &box_min_width, &box_min_height, &border, TRUE);
  else
    clamp_dimensions (width, height, &box_min_width, &box_min_height, &border, FALSE);

  box_alloc.x = border.left;
  box_alloc.y = border.top;
  box_alloc.width = box_min_width;
  box_alloc.height = box_min_height;

  gtk_widget_size_allocate (priv->trough_widget, &box_alloc, -1);
}

static void
gtk_range_unmap (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);

  stop_scrolling (range);

  GTK_WIDGET_CLASS (gtk_range_parent_class)->unmap (widget);
}

static void
gtk_range_direction_changed (GtkWidget        *widget,
                             GtkTextDirection  previous_direction)
{
  GtkRange *range = GTK_RANGE (widget);

  update_fill_position (range);
  update_highlight_position (range);

  GTK_WIDGET_CLASS (gtk_range_parent_class)->direction_changed (widget, previous_direction);
}

static void
gtk_range_render_trough (GtkGizmo    *gizmo,
                         GtkSnapshot *snapshot)
{
  GtkWidget *widget = gtk_widget_get_parent (GTK_WIDGET (gizmo));
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  /* HACK: GtkColorScale wants to draw its own trough
   * so we let it...
   */
  if (GTK_IS_COLOR_SCALE (widget))
    gtk_color_scale_snapshot_trough (GTK_COLOR_SCALE (widget), snapshot,
                                     gtk_widget_get_width (GTK_WIDGET (gizmo)),
                                     gtk_widget_get_height (GTK_WIDGET (gizmo)));

  if (priv->show_fill_level &&
      gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment) -
      gtk_adjustment_get_lower (priv->adjustment) != 0)
    gtk_widget_snapshot_child (GTK_WIDGET (gizmo), priv->fill_widget, snapshot);

  if (priv->highlight_widget)
    gtk_widget_snapshot_child (GTK_WIDGET (gizmo), priv->highlight_widget, snapshot);

  gtk_widget_snapshot_child (GTK_WIDGET (gizmo), priv->slider_widget, snapshot);
}

static void
range_grab_add (GtkRange  *range,
                GtkWidget *location)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (range));

  /* Don't perform any GDK/GTK+ grab here. Since a button
   * is down, there's an ongoing implicit grab on
   * the widget, which pretty much guarantees this
   * is the only widget receiving the pointer events.
   */
  priv->grab_location = location;

  gtk_style_context_add_class (context, "dragging");
}

static void
update_zoom_state (GtkRange *range,
                   gboolean  enabled)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (range));

  if (enabled)
    gtk_style_context_add_class (context, "fine-tune");
  else
    gtk_style_context_remove_class (context, "fine-tune");

  priv->zoom = enabled;
}

static void
range_grab_remove (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkStyleContext *context;

  if (!priv->grab_location)
    return;

  context = gtk_widget_get_style_context (GTK_WIDGET (range));

  priv->grab_location = NULL;

  update_zoom_state (range, FALSE);

  gtk_style_context_remove_class (context, "dragging");
}

static GtkScrollType
range_get_scroll_for_grab (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (!priv->grab_location)
    return GTK_SCROLL_NONE;

  /* In the trough */
  if (priv->grab_location == priv->trough_widget)
    {
      if (priv->trough_click_forward)
        return GTK_SCROLL_PAGE_FORWARD;
      else
        return GTK_SCROLL_PAGE_BACKWARD;
    }

  return GTK_SCROLL_NONE;
}

static gdouble
coord_to_value (GtkRange *range,
                gdouble   coord)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    slider_length;
  graphene_rect_t slider_bounds;

  if (!gtk_widget_compute_bounds (priv->slider_widget, priv->slider_widget, &slider_bounds))
    graphene_rect_init (&slider_bounds, 0, 0, gtk_widget_get_width (priv->trough_widget), gtk_widget_get_height (priv->trough_widget));

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      trough_length = gtk_widget_get_width (priv->trough_widget);
      slider_length = slider_bounds.size.width;
    }
  else
    {
      trough_length = gtk_widget_get_height (priv->trough_widget);
      slider_length = slider_bounds.size.height;
    }

  if (trough_length == slider_length)
    {
      frac = 1.0;
    }
  else
    {
      if (priv->slider_size_fixed)
        frac = MAX (0, coord) / (double) (trough_length);
      else
        frac = MAX (0, coord) / (double) (trough_length - slider_length);
    }

  if (should_invert (range))
    frac = 1.0 - frac;

  value = gtk_adjustment_get_lower (priv->adjustment) +
          frac * (gtk_adjustment_get_upper (priv->adjustment) -
                  gtk_adjustment_get_lower (priv->adjustment) -
                  gtk_adjustment_get_page_size (priv->adjustment));
  return value;
}

static gboolean
gtk_range_key_controller_key_pressed (GtkEventControllerKey *controller,
                                      guint                  keyval,
                                      guint                  keycode,
                                      GdkModifierType        state,
                                      GtkWidget             *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (gtk_gesture_is_active (priv->drag_gesture) &&
      keyval == GDK_KEY_Escape &&
      priv->grab_location != NULL)
    {
      stop_scrolling (range);

      return GDK_EVENT_STOP;
    }
  else if (priv->in_drag &&
           (keyval == GDK_KEY_Shift_L ||
            keyval == GDK_KEY_Shift_R))
    {
      graphene_rect_t slider_bounds;

      if (!gtk_widget_compute_bounds (priv->slider_widget, priv->trough_widget, &slider_bounds))
        return GDK_EVENT_STOP;

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        priv->slide_initial_slider_position = slider_bounds.origin.y;
      else
        priv->slide_initial_slider_position = slider_bounds.origin.x;
      update_zoom_state (range, !priv->zoom);

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
update_initial_slider_position (GtkRange *range,
                                double    x,
                                double    y)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  int trough_x, trough_y;

  gtk_widget_translate_coordinates (GTK_WIDGET (range), priv->trough_widget,
                                    x, y, &trough_x, &trough_y);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      priv->slide_initial_slider_position = MAX (0, priv->slider_x);
      priv->slide_initial_coordinate_delta = trough_x - priv->slide_initial_slider_position;
    }
  else
    {
      priv->slide_initial_slider_position = MAX (0, priv->slider_y);
      priv->slide_initial_coordinate_delta = trough_y - priv->slide_initial_slider_position;
    }
}

static void
gtk_range_long_press_gesture_pressed (GtkGestureLongPress *gesture,
                                      gdouble              x,
                                      gdouble              y,
                                      GtkRange            *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkWidget *mouse_location;

  mouse_location = gtk_widget_pick (GTK_WIDGET (range), x, y, GTK_PICK_DEFAULT);

  if (mouse_location == priv->slider_widget && !priv->zoom)
    {
      update_initial_slider_position (range, x, y);
      update_zoom_state (range, TRUE);
    }
}

static void
gtk_range_click_gesture_pressed (GtkGestureClick *gesture,
                                 guint            n_press,
                                 gdouble          x,
                                 gdouble          y,
                                 GtkRange        *range)
{
  GtkWidget *widget = GTK_WIDGET (range);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GdkDevice *source_device;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GdkInputSource source;
  gboolean primary_warps;
  gboolean shift_pressed;
  guint button;
  GdkModifierType state_mask;
  GtkWidget *mouse_location;

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  gdk_event_get_state (event, &state_mask);
  shift_pressed = (state_mask & GDK_SHIFT_MASK) != 0;

  source_device = gdk_event_get_source_device ((GdkEvent *) event);
  source = gdk_device_get_source (source_device);

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-primary-button-warps-slider", &primary_warps,
                NULL);

  mouse_location = gtk_widget_pick (widget, x, y, 0);

  /* For the purposes of this function, we ignore fill and highlight and
   * handle them like the trough */
  if (mouse_location == priv->fill_widget ||
      mouse_location == priv->highlight_widget)
    mouse_location = priv->trough_widget;

  if (mouse_location == priv->slider_widget &&
      gdk_event_triggers_context_menu (event))
    {
      gboolean handled;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      g_signal_emit_by_name (widget, "popup-menu", &handled);
      return;
    }

  if (mouse_location == priv->slider_widget)
    {
      /* Shift-click in the slider = fine adjustment */
      if (shift_pressed)
        update_zoom_state (range, TRUE);

      update_initial_slider_position (range, x, y);
      range_grab_add (range, priv->slider_widget);
    }
  else if (mouse_location == priv->trough_widget &&
           (source == GDK_SOURCE_TOUCHSCREEN ||
            (primary_warps && !shift_pressed && button == GDK_BUTTON_PRIMARY) ||
            (!primary_warps && shift_pressed && button == GDK_BUTTON_PRIMARY) ||
            (!primary_warps && button == GDK_BUTTON_MIDDLE)))
    {
      int slider_range_x, slider_range_y;
      graphene_rect_t slider_bounds;

      gtk_widget_translate_coordinates (priv->trough_widget, widget,
                                        priv->slider_x, priv->slider_y,
                                        &slider_range_x, &slider_range_y);

      /* If we aren't fixed, center on the slider. I.e. if this is not a scale... */
      if (!priv->slider_size_fixed &&
          gtk_widget_compute_bounds (priv->slider_widget, priv->slider_widget, &slider_bounds))
        {
          slider_range_x += (slider_bounds.size.width  / 2);
          slider_range_y += (slider_bounds.size.height / 2);
        }

      update_initial_slider_position (range, slider_range_x, slider_range_y);

      range_grab_add (range, priv->slider_widget);

      update_slider_position (range, x, y);
    }
  else if (mouse_location == priv->trough_widget &&
           ((primary_warps && shift_pressed && button == GDK_BUTTON_PRIMARY) ||
            (!primary_warps && !shift_pressed && button == GDK_BUTTON_PRIMARY) ||
            (primary_warps && button == GDK_BUTTON_MIDDLE)))
    {
      /* jump by pages */
      GtkScrollType scroll;
      gdouble click_value;

      click_value = coord_to_value (range,
                                    priv->orientation == GTK_ORIENTATION_VERTICAL ?
                                    y : x);

      priv->trough_click_forward = click_value > gtk_adjustment_get_value (priv->adjustment);
      range_grab_add (range, priv->trough_widget);

      scroll = range_get_scroll_for_grab (range);
      gtk_range_add_step_timer (range, scroll);
    }
  else if (mouse_location == priv->trough_widget &&
           button == GDK_BUTTON_SECONDARY)
    {
      /* autoscroll */
      gdouble click_value;

      click_value = coord_to_value (range,
                                    priv->orientation == GTK_ORIENTATION_VERTICAL ?
                                    y : x);

      priv->trough_click_forward = click_value > gtk_adjustment_get_value (priv->adjustment);
      range_grab_add (range, priv->trough_widget);

      remove_autoscroll (range);
      priv->autoscroll_mode = priv->trough_click_forward ? GTK_SCROLL_END : GTK_SCROLL_START;
      add_autoscroll (range);
    }

  if (priv->grab_location == priv->slider_widget);
    /* leave it to ::drag-begin to claim the sequence */
  else if (priv->grab_location != NULL)
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_range_click_gesture_released (GtkGestureClick *gesture,
                                  guint            n_press,
                                  gdouble          x,
                                  gdouble          y,
                                  GtkRange        *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  priv->in_drag = FALSE;
  stop_scrolling (range);
}

/* During a slide, move the slider as required given new mouse position */
static void
update_slider_position (GtkRange *range,
                        gint      mouse_x,
                        gint      mouse_y)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  graphene_rect_t trough_bounds;
  gdouble delta;
  gdouble c;
  gdouble new_value;
  gboolean handled;
  gdouble next_value;
  gdouble mark_value;
  gdouble mark_delta;
  gdouble zoom;
  gint i;

  gtk_widget_translate_coordinates (GTK_WIDGET (range), priv->trough_widget,
                                    mouse_x, mouse_y, &mouse_x, &mouse_y);

  if (priv->zoom &&
      gtk_widget_compute_bounds (priv->trough_widget, priv->trough_widget, &trough_bounds))
    {
      zoom = MIN(1.0, (priv->orientation == GTK_ORIENTATION_VERTICAL ?
                       trough_bounds.size.height : trough_bounds.size.width) /
                       (gtk_adjustment_get_upper (priv->adjustment) -
                        gtk_adjustment_get_lower (priv->adjustment) -
                        gtk_adjustment_get_page_size (priv->adjustment)));

      /* the above is ineffective for scales, so just set a zoom factor */
      if (zoom == 1.0)
        zoom = 0.25;
    }
  else
    zoom = 1.0;

  /* recalculate the initial position from the current position */
  if (priv->slide_initial_slider_position == -1)
    {
      graphene_rect_t slider_bounds;
      double zoom_divisor;

      if (!gtk_widget_compute_bounds (priv->slider_widget, GTK_WIDGET (range), &slider_bounds))
        graphene_rect_init (&slider_bounds, 0, 0, 0, 0);

      if (zoom == 1.0)
        zoom_divisor = 1.0;
      else
        zoom_divisor = zoom - 1.0;

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        priv->slide_initial_slider_position = (zoom * (mouse_y - priv->slide_initial_coordinate_delta) - slider_bounds.origin.y) / zoom_divisor;
      else
        priv->slide_initial_slider_position = (zoom * (mouse_x - priv->slide_initial_coordinate_delta) - slider_bounds.origin.x) / zoom_divisor;
    }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - (priv->slide_initial_coordinate_delta + priv->slide_initial_slider_position);
  else
    delta = mouse_x - (priv->slide_initial_coordinate_delta + priv->slide_initial_slider_position);

  c = priv->slide_initial_slider_position + zoom * delta;

  new_value = coord_to_value (range, c);
  next_value = coord_to_value (range, c + 1);
  mark_delta = fabs (next_value - new_value);

  for (i = 0; i < priv->n_marks; i++)
    {
      mark_value = priv->marks[i];

      if (fabs (gtk_adjustment_get_value (priv->adjustment) - mark_value) < 3 * mark_delta)
        {
          if (fabs (new_value - mark_value) < MARK_SNAP_LENGTH * mark_delta)
            {
              new_value = mark_value;
              break;
            }
        }
    }

  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_JUMP, new_value, &handled);
}

static void
remove_autoscroll (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (priv->autoscroll_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (range),
                                       priv->autoscroll_id);
      priv->autoscroll_id = 0;
    }

  /* unset initial position so it can be calculated */
  priv->slide_initial_slider_position = -1;

  priv->autoscroll_mode = GTK_SCROLL_NONE;
}

static gboolean
autoscroll_cb (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       data)
{
  GtkRange *range = GTK_RANGE (data);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkAdjustment *adj = priv->adjustment;
  gdouble increment;
  gdouble value;
  gboolean handled;
  gdouble step, page;

  step = gtk_adjustment_get_step_increment (adj);
  page = gtk_adjustment_get_page_increment (adj);

  switch ((guint) priv->autoscroll_mode)
    {
    case GTK_SCROLL_STEP_FORWARD:
      increment = step / AUTOSCROLL_FACTOR;
      break;
    case GTK_SCROLL_PAGE_FORWARD:
      increment = page / AUTOSCROLL_FACTOR;
      break;
    case GTK_SCROLL_STEP_BACKWARD:
      increment = - step / AUTOSCROLL_FACTOR;
      break;
    case GTK_SCROLL_PAGE_BACKWARD:
      increment = - page / AUTOSCROLL_FACTOR;
      break;
    case GTK_SCROLL_START:
    case GTK_SCROLL_END:
      {
        gdouble x, y;
        gdouble distance, t;

        /* Vary scrolling speed from slow (ie step) to fast (2 * page),
         * based on the distance of the pointer from the widget. We start
         * speeding up if the pointer moves at least 20 pixels away, and
         * we reach maximum speed when it is 220 pixels away.
         */
        if (!gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (priv->drag_gesture), &x, &y))
          {
            x = 0.0;
            y = 0.0;
          }
        if (gtk_orientable_get_orientation (GTK_ORIENTABLE (range)) == GTK_ORIENTATION_HORIZONTAL)
          distance = fabs (y);
        else
          distance = fabs (x);
        distance = CLAMP (distance - 20, 0.0, 200);
        t = distance / 100.0;
        step = (1 - t) * step + t * page;
        if (priv->autoscroll_mode == GTK_SCROLL_END)
          increment = step / AUTOSCROLL_FACTOR;
        else
          increment = - step / AUTOSCROLL_FACTOR;
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  value = gtk_adjustment_get_value (adj);
  value += increment;

  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_JUMP, value, &handled);

  return G_SOURCE_CONTINUE;
}

static void
add_autoscroll (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (priv->autoscroll_id != 0 ||
      priv->autoscroll_mode == GTK_SCROLL_NONE)
    return;

  priv->autoscroll_id = gtk_widget_add_tick_callback (GTK_WIDGET (range),
                                                      autoscroll_cb, range, NULL);
}

static void
stop_scrolling (GtkRange *range)
{
  range_grab_remove (range);
  gtk_range_remove_step_timer (range);
  remove_autoscroll (range);
}

static gboolean
gtk_range_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                    gdouble                   dx,
                                    gdouble                   dy,
                                    GtkRange                 *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble scroll_unit, delta;
  gboolean handled;
  GtkOrientation move_orientation;

#ifdef GDK_WINDOWING_QUARTZ
  scroll_unit = 1;
#else
  scroll_unit = gtk_adjustment_get_page_increment (priv->adjustment);
#endif

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL && dx != 0)
    {
      move_orientation = GTK_ORIENTATION_HORIZONTAL;
      delta = dx * scroll_unit;
    }
  else
    {
      move_orientation = GTK_ORIENTATION_VERTICAL;
      delta = dy * scroll_unit;
    }

  if (delta != 0 && should_invert_move (range, move_orientation))
    delta = - delta;

  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_JUMP, gtk_adjustment_get_value (priv->adjustment) + delta,
                 &handled);

  return GDK_EVENT_STOP;
}

static void
update_autoscroll_mode (GtkRange *range,
                        int       mouse_x,
                        int       mouse_y)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GtkScrollType mode = GTK_SCROLL_NONE;

  if (priv->zoom)
    {
      int width, height;
      gint size, pos;

      width = gtk_widget_get_width (GTK_WIDGET (range));
      height = gtk_widget_get_height (GTK_WIDGET (range));

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          size = height;
          pos = mouse_y;
        }
      else
        {
          size = width;
          pos = mouse_x;
        }

      if (pos < SCROLL_EDGE_SIZE)
        mode = priv->inverted ? GTK_SCROLL_STEP_FORWARD : GTK_SCROLL_STEP_BACKWARD;
      else if (pos > (size - SCROLL_EDGE_SIZE))
        mode = priv->inverted ? GTK_SCROLL_STEP_BACKWARD : GTK_SCROLL_STEP_FORWARD;
    }

  if (mode != priv->autoscroll_mode)
    {
      remove_autoscroll (range);
      priv->autoscroll_mode = mode;
      add_autoscroll (range);
    }
}

static void
gtk_range_drag_gesture_update (GtkGestureDrag *gesture,
                               gdouble         offset_x,
                               gdouble         offset_y,
                               GtkRange       *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble start_x, start_y;

  if (priv->grab_location == priv->slider_widget)
    {
      int mouse_x, mouse_y;

      gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
      mouse_x = start_x + offset_x;
      mouse_y = start_y + offset_y;
      priv->in_drag = TRUE;
      update_autoscroll_mode (range, mouse_x, mouse_y);

      if (priv->autoscroll_mode == GTK_SCROLL_NONE)
        update_slider_position (range, mouse_x, mouse_y);
    }
}

static void
gtk_range_drag_gesture_begin (GtkGestureDrag *gesture,
                              gdouble         offset_x,
                              gdouble         offset_y,
                              GtkRange       *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (priv->grab_location == priv->slider_widget)
    gtk_gesture_set_state (priv->drag_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_range_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GtkRange *range = GTK_RANGE (data);

  gtk_range_calc_slider (range);

  /* Note that we don't round off to priv->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */
}

static void
gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GtkRange *range = GTK_RANGE (data);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  /* Note that we don't round off to priv->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */

  g_signal_emit (range, signals[VALUE_CHANGED], 0);

  gtk_widget_queue_allocate (priv->trough_widget);
}

static void
apply_marks (GtkRange *range, 
             gdouble   oldval,
             gdouble  *newval)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gint i;
  gdouble mark;

  for (i = 0; i < priv->n_marks; i++)
    {
      mark = priv->marks[i];
      if ((oldval < mark && mark < *newval) ||
          (oldval > mark && mark > *newval))
        {
          *newval = mark;
          return;
        }
    }
}

static void
step_back (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble newval;
  gboolean handled;

  newval = gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_step_increment (priv->adjustment);
  apply_marks (range, gtk_adjustment_get_value (priv->adjustment), &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_STEP_BACKWARD, newval, &handled);
}

static void
step_forward (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble newval;
  gboolean handled;

  newval = gtk_adjustment_get_value (priv->adjustment) + gtk_adjustment_get_step_increment (priv->adjustment);
  apply_marks (range, gtk_adjustment_get_value (priv->adjustment), &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_STEP_FORWARD, newval, &handled);
}


static void
page_back (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble newval;
  gboolean handled;

  newval = gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_page_increment (priv->adjustment);
  apply_marks (range, gtk_adjustment_get_value (priv->adjustment), &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_PAGE_BACKWARD, newval, &handled);
}

static void
page_forward (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble newval;
  gboolean handled;

  newval = gtk_adjustment_get_value (priv->adjustment) + gtk_adjustment_get_page_increment (priv->adjustment);
  apply_marks (range, gtk_adjustment_get_value (priv->adjustment), &newval);
  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_PAGE_FORWARD, newval, &handled);
}

static void
scroll_begin (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gboolean handled;

  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_START, gtk_adjustment_get_lower (priv->adjustment),
                 &handled);
}

static void
scroll_end (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble newval;
  gboolean handled;

  newval = gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment);
  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_END, newval,
                 &handled);
}

static gboolean
gtk_range_scroll (GtkRange     *range,
                  GtkScrollType scroll)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gdouble old_value = gtk_adjustment_get_value (priv->adjustment);

  switch (scroll)
    {
    case GTK_SCROLL_STEP_LEFT:
      if (should_invert_move (range, GTK_ORIENTATION_HORIZONTAL))
        step_forward (range);
      else
        step_back (range);
      break;
                    
    case GTK_SCROLL_STEP_UP:
      if (should_invert_move (range, GTK_ORIENTATION_VERTICAL))
        step_forward (range);
      else
        step_back (range);
      break;

    case GTK_SCROLL_STEP_RIGHT:
      if (should_invert_move (range, GTK_ORIENTATION_HORIZONTAL))
        step_back (range);
      else
        step_forward (range);
      break;
                    
    case GTK_SCROLL_STEP_DOWN:
      if (should_invert_move (range, GTK_ORIENTATION_VERTICAL))
        step_back (range);
      else
        step_forward (range);
      break;
                  
    case GTK_SCROLL_STEP_BACKWARD:
      step_back (range);
      break;
                  
    case GTK_SCROLL_STEP_FORWARD:
      step_forward (range);
      break;

    case GTK_SCROLL_PAGE_LEFT:
      if (should_invert_move (range, GTK_ORIENTATION_HORIZONTAL))
        page_forward (range);
      else
        page_back (range);
      break;
                    
    case GTK_SCROLL_PAGE_UP:
      if (should_invert_move (range, GTK_ORIENTATION_VERTICAL))
        page_forward (range);
      else
        page_back (range);
      break;

    case GTK_SCROLL_PAGE_RIGHT:
      if (should_invert_move (range, GTK_ORIENTATION_HORIZONTAL))
        page_back (range);
      else
        page_forward (range);
      break;
                    
    case GTK_SCROLL_PAGE_DOWN:
      if (should_invert_move (range, GTK_ORIENTATION_VERTICAL))
        page_back (range);
      else
        page_forward (range);
      break;
                  
    case GTK_SCROLL_PAGE_BACKWARD:
      page_back (range);
      break;
                  
    case GTK_SCROLL_PAGE_FORWARD:
      page_forward (range);
      break;

    case GTK_SCROLL_START:
      scroll_begin (range);
      break;

    case GTK_SCROLL_END:
      scroll_end (range);
      break;

    case GTK_SCROLL_JUMP:
      /* Used by CList, range doesn't use it. */
      break;

    case GTK_SCROLL_NONE:
    default:
      break;
    }

  return gtk_adjustment_get_value (priv->adjustment) != old_value;
}

static void
gtk_range_move_slider (GtkRange     *range,
                       GtkScrollType scroll)
{
  if (! gtk_range_scroll (range, scroll))
    gtk_widget_error_bell (GTK_WIDGET (range));
}

static void
gtk_range_compute_slider_position (GtkRange     *range,
                                   gdouble       adjustment_value,
                                   GdkRectangle *slider_rect)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  int trough_width, trough_height;
  int slider_width, slider_height, min_slider_size;
  const double upper = gtk_adjustment_get_upper (priv->adjustment);
  const double lower = gtk_adjustment_get_lower (priv->adjustment);
  const double page_size = gtk_adjustment_get_page_size (priv->adjustment);

  gtk_widget_measure (priv->slider_widget,
                      GTK_ORIENTATION_HORIZONTAL, -1,
                      &slider_width, NULL,
                      NULL, NULL);
  gtk_widget_measure (priv->slider_widget,
                      GTK_ORIENTATION_VERTICAL, slider_width,
                      &slider_height, NULL,
                      NULL, NULL);

  trough_width = gtk_widget_get_width (priv->trough_widget);
  trough_height = gtk_widget_get_height (priv->trough_widget);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint y, bottom, top, height;
        
      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      slider_rect->x = (int) floor ((trough_width - slider_width) / 2);
      slider_rect->width = slider_width;

      min_slider_size = slider_height;

      /* Compute slider position/length */
      top = 0;
      bottom = top + trough_height;

      /* slider height is the fraction (page_size /
       * total_adjustment_range) times the trough height in pixels
       */

      if (upper - lower != 0)
        height = (bottom - top) * (page_size / (upper - lower));
      else
        height = min_slider_size;

      if (height < min_slider_size ||
          priv->slider_size_fixed)
        height = min_slider_size;

      height = MIN (height, trough_height);
      
      y = top;

      if (upper - lower - page_size != 0)
        y += (bottom - top - height) * ((adjustment_value - lower)  / (upper - lower - page_size));

      y = CLAMP (y, top, bottom);
      
      if (should_invert (range))
        y = bottom - (y - top + height);
      
      slider_rect->y = y;
      slider_rect->height = height;
    }
  else
    {
      gint x, left, right, width;
        
      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      slider_rect->y = (int) floor ((trough_height - slider_height) / 2);
      slider_rect->height = slider_height;

      min_slider_size = slider_width;

      /* Compute slider position/length */
      left = 0;
      right = left + trough_width;

      /* slider width is the fraction (page_size /
       * total_adjustment_range) times the trough width in pixels
       */

      if (upper - lower != 0)
        width = (right - left) * (page_size / (upper - lower));
      else
        width = min_slider_size;

      if (width < min_slider_size ||
          priv->slider_size_fixed)
        width = min_slider_size;

      width = MIN (width, trough_width);

      x = left;

      if (upper - lower - page_size != 0)
        x += (right - left - width) * ((adjustment_value - lower) / (upper - lower - page_size));
      
      x = CLAMP (x, left, right);
      
      if (should_invert (range))
        x = right - (x - left + width);
      
      slider_rect->x = x;
      slider_rect->width = width;
    }
}

static void
gtk_range_calc_slider (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gboolean visible;

  if (GTK_IS_SCALE (range) &&
      gtk_adjustment_get_upper (priv->adjustment) == gtk_adjustment_get_lower (priv->adjustment))
    visible = FALSE;
  else
    visible = TRUE;

  gtk_widget_set_visible (priv->slider_widget, visible);

  gtk_widget_queue_allocate (priv->trough_widget);
}

static void
gtk_range_calc_marks (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  GdkRectangle slider;
  gint i;

  for (i = 0; i < priv->n_marks; i++)
    {
      gtk_range_compute_slider_position (range, priv->marks[i], &slider);
      gtk_widget_translate_coordinates (priv->trough_widget, GTK_WIDGET (range),
                                        slider.x, slider.y, &slider.x, &slider.y);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        priv->mark_pos[i] = slider.x + slider.width / 2;
      else
        priv->mark_pos[i] = slider.y + slider.height / 2;
    }
}

static gboolean
gtk_range_real_change_value (GtkRange      *range,
                             GtkScrollType  scroll,
                             gdouble        value)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  /* potentially adjust the bounds _before_ we clamp */
  g_signal_emit (range, signals[ADJUST_BOUNDS], 0, value);

  if (priv->restrict_to_fill_level)
    value = MIN (value, MAX (gtk_adjustment_get_lower (priv->adjustment),
                             priv->fill_level));

  value = CLAMP (value, gtk_adjustment_get_lower (priv->adjustment),
                 (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment)));

  if (priv->round_digits >= 0)
    {
      gdouble power;
      gint i;

      i = priv->round_digits;
      power = 1;
      while (i--)
        power *= 10;

      value = floor ((value * power) + 0.5) / power;
    }

  if (priv->in_drag || priv->autoscroll_id)
    gtk_adjustment_set_value (priv->adjustment, value);
  else
    gtk_adjustment_animate_to_value (priv->adjustment, value);

  return FALSE;
}

struct _GtkRangeStepTimer
{
  guint timeout_id;
  GtkScrollType step;
};

static gboolean
second_timeout (gpointer data)
{
  GtkRange *range = GTK_RANGE (data);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  gtk_range_scroll (range, priv->timer->step);

  return G_SOURCE_CONTINUE;
}

static gboolean
initial_timeout (gpointer data)
{
  GtkRange *range = GTK_RANGE (data);
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  priv->timer->timeout_id = g_timeout_add (TIMEOUT_REPEAT, second_timeout, range);
  g_source_set_name_by_id (priv->timer->timeout_id, "[gtk] second_timeout");
  return G_SOURCE_REMOVE;
}

static void
gtk_range_add_step_timer (GtkRange      *range,
                          GtkScrollType  step)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (priv->timer == NULL);
  g_return_if_fail (step != GTK_SCROLL_NONE);

  priv->timer = g_new (GtkRangeStepTimer, 1);

  priv->timer->timeout_id = g_timeout_add (TIMEOUT_INITIAL, initial_timeout, range);
  g_source_set_name_by_id (priv->timer->timeout_id, "[gtk] initial_timeout");
  priv->timer->step = step;

  gtk_range_scroll (range, priv->timer->step);
}

static void
gtk_range_remove_step_timer (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (priv->timer)
    {
      if (priv->timer->timeout_id != 0)
        g_source_remove (priv->timer->timeout_id);

      g_free (priv->timer);

      priv->timer = NULL;
    }
}

void
_gtk_range_set_has_origin (GtkRange *range,
                           gboolean  has_origin)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  if (has_origin)
    {
      priv->highlight_widget = gtk_gizmo_new ("highlight", NULL, NULL, NULL, NULL);
      gtk_widget_insert_before (priv->highlight_widget, priv->trough_widget, priv->slider_widget);

      update_highlight_position (range);
    }
  else
    {
      g_clear_pointer (&priv->highlight_widget, gtk_widget_unparent);
    }
}

gboolean
_gtk_range_get_has_origin (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  return priv->highlight_widget != NULL;
}

void
_gtk_range_set_stop_values (GtkRange *range,
                            gdouble  *values,
                            gint      n_values)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);
  gint i;

  g_free (priv->marks);
  priv->marks = g_new (gdouble, n_values);

  g_free (priv->mark_pos);
  priv->mark_pos = g_new (gint, n_values);

  priv->n_marks = n_values;

  for (i = 0; i < n_values; i++) 
    priv->marks[i] = values[i];

  gtk_range_calc_marks (range);
}

gint
_gtk_range_get_stop_positions (GtkRange  *range,
                               gint     **values)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  gtk_range_calc_marks (range);

  if (values)
    *values = g_memdup (priv->mark_pos, priv->n_marks * sizeof (gint));

  return priv->n_marks;
}

/**
 * gtk_range_set_round_digits:
 * @range: a #GtkRange
 * @round_digits: the precision in digits, or -1
 *
 * Sets the number of digits to round the value to when
 * it changes. See #GtkRange::change-value.
 */
void
gtk_range_set_round_digits (GtkRange *range,
                            gint      round_digits)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (round_digits >= -1);

  if (priv->round_digits != round_digits)
    {
      priv->round_digits = round_digits;
      g_object_notify_by_pspec (G_OBJECT (range), properties[PROP_ROUND_DIGITS]);
    }
}

/**
 * gtk_range_get_round_digits:
 * @range: a #GtkRange
 *
 * Gets the number of digits to round the value to when
 * it changes. See #GtkRange::change-value.
 *
 * Returns: the number of digits to round to
 */
gint
gtk_range_get_round_digits (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  g_return_val_if_fail (GTK_IS_RANGE (range), -1);

  return priv->round_digits;
}

GtkWidget *
gtk_range_get_slider_widget (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  return priv->slider_widget;
}

GtkWidget *
gtk_range_get_trough_widget (GtkRange *range)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  return priv->trough_widget;
}

void
gtk_range_start_autoscroll (GtkRange      *range,
                            GtkScrollType  scroll_type)
{
  GtkRangePrivate *priv = gtk_range_get_instance_private (range);

  remove_autoscroll (range);
  priv->autoscroll_mode = scroll_type;
  add_autoscroll (range);
}

void
gtk_range_stop_autoscroll (GtkRange *range)
{
  remove_autoscroll (range);
}
