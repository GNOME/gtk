/*
 * Copyright (C) 2012 Alexander Larsson <alexl@redhat.com>
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

#include "gtklistbox.h"

#include "gtkactionhelperprivate.h"
#include "gtkadjustmentprivate.h"
#include "gtkbindings.h"
#include "gtkbuildable.h"
#include "gtkcontainerprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkdnd.h"
#include "gtkgestureclick.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkscrollable.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtklistboxaccessibleprivate.h"
#include "a11y/gtklistboxrowaccessible.h"

#include <float.h>
#include <math.h>
#include <string.h>

/**
 * SECTION:gtklistbox
 * @Short_description: A list container
 * @Title: GtkListBox
 * @See_also: #GtkScrolledWindow
 *
 * A GtkListBox is a vertical container that contains GtkListBoxRow
 * children. These rows can by dynamically sorted and filtered, and
 * headers can be added dynamically depending on the row content.
 * It also allows keyboard and mouse navigation and selection like
 * a typical list.
 *
 * Using GtkListBox is often an alternative to #GtkTreeView, especially
 * when the list contents has a more complicated layout than what is allowed
 * by a #GtkCellRenderer, or when the contents is interactive (i.e. has a
 * button in it).
 *
 * Although a #GtkListBox must have only #GtkListBoxRow children you can
 * add any kind of widget to it via gtk_container_add(), and a #GtkListBoxRow
 * widget will automatically be inserted between the list and the widget.
 *
 * #GtkListBoxRows can be marked as activatable or selectable. If a row
 * is activatable, #GtkListBox::row-activated will be emitted for it when
 * the user tries to activate it. If it is selectable, the row will be marked
 * as selected when the user tries to select it.
 *
 * The GtkListBox widget was added in GTK+ 3.10.
 *
 * # GtkListBox as GtkBuildable
 *
 * The GtkListBox implementation of the #GtkBuildable interface supports
 * setting a child as the placeholder by specifying “placeholder” as the “type”
 * attribute of a <child> element. See gtk_list_box_set_placeholder() for info.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * list[.separators]
 * ╰── row[.activatable]
 * ]|
 *
 * GtkListBox uses a single CSS node named list. It may carry the .separators style
 * class, when the #GtkListBox::show-separators property is set. Each GtkListBoxRow uses
 * a single CSS node named row. The row nodes get the .activatable
 * style class added when appropriate.
 */

typedef struct _GtkListBoxClass   GtkListBoxClass;

struct _GtkListBox
{
  GtkContainer parent_instance;
};

struct _GtkListBoxClass
{
  GtkContainerClass parent_class;

  void (*row_selected)        (GtkListBox      *box,
                               GtkListBoxRow   *row);
  void (*row_activated)       (GtkListBox      *box,
                               GtkListBoxRow   *row);
  void (*activate_cursor_row) (GtkListBox      *box);
  void (*toggle_cursor_row)   (GtkListBox      *box);
  void (*move_cursor)         (GtkListBox      *box,
                               GtkMovementStep  step,
                               gint             count);
  void (*selected_rows_changed) (GtkListBox    *box);
  void (*select_all)            (GtkListBox    *box);
  void (*unselect_all)          (GtkListBox    *box);
};

typedef struct
{
  GSequence *children;
  GHashTable *header_hash;

  GtkWidget *placeholder;

  GtkListBoxSortFunc sort_func;
  gpointer sort_func_target;
  GDestroyNotify sort_func_target_destroy_notify;

  GtkListBoxFilterFunc filter_func;
  gpointer filter_func_target;
  GDestroyNotify filter_func_target_destroy_notify;

  GtkListBoxUpdateHeaderFunc update_header_func;
  gpointer update_header_func_target;
  GDestroyNotify update_header_func_target_destroy_notify;

  GtkListBoxRow *selected_row;
  GtkListBoxRow *cursor_row;

  GtkListBoxRow *active_row;

  GtkSelectionMode selection_mode;

  gulong adjustment_changed_id;
  GtkWidget *scrollable_parent;
  GtkAdjustment *adjustment;
  gboolean activate_single_click;
  gboolean accept_unpaired_release;
  gboolean show_separators;

  /* DnD */
  GtkListBoxRow *drag_highlighted_row;

  int n_visible_rows;

  GListModel *bound_model;
  GtkListBoxCreateWidgetFunc create_widget_func;
  gpointer create_widget_func_data;
  GDestroyNotify create_widget_func_data_destroy;
} GtkListBoxPrivate;

typedef struct
{
  GSequenceIter *iter;
  GtkWidget *header;
  GtkActionHelper *action_helper;
  gint y;
  gint height;
  guint visible     :1;
  guint selected    :1;
  guint activatable :1;
  guint selectable  :1;
} GtkListBoxRowPrivate;

enum {
  ROW_SELECTED,
  ROW_ACTIVATED,
  ACTIVATE_CURSOR_ROW,
  TOGGLE_CURSOR_ROW,
  MOVE_CURSOR,
  SELECTED_ROWS_CHANGED,
  SELECT_ALL,
  UNSELECT_ALL,
  LAST_SIGNAL
};

enum {
  ROW__ACTIVATE,
  ROW__LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SELECTION_MODE,
  PROP_ACTIVATE_ON_SINGLE_CLICK,
  PROP_ACCEPT_UNPAIRED_RELEASE,
  PROP_SHOW_SEPARATORS,
  LAST_PROPERTY
};

enum {
  ROW_PROP_0,
  ROW_PROP_ACTIVATABLE,
  ROW_PROP_SELECTABLE,

  /* actionable properties */
  ROW_PROP_ACTION_NAME,
  ROW_PROP_ACTION_TARGET,

  LAST_ROW_PROPERTY = ROW_PROP_ACTION_NAME
};

#define BOX_PRIV(box) ((GtkListBoxPrivate*)gtk_list_box_get_instance_private ((GtkListBox*)(box)))
#define ROW_PRIV(row) ((GtkListBoxRowPrivate*)gtk_list_box_row_get_instance_private ((GtkListBoxRow*)(row)))

static GtkBuildableIface *parent_buildable_iface;

static void     gtk_list_box_buildable_interface_init   (GtkBuildableIface *iface);

static void     gtk_list_box_row_actionable_iface_init  (GtkActionableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkListBox, gtk_list_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkListBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_list_box_buildable_interface_init))
G_DEFINE_TYPE_WITH_CODE (GtkListBoxRow, gtk_list_box_row, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkListBoxRow)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_list_box_row_actionable_iface_init))

static void                 gtk_list_box_apply_filter_all             (GtkListBox          *box);
static void                 gtk_list_box_update_header                (GtkListBox          *box,
                                                                       GSequenceIter       *iter);
static GSequenceIter *      gtk_list_box_get_next_visible             (GtkListBox          *box,
                                                                       GSequenceIter       *iter);
static void                 gtk_list_box_apply_filter                 (GtkListBox          *box,
                                                                       GtkListBoxRow       *row);
static void                 gtk_list_box_add_move_binding             (GtkBindingSet       *binding_set,
                                                                       guint                keyval,
                                                                       GdkModifierType      modmask,
                                                                       GtkMovementStep      step,
                                                                       gint                 count);
static void                 gtk_list_box_update_cursor                (GtkListBox          *box,
                                                                       GtkListBoxRow       *row,
                                                                       gboolean             grab_focus);
static void                 gtk_list_box_show                         (GtkWidget           *widget);
static gboolean             gtk_list_box_focus                        (GtkWidget           *widget,
                                                                       GtkDirectionType     direction);
static GSequenceIter*       gtk_list_box_get_previous_visible         (GtkListBox          *box,
                                                                       GSequenceIter       *iter);
static GtkListBoxRow       *gtk_list_box_get_first_focusable          (GtkListBox          *box);
static GtkListBoxRow       *gtk_list_box_get_last_focusable           (GtkListBox          *box);
static void                 gtk_list_box_add                          (GtkContainer        *container,
                                                                       GtkWidget           *widget);
static void                 gtk_list_box_remove                       (GtkContainer        *container,
                                                                       GtkWidget           *widget);
static void                 gtk_list_box_forall                       (GtkContainer        *container,
                                                                       GtkCallback          callback,
                                                                       gpointer             callback_target);
static void                 gtk_list_box_compute_expand               (GtkWidget           *widget,
                                                                       gboolean            *hexpand,
                                                                       gboolean            *vexpand);
static GType                gtk_list_box_child_type                   (GtkContainer        *container);
static GtkSizeRequestMode   gtk_list_box_get_request_mode             (GtkWidget           *widget);
static void                 gtk_list_box_size_allocate                (GtkWidget           *widget,
                                                                       int                  width,
                                                                       int                  height,
                                                                       int                  baseline);
static void                 gtk_list_box_drag_leave                   (GtkWidget           *widget,
                                                                       GdkDrop             *drop);
static void                 gtk_list_box_activate_cursor_row          (GtkListBox          *box);
static void                 gtk_list_box_toggle_cursor_row            (GtkListBox          *box);
static void                 gtk_list_box_move_cursor                  (GtkListBox          *box,
                                                                       GtkMovementStep      step,
                                                                       gint                 count);
static void                 gtk_list_box_finalize                     (GObject             *obj);
static void                 gtk_list_box_parent_cb                    (GObject             *object,
                                                                       GParamSpec          *pspec,
                                                                       gpointer             user_data);
static void                 gtk_list_box_select_row_internal            (GtkListBox          *box,
                                                                         GtkListBoxRow       *row);
static void                 gtk_list_box_unselect_row_internal          (GtkListBox          *box,
                                                                         GtkListBoxRow       *row);
static void                 gtk_list_box_select_all_between             (GtkListBox          *box,
                                                                         GtkListBoxRow       *row1,
                                                                         GtkListBoxRow       *row2,
                                                                         gboolean             modify);
static gboolean             gtk_list_box_unselect_all_internal          (GtkListBox          *box);
static void                 gtk_list_box_selected_rows_changed          (GtkListBox          *box);
static void                 gtk_list_box_set_accept_unpaired_release    (GtkListBox          *box,
                                                                         gboolean             accept);

static void gtk_list_box_click_gesture_pressed  (GtkGestureClick  *gesture,
                                                 guint             n_press,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 GtkListBox       *box);
static void gtk_list_box_click_gesture_released (GtkGestureClick  *gesture,
                                                 guint             n_press,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 GtkListBox       *box);
static void gtk_list_box_click_unpaired_release (GtkGestureClick  *gesture,
                                                 gdouble           x,
                                                 gdouble           y,
                                                 guint             button,
                                                 GdkEventSequence *sequence,
                                                 GtkListBox       *box);
static void gtk_list_box_click_gesture_stopped  (GtkGestureClick  *gesture,
                                                 GtkListBox       *box);

static void gtk_list_box_update_row_styles (GtkListBox    *box);
static void gtk_list_box_update_row_style  (GtkListBox    *box,
                                            GtkListBoxRow *row);

static void                 gtk_list_box_bound_model_changed            (GListModel          *list,
                                                                         guint                position,
                                                                         guint                removed,
                                                                         guint                added,
                                                                         gpointer             user_data);

static void                 gtk_list_box_check_model_compat             (GtkListBox          *box);

static void gtk_list_box_measure (GtkWidget     *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);



static GParamSpec *properties[LAST_PROPERTY] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *row_properties[LAST_ROW_PROPERTY] = { NULL, };
static guint row_signals[ROW__LAST_SIGNAL] = { 0 };

/**
 * gtk_list_box_new:
 *
 * Creates a new #GtkListBox container.
 *
 * Returns: a new #GtkListBox
 */
GtkWidget *
gtk_list_box_new (void)
{
  return g_object_new (GTK_TYPE_LIST_BOX, NULL);
}

