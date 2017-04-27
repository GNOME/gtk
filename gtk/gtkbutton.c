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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/**
 * SECTION:gtkbutton
 * @Short_description: A widget that emits a signal when clicked on
 * @Title: GtkButton
 *
 * The #GtkButton widget is generally used to trigger a callback function that is
 * called when the button is pressed.  The various signals and how to use them
 * are outlined below.
 *
 * The #GtkButton widget can hold any valid child widget.  That is, it can hold
 * almost any other standard #GtkWidget.  The most commonly used child is the
 * #GtkLabel.
 *
 * # CSS nodes
 *
 * GtkButton has a single CSS node with name button. The node will get the
 * style classes .image-button or .text-button, if the content is just an
 * image or label, respectively. It may also receive the .flat style class.
 *
 * Other style classes that are commonly used with GtkButton include
 * .suggested-action and .destructive-action. In special cases, buttons
 * can be made round by adding the .circular style class.
 *
 * Button-like widgets like #GtkToggleButton, #GtkMenuButton, #GtkVolumeButton,
 * #GtkLockButton, #GtkColorButton, #GtkFontButton or #GtkFileChooserButton use
 * style classes such as .toggle, .popup, .scale, .lock, .color, .font, .file
 * to differentiate themselves from a plain GtkButton.
 */

#include "config.h"

#include "gtkbutton.h"
#include "gtkbuttonprivate.h"

#include <string.h>
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkimage.h"
#include "gtkbox.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkbuttonaccessible.h"
#include "gtkapplicationprivate.h"
#include "gtkactionhelper.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkcontainerprivate.h"

/* Time out before giving up on getting a key release when animating
 * the close button.
 */
#define ACTIVATE_TIMEOUT 250


enum {
  CLICKED,
  ENTER,
  LEAVE,
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_RELIEF,
  PROP_USE_UNDERLINE,
  PROP_ICON_NAME,

