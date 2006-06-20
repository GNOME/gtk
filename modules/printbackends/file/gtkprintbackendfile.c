/* GTK - The GIMP Toolkit
 * gtkprintbackendpdf.c: Default implementation of GtkPrintBackend 
 * for printing to a file
 * Copyright (C) 2003, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <cairo.h>
#include <cairo-pdf.h>

#include <glib/gi18n-lib.h>

#include "gtkprintoperation.h"

#include "gtkprintbackend.h"
#include "gtkprintbackendfile.h"

#include "gtkprinter.h"
#include "gtkprinter-private.h"

typedef struct _GtkPrintBackendFileClass GtkPrintBackendFileClass;

#define GTK_PRINT_BACKEND_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_BACKEND_FILE, GtkPrintBackendFileClass))
#define GTK_IS_PRINT_BACKEND_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_BACKEND_FILE))
#define GTK_PRINT_BACKEND_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_BACKEND_FILE, GtkPrintBackendFileClass))

#define _STREAM_MAX_CHUNK_SIZE 8192

static GType print_backend_file_type = 0;

struct _GtkPrintBackendFileClass
{
  GtkPrintBackendClass parent_class;
};

struct _GtkPrintBackendFile
{
  GtkPrintBackend parent_instance;
};

static GObjectClass *backend_parent_class;

static void                 gtk_print_backend_file_class_init      (GtkPrintBackendFileClass *class);
static void                 gtk_print_backend_file_init            (GtkPrintBackendFile      *impl);
static void                 file_printer_get_settings_from_options (GtkPrinter              *printer,
								    GtkPrinterOptionSet     *options,
								    GtkPrintSettings        *settings);
static GtkPrinterOptionSet *file_printer_get_options               (GtkPrinter              *printer,
								    GtkPrintSettings        *settings,
								    GtkPageSetup            *page_setup,
								    GtkPrintCapabilities     capabilities);
static void                 file_printer_prepare_for_print         (GtkPrinter              *printer,
								    GtkPrintJob             *print_job,
								    GtkPrintSettings        *settings,
								    GtkPageSetup            *page_setup);
static void                 gtk_print_backend_file_print_stream     (GtkPrintBackend         *print_backend,
								    GtkPrintJob             *job,
								    gint                     data_fd,
								    GtkPrintJobCompleteFunc  callback,
								    gpointer                 user_data,
								    GDestroyNotify           dnotify);
static cairo_surface_t *    file_printer_create_cairo_surface      (GtkPrinter              *printer,
								    GtkPrintSettings        *settings,
								    gdouble                  width,
								    gdouble                  height,
								    gint                     cache_fd);

static void
gtk_print_backend_file_register_type (GTypeModule *module)
{
  static const GTypeInfo print_backend_file_info =
  {
    sizeof (GtkPrintBackendFileClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_print_backend_file_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkPrintBackendFile),
    0,		/* n_preallocs */
    (GInstanceInitFunc) gtk_print_backend_file_init,
  };

  print_backend_file_type = g_type_module_register_type (module,
                                                         GTK_TYPE_PRINT_BACKEND,
                                                         "GtkPrintBackendFile",
                                                         &print_backend_file_info, 0);
}

G_MODULE_EXPORT void 
pb_module_init (GTypeModule *module)
{
  gtk_print_backend_file_register_type (module);
}

G_MODULE_EXPORT void 
pb_module_exit (void)
{

}
  
G_MODULE_EXPORT GtkPrintBackend * 
pb_module_create (void)
{
  return gtk_print_backend_file_new ();
}

/*
 * GtkPrintBackendFile
 */
GType
gtk_print_backend_file_get_type (void)
{
  return print_backend_file_type;
}

/**
 * gtk_print_backend_file_new:
 *
 * Creates a new #GtkPrintBackendFile object. #GtkPrintBackendFile
 * implements the #GtkPrintBackend interface with direct access to
 * the filesystem using Unix/Linux API calls
 *
 * Return value: the new #GtkPrintBackendFile object
 **/
GtkPrintBackend *
gtk_print_backend_file_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_BACKEND_FILE, NULL);
}

