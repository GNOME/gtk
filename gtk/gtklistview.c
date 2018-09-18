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

#include "gtkintl.h"
#include "gtkrbtreeprivate.h"
#include "gtklistitemfactoryprivate.h"

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
  GtkListItemFactory *item_factory;

  GtkRbTree *rows;
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
  PROP_MODEL,

  N_PROPS
};

G_DEFINE_TYPE (GtkListView, gtk_list_view, GTK_TYPE_WIDGET)

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

  g_clear_pointer (&row->widget, gtk_widget_unparent);
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
  int nat;

  child_allocation.width = width;

  for (row = gtk_rb_tree_get_first (self->rows);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      gtk_widget_measure (row->widget, GTK_ORIENTATION_VERTICAL,
                          width,
                          NULL, &nat, NULL, NULL);
      if (row->height != nat)
        {
          row->height = nat;
          gtk_rb_tree_node_mark_dirty (row);
        }
      child_allocation.height = row->height;
      gtk_widget_size_allocate (row->widget, &child_allocation, -1);
      child_allocation.y += child_allocation.height;
    }
}

static void
gtk_list_view_remove_rows (GtkListView *self,
                           guint        position,
                           guint        n_rows)
{
  ListRow *row;
  guint i;

  if (n_rows == 0)
    return;

  row = gtk_list_view_get_row (self, position, NULL);

  for (i = 0; i < n_rows; i++)
    {
      ListRow *next = gtk_rb_tree_node_get_next (row);
      gtk_rb_tree_remove (self->rows, row);
      row = next;
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_list_view_add_rows (GtkListView *self,
                        guint        position,
                        guint        n_rows)
{
  ListRow *row;
  guint i;

  if (n_rows == 0)
    return;

  row = gtk_list_view_get_row (self, position, NULL);

  for (i = 0; i < n_rows; i++)
    {
      ListRow *new_row;
      gpointer item;

      new_row = gtk_rb_tree_insert_before (self->rows, row);
      new_row->n_rows = 1;
      new_row->widget = gtk_list_item_factory_create (self->item_factory);
      gtk_widget_insert_before (new_row->widget, GTK_WIDGET (self), row ? row->widget : NULL);
      item = g_list_model_get_item (self->model, position + i);
      gtk_list_item_factory_bind (self->item_factory, new_row->widget, item);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_list_view_model_items_changed_cb (GListModel  *model,
                                      guint        position,
                                      guint        removed,
                                      guint        added,
                                      GtkListView *self)
{
  gtk_list_view_remove_rows (self, position, removed);
  gtk_list_view_add_rows (self, position, added);
}

static void
gtk_list_view_clear_model (GtkListView *self)
{
  if (self->model == NULL)
    return;

  gtk_list_view_remove_rows (self, 0, g_list_model_get_n_items (self->model));

  g_signal_handlers_disconnect_by_func (self->model, gtk_list_view_model_items_changed_cb, self);
  g_clear_object (&self->model);
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  gtk_list_view_clear_model (self);

  g_clear_object (&self->item_factory);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_finalize (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  gtk_rb_tree_unref (self->rows);

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
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
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
    case PROP_MODEL:
      gtk_list_view_set_model (self, g_value_get_object (value));
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

  widget_class->measure = gtk_list_view_measure;
  widget_class->size_allocate = gtk_list_view_size_allocate;

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->finalize = gtk_list_view_finalize;
  gobject_class->get_property = gtk_list_view_get_property;
  gobject_class->set_property = gtk_list_view_set_property;

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
  self->rows = gtk_rb_tree_new (ListRow,
                                ListRowAugment,
                                list_row_augment,
                                list_row_clear,
                                NULL);
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

  if (model)
    {
      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed",
                        G_CALLBACK (gtk_list_view_model_items_changed_cb),
                        self);

      gtk_list_view_add_rows (self, 0, g_list_model_get_n_items (model));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

void
gtk_list_view_set_functions (GtkListView            *self,
                             GtkListCreateWidgetFunc create_func,
                             GtkListBindWidgetFunc   bind_func,
                             gpointer                user_data,
                             GDestroyNotify          user_destroy)
{
  guint n_items;

  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (create_func);
  g_return_if_fail (bind_func);
  g_return_if_fail (user_data != NULL || user_destroy == NULL);

  n_items = self->model ? g_list_model_get_n_items (self->model) : 0;
  gtk_list_view_remove_rows (self, 0, n_items);

  g_clear_object (&self->item_factory);
  self->item_factory = gtk_list_item_factory_new (create_func, bind_func, user_data, user_destroy);

  gtk_list_view_add_rows (self, 0, n_items);
}

