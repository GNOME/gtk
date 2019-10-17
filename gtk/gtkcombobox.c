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

#include "gtkcomboboxprivate.h"

#include "gtkbindings.h"
#include "gtkbox.h"
#include "gtkcellareabox.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellview.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkframe.h"
#include "gtkiconprivate.h"
#include "gtkintl.h"
#include "gtkliststore.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkprivate.h"
#include "gtktogglebutton.h"
#include "gtktreemenu.h"
#include "gtktypebuiltins.h"
#include "gtkeventcontrollerkey.h"

#include "a11y/gtkcomboboxaccessible.h"

#include <gobject/gvaluecollector.h>
#include <string.h>
#include <stdarg.h>

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
 * To allow the user to enter values not in the model, the “has-entry”
 * property allows the GtkComboBox to contain a #GtkEntry. This entry
 * can be accessed by calling gtk_bin_get_child() on the combo box.
 *
 * For a simple list of textual choices, the model-view API of GtkComboBox
 * can be a bit overwhelming. In this case, #GtkComboBoxText offers a
 * simple alternative. Both GtkComboBox and #GtkComboBoxText can contain
 * an entry.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * combobox
 * ├── box.linked
 * │   ╰── button.combo
 * │       ╰── box
 * │           ├── cellview
 * │           ╰── arrow
 * ╰── window.popup
 * ]|
 *
 * A normal combobox contains a box with the .linked class, a button
 * with the .combo class and inside those buttons, there are a cellview and
 * an arrow.
 *
 * |[<!-- language="plain" -->
 * combobox
 * ├── box.linked
 * │   ├── entry.combo
 * │   ╰── button.combo
 * │       ╰── box
 * │           ╰── arrow
 * ╰── window.popup
 * ]|
 *
 * A GtkComboBox with an entry has a single CSS node with name combobox. It
 * contains a box with the .linked class. That box contains an entry and a
 * button, both with the .combo class added.
 * The button also contains another node with name arrow.
 */


typedef struct
{
  GtkTreeModel *model;

  GtkCellArea *area;

  gint active; /* Only temporary */
  GtkTreeRowReference *active_row;

  GtkWidget *cell_view;

  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *arrow;

  GtkWidget *popup_widget;

  guint popup_idle_id;
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
  guint has_frame : 1;
  guint is_cell_renderer : 1;
  guint editing_canceled : 1;
  guint auto_scroll : 1;
  guint button_sensitivity : 2;
  guint has_entry : 1;
  guint popup_fixed_width : 1;

  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;
} GtkComboBoxPrivate;

/* There are 2 modes to this widget, which can be characterized as follows:
 *
 * 1) no child added:
 *
 * cell_view -> GtkCellView, regular child
 * button -> GtkToggleButton set_parent to combo
 * arrow -> GtkArrow set_parent to button
 * popup_widget -> GtkMenu
 *
 * 2) child added:
 *
 * cell_view -> NULL
 * button -> GtkToggleButton set_parent to combo
 * arrow -> GtkArrow, child of button
 * popup_widget -> GtkMenu
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
  PROP_ACTIVE,
  PROP_HAS_FRAME,
  PROP_POPUP_SHOWN,
  PROP_BUTTON_SENSITIVITY,
  PROP_EDITING_CANCELED,
  PROP_HAS_ENTRY,
  PROP_ENTRY_TEXT_COLUMN,
  PROP_POPUP_FIXED_WIDTH,
  PROP_ID_COLUMN,
  PROP_ACTIVE_ID
};

static guint combo_box_signals[LAST_SIGNAL] = {0,};

/* common */

static void     gtk_combo_box_cell_layout_init     (GtkCellLayoutIface *iface);
static void     gtk_combo_box_cell_editable_init   (GtkCellEditableIface *iface);
static void     gtk_combo_box_constructed          (GObject          *object);
static void     gtk_combo_box_dispose              (GObject          *object);
static void     gtk_combo_box_unmap                (GtkWidget        *widget);
static void     gtk_combo_box_destroy              (GtkWidget        *widget);

static void     gtk_combo_box_set_property         (GObject         *object,
                                                    guint            prop_id,
                                                    const GValue    *value,
                                                    GParamSpec      *spec);
static void     gtk_combo_box_get_property         (GObject         *object,
                                                    guint            prop_id,
                                                    GValue          *value,
                                                    GParamSpec      *spec);

static gboolean gtk_combo_box_grab_focus           (GtkWidget       *widget);
static void     gtk_combo_box_button_toggled       (GtkWidget       *widget,
                                                    gpointer         data);
static void     gtk_combo_box_add                  (GtkContainer    *container,
                                                    GtkWidget       *widget);
static void     gtk_combo_box_remove               (GtkContainer    *container,
                                                    GtkWidget       *widget);

