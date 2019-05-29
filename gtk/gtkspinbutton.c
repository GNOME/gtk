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

#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkeditable.h"
#include "gtkimage.h"
#include "gtktext.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkgestureclick.h"
#include "gtkgestureswipe.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkspinbuttonaccessible.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>

#define MAX_TIMER_CALLS       5
#define EPSILON               1e-10
#define MAX_DIGITS            20
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
 * properties. Note that GtkSpinButton will by default make its entry
 * large enough to accomodate the lower and upper bounds of the adjustment,
 * which can lead to surprising results. Best practice is to set both
 * the #GtkEntry:width-chars and #GtkEntry:max-width-chars poperties
 * to the desired number of characters to display in the entry.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * spinbutton.horizontal
 * ╰── box.horizontal
 *     ├── text
 *     │    ├── undershoot.left
 *     │    ╰── undershoot.right
 *     ├── button.down
 *     ╰── button.up
 * ]|
 *
 * |[<!-- language="plain" -->
 * spinbutton.vertical
 * ╰── box.vertical
 *     ├── button.up
 *     ├── text
 *     │    ├── undershoot.left
 *     │    ╰── undershoot.right
 *     ╰── button.down
 * ]|
 *
 * GtkSpinButtons main CSS node has the name spinbutton. It creates subnodes
 * for the entry and the two buttons, with these names. The button nodes have
 * the style classes .up and .down. The GtkText subnodes (if present) are put
 * below the text node. The orientation of the spin button is reflected in
 * the .vertical or .horizontal style class on the main node.
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
 *
 *   // creates the spinbutton, with no decimal places
 *   button = gtk_spin_button_new (adjustment, 1.0, 0);
 *   gtk_container_add (GTK_CONTAINER (window), button);
 *
 *   gtk_widget_show (window);
 * }
 * ]|
 *
 * ## Using a GtkSpinButton to get a floating point value
 *
 * |[<!-- language="C" -->
 * // Provides a function to retrieve a floating point value from a
 * // GtkSpinButton, and creates a high precision spin button.
 *
 * float
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
 *
 *   // creates the spinbutton, with three decimal places
 *   button = gtk_spin_button_new (adjustment, 0.001, 3);
 *   gtk_container_add (GTK_CONTAINER (window), button);
 *
 *   gtk_widget_show (window);
 * }
 * ]|
 */

typedef struct _GtkSpinButton      GtkSpinButton;
typedef struct _GtkSpinButtonClass GtkSpinButtonClass;

struct _GtkSpinButton
{
  GtkWidget parent_instance;
};

struct _GtkSpinButtonClass
{
  GtkWidgetClass parent_class;

  gint (*input)  (GtkSpinButton *spin_button,
                  gdouble       *new_value);
  gint (*output) (GtkSpinButton *spin_button);
  void (*value_changed) (GtkSpinButton *spin_button);

  /* Action signals for keybindings, do not connect to these */
  void (*change_value) (GtkSpinButton *spin_button,
                        GtkScrollType  scroll);

  void (*wrapped) (GtkSpinButton *spin_button);
};

struct _GtkSpinButtonPrivate
{
  GtkAdjustment *adjustment;

  GtkWidget     *box;
  GtkWidget     *entry;

  GtkWidget     *up_button;
  GtkWidget     *down_button;

  GtkWidget     *click_child;

  guint32        timer;

  GtkSpinButtonUpdatePolicy update_policy;

  gdouble        climb_rate;
  gdouble        timer_step;

  GtkOrientation orientation;

  guint          digits        : 10;
  guint          need_timer    : 1;
  guint          numeric       : 1;
  guint          snap_to_ticks : 1;
  guint          timer_calls   : 3;
  guint          wrap          : 1;
};
typedef struct _GtkSpinButtonPrivate GtkSpinButtonPrivate;

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
  NUM_SPINBUTTON_PROPS,
  PROP_ORIENTATION = NUM_SPINBUTTON_PROPS
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
static void gtk_spin_button_realize        (GtkWidget          *widget);
static void gtk_spin_button_measure (GtkWidget      *widget,
                                     GtkOrientation  orientation,
                                     int             for_size,
                                     int            *minimum,
                                     int            *natural,
                                     int            *minimum_baseline,
                                     int            *natural_baseline);
