/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext.c: GL context abstraction
 *
 * Copyright © 2014  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GdkGLContext:
 *
 * Represents a platform-specific OpenGL draw context.
 *
 * `GdkGLContext`s are created for a surface using
 * [method@Gdk.Surface.create_gl_context], and the context will match
 * the characteristics of the surface.
 *
 * A `GdkGLContext` is not tied to any particular normal framebuffer.
 * For instance, it cannot draw to the surface back buffer. The GDK
 * repaint system is in full control of the painting to that. Instead,
 * you can create render buffers or textures and use [func@cairo_draw_from_gl]
 * in the draw function of your widget to draw them. Then GDK will handle
 * the integration of your rendering with that of other widgets.
 *
 * Support for `GdkGLContext` is platform-specific and context creation
 * can fail, returning %NULL context.
 *
 * A `GdkGLContext` has to be made "current" in order to start using
 * it, otherwise any OpenGL call will be ignored.
 *
 * ## Creating a new OpenGL context
 *
 * In order to create a new `GdkGLContext` instance you need a `GdkSurface`,
 * which you typically get during the realize call of a widget.
 *
 * A `GdkGLContext` is not realized until either [method@Gdk.GLContext.make_current]
 * or [method@Gdk.GLContext.realize] is called. It is possible to specify
 * details of the GL context like the OpenGL version to be used, or whether
 * the GL context should have extra state validation enabled after calling
 * [method@Gdk.Surface.create_gl_context] by calling [method@Gdk.GLContext.realize].
 * If the realization fails you have the option to change the settings of
 * the `GdkGLContext` and try again.
 *
 * ## Using a GdkGLContext
 *
 * You will need to make the `GdkGLContext` the current context before issuing
 * OpenGL calls; the system sends OpenGL commands to whichever context is current.
 * It is possible to have multiple contexts, so you always need to ensure that
 * the one which you want to draw with is the current one before issuing commands:
 *
 * ```c
 * gdk_gl_context_make_current (context);
 * ```
 *
 * You can now perform your drawing using OpenGL commands.
 *
 * You can check which `GdkGLContext` is the current one by using
 * [func@Gdk.GLContext.get_current]; you can also unset any `GdkGLContext`
 * that is currently set by calling [func@Gdk.GLContext.clear_current].
 */

#include "config.h"

#include "gdkglcontextprivate.h"

#include "gdkdebugprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkdmabufeglprivate.h"
#include "gdkdmabuffourccprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdkprofilerprivate.h"
#include "gdkglversionprivate.h"
#include "gdkdmabufformatsprivate.h"

#include "gdkprivate.h"

#include <glib/gi18n-lib.h>

#ifdef GDK_WINDOWING_WIN32
# include "gdk/win32/gdkwin32.h"
#endif

#include <epoxy/gl.h>
#ifdef HAVE_EGL
#include <epoxy/egl.h>
#endif

#include <math.h>

#define DEFAULT_ALLOWED_APIS GDK_GL_API_GL | GDK_GL_API_GLES

const GdkDebugKey gdk_gl_feature_keys[] = {
  { "debug", GDK_GL_FEATURE_DEBUG, "GL_KHR_debug" },
  { "base-instance", GDK_GL_FEATURE_BASE_INSTANCE, "GL_ARB_base_instance" },
  { "buffer-storage", GDK_GL_FEATURE_BUFFER_STORAGE, "GL_EXT_buffer_storage" },
  { "external-objects", GDK_GL_FEATURE_EXTERNAL_OBJECTS, "GL_EXT_memory_object and GL_EXT_semaphore"},
  { "external-objects-win32", GDK_GL_FEATURE_EXTERNAL_OBJECTS_WIN32, "GL_EXT_memory_object_win32 and GL_EXT_semaphore_win32" },
};

typedef struct _GdkGLContextPrivate GdkGLContextPrivate;

struct _GdkGLContextPrivate
{
  GdkGLVersion required;
  GdkGLVersion gl_version;

  GdkGLMemoryFlags memory_flags[GDK_MEMORY_N_FORMATS];

  GdkGLFeatures features;
  guint surface_attached : 1;
  guint use_khr_debug : 1;
  guint has_debug_output : 1;
  guint extensions_checked : 1;
  guint debug_enabled : 1;
  guint forward_compatible : 1;
  guint is_legacy : 1;

  GdkGLAPI allowed_apis;
  GdkGLAPI api;

  int max_debug_label_length;

#ifdef HAVE_EGL
  EGLContext egl_context;
  EGLBoolean (*eglSwapBuffersWithDamage) (EGLDisplay, EGLSurface, const EGLint *, EGLint);
#endif
};

enum {
  PROP_0,

  PROP_ALLOWED_APIS,
  PROP_API,
  PROP_SHARED_CONTEXT,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

/**
 * gdk_gl_error_quark:
 *
 * Registers an error quark for [class@Gdk.GLContext] errors.
 *
 * Returns: the error quark
 **/
G_DEFINE_QUARK (gdk-gl-error-quark, gdk_gl_error)

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkGLContext, gdk_gl_context, GDK_TYPE_DRAW_CONTEXT)

typedef struct _MaskedContext MaskedContext;

static inline MaskedContext *
mask_context (GdkGLContext *context,
              gboolean      surfaceless)
{
  return (MaskedContext *) GSIZE_TO_POINTER (GPOINTER_TO_SIZE (context) | (surfaceless ? 1 : 0));
}

static inline GdkGLContext *
unmask_context (MaskedContext *mask)
{
  return GDK_GL_CONTEXT (GSIZE_TO_POINTER (GPOINTER_TO_SIZE (mask) & ~(gsize) 1));
}

static inline gboolean
mask_is_surfaceless (MaskedContext *mask)
{
  return GPOINTER_TO_SIZE (mask) & (gsize) 1;
}

static void
unref_unmasked (gpointer data)
{
  g_object_unref (unmask_context (data));
}

static GPrivate thread_current_context = G_PRIVATE_INIT (unref_unmasked);

static void
gdk_gl_context_clear_old_updated_area (GdkGLContext *context)
{
  for (unsigned int i = 0; i < GDK_GL_MAX_TRACKED_BUFFERS; i++)
    g_clear_pointer (&context->old_updated_area[i], cairo_region_destroy);
}

static void
gdk_gl_context_dispose (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
#ifdef HAVE_EGL
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->egl_context != NULL)
    {
      GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
      EGLDisplay *egl_display = gdk_display_get_egl_display (display);

      if (eglGetCurrentContext () == priv->egl_context)
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      GDK_DISPLAY_DEBUG (display, OPENGL, "Destroying EGL context");

      eglDestroyContext (egl_display, priv->egl_context);
      priv->egl_context = NULL;
    }
