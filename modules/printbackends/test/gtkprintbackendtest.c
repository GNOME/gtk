/* GTK - The GIMP Toolkit
 * gtkprintbackendpdf.c: Test implementation of GtkPrintBackend 
 * for printing to a test
 * Copyright (C) 2007, Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>

#include <glib/gi18n-lib.h>

#include <gtk/gtkprintbackend.h>
#include <gtk/gtkunixprint.h>
#include <gtk/gtkprinter-private.h>

#include "gtkprintbackendtest.h"


typedef struct _GtkPrintBackendTestClass GtkPrintBackendTestClass;

#define GTK_PRINT_BACKEND_TEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_BACKEND_TEST, GtkPrintBackendTestClass))
#define GTK_IS_PRINT_BACKEND_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_BACKEND_TEST))
#define GTK_PRINT_BACKENDTEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_BACKEND_TEST, GtkPrintBackendTestClass))

#define _STREAM_MAX_CHUNK_SIZE 8192

static GType print_backend_test_type = 0;

struct _GtkPrintBackendTestClass
{
  GtkPrintBackendClass parent_class;
};

struct _GtkPrintBackendTest
{
  GtkPrintBackend parent_instance;
};

typedef enum
{
  FORMAT_PDF,
  FORMAT_PS,
  N_FORMATS
} OutputFormat;

static const gchar* formats[N_FORMATS] =
{
  "pdf",
  "ps"
};

static GObjectClass *backend_parent_class;

static void                 gtk_print_backend_test_class_init      (GtkPrintBackendTestClass *class);
static void                 gtk_print_backend_test_init            (GtkPrintBackendTest      *impl);
static void                 test_printer_get_settings_from_options (GtkPrinter              *printer,
								    GtkPrinterOptionSet     *options,
								    GtkPrintSettings        *settings);
static GtkPrinterOptionSet *test_printer_get_options               (GtkPrinter              *printer,
								    GtkPrintSettings        *settings,
								    GtkPageSetup            *page_setup,
								    GtkPrintCapabilities     capabilities);
static void                 test_printer_prepare_for_print         (GtkPrinter              *printer,
								    GtkPrintJob             *print_job,
								    GtkPrintSettings        *settings,
								    GtkPageSetup            *page_setup);
static void                 gtk_print_backend_test_print_stream    (GtkPrintBackend         *print_backend,
								    GtkPrintJob             *job,
								    GIOChannel              *data_io,
								    GtkPrintJobCompleteFunc  callback,
								    gpointer                 user_data,
								    GDestroyNotify           dnotify);
static cairo_surface_t *    test_printer_create_cairo_surface      (GtkPrinter              *printer,
								    GtkPrintSettings        *settings,
								    gdouble                  width,
								    gdouble                  height,
								    GIOChannel              *cache_io);

static void                 test_printer_request_details           (GtkPrinter              *printer);

static void
gtk_print_backend_test_register_type (GTypeModule *module)
{
  const GTypeInfo print_backend_test_info =
  {
    sizeof (GtkPrintBackendTestClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_print_backend_test_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkPrintBackendTest),
    0,		/* n_preallocs */
    (GInstanceInitFunc) gtk_print_backend_test_init,
  };

  print_backend_test_type = g_type_module_register_type (module,
                                                         GTK_TYPE_PRINT_BACKEND,
                                                         "GtkPrintBackendTest",
                                                         &print_backend_test_info, 0);
}

G_MODULE_EXPORT void 
pb_module_init (GTypeModule *module)
{
  gtk_print_backend_test_register_type (module);
}

G_MODULE_EXPORT void 
pb_module_exit (void)
{

}
  
G_MODULE_EXPORT GtkPrintBackend * 
pb_module_create (void)
{
  return gtk_print_backend_test_new ();
}

/*
 * GtkPrintBackendTest
 */
GType
gtk_print_backend_test_get_type (void)
{
  return print_backend_test_type;
}

