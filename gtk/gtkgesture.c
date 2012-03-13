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

#include "gtkgesture.h"

#include "gtkeventtrackerprivate.h"
#include "gtkintl.h"
#include "gtksequence.h"

/**
 * SECTION:gtkgesture
 * @Short_description: Gesture recognition
 * @Title: GtkGesture
 * @See_also: #GtkGestureRecognizer, #GtkEventTracker
 *
 * The #GtkGesture object - or rather its subclasses - are #GtkEventTrackers
 * that cooperates with other #GtkGestures on the recognizing of
 * #GdkSequences.
 *
 * #GtkGesture was added in GTK 3.6.
 */


enum {
  PROP_0
};

struct _GtkGesturePrivate {
  GPtrArray *sequences;

  guint accepted :1;
};

G_DEFINE_ABSTRACT_TYPE (GtkGesture, gtk_gesture, GTK_TYPE_EVENT_TRACKER)

static void
gtk_gesture_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  //GtkGesture *gesture = GTK_GESTURE (object);
  //GtkGesturePrivate *priv = gesture->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gesture_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  //GtkGesture *gesture = GTK_GESTURE (object);
  //GtkGesturePrivate *priv = gesture->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gesture_remove_all_sequences (GtkGesture *gesture)
{
  GtkGesturePrivate *priv = gesture->priv;
  
  if (priv->accepted)
    return;

  while (priv->sequences->len > 0)
    gtk_gesture_remove_sequence (gesture, g_ptr_array_index (priv->sequences,
                                                             priv->sequences->len - 1));
}

static void
gtk_gesture_dispose (GObject *object)
{
  GtkGesture *gesture = GTK_GESTURE (object);
  GtkGesturePrivate *priv = gesture->priv;

  gtk_gesture_remove_all_sequences (gesture);

  g_ptr_array_unref (priv->sequences);
  priv->sequences = NULL;

  G_OBJECT_CLASS (gtk_gesture_parent_class)->dispose (object);
}

static void
gtk_gesture_default_sequence_given (GtkGesture       *gesture,
                                    GdkEventSequence *sequence)
{
}

static void
gtk_gesture_default_sequence_stolen (GtkGesture       *gesture,
                                     GdkEventSequence *sequence,
                                     GtkGesture       *stealer)
{
  gtk_event_tracker_cancel (GTK_EVENT_TRACKER (gesture));
}

static void
gtk_gesture_class_init (GtkGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->sequence_stolen = gtk_gesture_default_sequence_stolen;
  klass->sequence_given = gtk_gesture_default_sequence_given;

  object_class->dispose = gtk_gesture_dispose;
  object_class->set_property = gtk_gesture_set_property;
  object_class->get_property = gtk_gesture_get_property;

  g_type_class_add_private (object_class, sizeof (GtkGesturePrivate));
}

static void
gtk_gesture_init (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gesture->priv = G_TYPE_INSTANCE_GET_PRIVATE (gesture,
                                                      GTK_TYPE_GESTURE,
                                                      GtkGesturePrivate);

  priv->sequences = g_ptr_array_new ();
}

static void
gtk_gesture_accept_sequence (GtkGesture       *stealer,
                             GdkEventSequence *sequence)
{
  GtkEventTracker *tracker, *next;

  for (tracker = _gtk_event_tracker_get_next (GTK_EVENT_TRACKER (stealer));
       tracker;
       tracker = next)
    {
      GtkGesture *gesture;

      next = _gtk_event_tracker_get_next (tracker);

      if (!GTK_IS_GESTURE (tracker))
        continue;

      gesture = GTK_GESTURE (tracker);

      if (!gtk_gesture_has_sequence (gesture, sequence))
        continue;

      /* need some magic here because a lot of gestures wanna cancel themselves
       * in this vfunc */
      g_object_ref (gesture);

      GTK_GESTURE_GET_CLASS (gesture)->sequence_stolen (gesture,
                                                        sequence,
                                                        stealer);

      next = _gtk_event_tracker_get_next (tracker);
      g_object_unref (gesture);
    }
}

