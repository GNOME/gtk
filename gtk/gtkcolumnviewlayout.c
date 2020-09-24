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

#include "gtkcolumnviewlayoutprivate.h"

#include "gtkcolumnviewcellprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewprivate.h"
#include "gtkcolumnviewtitleprivate.h"
#include "gtkwidgetprivate.h"

struct _GtkColumnViewLayout
{
  GtkLayoutManager parent_instance;

  GtkColumnView *view; /* no reference */
};

G_DEFINE_TYPE (GtkColumnViewLayout, gtk_column_view_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_column_view_layout_measure_along (GtkColumnViewLayout *self,
                                      GtkWidget           *widget,
                                      int                  for_size,
                                      int                 *minimum,
                                      int                 *natural,
                                      int                 *minimum_baseline,
                                      int                 *natural_baseline)
{
  GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;
  GtkWidget *child;
  guint i, n;
  GtkRequestedSize *sizes = NULL;

  if (for_size > -1)
    {
      n = g_list_model_get_n_items (gtk_column_view_get_columns (self->view));
      sizes = g_newa (GtkRequestedSize, n);
      gtk_column_view_distribute_width (self->view, for_size, sizes);
    }

  for (child = _gtk_widget_get_first_child (widget), i = 0;
       child != NULL;
       child = _gtk_widget_get_next_sibling (child), i++)
    {
      int child_min = 0;
      int child_nat = 0;
      int child_min_baseline = -1;
      int child_nat_baseline = -1;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, orientation,
                          for_size > -1 ? sizes[i].minimum_size : -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);

      if (child_min_baseline > -1)
        *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
      if (child_nat_baseline > -1)
        *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
    }
}

static void
gtk_column_view_layout_measure (GtkLayoutManager *layout,
                                GtkWidget        *widget,
                                GtkOrientation    orientation,
                                int               for_size,
                                int              *minimum,
                                int              *natural,
                                int              *minimum_baseline,
                                int              *natural_baseline)
{
  GtkColumnViewLayout *self = GTK_COLUMN_VIEW_LAYOUT (layout);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_column_view_measure_across (GTK_COLUMN_VIEW (self->view),
                                      minimum,
                                      natural);
    }
  else
    {
      gtk_column_view_layout_measure_along (self,
                                            widget, 
                                            for_size,
                                            minimum,
                                            natural,
                                            minimum_baseline,
                                            natural_baseline);
    }
}

static void
gtk_column_view_layout_allocate (GtkLayoutManager *layout_manager,
                                 GtkWidget        *widget,
                                 int               width,
                                 int               height,
                                 int               baseline)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkColumnViewColumn *column;
      int col_x, col_width, min;

      if (!gtk_widget_should_layout (child))
        continue;

      if (GTK_IS_COLUMN_VIEW_CELL (child))
        {
          column = gtk_column_view_cell_get_column (GTK_COLUMN_VIEW_CELL (child));
          gtk_column_view_column_get_allocation (column, &col_x, &col_width);
        }
      else
        {
          column = gtk_column_view_title_get_column (GTK_COLUMN_VIEW_TITLE (child));
          gtk_column_view_column_get_header_allocation (column, &col_x, &col_width);
        }

      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

      gtk_widget_size_allocate (child, &(GtkAllocation) { col_x, 0, MAX (min, col_width), height }, baseline);
    }
}

static void
gtk_column_view_layout_class_init (GtkColumnViewLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_manager_class->measure = gtk_column_view_layout_measure;
  layout_manager_class->allocate = gtk_column_view_layout_allocate;
}

static void
gtk_column_view_layout_init (GtkColumnViewLayout *self)
{
}

GtkLayoutManager *
gtk_column_view_layout_new (GtkColumnView *view)
{
  GtkColumnViewLayout *result;

  result = g_object_new (GTK_TYPE_COLUMN_VIEW_LAYOUT, NULL);

  result->view = view;

  return GTK_LAYOUT_MANAGER (result);
}
