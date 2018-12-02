
#ifndef __GSK_GL_NODE_SAMPLE_PRIVATE_H__
#define __GSK_GL_NODE_SAMPLE_PRIVATE_H__

#include <glib.h>
#include "gskenums.h"
#include "gskrendernode.h"

/* TODO: We have no other way for this...? */
#define N_NODE_TYPES (GSK_DEBUG_NODE + 1)

typedef struct
{
  struct {
    const char *class_name;
    guint count;
  } nodes[N_NODE_TYPES];
  guint count;
} NodeSample;

void node_sample_init  (NodeSample       *self);
void node_sample_reset (NodeSample       *self);
void node_sample_add   (NodeSample       *self,
                        GskRenderNode    *node);
void node_sample_print (const NodeSample *self,
                        const char       *prefix);

#endif