static void
gtk_list_box_get_property (GObject    *obj,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkListBoxPrivate *priv = BOX_PRIV (obj);

  switch (property_id)
    {
    case PROP_SELECTION_MODE:
      g_value_set_enum (value, priv->selection_mode);
      break;
    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      g_value_set_boolean (value, priv->activate_single_click);
      break;
    case PROP_ACCEPT_UNPAIRED_RELEASE:
      g_value_set_boolean (value, priv->accept_unpaired_release);
      break;
    case PROP_SHOW_SEPARATORS:
      g_value_set_boolean (value, priv->show_separators);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_list_box_set_property (GObject      *obj,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkListBox *box = GTK_LIST_BOX (obj);

  switch (property_id)
    {
    case PROP_SELECTION_MODE:
      gtk_list_box_set_selection_mode (box, g_value_get_enum (value));
      break;
    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      gtk_list_box_set_activate_on_single_click (box, g_value_get_boolean (value));
      break;
    case PROP_ACCEPT_UNPAIRED_RELEASE:
      gtk_list_box_set_accept_unpaired_release (box, g_value_get_boolean (value));
      break;
    case PROP_SHOW_SEPARATORS:
      gtk_list_box_set_show_separators (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_list_box_finalize (GObject *obj)
{
  GtkListBoxPrivate *priv = BOX_PRIV (obj);

  if (priv->sort_func_target_destroy_notify != NULL)
    priv->sort_func_target_destroy_notify (priv->sort_func_target);
  if (priv->filter_func_target_destroy_notify != NULL)
    priv->filter_func_target_destroy_notify (priv->filter_func_target);
  if (priv->update_header_func_target_destroy_notify != NULL)
    priv->update_header_func_target_destroy_notify (priv->update_header_func_target);

  g_clear_object (&priv->adjustment);
  g_clear_object (&priv->drag_highlighted_row);

  g_sequence_free (priv->children);
  g_hash_table_unref (priv->header_hash);

  if (priv->bound_model)
    {
      if (priv->create_widget_func_data_destroy)
        priv->create_widget_func_data_destroy (priv->create_widget_func_data);

      g_signal_handlers_disconnect_by_func (priv->bound_model, gtk_list_box_bound_model_changed, obj);
      g_clear_object (&priv->bound_model);
    }

  G_OBJECT_CLASS (gtk_list_box_parent_class)->finalize (obj);
}

static void
gtk_list_box_class_init (GtkListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_LIST_BOX_ACCESSIBLE);

  object_class->get_property = gtk_list_box_get_property;
  object_class->set_property = gtk_list_box_set_property;
  object_class->finalize = gtk_list_box_finalize;
  widget_class->show = gtk_list_box_show;
  widget_class->focus = gtk_list_box_focus;
  widget_class->compute_expand = gtk_list_box_compute_expand;
  widget_class->get_request_mode = gtk_list_box_get_request_mode;
  widget_class->measure = gtk_list_box_measure;
  widget_class->size_allocate = gtk_list_box_size_allocate;
  widget_class->drag_leave = gtk_list_box_drag_leave;
  container_class->add = gtk_list_box_add;
  container_class->remove = gtk_list_box_remove;
  container_class->forall = gtk_list_box_forall;
  container_class->child_type = gtk_list_box_child_type;
  klass->activate_cursor_row = gtk_list_box_activate_cursor_row;
  klass->toggle_cursor_row = gtk_list_box_toggle_cursor_row;
  klass->move_cursor = gtk_list_box_move_cursor;
  klass->select_all = gtk_list_box_select_all;
  klass->unselect_all = gtk_list_box_unselect_all;
  klass->selected_rows_changed = gtk_list_box_selected_rows_changed;

  properties[PROP_SELECTION_MODE] =
    g_param_spec_enum ("selection-mode",
                       P_("Selection mode"),
                       P_("The selection mode"),
                       GTK_TYPE_SELECTION_MODE,
                       GTK_SELECTION_SINGLE,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ACTIVATE_ON_SINGLE_CLICK] =
    g_param_spec_boolean ("activate-on-single-click",
                          P_("Activate on Single Click"),
                          P_("Activate row on a single click"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_ACCEPT_UNPAIRED_RELEASE] =
    g_param_spec_boolean ("accept-unpaired-release",
                          P_("Accept unpaired release"),
                          P_("Accept unpaired release"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_SEPARATORS] =
    g_param_spec_boolean ("show-separators",
                          P_("Show separators"),
                          P_("Show separators between rows"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);

  /**
   * GtkListBox::row-selected:
   * @box: the #GtkListBox
   * @row: (nullable): the selected row
   *
   * The ::row-selected signal is emitted when a new row is selected, or
   * (with a %NULL @row) when the selection is cleared.
   *
   * When the @box is using #GTK_SELECTION_MULTIPLE, this signal will not
   * give you the full picture of selection changes, and you should use
   * the #GtkListBox::selected-rows-changed signal instead.
   */
  signals[ROW_SELECTED] =
    g_signal_new (I_("row-selected"),
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkListBoxClass, row_selected),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_LIST_BOX_ROW);

  /**
   * GtkListBox::selected-rows-changed:
   * @box: the #GtkListBox on wich the signal is emitted
   *
   * The ::selected-rows-changed signal is emitted when the
   * set of selected rows changes.
   */
  signals[SELECTED_ROWS_CHANGED] = g_signal_new (I_("selected-rows-changed"),
                                                 GTK_TYPE_LIST_BOX,
                                                 G_SIGNAL_RUN_FIRST,
                                                 G_STRUCT_OFFSET (GtkListBoxClass, selected_rows_changed),
                                                 NULL, NULL,
                                                 NULL,
                                                 G_TYPE_NONE, 0);

  /**
   * GtkListBox::select-all:
   * @box: the #GtkListBox on which the signal is emitted
   *
   * The ::select-all signal is a [keybinding signal][GtkBindingSignal]
   * which gets emitted to select all children of the box, if the selection
   * mode permits it.
   *
   * The default bindings for this signal is Ctrl-a.
   */
  signals[SELECT_ALL] = g_signal_new (I_("select-all"),
                                      GTK_TYPE_LIST_BOX,
                                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                      G_STRUCT_OFFSET (GtkListBoxClass, select_all),
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 0);

  /**
   * GtkListBox::unselect-all:
   * @box: the #GtkListBox on which the signal is emitted
   * 
   * The ::unselect-all signal is a [keybinding signal][GtkBindingSignal]
   * which gets emitted to unselect all children of the box, if the selection
   * mode permits it.
   *
   * The default bindings for this signal is Ctrl-Shift-a.
   */
  signals[UNSELECT_ALL] = g_signal_new (I_("unselect-all"),
                                        GTK_TYPE_LIST_BOX,
                                        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                        G_STRUCT_OFFSET (GtkListBoxClass, unselect_all),
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);

  /**
   * GtkListBox::row-activated:
   * @box: the #GtkListBox
   * @row: the activated row
   *
   * The ::row-activated signal is emitted when a row has been activated by the user.
   */
  signals[ROW_ACTIVATED] =
    g_signal_new (I_("row-activated"),
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkListBoxClass, row_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_LIST_BOX_ROW);
  signals[ACTIVATE_CURSOR_ROW] =
    g_signal_new (I_("activate-cursor-row"),
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxClass, activate_cursor_row),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  signals[TOGGLE_CURSOR_ROW] =
    g_signal_new (I_("toggle-cursor-row"),
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxClass, toggle_cursor_row),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxClass, move_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_MOVEMENT_STEP, G_TYPE_INT);
  g_signal_set_va_marshaller (signals[MOVE_CURSOR],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__ENUM_INTv);

  widget_class->activate_signal = signals[ACTIVATE_CURSOR_ROW];

  binding_set = gtk_binding_set_by_class (klass);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Home, 0,
                                 GTK_MOVEMENT_BUFFER_ENDS, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Home, 0,
                                 GTK_MOVEMENT_BUFFER_ENDS, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_End, 0,
                                 GTK_MOVEMENT_BUFFER_ENDS, 1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_End, 0,
                                 GTK_MOVEMENT_BUFFER_ENDS, 1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Up, 0,
                                 GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Up, 0,
                                 GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Down, 0,
                                 GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Down, 0,
                                 GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Page_Up, 0,
                                 GTK_MOVEMENT_PAGES, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Page_Up, 0,
                                 GTK_MOVEMENT_PAGES, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Page_Down, 0,
                                 GTK_MOVEMENT_PAGES, 1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Page_Down, 0,
                                 GTK_MOVEMENT_PAGES, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK,
                                "toggle-cursor-row", 0, NULL);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                "toggle-cursor-row", 0, NULL);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                "select-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "unselect-all", 0);

  gtk_widget_class_set_css_name (widget_class, I_("list"));
}

static void
gtk_list_box_init (GtkListBox *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkWidget *widget = GTK_WIDGET (box);
  GtkGesture *gesture;

  priv->selection_mode = GTK_SELECTION_SINGLE;
  priv->activate_single_click = TRUE;

  priv->children = g_sequence_new (NULL);
  priv->header_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_list_box_click_gesture_pressed), box);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_list_box_click_gesture_released), box);
  g_signal_connect (gesture, "stopped",
                    G_CALLBACK (gtk_list_box_click_gesture_stopped), box);
  g_signal_connect (gesture, "unpaired-release",
                    G_CALLBACK (gtk_list_box_click_unpaired_release), box);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

  g_signal_connect (box, "notify::parent", G_CALLBACK (gtk_list_box_parent_cb), NULL);
}

/**
 * gtk_list_box_get_selected_row:
 * @box: a #GtkListBox
 *
 * Gets the selected row.
 *
 * Note that the box may allow multiple selection, in which
 * case you should use gtk_list_box_selected_foreach() to
 * find all selected rows.
 *
 * Returns: (transfer none): the selected row
 */
GtkListBoxRow *
gtk_list_box_get_selected_row (GtkListBox *box)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX (box), NULL);

  return BOX_PRIV (box)->selected_row;
}

/**
 * gtk_list_box_get_row_at_index:
 * @box: a #GtkListBox
 * @index_: the index of the row
 *
 * Gets the n-th child in the list (not counting headers).
 * If @_index is negative or larger than the number of items in the
 * list, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): the child #GtkWidget or %NULL
 */
GtkListBoxRow *
gtk_list_box_get_row_at_index (GtkListBox *box,
                               gint        index_)
{
  GSequenceIter *iter;

  g_return_val_if_fail (GTK_IS_LIST_BOX (box), NULL);

  iter = g_sequence_get_iter_at_pos (BOX_PRIV (box)->children, index_);
  if (!g_sequence_iter_is_end (iter))
    return g_sequence_get (iter);

  return NULL;
}

static int
row_y_cmp_func (gconstpointer a,
                gconstpointer b,
                gpointer      user_data)
{
  int y = GPOINTER_TO_INT (b);
  GtkListBoxRowPrivate *row_priv = ROW_PRIV (a);


  if (y < row_priv->y)
    return 1;
  else if (y >= row_priv->y + row_priv->height)
    return -1;

  return 0;
}

/**
 * gtk_list_box_get_row_at_y:
 * @box: a #GtkListBox
 * @y: position
 *
 * Gets the row at the @y position.
 *
 * Returns: (transfer none) (nullable): the row or %NULL
 *   in case no row exists for the given y coordinate.
 */
GtkListBoxRow *
gtk_list_box_get_row_at_y (GtkListBox *box,
                           gint        y)
{
  GSequenceIter *iter;

  g_return_val_if_fail (GTK_IS_LIST_BOX (box), NULL);

  iter = g_sequence_lookup (BOX_PRIV (box)->children,
                            GINT_TO_POINTER (y),
                            row_y_cmp_func,
                            NULL);

  if (iter)
    return GTK_LIST_BOX_ROW (g_sequence_get (iter));

  return NULL;
}

/**
 * gtk_list_box_select_row:
 * @box: a #GtkListBox
 * @row: (allow-none): The row to select or %NULL
 *
 * Make @row the currently selected row.
 */
void
gtk_list_box_select_row (GtkListBox    *box,
                         GtkListBoxRow *row)
{
  gboolean dirty = FALSE;

  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (row == NULL || GTK_IS_LIST_BOX_ROW (row));

  if (row)
    gtk_list_box_select_row_internal (box, row);
  else
    dirty = gtk_list_box_unselect_all_internal (box);

  if (dirty)
    {
      g_signal_emit (box, signals[ROW_SELECTED], 0, NULL);
      g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
    }
}

/**
 * gtk_list_box_unselect_row:
 * @box: a #GtkListBox
 * @row: the row to unselected
 *
 * Unselects a single row of @box, if the selection mode allows it.
 */
void
gtk_list_box_unselect_row (GtkListBox    *box,
                           GtkListBoxRow *row)
{
  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));
  
  gtk_list_box_unselect_row_internal (box, row);
} 

/**
 * gtk_list_box_select_all:
 * @box: a #GtkListBox
 *
 * Select all children of @box, if the selection mode allows it.
 */
