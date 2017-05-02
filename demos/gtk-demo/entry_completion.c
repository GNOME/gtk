/* Entry/Entry Completion
 *
 * GtkEntryCompletion provides a mechanism for adding support for
 * completion in GtkEntry.
 *
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

/* Creates a tree model containing the completions */
GtkTreeModel *
create_completion_model (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  
  store = gtk_list_store_new (1, G_TYPE_STRING);

  /* Append one word */
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "GNOME", -1);

  /* Append another word */
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "total", -1);

  /* And another word */
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "totally", -1);
  
  return GTK_TREE_MODEL (store);
}


GtkWidget *
do_entry_completion (GtkWidget *do_widget)
{
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkEntryCompletion *completion;
  GtkTreeModel *completion_model;
  
  if (!window)
  {
    window = gtk_dialog_new_with_buttons ("GtkEntryCompletion",
					  GTK_WINDOW (do_widget),
					  0,
					  GTK_STOCK_CLOSE,
					  GTK_RESPONSE_NONE,
					  NULL);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

    g_signal_connect (window, "response",
		      G_CALLBACK (gtk_widget_destroy), NULL);
    g_signal_connect (window, "destroy",
		      G_CALLBACK (gtk_widget_destroyed), &window);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))), vbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), "Completion demo, try writing <b>total</b> or <b>gnome</b> for example.");
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    /* Create our entry */
    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

    /* Create the completion object */
    completion = gtk_entry_completion_new ();

    /* Assign the completion to the entry */
    gtk_entry_set_completion (GTK_ENTRY (entry), completion);
    g_object_unref (completion);
    
    /* Create a tree model and use it as the completion model */
    completion_model = create_completion_model ();
    gtk_entry_completion_set_model (completion, completion_model);
    g_object_unref (completion_model);
    
    /* Use model column 0 as the text column */
    gtk_entry_completion_set_text_column (completion, 0);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}