  /* actionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  LAST_PROP = PROP_ACTION_NAME
};

enum {
  LABEL_CHILD,
  ICON_CHILD,
  WIDGET_CHILD
};


static void gtk_button_finalize       (GObject            *object);
static void gtk_button_dispose        (GObject            *object);
static void gtk_button_set_property   (GObject            *object,
                                       guint               prop_id,
                                       const GValue       *value,
                                       GParamSpec         *pspec);
static void gtk_button_get_property   (GObject            *object,
                                       guint               prop_id,
                                       GValue             *value,
                                       GParamSpec         *pspec);
static void gtk_button_screen_changed (GtkWidget          *widget,
				       GdkScreen          *previous_screen);
static void gtk_button_realize (GtkWidget * widget);
static void gtk_button_unrealize (GtkWidget * widget);
static void gtk_button_map (GtkWidget * widget);
static void gtk_button_unmap (GtkWidget * widget);
static void gtk_button_size_allocate (GtkWidget * widget,
				      GtkAllocation * allocation);
static void gtk_button_snapshot (GtkWidget * widget, GtkSnapshot *snapshot);
static gint gtk_button_grab_broken (GtkWidget * widget,
				    GdkEventGrabBroken * event);
static gint gtk_button_key_release (GtkWidget * widget, GdkEventKey * event);
static gint gtk_button_enter_notify (GtkWidget * widget,
				     GdkEventCrossing * event);
static gint gtk_button_leave_notify (GtkWidget * widget,
				     GdkEventCrossing * event);
static void gtk_real_button_clicked (GtkButton * button);
static void gtk_real_button_activate  (GtkButton          *button);
static void gtk_button_update_state   (GtkButton          *button);
static void gtk_button_finish_activate (GtkButton         *button,
					gboolean           do_it);

static void gtk_button_state_flags_changed (GtkWidget     *widget,
                                            GtkStateFlags  previous_state);
static void gtk_button_grab_notify     (GtkWidget             *widget,
					gboolean               was_grabbed);
static void gtk_button_do_release      (GtkButton             *button,
                                        gboolean               emit_clicked);

static void gtk_button_actionable_iface_init     (GtkActionableInterface *iface);

static void gtk_button_measure_ (GtkWidget      *widget,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural,
                                 int            *minimum_baseline,
                                 int            *natural_baseline);
static void     gtk_button_measure  (GtkCssGadget        *gadget,
                                     GtkOrientation       orientation,
                                     int                  for_size,
                                     int                 *minimum_size,
                                     int                 *natural_size,
                                     int                 *minimum_baseline,
                                     int                 *natural_baseline,
                                     gpointer             data);
static void     gtk_button_allocate (GtkCssGadget        *gadget,
                                     const GtkAllocation *allocation,
                                     int                  baseline,
                                     GtkAllocation       *out_clip,
                                     gpointer             data);
static void gtk_button_set_child_type (GtkButton *button, guint child_type);
static gboolean gtk_button_render   (GtkCssGadget        *gadget,
                                     GtkSnapshot         *snapshot,
                                     int                  x,
                                     int                  y,
                                     int                  width,
                                     int                  height,
                                     gpointer             data);

static GParamSpec *props[LAST_PROP] = { NULL, };
static guint button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkButton, gtk_button, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_button_actionable_iface_init))

static void
gtk_button_add (GtkContainer *container, GtkWidget *child)
{
  GtkButton *button = GTK_BUTTON (container);

  if (button->priv->child_type != WIDGET_CHILD)
    gtk_container_remove (container, gtk_bin_get_child (GTK_BIN (button)));

  gtk_button_set_child_type (button, WIDGET_CHILD);

  GTK_CONTAINER_CLASS (gtk_button_parent_class)->add (container, child);
}

static void
gtk_button_remove (GtkContainer *container, GtkWidget *child)
{
  GtkButton *button = GTK_BUTTON (container);

  gtk_button_set_child_type (button, WIDGET_CHILD);

  GTK_CONTAINER_CLASS (gtk_button_parent_class)->remove (container, child);
}

static void
gtk_button_class_init (GtkButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = (GtkWidgetClass*) klass;
  container_class = GTK_CONTAINER_CLASS (klass);

  gobject_class->dispose      = gtk_button_dispose;
  gobject_class->finalize     = gtk_button_finalize;
  gobject_class->set_property = gtk_button_set_property;
  gobject_class->get_property = gtk_button_get_property;

  widget_class->measure = gtk_button_measure_;
  widget_class->screen_changed = gtk_button_screen_changed;
  widget_class->realize = gtk_button_realize;
  widget_class->unrealize = gtk_button_unrealize;
  widget_class->map = gtk_button_map;
  widget_class->unmap = gtk_button_unmap;
  widget_class->size_allocate = gtk_button_size_allocate;
  widget_class->snapshot = gtk_button_snapshot;
  widget_class->grab_broken_event = gtk_button_grab_broken;
  widget_class->key_release_event = gtk_button_key_release;
  widget_class->enter_notify_event = gtk_button_enter_notify;
  widget_class->leave_notify_event = gtk_button_leave_notify;
  widget_class->state_flags_changed = gtk_button_state_flags_changed;
  widget_class->grab_notify = gtk_button_grab_notify;

  container_class->add    = gtk_button_add;
  container_class->remove = gtk_button_remove;

  klass->clicked = NULL;
  klass->activate = gtk_real_button_activate;

  props[PROP_LABEL] =
    g_param_spec_string ("label",
                         P_("Label"),
                         P_("Text of the label widget inside the button, if the button contains a label widget"),
                         NULL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline",
                          P_("Use underline"),
                          P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_RELIEF] =
    g_param_spec_enum ("relief",
                       P_("Border relief"),
                       P_("The border relief style"),
                       GTK_TYPE_RELIEF_STYLE,
                       GTK_RELIEF_NORMAL,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         P_("Icon Name"),
                         P_("The name of the icon used to automatically populate the button"),
                         NULL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);


  g_object_class_install_properties (gobject_class, LAST_PROP, props);

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  /**
   * GtkButton::clicked:
   * @button: the object that received the signal
   *
   * Emitted when the button has been activated (pressed and released).
   */
  button_signals[CLICKED] =
    g_signal_new (I_("clicked"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkButtonClass, clicked),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::activate:
   * @widget: the object which received the signal.
   *
   * The ::activate signal on GtkButton is an action signal and
   * emitting it causes the button to animate press then release.
   * Applications should never connect to this signal, but use the
   * #GtkButton::clicked signal.
   */
  button_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkButtonClass, activate),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = button_signals[ACTIVATE];

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_BUTTON_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "button");
}

