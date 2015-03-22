/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkSpinButton widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

#include "gtkspinbutton.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>

#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkentryprivate.h"
#include "gtkiconhelperprivate.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkspinbuttonaccessible.h"

#define MIN_SPIN_BUTTON_WIDTH 30
#define MAX_TIMER_CALLS       5
#define EPSILON               1e-10
#define MAX_DIGITS            20
#define MIN_ARROW_WIDTH       6
#define TIMEOUT_INITIAL       500
#define TIMEOUT_REPEAT        50

/**
 * SECTION:gtkspinbutton
 * @Title: GtkSpinButton
 * @Short_description: Retrieve an integer or floating-point number from
 *     the user
 * @See_also: #GtkEntry
 *
 * A #GtkSpinButton is an ideal way to allow the user to set the value of
 * some attribute. Rather than having to directly type a number into a
 * #GtkEntry, GtkSpinButton allows the user to click on one of two arrows
 * to increment or decrement the displayed value. A value can still be
 * typed in, with the bonus that it can be checked to ensure it is in a
 * given range.
 *
 * The main properties of a GtkSpinButton are through an adjustment.
 * See the #GtkAdjustment section for more details about an adjustment's
 * properties.
 *
 * ## Using a GtkSpinButton to get an integer
 *
 * |[<!-- language="C" -->
 * // Provides a function to retrieve an integer value from a GtkSpinButton
 * // and creates a spin button to model percentage values.
 *
 * gint
 * grab_int_value (GtkSpinButton *button,
 *                 gpointer       user_data)
 * {
 *   return gtk_spin_button_get_value_as_int (button);
 * }
 *
 * void
 * create_integer_spin_button (void)
 * {
 *
 *   GtkWidget *window, *button;
 *   GtkAdjustment *adjustment;
 *
 *   adjustment = gtk_adjustment_new (50.0, 0.0, 100.0, 1.0, 5.0, 0.0);
 *
 *   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 *   gtk_container_set_border_width (GTK_CONTAINER (window), 5);
 *
 *   // creates the spinbutton, with no decimal places
 *   button = gtk_spin_button_new (adjustment, 1.0, 0);
 *   gtk_container_add (GTK_CONTAINER (window), button);
 *
 *   gtk_widget_show_all (window);
 * }
 * ]|
 *
 * ## Using a GtkSpinButton to get a floating point value
 *
 * |[<!-- language="C" -->
 * // Provides a function to retrieve a floating point value from a
 * // GtkSpinButton, and creates a high precision spin button.
 *
 * gfloat
 * grab_float_value (GtkSpinButton *button,
 *                   gpointer       user_data)
 * {
 *   return gtk_spin_button_get_value (button);
 * }
 *
 * void
 * create_floating_spin_button (void)
 * {
 *   GtkWidget *window, *button;
 *   GtkAdjustment *adjustment;
 *
 *   adjustment = gtk_adjustment_new (2.500, 0.0, 5.0, 0.001, 0.1, 0.0);
 *
 *   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 *   gtk_container_set_border_width (GTK_CONTAINER (window), 5);
 *
 *   // creates the spinbutton, with three decimal places
 *   button = gtk_spin_button_new (adjustment, 0.001, 3);
 *   gtk_container_add (GTK_CONTAINER (window), button);
 *
 *   gtk_widget_show_all (window);
 * }
 * ]|
 */

struct _GtkSpinButtonPrivate
{
  GtkAdjustment *adjustment;

  GdkWindow     *down_panel;
  GdkWindow     *up_panel;

  GtkStyleContext *down_panel_context;
  GtkStyleContext *up_panel_context;

  GdkWindow     *click_child;
  GdkWindow     *in_child;

  guint32        timer;

  GtkSpinButtonUpdatePolicy update_policy;

  gdouble        climb_rate;
  gdouble        timer_step;

  GtkOrientation orientation;

  GtkGesture *swipe_gesture;

  guint          button        : 2;
  guint          digits        : 10;
  guint          need_timer    : 1;
  guint          numeric       : 1;
  guint          snap_to_ticks : 1;
  guint          timer_calls   : 3;
  guint          wrap          : 1;
};

enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_CLIMB_RATE,
  PROP_DIGITS,
  PROP_SNAP_TO_TICKS,
  PROP_NUMERIC,
  PROP_WRAP,
  PROP_UPDATE_POLICY,
  PROP_VALUE,
  PROP_ORIENTATION
};

/* Signals */
enum
{
  INPUT,
  OUTPUT,
  VALUE_CHANGED,
  CHANGE_VALUE,
  WRAPPED,
  LAST_SIGNAL
};

static void gtk_spin_button_editable_init  (GtkEditableInterface *iface);
static void gtk_spin_button_finalize       (GObject            *object);
static void gtk_spin_button_set_property   (GObject         *object,
                                            guint            prop_id,
                                            const GValue    *value,
                                            GParamSpec      *pspec);
static void gtk_spin_button_get_property   (GObject         *object,
                                            guint            prop_id,
                                            GValue          *value,
                                            GParamSpec      *pspec);
static void gtk_spin_button_destroy        (GtkWidget          *widget);
static void gtk_spin_button_map            (GtkWidget          *widget);
static void gtk_spin_button_unmap          (GtkWidget          *widget);
static void gtk_spin_button_realize        (GtkWidget          *widget);
static void gtk_spin_button_unrealize      (GtkWidget          *widget);
static void gtk_spin_button_get_preferred_width  (GtkWidget          *widget,
                                                  gint               *minimum,
                                                  gint               *natural);
static void gtk_spin_button_get_preferred_height (GtkWidget          *widget,
                                                  gint               *minimum,
                                                  gint               *natural);
static void gtk_spin_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
									 gint       width,
									 gint      *minimum,
									 gint      *natural,
									 gint      *minimum_baseline,
									 gint      *natural_baseline);
static void gtk_spin_button_size_allocate  (GtkWidget          *widget,
                                            GtkAllocation      *allocation);
static gint gtk_spin_button_draw           (GtkWidget          *widget,
                                            cairo_t            *cr);
static gint gtk_spin_button_button_press   (GtkWidget          *widget,
                                            GdkEventButton     *event);
static gint gtk_spin_button_button_release (GtkWidget          *widget,
                                            GdkEventButton     *event);
static gint gtk_spin_button_motion_notify  (GtkWidget          *widget,
                                            GdkEventMotion     *event);
static gint gtk_spin_button_enter_notify   (GtkWidget          *widget,
                                            GdkEventCrossing   *event);
static gint gtk_spin_button_leave_notify   (GtkWidget          *widget,
                                            GdkEventCrossing   *event);
static gint gtk_spin_button_focus_out      (GtkWidget          *widget,
                                            GdkEventFocus      *event);
static void gtk_spin_button_grab_notify    (GtkWidget          *widget,
                                            gboolean            was_grabbed);
static void gtk_spin_button_state_flags_changed  (GtkWidget     *widget,
                                                  GtkStateFlags  previous_state);
static void gtk_spin_button_style_updated   (GtkWidget         *widget);
static gboolean gtk_spin_button_timer          (GtkSpinButton      *spin_button);
static gboolean gtk_spin_button_stop_spinning  (GtkSpinButton      *spin);
static void gtk_spin_button_value_changed  (GtkAdjustment      *adjustment,
                                            GtkSpinButton      *spin_button);
static gint gtk_spin_button_key_release    (GtkWidget          *widget,
                                            GdkEventKey        *event);
static gint gtk_spin_button_scroll         (GtkWidget          *widget,
                                            GdkEventScroll     *event);
static void gtk_spin_button_activate       (GtkEntry           *entry);
static void gtk_spin_button_get_text_area_size (GtkEntry *entry,
                                                gint     *x,
                                                gint     *y,
                                                gint     *width,
                                                gint     *height);
static void gtk_spin_button_get_frame_size (GtkEntry *entry,
                                            gint     *x,
                                            gint     *y,
                                            gint     *width,
                                            gint     *height);
static void gtk_spin_button_unset_adjustment (GtkSpinButton *spin_button);
static void gtk_spin_button_set_orientation (GtkSpinButton     *spin_button,
                                             GtkOrientation     orientation);
static void gtk_spin_button_snap           (GtkSpinButton      *spin_button,
                                            gdouble             val);
static void gtk_spin_button_insert_text    (GtkEditable        *editable,
                                            const gchar        *new_text,
                                            gint                new_text_length,
                                            gint               *position);
static void gtk_spin_button_real_spin      (GtkSpinButton      *spin_button,
                                            gdouble             step);
static void gtk_spin_button_real_change_value (GtkSpinButton   *spin,
                                               GtkScrollType    scroll);

static gint gtk_spin_button_default_input  (GtkSpinButton      *spin_button,
                                            gdouble            *new_val);
static void gtk_spin_button_default_output (GtkSpinButton      *spin_button);

static guint spinbutton_signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE_WITH_CODE (GtkSpinButton, gtk_spin_button, GTK_TYPE_ENTRY,
                         G_ADD_PRIVATE (GtkSpinButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_spin_button_editable_init))

#define add_spin_binding(binding_set, keyval, mask, scroll)            \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,             \
                                "change-value", 1,                     \
                                GTK_TYPE_SCROLL_TYPE, scroll)

