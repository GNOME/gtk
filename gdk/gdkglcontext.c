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
 * `GdkGLContext` is an object representing a platform-specific
 * OpenGL draw context.
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

#include "gdkdebug.h"
#include "gdkdisplayprivate.h"
#include "gdkintl.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdkprofilerprivate.h"

#include "gdk-private.h"

#ifdef GDK_WINDOWING_WIN32
# include "gdk/win32/gdkwin32.h"
#endif

#include <epoxy/gl.h>
#ifdef HAVE_EGL
#include <epoxy/egl.h>
#endif

#define DEFAULT_ALLOWED_APIS GDK_GL_API_GL | GDK_GL_API_GLES

typedef struct {
  int major;
  int minor;
  int gl_version;

  guint has_khr_debug : 1;
  guint use_khr_debug : 1;
  guint has_unpack_subimage : 1;
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
} GdkGLContextPrivate;

enum {
  PROP_0,

  PROP_ALLOWED_APIS,
  PROP_API,
  PROP_SHARED_CONTEXT,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

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
  int i;

  for (i = 0; i < 2; i++)
    {
      g_clear_pointer (&context->old_updated_area[i], cairo_region_destroy);
    }
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

      GDK_DISPLAY_NOTE (display, OPENGL, g_message ("Destroying EGL context"));

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

#define N_EGL_ATTRS     16

static GdkGLAPI
gdk_gl_context_realize_egl (GdkGLContext  *context,
                            GError       **error)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);

  EGLConfig egl_config;
  GdkGLContext *share = gdk_display_get_gl_context (display);
  GdkGLContextPrivate *share_priv = gdk_gl_context_get_instance_private (share);
  EGLContext ctx;
  EGLint context_attribs[N_EGL_ATTRS];
  int major, minor, flags;
  gboolean debug_bit, forward_bit, legacy_bit;
  GdkGLAPI api;
  int i = 0;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;

  if (share != NULL)
    gdk_gl_context_get_required_version (share, &major, &minor);
  else
    gdk_gl_context_get_required_version (context, &major, &minor);

  debug_bit = gdk_gl_context_get_debug_enabled (context);
  forward_bit = gdk_gl_context_get_forward_compatible (context);
  legacy_bit = GDK_DISPLAY_DEBUG_CHECK (display, GL_LEGACY) ||
    (share != NULL && gdk_gl_context_is_legacy (share));

  if (display->have_egl_no_config_context)
    egl_config = NULL;
  else
    egl_config = gdk_display_get_egl_config (display);

  flags = 0;

  if (debug_bit)
    flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
  if (forward_bit)
    flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

  if (gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, NULL) &&
      eglBindAPI (EGL_OPENGL_API))
    {
      /* We want a core profile, unless in legacy mode */
      context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
      context_attribs[i++] = legacy_bit
        ? EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR
        : EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;

      /* Specify the version */
      context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
      context_attribs[i++] = legacy_bit ? 3 : major;
      context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION_KHR;
      context_attribs[i++] = legacy_bit ? 0 : minor;
      api = GDK_GL_API_GL;
    }
  else if (gdk_gl_context_is_api_allowed (context, GDK_GL_API_GLES, NULL) &&
           eglBindAPI (EGL_OPENGL_ES_API))
    {
      context_attribs[i++] = EGL_CONTEXT_CLIENT_VERSION;
      if (major == 3)
        context_attribs[i++] = 3;
      else
        context_attribs[i++] = 2;
      api = GDK_GL_API_GLES;
    }
  else
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("The EGL implementation does not support any allowed APIs"));
      return 0;
    }

  /* Specify the flags */
  context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
  context_attribs[i++] = flags;

  context_attribs[i++] = EGL_NONE;
  g_assert (i < N_EGL_ATTRS);

  GDK_DISPLAY_NOTE (display, OPENGL,
                    g_message ("Creating EGL context version %d.%d (debug:%s, forward:%s, legacy:%s, es:%s)",
                               major, minor,
                               debug_bit ? "yes" : "no",
                               forward_bit ? "yes" : "no",
                               legacy_bit ? "yes" : "no",
                               api == GDK_GL_API_GLES ? "yes" : "no"));

  ctx = eglCreateContext (egl_display,
                          egl_config,
                          share != NULL ? share_priv->egl_context
                          : EGL_NO_CONTEXT,
                          context_attribs);

  /* If context creation failed without the ES bit, let's try again with it */
  if (ctx == NULL && gdk_gl_context_is_api_allowed (context, GDK_GL_API_GLES, NULL) && eglBindAPI (EGL_OPENGL_ES_API))
    {
      i = 0;
      context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION;
      context_attribs[i++] = 2;
      context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION;
      context_attribs[i++] = 0;
      context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
      context_attribs[i++] = flags & ~EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
      context_attribs[i++] = EGL_NONE;
      g_assert (i < N_EGL_ATTRS);

      legacy_bit = FALSE;
      api = GDK_GL_API_GLES;

      GDK_DISPLAY_NOTE (display, OPENGL,
                        g_message ("eglCreateContext failed, switching to OpenGL ES"));
      ctx = eglCreateContext (egl_display,
                              egl_config,
                              share != NULL ? share_priv->egl_context
                              : EGL_NO_CONTEXT,
                              context_attribs);
    }

  /* If context creation failed without the legacy bit, let's try again with it */
  if (ctx == NULL && gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, NULL) && eglBindAPI (EGL_OPENGL_API))
    {
      i = 0;
      context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
      context_attribs[i++] = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;
      context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION;
      context_attribs[i++] = 3;
      context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION;
      context_attribs[i++] = 0;
      context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
      context_attribs[i++] = flags & ~EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
      context_attribs[i++] = EGL_NONE;
      g_assert (i < N_EGL_ATTRS);

      legacy_bit = TRUE;
      api = GDK_GL_API_GL;

      GDK_DISPLAY_NOTE (display, OPENGL,
                        g_message ("eglCreateContext failed, switching to legacy"));
      ctx = eglCreateContext (egl_display,
                              egl_config,
                              share != NULL ? share_priv->egl_context
                              : EGL_NO_CONTEXT,
                              context_attribs);
    }

  if (ctx == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return 0;
    }

  GDK_DISPLAY_NOTE (display, OPENGL, g_message ("Created EGL context[%p]", ctx));

  priv->egl_context = ctx;

  gdk_gl_context_set_is_legacy (context, legacy_bit);

  if (epoxy_has_egl_extension (egl_display, "EGL_KHR_swap_buffers_with_damage"))
    priv->eglSwapBuffersWithDamage = (gpointer)epoxy_eglGetProcAddress ("eglSwapBuffersWithDamageKHR");
  else if (epoxy_has_egl_extension (egl_display, "EGL_EXT_swap_buffers_with_damage"))
    priv->eglSwapBuffersWithDamage = (gpointer)epoxy_eglGetProcAddress ("eglSwapBuffersWithDamageEXT");

  gdk_profiler_end_mark (start_time, "realize GdkWaylandGLContext", NULL);

  return api;
}

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
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
#ifdef HAVE_EGL
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

  if (priv->egl_context && display->have_egl_buffer_age)
    {
      EGLSurface egl_surface;
      int buffer_age = 0;
      egl_surface = gdk_surface_get_egl_surface (surface);
      gdk_gl_context_make_current (context);
      eglQuerySurface (gdk_display_get_egl_display (display), egl_surface,
                       EGL_BUFFER_AGE_EXT, &buffer_age);

      switch (buffer_age)
        {
          case 1:
            return cairo_region_create ();
            break;

          case 2:
            if (context->old_updated_area[0])
              return cairo_region_copy (context->old_updated_area[0]);
            break;

          case 3:
            if (context->old_updated_area[0] &&
                context->old_updated_area[1])
              {
                cairo_region_t *damage = cairo_region_copy (context->old_updated_area[0]);
                cairo_region_union (damage, context->old_updated_area[1]);
                return damage;
              }
            break;

          default:
            ;
        }
    }
