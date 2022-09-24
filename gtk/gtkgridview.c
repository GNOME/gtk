/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkgridview.h"

#include "gtkbitset.h"
#include "gtklistbaseprivate.h"
#include "gtklistitemfactory.h"
#include "gtklistitemmanagerprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtksingleselection.h"
#include "gtkwidgetprivate.h"
#include "gtkmultiselection.h"

/* Maximum number of list items created by the gridview.
 * For debugging, you can set this to G_MAXUINT to ensure
 * there's always a list item for every row.
 *
 * We multiply this number with GtkGridView:max-columns so
 * that we can always display at least this many rows.
 */
#define GTK_GRID_VIEW_MAX_VISIBLE_ROWS (30)

#define DEFAULT_MAX_COLUMNS (7)

/**
 * GtkGridView:
 *
 * `GtkGridView` presents a large dynamic grid of items.
 *
 * `GtkGridView` uses its factory to generate one child widget for each
 * visible item and shows them in a grid. The orientation of the grid view
 * determines if the grid reflows vertically or horizontally.
 *
 * `GtkGridView` allows the user to select items according to the selection
 * characteristics of the model. For models that allow multiple selected items,
 * it is possible to turn on _rubberband selection_, using
 * [property@Gtk.GridView:enable-rubberband].
 *
 * To learn more about the list widget framework, see the
 * [overview](section-list-widget.html).
 *
 * # CSS nodes
 *
 * ```
 * gridview
 * ├── child[.activatable]
 * │
 * ├── child[.activatable]
 * │
 * ┊
 * ╰── [rubberband]
 * ```
 *
 * `GtkGridView` uses a single CSS node with name `gridview`. Each child uses
 * a single CSS node with name `child`. If the [property@Gtk.ListItem:activatable]
 * property is set, the corresponding row will have the `.activatable` style
 * class. For rubberband selection, a subnode with name `rubberband` is used.
 *
 * # Accessibility
 *
 * `GtkGridView` uses the %GTK_ACCESSIBLE_ROLE_GRID role, and the items
 * use the %GTK_ACCESSIBLE_ROLE_GRID_CELL role.
 */

typedef struct _Cell Cell;
typedef struct _CellAugment CellAugment;

struct _GtkGridView
{
  GtkListBase parent_instance;

  GtkListItemManager *item_manager;
  guint min_columns;
  guint max_columns;
  /* set in size_allocate */
  guint n_columns;
  int unknown_row_height;
  double column_width;
};

struct _GtkGridViewClass
{
  GtkListBaseClass parent_class;
};

struct _Cell
{
  GtkListItemManagerItem parent;
  guint size; /* total, only counting cells in first column */
};

struct _CellAugment
{
  GtkListItemManagerItemAugment parent;
  guint size; /* total, only counting first column */
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_MAX_COLUMNS,
  PROP_MIN_COLUMNS,
  PROP_MODEL,
  PROP_SINGLE_CLICK_ACTIVATE,
  PROP_ENABLE_RUBBERBAND,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkGridView, gtk_grid_view, GTK_TYPE_LIST_BASE)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void G_GNUC_UNUSED
dump (GtkGridView *self)
{
  Cell *cell;
  guint n_widgets, n_list_rows, n_items;

  n_widgets = 0;
  n_list_rows = 0;
  n_items = 0;
  //g_print ("ANCHOR: %u - %u\n", self->anchor_start, self->anchor_end);
  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (cell->parent.widget)
        n_widgets++;
      n_list_rows++;
      n_items += cell->parent.n_items;
      g_print ("%6u%6u %5ux%3u %s (%upx)\n",
               cell->parent.n_items, n_items,
               n_items / (self->n_columns ? self->n_columns : self->min_columns),
               n_items % (self->n_columns ? self->n_columns : self->min_columns),
               cell->parent.widget ? " (widget)" : "", cell->size);
    }

  g_print ("  => %u widgets in %u list rows\n", n_widgets, n_list_rows);
}

static void
cell_augment (GtkRbTree *tree,
              gpointer   node_augment,
              gpointer   node,
              gpointer   left,
              gpointer   right)
{
  Cell *cell = node;
  CellAugment *aug = node_augment;

  gtk_list_item_manager_augment_node (tree, node_augment, node, left, right);

  aug->size = cell->size;

  if (left)
    {
      CellAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->size += left_aug->size;
    }

  if (right)
    {
      CellAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->size += right_aug->size;
    }
}