#endif

  gdk_gl_context_clear_old_updated_area (context);

  G_OBJECT_CLASS (gdk_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_gl_context_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdkGLContext *self = GDK_GL_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_ALLOWED_APIS:
      gdk_gl_context_set_allowed_apis (self, g_value_get_flags (value));
      break;

    case PROP_SHARED_CONTEXT:
      g_assert (g_value_get_object (value) == NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_gl_context_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdkGLContext *self = GDK_GL_CONTEXT (object);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ALLOWED_APIS:
      g_value_set_flags (value, priv->allowed_apis);
      break;

    case PROP_API:
      g_value_set_flags (value, priv->api);
      break;

    case PROP_SHARED_CONTEXT:
      g_value_set_object (value, NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

#define N_EGL_ATTRS 16

#ifdef HAVE_EGL
static inline EGLenum
gdk_api_to_egl_api (GdkGLAPI api)
{
  switch (api)
    {
    case GDK_GL_API_GLES:
      return EGL_OPENGL_ES_API;
    case GDK_GL_API_GL:
    default:
      return EGL_OPENGL_API;
    }
}

static GdkGLAPI
gdk_gl_context_create_egl_context (GdkGLContext *context,
                                   GdkGLAPI      api,
                                   gboolean      legacy)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);
  GdkGLContext *share = gdk_display_get_gl_context (display);
  GdkGLContextPrivate *share_priv = share ? gdk_gl_context_get_instance_private (share) : NULL;
  EGLConfig egl_config;
  EGLContext ctx;
  EGLint context_attribs[N_EGL_ATTRS], i = 0, flags = 0;
  gsize major_idx, minor_idx;
  gboolean debug_bit, forward_bit;
  GdkGLVersion version;
  const GdkGLVersion* supported_versions;
  gsize j;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;

  if (!gdk_gl_context_is_api_allowed (context, api, NULL))
    return 0;

  /* We will use the default version matching the context status
   * unless the user requested a version which makes sense */
  gdk_gl_context_get_matching_version (context, api, legacy, &version);

  if (!eglBindAPI (gdk_api_to_egl_api (api)))
    return 0;

  debug_bit = gdk_gl_context_get_debug_enabled (context);
  forward_bit = gdk_gl_context_get_forward_compatible (context);

  if (display->have_egl_no_config_context)
    egl_config = NULL;
  else
    egl_config = gdk_display_get_egl_config (display, GDK_MEMORY_U8);

  if (debug_bit)
    flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
  if (forward_bit)
    flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

  if (api == GDK_GL_API_GL)
    {
      /* We want a core profile, unless in legacy mode */
      context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
      context_attribs[i++] = legacy
        ? EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT
        : EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    }

  if (legacy || api == GDK_GL_API_GLES)
    flags &= ~EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

  context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION;
  major_idx = i++;
  context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION;
  minor_idx = i++;
  context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
  context_attribs[i++] = flags;

  context_attribs[i++] = EGL_NONE;
  g_assert (i < N_EGL_ATTRS);

  GDK_DISPLAY_DEBUG (display, OPENGL,
                     "Creating EGL context version %d.%d (debug:%s, forward:%s, legacy:%s, es:%s)",
                     gdk_gl_version_get_major (&version), gdk_gl_version_get_minor (&version),
                     debug_bit ? "yes" : "no",
                     forward_bit ? "yes" : "no",
                     legacy ? "yes" : "no",
                     api == GDK_GL_API_GLES ? "yes" : "no");

  supported_versions = gdk_gl_versions_get_for_api (api);
  ctx = EGL_NO_CONTEXT;
  for (j = 0; gdk_gl_version_greater_equal (&supported_versions[j], &version); j++)
    {
      context_attribs [major_idx] = gdk_gl_version_get_major (&supported_versions[j]);
      context_attribs [minor_idx] = gdk_gl_version_get_minor (&supported_versions[j]);

      ctx = eglCreateContext (egl_display,
                              egl_config,
                              share ? share_priv->egl_context : EGL_NO_CONTEXT,
                              context_attribs);
      if (ctx != EGL_NO_CONTEXT)
        break;
    }

  if (ctx == EGL_NO_CONTEXT)
    return 0;

  priv->egl_context = ctx;
  gdk_gl_context_set_version (context, &supported_versions[j]);
  gdk_gl_context_set_is_legacy (context, legacy);

  if (epoxy_has_egl_extension (egl_display, "EGL_KHR_swap_buffers_with_damage"))
    priv->eglSwapBuffersWithDamage = (gpointer) epoxy_eglGetProcAddress ("eglSwapBuffersWithDamageKHR");
  else if (epoxy_has_egl_extension (egl_display, "EGL_EXT_swap_buffers_with_damage"))
    priv->eglSwapBuffersWithDamage = (gpointer) epoxy_eglGetProcAddress ("eglSwapBuffersWithDamageEXT");

  gdk_profiler_end_mark (start_time, "Create EGL context", NULL);

  return api;
}

static GdkGLAPI
gdk_gl_context_realize_egl (GdkGLContext  *context,
                            GError       **error)
{
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkGLContext *share = gdk_display_get_gl_context (display);
  GdkDebugFlags flags;
  GdkGLAPI api, preferred_api;
  gboolean prefer_legacy;

  flags = gdk_display_get_debug_flags(display);

  if (share && gdk_gl_context_is_api_allowed (context,
                                              gdk_gl_context_get_api (share),
                                              NULL))
    preferred_api = gdk_gl_context_get_api (share);
  else if ((flags & GDK_DEBUG_GL_PREFER_GL) != 0 &&
           gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, NULL))
    preferred_api = GDK_GL_API_GL;
  else if (gdk_gl_context_is_api_allowed (context, GDK_GL_API_GLES, NULL))
    preferred_api = GDK_GL_API_GLES;
  else if ((flags & GDK_DEBUG_GL_PREFER_GL) == 0 &&
            gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, NULL))
    preferred_api = GDK_GL_API_GL;
  else
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL API allowed."));
      return 0;
    }

  prefer_legacy = share != NULL && gdk_gl_context_is_legacy (share);

  if (preferred_api == GDK_GL_API_GL)
    {
      if ((api = gdk_gl_context_create_egl_context (context, GDK_GL_API_GL, prefer_legacy)) ||
          (api = gdk_gl_context_create_egl_context (context, GDK_GL_API_GLES, FALSE)) ||
          (api = gdk_gl_context_create_egl_context (context, GDK_GL_API_GL, TRUE)))
        return api;
    }
  else
    {
      if ((api = gdk_gl_context_create_egl_context (context, GDK_GL_API_GLES, FALSE)) ||
          (api = gdk_gl_context_create_egl_context (context, GDK_GL_API_GL, prefer_legacy)) ||
          (api = gdk_gl_context_create_egl_context (context, GDK_GL_API_GL, TRUE)))
        return api;
    }

  g_set_error_literal (error, GDK_GL_ERROR,
                       GDK_GL_ERROR_NOT_AVAILABLE,
                       _("Unable to create a GL context"));
  return 0;
}
#endif /* HAVE_EGL */

static GdkGLAPI
gdk_gl_context_default_realize (GdkGLContext  *context,
                                GError       **error)
{
#ifdef HAVE_EGL
  GdkDisplay *display = gdk_gl_context_get_display (context);

  if (gdk_display_get_egl_display (display))
    return gdk_gl_context_realize_egl (context, error);
#endif

  g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                       _("The current backend does not support OpenGL"));
  return 0;
}

#undef N_EGL_ATTRS

static cairo_region_t *
gdk_gl_context_real_get_damage (GdkGLContext *context)
{
  GdkDrawContext *draw_context = GDK_DRAW_CONTEXT (context);
  guint buffer_width, buffer_height;
#ifdef HAVE_EGL
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display = gdk_draw_context_get_display (draw_context);

  if (priv->egl_context && display->have_egl_buffer_age)
    {
      GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
      EGLDisplay egl_display;
      EGLSurface egl_surface;
      EGLint swap_behavior = EGL_BUFFER_DESTROYED, buffer_age = 0;

      egl_display = gdk_display_get_egl_display (display);
      egl_surface = gdk_surface_get_egl_surface (surface);

      gdk_gl_context_make_current (context);

      eglQuerySurface (egl_display, egl_surface, EGL_SWAP_BEHAVIOR, &swap_behavior);
      eglQuerySurface (egl_display, egl_surface, EGL_BUFFER_AGE_EXT, &buffer_age);

      if (swap_behavior == EGL_BUFFER_PRESERVED &&
          buffer_age > 0 && buffer_age <= GDK_GL_MAX_TRACKED_BUFFERS)
        {
          cairo_region_t *damage = cairo_region_create ();
          int i;

          for (i = 0; i < buffer_age - 1; i++)
            {
              if (context->old_updated_area[i] == NULL)
                {
                  gdk_draw_context_get_buffer_size (draw_context, &buffer_width, &buffer_height);
                  cairo_region_create_rectangle (&(GdkRectangle) {
                                                     0, 0,
                                                     buffer_width,
                                                     buffer_height
                                                 });
                  break;
                }
              cairo_region_union (damage, context->old_updated_area[i]);
            }

          return damage;
        }
    }
#endif

  gdk_draw_context_get_buffer_size (draw_context, &buffer_width, &buffer_height);
  return cairo_region_create_rectangle (&(GdkRectangle) {
                                            0, 0,
                                            buffer_width,
                                            buffer_height
                                        });
}

static gboolean
gdk_gl_context_real_is_shared (GdkGLContext *self,
                               GdkGLContext *other)
{
  if (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self)) != gdk_draw_context_get_display (GDK_DRAW_CONTEXT (other)))
    return FALSE;

  /* XXX: Should we check es or legacy here? */

  return TRUE;
}

static gboolean
gdk_gl_context_real_is_current (GdkGLContext *self)
{
#ifdef HAVE_EGL
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  return priv->egl_context == eglGetCurrentContext ();
#else
  return TRUE;
#endif
}

static gboolean
gdk_gl_context_real_clear_current (GdkGLContext *context)
{
#ifdef HAVE_EGL
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->egl_context == NULL)
    return FALSE;

  return eglMakeCurrent (gdk_display_get_egl_display (display),
                         EGL_NO_SURFACE,
                         EGL_NO_SURFACE,
                         EGL_NO_CONTEXT);
#else
  return FALSE;
#endif
}

static gboolean
gdk_gl_context_real_make_current (GdkGLContext *context,
                                  gboolean      surfaceless)
{
#ifdef HAVE_EGL
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  EGLSurface egl_surface;

  if (priv->egl_context == NULL)
    return FALSE;

  if (!surfaceless)
    egl_surface = gdk_surface_get_egl_surface (gdk_gl_context_get_surface (context));
  else
    egl_surface = EGL_NO_SURFACE;

  return eglMakeCurrent (gdk_display_get_egl_display (display),
                         egl_surface,
                         egl_surface,
                         priv->egl_context);
#else
  return FALSE;
#endif
}

static void
gdk_gl_context_real_begin_frame (GdkDrawContext  *draw_context,
                                 gpointer         context_data,
                                 GdkMemoryDepth   depth,
                                 cairo_region_t  *region,
                                 GdkColorState  **out_color_state,
                                 GdkMemoryDepth  *out_depth)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  G_GNUC_UNUSED GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkSurface *surface = gdk_draw_context_get_surface (draw_context);
  GdkColorState *color_state;
  cairo_region_t *damage;
  guint ww, wh;
  int i;

  color_state = gdk_surface_get_color_state (surface);

  depth = gdk_memory_depth_merge (depth, gdk_color_state_get_depth (color_state));

  g_assert (depth != GDK_MEMORY_U8_SRGB || gdk_color_state_get_no_srgb_tf (color_state) != NULL);

