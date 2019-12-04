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
  GtkColumnViewColumn *column;
  GtkSorter *sorter;
  gboolean   inverted;
  gulong     changed_id;
} Sorter;
 
static void
free_sorter (gpointer data)
{
  Sorter *s = data;

  g_signal_handler_disconnect (s->sorter, s->changed_id);
  g_object_unref (s->sorter);
  g_object_unref (s->column);
  g_free (s);
}

struct _GtkColumnViewSorter
{
  GtkSorter parent_instance;

  GList *sorters;
};

G_DEFINE_TYPE (GtkColumnViewSorter, gtk_column_view_sorter, GTK_TYPE_SORTER)

static int
gtk_column_view_sorter_compare (GtkSorter *sorter,
                                gpointer   item1,
                                gpointer   item2)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (sorter);
  int result = 0;
  GList *l;

  for (l = self->sorters; l; l = l->next)
    {
      Sorter *s = l->data;
      GtkSorter *ss = gtk_column_view_column_get_sorter (s->column);

      result = gtk_sorter_compare (ss, item1, item2);
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

  g_list_free_full (self->sorters, free_sorter);
  self->sorters = NULL;

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
gtk_column_view_sorter_init (GtkColumnViewSorter *self)
{
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

static gboolean
remove_column (GtkColumnViewSorter *self,
               GtkColumnViewColumn *column)
{
  GList *l;

  for (l = self->sorters; l; l = l->next)
    {
      Sorter *s = l->data;
      if (s->column == column)
        {
          self->sorters = g_list_remove_link (self->sorters, l);
          free_sorter (s);
          g_list_free (l);
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
gtk_column_view_sorter_add_column (GtkColumnViewSorter *self,
                                   GtkColumnViewColumn *column)
{
  GtkSorter *sorter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), FALSE);
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column), FALSE);

  sorter = gtk_column_view_column_get_sorter (column);
  if (sorter == NULL)
    return FALSE;

  if (self->sorters != NULL)
    {
      s = self->sorters->data;
      if (s->column == column)
        {
          s->inverted = !s->inverted;
          goto out;
        }
    }

  remove_column (self, column);

  s = g_new (Sorter, 1);
  s->column = g_object_ref (column);
  s->sorter = g_object_ref (sorter);
  s->changed_id = g_signal_connect (sorter, "changed", G_CALLBACK (changed_cb), self);
  s->inverted = FALSE;
 
  self->sorters = g_list_prepend (self->sorters, s);

out:
  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  return TRUE;
}

gboolean
gtk_column_view_sorter_remove_column (GtkColumnViewSorter *self,
                                      GtkColumnViewColumn *column)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), FALSE);
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column), FALSE);

  if (remove_column (self, column))
    {
      gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
      return TRUE;
    }

  return FALSE;
}

gboolean
gtk_column_view_sorter_set_column (GtkColumnViewSorter *self,
                                   GtkColumnViewColumn *column,
                                   gboolean             inverted)
{
  GtkSorter *sorter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), FALSE);
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column), FALSE);
  
  sorter = gtk_column_view_column_get_sorter (column);
  if (sorter == NULL)
    return FALSE;

  g_list_free_full (self->sorters, free_sorter);

  s = g_new (Sorter, 1);
  s->column = g_object_ref (column);
  s->sorter = g_object_ref (sorter);
  s->changed_id = g_signal_connect (sorter, "changed", G_CALLBACK (changed_cb), self);
  s->inverted = inverted;
 
  self->sorters = g_list_prepend (self->sorters, s);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  return TRUE;
}

void
gtk_column_view_sorter_reset (GtkColumnViewSorter *self)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self));

  if (self->sorters == NULL)
    return;

  g_list_free_full (self->sorters, free_sorter);
  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
}

GtkColumnViewColumn *
gtk_column_view_sorter_get_active (GtkColumnViewSorter *self,
                                   gboolean            *inverted)
{
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), NULL);

  if (self->sorters == NULL)
    return NULL;

  s = self->sorters->data;

  *inverted = s->inverted;

  return s->column;
}