static void
gtk_spin_button_class_init (GtkSpinButtonClass *class)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (class);
  GtkEntryClass    *entry_class = GTK_ENTRY_CLASS (class);
  GtkBindingSet    *binding_set;

  gobject_class->finalize = gtk_spin_button_finalize;
  gobject_class->set_property = gtk_spin_button_set_property;
  gobject_class->get_property = gtk_spin_button_get_property;

  widget_class->destroy = gtk_spin_button_destroy;
  widget_class->map = gtk_spin_button_map;
  widget_class->unmap = gtk_spin_button_unmap;
  widget_class->realize = gtk_spin_button_realize;
  widget_class->unrealize = gtk_spin_button_unrealize;
  widget_class->get_preferred_width = gtk_spin_button_get_preferred_width;
  widget_class->get_preferred_height = gtk_spin_button_get_preferred_height;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_spin_button_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = gtk_spin_button_size_allocate;
  widget_class->draw = gtk_spin_button_draw;
  widget_class->scroll_event = gtk_spin_button_scroll;
  widget_class->button_press_event = gtk_spin_button_button_press;
  widget_class->button_release_event = gtk_spin_button_button_release;
  widget_class->motion_notify_event = gtk_spin_button_motion_notify;
  widget_class->key_release_event = gtk_spin_button_key_release;
  widget_class->enter_notify_event = gtk_spin_button_enter_notify;
  widget_class->leave_notify_event = gtk_spin_button_leave_notify;
  widget_class->focus_out_event = gtk_spin_button_focus_out;
  widget_class->grab_notify = gtk_spin_button_grab_notify;
  widget_class->state_flags_changed = gtk_spin_button_state_flags_changed;
  widget_class->style_updated = gtk_spin_button_style_updated;

  entry_class->activate = gtk_spin_button_activate;
  entry_class->get_text_area_size = gtk_spin_button_get_text_area_size;
  entry_class->get_frame_size = gtk_spin_button_get_frame_size;

  class->input = NULL;
  class->output = NULL;
  class->change_value = gtk_spin_button_real_change_value;

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        P_("Adjustment"),
                                                        P_("The adjustment that holds the value of the spin button"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CLIMB_RATE,
                                   g_param_spec_double ("climb-rate",
                                                        P_("Climb Rate"),
                                                        P_("The acceleration rate when you hold down a button"),
                                                        0.0, G_MAXDOUBLE, 0.0,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_uint ("digits",
                                                      P_("Digits"),
                                                      P_("The number of decimal places to display"),
                                                      0, MAX_DIGITS, 0,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_TO_TICKS,
                                   g_param_spec_boolean ("snap-to-ticks",
                                                         P_("Snap to Ticks"),
                                                         P_("Whether erroneous values are automatically changed to a spin button's nearest step increment"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_NUMERIC,
                                   g_param_spec_boolean ("numeric",
                                                         P_("Numeric"),
                                                         P_("Whether non-numeric characters should be ignored"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                         P_("Wrap"),
                                                         P_("Whether a spin button should wrap upon reaching its limits"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_UPDATE_POLICY,
                                   g_param_spec_enum ("update-policy",
                                                      P_("Update Policy"),
                                                      P_("Whether the spin button should update always, or only when the value is legal"),
                                                      GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
                                                      GTK_UPDATE_ALWAYS,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
                                                        P_("Value"),
                                                        P_("Reads the current value, or sets a new value"),
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_override_property (gobject_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  gtk_widget_class_install_style_property_parser (widget_class,
                                                  g_param_spec_enum ("shadow-type",
                                                                     "Shadow Type",
                                                                     P_("Style of bevel around the spin button"),
                                                                     GTK_TYPE_SHADOW_TYPE,
                                                                     GTK_SHADOW_IN,
                                                                     GTK_PARAM_READABLE),
                                                  gtk_rc_property_parse_enum);

  /**
   * GtkSpinButton::input:
   * @spin_button: the object on which the signal was emitted
   * @new_value: (out) (type double): return location for the new value
   *
   * The ::input signal can be used to influence the conversion of
   * the users input into a double value. The signal handler is
   * expected to use gtk_entry_get_text() to retrieve the text of
   * the entry and set @new_value to the new value.
   *
   * The default conversion uses g_strtod().
   *
   * Returns: %TRUE for a successful conversion, %FALSE if the input
   *     was not handled, and %GTK_INPUT_ERROR if the conversion failed.
   */
  spinbutton_signals[INPUT] =
    g_signal_new (I_("input"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, input),
                  NULL, NULL,
                  _gtk_marshal_INT__POINTER,
                  G_TYPE_INT, 1,
                  G_TYPE_POINTER);

  /**
   * GtkSpinButton::output:
   * @spin_button: the object on which the signal was emitted
   *
   * The ::output signal can be used to change to formatting
   * of the value that is displayed in the spin buttons entry.
   * |[<!-- language="C" -->
   * // show leading zeros
   * static gboolean
   * on_output (GtkSpinButton *spin,
   *            gpointer       data)
   * {
   *    GtkAdjustment *adjustment;
   *    gchar *text;
   *    int value;
   *
   *    adjustment = gtk_spin_button_get_adjustment (spin);
   *    value = (int)gtk_adjustment_get_value (adjustment);
   *    text = g_strdup_printf ("%02d", value);
   *    gtk_entry_set_text (GTK_ENTRY (spin), text);
   *    g_free (text);
   *
   *    return TRUE;
   * }
   * ]|
   *
   * Returns: %TRUE if the value has been displayed
   */
  spinbutton_signals[OUTPUT] =
    g_signal_new (I_("output"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, output),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * GtkSpinButton::value-changed:
   * @spin_button: the object on which the signal was emitted
   *
   * The ::value-changed signal is emitted when the value represented by
   * @spinbutton changes. Also see the #GtkSpinButton::output signal.
   */
  spinbutton_signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, value_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkSpinButton::wrapped:
   * @spin_button: the object on which the signal was emitted
   *
   * The ::wrapped signal is emitted right after the spinbutton wraps
   * from its maximum to minimum value or vice-versa.
   *
   * Since: 2.10
   */
  spinbutton_signals[WRAPPED] =
    g_signal_new (I_("wrapped"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, wrapped),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* Action signals */
  /**
   * GtkSpinButton::change-value:
   * @spin_button: the object on which the signal was emitted
   * @scroll: a #GtkScrollType to specify the speed and amount of change
   *
   * The ::change-value signal is a [keybinding signal][GtkBindingSignal] 
   * which gets emitted when the user initiates a value change. 
   *
   * Applications should not connect to it, but may emit it with 
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal are Up/Down and PageUp and/PageDown.
   */
  spinbutton_signals[CHANGE_VALUE] =
    g_signal_new (I_("change-value"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, change_value),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);

  binding_set = gtk_binding_set_by_class (class);

  add_spin_binding (binding_set, GDK_KEY_Up, 0, GTK_SCROLL_STEP_UP);
  add_spin_binding (binding_set, GDK_KEY_KP_Up, 0, GTK_SCROLL_STEP_UP);
  add_spin_binding (binding_set, GDK_KEY_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_spin_binding (binding_set, GDK_KEY_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_spin_binding (binding_set, GDK_KEY_Page_Up, 0, GTK_SCROLL_PAGE_UP);
  add_spin_binding (binding_set, GDK_KEY_Page_Down, 0, GTK_SCROLL_PAGE_DOWN);
  add_spin_binding (binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK, GTK_SCROLL_END);
  add_spin_binding (binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_START);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SPIN_BUTTON_ACCESSIBLE);
}

static void
gtk_spin_button_editable_init (GtkEditableInterface *iface)
{
  iface->insert_text = gtk_spin_button_insert_text;
}

static void
gtk_spin_button_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  switch (prop_id)
    {
      GtkAdjustment *adjustment;

    case PROP_ADJUSTMENT:
      adjustment = GTK_ADJUSTMENT (g_value_get_object (value));
      gtk_spin_button_set_adjustment (spin_button, adjustment);
      break;
    case PROP_CLIMB_RATE:
      gtk_spin_button_configure (spin_button,
                                 priv->adjustment,
                                 g_value_get_double (value),
                                 priv->digits);
      break;
    case PROP_DIGITS:
      gtk_spin_button_configure (spin_button,
                                 priv->adjustment,
                                 priv->climb_rate,
                                 g_value_get_uint (value));
      break;
    case PROP_SNAP_TO_TICKS:
      gtk_spin_button_set_snap_to_ticks (spin_button, g_value_get_boolean (value));
      break;
    case PROP_NUMERIC:
      gtk_spin_button_set_numeric (spin_button, g_value_get_boolean (value));
      break;
    case PROP_WRAP:
      gtk_spin_button_set_wrap (spin_button, g_value_get_boolean (value));
      break;
    case PROP_UPDATE_POLICY:
      gtk_spin_button_set_update_policy (spin_button, g_value_get_enum (value));
      break;
    case PROP_VALUE:
      gtk_spin_button_set_value (spin_button, g_value_get_double (value));
      break;
    case PROP_ORIENTATION:
      gtk_spin_button_set_orientation (spin_button, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_spin_button_get_property (GObject      *object,
                              guint         prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;
    case PROP_CLIMB_RATE:
      g_value_set_double (value, priv->climb_rate);
      break;
    case PROP_DIGITS:
      g_value_set_uint (value, priv->digits);
      break;
    case PROP_SNAP_TO_TICKS:
      g_value_set_boolean (value, priv->snap_to_ticks);
      break;
    case PROP_NUMERIC:
      g_value_set_boolean (value, priv->numeric);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, priv->wrap);
      break;
    case PROP_UPDATE_POLICY:
      g_value_set_enum (value, priv->update_policy);
      break;
     case PROP_VALUE:
       g_value_set_double (value, gtk_adjustment_get_value (priv->adjustment));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
swipe_gesture_begin (GtkGesture       *gesture,
                     GdkEventSequence *sequence,
                     GtkSpinButton    *spin_button)
{
  GdkEventSequence *current;
  const GdkEvent *event;

  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (gesture, current);

  if (event->any.window == spin_button->priv->up_panel ||
      event->any.window == spin_button->priv->down_panel)
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_widget_grab_focus (GTK_WIDGET (spin_button));
}

static void
swipe_gesture_update (GtkGesture       *gesture,
                      GdkEventSequence *sequence,
                      GtkSpinButton    *spin_button)
{
  gdouble vel_y;

  gtk_gesture_swipe_get_velocity (GTK_GESTURE_SWIPE (gesture), NULL, &vel_y);
  gtk_spin_button_real_spin (spin_button, -vel_y / 20);
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv;
  GtkStyleContext *context;

  spin_button->priv = gtk_spin_button_get_instance_private (spin_button);
  priv = spin_button->priv;

  priv->adjustment = NULL;
  priv->down_panel = NULL;
  priv->up_panel = NULL;
  priv->timer = 0;
  priv->climb_rate = 0.0;
  priv->timer_step = 0.0;
  priv->update_policy = GTK_UPDATE_ALWAYS;
  priv->in_child = NULL;
  priv->click_child = NULL;
  priv->button = 0;
  priv->need_timer = FALSE;
  priv->timer_calls = 0;
  priv->digits = 0;
  priv->numeric = FALSE;
  priv->wrap = FALSE;
  priv->snap_to_ticks = FALSE;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_spin_button_set_adjustment (spin_button, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (spin_button));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SPINBUTTON);
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (spin_button));

  gtk_widget_add_events (GTK_WIDGET (spin_button), GDK_SCROLL_MASK);

  priv->swipe_gesture = gtk_gesture_swipe_new (GTK_WIDGET (spin_button));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->swipe_gesture), TRUE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->swipe_gesture),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (priv->swipe_gesture, "begin",
                    G_CALLBACK (swipe_gesture_begin), spin_button);
  g_signal_connect (priv->swipe_gesture, "update",
                    G_CALLBACK (swipe_gesture_update), spin_button);
}

static void
gtk_spin_button_finalize (GObject *object)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  gtk_spin_button_unset_adjustment (spin_button);

  if (priv->down_panel_context)
    g_object_unref (priv->down_panel_context);

  if (priv->up_panel_context)
    g_object_unref (priv->up_panel_context);

  g_object_unref (priv->swipe_gesture);

  G_OBJECT_CLASS (gtk_spin_button_parent_class)->finalize (object);
}

static void
gtk_spin_button_destroy (GtkWidget *widget)
{
  gtk_spin_button_stop_spinning (GTK_SPIN_BUTTON (widget));

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->destroy (widget);
}

static void
gtk_spin_button_map (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  if (gtk_widget_get_realized (widget) && !gtk_widget_get_mapped (widget))
    {
      GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->map (widget);
      gdk_window_show (priv->down_panel);
      gdk_window_show (priv->up_panel);
    }
}

static void
gtk_spin_button_unmap (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  if (gtk_widget_get_mapped (widget))
    {
      gtk_spin_button_stop_spinning (GTK_SPIN_BUTTON (widget));

      gdk_window_hide (priv->down_panel);
      gdk_window_hide (priv->up_panel);
      GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->unmap (widget);
    }
}

static void
gtk_spin_button_panel_nthchildize_context (GtkSpinButton *spin_button,
                                           GtkStyleContext *context,
                                           gboolean         is_down_panel)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkWidget *widget = GTK_WIDGET (spin_button);
  GtkWidgetPath *path, *siblings_path;
  GtkTextDirection direction;
  gint up_pos, down_pos;

  /* We are a subclass of GtkEntry, which is not a GtkContainer, so we
   * have to emulate what gtk_container_get_path_for_child() would do
   * for the button panels
   */
  path = _gtk_widget_create_path (widget);
  siblings_path = gtk_widget_path_new ();

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      direction = gtk_widget_get_direction (widget);

      /* flip children order for RTL */
      if (direction == GTK_TEXT_DIR_RTL)
        {
          up_pos = gtk_widget_path_append_type (siblings_path, GTK_TYPE_SPIN_BUTTON);
          down_pos = gtk_widget_path_append_type (siblings_path, GTK_TYPE_SPIN_BUTTON);
          gtk_widget_path_append_type (siblings_path, GTK_TYPE_ENTRY);
        }
      else
        {
          gtk_widget_path_append_type (siblings_path, GTK_TYPE_ENTRY);
          down_pos = gtk_widget_path_append_type (siblings_path, GTK_TYPE_SPIN_BUTTON);
          up_pos = gtk_widget_path_append_type (siblings_path, GTK_TYPE_SPIN_BUTTON);
        }
    }
  else
    {
      up_pos = gtk_widget_path_append_type (siblings_path, GTK_TYPE_SPIN_BUTTON);
      gtk_widget_path_append_type (siblings_path, GTK_TYPE_ENTRY);
      down_pos = gtk_widget_path_append_type (siblings_path, GTK_TYPE_SPIN_BUTTON);
    }

  gtk_widget_path_iter_add_class (siblings_path, up_pos, GTK_STYLE_CLASS_BUTTON);
  gtk_widget_path_iter_add_class (siblings_path, down_pos, GTK_STYLE_CLASS_BUTTON);

  /* this allows compatibility for themes that use .spinbutton.button */
  gtk_widget_path_iter_add_class (siblings_path, up_pos, GTK_STYLE_CLASS_SPINBUTTON);
  gtk_widget_path_iter_add_class (siblings_path, down_pos, GTK_STYLE_CLASS_SPINBUTTON);

  if (is_down_panel)
    gtk_widget_path_append_with_siblings (path, siblings_path, down_pos);
  else
    gtk_widget_path_append_with_siblings (path, siblings_path, up_pos);

  gtk_style_context_set_path (context, path);
  gtk_widget_path_unref (path);
  gtk_widget_path_unref (siblings_path);
}

static gboolean
gtk_spin_button_panel_at_limit (GtkSpinButton *spin_button,
                                GdkWindow     *panel)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GdkWindow *effective_panel;

  if (priv->wrap)
    return FALSE;

  if (gtk_adjustment_get_step_increment (priv->adjustment) > 0)
    effective_panel = panel;
  else
    effective_panel = panel == priv->up_panel ? priv->down_panel : priv->up_panel;

  if (effective_panel == priv->up_panel &&
      (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_value (priv->adjustment) <= EPSILON))
    return TRUE;

  if (effective_panel == priv->down_panel &&
      (gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) <= EPSILON))
    return TRUE;

  return FALSE;
}

static GtkStateFlags
gtk_spin_button_panel_get_state (GtkSpinButton *spin_button,
                                 GdkWindow *panel)
{
  GtkStateFlags state;
  GtkSpinButtonPrivate *priv = spin_button->priv;

  state = gtk_widget_get_state_flags (GTK_WIDGET (spin_button));

  state &= ~(GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT);

  if ((state & GTK_STATE_FLAG_INSENSITIVE) ||
      gtk_spin_button_panel_at_limit (spin_button, panel) ||
      !gtk_editable_get_editable (GTK_EDITABLE (spin_button)))
    {
      state |= GTK_STATE_FLAG_INSENSITIVE;
    }
  else
    {
      if (priv->click_child == panel)
        state |= GTK_STATE_FLAG_ACTIVE;
      else if (priv->in_child == panel &&
               priv->click_child == NULL)
        state |= GTK_STATE_FLAG_PRELIGHT;
    }

  return state;
}

static GtkStyleContext *
gtk_spin_button_panel_get_context (GtkSpinButton *spin_button,
                                   GdkWindow *panel)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkStyleContext **contextp;

  contextp = (panel == priv->down_panel) ?
    &priv->down_panel_context : &priv->up_panel_context;

  if (*contextp == NULL)
    {
      *contextp = gtk_style_context_new ();
      gtk_spin_button_panel_nthchildize_context (spin_button, *contextp,
                                                 panel == priv->down_panel);
    }

  return *contextp;
}

static void
gtk_spin_button_panel_get_size (GtkSpinButton *spin_button,
                                GdkWindow *panel,
                                gint *width,
                                gint *height)
{
  GtkBorder button_padding, button_border;
  GtkStyleContext *context;
  GtkStateFlags state;
  gint icon_size, w, h;

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
  icon_size = MAX (w, h);

  context = gtk_spin_button_panel_get_context (spin_button, panel);
  state = gtk_spin_button_panel_get_state (spin_button, panel);

  gtk_style_context_get_padding (context, state, &button_padding);
  gtk_style_context_get_border (context, state, &button_border);

  if (width)
    *width = icon_size + button_padding.left + button_padding.right +
             button_border.left + button_border.right;

  if (height)
    *height = icon_size + button_padding.top + button_padding.bottom +
              button_border.top + button_border.bottom;
}

static void
gtk_spin_button_panel_get_allocations (GtkSpinButton *spin_button,
                                       GtkAllocation *down_allocation_out,
                                       GtkAllocation *up_allocation_out)
{
  GtkWidget *widget = GTK_WIDGET (spin_button);
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkAllocation spin_allocation, down_allocation, up_allocation, allocation;
  GtkRequisition requisition;
  gint req_height;
  gint down_panel_width, down_panel_height;
  gint up_panel_width, up_panel_height;
  GtkStyleContext *context;
  GtkBorder border;

  gtk_widget_get_allocation (widget, &spin_allocation);
  gtk_widget_get_preferred_size (widget, &requisition, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (spin_button));
  gtk_style_context_get_border (context, GTK_STATE_FLAG_NORMAL, &border);

  gtk_spin_button_panel_get_size (spin_button, priv->down_panel, &down_panel_width, &down_panel_height);
  gtk_spin_button_panel_get_size (spin_button, priv->up_panel, &up_panel_width, &up_panel_height);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      req_height = requisition.height - gtk_widget_get_margin_top (widget) - gtk_widget_get_margin_bottom (widget);

      /* both panels have the same size, and they're as tall as the entry allocation,
       * excluding margins
       */
      allocation.height = MIN (req_height, spin_allocation.height) - border.top - border.bottom;
      allocation.y = spin_allocation.y + border.top + (spin_allocation.height - req_height) / 2;
      down_allocation = up_allocation = allocation;

      down_allocation.width = down_panel_width;
      up_allocation.width = up_panel_width;

      /* invert x axis allocation for RTL */
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        {
          up_allocation.x = spin_allocation.x + border.left;
          down_allocation.x = up_allocation.x + up_panel_width;
        }
      else
        {
          up_allocation.x = spin_allocation.x + spin_allocation.width - up_panel_width - border.right;
          down_allocation.x = up_allocation.x - down_panel_width;
        }
    }
  else
    {
      /* both panels have the same size, and they're as wide as the entry allocation */
      allocation.width = spin_allocation.width;
      allocation.x = spin_allocation.x;
      down_allocation = up_allocation = allocation;

      down_allocation.height = down_panel_height;
      up_allocation.height = up_panel_height;

      up_allocation.y = spin_allocation.y;
      down_allocation.y = spin_allocation.y + spin_allocation.height - down_allocation.height;
    }

  if (down_allocation_out)
    *down_allocation_out = down_allocation;
  if (up_allocation_out)
    *up_allocation_out = up_allocation;
}

static void
gtk_spin_button_panel_draw (GtkSpinButton   *spin_button,
                            cairo_t         *cr,
                            GdkWindow       *panel)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkWidget *widget;
  gdouble width, height, x, y;
  gint icon_width, icon_height;
  GtkIconHelper *icon_helper;

  widget = GTK_WIDGET (spin_button);

  cairo_save (cr);
  gtk_cairo_transform_to_window (cr, widget, panel);

  context = gtk_spin_button_panel_get_context (spin_button, panel);
  state = gtk_spin_button_panel_get_state (spin_button, panel);
  gtk_style_context_set_state (context, state);

  height = gdk_window_get_height (panel);
  width = gdk_window_get_width (panel);

  icon_helper = _gtk_icon_helper_new ();
  _gtk_icon_helper_set_window (icon_helper, panel);
  _gtk_icon_helper_set_use_fallback (icon_helper, TRUE);

  if (panel == priv->down_panel)
    _gtk_icon_helper_set_icon_name (icon_helper, "list-remove-symbolic", GTK_ICON_SIZE_MENU);
  else
    _gtk_icon_helper_set_icon_name (icon_helper, "list-add-symbolic", GTK_ICON_SIZE_MENU);

  _gtk_icon_helper_get_size (icon_helper, context,
                             &icon_width, &icon_height);

  gtk_render_background (context, cr,
                         0, 0, width, height);
  gtk_render_frame (context, cr,
                    0, 0, width, height);

  x = floor ((width - icon_width) / 2.0);
  y = floor ((height - icon_height) / 2.0);

  _gtk_icon_helper_draw (icon_helper, context, cr,
                         x, y);
  cairo_restore (cr);

  g_object_unref (icon_helper);
}

static void
gtk_spin_button_realize (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkAllocation down_allocation, up_allocation;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gboolean return_val;

  gtk_widget_set_events (widget, gtk_widget_get_events (widget) |
                         GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->realize (widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
    | GDK_POINTER_MOTION_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  gtk_spin_button_panel_get_allocations (spin_button, &down_allocation, &up_allocation);

  /* create the left panel window */
  attributes.x = down_allocation.x;
  attributes.y = down_allocation.y;
  attributes.width = down_allocation.width;
  attributes.height = down_allocation.height;

  priv->down_panel = gdk_window_new (gtk_widget_get_window (widget),
                                     &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->down_panel);

  /* create the right panel window */
  attributes.x = up_allocation.x;
  attributes.y = up_allocation.y;
  attributes.width = up_allocation.width;
  attributes.height = up_allocation.height;

  priv->up_panel = gdk_window_new (gtk_widget_get_window (widget),
                                      &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->up_panel);

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);

  /* If output wasn't processed explicitly by the method connected to the
   * 'output' signal; and if we don't have any explicit 'text' set initially,
   * fallback to the default output. */
  if (!return_val &&
      (spin_button->priv->numeric || gtk_entry_get_text (GTK_ENTRY (spin_button)) == NULL))
    gtk_spin_button_default_output (spin_button);

  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

static void
gtk_spin_button_unrealize (GtkWidget *widget)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->unrealize (widget);

  if (priv->down_panel)
    {
      gtk_widget_unregister_window (widget, priv->down_panel);
      gdk_window_destroy (priv->down_panel);
      priv->down_panel = NULL;
    }

  if (priv->up_panel)
    {
      gtk_widget_unregister_window (widget, priv->up_panel);
      gdk_window_destroy (priv->up_panel);
      priv->up_panel = NULL;
    }
}

/* Callback used when the spin button's adjustment changes.
 * We need to redraw the arrows when the adjustment’s range
 * changes, and reevaluate our size request.
 */
static void
adjustment_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (data);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);
  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

static void
gtk_spin_button_unset_adjustment (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;

  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            gtk_spin_button_value_changed,
                                            spin_button);
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            adjustment_changed_cb,
                                            spin_button);
      g_clear_object (&priv->adjustment);
    }
}

