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
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
  gtk_widget_show (dialog);
  i++;
}

typedef struct {
  GtkWidget *local_entry1;
  GtkWidget *local_entry2;
  GtkWidget *global_entry1;
  GtkWidget *global_entry2;
} ResponseData;

static void
on_dialog_response (GtkDialog *dialog,
                    int        response,
                    gpointer   user_data)
{
  ResponseData *data = user_data;

  if (response == GTK_RESPONSE_OK)
    {
      gtk_editable_set_text (GTK_EDITABLE (data->global_entry1),
                             gtk_editable_get_text (GTK_EDITABLE (data->local_entry1)));
      gtk_editable_set_text (GTK_EDITABLE (data->global_entry2),
                             gtk_editable_get_text (GTK_EDITABLE (data->local_entry2)));
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
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
  ResponseData *data;

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
  gtk_box_append (GTK_BOX (content_area), hbox);

  image = gtk_image_new_from_icon_name ("dialog-question");
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_box_append (GTK_BOX (hbox), image);

  table = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_grid_set_column_spacing (GTK_GRID (table), 4);
  gtk_box_append (GTK_BOX (hbox), table);
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

  data = g_new (ResponseData, 1);
  data->local_entry1 = local_entry1;
  data->local_entry2 = local_entry2;
  data->global_entry1 = entry1;
  data->global_entry2 = entry2;

  g_signal_connect_data (dialog, "response",
                         G_CALLBACK (on_dialog_response),
                         data, (GClosureNotify) g_free,
                         0);

  gtk_widget_show (dialog);
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
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      frame = gtk_frame_new ("Dialogs");
      gtk_widget_set_margin_start (frame, 8);
      gtk_widget_set_margin_end (frame, 8);
      gtk_widget_set_margin_top (frame, 8);
      gtk_widget_set_margin_bottom (frame, 8);
      gtk_window_set_child (GTK_WINDOW (window), frame);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (vbox, 8);
      gtk_widget_set_margin_end (vbox, 8);
      gtk_widget_set_margin_top (vbox, 8);
      gtk_widget_set_margin_bottom (vbox, 8);
      gtk_frame_set_child (GTK_FRAME (frame), vbox);

      /* Standard message dialog */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);
      button = gtk_button_new_with_mnemonic ("_Message Dialog");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (message_dialog_clicked), NULL);
      gtk_box_append (GTK_BOX (hbox), button);

      gtk_box_append (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

      /* Interactive dialog*/
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);
      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      button = gtk_button_new_with_mnemonic ("_Interactive Dialog");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (interactive_dialog_clicked), NULL);
      gtk_box_append (GTK_BOX (hbox), vbox2);
      gtk_box_append (GTK_BOX (vbox2), button);

      table = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (table), 4);
      gtk_grid_set_column_spacing (GTK_GRID (table), 4);
      gtk_box_append (GTK_BOX (hbox), table);

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
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
