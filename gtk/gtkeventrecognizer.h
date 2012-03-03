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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_EVENT_RECOGNIZER_H__
#define __GTK_EVENT_RECOGNIZER_H__

#include <gdk/gdk.h>

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_RECOGNIZER           (gtk_event_recognizer_get_type ())
#define GTK_EVENT_RECOGNIZER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_EVENT_RECOGNIZER, GtkEventRecognizer))
#define GTK_EVENT_RECOGNIZER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_EVENT_RECOGNIZER, GtkEventRecognizerClass))
#define GTK_IS_EVENT_RECOGNIZER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_EVENT_RECOGNIZER))
#define GTK_IS_EVENT_RECOGNIZER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_EVENT_RECOGNIZER))
#define GTK_EVENT_RECOGNIZER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_EVENT_RECOGNIZER, GtkEventRecognizerClass))

typedef struct _GtkEventRecognizerClass        GtkEventRecognizerClass;
typedef struct _GtkEventRecognizerPrivate      GtkEventRecognizerPrivate;
typedef struct _GtkEventRecognizerClassPrivate GtkEventRecognizerClassPrivate;

struct _GtkEventRecognizer
{
  GObject parent;

  GtkEventRecognizerPrivate *priv;
};

struct _GtkEventRecognizerClass
{
  GObjectClass parent_class;

  /* XXX: Put into private struct */
  gint                event_mask;
  GType               tracker_type;

  void                (* recognize)                       (GtkEventRecognizer *recognizer,
                                                           GtkWidget          *widget,
                                                           GdkEvent           *event);
  gboolean            (* track)                           (GtkEventRecognizer *recognizer,
                                                           GtkEventTracker    *tracker,
                                                           GdkEvent           *event);

  /* signals */
  void                (* started)                         (GtkEventRecognizer *recognizer,
                                                           GtkEventTracker    *tracker);
  void                (* updated)                         (GtkEventRecognizer *recognizer,
                                                           GtkEventTracker    *tracker);
  void                (* cancelled)                       (GtkEventRecognizer *recognizer,
                                                           GtkEventTracker    *tracker);
  void                (* finished)                        (GtkEventRecognizer *recognizer,
                                                           GtkEventTracker    *tracker);

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
};

GType                 gtk_event_recognizer_get_type               (void) G_GNUC_CONST;

gint                  gtk_event_recognizer_class_get_event_mask   (GtkEventRecognizerClass *klass);
void                  gtk_event_recognizer_class_set_event_mask   (GtkEventRecognizerClass *klass,
                                                                   gint                     event_mask);
GType                 gtk_event_recognizer_class_get_tracker_type (GtkEventRecognizerClass *klass);
void                  gtk_event_recognizer_class_set_tracker_type (GtkEventRecognizerClass *klass,
                                                                   GType                    tracker_type);

void                  gtk_event_recognizer_create_tracker         (GtkEventRecognizer      *recognizer,
                                                                   GtkWidget               *widget,
                                                                   GdkEvent                *event);


G_END_DECLS

#endif /* __GTK_EVENT_RECOGNIZER_H__ */
