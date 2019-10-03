/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
 * Author: Matthias Clasen
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

#include <string.h>

#include "gtkgrid.h"

#include "gtkcontainerprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkgridlayout.h"
#include "gtkorientableprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"


/**
 * SECTION:gtkgrid
 * @Short_description: Pack widgets in rows and columns
 * @Title: GtkGrid
 * @See_also: #GtkBox
 *
 * GtkGrid is a container which arranges its child widgets in
 * rows and columns, with arbitrary positions and horizontal/vertical spans.
 *
 * Children are added using gtk_grid_attach(). They can span multiple
 * rows or columns. It is also possible to add a child next to an
 * existing child, using gtk_grid_attach_next_to(). The behaviour of
 * GtkGrid when several children occupy the same grid cell is undefined.
 *
 * GtkGrid can be used like a #GtkBox by just using gtk_container_add(),
 * which will place children next to each other in the direction determined
 * by the #GtkOrientable:orientation property. However, if all you want is a
 * single row or column, then #GtkBox is the preferred widget.
 *
 * # CSS nodes
 *
 * GtkGrid uses a single CSS node with name grid.
 */

typedef struct
{
  GtkLayoutManager *layout_manager;

  GtkOrientation orientation;
} GtkGridPrivate;

enum
{
  PROP_0,
  PROP_ROW_SPACING,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,
  PROP_BASELINE_ROW,
  N_PROPERTIES,

  /* GtkOrientable */
  PROP_ORIENTATION
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkGrid, gtk_grid, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkGrid)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


static void
gtk_grid_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkGrid *grid = GTK_GRID (object);
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_ROW_SPACING:
      g_value_set_int (value, gtk_grid_layout_get_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_int (value, gtk_grid_layout_get_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_grid_layout_get_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_grid_layout_get_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    case PROP_BASELINE_ROW:
      g_value_set_int (value, gtk_grid_layout_get_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_set_orientation (GtkGrid        *grid,
                          GtkOrientation  orientation)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (grid));

      g_object_notify (G_OBJECT (grid), "orientation");
    }
}

static void
gtk_grid_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkGrid *grid = GTK_GRID (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_grid_set_orientation (grid, g_value_get_enum (value));
      break;

    case PROP_ROW_SPACING:
      gtk_grid_set_row_spacing (grid, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPACING:
      gtk_grid_set_column_spacing (grid, g_value_get_int (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      gtk_grid_set_row_homogeneous (grid, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      gtk_grid_set_column_homogeneous (grid, g_value_get_boolean (value));
      break;

    case PROP_BASELINE_ROW:
      gtk_grid_set_baseline_row (grid, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
grid_attach (GtkGrid   *grid,
             GtkWidget *widget,
             gint       left,
             gint       top,
             gint       width,
             gint       height)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *grid_child;

  gtk_widget_set_parent (widget, GTK_WIDGET (grid));

  grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, widget));
  gtk_grid_layout_child_set_left_attach (grid_child, left);
  gtk_grid_layout_child_set_top_attach (grid_child, top);
  gtk_grid_layout_child_set_column_span (grid_child, width);
  gtk_grid_layout_child_set_row_span (grid_child, height);
}

/* Find the position 'touching' existing
 * children. @orientation and @max determine
 * from which direction to approach (horizontal
 * + max = right, vertical + !max = top, etc).
 * @op_pos, @op_span determine the rows/columns
 * in which the touching has to happen.
 */
static gint
find_attach_position (GtkGrid         *grid,
                      GtkOrientation   orientation,
                      gint             op_pos,
                      gint             op_span,
                      gboolean         max)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;
  gboolean hit;
  gint pos;

  if (max)
    pos = -G_MAXINT;
  else
    pos = G_MAXINT;

  hit = FALSE;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      int attach_pos = 0, attach_span = 0;
      int opposite_pos = 0, opposite_span = 0;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));

      switch (orientation)
        {
        case GTK_ORIENTATION_HORIZONTAL:
          attach_pos = gtk_grid_layout_child_get_left_attach (grid_child);
          attach_span = gtk_grid_layout_child_get_column_span (grid_child);
          opposite_pos = gtk_grid_layout_child_get_top_attach (grid_child);
          opposite_span = gtk_grid_layout_child_get_row_span (grid_child);
          break;

        case GTK_ORIENTATION_VERTICAL:
          attach_pos = gtk_grid_layout_child_get_top_attach (grid_child);
          attach_span = gtk_grid_layout_child_get_row_span (grid_child);
          opposite_pos = gtk_grid_layout_child_get_left_attach (grid_child);
          opposite_span = gtk_grid_layout_child_get_column_span (grid_child);
          break;

        default:
          break;
        }

      /* check if the ranges overlap */
      if (opposite_pos <= op_pos + op_span && op_pos <= opposite_pos + opposite_span)
        {
          hit = TRUE;

          if (max)
            pos = MAX (pos, attach_pos + attach_span);
          else
            pos = MIN (pos, attach_pos);
        }
     }

  if (!hit)
    pos = 0;

  return pos;
}

