/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdio.h>
#include "gtk/gtk.h"
#include <math.h>

/* Backing surface for drawing area */

static cairo_surface_t *surface = NULL;

/* Create a new backing surface of the appropriate size */
static void
size_allocate (GtkWidget     *widget,
               GtkAllocation *allocation,
               int            baseline,
               gpointer       data)
{
  if (surface)
    {
      cairo_surface_destroy (surface);
      surface = NULL;
    }

  if (gtk_widget_get_surface (widget))
    {
      cairo_t *cr;

      surface = gdk_surface_create_similar_surface (gtk_widget_get_surface (widget),
                                                   CAIRO_CONTENT_COLOR,
                                                   gtk_widget_get_width (widget),
                                                   gtk_widget_get_height (widget));
      cr = cairo_create (surface);

      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_paint (cr);

      cairo_destroy (cr);
    }
}

/* Refill the screen from the backing surface */
static void
draw (GtkDrawingArea *drawing_area,
      cairo_t        *cr,
      int             width,
      int             height,
      gpointer        data)
{
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
}

/* Draw a rectangle on the screen, size depending on pressure,
   and color on the type of device */
static void
draw_brush (GtkWidget *widget, GdkInputSource source,
	    gdouble x, gdouble y, gdouble pressure)
{
  GdkRGBA color;
  GdkRectangle update_rect;
  cairo_t *cr;

  color.alpha = 1.0;

  switch (source)
    {
    case GDK_SOURCE_MOUSE:
      color.red = color.green = 0.0;
      color.blue = 1.0;
      break;
    case GDK_SOURCE_PEN:
      color.red = color.green = color.blue = 0.0;
      break;
    case GDK_SOURCE_ERASER:
      color.red = color.green = color.blue = 1.0;
      break;
    default:
      color.red = color.blue = 0.0;
      color.green = 1.0;
    }

  update_rect.x = x - 10 * pressure;
  update_rect.y = y - 10 * pressure;
  update_rect.width = 20 * pressure;
  update_rect.height = 20 * pressure;

  cr = cairo_create (surface);
  gdk_cairo_set_source_rgba (cr, &color);
  gdk_cairo_rectangle (cr, &update_rect);
  cairo_fill (cr);
  cairo_destroy (cr);

  gtk_widget_queue_draw (widget);
}

static guint32 motion_time;

static const char *
device_source_name (GdkDevice *device)
{
  static const struct {GdkInputSource source; const char *name;} sources[] =
    {
      {GDK_SOURCE_MOUSE,       "mouse"},
      {GDK_SOURCE_PEN,         "pen"},
      {GDK_SOURCE_ERASER,      "eraser"},
      {GDK_SOURCE_CURSOR,      "cursor"},
      {GDK_SOURCE_KEYBOARD,    "keyboard"},
      {GDK_SOURCE_TOUCHSCREEN, "touchscreen"},
      {GDK_SOURCE_TOUCHPAD,    "touchpad"},
      {GDK_SOURCE_TRACKPOINT,  "trackpoint"},
      {GDK_SOURCE_TABLET_PAD,  "tablet pad"},
    };
  GdkInputSource source = gdk_device_get_source (device);
  int s;

  for (s = 0; s < G_N_ELEMENTS (sources); ++s)
    if (sources[s].source == source)
      return sources[s].name;

  return "unknown";
}

static void
print_axes (GdkEvent *event)
{
  gdouble *axes;
  guint n_axes;

  gdk_event_get_axes (event, &axes, &n_axes);
  
  if (axes)
    {
      GdkDevice *device = gdk_event_get_device (event);
      GdkDevice *source = gdk_event_get_source_device (event);
      int i;

      g_print ("%s (%s) via %s (%s): ",
               gdk_device_get_name (device),
               device_source_name (device),
               gdk_device_get_name (source),
               device_source_name (source));

      for (i = 0; i < gdk_device_get_n_axes (device); i++)
	g_print ("%g ", axes[i]);

      g_print ("\n");
    }
}

static void
drag_begin (GtkGesture *gesture,
            double      x,
            double      y,
            GtkWidget  *widget)
{
  if (surface != NULL)
    {
      gdouble pressure = 0.5;
      GdkDevice *device;
      gdouble x, y;
      GdkEvent *event;

      event = gtk_get_current_event ();
      device = gdk_event_get_device (event);
      gdk_event_get_coords (event, &x, &y);

      print_axes (event);
      gdk_event_get_axis (event, GDK_AXIS_PRESSURE, &pressure);
      draw_brush (widget, gdk_device_get_source (device), x, y, pressure);

      motion_time = gdk_event_get_time ((GdkEvent *)event);
    }
}

static gint
key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  guint keyval;

  gdk_event_get_keyval ((GdkEvent*)event, &keyval);

  if ((keyval >= 0x20) && (keyval <= 0xFF))
    printf("I got a %c\n", keyval);
  else
    printf("I got some other key\n");

  return TRUE;
}

static void
drag_update (GtkGesture *gesture,
             double      x,
             double      y,
             GtkWidget  *widget)
{
  GdkModifierType state;
  GdkDevice *device;
  GdkEvent *event;
  double start_x, start_y;

  event = gtk_get_current_event ();
  gdk_event_get_state (event, &state);
  device = gdk_event_get_device (event);

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture), &start_x, &start_y);

  if (state & GDK_BUTTON1_MASK && surface != NULL)
    {
      double pressure = 0.5;

      gdk_event_get_axis (event, GDK_AXIS_PRESSURE, &pressure);

      draw_brush (widget, gdk_device_get_source (device), start_x + x, start_y + y, pressure);

      motion_time = gdk_event_get_time (event);
    }

  print_axes (event);
}

void
quit (void)
{
  gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *drawing_area;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkGesture *gesture;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "Test Input");

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  g_signal_connect (window, "destroy",
		    G_CALLBACK (quit), NULL);

  /* Create the drawing area */

  drawing_area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (drawing_area, 200, 200);
  gtk_widget_set_vexpand (drawing_area, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area);

  gtk_widget_show (drawing_area);

  /* Signals used to handle backing surface */

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area), draw, NULL, NULL);
  g_signal_connect (drawing_area, "size-allocate", G_CALLBACK (size_allocate), NULL);

  gesture = gtk_gesture_drag_new ();
  g_object_set_data_full (G_OBJECT (drawing_area), "gesture",
                          gesture, g_object_unref);
  g_signal_connect (gesture, "drag-begin",
                    G_CALLBACK (drag_begin), drawing_area);
  g_signal_connect (gesture, "drag-update",
                    G_CALLBACK (drag_update), drawing_area);
  gtk_widget_add_controller (drawing_area, GTK_EVENT_CONTROLLER (gesture));

  g_signal_connect (drawing_area, "key_press_event",
		    G_CALLBACK (key_press_event), NULL);

  gtk_widget_set_can_focus (drawing_area, TRUE);
  gtk_widget_grab_focus (drawing_area);

  /* .. And create some buttons */
  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    window);
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
