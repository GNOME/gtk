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
 */

#include "config.h"

#include "gtkbutton.h"
#include "gtkbuttonprivate.h"

#include <string.h>
#include "deprecated/gtkalignment.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkimage.h"
#include "gtkbox.h"
#include "deprecated/gtkstock.h"
#include "deprecated/gtkactivatable.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkbuttonaccessible.h"
#include "gtkapplicationprivate.h"
#include "gtkactionhelper.h"

static const GtkBorder default_default_border = { 0, 0, 0, 0 };
static const GtkBorder default_default_outside_border = { 0, 0, 0, 0 };

/* Time out before giving up on getting a key release when animating
 * the close button.
 */
#define ACTIVATE_TIMEOUT 250


enum {
  PRESSED,
  RELEASED,
  CLICKED,
  ENTER,
  LEAVE,
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_IMAGE,
  PROP_RELIEF,
  PROP_USE_UNDERLINE,
  PROP_USE_STOCK,
  PROP_FOCUS_ON_CLICK,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_IMAGE_POSITION,
  PROP_ALWAYS_SHOW_IMAGE,

  /* actionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE,
  LAST_PROP = PROP_ACTION_NAME
};


static void gtk_button_destroy        (GtkWidget          *widget);
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
static void gtk_button_style_updated (GtkWidget * widget);
static void gtk_button_size_allocate (GtkWidget * widget,
				      GtkAllocation * allocation);
static gint gtk_button_draw (GtkWidget * widget, cairo_t *cr);
static gint gtk_button_grab_broken (GtkWidget * widget,
				    GdkEventGrabBroken * event);
static gint gtk_button_key_release (GtkWidget * widget, GdkEventKey * event);
static gint gtk_button_enter_notify (GtkWidget * widget,
				     GdkEventCrossing * event);
static gint gtk_button_leave_notify (GtkWidget * widget,
				     GdkEventCrossing * event);
static void gtk_real_button_pressed (GtkButton * button);
static void gtk_real_button_released (GtkButton * button);
static void gtk_real_button_clicked (GtkButton * button);
static void gtk_real_button_activate  (GtkButton          *button);
static void gtk_button_update_state   (GtkButton          *button);
static void gtk_button_enter_leave    (GtkButton          *button);
static void gtk_button_add            (GtkContainer       *container,
			               GtkWidget          *widget);
static GType gtk_button_child_type    (GtkContainer       *container);
static void gtk_button_finish_activate (GtkButton         *button,
					gboolean           do_it);

static void gtk_button_constructed (GObject *object);
static void gtk_button_construct_child (GtkButton             *button);
static void gtk_button_state_changed   (GtkWidget             *widget,
					GtkStateType           previous_state);
static void gtk_button_grab_notify     (GtkWidget             *widget,
					gboolean               was_grabbed);

static void gtk_button_actionable_iface_init     (GtkActionableInterface *iface);
static void gtk_button_activatable_interface_init(GtkActivatableIface  *iface);
static void gtk_button_update                    (GtkActivatable       *activatable,
				                  GtkAction            *action,
			                          const gchar          *property_name);
static void gtk_button_sync_action_properties    (GtkActivatable       *activatable,
                                                  GtkAction            *action);
static void gtk_button_set_related_action        (GtkButton            *button,
					          GtkAction            *action);
static void gtk_button_set_use_action_appearance (GtkButton            *button,
						  gboolean              use_appearance);

static void gtk_button_get_preferred_width             (GtkWidget           *widget,
                                                        gint                *minimum_size,
                                                        gint                *natural_size);
static void gtk_button_get_preferred_height            (GtkWidget           *widget,
                                                        gint                *minimum_size,
                                                        gint                *natural_size);
static void gtk_button_get_preferred_width_for_height  (GtkWidget           *widget,
                                                        gint                 for_size,
                                                        gint                *minimum_size,
                                                        gint                *natural_size);
static void gtk_button_get_preferred_height_for_width  (GtkWidget           *widget,
                                                        gint                 for_size,
                                                        gint                *minimum_size,
                                                        gint                *natural_size);
static void gtk_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
								    gint       width,
								    gint      *minimum_size,
								    gint      *natural_size,
								    gint      *minimum_baseline,
								    gint      *natural_baseline);
  
static GParamSpec *props[LAST_PROP] = { NULL, };
static guint button_signals[LAST_SIGNAL] = { 0 };

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE (GtkButton, gtk_button, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_button_actionable_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_button_activatable_interface_init))
G_GNUC_END_IGNORE_DEPRECATIONS;

static void
gtk_button_class_init (GtkButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  
  gobject_class->constructed  = gtk_button_constructed;
  gobject_class->dispose      = gtk_button_dispose;
  gobject_class->set_property = gtk_button_set_property;
  gobject_class->get_property = gtk_button_get_property;

  widget_class->get_preferred_width = gtk_button_get_preferred_width;
  widget_class->get_preferred_height = gtk_button_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_button_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_button_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_button_get_preferred_height_and_baseline_for_width;
  widget_class->destroy = gtk_button_destroy;
  widget_class->screen_changed = gtk_button_screen_changed;
  widget_class->realize = gtk_button_realize;
  widget_class->unrealize = gtk_button_unrealize;
  widget_class->map = gtk_button_map;
  widget_class->unmap = gtk_button_unmap;
  widget_class->style_updated = gtk_button_style_updated;
  widget_class->size_allocate = gtk_button_size_allocate;
  widget_class->draw = gtk_button_draw;
  widget_class->grab_broken_event = gtk_button_grab_broken;
  widget_class->key_release_event = gtk_button_key_release;
  widget_class->enter_notify_event = gtk_button_enter_notify;
  widget_class->leave_notify_event = gtk_button_leave_notify;
  widget_class->state_changed = gtk_button_state_changed;
  widget_class->grab_notify = gtk_button_grab_notify;

  container_class->child_type = gtk_button_child_type;
  container_class->add = gtk_button_add;
  gtk_container_class_handle_border_width (container_class);

  klass->pressed = gtk_real_button_pressed;
  klass->released = gtk_real_button_released;
  klass->clicked = NULL;
  klass->enter = gtk_button_enter_leave;
  klass->leave = gtk_button_enter_leave;
  klass->activate = gtk_real_button_activate;

  props[PROP_LABEL] =
    g_param_spec_string ("label",
                         P_("Label"),
                         P_("Text of the label widget inside the button, if the button contains a label widget"),
                         NULL,
                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);
  
  props[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline",
                          P_("Use underline"),
                          P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);
  
  /**
   * GtkButton:use-stock:
   *
   * Deprecated: 3.10
   */
  props[PROP_USE_STOCK] =
    g_param_spec_boolean ("use-stock",
                          P_("Use stock"),
                          P_("If set, the label is used to pick a stock item instead of being displayed"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED);
  
  props[PROP_FOCUS_ON_CLICK] =
    g_param_spec_boolean ("focus-on-click",
                          P_("Focus on click"),
                          P_("Whether the button grabs focus when it is clicked with the mouse"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  
  props[PROP_RELIEF] =
    g_param_spec_enum ("relief",
                       P_("Border relief"),
                       P_("The border relief style"),
                       GTK_TYPE_RELIEF_STYLE,
                       GTK_RELIEF_NORMAL,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  
  /**
   * GtkButton:xalign:
   *
   * If the child of the button is a #GtkMisc or #GtkAlignment, this property 
   * can be used to control its horizontal alignment. 0.0 is left aligned, 
   * 1.0 is right aligned.
   *
   * Since: 2.4
   *
   * Deprecated: 3.14: Access the child widget directly if you need to control
   * its alignment.
   */
  props[PROP_XALIGN] =
    g_param_spec_float ("xalign",
                        P_("Horizontal alignment for child"),
                        P_("Horizontal position of child in available space. 0.0 is left aligned, 1.0 is right aligned"),
                        0.0, 1.0, 0.5,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED);

  /**
   * GtkButton:yalign:
   *
   * If the child of the button is a #GtkMisc or #GtkAlignment, this property 
   * can be used to control its vertical alignment. 0.0 is top aligned, 
   * 1.0 is bottom aligned.
   *
   * Since: 2.4
   *
   * Deprecated: 3.14: Access the child widget directly if you need to control
   * its alignment.
   */
  props[PROP_YALIGN] =
    g_param_spec_float ("yalign",
                        P_("Vertical alignment for child"),
                        P_("Vertical position of child in available space. 0.0 is top aligned, 1.0 is bottom aligned"),
                        0.0, 1.0, 0.5,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY|G_PARAM_DEPRECATED);

  /**
   * GtkButton:image:
   *
   * The child widget to appear next to the button text.
   *
   * Since: 2.6
   */
  props[PROP_IMAGE] =
    g_param_spec_object ("image",
                         P_("Image widget"),
                         P_("Child widget to appear next to the button text"),
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:image-position:
   *
   * The position of the image relative to the text inside the button.
   *
   * Since: 2.10
   */
  props[PROP_IMAGE_POSITION] =
    g_param_spec_enum ("image-position",
                       P_("Image position"),
                       P_("The position of the image relative to the text"),
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_LEFT,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkButton:always-show-image:
   *
   * If %TRUE, the button will ignore the #GtkSettings:gtk-button-images
   * setting and always show the image, if available.
   *
   * Use this property if the button would be useless or hard to use
   * without the image.
   *
   * Since: 3.6
   */
  props[PROP_ALWAYS_SHOW_IMAGE] =
     g_param_spec_boolean ("always-show-image",
                           P_("Always show image"),
                           P_("Whether the image will always be shown"),
                           FALSE,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");
  G_GNUC_END_IGNORE_DEPRECATIONS;

  /**
   * GtkButton::pressed:
   * @button: the object that received the signal
   *
   * Emitted when the button is pressed.
   *
   * Deprecated: 2.8: Use the #GtkWidget::button-press-event signal.
   */ 
  button_signals[PRESSED] =
    g_signal_new (I_("pressed"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, pressed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::released:
   * @button: the object that received the signal
   *
   * Emitted when the button is released.
   *
   * Deprecated: 2.8: Use the #GtkWidget::button-release-event signal.
   */ 
  button_signals[RELEASED] =
    g_signal_new (I_("released"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, released),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

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
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::enter:
   * @button: the object that received the signal
   *
   * Emitted when the pointer enters the button.
   *
   * Deprecated: 2.8: Use the #GtkWidget::enter-notify-event signal.
   */ 
  button_signals[ENTER] =
    g_signal_new (I_("enter"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, enter),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::leave:
   * @button: the object that received the signal
   *
   * Emitted when the pointer leaves the button.
   *
   * Deprecated: 2.8: Use the #GtkWidget::leave-notify-event signal.
   */ 
  button_signals[LEAVE] =
    g_signal_new (I_("leave"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, leave),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
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
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = button_signals[ACTIVATE];

  /**
   * GtkButton:default-border:
   *
   * The "default-border" style property defines the extra space to add
   * around a button that can become the default widget of its window.
   * For more information about default widgets, see gtk_widget_grab_default().
   *
   * Deprecated: 3.14: use CSS margins and padding instead.
   */

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("default-border",
							       P_("Default Spacing"),
							       P_("Extra space to add for GTK_CAN_DEFAULT buttons"),
							       GTK_TYPE_BORDER,
							       GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkButton:default-outside-border:
   *
   * The "default-outside-border" style property defines the extra outside
   * space to add around a button that can become the default widget of its
   * window. Extra outside space is always drawn outside the button border.
   * For more information about default widgets, see gtk_widget_grab_default().
   *
   * Deprecated: 3.14: use CSS margins and padding instead.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("default-outside-border",
							       P_("Default Outside Spacing"),
							       P_("Extra space to add for GTK_CAN_DEFAULT buttons that is always drawn outside the border"),
							       GTK_TYPE_BORDER,
							       GTK_PARAM_READABLE|G_PARAM_DEPRECATED));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-displacement-x",
							     P_("Child X Displacement"),
							     P_("How far in the x direction to move the child when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE|G_PARAM_DEPRECATED));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-displacement-y",
							     P_("Child Y Displacement"),
							     P_("How far in the y direction to move the child when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkButton:displace-focus:
   *
   * Whether the child_displacement_x/child_displacement_y properties 
   * should also affect the focus rectangle.
   *
   * Since: 2.6
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("displace-focus",
								 P_("Displace focus"),
								 P_("Whether the child_displacement_x/_y properties should also affect the focus rectangle"),
								 FALSE,
								 GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkButton:inner-border:
   *
   * Sets the border between the button edges and child.
   *
   * Since: 2.10
   *
   * Deprecated: 3.4: Use the standard border and padding CSS properties;
   *   the value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("inner-border",
                                                               P_("Inner Border"),
                                                               P_("Border between button edges and child."),
                                                               GTK_TYPE_BORDER,
                                                               GTK_PARAM_READABLE | G_PARAM_DEPRECATED));

  /**
   * GtkButton::image-spacing:
   *
   * Spacing in pixels between the image and label.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("image-spacing",
							     P_("Image spacing"),
							     P_("Spacing in pixels between the image and label"),
							     0,
							     G_MAXINT,
							     2,
							     GTK_PARAM_READABLE));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_BUTTON_ACCESSIBLE);
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

  if (priv->focus_on_click && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  priv->in_button = TRUE;
  g_signal_emit (button, button_signals[PRESSED], 0);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
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
  const GdkEvent *event;
  GdkDevice *source;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  if (event)
    {
      source = gdk_event_get_source_device (event);
      if (source && gdk_device_get_source (source) == GDK_SOURCE_TOUCHSCREEN)
        priv->in_button = FALSE;
    }
  g_signal_emit (button, button_signals[RELEASED], 0);
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
gtk_button_init (GtkButton *button)
{
  GtkButtonPrivate *priv;
  GtkStyleContext *context;

  button->priv = gtk_button_get_instance_private (button);
  priv = button->priv;

  gtk_widget_set_can_focus (GTK_WIDGET (button), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (button), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);

  priv->label_text = NULL;

  priv->constructed = FALSE;
  priv->in_button = FALSE;
  priv->button_down = FALSE;
  priv->use_stock = FALSE;
  priv->use_underline = FALSE;
  priv->focus_on_click = TRUE;

  priv->xalign = 0.5;
  priv->yalign = 0.5;
  priv->align_set = 0;
  priv->image_is_stock = TRUE;
  priv->image_position = GTK_POS_LEFT;
  priv->use_action_appearance = TRUE;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

  priv->gesture = gtk_gesture_multi_press_new (GTK_WIDGET (button));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (priv->gesture, "pressed", G_CALLBACK (multipress_pressed_cb), button);
  g_signal_connect (priv->gesture, "released", G_CALLBACK (multipress_released_cb), button);
  g_signal_connect (priv->gesture, "update", G_CALLBACK (multipress_gesture_update_cb), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->gesture), GTK_PHASE_BUBBLE);
}

static void
gtk_button_destroy (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->label_text)
    {
      g_free (priv->label_text);
      priv->label_text = NULL;
    }

  g_clear_object (&priv->gesture);

  GTK_WIDGET_CLASS (gtk_button_parent_class)->destroy (widget);
}

static void
gtk_button_constructed (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  G_OBJECT_CLASS (gtk_button_parent_class)->constructed (object);

  priv->constructed = TRUE;

  if (priv->label_text != NULL)
    gtk_button_construct_child (button);
}


static GType
gtk_button_child_type  (GtkContainer     *container)
{
  if (!gtk_bin_get_child (GTK_BIN (container)))
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static void
maybe_set_alignment (GtkButton *button,
		     GtkWidget *widget)
{
  GtkButtonPrivate *priv = button->priv;

  if (!priv->align_set)
    return;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GTK_IS_MISC (widget))
    gtk_misc_set_alignment (GTK_MISC (widget), priv->xalign, priv->yalign);
  else if (GTK_IS_ALIGNMENT (widget))
    g_object_set (widget, "xalign", priv->xalign, "yalign", priv->yalign, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_button_add (GtkContainer *container,
		GtkWidget    *widget)
{
  maybe_set_alignment (GTK_BUTTON (container), widget);

  GTK_CONTAINER_CLASS (gtk_button_parent_class)->add (container, widget);
}

static void 
gtk_button_dispose (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  g_clear_object (&priv->action_helper);

  if (priv->action)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (button), NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      priv->action = NULL;
    }
  G_OBJECT_CLASS (gtk_button_parent_class)->dispose (object);
}

static void
gtk_button_set_action_name (GtkActionable *actionable,
                            const gchar   *action_name)
{
  GtkButton *button = GTK_BUTTON (actionable);

  g_return_if_fail (button->priv->action == NULL);

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
  GtkButtonPrivate *priv = button->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_IMAGE:
      gtk_button_set_image (button, (GtkWidget *) g_value_get_object (value));
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      gtk_button_set_always_show_image (button, g_value_get_boolean (value));
      break;
    case PROP_RELIEF:
      gtk_button_set_relief (button, g_value_get_enum (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_USE_STOCK:
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_button_set_use_stock (button, g_value_get_boolean (value));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      break;
    case PROP_FOCUS_ON_CLICK:
      gtk_button_set_focus_on_click (button, g_value_get_boolean (value));
      break;
    case PROP_XALIGN:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_button_set_alignment (button, g_value_get_float (value), priv->yalign);
G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    case PROP_YALIGN:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_button_set_alignment (button, priv->xalign, g_value_get_float (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    case PROP_IMAGE_POSITION:
      gtk_button_set_image_position (button, g_value_get_enum (value));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      gtk_button_set_related_action (button, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      gtk_button_set_use_action_appearance (button, g_value_get_boolean (value));
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
      g_value_set_string (value, priv->label_text);
      break;
    case PROP_IMAGE:
      g_value_set_object (value, (GObject *)priv->image);
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      g_value_set_boolean (value, gtk_button_get_always_show_image (button));
      break;
    case PROP_RELIEF:
      g_value_set_enum (value, gtk_button_get_relief (button));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_USE_STOCK:
      g_value_set_boolean (value, priv->use_stock);
      break;
    case PROP_FOCUS_ON_CLICK:
      g_value_set_boolean (value, priv->focus_on_click);
      break;
    case PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, priv->yalign);
      break;
    case PROP_IMAGE_POSITION:
      g_value_set_enum (value, priv->image_position);
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, priv->action);
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, priv->use_action_appearance);
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

static void 
gtk_button_activatable_interface_init (GtkActivatableIface  *iface)
{
  iface->update = gtk_button_update;
  iface->sync_action_properties = gtk_button_sync_action_properties;
}

static void
activatable_update_stock_id (GtkButton *button,
			     GtkAction *action)
{
  gboolean use_stock;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  use_stock = gtk_button_get_use_stock (button);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (!use_stock)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_button_set_label (button, gtk_action_get_stock_id (action));
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
activatable_update_short_label (GtkButton *button,
				GtkAction *action)
{
  GtkWidget *child;
  GtkWidget *image;
  gboolean use_stock;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  use_stock = gtk_button_get_use_stock (button);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (use_stock)
    return;

  image = gtk_button_get_image (button);

  /* Dont touch custom child... */
  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_IMAGE (image) ||
      child == NULL ||
      GTK_IS_LABEL (child))
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_button_set_label (button, gtk_action_get_short_label (action));
      G_GNUC_END_IGNORE_DEPRECATIONS;
      gtk_button_set_use_underline (button, TRUE);
    }
}

static void
activatable_update_icon_name (GtkButton *button,
			      GtkAction *action)
{
  GtkWidget *image;
  gboolean use_stock;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  use_stock = gtk_button_get_use_stock (button);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (use_stock)
    return;

  image = gtk_button_get_image (button);

  if (GTK_IS_IMAGE (image) &&
      (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_EMPTY ||
       gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_ICON_NAME))
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_image_set_from_icon_name (GTK_IMAGE (image),
                                    gtk_action_get_icon_name (action), GTK_ICON_SIZE_MENU);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
activatable_update_gicon (GtkButton *button,
			  GtkAction *action)
{
  GtkWidget *image = gtk_button_get_image (button);
  GIcon *icon;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  icon = gtk_action_get_gicon (action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (GTK_IS_IMAGE (image) &&
      (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_EMPTY ||
       gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_GICON))
    gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_BUTTON);
}

static void 
gtk_button_update (GtkActivatable *activatable,
		   GtkAction      *action,
	           const gchar    *property_name)
{
  GtkButton *button = GTK_BUTTON (activatable);
  GtkButtonPrivate *priv = button->priv;

  if (strcmp (property_name, "visible") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      if (gtk_action_is_visible (action))
	gtk_widget_show (GTK_WIDGET (activatable));
      else
	gtk_widget_hide (GTK_WIDGET (activatable));
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else if (strcmp (property_name, "sensitive") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }

  if (!priv->use_action_appearance)
    return;

  if (strcmp (property_name, "stock-id") == 0)
    activatable_update_stock_id (GTK_BUTTON (activatable), action);
  else if (strcmp (property_name, "gicon") == 0)
    activatable_update_gicon (GTK_BUTTON (activatable), action);
  else if (strcmp (property_name, "short-label") == 0)
    activatable_update_short_label (GTK_BUTTON (activatable), action);
  else if (strcmp (property_name, "icon-name") == 0)
    activatable_update_icon_name (GTK_BUTTON (activatable), action);
}

static void
gtk_button_sync_action_properties (GtkActivatable *activatable,
			           GtkAction      *action)
{
  GtkButton *button = GTK_BUTTON (activatable);
  GtkButtonPrivate *priv = button->priv;
  gboolean always_show_image;

  if (!action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (gtk_action_is_visible (action))
    gtk_widget_show (GTK_WIDGET (activatable));
  else
    gtk_widget_hide (GTK_WIDGET (activatable));
  
  gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
  always_show_image = gtk_action_get_always_show_image (action);
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (priv->use_action_appearance)
    {
      activatable_update_stock_id (GTK_BUTTON (activatable), action);
      activatable_update_short_label (GTK_BUTTON (activatable), action);
      activatable_update_gicon (GTK_BUTTON (activatable), action);
      activatable_update_icon_name (GTK_BUTTON (activatable), action);
    }

  gtk_button_set_always_show_image (button, always_show_image);
}

static void
gtk_button_set_related_action (GtkButton *button,
			       GtkAction *action)
{
  GtkButtonPrivate *priv = button->priv;

  g_return_if_fail (gtk_action_helper_get_action_name (button->priv->action_helper) == NULL);

  if (priv->action == action)
    return;

  /* This should be a default handler, but for compatibility reasons
   * we need to support derived classes that don't chain up their
   * clicked handler.
   */
  g_signal_handlers_disconnect_by_func (button, gtk_real_button_clicked, NULL);
  if (action)
    g_signal_connect_after (button, "clicked",
                            G_CALLBACK (gtk_real_button_clicked), NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (button), action);
  G_GNUC_END_IGNORE_DEPRECATIONS

  priv->action = action;
}

static void
gtk_button_set_use_action_appearance (GtkButton *button,
				      gboolean   use_appearance)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->use_action_appearance != use_appearance)
    {
      priv->use_action_appearance = use_appearance;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (button), priv->action);
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS

    }
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

static gboolean
show_image (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;
  gboolean show;

  if (priv->label_text && !priv->always_show_image)
    {
      GtkSettings *settings;

      settings = gtk_widget_get_settings (GTK_WIDGET (button));        
      g_object_get (settings, "gtk-button-images", &show, NULL);
    }
  else
    show = TRUE;

  return show;
}


static void
gtk_button_construct_child (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;
  GtkStyleContext *context;
  GtkStockItem item;
  GtkWidget *child;
  GtkWidget *label;
  GtkWidget *box;
  GtkWidget *align;
  GtkWidget *image = NULL;
  gchar *label_text = NULL;
  gint image_spacing;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_remove_class (context, "image-button");
  gtk_style_context_remove_class (context, "text-button");

  if (!priv->constructed)
    return;

  if (!priv->label_text && !priv->image)
    return;

  gtk_style_context_get_style (context,
                               "image-spacing", &image_spacing,
                               NULL);

  if (priv->image && !priv->image_is_stock)
    {
      GtkWidget *parent;

      image = g_object_ref (priv->image);

      parent = gtk_widget_get_parent (image);
      if (parent)
	gtk_container_remove (GTK_CONTAINER (parent), image);
    }

  priv->image = NULL;

  child = gtk_bin_get_child (GTK_BIN (button));
  if (child)
    gtk_container_remove (GTK_CONTAINER (button), child);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (priv->use_stock &&
      priv->label_text &&
      gtk_stock_lookup (priv->label_text, &item))
    {
      if (!image)
	image = g_object_ref (gtk_image_new_from_stock (priv->label_text, GTK_ICON_SIZE_BUTTON));

      label_text = item.label;
    }
  else
    label_text = priv->label_text;

  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (image)
    {
      priv->image = image;
      g_object_set (priv->image,
		    "visible", show_image (button),
		    "no-show-all", TRUE,
		    NULL);

      if (priv->image_position == GTK_POS_LEFT ||
	  priv->image_position == GTK_POS_RIGHT)
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, image_spacing);
      else
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, image_spacing);

      gtk_widget_set_valign (image, GTK_ALIGN_BASELINE);
      gtk_widget_set_valign (box, GTK_ALIGN_BASELINE);

      if (priv->align_set)
	align = gtk_alignment_new (priv->xalign, priv->yalign, 0.0, 0.0);
      else
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

      gtk_widget_set_valign (align, GTK_ALIGN_BASELINE);

      if (priv->image_position == GTK_POS_LEFT ||
	  priv->image_position == GTK_POS_TOP)
	gtk_box_pack_start (GTK_BOX (box), priv->image, FALSE, FALSE, 0);
      else
	gtk_box_pack_end (GTK_BOX (box), priv->image, FALSE, FALSE, 0);

      if (label_text)
	{
          if (priv->use_underline || priv->use_stock)
            {
	      label = gtk_label_new_with_mnemonic (label_text);
	      gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                             GTK_WIDGET (button));
            }
          else
            label = gtk_label_new (label_text);

	  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

	  if (priv->image_position == GTK_POS_RIGHT ||
	      priv->image_position == GTK_POS_BOTTOM)
	    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	  else
	    gtk_box_pack_end (GTK_BOX (box), label, FALSE, FALSE, 0);
	}
      else
        {
          gtk_style_context_add_class (context, "image-button");
        }

      gtk_container_add (GTK_CONTAINER (button), align);
      gtk_container_add (GTK_CONTAINER (align), box);
      gtk_widget_show_all (align);

      g_object_unref (image);

      return;
    }

  if (priv->use_underline || priv->use_stock)
    {
      label = gtk_label_new_with_mnemonic (priv->label_text);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));
    }
  else
    label = gtk_label_new (priv->label_text);

  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  maybe_set_alignment (button, label);

  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (button), label);

  gtk_style_context_add_class (context, "text-button");
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
 * @icon_name: an icon name
 * @size: (type int): an icon size
 *
 * Creates a new button containing an icon from the current icon theme.
 *
 * If the icon name isn’t known, a “broken image” icon will be
 * displayed instead. If the current icon theme is changed, the icon
 * will be updated appropriately.
 *
 * This function is a convenience wrapper around gtk_button_new() and
 * gtk_button_set_image().
 *
 * Returns: a new #GtkButton displaying the themed icon
 *
 * Since: 3.10
 **/
GtkWidget*
gtk_button_new_from_icon_name (const gchar *icon_name,
			       GtkIconSize  size)
{
  GtkWidget *button;
  GtkWidget *image;

  image = gtk_image_new_from_icon_name (icon_name, size);
  button =  g_object_new (GTK_TYPE_BUTTON,
			  "image", image,
			  NULL);

  return button;
}

/**
 * gtk_button_new_from_stock:
 * @stock_id: the name of the stock item 
 *
 * Creates a new #GtkButton containing the image and text from a stock item.
 * Some stock ids have preprocessor macros like #GTK_STOCK_OK and
 * #GTK_STOCK_APPLY.
 *
 * If @stock_id is unknown, then it will be treated as a mnemonic
 * label (as for gtk_button_new_with_mnemonic()).
 *
 * Returns: a new #GtkButton
 *
 * Deprecated: 3.10: Use gtk_button_new_with_label() instead.
 **/
GtkWidget*
gtk_button_new_from_stock (const gchar *stock_id)
{
  return g_object_new (GTK_TYPE_BUTTON,
                       "label", stock_id,
                       "use-stock", TRUE,
                       "use-underline", TRUE,
                       NULL);
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
 **/
GtkWidget*
gtk_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, "use-underline", TRUE,  NULL);
}

/**
 * gtk_button_pressed:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::pressed signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::button-press-event signal.
 */
void
gtk_button_pressed (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[PRESSED], 0);
}

/**
 * gtk_button_released:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::released signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::button-release-event signal.
 */
void
gtk_button_released (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[RELEASED], 0);
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
 * gtk_button_enter:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::enter signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::enter-notify-event signal.
 */
void
gtk_button_enter (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[ENTER], 0);
}

/**
 * gtk_button_leave:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::leave signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::leave-notify-event signal.
 */
void
gtk_button_leave (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[LEAVE], 0);
}

/**
 * gtk_button_set_relief:
 * @button: The #GtkButton you want to set relief styles of
 * @relief: The GtkReliefStyle as described above
 *
 * Sets the relief style of the edges of the given #GtkButton widget.
 * Two styles exist, %GTK_RELIEF_NORMAL and %GTK_RELIEF_NONE.
 * The default style is, as one can guess, %GTK_RELIEF_NORMAL.
 * The deprecated value %GTK_RELIEF_HALF behaves the same as
 * %GTK_RELIEF_NORMAL.
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
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_TOUCH_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  priv->event_window = gdk_window_new (window,
                                       &attributes, attributes_mask);
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
gtk_button_update_image_spacing (GtkButton       *button,
                                 GtkStyleContext *context)
{
  GtkButtonPrivate *priv = button->priv;
  GtkWidget *child; 
  gint spacing;

  /* Keep in sync with gtk_button_construct_child,
   * we only want to update the spacing if the box 
   * was constructed there.
   */
  if (!priv->constructed || !priv->image)
    return;

  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_ALIGNMENT (child))
    {
      child = gtk_bin_get_child (GTK_BIN (child));
      if (GTK_IS_BOX (child))
        {
          gtk_style_context_get_style (context,
                                       "image-spacing", &spacing,
                                       NULL);

          gtk_box_set_spacing (GTK_BOX (child), spacing);
        }
    }
}

static void
gtk_button_style_updated (GtkWidget *widget)
{
  GtkStyleContext *context;

  GTK_WIDGET_CLASS (gtk_button_parent_class)->style_updated (widget);

  context = gtk_widget_get_style_context (widget);

  gtk_button_update_image_spacing (GTK_BUTTON (widget), context);
}

static void
gtk_button_get_props (GtkButton *button,
		      GtkBorder *default_border,
		      GtkBorder *default_outside_border,
                      GtkBorder *padding,
                      GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder *tmp_border;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  state = gtk_style_context_get_state (context);

  if (default_border)
    {
      gtk_style_context_get_style (context,
                                   "default-border", &tmp_border,
                                   NULL);

      if (tmp_border)
	{
	  *default_border = *tmp_border;
	  gtk_border_free (tmp_border);
	}
      else
	*default_border = default_default_border;
    }

  if (default_outside_border)
    {
      gtk_style_context_get_style (context,
                                   "default-outside-border", &tmp_border,
                                   NULL);

      if (tmp_border)
	{
	  *default_outside_border = *tmp_border;
	  gtk_border_free (tmp_border);
	}
      else
	*default_outside_border = default_default_outside_border;
    }

  if (padding)
    gtk_style_context_get_padding (context, state, padding);

  if (border)
    gtk_style_context_get_border (context, state, border);
}

/* Computes the size of the border around the button's child
 * including all CSS and style properties so it can be used
 * during size allocation and size request phases. */
static void
gtk_button_get_full_border (GtkButton *button,
                            GtkBorder *full_border)
{
  GtkBorder default_border, padding, border;

  gtk_button_get_props (button, &default_border, NULL,
                        &padding, &border);

  full_border->left = padding.left + border.left;
  full_border->right = padding.right + border.right;
  full_border->top = padding.top + border.top;
  full_border->bottom = padding.bottom + border.bottom;

  if (gtk_widget_get_can_default (GTK_WIDGET (button)))
    {
      full_border->left += default_border.left;
      full_border->right += default_border.right;
      full_border->top += default_border.top;
      full_border->bottom += default_border.bottom;
    }
}

static void
gtk_button_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  child = gtk_bin_get_child (GTK_BIN (button));
  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation child_allocation;
      GtkBorder border;
      gint baseline;

      gtk_button_get_full_border (button, &border);

      child_allocation.x = allocation->x + border.left;
      child_allocation.y = allocation->y + border.top;
      child_allocation.width = allocation->width - border.left - border.right;
      child_allocation.height = allocation->height - border.top - border.bottom;

      baseline = gtk_widget_get_allocated_baseline (widget);
      if (baseline != -1)
	baseline -= border.top;

      child_allocation.width  = MAX (1, child_allocation.width);
      child_allocation.height = MAX (1, child_allocation.height);

      gtk_widget_size_allocate_with_baseline (child, &child_allocation, baseline);
    }

