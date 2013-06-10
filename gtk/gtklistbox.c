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

#include <gtk/gtk.h>
#include <gtkmarshalers.h>
#include <float.h>
#include <math.h>
#include <string.h>

#include "a11y/gtklistboxaccessibleprivate.h"

/**
 * SECTION:gtklistbox
 * @Short_description: A list container
 * @Title: GtkListBox
 *
 */

struct _GtkListBoxPrivate
{
  GSequence *children;
  GHashTable *separator_hash;

  GtkListBoxSortFunc sort_func;
  gpointer sort_func_target;
  GDestroyNotify sort_func_target_destroy_notify;

  GtkListBoxFilterFunc filter_func;
  gpointer filter_func_target;
  GDestroyNotify filter_func_target_destroy_notify;

  GtkListBoxUpdateSeparatorFunc update_separator_func;
  gpointer update_separator_func_target;
  GDestroyNotify update_separator_func_target_destroy_notify;

  GtkListBoxRow *selected_row;
  GtkListBoxRow *prelight_row;
  GtkListBoxRow *cursor_row;

  gboolean active_row_active;
  GtkListBoxRow *active_row;

  GtkSelectionMode selection_mode;

  GtkAdjustment *adjustment;
  gboolean activate_single_click;

  /* DnD */
  GtkListBoxRow *drag_highlighted_row;
};

struct _GtkListBoxRowPrivate
{
  GSequenceIter *iter;
  GtkWidget *separator;
  gint y;
  gint height;
};

enum {
  ROW_SELECTED,
  ROW_ACTIVATED,
  ACTIVATE_CURSOR_ROW,
  TOGGLE_CURSOR_ROW,
  MOVE_CURSOR,
  REFILTER,
  LAST_SIGNAL
};

enum  {
  PROP_0,
  PROP_SELECTION_MODE,
  PROP_ACTIVATE_ON_SINGLE_CLICK,
  LAST_PROPERTY
};

G_DEFINE_TYPE (GtkListBox, gtk_list_box, GTK_TYPE_CONTAINER)
G_DEFINE_TYPE (GtkListBoxRow, gtk_list_box_row, GTK_TYPE_BIN)

static void                 gtk_list_box_update_selected              (GtkListBox          *list_box,
                                                                       GtkListBoxRow       *row);
static void                 gtk_list_box_apply_filter_all             (GtkListBox          *list_box);
static void                 gtk_list_box_update_separator             (GtkListBox          *list_box,
                                                                       GSequenceIter       *iter);
static GSequenceIter *      gtk_list_box_get_next_visible             (GtkListBox          *list_box,
                                                                       GSequenceIter       *_iter);
static void                 gtk_list_box_apply_filter                 (GtkListBox          *list_box,
                                                                       GtkListBoxRow       *row);
static void                 gtk_list_box_add_move_binding             (GtkBindingSet       *binding_set,
                                                                       guint                keyval,
                                                                       GdkModifierType      modmask,
                                                                       GtkMovementStep      step,
                                                                       gint                 count);
static void                 gtk_list_box_update_cursor                (GtkListBox          *list_box,
                                                                       GtkListBoxRow       *row);
static void                 gtk_list_box_select_and_activate          (GtkListBox          *list_box,
                                                                       GtkListBoxRow       *row);
static void                 gtk_list_box_update_prelight              (GtkListBox          *list_box,
                                                                       GtkListBoxRow       *row);
static void                 gtk_list_box_update_active                (GtkListBox          *list_box,
                                                                       GtkListBoxRow       *row);
static gboolean             gtk_list_box_real_enter_notify_event      (GtkWidget           *widget,
                                                                       GdkEventCrossing    *event);
static gboolean             gtk_list_box_real_leave_notify_event      (GtkWidget           *widget,
                                                                       GdkEventCrossing    *event);
static gboolean             gtk_list_box_real_motion_notify_event     (GtkWidget           *widget,
                                                                       GdkEventMotion      *event);
static gboolean             gtk_list_box_real_button_press_event      (GtkWidget           *widget,
                                                                       GdkEventButton      *event);
static gboolean             gtk_list_box_real_button_release_event    (GtkWidget           *widget,
                                                                       GdkEventButton      *event);
static void                 gtk_list_box_real_show                    (GtkWidget           *widget);
static gboolean             gtk_list_box_real_focus                   (GtkWidget           *widget,
                                                                       GtkDirectionType     direction);
static GSequenceIter*       gtk_list_box_get_previous_visible         (GtkListBox          *list_box,
                                                                       GSequenceIter       *_iter);
static GtkListBoxRow       *gtk_list_box_get_first_visible            (GtkListBox          *list_box);
static GtkListBoxRow       *gtk_list_box_get_last_visible             (GtkListBox          *list_box);
static gboolean             gtk_list_box_real_draw                    (GtkWidget           *widget,
                                                                       cairo_t             *cr);
static void                 gtk_list_box_real_realize                 (GtkWidget           *widget);
static void                 gtk_list_box_real_add                     (GtkContainer        *container,
                                                                       GtkWidget           *widget);
static void                 gtk_list_box_real_remove                  (GtkContainer        *container,
                                                                       GtkWidget           *widget);
static void                 gtk_list_box_real_forall_internal         (GtkContainer        *container,
                                                                       gboolean             include_internals,
                                                                       GtkCallback          callback,
                                                                       void                *callback_target);
static void                 gtk_list_box_real_compute_expand_internal (GtkWidget           *widget,
                                                                       gboolean            *hexpand,
                                                                       gboolean            *vexpand);
static GType                gtk_list_box_real_child_type              (GtkContainer        *container);
static GtkSizeRequestMode   gtk_list_box_real_get_request_mode        (GtkWidget           *widget);
static void                 gtk_list_box_real_size_allocate           (GtkWidget           *widget,
                                                                       GtkAllocation       *allocation);
static void                 gtk_list_box_real_drag_leave              (GtkWidget           *widget,
                                                                       GdkDragContext      *context,
                                                                       guint                time_);
static void                 gtk_list_box_real_activate_cursor_row     (GtkListBox          *list_box);
static void                 gtk_list_box_real_toggle_cursor_row       (GtkListBox          *list_box);
static void                 gtk_list_box_real_move_cursor             (GtkListBox          *list_box,
                                                                       GtkMovementStep      step,
                                                                       gint                 count);
static void                 gtk_list_box_real_refilter                (GtkListBox          *list_box);
static void                 gtk_list_box_finalize                     (GObject             *obj);


static void                 gtk_list_box_real_get_preferred_height           (GtkWidget           *widget,
                                                                              gint                *minimum_height,
                                                                              gint                *natural_height);
static void                 gtk_list_box_real_get_preferred_height_for_width (GtkWidget           *widget,
                                                                              gint                 width,
                                                                              gint                *minimum_height,
                                                                              gint                *natural_height);
static void                 gtk_list_box_real_get_preferred_width            (GtkWidget           *widget,
                                                                              gint                *minimum_width,
                                                                              gint                *natural_width);
static void                 gtk_list_box_real_get_preferred_width_for_height (GtkWidget           *widget,
                                                                              gint                 height,
                                                                              gint                *minimum_width,
                                                                              gint                *natural_width);

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

GtkWidget *
gtk_list_box_new (void)
{
  return g_object_new (GTK_TYPE_LIST_BOX, NULL);
}