/**
 * gtk_print_backend_test_new:
 *
 * Creates a new #GtkPrintBackendTest object. #GtkPrintBackendTest
 * implements the #GtkPrintBackend interface with direct access to
 * the testsystem using Unix/Linux API calls
 *
 * Returns: the new #GtkPrintBackendTest object
 **/
GtkPrintBackend *
gtk_print_backend_test_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_BACKEND_TEST, NULL);
}

static void
gtk_print_backend_test_class_init (GtkPrintBackendTestClass *class)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_CLASS (class);

  backend_parent_class = g_type_class_peek_parent (class);

  backend_class->print_stream = gtk_print_backend_test_print_stream;
  backend_class->printer_create_cairo_surface = test_printer_create_cairo_surface;
  backend_class->printer_get_options = test_printer_get_options;
  backend_class->printer_get_settings_from_options = test_printer_get_settings_from_options;
  backend_class->printer_prepare_for_print = test_printer_prepare_for_print;
  backend_class->printer_request_details = test_printer_request_details;
}

/* return N_FORMATS if no explicit format in the settings */
static OutputFormat
format_from_settings (GtkPrintSettings *settings)
{
  const gchar *value;
  gint i;

  if (settings == NULL)
    return N_FORMATS;

  value = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
  if (value == NULL)
    return N_FORMATS;

  for (i = 0; i < N_FORMATS; ++i)
    if (strcmp (value, formats[i]) == 0)
      break;

  g_assert (i < N_FORMATS);

  return (OutputFormat) i;
}

static gchar *
output_test_from_settings (GtkPrintSettings *settings,
			   const gchar      *default_format)
{
  gchar *uri = NULL;
  
  if (settings)
    uri = g_strdup (gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_URI));

  if (uri == NULL)
    { 
      const gchar *extension;
      gchar *name, *locale_name, *path;

      if (default_format)
        extension = default_format;
      else
        {
          OutputFormat format;

          format = format_from_settings (settings);
          extension = format == FORMAT_PS ? "ps" : "pdf";
        }
 
      /* default filename used for print-to-test */ 
      name = g_strdup_printf (_("test-output.%s"), extension);
      locale_name = g_filename_from_utf8 (name, -1, NULL, NULL, NULL);
      g_free (name);

      if (locale_name != NULL)
        {
	  gchar *current_dir = g_get_current_dir ();
          path = g_build_filename (current_dir, locale_name, NULL);
          g_free (locale_name);

          uri = g_filename_to_uri (path, NULL, NULL);
          g_free (path);
	  g_free (current_dir);
	}
    }

  return uri;
}

static cairo_status_t
_cairo_write (void                *closure,
              const unsigned char *data,
              unsigned int         length)
{
  GIOChannel *io = (GIOChannel *)closure;
  gsize written;
  GError *error;

  error = NULL;

  GTK_NOTE (PRINTING,
            g_print ("TEST Backend: Writing %i byte chunk to temp test\n", length));

  while (length > 0) 
    {
      g_io_channel_write_chars (io, (const gchar *) data, length, &written, &error);

      if (error != NULL)
	{
	  GTK_NOTE (PRINTING,
                     g_print ("TEST Backend: Error writing to temp test, %s\n", error->message));

          g_error_free (error);
	  return CAIRO_STATUS_WRITE_ERROR;
	}    

      GTK_NOTE (PRINTING,
                g_print ("TEST Backend: Wrote %i bytes to temp test\n", (int)written));
      
      data += written;
      length -= written;
    }

  return CAIRO_STATUS_SUCCESS;
}


static cairo_surface_t *
test_printer_create_cairo_surface (GtkPrinter       *printer,
				   GtkPrintSettings *settings,
				   gdouble           width, 
				   gdouble           height,
				   GIOChannel       *cache_io)
{
  cairo_surface_t *surface;
  OutputFormat format;

  format = format_from_settings (settings);

  if (format == FORMAT_PS)
    surface = cairo_ps_surface_create_for_stream (_cairo_write, cache_io, width, height);
  else
    surface = cairo_pdf_surface_create_for_stream (_cairo_write, cache_io, width, height);

  cairo_surface_set_fallback_resolution (surface,
                                         2.0 * gtk_print_settings_get_printer_lpi (settings),
                                         2.0 * gtk_print_settings_get_printer_lpi (settings));

  return surface;
}

