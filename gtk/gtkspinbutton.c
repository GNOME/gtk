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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>

#include "gtkbindings.h"
#include "gtkspinbutton.h"
#include "gtkentryprivate.h"
#include "gtkmainprivate.h"
#include "gtkmarshalers.h"
#include "gtksettings.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"

#define MIN_SPIN_BUTTON_WIDTH 30
#define MAX_TIMER_CALLS       5
#define EPSILON               1e-10
#define MAX_DIGITS            20
#define MIN_ARROW_WIDTH       6


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
 * <example>
 * <title>Using a GtkSpinButton to get an integer</title>
 * <programlisting>
 * /&ast; Provides a function to retrieve an integer value from a
 *  &ast; GtkSpinButton and creates a spin button to model percentage
 *  &ast; values.
 *  &ast;/
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
 *   /&ast; creates the spinbutton, with no decimal places &ast;/
 *   button = gtk_spin_button_new (adjustment, 1.0, 0);
 *   gtk_container_add (GTK_CONTAINER (window), button);
 *
 *   gtk_widget_show_all (window);
 * }
 * </programlisting>
 * </example>
 *
 * <example>
 * <title>Using a GtkSpinButton to get a floating point value</title>
 * <programlisting>
 * /&ast; Provides a function to retrieve a floating point value from a
 *  &ast; GtkSpinButton, and creates a high precision spin button.
 *  &ast;/
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
 *   /&ast; creates the spinbutton, with three decimal places &ast;/
 *   button = gtk_spin_button_new (adjustment, 0.001, 3);
 *   gtk_container_add (GTK_CONTAINER (window), button);
 *
 *   gtk_widget_show_all (window);
 * }
 * </programlisting>
 * </example>
 */

struct _GtkSpinButtonPrivate
{
  GtkSpinButtonUpdatePolicy update_policy;
  GtkAdjustment *adjustment;

  GdkWindow     *panel;

  guint32        timer;

  gdouble        climb_rate;
  gdouble        timer_step;

  guint          button        : 2;
  guint          click_child   : 2; /* valid: GTK_ARROW_UP=0, GTK_ARROW_DOWN=1 or 2=NONE/BOTH */
  guint          digits        : 10;
  guint          in_child      : 2;
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
  PROP_VALUE
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
static void gtk_spin_button_style_updated  (GtkWidget          *widget);
static void gtk_spin_button_draw_arrow     (GtkSpinButton      *spin_button,
                                            GtkStyleContext    *context,
                                            cairo_t            *cr,
                                            GtkArrowType        arrow_type);
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
static gint gtk_spin_button_default_output (GtkSpinButton      *spin_button);

static gint spin_button_get_arrow_size     (GtkSpinButton      *spin_button);

static guint spinbutton_signals[LAST_SIGNAL] = {0};

#define NO_ARROW 2

G_DEFINE_TYPE_WITH_CODE (GtkSpinButton, gtk_spin_button, GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_spin_button_editable_init))

