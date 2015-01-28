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
 * @Short_description: OpenGL context
 *
 * #GdkGLContext is an object representing the platform-specific
 * OpenGL drawing context.
 *
 * #GdkGLContexts are created for a #GdkWindow using
 * gdk_window_create_gl_context(), and the context will match
 * the #GdkVisual of the window.
 *
 * A #GdkGLContext is not tied to any particular normal framebuffer.
 * For instance, it cannot draw to the #GdkWindow back buffer. The GDK
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
 * #GdkWindow, which you typically get during the realize call
 * of a widget.
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

#include "config.h"

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkinternals.h"

#include "gdkintl.h"
#include "gdk-private.h"

#include <epoxy/gl.h>

typedef struct {
  GdkDisplay *display;
  GdkWindow *window;
  GdkGLContext *shared_context;
  GdkGLProfile profile;

  int major;
  int minor;

  guint realized : 1;
  guint use_texture_rectangle : 1;
  guint has_gl_framebuffer_blit : 1;
  guint has_frame_terminator : 1;
  guint extensions_checked : 1;
  guint debug_enabled : 1;
  guint forward_compatible : 1;

  GdkGLContextPaintData *paint_data;
} GdkGLContextPrivate;

enum {
  PROP_0,

  PROP_DISPLAY,
  PROP_WINDOW,
  PROP_PROFILE,
  PROP_SHARED_CONTEXT,

  LAST_PROP
};

static GParamSpec *obj_pspecs[LAST_PROP] = { NULL, };

G_DEFINE_QUARK (gdk-gl-error-quark, gdk_gl_error)

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkGLContext, gdk_gl_context, G_TYPE_OBJECT)

static GPrivate thread_current_context = G_PRIVATE_INIT (g_object_unref);

