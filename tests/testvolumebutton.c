/* testvolumebutton.c
 * Copyright (C) 2007  Red Hat, Inc.
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

#include <gtk/gtk.h>

static void
value_changed (GtkWidget *button,
               gdouble volume,
               gpointer user_data)
{
  g_message ("volume changed to %f", volume);
}

static void
toggle_orientation (GtkWidget *button,
                    GtkWidget *scalebutton)
{
  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (scalebutton)) ==
      GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_orientable_set_orientation (GTK_ORIENTABLE (scalebutton),
                                        GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      gtk_orientable_set_orientation (GTK_ORIENTABLE (scalebutton),
                                        GTK_ORIENTATION_HORIZONTAL);
    }
}

static void
response_cb (GtkDialog *dialog,
             gint       arg1,
             gpointer   user_data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
show_error (gpointer data)
{
  GtkWindow *window = (GtkWindow *) data;
  GtkWidget *dialog;

  g_message ("showing error");

  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "This should have unbroken the grab");
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (response_cb), NULL);
  gtk_widget_show (dialog);

  return FALSE;
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *button2;
  GtkWidget *button3;
  GtkWidget *box;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  button = gtk_volume_button_new ();
  button2 = gtk_volume_button_new ();
  box = gtk_hbox_new (FALSE, 0);

  g_signal_connect (G_OBJECT (button), "value-changed",
                    G_CALLBACK (value_changed),
                    NULL);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), button);
  gtk_container_add (GTK_CONTAINER (box), button2);

  button3 = gtk_button_new_with_label ("Toggle orientation");
  gtk_container_add (GTK_CONTAINER (box), button3);

  g_signal_connect (G_OBJECT (button3), "clicked",
                    G_CALLBACK (toggle_orientation),
                    button);
  g_signal_connect (G_OBJECT (button3), "clicked",
                    G_CALLBACK (toggle_orientation),
                    button2);

  gtk_widget_show_all (window);
  gtk_button_clicked (GTK_BUTTON (button));
  g_timeout_add (4000, (GSourceFunc) show_error, window);

  gtk_main ();

  return 0;
}
