
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>

#include <gtk/gtk.h>

#include "prop-editor.h"


static void
win_style_set_cb (GtkWidget *win)
{
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (win)->vbox), 12);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (win)->vbox), 24);
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (win)->action_area), 0);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (win)->action_area), 6);
}


static gboolean
editor_delete_event_cb (GtkWidget *editor, gint response, gpointer user_data)
{
  gtk_widget_hide (editor);

  return TRUE;
}


static void
properties_button_clicked_cb (GtkWidget *button, GObject *entry)
{
  GtkWidget *editor;

  editor = g_object_get_data (entry, "properties-dialog");

  if (editor == NULL)
    {
      editor = create_prop_editor (G_OBJECT (entry), G_TYPE_INVALID);
      gtk_window_set_transient_for (GTK_WINDOW (editor),
				    GTK_WINDOW (gtk_widget_get_toplevel (button)));
      g_signal_connect (editor, "delete-event", G_CALLBACK (editor_delete_event_cb), NULL);
      g_object_set_data (entry, "properties-dialog", editor);
    }

  gtk_window_present (GTK_WINDOW (editor));
}


static void
chooser_current_folder_changed_cb (GtkFileChooser *chooser, gpointer user_data)
{
  gchar *folder, *filename;

  folder = gtk_file_chooser_get_current_folder (chooser);
  filename = gtk_file_chooser_get_filename (chooser);
  g_message ("%s:%s:\n`%s`\n\tFolder: `%s'\n\tFilename: `%s'", G_STRFUNC, G_STRLOC,
	     G_OBJECT_TYPE_NAME (chooser), folder, filename);
  g_free (folder);
  g_free (filename);
}


static void
chooser_selection_changed_cb (GtkFileChooser *chooser, gpointer user_data)
{
  GSList *selection;

  g_message ("%s:%s:\n`%s` Selection:", G_STRFUNC, G_STRLOC, G_OBJECT_TYPE_NAME (chooser));
  for (selection = gtk_file_chooser_get_filenames (chooser); selection != NULL;
       selection = g_slist_remove_link (selection, selection))
    {
      g_print ("`%s'\n", (const gchar *) selection->data);
      g_free (selection->data);
    }
  g_print ("Done.\n");
}


static void
chooser_file_activated_cb (GtkFileChooser *chooser, gpointer user_data)
{
  gchar *folder, *filename;

  folder = gtk_file_chooser_get_current_folder (chooser);
  filename = gtk_file_chooser_get_filename (chooser);
  g_message ("%s:%s:\n`%s`\nFolder: `%s'\nFilename: `%s'", G_STRFUNC, G_STRLOC, G_OBJECT_TYPE_NAME (chooser), folder, filename);
  g_free (folder);
  g_free (filename);
}


static void
chooser_update_preview_cb (GtkFileChooser *chooser, gpointer user_data)
{
  gchar *filename;

  filename = gtk_file_chooser_get_preview_filename (chooser);
  g_message ("%s:%s:\n`%s`\nPreview Filename: `%s'", G_STRFUNC, G_STRLOC, G_OBJECT_TYPE_NAME (chooser), filename);
  g_free (filename);
}


int
main (int argc, char *argv[])
{
  GtkWidget *win, *vbox, *frame, *alignment, *group_box, *hbox, *label, *chooser, *button;
  GtkSizeGroup *label_group;

  gtk_init (&argc, &argv);

  /* to test rtl layout, set RTL=1 in the environment */
  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  win = gtk_dialog_new_with_buttons ("TestFileChooserButton", NULL, GTK_DIALOG_NO_SEPARATOR,
				     GTK_STOCK_QUIT, GTK_RESPONSE_CLOSE, NULL);
  g_signal_connect (win, "style-set", G_CALLBACK (win_style_set_cb), NULL);
  g_signal_connect (win, "response", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_vbox_new (FALSE, 18);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (win)->vbox), vbox);

  frame = gtk_frame_new ("<b>GtkFileChooserButton</b>");
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_label_set_use_markup (GTK_LABEL (gtk_frame_get_label_widget (GTK_FRAME (frame))), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  
  label_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  
  group_box = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (alignment), group_box);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (group_box), hbox);

  label = gtk_label_new_with_mnemonic ("_Open:");
  gtk_size_group_add_widget (GTK_SIZE_GROUP (label_group), label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  chooser = gtk_file_chooser_button_new_with_backend ("Select A File - testfilechooserbutton",
						      "gtk+");
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser);
  g_signal_connect (chooser, "current-folder-changed",
		    G_CALLBACK (chooser_current_folder_changed_cb), NULL);
  g_signal_connect (chooser, "selection-changed", G_CALLBACK (chooser_selection_changed_cb), NULL);
  g_signal_connect (chooser, "file-activated", G_CALLBACK (chooser_file_activated_cb), NULL);
  g_signal_connect (chooser, "update-preview", G_CALLBACK (chooser_update_preview_cb), NULL);
  gtk_container_add (GTK_CONTAINER (hbox), chooser);

  button = gtk_button_new_with_label ("Properties...");
  g_signal_connect (button, "clicked", G_CALLBACK (properties_button_clicked_cb), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  g_object_unref (label_group);

  gtk_widget_show_all (win);
  gtk_window_present (GTK_WINDOW (win));

  gtk_main ();

  gtk_widget_destroy (win);

  return 0;
}
