/* GtkPrintUnixDialog
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
 * Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
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
#include <math.h>

#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkspinbutton.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtktreeselection.h"
#include "gtknotebook.h"
#include "gtkscrolledwindow.h"
#include "gtkcombobox.h"
#include "gtktogglebutton.h"
#include "gtkradiobutton.h"
#include "gtkdrawingarea.h"
#include "gtkvbox.h"
#include "gtktable.h"
#include "gtkframe.h"
#include "gtkalignment.h"
#include "gtklabel.h"

#include "gtkprintbackend.h"
#include "gtkprinter-private.h"
#include "gtkprintunixdialog.h"
#include "gtkprinteroptionwidget.h"
#include "gtkalias.h"

#define EXAMPLE_PAGE_AREA_SIZE 140

#define GTK_PRINT_UNIX_DIALOG_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_PRINT_UNIX_DIALOG, GtkPrintUnixDialogPrivate))

static void     gtk_print_unix_dialog_destroy      (GtkPrintUnixDialog *dialog);
static void     gtk_print_unix_dialog_finalize     (GObject            *object);
static void     gtk_print_unix_dialog_set_property (GObject            *object,
						    guint               prop_id,
						    const GValue       *value,
						    GParamSpec         *pspec);
static void     gtk_print_unix_dialog_get_property (GObject            *object,
						    guint               prop_id,
						    GValue             *value,
						    GParamSpec         *pspec);
static void     gtk_print_unix_dialog_style_set    (GtkWidget          *widget,
						    GtkStyle           *previous_style);
static void     populate_dialog                    (GtkPrintUnixDialog *dialog);
static void     unschedule_idle_mark_conflicts     (GtkPrintUnixDialog *dialog);
static void     selected_printer_changed           (GtkTreeSelection   *selection,
						    GtkPrintUnixDialog *dialog);
static void     clear_per_printer_ui               (GtkPrintUnixDialog *dialog);
static void     printer_added_cb                   (GtkPrintBackend    *backend,
						    GtkPrinter         *printer,
						    GtkPrintUnixDialog *dialog);
static void     printer_removed_cb                 (GtkPrintBackend    *backend,
						    GtkPrinter         *printer,
						    GtkPrintUnixDialog *dialog);
static void     printer_status_cb                  (GtkPrintBackend    *backend,
						    GtkPrinter         *printer,
						    GtkPrintUnixDialog *dialog);
static void     update_collate_icon                (GtkToggleButton    *toggle_button,
						    GtkPrintUnixDialog *dialog);
static gboolean dialog_get_collate                 (GtkPrintUnixDialog *dialog);
static gboolean dialog_get_reverse                 (GtkPrintUnixDialog *dialog);
static gint     dialog_get_n_copies                (GtkPrintUnixDialog *dialog);

enum {
  PROP_0,
  PROP_PAGE_SETUP,
  PROP_CURRENT_PAGE,
  PROP_PRINT_SETTINGS,
  PROP_SELECTED_PRINTER
};

enum {
  PRINTER_LIST_COL_ICON,
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_STATE,
  PRINTER_LIST_COL_JOBS,
  PRINTER_LIST_COL_LOCATION,
  PRINTER_LIST_COL_PRINTER_OBJ,
  PRINTER_LIST_N_COLS
};

struct GtkPrintUnixDialogPrivate
{
  GtkWidget *notebook;

  GtkWidget *printer_treeview;

  GtkPrintCapabilities manual_capabilities;
  GtkPrintCapabilities printer_capabilities;
  
  GtkTreeModel *printer_list;
  GtkTreeModelFilter *printer_list_filter;

  GtkPageSetup *page_setup;

  GtkWidget *all_pages_radio;
  GtkWidget *current_page_radio;
  GtkWidget *page_range_radio;
  GtkWidget *page_range_entry;
  
  GtkWidget *copies_spin;
  GtkWidget *collate_check;
  GtkWidget *reverse_check;
  GtkWidget *collate_image;
  GtkWidget *page_layout_preview;
  GtkWidget *scale_spin;
  GtkWidget *page_set_combo;
  GtkWidget *print_now_radio;
  GtkWidget *print_at_radio;
  GtkWidget *print_at_entry;
  GtkWidget *print_hold_radio;
  GtkWidget *preview_button;
  gboolean updating_print_at;
  GtkPrinterOptionWidget *pages_per_sheet;
  GtkPrinterOptionWidget *duplex;
  GtkPrinterOptionWidget *paper_type;
  GtkPrinterOptionWidget *paper_source;
  GtkPrinterOptionWidget *output_tray;
  GtkPrinterOptionWidget *job_prio;
  GtkPrinterOptionWidget *billing_info;
  GtkPrinterOptionWidget *cover_before;
  GtkPrinterOptionWidget *cover_after;

  GtkWidget *conflicts_widget;

  GtkWidget *job_page;
  GtkWidget *finishing_table;
  GtkWidget *finishing_page;
  GtkWidget *image_quality_table;
  GtkWidget *image_quality_page;
  GtkWidget *color_table;
  GtkWidget *color_page;

  GtkWidget *advanced_vbox;
  GtkWidget *advanced_page;

  GtkWidget *extension_point;

  /* These are set initially on selected printer (either default printer, 
   * printer taken from set settings, or user-selected), but when any setting 
   * is changed by the user it is cleared.
   */
  GtkPrintSettings *initial_settings;
  
  /* This is the initial printer set by set_settings. We look for it in the
   * added printers. We clear this whenever the user manually changes
   * to another printer, when the user changes a setting or when we find
   * this printer.
   */
  char *waiting_for_printer;
  gboolean internal_printer_change;
  
  GList *print_backends;
  
  GtkPrinter *current_printer;
  guint request_details_tag;
  GtkPrinterOptionSet *options;
  gulong options_changed_handler;
  gulong mark_conflicts_id;

  gchar *format_for_printer;
  
  gint current_page;
};

G_DEFINE_TYPE (GtkPrintUnixDialog, gtk_print_unix_dialog, GTK_TYPE_DIALOG)

static gboolean
is_default_printer (GtkPrintUnixDialog *dialog,
		    GtkPrinter         *printer)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  if (priv->format_for_printer)
    return strcmp (priv->format_for_printer,
		   gtk_printer_get_name (printer)) == 0;
 else
   return gtk_printer_is_default (printer);
}

static void
gtk_print_unix_dialog_class_init (GtkPrintUnixDialogClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  object_class->finalize = gtk_print_unix_dialog_finalize;
  object_class->set_property = gtk_print_unix_dialog_set_property;
  object_class->get_property = gtk_print_unix_dialog_get_property;

  widget_class->style_set = gtk_print_unix_dialog_style_set;

  g_object_class_install_property (object_class,
				   PROP_PAGE_SETUP,
				   g_param_spec_object ("page-setup",
							P_("Page Setup"),
							P_("The GtkPageSetup to use"),
							GTK_TYPE_PAGE_SETUP,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_CURRENT_PAGE,
				   g_param_spec_int ("current-page",
						     P_("Current Page"),
						     P_("The current page in the document"),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_PRINT_SETTINGS,
				   g_param_spec_object ("print-settings",
							P_("Print Settings"),
							P_("The GtkPrintSettings used for initializing the dialog"),
							GTK_TYPE_PRINT_SETTINGS,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_SELECTED_PRINTER,
				   g_param_spec_object ("selected-printer",
							P_("Selected Printer"),
							P_("The GtkPrinter which is selected"),
							GTK_TYPE_PRINTER,
							GTK_PARAM_READABLE));
  
  
  g_type_class_add_private (class, sizeof (GtkPrintUnixDialogPrivate));  
}

static void
gtk_print_unix_dialog_init (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  priv = dialog->priv = GTK_PRINT_UNIX_DIALOG_GET_PRIVATE (dialog); 
  priv->print_backends = NULL;
  priv->current_page = -1;

  priv->page_setup = gtk_page_setup_new ();

  g_signal_connect (dialog, 
                    "destroy", 
		    (GCallback) gtk_print_unix_dialog_destroy, 
		    NULL);

  priv->preview_button = gtk_button_new_from_stock (GTK_STOCK_PRINT_PREVIEW);
  gtk_widget_show (priv->preview_button);
   
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), 
                                priv->preview_button, 
                                GTK_RESPONSE_APPLY);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			  GTK_STOCK_PRINT, GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

  populate_dialog (dialog);  
}

static void
gtk_print_unix_dialog_destroy (GtkPrintUnixDialog *dialog)
{
  /* Make sure we don't destroy custom widgets owned by the backends */
  clear_per_printer_ui (dialog);  
}

static void
gtk_print_unix_dialog_finalize (GObject *object)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrintBackend *backend;
  GList *node;

  unschedule_idle_mark_conflicts (dialog);

  if (priv->request_details_tag)
    {
      g_source_remove (priv->request_details_tag);
      priv->request_details_tag = 0;
    }
  
  if (priv->current_printer)
    {
      g_object_unref (priv->current_printer);
      priv->current_printer = NULL;
    }

  if (priv->printer_list)
    {
      g_object_unref (priv->printer_list);
      priv->printer_list = NULL;
    }
 
  if (priv->printer_list_filter)
    {
      g_object_unref (priv->printer_list_filter);
      priv->printer_list_filter = NULL;
    }

 
  if (priv->options)
    {
      g_object_unref (priv->options);
      priv->options = NULL;
    }
 
 if (priv->page_setup)
    {
      g_object_unref (priv->page_setup);
      priv->page_setup = NULL;
    }

  if (priv->initial_settings)
    {
      g_object_unref (priv->initial_settings);
      priv->initial_settings = NULL;
    }

  g_free (priv->waiting_for_printer);
  priv->waiting_for_printer = NULL;
  
  g_free (priv->format_for_printer);
  priv->format_for_printer = NULL;

  for (node = priv->print_backends; node != NULL; node = node->next)
    {
      backend = GTK_PRINT_BACKEND (node->data);

      g_signal_handlers_disconnect_by_func (backend, printer_added_cb, dialog);
      g_signal_handlers_disconnect_by_func (backend, printer_removed_cb, dialog);
      g_signal_handlers_disconnect_by_func (backend, printer_status_cb, dialog);

      gtk_print_backend_destroy (backend);
      g_object_unref (backend);
    }
  
  g_list_free (priv->print_backends);
  priv->print_backends = NULL;
  
  G_OBJECT_CLASS (gtk_print_unix_dialog_parent_class)->finalize (object);
}

