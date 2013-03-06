/* gtkcombobox.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcombobox.h"

#include "gtkadjustment.h"
#include "gtkcellareabox.h"
#include "gtktreemenu.h"
#include "gtkarrow.h"
#include "gtkbindings.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellview.h"
#include "gtkeventbox.h"
#include "gtkframe.h"
#include "gtkbox.h"
#include "gtkliststore.h"
#include "gtkmain.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkscrolledwindow.h"
#include "gtkseparatormenuitem.h"
#include "deprecated/gtktearoffmenuitem.h"
#include "gtktogglebutton.h"
#include "gtktreeselection.h"
#include "gtkseparator.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

#include <gobject/gvaluecollector.h>

#include <string.h>
#include <stdarg.h>

#include "gtkmarshalers.h"
#include "gtkintl.h"

#include "gtkentryprivate.h"
#include "gtktreeprivate.h"
#include "a11y/gtkcomboboxaccessible.h"


/**
 * SECTION:gtkcombobox
 * @Short_description: A widget used to choose from a list of items
 * @Title: GtkComboBox
 * @See_also: #GtkComboBoxText, #GtkTreeModel, #GtkCellRenderer
 *
 * A GtkComboBox is a widget that allows the user to choose from a list of
 * valid choices. The GtkComboBox displays the selected choice. When
 * activated, the GtkComboBox displays a popup which allows the user to
 * make a new choice. The style in which the selected value is displayed,
 * and the style of the popup is determined by the current theme. It may
 * be similar to a Windows-style combo box.
 *
 * The GtkComboBox uses the model-view pattern; the list of valid choices
 * is specified in the form of a tree model, and the display of the choices
 * can be adapted to the data in the model by using cell renderers, as you
 * would in a tree view. This is possible since GtkComboBox implements the
 * #GtkCellLayout interface. The tree model holding the valid choices is
 * not restricted to a flat list, it can be a real tree, and the popup will
 * reflect the tree structure.
 *
 * To allow the user to enter values not in the model, the 'has-entry'
 * property allows the GtkComboBox to contain a #GtkEntry. This entry
 * can be accessed by calling gtk_bin_get_child() on the combo box.
 *
 * For a simple list of textual choices, the model-view API of GtkComboBox
 * can be a bit overwhelming. In this case, #GtkComboBoxText offers a
 * simple alternative. Both GtkComboBox and #GtkComboBoxText can contain
 * an entry.
 */


/* WELCOME, to THE house of evil code */
struct _GtkComboBoxPrivate
{
  GtkTreeModel *model;

  GtkCellArea *area;

  gint col_column;
  gint row_column;

  gint wrap_width;
  GtkShadowType shadow_type;

  gint active; /* Only temporary */
  GtkTreeRowReference *active_row;

  GtkWidget *tree_view;

  GtkWidget *cell_view;
  GtkWidget *cell_view_frame;

  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *arrow;
  GtkWidget *separator;

  GtkWidget *popup_widget;
  GtkWidget *popup_window;
  GtkWidget *scrolled_window;

  gulong inserted_id;
  gulong deleted_id;
  gulong reordered_id;
  gulong changed_id;
  guint popup_idle_id;
  guint activate_button;
  guint32 activate_time;
  guint scroll_timer;
  guint resize_idle_id;

  /* For "has-entry" specific behavior we track
   * an automated cell renderer and text column
   */
  gint  text_column;
  GtkCellRenderer *text_renderer;

  gint id_column;

  guint popup_in_progress : 1;
  guint popup_shown : 1;
  guint add_tearoffs : 1;
  guint has_frame : 1;
  guint is_cell_renderer : 1;
  guint editing_canceled : 1;
  guint auto_scroll : 1;
  guint focus_on_click : 1;
  guint button_sensitivity : 2;
  guint has_entry : 1;
  guint popup_fixed_width : 1;

  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;

  GdkDevice *grab_pointer;
  GdkDevice *grab_keyboard;

  gchar *tearoff_title;
};

/* While debugging this evil code, I have learned that
 * there are actually 4 modes to this widget, which can
 * be characterized as follows
 * 
 * 1) menu mode, no child added
 *
 * tree_view -> NULL
 * cell_view -> GtkCellView, regular child
 * cell_view_frame -> NULL
 * button -> GtkToggleButton set_parent to combo
 * arrow -> GtkArrow set_parent to button
 * separator -> GtkVSepator set_parent to button
 * popup_widget -> GtkMenu
 * popup_window -> NULL
 * scrolled_window -> NULL
 *
 * 2) menu mode, child added
 * 
 * tree_view -> NULL
 * cell_view -> NULL 
 * cell_view_frame -> NULL
 * button -> GtkToggleButton set_parent to combo
 * arrow -> GtkArrow, child of button
 * separator -> NULL
 * popup_widget -> GtkMenu
 * popup_window -> NULL
 * scrolled_window -> NULL
 *
 * 3) list mode, no child added
 * 
 * tree_view -> GtkTreeView, child of scrolled_window
 * cell_view -> GtkCellView, regular child
 * cell_view_frame -> GtkFrame, set parent to combo
 * button -> GtkToggleButton, set_parent to combo
 * arrow -> GtkArrow, child of button
 * separator -> NULL
 * popup_widget -> tree_view
 * popup_window -> GtkWindow
 * scrolled_window -> GtkScrolledWindow, child of popup_window
 *
 * 4) list mode, child added
 *
 * tree_view -> GtkTreeView, child of scrolled_window
 * cell_view -> NULL
 * cell_view_frame -> NULL
 * button -> GtkToggleButton, set_parent to combo
 * arrow -> GtkArrow, child of button
 * separator -> NULL
 * popup_widget -> tree_view
 * popup_window -> GtkWindow
 * scrolled_window -> GtkScrolledWindow, child of popup_window
 * 
 */

enum {
  CHANGED,
  MOVE_ACTIVE,
  POPUP,
  POPDOWN,
  FORMAT_ENTRY_TEXT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_WRAP_WIDTH,
  PROP_ROW_SPAN_COLUMN,
  PROP_COLUMN_SPAN_COLUMN,
  PROP_ACTIVE,
  PROP_ADD_TEAROFFS,
  PROP_TEAROFF_TITLE,
  PROP_HAS_FRAME,
  PROP_FOCUS_ON_CLICK,
  PROP_POPUP_SHOWN,
  PROP_BUTTON_SENSITIVITY,
  PROP_EDITING_CANCELED,
  PROP_HAS_ENTRY,
  PROP_ENTRY_TEXT_COLUMN,
  PROP_POPUP_FIXED_WIDTH,
  PROP_ID_COLUMN,
  PROP_ACTIVE_ID,
  PROP_CELL_AREA
};

static guint combo_box_signals[LAST_SIGNAL] = {0,};

#define SCROLL_TIME  100

/* common */

static void     gtk_combo_box_cell_layout_init     (GtkCellLayoutIface *iface);
static void     gtk_combo_box_cell_editable_init   (GtkCellEditableIface *iface);
static GObject *gtk_combo_box_constructor          (GType                  type,
                                                    guint                  n_construct_properties,
                                                    GObjectConstructParam *construct_properties);
static void     gtk_combo_box_dispose              (GObject          *object);
static void     gtk_combo_box_finalize             (GObject          *object);
static void     gtk_combo_box_destroy              (GtkWidget        *widget);

static void     gtk_combo_box_set_property         (GObject         *object,
                                                    guint            prop_id,
                                                    const GValue    *value,
                                                    GParamSpec      *spec);
static void     gtk_combo_box_get_property         (GObject         *object,
                                                    guint            prop_id,
                                                    GValue          *value,
                                                    GParamSpec      *spec);

static void     gtk_combo_box_state_flags_changed  (GtkWidget       *widget,
                                                    GtkStateFlags    previous);
static void     gtk_combo_box_grab_focus           (GtkWidget       *widget);
static void     gtk_combo_box_style_updated        (GtkWidget       *widget);
static void     gtk_combo_box_button_toggled       (GtkWidget       *widget,
                                                    gpointer         data);
static void     gtk_combo_box_button_state_flags_changed (GtkWidget     *widget,
                                                          GtkStateFlags  previous,
                                                          gpointer       data);
static void     gtk_combo_box_add                  (GtkContainer    *container,
                                                    GtkWidget       *widget);
static void     gtk_combo_box_remove               (GtkContainer    *container,
                                                    GtkWidget       *widget);

