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

#include <stdio.h>
#include <math.h>

#include "gtkrange.h"
#include "gtkrangeprivate.h"

#include "gtkadjustmentprivate.h"
#include "gtkcolorscaleprivate.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtkscrollbar.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"
#include "a11y/gtkrangeaccessible.h"

/**
 * SECTION:gtkrange
 * @Short_description: Base class for widgets which visualize an adjustment
 * @Title: GtkRange
 *
 * #GtkRange is the common base class for widgets which visualize an
 * adjustment, e.g #GtkScale or #GtkScrollbar.
 *
 * Apart from signals for monitoring the parameters of the adjustment,
 * #GtkRange provides properties and methods for influencing the sensitivity
 * of the “steppers”. It also provides properties and methods for setting a
 * “fill level” on range widgets. See gtk_range_set_fill_level().
 */


#define SCROLL_DELAY_FACTOR 5    /* Scroll repeat multiplier */
#define UPDATE_DELAY        300  /* Delay for queued update */
#define TIMEOUT_INITIAL     500
#define TIMEOUT_REPEAT      50
#define ZOOM_HOLD_TIME      500
#define AUTOSCROLL_FACTOR   20
#define SCROLL_EDGE_SIZE 15

typedef struct _GtkRangeStepTimer GtkRangeStepTimer;

typedef enum {
  MOUSE_OUTSIDE,
  MOUSE_STEPPER_A,
  MOUSE_STEPPER_B,
  MOUSE_STEPPER_C,
  MOUSE_STEPPER_D,
  MOUSE_TROUGH,
  MOUSE_SLIDER,
  MOUSE_WIDGET /* inside widget but not in any of the above GUI elements */
} MouseLocation;

struct _GtkRangePrivate
{
  MouseLocation      mouse_location;
  /* last mouse coords we got, or -1 if mouse is outside the range */
  gint  mouse_x;
  gint  mouse_y;
  MouseLocation      grab_location;   /* "grabbed" mouse location, OUTSIDE for no grab */

  GtkRangeStepTimer *timer;

  GtkAdjustment     *adjustment;
  GtkSensitivityType lower_sensitivity;
  GtkSensitivityType upper_sensitivity;

  GdkRectangle       range_rect;     /* Area of entire stepper + trough assembly in widget->window coords */
  /* These are in widget->window coordinates */
  GdkRectangle       stepper_a;
  GdkRectangle       stepper_b;
  GdkRectangle       stepper_c;
  GdkRectangle       stepper_d;
  GdkRectangle       trough;         /* The trough rectangle is the area the thumb can slide in, not the entire range_rect */
  GdkRectangle       slider;
  GdkWindow         *event_window;

  GQuark             slider_detail_quark;
  GQuark             stepper_detail_quark[4];

  GtkOrientation     orientation;

  gdouble  fill_level;
  gdouble *marks;

  gint *mark_pos;
  gint  min_slider_size;
  gint  n_marks;
  gint  round_digits;                /* Round off value to this many digits, -1 for no rounding */
  gint  slide_initial_slider_position;
  gint  slide_initial_coordinate_delta;
  gint  slider_start;                /* Slider range along the long dimension, in widget->window coords */
  gint  slider_end;

  /* Steppers are: < > ---- < >
   *               a b      c d
   */
  guint has_stepper_a          : 1;
  guint has_stepper_b          : 1;
  guint has_stepper_c          : 1;
  guint has_stepper_d          : 1;

  guint flippable              : 1;
  guint inverted               : 1;
  guint need_recalc            : 1;
  guint recalc_marks           : 1;
  guint slider_size_fixed      : 1;
  guint trough_click_forward   : 1;  /* trough click was on the forward side of slider */

  /* Stepper sensitivity */
  guint lower_sensitive        : 1;
  guint upper_sensitive        : 1;

  /* The range has an origin, should be drawn differently. Used by GtkScale */
  guint has_origin             : 1;

  /* Whether we're doing fine adjustment */
  guint zoom                   : 1;
  guint zoom_set               : 1;
  GtkGesture *long_press_gesture;
  GtkScrollType autoscroll_mode;
  guint autoscroll_id;

  GtkGesture *multipress_gesture;
  GtkGesture *drag_gesture;

  /* Fill level */
  guint show_fill_level        : 1;
  guint restrict_to_fill_level : 1;

  /* Whether dragging is ongoing */
  guint in_drag                : 1;
};


enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_ADJUSTMENT,
  PROP_INVERTED,
  PROP_LOWER_STEPPER_SENSITIVITY,
  PROP_UPPER_STEPPER_SENSITIVITY,
  PROP_SHOW_FILL_LEVEL,
  PROP_RESTRICT_TO_FILL_LEVEL,
  PROP_FILL_LEVEL,
  PROP_ROUND_DIGITS
};

enum {
  VALUE_CHANGED,
  ADJUST_BOUNDS,
  MOVE_SLIDER,
  CHANGE_VALUE,
  LAST_SIGNAL
};

typedef enum {
  STEPPER_A,
  STEPPER_B,
  STEPPER_C,
  STEPPER_D
} Stepper;

static void gtk_range_set_property   (GObject          *object,
                                      guint             prop_id,
                                      const GValue     *value,
                                      GParamSpec       *pspec);
static void gtk_range_get_property   (GObject          *object,
                                      guint             prop_id,
                                      GValue           *value,
                                      GParamSpec       *pspec);
static void gtk_range_destroy        (GtkWidget        *widget);
static void gtk_range_get_preferred_width
                                     (GtkWidget        *widget,
                                      gint             *minimum,
                                      gint             *natural);
static void gtk_range_get_preferred_height
                                     (GtkWidget        *widget,
                                      gint             *minimum,
                                      gint             *natural);
static void gtk_range_size_allocate  (GtkWidget        *widget,
                                      GtkAllocation    *allocation);
static void gtk_range_realize        (GtkWidget        *widget);
static void gtk_range_unrealize      (GtkWidget        *widget);
static void gtk_range_map            (GtkWidget        *widget);
static void gtk_range_unmap          (GtkWidget        *widget);
static gboolean gtk_range_draw       (GtkWidget        *widget,
                                      cairo_t          *cr);

static void gtk_range_multipress_gesture_pressed  (GtkGestureMultiPress *gesture,
                                                   guint                 n_press,
                                                   gdouble               x,
                                                   gdouble               y,
                                                   GtkRange             *range);
static void gtk_range_multipress_gesture_released (GtkGestureMultiPress *gesture,
                                                   guint                 n_press,
                                                   gdouble               x,
                                                   gdouble               y,
                                                   GtkRange             *range);
static void gtk_range_drag_gesture_update         (GtkGestureDrag       *gesture,
                                                   gdouble               offset_x,
                                                   gdouble               offset_y,
                                                   GtkRange             *range);
static void gtk_range_drag_gesture_end            (GtkGestureDrag       *gesture,
                                                   gdouble               offset_x,
                                                   gdouble               offset_y,
                                                   GtkRange             *range);
static void gtk_range_long_press_gesture_pressed  (GtkGestureLongPress  *gesture,
                                                   gdouble               x,
                                                   gdouble               y,
                                                   GtkRange             *range);


static gboolean gtk_range_scroll_event   (GtkWidget        *widget,
                                      GdkEventScroll   *event);
static gboolean gtk_range_event       (GtkWidget       *widget,
                                       GdkEvent        *event);
static void gtk_range_style_updated  (GtkWidget        *widget);
static void update_slider_position   (GtkRange	       *range,
				      gint              mouse_x,
				      gint              mouse_y);
static void stop_scrolling           (GtkRange         *range);

/* Range methods */

static void gtk_range_move_slider              (GtkRange         *range,
                                                GtkScrollType     scroll);

/* Internals */
static gboolean      gtk_range_scroll                   (GtkRange      *range,
                                                         GtkScrollType  scroll);
static gboolean      gtk_range_update_mouse_location    (GtkRange      *range);
static void          gtk_range_calc_layout              (GtkRange      *range,
							 gdouble	adjustment_value);
static void          gtk_range_calc_marks               (GtkRange      *range);
static void          gtk_range_get_props                (GtkRange      *range,
                                                         gint          *slider_width,
                                                         gint          *stepper_size,
                                                         gint          *trough_border,
                                                         gint          *stepper_spacing,
                                                         gboolean      *trough_under_steppers,
							 gint          *arrow_displacement_x,
							 gint	       *arrow_displacement_y);
static void          gtk_range_calc_request             (GtkRange      *range,
                                                         gint           slider_width,
                                                         gint           stepper_size,
                                                         gint           trough_border,
                                                         gint           stepper_spacing,
                                                         GdkRectangle  *range_rect,
                                                         GtkBorder     *border,
                                                         gint          *n_steppers_p,
                                                         gboolean      *has_steppers_ab,
                                                         gboolean      *has_steppers_cd,
                                                         gint          *slider_length_p);
static void          gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_adjustment_changed       (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_add_step_timer           (GtkRange      *range,
                                                         GtkScrollType  step);
static void          gtk_range_remove_step_timer        (GtkRange      *range);
static GdkRectangle* get_area                           (GtkRange      *range,
                                                         MouseLocation  location);
static gboolean      gtk_range_real_change_value        (GtkRange      *range,
                                                         GtkScrollType  scroll,
                                                         gdouble        value);
static gboolean      gtk_range_key_press                (GtkWidget     *range,
							 GdkEventKey   *event);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkRange, gtk_range, GTK_TYPE_WIDGET,
                                  G_ADD_PRIVATE (GtkRange)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                         NULL))

static guint signals[LAST_SIGNAL];


