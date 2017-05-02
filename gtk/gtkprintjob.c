/* GtkPrintJob
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <glib/gstdio.h>
#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkprintjob.h"
#include "gtkprinter.h"
#include "gtkprinter-private.h"
#include "gtkprintbackend.h"
#include "gtkalias.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct _GtkPrintJobPrivate
{
  gchar *title;

  GIOChannel *spool_io;
  cairo_surface_t *surface;

  GtkPrintStatus status;
  GtkPrintBackend *backend;  
  GtkPrinter *printer;
  GtkPrintSettings *settings;
  GtkPageSetup *page_setup;

  guint printer_set : 1;
  guint page_setup_set : 1;
  guint settings_set  : 1;
  guint track_print_status : 1;
};


#define GTK_PRINT_JOB_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_PRINT_JOB, GtkPrintJobPrivate))

static void     gtk_print_job_finalize     (GObject               *object);
static void     gtk_print_job_set_property (GObject               *object,
					    guint                  prop_id,
					    const GValue          *value,
					    GParamSpec            *pspec);
static void     gtk_print_job_get_property (GObject               *object,
					    guint                  prop_id,
					    GValue                *value,
					    GParamSpec            *pspec);
static GObject* gtk_print_job_constructor  (GType                  type,
					    guint                  n_construct_properties,
					    GObjectConstructParam *construct_params);

enum {
  STATUS_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_PRINTER,
  PROP_PAGE_SETUP,
  PROP_SETTINGS,
  PROP_TRACK_PRINT_STATUS
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkPrintJob, gtk_print_job, G_TYPE_OBJECT)

static void
gtk_print_job_class_init (GtkPrintJobClass *class)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) class;

  object_class->finalize = gtk_print_job_finalize;
  object_class->constructor = gtk_print_job_constructor;
  object_class->set_property = gtk_print_job_set_property;
  object_class->get_property = gtk_print_job_get_property;

  g_type_class_add_private (class, sizeof (GtkPrintJobPrivate));

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
						        P_("Title"),
						        P_("Title of the print job"),
						        NULL,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_PRINTER,
                                   g_param_spec_object ("printer",
						        P_("Printer"),
						        P_("Printer to print the job to"),
						        GTK_TYPE_PRINTER,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_SETTINGS,
                                   g_param_spec_object ("settings",
						        P_("Settings"),
						        P_("Printer settings"),
						        GTK_TYPE_PRINT_SETTINGS,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_PAGE_SETUP,
                                   g_param_spec_object ("page-setup",
						        P_("Page Setup"),
						        P_("Page Setup"),
						        GTK_TYPE_PAGE_SETUP,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
				   PROP_TRACK_PRINT_STATUS,
				   g_param_spec_boolean ("track-print-status",
							 P_("Track Print Status"),
							 P_("TRUE if the print job will continue to emit "
							    "status-changed signals after the print data "
							    "has been sent to the printer or print server."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintJob::status-changed:
   * @job: the #GtkPrintJob object on which the signal was emitted
   *
   * Gets emitted when the status of a job changes. The signal handler
   * can use gtk_print_job_get_status() to obtain the new status.
   *
   * Since: 2.10
   */
  signals[STATUS_CHANGED] =
    g_signal_new (I_("status-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintJobClass, status_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_print_job_init (GtkPrintJob *job)
{
  GtkPrintJobPrivate *priv;

  priv = job->priv = GTK_PRINT_JOB_GET_PRIVATE (job); 

  priv->spool_io = NULL;

  priv->title = g_strdup ("");
  priv->surface = NULL;
  priv->backend = NULL;
  priv->printer = NULL;

  priv->printer_set = FALSE;
  priv->settings_set = FALSE;
  priv->page_setup_set = FALSE;
  priv->status = GTK_PRINT_STATUS_INITIAL;
  priv->track_print_status = FALSE;
  
  job->print_pages = GTK_PRINT_PAGES_ALL;
  job->page_ranges = NULL;
  job->num_page_ranges = 0;
  job->collate = FALSE;
  job->reverse = FALSE;
  job->num_copies = 1;
  job->scale = 1.0;
  job->page_set = GTK_PAGE_SET_ALL;
  job->rotate_to_orientation = FALSE;
  job->number_up = 1;
  job->number_up_layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
}


static GObject*
gtk_print_job_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_params)
{
  GtkPrintJob *job;
  GtkPrintJobPrivate *priv;
  GObject *object;

  object =
    G_OBJECT_CLASS (gtk_print_job_parent_class)->constructor (type,
							      n_construct_properties,
							      construct_params);

  job = GTK_PRINT_JOB (object);

  priv = job->priv;
  g_assert (priv->printer_set &&
	    priv->settings_set &&
	    priv->page_setup_set);
  
  _gtk_printer_prepare_for_print (priv->printer,
				  job,
				  priv->settings,
				  priv->page_setup);

  return object;
}


static void
gtk_print_job_finalize (GObject *object)
{
  GtkPrintJob *job = GTK_PRINT_JOB (object);
  GtkPrintJobPrivate *priv = job->priv;

  if (priv->spool_io != NULL)
    {
      g_io_channel_unref (priv->spool_io);
      priv->spool_io = NULL;
    }
  
  if (priv->backend)
    g_object_unref (priv->backend);

  if (priv->printer)
    g_object_unref (priv->printer);

  if (priv->surface)
    cairo_surface_destroy (priv->surface);

  if (priv->settings)
    g_object_unref (priv->settings);
  
  if (priv->page_setup)
    g_object_unref (priv->page_setup);

  g_free (job->page_ranges);
  job->page_ranges = NULL;
  
  g_free (priv->title);
  priv->title = NULL;

  G_OBJECT_CLASS (gtk_print_job_parent_class)->finalize (object);
}

/**
 * gtk_print_job_new:
 * @title: the job title
 * @printer: a #GtkPrinter
 * @settings: a #GtkPrintSettings
 * @page_setup: a #GtkPageSetup
 *
 * Creates a new #GtkPrintJob.
 *
 * Return value: a new #GtkPrintJob
 *
 * Since: 2.10
 **/
GtkPrintJob *
gtk_print_job_new (const gchar      *title,
		   GtkPrinter       *printer,
		   GtkPrintSettings *settings,
		   GtkPageSetup     *page_setup)
{
  GObject *result;
  result = g_object_new (GTK_TYPE_PRINT_JOB,
                         "title", title,
			 "printer", printer,
			 "settings", settings,
			 "page-setup", page_setup,
			 NULL);
  return (GtkPrintJob *) result;
}

/**
 * gtk_print_job_get_settings:
 * @job: a #GtkPrintJob
 * 
 * Gets the #GtkPrintSettings of the print job.
 * 
 * Return value: (transfer none): the settings of @job
 *
 * Since: 2.10
 */
GtkPrintSettings *
gtk_print_job_get_settings (GtkPrintJob *job)
{
  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), NULL);
  
  return job->priv->settings;
}

