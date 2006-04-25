/* GTK - The GIMP Toolkit
 * gtkprintoperation.c: Print Operation
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
#include "string.h"
#include "gtkprintoperation-private.h"
#include "gtkmarshalers.h"
#include <cairo-pdf.h>
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define GTK_PRINT_OPERATION_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_PRINT_OPERATION, GtkPrintOperationPrivate))

enum {
  BEGIN_PRINT,
  REQUEST_PAGE_SETUP,
  DRAW_PAGE,
  END_PRINT,
  STATUS_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_DEFAULT_PAGE_SETUP,
  PROP_PRINT_SETTINGS,
  PROP_JOB_NAME,
  PROP_NR_OF_PAGES,
  PROP_CURRENT_PAGE,
  PROP_USE_FULL_PAGE,
  PROP_UNIT,
  PROP_SHOW_DIALOG,
  PROP_PDF_TARGET,
  PROP_STATUS,
  PROP_STATUS_STRING
};

static guint signals[LAST_SIGNAL] = { 0 };
static int job_nr = 0;

G_DEFINE_TYPE (GtkPrintOperation, gtk_print_operation, G_TYPE_OBJECT)

/**
 * gtk_print_error_quark:
 *
 * Registers an error quark for #GtkPrintOperation if necessary.
 * 
 * Return value: The error quark used for #GtkPrintOperation errors.
 *
 * Since: 2.10
 **/
GQuark     
gtk_print_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("gtk-print-error-quark");
  return quark;
}
     
static void
gtk_print_operation_finalize (GObject *object)
{
  GtkPrintOperation *print_operation = GTK_PRINT_OPERATION (object);

  if (print_operation->priv->free_platform_data &&
      print_operation->priv->platform_data)
    {
      print_operation->priv->free_platform_data (print_operation->priv->platform_data);
      print_operation->priv->free_platform_data = NULL;
    }

  if (print_operation->priv->default_page_setup)
    g_object_unref (print_operation->priv->default_page_setup);
  
  if (print_operation->priv->print_settings)
    g_object_unref (print_operation->priv->print_settings);
  
  g_free (print_operation->priv->pdf_target);
  g_free (print_operation->priv->job_name);

  G_OBJECT_CLASS (gtk_print_operation_parent_class)->finalize (object);
}

static void
gtk_print_operation_init (GtkPrintOperation *operation)
{
  const char *appname;

  operation->priv = GTK_PRINT_OPERATION_GET_PRIVATE (operation);

  operation->priv->status = GTK_PRINT_STATUS_INITIAL;
  operation->priv->status_string = g_strdup ("");
  operation->priv->default_page_setup = NULL;
  operation->priv->print_settings = NULL;
  operation->priv->nr_of_pages = -1;
  operation->priv->current_page = -1;
  operation->priv->use_full_page = FALSE;
  operation->priv->show_dialog = TRUE;
  operation->priv->pdf_target = NULL;

  operation->priv->unit = GTK_UNIT_PIXEL;

  appname = g_get_application_name ();
  operation->priv->job_name = g_strdup_printf ("%s job #%d",
					       appname, ++job_nr);
}