static void
gtk_range_class_init (GtkRangeClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_range_set_property;
  gobject_class->get_property = gtk_range_get_property;

  widget_class->destroy = gtk_range_destroy;
  widget_class->get_preferred_width = gtk_range_get_preferred_width;
  widget_class->get_preferred_height = gtk_range_get_preferred_height;
  widget_class->size_allocate = gtk_range_size_allocate;
  widget_class->realize = gtk_range_realize;
  widget_class->unrealize = gtk_range_unrealize;
  widget_class->map = gtk_range_map;
  widget_class->unmap = gtk_range_unmap;
  widget_class->draw = gtk_range_draw;
  widget_class->event = gtk_range_event;
  widget_class->scroll_event = gtk_range_scroll_event;
  widget_class->style_updated = gtk_range_style_updated;
  widget_class->key_press_event = gtk_range_key_press;

  class->move_slider = gtk_range_move_slider;
  class->change_value = gtk_range_real_change_value;

  class->slider_detail = "slider";
  class->stepper_detail = "stepper";

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
                  _gtk_marshal_VOID__VOID,
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
                  _gtk_marshal_VOID__DOUBLE,
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
                  _gtk_marshal_VOID__ENUM,
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
   * It is not possible to use delayed update policies in an overridden
   * #GtkRange::change-value handler.
   *
   * Returns: %TRUE to prevent other handlers from being invoked for
   *     the signal, %FALSE to propagate the signal further
   *
   * Since: 2.6
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

  g_object_class_override_property (gobject_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
							P_("Adjustment"),
							P_("The GtkAdjustment that contains the current value of this range object"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_INVERTED,
                                   g_param_spec_boolean ("inverted",
							P_("Inverted"),
							P_("Invert direction slider moves to increase range value"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_LOWER_STEPPER_SENSITIVITY,
                                   g_param_spec_enum ("lower-stepper-sensitivity",
						      P_("Lower stepper sensitivity"),
						      P_("The sensitivity policy for the stepper that points to the adjustment's lower side"),
						      GTK_TYPE_SENSITIVITY_TYPE,
						      GTK_SENSITIVITY_AUTO,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_UPPER_STEPPER_SENSITIVITY,
                                   g_param_spec_enum ("upper-stepper-sensitivity",
						      P_("Upper stepper sensitivity"),
						      P_("The sensitivity policy for the stepper that points to the adjustment's upper side"),
						      GTK_TYPE_SENSITIVITY_TYPE,
						      GTK_SENSITIVITY_AUTO,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRange:show-fill-level:
   *
   * The show-fill-level property controls whether fill level indicator
   * graphics are displayed on the trough. See
   * gtk_range_set_show_fill_level().
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_FILL_LEVEL,
                                   g_param_spec_boolean ("show-fill-level",
                                                         P_("Show Fill Level"),
                                                         P_("Whether to display a fill level indicator graphics on trough."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRange:restrict-to-fill-level:
   *
   * The restrict-to-fill-level property controls whether slider
   * movement is restricted to an upper boundary set by the
   * fill level. See gtk_range_set_restrict_to_fill_level().
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_RESTRICT_TO_FILL_LEVEL,
                                   g_param_spec_boolean ("restrict-to-fill-level",
                                                         P_("Restrict to Fill Level"),
                                                         P_("Whether to restrict the upper boundary to the fill level."),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRange:fill-level:
   *
   * The fill level (e.g. prebuffering of a network stream).
   * See gtk_range_set_fill_level().
   *
   * Since: 2.12
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILL_LEVEL,
                                   g_param_spec_double ("fill-level",
							P_("Fill Level"),
							P_("The fill level."),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkRange:round-digits:
   *
   * The number of digits to round the value to when
   * it changes, or -1. See #GtkRange::change-value.
   *
   * Since: 2.24
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ROUND_DIGITS,
                                   g_param_spec_int ("round-digits",
                                                     P_("Round Digits"),
                                                     P_("The number of digits to round the value to."),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("slider-width",
							     P_("Slider Width"),
							     P_("Width of scrollbar or scale thumb"),
							     0,
							     G_MAXINT,
							     14,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("trough-border",
                                                             P_("Trough Border"),
                                                             P_("Spacing between thumb/steppers and outer trough bevel"),
                                                             0,
                                                             G_MAXINT,
                                                             1,
                                                             GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("stepper-size",
							     P_("Stepper Size"),
							     P_("Length of step buttons at ends"),
							     0,
							     G_MAXINT,
							     14,
							     GTK_PARAM_READABLE));
  /**
   * GtkRange:stepper-spacing:
   *
   * The spacing between the stepper buttons and thumb. Note that
   * stepper-spacing won't have any effect if there are no steppers.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("stepper-spacing",
							     P_("Stepper Spacing"),
							     P_("Spacing between step buttons and thumb"),
                                                             0,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("arrow-displacement-x",
							     P_("Arrow X Displacement"),
							     P_("How far in the x direction to move the arrow when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("arrow-displacement-y",
							     P_("Arrow Y Displacement"),
							     P_("How far in the y direction to move the arrow when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));

  /**
   * GtkRange:trough-under-steppers:
   *
   * Whether to draw the trough across the full length of the range or
   * to exclude the steppers and their spacing.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("trough-under-steppers",
                                                                 P_("Trough Under Steppers"),
                                                                 P_("Whether to draw trough for full length of range or exclude the steppers and spacing"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkRange:arrow-scaling:
   *
   * The arrow size proportion relative to the scroll button size.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
							       P_("Arrow scaling"),
							       P_("Arrow scaling with regard to scroll button size"),
							       0.0, 1.0, 0.5,
							       GTK_PARAM_READABLE));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_RANGE_ACCESSIBLE);
}

static void
gtk_range_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkRange *range = GTK_RANGE (object);
  GtkRangePrivate *priv = range->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      if (priv->orientation != g_value_get_enum (value))
        {
          priv->orientation = g_value_get_enum (value);

          priv->slider_detail_quark = 0;
          priv->stepper_detail_quark[0] = 0;
          priv->stepper_detail_quark[1] = 0;
          priv->stepper_detail_quark[2] = 0;
          priv->stepper_detail_quark[3] = 0;

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
    case PROP_LOWER_STEPPER_SENSITIVITY:
      gtk_range_set_lower_stepper_sensitivity (range, g_value_get_enum (value));
      break;
    case PROP_UPPER_STEPPER_SENSITIVITY:
      gtk_range_set_upper_stepper_sensitivity (range, g_value_get_enum (value));
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
  GtkRangePrivate *priv = range->priv;

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
    case PROP_LOWER_STEPPER_SENSITIVITY:
      g_value_set_enum (value, gtk_range_get_lower_stepper_sensitivity (range));
      break;
    case PROP_UPPER_STEPPER_SENSITIVITY:
      g_value_set_enum (value, gtk_range_get_upper_stepper_sensitivity (range));
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
  GtkRangePrivate *priv;

  range->priv = gtk_range_get_instance_private (range);
  priv = range->priv;

  gtk_widget_set_has_window (GTK_WIDGET (range), FALSE);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->adjustment = NULL;
  priv->inverted = FALSE;
  priv->flippable = FALSE;
  priv->min_slider_size = 1;
  priv->has_stepper_a = FALSE;
  priv->has_stepper_b = FALSE;
  priv->has_stepper_c = FALSE;
  priv->has_stepper_d = FALSE;
  priv->need_recalc = TRUE;
  priv->round_digits = -1;
  priv->mouse_location = MOUSE_OUTSIDE;
  priv->mouse_x = -1;
  priv->mouse_y = -1;
  priv->grab_location = MOUSE_OUTSIDE;
  priv->lower_sensitivity = GTK_SENSITIVITY_AUTO;
  priv->upper_sensitivity = GTK_SENSITIVITY_AUTO;
  priv->lower_sensitive = TRUE;
  priv->upper_sensitive = TRUE;
  priv->has_origin = FALSE;
  priv->show_fill_level = FALSE;
  priv->restrict_to_fill_level = TRUE;
  priv->fill_level = G_MAXDOUBLE;
  priv->timer = NULL;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (range));

  priv->multipress_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (range));
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->multipress_gesture), 0);
  g_signal_connect (priv->multipress_gesture, "pressed",
                    G_CALLBACK (gtk_range_multipress_gesture_pressed), range);
  g_signal_connect (priv->multipress_gesture, "released",
                    G_CALLBACK (gtk_range_multipress_gesture_released), range);

  priv->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (range));
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_range_drag_gesture_update), range);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_range_drag_gesture_end), range);

  priv->long_press_gesture = gtk_gesture_long_press_new (GTK_WIDGET (range));
  gtk_gesture_group (priv->drag_gesture, priv->long_press_gesture);
  g_signal_connect (priv->long_press_gesture, "pressed",
                    G_CALLBACK (gtk_range_long_press_gesture_pressed), range);
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
  GtkRangePrivate *priv;

  g_return_val_if_fail (GTK_IS_RANGE (range), NULL);

  priv = range->priv;

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
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

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
      g_object_notify (G_OBJECT (range), "adjustment");
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
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  setting = setting != FALSE;

  if (setting != priv->inverted)
    {
      priv->inverted = setting;
      g_object_notify (G_OBJECT (range), "inverted");
      gtk_widget_queue_resize (GTK_WIDGET (range));
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
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->priv->inverted;
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
 *
 * Since: 2.18
 **/
void
gtk_range_set_flippable (GtkRange *range,
                         gboolean  flippable)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  flippable = flippable ? TRUE : FALSE;

  if (flippable != priv->flippable)
    {
      priv->flippable = flippable;

      gtk_widget_queue_draw (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_flippable:
 * @range: a #GtkRange
 *
 * Gets the value set by gtk_range_set_flippable().
 *
 * Returns: %TRUE if the range is flippable
 *
 * Since: 2.18
 **/
gboolean
gtk_range_get_flippable (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->priv->flippable;
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
 *
 * Since: 2.20
 **/
void
gtk_range_set_slider_size_fixed (GtkRange *range,
                                 gboolean  size_fixed)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  if (size_fixed != priv->slider_size_fixed)
    {
      priv->slider_size_fixed = size_fixed ? TRUE : FALSE;

      if (priv->adjustment && gtk_widget_get_mapped (GTK_WIDGET (range)))
        {
          priv->need_recalc = TRUE;
          gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));
          gtk_widget_queue_draw (GTK_WIDGET (range));
        }
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
 *
 * Since: 2.20
 **/
gboolean
gtk_range_get_slider_size_fixed (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->priv->slider_size_fixed;
}

/**
 * gtk_range_set_min_slider_size:
 * @range: a #GtkRange
 * @min_size: The slider’s minimum size
 *
 * Sets the minimum size of the range’s slider.
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * Since: 2.20
 **/
void
gtk_range_set_min_slider_size (GtkRange *range,
                               gint      min_size)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (min_size > 0);

  priv = range->priv;

  if (min_size != priv->min_slider_size)
    {
      priv->min_slider_size = min_size;

      if (gtk_widget_is_drawable (GTK_WIDGET (range)))
        {
          priv->need_recalc = TRUE;
          gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));
          gtk_widget_queue_draw (GTK_WIDGET (range));
        }
    }
}

/**
 * gtk_range_get_min_slider_size:
 * @range: a #GtkRange
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * See gtk_range_set_min_slider_size().
 *
 * Returns: The minimum size of the range’s slider.
 *
 * Since: 2.20
 **/
gint
gtk_range_get_min_slider_size (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->priv->min_slider_size;
}

/**
 * gtk_range_get_range_rect:
 * @range: a #GtkRange
 * @range_rect: (out): return location for the range rectangle
 *
 * This function returns the area that contains the range’s trough
 * and its steppers, in widget->window coordinates.
 *
 * This function is useful mainly for #GtkRange subclasses.
 *
 * Since: 2.20
 **/
void
gtk_range_get_range_rect (GtkRange     *range,
                          GdkRectangle *range_rect)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (range_rect != NULL);

  priv = range->priv;

  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));

  *range_rect = priv->range_rect;
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
 *
 * Since: 2.20
 **/
void
gtk_range_get_slider_range (GtkRange *range,
                            gint     *slider_start,
                            gint     *slider_end)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));

  if (slider_start)
    *slider_start = priv->slider_start;

  if (slider_end)
    *slider_end = priv->slider_end;
}

/**
 * gtk_range_set_lower_stepper_sensitivity:
 * @range:       a #GtkRange
 * @sensitivity: the lower stepper’s sensitivity policy.
 *
 * Sets the sensitivity policy for the stepper that points to the
 * 'lower' end of the GtkRange’s adjustment.
 *
 * Since: 2.10
 **/
void
gtk_range_set_lower_stepper_sensitivity (GtkRange           *range,
                                         GtkSensitivityType  sensitivity)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  if (priv->lower_sensitivity != sensitivity)
    {
      priv->lower_sensitivity = sensitivity;

      priv->need_recalc = TRUE;
      gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));
      gtk_widget_queue_draw (GTK_WIDGET (range));

      g_object_notify (G_OBJECT (range), "lower-stepper-sensitivity");
    }
}

/**
 * gtk_range_get_lower_stepper_sensitivity:
 * @range: a #GtkRange
 *
 * Gets the sensitivity policy for the stepper that points to the
 * 'lower' end of the GtkRange’s adjustment.
 *
 * Returns: The lower stepper’s sensitivity policy.
 *
 * Since: 2.10
 **/
GtkSensitivityType
gtk_range_get_lower_stepper_sensitivity (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_SENSITIVITY_AUTO);

  return range->priv->lower_sensitivity;
}

/**
 * gtk_range_set_upper_stepper_sensitivity:
 * @range:       a #GtkRange
 * @sensitivity: the upper stepper’s sensitivity policy.
 *
 * Sets the sensitivity policy for the stepper that points to the
 * 'upper' end of the GtkRange’s adjustment.
 *
 * Since: 2.10
 **/
void
gtk_range_set_upper_stepper_sensitivity (GtkRange           *range,
                                         GtkSensitivityType  sensitivity)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  if (priv->upper_sensitivity != sensitivity)
    {
      priv->upper_sensitivity = sensitivity;

      priv->need_recalc = TRUE;
      gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));
      gtk_widget_queue_draw (GTK_WIDGET (range));

      g_object_notify (G_OBJECT (range), "upper-stepper-sensitivity");
    }
}

