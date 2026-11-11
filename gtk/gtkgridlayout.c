/* gtkgridlayout.c: Layout manager for grid-like widgets
 * Copyright 2019-2026  GNOME Foundation, Sergey Bugaev
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 */

/**
 * GtkGridLayout:
 *
 * Arranges child widgets in rows and columns.
 *
 * Children have an "attach point" defined by the horizontal and vertical
 * index of the cell they occupy; children can span multiple rows or columns.
 * The layout properties for setting the attach points and spans are set
 * using the [class@Gtk.GridLayoutChild] associated to each child widget.
 *
 * The behaviour of `GtkGridLayout` when several children occupy the same
 * grid cell is undefined.
 *
 * `GtkGridLayout` can be used like a `GtkBoxLayout` if all children are
 * attached to the same row or column; however, if you only ever need a
 * single row or column, you should consider using `GtkBoxLayout`.
 */

/**
 * GtkGridLayoutChild:
 *
 * `GtkLayoutChild` subclass for children in a `GtkGridLayout`.
 */
#include "config.h"

#include "gtkgridlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkdebug.h"
#include "gtklayoutchild.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"

/* {{{ GtkGridLayoutChild */
typedef struct {
  int pos;
  int span;
} GridChildAttach;

struct _GtkGridLayoutChild
{
  GtkLayoutChild parent_instance;

  GridChildAttach attach[2];
};

#define CHILD_COLUMN(child)     ((child)->attach[GTK_ORIENTATION_HORIZONTAL].pos)
#define CHILD_COL_SPAN(child)   ((child)->attach[GTK_ORIENTATION_HORIZONTAL].span)
#define CHILD_ROW(child)        ((child)->attach[GTK_ORIENTATION_VERTICAL].pos)
#define CHILD_ROW_SPAN(child)   ((child)->attach[GTK_ORIENTATION_VERTICAL].span)

enum {
  PROP_CHILD_COLUMN = 1,
  PROP_CHILD_ROW,
  PROP_CHILD_COLUMN_SPAN,
  PROP_CHILD_ROW_SPAN,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

G_DEFINE_TYPE (GtkGridLayoutChild, gtk_grid_layout_child, GTK_TYPE_LAYOUT_CHILD)

static void
gtk_grid_layout_child_set_property (GObject      *gobject,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkGridLayoutChild *self = GTK_GRID_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_COLUMN:
      gtk_grid_layout_child_set_column (self, g_value_get_int (value));
      break;

    case PROP_CHILD_ROW:
      gtk_grid_layout_child_set_row (self, g_value_get_int (value));
      break;

    case PROP_CHILD_COLUMN_SPAN:
      gtk_grid_layout_child_set_column_span (self, g_value_get_int (value));
      break;

    case PROP_CHILD_ROW_SPAN:
      gtk_grid_layout_child_set_row_span (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_child_get_property (GObject    *gobject,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkGridLayoutChild *self = GTK_GRID_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_COLUMN:
      g_value_set_int (value, CHILD_COLUMN (self));
      break;

    case PROP_CHILD_ROW:
      g_value_set_int (value, CHILD_ROW (self));
      break;

    case PROP_CHILD_COLUMN_SPAN:
      g_value_set_int (value, CHILD_COL_SPAN (self));
      break;

    case PROP_CHILD_ROW_SPAN:
      g_value_set_int (value, CHILD_ROW_SPAN (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_child_class_init (GtkGridLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_grid_layout_child_set_property;
  gobject_class->get_property = gtk_grid_layout_child_get_property;

  /**
   * GtkGridLayoutChild:column:
   *
   * The column to place the child in.
   */
  child_props[PROP_CHILD_COLUMN] =
    g_param_spec_int ("column", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayoutChild:row:
   *
   * The row to place the child in.
   */
  child_props[PROP_CHILD_ROW] =
    g_param_spec_int ("row", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayoutChild:column-span:
   *
   * The number of columns the child spans to.
   */
  child_props[PROP_CHILD_COLUMN_SPAN] =
    g_param_spec_int ("column-span", NULL, NULL,
                      1, G_MAXINT, 1,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayoutChild:row-span:
   *
   * The number of rows the child spans to.
   */
  child_props[PROP_CHILD_ROW_SPAN] =
    g_param_spec_int ("row-span", NULL, NULL,
                      1, G_MAXINT, 1,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_CHILD_PROPERTIES, child_props);
}

static void
gtk_grid_layout_child_init (GtkGridLayoutChild *self)
{
  CHILD_ROW_SPAN (self) = 1;
  CHILD_COL_SPAN (self) = 1;
}

/**
 * gtk_grid_layout_child_set_row:
 * @child: a `GtkGridLayoutChild`
 * @row: the row for @child
 *
 * Sets the row to place @child in.
 */
void
gtk_grid_layout_child_set_row (GtkGridLayoutChild *child,
                               int                 row)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_ROW (child) == row)
    return;

  CHILD_ROW (child) = row;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_ROW]);
}

/**
 * gtk_grid_layout_child_get_row:
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the row number to which @child attaches its top side.
 *
 * Returns: the row number
 */
int
gtk_grid_layout_child_get_row (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 0);

  return CHILD_ROW (child);
}

/**
 * gtk_grid_layout_child_set_column:
 * @child: a `GtkGridLayoutChild`
 * @column: the attach point for @child
 *
 * Sets the column number to attach the left side of @child.
 */
void
gtk_grid_layout_child_set_column (GtkGridLayoutChild *child,
                                  int                 column)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_COLUMN (child) == column)
    return;

  CHILD_COLUMN (child) = column;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_COLUMN]);
}

/**
 * gtk_grid_layout_child_get_column:
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the column number to which @child attaches its left side.
 *
 * Returns: the column number
 */
int
gtk_grid_layout_child_get_column (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 0);

  return CHILD_COLUMN (child);
}

/**
 * gtk_grid_layout_child_set_column_span:
 * @child: a `GtkGridLayoutChild`
 * @span: the span of @child
 *
 * Sets the number of columns @child spans to.
 */
void
gtk_grid_layout_child_set_column_span (GtkGridLayoutChild *child,
                                       int                 span)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_COL_SPAN (child) == span)
    return;

  CHILD_COL_SPAN (child) = span;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_COLUMN_SPAN]);
}

/**
 * gtk_grid_layout_child_get_column_span:
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the number of columns that @child spans to.
 *
 * Returns: the number of columns
 */
int
gtk_grid_layout_child_get_column_span (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 1);

  return CHILD_COL_SPAN (child);
}

/**
 * gtk_grid_layout_child_set_row_span:
 * @child: a `GtkGridLayoutChild`
 * @span: the span of @child
 *
 * Sets the number of rows @child spans to.
 */
