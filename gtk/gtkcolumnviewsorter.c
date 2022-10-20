/*
 * Copyright © 2019 Matthias Clasen
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkcolumnviewsorterprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmultisorter.h"

static GtkColumnViewColumn *
get_column (GtkInvertibleSorter *sorter)
{
  return GTK_COLUMN_VIEW_COLUMN (g_object_get_data (G_OBJECT (sorter), "column"));
}

static void
remove_column (GtkSorter           *self,
               GtkColumnViewColumn *column)
{
  GtkInvertibleSorter *sorter = gtk_column_view_column_get_invertible_sorter (column);

  if (sorter == NULL)
    return;

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self)); i++)
    {
      GtkInvertibleSorter *s;

      s = g_list_model_get_item (G_LIST_MODEL (self), i);
      g_object_unref (s);

      if (s == sorter)
        {
          gtk_multi_sorter_remove (GTK_MULTI_SORTER (self), i);
          break;
        }
    }
}

void
gtk_column_view_sorter_activate_column (GtkSorter           *self,
                                        GtkColumnViewColumn *column)
{
  GtkMultiSorter *multi = GTK_MULTI_SORTER (self);
  GtkInvertibleSorter *sorter, *s;

  g_return_if_fail (GTK_IS_MULTI_SORTER (self));
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column));

  sorter = gtk_column_view_column_get_invertible_sorter (column);
  if (sorter == NULL)
    return;

  if (g_list_model_get_n_items (G_LIST_MODEL (self)) > 0)
    {
      s = g_list_model_get_item (G_LIST_MODEL (self), 0);
    }
  else
    s = NULL;

  if (s == sorter)
    {
      /* column is already first, toggle sort order */
      gtk_invertible_sorter_set_sort_order (s, 1 - gtk_invertible_sorter_get_sort_order (s));

      gtk_column_view_column_notify_sort (column);
    }
  else
    {
      /* move column to the first position */
      remove_column (self, column);
      gtk_invertible_sorter_set_sort_order (GTK_INVERTIBLE_SORTER (sorter), GTK_SORT_ASCENDING);
      g_object_ref (sorter);
      gtk_multi_sorter_splice (multi, 0, 0, (GtkSorter **)&sorter, 1);

      if (s)
        {
          gtk_column_view_column_notify_sort (get_column (s));
          g_object_unref (s);
        }
      gtk_column_view_column_notify_sort (column);
    }
}

void
gtk_column_view_sorter_remove_column (GtkSorter           *self,
                                      GtkColumnViewColumn *column)
{
  g_return_if_fail (GTK_IS_MULTI_SORTER (self));
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column));

  remove_column (self, column);
}

void
gtk_column_view_sorter_set_column (GtkSorter           *self,
                                   GtkColumnViewColumn *column,
                                   GtkSortType          direction)
{
  GtkMultiSorter *multi = GTK_MULTI_SORTER (self);
  GtkSorter *sorter;
  GtkInvertibleSorter *s;

  g_return_if_fail (GTK_IS_MULTI_SORTER (self));
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column));

  sorter = GTK_SORTER (gtk_column_view_column_get_invertible_sorter (column));
  if (sorter == NULL)
    return;

  if (g_list_model_get_n_items (G_LIST_MODEL (self)) > 0)
    s = g_list_model_get_item (G_LIST_MODEL (self), 0);
  else
    s = NULL;

  remove_column (self, column);

  gtk_invertible_sorter_set_sort_order (GTK_INVERTIBLE_SORTER (sorter), direction);
  g_object_ref (sorter);
  gtk_multi_sorter_splice (multi, 0, 0, &sorter, 1);

  if (s)
    {
      gtk_column_view_column_notify_sort (get_column (s));
      g_object_unref (s);
    }

  gtk_column_view_column_notify_sort (column);
}

void
gtk_column_view_sorter_clear (GtkSorter *self)
{
  GtkMultiSorter *multi = GTK_MULTI_SORTER (self);
  GtkInvertibleSorter *s;

  g_return_if_fail (GTK_IS_MULTI_SORTER (self));

  if (g_list_model_get_n_items (G_LIST_MODEL (self)) == 0)
    return;

  s = g_list_model_get_item (G_LIST_MODEL (self), 0);

  gtk_multi_sorter_splice (multi, 0, g_list_model_get_n_items (G_LIST_MODEL (self)), NULL, 0);

  gtk_column_view_column_notify_sort (get_column (s));
  g_object_unref (s);
}