static void
multipress_pressed_cb (GtkGestureMultiPress *gesture,
                       guint                 n_press,
                       gdouble               x,
                       gdouble               y,
                       GtkWidget            *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  priv->in_button = TRUE;

  if (!priv->activate_timeout)
    {
      priv->button_down = TRUE;
      gtk_button_update_state (button);
    }
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static gboolean
touch_release_in_button (GtkButton *button)
{
  GtkButtonPrivate *priv;
  gint width, height;
  GdkEvent *event;
  gdouble x, y;

  priv = button->priv;
  event = gtk_get_current_event ();

  if (!event)
    return FALSE;

  if (event->type != GDK_TOUCH_END ||
      event->touch.window != priv->event_window)
    {
      gdk_event_free (event);
      return FALSE;
    }

  gdk_event_get_coords (event, &x, &y);
  width = gdk_window_get_width (priv->event_window);
  height = gdk_window_get_height (priv->event_window);

  gdk_event_free (event);

  if (x >= 0 && x <= width &&
      y >= 0 && y <= height)
    return TRUE;

  return FALSE;
}

static void
multipress_released_cb (GtkGestureMultiPress *gesture,
                        guint                 n_press,
                        gdouble               x,
                        gdouble               y,
                        GtkWidget            *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  GdkEventSequence *sequence;

  gtk_button_do_release (button,
                         gtk_widget_is_sensitive (GTK_WIDGET (button)) &&
                         (button->priv->in_button ||
                          touch_release_in_button (button)));

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (sequence)
    {
      priv->in_button = FALSE;
      gtk_button_update_state (button);
    }
}

static void
multipress_gesture_update_cb (GtkGesture       *gesture,
                              GdkEventSequence *sequence,
                              GtkButton        *button)
{
  GtkButtonPrivate *priv = button->priv;
  GtkAllocation allocation;
  gboolean in_button;
  gdouble x, y;

  if (sequence != gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)))
    return;

  gtk_widget_get_allocation (GTK_WIDGET (button), &allocation);
  gtk_gesture_get_point (gesture, sequence, &x, &y);

  in_button = (x >= 0 && y >= 0 && x < allocation.width && y < allocation.height);

  if (priv->in_button != in_button)
    {
      priv->in_button = in_button;
      gtk_button_update_state (button);
    }
}

static void
multipress_gesture_cancel_cb (GtkGesture       *gesture,
                              GdkEventSequence *sequence,
                              GtkButton        *button)
{
  gtk_button_do_release (button, FALSE);
}

static void
gtk_button_set_child_type (GtkButton *button, guint child_type)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->child_type == child_type)
    return;

  if (child_type != LABEL_CHILD)
    g_object_notify_by_pspec (G_OBJECT (button), props[PROP_LABEL]);
  else if (child_type != ICON_CHILD)
    g_object_notify_by_pspec (G_OBJECT (button), props[PROP_ICON_NAME]);

  priv->child_type = child_type;
}

