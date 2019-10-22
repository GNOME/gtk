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

#include "gtkbindings.h"
#include "gtkintl.h"
#include "gtklistbaseprivate.h"
#include "gtklistitemmanagerprivate.h"
#include "gtkmain.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkrbtreeprivate.h"
#include "gtkselectionmodel.h"
#include "gtksingleselection.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

/* Maximum number of list items created by the listview.
 * For debugging, you can set this to G_MAXUINT to ensure
 * there's always a list item for every row.
 */
#define GTK_LIST_VIEW_MAX_LIST_ITEMS 200

/* Extra items to keep above + below every tracker */
#define GTK_LIST_VIEW_EXTRA_ITEMS 2

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
  GtkListBase parent_instance;

  GListModel *model;
  GtkListItemManager *item_manager;
  gboolean show_separators;
  GtkOrientation orientation;

  int list_width;

  GtkListItemTracker *anchor;
  double anchor_align;
  gboolean anchor_start;
  /* the last item that was selected - basically the location to extend selections from */
  GtkListItemTracker *selected;
  /* the item that has input focus */
  GtkListItemTracker *focus;
};

struct _GtkListViewClass
{
  GtkListBaseClass parent_class;
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
  PROP_FACTORY,
  PROP_MODEL,
  PROP_ORIENTATION,
  PROP_SHOW_SEPARATORS,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_CODE (GtkListView, gtk_list_view, GTK_TYPE_LIST_BASE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

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
gtk_list_view_get_position_at_y (GtkListView *self,
                                 int          y,
                                 int         *offset,
                                 int         *height)
{
  ListRow *row;
  guint pos;
  int remaining;

  row = gtk_list_view_get_row_at_y (self, y, &remaining);
  if (row == NULL)
    {
      pos = GTK_INVALID_LIST_POSITION;
      if (offset)
        *offset = 0;
      if (height)
        *height = 0;
      return GTK_INVALID_LIST_POSITION;
    }
  pos = gtk_list_item_manager_get_item_position (self->item_manager, row);
  g_assert (remaining < row->height * row->parent.n_items);
  pos += remaining / row->height;
  if (offset)
    *offset = remaining % row->height;
  if (height)
    *height = row->height;

  return pos;
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

static gboolean
gtk_list_view_get_size_at_position (GtkListView *self,
                                    guint        pos,
                                    int         *offset,
                                    int         *height)
{
  ListRow *row;
  guint skip;
  int y;

  row = gtk_list_item_manager_get_nth (self->item_manager, pos, &skip);
  if (row == NULL)
    {
      if (offset)
        *offset = 0;
      if (height)
        *height = 0;
      return FALSE;
    }

  y = list_row_get_y (self, row);
  y += skip * row->height;

  if (offset)
    *offset = y;
  if (height)
    *height = row->height;

  return TRUE;
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
gtk_list_view_set_anchor (GtkListView *self,
                          guint        position,
                          double       align,
                          gboolean     start)
{
  gtk_list_item_tracker_set_position (self->item_manager,
                                      self->anchor,
                                      position,
                                      GTK_LIST_VIEW_EXTRA_ITEMS + GTK_LIST_VIEW_MAX_LIST_ITEMS * align,
                                      GTK_LIST_VIEW_EXTRA_ITEMS + GTK_LIST_VIEW_MAX_LIST_ITEMS - 1 - GTK_LIST_VIEW_MAX_LIST_ITEMS * align);
  if (self->anchor_align != align || self->anchor_start != start)
    {
      self->anchor_align = align;
      self->anchor_start = start;
      gtk_widget_queue_allocate (GTK_WIDGET (self));
    }
}

static void
gtk_list_view_adjustment_value_changed (GtkListBase    *base,
                                        GtkOrientation  orientation)
{
  GtkListView *self = GTK_LIST_VIEW (base);

  if (orientation == self->orientation)
    {
      int page_size, total_size, value, from_start;
      int row_start, row_end;
      double align;
      gboolean top;
      guint pos;

      gtk_list_base_get_adjustment_values (base, orientation, &value, &total_size, &page_size);

      /* Compute how far down we've scrolled. That's the height
       * we want to align to. */
      align = (double) value / (total_size - page_size);
      from_start = round (align * page_size);
      
      pos = gtk_list_view_get_position_at_y (self,
                                             value + from_start,
                                             &row_start, &row_end);
      if (pos != GTK_INVALID_LIST_POSITION)
        {
          /* offset from value - which is where we wanna scroll to */
          row_start = from_start - row_start;
          row_end += row_start;

          /* find an anchor that is in the visible area */
          if (row_start > 0 && row_end < page_size)
            top = from_start - row_start <= row_end - from_start;
          else if (row_start > 0)
            top = TRUE;
          else if (row_end < page_size)
            top = FALSE;
          else
            {
              /* This is the case where the row occupies the whole visible area.
               * It's also the only case where align will not end up in [0..1] */
              top = from_start - row_start <= row_end - from_start;
            }

          /* Now compute the align so that when anchoring to the looked
           * up row, the position is pixel-exact.
           */
          align = (double) (top ? row_start : row_end) / page_size;
        }
      else
        {
          /* Happens if we scroll down to the end - we will query
           * exactly the pixel behind the last one we can get a row for.
           * So take the last row. */
          pos = g_list_model_get_n_items (self->model) - 1;
          align = 1.0;
          top = FALSE;
        }

      gtk_list_view_set_anchor (self, pos, align, top);
    }
  
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static int 
gtk_list_view_update_adjustments (GtkListView    *self,
                                  GtkOrientation  orientation)
{
  int upper, page_size, value;

  page_size = gtk_widget_get_size (GTK_WIDGET (self), orientation);

  if (orientation == self->orientation)
    {
      int offset, size;
      guint anchor_pos;

      upper = gtk_list_view_get_list_height (self);
      anchor_pos = gtk_list_item_tracker_get_position (self->item_manager, self->anchor);

      if (!gtk_list_view_get_size_at_position (self,
                                               anchor_pos,
                                               &offset,
                                               &size))
        {
          g_assert_not_reached ();
        }
      if (!self->anchor_start)
        offset += size;

      value = offset - self->anchor_align * page_size;
    }
  else
    {
      upper = self->list_width;
      gtk_list_base_get_adjustment_values (GTK_LIST_BASE (self), orientation, &value, NULL, NULL);
    }

  return gtk_list_base_set_adjustment_values (GTK_LIST_BASE (self), orientation, value, upper, page_size);
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
  GtkListView *self = GTK_LIST_VIEW (widget);

  if (orientation == self->orientation)
    gtk_list_view_measure_list (widget, orientation, for_size, minimum, natural);
  else
    gtk_list_view_measure_across (widget, orientation, for_size, minimum, natural);
}

static void
gtk_list_view_size_allocate_child (GtkListView *self,
                                   GtkWidget   *child,
                                   int          x,
                                   int          y,
                                   int          width,
                                   int          height)
{
  GtkAllocation child_allocation;

  if (self->orientation == GTK_ORIENTATION_VERTICAL)
    {
      child_allocation.x = x;
      child_allocation.y = y;
      child_allocation.width = width;
      child_allocation.height = height;
    }
  else if (_gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
    {
      child_allocation.x = y;
      child_allocation.y = x;
      child_allocation.width = height;
      child_allocation.height = width;
    }
  else
    {
      int mirror_point = gtk_widget_get_width (GTK_WIDGET (self));

      child_allocation.x = mirror_point - y - height; 
      child_allocation.y = x;
      child_allocation.width = height;
      child_allocation.height = width;
    }

  gtk_widget_size_allocate (child, &child_allocation, -1);
}

static void
gtk_list_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  GArray *heights;
  int min, nat, row_height;
  int x, y;
  GtkOrientation opposite_orientation;
  GtkScrollablePolicy scroll_policy;

  opposite_orientation = OPPOSITE_ORIENTATION (self->orientation);
  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), self->orientation);

  /* step 0: exit early if list is empty */
  if (gtk_list_item_manager_get_root (self->item_manager) == NULL)
    return;

  /* step 1: determine width of the list */
  gtk_widget_measure (widget, opposite_orientation,
                      -1,
                      &min, &nat, NULL, NULL);
  self->list_width = self->orientation == GTK_ORIENTATION_VERTICAL ? width : height;
  if (scroll_policy == GTK_SCROLL_MINIMUM)
    self->list_width = MAX (min, self->list_width);
  else
    self->list_width = MAX (nat, self->list_width);

  /* step 2: determine height of known list items */
  heights = g_array_new (FALSE, FALSE, sizeof (int));

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget == NULL)
        continue;

      gtk_widget_measure (row->parent.widget, self->orientation,
                          self->list_width,
                          &min, &nat, NULL, NULL);
      if (scroll_policy == GTK_SCROLL_MINIMUM)
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
  x = - gtk_list_view_update_adjustments (self, opposite_orientation);
  y = - gtk_list_view_update_adjustments (self, self->orientation);

  /* step 4: actually allocate the widgets */

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        {
          gtk_list_view_size_allocate_child (self,
                                             row->parent.widget,
                                             x,
                                             y,
                                             self->list_width,
                                             row->height);
        }

      y += row->height * row->parent.n_items;
    }
}

static void
gtk_list_view_select_item (GtkListView *self,
                           guint        pos,
                           gboolean     modify,
                           gboolean     extend)
{
  if (gtk_selection_model_user_select_item (gtk_list_item_manager_get_model (self->item_manager),
                                            pos,
                                            modify,
                                            extend ? gtk_list_item_tracker_get_position (self->item_manager, self->selected)
                                                   : GTK_INVALID_LIST_POSITION))
    {
      gtk_list_item_tracker_set_position (self->item_manager,
                                          self->selected,
                                          pos,
                                          0, 0);
    }
}

static gboolean
gtk_list_view_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkWidget *old_focus_child, *new_focus_child;

