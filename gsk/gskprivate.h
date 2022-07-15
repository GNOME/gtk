#ifndef __GSK_PRIVATE_H__
#define __GSK_PRIVATE_H__

#include <glib.h>
#include <pango/pango.h>

G_BEGIN_DECLS

void gsk_ensure_resources (void);

typedef struct _GskVulkanRender GskVulkanRender;
typedef struct _GskVulkanRenderPass GskVulkanRenderPass;

G_END_DECLS

#endif /* __GSK_PRIVATE_H__ */
