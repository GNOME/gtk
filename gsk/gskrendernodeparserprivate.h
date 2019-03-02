
#ifndef __GSK_RENDER_NODE_PARSER_PRIVATE_H__
#define __GSK_RENDER_NODE_PARSER_PRIVATE_H__

#include "gskrendernode.h"

GskRenderNode * gsk_render_node_deserialize_from_string (const char *string);
char *          gsk_render_node_serialize_to_string     (GskRenderNode *root);

#endif
