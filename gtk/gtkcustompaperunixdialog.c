/* GtkCustomPaperUnixDialog
 * Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 * Copyright © 2006, 2007, 2008 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include "config.h"
#include <string.h>
#include <locale.h>

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkliststore.h"

#include "gtktreeviewcolumn.h"
#include "gtklabel.h"
#include "gtkspinbutton.h"

#include "gtkcustompaperunixdialog.h"
#include "gtkprintbackend.h"
#include "gtkprintutils.h"
#include "gtkalias.h"

#define CUSTOM_PAPER_FILENAME ".gtk-custom-papers"


typedef struct
{
  GtkUnit    display_unit;
  GtkWidget *spin_button;
} UnitWidget;

struct GtkCustomPaperUnixDialogPrivate
{

  GtkWidget *treeview;
  GtkWidget *values_box;
  GtkWidget *printer_combo;
  GtkWidget *width_widget;
  GtkWidget *height_widget;
  GtkWidget *top_widget;
  GtkWidget *bottom_widget;
  GtkWidget *left_widget;
  GtkWidget *right_widget;

  GtkTreeViewColumn *text_column;

  gulong printer_inserted_tag;
  gulong printer_removed_tag;

  guint request_details_tag;
  GtkPrinter *request_details_printer;

  guint non_user_change : 1;

  GtkListStore *custom_paper_list;
  GtkListStore *printer_list;

  GList *print_backends;

  gchar *waiting_for_printer;
};

enum {
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_PRINTER,
  PRINTER_LIST_N_COLS
};

G_DEFINE_TYPE (GtkCustomPaperUnixDialog, gtk_custom_paper_unix_dialog, GTK_TYPE_DIALOG)

#define GTK_CUSTOM_PAPER_UNIX_DIALOG_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG, GtkCustomPaperUnixDialogPrivate))

static void gtk_custom_paper_unix_dialog_finalize  (GObject                *object);
static void populate_dialog                        (GtkCustomPaperUnixDialog *dialog);
static void printer_added_cb                       (GtkPrintBackend        *backend,
						    GtkPrinter             *printer,
						    GtkCustomPaperUnixDialog *dialog);
static void printer_removed_cb                     (GtkPrintBackend        *backend,
						    GtkPrinter             *printer,
						    GtkCustomPaperUnixDialog *dialog);
static void printer_status_cb                      (GtkPrintBackend        *backend,
						    GtkPrinter             *printer,
						    GtkCustomPaperUnixDialog *dialog);



GtkUnit
_gtk_print_get_default_user_units (void)
{
  /* Translate to the default units to use for presenting
   * lengths to the user. Translate to default:inch if you
   * want inches, otherwise translate to default:mm.
   * Do *not* translate it to "predefinito:mm", if it
   * it isn't default:mm or default:inch it will not work
   */
  gchar *e = _("default:mm");

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
  gchar *imperial = NULL;

  imperial = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
  if (imperial && imperial[0] == 2 )
    return GTK_UNIT_INCH;  /* imperial */
  if (imperial && imperial[0] == 1 )
    return GTK_UNIT_MM;  /* metric */
#endif

  if (strcmp (e, "default:inch")==0)
    return GTK_UNIT_INCH;
  else if (strcmp (e, "default:mm"))
    g_warning ("Whoever translated default:mm did so wrongly.\n");
  return GTK_UNIT_MM;
}

static char *
custom_paper_get_filename (void)
{
  gchar *filename;

  filename = g_build_filename (g_get_home_dir (),
			       CUSTOM_PAPER_FILENAME, NULL);
  g_assert (filename != NULL);
  return filename;
}

