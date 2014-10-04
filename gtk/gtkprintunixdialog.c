/* GtkPrintUnixDialog
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
 * Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 * Copyright © 2006, 2007 Christian Persch
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#include "gtkprintunixdialog.h"

#include "gtkcustompaperunixdialog.h"
#include "gtkprintbackend.h"
#include "gtkprinter-private.h"
#include "gtkprinteroptionwidget.h"
#include "gtkprintutils.h"

#include "gtkspinbutton.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkimage.h"
#include "gtktreeselection.h"
#include "gtknotebook.h"
#include "gtkscrolledwindow.h"
#include "gtkcombobox.h"
#include "gtktogglebutton.h"
#include "gtkradiobutton.h"
#include "gtkdrawingarea.h"
#include "gtkbox.h"
#include "gtkgrid.h"
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtkeventbox.h"
#include "gtkbuildable.h"
#include "gtkmessagedialog.h"
#include "gtkbutton.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkdialogprivate.h"


/**
 * SECTION:gtkprintunixdialog
 * @Short_description: A print dialog
 * @Title: GtkPrintUnixDialog
 * @See_also: #GtkPageSetupUnixDialog, #GtkPrinter, #GtkPrintJob
 *
 * GtkPrintUnixDialog implements a print dialog for platforms
 * which don’t provide a native print dialog, like Unix. It can
 * be used very much like any other GTK+ dialog, at the cost of
 * the portability offered by the
 * [high-level printing API][gtk3-High-level-Printing-API]
 *
 * In order to print something with #GtkPrintUnixDialog, you need
 * to use gtk_print_unix_dialog_get_selected_printer() to obtain
 * a #GtkPrinter object and use it to construct a #GtkPrintJob using
 * gtk_print_job_new().
 *
 * #GtkPrintUnixDialog uses the following response values:
 * - %GTK_RESPONSE_OK: for the “Print” button
 * - %GTK_RESPONSE_APPLY: for the “Preview” button
 * - %GTK_RESPONSE_CANCEL: for the “Cancel” button
 *
 * Printing support was added in GTK+ 2.10.
 *
 * # GtkPrintUnixDialog as GtkBuildable
 *
 * The GtkPrintUnixDialog implementation of the GtkBuildable interface exposes its
 * @notebook internal children with the name “notebook”.
 *
 * An example of a #GtkPrintUnixDialog UI definition fragment:
 * |[
 * <object class="GtkPrintUnixDialog" id="dialog1">
 *   <child internal-child="notebook">
 *     <object class="GtkNotebook" id="notebook">
 *       <child>
 *         <object class="GtkLabel" id="tabcontent">
 *         <property name="label">Content on notebook tab</property>
 *         </object>
 *       </child>
 *       <child type="tab">
 *         <object class="GtkLabel" id="tablabel">
 *           <property name="label">Tab label</property>
 *         </object>
 *         <packing>
 *           <property name="tab_expand">False</property>
 *           <property name="tab_fill">False</property>
 *         </packing>
 *       </child>
 *     </object>
 *   </child>
 * </object>
 * ]|
 */


#define EXAMPLE_PAGE_AREA_SIZE 110
#define RULER_DISTANCE 7.5
#define RULER_RADIUS 2


static void     gtk_print_unix_dialog_constructed  (GObject            *object);
static void     gtk_print_unix_dialog_destroy      (GtkWidget          *widget);
static void     gtk_print_unix_dialog_finalize     (GObject            *object);
static void     gtk_print_unix_dialog_set_property (GObject            *object,
                                                    guint               prop_id,
                                                    const GValue       *value,
                                                    GParamSpec         *pspec);
static void     gtk_print_unix_dialog_get_property (GObject            *object,
                                                    guint               prop_id,
                                                    GValue             *value,
                                                    GParamSpec         *pspec);
static void     gtk_print_unix_dialog_style_updated (GtkWidget          *widget);
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
static gboolean error_dialogs                      (GtkPrintUnixDialog *print_dialog,
						    gint                print_dialog_response_id,
						    gpointer            data);
static void     emit_ok_response                   (GtkTreeView        *tree_view,
						    GtkTreePath        *path,
						    GtkTreeViewColumn  *column,
						    gpointer           *user_data);
static void     update_page_range_entry_sensitivity(GtkWidget          *button,
						    GtkPrintUnixDialog *dialog);
static void     update_print_at_entry_sensitivity  (GtkWidget          *button,
						    GtkPrintUnixDialog *dialog);
static void     update_print_at_option             (GtkPrintUnixDialog *dialog);
static void     update_dialog_from_capabilities    (GtkPrintUnixDialog *dialog);
static gboolean draw_collate_cb                    (GtkWidget          *widget,
						    cairo_t            *cr,
						    GtkPrintUnixDialog *dialog);
static gboolean is_printer_active                  (GtkTreeModel        *model,
						    GtkTreeIter         *iter,
						    GtkPrintUnixDialog  *dialog);
static gint     default_printer_list_sort_func     (GtkTreeModel        *model,
						    GtkTreeIter         *a,
						    GtkTreeIter         *b,
						    gpointer             user_data);
static gboolean paper_size_row_is_separator        (GtkTreeModel        *model,
						    GtkTreeIter         *iter,
						    gpointer             data);
static void     page_name_func                     (GtkCellLayout       *cell_layout,
						    GtkCellRenderer     *cell,
						    GtkTreeModel        *tree_model,
						    GtkTreeIter         *iter,
						    gpointer             data);
static void     update_number_up_layout            (GtkPrintUnixDialog  *dialog);
static gboolean draw_page_cb                       (GtkWidget           *widget,
						    cairo_t             *cr,
						    GtkPrintUnixDialog  *dialog);


static gboolean dialog_get_collate                 (GtkPrintUnixDialog *dialog);
static gboolean dialog_get_reverse                 (GtkPrintUnixDialog *dialog);
static gint     dialog_get_n_copies                (GtkPrintUnixDialog *dialog);

static void     set_cell_sensitivity_func          (GtkTreeViewColumn *tree_column,
                                                    GtkCellRenderer   *cell,
                                                    GtkTreeModel      *model,
                                                    GtkTreeIter       *iter,
                                                    gpointer           data);
static gboolean set_active_printer                 (GtkPrintUnixDialog *dialog,
                                                    const gchar        *printer_name);
static void redraw_page_layout_preview             (GtkPrintUnixDialog *dialog);
static void load_print_backends                    (GtkPrintUnixDialog *dialog);
static gboolean printer_compare                    (GtkTreeModel       *model,
                                                    gint                column,
                                                    const gchar        *key,
                                                    GtkTreeIter        *iter,
                                                    gpointer            search_data);

/* GtkBuildable */
static void gtk_print_unix_dialog_buildable_init                    (GtkBuildableIface *iface);
static GObject *gtk_print_unix_dialog_buildable_get_internal_child  (GtkBuildable *buildable,
                                                                     GtkBuilder   *builder,
                                                                     const gchar  *childname);

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

/* Keep in line with liststore defined in gtkprintunixdialog.ui */
enum {
  PAGE_SETUP_LIST_COL_PAGE_SETUP,
  PAGE_SETUP_LIST_COL_IS_SEPARATOR,
  PAGE_SETUP_LIST_N_COLS
};

/* Keep in line with liststore defined in gtkprintunixdialog.ui */
enum {
  PRINTER_LIST_COL_ICON,
  PRINTER_LIST_COL_NAME,
  PRINTER_LIST_COL_STATE,
  PRINTER_LIST_COL_JOBS,
  PRINTER_LIST_COL_LOCATION,
  PRINTER_LIST_COL_PRINTER_OBJ,
  PRINTER_LIST_N_COLS
};

enum {
  PROP_0,
  PROP_PAGE_SETUP,
  PROP_CURRENT_PAGE,
  PROP_PRINT_SETTINGS,
  PROP_SELECTED_PRINTER,
  PROP_MANUAL_CAPABILITIES,
  PROP_SUPPORT_SELECTION,
  PROP_HAS_SELECTION,
  PROP_EMBED_PAGE_SETUP
};

struct GtkPrintUnixDialogPrivate
{
  GtkWidget *notebook;

  GtkWidget *printer_treeview;
  GtkTreeViewColumn *printer_icon_column;
  GtkTreeViewColumn *printer_name_column;
  GtkTreeViewColumn *printer_location_column;
  GtkTreeViewColumn *printer_status_column;
  GtkCellRenderer *printer_icon_renderer;
  GtkCellRenderer *printer_name_renderer;
  GtkCellRenderer *printer_location_renderer;
  GtkCellRenderer *printer_status_renderer;

  GtkPrintCapabilities manual_capabilities;
  GtkPrintCapabilities printer_capabilities;

  GtkTreeModel *printer_list;
  GtkTreeModelFilter *printer_list_filter;

  GtkPageSetup *page_setup;
  gboolean page_setup_set;
  gboolean embed_page_setup;
  GtkListStore *page_setup_list;
  GtkListStore *custom_paper_list;

  gboolean support_selection;
  gboolean has_selection;

