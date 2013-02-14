/* GtkPageSetupUnixDialog
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"
#include <string.h>
#include <locale.h>

#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkliststore.h"
#include "gtkstock.h"
#include "gtktreeviewcolumn.h"
#include "gtktreeselection.h"
#include "gtktreemodel.h"
#include "gtkbutton.h"
#include "gtkscrolledwindow.h"
#include "gtkcombobox.h"
#include "gtktogglebutton.h"
#include "gtkradiobutton.h"
#include "gtklabel.h"
#include "gtkgrid.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"

#include "gtkpagesetupunixdialog.h"
#include "gtkcustompaperunixdialog.h"
#include "gtkprintbackend.h"
#include "gtkpapersize.h"
#include "gtkprintutils.h"

/**
 * SECTION:gtkpagesetupunixdialog
 * @Short_description: A page setup dialog
 * @Title: GtkPageSetupUnixDialog
 *
 * #GtkPageSetupUnixDialog implements a page setup dialog for platforms
 * which don't provide a native page setup dialog, like Unix. It can
 * be used very much like any other GTK+ dialog, at the cost of
 * the portability offered by the <link
 * linkend="gtk-High-level-Printing-API">high-level printing API</link>
 *
 * Printing support was added in GTK+ 2.10.
 */


struct _GtkPageSetupUnixDialogPrivate
{
  GtkListStore *printer_list;
  GtkListStore *page_setup_list;
  GtkListStore *custom_paper_list;

  GList *print_backends;

  GtkWidget *printer_combo;
  GtkWidget *paper_size_combo;
  GtkWidget *paper_size_label;

  GtkWidget *portrait_radio;
  GtkWidget *reverse_portrait_radio;
  GtkWidget *landscape_radio;
  GtkWidget *reverse_landscape_radio;

  gulong request_details_tag;
  GtkPrinter *request_details_printer;

  GtkPrintSettings *print_settings;

  /* Save last setup so we can re-set it after selecting manage custom sizes */
  GtkPageSetup *last_setup;

  gchar *waiting_for_printer;
};

enum {
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_PRINTER,
  PRINTER_LIST_N_COLS
};

enum {
  PAGE_SETUP_LIST_COL_PAGE_SETUP,
  PAGE_SETUP_LIST_COL_IS_SEPARATOR,
  PAGE_SETUP_LIST_N_COLS
};

G_DEFINE_TYPE (GtkPageSetupUnixDialog, gtk_page_setup_unix_dialog, GTK_TYPE_DIALOG)

static void gtk_page_setup_unix_dialog_finalize  (GObject                *object);
static void populate_dialog                      (GtkPageSetupUnixDialog *dialog);
static void fill_paper_sizes_from_printer        (GtkPageSetupUnixDialog *dialog,
                                                  GtkPrinter             *printer);
static void printer_added_cb                     (GtkPrintBackend        *backend,
                                                  GtkPrinter             *printer,
                                                  GtkPageSetupUnixDialog *dialog);
static void printer_removed_cb                   (GtkPrintBackend        *backend,
                                                  GtkPrinter             *printer,
                                                  GtkPageSetupUnixDialog *dialog);
static void printer_status_cb                    (GtkPrintBackend        *backend,
                                                  GtkPrinter             *printer,
                                                  GtkPageSetupUnixDialog *dialog);



static const gchar const common_paper_sizes[][16] = {
  "na_letter",
  "na_legal",
  "iso_a4",
  "iso_a5",
  "roc_16k",
  "iso_b5",
  "jis_b5",
  "na_number-10",
  "iso_dl",
  "jpn_chou3",
  "na_ledger",
  "iso_a3",
};


static void
gtk_page_setup_unix_dialog_class_init (GtkPageSetupUnixDialogClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) class;

  object_class->finalize = gtk_page_setup_unix_dialog_finalize;

  g_type_class_add_private (class, sizeof (GtkPageSetupUnixDialogPrivate));
}