static void     gtk_combo_box_menu_show            (GtkWidget        *menu,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_hide            (GtkWidget        *menu,
                                                    gpointer          user_data);

static void     gtk_combo_box_unset_model          (GtkComboBox      *combo_box);

static void     gtk_combo_box_forall               (GtkContainer     *container,
                                                    GtkCallback       callback,
                                                    gpointer          callback_data);
static void     gtk_combo_box_set_active_internal  (GtkComboBox      *combo_box,
                                                    GtkTreePath      *path);

static void     gtk_combo_box_real_move_active     (GtkComboBox      *combo_box,
                                                    GtkScrollType     scroll);
static void     gtk_combo_box_real_popup           (GtkComboBox      *combo_box);
static gboolean gtk_combo_box_real_popdown         (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                                        gdouble                   dx,
                                                        gdouble                   dy,
                                                        GtkComboBox              *combo_box);

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

static void     gtk_combo_box_menu_activate        (GtkWidget        *menu,
                                                    const gchar      *path,
                                                    GtkComboBox      *combo_box);
static void     gtk_combo_box_update_sensitivity   (GtkComboBox      *combo_box);
static gboolean gtk_combo_box_menu_key (GtkEventControllerKey *key,
                                        guint                  keyval,
                                        guint                  keycode,
                                        GdkModifierType        modifiers,
                                        GtkComboBox           *combo_box);
static void     gtk_combo_box_menu_popup           (GtkComboBox      *combo_box);

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

static void     gtk_combo_box_buildable_init                 (GtkBuildableIface  *iface);
static void     gtk_combo_box_buildable_add_child            (GtkBuildable       *buildable,
                                                              GtkBuilder         *builder,
                                                              GObject            *child,
                                                              const gchar        *type);
static gboolean gtk_combo_box_buildable_custom_tag_start     (GtkBuildable       *buildable,
                                                              GtkBuilder         *builder,
                                                              GObject            *child,
                                                              const gchar        *tagname,
                                                              GtkBuildableParser *parser,
                                                              gpointer           *data);
static void     gtk_combo_box_buildable_custom_tag_end       (GtkBuildable       *buildable,
                                                              GtkBuilder         *builder,
                                                              GObject            *child,
                                                              const gchar        *tagname,
                                                              gpointer            data);
static GObject *gtk_combo_box_buildable_get_internal_child   (GtkBuildable       *buildable,
                                                              GtkBuilder         *builder,
                                                              const gchar        *childname);



/* GtkCellEditable method implementations */
static void     gtk_combo_box_start_editing                  (GtkCellEditable *cell_editable,
                                                              GdkEvent        *event);

G_DEFINE_TYPE_WITH_CODE (GtkComboBox, gtk_combo_box, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkComboBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_combo_box_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                gtk_combo_box_cell_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_combo_box_buildable_init))


/* common */
static void
gtk_combo_box_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  gtk_widget_measure (priv->box,
                      orientation,
                      size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_combo_box_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  gtk_widget_size_allocate (priv->box,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, baseline);

  if (gtk_widget_get_visible (priv->popup_widget))
    {
      gint menu_width;

      gtk_widget_set_size_request (priv->popup_widget, -1, -1);

      if (priv->popup_fixed_width)
        gtk_widget_measure (priv->popup_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                            &menu_width, NULL, NULL, NULL);
      else
        gtk_widget_measure (priv->popup_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                            NULL, &menu_width, NULL, NULL);

      gtk_widget_set_size_request (priv->popup_widget,
                                   MAX (width, menu_width), -1);

      /* reposition the menu after giving it a new width */
      gtk_menu_reposition (GTK_MENU (priv->popup_widget));
    }
}