static void     gtk_combo_box_menu_show            (GtkWidget        *menu,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_hide            (GtkWidget        *menu,
                                                    gpointer          user_data);

static void     gtk_combo_box_set_popup_widget     (GtkComboBox      *combo_box,
                                                    GtkWidget        *popup);
static void     gtk_combo_box_menu_position_below  (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_position_over   (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_position        (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);

static void     gtk_combo_box_unset_model          (GtkComboBox      *combo_box);

static void     gtk_combo_box_size_allocate        (GtkWidget        *widget,
                                                    GtkAllocation    *allocation);
static void     gtk_combo_box_forall               (GtkContainer     *container,
                                                    gboolean          include_internals,
                                                    GtkCallback       callback,
                                                    gpointer          callback_data);
static gboolean gtk_combo_box_draw                 (GtkWidget        *widget,
                                                    cairo_t          *cr);
static gboolean gtk_combo_box_scroll_event         (GtkWidget        *widget,
                                                    GdkEventScroll   *event);
static void     gtk_combo_box_set_active_internal  (GtkComboBox      *combo_box,
                                                    GtkTreePath      *path);

static void     gtk_combo_box_check_appearance     (GtkComboBox      *combo_box);
static void     gtk_combo_box_real_move_active     (GtkComboBox      *combo_box,
                                                    GtkScrollType     scroll);
static void     gtk_combo_box_real_popup           (GtkComboBox      *combo_box);
static gboolean gtk_combo_box_real_popdown         (GtkComboBox      *combo_box);

/* listening to the model */
static void     gtk_combo_box_model_row_inserted   (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          user_data);
static void     gtk_combo_box_model_row_deleted    (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    gpointer          user_data);
static void     gtk_combo_box_model_rows_reordered (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gint             *new_order,
                                                    gpointer          user_data);
static void     gtk_combo_box_model_row_changed    (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);
static void     gtk_combo_box_model_row_expanded   (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);

/* list */
static void     gtk_combo_box_list_position        (GtkComboBox      *combo_box,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *width,
                                                    gint             *height);
static void     gtk_combo_box_list_setup           (GtkComboBox      *combo_box);
static void     gtk_combo_box_list_destroy         (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_list_button_released (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_key_press       (GtkWidget        *widget,
                                                    GdkEventKey      *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_enter_notify    (GtkWidget        *widget,
                                                    GdkEventCrossing *event,
                                                    gpointer          data);
static void     gtk_combo_box_list_auto_scroll     (GtkComboBox   *combo,
                                                    gint           x,
                                                    gint           y);
static gboolean gtk_combo_box_list_scroll_timeout  (GtkComboBox   *combo);
static gboolean gtk_combo_box_list_button_pressed  (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);

static gboolean gtk_combo_box_list_select_func     (GtkTreeSelection *selection,
                                                    GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    gboolean          path_currently_selected,
                                                    gpointer          data);

static void     gtk_combo_box_list_row_changed     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);
static void     gtk_combo_box_list_popup_resize    (GtkComboBox      *combo_box);

/* menu */
static void     gtk_combo_box_menu_setup           (GtkComboBox      *combo_box,
                                                    gboolean          add_children);
static void     gtk_combo_box_update_title         (GtkComboBox      *combo_box);
static void     gtk_combo_box_menu_destroy         (GtkComboBox      *combo_box);


static gboolean gtk_combo_box_menu_button_press    (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_activate        (GtkWidget        *menu,
                                                    const gchar      *path,
                                                    GtkComboBox      *combo_box);
static void     gtk_combo_box_update_sensitivity   (GtkComboBox      *combo_box);
static gboolean gtk_combo_box_menu_key_press       (GtkWidget        *widget,
                                                    GdkEventKey      *event,
                                                    gpointer          data);
static void     gtk_combo_box_menu_popup           (GtkComboBox      *combo_box,
                                                    guint             button,
                                                    guint32           activate_time);

/* cell layout */
static GtkCellArea *gtk_combo_box_cell_layout_get_area       (GtkCellLayout    *cell_layout);

static gboolean gtk_combo_box_mnemonic_activate              (GtkWidget    *widget,
                                                              gboolean      group_cycling);

static void     gtk_combo_box_child_show                     (GtkWidget       *widget,
                                                              GtkComboBox     *combo_box);
static void     gtk_combo_box_child_hide                     (GtkWidget       *widget,
                                                              GtkComboBox     *combo_box);

/* GtkComboBox:has-entry callbacks */
static void     gtk_combo_box_entry_contents_changed         (GtkEntry        *entry,
                                                              gpointer         user_data);
static void     gtk_combo_box_entry_active_changed           (GtkComboBox     *combo_box,
                                                              gpointer         user_data);
static gchar   *gtk_combo_box_format_entry_text              (GtkComboBox     *combo_box,
							      const gchar     *path);

/* GtkBuildable method implementation */
static GtkBuildableIface *parent_buildable_iface;

static void     gtk_combo_box_buildable_init                 (GtkBuildableIface *iface);
static gboolean gtk_combo_box_buildable_custom_tag_start     (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              GMarkupParser *parser,
                                                              gpointer      *data);
static void     gtk_combo_box_buildable_custom_tag_end       (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              gpointer      *data);
static GObject *gtk_combo_box_buildable_get_internal_child   (GtkBuildable *buildable,
                                                              GtkBuilder   *builder,
                                                              const gchar  *childname);


/* GtkCellEditable method implementations */
static void     gtk_combo_box_start_editing                  (GtkCellEditable *cell_editable,
                                                              GdkEvent        *event);

static void     gtk_combo_box_get_preferred_width            (GtkWidget    *widget,
                                                              gint         *minimum_size,
                                                              gint         *natural_size);
static void     gtk_combo_box_get_preferred_height           (GtkWidget    *widget,
                                                              gint         *minimum_size,
                                                              gint         *natural_size);
static void     gtk_combo_box_get_preferred_width_for_height (GtkWidget    *widget,
                                                              gint          avail_size,
                                                              gint         *minimum_size,
                                                              gint         *natural_size);
static void     gtk_combo_box_get_preferred_height_for_width (GtkWidget    *widget,
                                                              gint          avail_size,
                                                              gint         *minimum_size,
                                                              gint         *natural_size);
static GtkWidgetPath *gtk_combo_box_get_path_for_child       (GtkContainer *container,
                                                              GtkWidget    *child);
static void     gtk_combo_box_direction_changed              (GtkWidget    *widget,
                                                              GtkTextDirection  previous_direction);

G_DEFINE_TYPE_WITH_CODE (GtkComboBox, gtk_combo_box, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_combo_box_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                gtk_combo_box_cell_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_combo_box_buildable_init))


/* common */
static void
gtk_combo_box_class_init (GtkComboBoxClass *klass)
{
  GObjectClass *object_class;
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  container_class = (GtkContainerClass *)klass;
  container_class->forall = gtk_combo_box_forall;
  container_class->add = gtk_combo_box_add;
  container_class->remove = gtk_combo_box_remove;
  container_class->get_path_for_child = gtk_combo_box_get_path_for_child;

  gtk_container_class_handle_border_width (container_class);

  widget_class = (GtkWidgetClass *)klass;
  widget_class->size_allocate = gtk_combo_box_size_allocate;
  widget_class->draw = gtk_combo_box_draw;
  widget_class->scroll_event = gtk_combo_box_scroll_event;
  widget_class->mnemonic_activate = gtk_combo_box_mnemonic_activate;
  widget_class->grab_focus = gtk_combo_box_grab_focus;
  widget_class->style_updated = gtk_combo_box_style_updated;
  widget_class->state_flags_changed = gtk_combo_box_state_flags_changed;
  widget_class->get_preferred_width = gtk_combo_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_combo_box_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_combo_box_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_combo_box_get_preferred_width_for_height;
  widget_class->destroy = gtk_combo_box_destroy;
  widget_class->direction_changed = gtk_combo_box_direction_changed;

  object_class = (GObjectClass *)klass;
  object_class->constructor = gtk_combo_box_constructor;
  object_class->dispose = gtk_combo_box_dispose;
  object_class->finalize = gtk_combo_box_finalize;
  object_class->set_property = gtk_combo_box_set_property;
  object_class->get_property = gtk_combo_box_get_property;

  klass->format_entry_text = gtk_combo_box_format_entry_text;

  /* signals */
  /**
   * GtkComboBox::changed:
   * @widget: the object which received the signal
   * 
   * The changed signal is emitted when the active
   * item is changed. The can be due to the user selecting
   * a different item from the list, or due to a
   * call to gtk_combo_box_set_active_iter().
   * It will also be emitted while typing into the entry of a combo box
   * with an entry.
   *
   * Since: 2.4
   */
  combo_box_signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * GtkComboBox::move-active:
   * @widget: the object that received the signal
   * @scroll_type: a #GtkScrollType
   *
   * The ::move-active signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to move the active selection.
   *
   * Since: 2.12
   */
  combo_box_signals[MOVE_ACTIVE] =
    g_signal_new_class_handler (I_("move-active"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_move_active),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__ENUM,
                                G_TYPE_NONE, 1,
                                GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkComboBox::popup:
   * @widget: the object that received the signal
   *
   * The ::popup signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to popup the combo box list.
   *
   * The default binding for this signal is Alt+Down.
   *
   * Since: 2.12
   */
  combo_box_signals[POPUP] =
    g_signal_new_class_handler (I_("popup"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_popup),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  /**
   * GtkComboBox::popdown:
   * @button: the object which received the signal
   *
   * The ::popdown signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to popdown the combo box list.
   *
   * The default bindings for this signal are Alt+Up and Escape.
   *
   * Since: 2.12
   */
  combo_box_signals[POPDOWN] =
    g_signal_new_class_handler (I_("popdown"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_popdown),
                                NULL, NULL,
                                _gtk_marshal_BOOLEAN__VOID,
                                G_TYPE_BOOLEAN, 0);

  /**
   * GtkComboBox::format-entry-text:
   * @combo: the object which received the signal
   * @path: the GtkTreePath string from the combo box's current model to format text for
   *
   * For combo boxes that are created with an entry (See GtkComboBox:has-entry).
   *
   * A signal which allows you to change how the text displayed in a combo box's
   * entry is displayed.
   *
   * Connect a signal handler which returns an allocated string representing
   * @path. That string will then be used to set the text in the combo box's entry.
   * The default signal handler uses the text from the GtkComboBox::entry-text-column 
   * model column.
   *
   * Here's an example signal handler which fetches data from the model and
   * displays it in the entry.
   * |[
   * static gchar*
   * format_entry_text_callback (GtkComboBox *combo,
   *                             const gchar *path,
   *                             gpointer     user_data)
   * {
   *   GtkTreeIter iter;
   *   GtkTreeModel model;
   *   gdouble      value;
   *   
   *   model = gtk_combo_box_get_model (combo);
   *
   *   gtk_tree_model_get_iter_from_string (model, &iter, path);
   *   gtk_tree_model_get (model, &iter, 
   *                       THE_DOUBLE_VALUE_COLUMN, &value,
   *                       -1);
   *
   *   return g_strdup_printf ("&percnt;g", value);
   * }
   * ]|
   *
   * Return value: (transfer full): a newly allocated string representing @path 
   * for the current GtkComboBox model.
   *
   * Since: 3.4
   */
  combo_box_signals[FORMAT_ENTRY_TEXT] =
    g_signal_new (I_("format-entry-text"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, format_entry_text),
                  _gtk_single_string_accumulator, NULL,
                  _gtk_marshal_STRING__STRING,
                  G_TYPE_STRING, 1, G_TYPE_STRING);

  /* key bindings */
  binding_set = gtk_binding_set_by_class (widget_class);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, GDK_MOD1_MASK,
                                "popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Down, GDK_MOD1_MASK,
                                "popup", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, GDK_MOD1_MASK,
                                "popdown", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Up, GDK_MOD1_MASK,
                                "popdown", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
                                "popdown", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Up, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Home, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_START);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Home, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_START);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Down, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_End, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_END);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_End, 0,
                                "move-active", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_END);

  /* properties */
  g_object_class_override_property (object_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");

  /**
   * GtkComboBox:model:
   *
   * The model from which the combo box takes the values shown
   * in the list.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("ComboBox model"),
                                                        P_("The model for the combo box"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkComboBox:wrap-width:
   *
   * If wrap-width is set to a positive value, the list will be
   * displayed in multiple columns, the number of columns is
   * determined by wrap-width.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_WIDTH,
                                   g_param_spec_int ("wrap-width",
                                                     P_("Wrap width"),
                                                     P_("Wrap width for laying out the items in a grid"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));


  /**
   * GtkComboBox:row-span-column:
   *
   * If this is set to a non-negative value, it must be the index of a column
   * of type %G_TYPE_INT in the model.
   *
   * The values of that column are used to determine how many rows a value in
   * the list will span. Therefore, the values in the model column pointed to
   * by this property must be greater than zero and not larger than wrap-width.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ROW_SPAN_COLUMN,
                                   g_param_spec_int ("row-span-column",
                                                     P_("Row span column"),
                                                     P_("TreeModel column containing the row span values"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));


  /**
   * GtkComboBox:column-span-column:
   *
   * If this is set to a non-negative value, it must be the index of a column
   * of type %G_TYPE_INT in the model.
   *
   * The values of that column are used to determine how many columns a value
   * in the list will span.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_COLUMN_SPAN_COLUMN,
                                   g_param_spec_int ("column-span-column",
                                                     P_("Column span column"),
                                                     P_("TreeModel column containing the column span values"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));


  /**
   * GtkComboBox:active:
   *
   * The item which is currently active. If the model is a non-flat treemodel,
   * and the active item is not an immediate child of the root of the tree,
   * this property has the value
   * <literal>gtk_tree_path_get_indices (path)[0]</literal>,
   * where <literal>path</literal> is the #GtkTreePath of the active item.
   *
   * Since: 2.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_int ("active",
                                                     P_("Active item"),
                                                     P_("The item which is currently active"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkComboBox:add-tearoffs:
   *
   * The add-tearoffs property controls whether generated menus
   * have tearoff menu items.
   *
   * Note that this only affects menu style combo boxes.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_ADD_TEAROFFS,
                                   g_param_spec_boolean ("add-tearoffs",
                                                         P_("Add tearoffs to menus"),
                                                         P_("Whether dropdowns should have a tearoff menu item"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkComboBox:has-frame:
   *
   * The has-frame property controls whether a frame
   * is drawn around the entry.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_FRAME,
                                   g_param_spec_boolean ("has-frame",
                                                         P_("Has Frame"),
                                                         P_("Whether the combo box draws a frame around the child"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FOCUS_ON_CLICK,
                                   g_param_spec_boolean ("focus-on-click",
                                                         P_("Focus on click"),
                                                         P_("Whether the combo box grabs focus when it is clicked with the mouse"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkComboBox:tearoff-title:
   *
   * A title that may be displayed by the window manager
   * when the popup is torn-off.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_TEAROFF_TITLE,
                                   g_param_spec_string ("tearoff-title",
                                                        P_("Tearoff Title"),
                                                        P_("A title that may be displayed by the window manager when the popup is torn-off"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));


  /**
   * GtkComboBox:popup-shown:
   *
   * Whether the combo boxes dropdown is popped up.
   * Note that this property is mainly useful, because
   * it allows you to connect to notify::popup-shown.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_POPUP_SHOWN,
                                   g_param_spec_boolean ("popup-shown",
                                                         P_("Popup shown"),
                                                         P_("Whether the combo's dropdown is shown"),
                                                         FALSE,
                                                         GTK_PARAM_READABLE));


   /**
    * GtkComboBox:button-sensitivity:
    *
    * Whether the dropdown button is sensitive when
    * the model is empty.
    *
    * Since: 2.14
    */
   g_object_class_install_property (object_class,
                                    PROP_BUTTON_SENSITIVITY,
                                    g_param_spec_enum ("button-sensitivity",
                                                       P_("Button Sensitivity"),
                                                       P_("Whether the dropdown button is sensitive when the model is empty"),
                                                       GTK_TYPE_SENSITIVITY_TYPE,
                                                       GTK_SENSITIVITY_AUTO,
                                                       GTK_PARAM_READWRITE));

   /**
    * GtkComboBox:has-entry:
    *
    * Whether the combo box has an entry.
    *
    * Since: 2.24
    */
   g_object_class_install_property (object_class,
                                    PROP_HAS_ENTRY,
                                    g_param_spec_boolean ("has-entry",
                                                          P_("Has Entry"),
                                                          P_("Whether combo box has an entry"),
                                                          FALSE,
                                                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

   /**
    * GtkComboBox:entry-text-column:
    *
    * The column in the combo box's model to associate with strings from the entry
    * if the combo was created with #GtkComboBox:has-entry = %TRUE.
    *
    * Since: 2.24
    */
   g_object_class_install_property (object_class,
                                    PROP_ENTRY_TEXT_COLUMN,
                                    g_param_spec_int ("entry-text-column",
                                                      P_("Entry Text Column"),
                                                      P_("The column in the combo box's model to associate "
                                                         "with strings from the entry if the combo was "
                                                         "created with #GtkComboBox:has-entry = %TRUE"),
                                                      -1, G_MAXINT, -1,
                                                      GTK_PARAM_READWRITE));

   /**
    * GtkComboBox:id-column:
    *
    * The column in the combo box's model that provides string
    * IDs for the values in the model, if != -1.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_ID_COLUMN,
                                    g_param_spec_int ("id-column",
                                                      P_("ID Column"),
                                                      P_("The column in the combo box's model that provides "
                                                      "string IDs for the values in the model"),
                                                      -1, G_MAXINT, -1,
                                                      GTK_PARAM_READWRITE));

   /**
    * GtkComboBox:active-id:
    *
    * The value of the ID column of the active row.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_ACTIVE_ID,
                                    g_param_spec_string ("active-id",
                                                         P_("Active id"),
                                                         P_("The value of the id column "
                                                         "for the active row"),
                                                         NULL, GTK_PARAM_READWRITE));

   /**
    * GtkComboBox:popup-fixed-width:
    *
    * Whether the popup's width should be a fixed width matching the
    * allocated width of the combo box.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_POPUP_FIXED_WIDTH,
                                    g_param_spec_boolean ("popup-fixed-width",
                                                          P_("Popup Fixed Width"),
                                                          P_("Whether the popup's width should be a "
                                                             "fixed width matching the allocated width "
                                                             "of the combo box"),
                                                          TRUE,
                                                          GTK_PARAM_READWRITE));

   /**
    * GtkComboBox:cell-area:
    *
    * The #GtkCellArea used to layout cell renderers for this combo box.
    *
    * If no area is specified when creating the combo box with gtk_combo_box_new_with_area() 
    * a horizontally oriented #GtkCellAreaBox will be used.
    *
    * Since: 3.0
    */
   g_object_class_install_property (object_class,
                                    PROP_CELL_AREA,
                                    g_param_spec_object ("cell-area",
                                                         P_("Cell Area"),
                                                         P_("The GtkCellArea used to layout cells"),
                                                         GTK_TYPE_CELL_AREA,
                                                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("appears-as-list",
                                                                 P_("Appears as list"),
                                                                 P_("Whether dropdowns should look like lists rather than menus"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkComboBox:arrow-size:
   *
   * Sets the minimum size of the arrow in the combo box.  Note
   * that the arrow size is coupled to the font size, so in case
   * a larger font is used, the arrow will be larger than set
   * by arrow size.
   *
   * Since: 2.12
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("arrow-size",
                                                             P_("Arrow Size"),
                                                             P_("The minimum size of the arrow in the combo box"),
                                                             0,
                                                             G_MAXINT,
                                                             15,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkComboBox:arrow-scaling:
   *
   * Sets the amount of space used up by the combobox arrow,
   * proportional to the font size.
   *
   * Since: 3.2
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("The amount of space used by the arrow"),
                                                             0,
                                                             2.0,
                                                             1.0,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkComboBox:shadow-type:
   *
   * Which kind of shadow to draw around the combo box.
   *
   * Since: 2.12
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow-type",
                                                              P_("Shadow type"),
                                                              P_("Which kind of shadow to draw around the combo box"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_NONE,
                                                              GTK_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (GtkComboBoxPrivate));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_COMBO_BOX_ACCESSIBLE);
}

static void
gtk_combo_box_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = gtk_combo_box_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_combo_box_buildable_custom_tag_end;
  iface->get_internal_child = gtk_combo_box_buildable_get_internal_child;
}

static void
gtk_combo_box_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_combo_box_cell_layout_get_area;
}

static void
gtk_combo_box_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = gtk_combo_box_start_editing;
}

static void
gtk_combo_box_init (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv;

  combo_box->priv = G_TYPE_INSTANCE_GET_PRIVATE (combo_box,
                                                 GTK_TYPE_COMBO_BOX,
                                                 GtkComboBoxPrivate);
  priv = combo_box->priv;

  priv->wrap_width = 0;

  priv->active = -1;
  priv->active_row = NULL;
  priv->col_column = -1;
  priv->row_column = -1;

  priv->popup_shown = FALSE;
  priv->add_tearoffs = FALSE;
  priv->has_frame = TRUE;
  priv->is_cell_renderer = FALSE;
  priv->editing_canceled = FALSE;
  priv->auto_scroll = FALSE;
  priv->focus_on_click = TRUE;
  priv->button_sensitivity = GTK_SENSITIVITY_AUTO;
  priv->has_entry = FALSE;
  priv->popup_fixed_width = TRUE;

  priv->text_column = -1;
  priv->text_renderer = NULL;
  priv->id_column = -1;
}

static void
gtk_combo_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkCellArea *area;

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_combo_box_set_model (combo_box, g_value_get_object (value));
      break;

    case PROP_WRAP_WIDTH:
      gtk_combo_box_set_wrap_width (combo_box, g_value_get_int (value));
      break;

    case PROP_ROW_SPAN_COLUMN:
      gtk_combo_box_set_row_span_column (combo_box, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPAN_COLUMN:
      gtk_combo_box_set_column_span_column (combo_box, g_value_get_int (value));
      break;

    case PROP_ACTIVE:
      gtk_combo_box_set_active (combo_box, g_value_get_int (value));
      break;

    case PROP_ADD_TEAROFFS:
      gtk_combo_box_set_add_tearoffs (combo_box, g_value_get_boolean (value));
      break;

    case PROP_HAS_FRAME:
      priv->has_frame = g_value_get_boolean (value);

      if (priv->has_entry)
        {
          GtkWidget *child;

          child = gtk_bin_get_child (GTK_BIN (combo_box));

          gtk_entry_set_has_frame (GTK_ENTRY (child), priv->has_frame);
        }

      break;

    case PROP_FOCUS_ON_CLICK:
      gtk_combo_box_set_focus_on_click (combo_box,
                                        g_value_get_boolean (value));
      break;

    case PROP_TEAROFF_TITLE:
      gtk_combo_box_set_title (combo_box, g_value_get_string (value));
      break;

    case PROP_POPUP_SHOWN:
      if (g_value_get_boolean (value))
        gtk_combo_box_popup (combo_box);
      else
        gtk_combo_box_popdown (combo_box);
      break;

    case PROP_BUTTON_SENSITIVITY:
      gtk_combo_box_set_button_sensitivity (combo_box,
                                            g_value_get_enum (value));
      break;

    case PROP_POPUP_FIXED_WIDTH:
      gtk_combo_box_set_popup_fixed_width (combo_box,
                                           g_value_get_boolean (value));
      break;

    case PROP_EDITING_CANCELED:
      priv->editing_canceled = g_value_get_boolean (value);
      break;

    case PROP_HAS_ENTRY:
      priv->has_entry = g_value_get_boolean (value);
      break;

    case PROP_ENTRY_TEXT_COLUMN:
      gtk_combo_box_set_entry_text_column (combo_box, g_value_get_int (value));
      break;

    case PROP_ID_COLUMN:
      gtk_combo_box_set_id_column (combo_box, g_value_get_int (value));
      break;

    case PROP_ACTIVE_ID:
      gtk_combo_box_set_active_id (combo_box, g_value_get_string (value));
      break;

    case PROP_CELL_AREA:
      /* Construct-only, can only be assigned once */
      area = g_value_get_object (value);
      if (area)
        {
          if (priv->area != NULL)
            {
              g_warning ("cell-area has already been set, ignoring construct property");
              g_object_ref_sink (area);
              g_object_unref (area);
            }
          else
            priv->area = g_object_ref_sink (area);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_combo_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = combo_box->priv;

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value, combo_box->priv->model);
        break;

      case PROP_WRAP_WIDTH:
        g_value_set_int (value, combo_box->priv->wrap_width);
        break;

      case PROP_ROW_SPAN_COLUMN:
        g_value_set_int (value, combo_box->priv->row_column);
        break;

      case PROP_COLUMN_SPAN_COLUMN:
        g_value_set_int (value, combo_box->priv->col_column);
        break;

      case PROP_ACTIVE:
        g_value_set_int (value, gtk_combo_box_get_active (combo_box));
        break;

      case PROP_ADD_TEAROFFS:
        g_value_set_boolean (value, gtk_combo_box_get_add_tearoffs (combo_box));
        break;

      case PROP_HAS_FRAME:
        g_value_set_boolean (value, combo_box->priv->has_frame);
        break;

      case PROP_FOCUS_ON_CLICK:
        g_value_set_boolean (value, combo_box->priv->focus_on_click);
        break;

      case PROP_TEAROFF_TITLE:
        g_value_set_string (value, gtk_combo_box_get_title (combo_box));
        break;

      case PROP_POPUP_SHOWN:
        g_value_set_boolean (value, combo_box->priv->popup_shown);
        break;

      case PROP_BUTTON_SENSITIVITY:
        g_value_set_enum (value, combo_box->priv->button_sensitivity);
        break;

      case PROP_POPUP_FIXED_WIDTH:
        g_value_set_boolean (value, combo_box->priv->popup_fixed_width);
        break;

      case PROP_EDITING_CANCELED:
        g_value_set_boolean (value, priv->editing_canceled);
        break;

      case PROP_HAS_ENTRY:
        g_value_set_boolean (value, priv->has_entry);
        break;

      case PROP_ENTRY_TEXT_COLUMN:
        g_value_set_int (value, priv->text_column);
        break;

      case PROP_ID_COLUMN:
        g_value_set_int (value, priv->id_column);
        break;

      case PROP_ACTIVE_ID:
        g_value_set_string (value, gtk_combo_box_get_active_id (combo_box));
        break;

      case PROP_CELL_AREA:
        g_value_set_object (value, priv->area);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_state_flags_changed (GtkWidget     *widget,
                                   GtkStateFlags  previous)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (gtk_widget_get_realized (widget))
    {
      if (priv->tree_view && priv->cell_view)
        {
          GtkStyleContext *context;
          GtkStateFlags state;
          GdkRGBA color;

          context  = gtk_widget_get_style_context (widget);
          state = gtk_widget_get_state_flags (widget);
          gtk_style_context_get_background_color (context, state, &color);

          gtk_cell_view_set_background_rgba (GTK_CELL_VIEW (priv->cell_view), &color);
        }
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_combo_box_button_state_flags_changed (GtkWidget     *widget,
                                          GtkStateFlags  previous,
                                          gpointer       data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (gtk_widget_get_realized (widget))
    {
      if (!priv->tree_view && priv->cell_view)
        gtk_widget_set_state_flags (priv->cell_view,
                                    gtk_widget_get_state_flags (widget),
                                    TRUE);
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_combo_box_invalidate_order_foreach (GtkWidget *widget)
{
  _gtk_widget_invalidate_style_context (widget, GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_SIBLING_POSITION);
}

static void
gtk_combo_box_invalidate_order (GtkComboBox *combo_box)
{
  gtk_container_forall (GTK_CONTAINER (combo_box),
                        (GtkCallback) gtk_combo_box_invalidate_order_foreach,
                        NULL);
}

static void
gtk_combo_box_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_direction)
{
  gtk_combo_box_invalidate_order (GTK_COMBO_BOX (widget));
}

static GtkWidgetPath *
gtk_combo_box_get_path_for_child (GtkContainer *container,
                                  GtkWidget    *child)
{
  GtkComboBoxPrivate *priv = GTK_COMBO_BOX (container)->priv;
  GtkWidgetPath *path;
  GtkWidget *widget;
  gboolean found = FALSE;
  GList *visible_children, *l;
  GtkWidgetPath *sibling_path;
  int pos;

  path = _gtk_widget_create_path (GTK_WIDGET (container));

  if (gtk_widget_get_visible (child))
    {
      visible_children = NULL;

      if (priv->button && gtk_widget_get_visible (priv->button))
        visible_children = g_list_prepend (visible_children, priv->button);

      if (priv->cell_view_frame && gtk_widget_get_visible (priv->cell_view_frame))
        visible_children = g_list_prepend (visible_children, priv->cell_view_frame);

      widget = gtk_bin_get_child (GTK_BIN (container));
      if (widget && gtk_widget_get_visible (widget))
        visible_children = g_list_prepend (visible_children, widget);

      if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL)
        visible_children = g_list_reverse (visible_children);

      pos = 0;

      for (l = visible_children; l; l = l->next)
        {
          widget = l->data;

          if (widget == child)
            {
              found = TRUE;
              break;
            }

          pos++;
        }
    }

  if (found)
    {
      sibling_path = gtk_widget_path_new ();

      for (l = visible_children; l; l = l->next)
        gtk_widget_path_append_for_widget (sibling_path, l->data);

      gtk_widget_path_append_with_siblings (path, sibling_path, pos);

      g_list_free (visible_children);
      gtk_widget_path_unref (sibling_path);
    }
  else
    {
      gtk_widget_path_append_for_widget (path, child);
    }

  return path;
}

static void
gtk_combo_box_check_appearance (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  gboolean appears_as_list;

  /* if wrap_width > 0, then we are in grid-mode and forced to use
   * unix style
   */
  if (priv->wrap_width)
    appears_as_list = FALSE;
  else
    gtk_widget_style_get (GTK_WIDGET (combo_box),
                          "appears-as-list", &appears_as_list,
                          NULL);
  
  if (appears_as_list)
    {
      /* Destroy all the menu mode widgets, if they exist. */
      if (GTK_IS_MENU (priv->popup_widget))
        gtk_combo_box_menu_destroy (combo_box);

      /* Create the list mode widgets, if they don't already exist. */
      if (!GTK_IS_TREE_VIEW (priv->tree_view))
        gtk_combo_box_list_setup (combo_box);
    }
  else
    {
      /* Destroy all the list mode widgets, if they exist. */
      if (GTK_IS_TREE_VIEW (priv->tree_view))
        gtk_combo_box_list_destroy (combo_box);

      /* Create the menu mode widgets, if they don't already exist. */
      if (!GTK_IS_MENU (priv->popup_widget))
        gtk_combo_box_menu_setup (combo_box, TRUE);
    }

  gtk_widget_style_get (GTK_WIDGET (combo_box),
                        "shadow-type", &priv->shadow_type,
                        NULL);
}

static void
gtk_combo_box_style_updated (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->style_updated (widget);

  gtk_combo_box_check_appearance (combo_box);

  if (priv->tree_view && priv->cell_view)
    {
      GtkStyleContext *context;
      GtkStateFlags state;
      GdkRGBA color;

      context  = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_background_color (context, state, &color);

      gtk_cell_view_set_background_rgba (GTK_CELL_VIEW (priv->cell_view), &color);
    }

  child = gtk_bin_get_child (GTK_BIN (combo_box));
  if (GTK_IS_ENTRY (child))
    g_object_set (child, "shadow-type",
                  GTK_SHADOW_NONE == priv->shadow_type ?
                  GTK_SHADOW_IN : GTK_SHADOW_NONE, NULL);
}

static void
gtk_combo_box_button_toggled (GtkWidget *widget,
                              gpointer   data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (!combo_box->priv->popup_in_progress)
        gtk_combo_box_popup (combo_box);
    }
  else
    gtk_combo_box_popdown (combo_box);
}

static void
gtk_combo_box_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->has_entry && !GTK_IS_ENTRY (widget))
    {
      g_warning ("Attempting to add a widget with type %s to a GtkComboBox that needs an entry "
                 "(need an instance of GtkEntry or of a subclass)",
                 G_OBJECT_TYPE_NAME (widget));
      return;
    }

  if (priv->cell_view &&
      gtk_widget_get_parent (priv->cell_view))
    {
      gtk_widget_unparent (priv->cell_view);
      _gtk_bin_set_child (GTK_BIN (container), NULL);
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
  
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
  _gtk_bin_set_child (GTK_BIN (container), widget);

  if (priv->cell_view &&
      widget != priv->cell_view)
    {
      /* since the cell_view was unparented, it's gone now */
      priv->cell_view = NULL;

      if (!priv->tree_view && priv->separator)
        {
          gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (priv->separator)),
                                priv->separator);
          priv->separator = NULL;

          gtk_widget_queue_resize (GTK_WIDGET (container));
        }
      else if (priv->cell_view_frame)
        {
          gtk_widget_unparent (priv->cell_view_frame);
          priv->cell_view_frame = NULL;
          priv->box = NULL;
        }
    }

  if (priv->has_entry)
    {
      /* this flag is a hack to tell the entry to fill its allocation.
       */
      _gtk_entry_set_is_cell_renderer (GTK_ENTRY (widget), TRUE);

      g_signal_connect (widget, "changed",
                        G_CALLBACK (gtk_combo_box_entry_contents_changed),
                        combo_box);

      gtk_entry_set_has_frame (GTK_ENTRY (widget), priv->has_frame);
    }
}

static void
gtk_combo_box_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *path;
  gboolean appears_as_list;

  if (priv->has_entry)
    {
      GtkWidget *child_widget;

      child_widget = gtk_bin_get_child (GTK_BIN (container));
      if (widget && widget == child_widget)
        {
          g_signal_handlers_disconnect_by_func (widget,
                                                gtk_combo_box_entry_contents_changed,
                                                container);
          _gtk_entry_set_is_cell_renderer (GTK_ENTRY (widget), FALSE);
        }
    }

  if (widget == priv->cell_view)
    priv->cell_view = NULL;

  gtk_widget_unparent (widget);
  _gtk_bin_set_child (GTK_BIN (container), NULL);

  if (gtk_widget_in_destruction (GTK_WIDGET (combo_box)))
    return;

  gtk_widget_queue_resize (GTK_WIDGET (container));

  if (!priv->tree_view)
    appears_as_list = FALSE;
  else
    appears_as_list = TRUE;
  
  if (appears_as_list)
    gtk_combo_box_list_destroy (combo_box);
  else if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_combo_box_menu_destroy (combo_box);
      gtk_menu_detach (GTK_MENU (priv->popup_widget));
      priv->popup_widget = NULL;
    }

  if (!priv->cell_view)
    {
      priv->cell_view = gtk_cell_view_new ();
      gtk_widget_set_parent (priv->cell_view, GTK_WIDGET (container));
      _gtk_bin_set_child (GTK_BIN (container), priv->cell_view);

      gtk_widget_show (priv->cell_view);
      gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view),
                               priv->model);
    }


  if (appears_as_list)
    gtk_combo_box_list_setup (combo_box);
  else
    gtk_combo_box_menu_setup (combo_box, TRUE);

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      path = gtk_tree_row_reference_get_path (priv->active_row);
      gtk_combo_box_set_active_internal (combo_box, path);
      gtk_tree_path_free (path);
    }
  else
    gtk_combo_box_set_active_internal (combo_box, NULL);
}