#ifdef HAVE_EGL
  if (priv->egl_context)
    *out_depth = gdk_surface_ensure_egl_surface (surface, depth);
  else
    *out_depth = GDK_MEMORY_U8;

  if (*out_depth == GDK_MEMORY_U8_SRGB)
    *out_color_state = gdk_color_state_get_no_srgb_tf (color_state);
  else
    *out_color_state = color_state;
#else
  *out_color_state = gdk_color_state_get_srgb ();
  *out_depth = GDK_MEMORY_U8;
#endif

  damage = GDK_GL_CONTEXT_GET_CLASS (context)->get_damage (context);

  g_clear_pointer (&context->old_updated_area[GDK_GL_MAX_TRACKED_BUFFERS - 1], cairo_region_destroy);
  for (i = GDK_GL_MAX_TRACKED_BUFFERS - 1; i > 0; i--)
    {
      context->old_updated_area[i] = context->old_updated_area[i - 1];
    }
  context->old_updated_area[0] = cairo_region_copy (region);

  cairo_region_union (region, damage);
  cairo_region_destroy (damage);

  gdk_draw_context_get_buffer_size (draw_context, &ww, &wh);

  gdk_gl_context_make_current (context);

  /* Initial setup */
  glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glViewport (0, 0, ww, wh);

#ifdef HAVE_EGL
  if (priv->egl_context && gdk_gl_context_check_version (context, NULL, "3.0"))
    glDrawBuffers (1, (GLenum[1]) { gdk_gl_context_get_use_es (context) ? GL_BACK : GL_BACK_LEFT });
#endif
}

static void
gdk_gl_context_real_end_frame (GdkDrawContext *draw_context,
                               gpointer        context_data,
                               cairo_region_t *painted)
{
#ifdef HAVE_EGL
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_surface_get_display (surface);
  EGLSurface egl_surface;
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  guint buffer_width, buffer_height;

  if (priv->egl_context == NULL)
    return;

  gdk_gl_context_make_current (context);

  egl_surface = gdk_surface_get_egl_surface (surface);
  gdk_draw_context_get_buffer_size (draw_context, &buffer_width, &buffer_height);

  if (priv->eglSwapBuffersWithDamage &&
      cairo_region_contains_rectangle (painted,
                                       &(cairo_rectangle_int_t) {
                                           0, 0,
                                           buffer_width, buffer_height
                                       }) == CAIRO_REGION_OVERLAP_IN)
    {
      EGLint stack_rects[4 * 4]; /* 4 rects */
      EGLint *heap_rects = NULL;
      int i, j, n_rects = cairo_region_num_rectangles (painted);
      EGLint *rects;

      if (n_rects < G_N_ELEMENTS (stack_rects) / 4)
        rects = (EGLint *)&stack_rects;
      else
        heap_rects = rects = g_new (EGLint, n_rects * 4);

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;

          cairo_region_get_rectangle (painted, i, &rect);
          rects[j++] = rect.x;
          rects[j++] = buffer_height - rect.height - rect.y;
          rects[j++] = rect.width;
          rects[j++] = rect.height;
        }
      priv->eglSwapBuffersWithDamage (gdk_display_get_egl_display (display), egl_surface, rects, n_rects);
      g_free (heap_rects);
    }
  else
    eglSwapBuffers (gdk_display_get_egl_display (display), egl_surface);
#endif

  gdk_profiler_add_mark (begin_time, GDK_PROFILER_CURRENT_TIME - begin_time, "EGL swap buffers", NULL);
}

static void
gdk_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);

  gdk_gl_context_clear_old_updated_area (context);
}

static guint
gdk_gl_context_real_get_default_framebuffer (GdkGLContext *self)
{
  return 0;
}

static void
gdk_gl_context_class_init (GdkGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  klass->realize = gdk_gl_context_default_realize;
  klass->get_damage = gdk_gl_context_real_get_damage;
  klass->is_shared = gdk_gl_context_real_is_shared;
  klass->make_current = gdk_gl_context_real_make_current;
  klass->clear_current = gdk_gl_context_real_clear_current;
  klass->is_current = gdk_gl_context_real_is_current;
  klass->get_default_framebuffer = gdk_gl_context_real_get_default_framebuffer;

  draw_context_class->begin_frame = gdk_gl_context_real_begin_frame;
  draw_context_class->end_frame = gdk_gl_context_real_end_frame;
  draw_context_class->surface_resized = gdk_gl_context_surface_resized;

  /**
   * GdkGLContext:shared-context:
   *
   * Always %NULL
   *
   * As many contexts can share data now and no single shared context exists
   * anymore, this function has been deprecated and now always returns %NULL.
   *
   * Deprecated: 4.4: Use [method@Gdk.GLContext.is_shared] to check if contexts
   *   can be shared.
   */
  properties[PROP_SHARED_CONTEXT] =
    g_param_spec_object ("shared-context", NULL, NULL,
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_DEPRECATED);

  /**
   * GdkGLContext:allowed-apis:
   *
   * The allowed APIs.
   *
   * Since: 4.6
   */
  properties[PROP_ALLOWED_APIS] =
    g_param_spec_flags ("allowed-apis", NULL, NULL,
                        GDK_TYPE_GL_API,
                        DEFAULT_ALLOWED_APIS,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkGLContext:api:
   *
   * The API currently in use.
   *
   * Since: 4.6
   */
  properties[PROP_API] =
    g_param_spec_flags ("api", NULL, NULL,
                        GDK_TYPE_GL_API,
                        0,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  gobject_class->set_property = gdk_gl_context_set_property;
  gobject_class->get_property = gdk_gl_context_get_property;
  gobject_class->dispose = gdk_gl_context_dispose;

  g_object_class_install_properties (gobject_class, LAST_PROP, properties);
}

static void
gdk_gl_context_init (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  priv->allowed_apis = DEFAULT_ALLOWED_APIS;
}

/* Must have called gdk_display_prepare_gl() before */
GdkGLContext *
gdk_gl_context_new (GdkDisplay *display,
                    GdkSurface *surface,
                    gboolean    surface_attached)
{
  GdkGLContextPrivate *priv;
  GdkGLContext *shared, *result;

  g_assert (surface == NULL || display == gdk_surface_get_display (surface));
  g_assert (!surface_attached || surface != NULL);

  /* assert gdk_display_prepare_gl() had been called */
  shared = gdk_display_get_gl_context (display);
  g_assert (shared);

  result = g_object_new (G_OBJECT_TYPE (shared),
                         "display", display,
                         "surface", surface,
                         NULL);

  priv = gdk_gl_context_get_instance_private (result);
  priv->surface_attached = surface_attached;

  return result;
}

void
gdk_gl_context_push_debug_group (GdkGLContext *context,
                                 const char   *message)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->use_khr_debug)
    glPushDebugGroupKHR (GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
}

void
gdk_gl_context_push_debug_group_printf (GdkGLContext *context,
                                        const char   *format,
                                        ...)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  char *message;
  va_list args;

  if (priv->use_khr_debug)
    {
      int msg_len;

      va_start (args, format);
      message = g_strdup_vprintf (format, args);
      va_end (args);

      msg_len = MIN (priv->max_debug_label_length, strlen (message) - 1);
      glPushDebugGroupKHR (GL_DEBUG_SOURCE_APPLICATION, 0, msg_len, message);
      g_free (message);
    }
}

void
gdk_gl_context_pop_debug_group (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->use_khr_debug)
    glPopDebugGroupKHR ();
}

void
gdk_gl_context_label_object (GdkGLContext *context,
                             guint         identifier,
                             guint         name,
                             const char   *label)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->use_khr_debug)
    glObjectLabel (identifier, name, -1, label);
}

void
gdk_gl_context_label_object_printf  (GdkGLContext *context,
                                     guint         identifier,
                                     guint         name,
                                     const char   *format,
                                     ...)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  char *message;
  va_list args;

  if (priv->use_khr_debug)
    {
      int msg_len;

      va_start (args, format);
      message = g_strdup_vprintf (format, args);
      va_end (args);

      msg_len = MIN (priv->max_debug_label_length, strlen (message) - 1);

      glObjectLabel (identifier, name, msg_len, message);
      g_free (message);
    }
}


gboolean
gdk_gl_context_has_feature (GdkGLContext  *self,
                            GdkGLFeatures  feature)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  return (priv->features & feature) == feature;
}

static gboolean
gdk_gl_context_is_realized (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  return priv->api != 0;
}

/**
 * gdk_gl_context_set_debug_enabled:
 * @context: a `GdkGLContext`
 * @enabled: whether to enable debugging in the context
 *
 * Sets whether the `GdkGLContext` should perform extra validations and
 * runtime checking.
 *
 * This is useful during development, but has additional overhead.
 *
 * The `GdkGLContext` must not be realized or made current prior to
 * calling this function.
 */
void
gdk_gl_context_set_debug_enabled (GdkGLContext *context,
                                  gboolean      enabled)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!gdk_gl_context_is_realized (context));

  enabled = !!enabled;

  priv->debug_enabled = enabled;
}

/**
 * gdk_gl_context_get_debug_enabled:
 * @context: a `GdkGLContext`
 *
 * Retrieves whether the context is doing extra validations and runtime checking.
 *
 * See [method@Gdk.GLContext.set_debug_enabled].
 *
 * Returns: %TRUE if debugging is enabled
 */
gboolean
gdk_gl_context_get_debug_enabled (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  return priv->debug_enabled;
}