static void
gtk_page_setup_unix_dialog_init (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv;
  GtkTreeIter iter;
  gchar *tmp;

  priv = dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
                                                     GTK_TYPE_PAGE_SETUP_UNIX_DIALOG,
                                                     GtkPageSetupUnixDialogPrivate);

  priv->print_backends = NULL;

  priv->printer_list = gtk_list_store_new (PRINTER_LIST_N_COLS,
                                                   G_TYPE_STRING,
                                                   G_TYPE_OBJECT);

  gtk_list_store_append (priv->printer_list, &iter);
  tmp = g_strdup_printf ("<b>%s</b>\n%s", _("Any Printer"), _("For portable documents"));
  gtk_list_store_set (priv->printer_list, &iter,
                      PRINTER_LIST_COL_NAME, tmp,
                      PRINTER_LIST_COL_PRINTER, NULL,
                      -1);
  g_free (tmp);

  priv->page_setup_list = gtk_list_store_new (PAGE_SETUP_LIST_N_COLS,
                                                      G_TYPE_OBJECT,
                                                      G_TYPE_BOOLEAN);

  priv->custom_paper_list = gtk_list_store_new (1, G_TYPE_OBJECT);
  _gtk_print_load_custom_papers (priv->custom_paper_list);

  populate_dialog (dialog);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_APPLY, GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
gtk_page_setup_unix_dialog_finalize (GObject *object)
{
  GtkPageSetupUnixDialog *dialog = GTK_PAGE_SETUP_UNIX_DIALOG (object);
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkPrintBackend *backend;
  GList *node;

  if (priv->request_details_tag)
    {
      g_signal_handler_disconnect (priv->request_details_printer,
                                   priv->request_details_tag);
      g_object_unref (priv->request_details_printer);
      priv->request_details_printer = NULL;
      priv->request_details_tag = 0;
    }

  if (priv->printer_list)
    {
      g_object_unref (priv->printer_list);
      priv->printer_list = NULL;
    }

  if (priv->page_setup_list)
    {
      g_object_unref (priv->page_setup_list);
      priv->page_setup_list = NULL;
    }

  if (priv->custom_paper_list)
    {
      g_object_unref (priv->custom_paper_list);
      priv->custom_paper_list = NULL;
    }

  if (priv->print_settings)
    {
      g_object_unref (priv->print_settings);
      priv->print_settings = NULL;
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

  G_OBJECT_CLASS (gtk_page_setup_unix_dialog_parent_class)->finalize (object);
}

static void
printer_added_cb (GtkPrintBackend        *backend,
                  GtkPrinter             *printer,
                  GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter;
  gchar *str;
  const gchar *location;

  if (gtk_printer_is_virtual (printer))
    return;

  location = gtk_printer_get_location (printer);
  if (location == NULL)
    location = "";
  str = g_strdup_printf ("<b>%s</b>\n%s",
                         gtk_printer_get_name (printer),
                         location);

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
                    GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");
  gtk_list_store_remove (GTK_LIST_STORE (priv->printer_list), iter);
}


static void
printer_status_cb (GtkPrintBackend        *backend,
                   GtkPrinter             *printer,
                   GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;
  gchar *str;
  const gchar *location;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");

  location = gtk_printer_get_location (printer);
  if (location == NULL)
    location = "";
  str = g_strdup_printf ("<b>%s</b>\n%s",
                         gtk_printer_get_name (printer),
                         location);
  gtk_list_store_set (priv->printer_list, iter,
                      PRINTER_LIST_COL_NAME, str,
                      -1);
  g_free (str);
}

static void
printer_list_initialize (GtkPageSetupUnixDialog *dialog,
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
load_print_backends (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GList *node;

  if (g_module_supported ())
    priv->print_backends = gtk_print_backend_load_modules ();

  for (node = priv->print_backends; node != NULL; node = node->next)
    printer_list_initialize (dialog, GTK_PRINT_BACKEND (node->data));
}

static gboolean
paper_size_row_is_separator (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  gboolean separator;

  gtk_tree_model_get (model, iter, PAGE_SETUP_LIST_COL_IS_SEPARATOR, &separator, -1);
  return separator;
}

static GtkPageSetup *
get_current_page_setup (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkPageSetup *current_page_setup;
  GtkComboBox *combo_box;
  GtkTreeIter iter;

  current_page_setup = NULL;

  combo_box = GTK_COMBO_BOX (priv->paper_size_combo);
  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    gtk_tree_model_get (GTK_TREE_MODEL (priv->page_setup_list), &iter,
                        PAGE_SETUP_LIST_COL_PAGE_SETUP, &current_page_setup, -1);

  if (current_page_setup)
    return current_page_setup;

  /* No selected page size, return the default one.
   * This is used to set the first page setup when the dialog is created
   * as there is no selection on the first printer_changed.
   */
  return gtk_page_setup_new ();
}

static gboolean
page_setup_is_equal (GtkPageSetup *a,
                     GtkPageSetup *b)
{
  return
    gtk_paper_size_is_equal (gtk_page_setup_get_paper_size (a),
                             gtk_page_setup_get_paper_size (b)) &&
    gtk_page_setup_get_top_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_top_margin (b, GTK_UNIT_MM) &&
    gtk_page_setup_get_bottom_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_bottom_margin (b, GTK_UNIT_MM) &&
    gtk_page_setup_get_left_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_left_margin (b, GTK_UNIT_MM) &&
    gtk_page_setup_get_right_margin (a, GTK_UNIT_MM) == gtk_page_setup_get_right_margin (b, GTK_UNIT_MM);
}

static gboolean
page_setup_is_same_size (GtkPageSetup *a,
                         GtkPageSetup *b)
{
  return gtk_paper_size_is_equal (gtk_page_setup_get_paper_size (a),
                                  gtk_page_setup_get_paper_size (b));
}

static gboolean
set_paper_size (GtkPageSetupUnixDialog *dialog,
                GtkPageSetup           *page_setup,
                gboolean                size_only,
                gboolean                add_item)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkPageSetup *list_page_setup;

  model = GTK_TREE_MODEL (priv->page_setup_list);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->page_setup_list), &iter,
                              PAGE_SETUP_LIST_COL_PAGE_SETUP, &list_page_setup, -1);
          if (list_page_setup == NULL)
            continue;

          if ((size_only && page_setup_is_same_size (page_setup, list_page_setup)) ||
              (!size_only && page_setup_is_equal (page_setup, list_page_setup)))
            {
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->paper_size_combo),
                                             &iter);
              g_object_unref (list_page_setup);
              return TRUE;
            }

          g_object_unref (list_page_setup);

        } while (gtk_tree_model_iter_next (model, &iter));
    }

  if (add_item)
    {
      gtk_list_store_append (priv->page_setup_list, &iter);
      gtk_list_store_set (priv->page_setup_list, &iter,
                          PAGE_SETUP_LIST_COL_IS_SEPARATOR, TRUE,
                          -1);
      gtk_list_store_append (priv->page_setup_list, &iter);
      gtk_list_store_set (priv->page_setup_list, &iter,
                          PAGE_SETUP_LIST_COL_PAGE_SETUP, page_setup,
                          -1);
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->paper_size_combo),
                                     &iter);
      return TRUE;
    }

  return FALSE;
}