typedef struct {
  GtkPrintBackend *backend;
  GtkPrintJobCompleteFunc callback;
  GtkPrintJob *job;
  GIOChannel *target_io;
  gpointer user_data;
  GDestroyNotify dnotify;
} _PrintStreamData;

static void
test_print_cb (GtkPrintBackendTest *print_backend,
               GError              *error,
               gpointer            user_data)
{
  _PrintStreamData *ps = (_PrintStreamData *) user_data;

  if (ps->target_io != NULL)
    g_io_channel_unref (ps->target_io);

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
test_write (GIOChannel   *source,
            GIOCondition  con,
            gpointer      user_data)
{
  gchar buf[_STREAM_MAX_CHUNK_SIZE];
  gsize bytes_read;
  GError *error;
  GIOStatus read_status;
  _PrintStreamData *ps = (_PrintStreamData *) user_data;

  error = NULL;

  read_status = 
    g_io_channel_read_chars (source,
                             buf,
                             _STREAM_MAX_CHUNK_SIZE,
                             &bytes_read,
                             &error);

  if (read_status != G_IO_STATUS_ERROR)
    {
      gsize bytes_written;

      g_io_channel_write_chars (ps->target_io, 
                                buf, 
				bytes_read, 
				&bytes_written, 
				&error);
    }

  if (error != NULL || read_status == G_IO_STATUS_EOF)
    {
      test_print_cb (GTK_PRINT_BACKEND_TEST (ps->backend), error, user_data);

      if (error != NULL)
        {
          GTK_NOTE (PRINTING,
                    g_print ("TEST Backend: %s\n", error->message));

          g_error_free (error);
        }

      return FALSE;
    }

  GTK_NOTE (PRINTING,
            g_print ("TEST Backend: Writing %i byte chunk to target test\n", (int)bytes_read));

  return TRUE;
}

static void
gtk_print_backend_test_print_stream (GtkPrintBackend        *print_backend,
				     GtkPrintJob            *job,
				     GIOChannel             *data_io,
				     GtkPrintJobCompleteFunc callback,
				     gpointer                user_data,
				     GDestroyNotify          dnotify)
{
  GError *internal_error = NULL;
  GtkPrinter *printer;
  _PrintStreamData *ps;
  GtkPrintSettings *settings;
  gchar *uri, *testname;

  printer = gtk_print_job_get_printer (job);
  settings = gtk_print_job_get_settings (job);

  ps = g_new0 (_PrintStreamData, 1);
  ps->callback = callback;
  ps->user_data = user_data;
  ps->dnotify = dnotify;
  ps->job = g_object_ref (job);
  ps->backend = print_backend;

  internal_error = NULL;
  uri = output_test_from_settings (settings, NULL);
  testname = g_filename_from_uri (uri, NULL, &internal_error);
  g_free (uri);

  if (testname == NULL)
    goto error;

  ps->target_io = g_io_channel_new_file (testname, "w", &internal_error);

  g_free (testname);

  if (internal_error == NULL)
    g_io_channel_set_encoding (ps->target_io, NULL, &internal_error);

error:
  if (internal_error != NULL)
    {
      test_print_cb (GTK_PRINT_BACKEND_TEST (print_backend),
                    internal_error, ps);

      g_error_free (internal_error);
      return;
    }

  g_io_add_watch (data_io, 
                  G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
                  (GIOFunc) test_write,
                  ps);
}

static void
gtk_print_backend_test_init (GtkPrintBackendTest *backend)
{
  GtkPrinter *printer;
  int i;

  /* make 100 of these printers */
  for (i = 0; i < 100; i++)
    {
      char *name;
 
      name = g_strdup_printf ("%s %i", _("Print to Test Printer"), i);
      printer = g_object_new (GTK_TYPE_PRINTER,
			      "name", name,
			      "backend", backend,
			      "is-virtual", FALSE, /* treat printer like a real one*/
			      NULL); 
      g_free (name);

      g_message ("TEST Backend: Adding printer %d\n", i);

      gtk_printer_set_has_details (printer, FALSE);
      gtk_printer_set_icon_name (printer, "edit-delete"); /* use a delete icon just for fun */
      gtk_printer_set_is_active (printer, TRUE);

      gtk_print_backend_add_printer (GTK_PRINT_BACKEND (backend), printer);
      g_object_unref (printer);
    }

  gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (backend));
}