static void
gdk_gl_context_dispose (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  GdkGLContext *current;

  current = g_private_get (&thread_current_context);
  if (current == context)
    g_private_replace (&thread_current_context, NULL);

  g_clear_object (&priv->display);
  g_clear_object (&priv->window);
  g_clear_object (&priv->shared_context);

  G_OBJECT_CLASS (gdk_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_gl_context_finalize (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_clear_pointer (&priv->paint_data, g_free);
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
    case PROP_DISPLAY:
      {
        GdkDisplay *display = g_value_get_object (value);

        if (display)
          g_object_ref (display);

        if (priv->display)
          g_object_unref (priv->display);

        priv->display = display;
      }
      break;

    case PROP_WINDOW:
      {
        GdkWindow *window = g_value_get_object (value);

        if (window)
          g_object_ref (window);

        if (priv->window)
          g_object_unref (priv->window);

        priv->window = window;
      }
      break;

    case PROP_SHARED_CONTEXT:
      {
        GdkGLContext *context = g_value_get_object (value);

        if (context != NULL)
          priv->shared_context = g_object_ref (context);
      }
      break;

    case PROP_PROFILE:
      priv->profile = g_value_get_enum (value);
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
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;

    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
      break;

    case PROP_SHARED_CONTEXT:
      g_value_set_object (value, priv->shared_context);
      break;

    case PROP_PROFILE:
      g_value_set_enum (value, priv->profile);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

/* Default implementation of upload_texture() for gdk_gl_texture_from_surface() */
static void
gdk_gl_context_upload_texture (GdkGLContext    *context,
                               cairo_surface_t *image_surface,
                               int              width,
                               int              height,
                               guint            texture_target)
{
  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, cairo_image_surface_get_stride (image_surface)/4);
  glTexImage2D (texture_target, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                cairo_image_surface_get_data (image_surface));
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
}

static void
gdk_gl_context_class_init (GdkGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /**
   * GdkGLContext:display:
   *
   * The #GdkWindow the gl context is bound to.
   *
   * Since: 3.16
   */
  obj_pspecs[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         P_("Display"),
                         P_("The GDK display the context is from"),
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:window:
   *
   * The #GdkWindow the gl context is bound to.
   *
   * Since: 3.16
   */
  obj_pspecs[PROP_WINDOW] =
    g_param_spec_object ("window",
                         P_("Window"),
                         P_("The GDK window bound to the GL context"),
                         GDK_TYPE_WINDOW,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:profile:
   *
   * The #GdkGLProfile of the context
   *
   * Since: 3.16
   */
  obj_pspecs[PROP_PROFILE] =
    g_param_spec_enum ("profile",
                       P_("Profile"),
                       P_("The GL profile the context was created for"),
                       GDK_TYPE_GL_PROFILE,
                       GDK_GL_PROFILE_DEFAULT,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:shared-context:
   *
   * The #GdkGLContext that this context is sharing data with, or #NULL
   *
   * Since: 3.16
   */
  obj_pspecs[PROP_SHARED_CONTEXT] =
    g_param_spec_object ("shared-context",
                         P_("Shared context"),
                         P_("The GL context this context share data with"),
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  gobject_class->set_property = gdk_gl_context_set_property;
  gobject_class->get_property = gdk_gl_context_get_property;
  gobject_class->dispose = gdk_gl_context_dispose;
  gobject_class->finalize = gdk_gl_context_finalize;

  /* Default Implementation of upload_texture() for gdk_gl_texture_from_surface() */
  klass->upload_texture = gdk_gl_context_upload_texture;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_pspecs);
}

static void
gdk_gl_context_init (GdkGLContext *self)
{
}

/*< private >
 * gdk_gl_context_end_frame:
 * @context: a #GdkGLContext
 * @painted: The area that has been redrawn this frame
 * @damage: The area that we know is actually different from the last frame
 *
 * Copies the back buffer to the front buffer.
 *
 * This function may call `glFlush()` implicitly before returning; it
 * is not recommended to call `glFlush()` explicitly before calling
 * this function.
 *
 * Since: 3.16
 */
void
gdk_gl_context_end_frame (GdkGLContext   *context,
                          cairo_region_t *painted,
                          cairo_region_t *damage)
{
  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  GDK_GL_CONTEXT_GET_CLASS (context)->end_frame (context, painted, damage);
}

GdkGLContextPaintData *
gdk_gl_context_get_paint_data (GdkGLContext *context)
{

  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (priv->paint_data == NULL)
    priv->paint_data = g_new0 (GdkGLContextPaintData, 1);

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

/**
 * gdk_gl_context_set_debug_enabled:
 * @context: a #GdkGLContext
 * @enabled: whether to enable debugging in the context
 *
 * Sets whether the #GdkGLContext should perform extra validations and
 * run time checking. This is useful during development, but has
 * additional overhead.
 *
 * The #GdkGLContext must not be realized.
 *
 * This function has effect only on #GdkGLContexts created using
 * the %GDK_GL_PROFILE_3_2_CORE profile.
 *
 * Since: 3.16
 */
void
gdk_gl_context_set_debug_enabled (GdkGLContext *context,
                                  gboolean      enabled)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!priv->realized);
  g_return_if_fail (priv->profile == GDK_GL_PROFILE_3_2_CORE);

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
 *
 * Since: 3.16
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
 * The #GdkGLContext must not be realized.
 *
 * This function has effect only on #GdkGLContexts created using
 * the %GDK_GL_PROFILE_3_2_CORE profile.
 *
 * Since: 3.16
 */
void
gdk_gl_context_set_forward_compatible (GdkGLContext *context,
                                       gboolean      compatible)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

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
 *
 * Since: 3.16
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
 * The #GdkGLContext must not be realized.
 *
 * This function has effect only on #GdkGLContexts created using
 * the %GDK_GL_PROFILE_3_2_CORE profile.
 *
 * Since: 3.16
 */
void
gdk_gl_context_set_required_version (GdkGLContext *context,
                                     int           major,
                                     int           minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (!priv->realized);
  g_return_if_fail (priv->profile == GDK_GL_PROFILE_3_2_CORE);

  priv->major = major;
  priv->minor = minor;
}

/**
 * gdk_gl_context_get_required_version:
 * @context: a #GdkGLContext
 * @major: (out) (nullable): return location for the major version to request
 * @minor: (out) (nullable): return location for the minor version to request
 *
 * Retrieves the major and minor version requested by calling
 * gdk_gl_context_set_required_version().
 *
 * Since: 3.16
 */
void
gdk_gl_context_get_required_version (GdkGLContext *context,
                                     int          *major,
                                     int          *minor)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  if (major != NULL && priv->major > 0)
    *major = priv->major;
  else
    {
      switch (priv->profile)
        {
        case GDK_GL_PROFILE_DEFAULT:
        case GDK_GL_PROFILE_LEGACY:
          if (major != NULL)
            *major = 1;
          break;

        case GDK_GL_PROFILE_3_2_CORE:
          if (major != NULL)
            *major = 3;
          break;
        }
    }

  if (minor != NULL && priv->minor > 0)
    *minor = priv->minor;
  else
    {
      switch (priv->profile)
        {
        case GDK_GL_PROFILE_DEFAULT:
        case GDK_GL_PROFILE_LEGACY:
          if (minor != NULL)
            *minor = 0;
          break;

        case GDK_GL_PROFILE_3_2_CORE:
          if (minor != NULL)
            *minor = 2;
          break;
        }
    }
}

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
 *
 * Since: 3.16
 */
gboolean
gdk_gl_context_realize (GdkGLContext  *context,
                        GError       **error)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  if (priv->realized)
    return priv->realized;

  priv->realized = GDK_GL_CONTEXT_GET_CLASS (context)->realize (context, error);

  return priv->realized;
}

static void
gdk_gl_context_check_extensions (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);
  gboolean has_npot, has_texture_rectangle;

  if (!priv->realized)
    return;

  if (priv->extensions_checked)
    return;

  has_npot = epoxy_has_gl_extension ("GL_ARB_texture_non_power_of_two");
  has_texture_rectangle = epoxy_has_gl_extension ("GL_ARB_texture_rectangle");

  priv->has_gl_framebuffer_blit = epoxy_has_gl_extension ("GL_EXT_framebuffer_blit");
  priv->has_frame_terminator = epoxy_has_gl_extension ("GL_GREMEDY_frame_terminator");

  if (G_UNLIKELY (_gdk_gl_flags & GDK_GL_TEXTURE_RECTANGLE))
    priv->use_texture_rectangle = TRUE;
  else if (has_npot)
    priv->use_texture_rectangle = FALSE;
  else if (has_texture_rectangle)
    priv->use_texture_rectangle = TRUE;
  else
    g_warning ("GL implementation doesn't support any form of non-power-of-two textures");

  priv->extensions_checked = TRUE;
}

/**
 * gdk_gl_context_make_current:
 * @context: a #GdkGLContext
 *
 * Makes the @context the current one.
 *
 * Since: 3.16
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

  if (gdk_display_make_gl_context_current (priv->display, context))
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
 * Returns: (transfer none): a #GdkDisplay or %NULL
 *
 * Since: 3.16
 */
GdkDisplay *
gdk_gl_context_get_display (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->display;
}

/**
 * gdk_gl_context_get_window:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkWindow used by the @context.
 *
 * Returns: (transfer none): a #GdkWindow or %NULL
 *
 * Since: 3.16
 */
GdkWindow *
gdk_gl_context_get_window (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->window;
}

/**
 * gdk_gl_context_get_profile:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkGLProfile that @context was created for.
 *
 * Returns: a #GdkGLProfile
 *
 * Since: 3.16
 */
GdkGLProfile
gdk_gl_context_get_profile (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), GDK_GL_PROFILE_LEGACY);

  return priv->profile;
}