#define add_spin_binding(binding_set, keyval, mask, scroll)            \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,             \
                                "change_value", 1,                     \
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
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_uint ("digits",
                                                      P_("Digits"),
                                                      P_("The number of decimal places to display"),
                                                      0,
                                                      MAX_DIGITS,
                                                      0,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_TO_TICKS,
                                   g_param_spec_boolean ("snap-to-ticks",
                                                         P_("Snap to Ticks"),
                                                         P_("Whether erroneous values are automatically changed to a spin button's nearest step increment"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_NUMERIC,
                                   g_param_spec_boolean ("numeric",
                                                         P_("Numeric"),
                                                         P_("Whether non-numeric characters should be ignored"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                         P_("Wrap"),
                                                         P_("Whether a spin button should wrap upon reaching its limits"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_UPDATE_POLICY,
                                   g_param_spec_enum ("update-policy",
                                                      P_("Update Policy"),
                                                      P_("Whether the spin button should update always, or only when the value is legal"),
                                                      GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
                                                      GTK_UPDATE_ALWAYS,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
                                                        P_("Value"),
                                                        P_("Reads the current value, or sets a new value"),
                                                        -G_MAXDOUBLE,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        GTK_PARAM_READWRITE));

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
   * @spin_button: the object which received the signal
   *
   * The ::output signal can be used to change to formatting
   * of the value that is displayed in the spin buttons entry.
   * |[
   * /&ast; show leading zeros &ast;/
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
   * @spinbutton: the object which received the signal
   *
   * The wrapped signal is emitted right after the spinbutton wraps
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

  g_type_class_add_private (class, sizeof (GtkSpinButtonPrivate));
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
      if (!adjustment)
        adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv;
  GtkStyleContext *context;

  spin_button->priv = G_TYPE_INSTANCE_GET_PRIVATE (spin_button,
                                                   GTK_TYPE_SPIN_BUTTON,
                                                   GtkSpinButtonPrivate);
  priv = spin_button->priv;

  priv->adjustment = NULL;
  priv->panel = NULL;
  priv->timer = 0;
  priv->climb_rate = 0.0;
  priv->timer_step = 0.0;
  priv->update_policy = GTK_UPDATE_ALWAYS;
  priv->in_child = NO_ARROW;
  priv->click_child = NO_ARROW;
  priv->button = 0;
  priv->need_timer = FALSE;
  priv->timer_calls = 0;
  priv->digits = 0;
  priv->numeric = FALSE;
  priv->wrap = FALSE;
  priv->snap_to_ticks = FALSE;

  gtk_spin_button_set_adjustment (spin_button,
                                  gtk_adjustment_new (0, 0, 0, 0, 0, 0));

  context = gtk_widget_get_style_context (GTK_WIDGET (spin_button));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SPINBUTTON);
}

static void
gtk_spin_button_finalize (GObject *object)
{
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (object), NULL);

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
      gdk_window_show (priv->panel);
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

      gdk_window_hide (priv->panel);
      GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->unmap (widget);
    }
}

static void
gtk_spin_button_realize (GtkWidget *widget)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkAllocation allocation;
  GtkRequisition requisition;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gboolean return_val;
  gint arrow_size;
  GtkBorder padding;

  arrow_size = spin_button_get_arrow_size (spin_button);

  gtk_widget_get_preferred_size (widget, &requisition, NULL);
  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_events (widget, gtk_widget_get_events (widget) |
                         GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->realize (widget);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  state = gtk_widget_get_state_flags (widget);
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (context, state, &padding);

  attributes.x = allocation.x + allocation.width - arrow_size - (padding.left + padding.right);
  attributes.y = allocation.y + (allocation.height - requisition.height) / 2;
  attributes.width = arrow_size + padding.left + padding.right;
  attributes.height = requisition.height;

  priv->panel = gdk_window_new (gtk_widget_get_window (widget),
                                &attributes, attributes_mask);
  gdk_window_set_user_data (priv->panel, widget);

  return_val = FALSE;
  g_signal_emit (spin_button, spinbutton_signals[OUTPUT], 0, &return_val);
  if (return_val == FALSE)
    gtk_spin_button_default_output (spin_button);

  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

static void
gtk_spin_button_unrealize (GtkWidget *widget)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->unrealize (widget);

  if (priv->panel)
    {
      gdk_window_set_user_data (priv->panel, NULL);
      gdk_window_destroy (priv->panel);
      priv->panel = NULL;
    }
}

static int
compute_double_length (double val, int digits)
{
  int a;
  int extra;

  a = 1;
  if (fabs (val) > 1.0)
    a = floor (log10 (fabs (val))) + 1;

  extra = 0;

  /* The dot: */
  if (digits > 0)
    extra++;

  /* The sign: */
  if (val < 0)
    extra++;

  return a + digits + extra;
}

static void
gtk_spin_button_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GtkSpinButton *spin_button = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkStyleContext *style_context;
  GtkBorder padding;
  gint arrow_size;

  style_context = gtk_widget_get_style_context (widget);

  arrow_size = spin_button_get_arrow_size (spin_button);

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->get_preferred_width (widget, minimum, natural);

  if (gtk_entry_get_width_chars (entry) < 0)
    {
      PangoContext *context;
      const PangoFontDescription *font_desc;
      PangoFontMetrics *metrics;
      gint width;
      gint w;
      gint string_len;
      gint max_string_len;
      gint digit_width;
      gboolean interior_focus;
      gint focus_width;
      gint xborder, yborder;
      GtkBorder inner_border;

      gtk_widget_style_get (widget,
                            "interior-focus", &interior_focus,
                            "focus-line-width", &focus_width,
                            NULL);

      font_desc = gtk_style_context_get_font (style_context, 0);

      context = gtk_widget_get_pango_context (widget);
      metrics = pango_context_get_metrics (context, font_desc,
                                           pango_context_get_language (context));

      digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
      digit_width = PANGO_SCALE *
        ((digit_width + PANGO_SCALE - 1) / PANGO_SCALE);

      pango_font_metrics_unref (metrics);

      /* Get max of MIN_SPIN_BUTTON_WIDTH, size of upper, size of lower */
      width = MIN_SPIN_BUTTON_WIDTH;
      max_string_len = MAX (10, compute_double_length (1e9 * gtk_adjustment_get_step_increment (priv->adjustment),
                                                       priv->digits));

      string_len = compute_double_length (gtk_adjustment_get_upper (priv->adjustment),
                                          priv->digits);
      w = PANGO_PIXELS (MIN (string_len, max_string_len) * digit_width);
      width = MAX (width, w);
      string_len = compute_double_length (gtk_adjustment_get_lower (priv->adjustment), priv->digits);
      w = PANGO_PIXELS (MIN (string_len, max_string_len) * digit_width);
      width = MAX (width, w);

      _gtk_entry_get_borders (entry, &xborder, &yborder);
      _gtk_entry_effective_inner_border (entry, &inner_border);

      width += xborder * 2 + inner_border.left + inner_border.right;

      *minimum = width;
      *natural = width;
    }

  gtk_style_context_get_padding (style_context,
                                 gtk_widget_get_state_flags (widget),
                                 &padding);

  *minimum += arrow_size + padding.left + padding.right;
  *natural += arrow_size + padding.left + padding.right;
}

static void
gtk_spin_button_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;
  GtkAllocation panel_allocation;
  GtkRequisition requisition;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint arrow_size;
  gint panel_width;

  arrow_size = spin_button_get_arrow_size (spin);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, &padding);
  panel_width = arrow_size + padding.left + padding.right;

  gtk_widget_get_preferred_size (widget, &requisition, NULL);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    panel_allocation.x = allocation->x;
  else
    panel_allocation.x = allocation->x + allocation->width - panel_width;

  panel_allocation.width = panel_width;
  panel_allocation.height = MIN (requisition.height, allocation->height);

  panel_allocation.y = allocation->y +
                       (allocation->height - requisition.height) / 2;

  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->panel,
                              panel_allocation.x,
                              panel_allocation.y,
                              panel_allocation.width,
                              panel_allocation.height);
    }

  gtk_widget_queue_draw (GTK_WIDGET (spin));
}

