/* testfilechooser.c
 * Copyright (C) 2003  Red Hat, Inc.
 * Author: Owen Taylor
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
#  include <io.h>
#  define localtime_r(t,b) *(b) = *localtime (t)
#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif
#endif

#if 0
static GtkWidget *preview_label;
static GtkWidget *preview_image;
#endif
static GtkFileChooserAction action;

static void
print_current_folder (GtkFileChooser *chooser)
{
  GFile *cwd;

  cwd = gtk_file_chooser_get_current_folder (chooser);
  if (cwd != NULL)
    {
      char *uri = g_file_get_uri (cwd);
      g_print ("Current folder changed :\n  %s\n", uri ? uri : "(null)");
      g_free (uri);
      g_object_unref (cwd);
    }
  else
    {
      g_print ("Current folder changed :\n  none\n");
    }
}

static void
print_selected (GtkFileChooser *chooser)
{
  GSList *uris = gtk_file_chooser_get_files (chooser);
  GSList *tmp_list;

  g_print ("Selection changed :\n");
  for (tmp_list = uris; tmp_list; tmp_list = tmp_list->next)
    {
      GFile *file = tmp_list->data;
      char *uri = g_file_get_uri (file);
      g_print ("  %s\n", uri ? uri : "(null)");
      g_free (uri);
    }
  g_print ("\n");
  g_slist_free_full (uris, g_object_unref);
}

static void
response_cb (GtkDialog *dialog,
	     gint       response_id,
             gpointer   data)
{
  gboolean *done = data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GSList *list;

      list = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));

      if (list)
	{
	  GSList *l;

	  g_print ("Selected files:\n");

	  for (l = list; l; l = l->next)
	    {
              GFile *file = l->data;
              char *uri = g_file_get_uri (file);
	      g_print ("  %s\n", uri ? uri : "(null)");
	      g_free (uri);
	    }

	  g_slist_free_full (list, g_object_unref);
	}
      else
	g_print ("No selected files\n");
    }
  else
    g_print ("Dialog was closed\n");

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static gboolean
no_backup_files_filter (const GtkFileFilterInfo *filter_info,
			gpointer                 data)
{
  gsize len = filter_info->display_name ? strlen (filter_info->display_name) : 0;
  if (len > 0 && filter_info->display_name[len - 1] == '~')
    return 0;
  else
    return 1;
}

static void
filter_changed (GtkFileChooserDialog *dialog,
		gpointer              data)
{
  g_print ("file filter changed\n");
}

#include <stdio.h>
#include <errno.h>
#define _(s) (s)

static void
size_prepared_cb (GdkPixbufLoader *loader,
		  int              width,
		  int              height,
		  int             *data)
{
	int des_width = data[0];
	int des_height = data[1];

	if (des_height >= height && des_width >= width) {
		/* Nothing */
	} else if ((double)height * des_width > (double)width * des_height) {
		width = 0.5 + (double)width * des_height / (double)height;
		height = des_height;
	} else {
		height = 0.5 + (double)height * des_width / (double)width;
		width = des_width;
	}

	gdk_pixbuf_loader_set_size (loader, width, height);
}

GdkPixbuf *
my_new_from_file_at_size (const char *filename,
			  int         width,
			  int         height,
			  GError    **error)
{
	GdkPixbufLoader *loader;
	GdkPixbuf       *pixbuf;
	int              info[2];
	struct stat st;

	guchar buffer [4096];
	int length;
	FILE *f;

	g_return_val_if_fail (filename != NULL, NULL);
        g_return_val_if_fail (width > 0 && height > 0, NULL);

	if (stat (filename, &st) != 0) {
                int errsv = errno;

		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (errsv),
			     _("Could not get information for file '%s': %s"),
			     filename, g_strerror (errsv));
		return NULL;
	}

	if (!S_ISREG (st.st_mode))
		return NULL;

	f = fopen (filename, "rb");
	if (!f) {
                int errsv = errno;

                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errsv),
                             _("Failed to open file '%s': %s"),
                             filename, g_strerror (errsv));
		return NULL;
        }

	loader = gdk_pixbuf_loader_new ();
#ifdef DONT_PRESERVE_ASPECT
	gdk_pixbuf_loader_set_size (loader, width, height);
#else
	info[0] = width;
	info[1] = height;
	g_signal_connect (loader, "size-prepared", G_CALLBACK (size_prepared_cb), info);
