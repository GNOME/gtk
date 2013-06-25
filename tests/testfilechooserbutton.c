/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/* GTK+: gtkfilechooserbutton.c
 * 
 * Copyright (c) 2004 James M. Cape <jcape@ignore-your.tv>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>

#include <gtk/gtk.h>

#include "prop-editor.h"

static gchar *backend = "gtk+";
static gboolean rtl = FALSE;
static GOptionEntry entries[] = {
  { "backend", 'b', 0, G_OPTION_ARG_STRING, &backend, "The filesystem backend to use.", "gtk+" },
  { "right-to-left", 'r', 0, G_OPTION_ARG_NONE, &rtl, "Force right-to-left layout.", NULL },
  { NULL }
};

static gchar *gtk_src_dir = NULL;


static void
win_style_set_cb (GtkWidget *win)
{
  GtkWidget *content_area, *action_area;

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (win));
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (win));

  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
  gtk_box_set_spacing (GTK_BOX (content_area), 24);
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);
}

static gboolean
delete_event_cb (GtkWidget *editor,
		 gint       response,
		 gpointer   user_data)
{
  gtk_widget_hide (editor);

  return TRUE;
}


static void
properties_button_clicked_cb (GtkWidget *button,
			      GObject   *entry)
{
  GtkWidget *editor;

  editor = g_object_get_data (entry, "properties-dialog");

  if (editor == NULL)
    {
      editor = create_prop_editor (G_OBJECT (entry), G_TYPE_INVALID);
      gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
      gtk_window_set_transient_for (GTK_WINDOW (editor),
				    GTK_WINDOW (gtk_widget_get_toplevel (button)));
      g_signal_connect (editor, "delete-event", G_CALLBACK (delete_event_cb), NULL);
      g_object_set_data (entry, "properties-dialog", editor);
    }

  gtk_window_present (GTK_WINDOW (editor));
}


static void
print_selected_path_clicked_cb (GtkWidget *button,
				gpointer   user_data)
{
  gchar *folder, *filename;

  folder = gtk_file_chooser_get_current_folder (user_data);
  filename = gtk_file_chooser_get_filename (user_data);
  g_message ("Currently Selected:\n\tFolder: `%s'\n\tFilename: `%s'\nDone.\n",
	     folder, filename);
  g_free (folder);
  g_free (filename);
}

static void
add_pwds_parent_as_shortcut_clicked_cb (GtkWidget *button,
					gpointer   user_data)
{
  GError *err = NULL;

  if (!gtk_file_chooser_add_shortcut_folder (user_data, gtk_src_dir, &err))
    {
      g_message ("Couldn't add `%s' as shortcut folder: %s", gtk_src_dir,
		 err->message);
      g_error_free (err);
    }
  else
    {
      g_message ("Added `%s' as shortcut folder.", gtk_src_dir);
    }
}

static void
del_pwds_parent_as_shortcut_clicked_cb (GtkWidget *button,
					gpointer   user_data)
{
  GError *err = NULL;

  if (!gtk_file_chooser_remove_shortcut_folder (user_data, gtk_src_dir, &err))
    {
      g_message ("Couldn't remove `%s' as shortcut folder: %s", gtk_src_dir,
		 err->message);
      g_error_free (err);
    }
  else
    {
      g_message ("Removed `%s' as shortcut folder.", gtk_src_dir);
    }
}

static void
unselect_all_clicked_cb (GtkWidget *button,
                         gpointer   user_data)
{
  gtk_file_chooser_unselect_all (user_data);
}

static void
tests_button_clicked_cb (GtkButton *real_button,
			 gpointer   user_data)
{
  GtkWidget *tests;

  tests = g_object_get_data (user_data, "tests-dialog");

  if (tests == NULL)
    {
      GtkWidget *box, *button;

      tests = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (tests),
			    "Tests - TestFileChooserButton");
      gtk_container_set_border_width (GTK_CONTAINER (tests), 12);
      gtk_window_set_transient_for (GTK_WINDOW (tests),
				    GTK_WINDOW (gtk_widget_get_toplevel (user_data)));

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (tests), box);
      gtk_widget_show (box);

      button = gtk_button_new_with_label ("Print Selected Path");
      g_signal_connect (button, "clicked",
			G_CALLBACK (print_selected_path_clicked_cb), user_data);
      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Add $PWD's Parent as Shortcut");
      g_signal_connect (button, "clicked",
			G_CALLBACK (add_pwds_parent_as_shortcut_clicked_cb), user_data);
      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Remove $PWD's Parent as Shortcut");
      g_signal_connect (button, "clicked",
			G_CALLBACK (del_pwds_parent_as_shortcut_clicked_cb), user_data);
      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Unselect all");
      g_signal_connect (button, "clicked",
			G_CALLBACK (unselect_all_clicked_cb), user_data);
      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      g_signal_connect (tests, "delete-event", G_CALLBACK (delete_event_cb), NULL);
      g_object_set_data (user_data, "tests-dialog", tests);
    }

  gtk_window_present (GTK_WINDOW (tests));
}

static void
chooser_current_folder_changed_cb (GtkFileChooser *chooser,
				   gpointer        user_data)
{
  gchar *folder, *filename;

  folder = gtk_file_chooser_get_current_folder_uri (chooser);
  filename = gtk_file_chooser_get_uri (chooser);
  g_message ("%s::current-folder-changed\n\tFolder: `%s'\n\tFilename: `%s'\nDone.\n",
	     G_OBJECT_TYPE_NAME (chooser), folder, filename);
  g_free (folder);
  g_free (filename);
}

static void
chooser_selection_changed_cb (GtkFileChooser *chooser,
			      gpointer        user_data)
{
  gchar *filename;

  filename = gtk_file_chooser_get_uri (chooser);
  g_message ("%s::selection-changed\n\tSelection:`%s'\nDone.\n",
	     G_OBJECT_TYPE_NAME (chooser), filename);
  g_free (filename);
}

static void
chooser_file_activated_cb (GtkFileChooser *chooser,
			   gpointer        user_data)
{
  gchar *folder, *filename;

  folder = gtk_file_chooser_get_current_folder_uri (chooser);
  filename = gtk_file_chooser_get_uri (chooser);
  g_message ("%s::file-activated\n\tFolder: `%s'\n\tFilename: `%s'\nDone.\n",
	     G_OBJECT_TYPE_NAME (chooser), folder, filename);
  g_free (folder);
  g_free (filename);
}

static void
chooser_update_preview_cb (GtkFileChooser *chooser,
			   gpointer        user_data)
{
  gchar *filename;

  filename = gtk_file_chooser_get_preview_uri (chooser);
  if (filename != NULL)
    {
      g_message ("%s::update-preview\n\tPreview Filename: `%s'\nDone.\n",
		 G_OBJECT_TYPE_NAME (chooser), filename);
      g_free (filename);
    }
}


int
main (int   argc,
      char *argv[])
{
  GtkWidget *win, *vbox, *frame, *alignment, *group_box;
  GtkWidget *hbox, *label, *chooser, *button;
  GtkSizeGroup *label_group;
  GOptionContext *context;
  gchar *cwd;

  context = g_option_context_new ("- test GtkFileChooserButton widget");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  g_option_context_parse (context, &argc, &argv, NULL);
  g_option_context_free (context);

  gtk_init (&argc, &argv);

  /* to test rtl layout, use "--right-to-left" */
  if (rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  cwd = g_get_current_dir();
  gtk_src_dir = g_path_get_dirname (cwd);
  g_free (cwd);

  win = gtk_dialog_new_with_buttons ("TestFileChooserButton", NULL, 0,
				     "_Quit", GTK_RESPONSE_CLOSE, NULL);
  g_signal_connect (win, "style-set", G_CALLBACK (win_style_set_cb), NULL);
  g_signal_connect (win, "response", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (win))), vbox);

  frame = gtk_frame_new ("<b>GtkFileChooserButton</b>");
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_label_set_use_markup (GTK_LABEL (gtk_frame_get_label_widget (GTK_FRAME (frame))), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  
  label_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  
  group_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (alignment), group_box);

  /* OPEN */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (group_box), hbox, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic ("_Open:");
  gtk_size_group_add_widget (GTK_SIZE_GROUP (label_group), label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  chooser = gtk_file_chooser_button_new ("Select A File - testfilechooserbutton",
                                         GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (chooser), gtk_src_dir, NULL);
  gtk_file_chooser_remove_shortcut_folder (GTK_FILE_CHOOSER (chooser), gtk_src_dir, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser);
  g_signal_connect (chooser, "current-folder-changed",
		    G_CALLBACK (chooser_current_folder_changed_cb), NULL);
  g_signal_connect (chooser, "selection-changed", G_CALLBACK (chooser_selection_changed_cb), NULL);
  g_signal_connect (chooser, "file-activated", G_CALLBACK (chooser_file_activated_cb), NULL);
  g_signal_connect (chooser, "update-preview", G_CALLBACK (chooser_update_preview_cb), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), chooser, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Properties");
  g_signal_connect (button, "clicked", G_CALLBACK (properties_button_clicked_cb), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Tests");
  g_signal_connect (button, "clicked", G_CALLBACK (tests_button_clicked_cb), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  /* SELECT_FOLDER */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (group_box), hbox, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic ("Select _Folder:");
  gtk_size_group_add_widget (GTK_SIZE_GROUP (label_group), label);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  chooser = gtk_file_chooser_button_new ("Select A Folder - testfilechooserbutton",
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (chooser), gtk_src_dir, NULL);
  gtk_file_chooser_remove_shortcut_folder (GTK_FILE_CHOOSER (chooser), gtk_src_dir, NULL);
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (chooser), gtk_src_dir, NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser);
  g_signal_connect (chooser, "current-folder-changed",
		    G_CALLBACK (chooser_current_folder_changed_cb), NULL);
  g_signal_connect (chooser, "selection-changed", G_CALLBACK (chooser_selection_changed_cb), NULL);
  g_signal_connect (chooser, "file-activated", G_CALLBACK (chooser_file_activated_cb), NULL);
  g_signal_connect (chooser, "update-preview", G_CALLBACK (chooser_update_preview_cb), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), chooser, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Properties");
  g_signal_connect (button, "clicked", G_CALLBACK (properties_button_clicked_cb), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Tests");
  g_signal_connect (button, "clicked", G_CALLBACK (tests_button_clicked_cb), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  g_object_unref (label_group);

  gtk_widget_show_all (win);
  gtk_window_present (GTK_WINDOW (win));

  gtk_main ();

  return 0;
}