static void
gtk_list_box_init (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv;

  list_box->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (list_box, GTK_TYPE_LIST_BOX, GtkListBoxPrivate);

  gtk_widget_set_has_window (GTK_WIDGET (list_box), TRUE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (list_box), TRUE);
  priv->selection_mode = GTK_SELECTION_SINGLE;
  priv->activate_single_click = TRUE;

  priv->children = g_sequence_new (NULL);
  priv->separator_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
}

static void
gtk_list_box_get_property (GObject    *obj,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkListBox *list_box = GTK_LIST_BOX (obj);

  switch (property_id)
    {
    case PROP_SELECTION_MODE:
      g_value_set_enum (value, list_box->priv->selection_mode);
      break;
    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      g_value_set_boolean (value, list_box->priv->activate_single_click);
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
  GtkListBox *list_box = GTK_LIST_BOX (obj);

  switch (property_id)
    {
    case PROP_SELECTION_MODE:
      gtk_list_box_set_selection_mode (list_box, g_value_get_enum (value));
      break;
    case PROP_ACTIVATE_ON_SINGLE_CLICK:
      gtk_list_box_set_activate_on_single_click (list_box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_list_box_finalize (GObject *obj)
{
  GtkListBox *list_box = GTK_LIST_BOX (obj);
  GtkListBoxPrivate *priv = list_box->priv;

  if (priv->sort_func_target_destroy_notify != NULL)
    priv->sort_func_target_destroy_notify (priv->sort_func_target);
  if (priv->filter_func_target_destroy_notify != NULL)
    priv->filter_func_target_destroy_notify (priv->filter_func_target);
  if (priv->update_separator_func_target_destroy_notify != NULL)
    priv->update_separator_func_target_destroy_notify (priv->update_separator_func_target);

  g_clear_object (&priv->adjustment);
  g_clear_object (&priv->drag_highlighted_row);

  g_sequence_free (priv->children);
  g_hash_table_unref (priv->separator_hash);

  G_OBJECT_CLASS (gtk_list_box_parent_class)->finalize (obj);
}

static void
gtk_list_box_class_init (GtkListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  g_type_class_add_private (klass, sizeof (GtkListBoxPrivate));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_LIST_BOX_ACCESSIBLE);

  object_class->get_property = gtk_list_box_get_property;
  object_class->set_property = gtk_list_box_set_property;
  object_class->finalize = gtk_list_box_finalize;
  widget_class->enter_notify_event = gtk_list_box_real_enter_notify_event;
  widget_class->leave_notify_event = gtk_list_box_real_leave_notify_event;
  widget_class->motion_notify_event = gtk_list_box_real_motion_notify_event;
  widget_class->button_press_event = gtk_list_box_real_button_press_event;
  widget_class->button_release_event = gtk_list_box_real_button_release_event;
  widget_class->show = gtk_list_box_real_show;
  widget_class->focus = gtk_list_box_real_focus;
  widget_class->draw = gtk_list_box_real_draw;
  widget_class->realize = gtk_list_box_real_realize;
  widget_class->compute_expand = gtk_list_box_real_compute_expand_internal;
  widget_class->get_request_mode = gtk_list_box_real_get_request_mode;
  widget_class->get_preferred_height = gtk_list_box_real_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_list_box_real_get_preferred_height_for_width;
  widget_class->get_preferred_width = gtk_list_box_real_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_list_box_real_get_preferred_width_for_height;
  widget_class->size_allocate = gtk_list_box_real_size_allocate;
  widget_class->drag_leave = gtk_list_box_real_drag_leave;
  container_class->add = gtk_list_box_real_add;
  container_class->remove = gtk_list_box_real_remove;
  container_class->forall = gtk_list_box_real_forall_internal;
  container_class->child_type = gtk_list_box_real_child_type;
  klass->activate_cursor_row = gtk_list_box_real_activate_cursor_row;
  klass->toggle_cursor_row = gtk_list_box_real_toggle_cursor_row;
  klass->move_cursor = gtk_list_box_real_move_cursor;
  klass->refilter = gtk_list_box_real_refilter;

  properties[PROP_SELECTION_MODE] =
    g_param_spec_enum ("selection-mode",
                       "Selection mode",
                       "The selection mode",
                       GTK_TYPE_SELECTION_MODE,
                       GTK_SELECTION_SINGLE,
                       G_PARAM_READWRITE);

  properties[PROP_ACTIVATE_ON_SINGLE_CLICK] =
    g_param_spec_boolean ("activate-on-single-click",
                          "Activate on Single Click",
                          "Activate row on a single click",
                          TRUE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);

  signals[ROW_SELECTED] =
    g_signal_new ("row-selected",
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkListBoxClass, row_selected),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  signals[ROW_ACTIVATED] =
    g_signal_new ("row-activated",
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkListBoxClass, row_activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_WIDGET);
  signals[ACTIVATE_CURSOR_ROW] =
    g_signal_new ("activate-cursor-row",
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxClass, activate_cursor_row),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[TOGGLE_CURSOR_ROW] =
    g_signal_new ("toggle-cursor-row",
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxClass, toggle_cursor_row),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  signals[MOVE_CURSOR] =
    g_signal_new ("move-cursor",
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListBoxClass, move_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_MOVEMENT_STEP, G_TYPE_INT);
  signals[REFILTER] =
    g_signal_new ("refilter",
                  GTK_TYPE_LIST_BOX,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkListBoxClass, refilter),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

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
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Up, GDK_CONTROL_MASK,
                                 GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Up, GDK_CONTROL_MASK,
                                 GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_Down, GDK_CONTROL_MASK,
                                 GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_list_box_add_move_binding (binding_set, GDK_KEY_KP_Down, GDK_CONTROL_MASK,
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
}

/**
 * gtk_list_box_get_selected_row:
 * @list_box: An #GtkListBox.
 *
 * Gets the selected row.
 *
 * Return value: (transfer none): The selected #GtkWidget.
 **/
GtkListBoxRow *
gtk_list_box_get_selected_row (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_val_if_fail (list_box != NULL, NULL);

  if (priv->selected_row != NULL)
    return priv->selected_row;

  return NULL;
}

/**
 * gtk_list_box_get_row_at_index:
 * @list_box: An #GtkListBox.
 * @index: the index of the row
 *
 * Gets the n:th child in the list (not counting separators).
 *
 * Return value: (transfer none): The child #GtkWidget.
 **/
GtkListBoxRow *
gtk_list_box_get_row_at_index (GtkListBox *list_box, gint index)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GSequenceIter *iter;

  g_return_val_if_fail (list_box != NULL, NULL);

  iter = g_sequence_get_iter_at_pos (priv->children, index);
  if (iter)
    return g_sequence_get (iter);

  return NULL;
}

/**
 * gtk_list_box_get_row_at_y:
 * @list_box: An #GtkListBox.
 * @y: position
 *
 * Gets the row at the y position.
 *
 * Return value: (transfer none): The row.
 **/
GtkListBoxRow *
gtk_list_box_get_row_at_y (GtkListBox *list_box, gint y)
{
  GtkListBoxRow *row, *found_row;
  GtkListBoxRowPrivate *row_priv;
  GtkListBoxPrivate *priv = list_box->priv;
  GSequenceIter *iter;

  g_return_val_if_fail (list_box != NULL, NULL);

  /* TODO: This should use g_sequence_search */

  found_row = NULL;
  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = (GtkListBoxRow*) g_sequence_get (iter);
      row_priv = row->priv;
      if (y >= row_priv->y && y < (row_priv->y + row_priv->height))
        {
          found_row = row;
          break;
        }
    }

  return found_row;
}


/**
 * gtk_list_box_select_row:
 * @list_box: a #GtkListBox
 * @row: (allow-none): The row to select or %NULL
 */
void
gtk_list_box_select_row (GtkListBox *list_box, GtkListBoxRow *row)
{
  g_return_if_fail (list_box != NULL);

  gtk_list_box_update_selected (list_box, row);
}

void
gtk_list_box_set_adjustment (GtkListBox *list_box,
                             GtkAdjustment *adjustment)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  g_object_ref (adjustment);
  if (priv->adjustment)
    g_object_unref (priv->adjustment);
  priv->adjustment = adjustment;
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (list_box),
                                       adjustment);
}

