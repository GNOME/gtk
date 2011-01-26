/* GTK - The GIMP Toolkit
 * gtkprintoperation-unix.c: Print Operation Details for Unix 
 *                           and Unix-like platforms
 * Copyright (C) 2006, Red Hat, Inc.
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>       
#include <fcntl.h>

#include <glib/gstdio.h>
#include "gtkprintoperation-private.h"
#include "gtkmessagedialog.h"

#include <cairo-pdf.h>
#include <cairo-ps.h>
#include "gtkprivate.h"
#include "gtkprintunixdialog.h"
#include "gtkpagesetupunixdialog.h"
#include "gtkprintbackend.h"
#include "gtkprinter.h"
#include "gtkprintjob.h"
#include "gtklabel.h"
#include "gtkintl.h"
#include "gtkalias.h"

typedef struct 
{
  GtkWindow *parent;        /* just in case we need to throw error dialogs */
  GMainLoop *loop;
  gboolean data_sent;

  /* Real printing (not preview) */
  GtkPrintJob *job;         /* the job we are sending to the printer */
  cairo_surface_t *surface;
  gulong job_status_changed_tag;

  
} GtkPrintOperationUnix;

typedef struct _PrinterFinder PrinterFinder;

static void printer_finder_free (PrinterFinder *finder);
static void find_printer        (const gchar   *printer,
				 GFunc          func,
				 gpointer       data);

static void
unix_start_page (GtkPrintOperation *op,
		 GtkPrintContext   *print_context,
		 GtkPageSetup      *page_setup)
{
  GtkPrintOperationUnix *op_unix;  
  GtkPaperSize *paper_size;
  cairo_surface_type_t type;
  gdouble w, h;

  op_unix = op->priv->platform_data;
  
  paper_size = gtk_page_setup_get_paper_size (page_setup);

  w = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  h = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
  
  type = cairo_surface_get_type (op_unix->surface);

  if ((op->priv->manual_number_up < 2) ||
      (op->priv->page_position % op->priv->manual_number_up == 0))
    {
      if (type == CAIRO_SURFACE_TYPE_PS)
        {
          cairo_ps_surface_set_size (op_unix->surface, w, h);
          cairo_ps_surface_dsc_begin_page_setup (op_unix->surface);
          switch (gtk_page_setup_get_orientation (page_setup))
            {
              case GTK_PAGE_ORIENTATION_PORTRAIT:
              case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
                cairo_ps_surface_dsc_comment (op_unix->surface, "%%PageOrientation: Portrait");
                break;

              case GTK_PAGE_ORIENTATION_LANDSCAPE:
              case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
                cairo_ps_surface_dsc_comment (op_unix->surface, "%%PageOrientation: Landscape");
                break;
            }
         }
      else if (type == CAIRO_SURFACE_TYPE_PDF)
        {
          cairo_pdf_surface_set_size (op_unix->surface, w, h);
        }
    }
}

static void
unix_end_page (GtkPrintOperation *op,
	       GtkPrintContext   *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo_context (print_context);

  if ((op->priv->manual_number_up < 2) ||
      ((op->priv->page_position + 1) % op->priv->manual_number_up == 0) ||
      (op->priv->page_position == op->priv->nr_of_pages_to_print - 1))
    cairo_show_page (cr);
}

static void
op_unix_free (GtkPrintOperationUnix *op_unix)
{
  if (op_unix->job)
    {
      g_signal_handler_disconnect (op_unix->job,
				   op_unix->job_status_changed_tag);
      g_object_unref (op_unix->job);
    }

  g_free (op_unix);
}

static gchar *
shell_command_substitute_file (const gchar *cmd,
			       const gchar *pdf_filename,
			       const gchar *settings_filename,
                               gboolean    *pdf_filename_replaced,
                               gboolean    *settings_filename_replaced)
{
  const gchar *inptr, *start;
  GString *final;

  g_return_val_if_fail (cmd != NULL, NULL);
  g_return_val_if_fail (pdf_filename != NULL, NULL);
  g_return_val_if_fail (settings_filename != NULL, NULL);

  final = g_string_new (NULL);

  *pdf_filename_replaced = FALSE;
  *settings_filename_replaced = FALSE;

  start = inptr = cmd;
  while ((inptr = strchr (inptr, '%')) != NULL) 
    {
      g_string_append_len (final, start, inptr - start);
      inptr++;
      switch (*inptr) 
        {
          case 'f':
            g_string_append (final, pdf_filename);
            *pdf_filename_replaced = TRUE;
            break;

          case 's':
            g_string_append (final, settings_filename);
            *settings_filename_replaced = TRUE;
            break;

          case '%':
            g_string_append_c (final, '%');
            break;

          default:
            g_string_append_c (final, '%');
            if (*inptr)
              g_string_append_c (final, *inptr);
            break;
        }
      if (*inptr)
        inptr++;
      start = inptr;
    }
  g_string_append (final, start);

  return g_string_free (final, FALSE);
}

