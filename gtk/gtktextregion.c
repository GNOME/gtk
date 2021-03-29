/* gtktextregion.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtktextregionprivate.h"
#include "gtktextregionbtree.h"

#ifndef G_DISABLE_ASSERT
# define DEBUG_VALIDATE(a,b) G_STMT_START { if (a) gtk_text_region_node_validate(a,b); } G_STMT_END
#else
# define DEBUG_VALIDATE(a,b) G_STMT_START { } G_STMT_END
#endif

static inline void
gtk_text_region_invalid_cache (GtkTextRegion *region)
{
  region->cached_result = NULL;
  region->cached_result_offset = 0;
}

G_GNUC_UNUSED static void
gtk_text_region_node_validate (GtkTextRegionNode *node,
                               GtkTextRegionNode *parent)
{
  gsize length = 0;
  gsize length_in_parent = 0;

  g_assert (node != NULL);
  g_assert (UNTAG (node->tagged_parent) == parent);
  g_assert (gtk_text_region_node_is_leaf (node) ||
            UNTAG (node->tagged_parent) == node->tagged_parent);
  g_assert (!parent || !gtk_text_region_node_is_leaf (parent));
  g_assert (!parent || !SORTED_ARRAY_IS_EMPTY (&parent->branch.children));

  if (parent != NULL)
    {
      SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
        if (child->node == node)
          {
            length_in_parent = child->length;
            goto found;
          }
      });
      g_assert_not_reached ();
    }
found:

  if (parent != NULL)
    g_assert_cmpint (length_in_parent, ==, gtk_text_region_node_length (node));

  for (GtkTextRegionNode *iter = parent;
       iter != NULL;
       iter = gtk_text_region_node_get_parent (iter))
    g_assert_false (gtk_text_region_node_is_leaf (iter));

  if (gtk_text_region_node_is_leaf (node))
    {
      SORTED_ARRAY_FOREACH (&node->leaf.runs, GtkTextRegionRun, run, {
        g_assert_cmpint (run->length, >, 0);
        length += run->length;
      });

      if (node->leaf.prev != NULL)
        g_assert_true (gtk_text_region_node_is_leaf (node->leaf.prev));

      if (node->leaf.next != NULL)
        g_assert_true (gtk_text_region_node_is_leaf (node->leaf.next));
    }
  else
    {
      SORTED_ARRAY_FOREACH (&node->branch.children, GtkTextRegionChild, child, {
        GtkTextRegionChild *next = SORTED_ARRAY_FOREACH_PEEK (&node->branch.children);

        g_assert_nonnull (child->node);
        g_assert_cmpint (child->length, >, 0);
        g_assert_cmpint (child->length, ==, gtk_text_region_node_length (child->node));
        g_assert_true (gtk_text_region_node_get_parent (child->node) == node);

        length += child->length;

        if (next != NULL && next->node)
          {
            g_assert_cmpint (gtk_text_region_node_is_leaf (child->node), ==,
                             gtk_text_region_node_is_leaf (next->node));

            if (gtk_text_region_node_is_leaf (child->node))
              {
                g_assert_true (child->node->leaf.next == next->node);
                g_assert_true (child->node == next->node->leaf.prev);
              }
            else
              {
                g_assert_true (child->node->branch.next == next->node);
                g_assert_true (child->node == next->node->branch.prev);
              }
          }
      });
    }

  if (parent != NULL)
    g_assert_cmpint (length_in_parent, ==, length);
}

static void
gtk_text_region_split (GtkTextRegion          *region,
                       gsize                   offset,
                       const GtkTextRegionRun *run,
                       GtkTextRegionRun       *left,
                       GtkTextRegionRun       *right)
{
  if (region->split_func != NULL)
    region->split_func (offset, run, left, right);
}

static GtkTextRegionNode *
gtk_text_region_node_new (GtkTextRegionNode *parent,
                          gboolean           is_leaf)
{
  GtkTextRegionNode *node;

  g_assert (UNTAG (parent) == parent);

  node = g_new0 (GtkTextRegionNode, 1);
  node->tagged_parent = TAG (parent, is_leaf);

  if (is_leaf)
    {
      SORTED_ARRAY_INIT (&node->leaf.runs);
      node->leaf.prev = NULL;
      node->leaf.next = NULL;
    }
  else
    {
      SORTED_ARRAY_INIT (&node->branch.children);
    }

  g_assert (gtk_text_region_node_get_parent (node) == parent);

  return node;
}

static void
gtk_text_region_subtract_from_parents (GtkTextRegion     *region,
                                       GtkTextRegionNode *node,
                                       gsize              length)
{
  GtkTextRegionNode *parent = gtk_text_region_node_get_parent (node);

  if (parent == NULL || length == 0)
    return;

  gtk_text_region_invalid_cache (region);

  SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
    if (child->node == node)
      {
        g_assert (length <= child->length);
        child->length -= length;
        gtk_text_region_subtract_from_parents (region, parent, length);
        return;
      }
  });

  g_assert_not_reached ();
}

static void
gtk_text_region_add_to_parents (GtkTextRegion     *region,
                                GtkTextRegionNode *node,
                                gsize              length)
{
  GtkTextRegionNode *parent = gtk_text_region_node_get_parent (node);

  if (parent == NULL || length == 0)
    return;

  gtk_text_region_invalid_cache (region);

  SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
    if (child->node == node)
      {
        child->length += length;
        gtk_text_region_add_to_parents (region, parent, length);
        return;
      }
  });

  g_assert_not_reached ();
}

static inline gboolean
gtk_text_region_node_is_root (GtkTextRegionNode *node)
{
  return node != NULL && gtk_text_region_node_get_parent (node) == NULL;
}

static GtkTextRegionNode *
gtk_text_region_node_search_recurse (GtkTextRegionNode *node,
                                     gsize              offset,
                                     gsize             *offset_within_node)
{
  GtkTextRegionChild *last_child = NULL;

  g_assert (node != NULL);
  g_assert (offset_within_node != NULL);

  /* If we reached a leaf, that is all we need to do */
  if (gtk_text_region_node_is_leaf (node))
    {
      *offset_within_node = offset;
      return node;
    }

  g_assert (!gtk_text_region_node_is_leaf (node));
  g_assert (!SORTED_ARRAY_IS_EMPTY (&node->branch.children));
  g_assert (offset <= gtk_text_region_node_length (node));

  SORTED_ARRAY_FOREACH (&node->branch.children, GtkTextRegionChild, child, {
    g_assert (child->length > 0);
    g_assert (child->node != NULL);

    if (offset < child->length)
      return gtk_text_region_node_search_recurse (child->node, offset, offset_within_node);

    offset -= child->length;
    last_child = child;
  });

  /* We're right-most, so it belongs at the end. Add back the length we removed
   * while trying to resolve within the parent branch.
   */
  g_assert (last_child != NULL);
  g_assert (node->branch.next == NULL);
  return gtk_text_region_node_search_recurse (last_child->node,
                                              offset + last_child->length,
                                              offset_within_node);
}