GtkAdjustment *
gtk_list_box_get_adjustment (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_val_if_fail (list_box != NULL, NULL);

  return priv->adjustment;
}

void
gtk_list_box_add_to_scrolled (GtkListBox *list_box,
                              GtkScrolledWindow *scrolled)
{
  g_return_if_fail (list_box != NULL);
  g_return_if_fail (scrolled != NULL);

  gtk_container_add (GTK_CONTAINER (scrolled), GTK_WIDGET (list_box));
  gtk_list_box_set_adjustment (list_box,
                               gtk_scrolled_window_get_vadjustment (scrolled));
}


void
gtk_list_box_set_selection_mode (GtkListBox *list_box, GtkSelectionMode mode)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  if (mode == GTK_SELECTION_MULTIPLE)
    {
      g_warning ("Multiple selections not supported");
      return;
    }

  if (priv->selection_mode == mode)
    return;

  priv->selection_mode = mode;
  if (mode == GTK_SELECTION_NONE)
    gtk_list_box_update_selected (list_box, NULL);

  g_object_notify_by_pspec (G_OBJECT (list_box), properties[PROP_SELECTION_MODE]);
}

GtkSelectionMode
gtk_list_box_get_selection_mode (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_val_if_fail (list_box != NULL, 0);

  return priv->selection_mode;
}

/**
 * gtk_list_box_set_filter_func:
 * @filter_func: (allow-none):
 * @user_data:
 * @destroy:
 */
void
gtk_list_box_set_filter_func (GtkListBox *list_box,
                              GtkListBoxFilterFunc filter_func,
                              gpointer user_data,
                              GDestroyNotify destroy)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  if (priv->filter_func_target_destroy_notify != NULL)
    priv->filter_func_target_destroy_notify (priv->filter_func_target);

  priv->filter_func = filter_func;
  priv->filter_func_target = user_data;
  priv->filter_func_target_destroy_notify = destroy;

  gtk_list_box_refilter (list_box);
}

/**
 * gtk_list_box_set_separator_func:
 * @update_separator: (allow-none):
 * @user_data:
 * @destroy:
 */
void
gtk_list_box_set_separator_func (GtkListBox *list_box,
                                 GtkListBoxUpdateSeparatorFunc update_separator,
                                 gpointer user_data,
                                 GDestroyNotify destroy)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  if (priv->update_separator_func_target_destroy_notify != NULL)
    priv->update_separator_func_target_destroy_notify (priv->update_separator_func_target);

  priv->update_separator_func = update_separator;
  priv->update_separator_func_target = user_data;
  priv->update_separator_func_target_destroy_notify = destroy;
  gtk_list_box_reseparate (list_box);
}

static void
gtk_list_box_real_refilter (GtkListBox *list_box)
{
  gtk_list_box_apply_filter_all (list_box);
  gtk_list_box_reseparate (list_box);
  gtk_widget_queue_resize (GTK_WIDGET (list_box));
}

void
gtk_list_box_refilter (GtkListBox *list_box)
{
  g_return_if_fail (list_box != NULL);

  g_signal_emit (list_box, signals[REFILTER], 0);
}

static gint
do_sort (GtkListBoxRow *a,
         GtkListBoxRow *b,
         GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  return priv->sort_func (a, b, priv->sort_func_target);
}

void
gtk_list_box_resort (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  g_sequence_sort (priv->children,
                   (GCompareDataFunc)do_sort, list_box);
  gtk_list_box_reseparate (list_box);
  gtk_widget_queue_resize (GTK_WIDGET (list_box));
}

void
gtk_list_box_reseparate (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GSequenceIter *iter;

  g_return_if_fail (list_box != NULL);

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    gtk_list_box_update_separator (list_box, iter);

  gtk_widget_queue_resize (GTK_WIDGET (list_box));
}

/**
 * gtk_list_box_set_sort_func:
 * @sort_func: (closure user_data) (allow-none):
 * @user_data:
 * @destroy:
 */
void
gtk_list_box_set_sort_func (GtkListBox *list_box,
                            GtkListBoxSortFunc sort_func,
                            gpointer user_data,
                            GDestroyNotify destroy)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  if (priv->sort_func_target_destroy_notify != NULL)
    priv->sort_func_target_destroy_notify (priv->sort_func_target);

  priv->sort_func = sort_func;
  priv->sort_func_target = user_data;
  priv->sort_func_target_destroy_notify = destroy;
  gtk_list_box_resort (list_box);
}

static void
gtk_list_box_got_row_changed (GtkListBox *list_box, GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GSequenceIter *prev_next, *next;

  g_return_if_fail (list_box != NULL);
  g_return_if_fail (row != NULL);

  prev_next = gtk_list_box_get_next_visible (list_box, row->priv->iter);
  if (priv->sort_func != NULL)
    {
      g_sequence_sort_changed (row->priv->iter,
                               (GCompareDataFunc)do_sort,
                               list_box);
      gtk_widget_queue_resize (GTK_WIDGET (list_box));
    }
  gtk_list_box_apply_filter (list_box, row);
  if (gtk_widget_get_visible (GTK_WIDGET (list_box)))
    {
      next = gtk_list_box_get_next_visible (list_box, row->priv->iter);
      gtk_list_box_update_separator (list_box, row->priv->iter);
      gtk_list_box_update_separator (list_box, next);
      gtk_list_box_update_separator (list_box, prev_next);
    }
}

void
gtk_list_box_set_activate_on_single_click (GtkListBox *list_box,
                                           gboolean single)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  single = single != FALSE;

  if (priv->activate_single_click == single)
    return;

  priv->activate_single_click = single;

  g_object_notify_by_pspec (G_OBJECT (list_box), properties[PROP_ACTIVATE_ON_SINGLE_CLICK]);
}