GList *
_gtk_load_custom_papers (void)
{
  GKeyFile *keyfile;
  gchar *filename;
  gchar **groups;
  gsize n_groups, i;
  gboolean load_ok;
  GList *result = NULL;

  filename = custom_paper_get_filename ();

  keyfile = g_key_file_new ();
  load_ok = g_key_file_load_from_file (keyfile, filename, 0, NULL);
  g_free (filename);
  if (!load_ok)
    {
      g_key_file_free (keyfile);
      return NULL;
    }

  groups = g_key_file_get_groups (keyfile, &n_groups);
  for (i = 0; i < n_groups; ++i)
    {
      GtkPageSetup *page_setup;

      page_setup = gtk_page_setup_new_from_key_file (keyfile, groups[i], NULL);
      if (!page_setup)
        continue;

      result = g_list_prepend (result, page_setup);
    }

  g_strfreev (groups);
  g_key_file_free (keyfile);

  return g_list_reverse (result);
}

void
_gtk_print_load_custom_papers (GtkListStore *store)
{
  GtkTreeIter iter;
  GList *papers, *p;
  GtkPageSetup *page_setup;

  gtk_list_store_clear (store);

  papers = _gtk_load_custom_papers ();
  for (p = papers; p; p = p->next)
    {
      page_setup = p->data;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
			  0, page_setup,
			  -1);
      g_object_unref (page_setup);
    }

  g_list_free (papers);
}

void
_gtk_print_save_custom_papers (GtkListStore *store)
{
  GtkTreeModel *model = GTK_TREE_MODEL (store);
  GtkTreeIter iter;
  GKeyFile *keyfile;
  gchar *filename, *data;
  gsize len;
  gint i = 0;

  keyfile = g_key_file_new ();

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
	{
	  GtkPageSetup *page_setup;
	  gchar group[32];

	  g_snprintf (group, sizeof (group), "Paper%u", i);

	  gtk_tree_model_get (model, &iter, 0, &page_setup, -1);

	  gtk_page_setup_to_key_file (page_setup, keyfile, group);

	  ++i;
	} while (gtk_tree_model_iter_next (model, &iter));
    }

  filename = custom_paper_get_filename ();
  data = g_key_file_to_data (keyfile, &len, NULL);
  g_file_set_contents (filename, data, len, NULL);
  g_free (data);
  g_free (filename);
}

static void
gtk_custom_paper_unix_dialog_class_init (GtkCustomPaperUnixDialogClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  object_class->finalize = gtk_custom_paper_unix_dialog_finalize;

  g_type_class_add_private (class, sizeof (GtkCustomPaperUnixDialogPrivate));
}

static void
custom_paper_dialog_response_cb (GtkDialog *dialog,
				 gint       response,
				 gpointer   user_data)
{
  GtkCustomPaperUnixDialogPrivate *priv = GTK_CUSTOM_PAPER_UNIX_DIALOG (dialog)->priv;

  _gtk_print_save_custom_papers (priv->custom_paper_list);
}

static void
gtk_custom_paper_unix_dialog_init (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv;
  GtkTreeIter iter;

  priv = dialog->priv = GTK_CUSTOM_PAPER_UNIX_DIALOG_GET_PRIVATE (dialog);

  priv->print_backends = NULL;

  priv->request_details_printer = NULL;
  priv->request_details_tag = 0;

  priv->printer_list = gtk_list_store_new (PRINTER_LIST_N_COLS,
					   G_TYPE_STRING,
					   G_TYPE_OBJECT);

  gtk_list_store_append (priv->printer_list, &iter);

  priv->custom_paper_list = gtk_list_store_new (1, G_TYPE_OBJECT);
  _gtk_print_load_custom_papers (priv->custom_paper_list);

  populate_dialog (dialog);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

  g_signal_connect (dialog, "response", G_CALLBACK (custom_paper_dialog_response_cb), NULL);
}