/**
 * gtk_print_job_get_printer:
 * @job: a #GtkPrintJob
 * 
 * Gets the #GtkPrinter of the print job.
 * 
 * Return value: (transfer none): the printer of @job
 *
 * Since: 2.10
 */
GtkPrinter *
gtk_print_job_get_printer (GtkPrintJob *job)
{
  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), NULL);
  
  return job->priv->printer;
}

/**
 * gtk_print_job_get_title:
 * @job: a #GtkPrintJob
 * 
 * Gets the job title.
 * 
 * Return value: the title of @job
 *
 * Since: 2.10
 */
const gchar *
gtk_print_job_get_title (GtkPrintJob *job)
{
  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), NULL);
  
  return job->priv->title;
}

/**
 * gtk_print_job_get_status:
 * @job: a #GtkPrintJob
 * 
 * Gets the status of the print job.
 * 
 * Return value: the status of @job
 *
 * Since: 2.10
 */
GtkPrintStatus
gtk_print_job_get_status (GtkPrintJob *job)
{
  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), GTK_PRINT_STATUS_FINISHED);
  
  return job->priv->status;
}

void
gtk_print_job_set_status (GtkPrintJob   *job,
			  GtkPrintStatus status)
{
  GtkPrintJobPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_JOB (job));

  priv = job->priv;

  if (priv->status == status)
    return;

  priv->status = status;
  g_signal_emit (job, signals[STATUS_CHANGED], 0);
}