static void
gtk_list_box_add_move_binding (GtkBindingSet *binding_set,
                               guint keyval,
                               GdkModifierType modmask,
                               GtkMovementStep step,
                               gint count)
{
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move-cursor", (guint) 2, GTK_TYPE_MOVEMENT_STEP, step, G_TYPE_INT, count, NULL);

  if ((modmask & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    return;

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK,
                                "move-cursor", (guint) 2, GTK_TYPE_MOVEMENT_STEP, step, G_TYPE_INT, count, NULL);
}

static void
gtk_list_box_update_cursor (GtkListBox *list_box,
                            GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;

  priv->cursor_row = row;
  gtk_widget_grab_focus (GTK_WIDGET (row));
  gtk_widget_queue_draw (GTK_WIDGET (row));
  _gtk_list_box_accessible_update_cursor (list_box, row);
}

static void
gtk_list_box_update_selected (GtkListBox *list_box,
                              GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;

  if (row != priv->selected_row &&
      (row == NULL || priv->selection_mode != GTK_SELECTION_NONE))
    {
      if (priv->selected_row)
        gtk_widget_unset_state_flags (GTK_WIDGET (priv->selected_row),
                                      GTK_STATE_FLAG_SELECTED);
      priv->selected_row = row;
      if (priv->selected_row)
        gtk_widget_set_state_flags (GTK_WIDGET (priv->selected_row),
                                    GTK_STATE_FLAG_SELECTED,
                                    FALSE);
      g_signal_emit (list_box, signals[ROW_SELECTED], 0,
                     priv->selected_row);
      gtk_widget_queue_draw (GTK_WIDGET (list_box));
    }
  _gtk_list_box_accessible_selection_changed (list_box);
  if (row != NULL)
    gtk_list_box_update_cursor (list_box, row);
}

static void
gtk_list_box_select_and_activate (GtkListBox *list_box, GtkListBoxRow *row)
{
  gtk_list_box_update_selected (list_box, row);

  if (row != NULL)
    g_signal_emit (list_box, signals[ROW_ACTIVATED], 0, row);
}

static void
gtk_list_box_update_prelight (GtkListBox *list_box, GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;

  if (row != priv->prelight_row)
    {
      if (priv->prelight_row)
        gtk_widget_unset_state_flags (GTK_WIDGET (priv->prelight_row),
                                      GTK_STATE_FLAG_PRELIGHT);
      priv->prelight_row = row;
      if (priv->prelight_row)
        gtk_widget_set_state_flags (GTK_WIDGET (priv->prelight_row),
                                    GTK_STATE_FLAG_PRELIGHT,
                                    FALSE);
      gtk_widget_queue_draw (GTK_WIDGET (list_box));
    }
}

static void
gtk_list_box_update_active (GtkListBox *list_box, GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;
  gboolean val;

  val = priv->active_row == row;
  if (priv->active_row != NULL &&
      val != priv->active_row_active)
    {
      priv->active_row_active = val;
      if (priv->active_row_active)
        gtk_widget_set_state_flags (GTK_WIDGET (priv->active_row),
                                    GTK_STATE_FLAG_ACTIVE,
                                    FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (priv->active_row),
                                      GTK_STATE_FLAG_ACTIVE);
      gtk_widget_queue_draw (GTK_WIDGET (list_box));
    }
}

static gboolean
gtk_list_box_real_enter_notify_event (GtkWidget *widget,
                                      GdkEventCrossing *event)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxRow *row;


  if (event->window != gtk_widget_get_window (GTK_WIDGET (list_box)))
    return FALSE;

  row = gtk_list_box_get_row_at_y (list_box, event->y);
  gtk_list_box_update_prelight (list_box, row);
  gtk_list_box_update_active (list_box, row);

  return FALSE;
}

static gboolean
gtk_list_box_real_leave_notify_event (GtkWidget *widget,
                                      GdkEventCrossing *event)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxRow *row = NULL;

  if (event->window != gtk_widget_get_window (GTK_WIDGET (list_box)))
    return FALSE;

  if (event->detail != GDK_NOTIFY_INFERIOR)
    row = NULL;
  else
    row = gtk_list_box_get_row_at_y (list_box, event->y);

  gtk_list_box_update_prelight (list_box, row);
  gtk_list_box_update_active (list_box, row);

  return FALSE;
}

static gboolean
gtk_list_box_real_motion_notify_event (GtkWidget *widget,
                                       GdkEventMotion *event)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxRow *row;
  GdkWindow *window, *event_window;
  gint relative_y;
  gdouble parent_y;

  window = gtk_widget_get_window (GTK_WIDGET (list_box));
  event_window = event->window;
  relative_y = event->y;

  while ((event_window != NULL) && (event_window != window))
    {
      gdk_window_coords_to_parent (event_window, 0, relative_y, NULL, &parent_y);
      relative_y = parent_y;
      event_window = gdk_window_get_effective_parent (event_window);
    }

  row = gtk_list_box_get_row_at_y (list_box, relative_y);
  gtk_list_box_update_prelight (list_box, row);
  gtk_list_box_update_active (list_box, row);

  return FALSE;
}

static gboolean
gtk_list_box_real_button_press_event (GtkWidget *widget,
                                      GdkEventButton *event)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = list_box->priv;

  if (event->button == GDK_BUTTON_PRIMARY)
    {
      GtkListBoxRow *row;
      row = gtk_list_box_get_row_at_y (list_box, event->y);
      if (row != NULL)
        {
          priv->active_row = row;
          priv->active_row_active = TRUE;
          gtk_widget_set_state_flags (GTK_WIDGET (priv->active_row),
                                      GTK_STATE_FLAG_ACTIVE,
                                      FALSE);
          gtk_widget_queue_draw (GTK_WIDGET (list_box));
          if (event->type == GDK_2BUTTON_PRESS &&
              !priv->activate_single_click)
            g_signal_emit (list_box, signals[ROW_ACTIVATED], 0,
                           row);

        }
      /* TODO:
         Should mark as active while down,
         and handle grab breaks */
    }

  return FALSE;
}

static gboolean
gtk_list_box_real_button_release_event (GtkWidget *widget,
                                        GdkEventButton *event)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = list_box->priv;

  if (event->button == GDK_BUTTON_PRIMARY)
    {
      if (priv->active_row != NULL &&
          priv->active_row_active)
        {
          if (priv->activate_single_click)
            gtk_list_box_select_and_activate (list_box, priv->active_row);
          else
            gtk_list_box_update_selected (list_box, priv->active_row);
        }
      if (priv->active_row_active)
        gtk_widget_unset_state_flags (GTK_WIDGET (priv->active_row),
                                      GTK_STATE_FLAG_ACTIVE);
      priv->active_row = NULL;
      priv->active_row_active = FALSE;
      gtk_widget_queue_draw (GTK_WIDGET (list_box));
  }

  return FALSE;
}

static void
gtk_list_box_real_show (GtkWidget *widget)
{
  GtkListBox * list_box = GTK_LIST_BOX (widget);

  gtk_list_box_reseparate (list_box);

  GTK_WIDGET_CLASS (gtk_list_box_parent_class)->show (widget);
}

static gboolean
gtk_list_box_real_focus (GtkWidget* widget, GtkDirectionType direction)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = list_box->priv;
  GtkWidget *focus_child;
  GtkListBoxRow *next_focus_row;

  focus_child = gtk_container_get_focus_child ((GtkContainer*) list_box);
  next_focus_row = NULL;
  if (focus_child != NULL)
    {
      GSequenceIter* i;

      if (gtk_widget_child_focus (focus_child, direction))
        return TRUE;

      if (direction == GTK_DIR_UP || direction == GTK_DIR_TAB_BACKWARD)
        {
          i = gtk_list_box_get_previous_visible (list_box, priv->cursor_row->priv->iter);
          if (i != NULL)
            next_focus_row = g_sequence_get (i);
        }
      else if (direction == GTK_DIR_DOWN || direction == GTK_DIR_TAB_FORWARD)
        {
          i = gtk_list_box_get_next_visible (list_box, priv->cursor_row->priv->iter);
          if (!g_sequence_iter_is_end (i))
            next_focus_row = g_sequence_get (i);
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
            next_focus_row = gtk_list_box_get_last_visible (list_box);
          break;
        default:
          next_focus_row = priv->selected_row;
          if (next_focus_row == NULL)
            next_focus_row =
              gtk_list_box_get_first_visible (list_box);
          break;
        }
    }

  if (next_focus_row == NULL)
    {
      if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
        {
          if (gtk_widget_keynav_failed (GTK_WIDGET (list_box), direction))
            return TRUE;
        }

      return FALSE;
    }

  if (gtk_widget_child_focus (GTK_WIDGET (next_focus_row), direction))
    return TRUE;

  return TRUE;
}