static void
gtk_combo_box_compute_expand (GtkWidget *widget,
                              gboolean  *hexpand,
                              gboolean  *vexpand)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (combo_box));
  if (child && child != priv->cell_view)
    {
      *hexpand = gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

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

  widget_class = (GtkWidgetClass *)klass;
  widget_class->size_allocate = gtk_combo_box_size_allocate;
  widget_class->mnemonic_activate = gtk_combo_box_mnemonic_activate;
  widget_class->grab_focus = gtk_combo_box_grab_focus;
  widget_class->measure = gtk_combo_box_measure;
  widget_class->unmap = gtk_combo_box_unmap;
  widget_class->destroy = gtk_combo_box_destroy;
  widget_class->compute_expand = gtk_combo_box_compute_expand;

  object_class = (GObjectClass *)klass;
  object_class->constructed = gtk_combo_box_constructed;
  object_class->dispose = gtk_combo_box_dispose;
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
   */
  combo_box_signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  /**
   * GtkComboBox::move-active:
   * @widget: the object that received the signal
   * @scroll_type: a #GtkScrollType
   *
   * The ::move-active signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to move the active selection.
   */
  combo_box_signals[MOVE_ACTIVE] =
    g_signal_new_class_handler (I_("move-active"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_move_active),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1,
                                GTK_TYPE_SCROLL_TYPE);

  /**
   * GtkComboBox::popup:
   * @widget: the object that received the signal
   *
   * The ::popup signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to popup the combo box list.
   *
   * The default binding for this signal is Alt+Down.
   */
  combo_box_signals[POPUP] =
    g_signal_new_class_handler (I_("popup"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_combo_box_real_popup),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);
  /**
   * GtkComboBox::popdown:
   * @button: the object which received the signal
   *
   * The ::popdown signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to popdown the combo box list.
   *
   * The default bindings for this signal are Alt+Up and Escape.
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
   * |[<!-- language="C" -->
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
   *   return g_strdup_printf ("%g", value);
   * }
   * ]|
   *
   * Returns: (transfer full): a newly allocated string representing @path
   * for the current GtkComboBox model.
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
   */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("ComboBox model"),
                                                        P_("The model for the combo box"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkComboBox:active:
   *
   * The item which is currently active. If the model is a non-flat treemodel,
   * and the active item is not an immediate child of the root of the tree,
   * this property has the value
   * `gtk_tree_path_get_indices (path)[0]`,
   * where `path` is the #GtkTreePath of the active item.
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_int ("active",
                                                     P_("Active item"),
                                                     P_("The item which is currently active"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkComboBox:has-frame:
   *
   * The has-frame property controls whether a frame
   * is drawn around the entry.
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_FRAME,
                                   g_param_spec_boolean ("has-frame",
                                                         P_("Has Frame"),
                                                         P_("Whether the combo box draws a frame around the child"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkComboBox:popup-shown:
   *
   * Whether the combo boxes dropdown is popped up.
   * Note that this property is mainly useful, because
   * it allows you to connect to notify::popup-shown.
   */
  g_object_class_install_property (object_class,
                                   PROP_POPUP_SHOWN,
                                   g_param_spec_boolean ("popup-shown",
                                                         P_("Popup shown"),
                                                         P_("Whether the combo’s dropdown is shown"),
                                                         FALSE,
                                                         GTK_PARAM_READABLE));


   /**
    * GtkComboBox:button-sensitivity:
    *
    * Whether the dropdown button is sensitive when
    * the model is empty.
    */
   g_object_class_install_property (object_class,
                                    PROP_BUTTON_SENSITIVITY,
                                    g_param_spec_enum ("button-sensitivity",
                                                       P_("Button Sensitivity"),
                                                       P_("Whether the dropdown button is sensitive when the model is empty"),
                                                       GTK_TYPE_SENSITIVITY_TYPE,
                                                       GTK_SENSITIVITY_AUTO,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:has-entry:
    *
    * Whether the combo box has an entry.
    */
   g_object_class_install_property (object_class,
                                    PROP_HAS_ENTRY,
                                    g_param_spec_boolean ("has-entry",
                                                          P_("Has Entry"),
                                                          P_("Whether combo box has an entry"),
                                                          FALSE,
                                                          GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

   /**
    * GtkComboBox:entry-text-column:
    *
    * The column in the combo box's model to associate with strings from the entry
    * if the combo was created with #GtkComboBox:has-entry = %TRUE.
    */
   g_object_class_install_property (object_class,
                                    PROP_ENTRY_TEXT_COLUMN,
                                    g_param_spec_int ("entry-text-column",
                                                      P_("Entry Text Column"),
                                                      P_("The column in the combo box’s model to associate "
                                                         "with strings from the entry if the combo was "
                                                         "created with #GtkComboBox:has-entry = %TRUE"),
                                                      -1, G_MAXINT, -1,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:id-column:
    *
    * The column in the combo box's model that provides string
    * IDs for the values in the model, if != -1.
    */
   g_object_class_install_property (object_class,
                                    PROP_ID_COLUMN,
                                    g_param_spec_int ("id-column",
                                                      P_("ID Column"),
                                                      P_("The column in the combo box’s model that provides "
                                                      "string IDs for the values in the model"),
                                                      -1, G_MAXINT, -1,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:active-id:
    *
    * The value of the ID column of the active row.
    */
   g_object_class_install_property (object_class,
                                    PROP_ACTIVE_ID,
                                    g_param_spec_string ("active-id",
                                                         P_("Active id"),
                                                         P_("The value of the id column "
                                                         "for the active row"),
                                                         NULL,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkComboBox:popup-fixed-width:
    *
    * Whether the popup's width should be a fixed width matching the
    * allocated width of the combo box.
    */
   g_object_class_install_property (object_class,
                                    PROP_POPUP_FIXED_WIDTH,
                                    g_param_spec_boolean ("popup-fixed-width",
                                                          P_("Popup Fixed Width"),
                                                          P_("Whether the popup’s width should be a "
                                                             "fixed width matching the allocated width "
                                                             "of the combo box"),
                                                          TRUE,
                                                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkcombobox.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, box);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, button);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, arrow);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkComboBox, popup_widget);
  gtk_widget_class_bind_template_callback (widget_class, gtk_combo_box_button_toggled);
  gtk_widget_class_bind_template_callback (widget_class, gtk_combo_box_menu_activate);
  gtk_widget_class_bind_template_callback (widget_class, gtk_combo_box_menu_key);
  gtk_widget_class_bind_template_callback (widget_class, gtk_combo_box_menu_show);
  gtk_widget_class_bind_template_callback (widget_class, gtk_combo_box_menu_hide);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_COMBO_BOX_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("combobox"));
}

static void
gtk_combo_box_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_combo_box_buildable_add_child;
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

static gboolean
gtk_combo_box_row_separator_func (GtkTreeModel      *model,
                                  GtkTreeIter       *iter,
                                  GtkComboBox       *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->row_separator_func)
    return priv->row_separator_func (model, iter, priv->row_separator_data);

  return FALSE;
}

static void
gtk_combo_box_init (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkStyleContext *context;
  GtkTreeMenu *menu;
  GtkEventController *controller;

  priv->active = -1;
  priv->active_row = NULL;

  priv->popup_shown = FALSE;
  priv->has_frame = TRUE;
  priv->is_cell_renderer = FALSE;
  priv->editing_canceled = FALSE;
  priv->auto_scroll = FALSE;
  priv->button_sensitivity = GTK_SENSITIVITY_AUTO;
  priv->has_entry = FALSE;
  priv->popup_fixed_width = TRUE;

  priv->text_column = -1;
  priv->text_renderer = NULL;
  priv->id_column = -1;

  g_type_ensure (GTK_TYPE_ICON);
  g_type_ensure (GTK_TYPE_TREE_MENU);
  gtk_widget_init_template (GTK_WIDGET (combo_box));

  context = gtk_widget_get_style_context (priv->button);
  gtk_style_context_remove_class (context, "toggle");
  gtk_style_context_add_class (context, "combo");

  menu = GTK_TREE_MENU (priv->popup_widget);
  _gtk_tree_menu_set_row_separator_func (menu,
                                         (GtkTreeViewRowSeparatorFunc)gtk_combo_box_row_separator_func,
                                         combo_box, NULL);
  gtk_menu_attach_to_widget (GTK_MENU (menu),
                             GTK_WIDGET (combo_box),
                             NULL);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                                GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  g_signal_connect (controller, "scroll",
                    G_CALLBACK (gtk_combo_box_scroll_controller_scroll),
                    combo_box);
  gtk_widget_add_controller (GTK_WIDGET (combo_box), controller);
}

static void
gtk_combo_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_combo_box_set_model (combo_box, g_value_get_object (value));
      break;

    case PROP_ACTIVE:
      gtk_combo_box_set_active (combo_box, g_value_get_int (value));
      break;

    case PROP_HAS_FRAME:
      if (priv->has_frame != g_value_get_boolean (value))
        {
          priv->has_frame = g_value_get_boolean (value);
          if (priv->has_entry)
            gtk_entry_set_has_frame (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_box))),
                                     priv->has_frame);
          g_object_notify (object, "has-frame");
        }
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
      if (priv->editing_canceled != g_value_get_boolean (value))
        {
          priv->editing_canceled = g_value_get_boolean (value);
          g_object_notify (object, "editing-canceled");
        }
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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value, priv->model);
        break;

      case PROP_ACTIVE:
        g_value_set_int (value, gtk_combo_box_get_active (combo_box));
        break;

      case PROP_HAS_FRAME:
        g_value_set_boolean (value, priv->has_frame);
        break;

      case PROP_POPUP_SHOWN:
        g_value_set_boolean (value, priv->popup_shown);
        break;

      case PROP_BUTTON_SENSITIVITY:
        g_value_set_enum (value, priv->button_sensitivity);
        break;

      case PROP_POPUP_FIXED_WIDTH:
        g_value_set_boolean (value, priv->popup_fixed_width);
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

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_button_toggled (GtkWidget *widget,
                              gpointer   data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (!priv->popup_in_progress)
        gtk_combo_box_popup (combo_box);
    }
  else
    {
      gtk_combo_box_popdown (combo_box);
    }
}

static void
gtk_combo_box_create_child (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkWidget *child;

  if (priv->has_entry)
    {
      GtkWidget *entry;
      GtkStyleContext *context;

      entry = gtk_entry_new ();
      gtk_container_add (GTK_CONTAINER (combo_box), entry);

      context = gtk_widget_get_style_context (GTK_WIDGET (entry));
      gtk_style_context_add_class (context, "combo");

      g_signal_connect (combo_box, "changed",
                        G_CALLBACK (gtk_combo_box_entry_active_changed), NULL);
    }
  else
    {
      child = gtk_cell_view_new_with_context (priv->area, NULL);
      priv->cell_view = child;
      gtk_widget_set_hexpand (child, TRUE);
      gtk_cell_view_set_fit_model (GTK_CELL_VIEW (priv->cell_view), TRUE);
      gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view), priv->model);
      gtk_box_insert_child_after (GTK_BOX (gtk_widget_get_parent (priv->arrow)), priv->cell_view, NULL);
      _gtk_bin_set_child (GTK_BIN (combo_box), priv->cell_view);
    }
}

static void
gtk_combo_box_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->box == NULL)
    {
      gtk_widget_set_parent (widget, GTK_WIDGET (container));
      return;
    }

  if (priv->has_entry && !GTK_IS_ENTRY (widget))
    {
      g_warning ("Attempting to add a widget with type %s to a GtkComboBox that needs an entry "
                 "(need an instance of GtkEntry or of a subclass)",
                 G_OBJECT_TYPE_NAME (widget));
      return;
    }

  if (priv->cell_view)
    {
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (priv->cell_view)),
                            priv->cell_view);
      _gtk_bin_set_child (GTK_BIN (container), NULL);
      priv->cell_view = NULL;
    }

  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_insert_child_after (GTK_BOX (priv->box), widget, NULL);
  _gtk_bin_set_child (GTK_BIN (container), widget);

  if (priv->has_entry)
    {
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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreePath *path;
  if (priv->has_entry)
    {
      GtkWidget *child_widget;

      child_widget = gtk_bin_get_child (GTK_BIN (container));
      if (widget && widget == child_widget)
        {
          g_signal_handlers_disconnect_by_func (widget,
                                                gtk_combo_box_entry_contents_changed,
                                                container);
        }
    }

  gtk_container_remove (GTK_CONTAINER (priv->box), widget);
  _gtk_bin_set_child (GTK_BIN (container), NULL);

  if (gtk_widget_in_destruction (GTK_WIDGET (combo_box)))
    return;

  gtk_widget_queue_resize (GTK_WIDGET (container));

  gtk_combo_box_create_child (combo_box);

  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      path = gtk_tree_row_reference_get_path (priv->active_row);
      gtk_combo_box_set_active_internal (combo_box, path);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_combo_box_set_active_internal (combo_box, NULL);
    }
}

