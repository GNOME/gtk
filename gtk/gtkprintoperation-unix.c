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
  GtkWindow *parent;

  op_unix = (GtkPrintOperationUnix *) user_data;

  parent = op_unix->parent;

  if (error != NULL)
    {
      GtkWidget *edialog;
      edialog = gtk_message_dialog_new (parent, 
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

GtkPrintOperationResult
_gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation  *op,
						  GtkWindow          *parent,
						  gboolean           *do_print,
						  GError            **error)
{
  GtkWidget *pd;
  GtkPrintOperationResult result;
  GtkPageSetup *page_setup;
  
  result = GTK_PRINT_OPERATION_RESULT_CANCEL;

  if (op->priv->default_page_setup)
    page_setup = gtk_page_setup_copy (op->priv->default_page_setup);
  else
    page_setup = gtk_page_setup_new ();

  pd = gtk_print_unix_dialog_new (NULL, parent);

  if (op->priv->print_settings)
    gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG (pd),
					op->priv->print_settings);

  gtk_print_unix_dialog_set_page_setup (GTK_PRINT_UNIX_DIALOG (pd), page_setup);
  
  *do_print = FALSE; 
  if (gtk_dialog_run (GTK_DIALOG (pd)) == GTK_RESPONSE_OK)
    {
      GtkPrintOperationUnix *op_unix;
      GtkPrinter *printer;
      GtkPrintSettings *settings;

      result = GTK_PRINT_OPERATION_RESULT_APPLY;
      
      printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (pd));
      if (printer == NULL)
	goto out;
      
      *do_print = TRUE;

      settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (pd));
      gtk_print_operation_set_print_settings (op, settings);

      op_unix = g_new0 (GtkPrintOperationUnix, 1);
      op_unix->job = gtk_print_job_new (op->priv->job_name,
					printer,
					settings,
					page_setup);
      g_object_unref (settings);

      op->priv->surface = gtk_print_job_get_surface (op_unix->job, error);
      if (op->priv->surface == NULL)
        {
	  *do_print = FALSE;
	  op_unix_free (op_unix);
	  result = GTK_PRINT_OPERATION_RESULT_ERROR;
	  goto out;
	}

      _gtk_print_operation_set_status (op, gtk_print_job_get_status (op_unix->job), NULL);
      op_unix->job_status_changed_tag =
	g_signal_connect (op_unix->job, "status_changed",
			  G_CALLBACK (job_status_changed_cb), op);
      
      op_unix->parent = parent;

      op->priv->dpi_x = 72;
      op->priv->dpi_y = 72;
 
      op->priv->platform_data = op_unix;
      op->priv->free_platform_data = (GDestroyNotify) op_unix_free;

      op->priv->print_pages = op_unix->job->print_pages;
      op->priv->page_ranges = op_unix->job->page_ranges;
      op->priv->num_page_ranges = op_unix->job->num_page_ranges;
  
      op->priv->manual_num_copies = op_unix->job->num_copies;
      op->priv->manual_collation = op_unix->job->collate;
      op->priv->manual_reverse = op_unix->job->reverse;
      op->priv->manual_page_set = op_unix->job->page_set;
      op->priv->manual_scale = op_unix->job->scale;
      op->priv->manual_orientation = op_unix->job->rotate_to_orientation;
    } 

  op->priv->start_page = unix_start_page;
  op->priv->end_page = unix_end_page;
  op->priv->end_run = unix_end_run;

 out:
  g_object_unref (page_setup); 
  
  gtk_widget_destroy (pd);

  return result;
}

/**
 * gtk_print_run_page_setup_dialog:
 * @parent: transient parent, or %NULL
 * @page_setup: an existing #GtkPageSetup, or %NULL
 * @settings: a #GtkPrintSettings
 * 
 * Runs a page setup dialog, letting the user modify 
 * the values from @page_setup. If the user cancels
 * the dialog, the returned #GtkPageSetup is identical
 * to the passed in @page_setup, otherwise it contains
 * the modifications done in the dialog.
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
  GtkPageSetup *new_page_setup;
  
  dialog = gtk_page_setup_unix_dialog_new (NULL, parent);
  if (page_setup)
    gtk_page_setup_unix_dialog_set_page_setup (GTK_PAGE_SETUP_UNIX_DIALOG (dialog),
					       page_setup);
  gtk_page_setup_unix_dialog_set_print_settings (GTK_PAGE_SETUP_UNIX_DIALOG (dialog),
						 settings);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    new_page_setup = gtk_page_setup_unix_dialog_get_page_setup (GTK_PAGE_SETUP_UNIX_DIALOG (dialog));
  else 
    {
      if (page_setup)
	new_page_setup = gtk_page_setup_copy (page_setup);
      else
	new_page_setup = gtk_page_setup_new ();
    }
      
  gtk_widget_destroy (dialog);
  
  return new_page_setup;
}


#define __GTK_PRINT_OPERATION_UNIX_C__
#include "gtkaliasdef.c"