void
_gtk_print_operation_platform_backend_launch_preview (GtkPrintOperation *op,
						      cairo_surface_t   *surface,
						      GtkWindow         *parent,
						      const gchar       *filename)
{
  gint argc;
  gchar **argv;
  gchar *cmd;
  gchar *preview_cmd;
  GtkSettings *settings;
  GtkPrintSettings *print_settings = NULL;
  GtkPageSetup *page_setup;
  GKeyFile *key_file = NULL;
  gchar *data = NULL;
  gsize data_len;
  gchar *settings_filename = NULL;
  gchar *quoted_filename;
  gchar *quoted_settings_filename;
  gboolean filename_used = FALSE;
  gboolean settings_used = FALSE;
  GdkScreen *screen;
  GError *error = NULL;
  gint fd;
  gboolean retval;

  cairo_surface_destroy (surface);
 
  if (parent)
    screen = gtk_window_get_screen (parent);
  else
    screen = gdk_screen_get_default ();

  fd = g_file_open_tmp ("settingsXXXXXX.ini", &settings_filename, &error);
  if (fd < 0) 
    goto out;

  key_file = g_key_file_new ();
  
  print_settings = gtk_print_settings_copy (gtk_print_operation_get_print_settings (op));

  if (print_settings != NULL)
    {
      gtk_print_settings_set_reverse (print_settings, FALSE);
      gtk_print_settings_set_page_set (print_settings, GTK_PAGE_SET_ALL);
      gtk_print_settings_set_scale (print_settings, 1.0);
      gtk_print_settings_set_number_up (print_settings, 1);
      gtk_print_settings_set_number_up_layout (print_settings, GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM);

      /*  These removals are neccessary because cups-* settings have higher priority
       *  than normal settings.
       */
      gtk_print_settings_unset (print_settings, "cups-reverse");
      gtk_print_settings_unset (print_settings, "cups-page-set");
      gtk_print_settings_unset (print_settings, "cups-scale");
      gtk_print_settings_unset (print_settings, "cups-number-up");
      gtk_print_settings_unset (print_settings, "cups-number-up-layout");

      gtk_print_settings_to_key_file (print_settings, key_file, NULL);
      g_object_unref (print_settings);
    }

  page_setup = gtk_print_context_get_page_setup (op->priv->print_context);
  gtk_page_setup_to_key_file (page_setup, key_file, NULL);

  g_key_file_set_string (key_file, "Print Job", "title", op->priv->job_name);

  data = g_key_file_to_data (key_file, &data_len, &error);
  if (!data)
    goto out;

  retval = g_file_set_contents (settings_filename, data, data_len, &error);
  if (!retval)
    goto out;

  settings = gtk_settings_get_for_screen (screen);
  g_object_get (settings, "gtk-print-preview-command", &preview_cmd, NULL);

  quoted_filename = g_shell_quote (filename);
  quoted_settings_filename = g_shell_quote (settings_filename);
  cmd = shell_command_substitute_file (preview_cmd, quoted_filename, quoted_settings_filename, &filename_used, &settings_used);
  g_shell_parse_argv (cmd, &argc, &argv, &error);

  g_free (preview_cmd);
  g_free (quoted_filename);
  g_free (quoted_settings_filename);
  g_free (cmd);

  if (error != NULL)
    goto out;

  gdk_spawn_on_screen (screen, NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

  g_strfreev (argv);

  if (error != NULL)
    {
      gchar* uri;

      g_warning ("%s %s", _("Error launching preview"), error->message);

      g_error_free (error);
      error = NULL;
      uri = g_filename_to_uri (filename, NULL, NULL);
      gtk_show_uri (screen, uri, GDK_CURRENT_TIME, &error);
      g_free (uri);
    }

 out:
  if (error != NULL)
    {
      GtkWidget *edialog;
      edialog = gtk_message_dialog_new (parent, 
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Error launching preview") /* FIXME better text */);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (edialog),
                                                "%s", error->message);
      g_signal_connect (edialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_window_present (GTK_WINDOW (edialog));

      g_error_free (error);

      filename_used = FALSE; 
      settings_used = FALSE;
   } 

  if (!filename_used)
    g_unlink (filename);

  if (!settings_used)
    g_unlink (settings_filename);

  if (fd > 0)
    close (fd);
  
  if (key_file)
    g_key_file_free (key_file);
  g_free (data);
  g_free (settings_filename);
}

static void
unix_finish_send  (GtkPrintJob *job,
                   gpointer     user_data, 
                   GError      *error)
{
  GtkPrintOperation *op = (GtkPrintOperation *) user_data;
  GtkPrintOperationUnix *op_unix = op->priv->platform_data;

  if (error != NULL)
    {
      GtkWidget *edialog;
      edialog = gtk_message_dialog_new (op_unix->parent, 
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Error printing") /* FIXME better text */);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (edialog),
                                                "%s", error->message);
      gtk_window_set_modal (GTK_WINDOW (edialog), TRUE);
      g_signal_connect (edialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_window_present (GTK_WINDOW (edialog));
    }

  op_unix->data_sent = TRUE;

  if (op_unix->loop)
    g_main_loop_quit (op_unix->loop);

  g_object_unref (op);
}

static void
unix_end_run (GtkPrintOperation *op,
	      gboolean           wait,
	      gboolean           cancelled)
{
  GtkPrintOperationUnix *op_unix = op->priv->platform_data;

  cairo_surface_finish (op_unix->surface);
  
  if (cancelled)
    return;

  if (wait)
    op_unix->loop = g_main_loop_new (NULL, FALSE);
  
  /* TODO: Check for error */
  if (op_unix->job != NULL)
    {
      g_object_ref (op);
      gtk_print_job_send (op_unix->job,
                          unix_finish_send, 
                          op, NULL);
    }

  if (wait)
    {
      g_object_ref (op);
      if (!op_unix->data_sent)
	{
	  GDK_THREADS_LEAVE ();  
	  g_main_loop_run (op_unix->loop);
	  GDK_THREADS_ENTER ();  
	}
      g_main_loop_unref (op_unix->loop);
      op_unix->loop = NULL;
      g_object_unref (op);
    }
}

static void
job_status_changed_cb (GtkPrintJob       *job, 
		       GtkPrintOperation *op)
{
  _gtk_print_operation_set_status (op, gtk_print_job_get_status (job), NULL);
}


static void
print_setup_changed_cb (GtkPrintUnixDialog *print_dialog, 
                        GParamSpec         *pspec,
                        gpointer            user_data)
{
  GtkPageSetup             *page_setup;
  GtkPrintSettings         *print_settings;
  GtkPrintOperation        *op = user_data;
  GtkPrintOperationPrivate *priv = op->priv;

  page_setup = gtk_print_unix_dialog_get_page_setup (print_dialog);
  print_settings = gtk_print_unix_dialog_get_settings (print_dialog);

  g_signal_emit_by_name (op,
                         "update-custom-widget",
                         priv->custom_widget,
                         page_setup,
                         print_settings);
}

static GtkWidget *
get_print_dialog (GtkPrintOperation *op,
                  GtkWindow         *parent)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkWidget *pd, *label;
  const gchar *custom_tab_label;

  pd = gtk_print_unix_dialog_new (NULL, parent);

  gtk_print_unix_dialog_set_manual_capabilities (GTK_PRINT_UNIX_DIALOG (pd),
						 GTK_PRINT_CAPABILITY_PAGE_SET |
						 GTK_PRINT_CAPABILITY_COPIES |
						 GTK_PRINT_CAPABILITY_COLLATE |
						 GTK_PRINT_CAPABILITY_REVERSE |
						 GTK_PRINT_CAPABILITY_SCALE |
						 GTK_PRINT_CAPABILITY_PREVIEW |
						 GTK_PRINT_CAPABILITY_NUMBER_UP |
						 GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT);

  if (priv->print_settings)
    gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG (pd),
					priv->print_settings);

  if (priv->default_page_setup)
    gtk_print_unix_dialog_set_page_setup (GTK_PRINT_UNIX_DIALOG (pd), 
                                          priv->default_page_setup);

  gtk_print_unix_dialog_set_embed_page_setup (GTK_PRINT_UNIX_DIALOG (pd),
                                              priv->embed_page_setup);

  gtk_print_unix_dialog_set_current_page (GTK_PRINT_UNIX_DIALOG (pd), 
                                          priv->current_page);

  gtk_print_unix_dialog_set_support_selection (GTK_PRINT_UNIX_DIALOG (pd),
                                               priv->support_selection);

  gtk_print_unix_dialog_set_has_selection (GTK_PRINT_UNIX_DIALOG (pd),
                                           priv->has_selection);

  g_signal_emit_by_name (op, "create-custom-widget",
			 &priv->custom_widget);

  if (priv->custom_widget) 
    {
      custom_tab_label = priv->custom_tab_label;
      
      if (custom_tab_label == NULL)
	{
	  custom_tab_label = g_get_application_name ();
	  if (custom_tab_label == NULL)
	    custom_tab_label = _("Application");
	}

      label = gtk_label_new (custom_tab_label);
      
      gtk_print_unix_dialog_add_custom_tab (GTK_PRINT_UNIX_DIALOG (pd),
					    priv->custom_widget, label);

      g_signal_connect (pd, "notify::selected-printer", (GCallback) print_setup_changed_cb, op);
      g_signal_connect (pd, "notify::page-setup", (GCallback) print_setup_changed_cb, op);
    }
  
  return pd;
}
  
typedef struct 
{
  GtkPrintOperation           *op;
  gboolean                     do_print;
  gboolean                     do_preview;
  GtkPrintOperationResult      result;
  GtkPrintOperationPrintFunc   print_cb;
  GDestroyNotify               destroy;
  GtkWindow                   *parent;
  GMainLoop                   *loop;
} PrintResponseData;

static void
print_response_data_free (gpointer data)
{
  PrintResponseData *rdata = data;

  g_object_unref (rdata->op);
  g_free (rdata);
}

static void
finish_print (PrintResponseData *rdata,
	      GtkPrinter        *printer,
	      GtkPageSetup      *page_setup,
	      GtkPrintSettings  *settings,
	      gboolean           page_setup_set)
{
  GtkPrintOperation *op = rdata->op;
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPrintJob *job;
  gdouble top, bottom, left, right;
  
  if (rdata->do_print)
    {
      gtk_print_operation_set_print_settings (op, settings);
      priv->print_context = _gtk_print_context_new (op);

      if (gtk_print_settings_get_number_up (settings) < 2)
        {
	  if (printer && gtk_printer_get_hard_margins (printer, &top, &bottom, &left, &right))
	    _gtk_print_context_set_hard_margins (priv->print_context, top, bottom, left, right);
	}
      else
        {
	  /* Pages do not have any unprintable area when printing n-up as each page on the
	   * sheet has been scaled down and translated to a position within the printable
	   * area of the sheet.
	   */
	  _gtk_print_context_set_hard_margins (priv->print_context, 0, 0, 0, 0);
	}

      if (page_setup != NULL &&
          (gtk_print_operation_get_default_page_setup (op) == NULL ||
           page_setup_set))
        gtk_print_operation_set_default_page_setup (op, page_setup);

      _gtk_print_context_set_page_setup (priv->print_context, page_setup);

      if (!rdata->do_preview)
        {
	  GtkPrintOperationUnix *op_unix;
	  cairo_t *cr;
	  
	  op_unix = g_new0 (GtkPrintOperationUnix, 1);
	  priv->platform_data = op_unix;
	  priv->free_platform_data = (GDestroyNotify) op_unix_free;
	  op_unix->parent = rdata->parent;
	  
	  priv->start_page = unix_start_page;
	  priv->end_page = unix_end_page;
	  priv->end_run = unix_end_run;
	  
	  job = gtk_print_job_new (priv->job_name, printer, settings, page_setup);
          op_unix->job = job;
          gtk_print_job_set_track_print_status (job, priv->track_print_status);
	  
	  op_unix->surface = gtk_print_job_get_surface (job, &priv->error);
	  if (op_unix->surface == NULL) 
            {
	      rdata->result = GTK_PRINT_OPERATION_RESULT_ERROR;
	      rdata->do_print = FALSE;
	      goto out;
            }
	  
	  cr = cairo_create (op_unix->surface);
	  gtk_print_context_set_cairo_context (priv->print_context, cr, 72, 72);
	  cairo_destroy (cr);

          _gtk_print_operation_set_status (op, gtk_print_job_get_status (job), NULL);
	  
          op_unix->job_status_changed_tag =
	    g_signal_connect (job, "status-changed",
			      G_CALLBACK (job_status_changed_cb), op);
	  
          priv->print_pages = job->print_pages;
          priv->page_ranges = job->page_ranges;
          priv->num_page_ranges = job->num_page_ranges;
	  
          priv->manual_num_copies = job->num_copies;
          priv->manual_collation = job->collate;
          priv->manual_reverse = job->reverse;
          priv->manual_page_set = job->page_set;
          priv->manual_scale = job->scale;
          priv->manual_orientation = job->rotate_to_orientation;
          priv->manual_number_up = job->number_up;
          priv->manual_number_up_layout = job->number_up_layout;
        }
    } 
 out:
  if (rdata->print_cb)
    rdata->print_cb (op, rdata->parent, rdata->do_print, rdata->result); 

  if (rdata->destroy)
    rdata->destroy (rdata);
}

static void 
handle_print_response (GtkWidget *dialog,
		       gint       response,
		       gpointer   data)
{
  GtkPrintUnixDialog *pd = GTK_PRINT_UNIX_DIALOG (dialog);
  PrintResponseData *rdata = data;
  GtkPrintSettings *settings = NULL;
  GtkPageSetup *page_setup = NULL;
  GtkPrinter *printer = NULL;
  gboolean page_setup_set = FALSE;

  if (response == GTK_RESPONSE_OK)
    {
      printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (pd));

      rdata->result = GTK_PRINT_OPERATION_RESULT_APPLY;
      rdata->do_preview = FALSE;
      if (printer != NULL)
	rdata->do_print = TRUE;
    } 
  else if (response == GTK_RESPONSE_APPLY)
    {
      /* print preview */
      rdata->result = GTK_PRINT_OPERATION_RESULT_APPLY;
      rdata->do_preview = TRUE;
      rdata->do_print = TRUE;

      rdata->op->priv->action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
    }

  if (rdata->do_print)
    {
      settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (pd));
      page_setup = gtk_print_unix_dialog_get_page_setup (GTK_PRINT_UNIX_DIALOG (pd));
      page_setup_set = gtk_print_unix_dialog_get_page_setup_set (GTK_PRINT_UNIX_DIALOG (pd));

      /* Set new print settings now so that custom-widget options
       * can be added to the settings in the callback
       */
      gtk_print_operation_set_print_settings (rdata->op, settings);
      g_signal_emit_by_name (rdata->op, "custom-widget-apply", rdata->op->priv->custom_widget);
    }
  
  finish_print (rdata, printer, page_setup, settings, page_setup_set);

  if (settings)
    g_object_unref (settings);
    
  gtk_widget_destroy (GTK_WIDGET (pd));
 
}


