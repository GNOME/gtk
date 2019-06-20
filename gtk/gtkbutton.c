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
 * #GtkLockButton, #GtkColorButton or #GtkFontButton use style classes such as
 * .toggle, .popup, .scale, .lock, .color on the button node
 * to differentiate themselves from a plain GtkButton.
 */

#include "config.h"

#include "gtkbuttonprivate.h"

#include "gtkactionhelperprivate.h"
#include "gtkcheckbutton.h"
#include "gtkcontainerprivate.h"
#include "gtkgestureclick.h"
#include "gtkeventcontrollerkey.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"

#include "a11y/gtkbuttonaccessible.h"

#include <string.h>

/* Time out before giving up on getting a key release when animating
 * the close button.
 */
#define ACTIVATE_TIMEOUT 250

struct _GtkButtonPrivate
{
  GtkActionHelper       *action_helper;

  GtkGesture            *gesture;

  guint                  activate_timeout;

  guint          button_down           : 1;
  guint          in_button             : 1;
  guint          use_underline         : 1;
  guint          child_type            : 2;
};

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


static void gtk_button_dispose        (GObject            *object);
static void gtk_button_set_property   (GObject            *object,
                                       guint               prop_id,
                                       const GValue       *value,
                                       GParamSpec         *pspec);
static void gtk_button_get_property   (GObject            *object,
                                       guint               prop_id,
                                       GValue             *value,
                                       GParamSpec         *pspec);
static void gtk_button_unrealize (GtkWidget * widget);
static void gtk_real_button_clicked (GtkButton * button);
static void gtk_real_button_activate  (GtkButton          *button);
static void gtk_button_finish_activate (GtkButton         *button,
					gboolean           do_it);

static void gtk_button_state_flags_changed (GtkWidget     *widget,
                                            GtkStateFlags  previous_state);
static void gtk_button_grab_notify     (GtkWidget             *widget,
					gboolean               was_grabbed);
static void gtk_button_do_release      (GtkButton             *button,
                                        gboolean               emit_clicked);

static void gtk_button_actionable_iface_init     (GtkActionableInterface *iface);

static void gtk_button_set_child_type (GtkButton *button, guint child_type);

static GParamSpec *props[LAST_PROP] = { NULL, };
static guint button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkButton, gtk_button, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_button_actionable_iface_init))

static void
gtk_button_add (GtkContainer *container, GtkWidget *child)
{
  GtkButton *button = GTK_BUTTON (container);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->child_type != WIDGET_CHILD)
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
gtk_button_unmap (GtkWidget *widget)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (GTK_BUTTON (widget));

  priv->in_button = FALSE;

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unmap (widget);
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
  gobject_class->set_property = gtk_button_set_property;
  gobject_class->get_property = gtk_button_get_property;

  widget_class->unrealize = gtk_button_unrealize;
  widget_class->state_flags_changed = gtk_button_state_flags_changed;
  widget_class->grab_notify = gtk_button_grab_notify;
  widget_class->unmap = gtk_button_unmap;

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
  gtk_widget_class_set_css_name (widget_class, I_("button"));
}

static void
click_pressed_cb (GtkGestureClick *gesture,
                  guint            n_press,
                  gdouble          x,
                  gdouble          y,
                  GtkWidget       *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  priv->in_button = TRUE;

  if (!priv->activate_timeout)
    priv->button_down = TRUE;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static gboolean
touch_release_in_button (GtkButton *button)
{
  GdkEvent *event;
  gdouble x, y;

  event = gtk_get_current_event ();

  if (!event)
    return FALSE;

  if (gdk_event_get_event_type (event) != GDK_TOUCH_END)
    {
      g_object_unref (event);
      return FALSE;
    }

  gdk_event_get_coords (event, &x, &y);

  g_object_unref (event);

  if (gtk_widget_contains (GTK_WIDGET (button), x, y))
    return TRUE;

  return FALSE;
}

static void
click_released_cb (GtkGestureClick *gesture,
                   guint            n_press,
                   gdouble          x,
                   gdouble          y,
                   GtkWidget       *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);
  GdkEventSequence *sequence;

  gtk_button_do_release (button,
                         gtk_widget_is_sensitive (GTK_WIDGET (button)) &&
                         (priv->in_button ||
                          touch_release_in_button (button)));

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (sequence)
    priv->in_button = FALSE;
}

static void
click_gesture_cancel_cb (GtkGesture       *gesture,
                         GdkEventSequence *sequence,
                         GtkButton        *button)
{
  gtk_button_do_release (button, FALSE);
}

static gboolean
key_controller_key_pressed_cb (GtkEventControllerKey *controller,
                               guint                  keyval,
                               guint                  keycode,
                               guint                  modifiers,
                               GtkButton             *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return priv->activate_timeout != 0;
}

