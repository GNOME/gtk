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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>

#include <gtk/gtk.h>

static void
oval_path (cairo_t *cr,
           double xc, double yc,
           double xr, double yr)
{
  cairo_matrix_t *matrix;

  matrix = cairo_matrix_create ();
  cairo_current_matrix (cr, matrix);

  cairo_translate (cr, xc, yc);
  cairo_scale (cr, 1.0, yr / xr);
  cairo_move_to (cr, xr, 0.0);
  cairo_arc (cr,
	     0, 0,
	     xr,
	     0, 2 * G_PI);
  cairo_close_path (cr);

  cairo_set_matrix (cr, matrix);
  cairo_matrix_destroy (matrix);
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
  cairo_set_rgb_color (cr, 0.4, 0.4, 0.4);
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

  cairo_set_rgb_color (cr, 0.7, 0.7, 0.7);
  cairo_fill (cr);
}

/* Draw a red, green, and blue circle equally spaced inside
 * the larger circle of radius r at (xc, yc)
 */
static void
draw_3circles (cairo_t *cr,
               double xc, double yc,
               double radius)
{
  double subradius = radius * (2 / 3. - 0.1);
    
  cairo_set_rgb_color (cr, 1., 0., 0.);
  oval_path (cr,
	     xc + radius / 3. * cos (G_PI * (0.5)),
	     yc - radius / 3. * sin (G_PI * (0.5)),
	     subradius, subradius);
  cairo_fill (cr);
    
  cairo_set_rgb_color (cr, 0., 1., 0.);
  oval_path (cr,
	     xc + radius / 3. * cos (G_PI * (0.5 + 2/.3)),
	     yc - radius / 3. * sin (G_PI * (0.5 + 2/.3)),
	     subradius, subradius);
  cairo_fill (cr);
    
  cairo_set_rgb_color (cr, 0., 0., 1.);
  oval_path (cr,
	     xc + radius / 3. * cos (G_PI * (0.5 + 4/.3)),
	     yc - radius / 3. * sin (G_PI * (0.5 + 4/.3)),
	     subradius, subradius);
  cairo_fill (cr);
}

static void
draw (cairo_t *cr,
      int      width,
      int      height)
{
  cairo_surface_t *overlay, *punch, *circles;

  /* Fill the background */
  double radius = 0.5 * (width < height ? width : height) - 10;
  double xc = width / 2.;
  double yc = height / 2.;

  overlay = cairo_surface_create_similar (cairo_current_target_surface (cr),
					  CAIRO_FORMAT_ARGB32,
					  width, height);
  if (overlay == NULL)
    return;

  punch = cairo_surface_create_similar (cairo_current_target_surface (cr),
					CAIRO_FORMAT_A8,
					width, height);
  if (punch == NULL)
    return;

  circles = cairo_surface_create_similar (cairo_current_target_surface (cr),
					  CAIRO_FORMAT_ARGB32,
					  width, height);
  if (circles == NULL)
    return;
    
  fill_checks (cr, 0, 0, width, height);

  cairo_save (cr);
  cairo_set_target_surface (cr, overlay);
  cairo_identity_matrix (cr);

  /* Draw a black circle on the overlay
   */
  cairo_set_rgb_color (cr, 0., 0., 0.);
  oval_path (cr, xc, yc, radius, radius);
  cairo_fill (cr);

  cairo_save (cr);
  cairo_set_target_surface (cr, punch);

  /* Draw 3 circles to the punch surface, then cut
   * that out of the main circle in the overlay
   */
  draw_3circles (cr, xc, yc, radius);

  cairo_restore (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OUT_REVERSE);
  cairo_show_surface (cr, punch, width, height);

  /* Now draw the 3 circles in a subgroup again
   * at half intensity, and use OperatorAdd to join up
   * without seams.
   */
  cairo_save (cr);
  cairo_set_target_surface (cr, circles);

  cairo_set_alpha (cr, 0.5);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  draw_3circles (cr, xc, yc, radius);

  cairo_restore (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  cairo_show_surface (cr, circles, width, height);

  cairo_restore (cr);

  cairo_show_surface (cr, overlay, width, height);

  cairo_surface_destroy (overlay);
  cairo_surface_destroy (punch);
  cairo_surface_destroy (circles);
}

static gboolean
on_expose_event (GtkWidget      *widget,
		 GdkEventExpose *event,
		 gpointer        data)
{
  cairo_t *cr;

  cr = cairo_create ();
  gdk_drawable_set_cairo_target (GDK_DRAWABLE (widget->window), cr);

  draw (cr, widget->allocation.width, widget->allocation.height);

  cairo_destroy (cr);

  return FALSE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *darea;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_window_set_title (GTK_WINDOW (window), "cairo: Knockout Groups");

  darea = gtk_drawing_area_new ();
  gtk_container_add (GTK_CONTAINER (window), darea);

  g_signal_connect (darea, "expose-event",
		    G_CALLBACK (on_expose_event), NULL);
  g_signal_connect (window, "destroy-event",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);
  
  gtk_main ();

  return 0;
}
