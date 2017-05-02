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
#include <math.h>

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
  UPDATE_CUSTOM_WIDGET,
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
  PROP_CUSTOM_TAB_LABEL,
  PROP_EMBED_PAGE_SETUP,
  PROP_HAS_SELECTION,
  PROP_SUPPORT_SELECTION,
  PROP_N_PAGES_TO_PRINT
};

static guint signals[LAST_SIGNAL] = { 0 };
static int job_nr = 0;
typedef struct _PrintPagesData PrintPagesData;

static void          preview_iface_init      (GtkPrintOperationPreviewIface *iface);
static GtkPageSetup *create_page_setup       (GtkPrintOperation             *op);
static void          common_render_page      (GtkPrintOperation             *op,
					      gint                           page_nr);
static void          increment_page_sequence (PrintPagesData *data);
static void          prepare_data            (PrintPagesData *data);
static void          clamp_page_ranges       (PrintPagesData *data);


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
  priv->nr_of_pages_to_print = -1;
  priv->page_position = -1;
  priv->current_page = -1;
  priv->use_full_page = FALSE;
  priv->show_progress = FALSE;
  priv->export_filename = NULL;
  priv->track_print_status = FALSE;
  priv->is_sync = FALSE;
  priv->support_selection = FALSE;
  priv->has_selection = FALSE;
  priv->embed_page_setup = FALSE;

  priv->page_drawing_state = GTK_PAGE_DRAWING_STATE_READY;

  priv->rloop = NULL;
  priv->unit = GTK_UNIT_PIXEL;

  appname = g_get_application_name ();
  if (appname == NULL)
    appname = "";
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
    case GTK_PRINT_PAGES_SELECTION:
    case GTK_PRINT_PAGES_ALL:
      return (page_nr >= 0) && (page_nr < priv->nr_of_pages);
    case GTK_PRINT_PAGES_CURRENT:
      return page_nr == priv->current_page;
    case GTK_PRINT_PAGES_RANGES:
      for (i = 0; i < priv->num_page_ranges; i++)
	{
	  if (page_nr >= priv->page_ranges[i].start &&
	      (page_nr <= priv->page_ranges[i].end || priv->page_ranges[i].end == -1))
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
  if ((op->priv->manual_number_up < 2) ||
      (op->priv->page_position % op->priv->manual_number_up == 0))
    g_signal_emit_by_name (op, "got-page-size", print_context, page_setup);
}

static void
preview_end_page (GtkPrintOperation *op,
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
    case PROP_EMBED_PAGE_SETUP:
      gtk_print_operation_set_embed_page_setup (op, g_value_get_boolean (value));
      break;
    case PROP_HAS_SELECTION:
      gtk_print_operation_set_has_selection (op, g_value_get_boolean (value));
      break;
    case PROP_SUPPORT_SELECTION:
      gtk_print_operation_set_support_selection (op, g_value_get_boolean (value));
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
    case PROP_EMBED_PAGE_SETUP:
      g_value_set_boolean (value, priv->embed_page_setup);
      break;
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, priv->has_selection);
      break;
    case PROP_SUPPORT_SELECTION:
      g_value_set_boolean (value, priv->support_selection);
      break;
    case PROP_N_PAGES_TO_PRINT:
      g_value_set_int (value, priv->nr_of_pages_to_print);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

struct _PrintPagesData
{
  GtkPrintOperation *op;
  gint uncollated_copies;
  gint collated_copies;
  gint uncollated, collated, total;

  gint range, num_ranges;
  GtkPageRange *ranges;
  GtkPageRange one_range;

  gint page;
  gint sheet;
  gint first_position, last_position;
  gint first_sheet;
  gint num_of_sheets;
  gint *pages;

  GtkWidget *progress;
 
  gboolean initialized;
  gboolean is_preview;
  gboolean done;
};

typedef struct
{
  GtkPrintOperationPreview *preview;
  GtkPrintContext *print_context;
  GtkWindow *parent;
  cairo_surface_t *surface;
  gchar *filename;
  gboolean wait;
  PrintPagesData *pages_data;
} PreviewOp;

static void
preview_print_idle_done (gpointer data)
{
  GtkPrintOperation *op;
  PreviewOp *pop = (PreviewOp *) data;

  op = GTK_PRINT_OPERATION (pop->preview);

  cairo_surface_finish (pop->surface);
  /* Surface is destroyed in launch_preview */
  _gtk_print_operation_platform_backend_launch_preview (op,
							pop->surface,
							pop->parent,
							pop->filename);

  g_free (pop->filename);

  gtk_print_operation_preview_end_preview (pop->preview);

  g_object_unref (pop->pages_data->op);
  g_free (pop->pages_data->pages);
  g_free (pop->pages_data);

  g_object_unref (op);
  g_free (pop);
}