static void
gtk_spin_button_set_orientation (GtkSpinButton *spin,
                                 GtkOrientation orientation)
{
  GtkEntry *entry = GTK_ENTRY (spin);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (spin));

  /* change alignment if it's the default */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL &&
      gtk_entry_get_alignment (entry) == 0.0)
    gtk_entry_set_alignment (entry, 0.5);
  else if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
           gtk_entry_get_alignment (entry) == 0.5)
    gtk_entry_set_alignment (entry, 0.0);

  g_object_notify (G_OBJECT (spin), "orientation");
  gtk_widget_queue_resize (GTK_WIDGET (spin));
}

static gint
measure_string_width (PangoLayout *layout,
                      const gchar *string)
{
  gint width;

  pango_layout_set_text (layout, string, -1);
  pango_layout_get_pixel_size (layout, &width, NULL);

  return width;
}

static gchar *
gtk_spin_button_format_for_value (GtkSpinButton *spin_button,
                                  gdouble        value)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  gchar *buf = g_strdup_printf ("%0.*f", priv->digits, value);

  return buf;
}

static void
gtk_spin_button_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkEntry *entry = GTK_ENTRY (widget);

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->get_preferred_width (widget, minimum, natural);

  if (gtk_entry_get_width_chars (entry) < 0)
    {
      gint width, w;
      GtkBorder borders;
      PangoLayout *layout;
      gchar *str;

      layout = pango_layout_copy (gtk_entry_get_layout (entry));

      /* Get max of MIN_SPIN_BUTTON_WIDTH, size of upper, size of lower */
      width = MIN_SPIN_BUTTON_WIDTH;

      str = gtk_spin_button_format_for_value (spin_button,
                                              gtk_adjustment_get_upper (priv->adjustment));
      w = measure_string_width (layout, str);
      width = MAX (width, w);
      g_free (str);

      str = gtk_spin_button_format_for_value (spin_button,
                                              gtk_adjustment_get_lower (priv->adjustment));
      w = measure_string_width (layout, str);
      width = MAX (width, w);
      g_free (str);

      _gtk_entry_get_borders (entry, &borders);
      width += borders.left + borders.right;

      *minimum = width;
      *natural = width;

      g_object_unref (layout);
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint down_panel_width;
      gint up_panel_width;

      gtk_spin_button_panel_get_size (spin_button, priv->down_panel, &down_panel_width, NULL);
      gtk_spin_button_panel_get_size (spin_button, priv->up_panel, &up_panel_width, NULL);

      *minimum += up_panel_width + down_panel_width;
      *natural += up_panel_width + down_panel_width;
    }
}