/**
 * gtk_range_get_upper_stepper_sensitivity:
 * @range: a #GtkRange
 *
 * Gets the sensitivity policy for the stepper that points to the
 * 'upper' end of the GtkRange’s adjustment.
 *
 * Returns: The upper stepper’s sensitivity policy.
 *
 * Since: 2.10
 **/
GtkSensitivityType
gtk_range_get_upper_stepper_sensitivity (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_SENSITIVITY_AUTO);

  return range->priv->upper_sensitivity;
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
  GtkAdjustment *adjustment;

  g_return_if_fail (GTK_IS_RANGE (range));

  adjustment = range->priv->adjustment;

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
  GtkRangePrivate *priv;
  GtkAdjustment *adjustment;
  gdouble value;
  
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (min <= max);

  priv = range->priv;
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
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

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
  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return gtk_adjustment_get_value (range->priv->adjustment);
}

/**
 * gtk_range_set_show_fill_level:
 * @range:           A #GtkRange
 * @show_fill_level: Whether a fill level indicator graphics is shown.
 *
 * Sets whether a graphical fill level is show on the trough. See
 * gtk_range_set_fill_level() for a general description of the fill
 * level concept.
 *
 * Since: 2.12
 **/
void
gtk_range_set_show_fill_level (GtkRange *range,
                               gboolean  show_fill_level)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  show_fill_level = show_fill_level ? TRUE : FALSE;

  if (show_fill_level != priv->show_fill_level)
    {
      priv->show_fill_level = show_fill_level;
      g_object_notify (G_OBJECT (range), "show-fill-level");
      gtk_widget_queue_draw (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_show_fill_level:
 * @range: A #GtkRange
 *
 * Gets whether the range displays the fill level graphically.
 *
 * Returns: %TRUE if @range shows the fill level.
 *
 * Since: 2.12
 **/
gboolean
gtk_range_get_show_fill_level (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->priv->show_fill_level;
}

/**
 * gtk_range_set_restrict_to_fill_level:
 * @range:                  A #GtkRange
 * @restrict_to_fill_level: Whether the fill level restricts slider movement.
 *
 * Sets whether the slider is restricted to the fill level. See
 * gtk_range_set_fill_level() for a general description of the fill
 * level concept.
 *
 * Since: 2.12
 **/
void
gtk_range_set_restrict_to_fill_level (GtkRange *range,
                                      gboolean  restrict_to_fill_level)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  restrict_to_fill_level = restrict_to_fill_level ? TRUE : FALSE;

  if (restrict_to_fill_level != priv->restrict_to_fill_level)
    {
      priv->restrict_to_fill_level = restrict_to_fill_level;
      g_object_notify (G_OBJECT (range), "restrict-to-fill-level");

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
 *
 * Since: 2.12
 **/
gboolean
gtk_range_get_restrict_to_fill_level (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->priv->restrict_to_fill_level;
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
 *
 * Since: 2.12
 **/
void
gtk_range_set_fill_level (GtkRange *range,
                          gdouble   fill_level)
{
  GtkRangePrivate *priv;

  g_return_if_fail (GTK_IS_RANGE (range));

  priv = range->priv;

  if (fill_level != priv->fill_level)
    {
      priv->fill_level = fill_level;
      g_object_notify (G_OBJECT (range), "fill-level");

      if (priv->show_fill_level)
        gtk_widget_queue_draw (GTK_WIDGET (range));

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
 *
 * Since: 2.12
 **/
gdouble
gtk_range_get_fill_level (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return range->priv->fill_level;
}

static gboolean
should_invert (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    return
      (priv->inverted && !priv->flippable) ||
      (priv->inverted && priv->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_LTR) ||
      (!priv->inverted && priv->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_RTL);
  else
    return priv->inverted;
}

static void
gtk_range_destroy (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  gtk_range_remove_step_timer (range);

  g_clear_object (&priv->drag_gesture);
  g_clear_object (&priv->multipress_gesture);
  g_clear_object (&priv->long_press_gesture);

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
gtk_range_get_preferred_width (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  GtkRange *range;
  gint slider_width, stepper_size, trough_border, stepper_spacing;
  GdkRectangle range_rect;
  GtkBorder border;

  range = GTK_RANGE (widget);

  gtk_range_get_props (range,
                       &slider_width, &stepper_size,
                       &trough_border,
                       &stepper_spacing, NULL,
                       NULL, NULL);

  gtk_range_calc_request (range,
                          slider_width, stepper_size,
                          trough_border, stepper_spacing,
                          &range_rect, &border, NULL, NULL, NULL, NULL);

  *minimum = *natural = range_rect.width + border.left + border.right;
}

static void
gtk_range_get_preferred_height (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  GtkRange *range;
  gint slider_width, stepper_size, trough_border, stepper_spacing;
  GdkRectangle range_rect;
  GtkBorder border;

  range = GTK_RANGE (widget);

  gtk_range_get_props (range,
                       &slider_width, &stepper_size,
                       &trough_border,
                       &stepper_spacing, NULL,
                       NULL, NULL);

  gtk_range_calc_request (range,
                          slider_width, stepper_size,
                          trough_border, stepper_spacing,
                          &range_rect, &border, NULL, NULL, NULL, NULL);

  *minimum = *natural = range_rect.height + border.top + border.bottom;
}

static void
gtk_range_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  gtk_widget_set_allocation (widget, allocation);

  priv->recalc_marks = TRUE;

  priv->need_recalc = TRUE;
  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);
}

static void
gtk_range_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));

  gtk_widget_set_realized (widget, TRUE);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_SCROLL_MASK |
                            GDK_SMOOTH_SCROLL_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					&attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_range_unrealize (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  gtk_range_remove_step_timer (range);

  gtk_widget_unregister_window (widget, priv->event_window);
  gdk_window_destroy (priv->event_window);
  priv->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_range_parent_class)->unrealize (widget);
}

static void
gtk_range_map (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gtk_range_parent_class)->map (widget);
}

static void
gtk_range_unmap (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  stop_scrolling (range);

  gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_range_parent_class)->unmap (widget);
}

static void
_gtk_range_update_context_for_stepper (GtkRange        *range,
                                       GtkStyleContext *context,
                                       Stepper          stepper)
{
  GtkRangePrivate *priv = range->priv;
  GtkJunctionSides sides = 0;
  gboolean vertical, is_rtl;

  vertical = (priv->orientation == GTK_ORIENTATION_VERTICAL);
  is_rtl = (gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_RTL);

  /* Take junction sides from what's been
   * previously set to the widget itself
   */
  sides = gtk_style_context_get_junction_sides (context);

  if (vertical)
    sides &= ~(GTK_JUNCTION_TOP | GTK_JUNCTION_BOTTOM);
  else
    sides &= ~(GTK_JUNCTION_LEFT | GTK_JUNCTION_RIGHT);

  switch (stepper)
    {
    case STEPPER_A:
      if (vertical)
        sides |= GTK_JUNCTION_BOTTOM;
      else
        sides |= (is_rtl) ? GTK_JUNCTION_LEFT : GTK_JUNCTION_RIGHT;
      break;
    case STEPPER_B:
      if (priv->has_stepper_a)
        {
          if (vertical)
            sides |= GTK_JUNCTION_TOP;
          else
            sides |= (is_rtl) ? GTK_JUNCTION_RIGHT : GTK_JUNCTION_LEFT;
        }

      if (vertical)
        sides |= GTK_JUNCTION_BOTTOM;
      else
        sides |= (is_rtl) ? GTK_JUNCTION_LEFT : GTK_JUNCTION_RIGHT;
      break;
    case STEPPER_C:
      if (priv->has_stepper_d)
        {
          if (vertical)
            sides |= GTK_JUNCTION_BOTTOM;
          else
            sides |= (is_rtl) ? GTK_JUNCTION_LEFT : GTK_JUNCTION_RIGHT;
        }

      if (vertical)
        sides |= GTK_JUNCTION_TOP;
      else
        sides |= (is_rtl) ? GTK_JUNCTION_RIGHT : GTK_JUNCTION_LEFT;
      break;
    case STEPPER_D:
      if (vertical)
        sides |= GTK_JUNCTION_TOP;
      else
        sides |= (is_rtl) ? GTK_JUNCTION_RIGHT : GTK_JUNCTION_LEFT;
      break;
    }

  gtk_style_context_set_junction_sides (context, sides);
}