static gboolean
preview_print_idle (gpointer data)
{
  PreviewOp *pop;
  GtkPrintOperation *op;
  GtkPrintOperationPrivate *priv; 
  gboolean done = FALSE;

  pop = (PreviewOp *) data;
  op = GTK_PRINT_OPERATION (pop->preview);
  priv = op->priv;


  if (priv->page_drawing_state == GTK_PAGE_DRAWING_STATE_READY)
    {
      if (!pop->pages_data->initialized)
        {
          pop->pages_data->initialized = TRUE;
          prepare_data (pop->pages_data);
        }
      else
        {
          increment_page_sequence (pop->pages_data);

          if (!pop->pages_data->done)
            gtk_print_operation_preview_render_page (pop->preview, pop->pages_data->page);
          else
            done = priv->page_drawing_state == GTK_PAGE_DRAWING_STATE_READY;
        }
    }

  return !done;
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
  pop->print_context = context;

  g_object_ref (preview);
      
  gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE + 10,
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
  pop->pages_data = g_new0 (PrintPagesData, 1);
  pop->pages_data->op = g_object_ref (GTK_PRINT_OPERATION (preview));
  pop->pages_data->is_preview = TRUE;

  page_setup = gtk_print_context_get_page_setup (context);

  pop->surface =
    _gtk_print_operation_platform_backend_create_preview_surface (op,
								  page_setup,
								  &dpi_x, &dpi_y,
								  &pop->filename);

  if (pop->surface == NULL)
    {
      g_free (pop);
      return FALSE;
    }

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

static gboolean
gtk_print_operation_paginate (GtkPrintOperation *operation,
                              GtkPrintContext   *context)
{
  /* assume the number of pages is already set and pagination is not needed */
  return TRUE;
}

static void
gtk_print_operation_done (GtkPrintOperation       *operation,
                          GtkPrintOperationResult  result)
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

static gboolean
paginate_accumulator (GSignalInvocationHint *ihint,
                      GValue                *return_accu,
                      const GValue          *handler_return,
                      gpointer               dummy)
{
  *return_accu = *handler_return;

  /* Stop signal emission on first invocation, so if it's a callback then
   * the default handler won't run. */
  return FALSE;
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
  class->paginate = gtk_print_operation_paginate;
  class->done = gtk_print_operation_done;
  
  g_type_class_add_private (gobject_class, sizeof (GtkPrintOperationPrivate));

  /**
   * GtkPrintOperation::done:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @result: the result of the print operation
   *
   * Emitted when the print operation run has finished doing
   * everything required for printing. 
   *
   * @result gives you information about what happened during the run. 
   * If @result is %GTK_PRINT_OPERATION_RESULT_ERROR then you can call
   * gtk_print_operation_get_error() for more information.
   *
   * If you enabled print status tracking then 
   * gtk_print_operation_is_finished() may still return %FALSE 
   * after #GtkPrintOperation::done was emitted.
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
   * A typical use for ::begin-print is to use the parameters from the
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
   * Emitted after the #GtkPrintOperation::begin-print signal, but before 
   * the actual rendering starts. It keeps getting emitted until a connected 
   * signal handler returns %TRUE.
   *
   * The ::paginate signal is intended to be used for paginating a document
   * in small chunks, to avoid blocking the user interface for a long
   * time. The signal handler should update the number of pages using
   * gtk_print_operation_set_n_pages(), and return %TRUE if the document
   * has been completely paginated.
   *
   * If you don't need to do pagination in chunks, you can simply do
   * it all in the ::begin-print handler, and set the number of pages
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
		  paginate_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT,
		  G_TYPE_BOOLEAN, 1, GTK_TYPE_PRINT_CONTEXT);


  /**
   * GtkPrintOperation::request-page-setup:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   * @page_nr: the number of the currently printed page (0-based)
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
   * @page_nr: the number of the currently printed page (0-based)
   *
   * Emitted for every page that is printed. The signal handler
   * must render the @page_nr's page onto the cairo context obtained
   * from @context using gtk_print_context_get_cairo_context().
   * |[
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
   *   pango_layout_set_width (layout, width * PANGO_SCALE);
   *   pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
   *      		      
   *   pango_layout_get_size (layout, NULL, &layout_height);
   *   text_height = (gdouble)layout_height / PANGO_SCALE;
   *   
   *   cairo_move_to (cr, width / 2,  (HEADER_HEIGHT - text_height) / 2);
   *   pango_cairo_show_layout (cr, layout);
   *   
   *   g_object_unref (layout);
   * }
   * ]|
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
   * been allocated in the #GtkPrintOperation::begin-print handler.
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
   * The print dialog owns the returned widget, and its lifetime is not 
   * controlled by the application. However, the widget is guaranteed 
   * to stay around until the #GtkPrintOperation::custom-widget-apply 
   * signal is emitted on the operation. Then you can read out any 
   * information you need from the widgets.
   *
   * Returns: (transfer none): A custom widget that gets embedded in
   *          the print dialog, or %NULL
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
   * GtkPrintOperation::update-custom-widget:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @widget: the custom widget added in create-custom-widget
   * @setup: actual page setup
   * @settings: actual print settings
   *
   * Emitted after change of selected printer. The actual page setup and
   * print settings are passed to the custom widget, which can actualize
   * itself according to this change.
   *
   * Since: 2.18
   */
  signals[UPDATE_CUSTOM_WIDGET] =
    g_signal_new (I_("update-custom-widget"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, update_custom_widget),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT_OBJECT,
		  G_TYPE_NONE, 3, GTK_TYPE_WIDGET, GTK_TYPE_PAGE_SETUP, GTK_TYPE_PRINT_SETTINGS);

  /**
   * GtkPrintOperation::custom-widget-apply:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @widget: the custom widget added in create-custom-widget
   *
   * Emitted right before #GtkPrintOperation::begin-print if you added
   * a custom widget in the #GtkPrintOperation::create-custom-widget handler. 
   * When you get this signal you should read the information from the 
   * custom widgets, as the widgets are not guaraneed to be around at a 
   * later time.
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
   * @parent: (allow-none): the #GtkWindow to use as window parent, or %NULL
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
   * to the #GtkPrintOperation::request-page-setup signal.
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
   * before the rendering starts. It may be set in a 
   * #GtkPrintOperation::begin-print signal hander.
   *
   * Note that the page numbers passed to the 
   * #GtkPrintOperation::request-page-setup and 
   * #GtkPrintOperation::draw-page signals are 0-based, i.e. if 
   * the user chooses to print all pages, the last ::draw-page signal 
   * will be for page @n_pages - 1.
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
							 P_("TRUE if the origin of the context should be at the corner of the page and not the corner of the imageable area"),
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
   * emit the #GtkPrintOperation::done signal when the operation is actually 
   * done.
   *
   * The Windows port does not support asynchronous operation at all (this 
   * is unlikely to change). On other platforms, all actions except for 
   * %GTK_PRINT_OPERATION_ACTION_EXPORT support asynchronous operation.
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
   * The name of a file to generate instead of showing the print dialog. 
   * Currently, PDF is the only supported format.
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
   * See the #GtkPrintOperation:status property for a status value that 
   * is suitable for programmatic use. 
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

  /**
   * GtkPrintOperation:support-selection:
   *
   * If %TRUE, the print operation will support print of selection.
   * This allows the print dialog to show a "Selection" button.
   * 
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
				   PROP_SUPPORT_SELECTION,
				   g_param_spec_boolean ("support-selection",
							 P_("Support Selection"),
							 P_("TRUE if the print operation will support print of selection."),
							 FALSE,
							 GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:has-selection:
   *
   * Determines whether there is a selection in your application.
   * This can allow your application to print the selection.
   * This is typically used to make a "Selection" button sensitive.
   * 
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
				   PROP_HAS_SELECTION,
				   g_param_spec_boolean ("has-selection",
							 P_("Has Selection"),
							 P_("TRUE if a selection exists."),
							 FALSE,
							 GTK_PARAM_READWRITE));


  /**
   * GtkPrintOperation:embed-page-setup:
   *
   * If %TRUE, page size combo box and orientation combo box are embedded into page setup page.
   * 
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
				   PROP_EMBED_PAGE_SETUP,
				   g_param_spec_boolean ("embed-page-setup",
							 P_("Embed Page Setup"),
							 P_("TRUE if page setup combos are embedded in GtkPrintDialog"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  /**
   * GtkPrintOperation:n-pages-to-print:
   *
   * The number of pages that will be printed.
   *
   * Note that this value is set during print preparation phase
   * (%GTK_PRINT_STATUS_PREPARING), so this value should never be
   * get before the data generation phase (%GTK_PRINT_STATUS_GENERATING_DATA).
   * You can connect to the #GtkPrintOperation::status-changed signal
   * and call gtk_print_operation_get_n_pages_to_print() when
   * print status is %GTK_PRINT_STATUS_GENERATING_DATA.
   * This is typically used to track the progress of print operation.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
				   PROP_N_PAGES_TO_PRINT,
				   g_param_spec_int ("n-pages-to-print",
						     P_("Number of Pages To Print"),
						     P_("The number of pages that will be printed."),
						     -1,
						     G_MAXINT,
						     -1,
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
 * @default_page_setup: (allow-none): a #GtkPageSetup, or %NULL
 *
 * Makes @default_page_setup the default page setup for @op.
 *
 * This page setup will be used by gtk_print_operation_run(),
 * but it can be overridden on a per-page basis by connecting
 * to the #GtkPrintOperation::request-page-setup signal.
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
 * Returns: (transfer none): the default page setup
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
 * @print_settings: (allow-none): #GtkPrintSettings
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
 * Return value: (transfer none): the current print settings of @op.
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
 * before the rendering starts. It may be set in a 
 * #GtkPrintOperation::begin-print signal hander.
 *
 * Note that the page numbers passed to the 
 * #GtkPrintOperation::request-page-setup 
 * and #GtkPrintOperation::draw-page signals are 0-based, i.e. if 
 * the user chooses to print all pages, the last ::draw-page signal 
 * will be for page @n_pages - 1.
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
    NC_("print operation status", "Initial state"),
    NC_("print operation status", "Preparing to print"),
    NC_("print operation status", "Generating data"),
    NC_("print operation status", "Sending data"),
    NC_("print operation status", "Waiting"),
    NC_("print operation status", "Blocking on issue"),
    NC_("print operation status", "Printing"),
    NC_("print operation status", "Finished"),
    NC_("print operation status", "Finished with error")
  };

  if (status < 0 || status > GTK_PRINT_STATUS_FINISHED_ABORTED)
    status = GTK_PRINT_STATUS_FINISHED_ABORTED;

  if (string == NULL)
    string = g_dpgettext2 (GETTEXT_PACKAGE, "print operation status", status_strs[status]);
  
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
const gchar *
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
 * @label: (allow-none): the label to use, or %NULL to use the default label
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

  if ((op->priv->manual_number_up < 2) ||
      ((op->priv->page_position + 1) % op->priv->manual_number_up == 0) ||
      (op->priv->page_position == op->priv->nr_of_pages_to_print - 1))
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
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      g_set_error_literal (&priv->error,
                           GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_GENERAL,
                           cairo_status_to_string (cairo_surface_status (surface)));
      *do_print = FALSE;
      return GTK_PRINT_OPERATION_RESULT_ERROR;
    }

  /* this would crash on a nil surface */
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
  priv->manual_number_up = 1;
  priv->manual_number_up_layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
  
  *do_print = TRUE;
  
  priv->start_page = pdf_start_page;
  priv->end_page = pdf_end_page;
  priv->end_run = pdf_end_run;
  
  return GTK_PRINT_OPERATION_RESULT_APPLY; 
}


static void
clamp_page_ranges (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv; 
  gint                      num_of_correct_ranges;
  gint                      i;

  priv = data->op->priv;

  num_of_correct_ranges = 0;

  for (i = 0; i < data->num_ranges; i++)
    if ((data->ranges[i].start >= 0) &&
        (data->ranges[i].start < priv->nr_of_pages) &&
        (data->ranges[i].end >= 0) &&
        (data->ranges[i].end < priv->nr_of_pages))
      {
        data->ranges[num_of_correct_ranges] = data->ranges[i];
        num_of_correct_ranges++;
      }
    else if ((data->ranges[i].start >= 0) &&
             (data->ranges[i].start < priv->nr_of_pages) &&
             (data->ranges[i].end >= priv->nr_of_pages))
      {
        data->ranges[i].end = priv->nr_of_pages - 1;
        data->ranges[num_of_correct_ranges] = data->ranges[i];
        num_of_correct_ranges++;
      }
    else if ((data->ranges[i].end >= 0) &&
             (data->ranges[i].end < priv->nr_of_pages) &&
             (data->ranges[i].start < 0))
      {
        data->ranges[i].start = 0;
        data->ranges[num_of_correct_ranges] = data->ranges[i];
        num_of_correct_ranges++;
      }

  data->num_ranges = num_of_correct_ranges;
}

static void
increment_page_sequence (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv = data->op->priv;
  gint inc;

  if (data->total == -1)
    {
      data->total = 0;
      return;
    }

  /* check whether we reached last position */
  if (priv->page_position == data->last_position &&
      !(data->collated_copies > 1 && data->collated < (data->collated_copies - 1)))
    {
      if (data->uncollated_copies > 1 && data->uncollated < (data->uncollated_copies - 1))
        {
          priv->page_position = data->first_position;
          data->sheet = data->first_sheet;
          data->uncollated++;
        }
      else
        {
          data->done = TRUE;
	  return;
        }
    }
  else
    {
      if (priv->manual_reverse)
        inc = -1;
      else
        inc = 1;

      /* changing sheet */
      if (priv->manual_number_up < 2 ||
          (priv->page_position + 1) % priv->manual_number_up == 0 ||
          priv->page_position == data->last_position ||
          priv->page_position == priv->nr_of_pages_to_print - 1)
        {
          /* check whether to print the same sheet again */
          if (data->collated_copies > 1)
            {
              if (data->collated < (data->collated_copies - 1))
                {
                  data->collated++;
                  data->total++;
                  priv->page_position = data->sheet * priv->manual_number_up;

                  if (priv->page_position < 0 ||
                      priv->page_position >= priv->nr_of_pages_to_print ||
                      data->sheet < 0 ||
                      data->sheet >= data->num_of_sheets)
		    {
                      data->done = TRUE;
		      return;
		    }
                  else
                    data->page = data->pages[priv->page_position];

                  return;
                }
              else
                data->collated = 0;
            }

          if (priv->manual_page_set == GTK_PAGE_SET_ODD ||
              priv->manual_page_set == GTK_PAGE_SET_EVEN)
            data->sheet += 2 * inc;
          else
            data->sheet += inc;

          priv->page_position = data->sheet * priv->manual_number_up;
        }
      else
        priv->page_position += 1;
    }

  /* general check */
  if (priv->page_position < 0 ||
      priv->page_position >= priv->nr_of_pages_to_print ||
      data->sheet < 0 ||
      data->sheet >= data->num_of_sheets)
    {
      data->done = TRUE;
      return;
    }
  else
    data->page = data->pages[priv->page_position];

  data->total++;
}

static void
print_pages_idle_done (gpointer user_data)
{
  PrintPagesData *data;
  GtkPrintOperationPrivate *priv;

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

  if (!data->is_preview)
    g_signal_emit (data->op, signals[DONE], 0,
		   priv->cancelled ?
		   GTK_PRINT_OPERATION_RESULT_CANCEL :
		   GTK_PRINT_OPERATION_RESULT_APPLY);
  
  g_object_unref (data->op);
  g_free (data->pages);
  g_free (data);
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
	  if (priv->nr_of_pages_to_print > 0)
	    text = g_strdup_printf (_("Preparing %d"), priv->nr_of_pages_to_print);
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

/**
 * gtk_print_operation_set_defer_drawing:
 * @op: a #GtkPrintOperation
 * 
 * Sets up the #GtkPrintOperation to wait for calling of
 * gtk_print_operation_draw_page_finish() from application. It can
 * be used for drawing page in another thread.
 *
 * This function must be called in the callback of "draw-page" signal.
 *
 * Since: 2.16
 **/
void
gtk_print_operation_set_defer_drawing (GtkPrintOperation *op)
{
  GtkPrintOperationPrivate *priv = op->priv;

  g_return_if_fail (priv->page_drawing_state == GTK_PAGE_DRAWING_STATE_DRAWING);

  priv->page_drawing_state = GTK_PAGE_DRAWING_STATE_DEFERRED_DRAWING;
}

/**
 * gtk_print_operation_set_embed_page_setup:
 * @op: a #GtkPrintOperation
 * @embed: %TRUE to embed page setup selection in the #GtkPrintDialog
 *
 * Embed page size combo box and orientation combo box into page setup page.
 * Selected page setup is stored as default page setup in #GtkPrintOperation.
 *
 * Since: 2.18
 **/
void
gtk_print_operation_set_embed_page_setup (GtkPrintOperation  *op,
                                          gboolean            embed)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  embed = embed != FALSE;
  if (priv->embed_page_setup != embed)
    {
      priv->embed_page_setup = embed;
      g_object_notify (G_OBJECT (op), "embed-page-setup");
    }
}

/**
 * gtk_print_operation_get_embed_page_setup:
 * @op: a #GtkPrintOperation
 *
 * Gets the value of #GtkPrintOperation::embed-page-setup property.
 * 
 * Returns: whether page setup selection combos are embedded
 *
 * Since: 2.18
 */
gboolean
gtk_print_operation_get_embed_page_setup (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), FALSE);

  return op->priv->embed_page_setup;
}