static void
gtk_print_operation_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkPrintOperation *op = GTK_PRINT_OPERATION (object);
  
  switch (prop_id)
    {
    case PROP_DEFAULT_PAGE_SETUP:
      gtk_print_operation_set_default_page_setup (op, g_value_get_object (value));
      break;
    case PROP_PRINT_SETTINGS:
      gtk_print_operation_set_print_settings (op, g_value_get_object (value));
      break;
    case PROP_JOB_NAME:
      gtk_print_operation_set_job_name (op, g_value_get_string (value));
      break;
    case PROP_NR_OF_PAGES:
      gtk_print_operation_set_nr_of_pages (op, g_value_get_int (value));
      break;
    case PROP_CURRENT_PAGE:
      gtk_print_operation_set_current_page (op, g_value_get_int (value));
      break;
    case PROP_USE_FULL_PAGE:
      gtk_print_operation_set_use_full_page (op, g_value_get_boolean (value));
      break;
    case PROP_UNIT:
      gtk_print_operation_set_unit (op, g_value_get_enum (value));
      break;
    case PROP_SHOW_DIALOG:
      gtk_print_operation_set_show_dialog (op, g_value_get_boolean (value));
      break;
    case PROP_PDF_TARGET:
      gtk_print_operation_set_pdf_target (op, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_operation_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GtkPrintOperation *op = GTK_PRINT_OPERATION (object);
  
  switch (prop_id)
    {
    case PROP_DEFAULT_PAGE_SETUP:
      g_value_set_object (value, op->priv->default_page_setup);
      break;
    case PROP_PRINT_SETTINGS:
      g_value_set_object (value, op->priv->print_settings);
      break;
    case PROP_JOB_NAME:
      g_value_set_string (value, op->priv->job_name);
      break;
    case PROP_NR_OF_PAGES:
      g_value_set_int (value, op->priv->nr_of_pages);
      break;
    case PROP_CURRENT_PAGE:
      g_value_set_int (value, op->priv->current_page);
      break;      
    case PROP_USE_FULL_PAGE:
      g_value_set_boolean (value, op->priv->use_full_page);
      break;
    case PROP_UNIT:
      g_value_set_enum (value, op->priv->unit);
      break;
    case PROP_SHOW_DIALOG:
      g_value_set_boolean (value, op->priv->show_dialog);
      break;
    case PROP_PDF_TARGET:
      g_value_set_string (value, op->priv->pdf_target);
      break;
    case PROP_STATUS:
      g_value_set_enum (value, op->priv->status);
      break;
    case PROP_STATUS_STRING:
      g_value_set_string (value, op->priv->status_string);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_operation_class_init (GtkPrintOperationClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->set_property = gtk_print_operation_set_property;
  gobject_class->get_property = gtk_print_operation_get_property;
  gobject_class->finalize = gtk_print_operation_finalize;
  
  g_type_class_add_private (gobject_class, sizeof (GtkPrintOperationPrivate));

  /**
   * GtkPrintOperation::begin-print:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Gets emitted after the user has finished changing print settings
   * in the dialog, before the actual rendering starts. 
   *
   * A typical use for this signal is to use the parameters from the
   * #GtkPrintContext and paginate the document accordingly, and then
   * set the number of pages with gtk_print_operation_set_nr_of_pages().
   *
   * Since: 2.10
   */
  signals[BEGIN_PRINT] =
    g_signal_new ("begin_print",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, begin_print),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINT_CONTEXT);

  /**
   * GtkPrintOperation::request-page-setup:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   * @page_nr: the number of the currently printed page
   * @setup: the #GtkPageSetup 
   * 
   * Gets emitted once for every page that is printed, to give
   * the application a chance to modify the page setup. Any changes 
   * done to @setup will be in force only for printing this page.
   *
   * Since: 2.10
   */
  signals[REQUEST_PAGE_SETUP] =
    g_signal_new ("request_page_setup",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, request_page_setup),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT_OBJECT,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_PRINT_CONTEXT,
		  G_TYPE_INT,
		  GTK_TYPE_PAGE_SETUP);

  /**
   * GtkPrintOperation::draw-page:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   * @page_nr: the number of the currently printed page
   *
   * Gets emitted for every page that is printed. The signal handler
   * must render the @page_nr's page onto the cairo context obtained
   * from @context using gtk_print_context_get_cairo().
   *
   * <informalexample><programlisting>
   *  FIXME: need an example here
   * </programlisting></informalexample>
   *
   * Use gtk_print_operation_set_use_full_page() and 
   * gtk_print_operation_set_unit() before starting the print operation
   * to set up the transformation of the cairo context according to your
   * needs.
   * 
   * Since: 2.10
   */
  signals[DRAW_PAGE] =
    g_signal_new ("draw_page",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, draw_page),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_PRINT_CONTEXT,
		  G_TYPE_INT);

  /**
   * GtkPrintOperation::end-print:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Gets emitted after all pages have been rendered. 
   * A handler for this signal can clean up any resources that have
   * been allocated in the ::begin-print handler.
   * 
   * Since: 2.10
   */
  signals[END_PRINT] =
    g_signal_new ("end_print",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, end_print),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINT_CONTEXT);

  /**
   * GtkPrintOperation::status-changed:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   *
   * Gets emitted at between the various phases of the print operation.
   * See #GtkPrintStatus for the phases that are being discriminated.
   * Use gtk_print_operation_get_status() to find out the current
   * status.
   *
   * Since: 2.10
   */
  signals[STATUS_CHANGED] =
   g_signal_new ("status-changed",
                 G_TYPE_FROM_CLASS (class),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET (GtkPrintOperationClass, status_changed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  /**
   * GtkPrintOperation:default-page-setup:
   *
   * The #GtkPageSetup used by default.
   * 
   * This page setup will be used by gtk_print_operation_run(),
   * but it can be overridden on a per-page basis by connecting
   * to the ::request-page-setup signal.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_DEFAULT_PAGE_SETUP,
				   g_param_spec_object ("default-page-setup",
							P_("Default Page Setup"),
							P_("The GtkPageSetup used by default"),
							GTK_TYPE_PAGE_SETUP,
							GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:print-settings:
   *
   * The #GtkPrintSettings used for initializing the dialog.
   *
   * Setting this property is typically used to re-establish print 
   * settings from a previous print operation, see gtk_print_operation_run().
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_PRINT_SETTINGS,
				   g_param_spec_object ("print-settings",
							P_("Print Settings"),
							P_("The GtkPrintSettings used for initializing the dialog"),
							GTK_TYPE_PRINT_SETTINGS,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:job-name:
   *
   * A string used to identify the job (e.g. in monitoring applications like eggcups). 
   * 
   * If you don't set a job name, GTK+ picks a default one by numbering successive 
   * print jobs.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_JOB_NAME,
				   g_param_spec_string ("job-name",
							P_("Job Name"),
							P_("A string used for identifying the print job."),
							"",
							GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:number-of-pages:
   *
   * The number of pages in the document. 
   *
   * This <emphasis>must</emphasis> be set to a positive number
   * before the rendering starts. It may be set in a ::begin-print signal hander.
   *
   * Note that the page numbers passed to the ::request-page-setup and ::draw-page 
   * signals are 0-based, i.e. if the user chooses to print all pages, the last 
   * ::draw-page signal will be for page @n_pages - 1.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_NR_OF_PAGES,
				   g_param_spec_int ("number-of-pages",
						     P_("Number of Pages"),
						     P_("The number of pages in the document."),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:current-page:
   *
   * The current page in the document.
   *
   * If this is set before gtk_print_operation_run(), 
   * the user will be able to select to print only the current page.
   *
   * Note that this only makes sense for pre-paginated documents.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_CURRENT_PAGE,
				   g_param_spec_int ("current-page",
						     P_("Current Page"),
						     P_("The current page in the document."),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:use-full-page:
   *
   * If %TRUE, the transformation for the cairo context obtained from 
   * #GtkPrintContext puts the origin at the top left corner of the page 
   * (which may not be the top left corner of the sheet, depending on page 
   * orientation and the number of pages per sheet). Otherwise, the origin 
   * is at the top left corner of the imageable area (i.e. inside the margins).
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_USE_FULL_PAGE,
				   g_param_spec_boolean ("use-full-page",
							 P_("Use full page"),
							 P_("%TRUE if the the origin of the context should be at the corner of the page and not the corner of the imageable area"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:unit:
   *
   * The transformation for the cairo context obtained from
   * #GtkPrintContext is set up in such a way that distances are measured 
   * in units of @unit.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_UNIT,
				   g_param_spec_enum ("unit",
						      P_("Unit"),
						      P_("The unit in which distances can be measured in the context"),
						      GTK_TYPE_UNIT,
						      GTK_UNIT_PIXEL,
						      GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:show-dialog:
   *
   * Determines whether calling gtk_print_operation_run() will present
   * a print dialog to the user, or just print to the default printer.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_DIALOG,
				   g_param_spec_boolean ("show-dialog",
							 P_("Show Dialog"),
							 P_("%TRUE if gtk_print_operation_run() should show the print dialog."),
							 TRUE,
							 GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:pdf-target:
   *
   * The name of a PDF file to generate instead of showing the print dialog. 
   *
   * The indended use of this property is for implementing "Export to PDF" 
   * actions.
   *
   * "Print to PDF" support is independent of this and is done
   * by letting the user pick the "Print to PDF" item from the list
   * of printers in the print dialog.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_JOB_NAME,
				   g_param_spec_string ("pdf-target",
							P_("PDF target filename"),
							P_(""),
							NULL,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:status:
   *
   * The status of the print operation.
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_STATUS,
				   g_param_spec_enum ("status",
						      P_("Status"),
						      P_("The status of the print operation"),
						      GTK_TYPE_PRINT_STATUS,
						      GTK_PRINT_STATUS_INITIAL,
						      GTK_PARAM_READABLE));
  
  /**
   * GtkPrintOperation:status-string:
   *
   * A string representation of the status of the print operation. 
   * The string is translated and suitable for displaying the print 
   * status e.g. in a #GtkStatusbar.
   *
   * See the ::status property for a status value that is suitable 
   * for programmatic use. 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_STATUS_STRING,
				   g_param_spec_string ("status-string",
							P_("Status String"),
							P_("A human-readable description of the status"),
							"",
							GTK_PARAM_READABLE));
  

}

/**
 * gtk_print_operation_new:
 *
 * Creates a new #GtkPrintOperation. 
 *
 * Returns: a new #GtkPrintOperation
 *
 * Since: 2.10
 */
GtkPrintOperation *
gtk_print_operation_new (void)
{
  GtkPrintOperation *print_operation;

  print_operation = g_object_new (GTK_TYPE_PRINT_OPERATION, NULL);
  
  return print_operation;
}

/**
 * gtk_print_operation_set_default_page_setup:
 * @op: a #GtkPrintOperation
 * @default_page_setup: a #GtkPageSetup, or %NULL
 * 
 * Makes @default_page_setup the default page setup for @op.
 *
 * This page setup will be used by gtk_print_operation_run(),
 * but it can be overridden on a per-page basis by connecting
 * to the ::request-page-setup signal.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_default_page_setup (GtkPrintOperation *op,
					    GtkPageSetup      *default_page_setup)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (default_page_setup == NULL || 
                    GTK_IS_PAGE_SETUP (default_page_setup));

  if (default_page_setup != op->priv->default_page_setup)
    {
      if (default_page_setup)
	g_object_ref (default_page_setup);
      
      if (op->priv->default_page_setup)
	g_object_unref (op->priv->default_page_setup);
      
      op->priv->default_page_setup = default_page_setup;
     
      g_object_notify (G_OBJECT (op), "default-page-setup");
    }
}

/**
 * gtk_print_operation_get_default_page_setup:
 * @op: a #GtkPrintOperation
 *
 * Returns the default page setup, see 
 * gtk_print_operation_set_default_page_setup().
 *
 * Returns: the default page setup 
 *
 * Since: 2.10
 */
GtkPageSetup *
gtk_print_operation_get_default_page_setup (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), NULL);

  return op->priv->default_page_setup;
}