static void
found_printer (GtkPrinter        *printer,
	       PrintResponseData *rdata)
{
  GtkPrintOperation *op = rdata->op;
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPrintSettings *settings = NULL;
  GtkPageSetup *page_setup = NULL;
  
  if (rdata->loop)
    g_main_loop_quit (rdata->loop);

  if (printer != NULL) 
    {
      rdata->result = GTK_PRINT_OPERATION_RESULT_APPLY;

      rdata->do_print = TRUE;

      if (priv->print_settings)
	settings = gtk_print_settings_copy (priv->print_settings);
      else
	settings = gtk_print_settings_new ();

      gtk_print_settings_set_printer (settings,
				      gtk_printer_get_name (printer));
      
      if (priv->default_page_setup)
	page_setup = gtk_page_setup_copy (priv->default_page_setup);
      else
	page_setup = gtk_page_setup_new ();
  }
  
  finish_print (rdata, printer, page_setup, settings, FALSE);

  if (settings)
    g_object_unref (settings);
  
  if (page_setup)
    g_object_unref (page_setup);
}

void
_gtk_print_operation_platform_backend_run_dialog_async (GtkPrintOperation          *op,
							gboolean                    show_dialog,
                                                        GtkWindow                  *parent,
							GtkPrintOperationPrintFunc  print_cb)
{
  GtkWidget *pd;
  PrintResponseData *rdata;
  const gchar *printer_name;

  rdata = g_new (PrintResponseData, 1);
  rdata->op = g_object_ref (op);
  rdata->do_print = FALSE;
  rdata->do_preview = FALSE;
  rdata->result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  rdata->print_cb = print_cb;
  rdata->parent = parent;
  rdata->loop = NULL;
  rdata->destroy = print_response_data_free;
  
  if (show_dialog)
    {
      pd = get_print_dialog (op, parent);
      gtk_window_set_modal (GTK_WINDOW (pd), TRUE);

      g_signal_connect (pd, "response", 
			G_CALLBACK (handle_print_response), rdata);
      
      gtk_window_present (GTK_WINDOW (pd));
    }
  else
    {
      printer_name = NULL;
      if (op->priv->print_settings)
	printer_name = gtk_print_settings_get_printer (op->priv->print_settings);
      
      find_printer (printer_name, (GFunc) found_printer, rdata);
    }
}