  _gtk_widget_set_simple_clip (widget, NULL);
}

static gboolean
gtk_button_draw (GtkWidget *widget,
		 cairo_t   *cr)
{
  GtkButton *button = GTK_BUTTON (widget);
  gint x, y;
  gint width, height;
  GtkBorder default_border;
  GtkBorder default_outside_border;
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_button_get_props (button, &default_border, &default_outside_border, NULL, NULL);

  x = 0;
  y = 0;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  if (gtk_widget_has_default (widget) &&
      gtk_button_get_relief (button) == GTK_RELIEF_NORMAL)
    {
      x += default_border.left;
      y += default_border.top;
      width -= default_border.left + default_border.right;
      height -= default_border.top + default_border.bottom;
    }
  else if (gtk_widget_get_can_default (widget))
    {
      x += default_outside_border.left;
      y += default_outside_border.top;
      width -= default_outside_border.left + default_outside_border.right;
      height -= default_outside_border.top + default_outside_border.bottom;
    }

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkBorder border;

      gtk_style_context_get_border (context, state, &border);

      x += border.left;
      y += border.top;
      width -= border.left + border.right;
      height -= border.top + border.bottom;

      gtk_render_focus (context, cr, x, y, width, height);
    }

  GTK_WIDGET_CLASS (gtk_button_parent_class)->draw (widget, cr);

  return FALSE;
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
      g_signal_emit (button, button_signals[ENTER], 0);
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
      g_signal_emit (button, button_signals[LEAVE], 0);
    }

  return FALSE;
}