/*<private>
 * gtk_grid_view_get_cell_at_y:
 * @self: a `GtkGridView`
 * @y: an offset in direction of @self's orientation
 * @position: (out caller-allocates) (optional): stores the position
 *   index of the returned row
 * @offset: (out caller-allocates) (optional): stores the offset
 *   in pixels between y and top of cell.
 * @size: (out caller-allocates) (optional): stores the height
 *   of the cell
 *
 * Gets the Cell that occupies the leftmost position in the row at offset
 * @y into the primary direction.
 *
 * If y is larger than the height of all cells, %NULL will be returned.
 * In particular that means that for an empty grid, %NULL is returned
 * for any value.
 *
 * Returns: (nullable): The first cell at offset y
 **/
static Cell *
gtk_grid_view_get_cell_at_y (GtkGridView *self,
                             int          y,
                             guint       *position,
                             int         *offset,
                             int         *size)
{
  Cell *cell, *tmp;
  guint pos;

  cell = gtk_list_item_manager_get_root (self->item_manager);
  pos = 0;

  while (cell)
    {
      tmp = gtk_rb_tree_node_get_left (cell);
      if (tmp)
        {
          CellAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, tmp);
          if (y < aug->size)
            {
              cell = tmp;
              continue;
            }
          y -= aug->size;
          pos += aug->parent.n_items;
        }

      if (y < cell->size)
        break;
      y -= cell->size;
      pos += cell->parent.n_items;

      cell = gtk_rb_tree_node_get_right (cell);
    }

  if (cell == NULL)
    {
      if (position)
        *position = 0;
      if (offset)
        *offset = 0;
      if (size)
        *size = 0;
      return NULL;
    }

  /* We know have the (range of) cell(s) that contains this offset.
   * Now for the hard part of computing which index this actually is.
   */
  if (offset || position || size)
    {
      guint n_items = cell->parent.n_items;
      guint no_widget_rows, skip;

      /* skip remaining items at end of row */
      if (pos % self->n_columns)
        {
          skip = self->n_columns - pos % self->n_columns;
          if (n_items <= skip)
            {
              g_warning ("ran out of items");
              if (position)
                *position = 0;
              if (offset)
                *offset = 0;
              if (size)
                *size = 0;
              return NULL;
            }
          n_items -= skip;
          pos += skip;
        }

      /* Skip all the rows this index doesn't go into */
      no_widget_rows = (n_items - 1) / self->n_columns;
      skip = MIN (y / self->unknown_row_height, no_widget_rows);
      y -= skip * self->unknown_row_height;
      pos += self->n_columns * skip;

      if (position)
        *position = pos;
      if (offset)
        *offset = y;
      if (size)
        {
          if (skip < no_widget_rows)
            *size = self->unknown_row_height;
          else
            *size = cell->size - no_widget_rows * self->unknown_row_height;
        }
    }

  return cell;
}

static gboolean
gtk_grid_view_get_allocation_along (GtkListBase *base,
                                    guint        pos,
                                    int         *offset,
                                    int         *size)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  Cell *cell, *tmp;
  int y;

  cell = gtk_list_item_manager_get_root (self->item_manager);
  y = 0;
  pos -= pos % self->n_columns;

  while (cell)
    {
      tmp = gtk_rb_tree_node_get_left (cell);
      if (tmp)
        {
          CellAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, tmp);
          if (pos < aug->parent.n_items)
            {
              cell = tmp;
              continue;
            }
          pos -= aug->parent.n_items;
          y += aug->size;
        }

      if (pos < cell->parent.n_items)
        break;
      y += cell->size;
      pos -= cell->parent.n_items;

      cell = gtk_rb_tree_node_get_right (cell);
    }

  if (cell == NULL)
    {
      if (offset)
        *offset = 0;
      if (size)
        *size = 0;
      return FALSE;
    }

  /* We know have the (range of) cell(s) that contains this offset.
   * Now for the hard part of computing which index this actually is.
   */
  if (offset || size)
    {
      guint n_items = cell->parent.n_items;
      guint skip;

      /* skip remaining items at end of row */
      if (pos % self->n_columns)
        {
          skip = pos % self->n_columns;
          n_items -= skip;
          pos -= skip;
        }

      /* Skip all the rows this index doesn't go into */
      skip = pos / self->n_columns;
      n_items -= skip * self->n_columns;
      y += skip * self->unknown_row_height;

      if (offset)
        *offset = y;
      if (size)
        {
          if (n_items > self->n_columns)
            *size = self->unknown_row_height;
          else
            *size = cell->size - skip * self->unknown_row_height;
        }
    }

  return TRUE;
}

