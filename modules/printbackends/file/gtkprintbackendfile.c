/* GTK - The GIMP Toolkit
 * gtkprintbackendfile.c: Default implementation of GtkPrintBackend 
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
#include <cairo-svg.h>

#include <glib/gi18n-lib.h>

#include "gtk/gtk.h"
#include "gtk/gtkprinter-private.h"

#include "gtkprintbackendfile.h"

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

typedef enum
{
  FORMAT_PDF,
  FORMAT_PS,
  FORMAT_SVG,
  N_FORMATS
} OutputFormat;

static const gchar* formats[N_FORMATS] =
{
  "pdf",
  "ps",
  "svg"
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
static void                 gtk_print_backend_file_print_stream    (GtkPrintBackend         *print_backend,
								    GtkPrintJob             *job,
								    GIOChannel              *data_io,
								    GtkPrintJobCompleteFunc  callback,
								    gpointer                 user_data,
								    GDestroyNotify           dnotify);
static cairo_surface_t *    file_printer_create_cairo_surface      (GtkPrinter              *printer,
								    GtkPrintSettings        *settings,
								    gdouble                  width,
								    gdouble                  height,
								    GIOChannel              *cache_io);

static GList *              file_printer_list_papers               (GtkPrinter              *printer);
static GtkPageSetup *       file_printer_get_default_page_size     (GtkPrinter              *printer);

static void
gtk_print_backend_file_register_type (GTypeModule *module)
{
  const GTypeInfo print_backend_file_info =
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
  backend_class->printer_list_papers = file_printer_list_papers;
  backend_class->printer_get_default_page_size = file_printer_get_default_page_size;
}

/* return N_FORMATS if no explicit format in the settings */
static OutputFormat
format_from_settings (GtkPrintSettings *settings)
{
  const gchar *value;
  gint i;

  if (settings == NULL)
    return N_FORMATS;

  value = gtk_print_settings_get (settings,
                                  GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
  if (value == NULL)
    return N_FORMATS;

  for (i = 0; i < N_FORMATS; ++i)
    if (strcmp (value, formats[i]) == 0)
      break;

  g_assert (i < N_FORMATS);

  return (OutputFormat) i;
}

static gchar *
output_file_from_settings (GtkPrintSettings *settings,
			   const gchar      *default_format)
{
  gchar *uri = NULL;

  if (settings)
    uri = g_strdup (gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_URI));

  if (uri == NULL)
    { 
      const gchar *extension, *basename, *output_dir;
      gchar *name, *locale_name, *path;

      if (default_format)
        extension = default_format;
      else
        {
          OutputFormat format;

          format = format_from_settings (settings);
          switch (format)
            {
              default:
              case FORMAT_PDF:
                extension = "pdf";
                break;
              case FORMAT_PS:
                extension = "ps";
                break;
              case FORMAT_SVG:
                extension = "svg";
                break;
            }
        }

      basename = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME);
      if (basename == NULL)
        basename = _("output");

      name = g_strconcat (basename, ".", extension, NULL);

      locale_name = g_filename_from_utf8 (name, -1, NULL, NULL, NULL);
      g_free (name);

      if (locale_name != NULL)
        {      
          output_dir = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_DIR);
          if (output_dir == NULL)
            {
              const gchar *document_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);

              if (document_dir == NULL)
                {
                  gchar *current_dir = g_get_current_dir ();
                  path = g_build_filename (current_dir, locale_name, NULL);
                  g_free (current_dir);
                }
              else
                path = g_build_filename (document_dir, locale_name, NULL);

              uri = g_filename_to_uri (path, NULL, NULL); 
	    }
          else
            {
              path = g_build_filename (output_dir, locale_name, NULL);
              uri = g_filename_to_uri (path, NULL, NULL);
            }

          g_free (path); 
          g_free (locale_name);
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
  gsize written = 0;
  GError *error;

  error = NULL;

  GTK_NOTE (PRINTING,
            g_print ("FILE Backend: Writting %i byte chunk to temp file\n", length));

  while (length > 0) 
    {
      GIOStatus status;

      status = g_io_channel_write_chars (io, (const gchar *) data, length, &written, &error);

      if (status == G_IO_STATUS_ERROR)
        {
          if (error != NULL)
            {
              GTK_NOTE (PRINTING,
                        g_print ("FILE Backend: Error writting to temp file, %s\n", error->message));

              g_error_free (error);
            }

	  return CAIRO_STATUS_WRITE_ERROR;
	}    

      GTK_NOTE (PRINTING,
                g_print ("FILE Backend: Wrote %i bytes to temp file\n", written));
      
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
				   GIOChannel       *cache_io)
{
  cairo_surface_t *surface;
  OutputFormat format;
  const cairo_svg_version_t *versions;
  int num_versions = 0;

  format = format_from_settings (settings);

  switch (format)
    {
      default:
      case FORMAT_PDF:
        surface = cairo_pdf_surface_create_for_stream (_cairo_write, cache_io, width, height);
        break;
      case FORMAT_PS:
        surface = cairo_ps_surface_create_for_stream (_cairo_write, cache_io, width, height);
        break;
      case FORMAT_SVG:
        surface = cairo_svg_surface_create_for_stream (_cairo_write, cache_io, width, height);
        cairo_svg_get_versions (&versions, &num_versions);
        if (num_versions > 0)
          cairo_svg_surface_restrict_to_version (surface, versions[num_versions - 1]);
        break;
    }

  cairo_surface_set_fallback_resolution (surface,
                                         2.0 * gtk_print_settings_get_printer_lpi (settings),
                                         2.0 * gtk_print_settings_get_printer_lpi (settings));

  return surface;
}