static void
gtk_combo_box_menu_show (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  gtk_combo_box_child_hide (menu,user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                FALSE);
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
tree_column_row_is_sensitive (GtkComboBox *combo_box,
                              GtkTreeIter *iter)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->row_separator_func)
    {
      if (priv->row_separator_func (priv->model, iter,
                                    priv->row_separator_data))
        return FALSE;
    }

  gtk_cell_area_apply_attributes (priv->area, priv->model, iter, FALSE, FALSE);
  return cell_layout_is_sensitive (GTK_CELL_LAYOUT (priv->area));
}

static void
update_menu_sensitivity (GtkComboBox *combo_box,
                         GtkWidget   *menu)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GList *children, *child;
  GtkWidget *item, *submenu;
  GtkWidget *cell_view;
  gboolean sensitive;

  if (!priv->model)
    return;

  children = gtk_menu_shell_get_items (GTK_MENU_SHELL (menu));

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
          gtk_widget_set_sensitive (item, sensitive);
        }
    }

  g_list_free (children);
}

static void
gtk_combo_box_menu_popup (GtkComboBox    *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  gint active_item;
  GtkWidget *active;
  int width, min_width, nat_width;

  update_menu_sensitivity (combo_box, priv->popup_widget);

  active_item = -1;
  if (gtk_tree_row_reference_valid (priv->active_row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (priv->active_row);
      active_item = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }

  /* FIXME handle nested menus better */
  gtk_menu_set_active (GTK_MENU (priv->popup_widget), active_item);

  width = gtk_widget_get_width (GTK_WIDGET (combo_box));
  gtk_widget_set_size_request (priv->popup_widget, -1, -1);
  gtk_widget_measure (priv->popup_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                      &min_width, &nat_width, NULL, NULL);

  if (priv->popup_fixed_width)
    width = MAX (width, min_width);
  else
    width = MAX (width, nat_width);

  gtk_widget_set_size_request (priv->popup_widget, width, -1);

  g_signal_handlers_disconnect_by_func (priv->popup_widget,
                                        gtk_menu_update_scroll_offset,
                                        NULL);

  g_object_set (priv->popup_widget, "menu-type-hint", GDK_SURFACE_TYPE_HINT_COMBO, NULL);

  if (priv->cell_view == NULL)
    {
      g_object_set (priv->popup_widget,
                    "anchor-hints", (GDK_ANCHOR_FLIP_Y |
                                     GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    "rect-anchor-dx", 0,
                    NULL);

      gtk_menu_popup_at_widget (GTK_MENU (priv->popup_widget),
                                gtk_bin_get_child (GTK_BIN (combo_box)),
                                GDK_GRAVITY_SOUTH_WEST,
                                GDK_GRAVITY_NORTH_WEST,
                                NULL);
    }
  else
    {
      gint rect_anchor_dy = -2;
      GList *i;
      GtkWidget *child;

      /* FIXME handle nested menus better */
      active = gtk_menu_get_active (GTK_MENU (priv->popup_widget));

      if (!(active && gtk_widget_get_visible (active)))
        {
          GList *children;
          children = gtk_menu_shell_get_items (GTK_MENU_SHELL (priv->popup_widget));
          for (i = children; i && !active; i = i->next)
            {
              child = i->data;

              if (child && gtk_widget_get_visible (child))
                active = child;
            }
          g_list_free (children);
        }

      if (active)
        {
          gint child_height;
          GList *children;
          children = gtk_menu_shell_get_items (GTK_MENU_SHELL (priv->popup_widget));
          for (i = children; i && i->data != active; i = i->next)
            {
              child = i->data;

              if (child && gtk_widget_get_visible (child))
                {
                  gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1,
                                      &child_height, NULL, NULL, NULL);
                  rect_anchor_dy -= child_height;
                }
            }
          g_list_free (children);

          gtk_widget_measure (active, GTK_ORIENTATION_VERTICAL, -1,
                              &child_height, NULL, NULL, NULL);
          rect_anchor_dy -= child_height / 2;
        }

      g_object_set (priv->popup_widget,
                    "anchor-hints", (GDK_ANCHOR_SLIDE |
                                     GDK_ANCHOR_RESIZE),
                    "rect-anchor-dy", rect_anchor_dy,
                    NULL);

      g_signal_connect (priv->popup_widget,
                        "popped-up",
                        G_CALLBACK (gtk_menu_update_scroll_offset),
                        NULL);

      gtk_menu_popup_at_widget (GTK_MENU (priv->popup_widget),
                                GTK_WIDGET (combo_box),
                                GDK_GRAVITY_WEST,
                                GDK_GRAVITY_NORTH_WEST,
                                NULL);
    }

    /* Re-get the active item before selecting it, as a popped-up handler – like
     * that of FileChooserButton in folder mode – can refilter the model, making
     * the original active item pointer invalid. This seems ugly and makes some
     * of the above code pointless in such cases, so hopefully we can FIXME. */
    active = gtk_menu_get_active (GTK_MENU (priv->popup_widget));
    if (active && gtk_widget_get_visible (active))
      gtk_menu_shell_select_item (GTK_MENU_SHELL (priv->popup_widget), active);
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
 * Before calling this, @combo_box must be mapped, or nothing will happen.
 */
void
gtk_combo_box_popup (GtkComboBox *combo_box)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (gtk_widget_get_mapped (GTK_WIDGET (combo_box)))
    g_signal_emit (combo_box, combo_box_signals[POPUP], 0);
}

/**
 * gtk_combo_box_popup_for_device:
 * @combo_box: a #GtkComboBox
 * @device: a #GdkDevice
 *
 * Pops up the menu of @combo_box. Note that currently this does not do anything
 * with the device, as it was previously only used for list-mode ComboBoxes,
 * and those were removed in GTK+ 4. However, it is retained in case similar
 * functionality is added back later.
 **/
void
gtk_combo_box_popup_for_device (GtkComboBox *combo_box,
                                GdkDevice   *device)
{
  /* As above, this currently does not do anything useful, and nothing with the
   * passed-in device. But the bits that are not blatantly obsolete are kept. */
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (!gtk_widget_get_realized (GTK_WIDGET (combo_box)))
    return;

  if (gtk_widget_get_mapped (priv->popup_widget))
    return;

  gtk_combo_box_menu_popup (combo_box);
}

static void
gtk_combo_box_real_popup (GtkComboBox *combo_box)
{
  gtk_combo_box_menu_popup (combo_box);
}

static gboolean
gtk_combo_box_real_popdown (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->popup_shown)
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
 */
void
gtk_combo_box_popdown (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  gtk_menu_popdown (GTK_MENU (priv->popup_widget));
}

static void
gtk_combo_box_unset_model (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->model)
    {
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_row_inserted,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_row_deleted,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_rows_reordered,
                                            combo_box);
      g_signal_handlers_disconnect_by_func (priv->model,
                                            gtk_combo_box_model_row_changed,
                                            combo_box);

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
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (container));
  if (child && child != priv->cell_view)
    (* callback) (child, callback_data);
}