/**
 * gtk_print_operation_draw_page_finish:
 * @op: a #GtkPrintOperation
 * 
 * Signalize that drawing of particular page is complete.
 *
 * It is called after completion of page drawing (e.g. drawing in another
 * thread).
 * If gtk_print_operation_set_defer_drawing() was called before, then this function
 * has to be called by application. In another case it is called by the library
 * itself.
 *
 * Since: 2.16
 **/
void
gtk_print_operation_draw_page_finish (GtkPrintOperation *op)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  GtkPrintContext *print_context;
  cairo_t *cr;
  
  print_context = priv->print_context;
  page_setup = gtk_print_context_get_page_setup (print_context);

  cr = gtk_print_context_get_cairo_context (print_context);

  priv->end_page (op, print_context);
  
  cairo_restore (cr);

  g_object_unref (page_setup);

  priv->page_drawing_state = GTK_PAGE_DRAWING_STATE_READY;
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
  if (priv->manual_scale != 1.0 && priv->manual_number_up <= 1)
    cairo_scale (cr,
		 priv->manual_scale,
		 priv->manual_scale);
  
  if (priv->manual_orientation)
    _gtk_print_context_rotate_according_to_orientation (print_context);

  if (priv->manual_number_up > 1)
    {
      GtkPageOrientation  orientation;
      GtkPageSetup       *page_setup;
      gdouble             paper_width, paper_height;
      gdouble             page_width, page_height;
      gdouble             context_width, context_height;
      gdouble             bottom_margin, top_margin, left_margin, right_margin;
      gdouble             x_step, y_step;
      gdouble             x_scale, y_scale, scale;
      gdouble             horizontal_offset = 0.0, vertical_offset = 0.0;
      gint                columns, rows, x, y, tmp_length;

      page_setup = gtk_print_context_get_page_setup (print_context);
      orientation = gtk_page_setup_get_orientation (page_setup);

      top_margin = gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_POINTS);
      bottom_margin = gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_POINTS);
      left_margin = gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_POINTS);
      right_margin = gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_POINTS);

      paper_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
      paper_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);

      context_width = gtk_print_context_get_width (print_context);
      context_height = gtk_print_context_get_height (print_context);

      if (orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
          orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
        {
          page_width = paper_width - (left_margin + right_margin);
          page_height = paper_height - (top_margin + bottom_margin);
        }
      else
        {
          page_width = paper_width - (top_margin + bottom_margin);
          page_height = paper_height - (left_margin + right_margin);
        }

      if (orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
          orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
        cairo_translate (cr, left_margin, top_margin);
      else
        cairo_translate (cr, top_margin, left_margin);

      switch (priv->manual_number_up)
        {
          default:
            columns = 1;
            rows = 1;
            break;
          case 2:
            columns = 2;
            rows = 1;
            break;
          case 4:
            columns = 2;
            rows = 2;
            break;
          case 6:
            columns = 3;
            rows = 2;
            break;
          case 9:
            columns = 3;
            rows = 3;
            break;
          case 16:
            columns = 4;
            rows = 4;
            break;
        }

      if (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE ||
          orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE)
        {
          tmp_length = columns;
          columns = rows;
          rows = tmp_length;
        }

      switch (priv->manual_number_up_layout)
        {
          case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM:
            x = priv->page_position % columns;
            y = (priv->page_position / columns) % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP:
            x = priv->page_position % columns;
            y = rows - 1 - (priv->page_position / columns) % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM:
            x = columns - 1 - priv->page_position % columns;
            y = (priv->page_position / columns) % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP:
            x = columns - 1 - priv->page_position % columns;
            y = rows - 1 - (priv->page_position / columns) % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT:
            x = (priv->page_position / rows) % columns;
            y = priv->page_position % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT:
            x = columns - 1 - (priv->page_position / rows) % columns;
            y = priv->page_position % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT:
            x = (priv->page_position / rows) % columns;
            y = rows - 1 - priv->page_position % rows;
            break;
          case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT:
            x = columns - 1 - (priv->page_position / rows) % columns;
            y = rows - 1 - priv->page_position % rows;
            break;
          default:
            g_assert_not_reached();
            x = 0;
            y = 0;
        }

      if (priv->manual_number_up == 4 || priv->manual_number_up == 9 || priv->manual_number_up == 16)
        {
          x_scale = page_width / (columns * paper_width);
          y_scale = page_height / (rows * paper_height);

          scale = x_scale < y_scale ? x_scale : y_scale;

          x_step = paper_width * (x_scale / scale);
          y_step = paper_height * (y_scale / scale);

          if ((left_margin + right_margin) > 0)
            {
              horizontal_offset = left_margin * (x_step - context_width) / (left_margin + right_margin);
              vertical_offset = top_margin * (y_step - context_height) / (top_margin + bottom_margin);
            }
          else
            {
              horizontal_offset = (x_step - context_width) / 2.0;
              vertical_offset = (y_step - context_height) / 2.0;
            }

          cairo_scale (cr, scale, scale);

          cairo_translate (cr,
                           x * x_step + horizontal_offset,
                           y * y_step + vertical_offset);

          if (priv->manual_scale != 1.0)
            cairo_scale (cr, priv->manual_scale, priv->manual_scale);
        }

      if (priv->manual_number_up == 2 || priv->manual_number_up == 6)
        {
          x_scale = page_height / (columns * paper_width);
          y_scale = page_width / (rows * paper_height);

          scale = x_scale < y_scale ? x_scale : y_scale;

          horizontal_offset = (paper_width * (x_scale / scale) - paper_width) / 2.0 * columns;
          vertical_offset = (paper_height * (y_scale / scale) - paper_height) / 2.0 * rows;

          if (!priv->use_full_page)
            {
              horizontal_offset -= right_margin;
              vertical_offset += top_margin;
            }

          cairo_scale (cr, scale, scale);

          cairo_translate (cr,
                           y * paper_height + vertical_offset,
                           (columns - x) * paper_width + horizontal_offset);

          if (priv->manual_scale != 1.0)
            cairo_scale (cr, priv->manual_scale, priv->manual_scale);

          cairo_rotate (cr, - G_PI / 2);
        }
    }
  else
    if (!priv->use_full_page)
      _gtk_print_context_translate_into_margin (print_context);
  
  priv->page_drawing_state = GTK_PAGE_DRAWING_STATE_DRAWING;

  g_signal_emit (op, signals[DRAW_PAGE], 0, 
		 print_context, page_nr);

  if (priv->page_drawing_state == GTK_PAGE_DRAWING_STATE_DRAWING)
    gtk_print_operation_draw_page_finish (op);
}