static void
gtk_combo_box_menu_show (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  gtk_combo_box_child_show (menu, user_data);

  priv->popup_in_progress = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                TRUE);
  priv->popup_in_progress = FALSE;
}

static void
gtk_combo_box_menu_hide (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_combo_box_child_hide (menu,user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                FALSE);
}

static void
gtk_combo_box_detacher (GtkWidget *widget,
                        GtkMenu          *menu)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  g_return_if_fail (priv->popup_widget == (GtkWidget *) menu);

  g_signal_handlers_disconnect_by_func (menu->priv->toplevel,
                                        gtk_combo_box_menu_show,
                                        combo_box);
  g_signal_handlers_disconnect_by_func (menu->priv->toplevel,
                                        gtk_combo_box_menu_hide,
                                        combo_box);
  
  priv->popup_widget = NULL;
}

static void
gtk_combo_box_set_popup_widget (GtkComboBox *combo_box,
                                GtkWidget   *popup)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_menu_detach (GTK_MENU (priv->popup_widget));
      priv->popup_widget = NULL;
    }
  else if (priv->popup_widget)
    {
      gtk_container_remove (GTK_CONTAINER (priv->scrolled_window),
                            priv->popup_widget);
      g_object_unref (priv->popup_widget);
      priv->popup_widget = NULL;
    }

  if (GTK_IS_MENU (popup))
    {
      if (priv->popup_window)
        {
          gtk_widget_destroy (priv->popup_window);
          priv->popup_window = NULL;
        }

      priv->popup_widget = popup;

      /*
       * Note that we connect to show/hide on the toplevel, not the
       * menu itself, since the menu is not shown/hidden when it is
       * popped up while torn-off.
       */
      g_signal_connect (GTK_MENU (popup)->priv->toplevel, "show",
                        G_CALLBACK (gtk_combo_box_menu_show), combo_box);
      g_signal_connect (GTK_MENU (popup)->priv->toplevel, "hide",
                        G_CALLBACK (gtk_combo_box_menu_hide), combo_box);

      gtk_menu_attach_to_widget (GTK_MENU (popup),
                                 GTK_WIDGET (combo_box),
                                 gtk_combo_box_detacher);
    }
  else
    {
      if (!priv->popup_window)
        {
          GtkWidget *toplevel;

          priv->popup_window = gtk_window_new (GTK_WINDOW_POPUP);
          gtk_widget_set_name (priv->popup_window, "gtk-combobox-popup-window");

          gtk_window_set_type_hint (GTK_WINDOW (priv->popup_window),
                                    GDK_WINDOW_TYPE_HINT_COMBO);

          g_signal_connect (GTK_WINDOW (priv->popup_window),"show",
                            G_CALLBACK (gtk_combo_box_child_show),
                            combo_box);
          g_signal_connect (GTK_WINDOW (priv->popup_window),"hide",
                            G_CALLBACK (gtk_combo_box_child_hide),
                            combo_box);

          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
          if (GTK_IS_WINDOW (toplevel))
            {
              gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                           GTK_WINDOW (priv->popup_window));
              gtk_window_set_transient_for (GTK_WINDOW (priv->popup_window),
                                            GTK_WINDOW (toplevel));
            }

          gtk_window_set_resizable (GTK_WINDOW (priv->popup_window), FALSE);
          gtk_window_set_screen (GTK_WINDOW (priv->popup_window),
                                 gtk_widget_get_screen (GTK_WIDGET (combo_box)));

          priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);

          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                          GTK_POLICY_NEVER,
                                          GTK_POLICY_NEVER);
          gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                               GTK_SHADOW_IN);

          gtk_widget_show (priv->scrolled_window);

          gtk_container_add (GTK_CONTAINER (priv->popup_window),
                             priv->scrolled_window);
        }

      gtk_container_add (GTK_CONTAINER (priv->scrolled_window),
                         popup);

      gtk_widget_show (popup);
      g_object_ref (popup);
      priv->popup_widget = popup;
    }
}

static void
get_widget_padding_and_border (GtkWidget *widget,
                               GtkBorder *padding)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_get_padding (context, state, padding);
  gtk_style_context_get_border (context, state, &tmp);

  padding->top += tmp.top;
  padding->right += tmp.right;
  padding->bottom += tmp.bottom;
  padding->left += tmp.left;
}

static void
gtk_combo_box_menu_position_below (GtkMenu  *menu,
                                   gint     *x,
                                   gint     *y,
                                   gint     *push_in,
                                   gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkAllocation child_allocation;
  gint sx, sy;
  GtkWidget *child;
  GtkRequisition req;
  GdkScreen *screen;
  gint monitor_num;
  GdkRectangle monitor;
  GtkBorder padding;

  /* FIXME: is using the size request here broken? */
  child = gtk_bin_get_child (GTK_BIN (combo_box));

  sx = sy = 0;

  gtk_widget_get_allocation (child, &child_allocation);

  if (!gtk_widget_get_has_window (child))
    {
      sx += child_allocation.x;
      sy += child_allocation.y;
    }

  gdk_window_get_root_coords (gtk_widget_get_window (child),
                              sx, sy, &sx, &sy);
  get_widget_padding_and_border (GTK_WIDGET (combo_box), &padding);

  if (gtk_widget_get_direction (GTK_WIDGET (combo_box)) == GTK_TEXT_DIR_RTL)
    sx += padding.left;
  else
    sx -= padding.left;

  if (combo_box->priv->popup_fixed_width)
    gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);
  else
    gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, &req);

  if (gtk_widget_get_direction (GTK_WIDGET (combo_box)) == GTK_TEXT_DIR_LTR)
    *x = sx;
  else
    *x = sx + child_allocation.width - req.width;
  *y = sy;

  screen = gtk_widget_get_screen (GTK_WIDGET (combo_box));
  monitor_num = gdk_screen_get_monitor_at_window (screen,
                                                  gtk_widget_get_window (GTK_WIDGET (combo_box)));
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  if (*x < monitor.x)
    *x = monitor.x;
  else if (*x + req.width > monitor.x + monitor.width)
    *x = monitor.x + monitor.width - req.width;

  if (monitor.y + monitor.height - *y - child_allocation.height >= req.height)
    *y += child_allocation.height;
  else if (*y - monitor.y >= req.height)
    *y -= req.height;
  else if (monitor.y + monitor.height - *y - child_allocation.height > *y - monitor.y)
    *y += child_allocation.height;
  else
    *y -= req.height;

   *push_in = FALSE;
}

static void
gtk_combo_box_menu_position_over (GtkMenu  *menu,
                                  gint     *x,
                                  gint     *y,
                                  gboolean *push_in,
                                  gpointer  user_data)
{
  GtkComboBox    *combo_box;
  GtkWidget      *active;
  GtkWidget      *child;
  GtkWidget      *widget;
  GtkAllocation   allocation;
  GtkAllocation   child_allocation;
  GList          *children;
  gint            screen_width;
  gint            menu_xpos;
  gint            menu_ypos;
  gint            menu_width;

  combo_box = GTK_COMBO_BOX (user_data);
  widget = GTK_WIDGET (combo_box);

  active = gtk_menu_get_active (GTK_MENU (combo_box->priv->popup_widget));

  gtk_widget_get_allocation (widget, &allocation);

  menu_xpos = allocation.x;
  menu_ypos = allocation.y + allocation.height / 2 - 2;

  if (combo_box->priv->popup_fixed_width)
    gtk_widget_get_preferred_width (GTK_WIDGET (menu), &menu_width, NULL);
  else
    gtk_widget_get_preferred_width (GTK_WIDGET (menu), NULL, &menu_width);

  if (active != NULL)
    {
      gtk_widget_get_allocation (active, &child_allocation);
      menu_ypos -= child_allocation.height / 2;
    }

  children = GTK_MENU_SHELL (combo_box->priv->popup_widget)->priv->children;
  while (children)
    {
      child = children->data;

      if (active == child)
        break;

      if (gtk_widget_get_visible (child))
        {
          gtk_widget_get_allocation (child, &child_allocation);

          menu_ypos -= child_allocation.height;
        }

      children = children->next;
    }

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    menu_xpos = menu_xpos + allocation.width - menu_width;

  gdk_window_get_root_coords (gtk_widget_get_window (widget),
                              menu_xpos, menu_ypos,
                              &menu_xpos, &menu_ypos);

  /* Clamp the position on screen */
  screen_width = gdk_screen_get_width (gtk_widget_get_screen (widget));
  
  if (menu_xpos < 0)
    menu_xpos = 0;
  else if ((menu_xpos + menu_width) > screen_width)
    menu_xpos -= ((menu_xpos + menu_width) - screen_width);

  *x = menu_xpos;
  *y = menu_ypos;

  *push_in = TRUE;
}

static void
gtk_combo_box_menu_position (GtkMenu  *menu,
                             gint     *x,
                             gint     *y,
                             gint     *push_in,
                             gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *menu_item;

  if (priv->wrap_width > 0 || priv->cell_view == NULL)
    gtk_combo_box_menu_position_below (menu, x, y, push_in, user_data);
  else
    {
      /* FIXME handle nested menus better */
      menu_item = gtk_menu_get_active (GTK_MENU (priv->popup_widget));
      if (menu_item)
        gtk_menu_shell_select_item (GTK_MENU_SHELL (priv->popup_widget),
                                    menu_item);

      gtk_combo_box_menu_position_over (menu, x, y, push_in, user_data);
    }

  if (!gtk_widget_get_visible (GTK_MENU (priv->popup_widget)->priv->toplevel))
    gtk_window_set_type_hint (GTK_WINDOW (GTK_MENU (priv->popup_widget)->priv->toplevel),
                              GDK_WINDOW_TYPE_HINT_COMBO);
}

