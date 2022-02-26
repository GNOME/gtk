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
#include "gtkintl.h"
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
  double column_width;
};

struct _GtkGridViewClass
{
  GtkListBaseClass parent_class;
};

struct _Cell
{
  GtkListItemManagerItem parent;
  enum {
    /* A newly created cell that hasn't yet been allocated */
    UNKNOWN,
    /* One or more cells in the same row.
     * May contain a widget, but don't have to.
     */
    CELL,
    /* One or more rows spanning all columns.
     * Does not contain a widget.
     */
    BLOCK,
    /* The bottom right part of the last row.
     * A 0-size cell that contains the bottom right area of
     * a grid where the number of items is not a whole multiple
     * of the columns
     */
    FILLER
  } cell_type;
  int height;
  int section_height;
  int col_start;
  int col_end;
};

struct _CellAugment
{
  GtkListItemManagerItemAugment parent;
  int height;
  int col_start;
  int col_end;
};

enum
{
  PROP_0,
  PROP_ENABLE_RUBBERBAND,
  PROP_FACTORY,
  PROP_MAX_COLUMNS,
  PROP_MIN_COLUMNS,
  PROP_MODEL,
  PROP_SECTION_FACTORY,
  PROP_SINGLE_CLICK_ACTIVATE,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkGridView, gtk_grid_view, GTK_TYPE_LIST_BASE)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };


#include "gdk/gdkrgbaprivate.h"
#include "gtkadjustment.h"
#include "gtkscrollable.h"
#include "gtksnapshot.h"

static inline int
column_x (GtkGridView *self,
          guint        column)
{
  return ceil (column * self->column_width);
}

static inline guint
column_from_x (GtkGridView *self,
               double       x)
{
  return floor (x / self->column_width);
}

static inline int
cell_x (GtkGridView *self,
        Cell        *cell)
{
  return column_x (self, cell->col_start);
}

static inline int
cell_width (GtkGridView *self,
            Cell        *cell)
{
  return column_x (self, cell->col_end) - column_x (self, cell->col_start);
}

static inline int
cell_y (GtkGridView *self,
        Cell        *cell)
{
  CellAugment *left_aug;
  Cell *prev, *left;
  int y;

  left = gtk_rb_tree_node_get_left (cell);
  if (left != NULL)
    {
      left_aug = gtk_list_item_manager_get_item_augment (self->item_manager, left);
      y = left_aug->height;
      if (cell->col_start != 0)
        y -= cell->height;
    }
  else
    {
      y = 0;
    }

  prev = cell;
  for (cell = gtk_rb_tree_node_get_parent (cell);
       cell != NULL;
       cell = gtk_rb_tree_node_get_parent (cell))
    {
      left = gtk_rb_tree_node_get_left (cell);
      if (left != prev)
        {
          if (left != NULL)
            {
              left_aug = gtk_list_item_manager_get_item_augment (self->item_manager, left);
              y += left_aug->height;
            }
          if (cell->col_start == 0)
            y += cell->height;
          if (cell->col_end != self->n_columns)
            y -= cell->height;
        }
      prev = cell;
    }

  return y;
}

static inline int
cell_height (GtkGridView *self,
             Cell        *cell)
{
  return cell->section_height + cell->height;
}