static void
gtk_combo_box_child_show (GtkWidget *widget,
                          GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  priv->popup_shown = TRUE;
  g_object_notify (G_OBJECT (combo_box), "popup-shown");
}

static void
gtk_combo_box_child_hide (GtkWidget *widget,
                          GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  priv->popup_shown = FALSE;
  g_object_notify (G_OBJECT (combo_box), "popup-shown");
}

typedef struct {
  GtkComboBox *combo;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean found;
  gboolean set;
} SearchData;

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
           GtkTreeIter  *next)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.path = gtk_tree_model_get_path (model, iter);
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

  search_data->set = TRUE;
  search_data->iter = *iter;

  return FALSE;
}

static gboolean
tree_prev (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *iter,
           GtkTreeIter  *prev)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.path = gtk_tree_model_get_path (model, iter);
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

  search_data->set = TRUE;
  search_data->iter = *iter;

  return FALSE;
}

static gboolean
tree_last (GtkComboBox  *combo,
           GtkTreeModel *model,
           GtkTreeIter  *last)
{
  SearchData search_data;

  search_data.combo = combo;
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

  search_data->set = TRUE;
  search_data->iter = *iter;

  return TRUE;
}

static gboolean
tree_first (GtkComboBox  *combo,
            GtkTreeModel *model,
            GtkTreeIter  *first)
{
  SearchData search_data;

  search_data.combo = combo;
  search_data.set = FALSE;

  gtk_tree_model_foreach (model, tree_first_func, &search_data);

  *first = search_data.iter;

  return search_data.set;
}

