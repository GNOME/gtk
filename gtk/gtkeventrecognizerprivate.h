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

#ifndef __GTK_EVENT_RECOGNIZER_PRIVATE_H__
#define __GTK_EVENT_RECOGNIZER_PRIVATE_H__

#include <gdk/gdk.h>

#include <gtk/gtktypes.h>

void                _gtk_event_recognizer_recognize       (GtkEventRecognizer *recognizer,
                                                           GtkWidget          *widget,
                                                           GdkEvent           *event);
gboolean            _gtk_event_recognizer_track           (GtkEventRecognizer *recognizer,
                                                           GtkEventTracker    *tracker,
                                                           GdkEvent           *event);

#endif /* __GTK_EVENT_RECOGNIZER_PRIVATE_H__ */
