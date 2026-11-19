#include "config.h"

#include "gsknodewalkerprivate.h"

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

  gsk_node_stack_append (&self->node_stack, &(GskNodeStackNode) { node, pos });

  return TRUE;
}

void
gsk_node_walker_end_node (GskNodeWalker *self)
{
  g_assert (gsk_node_stack_get_size (&self->node_stack) > 1);

  gsk_node_stack_set_size (&self->node_stack, gsk_node_stack_get_size (&self->node_stack) - 1);
}

