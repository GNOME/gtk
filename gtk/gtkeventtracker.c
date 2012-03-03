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

#include "gtkeventtracker.h"

#include "gtkeventrecognizer.h"
#include "gtkeventrecognizerprivate.h"
#include "gtkintl.h"
#include "gtkwidget.h"

/**
 * SECTION:gtkeventtracker
 * @Short_description: Tracks the events from a #GtkEventRecognizer
 * @Title: GtkEventTracker
 * @See_also: #GtkEventRecognizer
 *
 * The #GtkEventTracker object - or a subclass of it - is used to track
 * sequences of events as recognized by a #GtkEventRecognizer. Once the
 * recognizer finds it can potentially identify a sequence of events, it
 * creates a #GtkEventTracker and uses it to store information about the
 * event. See the event handling howto for a highlevel overview.
 *
 * #GtkEventTracker was added in GTK 3.6.
 */


enum {
  PROP_0,
  PROP_RECOGNIZER,
  PROP_WIDGET
};

struct _GtkEventTrackerPrivate {
  GtkEventRecognizer *recognizer;
  GtkWidget *widget;

  guint started :1;
  guint finished :1;
  guint cancelled :1;
};

G_DEFINE_ABSTRACT_TYPE (GtkEventTracker, gtk_event_tracker, G_TYPE_OBJECT)

static GQueue trackers = G_QUEUE_INIT;

static void
gtk_event_tracker_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkEventTracker *tracker = GTK_EVENT_TRACKER (object);
  GtkEventTrackerPrivate *priv = tracker->priv;

  switch (prop_id)
    {
    case PROP_RECOGNIZER:
      priv->recognizer = g_value_dup_object (value);
      if (priv->recognizer == NULL)
        g_critical ("Attempting to construct a `%s' without a recognizer", G_OBJECT_TYPE_NAME (object));
      break;
    case PROP_WIDGET:
      priv->widget = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_event_tracker_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkEventTracker *tracker = GTK_EVENT_TRACKER (object);
  GtkEventTrackerPrivate *priv = tracker->priv;

  switch (prop_id)
    {
    case PROP_RECOGNIZER:
      g_value_set_object (value, priv->recognizer);
      break;
    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_event_tracker_dispose (GObject *object)
{
  GtkEventTracker *tracker = GTK_EVENT_TRACKER (object);
  GtkEventTrackerPrivate *priv = tracker->priv;

  g_queue_remove (&trackers, tracker);

  g_clear_object (&priv->recognizer);
  g_clear_object (&priv->widget);

  G_OBJECT_CLASS (gtk_event_tracker_parent_class)->dispose (object);
}

static void
gtk_event_tracker_class_init (GtkEventTrackerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_event_tracker_dispose;
  object_class->set_property = gtk_event_tracker_set_property;
  object_class->get_property = gtk_event_tracker_get_property;

  g_object_class_install_property (object_class,
                                   PROP_RECOGNIZER,
                                   g_param_spec_object ("recognizer",
                                                        P_("recognizer"),
                                                        P_("Recognizer running this tracker"),
                                                        GTK_TYPE_EVENT_RECOGNIZER,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        P_("widget"),
                                                        P_("Widget that spawned this tracker"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GtkEventTrackerPrivate));
}

static void
gtk_event_tracker_init (GtkEventTracker *tracker)
{
  tracker->priv = G_TYPE_INSTANCE_GET_PRIVATE (tracker,
                                               GTK_TYPE_EVENT_TRACKER,
                                               GtkEventTrackerPrivate);
}

/**
 * gtk_event_tracker_get_recognizer:
 * @tracker: The event tracker
 *
 * Gets the recognizer that spawned this @tracker.
 *
 * Returns: The recognizer that spawned this tracker
 *
 * @Since: 3.6
 **/
GtkEventRecognizer *
gtk_event_tracker_get_recognizer (GtkEventTracker *tracker)
{
  g_return_val_if_fail (GTK_IS_EVENT_TRACKER (tracker), NULL);

  return tracker->priv->recognizer;
}

/**
 * gtk_event_tracker_get_widget:
 * @tracker: The event tracker
 *
 * Gets the widget that is affected by this tracker.
 *
 * Returns: The widget this tracker runs on or %NULL if none.
 *
 * @Since: 3.6
 **/
GtkWidget *
gtk_event_tracker_get_widget (GtkEventTracker *tracker)
{
  g_return_val_if_fail (GTK_IS_EVENT_TRACKER (tracker), NULL);

  return tracker->priv->widget;
}

/**
 * gtk_event_tracker_is_cancelled:
 * @tracker: The event tracker
 *
 * Queries if the tracker has been cancelled. A #GtkEventTracker
 * can be cancelled for various reasons. See gtk_event_tracker_cancel()
 * for details.
 *
 * Returns: %TRUE if the @tracker has been cancelled
 **/
gboolean
gtk_event_tracker_is_cancelled (GtkEventTracker *tracker)
{
  g_return_val_if_fail (GTK_IS_EVENT_TRACKER (tracker), TRUE);

  return tracker->priv->cancelled;
}

/**
 * gtk_event_tracker_is_started:
 * @tracker: The event tracker
 *
 * Checks if the event tracker is started. An event tracker is
 * considered started after the GtkEventRecognizer::started signal
 * has been emitted for it.
 *
 * Returns: %TRUE if the event tracker is started
 **/
gboolean
gtk_event_tracker_is_started (GtkEventTracker *tracker)
{
  g_return_val_if_fail (GTK_IS_EVENT_TRACKER (tracker), TRUE);

  return tracker->priv->started;
}

/**
 * gtk_event_tracker_is_finished:
 * @tracker: The event tracker
 *
 * Checks if the event tracker is finished. An event tracker is
 * considered finished when it will not process events or emit
 * signals anymore. At that point, the GtkEventRecognizer::finished
 * or GtkEventRecognizer::cancelled signal will have been emitted
 * for @recognizer.
 *
 * Returns: %TRUE if the event tracker is finished
 **/
gboolean
gtk_event_tracker_is_finished (GtkEventTracker *tracker)
{
  g_return_val_if_fail (GTK_IS_EVENT_TRACKER (tracker), TRUE);

  return tracker->priv->finished;
}

/**
 * gtk_event_tracker_cancel:
 * @tracker: The event tracker
 *
 * Cancels the event tracker if the event tracker is not finished yet.
 * If the event tracker was already finished, this function returns
 * immediately.
 *
 * Cancelling an event tracker will cause the 
 * GtkEventRecognizer::cancelled signal to be emitted and the @tracker
 * will not process any new events.
 **/
void
gtk_event_tracker_cancel (GtkEventTracker *tracker)
{
  GtkEventTrackerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_TRACKER (tracker));

  priv = tracker->priv;

  if (priv->finished)
    return;

  priv->finished = TRUE;
  priv->cancelled = TRUE;

  if (priv->started)
    g_signal_emit_by_name (priv->recognizer,
                           "cancelled",
                           tracker);
  else
    priv->started = TRUE;

  /* release the reference from adding the tracker upon construction */
  g_object_unref (tracker);
}

/**
 * gtk_event_tracker_start:
 * @tracker: The tracker to start
 *
 * Emits the GtkEventRecognizer::started signal for the @tracker. This
 * signal should be emitted when @tracker should be made public and
 * widgets using it might want to provide feedback for an impending event
 * recognition.
 *
 * This function should only be called by #GtkEventRecognizer
 * implementations.
 **/
void
gtk_event_tracker_start (GtkEventTracker *tracker)
{
  GtkEventTrackerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_TRACKER (tracker));

  priv = tracker->priv;
  if (priv->started)
    return;

  priv->started = TRUE;

  g_signal_emit_by_name (priv->recognizer,
                         "started",
                         tracker);
}

/**
 * gtk_event_tracker_update:
 * @tracker: The tracker to update
 *
 * Emits the GtkEventRecognizer::updated signal for the @tracker. This
 * signal should be emitted when @tracker has updated its state and
 * widgets might want to update their state based on it.
 *
 * This function should only be called by #GtkEventRecognizer
 * implementations.
 **/
void
gtk_event_tracker_update (GtkEventTracker *tracker)
{
  GtkEventTrackerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_TRACKER (tracker));

  priv = tracker->priv;

  g_object_ref (tracker);

  gtk_event_tracker_start (tracker);

  if (priv->finished)
    return;

  g_signal_emit_by_name (priv->recognizer,
                         "updated",
                         tracker);

  g_object_unref (tracker);
}

