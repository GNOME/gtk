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
static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
  GtkAllocation allocation;
  cairo_t *cr;

  if (surface)
    cairo_surface_destroy (surface);

  gtk_widget_get_allocation (widget, &allocation);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               allocation.width,
                                               allocation.height);
  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_destroy (cr);

  return TRUE;
}

/* Refill the screen from the backing surface */
static gboolean
draw (GtkWidget *widget, cairo_t *cr)
{
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
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

  gtk_widget_queue_draw_area (widget,
			      update_rect.x, update_rect.y,
			      update_rect.width, update_rect.height);
}

static guint32 motion_time;

static void
print_axes (GdkDevice *device, gdouble *axes)
{
  int i;
  
  if (axes)
    {
      g_print ("%s ", gdk_device_get_name (device));

      for (i = 0; i < gdk_device_get_n_axes (device); i++)
	g_print ("%g ", axes[i]);

      g_print ("\n");
    }
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  guint button;

  gdk_event_get_button ((GdkEvent *)event, &button);

  if (button == GDK_BUTTON_PRIMARY && surface != NULL)
    {
      gdouble pressure = 0.5;
      GdkDevice *device;
      gdouble *axes;
      guint n_axes;
      gdouble x, y;

      device = gdk_event_get_device ((GdkEvent *)event);
      gdk_event_get_axes ((GdkEvent *)event, &axes, &n_axes);
      gdk_event_get_coords ((GdkEvent *)event, &x, &y);

      print_axes (device, axes);
      gdk_event_get_axis ((GdkEvent *)event, GDK_AXIS_PRESSURE, &pressure);
      draw_brush (widget, gdk_device_get_source (device), x, y, pressure);

      motion_time = gdk_event_get_time ((GdkEvent *)event);
    }

  return TRUE;
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

static gint
motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  GdkTimeCoord **events;
  gint n_events;
  int i;
  GdkModifierType state;
  GdkDevice *device;
  gdouble *axes;
  guint n_axes;

  gdk_event_get_state ((GdkEvent *)event, &state);
  device = gdk_event_get_device ((GdkEvent *)event);

  if (state & GDK_BUTTON1_MASK && surface != NULL)
    {
      if (gdk_device_get_history (device,
                                  gdk_event_get_window ((GdkEvent *)event),
				  motion_time,
                                  gdk_event_get_time ((GdkEvent *)event),
				  &events, &n_events))
	{
	  for (i=0; i<n_events; i++)
	    {
	      double x = 0, y = 0, pressure = 0.5;

	      gdk_device_get_axis (device, events[i]->axes, GDK_AXIS_X, &x);
	      gdk_device_get_axis (device, events[i]->axes, GDK_AXIS_Y, &y);
	      gdk_device_get_axis (device, events[i]->axes, GDK_AXIS_PRESSURE, &pressure);
	      draw_brush (widget, gdk_device_get_source (device), x, y, pressure);

	      print_axes (device, events[i]->axes);
	    }
	  gdk_device_free_history (events, n_events);
	}
      else
	{
	  double pressure = 0.5;
          gdouble x, y;

	  gdk_event_get_axis ((GdkEvent *)event, GDK_AXIS_PRESSURE, &pressure);

          gdk_event_get_coords ((GdkEvent *)event, &x, &y);
	  draw_brush (widget, gdk_device_get_source (device), x, y, pressure);
	}
      motion_time = gdk_event_get_time ((GdkEvent *)event);
    }

  gdk_event_get_axes ((GdkEvent *)event, &axes, &n_axes);
  print_axes (device, axes);

  return TRUE;
}

/* We track the next two events to know when we need to draw a
   cursor */

static gint
proximity_out_event (GtkWidget *widget, GdkEventProximity *event)
{
  return TRUE;
}

static gint
leave_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
  return TRUE;
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
  GdkWindow *gdk_win;

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

  g_signal_connect (drawing_area, "draw",
		    G_CALLBACK (draw), NULL);
  g_signal_connect (drawing_area, "configure_event",
		    G_CALLBACK (configure_event), NULL);

  /* Event signals */

  g_signal_connect (drawing_area, "motion_notify_event",
		    G_CALLBACK (motion_notify_event), NULL);
  g_signal_connect (drawing_area, "button_press_event",
		    G_CALLBACK (button_press_event), NULL);
  g_signal_connect (drawing_area, "key_press_event",
		    G_CALLBACK (key_press_event), NULL);

  g_signal_connect (drawing_area, "leave_notify_event",
		    G_CALLBACK (leave_notify_event), NULL);
  g_signal_connect (drawing_area, "proximity_out_event",
		    G_CALLBACK (proximity_out_event), NULL);

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

  /* request all motion events */
  gdk_win = gtk_widget_get_window (drawing_area);
  gdk_window_set_event_compression (gdk_win, FALSE);

  gtk_main ();

  return 0;
}