static void
gtk_real_button_pressed (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout)
    return;

  priv->button_down = TRUE;
  gtk_button_update_state (button);
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
gtk_real_button_released (GtkButton *button)
{
  gtk_button_do_release (button,
                         gtk_widget_is_sensitive (GTK_WIDGET (button)) &&
                         (button->priv->in_button ||
                          touch_release_in_button (button)));
}

static void 
gtk_real_button_clicked (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);

  if (priv->action)
    gtk_action_activate (priv->action);
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
  guint32 time;

  device = gtk_get_current_event_device ();

  if (device && gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  if (gtk_widget_get_realized (widget) && !priv->activate_timeout)
    {
      time = gtk_get_current_event_time ();

      /* bgo#626336 - Only grab if we have a device (from an event), not if we
       * were activated programmatically when no event is available.
       */
      if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
	{
	  if (gdk_device_grab (device, priv->event_window,
			       GDK_OWNERSHIP_WINDOW, TRUE,
			       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			       NULL, time) == GDK_GRAB_SUCCESS)
	    {
	      gtk_device_grab_add (widget, device, TRUE);
	      priv->grab_keyboard = device;
	      priv->grab_time = time;
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
      gdk_device_ungrab (priv->grab_keyboard, priv->grab_time);
      gtk_device_grab_remove (widget, priv->grab_keyboard);
      priv->grab_keyboard = NULL;
    }

  priv->button_down = FALSE;

  gtk_button_update_state (button);

  if (do_it)
    gtk_button_clicked (button);
}


static void
gtk_button_get_size (GtkWidget      *widget,
		     GtkOrientation  orientation,
                     gint            for_size,
		     gint           *minimum_size,
		     gint           *natural_size,
		     gint           *minimum_baseline,
		     gint           *natural_baseline)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkWidget *child;
  GtkBorder border;
  gint minimum, natural;

  gtk_button_get_full_border (button, &border);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum = border.left + border.right;
      natural = minimum;

      if (for_size >= 0)
        for_size -= border.top + border.bottom;
    }
  else
    {
      minimum = border.top + border.bottom;
      natural = minimum;

      if (for_size >= 0)
        for_size -= border.left + border.right;
    }

  if ((child = gtk_bin_get_child (GTK_BIN (button))) && 
      gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;
      gint child_min_baseline = -1, child_nat_baseline = -1;

      _gtk_widget_get_preferred_size_for_size (child,
                                               orientation,
                                               for_size,
                                               &child_min, &child_nat,
                                               &child_min_baseline, &child_nat_baseline);

      if (minimum_baseline && child_min_baseline >= 0)
	*minimum_baseline = child_min_baseline + border.top;
      if (natural_baseline && child_nat_baseline >= 0)
	*natural_baseline = child_nat_baseline + border.top;

      minimum += child_min;
      natural += child_nat;
    }

  *minimum_size = minimum;
  *natural_size = natural;
}

