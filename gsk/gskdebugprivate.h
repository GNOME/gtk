#ifndef __GSK_DEBUG_PRIVATE_H__
#define __GSK_DEBUG_PRIVATE_H__

#include <glib.h>
#include "gdk/gdkdebugprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_DEBUG_RENDERER              = 1 <<  0,
  GSK_DEBUG_CAIRO                 = 1 <<  1,
  GSK_DEBUG_OPENGL                = 1 <<  2,
  GSK_DEBUG_SHADERS               = 1 <<  3,
  GSK_DEBUG_SURFACE               = 1 <<  4,
  GSK_DEBUG_VULKAN                = 1 <<  5,
  GSK_DEBUG_FALLBACK              = 1 <<  6,
  GSK_DEBUG_GLYPH_CACHE           = 1 <<  7,
  /* flags below may affect behavior */
  GSK_DEBUG_GEOMETRY              = 1 <<  9,
  GSK_DEBUG_FULL_REDRAW           = 1 << 10,
  GSK_DEBUG_SYNC                  = 1 << 11,
  GSK_DEBUG_VULKAN_STAGING_IMAGE  = 1 << 12,
  GSK_DEBUG_VULKAN_STAGING_BUFFER = 1 << 13
} GskDebugFlags;

#define GSK_DEBUG_ANY ((1 << 13) - 1)

GskDebugFlags gsk_get_debug_flags (void);
void          gsk_set_debug_flags (GskDebugFlags flags);

gboolean gsk_check_debug_flags (GskDebugFlags flags);

#ifdef G_ENABLE_DEBUG

#define GSK_DEBUG_CHECK(type)           G_UNLIKELY (gsk_check_debug_flags (GSK_DEBUG_ ## type))
#define GSK_RENDERER_DEBUG_CHECK(renderer,type) \
  G_UNLIKELY ((gsk_renderer_get_debug_flags (renderer) & GSK_DEBUG_ ## type) != 0)

#define GSK_RENDERER_DEBUG(renderer,type,...)                               \
    G_STMT_START {                                                          \
    if (GSK_RENDERER_DEBUG_CHECK (renderer,type))                           \
      gdk_debug_message (__VA_ARGS__);                                      \
    } G_STMT_END

#define GSK_DEBUG(type,...)                                                 \
    G_STMT_START {                                                          \
    if (GSK_DEBUG_CHECK (type))                                             \
      gdk_debug_message (__VA_ARGS__);                                      \
    } G_STMT_END

#else

#define GSK_DEBUG_CHECK(type)           0
#define GSK_RENDERER_DEBUG_CHECK(renderer,type) 0
#define GSK_RENDERER_DEBUG(display,type,...)
#define GSK_DEBUG(type,...)

#endif

G_END_DECLS

#endif /* __GSK_DEBUG_PRIVATE_H__ */