void
gtk_list_box_select_all (GtkListBox *box)
{
  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (BOX_PRIV (box)->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  if (g_sequence_get_length (BOX_PRIV (box)->children) > 0)
    {
      gtk_list_box_select_all_between (box, NULL, NULL, FALSE);
      g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
    }
}

/**
 * gtk_list_box_unselect_all:
 * @box: a #GtkListBox
 *
 * Unselect all children of @box, if the selection mode allows it.
 */
void
gtk_list_box_unselect_all (GtkListBox *box)
{
  gboolean dirty = FALSE;

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (BOX_PRIV (box)->selection_mode == GTK_SELECTION_BROWSE)
    return;

  dirty = gtk_list_box_unselect_all_internal (box);

  if (dirty)
    {
      g_signal_emit (box, signals[ROW_SELECTED], 0, NULL);
      g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
    }
}

static void
gtk_list_box_selected_rows_changed (GtkListBox *box)
{
  _gtk_list_box_accessible_selection_changed (box);
}

/**
 * GtkListBoxForeachFunc:
 * @box: a #GtkListBox
 * @row: a #GtkListBoxRow
 * @user_data: (closure): user data
 *
 * A function used by gtk_list_box_selected_foreach().
 * It will be called on every selected child of the @box.
 */

/**
 * gtk_list_box_selected_foreach:
 * @box: a #GtkListBox
 * @func: (scope call): the function to call for each selected child
 * @data: user data to pass to the function
 *
 * Calls a function for each selected child.
 *
 * Note that the selection cannot be modified from within this function.
 */
void
gtk_list_box_selected_foreach (GtkListBox            *box,
                               GtkListBoxForeachFunc  func,
                               gpointer               data)
{
  GtkListBoxRow *row;
  GSequenceIter *iter;

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      if (gtk_list_box_row_is_selected (row))
        (*func) (box, row, data);
    }
}

/**
 * gtk_list_box_get_selected_rows:
 * @box: a #GtkListBox
 *
 * Creates a list of all selected children.
 *
 * Returns: (element-type GtkListBoxRow) (transfer container):
 *     A #GList containing the #GtkWidget for each selected child.
 *     Free with g_list_free() when done.
 */
GList *
gtk_list_box_get_selected_rows (GtkListBox *box)
{
  GtkListBoxRow *row;
  GSequenceIter *iter;
  GList *selected = NULL;

  g_return_val_if_fail (GTK_IS_LIST_BOX (box), NULL);

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      if (gtk_list_box_row_is_selected (row))
        selected = g_list_prepend (selected, row);
    }

  return g_list_reverse (selected);
}

/**
 * gtk_list_box_set_placeholder:
 * @box: a #GtkListBox
 * @placeholder: (allow-none): a #GtkWidget or %NULL
 *
 * Sets the placeholder widget that is shown in the list when
 * it doesn't display any visible children.
 */
void
gtk_list_box_set_placeholder (GtkListBox *box,
                              GtkWidget  *placeholder)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->placeholder)
    {
      gtk_widget_unparent (priv->placeholder);
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }

  priv->placeholder = placeholder;

  if (placeholder)
    {
      gtk_widget_set_parent (placeholder, GTK_WIDGET (box));
      gtk_widget_set_child_visible (placeholder,
                                    priv->n_visible_rows == 0);
    }
}


/**
 * gtk_list_box_set_adjustment:
 * @box: a #GtkListBox
 * @adjustment: (allow-none): the adjustment, or %NULL
 *
 * Sets the adjustment (if any) that the widget uses to
 * for vertical scrolling. For instance, this is used
 * to get the page size for PageUp/Down key handling.
 *
 * In the normal case when the @box is packed inside
 * a #GtkScrolledWindow the adjustment from that will
 * be picked up automatically, so there is no need
 * to manually do that.
 */
void
gtk_list_box_set_adjustment (GtkListBox    *box,
                             GtkAdjustment *adjustment)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    g_object_ref_sink (adjustment);
  if (priv->adjustment)
    g_object_unref (priv->adjustment);
  priv->adjustment = adjustment;
}

/**
 * gtk_list_box_get_adjustment:
 * @box: a #GtkListBox
 *
 * Gets the adjustment (if any) that the widget uses to
 * for vertical scrolling.
 *
 * Returns: (transfer none): the adjustment
 */
GtkAdjustment *
gtk_list_box_get_adjustment (GtkListBox *box)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX (box), NULL);

  return BOX_PRIV (box)->adjustment;
}

static void
adjustment_changed (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    data)
{
  GtkAdjustment *adjustment;

  adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (object));
  gtk_list_box_set_adjustment (GTK_LIST_BOX (data), adjustment);
}

static void
gtk_list_box_parent_cb (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GtkListBoxPrivate *priv = BOX_PRIV (object);
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (object));

  if (priv->adjustment_changed_id != 0 &&
      priv->scrollable_parent != NULL)
    {
      g_signal_handler_disconnect (priv->scrollable_parent,
                                   priv->adjustment_changed_id);
    }

  if (parent && GTK_IS_SCROLLABLE (parent))
    {
      adjustment_changed (G_OBJECT (parent), NULL, object);
      priv->scrollable_parent = parent;
      priv->adjustment_changed_id = g_signal_connect (parent, "notify::vadjustment",
                                                      G_CALLBACK (adjustment_changed), object);
    }
  else
    {
      gtk_list_box_set_adjustment (GTK_LIST_BOX (object), NULL);
      priv->adjustment_changed_id = 0;
      priv->scrollable_parent = NULL;
    }
}

/**
 * gtk_list_box_set_selection_mode:
 * @box: a #GtkListBox
 * @mode: The #GtkSelectionMode
 *
 * Sets how selection works in the listbox.
 * See #GtkSelectionMode for details.
 */
void
gtk_list_box_set_selection_mode (GtkListBox       *box,
                                 GtkSelectionMode  mode)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  gboolean dirty = FALSE;

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->selection_mode == mode)
    return;

  if (mode == GTK_SELECTION_NONE ||
      priv->selection_mode == GTK_SELECTION_MULTIPLE)
    dirty = gtk_list_box_unselect_all_internal (box);

  priv->selection_mode = mode;

  gtk_list_box_update_row_styles (box);

  g_object_notify_by_pspec (G_OBJECT (box), properties[PROP_SELECTION_MODE]);

  if (dirty)
    {
      g_signal_emit (box, signals[ROW_SELECTED], 0, NULL);
      g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
    }
}

/**
 * gtk_list_box_get_selection_mode:
 * @box: a #GtkListBox
 *
 * Gets the selection mode of the listbox.
 *
 * Returns: a #GtkSelectionMode
 */
GtkSelectionMode
gtk_list_box_get_selection_mode (GtkListBox *box)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX (box), GTK_SELECTION_NONE);

  return BOX_PRIV (box)->selection_mode;
}

/**
 * gtk_list_box_set_filter_func:
 * @box: a #GtkListBox
 * @filter_func: (closure user_data) (allow-none): callback that lets you filter which rows to show
 * @user_data: user data passed to @filter_func
 * @destroy: destroy notifier for @user_data
 *
 * By setting a filter function on the @box one can decide dynamically which
 * of the rows to show. For instance, to implement a search function on a list that
 * filters the original list to only show the matching rows.
 *
 * The @filter_func will be called for each row after the call, and it will
 * continue to be called each time a row changes (via gtk_list_box_row_changed()) or
 * when gtk_list_box_invalidate_filter() is called.
 *
 * Note that using a filter function is incompatible with using a model
 * (see gtk_list_box_bind_model()).
 */
void
gtk_list_box_set_filter_func (GtkListBox           *box,
                              GtkListBoxFilterFunc  filter_func,
                              gpointer              user_data,
                              GDestroyNotify        destroy)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->filter_func_target_destroy_notify != NULL)
    priv->filter_func_target_destroy_notify (priv->filter_func_target);

  priv->filter_func = filter_func;
  priv->filter_func_target = user_data;
  priv->filter_func_target_destroy_notify = destroy;

  gtk_list_box_check_model_compat (box);

  gtk_list_box_invalidate_filter (box);
}

/**
 * gtk_list_box_set_header_func:
 * @box: a #GtkListBox
 * @update_header: (closure user_data) (allow-none): callback that lets you add row headers
 * @user_data: user data passed to @update_header
 * @destroy: destroy notifier for @user_data
 *
 * By setting a header function on the @box one can dynamically add headers
 * in front of rows, depending on the contents of the row and its position in the list.
 * For instance, one could use it to add headers in front of the first item of a
 * new kind, in a list sorted by the kind.
 *
 * The @update_header can look at the current header widget using gtk_list_box_row_get_header()
 * and either update the state of the widget as needed, or set a new one using
 * gtk_list_box_row_set_header(). If no header is needed, set the header to %NULL.
 *
 * Note that you may get many calls @update_header to this for a particular row when e.g.
 * changing things that don’t affect the header. In this case it is important for performance
 * to not blindly replace an existing header with an identical one.
 *
 * The @update_header function will be called for each row after the call, and it will
 * continue to be called each time a row changes (via gtk_list_box_row_changed()) and when
 * the row before changes (either by gtk_list_box_row_changed() on the previous row, or when
 * the previous row becomes a different row). It is also called for all rows when
 * gtk_list_box_invalidate_headers() is called.
 */
void
gtk_list_box_set_header_func (GtkListBox                 *box,
                              GtkListBoxUpdateHeaderFunc  update_header,
                              gpointer                    user_data,
                              GDestroyNotify              destroy)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->update_header_func_target_destroy_notify != NULL)
    priv->update_header_func_target_destroy_notify (priv->update_header_func_target);

  priv->update_header_func = update_header;
  priv->update_header_func_target = user_data;
  priv->update_header_func_target_destroy_notify = destroy;
  gtk_list_box_invalidate_headers (box);
}

/**
 * gtk_list_box_invalidate_filter:
 * @box: a #GtkListBox
 *
 * Update the filtering for all rows. Call this when result
 * of the filter function on the @box is changed due
 * to an external factor. For instance, this would be used
 * if the filter function just looked for a specific search
 * string and the entry with the search string has changed.
 */
void
gtk_list_box_invalidate_filter (GtkListBox *box)
{
  g_return_if_fail (GTK_IS_LIST_BOX (box));

  gtk_list_box_apply_filter_all (box);
  gtk_list_box_invalidate_headers (box);
  gtk_widget_queue_resize (GTK_WIDGET (box));
}

static gint
do_sort (GtkListBoxRow *a,
         GtkListBoxRow *b,
         GtkListBox    *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  return priv->sort_func (a, b, priv->sort_func_target);
}

static void
gtk_list_box_css_node_foreach (gpointer data,
                               gpointer user_data)
{
  GtkWidget **previous = user_data;
  GtkWidget *row = data;
  GtkCssNode *row_node;
  GtkCssNode *prev_node;

  if (*previous)
    {
      prev_node = gtk_widget_get_css_node (*previous);
      row_node = gtk_widget_get_css_node (row);
      gtk_css_node_insert_after (gtk_css_node_get_parent (row_node),
                                 row_node,
                                 prev_node);
    }

  *previous = row;
}

/**
 * gtk_list_box_invalidate_sort:
 * @box: a #GtkListBox
 *
 * Update the sorting for all rows. Call this when result
 * of the sort function on the @box is changed due
 * to an external factor.
 */
void
gtk_list_box_invalidate_sort (GtkListBox *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkWidget *previous = NULL;

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->sort_func == NULL)
    return;

  g_sequence_sort (priv->children, (GCompareDataFunc)do_sort, box);
  g_sequence_foreach (priv->children, gtk_list_box_css_node_foreach, &previous);

  gtk_list_box_invalidate_headers (box);
  gtk_widget_queue_resize (GTK_WIDGET (box));
}

static void
gtk_list_box_do_reseparate (GtkListBox *box)
{
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    gtk_list_box_update_header (box, iter);

  gtk_widget_queue_resize (GTK_WIDGET (box));
}


/**
 * gtk_list_box_invalidate_headers:
 * @box: a #GtkListBox
 *
 * Update the separators for all rows. Call this when result
 * of the header function on the @box is changed due
 * to an external factor.
 */
void
gtk_list_box_invalidate_headers (GtkListBox *box)
{
  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (!gtk_widget_get_visible (GTK_WIDGET (box)))
    return;

  gtk_list_box_do_reseparate (box);
}

/**
 * gtk_list_box_set_sort_func:
 * @box: a #GtkListBox
 * @sort_func: (closure user_data) (allow-none): the sort function
 * @user_data: user data passed to @sort_func
 * @destroy: destroy notifier for @user_data
 *
 * By setting a sort function on the @box one can dynamically reorder the rows
 * of the list, based on the contents of the rows.
 *
 * The @sort_func will be called for each row after the call, and will continue to
 * be called each time a row changes (via gtk_list_box_row_changed()) and when
 * gtk_list_box_invalidate_sort() is called.
 *
 * Note that using a sort function is incompatible with using a model
 * (see gtk_list_box_bind_model()).
 */