static void gtk_spin_button_size_allocate  (GtkWidget *widget,
                                            int        width,
                                            int        height,
                                            int        baseline);
static void gtk_spin_button_grab_notify    (GtkWidget          *widget,
                                            gboolean            was_grabbed);
static void gtk_spin_button_state_flags_changed  (GtkWidget     *widget,
                                                  GtkStateFlags  previous_state);
static gboolean gtk_spin_button_timer          (GtkSpinButton      *spin_button);
static gboolean gtk_spin_button_stop_spinning  (GtkSpinButton      *spin);
static void gtk_spin_button_value_changed  (GtkAdjustment      *adjustment,
                                            GtkSpinButton      *spin_button);

static void gtk_spin_button_activate       (GtkText            *entry,
                                            gpointer            user_data);
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
static GParamSpec *spinbutton_props[NUM_SPINBUTTON_PROPS] = {NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkSpinButton, gtk_spin_button, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkSpinButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_spin_button_editable_init))

#define add_spin_binding(binding_set, keyval, mask, scroll)            \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,             \
                                "change-value", 1,                     \
                                GTK_TYPE_SCROLL_TYPE, scroll)


static void
gtk_spin_button_grab_focus (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  gtk_widget_grab_focus (priv->entry);
}

static gboolean
gtk_spin_button_mnemonic_activate (GtkWidget *widget,
                                   gboolean   group_cycling)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  gtk_widget_grab_focus (priv->entry);

  return TRUE;
}