static GtkTextRegionNode *
gtk_text_region_search (GtkTextRegion *region,
                        gsize          offset,
                        gsize         *offset_within_node)
{
  GtkTextRegionNode *result;

  *offset_within_node = 0;

  g_assert (region->cached_result == NULL ||
            gtk_text_region_node_is_leaf (region->cached_result));

  /* Try to reuse cached node to avoid traversal since in most cases
   * an insert will be followed by another insert.
   */
  if (region->cached_result != NULL && offset >= region->cached_result_offset)
    {
      gsize calc_offset = region->cached_result_offset + gtk_text_region_node_length (region->cached_result);

      if (offset < calc_offset ||
          (offset == calc_offset && region->cached_result->leaf.next == NULL))
        {
          *offset_within_node = offset - region->cached_result_offset;
          return region->cached_result;
        }
    }

  if (offset == 0)
    result = _gtk_text_region_get_first_leaf (region);
  else
    result = gtk_text_region_node_search_recurse (&region->root, offset, offset_within_node);

  /* Now save it for cached reuse */
  if (result != NULL)
    {
      region->cached_result = result;
      region->cached_result_offset = offset - *offset_within_node;
    }

  return result;
}

static void
gtk_text_region_root_split (GtkTextRegion     *region,
                            GtkTextRegionNode *root)
{
  GtkTextRegionNode *left;
  GtkTextRegionNode *right;
  GtkTextRegionChild new_child;

  g_assert (region != NULL);
  g_assert (!gtk_text_region_node_is_leaf (root));
  g_assert (gtk_text_region_node_is_root (root));
  g_assert (!SORTED_ARRAY_IS_EMPTY (&root->branch.children));

  left = gtk_text_region_node_new (root, FALSE);
  right = gtk_text_region_node_new (root, FALSE);

  left->branch.next = right;
  right->branch.prev = left;

  SORTED_ARRAY_SPLIT2 (&root->branch.children, &left->branch.children, &right->branch.children);
  SORTED_ARRAY_FOREACH (&left->branch.children, GtkTextRegionChild, child, {
    gtk_text_region_node_set_parent (child->node, left);
  });
  SORTED_ARRAY_FOREACH (&right->branch.children, GtkTextRegionChild, child, {
    gtk_text_region_node_set_parent (child->node, right);
  });

  g_assert (SORTED_ARRAY_IS_EMPTY (&root->branch.children));

  new_child.node = right;
  new_child.length = gtk_text_region_node_length (right);
  SORTED_ARRAY_PUSH_HEAD (&root->branch.children, new_child);

  new_child.node = left;
  new_child.length = gtk_text_region_node_length (left);
  SORTED_ARRAY_PUSH_HEAD (&root->branch.children, new_child);

  g_assert (SORTED_ARRAY_LENGTH (&root->branch.children) == 2);

  DEBUG_VALIDATE (root, NULL);
  DEBUG_VALIDATE (left, root);
  DEBUG_VALIDATE (right, root);
}