/**
 * gdk_gl_context_get_shared_context:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkGLContext that this @context share data with.
 *
 * Returns: (transfer none): a #GdkGLContext or %NULL
 *
 * Since: 3.16
 */
GdkGLContext *
gdk_gl_context_get_shared_context (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->shared_context;
}

/**
 * gdk_gl_context_clear_current:
 *
 * Clears the current #GdkGLContext.
 *
 * Any OpenGL call after this function returns will be ignored
 * until gdk_gl_context_make_current() is called.
 *
 * Since: 3.16
 */
void
gdk_gl_context_clear_current (void)
{
  GdkGLContext *current;

  current = g_private_get (&thread_current_context);
  if (current != NULL)
    {
      GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (current);

      if (gdk_display_make_gl_context_current (priv->display, NULL))
        g_private_replace (&thread_current_context, NULL);
    }
}

/**
 * gdk_gl_context_get_current:
 *
 * Retrieves the current #GdkGLContext.
 *
 * Returns: (transfer none): the current #GdkGLContext, or %NULL
 *
 * Since: 3.16
 */
GdkGLContext *
gdk_gl_context_get_current (void)
{
  GdkGLContext *current;

  current = g_private_get (&thread_current_context);

  return current;
}

/**
 * gdk_gl_get_flags:
 *
 * Returns the currently active GL flags.
 *
 * Returns: the GL flags
 *
 * Since: 3.16
 */
GdkGLFlags
gdk_gl_get_flags (void)
{
  return _gdk_gl_flags;
}

/**
 * gdk_gl_set_flags:
 * @flags: #GdkGLFlags to set
 *
 * Sets GL flags.
 *
 * Since: 3.16
 */
void
gdk_gl_set_flags (GdkGLFlags flags)
{
  _gdk_gl_flags = flags;
}