static void
gtk_grid_add (GtkContainer *container,
              GtkWidget    *child)
{
  GtkGrid *grid = GTK_GRID (container);
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  gint pos[2] = { 0, 0 };

  pos[priv->orientation] = find_attach_position (grid, priv->orientation, 0, 1, TRUE);
  grid_attach (grid, child, pos[0], pos[1], 1, 1);
}

static void
gtk_grid_remove (GtkContainer *container,
                 GtkWidget    *child)
{
  GtkGrid *grid = GTK_GRID (container);
  gboolean was_visible;

  was_visible = _gtk_widget_get_visible (child);
  gtk_widget_unparent (child);

  if (was_visible && _gtk_widget_get_visible (GTK_WIDGET (grid)))
    gtk_widget_queue_resize (GTK_WIDGET (grid));
}

static void
gtk_grid_forall (GtkContainer *container,
                 GtkCallback   callback,
                 gpointer      callback_data)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (container));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      (* callback) (child, callback_data);

      child = next;
    }
}

static GType
gtk_grid_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_grid_dispose (GObject *object)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (object));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      gtk_widget_unparent (child);

      child = next;
    }

  G_OBJECT_CLASS (gtk_grid_parent_class)->dispose (object);
}

static void
gtk_grid_class_init (GtkGridClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->dispose = gtk_grid_dispose;
  object_class->get_property = gtk_grid_get_property;
  object_class->set_property = gtk_grid_set_property;

  container_class->add = gtk_grid_add;
  container_class->remove = gtk_grid_remove;
  container_class->forall = gtk_grid_forall;
  container_class->child_type = gtk_grid_child_type;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  obj_properties[PROP_ROW_SPACING] =
    g_param_spec_int ("row-spacing",
                      P_("Row spacing"),
                      P_("The amount of space between two consecutive rows"),
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_COLUMN_SPACING] =
    g_param_spec_int ("column-spacing",
                      P_("Column spacing"),
                      P_("The amount of space between two consecutive columns"),
                      0, G_MAXINT16, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous",
                          P_("Row Homogeneous"),
                          P_("If TRUE, the rows are all the same height"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous",
                          P_("Column Homogeneous"),
                          P_("If TRUE, the columns are all the same width"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  obj_properties[PROP_BASELINE_ROW] =
    g_param_spec_int ("baseline-row",
                      P_("Baseline Row"),
                      P_("The row to align the to the baseline when valign is GTK_ALIGN_BASELINE"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_css_name (widget_class, I_("grid"));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_GRID_LAYOUT);
}

static void
gtk_grid_init (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  priv->layout_manager = gtk_widget_get_layout_manager (GTK_WIDGET (grid));

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (grid));
}

/**
 * gtk_grid_new:
 *
 * Creates a new grid widget.
 *
 * Returns: the new #GtkGrid
 */
GtkWidget *
gtk_grid_new (void)
{
  return g_object_new (GTK_TYPE_GRID, NULL);
}

/**
 * gtk_grid_attach:
 * @grid: a #GtkGrid
 * @child: the widget to add
 * @left: the column number to attach the left side of @child to
 * @top: the row number to attach the top side of @child to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The position of @child is determined by @left and @top. The
 * number of “cells” that @child will occupy is determined by
 * @width and @height.
 */
void
gtk_grid_attach (GtkGrid   *grid,
                 GtkWidget *child,
                 gint       left,
                 gint       top,
                 gint       width,
                 gint       height)
{
  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  grid_attach (grid, child, left, top, width, height);
}

/**
 * gtk_grid_attach_next_to:
 * @grid: a #GtkGrid
 * @child: the widget to add
 * @sibling: (allow-none): the child of @grid that @child will be placed
 *     next to, or %NULL to place @child at the beginning or end
 * @side: the side of @sibling that @child is positioned next to
 * @width: the number of columns that @child will span
 * @height: the number of rows that @child will span
 *
 * Adds a widget to the grid.
 *
 * The widget is placed next to @sibling, on the side determined by
 * @side. When @sibling is %NULL, the widget is placed in row (for
 * left or right placement) or column 0 (for top or bottom placement),
 * at the end indicated by @side.
 *
 * Attaching widgets labeled [1], [2], [3] with @sibling == %NULL and
 * @side == %GTK_POS_LEFT yields a layout of [3][2][1].
 */
void
gtk_grid_attach_next_to (GtkGrid         *grid,
                         GtkWidget       *child,
                         GtkWidget       *sibling,
                         GtkPositionType  side,
                         gint             width,
                         gint             height)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *grid_sibling;
  gint left, top;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (sibling == NULL || _gtk_widget_get_parent (sibling) == (GtkWidget*)grid);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  if (sibling != NULL)
    {
      grid_sibling = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, sibling));

      switch (side)
        {
        case GTK_POS_LEFT:
          left = gtk_grid_layout_child_get_left_attach (grid_sibling) - width;
          top = gtk_grid_layout_child_get_top_attach (grid_sibling);
          break;
        case GTK_POS_RIGHT:
          left = gtk_grid_layout_child_get_left_attach (grid_sibling) +
                 gtk_grid_layout_child_get_column_span (grid_sibling);
          top = gtk_grid_layout_child_get_top_attach (grid_sibling);
          break;
        case GTK_POS_TOP:
          left = gtk_grid_layout_child_get_left_attach (grid_sibling);
          top = gtk_grid_layout_child_get_top_attach (grid_sibling) - height;
          break;
        case GTK_POS_BOTTOM:
          left = gtk_grid_layout_child_get_left_attach (grid_sibling);
          top = gtk_grid_layout_child_get_top_attach (grid_sibling) +
                gtk_grid_layout_child_get_row_span (grid_sibling);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      switch (side)
        {
        case GTK_POS_LEFT:
          left = find_attach_position (grid, GTK_ORIENTATION_HORIZONTAL, 0, height, FALSE);
          left -= width;
          top = 0;
          break;
        case GTK_POS_RIGHT:
          left = find_attach_position (grid, GTK_ORIENTATION_HORIZONTAL, 0, height, TRUE);
          top = 0;
          break;
        case GTK_POS_TOP:
          left = 0;
          top = find_attach_position (grid, GTK_ORIENTATION_VERTICAL, 0, width, FALSE);
          top -= height;
          break;
        case GTK_POS_BOTTOM:
          left = 0;
          top = find_attach_position (grid, GTK_ORIENTATION_VERTICAL, 0, width, TRUE);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  grid_attach (grid, child, left, top, width, height);
}

/**
 * gtk_grid_get_child_at:
 * @grid: a #GtkGrid
 * @left: the left edge of the cell
 * @top: the top edge of the cell
 *
 * Gets the child of @grid whose area covers the grid
 * cell whose upper left corner is at @left, @top.
 *
 * Returns: (transfer none) (nullable): the child at the given position, or %NULL
 */
GtkWidget *
gtk_grid_get_child_at (GtkGrid *grid,
                       gint     left,
                       gint     top)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_val_if_fail (GTK_IS_GRID (grid), NULL);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      int child_left, child_top, child_width, child_height;
      
      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      child_left = gtk_grid_layout_child_get_left_attach (grid_child);
      child_top = gtk_grid_layout_child_get_top_attach (grid_child);
      child_width = gtk_grid_layout_child_get_column_span (grid_child);
      child_height = gtk_grid_layout_child_get_row_span (grid_child);

      if (child_left <= left &&
          child_left + child_width > left &&
          child_top <= top &&
          child_top + child_height > top)
        return child;
    }

  return NULL;
}

/**
 * gtk_grid_insert_row:
 * @grid: a #GtkGrid
 * @position: the position to insert the row at
 *
 * Inserts a row at the specified position.
 *
 * Children which are attached at or below this position
 * are moved one row down. Children which span across this
 * position are grown to span the new row.
 */
void
gtk_grid_insert_row (GtkGrid *grid,
                     gint     position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;
  int top, height;

  g_return_if_fail (GTK_IS_GRID (grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      
      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      top = gtk_grid_layout_child_get_top_attach (grid_child);
      height = gtk_grid_layout_child_get_row_span (grid_child);

      if (top >= position)
        gtk_grid_layout_child_set_top_attach (grid_child, top + 1);
      else if (top + height > position)
        gtk_grid_layout_child_set_row_span (grid_child, height + 1);
    }
}

/**
 * gtk_grid_remove_row:
 * @grid: a #GtkGrid
 * @position: the position of the row to remove
 *
 * Removes a row from the grid.
 *
 * Children that are placed in this row are removed,
 * spanning children that overlap this row have their
 * height reduced by one, and children below the row
 * are moved up.
 */
void
gtk_grid_remove_row (GtkGrid *grid,
                     gint     position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  child = gtk_widget_get_first_child (GTK_WIDGET (grid));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      GtkGridLayoutChild *grid_child;
      int top, height;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      top = gtk_grid_layout_child_get_top_attach (grid_child);
      height = gtk_grid_layout_child_get_row_span (grid_child);

      if (top <= position && top + height > position)
        height--;
      if (top > position)
        top--;

      if (height <= 0)
        {
          gtk_container_remove (GTK_CONTAINER (grid), child);
        }
      else
        {
          gtk_grid_layout_child_set_row_span (grid_child, height);
          gtk_grid_layout_child_set_top_attach (grid_child, top);
        }

      child = next;
    }
}

/**
 * gtk_grid_insert_column:
 * @grid: a #GtkGrid
 * @position: the position to insert the column at
 *
 * Inserts a column at the specified position.
 *
 * Children which are attached at or to the right of this position
 * are moved one column to the right. Children which span across this
 * position are grown to span the new column.
 */
void
gtk_grid_insert_column (GtkGrid *grid,
                        gint     position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkGridLayoutChild *grid_child;
      int left, width;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));
      left = gtk_grid_layout_child_get_left_attach (grid_child);
      width = gtk_grid_layout_child_get_column_span (grid_child);

      if (left >= position)
        gtk_grid_layout_child_set_left_attach (grid_child, left + 1);
      else if (left + width > position)
        gtk_grid_layout_child_set_column_span (grid_child, width + 1);
    }
}