static gint
gtk_spin_button_draw (GtkWidget      *widget,
                      cairo_t        *cr)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;
  GtkStyleContext *context;
  GtkStateFlags state = 0;
  gboolean is_rtl;

  is_rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  context = gtk_widget_get_style_context (widget);

  cairo_save (cr);
  GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->draw (widget, cr);
  cairo_restore (cr);

  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);
  gtk_style_context_remove_class (context, GTK_STYLE_CLASS_ENTRY);

  if (is_rtl)
    gtk_style_context_set_junction_sides (context, GTK_JUNCTION_RIGHT);
  else
    gtk_style_context_set_junction_sides (context, GTK_JUNCTION_LEFT);

  gtk_cairo_transform_to_window (cr, widget, priv->panel);

  if (gtk_entry_get_has_frame (GTK_ENTRY (widget)))
    gtk_render_background (context, cr, 0, 0,
                           gdk_window_get_width (priv->panel),
                           gdk_window_get_height (priv->panel));

  gtk_spin_button_draw_arrow (spin, context, cr, GTK_ARROW_UP);
  gtk_spin_button_draw_arrow (spin, context, cr, GTK_ARROW_DOWN);

  gtk_style_context_restore (context);

  return FALSE;
}

static gboolean
spin_button_at_limit (GtkSpinButton *spin_button,
                     GtkArrowType   arrow)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  GtkArrowType effective_arrow;

  if (priv->wrap)
    return FALSE;

  if (gtk_adjustment_get_step_increment (priv->adjustment) > 0)
    effective_arrow = arrow;
  else
    effective_arrow = arrow == GTK_ARROW_UP ? GTK_ARROW_DOWN : GTK_ARROW_UP;

  if (effective_arrow == GTK_ARROW_UP &&
      (gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_value (priv->adjustment) <= EPSILON))
    return TRUE;

  if (effective_arrow == GTK_ARROW_DOWN &&
      (gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment) <= EPSILON))
    return TRUE;

  return FALSE;
}

