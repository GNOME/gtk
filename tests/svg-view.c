/* svg-view.c
 * Copyright (C) 2025  Red Hat, Inc
 * Author: Matthias Clasen
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
#include "config.h"
#include <gtk/gtk.h>


/* Show an SVG animation using the SVG renderer.
 * Left/right click change states.
 */


static void
clicked (GtkGestureClick *click,
         int              n_press,
         double           x,
         double           y,
         GtkSvg          *svg)
{
  unsigned int state;
  unsigned int n_states;

  state = gtk_svg_get_state (svg);
  n_states = gtk_svg_get_n_states (svg);

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click)) == 1)
    {
      if (state + 1 == n_states)
        state = GTK_SVG_STATE_EMPTY;
      else if (state == GTK_SVG_STATE_EMPTY)
        state = 0;
      else
        state++;
    }
  else
    {
      if (state == 0)
        state = GTK_SVG_STATE_EMPTY;
      else if (state == GTK_SVG_STATE_EMPTY)
        state = n_states - 1;
      else
        state--;
    }

  g_print ("state now %u\n", state);
  gtk_svg_set_state (svg, state);
}

static void
error_cb (GtkSvg *svg, GError *error)
{
  if (error->domain == GTK_SVG_ERROR)
    {
      const GtkSvgLocation *start = gtk_svg_error_get_start (error);
      const GtkSvgLocation *end = gtk_svg_error_get_end (error);
      const char *element = gtk_svg_error_get_element (error);
      const char *attribute = gtk_svg_error_get_attribute (error);

      if (start)
        {
          if (end->lines != start->lines || end->line_chars != start->line_chars)
            g_print ("%lu.%lu - %lu.%lu: ",
                     start->lines, start->line_chars,
                     end->lines, end->line_chars);
          else
            g_print ("%lu.%lu: ", start->lines, start->line_chars);
        }

      if (element && attribute)
        g_print ("(%s / %s) ", element, attribute);
      else if (element)
        g_print ("(%s) ", element);
    }

  g_print ("%s\n", error->message);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *picture;
  char *contents;
  size_t length;
  GBytes *bytes;
  GError *error = NULL;
  GtkSvg *svg;
  GtkEventController *click;

  if (argc < 2)
    {
      g_print ("No svg file given.\n");
      return 0;
    }

  gtk_init ();

  window = gtk_window_new ();

  if (!g_file_get_contents (argv[1], &contents, &length, &error))
    g_error ("%s", error->message);

  bytes = g_bytes_new_take (contents, length);

  svg = gtk_svg_new ();
  g_signal_connect (svg, "error", G_CALLBACK (error_cb), NULL);
  gtk_svg_load_from_bytes (svg, bytes);

  gtk_svg_play (svg);

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (svg));
  gtk_window_set_child (GTK_WINDOW (window), picture);

  click = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
  g_signal_connect (click, "pressed", G_CALLBACK (clicked), svg);
  gtk_widget_add_controller (picture, click);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()))
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