static void
prepare_data (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv;
  GtkPageSetup             *page_setup;
  gboolean                  paginated = FALSE;
  gint                      i, j, counter;

  priv = data->op->priv;

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

  if (!data->initialized)
    {
      data->initialized = TRUE;
      page_setup = create_page_setup (data->op);
      _gtk_print_context_set_page_setup (priv->print_context,
                                         page_setup);
      g_object_unref (page_setup);

      g_signal_emit (data->op, signals[BEGIN_PRINT], 0, priv->print_context);

      return;
    }

  g_signal_emit (data->op, signals[PAGINATE], 0, priv->print_context, &paginated);
  if (!paginated)
    return;

  /* Initialize parts of PrintPagesData that depend on nr_of_pages
   */
  if (priv->print_pages == GTK_PRINT_PAGES_RANGES)
    {
      if (priv->page_ranges == NULL) 
        {
          g_warning ("no pages to print");
          priv->cancelled = TRUE;
          return;
        }
      data->ranges = priv->page_ranges;
      data->num_ranges = priv->num_page_ranges;
      for (i = 0; i < data->num_ranges; i++)
        if (data->ranges[i].end == -1 || 
            data->ranges[i].end >= priv->nr_of_pages)
          data->ranges[i].end = priv->nr_of_pages - 1;
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

  clamp_page_ranges (data);

  if (data->num_ranges < 1) 
    {
      priv->cancelled = TRUE;
      return;
    }

  priv->nr_of_pages_to_print = 0;
  for (i = 0; i < data->num_ranges; i++)
    priv->nr_of_pages_to_print += data->ranges[i].end - data->ranges[i].start + 1;

  data->pages = g_new (gint, priv->nr_of_pages_to_print);
  counter = 0;
  for (i = 0; i < data->num_ranges; i++)
    for (j = data->ranges[i].start; j <= data->ranges[i].end; j++)
      {
        data->pages[counter] = j;
        counter++;
      }

  data->total = -1;
  data->collated = 0;
  data->uncollated = 0;

  if (priv->manual_number_up > 1)
    {
      if (priv->nr_of_pages_to_print % priv->manual_number_up == 0)
        data->num_of_sheets = priv->nr_of_pages_to_print / priv->manual_number_up;
      else
        data->num_of_sheets = priv->nr_of_pages_to_print / priv->manual_number_up + 1;
    }
  else
    data->num_of_sheets = priv->nr_of_pages_to_print;

  if (priv->manual_reverse)
    {
      /* data->sheet is 0-based */
      if (priv->manual_page_set == GTK_PAGE_SET_ODD)
        data->sheet = (data->num_of_sheets - 1) - (data->num_of_sheets - 1) % 2;
      else if (priv->manual_page_set == GTK_PAGE_SET_EVEN)
        data->sheet = (data->num_of_sheets - 1) - (1 - (data->num_of_sheets - 1) % 2);
      else
        data->sheet = data->num_of_sheets - 1;
    }
  else
    {
      /* data->sheet is 0-based */
      if (priv->manual_page_set == GTK_PAGE_SET_ODD)
        data->sheet = 0;
      else if (priv->manual_page_set == GTK_PAGE_SET_EVEN)
        {
          if (data->num_of_sheets > 1)
            data->sheet = 1;
          else
            data->sheet = -1;
        }
      else
        data->sheet = 0;
    }

  priv->page_position = data->sheet * priv->manual_number_up;

  if (priv->page_position < 0 || priv->page_position >= priv->nr_of_pages_to_print)
    {
      priv->cancelled = TRUE;
      return;
    }

  data->page = data->pages[priv->page_position];
  data->first_position = priv->page_position;
  data->first_sheet = data->sheet;

  if (priv->manual_reverse)
    {
      if (priv->manual_page_set == GTK_PAGE_SET_ODD)
        data->last_position = MIN (priv->manual_number_up - 1, priv->nr_of_pages_to_print - 1);
      else if (priv->manual_page_set == GTK_PAGE_SET_EVEN)
        data->last_position = MIN (2 * priv->manual_number_up - 1, priv->nr_of_pages_to_print - 1);
      else
        data->last_position = MIN (priv->manual_number_up - 1, priv->nr_of_pages_to_print - 1);
    }
  else
    {
      if (priv->manual_page_set == GTK_PAGE_SET_ODD)
        data->last_position = MIN (((data->num_of_sheets - 1) - ((data->num_of_sheets - 1) % 2)) * priv->manual_number_up - 1, priv->nr_of_pages_to_print - 1);
      else if (priv->manual_page_set == GTK_PAGE_SET_EVEN)
        data->last_position = MIN (((data->num_of_sheets - 1) - (1 - (data->num_of_sheets - 1) % 2)) * priv->manual_number_up - 1, priv->nr_of_pages_to_print - 1);
      else
        data->last_position = priv->nr_of_pages_to_print - 1;
    }


  _gtk_print_operation_set_status (data->op, 
                                   GTK_PRINT_STATUS_GENERATING_DATA, 
                                   NULL);
}

static gboolean
print_pages_idle (gpointer user_data)
{
  PrintPagesData *data; 
  GtkPrintOperationPrivate *priv;
  gboolean done = FALSE;

  data = (PrintPagesData*)user_data;
  priv = data->op->priv;

  if (priv->page_drawing_state == GTK_PAGE_DRAWING_STATE_READY)
    {
      if (priv->status == GTK_PRINT_STATUS_PREPARING)
        {
          prepare_data (data);
          goto out;
        }

      if (data->is_preview && !priv->cancelled)
        {
          done = TRUE;

          g_signal_emit_by_name (data->op, "ready", priv->print_context);
          goto out;
        }

      increment_page_sequence (data);

      if (!data->done)
        common_render_page (data->op, data->page);
      else
        done = priv->page_drawing_state == GTK_PAGE_DRAWING_STATE_READY;

 out:

      if (priv->cancelled)
        {
          _gtk_print_operation_set_status (data->op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);

          data->is_preview = FALSE;
          done = TRUE;
        }

      if (done && !data->is_preview)
        {
          g_signal_emit (data->op, signals[END_PRINT], 0, priv->print_context);
          priv->end_run (data->op, priv->is_sync, priv->cancelled);
        }

      update_progress (data);
    }

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
  gtk_window_present (GTK_WINDOW (data->progress));

  data->op->priv->show_progress_timeout_id = 0;

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
	gdk_threads_add_timeout (SHOW_PROGRESS_TIME, 
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

      if (!handled)
        {
          GtkWidget *error_dialog;

          error_dialog = gtk_message_dialog_new (parent,
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_OK,
                                                 _("Error creating print preview"));

          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (error_dialog),
                                                    _("The most probable reason is that a temporary file could not be created."));

          if (parent && parent->group)
            gtk_window_group_add_window (parent->group, GTK_WINDOW (error_dialog));

          g_signal_connect (error_dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), NULL);

          gtk_widget_show (error_dialog);

          print_pages_idle_done (data);

          return;
        }

      if (gtk_print_context_get_cairo_context (priv->print_context) == NULL)
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
      priv->manual_reverse = gtk_print_settings_get_reverse (priv->print_settings);
      priv->manual_page_set = gtk_print_settings_get_page_set (priv->print_settings);
      priv->manual_scale = gtk_print_settings_get_scale (priv->print_settings) / 100.0;
      priv->manual_orientation = TRUE;
      priv->manual_number_up = gtk_print_settings_get_number_up (priv->print_settings);
      priv->manual_number_up_layout = gtk_print_settings_get_number_up_layout (priv->print_settings);
    }
  
  priv->print_pages_idle_id = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE + 10,
					                 print_pages_idle, 
					                 data, 
					                 print_pages_idle_done);
  
  /* Recursive main loop to make sure we don't exit  on sync operations  */
  if (priv->is_sync)
    {
      priv->rloop = g_main_loop_new (NULL, FALSE);

      g_object_ref (op);
      GDK_THREADS_LEAVE ();
      g_main_loop_run (priv->rloop);
      GDK_THREADS_ENTER ();
      
      g_main_loop_unref (priv->rloop);
      priv->rloop = NULL;
      g_object_unref (op);
    }
}

