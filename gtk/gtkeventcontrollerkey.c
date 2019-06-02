/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */

/**
 * SECTION:gtkeventcontrollerkey
 * @Short_description: Event controller for key events
 * @Title: GtkEventControllerKey
 * @See_also: #GtkEventController
 *
 * #GtkEventControllerKey is an event controller meant for situations
 * where you need access to key events.
 **/

#include "config.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkbindings.h"
#include "gtkenums.h"
#include "gtkmain.h"

#include <gdk/gdk.h>

struct _GtkEventControllerKey
{
  GtkEventController parent_instance;
  GtkIMContext *im_context;
  GHashTable *pressed_keys;

  GdkModifierType state;

  const GdkEvent *current_event;

  guint is_focus       : 1;
  guint contains_focus : 1;
};

struct _GtkEventControllerKeyClass
{
  GtkEventControllerClass parent_class;
};

enum {
  KEY_PRESSED,
  KEY_RELEASED,
  MODIFIERS,
  IM_UPDATE,
  FOCUS_IN,
  FOCUS_OUT,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

enum {
  PROP_IS_FOCUS = 1,
  PROP_CONTAINS_FOCUS,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkEventControllerKey, gtk_event_controller_key,
               GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_event_controller_key_finalize (GObject *object)
{
  GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY (object);

  g_hash_table_destroy (key->pressed_keys);
  g_clear_object (&key->im_context);

  G_OBJECT_CLASS (gtk_event_controller_key_parent_class)->finalize (object);
}

static void
update_focus (GtkEventControllerKey *key,
              gboolean               focus_in,
              GdkNotifyType          detail)
{
  gboolean is_focus;
  gboolean contains_focus;

  switch (detail)
    {
    case GDK_NOTIFY_VIRTUAL:
    case GDK_NOTIFY_NONLINEAR_VIRTUAL:
      is_focus = FALSE;
      contains_focus = focus_in;
      break;
    case GDK_NOTIFY_ANCESTOR:
    case GDK_NOTIFY_NONLINEAR:
      is_focus = focus_in;
      contains_focus = FALSE;
      break;
    case GDK_NOTIFY_INFERIOR:
      is_focus = focus_in;
      contains_focus = !focus_in;
      break;
    case GDK_NOTIFY_UNKNOWN:
    default:
      g_warning ("Unknown focus change detail");
      return;
    }

  g_object_freeze_notify (G_OBJECT (key));
  if (key->is_focus != is_focus)
    {
      key->is_focus = is_focus;
      g_object_notify (G_OBJECT (key), "is-focus");
    }
  if (key->contains_focus != contains_focus)
    {
      key->contains_focus = contains_focus;
      g_object_notify (G_OBJECT (key), "contains-focus");
    }
  g_object_thaw_notify (G_OBJECT (key));
}

static gboolean
gtk_event_controller_key_handle_event (GtkEventController *controller,
                                       const GdkEvent     *event)
{
  GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY (controller);
  GdkEventType event_type = gdk_event_get_event_type (event);
  GdkModifierType state;
  guint16 keycode;
  guint keyval;
  gboolean handled = FALSE;

  if (event_type == GDK_FOCUS_CHANGE)
    {
      gboolean focus_in;
      GdkCrossingMode mode;
      GdkNotifyType detail;

      gdk_event_get_focus_in (event, &focus_in);
      gdk_event_get_crossing_mode (event, &mode);
      gdk_event_get_crossing_detail (event, &detail);

      update_focus (key, focus_in, detail);

      key->current_event = event;

      if (focus_in)
        g_signal_emit (controller, signals[FOCUS_IN], 0, mode, detail);
      else
        g_signal_emit (controller, signals[FOCUS_OUT], 0, mode, detail);

      key->current_event = NULL;

      return FALSE;
    }

  if (event_type != GDK_KEY_PRESS && event_type != GDK_KEY_RELEASE)
    return FALSE;

  if (key->im_context &&
      gtk_im_context_filter_keypress (key->im_context, (GdkEventKey *) event))
    {
      g_signal_emit (controller, signals[IM_UPDATE], 0);
      return TRUE;
    }

  key->current_event = event;

  gdk_event_get_state (event, &state);
  if (key->state != state)
    {
      gboolean unused;

      key->state = state;
      g_signal_emit (controller, signals[MODIFIERS], 0, state, &unused);
    }

  gdk_event_get_keycode (event, &keycode);
  gdk_event_get_keyval (event, &keyval);

  if (event_type == GDK_KEY_PRESS)
    {
      g_signal_emit (controller, signals[KEY_PRESSED], 0,
                     keyval, keycode, state, &handled);
      if (handled)
        g_hash_table_add (key->pressed_keys, GUINT_TO_POINTER (keyval));
    }
  else if (event_type == GDK_KEY_RELEASE)
    {
      g_signal_emit (controller, signals[KEY_RELEASED], 0,
                     keyval, keycode, state);

      handled = g_hash_table_lookup (key->pressed_keys, GUINT_TO_POINTER (keyval)) != NULL;
      g_hash_table_remove (key->pressed_keys, GUINT_TO_POINTER (keyval));
    }
  else
    handled = FALSE;

  key->current_event = NULL;

  return handled;
}

static void
gtk_event_controller_key_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkEventControllerKey *controller = GTK_EVENT_CONTROLLER_KEY (object);

