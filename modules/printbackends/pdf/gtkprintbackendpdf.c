/* GTK - The GIMP Toolkit
 * gtkprintbackendpdf.c: Default implementation of GtkPrintBackend 
 * for printing to PDF files
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <errno.h>
#include <cairo.h>
#include <cairo-pdf.h>

#include <glib/gi18n-lib.h>

#include "gtkprintoperation.h"

#include "gtkprintbackend.h"
#include "gtkprintbackendpdf.h"

#include "gtkprinter.h"

typedef struct _GtkPrintBackendPdfClass GtkPrintBackendPdfClass;

#define GTK_PRINT_BACKEND_PDF_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_BACKEND_PDF, GtkPrintBackendPdfClass))
#define GTK_IS_PRINT_BACKEND_PDF_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_BACKEND_PDF))
#define GTK_PRINT_BACKEND_PDF_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_BACKEND_PDF, GtkPrintBackendPdfClass))

#define _PDF_MAX_CHUNK_SIZE 8192

static GType print_backend_pdf_type = 0;

struct _GtkPrintBackendPdfClass
{
  GtkPrintBackendClass parent_class;
};

struct _GtkPrintBackendPdf
{
  GtkPrintBackend parent_instance;
};

static GObjectClass *backend_parent_class;

static void                 gtk_print_backend_pdf_class_init      (GtkPrintBackendPdfClass *class);
static void                 gtk_print_backend_pdf_init            (GtkPrintBackendPdf      *impl);
static void                 pdf_printer_get_settings_from_options (GtkPrinter              *printer,
								   GtkPrinterOptionSet     *options,
								   GtkPrintSettings        *settings);
static gboolean             pdf_printer_mark_conflicts            (GtkPrinter              *printer,
								   GtkPrinterOptionSet     *options);
static GtkPrinterOptionSet *pdf_printer_get_options               (GtkPrinter              *printer,
								   GtkPrintSettings        *settings,
								   GtkPageSetup            *page_setup);
static void                 pdf_printer_prepare_for_print         (GtkPrinter              *printer,
								   GtkPrintJob             *print_job,
								   GtkPrintSettings        *settings,
								   GtkPageSetup            *page_setup);
static void                 pdf_printer_get_hard_margins          (GtkPrinter              *printer,
								   double                  *top,
								   double                  *bottom,
								   double                  *left,
								   double                  *right);
static void                 pdf_printer_request_details           (GtkPrinter              *printer);
static GList *              pdf_printer_list_papers               (GtkPrinter              *printer);
static void                 gtk_print_backend_pdf_print_stream    (GtkPrintBackend         *print_backend,
								   GtkPrintJob             *job,
								   gint                     data_fd,
								   GtkPrintJobCompleteFunc  callback,
								   gpointer                 user_data,
								   GDestroyNotify           dnotify);
static cairo_surface_t *    pdf_printer_create_cairo_surface      (GtkPrinter              *printer,
								   gdouble                  width,
								   gdouble                  height,
								   gint                     cache_fd);

static void
gtk_print_backend_pdf_register_type (GTypeModule *module)
{
  static const GTypeInfo print_backend_pdf_info =
  {
    sizeof (GtkPrintBackendPdfClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_print_backend_pdf_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkPrintBackendPdf),
    0,		/* n_preallocs */
    (GInstanceInitFunc) gtk_print_backend_pdf_init,
  };

  print_backend_pdf_type = g_type_module_register_type (module,
                                                        GTK_TYPE_PRINT_BACKEND,
                                                        "GtkPrintBackendPdf",
                                                        &print_backend_pdf_info, 0);
}

G_MODULE_EXPORT void 
pb_module_init (GTypeModule *module)
{
  gtk_print_backend_pdf_register_type (module);
}

G_MODULE_EXPORT void 
pb_module_exit (void)
{

}
  
G_MODULE_EXPORT GtkPrintBackend * 
pb_module_create (void)
{
  return gtk_print_backend_pdf_new ();
}

/*
 * GtkPrintBackendPdf
 */
GType
gtk_print_backend_pdf_get_type (void)
{
  return print_backend_pdf_type;
}

/**
 * gtk_print_backend_pdf_new:
 *
 * Creates a new #GtkPrintBackendPdf object. #GtkPrintBackendPdf
 * implements the #GtkPrintBackend interface with direct access to
 * the filesystem using Unix/Linux API calls
 *
 * Return value: the new #GtkPrintBackendPdf object
 **/
GtkPrintBackend *
gtk_print_backend_pdf_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_BACKEND_PDF, NULL);
}

static void
gtk_print_backend_pdf_class_init (GtkPrintBackendPdfClass *class)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_CLASS (class);

  backend_parent_class = g_type_class_peek_parent (class);

  backend_class->print_stream = gtk_print_backend_pdf_print_stream;
  backend_class->printer_request_details = pdf_printer_request_details;
  backend_class->printer_create_cairo_surface = pdf_printer_create_cairo_surface;
  backend_class->printer_get_options = pdf_printer_get_options;
  backend_class->printer_mark_conflicts = pdf_printer_mark_conflicts;
  backend_class->printer_get_settings_from_options = pdf_printer_get_settings_from_options;
  backend_class->printer_prepare_for_print = pdf_printer_prepare_for_print;
  backend_class->printer_list_papers = pdf_printer_list_papers;
  backend_class->printer_get_hard_margins = pdf_printer_get_hard_margins;
}