/**
 * gtk_print_operation_get_error:
 * @op: a #GtkPrintOperation
 * @error: return location for the error
 * 
 * Call this when the result of a print operation is
 * %GTK_PRINT_OPERATION_RESULT_ERROR, either as returned by 
 * gtk_print_operation_run(), or in the #GtkPrintOperation::done signal 
 * handler. The returned #GError will contain more details on what went wrong.
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
 * @parent: (allow-none): Transient parent of the dialog
 * @error: (allow-none): Return location for errors, or %NULL
 *
 * Runs the print operation, by first letting the user modify
 * print settings in the print dialog, and then print the document.
 *
 * Normally that this function does not return until the rendering of all 
 * pages is complete. You can connect to the 
 * #GtkPrintOperation::status-changed signal on @op to obtain some 
 * information about the progress of the print operation. 
 * Furthermore, it may use a recursive mainloop to show the print dialog.
 *
 * If you call gtk_print_operation_set_allow_async() or set the 
 * #GtkPrintOperation:allow-async property the operation will run 
 * asynchronously if this is supported on the platform. The 
 * #GtkPrintOperation::done signal will be emitted with the result of the 
 * operation when the it is done (i.e. when the dialog is canceled, or when 
 * the print succeeds or fails).
 * |[
 * if (settings != NULL)
 *   gtk_print_operation_set_print_settings (print, settings);
 *   
 * if (page_setup != NULL)
 *   gtk_print_operation_set_default_page_setup (print, page_setup);
 *   
 * g_signal_connect (print, "begin-print", 
 *                   G_CALLBACK (begin_print), &data);
 * g_signal_connect (print, "draw-page", 
 *                   G_CALLBACK (draw_page), &data);
 *  
 * res = gtk_print_operation_run (print, 
 *                                GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, 
 *                                parent, 
 *                                &error);
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
 * ]|
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
 *   asynchronously, and will emit the #GtkPrintOperation::done signal when 
 *   done.
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
 * be called from a #GtkPrintOperation::begin-print, 
 * #GtkPrintOperation::paginate or #GtkPrintOperation::draw-page
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

/**
 * gtk_print_operation_set_support_selection:
 * @op: a #GtkPrintOperation
 * @support_selection: %TRUE to support selection
 *
 * Sets whether selection is supported by #GtkPrintOperation.
 *
 * Since: 2.18
 */
