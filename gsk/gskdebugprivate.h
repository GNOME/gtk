#ifndef __GSK_DEBUG_PRIVATE_H__
#define __GSK_DEBUG_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  GSK_DEBUG_RENDER_NODE = 1 << 0,
  GSK_DEBUG_RENDERER    = 1 << 1,
  GSK_DEBUG_CAIRO       = 1 << 2,
  GSK_DEBUG_OPENGL      = 1 << 3,
  GSK_DEBUG_SHADERS     = 1 << 4,
  GSK_DEBUG_TRANSFORMS  = 1 << 5,
  GSK_DEBUG_SURFACE     = 1 << 6,
  GSK_DEBUG_VULKAN      = 1 << 7,
  GSK_DEBUG_FALLBACK    = 1 << 8
} GskDebugFlags;

typedef enum {
  GSK_RENDERING_MODE_GEOMETRY       = 1 << 0,
  GSK_RENDERING_MODE_SHADERS        = 1 << 1,
  GSK_RENDERING_MODE_SYNC           = 1 << 2,
  GSK_RENDERING_MODE_FULL_REDRAW    = 1 << 3,
  GSK_RENDERING_MODE_STAGING_IMAGE  = 1 << 4,
  GSK_RENDERING_MODE_STAGING_BUFFER = 1 << 5
} GskRenderingMode;

gboolean gsk_check_debug_flags (GskDebugFlags flags);

gboolean gsk_check_rendering_flags (GskRenderingMode flags);

#ifdef G_ENABLE_DEBUG

#define GSK_DEBUG_CHECK(type)           G_UNLIKELY (gsk_check_debug_flags (GSK_DEBUG_ ## type))
#define GSK_DEBUG_CHECK2(type1,type2)   G_UNLIKELY (gsk_check_debug_flags (GSK_DEBUG_ ## type1 | GSK_DEBUG_ ## type2))
#define GSK_RENDER_MODE_CHECK(type)     G_UNLIKELY (gsk_check_rendering_flags (GSK_RENDERING_MODE_ ## type))

#define GSK_NOTE(type,action)   G_STMT_START {  \
  if (GSK_DEBUG_CHECK (type)) {                 \
    action;                                     \
  }                             } G_STMT_END

#define GSK_NOTE2(type1,type2,action)   G_STMT_START {  \
  if (GSK_DEBUG_CHECK2 (type1, type2)) {                \
    action;                                             \
  }                             } G_STMT_END
#else

#define GSK_RENDER_MODE_CHECK(type)     0
#define GSK_DEBUG_CHECK(type)           0
#define GSK_DEBUG_CHECK2(type1,type2)   0
#define GSK_NOTE(type,action)
#define GSK_NOTE2(type1,type2,action)

#endif

G_END_DECLS

#endif /* __GSK_DEBUG_PRIVATE_H__ */
