/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistview.h"

#include "gtkadjustment.h"
#include "gtkintl.h"
#include "gtkrbtreeprivate.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistitemmanagerprivate.h"
#include "gtkscrollable.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtklistview
 * @title: GtkListView
 * @short_description: A widget for displaying lists
 * @see_also: #GListModel
 *
 * GtkListView is a widget to present a view into a large dynamic list of items.
 */

typedef struct _ListRow ListRow;
typedef struct _ListRowAugment ListRowAugment;

struct _GtkListView
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkListItemManager *item_manager;
  GtkAdjustment *adjustment[2];
  GtkScrollablePolicy scroll_policy[2];

  GtkRbTree *rows;
  int list_width;

  /* managing the visible region */
  guint anchor_pos;
  int anchor_dy;
  guint first_visible_pos;
  guint lasst_visible_pos;
};

struct _ListRow
{
  guint n_rows;
  guint height;
  GtkWidget *widget;
};

struct _ListRowAugment
{
  guint n_rows;
  guint height;
};

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_MODEL,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,

  N_PROPS
};

G_DEFINE_TYPE_WITH_CODE (GtkListView, gtk_list_view, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
list_row_augment (GtkRbTree *tree,
                  gpointer   node_augment,
                  gpointer   node,
                  gpointer   left,
                  gpointer   right)
{
  ListRow *row = node;
  ListRowAugment *aug = node_augment;

  aug->height = row->height;
  aug->n_rows = row->n_rows;

  if (left)
    {
      ListRowAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->height += left_aug->height;
      aug->n_rows += left_aug->n_rows;
    }

  if (right)
    {
      ListRowAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->height += right_aug->height;
      aug->n_rows += right_aug->n_rows;
    }
}

static void
list_row_clear (gpointer _row)
{
  ListRow *row = _row;

  g_assert (row->widget == NULL);
}

static ListRow *
gtk_list_view_get_row (GtkListView *self,
                       guint        position,
                       guint       *offset)
{
  ListRow *row, *tmp;

  row = gtk_rb_tree_get_root (self->rows);

  while (row)
    {
      tmp = gtk_rb_tree_node_get_left (row);
      if (tmp)
        {
          ListRowAugment *aug = gtk_rb_tree_get_augment (self->rows, tmp);
          if (position < aug->n_rows)
            {
              row = tmp;
              continue;
            }
          position -= aug->n_rows;
        }

      if (position < row->n_rows)
        break;
      position -= row->n_rows;

      row = gtk_rb_tree_node_get_right (row);
    }

  if (offset)
    *offset = row ? position : 0;

  return row;
}

static guint
list_row_get_position (GtkListView *self,
                       ListRow     *row)
{
  ListRow *parent, *left;
  int pos;

  left = gtk_rb_tree_node_get_left (row);
  if (left)
    {
      ListRowAugment *aug = gtk_rb_tree_get_augment (self->rows, left);
      pos = aug->n_rows;
    }
  else
    {
      pos = 0; 
    }

  for (parent = gtk_rb_tree_node_get_parent (row);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (row))
    {
      left = gtk_rb_tree_node_get_left (parent);

      if (left != row)
        {
          if (left)
            {
              ListRowAugment *aug = gtk_rb_tree_get_augment (self->rows, left);
              pos += aug->n_rows;
            }
          pos += parent->n_rows;
        }

      row = parent;
    }

  return pos;
}

static ListRow *
gtk_list_view_get_row_at_y (GtkListView *self,
                            int          y,
                            int         *offset)
{
  ListRow *row, *tmp;

  row = gtk_rb_tree_get_root (self->rows);

  while (row)
    {
      tmp = gtk_rb_tree_node_get_left (row);
      if (tmp)
        {
          ListRowAugment *aug = gtk_rb_tree_get_augment (self->rows, tmp);
          if (y < aug->height)
            {
              row = tmp;
              continue;
            }
          y -= aug->height;
        }

      if (y < row->height)
        break;
      y -= row->height;

      row = gtk_rb_tree_node_get_right (row);
    }

  if (offset)
    *offset = row ? y : 0;

  return row;
}

static int
list_row_get_y (GtkListView *self,
                ListRow     *row)
{
  ListRow *parent;
  int y = 0;

  y = 0; 
  for (parent = gtk_rb_tree_node_get_parent (row);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (row))
    {
      ListRow *left = gtk_rb_tree_node_get_left (parent);

      if (left != row)
        {
          if (left)
            {
              ListRowAugment *aug = gtk_rb_tree_get_augment (self->rows, left);
              y += aug->height;
            }
          y += parent->height;
        }

      row = parent;
    }

  return y ;
}

static int
gtk_list_view_get_list_height (GtkListView *self)
{
  ListRow *row;
  ListRowAugment *aug;
  
  row = gtk_rb_tree_get_root (self->rows);
  if (row == NULL)
    return 0;

  aug = gtk_rb_tree_get_augment (self->rows, row);
  return aug->height;
}

static void
gtk_list_view_update_adjustments (GtkListView    *self,
                                  GtkOrientation  orientation)
{
  double upper, page_size, value;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      page_size = gtk_widget_get_width (GTK_WIDGET (self));
      upper = self->list_width;
      value = 0;
      if (_gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
        value = upper - page_size - value;
      value = gtk_adjustment_get_value (self->adjustment[orientation]);
    }
  else
    {
      ListRow *row;

      page_size = gtk_widget_get_height (GTK_WIDGET (self));
      upper = gtk_list_view_get_list_height (self);

      row = gtk_list_view_get_row (self, self->anchor_pos, NULL);
      if (row)
        value = list_row_get_y (self, row);
      else
        value = 0;
      value += self->anchor_dy;
    }
  upper = MAX (upper, page_size);

  gtk_adjustment_configure (self->adjustment[orientation],
                            value,
                            0,
                            upper,
                            page_size * 0.1,
                            page_size * 0.9,
                            page_size);
}

static void
gtk_list_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  int min, nat, child_min, child_nat;

  /* XXX: Figure out how to split a given height into per-row heights.
   * Good luck! */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    for_size = -1;

  min = 0;
  nat = 0;

  for (row = gtk_rb_tree_get_first (self->rows);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      gtk_widget_measure (row->widget,
                          orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          min = MAX (min, child_min);
          nat = MAX (nat, child_nat);
        }
      else
        {
          min += child_nat;
          nat = min;
        }
    }

  *minimum = min;
  *natural = nat;
}