static cairo_status_t
write_preview (void                *closure,
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

static void
close_preview (void *data)
{
  gint fd = GPOINTER_TO_INT (data);

  close (fd);
}

cairo_surface_t *
_gtk_print_operation_platform_backend_create_preview_surface (GtkPrintOperation *op,
							      GtkPageSetup      *page_setup,
							      gdouble           *dpi_x,
							      gdouble           *dpi_y,
							      gchar            **target)
{
  gchar *filename;
  gint fd;
  GtkPaperSize *paper_size;
  gdouble w, h;
  cairo_surface_t *surface;
  static cairo_user_data_key_t key;
  
  filename = g_build_filename (g_get_tmp_dir (), "previewXXXXXX.pdf", NULL);
  fd = g_mkstemp (filename);

  if (fd < 0)
    {
      g_free (filename);
      return NULL;
    }

  *target = filename;
  
  paper_size = gtk_page_setup_get_paper_size (page_setup);
  w = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  h = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
    
  *dpi_x = *dpi_y = 72;
  surface = cairo_pdf_surface_create_for_stream (write_preview, GINT_TO_POINTER (fd), w, h);
 
  cairo_surface_set_user_data (surface, &key, GINT_TO_POINTER (fd), close_preview);

  return surface;
}

void
_gtk_print_operation_platform_backend_preview_start_page (GtkPrintOperation *op,
							  cairo_surface_t   *surface,
							  cairo_t           *cr)
{
}

void
_gtk_print_operation_platform_backend_preview_end_page (GtkPrintOperation *op,
							cairo_surface_t   *surface,
							cairo_t           *cr)
{
  cairo_show_page (cr);
}

void
_gtk_print_operation_platform_backend_resize_preview_surface (GtkPrintOperation *op,
							      GtkPageSetup      *page_setup,
							      cairo_surface_t   *surface)
{
  GtkPaperSize *paper_size;
  gdouble w, h;
  
  paper_size = gtk_page_setup_get_paper_size (page_setup);
  w = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  h = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
  cairo_pdf_surface_set_size (surface, w, h);
}


GtkPrintOperationResult
_gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *op,
						  gboolean           show_dialog,
						  GtkWindow         *parent,
						  gboolean          *do_print)
 {
  GtkWidget *pd;
  PrintResponseData rdata;
  gint response;  
  const gchar *printer_name;
   
  rdata.op = op;
  rdata.do_print = FALSE;
  rdata.do_preview = FALSE;
  rdata.result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  rdata.print_cb = NULL;
  rdata.destroy = NULL;
  rdata.parent = parent;
  rdata.loop = NULL;

  if (show_dialog)
    {
      pd = get_print_dialog (op, parent);

      response = gtk_dialog_run (GTK_DIALOG (pd));
      handle_print_response (pd, response, &rdata);
    }
  else
    {
      printer_name = NULL;
      if (op->priv->print_settings)
	printer_name = gtk_print_settings_get_printer (op->priv->print_settings);
      
      rdata.loop = g_main_loop_new (NULL, FALSE);
      find_printer (printer_name,
		    (GFunc) found_printer, &rdata);

      GDK_THREADS_LEAVE ();  
      g_main_loop_run (rdata.loop);
      GDK_THREADS_ENTER ();  

      g_main_loop_unref (rdata.loop);
      rdata.loop = NULL;
    }

  *do_print = rdata.do_print;
  
  return rdata.result;
}