static gboolean
gtk_combo_box_scroll_controller_scroll (GtkEventControllerScroll *scroll,
                                        gdouble                   dx,
                                        gdouble                   dy,
                                        GtkComboBox              *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  gboolean found = FALSE;
  GtkTreeIter iter;
  GtkTreeIter new_iter;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    return GDK_EVENT_PROPAGATE;

  if (dy < 0)
    found = tree_prev (combo_box, priv->model, &iter, &new_iter);
  else if (dy > 0)
    found = tree_next (combo_box, priv->model, &iter, &new_iter);

  if (found)
    gtk_combo_box_set_active_iter (combo_box, &new_iter);

  return found;

}

/* callbacks */
static void
gtk_combo_box_menu_activate (GtkWidget   *menu,
                             const gchar *path,
                             GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_from_string (priv->model, &iter, path))
    gtk_combo_box_set_active_iter (combo_box, &iter);

  g_object_set (combo_box,
                "editing-canceled", FALSE,
                NULL);
}


static void
gtk_combo_box_update_sensitivity (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreeIter iter;
  gboolean sensitive = TRUE; /* fool code checkers */

  if (!priv->button)
    return;

  switch (priv->button_sensitivity)
    {
      case GTK_SENSITIVITY_ON:
        sensitive = TRUE;
        break;
      case GTK_SENSITIVITY_OFF:
        sensitive = FALSE;
        break;
      case GTK_SENSITIVITY_AUTO:
        sensitive = priv->model &&
                    gtk_tree_model_get_iter_first (priv->model, &iter);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  gtk_widget_set_sensitive (priv->button, sensitive);
}

static void
gtk_combo_box_model_row_inserted (GtkTreeModel     *model,
                                  GtkTreePath      *path,
                                  GtkTreeIter      *iter,
                                  gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_combo_box_update_sensitivity (combo_box);
}

static void
gtk_combo_box_model_row_deleted (GtkTreeModel     *model,
                                 GtkTreePath      *path,
                                 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (!gtk_tree_row_reference_valid (priv->active_row))
    {
      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view), NULL);
      g_signal_emit (combo_box, combo_box_signals[CHANGED], 0);
    }

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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
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
}

static gboolean
gtk_combo_box_menu_key (GtkEventControllerKey *key,
                        guint                  keyval,
                        guint                  keycode,
                        GdkModifierType        modifiers,
                        GtkComboBox           *combo_box)
{
  GtkWidget *widget;
  GdkEvent *event;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (key));
  event = gtk_get_current_event ();

  if (!gtk_bindings_activate_event (G_OBJECT (widget), (GdkEventKey *)event))
    {
      gtk_event_controller_key_forward (key, GTK_WIDGET (combo_box));
    }

  g_object_unref (event);

  return TRUE;
}

/*
 * GtkCellLayout implementation
 */
static GtkCellArea *
gtk_combo_box_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (cell_layout);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

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
 * Returns: A new #GtkComboBox.
 */
GtkWidget *
gtk_combo_box_new (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX, NULL);
}

/**
 * gtk_combo_box_new_with_entry:
 *
 * Creates a new empty #GtkComboBox with an entry.
 *
 * Returns: A new #GtkComboBox.
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
 * Returns: A new #GtkComboBox.
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
 * Returns: A new #GtkComboBox
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
 * gtk_combo_box_get_active:
 * @combo_box: A #GtkComboBox
 *
 * Returns the index of the currently active item, or -1 if there’s no
 * active item. If the model is a non-flat treemodel, and the active item
 * is not an immediate child of the root of the tree, this function returns
 * `gtk_tree_path_get_indices (path)[0]`, where
 * `path` is the #GtkTreePath of the active item.
 *
 * Returns: An integer which is the index of the currently active item,
 *     or -1 if there’s no active item.
 */
gint
gtk_combo_box_get_active (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  gint result;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

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
 */
void
gtk_combo_box_set_active (GtkComboBox *combo_box,
                          gint         index_)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreePath *path = NULL;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (index_ >= -1);

  if (priv->model == NULL)
    {
      /* Save index, in case the model is set after the index */
      priv->active = index_;
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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
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
      gtk_menu_set_active (GTK_MENU (priv->popup_widget), -1);

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

      /* FIXME handle nested menus better */
      gtk_menu_set_active (GTK_MENU (priv->popup_widget),
                           gtk_tree_path_get_indices (path)[0]);

      if (priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (priv->cell_view),
                                         path);
    }

  g_signal_emit (combo_box, combo_box_signals[CHANGED], 0);
  g_object_notify (G_OBJECT (combo_box), "active");
  if (priv->id_column >= 0)
    g_object_notify (G_OBJECT (combo_box), "active-id");
}


/**
 * gtk_combo_box_get_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: (out): A #GtkTreeIter
 *
 * Sets @iter to point to the currently active item, if any item is active.
 * Otherwise, @iter is left unchanged.
 *
 * Returns: %TRUE if @iter was set, %FALSE otherwise
 */
gboolean
gtk_combo_box_get_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreePath *path;
  gboolean result;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  if (!gtk_tree_row_reference_valid (priv->active_row))
    return FALSE;

  path = gtk_tree_row_reference_get_path (priv->active_row);
  result = gtk_tree_model_get_iter (priv->model, iter, path);
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
 */