/**
 * gtk_grid_remove_column:
 * @grid: a #GtkGrid
 * @position: the position of the column to remove
 *
 * Removes a column from the grid.
 *
 * Children that are placed in this column are removed,
 * spanning children that overlap this column have their
 * width reduced by one, and children after the column
 * are moved to the left.
 */
void
gtk_grid_remove_column (GtkGrid *grid,
                        gint     position)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_GRID (grid));

  child = gtk_widget_get_first_child (GTK_WIDGET (grid));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      GtkGridLayoutChild *grid_child;
      int left, width;

      grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));

      left = gtk_grid_layout_child_get_left_attach (grid_child);
      width = gtk_grid_layout_child_get_column_span (grid_child);

      if (left <= position && left + width > position)
        width--;
      if (left > position)
        left--;

      if (width <= 0)
        {
          gtk_container_remove (GTK_CONTAINER (grid), child);
        }
      else
        {
          gtk_grid_layout_child_set_column_span (grid_child, width);
          gtk_grid_layout_child_set_left_attach (grid_child, left);
        }

      child = next;
    }
}

/**
 * gtk_grid_insert_next_to:
 * @grid: a #GtkGrid
 * @sibling: the child of @grid that the new row or column will be
 *     placed next to
 * @side: the side of @sibling that @child is positioned next to
 *
 * Inserts a row or column at the specified position.
 *
 * The new row or column is placed next to @sibling, on the side
 * determined by @side. If @side is %GTK_POS_TOP or %GTK_POS_BOTTOM,
 * a row is inserted. If @side is %GTK_POS_LEFT of %GTK_POS_RIGHT,
 * a column is inserted.
 */