/**
 * gdk_gl_context_set_forward_compatible:
 * @context: a `GdkGLContext`
 * @compatible: whether the context should be forward-compatible
 *
 * Sets whether the `GdkGLContext` should be forward-compatible.
 *
 * Forward-compatible contexts must not support OpenGL functionality that
 * has been marked as deprecated in the requested version; non-forward
 * compatible contexts, on the other hand, must support both deprecated and
 * non deprecated functionality.
 *
 * The `GdkGLContext` must not be realized or made current prior to calling
 * this function.
 */
void
gdk_gl_context_set_forward_compatible (GdkGLContext *context,
                                       gboolean      compatible)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!gdk_gl_context_is_realized (context));

  compatible = !!compatible;

  priv->forward_compatible = compatible;
}

/**
 * gdk_gl_context_get_forward_compatible:
 * @context: a `GdkGLContext`
 *
 * Retrieves whether the context is forward-compatible.
 *
 * See [method@Gdk.GLContext.set_forward_compatible].
 *
 * Returns: %TRUE if the context should be forward-compatible
 */
gboolean
gdk_gl_context_get_forward_compatible (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  return priv->forward_compatible;
}

void
gdk_gl_context_get_matching_version (GdkGLContext *context,
                                     GdkGLAPI      api,
                                     gboolean      legacy,
                                     GdkGLVersion *out_version)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkGLVersion min_version;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  if (api == GDK_GL_API_GL)
    min_version = GDK_GL_MIN_GL_VERSION;
  else
    min_version = GDK_GL_MIN_GLES_VERSION;

  if (gdk_gl_version_greater_equal (&priv->required, &min_version))
    *out_version = priv->required;
  else
    *out_version = min_version;

}

/**
 * gdk_gl_context_set_required_version:
 * @context: a `GdkGLContext`
 * @major: the major version to request
 * @minor: the minor version to request
 *
 * Sets the major and minor version of OpenGL to request.
 *
 * Setting @major and @minor to zero will use the default values.
 *
 * Setting @major and @minor lower than the minimum versions required
 * by GTK will result in the context choosing the minimum version.
 *
 * The @context must not be realized or made current prior to calling
 * this function.
 */
void
gdk_gl_context_set_required_version (GdkGLContext *context,
                                     int           major,
                                     int           minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!gdk_gl_context_is_realized (context));

  priv->required = GDK_GL_VERSION_INIT (major, minor);
}

gboolean
gdk_gl_context_check_gl_version (GdkGLContext       *self,
                                 const GdkGLVersion *required_gl,
                                 const GdkGLVersion *required_gles)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self), FALSE);

  if (!gdk_gl_context_is_realized (self))
    return FALSE;

  switch (priv->api)
    {
    case GDK_GL_API_GL:
      return required_gl == NULL || gdk_gl_version_greater_equal (&priv->gl_version, required_gl);

    case GDK_GL_API_GLES:
      return required_gles == NULL || gdk_gl_version_greater_equal (&priv->gl_version, required_gles);

    default:
      g_return_val_if_reached (FALSE);

    }
}

/**
 * gdk_gl_context_get_required_version:
 * @context: a `GdkGLContext`
 * @major: (out) (nullable): return location for the major version to request
 * @minor: (out) (nullable): return location for the minor version to request
 *
 * Retrieves required OpenGL version set as a requirement for the @context
 * realization. It will not change even if a greater OpenGL version is supported
 * and used after the @context is realized. See
 * [method@Gdk.GLContext.get_version] for the real version in use.
 *
 * See [method@Gdk.GLContext.set_required_version].
 */
void
gdk_gl_context_get_required_version (GdkGLContext *context,
                                     int          *major,
                                     int          *minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  if (major != NULL)
    *major = gdk_gl_version_get_major (&priv->required);
  if (minor != NULL)
    *minor = gdk_gl_version_get_minor (&priv->required);
}

/**
 * gdk_gl_context_is_legacy:
 * @context: a `GdkGLContext`
 *
 * Whether the `GdkGLContext` is in legacy mode or not.
 *
 * The `GdkGLContext` must be realized before calling this function.
 *
 * When realizing a GL context, GDK will try to use the OpenGL 3.2 core
 * profile; this profile removes all the OpenGL API that was deprecated
 * prior to the 3.2 version of the specification. If the realization is
 * successful, this function will return %FALSE.
 *
 * If the underlying OpenGL implementation does not support core profiles,
 * GDK will fall back to a pre-3.2 compatibility profile, and this function
 * will return %TRUE.
 *
 * You can use the value returned by this function to decide which kind
 * of OpenGL API to use, or whether to do extension discovery, or what
 * kind of shader programs to load.
 *
 * Returns: %TRUE if the GL context is in legacy mode
 */
gboolean
gdk_gl_context_is_legacy (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (gdk_gl_context_is_realized (context), FALSE);

  return priv->is_legacy;
}

void
gdk_gl_context_set_version (GdkGLContext       *context,
                            const GdkGLVersion *version)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  priv->gl_version = *version;
}

void
gdk_gl_context_set_is_legacy (GdkGLContext *context,
                              gboolean      is_legacy)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  priv->is_legacy = !!is_legacy;
}

/**
 * gdk_gl_context_is_shared:
 * @self: a `GdkGLContext`
 * @other: the `GdkGLContext` that should be compatible with @self
 *
 * Checks if the two GL contexts can share resources.
 *
 * When they can, the texture IDs from @other can be used in @self. This
 * is particularly useful when passing `GdkGLTexture` objects between
 * different contexts.
 *
 * Contexts created for the same display with the same properties will
 * always be compatible, even if they are created for different surfaces.
 * For other contexts it depends on the GL backend.
 *
 * Both contexts must be realized for this check to succeed. If either one
 * is not, this function will return %FALSE.
 *
 * Returns: %TRUE if the two GL contexts are compatible.
 *
 * Since: 4.4
 */
gboolean
gdk_gl_context_is_shared (GdkGLContext *self,
                          GdkGLContext *other)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self), FALSE);
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (other), FALSE);

  if (!gdk_gl_context_is_realized (self) ||
      !gdk_gl_context_is_realized (other))
    return FALSE;

  return GDK_GL_CONTEXT_GET_CLASS (self)->is_shared (self, other);
}

/**
 * gdk_gl_context_set_allowed_apis:
 * @self: a GL context
 * @apis: the allowed APIs
 *
 * Sets the allowed APIs. When gdk_gl_context_realize() is called, only the
 * allowed APIs will be tried. If you set this to 0, realizing will always fail.
 *
 * If you set it on a realized context, the property will not have any effect.
 * It is only relevant during gdk_gl_context_realize().
 *
 * By default, all APIs are allowed.
 *
 * Since: 4.6
 **/
void
gdk_gl_context_set_allowed_apis (GdkGLContext *self,
                                 GdkGLAPI      apis)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  g_return_if_fail (GDK_IS_GL_CONTEXT (self));

  if (priv->allowed_apis == apis)
    return;

  priv->allowed_apis = apis;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALLOWED_APIS]);
}

/**
 * gdk_gl_context_get_allowed_apis:
 * @self: a GL context
 *
 * Gets the allowed APIs set via gdk_gl_context_set_allowed_apis().
 *
 * Returns: the allowed APIs
 *
 * Since: 4.6
 **/
GdkGLAPI
gdk_gl_context_get_allowed_apis (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self), 0);

  return priv->allowed_apis;
}

/**
 * gdk_gl_context_get_api:
 * @self: a GL context
 *
 * Gets the API currently in use.
 *
 * If the renderer has not been realized yet, 0 is returned.
 *
 * Returns: the currently used API
 *
 * Since: 4.6
 **/
GdkGLAPI
gdk_gl_context_get_api (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self), 0);

  return priv->api;
}

gboolean
gdk_gl_context_is_api_allowed (GdkGLContext  *self,
                               GdkGLAPI       api,
                               GError       **error)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);
  GdkGLAPI allowed_apis;

  allowed_apis = priv->allowed_apis;

  if (!gdk_has_feature (GDK_FEATURE_GLES_API))
    {
      if (api == GDK_GL_API_GLES)
        {
          g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                               _("OpenGL ES API disabled via GDK_DISABLE"));
          return FALSE;
        }

      allowed_apis &= ~GDK_GL_API_GLES;
    }

  if (!gdk_has_feature (GDK_FEATURE_GL_API))
    {
      if (api == GDK_GL_API_GL)
        {
          g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                               _("OpenGL API disabled via GDK_DISABLE"));
          return FALSE;
        }

      allowed_apis &= ~GDK_GL_API_GL;
    }

  if (allowed_apis & api)
    return TRUE;

  g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
               _("Application does not support %s API"),
               api == GDK_GL_API_GL ? "OpenGL" : "OpenGL ES");

  return FALSE;
}

/**
 * gdk_gl_context_set_use_es:
 * @context: a `GdkGLContext`
 * @use_es: whether the context should use OpenGL ES instead of OpenGL,
 *   or -1 to allow auto-detection
 *
 * Requests that GDK create an OpenGL ES context instead of an OpenGL one.
 *
 * Not all platforms support OpenGL ES.
 *
 * The @context must not have been realized.
 *
 * By default, GDK will attempt to automatically detect whether the
 * underlying GL implementation is OpenGL or OpenGL ES once the @context
 * is realized.
 *
 * You should check the return value of [method@Gdk.GLContext.get_use_es]
 * after calling [method@Gdk.GLContext.realize] to decide whether to use
 * the OpenGL or OpenGL ES API, extensions, or shaders.
 */