typedef struct 
{
  GtkPageSetup         *page_setup;
  GtkPageSetupDoneFunc  done_cb;
  gpointer              data;
  GDestroyNotify        destroy;
} PageSetupResponseData;

static void
page_setup_data_free (gpointer data)
{
  PageSetupResponseData *rdata = data;

  if (rdata->page_setup)
    g_object_unref (rdata->page_setup);

  g_free (rdata);
}

static void
handle_page_setup_response (GtkWidget *dialog,
			    gint       response,
			    gpointer   data)
{
  GtkPageSetupUnixDialog *psd;
  PageSetupResponseData *rdata = data;

  psd = GTK_PAGE_SETUP_UNIX_DIALOG (dialog);
  if (response == GTK_RESPONSE_OK)
    rdata->page_setup = gtk_page_setup_unix_dialog_get_page_setup (psd);

  gtk_widget_destroy (dialog);

  if (rdata->done_cb)
    rdata->done_cb (rdata->page_setup, rdata->data);

  if (rdata->destroy)
    rdata->destroy (rdata);
}

static GtkWidget *
get_page_setup_dialog (GtkWindow        *parent,
		       GtkPageSetup     *page_setup,
		       GtkPrintSettings *settings)
{
  GtkWidget *dialog;

  dialog = gtk_page_setup_unix_dialog_new (NULL, parent);
  if (page_setup)
    gtk_page_setup_unix_dialog_set_page_setup (GTK_PAGE_SETUP_UNIX_DIALOG (dialog),
					       page_setup);
  gtk_page_setup_unix_dialog_set_print_settings (GTK_PAGE_SETUP_UNIX_DIALOG (dialog),
						 settings);

  return dialog;
}

