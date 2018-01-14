#include "gskdebugprivate.h"

#ifdef G_ENABLE_DEBUG
static const GDebugKey gsk_debug_keys[] = {
  { "renderer", GSK_DEBUG_RENDERER },
  { "cairo", GSK_DEBUG_CAIRO },
  { "opengl", GSK_DEBUG_OPENGL },
  { "shaders", GSK_DEBUG_SHADERS },
  { "surface", GSK_DEBUG_SURFACE },
  { "vulkan", GSK_DEBUG_VULKAN },
  { "fallback", GSK_DEBUG_FALLBACK },
  { "glyphcache", GSK_DEBUG_GLYPH_CACHE },
  { "geometry", GSK_DEBUG_GEOMETRY },
  { "full-redraw", GSK_DEBUG_FULL_REDRAW},
  { "sync", GSK_DEBUG_SYNC },
  { "vulkan-staging-image", GSK_DEBUG_VULKAN_STAGING_IMAGE },
  { "vulkan-staging-buffer", GSK_DEBUG_VULKAN_STAGING_BUFFER }
};
#endif

static guint gsk_debug_flags;

static void
init_debug_flags (void)
{
#ifdef G_ENABLE_DEBUG
  static volatile gsize gsk_debug_flags__set;

  if (g_once_init_enter (&gsk_debug_flags__set))
    {
      const char *env = g_getenv ("GSK_DEBUG");

      gsk_debug_flags = g_parse_debug_string (env,
                                              (GDebugKey *) gsk_debug_keys,
                                              G_N_ELEMENTS (gsk_debug_keys));

      g_once_init_leave (&gsk_debug_flags__set, TRUE);
    }
#endif
}

gboolean
gsk_check_debug_flags (GskDebugFlags flags)
{
  init_debug_flags ();

  return (gsk_debug_flags & flags) != 0;
}

GskDebugFlags
gsk_get_debug_flags (void)
{
  init_debug_flags ();

  return gsk_debug_flags;
}