#endif

  return cairo_region_create_rectangle (&(GdkRectangle) {
                                            0, 0,
                                            gdk_surface_get_width (surface),
                                            gdk_surface_get_height (surface)
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
gdk_gl_context_real_clear_current (GdkGLContext *context)
{
  GdkDisplay *display = gdk_gl_context_get_display (context);
#ifdef HAVE_EGL
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
gdk_gl_context_real_begin_frame (GdkDrawContext *draw_context,
                                 gboolean        prefers_high_depth,
                                 cairo_region_t *region)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  G_GNUC_UNUSED GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkSurface *surface;
  cairo_region_t *damage;
  int ww, wh;

  surface = gdk_draw_context_get_surface (draw_context);

#ifdef HAVE_EGL
  if (priv->egl_context)
    gdk_surface_ensure_egl_surface (surface, prefers_high_depth);
#endif

  damage = GDK_GL_CONTEXT_GET_CLASS (context)->get_damage (context);

  if (context->old_updated_area[1])
    cairo_region_destroy (context->old_updated_area[1]);
  context->old_updated_area[1] = context->old_updated_area[0];
  context->old_updated_area[0] = cairo_region_copy (region);

  cairo_region_union (region, damage);
  cairo_region_destroy (damage);

  ww = gdk_surface_get_width (surface) * gdk_surface_get_scale_factor (surface);
  wh = gdk_surface_get_height (surface) * gdk_surface_get_scale_factor (surface);

  gdk_gl_context_make_current (context);

  /* Initial setup */
  glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glViewport (0, 0, ww, wh);

#ifdef HAVE_EGL
  if (priv->egl_context && gdk_gl_context_check_version (context, 0, 0, 3, 0))
    glDrawBuffers (1, (GLenum[1]) { gdk_gl_context_get_use_es (context) ? GL_BACK : GL_BACK_LEFT });
#endif
}

static void
gdk_gl_context_real_end_frame (GdkDrawContext *draw_context,
                               cairo_region_t *painted)
{
#ifdef HAVE_EGL
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_surface_get_display (surface);
  EGLSurface egl_surface;

  if (priv->egl_context == NULL)
    return;

  gdk_gl_context_make_current (context);

  egl_surface = gdk_surface_get_egl_surface (surface);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "EGL", "swap buffers");

  if (priv->eglSwapBuffersWithDamage)
    {
      EGLint stack_rects[4 * 4]; /* 4 rects */
      EGLint *heap_rects = NULL;
      int i, j, n_rects = cairo_region_num_rectangles (painted);
      int surface_height = gdk_surface_get_height (surface);
      int scale = gdk_surface_get_scale_factor (surface);
      EGLint *rects;

      if (n_rects < G_N_ELEMENTS (stack_rects) / 4)
        rects = (EGLint *)&stack_rects;
      else
        heap_rects = rects = g_new (EGLint, n_rects * 4);

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;

          cairo_region_get_rectangle (painted, i, &rect);
          rects[j++] = rect.x * scale;
          rects[j++] = (surface_height - rect.height - rect.y) * scale;
          rects[j++] = rect.width * scale;
          rects[j++] = rect.height * scale;
        }
      priv->eglSwapBuffersWithDamage (gdk_display_get_egl_display (display), egl_surface, rects, n_rects);
      g_free (heap_rects);
    }
  else
    eglSwapBuffers (gdk_display_get_egl_display (display), egl_surface);
#endif
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
  klass->get_default_framebuffer = gdk_gl_context_real_get_default_framebuffer;

  draw_context_class->begin_frame = gdk_gl_context_real_begin_frame;
  draw_context_class->end_frame = gdk_gl_context_real_end_frame;
  draw_context_class->surface_resized = gdk_gl_context_surface_resized;

  /**
   * GdkGLContext:shared-context: (attributes org.gtk.Property.get=gdk_gl_context_get_shared_context)
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
    g_param_spec_object ("shared-context",
                         P_("Shared context"),
                         P_("The GL context this context shares data with"),
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_DEPRECATED);

  /**
   * GdkGLContext:allowed-apis: (attributes org.gtk.Property.get=gdk_gl_context_get_allowed_apis org.gtk.Property.gdk_gl_context_set_allowed_apis)
   *
   * The allowed APIs.
   *
   * Since: 4.6
   */
  properties[PROP_ALLOWED_APIS] =
    g_param_spec_flags ("allowed-apis",
                        P_("Allowed APIs"),
                        P_("The list of allowed APIs for this context"),
                        GDK_TYPE_GL_API,
                        DEFAULT_ALLOWED_APIS,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkGLContext:api: (attributes org.gtk.Property.get=gdk_gl_context_get_api)
   *
   * The API currently in use.
   *
   * Since: 4.6
   */
  properties[PROP_API] =
    g_param_spec_flags ("api",
                        P_("API"),
                        P_("The API currently in use"),
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
                    GdkSurface *surface)
{
  GdkGLContext *shared;

  g_assert (surface == NULL || display == gdk_surface_get_display (surface));

  /* assert gdk_display_prepare_gl() had been called */
  shared = gdk_display_get_gl_context (display);
  g_assert (shared);

  return g_object_new (G_OBJECT_TYPE (shared),
                       "display", display,
                       "surface", surface,
                       NULL);
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
gdk_gl_context_has_unpack_subimage (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  return priv->has_unpack_subimage;
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
 * The `GdkGLContext` must not be realized or made current prior to calling
 * this function.
 */
void
gdk_gl_context_set_required_version (GdkGLContext *context,
                                     int           major,
                                     int           minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  gboolean force_gles = FALSE;
  int version, min_ver;
#ifdef G_ENABLE_DEBUG
  GdkDisplay *display;
#endif

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!gdk_gl_context_is_realized (context));

  /* this will take care of the default */
  if (major == 0 && minor == 0)
    {
      priv->major = 0;
      priv->minor = 0;
      return;
    }

  version = (major * 100) + minor;

#ifdef G_ENABLE_DEBUG
  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  force_gles = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES);
#endif
  /* Enforce a minimum context version number of 3.2 for desktop GL,
   * and 2.0 for GLES
   */
  if (gdk_gl_context_get_use_es (context) || force_gles)
    min_ver = 200;
  else
    min_ver = 302;

  if (version < min_ver)
    {
      g_warning ("gdk_gl_context_set_required_version - GL context versions less than 3.2 are not supported.");
      version = min_ver;
    }
  priv->major = version / 100;
  priv->minor = version % 100;
}

gboolean
gdk_gl_context_check_version (GdkGLContext *self,
                              int           required_gl_major,
                              int           required_gl_minor,
                              int           required_gles_major,
                              int           required_gles_minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self), FALSE);
  g_return_val_if_fail (required_gl_minor < 10, FALSE);
  g_return_val_if_fail (required_gles_minor < 10, FALSE);

  if (!gdk_gl_context_is_realized (self))
    return FALSE;

  switch (priv->api)
    {
    case GDK_GL_API_GL:
      return priv->gl_version >= required_gl_major * 10 + required_gl_minor;

    case GDK_GL_API_GLES:
      return priv->gl_version >= required_gles_major * 10 + required_gles_minor;

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
 * Retrieves required OpenGL version.
 *
 * See [method@Gdk.GLContext.set_required_version].
 */
void
gdk_gl_context_get_required_version (GdkGLContext *context,
                                     int          *major,
                                     int          *minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  gboolean force_gles = FALSE;
  GdkDisplay *display;
  int default_major, default_minor;
  int maj, min;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

#ifdef G_ENABLE_DEBUG
  force_gles = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES);
#endif

  /* libANGLE on Windows at least requires GLES 3.0+ */
  if (display->have_egl_win32_libangle)
    force_gles = TRUE;

  /* Default fallback values for uninitialised contexts; we
   * enforce a context version number of 3.2 for desktop GL,
   * and 2.0 for GLES
   */
  if (gdk_gl_context_get_use_es (context) || force_gles)
    {
      default_major = display->have_egl_win32_libangle ? 3 : 2;
      default_minor = 0;
    }
  else
    {
      default_major = 3;
      default_minor = 2;
    }

  if (priv->major > 0)
    maj = priv->major;
  else
    maj = default_major;

  if (priv->minor > 0)
    min = priv->minor;
  else
    min = default_minor;

void
gdk_gl_context_get_clipped_version (GdkGLContext *context,
                                    int           min_major,
                                    int           min_minor,
                                    int          *major,
                                    int          *minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  int maj = min_major, min = min_minor;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  if (priv->major > maj || (priv->major == maj && priv->minor > min))
    {
      maj = priv->major;
      min = priv->minor;
    }

  if (major != NULL)
    *major = maj;
  if (minor != NULL)
    *minor = min;
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
 * gdk_gl_context_set_allowed_apis: (attributes org.gtk.Method.set_property=allowed-apis)
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
 * gdk_gl_context_get_allowed_apis: (attributes org.gtk.Method.get_property=allowed-apis)
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
 * gdk_gl_context_get_api: (attributes org.gtk.Method.get_property=api)
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

  if (GDK_DISPLAY_DEBUG_CHECK (gdk_gl_context_get_display (self), GL_GLES))
    {
      if (!(api & GDK_GL_API_GLES))
        {
          g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                               _("Anything but OpenGL ES disabled via GDK_DEBUG"));
          return FALSE;
        }
    }

  if (priv->allowed_apis & api)
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
    g_object_notify_by_pspec (G_OBJECT (context), properties[PROP_API]);

  return priv->api;
}

static void
gdk_gl_context_check_extensions (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  gboolean gl_debug = FALSE;
#ifdef G_ENABLE_DEBUG
  GdkDisplay *display;
#endif

  if (!gdk_gl_context_is_realized (context))
    return;

  if (priv->extensions_checked)
    return;

  priv->gl_version = epoxy_gl_version ();

  priv->has_debug_output = epoxy_has_gl_extension ("GL_ARB_debug_output") ||
                           epoxy_has_gl_extension ("GL_KHR_debug");

#ifdef G_ENABLE_DEBUG
  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  gl_debug = GDK_DISPLAY_DEBUG_CHECK (display, GL_DEBUG);
#endif

  if (priv->has_debug_output
#ifndef G_ENABLE_CONSISTENCY_CHECKS
      && gl_debug
#endif
      )
    {
      gdk_gl_context_make_current (context);
      glEnable (GL_DEBUG_OUTPUT);
      glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback (gl_debug_message_callback, NULL);
    }

  if (gdk_gl_context_get_use_es (context))
    {
      priv->has_unpack_subimage = epoxy_has_gl_extension ("GL_EXT_unpack_subimage");
      priv->has_khr_debug = epoxy_has_gl_extension ("GL_KHR_debug");
    }
  else
    {
      priv->has_unpack_subimage = TRUE;
      priv->has_khr_debug = epoxy_has_gl_extension ("GL_KHR_debug");

      /* We asked for a core profile, but we didn't get one, so we're in legacy mode */
      if (priv->gl_version < 32)
        priv->is_legacy = TRUE;
    }

  if (priv->has_khr_debug && gl_debug)
    {
      priv->use_khr_debug = TRUE;
      glGetIntegerv (GL_MAX_LABEL_LENGTH, &priv->max_debug_label_length);
    }

  GDK_DISPLAY_NOTE (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)), OPENGL,
    g_message ("%s version: %d.%d (%s)\n"
                       "* GLSL version: %s\n"
                       "* Extensions checked:\n"
                       " - GL_KHR_debug: %s\n"
                       " - GL_EXT_unpack_subimage: %s",
                       gdk_gl_context_get_use_es (context) ? "OpenGL ES" : "OpenGL",
                       priv->gl_version / 10, priv->gl_version % 10,
                       priv->is_legacy ? "legacy" : "core",
                       glGetString (GL_SHADING_LANGUAGE_VERSION),
                       priv->has_khr_debug ? "yes" : "no",
                       priv->has_unpack_subimage ? "yes" : "no"));

  priv->extensions_checked = TRUE;
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
  MaskedContext *current, *masked_context;
  gboolean surfaceless;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  surfaceless = !gdk_draw_context_is_in_frame (GDK_DRAW_CONTEXT (context));
  masked_context = mask_context (context, surfaceless);

  current = g_private_get (&thread_current_context);
  if (current == masked_context)
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
 * gdk_gl_context_get_shared_context: (attributes org.gtk.Method.get_property=shared-context)
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
 *
 * If the @context has never been made current, the version cannot
 * be known and it will return 0 for both @major and @minor.
 *
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
    *major = priv->gl_version / 10;
  if (minor != NULL)
    *minor = priv->gl_version % 10;
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
 **/