static void
fill_custom_paper_sizes (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter, paper_iter;
  GtkTreeModel *model;

  model = GTK_TREE_MODEL (priv->custom_paper_list);
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      gtk_list_store_append (priv->page_setup_list, &paper_iter);
      gtk_list_store_set (priv->page_setup_list, &paper_iter,
                          PAGE_SETUP_LIST_COL_IS_SEPARATOR, TRUE,
                          -1);
      do
        {
          GtkPageSetup *page_setup;
          gtk_tree_model_get (model, &iter, 0, &page_setup, -1);

          gtk_list_store_append (priv->page_setup_list, &paper_iter);
          gtk_list_store_set (priv->page_setup_list, &paper_iter,
                              PAGE_SETUP_LIST_COL_PAGE_SETUP, page_setup,
                              -1);

          g_object_unref (page_setup);
        } while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_list_store_append (priv->page_setup_list, &paper_iter);
  gtk_list_store_set (priv->page_setup_list, &paper_iter,
                      PAGE_SETUP_LIST_COL_IS_SEPARATOR, TRUE,
                      -1);
  gtk_list_store_append (priv->page_setup_list, &paper_iter);
  gtk_list_store_set (priv->page_setup_list, &paper_iter,
                      PAGE_SETUP_LIST_COL_PAGE_SETUP, NULL,
                      -1);
}