static void
gtk_spin_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
							     gint       width,
							     gint      *minimum,
							     gint      *natural,
							     gint      *minimum_baseline,
							     gint      *natural_baseline)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->get_preferred_height_and_baseline_for_width (widget, width,
												minimum, natural,
												minimum_baseline,
												natural_baseline);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint down_panel_height;
      gint up_panel_height;

      gtk_spin_button_panel_get_size (spin_button, priv->down_panel, NULL, &down_panel_height);
      gtk_spin_button_panel_get_size (spin_button, priv->up_panel, NULL, &up_panel_height);

      *minimum += up_panel_height + down_panel_height;
      *natural += up_panel_height + down_panel_height;

      if (minimum_baseline && *minimum_baseline != -1)
	*minimum_baseline += up_panel_height;
      if (natural_baseline && *natural_baseline != -1)
	*natural_baseline += up_panel_height;
    }
}

static void
gtk_spin_button_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  gtk_spin_button_get_preferred_height_and_baseline_for_width (widget, -1, minimum, natural, NULL, NULL);
}

static void
gtk_spin_button_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;
  GtkAllocation down_panel_allocation, up_panel_allocation;

  gtk_widget_set_allocation (widget, allocation);
  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->size_allocate (widget, allocation);

  gtk_spin_button_panel_get_allocations (spin, &down_panel_allocation, &up_panel_allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->down_panel,
                              down_panel_allocation.x,
                              down_panel_allocation.y,
                              down_panel_allocation.width,
                              down_panel_allocation.height);

      gdk_window_move_resize (priv->up_panel,
                              up_panel_allocation.x,
                              up_panel_allocation.y,
                              up_panel_allocation.width,
                              up_panel_allocation.height);
    }

  gtk_widget_queue_draw (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_draw (GtkWidget      *widget,
                      cairo_t        *cr)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->draw (widget, cr);

  /* Draw the buttons */
  gtk_spin_button_panel_draw (spin, cr, priv->down_panel);
  gtk_spin_button_panel_draw (spin, cr, priv->up_panel);

  return FALSE;
}