static void
gtk_custom_paper_unix_dialog_finalize (GObject *object)
{
  GtkCustomPaperUnixDialog *dialog = GTK_CUSTOM_PAPER_UNIX_DIALOG (object);
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkPrintBackend *backend;
  GList *node;

  if (priv->printer_list)
    {
      g_signal_handler_disconnect (priv->printer_list, priv->printer_inserted_tag);
      g_signal_handler_disconnect (priv->printer_list, priv->printer_removed_tag);
      g_object_unref (priv->printer_list);
      priv->printer_list = NULL;
    }

  if (priv->request_details_tag)
    {
      g_signal_handler_disconnect (priv->request_details_printer,
				   priv->request_details_tag);
      g_object_unref (priv->request_details_printer);
      priv->request_details_printer = NULL;
      priv->request_details_tag = 0;
    }

  if (priv->custom_paper_list)
    {
      g_object_unref (priv->custom_paper_list);
      priv->custom_paper_list = NULL;
    }

  g_free (priv->waiting_for_printer);
  priv->waiting_for_printer = NULL;

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

  G_OBJECT_CLASS (gtk_custom_paper_unix_dialog_parent_class)->finalize (object);
}

/**
 * gtk_custom_paper_unix_dialog_new:
 * @title: (allow-none): the title of the dialog, or %NULL
 * @parent: (allow-none): transient parent of the dialog, or %NULL
 *
 * Creates a new custom paper dialog.
 *
 * Returns: the new #GtkCustomPaperUnixDialog
 *
 * Since: 2.18
 */
GtkWidget *
_gtk_custom_paper_unix_dialog_new (GtkWindow   *parent,
				  const gchar *title)
{
  GtkWidget *result;

  if (title == NULL)
    title = _("Manage Custom Sizes");

  result = g_object_new (GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG,
                         "title", title,
                         "transient-for", parent,
                         "modal", parent != NULL,
                         "destroy-with-parent", TRUE,
                         NULL);

  return result;
}

static void
printer_added_cb (GtkPrintBackend          *backend,
		  GtkPrinter               *printer,
		  GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter;
  gchar *str;

  if (gtk_printer_is_virtual (printer))
    return;

  str = g_strdup_printf ("<b>%s</b>",
			 gtk_printer_get_name (printer));

  gtk_list_store_append (priv->printer_list, &iter);
  gtk_list_store_set (priv->printer_list, &iter,
                      PRINTER_LIST_COL_NAME, str,
                      PRINTER_LIST_COL_PRINTER, printer,
                      -1);

  g_object_set_data_full (G_OBJECT (printer),
			  "gtk-print-tree-iter",
                          gtk_tree_iter_copy (&iter),
                          (GDestroyNotify) gtk_tree_iter_free);

  g_free (str);

  if (priv->waiting_for_printer != NULL &&
      strcmp (priv->waiting_for_printer,
	      gtk_printer_get_name (printer)) == 0)
    {
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->printer_combo),
				     &iter);
      priv->waiting_for_printer = NULL;
    }
}

static void
printer_removed_cb (GtkPrintBackend        *backend,
		    GtkPrinter             *printer,
		    GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");
  gtk_list_store_remove (GTK_LIST_STORE (priv->printer_list), iter);
}


static void
printer_status_cb (GtkPrintBackend        *backend,
                   GtkPrinter             *printer,
		   GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;
  gchar *str;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");

  str = g_strdup_printf ("<b>%s</b>",
			 gtk_printer_get_name (printer));
  gtk_list_store_set (priv->printer_list, iter,
                      PRINTER_LIST_COL_NAME, str,
                      -1);
  g_free (str);
}