static void
gtk_spin_button_draw_arrow (GtkSpinButton   *spin_button,
                            GtkStyleContext *context,
                            cairo_t         *cr,
                            GtkArrowType     arrow_type)
{
  GtkSpinButtonPrivate *priv;
  GtkJunctionSides junction;
  GtkStateFlags state;
  GtkWidget *widget;
  GtkBorder padding;
  gdouble angle;
  gint x;
  gint y;
  gint panel_height;
  gint height;
  gint width;
  gint h, w;

  g_return_if_fail (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

  priv = spin_button->priv;
  widget = GTK_WIDGET (spin_button);
  junction = gtk_style_context_get_junction_sides (context);

  panel_height = gdk_window_get_height (priv->panel);

  if (arrow_type == GTK_ARROW_UP)
    {
      x = 0;
      y = 0;

      height = panel_height / 2;
      angle = 0;
      junction |= GTK_JUNCTION_BOTTOM;
    }
  else
    {
      x = 0;
      y = panel_height / 2;

      height = (panel_height + 1) / 2;
      angle = G_PI;
      junction |= GTK_JUNCTION_TOP;
    }

  if (spin_button_at_limit (spin_button, arrow_type))
    state = GTK_STATE_FLAG_INSENSITIVE;
  else
    {
      if (priv->click_child == arrow_type)
        state = GTK_STATE_ACTIVE;
      else
        {
          if (priv->in_child == arrow_type &&
              priv->click_child == NO_ARROW)
            state = GTK_STATE_FLAG_PRELIGHT;
          else
            state = gtk_widget_get_state_flags (widget);
        }
    }

  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_set_junction_sides (context, junction);
  gtk_style_context_set_state (context, state);

  width = spin_button_get_arrow_size (spin_button) + padding.left + padding.right;

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  height = panel_height;

  if (arrow_type == GTK_ARROW_DOWN)
    {
      y = height / 2;
      height = height - y - 2;
    }
  else
    {
      y = 2;
      height = height / 2 - 2;
    }

  width -= 3;

  if (widget && gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = 2;
  else
    x = 1;

  w = width / 2;
  w -= w % 2 - 1; /* force odd */
  h = (w + 1) / 2;

  x += (width - w) / 2;
  y += (height - h) / 2;

  height = h;
  width = w;

  gtk_render_arrow (context, cr,
                    angle, x, y, width);

  gtk_style_context_restore (context);
}

static gint
gtk_spin_button_enter_notify (GtkWidget        *widget,
                              GdkEventCrossing *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;
  GtkRequisition requisition;

  if (event->window == priv->panel)
    {
      GdkDevice *device;
      gint x;
      gint y;

      device = gdk_event_get_device ((GdkEvent *) event);
      gdk_window_get_device_position (priv->panel, device, &x, &y, NULL);

      gtk_widget_get_preferred_size (widget, &requisition, NULL);

      if (y <= requisition.height / 2)
        priv->in_child = GTK_ARROW_UP;
      else
        priv->in_child = GTK_ARROW_DOWN;

      gtk_widget_queue_draw (GTK_WIDGET (spin));
    }

  if (GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->enter_notify_event)
    return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->enter_notify_event (widget, event);

  return FALSE;
}

static gint
gtk_spin_button_leave_notify (GtkWidget        *widget,
                              GdkEventCrossing *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  priv->in_child = NO_ARROW;
  gtk_widget_queue_draw (GTK_WIDGET (spin));

  if (GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->leave_notify_event)
    return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->leave_notify_event (widget, event);

  return FALSE;
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

  if (gtk_widget_get_realized (widget))
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_set_background (context, priv->panel);
    }

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

  priv->click_child = NO_ARROW;

  return did_spin;
}

static void
start_spinning (GtkSpinButton *spin,
                GtkArrowType   click_child,
                gdouble        step)
{
  GtkSpinButtonPrivate *priv;

  g_return_if_fail (click_child == GTK_ARROW_UP || click_child == GTK_ARROW_DOWN);

  priv = spin->priv;

  priv->click_child = click_child;

  if (!priv->timer)
    {
      GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (spin));
      guint        timeout;

      g_object_get (settings, "gtk-timeout-initial", &timeout, NULL);

      priv->timer_step = step;
      priv->need_timer = TRUE;
      priv->timer = gdk_threads_add_timeout (timeout,
                                   (GSourceFunc) gtk_spin_button_timer,
                                   (gpointer) spin);
    }
  gtk_spin_button_real_spin (spin, click_child == GTK_ARROW_UP ? step : -step);

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
      if (event->window == priv->panel)
        {
          GtkRequisition requisition;

          if (!gtk_widget_has_focus (widget))
            gtk_widget_grab_focus (widget);
          priv->button = event->button;

          if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
            gtk_spin_button_update (spin);

          gtk_widget_get_preferred_size (widget, &requisition, NULL);

          if (event->y <= requisition.height / 2)
            {
              if (event->button == 1)
                start_spinning (spin, GTK_ARROW_UP, gtk_adjustment_get_step_increment (priv->adjustment));
              else if (event->button == 2)
                start_spinning (spin, GTK_ARROW_UP, gtk_adjustment_get_page_increment (priv->adjustment));
              else
                priv->click_child = GTK_ARROW_UP;
            }
          else
            {
              if (event->button == 1)
                start_spinning (spin, GTK_ARROW_DOWN, gtk_adjustment_get_step_increment (priv->adjustment));
              else if (event->button == 2)
                start_spinning (spin, GTK_ARROW_DOWN, gtk_adjustment_get_page_increment (priv->adjustment));
              else
                priv->click_child = GTK_ARROW_DOWN;
            }
          return TRUE;
        }
      else
        return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->button_press_event (widget, event);
    }
  return FALSE;
}