static GtkTextRegionNode *
gtk_text_region_branch_split (GtkTextRegion     *region,
                              GtkTextRegionNode *left)
{
  G_GNUC_UNUSED gsize old_length;
  GtkTextRegionNode *parent;
  GtkTextRegionNode *right;
  gsize right_length = 0;
  gsize left_length = 0;
  guint i = 0;

  g_assert (region != NULL);
  g_assert (left != NULL);
  g_assert (!gtk_text_region_node_is_leaf (left));
  g_assert (!gtk_text_region_node_is_root (left));

  old_length = gtk_text_region_node_length (left);

  /*
   * This operation should not change the height of the tree. Only
   * splitting the root node can change the height of the tree. So
   * here we add a new right node, and update the parent to point to
   * it right after our node.
   *
   * Since no new items are added, lengths do not change and we do
   * not need to update lengths up the hierarchy except for our two
   * effected nodes (and their direct parent).
   */

  parent = gtk_text_region_node_get_parent (left);

  /* Create a new node to split half the items into */
  right = gtk_text_region_node_new (parent, FALSE);

  /* Insert node into branches linked list */
  right->branch.next = left->branch.next;
  right->branch.prev = left;
  if (right->branch.next != NULL)
    right->branch.next->branch.prev = right;
  left->branch.next = right;

  SORTED_ARRAY_SPLIT (&left->branch.children, &right->branch.children);
  SORTED_ARRAY_FOREACH (&right->branch.children, GtkTextRegionChild, child, {
    gtk_text_region_node_set_parent (child->node, right);
  });

#ifndef G_DISABLE_ASSERT
  SORTED_ARRAY_FOREACH (&left->branch.children, GtkTextRegionChild, child, {
    g_assert (gtk_text_region_node_get_parent (child->node) == left);
  });
#endif

  right_length = gtk_text_region_node_length (right);
  left_length = gtk_text_region_node_length (left);

  g_assert (right_length + left_length == old_length);
  g_assert (SORTED_ARRAY_LENGTH (&parent->branch.children) < SORTED_ARRAY_CAPACITY (&parent->branch.children));

  SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
    i++;

    if (child->node == left)
      {
        GtkTextRegionChild right_child;

        right_child.node = right;
        right_child.length = right_length;

        child->length = left_length;

        SORTED_ARRAY_INSERT_VAL (&parent->branch.children, i, right_child);

        DEBUG_VALIDATE (left, parent);
        DEBUG_VALIDATE (right, parent);
        DEBUG_VALIDATE (parent, gtk_text_region_node_get_parent (parent));

        return right;
      }
  });

  g_assert_not_reached ();
}

static GtkTextRegionNode *
gtk_text_region_leaf_split (GtkTextRegion     *region,
                            GtkTextRegionNode *left)
{
  G_GNUC_UNUSED gsize length;
  GtkTextRegionNode *parent;
  GtkTextRegionNode *right;
  gsize right_length;
  guint i;

  g_assert (region != NULL);
  g_assert (left != NULL);
  g_assert (gtk_text_region_node_is_leaf (left));

  parent = gtk_text_region_node_get_parent (left);

  g_assert (parent != left);
  g_assert (!gtk_text_region_node_is_leaf (parent));
  g_assert (!SORTED_ARRAY_IS_EMPTY (&parent->branch.children));
  g_assert (!SORTED_ARRAY_IS_FULL (&parent->branch.children));

  length = gtk_text_region_node_length (left);

  g_assert (length > 0);

  DEBUG_VALIDATE (parent, gtk_text_region_node_get_parent (parent));
  DEBUG_VALIDATE (left, parent);

  right = gtk_text_region_node_new (parent, TRUE);

  SORTED_ARRAY_SPLIT (&left->leaf.runs, &right->leaf.runs);
  right_length = gtk_text_region_node_length (right);

  g_assert (length == right_length + gtk_text_region_node_length (left));
  g_assert (gtk_text_region_node_is_leaf (left));
  g_assert (gtk_text_region_node_is_leaf (right));

  i = 0;
  SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
    G_GNUC_UNUSED const GtkTextRegionChild *next = SORTED_ARRAY_FOREACH_PEEK (&parent->branch.children);

    ++i;

    g_assert (gtk_text_region_node_is_leaf (child->node));
    g_assert (next == NULL || gtk_text_region_node_is_leaf (next->node));

    if (child->node == left)
      {
        GtkTextRegionChild right_child;

        g_assert (child->length >= right_length);
        g_assert (next == NULL || left->leaf.next == next->node);

        if (left->leaf.next != NULL)
          left->leaf.next->leaf.prev = right;

        right->leaf.prev = left;
        right->leaf.next = left->leaf.next;
        left->leaf.next = right;

        right_child.node = right;
        right_child.length = right_length;

        child->length -= right_length;

        g_assert (child->length > 0);
        g_assert (right_child.length > 0);

        SORTED_ARRAY_INSERT_VAL (&parent->branch.children, i, right_child);

        g_assert (right != NULL);
        g_assert (gtk_text_region_node_is_leaf (right));
        g_assert (right->leaf.prev == left);
        g_assert (left->leaf.next == right);

        DEBUG_VALIDATE (left, parent);
        DEBUG_VALIDATE (right, parent);
        DEBUG_VALIDATE (parent, gtk_text_region_node_get_parent (parent));

        return right;
      }
  });

  g_assert_not_reached ();
}