  old_focus_child = gtk_widget_get_focus_child (widget);

  if (old_focus_child == NULL &&
      (direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD))
    {
      ListRow *row;
      guint pos;

      /* When tabbing into the listview, don't focus the first/last item,
       * but keep the previously focused item
       */
      pos = gtk_list_item_tracker_get_position (self->item_manager, self->focus);
      row = gtk_list_item_manager_get_nth (self->item_manager, pos, NULL);
      if (row && gtk_widget_grab_focus (row->parent.widget))
        goto moved_focus;
    }

  if (!GTK_WIDGET_CLASS (gtk_list_view_parent_class)->focus (widget, direction))
    return FALSE;

moved_focus:
  new_focus_child = gtk_widget_get_focus_child (widget);

  if (old_focus_child != new_focus_child &&
      GTK_IS_LIST_ITEM (new_focus_child))
    {
      GdkModifierType state;
      GdkModifierType mask;
      gboolean extend = FALSE, modify = FALSE;

      if (old_focus_child && gtk_get_current_event_state (&state))
        {
          mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);
          if ((state & mask) == mask)
            modify = TRUE;
          mask = gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);
          if ((state & mask) == mask)
            extend = TRUE;
        }

      gtk_list_view_select_item (self,
                                 gtk_list_item_get_position (GTK_LIST_ITEM (new_focus_child)),
                                 modify,
                                 extend);
    }

  return TRUE;
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  g_clear_object (&self->model);

  if (self->anchor)
    {
      gtk_list_item_tracker_free (self->item_manager, self->anchor);
      self->anchor = NULL;
    }
  if (self->selected)
    {
      gtk_list_item_tracker_free (self->item_manager, self->selected);
      self->selected = NULL;
    }
  if (self->focus)
    {
      gtk_list_item_tracker_free (self->item_manager, self->focus);
      self->focus = NULL;
    }
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
    case PROP_FACTORY:
      g_value_set_object (value, gtk_list_item_manager_get_factory (self->item_manager));
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;

    case PROP_SHOW_SEPARATORS:
      g_value_set_boolean (value, self->show_separators);
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
    case PROP_FACTORY:
      gtk_list_view_set_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_list_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (self->orientation != orientation)
          {
            self->orientation = orientation;
            _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ORIENTATION]);
          }
      }
      break;

    case PROP_SHOW_SEPARATORS:
      gtk_list_view_set_show_separators (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_select_item_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  guint pos;
  gboolean modify, extend;

  g_variant_get (parameter, "(ubb)", &pos, &modify, &extend);

  gtk_list_view_select_item (self, pos, modify, extend);
}