static gboolean
gtk_list_box_real_draw (GtkWidget* widget, cairo_t* cr)
{
  GtkAllocation allocation;
  GtkStyleContext *context;

  gtk_widget_get_allocation (widget, &allocation);
  context = gtk_widget_get_style_context (widget);
  gtk_render_background (context, cr, 0, 0, allocation.width, allocation.height);

  GTK_WIDGET_CLASS (gtk_list_box_parent_class)->draw (widget, cr);

  return TRUE;
}

static void
gtk_list_box_real_realize (GtkWidget* widget)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkAllocation allocation;
  GdkWindowAttr attributes = {0};
  GdkWindow *window;

  gtk_widget_get_allocation (GTK_WIDGET (list_box), &allocation);
  gtk_widget_set_realized (GTK_WIDGET (list_box), TRUE);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (GTK_WIDGET (list_box)) |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK |
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;
  attributes.wclass = GDK_INPUT_OUTPUT;

  window = gdk_window_new (gtk_widget_get_parent_window (GTK_WIDGET (list_box)),
                           &attributes, GDK_WA_X | GDK_WA_Y);
  gtk_style_context_set_background (gtk_widget_get_style_context (GTK_WIDGET (list_box)), window);
  gdk_window_set_user_data (window, (GObject*) list_box);
  gtk_widget_set_window (GTK_WIDGET (list_box), window); /* Passes ownership */
}


static void
gtk_list_box_apply_filter (GtkListBox *list_box, GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;
  gboolean do_show;

  do_show = TRUE;
  if (priv->filter_func != NULL)
    do_show = priv->filter_func (row, priv->filter_func_target);

  gtk_widget_set_child_visible (GTK_WIDGET (row), do_show);
}

static void
gtk_list_box_apply_filter_all (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GtkListBoxRow *row;
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      gtk_list_box_apply_filter (list_box, row);
    }
}

/* Children are visible if they are shown by the app (visible)
   and not filtered out (child_visible) by the listbox */
static gboolean
row_is_visible (GtkListBoxRow *row)
{
  return gtk_widget_get_visible (GTK_WIDGET (row)) && gtk_widget_get_child_visible (GTK_WIDGET (row));
}

static GtkListBoxRow *
gtk_list_box_get_first_visible (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GtkListBoxRow *row;
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
        row = g_sequence_get (iter);
        if (row_is_visible (row))
          return row;
    }

  return NULL;
}


static GtkListBoxRow *
gtk_list_box_get_last_visible (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GtkListBoxRow *row;
  GSequenceIter *iter;

  iter = g_sequence_get_end_iter (priv->children);
  while (!g_sequence_iter_is_begin (iter))
    {
      iter = g_sequence_iter_prev (iter);
      row = g_sequence_get (iter);
      if (row_is_visible (row))
        return row;
    }

  return NULL;
}

static GSequenceIter*
gtk_list_box_get_previous_visible (GtkListBox *list_box,
                                   GSequenceIter* iter)
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

static GSequenceIter*
gtk_list_box_get_next_visible (GtkListBox *list_box, GSequenceIter* iter)
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


static void
gtk_list_box_update_separator (GtkListBox *list_box, GSequenceIter* iter)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GtkListBoxRow *row;
  GSequenceIter *before_iter;
  GtkListBoxRow *before_row;
  GtkWidget *old_separator;

  if (iter == NULL || g_sequence_iter_is_end (iter))
    return;

  row = g_sequence_get (iter);
  before_iter = gtk_list_box_get_previous_visible (list_box, iter);
  if (row)
    g_object_ref (row);
  before_row = NULL;
  if (before_iter != NULL)
    {
      before_row = g_sequence_get (before_iter);
      if (before_row)
        g_object_ref (before_row);
    }

  if (priv->update_separator_func != NULL &&
      row_is_visible (row))
    {
      old_separator = row->priv->separator;
      if (old_separator)
        g_object_ref (old_separator);
      priv->update_separator_func (row,
                                   before_row,
                                   priv->update_separator_func_target);
      if (old_separator != row->priv->separator)
        {
          if (old_separator != NULL)
            {
              gtk_widget_unparent (old_separator);
              g_hash_table_remove (priv->separator_hash, old_separator);
            }
          if (row->priv->separator != NULL)
            {
              g_hash_table_insert (priv->separator_hash, row->priv->separator, row);
              gtk_widget_set_parent (row->priv->separator, GTK_WIDGET (list_box));
              gtk_widget_show (row->priv->separator);
            }
          gtk_widget_queue_resize (GTK_WIDGET (list_box));
        }
      if (old_separator)
        g_object_unref (old_separator);
    }
  else
    {
      if (row->priv->separator != NULL)
        {
          g_hash_table_remove (priv->separator_hash, row->priv->separator);
          gtk_widget_unparent (row->priv->separator);
          gtk_list_box_row_set_separator (row, NULL);
          gtk_widget_queue_resize (GTK_WIDGET (list_box));
        }
    }
  if (before_row)
    g_object_unref (before_row);
  if (row)
    g_object_unref (row);
}

static void
gtk_list_box_row_visibility_changed (GtkListBox *list_box, GtkListBoxRow *row)
{
  if (gtk_widget_get_visible (GTK_WIDGET (list_box)))
    {
      gtk_list_box_update_separator (list_box, row->priv->iter);
      gtk_list_box_update_separator (list_box,
                                     gtk_list_box_get_next_visible (list_box, row->priv->iter));
    }
}

static void
gtk_list_box_real_add (GtkContainer* container, GtkWidget *child)
{
  GtkListBox *list_box = GTK_LIST_BOX (container);
  GtkListBoxPrivate *priv = list_box->priv;
  GtkListBoxRow *row;
  GSequenceIter* iter = NULL;

  if (GTK_IS_LIST_BOX_ROW (child))
    row = GTK_LIST_BOX_ROW (child);
  else
    {
      row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
      gtk_widget_show (GTK_WIDGET (row));
      gtk_container_add (GTK_CONTAINER (row), child);
    }

  if (priv->sort_func != NULL)
    iter = g_sequence_insert_sorted (priv->children, row,
                                     (GCompareDataFunc)do_sort, list_box);
  else
    iter = g_sequence_append (priv->children, row);

  row->priv->iter = iter;
  gtk_widget_set_parent (GTK_WIDGET (row), GTK_WIDGET (list_box));
  gtk_list_box_apply_filter (list_box, row);
  gtk_list_box_row_visibility_changed (list_box, row);
}