static gint
gtk_spin_button_enter_notify (GtkWidget        *widget,
                              GdkEventCrossing *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if ((event->window == priv->down_panel) ||
      (event->window == priv->up_panel))
    {
      priv->in_child = event->window;
      gtk_widget_queue_draw (GTK_WIDGET (spin));
    }

  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->enter_notify_event (widget, event);
}

static gint
gtk_spin_button_leave_notify (GtkWidget        *widget,
                              GdkEventCrossing *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (priv->in_child != NULL)
    {
      priv->in_child = NULL;
      gtk_widget_queue_draw (GTK_WIDGET (spin));
    }

  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->leave_notify_event (widget, event);
}

static gint
gtk_spin_button_focus_out (GtkWidget     *widget,
                           GdkEventFocus *event)
{
  if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
    gtk_spin_button_update (GTK_SPIN_BUTTON (widget));

  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->focus_out_event (widget, event);
}

static void
gtk_spin_button_grab_notify (GtkWidget *widget,
                             gboolean   was_grabbed)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (!was_grabbed)
    {
      if (gtk_spin_button_stop_spinning (spin))
        gtk_widget_queue_draw (GTK_WIDGET (spin));
    }
}

static void
gtk_spin_button_state_flags_changed (GtkWidget     *widget,
                                     GtkStateFlags  previous_state)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  if (!gtk_widget_is_sensitive (widget))
    {
      if (gtk_spin_button_stop_spinning (spin))
        gtk_widget_queue_draw (GTK_WIDGET (spin));
    }
}

static void
gtk_spin_button_style_updated (GtkWidget *widget)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (priv->down_panel_context)
    gtk_spin_button_panel_nthchildize_context (spin,
                                               priv->down_panel_context,
                                               TRUE);

  if (priv->up_panel_context)
    gtk_spin_button_panel_nthchildize_context (spin,
                                               priv->up_panel_context,
                                               FALSE);

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->style_updated (widget);
}

static gint
gtk_spin_button_scroll (GtkWidget      *widget,
                        GdkEventScroll *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (event->direction == GDK_SCROLL_UP)
    {
      if (!gtk_widget_has_focus (widget))
        gtk_widget_grab_focus (widget);
      gtk_spin_button_real_spin (spin, gtk_adjustment_get_step_increment (priv->adjustment));
    }
  else if (event->direction == GDK_SCROLL_DOWN)
    {
      if (!gtk_widget_has_focus (widget))
        gtk_widget_grab_focus (widget);
      gtk_spin_button_real_spin (spin, -gtk_adjustment_get_step_increment (priv->adjustment));
    }
  else
    return FALSE;

  return TRUE;
}

static gboolean
gtk_spin_button_stop_spinning (GtkSpinButton *spin)
{
  GtkSpinButtonPrivate *priv = spin->priv;
  gboolean did_spin = FALSE;

  if (priv->timer)
    {
      g_source_remove (priv->timer);
      priv->timer = 0;
      priv->need_timer = FALSE;

      did_spin = TRUE;
    }

  priv->button = 0;
  priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);
  priv->timer_calls = 0;

  priv->click_child = NULL;

  return did_spin;
}

static void
start_spinning (GtkSpinButton *spin,
                GdkWindow     *click_child,
                gdouble        step)
{
  GtkSpinButtonPrivate *priv;

  priv = spin->priv;

  priv->click_child = click_child;

  if (!priv->timer)
    {
      priv->timer_step = step;
      priv->need_timer = TRUE;
      priv->timer = gdk_threads_add_timeout (TIMEOUT_INITIAL,
                                   (GSourceFunc) gtk_spin_button_timer,
                                   (gpointer) spin);
      g_source_set_name_by_id (priv->timer, "[gtk+] gtk_spin_button_timer");
    }
  gtk_spin_button_real_spin (spin, click_child == priv->up_panel ? step : -step);

  gtk_widget_queue_draw (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (!priv->button)
    {
      if ((event->window == priv->down_panel) ||
          (event->window == priv->up_panel))
        {
          if (!gtk_widget_has_focus (widget))
            gtk_widget_grab_focus (widget);
          priv->button = event->button;

          if (gtk_editable_get_editable (GTK_EDITABLE (widget))) {
            gtk_spin_button_update (spin);

            if (event->button == GDK_BUTTON_PRIMARY)
              start_spinning (spin, event->window, gtk_adjustment_get_step_increment (priv->adjustment));
            else if (event->button == GDK_BUTTON_MIDDLE)
              start_spinning (spin, event->window, gtk_adjustment_get_page_increment (priv->adjustment));
            else
              priv->click_child = event->window;
          } else
            gtk_widget_error_bell (widget);

          return TRUE;
        }
      else
        {
          return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->button_press_event (widget, event);
        }
    }
  return FALSE;
}

static gint
gtk_spin_button_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (event->button == priv->button)
    {
      GdkWindow *click_child = priv->click_child;

      gtk_spin_button_stop_spinning (spin);

      if (event->button == GDK_BUTTON_SECONDARY)
        {
          gdouble diff;

          if (event->window == priv->down_panel &&
              click_child == event->window)
            {
              diff = gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment);
              if (diff > EPSILON)
                gtk_spin_button_real_spin (spin, -diff);
            }
          else if (event->window == priv->up_panel &&
                   click_child == event->window)
            {
              diff = gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_value (priv->adjustment);
              if (diff > EPSILON)
                gtk_spin_button_real_spin (spin, diff);
            }
        }
      gtk_widget_queue_draw (GTK_WIDGET (spin));

      return TRUE;
    }
  else
    {
      return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->button_release_event (widget, event);
    }
}

static gint
gtk_spin_button_motion_notify (GtkWidget      *widget,
                               GdkEventMotion *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (priv->button)
    return FALSE;

  if (event->window == priv->down_panel ||
      event->window == priv->up_panel)
    {
      gdk_event_request_motions (event);

      priv->in_child = event->window;
      gtk_widget_queue_draw (widget);

      return FALSE;
    }

  if (gtk_gesture_is_recognized (priv->swipe_gesture))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->motion_notify_event (widget, event);
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  gboolean retval = FALSE;

  if (priv->timer)
    {
      if (priv->click_child == priv->up_panel)
        gtk_spin_button_real_spin (spin_button, priv->timer_step);
      else
        gtk_spin_button_real_spin (spin_button, -priv->timer_step);

      if (priv->need_timer)
        {
          priv->need_timer = FALSE;
          priv->timer = gdk_threads_add_timeout (TIMEOUT_REPEAT,
                                              (GSourceFunc) gtk_spin_button_timer,
                                              (gpointer) spin_button);
          g_source_set_name_by_id (priv->timer, "[gtk+] gtk_spin_button_timer");
        }
      else
        {
          if (priv->climb_rate > 0.0 && priv->timer_step
              < gtk_adjustment_get_page_increment (priv->adjustment))
            {
              if (priv->timer_calls < MAX_TIMER_CALLS)
                priv->timer_calls++;
              else
                {
                  priv->timer_calls = 0;
                  priv->timer_step += priv->climb_rate;
                }
            }
          retval = TRUE;
        }
    }

  return retval;
}

static void
gtk_spin_button_value_changed (GtkAdjustment *adjustment,
                               GtkSpinButton *spin_button)
{
  gboolean return_val;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);
  if (return_val == FALSE)
    gtk_spin_button_default_output (spin_button);

  g_signal_emit (spin_button, spinbutton_signals[VALUE_CHANGED], 0);

  gtk_widget_queue_draw (GTK_WIDGET (spin_button));

  g_object_notify (G_OBJECT (spin_button), "value");
}