  GtkWidget *all_pages_radio;
  GtkWidget *current_page_radio;
  GtkWidget *selection_radio;
  GtkWidget *range_table;
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
  GtkWidget *paper_size_combo;
  GtkWidget *paper_size_combo_label;
  GtkCellRenderer *paper_size_renderer;
  GtkWidget *orientation_combo;
  GtkWidget *orientation_combo_label;
  gboolean internal_page_setup_change;
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
  GtkPrinterOptionWidget *number_up_layout;

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
   * printer taken from set settings, or user-selected), but when any
   * setting is changed by the user it is cleared.
   */
  GtkPrintSettings *initial_settings;

  GtkPrinterOption *number_up_layout_n_option;
  GtkPrinterOption *number_up_layout_2_option;

  /* This is the initial printer set by set_settings. We look for it in
   * the added printers. We clear this whenever the user manually changes
   * to another printer, when the user changes a setting or when we find
   * this printer.
   */
  char *waiting_for_printer;
  gboolean internal_printer_change;

  GList *print_backends;

  GtkPrinter *current_printer;
  GtkPrinter *request_details_printer;
  gulong request_details_tag;
  GtkPrinterOptionSet *options;
  gulong options_changed_handler;
  gulong mark_conflicts_id;

  gchar *format_for_printer;

  gint current_page;
};

G_DEFINE_TYPE_WITH_CODE (GtkPrintUnixDialog, gtk_print_unix_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (GtkPrintUnixDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_print_unix_dialog_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

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

  object_class->constructed = gtk_print_unix_dialog_constructed;
  object_class->finalize = gtk_print_unix_dialog_finalize;
  object_class->set_property = gtk_print_unix_dialog_set_property;
  object_class->get_property = gtk_print_unix_dialog_get_property;

  widget_class->style_updated = gtk_print_unix_dialog_style_updated;
  widget_class->destroy = gtk_print_unix_dialog_destroy;

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

  g_object_class_install_property (object_class,
                                   PROP_MANUAL_CAPABILITIES,
                                   g_param_spec_flags ("manual-capabilities",
                                                       P_("Manual Capabilities"),
                                                       P_("Capabilities the application can handle"),
                                                       GTK_TYPE_PRINT_CAPABILITIES,
                                                       0,
                                                       GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SUPPORT_SELECTION,
                                   g_param_spec_boolean ("support-selection",
                                                         P_("Support Selection"),
                                                         P_("Whether the dialog supports selection"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_HAS_SELECTION,
                                   g_param_spec_boolean ("has-selection",
                                                         P_("Has Selection"),
                                                         P_("Whether the application has a selection"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

   g_object_class_install_property (object_class,
                                   PROP_EMBED_PAGE_SETUP,
                                   g_param_spec_boolean ("embed-page-setup",
                                                         P_("Embed Page Setup"),
                                                         P_("TRUE if page setup combos are embedded in GtkPrintUnixDialog"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkprintunixdialog.ui");

  /* GtkTreeView / GtkTreeModel */
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_treeview);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_list_filter);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, page_setup_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, custom_paper_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_icon_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_name_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_location_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_status_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_icon_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_name_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_location_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, printer_status_renderer);

  /* General Widgetry */
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, all_pages_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, all_pages_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, current_page_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, selection_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, range_table);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, page_range_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, page_range_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, copies_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, collate_check);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, reverse_check);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, collate_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, page_layout_preview);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, scale_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, page_set_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, print_now_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, print_at_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, print_at_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, print_hold_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, paper_size_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, paper_size_combo_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, paper_size_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, orientation_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, orientation_combo_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, conflicts_widget);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, job_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, finishing_table);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, finishing_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, image_quality_table);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, image_quality_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, color_table);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, color_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, advanced_vbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, advanced_page);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, extension_point);

  /* GtkPrinterOptionWidgets... */
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, pages_per_sheet);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, duplex);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, paper_type);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, paper_source);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, output_tray);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, job_prio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, billing_info);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, cover_before);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, cover_after);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPrintUnixDialog, number_up_layout);

  /* Callbacks handled in the UI */
  gtk_widget_class_bind_template_callback (widget_class, redraw_page_layout_preview);
  gtk_widget_class_bind_template_callback (widget_class, error_dialogs);
  gtk_widget_class_bind_template_callback (widget_class, emit_ok_response);
  gtk_widget_class_bind_template_callback (widget_class, selected_printer_changed);
  gtk_widget_class_bind_template_callback (widget_class, update_page_range_entry_sensitivity);
  gtk_widget_class_bind_template_callback (widget_class, update_print_at_entry_sensitivity);
  gtk_widget_class_bind_template_callback (widget_class, update_print_at_option);
  gtk_widget_class_bind_template_callback (widget_class, update_dialog_from_capabilities);
  gtk_widget_class_bind_template_callback (widget_class, update_collate_icon);
  gtk_widget_class_bind_template_callback (widget_class, draw_collate_cb);
  gtk_widget_class_bind_template_callback (widget_class, redraw_page_layout_preview);
  gtk_widget_class_bind_template_callback (widget_class, update_number_up_layout);
  gtk_widget_class_bind_template_callback (widget_class, redraw_page_layout_preview);
  gtk_widget_class_bind_template_callback (widget_class, draw_page_cb);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel = NULL;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return NULL;
  else
    return GTK_WINDOW (toplevel);
}

static void
set_busy_cursor (GtkPrintUnixDialog *dialog,
                 gboolean            busy)
{
  GtkWidget *widget;
  GtkWindow *toplevel;
  GdkDisplay *display;
  GdkCursor *cursor;

  toplevel = get_toplevel (GTK_WIDGET (dialog));
  widget = GTK_WIDGET (toplevel);

  if (!toplevel || !gtk_widget_get_realized (widget))
    return;

  display = gtk_widget_get_display (widget);

  if (busy)
    cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
  else
    cursor = NULL;

  gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
  gdk_display_flush (display);

  if (cursor)
    g_object_unref (cursor);
}

/* This function handles error messages before printing.
 */
static gboolean
error_dialogs (GtkPrintUnixDialog *print_dialog,
               gint                print_dialog_response_id,
               gpointer            data)
{
  GtkPrintUnixDialogPrivate *priv = print_dialog->priv;
  GtkPrinterOption          *option = NULL;
  GtkPrinter                *printer = NULL;
  GtkWindow                 *toplevel = NULL;
  GtkWidget                 *dialog = NULL;
  GFile                     *file = NULL;
  gchar                     *basename = NULL;
  gchar                     *dirname = NULL;
  int                        response;

  if (print_dialog != NULL && print_dialog_response_id == GTK_RESPONSE_OK)
    {
      printer = gtk_print_unix_dialog_get_selected_printer (print_dialog);

      if (printer != NULL)
        {
          if (priv->request_details_tag || !gtk_printer_is_accepting_jobs (printer))
            {
              g_signal_stop_emission_by_name (print_dialog, "response");
              return TRUE;
            }

          /* Shows overwrite confirmation dialog in the case of printing
           * to file which already exists.
           */
          if (gtk_printer_is_virtual (printer))
            {
              option = gtk_printer_option_set_lookup (priv->options,
                                                      "gtk-main-page-custom-input");

              if (option != NULL &&
                  option->type == GTK_PRINTER_OPTION_TYPE_FILESAVE)
                {
                  file = g_file_new_for_uri (option->value);

                  if (g_file_query_exists (file, NULL))
                    {
                      GFile *parent;

                      toplevel = get_toplevel (GTK_WIDGET (print_dialog));

                      basename = g_file_get_basename (file);
                      parent = g_file_get_parent (file);
                      dirname = g_file_get_parse_name (parent);
                      g_object_unref (parent);

                      dialog = gtk_message_dialog_new (toplevel,
                                                       GTK_DIALOG_MODAL |
                                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                                       GTK_MESSAGE_QUESTION,
                                                       GTK_BUTTONS_NONE,
                                                       _("A file named “%s” already exists.  Do you want to replace it?"),
                                                       basename);

                      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                                _("The file already exists in “%s”.  Replacing it will "
                                                                "overwrite its contents."),
                                                                dirname);

                      gtk_dialog_add_button (GTK_DIALOG (dialog),
                                             _("_Cancel"),
                                             GTK_RESPONSE_CANCEL);
                      gtk_dialog_add_button (GTK_DIALOG (dialog),
                                             _("_Replace"),
                                             GTK_RESPONSE_ACCEPT);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                      gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                                               GTK_RESPONSE_ACCEPT,
                                                               GTK_RESPONSE_CANCEL,
                                                               -1);
G_GNUC_END_IGNORE_DEPRECATIONS
                      gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                                       GTK_RESPONSE_ACCEPT);

                      if (gtk_window_has_group (toplevel))
                        gtk_window_group_add_window (gtk_window_get_group (toplevel),
                                                     GTK_WINDOW (dialog));

                      response = gtk_dialog_run (GTK_DIALOG (dialog));

                      gtk_widget_destroy (dialog);

                      g_free (dirname);
                      g_free (basename);

                      if (response != GTK_RESPONSE_ACCEPT)
                        {
                          g_signal_stop_emission_by_name (print_dialog, "response");
                          g_object_unref (file);
                          return TRUE;
                        }
                    }

                  g_object_unref (file);
                }
            }
        }
    }
  return FALSE;
}

