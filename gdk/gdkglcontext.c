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
 * SECTION:gdkglcontext
 * @Title: GdkGLContext
 * @Short_description: OpenGL draw context
 *
 * #GdkGLContext is an object representing the platform-specific
 * OpenGL draw context.
 *
 * #GdkGLContexts are created for a #GdkSurface using
 * gdk_surface_create_gl_context(), and the context will match the
 * the characteristics of the surface.
 *
 * A #GdkGLContext is not tied to any particular normal framebuffer.
 * For instance, it cannot draw to the #GdkSurface back buffer. The GDK
 * repaint system is in full control of the painting to that. Instead,
 * you can create render buffers or textures and use gdk_cairo_draw_from_gl()
 * in the draw function of your widget to draw them. Then GDK will handle
 * the integration of your rendering with that of other widgets.
 *
 * Support for #GdkGLContext is platform-specific, context creation
 * can fail, returning %NULL context.
 *
 * A #GdkGLContext has to be made "current" in order to start using
 * it, otherwise any OpenGL call will be ignored.
 *
 * ## Creating a new OpenGL context ##
 *
 * In order to create a new #GdkGLContext instance you need a
 * #GdkSurface, which you typically get during the realize call
 * of a widget.
 *
 * A #GdkGLContext is not realized until either gdk_gl_context_make_current(),
 * or until it is realized using gdk_gl_context_realize(). It is possible to
 * specify details of the GL context like the OpenGL version to be used, or
 * whether the GL context should have extra state validation enabled after
 * calling gdk_surface_create_gl_context() by calling gdk_gl_context_realize().
 * If the realization fails you have the option to change the settings of the
 * #GdkGLContext and try again.
 *
 * ## Using a GdkGLContext ##
 *
 * You will need to make the #GdkGLContext the current context
 * before issuing OpenGL calls; the system sends OpenGL commands to
 * whichever context is current. It is possible to have multiple
 * contexts, so you always need to ensure that the one which you
 * want to draw with is the current one before issuing commands:
 *
 * |[<!-- language="C" -->
 *   gdk_gl_context_make_current (context);
 * ]|
 *
 * You can now perform your drawing using OpenGL commands.
 *
 * You can check which #GdkGLContext is the current one by using
 * gdk_gl_context_get_current(); you can also unset any #GdkGLContext
 * that is currently set by calling gdk_gl_context_clear_current().
 */

/**
 * GdkGLContext:
 *
 * The GdkGLContext struct contains only private fields and
 * should not be accessed directly.
 */

#include "config.h"

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkinternals.h"

#include "gdkintl.h"
#include "gdk-private.h"

#include <epoxy/gl.h>

typedef struct {
  GdkGLContext *shared_context;

  int major;
  int minor;
  int gl_version;

  guint realized : 1;
  guint use_texture_rectangle : 1;
  guint has_gl_framebuffer_blit : 1;
  guint has_frame_terminator : 1;
  guint has_khr_debug : 1;
  guint use_khr_debug : 1;
  guint has_unpack_subimage : 1;
  guint has_debug_output : 1;
  guint extensions_checked : 1;
  guint debug_enabled : 1;
  guint forward_compatible : 1;
  guint is_legacy : 1;

  int use_es;

  int max_debug_label_length;

  GdkGLContextPaintData *paint_data;
} GdkGLContextPrivate;

enum {
  PROP_0,

  PROP_SHARED_CONTEXT,

  LAST_PROP
};

static GParamSpec *obj_pspecs[LAST_PROP] = { NULL, };

G_DEFINE_QUARK (gdk-gl-error-quark, gdk_gl_error)

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkGLContext, gdk_gl_context, GDK_TYPE_DRAW_CONTEXT)