static void
gtk_spin_button_real_change_value (GtkSpinButton *spin,
                                   GtkScrollType  scroll)
{
  GtkSpinButtonPrivate *priv = spin->priv;
  gdouble old_value;

  if (!gtk_editable_get_editable (GTK_EDITABLE (spin)))
    {
      gtk_widget_error_bell (GTK_WIDGET (spin));
      return;
    }

  /* When the key binding is activated, there may be an outstanding
   * value, so we first have to commit what is currently written in
   * the spin buttons text entry. See #106574
   */
  gtk_spin_button_update (spin);

  old_value = gtk_adjustment_get_value (priv->adjustment);

  switch (scroll)
    {
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_DOWN:
    case GTK_SCROLL_STEP_LEFT:
      gtk_spin_button_real_spin (spin, -priv->timer_step);

      if (priv->climb_rate > 0.0 && priv->timer_step
          < gtk_adjustment_get_page_increment (priv->adjustment))
        {
          if (priv->timer_calls < MAX_TIMER_CALLS)
            priv->timer_calls++;
          else
            {
              priv->timer_calls = 0;
              priv->timer_step += priv->climb_rate;
            }
        }
      break;

    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_STEP_UP:
    case GTK_SCROLL_STEP_RIGHT:
      gtk_spin_button_real_spin (spin, priv->timer_step);

      if (priv->climb_rate > 0.0 && priv->timer_step
          < gtk_adjustment_get_page_increment (priv->adjustment))
        {
          if (priv->timer_calls < MAX_TIMER_CALLS)
            priv->timer_calls++;
          else
            {
              priv->timer_calls = 0;
              priv->timer_step += priv->climb_rate;
            }
        }
      break;

    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_DOWN:
    case GTK_SCROLL_PAGE_LEFT:
      gtk_spin_button_real_spin (spin, -gtk_adjustment_get_page_increment (priv->adjustment));
      break;

    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_RIGHT:
      gtk_spin_button_real_spin (spin, gtk_adjustment_get_page_increment (priv->adjustment));
      break;

    case GTK_SCROLL_START:
      {
        gdouble diff = gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment);
        if (diff > EPSILON)
          gtk_spin_button_real_spin (spin, -diff);
        break;
      }

    case GTK_SCROLL_END:
      {
        gdouble diff = gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_value (priv->adjustment);
        if (diff > EPSILON)
          gtk_spin_button_real_spin (spin, diff);
        break;
      }

    default:
      g_warning ("Invalid scroll type %d for GtkSpinButton::change-value", scroll);
      break;
    }

  gtk_spin_button_update (spin);

  if (gtk_adjustment_get_value (priv->adjustment) == old_value)
    gtk_widget_error_bell (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_key_release (GtkWidget   *widget,
                             GdkEventKey *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  /* We only get a release at the end of a key repeat run, so reset the timer_step */
  priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);
  priv->timer_calls = 0;

  return TRUE;
}

static void
gtk_spin_button_snap (GtkSpinButton *spin_button,
                      gdouble        val)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  gdouble inc;
  gdouble tmp;

  inc = gtk_adjustment_get_step_increment (priv->adjustment);
  if (inc == 0)
    return;

  tmp = (val - gtk_adjustment_get_lower (priv->adjustment)) / inc;
  if (tmp - floor (tmp) < ceil (tmp) - tmp)
    val = gtk_adjustment_get_lower (priv->adjustment) + floor (tmp) * inc;
  else
    val = gtk_adjustment_get_lower (priv->adjustment) + ceil (tmp) * inc;

  gtk_spin_button_set_value (spin_button, val);
}

static void
gtk_spin_button_activate (GtkEntry *entry)
{
  if (gtk_editable_get_editable (GTK_EDITABLE (entry)))
    gtk_spin_button_update (GTK_SPIN_BUTTON (entry));

  /* Chain up so that entry->activates_default is honored */
  GTK_ENTRY_CLASS (gtk_spin_button_parent_class)->activate (entry);
}

static void
gtk_spin_button_get_frame_size (GtkEntry *entry,
                                gint     *x,
                                gint     *y,
                                gint     *width,
                                gint     *height)
{
  GtkSpinButtonPrivate *priv = GTK_SPIN_BUTTON (entry)->priv;
  gint up_panel_width, up_panel_height;
  gint down_panel_width, down_panel_height;

  gtk_spin_button_panel_get_size (GTK_SPIN_BUTTON (entry), priv->up_panel, &up_panel_width, &up_panel_height);
  gtk_spin_button_panel_get_size (GTK_SPIN_BUTTON (entry), priv->down_panel, &down_panel_width, &down_panel_height);

  GTK_ENTRY_CLASS (gtk_spin_button_parent_class)->get_frame_size (entry, x, y, width, height);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (y)
        *y += up_panel_height;

      if (height)
        *height -= up_panel_height + down_panel_height;
    }
}

static void
gtk_spin_button_get_text_area_size (GtkEntry *entry,
                                    gint     *x,
                                    gint     *y,
                                    gint     *width,
                                    gint     *height)
{
  GtkSpinButtonPrivate *priv = GTK_SPIN_BUTTON (entry)->priv;
  gint up_panel_width, up_panel_height;
  gint down_panel_width, down_panel_height;

  gtk_spin_button_panel_get_size (GTK_SPIN_BUTTON (entry), priv->up_panel, &up_panel_width, &up_panel_height);
  gtk_spin_button_panel_get_size (GTK_SPIN_BUTTON (entry), priv->down_panel, &down_panel_width, &down_panel_height);

  GTK_ENTRY_CLASS (gtk_spin_button_parent_class)->get_text_area_size (entry, x, y, width, height);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
        {
          if (x)
            *x += up_panel_width + down_panel_width;
        }

      if (width)
        *width -= up_panel_width + down_panel_width;
    }
  else
    {
      if (y)
        *y += up_panel_height;

      if (height)
        *height -= up_panel_height + down_panel_height;
    }
}

static void
gtk_spin_button_insert_text (GtkEditable *editable,
                             const gchar *new_text,
                             gint         new_text_length,
                             gint        *position)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  GtkSpinButton *spin = GTK_SPIN_BUTTON (editable);
  GtkSpinButtonPrivate *priv = spin->priv;
  GtkEditableInterface *parent_editable_iface;

  parent_editable_iface = g_type_interface_peek (gtk_spin_button_parent_class,
                                                 GTK_TYPE_EDITABLE);

  if (priv->numeric)
    {
      struct lconv *lc;
      gboolean sign;
      gint dotpos = -1;
      gint i;
      guint32 pos_sign;
      guint32 neg_sign;
      gint entry_length;
      const gchar *entry_text;

      entry_length = gtk_entry_get_text_length (entry);
      entry_text = gtk_entry_get_text (entry);

      lc = localeconv ();

      if (*(lc->negative_sign))
        neg_sign = *(lc->negative_sign);
      else
        neg_sign = '-';

      if (*(lc->positive_sign))
        pos_sign = *(lc->positive_sign);
      else
        pos_sign = '+';

#ifdef G_OS_WIN32
      /* Workaround for bug caused by some Windows application messing
       * up the positive sign of the current locale, more specifically
       * HKEY_CURRENT_USER\Control Panel\International\sPositiveSign.
       * See bug #330743 and for instance
       * http://www.msnewsgroups.net/group/microsoft.public.dotnet.languages.csharp/topic36024.aspx
       *
       * I don't know if the positive sign always gets bogusly set to
       * a digit when the above Registry value is corrupted as
       * described. (In my test case, it got set to "8", and in the
       * bug report above it presumably was set ot "0".) Probably it
       * might get set to almost anything? So how to distinguish a
       * bogus value from some correct one for some locale? That is
       * probably hard, but at least we should filter out the
       * digits...
       */
      if (pos_sign >= '0' && pos_sign <= '9')
        pos_sign = '+';
#endif

      for (sign=0, i=0; i<entry_length; i++)
        if ((entry_text[i] == neg_sign) ||
            (entry_text[i] == pos_sign))
          {
            sign = 1;
            break;
          }

      if (sign && !(*position))
        return;

      for (dotpos=-1, i=0; i<entry_length; i++)
        if (entry_text[i] == *(lc->decimal_point))
          {
            dotpos = i;
            break;
          }

      if (dotpos > -1 && *position > dotpos &&
          (gint)priv->digits - entry_length
            + dotpos - new_text_length + 1 < 0)
        return;

      for (i = 0; i < new_text_length; i++)
        {
          if (new_text[i] == neg_sign || new_text[i] == pos_sign)
            {
              if (sign || (*position) || i)
                return;
              sign = TRUE;
            }
          else if (new_text[i] == *(lc->decimal_point))
            {
              if (!priv->digits || dotpos > -1 ||
                  (new_text_length - 1 - i + entry_length
                    - *position > (gint)priv->digits))
                return;
              dotpos = *position + i;
            }
          else if (new_text[i] < 0x30 || new_text[i] > 0x39)
            return;
        }
    }

  parent_editable_iface->insert_text (editable, new_text,
                                      new_text_length, position);
}

static void
gtk_spin_button_real_spin (GtkSpinButton *spin_button,
                           gdouble        increment)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkAdjustment *adjustment;
  gdouble new_value = 0.0;
  gboolean wrapped = FALSE;

  adjustment = priv->adjustment;

  new_value = gtk_adjustment_get_value (adjustment) + increment;

  if (increment > 0)
    {
      if (priv->wrap)
        {
          if (fabs (gtk_adjustment_get_value (adjustment) - gtk_adjustment_get_upper (adjustment)) < EPSILON)
            {
              new_value = gtk_adjustment_get_lower (adjustment);
              wrapped = TRUE;
            }
          else if (new_value > gtk_adjustment_get_upper (adjustment))
            new_value = gtk_adjustment_get_upper (adjustment);
        }
      else
        new_value = MIN (new_value, gtk_adjustment_get_upper (adjustment));
    }
  else if (increment < 0)
    {
      if (priv->wrap)
        {
          if (fabs (gtk_adjustment_get_value (adjustment) - gtk_adjustment_get_lower (adjustment)) < EPSILON)
            {
              new_value = gtk_adjustment_get_upper (adjustment);
              wrapped = TRUE;
            }
          else if (new_value < gtk_adjustment_get_lower (adjustment))
            new_value = gtk_adjustment_get_lower (adjustment);
        }
      else
        new_value = MAX (new_value, gtk_adjustment_get_lower (adjustment));
    }

  if (fabs (new_value - gtk_adjustment_get_value (adjustment)) > EPSILON)
    gtk_adjustment_set_value (adjustment, new_value);

  if (wrapped)
    g_signal_emit (spin_button, spinbutton_signals[WRAPPED], 0);

  gtk_widget_queue_draw (GTK_WIDGET (spin_button));
}

