/* GTK - The GIMP Toolkit
 * gtkprintoperation-unix.c: Print Operation Details for Unix and Unix like platforms
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

#include "gtkprintoperation-private.h"
#include "gtkmarshal.h"
#include "gtkmessagedialog.h"

#include "gtkprintunixdialog.h"
#include "gtkpagesetupunixdialog.h"
#include "gtkprintbackend.h"
#include "gtkprinter.h"
#include "gtkprintjob.h"
#include "gtkalias.h"

typedef struct {
  GtkPrintJob *job;         /* the job we are sending to the printer */
  gulong job_status_changed_tag;
  GtkWindow *parent;        /* just in case we need to throw error dialogs */
} GtkPrintOperationUnix;

static void
unix_start_page (GtkPrintOperation *op,
		 GtkPrintContext   *print_context,
		 GtkPageSetup      *page_setup)
{

}

static void
unix_end_page (GtkPrintOperation *op,
	       GtkPrintContext   *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo (print_context);
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

static void
unix_finish_send  (GtkPrintJob *job,
                   void        *user_data, 
                   GError      *error)
{
  GtkPrintOperationUnix *op_unix;

  op_unix = (GtkPrintOperationUnix *) user_data;

  if (error != NULL)
    {
      GtkWidget *edialog;
      edialog = gtk_message_dialog_new (op_unix->parent, 
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "Error printing: %s",
                                        error->message);

      gtk_dialog_run (GTK_DIALOG (edialog));
      gtk_widget_destroy (edialog);
    }
}

static void
unix_end_run (GtkPrintOperation *op)
{
  GtkPrintOperationUnix *op_unix = op->priv->platform_data;
 
  /* TODO: Check for error */
  gtk_print_job_send (op_unix->job,
                      unix_finish_send, 
                      op_unix, NULL,
		      NULL);
}

static void
job_status_changed_cb (GtkPrintJob       *job, 
		       GtkPrintOperation *op)
{
  _gtk_print_operation_set_status (op, gtk_print_job_get_status (job), NULL);
}


static GtkWidget *
get_print_dialog (GtkPrintOperation *op,
                  GtkWindow         *parent)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkWidget *pd;
  GtkPageSetup *page_setup;

  pd = gtk_print_unix_dialog_new (NULL, parent);

  if (priv->print_settings)
    gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG (pd),
					priv->print_settings);
  if (priv->default_page_setup)
    page_setup = gtk_page_setup_copy (priv->default_page_setup);
  else
    page_setup = gtk_page_setup_new ();

  gtk_print_unix_dialog_set_page_setup (GTK_PRINT_UNIX_DIALOG (pd), 
                                        page_setup);
  g_object_unref (page_setup);

  return pd;
}
  
typedef struct {
  GtkPrintOperation           *op;
  gboolean                     do_print;
  GError                     **error;
  GtkPrintOperationResult      result;
  GtkPrintOperationPrintFunc   print_cb;
  GDestroyNotify               destroy;
} PrintResponseData;

static void
print_response_data_free (gpointer data)
{
  PrintResponseData *rdata = data;

  g_object_unref (rdata->op);
  g_free (rdata);
}

