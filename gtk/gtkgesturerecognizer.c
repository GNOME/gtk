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

#include "gtkgesturerecognizer.h"

#include "gtkgesture.h"

/**
 * SECTION:gtkgesturerecognizer
 * @Short_description: Recognizes gestures
 * @Title: GtkGestureRecognizer
 * @See_also: #GtkGesture, #GtkEventRecognizer
 *
 * FIXME
 *
 * #GtkGestureRecognizer was added in GTK 3.6.
 */

G_DEFINE_ABSTRACT_TYPE (GtkGestureRecognizer, gtk_gesture_recognizer, GTK_TYPE_EVENT_RECOGNIZER)

static void
gtk_gesture_recognizer_finished (GtkEventRecognizer *recognizer,
                                 GtkEventTracker    *tracker)
{
  gtk_gesture_accept (GTK_GESTURE (tracker));
}

static void
gtk_gesture_recognizer_class_init (GtkGestureRecognizerClass *klass)
{
  GtkEventRecognizerClass *recognizer_class = GTK_EVENT_RECOGNIZER_CLASS (klass);

  recognizer_class->finished = gtk_gesture_recognizer_finished;

  gtk_event_recognizer_class_set_tracker_type (recognizer_class, GTK_TYPE_GESTURE);
}

static void
gtk_gesture_recognizer_init (GtkGestureRecognizer *recognizer)
{
}
