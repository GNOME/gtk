#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include "gtkfilechooserdialog.h"
#include "gtkfilechooser.h"
#include "prop-editor.h"

#ifdef USE_GNOME_VFS
#include "gtkfilesystemgnomevfs.h"
#else
#include "gtkfilesystemunix.h"
#endif

static GtkWidget *preview_label;
static GtkWidget *preview_image;

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

static gboolean
no_backup_files_filter (const GtkFileFilterInfo *filter_info,
			gpointer                 data)
{
  gsize len = strlen (filter_info->display_name);
  if (len > 0 && filter_info->display_name[len - 1] == '~')
    return 0;
  else
    return 1;
}

static char *
format_time (time_t t)
{
  gchar buf[128];
  struct tm tm_buf;
  time_t now = time (NULL);
  const char *format;

  if (abs (now - t) < 24*60*60)
    format = "%X";
  else
    format = "%x";

  localtime_r (&t, &tm_buf);
  if (strftime (buf, sizeof (buf), format, &tm_buf) == 0)
    return g_strdup ("<unknown>");
  else
    return g_strdup (buf);
}

static char *
format_size (gint64 size)
{
  if (size < (gint64)1024)
    return g_strdup_printf ("%d bytes", (gint)size);
  else if (size < (gint64)1024*1024)
    return g_strdup_printf ("%.1f K", size / (1024.));
  else if (size < (gint64)1024*1024*1024)
    return g_strdup_printf ("%.1f M", size / (1024.*1024.));
  else
    return g_strdup_printf ("%.1f G", size / (1024.*1024.*1024.));
}

#include <stdio.h>
#include <errno.h>
#define _(s) (s)

static void
size_prepared_cb (GdkPixbufLoader *loader, 
		  int              width,
		  int              height,
		  gpointer         data)
{
	struct {
		int width;
		int height;
	} *info = data;

	if ((double)height * (double)info->width >
	    (double)width * (double)info->height) {
		width = 0.5 + (double)width * (double)info->height / (double)height;
		height = info->height;
	} else {
		height = 0.5 + (double)height * (double)info->width / (double)width;
		width = info->width;
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
	struct {
		int width;
		int height;
	} info;
	struct stat st;

	guchar buffer [4096];
	int length;
	FILE *f;

	g_return_val_if_fail (filename != NULL, NULL);
        g_return_val_if_fail (width > 0 && height > 0, NULL);

	if (stat (filename, &st) != 0) {
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (errno),
			     _("Could not get information for file '%s': %s"),
			     filename, g_strerror (errno));
		return NULL;
	}

	if (!S_ISREG (st.st_mode))
		return NULL;

	f = fopen (filename, "rb");
	if (!f) {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             _("Failed to open file '%s': %s"),
                             filename, g_strerror (errno));
		return NULL;
        }
	
	loader = gdk_pixbuf_loader_new ();
#ifdef DONT_PRESERVE_ASPECT
	gdk_pixbuf_loader_set_size (loader, width, height);
#else
	info.width = width;
	info.height = height;
	g_signal_connect (loader, "size-prepared", G_CALLBACK (&size_prepared_cb), &info);
#endif	

	while (!feof (f)) {
		length = fread (buffer, 1, sizeof (buffer), f);
		if (length > 0)
			if (!gdk_pixbuf_loader_write (loader, buffer, length, error)) {
			        gdk_pixbuf_loader_close (loader, NULL);
				fclose (f);
				g_object_unref (G_OBJECT (loader));
				return NULL;
			}
	}

	fclose (f);

	if (!gdk_pixbuf_loader_close (loader, error)) {
		g_object_unref (G_OBJECT (loader));
		return NULL;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!pixbuf) {
		g_object_unref (G_OBJECT (loader));
		g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             _("Failed to load image '%s': reason not known, probably a corrupt image file"),
                             filename);
		return NULL;
	}

	g_object_ref (pixbuf);

	g_object_unref (G_OBJECT (loader));

	return pixbuf;
}

static void
update_preview_cb (GtkFileChooser *chooser)
{
  gchar *filename = gtk_file_chooser_get_preview_filename (chooser);
  gboolean have_preview = FALSE;
  
  if (filename)
    {
      GdkPixbuf *pixbuf;
      GError *error = NULL;

      pixbuf = my_new_from_file_at_size (filename, 128, 128, &error);
      if (pixbuf)
	{
	  gtk_image_set_from_pixbuf (GTK_IMAGE (preview_image), pixbuf);
	  g_object_unref (pixbuf);
	  gtk_widget_show (preview_image);
	  gtk_widget_hide (preview_label);
	  have_preview = TRUE;
	}
      else
	{
	  struct stat buf;
	  if (stat (filename, &buf) == 0)
	    {
	      gchar *preview_text;
	      gchar *size_str;
	      gchar *modified_time;
	      
	      size_str = format_size (buf.st_size);
	      modified_time = format_time (buf.st_mtime);
	      
	      preview_text = g_strdup_printf ("<i>Modified:</i>\t%s\n"
					      "<i>Size:</i>\t%s\n",
					      modified_time,
					      size_str);
	      gtk_label_set_markup (GTK_LABEL (preview_label), preview_text);
	      g_free (modified_time);
	      g_free (size_str);
	      g_free (preview_text);
	      
	      gtk_widget_hide (preview_image);
	      gtk_widget_show (preview_label);
	      have_preview = TRUE;
	    }
	}
      
      g_free (filename);
    }

  gtk_file_chooser_set_preview_widget_active (chooser, have_preview);
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
  GtkFileFilter *filter;
  GtkWidget *preview_vbox;
  GtkWidget *extra;
  
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

  /* Filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All Files");
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "No backup files");
  gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_DISPLAY_NAME,
			      no_backup_files_filter, NULL, NULL);
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  /* Make this filter the default */
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "PNG and JPEG");
  gtk_file_filter_add_mime_type (filter, "image/jpeg");
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  /* Preview widget */
  preview_vbox = gtk_vbox_new (0, FALSE);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog), preview_vbox);
  
  preview_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (preview_vbox), preview_label, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (preview_label), 6, 6);
  
  preview_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (preview_vbox), preview_image, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (preview_image), 6, 6);
  
  update_preview_cb (GTK_FILE_CHOOSER (dialog));
  g_signal_connect (dialog, "update-preview",
		    G_CALLBACK (update_preview_cb), NULL);

  /* Extra widget */
  extra = gtk_check_button_new_with_mnemonic ("_Frobnicate the file");
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), extra);
  
  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 400);
  /* show_all() to reveal bugs in composite widget handling */
  gtk_widget_show_all (dialog);

  /* Extra controls for manipulating the test environment
   */
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