/**
 * gtk_event_tracker_finish:
 * @tracker: The event tracker
 *
 * Marks the event tracker as finished and emits the 
 * GtkEventRecognizer::finished signal. If the @tracker has
 * already been finished, nothing happens.
 *
 * This function should only be called by #GtkEventRecognizer
 * implementations.
 **/
void
gtk_event_tracker_finish (GtkEventTracker *tracker)
{
  GtkEventTrackerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_TRACKER (tracker));

  priv = tracker->priv;

  if (priv->finished)
    return;

  priv->finished = TRUE;

  if (priv->started)
    g_signal_emit_by_name (priv->recognizer,
                           "finished",
                           tracker);

  /* release the reference from adding the tracker upon construction */
  g_object_unref (tracker);
}

void
_gtk_event_tracker_add (GtkEventTracker *tracker)
{
  g_queue_push_tail (&trackers, tracker);
}

gboolean
_gtk_event_trackers_invoke (GdkEvent *event)
{
  GList *list;
  gboolean eat_event = FALSE;

  g_return_val_if_fail (event != NULL, FALSE);

  list = g_queue_peek_head_link (&trackers); 
  while (list)
    {
      GtkEventTracker *tracker = list->data;

      g_object_ref (tracker);

      eat_event |= _gtk_event_recognizer_track (tracker->priv->recognizer,
                                                tracker,
                                                event);

      list = list->next;
      g_object_unref (tracker);
    }

  return eat_event;
}
