#include "config.h"

#include "gsknodewalkerprivate.h"

#include "gskrendernodeprivate.h"
#include "gsktransform.h"
#include "gsktransformnode.h"

typedef struct _GskNodeStackNode GskNodeStackNode;
struct _GskNodeStackNode
{
  GskRenderNode *node;
  guint pos;
};

#define GDK_ARRAY_NAME gsk_node_stack
#define GDK_ARRAY_TYPE_NAME GskNodeStack
#define GDK_ARRAY_ELEMENT_TYPE GskNodeStackNode
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 64
#include "gdk/gdkarrayimpl.c"

struct _GskNodeWalker
{
  GskNodeStack node_stack;

  GskNodeWalker *replay;
  gsize replay_start;
  gsize replay_end;
  gsize replay_pos;
};

GskNodeWalker *
gsk_node_walker_new (GskRenderNode *node)
{
  GskNodeWalker *self;

  self = g_new0 (GskNodeWalker, 1);
  gsk_node_stack_init (&self->node_stack);
  gsk_node_stack_append (&self->node_stack, &(GskNodeStackNode) { node, 0 });

  return self;
}

GskNodeWalker *
gsk_node_walker_new_replay (GskNodeWalker *replay,
                            gsize          start_pos,
                            gsize          end_pos)
{
  GskNodeWalker *self;

  self = gsk_node_walker_new (gsk_node_stack_get (&replay->node_stack, start_pos)->node);

  self->replay = replay;
  self->replay_start = start_pos;
  self->replay_end = end_pos;
  self->replay_pos = 1;

  return self;
}

void
gsk_node_walker_free (GskNodeWalker *self)
{
  g_assert (gsk_node_stack_get_size (&self->node_stack) == 1);

  gsk_node_stack_clear (&self->node_stack);

  g_free (self);
}

gsize
gsk_node_walker_get_n_nodes (GskNodeWalker *self)
{
  return gsk_node_stack_get_size (&self->node_stack);
}

GskRenderNode *
gsk_node_walker_get_node (GskNodeWalker *self,
                          gsize          depth)
{
  return gsk_node_stack_get (&self->node_stack, depth)->node;
}

gsize
gsk_node_walker_get_pos (GskNodeWalker *self,
                         gsize          depth)
{
  return gsk_node_stack_get (&self->node_stack, depth)->pos;
}

gboolean
gsk_node_walker_start_node (GskNodeWalker *self,
                            GskRenderNode *node,
                            gsize          pos)
{
#ifndef G_DISABLE_ASSERT
  GskNodeStackNode *stack;
  GskRenderNode **children;
  gsize size, n_children;

  size = gsk_node_stack_get_size (&self->node_stack);
  stack = gsk_node_stack_get (&self->node_stack, size - 1);
  children = gsk_render_node_get_children (stack->node, &n_children);
  g_assert (pos < n_children);
  g_assert (children[pos] == node);
#endif

  if (self->replay_pos == gsk_node_stack_get_size (&self->node_stack))
    {
      GskNodeStackNode *check = gsk_node_stack_get (&self->replay->node_stack, self->replay_pos + self->replay_start);
      if (pos > check->pos)
        {
          return FALSE;
        }
      else if (pos == check->pos)
        {
          if (self->replay_pos + self->replay_start == self->replay_end)
            return FALSE;
          self->replay_pos++;
        }
    }

  gsk_node_stack_append (&self->node_stack, &(GskNodeStackNode) { node, pos });

  return TRUE;
}

void
gsk_node_walker_end_node (GskNodeWalker *self)
{
  g_assert (gsk_node_stack_get_size (&self->node_stack) > 1);

  gsk_node_stack_set_size (&self->node_stack, gsk_node_stack_get_size (&self->node_stack) - 1);

  if (self->replay_pos > gsk_node_stack_get_size (&self->node_stack))
    self->replay_pos--;
}

gboolean
gsk_node_walker_is_partial_node (GskNodeWalker *self)
{
  return self->replay_pos == gsk_node_stack_get_size (&self->node_stack);
}

GskRenderNode *
gsk_node_walker_get_copy_node (GskNodeWalker *self,
                               gsize          depth,
                               gsize         *out_pos)
{
  gsize i;

  for (i = gsk_node_stack_get_size (&self->node_stack); i-- > 0; )
    {
      GskNodeStackNode *stack = gsk_node_stack_get (&self->node_stack, i);

      if (gsk_render_node_isolates_copies (stack->node))
        return NULL;

      if (gsk_render_node_get_node_type (stack->node) != GSK_COPY_NODE)
        continue;

      if (depth == 0)
        {
          *out_pos = i;
          return stack->node;
        }
      
      depth--;
    }

  return NULL;
}

gsize
gsk_node_walker_get_replay_start (GskNodeWalker  *self,
                                  gsize           pos,
                                  GskTransform  **out_transform)
{
  gsize i;

  if (out_transform)
    *out_transform = NULL;

  if (pos == 0)
    return 0;

  for (i = pos; i-- > 0; )
    {
      GskNodeStackNode *stack = gsk_node_stack_get (&self->node_stack, i);

      if (gsk_render_node_isolates_background (stack->node))
        return i + 1;

      if (gsk_render_node_get_node_type (stack->node) == GSK_TRANSFORM_NODE && out_transform)
        {
          GskTransform *inv = gsk_transform_invert (gsk_transform_ref (gsk_transform_node_get_transform (stack->node)));
          *out_transform = gsk_transform_transform (*out_transform, inv);
          gsk_transform_unref (inv);
        }
    }

  return 0;
}