static void
gtk_print_unix_dialog_init (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv;
  GtkTreeSortable *sort;
  GtkWidget *widget;

  dialog->priv = gtk_print_unix_dialog_get_instance_private (dialog);
  priv = dialog->priv;

  priv->print_backends = NULL;
  priv->current_page = -1;
  priv->number_up_layout_n_option = NULL;
  priv->number_up_layout_2_option = NULL;

  priv->page_setup = gtk_page_setup_new ();
  priv->page_setup_set = FALSE;
  priv->embed_page_setup = FALSE;
  priv->internal_page_setup_change = FALSE;

  priv->support_selection = FALSE;
  priv->has_selection = FALSE;

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Pre_view"), GTK_RESPONSE_APPLY,
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Print"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  widget = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_widget_set_sensitive (widget, FALSE);

  /* Treeview auxilary functions need to be setup here  */
  gtk_tree_model_filter_set_visible_func (priv->printer_list_filter,
                                          (GtkTreeModelFilterVisibleFunc) is_printer_active,
                                          dialog,
                                          NULL);

  sort = GTK_TREE_SORTABLE (priv->printer_list);
  gtk_tree_sortable_set_default_sort_func (sort,
                                           default_printer_list_sort_func,
                                           NULL,
                                           NULL);

  gtk_tree_sortable_set_sort_column_id (sort,
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);

  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (priv->printer_treeview),
                                       printer_compare, NULL, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->printer_icon_column,
					   priv->printer_icon_renderer,
					   set_cell_sensitivity_func, NULL, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->printer_name_column,
					   priv->printer_name_renderer,
					   set_cell_sensitivity_func, NULL, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->printer_location_column,
					   priv->printer_location_renderer,
					   set_cell_sensitivity_func, NULL, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->printer_status_column,
					   priv->printer_status_renderer,
					   set_cell_sensitivity_func, NULL, NULL);


  /* Paper size combo auxilary funcs */
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (priv->paper_size_combo),
                                        paper_size_row_is_separator, NULL, NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->paper_size_combo),
				      priv->paper_size_renderer,
                                      page_name_func, NULL, NULL);

  /* Preview drawing area has no window */
  gtk_widget_set_has_window (priv->page_layout_preview, FALSE);

  /* Load backends */
  load_print_backends (dialog);

  /* Load custom papers */
  _gtk_print_load_custom_papers (priv->custom_paper_list);
}

static void
gtk_print_unix_dialog_constructed (GObject *object)
{
  gboolean use_header;

  G_OBJECT_CLASS (gtk_print_unix_dialog_parent_class)->constructed (object);

  g_object_get (object, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
       /* Reorder the preview button */
       GtkWidget *button, *parent;
       button = gtk_dialog_get_widget_for_response (GTK_DIALOG (object), GTK_RESPONSE_APPLY);
       g_object_ref (button);
       parent = gtk_widget_get_parent (button);
       gtk_container_remove (GTK_CONTAINER (parent), button); 
       gtk_header_bar_pack_end (GTK_HEADER_BAR (parent), button);
       g_object_unref (button);
    }
}

static void
gtk_print_unix_dialog_destroy (GtkWidget *widget)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (widget);

  /* Make sure we don't destroy custom widgets owned by the backends */
  clear_per_printer_ui (dialog);

  GTK_WIDGET_CLASS (gtk_print_unix_dialog_parent_class)->destroy (widget);
}

static void
disconnect_printer_details_request (GtkPrintUnixDialog *dialog,
                                    gboolean            details_failed)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  if (priv->request_details_tag)
    {
      g_signal_handler_disconnect (priv->request_details_printer,
                                   priv->request_details_tag);
      priv->request_details_tag = 0;
      set_busy_cursor (dialog, FALSE);
      if (details_failed)
        gtk_list_store_set (GTK_LIST_STORE (priv->printer_list),
                            g_object_get_data (G_OBJECT (priv->request_details_printer),
                                               "gtk-print-tree-iter"),
                            PRINTER_LIST_COL_STATE,
                             _("Getting printer information failed"),
                            -1);
      else
        gtk_list_store_set (GTK_LIST_STORE (priv->printer_list),
                            g_object_get_data (G_OBJECT (priv->request_details_printer),
                                               "gtk-print-tree-iter"),
                            PRINTER_LIST_COL_STATE,
                            gtk_printer_get_state_message (priv->request_details_printer),
                            -1);
      g_object_unref (priv->request_details_printer);
      priv->request_details_printer = NULL;
    }
}

static void
gtk_print_unix_dialog_finalize (GObject *object)
{
  GtkPrintUnixDialog *dialog = GTK_PRINT_UNIX_DIALOG (object);
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrintBackend *backend;
  GList *node;

  unschedule_idle_mark_conflicts (dialog);
  disconnect_printer_details_request (dialog, FALSE);

  g_clear_object (&priv->current_printer);
  g_clear_object (&priv->options);

  if (priv->number_up_layout_2_option)
    {
      priv->number_up_layout_2_option->choices[0] = NULL;
      priv->number_up_layout_2_option->choices[1] = NULL;
      g_free (priv->number_up_layout_2_option->choices_display[0]);
      g_free (priv->number_up_layout_2_option->choices_display[1]);
      priv->number_up_layout_2_option->choices_display[0] = NULL;
      priv->number_up_layout_2_option->choices_display[1] = NULL;
      g_object_unref (priv->number_up_layout_2_option);
      priv->number_up_layout_2_option = NULL;
    }

  g_clear_object (&priv->number_up_layout_n_option);
  g_clear_object (&priv->page_setup);
  g_clear_object (&priv->initial_settings);
  g_clear_pointer (&priv->waiting_for_printer, (GDestroyNotify)g_free);
  g_clear_pointer (&priv->format_for_printer, (GDestroyNotify)g_free);

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

  g_clear_object (&priv->page_setup_list);

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
gtk_print_unix_dialog_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->get_internal_child = gtk_print_unix_dialog_buildable_get_internal_child;
}

static GObject *
gtk_print_unix_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                    GtkBuilder   *builder,
                                                    const gchar  *childname)
{
  if (strcmp (childname, "notebook") == 0)
    return G_OBJECT (GTK_PRINT_UNIX_DIALOG (buildable)->priv->notebook);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/* This function controls "sensitive" property of GtkCellRenderer
 * based on pause state of printers.
 */
void set_cell_sensitivity_func (GtkTreeViewColumn *tree_column,
                                GtkCellRenderer   *cell,
                                GtkTreeModel      *tree_model,
                                GtkTreeIter       *iter,
                                gpointer           data)
{
  GtkPrinter *printer;

  gtk_tree_model_get (tree_model, iter,
                      PRINTER_LIST_COL_PRINTER_OBJ, &printer,
                      -1);

  if (printer != NULL && !gtk_printer_is_accepting_jobs (printer))
    g_object_set (cell, "sensitive", FALSE, NULL);
  else
    g_object_set (cell, "sensitive", TRUE, NULL);

  g_object_unref (printer);
}

static void
printer_status_cb (GtkPrintBackend    *backend,
                   GtkPrinter         *printer,
                   GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter *iter;
  GtkTreeSelection *selection;

  iter = g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter");

  gtk_list_store_set (GTK_LIST_STORE (priv->printer_list), iter,
                      PRINTER_LIST_COL_ICON, gtk_printer_get_icon_name (printer),
                      PRINTER_LIST_COL_STATE, gtk_printer_get_state_message (printer),
                      PRINTER_LIST_COL_JOBS, gtk_printer_get_job_count (printer),
                      PRINTER_LIST_COL_LOCATION, gtk_printer_get_location (printer),
                      -1);

  /* When the pause state change then we need to update sensitive property
   * of GTK_RESPONSE_OK button inside of selected_printer_changed function.
   */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->printer_treeview));
  priv->internal_printer_change = TRUE;
  selected_printer_changed (selection, dialog);
  priv->internal_printer_change = FALSE;

  if (gtk_print_backend_printer_list_is_done (backend) &&
      gtk_printer_is_default (printer) &&
      (gtk_tree_selection_count_selected_rows (selection) == 0))
    set_active_printer (dialog, gtk_printer_get_name (printer));
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
      strcmp (gtk_printer_get_name (printer), priv->waiting_for_printer) == 0)
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

  g_signal_connect_object (print_backend, "printer-added",
                           (GCallback) printer_added_cb, G_OBJECT (dialog), 0);

  g_signal_connect_object (print_backend, "printer-removed",
                           (GCallback) printer_removed_cb, G_OBJECT (dialog), 0);

  g_signal_connect_object (print_backend, "printer-status-changed",
                           (GCallback) printer_status_cb, G_OBJECT (dialog), 0);

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
    {
      GtkPrintBackend *backend = node->data;
      printer_list_initialize (dialog, backend);
    }
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
    case PROP_MANUAL_CAPABILITIES:
      gtk_print_unix_dialog_set_manual_capabilities (dialog, g_value_get_flags (value));
      break;
    case PROP_SUPPORT_SELECTION:
      gtk_print_unix_dialog_set_support_selection (dialog, g_value_get_boolean (value));
      break;
    case PROP_HAS_SELECTION:
      gtk_print_unix_dialog_set_has_selection (dialog, g_value_get_boolean (value));
      break;
    case PROP_EMBED_PAGE_SETUP:
      gtk_print_unix_dialog_set_embed_page_setup (dialog, g_value_get_boolean (value));
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
      g_value_take_object (value, gtk_print_unix_dialog_get_settings (dialog));
      break;
    case PROP_SELECTED_PRINTER:
      g_value_set_object (value, priv->current_printer);
      break;
    case PROP_MANUAL_CAPABILITIES:
      g_value_set_flags (value, priv->manual_capabilities);
      break;
    case PROP_SUPPORT_SELECTION:
      g_value_set_boolean (value, priv->support_selection);
      break;
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, priv->has_selection);
      break;
    case PROP_EMBED_PAGE_SETUP:
      g_value_set_boolean (value, priv->embed_page_setup);
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

  gtk_tree_model_get (model, iter,
                      PRINTER_LIST_COL_PRINTER_OBJ, &printer,
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
  if (a_printer)
    g_object_unref (a_printer);
  if (b_printer)
    g_object_unref (b_printer);

  return result;
}

