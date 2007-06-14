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

#include <errno.h>
#include <stdlib.h>       

#include <string.h>
#include "gtkprintoperation-private.h"
#include "gtkmarshalers.h"
#include <cairo-pdf.h>
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmessagedialog.h"
#include "gtkalias.h"

#define SHOW_PROGRESS_TIME 1200

#define GTK_PRINT_OPERATION_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_PRINT_OPERATION, GtkPrintOperationPrivate))

enum 
{
  DONE,
  BEGIN_PRINT,
  PAGINATE,
  REQUEST_PAGE_SETUP,
  DRAW_PAGE,
  END_PRINT,
  STATUS_CHANGED,
  CREATE_CUSTOM_WIDGET,
  CUSTOM_WIDGET_APPLY,
  PREVIEW,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_DEFAULT_PAGE_SETUP,
  PROP_PRINT_SETTINGS,
  PROP_JOB_NAME,
  PROP_N_PAGES,
  PROP_CURRENT_PAGE,
  PROP_USE_FULL_PAGE,
  PROP_TRACK_PRINT_STATUS,
  PROP_UNIT,
  PROP_SHOW_PROGRESS,
  PROP_ALLOW_ASYNC,
  PROP_EXPORT_FILENAME,
  PROP_STATUS,
  PROP_STATUS_STRING,
  PROP_CUSTOM_TAB_LABEL
};

static guint signals[LAST_SIGNAL] = { 0 };
static int job_nr = 0;

static void          preview_iface_init (GtkPrintOperationPreviewIface *iface);
static GtkPageSetup *create_page_setup  (GtkPrintOperation             *op);
static void          common_render_page (GtkPrintOperation             *op,
					 gint                           page_nr);


G_DEFINE_TYPE_WITH_CODE (GtkPrintOperation, gtk_print_operation, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_PRINT_OPERATION_PREVIEW,
						preview_iface_init))

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
  GtkPrintOperationPrivate *priv = print_operation->priv;

  if (priv->free_platform_data &&
      priv->platform_data)
    {
      priv->free_platform_data (priv->platform_data);
      priv->free_platform_data = NULL;
    }

  if (priv->default_page_setup)
    g_object_unref (priv->default_page_setup);
  
  if (priv->print_settings)
    g_object_unref (priv->print_settings);
  
  if (priv->print_context)
    g_object_unref (priv->print_context);

  g_free (priv->export_filename);
  g_free (priv->job_name);
  g_free (priv->custom_tab_label);
  g_free (priv->status_string);

  if (priv->print_pages_idle_id > 0)
    g_source_remove (priv->print_pages_idle_id);

  if (priv->show_progress_timeout_id > 0)
    g_source_remove (priv->show_progress_timeout_id);

  if (priv->error)
    g_error_free (priv->error);
  
  G_OBJECT_CLASS (gtk_print_operation_parent_class)->finalize (object);
}

static void
gtk_print_operation_init (GtkPrintOperation *operation)
{
  GtkPrintOperationPrivate *priv;
  const char *appname;

  priv = operation->priv = GTK_PRINT_OPERATION_GET_PRIVATE (operation);

  priv->status = GTK_PRINT_STATUS_INITIAL;
  priv->status_string = g_strdup ("");
  priv->default_page_setup = NULL;
  priv->print_settings = NULL;
  priv->nr_of_pages = -1;
  priv->current_page = -1;
  priv->use_full_page = FALSE;
  priv->show_progress = FALSE;
  priv->export_filename = NULL;
  priv->track_print_status = FALSE;
  priv->is_sync = FALSE;

  priv->rloop = NULL;
  priv->unit = GTK_UNIT_PIXEL;

  appname = g_get_application_name ();
  /* translators: this string is the default job title for print
   * jobs. %s gets replaced by the application name, %d gets replaced
   * by the job number.
   */
  priv->job_name = g_strdup_printf (_("%s job #%d"), appname, ++job_nr);
}

static void
preview_iface_render_page (GtkPrintOperationPreview *preview,
			   gint                      page_nr)
{

  GtkPrintOperation *op;

  op = GTK_PRINT_OPERATION (preview);
  common_render_page (op, page_nr);
}

static void
preview_iface_end_preview (GtkPrintOperationPreview *preview)
{
  GtkPrintOperation *op;
  
  op = GTK_PRINT_OPERATION (preview);

  g_signal_emit (op, signals[END_PRINT], 0, op->priv->print_context);

  if (op->priv->rloop)
    g_main_loop_quit (op->priv->rloop);
  
  if (op->priv->end_run)
    op->priv->end_run (op, op->priv->is_sync, TRUE);
  
  _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED, NULL);

  g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_APPLY);
}

static gboolean
preview_iface_is_selected (GtkPrintOperationPreview *preview,
			   gint                      page_nr)
{
  GtkPrintOperation *op;
  GtkPrintOperationPrivate *priv;
  int i;
  
  op = GTK_PRINT_OPERATION (preview);
  priv = op->priv;
  
  switch (priv->print_pages)
    {
    case GTK_PRINT_PAGES_ALL:
      return (page_nr >= 0) && (page_nr < priv->nr_of_pages);
    case GTK_PRINT_PAGES_CURRENT:
      return page_nr == priv->current_page;
    case GTK_PRINT_PAGES_RANGES:
      for (i = 0; i < priv->num_page_ranges; i++)
	{
	  if (page_nr >= priv->page_ranges[i].start &&
	      page_nr <= priv->page_ranges[i].end)
	    return TRUE;
	}
      return FALSE;
    }
  return FALSE;
}

static void
preview_iface_init (GtkPrintOperationPreviewIface *iface)
{
  iface->render_page = preview_iface_render_page;
  iface->end_preview = preview_iface_end_preview;
  iface->is_selected = preview_iface_is_selected;
}

static void
preview_start_page (GtkPrintOperation *op,
		    GtkPrintContext   *print_context,
		    GtkPageSetup      *page_setup)
{
  g_signal_emit_by_name (op, "got-page-size", print_context, page_setup);
}

static void
preview_end_page (GtkPrintOperation *op,
		  GtkPrintContext   *print_context)
{
}