static void 
gtk_button_get_preferred_width (GtkWidget *widget,
                                gint      *minimum_size,
                                gint      *natural_size)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_HORIZONTAL, -1, minimum_size, natural_size, NULL, NULL);
}

static void 
gtk_button_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum_size,
                                 gint      *natural_size)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_VERTICAL, -1, minimum_size, natural_size, NULL, NULL);
}

static void 
gtk_button_get_preferred_width_for_height (GtkWidget *widget,
                                           gint       for_size,
                                           gint      *minimum_size,
                                           gint      *natural_size)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_HORIZONTAL, for_size, minimum_size, natural_size, NULL, NULL);
}

static void 
gtk_button_get_preferred_height_for_width (GtkWidget *widget,
                                           gint       for_size,
                                           gint      *minimum_size,
                                           gint      *natural_size)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_VERTICAL, for_size, minimum_size, natural_size, NULL, NULL);
}

static void
gtk_button_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
							gint       width,
							gint      *minimum_size,
							gint      *natural_size,
							gint      *minimum_baseline,
							gint      *natural_baseline)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_VERTICAL, width, minimum_size, natural_size, minimum_baseline, natural_baseline);
}

/**
 * gtk_button_set_label:
 * @button: a #GtkButton
 * @label: a string
 *
 * Sets the text of the label of the button to @str. This text is
 * also used to select the stock item if gtk_button_set_use_stock()
 * is used.
 *
 * This will also clear any previously set labels.
 **/