static GtkWidget *
wrap_in_frame (const gchar *label,
               GtkWidget   *child)
{
  GtkWidget *frame, *label_widget;
  gchar *bold_text;

  label_widget = gtk_label_new (NULL);
  gtk_widget_set_halign (label_widget, GTK_ALIGN_START);
  gtk_widget_set_valign (label_widget, GTK_ALIGN_CENTER);
  gtk_widget_show (label_widget);

  bold_text = g_markup_printf_escaped ("<b>%s</b>", label);
  gtk_label_set_markup (GTK_LABEL (label_widget), bold_text);
  g_free (bold_text);

  frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (frame), label_widget, FALSE, FALSE, 0);

  gtk_widget_set_margin_start (child, 12);
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_widget_set_valign (child, GTK_ALIGN_FILL);

  gtk_box_pack_start (GTK_BOX (frame), child, FALSE, FALSE, 0);

  gtk_widget_show (frame);

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
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      gtk_box_pack_start (GTK_BOX (extension_point), hbox, TRUE, TRUE, 0);
    }
  else
    gtk_box_pack_start (GTK_BOX (extension_point), widget, TRUE, TRUE, 0);
}

static gint
grid_rows (GtkGrid *table)
{
  gint t0, t1, t, h;
  GList *children, *c;

  children = gtk_container_get_children (GTK_CONTAINER (table));
  t0 = t1 = 0;
  for (c = children; c; c = c->next)
    {
      gtk_container_child_get (GTK_CONTAINER (table), c->data,
                               "top-attach", &t,
                               "height", &h,
                               NULL);
      if (c == children)
        {
          t0 = t;
          t1 = t + h;
        }
      else
        {
          if (t < t0)
            t0 = t;
          if (t + h > t1)
            t1 = t + h;
        }
    }
  g_list_free (children);

  return t1 - t0;
}

static void
add_option_to_table (GtkPrinterOption *option,
                     gpointer          user_data)
{
  GtkGrid *table;
  GtkWidget *label, *widget;
  guint row;

  table = GTK_GRID (user_data);

  if (g_str_has_prefix (option->name, "gtk-"))
    return;

  row = grid_rows (table);

  widget = gtk_printer_option_widget_new (option);
  gtk_widget_show (widget);

  if (gtk_printer_option_widget_has_external_label (GTK_PRINTER_OPTION_WIDGET (widget)))
    {
      label = gtk_printer_option_widget_get_external_label (GTK_PRINTER_OPTION_WIDGET (widget));
      gtk_widget_show (label);

      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

      gtk_grid_attach (table, label, 0, row - 1, 1, 1);
      gtk_grid_attach (table, widget, 1, row - 1, 1, 1);
    }
  else
    gtk_grid_attach (table, widget, 0, row - 1, 2, 1);
}

static void
setup_page_table (GtkPrinterOptionSet *options,
                  const gchar         *group,
                  GtkWidget           *table,
                  GtkWidget           *page)
{
  gint nrows;

  gtk_printer_option_set_foreach_in_group (options, group,
                                           add_option_to_table,
                                           table);

  nrows = grid_rows (GTK_GRID (table));
  if (nrows == 0)
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
      const gchar *text;

      text = gtk_entry_get_text (GTK_ENTRY (priv->print_at_entry));
      gtk_printer_option_set (option, text);
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

  gtk_widget_set_sensitive (priv->print_at_entry, FALSE);
  gtk_widget_set_sensitive (priv->print_at_radio,
                            gtk_printer_option_has_choice (option, "at"));

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
    gtk_entry_set_text (GTK_ENTRY (priv->print_at_entry), option->value);

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
  guint nrows;
  GList *children;

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
  setup_option (dialog, "gtk-n-up-layout", priv->number_up_layout);
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

  /* A bit of a hack, keep the last option flush right.
   * This keeps the file format radios from moving as the
   * filename changes.
   */
  children = gtk_container_get_children (GTK_CONTAINER (priv->extension_point));
  l = g_list_last (children);
  if (l && l != children)
    gtk_widget_set_halign (GTK_WIDGET (l->data), GTK_ALIGN_END);
  g_list_free (children);

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

      table = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (table), 6);
      gtk_grid_set_column_spacing (GTK_GRID (table), 12);

      gtk_printer_option_set_foreach_in_group (priv->options,
                                               group,
                                               add_option_to_table,
                                               table);

      nrows = grid_rows (GTK_GRID (table));
      if (nrows == 0)
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

  g_list_free_full (groups, g_free);
}

static void
update_dialog_from_capabilities (GtkPrintUnixDialog *dialog)
{
  GtkPrintCapabilities caps;
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  gboolean can_collate;
  const gchar *copies;
  GtkWidget *button;

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
  gtk_widget_set_sensitive (GTK_WIDGET (priv->pages_per_sheet),
                            caps & GTK_PRINT_CAPABILITY_NUMBER_UP);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_APPLY);
  gtk_widget_set_visible (button, (caps & GTK_PRINT_CAPABILITY_PREVIEW) != 0);

  update_collate_icon (NULL, dialog);

  gtk_tree_model_filter_refilter (priv->printer_list_filter);
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
set_paper_size (GtkPrintUnixDialog *dialog,
                GtkPageSetup       *page_setup,
                gboolean            size_only,
                gboolean            add_item)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkPageSetup *list_page_setup;

  if (!priv->internal_page_setup_change)
    return TRUE;

  if (page_setup == NULL)
    return FALSE;

  model = GTK_TREE_MODEL (priv->page_setup_list);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->page_setup_list), &iter,
                              PAGE_SETUP_LIST_COL_PAGE_SETUP, &list_page_setup,
                              -1);
          if (list_page_setup == NULL)
            continue;

          if ((size_only && page_setup_is_same_size (page_setup, list_page_setup)) ||
              (!size_only && page_setup_is_equal (page_setup, list_page_setup)))
            {
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->paper_size_combo),
                                             &iter);
              gtk_combo_box_set_active (GTK_COMBO_BOX (priv->orientation_combo),
                                        gtk_page_setup_get_orientation (page_setup));
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
      gtk_combo_box_set_active (GTK_COMBO_BOX (priv->orientation_combo),
                                gtk_page_setup_get_orientation (page_setup));
      return TRUE;
    }

  return FALSE;
}

static void
fill_custom_paper_sizes (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
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
fill_paper_sizes (GtkPrintUnixDialog *dialog,
                  GtkPrinter         *printer)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GList *list, *l;
  GtkPageSetup *page_setup;
  GtkPaperSize *paper_size;
  GtkTreeIter iter;
  gint i;

  gtk_list_store_clear (priv->page_setup_list);

  if (printer == NULL || (list = gtk_printer_list_papers (printer)) == NULL)
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
}

static void
update_paper_sizes (GtkPrintUnixDialog *dialog)
{
  GtkPageSetup *current_page_setup = NULL;
  GtkPrinter   *printer;

  printer = gtk_print_unix_dialog_get_selected_printer (dialog);

  fill_paper_sizes (dialog, printer);

  current_page_setup = gtk_page_setup_copy (gtk_print_unix_dialog_get_page_setup (dialog));

  if (current_page_setup)
    {
      if (!set_paper_size (dialog, current_page_setup, FALSE, FALSE))
        set_paper_size (dialog, current_page_setup, TRUE, TRUE);

      g_object_unref (current_page_setup);
    }
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

  priv->mark_conflicts_id = gdk_threads_add_idle (mark_conflicts_callback,
                                        dialog);
  g_source_set_name_by_id (priv->mark_conflicts_id, "[gtk+] mark_conflicts_callback");
}

static void
options_changed_cb (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  schedule_idle_mark_conflicts (dialog);

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

  if (priv->finishing_table == NULL)
    return;

  gtk_container_foreach (GTK_CONTAINER (priv->finishing_table),
                         (GtkCallback)gtk_widget_destroy, NULL);
  gtk_container_foreach (GTK_CONTAINER (priv->image_quality_table),
                         (GtkCallback)gtk_widget_destroy, NULL);
  gtk_container_foreach (GTK_CONTAINER (priv->color_table),
                         (GtkCallback)gtk_widget_destroy, NULL);
  gtk_container_foreach (GTK_CONTAINER (priv->advanced_vbox),
                         (GtkCallback)gtk_widget_destroy, NULL);
  extension_point_clear_children (GTK_CONTAINER (priv->extension_point));
}