static GPrivate thread_current_context = G_PRIVATE_INIT (g_object_unref);

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
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkGLContext *current;

  gdk_gl_context_clear_old_updated_area (context);

  current = g_private_get (&thread_current_context);
  if (current == context)
    g_private_replace (&thread_current_context, NULL);

  g_clear_object (&priv->shared_context);

  G_OBJECT_CLASS (gdk_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_gl_context_finalize (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_clear_pointer (&priv->paint_data, g_free);
  G_OBJECT_CLASS (gdk_gl_context_parent_class)->finalize (gobject);
}

static void
gdk_gl_context_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private ((GdkGLContext *) gobject);

  switch (prop_id)
    {
    case PROP_SHARED_CONTEXT:
      {
        GdkGLContext *context = g_value_get_object (value);

        if (context != NULL)
          priv->shared_context = g_object_ref (context);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_gl_context_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private ((GdkGLContext *) gobject);

  switch (prop_id)
    {
    case PROP_SHARED_CONTEXT:
      g_value_set_object (value, priv->shared_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

void
gdk_gl_context_upload_texture (GdkGLContext    *context,
                               const guchar    *data,
                               int              width,
                               int              height,
                               int              stride,
                               guint            texture_target)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  /* GL_UNPACK_ROW_LENGTH is available on desktop GL, OpenGL ES >= 3.0, or if
   * the GL_EXT_unpack_subimage extension for OpenGL ES 2.0 is available
   */
  if (!priv->use_es ||
      (priv->use_es && (priv->gl_version >= 30 || priv->has_unpack_subimage)))
    {
      glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, stride / 4);

      if (priv->use_es)
        glTexImage2D (texture_target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      data);
      else
        glTexImage2D (texture_target, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                      data);

      glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    }
  else
    {
      int i;

      if (priv->use_es)
        {
          glTexImage2D (texture_target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

          for (i = 0; i < height; i++)
            glTexSubImage2D (texture_target, 0, 0, i, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, data + (i * stride));
        }
      else
        {
          glTexImage2D (texture_target, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

          for (i = 0; i < height; i++)
            glTexSubImage2D (texture_target, 0, 0, i, width, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data + (i * stride));
        }
    }
}

static gboolean
gdk_gl_context_real_realize (GdkGLContext  *self,
                             GError       **error)
{
  g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                       "The current backend does not support OpenGL");

  return FALSE;
}

static cairo_region_t *
gdk_gl_context_real_get_damage (GdkGLContext *context)
{
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));

  return cairo_region_create_rectangle (&(GdkRectangle) {
                                            0, 0,
                                            gdk_surface_get_width (surface),
                                            gdk_surface_get_height (surface)
                                        });
}

static void
gdk_gl_context_real_begin_frame (GdkDrawContext *draw_context,
                                 cairo_region_t *region)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface;
  GdkGLContext *shared;
  cairo_region_t *damage;
  int ww, wh;

  shared = gdk_gl_context_get_shared_context (context);
  if (shared)
    {
      GDK_DRAW_CONTEXT_GET_CLASS (GDK_DRAW_CONTEXT (shared))->begin_frame (GDK_DRAW_CONTEXT (shared), region);
      return;
    }

  damage = GDK_GL_CONTEXT_GET_CLASS (context)->get_damage (context);

  if (context->old_updated_area[1])
    cairo_region_destroy (context->old_updated_area[1]);
  context->old_updated_area[1] = context->old_updated_area[0];
  context->old_updated_area[0] = cairo_region_copy (region);

  cairo_region_union (region, damage);
  cairo_region_destroy (damage);

  surface = gdk_draw_context_get_surface (draw_context);
  ww = gdk_surface_get_width (surface) * gdk_surface_get_scale_factor (surface);
  wh = gdk_surface_get_height (surface) * gdk_surface_get_scale_factor (surface);

  gdk_gl_context_make_current (context);

  /* Initial setup */
  glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
  glDisable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glViewport (0, 0, ww, wh);
}

static void
gdk_gl_context_real_end_frame (GdkDrawContext *draw_context,
                               cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkGLContext *shared;

  shared = gdk_gl_context_get_shared_context (context);
  if (shared)
    {
      GDK_DRAW_CONTEXT_GET_CLASS (GDK_DRAW_CONTEXT (shared))->end_frame (GDK_DRAW_CONTEXT (shared), painted);
      return;
    }
}

static void
gdk_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);

  gdk_gl_context_clear_old_updated_area (context);
}

static void
gdk_gl_context_class_init (GdkGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  klass->realize = gdk_gl_context_real_realize;
  klass->get_damage = gdk_gl_context_real_get_damage;

  draw_context_class->begin_frame = gdk_gl_context_real_begin_frame;
  draw_context_class->end_frame = gdk_gl_context_real_end_frame;
  draw_context_class->surface_resized = gdk_gl_context_surface_resized;

  /**
   * GdkGLContext:shared-context:
   *
   * The #GdkGLContext that this context is sharing data with, or %NULL
   */
  obj_pspecs[PROP_SHARED_CONTEXT] =
    g_param_spec_object ("shared-context",
                         P_("Shared context"),
                         P_("The GL context this context shares data with"),
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  gobject_class->set_property = gdk_gl_context_set_property;
  gobject_class->get_property = gdk_gl_context_get_property;
  gobject_class->dispose = gdk_gl_context_dispose;
  gobject_class->finalize = gdk_gl_context_finalize;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_pspecs);
}

