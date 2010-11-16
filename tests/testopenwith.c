/* testopenwith.c
 * Copyright (C) 2010 Red Hat, Inc.
 * Authors: Cosimo Cecchi
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

#include <config.h>

#include <stdlib.h>
#include <gtk/gtk.h>

static GtkWidget *toplevel;
static GFile *file;
static GtkWidget *grid, *file_l, *open, *show_all, *show_set_as_default;
static GtkWidget *radio_file, *radio_file_default, *radio_content, *radio_content_default, *dialog;

static void
dialog_response (GtkDialog *d,
		 gint response_id,
		 gpointer user_data)
{
  GAppInfo *app_info;
  const gchar *name;

  g_print ("Response: %d\n", response_id);

  if (response_id == GTK_RESPONSE_OK)
    {
      app_info = gtk_open_with_dialog_get_selected_application (GTK_OPEN_WITH_DIALOG (d));
      name = g_app_info_get_name (app_info);
      g_print ("Application selected: %s\n", name);

      g_object_unref (app_info);
    }

  gtk_widget_destroy (GTK_WIDGET (d));
}

static void
display_dialog (GtkButton *b,
		gpointer user_data)
{
  gboolean use_file = FALSE;
  gboolean default_mode = FALSE;
  gchar *content_type = NULL;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_file)))
    {
      use_file = TRUE;
      default_mode = FALSE;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_file_default)))
    {
      use_file = TRUE;
      default_mode = TRUE;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_content)))
    {
      use_file = FALSE;
      default_mode = FALSE;
    }
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_content_default)))
    {
      use_file = FALSE;
      default_mode = TRUE;
    }

  if (use_file)
    {
      dialog = gtk_open_with_dialog_new (GTK_WINDOW (toplevel),
					 0, default_mode ?
					 GTK_OPEN_WITH_DIALOG_MODE_SELECT_DEFAULT :
					 GTK_OPEN_WITH_DIALOG_MODE_SELECT_ONE,
					 file);
    }
  else
    {
      GFileInfo *info;

      info = g_file_query_info (file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				0, NULL, NULL);
      content_type = g_strdup (g_file_info_get_content_type (info));

      g_object_unref (info);

      dialog = gtk_open_with_dialog_new_for_content_type (GTK_WINDOW (toplevel),
							  0, default_mode ?
							  GTK_OPEN_WITH_DIALOG_MODE_SELECT_DEFAULT :
							  GTK_OPEN_WITH_DIALOG_MODE_SELECT_ONE,
							  content_type);
    }

  gtk_open_with_dialog_set_show_other_applications (GTK_OPEN_WITH_DIALOG (dialog),
						    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (show_all)));
  gtk_open_with_dialog_set_show_set_as_default_button (GTK_OPEN_WITH_DIALOG (dialog),
						       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (show_set_as_default)));
  gtk_widget_show (dialog);

  g_signal_connect (dialog, "response",
		    G_CALLBACK (dialog_response), NULL);

  g_free (content_type);
}

static void
show_all_toggled (GtkToggleButton *b,
		  gpointer user_data)
{
  if (dialog != NULL)
    {
      gboolean toggled;

      toggled = gtk_toggle_button_get_active (b);

      gtk_open_with_dialog_set_show_other_applications (GTK_OPEN_WITH_DIALOG (dialog),
							toggled);
    }
}

static void
show_set_as_default_toggled (GtkToggleButton *b,
		  gpointer user_data)
{
  if (dialog != NULL)
    {
      gboolean toggled;

      toggled = gtk_toggle_button_get_active (b);

      gtk_open_with_dialog_set_show_set_as_default_button (GTK_OPEN_WITH_DIALOG (dialog),
							   toggled);
    }
}

static void
button_clicked (GtkButton *b,
		gpointer user_data)
{
  GtkWidget *w;
  gchar *path;

  w = gtk_file_chooser_dialog_new ("Select file",
				   GTK_WINDOW (toplevel),
				   GTK_FILE_CHOOSER_ACTION_OPEN,
				   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				   NULL);

  gtk_dialog_run (GTK_DIALOG (w));
  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (w));
  path = g_file_get_path (file);
  gtk_button_set_label (GTK_BUTTON (file_l), path);

  gtk_widget_destroy (w);

  gtk_widget_set_sensitive (open, TRUE);

  g_free (path);
}

int
main (int argc,
      char **argv)
{
  GtkWidget *w;

  g_type_init ();
  gtk_init (&argc, &argv);

  toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  grid = gtk_grid_new ();

  w = gtk_label_new ("File:");
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid),
		   w, 0, 0, 1, 1);

  file_l = gtk_button_new_with_label ("Select");
  gtk_widget_set_halign (file_l, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (grid), file_l,
			   w, GTK_POS_RIGHT, 1, 1);
  g_signal_connect (file_l, "clicked",
		    G_CALLBACK (button_clicked), NULL);

  radio_file = gtk_radio_button_new_with_label (NULL, "Use GFile and select one");
  radio_file_default = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_file),
								    "Use GFile and select default");
  radio_content = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_file),
							       "Use content type and select one");
  radio_content_default = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio_file),
								       "Use content type and select default");

  gtk_grid_attach (GTK_GRID (grid), radio_file,
		   0, 1, 1, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), radio_file_default,
			   radio_file, GTK_POS_BOTTOM, 1, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), radio_content,
			   radio_file_default, GTK_POS_BOTTOM, 1, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), radio_content_default,
			   radio_content, GTK_POS_BOTTOM, 1, 1);

  open = gtk_button_new_with_label ("Trigger Open With dialog");
  gtk_grid_attach_next_to (GTK_GRID (grid), open,
			   radio_content_default, GTK_POS_BOTTOM, 1, 1);
  gtk_widget_set_sensitive (open, FALSE);
  g_signal_connect (open, "clicked",
		    G_CALLBACK (display_dialog), NULL);

  show_all = gtk_check_button_new_with_label ("Show all applications");
  gtk_grid_attach_next_to (GTK_GRID (grid), show_all,
			   open, GTK_POS_BOTTOM, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (show_all), TRUE);
  g_signal_connect (show_all, "toggled",
		    G_CALLBACK (show_all_toggled), NULL);

  show_set_as_default = gtk_check_button_new_with_label ("Show set as default");
  gtk_grid_attach_next_to (GTK_GRID (grid), show_set_as_default,
			   show_all, GTK_POS_BOTTOM, 1, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (show_set_as_default), TRUE);
  g_signal_connect (show_set_as_default, "toggled",
		    G_CALLBACK (show_set_as_default_toggled), NULL);

  gtk_container_add (GTK_CONTAINER (toplevel), grid);

  gtk_widget_show_all (toplevel);
  g_signal_connect (toplevel, "delete-event",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return EXIT_SUCCESS;
}
