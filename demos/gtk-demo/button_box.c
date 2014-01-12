/* Button Boxes
 *
 * The Button Box widgets are used to arrange buttons with padding.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *
create_bbox (gint  horizontal,
             char *title,
             gint  spacing,
             gint  layout)
{
  GtkWidget *frame;
  GtkWidget *bbox;
  GtkWidget *button;

  frame = gtk_frame_new (title);

  if (horizontal)
    bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  else
    bbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);

  gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), bbox);

  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), layout);
  gtk_box_set_spacing (GTK_BOX (bbox), spacing);

  button = gtk_button_new_with_label (_("OK"));
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_button_new_with_label (_("Cancel"));
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_button_new_with_label (_("Help"));
  gtk_container_add (GTK_CONTAINER (bbox), button);

  return frame;
}

GtkWidget *
do_button_box (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *frame_horz;
  GtkWidget *frame_vert;

  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_screen (GTK_WINDOW (window),
                           gtk_widget_get_screen (do_widget));
    gtk_window_set_title (GTK_WINDOW (window), "Button Boxes");

    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_widget_destroyed),
                      &window);

    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);

    frame_horz = gtk_frame_new ("Horizontal Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_horz, TRUE, TRUE, 10);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
    gtk_container_add (GTK_CONTAINER (frame_horz), vbox);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Spread", 40, GTK_BUTTONBOX_SPREAD),
                        TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Edge", 40, GTK_BUTTONBOX_EDGE),
                        TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Start", 40, GTK_BUTTONBOX_START),
                        TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "End", 40, GTK_BUTTONBOX_END),
                        TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Center", 40, GTK_BUTTONBOX_CENTER),
                        TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Expand", 40, GTK_BUTTONBOX_EXPAND),
                        TRUE, TRUE, 5);

    frame_vert = gtk_frame_new ("Vertical Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_vert, TRUE, TRUE, 10);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_container_add (GTK_CONTAINER (frame_vert), hbox);

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Spread", 10, GTK_BUTTONBOX_SPREAD),
                        TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Edge", 10, GTK_BUTTONBOX_EDGE),
                        TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Start", 10, GTK_BUTTONBOX_START),
                        TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "End", 10, GTK_BUTTONBOX_END),
                        TRUE, TRUE, 5);
    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Center", 10, GTK_BUTTONBOX_CENTER),
                        TRUE, TRUE, 5);
    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Expand", 10, GTK_BUTTONBOX_EXPAND),
                        TRUE, TRUE, 5);
  }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