static void
gtk_spin_button_class_init (GtkSpinButtonClass *class)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet    *binding_set;

  gobject_class->finalize = gtk_spin_button_finalize;
  gobject_class->set_property = gtk_spin_button_set_property;
  gobject_class->get_property = gtk_spin_button_get_property;

  widget_class->destroy = gtk_spin_button_destroy;
  widget_class->realize = gtk_spin_button_realize;
  widget_class->measure = gtk_spin_button_measure;
  widget_class->size_allocate = gtk_spin_button_size_allocate;
  widget_class->grab_notify = gtk_spin_button_grab_notify;
  widget_class->state_flags_changed = gtk_spin_button_state_flags_changed;
  widget_class->grab_focus = gtk_spin_button_grab_focus;
  widget_class->mnemonic_activate = gtk_spin_button_mnemonic_activate;

  class->input = NULL;
  class->output = NULL;
  class->change_value = gtk_spin_button_real_change_value;

  spinbutton_props[PROP_ADJUSTMENT] =
    g_param_spec_object ("adjustment",
                         P_("Adjustment"),
                         P_("The adjustment that holds the value of the spin button"),
                         GTK_TYPE_ADJUSTMENT,
                         GTK_PARAM_READWRITE);

  spinbutton_props[PROP_CLIMB_RATE] =
    g_param_spec_double ("climb-rate",
                         P_("Climb Rate"),
                         P_("The acceleration rate when you hold down a button or key"),
                         0.0, G_MAXDOUBLE, 0.0,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  spinbutton_props[PROP_DIGITS] =
    g_param_spec_uint ("digits",
                       P_("Digits"),
                       P_("The number of decimal places to display"),
                       0, MAX_DIGITS, 0,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  spinbutton_props[PROP_SNAP_TO_TICKS] =
    g_param_spec_boolean ("snap-to-ticks",
                          P_("Snap to Ticks"),
                          P_("Whether erroneous values are automatically changed to a spin button’s nearest step increment"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  spinbutton_props[PROP_NUMERIC] =
    g_param_spec_boolean ("numeric",
                          P_("Numeric"),
                          P_("Whether non-numeric characters should be ignored"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  spinbutton_props[PROP_WRAP] =
    g_param_spec_boolean ("wrap",
                          P_("Wrap"),
                          P_("Whether a spin button should wrap upon reaching its limits"),
                          FALSE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  spinbutton_props[PROP_UPDATE_POLICY] =
    g_param_spec_enum ("update-policy",
                       P_("Update Policy"),
                       P_("Whether the spin button should update always, or only when the value is legal"),
                       GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
                       GTK_UPDATE_ALWAYS,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  spinbutton_props[PROP_VALUE] =
    g_param_spec_double ("value",
                         P_("Value"),
                         P_("Reads the current value, or sets a new value"),
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_SPINBUTTON_PROPS, spinbutton_props);
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");
  gtk_editable_install_properties (gobject_class, PROP_ORIENTATION + 1);

  /**
   * GtkSpinButton::input:
   * @spin_button: the object on which the signal was emitted
   * @new_value: (out) (type double): return location for the new value
   *
   * The ::input signal can be used to influence the conversion of
   * the users input into a double value. The signal handler is
   * expected to use gtk_editable_get_text() to retrieve the text of
   * the spinbutton and set @new_value to the new value.
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
   *    gtk_spin_button_set_text (spin, text):
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
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSpinButton::wrapped:
   * @spin_button: the object on which the signal was emitted
   *
   * The ::wrapped signal is emitted right after the spinbutton wraps
   * from its maximum to minimum value or vice-versa.
   */
  spinbutton_signals[WRAPPED] =
    g_signal_new (I_("wrapped"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSpinButtonClass, wrapped),
                  NULL, NULL,
                  NULL,
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
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);

  binding_set = gtk_binding_set_by_class (class);

  add_spin_binding (binding_set, GDK_KEY_Up, 0, GTK_SCROLL_STEP_UP);
  add_spin_binding (binding_set, GDK_KEY_KP_Up, 0, GTK_SCROLL_STEP_UP);
  add_spin_binding (binding_set, GDK_KEY_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_spin_binding (binding_set, GDK_KEY_KP_Down, 0, GTK_SCROLL_STEP_DOWN);
  add_spin_binding (binding_set, GDK_KEY_Page_Up, 0, GTK_SCROLL_PAGE_UP);
  add_spin_binding (binding_set, GDK_KEY_Page_Down, 0, GTK_SCROLL_PAGE_DOWN);
  add_spin_binding (binding_set, GDK_KEY_End, GDK_CONTROL_MASK, GTK_SCROLL_END);
  add_spin_binding (binding_set, GDK_KEY_Home, GDK_CONTROL_MASK, GTK_SCROLL_START);
  add_spin_binding (binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK, GTK_SCROLL_END);
  add_spin_binding (binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK, GTK_SCROLL_START);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SPIN_BUTTON_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("spinbutton"));
}

static GtkEditable *
gtk_spin_button_get_delegate (GtkEditable *editable)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (editable);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  return GTK_EDITABLE (priv->entry);
}

static void
gtk_spin_button_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_spin_button_get_delegate;
  iface->insert_text = gtk_spin_button_insert_text;
}

static void
gtk_spin_button_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

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

static gboolean
scroll_controller_scroll (GtkEventControllerScroll *Scroll,
			  gdouble                   dx,
			  gdouble                   dy,
			  GtkWidget                *widget)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);
  gtk_spin_button_real_spin (spin, -dy * gtk_adjustment_get_step_increment (priv->adjustment));

  return GDK_EVENT_STOP;
}

static gboolean
gtk_spin_button_stop_spinning (GtkSpinButton *spin)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin);
  gboolean did_spin = FALSE;

  if (priv->timer)
    {
      g_source_remove (priv->timer);
      priv->timer = 0;
      priv->need_timer = FALSE;

      did_spin = TRUE;
    }

  priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);
  priv->timer_calls = 0;

  priv->click_child = NULL;

  return did_spin;
}

static void
start_spinning (GtkSpinButton *spin,
                GtkWidget     *click_child,
                gdouble        step)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin);

  priv->click_child = click_child;

  if (!priv->timer)
    {
      priv->timer_step = step;
      priv->need_timer = TRUE;
      priv->timer = g_timeout_add (TIMEOUT_INITIAL,
                                   (GSourceFunc) gtk_spin_button_timer,
                                   (gpointer) spin);
      g_source_set_name_by_id (priv->timer, "[gtk] gtk_spin_button_timer");
    }
  gtk_spin_button_real_spin (spin, click_child == priv->up_button ? step : -step);

  gtk_widget_queue_draw (GTK_WIDGET (spin));
}