static gboolean
gtk_grid_view_get_allocation_across (GtkListBase *base,
                                     guint        pos,
                                     int         *offset,
                                     int         *size)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  guint start;

  pos %= self->n_columns;
  start = ceil (self->column_width * pos);

  if (offset)
    *offset = start;
  if (size)
    *size = ceil (self->column_width * (pos + 1)) - start;

  return TRUE;
}

static int
gtk_grid_view_compute_total_height (GtkGridView *self)
{
  Cell *cell;
  CellAugment *aug;

  cell = gtk_list_item_manager_get_root (self->item_manager);
  if (cell == NULL)
    return 0;
  aug = gtk_list_item_manager_get_item_augment (self->item_manager, cell);
  return aug->size;
}

static gboolean
gtk_grid_view_get_position_from_allocation (GtkListBase           *base,
                                            int                    across,
                                            int                    along,
                                            guint                 *position,
                                            cairo_rectangle_int_t *area)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  int offset, size;
  guint pos, n_items;

  if (across >= self->column_width * self->n_columns)
    return FALSE;

  n_items = gtk_list_base_get_n_items (base);
  along = CLAMP (along, 0, gtk_grid_view_compute_total_height (self) - 1);
  across = across < 0 ? 0 : across;

  if (!gtk_grid_view_get_cell_at_y (self,
                                    along,
                                    &pos,
                                    &offset,
                                    &size))
    return FALSE;

  pos += floor (across / self->column_width);

  if (pos >= n_items)
    {
      /* Ugh, we're in the last row and don't have enough items
       * to fill the row.
       * Do it the hard way then... */
      pos = n_items - 1;
    }

  *position = pos;
  if (area)
    {
      area->x = ceil (self->column_width * (pos % self->n_columns));
      area->width = ceil (self->column_width * (1 + pos % self->n_columns)) - area->x;
      area->y = along - offset;
      area->height = size;
    }

  return TRUE;
}

static GtkBitset *
gtk_grid_view_get_items_in_rect (GtkListBase        *base,
                                 const GdkRectangle *rect)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  guint first_row, last_row, first_column, last_column, n_items;
  GtkBitset *result;

  result = gtk_bitset_new_empty ();

  if (rect->y >= gtk_grid_view_compute_total_height (self))
    return result;

  n_items = gtk_list_base_get_n_items (base);
  if (n_items == 0)
    return result;

  first_column = fmax (floor (rect->x / self->column_width), 0);
  last_column = fmin (floor ((rect->x + rect->width) / self->column_width), self->n_columns - 1);
  if (!gtk_grid_view_get_cell_at_y (self, rect->y, &first_row, NULL, NULL))
    first_row = rect->y < 0 ? 0 : n_items - 1;
  if (!gtk_grid_view_get_cell_at_y (self, rect->y + rect->height, &last_row, NULL, NULL))
    last_row = rect->y + rect->height < 0 ? 0 : n_items - 1;

  gtk_bitset_add_rectangle (result,
                            first_row + first_column,
                            last_column - first_column + 1,
                            (last_row - first_row) / self->n_columns + 1,
                            self->n_columns);

  return result;
}

static guint
gtk_grid_view_move_focus_along (GtkListBase *base,
                                guint        pos,
                                int          steps)
{
  GtkGridView *self = GTK_GRID_VIEW (base);

  steps *= self->n_columns;

  if (steps < 0)
    {
      if (pos >= self->n_columns)
        pos -= MIN (pos, -steps);
    }
  else
    {
      guint n_items = gtk_list_base_get_n_items (base);
      if (n_items / self->n_columns > pos / self->n_columns)
        pos += MIN (n_items - pos - 1, steps);
    }

  return pos;
}

static guint
gtk_grid_view_move_focus_across (GtkListBase *base,
                                 guint        pos,
                                 int          steps)
{
  if (steps < 0)
    return pos - MIN (pos, -steps);
  else
    {
      guint n_items = gtk_list_base_get_n_items (base);
      pos += MIN (n_items - pos - 1, steps);
    }

  return pos;
}

static int
compare_ints (gconstpointer first,
              gconstpointer second)
{
  return *(int *) first - *(int *) second;
}