void
gdk_gl_context_clear_current_if_surface (GdkSurface *surface)
{
  MaskedContext *current;

  current = g_private_get (&thread_current_context);
  if (current != NULL && !mask_is_surfaceless (current))
    {
      GdkGLContext *context = unmask_context (current);

      if (gdk_gl_context_get_surface (context) != surface)
        return;

      if (GDK_GL_CONTEXT_GET_CLASS (context)->clear_current (context))
        g_private_replace (&thread_current_context, NULL);
    }
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

  current = g_private_get (&thread_current_context);

  return unmask_context (current);
}

gboolean
gdk_gl_context_has_debug (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  return priv->debug_enabled || priv->use_khr_debug;
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
  if (the_gl_backend_type == GDK_GL_NONE ||
      the_gl_backend_type == backend_type)
    return TRUE;

  g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
               /* translators: This is about OpenGL backend names, like
                * "Trying to use X11 GLX, but EGL is already in use" */
               _("Trying to use %s, but %s is already in use"),
               gl_backend_names[backend_type],
               gl_backend_names[the_gl_backend_type]);
  return FALSE;
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
      GDK_NOTE (OPENGL, g_print ("Using OpenGL backend %s\n", gl_backend_names[the_gl_backend_type]));
      GDK_NOTE (MISC, g_message ("Using Opengl backend %s", gl_backend_names[the_gl_backend_type]));
    }

  g_assert (the_gl_backend_type == backend_type);
}