static void
fill_paper_sizes_from_printer (GtkPageSetupUnixDialog *dialog,
                               GtkPrinter             *printer)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GList *list, *l;
  GtkPageSetup *current_page_setup, *page_setup;
  GtkPaperSize *paper_size;
  GtkTreeIter iter;
  gint i;

  gtk_list_store_clear (priv->page_setup_list);

  if (printer == NULL)
    {
      for (i = 0; i < G_N_ELEMENTS (common_paper_sizes); i++)
        {
          page_setup = gtk_page_setup_new ();
          paper_size = gtk_paper_size_new (common_paper_sizes[i]);
          gtk_page_setup_set_paper_size_and_default_margins (page_setup, paper_size);
          gtk_paper_size_free (paper_size);

          gtk_list_store_append (priv->page_setup_list, &iter);
          gtk_list_store_set (priv->page_setup_list, &iter,
                              PAGE_SETUP_LIST_COL_PAGE_SETUP, page_setup,
                              -1);
          g_object_unref (page_setup);
        }
    }
  else
    {
      list = gtk_printer_list_papers (printer);
      /* TODO: We should really sort this list so interesting size
         are at the top */
      for (l = list; l != NULL; l = l->next)
        {
          page_setup = l->data;
          gtk_list_store_append (priv->page_setup_list, &iter);
          gtk_list_store_set (priv->page_setup_list, &iter,
                              PAGE_SETUP_LIST_COL_PAGE_SETUP, page_setup,
                              -1);
          g_object_unref (page_setup);
        }
      g_list_free (list);
    }

  fill_custom_paper_sizes (dialog);

  current_page_setup = NULL;

  /* When selecting a different printer, select its default paper size */
  if (printer != NULL)
    current_page_setup = gtk_printer_get_default_page_size (printer);

  if (current_page_setup == NULL)
    current_page_setup = get_current_page_setup (dialog);

  if (!set_paper_size (dialog, current_page_setup, FALSE, FALSE))
    set_paper_size (dialog, current_page_setup, TRUE, TRUE);

  if (current_page_setup)
    g_object_unref (current_page_setup);
}

static void
printer_changed_finished_callback (GtkPrinter             *printer,
                                   gboolean                success,
                                   GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;

  g_signal_handler_disconnect (priv->request_details_printer,
                               priv->request_details_tag);
  g_object_unref (priv->request_details_printer);
  priv->request_details_tag = 0;
  priv->request_details_printer = NULL;

  if (success)
    fill_paper_sizes_from_printer (dialog, printer);

}

static void
printer_changed_callback (GtkComboBox            *combo_box,
                          GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkPrinter *printer;
  GtkTreeIter iter;

  /* If we're waiting for a specific printer but the user changed
   * to another printer, cancel that wait.
   */
  if (priv->waiting_for_printer)
    {
      g_free (priv->waiting_for_printer);
      priv->waiting_for_printer = NULL;
    }

  if (priv->request_details_tag)
    {
      g_signal_handler_disconnect (priv->request_details_printer,
                                   priv->request_details_tag);
      g_object_unref (priv->request_details_printer);
      priv->request_details_printer = NULL;
      priv->request_details_tag = 0;
    }

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      gtk_tree_model_get (gtk_combo_box_get_model (combo_box), &iter,
                          PRINTER_LIST_COL_PRINTER, &printer, -1);

      if (printer == NULL || gtk_printer_has_details (printer))
        fill_paper_sizes_from_printer (dialog, printer);
      else
        {
          priv->request_details_printer = g_object_ref (printer);
          priv->request_details_tag =
            g_signal_connect (printer, "details-acquired",
                              G_CALLBACK (printer_changed_finished_callback), dialog);
          gtk_printer_request_details (printer);

        }

      if (printer)
        g_object_unref (printer);

      if (priv->print_settings)
        {
          const char *name = NULL;

          if (printer)
            name = gtk_printer_get_name (printer);

          gtk_print_settings_set (priv->print_settings,
                                  "format-for-printer", name);
        }
    }
}

/* We do this munging because we don't want to show zero digits
   after the decimal point, and not to many such digits if they
   are nonzero. I wish printf let you specify max precision for %f... */
static gchar *
double_to_string (gdouble d,
                  GtkUnit unit)
{
  gchar *val, *p;
  struct lconv *locale_data;
  const gchar *decimal_point;
  gint decimal_point_len;

  locale_data = localeconv ();
  decimal_point = locale_data->decimal_point;
  decimal_point_len = strlen (decimal_point);

  /* Max two decimal digits for inch, max one for mm */
  if (unit == GTK_UNIT_INCH)
    val = g_strdup_printf ("%.2f", d);
  else
    val = g_strdup_printf ("%.1f", d);

  if (strstr (val, decimal_point))
    {
      p = val + strlen (val) - 1;
      while (*p == '0')
        p--;
      if (p - val + 1 >= decimal_point_len &&
          strncmp (p - (decimal_point_len - 1), decimal_point, decimal_point_len) == 0)
        p -= decimal_point_len;
      p[1] = '\0';
    }

  return val;
}


