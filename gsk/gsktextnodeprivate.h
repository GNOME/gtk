#pragma once

#include "gsktextnode.h"

G_BEGIN_DECLS

void            gsk_text_node_serialize_glyphs          (GskRenderNode               *self,
                                                         GString                     *str);
 
cairo_hint_style_t
                gsk_text_node_get_font_hint_style       (const GskRenderNode         *self) G_GNUC_PURE;

G_END_DECLS