typedef struct {
  GtkPrintBackend *backend;
  GtkPrintJobCompleteFunc callback;
  GtkPrintJob *job;
  GFileOutputStream *target_io_stream;
  gpointer user_data;
  GDestroyNotify dnotify;
} _PrintStreamData;

/* expects GDK lock to be held */
static void
file_print_cb_locked (GtkPrintBackendFile *print_backend,
                      GError              *error,
                      gpointer            user_data)
{
  _PrintStreamData *ps = (_PrintStreamData *) user_data;

  if (ps->target_io_stream != NULL)
    g_output_stream_close (G_OUTPUT_STREAM (ps->target_io_stream), NULL, NULL);

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

static void
file_print_cb (GtkPrintBackendFile *print_backend,
               GError              *error,
               gpointer            user_data)
{
  gdk_threads_enter ();

  file_print_cb_locked (print_backend, error, user_data);

  gdk_threads_leave ();
}

static gboolean
file_write (GIOChannel   *source,
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

      g_output_stream_write_all (G_OUTPUT_STREAM (ps->target_io_stream),
                                 buf,
                                 bytes_read,
                                 &bytes_written,
                                 NULL,
                                 &error);
    }

  if (error != NULL || read_status == G_IO_STATUS_EOF)
    {
      file_print_cb (GTK_PRINT_BACKEND_FILE (ps->backend), error, user_data);

      if (error != NULL)
        {
          GTK_NOTE (PRINTING,
                    g_print ("FILE Backend: %s\n", error->message));

          g_error_free (error);
        }

      return FALSE;
    }

  GTK_NOTE (PRINTING,
            g_print ("FILE Backend: Writting %i byte chunk to target file\n", bytes_read));

  return TRUE;
}

static void
gtk_print_backend_file_print_stream (GtkPrintBackend        *print_backend,
				     GtkPrintJob            *job,
				     GIOChannel             *data_io,
				     GtkPrintJobCompleteFunc callback,
				     gpointer                user_data,
				     GDestroyNotify          dnotify)
{
  GError *internal_error = NULL;
  _PrintStreamData *ps;
  GtkPrintSettings *settings;
  gchar *uri;
  GFile *file = NULL;

  settings = gtk_print_job_get_settings (job);

  ps = g_new0 (_PrintStreamData, 1);
  ps->callback = callback;
  ps->user_data = user_data;
  ps->dnotify = dnotify;
  ps->job = g_object_ref (job);
  ps->backend = print_backend;

  internal_error = NULL;
  uri = output_file_from_settings (settings, NULL);

  if (uri == NULL)
    goto error;

  file = g_file_new_for_uri (uri);
  ps->target_io_stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &internal_error);

  g_object_unref (file);
  g_free (uri);

error:
  if (internal_error != NULL)
    {
      file_print_cb_locked (GTK_PRINT_BACKEND_FILE (print_backend),
                            internal_error, ps);

      g_error_free (internal_error);
      return;
    }

  g_io_add_watch (data_io, 
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
			  "accepts-pdf", TRUE,
			  NULL); 

  gtk_printer_set_has_details (printer, TRUE);
  gtk_printer_set_icon_name (printer, "document-save");
  gtk_printer_set_is_active (printer, TRUE);

  gtk_print_backend_add_printer (GTK_PRINT_BACKEND (backend), printer);
  g_object_unref (printer);

  gtk_print_backend_set_list_done (GTK_PRINT_BACKEND (backend));
}

typedef struct {
  GtkPrinter          *printer;
  GtkPrinterOptionSet *set;
} _OutputFormatChangedData;