static void
gtk_list_view_select_all (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkSelectionModel *selection_model;

  selection_model = gtk_list_item_manager_get_model (self->item_manager);

  gtk_selection_model_select_all (selection_model);
}

static void
gtk_list_view_unselect_all (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkSelectionModel *selection_model;

  selection_model = gtk_list_item_manager_get_model (self->item_manager);

  gtk_selection_model_unselect_all (selection_model);
}

static void
gtk_list_view_update_focus_tracker (GtkListView *self)
{
  GtkWidget *focus_child;
  guint pos;

  focus_child = gtk_widget_get_focus_child (GTK_WIDGET (self));
  if (!GTK_IS_LIST_ITEM (focus_child))
    return;

  pos = gtk_list_item_get_position (GTK_LIST_ITEM (focus_child));
  if (pos != gtk_list_item_tracker_get_position (self->item_manager, self->focus))
    {
      gtk_list_item_tracker_set_position (self->item_manager,
                                          self->focus,
                                          pos,
                                          GTK_LIST_VIEW_EXTRA_ITEMS,
                                          GTK_LIST_VIEW_EXTRA_ITEMS);
    }
}

static void
gtk_list_view_compute_scroll_align (GtkListView    *self,
                                    GtkOrientation  orientation,
                                    int             cell_start,
                                    int             cell_end,
                                    double          current_align,
                                    gboolean        current_start,
                                    double         *new_align,
                                    gboolean       *new_start)
{
  int visible_start, visible_size, visible_end;
  int cell_size;

  gtk_list_base_get_adjustment_values (GTK_LIST_BASE (self),
                                       orientation,
                                       &visible_start,
                                       NULL,
                                       &visible_size);
  visible_end = visible_start + visible_size;
  cell_size = cell_end - cell_start;

  if (cell_size <= visible_size)
    {
      if (cell_start < visible_start)
        {
          *new_align = 0.0;
          *new_start = TRUE;
        }
      else if (cell_end > visible_end)
        {
          *new_align = 1.0;
          *new_start = FALSE;
        }
      else
        {
          /* XXX: start or end here? */
          *new_start = TRUE;
          *new_align = (double) (cell_start - visible_start) / visible_size;
        }
    }
  else
    {
      /* This is the unlikely case of the cell being higher than the visible area */
      if (cell_start > visible_start)
        {
          *new_align = 0.0;
          *new_start = TRUE;
        }
      else if (cell_end < visible_end)
        {
          *new_align = 1.0;
          *new_start = FALSE;
        }
      else
        {
          /* the cell already covers the whole screen */
          *new_align = current_align;
          *new_start = current_start;
        }
    }
}