void
gtk_list_box_set_sort_func (GtkListBox         *box,
                            GtkListBoxSortFunc  sort_func,
                            gpointer            user_data,
                            GDestroyNotify      destroy)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->sort_func_target_destroy_notify != NULL)
    priv->sort_func_target_destroy_notify (priv->sort_func_target);

  priv->sort_func = sort_func;
  priv->sort_func_target = user_data;
  priv->sort_func_target_destroy_notify = destroy;

  gtk_list_box_check_model_compat (box);

  gtk_list_box_invalidate_sort (box);
}

static void
gtk_list_box_got_row_changed (GtkListBox    *box,
                              GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkListBoxRowPrivate *row_priv = ROW_PRIV (row);
  GSequenceIter *prev_next, *next;

  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  prev_next = gtk_list_box_get_next_visible (box, row_priv->iter);
  if (priv->sort_func != NULL)
    {
      g_sequence_sort_changed (row_priv->iter,
                               (GCompareDataFunc)do_sort,
                               box);
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
  gtk_list_box_apply_filter (box, row);
  if (gtk_widget_get_visible (GTK_WIDGET (box)))
    {
      next = gtk_list_box_get_next_visible (box, row_priv->iter);
      gtk_list_box_update_header (box, row_priv->iter);
      gtk_list_box_update_header (box, next);
      gtk_list_box_update_header (box, prev_next);
    }
}

/**
 * gtk_list_box_set_activate_on_single_click:
 * @box: a #GtkListBox
 * @single: a boolean
 *
 * If @single is %TRUE, rows will be activated when you click on them,
 * otherwise you need to double-click.
 */
void
gtk_list_box_set_activate_on_single_click (GtkListBox *box,
                                           gboolean    single)
{
  g_return_if_fail (GTK_IS_LIST_BOX (box));

  single = single != FALSE;

  if (BOX_PRIV (box)->activate_single_click == single)
    return;

  BOX_PRIV (box)->activate_single_click = single;

  g_object_notify_by_pspec (G_OBJECT (box), properties[PROP_ACTIVATE_ON_SINGLE_CLICK]);
}

/**
 * gtk_list_box_get_activate_on_single_click:
 * @box: a #GtkListBox
 *
 * Returns whether rows activate on single clicks.
 *
 * Returns: %TRUE if rows are activated on single click, %FALSE otherwise
 */
gboolean
gtk_list_box_get_activate_on_single_click (GtkListBox *box)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX (box), FALSE);

  return BOX_PRIV (box)->activate_single_click;
}

void
gtk_list_box_set_accept_unpaired_release (GtkListBox *box,
                                          gboolean    accept)
{
  if (BOX_PRIV (box)->accept_unpaired_release == accept)
    return;

  BOX_PRIV (box)->accept_unpaired_release = accept;

  g_object_notify_by_pspec (G_OBJECT (box), properties[PROP_ACCEPT_UNPAIRED_RELEASE]);
}

static void
gtk_list_box_add_move_binding (GtkBindingSet   *binding_set,
                               guint            keyval,
                               GdkModifierType  modmask,
                               GtkMovementStep  step,
                               gint             count)
{
  GdkDisplay *display;
  GdkModifierType extend_mod_mask = GDK_SHIFT_MASK;
  GdkModifierType modify_mod_mask = GDK_CONTROL_MASK;

  display = gdk_display_get_default ();
  if (display)
    {
      extend_mod_mask = gdk_keymap_get_modifier_mask (gdk_display_get_keymap (display),
                                                      GDK_MODIFIER_INTENT_EXTEND_SELECTION);
      modify_mod_mask = gdk_keymap_get_modifier_mask (gdk_display_get_keymap (display),
                                                      GDK_MODIFIER_INTENT_MODIFY_SELECTION);
    }

  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, step,
                                G_TYPE_INT, count,
                                NULL);
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | extend_mod_mask,
                                "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, step,
                                G_TYPE_INT, count,
                                NULL);
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | modify_mod_mask,
                                "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, step,
                                G_TYPE_INT, count,
                                NULL);
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | extend_mod_mask | modify_mod_mask,
                                "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, step,
                                G_TYPE_INT, count,
                                NULL);
}

static void
ensure_row_visible (GtkListBox    *box,
                    GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkWidget *header;
  gint y, height;
  graphene_rect_t rect;

  if (!priv->adjustment)
    return;

  if (!gtk_widget_compute_bounds (GTK_WIDGET (row), GTK_WIDGET (box), &rect))
    return;

  y = rect.origin.y;
  height = rect.size.height;

  /* If the row has a header, we want to ensure that it is visible as well. */
  header = ROW_PRIV (row)->header;
  if (GTK_IS_WIDGET (header) && gtk_widget_is_drawable (header))
    {
      if (gtk_widget_compute_bounds (header, GTK_WIDGET (box), &rect))
        {
          y = rect.origin.y;
          height += rect.size.height;
        }
    }

  gtk_adjustment_clamp_page (priv->adjustment, y, y + height);
}

static void
gtk_list_box_update_cursor (GtkListBox    *box,
                            GtkListBoxRow *row,
                            gboolean grab_focus)
{
  BOX_PRIV (box)->cursor_row = row;
  ensure_row_visible (box, row);
  if (grab_focus)
    gtk_widget_grab_focus (GTK_WIDGET (row));
  gtk_widget_queue_draw (GTK_WIDGET (row));
  _gtk_list_box_accessible_update_cursor (box, row);
}

static GtkListBox *
gtk_list_box_row_get_box (GtkListBoxRow *row)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (row));
  if (parent && GTK_IS_LIST_BOX (parent))
    return GTK_LIST_BOX (parent);

  return NULL;
}

static gboolean
row_is_visible (GtkListBoxRow *row)
{
  return ROW_PRIV (row)->visible;
}

static gboolean
gtk_list_box_row_set_selected (GtkListBoxRow *row,
                               gboolean       selected)
{
  if (!ROW_PRIV (row)->selectable)
    return FALSE;

  if (ROW_PRIV (row)->selected != selected)
    {
      ROW_PRIV (row)->selected = selected;
      if (selected)
        gtk_widget_set_state_flags (GTK_WIDGET (row),
                                    GTK_STATE_FLAG_SELECTED, FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (row),
                                      GTK_STATE_FLAG_SELECTED);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_list_box_unselect_all_internal (GtkListBox *box)
{
  GtkListBoxRow *row;
  GSequenceIter *iter;
  gboolean dirty = FALSE;

  if (BOX_PRIV (box)->selection_mode == GTK_SELECTION_NONE)
    return FALSE;

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      dirty |= gtk_list_box_row_set_selected (row, FALSE);
    }

  BOX_PRIV (box)->selected_row = NULL;

  return dirty;
}

static void
gtk_list_box_unselect_row_internal (GtkListBox    *box,
                                    GtkListBoxRow *row)
{
  if (!ROW_PRIV (row)->selected)
    return;

  if (BOX_PRIV (box)->selection_mode == GTK_SELECTION_NONE)
    return;
  else if (BOX_PRIV (box)->selection_mode != GTK_SELECTION_MULTIPLE)
    gtk_list_box_unselect_all_internal (box);
  else
    gtk_list_box_row_set_selected (row, FALSE);

  g_signal_emit (box, signals[ROW_SELECTED], 0, NULL);
  g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
}

static void
gtk_list_box_select_row_internal (GtkListBox    *box,
                                  GtkListBoxRow *row)
{
  if (!ROW_PRIV (row)->selectable)
    return;

  if (ROW_PRIV (row)->selected)
    return;

  if (BOX_PRIV (box)->selection_mode == GTK_SELECTION_NONE)
    return;

  if (BOX_PRIV (box)->selection_mode != GTK_SELECTION_MULTIPLE)
    gtk_list_box_unselect_all_internal (box);

  gtk_list_box_row_set_selected (row, TRUE);
  BOX_PRIV (box)->selected_row = row;

  g_signal_emit (box, signals[ROW_SELECTED], 0, row);
  g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
}

static void
gtk_list_box_select_all_between (GtkListBox    *box,
                                 GtkListBoxRow *row1,
                                 GtkListBoxRow *row2,
                                 gboolean       modify)
{
  GSequenceIter *iter, *iter1, *iter2;

  if (row1)
    iter1 = ROW_PRIV (row1)->iter;
  else
    iter1 = g_sequence_get_begin_iter (BOX_PRIV (box)->children);

  if (row2)
    iter2 = ROW_PRIV (row2)->iter;
  else
    iter2 = g_sequence_get_end_iter (BOX_PRIV (box)->children);

  if (g_sequence_iter_compare (iter2, iter1) < 0)
    {
      iter = iter1;
      iter1 = iter2;
      iter2 = iter;
    }

  for (iter = iter1;
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      GtkListBoxRow *row;

      row = GTK_LIST_BOX_ROW (g_sequence_get (iter));
      if (row_is_visible (row))
        {
          if (modify)
            gtk_list_box_row_set_selected (row, !ROW_PRIV (row)->selected);
          else
            gtk_list_box_row_set_selected (row, TRUE);
        }

      if (g_sequence_iter_compare (iter, iter2) == 0)
        break;
    }
}

#define gtk_list_box_update_selection(b,r,m,e) \
  gtk_list_box_update_selection_full((b), (r), (m), (e), TRUE)
static void
gtk_list_box_update_selection_full (GtkListBox    *box,
                                    GtkListBoxRow *row,
                                    gboolean       modify,
                                    gboolean       extend,
                                    gboolean       grab_cursor)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  gtk_list_box_update_cursor (box, row, grab_cursor);

  if (priv->selection_mode == GTK_SELECTION_NONE)
    return;

  if (!ROW_PRIV (row)->selectable)
    return;

  if (priv->selection_mode == GTK_SELECTION_BROWSE)
    {
      gtk_list_box_unselect_all_internal (box);
      gtk_list_box_row_set_selected (row, TRUE);
      priv->selected_row = row;
      g_signal_emit (box, signals[ROW_SELECTED], 0, row);
    }
  else if (priv->selection_mode == GTK_SELECTION_SINGLE)
    {
      gboolean was_selected;

      was_selected = ROW_PRIV (row)->selected;
      gtk_list_box_unselect_all_internal (box);
      gtk_list_box_row_set_selected (row, modify ? !was_selected : TRUE);
      priv->selected_row = ROW_PRIV (row)->selected ? row : NULL;
      g_signal_emit (box, signals[ROW_SELECTED], 0, priv->selected_row);
    }
  else /* GTK_SELECTION_MULTIPLE */
    {
      if (extend)
        {
          GtkListBoxRow *selected_row;

          selected_row = priv->selected_row;

          gtk_list_box_unselect_all_internal (box);

          if (selected_row == NULL)
            {
              gtk_list_box_row_set_selected (row, TRUE);
              priv->selected_row = row;
              g_signal_emit (box, signals[ROW_SELECTED], 0, row);
            }
          else
            gtk_list_box_select_all_between (box, selected_row, row, FALSE);
        }
      else
        {
          if (modify)
            {
              gtk_list_box_row_set_selected (row, !ROW_PRIV (row)->selected);
              g_signal_emit (box, signals[ROW_SELECTED], 0, ROW_PRIV (row)->selected ? row
                                                                                     : NULL);
            }
          else
            {
              gtk_list_box_unselect_all_internal (box);
              gtk_list_box_row_set_selected (row, !ROW_PRIV (row)->selected);
              priv->selected_row = row;
              g_signal_emit (box, signals[ROW_SELECTED], 0, row);
            }
        }
    }

  g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
}

static void
gtk_list_box_activate (GtkListBox    *box,
                       GtkListBoxRow *row)
{
  GtkListBoxRowPrivate *priv = ROW_PRIV (row);

  if (!gtk_list_box_row_get_activatable (row))
    return;

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);
  else
    g_signal_emit (box, signals[ROW_ACTIVATED], 0, row);
}

#define gtk_list_box_select_and_activate(b,r) \
  gtk_list_box_select_and_activate_full ((b), (r), TRUE)
static void
gtk_list_box_select_and_activate_full (GtkListBox    *box,
                                       GtkListBoxRow *row,
                                       gboolean       grab_focus)
{
  if (row != NULL)
    {
      gtk_list_box_select_row_internal (box, row);
      gtk_list_box_update_cursor (box, row, grab_focus);
      gtk_list_box_activate (box, row);
    }
}

static void
gtk_list_box_click_gesture_pressed (GtkGestureClick *gesture,
                                    guint            n_press,
                                    gdouble          x,
                                    gdouble          y,
                                    GtkListBox      *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkListBoxRow *row;

  priv->active_row = NULL;
  row = gtk_list_box_get_row_at_y (box, y);

  if (row != NULL && gtk_widget_is_sensitive (GTK_WIDGET (row)))
    {
      priv->active_row = row;

      if (n_press == 2 && !priv->activate_single_click)
        gtk_list_box_activate (box, row);
    }
}