static void
gtk_gesture_give_sequence (GtkGesture       *gesture,
                           GdkEventSequence *sequence)
{
  GtkGestureClass *klass = GTK_GESTURE_GET_CLASS (gesture);

  klass->sequence_given (gesture, sequence);
}

/**
 * gtk_gesture_add_sequence:
 * @gesture: the gesture
 * @sequence: the sequence to add to @gesture
 *
 * Marks @gesture as working with @sequence. If @gesture is or gets
 * accepted, other gestures will be notified about @gesture using @sequence
 * and @gesture will be notified if other gestures use @sequence.
 **/
void
gtk_gesture_add_sequence (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  gboolean new_owner;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (sequence != NULL);

  priv = gesture->priv;
  new_owner = gtk_sequence_get_owner (sequence) == NULL;

  g_ptr_array_add (priv->sequences, sequence);
  if (new_owner)
    gtk_gesture_give_sequence (gesture, sequence);
  if (priv->accepted)
    gtk_gesture_accept_sequence (gesture, sequence);
}

/**
 * gtk_gesture_remove_sequence:
 * @gesture: the gesture
 * @sequence: the sequence to remove. It must have been added with 
 *   gtk_gesture_add_sequence() previously.
 *
 * Removes the sequence from the list of tracked sequences. This function
 * does nothing if the gesture has been accepted. 
 *
 * This function should only be called from #GtkGesture implementations.
 **/
void
gtk_gesture_remove_sequence (GtkGesture       *gesture,
                             GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  gboolean was_owner;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (sequence != NULL);

  priv = gesture->priv;

  if (priv->accepted)
    return;

  was_owner = gtk_gesture_owns_sequence (gesture, sequence);

  g_ptr_array_remove (priv->sequences, sequence);

  if (was_owner)
    {
      GtkEventTracker *tracker;

      for (tracker = _gtk_event_tracker_get_next (GTK_EVENT_TRACKER (gesture));
           tracker;
           tracker = _gtk_event_tracker_get_next (tracker))
        {
          if (GTK_IS_GESTURE (tracker) &&
              gtk_gesture_has_sequence (GTK_GESTURE (tracker), sequence))
            {
              gtk_gesture_give_sequence (GTK_GESTURE (tracker), sequence);
              break;
            }
        }
    }
}

gboolean
gtk_gesture_has_sequence (GtkGesture       *gesture,
                          GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  guint i;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (sequence != NULL, FALSE);

  priv = gesture->priv;

  for (i = 0; i < priv->sequences->len; i++)
    {
      if (g_ptr_array_index (priv->sequences, i) == sequence)
        return TRUE;
    }

  return FALSE;
}

gboolean
gtk_gesture_owns_sequence (GtkGesture       *gesture,
                           GdkEventSequence *sequence)
{
  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (sequence != NULL, FALSE);

  return gtk_sequence_get_owner (sequence) == gesture;
}

gboolean
gtk_gesture_owns_all_sequences (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  guint i;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gesture->priv;

  for (i = 0; i < priv->sequences->len; i++)
    {
      if (!gtk_gesture_owns_sequence (gesture,
                                      g_ptr_array_index (priv->sequences, i)))
          return FALSE;
    }

  return TRUE;
}

/**
 * gtk_gesture_accept:
 * @gesture: the gesture to accept
 *
 * Accepts @gesture if it hasn't been accepted yet. An accepted gesture
 * is a gesture that will successfully finish to recognize itself.
 *
 * If a gesture marks itself as accepted, all its sequences that were
 * added with gtk_gesture_add_sequence() are assumed to be consumed by
 * this @gesture and other gestures will be notified about this.
 *
 * This function should only be called from #GtkGesture implementations.
 **/
void
gtk_gesture_accept (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  guint i;

  g_return_if_fail (GTK_IS_GESTURE (gesture));

  priv = gesture->priv;

  if (priv->accepted)
    return;

  priv->accepted = TRUE;

  for (i = 0; i < priv->sequences->len; i++)
    {
      gtk_gesture_accept_sequence (gesture,
                                   g_ptr_array_index (priv->sequences, i));
    }
}

gboolean
gtk_gesture_is_accepted (GtkGesture *gesture)
{
  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  return gesture->priv->accepted;
}