static void
printer_removed_cb (GtkPrintBackend    *backend, 
                    GtkPrinter         *printer, 
		    GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");
  gtk_list_store_remove (GTK_LIST_STORE (priv->printer_list), iter);
}

static void
printer_status_cb (GtkPrintBackend    *backend, 
		   GtkPrinter         *printer, 
		   GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");

  gtk_list_store_set (GTK_LIST_STORE (priv->printer_list), iter,
                      PRINTER_LIST_COL_ICON, gtk_printer_get_icon_name (printer),
                      PRINTER_LIST_COL_STATE, gtk_printer_get_state_message (printer),
                      PRINTER_LIST_COL_JOBS, gtk_printer_get_job_count (printer),
                      PRINTER_LIST_COL_LOCATION, gtk_printer_get_location (printer),
                      -1);

}

static void
printer_added_cb (GtkPrintBackend    *backend, 
                  GtkPrinter         *printer, 
		  GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter, filter_iter;
  GtkTreeSelection *selection;
  GtkTreePath *path;

  gtk_list_store_append (GTK_LIST_STORE (priv->printer_list), &iter);
  
  g_object_set_data_full (G_OBJECT (printer), 
                         "gtk-print-tree-iter", 
                          gtk_tree_iter_copy (&iter),
                          (GDestroyNotify) gtk_tree_iter_free);

  gtk_list_store_set (GTK_LIST_STORE (priv->printer_list), &iter,
                      PRINTER_LIST_COL_ICON, gtk_printer_get_icon_name (printer),
                      PRINTER_LIST_COL_NAME, gtk_printer_get_name (printer),
                      PRINTER_LIST_COL_STATE, gtk_printer_get_state_message (printer),
                      PRINTER_LIST_COL_JOBS, gtk_printer_get_job_count (printer),
                      PRINTER_LIST_COL_LOCATION, gtk_printer_get_location (printer),
                      PRINTER_LIST_COL_PRINTER_OBJ, printer,
                      -1);

  gtk_tree_model_filter_convert_child_iter_to_iter (priv->printer_list_filter,
						    &filter_iter, &iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->printer_list_filter), &filter_iter);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->printer_treeview));
  
  if (priv->waiting_for_printer != NULL &&
      strcmp (gtk_printer_get_name (printer),
	      priv->waiting_for_printer) == 0)
    {
      priv->internal_printer_change = TRUE;
      gtk_tree_selection_select_iter (selection, &filter_iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->printer_treeview),
                                    path, NULL, TRUE, 0.5, 0.0);
      priv->internal_printer_change = FALSE;
      g_free (priv->waiting_for_printer);
      priv->waiting_for_printer = NULL;
    }
  else if (is_default_printer (dialog, printer) &&
	   gtk_tree_selection_count_selected_rows (selection) == 0)
    {
      priv->internal_printer_change = TRUE;
      gtk_tree_selection_select_iter (selection, &filter_iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->printer_treeview),
                                    path, NULL, TRUE, 0.5, 0.0);
      priv->internal_printer_change = FALSE;
    }

  gtk_tree_path_free (path);
}

static void
printer_list_initialize (GtkPrintUnixDialog *dialog,
			 GtkPrintBackend    *print_backend)
{
  GList *list;
  GList *node;

  g_return_if_fail (print_backend != NULL);

  g_signal_connect_object (print_backend, 
			   "printer-added", 
			   (GCallback) printer_added_cb, 
			   G_OBJECT (dialog), 0);

  g_signal_connect_object (print_backend, 
			   "printer-removed", 
			   (GCallback) printer_removed_cb, 
			   G_OBJECT (dialog), 0);

  g_signal_connect_object (print_backend, 
			   "printer-status-changed", 
			   (GCallback) printer_status_cb, 
			   G_OBJECT (dialog), 0);

  list = gtk_print_backend_get_printer_list (print_backend);

  node = list;
  while (node != NULL)
    {
      printer_added_cb (print_backend, node->data, dialog);
      node = node->next;
    }

  g_list_free (list);
}

static void
load_print_backends (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GList *node;

  if (g_module_supported ())
    priv->print_backends = gtk_print_backend_load_modules ();

  for (node = priv->print_backends; node != NULL; node = node->next)
    printer_list_initialize (dialog, GTK_PRINT_BACKEND (node->data));
}

static void
gtk_print_unix_dialog_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)

{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);

  switch (prop_id)
    {
    case PROP_PAGE_SETUP:
      gtk_print_unix_dialog_set_page_setup (dialog, g_value_get_object (value));
      break;
    case PROP_CURRENT_PAGE:
      gtk_print_unix_dialog_set_current_page (dialog, g_value_get_int (value));
      break;
    case PROP_PRINT_SETTINGS:
      gtk_print_unix_dialog_set_settings (dialog, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_unix_dialog_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    case PROP_PAGE_SETUP:
      g_value_set_object (value, priv->page_setup);
      break;
    case PROP_CURRENT_PAGE:
      g_value_set_int (value, priv->current_page);
      break;
    case PROP_PRINT_SETTINGS:
      g_value_set_object (value, gtk_print_unix_dialog_get_settings (dialog));
      break;
    case PROP_SELECTED_PRINTER:
      g_value_set_object (value, priv->current_printer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
is_printer_active (GtkTreeModel       *model,
                   GtkTreeIter        *iter,
		   GtkPrintUnixDialog *dialog)
{
  gboolean result;
  GtkPrinter *printer;
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  gtk_tree_model_get (model,
		      iter,
		      PRINTER_LIST_COL_PRINTER_OBJ,
                      &printer,
		      -1);
  
  if (printer == NULL)
    return FALSE;

  result = gtk_printer_is_active (printer);
  
  if (result && 
      priv->manual_capabilities & (GTK_PRINT_CAPABILITY_GENERATE_PDF |
				   GTK_PRINT_CAPABILITY_GENERATE_PS))
    {
       /* Check that the printer can handle at least one of the data 
	* formats that the application supports.
        */
       result = ((priv->manual_capabilities & GTK_PRINT_CAPABILITY_GENERATE_PDF) &&
		 gtk_printer_accepts_pdf (printer)) ||
		((priv->manual_capabilities & GTK_PRINT_CAPABILITY_GENERATE_PS) &&
		 gtk_printer_accepts_ps (printer));
    }
  
  g_object_unref (printer);
  
  return result;
}

static gint
default_printer_list_sort_func (GtkTreeModel *model,
                                GtkTreeIter  *a,
                                GtkTreeIter  *b,
                                gpointer      user_data)
{
  gchar *a_name;
  gchar *b_name;
  GtkPrinter *a_printer;
  GtkPrinter *b_printer;
  gint result;

  gtk_tree_model_get (model, a, 
                      PRINTER_LIST_COL_NAME, &a_name, 
		      PRINTER_LIST_COL_PRINTER_OBJ, &a_printer,
		      -1);
  gtk_tree_model_get (model, b, 
                      PRINTER_LIST_COL_NAME, &b_name,
		      PRINTER_LIST_COL_PRINTER_OBJ, &b_printer,
		      -1);

  if (a_printer == NULL && b_printer == NULL)
    result = 0;
  else if (a_printer == NULL)
   result = G_MAXINT;
  else if (b_printer == NULL)
   result = G_MININT;
  else if (gtk_printer_is_virtual (a_printer) && gtk_printer_is_virtual (b_printer))
    result = 0;
  else if (gtk_printer_is_virtual (a_printer) && !gtk_printer_is_virtual (b_printer))
    result = G_MININT;
  else if (!gtk_printer_is_virtual (a_printer) && gtk_printer_is_virtual (b_printer))
    result = G_MAXINT;
  else if (a_name == NULL && b_name == NULL)
    result = 0;
  else if (a_name == NULL && b_name != NULL)
    result = 1;
  else if (a_name != NULL && b_name == NULL)
    result = -1;
  else
    result = g_ascii_strcasecmp (a_name, b_name);

  g_free (a_name);
  g_free (b_name);
  g_object_unref (a_printer);
  g_object_unref (b_printer);

  return result;
}


static void
create_printer_list_model (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkListStore *model;
  GtkTreeSortable *sort;

  model = gtk_list_store_new (PRINTER_LIST_N_COLS,
                              G_TYPE_STRING,
                              G_TYPE_STRING, 
                              G_TYPE_STRING, 
                              G_TYPE_INT, 
                              G_TYPE_STRING,
                              G_TYPE_OBJECT);

  priv->printer_list = (GtkTreeModel *)model;
  priv->printer_list_filter = (GtkTreeModelFilter *) gtk_tree_model_filter_new ((GtkTreeModel *)model,
											NULL);

  gtk_tree_model_filter_set_visible_func (priv->printer_list_filter,
					  (GtkTreeModelFilterVisibleFunc) is_printer_active,
					  dialog,
					  NULL);

  sort = GTK_TREE_SORTABLE (model);
  gtk_tree_sortable_set_default_sort_func (sort,
					   default_printer_list_sort_func,
					   NULL,
					   NULL);
 
  gtk_tree_sortable_set_sort_column_id (sort,
					GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					GTK_SORT_ASCENDING);

}


static GtkWidget *
wrap_in_frame (const gchar *label, 
               GtkWidget   *child)
{
  GtkWidget *frame, *alignment, *label_widget;
  gchar *bold_text;

  label_widget = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
  gtk_widget_show (label_widget);
  
  bold_text = g_markup_printf_escaped ("<b>%s</b>", label);
  gtk_label_set_markup (GTK_LABEL (label_widget), bold_text);
  g_free (bold_text);
  
  frame = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (frame), label_widget, FALSE, FALSE, 0);
  
  alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
			     0, 0, 12, 0);
  gtk_box_pack_start (GTK_BOX (frame), alignment, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (alignment), child);

  gtk_widget_show (frame);
  gtk_widget_show (alignment);
  
  return frame;
}

static gboolean
setup_option (GtkPrintUnixDialog     *dialog,
	      const gchar            *option_name,
	      GtkPrinterOptionWidget *widget)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrinterOption *option;

  option = gtk_printer_option_set_lookup (priv->options, option_name);
  gtk_printer_option_widget_set_source (widget, option);

  return option != NULL;
}

