/*  Copyright 2024 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-image-tool.h"

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *is_done = user_data;

  *is_done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
show_files (char     **filenames,
            gboolean   decorated)
{
  GtkWidget *sw;
  GtkWidget *window;
  gboolean done = FALSE;
  GString *title;
  GtkWidget *box;

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  title = g_string_new ("");
  for (int i = 0; i < g_strv_length (filenames); i++)
    {
      char *name = g_path_get_basename (filenames[i]);

      if (title->len > 0)
        g_string_append (title, " / ");
      g_string_append (title, name);

      g_free (name);
    }

  gtk_window_set_decorated (GTK_WINDOW (window), decorated);
  gtk_window_set_resizable (GTK_WINDOW (window), decorated);

  gtk_window_set_title (GTK_WINDOW (window), title->str);
  g_string_free (title, TRUE);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);

  gtk_window_set_child (GTK_WINDOW (window), sw);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);

  for (int i = 0; i < g_strv_length (filenames); i++)
    {
      GdkTexture *texture;
      GtkWidget *picture;

      texture = load_image_file (filenames[i]);

      picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_SCALE_DOWN);

      if (i > 0)
        gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

      gtk_box_append (GTK_BOX (box), picture);

      g_object_unref (texture);
    }

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

void
do_show (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  gboolean decorated = TRUE;
  const GOptionEntry entries[] = {
    { "undecorated", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &decorated, N_("Don't add a titlebar"), NULL },

    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GError *error = NULL;

  g_set_prgname ("gtk4-image-tool show");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Show one or more images."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No image file specified\n"));
      exit (1);
    }

  show_files (filenames, decorated);

  g_strfreev (filenames);
}
