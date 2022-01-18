#ifndef __GSK_PRIVATE_H__
#define __GSK_PRIVATE_H__

#include <glib.h>
#include <pango2/pango.h>

G_BEGIN_DECLS

void gsk_ensure_resources (void);

int pango_glyph_string_num_glyphs (Pango2GlyphString *glyphs) G_GNUC_PURE;

typedef struct _GskVulkanRender GskVulkanRender;
typedef struct _GskVulkanRenderPass GskVulkanRenderPass;

G_END_DECLS

#endif /* __GSK_PRIVATE_H__ */