void
gtk_button_set_label (GtkButton   *button,
		      const gchar *label)
{
  GtkButtonPrivate *priv;
  gchar *new_label;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  new_label = g_strdup (label);
  g_free (priv->label_text);
  priv->label_text = new_label;

  gtk_button_construct_child (button);
  
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
 * Returns: The text of the label widget. This string is owned
 * by the widget and must not be modified or freed.
 **/
const gchar *
gtk_button_get_label (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  return button->priv->label_text;
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
      priv->use_underline = use_underline;

      gtk_button_construct_child (button);
      
      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_USE_UNDERLINE]);
    }
}

/**
 * gtk_button_get_use_underline:
 * @button: a #GtkButton
 *
 * Returns whether an embedded underline in the button label indicates a
 * mnemonic. See gtk_button_set_use_underline ().
 *
 * Returns: %TRUE if an embedded underline in the button label
 *               indicates the mnemonic accelerator keys.
 **/
gboolean
gtk_button_get_use_underline (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return button->priv->use_underline;
}

/**
 * gtk_button_set_use_stock:
 * @button: a #GtkButton
 * @use_stock: %TRUE if the button should use a stock item
 *
 * If %TRUE, the label set on the button is used as a
 * stock id to select the stock item for the button.
 *
 * Deprecated: 3.10
 */