static void
gtk_button_init (GtkButton *button)
{
  GtkButtonPrivate *priv;

  button->priv = gtk_button_get_instance_private (button);
  priv = button->priv;

  gtk_widget_set_can_focus (GTK_WIDGET (button), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (button), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);

  priv->in_button = FALSE;
  priv->button_down = FALSE;
  priv->use_underline = FALSE;
  priv->child_type = WIDGET_CHILD;

  priv->gesture = gtk_gesture_multi_press_new (GTK_WIDGET (button));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (priv->gesture, "pressed", G_CALLBACK (multipress_pressed_cb), button);
  g_signal_connect (priv->gesture, "released", G_CALLBACK (multipress_released_cb), button);
  g_signal_connect (priv->gesture, "update", G_CALLBACK (multipress_gesture_update_cb), button);
  g_signal_connect (priv->gesture, "cancel", G_CALLBACK (multipress_gesture_cancel_cb), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->gesture), GTK_PHASE_BUBBLE);

  priv->gadget = gtk_css_custom_gadget_new_for_node (gtk_widget_get_css_node (GTK_WIDGET (button)),
                                                     GTK_WIDGET (button),
                                                     gtk_button_measure,
                                                     gtk_button_allocate,
                                                     gtk_button_render,
                                                     NULL,
                                                     NULL);

}

static void
gtk_button_finalize (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  g_clear_object (&priv->gesture);
  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_button_parent_class)->finalize (object);
}

static void
gtk_button_dispose (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  g_clear_object (&priv->action_helper);

  G_OBJECT_CLASS (gtk_button_parent_class)->dispose (object);
}

static void
gtk_button_set_action_name (GtkActionable *actionable,
                            const gchar   *action_name)
{
  GtkButton *button = GTK_BUTTON (actionable);

  if (!button->priv->action_helper)
    button->priv->action_helper = gtk_action_helper_new (actionable);

  g_signal_handlers_disconnect_by_func (button, gtk_real_button_clicked, NULL);
  if (action_name)
    g_signal_connect_after (button, "clicked", G_CALLBACK (gtk_real_button_clicked), NULL);

  gtk_action_helper_set_action_name (button->priv->action_helper, action_name);
}

static void
gtk_button_set_action_target_value (GtkActionable *actionable,
                                    GVariant      *action_target)
{
  GtkButton *button = GTK_BUTTON (actionable);

  if (!button->priv->action_helper)
    button->priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (button->priv->action_helper, action_target);
}

static void
gtk_button_set_property (GObject         *object,
                         guint            prop_id,
                         const GValue    *value,
                         GParamSpec      *pspec)
{
  GtkButton *button = GTK_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_RELIEF:
      gtk_button_set_relief (button, g_value_get_enum (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_ICON_NAME:
      gtk_button_set_icon_name (button, g_value_get_string (value));
      break;
    case PROP_ACTION_NAME:
      gtk_button_set_action_name (GTK_ACTIONABLE (button), g_value_get_string (value));
      break;
    case PROP_ACTION_TARGET:
      gtk_button_set_action_target_value (GTK_ACTIONABLE (button), g_value_get_variant (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_get_property (GObject         *object,
                         guint            prop_id,
                         GValue          *value,
                         GParamSpec      *pspec)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_button_get_label (button));
      break;
    case PROP_RELIEF:
      g_value_set_enum (value, gtk_button_get_relief (button));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, gtk_button_get_icon_name (button));
      break;
    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (priv->action_helper));
      break;
    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (priv->action_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const gchar *
gtk_button_get_action_name (GtkActionable *actionable)
{
  GtkButton *button = GTK_BUTTON (actionable);

  return gtk_action_helper_get_action_name (button->priv->action_helper);
}

static GVariant *
gtk_button_get_action_target_value (GtkActionable *actionable)
{
  GtkButton *button = GTK_BUTTON (actionable);

  return gtk_action_helper_get_action_target_value (button->priv->action_helper);
}

static void
gtk_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_button_get_action_name;
  iface->set_action_name = gtk_button_set_action_name;
  iface->get_action_target_value = gtk_button_get_action_target_value;
  iface->set_action_target_value = gtk_button_set_action_target_value;
}

/**
 * gtk_button_new:
 *
 * Creates a new #GtkButton widget. To add a child widget to the button,
 * use gtk_container_add().
 *
 * Returns: The newly created #GtkButton widget.
 */
GtkWidget*
gtk_button_new (void)
{
  return g_object_new (GTK_TYPE_BUTTON, NULL);
}

/**
 * gtk_button_new_with_label:
 * @label: The text you want the #GtkLabel to hold.
 *
 * Creates a #GtkButton widget with a #GtkLabel child containing the given
 * text.
 *
 * Returns: The newly created #GtkButton widget.
 */
GtkWidget*
gtk_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, NULL);
}