  switch (prop_id)
    {
    case PROP_IS_FOCUS:
      g_value_set_boolean (value, controller->is_focus);
      break;

    case PROP_CONTAINS_FOCUS:
      g_value_set_boolean (value, controller->contains_focus);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_key_class_init (GtkEventControllerKeyClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_event_controller_key_finalize;
  object_class->get_property = gtk_event_controller_key_get_property;
  controller_class->handle_event = gtk_event_controller_key_handle_event;

  /**
   * GtkEventControllerKey:is-focus:
   *
   * Whether focus is in the controllers widget itself,
   * as opposed to in a descendent widget. See
   * #GtkEventControllerKey:contains-focus.
   *
   * When handling focus events, this property is updated
   * before #GtkEventControllerKey::focus-in or
   * #GtkEventControllerKey::focus-out are emitted.
   */
  props[PROP_IS_FOCUS] =
      g_param_spec_boolean ("is-focus",
                            P_("Is Focus"),
                            P_("Whether the focus is in the controllers widget"),
                            FALSE,
                            G_PARAM_READABLE);

  /**
   * GtkEventControllerKey:contains-focus:
   *
   * Whether focus is in a descendant of the controllers widget.
   * See #GtkEventControllerKey:is-focus.
   *
   * When handling focus events, this property is updated
   * before #GtkEventControllerKey::focus-in or
   * #GtkEventControllerKey::focus-out are emitted.
   */
  props[PROP_CONTAINS_FOCUS] =
      g_param_spec_boolean ("contains-focus",
                            P_("Contains Focus"),
                            P_("Whether the focus is in a descendant of the controllers widget"),
                            FALSE,
                            G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  /**
   * GtkEventControllerKey::key-pressed:
   * @controller: the object which received the signal.
   * @keyval: the pressed key.
   * @keycode: the raw code of the pressed key.
   * @state: the bitmask, representing the state of modifier keys and pointer buttons. See #GdkModifierType.
   *
   * This signal is emitted whenever a key is pressed.
   *
   * Returns: %TRUE if the key press was handled, %FALSE otherwise.
   */
  signals[KEY_PRESSED] =
    g_signal_new (I_("key-pressed"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__UINT_UINT_FLAGS,
                  G_TYPE_BOOLEAN, 3, G_TYPE_UINT, G_TYPE_UINT, GDK_TYPE_MODIFIER_TYPE);
  g_signal_set_va_marshaller (signals[KEY_PRESSED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__UINT_UINT_FLAGSv);

  /**
   * GtkEventControllerKey::key-released:
   * @controller: the object which received the signal.
   * @keyval: the released key.
   * @keycode: the raw code of the released key.
   * @state: the bitmask, representing the state of modifier keys and pointer buttons. See #GdkModifierType.
   *
   * This signal is emitted whenever a key is released.
   */
  signals[KEY_RELEASED] =
    g_signal_new (I_("key-released"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__UINT_UINT_FLAGS,
                  G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, GDK_TYPE_MODIFIER_TYPE);
  g_signal_set_va_marshaller (signals[KEY_RELEASED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__UINT_UINT_FLAGSv);

  /**
   * GtkEventControllerKey::modifiers:
   * @controller: the object which received the signal.
   * @keyval: the released key.
   * @state: the bitmask, representing the new state of modifier keys and
   *   pointer buttons. See #GdkModifierType.
   *
   * This signal is emitted whenever the state of modifier keys and pointer
   * buttons change.
   */
  signals[MODIFIERS] =
    g_signal_new (I_("modifiers"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL,
                  NULL,
                  _gtk_marshal_BOOLEAN__FLAGS,
                  G_TYPE_BOOLEAN, 1, GDK_TYPE_MODIFIER_TYPE);
  g_signal_set_va_marshaller (signals[MODIFIERS],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__FLAGSv);

  /**
   * GtkEventControllerKey::im-update:
   * @controller: the object which received the signal.
   *
   * This signal is emitted whenever the input method context filters away a
   * keypress and prevents the @controller receiving it. See
   * gtk_event_controller_key_set_im_context() and
   * gtk_im_context_filter_keypress().
   */
  signals[IM_UPDATE] =
    g_signal_new (I_("im-update"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEventControllerKey::focus-in:
   * @controller: the object which received the signal.
   * @mode: crossing mode indicating what caused this change
   * @detail: detail indication where the focus is coming from
   *
   * This signal is emitted whenever the widget controlled
   * by the @controller or one of its descendants) is given
   * the keyboard focus.
   */
  signals[FOCUS_IN] =
    g_signal_new (I_("focus-in"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  GDK_TYPE_CROSSING_MODE,
                  GDK_TYPE_NOTIFY_TYPE);

  /**
   * GtkEventControllerKey::focus-out:
   * @controller: the object which received the signal.
   * @mode: crossing mode indicating what caused this change
   * @detail: detail indication where the focus is going
   *
   * This signal is emitted whenever the widget controlled
   * by the @controller (or one of its descendants) loses
   * the keyboard focus.
   */
  signals[FOCUS_OUT] =
    g_signal_new (I_("focus-out"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  GDK_TYPE_CROSSING_MODE,
                  GDK_TYPE_NOTIFY_TYPE);
}

static void
gtk_event_controller_key_init (GtkEventControllerKey *controller)
{
  controller->pressed_keys = g_hash_table_new (NULL, NULL);
}

/**
 * gtk_event_controller_key_new:
 *
 * Creates a new event controller that will handle key events.
 *
 * Returns: a new #GtkEventControllerKey
 **/
GtkEventController *
gtk_event_controller_key_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_KEY, NULL);
}

/**
 * gtk_event_controller_key_set_im_context:
 * @controller: a #GtkEventControllerKey
 * @im_context: a #GtkIMContext
 *
 * Sets the input method context of the key @controller.
 **/
void
gtk_event_controller_key_set_im_context (GtkEventControllerKey *controller,
                                         GtkIMContext          *im_context)
{
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller));
  g_return_if_fail (!im_context || GTK_IS_IM_CONTEXT (im_context));

  if (controller->im_context)
    gtk_im_context_reset (controller->im_context);

  g_set_object (&controller->im_context, im_context);
}

/**
 * gtk_event_controller_key_get_im_context:
 * @controller: a #GtkEventControllerKey
 *
 * Gets the input method context of the key @controller.
 *
 * Returns: (transfer none): the #GtkIMContext
 **/
GtkIMContext *
gtk_event_controller_key_get_im_context (GtkEventControllerKey *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), NULL);

  return controller->im_context;
}

/**
 * gtk_event_controller_key_forward:
 * @controller: a #GtkEventControllerKey
 * @widget: a #GtkWidget
 *
 * Forwards the current event of this @controller to a @widget.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerKey::key-pressed,
 * #GtkEventControllerKey::key-released
 * or
 * #GtkEventControllerKey::modifiers
 * signals.
 *
 * Returns: whether the @widget handled the event
 **/
gboolean
gtk_event_controller_key_forward (GtkEventControllerKey *controller,
                                  GtkWidget             *widget)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (controller->current_event != NULL, FALSE);
  g_return_val_if_fail (gdk_event_get_event_type (controller->current_event) == GDK_KEY_PRESS ||
                        gdk_event_get_event_type (controller->current_event) == GDK_KEY_RELEASE, FALSE);

  if (!gtk_widget_get_realized (widget))
    gtk_widget_realize (widget);

  if (gtk_widget_run_controllers (widget, controller->current_event,
				  GTK_PHASE_CAPTURE))
    return TRUE;
  if (gtk_widget_run_controllers (widget, controller->current_event,
				  GTK_PHASE_TARGET))
    return TRUE;
  if (gtk_widget_run_controllers (widget, controller->current_event,
				  GTK_PHASE_BUBBLE))
    return TRUE;

  if (gtk_bindings_activate_event (G_OBJECT (widget), (GdkEventKey *)controller->current_event))
    return TRUE;

  return FALSE;
}

/**
 * gtk_event_controller_key_get_group:
 * @controller: a #GtkEventControllerKey
 *
 * Gets the key group of the current event of this @controller.
 * See gdk_event_get_key_group().
 *
 * Returns: the key group
 **/
guint
gtk_event_controller_key_get_group (GtkEventControllerKey *controller)
{
  guint group;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), FALSE);
  g_return_val_if_fail (controller->current_event != NULL, FALSE);

  gdk_event_get_key_group (controller->current_event, &group);

  return group;
}

/**
 * gtk_event_controller_key_get_focus_origin:
 * @controller: a #GtkEventControllerKey
 *
 * Returns the widget that was holding focus before.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerKey::focus-in and
 * #GtkEventControllerKey::focus-out signals.
 *
 * Returns: (transfer none): the previous focus
 */
GtkWidget *
gtk_event_controller_key_get_focus_origin (GtkEventControllerKey *controller)
{
  gboolean focus_in;
  GtkWidget *origin;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), NULL);
  g_return_val_if_fail (controller->current_event != NULL, NULL);
  g_return_val_if_fail (gdk_event_get_event_type (controller->current_event) == GDK_FOCUS_CHANGE, NULL);

  gdk_event_get_focus_in (controller->current_event, &focus_in);

  if (focus_in)
    origin = (GtkWidget *)gdk_event_get_related_target (controller->current_event);
  else
    origin = (GtkWidget *)gdk_event_get_target (controller->current_event);

  return origin;
}

/**
 * gtk_event_controller_key_get_focus_target:
 * @controller: a #GtkEventControllerKey
 *
 * Returns the widget that will be holding focus afterwards.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerKey::focus-in and
 * #GtkEventControllerKey::focus-out signals.
 *
 * Returns: (transfer none): the next focus
 */
GtkWidget *
gtk_event_controller_key_get_focus_target (GtkEventControllerKey *controller)
{
  gboolean focus_in;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), NULL);
  g_return_val_if_fail (controller->current_event != NULL, NULL);
  g_return_val_if_fail (gdk_event_get_event_type (controller->current_event) == GDK_FOCUS_CHANGE, NULL);

  gdk_event_get_focus_in (controller->current_event, &focus_in);

  if (focus_in)
    return (GtkWidget *)gdk_event_get_target (controller->current_event);
  else
    return (GtkWidget *)gdk_event_get_related_target (controller->current_event);
}
