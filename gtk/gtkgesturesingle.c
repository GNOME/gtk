/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
 * SECTION:gtkgesturesingle
 * @Short_description: Base class for mouse/single-touch gestures
 * @Title: GtkGestureSingle
 *
 * #GtkGestureSingle is a subclass of #GtkGesture, optimized (although
 * not restricted) for dealing with mouse and single-touch gestures. Under
 * interaction, these gestures stick to the first interacting sequence, which
 * is accessible through gtk_gesture_single_get_current_sequence() while the
 * gesture is being interacted with.
 *
 * By default gestures react to both %GDK_BUTTON_PRIMARY and touch
 * events, gtk_gesture_single_set_touch_only() can be used to change the
 * touch behavior. Callers may also specify a different mouse button number
 * to interact with through gtk_gesture_single_set_button(), or react to any
 * mouse button by setting 0. While the gesture is active, the button being
 * currently pressed can be known through gtk_gesture_single_get_current_button().
 */

#include "config.h"
#include "gtkgesturesingle.h"
#include "gtkgesturesingleprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkdebug.h"

typedef struct _GtkGestureSinglePrivate GtkGestureSinglePrivate;

struct _GtkGestureSinglePrivate
{
  GdkEventSequence *current_sequence;
  guint button;
  guint current_button;
  guint touch_only : 1;
  guint exclusive  : 1;
};

enum {
  PROP_TOUCH_ONLY = 1,
  PROP_EXCLUSIVE,
  PROP_BUTTON
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureSingle, gtk_gesture_single,
                            GTK_TYPE_GESTURE)

static void
gtk_gesture_single_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkGestureSinglePrivate *priv;

  priv = gtk_gesture_single_get_instance_private (GTK_GESTURE_SINGLE (object));

  switch (prop_id)
    {
    case PROP_TOUCH_ONLY:
      g_value_set_boolean (value, priv->touch_only);
      break;
    case PROP_EXCLUSIVE:
      g_value_set_boolean (value, priv->exclusive);
      break;
    case PROP_BUTTON:
      g_value_set_uint (value, priv->button);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_single_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_TOUCH_ONLY:
      gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (object),
                                         g_value_get_boolean (value));
      break;
    case PROP_EXCLUSIVE:
      gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (object),
                                        g_value_get_boolean (value));
      break;
    case PROP_BUTTON:
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (object),
                                     g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_single_cancel (GtkGesture       *gesture,
                           GdkEventSequence *sequence)
{
  GtkGestureSinglePrivate *priv;

  priv = gtk_gesture_single_get_instance_private (GTK_GESTURE_SINGLE (gesture));

  if (sequence == priv->current_sequence)
    priv->current_button = 0;
}