static void
custom_paper_dialog_response_cb (GtkDialog *custom_paper_dialog,
                                 gint       response_id,
                                 gpointer   user_data)
{
  GtkPageSetupUnixDialog *page_setup_dialog = GTK_PAGE_SETUP_UNIX_DIALOG (user_data);
  GtkPageSetupUnixDialogPrivate *priv = page_setup_dialog->priv;

  _gtk_print_load_custom_papers (priv->custom_paper_list);

  /* Update printer page list */
  printer_changed_callback (GTK_COMBO_BOX (priv->printer_combo), page_setup_dialog);

  gtk_widget_destroy (GTK_WIDGET (custom_paper_dialog));
}

static void
paper_size_changed (GtkComboBox            *combo_box,
                    GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter;
  GtkPageSetup *page_setup, *last_page_setup;
  GtkUnit unit;
  gchar *str, *w, *h;
  gchar *top, *bottom, *left, *right;
  GtkLabel *label;
  const gchar *unit_str;

  label = GTK_LABEL (priv->paper_size_label);

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      gtk_tree_model_get (gtk_combo_box_get_model (combo_box),
                          &iter, PAGE_SETUP_LIST_COL_PAGE_SETUP, &page_setup, -1);

      if (page_setup == NULL)
        {
          GtkWidget *custom_paper_dialog;

          /* Change from "manage" menu item to last value */
          if (priv->last_setup)
            last_page_setup = g_object_ref (priv->last_setup);
          else
            last_page_setup = gtk_page_setup_new (); /* "good" default */
          set_paper_size (dialog, last_page_setup, FALSE, TRUE);
          g_object_unref (last_page_setup);

          /* And show the custom paper dialog */
          custom_paper_dialog = _gtk_custom_paper_unix_dialog_new (GTK_WINDOW (dialog), NULL);
          g_signal_connect (custom_paper_dialog, "response", G_CALLBACK (custom_paper_dialog_response_cb), dialog);
          gtk_window_present (GTK_WINDOW (custom_paper_dialog));

          return;
        }

      if (priv->last_setup)
        g_object_unref (priv->last_setup);

      priv->last_setup = g_object_ref (page_setup);

      unit = _gtk_print_get_default_user_units ();

      if (unit == GTK_UNIT_MM)
        unit_str = _("mm");
      else
        unit_str = _("inch");

      w = double_to_string (gtk_page_setup_get_paper_width (page_setup, unit),
                            unit);
      h = double_to_string (gtk_page_setup_get_paper_height (page_setup, unit),
                            unit);
      str = g_strdup_printf ("%s x %s %s", w, h, unit_str);
      g_free (w);
      g_free (h);

      gtk_label_set_text (label, str);
      g_free (str);

      top = double_to_string (gtk_page_setup_get_top_margin (page_setup, unit), unit);
      bottom = double_to_string (gtk_page_setup_get_bottom_margin (page_setup, unit), unit);
      left = double_to_string (gtk_page_setup_get_left_margin (page_setup, unit), unit);
      right = double_to_string (gtk_page_setup_get_right_margin (page_setup, unit), unit);

      str = g_strdup_printf (_("Margins:\n"
                               " Left: %s %s\n"
                               " Right: %s %s\n"
                               " Top: %s %s\n"
                               " Bottom: %s %s"
                               ),
                             left, unit_str,
                             right, unit_str,
                             top, unit_str,
                             bottom, unit_str);
      g_free (top);
      g_free (bottom);
      g_free (left);
      g_free (right);

      gtk_widget_set_tooltip_text (priv->paper_size_label, str);
      g_free (str);

      g_object_unref (page_setup);
    }
  else
    {
      gtk_label_set_text (label, "");
      gtk_widget_set_tooltip_text (priv->paper_size_label, NULL);
      if (priv->last_setup)
        g_object_unref (priv->last_setup);
      priv->last_setup = NULL;
    }
}

static void
page_name_func (GtkCellLayout   *cell_layout,
                GtkCellRenderer *cell,
                GtkTreeModel    *tree_model,
                GtkTreeIter     *iter,
                gpointer         data)
{
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;

  gtk_tree_model_get (tree_model, iter,
                      PAGE_SETUP_LIST_COL_PAGE_SETUP, &page_setup, -1);
  if (page_setup)
    {
      paper_size = gtk_page_setup_get_paper_size (page_setup);
      g_object_set (cell, "text",  gtk_paper_size_get_display_name (paper_size), NULL);
      g_object_unref (page_setup);
    }
  else
    g_object_set (cell, "text",  _("Manage Custom Sizes…"), NULL);

}