/**
 * gtk_button_new_from_icon_name:
 * @icon_name: (nullable): an icon name or %NULL
 * @size: (type int): an icon size (#GtkIconSize)
 *
 * Creates a new button containing an icon from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead. If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Returns: a new #GtkButton displaying the themed icon
 *
 * Since: 3.10
 */
GtkWidget*
gtk_button_new_from_icon_name (const gchar *icon_name,
                               GtkIconSize  size)
{
  GtkWidget *button;
  GtkWidget *image;

  image = gtk_image_new_from_icon_name (icon_name, size);
  button =  g_object_new (GTK_TYPE_BUTTON, NULL);
  gtk_container_add (GTK_CONTAINER (button), image);

  return button;
}

/**
 * gtk_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkButton containing a label.
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use “__” (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 *
 * Returns: a new #GtkButton
 */
GtkWidget*
gtk_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, "use-underline", TRUE,  NULL);
}

/**
 * gtk_button_clicked:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::clicked signal to the given #GtkButton.
 */
void
gtk_button_clicked (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[CLICKED], 0);
}

/**
 * gtk_button_set_relief:
 * @button: The #GtkButton you want to set relief styles of
 * @relief: The GtkReliefStyle as described above
 *
 * Sets the relief style of the edges of the given #GtkButton widget.
 * Two styles exist, %GTK_RELIEF_NORMAL and %GTK_RELIEF_NONE.
 * The default style is, as one can guess, %GTK_RELIEF_NORMAL.
 */
void
gtk_button_set_relief (GtkButton      *button,
		       GtkReliefStyle  relief)
{
  GtkStyleContext *context;
  GtkReliefStyle old_relief;

  g_return_if_fail (GTK_IS_BUTTON (button));

  old_relief = gtk_button_get_relief (button);
  if (old_relief != relief)
    {
      context = gtk_widget_get_style_context (GTK_WIDGET (button));
      if (relief == GTK_RELIEF_NONE)
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_FLAT);
      else
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_FLAT);

      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_RELIEF]);
    }
}

/**
 * gtk_button_get_relief:
 * @button: The #GtkButton you want the #GtkReliefStyle from.
 *
 * Returns the current relief style of the given #GtkButton.
 *
 * Returns: The current #GtkReliefStyle
 */
GtkReliefStyle
gtk_button_get_relief (GtkButton *button)
{
  GtkStyleContext *context;

  g_return_val_if_fail (GTK_IS_BUTTON (button), GTK_RELIEF_NORMAL);

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  if (gtk_style_context_has_class (context, GTK_STYLE_CLASS_FLAT))
    return GTK_RELIEF_NONE;
  else
    return GTK_RELIEF_NORMAL;
}

static void
gtk_button_realize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  GtkAllocation allocation;

  GTK_WIDGET_CLASS (gtk_button_parent_class)->realize (widget);

  gtk_widget_get_allocation (widget, &allocation);

  priv->event_window = gdk_window_new_input (gtk_widget_get_window (widget),
                                             gtk_widget_get_events (widget)
                                             | GDK_BUTTON_PRESS_MASK
                                             | GDK_BUTTON_RELEASE_MASK
                                             | GDK_TOUCH_MASK
                                             | GDK_ENTER_NOTIFY_MASK
                                             | GDK_LEAVE_NOTIFY_MASK,
                                             &allocation);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_button_unrealize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, FALSE);

  if (priv->event_window)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unrealize (widget);
}