static void
set_printer_format_from_option_set (GtkPrinter          *printer,
				    GtkPrinterOptionSet *set)
{
  GtkPrinterOption *format_option;
  const gchar *value;
  gint i;

  format_option = gtk_printer_option_set_lookup (set, "output-file-format");
  if (format_option && format_option->value)
    {
      value = format_option->value;
      if (value)
        {
	  for (i = 0; i < N_FORMATS; ++i)
	    if (strcmp (value, formats[i]) == 0)
	      break;

	  g_assert (i < N_FORMATS);

	  switch (i)
	    {
	      case FORMAT_PDF:
		gtk_printer_set_accepts_pdf (printer, TRUE);
		gtk_printer_set_accepts_ps (printer, FALSE);
		break;
	      case FORMAT_PS:
		gtk_printer_set_accepts_pdf (printer, FALSE);
		gtk_printer_set_accepts_ps (printer, TRUE);
		break;
	      case FORMAT_SVG:
	      default:
		gtk_printer_set_accepts_pdf (printer, FALSE);
		gtk_printer_set_accepts_ps (printer, FALSE);
		break;
	    }
	}
    }
}

static void
file_printer_output_file_format_changed (GtkPrinterOption    *format_option,
					 gpointer             user_data)
{
  GtkPrinterOption *uri_option;
  gchar            *base = NULL;
  _OutputFormatChangedData *data = (_OutputFormatChangedData *) user_data;

  if (! format_option->value)
    return;

  uri_option = gtk_printer_option_set_lookup (data->set,
                                              "gtk-main-page-custom-input");

  if (uri_option && uri_option->value)
    {
      const gchar *uri = uri_option->value;
      const gchar *dot = strrchr (uri, '.');

      if (dot)
        {
          gint i;

          /*  check if the file extension matches one of the known ones  */
          for (i = 0; i < N_FORMATS; i++)
            if (strcmp (dot + 1, formats[i]) == 0)
              break;

          if (i < N_FORMATS && strcmp (formats[i], format_option->value))
            {
              /*  the file extension is known but doesn't match the
               *  selected one, strip it away
               */
              base = g_strndup (uri, dot - uri);
            }
        }
      else
        {
          /*  there's no file extension  */
          base = g_strdup (uri);
        }
    }

  if (base)
    {
      gchar *tmp = g_strdup_printf ("%s.%s", base, format_option->value);

      gtk_printer_option_set (uri_option, tmp);
      g_free (tmp);
      g_free (base);
    }

  set_printer_format_from_option_set (data->printer, data->set);
}

static GtkPrinterOptionSet *
file_printer_get_options (GtkPrinter           *printer,
			  GtkPrintSettings     *settings,
			  GtkPageSetup         *page_setup,
			  GtkPrintCapabilities  capabilities)
{
  GtkPrinterOptionSet *set;
  GtkPrinterOption *option;
  const gchar *n_up[] = {"1", "2", "4", "6", "9", "16" };
  const gchar *pages_per_sheet = NULL;
  const gchar *format_names[N_FORMATS] = { N_("PDF"), N_("Postscript"), N_("SVG") };
  const gchar *supported_formats[N_FORMATS];
  gchar *display_format_names[N_FORMATS];
  gint n_formats = 0;
  OutputFormat format;
  gchar *uri;
  gint current_format = 0;
  _OutputFormatChangedData *format_changed_data;

  format = format_from_settings (settings);

  set = gtk_printer_option_set_new ();

  option = gtk_printer_option_new ("gtk-n-up", _("Pages per _sheet:"), GTK_PRINTER_OPTION_TYPE_PICKONE);
  gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (n_up),
					 (char **) n_up, (char **) n_up /* FIXME i18n (localised digits)! */);
  if (settings)
    pages_per_sheet = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_NUMBER_UP);
  if (pages_per_sheet)
    gtk_printer_option_set (option, pages_per_sheet);
  else
    gtk_printer_option_set (option, "1");
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  if (capabilities & (GTK_PRINT_CAPABILITY_GENERATE_PDF | GTK_PRINT_CAPABILITY_GENERATE_PS))
    {
      if (capabilities & GTK_PRINT_CAPABILITY_GENERATE_PDF)
        {
	  if (format == FORMAT_PDF || format == N_FORMATS)
            {
              format = FORMAT_PDF;
	      current_format = n_formats;
            }
          supported_formats[n_formats] = formats[FORMAT_PDF];
	  display_format_names[n_formats] = _(format_names[FORMAT_PDF]);
	  n_formats++;
	}
      if (capabilities & GTK_PRINT_CAPABILITY_GENERATE_PS)
        {
	  if (format == FORMAT_PS || format == N_FORMATS)
	    current_format = n_formats;
          supported_formats[n_formats] = formats[FORMAT_PS];
          display_format_names[n_formats] = _(format_names[FORMAT_PS]);
	  n_formats++;
	}
    }
  else
    {
      switch (format)
        {
          default:
          case FORMAT_PDF:
            current_format = FORMAT_PDF;
            break;
          case FORMAT_PS:
            current_format = FORMAT_PS;
            break;
          case FORMAT_SVG:
            current_format = FORMAT_SVG;            
            break;
        }

      for (n_formats = 0; n_formats < N_FORMATS; ++n_formats)
        {
	  supported_formats[n_formats] = formats[n_formats];
          display_format_names[n_formats] = _(format_names[n_formats]);
	}
    }

  uri = output_file_from_settings (settings, supported_formats[current_format]);

  option = gtk_printer_option_new ("gtk-main-page-custom-input", _("File"), 
				   GTK_PRINTER_OPTION_TYPE_FILESAVE);
  gtk_printer_option_set_activates_default (option, TRUE);
  gtk_printer_option_set (option, uri);
  g_free (uri);
  option->group = g_strdup ("GtkPrintDialogExtension");
  gtk_printer_option_set_add (set, option);

  if (n_formats > 1)
    {
      option = gtk_printer_option_new ("output-file-format", _("_Output format"), 
				       GTK_PRINTER_OPTION_TYPE_ALTERNATIVE);
      option->group = g_strdup ("GtkPrintDialogExtension");

      gtk_printer_option_choices_from_array (option, n_formats,
					     (char **) supported_formats,
					     display_format_names);
      gtk_printer_option_set (option, supported_formats[current_format]);
      gtk_printer_option_set_add (set, option);

      set_printer_format_from_option_set (printer, set);
      format_changed_data = g_new (_OutputFormatChangedData, 1);
      format_changed_data->printer = printer;
      format_changed_data->set = set;
      g_signal_connect_data (option, "changed",
			     G_CALLBACK (file_printer_output_file_format_changed),
			     format_changed_data, (GClosureNotify)g_free, 0);

      g_object_unref (option);
    }

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

  option = gtk_printer_option_set_lookup (options, "output-file-format");
  if (option)
    gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, option->value);

  option = gtk_printer_option_set_lookup (options, "gtk-n-up");
  if (option)
    gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_NUMBER_UP, option->value);

  option = gtk_printer_option_set_lookup (options, "gtk-n-up-layout");
  if (option)
    gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT, option->value);
}