void
gtk_grid_insert_next_to (GtkGrid         *grid,
                         GtkWidget       *sibling,
                         GtkPositionType  side)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *child;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (sibling));
  g_return_if_fail (_gtk_widget_get_parent (sibling) == (GtkWidget*)grid);

  child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, sibling));

  switch (side)
    {
    case GTK_POS_LEFT:
      gtk_grid_insert_column (grid, gtk_grid_layout_child_get_left_attach (child));
      break;
    case GTK_POS_RIGHT:
      {
        int col = gtk_grid_layout_child_get_left_attach (child) +
                  gtk_grid_layout_child_get_column_span (child);
        gtk_grid_insert_column (grid, col);
      }
      break;
    case GTK_POS_TOP:
      gtk_grid_insert_row (grid, gtk_grid_layout_child_get_top_attach (child));
      break;
    case GTK_POS_BOTTOM:
      {
        int row = gtk_grid_layout_child_get_top_attach (child) +
                  gtk_grid_layout_child_get_row_span (child);
        gtk_grid_insert_row (grid, row);
      }
      break;
    default:
      g_assert_not_reached ();
    }
}

/**
 * gtk_grid_set_row_homogeneous:
 * @grid: a #GtkGrid
 * @homogeneous: %TRUE to make rows homogeneous
 *
 * Sets whether all rows of @grid will have the same height.
 */