/**
 * gtk_print_job_set_source_file:
 * @job: a #GtkPrintJob
 * @filename: the file to be printed
 * @error: return location for errors
 * 
 * Make the #GtkPrintJob send an existing document to the 
 * printing system. The file can be in any format understood
 * by the platforms printing system (typically PostScript,
 * but on many platforms PDF may work too). See 
 * gtk_printer_accepts_pdf() and gtk_printer_accepts_ps().
 * 
 * Returns: %FALSE if an error occurred
 *
 * Since: 2.10
 **/
gboolean
gtk_print_job_set_source_file (GtkPrintJob *job,
			       const gchar *filename,
			       GError     **error)
{
  GtkPrintJobPrivate *priv;
  GError *tmp_error;

  tmp_error = NULL;

  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), FALSE);

  priv = job->priv;

  priv->spool_io = g_io_channel_new_file (filename, "r", &tmp_error);

  if (tmp_error == NULL)
    g_io_channel_set_encoding (priv->spool_io, NULL, &tmp_error);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_print_job_get_surface:
 * @job: a #GtkPrintJob
 * @error: (allow-none): return location for errors, or %NULL
 * 
 * Gets a cairo surface onto which the pages of
 * the print job should be rendered.
 * 
 * Return value: (transfer none): the cairo surface of @job
 *
 * Since: 2.10
 **/
cairo_surface_t *
gtk_print_job_get_surface (GtkPrintJob  *job,
			   GError      **error)
{
  GtkPrintJobPrivate *priv;
  gchar *filename = NULL;
  gdouble width, height;
  GtkPaperSize *paper_size;
  int fd;
  GError *tmp_error;

  tmp_error = NULL;

  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), NULL);

  priv = job->priv;

  if (priv->surface)
    return priv->surface;
 
  g_return_val_if_fail (priv->spool_io == NULL, NULL);
 
  fd = g_file_open_tmp ("gtkprint_XXXXXX", 
			 &filename, 
			 &tmp_error);
  if (fd == -1)
    {
      g_free (filename);
      g_propagate_error (error, tmp_error);
      return NULL;
    }

  fchmod (fd, S_IRUSR | S_IWUSR);
  
#ifdef G_ENABLE_DEBUG 
  /* If we are debugging printing don't delete the tmp files */
  if (!(gtk_debug_flags & GTK_DEBUG_PRINTING))
#endif /* G_ENABLE_DEBUG */
  g_unlink (filename);
  g_free (filename);

  paper_size = gtk_page_setup_get_paper_size (priv->page_setup);
  width = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  height = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
 
  priv->spool_io = g_io_channel_unix_new (fd);
  g_io_channel_set_close_on_unref (priv->spool_io, TRUE);
  g_io_channel_set_encoding (priv->spool_io, NULL, &tmp_error);
  
  if (tmp_error != NULL)
    {
      g_io_channel_unref (priv->spool_io);
      priv->spool_io = NULL;
      g_propagate_error (error, tmp_error);
      return NULL;
    }

  priv->surface = _gtk_printer_create_cairo_surface (priv->printer,
						     priv->settings,
						     width, height,
						     priv->spool_io);
  
  return priv->surface;
}

/**
 * gtk_print_job_set_track_print_status:
 * @job: a #GtkPrintJob
 * @track_status: %TRUE to track status after printing
 * 
 * If track_status is %TRUE, the print job will try to continue report
 * on the status of the print job in the printer queues and printer. This
 * can allow your application to show things like "out of paper" issues,
 * and when the print job actually reaches the printer.
 * 
 * This function is often implemented using some form of polling, so it should
 * not be enabled unless needed.
 *
 * Since: 2.10
 */