static GtkWidget *
create_radio_button (GSList      *group,
                     const gchar *stock_id)
{
  GtkWidget *radio_button, *image, *label, *hbox;
  GtkStockItem item;

  radio_button = gtk_radio_button_new (group);
  image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_stock_lookup (stock_id, &item);
  label = gtk_label_new (item.label);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (radio_button), hbox);
  gtk_container_add (GTK_CONTAINER (hbox), image);
  gtk_container_add (GTK_CONTAINER (hbox), label);

  gtk_widget_show_all (radio_button);

  return radio_button;
}

static void
populate_dialog (GtkPageSetupUnixDialog *ps_dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = ps_dialog->priv;
  GtkDialog *dialog = GTK_DIALOG (ps_dialog);
  GtkWidget *table, *label, *combo, *radio_button;
  GtkWidget *action_area, *content_area;
  GtkCellRenderer *cell;

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  content_area = gtk_dialog_get_content_area (dialog);
  action_area = gtk_dialog_get_action_area (dialog);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);

  table = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (table), 6);
  gtk_grid_set_column_spacing (GTK_GRID (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_box_pack_start (GTK_BOX (content_area), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  label = gtk_label_new_with_mnemonic (_("_Format for:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->printer_list));
  priv->printer_combo = combo;

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
                                  "markup", PRINTER_LIST_COL_NAME,
                                  NULL);

  gtk_widget_set_halign (combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (table), combo, 1, 0, 3, 1);
  gtk_widget_show (combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  label = gtk_label_new_with_mnemonic (_("_Paper size:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->page_setup_list));
  priv->paper_size_combo = combo;
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
                                        paper_size_row_is_separator, NULL, NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), cell,
                                      page_name_func, NULL, NULL);

  gtk_widget_set_halign (combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (table), combo, 1, 1, 3, 1);
  gtk_widget_show (combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  label = gtk_label_new (NULL);
  priv->paper_size_label = label;
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (table), label, 1, 2, 3, 1);
  gtk_widget_show (label);

  label = gtk_label_new_with_mnemonic (_("_Orientation:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
  gtk_widget_show (label);

  radio_button = create_radio_button (NULL, GTK_STOCK_ORIENTATION_PORTRAIT);
  priv->portrait_radio = radio_button;
  gtk_widget_set_halign (combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (table), radio_button, 1, 3, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), radio_button);

  radio_button = create_radio_button (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button)),
                                      GTK_STOCK_ORIENTATION_REVERSE_PORTRAIT);
  priv->reverse_portrait_radio = radio_button;
  gtk_widget_set_halign (combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (table), radio_button, 2, 3, 1, 1);

  radio_button = create_radio_button (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button)),
                                      GTK_STOCK_ORIENTATION_LANDSCAPE);
  priv->landscape_radio = radio_button;
  gtk_widget_set_halign (combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (table), radio_button, 1, 4, 1, 1);
  gtk_widget_show (radio_button);

  radio_button = create_radio_button (gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button)),
                                      GTK_STOCK_ORIENTATION_REVERSE_LANDSCAPE);
  priv->reverse_landscape_radio = radio_button;
  gtk_widget_set_halign (combo, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (combo, TRUE);
  gtk_grid_attach (GTK_GRID (table), radio_button, 2, 4, 1, 1);

  g_signal_connect (priv->paper_size_combo, "changed", G_CALLBACK (paper_size_changed), ps_dialog);
  g_signal_connect (priv->printer_combo, "changed", G_CALLBACK (printer_changed_callback), ps_dialog);
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->printer_combo), 0);

  load_print_backends (ps_dialog);
}

/**
 * gtk_page_setup_unix_dialog_new:
 * @title: (allow-none): the title of the dialog, or %NULL
 * @parent: (allow-none): transient parent of the dialog, or %NULL
 *
 * Creates a new page setup dialog.
 *
 * Returns: the new #GtkPageSetupUnixDialog
 *
 * Since: 2.10
 */
