
#include <glib/gprintf.h>
#include "gskglnodesampleprivate.h"
#include "gskrendernodeprivate.h"

void
node_sample_init (NodeSample *self)
{
  memset (self->nodes, 0, sizeof (self->nodes));
  self->count = 0;
}

void
node_sample_reset (NodeSample *self)
{
  node_sample_init (self);
}

void
node_sample_add (NodeSample    *self,
                 GskRenderNode *node)
{
  const guint node_type = gsk_render_node_get_node_type (node);

  g_assert (node_type <= N_NODE_TYPES);

  if (self->nodes[node_type].class_name == NULL)
    self->nodes[node_type].class_name = g_type_name_from_instance ((GTypeInstance *) node); 

  self->nodes[node_type].count ++;
  self->count ++;
}

void
node_sample_print (const NodeSample *self,
                   const char       *prefix)
{
  guint i;

  g_printf ("%s:\n", prefix);

  for (i = 0; i < N_NODE_TYPES; i ++)
    {
      if (self->nodes[i].count > 0)
        {
          double p = (double)self->nodes[i].count / (double)self->count;

          g_printf ("%s: %u (%.2f%%)\n", self->nodes[i].class_name, self->nodes[i].count, p * 100.0);
        }
    }
}