static void
get_current_selection_modifiers (GtkWidget *widget,
                                 gboolean  *modify,
                                 gboolean  *extend)
{
  GdkModifierType state = 0;
  GdkModifierType mask;

  *modify = FALSE;
  *extend = FALSE;

  if (gtk_get_current_event_state (&state))
    {
      mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);
      if ((state & mask) == mask)
        *modify = TRUE;
      mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);
      if ((state & mask) == mask)
        *extend = TRUE;
    }
}

static void
gtk_list_box_click_unpaired_release (GtkGestureClick  *gesture,
                                     gdouble           x,
                                     gdouble           y,
                                     guint             button,
                                     GdkEventSequence *sequence,
                                     GtkListBox       *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkListBoxRow *row;

  if (!priv->activate_single_click || !priv->accept_unpaired_release)
    return;

  row = gtk_list_box_get_row_at_y (box, y);

  if (row)
    gtk_list_box_select_and_activate (box, row);
}

static void
gtk_list_box_click_gesture_released (GtkGestureClick *gesture,
                                     guint            n_press,
                                     gdouble          x,
                                     gdouble          y,
                                     GtkListBox      *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  /* Take a ref to protect against reentrancy
   * (the activation may destroy the widget)
   */
  g_object_ref (box);

  if (priv->active_row != NULL &&
      priv->active_row == gtk_list_box_get_row_at_y (box, y))
    {
      gboolean focus_on_click = gtk_widget_get_focus_on_click (GTK_WIDGET (priv->active_row));

      if (n_press == 1 && priv->activate_single_click)
        gtk_list_box_select_and_activate_full (box, priv->active_row, focus_on_click);
      else
        {
          GdkEventSequence *sequence;
          GdkInputSource source;
          const GdkEvent *event;
          gboolean modify;
          gboolean extend;

          get_current_selection_modifiers (GTK_WIDGET (box), &modify, &extend);
          /* With touch, we default to modifying the selection.
           * You can still clear the selection and start over
           * by holding Ctrl.
           */
          sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
          event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
          source = gdk_device_get_source (gdk_event_get_source_device (event));

          if (source == GDK_SOURCE_TOUCHSCREEN)
            modify = !modify;

          gtk_list_box_update_selection_full (box, priv->active_row, modify, extend, focus_on_click);
        }
    }

  if (priv->active_row)
    {
      priv->active_row = NULL;
    }

  g_object_unref (box);
}

static void
gtk_list_box_click_gesture_stopped (GtkGestureClick *gesture,
                                         GtkListBox           *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  if (priv->active_row)
    {
      priv->active_row = NULL;
      gtk_widget_queue_draw (GTK_WIDGET (box));
    }
}

static void
gtk_list_box_show (GtkWidget *widget)
{
  gtk_list_box_do_reseparate (GTK_LIST_BOX (widget));

  GTK_WIDGET_CLASS (gtk_list_box_parent_class)->show (widget);
}

static gboolean
gtk_list_box_focus (GtkWidget        *widget,
                    GtkDirectionType  direction)
{
  GtkListBox *box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkWidget *focus_child;
  GtkListBoxRow *next_focus_row;
  GtkWidget *row;
  GtkWidget *header;

  focus_child = gtk_widget_get_focus_child (widget);

  next_focus_row = NULL;
  if (focus_child != NULL)
    {
      GSequenceIter *i;

      if (gtk_widget_child_focus (focus_child, direction))
        return TRUE;

      if (direction == GTK_DIR_UP || direction == GTK_DIR_TAB_BACKWARD)
        {
          if (GTK_IS_LIST_BOX_ROW (focus_child))
            {
              header = ROW_PRIV (GTK_LIST_BOX_ROW (focus_child))->header;
              if (header && gtk_widget_child_focus (header, direction))
                return TRUE;
            }

          if (GTK_IS_LIST_BOX_ROW (focus_child))
            row = focus_child;
          else
            row = g_hash_table_lookup (priv->header_hash, focus_child);

          if (GTK_IS_LIST_BOX_ROW (row))
            i = gtk_list_box_get_previous_visible (box, ROW_PRIV (GTK_LIST_BOX_ROW (row))->iter);
          else
            i = NULL;

          while (i != NULL)
            {
              if (gtk_widget_get_sensitive (g_sequence_get (i)))
                {
                  next_focus_row = g_sequence_get (i);
                  break;
                }

              i = gtk_list_box_get_previous_visible (box, i);
            }
        }
      else if (direction == GTK_DIR_DOWN || direction == GTK_DIR_TAB_FORWARD)
        {
          if (GTK_IS_LIST_BOX_ROW (focus_child))
            i = gtk_list_box_get_next_visible (box, ROW_PRIV (GTK_LIST_BOX_ROW (focus_child))->iter);
          else
            {
              row = g_hash_table_lookup (priv->header_hash, focus_child);
              if (GTK_IS_LIST_BOX_ROW (row))
                i = ROW_PRIV (GTK_LIST_BOX_ROW (row))->iter;
              else
                i = NULL;
            }

          while (!g_sequence_iter_is_end (i))
            {
              if (gtk_widget_get_sensitive (g_sequence_get (i)))
                {
                  next_focus_row = g_sequence_get (i);
                  break;
                }

              i = gtk_list_box_get_next_visible (box, i);
            }
        }
    }
  else
    {
      /* No current focus row */
      switch (direction)
        {
        case GTK_DIR_UP:
        case GTK_DIR_TAB_BACKWARD:
          next_focus_row = priv->selected_row;
          if (next_focus_row == NULL)
            next_focus_row = gtk_list_box_get_last_focusable (box);
          break;
        case GTK_DIR_DOWN:
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_LEFT:
        case GTK_DIR_RIGHT:
        default:
          next_focus_row = priv->selected_row;
          if (next_focus_row == NULL)
            next_focus_row = gtk_list_box_get_first_focusable (box);
          break;
        }
    }

  if (next_focus_row == NULL)
    {
      if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
        {
          if (gtk_widget_keynav_failed (GTK_WIDGET (box), direction))
            return TRUE;
        }

      return FALSE;
    }

  if (direction == GTK_DIR_DOWN || direction == GTK_DIR_TAB_FORWARD)
    {
      header = ROW_PRIV (next_focus_row)->header;
      if (header && gtk_widget_child_focus (header, direction))
        return TRUE;
    }

  if (gtk_widget_child_focus (GTK_WIDGET (next_focus_row), direction))
    return TRUE;

  return FALSE;
}

static void
list_box_add_visible_rows (GtkListBox *box,
                           gint        n)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  int was_zero;

  was_zero = priv->n_visible_rows == 0;
  priv->n_visible_rows += n;

  if (priv->placeholder &&
      (was_zero || priv->n_visible_rows == 0))
    gtk_widget_set_child_visible (GTK_WIDGET (priv->placeholder),
                                  priv->n_visible_rows == 0);
}

/* Children are visible if they are shown by the app (visible)
 * and not filtered out (child_visible) by the listbox
 */
static void
update_row_is_visible (GtkListBox    *box,
                       GtkListBoxRow *row)
{
  GtkListBoxRowPrivate *row_priv = ROW_PRIV (row);
  gboolean was_visible;

  was_visible = row_priv->visible;

  row_priv->visible =
    gtk_widget_get_visible (GTK_WIDGET (row)) &&
    gtk_widget_get_child_visible (GTK_WIDGET (row));

  if (was_visible && !row_priv->visible)
    list_box_add_visible_rows (box, -1);
  if (!was_visible && row_priv->visible)
    list_box_add_visible_rows (box, 1);
}

static void
gtk_list_box_apply_filter (GtkListBox    *box,
                           GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  gboolean do_show;

  do_show = TRUE;
  if (priv->filter_func != NULL)
    do_show = priv->filter_func (row, priv->filter_func_target);

  gtk_widget_set_child_visible (GTK_WIDGET (row), do_show);

  update_row_is_visible (box, row);
}

static void
gtk_list_box_apply_filter_all (GtkListBox *box)
{
  GtkListBoxRow *row;
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      gtk_list_box_apply_filter (box, row);
    }
}

static GtkListBoxRow *
gtk_list_box_get_first_focusable (GtkListBox *box)
{
  GtkListBoxRow *row;
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
        row = g_sequence_get (iter);
        if (row_is_visible (row) && gtk_widget_is_sensitive (GTK_WIDGET (row)))
          return row;
    }

  return NULL;
}

static GtkListBoxRow *
gtk_list_box_get_last_focusable (GtkListBox *box)
{
  GtkListBoxRow *row;
  GSequenceIter *iter;

  iter = g_sequence_get_end_iter (BOX_PRIV (box)->children);
  while (!g_sequence_iter_is_begin (iter))
    {
      iter = g_sequence_iter_prev (iter);
      row = g_sequence_get (iter);
      if (row_is_visible (row) && gtk_widget_is_sensitive (GTK_WIDGET (row)))
        return row;
    }

  return NULL;
}

static GSequenceIter *
gtk_list_box_get_previous_visible (GtkListBox    *box,
                                   GSequenceIter *iter)
{
  GtkListBoxRow *row;

  if (g_sequence_iter_is_begin (iter))
    return NULL;

  do
    {
      iter = g_sequence_iter_prev (iter);
      row = g_sequence_get (iter);
      if (row_is_visible (row))
        return iter;
    }
  while (!g_sequence_iter_is_begin (iter));

  return NULL;
}

static GSequenceIter *
gtk_list_box_get_next_visible (GtkListBox    *box,
                               GSequenceIter *iter)
{
  GtkListBoxRow *row;

  if (g_sequence_iter_is_end (iter))
    return iter;

  do
    {
      iter = g_sequence_iter_next (iter);
      if (!g_sequence_iter_is_end (iter))
        {
        row = g_sequence_get (iter);
        if (row_is_visible (row))
          return iter;
        }
    }
  while (!g_sequence_iter_is_end (iter));

  return iter;
}

static GSequenceIter *
gtk_list_box_get_last_visible (GtkListBox    *box,
                               GSequenceIter *iter)
{
  GSequenceIter *next = NULL;

  if (g_sequence_iter_is_end (iter))
    return NULL;

  do
    {
      next = gtk_list_box_get_next_visible (box, iter);

      if (!g_sequence_iter_is_end (next))
        iter = next;
    }
  while (!g_sequence_iter_is_end (next));

  return iter;
}

static void
gtk_list_box_update_header (GtkListBox    *box,
                            GSequenceIter *iter)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkListBoxRow *row;
  GSequenceIter *before_iter;
  GtkListBoxRow *before_row;
  GtkWidget *old_header, *new_header;

  if (iter == NULL || g_sequence_iter_is_end (iter))
    return;

  row = g_sequence_get (iter);
  g_object_ref (row);

  before_iter = gtk_list_box_get_previous_visible (box, iter);
  before_row = NULL;
  if (before_iter != NULL)
    {
      before_row = g_sequence_get (before_iter);
      if (before_row)
        g_object_ref (before_row);
    }

  if (priv->update_header_func != NULL &&
      row_is_visible (row))
    {
      old_header = ROW_PRIV (row)->header;
      if (old_header)
        g_object_ref (old_header);
      priv->update_header_func (row,
                                before_row,
                                priv->update_header_func_target);
      new_header = ROW_PRIV (row)->header;
      if (old_header != new_header)
        {
          if (old_header != NULL &&
              g_hash_table_lookup (priv->header_hash, old_header) == row)
            {
              /* Only unparent the @old_header if it hasn’t been re-used as the
               * header for a different row. */
              gtk_widget_unparent (old_header);
              g_hash_table_remove (priv->header_hash, old_header);
            }
          if (new_header != NULL)
            {
              g_hash_table_insert (priv->header_hash, new_header, row);
              gtk_widget_unparent (new_header);
              gtk_widget_set_parent (new_header, GTK_WIDGET (box));
              gtk_widget_show (new_header);
            }
          gtk_widget_queue_resize (GTK_WIDGET (box));
        }
      if (old_header)
        g_object_unref (old_header);
    }
  else
    {
      if (ROW_PRIV (row)->header != NULL)
        {
          g_hash_table_remove (priv->header_hash, ROW_PRIV (row)->header);
          gtk_widget_unparent (ROW_PRIV (row)->header);
          gtk_list_box_row_set_header (row, NULL);
          gtk_widget_queue_resize (GTK_WIDGET (box));
        }
    }
  if (before_row)
    g_object_unref (before_row);
  g_object_unref (row);
}

