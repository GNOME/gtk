/* Dialogs and Message Boxes
 *
 * Dialog widgets are used to pop up a transient window for user feedback.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *entry1 = NULL;
static GtkWidget *entry2 = NULL;

static void
message_dialog_clicked (GtkButton *button,
                        gpointer   user_data)
{
  GtkWidget *dialog;
  static gint i = 1;

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK_CANCEL,
                                   "This message box has been popped up the following\n"
                                   "number of times:");
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%d", i);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  i++;
}

static void
interactive_dialog_clicked (GtkButton *button,
                            gpointer   user_data)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *table;
  GtkWidget *local_entry1;
  GtkWidget *local_entry2;
  GtkWidget *label;
  gint response;

  dialog = gtk_dialog_new_with_buttons ("Interactive Dialog",
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL| GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_OK"),
                                        GTK_RESPONSE_OK,
                                        "_Cancel",
                                        GTK_RESPONSE_CANCEL,
                                        NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (content_area), hbox);

  image = gtk_image_new_from_icon_name ("dialog-question");
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_container_add (GTK_CONTAINER (hbox), image);

  table = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_grid_set_column_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (hbox), table);
  label = gtk_label_new_with_mnemonic ("_Entry 1");
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  local_entry1 = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (local_entry1), gtk_editable_get_text (GTK_EDITABLE (entry1)));
  gtk_grid_attach (GTK_GRID (table), local_entry1, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry1);

  label = gtk_label_new_with_mnemonic ("E_ntry 2");
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

  local_entry2 = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (local_entry2), gtk_editable_get_text (GTK_EDITABLE (entry2)));
  gtk_grid_attach (GTK_GRID (table), local_entry2, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry2);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      gtk_editable_set_text (GTK_EDITABLE (entry1), gtk_editable_get_text (GTK_EDITABLE (local_entry1)));
      gtk_editable_set_text (GTK_EDITABLE (entry2), gtk_editable_get_text (GTK_EDITABLE (local_entry2)));
    }

  gtk_widget_destroy (dialog);
}

GtkWidget *
do_dialog (GtkWidget *do_widget)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Dialogs and Message Boxes");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      frame = gtk_frame_new ("Dialogs");
      gtk_widget_set_margin_start (frame, 8);
      gtk_widget_set_margin_end (frame, 8);
      gtk_widget_set_margin_top (frame, 8);
      gtk_widget_set_margin_bottom (frame, 8);
      gtk_container_add (GTK_CONTAINER (window), frame);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (vbox, 8);
      gtk_widget_set_margin_end (vbox, 8);
      gtk_widget_set_margin_top (vbox, 8);
      gtk_widget_set_margin_bottom (vbox, 8);
      gtk_container_add (GTK_CONTAINER (frame), vbox);

      /* Standard message dialog */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);
      button = gtk_button_new_with_mnemonic ("_Message Dialog");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (message_dialog_clicked), NULL);
      gtk_container_add (GTK_CONTAINER (hbox), button);

      gtk_container_add (GTK_CONTAINER (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

      /* Interactive dialog*/
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      button = gtk_button_new_with_mnemonic ("_Interactive Dialog");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (interactive_dialog_clicked), NULL);
      gtk_container_add (GTK_CONTAINER (hbox), vbox2);
      gtk_container_add (GTK_CONTAINER (vbox2), button);

      table = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (table), 4);
      gtk_grid_set_column_spacing (GTK_GRID (table), 4);
      gtk_container_add (GTK_CONTAINER (hbox), table);

      label = gtk_label_new_with_mnemonic ("_Entry 1");
      gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

      entry1 = gtk_entry_new ();
      gtk_grid_attach (GTK_GRID (table), entry1, 1, 0, 1, 1);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry1);

      label = gtk_label_new_with_mnemonic ("E_ntry 2");
      gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

      entry2 = gtk_entry_new ();
      gtk_grid_attach (GTK_GRID (table), entry2, 1, 1, 1, 1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