void
gtk_print_operation_set_support_selection (GtkPrintOperation  *op,
                                           gboolean            support_selection)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  support_selection = support_selection != FALSE;
  if (priv->support_selection != support_selection)
    {
      priv->support_selection = support_selection;
      g_object_notify (G_OBJECT (op), "support-selection");
    }
}

/**
 * gtk_print_operation_get_support_selection:
 * @op: a #GtkPrintOperation
 *
 * Gets the value of #GtkPrintOperation::support-selection property.
 * 
 * Returns: whether the application supports print of selection
 *
 * Since: 2.18
 */
gboolean
gtk_print_operation_get_support_selection (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), FALSE);

  return op->priv->support_selection;
}

/**
 * gtk_print_operation_set_has_selection:
 * @op: a #GtkPrintOperation
 * @has_selection: %TRUE indicates that a selection exists
 *
 * Sets whether there is a selection to print.
 *
 * Application has to set number of pages to which the selection
 * will draw by gtk_print_operation_set_n_pages() in a callback of
 * #GtkPrintOperation::begin-print.
 *
 * Since: 2.18
 */
void
gtk_print_operation_set_has_selection (GtkPrintOperation  *op,
                                       gboolean            has_selection)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  has_selection = has_selection != FALSE;
  if (priv->has_selection != has_selection)
    {
      priv->has_selection = has_selection;
      g_object_notify (G_OBJECT (op), "has-selection");
    }
}

