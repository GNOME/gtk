/* testrecentchooser.c
 * Copyright (C) 2006  Emmanuele Bassi.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
# include <io.h>
# define localtime_r(t,b) *(b) = localtime (t)
# ifndef S_ISREG
#  define S_ISREG(m) ((m) & _S_IFREG)
# endif
#endif

#include "prop-editor.h"

static void
print_current_item (GtkRecentChooser *chooser)
{
  gchar *uri;

  uri = gtk_recent_chooser_get_current_uri (chooser);
  g_print ("Current item changed :\n  %s\n", uri ? uri : "null");
  g_free (uri);
}

static void
print_selected (GtkRecentChooser *chooser)
{
  gsize uris_len, i;
  gchar **uris = gtk_recent_chooser_get_uris (chooser, &uris_len);

  g_print ("Selection changed :\n");
  for (i = 0; i < uris_len; i++)
    g_print ("  %s\n", uris[i]);
  g_print ("\n");

  g_strfreev (uris);
}

static void
response_cb (GtkDialog *dialog,
	     gint       response_id)
{
  if (response_id == GTK_RESPONSE_OK)
    {
    }
  else
    g_print ("Dialog was closed\n");

  gtk_main_quit ();
}

static void
filter_changed (GtkRecentChooserDialog *dialog,
		gpointer                data)
{
  g_print ("recent filter changed\n");
}

static void
notify_multiple_cb (GtkWidget  *dialog,
		    GParamSpec *pspec,
		    GtkWidget  *button)
{
  gboolean multiple;

  multiple = gtk_recent_chooser_get_select_multiple (GTK_RECENT_CHOOSER (dialog));

  gtk_widget_set_sensitive (button, multiple);
}

static void
kill_dependent (GtkWindow *win,
		GtkWidget *dep)
{
  gtk_widget_destroy (dep);
  g_object_unref (dep);
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *control_window;
  GtkWidget *vbbox;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkRecentFilter *filter;
  gint i;
  gboolean multiple = FALSE;
  
  gtk_init (&argc, &argv);

  /* to test rtl layout, set RTL=1 in the environment */
  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  for (i = 1; i < argc; i++)
    {
      if (!strcmp ("--multiple", argv[i]))
	multiple = TRUE;
    }

  dialog = g_object_new (GTK_TYPE_RECENT_CHOOSER_DIALOG,
		         "select-multiple", multiple,
                         "show-tips", TRUE,
                         "show-icons", TRUE,
			 NULL);
  gtk_window_set_title (GTK_WINDOW (dialog), "Select a file");
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
		  	  "_Cancel", GTK_RESPONSE_CANCEL,
			  "_Open", GTK_RESPONSE_OK,
			  NULL);
  
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect (dialog, "item-activated",
		    G_CALLBACK (print_current_item), NULL);
  g_signal_connect (dialog, "selection-changed",
		    G_CALLBACK (print_selected), NULL);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb), NULL);
  
  /* filters */
  filter = gtk_recent_filter_new ();
  gtk_recent_filter_set_name (filter, "All Files");
  gtk_recent_filter_add_pattern (filter, "*");
  gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (dialog), filter);

  filter = gtk_recent_filter_new ();
  gtk_recent_filter_set_name (filter, "Only PDF Files");
  gtk_recent_filter_add_mime_type (filter, "application/pdf");
  gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (dialog), filter);

  g_signal_connect (dialog, "notify::filter",
		    G_CALLBACK (filter_changed), NULL);

  gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (dialog), filter);

  filter = gtk_recent_filter_new ();
  gtk_recent_filter_set_name (filter, "PNG and JPEG");
  gtk_recent_filter_add_mime_type (filter, "image/png");
  gtk_recent_filter_add_mime_type (filter, "image/jpeg");
  gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (dialog), filter);

  gtk_widget_show_all (dialog);

  create_prop_editor (G_OBJECT (dialog), GTK_TYPE_RECENT_CHOOSER);

  control_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  vbbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (control_window), vbbox);

  button = gtk_button_new_with_mnemonic ("_Select all");
  gtk_widget_set_sensitive (button, multiple);
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect_swapped (button, "clicked",
		            G_CALLBACK (gtk_recent_chooser_select_all), dialog);
  g_signal_connect (dialog, "notify::select-multiple",
		    G_CALLBACK (notify_multiple_cb), button);

  button = gtk_button_new_with_mnemonic ("_Unselect all");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect_swapped (button, "clicked",
		            G_CALLBACK (gtk_recent_chooser_unselect_all), dialog);

  gtk_widget_show_all (control_window);
  
  g_object_ref (control_window);
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (kill_dependent), control_window);
  
  g_object_ref (dialog);
  gtk_main ();
  gtk_widget_destroy (dialog);
  g_object_unref (dialog);

  return 0;
}