static void G_GNUC_UNUSED
dump (GtkGridView   *self,
      const GdkRGBA *bg)
{
  Cell *cell;
  gsize n;
  char *filename;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GdkRGBA black = GDK_RGBA("000000");
  float scale = 8.0;
  GtkAdjustment *hadj, *vadj;
  int y;

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (self));
  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));
  if (gtk_adjustment_get_upper (hadj) <= 0 ||
      gtk_adjustment_get_upper (vadj) <= 0)
    return;

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_scale (snapshot, 1 / scale, 1 / scale);

  gtk_snapshot_append_color (snapshot,
                             bg,
                             &GRAPHENE_RECT_INIT (gtk_adjustment_get_lower (hadj),
                                                  gtk_adjustment_get_lower (vadj),
                                                  gtk_adjustment_get_upper (hadj),
                                                  gtk_adjustment_get_upper (vadj)));

  y = 0;
  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      GskRoundedRect r = GSK_ROUNDED_RECT_INIT (cell_x (self, cell), y, cell_width (self, cell), cell_height (self, cell));

      switch (cell->cell_type)
        {
        case UNKNOWN:
          break;

        case CELL:
          if (cell->parent.widget)
            gtk_snapshot_append_color (snapshot, &GDK_RGBA("FFFFFF"), &r.bounds);
          if (cell->section_height)
            gtk_snapshot_append_color (snapshot,
                                       &GDK_RGBA("7F7F00"),
                                       &GRAPHENE_RECT_INIT (r.bounds.origin.x,
                                                            r.bounds.origin.y,
                                                            r.bounds.size.width,
                                                            cell->section_height));

          gtk_snapshot_append_border (snapshot,
                                      &r,
                                      (float[4]) { scale, scale, scale, scale },
                                      (GdkRGBA[4]) { black, black, black, black });
          if (cell->col_end == self->n_columns)
            y += cell_height (self, cell);
          break;

        case FILLER:
          gtk_snapshot_push_repeat (snapshot, &r.bounds, &GRAPHENE_RECT_INIT (0, 0, 2 * scale, 2 * scale));
          gtk_snapshot_append_color (snapshot, &black, &GRAPHENE_RECT_INIT (0, 0, scale, scale));
          gtk_snapshot_pop (snapshot);
          gtk_snapshot_append_border (snapshot,
                                      &r,
                                      (float[4]) { scale, scale, scale, scale },
                                      (GdkRGBA[4]) { black, black, black, black });
          y += cell_height (self, cell);
          break;

        case BLOCK:
          if (cell->section_height)
            gtk_snapshot_append_color (snapshot,
                                       &GDK_RGBA("7F7F00"),
                                       &GRAPHENE_RECT_INIT (r.bounds.origin.x,
                                                            r.bounds.origin.y,
                                                            r.bounds.size.width,
                                                            cell->section_height));
          gtk_snapshot_append_border (snapshot,
                                      &r,
                                      (float[4]) { scale, scale, scale, scale },
                                      (GdkRGBA[4]) { black, black, black, black });
          y += cell_height (self, cell);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
    }

  gtk_snapshot_append_color (snapshot,
                             &GDK_RGBA("3584E450"),
                             &GRAPHENE_RECT_INIT (gtk_adjustment_get_value (hadj),
                                                  gtk_adjustment_get_value (vadj),
                                                  gtk_adjustment_get_page_size (hadj),
                                                  gtk_adjustment_get_page_size (vadj)));

  node = gtk_snapshot_free_to_node (snapshot);

  n = GPOINTER_TO_SIZE (g_object_get_data (G_OBJECT (self), "-gtk-dump-counter"));
  n++;
  g_object_set_data (G_OBJECT (self), "-gtk-dump-counter", GSIZE_TO_POINTER (n));
  filename = g_strdup_printf ("gridview-%p-%zu.node", self, n);

  gsk_render_node_write_to_file (node, filename, NULL);

  g_free (filename);
  gsk_render_node_unref (node);
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

  if (left)
    {
      CellAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->height = left_aug->height;
      aug->col_start = left_aug->col_start;
      if (cell->col_start == 0)
        aug->height += cell->section_height + cell->height;
    }
  else
    {
      aug->height = cell->section_height + cell->height;
      aug->col_start = cell->col_start;
    }

  if (right)
    {
      CellAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->height += right_aug->height;
      aug->col_end = right_aug->col_end;

      if (right_aug->col_start != 0)
        aug->height -= cell->section_height + cell->height;
    }
  else
    {
      aug->col_end = cell->col_end;
    }
}

static Cell *
get_cell_at (GtkGridView *self,
             guint        col,
             int          y,
             guint       *pos_cell,
             int         *y_cell)
{
  Cell *cell, *tmp;
  guint pos;
  int y_org;

  cell = gtk_list_item_manager_get_root (self->item_manager);
  y_org = y;
  pos = 0;

  while (cell)
    {
      tmp = gtk_rb_tree_node_get_left (cell);
      if (tmp)
        {
          CellAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, tmp);
          if (y < aug->height - cell_height (self, cell) ||
              (y < aug->height && col < aug->col_end))
            {
              cell = tmp;
              continue;
            }
          if (cell->col_start == 0)
            y -= aug->height;
          else
            y -= aug->height - cell_height (self, cell);
          pos += aug->parent.n_items;
        }

      if (y < cell_height (self, cell) && col >= cell->col_start && col < cell->col_end)
        break;
      if (cell->col_end == self->n_columns)
        y -= cell_height (self, cell);
      pos += cell->parent.n_items;

      cell = gtk_rb_tree_node_get_right (cell);
    }

  if (cell == NULL)
    {
      if (pos_cell)
        *pos_cell = 0;
      if (y_cell)
        *y_cell = 0;
      return NULL;
    }

  if (pos_cell)
    *pos_cell = pos;
  if (y_cell)
    *y_cell = y_org - y;

  return cell;
}