static void
add_option_to_extension_point (GtkPrinterOption *option,
		               gpointer          data)
{
  GtkWidget *extension_point = data;
  GtkWidget *widget;

  widget = gtk_printer_option_widget_new (option);
  gtk_widget_show (widget);
   
  if (gtk_printer_option_widget_has_external_label (GTK_PRINTER_OPTION_WIDGET (widget)))
    {
      GtkWidget *label, *hbox;
      
      label = gtk_printer_option_widget_get_external_label (GTK_PRINTER_OPTION_WIDGET (widget));
      gtk_widget_show (label);
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
      
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
      gtk_widget_show (hbox);
      
      gtk_box_pack_start (GTK_BOX (extension_point), hbox, FALSE, FALSE, 0);
    }
  else
    gtk_box_pack_start (GTK_BOX (extension_point), widget, FALSE, FALSE, 0);
}

static void
add_option_to_table (GtkPrinterOption *option,
		     gpointer          user_data)
{
  GtkTable *table;
  GtkWidget *label, *widget;
  gint row;

  table = GTK_TABLE (user_data);
  
  if (g_str_has_prefix (option->name, "gtk-"))
    return;
  
  widget = gtk_printer_option_widget_new (option);
  gtk_widget_show (widget);

  row = table->nrows;
  gtk_table_resize (table, table->nrows + 1, 2);
  
  if (gtk_printer_option_widget_has_external_label (GTK_PRINTER_OPTION_WIDGET (widget)))
    {
      label = gtk_printer_option_widget_get_external_label (GTK_PRINTER_OPTION_WIDGET (widget));
      gtk_widget_show (label);

      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

      gtk_table_attach (table, label,
			0, 1, row - 1 , row,  GTK_FILL, 0, 0, 0);
      
      gtk_table_attach (table, widget,
			1, 2, row - 1, row,  GTK_FILL, 0, 0, 0);
    }
  else
    gtk_table_attach (table, widget,
		      0, 2, row - 1, row,  GTK_FILL, 0, 0, 0);
}


static void
setup_page_table (GtkPrinterOptionSet *options,
		  const gchar         *group,
		  GtkWidget           *table,
		  GtkWidget           *page)
{
  gtk_printer_option_set_foreach_in_group (options, group,
					   add_option_to_table,
					   table);
  if (GTK_TABLE (table)->nrows == 1)
    gtk_widget_hide (page);
  else
    gtk_widget_show (page);
}

static void
update_print_at_option (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrinterOption *option;
  
  option = gtk_printer_option_set_lookup (priv->options, "gtk-print-time");

  if (option == NULL)
    return;
  
  if (priv->updating_print_at)
    return;
  
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->print_at_radio)))
    gtk_printer_option_set (option, "at");
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->print_hold_radio)))
    gtk_printer_option_set (option, "on-hold");
  else
    gtk_printer_option_set (option, "now");
  
  option = gtk_printer_option_set_lookup (priv->options, "gtk-print-time-text");
  if (option != NULL)
    {
      const char *text = gtk_entry_get_text (GTK_ENTRY (priv->print_at_entry));
      gtk_printer_option_set (option,text);
    }
}


static gboolean
setup_print_at (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrinterOption *option;
  
  option = gtk_printer_option_set_lookup (priv->options, "gtk-print-time");
 
  if (option == NULL)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->print_now_radio),
				    TRUE);
      gtk_widget_set_sensitive (priv->print_at_radio, FALSE);
      gtk_widget_set_sensitive (priv->print_at_entry, FALSE);
      gtk_widget_set_sensitive (priv->print_hold_radio, FALSE);
      gtk_entry_set_text (GTK_ENTRY (priv->print_at_entry), "");
      return FALSE;
    }

  priv->updating_print_at = TRUE;
  
  if (gtk_printer_option_has_choice (option, "at"))
    {
      gtk_widget_set_sensitive (priv->print_at_radio, TRUE);
      gtk_widget_set_sensitive (priv->print_at_entry, TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (priv->print_at_radio, FALSE);
      gtk_widget_set_sensitive (priv->print_at_entry, FALSE);
    }
  
  gtk_widget_set_sensitive (priv->print_hold_radio,
			    gtk_printer_option_has_choice (option, "on-hold"));

  update_print_at_option (dialog);

  if (strcmp (option->value, "at") == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->print_at_radio),
				  TRUE);
  else if (strcmp (option->value, "on-hold") == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->print_hold_radio),
				  TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->print_now_radio),
				  TRUE);

  option = gtk_printer_option_set_lookup (priv->options, "gtk-print-time-text");
  if (option != NULL)
    gtk_entry_set_text (GTK_ENTRY (priv->print_at_entry),
			option->value);
  

  priv->updating_print_at = FALSE;

  return TRUE;
}
	     