static void
printer_details_acquired (GtkPrinter         *printer,
                          gboolean            success,
                          GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;

  disconnect_printer_details_request (dialog, !success);

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
   * the printer specified in the initial settings
   */
  if (priv->waiting_for_printer &&
      !priv->internal_printer_change)
    {
      g_free (priv->waiting_for_printer);
      priv->waiting_for_printer = NULL;
    }

  disconnect_printer_details_request (dialog, FALSE);

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

  /* sets GTK_RESPONSE_OK button sensitivity depending on whether the printer
   * accepts/rejects jobs
   */
  if (printer != NULL)
    {
      if (!gtk_printer_is_accepting_jobs (printer))
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
      else if (priv->current_printer == printer && gtk_printer_has_details (printer))

        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
    }

  if (printer != NULL && !gtk_printer_has_details (printer))
    {
      gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
      priv->request_details_tag =
        g_signal_connect (printer, "details-acquired",
                          G_CALLBACK (printer_details_acquired), dialog);
      /* take the reference */
      priv->request_details_printer = printer;
      set_busy_cursor (dialog, TRUE);
      gtk_list_store_set (GTK_LIST_STORE (priv->printer_list),
                          g_object_get_data (G_OBJECT (printer), "gtk-print-tree-iter"),
                          PRINTER_LIST_COL_STATE, _("Getting printer information…"),
                          -1);
      gtk_printer_request_details (printer);
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
      g_clear_object (&priv->options);
      clear_per_printer_ui (dialog);
    }

  g_clear_object (&priv->current_printer);
  priv->printer_capabilities = 0;

  if (printer != NULL && gtk_printer_is_accepting_jobs (printer))
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
  priv->current_printer = printer;

  if (printer != NULL)
    {
      if (!priv->page_setup_set)
        {
          /* if no explicit page setup has been set, use the printer default */
          GtkPageSetup *page_setup;

          page_setup = gtk_printer_get_default_page_size (printer);

          if (!page_setup)
            page_setup = gtk_page_setup_new ();

          if (page_setup && priv->page_setup)
            gtk_page_setup_set_orientation (page_setup, gtk_page_setup_get_orientation (priv->page_setup));

          g_object_unref (priv->page_setup);
          priv->page_setup = page_setup;
        }

      priv->printer_capabilities = gtk_printer_get_capabilities (printer);
      priv->options = _gtk_printer_get_options (printer,
                                                priv->initial_settings,
                                                priv->page_setup,
                                                priv->manual_capabilities);

      priv->options_changed_handler =
        g_signal_connect_swapped (priv->options, "changed", G_CALLBACK (options_changed_cb), dialog);
      schedule_idle_mark_conflicts (dialog);
    }

  update_dialog_from_settings (dialog);
  update_dialog_from_capabilities (dialog);

  priv->internal_page_setup_change = TRUE;
  update_paper_sizes (dialog);
  priv->internal_page_setup_change = FALSE;

  g_object_notify ( G_OBJECT(dialog), "selected-printer");
}

static gboolean
printer_compare (GtkTreeModel *model,
                 gint          column,
                 const gchar  *key,
                 GtkTreeIter  *iter,
                 gpointer      search_data)
{
  gboolean matches = FALSE;

  if (key != NULL)
    {
      gchar  *name = NULL;
      gchar  *location = NULL;
      gchar  *casefold_key = NULL;
      gchar  *casefold_name = NULL;
      gchar  *casefold_location = NULL;
      gchar **keys;
      gchar  *tmp1, *tmp2;
      gint    i;

      gtk_tree_model_get (model, iter,
                          PRINTER_LIST_COL_NAME, &name,
                          PRINTER_LIST_COL_LOCATION, &location,
                          -1);

      casefold_key = g_utf8_casefold (key, -1);

      if (name != NULL)
        {
          casefold_name = g_utf8_casefold (name, -1);
          g_free (name);
        }

      if (location != NULL)
        {
          casefold_location = g_utf8_casefold (location, -1);
          g_free (location);
        }

      if (casefold_name != NULL ||
          casefold_location != NULL)
        {
          keys = g_strsplit_set (casefold_key, " \t", 0);
          if (keys != NULL)
            {
              matches = TRUE;

              for (i = 0; keys[i] != NULL; i++)
                {
                  if (keys[i][0] != '\0')
                    {
                      tmp1 = tmp2 = NULL;

                      if (casefold_name != NULL)
                        tmp1 = g_strstr_len (casefold_name, -1, keys[i]);

                      if (casefold_location != NULL)
                        tmp2 = g_strstr_len (casefold_location, -1, keys[i]);

                      if (tmp1 == NULL && tmp2 == NULL)
                        {
                          matches = FALSE;
                          break;
                        }
                    }
                }

              g_strfreev (keys);
            }
        }

      g_free (casefold_location);
      g_free (casefold_name);
      g_free (casefold_key);
    }

  return !matches;
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
            gint       x,
            gint       y,
            gchar     *text,
            gint       text_x)
{
  GtkStyleContext *context;
  gint width, height;
  gint text_y;

  width = 20;
  height = 26;
  text_y = 21;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  cairo_select_font_face (cr, "Sans",
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, 9);
  cairo_move_to (cr, x + text_x, y + text_y);
  cairo_show_text (cr, text);

  gtk_style_context_restore (context);
}

static gboolean
draw_collate_cb (GtkWidget          *widget,
                 cairo_t            *cr,
                 GtkPrintUnixDialog *dialog)
{
  gboolean collate, reverse, rtl;
  gint copies;
  gint text_x;
  gint x, y, x1, x2, p1, p2;

  collate = dialog_get_collate (dialog);
  reverse = dialog_get_reverse (dialog);
  copies = dialog_get_n_copies (dialog);

  rtl = (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL);

  x = (gtk_widget_get_allocated_width (widget) - 30) / 2;
  y = (gtk_widget_get_allocated_height (widget) - 36) / 2;
  if (rtl)
    {
      x1 = x;
      x2 = x - 36;
      p1 = 0;
      p2 = 10;
      text_x = 4;
    }
  else
    {
      x1 = x;
      x2 = x + 36;
      p1 = 10;
      p2 = 0;
      text_x = 11;
    }

  if (copies == 1)
    {
      paint_page (widget, cr, x1 + p1, y, reverse ? "1" : "2", text_x);
      paint_page (widget, cr, x1 + p2, y + 10, reverse ? "2" : "1", text_x);
    }
  else
    {
      paint_page (widget, cr, x1 + p1, y, collate == reverse ? "1" : "2", text_x);
      paint_page (widget, cr, x1 + p2, y + 10, reverse ? "2" : "1", text_x);

      paint_page (widget, cr, x2 + p1, y, reverse ? "1" : "2", text_x);
      paint_page (widget, cr, x2 + p2, y + 10, collate == reverse ? "2" : "1", text_x);
    }    

  return TRUE;
}

static void
gtk_print_unix_dialog_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_print_unix_dialog_parent_class)->style_updated (widget);

  if (gtk_widget_has_screen (widget))
    {
      GtkPrintUnixDialog *dialog = (GtkPrintUnixDialog *)widget;
      GtkPrintUnixDialogPrivate *priv = dialog->priv;
      gint size;
      gfloat scale;

      gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &size, NULL);
      scale = size / 48.0;

      gtk_widget_set_size_request (priv->collate_image,
                                   (50 + 20) * scale,
                                   (15 + 26) * scale);
    }
}

static void
update_page_range_entry_sensitivity (GtkWidget *button,
				     GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  gtk_widget_set_sensitive (priv->page_range_entry, active);

  if (active)
    gtk_widget_grab_focus (priv->page_range_entry);
}

static void
update_print_at_entry_sensitivity (GtkWidget *button,
				   GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  gtk_widget_set_sensitive (priv->print_at_entry, active);

  if (active)
    gtk_widget_grab_focus (priv->print_at_entry);
}

static void
emit_ok_response (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer          *user_data)
{
  GtkPrintUnixDialog *print_dialog;

  print_dialog = (GtkPrintUnixDialog *) user_data;

  gtk_dialog_response (GTK_DIALOG (print_dialog), GTK_RESPONSE_OK);
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
      while (isspace (*p)) p++;

      if (*p == '-')
        {
          /* a half-open range like -2 */
          start = 1;
        }
      else
        {
          start = (int)strtol (p, &next, 10);
          if (start < 1)
            start = 1;
          p = next;
        }

      end = start;

      while (isspace (*p)) p++;

      if (*p == '-')
        {
          p++;
          end = (int)strtol (p, &next, 10);
          if (next == p) /* a half-open range like 2- */
            end = 0;
          else if (end < start)
            end = start;
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
  GString *s = g_string_new (NULL);

  for (i = 0; i < n_ranges; i++)
    {
      g_string_append_printf (s, "%d", ranges[i].start + 1);
      if (ranges[i].end > ranges[i].start)
        g_string_append_printf (s, "-%d", ranges[i].end + 1);
      else if (ranges[i].end == -1)
        g_string_append (s, "-");

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
  else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->selection_radio)))
    return GTK_PRINT_PAGES_SELECTION;
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
  else if (pages == GTK_PRINT_PAGES_SELECTION)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selection_radio), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_pages_radio), TRUE);
}

static gdouble
dialog_get_scale (GtkPrintUnixDialog *dialog)
{
  if (gtk_widget_is_sensitive (dialog->priv->scale_spin))
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
  if (gtk_widget_is_sensitive (dialog->priv->page_set_combo))
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
  if (gtk_widget_is_sensitive (dialog->priv->copies_spin))
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
  if (gtk_widget_is_sensitive (dialog->priv->collate_check))
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
  if (gtk_widget_is_sensitive (dialog->priv->reverse_check))
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

static GtkNumberUpLayout
dialog_get_number_up_layout (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrintCapabilities       caps;
  GtkNumberUpLayout          layout;
  const gchar               *val;
  GEnumClass                *enum_class;
  GEnumValue                *enum_value;

  val = gtk_printer_option_widget_get_value (priv->number_up_layout);

  caps = priv->manual_capabilities | priv->printer_capabilities;

  if ((caps & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT) == 0)
    return GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;

  if (gtk_widget_get_direction (GTK_WIDGET (dialog)) == GTK_TEXT_DIR_LTR)
    layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
  else
    layout = GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM;

  if (val == NULL)
    return layout;

  if (val[0] == '\0' && priv->options)
    {
      GtkPrinterOption *option = gtk_printer_option_set_lookup (priv->options, "gtk-n-up-layout");
      if (option)
        val = option->value;
    }

  enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);
  enum_value = g_enum_get_value_by_nick (enum_class, val);
  if (enum_value)
    layout = enum_value->value;
  g_type_class_unref (enum_class);

  return layout;
}