static gint
gtk_spin_button_default_input (GtkSpinButton *spin_button,
                               gdouble       *new_val)
{
  gchar *err = NULL;

  *new_val = g_strtod (gtk_entry_get_text (GTK_ENTRY (spin_button)), &err);
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return FALSE;
}

static void
gtk_spin_button_default_output (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  gchar *buf = gtk_spin_button_format_for_value (spin_button,
                                                 gtk_adjustment_get_value (priv->adjustment));

  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);

  g_free (buf);
}


/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/


/**
 * gtk_spin_button_configure:
 * @spin_button: a #GtkSpinButton
 * @adjustment: (allow-none):  a #GtkAdjustment
 * @climb_rate: the new climb rate
 * @digits: the number of decimal places to display in the spin button
 *
 * Changes the properties of an existing spin button. The adjustment,
 * climb rate, and number of decimal places are all changed accordingly,
 * after this function call.
 */
void
gtk_spin_button_configure (GtkSpinButton *spin_button,
                           GtkAdjustment *adjustment,
                           gdouble        climb_rate,
                           guint          digits)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (!adjustment)
    adjustment = priv->adjustment;

  g_object_freeze_notify (G_OBJECT (spin_button));
  if (priv->adjustment != adjustment)
    {
      gtk_spin_button_unset_adjustment (spin_button);

      priv->adjustment = adjustment;
      g_object_ref_sink (adjustment);
      g_signal_connect (adjustment, "value-changed",
                        G_CALLBACK (gtk_spin_button_value_changed),
                        spin_button);
      g_signal_connect (adjustment, "changed",
                        G_CALLBACK (adjustment_changed_cb),
                        spin_button);
      priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);

      g_object_notify (G_OBJECT (spin_button), "adjustment");
      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }

  if (priv->digits != digits)
    {
      priv->digits = digits;
      g_object_notify (G_OBJECT (spin_button), "digits");
    }

  if (priv->climb_rate != climb_rate)
    {
      priv->climb_rate = climb_rate;
      g_object_notify (G_OBJECT (spin_button), "climb-rate");
    }
  g_object_thaw_notify (G_OBJECT (spin_button));

  gtk_adjustment_value_changed (adjustment);
}

/**
 * gtk_spin_button_new:
 * @adjustment: (allow-none): the #GtkAdjustment object that this spin
 *     button should use, or %NULL
 * @climb_rate: specifies how much the spin button changes when an arrow
 *     is clicked on
 * @digits: the number of decimal places to display
 *
 * Creates a new #GtkSpinButton.
 *
 * Returns: The new spin button as a #GtkWidget
 */
GtkWidget *
gtk_spin_button_new (GtkAdjustment *adjustment,
                     gdouble        climb_rate,
                     guint          digits)
{
  GtkSpinButton *spin;

  if (adjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  spin = g_object_new (GTK_TYPE_SPIN_BUTTON, NULL);

  gtk_spin_button_configure (spin, adjustment, climb_rate, digits);

  return GTK_WIDGET (spin);
}

/**
 * gtk_spin_button_new_with_range:
 * @min: Minimum allowable value
 * @max: Maximum allowable value
 * @step: Increment added or subtracted by spinning the widget
 *
 * This is a convenience constructor that allows creation of a numeric
 * #GtkSpinButton without manually creating an adjustment. The value is
 * initially set to the minimum value and a page increment of 10 * @step
 * is the default. The precision of the spin button is equivalent to the
 * precision of @step.
 *
 * Note that the way in which the precision is derived works best if @step
 * is a power of ten. If the resulting precision is not suitable for your
 * needs, use gtk_spin_button_set_digits() to correct it.
 *
 * Returns: The new spin button as a #GtkWidget
 */
GtkWidget *
gtk_spin_button_new_with_range (gdouble min,
                                gdouble max,
                                gdouble step)
{
  GtkAdjustment *adjustment;
  GtkSpinButton *spin;
  gint digits;

  g_return_val_if_fail (min <= max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  spin = g_object_new (GTK_TYPE_SPIN_BUTTON, NULL);

  adjustment = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  if (fabs (step) >= 1.0 || step == 0.0)
    digits = 0;
  else {
    digits = abs ((gint) floor (log10 (fabs (step))));
    if (digits > MAX_DIGITS)
      digits = MAX_DIGITS;
  }

  gtk_spin_button_configure (spin, adjustment, step, digits);

  gtk_spin_button_set_numeric (spin, TRUE);

  return GTK_WIDGET (spin);
}

/**
 * gtk_spin_button_set_adjustment:
 * @spin_button: a #GtkSpinButton
 * @adjustment: a #GtkAdjustment to replace the existing adjustment
 *
 * Replaces the #GtkAdjustment associated with @spin_button.
 */
void
gtk_spin_button_set_adjustment (GtkSpinButton *spin_button,
                                GtkAdjustment *adjustment)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  gtk_spin_button_configure (spin_button,
                             adjustment,
                             priv->climb_rate,
                             priv->digits);
}

/**
 * gtk_spin_button_get_adjustment:
 * @spin_button: a #GtkSpinButton
 *
 * Get the adjustment associated with a #GtkSpinButton
 *
 * Returns: (transfer none): the #GtkAdjustment of @spin_button
 **/
GtkAdjustment *
gtk_spin_button_get_adjustment (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), NULL);

  return spin_button->priv->adjustment;
}

/**
 * gtk_spin_button_set_digits:
 * @spin_button: a #GtkSpinButton
 * @digits: the number of digits after the decimal point to be displayed for the spin button’s value
 *
 * Set the precision to be displayed by @spin_button. Up to 20 digit precision
 * is allowed.
 **/
void
gtk_spin_button_set_digits (GtkSpinButton *spin_button,
                            guint          digits)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (priv->digits != digits)
    {
      priv->digits = digits;
      gtk_spin_button_value_changed (priv->adjustment, spin_button);
      g_object_notify (G_OBJECT (spin_button), "digits");

      /* since lower/upper may have changed */
      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }
}

/**
 * gtk_spin_button_get_digits:
 * @spin_button: a #GtkSpinButton
 *
 * Fetches the precision of @spin_button. See gtk_spin_button_set_digits().
 *
 * Returns: the current precision
 **/
guint
gtk_spin_button_get_digits (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  return spin_button->priv->digits;
}

/**
 * gtk_spin_button_set_increments:
 * @spin_button: a #GtkSpinButton
 * @step: increment applied for a button 1 press.
 * @page: increment applied for a button 2 press.
 *
 * Sets the step and page increments for spin_button.  This affects how
 * quickly the value changes when the spin button’s arrows are activated.
 **/
void
gtk_spin_button_set_increments (GtkSpinButton *spin_button,
                                gdouble        step,
                                gdouble        page)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  gtk_adjustment_configure (priv->adjustment,
                            gtk_adjustment_get_value (priv->adjustment),
                            gtk_adjustment_get_lower (priv->adjustment),
                            gtk_adjustment_get_upper (priv->adjustment),
                            step,
                            page,
                            gtk_adjustment_get_page_size (priv->adjustment));
}

/**
 * gtk_spin_button_get_increments:
 * @spin_button: a #GtkSpinButton
 * @step: (out) (allow-none): location to store step increment, or %NULL
 * @page: (out) (allow-none): location to store page increment, or %NULL
 *
 * Gets the current step and page the increments used by @spin_button. See
 * gtk_spin_button_set_increments().
 **/
void
gtk_spin_button_get_increments (GtkSpinButton *spin_button,
                                gdouble       *step,
                                gdouble       *page)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (step)
    *step = gtk_adjustment_get_step_increment (priv->adjustment);
  if (page)
    *page = gtk_adjustment_get_page_increment (priv->adjustment);
}

/**
 * gtk_spin_button_set_range:
 * @spin_button: a #GtkSpinButton
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * Sets the minimum and maximum allowable values for @spin_button.
 *
 * If the current value is outside this range, it will be adjusted
 * to fit within the range, otherwise it will remain unchanged.
 */
void
gtk_spin_button_set_range (GtkSpinButton *spin_button,
                           gdouble        min,
                           gdouble        max)
{
  GtkAdjustment *adjustment;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  adjustment = spin_button->priv->adjustment;

  gtk_adjustment_configure (adjustment,
                            CLAMP (gtk_adjustment_get_value (adjustment), min, max),
                            min,
                            max,
                            gtk_adjustment_get_step_increment (adjustment),
                            gtk_adjustment_get_page_increment (adjustment),
                            gtk_adjustment_get_page_size (adjustment));
}

/**
 * gtk_spin_button_get_range:
 * @spin_button: a #GtkSpinButton
 * @min: (out) (allow-none): location to store minimum allowed value, or %NULL
 * @max: (out) (allow-none): location to store maximum allowed value, or %NULL
 *
 * Gets the range allowed for @spin_button.
 * See gtk_spin_button_set_range().
 */
void
gtk_spin_button_get_range (GtkSpinButton *spin_button,
                           gdouble       *min,
                           gdouble       *max)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (min)
    *min = gtk_adjustment_get_lower (priv->adjustment);
  if (max)
    *max = gtk_adjustment_get_upper (priv->adjustment);
}