static void
update_dialog_from_settings (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GList *groups, *l;
  gchar *group;
  GtkWidget *table, *frame;
  gboolean has_advanced, has_job;
 
  if (priv->current_printer == NULL)
    {
       clear_per_printer_ui (dialog);
       gtk_widget_hide (priv->job_page);
       gtk_widget_hide (priv->advanced_page);
       gtk_widget_hide (priv->image_quality_page);
       gtk_widget_hide (priv->finishing_page);
       gtk_widget_hide (priv->color_page);
       gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

       return;
    }
 
  setup_option (dialog, "gtk-n-up", priv->pages_per_sheet);
  setup_option (dialog, "gtk-duplex", priv->duplex);
  setup_option (dialog, "gtk-paper-type", priv->paper_type);
  setup_option (dialog, "gtk-paper-source", priv->paper_source);
  setup_option (dialog, "gtk-output-tray", priv->output_tray);

  has_job = FALSE;
  has_job |= setup_option (dialog, "gtk-job-prio", priv->job_prio);
  has_job |= setup_option (dialog, "gtk-billing-info", priv->billing_info);
  has_job |= setup_option (dialog, "gtk-cover-before", priv->cover_before);
  has_job |= setup_option (dialog, "gtk-cover-after", priv->cover_after);
  has_job |= setup_print_at (dialog);
  
  if (has_job)
    gtk_widget_show (priv->job_page);
  else
    gtk_widget_hide (priv->job_page);

  
  setup_page_table (priv->options,
		    "ImageQualityPage",
		    priv->image_quality_table,
		    priv->image_quality_page);
  
  setup_page_table (priv->options,
		    "FinishingPage",
		    priv->finishing_table,
		    priv->finishing_page);

  setup_page_table (priv->options,
		    "ColorPage",
		    priv->color_table,
		    priv->color_page);

  gtk_printer_option_set_foreach_in_group (priv->options,
					   "GtkPrintDialogExtension",
					   add_option_to_extension_point,
					   priv->extension_point);

  /* Put the rest of the groups in the advanced page */
  groups = gtk_printer_option_set_get_groups (priv->options);

  has_advanced = FALSE;
  for (l = groups; l != NULL; l = l->next)
    {
      group = l->data;

      if (group == NULL)
	continue;
      
      if (strcmp (group, "ImageQualityPage") == 0 ||
	  strcmp (group, "ColorPage") == 0 ||
	  strcmp (group, "FinishingPage") == 0 ||
	  strcmp (group, "GtkPrintDialogExtension") == 0)
	continue;

      table = gtk_table_new (1, 2, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 6);
      gtk_table_set_col_spacings (GTK_TABLE (table), 12);
      
      gtk_printer_option_set_foreach_in_group (priv->options,
					       group,
					       add_option_to_table,
					       table);
      if (GTK_TABLE (table)->nrows == 1)
	gtk_widget_destroy (table);
      else
	{
	  has_advanced = TRUE;
	  frame = wrap_in_frame (group, table);
	  gtk_widget_show (table);
	  gtk_widget_show (frame);
	  
	  gtk_box_pack_start (GTK_BOX (priv->advanced_vbox),
			      frame, FALSE, FALSE, 0);
	}
    }

  if (has_advanced)
    gtk_widget_show (priv->advanced_page);
  else
    gtk_widget_hide (priv->advanced_page);

  
  g_list_foreach (groups, (GFunc) g_free, NULL);
  g_list_free (groups);
}

static void
update_dialog_from_capabilities (GtkPrintUnixDialog *dialog)
{
  GtkPrintCapabilities caps;
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  gboolean can_collate;
  const gchar *copies;

  copies = gtk_entry_get_text (GTK_ENTRY (priv->copies_spin));
  can_collate = (*copies != '\0' && atoi (copies) > 1);

  caps = priv->manual_capabilities | priv->printer_capabilities;

  gtk_widget_set_sensitive (priv->page_set_combo,
			    caps & GTK_PRINT_CAPABILITY_PAGE_SET);
  gtk_widget_set_sensitive (priv->copies_spin,
			    caps & GTK_PRINT_CAPABILITY_COPIES);
  gtk_widget_set_sensitive (priv->collate_check,
			    can_collate && 
			    (caps & GTK_PRINT_CAPABILITY_COLLATE));
  gtk_widget_set_sensitive (priv->reverse_check,
			    caps & GTK_PRINT_CAPABILITY_REVERSE);
  gtk_widget_set_sensitive (priv->scale_spin,
			    caps & GTK_PRINT_CAPABILITY_SCALE);

  if (caps & GTK_PRINT_CAPABILITY_PREVIEW)
    gtk_widget_show (priv->preview_button);
  else
    gtk_widget_hide (priv->preview_button);

  update_collate_icon (NULL, dialog);

  gtk_tree_model_filter_refilter (priv->printer_list_filter);
}

static void
mark_conflicts (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrinter *printer;
  gboolean have_conflict;

  have_conflict = FALSE;

  printer = priv->current_printer;

  if (printer)
    {

      g_signal_handler_block (priv->options,
			      priv->options_changed_handler);
      
      gtk_printer_option_set_clear_conflicts (priv->options);
      
      have_conflict = _gtk_printer_mark_conflicts (printer,
						   priv->options);
      
      g_signal_handler_unblock (priv->options,
				priv->options_changed_handler);
    }

  if (have_conflict)
    gtk_widget_show (priv->conflicts_widget);
  else
    gtk_widget_hide (priv->conflicts_widget);
}

static gboolean
mark_conflicts_callback (gpointer data)
{
  GtkPrintUnixDialog *dialog = data;
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  priv->mark_conflicts_id = 0;

  mark_conflicts (dialog);

  return FALSE;
}

static void
unschedule_idle_mark_conflicts (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  if (priv->mark_conflicts_id != 0)
    {
      g_source_remove (priv->mark_conflicts_id);
      priv->mark_conflicts_id = 0;
    }
}

static void
schedule_idle_mark_conflicts (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  if (priv->mark_conflicts_id != 0)
    return;

  priv->mark_conflicts_id = g_idle_add (mark_conflicts_callback,
					dialog);
}

static void
options_changed_cb (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  schedule_idle_mark_conflicts (dialog);

  if (priv->initial_settings)
    {
      g_object_unref (priv->initial_settings);
      priv->initial_settings = NULL;
    }

  g_free (priv->waiting_for_printer);
  priv->waiting_for_printer = NULL;
}

static void
remove_custom_widget (GtkWidget    *widget,
                      GtkContainer *container)
{
  gtk_container_remove (container, widget);
}

static void
extension_point_clear_children (GtkContainer *container)
{
  gtk_container_foreach (container,
                         (GtkCallback)remove_custom_widget,
                         container);
}

static void
clear_per_printer_ui (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  gtk_container_foreach (GTK_CONTAINER (priv->finishing_table),
			 (GtkCallback)gtk_widget_destroy,
			 NULL);
  gtk_table_resize (GTK_TABLE (priv->finishing_table), 1, 2);
  gtk_container_foreach (GTK_CONTAINER (priv->image_quality_table),
			 (GtkCallback)gtk_widget_destroy,
			 NULL);
  gtk_table_resize (GTK_TABLE (priv->image_quality_table), 1, 2);
  gtk_container_foreach (GTK_CONTAINER (priv->color_table),
			 (GtkCallback)gtk_widget_destroy,
			 NULL);
  gtk_table_resize (GTK_TABLE (priv->color_table), 1, 2);
  gtk_container_foreach (GTK_CONTAINER (priv->advanced_vbox),
			 (GtkCallback)gtk_widget_destroy,
			 NULL);
  extension_point_clear_children (GTK_CONTAINER (priv->extension_point));
}

static void
printer_details_acquired (GtkPrinter         *printer,
			  gboolean            success,
			  GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  priv->request_details_tag = 0;
  
  if (success)
    {
      GtkTreeSelection *selection;
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->printer_treeview));
      
      selected_printer_changed (selection, dialog);
    }
}

static void
selected_printer_changed (GtkTreeSelection   *selection,
			  GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrinter *printer;
  GtkTreeIter iter, filter_iter;

  /* Whenever the user selects a printer we stop looking for
     the printer specified in the initial settings */
  if (priv->waiting_for_printer &&
      !priv->internal_printer_change)
    {
      g_free (priv->waiting_for_printer);
      priv->waiting_for_printer = NULL;
    }
  
  if (priv->request_details_tag)
    {
      g_source_remove (priv->request_details_tag);
      priv->request_details_tag = 0;
    }
  
  printer = NULL;
  if (gtk_tree_selection_get_selected (selection, NULL, &filter_iter))
    {
      gtk_tree_model_filter_convert_iter_to_child_iter (priv->printer_list_filter,
							&iter,
							&filter_iter);

      gtk_tree_model_get (priv->printer_list, &iter,
  			  PRINTER_LIST_COL_PRINTER_OBJ, &printer,
			  -1);
    }
  
  if (printer != NULL && !_gtk_printer_has_details (printer))
    {
      gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
      priv->request_details_tag =
	g_signal_connect (printer, "details-acquired",
			  G_CALLBACK (printer_details_acquired), dialog);
      _gtk_printer_request_details (printer);
      g_object_unref (printer);
      return;
    }
  
  if (printer == priv->current_printer)
    {
      if (printer)
	g_object_unref (printer);
      return;
    }

  if (priv->options)
    {
      g_object_unref (priv->options);
      priv->options = NULL;  

      clear_per_printer_ui (dialog);
    }

  if (priv->current_printer)
    {
      g_object_unref (priv->current_printer);
    }

  priv->printer_capabilities = 0;
  
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
  priv->current_printer = printer;

  if (printer != NULL)
    {
      priv->printer_capabilities = _gtk_printer_get_capabilities (printer);
      priv->options = _gtk_printer_get_options (printer, 
						priv->initial_settings,
						priv->page_setup,
						priv->manual_capabilities);
  
      priv->options_changed_handler = 
        g_signal_connect_swapped (priv->options, "changed", G_CALLBACK (options_changed_cb), dialog);
    }

  update_dialog_from_settings (dialog);
  update_dialog_from_capabilities (dialog);
}

static void
update_collate_icon (GtkToggleButton    *toggle_button,
		     GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  gtk_widget_queue_draw (priv->collate_image);
}