GtkWidget *
gtk_page_setup_unix_dialog_new (const gchar *title,
                                GtkWindow   *parent)
{
  GtkWidget *result;

  if (title == NULL)
    title = _("Page Setup");

  result = g_object_new (GTK_TYPE_PAGE_SETUP_UNIX_DIALOG,
                         "title", title,
                         NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  return result;
}

static GtkPageOrientation
get_orientation (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->portrait_radio)))
    return GTK_PAGE_ORIENTATION_PORTRAIT;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->landscape_radio)))
    return GTK_PAGE_ORIENTATION_LANDSCAPE;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->reverse_landscape_radio)))
    return GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE;
  return GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT;
}

static void
set_orientation (GtkPageSetupUnixDialog *dialog,
                 GtkPageOrientation      orientation)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;

  switch (orientation)
    {
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->reverse_portrait_radio), TRUE);
      break;
    case GTK_PAGE_ORIENTATION_PORTRAIT:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->portrait_radio), TRUE);
      break;
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->landscape_radio), TRUE);
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->reverse_landscape_radio), TRUE);
      break;
    }
}

/**
 * gtk_page_setup_unix_dialog_set_page_setup:
 * @dialog: a #GtkPageSetupUnixDialog
 * @page_setup: a #GtkPageSetup
 *
 * Sets the #GtkPageSetup from which the page setup
 * dialog takes its values.
 *
 * Since: 2.10
 **/
void
gtk_page_setup_unix_dialog_set_page_setup (GtkPageSetupUnixDialog *dialog,
                                           GtkPageSetup           *page_setup)
{
  if (page_setup)
    {
      set_paper_size (dialog, page_setup, FALSE, TRUE);
      set_orientation (dialog, gtk_page_setup_get_orientation (page_setup));
    }
}

/**
 * gtk_page_setup_unix_dialog_get_page_setup:
 * @dialog: a #GtkPageSetupUnixDialog
 *
 * Gets the currently selected page setup from the dialog.
 *
 * Returns: (transfer none): the current page setup
 *
 * Since: 2.10
 **/
GtkPageSetup *
gtk_page_setup_unix_dialog_get_page_setup (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetup *page_setup;

  page_setup = get_current_page_setup (dialog);

  gtk_page_setup_set_orientation (page_setup, get_orientation (dialog));

  return page_setup;
}

static gboolean
set_active_printer (GtkPageSetupUnixDialog *dialog,
                    const gchar            *printer_name)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkPrinter *printer;

  model = GTK_TREE_MODEL (priv->printer_list);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->printer_list), &iter,
                              PRINTER_LIST_COL_PRINTER, &printer, -1);
          if (printer == NULL)
            continue;

          if (strcmp (gtk_printer_get_name (printer), printer_name) == 0)
            {
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->printer_combo),
                                             &iter);
              g_object_unref (printer);
              return TRUE;
            }

          g_object_unref (printer);

        } while (gtk_tree_model_iter_next (model, &iter));
    }

  return FALSE;
}

/**
 * gtk_page_setup_unix_dialog_set_print_settings:
 * @dialog: a #GtkPageSetupUnixDialog
 * @print_settings: a #GtkPrintSettings
 *
 * Sets the #GtkPrintSettings from which the page setup dialog
 * takes its values.
 *
 * Since: 2.10
 **/
void
gtk_page_setup_unix_dialog_set_print_settings (GtkPageSetupUnixDialog *dialog,
                                               GtkPrintSettings       *print_settings)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;
  const gchar *format_for_printer;

  if (priv->print_settings == print_settings) return;

  if (priv->print_settings)
    g_object_unref (priv->print_settings);

  priv->print_settings = print_settings;

  if (print_settings)
    {
      g_object_ref (print_settings);

      format_for_printer = gtk_print_settings_get (print_settings, "format-for-printer");

      /* Set printer if in list, otherwise set when
       * that printer is added
       */
      if (format_for_printer &&
          !set_active_printer (dialog, format_for_printer))
        priv->waiting_for_printer = g_strdup (format_for_printer);
    }
}

/**
 * gtk_page_setup_unix_dialog_get_print_settings:
 * @dialog: a #GtkPageSetupUnixDialog
 *
 * Gets the current print settings from the dialog.
 *
 * Returns: (transfer none): the current print settings
 *
 * Since: 2.10
 **/
GtkPrintSettings *
gtk_page_setup_unix_dialog_get_print_settings (GtkPageSetupUnixDialog *dialog)
{
  GtkPageSetupUnixDialogPrivate *priv = dialog->priv;

  return priv->print_settings;
}