static void
key_controller_key_released_cb (GtkEventControllerKey *controller,
                                guint                  keyval,
                                guint                  keycode,
                                guint                  modifiers,
                                GtkButton             *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, TRUE);
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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);
  GtkEventController *key_controller;

  gtk_widget_set_can_focus (GTK_WIDGET (button), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (button), TRUE);

  priv->in_button = FALSE;
  priv->button_down = FALSE;
  priv->use_underline = FALSE;
  priv->child_type = WIDGET_CHILD;

  priv->gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (priv->gesture, "pressed", G_CALLBACK (click_pressed_cb), button);
  g_signal_connect (priv->gesture, "released", G_CALLBACK (click_released_cb), button);
  g_signal_connect (priv->gesture, "cancel", G_CALLBACK (click_gesture_cancel_cb), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->gesture), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (button), GTK_EVENT_CONTROLLER (priv->gesture));

  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed", G_CALLBACK (key_controller_key_pressed_cb), button);
  g_signal_connect (key_controller, "key-released", G_CALLBACK (key_controller_key_released_cb), button);
  gtk_widget_add_controller (GTK_WIDGET (button), key_controller);
}

static void
gtk_button_dispose (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_clear_object (&priv->action_helper);

  G_OBJECT_CLASS (gtk_button_parent_class)->dispose (object);
}

static void
gtk_button_set_action_name (GtkActionable *actionable,
                            const gchar   *action_name)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  g_signal_handlers_disconnect_by_func (button, gtk_real_button_clicked, NULL);
  if (action_name)
    g_signal_connect_after (button, "clicked", G_CALLBACK (gtk_real_button_clicked), NULL);

  gtk_action_helper_set_action_name (priv->action_helper, action_name);
}

static void
gtk_button_set_action_target_value (GtkActionable *actionable,
                                    GVariant      *action_target)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (priv->action_helper, action_target);
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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return gtk_action_helper_get_action_name (priv->action_helper);
}

static GVariant *
gtk_button_get_action_target_value (GtkActionable *actionable)
{
  GtkButton *button = GTK_BUTTON (actionable);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return gtk_action_helper_get_action_target_value (priv->action_helper);
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
 *
 * Creates a new button containing an icon from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead. If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * Returns: a new #GtkButton displaying the themed icon
 */
GtkWidget*
gtk_button_new_from_icon_name (const gchar *icon_name)
{
  GtkWidget *button;

  button = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name", icon_name,
                         NULL);

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
gtk_button_unrealize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, FALSE);

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unrealize (widget);
}

static void
gtk_button_do_release (GtkButton *button,
                       gboolean   emit_clicked)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (priv->button_down)
    {
      priv->button_down = FALSE;

      if (priv->activate_timeout)
	return;

      if (emit_clicked)
        g_signal_emit (button, button_signals[CLICKED], 0);
    }
}

static void
gtk_real_button_clicked (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  if (gtk_widget_get_realized (widget) && !priv->activate_timeout)
    {
      priv->activate_timeout = g_timeout_add (ACTIVATE_TIMEOUT, button_activate_timeout, button);
      g_source_set_name_by_id (priv->activate_timeout, "[gtk] button_activate_timeout");
      priv->button_down = TRUE;
    }
}

static void
gtk_button_finish_activate (GtkButton *button,
			    gboolean   do_it)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_source_remove (priv->activate_timeout);
  priv->activate_timeout = 0;

  priv->button_down = FALSE;

  if (do_it)
    g_signal_emit (button, button_signals[CLICKED], 0);
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
      if (GTK_IS_CHECK_BUTTON (button))
        {
          gtk_label_set_xalign (GTK_LABEL (child), 0.0);
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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_BUTTON (button));

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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return priv->use_underline;
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
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  GTK_WIDGET_CLASS (gtk_button_parent_class)->grab_notify (widget, was_grabbed);

  if (was_grabbed && priv->activate_timeout)
    gtk_button_finish_activate (button, FALSE);

  if (!was_grabbed)
    gtk_button_do_release (button, FALSE);
}

/**
 * gtk_button_set_icon_name:
 * @button: A #GtkButton
 * @icon_name: A icon name
 *
 * Adds a #GtkImage with the given icon name as a child. If @button already
 * contains a child widget, that child widget will be removed and replaced
 * with the image.
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

      child = gtk_image_new_from_icon_name (icon_name);
      gtk_container_add (GTK_CONTAINER (button), child);
      gtk_style_context_remove_class (context, "text-button");
      gtk_style_context_add_class (context, "image-button");
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (child), icon_name);
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
 */
const char *
gtk_button_get_icon_name (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  if (priv->child_type == ICON_CHILD)
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (button));

      return gtk_image_get_icon_name (GTK_IMAGE (child));
    }

  return NULL;
}

GtkGesture *
gtk_button_get_gesture (GtkButton *button)
{
  GtkButtonPrivate *priv = gtk_button_get_instance_private (button);

  return priv->gesture;
}