static void
paint_page (GtkWidget *widget,
	    cairo_t   *cr, 
	    gfloat     scale,
	    gint       x_offset, 
	    gint       y_offset,
	    gchar     *text,
	    gint       text_x)
{
  gint x, y, width, height;
  gint text_y, linewidth;

  x = x_offset * scale;
  y = y_offset * scale;
  width = 20 * scale;
  height = 26 * scale;

  linewidth = 2;
  text_y = 21;

  gdk_cairo_set_source_color (cr, &widget->style->base[GTK_STATE_NORMAL]);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);

  gdk_cairo_set_source_color (cr, &widget->style->text[GTK_STATE_NORMAL]);
  cairo_set_line_width (cr, linewidth);
  cairo_rectangle (cr, x + linewidth/2.0, y + linewidth/2.0, width - linewidth, height - linewidth);
  cairo_stroke (cr);

  cairo_select_font_face (cr, "Sans", 
			  CAIRO_FONT_SLANT_NORMAL,
			  CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, (gint)(9 * scale));
  cairo_move_to (cr, x + (gint)(text_x * scale), y + (gint)(text_y * scale));
  cairo_show_text (cr, text);
}


static gboolean
draw_collate_cb (GtkWidget	    *widget,
		 GdkEventExpose     *event,
		 GtkPrintUnixDialog *dialog)
{
  GtkSettings *settings;
  cairo_t *cr;
  gint size;
  gfloat scale;
  gboolean collate, reverse, rtl;
  gint copies;
  gint text_x;

  collate = dialog_get_collate (dialog);
  reverse = dialog_get_reverse (dialog);
  copies = dialog_get_n_copies (dialog);

  rtl = (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL);

  settings = gtk_widget_get_settings (widget);
  gtk_icon_size_lookup_for_settings (settings,
				     GTK_ICON_SIZE_DIALOG,
				     &size,
				     NULL);
  scale = size / 48.0;
  text_x = rtl ? 4 : 11;

  cr = gdk_cairo_create (widget->window);

  if (copies == 1)
    {
      paint_page (widget, cr, scale, rtl ? 40: 15, 5, reverse ? "1" : "2", text_x);
      paint_page (widget, cr, scale, rtl ? 50: 5, 15, reverse ? "2" : "1", text_x);
    }
  else
    {
      paint_page (widget, cr, scale, rtl ? 40: 15, 5, collate == reverse ? "1" : "2", text_x);
      paint_page (widget, cr, scale, rtl ? 50: 5, 15, reverse ? "2" : "1", text_x);

      paint_page (widget, cr, scale, rtl ? 5 : 50, 5, reverse ? "1" : "2", text_x);
      paint_page (widget, cr, scale, rtl ? 15 : 40, 15, collate == reverse ? "2" : "1", text_x);
    }

  cairo_destroy (cr);

  return TRUE;
}

static void
gtk_print_unix_dialog_style_set (GtkWidget *widget,
				 GtkStyle  *previous_style)
{
  if (GTK_WIDGET_CLASS (gtk_print_unix_dialog_parent_class)->style_set)
    GTK_WIDGET_CLASS (gtk_print_unix_dialog_parent_class)->style_set (widget, previous_style);

  if (gtk_widget_has_screen (widget))
    {
      GtkPrintUnixDialog *dialog = (GtkPrintUnixDialog *)widget;
      GtkPrintUnixDialogPrivate *priv = dialog->priv;
      GtkSettings *settings;
      gint size;
      gfloat scale;
      
      settings = gtk_widget_get_settings (widget);
      gtk_icon_size_lookup_for_settings (settings,
					 GTK_ICON_SIZE_DIALOG,
					 &size,
					 NULL);
      scale = size / 48.0;
      
      gtk_widget_set_size_request (priv->collate_image, 
				   (50 + 20) * scale,
				   (15 + 26) * scale);
    }
}

static void
update_range_sensitivity (GtkWidget *button,
			  GtkWidget *range)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  gtk_widget_set_sensitive (range, active);
}

static void
create_main_page (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkWidget *main_vbox, *label, *vbox, *hbox;
  GtkWidget *scrolled, *treeview, *frame, *table;
  GtkWidget *entry, *spinbutton;
  GtkWidget *radio, *check, *image;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  GtkWidget *custom_input;

  main_vbox = gtk_vbox_new (FALSE, 18);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_widget_show (main_vbox);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
				       GTK_SHADOW_IN);
  gtk_widget_show (scrolled);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

  treeview = gtk_tree_view_new_with_model ((GtkTreeModel *) priv->printer_list_filter);
  priv->printer_treeview = treeview;
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview), PRINTER_LIST_COL_NAME);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (selection, "changed", G_CALLBACK (selected_printer_changed), dialog);
 
  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("",
						     renderer,
						     "icon-name",
						     PRINTER_LIST_COL_ICON,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Printer"),
						     renderer,
						     "text",
						     PRINTER_LIST_COL_NAME,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Location"),
						     renderer,
						     "text",
						     PRINTER_LIST_COL_LOCATION,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  column = gtk_tree_view_column_new_with_attributes (_("Status"),
						     renderer,
						     "text",
						     PRINTER_LIST_COL_STATE,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  
  gtk_widget_show (treeview);
  gtk_container_add (GTK_CONTAINER (scrolled), treeview);

  custom_input = gtk_hbox_new (FALSE, 18);
  gtk_widget_show (custom_input);
  gtk_box_pack_start (GTK_BOX (vbox), custom_input, FALSE, FALSE, 0);
  priv->extension_point = custom_input;

  hbox = gtk_hbox_new (FALSE, 18);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Print Pages"), table);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (table);

  radio = gtk_radio_button_new_with_mnemonic (NULL, _("_All"));
  priv->all_pages_radio = radio;
  gtk_widget_show (radio);
  gtk_table_attach (GTK_TABLE (table), radio,
		    0, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  radio = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio)),
					      _("C_urrent"));
  if (priv->current_page == -1)
    gtk_widget_set_sensitive (radio, FALSE);    
  priv->current_page_radio = radio;
  gtk_widget_show (radio);
  gtk_table_attach (GTK_TABLE (table), radio,
		    0, 2, 1, 2,  GTK_FILL, 0,
		    0, 0);
  radio = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio)),
					      _("Ra_nge: "));
  priv->page_range_radio = radio;
  gtk_widget_show (radio);
  gtk_table_attach (GTK_TABLE (table), radio,
		    0, 1, 2, 3,  GTK_FILL, 0,
		    0, 0);
  entry = gtk_entry_new ();
  priv->page_range_entry = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table), entry,
		    1, 2, 2, 3,  GTK_FILL, 0,
		    0, 0);
  g_signal_connect (radio, "toggled", G_CALLBACK (update_range_sensitivity), entry);
  update_range_sensitivity (radio, entry);

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Copies"), table);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (table);

  /* FIXME chpe: too much space between Copies and spinbutton, put those 2 in a hbox and make it span 2 columns */
  label = gtk_label_new_with_mnemonic (_("Copie_s:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1,  GTK_FILL, 0,
		    0, 0);
  spinbutton = gtk_spin_button_new_with_range (1.0, 100.0, 1.0);
  priv->copies_spin = spinbutton;
  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table), spinbutton,
		    1, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  g_signal_connect_swapped (spinbutton, "value-changed", 
			    G_CALLBACK (update_dialog_from_capabilities), dialog);
  g_signal_connect_swapped (spinbutton, "changed", 
			    G_CALLBACK (update_dialog_from_capabilities), dialog);
  
  check = gtk_check_button_new_with_mnemonic (_("C_ollate"));
  priv->collate_check = check;
  g_signal_connect (check, "toggled", G_CALLBACK (update_collate_icon), dialog);
  gtk_widget_show (check);
  gtk_table_attach (GTK_TABLE (table), check,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);

  check = gtk_check_button_new_with_mnemonic (_("_Reverse"));
  g_signal_connect (check, "toggled", G_CALLBACK (update_collate_icon), dialog);
  priv->reverse_check = check;
  gtk_widget_show (check);
  gtk_table_attach (GTK_TABLE (table), check,
		    0, 1, 2, 3,  GTK_FILL, 0,
		    0, 0);

  image = gtk_drawing_area_new ();
  priv->collate_image = image;
  gtk_widget_show (image);
  gtk_widget_set_size_request (image, 70, 90);
  gtk_table_attach (GTK_TABLE (table), image,
		    1, 2, 1, 3, GTK_FILL, 0,
		    0, 0);
  g_signal_connect (image, "expose-event", G_CALLBACK (draw_collate_cb), dialog);

  label = gtk_label_new (_("General"));
  gtk_widget_show (label);
  
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), main_vbox, label);
}

static gboolean
is_range_separator (gchar c)
{
  return (c == ',' || c == ';' || c == ':');
}