static void
preview_end_run (GtkPrintOperation *op,
		 gboolean           wait,
		 gboolean           cancelled)
{
  g_free (op->priv->page_ranges);
  op->priv->page_ranges = NULL;
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
    case PROP_N_PAGES:
      gtk_print_operation_set_n_pages (op, g_value_get_int (value));
      break;
    case PROP_CURRENT_PAGE:
      gtk_print_operation_set_current_page (op, g_value_get_int (value));
      break;
    case PROP_USE_FULL_PAGE:
      gtk_print_operation_set_use_full_page (op, g_value_get_boolean (value));
      break;
    case PROP_TRACK_PRINT_STATUS:
      gtk_print_operation_set_track_print_status (op, g_value_get_boolean (value));
      break;
    case PROP_UNIT:
      gtk_print_operation_set_unit (op, g_value_get_enum (value));
      break;
    case PROP_ALLOW_ASYNC:
      gtk_print_operation_set_allow_async (op, g_value_get_boolean (value));
      break;
    case PROP_SHOW_PROGRESS:
      gtk_print_operation_set_show_progress (op, g_value_get_boolean (value));
      break;
    case PROP_EXPORT_FILENAME:
      gtk_print_operation_set_export_filename (op, g_value_get_string (value));
      break;
    case PROP_CUSTOM_TAB_LABEL:
      gtk_print_operation_set_custom_tab_label (op, g_value_get_string (value));
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
  GtkPrintOperationPrivate *priv = op->priv;

  switch (prop_id)
    {
    case PROP_DEFAULT_PAGE_SETUP:
      g_value_set_object (value, priv->default_page_setup);
      break;
    case PROP_PRINT_SETTINGS:
      g_value_set_object (value, priv->print_settings);
      break;
    case PROP_JOB_NAME:
      g_value_set_string (value, priv->job_name);
      break;
    case PROP_N_PAGES:
      g_value_set_int (value, priv->nr_of_pages);
      break;
    case PROP_CURRENT_PAGE:
      g_value_set_int (value, priv->current_page);
      break;      
    case PROP_USE_FULL_PAGE:
      g_value_set_boolean (value, priv->use_full_page);
      break;
    case PROP_TRACK_PRINT_STATUS:
      g_value_set_boolean (value, priv->track_print_status);
      break;
    case PROP_UNIT:
      g_value_set_enum (value, priv->unit);
      break;
    case PROP_ALLOW_ASYNC:
      g_value_set_boolean (value, priv->allow_async);
      break;
    case PROP_SHOW_PROGRESS:
      g_value_set_boolean (value, priv->show_progress);
      break;
    case PROP_EXPORT_FILENAME:
      g_value_set_string (value, priv->export_filename);
      break;
    case PROP_STATUS:
      g_value_set_enum (value, priv->status);
      break;
    case PROP_STATUS_STRING:
      g_value_set_string (value, priv->status_string);
      break;
    case PROP_CUSTOM_TAB_LABEL:
      g_value_set_string (value, priv->custom_tab_label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

typedef struct
{
  GtkPrintOperationPreview *preview;
  GtkPrintContext *print_context;
  GtkWindow *parent;
  cairo_surface_t *surface;
  gchar *filename;
  guint page_nr;
  gboolean wait;
} PreviewOp;

static void
preview_print_idle_done (gpointer data)
{
  GtkPrintOperation *op;
  PreviewOp *pop = (PreviewOp *) data;

  GDK_THREADS_ENTER ();
  
  op = GTK_PRINT_OPERATION (pop->preview);

  cairo_surface_finish (pop->surface);
  /* Surface is destroyed in launch_preview */
  _gtk_print_operation_platform_backend_launch_preview (op,
							pop->surface,
							pop->parent,
							pop->filename);

  g_free (pop->filename);

  gtk_print_operation_preview_end_preview (pop->preview);

  g_object_unref (op);
  g_free (pop);
  
  GDK_THREADS_LEAVE ();
}

static gboolean
preview_print_idle (gpointer data)
{
  PreviewOp *pop;
  GtkPrintOperation *op;
  gboolean retval = TRUE;
  cairo_t *cr;

  GDK_THREADS_ENTER ();

  pop = (PreviewOp *) data;
  op = GTK_PRINT_OPERATION (pop->preview);

  gtk_print_operation_preview_render_page (pop->preview, pop->page_nr);
  
  cr = gtk_print_context_get_cairo_context (pop->print_context);
  _gtk_print_operation_platform_backend_preview_end_page (op, pop->surface, cr);
  
  /* TODO: print out sheets not pages and follow ranges */
  pop->page_nr++;
  if (op->priv->nr_of_pages <= pop->page_nr)
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static void
preview_got_page_size (GtkPrintOperationPreview *preview, 
		       GtkPrintContext          *context,
		       GtkPageSetup             *page_setup,
		       PreviewOp                *pop)
{
  GtkPrintOperation *op = GTK_PRINT_OPERATION (preview);
  cairo_t *cr;

  _gtk_print_operation_platform_backend_resize_preview_surface (op, page_setup, pop->surface);

  cr = gtk_print_context_get_cairo_context (pop->print_context);
  _gtk_print_operation_platform_backend_preview_start_page (op, pop->surface, cr);

}

static void
preview_ready (GtkPrintOperationPreview *preview,
               GtkPrintContext          *context,
	       PreviewOp                *pop)
{
  pop->page_nr = 0;
  pop->print_context = context;

  g_object_ref (preview);
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE + 10,
	           preview_print_idle,
		   pop,
		   preview_print_idle_done);
}


static gboolean
gtk_print_operation_preview_handler (GtkPrintOperation        *op,
                                     GtkPrintOperationPreview *preview, 
				     GtkPrintContext          *context,
				     GtkWindow                *parent)
{
  gdouble dpi_x, dpi_y;
  PreviewOp *pop;
  GtkPageSetup *page_setup;
  cairo_t *cr;

  pop = g_new0 (PreviewOp, 1);
  pop->filename = NULL;
  pop->preview = preview;
  pop->parent = parent;

  page_setup = gtk_print_context_get_page_setup (context);

  pop->surface =
    _gtk_print_operation_platform_backend_create_preview_surface (op,
								  page_setup,
								  &dpi_x, &dpi_y,
								  &pop->filename);

  cr = cairo_create (pop->surface);
  gtk_print_context_set_cairo_context (op->priv->print_context, cr,
				       dpi_x, dpi_y);
  cairo_destroy (cr);

  g_signal_connect (preview, "ready", (GCallback) preview_ready, pop);
  g_signal_connect (preview, "got-page-size", (GCallback) preview_got_page_size, pop);
  
  return TRUE;
}

static GtkWidget *
gtk_print_operation_create_custom_widget (GtkPrintOperation *operation)
{
  return NULL;
}

static void
gtk_print_operation_done (GtkPrintOperation *operation)
{
  GtkPrintOperationPrivate *priv = operation->priv;

  if (priv->print_context)
    {
      g_object_unref (priv->print_context);
      priv->print_context = NULL;
    } 
}

static gboolean
custom_widget_accumulator (GSignalInvocationHint *ihint,
			   GValue                *return_accu,
			   const GValue          *handler_return,
			   gpointer               dummy)
{
  gboolean continue_emission;
  GtkWidget *widget;
  
  widget = g_value_get_object (handler_return);
  if (widget != NULL)
    g_value_set_object (return_accu, widget);
  continue_emission = (widget == NULL);
  
  return continue_emission;
}

static void
gtk_print_operation_class_init (GtkPrintOperationClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->set_property = gtk_print_operation_set_property;
  gobject_class->get_property = gtk_print_operation_get_property;
  gobject_class->finalize = gtk_print_operation_finalize;
 
  class->preview = gtk_print_operation_preview_handler; 
  class->create_custom_widget = gtk_print_operation_create_custom_widget;
  class->done = gtk_print_operation_done;
  
  g_type_class_add_private (gobject_class, sizeof (GtkPrintOperationPrivate));

  /**
   * GtkPrintOperation::done:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @result: the result of the print operation
   *
   * Emitted when the print operation run has finished doing
   * everything required for printing. @result gives you information
   * about what happened during the run. If @result is
   * %GTK_PRINT_OPERATION_RESULT_ERROR then you can call
   * gtk_print_operation_get_error() for more information.
   *
   * If you enabled print status tracking then 
   * gtk_print_operation_is_finished() may still return %FALSE 
   * after this was emitted.
   *
   * Since: 2.10
   */
  signals[DONE] =
    g_signal_new (I_("done"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, done),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINT_OPERATION_RESULT);

  /**
   * GtkPrintOperation::begin-print:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Emitted after the user has finished changing print settings
   * in the dialog, before the actual rendering starts. 
   *
   * A typical use for this signal is to use the parameters from the
   * #GtkPrintContext and paginate the document accordingly, and then
   * set the number of pages with gtk_print_operation_set_n_pages().
   *
   * Since: 2.10
   */
  signals[BEGIN_PRINT] =
    g_signal_new (I_("begin-print"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, begin_print),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINT_CONTEXT);

   /**
   * GtkPrintOperation::paginate:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Emitted after the begin-print signal, but before the actual 
   * rendering starts. It keeps getting emitted until it returns %FALSE. 
   *
   * This signal is intended to be used for paginating the document
   * in small chunks, to avoid blocking the user interface for a long
   * time. The signal handler should update the number of pages using
   * gtk_print_operation_set_n_pages(), and return %TRUE if the document
   * has been completely paginated.
   *
   * If you don't need to do pagination in chunks, you can simply do
   * it all in the begin-print handler, and set the number of pages
   * from there.
   *
   * Return value: %TRUE if pagination is complete
   *
   * Since: 2.10
   */
  signals[PAGINATE] =
    g_signal_new (I_("paginate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, paginate),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT,
		  G_TYPE_BOOLEAN, 1, GTK_TYPE_PRINT_CONTEXT);


  /**
   * GtkPrintOperation::request-page-setup:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   * @page_nr: the number of the currently printed page
   * @setup: the #GtkPageSetup 
   * 
   * Emitted once for every page that is printed, to give
   * the application a chance to modify the page setup. Any changes 
   * done to @setup will be in force only for printing this page.
   *
   * Since: 2.10
   */
  signals[REQUEST_PAGE_SETUP] =
    g_signal_new (I_("request-page-setup"),
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
   * Emitted for every page that is printed. The signal handler
   * must render the @page_nr's page onto the cairo context obtained
   * from @context using gtk_print_context_get_cairo_context().
   *
   * <informalexample><programlisting>
   * static void
   * draw_page (GtkPrintOperation *operation,
   *            GtkPrintContext   *context,
   *            gint               page_nr,
   *            gpointer           user_data)
   * {
   *   cairo_t *cr;
   *   PangoLayout *layout;
   *   gdouble width, text_height;
   *   gint layout_height;
   *   PangoFontDescription *desc;
   *   
   *   cr = gtk_print_context_get_cairo_context (context);
   *   width = gtk_print_context_get_width (context);
   *   
   *   cairo_rectangle (cr, 0, 0, width, HEADER_HEIGHT);
   *   
   *   cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
   *   cairo_fill (cr);
   *   
   *   layout = gtk_print_context_create_pango_layout (context);
   *   
   *   desc = pango_font_description_from_string ("sans 14");
   *   pango_layout_set_font_description (layout, desc);
   *   pango_font_description_free (desc);
   *   
   *   pango_layout_set_text (layout, "some text", -1);
   *   pango_layout_set_width (layout, width);
   *   pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
   *      		      
   *   pango_layout_get_size (layout, NULL, &amp;layout_height);
   *   text_height = (gdouble)layout_height / PANGO_SCALE;
   *   
   *   cairo_move_to (cr, width / 2,  (HEADER_HEIGHT - text_height) / 2);
   *   pango_cairo_show_layout (cr, layout);
   *   
   *   g_object_unref (layout);
   * }
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
    g_signal_new (I_("draw-page"),
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
   * Emitted after all pages have been rendered. 
   * A handler for this signal can clean up any resources that have
   * been allocated in the ::begin-print handler.
   * 
   * Since: 2.10
   */
  signals[END_PRINT] =
    g_signal_new (I_("end-print"),
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
   * Emitted at between the various phases of the print operation.
   * See #GtkPrintStatus for the phases that are being discriminated.
   * Use gtk_print_operation_get_status() to find out the current
   * status.
   *
   * Since: 2.10
   */
  signals[STATUS_CHANGED] =
    g_signal_new (I_("status-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, status_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);


  /**
   * GtkPrintOperation::create-custom-widget:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   *
   * Emitted when displaying the print dialog. If you return a
   * widget in a handler for this signal it will be added to a custom
   * tab in the print dialog. You typically return a container widget
   * with multiple widgets in it.
   *
   * The print dialog owns the returned widget, and its lifetime
   * isn't controlled by the app. However, the widget is guaranteed
   * to stay around until the custom-widget-apply signal is emitted
   * on the operation. Then you can read out any information you need
   * from the widgets.
   *
   * Returns: A custom widget that gets embedded in the print dialog,
   *          or %NULL
   *
   * Since: 2.10
   */
  signals[CREATE_CUSTOM_WIDGET] =
    g_signal_new (I_("create-custom-widget"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, create_custom_widget),
		  custom_widget_accumulator, NULL,
		  _gtk_marshal_OBJECT__VOID,
		  G_TYPE_OBJECT, 0);

  /**
   * GtkPrintOperation::custom-widget-apply:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @widget: the custom widget added in create-custom-widget
   *
   * Emitted right before begin-print if you added
   * a custom widget in the create-custom-widget handler. When you get
   * this signal you should read the information from the custom widgets,
   * as the widgets are not guaraneed to be around at a later time.
   *
   * Since: 2.10
   */
  signals[CUSTOM_WIDGET_APPLY] =
    g_signal_new (I_("custom-widget-apply"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, custom_widget_apply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

   /**
   * GtkPrintOperation::preview:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @preview: the #GtkPrintPreviewOperation for the current operation
   * @context: the #GtkPrintContext that will be used
   * @parent: the #GtkWindow to use as window parent, or %NULL
   *
   * Gets emitted when a preview is requested from the native dialog.
   *
   * The default handler for this signal uses an external viewer 
   * application to preview.
   *
   * To implement a custom print preview, an application must return
   * %TRUE from its handler for this signal. In order to use the
   * provided @context for the preview implementation, it must be
   * given a suitable cairo context with gtk_print_context_set_cairo_context().
   * 
   * The custom preview implementation can use 
   * gtk_print_operation_preview_is_selected() and 
   * gtk_print_operation_preview_render_page() to find pages which
   * are selected for print and render them. The preview must be
   * finished by calling gtk_print_operation_preview_end_preview()
   * (typically in response to the user clicking a close button).
   *
   * Returns: %TRUE if the listener wants to take over control of the preview
   * 
   * Since: 2.10
   */
  signals[PREVIEW] =
    g_signal_new (I_("preview"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, preview),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_OBJECT_OBJECT,
		  G_TYPE_BOOLEAN, 3,
		  GTK_TYPE_PRINT_OPERATION_PREVIEW,
		  GTK_TYPE_PRINT_CONTEXT,
		  GTK_TYPE_WINDOW);

  
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
   * Setting this property is typically used to re-establish 
   * print settings from a previous print operation, see 
   * gtk_print_operation_run().
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
   * A string used to identify the job (e.g. in monitoring 
   * applications like eggcups). 
   * 
   * If you don't set a job name, GTK+ picks a default one 
   * by numbering successive print jobs.
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
   * GtkPrintOperation:n-pages:
   *
   * The number of pages in the document. 
   *
   * This <emphasis>must</emphasis> be set to a positive number
   * before the rendering starts. It may be set in a ::begin-print 
   * signal hander.
   *
   * Note that the page numbers passed to the ::request-page-setup 
   * and ::draw-page signals are 0-based, i.e. if the user chooses 
   * to print all pages, the last ::draw-page signal will be for 
   * page @n_pages - 1.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_N_PAGES,
				   g_param_spec_int ("n-pages",
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
						     P_("The current page in the document"),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:use-full-page:
   *
   * If %TRUE, the transformation for the cairo context obtained 
   * from #GtkPrintContext puts the origin at the top left corner 
   * of the page (which may not be the top left corner of the sheet, 
   * depending on page orientation and the number of pages per sheet). 
   * Otherwise, the origin is at the top left corner of the imageable 
   * area (i.e. inside the margins).
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_USE_FULL_PAGE,
				   g_param_spec_boolean ("use-full-page",
							 P_("Use full page"),
							 P_("TRUE if the the origin of the context should be at the corner of the page and not the corner of the imageable area"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:track-print-status:
   *
   * If %TRUE, the print operation will try to continue report on 
   * the status of the print job in the printer queues and printer. 
   * This can allow your application to show things like "out of paper" 
   * issues, and when the print job actually reaches the printer. 
   * However, this is often implemented using polling, and should 
   * not be enabled unless needed.
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_TRACK_PRINT_STATUS,
				   g_param_spec_boolean ("track-print-status",
							 P_("Track Print Status"),
							 P_("TRUE if the print operation will continue to report on the print job status after the print data has been sent to the printer or print server."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:unit:
   *
   * The transformation for the cairo context obtained from
   * #GtkPrintContext is set up in such a way that distances 
   * are measured in units of @unit.
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
   * GtkPrintOperation:show-progress:
   *
   * Determines whether to show a progress dialog during the 
   * print operation.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_PROGRESS,
				   g_param_spec_boolean ("show-progress",
							 P_("Show Dialog"),
							 P_("TRUE if a progress dialog is shown while printing."),
							 FALSE,
							 GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:allow-async:
   *
   * Determines whether the print operation may run asynchronously or not.
   *
   * Some systems don't support asynchronous printing, but those that do
   * will return %GTK_PRINT_OPERATION_RESULT_IN_PROGRESS as the status, and
   * emit the done signal when the operation is actually done.
   *
   * The Windows port does not support asynchronous operation
   * at all (this is unlikely to change). On other platforms, all actions
   * except for %GTK_PRINT_OPERATION_ACTION_EXPORT support asynchronous
   * operation.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_ALLOW_ASYNC,
				   g_param_spec_boolean ("allow-async",
							 P_("Allow Async"),
							 P_("TRUE if print process may run asynchronous."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:export-filename:
   *
   * The name of a file file to generate instead of showing 
   * the print dialog. Currently, PDF is the only supported
   * format.
   *
   * The intended use of this property is for implementing 
   * "Export to PDF" actions.
   *
   * "Print to PDF" support is independent of this and is done
   * by letting the user pick the "Print to PDF" item from the 
   * list of printers in the print dialog.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_EXPORT_FILENAME,
				   g_param_spec_string ("export-filename",
							P_("Export filename"),
							P_("Export filename"),
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
  

  /**
   * GtkPrintOperation:custom-tab-label:
   *
   * Used as the label of the tab containing custom widgets.
   * Note that this property may be ignored on some platforms.
   * 
   * If this is %NULL, GTK+ uses a default label.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_CUSTOM_TAB_LABEL,
				   g_param_spec_string ("custom-tab-label",
							P_("Custom tab label"),
							P_("Label for the tab containing custom widgets."),
							NULL,
							GTK_PARAM_READWRITE));

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
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (default_page_setup == NULL || 
                    GTK_IS_PAGE_SETUP (default_page_setup));

  priv = op->priv;

  if (default_page_setup != priv->default_page_setup)
    {
      if (default_page_setup)
	g_object_ref (default_page_setup);
      
      if (priv->default_page_setup)
	g_object_unref (priv->default_page_setup);
      
      priv->default_page_setup = default_page_setup;
     
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
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (print_settings == NULL || 
                    GTK_IS_PRINT_SETTINGS (print_settings));

  priv = op->priv;

  if (print_settings != priv->print_settings)
    {
      if (print_settings)
        g_object_ref (print_settings);

      if (priv->print_settings)
        g_object_unref (priv->print_settings);
  
      priv->print_settings = print_settings;

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
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (job_name != NULL);

  priv = op->priv;

  g_free (priv->job_name);
  priv->job_name = g_strdup (job_name);

  g_object_notify (G_OBJECT (op), "job-name");
}

/**
 * gtk_print_operation_set_n_pages:
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
gtk_print_operation_set_n_pages (GtkPrintOperation *op,
				 gint               n_pages)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (n_pages > 0);

  priv = op->priv;
  g_return_if_fail (priv->current_page == -1 || 
		    priv->current_page < n_pages);

  if (priv->nr_of_pages != n_pages)
    {
      priv->nr_of_pages = n_pages;

      g_object_notify (G_OBJECT (op), "n-pages");
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
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (current_page >= 0);

  priv = op->priv;
  g_return_if_fail (priv->nr_of_pages == -1 || 
		    current_page < priv->nr_of_pages);

  if (priv->current_page != current_page)
    {
      priv->current_page = current_page;

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
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  full_page = full_page != FALSE;
 
  priv = op->priv;
	
  if (priv->use_full_page != full_page)
    {
      priv->use_full_page = full_page;
   
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
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  if (priv->unit != unit)
    {
      priv->unit = unit;

      g_object_notify (G_OBJECT (op), "unit");
    }
}

/**
 * gtk_print_operation_set_track_print_status:
 * @op: a #GtkPrintOperation
 * @track_status: %TRUE to track status after printing
 * 
 * If track_status is %TRUE, the print operation will try to continue report
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
gtk_print_operation_set_track_print_status (GtkPrintOperation  *op,
					    gboolean            track_status)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  if (priv->track_print_status != track_status)
    {
      priv->track_print_status = track_status;

      g_object_notify (G_OBJECT (op), "track-print-status");
    }
}

void
_gtk_print_operation_set_status (GtkPrintOperation *op,
				 GtkPrintStatus     status,
				 const gchar       *string)
{
  GtkPrintOperationPrivate *priv = op->priv;
  static const gchar *status_strs[] = {
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
  
  if (priv->status == status &&
      strcmp (string, priv->status_string) == 0)
    return;
  
  g_free (priv->status_string);
  priv->status_string = g_strdup (string);
  priv->status = status;

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
 * Note: when you enable print status tracking the print operation
 * can be in a non-finished state even after done has been called, as
 * the operation status then tracks the print job status on the printer.
 * 
 * Return value: %TRUE, if the print operation is finished.
 *
 * Since: 2.10
 **/
gboolean
gtk_print_operation_is_finished (GtkPrintOperation *op)
{
  GtkPrintOperationPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), TRUE);

  priv = op->priv;
  return
    priv->status == GTK_PRINT_STATUS_FINISHED_ABORTED ||
    priv->status == GTK_PRINT_STATUS_FINISHED;
}

/**
 * gtk_print_operation_set_show_progress:
 * @op: a #GtkPrintOperation
 * @show_progress: %TRUE to show a progress dialog
 * 
 * If @show_progress is %TRUE, the print operation will show a 
 * progress dialog during the print operation.
 * 
 * Since: 2.10
 */
void
gtk_print_operation_set_show_progress (GtkPrintOperation  *op,
				       gboolean            show_progress)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  show_progress = show_progress != FALSE;

  if (priv->show_progress != show_progress)
    {
      priv->show_progress = show_progress;

      g_object_notify (G_OBJECT (op), "show-progress");
    }
}

/**
 * gtk_print_operation_set_allow_async:
 * @op: a #GtkPrintOperation
 * @allow_async: %TRUE to allow asynchronous operation
 *
 * Sets whether the gtk_print_operation_run() may return
 * before the print operation is completed. Note that
 * some platforms may not allow asynchronous operation.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_allow_async (GtkPrintOperation  *op,
				     gboolean            allow_async)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  allow_async = allow_async != FALSE;

  if (priv->allow_async != allow_async)
    {
      priv->allow_async = allow_async;

      g_object_notify (G_OBJECT (op), "allow-async");
    }
}


/**
 * gtk_print_operation_set_custom_tab_label:
 * @op: a #GtkPrintOperation
 * @label: the label to use, or %NULL to use the default label
 *
 * Sets the label for the tab holding custom widgets.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_custom_tab_label (GtkPrintOperation  *op,
					  const gchar        *label)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  g_free (priv->custom_tab_label);
  priv->custom_tab_label = g_strdup (label);

  g_object_notify (G_OBJECT (op), "custom-tab-label");
}


/**
 * gtk_print_operation_set_export_filename:
 * @op: a #GtkPrintOperation
 * @filename: the filename for the exported file
 * 
 * Sets up the #GtkPrintOperation to generate a file instead
 * of showing the print dialog. The indended use of this function
 * is for implementing "Export to PDF" actions. Currently, PDF
 * is the only supported format.
 *
 * "Print to PDF" support is independent of this and is done
 * by letting the user pick the "Print to PDF" item from the list
 * of printers in the print dialog.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_export_filename (GtkPrintOperation *op,
					 const gchar       *filename)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  g_free (priv->export_filename);
  priv->export_filename = g_strdup (filename);

  g_object_notify (G_OBJECT (op), "export-filename");
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
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  GtkPrintSettings *settings;
  
  if (priv->default_page_setup)
    page_setup = gtk_page_setup_copy (priv->default_page_setup);
  else
    page_setup = gtk_page_setup_new ();

  settings = priv->print_settings;
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
  GtkPaperSize *paper_size;
  cairo_surface_t *surface = op->priv->platform_data;
  gdouble w, h;

  paper_size = gtk_page_setup_get_paper_size (page_setup);

  w = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  h = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
  
  cairo_pdf_surface_set_size (surface, w, h);
}

static void
pdf_end_page (GtkPrintOperation *op,
	      GtkPrintContext   *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo_context (print_context);
  cairo_show_page (cr);
}

static void
pdf_end_run (GtkPrintOperation *op,
	     gboolean           wait,
	     gboolean           cancelled)
{
  GtkPrintOperationPrivate *priv = op->priv;
  cairo_surface_t *surface = priv->platform_data;

  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);

  priv->platform_data = NULL;
  priv->free_platform_data = NULL;
}

static GtkPrintOperationResult
run_pdf (GtkPrintOperation  *op,
	 GtkWindow          *parent,
	 gboolean           *do_print)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  cairo_surface_t *surface;
  cairo_t *cr;
  gdouble width, height;
  
  priv->print_context = _gtk_print_context_new (op);
  
  page_setup = create_page_setup (op);
  _gtk_print_context_set_page_setup (priv->print_context, page_setup);

  /* This will be overwritten later by the non-default size, but
     we need to pass some size: */
  width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
  height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
  g_object_unref (page_setup);
  
  surface = cairo_pdf_surface_create (priv->export_filename,
				      width, height);
  cairo_surface_set_fallback_resolution (surface, 300, 300);

  priv->platform_data = surface;
  priv->free_platform_data = (GDestroyNotify) cairo_surface_destroy;

  cr = cairo_create (surface);
  gtk_print_context_set_cairo_context (op->priv->print_context,
				       cr, 72, 72);
  cairo_destroy (cr);

  
  priv->print_pages = GTK_PRINT_PAGES_ALL;
  priv->page_ranges = NULL;
  priv->num_page_ranges = 0;

  priv->manual_num_copies = 1;
  priv->manual_collation = FALSE;
  priv->manual_reverse = FALSE;
  priv->manual_page_set = GTK_PAGE_SET_ALL;
  priv->manual_scale = 1.0;
  priv->manual_orientation = TRUE;
  
  *do_print = TRUE;
  
  priv->start_page = pdf_start_page;
  priv->end_page = pdf_end_page;
  priv->end_run = pdf_end_run;
  
  return GTK_PRINT_OPERATION_RESULT_APPLY; 
}

typedef struct
{
  GtkPrintOperation *op;
  gint uncollated_copies;
  gint collated_copies;
  gint uncollated, collated, total;

  gint range, num_ranges;
  GtkPageRange *ranges;
  GtkPageRange one_range;

  gint page, start, end, inc;

  GtkWidget *progress;
 
  gboolean initialized;
  gboolean is_preview; 
} PrintPagesData;

static void
find_range (PrintPagesData *data)
{
  GtkPageRange *range;

  range = &data->ranges[data->range];

  if (data->inc < 0)
    {
      data->start = range->end;
      data->end = range->start - 1;
    }
  else
    {
      data->start = range->start;
      data->end = range->end + 1;
    }
}

static gboolean 
increment_page_sequence (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv = data->op->priv;

  do {
    data->page += data->inc;
    if (data->page == data->end)
      {
	data->range += data->inc;
	if (data->range == -1 || data->range == data->num_ranges)
	  {
	    data->uncollated++;
	    if (data->uncollated == data->uncollated_copies)
	      return FALSE;

	    data->range = data->inc < 0 ? data->num_ranges - 1 : 0;
	  }
	find_range (data);
	data->page = data->start;
      }
  }
  while ((priv->manual_page_set == GTK_PAGE_SET_EVEN && data->page % 2 == 0) ||
	 (priv->manual_page_set == GTK_PAGE_SET_ODD && data->page % 2 == 1));

  return TRUE;
}

static void
print_pages_idle_done (gpointer user_data)
{
  PrintPagesData *data;
  GtkPrintOperationPrivate *priv;

  GDK_THREADS_ENTER ();

  data = (PrintPagesData*)user_data;
  priv = data->op->priv;

  priv->print_pages_idle_id = 0;

  if (priv->show_progress_timeout_id > 0)
    {
      g_source_remove (priv->show_progress_timeout_id);
      priv->show_progress_timeout_id = 0;
    }
 
  if (data->progress)
    gtk_widget_destroy (data->progress);

  if (priv->rloop && !data->is_preview) 
    g_main_loop_quit (priv->rloop);

  if (!data->is_preview || priv->cancelled)
    g_signal_emit (data->op, signals[DONE], 0,
		   priv->cancelled ?
		   GTK_PRINT_OPERATION_RESULT_CANCEL :
		   GTK_PRINT_OPERATION_RESULT_APPLY);
  
  g_object_unref (data->op);
  g_free (data);

  GDK_THREADS_LEAVE ();
}

static void
update_progress (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv; 
  gchar *text = NULL;
  
  priv = data->op->priv;
 
  if (data->progress)
    {
      if (priv->status == GTK_PRINT_STATUS_PREPARING)
	{
	  if (priv->nr_of_pages > 0)
	    text = g_strdup_printf (_("Preparing %d"), priv->nr_of_pages);
	  else
	    text = g_strdup (_("Preparing"));
	}
      else if (priv->status == GTK_PRINT_STATUS_GENERATING_DATA)
	text = g_strdup_printf (_("Printing %d"), data->total);
      
      if (text)
	{
	  g_object_set (data->progress, "text", text, NULL);
	  g_free (text);
	}
    }
 }

static void
common_render_page (GtkPrintOperation *op,
		    gint               page_nr)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  GtkPrintContext *print_context;
  cairo_t *cr;

  print_context = priv->print_context;
  
  page_setup = create_page_setup (op);
  
  g_signal_emit (op, signals[REQUEST_PAGE_SETUP], 0, 
		 print_context, page_nr, page_setup);
  
  _gtk_print_context_set_page_setup (print_context, page_setup);
  
  priv->start_page (op, print_context, page_setup);
  
  cr = gtk_print_context_get_cairo_context (print_context);
  
  cairo_save (cr);
  if (priv->manual_scale != 1.0)
    cairo_scale (cr,
		 priv->manual_scale,
		 priv->manual_scale);
  
  if (priv->manual_orientation)
    _gtk_print_context_rotate_according_to_orientation (print_context);
  
  if (!priv->use_full_page)
    _gtk_print_context_translate_into_margin (print_context);
  
  g_signal_emit (op, signals[DRAW_PAGE], 0, 
		 print_context, page_nr);

  priv->end_page (op, print_context);
  
  cairo_restore (cr);

  g_object_unref (page_setup);
}

static gboolean
print_pages_idle (gpointer user_data)
{
  PrintPagesData *data; 
  GtkPrintOperationPrivate *priv; 
  GtkPageSetup *page_setup;
  gboolean done = FALSE;

  GDK_THREADS_ENTER ();

  data = (PrintPagesData*)user_data;
  priv = data->op->priv;

  if (priv->status == GTK_PRINT_STATUS_PREPARING)
    {
      if (!data->initialized)
	{
	  data->initialized = TRUE;
	  page_setup = create_page_setup (data->op);
	  _gtk_print_context_set_page_setup (priv->print_context, 
					     page_setup);
	  g_object_unref (page_setup);
      
	  g_signal_emit (data->op, signals[BEGIN_PRINT], 0, priv->print_context);
      
	  if (priv->manual_collation)
	    {
	      data->uncollated_copies = priv->manual_num_copies;
	      data->collated_copies = 1;
	    }
	  else
	    {
	      data->uncollated_copies = 1;
	      data->collated_copies = priv->manual_num_copies;
	    }

	  goto out;
	}
      
      if (GTK_PRINT_OPERATION_GET_CLASS (data->op)->paginate != NULL ||
          g_signal_has_handler_pending (data->op, signals[PAGINATE], 0, FALSE))
	{
	  gboolean paginated = FALSE;

	  g_signal_emit (data->op, signals[PAGINATE], 0, priv->print_context, &paginated);
	  if (!paginated)
	    goto out;
	}

      /* FIXME handle this better */
      if (priv->nr_of_pages == 0)
	g_warning ("no pages to print");
      
      /* Initialize parts of PrintPagesData that depend on nr_of_pages
       */
      if (priv->print_pages == GTK_PRINT_PAGES_RANGES)
	{
	  data->ranges = priv->page_ranges;
	  data->num_ranges = priv->num_page_ranges;
	}
      else if (priv->print_pages == GTK_PRINT_PAGES_CURRENT &&
	       priv->current_page != -1)
	{
	  data->ranges = &data->one_range;
	  data->num_ranges = 1;
	  data->ranges[0].start = priv->current_page;
	  data->ranges[0].end = priv->current_page;
	}
      else
	{
	  data->ranges = &data->one_range;
	  data->num_ranges = 1;
	  data->ranges[0].start = 0;
	  data->ranges[0].end = priv->nr_of_pages - 1;
	}
      
      if (priv->manual_reverse)
	{
	  data->range = data->num_ranges - 1;
	  data->inc = -1;      
	}
      else
	{
	  data->range = 0;
	  data->inc = 1;      
	}
      find_range (data);
     
      /* go back one page, since we preincrement below */
      data->page = data->start - data->inc;
      data->collated = data->collated_copies - 1;

      _gtk_print_operation_set_status (data->op, 
				       GTK_PRINT_STATUS_GENERATING_DATA, 
				       NULL);

      goto out;
    }

  data->total++;
  data->collated++;
  if (data->collated == data->collated_copies)
    {
      data->collated = 0;
      if (!increment_page_sequence (data))
	{
	  done = TRUE;

	  goto out;
	}
    }
 
  if (data->is_preview && !priv->cancelled)
    {
      done = TRUE;

      g_signal_emit_by_name (data->op, "ready", priv->print_context);
      goto out;
    }

  common_render_page (data->op, data->page);

 out:

  if (priv->cancelled)
    {
      _gtk_print_operation_set_status (data->op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);
      
      done = TRUE;
    }

  if (done && (!data->is_preview || priv->cancelled))
    {
      g_signal_emit (data->op, signals[END_PRINT], 0, priv->print_context);
      priv->end_run (data->op, priv->is_sync, priv->cancelled);
    }

  update_progress (data);

  GDK_THREADS_LEAVE ();

  return !done;
}
  
static void
handle_progress_response (GtkWidget *dialog, 
			  gint       response,
			  gpointer   data)
{
  GtkPrintOperation *op = (GtkPrintOperation *)data;

  gtk_widget_hide (dialog);
  gtk_print_operation_cancel (op);
}

static gboolean
show_progress_timeout (PrintPagesData *data)
{
  GDK_THREADS_ENTER ();

  gtk_window_present (GTK_WINDOW (data->progress));

  data->op->priv->show_progress_timeout_id = 0;

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
print_pages (GtkPrintOperation       *op,
	     GtkWindow               *parent,
	     gboolean                 do_print,
	     GtkPrintOperationResult  result)
{
  GtkPrintOperationPrivate *priv = op->priv;
  PrintPagesData *data;
 
  if (!do_print) 
    {
      _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);
      g_signal_emit (op, signals[DONE], 0, result);
      return;
  }
  
  _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_PREPARING, NULL);  

  data = g_new0 (PrintPagesData, 1);
  data->op = g_object_ref (op);
  data->is_preview = (priv->action == GTK_PRINT_OPERATION_ACTION_PREVIEW);

  if (priv->show_progress)
    {
      GtkWidget *progress;

      progress = gtk_message_dialog_new (parent, 0, 
					 GTK_MESSAGE_OTHER,
					 GTK_BUTTONS_CANCEL,
					 _("Preparing"));
      g_signal_connect (progress, "response", 
			G_CALLBACK (handle_progress_response), op);

      priv->show_progress_timeout_id = 
	g_timeout_add (SHOW_PROGRESS_TIME, 
		       (GSourceFunc)show_progress_timeout,
		       data);

      data->progress = progress;
    }

  if (data->is_preview)
    {
      gboolean handled;
      
      g_signal_emit_by_name (op, "preview",
			     GTK_PRINT_OPERATION_PREVIEW (op),
			     priv->print_context,
			     parent,
			     &handled);
      
      if (!handled ||
	  gtk_print_context_get_cairo_context (priv->print_context) == NULL) 
	{
	  /* Programmer error */
	  g_error ("You must set a cairo context on the print context");
	}
      
      priv->start_page = preview_start_page;
      priv->end_page = preview_end_page;
      priv->end_run = preview_end_run;

      priv->print_pages = gtk_print_settings_get_print_pages (priv->print_settings);
      priv->page_ranges = gtk_print_settings_get_page_ranges (priv->print_settings,
							      &priv->num_page_ranges);
      priv->manual_num_copies = 1;
      priv->manual_collation = FALSE;
      priv->manual_reverse = FALSE;
      priv->manual_page_set = GTK_PAGE_SET_ALL;
      priv->manual_scale = 1.0;
      priv->manual_orientation = TRUE;
    }
  
  priv->print_pages_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE + 10,
					       print_pages_idle, 
					       data, 
					       print_pages_idle_done);
  
  /* Recursive main loop to make sure we don't exit  on sync operations  */
  if (priv->is_sync)
    {
      priv->rloop = g_main_loop_new (NULL, FALSE);

      GDK_THREADS_LEAVE ();
      g_main_loop_run (priv->rloop);
      GDK_THREADS_ENTER ();
      
      g_main_loop_unref (priv->rloop);
      priv->rloop = NULL;
    }
}

/**
 * gtk_print_operation_get_error:
 * @op: a #GtkPrintOperation
 * @error: return location for the error
 * 
 * Call this when the result of a print operation is
 * %GTK_PRINT_OPERATION_RESULT_ERROR, either as returned by 
 * gtk_print_operation_run(), or in the ::done signal handler. 
 * The returned #GError will contain more details on what went wrong.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_get_error (GtkPrintOperation  *op,
			       GError            **error)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  
  g_propagate_error (error, op->priv->error);

  op->priv->error = NULL;
}


/**
 * gtk_print_operation_run:
 * @op: a #GtkPrintOperation
 * @action: the action to start
 * @parent: Transient parent of the dialog, or %NULL
 * @error: Return location for errors, or %NULL
 * 
 * Runs the print operation, by first letting the user modify
 * print settings in the print dialog, and then print the document.
 *
 * Normally that this function does not return until the rendering of all 
 * pages is complete. You can connect to the ::status-changed signal on
 * @op to obtain some information about the progress of the print operation. 
 * Furthermore, it may use a recursive mainloop to show the print dialog.
 *
 * If you call gtk_print_operation_set_allow_async() or set the allow-async
 * property the operation will run asyncronously if this is supported on the
 * platform. The ::done signal will be emitted with the operation results when
 * the operation is done (i.e. when the dialog is canceled, or when the print
 * succeeds or fails).
 *
 * <informalexample><programlisting>
 * if (settings != NULL)
 *   gtk_print_operation_set_print_settings (print, settings);
 *   
 * if (page_setup != NULL)
 *   gtk_print_operation_set_default_page_setup (print, page_setup);
 *   
 * g_signal_connect (print, "begin-print", 
 *                   G_CALLBACK (begin_print), &amp;data);
 * g_signal_connect (print, "draw-page", 
 *                   G_CALLBACK (draw_page), &amp;data);
 *  
 * res = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, parent, &amp;error);
 *  
 * if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
 *  {
 *    error_dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
 *   			                     GTK_DIALOG_DESTROY_WITH_PARENT,
 * 					     GTK_MESSAGE_ERROR,
 * 					     GTK_BUTTONS_CLOSE,
 * 					     "Error printing file:\n%s",
 * 					     error->message);
 *    g_signal_connect (error_dialog, "response", 
 *                      G_CALLBACK (gtk_widget_destroy), NULL);
 *    gtk_widget_show (error_dialog);
 *    g_error_free (error);
 *  }
 * else if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
 *  {
 *    if (settings != NULL)
 *	g_object_unref (settings);
 *    settings = g_object_ref (gtk_print_operation_get_print_settings (print));
 *  }
 * </programlisting></informalexample>
 *
 * Note that gtk_print_operation_run() can only be called once on a
 * given #GtkPrintOperation.
 *
 * Return value: the result of the print operation. A return value of 
 *   %GTK_PRINT_OPERATION_RESULT_APPLY indicates that the printing was
 *   completed successfully. In this case, it is a good idea to obtain 
 *   the used print settings with gtk_print_operation_get_print_settings() 
 *   and store them for reuse with the next print operation. A value of
 *   %GTK_PRINT_OPERATION_RESULT_IN_PROGRESS means the operation is running
 *   asynchronously, and will emit the ::done signal when done.
 *
 * Since: 2.10
 **/
GtkPrintOperationResult
gtk_print_operation_run (GtkPrintOperation        *op,
			 GtkPrintOperationAction   action,
			 GtkWindow                *parent,
			 GError                  **error)
{
  GtkPrintOperationPrivate *priv;
  GtkPrintOperationResult result;
  GtkPageSetup *page_setup;
  gboolean do_print;
  gboolean run_print_pages;
  
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), 
                        GTK_PRINT_OPERATION_RESULT_ERROR);
  g_return_val_if_fail (op->priv->status == GTK_PRINT_STATUS_INITIAL,
                        GTK_PRINT_OPERATION_RESULT_ERROR);
  priv = op->priv;
  
  run_print_pages = TRUE;
  do_print = FALSE;
  priv->error = NULL;
  priv->action = action;

  if (priv->print_settings == NULL)
    priv->print_settings = gtk_print_settings_new ();
  
  if (action == GTK_PRINT_OPERATION_ACTION_EXPORT)
    {
      /* note: if you implement async EXPORT, update the docs
       * docs for the allow-async property.
       */
      priv->is_sync = TRUE;
      g_return_val_if_fail (priv->export_filename != NULL, GTK_PRINT_OPERATION_RESULT_ERROR);
      result = run_pdf (op, parent, &do_print);
    }
  else if (action == GTK_PRINT_OPERATION_ACTION_PREVIEW)
    {
      priv->is_sync = !priv->allow_async;
      priv->print_context = _gtk_print_context_new (op);
      page_setup = create_page_setup (op);
      _gtk_print_context_set_page_setup (priv->print_context, page_setup);
      g_object_unref (page_setup);
      do_print = TRUE;
      result = priv->is_sync ? GTK_PRINT_OPERATION_RESULT_APPLY : GTK_PRINT_OPERATION_RESULT_IN_PROGRESS;
    }
#ifndef G_OS_WIN32
  else if (priv->allow_async)
    {
      priv->is_sync = FALSE;
      _gtk_print_operation_platform_backend_run_dialog_async (op,
							      action == GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
							      parent,
							      print_pages);
      result = GTK_PRINT_OPERATION_RESULT_IN_PROGRESS;
      run_print_pages = FALSE; /* print_pages is called asynchronously from dialog */
    }
#endif
  else
    {
      priv->is_sync = TRUE;
      result = _gtk_print_operation_platform_backend_run_dialog (op, 
								 action == GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
								 parent,
								 &do_print);
    }

  if (run_print_pages)
    print_pages (op, parent, do_print, result);

  if (priv->error && error)
    *error = g_error_copy (priv->error);
  
  return result;
}

/**
 * gtk_print_operation_cancel:
 * @op: a #GtkPrintOperation
 *
 * Cancels a running print operation. This function may
 * be called from a begin-print, paginate or draw-page
 * signal handler to stop the currently running print 
 * operation.
 *
 * Since: 2.10
 */
void
gtk_print_operation_cancel (GtkPrintOperation *op)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  
  op->priv->cancelled = TRUE;
}



#define __GTK_PRINT_OPERATION_C__
#include "gtkaliasdef.c"