void
gtk_grid_layout_child_set_row_span (GtkGridLayoutChild *child,
                                    int                 span)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child));

  if (CHILD_ROW_SPAN (child) == span)
    return;

  CHILD_ROW_SPAN (child) = span;

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child)));

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_ROW_SPAN]);
}

/**
 * gtk_grid_layout_child_get_row_span:
 * @child: a `GtkGridLayoutChild`
 *
 * Retrieves the number of rows that @child spans to.
 *
 * Returns: the number of row
 */
int
gtk_grid_layout_child_get_row_span (GtkGridLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (child), 1);

  return CHILD_ROW_SPAN (child);
}

/* }}} */

/* {{{ GtkGridLayout */

typedef struct {
  int row;
  GtkBaselinePosition baseline_position;
} GridRowProperties;

static const GridRowProperties grid_row_properties_default = {
  0,
  GTK_BASELINE_POSITION_CENTER
};

/* A GridLineData struct contains row/column specific parts
 * of the grid.
 */
typedef struct {
  gint16 spacing;
  guint homogeneous : 1;
} GridLineData;

#define ROWS(layout)    (&(layout)->linedata[GTK_ORIENTATION_HORIZONTAL])
#define COLUMNS(layout) (&(layout)->linedata[GTK_ORIENTATION_VERTICAL])

typedef struct {
  gsize *edge_order;
  int min;
  int num;
} GridLines;

typedef struct {
  GtkGridLayout *layout;
  GtkWidget *widget;

  GridLines lines[2];
  GtkWidget **children;
  gsize n_children;
} GridContext;

struct _GtkGridLayout
{
  GtkLayoutManager parent_instance;

  /* Array<GridRowProperties> */
  GArray *row_properties;

  int baseline_row;

  GridLineData linedata[2];
};

enum {
  PROP_ROW_SPACING = 1,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,
  PROP_BASELINE_ROW,

  N_PROPERTIES
};

static GParamSpec *layout_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkGridLayout, gtk_grid_layout, GTK_TYPE_LAYOUT_MANAGER)

static inline GtkGridLayoutChild *
get_grid_child (GtkGridLayout *self,
                GtkWidget     *child)
{
  GtkLayoutManager *manager = GTK_LAYOUT_MANAGER (self);

  return (GtkGridLayoutChild *) gtk_layout_manager_get_layout_child (manager, child);
}

static int
get_spacing (GtkGridLayout  *self,
             GtkWidget      *widget,
             GtkOrientation  orientation)
{
  GtkCssNode *node = gtk_widget_get_css_node (widget);
  GtkCssStyle *style = gtk_css_node_get_style (node);
  GtkCssValue *border_spacing;
  int css_spacing;

  border_spacing = style->size->border_spacing;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing + self->linedata[orientation].spacing;
}

static void
grid_context_clear (GridContext *context)
{
  g_free_sized (context->lines[0].edge_order,
                (context->n_children + context->lines[0].num)
                * sizeof (gsize));
  g_free_sized (context->lines[1].edge_order,
                (context->n_children + context->lines[1].num)
                * sizeof (gsize));
  g_free_sized (context->children,
                context->n_children * sizeof (GtkWidget *));
}

/**
 * get_edge:
 * @index: an edge index, as stored in edge_order
 * @start: (out): start line of the edge, inclusive
 * @end: (out): end line of the edge, exclusive
 *
 * Both @start and @end are returned relative to the minimum user value for
 * lines in this orientation. In other words, the smallest @start can be is 0,
 * and the largest @end can be is context->lines[orientation].min.
 *
 * Returns: (nullable): the corresponding widget for child edges.
 */
static inline GtkWidget *
get_edge (GridContext    *context,
          GtkOrientation  orientation,
          gsize           index,
          gsize          *start,
          gsize          *end)
{
  GtkWidget *child;
  GtkGridLayoutChild *grid_child;

  if (index >= context->n_children)
    {
      /* Line edge */
      *start = index - context->n_children;
      *end = *start + 1;
      return NULL;
    }
  else
    {
      /* Child edge */
      child = context->children[index];
      grid_child = get_grid_child (context->layout, child);
      *start = grid_child->attach[orientation].pos -
        context->lines[orientation].min;
      *end = *start + grid_child->attach[orientation].span;
      return child;
    }
}

static int G_GNUC_PURE
compare_ends (const void    *a,
              const void    *b,
              void          *user_data,
              GtkOrientation orientation)
{
  gsize index1 = *(const gsize *) a;
  gsize index2 = *(const gsize *) b;
  GridContext *context = user_data;
  gsize start1, start2, end1, end2;

  get_edge (context, orientation, index1, &start1, &end1);
  get_edge (context, orientation, index2, &start2, &end2);

  return end1 - end2;
}

static int G_GNUC_PURE
compare_horizontal_ends (const void *a,
                         const void *b,
                         void       *user_data)
{
  return compare_ends (a, b, user_data, GTK_ORIENTATION_HORIZONTAL);
}

static int G_GNUC_PURE
compare_vertical_ends (const void *a,
                       const void *b,
                       void       *user_data)
{
  return compare_ends (a, b, user_data, GTK_ORIENTATION_VERTICAL);
}