static GtkPageRange *
dialog_get_page_ranges (GtkPrintUnixDialog *dialog,
			gint               *n_ranges_out)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  gint i, n_ranges;
  const gchar *text, *p;
  gchar *next;
  GtkPageRange *ranges;
  gint start, end;
  
  text = gtk_entry_get_text (GTK_ENTRY (priv->page_range_entry));

  if (*text == 0)
    {
      *n_ranges_out = 0;
      return NULL;
    }
  
  n_ranges = 1;
  p = text;
  while (*p)
    {
      if (is_range_separator (*p))
	n_ranges++;
      p++;
    }

  ranges = g_new0 (GtkPageRange, n_ranges);
  
  i = 0;
  p = text;
  while (*p)
    {
      start = (int)strtol (p, &next, 10);
      if (start < 1)
	start = 1;
      end = start;

      if (next != p)
	{
	  p = next;

	  if (*p == '-')
	    {
	      p++;
	      end = (int)strtol (p, NULL, 10);
	      if (end < start)
		end = start;
	    }
	}

      ranges[i].start = start - 1;
      ranges[i].end = end - 1;
      i++;

      /* Skip until end or separator */
      while (*p && !is_range_separator (*p))
	p++;

      /* if not at end, skip separator */
      if (*p)
	p++;
    }

  *n_ranges_out = i;
  
  return ranges;
}

static void
dialog_set_page_ranges (GtkPrintUnixDialog *dialog,
			GtkPageRange       *ranges,
			gint                n_ranges)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  gint i;
  GString *s = g_string_new ("");

  for (i = 0; i < n_ranges; i++)
    {
      g_string_append_printf (s, "%d", ranges[i].start + 1);
      if (ranges[i].end > ranges[i].start)
	g_string_append_printf (s, "-%d", ranges[i].end + 1);
      
      if (i != n_ranges - 1)
	g_string_append (s, ",");
    }

  gtk_entry_set_text (GTK_ENTRY (priv->page_range_entry), s->str);
  
  g_string_free (s, TRUE);
}


static GtkPrintPages
dialog_get_print_pages (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_pages_radio)))
    return GTK_PRINT_PAGES_ALL;
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->current_page_radio)))
    return GTK_PRINT_PAGES_CURRENT;
  else
    return GTK_PRINT_PAGES_RANGES;
}

static void
dialog_set_print_pages (GtkPrintUnixDialog *dialog, 
			GtkPrintPages       pages)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  if (pages == GTK_PRINT_PAGES_RANGES)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->page_range_radio), TRUE);
  else if (pages == GTK_PRINT_PAGES_CURRENT)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->current_page_radio), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_pages_radio), TRUE);
}

static gdouble
dialog_get_scale (GtkPrintUnixDialog *dialog)
{
  if (GTK_WIDGET_IS_SENSITIVE (dialog->priv->scale_spin))
    return gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->priv->scale_spin));
  else
    return 100.0;
}

static void
dialog_set_scale (GtkPrintUnixDialog *dialog, 
                  gdouble             val)
{
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->scale_spin), val);
}

static GtkPageSet
dialog_get_page_set (GtkPrintUnixDialog *dialog)
{
  if (GTK_WIDGET_IS_SENSITIVE (dialog->priv->page_set_combo))
    return (GtkPageSet)gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->page_set_combo));
  else
    return GTK_PAGE_SET_ALL;
}

static void
dialog_set_page_set (GtkPrintUnixDialog *dialog, 
                     GtkPageSet          val)
{
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->page_set_combo),
			    (int)val);
}

static gint
dialog_get_n_copies (GtkPrintUnixDialog *dialog)
{
  if (GTK_WIDGET_IS_SENSITIVE (dialog->priv->copies_spin))
    return gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (dialog->priv->copies_spin));
  return 1;
}

static void
dialog_set_n_copies (GtkPrintUnixDialog *dialog, 
                     gint                n_copies)
{
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->copies_spin),
			     n_copies);
}

static gboolean
dialog_get_collate (GtkPrintUnixDialog *dialog)
{
  if (GTK_WIDGET_IS_SENSITIVE (dialog->priv->collate_check))
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->collate_check));
  return FALSE;
}

static void
dialog_set_collate (GtkPrintUnixDialog *dialog, 
                    gboolean            collate)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->collate_check),
				collate);
}

static gboolean
dialog_get_reverse (GtkPrintUnixDialog *dialog)
{
  if (GTK_WIDGET_IS_SENSITIVE (dialog->priv->reverse_check))
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->reverse_check));
  return FALSE;
}

static void
dialog_set_reverse (GtkPrintUnixDialog *dialog, 
                    gboolean            reverse)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->reverse_check),
				reverse);
}

static gint 
dialog_get_pages_per_sheet (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  const gchar *val;
  gint num;

  val = gtk_printer_option_widget_get_value (priv->pages_per_sheet);

  num = 1;
  
  if (val)
    {
      num = atoi(val);
      if (num < 1)
	num = 1;
    }
  
  return num;
}


static gboolean
draw_page_cb (GtkWidget	         *widget,
	      GdkEventExpose     *event,
	      GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  cairo_t *cr;
  gdouble ratio;
  gint w, h, tmp, shadow_offset;
  gint pages_x, pages_y, i, x, y, layout_w, layout_h;
  gdouble page_width, page_height;
  GtkPageOrientation orientation;
  gboolean landscape;
  PangoLayout *layout;
  PangoFontDescription *font;
  gchar *text;
  GdkColor *color;
  
  orientation = gtk_page_setup_get_orientation (priv->page_setup);
  landscape =
    (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE) ||
    (orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE);
  
  cr = gdk_cairo_create (widget->window);
  
  ratio = G_SQRT2;

  w = (EXAMPLE_PAGE_AREA_SIZE - 3) / ratio;
  h = w * ratio;

  switch (dialog_get_pages_per_sheet (dialog))
    {
    default:
    case 1:
      pages_x = 1; pages_y = 1;
      break;
    case 2:
      landscape = !landscape;
      pages_x = 1; pages_y = 2;
      break;
    case 4:
      pages_x = 2; pages_y = 2;
      break;
    case 6:
      landscape = !landscape;
      pages_x = 2; pages_y = 3;
      break;
    case 9:
      pages_x = 3; pages_y = 3;
      break;
    case 16:
      pages_x = 4; pages_y = 4;
      break;
    }

  if (landscape)
    {
      tmp = w;
      w = h;
      h = tmp;

      tmp = pages_x;
      pages_x = pages_y;
      pages_y = tmp;
    }
  
  shadow_offset = 3;
  
  color = &widget->style->text[GTK_STATE_NORMAL];
  cairo_set_source_rgba (cr, color->red / 65535., color->green / 65535., color->blue / 65535, 0.5);
  cairo_rectangle (cr, shadow_offset + 1, shadow_offset + 1, w, h);
  cairo_fill (cr);
  
  gdk_cairo_set_source_color (cr, &widget->style->base[GTK_STATE_NORMAL]);
  cairo_rectangle (cr, 1, 1, w, h);
  cairo_fill (cr);
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr, 0.5, 0.5, w+1, h+1);
  
  gdk_cairo_set_source_color (cr, &widget->style->text[GTK_STATE_NORMAL]);
  cairo_stroke (cr);

  i = 1;

  page_width = (double)w / pages_x;
  page_height = (double)h / pages_y;

  layout  = pango_cairo_create_layout (cr);

  font = pango_font_description_new ();
  pango_font_description_set_family (font, "sans");
  pango_font_description_set_absolute_size (font, page_height * 0.4 * PANGO_SCALE);
  pango_layout_set_font_description (layout, font);
  pango_font_description_free (font);

  pango_layout_set_width (layout, page_width * PANGO_SCALE);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  
  for (y = 0; y < pages_y; y++)
    {
      for (x = 0; x < pages_x; x++)
	{
	  text = g_strdup_printf ("%d", i++);
	  pango_layout_set_text (layout, text, -1);
	  g_free (text);
	  pango_layout_get_size (layout, &layout_w, &layout_h);
	  cairo_save (cr);
	  cairo_translate (cr,
			   x * page_width,
			   y * page_height + (page_height - layout_h / 1024.0) / 2
			   );
	  
	  pango_cairo_show_layout (cr, layout);
	  cairo_restore (cr);
	}
    }
    
  cairo_destroy (cr);

  return TRUE;
}

static void
redraw_page_layout_preview (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  if (priv->page_layout_preview)
    gtk_widget_queue_draw (priv->page_layout_preview);
}

