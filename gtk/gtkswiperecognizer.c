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

#include "gtkswiperecognizer.h"

#include "gtkswipegesture.h"

/**
 * SECTION:gtkswiperecognizer
 * @Short_description: Recognizes vertical and horizontal swipe gestures
 * @Title: GtkSwipeRecognizer
 * @See_also: #GtkSwipeGesture
 *
 * #GtkSwipeRecoginzer recognizes vertical and horizontal swipe gestures.
 *
 * #GtkSwipeRecognizer was added in GTK 3.6.
 */

G_DEFINE_TYPE (GtkSwipeRecognizer, gtk_swipe_recognizer, GTK_TYPE_EVENT_RECOGNIZER)

static void
gtk_swipe_recognizer_recognize (GtkEventRecognizer *recognizer,
                                GtkWidget          *widget,
                                GdkEvent           *event)
{
  if (event->type == GDK_TOUCH_BEGIN)
    gtk_event_recognizer_create_tracker (recognizer, widget, event);
}

static gboolean
gtk_swipe_recognizer_track (GtkEventRecognizer *recognizer,
                            GtkEventTracker    *tracker,
                            GdkEvent           *event)
{
  GtkSwipeGesture *gesture = GTK_SWIPE_GESTURE (tracker);

  switch (event->type)
    {
    case GDK_TOUCH_BEGIN:
      return _gtk_swipe_gesture_begin (gesture, event);
    case GDK_TOUCH_END:
      return _gtk_swipe_gesture_end (gesture, event);
    case GDK_TOUCH_UPDATE:
      return _gtk_swipe_gesture_update (gesture, event);
    case GDK_TOUCH_CANCEL:
      return _gtk_swipe_gesture_cancel (gesture, event);
    default:
      return FALSE;
    }
}

static void
gtk_swipe_recognizer_class_init (GtkSwipeRecognizerClass *klass)
{
  GtkEventRecognizerClass *recognizer_class = GTK_EVENT_RECOGNIZER_CLASS (klass);

  recognizer_class->recognize = gtk_swipe_recognizer_recognize;
  recognizer_class->track = gtk_swipe_recognizer_track;

  gtk_event_recognizer_class_set_event_mask (recognizer_class,
                                             GDK_TOUCH_MASK);
  gtk_event_recognizer_class_set_tracker_type (recognizer_class,
                                               GTK_TYPE_SWIPE_GESTURE);
}

static void
gtk_swipe_recognizer_init (GtkSwipeRecognizer *recognizer)
{
}

GtkEventRecognizer *
gtk_swipe_recognizer_new (void)
{
  return g_object_new (GTK_TYPE_SWIPE_RECOGNIZER, NULL);
}