static int
gtk_grid_view_get_unknown_row_size (GtkGridView *self,
                                    GArray      *heights)
{
  g_return_val_if_fail (heights->len > 0, 0);

  /* return the median and hope rows are generally uniform with few outliers */
  g_array_sort (heights, compare_ints);

  return g_array_index (heights, int, heights->len / 2);
}

static void
gtk_grid_view_measure_column_size (GtkGridView *self,
                                   int         *minimum,
                                   int         *natural)
{
  GtkOrientation opposite;
  Cell *cell;
  int min, nat, child_min, child_nat;

  min = 0;
  nat = 0;
  opposite = gtk_list_base_get_opposite_orientation (GTK_LIST_BASE (self));

  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      /* ignore unavailable cells */
      if (cell->parent.widget == NULL)
        continue;

      gtk_widget_measure (cell->parent.widget,
                          opposite, -1,
                          &child_min, &child_nat, NULL, NULL);
      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

  *minimum = min;
  *natural = nat;
}

static void
gtk_grid_view_measure_across (GtkWidget *widget,
                              int        for_size,
                              int       *minimum,
                              int       *natural)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);

  gtk_grid_view_measure_column_size (self, minimum, natural);

  *minimum *= self->min_columns;
  *natural *= self->max_columns;
}

static guint
gtk_grid_view_compute_n_columns (GtkGridView *self,
                                 guint        for_size,
                                 int          min,
                                 int          nat)
{
  guint n_columns;

  /* rounding down is exactly what we want here, so int division works */
  if (gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self),
                                       gtk_list_base_get_opposite_orientation (GTK_LIST_BASE (self))) == GTK_SCROLL_MINIMUM)
    n_columns = for_size / MAX (1, min);
  else
    n_columns = for_size / MAX (1, nat);

  n_columns = CLAMP (n_columns, self->min_columns, self->max_columns);

  return n_columns;
}

static void
gtk_grid_view_measure_list (GtkWidget *widget,
                            int        for_size,
                            int       *minimum,
                            int       *natural)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);
  GtkScrollablePolicy scroll_policy;
  Cell *cell;
  int height, row_height, child_min, child_nat, column_size, col_min, col_nat;
  gboolean measured;
  GArray *heights;
  guint n_unknown, n_columns;
  guint i;

  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), gtk_list_base_get_orientation (GTK_LIST_BASE (self)));
  heights = g_array_new (FALSE, FALSE, sizeof (int));
  n_unknown = 0;
  height = 0;

  gtk_grid_view_measure_column_size (self, &col_min, &col_nat);
  for_size = MAX (for_size, col_min * (int) self->min_columns);
  n_columns = gtk_grid_view_compute_n_columns (self, for_size, col_min, col_nat);
  column_size = for_size / n_columns;

  i = 0;
  row_height = 0;
  measured = FALSE;
  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (cell->parent.widget)
        {
          gtk_widget_measure (cell->parent.widget,
                              gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                              column_size,
                              &child_min, &child_nat, NULL, NULL);
          if (scroll_policy == GTK_SCROLL_MINIMUM)
            row_height = MAX (row_height, child_min);
          else
            row_height = MAX (row_height, child_nat);
          measured = TRUE;
        }
      
      i += cell->parent.n_items;

      if (i >= n_columns)
        {
          if (measured)
            {
              g_array_append_val (heights, row_height);
              i -= n_columns;
              height += row_height;
              measured = FALSE;
              row_height = 0;
            }
          n_unknown += i / n_columns;
          i %= n_columns;
        }
    }

  if (i > 0)
    {
      if (measured)
        {
          g_array_append_val (heights, row_height);
          height += row_height;
        }
      else
        n_unknown++;
    }

  if (n_unknown)
    height += n_unknown * gtk_grid_view_get_unknown_row_size (self, heights);

  g_array_free (heights, TRUE);

  *minimum = height;
  *natural = height;
}

static void
gtk_grid_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);

  if (orientation == gtk_list_base_get_orientation (GTK_LIST_BASE (self)))
    gtk_grid_view_measure_list (widget, for_size, minimum, natural);
  else
    gtk_grid_view_measure_across (widget, for_size, minimum, natural);
}

static void
cell_set_size (Cell  *cell,
               guint  size)
{
  if (cell->size == size)
    return;

  cell->size = size;
  gtk_rb_tree_node_mark_dirty (cell);
}