static inline gboolean
gtk_text_region_node_needs_split (GtkTextRegionNode *node)
{
  /*
   * We want to split the tree node if there is not enough space to
   * split a single entry into two AND add a new entry. That means we
   * need two empty slots before we ever perform an insert.
   */

  if (!gtk_text_region_node_is_leaf (node))
    return SORTED_ARRAY_LENGTH (&node->branch.children) >= (SORTED_ARRAY_CAPACITY (&node->branch.children) - 2);
  else
    return SORTED_ARRAY_LENGTH (&node->leaf.runs) >= (SORTED_ARRAY_CAPACITY (&node->leaf.runs) - 2);
}

static inline GtkTextRegionNode *
gtk_text_region_node_split (GtkTextRegion     *region,
                            GtkTextRegionNode *node)
{
  GtkTextRegionNode *parent;

  g_assert (node != NULL);

  gtk_text_region_invalid_cache (region);

  parent = gtk_text_region_node_get_parent (node);

  if (parent != NULL &&
      gtk_text_region_node_needs_split (parent))
    gtk_text_region_node_split (region, parent);

  if (!gtk_text_region_node_is_leaf (node))
    {
      if (gtk_text_region_node_is_root (node))
        {
          gtk_text_region_root_split (region, node);
          return &region->root;
        }

      return gtk_text_region_branch_split (region, node);
    }
  else
    {
      return gtk_text_region_leaf_split (region, node);
    }
}

GtkTextRegion *
_gtk_text_region_new (GtkTextRegionJoinFunc  join_func,
                      GtkTextRegionSplitFunc split_func)
{
  GtkTextRegion *self;
  GtkTextRegionNode *leaf;
  GtkTextRegionChild child;

  self = g_new0 (GtkTextRegion, 1);
  self->length = 0;
  self->join_func = join_func;
  self->split_func = split_func;

  /* The B+Tree has a root node (a branch) and a single leaf
   * as a child to simplify how we do splits/rotations/etc.
   */
  leaf = gtk_text_region_node_new (&self->root, TRUE);

  child.node = leaf;
  child.length = 0;

  SORTED_ARRAY_INIT (&self->root.branch.children);
  SORTED_ARRAY_PUSH_HEAD (&self->root.branch.children, child);

  return self;
}

static void
gtk_text_region_node_free (GtkTextRegionNode *node)
{
  if (node == NULL)
    return;

  if (!gtk_text_region_node_is_leaf (node))
    {
      SORTED_ARRAY_FOREACH (&node->branch.children, GtkTextRegionChild, child, {
        gtk_text_region_node_free (child->node);
      });
    }

  g_free (node);
}

void
_gtk_text_region_free (GtkTextRegion *region)
{
  if (region != NULL)
    {
      g_assert (gtk_text_region_node_is_root (&region->root));
      g_assert (!SORTED_ARRAY_IS_EMPTY (&region->root.branch.children));

      SORTED_ARRAY_FOREACH (&region->root.branch.children, GtkTextRegionChild, child, {
        gtk_text_region_node_free (child->node);
      });

      g_free (region);
    }
}

static inline gboolean
join_run (GtkTextRegion          *region,
          gsize                   offset,
          const GtkTextRegionRun *left,
          const GtkTextRegionRun *right,
          GtkTextRegionRun       *joined)
{
  gboolean join;

  g_assert (region != NULL);
  g_assert (left != NULL);
  g_assert (right != NULL);
  g_assert (joined != NULL);

  if (region->join_func != NULL)
    join = region->join_func (offset, left, right);
  else
    join = FALSE;

  if (join)
    {
      joined->length = left->length + right->length;
      joined->data = left->data;

      return TRUE;
    }

  return FALSE;
}

