
#ifndef __GSK_RENDER_NODE_PARSER_PRIVATE_H__
#define __GSK_RENDER_NODE_PARSER_PRIVATE_H__

#include "gskrendernode.h"

GskRenderNode * gsk_render_node_deserialize_from_bytes  (GBytes            *bytes,
                                                         GskParseErrorFunc  error_func,
                                                         gpointer           user_data);

#endif