void
gtk_grid_set_row_homogeneous (GtkGrid  *grid,
                              gboolean  homogeneous)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  gboolean old_val;

  g_return_if_fail (GTK_IS_GRID (grid));

  old_val = gtk_grid_layout_get_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_val != !!homogeneous)
    {
      gtk_grid_layout_set_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager), homogeneous);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_ROW_HOMOGENEOUS]);
    }
}

/**
 * gtk_grid_get_row_homogeneous:
 * @grid: a #GtkGrid
 *
 * Returns whether all rows of @grid have the same height.
 *
 * Returns: whether all rows of @grid have the same height.
 */
gboolean
gtk_grid_get_row_homogeneous (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  return gtk_grid_layout_get_row_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_column_homogeneous:
 * @grid: a #GtkGrid
 * @homogeneous: %TRUE to make columns homogeneous
 *
 * Sets whether all columns of @grid will have the same width.
 */
void
gtk_grid_set_column_homogeneous (GtkGrid  *grid,
                                 gboolean  homogeneous)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  gboolean old_val;

  g_return_if_fail (GTK_IS_GRID (grid));

  old_val = gtk_grid_layout_get_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_val != !!homogeneous)
    {
      gtk_grid_layout_set_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager), homogeneous);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_COLUMN_HOMOGENEOUS]);
    }
}

/**
 * gtk_grid_get_column_homogeneous:
 * @grid: a #GtkGrid
 *
 * Returns whether all columns of @grid have the same width.
 *
 * Returns: whether all columns of @grid have the same width.
 */
gboolean
gtk_grid_get_column_homogeneous (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), FALSE);

  return gtk_grid_layout_get_column_homogeneous (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_row_spacing:
 * @grid: a #GtkGrid
 * @spacing: the amount of space to insert between rows
 *
 * Sets the amount of space between rows of @grid.
 */
void
gtk_grid_set_row_spacing (GtkGrid *grid,
                          guint    spacing)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  guint old_spacing;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  old_spacing = gtk_grid_layout_get_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_spacing != spacing)
    {
      gtk_grid_layout_set_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager), spacing);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_ROW_SPACING]);
    }
}

/**
 * gtk_grid_get_row_spacing:
 * @grid: a #GtkGrid
 *
 * Returns the amount of space between the rows of @grid.
 *
 * Returns: the row spacing of @grid
 */
guint
gtk_grid_get_row_spacing (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return gtk_grid_layout_get_row_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_column_spacing:
 * @grid: a #GtkGrid
 * @spacing: the amount of space to insert between columns
 *
 * Sets the amount of space between columns of @grid.
 */
void
gtk_grid_set_column_spacing (GtkGrid *grid,
                             guint    spacing)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  guint old_spacing;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (spacing <= G_MAXINT16);

  old_spacing = gtk_grid_layout_get_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_spacing != spacing)
    {
      gtk_grid_layout_set_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager), spacing);
      g_object_notify_by_pspec (G_OBJECT (grid), obj_properties [PROP_COLUMN_SPACING]);
    }
}