static void
gtk_button_map (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  GTK_WIDGET_CLASS (gtk_button_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
gtk_button_unmap (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->event_window)
    {
      gdk_window_hide (priv->event_window);
      priv->in_button = FALSE;
    }

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unmap (widget);
}

static void
gtk_button_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);
  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_button_allocate (GtkCssGadget        *gadget,
                     const GtkAllocation *allocation,
                     int                  baseline,
                     GtkAllocation       *out_clip,
                     gpointer             unused)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_css_gadget_get_owner (gadget);

  *out_clip = *allocation;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation clip;

      gtk_widget_size_allocate_with_baseline (child, (GtkAllocation *)allocation, baseline);
      gtk_widget_get_clip (child, &clip);
      gdk_rectangle_union (&clip, out_clip, out_clip);
    }

  if (gtk_widget_get_realized (widget))
    {
      GtkAllocation border_allocation;
      gtk_css_gadget_get_border_allocation (gadget, &border_allocation, NULL);
      gdk_window_move_resize (GTK_BUTTON (widget)->priv->event_window,
                              border_allocation.x,
                              border_allocation.y,
                              border_allocation.width,
                              border_allocation.height);
    }
}

static void
gtk_button_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  gtk_css_gadget_snapshot (GTK_BUTTON (widget)->priv->gadget, snapshot);
}

static gboolean
gtk_button_render (GtkCssGadget *gadget,
                   GtkSnapshot  *snapshot,
                   int           x,
                   int           y,
                   int           width,
                   int           height,
                   gpointer      data)
{
  GtkWidget *widget;

  widget = gtk_css_gadget_get_owner (gadget);

  GTK_WIDGET_CLASS (gtk_button_parent_class)->snapshot (widget, snapshot);

  return gtk_widget_has_visible_focus (widget);
}

static void
gtk_button_do_release (GtkButton *button,
                       gboolean   emit_clicked)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->button_down)
    {
      priv->button_down = FALSE;

      if (priv->activate_timeout)
	return;

      if (emit_clicked)
        gtk_button_clicked (button);

      gtk_button_update_state (button);
    }
}

static gboolean
gtk_button_grab_broken (GtkWidget          *widget,
			GdkEventGrabBroken *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  
  gtk_button_do_release (button, FALSE);

  return TRUE;
}

static gboolean
gtk_button_key_release (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout)
    {
      gtk_button_finish_activate (button, TRUE);
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (gtk_button_parent_class)->key_release_event)
    return GTK_WIDGET_CLASS (gtk_button_parent_class)->key_release_event (widget, event);
  else
    return FALSE;
}

static gboolean
gtk_button_enter_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if ((event->window == button->priv->event_window) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      priv->in_button = TRUE;
      gtk_button_update_state (button);
    }

  return FALSE;
}

static gboolean
gtk_button_leave_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if ((event->window == button->priv->event_window) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      priv->in_button = FALSE;
      gtk_button_update_state (button);
    }

  return FALSE;
}

static void
gtk_real_button_clicked (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);
}

static gboolean
button_activate_timeout (gpointer data)
{
  gtk_button_finish_activate (data, TRUE);

  return FALSE;
}