/**
 * gtk_print_run_page_setup_dialog:
 * @parent: (allow-none): transient parent
 * @page_setup: (allow-none): an existing #GtkPageSetup
 * @settings: a #GtkPrintSettings
 *
 * Runs a page setup dialog, letting the user modify the values from
 * @page_setup. If the user cancels the dialog, the returned #GtkPageSetup
 * is identical to the passed in @page_setup, otherwise it contains the 
 * modifications done in the dialog.
 *
 * Note that this function may use a recursive mainloop to show the page
 * setup dialog. See gtk_print_run_page_setup_dialog_async() if this is 
 * a problem.
 * 
 * Return value: (transfer full): a new #GtkPageSetup
 *
 * Since: 2.10
 */
GtkPageSetup *
gtk_print_run_page_setup_dialog (GtkWindow        *parent,
				 GtkPageSetup     *page_setup,
				 GtkPrintSettings *settings)
{
  GtkWidget *dialog;
  gint response;
  PageSetupResponseData rdata;  
  
  rdata.page_setup = NULL;
  rdata.done_cb = NULL;
  rdata.data = NULL;
  rdata.destroy = NULL;

  dialog = get_page_setup_dialog (parent, page_setup, settings);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  handle_page_setup_response (dialog, response, &rdata);
 
  if (rdata.page_setup)
    return rdata.page_setup;
  else if (page_setup)
    return gtk_page_setup_copy (page_setup);
  else
    return gtk_page_setup_new ();
}

