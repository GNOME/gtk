#include <gtk/gtk.h>
#include "gtkfilechooserdialog.h"
#include "gtkfilechooser.h"
#include "prop-editor.h"

#ifdef USE_GNOME_VFS
#include "gtkfilesystemgnomevfs.h"
#else
#include "gtkfilesystemunix.h"
#endif

static void
print_current_folder (GtkFileChooser *chooser)
{
  gchar *uri;

  uri = gtk_file_chooser_get_current_folder_uri (chooser);
  g_print ("Current folder changed :\n  %s\n", uri);
  g_free (uri);
}

static void
print_selected (GtkFileChooser *chooser)
{
  GSList *uris = gtk_file_chooser_get_uris (chooser);
  GSList *tmp_list;

  g_print ("Selection changed :\n");
  for (tmp_list = uris; tmp_list; tmp_list = tmp_list->next)
    {
      gchar *uri = tmp_list->data;
      g_print ("  %s\n", uri);
      g_free (uri);
    }
  g_print ("\n");
  g_slist_free (uris);
}

static void
response_cb (GtkDialog *dialog,
	     gint       response_id)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GtkWidget *control_window;
  GtkWidget *vbbox;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkWidget *prop_editor;
  GtkFileSystem *file_system;
  
  gtk_init (&argc, &argv);

#ifdef USE_GNOME_VFS
  file_system = gtk_file_system_gnome_vfs_new ();
#else  
  file_system = gtk_file_system_unix_new ();
#endif
  
  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			 "action", GTK_FILE_CHOOSER_ACTION_OPEN,
			 "file-system", file_system,
			 "title", "Select a file",
			 NULL);
			 
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  GTK_STOCK_OPEN, GTK_RESPONSE_OK,
			  NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  
  g_signal_connect (dialog, "selection-changed",
		    G_CALLBACK (print_selected), NULL);
  g_signal_connect (dialog, "current-folder-changed",
		    G_CALLBACK (print_current_folder), NULL);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb), NULL);
  
  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 400);
  gtk_widget_show (dialog);

  prop_editor = create_prop_editor (G_OBJECT (dialog), GTK_TYPE_FILE_CHOOSER);

  control_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  vbbox = gtk_vbutton_box_new ();
  gtk_container_add (GTK_CONTAINER (control_window), vbbox);

  button = gtk_button_new_with_mnemonic ("_Select all");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_file_chooser_select_all), dialog);
  
  button = gtk_button_new_with_mnemonic ("_Unselect all");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_file_chooser_unselect_all), dialog);

  gtk_widget_show_all (control_window);
  
  gtk_main ();

  return 0;
}