static void
gtk_list_box_real_remove (GtkContainer* container, GtkWidget* child)
{
  GtkListBox *list_box = GTK_LIST_BOX (container);
  GtkListBoxPrivate *priv = list_box->priv;
  gboolean was_visible;
  GtkListBoxRow *row;
  GSequenceIter *next;

  g_return_if_fail (child != NULL);
  was_visible = gtk_widget_get_visible (child);

  if (!GTK_IS_LIST_BOX_ROW (child))
    {
      row = g_hash_table_lookup (priv->separator_hash, child);
      if (row != NULL)
        {
          g_hash_table_remove (priv->separator_hash, child);
          g_clear_object (&row->priv->separator);
          gtk_widget_unparent (child);
          if (was_visible && gtk_widget_get_visible (GTK_WIDGET (list_box)))
            gtk_widget_queue_resize (GTK_WIDGET (list_box));
        }
      else
        {
          g_warning ("Tried to remove non-child %p\n", child);
        }
      return;
    }

  row = GTK_LIST_BOX_ROW (child);
  if (g_sequence_iter_get_sequence (row->priv->iter) != priv->children)
    {
      g_warning ("Tried to remove non-child %p\n", child);
      return;
    }

  if (row->priv->separator != NULL)
    {
      g_hash_table_remove (priv->separator_hash, row->priv->separator);
      gtk_widget_unparent (row->priv->separator);
      g_clear_object (&row->priv->separator);
    }

  if (row == priv->selected_row)
      gtk_list_box_update_selected (list_box, NULL);
  if (row == priv->prelight_row) {
    gtk_widget_unset_state_flags (GTK_WIDGET (priv->prelight_row),
                                  GTK_STATE_FLAG_PRELIGHT);
    priv->prelight_row = NULL;
  }
  if (row == priv->cursor_row)
    priv->cursor_row = NULL;
  if (row == priv->active_row) {
    if (priv->active_row_active)
      gtk_widget_unset_state_flags (GTK_WIDGET (priv->active_row),
                                    GTK_STATE_FLAG_ACTIVE);
    priv->active_row = NULL;
  }

  if (row == priv->drag_highlighted_row)
    gtk_list_box_drag_unhighlight_row (list_box);

  next = gtk_list_box_get_next_visible (list_box, row->priv->iter);
  gtk_widget_unparent (child);
  g_sequence_remove (row->priv->iter);
  if (gtk_widget_get_visible (GTK_WIDGET (list_box)))
    gtk_list_box_update_separator (list_box, next);

  if (was_visible && gtk_widget_get_visible (GTK_WIDGET (list_box)))
    gtk_widget_queue_resize (GTK_WIDGET (list_box));
}


static void
gtk_list_box_real_forall_internal (GtkContainer* container,
                                   gboolean include_internals,
                                   GtkCallback callback,
                                   void* callback_target)
{
  GtkListBox *list_box = GTK_LIST_BOX (container);
  GtkListBoxPrivate *priv = list_box->priv;
  GSequenceIter *iter;
  GtkListBoxRow *row;

  iter = g_sequence_get_begin_iter (priv->children);
  while (!g_sequence_iter_is_end (iter))
    {
      row = g_sequence_get (iter);
      iter = g_sequence_iter_next (iter);
      if (row->priv->separator != NULL && include_internals)
        callback (row->priv->separator, callback_target);
      callback (GTK_WIDGET (row), callback_target);
    }
}

static void
gtk_list_box_real_compute_expand_internal (GtkWidget* widget,
                                           gboolean* hexpand,
                                           gboolean* vexpand)
{
  GTK_WIDGET_CLASS (gtk_list_box_parent_class)->compute_expand (widget,
                                                                hexpand, vexpand);

  /* We don't expand vertically beyound the minimum size */
  if (vexpand)
    *vexpand = FALSE;
}

static GType
gtk_list_box_real_child_type (GtkContainer* container)
{
  /* We really support any type but we wrap it in a row. But that is more
     like a C helper function, in an abstract sense we only support
     row children, so that is what tools accessing this should use. */
  return GTK_TYPE_LIST_BOX_ROW;
}

static GtkSizeRequestMode
gtk_list_box_real_get_request_mode (GtkWidget* widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_list_box_real_get_preferred_height (GtkWidget* widget,
                                        gint* minimum_height,
                                        gint* natural_height)
{
  gint natural_width;
  gtk_list_box_real_get_preferred_width (widget, NULL, &natural_width);
  gtk_list_box_real_get_preferred_height_for_width (widget, natural_width,
                                                    minimum_height, natural_height);
}

static void
gtk_list_box_real_get_preferred_height_for_width (GtkWidget* widget, gint width,
                                                  gint* minimum_height_out, gint* natural_height_out)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = list_box->priv;
  GSequenceIter *iter;
  gint minimum_height;
  gint natural_height;

  minimum_height = 0;

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      GtkListBoxRow *row;
      gint row_min = 0;

      row = g_sequence_get (iter);
      if (!row_is_visible (row))
        continue;

      if (row->priv->separator != NULL)
        {
          gtk_widget_get_preferred_height_for_width (row->priv->separator, width, &row_min, NULL);
          minimum_height += row_min;
        }
      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (row), width,
                                                 &row_min, NULL);
      minimum_height += row_min;
    }

  /* We always allocate the minimum height, since handling
     expanding rows is way too costly, and unlikely to
     be used, as lists are generally put inside a scrolling window
     anyway.
  */
  natural_height = minimum_height;
  if (minimum_height_out)
    *minimum_height_out = minimum_height;
  if (natural_height_out)
    *natural_height_out = natural_height;
}

static void
gtk_list_box_real_get_preferred_width (GtkWidget* widget, gint* minimum_width_out, gint* natural_width_out)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = list_box->priv;
  gint minimum_width;
  gint natural_width;
  GSequenceIter *iter;
  GtkListBoxRow *row;
  gint row_min;
  gint row_nat;

  minimum_width = 0;
  natural_width = 0;

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      if (!row_is_visible (row))
        continue;

      gtk_widget_get_preferred_width (GTK_WIDGET (row), &row_min, &row_nat);
      minimum_width = MAX (minimum_width, row_min);
      natural_width = MAX (natural_width, row_nat);

      if (row->priv->separator != NULL)
        {
          gtk_widget_get_preferred_width (row->priv->separator, &row_min, &row_nat);
          minimum_width = MAX (minimum_width, row_min);
          natural_width = MAX (natural_width, row_nat);
        }
    }

  if (minimum_width_out)
    *minimum_width_out = minimum_width;
  if (natural_width_out)
    *natural_width_out = natural_width;
}

static void
gtk_list_box_real_get_preferred_width_for_height (GtkWidget *widget, gint height,
                                                  gint *minimum_width, gint *natural_width)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  gtk_list_box_real_get_preferred_width (GTK_WIDGET (list_box), minimum_width, natural_width);
}

static void
gtk_list_box_real_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);
  GtkListBoxPrivate *priv = list_box->priv;
  GtkAllocation child_allocation;
  GtkAllocation separator_allocation;
  GtkListBoxRow *row;
  GdkWindow *window;
  GSequenceIter *iter;
  int child_min;


  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = 0;
  child_allocation.height = 0;

  separator_allocation.x = 0;
  separator_allocation.y = 0;
  separator_allocation.width = 0;
  separator_allocation.height = 0;

  gtk_widget_set_allocation (GTK_WIDGET (list_box), allocation);
  window = gtk_widget_get_window (GTK_WIDGET (list_box));
  if (window != NULL)
    gdk_window_move_resize (window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation->width;
  separator_allocation.x = 0;
  separator_allocation.width = allocation->width;

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      row = g_sequence_get (iter);
      if (!row_is_visible (row))
        {
          row->priv->y = child_allocation.y;
          row->priv->height = 0;
          continue;
        }

      if (row->priv->separator != NULL)
        {
          gtk_widget_get_preferred_height_for_width (row->priv->separator,
                                                     allocation->width, &child_min, NULL);
          separator_allocation.height = child_min;
          separator_allocation.y = child_allocation.y;
          gtk_widget_size_allocate (row->priv->separator,
                                    &separator_allocation);
          child_allocation.y += child_min;
        }

      row->priv->y = child_allocation.y;

      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (row), child_allocation.width, &child_min, NULL);
      child_allocation.height = child_min;

      row->priv->height = child_allocation.height;
      gtk_widget_size_allocate (GTK_WIDGET (row), &child_allocation);

      child_allocation.y += child_min;
    }
}