/**
 * gtk_print_operation_set_print_settings:
 * @op: a #GtkPrintOperation
 * @print_settings: #GtkPrintSettings, or %NULL
 * 
 * Sets the print settings for @op. This is typically used to
 * re-establish print settings from a previous print operation,
 * see gtk_print_operation_run().
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_print_settings (GtkPrintOperation *op,
					GtkPrintSettings  *print_settings)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (print_settings == NULL || 
                    GTK_IS_PRINT_SETTINGS (print_settings));

  if (print_settings != op->priv->print_settings)
    {
      if (print_settings)
        g_object_ref (print_settings);

      if (op->priv->print_settings)
        g_object_unref (op->priv->print_settings);
  
      op->priv->print_settings = print_settings;

      g_object_notify (G_OBJECT (op), "print-settings");
    }
}

/**
 * gtk_print_operation_get_print_settings:
 * @op: a #GtkPrintOperation
 * 
 * Returns the current print settings. 
 *
 * Note that the return value is %NULL until either 
 * gtk_print_operation_set_print_settings() or 
 * gtk_print_operation_run() have been called.
 * 
 * Return value: the current print settings of @op.
 * 
 * Since: 2.10
 **/
GtkPrintSettings *
gtk_print_operation_get_print_settings (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), NULL);

  return op->priv->print_settings;
}