void
gtk_combo_box_set_model (GtkComboBox  *combo_box,
                         GtkTreeModel *model)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (model == priv->model)
    return;

  gtk_combo_box_unset_model (combo_box);

  if (model == NULL)
    goto out;

  priv->model = model;
  g_object_ref (priv->model);

  g_signal_connect (priv->model, "row-inserted",
                    G_CALLBACK (gtk_combo_box_model_row_inserted),
                    combo_box);
  g_signal_connect (priv->model, "row-deleted",
                    G_CALLBACK (gtk_combo_box_model_row_deleted),
                    combo_box);
  g_signal_connect (priv->model, "rows-reordered",
                    G_CALLBACK (gtk_combo_box_model_rows_reordered),
                    combo_box);
  g_signal_connect (priv->model, "row-changed",
                    G_CALLBACK (gtk_combo_box_model_row_changed),
                    combo_box);

  _gtk_tree_menu_set_model (GTK_TREE_MENU (priv->popup_widget),
                            priv->model);

  if (priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (priv->cell_view),
                             priv->model);

  if (priv->active != -1)
    {
      /* If an index was set in advance, apply it now */
      gtk_combo_box_set_active (combo_box, priv->active);
      priv->active = -1;
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
 * Returns: (transfer none): A #GtkTreeModel which was passed
 *     during construction.
 */
GtkTreeModel *
gtk_combo_box_get_model (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return priv->model;
}

static void
gtk_combo_box_real_move_active (GtkComboBox   *combo_box,
                                GtkScrollType  scroll)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreeIter iter;
  GtkTreeIter new_iter;
  gboolean    active_iter;
  gboolean    found;

  if (!priv->model)
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
          found = tree_prev (combo_box, priv->model,
                             &iter, &new_iter);
          break;
        }
      G_GNUC_FALLTHROUGH;

    case GTK_SCROLL_PAGE_FORWARD:
    case GTK_SCROLL_PAGE_DOWN:
    case GTK_SCROLL_PAGE_RIGHT:
    case GTK_SCROLL_END:
      found = tree_last (combo_box, priv->model, &new_iter);
      break;

    case GTK_SCROLL_STEP_FORWARD:
    case GTK_SCROLL_STEP_DOWN:
    case GTK_SCROLL_STEP_RIGHT:
      if (active_iter)
        {
          found = tree_next (combo_box, priv->model,
                             &iter, &new_iter);
          break;
        }
      G_GNUC_FALLTHROUGH;

    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_UP:
    case GTK_SCROLL_PAGE_LEFT:
    case GTK_SCROLL_START:
      found = tree_first (combo_box, priv->model, &new_iter);
      break;

    case GTK_SCROLL_NONE:
    case GTK_SCROLL_JUMP:
    default:
      return;
    }

  if (found && active_iter)
    {
      GtkTreePath *old_path;
      GtkTreePath *new_path;

      old_path = gtk_tree_model_get_path (priv->model, &iter);
      new_path = gtk_tree_model_get_path (priv->model, &new_iter);

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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->has_entry)
    {
      GtkWidget* child;

      child = gtk_bin_get_child (GTK_BIN (combo_box));
      if (child)
        gtk_widget_grab_focus (child);
    }
  else
    gtk_widget_grab_focus (priv->button);

  return TRUE;
}

static gboolean
gtk_combo_box_grab_focus (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->has_entry)
    {
      GtkWidget *child;

      child = gtk_bin_get_child (GTK_BIN (combo_box));
      if (child)
        return gtk_widget_grab_focus (child);
      else
        return FALSE;
    }
  else
    return gtk_widget_grab_focus (priv->button);
}

static void
gtk_combo_box_unmap (GtkWidget *widget)
{
  gtk_combo_box_popdown (GTK_COMBO_BOX (widget));

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->unmap (widget);
}

static void
gtk_combo_box_destroy (GtkWidget *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->popup_idle_id > 0)
    {
      g_source_remove (priv->popup_idle_id);
      priv->popup_idle_id = 0;
    }

  if (priv->box)
    {
      /* destroy things (unparent will kill the latest ref from us)
       * last unref on button will destroy the arrow
       */
      gtk_widget_unparent (priv->box);
      priv->box = NULL;
      priv->button = NULL;
      priv->arrow = NULL;
      _gtk_bin_set_child (GTK_BIN (combo_box), NULL);
    }

  if (priv->row_separator_destroy)
    priv->row_separator_destroy (priv->row_separator_data);

  priv->row_separator_func = NULL;
  priv->row_separator_data = NULL;
  priv->row_separator_destroy = NULL;

  GTK_WIDGET_CLASS (gtk_combo_box_parent_class)->destroy (widget);
  priv->cell_view = NULL;
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

          gtk_editable_set_text (GTK_EDITABLE (entry), text);

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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
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

static void
gtk_combo_box_constructed (GObject *object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->constructed (object);

  gtk_combo_box_create_child (combo_box);

  if (priv->has_entry)
    {
      priv->text_renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box),
                                  priv->text_renderer, TRUE);

      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), -1);
    }
}

static void
gtk_combo_box_dispose (GObject* object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->popup_widget)
    {
      /* Stop menu destruction triggering toggle on a now-invalid button */
      g_signal_handlers_disconnect_by_func (priv->popup_widget,
                                            gtk_combo_box_menu_hide,
                                            combo_box);
      gtk_menu_detach (GTK_MENU (priv->popup_widget));
      priv->popup_widget = NULL;
    }

  gtk_combo_box_unset_model (combo_box);

  G_OBJECT_CLASS (gtk_combo_box_parent_class)->dispose (object);
}

static gboolean
gtk_cell_editable_key_pressed (GtkEventControllerKey *key,
                               guint                  keyval,
                               guint                  keycode,
                               GdkModifierType        modifiers,
                               GtkComboBox           *combo_box)
{
  if (keyval == GDK_KEY_Escape)
    {
      g_object_set (combo_box,
                    "editing-canceled", TRUE,
                    NULL);
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

      return TRUE;
    }
  else if (keyval == GDK_KEY_Return ||
           keyval == GDK_KEY_ISO_Enter ||
           keyval == GDK_KEY_KP_Enter)
    {
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (combo_box));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (combo_box));

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_start_editing (GtkCellEditable *cell_editable,
                             GdkEvent        *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (cell_editable);
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkEventController *controller;
  GtkWidget *child;

  priv->is_cell_renderer = TRUE;

  controller = gtk_event_controller_key_new ();
  g_signal_connect_object (controller, "key-pressed",
                           G_CALLBACK (gtk_cell_editable_key_pressed),
                           cell_editable, 0);

  if (priv->cell_view)
    {
      gtk_widget_add_controller (priv->button, controller);
      gtk_widget_grab_focus (priv->button);
    }
  else
    {
      child = gtk_bin_get_child (GTK_BIN (combo_box));
      gtk_widget_add_controller (child, controller);

      gtk_widget_grab_focus (child);
      gtk_widget_set_can_focus (priv->button, FALSE);
    }
}