void
gtk_list_box_drag_unhighlight_row (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  g_return_if_fail (list_box != NULL);

  if (priv->drag_highlighted_row == NULL)
    return;

  gtk_drag_unhighlight (GTK_WIDGET (priv->drag_highlighted_row));
  g_clear_object (&priv->drag_highlighted_row);
}


void
gtk_list_box_drag_highlight_row (GtkListBox *list_box, GtkListBoxRow *row)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GtkListBoxRow *old_highlight;

  g_return_if_fail (list_box != NULL);
  g_return_if_fail (row != NULL);

  if (priv->drag_highlighted_row == row)
    return;

  gtk_list_box_drag_unhighlight_row (list_box);
  gtk_drag_highlight (GTK_WIDGET (row));

  old_highlight = priv->drag_highlighted_row;
  priv->drag_highlighted_row = g_object_ref (row);
  if (old_highlight)
    g_object_unref (old_highlight);
}

static void
gtk_list_box_real_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time_)
{
  GtkListBox *list_box = GTK_LIST_BOX (widget);

  gtk_list_box_drag_unhighlight_row (list_box);
}

static void
gtk_list_box_real_activate_cursor_row (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  gtk_list_box_select_and_activate (list_box, priv->cursor_row);
}

static void
gtk_list_box_real_toggle_cursor_row (GtkListBox *list_box)
{
  GtkListBoxPrivate *priv = list_box->priv;

  if (priv->cursor_row == NULL)
    return;

  if (priv->selection_mode == GTK_SELECTION_SINGLE &&
      priv->selected_row == priv->cursor_row)
    gtk_list_box_update_selected (list_box, NULL);
  else
    gtk_list_box_select_and_activate (list_box, priv->cursor_row);
}

static void
gtk_list_box_real_move_cursor (GtkListBox *list_box,
                               GtkMovementStep step,
                               gint count)
{
  GtkListBoxPrivate *priv = list_box->priv;
  GdkModifierType state;
  gboolean modify_selection_pressed;
  GtkListBoxRow *row;
  GdkModifierType modify_mod_mask;
  GtkListBoxRow *prev;
  GtkListBoxRow *next;
  gint page_size;
  GSequenceIter *iter;
  gint start_y;
  gint end_y;

  modify_selection_pressed = FALSE;

  if (gtk_get_current_event_state (&state))
    {
      modify_mod_mask = gtk_widget_get_modifier_mask (GTK_WIDGET (list_box),
                                                      GDK_MODIFIER_INTENT_MODIFY_SELECTION);
      if ((state & modify_mod_mask) == modify_mod_mask)
        modify_selection_pressed = TRUE;
    }

  row = NULL;
  switch (step)
    {
    case GTK_MOVEMENT_BUFFER_ENDS:
      if (count < 0)
        row = gtk_list_box_get_first_visible (list_box);
      else
        row = gtk_list_box_get_last_visible (list_box);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      if (priv->cursor_row != NULL)
        {
          iter = priv->cursor_row->priv->iter;

          while (count < 0  && iter != NULL)
            {
              iter = gtk_list_box_get_previous_visible (list_box, iter);
              count = count + 1;
            }
          while (count > 0  && iter != NULL)
            {
              iter = gtk_list_box_get_next_visible (list_box, iter);
              count = count - 1;
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
          start_y = priv->cursor_row->priv->y;
          end_y = start_y;
          iter = priv->cursor_row->priv->iter;

          row = priv->cursor_row;
          if (count < 0)
            {
              /* Up */
              while (iter != NULL && !g_sequence_iter_is_begin (iter))
                {
                  iter = gtk_list_box_get_previous_visible (list_box, iter);
                  if (iter == NULL)
                    break;

                  prev = g_sequence_get (iter);
                  if (prev->priv->y < start_y - page_size)
                    break;

                  row = prev;
                }
            }
          else
            {
              /* Down */
              while (iter != NULL && !g_sequence_iter_is_end (iter))
                {
                  iter = gtk_list_box_get_next_visible (list_box, iter);
                  if (g_sequence_iter_is_end (iter))
                    break;

                  next = g_sequence_get (iter);
                  if (next->priv->y > start_y + page_size)
                    break;

                  row = next;
                }
            }
          end_y = row->priv->y;
          if (end_y != start_y && priv->adjustment != NULL)
            gtk_adjustment_set_value (priv->adjustment,
                                      gtk_adjustment_get_value (priv->adjustment) +
                                      end_y - start_y);
        }
      break;
    default:
      return;
    }

  if (row == NULL || row == priv->cursor_row)
    {
      GtkDirectionType direction = count < 0 ? GTK_DIR_UP : GTK_DIR_DOWN;

      if (!gtk_widget_keynav_failed (GTK_WIDGET (list_box), direction))
        {
          GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (list_box));

          if (toplevel)
            gtk_widget_child_focus (toplevel,
                                    direction == GTK_DIR_UP ?
                                    GTK_DIR_TAB_BACKWARD :
                                    GTK_DIR_TAB_FORWARD);

        }

      return;
    }

  gtk_list_box_update_cursor (list_box, row);
  if (!modify_selection_pressed)
    gtk_list_box_update_selected (list_box, row);
}


GtkWidget *
gtk_list_box_row_new (void)
{
  return g_object_new (GTK_TYPE_LIST_BOX_ROW, NULL);
}

static void
gtk_list_box_row_init (GtkListBoxRow *row)
{
  GtkListBoxRowPrivate *priv;

  row->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (row, GTK_TYPE_LIST_BOX_ROW, GtkListBoxRowPrivate);

  gtk_widget_set_can_focus (GTK_WIDGET (row), TRUE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (row), TRUE);
}

static void
gtk_list_box_row_get_property (GObject    *obj,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
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
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
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

static void
gtk_list_box_row_set_focus (GtkListBoxRow *row)
{
  GtkListBox *list_box = gtk_list_box_row_get_box (row);
  GdkModifierType state = 0;
  gboolean modify_selection_pressed;

  modify_selection_pressed = FALSE;
  if (gtk_get_current_event_state (&state))
    {
      GdkModifierType modify_mod_mask;
      modify_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (list_box),
                                      GDK_MODIFIER_INTENT_MODIFY_SELECTION);
      if ((state & modify_mod_mask) == modify_mod_mask)
        modify_selection_pressed = TRUE;
    }

  if (modify_selection_pressed)
    gtk_list_box_update_cursor (list_box, row);
  else
    gtk_list_box_update_selected (list_box, row);
}

static gboolean
gtk_list_box_row_real_focus (GtkWidget* widget, GtkDirectionType direction)
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
  else if (gtk_container_get_focus_child (GTK_CONTAINER (row)) != NULL)
    {
      /* Child has focus, always navigate inside it first */

      if (gtk_widget_child_focus (child, direction))
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
gtk_list_box_row_real_show (GtkWidget *widget)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkListBox *list_box;;

  GTK_WIDGET_CLASS (gtk_list_box_row_parent_class)->show (widget);

  list_box = gtk_list_box_row_get_box (row);
  if (list_box)
    gtk_list_box_row_visibility_changed (list_box, row);
}