static void
file_printer_prepare_for_print (GtkPrinter       *printer,
				GtkPrintJob      *print_job,
				GtkPrintSettings *settings,
				GtkPageSetup     *page_setup)
{
  gdouble scale;
  GtkPrintPages pages;
  GtkPageRange *ranges;
  gint n_ranges;
  OutputFormat format;

  pages = gtk_print_settings_get_print_pages (settings);
  gtk_print_job_set_pages (print_job, pages);

  if (pages == GTK_PRINT_PAGES_RANGES)
    ranges = gtk_print_settings_get_page_ranges (settings, &n_ranges);
  else
    {
      ranges = NULL;
      n_ranges = 0;
    }

  gtk_print_job_set_page_ranges (print_job, ranges, n_ranges);
  gtk_print_job_set_collate (print_job, gtk_print_settings_get_collate (settings));
  gtk_print_job_set_reverse (print_job, gtk_print_settings_get_reverse (settings));
  gtk_print_job_set_num_copies (print_job, gtk_print_settings_get_n_copies (settings));
  gtk_print_job_set_n_up (print_job, gtk_print_settings_get_number_up (settings));
  gtk_print_job_set_n_up_layout (print_job, gtk_print_settings_get_number_up_layout (settings));

  scale = gtk_print_settings_get_scale (settings);
  if (scale != 100.0)
    gtk_print_job_set_scale (print_job, scale / 100.0);

  gtk_print_job_set_page_set (print_job, gtk_print_settings_get_page_set (settings));

  format = format_from_settings (settings);
  switch (format)
    {
      case FORMAT_PDF:
	gtk_print_job_set_rotate (print_job, FALSE);
        break;
      default:
      case FORMAT_PS:
      case FORMAT_SVG:
	gtk_print_job_set_rotate (print_job, TRUE);
        break;
    }
}

static GList *
file_printer_list_papers (GtkPrinter *printer)
{
  GList *result = NULL;
  GList *papers, *p;
  GtkPageSetup *page_setup;

  papers = gtk_paper_size_get_paper_sizes (FALSE);

  for (p = papers; p; p = p->next)
    {
      GtkPaperSize *paper_size = p->data;

      page_setup = gtk_page_setup_new ();
      gtk_page_setup_set_paper_size (page_setup, paper_size);
      gtk_paper_size_free (paper_size);
      result = g_list_prepend (result, page_setup);
    }

  g_list_free (papers);

  return g_list_reverse (result);
}

static GtkPageSetup *
file_printer_get_default_page_size (GtkPrinter *printer)
{
  GtkPageSetup *result = NULL;

  return result;
}