static gboolean
gtk_grid_view_get_allocation_along (GtkListBase *base,
                                    guint        pos,
                                    int         *offset,
                                    int         *size)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  Cell *cell;
  guint remaining;

  cell = gtk_list_item_manager_get_nth (self->item_manager, pos, &remaining);
  if (cell == NULL)
    return FALSE;

  /* only multi-row cell, remaining items are irrelevant for other cells */
  if (cell->cell_type == BLOCK)
    {
      int row_height = cell->height / self->n_columns;

      remaining /= self->n_columns;

      if (offset)
        *offset = cell_y (self, cell) + remaining * row_height + (remaining ? cell->section_height : 0);
      if (size)
        *size = row_height + (remaining ? 0 : cell->section_height);
    }
  else
    {
      if (offset)
        *offset = cell_y (self, cell);
      if (size)
        *size = cell_height (self, cell);
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
  Cell *cell;
  guint remaining;

  cell = gtk_list_item_manager_get_nth (self->item_manager, pos, &remaining);
  if (cell == NULL)
    return FALSE;

  remaining %= self->n_columns;
  if (offset)
    *offset = column_x (self, cell->col_start + remaining);
  if (size)
    *size = column_x (self, cell->col_start + remaining + 1) - column_x (self, cell->col_start + remaining);
  return TRUE;
}

static gboolean
gtk_grid_view_get_position_from_allocation (GtkListBase           *base,
                                            int                    x,
                                            int                    y,
                                            guint                 *position,
                                            cairo_rectangle_int_t *area)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  Cell *cell;
  int y_offset;
  guint pos, col;

  if (x < 0 || y < 0)
    return FALSE;

  col = column_from_x (self, x);
  if (col >= self->n_columns)
    return FALSE;

  cell = get_cell_at (self,
                      col,
                      y,
                      &pos,
                      &y_offset);
  if (cell == NULL)
    return FALSE;

  while (TRUE)
    {
      switch (cell->cell_type)
        {
          case CELL:
            *position = pos + col - cell->col_start;
            if (area)
              {
                area->x = column_x (self, col);
                area->width = column_x (self, col + 1) - area->x;
                area->y = y_offset;
                area->height = cell_height (self, cell);
              }
            return TRUE;

          case FILLER:
            cell = gtk_rb_tree_node_get_previous (cell);
            pos -= cell->parent.n_items;
            col = cell->col_start - 1;
            break;

          case BLOCK:
            {
              int row_height = (cell->height) / (cell->parent.n_items / self->n_columns);
              int row = (y - y_offset < cell->section_height) ? 0 : (y - y_offset - cell->section_height) / row_height;
              *position = pos + row * self->n_columns + col;
              if (area)
                {
                  area->x = column_x (self, col);
                  area->width = column_x (self, col + 1) - area->x;
                  if (row == 0)
                    {
                      area->y = y_offset;
                      area->height = row_height + cell->section_height;
                    }
                  else
                    {
                      area->y = y_offset + cell->section_height + row * row_height;
                      area->height = row_height;
                    }
                }
              return TRUE;
            }
            break;

          case UNKNOWN:
          default:
            g_assert_not_reached ();
            break;
        }
    }

  return TRUE;
}

static GtkBitset *
gtk_grid_view_get_items_in_rect (GtkListBase        *base,
                                 const GdkRectangle *rect)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  Cell *first_cell, *last_cell;
  guint first_col, last_col;
  guint first_pos, last_pos;
  int first_y, last_y;
  GtkBitset *result;

  result = gtk_bitset_new_empty ();

  first_col = rect->x < 0 ? 0 : column_from_x (self, rect->x);
  last_col = column_from_x (self, rect->x + rect->width);
  last_col = MIN (last_col, self->n_columns - 1);

  first_cell = get_cell_at (self, first_col, rect->y, &first_pos, &first_y);
  if (first_cell == NULL)
    return result;
  last_cell = get_cell_at (self, last_col, rect->y + rect->height, &last_pos, &last_y);
  if (last_cell == NULL)
    return result;

  /* XXX: this is wrong, but can be fixed when the rest works. */
  gtk_bitset_add_range_closed (result, first_pos, last_pos);

  return result;
}