static void
gtk_list_box_row_visibility_changed (GtkListBox    *box,
                                     GtkListBoxRow *row)
{
  update_row_is_visible (box, row);

  if (gtk_widget_get_visible (GTK_WIDGET (box)))
    {
      gtk_list_box_update_header (box, ROW_PRIV (row)->iter);
      gtk_list_box_update_header (box,
                                  gtk_list_box_get_next_visible (box, ROW_PRIV (row)->iter));
    }
}

static void
gtk_list_box_add (GtkContainer *container,
                  GtkWidget    *child)
{
  gtk_list_box_insert (GTK_LIST_BOX (container), child, -1);
}

static void
gtk_list_box_remove (GtkContainer *container,
                     GtkWidget    *child)
{
  GtkWidget *widget = GTK_WIDGET (container);
  GtkListBox *box = GTK_LIST_BOX (container);
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  gboolean was_visible;
  gboolean was_selected;
  GtkListBoxRow *row;
  GSequenceIter *iter;
  GSequenceIter *next;

  was_visible = gtk_widget_get_visible (child);

  if (child == priv->placeholder)
    {
      gtk_widget_unparent (child);
      priv->placeholder = NULL;
      if (was_visible && gtk_widget_get_visible (widget))
        gtk_widget_queue_resize (widget);

      return;
    }

  if (!GTK_IS_LIST_BOX_ROW (child))
    {
      row = g_hash_table_lookup (priv->header_hash, child);
      if (row != NULL)
        {
          g_hash_table_remove (priv->header_hash, child);
          g_clear_object (&ROW_PRIV (row)->header);
          gtk_widget_unparent (child);
          if (was_visible && gtk_widget_get_visible (widget))
            gtk_widget_queue_resize (widget);
        }
      else
        {
          g_warning ("Tried to remove non-child %p", child);
        }
      return;
    }

  row = GTK_LIST_BOX_ROW (child);
  iter = ROW_PRIV (row)->iter;
  if (g_sequence_iter_get_sequence (iter) != priv->children)
    {
      g_warning ("Tried to remove non-child %p", child);
      return;
    }

  was_selected = ROW_PRIV (row)->selected;

  if (ROW_PRIV (row)->visible)
    list_box_add_visible_rows (box, -1);

  if (ROW_PRIV (row)->header != NULL)
    {
      g_hash_table_remove (priv->header_hash, ROW_PRIV (row)->header);
      gtk_widget_unparent (ROW_PRIV (row)->header);
      g_clear_object (&ROW_PRIV (row)->header);
    }

  if (row == priv->selected_row)
    priv->selected_row = NULL;
  if (row == priv->cursor_row)
    priv->cursor_row = NULL;
  if (row == priv->active_row)
    priv->active_row = NULL;

  if (row == priv->drag_highlighted_row)
    gtk_list_box_drag_unhighlight_row (box);

  next = gtk_list_box_get_next_visible (box, iter);
  gtk_widget_unparent (child);
  g_sequence_remove (iter);

  /* After unparenting, those values are garbage */
  iter = NULL;
  row = NULL;
  child = NULL;

  if (gtk_widget_get_visible (widget))
    gtk_list_box_update_header (box, next);

  if (was_visible && gtk_widget_get_visible (GTK_WIDGET (box)))
    gtk_widget_queue_resize (widget);

  if (was_selected && !gtk_widget_in_destruction (widget))
    {
      g_signal_emit (box, signals[ROW_SELECTED], 0, NULL);
      g_signal_emit (box, signals[SELECTED_ROWS_CHANGED], 0);
    }
}

static void
gtk_list_box_forall (GtkContainer *container,
                     GtkCallback   callback,
                     gpointer      callback_target)
{
  GtkListBoxPrivate *priv = BOX_PRIV (container);
  GSequenceIter *iter;
  GtkListBoxRow *row;

  if (priv->placeholder != NULL)
    callback (priv->placeholder, callback_target);

  iter = g_sequence_get_begin_iter (priv->children);
  while (!g_sequence_iter_is_end (iter))
    {
      row = g_sequence_get (iter);
      iter = g_sequence_iter_next (iter);
      callback (GTK_WIDGET (row), callback_target);
    }
}

static void
gtk_list_box_compute_expand (GtkWidget *widget,
                             gboolean  *hexpand,
                             gboolean  *vexpand)
{
  GTK_WIDGET_CLASS (gtk_list_box_parent_class)->compute_expand (widget,
                                                                hexpand, vexpand);

  /* We don't expand vertically beyound the minimum size */
  if (vexpand)
    *vexpand = FALSE;
}

static GType
gtk_list_box_child_type (GtkContainer *container)
{
  /* We really support any type but we wrap it in a row. But that is
   * more like a C helper function, in an abstract sense we only support
   * row children, so that is what tools accessing this should use.
   */
  return GTK_TYPE_LIST_BOX_ROW;
}

static GtkSizeRequestMode
gtk_list_box_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_list_box_measure (GtkWidget     *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkListBoxPrivate *priv = BOX_PRIV (widget);
  GSequenceIter *iter;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = 0;
      *natural = 0;

      if (priv->placeholder && gtk_widget_get_child_visible (priv->placeholder))
        gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_HORIZONTAL, -1,
                            minimum, natural,
                            NULL, NULL);

      for (iter = g_sequence_get_begin_iter (priv->children);
           !g_sequence_iter_is_end (iter);
           iter = g_sequence_iter_next (iter))
        {
          GtkListBoxRow *row;
          gint row_min;
          gint row_nat;

          row = g_sequence_get (iter);

          /* We *do* take visible but filtered rows into account here so that
           * the list width doesn't change during filtering
           */
          if (!gtk_widget_get_visible (GTK_WIDGET (row)))
            continue;

          gtk_widget_measure (GTK_WIDGET (row), orientation, -1,
                              &row_min, &row_nat,
                              NULL, NULL);

          *minimum = MAX (*minimum, row_min);
          *natural = MAX (*natural, row_nat);

          if (ROW_PRIV (row)->header != NULL)
            {
              gtk_widget_measure (ROW_PRIV (row)->header, orientation, -1,
                                  &row_min, &row_nat,
                                  NULL, NULL);
              *minimum = MAX (*minimum, row_min);
              *natural = MAX (*natural, row_nat);
            }
        }
    }
  else
    {
      if (for_size < 0)
        {
          int f;
          gtk_list_box_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                                &f, &for_size, NULL, NULL);
        }

      *minimum = 0;

      if (priv->placeholder && gtk_widget_get_child_visible (priv->placeholder))
        gtk_widget_measure (priv->placeholder, orientation, for_size,
                            minimum, NULL,
                            NULL, NULL);

      for (iter = g_sequence_get_begin_iter (priv->children);
           !g_sequence_iter_is_end (iter);
           iter = g_sequence_iter_next (iter))
        {
          GtkListBoxRow *row;
          gint row_min = 0;

          row = g_sequence_get (iter);
          if (!row_is_visible (row))
            continue;

          if (ROW_PRIV (row)->header != NULL)
            {
              gtk_widget_measure (ROW_PRIV (row)->header, orientation, for_size,
                                  &row_min, NULL,
                                  NULL, NULL);
              *minimum += row_min;
            }
          gtk_widget_measure (GTK_WIDGET (row), orientation, for_size,
                              &row_min, NULL,
                              NULL, NULL);
          *minimum += row_min;
        }

      /* We always allocate the minimum height, since handling expanding rows
       * is way too costly, and unlikely to be used, as lists are generally put
       * inside a scrolling window anyway.
       */
      *natural = *minimum;
    }
}

static void
gtk_list_box_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkListBoxPrivate *priv = BOX_PRIV (widget);
  GtkAllocation child_allocation;
  GtkAllocation header_allocation;
  GtkListBoxRow *row;
  GSequenceIter *iter;
  int child_min;


  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = width;
  child_allocation.height = 0;

  header_allocation.x = 0;
  header_allocation.y = 0;
  header_allocation.width = width;
  header_allocation.height = 0;

  if (priv->placeholder && gtk_widget_get_child_visible (priv->placeholder))
    {
      gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_VERTICAL,
                          width,
                          &child_min, NULL, NULL, NULL);
      header_allocation.height = height;
      header_allocation.y = child_allocation.y;
      gtk_widget_size_allocate (priv->placeholder, &header_allocation, -1);
      child_allocation.y += child_min;
    }

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      if (!row_is_visible (row))
        {
          ROW_PRIV (row)->y = child_allocation.y;
          ROW_PRIV (row)->height = 0;
          continue;
        }

      if (ROW_PRIV (row)->header != NULL)
        {
          gtk_widget_measure (ROW_PRIV (row)->header, GTK_ORIENTATION_VERTICAL,
                              width,
                              &child_min, NULL, NULL, NULL);
          header_allocation.height = child_min;
          header_allocation.y = child_allocation.y;
          gtk_widget_size_allocate (ROW_PRIV (row)->header,
                                    &header_allocation,
                                    -1);
          child_allocation.y += child_min;
        }

      ROW_PRIV (row)->y = child_allocation.y;

      gtk_widget_measure (GTK_WIDGET (row), GTK_ORIENTATION_VERTICAL,
                          child_allocation.width,
                          &child_min, NULL, NULL, NULL);
      child_allocation.height = child_min;

      ROW_PRIV (row)->height = child_allocation.height;
      gtk_widget_size_allocate (GTK_WIDGET (row), &child_allocation, -1);
      child_allocation.y += child_min;
    }
}

/**
 * gtk_list_box_prepend:
 * @box: a #GtkListBox
 * @child: the #GtkWidget to add
 *
 * Prepend a widget to the list. If a sort function is set, the widget will
 * actually be inserted at the calculated position and this function has the
 * same effect of gtk_container_add().
 */
void
gtk_list_box_prepend (GtkListBox *box,
                      GtkWidget  *child)
{
  gtk_list_box_insert (box, child, 0);
}

/**
 * gtk_list_box_insert:
 * @box: a #GtkListBox
 * @child: the #GtkWidget to add
 * @position: the position to insert @child in
 *
 * Insert the @child into the @box at @position. If a sort function is
 * set, the widget will actually be inserted at the calculated position and
 * this function has the same effect of gtk_container_add().
 *
 * If @position is -1, or larger than the total number of items in the
 * @box, then the @child will be appended to the end.
 */
void
gtk_list_box_insert (GtkListBox *box,
                     GtkWidget  *child,
                     gint        position)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GtkListBoxRow *row;
  GSequenceIter *prev = NULL;
  GSequenceIter *iter = NULL;

  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (GTK_IS_LIST_BOX_ROW (child))
    row = GTK_LIST_BOX_ROW (child);
  else
    {
      row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
      gtk_container_add (GTK_CONTAINER (row), child);
    }

  if (priv->sort_func != NULL)
    iter = g_sequence_insert_sorted (priv->children, row,
                                     (GCompareDataFunc)do_sort, box);
  else if (position == 0)
    iter = g_sequence_prepend (priv->children, row);
  else if (position == -1)
    iter = g_sequence_append (priv->children, row);
  else
    {
      GSequenceIter *current_iter;

      current_iter = g_sequence_get_iter_at_pos (priv->children, position);
      iter = g_sequence_insert_before (current_iter, row);
    }

  ROW_PRIV (row)->iter = iter;
  prev = g_sequence_iter_prev (iter);
  gtk_widget_insert_after (GTK_WIDGET (row), GTK_WIDGET (box),
                           prev != iter ? g_sequence_get (prev) : NULL);

  gtk_widget_set_child_visible (GTK_WIDGET (row), TRUE);
  ROW_PRIV (row)->visible = gtk_widget_get_visible (GTK_WIDGET (row));
  if (ROW_PRIV (row)->visible)
    list_box_add_visible_rows (box, 1);
  gtk_list_box_apply_filter (box, row);
  gtk_list_box_update_row_style (box, row);
  if (gtk_widget_get_visible (GTK_WIDGET (box)))
    {
      gtk_list_box_update_header (box, ROW_PRIV (row)->iter);
      gtk_list_box_update_header (box,
                                  gtk_list_box_get_next_visible (box, ROW_PRIV (row)->iter));
    }
}

/**
 * gtk_list_box_drag_unhighlight_row:
 * @box: a #GtkListBox
 *
 * If a row has previously been highlighted via gtk_list_box_drag_highlight_row()
 * it will have the highlight removed.
 */
void
gtk_list_box_drag_unhighlight_row (GtkListBox *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->drag_highlighted_row == NULL)
    return;

  gtk_drag_unhighlight (GTK_WIDGET (priv->drag_highlighted_row));
  g_clear_object (&priv->drag_highlighted_row);
}