void
_gtk_text_region_insert (GtkTextRegion *region,
                         gsize          offset,
                         gsize          length,
                         gpointer       data)
{
  GtkTextRegionRun to_insert = { length, data };
  GtkTextRegionNode *target;
  GtkTextRegionNode *node;
  GtkTextRegionNode *parent;
  gsize offset_within_node = offset;
  guint i;

  g_assert (region != NULL);
  g_assert (offset <= region->length);

  if (length == 0)
    return;

  target = gtk_text_region_search (region, offset, &offset_within_node);

  g_assert (gtk_text_region_node_is_leaf (target));
  g_assert (offset_within_node <= gtk_text_region_node_length (target));

  /* We should only hit this if we have an empty tree. */
  if G_UNLIKELY (SORTED_ARRAY_IS_EMPTY (&target->leaf.runs))
    {
      g_assert (offset == 0);
      SORTED_ARRAY_PUSH_HEAD (&target->leaf.runs, to_insert);
      g_assert (gtk_text_region_node_length (target) == length);
      goto inserted;
    }

  /* Split up to region->root if necessary */
  if (gtk_text_region_node_needs_split (target))
    {
      DEBUG_VALIDATE (target, gtk_text_region_node_get_parent (target));

      /* Split the target into two and then re-locate our position as
       * we might need to be in another node.
       *
       * TODO: Potentially optimization here to look at prev/next to
       *       locate which we need. Complicated though since we don't
       *       have real offsets.
       */
      gtk_text_region_node_split (region, target);

      target = gtk_text_region_search (region, offset, &offset_within_node);

      g_assert (gtk_text_region_node_is_leaf (target));
      g_assert (offset_within_node <= gtk_text_region_node_length (target));
      DEBUG_VALIDATE (target, gtk_text_region_node_get_parent (target));
    }

  i = 0;
  SORTED_ARRAY_FOREACH (&target->leaf.runs, GtkTextRegionRun, run, {
    /*
     * If this insert request would happen immediately after this run,
     * we want to see if we can chain it to this run or the beginning
     * of the next run.
     *
     * Note: We coudld also follow the the B+tree style linked-leaf to
     *       the next leaf and compare against it's first item. But that is
     *       out of scope for this prototype.
     */

    if (offset_within_node == 0)
      {
        if (!join_run (region, offset, &to_insert, run, run))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i, to_insert);
        goto inserted;
      }
    else if (offset_within_node == run->length)
      {
        GtkTextRegionRun *next = SORTED_ARRAY_FOREACH_PEEK (&target->leaf.runs);

        /* Try to chain to the end of this run or the beginning of the next */
        if (!join_run (region, offset, run, &to_insert, run) &&
            (next == NULL || !join_run (region, offset, &to_insert, next, next)))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i + 1, to_insert);
        goto inserted;
      }
    else if (offset_within_node < run->length)
      {
        GtkTextRegionRun left;
        GtkTextRegionRun right;

        left.length = offset_within_node;
        left.data = run->data;

        right.length = run->length - offset_within_node;
        right.data = run->data;

        gtk_text_region_split (region, offset - offset_within_node, run, &left, &right);

        *run = left;

        if (!join_run (region, offset, &to_insert, &right, &to_insert))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i + 1, right);

        if (!join_run (region, offset - offset_within_node, run, &to_insert, run))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i + 1, to_insert);

        goto inserted;
      }

    offset_within_node -= run->length;

    i++;
  });

  g_assert_not_reached ();

inserted:

  g_assert (target != NULL);

  /*
   * Now update each of the parent nodes in the tree so that they have
   * an apprporiate length along with the child pointer. This allows them
   * to calculate offsets while walking the tree (without derefrencing the
   * child node) at the cost of us walking back up the tree.
   */
  for (parent = gtk_text_region_node_get_parent (target), node = target;
       parent != NULL;
       node = parent, parent = gtk_text_region_node_get_parent (node))
    {
      SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
        if (child->node == node)
          {
            child->length += length;
            goto found_in_parent;
          }
      });

      g_assert_not_reached ();

    found_in_parent:
      DEBUG_VALIDATE (node, parent);
      continue;
    }

  region->length += length;

  g_assert (region->length == gtk_text_region_node_length (&region->root));
}

void
_gtk_text_region_replace (GtkTextRegion *region,
                          gsize          offset,
                          gsize          length,
                          gpointer       data)
{
  g_assert (region != NULL);

  if (length == 0)
    return;

  /* TODO: This could be optimized to avoid possible splits
   *       by merging adjoining runs.
   */

  _gtk_text_region_remove (region, offset, length);
  _gtk_text_region_insert (region, offset, length, data);

  g_assert (region->length == gtk_text_region_node_length (&region->root));
}