void
gtk_button_set_use_stock (GtkButton *button,
			  gboolean   use_stock)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  use_stock = use_stock != FALSE;

  if (use_stock != priv->use_stock)
    {
      priv->use_stock = use_stock;

      gtk_button_construct_child (button);
      
      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_USE_STOCK]);
    }
}

/**
 * gtk_button_get_use_stock:
 * @button: a #GtkButton
 *
 * Returns whether the button label is a stock item.
 *
 * Returns: %TRUE if the button label is used to
 *               select a stock item instead of being
 *               used directly as the label text.
 *
 * Deprecated: 3.10
 */
gboolean
gtk_button_get_use_stock (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return button->priv->use_stock;
}

/**
 * gtk_button_set_focus_on_click:
 * @button: a #GtkButton
 * @focus_on_click: whether the button grabs focus when clicked with the mouse
 *
 * Sets whether the button will grab focus when it is clicked with the mouse.
 * Making mouse clicks not grab focus is useful in places like toolbars where
 * you don’t want the keyboard focus removed from the main area of the
 * application.
 *
 * Since: 2.4
 **/
void
gtk_button_set_focus_on_click (GtkButton *button,
			       gboolean   focus_on_click)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  focus_on_click = focus_on_click != FALSE;

  if (priv->focus_on_click != focus_on_click)
    {
      priv->focus_on_click = focus_on_click;
      
      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_FOCUS_ON_CLICK]);
    }
}