static void
handle_print_response (GtkWidget *dialog,
		       gint       response,
		       gpointer   data)
{
  GtkPrintUnixDialog *pd = GTK_PRINT_UNIX_DIALOG (dialog);
  PrintResponseData *rdata = data;
  GtkPrintOperation *op = rdata->op;
  GtkPrintOperationPrivate *priv = op->priv;

  if (response == GTK_RESPONSE_OK)
    {
      GtkPrintOperationUnix *op_unix;
      GtkPrinter *printer;
      GtkPrintSettings *settings;
      GtkPageSetup *page_setup;

      rdata->result = GTK_PRINT_OPERATION_RESULT_APPLY;

      printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (pd));
      if (printer == NULL)
	goto out;
      
      rdata->do_print = TRUE;

      settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (pd));
      page_setup = gtk_print_unix_dialog_get_page_setup (GTK_PRINT_UNIX_DIALOG (pd));

      gtk_print_operation_set_print_settings (op, settings);

      op_unix = g_new0 (GtkPrintOperationUnix, 1);
      op_unix->job = gtk_print_job_new (priv->job_name,
					printer,
					settings,
					page_setup);
      g_object_unref (settings);
  
      rdata->op->priv->surface = gtk_print_job_get_surface (op_unix->job, rdata->error);
      if (op->priv->surface == NULL)
        {
	  rdata->do_print = FALSE;
	  op_unix_free (op_unix);
	  rdata->result = GTK_PRINT_OPERATION_RESULT_ERROR;
	  goto out;
	}

      _gtk_print_operation_set_status (op, gtk_print_job_get_status (op_unix->job), NULL);
      op_unix->job_status_changed_tag =
	g_signal_connect (op_unix->job, "status_changed",
			  G_CALLBACK (job_status_changed_cb), op);
      
      op_unix->parent = gtk_window_get_transient_for (GTK_WINDOW (pd));

      priv->dpi_x = 72;
      priv->dpi_y = 72;
 
      priv->platform_data = op_unix;
      priv->free_platform_data = (GDestroyNotify) op_unix_free;

      priv->print_pages = op_unix->job->print_pages;
      priv->page_ranges = op_unix->job->page_ranges;
      priv->num_page_ranges = op_unix->job->num_page_ranges;
  
      priv->manual_num_copies = op_unix->job->num_copies;
      priv->manual_collation = op_unix->job->collate;
      priv->manual_reverse = op_unix->job->reverse;
      priv->manual_page_set = op_unix->job->page_set;
      priv->manual_scale = op_unix->job->scale;
      priv->manual_orientation = op_unix->job->rotate_to_orientation;
    } 

  priv->start_page = unix_start_page;
  priv->end_page = unix_end_page;
  priv->end_run = unix_end_run;

 out:  
  gtk_widget_destroy (GTK_WIDGET (pd));

  if (rdata->print_cb)
    {
      if (rdata->do_print)
        rdata->print_cb (op); 
      else
       _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL); 
    }

  if (rdata->destroy)
    rdata->destroy (rdata);
}

void
_gtk_print_operation_platform_backend_run_dialog_async (GtkPrintOperation          *op,
                                                        GtkWindow                  *parent,
							GtkPrintOperationPrintFunc  print_cb)
{
  GtkWidget *pd;
  PrintResponseData *rdata;

  rdata = g_new (PrintResponseData, 1);
  rdata->op = g_object_ref (op);
  rdata->do_print = FALSE;
  rdata->result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  rdata->error = NULL;
  rdata->print_cb = print_cb;
  rdata->destroy = print_response_data_free;
  
  pd = get_print_dialog (op, parent);
  gtk_window_set_modal (GTK_WINDOW (pd), TRUE);

  g_signal_connect (pd, "response", 
		    G_CALLBACK (handle_print_response), rdata);

  gtk_window_present (GTK_WINDOW (pd));
}

GtkPrintOperationResult
_gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *op,
						  GtkWindow         *parent,
						  gboolean          *do_print,
						  GError           **error)
 {
  GtkWidget *pd;
  PrintResponseData rdata;
  gint response;  
   
  rdata.op = op;
  rdata.do_print = FALSE;
  rdata.result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  rdata.error = error;
  rdata.print_cb = NULL;
  rdata.destroy = NULL;

  pd = get_print_dialog (op, parent);

  response = gtk_dialog_run (GTK_DIALOG (pd));
  handle_print_response (pd, response, &rdata);

  *do_print = rdata.do_print;

  return rdata.result;
}


typedef struct {
  GtkPageSetup  *page_setup;
  GFunc          done_cb;
  gpointer       data;
  GDestroyNotify destroy;
} PageSetupResponseData;

static void
page_setup_data_free (gpointer data)
{
  PageSetupResponseData *rdata = data;

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
 * @parent: transient parent, or %NULL
 * @page_setup: an existing #GtkPageSetup, or %NULL
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
 * Return value: a new #GtkPageSetup
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
 * @parent: transient parent, or %NULL
 * @page_setup: an existing #GtkPageSetup, or %NULL
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


#define __GTK_PRINT_OPERATION_UNIX_C__
#include "gtkaliasdef.c"