static void
gtk_list_view_scroll_to_item (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  int start, end;
  double align;
  gboolean top;
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);

  /* figure out primary orientation and if position is valid */
  if (!gtk_list_view_get_size_at_position (self, pos, &start, &end))
    return;

  end += start;
  gtk_list_view_compute_scroll_align (self,
                                      self->orientation,
                                      start, end,
                                      self->anchor_align, self->anchor_start,
                                      &align, &top);

  gtk_list_view_set_anchor (self, pos, align, top);

  /* HACK HACK HACK
   *
   * GTK has no way to track the focused child. But we now that when a listitem
   * gets focus, it calls this action. So we update our focus tracker from here
   * because it's the closest we can get to accurate tracking.
   */
  gtk_list_view_update_focus_tracker (self);
}

static void
gtk_list_view_activate_item (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);
  if (self->model == NULL || pos >= g_list_model_get_n_items (self->model))
    return;

  g_signal_emit (widget, signals[ACTIVATE], 0, pos);
}

static void
gtk_list_view_move_to (GtkListView *self,
                       guint        pos,
                       gboolean     select,
                       gboolean     modify,
                       gboolean     extend)
{
  ListRow *row;

  row = gtk_list_item_manager_get_nth (self->item_manager, pos, NULL);
  if (row == NULL)
    return;

  if (!row->parent.widget)
    {
      GtkListItemTracker *tracker = gtk_list_item_tracker_new (self->item_manager);

      /* We need a tracker here to create the widget.
       * That needs to have happened or we can't grab it.
       * And we can't use a different tracker, because they manage important rows,
       * so we create a temporary one. */
      gtk_list_item_tracker_set_position (self->item_manager, tracker, pos, 0, 0);

      row = gtk_list_item_manager_get_nth (self->item_manager, pos, NULL);
      g_assert (row->parent.widget);

      if (!gtk_widget_grab_focus (row->parent.widget))
          return; /* FIXME: What now? Can this even happen? */

      gtk_list_item_tracker_free (self->item_manager, tracker);
    }
  else
    {
      if (!gtk_widget_grab_focus (row->parent.widget))
          return; /* FIXME: What now? Can this even happen? */
    }

  if (select)
    gtk_list_view_select_item (self, pos, modify, extend);
}

