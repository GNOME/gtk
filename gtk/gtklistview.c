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
#include "gtkselectionmodel.h"
#include "gtksingleselection.h"
#include "gtkwidgetprivate.h"

/* Maximum number of list items created by the listview.
 * For debugging, you can set this to G_MAXUINT to ensure
 * there's always a list item for every row.
 */
#define GTK_LIST_VIEW_MAX_LIST_ITEMS 200

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

  int list_width;
};

struct _ListRow
{
  GtkListItemManagerItem parent;
  guint height; /* per row */
};

struct _ListRowAugment
{
  GtkListItemManagerItemAugment parent;
  guint height; /* total */
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

static void G_GNUC_UNUSED
dump (GtkListView *self)
{
  ListRow *row;
  guint n_widgets, n_list_rows;

  n_widgets = 0;
  n_list_rows = 0;
  //g_print ("ANCHOR: %u - %u\n", self->anchor_start, self->anchor_end);
  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        n_widgets++;
      n_list_rows++;
      g_print ("  %4u%s (%upx)\n", row->parent.n_items, row->parent.widget ? " (widget)" : "", row->height);
    }

  g_print ("  => %u widgets in %u list rows\n", n_widgets, n_list_rows);
}

static void
list_row_augment (GtkRbTree *tree,
                  gpointer   node_augment,
                  gpointer   node,
                  gpointer   left,
                  gpointer   right)
{
  ListRow *row = node;
  ListRowAugment *aug = node_augment;

  gtk_list_item_manager_augment_node (tree, node_augment, node, left, right);

  aug->height = row->height * row->parent.n_items;

  if (left)
    {
      ListRowAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->height += left_aug->height;
    }

  if (right)
    {
      ListRowAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->height += right_aug->height;
    }
}

static ListRow *
gtk_list_view_get_row_at_y (GtkListView *self,
                            int          y,
                            int         *offset)
{
  ListRow *row, *tmp;

  row = gtk_list_item_manager_get_root (self->item_manager);

  while (row)
    {
      tmp = gtk_rb_tree_node_get_left (row);
      if (tmp)
        {
          ListRowAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, tmp);
          if (y < aug->height)
            {
              row = tmp;
              continue;
            }
          y -= aug->height;
        }

      if (y < row->height * row->parent.n_items)
        break;
      y -= row->height * row->parent.n_items;

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
  ListRow *parent, *left;
  int y;

  left = gtk_rb_tree_node_get_left (row);
  if (left)
    {
      ListRowAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, left);
      y = aug->height;
    }
  else
    y = 0; 

  for (parent = gtk_rb_tree_node_get_parent (row);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (row))
    {
      left = gtk_rb_tree_node_get_left (parent);

      if (left != row)
        {
          if (left)
            {
              ListRowAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, left);
              y += aug->height;
            }
          y += parent->height * parent->parent.n_items;
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
  
  row = gtk_list_item_manager_get_root (self->item_manager);
  if (row == NULL)
    return 0;

  aug = gtk_list_item_manager_get_item_augment (self->item_manager, row);
  return aug->height;
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
        {
          pos = gtk_list_item_manager_get_item_position (self->item_manager, row) + dy / row->height;
        }
      else
        pos = 0;

      gtk_list_item_manager_set_anchor (self->item_manager, pos, 0, NULL, (guint) -1);
    }
  
  gtk_widget_queue_allocate (GTK_WIDGET (self));
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
      guint anchor;
      double anchor_align;

      page_size = gtk_widget_get_height (GTK_WIDGET (self));
      upper = gtk_list_view_get_list_height (self);

      anchor = gtk_list_item_manager_get_anchor (self->item_manager, &anchor_align);
      row = gtk_list_item_manager_get_nth (self->item_manager, anchor, NULL);
      if (row)
        value = list_row_get_y (self, row);
      else
        value = 0;
      value += anchor_align * (page_size - (row ? row->height : 0));
    }
  upper = MAX (upper, page_size);

  g_signal_handlers_block_by_func (self->adjustment[orientation],
                                   gtk_list_view_adjustment_value_changed_cb,
                                   self);
  gtk_adjustment_configure (self->adjustment[orientation],
                            value,
                            0,
                            upper,
                            page_size * 0.1,
                            page_size * 0.9,
                            page_size);
  g_signal_handlers_unblock_by_func (self->adjustment[orientation],
                                     gtk_list_view_adjustment_value_changed_cb,
                                     self);
}

static int
compare_ints (gconstpointer first,
               gconstpointer second)
{
  return *(int *) first - *(int *) second;
}

static guint
gtk_list_view_get_unknown_row_height (GtkListView *self,
                                      GArray      *heights)
{
  g_return_val_if_fail (heights->len > 0, 0);

  /* return the median and hope rows are generally uniform with few outliers */
  g_array_sort (heights, compare_ints);

  return g_array_index (heights, int, heights->len / 2);
}