/**
 * gtk_spin_button_get_value:
 * @spin_button: a #GtkSpinButton
 *
 * Get the value in the @spin_button.
 *
 * Returns: the value of @spin_button
 */
gdouble
gtk_spin_button_get_value (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0.0);

  return gtk_adjustment_get_value (spin_button->priv->adjustment);
}

/**
 * gtk_spin_button_get_value_as_int:
 * @spin_button: a #GtkSpinButton
 *
 * Get the value @spin_button represented as an integer.
 *
 * Returns: the value of @spin_button
 */
gint
gtk_spin_button_get_value_as_int (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv;
  gdouble val;

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  priv = spin_button->priv;

  val = gtk_adjustment_get_value (priv->adjustment);
  if (val - floor (val) < ceil (val) - val)
    return floor (val);
  else
    return ceil (val);
}

/**
 * gtk_spin_button_set_value:
 * @spin_button: a #GtkSpinButton
 * @value: the new value
 *
 * Sets the value of @spin_button.
 */
void
gtk_spin_button_set_value (GtkSpinButton *spin_button,
                           gdouble        value)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (fabs (value - gtk_adjustment_get_value (priv->adjustment)) > EPSILON)
    gtk_adjustment_set_value (priv->adjustment, value);
  else
    {
      gint return_val = FALSE;
      g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);
      if (!return_val)
        gtk_spin_button_default_output (spin_button);
    }
}

/**
 * gtk_spin_button_set_update_policy:
 * @spin_button: a #GtkSpinButton
 * @policy: a #GtkSpinButtonUpdatePolicy value
 *
 * Sets the update behavior of a spin button.
 * This determines whether the spin button is always updated
 * or only when a valid value is set.
 */
void
gtk_spin_button_set_update_policy (GtkSpinButton             *spin_button,
                                   GtkSpinButtonUpdatePolicy  policy)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  if (priv->update_policy != policy)
    {
      priv->update_policy = policy;
      g_object_notify (G_OBJECT (spin_button), "update-policy");
    }
}

/**
 * gtk_spin_button_get_update_policy:
 * @spin_button: a #GtkSpinButton
 *
 * Gets the update behavior of a spin button.
 * See gtk_spin_button_set_update_policy().
 *
 * Returns: the current update policy
 */
GtkSpinButtonUpdatePolicy
gtk_spin_button_get_update_policy (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), GTK_UPDATE_ALWAYS);

  return spin_button->priv->update_policy;
}

/**
 * gtk_spin_button_set_numeric:
 * @spin_button: a #GtkSpinButton
 * @numeric: flag indicating if only numeric entry is allowed
 *
 * Sets the flag that determines if non-numeric text can be typed
 * into the spin button.
 */
void
gtk_spin_button_set_numeric (GtkSpinButton *spin_button,
                             gboolean       numeric)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  numeric = numeric != FALSE;

  if (priv->numeric != numeric)
    {
       priv->numeric = numeric;
       g_object_notify (G_OBJECT (spin_button), "numeric");
    }
}

/**
 * gtk_spin_button_get_numeric:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether non-numeric text can be typed into the spin button.
 * See gtk_spin_button_set_numeric().
 *
 * Returns: %TRUE if only numeric text can be entered
 */
gboolean
gtk_spin_button_get_numeric (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->priv->numeric;
}

/**
 * gtk_spin_button_set_wrap:
 * @spin_button: a #GtkSpinButton
 * @wrap: a flag indicating if wrapping behavior is performed
 *
 * Sets the flag that determines if a spin button value wraps
 * around to the opposite limit when the upper or lower limit
 * of the range is exceeded.
 */
void
gtk_spin_button_set_wrap (GtkSpinButton  *spin_button,
                          gboolean        wrap)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  wrap = wrap != FALSE;

  if (priv->wrap != wrap)
    {
       priv->wrap = wrap;

       g_object_notify (G_OBJECT (spin_button), "wrap");
    }
}

/**
 * gtk_spin_button_get_wrap:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether the spin button’s value wraps around to the
 * opposite limit when the upper or lower limit of the range is
 * exceeded. See gtk_spin_button_set_wrap().
 *
 * Returns: %TRUE if the spin button wraps around
 */
gboolean
gtk_spin_button_get_wrap (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->priv->wrap;
}

/**
 * gtk_spin_button_set_snap_to_ticks:
 * @spin_button: a #GtkSpinButton
 * @snap_to_ticks: a flag indicating if invalid values should be corrected
 *
 * Sets the policy as to whether values are corrected to the
 * nearest step increment when a spin button is activated after
 * providing an invalid value.
 */
void
gtk_spin_button_set_snap_to_ticks (GtkSpinButton *spin_button,
                                   gboolean       snap_to_ticks)
{
  GtkSpinButtonPrivate *priv;
  guint new_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  new_val = (snap_to_ticks != 0);

  if (new_val != priv->snap_to_ticks)
    {
      priv->snap_to_ticks = new_val;
      if (new_val && gtk_editable_get_editable (GTK_EDITABLE (spin_button)))
        gtk_spin_button_update (spin_button);

      g_object_notify (G_OBJECT (spin_button), "snap-to-ticks");
    }
}

/**
 * gtk_spin_button_get_snap_to_ticks:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether the values are corrected to the nearest step.
 * See gtk_spin_button_set_snap_to_ticks().
 *
 * Returns: %TRUE if values are snapped to the nearest step
 */
gboolean
gtk_spin_button_get_snap_to_ticks (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->priv->snap_to_ticks;
}

/**
 * gtk_spin_button_spin:
 * @spin_button: a #GtkSpinButton
 * @direction: a #GtkSpinType indicating the direction to spin
 * @increment: step increment to apply in the specified direction
 *
 * Increment or decrement a spin button’s value in a specified
 * direction by a specified amount.
 */
void
gtk_spin_button_spin (GtkSpinButton *spin_button,
                      GtkSpinType    direction,
                      gdouble        increment)
{
  GtkSpinButtonPrivate *priv;
  GtkAdjustment *adjustment;
  gdouble diff;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  adjustment = priv->adjustment;

  /* for compatibility with the 1.0.x version of this function */
  if (increment != 0 && increment != gtk_adjustment_get_step_increment (adjustment) &&
      (direction == GTK_SPIN_STEP_FORWARD ||
       direction == GTK_SPIN_STEP_BACKWARD))
    {
      if (direction == GTK_SPIN_STEP_BACKWARD && increment > 0)
        increment = -increment;
      direction = GTK_SPIN_USER_DEFINED;
    }

  switch (direction)
    {
    case GTK_SPIN_STEP_FORWARD:

      gtk_spin_button_real_spin (spin_button, gtk_adjustment_get_step_increment (adjustment));
      break;

    case GTK_SPIN_STEP_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -gtk_adjustment_get_step_increment (adjustment));
      break;

    case GTK_SPIN_PAGE_FORWARD:

      gtk_spin_button_real_spin (spin_button, gtk_adjustment_get_page_increment (adjustment));
      break;

    case GTK_SPIN_PAGE_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -gtk_adjustment_get_page_increment (adjustment));
      break;

    case GTK_SPIN_HOME:

      diff = gtk_adjustment_get_value (adjustment) - gtk_adjustment_get_lower (adjustment);
      if (diff > EPSILON)
        gtk_spin_button_real_spin (spin_button, -diff);
      break;

    case GTK_SPIN_END:

      diff = gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_value (adjustment);
      if (diff > EPSILON)
        gtk_spin_button_real_spin (spin_button, diff);
      break;

    case GTK_SPIN_USER_DEFINED:

      if (increment != 0)
        gtk_spin_button_real_spin (spin_button, increment);
      break;

    default:
      break;
    }
}

/**
 * gtk_spin_button_update:
 * @spin_button: a #GtkSpinButton
 *
 * Manually force an update of the spin button.
 */
void
gtk_spin_button_update (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv;
  gdouble val;
  gint error = 0;
  gint return_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  priv = spin_button->priv;

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[INPUT], 0, &val, &return_val);
  if (return_val == FALSE)
    {
      return_val = gtk_spin_button_default_input (spin_button, &val);
      error = (return_val == GTK_INPUT_ERROR);
    }
  else if (return_val == GTK_INPUT_ERROR)
    error = 1;

  gtk_widget_queue_draw (GTK_WIDGET (spin_button));

  if (priv->update_policy == GTK_UPDATE_ALWAYS)
    {
      if (val < gtk_adjustment_get_lower (priv->adjustment))
        val = gtk_adjustment_get_lower (priv->adjustment);
      else if (val > gtk_adjustment_get_upper (priv->adjustment))
        val = gtk_adjustment_get_upper (priv->adjustment);
    }
  else if ((priv->update_policy == GTK_UPDATE_IF_VALID) &&
           (error ||
            val < gtk_adjustment_get_lower (priv->adjustment) ||
            val > gtk_adjustment_get_upper (priv->adjustment)))
    {
      gtk_spin_button_value_changed (priv->adjustment, spin_button);
      return;
    }

  if (priv->snap_to_ticks)
    gtk_spin_button_snap (spin_button, val);
  else
    gtk_spin_button_set_value (spin_button, val);
}

void
_gtk_spin_button_get_panels (GtkSpinButton *spin_button,
                             GdkWindow **down_panel,
                             GdkWindow **up_panel)
{
  if (down_panel != NULL)
    *down_panel = spin_button->priv->down_panel;

  if (up_panel != NULL)
    *up_panel = spin_button->priv->up_panel;
}