static gboolean
gtk_gesture_single_handle_event (GtkEventController *controller,
                                 const GdkEvent     *event)
{
  GdkEventSequence *sequence = NULL;
  GtkGestureSinglePrivate *priv;
  GdkDevice *source_device;
  GdkInputSource source;
  guint button = 0, i;
  gboolean retval, test_touchscreen = FALSE;

  source_device = gdk_event_get_source_device (event);

  if (!source_device)
    return FALSE;

  priv = gtk_gesture_single_get_instance_private (GTK_GESTURE_SINGLE (controller));
  source = gdk_device_get_source (source_device);

  if (source != GDK_SOURCE_TOUCHSCREEN)
    test_touchscreen = ((gtk_get_debug_flags () & GTK_DEBUG_TOUCHSCREEN) != 0 ||
                        g_getenv ("GTK_TEST_TOUCHSCREEN"));

  switch (event->type)
    {
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
    case GDK_TOUCH_UPDATE:
      if (priv->exclusive && !event->touch.emulating_pointer)
        return FALSE;

      sequence = event->touch.sequence;
      button = 1;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      if (priv->touch_only && !test_touchscreen && source != GDK_SOURCE_TOUCHSCREEN)
        return FALSE;

      button = event->button.button;
      break;
    case GDK_MOTION_NOTIFY:
      if (!gtk_gesture_handles_sequence (GTK_GESTURE (controller), sequence))
        return FALSE;
      if (priv->touch_only && !test_touchscreen && source != GDK_SOURCE_TOUCHSCREEN)
        return FALSE;

      if (priv->current_button > 0 && priv->current_button <= 5 &&
          (event->motion.state & (GDK_BUTTON1_MASK << (priv->current_button - 1))))
        button = priv->current_button;
      else if (priv->current_button == 0)
        {
          /* No current button, find out from the mask */
          for (i = 0; i < 3; i++)
            {
              if ((event->motion.state & (GDK_BUTTON1_MASK << i)) == 0)
                continue;
              button = i + 1;
              break;
            }
        }

      break;
    case GDK_TOUCH_CANCEL:
    case GDK_GRAB_BROKEN:
      return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_single_parent_class)->handle_event (controller,
                                                                                         event);
      break;
    default:
      return FALSE;
    }

  if (button == 0 ||
      (priv->button != 0 && priv->button != button) ||
      (priv->current_button != 0 && priv->current_button != button))
    {
      if (gtk_gesture_is_active (GTK_GESTURE (controller)))
        gtk_event_controller_reset (controller);
      return FALSE;
    }

  if (event->type == GDK_BUTTON_PRESS || event->type == GDK_TOUCH_BEGIN ||
      event->type == GDK_MOTION_NOTIFY || event->type == GDK_TOUCH_UPDATE)
    {
      if (!gtk_gesture_is_active (GTK_GESTURE (controller)))
        priv->current_sequence = sequence;

      priv->current_button = button;
    }

  retval = GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_single_parent_class)->handle_event (controller, event);

  if (sequence == priv->current_sequence &&
      (event->type == GDK_BUTTON_RELEASE || event->type == GDK_TOUCH_END))
    priv->current_button = 0;

  return retval;
}

static void
gtk_gesture_single_class_init (GtkGestureSingleClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_gesture_single_get_property;
  object_class->set_property = gtk_gesture_single_set_property;

  controller_class->handle_event = gtk_gesture_single_handle_event;

  gesture_class->cancel = gtk_gesture_single_cancel;

  /**
   * GtkGestureSingle:touch-only:
   *
   * Whether the gesture handles only touch events.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_TOUCH_ONLY,
                                   g_param_spec_boolean ("touch-only",
                                                         P_("Handle only touch events"),
                                                         P_("Whether the gesture handles"
                                                            " only touch events"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkGestureSingle:exclusive:
   *
   * Whether the gesture is exclusive. Exclusive gestures only listen to pointer
   * and pointer emulated events.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_EXCLUSIVE,
                                   g_param_spec_boolean ("exclusive",
                                                         P_("Whether the gesture is exclusive"),
                                                         P_("Whether the gesture is exclusive"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkGestureSingle:button:
   *
   * Mouse button number to listen to, or 0 to listen for any button.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_BUTTON,
                                   g_param_spec_uint ("button",
                                                      P_("Button number"),
                                                      P_("Button number to listen to"),
                                                      0, G_MAXUINT, GDK_BUTTON_PRIMARY,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
_gtk_gesture_single_update_evmask (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;
  GdkEventMask evmask;

  priv = gtk_gesture_single_get_instance_private (gesture);
  evmask = GDK_TOUCH_MASK;

  if (!priv->touch_only || g_getenv ("GTK_TEST_TOUCHSCREEN") ||
      (gtk_get_debug_flags () & GTK_DEBUG_TOUCHSCREEN) != 0)
    evmask |= GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
      GDK_BUTTON_MOTION_MASK;

  gtk_event_controller_set_event_mask (GTK_EVENT_CONTROLLER (gesture), evmask);
}

static void
gtk_gesture_single_init (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  priv = gtk_gesture_single_get_instance_private (gesture);
  priv->touch_only = FALSE;
  priv->button = GDK_BUTTON_PRIMARY;
  _gtk_gesture_single_update_evmask (gesture);
}

/**
 * gtk_gesture_single_get_touch_only:
 * @gesture: a #GtkGestureSingle
 *
 * Returns %TRUE if the gesture is only triggered by touch events.
 *
 * Returns: %TRUE if the gesture only handles touch events
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_single_get_touch_only (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), FALSE);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->touch_only;
}

/**
 * gtk_gesture_single_set_touch_only:
 * @gesture: a #GtkGestureSingle
 * @touch_only: whether @gesture handles only touch events
 *
 * If @touch_only is %TRUE, @gesture will only handle events of type
 * #GDK_TOUCH_BEGIN, #GDK_TOUCH_UPDATE or #GDK_TOUCH_END. If %FALSE,
 * mouse events will be handled too.
 *
 * Since: 3.14
 **/
void
gtk_gesture_single_set_touch_only (GtkGestureSingle *gesture,
                                   gboolean          touch_only)
{
  GtkGestureSinglePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_SINGLE (gesture));

  touch_only = touch_only != FALSE;
  priv = gtk_gesture_single_get_instance_private (gesture);

  if (priv->touch_only == touch_only)
    return;

  priv->touch_only = touch_only;
  _gtk_gesture_single_update_evmask (gesture);
  g_object_notify (G_OBJECT (gesture), "touch-only");
}