static void
grid_context_init (GridContext *context)
{
  GtkWidget *child;
  int min[2];
  int max[2];
  gsize num_edges[2];
  int i;

  min[0] = min[1] = G_MAXINT;
  max[0] = max[1] = G_MININT;

  /* Count visible children and the used line ranges */
  context->n_children = 0;

  for (child = _gtk_widget_get_first_child (context->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;
      context->n_children++;
      GtkGridLayoutChild *grid_child = get_grid_child (context->layout, child);
      GridChildAttach *attach = grid_child->attach;

      min[0] = MIN (min[0], attach[0].pos);
      max[0] = MAX (max[0], attach[0].pos + attach[0].span);
      min[1] = MIN (min[1], attach[1].pos);
      max[1] = MAX (max[1], attach[1].pos + attach[1].span);
    }

  if (context->n_children == 0)
    {
      context->children = NULL;
      context->lines[0].min = context->lines[1].min = 0;
      context->lines[0].num = context->lines[1].num = 0;
      context->lines[0].edge_order = context->lines[1].edge_order = NULL;
      return;
    }

  context->lines[0].min = min[0];
  context->lines[0].num = max[0] - min[0];
  context->lines[1].min = min[1];
  context->lines[1].num = max[1] - min[1];

  num_edges[0] = context->n_children + context->lines[0].num;
  num_edges[1] = context->n_children + context->lines[1].num;

  context->children = g_new (GtkWidget *, context->n_children);
  context->lines[0].edge_order = g_new (gsize, num_edges[0]);
  context->lines[1].edge_order = g_new (gsize, num_edges[1]);

  i = 0;
  for (child = _gtk_widget_get_first_child (context->widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;
      context->children[i] = child;
      context->lines[0].edge_order[i] = context->lines[1].edge_order[i] = i;
      i++;
    }
  g_assert (i == context->n_children);
  for (i = context->n_children; i < num_edges[0]; i++)
    context->lines[0].edge_order[i] = i;
  for (i = context->n_children; i < num_edges[1]; i++)
    context->lines[1].edge_order[i] = i;

  g_sort_array (context->lines[0].edge_order, num_edges[0],
                sizeof (gsize), compare_horizontal_ends, context);
  g_sort_array (context->lines[1].edge_order, num_edges[1],
                sizeof (gsize), compare_vertical_ends, context);
}

static GtkOrientation G_GNUC_PURE
grid_primary_orientation (GtkLayoutManager *manager)
{
  /* We could optimize for width-for-height grids here.  However, we expect
   * those to be rare; at the same time we want to maintain the invariant that
   * switching the request mode (which we can do depending on our children
   * switching theirs) never changes the produced layout and only affects
   * optimizations.
   */
#if 0
  if (gtk_layout_manager_get_request_mode (manager) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
    return GTK_ORIENTATION_VERTICAL;
#endif
  return GTK_ORIENTATION_HORIZONTAL;
}

static inline void
update_max (int *max, int value)
{
  if (value > *max)
    *max = value;
}

/* TODO: baselines */
static void
grid_context_measure (GridContext    *context,
                      GtkOrientation  orientation,
                      const int      *for_size_min,
                      const int      *for_size_nat,
                      int            *minimum,
                      int            *natural)
{
  int n_lines, i, j, line;
  int *min_sizes, *nat_sizes;
  int spacing = get_spacing (context->layout, context->widget, orientation);
  int homogeneous = context->layout->linedata[orientation].homogeneous;

  n_lines = context->lines[orientation].num;
  if (n_lines == 0)
    {
      g_assert (context->n_children == 0);
      *minimum = *natural = 0;
      return;
    }
  if (!homogeneous)
    {
      min_sizes = g_newa (int, n_lines + 1);
      nat_sizes = g_newa (int, n_lines + 1);

      min_sizes[0] = nat_sizes[0] = 0;
      line = 0;
    }
  else
    {
      *minimum = 0;
      *natural = 0;
    }

  for (i = j = 0; i < context->n_children; j++)
    {
      gsize edge_index;
      GtkWidget *child;
      int child_for_size_min, child_for_size_nat, child_min, child_nat;
      gsize start, end;

      /* Traverse child edges in the provided order, and skip over line edges */
      edge_index = context->lines[orientation].edge_order[j];
      if (edge_index < context->n_children)
        i++;
      else
        continue;
      child = get_edge (context, orientation, edge_index, &start, &end);

      child_for_size_min = for_size_min ? for_size_min[edge_index] : -1;
      child_for_size_nat = for_size_nat ? for_size_nat[edge_index] : -1;
      gtk_widget_measure (child, orientation, child_for_size_min,
                          &child_min, &child_nat, NULL, NULL);
      if (child_for_size_nat != child_for_size_min)
        gtk_widget_measure (child, orientation, child_for_size_nat,
                            NULL, &child_nat, NULL, NULL);

      if (!homogeneous)
        {
          int spacing_before;

          while (end > line)
            {
              min_sizes[line + 1] = min_sizes[line];
              nat_sizes[line + 1] = nat_sizes[line];
              if (line > 0)
                {
                  min_sizes[line + 1] += spacing;
                  nat_sizes[line + 1] += spacing;
                }
              line++;
            }
          spacing_before = start > 0 ? spacing : 0;
          update_max (&min_sizes[end], min_sizes[start] + spacing_before + child_min);
          update_max (&nat_sizes[end], nat_sizes[start] + spacing_before + child_nat);
        }
      else
        {
          int d = end - start;
          int full_spacing = (d-1) * spacing;

          if (child_min > full_spacing)
            update_max (minimum, (child_min - full_spacing + d-1) / d);
          if (child_nat > full_spacing)
            update_max (natural, (child_nat - full_spacing + d-1) / d);
        }
    }

  if (!homogeneous)
    {
      g_assert (line == n_lines);

      *minimum = min_sizes[n_lines];
      *natural = MAX (nat_sizes[n_lines], *minimum);
    }
  else
    {
      *minimum = *minimum * n_lines + (n_lines - 1) * spacing;
      *natural = *natural * n_lines + (n_lines - 1) * spacing;
      /* This can happen when for_size_nat[i] > for_size_min[i] */
      *natural = MAX (*minimum, *natural);
    }
}

/**
 * forward_dp:
 * @spacing: spacing between lines
 * @n_edges: number of edges, either n_children or n_children + n_lines
 * @reqs: (in) (array length=n_edges): base per-edge size requirements
 * @active: (in) (array length=n_edges): which edges are active
 * @whole: common increment for active edges' requirements on top of @reqs
 * @extra: additional increment for active edges' requirements
 * @dp: (out caller-allocates) (array): dynamic programming values
 *
 * Performs a single forward run of dynamic programming, aggregating the
 * requirements in @reqs (to which @whole and @extra are added for active
 * children). Sets 'dp[i]' equal to the minimum space required to fit the edges
 * that end before the i-th line; in particular 'dp[0]' should be zero, and
 * 'dp[n_lines]' equal to the overall required space for satisfying these
 * per-edge requirements.
 *
 * The semantics of @whole and @extra match how the two values are produced in
 * 'grid_grow_requirements'. This function is able to evaluate those potential
 * requirement increments just like if they were already committed to @reqs.
 * Pass 'whole = 0, extra = 0' to ignore this mechanism entirely.
 *
 * This is somewhat similar to 'grid_context_measure' above.
 */
static void
forward_dp (GridContext    *context,
            GtkOrientation  orientation,
            int             spacing,
            gsize           n_edges,
            const int      *reqs,
            const guint8   *active,
            guint           whole,
            guint           extra,
            int            *dp)
{
  gsize i, j, rank, edge_index, line, start, end;
  int spacing_before, req;

  line = 0;
  dp[line] = 0;

  for (i = j = 0, rank = 0; i < n_edges; j++)
    {
      edge_index = context->lines[orientation].edge_order[j];
      if (edge_index < n_edges)
        i++;
      else
        continue;
      get_edge (context, orientation, edge_index, &start, &end);

      req = reqs[edge_index];
      if (active[edge_index])
        req += whole + (rank++ < extra);
      while (end > line)
        {
          dp[line + 1] = dp[line];
          if (line > 0)
            dp[line + 1] += spacing;
          line++;
        }
      spacing_before = start > 0 ? spacing : 0;
      update_max (&dp[end], dp[start] + spacing_before + req);
    }
  g_assert (line == context->lines[orientation].num);
}

/**
 * backward_dp:
 * @spacing: spacing between lines
 * @n_edges: number of edges, either n_children or n_children + n_lines
 * @reqs: (in) (array length=n_edges): base per-edge size requirements
 * @dpb: (out caller-allocates) (array): dynamic programming values
 *
 * Performs a single backward run of dynamic programming, aggregating the
 * requirements in @reqs. Sets 'dpb[i]' equal to the minimum space required to
 * fit the edges that start after the i-th line; in particular 'dpb[n_lines]'
 * should be zero, and 'dpb[0]' equal to the overall required space for
 * satisfying these per-edge requirements.
 */
static void
backward_dp (GridContext    *context,
             GtkOrientation  orientation,
             int             spacing,
             gsize           n_edges,
             const int      *reqs,
             int            *dpb)
{
  gsize i, j, tmp, edge_index, line, start, end;
  int spacing_after, req;
  int n_lines = context->lines[orientation].num;

  /* dpb[n_lines] = 0; */
  memset (dpb, 0, sizeof (int) * (n_lines + 1));
  line = n_lines;

  for (i = j = 0; i < n_edges; j++)
    {
      tmp = context->n_children + n_lines - j - 1;
      edge_index = context->lines[orientation].edge_order[tmp];
      if (edge_index < n_edges)
        i++;
      else
        continue;
      get_edge (context, orientation, edge_index, &start, &end);

      req = reqs[edge_index];
      while (end < line)
        {
          int sp = 0;
          if (line < n_lines)
            sp = spacing;
          update_max (&dpb[line - 1], dpb[line] + sp);
          line--;
        }
      spacing_after = end < n_lines ? spacing : 0;
      update_max (&dpb[start], req + spacing_after + dpb[end]);
    }

  while (line > 0)
    {
      int sp = 0;
      if (line < n_lines) /* can this even not hold? */
        sp = spacing;
      update_max (&dpb[line - 1], dpb[line] + sp);
      line--;
    }
}

/**
 * grid_grow_requirements:
 * @n_edges: number of edges, either n_children or n_children + n_lines
 * @reqs: (inout) (array length=n_edges): per-edge size requirements
 * @targets: (in) (array length=n_edges): per-edge target sizes
 * @space: overall available space
 * @spacing: spacing between lines
 * @stop_at_bottleneck: if set, don't grow after the first bottleneck
 *
 * Grow the given per-edge size *requirements* in-place towards their @targets,
 * while their combination is feasible (fits into the provided @space). This is
 * the heart of the grid layout algorithm.
 *
 * Edges can represent child widgets or synthetic line egdes. A combination of
 * size requirements can be collectively feasible -- when there exists a size
 * allocation that satisfies all the requirements and fits into @space -- or
 * collectively infeasible. We evaluate whether the proposed requirements are
 * collectively feasible by doing a DP run on them, which outputs the minimum
 * size required to satisfy them at the same time, and seeing whether that
 * exceeds @space.
 *
 * We grow all "active" requirements at the same rate. An edge becomes inactive
 * either when its requirement reaches its tagret size, or when we determine
 * that the edge lies on a *critical chain* and its requirement cannot grow
 * anymore without making the combination of requirements collectively
 * infeasible (we call this a bottleneck). In other words, an edge becomes
 * inactive when its requirement either doesn't want to, or cannot, keep
 * growing. We stop once all the edges become inactive.
 *
 * After we hit the first bottleneck and deactivate edges that lie on the
 * critical chain(s), we can continue growing requirements of other edges that
 * remain active -- so long as the requirements remain collectively feasible. We
 * call this secondary growth, as opposed to the primary growth (which is what
 * happens before the first bottleneck).
 *
 * Note that this works on size requirements and not on any allocated sizes.
 * Allocated sizes may be larger than the requirements (but can not be smaller),
 * and there may be multiple valid allocations that satisfy a combination of
 * requirements. Moreover, several requirements taken collectively may imply a
 * stronger requirement for an edge compared to the edge's own requirement (see
 * 'grid_compute_closure').
 *
 * Returns: the used space, which could be less than @space if all edges reached
 * their targets without running into a bottleneck.
 */
static int
grid_grow_requirements (GridContext    *context,
                        GtkOrientation  orientation,
                        gsize           n_edges,
                        int            *reqs,
                        const int      *targets,
                        int             space,
                        int             spacing,
                        gboolean        stop_at_bottleneck)
{
  guint8 *active;
  gsize i, j, rank, n_active, n_deactivated, edge_index;
  int n_lines = context->lines[orientation].num;
  int *dp, *dpb;

  /* Used by the evaluation DP below */
  dp = g_newa (int, n_lines + 1);
  dpb = g_newa (int, n_lines + 1);

  /* Initially, all edges are active */
  active = g_newa (guint8, n_edges);
  n_active = 0;
  for (i = 0; i < n_edges; i++)
    {
      active[i] = reqs[i] < targets[i];
      if (active[i])
        n_active++;
    }

  while (n_active > 0)
    {
      guint min_diff;
      guint low, high, mid, max_slope;  /* binary search */
      guint whole, extra;  /* req increments */

      /* Smallest distance from an active child's requirement to its target */
      min_diff = G_MAXUINT;
      for (i = 0; i < n_edges; i++)
        {
          if (!active[i])
            continue;
          min_diff = MIN (min_diff, targets[i] - reqs[i]);
        }

      /* Binary search for the largest common value we could simultaneously
       * increment the requirements of all active edges by.  Alternative
       * implementations of the same idea here could use a simple linear search
       * (slower but simpler), or (potentially faster, but more complex) a
       * 2-dimensional DP, where dp[i][k] is the smallest size required for the
       * first 'i' lines, among the chains with exactly 'k' active edges.
       */

      /* low is always feasible, high is always infeasible */
      low = 0;
      if (G_UNLIKELY (!g_uint_checked_add (&high, MIN (space, min_diff), 1)))
        high = G_MAXUINT;

      /* Incrementing the value we binary-search for by 1 can increase the
       * result by at most 'max_slope' pixels.  Conversely, decrementing it by 1
       * can decrease the result by at most 'max_slope' pixels.  We use this to
       * further cull the binary search interval.
       */
      max_slope = MIN (n_active, n_lines);
      g_assert (max_slope > 0);

      while (low + 1 < high)
        {
          mid = low + (high - low) / 2;
          forward_dp (context, orientation, spacing, n_edges,
                      reqs, active, mid, 0, dp);
          if (dp[n_lines] <= space)
            {
              /* feasible */
              int slack = space - dp[n_lines];
              low = MIN (mid + slack / max_slope, high - 1);
            }
          else
            {
              /* infeasible */
              int excess = dp[n_lines] - space;
              high = mid - (excess - 1) / max_slope;
            }
          g_assert (low < high);
        }

      whole = low;
      /* We've established that we can simultaneously increase all active edges'
       * requirements by 'whole' (which may be 0).  Now run a "partial round":
       * even though we can no longer give *every* active child one more pixel,
       * we should still distribute the remaining few pixels between active
       * children, giving *some* of them one more pixel.
       *
       * For example, given a chain of 10 identical edges and 32 pixels of
       * space, we might set 'whole=3' and 'extra=2'; in other words, give 3
       * pixels to everyone equally, and one more pixel to some lucky two edges.
       *
       * An important consideration here is doing this in a way that preserves
       * monotonicity: when run with a larger value of 'space', we shouldn't
       * produce smaller results for any individual edge (at least not until we
       * hit the first bottleneck).  This applies both to distributing more
       * pixels in a partial round, as well as to "rolling over" from a partial
       * round into a previous complete round.  For the above example, both when
       * expanding the available space 32 -> 39 and 39 -> 40, these rounds
       * should behave monotonically.
       *
       * We achieve this monotonicity by considering partial increments in a
       * fixed order that doesn't depend on 'space', and always giving
       * increments to edges in this order without skipping any, even when
       * giving an increment to a later child would still be feasible.
       */
      low = 0;
      if (whole != min_diff) /* XXX comment why */
        {
          high = n_active;
          while (low + 1 < high)
            {
              mid = low + (high - low) / 2;
              forward_dp (context, orientation, spacing, n_edges,
                          reqs, active, whole, mid, dp);
              if (dp[n_lines] <= space)
                {
                  /* feasible */
                  int slack = space - dp[n_lines];
                  low = MIN (mid + slack, high - 1);
                }
              else
                {
                  /* infeasible */
                  int excess = dp[n_lines] - space;
                  high = mid - (excess - 1);
                }
              g_assert (low < high);
            }
        }

      /* Commit the increments, and see if any edge has reached its target */
      n_deactivated = 0;
      extra = low;
      for (i = j = 0, rank = 0; i < n_edges; j++)
        {
          edge_index = context->lines[orientation].edge_order[j];
          if (edge_index < n_edges)
            i++;
          else
            continue;
          /* Skip inactive edges */
          if (!active[edge_index])
            continue;
          reqs[edge_index] += whole + (rank++ < extra);
          /* This should never cause us to overallocate targets */
          g_assert (reqs[edge_index] <= targets[edge_index]);
          if (reqs[edge_index] == targets[edge_index])
            {
              active[edge_index] = FALSE;
              n_deactivated++;
            }
        }
      g_assert (rank == n_active);
      n_active -= n_deactivated;

      /*
       * It would be invalid to:
       *
       * if (n_deactivated)
       *   continue;
       *
       * here -- it could be that some edges have reached their targets *and*
       * some chains became tight at the same time.  For stop-at-bottleneck, we
       * would still need to stop then.
       */

      if (n_active == 0)
        /* Every edge is now satisfied, nothing can be blocked */
        break;

      /* See which edges are blocked, and deactivate them */
      forward_dp (context, orientation, spacing,
                  n_edges, reqs, active, 0, 0, dp);
      g_assert (dp[n_lines] <= space);
      if (dp[n_lines] < space)
        {
          /* If there's slack, no edge can be blocked.  This can only happen if
           * we skipped running the partial round above.
           */
          g_assert (whole == min_diff);
          continue;
        }

      backward_dp (context, orientation, spacing,
                   n_edges, reqs, dpb);
      g_assert (dpb[0] == space);

      n_deactivated = 0;
      for (edge_index = 0; edge_index < n_edges; edge_index++)
        {
          int spacing_before, spacing_after, full_req;
          gsize start, end;

          if (!active[edge_index])
            continue;
          get_edge (context, orientation, edge_index, &start, &end);
          /* See if this edge is blocked, i.e. lies on a critical chain */
          spacing_before = start > 0 ? spacing : 0;
          spacing_after = end < n_lines ? spacing : 0;
          full_req = dp[start] + spacing_before
            + reqs[edge_index] + spacing_after + dpb[end];
          g_assert (full_req <= space);
          if (full_req == space)
            {
              active[edge_index] = FALSE;
              n_deactivated++;
            }
        }
      if (n_deactivated == 0)
        {
          /* Same as above: nothing blocked, can only happen if we skipped the
           * partial round.
           */
          g_assert (whole == min_diff);
          continue;
        }

      n_active -= n_deactivated;

      if ((n_deactivated && stop_at_bottleneck) || !n_active)
        return space;
    }

  /* Done growing -- evaluate how much space we're using, and return */
  forward_dp (context, orientation, spacing,
              n_edges, reqs, active, 0, 0, dp);
  return dp[n_lines];
}

/**
 * grid_compute_closure:
 * @spacing: spacing between lines
 * @reqs: (array) (in): per-child size requirements
 * @closure: (array) (out caller-allocates): per-child guaranteed sizes
 *
 * Compute a closure of the given size requirements. This finds any potentially
 * stronger requirements / guarantees that the original requirements
 * collectively imply. In other words, *every* way to distribute space must
 * necessarily satisfy the stronger requirements if it satisfies the original
 * ones.
 *
 * For example, given
 *
 *          20
 *   |<------------>|
 *   |<---->|<----->|
 *      15      10
 *
 * the guaranteed size for the upper child is 25, even though its own direct
 * requirement is 20.
 */
static void
grid_compute_closure (GridContext    *context,
                      GtkOrientation  orientation,
                      int             spacing,
                      const int      *reqs,
                      int            *closure)
{
  int i, j, spacing_before, *dp;
  gsize start, end, start2, end2, column;

  dp = g_newa (int, context->lines[orientation].num + 1);
  for (i = 0; i < context->n_children; i++)
    {
      get_edge (context, orientation, i, &start, &end);
      /* Run a separate DP round for every child.  We start with 0 at the
       * child's start line, rather than the first line as usual.
       */
      column = start;
      dp[start] = 0;

      for (j = 0; j < context->n_children; j++)
        {
          get_edge (context, orientation, j, &start2, &end2);
          /* Skip edges that are not fully contained in the child's span */
          if (start2 < start || end2 > end)
            continue;
          while (end2 > column)
            {
              dp[column + 1] = dp[column];
              if (column > start)
                dp[column + 1] += spacing;
              column++;
            }
          spacing_before = start2 > start ? spacing : 0;
          update_max (&dp[end2], dp[start2] + spacing_before + reqs[j]);
        }

      /* We must have seen at least this edge itself */
      g_assert (column == end);
      closure[i] = dp[end];
      g_assert (closure[i] >= reqs[i]);
    }
}

/**
 * grid_context_distribute:
 * @orientation: the orientation to distribute @space along
 * @space: available space along @orientation
 * @for_size: (nullable) (array) (in): child sizes in the opposite orientation
 * @sizes: (out caller-allocates) (array) (optional): allocated child sizes
 * @monotonic_sizes: (out caller-allocates) (array) (optional): size guarantees
 *     that do not decrease with @space growth
 * @line_sizes: (out caller-allocates) (array) (optional): allocated line sizes
 *
 * Distribute the available @space along @orientation, allocating sizes to lines
 * and to children.
 *
 * If the @for_size array is provided, we obtain the child widgets' contextual
 * size requests, otherwise this will act on the child widgets' overall size
 * requests.
 *
 * Note that the sizes allocated to child widgets (nor to lines) are not
 * strictly monotonic: it is possible that this gives *less* space to a child
 * widget when run for a larger @space. This makes the @sizes output of this
 * function unsuitable to be used in widget measurements, in particular, as
 * for-sizes of child widgets when measuring the grid in the opposite
 * orientation. As a replacement, this function also outputs @monotonic_sizes;
 * those are potentially smaller than the corresponding @sizes, however they
 * *do* behave monotonically: they will not decrease as the @space increases.
 */
static void
grid_context_distribute (GridContext    *context,
                         GtkOrientation  orientation,
                         int             space,
                         const int      *for_size,
                         int            *sizes,
                         int            *monotonic_sizes,
                         int            *line_sizes)
{
  int *reqs, *targets;
  int i;
  int n_lines = context->lines[orientation].num;
  int spacing = get_spacing (context->layout, context->widget, orientation);
  int content_space;

  if (n_lines == 0)
    {
      g_assert (context->n_children == 0);
      return;
    }

  if (context->layout->linedata[orientation].homogeneous)
    {
      int line_size, extra;

      /* This is easy: spread the space evenly among lines.  The first 'extra'
       * lines get one additional pixel each.
       */
      g_assert (space >= spacing * (n_lines - 1));
      line_size = (space - spacing * (n_lines - 1)) / n_lines;
      extra = (space - spacing * (n_lines - 1)) % n_lines;

      if (line_sizes)
        for (i = 0; i < n_lines; i++)
          line_sizes[i] = line_size + (i < extra);

      if (sizes || monotonic_sizes)
        for (i = 0; i < context->n_children; i++)
          {
            gsize start, end, d, size;

            get_edge (context, orientation, i, &start, &end);
            d = end - start;
            size = d * line_size + (d - 1) * spacing;
            if (start < extra)
              size += MIN (end, extra) - start;

            if (sizes)
              sizes[i] = size;
            if (monotonic_sizes)
              monotonic_sizes[i] = size;
          }

      return;
    }

  reqs = g_newa (int, context->n_children + n_lines);
  targets = g_newa (int, context->n_children + n_lines);
  /* Grow up from minimum to natural sizes */
  for (i = 0; i < context->n_children; i++)
    gtk_widget_measure (context->children[i], orientation,
                        for_size ? for_size[i] : -1,
                        &reqs[i], &targets[i], NULL, NULL);

  if (monotonic_sizes)
    {
      /* Run only the primary growth, stopping at the first bottleneck */
      grid_grow_requirements (context, orientation, context->n_children,
                              reqs, targets, space, spacing, TRUE);
      /* Try to provide stronger guarantees implied by those requirements */
      grid_compute_closure (context, orientation, spacing,
                            reqs, monotonic_sizes);
    }
  if (!sizes && !line_sizes)
    return;

  grid_grow_requirements (context, orientation, context->n_children,
                          reqs, targets, space, spacing, FALSE);

  /* Grow expand children unboundedly */
  for (i = 0; i < context->n_children; i++)
    {
      if (gtk_widget_compute_expand (context->children[i], orientation))
        targets[i] = G_MAXINT;
      else
        /* This should not be strictly required, since the previous rounds have
         * already applied any growth there was room for.  Still, it should make
         * it a little easier for the algorithm to see that these edges are
         * immediately inactive.
         */
        targets[i] = reqs[i];
    }
  content_space = grid_grow_requirements (context, orientation,
                                          context->n_children,
                                          reqs, targets, space,
                                          spacing, FALSE);
  g_assert (content_space <= space);

  /* Fix child edges, and now grow line edges from 0 to infinity.  Clamp the
   * space available for line growth by what the children widgets actually use.
   *
   * This is how we actually allocate sizes to lines: it lets us both respect
   * the per-edge requirements established above, as well as provide nice
   * evenness where there's an ambiguity.
   */
  for (i = 0; i < context->n_children; i++)
    targets[i] = reqs[i];
  for (; i < context->n_children + n_lines; i++)
    {
      reqs[i] = 0;
      targets[i] = G_MAXINT;
    }
  grid_grow_requirements (context, orientation,
                          context->n_children + n_lines,
                          reqs, targets, content_space,
                          spacing, FALSE);
  /* Done growing, now produce outputs */
  if (line_sizes)
    memcpy (line_sizes, reqs + context->n_children,
            sizeof (int) * n_lines);

  if (sizes)
    for (i = 0; i < context->n_children; i++)
      {
        gsize start, end, c;

        get_edge (context, orientation, i, &start, &end);
        sizes[i] = spacing * (end - start - 1);
        for (c = start; c < end; c++)
          sizes[i] += reqs[context->n_children + c];
      }
}

static void
gtk_grid_layout_measure (GtkLayoutManager *manager,
                         GtkWidget        *widget,
                         GtkOrientation    orientation,
                         int               for_size,
                         int              *minimum,
                         int              *natural,
                         int              *minimum_baseline,
                         int              *natural_baseline)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (manager);
  GridContext context;
  int *min_sizes = NULL, *nat_sizes = NULL;

  context.layout = self;
  context.widget = widget;
  grid_context_init (&context);

  if (for_size != -1 && (orientation != grid_primary_orientation (manager) ||
                         self->linedata[orientation].homogeneous))
    {
      /* When measuring height for a specific width, we have to distribute that
       * available width first.  We have to use the conservative monotonic
       * widths for subsequently measuring the minimum heights of the children,
       * but we can use the more accurate non-monotonic widths for the natural
       * height calculation.
       */
      min_sizes = g_newa (int, context.n_children);
      nat_sizes = g_newa (int, context.n_children);
      grid_context_distribute (&context, OPPOSITE_ORIENTATION (orientation),
                               for_size, NULL, nat_sizes, min_sizes, NULL);
    }
  else if (for_size != -1)
    {
      /* When measuring width for a specific height, we need to binary search.
       * This is not going to be fast by any measure.
       */
      int low, high, mid;
      int opposite_min, opposite_nat;

      min_sizes = g_newa (int, context.n_children);
      nat_sizes = g_newa (int, context.n_children);

      grid_context_measure (&context, orientation,
                            NULL, NULL,
                            &opposite_min, &opposite_nat);

      /* Anything below 'low' is known not to fit, but 'low' itself might.
       * 'high' or larger is known to fit.
       */
      low = opposite_min;
      high = G_MAXINT;
      while (low < high)
        {
          if (high == G_MAXINT)
            mid = MAX (low, 16) * 2;
          else
            mid = low + (high - low) / 2;

          grid_context_distribute (&context, orientation, mid,
                                   NULL, NULL, min_sizes, NULL);
          grid_context_measure (&context, OPPOSITE_ORIENTATION (orientation),
                                min_sizes, NULL, &opposite_min, &opposite_nat);

          if (opposite_min > for_size)
            /* does not fit */
            low = mid + 1;
          else
            /* fits */
            high = mid;
        }
      *minimum = high;

      /* This is questionable */
      grid_context_distribute (&context, OPPOSITE_ORIENTATION (orientation),
                               for_size, NULL, nat_sizes, NULL, NULL);
      grid_context_measure (&context, orientation, NULL, nat_sizes,
                            &opposite_min, &opposite_nat);
      *natural = MAX (*minimum, opposite_nat);

      *minimum_baseline = *natural_baseline = -1;
      grid_context_clear (&context);
      return;
    }

  grid_context_measure (&context, orientation,
                        min_sizes, nat_sizes,
                        minimum, natural);
  grid_context_clear (&context);
  *minimum_baseline = *natural_baseline = -1;
}


#define GET_SIZE(width, height, orientation) (orientation == GTK_ORIENTATION_HORIZONTAL ? width : height)

static void
gtk_grid_layout_allocate (GtkLayoutManager *manager,
                          GtkWidget        *widget,
                          int               width,
                          int               height,
                          int               baseline)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (manager);
  GridContext context;
  int *child_widths, *child_heights;
  int *column_widths, *row_heights;
  GtkOrientation orientation = grid_primary_orientation (manager);
  int column_spacing, row_spacing;
  int i;

  context.layout = self;
  context.widget = widget;
  grid_context_init (&context);

  child_widths = g_newa (int, context.n_children);
  child_heights = g_newa (int, context.n_children);
  column_widths = g_newa (int, context.lines[GTK_ORIENTATION_HORIZONTAL].num);
  row_heights = g_newa (int, context.lines[GTK_ORIENTATION_VERTICAL].num);

  grid_context_distribute (&context, orientation,
                           GET_SIZE (width, height, orientation), NULL,
                           GET_SIZE (child_widths, child_heights, orientation),
                           NULL,
                           GET_SIZE (column_widths, row_heights, orientation));
  grid_context_distribute (&context, OPPOSITE_ORIENTATION (orientation),
                           GET_SIZE (height, width, orientation),
                           GET_SIZE (child_widths, child_heights, orientation),
                           GET_SIZE (child_heights, child_widths, orientation),
                           NULL,
                           GET_SIZE (row_heights, column_widths, orientation));

  column_spacing = get_spacing (self, widget, GTK_ORIENTATION_HORIZONTAL);
  row_spacing = get_spacing (self, widget, GTK_ORIENTATION_VERTICAL);

  for (i = 0; i < context.n_children; i++)
    {
      gsize start, end, j;
      GtkWidget *child;
      GtkAllocation allocation;

      get_edge (&context, GTK_ORIENTATION_HORIZONTAL, i, &start, &end);
      allocation.x = MAX (0, start) * column_spacing;
      for (j = 0; j < start; j++)
        allocation.x += column_widths[j];
      child = get_edge (&context, GTK_ORIENTATION_VERTICAL, i, &start, &end);
      allocation.y = MAX (0, start) * row_spacing;
      for (j = 0; j < start; j++)
        allocation.y += row_heights[j];

      allocation.width = child_widths[i];
      allocation.height = child_heights[i];
      gtk_widget_size_allocate (child, &allocation, -1);
    }

  grid_context_clear (&context);
}

static void
gtk_grid_layout_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ROW_SPACING:
      gtk_grid_layout_set_row_spacing (self, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPACING:
      gtk_grid_layout_set_column_spacing (self, g_value_get_int (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      gtk_grid_layout_set_row_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      gtk_grid_layout_set_column_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_BASELINE_ROW:
      gtk_grid_layout_set_baseline_row (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ROW_SPACING:
      g_value_set_int (value, COLUMNS (self)->spacing);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_int (value, ROWS (self)->spacing);
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, COLUMNS (self)->homogeneous);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, ROWS (self)->homogeneous);
      break;

    case PROP_BASELINE_ROW:
      g_value_set_int (value, self->baseline_row);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_finalize (GObject *gobject)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  g_clear_pointer (&self->row_properties, g_array_unref);

  G_OBJECT_CLASS (gtk_grid_layout_parent_class)->finalize (gobject);
}

static void
gtk_grid_layout_class_init (GtkGridLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->layout_child_type = GTK_TYPE_GRID_LAYOUT_CHILD;
  layout_class->measure = gtk_grid_layout_measure;
  layout_class->allocate = gtk_grid_layout_allocate;

  gobject_class->set_property = gtk_grid_layout_set_property;
  gobject_class->get_property = gtk_grid_layout_get_property;
  gobject_class->finalize = gtk_grid_layout_finalize;

  /**
   * GtkGridLayout:row-spacing:
   *
   * The amount of space between to consecutive rows.
   */
  layout_props[PROP_ROW_SPACING] =
    g_param_spec_int ("row-spacing", NULL, NULL,
                      0, G_MAXINT16, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:column-spacing:
   *
   * The amount of space between to consecutive columns.
   */
  layout_props[PROP_COLUMN_SPACING] =
    g_param_spec_int ("column-spacing", NULL, NULL,
                      0, G_MAXINT16, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:row-homogeneous:
   *
   * Whether all the rows in the grid have the same height.
   */
  layout_props[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:column-homogeneous:
   *
   * Whether all the columns in the grid have the same width.
   */
  layout_props[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGridLayout:baseline-row:
   *
   * The row to align to the baseline, when `GtkWidget:valign` is set
   * to %GTK_ALIGN_BASELINE.
   */
  layout_props[PROP_BASELINE_ROW] =
    g_param_spec_int ("baseline-row", NULL, NULL,
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, layout_props);
}

static void
gtk_grid_layout_init (GtkGridLayout *self)
{
}

/**
 * gtk_grid_layout_new:
 *
 * Creates a new `GtkGridLayout`.
 *
 * Returns: the newly created `GtkGridLayout`
 */
GtkLayoutManager *
gtk_grid_layout_new (void)
{
  return g_object_new (GTK_TYPE_GRID_LAYOUT, NULL);
}

/**
 * gtk_grid_layout_set_row_homogeneous:
 * @grid: a `GtkGridLayout`
 * @homogeneous: %TRUE to make rows homogeneous
 *
 * Sets whether all rows of @grid should have the same height.
 */
void
gtk_grid_layout_set_row_homogeneous (GtkGridLayout *grid,
                                     gboolean       homogeneous)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  /* Yes, homogeneous rows means all the columns have the same size */
  if (COLUMNS (grid)->homogeneous == !!homogeneous)
    return;

  COLUMNS (grid)->homogeneous = !!homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_ROW_HOMOGENEOUS]);
}

/**
 * gtk_grid_layout_get_row_homogeneous:
 * @grid: a `GtkGridLayout`
 *
 * Checks whether all rows of @grid should have the same height.
 *
 * Returns: %TRUE if the rows are homogeneous, and %FALSE otherwise
 */
gboolean
gtk_grid_layout_get_row_homogeneous (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), FALSE);

  return COLUMNS (grid)->homogeneous;
}

/**
 * gtk_grid_layout_set_row_spacing:
 * @grid: a `GtkGridLayout`
 * @spacing: the amount of space between rows, in pixels
 *
 * Sets the amount of space to insert between consecutive rows.
 */
void
gtk_grid_layout_set_row_spacing (GtkGridLayout *grid,
                                 guint          spacing)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  if (COLUMNS (grid)->spacing == spacing)
    return;

  COLUMNS (grid)->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_ROW_SPACING]);
}