static void
printer_list_initialize (GtkCustomPaperUnixDialog *dialog,
			 GtkPrintBackend        *print_backend)
{
  GList *list, *node;

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
load_print_backends (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GList *node;

  if (g_module_supported ())
    priv->print_backends = gtk_print_backend_load_modules ();

  for (node = priv->print_backends; node != NULL; node = node->next)
    printer_list_initialize (dialog, GTK_PRINT_BACKEND (node->data));
}

static void unit_widget_changed (GtkCustomPaperUnixDialog *dialog);

static GtkWidget *
new_unit_widget (GtkCustomPaperUnixDialog *dialog,
		 GtkUnit                   unit,
		 GtkWidget                *mnemonic_label)
{
  GtkWidget *hbox, *button, *label;
  UnitWidget *data;

  data = g_new0 (UnitWidget, 1);
  data->display_unit = unit;

  hbox = gtk_hbox_new (FALSE, 6);

  button = gtk_spin_button_new_with_range (0.0, 9999.0, 1);
  if (unit == GTK_UNIT_INCH)
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (button), 2);
  else
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (button), 1);

  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  data->spin_button = button;

  g_signal_connect_swapped (button, "value-changed",
			    G_CALLBACK (unit_widget_changed), dialog);

  if (unit == GTK_UNIT_INCH)
    label = gtk_label_new (_("inch"));
  else
    label = gtk_label_new (_("mm"));

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (mnemonic_label), button);

  g_object_set_data_full (G_OBJECT (hbox), "unit-data", data, g_free);

  return hbox;
}

static double
unit_widget_get (GtkWidget *unit_widget)
{
  UnitWidget *data = g_object_get_data (G_OBJECT (unit_widget), "unit-data");
  return _gtk_print_convert_to_mm (gtk_spin_button_get_value (GTK_SPIN_BUTTON (data->spin_button)),
				   data->display_unit);
}

static void
unit_widget_set (GtkWidget *unit_widget,
		 gdouble    value)
{
  UnitWidget *data;

  data = g_object_get_data (G_OBJECT (unit_widget), "unit-data");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (data->spin_button),
			     _gtk_print_convert_from_mm (value, data->display_unit));
}

static void
custom_paper_printer_data_func (GtkCellLayout   *cell_layout,
				GtkCellRenderer *cell,
				GtkTreeModel    *tree_model,
				GtkTreeIter     *iter,
				gpointer         data)
{
  GtkPrinter *printer;

  gtk_tree_model_get (tree_model, iter,
		      PRINTER_LIST_COL_PRINTER, &printer, -1);

  if (printer)
    g_object_set (cell, "text",  gtk_printer_get_name (printer), NULL);
  else
    g_object_set (cell, "text",  _("Margins from Printer..."), NULL);

  if (printer)
    g_object_unref (printer);
}

static void
update_combo_sensitivity_from_printers (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter;
  gboolean sensitive;
  GtkTreeSelection *selection;
  GtkTreeModel *model;

  sensitive = FALSE;
  model = GTK_TREE_MODEL (priv->printer_list);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  if (gtk_tree_model_get_iter_first (model, &iter) &&
      gtk_tree_model_iter_next (model, &iter) &&
      gtk_tree_selection_get_selected (selection, NULL, &iter))
    sensitive = TRUE;

  gtk_widget_set_sensitive (priv->printer_combo, sensitive);
}

static void
update_custom_widgets_from_list (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkPageSetup *page_setup;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));

  priv->non_user_change = TRUE;
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gtk_tree_model_get (model, &iter, 0, &page_setup, -1);

      unit_widget_set (priv->width_widget,
		       gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM));
      unit_widget_set (priv->height_widget,
		       gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM));
      unit_widget_set (priv->top_widget,
		       gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
      unit_widget_set (priv->bottom_widget,
		       gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
      unit_widget_set (priv->left_widget,
		       gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));
      unit_widget_set (priv->right_widget,
		       gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));

      gtk_widget_set_sensitive (priv->values_box, TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (priv->values_box, FALSE);
    }

  if (priv->printer_list)
    update_combo_sensitivity_from_printers (dialog);
  priv->non_user_change = FALSE;
}

static void
selected_custom_paper_changed (GtkTreeSelection         *selection,
			       GtkCustomPaperUnixDialog *dialog)
{
  update_custom_widgets_from_list (dialog);
}