static void
gtk_list_view_measure_across (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  int min, nat, child_min, child_nat;
  /* XXX: Figure out how to split a given height into per-row heights.
   * Good luck! */
  for_size = -1;

  min = 0;
  nat = 0;

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      /* ignore unavailable rows */
      if (row->parent.widget == NULL)
        continue;

      gtk_widget_measure (row->parent.widget,
                          orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

  *minimum = min;
  *natural = nat;
}

static void
gtk_list_view_measure_list (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  int min, nat, child_min, child_nat;
  GArray *min_heights, *nat_heights;
  guint n_unknown;

  min_heights = g_array_new (FALSE, FALSE, sizeof (int));
  nat_heights = g_array_new (FALSE, FALSE, sizeof (int));
  n_unknown = 0;
  min = 0;
  nat = 0;

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        {
          gtk_widget_measure (row->parent.widget,
                              orientation, for_size,
                              &child_min, &child_nat, NULL, NULL);
          g_array_append_val (min_heights, child_min);
          g_array_append_val (nat_heights, child_nat);
          min += child_min;
          nat += child_nat;
        }
      else
        {
          n_unknown += row->parent.n_items;
        }
    }

  if (n_unknown)
    {
      min += n_unknown * gtk_list_view_get_unknown_row_height (self, min_heights);
      nat += n_unknown * gtk_list_view_get_unknown_row_height (self, nat_heights);
    }
  g_array_free (min_heights, TRUE);
  g_array_free (nat_heights, TRUE);

  *minimum = min;
  *natural = nat;
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
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_list_view_measure_across (widget, orientation, for_size, minimum, natural);
  else
    gtk_list_view_measure_list (widget, orientation, for_size, minimum, natural);
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
  GArray *heights;
  int min, nat, row_height;

  /* step 0: exit early if list is empty */
  if (gtk_list_item_manager_get_root (self->item_manager) == NULL)
    return;

  /* step 1: determine width of the list */
  gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &min, &nat, NULL, NULL);
  if (self->scroll_policy[GTK_ORIENTATION_HORIZONTAL] == GTK_SCROLL_MINIMUM)
    self->list_width = MAX (min, width);
  else
    self->list_width = MAX (nat, width);

  /* step 2: determine height of known list items */
  heights = g_array_new (FALSE, FALSE, sizeof (int));

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget == NULL)
        continue;

      gtk_widget_measure (row->parent.widget, GTK_ORIENTATION_VERTICAL,
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
      g_array_append_val (heights, row_height);
    }

  /* step 3: determine height of unknown items */
  row_height = gtk_list_view_get_unknown_row_height (self, heights);
  g_array_free (heights, TRUE);

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        continue;

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
  child_allocation.y = - round (gtk_adjustment_get_value (self->adjustment[GTK_ORIENTATION_VERTICAL]));
  child_allocation.width = self->list_width;
  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        {
          child_allocation.height = row->height;
          gtk_widget_size_allocate (row->parent.widget, &child_allocation, -1);
        }

      child_allocation.y += row->height * row->parent.n_items;
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

  g_clear_object (&self->model);

  gtk_list_view_clear_adjustment (self, GTK_ORIENTATION_HORIZONTAL);
  gtk_list_view_clear_adjustment (self, GTK_ORIENTATION_VERTICAL);

  g_clear_object (&self->item_manager);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_finalize (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

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
  self->item_manager = gtk_list_item_manager_new (GTK_WIDGET (self), ListRow, ListRowAugment, list_row_augment);

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
 * @model: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use.
 *
 * If the @model is a #GtkSelectionModel, it is used for managing the selection.
 * Otherwise, @self creates a #GtkSingleSelection for the selection.
 **/
void
gtk_list_view_set_model (GtkListView *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  g_clear_object (&self->model);

  if (model)
    {
      GtkSelectionModel *selection_model;

      self->model = g_object_ref (model);

      if (GTK_IS_SELECTION_MODEL (model))
        selection_model = GTK_SELECTION_MODEL (g_object_ref (model));
      else
        selection_model = GTK_SELECTION_MODEL (gtk_single_selection_new (model));

      gtk_list_item_manager_set_model (self->item_manager, selection_model);

      g_object_unref (selection_model);
    }
  else
    {
      gtk_list_item_manager_set_model (self->item_manager, NULL);
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

  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (setup_func || bind_func);
  g_return_if_fail (user_data != NULL || user_destroy == NULL);

  factory = gtk_list_item_factory_new (setup_func, bind_func, user_data, user_destroy);
  gtk_list_item_manager_set_factory (self->item_manager, factory);
  g_object_unref (factory);
}

