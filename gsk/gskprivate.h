#ifndef __GSK_PRIVATE_H__
#define __GSK_PRIVATE_H__

#include "gskenums.h"

#include <glib.h>
#include <pango/pango.h>
#include <cairo.h>

G_BEGIN_DECLS

void gsk_ensure_resources (void);

int pango_glyph_string_num_glyphs (PangoGlyphString *glyphs);

GskTextRenderFlags gsk_text_render_flags_from_cairo (const cairo_font_options_t *options);
void               gsk_text_render_flags_to_cairo   (GskTextRenderFlags          flags,
                                                     cairo_font_options_t       *options);

typedef struct _GskVulkanRender GskVulkanRender;
typedef struct _GskVulkanRenderPass GskVulkanRenderPass;

G_END_DECLS

#endif /* __GSK_PRIVATE_H__ */