void
gtk_print_job_set_track_print_status (GtkPrintJob *job,
				      gboolean     track_status)
{
  GtkPrintJobPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_JOB (job));

  priv = job->priv;

  track_status = track_status != FALSE;

  if (priv->track_print_status != track_status)
    {
      priv->track_print_status = track_status;
      
      g_object_notify (G_OBJECT (job), "track-print-status");
    }
}

/**
 * gtk_print_job_get_track_print_status:
 * @job: a #GtkPrintJob
 *
 * Returns wheter jobs will be tracked after printing.
 * For details, see gtk_print_job_set_track_print_status().
 *
 * Return value: %TRUE if print job status will be reported after printing
 *
 * Since: 2.10
 */
gboolean
gtk_print_job_get_track_print_status (GtkPrintJob *job)
{
  GtkPrintJobPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), FALSE);

  priv = job->priv;
  
  return priv->track_print_status;
}

static void
gtk_print_job_set_property (GObject      *object,
	                    guint         prop_id,
	                    const GValue *value,
                            GParamSpec   *pspec)

{
  GtkPrintJob *job = GTK_PRINT_JOB (object);
  GtkPrintJobPrivate *priv = job->priv;
  GtkPrintSettings *settings;

  switch (prop_id)
    {
    case PROP_TITLE:
      g_free (priv->title);
      priv->title = g_value_dup_string (value);
      break;
    
    case PROP_PRINTER:
      priv->printer = GTK_PRINTER (g_value_dup_object (value));
      priv->printer_set = TRUE;
      priv->backend = g_object_ref (gtk_printer_get_backend (priv->printer));
      break;

    case PROP_PAGE_SETUP:
      priv->page_setup = GTK_PAGE_SETUP (g_value_dup_object (value));
      priv->page_setup_set = TRUE;
      break;
      
    case PROP_SETTINGS:
      /* We save a copy of the settings since we modify
       * if when preparing the printer job. */
      settings = GTK_PRINT_SETTINGS (g_value_get_object (value));
      priv->settings = gtk_print_settings_copy (settings);
      priv->settings_set = TRUE;
      break;

    case PROP_TRACK_PRINT_STATUS:
      gtk_print_job_set_track_print_status (job, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_job_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkPrintJob *job = GTK_PRINT_JOB (object);
  GtkPrintJobPrivate *priv = job->priv;

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_PRINTER:
      g_value_set_object (value, priv->printer);
      break;
    case PROP_SETTINGS:
      g_value_set_object (value, priv->settings);
      break;
    case PROP_PAGE_SETUP:
      g_value_set_object (value, priv->page_setup);
      break;
    case PROP_TRACK_PRINT_STATUS:
      g_value_set_boolean (value, priv->track_print_status);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_print_job_send:
 * @job: a GtkPrintJob
 * @callback: function to call when the job completes or an error occurs
 * @user_data: user data that gets passed to @callback
 * @dnotify: destroy notify for @user_data
 * 
 * Sends the print job off to the printer.  
 * 
 * Since: 2.10
 **/
void
gtk_print_job_send (GtkPrintJob             *job,
                    GtkPrintJobCompleteFunc  callback,
                    gpointer                 user_data,
		    GDestroyNotify           dnotify)
{
  GtkPrintJobPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_JOB (job));

  priv = job->priv;
  g_return_if_fail (priv->spool_io != NULL);
  
  gtk_print_job_set_status (job, GTK_PRINT_STATUS_SENDING_DATA);
  
  g_io_channel_seek_position (priv->spool_io, 0, G_SEEK_SET, NULL);
  
  gtk_print_backend_print_stream (priv->backend, job,
				  priv->spool_io,
                                  callback, user_data, dnotify);
}


#define __GTK_PRINT_JOB_C__
#include "gtkaliasdef.c"