static void
create_page_setup_page (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkWidget *main_vbox, *label, *hbox, *hbox2;
  GtkWidget *frame, *table, *widget;
  GtkWidget *combo, *spinbutton, *draw;

  main_vbox = gtk_vbox_new (FALSE, 18);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_widget_show (main_vbox);

  hbox = gtk_hbox_new (FALSE, 18);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

  table = gtk_table_new (5, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Layout"), table);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("Pages per _sheet:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  g_signal_connect_swapped (widget, "changed", G_CALLBACK (redraw_page_layout_preview), dialog);
  priv->pages_per_sheet = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new_with_mnemonic (_("T_wo-sided:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->duplex = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 1, 2,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new_with_mnemonic (_("_Only print:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 2, 3,  GTK_FILL, 0,
		    0, 0);

  combo = gtk_combo_box_new_text ();
  priv->page_set_combo = combo;
  gtk_widget_show (combo);
  gtk_table_attach (GTK_TABLE (table), combo,
		    1, 2, 2, 3,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  /* In enum order */
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("All sheets"));  
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Even sheets"));  
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Odd sheets"));  
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  label = gtk_label_new_with_mnemonic (_("Sc_ale:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 3, 4,  GTK_FILL, 0,
		    0, 0);

  hbox2 = gtk_hbox_new (FALSE, 6);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (table), hbox2,
		    1, 2, 3, 4,  GTK_FILL, 0,
		    0, 0);
  
  spinbutton = gtk_spin_button_new_with_range (1.0, 1000.0, 1.0);
  priv->scale_spin = spinbutton;
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinbutton), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinbutton), 100.0);
  gtk_box_pack_start (GTK_BOX (hbox2), spinbutton, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  gtk_widget_show (spinbutton);
  label = gtk_label_new ("%"); /* FIXMEchpe does there exist any language where % needs to be translated? */
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Paper"), table);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 6);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("Paper _type:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->paper_type = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new_with_mnemonic (_("Paper _source:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->paper_source = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 1, 2,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new_with_mnemonic (_("Output t_ray:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 2, 3,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->output_tray = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 2, 3,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  /* Add the page layout preview */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox2, TRUE, TRUE, 0);

  draw = gtk_drawing_area_new ();
  priv->page_layout_preview = draw;
  gtk_widget_set_size_request (draw, 200, 200);
  g_signal_connect (draw, "expose-event", G_CALLBACK (draw_page_cb), dialog);
  gtk_widget_show (draw);

  gtk_box_pack_start (GTK_BOX (hbox2), draw, TRUE, FALSE, 0);
  
  label = gtk_label_new (_("Page Setup"));
  gtk_widget_show (label);
  
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
			    main_vbox, label);
}

static void
create_job_page (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkWidget *main_table, *label;
  GtkWidget *frame, *table, *radio;
  GtkWidget *entry, *widget;

  main_table = gtk_table_new (2, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (main_table), 12);
  gtk_table_set_row_spacings (GTK_TABLE (main_table), 18);
  gtk_table_set_col_spacings (GTK_TABLE (main_table), 18);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Job Details"), table);
  gtk_table_attach (GTK_TABLE (main_table), frame,
		    0, 1, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("Pri_ority:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->job_prio = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new_with_mnemonic (_("_Billing info:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->billing_info = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 1, 2,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Print Document"), table);
  gtk_table_attach (GTK_TABLE (main_table), frame,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);
  gtk_widget_show (table);

  radio = gtk_radio_button_new_with_mnemonic (NULL, _("_Now"));
  priv->print_now_radio = radio;
  gtk_widget_show (radio);
  gtk_table_attach (GTK_TABLE (table), radio,
		    0, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  radio = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio)),
					      _("A_t:"));
  priv->print_at_radio = radio;
  gtk_widget_show (radio);
  gtk_table_attach (GTK_TABLE (table), radio,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);

  entry = gtk_entry_new ();
  priv->print_at_entry = entry;
  gtk_widget_show (entry);
  gtk_table_attach (GTK_TABLE (table), entry,
		    1, 2, 1, 2,  GTK_FILL, 0,
		    0, 0);

  radio = gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio)),
					      _("On _hold"));
  priv->print_hold_radio = radio;
  gtk_widget_show (radio);
  gtk_table_attach (GTK_TABLE (table), radio,
		    0, 2, 2, 3,  GTK_FILL, 0,
		    0, 0);

  g_signal_connect_swapped (priv->print_now_radio, "toggled",
			    G_CALLBACK (update_print_at_option), dialog);
  g_signal_connect_swapped (priv->print_at_radio, "toggled",
			    G_CALLBACK (update_print_at_option), dialog);
  g_signal_connect_swapped (priv->print_at_entry, "changed",
			    G_CALLBACK (update_print_at_option), dialog);
  g_signal_connect_swapped (priv->print_hold_radio, "toggled",
			    G_CALLBACK (update_print_at_option), dialog);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  frame = wrap_in_frame (_("Add Cover Page"), table);
  gtk_table_attach (GTK_TABLE (main_table), frame,
		    1, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("Be_fore:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->cover_before = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 0, 1,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new_with_mnemonic (_("_After:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 1, 2,  GTK_FILL, 0,
		    0, 0);

  widget = gtk_printer_option_widget_new (NULL);
  priv->cover_after = GTK_PRINTER_OPTION_WIDGET (widget);
  gtk_widget_show (widget);
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 1, 2,  GTK_FILL, 0,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

  label = gtk_label_new (_("Job"));
  gtk_widget_show (label);

  priv->job_page = main_table;
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
			    main_table, label);
}

static void 
create_optional_page (GtkPrintUnixDialog  *dialog,
		      const gchar         *text,
		      GtkWidget          **table_out,
		      GtkWidget          **page_out)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkWidget *table, *label, *scrolled;

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);
  
  table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_widget_show (table);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					 table);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN(scrolled)->child),
				GTK_SHADOW_NONE);
  
  label = gtk_label_new (text);
  gtk_widget_show (label);
  
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
			    scrolled, label);

  *table_out = table;
  *page_out = scrolled;
}

static void
create_advanced_page (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkWidget *main_vbox, *label, *scrolled;

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  priv->advanced_page = scrolled;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);

  main_vbox = gtk_vbox_new (FALSE, 18);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_widget_show (main_vbox);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					 main_vbox);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN(scrolled)->child),
				GTK_SHADOW_NONE);
  
  priv->advanced_vbox = main_vbox;
  
  label = gtk_label_new (_("Advanced"));
  gtk_widget_show (label);
  
  gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
			    scrolled, label);
}


static void
populate_dialog (GtkPrintUnixDialog *print_dialog)
{
  GtkPrintUnixDialogPrivate *priv = print_dialog->priv;
  GtkDialog *dialog = GTK_DIALOG (print_dialog);
  GtkWidget *vbox, *conflict_hbox, *image, *label;

  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  priv->notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbox), priv->notebook, TRUE, TRUE, 0);
  gtk_widget_show (priv->notebook);

  create_printer_list_model (print_dialog);

  create_main_page (print_dialog);
  create_page_setup_page (print_dialog);
  create_job_page (print_dialog);
  create_optional_page (print_dialog, _("Image Quality"),
			&priv->image_quality_table,
			&priv->image_quality_page);
  create_optional_page (print_dialog, _("Color"),
			&priv->color_table,
			&priv->color_page);
  create_optional_page (print_dialog, _("Finishing"),
			&priv->finishing_table,
			&priv->finishing_page);
  create_advanced_page (print_dialog);

  priv->conflicts_widget = conflict_hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_end (GTK_BOX (vbox), conflict_hbox, FALSE, FALSE, 0);
  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (conflict_hbox), image, FALSE, TRUE, 0);
  label = gtk_label_new (_("Some of the settings in the dialog conflict"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (conflict_hbox), label, FALSE, TRUE, 0);
  
  load_print_backends (print_dialog);
}

/**
 * gtk_print_unix_dialog_new:
 * @title: Title of the dialog, or %NULL
 * @parent: Transient parent of the dialog, or %NULL
 *
 * Creates a new #GtkPrintUnixDialog.
 *
 * Return value: a new #GtkPrintUnixDialog
 *
 * Since: 2.10
 **/
GtkWidget *
gtk_print_unix_dialog_new (const gchar *title,
			   GtkWindow   *parent)
{
  GtkWidget *result;
  const gchar *_title = _("Print");

  if (title)
    _title = title;

  result = g_object_new (GTK_TYPE_PRINT_UNIX_DIALOG,
			 "transient-for", parent,
                         "title", _title,
			 "has-separator", FALSE,
                         NULL);
  
  return result;
}

/**
 * gtk_print_unix_dialog_get_selected_printer:
 * @dialog: a #GtkPrintUnixDialog
 * 
 * Gets the currently selected printer.
 * 
 * Returns: the currently selected printer
 * 
 * Since: 2.10
 */
GtkPrinter *
gtk_print_unix_dialog_get_selected_printer (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), NULL);

  return dialog->priv->current_printer;
}

/**
 * gtk_print_unix_dialog_set_page_setup:
 * @dialog: a #GtkPrintUnixDialog
 * @page_setup: a #GtkPageSetup
 * 
 * Sets the page setup of the #GtkPrintUnixDialog.
 *
 * Since: 2.10
 */
void
gtk_print_unix_dialog_set_page_setup (GtkPrintUnixDialog *dialog,
				      GtkPageSetup       *page_setup)
{
  GtkPrintUnixDialogPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));
  g_return_if_fail (GTK_IS_PAGE_SETUP (page_setup));

  priv = dialog->priv;

  if (priv->page_setup != page_setup)
    {
      g_object_unref (priv->page_setup);
      priv->page_setup = g_object_ref (page_setup);

      g_object_notify (G_OBJECT (dialog), "page-setup");
    }
}