void
gdk_gl_context_set_use_es (GdkGLContext *context,
                           int           use_es)
{
  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!gdk_gl_context_is_realized (context));

  switch (use_es)
  {
    case -1:
      gdk_gl_context_set_allowed_apis (context, DEFAULT_ALLOWED_APIS);
      break;
    case 0:
      gdk_gl_context_set_allowed_apis (context, GDK_GL_API_GL);
      break;
    case 1:
      gdk_gl_context_set_allowed_apis (context, GDK_GL_API_GLES);
      break;
    default:
      /* Just ignore the call */
      break;
  }
}

/**
 * gdk_gl_context_get_use_es:
 * @context: a `GdkGLContext`
 *
 * Checks whether the @context is using an OpenGL or OpenGL ES profile.
 *
 * Returns: %TRUE if the `GdkGLContext` is using an OpenGL ES profile;
 * %FALSE if other profile is in use of if the @context has not yet
 * been realized.
 */
gboolean
gdk_gl_context_get_use_es (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  return priv->api == GDK_GL_API_GLES;
}

static void APIENTRY
gl_debug_message_callback (GLenum        source,
                           GLenum        type,
                           GLuint        id,
                           GLenum        severity,
                           GLsizei       length,
                           const GLchar *message,
                           const void   *user_data)
{
  const char *message_source;
  const char *message_type;
  const char *message_severity;
  GLogLevelFlags log_level;

  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    return;

  switch (source)
    {
    case GL_DEBUG_SOURCE_API:
      message_source = "API";
      break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      message_source = "Window System";
      break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      message_source = "Shader Compiler";
      break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      message_source = "Third Party";
      break;
    case GL_DEBUG_SOURCE_APPLICATION:
      message_source = "Application";
      break;
    case GL_DEBUG_SOURCE_OTHER:
    default:
      message_source = "Other";
    }

  switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
      message_type = "Error";
      break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      message_type = "Deprecated Behavior";
      break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      message_type = "Undefined Behavior";
      break;
    case GL_DEBUG_TYPE_PORTABILITY:
      message_type = "Portability";
      break;
    case GL_DEBUG_TYPE_PERFORMANCE:
      message_type = "Performance";
      break;
    case GL_DEBUG_TYPE_MARKER:
      message_type = "Marker";
      break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
      message_type = "Push Group";
      break;
    case GL_DEBUG_TYPE_POP_GROUP:
      message_type = "Pop Group";
      break;
    case GL_DEBUG_TYPE_OTHER:
    default:
      message_type = "Other";
    }

  switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
      message_severity = "High";
      log_level = G_LOG_LEVEL_CRITICAL;
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      message_severity = "Medium";
      log_level = G_LOG_LEVEL_WARNING;
      break;
    case GL_DEBUG_SEVERITY_LOW:
      message_severity = "Low";
      log_level = G_LOG_LEVEL_MESSAGE;
      break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      message_severity = "Notification";
      log_level = G_LOG_LEVEL_INFO;
      break;
    default:
      message_severity = "Unknown";
      log_level = G_LOG_LEVEL_MESSAGE;
    }

  /* There's no higher level function taking a log level argument... */
  g_log_structured_standard (G_LOG_DOMAIN, log_level,
                             __FILE__, G_STRINGIFY (__LINE__),
                             G_STRFUNC,
                             "OPENGL:\n    Source: %s\n    Type: %s\n    Severity: %s\n    Message: %s",
                             message_source, message_type, message_severity, message);
}

/**
 * gdk_gl_context_realize:
 * @context: a `GdkGLContext`
 * @error: return location for a `GError`
 *
 * Realizes the given `GdkGLContext`.
 *
 * It is safe to call this function on a realized `GdkGLContext`.
 *
 * Returns: %TRUE if the context is realized
 */
gboolean
gdk_gl_context_realize (GdkGLContext  *context,
                        GError       **error)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  if (priv->api)
    return TRUE;

  priv->api = GDK_GL_CONTEXT_GET_CLASS (context)->realize (context, error);

  if (priv->api)
    {
      g_assert (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (0, 0)));

      g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_API]);
    }

  return priv->api;
}

static void
gdk_gl_context_init_memory_flags (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);
  gsize i;

  if (!gdk_gl_context_get_use_es (self))
    {
      for (i = 0; i < G_N_ELEMENTS (priv->memory_flags); i++)
        {
          priv->memory_flags[i] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
        }
      return;
    }

  /* GLES 2.0 spec, tables 3.2 and 3.3 */
  priv->memory_flags[GDK_MEMORY_R8G8B8] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R8G8B8A8_PREMULTIPLIED] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R8G8B8A8] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
#if 0
  /* GLES2 can do these, but GTK can't */
  priv->memory_flags[GDK_MEMORY_A8] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_G8] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_G8A8_PREMULTIPLIED] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_G8A8] = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
#endif

  /* GLES 3.0.6 spec, table 3.13 */
  priv->memory_flags[GDK_MEMORY_G8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_A8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_G8A8_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_G8A8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R8G8B8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R8G8B8A8_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R8G8B8A8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R8G8B8X8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R16G16B16_FLOAT] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R16G16B16A16_FLOAT] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_A16_FLOAT] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  priv->memory_flags[GDK_MEMORY_R32G32B32_FLOAT] |= GDK_GL_FORMAT_USABLE;
  priv->memory_flags[GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE;
  priv->memory_flags[GDK_MEMORY_R32G32B32A32_FLOAT] |= GDK_GL_FORMAT_USABLE;
  priv->memory_flags[GDK_MEMORY_A32_FLOAT] |= GDK_GL_FORMAT_USABLE;

  /* no changes in GLES 3.1 spec, table 8.13 */

  if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 2)))
    {
      /* GLES 3.2 spec, table 8.10 */
      priv->memory_flags[GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] |= GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_R16G16B16A16_FLOAT] |= GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_A16_FLOAT] |= GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] |= GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_R32G32B32A32_FLOAT] |= GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_A32_FLOAT] |= GDK_GL_FORMAT_RENDERABLE;
    }

  if (epoxy_has_gl_extension ("GL_OES_rgb8_rgba8"))
    {
      priv->memory_flags[GDK_MEMORY_R8G8B8A8_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R8G8B8A8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R8G8B8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R8G8B8X8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
    }
  if (epoxy_has_gl_extension ("GL_EXT_abgr"))
    {
      priv->memory_flags[GDK_MEMORY_A8B8G8R8_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_A8B8G8R8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_X8B8G8R8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
    }
  if (epoxy_has_gl_extension ("GL_EXT_texture_format_BGRA8888"))
    {
      priv->memory_flags[GDK_MEMORY_B8G8R8A8_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_B8G8R8A8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_B8G8R8X8] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
    }

  if (epoxy_has_gl_extension ("GL_EXT_texture_norm16"))
    {
      priv->memory_flags[GDK_MEMORY_R16G16B16A16_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R16G16B16A16] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R16G16B16] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_G16A16_PREMULTIPLIED] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_G16A16] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_G16] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_A16] |= GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_RENDERABLE | GDK_GL_FORMAT_FILTERABLE;
    }
  if (epoxy_has_gl_extension ("GL_OES_texture_half_float"))
    {
      GdkGLMemoryFlags flags = GDK_GL_FORMAT_USABLE;
      if (epoxy_has_gl_extension ("GL_EXT_color_buffer_half_float"))
        flags |= GDK_GL_FORMAT_RENDERABLE;
      if (epoxy_has_gl_extension ("GL_OES_texture_half_float_linear"))
        flags |= GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] |= flags;
      priv->memory_flags[GDK_MEMORY_R16G16B16A16_FLOAT] |= flags;
      /* disabled for now, see https://gitlab.freedesktop.org/mesa/mesa/-/issues/10378 */
      priv->memory_flags[GDK_MEMORY_R16G16B16_FLOAT] |= flags & ~GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_A16_FLOAT] |= flags;
    }
  if (epoxy_has_gl_extension ("GL_OES_texture_float"))
    {
      GdkGLMemoryFlags flags = GDK_GL_FORMAT_USABLE;
      if (epoxy_has_gl_extension ("GL_EXT_color_buffer_float"))
        flags |= GDK_GL_FORMAT_RENDERABLE;
      if (epoxy_has_gl_extension ("GL_OES_texture_float_linear"))
        flags |= GDK_GL_FORMAT_FILTERABLE;
      priv->memory_flags[GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] |= flags;
      priv->memory_flags[GDK_MEMORY_R32G32B32A32_FLOAT] |= flags;
      priv->memory_flags[GDK_MEMORY_R32G32B32_FLOAT] |= flags & ~GDK_GL_FORMAT_RENDERABLE;
      priv->memory_flags[GDK_MEMORY_A32_FLOAT] |= flags;
    }
}

void
gdk_gl_version_init_epoxy (GdkGLVersion *version)
{
  int epoxy_version = epoxy_gl_version ();

  *version = GDK_GL_VERSION_INIT (epoxy_version / 10, epoxy_version % 10);
}