/**
 * gtk_gesture_single_get_exclusive:
 * @gesture: a #GtkGestureSingle
 *
 * Gets whether a gesture is exclusive. For more information, see
 * gtk_gesture_single_set_exclusive().
 *
 * Returns: Whether the gesture is exclusive
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_single_get_exclusive (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), FALSE);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->exclusive;
}

/**
 * gtk_gesture_single_set_exclusive:
 * @gesture: a #GtkGestureSingle
 * @exclusive: %TRUE to make @gesture exclusive
 *
 * Sets whether @gesture is exclusive. An exclusive gesture will
 * only handle pointer and "pointer emulated" touch events, so at
 * any given time, there is only one sequence able to interact with
 * those.
 *
 * Since: 3.14
 **/
void
gtk_gesture_single_set_exclusive (GtkGestureSingle *gesture,
                                  gboolean          exclusive)
{
  GtkGestureSinglePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_SINGLE (gesture));

  exclusive = exclusive != FALSE;
  priv = gtk_gesture_single_get_instance_private (gesture);

  if (priv->exclusive == exclusive)
    return;

  priv->exclusive = exclusive;
  _gtk_gesture_single_update_evmask (gesture);
  g_object_notify (G_OBJECT (gesture), "exclusive");
}

/**
 * gtk_gesture_single_get_button:
 * @gesture: a #GtkGestureSingle
 *
 * Returns the button number @gesture listens for, or 0 if @gesture
 * reacts to any button press.
 *
 * Returns: The button number, or 0 for any button
 *
 * Since: 3.14
 **/
guint
gtk_gesture_single_get_button (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), 0);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->button;
}

/**
 * gtk_gesture_single_set_button:
 * @gesture: a #GtkGestureSingle
 * @button: button number to listen to, or 0 for any button
 *
 * Sets the button number @gesture listens to. If non-0, every
 * button press from a different button number will be ignored.
 * Touch events implicitly match with button 1.
 *
 * Since: 3.14
 **/
void
gtk_gesture_single_set_button (GtkGestureSingle *gesture,
                               guint             button)
{
  GtkGestureSinglePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_SINGLE (gesture));

  priv = gtk_gesture_single_get_instance_private (gesture);

  if (priv->button == button)
    return;

  priv->button = button;
  g_object_notify (G_OBJECT (gesture), "button");
}

/**
 * gtk_gesture_single_get_current_button:
 * @gesture: a #GtkGestureSingle
 *
 * Returns the button number currently interacting with @gesture, or 0 if there
 * is none.
 *
 * Returns: The current button number
 *
 * Since: 3.14
 **/
guint
gtk_gesture_single_get_current_button (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), 0);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->current_button;
}

/**
 * gtk_gesture_single_get_current_sequence:
 * @gesture: a #GtkGestureSingle
 *
 * Returns the event sequence currently interacting with @gesture.
 * This is only meaningful if gtk_gesture_is_active() returns %TRUE.
 *
 * Returns: the current sequence
 *
 * Since: 3.14
 **/
GdkEventSequence *
gtk_gesture_single_get_current_sequence (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), NULL);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->current_sequence;
}