/**
 * gtk_print_unix_dialog_get_page_setup:
 * @dialog: a #GtkPrintUnixDialog
 * 
 * Gets the page setup that is used by the #GtkPrintUnixDialog.
 * 
 * Returns: the page setup of @dialog.
 *
 * Since: 2.10
 */
GtkPageSetup *
gtk_print_unix_dialog_get_page_setup (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), NULL);

  return dialog->priv->page_setup;
}

/**
 * gtk_print_unix_dialog_set_current_page:
 * @dialog: a #GtkPrintUnixDialog
 * @current_page: the current page number.
 * 
 * Sets the current page number. If @current_page is not -1, this enables
 * the current page choice for the range of pages to print.
 *
 * Since: 2.10
 */
void
gtk_print_unix_dialog_set_current_page (GtkPrintUnixDialog *dialog,
					gint                current_page)
{
  GtkPrintUnixDialogPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  priv = dialog->priv;

  if (priv->current_page != current_page)
    {
      priv->current_page = current_page;

      if (priv->current_page_radio)
	gtk_widget_set_sensitive (priv->current_page_radio, current_page != -1);

      g_object_notify (G_OBJECT (dialog), "current-page");
    }
}

/**
 * gtk_print_unix_dialog_get_current_page:
 * @dialog: a #GtkPrintUnixDialog
 * 
 * Gets the current page of the #GtkPrintDialog.
 * 
 * Returns: the current page of @dialog
 * 
 * Since: 2.10
 */
gint
gtk_print_unix_dialog_get_current_page (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), -1);

  return dialog->priv->current_page;
}

static gboolean
set_active_printer (GtkPrintUnixDialog *dialog,
		    const gchar        *printer_name)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeModel *model;
  GtkTreeIter iter, filter_iter;
  GtkTreeSelection *selection;
  GtkPrinter *printer;

  model = GTK_TREE_MODEL (priv->printer_list);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
	{
	  gtk_tree_model_get (GTK_TREE_MODEL (priv->printer_list), &iter,
			      PRINTER_LIST_COL_PRINTER_OBJ, &printer, -1);
	  if (printer == NULL)
	    continue;
	  
	  if (strcmp (gtk_printer_get_name (printer), printer_name) == 0)
	    {
	      gtk_tree_model_filter_convert_child_iter_to_iter (priv->printer_list_filter,
								&filter_iter, &iter);
	      
	      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->printer_treeview));
	      priv->internal_printer_change = TRUE;
	      gtk_tree_selection_select_iter (selection, &filter_iter);
	      priv->internal_printer_change = FALSE;
	      g_free (priv->waiting_for_printer);
	      priv->waiting_for_printer = NULL;
	      
	      g_object_unref (printer);
	      return TRUE;
	    }
	      
	  g_object_unref (printer);
	  
	} while (gtk_tree_model_iter_next (model, &iter));
    }
  
  return FALSE;
}

/**
 * gtk_print_unix_dialog_set_settings:
 * @dialog: a #GtkPrintUnixDialog
 * @settings: a #GtkPrintSettings, or %NULL
 * 
 * Sets the #GtkPrintSettings for the #GtkPrintUnixDialog. Typically,
 * this is used to restore saved print settings from a previous print
 * operation before the print dialog is shown.
 * 
 * Since: 2.10
 **/
void
gtk_print_unix_dialog_set_settings (GtkPrintUnixDialog *dialog,
				    GtkPrintSettings   *settings)
{
  GtkPrintUnixDialogPrivate *priv;
  const gchar *printer;
  GtkPageRange *ranges;
  gint num_ranges;
  
  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));
  g_return_if_fail (settings == NULL || GTK_IS_PRINT_SETTINGS (settings));

  priv = dialog->priv;

  if (settings != NULL)
    {
      dialog_set_collate (dialog, gtk_print_settings_get_collate (settings));
      dialog_set_reverse (dialog, gtk_print_settings_get_reverse (settings));
      dialog_set_n_copies (dialog, gtk_print_settings_get_n_copies (settings));
      dialog_set_scale (dialog, gtk_print_settings_get_scale (settings));
      dialog_set_page_set (dialog, gtk_print_settings_get_page_set (settings));
      dialog_set_print_pages (dialog, gtk_print_settings_get_print_pages (settings));
      ranges = gtk_print_settings_get_page_ranges (settings, &num_ranges);
      if (ranges)
        {
	  dialog_set_page_ranges (dialog, ranges, num_ranges);
          g_free (ranges);
        }

      priv->format_for_printer =
	g_strdup (gtk_print_settings_get (settings, "format-for-printer"));
    }

  if (priv->initial_settings)
    g_object_unref (priv->initial_settings);

  priv->initial_settings = settings;

  g_free (priv->waiting_for_printer);
  priv->waiting_for_printer = NULL;
  
  if (settings)
    {
      g_object_ref (settings);

      printer = gtk_print_settings_get_printer (settings);
      
      if (printer && !set_active_printer (dialog, printer))
	priv->waiting_for_printer = g_strdup (printer); 
    }

  g_object_notify (G_OBJECT (dialog), "print-settings");
}

/**
 * gtk_print_unix_dialog_get_settings:
 * @dialog: a #GtkPrintUnixDialog
 * 
 * Gets a new #GtkPrintSettings object that represents the
 * current values in the print dialog. Note that this creates a
 * <emphasis>new object</emphasis>, and you need to unref it
 * if don't want to keep it.
 * 
 * Returns: a new #GtkPrintSettings object with the values from @dialog
 *
 * Since: 2.10
 */
GtkPrintSettings *
gtk_print_unix_dialog_get_settings (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv;
  GtkPrintSettings *settings;
  GtkPrintPages print_pages;
  GtkPageRange *ranges;
  gint n_ranges;

  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), NULL);

  priv = dialog->priv;
  settings = gtk_print_settings_new ();

  if (priv->current_printer)
    gtk_print_settings_set_printer (settings,
				    gtk_printer_get_name (priv->current_printer));
  else
    gtk_print_settings_set_printer (settings, "default");
  
  gtk_print_settings_set (settings, "format-for-printer",
			  priv->format_for_printer);

  
  gtk_print_settings_set_collate (settings,
				  dialog_get_collate (dialog));
  
  gtk_print_settings_set_reverse (settings,
				  dialog_get_reverse (dialog));
  
  gtk_print_settings_set_n_copies (settings,
				   dialog_get_n_copies (dialog));

  gtk_print_settings_set_scale (settings,
				dialog_get_scale (dialog));
  
  gtk_print_settings_set_page_set (settings,
				   dialog_get_page_set (dialog));
  
  print_pages = dialog_get_print_pages (dialog);
  gtk_print_settings_set_print_pages (settings, print_pages);

  ranges = dialog_get_page_ranges (dialog, &n_ranges);
  if (ranges)
    {
      gtk_print_settings_set_page_ranges  (settings, ranges, n_ranges);
      g_free (ranges);
    }

  /* TODO: print when. How to handle? */

  if (priv->current_printer)
    _gtk_printer_get_settings_from_options (priv->current_printer,
					    priv->options,
					    settings);
  
  return settings;
}

/**
 * gtk_print_unix_dialog_add_custom_tab:
 * @dialog: a #GtkPrintUnixDialog
 * @child: the widget to put in the custom tab
 * @tab_label: the widget to use as tab label
 *
 * Adds a custom tab to the print dialog.
 *
 * Since: 2.10
 */
void
gtk_print_unix_dialog_add_custom_tab (GtkPrintUnixDialog *dialog,
				      GtkWidget          *child,
				      GtkWidget          *tab_label)
{
  gtk_notebook_insert_page (GTK_NOTEBOOK (dialog->priv->notebook),
			    child, tab_label, 2);
  gtk_widget_show (child);
  gtk_widget_show (tab_label);
}

/**
 * gtk_print_unix_dialog_set_manual_capabilities:
 * @dialog: a #GtkPrintUnixDialog
 * @capabilities: the printing capabilities of your application
 *
 * This lets you specify the printing capabilities your application
 * supports. For instance, if you can handle scaling the output then
 * you pass #GTK_PRINT_CAPABILITY_SCALE. If you don't pass that, then
 * the dialog will only let you select the scale if the printing
 * system automatically handles scaling.
 *
 * Since: 2.10
 */
void
gtk_print_unix_dialog_set_manual_capabilities (GtkPrintUnixDialog   *dialog,
					       GtkPrintCapabilities  capabilities)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  priv->manual_capabilities = capabilities;
  update_dialog_from_capabilities (dialog);

  if (priv->current_printer)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->printer_treeview));

      g_object_unref (priv->current_printer);
      priv->current_printer = NULL;
      priv->internal_printer_change = TRUE;
      selected_printer_changed (selection, dialog);
      priv->internal_printer_change = FALSE;
   }
}

#define __GTK_PRINT_UNIX_DIALOG_C__
#include "gtkaliasdef.c"

