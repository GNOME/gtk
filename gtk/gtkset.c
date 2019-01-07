/*
 * Copyright © 2019 Red Hat, Inc.
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

#include "gtkset.h"

/* Store a set of unsigned integers as a sorted array of ranges.
 */

typedef struct
{
  guint first;
  guint n_items;
} Range;

struct _GtkSet
{
  GArray *ranges;
};

typedef struct
{
  GtkSet *set;
  Range *current;
  int idx;
  guint pos;
} GtkRealSetIter;

GtkSet *
gtk_set_new (void)
{
  GtkSet *set;

  set = g_new (GtkSet, 1);
  set->ranges = g_array_new (FALSE, FALSE, sizeof (Range));
 
  return set;
}

void
gtk_set_free (GtkSet *set)
{
  g_array_free (set->ranges, TRUE);
  g_free (set);
}

gboolean
gtk_set_contains (GtkSet   *set,
                  guint     item)
{
  int i;

  for (i = 0; i < set->ranges->len; i++)
    {
      Range *r = &g_array_index (set->ranges, Range, i);

      if (item < r->first)
        return FALSE;

      if (item < r->first + r->n_items)
        return TRUE;
    }

  return FALSE;
}

void
gtk_set_remove_all (GtkSet *set)
{
  g_array_set_size (set->ranges, 0);
}

static int
range_compare (Range *r, Range *s)
{
  int ret = 0;

  if (r->first + r->n_items < s->first)
    ret = -1;
  else if (s->first + s->n_items < r->first)
    ret = 1;

  return ret;
}

void
gtk_set_add_range (GtkSet   *set,
                   guint     first_item,
                   guint     n_items)
{
  int i;
  Range s;
  int first = -1;
  int last = -1;

  s.first = first_item;
  s.n_items = n_items;

  for (i = 0; i < set->ranges->len; i++)
    {
      Range *r = &g_array_index (set->ranges, Range, i);
      int cmp = range_compare (&s, r);

      if (cmp < 0)
        break;

      if (cmp == 0)
        {
          if (first < 0)
            first = i;
          last = i;
        }
    }

  if (first > -1)
    {
      Range *r;
      guint start;
      guint end;

      r = &g_array_index (set->ranges, Range, first);
      start = MIN (s.first, r->first);

      r = &g_array_index (set->ranges, Range, last);
      end = MAX (s.first + s.n_items - 1, r->first + r->n_items - 1);

      s.first = start;
      s.n_items = end - start + 1;

      g_array_remove_range (set->ranges, first, last - first + 1);
      g_array_insert_val (set->ranges, first, s);
    }
  else
    g_array_insert_val (set->ranges, i, s);
}

void
gtk_set_remove_range (GtkSet *set,
                      guint   first_item,
                      guint   n_items)
{
  Range s;
  int i;
  int first = -1;
  int last = -1;

  s.first = first_item;
  s.n_items = n_items;

  for (i = 0; i < set->ranges->len; i++)
    {
      Range *r = &g_array_index (set->ranges, Range, i);
      int cmp = range_compare (&s, r);

      if (cmp < 0)
        break;

      if (cmp == 0)
        {
          if (first < 0)
            first = i;
          last = i;
        }
    }

  if (first > -1)
    {
      Range *r;
      Range a[2];
      int k = 0;

      r = &g_array_index (set->ranges, Range, first);
      if (r->first < s.first)
        {
          a[k].first = r->first;
          a[k].n_items = s.first - r->first;
          k++;
        }
      r = &g_array_index (set->ranges, Range, last);
      if (r->first + r->n_items > s.first + s.n_items)
        {
          a[k].first = s.first + s.n_items;
          a[k].n_items = r->first + r->n_items - a[k].first;
          k++;
        }
      g_array_remove_range (set->ranges, first, last - first + 1);
      if (k > 0)
        g_array_insert_vals (set->ranges, first, a, k);
   }
}

void
gtk_set_add_item (GtkSet *set,
                  guint   item)
{
  gtk_set_add_range (set, item, 1);
}

void
gtk_set_remove_item (GtkSet   *set,
                     guint     item)
{
  gtk_set_remove_range (set, item, 1);
}

/* This is peculiar operation: Replace every number n >= first by n + shift
 * This is only supported for negative shift if the shifting does not cause any
 * ranges to overlap.
 */
void
gtk_set_shift (GtkSet *set,
               guint first,
               int shift)
{
  int i;

  for (i = 0; i < set->ranges->len; i++)
    {
      Range *r = &g_array_index (set->ranges, Range, i);
      if (r->first >= first)
        r->first += shift;
    }
}

void
gtk_set_iter_init (GtkSetIter *iter,
                   GtkSet     *set)
{
  GtkRealSetIter *ri = (GtkRealSetIter *)iter;

  ri->set = set;
  ri->idx = -1;
  ri->current = 0;
}

gboolean
gtk_set_iter_next (GtkSetIter *iter,
                   guint      *item)
{
  GtkRealSetIter *ri = (GtkRealSetIter *)iter;

  if (ri->idx == -1)
    { 
next_range:
      ri->idx++;

      if (ri->idx == ri->set->ranges->len)
        return FALSE;

      ri->current = &g_array_index (ri->set->ranges, Range, ri->idx);
      ri->pos = ri->current->first;
    }
  else
    {
      ri->pos++;
      if (ri->pos == ri->current->first + ri->current->n_items)
        goto next_range;
    }

  *item = ri->pos;
  return TRUE;
}

#if 0
void
gtk_set_dump (GtkSet *set)
{
  int i;

  for (i = 0; i < set->ranges->len; i++)
    {
      Range *r = &g_array_index (set->ranges, Range, i);
      g_print (" %u:%u", r->first, r->n_items);
    }
  g_print ("\n");
}
#endif