/**
 * gtk_list_box_drag_highlight_row:
 * @box: a #GtkListBox
 * @row: a #GtkListBoxRow
 *
 * This is a helper function for implementing DnD onto a #GtkListBox.
 * The passed in @row will be highlighted via gtk_drag_highlight(),
 * and any previously highlighted row will be unhighlighted.
 *
 * The row will also be unhighlighted when the widget gets
 * a drag leave event.
 */
void
gtk_list_box_drag_highlight_row (GtkListBox    *box,
                                 GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  if (priv->drag_highlighted_row == row)
    return;

  gtk_list_box_drag_unhighlight_row (box);
  gtk_drag_highlight (GTK_WIDGET (row));
  priv->drag_highlighted_row = g_object_ref (row);
}

static void
gtk_list_box_drag_leave (GtkWidget *widget,
                         GdkDrop   *drop)
{
  gtk_list_box_drag_unhighlight_row (GTK_LIST_BOX (widget));
}

static void
gtk_list_box_activate_cursor_row (GtkListBox *box)
{
  gtk_list_box_select_and_activate (box, BOX_PRIV (box)->cursor_row);
}

static void
gtk_list_box_toggle_cursor_row (GtkListBox *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  if (priv->cursor_row == NULL)
    return;

  if ((priv->selection_mode == GTK_SELECTION_SINGLE ||
       priv->selection_mode == GTK_SELECTION_MULTIPLE) &&
      ROW_PRIV (priv->cursor_row)->selected)
    gtk_list_box_unselect_row_internal (box, priv->cursor_row);
  else
    gtk_list_box_select_and_activate (box, priv->cursor_row);
}

static void
gtk_list_box_move_cursor (GtkListBox      *box,
                          GtkMovementStep  step,
                          gint             count)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  gboolean modify;
  gboolean extend;
  GtkListBoxRow *row;
  gint page_size;
  GSequenceIter *iter;
  gint start_y;
  gint end_y;
  int height;

  row = NULL;
  switch ((guint) step)
    {
    case GTK_MOVEMENT_BUFFER_ENDS:
      if (count < 0)
        row = gtk_list_box_get_first_focusable (box);
      else
        row = gtk_list_box_get_last_focusable (box);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      if (priv->cursor_row != NULL)
        {
          gint i = count;

          iter = ROW_PRIV (priv->cursor_row)->iter;

          while (i < 0  && iter != NULL)
            {
              iter = gtk_list_box_get_previous_visible (box, iter);
              i = i + 1;
            }
          while (i > 0  && iter != NULL)
            {
              iter = gtk_list_box_get_next_visible (box, iter);
              i = i - 1;
            }

          if (iter != NULL && !g_sequence_iter_is_end (iter))
            row = g_sequence_get (iter);
        }
      break;
    case GTK_MOVEMENT_PAGES:
      page_size = 100;
      if (priv->adjustment != NULL)
        page_size = gtk_adjustment_get_page_increment (priv->adjustment);

      if (priv->cursor_row != NULL)
        {
          start_y = ROW_PRIV (priv->cursor_row)->y;
          height = gtk_widget_get_height (GTK_WIDGET (box));
          end_y = CLAMP (start_y + page_size * count, 0, height - 1);
          row = gtk_list_box_get_row_at_y (box, end_y);

          if (!row)
            {
              GSequenceIter *cursor_iter;
              GSequenceIter *next_iter;

              if (count > 0)
                {
                  cursor_iter = ROW_PRIV (priv->cursor_row)->iter;
                  next_iter = gtk_list_box_get_last_visible (box, cursor_iter);

                  if (next_iter)
                    {
                      row = g_sequence_get (next_iter);
                      end_y = ROW_PRIV (row)->y;
                    }
                }
              else
                {
                  row = gtk_list_box_get_row_at_index (box, 0);
                  end_y = ROW_PRIV (row)->y;
                }
            }
          else if (row == priv->cursor_row)
            {
              iter = ROW_PRIV (row)->iter;

              /* Move at least one row. This is important when the cursor_row's height is
               * greater than page_size */
              if (count < 0)
                iter = g_sequence_iter_prev (iter);
              else
                iter = g_sequence_iter_next (iter);

              if (!g_sequence_iter_is_begin (iter) && !g_sequence_iter_is_end (iter))
                {
                  row = g_sequence_get (iter);
                  end_y = ROW_PRIV (row)->y;
                }
            }

          if (end_y != start_y && priv->adjustment != NULL)
            gtk_adjustment_animate_to_value (priv->adjustment, end_y);
        }
      break;
    default:
      return;
    }

  if (row == NULL || row == priv->cursor_row)
    {
      GtkDirectionType direction = count < 0 ? GTK_DIR_UP : GTK_DIR_DOWN;

      if (!gtk_widget_keynav_failed (GTK_WIDGET (box), direction))
        {
          GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (box)));

          if (toplevel)
            gtk_widget_child_focus (toplevel,
                                    direction == GTK_DIR_UP ?
                                    GTK_DIR_TAB_BACKWARD :
                                    GTK_DIR_TAB_FORWARD);

        }

      return;
    }

  get_current_selection_modifiers (GTK_WIDGET (box), &modify, &extend);

  gtk_list_box_update_cursor (box, row, TRUE);
  if (!modify)
    gtk_list_box_update_selection (box, row, FALSE, extend);
}


/**
 * gtk_list_box_row_new:
 *
 * Creates a new #GtkListBoxRow, to be used as a child of a #GtkListBox.
 *
 * Returns: a new #GtkListBoxRow
 */
GtkWidget *
gtk_list_box_row_new (void)
{
  return g_object_new (GTK_TYPE_LIST_BOX_ROW, NULL);
}

static void
gtk_list_box_row_set_focus (GtkListBoxRow *row)
{
  GtkListBox *box = gtk_list_box_row_get_box (row);
  gboolean modify;
  gboolean extend;

  if (!box)
    return;

  get_current_selection_modifiers (GTK_WIDGET (row), &modify, &extend);

  if (modify)
    gtk_list_box_update_cursor (box, row, TRUE);
  else
    gtk_list_box_update_selection (box, row, FALSE, FALSE);
}

static gboolean
gtk_list_box_row_focus (GtkWidget        *widget,
                        GtkDirectionType  direction)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  gboolean had_focus = FALSE;
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));

  g_object_get (widget, "has-focus", &had_focus, NULL);
  if (had_focus)
    {
      /* If on row, going right, enter into possible container */
      if (child &&
          (direction == GTK_DIR_RIGHT || direction == GTK_DIR_TAB_FORWARD))
        {
          if (gtk_widget_child_focus (GTK_WIDGET (child), direction))
            return TRUE;
        }

      return FALSE;
    }
  else if (gtk_widget_get_focus_child (widget) != NULL)
    {
      /* Child has focus, always navigate inside it first */
      if (gtk_widget_child_focus (gtk_widget_get_focus_child (widget), direction))
        return TRUE;

      /* If exiting child container to the left, select row  */
      if (direction == GTK_DIR_LEFT || direction == GTK_DIR_TAB_BACKWARD)
        {
          gtk_list_box_row_set_focus (row);
          return TRUE;
        }

      return FALSE;
    }
  else
    {
      /* If coming from the left, enter into possible container */
      if (child &&
          (direction == GTK_DIR_LEFT || direction == GTK_DIR_TAB_BACKWARD))
        {
          if (gtk_widget_child_focus (child, direction))
            return TRUE;
        }

      gtk_list_box_row_set_focus (row);
      return TRUE;
    }
}

static void
gtk_list_box_row_activate (GtkListBoxRow *row)
{
  GtkListBox *box;

  box = gtk_list_box_row_get_box (row);
  if (box)
    gtk_list_box_select_and_activate (box, row);
}

static void
gtk_list_box_row_show (GtkWidget *widget)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkListBox *box;

  GTK_WIDGET_CLASS (gtk_list_box_row_parent_class)->show (widget);

  box = gtk_list_box_row_get_box (row);
  if (box)
    gtk_list_box_row_visibility_changed (box, row);
}

static void
gtk_list_box_row_hide (GtkWidget *widget)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkListBox *box;

  GTK_WIDGET_CLASS (gtk_list_box_row_parent_class)->hide (widget);

  box = gtk_list_box_row_get_box (row);
  if (box)
    gtk_list_box_row_visibility_changed (box, row);
}

/**
 * gtk_list_box_row_changed:
 * @row: a #GtkListBoxRow
 *
 * Marks @row as changed, causing any state that depends on this
 * to be updated. This affects sorting, filtering and headers.
 *
 * Note that calls to this method must be in sync with the data
 * used for the row functions. For instance, if the list is
 * mirroring some external data set, and *two* rows changed in the
 * external data set then when you call gtk_list_box_row_changed()
 * on the first row the sort function must only read the new data
 * for the first of the two changed rows, otherwise the resorting
 * of the rows will be wrong.
 *
 * This generally means that if you don’t fully control the data
 * model you have to duplicate the data that affects the listbox
 * row functions into the row widgets themselves. Another alternative
 * is to call gtk_list_box_invalidate_sort() on any model change,
 * but that is more expensive.
 */
void
gtk_list_box_row_changed (GtkListBoxRow *row)
{
  GtkListBox *box;

  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  box = gtk_list_box_row_get_box (row);
  if (box)
    gtk_list_box_got_row_changed (box, row);
}

/**
 * gtk_list_box_row_get_header:
 * @row: a #GtkListBoxRow
 *
 * Returns the current header of the @row. This can be used
 * in a #GtkListBoxUpdateHeaderFunc to see if there is a header
 * set already, and if so to update the state of it.
 *
 * Returns: (transfer none) (nullable): the current header, or %NULL if none
 */
GtkWidget *
gtk_list_box_row_get_header (GtkListBoxRow *row)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), NULL);

  return ROW_PRIV (row)->header;
}

/**
 * gtk_list_box_row_set_header:
 * @row: a #GtkListBoxRow
 * @header: (allow-none): the header, or %NULL
 *
 * Sets the current header of the @row. This is only allowed to be called
 * from a #GtkListBoxUpdateHeaderFunc. It will replace any existing
 * header in the row, and be shown in front of the row in the listbox.
 */
void
gtk_list_box_row_set_header (GtkListBoxRow *row,
                             GtkWidget     *header)
{
  GtkListBoxRowPrivate *priv;

  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));
  g_return_if_fail (header == NULL || GTK_IS_WIDGET (header));

  priv = ROW_PRIV (row);

  if (priv->header)
    g_object_unref (priv->header);

  priv->header = header;

  if (header)
    g_object_ref_sink (header);
}

/**
 * gtk_list_box_row_get_index:
 * @row: a #GtkListBoxRow
 *
 * Gets the current index of the @row in its #GtkListBox container.
 *
 * Returns: the index of the @row, or -1 if the @row is not in a listbox
 */
gint
gtk_list_box_row_get_index (GtkListBoxRow *row)
{
  GtkListBoxRowPrivate *priv;

  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), -1);

  priv = ROW_PRIV (row);

  if (priv->iter != NULL)
    return g_sequence_iter_get_position (priv->iter);

  return -1;
}

/**
 * gtk_list_box_row_is_selected:
 * @row: a #GtkListBoxRow
 *
 * Returns whether the child is currently selected in its
 * #GtkListBox container.
 *
 * Returns: %TRUE if @row is selected
 */
gboolean
gtk_list_box_row_is_selected (GtkListBoxRow *row)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), FALSE);

  return ROW_PRIV (row)->selected;
}

static void
gtk_list_box_update_row_style (GtkListBox    *box,
                               GtkListBoxRow *row)
{
  GtkStyleContext *context;
  gboolean can_select;

  if (box && BOX_PRIV (box)->selection_mode != GTK_SELECTION_NONE)
    can_select = TRUE;
  else
    can_select = FALSE;

  context = gtk_widget_get_style_context (GTK_WIDGET (row));
  if (ROW_PRIV (row)->activatable ||
      (ROW_PRIV (row)->selectable && can_select))
    gtk_style_context_add_class (context, "activatable");
  else
    gtk_style_context_remove_class (context, "activatable");
}

static void
gtk_list_box_update_row_styles (GtkListBox *box)
{
  GSequenceIter *iter;
  GtkListBoxRow *row;

  for (iter = g_sequence_get_begin_iter (BOX_PRIV (box)->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      gtk_list_box_update_row_style (box, row);
    }
}

/**
 * gtk_list_box_row_set_activatable:
 * @row: a #GtkListBoxRow
 * @activatable: %TRUE to mark the row as activatable
 *
 * Set the #GtkListBoxRow:activatable property for this row.
 */
void
gtk_list_box_row_set_activatable (GtkListBoxRow *row,
                                  gboolean       activatable)
{
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  activatable = activatable != FALSE;

  if (ROW_PRIV (row)->activatable != activatable)
    {
      ROW_PRIV (row)->activatable = activatable;

      gtk_list_box_update_row_style (gtk_list_box_row_get_box (row), row);
      g_object_notify_by_pspec (G_OBJECT (row), row_properties[ROW_PROP_ACTIVATABLE]);
    }
}

/**
 * gtk_list_box_row_get_activatable:
 * @row: a #GtkListBoxRow
 *
 * Gets the value of the #GtkListBoxRow:activatable property
 * for this row.
 *
 * Returns: %TRUE if the row is activatable
 */
gboolean
gtk_list_box_row_get_activatable (GtkListBoxRow *row)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), TRUE);

  return ROW_PRIV (row)->activatable;
}