static void
gtk_list_box_row_real_hide (GtkWidget *widget)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkListBox *list_box;

  GTK_WIDGET_CLASS (gtk_list_box_row_parent_class)->hide (widget);

  list_box = gtk_list_box_row_get_box (row);
  if (list_box)
    gtk_list_box_row_visibility_changed (list_box, row);
}

static gboolean
gtk_list_box_row_real_draw (GtkWidget* widget, cairo_t* cr)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkAllocation allocation = {0};
  GtkStyleContext* context;
  GtkStateFlags state;
  GtkBorder border;
  gint focus_pad;

  gtk_widget_get_allocation (widget, &allocation);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_render_background (context, cr, (gdouble) 0, (gdouble) 0, (gdouble) allocation.width, (gdouble) allocation.height);
  gtk_render_frame (context, cr, (gdouble) 0, (gdouble) 0, (gdouble) allocation.width, (gdouble) allocation.height);

  if (gtk_widget_has_visible_focus (GTK_WIDGET (row)))
    {
      gtk_style_context_get_border (context, state, &border);

      gtk_style_context_get_style (context,
                                   "focus-padding", &focus_pad,
                                   NULL);
      gtk_render_focus (context, cr, border.left + focus_pad, border.top + focus_pad,
                        allocation.width - 2 * focus_pad - border.left - border.right,
                        allocation.height - 2 * focus_pad - border.top - border.bottom);
    }

  GTK_WIDGET_CLASS (gtk_list_box_row_parent_class)->draw (widget, cr);

  return TRUE;
}

static void
gtk_list_box_row_get_full_border (GtkListBoxRow *row,
                                  GtkBorder *full_border)
{
  GtkWidget *widget = GTK_WIDGET (row);
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding, border;
  int focus_width, focus_pad;

  context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (context);

  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get_border (context, state, &border);
  gtk_style_context_get_style (context,
                               "focus-line-width", &focus_width,
                               "focus-padding", &focus_pad,
                               NULL);

  full_border->left = padding.left + border.left + focus_width + focus_pad;
  full_border->right = padding.right + border.right + focus_width + focus_pad;
  full_border->top = padding.top + border.top + focus_width + focus_pad;
  full_border->bottom = padding.bottom + border.bottom + focus_width + focus_pad;
}

static void gtk_list_box_row_real_get_preferred_height_for_width (GtkWidget* widget, gint width,
                                                                  gint* minimum_height_out, gint* natural_height_out);
static void gtk_list_box_row_real_get_preferred_width (GtkWidget* widget,
                                                       gint* minimum_width_out, gint* natural_width_out);

static void
gtk_list_box_row_real_get_preferred_height (GtkWidget* widget,
                                            gint* minimum_height,
                                            gint* natural_height)
{
  gint natural_width;
  gtk_list_box_row_real_get_preferred_width (widget, NULL, &natural_width);
  gtk_list_box_row_real_get_preferred_height_for_width (widget, natural_width,
                                                        minimum_height, natural_height);
}

static void
gtk_list_box_row_real_get_preferred_height_for_width (GtkWidget* widget, gint width,
                                                      gint* minimum_height_out, gint* natural_height_out)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkWidget *child;
  gint child_min = 0, child_natural = 0;
  GtkBorder full_border;

  gtk_list_box_row_get_full_border (row, &full_border);

  child = gtk_bin_get_child (GTK_BIN (row));
  if (child && gtk_widget_get_visible (child))
      gtk_widget_get_preferred_height_for_width (child, width - full_border.left - full_border.right,
                                                 &child_min, &child_natural);

  if (minimum_height_out)
    *minimum_height_out = full_border.top + child_min + full_border.bottom;
  if (natural_height_out)
    *natural_height_out = full_border.top + child_natural + full_border.bottom;
}

static void
gtk_list_box_row_real_get_preferred_width (GtkWidget* widget, gint* minimum_width_out, gint* natural_width_out)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkWidget *child;
  gint child_min = 0, child_natural = 0;
  GtkBorder full_border;

  gtk_list_box_row_get_full_border (row, &full_border);

  child = gtk_bin_get_child (GTK_BIN (row));
  if (child && gtk_widget_get_visible (child))
      gtk_widget_get_preferred_width (child,
                                      &child_min, &child_natural);

  if (minimum_width_out)
    *minimum_width_out = full_border.left + child_min + full_border.right;
  if (natural_width_out)
    *natural_width_out = full_border.left + child_natural + full_border.bottom;
}

static void
gtk_list_box_row_real_get_preferred_width_for_height (GtkWidget *widget, gint height,
                                                      gint *minimum_width, gint *natural_width)
{
  gtk_list_box_row_real_get_preferred_width (widget, minimum_width, natural_width);
}

static void
gtk_list_box_row_real_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (widget);
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (GTK_BIN (row));
  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation child_allocation;
      GtkBorder border;

      gtk_list_box_row_get_full_border (row, &border);

      child_allocation.x = allocation->x + border.left;
      child_allocation.y = allocation->y + border.top;
      child_allocation.width = allocation->width - border.left - border.right;
      child_allocation.height = allocation->height - border.top - border.bottom;

      child_allocation.width  = MAX (1, child_allocation.width);
      child_allocation.height = MAX (1, child_allocation.height);

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

void
gtk_list_box_row_changed (GtkListBoxRow *row)
{
  GtkListBox *list_box = gtk_list_box_row_get_box (row);

  if (list_box)
    gtk_list_box_got_row_changed (GTK_LIST_BOX (list_box), row);
}

/**
 * gtk_list_box_row_get_separator:
 *
 * Return value: (transfer none): The current separator.
 */
GtkWidget *
gtk_list_box_row_get_separator (GtkListBoxRow *row)
{
  return row->priv->separator;
}

/**
 * gtk_list_box_row_set_separator:
 * @separator: (allow-none):
 *
 */
void
gtk_list_box_row_set_separator (GtkListBoxRow *row,
                                GtkWidget *separator)
{
  if (row->priv->separator)
    g_object_unref (row->priv->separator);

  row->priv->separator = separator;

  if (separator)
    g_object_ref (separator);
}


static void
gtk_list_box_row_finalize (GObject *obj)
{
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (obj);
  GtkListBoxRowPrivate *priv = row->priv;

  g_clear_object (&priv->separator);

  G_OBJECT_CLASS (gtk_list_box_row_parent_class)->finalize (obj);
}

static void
gtk_list_box_row_class_init (GtkListBoxRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtkListBoxRowPrivate));

  object_class->get_property = gtk_list_box_row_get_property;
  object_class->set_property = gtk_list_box_row_set_property;
  object_class->finalize = gtk_list_box_row_finalize;

  widget_class->show = gtk_list_box_row_real_show;
  widget_class->hide = gtk_list_box_row_real_hide;
  widget_class->draw = gtk_list_box_row_real_draw;
  widget_class->get_preferred_height = gtk_list_box_row_real_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_list_box_row_real_get_preferred_height_for_width;
  widget_class->get_preferred_width = gtk_list_box_row_real_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_list_box_row_real_get_preferred_width_for_height;
  widget_class->size_allocate = gtk_list_box_row_real_size_allocate;
  widget_class->focus = gtk_list_box_row_real_focus;
}