static cairo_status_t
_cairo_write (void *cache_fd_as_pointer,
              const unsigned char *data,
              unsigned int         length)
{
  cairo_status_t result;
  gint cache_fd;
  cache_fd = GPOINTER_TO_INT (cache_fd_as_pointer);
  
  result = CAIRO_STATUS_WRITE_ERROR;
  
  /* write out the buffer */
  if (write (cache_fd, data, length) != -1)
      result = CAIRO_STATUS_SUCCESS;
   
  return result;
}


static cairo_surface_t *
pdf_printer_create_cairo_surface (GtkPrinter *printer,
				   gdouble width, 
				   gdouble height,
				   gint cache_fd)
{
  cairo_surface_t *surface;
  
  surface = cairo_pdf_surface_create_for_stream  (_cairo_write, GINT_TO_POINTER (cache_fd), width, height);

  /* TODO: DPI from settings object? */
  cairo_pdf_surface_set_dpi (surface, 300, 300);

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
pdf_print_cb (GtkPrintBackendPdf *print_backend,
              GError *error,
              gpointer user_data)
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
pdf_write (GIOChannel *source,
           GIOCondition con,
           gpointer user_data)
{
  gchar buf[_PDF_MAX_CHUNK_SIZE];
  gsize bytes_read;
  GError *error;
  _PrintStreamData *ps = (_PrintStreamData *) user_data;
  gint source_fd;

  error = NULL;

  source_fd = g_io_channel_unix_get_fd (source);

  bytes_read = read (source_fd,
                     buf,
                     _PDF_MAX_CHUNK_SIZE);

   

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
      pdf_print_cb (GTK_PRINT_BACKEND_PDF (ps->backend), error, user_data);

      return FALSE;
    }

  return TRUE;
}

static void
gtk_print_backend_pdf_print_stream (GtkPrintBackend *print_backend,
				    GtkPrintJob *job,
				    gint data_fd,
				    GtkPrintJobCompleteFunc callback,
				    gpointer user_data,
				    GDestroyNotify dnotify)
{
  GError *error;
  GtkPrinter *printer;
  _PrintStreamData *ps;
  GtkPrintSettings *settings;
  GIOChannel *save_channel;  
  const char *filename;

  printer = gtk_print_job_get_printer (job);
  settings = gtk_print_job_get_settings (job);

  error = NULL;

  filename = gtk_print_settings_get (settings, "pdf-filename");
  if (filename == NULL)
    filename = "output.pdf";
  
  ps = g_new0 (_PrintStreamData, 1);
  ps->callback = callback;
  ps->user_data = user_data;
  ps->job = g_object_ref (job);

  ps->target_fd = creat (filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  ps->backend = print_backend;

  if (ps->target_fd == -1)
    {
      error = g_error_new (GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_INTERNAL_ERROR, 
                           g_strerror (errno));

      pdf_print_cb (GTK_PRINT_BACKEND_PDF (print_backend),
                    error,
                    ps);

      return;
    }
  
  save_channel = g_io_channel_unix_new (data_fd);

  g_io_add_watch (save_channel, 
                  G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
                  (GIOFunc) pdf_write,
                  ps);
}

static void
gtk_print_backend_pdf_init (GtkPrintBackendPdf *backend)
{
  GtkPrinter *printer;
  
  printer = gtk_printer_new (_("Print to PDF"),
			     GTK_PRINT_BACKEND (backend),
			     TRUE); 

  gtk_printer_set_has_details (printer, TRUE);
  gtk_printer_set_icon_name (printer, "floppy");
  gtk_printer_set_is_active (printer, TRUE);

  gtk_print_backend_add_printer (GTK_PRINT_BACKEND (backend), printer);
  g_object_unref (printer);

  gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (backend));
}

static void
pdf_printer_request_details (GtkPrinter *printer)
{
}

static GtkPrinterOptionSet *
pdf_printer_get_options (GtkPrinter *printer,
			 GtkPrintSettings *settings,
			 GtkPageSetup *page_setup)
{
  GtkPrinterOptionSet *set;
  GtkPrinterOption *option;
  const char *filename;
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
  option->group = g_strdup ("GtkPrintDialogExtention");
  gtk_printer_option_set_add (set, option);

  if (settings != NULL &&
      (filename = gtk_print_settings_get (settings, "pdf-filename"))!= NULL)
    gtk_printer_option_set (option, filename);

  return set;
}


static gboolean
pdf_printer_mark_conflicts  (GtkPrinter          *printer,
			     GtkPrinterOptionSet *options)
{
  return FALSE;
}

static void
pdf_printer_get_settings_from_options (GtkPrinter *printer,
				       GtkPrinterOptionSet *options,
				       GtkPrintSettings *settings)
{
  GtkPrinterOption *option;

  option = gtk_printer_option_set_lookup (options, "gtk-main-page-custom-input");
  gtk_print_settings_set (settings, "pdf-filename", option->value);
}

static void
pdf_printer_prepare_for_print (GtkPrinter *printer,
			       GtkPrintJob *print_job,
			       GtkPrintSettings *settings,
			       GtkPageSetup *page_setup)
{
  double scale;

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

static void
pdf_printer_get_hard_margins (GtkPrinter *printer,
                              double *top,
                              double *bottom,
                              double *left,
                              double *right)
{
  *top = 0;
  *bottom = 0;
  *left = 0;
  *right = 0;
}

static GList *
pdf_printer_list_papers (GtkPrinter *printer)
{
  return NULL;
}