#endif

	while (!feof (f)) {
		length = fread (buffer, 1, sizeof (buffer), f);
		if (length > 0)
			if (!gdk_pixbuf_loader_write (loader, buffer, length, error)) {
			        gdk_pixbuf_loader_close (loader, NULL);
				fclose (f);
				g_object_unref (loader);
				return NULL;
			}
	}

	fclose (f);

	g_assert (*error == NULL);
	if (!gdk_pixbuf_loader_close (loader, error)) {
		g_object_unref (loader);
		return NULL;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!pixbuf) {
		g_object_unref (loader);

		/* did the loader set an error? */
		if (*error != NULL)
			return NULL;

		g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Failed to load image '%s': reason not known, probably a corrupt image file"),
                             filename);
		return NULL;
	}

	g_object_ref (pixbuf);

	g_object_unref (loader);

	return pixbuf;
}

static void
set_current_folder (GtkFileChooser *chooser,
		    const char     *name)
{
  GFile *file = g_file_new_for_path (name);
  if (!gtk_file_chooser_set_current_folder (chooser, file, NULL))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (chooser),
				       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "Could not set the folder to %s",
				       name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }
  g_object_unref (file);
}

static void
set_folder_nonexistent_cb (GtkButton      *button,
			   GtkFileChooser *chooser)
{
  set_current_folder (chooser, "/nonexistent");
}

static void
set_folder_existing_nonexistent_cb (GtkButton      *button,
				    GtkFileChooser *chooser)
{
  set_current_folder (chooser, "/usr/nonexistent");
}

static void
set_filename (GtkFileChooser *chooser,
	      const char     *name)
{
  GFile *file = g_file_new_for_path (name);
  if (!gtk_file_chooser_set_file (chooser, file, NULL))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (chooser),
				       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "Could not select %s",
				       name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }
  g_object_unref (file);
}

static void
set_filename_nonexistent_cb (GtkButton      *button,
			     GtkFileChooser *chooser)
{
  set_filename (chooser, "/nonexistent");
}

static void
set_filename_existing_nonexistent_cb (GtkButton      *button,
				      GtkFileChooser *chooser)
{
  set_filename (chooser, "/usr/nonexistent");
}

static void
get_selection_cb (GtkButton      *button,
		  GtkFileChooser *chooser)
{
  GSList *selection;

  selection = gtk_file_chooser_get_files (chooser);

  g_print ("Selection: ");

  if (selection == NULL)
    g_print ("empty\n");
  else
    {
      GSList *l;
      
      for (l = selection; l; l = l->next)
	{
          GFile *file = l->data;
	  char *uri = g_file_get_uri (file);

	  g_print ("%s\n", uri);

          g_free (uri);

	  if (l->next)
	    g_print ("           ");
	}
    }

  g_slist_free_full (selection, g_object_unref);
}

static void
get_current_name_cb (GtkButton      *button,
		     GtkFileChooser *chooser)
{
  char *name;

  name = gtk_file_chooser_get_current_name (chooser);
  g_print ("Current name: %s\n", name ? name : "NULL");
  g_free (name);
}

static void
unmap_and_remap_cb (GtkButton *button,
		    GtkFileChooser *chooser)
{
  gtk_widget_hide (GTK_WIDGET (chooser));
  gtk_widget_show (GTK_WIDGET (chooser));
}

static void
kill_dependent (GtkWindow *win, GtkWidget *dep)
{
  gtk_widget_destroy (dep);
  g_object_unref (dep);
}

static void
notify_multiple_cb (GtkWidget  *dialog,
		    GParamSpec *pspec,
		    GtkWidget  *button)
{
  gboolean multiple;

  multiple = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (dialog));

  gtk_widget_set_sensitive (button, multiple);
}