static void
gdk_gl_context_init (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  priv->use_es = -1;
}

GdkGLContextPaintData *
gdk_gl_context_get_paint_data (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->paint_data == NULL)
    {
      priv->paint_data = g_new0 (GdkGLContextPaintData, 1);
      priv->paint_data->is_legacy = priv->is_legacy;
      priv->paint_data->use_es = priv->use_es;
    }

  return priv->paint_data;
}

gboolean
gdk_gl_context_use_texture_rectangle (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  return priv->use_texture_rectangle;
}

gboolean
gdk_gl_context_has_framebuffer_blit (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  return priv->has_gl_framebuffer_blit;
}

gboolean
gdk_gl_context_has_frame_terminator (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  return priv->has_frame_terminator;
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
  gchar *message;
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
  gchar *message;
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

/**
 * gdk_gl_context_set_debug_enabled:
 * @context: a #GdkGLContext
 * @enabled: whether to enable debugging in the context
 *
 * Sets whether the #GdkGLContext should perform extra validations and
 * run time checking. This is useful during development, but has
 * additional overhead.
 *
 * The #GdkGLContext must not be realized or made current prior to
 * calling this function.
 */
void
gdk_gl_context_set_debug_enabled (GdkGLContext *context,
                                  gboolean      enabled)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!priv->realized);

  enabled = !!enabled;

  priv->debug_enabled = enabled;
}

/**
 * gdk_gl_context_get_debug_enabled:
 * @context: a #GdkGLContext
 *
 * Retrieves the value set using gdk_gl_context_set_debug_enabled().
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
 * @context: a #GdkGLContext
 * @compatible: whether the context should be forward compatible
 *
 * Sets whether the #GdkGLContext should be forward compatible.
 *
 * Forward compatibile contexts must not support OpenGL functionality that
 * has been marked as deprecated in the requested version; non-forward
 * compatible contexts, on the other hand, must support both deprecated and
 * non deprecated functionality.
 *
 * The #GdkGLContext must not be realized or made current prior to calling
 * this function.
 */
void
gdk_gl_context_set_forward_compatible (GdkGLContext *context,
                                       gboolean      compatible)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!priv->realized);

  compatible = !!compatible;

  priv->forward_compatible = compatible;
}

/**
 * gdk_gl_context_get_forward_compatible:
 * @context: a #GdkGLContext
 *
 * Retrieves the value set using gdk_gl_context_set_forward_compatible().
 *
 * Returns: %TRUE if the context should be forward compatible
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
 * @context: a #GdkGLContext
 * @major: the major version to request
 * @minor: the minor version to request
 *
 * Sets the major and minor version of OpenGL to request.
 *
 * Setting @major and @minor to zero will use the default values.
 *
 * The #GdkGLContext must not be realized or made current prior to calling
 * this function.
 */
void
gdk_gl_context_set_required_version (GdkGLContext *context,
                                     int           major,
                                     int           minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display;
  int version, min_ver;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!priv->realized);

  /* this will take care of the default */
  if (major == 0 && minor == 0)
    {
      priv->major = 0;
      priv->minor = 0;
      return;
    }

  /* Enforce a minimum context version number of 3.2 */
  version = (major * 100) + minor;

  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

  if (priv->use_es > 0 || GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES))
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

/**
 * gdk_gl_context_get_required_version:
 * @context: a #GdkGLContext
 * @major: (out) (nullable): return location for the major version to request
 * @minor: (out) (nullable): return location for the minor version to request
 *
 * Retrieves the major and minor version requested by calling
 * gdk_gl_context_set_required_version().
 */
void
gdk_gl_context_get_required_version (GdkGLContext *context,
                                     int          *major,
                                     int          *minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display;
  int default_major, default_minor;
  int maj, min;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

  if (priv->use_es > 0 || GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES))
    {
      default_major = 2;
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

  if (major != NULL)
    *major = maj;
  if (minor != NULL)
    *minor = min;
}