static void
button_pressed_cb (GtkGestureClick *gesture,
                   int              n_pressses,
                   double           x,
                   double           y,
                   gpointer         user_data)
{
  GtkSpinButton *spin_button = user_data;
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  GtkWidget *pressed_button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  gtk_widget_grab_focus (GTK_WIDGET (spin_button));

  if (gtk_editable_get_editable (GTK_EDITABLE (priv->entry)))
    {
      int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
      gtk_spin_button_update (spin_button);

      if (button == GDK_BUTTON_PRIMARY)
        start_spinning (spin_button, pressed_button, gtk_adjustment_get_step_increment (priv->adjustment));
      else if (button == GDK_BUTTON_MIDDLE)
        start_spinning (spin_button, pressed_button, gtk_adjustment_get_page_increment (priv->adjustment));
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (spin_button));
    }
}

static void
button_released_cb (GtkGestureClick *gesture,
                    int              n_pressses,
                    double           x,
                    double           y,
                    gpointer         user_data)
{
  GtkSpinButton *spin_button = user_data;
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  gtk_spin_button_stop_spinning (spin_button);

  if (button == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *button_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
      double diff;
      if (button_widget == priv->down_button)
        {
          diff = gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment);
          if (diff > EPSILON)
            gtk_spin_button_real_spin (spin_button, -diff);
        }
      else if (button_widget == priv->up_button)
        {
          diff = gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_value (priv->adjustment);
          if (diff > EPSILON)
            gtk_spin_button_real_spin (spin_button, diff);
        }
    }
}

static void
button_cancel_cb (GtkGesture       *gesture,
                  GdkEventSequence *sequence,
                  GtkSpinButton    *spin_button)
{
  gtk_spin_button_stop_spinning (spin_button);
}

static void
key_controller_key_released (GtkEventControllerKey *key,
                             guint                  keyval,
                             guint                  keycode,
                             GdkModifierType        modifiers,
                             GtkSpinButton         *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);
  priv->timer_calls = 0;
}