/**
 * gtk_grid_layout_get_row_spacing:
 * @grid: a `GtkGridLayout`
 *
 * Retrieves the spacing set with gtk_grid_layout_set_row_spacing().
 *
 * Returns: the spacing between consecutive rows
 */
guint
gtk_grid_layout_get_row_spacing (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), 0);

  return COLUMNS (grid)->spacing;
}

/**
 * gtk_grid_layout_set_column_homogeneous:
 * @grid: a `GtkGridLayout`
 * @homogeneous: %TRUE to make columns homogeneous
 *
 * Sets whether all columns of @grid should have the same width.
 */
void
gtk_grid_layout_set_column_homogeneous (GtkGridLayout *grid,
                                        gboolean       homogeneous)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  /* Yes, homogeneous columns means all the rows have the same size */
  if (ROWS (grid)->homogeneous == !!homogeneous)
    return;

  ROWS (grid)->homogeneous = !!homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_COLUMN_HOMOGENEOUS]);
}

/**
 * gtk_grid_layout_get_column_homogeneous:
 * @grid: a `GtkGridLayout`
 *
 * Checks whether all columns of @grid should have the same width.
 *
 * Returns: %TRUE if the columns are homogeneous, and %FALSE otherwise
 */
gboolean
gtk_grid_layout_get_column_homogeneous (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), FALSE);

  return ROWS (grid)->homogeneous;
}

