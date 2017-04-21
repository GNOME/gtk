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

  g_object_set (bbox, "margin", 5, NULL);

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

    main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    g_object_set (main_vbox, "margin", 10, NULL);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);

    frame_horz = gtk_frame_new ("Horizontal Button Boxes");
    gtk_widget_set_margin_top (frame_horz, 10);
    gtk_widget_set_margin_bottom (frame_horz, 10);
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_horz);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    g_object_set (vbox, "margin", 10, NULL);
    gtk_container_add (GTK_CONTAINER (frame_horz), vbox);

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Spread", 40, GTK_BUTTONBOX_SPREAD));

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Edge", 40, GTK_BUTTONBOX_EDGE));

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Start", 40, GTK_BUTTONBOX_START));

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "End", 40, GTK_BUTTONBOX_END));

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Center", 40, GTK_BUTTONBOX_CENTER));

    gtk_box_pack_start (GTK_BOX (vbox),
                        create_bbox (TRUE, "Expand", 0, GTK_BUTTONBOX_EXPAND));

    frame_vert = gtk_frame_new ("Vertical Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_vert);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
    g_object_set (hbox, "margin", 10, NULL);
    gtk_container_add (GTK_CONTAINER (frame_vert), hbox);

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Spread", 10, GTK_BUTTONBOX_SPREAD));

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Edge", 10, GTK_BUTTONBOX_EDGE));

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Start", 10, GTK_BUTTONBOX_START));

    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "End", 10, GTK_BUTTONBOX_END));
    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Center", 10, GTK_BUTTONBOX_CENTER));
    gtk_box_pack_start (GTK_BOX (hbox),
                        create_bbox (FALSE, "Expand", 0, GTK_BUTTONBOX_EXPAND));
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