static void
gtk_grid_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);
  Cell *cell, *start;
  GArray *heights;
  int min_row_height, row_height, col_min, col_nat;
  GtkOrientation orientation, opposite_orientation;
  GtkScrollablePolicy scroll_policy;
  gboolean known;
  int x, y;
  guint i;

  orientation = gtk_list_base_get_orientation (GTK_LIST_BASE (self));
  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), orientation);
  opposite_orientation = OPPOSITE_ORIENTATION (orientation);
  min_row_height = ceil ((double) height / GTK_GRID_VIEW_MAX_VISIBLE_ROWS);

  /* step 0: exit early if list is empty */
  if (gtk_list_item_manager_get_root (self->item_manager) == NULL)
    {
      gtk_list_base_update_adjustments (GTK_LIST_BASE (self), 0, 0, 0, 0, &x, &y);
      return;
    }

  /* step 1: determine width of the list */
  gtk_grid_view_measure_column_size (self, &col_min, &col_nat);
  self->n_columns = gtk_grid_view_compute_n_columns (self, 
                                                     orientation == GTK_ORIENTATION_VERTICAL ? width : height,
                                                     col_min, col_nat);
  self->column_width = (orientation == GTK_ORIENTATION_VERTICAL ? width : height) / self->n_columns;
  self->column_width = MAX (self->column_width, col_min);

  /* step 2: determine height of known rows */
  heights = g_array_new (FALSE, FALSE, sizeof (int));

  i = 0;
  row_height = 0;
  start = NULL;
  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (i == 0)
        start = cell;
      
      if (cell->parent.widget)
        {
          int min, nat, size;
          gtk_widget_measure (cell->parent.widget,
                              gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                              self->column_width,
                              &min, &nat, NULL, NULL);
          if (scroll_policy == GTK_SCROLL_MINIMUM)
            size = min;
          else
            size = nat;
          size = MAX (size, min_row_height);
          g_array_append_val (heights, size);
          row_height = MAX (row_height, size);
        }
      cell_set_size (cell, 0);
      i += cell->parent.n_items;

      if (i >= self->n_columns)
        {
          i %= self->n_columns;

          cell_set_size (start, start->size + row_height);
          start = cell;
          row_height = 0;
        }
    }
  if (i > 0)
    cell_set_size (start, start->size + row_height);

  /* step 3: determine height of rows with only unknown items */
  self->unknown_row_height = gtk_grid_view_get_unknown_row_size (self, heights);
  g_array_free (heights, TRUE);

  i = 0;
  known = FALSE;
  for (start = cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (i == 0)
        start = cell;

      if (cell->parent.widget)
        known = TRUE;

      i += cell->parent.n_items;
      if (i >= self->n_columns)
        {
          if (!known)
            cell_set_size (start, start->size + self->unknown_row_height);

          i -= self->n_columns;
          known = FALSE;

          if (i >= self->n_columns)
            {
              cell_set_size (cell, cell->size + self->unknown_row_height * (i / self->n_columns));
              i %= self->n_columns;
            }
          start = cell;
        }
    }
  if (i > 0 && !known)
    cell_set_size (start, start->size + self->unknown_row_height);

  /* step 4: update the adjustments */
  gtk_list_base_update_adjustments (GTK_LIST_BASE (self),
                                    self->column_width * self->n_columns,
                                    gtk_grid_view_compute_total_height (self),
                                    gtk_widget_get_size (widget, opposite_orientation),
                                    gtk_widget_get_size (widget, orientation),
                                    &x, &y);

  /* step 5: run the size_allocate loop */
  x = -x;
  y = -y;
  i = 0;
  row_height = 0;

  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (cell->parent.widget)
        {
          row_height += cell->size;

          gtk_list_base_size_allocate_child (GTK_LIST_BASE (self),
                                             cell->parent.widget,
                                             x + ceil (self->column_width * i),
                                             y,
                                             ceil (self->column_width * (i + 1)) - ceil (self->column_width * i),
                                             row_height);
          i++;
          if (i >= self->n_columns)
            {
              y += row_height;
              i -= self->n_columns;
              row_height = 0;
            }
        }
      else
        {
          i += cell->parent.n_items;
          /* skip remaining row if we didn't start one */
          if (i > cell->parent.n_items && i >= self->n_columns)
            {
              i -= self->n_columns;
              y += row_height;
              row_height = 0;
            }

          row_height += cell->size;

          /* skip rows that are completely contained by this cell */
          if (i >= self->n_columns)
            {
              guint unknown_rows, unknown_height;

              unknown_rows = i / self->n_columns;
              unknown_height = unknown_rows * self->unknown_row_height;
              row_height -= unknown_height;
              y += unknown_height;
              i %= self->n_columns;
              g_assert (row_height >= 0);
            }
        }
    }

  gtk_list_base_allocate_rubberband (GTK_LIST_BASE (widget));
}