static void
gtk_combo_box_list_position (GtkComboBox *combo_box,
                             gint        *x,
                             gint        *y,
                             gint        *width,
                             gint        *height)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkAllocation allocation;
  GdkScreen *screen;
  gint monitor_num;
  GdkRectangle monitor;
  GtkRequisition popup_req;
  GtkPolicyType hpolicy, vpolicy;
  GdkWindow *window;

  /* under windows, the drop down list is as wide as the combo box itself.
     see bug #340204 */
  GtkWidget *widget = GTK_WIDGET (combo_box);

  *x = *y = 0;

  gtk_widget_get_allocation (widget, &allocation);

  if (!gtk_widget_get_has_window (widget))
    {
      *x += allocation.x;
      *y += allocation.y;
    }

  window = gtk_widget_get_window (widget);

  gdk_window_get_root_coords (gtk_widget_get_window (widget),
                              *x, *y, x, y);

  *width = allocation.width;

  hpolicy = vpolicy = GTK_POLICY_NEVER;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  hpolicy, vpolicy);

  if (combo_box->priv->popup_fixed_width)
    {
      gtk_widget_get_preferred_size (priv->scrolled_window, &popup_req, NULL);

      if (popup_req.width > *width)
        {
          hpolicy = GTK_POLICY_ALWAYS;
          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                          hpolicy, vpolicy);
        }
    }
  else
    {
      /* XXX This code depends on treeviews properly reporting their natural width
       * list-mode menus won't fill up to their natural width until then */
      gtk_widget_get_preferred_size (priv->scrolled_window, NULL, &popup_req);

      if (popup_req.width > *width)
        {
          hpolicy = GTK_POLICY_NEVER;
          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                          hpolicy, vpolicy);

          *width = popup_req.width;
        }
    }

  *height = popup_req.height;

  screen = gtk_widget_get_screen (GTK_WIDGET (combo_box));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  if (gtk_widget_get_direction (GTK_WIDGET (combo_box)) == GTK_TEXT_DIR_RTL)
    *x = *x + allocation.width - *width;

  if (*x < monitor.x)
    *x = monitor.x;
  else if (*x + *width > monitor.x + monitor.width)
    *x = monitor.x + monitor.width - *width;

  if (*y + allocation.height + *height <= monitor.y + monitor.height)
    *y += allocation.height;
  else if (*y - *height >= monitor.y)
    *y -= *height;
  else if (monitor.y + monitor.height - (*y + allocation.height) > *y - monitor.y)
    {
      *y += allocation.height;
      *height = monitor.y + monitor.height - *y;
    }
  else
    {
      *height = *y - monitor.y;
      *y = monitor.y;
    }

  if (popup_req.height > *height)
    {
      vpolicy = GTK_POLICY_ALWAYS;

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                      hpolicy, vpolicy);
    }
}

static gboolean
cell_layout_is_sensitive (GtkCellLayout *layout)
{
  GList *cells, *list;
  gboolean sensitive;

  cells = gtk_cell_layout_get_cells (layout);

  sensitive = FALSE;
  for (list = cells; list; list = list->next)
    {
      g_object_get (list->data, "sensitive", &sensitive, NULL);

      if (sensitive)
        break;
    }
  g_list_free (cells);

  return sensitive;
}

static gboolean
cell_is_sensitive (GtkCellRenderer *cell,
                   gpointer         data)
{
  gboolean *sensitive = data;

  g_object_get (cell, "sensitive", sensitive, NULL);

  return *sensitive;
}

static gboolean
tree_column_row_is_sensitive (GtkComboBox *combo_box,
                              GtkTreeIter *iter)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->row_separator_func)
    {
      if (priv->row_separator_func (priv->model, iter,
                                    priv->row_separator_data))
        return FALSE;
    }

  if (priv->area)
    {
      gboolean sensitive;

      gtk_cell_area_apply_attributes (priv->area, priv->model, iter, FALSE, FALSE);

      sensitive = FALSE;

      gtk_cell_area_foreach (priv->area, cell_is_sensitive, &sensitive);

      return sensitive;
    }

  return TRUE;
}

static void
update_menu_sensitivity (GtkComboBox *combo_box,
                         GtkWidget   *menu)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GList *children, *child;
  GtkWidget *item, *submenu, *separator;
  GtkWidget *cell_view;
  gboolean sensitive;

  if (!priv->model)
    return;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (child = children; child; child = child->next)
    {
      item = GTK_WIDGET (child->data);
      cell_view = gtk_bin_get_child (GTK_BIN (item));

      if (!GTK_IS_CELL_VIEW (cell_view))
        continue;

      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));
      if (submenu != NULL)
        {
          gtk_widget_set_sensitive (item, TRUE);
          update_menu_sensitivity (combo_box, submenu);
        }
      else
        {
          sensitive = cell_layout_is_sensitive (GTK_CELL_LAYOUT (cell_view));

          if (menu != priv->popup_widget && child == children)
            {
              separator = GTK_WIDGET (child->next->data);
              g_object_set (item, "visible", sensitive, NULL);
              g_object_set (separator, "visible", sensitive, NULL);
            }
          else
            gtk_widget_set_sensitive (item, sensitive);
        }
    }

  g_list_free (children);
}

static void
gtk_combo_box_menu_popup (GtkComboBox *combo_box,
                          guint        button,
                          guint32      activate_time)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *path;
  gint active_item;
  gint width, min_width, nat_width;

  update_menu_sensitivity (combo_box, priv->popup_widget);

  active_item = -1;
  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      path = gtk_tree_row_reference_get_path (priv->active_row);
      active_item = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);

      if (priv->add_tearoffs)
        active_item++;
    }

  /* FIXME handle nested menus better */
  gtk_menu_set_active (GTK_MENU (priv->popup_widget), active_item);

  if (priv->wrap_width == 0)
    {
      GtkAllocation allocation;

      gtk_widget_get_allocation (GTK_WIDGET (combo_box), &allocation);
      width = allocation.width;
      gtk_widget_set_size_request (priv->popup_widget, -1, -1);
      gtk_widget_get_preferred_width (priv->popup_widget, &min_width, &nat_width);

      if (combo_box->priv->popup_fixed_width)
        width = MAX (width, min_width);
      else
        width = MAX (width, nat_width);

      gtk_widget_set_size_request (priv->popup_widget, width, -1);
    }

  gtk_menu_popup (GTK_MENU (priv->popup_widget),
                  NULL, NULL,
                  gtk_combo_box_menu_position, combo_box,
                  button, activate_time);
}

static gboolean
popup_grab_on_window (GdkWindow *window,
                      GdkDevice *keyboard,
                      GdkDevice *pointer,
                      guint32    activate_time)
{
  if (keyboard &&
      gdk_device_grab (keyboard, window,
                       GDK_OWNERSHIP_WINDOW, TRUE,
                       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                       NULL, activate_time) != GDK_GRAB_SUCCESS)
    return FALSE;

  if (pointer &&
      gdk_device_grab (pointer, window,
                       GDK_OWNERSHIP_WINDOW, TRUE,
                       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                       GDK_POINTER_MOTION_MASK,
                       NULL, activate_time) != GDK_GRAB_SUCCESS)
    {
      if (keyboard)
        gdk_device_ungrab (keyboard, activate_time);

      return FALSE;
    }

  return TRUE;
}

static gboolean
gtk_combo_box_grab_broken_event (GtkWidget          *widget,
                                 GdkEventGrabBroken *event,
                                 gpointer            user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (event->grab_window == NULL)
    gtk_combo_box_popdown (combo_box);

  return TRUE;
}

/**
 * gtk_combo_box_popup:
 * @combo_box: a #GtkComboBox
 *
 * Pops up the menu or dropdown list of @combo_box.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Since: 2.4
 */
void
gtk_combo_box_popup (GtkComboBox *combo_box)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  g_signal_emit (combo_box, combo_box_signals[POPUP], 0);
}

/**
 * gtk_combo_box_popup_for_device:
 * @combo_box: a #GtkComboBox
 * @device: a #GdkDevice
 *
 * Pops up the menu or dropdown list of @combo_box, the popup window
 * will be grabbed so only @device and its associated pointer/keyboard
 * are the only #GdkDevice<!-- -->s able to send events to it.
 *
 * Since: 3.0
 **/
void
gtk_combo_box_popup_for_device (GtkComboBox *combo_box,
                                GdkDevice   *device)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  gint x, y, width, height;
  GtkTreePath *path = NULL, *ppath;
  GtkWidget *toplevel;
  GdkDevice *keyboard, *pointer;
  guint32 time;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (!gtk_widget_get_realized (GTK_WIDGET (combo_box)))
    return;

  if (gtk_widget_get_mapped (priv->popup_widget))
    return;

  if (priv->grab_pointer && priv->grab_keyboard)
    return;

  time = gtk_get_current_event_time ();

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      keyboard = device;
      pointer = gdk_device_get_associated_device (device);
    }
  else
    {
      pointer = device;
      keyboard = gdk_device_get_associated_device (device);
    }

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_combo_box_menu_popup (combo_box,
                                priv->activate_button,
                                priv->activate_time);
      return;
    }

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
  if (GTK_IS_WINDOW (toplevel))
    gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                 GTK_WINDOW (priv->popup_window));

  gtk_widget_show_all (priv->scrolled_window);
  gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);

  gtk_widget_set_size_request (priv->popup_window, width, height);
  gtk_window_move (GTK_WINDOW (priv->popup_window), x, y);

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      path = gtk_tree_row_reference_get_path (priv->active_row);
      ppath = gtk_tree_path_copy (path);
      if (gtk_tree_path_up (ppath))
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->tree_view),
                                      ppath);
      gtk_tree_path_free (ppath);
    }
  gtk_tree_view_set_hover_expand (GTK_TREE_VIEW (priv->tree_view),
                                  TRUE);

  /* popup */
  gtk_widget_show (priv->popup_window);

  if (path)
    {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view),
                                path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  gtk_widget_grab_focus (priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                TRUE);

  if (!gtk_widget_has_focus (priv->tree_view))
    gtk_widget_grab_focus (priv->tree_view);

  if (!popup_grab_on_window (gtk_widget_get_window (priv->popup_window),
                             keyboard, pointer, time))
    {
      gtk_widget_hide (priv->popup_window);
      return;
    }

  gtk_device_grab_add (priv->popup_window, pointer, TRUE);
  priv->grab_pointer = pointer;
  priv->grab_keyboard = keyboard;

  g_signal_connect (priv->popup_window,
                    "grab-broken-event",
                    G_CALLBACK (gtk_combo_box_grab_broken_event),
                    combo_box);
}

static void
gtk_combo_box_real_popup (GtkComboBox *combo_box)
{
  GdkDevice *device;

  device = gtk_get_current_event_device ();

  if (!device)
    {
      GdkDeviceManager *device_manager;
      GdkDisplay *display;
      GList *devices;

      display = gtk_widget_get_display (GTK_WIDGET (combo_box));
      device_manager = gdk_display_get_device_manager (display);

      /* No device was set, pick the first master device */
      devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
      device = devices->data;
      g_list_free (devices);
    }

  gtk_combo_box_popup_for_device (combo_box, device);
}

static gboolean
gtk_combo_box_real_popdown (GtkComboBox *combo_box)
{
  if (combo_box->priv->popup_shown)
    {
      gtk_combo_box_popdown (combo_box);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_combo_box_popdown:
 * @combo_box: a #GtkComboBox
 *
 * Hides the menu or dropdown list of @combo_box.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Since: 2.4
 */
void
gtk_combo_box_popdown (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (GTK_IS_MENU (priv->popup_widget))
    {
      gtk_menu_popdown (GTK_MENU (priv->popup_widget));
      return;
    }

  if (!gtk_widget_get_realized (GTK_WIDGET (combo_box)))
    return;

  if (priv->grab_keyboard)
    gdk_device_ungrab (priv->grab_keyboard, GDK_CURRENT_TIME);
  gdk_device_ungrab (priv->grab_pointer, GDK_CURRENT_TIME);

  gtk_device_grab_remove (priv->popup_window, priv->grab_pointer);
  gtk_widget_hide (priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                FALSE);

  priv->grab_pointer = NULL;
  priv->grab_keyboard = NULL;
}

#define GTK_COMBO_BOX_SIZE_ALLOCATE_BUTTON                      \
  GtkAllocation button_allocation;                              \
  gtk_widget_get_preferred_size (combo_box->priv->button,       \
                                 &req, NULL);                   \
                                                                \
  if (is_rtl)                                                   \
    button_allocation.x = allocation->x;                        \
  else                                                          \
    button_allocation.x = allocation->x + allocation->width     \
     - req.width;                                               \
                                                                \
  button_allocation.y = allocation->y;                          \
  button_allocation.width = MAX (1, req.width);                 \
  button_allocation.height = allocation->height;                \
  button_allocation.height = MAX (1, button_allocation.height); \
                                                                \
  gtk_widget_size_allocate (combo_box->priv->button,            \
                            &button_allocation);


static void
gtk_combo_box_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child_widget;
  GtkAllocation child;
  GtkRequisition req;
  gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  GtkBorder padding;

  gtk_widget_set_allocation (widget, allocation);
  child_widget = gtk_bin_get_child (GTK_BIN (widget));
  get_widget_padding_and_border (widget, &padding);

  allocation->x += padding.left;
  allocation->y += padding.top;
  allocation->width -= padding.left + padding.right;
  allocation->height -= padding.top + padding.bottom;

  if (!priv->tree_view)
    {
      if (priv->cell_view)
        {
          GtkBorder button_padding;
          gint width;
          guint border_width;

          border_width = gtk_container_get_border_width (GTK_CONTAINER (priv->button));
          get_widget_padding_and_border (priv->button, &button_padding);

          /* menu mode; child_widget is priv->cell_view.
           * Allocate the button to the full combobox allocation (minus the
           * padding).
           */
          gtk_widget_size_allocate (priv->button, allocation);

          child.x = allocation->x;
          child.y = allocation->y;
          width = allocation->width;
          child.height = allocation->height;

          if (!priv->is_cell_renderer)
            {
              /* restrict allocation of the child into the button box
               * if we're not in cell renderer mode.
               */
              child.x += border_width + button_padding.left;
              child.y += border_width + button_padding.top;
              width -= 2 * border_width +
                button_padding.left + button_padding.right;
              child.height -= 2 * border_width +
                button_padding.top + button_padding.bottom;
            }

          /* allocate the box containing the separator and the arrow */
          gtk_widget_get_preferred_size (priv->box, &req, NULL);
          child.width = req.width;
          if (!is_rtl)
            child.x += width - req.width;
          child.width = MAX (1, child.width);
          child.height = MAX (1, child.height);
          gtk_widget_size_allocate (priv->box, &child);

          if (is_rtl)
            {
              child.x += req.width;
              child.width = allocation->x + allocation->width
                - border_width - child.x - button_padding.right;
            }
          else
            {
              child.width = child.x;
              child.x = allocation->x
                + border_width + button_padding.left;
              child.width -= child.x;
            }

          if (gtk_widget_get_visible (priv->popup_widget))
            {
              gint width, menu_width;

              if (priv->wrap_width == 0)
                {
                  GtkAllocation combo_box_allocation;

                  gtk_widget_get_allocation (GTK_WIDGET (combo_box), &combo_box_allocation);
                  width = combo_box_allocation.width;
                  gtk_widget_set_size_request (priv->popup_widget, -1, -1);

                  if (combo_box->priv->popup_fixed_width)
                    gtk_widget_get_preferred_width (priv->popup_widget, &menu_width, NULL);
                  else
                    gtk_widget_get_preferred_width (priv->popup_widget, NULL, &menu_width);

                  gtk_widget_set_size_request (priv->popup_widget,
                                               MAX (width, menu_width), -1);
               }

              /* reposition the menu after giving it a new width */
              gtk_menu_reposition (GTK_MENU (priv->popup_widget));
            }

          child.width = MAX (1, child.width);
          child.height = MAX (1, child.height);
          gtk_widget_size_allocate (child_widget, &child);
        }
      else
        {
          /* menu mode; child_widget has been set with gtk_container_add().
           * E.g. it might be a GtkEntry if priv->has_entry is TRUE.
           * Allocate the button at the far end, according to the direction
           * of the widget.
           */
          GTK_COMBO_BOX_SIZE_ALLOCATE_BUTTON

            /* After the macro, button_allocation has the button allocation rect */

          if (is_rtl)
            child.x = button_allocation.x + button_allocation.width;
          else
            child.x = allocation->x;

          child.y = allocation->y;
          child.width = allocation->width - button_allocation.width;
          child.height = button_allocation.height;

          child.width = MAX (1, child.width);

          gtk_widget_size_allocate (child_widget, &child);
        }
    }
  else
    {
      /* list mode; child_widget might be either priv->cell_view or a child
       * added with gtk_container_add().
       */

      /* After the macro, button_allocation has the button allocation rect */
      GTK_COMBO_BOX_SIZE_ALLOCATE_BUTTON

      if (is_rtl)
        child.x = button_allocation.x + button_allocation.width;
      else
        child.x = allocation->x;

      child.y = allocation->y;
      child.width = allocation->width - button_allocation.width;
      child.height = button_allocation.height;

      if (priv->cell_view_frame)
        {
          gtk_widget_size_allocate (priv->cell_view_frame, &child);

          /* restrict allocation of the child into the frame box if it's present */
          if (priv->has_frame)
            {
              GtkBorder frame_padding;
              guint border_width;

              border_width = gtk_container_get_border_width (GTK_CONTAINER (priv->cell_view_frame));
              get_widget_padding_and_border (priv->cell_view_frame, &frame_padding);

              child.x += border_width + frame_padding.left;
              child.y += border_width + frame_padding.right;
              child.width -= (2 * border_width) + frame_padding.left + frame_padding.right;
              child.height -= (2 * border_width) + frame_padding.top + frame_padding.bottom;
            }
        }

      if (gtk_widget_get_visible (priv->popup_window))
        {
          gint x, y, width, height;
          gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);
          gtk_window_move (GTK_WINDOW (priv->popup_window), x, y);
          gtk_widget_set_size_request (priv->popup_window, width, height);
        }

      /* allocate the child */
      child.width = MAX (1, child.width);
      child.height = MAX (1, child.height);
      gtk_widget_size_allocate (child_widget, &child);
    }
}

#undef GTK_COMBO_BOX_ALLOCATE_BUTTON

static void
gtk_combo_box_unset_model (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->model)
    {
      g_signal_handler_disconnect (priv->model,
                                   priv->inserted_id);
      g_signal_handler_disconnect (priv->model,
                                   priv->deleted_id);
      g_signal_handler_disconnect (priv->model,
                                   priv->reordered_id);
      g_signal_handler_disconnect (priv->model,
                                   priv->changed_id);
    }

  if (priv->model)
    {
      g_object_unref (priv->model);
      priv->model = NULL;
    }

  if (priv->active_row)
    {
      gtk_tree_row_reference_free (priv->active_row);
      priv->active_row = NULL;
    }

  if (priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view), NULL);
}