static gboolean
draw_page_cb (GtkWidget          *widget,
              cairo_t            *cr,
              GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkStyleContext *context;
  gdouble ratio;
  gint w, h, tmp, shadow_offset;
  gint pages_x, pages_y, i, x, y, layout_w, layout_h;
  gdouble page_width, page_height;
  GtkPageOrientation orientation;
  gboolean landscape;
  PangoLayout *layout;
  PangoFontDescription *font;
  gchar *text;
  GdkRGBA color;
  GtkNumberUpLayout number_up_layout;
  gint start_x, end_x, start_y, end_y;
  gint dx, dy;
  gint width, height;
  gboolean horizontal;
  GtkPageSetup *page_setup;
  gdouble paper_width, paper_height;
  gdouble pos_x, pos_y;
  gint pages_per_sheet;
  gboolean ltr = TRUE;
  GtkStateFlags state;

  orientation = gtk_page_setup_get_orientation (priv->page_setup);
  landscape =
    (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE) ||
    (orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE);

  number_up_layout = dialog_get_number_up_layout (dialog);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  state = gtk_widget_get_state_flags (widget);

  cairo_save (cr);

  page_setup = gtk_print_unix_dialog_get_page_setup (dialog);

  if (page_setup != NULL)
    {
      if (!landscape)
        {
          paper_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM);
          paper_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM);
        }
      else
        {
          paper_width = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_MM);
          paper_height = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_MM);
        }

      if (paper_width < paper_height)
        {
          h = EXAMPLE_PAGE_AREA_SIZE - 3;
          w = (paper_height != 0) ? h * paper_width / paper_height : 0;
        }
      else
        {
          w = EXAMPLE_PAGE_AREA_SIZE - 3;
          h = (paper_width != 0) ? w * paper_height / paper_width : 0;
        }

      if (paper_width == 0)
        w = 0;

      if (paper_height == 0)
        h = 0;
    }
  else
    {
      ratio = G_SQRT2;
      w = (EXAMPLE_PAGE_AREA_SIZE - 3) / ratio;
      h = EXAMPLE_PAGE_AREA_SIZE - 3;
    }

  pages_per_sheet = dialog_get_pages_per_sheet (dialog);
  switch (pages_per_sheet)
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

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

  pos_x = (width - w) / 2;
  pos_y = (height - h) / 2 - 10;
  cairo_translate (cr, pos_x, pos_y);

  shadow_offset = 3;

  gtk_style_context_get_color (context, state, &color);
  cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.5);
  cairo_rectangle (cr, shadow_offset + 1, shadow_offset + 1, w, h);
  cairo_fill (cr);

  gtk_render_background (context, cr, 1, 1, w, h);

  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr, 0.5, 0.5, w + 1, h + 1);
  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_stroke (cr);

  i = 1;

  page_width = (gdouble)w / pages_x;
  page_height = (gdouble)h / pages_y;

  layout  = pango_cairo_create_layout (cr);

  font = pango_font_description_new ();
  pango_font_description_set_family (font, "sans");

  if (page_height > 0)
    pango_font_description_set_absolute_size (font, page_height * 0.4 * PANGO_SCALE);
  else
    pango_font_description_set_absolute_size (font, 1);

  pango_layout_set_font_description (layout, font);
  pango_font_description_free (font);

  pango_layout_set_width (layout, page_width * PANGO_SCALE);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

  switch (number_up_layout)
    {
      default:
      case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = 0;
        end_y = pages_y - 1;
        dx = 1;
        dy = 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = pages_y - 1;
        end_y = 0;
        dx = 1;
        dy = - 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = 0;
        end_y = pages_y - 1;
        dx = - 1;
        dy = 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = pages_y - 1;
        end_y = 0;
        dx = - 1;
        dy = - 1;
        horizontal = TRUE;
        break;
      case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = 0;
        end_y = pages_y - 1;
        dx = 1;
        dy = 1;
        horizontal = FALSE;
        break;
      case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = 0;
        end_y = pages_y - 1;
        dx = - 1;
        dy = 1;
        horizontal = FALSE;
        break;
      case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT:
        start_x = 0;
        end_x = pages_x - 1;
        start_y = pages_y - 1;
        end_y = 0;
        dx = 1;
        dy = - 1;
        horizontal = FALSE;
        break;
      case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT:
        start_x = pages_x - 1;
        end_x = 0;
        start_y = pages_y - 1;
        end_y = 0;
        dx = - 1;
        dy = - 1;
        horizontal = FALSE;
        break;
    }

  if (horizontal)
    for (y = start_y; y != end_y + dy; y += dy)
      {
        for (x = start_x; x != end_x + dx; x += dx)
          {
            text = g_strdup_printf ("%d", i++);
            pango_layout_set_text (layout, text, -1);
            g_free (text);
            pango_layout_get_size (layout, &layout_w, &layout_h);
            cairo_save (cr);
            cairo_translate (cr,
                             x * page_width,
                             y * page_height + (page_height - layout_h / 1024.0) / 2);

            pango_cairo_show_layout (cr, layout);
            cairo_restore (cr);
          }
      }
  else
    for (x = start_x; x != end_x + dx; x += dx)
      {
        for (y = start_y; y != end_y + dy; y += dy)
          {
            text = g_strdup_printf ("%d", i++);
            pango_layout_set_text (layout, text, -1);
            g_free (text);
            pango_layout_get_size (layout, &layout_w, &layout_h);
            cairo_save (cr);
            cairo_translate (cr,
                             x * page_width,
                             y * page_height + (page_height - layout_h / 1024.0) / 2);

            pango_cairo_show_layout (cr, layout);
            cairo_restore (cr);
          }
      }

  g_object_unref (layout);

  if (page_setup != NULL)
    {
      PangoContext *pango_c = NULL;
      PangoFontDescription *pango_f = NULL;
      gint font_size = 12 * PANGO_SCALE;

      pos_x += 1;
      pos_y += 1;

      if (pages_per_sheet == 2 || pages_per_sheet == 6)
        {
          paper_width = gtk_page_setup_get_paper_height (page_setup, _gtk_print_get_default_user_units ());
          paper_height = gtk_page_setup_get_paper_width (page_setup, _gtk_print_get_default_user_units ());
        }
      else
        {
          paper_width = gtk_page_setup_get_paper_width (page_setup, _gtk_print_get_default_user_units ());
          paper_height = gtk_page_setup_get_paper_height (page_setup, _gtk_print_get_default_user_units ());
        }

      cairo_restore (cr);
      cairo_save (cr);

      layout = pango_cairo_create_layout (cr);

      font = pango_font_description_new ();
      pango_font_description_set_family (font, "sans");

      pango_c = gtk_widget_get_pango_context (widget);
      if (pango_c != NULL)
        {
          pango_f = pango_context_get_font_description (pango_c);
          if (pango_f != NULL)
            font_size = pango_font_description_get_size (pango_f);
        }

      pango_font_description_set_size (font, font_size);
      pango_layout_set_font_description (layout, font);
      pango_font_description_free (font);

      pango_layout_set_width (layout, -1);
      pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

      if (_gtk_print_get_default_user_units () == GTK_UNIT_MM)
        text = g_strdup_printf ("%.1f mm", paper_height);
      else
        text = g_strdup_printf ("%.2f inch", paper_height);

      pango_layout_set_text (layout, text, -1);
      g_free (text);
      pango_layout_get_size (layout, &layout_w, &layout_h);

      ltr = gtk_widget_get_direction (GTK_WIDGET (dialog)) == GTK_TEXT_DIR_LTR;

      if (ltr)
        cairo_translate (cr, pos_x - layout_w / PANGO_SCALE - 2 * RULER_DISTANCE,
                             (height - layout_h / PANGO_SCALE) / 2);
      else
        cairo_translate (cr, pos_x + w + shadow_offset + 2 * RULER_DISTANCE,
                             (height - layout_h / PANGO_SCALE) / 2);

      gtk_style_context_get_color (context, state, &color);
      gdk_cairo_set_source_rgba (cr, &color);
      pango_cairo_show_layout (cr, layout);

      cairo_restore (cr);
      cairo_save (cr);

      if (_gtk_print_get_default_user_units () == GTK_UNIT_MM)
        text = g_strdup_printf ("%.1f mm", paper_width);
      else
        text = g_strdup_printf ("%.2f inch", paper_width);

      pango_layout_set_text (layout, text, -1);
      g_free (text);
      pango_layout_get_size (layout, &layout_w, &layout_h);

      cairo_translate (cr, (width - layout_w / PANGO_SCALE) / 2,
                           pos_y + h + shadow_offset + 2 * RULER_DISTANCE);

      gtk_style_context_get_color (context, state, &color);
      gdk_cairo_set_source_rgba (cr, &color);
      pango_cairo_show_layout (cr, layout);

      g_object_unref (layout);

      cairo_restore (cr);

      cairo_set_line_width (cr, 1);

      gtk_style_context_get_color (context, state, &color);
      gdk_cairo_set_source_rgba (cr, &color);

      if (ltr)
        {
          cairo_move_to (cr, pos_x - RULER_DISTANCE, pos_y);
          cairo_line_to (cr, pos_x - RULER_DISTANCE, pos_y + h);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x - RULER_DISTANCE - RULER_RADIUS, pos_y - 0.5);
          cairo_line_to (cr, pos_x - RULER_DISTANCE + RULER_RADIUS, pos_y - 0.5);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x - RULER_DISTANCE - RULER_RADIUS, pos_y + h + 0.5);
          cairo_line_to (cr, pos_x - RULER_DISTANCE + RULER_RADIUS, pos_y + h + 0.5);
          cairo_stroke (cr);
        }
      else
        {
          cairo_move_to (cr, pos_x + w + shadow_offset + RULER_DISTANCE, pos_y);
          cairo_line_to (cr, pos_x + w + shadow_offset + RULER_DISTANCE, pos_y + h);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x + w + shadow_offset + RULER_DISTANCE - RULER_RADIUS, pos_y - 0.5);
          cairo_line_to (cr, pos_x + w + shadow_offset + RULER_DISTANCE + RULER_RADIUS, pos_y - 0.5);
          cairo_stroke (cr);

          cairo_move_to (cr, pos_x + w + shadow_offset + RULER_DISTANCE - RULER_RADIUS, pos_y + h + 0.5);
          cairo_line_to (cr, pos_x + w + shadow_offset + RULER_DISTANCE + RULER_RADIUS, pos_y + h + 0.5);
          cairo_stroke (cr);
        }

      cairo_move_to (cr, pos_x, pos_y + h + shadow_offset + RULER_DISTANCE);
      cairo_line_to (cr, pos_x + w, pos_y + h + shadow_offset + RULER_DISTANCE);
      cairo_stroke (cr);

      cairo_move_to (cr, pos_x - 0.5, pos_y + h + shadow_offset + RULER_DISTANCE - RULER_RADIUS);
      cairo_line_to (cr, pos_x - 0.5, pos_y + h + shadow_offset + RULER_DISTANCE + RULER_RADIUS);
      cairo_stroke (cr);

      cairo_move_to (cr, pos_x + w + 0.5, pos_y + h + shadow_offset + RULER_DISTANCE - RULER_RADIUS);
      cairo_line_to (cr, pos_x + w + 0.5, pos_y + h + shadow_offset + RULER_DISTANCE + RULER_RADIUS);
      cairo_stroke (cr);
    }

  gtk_style_context_restore (context);

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
update_number_up_layout (GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPrintCapabilities       caps;
  GtkPrinterOptionSet       *set;
  GtkNumberUpLayout          layout;
  GtkPrinterOption          *option;
  GtkPrinterOption          *old_option;
  GtkPageOrientation         page_orientation;

  set = priv->options;

  caps = priv->manual_capabilities | priv->printer_capabilities;

  if (caps & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT)
    {
      if (priv->number_up_layout_n_option == NULL)
        {
          priv->number_up_layout_n_option = gtk_printer_option_set_lookup (set, "gtk-n-up-layout");
          if (priv->number_up_layout_n_option == NULL)
            {
              char *n_up_layout[] = { "lrtb", "lrbt", "rltb", "rlbt", "tblr", "tbrl", "btlr", "btrl" };
               /* Translators: These strings name the possible arrangements of
                * multiple pages on a sheet when printing (same as in gtkprintbackendcups.c)
                */
              char *n_up_layout_display[] = { N_("Left to right, top to bottom"), N_("Left to right, bottom to top"),
                                              N_("Right to left, top to bottom"), N_("Right to left, bottom to top"),
                                              N_("Top to bottom, left to right"), N_("Top to bottom, right to left"),
                                              N_("Bottom to top, left to right"), N_("Bottom to top, right to left") };
              int i;

              priv->number_up_layout_n_option = gtk_printer_option_new ("gtk-n-up-layout",
                                                                        _("Page Ordering"),
                                                                        GTK_PRINTER_OPTION_TYPE_PICKONE);
              gtk_printer_option_allocate_choices (priv->number_up_layout_n_option, 8);

              for (i = 0; i < G_N_ELEMENTS (n_up_layout_display); i++)
                {
                  priv->number_up_layout_n_option->choices[i] = g_strdup (n_up_layout[i]);
                  priv->number_up_layout_n_option->choices_display[i] = g_strdup (_(n_up_layout_display[i]));
                }
            }
          g_object_ref (priv->number_up_layout_n_option);

          priv->number_up_layout_2_option = gtk_printer_option_new ("gtk-n-up-layout",
                                                                    _("Page Ordering"),
                                                                    GTK_PRINTER_OPTION_TYPE_PICKONE);
          gtk_printer_option_allocate_choices (priv->number_up_layout_2_option, 2);
        }

      page_orientation = gtk_page_setup_get_orientation (priv->page_setup);
      if (page_orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
          page_orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
        {
          if (! (priv->number_up_layout_2_option->choices[0] == priv->number_up_layout_n_option->choices[0] &&
                 priv->number_up_layout_2_option->choices[1] == priv->number_up_layout_n_option->choices[2]))
            {
              g_free (priv->number_up_layout_2_option->choices_display[0]);
              g_free (priv->number_up_layout_2_option->choices_display[1]);
              priv->number_up_layout_2_option->choices[0] = priv->number_up_layout_n_option->choices[0];
              priv->number_up_layout_2_option->choices[1] = priv->number_up_layout_n_option->choices[2];
              priv->number_up_layout_2_option->choices_display[0] = g_strdup ( _("Left to right"));
              priv->number_up_layout_2_option->choices_display[1] = g_strdup ( _("Right to left"));
            }
        }
      else
        {
          if (! (priv->number_up_layout_2_option->choices[0] == priv->number_up_layout_n_option->choices[0] &&
                 priv->number_up_layout_2_option->choices[1] == priv->number_up_layout_n_option->choices[1]))
            {
              g_free (priv->number_up_layout_2_option->choices_display[0]);
              g_free (priv->number_up_layout_2_option->choices_display[1]);
              priv->number_up_layout_2_option->choices[0] = priv->number_up_layout_n_option->choices[0];
              priv->number_up_layout_2_option->choices[1] = priv->number_up_layout_n_option->choices[1];
              priv->number_up_layout_2_option->choices_display[0] = g_strdup ( _("Top to bottom"));
              priv->number_up_layout_2_option->choices_display[1] = g_strdup ( _("Bottom to top"));
            }
        }

      layout = dialog_get_number_up_layout (dialog);

      old_option = gtk_printer_option_set_lookup (set, "gtk-n-up-layout");
      if (old_option != NULL)
        gtk_printer_option_set_remove (set, old_option);

      if (dialog_get_pages_per_sheet (dialog) != 1)
        {
          GEnumClass *enum_class;
          GEnumValue *enum_value;
          enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);

          if (dialog_get_pages_per_sheet (dialog) == 2)
            {
              option = priv->number_up_layout_2_option;

              switch (layout)
                {
                case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM:
                case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM);
                  break;

                case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP:
                case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP);
                  break;

                case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM:
                case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM);
                  break;

                case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP:
                case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT:
                  enum_value = g_enum_get_value (enum_class, GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP);
                  break;

                default:
                  g_assert_not_reached();
                  enum_value = NULL;
                }
            }
          else
            {
              option = priv->number_up_layout_n_option;

              enum_value = g_enum_get_value (enum_class, layout);
            }

          g_assert (enum_value != NULL);
          gtk_printer_option_set (option, enum_value->value_nick);
          g_type_class_unref (enum_class);

          gtk_printer_option_set_add (set, option);
        }
    }

  setup_option (dialog, "gtk-n-up-layout", priv->number_up_layout);

  if (priv->number_up_layout != NULL)
    gtk_widget_set_sensitive (GTK_WIDGET (priv->number_up_layout),
                              (caps & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT) &&
                              (dialog_get_pages_per_sheet (dialog) > 1));
}