/**
 * gtk_print_operation_set_job_name:
 * @op: a #GtkPrintOperation
 * @job_name: a string that identifies the print job
 * 
 * Sets the name of the print job. The name is used to identify 
 * the job (e.g. in monitoring applications like eggcups). 
 * 
 * If you don't set a job name, GTK+ picks a default one by 
 * numbering successive print jobs.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_job_name (GtkPrintOperation *op,
				  const gchar       *job_name)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (g_utf8_validate (job_name, -1, NULL));

  g_free (op->priv->job_name);
  op->priv->job_name = g_strdup (job_name);

  g_object_notify (G_OBJECT (op), "job-name");
}

/**
 * gtk_print_operation_set_nr_of_pages:
 * @op: a #GtkPrintOperation
 * @n_pages: the number of pages
 * 
 * Sets the number of pages in the document. 
 *
 * This <emphasis>must</emphasis> be set to a positive number
 * before the rendering starts. It may be set in a ::begin-print 
 * signal hander.
 *
 * Note that the page numbers passed to the ::request-page-setup 
 * and ::draw-page signals are 0-based, i.e. if the user chooses
 * to print all pages, the last ::draw-page signal will be
 * for page @n_pages - 1.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_nr_of_pages (GtkPrintOperation *op,
				     gint               n_pages)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (n_pages > 0);
  g_return_if_fail (op->priv->current_page == -1 || 
		    op->priv->current_page < n_pages);

  if (op->priv->nr_of_pages != n_pages)
    {
      op->priv->nr_of_pages = n_pages;

      g_object_notify (G_OBJECT (op), "number-of-pages");
    }
}

/**
 * gtk_print_operation_set_current_page:
 * @op: a #GtkPrintOperation
 * @current_page: the current page, 0-based
 *
 * Sets the current page.
 *
 * If this is called before gtk_print_operation_run(), 
 * the user will be able to select to print only the current page.
 *
 * Note that this only makes sense for pre-paginated documents.
 * 
 * Since: 2.10
 **/