static void
gtk_grid_view_dispose (GObject *object)
{
  GtkGridView *self = GTK_GRID_VIEW (object);

  self->item_manager = NULL;

  G_OBJECT_CLASS (gtk_grid_view_parent_class)->dispose (object);
}

static void
gtk_grid_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkGridView *self = GTK_GRID_VIEW (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, gtk_list_item_manager_get_factory (self->item_manager));
      break;

    case PROP_MAX_COLUMNS:
      g_value_set_uint (value, self->max_columns);
      break;

    case PROP_MIN_COLUMNS:
      g_value_set_uint (value, self->min_columns);
      break;

    case PROP_MODEL:
      g_value_set_object (value, gtk_list_base_get_model (GTK_LIST_BASE (self)));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      g_value_set_boolean (value, gtk_list_item_manager_get_single_click_activate (self->item_manager));
      break;

    case PROP_ENABLE_RUBBERBAND:
      g_value_set_boolean (value, gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_grid_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkGridView *self = GTK_GRID_VIEW (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_grid_view_set_factory (self, g_value_get_object (value));
      break;

    case PROP_MAX_COLUMNS:
      gtk_grid_view_set_max_columns (self, g_value_get_uint (value));
      break;

    case PROP_MIN_COLUMNS:
      gtk_grid_view_set_min_columns (self, g_value_get_uint (value));
      break;
 
    case PROP_MODEL:
      gtk_grid_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_grid_view_set_single_click_activate (self, g_value_get_boolean (value));
      break;

    case PROP_ENABLE_RUBBERBAND:
      gtk_grid_view_set_enable_rubberband (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_grid_view_activate_item (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);
  if (pos >= gtk_list_base_get_n_items (GTK_LIST_BASE (self)))
    return;

  g_signal_emit (widget, signals[ACTIVATE], 0, pos);
}

static void
gtk_grid_view_class_init (GtkGridViewClass *klass)
{
  GtkListBaseClass *list_base_class = GTK_LIST_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  list_base_class->list_item_name = "child";
  list_base_class->list_item_role = GTK_ACCESSIBLE_ROLE_GRID_CELL;
  list_base_class->list_item_size = sizeof (Cell);
  list_base_class->list_item_augment_size = sizeof (CellAugment);
  list_base_class->list_item_augment_func = cell_augment;
  list_base_class->get_allocation_along = gtk_grid_view_get_allocation_along;
  list_base_class->get_allocation_across = gtk_grid_view_get_allocation_across;
  list_base_class->get_items_in_rect = gtk_grid_view_get_items_in_rect;
  list_base_class->get_position_from_allocation = gtk_grid_view_get_position_from_allocation;
  list_base_class->move_focus_along = gtk_grid_view_move_focus_along;
  list_base_class->move_focus_across = gtk_grid_view_move_focus_across;

  widget_class->measure = gtk_grid_view_measure;
  widget_class->size_allocate = gtk_grid_view_size_allocate;

  gobject_class->dispose = gtk_grid_view_dispose;
  gobject_class->get_property = gtk_grid_view_get_property;
  gobject_class->set_property = gtk_grid_view_set_property;

  /**
   * GtkGridView:factory: (attributes org.gtk.Property.get=gtk_grid_view_get_factory org.gtk.Property.set=gtk_grid_view_set_factory)
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);


  /**
   * GtkGridView:max-columns: (attributes org.gtk.Property.get=gtk_grid_view_get_max_columns org.gtk.Property.set=gtk_grid_view_set_max_columns)
   *
   * Maximum number of columns per row.
   *
   * If this number is smaller than [property@Gtk.GridView:min-columns],
   * that value is used instead.
   */
  properties[PROP_MAX_COLUMNS] =
    g_param_spec_uint ("max-columns", NULL, NULL,
                       1, G_MAXUINT, DEFAULT_MAX_COLUMNS,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:min-columns: (attributes org.gtk.Property.get=gtk_grid_view_get_min_columns org.gtk.Property.set=gtk_grid_view_set_min_columns)
   *
   * Minimum number of columns per row.
   */
  properties[PROP_MIN_COLUMNS] =
    g_param_spec_uint ("min-columns", NULL, NULL,
                       1, G_MAXUINT, 1,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:model: (attributes org.gtk.Property.get=gtk_grid_view_get_model org.gtk.Property.set=gtk_grid_view_set_model)
   *
   * Model for the items displayed.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         GTK_TYPE_SELECTION_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:single-click-activate: (attributes org.gtk.Property.get=gtk_grid_view_get_single_click_activate org.gtk.Property.set=gtk_grid_view_set_single_click_activate)
   *
   * Activate rows on single click and select them on hover.
   */
  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridView:enable-rubberband: (attributes org.gtk.Property.get=gtk_grid_view_get_enable_rubberband org.gtk.Property.set=gtk_grid_view_set_enable_rubberband)
   *
   * Allow rubberband selection.
   */
  properties[PROP_ENABLE_RUBBERBAND] =
    g_param_spec_boolean ("enable-rubberband", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkGridView::activate:
   * @self: The `GtkGridView`
   * @position: position of item to activate
   *
   * Emitted when a cell has been activated by the user,
   * usually via activating the GtkGridView|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a gridview.
   * See [property@Gtk.ListItem:activatable] for details on how to use
   * this signal.
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
   * GtkGridView|list.activate-item:
   * @position: position of item to activate
   *
   * Activates the item given in @position by emitting the
   * [signal@Gtk.GridView::activate] signal.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.activate-item",
                                   "u",
                                   gtk_grid_view_activate_item);

  gtk_widget_class_set_css_name (widget_class, I_("gridview"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GRID);
}

static void
gtk_grid_view_init (GtkGridView *self)
{
  self->item_manager = gtk_list_base_get_manager (GTK_LIST_BASE (self));

  self->min_columns = 1;
  self->max_columns = DEFAULT_MAX_COLUMNS;
  self->n_columns = 1;

  gtk_list_base_set_anchor_max_widgets (GTK_LIST_BASE (self),
                                        self->max_columns * GTK_GRID_VIEW_MAX_VISIBLE_ROWS,
                                        self->max_columns);

  gtk_widget_add_css_class (GTK_WIDGET (self), "view");
}

/**
 * gtk_grid_view_new:
 * @model: (nullable) (transfer full): the model to use
 * @factory: (nullable) (transfer full): The factory to populate items with
 *
 * Creates a new `GtkGridView` that uses the given @factory for
 * mapping items to widgets.
 *
 * The function takes ownership of the
 * arguments, so you can write code like
 * ```c
 * grid_view = gtk_grid_view_new (create_model (),
 *   gtk_builder_list_item_factory_new_from_resource ("/resource.ui"));
 * ```
 *
 * Returns: a new `GtkGridView` using the given @model and @factory
 */
GtkWidget *
gtk_grid_view_new (GtkSelectionModel  *model,
                   GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model), NULL);
  g_return_val_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_GRID_VIEW,
                         "model", model,
                         "factory", factory,
                         NULL);

  /* consume the references */
  g_clear_object (&model);
  g_clear_object (&factory);

  return result;
}

/**
 * gtk_grid_view_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkGridView`
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GtkSelectionModel *
gtk_grid_view_get_model (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), NULL);

  return gtk_list_base_get_model (GTK_LIST_BASE (self));
}

/**
 * gtk_grid_view_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkGridView`
 * @model: (nullable) (transfer none): the model to use
 *
 * Sets the imodel to use.
 *
 * This must be a [iface@Gtk.SelectionModel].
 */
void
gtk_grid_view_set_model (GtkGridView       *self,
                         GtkSelectionModel *model)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));

  if (!gtk_list_base_set_model (GTK_LIST_BASE (self), model))
    return;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, GTK_IS_MULTI_SELECTION (model),
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_grid_view_get_factory: (attributes org.gtk.Method.get_property=factory)
 * @self: a `GtkGridView`
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 */
GtkListItemFactory *
gtk_grid_view_get_factory (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), NULL);

  return gtk_list_item_manager_get_factory (self->item_manager);
}