/**
 * gdk_gl_context_is_legacy:
 * @context: a #GdkGLContext
 *
 * Whether the #GdkGLContext is in legacy mode or not.
 *
 * The #GdkGLContext must be realized before calling this function.
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
  g_return_val_if_fail (priv->realized, FALSE);

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
 * gdk_gl_context_set_use_es:
 * @context: a #GdkGLContext:
 * @use_es: whether the context should use OpenGL ES instead of OpenGL,
 *   or -1 to allow auto-detection
 *
 * Requests that GDK create an OpenGL ES context instead of an OpenGL one,
 * if the platform and windowing system allows it.
 *
 * The @context must not have been realized.
 *
 * By default, GDK will attempt to automatically detect whether the
 * underlying GL implementation is OpenGL or OpenGL ES once the @context
 * is realized.
 *
 * You should check the return value of gdk_gl_context_get_use_es() after
 * calling gdk_gl_context_realize() to decide whether to use the OpenGL or
 * OpenGL ES API, extensions, or shaders.
 */
void
gdk_gl_context_set_use_es (GdkGLContext *context,
                           int           use_es)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!priv->realized);

  if (priv->use_es != use_es)
    priv->use_es = use_es;
}

/**
 * gdk_gl_context_get_use_es:
 * @context: a #GdkGLContext
 *
 * Checks whether the @context is using an OpenGL or OpenGL ES profile.
 *
 * Returns: %TRUE if the #GdkGLContext is using an OpenGL ES profile
 */
gboolean
gdk_gl_context_get_use_es (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  if (!priv->realized)
    return FALSE;

  return priv->use_es > 0;
}

#ifdef G_ENABLE_CONSISTENCY_CHECKS
static void
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
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      message_severity = "Medium";
      break;
    case GL_DEBUG_SEVERITY_LOW:
      message_severity = "Low";
      break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      message_severity = "Notification";
      break;
    default:
      message_severity = "Unknown";
    }

  g_warning ("OPENGL:\n    Source: %s\n    Type: %s\n    Severity: %s\n    Message: %s",
             message_source, message_type, message_severity, message);
}
#endif

/**
 * gdk_gl_context_realize:
 * @context: a #GdkGLContext
 * @error: return location for a #GError
 *
 * Realizes the given #GdkGLContext.
 *
 * It is safe to call this function on a realized #GdkGLContext.
 *
 * Returns: %TRUE if the context is realized
 */
gboolean
gdk_gl_context_realize (GdkGLContext  *context,
                        GError       **error)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  if (priv->realized)
    return TRUE;

  priv->realized = GDK_GL_CONTEXT_GET_CLASS (context)->realize (context, error);

  return priv->realized;
}

static void
gdk_gl_context_check_extensions (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkDisplay *display;
  gboolean has_npot, has_texture_rectangle;

  if (!priv->realized)
    return;

  if (priv->extensions_checked)
    return;

  priv->gl_version = epoxy_gl_version ();

  if (priv->use_es < 0)
    priv->use_es = !epoxy_is_desktop_gl ();

  priv->has_debug_output = epoxy_has_gl_extension ("GL_ARB_debug_output");

#ifdef G_ENABLE_CONSISTENCY_CHECKS
  if (priv->has_debug_output)
    {
      gdk_gl_context_make_current (context);
      glEnable (GL_DEBUG_OUTPUT);
      glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback (gl_debug_message_callback, NULL);
    }
#endif

  if (priv->use_es)
    {
      has_npot = priv->gl_version >= 20;
      has_texture_rectangle = FALSE;

      /* This should check for GL_NV_framebuffer_blit - see extension at:
       *
       * https://www.khronos.org/registry/gles/extensions/NV/NV_framebuffer_blit.txt
       */
      priv->has_gl_framebuffer_blit = FALSE;

      /* No OES version */
      priv->has_frame_terminator = FALSE;

      priv->has_unpack_subimage = epoxy_has_gl_extension ("GL_EXT_unpack_subimage");
      priv->has_khr_debug = epoxy_has_gl_extension ("GL_KHR_debug");
    }
  else
    {
      has_npot = epoxy_has_gl_extension ("GL_ARB_texture_non_power_of_two");
      has_texture_rectangle = epoxy_has_gl_extension ("GL_ARB_texture_rectangle");

      priv->has_gl_framebuffer_blit = epoxy_has_gl_extension ("GL_EXT_framebuffer_blit");
      priv->has_frame_terminator = epoxy_has_gl_extension ("GL_GREMEDY_frame_terminator");
      priv->has_unpack_subimage = TRUE;
      priv->has_khr_debug = epoxy_has_gl_extension ("GL_KHR_debug");

      /* We asked for a core profile, but we didn't get one, so we're in legacy mode */
      if (priv->gl_version < 32)
        priv->is_legacy = TRUE;
    }

  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));

  if (priv->has_khr_debug && GDK_DISPLAY_DEBUG_CHECK (display, GL_DEBUG))
    {
      priv->use_khr_debug = TRUE;
      glGetIntegerv (GL_MAX_LABEL_LENGTH, &priv->max_debug_label_length);
    }
  if (!priv->use_es && GDK_DISPLAY_DEBUG_CHECK (display, GL_TEXTURE_RECT))
    priv->use_texture_rectangle = TRUE;
  else if (has_npot)
    priv->use_texture_rectangle = FALSE;
  else if (has_texture_rectangle)
    priv->use_texture_rectangle = TRUE;
  else
    g_warning ("GL implementation doesn't support any form of non-power-of-two textures");

  GDK_DISPLAY_NOTE (display, OPENGL,
    g_message ("%s version: %d.%d (%s)\n"
                       "* GLSL version: %s\n"
                       "* Extensions checked:\n"
                       " - GL_ARB_texture_non_power_of_two: %s\n"
                       " - GL_ARB_texture_rectangle: %s\n"
                       " - GL_EXT_framebuffer_blit: %s\n"
                       " - GL_GREMEDY_frame_terminator: %s\n"
                       " - GL_KHR_debug: %s\n"
                       "* Using texture rectangle: %s",
                       priv->use_es ? "OpenGL ES" : "OpenGL",
                       priv->gl_version / 10, priv->gl_version % 10,
                       priv->is_legacy ? "legacy" : "core",
                       glGetString (GL_SHADING_LANGUAGE_VERSION),
                       has_npot ? "yes" : "no",
                       has_texture_rectangle ? "yes" : "no",
                       priv->has_gl_framebuffer_blit ? "yes" : "no",
                       priv->has_frame_terminator ? "yes" : "no",
                       priv->has_khr_debug ? "yes" : "no",
                       priv->use_texture_rectangle ? "yes" : "no"));

  priv->extensions_checked = TRUE;
}