static void
unit_widget_changed (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  gdouble w, h, top, bottom, left, right;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;

  if (priv->non_user_change)
    return;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (priv->custom_paper_list), &iter, 0, &page_setup, -1);

      w = unit_widget_get (priv->width_widget);
      h = unit_widget_get (priv->height_widget);

      paper_size = gtk_page_setup_get_paper_size (page_setup);
      gtk_paper_size_set_size (paper_size, w, h, GTK_UNIT_MM);

      top = unit_widget_get (priv->top_widget);
      bottom = unit_widget_get (priv->bottom_widget);
      left = unit_widget_get (priv->left_widget);
      right = unit_widget_get (priv->right_widget);

      gtk_page_setup_set_top_margin (page_setup, top, GTK_UNIT_MM);
      gtk_page_setup_set_bottom_margin (page_setup, bottom, GTK_UNIT_MM);
      gtk_page_setup_set_left_margin (page_setup, left, GTK_UNIT_MM);
      gtk_page_setup_set_right_margin (page_setup, right, GTK_UNIT_MM);

      g_object_unref (page_setup);
    }
}

static gboolean
custom_paper_name_used (GtkCustomPaperUnixDialog *dialog,
			const gchar              *name)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
	{
	  gtk_tree_model_get (model, &iter, 0, &page_setup, -1);
	  paper_size = gtk_page_setup_get_paper_size (page_setup);
	  if (strcmp (name,
		      gtk_paper_size_get_name (paper_size)) == 0)
	    {
	      g_object_unref (page_setup);
	      return TRUE;
	    }
	  g_object_unref (page_setup);
	} while (gtk_tree_model_iter_next (model, &iter));
    }

  return FALSE;
}

static void
add_custom_paper (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkListStore *store;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  GtkTreeSelection *selection;
  GtkTreePath *path;
  GtkTreeIter iter;
  gchar *name;
  gint i;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  store = priv->custom_paper_list;

  i = 1;
  name = NULL;
  do
    {
      g_free (name);
      name = g_strdup_printf (_("Custom Size %d"), i);
      i++;
    } while (custom_paper_name_used (dialog, name));

  page_setup = gtk_page_setup_new ();
  paper_size = gtk_paper_size_new_custom (name, name,
					  gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM),
					  gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM),
					  GTK_UNIT_MM);
  gtk_page_setup_set_paper_size (page_setup, paper_size);
  gtk_paper_size_free (paper_size);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, page_setup, -1);
  g_object_unref (page_setup);

  gtk_tree_selection_select_iter (selection, &iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
  gtk_widget_grab_focus (priv->treeview);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->treeview), path,
			    priv->text_column, TRUE);
  gtk_tree_path_free (path);
  g_free (name);
}

static void
remove_custom_paper (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkListStore *store;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  store = priv->custom_paper_list;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
      gtk_list_store_remove (store, &iter);

      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
	gtk_tree_selection_select_iter (selection, &iter);
      else if (gtk_tree_path_prev (path) && gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
	gtk_tree_selection_select_iter (selection, &iter);

      gtk_tree_path_free (path);
    }
}

static void
set_margins_from_printer (GtkCustomPaperUnixDialog *dialog,
			  GtkPrinter               *printer)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  gdouble top, bottom, left, right;

  top = bottom = left = right = 0;
  if (!gtk_printer_get_hard_margins (printer, &top, &bottom, &left, &right))
    return;

  priv->non_user_change = TRUE;
  unit_widget_set (priv->top_widget, _gtk_print_convert_to_mm (top, GTK_UNIT_POINTS));
  unit_widget_set (priv->bottom_widget, _gtk_print_convert_to_mm (bottom, GTK_UNIT_POINTS));
  unit_widget_set (priv->left_widget, _gtk_print_convert_to_mm (left, GTK_UNIT_POINTS));
  unit_widget_set (priv->right_widget, _gtk_print_convert_to_mm (right, GTK_UNIT_POINTS));
  priv->non_user_change = FALSE;

  /* Only send one change */
  unit_widget_changed (dialog);
}