static GdkGLFeatures
gdk_gl_context_check_features (GdkGLContext *context)
{
  GdkGLFeatures features = 0;

  if (epoxy_has_gl_extension ("GL_KHR_debug"))
    features |= GDK_GL_FEATURE_DEBUG;

  if (gdk_gl_context_check_version (context, "4.2", "9.9") ||
      epoxy_has_gl_extension ("GL_EXT_base_instance") ||
      epoxy_has_gl_extension ("GL_ARB_base_instance"))
    features |= GDK_GL_FEATURE_BASE_INSTANCE;

  if (gdk_gl_context_check_version (context, "4.4", "9.9") ||
      epoxy_has_gl_extension ("GL_EXT_buffer_storage") ||
      epoxy_has_gl_extension ("GL_ARB_buffer_storage"))
    features |= GDK_GL_FEATURE_BUFFER_STORAGE;

  if (epoxy_has_gl_extension ("GL_EXT_memory_object") &&
      epoxy_has_gl_extension ("GL_EXT_semaphore"))
    {
      features |= GDK_GL_FEATURE_EXTERNAL_OBJECTS;

      if (epoxy_has_gl_extension ("GL_EXT_memory_object_win32") &&
          epoxy_has_gl_extension ("GL_EXT_semaphore_win32"))
        features |= GDK_GL_FEATURE_EXTERNAL_OBJECTS_WIN32;
    }

  return features;
}

static void
gdk_gl_context_check_extensions (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkGLFeatures supported_features, disabled_features;
  gboolean gl_debug = FALSE;
  GdkDisplay *display;

  if (!gdk_gl_context_is_realized (context))
    return;

  if (priv->extensions_checked)
    return;

  priv->has_debug_output = epoxy_has_gl_extension ("GL_ARB_debug_output") ||
                           epoxy_has_gl_extension ("GL_KHR_debug");

  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  gl_debug = (gdk_display_get_debug_flags (display) & GDK_DEBUG_GL_DEBUG) != 0;

  if (priv->has_debug_output && gl_debug)
    {
      glEnable (GL_DEBUG_OUTPUT);
      glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback (gl_debug_message_callback, NULL);
    }

  /* If we asked for a core profile, but didn't get one, we're in legacy mode */
  if (!gdk_gl_context_get_use_es (context) &&
      !gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 2)))
    priv->is_legacy = TRUE;

  supported_features = gdk_gl_context_check_features (context);
  disabled_features = gdk_parse_debug_var ("GDK_GL_DISABLE",
      "GDK_GL_DISABLE can be set to values which cause GDK to disable\n"
      "certain OpenGL extensions.\n",
      gdk_gl_feature_keys,
      G_N_ELEMENTS (gdk_gl_feature_keys));

  /* handle feature dependencies */
  if (disabled_features & GDK_GL_FEATURE_EXTERNAL_OBJECTS)
    disabled_features |= GDK_GL_FEATURE_EXTERNAL_OBJECTS_WIN32;

  priv->features = supported_features & ~disabled_features;

  gdk_gl_context_init_memory_flags (context);

  if ((priv->features & GDK_GL_FEATURE_DEBUG) && gl_debug)
    {
      priv->use_khr_debug = TRUE;
      glGetIntegerv (GL_MAX_LABEL_LENGTH, &priv->max_debug_label_length);
    }

  if (GDK_DISPLAY_DEBUG_CHECK (display, OPENGL))
    {
      int i, max_texture_size;
      glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
      gdk_debug_message ("%s version: %d.%d (%s)",
                         gdk_gl_context_get_use_es (context) ? "OpenGL ES" : "OpenGL",
                         gdk_gl_version_get_major (&priv->gl_version), gdk_gl_version_get_minor (&priv->gl_version),
                         priv->is_legacy ? "legacy" : "core");
      gdk_debug_message ("GLSL version: %s", glGetString (GL_SHADING_LANGUAGE_VERSION));
      gdk_debug_message ("Max texture size: %d", max_texture_size);
      gdk_debug_message ("Enabled features (use GDK_GL_DISABLE env var to disable):");
      for (i = 0; i < G_N_ELEMENTS (gdk_gl_feature_keys); i++)
        {
          gdk_debug_message ("    %s: %s",
                             gdk_gl_feature_keys[i].key,
                             (priv->features & gdk_gl_feature_keys[i].value) ? "✓" :
                             ((disabled_features & gdk_gl_feature_keys[i].value) ? "disabled via env var" :
                             (((supported_features & gdk_gl_feature_keys[i].value) == 0) ? "✗" :
                             "Hum, what? This should not happen.")));
        }
    }

  priv->extensions_checked = TRUE;
}

static gboolean
gdk_gl_context_check_is_current (GdkGLContext *context)
{
  return GDK_GL_CONTEXT_GET_CLASS (context)->is_current (context);
}

/**
 * gdk_gl_context_make_current:
 * @context: a `GdkGLContext`
 *
 * Makes the @context the current one.
 */
void
gdk_gl_context_make_current (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  MaskedContext *current, *masked_context;
  gboolean surfaceless;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  if (priv->surface_attached)
    {
      surfaceless = FALSE;
    }
  else
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      surfaceless = !gdk_draw_context_is_in_frame (GDK_DRAW_CONTEXT (context));
G_GNUC_END_IGNORE_DEPRECATIONS
    }
  masked_context = mask_context (context, surfaceless);

  current = g_private_get (&thread_current_context);
  if (current == masked_context && gdk_gl_context_check_is_current (context))
    return;

  /* we need to realize the GdkGLContext if it wasn't explicitly realized */
  if (!gdk_gl_context_is_realized (context))
    {
      GError *error = NULL;

      gdk_gl_context_realize (context, &error);
      if (error != NULL)
        {
          g_critical ("Could not realize the GL context: %s", error->message);
          g_error_free (error);
          return;
        }
    }

  if (!GDK_GL_CONTEXT_GET_CLASS (context)->make_current (context, surfaceless))
    {
      g_warning ("gdk_gl_context_make_current() failed");
      return;
    }

  g_object_ref (context);
  g_private_replace (&thread_current_context, masked_context);
  gdk_gl_context_check_extensions (context);
}

/**
 * gdk_gl_context_get_display:
 * @context: a `GdkGLContext`
 *
 * Retrieves the display the @context is created for
 *
 * Returns: (nullable) (transfer none): a `GdkDisplay`
 */
GdkDisplay *
gdk_gl_context_get_display (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
}

/**
 * gdk_gl_context_get_surface:
 * @context: a `GdkGLContext`
 *
 * Retrieves the surface used by the @context.
 *
 * Returns: (nullable) (transfer none): a `GdkSurface`
 */
GdkSurface *
gdk_gl_context_get_surface (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
}

/**
 * gdk_gl_context_get_shared_context:
 * @context: a `GdkGLContext`
 *
 * Used to retrieves the `GdkGLContext` that this @context share data with.
 *
 * As many contexts can share data now and no single shared context exists
 * anymore, this function has been deprecated and now always returns %NULL.
 *
 * Returns: (nullable) (transfer none): %NULL
 *
 * Deprecated: 4.4: Use [method@Gdk.GLContext.is_shared] to check if contexts
 *   can be shared.
 */
GdkGLContext *
gdk_gl_context_get_shared_context (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return NULL;
}

/**
 * gdk_gl_context_get_version:
 * @context: a `GdkGLContext`
 * @major: (out): return location for the major version
 * @minor: (out): return location for the minor version
 *
 * Retrieves the OpenGL version of the @context.
 *
 * The @context must be realized prior to calling this function.
 */
void
gdk_gl_context_get_version (GdkGLContext *context,
                            int          *major,
                            int          *minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (gdk_gl_context_is_realized (context));

  if (major != NULL)
    *major = gdk_gl_version_get_major (&priv->gl_version);
  if (minor != NULL)
    *minor = gdk_gl_version_get_minor (&priv->gl_version);
}

const char *
gdk_gl_context_get_glsl_version_string (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  if (priv->api == GDK_GL_API_GL)
    {
      if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 6)))
        return "#version 460";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 5)))
        return "#version 450";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 4)))
        return "#version 440";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 3)))
        return "#version 430";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 2)))
        return "#version 420";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 1)))
        return "#version 410";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (4, 0)))
        return "#version 400";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 3)))
        return "#version 330";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 2)))
        return "#version 150";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 1)))
        return "#version 140";
      else
        return "#version 130";
    }
  else if (priv->api == GDK_GL_API_GLES)
    {
      if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 2)))
        return "#version 320 es";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 1)))
        return "#version 310 es";
      else if (gdk_gl_version_greater_equal (&priv->gl_version, &GDK_GL_VERSION_INIT (3, 0)))
        return "#version 300 es";
      else
        return "#version 100";
    }
  else
    {
      /* must be realized to be called */
      g_assert_not_reached ();
    }
}

/**
 * gdk_gl_context_clear_current:
 *
 * Clears the current `GdkGLContext`.
 *
 * Any OpenGL call after this function returns will be ignored
 * until [method@Gdk.GLContext.make_current] is called.
 */
void
gdk_gl_context_clear_current (void)
{
  MaskedContext *current;

  current = g_private_get (&thread_current_context);
  if (current != NULL)
    {
      GdkGLContext *context = unmask_context (current);

      if (GDK_GL_CONTEXT_GET_CLASS (context)->clear_current (context))
        g_private_replace (&thread_current_context, NULL);
    }
}