static guint
gtk_grid_view_move_focus_along (GtkListBase *base,
                                guint        pos,
                                int          steps)
{
  GtkGridView *self = GTK_GRID_VIEW (base);
  Cell *cell;
  guint col, remaining;

  cell = gtk_list_item_manager_get_nth (self->item_manager, pos, &remaining);
  if (cell == NULL)
    g_return_val_if_reached (pos);

  col = (cell->col_start + remaining) % self->n_columns;
  /* go to first row in case of blocks */
  steps += remaining / self->n_columns;
  /* go to start of cell */
  pos -= remaining;

  /* now advance to correct row by counting rows, then find
   * correct column */

  while (steps < 0)
    {
      if (cell->col_start == 0)
        steps++;
      cell = gtk_rb_tree_node_get_previous (cell);
      if (cell == NULL)
        {
          g_assert (pos == 0);
          cell = gtk_list_item_manager_get_first (self->item_manager);
          steps = 0;
          break;
        }
      pos -= cell->parent.n_items;
      steps -= MAX (1, cell->parent.n_items / self->n_columns) - 1;
    }
  while (steps > 0)
    {
      int rows = cell->parent.n_items / self->n_columns;
      if (rows > steps)
        return pos + steps * self->n_columns + col;
      else if (rows > 0)
        steps -= rows;
      else if (cell->col_end >= self->n_columns)
        steps--;
      pos += cell->parent.n_items;
      cell = gtk_rb_tree_node_get_next (cell);
      if (cell == NULL)
        {
          cell = gtk_list_item_manager_get_last (self->item_manager);
          pos -= cell->parent.n_items;
          steps = MAX (1, cell->parent.n_items / self->n_columns) - 1;
        }
    }
  while (cell->col_start > col)
    {
      cell = gtk_rb_tree_node_get_previous (cell);
      pos -= cell->parent.n_items;
    }
  while (cell->col_end <= col)
    {
      pos += cell->parent.n_items;
      cell = gtk_rb_tree_node_get_next (cell);
    }
  if (cell->parent.n_items == 0)
    pos--;
  else
    pos += MIN (cell->parent.n_items, col - cell->col_start);

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
                                   int         *natural,
                                   int         *minimum_section,
                                   int         *natural_section)
{
  GtkOrientation opposite;
  Cell *cell;
  int min, nat, min_sec, nat_sec, child_min, child_nat;

  min = 0;
  nat = 0;
  min_sec = 0;
  nat_sec = 0;
  opposite = gtk_list_base_get_opposite_orientation (GTK_LIST_BASE (self));

  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (cell->parent.section_header)
        {
          gtk_widget_measure (cell->parent.section_header,
                              opposite, -1,
                              &child_min, &child_nat, NULL, NULL);
          min_sec = MAX (min_sec, child_min);
          nat_sec = MAX (nat_sec, child_nat);
        }
      if (cell->parent.widget)
        {
          gtk_widget_measure (cell->parent.widget,
                              opposite, -1,
                              &child_min, &child_nat, NULL, NULL);
          min = MAX (min, child_min);
          nat = MAX (nat, child_nat);
        }
    }

  *minimum = min;
  *natural = nat;
  *minimum_section = min_sec;
  *natural_section = nat_sec;
}

static void
gtk_grid_view_measure_across (GtkWidget *widget,
                              int        for_size,
                              int       *minimum,
                              int       *natural)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);
  int min_section, nat_section;

  gtk_grid_view_measure_column_size (self, minimum, natural, &min_section, &nat_section);

  *minimum *= self->min_columns;
  *natural *= self->max_columns;
  *minimum = MAX (*minimum, min_section);
  *natural = MAX (*natural, nat_section);
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
  int height, row_height, child_min, child_nat, column_size, col_min, col_nat, sec_min, sec_nat;
  gboolean measured;
  GArray *heights;
  guint n_unknown, n_columns;
  guint i;

  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), gtk_list_base_get_orientation (GTK_LIST_BASE (self)));
  heights = g_array_new (FALSE, FALSE, sizeof (int));
  n_unknown = 0;
  height = 0;

  gtk_grid_view_measure_column_size (self, &col_min, &col_nat, &sec_min, &sec_nat);
  for_size = MAX (for_size, col_min * (int) self->min_columns);
  for_size = MAX (for_size, sec_min);
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
cell_set_size (Cell *cell,
               int   section_height,
               int   height,
               int   col_start,
               int   col_end)
{
  if (cell->section_height == section_height &&
      cell->height == height &&
      cell->col_start == col_start &&
      cell->col_end == col_end)
    return;

  g_assert ((cell->parent.section_header == NULL) || (section_height != 0));
  cell->section_height = section_height;
  cell->height = height;
  cell->col_start = col_start;
  cell->col_end = col_end;
  gtk_rb_tree_node_mark_dirty (cell);
}