static void
draw_stepper (GtkRange     *range,
              Stepper       stepper,
              cairo_t      *cr,
              GtkArrowType  arrow_type,
              gboolean      clicked,
              gboolean      prelighted,
              GtkStateFlags state)
{
  GtkRangePrivate *priv = range->priv;
  GtkAllocation allocation;
  GtkStyleContext *context;
  GtkWidget *widget = GTK_WIDGET (range);
  gfloat arrow_scaling;
  GdkRectangle *rect;
  gdouble arrow_x;
  gdouble arrow_y;
  gdouble arrow_size, angle;
  gboolean arrow_sensitive;

  switch (stepper)
    {
    case STEPPER_A:
      rect = &priv->stepper_a;
      break;
    case STEPPER_B:
      rect = &priv->stepper_b;
      break;
    case STEPPER_C:
      rect = &priv->stepper_c;
      break;
    case STEPPER_D:
      rect = &priv->stepper_d;
      break;
    default:
      g_assert_not_reached ();
    };

  gtk_widget_get_allocation (widget, &allocation);

  if ((!priv->inverted && (arrow_type == GTK_ARROW_DOWN ||
                            arrow_type == GTK_ARROW_RIGHT)) ||
      (priv->inverted  && (arrow_type == GTK_ARROW_UP ||
                            arrow_type == GTK_ARROW_LEFT)))
    {
      arrow_sensitive = priv->upper_sensitive;
    }
  else
    {
      arrow_sensitive = priv->lower_sensitive;
    }

  state &= ~(GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT);

  if ((state & GTK_STATE_FLAG_INSENSITIVE) || !arrow_sensitive)
    {
      state |= GTK_STATE_FLAG_INSENSITIVE;
    }
  else
    {
      if (clicked)
        state |= GTK_STATE_FLAG_ACTIVE;
      if (prelighted)
        state |= GTK_STATE_FLAG_PRELIGHT;
    }

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  /* don't set juction sides on scrollbar steppers */
  if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_SCROLLBAR))
    gtk_style_context_set_junction_sides (context, GTK_JUNCTION_NONE);
  else
    _gtk_range_update_context_for_stepper (range, context, stepper);

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);
  gtk_style_context_set_state (context, state);

  switch (arrow_type)
    {
    case GTK_ARROW_RIGHT:
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);
      break;
    case GTK_ARROW_DOWN:
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
      break;
    case GTK_ARROW_LEFT:
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);
      break;
    case GTK_ARROW_UP:
    default:
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);
      break;
    }

  gtk_render_background (context, cr,
                         rect->x, rect->y,
                         rect->width, rect->height);
  gtk_render_frame (context, cr,
                    rect->x, rect->y,
                    rect->width, rect->height);

  gtk_widget_style_get (widget, "arrow-scaling", &arrow_scaling, NULL);

  arrow_size = MIN (rect->width, rect->height) * arrow_scaling;
  arrow_x = rect->x + (rect->width - arrow_size) / 2;
  arrow_y = rect->y + (rect->height - arrow_size) / 2;

  if (clicked && arrow_sensitive)
    {
      gint arrow_displacement_x;
      gint arrow_displacement_y;

      gtk_range_get_props (GTK_RANGE (widget),
                           NULL, NULL, NULL, NULL, NULL,
                           &arrow_displacement_x, &arrow_displacement_y);

      arrow_x += arrow_displacement_x;
      arrow_y += arrow_displacement_y;
    }

  switch (arrow_type)
    {
    case GTK_ARROW_RIGHT:
      angle = G_PI / 2;
      break;
    case GTK_ARROW_DOWN:
      angle = G_PI;
      break;
    case GTK_ARROW_LEFT:
      angle = 3 * (G_PI / 2);
      break;
    case GTK_ARROW_UP:
    default:
      angle = 0;
      break;
    }

  gtk_render_arrow (context, cr,
                    angle,
                    arrow_x, arrow_y,
                    arrow_size);

  gtk_style_context_restore (context);
}

static gboolean
gtk_range_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;
  GtkStateFlags widget_state;
  gboolean draw_trough = TRUE;
  gboolean draw_slider = TRUE;
  GtkStyleContext *context;
  GtkBorder margin;

  context = gtk_widget_get_style_context (widget);

  if (GTK_IS_SCALE (widget) &&
      gtk_adjustment_get_upper (priv->adjustment) == gtk_adjustment_get_lower (priv->adjustment))
    {
      draw_trough = TRUE;
      draw_slider = FALSE;
    }
  if (GTK_IS_COLOR_SCALE (widget))
    {
      draw_trough = FALSE;
      draw_slider = TRUE;
    }

  gtk_range_calc_marks (range);
  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));

  widget_state = gtk_widget_get_state_flags (widget);

  /* Just to be confusing, we draw the trough for the whole
   * range rectangle, not the trough rectangle (the trough
   * rectangle is just for hit detection)
   */
  cairo_save (cr);
  gdk_cairo_rectangle (cr, &priv->range_rect);
  cairo_clip (cr);

    {
      gint     x      = priv->range_rect.x;
      gint     y      = priv->range_rect.y;
      gint     width  = priv->range_rect.width;
      gint     height = priv->range_rect.height;
      gboolean trough_under_steppers;
      gint     stepper_size;
      gint     stepper_spacing;

      gtk_widget_style_get (GTK_WIDGET (range),
                            "trough-under-steppers", &trough_under_steppers,
                            "stepper-size",          &stepper_size,
                            "stepper-spacing",       &stepper_spacing,
                            NULL);

      if (!trough_under_steppers)
        {
          gint offset  = 0;
          gint shorter = 0;

          if (priv->has_stepper_a)
            offset += stepper_size;

          if (priv->has_stepper_b)
            offset += stepper_size;

          shorter += offset;

          if (priv->has_stepper_c)
            shorter += stepper_size;

          if (priv->has_stepper_d)
            shorter += stepper_size;

          if (priv->has_stepper_a || priv->has_stepper_b)
            {
              offset  += stepper_spacing;
              shorter += stepper_spacing;
            }

          if (priv->has_stepper_c || priv->has_stepper_d)
            {
              shorter += stepper_spacing;
            }

          if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              x     += offset;
              width -= shorter;
            }
          else
            {
              y      += offset;
              height -= shorter;
            }
        }

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);
      gtk_style_context_get_margin (context, widget_state, &margin);

      x += margin.left;
      y += margin.top;
      width -= margin.left + margin.right;
      height -= margin.top + margin.bottom;

      if (draw_trough)
        {
          if (!priv->has_origin || !draw_slider)
            {
              gtk_render_background (context, cr,
                                     x, y, width, height);

              gtk_render_frame (context, cr,
                                x, y, width, height);
            }
          else
            {
              gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

              gint trough_change_pos_x = width;
              gint trough_change_pos_y = height;

              if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
                trough_change_pos_x = (priv->slider.x +
                                       priv->slider.width / 2 -
                                       x);
              else
                trough_change_pos_y = (priv->slider.y +
                                       priv->slider.height / 2 -
                                       y);

              gtk_style_context_save (context);
              if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
                {
                  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);

                  if (!is_rtl)
                    gtk_style_context_add_class (context, GTK_STYLE_CLASS_HIGHLIGHT);
                }
              else
                gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);

              gtk_render_background (context, cr, x, y,
                                     trough_change_pos_x,
                                     trough_change_pos_y);

              gtk_render_frame (context, cr, x, y,
                                trough_change_pos_x,
                                trough_change_pos_y);

              gtk_style_context_restore (context);

              if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
                trough_change_pos_y = 0;
              else
                trough_change_pos_x = 0;

              gtk_style_context_save (context);
              if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
                {
                  gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);

                  if (is_rtl)
                    gtk_style_context_add_class (context, GTK_STYLE_CLASS_HIGHLIGHT);
                }
              else
                {
                  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
                  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HIGHLIGHT);
                }

              gtk_render_background (context, cr,
                                     x + trough_change_pos_x, y + trough_change_pos_y,
                                     width - trough_change_pos_x,
                                     height - trough_change_pos_y);

              gtk_render_frame (context, cr,
                                x + trough_change_pos_x, y + trough_change_pos_y,
                                width - trough_change_pos_x,
                                height - trough_change_pos_y);

              gtk_style_context_restore (context);
            }
        }

      gtk_style_context_restore (context);

      if (priv->show_fill_level &&
          gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment) -
          gtk_adjustment_get_lower (priv->adjustment) != 0)
        {
          gdouble  fill_level  = priv->fill_level;
          gint     fill_x      = x;
          gint     fill_y      = y;
          gint     fill_width  = width;
          gint     fill_height = height;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);

          fill_level = CLAMP (fill_level, gtk_adjustment_get_lower (priv->adjustment),
                              gtk_adjustment_get_upper (priv->adjustment) -
                              gtk_adjustment_get_page_size (priv->adjustment));

          if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              fill_x     = priv->trough.x;
              fill_width = (priv->slider.width +
                            (fill_level - gtk_adjustment_get_lower (priv->adjustment)) /
                            (gtk_adjustment_get_upper (priv->adjustment) -
                             gtk_adjustment_get_lower (priv->adjustment) -
                             gtk_adjustment_get_page_size (priv->adjustment)) *
                            (priv->trough.width -
                             priv->slider.width));

              if (should_invert (range))
                fill_x += priv->trough.width - fill_width;
            }
          else
            {
              fill_y      = priv->trough.y;
              fill_height = (priv->slider.height +
                             (fill_level - gtk_adjustment_get_lower (priv->adjustment)) /
                             (gtk_adjustment_get_upper (priv->adjustment) -
                              gtk_adjustment_get_lower (priv->adjustment) -
                              gtk_adjustment_get_page_size (priv->adjustment)) *
                             (priv->trough.height -
                              priv->slider.height));

              if (should_invert (range))
                fill_y += priv->trough.height - fill_height;
            }

          gtk_render_activity (context, cr,
                               fill_x, fill_y,
                               fill_width, fill_height);

          gtk_style_context_restore (context);
        }

      if (!(widget_state & GTK_STATE_FLAG_INSENSITIVE) && gtk_widget_has_visible_focus (widget))
        {
          gtk_render_focus (context, cr,
                            priv->range_rect.x,
                            priv->range_rect.y,
                            priv->range_rect.width,
                            priv->range_rect.height);
        }
    }

  cairo_restore (cr);

  if (draw_slider)
    {
      GtkStateFlags state = widget_state;

      state &= ~(GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);

      if (priv->mouse_location == MOUSE_SLIDER && !(state & GTK_STATE_FLAG_INSENSITIVE))
        state |= GTK_STATE_FLAG_PRELIGHT;

      if (priv->grab_location == MOUSE_SLIDER)
        state |= GTK_STATE_FLAG_ACTIVE;

      cairo_save (cr);
      gdk_cairo_rectangle (cr, &priv->slider);
      cairo_clip (cr);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_SLIDER);
      gtk_style_context_set_state (context, state);

      gtk_render_slider (context, cr,
                         priv->slider.x,
                         priv->slider.y,
                         priv->slider.width,
                         priv->slider.height,
                         priv->orientation);

      gtk_style_context_restore (context);

      cairo_restore (cr);
    }

  if (priv->has_stepper_a)
    draw_stepper (range, STEPPER_A, cr,
                  priv->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_UP : GTK_ARROW_LEFT,
                  priv->grab_location == MOUSE_STEPPER_A,
                  priv->mouse_location == MOUSE_STEPPER_A,
                  widget_state);

  if (priv->has_stepper_b)
    draw_stepper (range, STEPPER_B, cr,
                  priv->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  priv->grab_location == MOUSE_STEPPER_B,
                  priv->mouse_location == MOUSE_STEPPER_B,
                  widget_state);

  if (priv->has_stepper_c)
    draw_stepper (range, STEPPER_C, cr,
                  priv->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_UP : GTK_ARROW_LEFT,
                  priv->grab_location == MOUSE_STEPPER_C,
                  priv->mouse_location == MOUSE_STEPPER_C,
                  widget_state);

  if (priv->has_stepper_d)
    draw_stepper (range, STEPPER_D, cr,
                  priv->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  priv->grab_location == MOUSE_STEPPER_D,
                  priv->mouse_location == MOUSE_STEPPER_D,
                  widget_state);

  return FALSE;
}

static void
range_grab_add (GtkRange      *range,
                MouseLocation  location)
{
  GtkRangePrivate *priv = range->priv;

  /* Don't perform any GDK/GTK+ grab here. Since a button
   * is down, there's an ongoing implicit grab on
   * priv->event_window, which pretty much guarantees this
   * is the only widget receiving the pointer events.
   */
  priv->grab_location = location;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (GTK_WIDGET (range));
}