/**
 * gtk_grid_get_column_spacing:
 * @grid: a #GtkGrid
 *
 * Returns the amount of space between the columns of @grid.
 *
 * Returns: the column spacing of @grid
 */
guint
gtk_grid_get_column_spacing (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return gtk_grid_layout_get_column_spacing (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_set_row_baseline_position:
 * @grid: a #GtkGrid
 * @row: a row index
 * @pos: a #GtkBaselinePosition
 *
 * Sets how the baseline should be positioned on @row of the
 * grid, in case that row is assigned more space than is requested.
 */
void
gtk_grid_set_row_baseline_position (GtkGrid            *grid,
				    gint                row,
				    GtkBaselinePosition pos)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_if_fail (GTK_IS_GRID (grid));

  gtk_grid_layout_set_row_baseline_position (GTK_GRID_LAYOUT (priv->layout_manager),
                                             row,
                                             pos);
}

/**
 * gtk_grid_get_row_baseline_position:
 * @grid: a #GtkGrid
 * @row: a row index
 *
 * Returns the baseline position of @row as set
 * by gtk_grid_set_row_baseline_position() or the default value
 * %GTK_BASELINE_POSITION_CENTER.
 *
 * Returns: the baseline position of @row
 */
GtkBaselinePosition
gtk_grid_get_row_baseline_position (GtkGrid      *grid,
				    gint          row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), GTK_BASELINE_POSITION_CENTER);

  return gtk_grid_layout_get_row_baseline_position (GTK_GRID_LAYOUT (priv->layout_manager), row);
}

/**
 * gtk_grid_set_baseline_row:
 * @grid: a #GtkGrid
 * @row: the row index
 *
 * Sets which row defines the global baseline for the entire grid.
 * Each row in the grid can have its own local baseline, but only
 * one of those is global, meaning it will be the baseline in the
 * parent of the @grid.
 */
void
gtk_grid_set_baseline_row (GtkGrid *grid,
			   gint     row)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  int old_row;

  g_return_if_fail (GTK_IS_GRID (grid));

  old_row = gtk_grid_layout_get_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager));
  if (old_row != row)
    {
      gtk_grid_layout_set_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager), row);
      g_object_notify (G_OBJECT (grid), "baseline-row");
    }
}

/**
 * gtk_grid_get_baseline_row:
 * @grid: a #GtkGrid
 *
 * Returns which row defines the global baseline of @grid.
 *
 * Returns: the row index defining the global baseline
 */
gint
gtk_grid_get_baseline_row (GtkGrid *grid)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);

  g_return_val_if_fail (GTK_IS_GRID (grid), 0);

  return gtk_grid_layout_get_baseline_row (GTK_GRID_LAYOUT (priv->layout_manager));
}

/**
 * gtk_grid_query_child:
 * @grid: a #GtkGrid
 * @child: a #GtkWidget child of @grid
 * @left: (out) (optional): the column used to attach the left side of @child
 * @top: (out) (optional): the row used to attach the top side of @child
 * @width: (out) (optional): the number of columns @child spans
 * @height: (out) (optional): the number of rows @child spans
 *
 * Queries the attach points and spans of @child inside the given #GtkGrid.
 */
void
gtk_grid_query_child (GtkGrid   *grid,
                      GtkWidget *child,
                      gint      *left,
                      gint      *top,
                      gint      *width,
                      gint      *height)
{
  GtkGridPrivate *priv = gtk_grid_get_instance_private (grid);
  GtkGridLayoutChild *grid_child;

  g_return_if_fail (GTK_IS_GRID (grid));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (_gtk_widget_get_parent (child) == (GtkWidget *) grid);

  grid_child = GTK_GRID_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout_manager, child));

  if (left != NULL)
    *left = gtk_grid_layout_child_get_left_attach (grid_child);
  if (top != NULL)
    *top = gtk_grid_layout_child_get_top_attach (grid_child);
  if (width != NULL)
    *width = gtk_grid_layout_child_get_column_span (grid_child);
  if (height != NULL)
    *height = gtk_grid_layout_child_get_row_span (grid_child);
}