guint
_gtk_text_region_get_length (GtkTextRegion *region)
{
  g_assert (region != NULL);

  return region->length;
}

static void
gtk_text_region_branch_compact (GtkTextRegion     *region,
                                GtkTextRegionNode *node)
{
  GtkTextRegionNode *parent;
  GtkTextRegionNode *left;
  GtkTextRegionNode *right;
  GtkTextRegionNode *target;
  gsize added = 0;
  gsize length;

  g_assert (region != NULL);
  g_assert (node != NULL);
  g_assert (!gtk_text_region_node_is_leaf (node));

  SORTED_ARRAY_FOREACH (&node->branch.children, GtkTextRegionChild, child, {
    if (child->node == NULL)
      {
        g_assert (child->length == 0);
        SORTED_ARRAY_FOREACH_REMOVE (&node->branch.children);
      }
  });

  if (gtk_text_region_node_is_root (node))
    return;

  parent = gtk_text_region_node_get_parent (node);

  g_assert (parent != NULL);
  g_assert (!gtk_text_region_node_is_leaf (parent));

  /* Reparent child in our stead if we can remove this node */
  if (SORTED_ARRAY_LENGTH (&node->branch.children) == 1 &&
      SORTED_ARRAY_LENGTH (&parent->branch.children) == 1)
    {
      GtkTextRegionChild *descendant = &SORTED_ARRAY_PEEK_HEAD (&node->branch.children);

      g_assert (parent->branch.prev == NULL);
      g_assert (parent->branch.next == NULL);
      g_assert (node->branch.prev == NULL);
      g_assert (node->branch.next == NULL);
      g_assert (descendant->node != NULL);

      SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
        if (child->node == node)
          {
            child->node = descendant->node;
            gtk_text_region_node_set_parent (child->node, parent);

            descendant->node = NULL;
            descendant->length = 0;

            goto compact_parent;
          }
      });

      g_assert_not_reached ();
    }

  if (node->branch.prev == NULL && node->branch.next == NULL)
    return;

  if (SORTED_ARRAY_LENGTH (&node->branch.children) >= GTK_TEXT_REGION_MIN_BRANCHES)
    return;

  length = gtk_text_region_node_length (node);
  gtk_text_region_subtract_from_parents (region, node, length);

  /* Remove this node, we'll reparent the children with edges */
  SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
    if (child->node == node)
      {
        SORTED_ARRAY_FOREACH_REMOVE (&parent->branch.children);
        goto found;
      }
  });

  g_assert_not_reached ();

found:
  left = node->branch.prev;
  right = node->branch.next;

  if (left != NULL)
    left->branch.next = right;

  if (right != NULL)
    right->branch.prev = left;

  if (left == NULL ||
      (right != NULL &&
       SORTED_ARRAY_LENGTH (&left->branch.children) > SORTED_ARRAY_LENGTH (&right->branch.children)))
    {
      target = right;

      g_assert (target->branch.prev == left);

      SORTED_ARRAY_FOREACH_REVERSE (&node->branch.children, GtkTextRegionChild, child, {
        if (SORTED_ARRAY_LENGTH (&target->branch.children) >= GTK_TEXT_REGION_MAX_BRANCHES-1)
          {
            gtk_text_region_add_to_parents (region, target, added);
            added = 0;
            gtk_text_region_branch_split (region, target);
            g_assert (target->branch.prev == left);
          }

        gtk_text_region_node_set_parent (child->node, target);
        added += child->length;
        SORTED_ARRAY_PUSH_HEAD (&target->branch.children, *child);

        child->node = NULL;
        child->length = 0;
      });

      gtk_text_region_add_to_parents (region, target, added);
    }
  else
    {
      target = left;

      g_assert (target->branch.next == right);

      SORTED_ARRAY_FOREACH (&node->branch.children, GtkTextRegionChild, child, {
        if (SORTED_ARRAY_LENGTH (&target->branch.children) >= GTK_TEXT_REGION_MAX_BRANCHES-1)
          {
            gtk_text_region_add_to_parents (region, target, added);
            added = 0;
            target = gtk_text_region_branch_split (region, target);
          }

        gtk_text_region_node_set_parent (child->node, target);
        added += child->length;
        SORTED_ARRAY_PUSH_TAIL (&target->branch.children, *child);

        child->node = NULL;
        child->length = 0;
      });

      gtk_text_region_add_to_parents (region, target, added);
    }

  DEBUG_VALIDATE (left, gtk_text_region_node_get_parent (left));
  DEBUG_VALIDATE (right, gtk_text_region_node_get_parent (right));
  DEBUG_VALIDATE (parent, gtk_text_region_node_get_parent (parent));