/**
 * gtk_grid_layout_set_column_spacing:
 * @grid: a `GtkGridLayout`
 * @spacing: the amount of space between columns, in pixels
 *
 * Sets the amount of space to insert between consecutive columns.
 */
void
gtk_grid_layout_set_column_spacing (GtkGridLayout *grid,
                                    guint          spacing)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  if (ROWS (grid)->spacing == spacing)
    return;

  ROWS (grid)->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_COLUMN_SPACING]);
}

/**
 * gtk_grid_layout_get_column_spacing:
 * @grid: a `GtkGridLayout`
 *
 * Retrieves the spacing set with gtk_grid_layout_set_column_spacing().
 *
 * Returns: the spacing between consecutive columns
 */
guint
gtk_grid_layout_get_column_spacing (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), 0);

  return ROWS (grid)->spacing;
}

static GridRowProperties *
find_row_properties (GtkGridLayout *self,
		     int            row)
{
  int i;

  if (self->row_properties == NULL)
    return NULL;

  for (i = 0; i < self->row_properties->len; i++)
    {
      GridRowProperties *prop = &g_array_index (self->row_properties, GridRowProperties, i);

      if (prop->row == row)
        return prop;
    }

  return NULL;
}

static GridRowProperties *
get_row_properties_or_create (GtkGridLayout *self,
			      int            row)
{
  GridRowProperties *props;

  props = find_row_properties (self, row);
  if (props != NULL)
    return props;

  /* This is the only place where we create the row properties array;
   * find_row_properties() is used by getters, so we should not create
   * the array there.
   */
  if (self->row_properties == NULL)
    self->row_properties = g_array_new (FALSE, FALSE, sizeof (GridRowProperties));

  g_array_append_vals (self->row_properties, &grid_row_properties_default, 1);
  props = &g_array_index (self->row_properties, GridRowProperties, self->row_properties->len - 1);
  props->row = row;

  return props;
}