static void
update_zoom_state (GtkRange *range,
                   gboolean  enabled)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (range));

  if (enabled)
    gtk_style_context_add_class (context, "fine-tune");
  else
    gtk_style_context_remove_class (context, "fine-tune");
  gtk_widget_queue_draw (GTK_WIDGET (range));

  range->priv->zoom = enabled;
}

static void
range_grab_remove (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;
  MouseLocation location;

  location = priv->grab_location;
  priv->grab_location = MOUSE_OUTSIDE;

  if (gtk_range_update_mouse_location (range) ||
      location != MOUSE_OUTSIDE)
    gtk_widget_queue_draw (GTK_WIDGET (range));

  update_zoom_state (range, FALSE);
  range->priv->zoom_set = FALSE;
}

static GtkScrollType
range_get_scroll_for_grab (GtkRange      *range)
{
  GtkRangePrivate *priv = range->priv;
  guint grab_button;
  gboolean invert;

  invert = should_invert (range);
  grab_button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (range->priv->multipress_gesture));

  switch (priv->grab_location)
    {
      /* Backward stepper */
    case MOUSE_STEPPER_A:
    case MOUSE_STEPPER_C:
      switch (grab_button)
        {
        case GDK_BUTTON_PRIMARY:
          return invert ? GTK_SCROLL_STEP_FORWARD : GTK_SCROLL_STEP_BACKWARD;
          break;
        case GDK_BUTTON_MIDDLE:
          return invert ? GTK_SCROLL_PAGE_FORWARD : GTK_SCROLL_PAGE_BACKWARD;
          break;
        case GDK_BUTTON_SECONDARY:
          return invert ? GTK_SCROLL_END : GTK_SCROLL_START;
          break;
        }
      break;

      /* Forward stepper */
    case MOUSE_STEPPER_B:
    case MOUSE_STEPPER_D:
      switch (grab_button)
        {
        case GDK_BUTTON_PRIMARY:
          return invert ? GTK_SCROLL_STEP_BACKWARD : GTK_SCROLL_STEP_FORWARD;
          break;
        case GDK_BUTTON_MIDDLE:
          return invert ? GTK_SCROLL_PAGE_BACKWARD : GTK_SCROLL_PAGE_FORWARD;
          break;
        case GDK_BUTTON_SECONDARY:
          return invert ? GTK_SCROLL_START : GTK_SCROLL_END;
          break;
       }
      break;

      /* In the trough */
    case MOUSE_TROUGH:
      {
        if (priv->trough_click_forward)
	  return GTK_SCROLL_PAGE_FORWARD;
        else
	  return GTK_SCROLL_PAGE_BACKWARD;
      }
      break;

    case MOUSE_OUTSIDE:
    case MOUSE_SLIDER:
    case MOUSE_WIDGET:
      break;
    }

  return GTK_SCROLL_NONE;
}

static gdouble
coord_to_value (GtkRange *range,
                gdouble   coord)
{
  GtkRangePrivate *priv = range->priv;
  gdouble frac;
  gdouble value;
  gint    trough_length;
  gint    trough_start;
  gint    slider_length;
  gint    trough_border;
  gint    trough_under_steppers;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      trough_length = priv->trough.height;
      trough_start  = priv->trough.y;
      slider_length = priv->slider.height;
    }
  else
    {
      trough_length = priv->trough.width;
      trough_start  = priv->trough.x;
      slider_length = priv->slider.width;
    }

  gtk_range_get_props (range, NULL, NULL, &trough_border, NULL,
                       &trough_under_steppers, NULL, NULL);

  if (! trough_under_steppers)
    {
      trough_start += trough_border;
      trough_length -= 2 * trough_border;
    }

  if (trough_length == slider_length)
    frac = 1.0;
  else
    frac = (MAX (0, coord - trough_start) /
            (gdouble) (trough_length - slider_length));

  if (should_invert (range))
    frac = 1.0 - frac;

  value = gtk_adjustment_get_lower (priv->adjustment) + frac * (gtk_adjustment_get_upper (priv->adjustment) -
                                            gtk_adjustment_get_lower (priv->adjustment) -
                                            gtk_adjustment_get_page_size (priv->adjustment));

  return value;
}

static gboolean
gtk_range_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GdkDevice *device;
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  device = gdk_event_get_device ((GdkEvent *) event);
  device = gdk_device_get_associated_device (device);

  if (gtk_gesture_is_active (priv->drag_gesture) &&
      device == gtk_gesture_get_device (priv->drag_gesture) &&
      event->keyval == GDK_KEY_Escape &&
      priv->grab_location != MOUSE_OUTSIDE)
    {
      stop_scrolling (range);

      update_slider_position (range,
			      priv->slide_initial_coordinate_delta + priv->slide_initial_slider_position,
			      priv->slide_initial_coordinate_delta + priv->slide_initial_slider_position);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_range_parent_class)->key_press_event (widget, event);
}

static void
gtk_range_long_press_gesture_pressed (GtkGestureLongPress *gesture,
                                      gdouble              x,
                                      gdouble              y,
                                      GtkRange            *range)
{
  update_zoom_state (range, TRUE);
  range->priv->zoom_set = TRUE;
}

static void
gtk_range_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                      guint                 n_press,
                                      gdouble               x,
                                      gdouble               y,
                                      GtkRange             *range)
{
  GtkWidget *widget = GTK_WIDGET (range);
  GtkRangePrivate *priv = range->priv;
  GdkDevice *source_device;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GdkInputSource source;
  gboolean primary_warps;
  gint page_increment_button, warp_button;
  guint button;

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  source_device = gdk_event_get_source_device ((GdkEvent *) event);
  source = gdk_device_get_source (source_device);

  priv->mouse_x = x;
  priv->mouse_y = y;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-primary-button-warps-slider", &primary_warps,
                NULL);
  if (primary_warps)
    {
      warp_button = GDK_BUTTON_PRIMARY;
      page_increment_button = GDK_BUTTON_SECONDARY;
    }
  else
    {
      warp_button = GDK_BUTTON_MIDDLE;
      page_increment_button = GDK_BUTTON_PRIMARY;
    }

  if (priv->mouse_location == MOUSE_SLIDER &&
      gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
      gboolean handled;

      g_signal_emit_by_name (widget, "popup-menu", &handled);

      return;
    }

  if (source != GDK_SOURCE_TOUCHSCREEN &&
      priv->mouse_location == MOUSE_TROUGH &&
      button == page_increment_button)
    {
      /* button 2 steps by page increment, as with button 2 on a stepper
       */
      GtkScrollType scroll;
      gdouble click_value;

      click_value = coord_to_value (range,
                                    priv->orientation == GTK_ORIENTATION_VERTICAL ?
                                    y : x);

      priv->trough_click_forward = click_value > gtk_adjustment_get_value (priv->adjustment);
      range_grab_add (range, MOUSE_TROUGH);

      scroll = range_get_scroll_for_grab (range);

      gtk_range_add_step_timer (range, scroll);
    }
  else if ((priv->mouse_location == MOUSE_STEPPER_A ||
            priv->mouse_location == MOUSE_STEPPER_B ||
            priv->mouse_location == MOUSE_STEPPER_C ||
            priv->mouse_location == MOUSE_STEPPER_D) &&
           (button == GDK_BUTTON_PRIMARY ||
            button == GDK_BUTTON_MIDDLE ||
            button == GDK_BUTTON_SECONDARY))
    {
      GtkAllocation allocation;
      GdkRectangle *stepper_area;
      GtkScrollType scroll;

      range_grab_add (range, priv->mouse_location);

      gtk_widget_get_allocation (widget, &allocation);
      stepper_area = get_area (range, priv->mouse_location);

      gtk_widget_queue_draw_area (widget,
                                  allocation.x + stepper_area->x,
                                  allocation.y + stepper_area->y,
                                  stepper_area->width,
                                  stepper_area->height);

      scroll = range_get_scroll_for_grab (range);
      if (scroll != GTK_SCROLL_NONE)
        gtk_range_add_step_timer (range, scroll);
    }
  else if ((priv->mouse_location == MOUSE_TROUGH &&
            (source == GDK_SOURCE_TOUCHSCREEN ||
             button == warp_button)) ||
           priv->mouse_location == MOUSE_SLIDER)
    {
      gboolean need_value_update = FALSE;

      /* Any button can be used to drag the slider, but you can start
       * dragging the slider with a trough click using button 1;
       * we warp the slider to mouse position, then begin the slider drag.
       */
      if (priv->mouse_location != MOUSE_SLIDER)
        {
          gdouble slider_low_value, slider_high_value, new_value;
          
          slider_high_value =
            coord_to_value (range,
                            priv->orientation == GTK_ORIENTATION_VERTICAL ?
                            y : x);
          slider_low_value =
            coord_to_value (range,
                            priv->orientation == GTK_ORIENTATION_VERTICAL ?
                            y - priv->slider.height :
                            x - priv->slider.width);

          /* compute new value for warped slider */
          new_value = slider_low_value + (slider_high_value - slider_low_value) / 2;

	  /* recalc slider, so we can set slide_initial_slider_position
           * properly
           */
	  priv->need_recalc = TRUE;
          gtk_range_calc_layout (range, new_value);

	  /* defer adjustment updates to update_slider_position() in order
	   * to keep pixel quantisation
	   */
	  need_value_update = TRUE;
        }
      else
        {
          GdkModifierType state_mask;

          gdk_event_get_state (event, &state_mask);

          /* Shift-click in the slider = fine adjustment */
          if (state_mask & GDK_SHIFT_MASK)
            {
              update_zoom_state (range, TRUE);
              range->priv->zoom_set = TRUE;
            }
        }

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          priv->slide_initial_slider_position = priv->slider.y;
          priv->slide_initial_coordinate_delta = y - priv->slider.y;
        }
      else
        {
          priv->slide_initial_slider_position = priv->slider.x;
          priv->slide_initial_coordinate_delta = x - priv->slider.x;
        }

      range_grab_add (range, MOUSE_SLIDER);

      gtk_widget_queue_draw (widget);

      if (need_value_update)
        update_slider_position (range, x, y);
    }

  if (priv->grab_location == MOUSE_SLIDER)
    gtk_gesture_set_state (priv->drag_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
  else if (priv->grab_location != MOUSE_OUTSIDE)
    gtk_gesture_set_state (priv->multipress_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_range_multipress_gesture_released (GtkGestureMultiPress *gesture,
                                       guint                 n_press,
                                       gdouble               x,
                                       gdouble               y,
                                       GtkRange             *range)
{
  GtkRangePrivate *priv = range->priv;

  priv->mouse_x = x;
  priv->mouse_y = y;
  stop_scrolling (range);
}

/* During a slide, move the slider as required given new mouse position */
static void
update_slider_position (GtkRange *range,
                        gint      mouse_x,
                        gint      mouse_y)
{
  GtkRangePrivate *priv = range->priv;
  gdouble delta;
  gdouble c;
  gdouble new_value;
  gboolean handled;
  gdouble next_value;
  gdouble mark_value;
  gdouble mark_delta;
  gdouble zoom;
  gint i;

  if (priv->zoom)
    {
      zoom = MIN(1.0, (priv->orientation == GTK_ORIENTATION_VERTICAL ?
                       priv->trough.height : priv->trough.width) /
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
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        priv->slide_initial_slider_position = (zoom * (mouse_y - priv->slide_initial_coordinate_delta) - priv->slider.y) / (zoom - 1.0);
      else
        priv->slide_initial_slider_position = (zoom * (mouse_x - priv->slide_initial_coordinate_delta) - priv->slider.x) / (zoom - 1.0);
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
          if (fabs (new_value - mark_value) < (priv->slider_end - priv->slider_start) * 0.5 * mark_delta)
            {
              new_value = mark_value;
              break;
            }
        }
    }

  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_JUMP, new_value,
                 &handled);
}

static void
remove_autoscroll (GtkRange *range)
{
  if (range->priv->autoscroll_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (range),
                                       range->priv->autoscroll_id);
      range->priv->autoscroll_id = 0;
    }

  /* unset initial position so it can be calculated */
  range->priv->slide_initial_slider_position = -1;

  range->priv->autoscroll_mode = GTK_SCROLL_NONE;
}