/**
 * gdk_gl_context_make_current:
 * @context: a #GdkGLContext
 *
 * Makes the @context the current one.
 */
void
gdk_gl_context_make_current (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkGLContext *current;

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  current = g_private_get (&thread_current_context);
  if (current == context)
    return;

  /* we need to realize the GdkGLContext if it wasn't explicitly realized */
  if (!priv->realized)
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

  if (gdk_display_make_gl_context_current (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context)), context))
    {
      g_private_replace (&thread_current_context, g_object_ref (context));
      gdk_gl_context_check_extensions (context);
    }
}

/**
 * gdk_gl_context_get_display:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkDisplay the @context is created for
 *
 * Returns: (nullable) (transfer none): a #GdkDisplay or %NULL
 */
GdkDisplay *
gdk_gl_context_get_display (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
}

/**
 * gdk_gl_context_get_surface:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkSurface used by the @context.
 *
 * Returns: (nullable) (transfer none): a #GdkSurface or %NULL
 */
GdkSurface *
gdk_gl_context_get_surface (GdkGLContext *context)
{
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
}

/**
 * gdk_gl_context_get_shared_context:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkGLContext that this @context share data with.
 *
 * Returns: (nullable) (transfer none): a #GdkGLContext or %NULL
 */
GdkGLContext *
gdk_gl_context_get_shared_context (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->shared_context;
}

/**
 * gdk_gl_context_get_version:
 * @context: a #GdkGLContext
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
  g_return_if_fail (priv->realized);

  if (major != NULL)
    *major = priv->gl_version / 10;
  if (minor != NULL)
    *minor = priv->gl_version % 10;
}

/**
 * gdk_gl_context_clear_current:
 *
 * Clears the current #GdkGLContext.
 *
 * Any OpenGL call after this function returns will be ignored
 * until gdk_gl_context_make_current() is called.
 */
void
gdk_gl_context_clear_current (void)
{
  GdkGLContext *current;

  current = g_private_get (&thread_current_context);
  if (current != NULL)
    {
      if (gdk_display_make_gl_context_current (gdk_draw_context_get_display (GDK_DRAW_CONTEXT (current)), NULL))
        g_private_replace (&thread_current_context, NULL);
    }
}

/**
 * gdk_gl_context_get_current:
 *
 * Retrieves the current #GdkGLContext.
 *
 * Returns: (nullable) (transfer none): the current #GdkGLContext, or %NULL
 */
GdkGLContext *
gdk_gl_context_get_current (void)
{
  GdkGLContext *current;

  current = g_private_get (&thread_current_context);

  return current;
}