int
main (int argc, char **argv)
{
  GtkWidget *control_window;
  GtkWidget *vbbox;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gboolean force_rtl = FALSE;
  gboolean multiple = FALSE;
  char *action_arg = NULL;
  char *initial_filename = NULL;
  char *initial_folder = NULL;
  GFile *file;
  GError *error = NULL;
  GOptionEntry options[] = {
    { "action", 'a', 0, G_OPTION_ARG_STRING, &action_arg, "Filechooser action", "ACTION" },
    { "multiple", 'm', 0, G_OPTION_ARG_NONE, &multiple, "Select multiple", NULL },
    { "right-to-left", 'r', 0, G_OPTION_ARG_NONE, &force_rtl, "Force right-to-left layout.", NULL },
    { "initial-filename", 'f', 0, G_OPTION_ARG_FILENAME, &initial_filename, "Initial filename to select", "FILENAME" },
    { "initial-folder", 'F', 0, G_OPTION_ARG_FILENAME, &initial_folder, "Initial folder to show", "FILENAME" },
    { NULL }
  };
  GOptionContext *context;
  gboolean done = FALSE;

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Failed to parse args: %s\n", error->message);
      g_error_free (error);
      return 1;
    }
  g_option_context_free (context);

  gtk_init ();

  if (initial_filename && initial_folder)
    {
      g_print ("Only one of --initial-filename and --initial-folder may be specified");
      return 1;
    }

  if (force_rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  action = GTK_FILE_CHOOSER_ACTION_OPEN;

  if (action_arg != NULL)
    {
      if (! strcmp ("open", action_arg))
	action = GTK_FILE_CHOOSER_ACTION_OPEN;
      else if (! strcmp ("save", action_arg))
	action = GTK_FILE_CHOOSER_ACTION_SAVE;
      else if (! strcmp ("select_folder", action_arg))
	action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      else
	{
	  g_print ("--action must be one of \"open\", \"save\", \"select_folder\"\n");
	  return 1;
	}

      g_free (action_arg);
    }

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			 "action", action,
			 "select-multiple", multiple,
			 NULL);

  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      gtk_window_set_title (GTK_WINDOW (dialog), "Select a file");
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      _("_Cancel"), GTK_RESPONSE_CANCEL,
			      _("_Open"), GTK_RESPONSE_OK,
			      NULL);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      gtk_window_set_title (GTK_WINDOW (dialog), "Save a file");
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      _("_Cancel"), GTK_RESPONSE_CANCEL,
			      _("_Save"), GTK_RESPONSE_OK,
			      NULL);
      break;
    }
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect (dialog, "selection-changed",
		    G_CALLBACK (print_selected), NULL);
  g_signal_connect (dialog, "current-folder-changed",
		    G_CALLBACK (print_current_folder), NULL);
  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb), &done);

  /* Filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All Files");
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  /* Make this filter the default */
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "No backup files");
  gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_DISPLAY_NAME,
			      no_backup_files_filter, NULL, NULL);
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Starts with D");
  gtk_file_filter_add_pattern (filter, "D*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  g_signal_connect (dialog, "notify::filter",
		    G_CALLBACK (filter_changed), NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "PNG and JPEG");
  gtk_file_filter_add_mime_type (filter, "image/jpeg");
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Images");
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  /* Choices */

  gtk_file_chooser_add_choice (GTK_FILE_CHOOSER (dialog), "choice1",
                               "Choose one:",
                               (const char *[]){"one", "two", "three", NULL},
                               (const char *[]){"One", "Two", "Three", NULL});
  gtk_file_chooser_set_choice (GTK_FILE_CHOOSER (dialog), "choice1", "two");

  /* Shortcuts */

  file = g_file_new_for_uri ("file:///usr/share/pixmaps");
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), file, NULL);
  g_object_unref (file);

  file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_MUSIC));
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), file, NULL);
  g_object_unref (file);

  /* Initial filename or folder */

  if (initial_filename)
    set_filename (GTK_FILE_CHOOSER (dialog), initial_filename);

  if (initial_folder)
    set_current_folder (GTK_FILE_CHOOSER (dialog), initial_folder);

  /* show_all() to reveal bugs in composite widget handling */
  gtk_widget_show (dialog);

  /* Extra controls for manipulating the test environment
   */

  control_window = gtk_window_new ();

  vbbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (control_window), vbbox);

  button = gtk_button_new_with_mnemonic ("_Select all");
  gtk_widget_set_sensitive (button, multiple);
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_file_chooser_select_all), dialog);
  g_signal_connect (dialog, "notify::select-multiple",
		    G_CALLBACK (notify_multiple_cb), button);

  button = gtk_button_new_with_mnemonic ("_Unselect all");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_file_chooser_unselect_all), dialog);

  button = gtk_button_new_with_label ("set_current_folder (\"/nonexistent\")");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_folder_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("set_current_folder (\"/usr/nonexistent\")");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_folder_existing_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("set_filename (\"/nonexistent\")");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_filename_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("set_filename (\"/usr/nonexistent\")");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_filename_existing_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("Get selection");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (get_selection_cb), dialog);

  button = gtk_button_new_with_label ("Get current name");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (get_current_name_cb), dialog);

  button = gtk_button_new_with_label ("Unmap and remap");
  gtk_container_add (GTK_CONTAINER (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (unmap_and_remap_cb), dialog);

  gtk_widget_show (control_window);

  g_object_ref (control_window);
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (kill_dependent), control_window);

  /* We need to hold a ref until we have destroyed the widgets, just in case
   * someone else destroys them.  We explicitly destroy windows to catch leaks.
   */
  g_object_ref (dialog);
  while (!done)
    g_main_context_iteration (NULL, TRUE);
  gtk_widget_destroy (dialog);
  g_object_unref (dialog);

  return 0;
}