static void
gtk_real_button_activate (GtkButton *button)
{
  GtkWidget *widget = GTK_WIDGET (button);
  GtkButtonPrivate *priv = button->priv;
  GdkDevice *device;

  device = gtk_get_current_event_device ();

  if (device && gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  if (gtk_widget_get_realized (widget) && !priv->activate_timeout)
    {
      /* bgo#626336 - Only grab if we have a device (from an event), not if we
       * were activated programmatically when no event is available.
       */
      if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
	{
          if (gdk_seat_grab (gdk_device_get_seat (device), priv->event_window,
                             GDK_SEAT_CAPABILITY_KEYBOARD, TRUE,
                             NULL, NULL, NULL, NULL) == GDK_GRAB_SUCCESS)
            {
              gtk_device_grab_add (widget, device, TRUE);
              priv->grab_keyboard = device;
	    }
	}

      priv->activate_timeout = gdk_threads_add_timeout (ACTIVATE_TIMEOUT,
						button_activate_timeout,
						button);
      g_source_set_name_by_id (priv->activate_timeout, "[gtk+] button_activate_timeout");
      priv->button_down = TRUE;
      gtk_button_update_state (button);
    }
}

static void
gtk_button_finish_activate (GtkButton *button,
			    gboolean   do_it)
{
  GtkWidget *widget = GTK_WIDGET (button);
  GtkButtonPrivate *priv = button->priv;

  g_source_remove (priv->activate_timeout);
  priv->activate_timeout = 0;

  if (priv->grab_keyboard)
    {
      gdk_seat_ungrab (gdk_device_get_seat (priv->grab_keyboard));
      gtk_device_grab_remove (widget, priv->grab_keyboard);
      priv->grab_keyboard = NULL;
    }

  priv->button_down = FALSE;

  gtk_button_update_state (button);

  if (do_it)
    gtk_button_clicked (button);
}


static void
gtk_button_measure (GtkCssGadget   *gadget,
		    GtkOrientation  orientation,
                    int             for_size,
		    int            *minimum,
		    int            *natural,
		    int            *minimum_baseline,
		    int            *natural_baseline,
                    gpointer        data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_css_gadget_get_owner (gadget);
  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
       gtk_widget_measure (child,
                           orientation,
                           for_size,
                           minimum, natural,
                           minimum_baseline, natural_baseline);
    }
  else
    {
      *minimum = 0;
      *natural = 0;
      if (minimum_baseline)
        *minimum_baseline = 0;
      if (natural_baseline)
        *natural_baseline = 0;
    }
}

static void
gtk_button_measure_ (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  gtk_css_gadget_get_preferred_size (GTK_BUTTON (widget)->priv->gadget,
                                     orientation,
                                     for_size,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

/**
 * gtk_button_set_label:
 * @button: a #GtkButton
 * @label: a string
 *
 * Sets the text of the label of the button to @label.
 *
 * This will also clear any previously set labels.
 */
void
gtk_button_set_label (GtkButton   *button,
                      const gchar *label)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);
  GtkWidget *child;
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_BUTTON (button));

  context = gtk_widget_get_style_context (GTK_WIDGET (button));

  child = gtk_bin_get_child (GTK_BIN (button));

  if (priv->child_type != LABEL_CHILD || child == NULL)
    {
      if (child != NULL)
        gtk_container_remove (GTK_CONTAINER (button), child);

      child = gtk_label_new (NULL);
      if (priv->use_underline)
        {
          gtk_label_set_use_underline (GTK_LABEL (child), priv->use_underline);
          gtk_label_set_mnemonic_widget (GTK_LABEL (child), GTK_WIDGET (button));
        }
      gtk_container_add (GTK_CONTAINER (button), child);
      gtk_style_context_remove_class (context, "image-button");
      gtk_style_context_add_class (context, "text-button");
    }

  gtk_label_set_label (GTK_LABEL (child), label);
  gtk_button_set_child_type (button, LABEL_CHILD);
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_LABEL]);
}

/**
 * gtk_button_get_label:
 * @button: a #GtkButton
 *
 * Fetches the text from the label of the button, as set by
 * gtk_button_set_label(). If the label text has not
 * been set the return value will be %NULL. This will be the
 * case if you create an empty button with gtk_button_new() to
 * use as a container.
 *
 * Returns: (nullable): The text of the label widget. This string is owned
 * by the widget and must not be modified or freed.
 */
const gchar *
gtk_button_get_label (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  if (priv->child_type == LABEL_CHILD)
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (button));
      return gtk_label_get_label (GTK_LABEL (child));
    }

  return NULL;
}

/**
 * gtk_button_set_use_underline:
 * @button: a #GtkButton
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text of the button label indicates
 * the next character should be used for the mnemonic accelerator key.
 */
void
gtk_button_set_use_underline (GtkButton *button,
			      gboolean   use_underline)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  use_underline = use_underline != FALSE;

  if (use_underline != priv->use_underline)
    {
      if (priv->child_type == LABEL_CHILD)
        {
          GtkWidget *child;
          child = gtk_bin_get_child (GTK_BIN (button));

          gtk_label_set_use_underline (GTK_LABEL (child), use_underline);
          gtk_label_set_mnemonic_widget (GTK_LABEL (child), GTK_WIDGET (button));
        }

      priv->use_underline = use_underline;
      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_USE_UNDERLINE]);
    }
}