/**
 * gtk_print_operation_get_has_selection:
 * @op: a #GtkPrintOperation
 *
 * Gets the value of #GtkPrintOperation::has-selection property.
 * 
 * Returns: whether there is a selection
 *
 * Since: 2.18
 */
gboolean
gtk_print_operation_get_has_selection (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), FALSE);

  return op->priv->has_selection;
}

/**
 * gtk_print_operation_get_n_pages_to_print:
 * @op: a #GtkPrintOperation
 *
 * Returns the number of pages that will be printed.
 *
 * Note that this value is set during print preparation phase
 * (%GTK_PRINT_STATUS_PREPARING), so this function should never be
 * called before the data generation phase (%GTK_PRINT_STATUS_GENERATING_DATA).
 * You can connect to the #GtkPrintOperation::status-changed signal
 * and call gtk_print_operation_get_n_pages_to_print() when
 * print status is %GTK_PRINT_STATUS_GENERATING_DATA.
 * This is typically used to track the progress of print operation.
 *
 * Returns: the number of pages that will be printed
 *
 * Since: 2.18
 **/
gint
gtk_print_operation_get_n_pages_to_print (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), -1);

  return op->priv->nr_of_pages_to_print;
}

#define __GTK_PRINT_OPERATION_C__
#include "gtkaliasdef.c"