static void
custom_paper_dialog_response_cb (GtkDialog *custom_paper_dialog,
                                 gint       response_id,
                                 gpointer   user_data)
{
  GtkPrintUnixDialog        *print_dialog = GTK_PRINT_UNIX_DIALOG (user_data);
  GtkPrintUnixDialogPrivate *priv = print_dialog->priv;
  GtkTreeModel              *model;
  GtkTreeIter                iter;

  _gtk_print_load_custom_papers (priv->custom_paper_list);

  priv->internal_page_setup_change = TRUE;
  update_paper_sizes (print_dialog);
  priv->internal_page_setup_change = FALSE;

  if (priv->page_setup_set)
    {
      model = GTK_TREE_MODEL (priv->custom_paper_list);
      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          do
            {
              GtkPageSetup *page_setup;
              gtk_tree_model_get (model, &iter, 0, &page_setup, -1);

              if (page_setup &&
                  g_strcmp0 (gtk_paper_size_get_display_name (gtk_page_setup_get_paper_size (page_setup)),
                             gtk_paper_size_get_display_name (gtk_page_setup_get_paper_size (priv->page_setup))) == 0)
                gtk_print_unix_dialog_set_page_setup (print_dialog, page_setup);

              g_object_unref (page_setup);
            } while (gtk_tree_model_iter_next (model, &iter));
        }
    }

  gtk_widget_destroy (GTK_WIDGET (custom_paper_dialog));
}

static void
orientation_changed (GtkComboBox        *combo_box,
                     GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkPageOrientation         orientation;
  GtkPageSetup              *page_setup;

  if (priv->internal_page_setup_change)
    return;

  orientation = (GtkPageOrientation) gtk_combo_box_get_active (GTK_COMBO_BOX (priv->orientation_combo));

  if (priv->page_setup)
    {
      page_setup = gtk_page_setup_copy (priv->page_setup);
      if (page_setup)
        gtk_page_setup_set_orientation (page_setup, orientation);

      gtk_print_unix_dialog_set_page_setup (dialog, page_setup);
    }

  redraw_page_layout_preview (dialog);
}