/*<private>
 * gdk_gl_context_clear_current_if_surface:
 * @surface: surface to clear for
 *
 * Does a gdk_gl_context_clear_current() if the current context is attached
 * to @surface, leaves the current context alone otherwise.
 * 
 * Returns: (nullable) (transfer full): The context that was cleared, so that it can be
 *   re-made current later
 **/
GdkGLContext *
gdk_gl_context_clear_current_if_surface (GdkSurface *surface)
{
  MaskedContext *current;

  current = g_private_get (&thread_current_context);
  if (current != NULL && !mask_is_surfaceless (current))
    {
      GdkGLContext *context = unmask_context (current);

      if (gdk_gl_context_get_surface (context) != surface)
        return NULL;

      g_object_ref (context);

      if (GDK_GL_CONTEXT_GET_CLASS (context)->clear_current (context))
        {
          g_private_replace (&thread_current_context, NULL);
          return context;
        }

      g_object_unref (context);
    }

  return NULL;
}

/**
 * gdk_gl_context_get_current:
 *
 * Retrieves the current `GdkGLContext`.
 *
 * Returns: (nullable) (transfer none): the current `GdkGLContext`
 */
GdkGLContext *
gdk_gl_context_get_current (void)
{
  MaskedContext *current;
  GdkGLContext *context;

  current = g_private_get (&thread_current_context);
  context = unmask_context (current);

  if (context && !gdk_gl_context_check_is_current (context))
    {
      g_private_replace (&thread_current_context, NULL);
      context = NULL;
    }

  return context;
}

GdkGLMemoryFlags
gdk_gl_context_get_format_flags (GdkGLContext    *self,
                                 GdkMemoryFormat  format)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  return priv->memory_flags[format];
}

/* Return if glGenVertexArrays, glBindVertexArray and glDeleteVertexArrays
 * can be used
 */
gboolean
gdk_gl_context_has_vertex_arrays (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  switch (priv->api)
    {
    case GDK_GL_API_GL:
      return TRUE;

    case GDK_GL_API_GLES:
      return gdk_gl_version_get_major (&priv->gl_version) >= 3;

    default:
      g_return_val_if_reached (FALSE);
    }
}

/* This is currently private! */
/* When using GL/ES, don't flip the 'R' and 'B' bits on Windows/ANGLE for glReadPixels() */
gboolean
gdk_gl_context_use_es_bgra (GdkGLContext *context)
{
  if (!gdk_gl_context_get_use_es (context))
    return FALSE;

#ifdef GDK_WINDOWING_WIN32
  if (GDK_WIN32_IS_GL_CONTEXT (context))
    return TRUE;
#endif

  return FALSE;
}

static GdkGLBackend the_gl_backend_type = GDK_GL_NONE;

static const char *gl_backend_names[] = {
  [GDK_GL_NONE] = "No GL (You should never read this)",
  [GDK_GL_EGL] = "EGL",
  [GDK_GL_GLX] = "X11 GLX",
  [GDK_GL_WGL] = "Windows WGL",
  [GDK_GL_CGL] = "Apple CGL"
};

/*<private>
 * gdk_gl_backend_can_be_used:
 * @backend_type: Type of backend to check
 * @error: Return location for an error
 *
 * Checks if this backend type can be used. When multiple displays
 * are opened that use different GL backends, conflicts can arise,
 * so this function checks that all displays use compatible GL
 * backends.
 *
 * Returns: %TRUE if the backend can still be used
 */
gboolean
gdk_gl_backend_can_be_used (GdkGLBackend   backend_type,
                            GError       **error)
{
  if (the_gl_backend_type != GDK_GL_NONE &&
      the_gl_backend_type != backend_type)
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   /* translators: This is about OpenGL backend names, like
                    * "Trying to use X11 GLX, but EGL is already in use"
                    */
                   _("Trying to use %s, but %s is already in use"),
                   gl_backend_names[backend_type],
                   gl_backend_names[the_gl_backend_type]);
      return FALSE;
    }

  if ((backend_type == GDK_GL_EGL && !gdk_has_feature (GDK_FEATURE_EGL)) ||
      (backend_type == GDK_GL_GLX && !gdk_has_feature (GDK_FEATURE_GLX)) ||
      (backend_type == GDK_GL_WGL && !gdk_has_feature (GDK_FEATURE_WGL)))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   _("Trying to use %s, but it is disabled via GDK_DISABLE"),
                   gl_backend_names[backend_type]);
      return FALSE;
    }

  return TRUE;
}

/*<private>
 * gdk_gl_backend_use:
 * @backend_type: Type of backend
 *
 * Ensures that the backend in use is the given one. If another backend
 * is already in use, this function will abort the program. It should
 * have previously checked via gdk_gl_backend_can_be_used().
 **/
void
gdk_gl_backend_use (GdkGLBackend backend_type)
{
  /* Check that the context class is properly initializing its backend type */
  g_assert (backend_type != GDK_GL_NONE);

  if (the_gl_backend_type == GDK_GL_NONE)
    {
      the_gl_backend_type = backend_type;
      /* This is important!!!11eleven
       * (But really: How do I print a message in 2 categories?) */
      GDK_DEBUG (OPENGL, "Using OpenGL backend %s", gl_backend_names[the_gl_backend_type]);
      GDK_DEBUG (MISC, "Using OpenGL backend %s", gl_backend_names[the_gl_backend_type]);
    }

  g_assert (the_gl_backend_type == backend_type);
}

gboolean
gdk_gl_context_export_dmabuf (GdkGLContext *self,
                              unsigned int  texture_id,
                              GdkDmabuf    *dmabuf)
{
#if defined(HAVE_EGL) && defined(HAVE_DMABUF)
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);
  GdkDisplay *display = gdk_gl_context_get_display (self);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);
  EGLContext egl_context = priv->egl_context;
  EGLint attribs[10];
  EGLImage image;
  gboolean result = FALSE;
  int i;
  int fourcc;
  int n_planes;
  EGLuint64KHR modifiers[GDK_DMABUF_MAX_PLANES];
  int fds[GDK_DMABUF_MAX_PLANES];
  int strides[GDK_DMABUF_MAX_PLANES];
  int offsets[GDK_DMABUF_MAX_PLANES];

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self), FALSE);
  g_return_val_if_fail (texture_id > 0, FALSE);
  g_return_val_if_fail (dmabuf != NULL, FALSE);

  if (egl_display == EGL_NO_DISPLAY || !display->have_egl_dma_buf_export)
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "Can't export dmabufs from GL, missing EGL or EGL_EXT_image_dma_buf_export");
      return 0;
    }

  GDK_DISPLAY_DEBUG (display, DMABUF, "Exporting GL texture to dmabuf");

  i = 0;
  attribs[i++] = EGL_IMAGE_PRESERVED_KHR;
  attribs[i++] = EGL_TRUE;

  attribs[i++] = EGL_NONE;

  image = eglCreateImageKHR (egl_display,
                             egl_context,
                             EGL_GL_TEXTURE_2D_KHR,
                             (EGLClientBuffer)GUINT_TO_POINTER (texture_id),
                             attribs);

  if (image == EGL_NO_IMAGE)
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "Creating EGLImage for dmabuf failed: %#x", eglGetError ());
      return FALSE;
    }

  if (!eglExportDMABUFImageQueryMESA (egl_display,
                                      image,
                                      &fourcc,
                                      &n_planes,
                                      NULL))
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "eglExportDMABUFImageQueryMESA failed: %#x", eglGetError ());
      goto out;
    }

  if (n_planes < 1 || n_planes > GDK_DMABUF_MAX_PLANES)
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "dmabufs with %d planes are not supported", n_planes);
      goto out;
    }

  if (!eglExportDMABUFImageQueryMESA (egl_display,
                                      image,
                                      &fourcc,
                                      &n_planes,
                                      modifiers))
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "eglExportDMABUFImageQueryMESA for modifiers failed: %#x", eglGetError ());
      goto out;
    }

  if (!eglExportDMABUFImageMESA (egl_display,
                                 image,
                                 fds,
                                 strides,
                                 offsets))
    {
      g_warning ("eglExportDMABUFImage failed: %#x", eglGetError ());
      goto out;
    }

  for (i = 0; i < n_planes; i++)
    {
      if (fds[i] == -1)
        {
          g_warning ("dmabuf plane %d has no file descriptor", i);
          goto out;
        }
    }

  dmabuf->fourcc = (guint32)fourcc;
  dmabuf->modifier = modifiers[0];
  dmabuf->n_planes = n_planes;

  for (i = 0; i < n_planes; i++)
    {
      dmabuf->planes[i].fd = fds[i];
      dmabuf->planes[i].stride = (int) strides[i];
      dmabuf->planes[i].offset = (int) offsets[i];
    }

  GDK_DISPLAY_DEBUG (display, DMABUF,
                     "Exported GL texture to dmabuf (format: %.4s:%#" G_GINT64_MODIFIER "x, planes: %d)",
                     (char *)&fourcc, modifiers[0], n_planes);

  result = TRUE;

out:
  eglDestroyImageKHR (egl_display, image);

  return result;
#else
  return FALSE;
#endif
}