static void
get_margins_finished_callback (GtkPrinter               *printer,
			       gboolean                  success,
			       GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;

  g_signal_handler_disconnect (priv->request_details_printer,
			       priv->request_details_tag);
  g_object_unref (priv->request_details_printer);
  priv->request_details_tag = 0;
  priv->request_details_printer = NULL;

  if (success)
    set_margins_from_printer (dialog, printer);

  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->printer_combo), 0);
}

static void
margins_from_printer_changed (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter;
  GtkComboBox *combo;
  GtkPrinter *printer;

  combo = GTK_COMBO_BOX (priv->printer_combo);

  if (priv->request_details_tag)
    {
      g_signal_handler_disconnect (priv->request_details_printer,
				   priv->request_details_tag);
      g_object_unref (priv->request_details_printer);
      priv->request_details_printer = NULL;
      priv->request_details_tag = 0;
    }

  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
			  PRINTER_LIST_COL_PRINTER, &printer, -1);

      if (printer)
	{
	  if (gtk_printer_has_details (printer))
	    {
	      set_margins_from_printer (dialog, printer);
	      gtk_combo_box_set_active (combo, 0);
	    }
	  else
	    {
	      priv->request_details_printer = g_object_ref (printer);
	      priv->request_details_tag =
		g_signal_connect (printer, "details-acquired",
				  G_CALLBACK (get_margins_finished_callback), dialog);
	      gtk_printer_request_details (printer);
	    }

	  g_object_unref (printer);
	}
    }
}

static void
custom_size_name_edited (GtkCellRenderer          *cell,
			 gchar                    *path_string,
			 gchar                    *new_text,
			 GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkListStore *store;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;

  store = priv->custom_paper_list;
  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &page_setup, -1);
  gtk_tree_path_free (path);

  paper_size = gtk_paper_size_new_custom (new_text, new_text,
					  gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM),
					  gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM),
					  GTK_UNIT_MM);
  gtk_page_setup_set_paper_size (page_setup, paper_size);
  gtk_paper_size_free (paper_size);

  g_object_unref (page_setup);
}

static void
custom_name_func (GtkTreeViewColumn *tree_column,
		  GtkCellRenderer   *cell,
		  GtkTreeModel      *tree_model,
		  GtkTreeIter       *iter,
		  gpointer           data)
{
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;

  gtk_tree_model_get (tree_model, iter, 0, &page_setup, -1);
  if (page_setup)
    {
      paper_size = gtk_page_setup_get_paper_size (page_setup);
      g_object_set (cell, "text",  gtk_paper_size_get_display_name (paper_size), NULL);
      g_object_unref (page_setup);
    }
}