void
gtk_print_operation_set_current_page (GtkPrintOperation *op,
				      gint               current_page)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (current_page >= 0);
  g_return_if_fail (op->priv->nr_of_pages == -1 || 
		    current_page < op->priv->nr_of_pages);
 
  if (op->priv->current_page != current_page)
    {
      op->priv->current_page = current_page;

      g_object_notify (G_OBJECT (op), "current-page");
    }
}

/**
 * gtk_print_operation_set_use_full_page:
 * @op: a #GtkPrintOperation
 * @full_page: %TRUE to set up the #GtkPrintContext for the full page
 * 
 * If @full_page is %TRUE, the transformation for the cairo context 
 * obtained from #GtkPrintContext puts the origin at the top left 
 * corner of the page (which may not be the top left corner of the 
 * sheet, depending on page orientation and the number of pages per 
 * sheet). Otherwise, the origin is at the top left corner of the
 * imageable area (i.e. inside the margins).
 * 
 * Since: 2.10 
 */
void
gtk_print_operation_set_use_full_page (GtkPrintOperation *op,
				       gboolean           full_page)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  full_page = full_page != FALSE;
  
  if (op->priv->use_full_page != full_page)
    {
      op->priv->use_full_page = full_page;
   
      g_object_notify (G_OBJECT (op), "use-full-page");
    }
}

/**
 * gtk_print_operation_set_unit:
 * @op: a #GtkPrintOperation
 * @unit: the unit to use
 * 
 * Sets up the transformation for the cairo context obtained from
 * #GtkPrintContext in such a way that distances are measured in 
 * units of @unit.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_unit (GtkPrintOperation *op,
			      GtkUnit            unit)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  if (op->priv->unit != unit)
    {
      op->priv->unit = unit;

      g_object_notify (G_OBJECT (op), "unit");
    }
}

void
_gtk_print_operation_set_status (GtkPrintOperation *op,
				 GtkPrintStatus     status,
				 const gchar       *string)
{
  const gchar *status_strs[] = {
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Initial state"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Preparing to print"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Generating data"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Sending data"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Waiting"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Blocking on issue"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Printing"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Finished"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Finished with error")
  };

  if (status < 0 || status > GTK_PRINT_STATUS_FINISHED_ABORTED)
    status = GTK_PRINT_STATUS_FINISHED_ABORTED;

  if (string == NULL)
    string = g_strip_context (status_strs[status],
			      gettext (status_strs[status]));
  
  if (op->priv->status == status &&
      strcmp (string, op->priv->status_string) == 0)
    return;
  
  g_free (op->priv->status_string);
  op->priv->status_string = g_strdup (string);
  op->priv->status = status;

  g_object_notify (G_OBJECT (op), "status");
  g_object_notify (G_OBJECT (op), "status-string");

  g_signal_emit (op, signals[STATUS_CHANGED], 0);
}


/**
 * gtk_print_operation_get_status:
 * @op: a #GtkPrintOperation
 * 
 * Returns the status of the print operation. 
 * Also see gtk_print_operation_get_status_string().
 * 
 * Return value: the status of the print operation
 *
 * Since: 2.10
 **/