static void
gtk_grid_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);
  Cell *cell, *tmp;
  GArray *heights;
  int min_row_height, row_height, section_height, total_height, total_width, col_min, col_nat, sec_min, sec_nat;
  GtkOrientation orientation, opposite_orientation;
  GtkScrollablePolicy scroll_policy;
  int x, y;
  guint i;

  dump (self, &GDK_RGBA("D0B0B0"));

  orientation = gtk_list_base_get_orientation (GTK_LIST_BASE (self));
  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), orientation);
  opposite_orientation = OPPOSITE_ORIENTATION (orientation);
  min_row_height = ceil ((double) height / GTK_GRID_VIEW_MAX_VISIBLE_ROWS);

  /* step 1: exit early if list is empty */
  gtk_list_item_manager_gc (self->item_manager);
  if (gtk_list_item_manager_get_root (self->item_manager) == NULL)
    return;

  /* step 2: determine width of the list */
  gtk_grid_view_measure_column_size (self, &col_min, &col_nat, &sec_min, &sec_nat);
  total_width = orientation == GTK_ORIENTATION_VERTICAL ? width : height;
  total_width = MAX (total_width, sec_min);
  self->n_columns = gtk_grid_view_compute_n_columns (self, 
                                                     total_width,
                                                     col_min, col_nat);
  self->column_width = (double) total_width / self->n_columns;
  if (col_min > self->column_width)
    {
      self->column_width = col_min;
      total_width = col_min * self->n_columns;
    }

  /* step 3: Clean up, so we have the right items in the right order with the right types in the list */
  gtk_list_item_manager_gc (self->item_manager);
  i = 0;
  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (cell->parent.section_header && i > 0)
        {
          cell->cell_type = FILLER;
          cell = gtk_list_item_manager_split_item (self->item_manager, cell, 0);
          i = 0;
        }

      if (cell->parent.n_items + i > self->n_columns)
        {
          if (i > 0)
            {
              cell->cell_type = CELL;
              cell = gtk_list_item_manager_split_item (self->item_manager, cell, self->n_columns - i);
              i = 0;
            }
          if (cell->parent.n_items >= self->n_columns)
            {
              int remainder = cell->parent.n_items % self->n_columns;
              cell->cell_type = BLOCK;
              if (remainder)
                {
                  cell = gtk_list_item_manager_split_item (self->item_manager, cell, cell->parent.n_items - remainder);
                  cell->cell_type = CELL;
                }
            }
          else
            {
              cell->cell_type = CELL;
            }
        }
      else
        {
          cell->cell_type = CELL;
        }
      i = (i + cell->parent.n_items) % self->n_columns;
    }
  if (i > 0)
    {
       cell = gtk_list_item_manager_get_last (self->item_manager);
       cell = gtk_list_item_manager_split_item (self->item_manager, cell, cell->parent.n_items);
       cell->cell_type = FILLER;
    }

  /* step 4: determine height of known rows */
  heights = g_array_new (FALSE, FALSE, sizeof (int));
  total_height = 0;

  i = 0;
  row_height = 0;
  cell = gtk_list_item_manager_get_first (self->item_manager);
  while (cell != NULL)
    {
      switch (cell->cell_type)
        {
        case CELL:
          tmp = cell;
          row_height = 0;
          for (i = 0;
               i < self->n_columns;
               cell = gtk_rb_tree_node_get_next (cell))
            {
              i += cell->parent.n_items;
              if (cell->cell_type == FILLER)
                {
                  i = self->n_columns;
                }
              else if (cell->parent.widget)
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
            }
          if (row_height > 0)
            {
              int min, nat;
              total_height += row_height;
              if (tmp->parent.section_header)
                {
                  gtk_widget_measure (tmp->parent.section_header,
                                      gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                                      total_width,
                                      &min, &nat, NULL, NULL);
                  if (scroll_policy == GTK_SCROLL_MINIMUM)
                    section_height = min;
                  else
                    section_height = nat;
                  total_height += section_height;
                }
              else
                section_height = 0;

              for (i = 0;
                   tmp != cell;
                   tmp = gtk_rb_tree_node_get_next (tmp))
                {
                  if (tmp->cell_type == FILLER)
                    cell_set_size (tmp, section_height, row_height, i, self->n_columns);
                  else
                    cell_set_size (tmp, section_height, row_height, i, i + tmp->parent.n_items);
                  i += tmp->parent.n_items;
                }
            }
          break;

        case BLOCK:
          cell = gtk_rb_tree_node_get_next (cell);
          break;

        case FILLER:
        case UNKNOWN:
        default:
          g_assert_not_reached ();
          break;
        }
    }

  /* step 5: determine height of rows with only unknown items and assign their size */
  row_height = gtk_grid_view_get_unknown_row_size (self, heights);
  g_array_free (heights, TRUE);
  i = 0;
  cell = gtk_list_item_manager_get_first (self->item_manager);
  while (cell != NULL)
    {
      switch (cell->cell_type)
        {
        case CELL:
          tmp = cell;
          /* check if we should skip the row */
          for (i = 0;
               i < self->n_columns;
               cell = gtk_rb_tree_node_get_next (cell))
            {
              if (cell->parent.widget != NULL)
                tmp = cell;
              i += cell->parent.n_items;
              if (cell->cell_type == FILLER)
                i = self->n_columns;
            }
          /* no cell with a parent found, need to allocate it */
          if (tmp->parent.widget == NULL)
            {
              if (tmp->parent.section_header)
                {
                  int min, nat;
                  gtk_widget_measure (tmp->parent.section_header,
                                      gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                                      total_width,
                                      &min, &nat, NULL, NULL);
                  if (scroll_policy == GTK_SCROLL_MINIMUM)
                    section_height = min;
                  else
                    section_height = nat;
                }
              else
                section_height = 0;
              total_height += section_height + row_height;
              for (i = 0;
                   tmp != cell;
                   tmp = gtk_rb_tree_node_get_next (tmp))
                {
                  if (tmp->cell_type == FILLER)
                    cell_set_size (tmp, section_height, row_height, i, self->n_columns);
                  else
                    cell_set_size (tmp, section_height, row_height, i, i + tmp->parent.n_items);
                  i += tmp->parent.n_items;
                }
            }
          break;
        
        case BLOCK:
          if (cell->parent.section_header)
            {
              int min, nat;
              gtk_widget_measure (cell->parent.section_header,
                                  gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                                  total_width,
                                  &min, &nat, NULL, NULL);
              if (scroll_policy == GTK_SCROLL_MINIMUM)
                section_height = min;
              else
                section_height = nat;
            }
          else
            section_height = 0;
          i = cell->parent.n_items / self->n_columns;
          cell_set_size (cell, section_height, i * row_height, 0, self->n_columns);
          total_height += section_height + row_height * i;
          cell = gtk_rb_tree_node_get_next (cell);
          break;

        case FILLER:
        case UNKNOWN:
        default:
          g_assert_not_reached ();
          break;
        }
    }

  /* step 7: update the adjustments */
  gtk_list_base_update_adjustments (GTK_LIST_BASE (self),
                                    total_width,
                                    total_height,
                                    gtk_widget_get_size (widget, opposite_orientation),
                                    gtk_widget_get_size (widget, orientation),
                                    &x, &y);

  /* step 8: run the size_allocate loop */
  y = -y;
  for (cell = gtk_list_item_manager_get_first (self->item_manager);
       cell != NULL;
       cell = gtk_rb_tree_node_get_next (cell))
    {
      if (cell->parent.section_header)
        {
          gtk_list_base_size_allocate_child (GTK_LIST_BASE (self),
                                             cell->parent.section_header,
                                             - x,
                                             y,
                                             total_width,
                                             cell->section_height);
        }
      switch (cell->cell_type)
        {
        case CELL:
          if (cell->parent.widget)
            {
              gtk_list_base_size_allocate_child (GTK_LIST_BASE (self),
                                                 cell->parent.widget,
                                                 cell_x (self, cell) - x,
                                                 y + cell->section_height,
                                                 cell_width (self, cell),
                                                 cell->height);
            }
          if (cell->col_end == self->n_columns)
            y += cell_height (self, cell);
          break;

        case BLOCK:
        case FILLER:
          y += cell_height (self, cell);
          break;

        case UNKNOWN:
        default:
          g_assert_not_reached ();
          break;
        }
    }

  gtk_list_base_allocate_rubberband (GTK_LIST_BASE (widget));

  dump (self, &GDK_RGBA("B0D0B0"));
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
    case PROP_ENABLE_RUBBERBAND:
      g_value_set_boolean (value, gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)));
      break;

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

    case PROP_SECTION_FACTORY:
      g_value_set_object (value, gtk_list_item_manager_get_section_factory (self->item_manager));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      g_value_set_boolean (value, gtk_list_item_manager_get_single_click_activate (self->item_manager));
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
    case PROP_ENABLE_RUBBERBAND:
      gtk_grid_view_set_enable_rubberband (self, g_value_get_boolean (value));
      break;

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

    case PROP_SECTION_FACTORY:
      gtk_grid_view_set_section_factory (self, g_value_get_object (value));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_grid_view_set_single_click_activate (self, g_value_get_boolean (value));
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
   * GtkGridView:enable-rubberband: (attributes org.gtk.Property.get=gtk_grid_view_get_enable_rubberband org.gtk.Property.set=gtk_grid_view_set_enable_rubberband)
   *
   * Allow rubberband selection.
   */
  properties[PROP_ENABLE_RUBBERBAND] =
    g_param_spec_boolean ("enable-rubberband",
                          P_("Enable rubberband selection"),
                          P_("Allow selecting items by dragging with the mouse"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridView:factory: (attributes org.gtk.Property.get=gtk_grid_view_get_factory org.gtk.Property.set=gtk_grid_view_set_factory)
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
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
    g_param_spec_uint ("max-columns",
                       P_("Max columns"),
                       P_("Maximum number of columns per row"),
                       1, G_MAXUINT, DEFAULT_MAX_COLUMNS,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:min-columns: (attributes org.gtk.Property.get=gtk_grid_view_get_min_columns org.gtk.Property.set=gtk_grid_view_set_min_columns)
   *
   * Minimum number of columns per row.
   */
  properties[PROP_MIN_COLUMNS] =
    g_param_spec_uint ("min-columns",
                       P_("Min columns"),
                       P_("Minimum number of columns per row"),
                       1, G_MAXUINT, 1,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:model: (attributes org.gtk.Property.get=gtk_grid_view_get_model org.gtk.Property.set=gtk_grid_view_set_model)
   *
   * Model for the items displayed.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         GTK_TYPE_SELECTION_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:section-factory: (attributes org.gtk.Property.get=gtk_grid_view_get_section_factory org.gtk.Property.set=gtk_grid_view_set_section_factory)
   *
   * Factory for populating section headers.
   *
   * Since: 4.8
   */
  properties[PROP_SECTION_FACTORY] =
    g_param_spec_object ("section-factory",
                         P_("Section factory"),
                         P_("Factory for populating secion headers"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:single-click-activate: (attributes org.gtk.Property.get=gtk_grid_view_get_single_click_activate org.gtk.Property.set=gtk_grid_view_set_single_click_activate)
   *
   * Activate rows on single click and select them on hover.
   */
  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate",
                          P_("Single click activate"),
                          P_("Activate rows on single click"),
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
 * gtk_grid_view_get_section_factory: (attributes org.gtk.Method.get_property=section-factory)
 * @self: a `GtkListView`
 *
 * Gets the factory that's currently used to populate section headers.
 *
 * Returns: (nullable) (transfer none): The factory in use
 *
 * Since: 4.8
 */
GtkListItemFactory *
gtk_grid_view_get_section_factory (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), NULL);

  return gtk_list_item_manager_get_section_factory (self->item_manager);
}

/**
 * gtk_grid_view_set_section_factory: (attributes org.gtk.Method.set_property=section-factory)
 * @self: a `GtkListView`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating section headers.
 *
 * If this factory is set to %NULL, the grid will not use section.
 *
 * Since: 4.8
 */
void
gtk_grid_view_set_section_factory (GtkGridView        *self,
                                   GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  if (factory == gtk_list_item_manager_get_section_factory (self->item_manager))
    return;

  gtk_list_item_manager_set_section_factory (self->item_manager, factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECTION_FACTORY]);
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