static void
gtk_combo_box_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;

  if (include_internals)
    {
      if (priv->button)
        (* callback) (priv->button, callback_data);
      if (priv->cell_view_frame)
        (* callback) (priv->cell_view_frame, callback_data);
    }

  child = gtk_bin_get_child (GTK_BIN (container));
  if (child)
    (* callback) (child, callback_data);
}

static void
gtk_combo_box_child_show (GtkWidget *widget,
                          GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  priv->popup_shown = TRUE;
  g_object_notify (G_OBJECT (combo_box), "popup-shown");
}

static void
gtk_combo_box_child_hide (GtkWidget *widget,
                          GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  priv->popup_shown = FALSE;
  g_object_notify (G_OBJECT (combo_box), "popup-shown");
}

static gboolean
gtk_combo_box_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (priv->shadow_type != GTK_SHADOW_NONE)
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (widget);

      gtk_render_background (context, cr, 0, 0,
                             gtk_widget_get_allocated_width (widget),
                             gtk_widget_get_allocated_height (widget));
      gtk_render_frame (context, cr, 0, 0,
                        gtk_widget_get_allocated_width (widget),
                        gtk_widget_get_allocated_height (widget));
    }

  gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                priv->button, cr);

  if (priv->tree_view && priv->cell_view_frame)
    {
      gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                    priv->cell_view_frame, cr);
    }

  gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                gtk_bin_get_child (GTK_BIN (widget)),
                                cr);

  return FALSE;
}

typedef struct {
  GtkComboBox *combo;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean found;
  gboolean set;
  gboolean visible;
} SearchData;

static gboolean
path_visible (GtkTreeView *view,
              GtkTreePath *path)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  /* Note that we rely on the fact that collapsed rows don't have nodes
   */
  return _gtk_tree_view_find_node (view, path, &tree, &node);
}

static gboolean
tree_next_func (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (search_data->found)
    {
      if (!tree_column_row_is_sensitive (search_data->combo, iter))
        return FALSE;

      if (search_data->visible &&
          !path_visible (GTK_TREE_VIEW (search_data->combo->priv->tree_view), path))
        return FALSE;

      search_data->set = TRUE;
      search_data->iter = *iter;

      return TRUE;
    }

  if (gtk_tree_path_compare (path, search_data->path) == 0)
    search_data->found = TRUE;

  return FALSE;
}

static gboolean
tree_next (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *iter,
           GtkTreeIter  *next,
           gboolean      visible)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.path = gtk_tree_model_get_path (model, iter);
  search_data.visible = visible;
  search_data.found = FALSE;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_next_func, &search_data);

  *next = search_data.iter;

  gtk_tree_path_free (search_data.path);

  return search_data.set;
}

static gboolean
tree_prev_func (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (gtk_tree_path_compare (path, search_data->path) == 0)
    {
      search_data->found = TRUE;
      return TRUE;
    }

  if (!tree_column_row_is_sensitive (search_data->combo, iter))
    return FALSE;

  if (search_data->visible &&
      !path_visible (GTK_TREE_VIEW (search_data->combo->priv->tree_view), path))
    return FALSE;

  search_data->set = TRUE;
  search_data->iter = *iter;

  return FALSE;
}

static gboolean
tree_prev (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *iter,
           GtkTreeIter  *prev,
           gboolean      visible)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.path = gtk_tree_model_get_path (model, iter);
  search_data.visible = visible;
  search_data.found = FALSE;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_prev_func, &search_data);

  *prev = search_data.iter;

  gtk_tree_path_free (search_data.path);

  return search_data.set;
}

static gboolean
tree_last_func (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (!tree_column_row_is_sensitive (search_data->combo, iter))
    return FALSE;

  /* Note that we rely on the fact that collapsed rows don't have nodes
   */
  if (search_data->visible &&
      !path_visible (GTK_TREE_VIEW (search_data->combo->priv->tree_view), path))
    return FALSE;

  search_data->set = TRUE;
  search_data->iter = *iter;

  return FALSE;
}

static gboolean
tree_last (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *last,
           gboolean      visible)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.visible = visible;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_last_func, &search_data);

  *last = search_data.iter;

  return search_data.set;
}


static gboolean
tree_first_func (GtkTreeModel *model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      data)
{
  SearchData *search_data = (SearchData *)data;

  if (!tree_column_row_is_sensitive (search_data->combo, iter))
    return FALSE;

  if (search_data->visible &&
      !path_visible (GTK_TREE_VIEW (search_data->combo->priv->tree_view), path))
    return FALSE;

  search_data->set = TRUE;
  search_data->iter = *iter;

  return TRUE;
}

static gboolean
tree_first (GtkComboBox  *combo,
            GtkTreeModel *model,
            GtkTreeIter  *first,
            gboolean      visible)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.visible = visible;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_first_func, &search_data);

  *first = search_data.iter;

  return search_data.set;
}

static gboolean
gtk_combo_box_scroll_event (GtkWidget          *widget,
                            GdkEventScroll     *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  gboolean found;
  GtkTreeIter iter;
  GtkTreeIter new_iter;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    return TRUE;

  if (event->direction == GDK_SCROLL_UP)
    found = tree_prev (combo_box, combo_box->priv->model,
                       &iter, &new_iter, FALSE);
  else
    found = tree_next (combo_box, combo_box->priv->model,
                       &iter, &new_iter, FALSE);

  if (found)
    gtk_combo_box_set_active_iter (combo_box, &new_iter);

  return TRUE;
}

/*
 * menu style
 */
static gboolean
gtk_combo_box_row_separator_func (GtkTreeModel      *model,
                                  GtkTreeIter       *iter,
                                  GtkComboBox       *combo)
{
  GtkComboBoxPrivate *priv = combo->priv;

  if (priv->row_separator_func)
    return priv->row_separator_func (model, iter, priv->row_separator_data);

  return FALSE;
}

static gboolean
gtk_combo_box_header_func (GtkTreeModel      *model,
                           GtkTreeIter       *iter,
                           GtkComboBox       *combo)
{
  /* Every submenu has a selectable header, however we
   * can expose a method to make that configurable by
   * the user (like row_separator_func is done) */
  return TRUE;
}

static void
gtk_combo_box_menu_setup (GtkComboBox *combo_box,
                          gboolean     add_children)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkWidget *child;
  GtkWidget *menu;

  child = gtk_bin_get_child (GTK_BIN (combo_box));

  if (priv->cell_view)
    {
      priv->button = gtk_toggle_button_new ();
      gtk_button_set_focus_on_click (GTK_BUTTON (priv->button),
                                     priv->focus_on_click);

      g_signal_connect (priv->button, "toggled",
                        G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
      gtk_widget_set_parent (priv->button,
                             gtk_widget_get_parent (child));

      priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (priv->button), priv->box);

      priv->separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
      gtk_container_add (GTK_CONTAINER (priv->box), priv->separator);

      priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (priv->box), priv->arrow);
      gtk_widget_add_events (priv->button, GDK_SCROLL_MASK);

      gtk_widget_show_all (priv->button);
    }
  else
    {
      priv->button = gtk_toggle_button_new ();
      gtk_button_set_focus_on_click (GTK_BUTTON (priv->button),
                                     priv->focus_on_click);

      g_signal_connect (priv->button, "toggled",
                        G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
      gtk_widget_set_parent (priv->button,
                             gtk_widget_get_parent (child));

      priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (priv->button), priv->arrow);
      gtk_widget_add_events (priv->button, GDK_SCROLL_MASK);
      gtk_widget_show_all (priv->button);
    }

  g_signal_connect (priv->button, "button-press-event",
                    G_CALLBACK (gtk_combo_box_menu_button_press),
                    combo_box);
  g_signal_connect (priv->button, "state-flags-changed",
                    G_CALLBACK (gtk_combo_box_button_state_flags_changed),
                    combo_box);

  /* create our funky menu */
  menu = _gtk_tree_menu_new_with_area (priv->area);
  gtk_widget_set_name (menu, "gtk-combobox-popup-menu");

  _gtk_tree_menu_set_model (GTK_TREE_MENU (menu), priv->model);

  _gtk_tree_menu_set_wrap_width (GTK_TREE_MENU (menu), priv->wrap_width);
  _gtk_tree_menu_set_row_span_column (GTK_TREE_MENU (menu), priv->row_column);
  _gtk_tree_menu_set_column_span_column (GTK_TREE_MENU (menu), priv->col_column);
  _gtk_tree_menu_set_tearoff (GTK_TREE_MENU (menu),
                              combo_box->priv->add_tearoffs);

  g_signal_connect (menu, "menu-activate",
                    G_CALLBACK (gtk_combo_box_menu_activate), combo_box);

  /* Chain our row_separator_func through */
  _gtk_tree_menu_set_row_separator_func (GTK_TREE_MENU (menu),
                                         (GtkTreeViewRowSeparatorFunc)gtk_combo_box_row_separator_func,
                                         combo_box, NULL);

  _gtk_tree_menu_set_header_func (GTK_TREE_MENU (menu),
                                  (GtkTreeMenuHeaderFunc)gtk_combo_box_header_func,
                                  combo_box, NULL);

  g_signal_connect (menu, "key-press-event",
                    G_CALLBACK (gtk_combo_box_menu_key_press), combo_box);
  gtk_combo_box_set_popup_widget (combo_box, menu);

  gtk_combo_box_update_title (combo_box);
}

static void
gtk_combo_box_menu_destroy (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  g_signal_handlers_disconnect_matched (priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_menu_button_press, NULL);
  g_signal_handlers_disconnect_matched (priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_button_state_flags_changed, combo_box);
  g_signal_handlers_disconnect_matched (priv->popup_widget,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_menu_activate, combo_box);

  /* unparent will remove our latest ref */
  gtk_widget_unparent (priv->button);

  priv->box = NULL;
  priv->button = NULL;
  priv->arrow = NULL;
  priv->separator = NULL;

  /* changing the popup window will unref the menu and the children */
}

/* callbacks */
static gboolean
gtk_combo_box_menu_button_press (GtkWidget      *widget,
                                 GdkEventButton *event,
                                 gpointer        user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (GTK_IS_MENU (priv->popup_widget) &&
      event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY)
    {
      if (priv->focus_on_click &&
          !gtk_widget_has_focus (priv->button))
        gtk_widget_grab_focus (priv->button);

      gtk_combo_box_menu_popup (combo_box, event->button, event->time);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_menu_activate (GtkWidget   *menu,
                             const gchar *path,
                             GtkComboBox *combo_box)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_from_string (combo_box->priv->model, &iter, path))
    gtk_combo_box_set_active_iter (combo_box, &iter);

  g_object_set (combo_box,
                "editing-canceled", FALSE,
                NULL);
}


static void
gtk_combo_box_update_sensitivity (GtkComboBox *combo_box)
{
  GtkTreeIter iter;
  gboolean sensitive = TRUE; /* fool code checkers */

  if (!combo_box->priv->button)
    return;

  switch (combo_box->priv->button_sensitivity)
    {
      case GTK_SENSITIVITY_ON:
        sensitive = TRUE;
        break;
      case GTK_SENSITIVITY_OFF:
        sensitive = FALSE;
        break;
      case GTK_SENSITIVITY_AUTO:
        sensitive = combo_box->priv->model &&
                    gtk_tree_model_get_iter_first (combo_box->priv->model, &iter);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  gtk_widget_set_sensitive (combo_box->priv->button, sensitive);

  /* In list-mode, we also need to update sensitivity of the event box */
  if (GTK_IS_TREE_VIEW (combo_box->priv->tree_view)
      && combo_box->priv->cell_view)
    gtk_widget_set_sensitive (combo_box->priv->box, sensitive);
}

static void
gtk_combo_box_model_row_inserted (GtkTreeModel     *model,
                                  GtkTreePath      *path,
                                  GtkTreeIter      *iter,
                                  gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (combo_box->priv->tree_view)
    gtk_combo_box_list_popup_resize (combo_box);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_model_row_deleted (GtkTreeModel     *model,
                                 GtkTreePath      *path,
                                 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (!gtk_tree_row_reference_valid (priv->active_row))
    {
      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view), NULL);
      g_signal_emit (combo_box, combo_box_signals[CHANGED], 0);
    }
  
  if (priv->tree_view)
    gtk_combo_box_list_popup_resize (combo_box);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_model_rows_reordered (GtkTreeModel    *model,
                                    GtkTreePath     *path,
                                    GtkTreeIter     *iter,
                                    gint            *new_order,
                                    gpointer         user_data)
{
  gtk_tree_row_reference_reordered (G_OBJECT (user_data), path, iter, new_order);
}

static void
gtk_combo_box_model_row_changed (GtkTreeModel     *model,
                                 GtkTreePath      *path,
                                 GtkTreeIter      *iter,
                                 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *active_path;

  /* FIXME this belongs to GtkCellView */
  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      active_path = gtk_tree_row_reference_get_path (priv->active_row);
      if (gtk_tree_path_compare (path, active_path) == 0 &&
          priv->cell_view)
        gtk_widget_queue_resize (GTK_WIDGET (priv->cell_view));
      gtk_tree_path_free (active_path);
    }

  if (priv->tree_view)
    gtk_combo_box_list_row_changed (model, path, iter, user_data);
}

static gboolean
list_popup_resize_idle (gpointer user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = combo_box->priv;
  gint x, y, width, height;

  if (priv->tree_view && gtk_widget_get_mapped (priv->popup_window))
    {
      gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);

      gtk_widget_set_size_request (priv->popup_window, width, height);
      gtk_window_move (GTK_WINDOW (priv->popup_window), x, y);
    }

  priv->resize_idle_id = 0;

  return FALSE;
}

static void
gtk_combo_box_list_popup_resize (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  if (!priv->resize_idle_id)
    priv->resize_idle_id =
      gdk_threads_add_idle (list_popup_resize_idle, combo_box);
}

static void
gtk_combo_box_model_row_expanded (GtkTreeModel     *model,
                                  GtkTreePath      *path,
                                  GtkTreeIter      *iter,
                                  gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_combo_box_list_popup_resize (combo_box);
}


/*
 * list style
 */

static void
gtk_combo_box_list_setup (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeSelection *sel;
  GtkWidget *child;
  GtkWidget *widget = GTK_WIDGET (combo_box);

  priv->button = gtk_toggle_button_new ();
  child = gtk_bin_get_child (GTK_BIN (combo_box));
  gtk_widget_set_parent (priv->button,
                         gtk_widget_get_parent (child));
  g_signal_connect (priv->button, "button-press-event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed), combo_box);
  g_signal_connect (priv->button, "toggled",
                    G_CALLBACK (gtk_combo_box_button_toggled), combo_box);

  priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (priv->button), priv->arrow);
  priv->separator = NULL;
  gtk_widget_show_all (priv->button);

  if (priv->cell_view)
    {
      GtkStyleContext *context;
      GtkStateFlags state;
      GdkRGBA color;

      context  = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_background_color (context, state, &color);

      gtk_cell_view_set_background_rgba (GTK_CELL_VIEW (priv->cell_view), &color);

      priv->box = gtk_event_box_new ();
      gtk_event_box_set_visible_window (GTK_EVENT_BOX (priv->box),
                                        FALSE);

      if (priv->has_frame)
        {
          priv->cell_view_frame = gtk_frame_new (NULL);
          gtk_frame_set_shadow_type (GTK_FRAME (priv->cell_view_frame),
                                     GTK_SHADOW_IN);
        }
      else
        {
          combo_box->priv->cell_view_frame = gtk_event_box_new ();
          gtk_event_box_set_visible_window (GTK_EVENT_BOX (combo_box->priv->cell_view_frame),
                                            FALSE);
        }

      gtk_widget_set_parent (priv->cell_view_frame,
                             gtk_widget_get_parent (child));
      gtk_container_add (GTK_CONTAINER (priv->cell_view_frame), priv->box);
      gtk_widget_show_all (priv->cell_view_frame);

      g_signal_connect (priv->box, "button-press-event",
                        G_CALLBACK (gtk_combo_box_list_button_pressed),
                        combo_box);
    }

  priv->tree_view = gtk_tree_view_new ();
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
  gtk_tree_selection_set_select_function (sel,
                                          gtk_combo_box_list_select_func,
                                          NULL, NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view),
                                     FALSE);
  gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (priv->tree_view),
                                     TRUE);

  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->tree_view),
                                        (GtkTreeViewRowSeparatorFunc)gtk_combo_box_row_separator_func,
                                        combo_box, NULL);

  if (priv->model)
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), priv->model);

  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view),
                               gtk_tree_view_column_new_with_area (priv->area));

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (priv->active_row);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view),
                                path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  /* set sample/popup widgets */
  gtk_combo_box_set_popup_widget (combo_box, priv->tree_view);

  g_signal_connect (priv->tree_view, "key-press-event",
                    G_CALLBACK (gtk_combo_box_list_key_press),
                    combo_box);
  g_signal_connect (priv->tree_view, "enter-notify-event",
                    G_CALLBACK (gtk_combo_box_list_enter_notify),
                    combo_box);
  g_signal_connect (priv->tree_view, "row-expanded",
                    G_CALLBACK (gtk_combo_box_model_row_expanded),
                    combo_box);
  g_signal_connect (priv->tree_view, "row-collapsed",
                    G_CALLBACK (gtk_combo_box_model_row_expanded),
                    combo_box);
  g_signal_connect (priv->popup_window, "button-press-event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed),
                    combo_box);
  g_signal_connect (priv->popup_window, "button-release-event",
                    G_CALLBACK (gtk_combo_box_list_button_released),
                    combo_box);

  gtk_widget_show (priv->tree_view);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_list_destroy (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;

  /* disconnect signals */
  g_signal_handlers_disconnect_matched (priv->tree_view,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL, combo_box);
  g_signal_handlers_disconnect_matched (priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_list_button_pressed,
                                        NULL);
  g_signal_handlers_disconnect_matched (priv->popup_window,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_list_button_pressed,
                                        NULL);
  g_signal_handlers_disconnect_matched (priv->popup_window,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_list_button_released,
                                        NULL);

  g_signal_handlers_disconnect_matched (priv->popup_window,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_child_show,
                                        NULL);

  g_signal_handlers_disconnect_matched (priv->popup_window,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_child_hide,
                                        NULL);

  if (priv->box)
    g_signal_handlers_disconnect_matched (priv->box,
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL,
                                          gtk_combo_box_list_button_pressed,
                                          NULL);

  /* destroy things (unparent will kill the latest ref from us)
   * last unref on button will destroy the arrow
   */
  gtk_widget_unparent (priv->button);
  priv->button = NULL;
  priv->arrow = NULL;

  if (priv->cell_view)
    {
      g_object_set (priv->cell_view,
                    "background-set", FALSE,
                    NULL);
    }

  if (priv->cell_view_frame)
    {
      gtk_widget_unparent (priv->cell_view_frame);
      priv->cell_view_frame = NULL;
      priv->box = NULL;
    }

  if (priv->scroll_timer)
    {
      g_source_remove (priv->scroll_timer);
      priv->scroll_timer = 0;
    }

  if (priv->resize_idle_id)
    {
      g_source_remove (priv->resize_idle_id);
      priv->resize_idle_id = 0;
    }

  gtk_widget_destroy (priv->tree_view);

  priv->tree_view = NULL;
  if (priv->popup_widget)
    {
      g_object_unref (priv->popup_widget);
      priv->popup_widget = NULL;
    }
}