static void
key_controller_focus_out (GtkEventControllerKey *key,
                          GdkCrossingMode        mode,
                          GdkNotifyType          detail,
                          GtkSpinButton         *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  if (gtk_editable_get_editable (GTK_EDITABLE (priv->entry)))
    gtk_spin_button_update (spin_button);
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  GtkEventController *controller;
  GtkGesture *gesture;

  priv->adjustment = NULL;
  priv->timer = 0;
  priv->climb_rate = 0.0;
  priv->timer_step = 0.0;
  priv->update_policy = GTK_UPDATE_ALWAYS;
  priv->click_child = NULL;
  priv->need_timer = FALSE;
  priv->timer_calls = 0;
  priv->digits = 0;
  priv->numeric = FALSE;
  priv->wrap = FALSE;
  priv->snap_to_ticks = FALSE;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (spin_button));

  priv->box = gtk_box_new (priv->orientation, 0);
  gtk_widget_set_parent (priv->box, GTK_WIDGET (spin_button));

  priv->entry = gtk_text_new ();
  gtk_editable_init_delegate (GTK_EDITABLE (spin_button));
  gtk_editable_set_width_chars (GTK_EDITABLE (priv->entry), 0);
  gtk_editable_set_max_width_chars (GTK_EDITABLE (priv->entry), 0);
  gtk_widget_set_hexpand (priv->entry, TRUE);
  gtk_widget_set_vexpand (priv->entry, TRUE);
  g_signal_connect (priv->entry, "activate", G_CALLBACK (gtk_spin_button_activate), spin_button);
  gtk_container_add (GTK_CONTAINER (priv->box), priv->entry);

  priv->down_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (priv->down_button), gtk_image_new_from_icon_name ("value-decrease-symbolic"));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->down_button), "image-button");
  gtk_widget_set_can_focus (priv->down_button, FALSE);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->down_button), "down");
  gtk_container_add (GTK_CONTAINER (priv->box), priv->down_button);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (button_pressed_cb), spin_button);
  g_signal_connect (gesture, "released", G_CALLBACK (button_released_cb), spin_button);
  g_signal_connect (gesture, "cancel", G_CALLBACK (button_cancel_cb), spin_button);
  gtk_widget_add_controller (GTK_WIDGET (priv->down_button), GTK_EVENT_CONTROLLER (gesture));

  priv->up_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (priv->up_button), gtk_image_new_from_icon_name ("value-increase-symbolic"));
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->up_button), "image-button");
  gtk_widget_set_can_focus (priv->up_button, FALSE);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->up_button), "up");
  gtk_container_add (GTK_CONTAINER (priv->box), priv->up_button);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (button_pressed_cb), spin_button);
  g_signal_connect (gesture, "released", G_CALLBACK (button_released_cb), spin_button);
  g_signal_connect (gesture, "cancel", G_CALLBACK (button_cancel_cb), spin_button);
  gtk_widget_add_controller (GTK_WIDGET (priv->up_button), GTK_EVENT_CONTROLLER (gesture));

  gtk_spin_button_set_adjustment (spin_button, NULL);

  gesture = gtk_gesture_swipe_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (gesture, "begin",
                    G_CALLBACK (swipe_gesture_begin), spin_button);
  g_signal_connect (gesture, "update",
                    G_CALLBACK (swipe_gesture_update), spin_button);
  gtk_widget_add_controller (GTK_WIDGET (spin_button), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
				                GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (scroll_controller_scroll), spin_button);
  gtk_widget_add_controller (GTK_WIDGET (spin_button), controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-released",
                    G_CALLBACK (key_controller_key_released), spin_button);
  g_signal_connect (controller, "focus-out",
                    G_CALLBACK (key_controller_focus_out), spin_button);
  gtk_widget_add_controller (GTK_WIDGET (spin_button), controller);
}

static void
gtk_spin_button_finalize (GObject *object)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (object);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  gtk_spin_button_unset_adjustment (spin_button);

  gtk_editable_finish_delegate (GTK_EDITABLE (spin_button));

  gtk_widget_unparent (priv->box);

  G_OBJECT_CLASS (gtk_spin_button_parent_class)->finalize (object);
}

static void
gtk_spin_button_destroy (GtkWidget *widget)
{
  gtk_spin_button_stop_spinning (GTK_SPIN_BUTTON (widget));

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->destroy (widget);
}

static void
gtk_spin_button_realize (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gboolean return_val;
  const char *text;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->realize (widget);

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);

  /* If output wasn't processed explicitly by the method connected to the
   * 'output' signal; and if we don't have any explicit 'text' set initially,
   * fallback to the default output. */
  text = gtk_editable_get_text (GTK_EDITABLE (priv->entry));
  if (!return_val && (priv->numeric || text == NULL || *text == '\0'))
    gtk_spin_button_default_output (spin_button);
}

/* This is called when :value, :wrap, or the bounds of the adjustment change,
 * as the combination of those determines if our up|down_button are sensitive */
static void
update_buttons_sensitivity (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  int lower = gtk_adjustment_get_lower (priv->adjustment);
  int upper = gtk_adjustment_get_upper (priv->adjustment);
  int value = gtk_adjustment_get_value (priv->adjustment);

  gtk_widget_set_sensitive (priv->up_button,
                            priv->wrap || upper - value > EPSILON);
  gtk_widget_set_sensitive (priv->down_button,
                            priv->wrap || value - lower > EPSILON);
}

/* Callback used when the spin button's adjustment changes.
 * We need to reevaluate our size request & up|down_button sensitivity. */
static void
adjustment_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (data);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);

  update_buttons_sensitivity (spin_button);
  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