/**
 * gtk_list_box_row_set_selectable:
 * @row: a #GtkListBoxRow
 * @selectable: %TRUE to mark the row as selectable
 *
 * Set the #GtkListBoxRow:selectable property for this row.
 */
void
gtk_list_box_row_set_selectable (GtkListBoxRow *row,
                                 gboolean       selectable)
{
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  selectable = selectable != FALSE;

  if (ROW_PRIV (row)->selectable != selectable)
    {
      if (!selectable)
        gtk_list_box_row_set_selected (row, FALSE);
 
      ROW_PRIV (row)->selectable = selectable;

      gtk_list_box_update_row_style (gtk_list_box_row_get_box (row), row);
      g_object_notify_by_pspec (G_OBJECT (row), row_properties[ROW_PROP_SELECTABLE]);
    }
}

/**
 * gtk_list_box_row_get_selectable:
 * @row: a #GtkListBoxRow
 *
 * Gets the value of the #GtkListBoxRow:selectable property
 * for this row.
 *
 * Returns: %TRUE if the row is selectable
 */
gboolean
gtk_list_box_row_get_selectable (GtkListBoxRow *row)
{
  g_return_val_if_fail (GTK_IS_LIST_BOX_ROW (row), TRUE);

  return ROW_PRIV (row)->selectable;
}

static void
gtk_list_box_row_set_action_name (GtkActionable *actionable,
                                  const gchar   *action_name)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (actionable);
  GtkListBoxRowPrivate *priv = ROW_PRIV (row);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_name (priv->action_helper, action_name);
}

static void
gtk_list_box_row_set_action_target_value (GtkActionable *actionable,
                                          GVariant      *action_target)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (actionable);
  GtkListBoxRowPrivate *priv = ROW_PRIV (row);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (priv->action_helper, action_target);
}

static void
gtk_list_box_row_get_property (GObject    *obj,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (obj);

  switch (property_id)
    {
    case ROW_PROP_ACTIVATABLE:
      g_value_set_boolean (value, gtk_list_box_row_get_activatable (row));
      break;
    case ROW_PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_list_box_row_get_selectable (row));
      break;
    case ROW_PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (ROW_PRIV (row)->action_helper));
      break;
    case ROW_PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (ROW_PRIV (row)->action_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_list_box_row_set_property (GObject      *obj,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (obj);

  switch (property_id)
    {
    case ROW_PROP_ACTIVATABLE:
      gtk_list_box_row_set_activatable (row, g_value_get_boolean (value));
      break;
    case ROW_PROP_SELECTABLE:
      gtk_list_box_row_set_selectable (row, g_value_get_boolean (value));
      break;
    case ROW_PROP_ACTION_NAME:
      gtk_list_box_row_set_action_name (GTK_ACTIONABLE (row), g_value_get_string (value));
      break;
    case ROW_PROP_ACTION_TARGET:
      gtk_list_box_row_set_action_target_value (GTK_ACTIONABLE (row), g_value_get_variant (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static const gchar *
gtk_list_box_row_get_action_name (GtkActionable *actionable)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (actionable);

  return gtk_action_helper_get_action_name (ROW_PRIV (row)->action_helper);
}

static GVariant *
gtk_list_box_row_get_action_target_value (GtkActionable *actionable)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (actionable);

  return gtk_action_helper_get_action_target_value (ROW_PRIV (row)->action_helper);
}

static void
gtk_list_box_row_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_list_box_row_get_action_name;
  iface->set_action_name = gtk_list_box_row_set_action_name;
  iface->get_action_target_value = gtk_list_box_row_get_action_target_value;
  iface->set_action_target_value = gtk_list_box_row_set_action_target_value;
}

static void
gtk_list_box_row_finalize (GObject *obj)
{
  g_clear_object (&ROW_PRIV (GTK_LIST_BOX_ROW (obj))->header);

  G_OBJECT_CLASS (gtk_list_box_row_parent_class)->finalize (obj);
}

static void
gtk_list_box_row_dispose (GObject *object)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (object);
  GtkListBoxRowPrivate *priv = ROW_PRIV (row);

  g_clear_object (&priv->action_helper);

  G_OBJECT_CLASS (gtk_list_box_row_parent_class)->dispose (object);
}

static gboolean
gtk_list_box_row_grab_focus (GtkWidget *widget)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkListBox *box = gtk_list_box_row_get_box (row);

  g_return_val_if_fail (box != NULL, FALSE);

  if (!GTK_WIDGET_CLASS (gtk_list_box_row_parent_class)->grab_focus (widget))
    return FALSE;

  if (BOX_PRIV (box)->cursor_row != row)
    gtk_list_box_update_cursor (box, row, FALSE);

  return TRUE;
}

static void
gtk_list_box_row_class_init (GtkListBoxRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_LIST_BOX_ROW_ACCESSIBLE);

  object_class->get_property = gtk_list_box_row_get_property;
  object_class->set_property = gtk_list_box_row_set_property;
  object_class->finalize = gtk_list_box_row_finalize;
  object_class->dispose = gtk_list_box_row_dispose;

  widget_class->show = gtk_list_box_row_show;
  widget_class->hide = gtk_list_box_row_hide;
  widget_class->focus = gtk_list_box_row_focus;
  widget_class->grab_focus = gtk_list_box_row_grab_focus;

  klass->activate = gtk_list_box_row_activate;

  /**
   * GtkListBoxRow::activate:
   *
   * This is a keybinding signal, which will cause this row to be activated.
   *
   * If you want to be notified when the user activates a row (by key or not),
   * use the #GtkListBox::row-activated signal on the row’s parent #GtkListBox.
   */
  row_signals[ROW__ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxRowClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  widget_class->activate_signal = row_signals[ROW__ACTIVATE];

  /**
   * GtkListBoxRow:activatable:
   *
   * The property determines whether the #GtkListBox::row-activated
   * signal will be emitted for this row.
   */
  row_properties[ROW_PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable",
                          P_("Activatable"),
                          P_("Whether this row can be activated"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListBoxRow:selectable:
   *
   * The property determines whether this row can be selected.
   */
  row_properties[ROW_PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable",
                          P_("Selectable"),
                          P_("Whether this row can be selected"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_ROW_PROPERTY, row_properties);

  g_object_class_override_property (object_class, ROW_PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (object_class, ROW_PROP_ACTION_TARGET, "action-target");

  gtk_widget_class_set_css_name (widget_class, I_("row"));
}

static void
gtk_list_box_row_init (GtkListBoxRow *row)
{
  gtk_widget_set_can_focus (GTK_WIDGET (row), TRUE);

  ROW_PRIV (row)->activatable = TRUE;
  ROW_PRIV (row)->selectable = TRUE;

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (row)),
                               "activatable");
}

static void
gtk_list_box_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const gchar  *type)
{
  if (type && strcmp (type, "placeholder") == 0)
    gtk_list_box_set_placeholder (GTK_LIST_BOX (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_list_box_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_list_box_buildable_add_child;
}

static void
gtk_list_box_bound_model_changed (GListModel *list,
                                  guint       position,
                                  guint       removed,
                                  guint       added,
                                  gpointer    user_data)
{
  GtkListBox *box = user_data;
  GtkListBoxPrivate *priv = BOX_PRIV (user_data);
  guint i;

  while (removed--)
    {
      GtkListBoxRow *row;

      row = gtk_list_box_get_row_at_index (box, position);
      gtk_container_remove (GTK_CONTAINER (box), GTK_WIDGET (row));
    }

  for (i = 0; i < added; i++)
    {
      GObject *item;
      GtkWidget *widget;

      item = g_list_model_get_item (list, position + i);
      widget = priv->create_widget_func (item, priv->create_widget_func_data);

      /* We allow the create_widget_func to either return a full
       * reference or a floating reference.  If we got the floating
       * reference, then turn it into a full reference now.  That means
       * that gtk_list_box_insert() will take another full reference.
       * Finally, we'll release this full reference below, leaving only
       * the one held by the box.
       */
      if (g_object_is_floating (widget))
        g_object_ref_sink (widget);

      gtk_widget_show (widget);
      gtk_list_box_insert (box, widget, position + i);

      g_object_unref (widget);
      g_object_unref (item);
    }
}

static void
gtk_list_box_check_model_compat (GtkListBox *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  if (priv->bound_model &&
      (priv->sort_func || priv->filter_func))
    g_warning ("GtkListBox with a model will ignore sort and filter functions");
}

/**
 * gtk_list_box_bind_model:
 * @box: a #GtkListBox
 * @model: (nullable): the #GListModel to be bound to @box
 * @create_widget_func: (nullable): a function that creates widgets for items
 *   or %NULL in case you also passed %NULL as @model
 * @user_data: user data passed to @create_widget_func
 * @user_data_free_func: function for freeing @user_data
 *
 * Binds @model to @box.
 *
 * If @box was already bound to a model, that previous binding is
 * destroyed.
 *
 * The contents of @box are cleared and then filled with widgets that
 * represent items from @model. @box is updated whenever @model changes.
 * If @model is %NULL, @box is left empty.
 *
 * It is undefined to add or remove widgets directly (for example, with
 * gtk_list_box_insert() or gtk_container_add()) while @box is bound to a
 * model.
 *
 * Note that using a model is incompatible with the filtering and sorting
 * functionality in GtkListBox. When using a model, filtering and sorting
 * should be implemented by the model.
 */
void
gtk_list_box_bind_model (GtkListBox                 *box,
                         GListModel                 *model,
                         GtkListBoxCreateWidgetFunc  create_widget_func,
                         gpointer                    user_data,
                         GDestroyNotify              user_data_free_func)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);
  GSequenceIter *iter;

  g_return_if_fail (GTK_IS_LIST_BOX (box));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  g_return_if_fail (model == NULL || create_widget_func != NULL);

  if (priv->bound_model)
    {
      if (priv->create_widget_func_data_destroy)
        priv->create_widget_func_data_destroy (priv->create_widget_func_data);

      g_signal_handlers_disconnect_by_func (priv->bound_model, gtk_list_box_bound_model_changed, box);
      g_clear_object (&priv->bound_model);
    }

  iter = g_sequence_get_begin_iter (priv->children);
  while (!g_sequence_iter_is_end (iter))
    {
      GtkWidget *row = g_sequence_get (iter);
      iter = g_sequence_iter_next (iter);
      gtk_list_box_remove (GTK_CONTAINER (box), row);
    }


  if (model == NULL)
    return;

  priv->bound_model = g_object_ref (model);
  priv->create_widget_func = create_widget_func;
  priv->create_widget_func_data = user_data;
  priv->create_widget_func_data_destroy = user_data_free_func;

  gtk_list_box_check_model_compat (box);

  g_signal_connect (priv->bound_model, "items-changed", G_CALLBACK (gtk_list_box_bound_model_changed), box);
  gtk_list_box_bound_model_changed (model, 0, 0, g_list_model_get_n_items (model), box);
}

/**
 * gtk_list_box_set_show_separators:
 * @box: a #GtkListBox
 * @show_separators: %TRUE to show separators
 *
 * Sets whether the list box should show separators
 * between rows.
 */
void
gtk_list_box_set_show_separators (GtkListBox *box,
                                  gboolean    show_separators)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_if_fail (GTK_IS_LIST_BOX (box));

  if (priv->show_separators == show_separators)
    return;

  priv->show_separators = show_separators;

  if (show_separators)
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (box)), "separators");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (box)), "separators");

  g_object_notify_by_pspec (G_OBJECT (box), properties[PROP_SHOW_SEPARATORS]);
}

/**
 * gtk_list_box_get_show_separators:
 * @box: a #GtkListBox
 *
 * Returns whether the list box should show separators
 * between rows.
 *
 * Returns: %TRUE if the list box shows separators
 */
gboolean
gtk_list_box_get_show_separators (GtkListBox *box)
{
  GtkListBoxPrivate *priv = BOX_PRIV (box);

  g_return_val_if_fail (GTK_IS_LIST_BOX (box), FALSE);

  return priv->show_separators;
}
