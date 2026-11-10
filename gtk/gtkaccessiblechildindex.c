/* gtkaccessiblechildindex.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkaccessiblechildindexprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkrbtreeprivate.h"
#include "gtkwidgetprivate.h"

typedef struct
{
  GtkWidget *widget;
  guint      present;
} AccessibleChildNode;

typedef struct
{
  guint present;
} AccessibleChildAugment;

struct _GtkAccessibleChildIndex
{
  GtkRbTree  *tree;
  GHashTable *nodes;
};

static void
accessible_child_augment (GtkRbTree *tree,
                          gpointer   augment,
                          gpointer   node,
                          gpointer   left,
                          gpointer   right)
{
  AccessibleChildAugment *result = augment;

  result->present = ((AccessibleChildNode *) node)->present;

  if (left)
    result->present += ((AccessibleChildAugment *) gtk_rb_tree_get_augment (tree, left))->present;

  if (right)
    result->present += ((AccessibleChildAugment *) gtk_rb_tree_get_augment (tree, right))->present;
}

static void
accessible_child_index_free (GtkAccessibleChildIndex *index)
{
  g_hash_table_unref (index->nodes);
  gtk_rb_tree_unref (index->tree);
  g_free (index);
}

void
gtk_widget_invalidate_accessible_child_index (GtkWidget *parent)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (parent));

  priv = parent->priv;
  priv->accessible_child_index_version++;
  priv->accessible_child_index_disabled = FALSE;

  g_clear_pointer (&priv->accessible_child_index, accessible_child_index_free);
}

static gboolean
accessible_child_uses_widget_hierarchy (GtkWidget *child)
{
  GtkAccessibleInterface *iface = GTK_ACCESSIBLE_GET_IFACE (child);
  GtkATContext *context;
  gboolean uses_widget_hierarchy;

  if (iface->get_accessible_parent != gtk_widget_accessible_get_accessible_parent ||
      iface->get_next_accessible_sibling != gtk_widget_accessible_get_next_accessible_sibling)
    return FALSE;

  if (!(context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child))))
    return TRUE;

  uses_widget_hierarchy = context->accessible_parent == NULL &&
                          context->next_accessible_sibling == NULL;

  g_object_unref (context);

  return uses_widget_hierarchy;
}

int
gtk_widget_get_accessible_child_index (GtkWidget *parent,
                                       GtkWidget *child)
{
  GtkWidgetPrivate *priv;
  GtkAccessibleChildIndex *index;
  AccessibleChildNode *node;
  GtkAccessibleInterface *iface;
  guint version;
  guint position;
  gpointer ancestor;
  int result = -1;

  g_return_val_if_fail (GTK_IS_WIDGET (parent), -1);
  g_return_val_if_fail (GTK_IS_WIDGET (child), -1);

  priv = parent->priv;
  iface = GTK_ACCESSIBLE_GET_IFACE (parent);

  if (child->priv->parent != parent ||
      iface->get_first_accessible_child != gtk_widget_accessible_get_first_accessible_child ||
      priv->accessible_child_index_busy ||
      priv->accessible_child_index_disabled)
    return -1;

  /* Context getters can reenter even when the index is already built. */
  priv->accessible_child_index_busy = TRUE;
  version = priv->accessible_child_index_version;

  if (!accessible_child_uses_widget_hierarchy (child) ||
      version != priv->accessible_child_index_version)
    goto out;

  index = priv->accessible_child_index;

  if (index == NULL)
    {
      GtkWidget *candidate;
      gboolean incompatible = FALSE;

      index = g_new0 (GtkAccessibleChildIndex, 1);
      index->nodes = g_hash_table_new (g_direct_hash, g_direct_equal);
      index->tree = gtk_rb_tree_new (AccessibleChildNode,
                                     AccessibleChildAugment,
                                     accessible_child_augment,
                                     NULL,
                                     NULL);

      for (candidate = priv->first_child; candidate != NULL;)
        {
          guint present;

          if (!accessible_child_uses_widget_hierarchy (candidate))
            {
              incompatible = TRUE;
              break;
            }

          if (version != priv->accessible_child_index_version)
            break;

          present = gtk_accessible_should_present (GTK_ACCESSIBLE (candidate));
          if (version != priv->accessible_child_index_version)
            break;

          node = gtk_rb_tree_insert_after (index->tree, gtk_rb_tree_get_last (index->tree));
          node->widget = candidate;
          node->present = present;

          gtk_rb_tree_node_mark_dirty (node);
          g_hash_table_insert (index->nodes, candidate, node);

          candidate = candidate->priv->next_sibling;
        }

      if (candidate != NULL || version != priv->accessible_child_index_version)
        {
          if (incompatible && version == priv->accessible_child_index_version)
            priv->accessible_child_index_disabled = TRUE;
          accessible_child_index_free (index);
          goto out;
        }

      priv->accessible_child_index = index;
    }

  if (!(node = g_hash_table_lookup (index->nodes, child)))
    goto out;

  position = 0;

  if ((ancestor = gtk_rb_tree_node_get_left (node)))
    position += ((AccessibleChildAugment *) gtk_rb_tree_get_augment (index->tree, ancestor))->present;

  for (ancestor = gtk_rb_tree_node_get_parent (node);
       ancestor != NULL;
       node = ancestor, ancestor = gtk_rb_tree_node_get_parent (ancestor))
    {
      if (gtk_rb_tree_node_get_right (ancestor) == node)
        {
          AccessibleChildNode *previous = ancestor;
          gpointer left = gtk_rb_tree_node_get_left (ancestor);

          position += previous->present;

          if (left)
            position += ((AccessibleChildAugment *) gtk_rb_tree_get_augment (index->tree, left))->present;
        }
    }

  result = position;