compact_parent:
  if (parent != NULL)
    gtk_text_region_branch_compact (region, parent);

  gtk_text_region_node_free (node);
}

static void
gtk_text_region_leaf_compact (GtkTextRegion     *region,
                              GtkTextRegionNode *node)
{
  GtkTextRegionNode *parent;
  GtkTextRegionNode *target;
  GtkTextRegionNode *left;
  GtkTextRegionNode *right;
  gsize added = 0;

  g_assert (region != NULL);
  g_assert (node != NULL);
  g_assert (gtk_text_region_node_is_leaf (node));
  g_assert (SORTED_ARRAY_LENGTH (&node->leaf.runs) < GTK_TEXT_REGION_MIN_RUNS);

  /* Short-circuit if we are the only node */
  if (node->leaf.prev == NULL && node->leaf.next == NULL)
    return;

  parent = gtk_text_region_node_get_parent (node);
  left = node->leaf.prev;
  right = node->leaf.next;

  g_assert (parent != NULL);
  g_assert (!gtk_text_region_node_is_leaf (parent));
  g_assert (left == NULL || gtk_text_region_node_is_leaf (left));
  g_assert (right == NULL || gtk_text_region_node_is_leaf (right));

  SORTED_ARRAY_FOREACH (&parent->branch.children, GtkTextRegionChild, child, {
    if (child->node == node)
      {
        gtk_text_region_subtract_from_parents (region, node, child->length);
        g_assert (child->length == 0);
        SORTED_ARRAY_FOREACH_REMOVE (&parent->branch.children);
        goto found;
      }
  });

  g_assert_not_reached ();

found:
  if (left != NULL)
    left->leaf.next = right;

  if (right != NULL)
    right->leaf.prev = left;

  node->leaf.next = NULL;
  node->leaf.prev = NULL;

  if (left == NULL ||
      (right != NULL &&
       SORTED_ARRAY_LENGTH (&left->leaf.runs) > SORTED_ARRAY_LENGTH (&right->leaf.runs)))
    {
      target = right;

      g_assert (target->leaf.prev == left);

      SORTED_ARRAY_FOREACH_REVERSE (&node->leaf.runs, GtkTextRegionRun, run, {
        if (SORTED_ARRAY_LENGTH (&target->leaf.runs) >= GTK_TEXT_REGION_MAX_RUNS-1)
          {
            gtk_text_region_add_to_parents (region, target, added);
            added = 0;
            gtk_text_region_node_split (region, target);
            g_assert (target->leaf.prev == left);
          }

        added += run->length;
        SORTED_ARRAY_PUSH_HEAD (&target->leaf.runs, *run);
      });

      gtk_text_region_add_to_parents (region, target, added);
    }
  else
    {
      target = left;

      g_assert (target->leaf.next == right);

      SORTED_ARRAY_FOREACH (&node->leaf.runs, GtkTextRegionRun, run, {
        if (SORTED_ARRAY_LENGTH (&target->leaf.runs) >= GTK_TEXT_REGION_MAX_RUNS-1)
          {
            gtk_text_region_add_to_parents (region, target, added);
            added = 0;

            target = gtk_text_region_node_split (region, target);

            left = target;
          }

        added += run->length;
        SORTED_ARRAY_PUSH_TAIL (&target->leaf.runs, *run);
      });

      gtk_text_region_add_to_parents (region, target, added);
    }

  DEBUG_VALIDATE (left, gtk_text_region_node_get_parent (left));
  DEBUG_VALIDATE (right, gtk_text_region_node_get_parent (right));
  DEBUG_VALIDATE (parent, gtk_text_region_node_get_parent (parent));

  gtk_text_region_branch_compact (region, parent);

  gtk_text_region_node_free (node);
}

