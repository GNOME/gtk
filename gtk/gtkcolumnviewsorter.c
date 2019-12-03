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

#include "gtkintl.h"
#include "gtktypebuiltins.h"

typedef struct
{
  GtkSorter *sorter;
  gboolean   inverted;
  gulong     changed_id;
} Sorter;
 
struct _GtkColumnViewSorter
{
  GtkSorter parent_instance;

  GArray *sorters;
};

G_DEFINE_TYPE (GtkColumnViewSorter, gtk_column_view_sorter, GTK_TYPE_SORTER)

static int
gtk_column_view_sorter_compare (GtkSorter *sorter,
                                gpointer   item1,
                                gpointer   item2)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (sorter);
  int result = 0;
  int i;

  for (i = 0; i < self->sorters->len; i++)
    {
      Sorter *s = &g_array_index (self->sorters, Sorter, i);

      result = gtk_sorter_compare (s->sorter, item1, item2);
      if (s->inverted)
        result = - result;

      if (result != 0)
        break;
    }

  return result;
}

static void changed_cb (GtkSorter *sorter, int change, gpointer data);

static void
gtk_column_view_sorter_dispose (GObject *object)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (object);

  g_clear_pointer (&self->sorters, g_array_unref);

  G_OBJECT_CLASS (gtk_column_view_sorter_parent_class)->dispose (object);
}

static void
gtk_column_view_sorter_class_init (GtkColumnViewSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_column_view_sorter_compare;

  object_class->dispose = gtk_column_view_sorter_dispose;
}

static void
clear_sorter (gpointer data)
{
  Sorter *s = data;

  g_signal_handler_disconnect (s->sorter, s->changed_id);
  g_object_unref (s->sorter);
}

static void
create_sorters (GtkColumnViewSorter *self)
{
  self->sorters = g_array_new (FALSE, TRUE, sizeof (Sorter));
  g_array_set_clear_func (self->sorters, clear_sorter);
}

static void
gtk_column_view_sorter_init (GtkColumnViewSorter *self)
{
  create_sorters (self);
}

GtkSorter *
gtk_column_view_sorter_new (void)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_SORTER, NULL);
}

static void
changed_cb (GtkSorter *sorter, int change, gpointer data)
{
  gtk_sorter_changed (GTK_SORTER (data), GTK_SORTER_CHANGE_DIFFERENT);
}

void
gtk_column_view_sorter_add_sorter (GtkColumnViewSorter *self,
                                   GtkSorter           *sorter)
{
  Sorter data;
  Sorter *s;
  Sorter *s1;
  int i, j;

  g_return_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self));

  if (self->sorters->len > 0)
    {
      s = &g_array_index (self->sorters, Sorter, 0);
      if (s->sorter == sorter)
        {
          s->inverted = !s->inverted;
          goto out;
        }

      for (i = 1; i < self->sorters->len; i++)
        {
          s = &g_array_index (self->sorters, Sorter, i);
          if (s->sorter == sorter)
            {
              for (j = i - 1; j >= 0; j--)
                {
                  s1 = &g_array_index (self->sorters, Sorter, j);
                  s->sorter = s1->sorter;
                  s->inverted = s1->inverted;
                  s = s1;
                }

              s1->sorter = sorter;
              s1->inverted = FALSE;
              goto out;
            }
        }
    }

  data.sorter = g_object_ref (sorter);
  data.changed_id = g_signal_connect (sorter, "changed", G_CALLBACK (changed_cb), self);
  data.inverted = FALSE;
  g_array_append_vals (self->sorters, &data, 1);

out:
  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
}

void
gtk_column_view_sorter_remove_sorter (GtkColumnViewSorter *self,
                                      GtkSorter           *sorter)
{
  int i;

  for (i = 0; i < self->sorters->len; i++)
    {
      Sorter *s = &g_array_index (self->sorters, Sorter, i);
      if (s->sorter == sorter)
        {
          g_array_remove_index (self->sorters, i);
          gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
          break;
        }
    }
}

void
gtk_column_view_sorter_reset (GtkColumnViewSorter *self)
{
  if (self->sorters->len == 0)
    return;

  g_array_unref (self->sorters);
  create_sorters (self);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
}

GtkSorter *
gtk_column_view_sorter_get_active (GtkColumnViewSorter *self,
                                   gboolean            *inverted)
{
  Sorter *s;

  if (self->sorters->len == 0)
    return NULL;

  s = &g_array_index (self->sorters, Sorter, 0);

  *inverted = s->inverted;

  return s->sorter;
}
