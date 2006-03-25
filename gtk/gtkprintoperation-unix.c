/* EGG - The GIMP Toolkit
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

#include "gtkprintunixdialog.h"
#include "gtkpagesetupunixdialog.h"
#include "gtkprintbackend.h"
#include "gtkprinter.h"
#include "gtkprintjob.h"
#include "gtkalias.h"

typedef struct {
  GtkPrintJob *job;         /* the job we are sending to the printer */
  GtkWindow *parent;        /* parent window just in case we need to throw error dialogs */
} GtkPrintOperationUnix;

static void
unix_start_page (GtkPrintOperation *op,
		 GtkPrintContext *print_context,
		 GtkPageSetup *page_setup)
{

}

static void
unix_end_page (GtkPrintOperation *op,
	       GtkPrintContext *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo (print_context);
  cairo_show_page (cr);
}

static void
_op_unix_free (GtkPrintOperationUnix *op_unix)
{
  if (op_unix->job)
    g_object_unref (G_OBJECT (op_unix->job));

  g_free (op_unix);
}

static void
unix_finish_send  (GtkPrintJob *job,
                   void *user_data, 
                   GError **error)
{
  GtkPrintOperationUnix *op_unix;
  GtkWindow *parent;

  op_unix = (GtkPrintOperationUnix *) user_data;

  parent = op_unix->parent;

  _op_unix_free (op_unix);

  if (error != NULL && *error != NULL)
    {
      GtkWidget *edialog;
      GError *err = *error;

      edialog = gtk_message_dialog_new (parent, 
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "Error printing: %s",
                                        err->message);

      gtk_dialog_run (GTK_DIALOG (edialog));
      gtk_widget_destroy (edialog);
    }
}

static void
unix_end_run (GtkPrintOperation *op)
{
  GtkPrintOperationUnix *op_unix = op->priv->platform_data;
 
  /* TODO: Check for error */
  gtk_print_job_send (g_object_ref (op_unix->job),
                      unix_finish_send, 
                      op_unix, NULL);

  op->priv->platform_data = NULL;
}

GtkPrintOperationResult
_gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *op,
						  GtkWindow *parent,
						  gboolean *do_print,
						  GError **error)
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
  if (gtk_dialog_run (GTK_DIALOG (pd)) == GTK_RESPONSE_ACCEPT)
    {
      GtkPrintOperationUnix *op_unix;
      GtkPrinter *printer;
      GtkPrintSettings *settings, *settings_copy;

      result = GTK_PRINT_OPERATION_RESULT_APPLY;
      
      printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (pd));
      if (printer == NULL)
	goto out;
      
      *do_print = TRUE;

      settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (pd));

      /* We save a copy to return to the user to avoid exposing
	 the extra settings preparint the printer job adds. */
      settings_copy = gtk_print_settings_copy (settings);
      gtk_print_operation_set_print_settings (op, settings_copy);
      g_object_unref (settings_copy);

      op_unix = g_new (GtkPrintOperationUnix, 1);
      op_unix->job = gtk_printer_prep_job (printer,
					   settings,
					   page_setup,
                                           "Title",
					   error);
      g_object_unref (settings);
    
      if (error != NULL && *error != NULL)
        {
	  *do_print = FALSE;
	  _op_unix_free (op_unix);
	  result = GTK_PRINT_OPERATION_RESULT_ERROR;
	  goto out;
	}

      op_unix->parent = parent;

      op->priv->surface = gtk_print_job_get_surface (op_unix->job);

      op->priv->dpi_x = 72;
      op->priv->dpi_y = 72;
 
      op->priv->platform_data = op_unix;

      /* TODO: hook up to dialog elements */
      op->priv->manual_num_copies =
	gtk_print_settings_get_int_with_default (settings, "manual-num-copies", 1);
      op->priv->manual_collation =
	gtk_print_settings_get_bool (settings, "manual-collate");
      op->priv->manual_scale =
	gtk_print_settings_get_double_with_default (settings, "manual-scale", 100.0);
      op->priv->manual_orientation =
	gtk_print_settings_get_bool (settings, "manual-orientation");
    } 

  op->priv->start_page = unix_start_page;
  op->priv->end_page = unix_end_page;
  op->priv->end_run = unix_end_run;

 out:
  g_object_unref (page_setup); 
  
  gtk_widget_destroy (pd);

  return result;
}

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
  gtk_dialog_run (GTK_DIALOG (dialog));

  new_page_setup = gtk_page_setup_unix_dialog_get_page_setup (GTK_PAGE_SETUP_UNIX_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return new_page_setup;
}


#define __GTK_PRINT_OPERATION_UNIX_C__
#include "gtkaliasdef.c"