/**
 * gtk_print_run_page_setup_dialog_async:
 * @parent: (allow-none): transient parent, or %NULL
 * @page_setup: (allow-none): an existing #GtkPageSetup, or %NULL
 * @settings: a #GtkPrintSettings
 * @done_cb: a function to call when the user saves the modified page setup
 * @data: user data to pass to @done_cb
 * 
 * Runs a page setup dialog, letting the user modify the values from @page_setup. 
 *
 * In contrast to gtk_print_run_page_setup_dialog(), this function  returns after 
 * showing the page setup dialog on platforms that support this, and calls @done_cb 
 * from a signal handler for the ::response signal of the dialog.
 *
 * Since: 2.10
 */
void
gtk_print_run_page_setup_dialog_async (GtkWindow            *parent,
				       GtkPageSetup         *page_setup,
				       GtkPrintSettings     *settings,
				       GtkPageSetupDoneFunc  done_cb,
				       gpointer              data)
{
  GtkWidget *dialog;
  PageSetupResponseData *rdata;
  
  dialog = get_page_setup_dialog (parent, page_setup, settings);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  
  rdata = g_new (PageSetupResponseData, 1);
  rdata->page_setup = NULL;
  rdata->done_cb = done_cb;
  rdata->data = data;
  rdata->destroy = page_setup_data_free;

  g_signal_connect (dialog, "response",
		    G_CALLBACK (handle_page_setup_response), rdata);
 
  gtk_window_present (GTK_WINDOW (dialog));
 }

struct _PrinterFinder 
{
  gboolean found_printer;
  GFunc func;
  gpointer data;
  gchar *printer_name;
  GList *backends;
  guint timeout_tag;
  GtkPrinter *printer;
  GtkPrinter *default_printer;
  GtkPrinter *first_printer;
};