static void
gtk_list_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkAllocation child_allocation = { 0, 0, 0, 0 };
  ListRow *row;
  int min, nat, row_height;

  /* step 1: determine width of the list */
  gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &min, &nat, NULL, NULL);
  if (self->scroll_policy[GTK_ORIENTATION_HORIZONTAL] == GTK_SCROLL_MINIMUM)
    self->list_width = MAX (min, width);
  else
    self->list_width = MAX (nat, width);

  /* step 2: determine height of list */
  for (row = gtk_rb_tree_get_first (self->rows);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      gtk_widget_measure (row->widget, GTK_ORIENTATION_VERTICAL,
                          self->list_width,
                          &min, &nat, NULL, NULL);
      if (self->scroll_policy[GTK_ORIENTATION_VERTICAL] == GTK_SCROLL_MINIMUM)
        row_height = min;
      else
        row_height = nat;
      if (row->height != row_height)
        {
          row->height = row_height;
          gtk_rb_tree_node_mark_dirty (row);
        }
    }

  /* step 3: update the adjustments */
  gtk_list_view_update_adjustments (self, GTK_ORIENTATION_HORIZONTAL);
  gtk_list_view_update_adjustments (self, GTK_ORIENTATION_VERTICAL);

  /* step 4: actually allocate the widgets */
  child_allocation.x = - gtk_adjustment_get_value (self->adjustment[GTK_ORIENTATION_HORIZONTAL]);
  child_allocation.y = - gtk_adjustment_get_value (self->adjustment[GTK_ORIENTATION_VERTICAL]);
  child_allocation.width = self->list_width;
  for (row = gtk_rb_tree_get_first (self->rows);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      child_allocation.height = row->height;
      gtk_widget_size_allocate (row->widget, &child_allocation, -1);
      child_allocation.y += child_allocation.height;
    }
}

