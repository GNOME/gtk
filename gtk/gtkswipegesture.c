/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkswipegesture.h"

#include "gtksequencetrackerprivate.h"
#include "gtkswiperecognizer.h"

#include <math.h>

/**
 * SECTION:gtkswipegesture
 * @Short_description: Tracks the swipes from a #GtkSwipeRecognizer
 * @Title: GtkSwipeGesture
 * @See_also: #GtkSwipeRecognizer
 *
 * The #GtkSwipeGesture object - or a subclass of it - is used to track
 * sequences of swipes as recognized by a #GtkSwipeRecognizer. Once the
 * recognizer finds it can potentially identify a sequence of swipes, it
 * creates a #GtkSwipeGesture and uses it to store information about the
 * swipe. See the swipe handling howto for a highlevel overview.
 *
 * #GtkSwipeGesture was added in GTK 3.6.
 */


enum {
  PROP_0,
};

struct _GtkSwipeGesturePrivate {
  GtkSequenceTracker *sequence[2];
  GtkMovementDirection direction;
};

G_DEFINE_TYPE (GtkSwipeGesture, gtk_swipe_gesture, GTK_TYPE_EVENT_TRACKER)

static void
gtk_swipe_gesture_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  //GtkSwipeGesture *gesture = GTK_SWIPE_GESTURE (object);
  //GtkSwipeGesturePrivate *priv = gesture->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_swipe_gesture_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  //GtkSwipeGesture *gesture = GTK_SWIPE_GESTURE (object);
  //GtkSwipeGesturePrivate *priv = gesture->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_swipe_gesture_dispose (GObject *object)
{
  //GtkSwipeGesture *gesture = GTK_SWIPE_GESTURE (object);

  G_OBJECT_CLASS (gtk_swipe_gesture_parent_class)->dispose (object);
}

static void
gtk_swipe_gesture_class_init (GtkSwipeGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_swipe_gesture_dispose;
  object_class->set_property = gtk_swipe_gesture_set_property;
  object_class->get_property = gtk_swipe_gesture_get_property;

  g_type_class_add_private (object_class, sizeof (GtkSwipeGesturePrivate));
}

static void
gtk_swipe_gesture_init (GtkSwipeGesture *gesture)
{
  GtkSwipeGesturePrivate *priv;

  priv = gesture->priv = G_TYPE_INSTANCE_GET_PRIVATE (gesture,
                                                      GTK_TYPE_SWIPE_GESTURE,
                                                      GtkSwipeGesturePrivate);

  priv->direction = GTK_DIR_ANY;
}

gboolean
_gtk_swipe_gesture_begin (GtkSwipeGesture *gesture,
                          GdkEvent        *event)
{
  GtkSwipeGesturePrivate *priv = gesture->priv;

  if (priv->sequence[1] != NULL)
    return FALSE;

  if (priv->sequence[0] == NULL)
    priv->sequence[0] = _gtk_sequence_tracker_new (event);
  else
    priv->sequence[1] = _gtk_sequence_tracker_new (event);

  if (priv->sequence[1])
    gtk_event_tracker_start (GTK_EVENT_TRACKER (gesture));

  return FALSE;
}

gboolean
gtk_swipe_gesture_update_for_event (GtkSwipeGesture *gesture,
                                    GdkEvent        *event)
{
  GtkSwipeGesturePrivate *priv = gesture->priv;
  gboolean result = FALSE;
  guint i;

  for (i = 0; i < 2; i++)
    {
      if (priv->sequence[i] &&
          _gtk_sequence_tracker_update (priv->sequence[i], event))
        {
          priv->direction &= _gtk_sequence_tracker_get_direction (priv->sequence[i]);
          if (priv->direction == 0)
            {
              gtk_event_tracker_cancel (GTK_EVENT_TRACKER (gesture));
              return FALSE;
            }

          result = TRUE;
        }
    }

  return result;
}

gboolean
_gtk_swipe_gesture_update (GtkSwipeGesture *gesture,
                           GdkEvent        *event)
{
  if (gtk_swipe_gesture_update_for_event (gesture, event) &&
      gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)))
    gtk_event_tracker_update (GTK_EVENT_TRACKER (gesture));

  return FALSE;
}

gboolean
_gtk_swipe_gesture_end (GtkSwipeGesture *gesture,
                        GdkEvent        *event)
{
  if (gtk_swipe_gesture_update_for_event (gesture, event))
    {
      if (gesture->priv->sequence[1])
        gtk_event_tracker_finish (GTK_EVENT_TRACKER (gesture));
      else
        gtk_event_tracker_cancel (GTK_EVENT_TRACKER (gesture));
    }

  return FALSE;
}

gboolean
_gtk_swipe_gesture_cancel (GtkSwipeGesture *gesture,
                           GdkEvent        *event)
{
  if (gtk_swipe_gesture_update_for_event (gesture, event))
    gtk_event_tracker_cancel (GTK_EVENT_TRACKER (gesture));

  return FALSE;
}

void
gtk_swipe_gesture_get_offset (GtkSwipeGesture *gesture,
                              double          *x,
                              double          *y)
{
  GtkSwipeGesturePrivate *priv;

  g_return_if_fail (GTK_IS_SWIPE_GESTURE (gesture));
  
  priv = gesture->priv;

  if (!gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)) ||
      gtk_event_tracker_is_cancelled (GTK_EVENT_TRACKER (gesture)))
    {
      if (x)
        *x = 0;
      if (y)
        *y = 0;

      return;
    }

  if (x)
    *x = (_gtk_sequence_tracker_get_x_offset (priv->sequence[0])
          + _gtk_sequence_tracker_get_x_offset (priv->sequence[1])) / 2;
  if (y)
    *y = (_gtk_sequence_tracker_get_y_offset (priv->sequence[0])
          + _gtk_sequence_tracker_get_y_offset (priv->sequence[1])) / 2;
}

