/* Dialog and Message Boxes
 *
 * Dialog widgets are used to pop up a transient window for user feedback.
 */

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
				   GTK_BUTTONS_OK,
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
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *stock;
  GtkWidget *table;
  GtkWidget *local_entry1;
  GtkWidget *local_entry2;
  GtkWidget *label;
  gint response;

  dialog = gtk_dialog_new_with_buttons ("Interactive Dialog",
					GTK_WINDOW (window),
					GTK_DIALOG_MODAL| GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_OK,
					GTK_RESPONSE_OK,
                                        "_Non-stock Button",
                                        GTK_RESPONSE_CANCEL,
					NULL);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, FALSE, 0);

  stock = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), stock, FALSE, FALSE, 0);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);
  label = gtk_label_new_with_mnemonic ("_Entry 1");
  gtk_table_attach_defaults (GTK_TABLE (table),
			     label,
			     0, 1, 0, 1);
  local_entry1 = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (local_entry1), gtk_entry_get_text (GTK_ENTRY (entry1)));
  gtk_table_attach_defaults (GTK_TABLE (table), local_entry1, 1, 2, 0, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry1);

  label = gtk_label_new_with_mnemonic ("E_ntry 2");
  gtk_table_attach_defaults (GTK_TABLE (table),
			     label,
			     0, 1, 1, 2);

  local_entry2 = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (local_entry2), gtk_entry_get_text (GTK_ENTRY (entry2)));
  gtk_table_attach_defaults (GTK_TABLE (table), local_entry2, 1, 2, 1, 2);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry2);
  
  gtk_widget_show_all (hbox);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      gtk_entry_set_text (GTK_ENTRY (entry1), gtk_entry_get_text (GTK_ENTRY (local_entry1)));
      gtk_entry_set_text (GTK_ENTRY (entry2), gtk_entry_get_text (GTK_ENTRY (local_entry2)));
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
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Dialogs");

      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      frame = gtk_frame_new ("Dialogs");
      gtk_container_add (GTK_CONTAINER (window), frame);

      vbox = gtk_vbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
      gtk_container_add (GTK_CONTAINER (frame), vbox);

      /* Standard message dialog */
      hbox = gtk_hbox_new (FALSE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      button = gtk_button_new_with_mnemonic ("_Message Dialog");
      g_signal_connect (button, "clicked",
			G_CALLBACK (message_dialog_clicked), NULL);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);

      /* Interactive dialog*/
      hbox = gtk_hbox_new (FALSE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      vbox2 = gtk_vbox_new (FALSE, 0);

      button = gtk_button_new_with_mnemonic ("_Interactive Dialog");
      g_signal_connect (button, "clicked",
			G_CALLBACK (interactive_dialog_clicked), NULL);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);

      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 4);
      gtk_table_set_col_spacings (GTK_TABLE (table), 4);
      gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);

      label = gtk_label_new_with_mnemonic ("_Entry 1");
      gtk_table_attach_defaults (GTK_TABLE (table),
				 label,
				 0, 1, 0, 1);

      entry1 = gtk_entry_new ();
      gtk_table_attach_defaults (GTK_TABLE (table), entry1, 1, 2, 0, 1);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry1);

      label = gtk_label_new_with_mnemonic ("E_ntry 2");
      
      gtk_table_attach_defaults (GTK_TABLE (table),
				 label,
				 0, 1, 1, 2);

      entry2 = gtk_entry_new ();
      gtk_table_attach_defaults (GTK_TABLE (table), entry2, 1, 2, 1, 2);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry2);
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