/**
 * gtk_grid_view_set_factory: (attributes org.gtk.Method.set_property=factory)
 * @self: a `GtkGridView`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating list items.
 */
void
gtk_grid_view_set_factory (GtkGridView        *self,
                           GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  if (factory == gtk_list_item_manager_get_factory (self->item_manager))
    return;

  gtk_list_item_manager_set_factory (self->item_manager, factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_grid_view_get_max_columns: (attributes org.gtk.Method.get_property=max-columns)
 * @self: a `GtkGridView`
 *
 * Gets the maximum number of columns that the grid will use.
 *
 * Returns: The maximum number of columns
 */
guint
gtk_grid_view_get_max_columns (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), DEFAULT_MAX_COLUMNS);

  return self->max_columns;
}

/**
 * gtk_grid_view_set_max_columns: (attributes org.gtk.Method.set_property=max-columns)
 * @self: a `GtkGridView`
 * @max_columns: The maximum number of columns
 *
 * Sets the maximum number of columns to use.
 *
 * This number must be at least 1.
 *
 * If @max_columns is smaller than the minimum set via
 * [method@Gtk.GridView.set_min_columns], that value is used instead.
 */
void
gtk_grid_view_set_max_columns (GtkGridView *self,
                               guint        max_columns)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (max_columns > 0);

  if (self->max_columns == max_columns)
    return;

  self->max_columns = max_columns;

  gtk_list_base_set_anchor_max_widgets (GTK_LIST_BASE (self),
                                        self->max_columns * GTK_GRID_VIEW_MAX_VISIBLE_ROWS,
                                        self->max_columns);

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_COLUMNS]);
}