static void
gtk_spin_button_unset_adjustment (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

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
gtk_spin_button_set_orientation (GtkSpinButton  *spin,
                                 GtkOrientation  orientation)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin);
  GtkEditable *editable = GTK_EDITABLE (priv->entry);

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (spin));

  /* change alignment if it's the default */
  if (priv->orientation == GTK_ORIENTATION_VERTICAL &&
      gtk_editable_get_alignment (editable) == 0.0)
    gtk_editable_set_alignment (editable, 0.5);
  else if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
           gtk_editable_get_alignment (editable) == 0.5)
    gtk_editable_set_alignment (editable, 0.0);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Current orientation of the box is vertical! */
      gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->up_button, NULL);
      gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->entry, NULL);
    }
  else
    {
      /* Current orientation of the box is horizontal! */
      gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->entry, NULL);
      gtk_box_reorder_child_after (GTK_BOX (priv->box), priv->up_button, NULL);
    }

  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->box), priv->orientation);

  g_object_notify (G_OBJECT (spin), "orientation");
}

static gchar *
weed_out_neg_zero (gchar *str,
                   gint   digits)
{
  if (str[0] == '-')
    {
      gchar neg_zero[8];
      g_snprintf (neg_zero, 8, "%0.*f", digits, -0.0);
      if (strcmp (neg_zero, str) == 0)
        memmove (str, str + 1, strlen (str) - 1);
    }
  return str;
}

static gchar *
gtk_spin_button_format_for_value (GtkSpinButton *spin_button,
                                  gdouble        value)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gchar *buf = g_strdup_printf ("%0.*f", priv->digits, value);

  return weed_out_neg_zero (buf, priv->digits);
}

static void
gtk_spin_button_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (GTK_SPIN_BUTTON (widget));

  gtk_widget_measure (priv->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_spin_button_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (GTK_SPIN_BUTTON (widget));

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            },
                            baseline);
}

static void
gtk_spin_button_grab_notify (GtkWidget *widget,
                             gboolean   was_grabbed)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->grab_notify (widget, was_grabbed);

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

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->state_flags_changed (widget, previous_state);
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gboolean retval = FALSE;

  if (priv->timer)
    {
      if (priv->click_child == priv->up_button)
        gtk_spin_button_real_spin (spin_button, priv->timer_step);
      else
        gtk_spin_button_real_spin (spin_button, -priv->timer_step);

      if (priv->need_timer)
        {
          priv->need_timer = FALSE;
          priv->timer = g_timeout_add (TIMEOUT_REPEAT,
                                       (GSourceFunc) gtk_spin_button_timer,
                                       spin_button);
          g_source_set_name_by_id (priv->timer, "[gtk] gtk_spin_button_timer");
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

  update_buttons_sensitivity (spin_button);
  gtk_widget_queue_draw (GTK_WIDGET (spin_button));

  g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_VALUE]);
}

static void
gtk_spin_button_real_change_value (GtkSpinButton *spin,
                                   GtkScrollType  scroll)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin);
  gdouble old_value;

  if (!gtk_editable_get_editable (GTK_EDITABLE (priv->entry)))
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

    case GTK_SCROLL_NONE:
    case GTK_SCROLL_JUMP:
    default:
      g_warning ("Invalid scroll type %d for GtkSpinButton::change-value", scroll);
      break;
    }

  gtk_spin_button_update (spin);

  if (gtk_adjustment_get_value (priv->adjustment) == old_value)
    gtk_widget_error_bell (GTK_WIDGET (spin));
}

static void
gtk_spin_button_snap (GtkSpinButton *spin_button,
                      gdouble        val)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
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
gtk_spin_button_activate (GtkText *entry,
                          gpointer  user_data)
{
  GtkSpinButton *spin_button = user_data;
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  if (gtk_editable_get_editable (GTK_EDITABLE (priv->entry)))
    gtk_spin_button_update (spin_button);
}

static void
gtk_spin_button_insert_text (GtkEditable *editable,
                             const gchar *new_text,
                             gint         new_text_length,
                             gint        *position)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (editable);
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin);

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

      entry_text = gtk_editable_get_text (GTK_EDITABLE (priv->entry));
      entry_length = g_utf8_strlen (entry_text, -1);

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

  gtk_editable_insert_text (GTK_EDITABLE (priv->entry),
                            new_text, new_text_length, position);
}

static void
gtk_spin_button_real_spin (GtkSpinButton *spin_button,
                           gdouble        increment)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
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
}

