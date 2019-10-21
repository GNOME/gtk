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
 *
 * This object was added in 3.24.
 **/

#include "config.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkbindings.h"

#include <gdk/gdk.h>

struct _GtkEventControllerKey
{
  GtkEventController parent_instance;
  GtkIMContext *im_context;
  GHashTable *pressed_keys;

  GdkModifierType state;

  const GdkEvent *current_event;
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

G_DEFINE_TYPE (GtkEventControllerKey, gtk_event_controller_key,
               GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_event_controller_finalize (GObject *object)
{
  GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY (object);

  g_hash_table_destroy (key->pressed_keys);
  g_clear_object (&key->im_context);

  G_OBJECT_CLASS (gtk_event_controller_key_parent_class)->finalize (object);
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
      if (event->focus_change.in)
        g_signal_emit (controller, signals[FOCUS_IN], 0);
      else
        g_signal_emit (controller, signals[FOCUS_OUT], 0);

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
gtk_event_controller_key_class_init (GtkEventControllerKeyClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_event_controller_finalize;
  controller_class->handle_event = gtk_event_controller_key_handle_event;

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
   *
   * Since: 3.24
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
   *
   * Since: 3.24
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

  signals[IM_UPDATE] =
    g_signal_new (I_("im-update"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  signals[FOCUS_IN] =
    g_signal_new (I_("focus-in"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  signals[FOCUS_OUT] =
    g_signal_new (I_("focus-out"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_event_controller_key_init (GtkEventControllerKey *controller)
{
  controller->pressed_keys = g_hash_table_new (NULL, NULL);
}

GtkEventController *
gtk_event_controller_key_new (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_KEY,
                       "widget", widget,
                       NULL);
}

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
 * Gets the IM context of a key controller.
 *
 * Returns: (transfer none): the IM context
 *
 * Since: 3.24
 **/
GtkIMContext *
gtk_event_controller_key_get_im_context (GtkEventControllerKey *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), NULL);

  return controller->im_context;
}

gboolean
gtk_event_controller_key_forward (GtkEventControllerKey *controller,
                                  GtkWidget             *widget)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (controller->current_event != NULL, FALSE);

  if (!gtk_widget_get_realized (widget))
    gtk_widget_realize (widget);

  if (_gtk_widget_captured_event (widget, (GdkEvent *) controller->current_event))
    return TRUE;
  if (gtk_widget_event (widget, (GdkEvent *) controller->current_event))
    return TRUE;

  return FALSE;
}

guint
gtk_event_controller_key_get_group (GtkEventControllerKey *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), FALSE);
  g_return_val_if_fail (controller->current_event != NULL, FALSE);

  return controller->current_event->key.group;
}