/* callbacks */

static gboolean
gtk_combo_box_list_button_pressed (GtkWidget      *widget,
                                   GdkEventButton *event,
                                   gpointer        data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (ewidget == priv->popup_window)
    return TRUE;

  if ((ewidget != priv->button && ewidget != priv->box) ||
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)))
    return FALSE;

  if (priv->focus_on_click &&
      !gtk_widget_has_focus (priv->button))
    gtk_widget_grab_focus (priv->button);

  gtk_combo_box_popup_for_device (combo_box, event->device);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button), TRUE);

  priv->auto_scroll = FALSE;
  if (priv->scroll_timer == 0)
    priv->scroll_timer = gdk_threads_add_timeout (SCROLL_TIME,
                                                  (GSourceFunc) gtk_combo_box_list_scroll_timeout,
                                                   combo_box);

  priv->popup_in_progress = TRUE;

  return TRUE;
}

static gboolean
gtk_combo_box_list_button_released (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    gpointer        data)
{
  gboolean ret;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;

  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = combo_box->priv;

  gboolean popup_in_progress = FALSE;

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (priv->popup_in_progress)
    {
      popup_in_progress = TRUE;
      priv->popup_in_progress = FALSE;
    }

  gtk_tree_view_set_hover_expand (GTK_TREE_VIEW (priv->tree_view),
                                  FALSE);
  if (priv->scroll_timer)
    {
      g_source_remove (priv->scroll_timer);
      priv->scroll_timer = 0;
    }

  if (ewidget != priv->tree_view)
    {
      if ((ewidget == priv->button ||
           ewidget == priv->box) &&
          !popup_in_progress &&
          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)))
        {
          gtk_combo_box_popdown (combo_box);
          return TRUE;
        }

      /* released outside treeview */
      if (ewidget != priv->button && ewidget != priv->box)
        {
          gtk_combo_box_popdown (combo_box);

          return TRUE;
        }

      return FALSE;
    }

  /* select something cool */
  ret = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree_view),
                                       event->x, event->y,
                                       &path,
                                       NULL, NULL, NULL);

  if (!ret)
    return TRUE; /* clicked outside window? */

  gtk_tree_model_get_iter (priv->model, &iter, path);
  gtk_tree_path_free (path);

  gtk_combo_box_popdown (combo_box);

  if (tree_column_row_is_sensitive (combo_box, &iter))
    gtk_combo_box_set_active_iter (combo_box, &iter);

  return TRUE;
}

static gboolean
gtk_combo_box_menu_key_press (GtkWidget   *widget,
                              GdkEventKey *event,
                              gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (!gtk_bindings_activate_event (G_OBJECT (widget), event))
    {
      /* The menu hasn't managed the
       * event, forward it to the combobox
       */
      gtk_bindings_activate_event (G_OBJECT (combo_box), event);
    }

  return TRUE;
}

static gboolean
gtk_combo_box_list_key_press (GtkWidget   *widget,
                              GdkEventKey *event,
                              gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkTreeIter iter;

  if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_ISO_Enter || event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_space || event->keyval == GDK_KEY_KP_Space)
  {
    GtkTreeModel *model = NULL;

    gtk_combo_box_popdown (combo_box);

    if (combo_box->priv->model)
      {
        GtkTreeSelection *sel;

        sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view));

        if (gtk_tree_selection_get_selected (sel, &model, &iter))
          gtk_combo_box_set_active_iter (combo_box, &iter);
      }

    return TRUE;
  }

  if (!gtk_bindings_activate_event (G_OBJECT (widget), event))
    {
      /* The list hasn't managed the
       * event, forward it to the combobox
       */
      gtk_bindings_activate_event (G_OBJECT (combo_box), event);
    }

  return TRUE;
}

static void
gtk_combo_box_list_auto_scroll (GtkComboBox *combo_box,
                                gint         x,
                                gint         y)
{
  GtkAdjustment *adj;
  GtkAllocation allocation;
  GtkWidget *tree_view = combo_box->priv->tree_view;
  gdouble value;

  gtk_widget_get_allocation (tree_view, &allocation);

  adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (combo_box->priv->scrolled_window));
  if (adj && gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj) > gtk_adjustment_get_page_size (adj))
    {
      if (x <= allocation.x &&
          gtk_adjustment_get_lower (adj) < gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) - (allocation.x - x + 1);
          gtk_adjustment_set_value (adj, value);
        }
      else if (x >= allocation.x + allocation.width &&
               gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj) > gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) + (x - allocation.x - allocation.width + 1);
          gtk_adjustment_set_value (adj, MAX (value, 0.0));
        }
    }

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (combo_box->priv->scrolled_window));
  if (adj && gtk_adjustment_get_upper (adj) - gtk_adjustment_get_lower (adj) > gtk_adjustment_get_page_size (adj))
    {
      if (y <= allocation.y &&
          gtk_adjustment_get_lower (adj) < gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) - (allocation.y - y + 1);
          gtk_adjustment_set_value (adj, value);
        }
      else if (y >= allocation.height &&
               gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj) > gtk_adjustment_get_value (adj))
        {
          value = gtk_adjustment_get_value (adj) + (y - allocation.height + 1);
          gtk_adjustment_set_value (adj, MAX (value, 0.0));
        }
    }
}

static gboolean
gtk_combo_box_list_scroll_timeout (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  gint x, y;

  if (priv->auto_scroll)
    {
      gdk_window_get_device_position (gtk_widget_get_window (priv->tree_view),
                                      priv->grab_pointer,
                                      &x, &y, NULL);
      gtk_combo_box_list_auto_scroll (combo_box, x, y);
    }

  return TRUE;
}

static gboolean
gtk_combo_box_list_enter_notify (GtkWidget        *widget,
                                 GdkEventCrossing *event,
                                 gpointer          data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  combo_box->priv->auto_scroll = TRUE;

  return TRUE;
}

static gboolean
gtk_combo_box_list_select_func (GtkTreeSelection *selection,
                                GtkTreeModel     *model,
                                GtkTreePath      *path,
                                gboolean          path_currently_selected,
                                gpointer          data)
{
  GList *list, *columns;
  gboolean sensitive = FALSE;

  columns = gtk_tree_view_get_columns (gtk_tree_selection_get_tree_view (selection));

  for (list = columns; list && !sensitive; list = list->next)
    {
      GList *cells, *cell;
      gboolean cell_sensitive, cell_visible;
      GtkTreeIter iter;
      GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (list->data);

      if (!gtk_tree_view_column_get_visible (column))
        continue;

      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_view_column_cell_set_cell_data (column, model, &iter,
                                               FALSE, FALSE);

      cell = cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
      while (cell)
        {
          g_object_get (cell->data,
                        "sensitive", &cell_sensitive,
                        "visible", &cell_visible,
                        NULL);

          if (cell_visible && cell_sensitive)
            {
              sensitive = TRUE;
              break;
            }

          cell = cell->next;
        }

      g_list_free (cells);
    }

  g_list_free (columns);

  return sensitive;
}

static void
gtk_combo_box_list_row_changed (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      data)
{
  /* XXX Do nothing ? */
}

/*
 * GtkCellLayout implementation
 */
static GtkCellArea *
gtk_combo_box_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkComboBox *combo = GTK_COMBO_BOX (cell_layout);
  GtkComboBoxPrivate *priv = combo->priv;

  if (G_UNLIKELY (!priv->area))
    {
      priv->area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->area);
    }

  return priv->area;
}

/*
 * public API
 */

/**
 * gtk_combo_box_new:
 *
 * Creates a new empty #GtkComboBox.
 *
 * Return value: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, NULL);
}

/**
 * gtk_combo_box_new_with_area:
 * @area: the #GtkCellArea to use to layout cell renderers
 *
 * Creates a new empty #GtkComboBox using @area to layout cells.
 *
 * Return value: A new #GtkComboBox.
 */
GtkWidget *
gtk_combo_box_new_with_area (GtkCellArea  *area)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, "cell-area", area, NULL);
}

/**
 * gtk_combo_box_new_with_area_and_entry:
 * @area: the #GtkCellArea to use to layout cell renderers
 *
 * Creates a new empty #GtkComboBox with an entry.
 *
 * The new combo box will use @area to layout cells.
 *
 * Return value: A new #GtkComboBox.
 */
GtkWidget *
gtk_combo_box_new_with_area_and_entry (GtkCellArea *area)
{
  return g_object_new (GTK_TYPE_COMBO_BOX,
                       "has-entry", TRUE,
                       "cell-area", area,
                       NULL);
}


/**
 * gtk_combo_box_new_with_entry:
 *
 * Creates a new empty #GtkComboBox with an entry.
 *
 * Return value: A new #GtkComboBox.
 */
GtkWidget *
gtk_combo_box_new_with_entry (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, "has-entry", TRUE, NULL);
}

/**
 * gtk_combo_box_new_with_model:
 * @model: A #GtkTreeModel.
 *
 * Creates a new #GtkComboBox with the model initialized to @model.
 *
 * Return value: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new_with_model (GtkTreeModel *model)
{
  GtkComboBox *combo_box;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

  combo_box = g_object_new (GTK_TYPE_COMBO_BOX, "model", model, NULL);

  return GTK_WIDGET (combo_box);
}

/**
 * gtk_combo_box_new_with_model_and_entry:
 * @model: A #GtkTreeModel
 *
 * Creates a new empty #GtkComboBox with an entry
 * and with the model initialized to @model.
 *
 * Return value: A new #GtkComboBox
 */
GtkWidget *
gtk_combo_box_new_with_model_and_entry (GtkTreeModel *model)
{
  return g_object_new (GTK_TYPE_COMBO_BOX,
                       "has-entry", TRUE,
                       "model", model,
                       NULL);
}

/**
 * gtk_combo_box_get_wrap_width:
 * @combo_box: A #GtkComboBox
 *
 * Returns the wrap width which is used to determine the number of columns
 * for the popup menu. If the wrap width is larger than 1, the combo box
 * is in table mode.
 *
 * Returns: the wrap width.
 *
 * Since: 2.6
 */
gint
gtk_combo_box_get_wrap_width (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

  return combo_box->priv->wrap_width;
}

/**
 * gtk_combo_box_set_wrap_width:
 * @combo_box: A #GtkComboBox
 * @width: Preferred number of columns
 *
 * Sets the wrap width of @combo_box to be @width. The wrap width is basically
 * the preferred number of columns when you want the popup to be layed out
 * in a table.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_wrap_width (GtkComboBox *combo_box,
                              gint         width)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (width >= 0);

  priv = combo_box->priv;

  if (width != priv->wrap_width)
    {
      priv->wrap_width = width;

      gtk_combo_box_check_appearance (combo_box);

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_wrap_width (GTK_TREE_MENU (priv->popup_widget), priv->wrap_width);

      g_object_notify (G_OBJECT (combo_box), "wrap-width");
    }
}

/**
 * gtk_combo_box_get_row_span_column:
 * @combo_box: A #GtkComboBox
 *
 * Returns the column with row span information for @combo_box.
 *
 * Returns: the row span column.
 *
 * Since: 2.6
 */
gint
gtk_combo_box_get_row_span_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

  return combo_box->priv->row_column;
}

/**
 * gtk_combo_box_set_row_span_column:
 * @combo_box: A #GtkComboBox.
 * @row_span: A column in the model passed during construction.
 *
 * Sets the column with row span information for @combo_box to be @row_span.
 * The row span column contains integers which indicate how many rows
 * an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_row_span_column (GtkComboBox *combo_box,
                                   gint         row_span)
{
  GtkComboBoxPrivate *priv;
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  col = gtk_tree_model_get_n_columns (priv->model);
  g_return_if_fail (row_span >= -1 && row_span < col);

  if (row_span != priv->row_column)
    {
      priv->row_column = row_span;

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_row_span_column (GTK_TREE_MENU (priv->popup_widget), priv->row_column);

      g_object_notify (G_OBJECT (combo_box), "row-span-column");
    }
}

/**
 * gtk_combo_box_get_column_span_column:
 * @combo_box: A #GtkComboBox
 *
 * Returns the column with column span information for @combo_box.
 *
 * Returns: the column span column.
 *
 * Since: 2.6
 */
gint
gtk_combo_box_get_column_span_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

  return combo_box->priv->col_column;
}

/**
 * gtk_combo_box_set_column_span_column:
 * @combo_box: A #GtkComboBox
 * @column_span: A column in the model passed during construction
 *
 * Sets the column with column span information for @combo_box to be
 * @column_span. The column span column contains integers which indicate
 * how many columns an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_column_span_column (GtkComboBox *combo_box,
                                      gint         column_span)
{
  GtkComboBoxPrivate *priv;
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  col = gtk_tree_model_get_n_columns (priv->model);
  g_return_if_fail (column_span >= -1 && column_span < col);

  if (column_span != priv->col_column)
    {
      priv->col_column = column_span;

      if (GTK_IS_TREE_MENU (priv->popup_widget))
        _gtk_tree_menu_set_column_span_column (GTK_TREE_MENU (priv->popup_widget), priv->col_column);

      g_object_notify (G_OBJECT (combo_box), "column-span-column");
    }
}

/**
 * gtk_combo_box_get_active:
 * @combo_box: A #GtkComboBox
 *
 * Returns the index of the currently active item, or -1 if there's no
 * active item. If the model is a non-flat treemodel, and the active item
 * is not an immediate child of the root of the tree, this function returns
 * <literal>gtk_tree_path_get_indices (path)[0]</literal>, where
 * <literal>path</literal> is the #GtkTreePath of the active item.
 *
 * Return value: An integer which is the index of the currently active item,
 *     or -1 if there's no active item.
 *
 * Since: 2.4
 */
gint
gtk_combo_box_get_active (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv;
  gint result;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  priv = combo_box->priv;

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (priv->active_row);
      result = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }
  else
    result = -1;

  return result;
}

/**
 * gtk_combo_box_set_active:
 * @combo_box: A #GtkComboBox
 * @index_: An index in the model passed during construction, or -1 to have
 * no active item
 *
 * Sets the active item of @combo_box to be the item at @index.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_active (GtkComboBox *combo_box,
                          gint         index_)
{
  GtkTreePath *path = NULL;
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (index_ >= -1);

  if (combo_box->priv->model == NULL)
    {
      /* Save index, in case the model is set after the index */
      combo_box->priv->active = index_;
      if (index_ != -1)
        return;
    }

  if (index_ != -1)
    path = gtk_tree_path_new_from_indices (index_, -1);

  gtk_combo_box_set_active_internal (combo_box, path);

  if (path)
    gtk_tree_path_free (path);
}

static void
gtk_combo_box_set_active_internal (GtkComboBox *combo_box,
                                   GtkTreePath *path)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreePath *active_path;
  gint path_cmp;

  /* Remember whether the initially active row is valid. */
  gboolean is_valid_row_reference = gtk_tree_row_reference_valid (priv->active_row);

  if (path && is_valid_row_reference)
    {
      active_path = gtk_tree_row_reference_get_path (priv->active_row);
      path_cmp = gtk_tree_path_compare (path, active_path);
      gtk_tree_path_free (active_path);
      if (path_cmp == 0)
        return;
    }

  if (priv->active_row)
    {
      gtk_tree_row_reference_free (priv->active_row);
      priv->active_row = NULL;
    }

  if (!path)
    {
      if (priv->tree_view)
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view)));
      else
        {
          GtkMenu *menu = GTK_MENU (priv->popup_widget);

          if (GTK_IS_MENU (menu))
            gtk_menu_set_active (menu, -1);
        }

      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view), NULL);

      /*
       *  Do not emit a "changed" signal when an already invalid selection was
       *  now set to invalid.
       */
      if (!is_valid_row_reference)
        return;
    }
  else
    {
      priv->active_row =
        gtk_tree_row_reference_new (priv->model, path);

      if (priv->tree_view)
        {
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view),
                                    path, NULL, FALSE);
        }
      else if (GTK_IS_MENU (priv->popup_widget))
        {
          /* FIXME handle nested menus better */
          gtk_menu_set_active (GTK_MENU (priv->popup_widget),
                               gtk_tree_path_get_indices (path)[0]);
        }

      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view),
                                         path);
    }

  g_signal_emit (combo_box, combo_box_signals[CHANGED], 0);
  g_object_notify (G_OBJECT (combo_box), "active");
  if (combo_box->priv->id_column >= 0)
    g_object_notify (G_OBJECT (combo_box), "active-id");
}


/**
 * gtk_combo_box_get_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: (out): The uninitialized #GtkTreeIter
 *
 * Sets @iter to point to the current active item, if it exists.
 *
 * Return value: %TRUE, if @iter was set
 *
 * Since: 2.4
 */
gboolean
gtk_combo_box_get_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  if (!gtk_tree_row_reference_valid (combo_box->priv->active_row))
    return FALSE;

  path = gtk_tree_row_reference_get_path (combo_box->priv->active_row);
  result = gtk_tree_model_get_iter (combo_box->priv->model, iter, path);
  gtk_tree_path_free (path);

  return result;
}

