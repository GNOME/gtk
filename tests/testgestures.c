/*
 * Copyright (C) 2012 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>

typedef struct _SwipeData SwipeData;
struct _SwipeData {
  double x_offset;
  double y_offset;
  double progress;
  guint color;
};

static void
update_swipe (GtkEventRecognizer *recognizer,
              GtkEventTracker    *tracker,
              SwipeData          *data)
{
  GtkSwipeGesture *swipe = GTK_SWIPE_GESTURE (tracker);
  GtkWidget *widget = gtk_event_tracker_get_widget (tracker);
  double x, y;

  gtk_swipe_gesture_get_offset (swipe, &x, &y);
  x /= gtk_widget_get_allocated_width (widget);
  y /= gtk_widget_get_allocated_height (widget);
  data->x_offset = CLAMP (x, -1, 1);
  data->y_offset = CLAMP (y, -1, 1);

  gtk_widget_queue_draw (widget);
}

static void
finish_swipe (GtkEventRecognizer *recognizer,
              GtkEventTracker    *tracker,
              SwipeData          *data)
{
  GtkSwipeGesture *swipe = GTK_SWIPE_GESTURE (tracker);
  GtkWidget *widget = gtk_event_tracker_get_widget (tracker);
  double x, y;

  gtk_swipe_gesture_get_offset (swipe, &x, &y);
  x /= gtk_widget_get_allocated_width (widget);
  y /= gtk_widget_get_allocated_height (widget);

  if (ABS (x) >= 0.5)
    data->color ^= 1;
  if (ABS (y) >= 0.5)
    data->color ^= 2;

  data->x_offset = 0;
  data->y_offset = 0;

  gtk_widget_queue_draw (widget);
}

static void
cancel_swipe (GtkEventRecognizer *recognizer,
              GtkEventTracker    *tracker,
              SwipeData          *data)
{
  GtkWidget *widget = gtk_event_tracker_get_widget (tracker);

  data->x_offset = 0;
  data->y_offset = 0;

  gtk_widget_queue_draw (widget);
}


static gboolean
draw_swipe (GtkWidget *widget,
            cairo_t   *cr,
            SwipeData *data)
{
  static const GdkRGBA colors[4] = {
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
    { 1, 1, 0, 1 },
  };
  int i, x, y, w, h;

  w = gtk_widget_get_allocated_width (widget);
  h = gtk_widget_get_allocated_height (widget);
  x = data->x_offset * w;
  y = data->y_offset * h;

  cairo_translate (cr, x, y);

  for (i = 0; i < 4; i++)
    {
      gdk_cairo_set_source_rgba (cr, &colors[data->color ^ i]);
      cairo_rectangle (cr,
                       i % 2 ? (x > 0 ? -w : w) : 0, 
                       i / 2 ? (y > 0 ? -h : h) : 0,
                       w, h);
      cairo_fill (cr);
    }

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkEventRecognizer *recognizer;
  GtkWidgetClass *area_class;
  GtkWidget *window, *area;
  SwipeData swipe_data = { 0, };

  gtk_init (&argc, &argv);

  area_class = g_type_class_ref (GTK_TYPE_DRAWING_AREA);

  recognizer = gtk_swipe_recognizer_new ();
  g_signal_connect (recognizer, "started", G_CALLBACK (update_swipe), &swipe_data);
  g_signal_connect (recognizer, "updated", G_CALLBACK (update_swipe), &swipe_data);
  g_signal_connect (recognizer, "finished", G_CALLBACK (finish_swipe), &swipe_data);
  g_signal_connect (recognizer, "cancelled", G_CALLBACK (cancel_swipe), &swipe_data);

  /* This next line is a hack and you should never do it in real life.
   * Instead, do a subclass. Or get me to add gtk_widget_add_recognizer(). */
  gtk_widget_class_add_recognizer (area_class, recognizer);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  
  area = gtk_drawing_area_new ();
  gtk_widget_set_has_window (area, TRUE);
  gtk_widget_set_size_request (area, 400, 300);
  g_signal_connect_after (area, "draw", G_CALLBACK (draw_swipe), &swipe_data);
  gtk_container_add (GTK_CONTAINER (window), area);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