static const GridRowProperties *
get_row_properties_or_default (GtkGridLayout *self,
			       int            row)
{
  GridRowProperties *props;

  props = find_row_properties (self, row);
  if (props != NULL)
    return props;

  return &grid_row_properties_default;
}

/**
 * gtk_grid_layout_set_row_baseline_position:
 * @grid: a `GtkGridLayout`
 * @row: a row index
 * @pos: a `GtkBaselinePosition`
 *
 * Sets how the baseline should be positioned on @row of the
 * grid, in case that row is assigned more space than is requested.
 */
void
gtk_grid_layout_set_row_baseline_position (GtkGridLayout       *grid,
                                           int                  row,
                                           GtkBaselinePosition  pos)
{
  GridRowProperties *props;

  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  props = get_row_properties_or_create (grid, row);

  if (props->baseline_position == pos)
    return;

  props->baseline_position = pos;
  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
}

/**
 * gtk_grid_layout_get_row_baseline_position:
 * @grid: a `GtkGridLayout`
 * @row: a row index
 *
 * Returns the baseline position of @row.
 *
 * If no value has been set with
 * [method@Gtk.GridLayout.set_row_baseline_position],
 * the default value of %GTK_BASELINE_POSITION_CENTER
 * is returned.
 *
 * Returns: the baseline position of @row
 */