static void
gtk_list_view_move_cursor (GtkWidget *widget,
                           GVariant  *args,
                           gpointer   unused)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  int amount;
  guint orientation;
  guint pos, new_pos, n_items;
  gboolean select, modify, extend;

  g_variant_get (args, "(ubbbi)", &orientation, &select, &modify, &extend, &amount);

  if (self->orientation != orientation)
    return;

  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    amount = -amount;

  pos = gtk_list_item_tracker_get_position (self->item_manager, self->focus);
  n_items = self->model ? g_list_model_get_n_items (self->model) : 0;
  if (pos >= n_items)
    return;

  new_pos = pos + amount;
  /* This overflow check only works reliably for amount == 1 */
  if (new_pos >= n_items)
    return;

  gtk_list_view_move_to (self, new_pos, select, modify, extend);
}

static void
gtk_list_view_move_cursor_to_start (GtkWidget *widget,
                                    GVariant  *args,
                                    gpointer   unused)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  gboolean select, modify, extend;

  if (self->model == NULL || g_list_model_get_n_items (self->model) == 0)
    return;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_view_move_to (self, 0, select, modify, extend);
}

static void
gtk_list_view_move_cursor_to_end (GtkWidget *widget,
                                  GVariant  *args,
                                  gpointer   unused)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  gboolean select, modify, extend;
  guint n_items;

  if (self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (self->model);
  if (n_items == 0)
    return;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_view_move_to (self, n_items - 1, select, modify, extend);
}

static void
gtk_list_view_move_cursor_page_up (GtkWidget *widget,
                                   GVariant  *args,
                                   gpointer   unused)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  gboolean select, modify, extend;
  guint start, pos, n_items;
  ListRow *row;
  int pixels, offset;

  start = gtk_list_item_tracker_get_position (self->item_manager, self->focus);
  row = gtk_list_item_manager_get_nth (self->item_manager, start, NULL);
  if (row == NULL)
    return;
  n_items = self->model ? g_list_model_get_n_items (self->model) : 0;
  /* check that we can go at least one row up */
  if (n_items == 0 || start == 0)
    return;

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    pixels = gtk_widget_get_width (widget);
  else
    pixels = gtk_widget_get_height (widget);
  pixels -= row->height;

  pos = gtk_list_view_get_position_at_y (self, 
                                         MAX (0, list_row_get_y (self, row) - pixels),
                                         &offset,
                                         NULL);
  /* there'll always be rows between 0 and this row */
  g_assert (pos < n_items);
  /* if row is too high, go one row less */
  if (offset > 0)
    pos++;
  /* but go at least 1 row */
  if (pos >= start)
    pos = start - 1;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_view_move_to (self, pos, select, modify, extend);
}

static void
gtk_list_view_move_cursor_page_down (GtkWidget *widget,
                                     GVariant  *args,
                                     gpointer   unused)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  gboolean select, modify, extend;
  guint start, pos, n_items;
  ListRow *row;
  int pixels, offset;

  start = gtk_list_item_tracker_get_position (self->item_manager, self->focus);
  row = gtk_list_item_manager_get_nth (self->item_manager, start, NULL);
  if (row == NULL)
    return;
  n_items = self->model ? g_list_model_get_n_items (self->model) : 0;
  /* check that we can go at least one row down */
  if (n_items == 0 || start >= n_items - 1)
    return;

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    pixels = gtk_widget_get_width (widget);
  else
    pixels = gtk_widget_get_height (widget);

  pos = gtk_list_view_get_position_at_y (self, 
                                         list_row_get_y (self, row) + pixels,
                                         &offset,
                                         NULL);
  if (pos >= n_items)
    pos = n_items - 1;
  /* if row is too high, go one row less */
  else if (pos > 0 && offset > 0)
    pos--;
  /* but go at least 1 row */
  if (pos <= start)
    pos = start + 1;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_view_move_to (self, pos, select, modify, extend);
}

