/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

/**
 * SECTION:GskRenderNodeIter
 * @title: GskRenderNodeIter
 * @Short_desc: Iterator helper for render nodes
 *
 * TODO
 */

#include "config.h"

#include "gskrendernodeiter.h"
#include "gskrendernodeprivate.h"

typedef struct {
  GskRenderNode *root;
  GskRenderNode *current;
  gint64 age;
  gpointer reserved1;
  gpointer reserved2;
} RealIter;

#define REAL_ITER(iter)	((RealIter *) (iter))

/**
 * gsk_render_node_iter_new: (constructor)
 *
 * Allocates a new #GskRenderNodeIter.
 *
 * Returns: (transfer full): the newly allocated #GskRenderNodeIter
 *
 * Since: 3.22
 */
GskRenderNodeIter *
gsk_render_node_iter_new (void)
{
  return g_slice_new (GskRenderNodeIter);
}

/*< private >
 * gsk_render_node_iter_copy:
 * @src: a #GskRenderNodeIter
 *
 * Copies a #GskRenderNodeIter.
 *
 * Returns: (transfer full): a #GskRenderNodeIter
 */
static GskRenderNodeIter *
gsk_render_node_iter_copy (GskRenderNodeIter *src)
{
  return g_slice_dup (GskRenderNodeIter, src);
}

/**
 * gsk_render_node_iter_free:
 * @iter: a #GskRenderNodeIter
 *
 * Frees the resources allocated by gsk_render_node_iter_new().
 *
 * Since: 3.22
 */
void
gsk_render_node_iter_free (GskRenderNodeIter *iter)
{
  g_slice_free (GskRenderNodeIter, iter);
}

G_DEFINE_BOXED_TYPE (GskRenderNodeIter, gsk_render_node_iter,
		     gsk_render_node_iter_copy,
		     gsk_render_node_iter_free)

/**
 * gsk_render_node_iter_init:
 * @iter: a #GskRenderNodeIter
 * @node: a #GskRenderNode
 *
 * Initializes a #GskRenderNodeIter for iterating over the
 * children of @node.
 *
 * It's safe to call this function multiple times on the same
 * #GskRenderNodeIter instance.
 *
 * Since: 3.22
 */
void
gsk_render_node_iter_init (GskRenderNodeIter *iter,
                           GskRenderNode     *node)
{
  RealIter *riter = REAL_ITER (iter);

  g_return_if_fail (iter != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  riter->root = node;
  riter->age = node->age;
  riter->current = NULL;
}

/**
 * gsk_render_node_iter_is_valid:
 * @iter: a #GskRenderNodeIter
 *
 * Checks whether a #GskRenderNodeIter is associated to a #GskRenderNode,
 * or whether the associated node was modified while iterating.
 *
 * Returns: %TRUE if the iterator is still valid.
 *
 * Since: 3.22
 */
gboolean
gsk_render_node_iter_is_valid (GskRenderNodeIter *iter)
{
  RealIter *riter = REAL_ITER (iter);

  g_return_val_if_fail (iter != NULL, FALSE);

  if (riter->root == NULL)
    return FALSE;

  return riter->root->age == riter->age;
}

/**
 * gsk_render_node_iter_next:
 * @iter: a #GskRenderNodeIter
 * @child: (out) (transfer none): return location for a #GskRenderNode
 *
 * Advances the @iter and retrieves the next child of the root #GskRenderNode
 * used to initialize the #GskRenderNodeIter.
 *
 * If the iterator could advance, this function returns %TRUE and sets the
 * @child argument with the child #GskRenderNode.
 *
 * If the iterator could not advance, this function returns %FALSE and the
 * contents of the @child argument are undefined.
 *
 * Returns: %TRUE if the iterator could advance, and %FALSE otherwise
 *
 * Since: 3.22
 */
gboolean
gsk_render_node_iter_next (GskRenderNodeIter  *iter,
                           GskRenderNode     **child)
{
  RealIter *riter = REAL_ITER (iter);

  g_return_val_if_fail (riter != NULL, FALSE);
  g_return_val_if_fail (riter->root != NULL, FALSE);
  g_return_val_if_fail (riter->root->age == riter->age, FALSE);

  if (riter->current == NULL)
    riter->current = riter->root->first_child;
  else
    riter->current = riter->current->next_sibling;

  if (child != NULL)
    *child = riter->current;

  return riter->current != NULL;
}

/**
 * gsk_render_node_iter_prev:
 * @iter: a #GskRenderNodeIter
 * @child: (out) (transfer none): return location for a #GskRenderNode
 *
 * Advances the @iter and retrieves the previous child of the root
 * #GskRenderNode used to initialize the #GskRenderNodeIter.
 *
 * If the iterator could advance, this function returns %TRUE and sets the
 * @child argument with the child #GskRenderNode.
 *
 * If the iterator could not advance, this function returns %FALSE and the
 * contents of the @child argument are undefined.
 *
 * Returns: %TRUE if the iterator could advance, and %FALSE otherwise
 *
 * Since: 3.22
 */
gboolean
gsk_render_node_iter_prev (GskRenderNodeIter  *iter,
                           GskRenderNode     **child)
{
  RealIter *riter = REAL_ITER (iter);

  g_return_val_if_fail (riter != NULL, FALSE);
  g_return_val_if_fail (riter->root != NULL, FALSE);
  g_return_val_if_fail (riter->root->age == riter->age, FALSE);

  if (riter->current == NULL)
    riter->current = riter->root->last_child;
  else
    riter->current = riter->current->prev_sibling;

  if (child != NULL)
    *child = riter->current;

  return riter->current != NULL;
}

/**
 * gsk_render_node_iter_remove:
 * @iter: a #GskRenderNodeIter
 *
 * Removes the child #GskRenderNode currently being visited by
 * the iterator.
 *
 * Calling this function on an invalid #GskRenderNodeIter results
 * in undefined behavior.
 *
 * Since: 3.22
 */
void
gsk_render_node_iter_remove (GskRenderNodeIter *iter)
{
  RealIter *riter = REAL_ITER (iter);
  GskRenderNode *tmp;

  g_return_if_fail (riter != NULL);
  g_return_if_fail (riter->root != NULL);
  g_return_if_fail (riter->root->age == riter->age);
  g_return_if_fail (riter->current != NULL);

  tmp = riter->current;

  if (tmp != NULL)
    {
      riter->current = tmp->prev_sibling;

      gsk_render_node_remove_child (riter->root, tmp);

      riter->age += 1;

      /* Safety net */
      g_assert (riter->age == riter->root->age);
    }
}