static void
gtk_print_backend_file_class_init (GtkPrintBackendFileClass *class)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_CLASS (class);

  backend_parent_class = g_type_class_peek_parent (class);

  backend_class->print_stream = gtk_print_backend_file_print_stream;
  backend_class->printer_create_cairo_surface = file_printer_create_cairo_surface;
  backend_class->printer_get_options = file_printer_get_options;
  backend_class->printer_get_settings_from_options = file_printer_get_settings_from_options;
  backend_class->printer_prepare_for_print = file_printer_prepare_for_print;
}

static cairo_status_t
_cairo_write (void                *closure,
              const unsigned char *data,
              unsigned int         length)
{
  gint fd = GPOINTER_TO_INT (closure);
  gssize written;
  
  while (length > 0) 
    {
      written = write (fd, data, length);

      if (written == -1)
	{
	  if (errno == EAGAIN || errno == EINTR)
	    continue;
	  
	  return CAIRO_STATUS_WRITE_ERROR;
	}    

      data += written;
      length -= written;
    }

  return CAIRO_STATUS_SUCCESS;
}


static cairo_surface_t *
file_printer_create_cairo_surface (GtkPrinter       *printer,
				   GtkPrintSettings *settings,
				   gdouble           width, 
				   gdouble           height,
				   gint              cache_fd)
{
  cairo_surface_t *surface;
  
  surface = cairo_pdf_surface_create_for_stream  (_cairo_write, GINT_TO_POINTER (cache_fd), width, height);

  /* TODO: DPI from settings object? */
  cairo_surface_set_fallback_resolution (surface, 300, 300);

  return surface;
}

typedef struct {
  GtkPrintBackend *backend;
  GtkPrintJobCompleteFunc callback;
  GtkPrintJob *job;
  gint target_fd;
  gpointer user_data;
  GDestroyNotify dnotify;
} _PrintStreamData;

static void
file_print_cb (GtkPrintBackendFile *print_backend,
               GError              *error,
               gpointer            user_data)
{
  _PrintStreamData *ps = (_PrintStreamData *) user_data;

  if (ps->target_fd > 0)
    close (ps->target_fd);

  if (ps->callback)
    ps->callback (ps->job, ps->user_data, error);

  if (ps->dnotify)
    ps->dnotify (ps->user_data);

  gtk_print_job_set_status (ps->job,
			    (error != NULL)?GTK_PRINT_STATUS_FINISHED_ABORTED:GTK_PRINT_STATUS_FINISHED);

  if (ps->job)
    g_object_unref (ps->job);
 
  g_free (ps);
}

static gboolean
file_write (GIOChannel   *source,
            GIOCondition  con,
            gpointer      user_data)
{
  gchar buf[_STREAM_MAX_CHUNK_SIZE];
  gsize bytes_read;
  GError *error;
  _PrintStreamData *ps = (_PrintStreamData *) user_data;
  gint source_fd;

  error = NULL;

  source_fd = g_io_channel_unix_get_fd (source);

  bytes_read = read (source_fd,
                     buf,
                     _STREAM_MAX_CHUNK_SIZE);

  if (bytes_read > 0)
    {
      if (write (ps->target_fd, buf, bytes_read) == -1)
        {
          error = g_error_new (GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_INTERNAL_ERROR, 
                           g_strerror (errno));
        }
    }
  else if (bytes_read == -1)
    {
      error = g_error_new (GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_INTERNAL_ERROR, 
                           g_strerror (errno));
    }

  if (bytes_read == 0 || error != NULL)
    {
      file_print_cb (GTK_PRINT_BACKEND_FILE (ps->backend), error, user_data);

      return FALSE;
    }

  return TRUE;
}