static void
gtk_list_view_add_custom_move_binding (GtkBindingSet      *binding_set,
                                       guint               keyval,
                                       GtkBindingCallback  callback)
{
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  0,
                                  callback,
                                  g_variant_new ("(bbb)", TRUE, FALSE, FALSE),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK,
                                  callback,
                                  g_variant_new ("(bbb)", FALSE, FALSE, FALSE),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_SHIFT_MASK,
                                  callback,
                                  g_variant_new ("(bbb)", TRUE, FALSE, TRUE),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                  callback,
                                  g_variant_new ("(bbb)", TRUE, TRUE, TRUE),
                                  NULL, NULL);
}

static void
gtk_list_view_add_move_binding (GtkBindingSet  *binding_set,
                                guint           keyval,
                                GtkOrientation  orientation,
                                int             amount)
{
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK,
                                  gtk_list_view_move_cursor,
                                  g_variant_new ("(ubbbi)", orientation, FALSE, FALSE, FALSE, amount),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_SHIFT_MASK,
                                  gtk_list_view_move_cursor,
                                  g_variant_new ("(ubbbi)", orientation, TRUE, FALSE, TRUE, amount),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                  gtk_list_view_move_cursor,
                                  g_variant_new ("(ubbbi)", orientation, TRUE, TRUE, TRUE, amount),
                                  NULL, NULL);
}

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GtkListBaseClass *list_base_class = GTK_LIST_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  list_base_class->adjustment_value_changed = gtk_list_view_adjustment_value_changed;

  widget_class->measure = gtk_list_view_measure;
  widget_class->size_allocate = gtk_list_view_size_allocate;
  widget_class->focus = gtk_list_view_focus;

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->finalize = gtk_list_view_finalize;
  gobject_class->get_property = gtk_list_view_get_property;
  gobject_class->set_property = gtk_list_view_set_property;

  /**
   * GtkListView:factory:
   *
   * Factory for populating list items
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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

  /**
   * GtkListView:orientation:
   *
   * The orientation of the orientable. See GtkOrientable:orientation
   * for details.
   */
  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       P_("Orientation"),
                       P_("The orientation of the orientable"),
                       GTK_TYPE_ORIENTATION,
                       GTK_ORIENTATION_VERTICAL,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListView:show-separators:
   *
   * Show separators between rows
   */
  properties[PROP_SHOW_SEPARATORS] =
    g_param_spec_boolean ("show-separators",
                          P_("Show separators"),
                          P_("Show separators between rows"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkListView::activate:
   * @self: The #GtkListView
   * @position: position of item to activate
   *
   * The ::activate signal is emitted when a row has been activated by the user,
   * usually via activating the GtkListView|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a listview.
   * See gtk_list_item_set_activatable() for details on how to use this signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__UINTv);

  /**
   * GtkListView|list.activate-item:
   * @position: position of item to activate
   *
   * Activates the item given in @position by emitting the GtkListView::activate
   * signal.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.activate-item",
                                   "u",
                                   gtk_list_view_activate_item);

  /**
   * GtkListView|list.select-item:
   * @position: position of item to select
   * @modify: %TRUE to toggle the existing selection, %FALSE to select
   * @extend: %TRUE to extend the selection
   *
   * Changes selection.
   *
   * If @extend is %TRUE and the model supports selecting ranges, the
   * affected items are all items from the last selected item to the item
   * in @position.
   * If @extend is %FALSE or selecting ranges is not supported, only the
   * item in @position is affected.
   *
   * If @modify is %TRUE, the affected items will be set to the same state.
   * If @modify is %FALSE, the affected items will be selected and
   * all other items will be deselected.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.select-item",
                                   "(ubb)",
                                   gtk_list_view_select_item_action);

  /**
   * GtkListView|list.select-all:
   *
   * If the selection model supports it, select all items in the model.
   * If not, do nothing.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.select-all",
                                   NULL,
                                   gtk_list_view_select_all);

  /**
   * GtkListView|list.unselect-all:
   *
   * If the selection model supports it, unselect all items in the model.
   * If not, do nothing.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.unselect-all",
                                   NULL,
                                   gtk_list_view_unselect_all);

  /**
   * GtkListView|list.scroll-to-item:
   * @position: position of item to scroll to
   *
   * Scrolls to the item given in @position with the minimum amount
   * of scrolling required. If the item is already visible, nothing happens.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.scroll-to-item",
                                   "u",
                                   gtk_list_view_scroll_to_item);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Up, GTK_ORIENTATION_VERTICAL, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Up, GTK_ORIENTATION_VERTICAL, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Down, GTK_ORIENTATION_VERTICAL, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Down, GTK_ORIENTATION_VERTICAL, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Left, GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Left, GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Right, GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Right, GTK_ORIENTATION_HORIZONTAL, 1);

  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_Home, gtk_list_view_move_cursor_to_start);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_KP_Home, gtk_list_view_move_cursor_to_start);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_End, gtk_list_view_move_cursor_to_end);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_KP_End, gtk_list_view_move_cursor_to_end);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_Page_Up, gtk_list_view_move_cursor_page_up);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_KP_Page_Up, gtk_list_view_move_cursor_page_up);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_Page_Down, gtk_list_view_move_cursor_page_down);
  gtk_list_view_add_custom_move_binding (binding_set, GDK_KEY_KP_Page_Down, gtk_list_view_move_cursor_page_down);

  gtk_binding_entry_add_action (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, "list.select-all", NULL);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK, "list.select-all", NULL);

  gtk_binding_entry_add_action (binding_set, GDK_KEY_A, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "list.unselect-all", NULL);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_backslash, GDK_CONTROL_MASK, "list.unselect-all", NULL);

  gtk_widget_class_set_css_name (widget_class, I_("list"));
}

static void
gtk_list_view_init (GtkListView *self)
{
  self->item_manager = gtk_list_item_manager_new (GTK_WIDGET (self), "row", ListRow, ListRowAugment, list_row_augment);
  self->focus = gtk_list_item_tracker_new (self->item_manager);
  self->anchor = gtk_list_item_tracker_new (self->item_manager);
  self->selected = gtk_list_item_tracker_new (self->item_manager);
  self->orientation = GTK_ORIENTATION_VERTICAL;
}

/**
 * gtk_list_view_new:
 *
 * Creates a new empty #GtkListView.
 *
 * You most likely want to call gtk_list_view_set_factory() to
 * set up a way to map its items to widgets and gtk_list_view_set_model()
 * to set a model to provide items next.
 *
 * Returns: a new #GtkListView
 **/
GtkWidget *
gtk_list_view_new (void)
{
  return g_object_new (GTK_TYPE_LIST_VIEW, NULL);
}

/**
 * gtk_list_view_new_with_factory:
 * @factory: (transfer full): The factory to populate items with
 *
 * Creates a new #GtkListView that uses the given @factory for
 * mapping items to widgets.
 *
 * You most likely want to call gtk_list_view_set_model() to set
 * a model next.
 *
 * The function takes ownership of the
 * argument, so you can write code like
 * ```
 *   list_view = gtk_list_view_new_with_factory (
 *     gtk_builder_list_item_factory_newfrom_resource ("/resource.ui"));
 * ```
 *
 * Returns: a new #GtkListView using the given @factory
 **/
GtkWidget *
gtk_list_view_new_with_factory (GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_LIST_VIEW,
                         "factory", factory,
                         NULL);

  g_object_unref (factory);

  return result;
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
      gtk_list_view_set_anchor (self, 0, 0.0, TRUE);

      g_object_unref (selection_model);
    }
  else
    {
      gtk_list_item_manager_set_model (self->item_manager, NULL);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_list_view_get_factory:
 * @self: a #GtkListView
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_list_view_get_factory (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return gtk_list_item_manager_get_factory (self->item_manager);
}

/**
 * gtk_list_view_set_factory:
 * @self: a #GtkListView
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items.
 **/
void
gtk_list_view_set_factory (GtkListView        *self,
                           GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (factory == gtk_list_item_manager_get_factory (self->item_manager))
    return;

  gtk_list_item_manager_set_factory (self->item_manager, factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_list_view_set_show_separators:
 * @self: a #GtkListView
 * @show_separators: %TRUE to show separators
 *
 * Sets whether the list box should show separators
 * between rows.
 */
void
gtk_list_view_set_show_separators (GtkListView *self,
                                   gboolean     show_separators)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (self->show_separators == show_separators)
    return;

  self->show_separators = show_separators;

  if (show_separators)
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "separators");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "separators");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SEPARATORS]);
}

/**
 * gtk_list_view_get_show_separators:
 * @self: a #GtkListView
 *
 * Returns whether the list box should show separators
 * between rows.
 *
 * Returns: %TRUE if the list box shows separators
 */
gboolean
gtk_list_view_get_show_separators (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return self->show_separators;
}
