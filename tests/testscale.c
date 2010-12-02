/* testscale.c - scale mark demo
 * Copyright (C) 2009 Red Hat, Inc.
 * Author: Matthias Clasen
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

static void
show_trough_toggled (GtkToggleButton *button,
                     GtkScale        *scale)
{
  gboolean value;

  value = gtk_toggle_button_get_active (button);
  gtk_range_set_range (GTK_RANGE (scale), 0., value ? 100.0 : 0.);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *box2;
  GtkWidget *frame;
  GtkWidget *scale;
  GtkWidget *toggle;
  gdouble marks[3] = { 0.0, 50.0, 100.0 };
  const gchar *labels[3] = { 
    "<small>Left</small>", 
    "<small>Middle</small>", 
    "<small>Right</small>" 
  };

  gdouble bath_marks[4] = { 0.0, 33.3, 66.6, 100.0 };
  const gchar *bath_labels[4] = { 
    "<span color='blue' size='small'>Cold</span>", 
    "<span size='small'>Baby bath</span>", 
    "<span size='small'>Hot tub</span>", 
    "<span color='Red' size='small'>Hot</span>" 
  };

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Ranges with marks");
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  frame = gtk_frame_new ("No marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

  frame = gtk_frame_new ("Simple marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_BOTTOM, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);
 
  frame = gtk_frame_new ("Labeled marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_BOTTOM, labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_BOTTOM, labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_BOTTOM, labels[2]);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Some labels");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_BOTTOM, labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_BOTTOM, labels[2]);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Above and below");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[0], GTK_POS_TOP, bath_labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[1], GTK_POS_BOTTOM, bath_labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[2], GTK_POS_BOTTOM, bath_labels[2]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[3], GTK_POS_TOP, bath_labels[3]);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

  frame = gtk_frame_new ("Show/hide trough");
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 100, 1);
  gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
  toggle = gtk_toggle_button_new_with_label ("Show slider trough");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (show_trough_toggled), scale);
  gtk_box_pack_start (GTK_BOX (box2), toggle, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box2);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start (GTK_BOX (box), box2, TRUE, TRUE, 0);

  frame = gtk_frame_new ("No marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box2), frame, FALSE, FALSE, 0);

  frame = gtk_frame_new ("Simple marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_LEFT, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_LEFT, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_LEFT, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box2), frame, FALSE, FALSE, 0);
 
  frame = gtk_frame_new ("Labeled marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_LEFT, labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_LEFT, labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_LEFT, labels[2]);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box2), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Some labels");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_LEFT, labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_LEFT, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_LEFT, labels[2]);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box2), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Right and left");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
                                    0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[0], GTK_POS_RIGHT, bath_labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[1], GTK_POS_LEFT, bath_labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[2], GTK_POS_LEFT, bath_labels[2]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[3], GTK_POS_RIGHT, bath_labels[3]);
  gtk_container_add (GTK_CONTAINER (frame), scale);
  gtk_box_pack_start (GTK_BOX (box2), frame, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}