/**
 * gtk_button_get_focus_on_click:
 * @button: a #GtkButton
 *
 * Returns whether the button grabs focus when it is clicked with the mouse.
 * See gtk_button_set_focus_on_click().
 *
 * Returns: %TRUE if the button grabs focus when it is clicked with
 *               the mouse.
 *
 * Since: 2.4
 **/
gboolean
gtk_button_get_focus_on_click (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  
  return button->priv->focus_on_click;
}

/**
 * gtk_button_set_alignment:
 * @button: a #GtkButton
 * @xalign: the horizontal position of the child, 0.0 is left aligned, 
 *   1.0 is right aligned
 * @yalign: the vertical position of the child, 0.0 is top aligned, 
 *   1.0 is bottom aligned
 *
 * Sets the alignment of the child. This property has no effect unless 
 * the child is a #GtkMisc or a #GtkAlignment.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Access the child widget directly if you need to control
 * its alignment.
 */
void
gtk_button_set_alignment (GtkButton *button,
			  gfloat     xalign,
			  gfloat     yalign)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));
  
  priv = button->priv;

  priv->xalign = xalign;
  priv->yalign = yalign;
  priv->align_set = 1;

  maybe_set_alignment (button, gtk_bin_get_child (GTK_BIN (button)));

  g_object_freeze_notify (G_OBJECT (button));
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_XALIGN]);
  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_YALIGN]);
  g_object_thaw_notify (G_OBJECT (button));
}