GtkPrintStatus
gtk_print_operation_get_status (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), 
                        GTK_PRINT_STATUS_FINISHED_ABORTED);

  return op->priv->status;
}

/**
 * gtk_print_operation_get_status_string:
 * @op: a #GtkPrintOperation
 * 
 * Returns a string representation of the status of the 
 * print operation. The string is translated and suitable
 * for displaying the print status e.g. in a #GtkStatusbar.
 *
 * Use gtk_print_operation_get_status() to obtain a status
 * value that is suitable for programmatic use. 
 * 
 * Return value: a string representation of the status
 *    of the print operation
 *
 * Since: 2.10
 **/
G_CONST_RETURN gchar *
gtk_print_operation_get_status_string (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), "");

  return op->priv->status_string;
}

/**
 * gtk_print_operation_is_finished:
 * @op: a #GtkPrintOperation
 * 
 * A convenience function to find out if the print operation
 * is finished, either successfully (%GTK_PRINT_STATUS_FINISHED)
 * or unsuccessfully (%GTK_PRINT_STATUS_FINISHED_ABORTED).
 * 
 * Return value: %TRUE, if the print operation is finished.
 *
 * Since: 2.10
 **/
gboolean
gtk_print_operation_is_finished (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), TRUE);

  return
    op->priv->status == GTK_PRINT_STATUS_FINISHED_ABORTED ||
    op->priv->status == GTK_PRINT_STATUS_FINISHED;
}


/**
 * gtk_print_operation_set_show_dialog:
 * @op: a #GtkPrintOperation
 * @show_dialog: %TRUE to show the print dialog
 * 
 * Sets whether calling gtk_print_operation_run() will present
 * a print dialog to the user, or just print to the default printer.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_show_dialog (GtkPrintOperation *op,
				     gboolean           show_dialog)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  
  show_dialog = show_dialog != FALSE;

  if (op->priv->show_dialog != show_dialog)
    {
      op->priv->show_dialog = show_dialog;

      g_object_notify (G_OBJECT (op), "show-dialog");
    }
}

/**
 * gtk_print_operation_set_pdf_target:
 * @op: a #GtkPrintOperation
 * @filename: the filename for the PDF file
 * 
 * Sets up the #GtkPrintOperation to generate a PDF file instead
 * of showing the print dialog. The indended use of this function
 * is for implementing "Export to PDF" actions.
 *
 * "Print to PDF" support is independent of this and is done
 * by letting the user pick the "Print to PDF" item from the list
 * of printers in the print dialog.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_pdf_target (GtkPrintOperation *op,
				    const gchar       *filename)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  g_free (op->priv->pdf_target);
  op->priv->pdf_target = g_strdup (filename);

  g_object_notify (G_OBJECT (op), "pdf-target");
}

/* Creates the initial page setup used for printing unless the
 * app overrides this on a per-page basis using request_page_setup.
 *
 * Data is taken from, in order, if existing:
 *
 * PrintSettings returned from the print dialog
 *  (initial dialog values are set from default_page_setup
 *   if unset in app specified print_settings)
 * default_page_setup
 * per-locale default setup
 */
static GtkPageSetup *
create_page_setup (GtkPrintOperation *op)
{
  GtkPageSetup *page_setup;
  GtkPrintSettings *settings;
  
  if (op->priv->default_page_setup)
    page_setup = gtk_page_setup_copy (op->priv->default_page_setup);
  else
    page_setup = gtk_page_setup_new ();

  settings = op->priv->print_settings;
  if (settings)
    {
      GtkPaperSize *paper_size;
      
      if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_ORIENTATION))
	gtk_page_setup_set_orientation (page_setup,
					gtk_print_settings_get_orientation (settings));


      paper_size = gtk_print_settings_get_paper_size (settings);
      if (paper_size)
	{
	  gtk_page_setup_set_paper_size (page_setup, paper_size);
	  gtk_paper_size_free (paper_size);
	}

      /* TODO: Margins? */
    }
  
  return page_setup;
}