static gboolean
autoscroll_cb (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       data)
{
  GtkRange *range;
  gdouble increment;
  gdouble value;
  gboolean handled;

  range = GTK_RANGE (data);

  increment = gtk_adjustment_get_step_increment (range->priv->adjustment) / AUTOSCROLL_FACTOR;
  if (range->priv->autoscroll_mode == GTK_SCROLL_STEP_BACKWARD)
    increment *= -1;

  value = gtk_adjustment_get_value (range->priv->adjustment);
  value += increment;
  g_signal_emit (range, signals[CHANGE_VALUE], 0, GTK_SCROLL_JUMP, value,
                 &handled);

  return G_SOURCE_CONTINUE;
}

static void
add_autoscroll (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;

  if (priv->autoscroll_id != 0
      || priv->autoscroll_mode == GTK_SCROLL_NONE)
    return;

  priv->autoscroll_id = gtk_widget_add_tick_callback (GTK_WIDGET (range),
                                                      autoscroll_cb,
                                                      range,
                                                      NULL);
}

static void
stop_scrolling (GtkRange *range)
{
  range_grab_remove (range);
  gtk_range_remove_step_timer (range);
  remove_autoscroll (range);
}

/**
 * _gtk_range_get_wheel_delta:
 * @range: a #GtkRange
 * @event: A #GdkEventScroll
 *
 * Returns a good step value for the mouse wheel.
 *
 * Returns: A good step value for the mouse wheel.
 *
 * Since: 2.4
 **/
gdouble
_gtk_range_get_wheel_delta (GtkRange       *range,
                            GdkEventScroll *event)
{
  GtkRangePrivate *priv = range->priv;
  GtkAdjustment *adjustment = priv->adjustment;
  gdouble dx, dy;
  gdouble delta;
  gdouble page_size;
  gdouble page_increment;
  gdouble scroll_unit;

  page_size = gtk_adjustment_get_page_size (adjustment);
  page_increment = gtk_adjustment_get_page_increment (adjustment);

  if (GTK_IS_SCROLLBAR (range))
    scroll_unit = pow (page_size, 2.0 / 3.0);
  else
    scroll_unit = page_increment;

  if (gdk_event_get_scroll_deltas ((GdkEvent *) event, &dx, &dy))
    {
      if (dx != 0 &&
          gtk_orientable_get_orientation (GTK_ORIENTABLE (range)) == GTK_ORIENTATION_HORIZONTAL)
        delta = dx * scroll_unit;
      else
        delta = dy * scroll_unit;
    }
  else
    {
      if (event->direction == GDK_SCROLL_UP ||
          event->direction == GDK_SCROLL_LEFT)
        delta = - scroll_unit;
      else
        delta = scroll_unit;
    }

  if (priv->inverted)
    delta = - delta;

  return delta;
}

static gboolean
gtk_range_scroll_event (GtkWidget      *widget,
			GdkEventScroll *event)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  if (gtk_widget_get_realized (widget))
    {
      gdouble delta;
      gboolean handled;

      delta = _gtk_range_get_wheel_delta (range, event);

      g_signal_emit (range, signals[CHANGE_VALUE], 0,
                     GTK_SCROLL_JUMP, gtk_adjustment_get_value (priv->adjustment) + delta,
                     &handled);
    }

  return TRUE;
}

static void
update_autoscroll_mode (GtkRange *range)
{
  GtkScrollType mode = GTK_SCROLL_NONE;

  if (range->priv->zoom)
    {
      GtkAllocation allocation;
      gint size, pos;

      gtk_widget_get_allocation (GTK_WIDGET (range), &allocation);

      if (range->priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          size = allocation.height;
          pos = range->priv->mouse_y;
        }
      else
        {
          size = allocation.width;
          pos = range->priv->mouse_x;
        }

      if (pos < SCROLL_EDGE_SIZE)
        mode = range->priv->inverted ? GTK_SCROLL_STEP_FORWARD : GTK_SCROLL_STEP_BACKWARD;
      else if (pos > (size - SCROLL_EDGE_SIZE))
        mode = range->priv->inverted ? GTK_SCROLL_STEP_BACKWARD : GTK_SCROLL_STEP_FORWARD;
    }

  if (mode != range->priv->autoscroll_mode)
    {
      remove_autoscroll (range);
      range->priv->autoscroll_mode = mode;
      add_autoscroll (range);
    }
}

static void
gtk_range_drag_gesture_update (GtkGestureDrag *gesture,
                               gdouble         offset_x,
                               gdouble         offset_y,
                               GtkRange       *range)
{
  GtkRangePrivate *priv = range->priv;
  gdouble start_x, start_y;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
  priv->mouse_x = start_x + offset_x;
  priv->mouse_y = start_y + offset_y;
  priv->in_drag = TRUE;

  update_autoscroll_mode (range);

  if (priv->autoscroll_mode == GTK_SCROLL_NONE)
    update_slider_position (range, priv->mouse_x, priv->mouse_y);
}

static void
gtk_range_drag_gesture_end (GtkGestureDrag       *gesture,
                            gdouble               offset_x,
                            gdouble               offset_y,
                            GtkRange             *range)
{
  range->priv->in_drag = FALSE;
  stop_scrolling (range);
}

static gboolean
gtk_range_event (GtkWidget *widget,
                 GdkEvent  *event)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;
  gdouble x, y;

  if (event->type == GDK_LEAVE_NOTIFY)
    {
      priv->mouse_x = -1;
      priv->mouse_y = -1;
    }
  else if (gdk_event_get_coords (event, &x, &y))
    {
      priv->mouse_x = x;
      priv->mouse_y = y;
    }

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);

  return GDK_EVENT_PROPAGATE;
}

#define check_rectangle(rectangle1, rectangle2)              \
  {                                                          \
    if (rectangle1.x != rectangle2.x) return TRUE;           \
    if (rectangle1.y != rectangle2.y) return TRUE;           \
    if (rectangle1.width  != rectangle2.width)  return TRUE; \
    if (rectangle1.height != rectangle2.height) return TRUE; \
  }

static gboolean
layout_changed (GtkRangePrivate *priv1,
		GtkRangePrivate *priv2)
{
  check_rectangle (priv1->slider, priv2->slider);
  check_rectangle (priv1->trough, priv2->trough);
  check_rectangle (priv1->stepper_a, priv2->stepper_a);
  check_rectangle (priv1->stepper_d, priv2->stepper_d);
  check_rectangle (priv1->stepper_b, priv2->stepper_b);
  check_rectangle (priv1->stepper_c, priv2->stepper_c);

  if (priv1->upper_sensitive != priv2->upper_sensitive) return TRUE;
  if (priv1->lower_sensitive != priv2->lower_sensitive) return TRUE;

  return FALSE;
}

static void
gtk_range_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GtkRange *range = GTK_RANGE (data);
  GtkRangePrivate *priv = range->priv;
  GtkRangePrivate priv_aux = *priv;

  priv->recalc_marks = TRUE;
  priv->need_recalc = TRUE;
  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));

  /* now check whether the layout changed  */
  if (layout_changed (priv, &priv_aux))
    gtk_widget_queue_draw (GTK_WIDGET (range));

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
  GtkRangePrivate *priv = range->priv;
  GtkRangePrivate priv_aux = *priv;

  priv->need_recalc = TRUE;
  gtk_range_calc_layout (range, gtk_adjustment_get_value (priv->adjustment));
  
  /* now check whether the layout changed  */
  if (layout_changed (priv, &priv_aux) ||
      (GTK_IS_SCALE (range) && gtk_scale_get_draw_value (GTK_SCALE (range))))
    {
      gtk_widget_queue_draw (GTK_WIDGET (range));
    }

  /* Note that we don't round off to priv->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */

  g_signal_emit (range, signals[VALUE_CHANGED], 0);
}

static void
gtk_range_style_updated (GtkWidget *widget)
{
  GtkRange *range = GTK_RANGE (widget);
  GtkRangePrivate *priv = range->priv;

  priv->need_recalc = TRUE;

  GTK_WIDGET_CLASS (gtk_range_parent_class)->style_updated (widget);
}

static void
apply_marks (GtkRange *range, 
             gdouble   oldval,
             gdouble  *newval)
{
  GtkRangePrivate *priv = range->priv;
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
  GtkRangePrivate *priv = range->priv;
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
  GtkRangePrivate *priv = range->priv;
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
  GtkRangePrivate *priv = range->priv;
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
  GtkRangePrivate *priv = range->priv;
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
  GtkRangePrivate *priv = range->priv;
  gboolean handled;

  g_signal_emit (range, signals[CHANGE_VALUE], 0,
                 GTK_SCROLL_START, gtk_adjustment_get_lower (priv->adjustment),
                 &handled);
}

