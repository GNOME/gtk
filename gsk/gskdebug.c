#include "gskdebugprivate.h"

#ifdef G_ENABLE_DEBUG
static const GDebugKey gsk_debug_keys[] = {
  { "rendernode", GSK_DEBUG_RENDER_NODE },
  { "renderer", GSK_DEBUG_RENDERER },
  { "cairo", GSK_DEBUG_CAIRO },
  { "opengl", GSK_DEBUG_OPENGL },
  { "shaders", GSK_DEBUG_SHADERS },
  { "transforms", GSK_DEBUG_TRANSFORMS },
  { "surface", GSK_DEBUG_SURFACE },
  { "vulkan", GSK_DEBUG_VULKAN },
  { "fallback", GSK_DEBUG_FALLBACK }
};
#endif

static const GDebugKey gsk_rendering_keys[] = {
  { "geometry", GSK_RENDERING_MODE_GEOMETRY },
  { "shaders", GSK_RENDERING_MODE_SHADERS },
  { "sync", GSK_RENDERING_MODE_SYNC },
  { "full-redraw", GSK_RENDERING_MODE_FULL_REDRAW},
  { "staging-image", GSK_RENDERING_MODE_STAGING_IMAGE },
  { "staging-buffer", GSK_RENDERING_MODE_STAGING_BUFFER }
};

gboolean
gsk_check_debug_flags (GskDebugFlags flags)
{
#ifdef G_ENABLE_DEBUG
  static volatile gsize gsk_debug_flags__set;
  static guint gsk_debug_flags;

  if (g_once_init_enter (&gsk_debug_flags__set))
    {
      const char *env = g_getenv ("GSK_DEBUG");

      gsk_debug_flags = g_parse_debug_string (env,
                                              (GDebugKey *) gsk_debug_keys,
                                              G_N_ELEMENTS (gsk_debug_keys));

      g_once_init_leave (&gsk_debug_flags__set, TRUE);
    }

  return (gsk_debug_flags & flags) != 0;
#else
  return FALSE;
#endif
}

gboolean
gsk_check_rendering_flags (GskRenderingMode flags)
{
  static volatile gsize gsk_rendering_flags__set;
  static guint gsk_rendering_flags;

  if (g_once_init_enter (&gsk_rendering_flags__set))
    {
      const char *env = g_getenv ("GSK_RENDERING_MODE");

      gsk_rendering_flags = g_parse_debug_string (env,
                                                  (GDebugKey *) gsk_rendering_keys,
                                                  G_N_ELEMENTS (gsk_rendering_keys));

      g_once_init_leave (&gsk_rendering_flags__set, TRUE);
    }

  return (gsk_rendering_flags & flags) != 0;
}