static gint
gtk_spin_button_default_input (GtkSpinButton *spin_button,
                               gdouble       *new_val)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gchar *err = NULL;
  const char *text = gtk_editable_get_text (GTK_EDITABLE (priv->entry));

  *new_val = g_strtod (text, &err);
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return FALSE;
}

static void
gtk_spin_button_default_output (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gchar *buf = gtk_spin_button_format_for_value (spin_button,
                                                 gtk_adjustment_get_value (priv->adjustment));

  if (strcmp (buf, gtk_editable_get_text (GTK_EDITABLE (priv->entry))))
    gtk_editable_set_text (GTK_EDITABLE (priv->entry), buf);

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
 * @adjustment: (nullable): a #GtkAdjustment to replace the spin button’s
 *     existing adjustment, or %NULL to leave its current adjustment unchanged
 * @climb_rate: the new climb rate
 * @digits: the number of decimal places to display in the spin button
 *
 * Changes the properties of an existing spin button. The adjustment,
 * climb rate, and number of decimal places are updated accordingly.
 */
void
gtk_spin_button_configure (GtkSpinButton *spin_button,
                           GtkAdjustment *adjustment,
                           gdouble        climb_rate,
                           guint          digits)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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

      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_ADJUSTMENT]);
      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }

  if (priv->digits != digits)
    {
      priv->digits = digits;
      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_DIGITS]);
    }

  if (priv->climb_rate != climb_rate)
    {
      priv->climb_rate = climb_rate;
      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_CLIMB_RATE]);
    }

  g_object_thaw_notify (G_OBJECT (spin_button));

  gtk_spin_button_value_changed (adjustment, spin_button);
}

/**
 * gtk_spin_button_new:
 * @adjustment: (allow-none): the #GtkAdjustment object that this spin
 *     button should use, or %NULL
 * @climb_rate: specifies by how much the rate of change in the value will
 *     accelerate if you continue to hold down an up/down button or arrow key
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), NULL);

  return priv->adjustment;
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (priv->digits != digits)
    {
      priv->digits = digits;
      gtk_spin_button_value_changed (priv->adjustment, spin_button);
      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_DIGITS]);

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  return priv->digits;
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  GtkAdjustment *adjustment;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  adjustment = priv->adjustment;

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
 * @min: (out) (optional): location to store minimum allowed value, or %NULL
 * @max: (out) (optional): location to store maximum allowed value, or %NULL
 *
 * Gets the range allowed for @spin_button.
 * See gtk_spin_button_set_range().
 */
void
gtk_spin_button_get_range (GtkSpinButton *spin_button,
                           gdouble       *min,
                           gdouble       *max)
{
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0.0);

  return gtk_adjustment_get_value (priv->adjustment);
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gdouble val;

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (priv->update_policy != policy)
    {
      priv->update_policy = policy;
      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_UPDATE_POLICY]);
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), GTK_UPDATE_ALWAYS);

  return priv->update_policy;
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  numeric = numeric != FALSE;

  if (priv->numeric != numeric)
    {
      priv->numeric = numeric;
      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_NUMERIC]);
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return priv->numeric;
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  wrap = wrap != FALSE;

  if (priv->wrap != wrap)
    {
      priv->wrap = wrap;
      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_WRAP]);

      update_buttons_sensitivity (spin_button);
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return priv->wrap;
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  guint new_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  new_val = (snap_to_ticks != 0);

  if (new_val != priv->snap_to_ticks)
    {
      priv->snap_to_ticks = new_val;
      if (new_val && gtk_editable_get_editable (GTK_EDITABLE (priv->entry)))
        gtk_spin_button_update (spin_button);

      g_object_notify_by_pspec (G_OBJECT (spin_button), spinbutton_props[PROP_SNAP_TO_TICKS]);
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return priv->snap_to_ticks;
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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  GtkAdjustment *adjustment;
  gdouble diff;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
  GtkSpinButtonPrivate *priv = gtk_spin_button_get_instance_private (spin_button);
  gdouble val;
  gint error = 0;
  gint return_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

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