static GtkPrinterOptionSet *
test_printer_get_options (GtkPrinter           *printer,
			  GtkPrintSettings     *settings,
			  GtkPageSetup         *page_setup,
			  GtkPrintCapabilities  capabilities)
{
  GtkPrinterOptionSet *set;
  GtkPrinterOption *option;
  const gchar *n_up[] = { "1" };
  OutputFormat format;

  format = format_from_settings (settings);

  set = gtk_printer_option_set_new ();

  option = gtk_printer_option_new ("gtk-n-up", _("Pages per _sheet:"), GTK_PRINTER_OPTION_TYPE_PICKONE);
  gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (n_up),
					 (char **) n_up, (char **) n_up /* FIXME i18n (localised digits)! */);
  gtk_printer_option_set (option, "1");
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  return set;
}

static void
test_printer_get_settings_from_options (GtkPrinter          *printer,
					GtkPrinterOptionSet *options,
					GtkPrintSettings    *settings)
{
}

static void
test_printer_prepare_for_print (GtkPrinter       *printer,
				GtkPrintJob      *print_job,
				GtkPrintSettings *settings,
				GtkPageSetup     *page_setup)
{
  gdouble scale;

  gtk_print_job_set_pages (print_job, gtk_print_settings_get_print_pages (settings));
  gtk_print_job_set_page_ranges (print_job, NULL, 0);
  
  if (gtk_print_job_get_pages (print_job) == GTK_PRINT_PAGES_RANGES)
    {
      GtkPageRange *page_ranges;
      gint num_page_ranges;
      page_ranges = gtk_print_settings_get_page_ranges (settings, &num_page_ranges);
      gtk_print_job_set_page_ranges (print_job, page_ranges, num_page_ranges);
    }

  gtk_print_job_set_collate (print_job, gtk_print_settings_get_collate (settings));
  gtk_print_job_set_reverse (print_job, gtk_print_settings_get_reverse (settings));
  gtk_print_job_set_num_copies (print_job, gtk_print_settings_get_n_copies (settings));

  scale = gtk_print_settings_get_scale (settings);
  if (scale != 100.0)
    gtk_print_job_set_scale (print_job, scale/100.0);

  gtk_print_job_set_page_set (print_job, gtk_print_settings_get_page_set (settings));
  gtk_print_job_set_rotate (print_job, TRUE);
}

static gboolean
test_printer_details_acquired_cb (GtkPrinter *printer)
{
  gboolean success;
  gint weight;

  /* weight towards success */
  weight = g_random_int_range (0, 100);

  success = FALSE;
  if (weight < 75)
    success = TRUE;

  g_message ("success %i", success);
  gtk_printer_set_has_details (printer, success);
  g_signal_emit_by_name (printer, "details-acquired", success);

  return G_SOURCE_REMOVE;
}

static void
test_printer_request_details (GtkPrinter *printer)
{
  gint weight;
  gint time;
  /* set the timer to succeed or fail at a random time interval */
  /* weight towards the shorter end */
  weight = g_random_int_range (0, 100);
  if (weight < 50)
    time = g_random_int_range (0, 2);
  else if (weight < 75)
    time = g_random_int_range (1, 5);
  else
    time = g_random_int_range (1, 10);

  g_message ("Gathering details in %i seconds", time);

  if (time == 0)
    time = 10;
  else
    time *= 1000;

  g_timeout_add (time, (GSourceFunc) test_printer_details_acquired_cb, printer);
}


