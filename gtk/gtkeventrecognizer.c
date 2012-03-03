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

#include "gtkeventrecognizer.h"
#include "gtkeventrecognizerprivate.h"

#include "gtkeventtracker.h"
#include "gtkeventtrackerprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkwidget.h"

/**
 * SECTION:gtkeventrecognizer
 * @Short_description: Recognizes gestures from events
 * @Title: GtkEventRecognizer
 * @See_also: #GtkEventTracker
 *
 * #GtkEventRecoginzer and its subclasses are used for defining the event
 * handling behavior of #GtkWidgets. 
 *
 * #GtkEventRecognizer was added in GTK 3.6.
 */

enum {
  STARTED,
  UPDATED,
  FINISHED,
  CANCELLED,
  /* add more */
  LAST_SIGNAL
};

guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE (GtkEventRecognizer, gtk_event_recognizer, G_TYPE_OBJECT)

static void
gtk_event_recognizer_default_recognize (GtkEventRecognizer *recognizer,
                                        GtkWidget          *widget,
                                        GdkEvent           *event)
{
}

static gboolean
gtk_event_recognizer_default_track (GtkEventRecognizer *recognizer,
                                    GtkEventTracker    *tracker,
                                    GdkEvent           *event)
{
  return FALSE;
}

static void
gtk_event_recognizer_class_init (GtkEventRecognizerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->tracker_type = GTK_TYPE_EVENT_TRACKER;

  klass->recognize = gtk_event_recognizer_default_recognize;
  klass->track = gtk_event_recognizer_default_track;

  /**
   * GtkEventRecognizer::started:
   * @recognizer: the recognizer
   * @tracker: the tracker that was started
   *
   * Signals that @tracker has started recognizing an event sequence. Widgets
   * using @recognizer may now wish to update transient state based on
   * @tracker.
   *
   * From now on, @recognizer will emit GtkEventRecognizer::updated signals
   * for @tracker until either the sequence got cancelled via a
   * GtkEventRecognizer::cancelled signal emission or successfully recognized
   * with a GtkEventRecognizer::finished signal.
   */
  signals[STARTED] =
    g_signal_new (I_("started"),
                  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEventRecognizerClass, started),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_EVENT_TRACKER);
  /**
   * GtkEventRecognizer::updated:
   * @recognizer: the recognizer
   * @tracker: the tracker that was updated
   *
   * Signals that @tracker has updated its internal state while recognizing 
   * an event sequence. Widgets using @recognizer may now wish to update 
   * transient state based on @tracker.
   */
  signals[UPDATED] =
    g_signal_new (I_("updated"),
                  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEventRecognizerClass, updated),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_EVENT_TRACKER);
  /**
   * GtkEventRecognizer::finished:
   * @recognizer: the recognizer
   * @tracker: the tracker that was finished
   *
   * Signals that @tracker has successfully recognized an event sequence and
   * will now stop processing events or change state. Widgets using @recognizer
   * should now update their state based on this event based on the information
   * provided by @tracker.
   *
   * This signal will only be emitted after GtkEventRecognizer::started has
   * been emitted for @tracker. It might not ever be emitted if @tracker was
   * cancelled and GtkEventRecognizer::cancelled has been emitted instead. After
   * this signal has been emitted, no new signals will be emitted for @tracker.
   */
  signals[FINISHED] =
    g_signal_new (I_("finished"),
                  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEventRecognizerClass, finished),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_EVENT_TRACKER);
  /**
   * GtkEventRecognizer::cancelled:
   * @recognizer: the recognizer
   * @tracker: the tracker that was cancelled
   *
   * Signals that @tracker has been cancelled. It will now stop tracking
   * events. A widget should now undo all modifications it did due prior signal
   * emissions.
   *
   * This signal will only be emitted after GtkEventRecognizer::started has
   * been emitted for @tracker. If it gets emitted, no other events will be emitted
   * for @tracker.
   */
  signals[CANCELLED] =
    g_signal_new (I_("cancelled"),
                  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEventRecognizerClass, cancelled),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_EVENT_TRACKER);
}

static void
gtk_event_recognizer_init (GtkEventRecognizer *recognizer)
{
}

gint
gtk_event_recognizer_class_get_event_mask (GtkEventRecognizerClass *klass)
{
  g_return_val_if_fail (GTK_IS_EVENT_RECOGNIZER_CLASS (klass), 0);

  return klass->event_mask;
}

void
gtk_event_recognizer_class_set_event_mask (GtkEventRecognizerClass *klass,
                                           gint                     event_mask)
{
  g_return_if_fail (GTK_IS_EVENT_RECOGNIZER_CLASS (klass));

  klass->event_mask = event_mask;
}

GType
gtk_event_recognizer_class_get_tracker_type (GtkEventRecognizerClass *klass)
{
  g_return_val_if_fail (GTK_IS_EVENT_RECOGNIZER_CLASS (klass), 0);

  return klass->tracker_type;
}

void
gtk_event_recognizer_class_set_tracker_type (GtkEventRecognizerClass *klass,
                                             GType                    tracker_type)
{
  g_return_if_fail (GTK_IS_EVENT_RECOGNIZER_CLASS (klass));
  g_return_if_fail (g_type_is_a (tracker_type, klass->tracker_type));

  klass->tracker_type = tracker_type;
}

void
gtk_event_recognizer_create_tracker (GtkEventRecognizer *recognizer,
                                     GtkWidget          *widget,
                                     GdkEvent           *event)
{
  GtkEventTracker *tracker;

  g_return_if_fail (GTK_IS_EVENT_RECOGNIZER (recognizer));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (event != NULL);

  tracker = g_object_new (GTK_EVENT_RECOGNIZER_GET_CLASS (recognizer)->tracker_type,
                          "recognizer", recognizer,
                          "widget", widget,
                          NULL);
  
  _gtk_event_tracker_add (tracker);
  _gtk_event_recognizer_track (recognizer,
                               tracker,
                               event);
}

void
_gtk_event_recognizer_recognize (GtkEventRecognizer *recognizer,
                                 GtkWidget          *widget,
                                 GdkEvent           *event)
{
  GtkEventRecognizerClass *klass;

  g_return_if_fail (GTK_IS_EVENT_RECOGNIZER (recognizer));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (event != NULL);

  klass = GTK_EVENT_RECOGNIZER_GET_CLASS (recognizer);

  klass->recognize (recognizer, widget, event);
}

gboolean
_gtk_event_recognizer_track (GtkEventRecognizer *recognizer,
                             GtkEventTracker    *tracker,
                             GdkEvent           *event)
{
  GtkEventRecognizerClass *klass;

  g_return_val_if_fail (GTK_IS_EVENT_RECOGNIZER (recognizer), FALSE);
  g_return_val_if_fail (GTK_IS_EVENT_TRACKER (tracker), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gtk_event_tracker_is_finished (tracker))
    return FALSE;

  klass = GTK_EVENT_RECOGNIZER_GET_CLASS (recognizer);
  return klass->track (recognizer, tracker, event);
}