static void 
pdf_start_page (GtkPrintOperation *op,
		GtkPrintContext   *print_context,
		GtkPageSetup      *page_setup)
{
  /* TODO: Set up page size, not supported in cairo yet */
}

static void
pdf_end_page (GtkPrintOperation *op,
	      GtkPrintContext   *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo (print_context);
  cairo_show_page (cr);
}

static void
pdf_end_run (GtkPrintOperation *op)
{
  cairo_surface_destroy (op->priv->surface);
  op->priv->surface = NULL;
}

static GtkPrintOperationResult
run_pdf (GtkPrintOperation  *op,
	 GtkWindow          *parent,
	 gboolean           *do_print,
	 GError            **error)
{
  GtkPageSetup *page_setup;
  double width, height;
  /* This will be overwritten later by the non-default size, but
     we need to pass some size: */
  
  page_setup = create_page_setup (op);
  width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
  height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
  g_object_unref (page_setup);
  
  op->priv->surface = cairo_pdf_surface_create (op->priv->pdf_target,
						width, height);
  /* TODO: DPI from settings object? */
  cairo_pdf_surface_set_dpi (op->priv->surface, 300, 300);
  
  op->priv->dpi_x = 72;
  op->priv->dpi_y = 72;

  op->priv->manual_num_copies = 1;
  op->priv->manual_collation = FALSE;
  
  *do_print = TRUE;
  
  op->priv->start_page = pdf_start_page;
  op->priv->end_page = pdf_end_page;
  op->priv->end_run = pdf_end_run;
  
  return GTK_PRINT_OPERATION_RESULT_APPLY; 
}

static void
print_pages (GtkPrintOperation *op)
{
  int page, range;
  GtkPageSetup *initial_page_setup, *page_setup;
  GtkPrintContext *print_context;
  cairo_t *cr;
  int uncollated_copies, collated_copies;
  int i, j;
  GtkPageRange *ranges;
  GtkPageRange one_range;
  int num_ranges;

  if (op->priv->manual_collation)
    {
      uncollated_copies = op->priv->manual_num_copies;
      collated_copies = 1;
    }
  else
    {
      uncollated_copies = 1;
      collated_copies = op->priv->manual_num_copies;
    }

  print_context = _gtk_print_context_new (op);

  initial_page_setup = create_page_setup (op);
  _gtk_print_context_set_page_setup (print_context, initial_page_setup);

  _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_PREPARING, NULL);
  g_signal_emit (op, signals[BEGIN_PRINT], 0, print_context);
  
  g_return_if_fail (op->priv->nr_of_pages > 0);

  if (op->priv->print_pages == GTK_PRINT_PAGES_RANGES)
    {
      ranges = op->priv->page_ranges;
      num_ranges = op->priv->num_page_ranges;
    }
  else if (op->priv->print_pages == GTK_PRINT_PAGES_CURRENT &&
	   op->priv->current_page != -1)
    {
      ranges = &one_range;
      num_ranges = 1;
      ranges[0].start = op->priv->current_page;
      ranges[0].end = op->priv->current_page;
    }
  else
    {
      ranges = &one_range;
      num_ranges = 1;
      ranges[0].start = 0;
      ranges[0].end = op->priv->nr_of_pages - 1;
    }
  
  _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_GENERATING_DATA, NULL);

  for (i = 0; i < uncollated_copies; i++)
    {
      for (range = 0; range < num_ranges; range ++)
	{
	  int start, end, inc;
	  int low = ranges[range].start;
	  int high = ranges[range].end;
	  
	  if (op->priv->manual_reverse)
	    {
	      start = high;
	      end = low - 1;
	      inc = -1;
	    }
	  else
	    {
	      start = low;
	      end = high + 1;
	      inc = 1;
	    }
	  for (page = start; page != end; page += inc)
	    {
	      if ((op->priv->manual_page_set == GTK_PAGE_SET_EVEN && page % 2 == 0) ||
		  (op->priv->manual_page_set == GTK_PAGE_SET_ODD && page % 2 == 1))
		continue;
	      
	      for (j = 0; j < collated_copies; j++)
		{
		  page_setup = gtk_page_setup_copy (initial_page_setup);
		  g_signal_emit (op, signals[REQUEST_PAGE_SETUP], 0, print_context, page, page_setup);
		  
		  _gtk_print_context_set_page_setup (print_context, page_setup);
		  op->priv->start_page (op, print_context, page_setup);
		  
		  cr = gtk_print_context_get_cairo (print_context);

		  cairo_save (cr);
		  if (op->priv->manual_scale != 100.0)
		    cairo_scale (cr,
				 op->priv->manual_scale,
				 op->priv->manual_scale);
		  
		  if (op->priv->manual_orientation)
		    _gtk_print_context_rotate_according_to_orientation (print_context);

		  if (!op->priv->use_full_page)
		    _gtk_print_context_translate_into_margin (print_context);
	  
		  g_signal_emit (op, signals[DRAW_PAGE], 0, 
				 print_context, page);
		  
		  op->priv->end_page (op, print_context);
		  
		  cairo_restore (cr);
		  
		  g_object_unref (page_setup);

		  /* Iterate the mainloop so that we redraw windows */
		  while (gtk_events_pending ())
		    gtk_main_iteration ();
		}
	    }
	}
    }
  
  g_signal_emit (op, signals[END_PRINT], 0, print_context);

  g_object_unref (print_context);
  g_object_unref (initial_page_setup);

  cairo_surface_finish (op->priv->surface);
  op->priv->end_run (op);

}