static void
scroll_end (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;
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
  GtkRangePrivate *priv = range->priv;
  gdouble old_value = gtk_adjustment_get_value (priv->adjustment);

  switch (scroll)
    {
    case GTK_SCROLL_STEP_LEFT:
      if (should_invert (range))
        step_forward (range);
      else
        step_back (range);
      break;
                    
    case GTK_SCROLL_STEP_UP:
      if (should_invert (range))
        step_forward (range);
      else
        step_back (range);
      break;

    case GTK_SCROLL_STEP_RIGHT:
      if (should_invert (range))
        step_back (range);
      else
        step_forward (range);
      break;
                    
    case GTK_SCROLL_STEP_DOWN:
      if (should_invert (range))
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
      if (should_invert (range))
        page_forward (range);
      else
        page_back (range);
      break;
                    
    case GTK_SCROLL_PAGE_UP:
      if (should_invert (range))
        page_forward (range);
      else
        page_back (range);
      break;

    case GTK_SCROLL_PAGE_RIGHT:
      if (should_invert (range))
        page_back (range);
      else
        page_forward (range);
      break;
                    
    case GTK_SCROLL_PAGE_DOWN:
      if (should_invert (range))
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
gtk_range_get_props (GtkRange  *range,
                     gint      *slider_width,
                     gint      *stepper_size,
                     gint      *trough_border,
                     gint      *stepper_spacing,
                     gboolean  *trough_under_steppers,
		     gint      *arrow_displacement_x,
		     gint      *arrow_displacement_y)
{
  GtkWidget *widget =  GTK_WIDGET (range);
  gint tmp_slider_width, tmp_stepper_size, tmp_trough_border;
  gint tmp_stepper_spacing, tmp_trough_under_steppers;
  gint tmp_arrow_displacement_x, tmp_arrow_displacement_y;
  
  gtk_widget_style_get (widget,
                        "slider-width", &tmp_slider_width,
                        "trough-border", &tmp_trough_border,
                        "stepper-size", &tmp_stepper_size,
                        "stepper-spacing", &tmp_stepper_spacing,
                        "trough-under-steppers", &tmp_trough_under_steppers,
			"arrow-displacement-x", &tmp_arrow_displacement_x,
			"arrow-displacement-y", &tmp_arrow_displacement_y,
                        NULL);

  if (slider_width)
    *slider_width = tmp_slider_width;

  if (trough_border)
    *trough_border = tmp_trough_border;

  if (stepper_size)
    *stepper_size = tmp_stepper_size;

  if (stepper_spacing)
    *stepper_spacing = tmp_stepper_spacing;

  if (trough_under_steppers)
    *trough_under_steppers = tmp_trough_under_steppers;

  if (arrow_displacement_x)
    *arrow_displacement_x = tmp_arrow_displacement_x;

  if (arrow_displacement_y)
    *arrow_displacement_y = tmp_arrow_displacement_y;
}

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

/* Update mouse location, return TRUE if it changes */
static gboolean
gtk_range_update_mouse_location (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;
  GtkAllocation allocation;
  gint x, y;
  MouseLocation old;
  GtkWidget *widget = GTK_WIDGET (range);

  old = priv->mouse_location;

  x = priv->mouse_x;
  y = priv->mouse_y;

  gtk_widget_get_allocation (widget, &allocation);

  if (priv->grab_location != MOUSE_OUTSIDE)
    priv->mouse_location = priv->grab_location;
  else if (POINT_IN_RECT (x, y, priv->stepper_a))
    priv->mouse_location = MOUSE_STEPPER_A;
  else if (POINT_IN_RECT (x, y, priv->stepper_b))
    priv->mouse_location = MOUSE_STEPPER_B;
  else if (POINT_IN_RECT (x, y, priv->stepper_c))
    priv->mouse_location = MOUSE_STEPPER_C;
  else if (POINT_IN_RECT (x, y, priv->stepper_d))
    priv->mouse_location = MOUSE_STEPPER_D;
  else if (POINT_IN_RECT (x, y, priv->slider))
    priv->mouse_location = MOUSE_SLIDER;
  else if (POINT_IN_RECT (x, y, priv->trough))
    priv->mouse_location = MOUSE_TROUGH;
  else if (POINT_IN_RECT (x, y, allocation))
    priv->mouse_location = MOUSE_WIDGET;
  else
    priv->mouse_location = MOUSE_OUTSIDE;

  return old != priv->mouse_location;
}

/* Clamp rect, border inside widget->allocation, such that we prefer
 * to take space from border not rect in all directions, and prefer to
 * give space to border over rect in one direction.
 */
static void
clamp_dimensions (GtkWidget    *widget,
                  GdkRectangle *rect,
                  GtkBorder    *border,
                  gboolean      border_expands_horizontally)
{
  GtkAllocation allocation;
  gint extra, shortage;
  
  g_return_if_fail (rect->x == 0);
  g_return_if_fail (rect->y == 0);  
  g_return_if_fail (rect->width >= 0);
  g_return_if_fail (rect->height >= 0);

  gtk_widget_get_allocation (widget, &allocation);

  /* Width */

  extra = allocation.width - border->left - border->right - rect->width;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          border->left += extra / 2;
          border->right += extra / 2 + extra % 2;
        }
      else
        {
          rect->width += extra;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = rect->width - allocation.width;
  if (shortage > 0)
    {
      rect->width = allocation.width;
      /* lose the border */
      border->left = 0;
      border->right = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->width + border->left + border->right - allocation.width;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->left -= shortage / 2;
          border->right -= shortage / 2 + shortage % 2;
        }
    }

  /* Height */

  extra = allocation.height - border->top - border->bottom - rect->height;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          /* don't expand border vertically */
          rect->height += extra;
        }
      else
        {
          border->top += extra / 2;
          border->bottom += extra / 2 + extra % 2;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = rect->height - allocation.height;
  if (shortage > 0)
    {
      rect->height = allocation.height;
      /* lose the border */
      border->top = 0;
      border->bottom = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->height + border->top + border->bottom - allocation.height;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->top -= shortage / 2;
          border->bottom -= shortage / 2 + shortage % 2;
        }
    }
}

static void
gtk_range_calc_request (GtkRange      *range,
                        gint           slider_width,
                        gint           stepper_size,
                        gint           trough_border,
                        gint           stepper_spacing,
                        GdkRectangle  *range_rect,
                        GtkBorder     *border,
                        gint          *n_steppers_p,
                        gboolean      *has_steppers_ab,
                        gboolean      *has_steppers_cd,
                        gint          *slider_length_p)
{
  GtkRangePrivate *priv = range->priv;
  gint slider_length;
  gint n_steppers;
  gint n_steppers_ab;
  gint n_steppers_cd;

  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (GTK_RANGE_GET_CLASS (range)->get_range_border)
    GTK_RANGE_GET_CLASS (range)->get_range_border (range, border);

  n_steppers_ab = 0;
  n_steppers_cd = 0;

  if (priv->has_stepper_a)
    n_steppers_ab += 1;
  if (priv->has_stepper_b)
    n_steppers_ab += 1;
  if (priv->has_stepper_c)
    n_steppers_cd += 1;
  if (priv->has_stepper_d)
    n_steppers_cd += 1;

  n_steppers = n_steppers_ab + n_steppers_cd;

  slider_length = priv->min_slider_size;

  range_rect->x = 0;
  range_rect->y = 0;
  
  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      range_rect->width =  + trough_border * 2 + slider_width;
      range_rect->height = stepper_size * n_steppers + trough_border * 2 + slider_length;

      if (n_steppers_ab > 0)
        range_rect->height += stepper_spacing;

      if (n_steppers_cd > 0)
        range_rect->height += stepper_spacing;
    }
  else
    {
      range_rect->width = stepper_size * n_steppers + trough_border * 2 + slider_length;
      range_rect->height = trough_border * 2 + slider_width;

      if (n_steppers_ab > 0)
        range_rect->width += stepper_spacing;

      if (n_steppers_cd > 0)
        range_rect->width += stepper_spacing;
    }

  if (n_steppers_p)
    *n_steppers_p = n_steppers;

  if (has_steppers_ab)
    *has_steppers_ab = (n_steppers_ab > 0);

  if (has_steppers_cd)
    *has_steppers_cd = (n_steppers_cd > 0);

  if (slider_length_p)
    *slider_length_p = slider_length;
}