/**
 * gtk_combo_box_set_popup_fixed_width:
 * @combo_box: a #GtkComboBox
 * @fixed: whether to use a fixed popup width
 *
 * Specifies whether the popup’s width should be a fixed width
 * matching the allocated width of the combo box.
 **/
void
gtk_combo_box_set_popup_fixed_width (GtkComboBox *combo_box,
                                     gboolean     fixed)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (priv->popup_fixed_width != fixed)
    {
      priv->popup_fixed_width = fixed;

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
 **/
gboolean
gtk_combo_box_get_popup_fixed_width (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return priv->popup_fixed_width;
}


/**
 * gtk_combo_box_get_popup_accessible:
 * @combo_box: a #GtkComboBox
 *
 * Gets the accessible object corresponding to the combo box’s popup.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Returns: (transfer none): the accessible object corresponding
 *     to the combo box’s popup.
 */
AtkObject*
gtk_combo_box_get_popup_accessible (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return gtk_widget_get_accessible (priv->popup_widget);
}

/**
 * gtk_combo_box_get_row_separator_func: (skip)
 * @combo_box: a #GtkComboBox
 *
 * Returns the current row separator function.
 *
 * Returns: the current row separator function.
 */
GtkTreeViewRowSeparatorFunc
gtk_combo_box_get_row_separator_func (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return priv->row_separator_func;
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
 */
void
gtk_combo_box_set_row_separator_func (GtkComboBox                 *combo_box,
                                      GtkTreeViewRowSeparatorFunc  func,
                                      gpointer                     data,
                                      GDestroyNotify               destroy)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (priv->row_separator_destroy)
    priv->row_separator_destroy (priv->row_separator_data);

  priv->row_separator_func = func;
  priv->row_separator_data = data;
  priv->row_separator_destroy = destroy;

  /* Make the TreeMenu rebuild itself using the new separator func */
  _gtk_tree_menu_set_model (GTK_TREE_MENU (priv->popup_widget), NULL);
  _gtk_tree_menu_set_model (GTK_TREE_MENU (priv->popup_widget), priv->model);

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
 **/
void
gtk_combo_box_set_button_sensitivity (GtkComboBox        *combo_box,
                                      GtkSensitivityType  sensitivity)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (priv->button_sensitivity != sensitivity)
    {
      priv->button_sensitivity = sensitivity;
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
 * Returns: %GTK_SENSITIVITY_ON if the dropdown button
 *    is sensitive when the model is empty, %GTK_SENSITIVITY_OFF
 *    if the button is always insensitive or
 *    %GTK_SENSITIVITY_AUTO if it is only sensitive as long as
 *    the model has one item to be selected.
 **/
GtkSensitivityType
gtk_combo_box_get_button_sensitivity (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return priv->button_sensitivity;
}


/**
 * gtk_combo_box_get_has_entry:
 * @combo_box: a #GtkComboBox
 *
 * Returns whether the combo box has an entry.
 *
 * Returns: whether there is an entry in @combo_box.
 **/
gboolean
gtk_combo_box_get_has_entry (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  return priv->has_entry;
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
 */
void
gtk_combo_box_set_entry_text_column (GtkComboBox *combo_box,
                                     gint         text_column)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreeModel *model;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  model = gtk_combo_box_get_model (combo_box);

  g_return_if_fail (text_column >= 0);
  g_return_if_fail (model == NULL || text_column < gtk_tree_model_get_n_columns (model));

  if (priv->text_column != text_column)
    {
      priv->text_column = text_column;

      if (priv->text_renderer != NULL)
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box),
                                        priv->text_renderer,
                                        "text", text_column,
                                        NULL);

      g_object_notify (G_OBJECT (combo_box), "entry-text-column");
    }
}

/**
 * gtk_combo_box_get_entry_text_column:
 * @combo_box: A #GtkComboBox.
 *
 * Returns the column which @combo_box is using to get the strings
 * from to display in the internal entry.
 *
 * Returns: A column in the data source model of @combo_box.
 */
gint
gtk_combo_box_get_entry_text_column (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return priv->text_column;
}

static void
gtk_combo_box_buildable_add_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   GObject      *child,
                                   const gchar  *type)
{
  if (GTK_IS_CELL_RENDERER (child))
    _gtk_cell_layout_buildable_add_child (buildable, builder, child, type);
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static gboolean
gtk_combo_box_buildable_custom_tag_start (GtkBuildable       *buildable,
                                          GtkBuilder         *builder,
                                          GObject            *child,
                                          const gchar        *tagname,
                                          GtkBuildableParser *parser,
                                          gpointer           *data)
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
                                        gpointer      data)
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
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  if (priv->has_entry && strcmp (childname, "entry") == 0)
    return G_OBJECT (gtk_bin_get_child (GTK_BIN (buildable)));

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/**
 * gtk_combo_box_set_id_column:
 * @combo_box: A #GtkComboBox
 * @id_column: A column in @model to get string IDs for values from
 *
 * Sets the model column which @combo_box should use to get string IDs
 * for values from. The column @id_column in the model of @combo_box
 * must be of type %G_TYPE_STRING.
 */
void
gtk_combo_box_set_id_column (GtkComboBox *combo_box,
                             gint         id_column)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
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
 * Returns: A column in the data source model of @combo_box.
 */
gint
gtk_combo_box_get_id_column (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return priv->id_column;
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
 * Returns: (nullable): the ID of the active row, or %NULL
 **/
const gchar *
gtk_combo_box_get_active_id (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint column;

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  column = priv->id_column;

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
 **/
gboolean
gtk_combo_box_set_active_id (GtkComboBox *combo_box,
                             const gchar *active_id)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);
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

  column = priv->id_column;

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

  g_object_notify (G_OBJECT (combo_box), "active-id");

  return match;
}

GtkWidget *
gtk_combo_box_get_popup (GtkComboBox *combo_box)
{
  GtkComboBoxPrivate *priv = gtk_combo_box_get_instance_private (combo_box);

  return priv->popup_widget;
}
