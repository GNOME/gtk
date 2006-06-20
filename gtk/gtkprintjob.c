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

  gint spool_file_fd;
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
  GTK_PRINT_JOB_PROP_TITLE,
  GTK_PRINT_JOB_PROP_PRINTER,
  GTK_PRINT_JOB_PROP_PAGE_SETUP,
  GTK_PRINT_JOB_PROP_SETTINGS
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

  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   GTK_PRINT_JOB_PROP_TITLE,
                                   g_param_spec_string ("title",
						        P_("Title"),
						        P_("Title of the print job"),
						        NULL,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   GTK_PRINT_JOB_PROP_PRINTER,
                                   g_param_spec_object ("printer",
						        P_("Printer"),
						        P_("Printer to print the job to"),
						        GTK_TYPE_PRINTER,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   GTK_PRINT_JOB_PROP_SETTINGS,
                                   g_param_spec_object ("settings",
						        P_("Settings"),
						        P_("Printer settings"),
						        GTK_TYPE_PRINT_SETTINGS,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   GTK_PRINT_JOB_PROP_PAGE_SETUP,
                                   g_param_spec_object ("page-setup",
						        P_("Page Setup"),
						        P_("Page Setup"),
						        GTK_TYPE_PAGE_SETUP,
							GTK_PARAM_READWRITE |
						        G_PARAM_CONSTRUCT_ONLY));

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

  priv->spool_file_fd = -1;

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

  if (priv->spool_file_fd >= 0)
    {
      close (priv->spool_file_fd);
      priv->spool_file_fd = -1;
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
 * Return value: the settings of @job
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
 * Return value: the printer of @job
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
G_CONST_RETURN gchar *
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

  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), FALSE);

  priv = job->priv;

  priv->spool_file_fd = g_open (filename, O_RDONLY|O_BINARY);
  if (priv->spool_file_fd < 0)
    {
      gchar *display_filename = g_filename_display_name (filename);
      int save_errno = errno;
      
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (save_errno),
                   _("Failed to open file '%s': %s"),
                   display_filename, 
		   g_strerror (save_errno));
      
      g_free (display_filename);

      return FALSE;
    }
    return TRUE;
}

/**
 * gtk_print_job_get_surface:
 * @job: a #GtkPrintJob
 * @error: return location for errors, or %NULL
 * 
 * Gets a cairo surface onto which the pages of
 * the print job should be rendered.
 * 
 * Return value: the cairo surface of @job
 *
 * Since: 2.10
 **/
cairo_surface_t *
gtk_print_job_get_surface (GtkPrintJob  *job,
			   GError      **error)
{
  GtkPrintJobPrivate *priv;
  gchar *filename;
  gdouble width, height;
  GtkPaperSize *paper_size;
  
  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), NULL);

  priv = job->priv;

  if (priv->surface)
    return priv->surface;
 
  g_return_val_if_fail (priv->spool_file_fd == -1, NULL);
 
  priv->spool_file_fd = g_file_open_tmp ("gtkprint_XXXXXX", 
					 &filename, 
					 error);
  if (priv->spool_file_fd == -1)
    return NULL;

  fchmod (priv->spool_file_fd, S_IRUSR | S_IWUSR);
  unlink (filename);

  paper_size = gtk_page_setup_get_paper_size (priv->page_setup);
  width = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  height = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
  
  priv->surface = _gtk_printer_create_cairo_surface (priv->printer,
						     priv->settings,
						     width, height,
						     priv->spool_file_fd);
 
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
      
      g_object_notify (G_OBJECT (job), "track-status");
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
    case GTK_PRINT_JOB_PROP_TITLE:
      priv->title = g_value_dup_string (value);
      break;
    
    case GTK_PRINT_JOB_PROP_PRINTER:
      priv->printer = GTK_PRINTER (g_value_dup_object (value));
      priv->printer_set = TRUE;
      priv->backend = g_object_ref (gtk_printer_get_backend (priv->printer));
      break;

    case GTK_PRINT_JOB_PROP_PAGE_SETUP:
      priv->page_setup = GTK_PAGE_SETUP (g_value_dup_object (value));
      priv->page_setup_set = TRUE;
      break;
      
    case GTK_PRINT_JOB_PROP_SETTINGS:
      /* We save a copy of the settings since we modify
       * if when preparing the printer job. */
      settings = GTK_PRINT_SETTINGS (g_value_get_object (value));
      priv->settings = gtk_print_settings_copy (settings);
      priv->settings_set = TRUE;
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
    case GTK_PRINT_JOB_PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case GTK_PRINT_JOB_PROP_PRINTER:
      g_value_set_object (value, priv->printer);
      break;
    case GTK_PRINT_JOB_PROP_SETTINGS:
      g_value_set_object (value, priv->settings);
      break;
    case GTK_PRINT_JOB_PROP_PAGE_SETUP:
      g_value_set_object (value, priv->page_setup);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_print_job_send:
 * @job: a GtkPrintJob
 * @callback: function to call when the job completes
 * @user_data: user data that gets passed to @callback
 * @dnotify: destroy notify for @user_data
 * @error: return location for errors, or %NULL
 * 
 * Sends the print job off to the printer.  
 * 
 * Return value: %FALSE if an error occurred
 *
 * Since: 2.10
 **/
gboolean
gtk_print_job_send (GtkPrintJob             *job,
                    GtkPrintJobCompleteFunc  callback,
                    gpointer                 user_data,
		    GDestroyNotify           dnotify,
		    GError                 **error)
{
  GtkPrintJobPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINT_JOB (job), FALSE);

  priv = job->priv;
  g_return_val_if_fail (priv->spool_file_fd > 0, FALSE);
  
  gtk_print_job_set_status (job, GTK_PRINT_STATUS_SENDING_DATA);
  lseek (priv->spool_file_fd, 0, SEEK_SET);
  gtk_print_backend_print_stream (priv->backend,
                                  job,
				  priv->spool_file_fd,
                                  callback,
                                  user_data,
				  dnotify);

  return TRUE;
}

GType
gtk_print_capabilities_get_type (void)
{
  static GType etype = 0;

  if (etype == 0)
    {
      static const GFlagsValue values[] = {
        { GTK_PRINT_CAPABILITY_PAGE_SET, "GTK_PRINT_CAPABILITY_PAGE_SET", "page-set" },
        { GTK_PRINT_CAPABILITY_COPIES, "GTK_PRINT_CAPABILITY_COPIES", "copies" },
        { GTK_PRINT_CAPABILITY_COLLATE, "GTK_PRINT_CAPABILITY_COLLATE", "collate" },
        { GTK_PRINT_CAPABILITY_REVERSE, "GTK_PRINT_CAPABILITY_REVERSE", "reverse" },
        { GTK_PRINT_CAPABILITY_SCALE, "GTK_PRINT_CAPABILITY_SCALE", "scale" },
        { 0, NULL, NULL }
      };

      etype = g_flags_register_static (I_("GtkPrintCapabilities"), values);
    }

  return etype;
}


#define __GTK_PRINT_JOB_C__
#include "gtkaliasdef.c"