/**
 * gtk_button_get_use_underline:
 * @button: a #GtkButton
 *
 * Returns whether an embedded underline in the button label indicates a
 * mnemonic. See gtk_button_set_use_underline().
 *
 * Returns: %TRUE if an embedded underline in the button label
 *               indicates the mnemonic accelerator keys.
 */
gboolean
gtk_button_get_use_underline (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return button->priv->use_underline;
}

static void
gtk_button_update_state (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;
  GtkStateFlags new_state;
  gboolean depressed;

  if (priv->activate_timeout)
    depressed = TRUE;
  else
    depressed = priv->in_button && priv->button_down;

  new_state = gtk_widget_get_state_flags (GTK_WIDGET (button)) &
    ~(GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);

  if (priv->in_button)
    new_state |= GTK_STATE_FLAG_PRELIGHT;

  if (depressed)
    new_state |= GTK_STATE_FLAG_ACTIVE;

  gtk_widget_set_state_flags (GTK_WIDGET (button), new_state, TRUE);
}

static void
gtk_button_screen_changed (GtkWidget *widget,
			   GdkScreen *previous_screen)
{
  GtkButton *button;
  GtkButtonPrivate *priv;

  if (!gtk_widget_has_screen (widget))
    return;

  button = GTK_BUTTON (widget);
  priv = button->priv;

  /* If the button is being pressed while the screen changes the
    release might never occur, so we reset the state. */
  if (priv->button_down)
    {
      priv->button_down = FALSE;
      gtk_button_update_state (button);
    }
}

static void
gtk_button_state_flags_changed (GtkWidget     *widget,
                                GtkStateFlags  previous_state)
{
  GtkButton *button = GTK_BUTTON (widget);

  if (!gtk_widget_is_sensitive (widget))
    gtk_button_do_release (button, FALSE);
}

static void
gtk_button_grab_notify (GtkWidget *widget,
			gboolean   was_grabbed)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout &&
      priv->grab_keyboard &&
      gtk_widget_device_is_shadowed (widget, priv->grab_keyboard))
    gtk_button_finish_activate (button, FALSE);

  if (!was_grabbed)
    gtk_button_do_release (button, FALSE);
}

/**
 * gtk_button_set_icon_name:
 * @button: A #GtkButton
 * @icon_name: A icon name
 *
 * Adds a #GtkImage with the given icon name as a child. The icon will be
 * of size %GTK_ICON_SIZE_BUTTON. If @button already contains a child widget,
 * that child widget will be removed and replaced with the image.
 *
 * Since: 3.90
 */
void
gtk_button_set_icon_name (GtkButton  *button,
                          const char *icon_name)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);
  GtkWidget *child;
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (icon_name != NULL);

  child = gtk_bin_get_child (GTK_BIN (button));
  context = gtk_widget_get_style_context (GTK_WIDGET (button));

  if (priv->child_type != ICON_CHILD || child == NULL)
    {
      if (child != NULL)
        gtk_container_remove (GTK_CONTAINER (button), child);

      child = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
      gtk_container_add (GTK_CONTAINER (button), child);
      gtk_style_context_remove_class (context, "text-button");
      gtk_style_context_add_class (context, "image-button");
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (child), icon_name, GTK_ICON_SIZE_BUTTON);
    }

  gtk_button_set_child_type (button, ICON_CHILD);
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_ICON_NAME]);
}

/**
 * gtk_button_get_icon_name:
 * @button: A #GtkButton
 *
 * Returns the icon name set via gtk_button_set_icon_name().
 *
 * Returns: (nullable): The icon name set via gtk_button_set_icon_name()
 *
 * Since: 3.90
 */
const char *
gtk_button_get_icon_name (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  if (priv->child_type == ICON_CHILD)
    {
      const char *icon_name;
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (button));
      gtk_image_get_icon_name (GTK_IMAGE (child), &icon_name, NULL);

      return icon_name;
    }

  return NULL;
}