static void
gtk_range_calc_layout (GtkRange *range,
		       gdouble   adjustment_value)
{
  GtkRangePrivate *priv = range->priv;
  gint slider_width, stepper_size, trough_border, stepper_spacing;
  gint slider_length;
  GtkBorder border;
  gint n_steppers;
  gboolean has_steppers_ab;
  gboolean has_steppers_cd;
  gboolean trough_under_steppers;
  GdkRectangle range_rect;
  GtkWidget *widget;

  if (!priv->need_recalc)
    return;

  /* If we have a too-small allocation, we prefer the steppers over
   * the trough/slider, probably the steppers are a more useful
   * feature in small spaces.
   *
   * Also, we prefer to draw the range itself rather than the border
   * areas if there's a conflict, since the borders will be decoration
   * not controls. Though this depends on subclasses cooperating by
   * not drawing on priv->range_rect.
   */

  widget = GTK_WIDGET (range);

  gtk_range_get_props (range,
                       &slider_width, &stepper_size,
                       &trough_border,
                       &stepper_spacing, &trough_under_steppers,
		       NULL, NULL);

  gtk_range_calc_request (range, 
                          slider_width, stepper_size,
                          trough_border, stepper_spacing,
                          &range_rect, &border, &n_steppers,
                          &has_steppers_ab, &has_steppers_cd, &slider_length);
  
  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      clamp_dimensions (widget, &range_rect, &border, TRUE);
    }
  else
    {
      clamp_dimensions (widget, &range_rect, &border, FALSE);
    }
  
  range_rect.x = border.left;
  range_rect.y = border.top;

  priv->range_rect = range_rect;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint stepper_width, stepper_height;

      /* Steppers are the width of the range, and stepper_size in
       * height, or if we don't have enough height, divided equally
       * among available space.
       */
      stepper_width = range_rect.width;

      if (trough_under_steppers)
        stepper_width -= trough_border * 2;

      if (stepper_width < 1)
        stepper_width = range_rect.width; /* screw the trough border */

      if (n_steppers == 0)
        stepper_height = 0; /* avoid divide by n_steppers */
      else
        stepper_height = MIN (stepper_size, (range_rect.height / n_steppers));

      /* Stepper A */
      
      priv->stepper_a.x = range_rect.x + trough_border * trough_under_steppers;
      priv->stepper_a.y = range_rect.y + trough_border * trough_under_steppers;

      if (priv->has_stepper_a)
        {
          priv->stepper_a.width = stepper_width;
          priv->stepper_a.height = stepper_height;
        }
      else
        {
          priv->stepper_a.width = 0;
          priv->stepper_a.height = 0;
        }

      /* Stepper B */
      
      priv->stepper_b.x = priv->stepper_a.x;
      priv->stepper_b.y = priv->stepper_a.y + priv->stepper_a.height;

      if (priv->has_stepper_b)
        {
          priv->stepper_b.width = stepper_width;
          priv->stepper_b.height = stepper_height;
        }
      else
        {
          priv->stepper_b.width = 0;
          priv->stepper_b.height = 0;
        }

      /* Stepper D */

      if (priv->has_stepper_d)
        {
          priv->stepper_d.width = stepper_width;
          priv->stepper_d.height = stepper_height;
        }
      else
        {
          priv->stepper_d.width = 0;
          priv->stepper_d.height = 0;
        }
      
      priv->stepper_d.x = priv->stepper_a.x;
      priv->stepper_d.y = range_rect.y + range_rect.height - priv->stepper_d.height - trough_border * trough_under_steppers;

      /* Stepper C */

      if (priv->has_stepper_c)
        {
          priv->stepper_c.width = stepper_width;
          priv->stepper_c.height = stepper_height;
        }
      else
        {
          priv->stepper_c.width = 0;
          priv->stepper_c.height = 0;
        }
      
      priv->stepper_c.x = priv->stepper_a.x;
      priv->stepper_c.y = priv->stepper_d.y - priv->stepper_c.height;

      /* Now the trough is the remaining space between steppers B and C,
       * if any, minus spacing
       */
      priv->trough.x = range_rect.x;
      priv->trough.y = priv->stepper_b.y + priv->stepper_b.height + stepper_spacing * has_steppers_ab;
      priv->trough.width = range_rect.width;
      priv->trough.height = priv->stepper_c.y - priv->trough.y - stepper_spacing * has_steppers_cd;

      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      priv->slider.x = priv->trough.x + trough_border;
      priv->slider.width = priv->trough.width - trough_border * 2;

      /* Compute slider position/length */
      {
        gint y, bottom, top, height;
        
        top = priv->trough.y;
        bottom = priv->trough.y + priv->trough.height;

        if (! trough_under_steppers)
          {
            top += trough_border;
            bottom -= trough_border;
          }

        /* slider height is the fraction (page_size /
         * total_adjustment_range) times the trough height in pixels
         */

	if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) != 0)
	  height = ((bottom - top) * (gtk_adjustment_get_page_size (priv->adjustment) /
				       (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment))));
	else
          height = priv->min_slider_size;

        if (height < priv->min_slider_size ||
            priv->slider_size_fixed)
          height = priv->min_slider_size;

        height = MIN (height, priv->trough.height);
        
        y = top;

	if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment) != 0)
	  y += (bottom - top - height) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
					  (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment)));

        y = CLAMP (y, top, bottom);
        
        if (should_invert (range))
          y = bottom - (y - top + height);
        
        priv->slider.y = y;
        priv->slider.height = height;

        /* These are publically exported */
        priv->slider_start = priv->slider.y;
        priv->slider_end = priv->slider.y + priv->slider.height;
      }
    }
  else
    {
      gint stepper_width, stepper_height;

      /* Steppers are the height of the range, and stepper_size in
       * width, or if we don't have enough width, divided equally
       * among available space.
       */
      stepper_height = range_rect.height;

      if (trough_under_steppers)
        stepper_height -= trough_border * 2;

      if (stepper_height < 1)
        stepper_height = range_rect.height; /* screw the trough border */

      if (n_steppers == 0)
        stepper_width = 0; /* avoid divide by n_steppers */
      else
        stepper_width = MIN (stepper_size, (range_rect.width / n_steppers));

      /* Stepper A */
      
      priv->stepper_a.x = range_rect.x + trough_border * trough_under_steppers;
      priv->stepper_a.y = range_rect.y + trough_border * trough_under_steppers;

      if (priv->has_stepper_a)
        {
          priv->stepper_a.width = stepper_width;
          priv->stepper_a.height = stepper_height;
        }
      else
        {
          priv->stepper_a.width = 0;
          priv->stepper_a.height = 0;
        }

      /* Stepper B */
      
      priv->stepper_b.x = priv->stepper_a.x + priv->stepper_a.width;
      priv->stepper_b.y = priv->stepper_a.y;

      if (priv->has_stepper_b)
        {
          priv->stepper_b.width = stepper_width;
          priv->stepper_b.height = stepper_height;
        }
      else
        {
          priv->stepper_b.width = 0;
          priv->stepper_b.height = 0;
        }

      /* Stepper D */

      if (priv->has_stepper_d)
        {
          priv->stepper_d.width = stepper_width;
          priv->stepper_d.height = stepper_height;
        }
      else
        {
          priv->stepper_d.width = 0;
          priv->stepper_d.height = 0;
        }

      priv->stepper_d.x = range_rect.x + range_rect.width - priv->stepper_d.width - trough_border * trough_under_steppers;
      priv->stepper_d.y = priv->stepper_a.y;


      /* Stepper C */

      if (priv->has_stepper_c)
        {
          priv->stepper_c.width = stepper_width;
          priv->stepper_c.height = stepper_height;
        }
      else
        {
          priv->stepper_c.width = 0;
          priv->stepper_c.height = 0;
        }
      
      priv->stepper_c.x = priv->stepper_d.x - priv->stepper_c.width;
      priv->stepper_c.y = priv->stepper_a.y;

      /* Now the trough is the remaining space between steppers B and C,
       * if any
       */
      priv->trough.x = priv->stepper_b.x + priv->stepper_b.width + stepper_spacing * has_steppers_ab;
      priv->trough.y = range_rect.y;
      priv->trough.width = priv->stepper_c.x - priv->trough.x - stepper_spacing * has_steppers_cd;
      priv->trough.height = range_rect.height;

      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      priv->slider.y = priv->trough.y + trough_border;
      priv->slider.height = priv->trough.height - trough_border * 2;

      /* Compute slider position/length */
      {
        gint x, left, right, width;
        
        left = priv->trough.x;
        right = priv->trough.x + priv->trough.width;

        if (! trough_under_steppers)
          {
            left += trough_border;
            right -= trough_border;
          }

        /* slider width is the fraction (page_size /
         * total_adjustment_range) times the trough width in pixels
         */

	if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) != 0)
	  width = ((right - left) * (gtk_adjustment_get_page_size (priv->adjustment) /
                                   (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment))));
	else
	  width = priv->min_slider_size;

        if (width < priv->min_slider_size ||
            priv->slider_size_fixed)
          width = priv->min_slider_size;

        width = MIN (width, priv->trough.width);
        
        x = left;

	if (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment) != 0)
          x += (right - left - width) * ((adjustment_value - gtk_adjustment_get_lower (priv->adjustment)) /
                                         (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment)));
        
        x = CLAMP (x, left, right);
        
        if (should_invert (range))
          x = right - (x - left + width);
        
        priv->slider.x = x;
        priv->slider.width = width;

        /* These are publically exported */
        priv->slider_start = priv->slider.x;
        priv->slider_end = priv->slider.x + priv->slider.width;
      }
    }
  
  gtk_range_update_mouse_location (range);

  switch (priv->upper_sensitivity)
    {
    case GTK_SENSITIVITY_AUTO:
      priv->upper_sensitive =
        (gtk_adjustment_get_value (priv->adjustment) <
         (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_page_size (priv->adjustment)));
      break;

    case GTK_SENSITIVITY_ON:
      priv->upper_sensitive = TRUE;
      break;

    case GTK_SENSITIVITY_OFF:
      priv->upper_sensitive = FALSE;
      break;
    }

  switch (priv->lower_sensitivity)
    {
    case GTK_SENSITIVITY_AUTO:
      priv->lower_sensitive =
        (gtk_adjustment_get_value (priv->adjustment) > gtk_adjustment_get_lower (priv->adjustment));
      break;

    case GTK_SENSITIVITY_ON:
      priv->lower_sensitive = TRUE;
      break;

    case GTK_SENSITIVITY_OFF:
      priv->lower_sensitive = FALSE;
      break;
    }
}

static GdkRectangle*
get_area (GtkRange     *range,
          MouseLocation location)
{
  GtkRangePrivate *priv = range->priv;

  switch (location)
    {
    case MOUSE_STEPPER_A:
      return &priv->stepper_a;
    case MOUSE_STEPPER_B:
      return &priv->stepper_b;
    case MOUSE_STEPPER_C:
      return &priv->stepper_c;
    case MOUSE_STEPPER_D:
      return &priv->stepper_d;
    case MOUSE_TROUGH:
      return &priv->trough;
    case MOUSE_SLIDER:
      return &priv->slider;
    case MOUSE_WIDGET:
    case MOUSE_OUTSIDE:
      break;
    }

  g_warning (G_STRLOC": bug");
  return NULL;
}

static void
gtk_range_calc_marks (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;
  gint i;

  if (!priv->recalc_marks)
    return;

  priv->recalc_marks = FALSE;

  for (i = 0; i < priv->n_marks; i++)
    {
      priv->need_recalc = TRUE;
      gtk_range_calc_layout (range, priv->marks[i]);
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        priv->mark_pos[i] = priv->slider.x + priv->slider.width / 2;
      else
        priv->mark_pos[i] = priv->slider.y + priv->slider.height / 2;
    }

  priv->need_recalc = TRUE;
}

static gboolean
gtk_range_real_change_value (GtkRange      *range,
                             GtkScrollType  scroll,
                             gdouble        value)
{
  GtkRangePrivate *priv = range->priv;

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

  if (gtk_adjustment_get_value (priv->adjustment) != value)
    {
      priv->need_recalc = TRUE;

      gtk_widget_queue_draw (GTK_WIDGET (range));

      if (priv->in_drag)
        gtk_adjustment_set_value (priv->adjustment, value);
      else
        gtk_adjustment_animate_to_value (priv->adjustment, value);
    }

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
  GtkRangePrivate *priv = range->priv;

  gtk_range_scroll (range, priv->timer->step);

  return TRUE;
}

static gboolean
initial_timeout (gpointer data)
{
  GtkRange *range = GTK_RANGE (data);
  GtkRangePrivate *priv = range->priv;

  priv->timer->timeout_id = gdk_threads_add_timeout (TIMEOUT_REPEAT * SCROLL_DELAY_FACTOR,
                                                     second_timeout,
                                                     range);
  g_source_set_name_by_id (priv->timer->timeout_id, "[gtk+] second_timeout");
  /* remove self */
  return FALSE;
}

static void
gtk_range_add_step_timer (GtkRange      *range,
                          GtkScrollType  step)
{
  GtkRangePrivate *priv = range->priv;

  g_return_if_fail (priv->timer == NULL);
  g_return_if_fail (step != GTK_SCROLL_NONE);

  priv->timer = g_new (GtkRangeStepTimer, 1);

  priv->timer->timeout_id = gdk_threads_add_timeout (TIMEOUT_INITIAL,
                                                     initial_timeout,
                                                     range);
  g_source_set_name_by_id (priv->timer->timeout_id, "[gtk+] initial_timeout");
  priv->timer->step = step;

  gtk_range_scroll (range, priv->timer->step);
}

static void
gtk_range_remove_step_timer (GtkRange *range)
{
  GtkRangePrivate *priv = range->priv;

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
  range->priv->has_origin = has_origin;
}

gboolean
_gtk_range_get_has_origin (GtkRange *range)
{
  return range->priv->has_origin;
}

void
_gtk_range_set_stop_values (GtkRange *range,
                            gdouble  *values,
                            gint      n_values)
{
  GtkRangePrivate *priv = range->priv;
  gint i;

  g_free (priv->marks);
  priv->marks = g_new (gdouble, n_values);

  g_free (priv->mark_pos);
  priv->mark_pos = g_new (gint, n_values);

  priv->n_marks = n_values;

  for (i = 0; i < n_values; i++) 
    priv->marks[i] = values[i];

  priv->recalc_marks = TRUE;
}

gint
_gtk_range_get_stop_positions (GtkRange  *range,
                               gint     **values)
{
  GtkRangePrivate *priv = range->priv;

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
 *
 * Since: 2.24
 */
void
gtk_range_set_round_digits (GtkRange *range,
                            gint      round_digits)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (round_digits >= -1);

  if (range->priv->round_digits != round_digits)
    {
      range->priv->round_digits = round_digits;
      g_object_notify (G_OBJECT (range), "round-digits");
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
 *
 * Since: 2.24
 */
gint
gtk_range_get_round_digits (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), -1);

  return range->priv->round_digits;
}

void
_gtk_range_set_steppers (GtkRange      *range,
                         gboolean       has_a,
                         gboolean       has_b,
                         gboolean       has_c,
                         gboolean       has_d)
{
  range->priv->has_stepper_a = has_a;
  range->priv->has_stepper_b = has_b;
  range->priv->has_stepper_c = has_c;
  range->priv->has_stepper_d = has_d;
}
