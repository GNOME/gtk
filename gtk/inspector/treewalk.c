/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "treewalk.h"

struct _GtkTreeWalk
{
  GtkTreeModel *model;
  GtkTreeIter position;
  gboolean visited;
  RowPredicate predicate;
  gpointer data;
  GDestroyNotify destroy;
};

GtkTreeWalk *
gtk_tree_walk_new (GtkTreeModel   *model,
                   RowPredicate    predicate,
                   gpointer        data,
                   GDestroyNotify  destroy)
{
  GtkTreeWalk *walk;

  walk = g_new (GtkTreeWalk, 1);
  walk->model = g_object_ref (model);
  walk->visited = FALSE;
  walk->predicate = predicate;
  walk->data = data;
  walk->destroy = destroy;

  return walk;
}

void
gtk_tree_walk_free (GtkTreeWalk *walk)
{
  g_object_unref (walk->model);

  if (walk->destroy)
    walk->destroy (walk->data);

  g_free (walk);
}

void
gtk_tree_walk_reset (GtkTreeWalk *walk,
                     GtkTreeIter *iter)
{
  if (iter)
    {
      walk->position = *iter;
      walk->visited = TRUE;
    }
  else
    {
      walk->visited = FALSE;
    }
}

static gboolean
gtk_tree_walk_step_forward (GtkTreeWalk *walk)
{
  GtkTreeIter next, up;

  if (!walk->visited)
    {
      if (!gtk_tree_model_get_iter_first (walk->model, &walk->position))
        return FALSE;

      walk->visited = TRUE;
      return TRUE;
    }

  if (gtk_tree_model_iter_children (walk->model, &next, &walk->position))
    {
      walk->position = next;
      return TRUE;
    }

  next = walk->position;
  do
    {
      up = next;
      if (gtk_tree_model_iter_next (walk->model, &next))
        {
          walk->position = next;
          return TRUE;
        }
    }
  while (gtk_tree_model_iter_parent (walk->model, &next, &up));

  return FALSE;
}

static gboolean
gtk_tree_model_iter_last_child (GtkTreeModel *model,
                                GtkTreeIter  *iter,
                                GtkTreeIter  *parent)
{
  GtkTreeIter next;

  if (!gtk_tree_model_iter_children (model, &next, parent))
    return FALSE;

  do 
    *iter = next;
  while (gtk_tree_model_iter_next (model, &next));

  return TRUE;
}

static gboolean
gtk_tree_model_get_iter_last (GtkTreeModel *model,
                              GtkTreeIter  *iter)
{
  GtkTreeIter next;

  if (!gtk_tree_model_iter_last_child (model, &next, NULL))
    return FALSE;

  do
    *iter = next;
  while (gtk_tree_model_iter_last_child (model, &next, &next));

  return TRUE;
}

static gboolean
gtk_tree_walk_step_back (GtkTreeWalk *walk)
{
  GtkTreeIter previous, down;

  if (!walk->visited)
    {
      if (!gtk_tree_model_get_iter_last (walk->model, &walk->position))
        return FALSE;

      walk->visited = TRUE;
      return TRUE;
    }

  previous = walk->position;
  if (gtk_tree_model_iter_previous (walk->model, &previous))
    {
      while (gtk_tree_model_iter_last_child (walk->model, &down, &previous))
        previous = down;

      walk->position = previous;
      return TRUE; 
    }

  if (gtk_tree_model_iter_parent (walk->model, &previous, &walk->position))
    {
      walk->position = previous;
      return TRUE; 
    }

  return FALSE;
}

static gboolean
gtk_tree_walk_step (GtkTreeWalk *walk, gboolean backwards)
{
  if (backwards)
    return gtk_tree_walk_step_back (walk);
  else
    return gtk_tree_walk_step_forward (walk);
}

static gboolean
row_is_match (GtkTreeWalk *walk)
{
  if (walk->predicate)
    return walk->predicate (walk->model, &walk->position, walk->data);
  return TRUE;
}

gboolean
gtk_tree_walk_next_match (GtkTreeWalk *walk,
                          gboolean     force_move,
                          gboolean     backwards,
                          GtkTreeIter *iter)
{
  gboolean moved = FALSE;
  gboolean was_visited;
  GtkTreeIter position;

  was_visited = walk->visited;
  position = walk->position;

  do
    {
      if (moved || (!force_move && walk->visited))
        {
          if (row_is_match (walk))
            {
              *iter = walk->position;
              return TRUE;
            }
        }
      moved = TRUE;
    }
  while (gtk_tree_walk_step (walk, backwards));

  walk->visited = was_visited;
  walk->position = position;

  return FALSE;
}

gboolean
gtk_tree_walk_get_position (GtkTreeWalk *walk,
                            GtkTreeIter *iter)
{
  *iter = walk->position;
  return walk->visited;
}