void
_gtk_text_region_remove (GtkTextRegion *region,
                         gsize          offset,
                         gsize          length)
{
  GtkTextRegionNode *target;
  gsize offset_within_node;
  gsize to_remove = length;
  gsize calc_offset;
  guint i;

  g_assert (region != NULL);
  g_assert (length <= region->length);
  g_assert (offset < region->length);
  g_assert (length <= region->length - offset);

  if (length == 0)
    return;

  target = gtk_text_region_search (region, offset, &offset_within_node);

  g_assert (target != NULL);
  g_assert (gtk_text_region_node_is_leaf (target));
  g_assert (SORTED_ARRAY_LENGTH (&target->leaf.runs) > 0);
  g_assert (offset >= offset_within_node);

  calc_offset = offset - offset_within_node;

  i = 0;
  SORTED_ARRAY_FOREACH (&target->leaf.runs, GtkTextRegionRun, run, {
    ++i;

    g_assert (to_remove > 0);

    if (offset_within_node >= run->length)
      {
        offset_within_node -= run->length;
        calc_offset += run->length;
      }
    else if (offset_within_node > 0 && to_remove >= run->length - offset_within_node)
      {
        GtkTextRegionRun left;
        GtkTextRegionRun right;

        left.length = offset_within_node;
        left.data = run->data;
        right.length = run->length - left.length;
        right.data = run->data;
        gtk_text_region_split (region, calc_offset, run, &left, &right);

        to_remove -= right.length;
        calc_offset += left.length;
        offset_within_node = 0;

        *run = left;

        if (to_remove == 0)
          break;
      }
    else if (offset_within_node > 0 && to_remove < run->length - offset_within_node)
      {
        GtkTextRegionRun left;
        GtkTextRegionRun right;
        GtkTextRegionRun right2;
        GtkTextRegionRun center;

        left.length = offset_within_node;
        left.data = run->data;
        right.length = run->length - left.length;
        right.data = run->data;
        gtk_text_region_split (region, calc_offset, run, &left, &right);

        center.length = to_remove;
        center.data = run->data;
        right2.length = run->length - offset_within_node - to_remove;
        right2.data = run->data;
        gtk_text_region_split (region, calc_offset + left.length, &right, &center, &right2);

        *run = left;

        if (!join_run (region, calc_offset, run, &right2, run))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i, right2);

        offset_within_node = 0;
        to_remove = 0;

        break;
      }
    else if (offset_within_node == 0 && to_remove < run->length)
      {
        GtkTextRegionRun left;
        GtkTextRegionRun right;

        left.length = to_remove;
        left.data = run->data;

        right.length = run->length - to_remove;
        right.data = run->data;

        gtk_text_region_split (region, calc_offset, run, &left, &right);

        to_remove = 0;
        offset_within_node = 0;

        *run = right;

        break;
      }
    else if (offset_within_node == 0 && to_remove >= run->length)
      {
        to_remove -= run->length;

        SORTED_ARRAY_FOREACH_REMOVE (&target->leaf.runs);

        if (to_remove == 0)
          break;
      }
    else
      {
        g_assert_not_reached ();
      }

    g_assert (to_remove > 0);
  });

  region->length -= length - to_remove;
  gtk_text_region_subtract_from_parents (region, target, length - to_remove);

  if (SORTED_ARRAY_LENGTH (&target->leaf.runs) < GTK_TEXT_REGION_MIN_RUNS)
    gtk_text_region_leaf_compact (region, target);

  g_assert (region->length == gtk_text_region_node_length (&region->root));

  if (to_remove > 0)
    _gtk_text_region_remove (region, offset, to_remove);
}

void
_gtk_text_region_foreach (GtkTextRegion            *region,
                          GtkTextRegionForeachFunc  func,
                          gpointer                  user_data)
{
  GtkTextRegionNode *leaf;
  gsize offset = 0;

  g_return_if_fail (region != NULL);
  g_return_if_fail (func != NULL);

  for (leaf = _gtk_text_region_get_first_leaf (region);
       leaf != NULL;
       leaf = leaf->leaf.next)
    {
      g_assert (leaf->leaf.next == NULL || leaf->leaf.next->leaf.prev == leaf);

      SORTED_ARRAY_FOREACH (&leaf->leaf.runs, GtkTextRegionRun, run, {
        func (offset, run, user_data);
        offset += run->length;
      });
    }
}

void
_gtk_text_region_foreach_in_range (GtkTextRegion            *region,
                                   gsize                     begin,
                                   gsize                     end,
                                   GtkTextRegionForeachFunc  func,
                                   gpointer                  user_data)
{
  GtkTextRegionNode *leaf;
  gsize position;
  gsize offset_within_node = 0;

  g_return_if_fail (region != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (begin <= region->length);
  g_return_if_fail (end <= region->length);
  g_return_if_fail (begin <= end);

  if (begin == end || begin == region->length)
    return;

  if (begin == 0)
    leaf = _gtk_text_region_get_first_leaf (region);
  else
    leaf = gtk_text_region_search (region, begin, &offset_within_node);

  g_assert (offset_within_node < gtk_text_region_node_length (leaf));

  position = begin - offset_within_node;

  while (position < end)
    {
      SORTED_ARRAY_FOREACH (&leaf->leaf.runs, GtkTextRegionRun, run, {
        if (offset_within_node >= run->length)
          {
            offset_within_node -= run->length;
          }
        else
          {
            offset_within_node = 0;
            func (position, run, user_data);
          }

        position += run->length;

        if (position >= end)
          break;
      });

      leaf = leaf->leaf.next;
    }
}
