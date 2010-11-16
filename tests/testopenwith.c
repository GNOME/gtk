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

static void
dialog_response_cb (GtkDialog *dialog,
		    gint response_id,
		    gpointer _user_data)
{
  g_print ("Response: %d\n", response_id);

  gtk_widget_destroy (GTK_WIDGET (dialog));
  gtk_main_quit ();
}

int
main (int argc,
      char **argv)
{
  GOptionContext *context;
  GError *error = NULL;
  gchar **files = NULL;
  GtkWidget *dialog;
  GFile *file;
  GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, NULL},
    { NULL }
  };

  g_type_init ();

  context = g_option_context_new ("- Test for GtkOpenWithDialog");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  g_option_context_parse (context, &argc, &argv, &error);

  if (error != NULL)
    {
      g_critical ("Error: %s\n", error->message);
      g_error_free (error);
      g_option_context_free (context);

      return EXIT_FAILURE;
    }

  g_option_context_free (context);

  if (files == NULL || files[0] == NULL)
    {
      g_critical ("You need to specify a file path.");
      return EXIT_FAILURE;
    }

  file = g_file_new_for_commandline_arg (files[0]);
  dialog = gtk_open_with_dialog_new (NULL, 0, GTK_OPEN_WITH_DIALOG_MODE_SELECT_DEFAULT,
				     file);

  gtk_widget_show (dialog);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (dialog_response_cb), NULL);

  gtk_main ();

  return EXIT_SUCCESS;
}