static GtkWidget *
wrap_in_frame (const gchar *label,
               GtkWidget   *child)
{
  GtkWidget *frame, *alignment, *label_widget;
  gchar *bold_text;

  label_widget = gtk_label_new (NULL);
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

static void
populate_dialog (GtkCustomPaperUnixDialog *dialog)
{
  GtkCustomPaperUnixDialogPrivate *priv = dialog->priv;
  GtkWidget *image, *table, *label, *widget, *frame, *combo;
  GtkWidget *hbox, *vbox, *treeview, *scrolled, *button_box, *button;
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkUnit user_units;

  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area), 6);

  hbox = gtk_hbox_new (FALSE, 18);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
				       GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
  gtk_widget_show (scrolled);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->custom_paper_list));
  priv->treeview = treeview;
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
  gtk_widget_set_size_request (treeview, 140, -1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (selection, "changed", G_CALLBACK (selected_custom_paper_changed), dialog);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "editable", TRUE, NULL);
  g_signal_connect (cell, "edited", 
		    G_CALLBACK (custom_size_name_edited), dialog);
  priv->text_column = column =
    gtk_tree_view_column_new_with_attributes ("paper", cell,
					      NULL);
  gtk_tree_view_column_set_cell_data_func  (column, cell, custom_name_func, NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  gtk_container_add (GTK_CONTAINER (scrolled), treeview);
  gtk_widget_show (treeview);

  button_box = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);
  gtk_widget_show (button_box);

  button = gtk_button_new ();
  image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect_swapped (button, "clicked", G_CALLBACK (add_custom_paper), dialog);

  button = gtk_button_new ();
  image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect_swapped (button, "clicked", G_CALLBACK (remove_custom_paper), dialog);

  user_units = _gtk_print_get_default_user_units ();

  vbox = gtk_vbox_new (FALSE, 18);
  priv->values_box = vbox;
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 2, FALSE);

  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);

  label = gtk_label_new_with_mnemonic (_("_Width:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1, GTK_FILL, 0, 0, 0);

  widget = new_unit_widget (dialog, user_units, label);
  priv->width_widget = widget;
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (widget);

  label = gtk_label_new_with_mnemonic (_("_Height:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 1, 2, GTK_FILL, 0, 0, 0);

  widget = new_unit_widget (dialog, user_units, label);
  priv->height_widget = widget;
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 1, 2, GTK_FILL, 0, 0, 0);
  gtk_widget_show (widget);

  frame = wrap_in_frame (_("Paper Size"), table);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (5, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);

  label = gtk_label_new_with_mnemonic (_("_Top:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);

  widget = new_unit_widget (dialog, user_units, label);
  priv->top_widget = widget;
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 0, 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (widget);

  label = gtk_label_new_with_mnemonic (_("_Bottom:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1 , 1, 2, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);

  widget = new_unit_widget (dialog, user_units, label);
  priv->bottom_widget = widget;
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 1, 2, GTK_FILL, 0, 0, 0);
  gtk_widget_show (widget);

  label = gtk_label_new_with_mnemonic (_("_Left:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 2, 3, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);

  widget = new_unit_widget (dialog, user_units, label);
  priv->left_widget = widget;
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 2, 3, GTK_FILL, 0, 0, 0);
  gtk_widget_show (widget);

  label = gtk_label_new_with_mnemonic (_("_Right:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, 3, 4, GTK_FILL, 0, 0, 0);
  gtk_widget_show (label);

  widget = new_unit_widget (dialog, user_units, label);
  priv->right_widget = widget;
  gtk_table_attach (GTK_TABLE (table), widget,
		    1, 2, 3, 4, GTK_FILL, 0, 0, 0);
  gtk_widget_show (widget);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox,
		    0, 2, 4, 5, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  gtk_widget_show (hbox);

  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->printer_list));
  priv->printer_combo = combo;

  priv->printer_inserted_tag =
    g_signal_connect_swapped (priv->printer_list, "row-inserted",
			      G_CALLBACK (update_combo_sensitivity_from_printers), dialog);
  priv->printer_removed_tag =
    g_signal_connect_swapped (priv->printer_list, "row-deleted",
			      G_CALLBACK (update_combo_sensitivity_from_printers), dialog);
  update_combo_sensitivity_from_printers (dialog);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), cell,
				      custom_paper_printer_data_func,
				      NULL, NULL);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_widget_show (combo);

  g_signal_connect_swapped (combo, "changed",
			    G_CALLBACK (margins_from_printer_changed), dialog);

  frame = wrap_in_frame (_("Paper Margins"), table);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  update_custom_widgets_from_list (dialog);

  /* If no custom sizes, add one */
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->custom_paper_list),
				      &iter))
    {
      /* Need to realize treeview so we can start the rename */
      gtk_widget_realize (treeview);
      add_custom_paper (dialog);
    }

  gtk_window_present (GTK_WINDOW (dialog));

  load_print_backends (dialog);
}


#define __GTK_CUSTOM_PAPER_UNIX_DIALOG_C__
#include "gtkaliasdef.c"
