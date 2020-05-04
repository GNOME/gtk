/* testimage.c
 * Copyright (C) 2005  Red Hat, Inc.
 * Based on cairo-demo/X11/cairo-knockout.c
 *
 * Author: Owen Taylor
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>

#include <gtk/gtk.h>

static void
oval_path (cairo_t *cr,
           double xc, double yc,
           double xr, double yr)
{
  cairo_save (cr);

  cairo_translate (cr, xc, yc);
  cairo_scale (cr, 1.0, yr / xr);
  cairo_move_to (cr, xr, 0.0);
  cairo_arc (cr,
	     0, 0,
	     xr,
	     0, 2 * G_PI);
  cairo_close_path (cr);

  cairo_restore (cr);
}

/* Create a path that is a circular oval with radii xr, yr at xc,
 * yc.
 */
/* Fill the given area with checks in the standard style
 * for showing compositing effects.
 *
 * It would make sense to do this as a repeating surface,
 * but most implementations of RENDER currently have broken
 * implementations of repeat + transform, even when the
 * transform is a translation.
 */
static void
fill_checks (cairo_t *cr,
             int x,     int y,
             int width, int height)
{
  int i, j;
  
#define CHECK_SIZE 32

  cairo_rectangle (cr, x, y, width, height);
  cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
  cairo_fill (cr);

  /* Only works for CHECK_SIZE a power of 2 */
  j = x & (-CHECK_SIZE);
  
  for (; j < height; j += CHECK_SIZE)
    {
      i = y & (-CHECK_SIZE);
      for (; i < width; i += CHECK_SIZE)
	if ((i / CHECK_SIZE + j / CHECK_SIZE) % 2 == 0)
	  cairo_rectangle (cr, i, j, CHECK_SIZE, CHECK_SIZE);
    }

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_fill (cr);
}

/* Draw a red, green, and blue circle equally spaced inside
 * the larger circle of radius r at (xc, yc)
 */
static void
draw_3circles (cairo_t *cr,
               double xc, double yc,
               double radius,
	       double alpha)
{
  double subradius = radius * (2 / 3. - 0.1);
    
  cairo_set_source_rgba (cr, 1., 0., 0., alpha);
  oval_path (cr,
	     xc + radius / 3. * cos (G_PI * (0.5)),
	     yc - radius / 3. * sin (G_PI * (0.5)),
	     subradius, subradius);
  cairo_fill (cr);
    
  cairo_set_source_rgba (cr, 0., 1., 0., alpha);
  oval_path (cr,
	     xc + radius / 3. * cos (G_PI * (0.5 + 2/.3)),
	     yc - radius / 3. * sin (G_PI * (0.5 + 2/.3)),
	     subradius, subradius);
  cairo_fill (cr);
    
  cairo_set_source_rgba (cr, 0., 0., 1., alpha);
  oval_path (cr,
	     xc + radius / 3. * cos (G_PI * (0.5 + 4/.3)),
	     yc - radius / 3. * sin (G_PI * (0.5 + 4/.3)),
	     subradius, subradius);
  cairo_fill (cr);
}

static void
on_draw (GtkDrawingArea *darea,
         cairo_t        *cr,
         int             width,
         int             height,
         gpointer        data)
{
  cairo_surface_t *overlay, *punch, *circles;
  cairo_t *overlay_cr, *punch_cr, *circles_cr;

  /* Fill the background */
  double radius = 0.5 * (width < height ? width : height) - 10;
  double xc = width / 2.;
  double yc = height / 2.;

  overlay = cairo_surface_create_similar (cairo_get_target (cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  width, height);

  punch = cairo_surface_create_similar (cairo_get_target (cr),
					CAIRO_CONTENT_ALPHA,
					width, height);

  circles = cairo_surface_create_similar (cairo_get_target (cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  width, height);
    
  fill_checks (cr, 0, 0, width, height);

  /* Draw a black circle on the overlay
   */
  overlay_cr = cairo_create (overlay);
  cairo_set_source_rgb (overlay_cr, 0., 0., 0.);
  oval_path (overlay_cr, xc, yc, radius, radius);
  cairo_fill (overlay_cr);

  /* Draw 3 circles to the punch surface, then cut
   * that out of the main circle in the overlay
   */
  punch_cr = cairo_create (punch);
  draw_3circles (punch_cr, xc, yc, radius, 1.0);
  cairo_destroy (punch_cr);

  cairo_set_operator (overlay_cr, CAIRO_OPERATOR_DEST_OUT);
  cairo_set_source_surface (overlay_cr, punch, 0, 0);
  cairo_paint (overlay_cr);

  /* Now draw the 3 circles in a subgroup again
   * at half intensity, and use OperatorAdd to join up
   * without seams.
   */
  circles_cr = cairo_create (circles);
  
  cairo_set_operator (circles_cr, CAIRO_OPERATOR_OVER);
  draw_3circles (circles_cr, xc, yc, radius, 0.5);
  cairo_destroy (circles_cr);

  cairo_set_operator (overlay_cr, CAIRO_OPERATOR_ADD);
  cairo_set_source_surface (overlay_cr, circles, 0, 0);
  cairo_paint (overlay_cr);

  cairo_destroy (overlay_cr);

  cairo_set_source_surface (cr, overlay, 0, 0);
  cairo_paint (cr);

  cairo_surface_destroy (overlay);
  cairo_surface_destroy (punch);
  cairo_surface_destroy (circles);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *darea;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_window_set_title (GTK_WINDOW (window), "cairo: Knockout Groups");

  darea = gtk_drawing_area_new ();
  gtk_window_set_child (GTK_WINDOW (window), darea);

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (darea), on_draw, NULL, NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  gtk_widget_show (window);
  
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