static gboolean
find_printer_idle (gpointer data)
{
  PrinterFinder *finder = data;
  GtkPrinter *printer;

  if (finder->printer != NULL)
    printer = finder->printer;
  else if (finder->default_printer != NULL)
    printer = finder->default_printer;
  else if (finder->first_printer != NULL)
    printer = finder->first_printer;
  else
    printer = NULL;

  finder->func (printer, finder->data);
  
  printer_finder_free (finder);

  return FALSE;
}

static void
printer_added_cb (GtkPrintBackend *backend, 
                  GtkPrinter      *printer, 
		  PrinterFinder   *finder)
{
  if (finder->found_printer)
    return;

  /* FIXME this skips "Print to PDF" - is this intentional ? */
  if (gtk_printer_is_virtual (printer))
    return;

  if (finder->printer_name != NULL &&
      strcmp (gtk_printer_get_name (printer), finder->printer_name) == 0)
    {
      finder->printer = g_object_ref (printer);
      finder->found_printer = TRUE;
    }
  else if (finder->default_printer == NULL &&
	   gtk_printer_is_default (printer))
    {
      finder->default_printer = g_object_ref (printer);
      if (finder->printer_name == NULL)
	finder->found_printer = TRUE;
    }
  else
    if (finder->first_printer == NULL)
      finder->first_printer = g_object_ref (printer);
  
  if (finder->found_printer)
    g_idle_add (find_printer_idle, finder);
}

static void
printer_list_done_cb (GtkPrintBackend *backend, 
		      PrinterFinder   *finder)
{
  finder->backends = g_list_remove (finder->backends, backend);
  
  g_signal_handlers_disconnect_by_func (backend, printer_added_cb, finder);
  g_signal_handlers_disconnect_by_func (backend, printer_list_done_cb, finder);
  
  gtk_print_backend_destroy (backend);
  g_object_unref (backend);

  if (finder->backends == NULL && !finder->found_printer)
    g_idle_add (find_printer_idle, finder);
}

static void
find_printer_init (PrinterFinder   *finder,
		   GtkPrintBackend *backend)
{
  GList *list;
  GList *node;

  list = gtk_print_backend_get_printer_list (backend);

  node = list;
  while (node != NULL)
    {
      printer_added_cb (backend, node->data, finder);
      node = node->next;

      if (finder->found_printer)
	break;
    }

  g_list_free (list);

  if (gtk_print_backend_printer_list_is_done (backend))
    {
      finder->backends = g_list_remove (finder->backends, backend);
      gtk_print_backend_destroy (backend);
      g_object_unref (backend);
    }
  else
    {
      g_signal_connect (backend, "printer-added", 
			(GCallback) printer_added_cb, 
			finder);
      g_signal_connect (backend, "printer-list-done", 
			(GCallback) printer_list_done_cb, 
			finder);
    }

}

static void
printer_finder_free (PrinterFinder *finder)
{
  GList *l;
  
  g_free (finder->printer_name);
  
  if (finder->printer)
    g_object_unref (finder->printer);
  
  if (finder->default_printer)
    g_object_unref (finder->default_printer);
  
  if (finder->first_printer)
    g_object_unref (finder->first_printer);

  for (l = finder->backends; l != NULL; l = l->next)
    {
      GtkPrintBackend *backend = l->data;
      g_signal_handlers_disconnect_by_func (backend, printer_added_cb, finder);
      g_signal_handlers_disconnect_by_func (backend, printer_list_done_cb, finder);
      gtk_print_backend_destroy (backend);
      g_object_unref (backend);
    }
  
  g_list_free (finder->backends);
  
  g_free (finder);
}

static void 
find_printer (const gchar *printer,
	      GFunc        func,
	      gpointer     data)
{
  GList *node, *next;
  PrinterFinder *finder;

  finder = g_new0 (PrinterFinder, 1);

  finder->printer_name = g_strdup (printer);
  finder->func = func;
  finder->data = data;
  
  finder->backends = NULL;
  if (g_module_supported ())
    finder->backends = gtk_print_backend_load_modules ();

  for (node = finder->backends; !finder->found_printer && node != NULL; node = next)
    {
      next = node->next;
      find_printer_init (finder, GTK_PRINT_BACKEND (node->data));
    }

  if (finder->backends == NULL && !finder->found_printer)
    g_idle_add (find_printer_idle, finder);
}

#define __GTK_PRINT_OPERATION_UNIX_C__
#include "gtkaliasdef.c"