static void
gtk_list_view_remove_rows (GtkListView              *self,
                           GtkListItemManagerChange *change,
                           guint                     position,
                           guint                     n_rows)
{
  ListRow *row;
  guint i, n_remaining;

  if (n_rows == 0)
    return;

  n_remaining = self->model ? g_list_model_get_n_items (self->model) : 0;
  if (self->anchor_pos >= position + n_rows)
    {
      self->anchor_pos -= n_rows;
    }
  else if (self->anchor_pos >= position)
    {
      self->anchor_pos = position;
      if (self->anchor_pos > 0 && self->anchor_pos >= n_remaining)
        self->anchor_pos = n_remaining - 1;
      self->anchor_dy = 0;
    }

  row = gtk_list_view_get_row (self, position, NULL);

  for (i = 0; i < n_rows; i++)
    {
      ListRow *next = gtk_rb_tree_node_get_next (row);
      gtk_list_item_manager_release_list_item (self->item_manager, change, row->widget);
      row->widget = NULL;
      gtk_rb_tree_remove (self->rows, row);
      row = next;
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_list_view_add_rows (GtkListView              *self,
                        GtkListItemManagerChange *change,
                        guint                     position,
                        guint                     n_rows)
{  
  ListRow *row;
  guint i, n_total;

  if (n_rows == 0)
    return;

  n_total = self->model ? g_list_model_get_n_items (self->model) : 0;
  if (self->anchor_pos >= position)
    {
      if (n_total != n_rows) /* the model was not empty before */
        self->anchor_pos += n_rows;
    }

  row = gtk_list_view_get_row (self, position, NULL);

  for (i = 0; i < n_rows; i++)
    {
      ListRow *new_row;
        
      new_row = gtk_rb_tree_insert_before (self->rows, row);
      new_row->n_rows = 1;
      if (change)
        {
          new_row->widget = gtk_list_item_manager_try_reacquire_list_item (self->item_manager,
                                                                           change,
                                                                           position + i,
                                                                           row ? row->widget : NULL);
        }
      if (new_row->widget == NULL)
        {
          new_row->widget = gtk_list_item_manager_acquire_list_item (self->item_manager,
                                                                     position + i,
                                                                     row ? row->widget : NULL);
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_list_view_update_rows (GtkListView *self,
                           guint        position)
{
  ListRow *row;

  for (row = gtk_list_view_get_row (self, position, NULL);
       row;
       row = gtk_rb_tree_node_get_next (row))
    {
      gtk_list_item_manager_update_list_item (self->item_manager, row->widget, position);

      position++;
    }
}

static void
gtk_list_view_model_items_changed_cb (GListModel  *model,
                                      guint        position,
                                      guint        removed,
                                      guint        added,
                                      GtkListView *self)
{
  GtkListItemManagerChange *change;

  change = gtk_list_item_manager_begin_change (self->item_manager);

  gtk_list_view_remove_rows (self, change, position, removed);
  gtk_list_view_add_rows (self, change, position, added);
  if (removed != added)
    gtk_list_view_update_rows (self, position + added);

  gtk_list_item_manager_end_change (self->item_manager, change);
}

static void
gtk_list_view_clear_model (GtkListView *self)
{
  if (self->model == NULL)
    return;

  gtk_list_view_remove_rows (self, NULL, 0, g_list_model_get_n_items (self->model));

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_view_model_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_list_view_adjustment_value_changed_cb (GtkAdjustment *adjustment,
                                           GtkListView   *self)
{
  if (adjustment == self->adjustment[GTK_ORIENTATION_VERTICAL])
    {
      ListRow *row;
      guint pos;
      int dy;

      row = gtk_list_view_get_row_at_y (self, gtk_adjustment_get_value (adjustment), &dy);
      if (row)
        pos = list_row_get_position (self, row);
      else
        pos = 0;

      if (pos != self->anchor_pos || dy != self->anchor_dy)
        {
          self->anchor_pos = pos;
          self->anchor_dy = dy;
          gtk_widget_queue_allocate (GTK_WIDGET (self));
        }
    }
  else
    { 
      gtk_widget_queue_allocate (GTK_WIDGET (self));
    }
}

static void
gtk_list_view_clear_adjustment (GtkListView    *self,
                                GtkOrientation  orientation)
{
  if (self->adjustment[orientation] == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->adjustment[orientation],
                                        gtk_list_view_adjustment_value_changed_cb,
                                        self);
  g_clear_object (&self->adjustment[orientation]);
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  gtk_list_view_clear_model (self);

  gtk_list_view_clear_adjustment (self, GTK_ORIENTATION_HORIZONTAL);
  gtk_list_view_clear_adjustment (self, GTK_ORIENTATION_VERTICAL);

  g_clear_object (&self->item_manager);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_finalize (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  gtk_rb_tree_unref (self->rows);
  g_clear_object (&self->item_manager);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->finalize (object);
}

static void
gtk_list_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->adjustment[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, self->scroll_policy[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, self->adjustment[GTK_ORIENTATION_VERTICAL]);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, self->scroll_policy[GTK_ORIENTATION_VERTICAL]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_set_adjustment (GtkListView    *self,
                              GtkOrientation  orientation,
                              GtkAdjustment  *adjustment)
{
  if (self->adjustment[orientation] == adjustment)
    return;

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  g_object_ref_sink (adjustment);

  gtk_list_view_clear_adjustment (self, orientation);

  self->adjustment[orientation] = adjustment;
  gtk_list_view_update_adjustments (self, orientation);

  g_signal_connect (adjustment, "value-changed",
		    G_CALLBACK (gtk_list_view_adjustment_value_changed_cb),
		    self);
}

static void
gtk_list_view_set_scroll_policy (GtkListView         *self,
                                 GtkOrientation       orientation,
                                 GtkScrollablePolicy  scroll_policy)
{
  if (self->scroll_policy[orientation] == scroll_policy)
    return;

  self->scroll_policy[orientation] = scroll_policy;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self),
                            orientation == GTK_ORIENTATION_HORIZONTAL
                            ? properties[PROP_HSCROLL_POLICY]
                            : properties[PROP_VSCROLL_POLICY]);
}

static void
gtk_list_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      gtk_list_view_set_adjustment (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      gtk_list_view_set_scroll_policy (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_enum (value));
      break;

    case PROP_MODEL:
      gtk_list_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_VADJUSTMENT:
      gtk_list_view_set_adjustment (self, GTK_ORIENTATION_VERTICAL, g_value_get_object (value));
      break;

    case PROP_VSCROLL_POLICY:
      gtk_list_view_set_scroll_policy (self, GTK_ORIENTATION_VERTICAL, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer iface;

  widget_class->measure = gtk_list_view_measure;
  widget_class->size_allocate = gtk_list_view_size_allocate;

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->finalize = gtk_list_view_finalize;
  gobject_class->get_property = gtk_list_view_get_property;
  gobject_class->set_property = gtk_list_view_set_property;

  /* GtkScrollable implementation */
  iface = g_type_default_interface_peek (GTK_TYPE_SCROLLABLE);
  properties[PROP_HADJUSTMENT] =
      g_param_spec_override ("hadjustment",
                             g_object_interface_find_property (iface, "hadjustment"));
  properties[PROP_HSCROLL_POLICY] =
      g_param_spec_override ("hscroll-policy",
                             g_object_interface_find_property (iface, "hscroll-policy"));
  properties[PROP_VADJUSTMENT] =
      g_param_spec_override ("vadjustment",
                             g_object_interface_find_property (iface, "vadjustment"));
  properties[PROP_VSCROLL_POLICY] =
      g_param_spec_override ("vscroll-policy",
                             g_object_interface_find_property (iface, "vscroll-policy"));

  /**
   * GtkListView:model:
   *
   * Model for the items displayed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("list"));
}

static void
gtk_list_view_init (GtkListView *self)
{
  self->item_manager = gtk_list_item_manager_new (GTK_WIDGET (self));

  self->rows = gtk_rb_tree_new (ListRow,
                                ListRowAugment,
                                list_row_augment,
                                list_row_clear,
                                NULL);

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

/**
 * gtk_list_view_new:
 *
 * Creates a new empty #GtkListView.
 *
 * You most likely want to call gtk_list_view_set_model() to set
 * a model and then set up a way to map its items to widgets next.
 *
 * Returns: a new #GtkListView
 **/
GtkWidget *
gtk_list_view_new (void)
{
  return g_object_new (GTK_TYPE_LIST_VIEW, NULL);
}

/**
 * gtk_list_view_get_model:
 * @self: a #GtkListView
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_list_view_get_model (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return self->model;
}

/**
 * gtk_list_view_set_model:
 * @self: a #GtkListView
 * @file: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use for
 **/
void
gtk_list_view_set_model (GtkListView *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  gtk_list_view_clear_model (self);

  gtk_list_item_manager_set_model (self->item_manager, model);

  if (model)
    {
      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed",
                        G_CALLBACK (gtk_list_view_model_items_changed_cb),
                        self);

      gtk_list_view_add_rows (self, NULL, 0, g_list_model_get_n_items (model));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

void
gtk_list_view_set_functions (GtkListView          *self,
                             GtkListItemSetupFunc  setup_func,
                             GtkListItemBindFunc   bind_func,
                             gpointer              user_data,
                             GDestroyNotify        user_destroy)
{
  GtkListItemFactory *factory;
  guint n_items;

  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (setup_func || bind_func);
  g_return_if_fail (user_data != NULL || user_destroy == NULL);

  n_items = self->model ? g_list_model_get_n_items (self->model) : 0;
  gtk_list_view_remove_rows (self, NULL, 0, n_items);

  factory = gtk_list_item_factory_new (setup_func, bind_func, user_data, user_destroy);
  gtk_list_item_manager_set_factory (self->item_manager, factory);
  g_object_unref (factory);

  gtk_list_view_add_rows (self, NULL, 0, n_items);
}