/**
 * gtk_combo_box_set_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: (allow-none): The #GtkTreeIter, or %NULL
 *
 * Sets the current active item to be the one referenced by @iter, or
 * unsets the active item if @iter is %NULL.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path = NULL;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (iter)
    path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo_box), iter);

  gtk_combo_box_set_active_internal (combo_box, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_combo_box_set_model:
 * @combo_box: A #GtkComboBox
 * @model: (allow-none): A #GtkTreeModel
 *
 * Sets the model used by @combo_box to be @model. Will unset a previously set
 * model (if applicable). If model is %NULL, then it will unset the model.
 *
 * Note that this function does not clear the cell renderers, you have to
 * call gtk_cell_layout_clear() yourself if you need to set up different
 * cell renderers for the new model.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_model (GtkComboBox  *combo_box,
                         GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (model == combo_box->priv->model)
    return;

  gtk_combo_box_unset_model (combo_box);

  if (model == NULL)
    goto out;

  combo_box->priv->model = model;
  g_object_ref (combo_box->priv->model);

  combo_box->priv->inserted_id =
    g_signal_connect (combo_box->priv->model, "row-inserted",
                      G_CALLBACK (gtk_combo_box_model_row_inserted),
                      combo_box);
  combo_box->priv->deleted_id =
    g_signal_connect (combo_box->priv->model, "row-deleted",
                      G_CALLBACK (gtk_combo_box_model_row_deleted),
                      combo_box);
  combo_box->priv->reordered_id =
    g_signal_connect (combo_box->priv->model, "rows-reordered",
                      G_CALLBACK (gtk_combo_box_model_rows_reordered),
                      combo_box);
  combo_box->priv->changed_id =
    g_signal_connect (combo_box->priv->model, "row-changed",
                      G_CALLBACK (gtk_combo_box_model_row_changed),
                      combo_box);

  if (combo_box->priv->tree_view)
    {
      /* list mode */
      gtk_tree_view_set_model (GTK_TREE_VIEW (combo_box->priv->tree_view),
                               combo_box->priv->model);
      gtk_combo_box_list_popup_resize (combo_box);
    }

  if (GTK_IS_TREE_MENU (combo_box->priv->popup_widget))
    {
      /* menu mode */
      _gtk_tree_menu_set_model (GTK_TREE_MENU (combo_box->priv->popup_widget),
                                combo_box->priv->model);
    }

  if (combo_box->priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (combo_box->priv->cell_view),
                             combo_box->priv->model);

  if (combo_box->priv->active != -1)
    {
      /* If an index was set in advance, apply it now */
      gtk_combo_box_set_active (combo_box, combo_box->priv->active);
      combo_box->priv->active = -1;
    }

out:
  gtk_combo_box_update_sensitivity (combo_box);

  g_object_notify (G_OBJECT (combo_box), "model");
}

/**
 * gtk_combo_box_get_model:
 * @combo_box: A #GtkComboBox
 *
 * Returns the #GtkTreeModel which is acting as data source for @combo_box.
 *
 * Return value: (transfer none): A #GtkTreeModel which was passed
 *     during construction.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_combo_box_get_model (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->model;
}

static void
gtk_combo_box_real_move_active (GtkComboBox   *combo_box,
                                GtkScrollType  scroll)
{
  GtkTreeIter iter;
  GtkTreeIter new_iter;
  gboolean    active_iter;
  gboolean    found;

  if (!combo_box->priv->model)
    {
      gtk_widget_error_bell (GTK_WIDGET (combo_box));
      return;
    }

  active_iter = gtk_combo_box_get_active_iter (combo_box, &iter);

  switch (scroll)
    {
    case GTK_SCROLL_STEP_BACKWARD:
    case GTK_SCROLL_STEP_UP:
    case GTK_SCROLL_STEP_LEFT:
      if (active_iter)
        {
          found = tree_prev (combo_box, combo_box->priv->model,
                             &iter, &new_iter, FALSE);
          break;
        }
      /* else fall through */

    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_PAGE_DOWN:
    case GTK_SCROLL_PAGE_RIGHT:
    case GTK_SCROLL_END:
      found = tree_last (combo_box, combo_box->priv->model, &new_iter, FALSE);
      break;

    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_STEP_DOWN:
    case GTK_SCROLL_STEP_RIGHT:
      if (active_iter)
        {
          found = tree_next (combo_box, combo_box->priv->model,
                             &iter, &new_iter, FALSE);
          break;
        }
      /* else fall through */

    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_LEFT:
    case GTK_SCROLL_START:
      found = tree_first (combo_box, combo_box->priv->model, &new_iter, FALSE);
      break;

    default:
      return;
    }

  if (found && active_iter)
    {
      GtkTreePath *old_path;
      GtkTreePath *new_path;

      old_path = gtk_tree_model_get_path (combo_box->priv->model, &iter);
      new_path = gtk_tree_model_get_path (combo_box->priv->model, &new_iter);

      if (gtk_tree_path_compare (old_path, new_path) == 0)
        found = FALSE;

      gtk_tree_path_free (old_path);
      gtk_tree_path_free (new_path);
    }

  if (found)
    {
      gtk_combo_box_set_active_iter (combo_box, &new_iter);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (combo_box));
    }
}

static gboolean
gtk_combo_box_mnemonic_activate (GtkWidget *widget,
                                 gboolean   group_cycling)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (combo_box->priv->has_entry)
    {
      GtkWidget* child;

      child = gtk_bin_get_child (GTK_BIN (combo_box));
      if (child)
        gtk_widget_grab_focus (child);
    }
  else
    gtk_widget_grab_focus (combo_box->priv->button);

  return TRUE;
}

static void
gtk_combo_box_grab_focus (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (combo_box->priv->has_entry)
    {
      GtkWidget *child;

      child = gtk_bin_get_child (GTK_BIN (combo_box));
      if (child)
        gtk_widget_grab_focus (child);
    }
  else
    gtk_widget_grab_focus (combo_box->priv->button);
}

static void
gtk_combo_box_destroy (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (combo_box->priv->popup_idle_id > 0)
    {
      g_source_remove (combo_box->priv->popup_idle_id);
      combo_box->priv->popup_idle_id = 0;
    }

  gtk_combo_box_popdown (combo_box);

  if (combo_box->priv->row_separator_destroy)
    combo_box->priv->row_separator_destroy (combo_box->priv->row_separator_data);

  combo_box->priv->row_separator_func = NULL;
  combo_box->priv->row_separator_data = NULL;
  combo_box->priv->row_separator_destroy = NULL;

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->destroy (widget);
  combo_box->priv->cell_view = NULL;
}

static void
gtk_combo_box_entry_contents_changed (GtkEntry *entry,
                                      gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  /*
   *  Fixes regression reported in bug #574059. The old functionality relied on
   *  bug #572478.  As a bugfix, we now emit the "changed" signal ourselves
   *  when the selection was already set to -1.
   */
  if (gtk_combo_box_get_active(combo_box) == -1)
    g_signal_emit_by_name (combo_box, "changed");
  else
    gtk_combo_box_set_active (combo_box, -1);
}

static void
gtk_combo_box_entry_active_changed (GtkComboBox *combo_box,
                                    gpointer     user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      GtkEntry *entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box)));

      if (entry)
        {
	  GtkTreePath *path;
	  gchar       *path_str;
	  gchar       *text = NULL;

          model    = gtk_combo_box_get_model (combo_box);
	  path     = gtk_tree_model_get_path (model, &iter);
	  path_str = gtk_tree_path_to_string (path);

          g_signal_handlers_block_by_func (entry,
                                           gtk_combo_box_entry_contents_changed,
                                           combo_box);


	  g_signal_emit (combo_box, combo_box_signals[FORMAT_ENTRY_TEXT], 0, 
			 path_str, &text);

	  gtk_entry_set_text (entry, text);

          g_signal_handlers_unblock_by_func (entry,
                                             gtk_combo_box_entry_contents_changed,
                                             combo_box);

	  gtk_tree_path_free (path);
	  g_free (text);
	  g_free (path_str);
        }
    }
}

static gchar *
gtk_combo_box_format_entry_text (GtkComboBox     *combo_box,
				 const gchar     *path)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  gchar              *text = NULL;

  if (priv->text_column >= 0)
    {
      model = gtk_combo_box_get_model (combo_box);
      gtk_tree_model_get_iter_from_string (model, &iter, path);

      gtk_tree_model_get (model, &iter,
			  priv->text_column, &text,
			  -1);
    }

  return text;
}


static GObject *
gtk_combo_box_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
  GObject            *object;
  GtkComboBox        *combo_box;
  GtkComboBoxPrivate *priv;

  object = G_OBJECT_CLASS (gtk_combo_box_parent_class)->constructor
    (type, n_construct_properties, construct_properties);

  combo_box = GTK_COMBO_BOX (object);
  priv      = combo_box->priv;

  if (!priv->area)
    {
      priv->area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->area);
    }

  priv->cell_view = gtk_cell_view_new_with_context (priv->area, NULL);
  gtk_cell_view_set_fit_model (GTK_CELL_VIEW (priv->cell_view), TRUE);
  gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view), priv->model);
  gtk_widget_set_parent (priv->cell_view, GTK_WIDGET (combo_box));
  _gtk_bin_set_child (GTK_BIN (combo_box), priv->cell_view);
  gtk_widget_show (priv->cell_view);

  gtk_combo_box_check_appearance (combo_box);

  if (priv->has_entry)
    {
      GtkWidget *entry;
      GtkStyleContext *context;

      entry = gtk_entry_new ();
      gtk_widget_show (entry);
      gtk_container_add (GTK_CONTAINER (combo_box), entry);

      context = gtk_widget_get_style_context (GTK_WIDGET (combo_box));
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_COMBOBOX_ENTRY);

      priv->text_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box),
                                  priv->text_renderer, TRUE);

      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), -1);

      g_signal_connect (combo_box, "changed",
                        G_CALLBACK (gtk_combo_box_entry_active_changed), NULL);
    }

  return object;
}


static void
gtk_combo_box_dispose(GObject* object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_combo_box_menu_destroy (combo_box);
      gtk_menu_detach (GTK_MENU (combo_box->priv->popup_widget));
      combo_box->priv->popup_widget = NULL;
    }

  if (combo_box->priv->area)
    {
      g_object_unref (combo_box->priv->area);
      combo_box->priv->area = NULL;
    }

  if (GTK_IS_TREE_VIEW (combo_box->priv->tree_view))
    gtk_combo_box_list_destroy (combo_box);

  if (combo_box->priv->popup_window)
    {
      gtk_widget_destroy (combo_box->priv->popup_window);
      combo_box->priv->popup_window = NULL;
    }

  gtk_combo_box_unset_model (combo_box);

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->dispose (object);
}

static void
gtk_combo_box_finalize (GObject *object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  g_free (combo_box->priv->tearoff_title);

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->finalize (object);
}

static gboolean
gtk_cell_editable_key_press (GtkWidget   *widget,
                             GdkEventKey *event,
                             gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (event->keyval == GDK_KEY_Escape)
    {
      g_object_set (combo_box,
                    "editing-canceled", TRUE,
                    NULL);
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

      return TRUE;
    }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_ISO_Enter ||
           event->keyval == GDK_KEY_KP_Enter)
    {
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

      return TRUE;
    }

  return FALSE;
}

static gboolean
popdown_idle (gpointer data)
{
  GtkComboBox *combo_box;

  combo_box = GTK_COMBO_BOX (data);

  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

  g_object_unref (combo_box);

  return FALSE;
}

static void
popdown_handler (GtkWidget *widget,
                 gpointer   data)
{
  gdk_threads_add_idle (popdown_idle, g_object_ref (data));
}

static gboolean
popup_idle (gpointer data)
{
  GtkComboBox *combo_box;

  combo_box = GTK_COMBO_BOX (data);

  if (GTK_IS_MENU (combo_box->priv->popup_widget) &&
      combo_box->priv->cell_view)
    g_signal_connect_object (combo_box->priv->popup_widget,
                             "unmap", G_CALLBACK (popdown_handler),
                             combo_box, 0);

  /* we unset this if a menu item is activated */
  g_object_set (combo_box,
                "editing-canceled", TRUE,
                NULL);
  gtk_combo_box_popup (combo_box);

  combo_box->priv->popup_idle_id = 0;
  combo_box->priv->activate_button = 0;
  combo_box->priv->activate_time = 0;

  return FALSE;
}

static void
gtk_combo_box_start_editing (GtkCellEditable *cell_editable,
                             GdkEvent        *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (cell_editable);
  GtkWidget *child;

  combo_box->priv->is_cell_renderer = TRUE;

  if (combo_box->priv->cell_view)
    {
      g_signal_connect_object (combo_box->priv->button, "key-press-event",
                               G_CALLBACK (gtk_cell_editable_key_press),
                               cell_editable, 0);

      gtk_widget_grab_focus (combo_box->priv->button);
    }
  else
    {
      child = gtk_bin_get_child (GTK_BIN (combo_box));

      g_signal_connect_object (child, "key-press-event",
                               G_CALLBACK (gtk_cell_editable_key_press),
                               cell_editable, 0);

      gtk_widget_grab_focus (child);
      gtk_widget_set_can_focus (combo_box->priv->button, FALSE);
    }

  /* we do the immediate popup only for the optionmenu-like
   * appearance
   */
  if (combo_box->priv->is_cell_renderer &&
      combo_box->priv->cell_view && !combo_box->priv->tree_view)
    {
      if (event && event->type == GDK_BUTTON_PRESS)
        {
          GdkEventButton *event_button = (GdkEventButton *)event;

          combo_box->priv->activate_button = event_button->button;
          combo_box->priv->activate_time = event_button->time;
        }

      combo_box->priv->popup_idle_id =
          gdk_threads_add_idle (popup_idle, combo_box);
    }
}


/**
 * gtk_combo_box_get_add_tearoffs:
 * @combo_box: a #GtkComboBox
 *
 * Gets the current value of the :add-tearoffs property.
 *
 * Return value: the current value of the :add-tearoffs property.
 */
gboolean
gtk_combo_box_get_add_tearoffs (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->add_tearoffs;
}

/**
 * gtk_combo_box_set_add_tearoffs:
 * @combo_box: a #GtkComboBox
 * @add_tearoffs: %TRUE to add tearoff menu items
 *
 * Sets whether the popup menu should have a tearoff
 * menu item.
 *
 * Since: 2.6
 */
void
gtk_combo_box_set_add_tearoffs (GtkComboBox *combo_box,
                                gboolean     add_tearoffs)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  add_tearoffs = add_tearoffs != FALSE;

  if (combo_box->priv->add_tearoffs != add_tearoffs)
    {
      combo_box->priv->add_tearoffs = add_tearoffs;
      gtk_combo_box_check_appearance (combo_box);

      if (GTK_IS_TREE_MENU (combo_box->priv->popup_widget))
        _gtk_tree_menu_set_tearoff (GTK_TREE_MENU (combo_box->priv->popup_widget),
                                    combo_box->priv->add_tearoffs);

      g_object_notify (G_OBJECT (combo_box), "add-tearoffs");
    }
}

/**
 * gtk_combo_box_get_title:
 * @combo_box: a #GtkComboBox
 *
 * Gets the current title of the menu in tearoff mode. See
 * gtk_combo_box_set_add_tearoffs().
 *
 * Returns: the menu's title in tearoff mode. This is an internal copy of the
 * string which must not be freed.
 *
 * Since: 2.10
 */
const gchar*
gtk_combo_box_get_title (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->tearoff_title;
}

static void
gtk_combo_box_update_title (GtkComboBox *combo_box)
{
  gtk_combo_box_check_appearance (combo_box);

  if (combo_box->priv->popup_widget &&
      GTK_IS_MENU (combo_box->priv->popup_widget))
    gtk_menu_set_title (GTK_MENU (combo_box->priv->popup_widget),
                        combo_box->priv->tearoff_title);
}

/**
 * gtk_combo_box_set_title:
 * @combo_box: a #GtkComboBox
 * @title: a title for the menu in tearoff mode
 *
 * Sets the menu's title in tearoff mode.
 *
 * Since: 2.10
 */
void
gtk_combo_box_set_title (GtkComboBox *combo_box,
                         const gchar *title)
{
  GtkComboBoxPrivate *priv;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  priv = combo_box->priv;

  if (strcmp (title ? title : "",
              priv->tearoff_title ? priv->tearoff_title : "") != 0)
    {
      g_free (priv->tearoff_title);
      priv->tearoff_title = g_strdup (title);

      gtk_combo_box_update_title (combo_box);

      g_object_notify (G_OBJECT (combo_box), "tearoff-title");
    }
}


/**
 * gtk_combo_box_set_popup_fixed_width:
 * @combo_box: a #GtkComboBox
 * @fixed: whether to use a fixed popup width
 *
 * Specifies whether the popup's width should be a fixed width
 * matching the allocated width of the combo box.
 *
 * Since: 3.0
 **/
void
gtk_combo_box_set_popup_fixed_width (GtkComboBox *combo_box,
                                     gboolean     fixed)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (combo_box->priv->popup_fixed_width != fixed)
    {
      combo_box->priv->popup_fixed_width = fixed;

      g_object_notify (G_OBJECT (combo_box), "popup-fixed-width");
    }
}

/**
 * gtk_combo_box_get_popup_fixed_width:
 * @combo_box: a #GtkComboBox
 *
 * Gets whether the popup uses a fixed width matching
 * the allocated width of the combo box.
 *
 * Returns: %TRUE if the popup uses a fixed width
 *
 * Since: 3.0
 **/
gboolean
gtk_combo_box_get_popup_fixed_width (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->popup_fixed_width;
}


/**
 * gtk_combo_box_get_popup_accessible:
 * @combo_box: a #GtkComboBox
 *
 * Gets the accessible object corresponding to the combo box's popup.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Returns: (transfer none): the accessible object corresponding
 *     to the combo box's popup.
 *
 * Since: 2.6
 */
AtkObject*
gtk_combo_box_get_popup_accessible (GtkComboBox *combo_box)
{
  AtkObject *atk_obj;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  if (combo_box->priv->popup_widget)
    {
      atk_obj = gtk_widget_get_accessible (combo_box->priv->popup_widget);
      return atk_obj;
    }

  return NULL;
}