/**
 * gtk_grid_view_get_min_columns: (attributes org.gtk.Method.get_property=min-columns)
 * @self: a `GtkGridView`
 *
 * Gets the minimum number of columns that the grid will use.
 *
 * Returns: The minimum number of columns
 */
guint
gtk_grid_view_get_min_columns (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), 1);

  return self->min_columns;
}

/**
 * gtk_grid_view_set_min_columns: (attributes org.gtk.Method.set_property=min-columns)
 * @self: a `GtkGridView`
 * @min_columns: The minimum number of columns
 *
 * Sets the minimum number of columns to use.
 *
 * This number must be at least 1.
 *
 * If @min_columns is smaller than the minimum set via
 * [method@Gtk.GridView.set_max_columns], that value is ignored.
 */
void
gtk_grid_view_set_min_columns (GtkGridView *self,
                               guint        min_columns)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (min_columns > 0);

  if (self->min_columns == min_columns)
    return;

  self->min_columns = min_columns;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_COLUMNS]);
}

/**
 * gtk_grid_view_set_single_click_activate: (attributes org.gtk.Method.set_property=single-click-activate)
 * @self: a `GtkGridView`
 * @single_click_activate: %TRUE to activate items on single click
 *
 * Sets whether items should be activated on single click and
 * selected on hover.
 */
void
gtk_grid_view_set_single_click_activate (GtkGridView *self,
                                         gboolean     single_click_activate)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));

  if (single_click_activate == gtk_list_item_manager_get_single_click_activate (self->item_manager))
    return;

  gtk_list_item_manager_set_single_click_activate (self->item_manager, single_click_activate);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SINGLE_CLICK_ACTIVATE]);
}

/**
 * gtk_grid_view_get_single_click_activate: (attributes org.gtk.Method.get_property=single-click-activate)
 * @self: a `GtkGridView`
 *
 * Returns whether items will be activated on single click and
 * selected on hover.
 *
 * Returns: %TRUE if items are activated on single click
 */
gboolean
gtk_grid_view_get_single_click_activate (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), FALSE);

  return gtk_list_item_manager_get_single_click_activate (self->item_manager);
}

/**
 * gtk_grid_view_set_enable_rubberband: (attributes org.gtk.Method.set_property=enable-rubberband)
 * @self: a `GtkGridView`
 * @enable_rubberband: %TRUE to enable rubberband selection
 *
 * Sets whether selections can be changed by dragging with the mouse.
 */
void
gtk_grid_view_set_enable_rubberband (GtkGridView *self,
                                     gboolean     enable_rubberband)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));

  if (enable_rubberband == gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)))
    return;

  gtk_list_base_set_enable_rubberband (GTK_LIST_BASE (self), enable_rubberband);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_RUBBERBAND]);
}

/**
 * gtk_grid_view_get_enable_rubberband: (attributes org.gtk.Method.get_property=enable-rubberband)
 * @self: a `GtkGridView`
 *
 * Returns whether rows can be selected by dragging with the mouse.
 *
 * Returns: %TRUE if rubberband selection is enabled
 */
gboolean
gtk_grid_view_get_enable_rubberband (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), FALSE);

  return gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self));
}