out:
  priv->accessible_child_index_busy = FALSE;

  return result;
}

void
gtk_widget_update_accessible_child_index (GtkWidget *child)
{
  GtkWidget *parent;
  GtkAccessibleChildIndex *index;
  AccessibleChildNode *node;
  guint present;
  guint version;

  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!(parent = child->priv->parent))
    return;

  parent->priv->accessible_child_index_version++;

  if (parent->priv->accessible_child_index_busy)
    {
      gtk_widget_invalidate_accessible_child_index (parent);
      return;
    }

  if (!(index = parent->priv->accessible_child_index))
    return;

  parent->priv->accessible_child_index_busy = TRUE;
  version = parent->priv->accessible_child_index_version;
  present = gtk_accessible_should_present (GTK_ACCESSIBLE (child));

  if (version != parent->priv->accessible_child_index_version)
    {
      gtk_widget_invalidate_accessible_child_index (parent);
      goto out;
    }

  if (!(index = parent->priv->accessible_child_index))
    goto out;

  if (!(node = g_hash_table_lookup (index->nodes, child)))
    goto out;

  if (node->present != present)
    {
      node->present = present;
      gtk_rb_tree_node_mark_dirty (node);
    }

out:
  parent->priv->accessible_child_index_busy = FALSE;
}

void
gtk_widget_accessible_child_index_remove (GtkWidget *parent,
                                          GtkWidget *child)
{
  GtkAccessibleChildIndex *index = parent->priv->accessible_child_index;
  AccessibleChildNode *node;

  parent->priv->accessible_child_index_version++;
  parent->priv->accessible_child_index_disabled = FALSE;

  if (parent->priv->accessible_child_index_busy)
    {
      gtk_widget_invalidate_accessible_child_index (parent);
      return;
    }

  if (index == NULL)
    return;

  if ((node = g_hash_table_lookup (index->nodes, child)))
    {
      g_hash_table_remove (index->nodes, child);
      gtk_rb_tree_remove (index->tree, node);
    }
}

void
gtk_widget_accessible_child_index_reposition (GtkWidget *parent,
                                              GtkWidget *child,
                                              GtkWidget *previous_sibling)
{
  GtkAccessibleChildIndex *index = parent->priv->accessible_child_index;
  AccessibleChildNode *node;
  AccessibleChildNode *previous;
  guint present;
  guint version;

  parent->priv->accessible_child_index_version++;
  parent->priv->accessible_child_index_disabled = FALSE;

  if (parent->priv->accessible_child_index_busy)
    {
      gtk_widget_invalidate_accessible_child_index (parent);
      return;
    }

  if (index == NULL)
    return;

  parent->priv->accessible_child_index_busy = TRUE;
  version = parent->priv->accessible_child_index_version;

  if (!accessible_child_uses_widget_hierarchy (child) ||
      version != parent->priv->accessible_child_index_version)
    {
      gtk_widget_invalidate_accessible_child_index (parent);
      goto out;
    }

  present = gtk_accessible_should_present (GTK_ACCESSIBLE (child));

  if (version != parent->priv->accessible_child_index_version)
    {
      gtk_widget_invalidate_accessible_child_index (parent);
      goto out;
    }

  if (!(index = parent->priv->accessible_child_index))
    goto out;

  if ((node = g_hash_table_lookup (index->nodes, child)))
    {
      g_hash_table_remove (index->nodes, child);
      gtk_rb_tree_remove (index->tree, node);
    }

  previous = previous_sibling ? g_hash_table_lookup (index->nodes, previous_sibling) : NULL;
  node = gtk_rb_tree_insert_after (index->tree, previous);
  node->widget = child;
  node->present = present;

  gtk_rb_tree_node_mark_dirty (node);
  g_hash_table_insert (index->nodes, child, node);

out:
  parent->priv->accessible_child_index_busy = FALSE;
}

void
gtk_widget_clear_accessible_child_index (GtkWidget *widget)
{
  g_clear_pointer (&widget->priv->accessible_child_index, accessible_child_index_free);
}