static void
gtk_print_backend_file_print_stream (GtkPrintBackend        *print_backend,
				    GtkPrintJob            *job,
				    gint                    data_fd,
				    GtkPrintJobCompleteFunc callback,
				    gpointer                user_data,
				    GDestroyNotify          dnotify)
{
  GError *error;
  GtkPrinter *printer;
  _PrintStreamData *ps;
  GtkPrintSettings *settings;
  GIOChannel *save_channel;  
  const gchar *uri;
  gchar *filename = NULL; /* quit gcc */

  printer = gtk_print_job_get_printer (job);
  settings = gtk_print_job_get_settings (job);

  error = NULL;

  uri = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_URI);
  if (uri)
    filename = g_filename_from_uri (uri, NULL, NULL);
  /* FIXME: shouldn't we error out if we get an URI we cannot handle,
   * rather than to print to some random file somewhere?
   */
  if (filename == NULL)
    filename = g_strdup_printf ("output.pdf");
  
  ps = g_new0 (_PrintStreamData, 1);
  ps->callback = callback;
  ps->user_data = user_data;
  ps->dnotify = dnotify;
  ps->job = g_object_ref (job);
  ps->backend = print_backend;

  ps->target_fd = creat (filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  g_free (filename);

  if (ps->target_fd == -1)
    {
      error = g_error_new (GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_INTERNAL_ERROR, 
                           g_strerror (errno));

      file_print_cb (GTK_PRINT_BACKEND_FILE (print_backend), error, ps);

      return;
    }
  
  save_channel = g_io_channel_unix_new (data_fd);

  g_io_add_watch (save_channel, 
                  G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
                  (GIOFunc) file_write,
                  ps);
}

static void
gtk_print_backend_file_init (GtkPrintBackendFile *backend)
{
  GtkPrinter *printer;
  
  printer = g_object_new (GTK_TYPE_PRINTER,
			  "name", _("Print to File"),
			  "backend", backend,
			  "is-virtual", TRUE,
			  "accepts-ps", FALSE,
			  NULL); 

  gtk_printer_set_has_details (printer, TRUE);
  gtk_printer_set_icon_name (printer, "gtk-floppy");
  gtk_printer_set_is_active (printer, TRUE);

  gtk_print_backend_add_printer (GTK_PRINT_BACKEND (backend), printer);
  g_object_unref (printer);

  gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (backend));
}

static GtkPrinterOptionSet *
file_printer_get_options (GtkPrinter           *printer,
			  GtkPrintSettings     *settings,
			  GtkPageSetup         *page_setup,
			  GtkPrintCapabilities  capabilities)
{
  GtkPrinterOptionSet *set;
  GtkPrinterOption *option;
  const char *uri;
  char *n_up[] = {"1" };

  set = gtk_printer_option_set_new ();

  option = gtk_printer_option_new ("gtk-n-up", _("Pages Per Sheet"), GTK_PRINTER_OPTION_TYPE_PICKONE);
  gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (n_up),
					 n_up, n_up);
  gtk_printer_option_set (option, "1");
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  option = gtk_printer_option_new ("gtk-main-page-custom-input", _("File"), GTK_PRINTER_OPTION_TYPE_FILESAVE);
  gtk_printer_option_set (option, "output.pdf");
  option->group = g_strdup ("GtkPrintDialogExtension");
  gtk_printer_option_set_add (set, option);

  if (settings != NULL &&
      (uri = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_URI))!= NULL)
    gtk_printer_option_set (option, uri);

  return set;
}

static void
file_printer_get_settings_from_options (GtkPrinter          *printer,
					GtkPrinterOptionSet *options,
					GtkPrintSettings    *settings)
{
  GtkPrinterOption *option;

  option = gtk_printer_option_set_lookup (options, "gtk-main-page-custom-input");
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_URI, option->value);
}

static void
file_printer_prepare_for_print (GtkPrinter       *printer,
				GtkPrintJob      *print_job,
				GtkPrintSettings *settings,
				GtkPageSetup     *page_setup)
{
  gdouble scale;

  print_job->print_pages = gtk_print_settings_get_print_pages (settings);
  print_job->page_ranges = NULL;
  print_job->num_page_ranges = 0;
  
  if (print_job->print_pages == GTK_PRINT_PAGES_RANGES)
    print_job->page_ranges =
      gtk_print_settings_get_page_ranges (settings,
					  &print_job->num_page_ranges);
  
  print_job->collate = gtk_print_settings_get_collate (settings);
  print_job->reverse = gtk_print_settings_get_reverse (settings);
  print_job->num_copies = gtk_print_settings_get_n_copies (settings);

  scale = gtk_print_settings_get_scale (settings);
  if (scale != 100.0)
    print_job->scale = scale/100.0;

  print_job->page_set = gtk_print_settings_get_page_set (settings);
  print_job->rotate_to_orientation = TRUE;
}
