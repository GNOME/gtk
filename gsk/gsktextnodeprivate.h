#pragma once

#include "gsktextnode.h"
#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

void            gsk_text_node_serialize_glyphs          (GskRenderNode               *self,
                                                         GString                     *str);
 
cairo_hint_style_t
                gsk_text_node_get_font_hint_style       (const GskRenderNode         *self) G_GNUC_PURE;

GskRenderNode * gsk_text_node_new2                      (PangoFont              *font,
                                                         PangoGlyphString       *glyphs,
                                                         const GdkColor         *color,
                                                         const graphene_point_t *offset);
const GdkColor *gsk_text_node_get_gdk_color             (const GskRenderNode    *node);

G_END_DECLS