/**
 * gtk_combo_box_get_row_separator_func: (skip)
 * @combo_box: a #GtkComboBox
 *
 * Returns the current row separator function.
 *
 * Return value: the current row separator function.
 *
 * Since: 2.6
 */
GtkTreeViewRowSeparatorFunc
gtk_combo_box_get_row_separator_func (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->row_separator_func;
}

/**
 * gtk_combo_box_set_row_separator_func:
 * @combo_box: a #GtkComboBox
 * @func: a #GtkTreeViewRowSeparatorFunc
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): destroy notifier for @data, or %NULL
 *
 * Sets the row separator function, which is used to determine
 * whether a row should be drawn as a separator. If the row separator
 * function is %NULL, no separators are drawn. This is the default value.
 *
 * Since: 2.6
 */
void
gtk_combo_box_set_row_separator_func (GtkComboBox                 *combo_box,
                                      GtkTreeViewRowSeparatorFunc  func,
                                      gpointer                     data,
                                      GDestroyNotify               destroy)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (combo_box->priv->row_separator_destroy)
    combo_box->priv->row_separator_destroy (combo_box->priv->row_separator_data);

  combo_box->priv->row_separator_func = func;
  combo_box->priv->row_separator_data = data;
  combo_box->priv->row_separator_destroy = destroy;

  /* Provoke the underlying treeview/menu to rebuild themselves with the new separator func */
  if (combo_box->priv->tree_view)
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (combo_box->priv->tree_view), NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (combo_box->priv->tree_view), combo_box->priv->model);
    }

  if (GTK_IS_TREE_MENU (combo_box->priv->popup_widget))
    {
      _gtk_tree_menu_set_model (GTK_TREE_MENU (combo_box->priv->popup_widget), NULL);
      _gtk_tree_menu_set_model (GTK_TREE_MENU (combo_box->priv->popup_widget), combo_box->priv->model);
    }

  gtk_widget_queue_draw (GTK_WIDGET (combo_box));
}

/**
 * gtk_combo_box_set_button_sensitivity:
 * @combo_box: a #GtkComboBox
 * @sensitivity: specify the sensitivity of the dropdown button
 *
 * Sets whether the dropdown button of the combo box should be
 * always sensitive (%GTK_SENSITIVITY_ON), never sensitive (%GTK_SENSITIVITY_OFF)
 * or only if there is at least one item to display (%GTK_SENSITIVITY_AUTO).
 *
 * Since: 2.14
 **/
void
gtk_combo_box_set_button_sensitivity (GtkComboBox        *combo_box,
                                      GtkSensitivityType  sensitivity)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (combo_box->priv->button_sensitivity != sensitivity)
    {
      combo_box->priv->button_sensitivity = sensitivity;
      gtk_combo_box_update_sensitivity (combo_box);

      g_object_notify (G_OBJECT (combo_box), "button-sensitivity");
    }
}

/**
 * gtk_combo_box_get_button_sensitivity:
 * @combo_box: a #GtkComboBox
 *
 * Returns whether the combo box sets the dropdown button
 * sensitive or not when there are no items in the model.
 *
 * Return Value: %GTK_SENSITIVITY_ON if the dropdown button
 *    is sensitive when the model is empty, %GTK_SENSITIVITY_OFF
 *    if the button is always insensitive or
 *    %GTK_SENSITIVITY_AUTO if it is only sensitive as long as
 *    the model has one item to be selected.
 *
 * Since: 2.14
 **/
GtkSensitivityType
gtk_combo_box_get_button_sensitivity (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->button_sensitivity;
}


/**
 * gtk_combo_box_get_has_entry:
 * @combo_box: a #GtkComboBox
 *
 * Returns whether the combo box has an entry.
 *
 * Return Value: whether there is an entry in @combo_box.
 *
 * Since: 2.24
 **/
gboolean
gtk_combo_box_get_has_entry (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return combo_box->priv->has_entry;
}

/**
 * gtk_combo_box_set_entry_text_column:
 * @combo_box: A #GtkComboBox
 * @text_column: A column in @model to get the strings from for
 *     the internal entry
 *
 * Sets the model column which @combo_box should use to get strings from
 * to be @text_column. The column @text_column in the model of @combo_box
 * must be of type %G_TYPE_STRING.
 *
 * This is only relevant if @combo_box has been created with
 * #GtkComboBox:has-entry as %TRUE.
 *
 * Since: 2.24
 */
void
gtk_combo_box_set_entry_text_column (GtkComboBox *combo_box,
                                     gint         text_column)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeModel *model;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  model = gtk_combo_box_get_model (combo_box);

  g_return_if_fail (text_column >= 0);
  g_return_if_fail (model == NULL || text_column < gtk_tree_model_get_n_columns (model));

  priv->text_column = text_column;

  if (priv->text_renderer != NULL)
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box),
                                    priv->text_renderer,
                                    "text", text_column,
                                    NULL);
}

/**
 * gtk_combo_box_get_entry_text_column:
 * @combo_box: A #GtkComboBox.
 *
 * Returns the column which @combo_box is using to get the strings
 * from to display in the internal entry.
 *
 * Return value: A column in the data source model of @combo_box.
 *
 * Since: 2.24
 */
gint
gtk_combo_box_get_entry_text_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return combo_box->priv->text_column;
}

/**
 * gtk_combo_box_set_focus_on_click:
 * @combo: a #GtkComboBox
 * @focus_on_click: whether the combo box grabs focus when clicked
 *    with the mouse
 *
 * Sets whether the combo box will grab focus when it is clicked with
 * the mouse. Making mouse clicks not grab focus is useful in places
 * like toolbars where you don't want the keyboard focus removed from
 * the main area of the application.
 *
 * Since: 2.6
 */
void
gtk_combo_box_set_focus_on_click (GtkComboBox *combo_box,
                                  gboolean     focus_on_click)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  focus_on_click = focus_on_click != FALSE;

  if (combo_box->priv->focus_on_click != focus_on_click)
    {
      combo_box->priv->focus_on_click = focus_on_click;

      if (combo_box->priv->button)
        gtk_button_set_focus_on_click (GTK_BUTTON (combo_box->priv->button),
                                       focus_on_click);

      g_object_notify (G_OBJECT (combo_box), "focus-on-click");
    }
}

/**
 * gtk_combo_box_get_focus_on_click:
 * @combo: a #GtkComboBox
 *
 * Returns whether the combo box grabs focus when it is clicked
 * with the mouse. See gtk_combo_box_set_focus_on_click().
 *
 * Return value: %TRUE if the combo box grabs focus when it is
 *     clicked with the mouse.
 *
 * Since: 2.6
 */
gboolean
gtk_combo_box_get_focus_on_click (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);
  
  return combo_box->priv->focus_on_click;
}


static gboolean
gtk_combo_box_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  return _gtk_cell_layout_buildable_custom_tag_start (buildable, builder, child,
                                                      tagname, parser, data);
}

static void
gtk_combo_box_buildable_custom_tag_end (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const gchar  *tagname,
                                        gpointer     *data)
{
  if (!_gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname, data))
    parent_buildable_iface->custom_tag_end (buildable, builder, child, tagname, data);
}

static GObject *
gtk_combo_box_buildable_get_internal_child (GtkBuildable *buildable,
                                            GtkBuilder   *builder,
                                            const gchar  *childname)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (buildable);

  if (combo_box->priv->has_entry && strcmp (childname, "entry") == 0)
    return G_OBJECT (gtk_bin_get_child (GTK_BIN (buildable)));

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
gtk_combo_box_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  GtkComboBox           *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate    *priv = combo_box->priv;
  gint                   font_size, arrow_size;
  PangoContext          *context;
  PangoFontMetrics      *metrics;
  GtkWidget             *child;
  gint                   minimum_width = 0, natural_width = 0;
  gint                   child_min, child_nat;
  GtkBorder              padding;
  gfloat                 arrow_scaling;

  child = gtk_bin_get_child (GTK_BIN (widget));

  /* common */
  gtk_widget_get_preferred_width (child, &child_min, &child_nat);

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "arrow-size", &arrow_size,
                        "arrow-scaling", &arrow_scaling,
                        NULL);

  get_widget_padding_and_border (widget, &padding);

  context = gtk_widget_get_pango_context (GTK_WIDGET (widget));
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));
  font_size = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                            pango_font_metrics_get_descent (metrics));
  pango_font_metrics_unref (metrics);

  arrow_size = MAX (arrow_size, font_size) * arrow_scaling;

  gtk_widget_set_size_request (priv->arrow, arrow_size, arrow_size);

  if (!priv->tree_view)
    {
      /* menu mode */
      if (priv->cell_view)
        {
          gint box_width, xpad;
          GtkBorder button_padding;

          get_widget_padding_and_border (priv->button, &button_padding);

          gtk_widget_get_preferred_width (priv->box, &box_width, NULL);
          xpad = button_padding.left + button_padding.right + padding.left + padding.right;

          minimum_width  = child_min + box_width + xpad;
          natural_width  = child_nat + box_width + xpad;
        }
      else
        {
          gint but_width, but_nat_width;

          gtk_widget_get_preferred_width (priv->button,
                                          &but_width, &but_nat_width);

          minimum_width  = child_min + but_width;
          natural_width  = child_nat + but_nat_width;
        }
    }
  else
    {
      /* list mode */
      gint button_width, button_nat_width;

      /* sample + frame */
      minimum_width = child_min;
      natural_width = child_nat;

      if (priv->cell_view_frame)
        {
          if (priv->has_frame)
            {
              gint border_width, xpad;
              GtkBorder frame_padding;

              border_width = gtk_container_get_border_width (GTK_CONTAINER (priv->cell_view_frame));
              get_widget_padding_and_border (priv->cell_view_frame, &frame_padding);
              xpad = (2 * border_width) + frame_padding.left + frame_padding.right;

              minimum_width  += xpad;
              natural_width  += xpad;
            }
        }

      /* the button */
      gtk_widget_get_preferred_width (priv->button,
                                      &button_width, &button_nat_width);

      minimum_width += button_width;
      natural_width += button_nat_width;
    }

  minimum_width += padding.left + padding.right;
  natural_width += padding.left + padding.right;

  if (minimum_size)
    *minimum_size = minimum_width;

  if (natural_size)
    *natural_size = natural_width;
}

static void
gtk_combo_box_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gint min_width;

  /* Combo box is height-for-width only
   * (so we always just reserve enough height for the minimum width) */
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_width, NULL);
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, min_width, minimum_size, natural_size);
}

static void
gtk_combo_box_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       avail_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  /* Combo box is height-for-width only
   * (so we assume we always reserved enough height for the minimum width) */
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_size, natural_size);
}


static void
gtk_combo_box_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       avail_size,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  GtkComboBox           *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate    *priv = combo_box->priv;
  gint                   min_height = 0, nat_height = 0;
  gint                   size;
  GtkWidget             *child;
  GtkBorder              padding;

  child = gtk_bin_get_child (GTK_BIN (widget));

  get_widget_padding_and_border (widget, &padding);
  size = avail_size;

  if (!priv->tree_view)
    {
      /* menu mode */
      if (priv->cell_view)
        {
          /* calculate x/y padding and separator/arrow size */
          gint box_width, box_height;
          gint xpad, ypad;
          GtkBorder button_padding;

          get_widget_padding_and_border (priv->button, &button_padding);

          gtk_widget_get_preferred_width (priv->box, &box_width, NULL);
          gtk_widget_get_preferred_height_for_width (priv->box,
                                                     box_width, &box_height, NULL);

          xpad = button_padding.left + button_padding.right;
          ypad = button_padding.top + button_padding.bottom;

          size -= box_width + xpad;

          /* Get height-for-width of the child widget, usually a GtkCellArea calculating
           * and fitting the whole treemodel */
          gtk_widget_get_preferred_height_for_width (child, size, &min_height, &nat_height);

          min_height = MAX (min_height, box_height);
          nat_height = MAX (nat_height, box_height);

          min_height += ypad;
          nat_height += ypad;
        }
      else
        {
          /* there is a custom child widget inside (no priv->cell_view) */
          gint but_width, but_height;

          gtk_widget_get_preferred_width (priv->button, &but_width, NULL);
          gtk_widget_get_preferred_height_for_width (priv->button,
                                                     but_width, &but_height, NULL);

          size -= but_width;

          /* Get height-for-width of the child widget, usually a GtkCellArea calculating
           * and fitting the whole treemodel */
          gtk_widget_get_preferred_height_for_width (child, size, &min_height, &nat_height);

          min_height = MAX (min_height, but_height);
          nat_height = MAX (nat_height, but_height);
        }
    }
  else
    {
      /* list mode */
      gint but_width, but_height;
      gint xpad = 0, ypad = 0;

      gtk_widget_get_preferred_width (priv->button, &but_width, NULL);
      gtk_widget_get_preferred_height_for_width (priv->button,
                                                 but_width, &but_height, NULL);

      if (priv->cell_view_frame && priv->has_frame)
        {
          GtkBorder frame_padding;
          gint border_width;

          border_width = gtk_container_get_border_width (GTK_CONTAINER (priv->cell_view_frame));
          get_widget_padding_and_border (GTK_WIDGET (priv->cell_view_frame), &frame_padding);

          xpad = (2 * border_width) + padding.left + frame_padding.right;
          ypad = (2 * border_width) + padding.top + frame_padding.bottom;
        }

      size -= but_width;
      size -= xpad;

      /* Get height-for-width of the child widget, usually a GtkCellArea calculating
       * and fitting the whole treemodel */
      gtk_widget_get_preferred_height_for_width (child, size, &min_height, &nat_height);

      min_height = MAX (min_height, but_height);
      nat_height = MAX (nat_height, but_height);

      min_height += ypad;
      nat_height += ypad;
    }

  min_height += padding.top + padding.bottom;
  nat_height += padding.top + padding.bottom;

  if (minimum_size)
    *minimum_size = min_height;

  if (natural_size)
    *natural_size = nat_height;
}

/**
 * gtk_combo_box_set_id_column:
 * @combo_box: A #GtkComboBox
 * @id_column: A column in @model to get string IDs for values from
 *
 * Sets the model column which @combo_box should use to get string IDs
 * for values from. The column @id_column in the model of @combo_box
 * must be of type %G_TYPE_STRING.
 *
 * Since: 3.0
 */
void
gtk_combo_box_set_id_column (GtkComboBox *combo_box,
                             gint         id_column)
{
  GtkComboBoxPrivate *priv = combo_box->priv;
  GtkTreeModel *model;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (id_column != priv->id_column)
    {
      model = gtk_combo_box_get_model (combo_box);

      g_return_if_fail (id_column >= 0);
      g_return_if_fail (model == NULL ||
                        id_column < gtk_tree_model_get_n_columns (model));

      priv->id_column = id_column;

      g_object_notify (G_OBJECT (combo_box), "id-column");
      g_object_notify (G_OBJECT (combo_box), "active-id");
    }
}

/**
 * gtk_combo_box_get_id_column:
 * @combo_box: A #GtkComboBox
 *
 * Returns the column which @combo_box is using to get string IDs
 * for values from.
 *
 * Return value: A column in the data source model of @combo_box.
 *
 * Since: 3.0
 */
gint
gtk_combo_box_get_id_column (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return combo_box->priv->id_column;
}

/**
 * gtk_combo_box_get_active_id:
 * @combo_box: a #GtkComboBox
 *
 * Returns the ID of the active row of @combo_box.  This value is taken
 * from the active row and the column specified by the #GtkComboBox:id-column
 * property of @combo_box (see gtk_combo_box_set_id_column()).
 *
 * The returned value is an interned string which means that you can
 * compare the pointer by value to other interned strings and that you
 * must not free it.
 *
 * If the #GtkComboBox:id-column property of @combo_box is not set, or if
 * no row is active, or if the active row has a %NULL ID value, then %NULL
 * is returned.
 *
 * Return value: the ID of the active row, or %NULL
 *
 * Since: 3.0
 **/
const gchar *
gtk_combo_box_get_active_id (GtkComboBox *combo_box)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint column;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  column = combo_box->priv->id_column;

  if (column < 0)
    return NULL;

  model = gtk_combo_box_get_model (combo_box);
  g_return_val_if_fail (gtk_tree_model_get_column_type (model, column) ==
                        G_TYPE_STRING, NULL);

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      const gchar *interned;
      gchar *id;

      gtk_tree_model_get (model, &iter, column, &id, -1);
      interned = g_intern_string (id);
      g_free (id);

      return interned;
    }

  return NULL;
}

/**
 * gtk_combo_box_set_active_id:
 * @combo_box: a #GtkComboBox
 * @active_id: (allow-none): the ID of the row to select, or %NULL
 *
 * Changes the active row of @combo_box to the one that has an ID equal to
 * @active_id, or unsets the active row if @active_id is %NULL.  Rows having
 * a %NULL ID string cannot be made active by this function.
 *
 * If the #GtkComboBox:id-column property of @combo_box is unset or if no
 * row has the given ID then the function does nothing and returns %FALSE.
 *
 * Returns: %TRUE if a row with a matching ID was found.  If a %NULL
 *          @active_id was given to unset the active row, the function
 *          always returns %TRUE.
 *
 * Since: 3.0
 **/
gboolean
gtk_combo_box_set_active_id (GtkComboBox *combo_box,
                             const gchar *active_id)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean match = FALSE;
  gint column;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  if (active_id == NULL)
    {
      gtk_combo_box_set_active (combo_box, -1);
      return TRUE;  /* active row was successfully unset */
    }

  column = combo_box->priv->id_column;

  if (column < 0)
    return FALSE;

  model = gtk_combo_box_get_model (combo_box);
  g_return_val_if_fail (gtk_tree_model_get_column_type (model, column) ==
                        G_TYPE_STRING, FALSE);

  if (gtk_tree_model_get_iter_first (model, &iter))
    do {
      gchar *id;

      gtk_tree_model_get (model, &iter, column, &id, -1);
      if (id != NULL)
        match = strcmp (id, active_id) == 0;
      g_free (id);

      if (match)
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          break;
        }
    } while (gtk_tree_model_iter_next (model, &iter));

    return match;
}
