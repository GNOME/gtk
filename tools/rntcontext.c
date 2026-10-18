/*  Copyright 2026 Benjamin Otte
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "rntcontext.h"

struct _RntContext
{
  GPtrArray *nodes;

  gboolean failed;
};

RntContext *
rnt_context_new (void)
{
  RntContext *self;

  self = g_new0 (RntContext, 1);

  self->nodes = g_ptr_array_new ();
  g_ptr_array_set_free_func (self->nodes, (GDestroyNotify) gsk_render_node_unref);

  return self;
}

void
rnt_context_free (RntContext *self)
{
  g_ptr_array_free (self->nodes, TRUE);

  g_free (self);
}

/**
 * rnt_context_get_node:
 * @self: the context
 * @nth: index of item to return
 *
 * Gets the nth node from the node stack or `NULL` if the node
 * stack doesn't contain that many nodes.
 *
 * Returns: (transfer none) (nullable): the nth node
 *   in the node stack
 **/
GskRenderNode *
rnt_context_get_node (RntContext *self,
                      gsize       nth)
{
  if (nth >= self->nodes->len)
    return NULL;

  return g_ptr_array_index (self->nodes, self->nodes->len - nth - 1);
}

/**
 * rnt_context_push_node:
 * @self: the context
 * @node: (transfer full) the rendernode to push
 *   to the top of the stack
 *
 * Pushes a new rendrenode to the top of the node stack.
 **/
void
rnt_context_push_node (RntContext    *self,
                       GskRenderNode *node)
{
  g_ptr_array_add (self->nodes, node);
}

/**
 * rnt_context_pop_node:
 * @self: the context
 *
 * Pops the topmost node from the node stack and returns it.
 *
 * If the stack was empty, null is returned.
 *
 * Result: (transfrer full) (nullable): The topmost node from
 *   the stack
 **/
GskRenderNode *
rnt_context_pop_node (RntContext *self)
{
  if (self->nodes->len == 0)
    return NULL;

  return g_ptr_array_steal_index (self->nodes,  self->nodes->len - 1);
}

/**
 * rnt_context_get_failed:
 * @ctx: the context
 *
 * Checks for reported failure. See rnt_context_set_failed() for
 * details.
 *
 * Returns: true if the context has failed
 **/
gboolean
rnt_context_get_failed (RntContext *ctx)
{
  return ctx->failed;
}

/**
 * rnt_context_set_failed:
 * @ctx: the context
 * @failed: true if failure occured, false to reset any previous
 *   failure
 *
 * Sets if this context is in a failure state.
 *
 * A failure state indicates to `gtk-rendernode-tool` that it should
 * exit with a non-zero exit status. It does not abort processing any
 * ongoing filter chain.
 **/
void
rnt_context_set_failed (RntContext *ctx,
                        gboolean    failed)
{
  ctx->failed = failed;
}