static void
paper_size_changed (GtkComboBox        *combo_box,
                    GtkPrintUnixDialog *dialog)
{
  GtkPrintUnixDialogPrivate *priv = dialog->priv;
  GtkTreeIter iter;
  GtkPageSetup *page_setup, *last_page_setup;
  GtkPageOrientation orientation;

  if (priv->internal_page_setup_change)
    return;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      gtk_tree_model_get (gtk_combo_box_get_model (combo_box),
                          &iter, PAGE_SETUP_LIST_COL_PAGE_SETUP, &page_setup,
                          -1);

      if (page_setup == NULL)
        {
          GtkWidget *custom_paper_dialog;

          /* Change from "manage" menu item to last value */
          if (priv->page_setup)
            last_page_setup = g_object_ref (priv->page_setup);
          else
            last_page_setup = gtk_page_setup_new (); /* "good" default */

          if (!set_paper_size (dialog, last_page_setup, FALSE, FALSE))
            set_paper_size (dialog, last_page_setup, TRUE, TRUE);
          g_object_unref (last_page_setup);

          /* And show the custom paper dialog */
          custom_paper_dialog = _gtk_custom_paper_unix_dialog_new (GTK_WINDOW (dialog), _("Manage Custom Sizes"));
          g_signal_connect (custom_paper_dialog, "response", G_CALLBACK (custom_paper_dialog_response_cb), dialog);
          gtk_window_present (GTK_WINDOW (custom_paper_dialog));

          return;
        }

      if (priv->page_setup)
        orientation = gtk_page_setup_get_orientation (priv->page_setup);
      else
        orientation = GTK_PAGE_ORIENTATION_PORTRAIT;

      gtk_page_setup_set_orientation (page_setup, orientation);
      gtk_print_unix_dialog_set_page_setup (dialog, page_setup);

      g_object_unref (page_setup);
    }

  redraw_page_layout_preview (dialog);
}

static gboolean
paper_size_row_is_separator (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  gboolean separator;

  gtk_tree_model_get (model, iter,
                      PAGE_SETUP_LIST_COL_IS_SEPARATOR, &separator,
                      -1);
  return separator;
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
                      PAGE_SETUP_LIST_COL_PAGE_SETUP, &page_setup,
                      -1);
  if (page_setup)
    {
      paper_size = gtk_page_setup_get_paper_size (page_setup);
      g_object_set (cell, "text",  gtk_paper_size_get_display_name (paper_size), NULL);
      g_object_unref (page_setup);
    }
  else
    g_object_set (cell, "text",  _("Manage Custom Sizes…"), NULL);
}

/**
 * gtk_print_unix_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 *
 * Creates a new #GtkPrintUnixDialog.
 *
 * Returns: a new #GtkPrintUnixDialog
 *
 * Since: 2.10
 */
GtkWidget *
gtk_print_unix_dialog_new (const gchar *title,
                           GtkWindow   *parent)
{
  GtkWidget *result;

  result = g_object_new (GTK_TYPE_PRINT_UNIX_DIALOG,
                         "transient-for", parent,
                         "title", title ? title : _("Print"),
                         NULL);

  return result;
}

/**
 * gtk_print_unix_dialog_get_selected_printer:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the currently selected printer.
 *
 * Returns: (transfer none): the currently selected printer
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

      priv->page_setup_set = TRUE;

      g_object_notify (G_OBJECT (dialog), "page-setup");
    }
}

/**
 * gtk_print_unix_dialog_get_page_setup:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the page setup that is used by the #GtkPrintUnixDialog.
 *
 * Returns: (transfer none): the page setup of @dialog.
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
 * gtk_print_unix_dialog_get_page_setup_set:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the page setup that is used by the #GtkPrintUnixDialog.
 *
 * Returns: whether a page setup was set by user.
 *
 * Since: 2.18
 */
gboolean
gtk_print_unix_dialog_get_page_setup_set (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->priv->page_setup_set;
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
 * Gets the current page of the #GtkPrintUnixDialog.
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
                              PRINTER_LIST_COL_PRINTER_OBJ, &printer,
                              -1);
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
 * @settings: (allow-none): a #GtkPrintSettings, or %NULL
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
 * new object, and you need to unref it
 * if don’t want to keep it.
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
      gtk_print_settings_set_page_ranges (settings, ranges, n_ranges);
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
 * you pass #GTK_PRINT_CAPABILITY_SCALE. If you don’t pass that, then
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

  if (priv->manual_capabilities != capabilities)
    {
      priv->manual_capabilities = capabilities;
      update_dialog_from_capabilities (dialog);

      if (priv->current_printer)
        {
          GtkTreeSelection *selection;

          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->printer_treeview));
          g_clear_object (&priv->current_printer);
          priv->internal_printer_change = TRUE;
          selected_printer_changed (selection, dialog);
          priv->internal_printer_change = FALSE;
       }

      g_object_notify (G_OBJECT (dialog), "manual-capabilities");
    }
}

/**
 * gtk_print_unix_dialog_get_manual_capabilities:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the value of #GtkPrintUnixDialog:manual-capabilities property.
 *
 * Returns: the printing capabilities
 *
 * Since: 2.18
 */
GtkPrintCapabilities
gtk_print_unix_dialog_get_manual_capabilities (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->priv->manual_capabilities;
}

/**
 * gtk_print_unix_dialog_set_support_selection:
 * @dialog: a #GtkPrintUnixDialog
 * @support_selection: %TRUE to allow print selection
 *
 * Sets whether the print dialog allows user to print a selection.
 *
 * Since: 2.18
 */
void
gtk_print_unix_dialog_set_support_selection (GtkPrintUnixDialog *dialog,
                                             gboolean            support_selection)
{
  GtkPrintUnixDialogPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  priv = dialog->priv;

  support_selection = support_selection != FALSE;
  if (priv->support_selection != support_selection)
    {
      priv->support_selection = support_selection;

      if (priv->selection_radio)
        {
          if (support_selection)
            {
              gtk_widget_set_sensitive (priv->selection_radio, priv->has_selection);
              gtk_widget_show (priv->selection_radio);
            }
          else
            {
              gtk_widget_set_sensitive (priv->selection_radio, FALSE);
              gtk_widget_hide (priv->selection_radio);
            }
        }

      g_object_notify (G_OBJECT (dialog), "support-selection");
    }
}

/**
 * gtk_print_unix_dialog_get_support_selection:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the value of #GtkPrintUnixDialog:support-selection property.
 *
 * Returns: whether the application supports print of selection
 *
 * Since: 2.18
 */
gboolean
gtk_print_unix_dialog_get_support_selection (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->priv->support_selection;
}

/**
 * gtk_print_unix_dialog_set_has_selection:
 * @dialog: a #GtkPrintUnixDialog
 * @has_selection: %TRUE indicates that a selection exists
 *
 * Sets whether a selection exists.
 *
 * Since: 2.18
 */
void
gtk_print_unix_dialog_set_has_selection (GtkPrintUnixDialog *dialog,
                                         gboolean            has_selection)
{
  GtkPrintUnixDialogPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  priv = dialog->priv;

  has_selection = has_selection != FALSE;
  if (priv->has_selection != has_selection)
    {
      priv->has_selection = has_selection;

      if (priv->selection_radio)
        {
          if (priv->support_selection)
            gtk_widget_set_sensitive (priv->selection_radio, has_selection);
          else
            gtk_widget_set_sensitive (priv->selection_radio, FALSE);
        }

      g_object_notify (G_OBJECT (dialog), "has-selection");
    }
}

/**
 * gtk_print_unix_dialog_get_has_selection:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the value of #GtkPrintUnixDialog:has-selection property.
 *
 * Returns: whether there is a selection
 *
 * Since: 2.18
 */
gboolean
gtk_print_unix_dialog_get_has_selection (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->priv->has_selection;
}

/**
 * gtk_print_unix_dialog_set_embed_page_setup
 * @dialog: a #GtkPrintUnixDialog
 * @embed: embed page setup selection
 *
 * Embed page size combo box and orientation combo box into page setup page.
 *
 * Since: 2.18
 */
void
gtk_print_unix_dialog_set_embed_page_setup (GtkPrintUnixDialog *dialog,
                                            gboolean            embed)
{
  GtkPrintUnixDialogPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog));

  priv = dialog->priv;

  embed = embed != FALSE;
  if (priv->embed_page_setup != embed)
    {
      priv->embed_page_setup = embed;

      gtk_widget_set_sensitive (priv->paper_size_combo, priv->embed_page_setup);
      gtk_widget_set_sensitive (priv->orientation_combo, priv->embed_page_setup);

      if (priv->embed_page_setup)
        {
          if (priv->paper_size_combo != NULL)
            g_signal_connect (priv->paper_size_combo, "changed", G_CALLBACK (paper_size_changed), dialog);

          if (priv->orientation_combo)
            g_signal_connect (priv->orientation_combo, "changed", G_CALLBACK (orientation_changed), dialog);
        }
      else
        {
          if (priv->paper_size_combo != NULL)
            g_signal_handlers_disconnect_by_func (priv->paper_size_combo, G_CALLBACK (paper_size_changed), dialog);

          if (priv->orientation_combo)
            g_signal_handlers_disconnect_by_func (priv->orientation_combo, G_CALLBACK (orientation_changed), dialog);
        }

      priv->internal_page_setup_change = TRUE;
      update_paper_sizes (dialog);
      priv->internal_page_setup_change = FALSE;
    }
}

/**
 * gtk_print_unix_dialog_get_embed_page_setup:
 * @dialog: a #GtkPrintUnixDialog
 *
 * Gets the value of #GtkPrintUnixDialog:embed-page-setup property.
 *
 * Returns: whether there is a selection
 *
 * Since: 2.18
 */
gboolean
gtk_print_unix_dialog_get_embed_page_setup (GtkPrintUnixDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_PRINT_UNIX_DIALOG (dialog), FALSE);

  return dialog->priv->embed_page_setup;
}
