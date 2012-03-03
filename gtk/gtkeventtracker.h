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

#ifndef __GTK_EVENT_TRACKER_H__
#define __GTK_EVENT_TRACKER_H__

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_TRACKER           (gtk_event_tracker_get_type ())
#define GTK_EVENT_TRACKER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_EVENT_TRACKER, GtkEventTracker))
#define GTK_EVENT_TRACKER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_EVENT_TRACKER, GtkEventTrackerClass))
#define GTK_IS_EVENT_TRACKER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_EVENT_TRACKER))
#define GTK_IS_EVENT_TRACKER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_EVENT_TRACKER))
#define GTK_EVENT_TRACKER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_EVENT_TRACKER, GtkEventTrackerClass))

typedef struct _GtkEventTrackerClass      GtkEventTrackerClass;
typedef struct _GtkEventTrackerPrivate    GtkEventTrackerPrivate;

struct _GtkEventTracker
{
  GObject parent;

  GtkEventTrackerPrivate *priv;
};

struct _GtkEventTrackerClass
{
  GObjectClass parent_class;

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

GType                 gtk_event_tracker_get_type          (void) G_GNUC_CONST;

GtkEventRecognizer *  gtk_event_tracker_get_recognizer    (GtkEventTracker      *tracker);
GtkWidget *           gtk_event_tracker_get_widget        (GtkEventTracker      *tracker);

gboolean              gtk_event_tracker_is_started        (GtkEventTracker      *tracker);
gboolean              gtk_event_tracker_is_finished       (GtkEventTracker      *tracker);
gboolean              gtk_event_tracker_is_cancelled      (GtkEventTracker      *tracker);

void                  gtk_event_tracker_cancel            (GtkEventTracker      *tracker);

void                  gtk_event_tracker_start             (GtkEventTracker      *tracker);
void                  gtk_event_tracker_update            (GtkEventTracker      *tracker);
void                  gtk_event_tracker_finish            (GtkEventTracker      *tracker);

G_END_DECLS

#endif /* __GTK_EVENT_TRACKER_H__ */