/**
 * gtk_print_operation_run:
 * @op: a #GtkPrintOperation
 * @parent: Transient parent of the dialog, or %NULL
 * @error: Return location for errors, or %NULL
 * 
 * Runs the print operation, by first letting the user modify
 * print settings in the print dialog, and then print the
 * document.
 *
 * Note that this function does not return until the rendering of all 
 * pages is complete. You can connect to the ::status-changed signal on
 * @op to obtain some information about the progress of the print operation. 
 * Furthermore, it may use a recursive mainloop to show the print dialog.
 * See gtk_print_operation_run_async() if this is a problem.
 * 
 * <informalexample><programlisting>
 *  FIXME: need an example here
 * </programlisting></informalexample>
 *
 * Return value: the result of the print operation. A return value of 
 *   %GTK_PRINT_OPERATION_RESULT_APPLY indicates that the printing was 
 *   completed successfully. In this case, it is a good idea to obtain 
 *   the used print settings with gtk_print_operation_get_print_settings() 
 *   and store them for reuse with the next print operation.
 *
 * Since: 2.10
 **/
GtkPrintOperationResult
gtk_print_operation_run (GtkPrintOperation  *op,
			 GtkWindow          *parent,
			 GError            **error)
{
  GtkPrintOperationResult result;
  gboolean do_print;
  
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), 
                        GTK_PRINT_OPERATION_RESULT_ERROR);

  if (op->priv->pdf_target != NULL)
    result = run_pdf (op, parent, &do_print, error);
  else
    result = _gtk_print_operation_platform_backend_run_dialog (op, 
							       parent,
							       &do_print,
							       error);
  if (do_print)
    print_pages (op);
  else 
    _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);

  return result;
}

/**
 * gtk_print_operation_run_async:
 * @op: a #GtkPrintOperation
 * @parent: Transient parent of the dialog, or %NULL
 * 
 * Runs the print operation, by first letting the user modify
 * print settings in the print dialog, and then print the
 * document.
 *
 * In contrast to gtk_print_operation_run(), this function returns after 
 * showing the print dialog on platforms that support this, and handles 
 * the printing by connecting a signal handler to the ::response signal 
 * of the dialog. 
 * 
 * If you use this function, it is recommended that you store the modified
 * #GtkPrintSettings in a ::begin-print or ::end-print signal handler.
 * 
 * Since: 2.10
 **/
void
gtk_print_operation_run_async (GtkPrintOperation *op,
			       GtkWindow         *parent)
{
  gboolean do_print;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op)); 

  if (op->priv->pdf_target != NULL)
    {
      run_pdf (op, parent, &do_print, NULL);
      if (do_print)
	print_pages (op);
      else 
	_gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);
    }
  else
    _gtk_print_operation_platform_backend_run_dialog_async (op, 
							    parent,
							    print_pages);
}


#define __GTK_PRINT_OPERATION_C__
#include "gtkaliasdef.c"