/**
 * gtk_button_get_alignment:
 * @button: a #GtkButton
 * @xalign: (out): return location for horizontal alignment
 * @yalign: (out): return location for vertical alignment
 *
 * Gets the alignment of the child in the button.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Access the child widget directly if you need to control
 * its alignment.
 */
void
gtk_button_get_alignment (GtkButton *button,
			  gfloat    *xalign,
			  gfloat    *yalign)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));
  
  priv = button->priv;
 
  if (xalign) 
    *xalign = priv->xalign;

  if (yalign)
    *yalign = priv->yalign;
}

static void
gtk_button_enter_leave (GtkButton *button)
{
  gtk_button_update_state (button);
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
show_image_change_notify (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->image) 
    {
      if (show_image (button))
	gtk_widget_show (priv->image);
      else
	gtk_widget_hide (priv->image);
    }
}

static void
traverse_container (GtkWidget *widget,
		    gpointer   data)
{
  if (GTK_IS_BUTTON (widget))
    show_image_change_notify (GTK_BUTTON (widget));
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), traverse_container, NULL);
}

static void
gtk_button_setting_changed (GtkSettings *settings)
{
  GList *list, *l;

  list = gtk_window_list_toplevels ();

  for (l = list; l; l = l->next)
    gtk_container_forall (GTK_CONTAINER (l->data), 
			  traverse_container, NULL);

  g_list_free (list);
}

static void
gtk_button_screen_changed (GtkWidget *widget,
			   GdkScreen *previous_screen)
{
  GtkButton *button;
  GtkButtonPrivate *priv;
  GtkSettings *settings;
  gulong show_image_connection;

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

  settings = gtk_widget_get_settings (widget);

  show_image_connection = 
    g_signal_handler_find (settings, G_SIGNAL_MATCH_FUNC, 0, 0,
                           NULL, gtk_button_setting_changed, NULL);
  
  if (show_image_connection)
    return;

  g_signal_connect (settings, "notify::gtk-button-images",
                    G_CALLBACK (gtk_button_setting_changed), NULL);

  show_image_change_notify (button);
}

static void
gtk_button_state_changed (GtkWidget    *widget,
                          GtkStateType  previous_state)
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
 * gtk_button_set_image:
 * @button: a #GtkButton
 * @image: a widget to set as the image for the button
 *
 * Set the image of @button to the given widget. The image will be
 * displayed if the label text is %NULL or if
 * #GtkButton:always-show-image is %TRUE. You don’t have to call
 * gtk_widget_show() on @image yourself.
 *
 * Since: 2.6
 */
void
gtk_button_set_image (GtkButton *button,
		      GtkWidget *image)
{
  GtkButtonPrivate *priv;
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (image == NULL || GTK_IS_WIDGET (image));

  priv = button->priv;

  if (priv->image)
    {
      parent = gtk_widget_get_parent (priv->image);
      if (parent)
        gtk_container_remove (GTK_CONTAINER (parent), priv->image);
    }

  priv->image = image;
  priv->image_is_stock = (image == NULL);

  gtk_button_construct_child (button);

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_IMAGE]);
}

/**
 * gtk_button_get_image:
 * @button: a #GtkButton
 *
 * Gets the widget that is currenty set as the image of @button.
 * This may have been explicitly set by gtk_button_set_image()
 * or constructed by gtk_button_new_from_stock().
 *
 * Returns: (transfer none): a #GtkWidget or %NULL in case there is no image
 *
 * Since: 2.6
 */
GtkWidget *
gtk_button_get_image (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);
  
  return button->priv->image;
}

/**
 * gtk_button_set_image_position:
 * @button: a #GtkButton
 * @position: the position
 *
 * Sets the position of the image relative to the text 
 * inside the button.
 *
 * Since: 2.10
 */ 
void
gtk_button_set_image_position (GtkButton       *button,
			       GtkPositionType  position)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (position >= GTK_POS_LEFT && position <= GTK_POS_BOTTOM);
  
  priv = button->priv;

  if (priv->image_position != position)
    {
      priv->image_position = position;

      gtk_button_construct_child (button);

      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_IMAGE_POSITION]);
    }
}

/**
 * gtk_button_get_image_position:
 * @button: a #GtkButton
 *
 * Gets the position of the image relative to the text 
 * inside the button.
 *
 * Returns: the position
 *
 * Since: 2.10
 */
GtkPositionType
gtk_button_get_image_position (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), GTK_POS_LEFT);
  
  return button->priv->image_position;
}

/**
 * gtk_button_set_always_show_image:
 * @button: a #GtkButton
 * @always_show: %TRUE if the menuitem should always show the image
 *
 * If %TRUE, the button will ignore the #GtkSettings:gtk-button-images
 * setting and always show the image, if available.
 *
 * Use this property if the button  would be useless or hard to use
 * without the image.
 *
 * Since: 3.6
 */
void
gtk_button_set_always_show_image (GtkButton *button,
                                  gboolean    always_show)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  if (priv->always_show_image != always_show)
    {
      priv->always_show_image = always_show;

      if (priv->image)
        {
          if (show_image (button))
            gtk_widget_show (priv->image);
          else
            gtk_widget_hide (priv->image);
        }

      g_object_notify_by_pspec (G_OBJECT (button), props[PROP_ALWAYS_SHOW_IMAGE]);
    }
}

/**
 * gtk_button_get_always_show_image:
 * @button: a #GtkButton
 *
 * Returns whether the button will ignore the #GtkSettings:gtk-button-images
 * setting and always show the image, if available.
 *
 * Returns: %TRUE if the button will always show the image
 *
 * Since: 3.6
 */
gboolean
gtk_button_get_always_show_image (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return button->priv->always_show_image;
}

/**
 * gtk_button_get_event_window:
 * @button: a #GtkButton
 *
 * Returns the button’s event window if it is realized, %NULL otherwise.
 * This function should be rarely needed.
 *
 * Returns: (transfer none): @button’s event window.
 *
 * Since: 2.22
 */
GdkWindow*
gtk_button_get_event_window (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  return button->priv->event_window;
}