GtkBaselinePosition
gtk_grid_layout_get_row_baseline_position (GtkGridLayout *grid,
                                           int            row)
{
  const GridRowProperties *props;

  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), GTK_BASELINE_POSITION_CENTER);

  props = get_row_properties_or_default (grid, row);

  return props->baseline_position;
}

/**
 * gtk_grid_layout_set_baseline_row:
 * @grid: a `GtkGridLayout`
 * @row: the row index
 *
 * Sets which row defines the global baseline for the entire grid.
 *
 * Each row in the grid can have its own local baseline, but only
 * one of those is global, meaning it will be the baseline in the
 * parent of the @grid.
 */
void
gtk_grid_layout_set_baseline_row (GtkGridLayout *grid,
                                  int            row)
{
  g_return_if_fail (GTK_IS_GRID_LAYOUT (grid));

  if (grid->baseline_row == row)
    return;

  grid->baseline_row = row;
  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (grid));
  g_object_notify_by_pspec (G_OBJECT (grid), layout_props[PROP_BASELINE_ROW]);
}

/**
 * gtk_grid_layout_get_baseline_row:
 * @grid: a `GtkGridLayout`
 *
 * Retrieves the row set with gtk_grid_layout_set_baseline_row().
 *
 * Returns: the global baseline row
 */
int
gtk_grid_layout_get_baseline_row (GtkGridLayout *grid)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT (grid), 0);

  return grid->baseline_row;
}
/* }}} */