static gboolean
gdk_gl_context_find_format (GdkGLContext    *self,
                            GdkMemoryAlpha   alpha,
                            GLint            gl_format,
                            GLint            gl_type,
                            GdkMemoryFormat *out_format)
{
  GdkMemoryFormat format;

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      GLint q_internal_format, q_internal_srgb_format;
      GLenum q_format, q_type;
      GdkSwizzle q_swizzle;

      if (gdk_memory_format_alpha (format) != alpha)
        continue;

      if (!(gdk_gl_context_get_format_flags (self, format) & GDK_GL_FORMAT_RENDERABLE))
        continue;

      if (!gdk_memory_format_gl_format (format,
                                        0,
                                        gdk_gl_context_get_use_es (self),
                                        &q_internal_format,
                                        &q_internal_srgb_format,
                                        &q_format,
                                        &q_type,
                                        &q_swizzle))
        continue;

      if (q_format != gl_format || q_type != gl_type)
        continue;

      *out_format = format;
      return TRUE;
    }

  return FALSE;
}

void
gdk_gl_context_download (GdkGLContext          *self,
                         GLuint                 tex_id,
                         GdkMemoryFormat        tex_format,
                         GdkColorState         *tex_color_state,
                         guchar                *dest_data,
                         const GdkMemoryLayout *dest_layout,
                         GdkColorState         *dest_color_state)
{
  gsize expected_stride;
  GLint gl_internal_format, gl_internal_srgb_format;
  GLenum gl_format, gl_type;
  GdkSwizzle gl_swizzle;

  g_assert (gdk_memory_format_get_n_planes (tex_format) == 1);

  expected_stride = dest_layout->width / gdk_memory_format_get_plane_block_width (dest_layout->format, 0)
                                       * gdk_memory_format_get_plane_block_bytes (dest_layout->format, 0);
  expected_stride = (expected_stride + 3) & ~3;

  if (!gdk_gl_context_get_use_es (self) &&
      ((gdk_gl_context_get_format_flags (self, tex_format) & GDK_GL_FORMAT_USABLE) == GDK_GL_FORMAT_USABLE))
    {
      if (!gdk_memory_format_gl_format (tex_format,
                                        0,
                                        gdk_gl_context_get_use_es (self),
                                        &gl_internal_format, &gl_internal_srgb_format,
                                        &gl_format, &gl_type, &gl_swizzle))
        {
          g_assert_not_reached ();
        }

      glBindTexture (GL_TEXTURE_2D, tex_id);

      if (dest_layout->planes[0].stride == expected_stride &&
          dest_layout->format == tex_format)
        {
          glGetTexImage (GL_TEXTURE_2D,
                         0,
                         gl_format,
                         gl_type,
                         dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, 0));

          gdk_memory_convert_color_state (dest_data,
                                          dest_layout,
                                          dest_color_state,
                                          tex_color_state);
        }
      else
        {
          GdkMemoryLayout pixel_layout;
          guchar *pixels;

          gdk_memory_layout_init (&pixel_layout, tex_format, dest_layout->width, dest_layout->height, 1);
          pixels = g_malloc (pixel_layout.size);

          glPixelStorei (GL_PACK_ALIGNMENT, 1);
          glGetTexImage (GL_TEXTURE_2D,
                         0,
                         gl_format,
                         gl_type,
                         pixels);

          gdk_memory_convert (dest_data,
                              dest_layout,
                              dest_color_state,
                              pixels,
                              &pixel_layout,
                              tex_color_state);

          g_free (pixels);
        }
    }
  else
    {
      GdkMemoryFormat actual_format;
      GLenum gl_read_format, gl_read_type;
      GLuint fbo;

      glGenFramebuffers (1, &fbo);
      glBindFramebuffer (GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);
      if (gdk_gl_context_check_version (self, "4.3", "3.1"))
        {
          GLint read_format, read_type;
          glGetFramebufferParameteriv (GL_FRAMEBUFFER, GL_IMPLEMENTATION_COLOR_READ_FORMAT, &read_format);
          glGetFramebufferParameteriv (GL_FRAMEBUFFER, GL_IMPLEMENTATION_COLOR_READ_TYPE, &read_type);
          if (gdk_gl_context_find_format (self, gdk_memory_format_alpha (tex_format), read_format, read_type, &actual_format))
            {
              gl_read_format = read_format;
              gl_read_type = read_type;
            }
          else
            {
              actual_format = gdk_memory_depth_get_format (gdk_memory_format_get_depth (tex_format, FALSE));
              if (gdk_memory_format_alpha (tex_format) == GDK_MEMORY_ALPHA_STRAIGHT)
                actual_format = gdk_memory_format_get_straight (actual_format);

              if (!gdk_memory_format_gl_format (actual_format,
                                                0,
                                                gdk_gl_context_get_use_es (self),
                                                &gl_internal_format, &gl_internal_srgb_format,
                                                &gl_read_format, &gl_read_type, &gl_swizzle))
                {
                  g_assert_not_reached ();
                }
            }
        }
      else
        {
          actual_format = gdk_memory_depth_get_format (gdk_memory_format_get_depth (tex_format, FALSE));
          if (gdk_memory_format_alpha (tex_format) == GDK_MEMORY_ALPHA_STRAIGHT)
            actual_format = gdk_memory_format_get_straight (actual_format);

          if (!gdk_memory_format_gl_format (actual_format,
                                            0,
                                            gdk_gl_context_get_use_es (self),
                                            &gl_internal_format, &gl_internal_srgb_format,
                                            &gl_read_format, &gl_read_type, &gl_swizzle))
            {
              g_assert_not_reached ();
            }
        }

      if (dest_layout->format == actual_format &&
          (dest_layout->planes[0].stride == expected_stride))
        {
          glReadPixels (0, 0,
                        dest_layout->width, dest_layout->height,
                        gl_read_format,
                        gl_read_type,
                        dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, 0));

          gdk_memory_convert_color_state (dest_data,
                                          dest_layout,
                                          dest_color_state,
                                          tex_color_state);
        }
      else
        {
          GdkMemoryLayout pixel_layout;
          guchar *pixels;

          gdk_memory_layout_init (&pixel_layout, actual_format, dest_layout->width, dest_layout->height, 1);
          pixels = g_malloc (pixel_layout.size);

          glPixelStorei (GL_PACK_ALIGNMENT, 1);
          glReadPixels (0, 0,
                        dest_layout->width, dest_layout->height,
                        gl_read_format,
                        gl_read_type,
                        pixels);

          /* Fix up gles inadequacies */

          if (gl_read_format == GL_RGBA &&
              gl_read_type == GL_UNSIGNED_BYTE &&
              (tex_format == GDK_MEMORY_G8A8 ||
               tex_format == GDK_MEMORY_G8A8_PREMULTIPLIED ||
               tex_format == GDK_MEMORY_G8 ||
               tex_format == GDK_MEMORY_A8))
            {
              gsize stride = pixel_layout.planes[0].stride;
              gsize actual_bpp = gdk_memory_format_get_plane_block_bytes (actual_format, 0);

              for (unsigned int y = 0; y < pixel_layout.height; y++)
                {
                  for (unsigned int x = 0; x < pixel_layout.width; x++)
                    {
                      guchar *data = &pixels[y * stride + x * actual_bpp];
                      if (tex_format == GDK_MEMORY_G8A8 ||
                          tex_format == GDK_MEMORY_G8A8_PREMULTIPLIED)
                        {
                          data[3] = data[1];
                          data[1] = data[0];
                          data[2] = data[0];
                        }
                      else if (tex_format == GDK_MEMORY_G8)
                        {
                          data[1] = data[0];
                          data[2] = data[0];
                          data[3] = 0xff;
                        }
                      else if (tex_format == GDK_MEMORY_A8)
                        {
                          data[3] = data[0];
                          data[0] = 0;
                          data[1] = 0;
                          data[2] = 0;
                        }
                    }
                }
            }

          if (gl_read_format == GL_RGBA &&
              gl_read_type == GL_UNSIGNED_SHORT &&
              (tex_format == GDK_MEMORY_G16A16 ||
               tex_format == GDK_MEMORY_G16A16_PREMULTIPLIED ||
               tex_format == GDK_MEMORY_G16 ||
               tex_format == GDK_MEMORY_A16))
            {
              gsize stride = pixel_layout.planes[0].stride;
              gsize actual_bpp = gdk_memory_format_get_plane_block_bytes (actual_format, 0);

              for (unsigned int y = 0; y < pixel_layout.height; y++)
                {
                  for (unsigned int x = 0; x < pixel_layout.width; x++)
                    {
                      guint16 *data = (guint16 *) &pixels[y * stride + x * actual_bpp];
                      if (tex_format == GDK_MEMORY_G16A16 ||
                          tex_format == GDK_MEMORY_G16A16_PREMULTIPLIED)
                        {
                          data[3] = data[1];
                          data[1] = data[0];
                          data[2] = data[0];
                        }
                      else if (tex_format == GDK_MEMORY_G16)
                        {
                          data[1] = data[0];
                          data[2] = data[0];
                          data[3] = 0xffff;
                        }
                      else if (tex_format == GDK_MEMORY_A16)
                        {
                          data[3] = data[0];
                          data[0] = 0;
                          data[1] = 0;
                          data[2] = 0;
                        }
                    }
                }
            }

          gdk_memory_convert (dest_data,
                              dest_layout,
                              dest_color_state,
                              pixels,
                              &pixel_layout,
                              tex_color_state);

          g_free (pixels);
        }
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers (1, &fbo);
    }
}