static gint
gtk_spin_button_button_release (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;
  gint arrow_size;

  arrow_size = spin_button_get_arrow_size (spin);

  if (event->button == priv->button)
    {
      int click_child = priv->click_child;

      gtk_spin_button_stop_spinning (spin);

      if (event->button == 3)
        {
          GtkRequisition requisition;
          GtkStyleContext *context;
          GtkStateFlags state;
          GtkBorder padding;

          gtk_widget_get_preferred_size (widget, &requisition, NULL);

          context = gtk_widget_get_style_context (widget);
          state = gtk_widget_get_state_flags (widget);
          gtk_style_context_get_padding (context, state, &padding);

          if (event->y >= 0 && event->x >= 0 &&
              event->y <= requisition.height &&
              event->x <= arrow_size + padding.left + padding.right)
            {
              if (click_child == GTK_ARROW_UP &&
                  event->y <= requisition.height / 2)
                {
                  gdouble diff;

                  diff = gtk_adjustment_get_upper (priv->adjustment) - gtk_adjustment_get_value (priv->adjustment);
                  if (diff > EPSILON)
                    gtk_spin_button_real_spin (spin, diff);
                }
              else if (click_child == GTK_ARROW_DOWN &&
                       event->y > requisition.height / 2)
                {
                  gdouble diff;

                  diff = gtk_adjustment_get_value (priv->adjustment) - gtk_adjustment_get_lower (priv->adjustment);
                  if (diff > EPSILON)
                    gtk_spin_button_real_spin (spin, -diff);
                }
            }
        }
      gtk_widget_queue_draw (GTK_WIDGET (spin));

      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->button_release_event (widget, event);
}

static gint
gtk_spin_button_motion_notify (GtkWidget      *widget,
                               GdkEventMotion *event)
{
  GtkSpinButton *spin = GTK_SPIN_BUTTON (widget);
  GtkSpinButtonPrivate *priv = spin->priv;

  if (priv->button)
    return FALSE;

  if (event->window == priv->panel)
    {
      GtkRequisition requisition;
      gint y = event->y;

      gdk_event_request_motions (event);

      gtk_widget_get_preferred_size (widget, &requisition, NULL);

      if (y <= requisition.height / 2 &&
          priv->in_child == GTK_ARROW_DOWN)
        {
          priv->in_child = GTK_ARROW_UP;
          gtk_widget_queue_draw (GTK_WIDGET (spin));
        }
      else if (y > requisition.height / 2 &&
          priv->in_child == GTK_ARROW_UP)
        {
          priv->in_child = GTK_ARROW_DOWN;
          gtk_widget_queue_draw (GTK_WIDGET (spin));
        }

      return FALSE;
    }

  return GTK_WIDGET_CLASS (gtk_spin_button_parent_class)->motion_notify_event (widget, event);
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;
  gboolean retval = FALSE;

  if (priv->timer)
    {
      if (priv->click_child == GTK_ARROW_UP)
        gtk_spin_button_real_spin (spin_button, priv->timer_step);
      else
        gtk_spin_button_real_spin (spin_button, -priv->timer_step);

      if (priv->need_timer)
        {
          GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (spin_button));
          guint        timeout;

          g_object_get (settings, "gtk-timeout-repeat", &timeout, NULL);

          priv->need_timer = FALSE;
          priv->timer = gdk_threads_add_timeout (timeout,
                                              (GSourceFunc) gtk_spin_button_timer,
                                              (gpointer) spin_button);
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

  /* When the key binding is activated, there may be an outstanding
   * value, so we first have to commit what is currently written in
   * the spin buttons text entry. See #106574
   */
  gtk_spin_button_update (spin);

  old_value = gtk_adjustment_get_value (priv->adjustment);

  /* We don't test whether the entry is editable, since
   * this key binding conceptually corresponds to changing
   * the value with the buttons using the mouse, which
   * we allow for non-editable spin buttons.
   */
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
gtk_spin_button_get_text_area_size (GtkEntry *entry,
                                    gint     *x,
                                    gint     *y,
                                    gint     *width,
                                    gint     *height)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkWidget *widget;
  GtkBorder padding;
  gint arrow_size;
  gint panel_width;

  GTK_ENTRY_CLASS (gtk_spin_button_parent_class)->get_text_area_size (entry, x, y, width, height);

  widget = GTK_WIDGET (entry);
  state = gtk_widget_get_state_flags (widget);
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_padding (context, state, &padding);

  arrow_size = spin_button_get_arrow_size (GTK_SPIN_BUTTON (entry));
  panel_width = arrow_size + padding.left + padding.right;

  if (width)
    *width -= panel_width;

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL && x)
    *x += panel_width;
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

static gint
gtk_spin_button_default_output (GtkSpinButton *spin_button)
{
  GtkSpinButtonPrivate *priv = spin_button->priv;

  gchar *buf = g_strdup_printf ("%0.*f", priv->digits, gtk_adjustment_get_value (priv->adjustment));

  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  g_free (buf);
  return FALSE;
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

  if (adjustment)
    gtk_spin_button_set_adjustment (spin_button, adjustment);
  else
    adjustment = priv->adjustment;

  g_object_freeze_notify (G_OBJECT (spin_button));
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
 * Return value: The new spin button as a #GtkWidget
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

/* Callback used when the spin button's adjustment changes.
 * We need to redraw the arrows when the adjustment's range
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

  if (priv->adjustment != adjustment)
    {
      if (priv->adjustment)
        {
          g_signal_handlers_disconnect_by_func (priv->adjustment,
                                                gtk_spin_button_value_changed,
                                                spin_button);
          g_signal_handlers_disconnect_by_func (priv->adjustment,
                                                adjustment_changed_cb,
                                                spin_button);
          g_object_unref (priv->adjustment);
        }
      priv->adjustment = adjustment;
      if (adjustment)
        {
          g_object_ref_sink (adjustment);
          g_signal_connect (adjustment, "value-changed",
                            G_CALLBACK (gtk_spin_button_value_changed),
                            spin_button);
          g_signal_connect (adjustment, "changed",
                            G_CALLBACK (adjustment_changed_cb),
                            spin_button);
          priv->timer_step = gtk_adjustment_get_step_increment (priv->adjustment);
        }

      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }

  g_object_notify (G_OBJECT (spin_button), "adjustment");
}

/**
 * gtk_spin_button_get_adjustment:
 * @spin_button: a #GtkSpinButton
 *
 * Get the adjustment associated with a #GtkSpinButton
 *
 * Return value: (transfer none): the #GtkAdjustment of @spin_button
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
 * @digits: the number of digits after the decimal point to be displayed for the spin button's value
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
 * quickly the value changes when the spin button's arrows are activated.
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
 * Return value: the value of @spin_button
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
 * Return value: the value of @spin_button
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
      if (return_val == FALSE)
        gtk_spin_button_default_output (spin_button);
    }
}

/**
 * gtk_spin_button_set_update_policy:
 * @spin_button: a #GtkSpinButton
 * @policy: a #GtkSpinButtonUpdatePolicy value
 *
 * Sets the update behavior of a spin button.
 * This determines wether the spin button is always updated
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
 * Return value: the current update policy
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
 * Return value: %TRUE if only numeric text can be entered
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
 * Returns whether the spin button's value wraps around to the
 * opposite limit when the upper or lower limit of the range is
 * exceeded. See gtk_spin_button_set_wrap().
 *
 * Return value: %TRUE if the spin button wraps around
 */
gboolean
gtk_spin_button_get_wrap (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->priv->wrap;
}

static gint
spin_button_get_arrow_size (GtkSpinButton *spin_button)
{
  const PangoFontDescription *font_desc;
  GtkStyleContext *context;
  gint size;
  gint arrow_size;

  /* FIXME: use getter */
  context = gtk_widget_get_style_context (GTK_WIDGET (spin_button));
  font_desc = gtk_style_context_get_font (context, 0);

  size = pango_font_description_get_size (font_desc);
  arrow_size = MAX (PANGO_PIXELS (size), MIN_ARROW_WIDTH);

  return arrow_size - arrow_size % 2; /* force even */
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
 * Return value: %TRUE if values are snapped to the nearest step
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
 * Increment or decrement a spin button's value in a specified
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

GdkWindow *
_gtk_spin_button_get_panel (GtkSpinButton *spin_button)
{
  return spin_button->priv->panel;
}
