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
 * #GdkGLContexts are created via a #GdkDisplay by specifying a
 * #GdkGLPixelFormat to be used by the OpenGL context.
 *
 * Support for #GdkGLContext is platform specific; in order to
 * discover if the platform supports OpenGL, you should use the
 * #GdkGLPixelFormat class.
 *
 * A #GdkGLContext has to be associated with a #GdkWindow and
 * made "current", otherwise any OpenGL call will be ignored.
 *
 * ## Creating a new OpenGL context ##
 *
 * In order to create a new #GdkGLContext instance you need a
 * #GdkGLPixelFormat instance and a #GdkDisplay.
 *
 * The #GdkGLPixelFormat class contains configuration option that
 * you require from the windowing system to be available on the
 * #GdkGLContext.
 *
 * |[<!-- language="C" -->
 *   GdkGLPixelFormat *format;
 *
 *   format = gdk_gl_pixel_format_new ("double-buffer", TRUE,
 *                                     "depth-size", 32,
 *                                     NULL);
 * ]|
 *
 * The example above will create a pixel format with double buffering
 * and a depth buffer size of 32 bits.
 *
 * You can either choose to validate the pixel format, in case you
 * have the ability to change your drawing code depending on it, or
 * just ask the #GdkDisplay to create the #GdkGLContext with it, which
 * will implicitly validate the pixel format and return an error if it
 * could not find an OpenGL context that satisfied the requirements
 * of the pixel format:
 *
 * |[<!-- language="C" -->
 *   GError *error = NULL;
 *
 *   // the "display" variable has been set elsewhere
 *   GdkGLContext *context =
 *     gdk_display_create_gl_context (display, format, &error);
 *
 *   if (error != NULL)
 *     {
 *       // handle error condition
 *     }
 *
 *   // you can release the reference on the pixel format at
 *   // this point
 *   g_object_unref (format);
 * ]|
 *
 * ## Using a GdkGLContext ##
 *
 * In order to use a #GdkGLContext to draw with OpenGL commands
 * on a #GdkWindow, it's necessary to bind the context to the
 * window:
 *
 * |[<!-- language="C" -->
 *   // associates the window to the context
 *   gdk_gl_context_set_window (context, window);
 * ]|
 *
 * This ensures that the #GdkGLContext can refer to the #GdkWindow,
 * as well as the #GdkWindow can present the result of the OpenGL
 * commands.
 *
 * You will also need to make the #GdkGLContext the current context
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
 * Once you finished drawing your frame, and you want to present the
 * result on the window bound to the #GdkGLContext, you should call
 * gdk_gl_context_flush_buffer().
 *
 * If the #GdkWindow bound to the #GdkGLContext changes size, you
 * will need to call gdk_gl_context_update() to ensure that the OpenGL
 * viewport is kept in sync with the size of the window.
 *
 * You can detach the currently bound #GdkWindow from a #GdkGLContext
 * by using gdk_gl_context_set_window() with a %NULL argument.
 *
 * You can check which #GdkGLContext is the current one by using
 * gdk_gl_context_get_current(); you can also unset any #GdkGLContext
 * that is currently set by calling gdk_gl_context_clear_current().
 */

#include "config.h"

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglpixelformat.h"
#include "gdkvisual.h"
#include "gdkinternals.h"

#include "gdkintl.h"

typedef struct {
  GdkDisplay *display;
  GdkGLPixelFormat *pixel_format;
  GdkWindow *window;
  GdkVisual *visual;

  gboolean swap_interval;
} GdkGLContextPrivate;

enum {
  PROP_0,

  PROP_DISPLAY,
  PROP_PIXEL_FORMAT,
  PROP_WINDOW,
  PROP_VISUAL,

  PROP_SWAP_INTERVAL,

  LAST_PROP
};

static GParamSpec *obj_pspecs[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkGLContext, gdk_gl_context, G_TYPE_OBJECT)

static void
gdk_gl_context_dispose (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  gdk_display_destroy_gl_context (priv->display, context);

  g_clear_object (&priv->display);
  g_clear_object (&priv->pixel_format);
  g_clear_object (&priv->window);
  g_clear_object (&priv->visual);

  G_OBJECT_CLASS (gdk_gl_context_parent_class)->dispose (gobject);
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
      priv->display = g_object_ref (g_value_get_object (value));
      break;

    case PROP_PIXEL_FORMAT:
      priv->pixel_format = g_object_ref (g_value_get_object (value));
      break;

    case PROP_WINDOW:
      {
        GdkGLContext *context = GDK_GL_CONTEXT (gobject);
        GdkWindow *window = g_value_get_object (value);

        gdk_gl_context_set_window (context, window);
      }
      break;

    case PROP_VISUAL:
      {
        GdkVisual *visual = g_value_get_object (value);

        if (visual != NULL)
          priv->visual = g_object_ref (visual);
      }
      break;

    case PROP_SWAP_INTERVAL:
      priv->swap_interval = g_value_get_boolean (value);
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

    case PROP_PIXEL_FORMAT:
      g_value_set_object (value, priv->pixel_format);
      break;

    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
      break;

    case PROP_VISUAL:
      g_value_set_object (value, priv->visual);
      break;

    case PROP_SWAP_INTERVAL:
      g_value_set_boolean (value, priv->swap_interval);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_gl_context_class_init (GdkGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /**
   * GdkGLContext:display:
   *
   * The #GdkDisplay used by the context.
   *
   * Since: 3.14
   */
  obj_pspecs[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "The GDK display used by the GL context",
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:pixel-format:
   *
   * The #GdkGLPixelFormat used to create the context.
   *
   * Since: 3.14
   */
  obj_pspecs[PROP_PIXEL_FORMAT] =
    g_param_spec_object ("pixel-format",
                         "Pixel Format",
                         "The GDK pixel format used by the GL context",
                         GDK_TYPE_GL_PIXEL_FORMAT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:window:
   *
   * The #GdkWindow currently bound to the context.
   *
   * You typically need to bind a #GdkWindow to a #GdkGLContext prior
   * to calling gdk_gl_context_make_current().
   *
   * Since: 3.14
   */
  obj_pspecs[PROP_WINDOW] =
    g_param_spec_object ("window",
                         P_("Window"),
                         P_("The GDK window currently bound to the GL context"),
                         GDK_TYPE_WINDOW,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:visual:
   *
   * The #GdkVisual matching the #GdkGLPixelFormat used by the context.
   *
   * Since: 3.14
   */
  obj_pspecs[PROP_VISUAL] =
    g_param_spec_object ("visual",
                         P_("Visual"),
                         P_("The GDK visual used by the GL context"),
                         GDK_TYPE_VISUAL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLContext:swap-interval:
   *
   * The swap interval of the context.
   *
   * If set to %TRUE (the default), buffers will be flushed only during
   * the vertical refresh of the display.
   *
   * If set to %FALSE, gdk_gl_context_flush_buffer() will execute
   * the buffer flush as soon as possible.
   *
   * Since: 3.14
   */
  obj_pspecs[PROP_SWAP_INTERVAL] =
    g_param_spec_boolean ("swap-interval",
                          P_("Swap Interval"),
                          P_("The swap interval of the GL context"),
                          TRUE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  gobject_class->set_property = gdk_gl_context_set_property;
  gobject_class->get_property = gdk_gl_context_get_property;
  gobject_class->dispose = gdk_gl_context_dispose;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_pspecs);
}

static void
gdk_gl_context_init (GdkGLContext *self)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (self);

  priv->swap_interval = TRUE;
}

/**
 * gdk_gl_context_get_display:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkDisplay associated with the @context.
 *
 * Returns: (transfer none): the #GdkDisplay
 *
 * Since: 3.14
 */
GdkDisplay *
gdk_gl_context_get_display (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->display;
}

/**
 * gdk_gl_context_get_pixel_format:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkGLPixelFormat associated with the @context.
 *
 * Returns: (transfer none): the #GdkDisplay
 *
 * Since: 3.14
 */
GdkGLPixelFormat *
gdk_gl_context_get_pixel_format (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->pixel_format;
}

/**
 * gdk_gl_context_get_visual:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkVisual associated with the @context.
 *
 * Returns: (transfer none): the #GdkVisual
 *
 * Since: 3.14
 */
GdkVisual *
gdk_gl_context_get_visual (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->visual;
}

/**
 * gdk_gl_context_flush_buffer:
 * @context: a #GdkGLContext
 *
 * Copies the back buffer to the front buffer.
 *
 * If the #GdkGLContext is not double buffered, this function does not
 * do anything.
 *
 * Depending on the value of the #GdkGLContext:swap-interval property,
 * the copy may take place during the vertical refresh of the display
 * rather than immediately.
 *
 * This function may call `glFlush()` implicitly before returning; it
 * is not recommended to call `glFlush()` explicitly before calling
 * this function.
 *
 * Since: 3.14
 */
void
gdk_gl_context_flush_buffer (GdkGLContext *context)
{
  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  GDK_GL_CONTEXT_GET_CLASS (context)->flush_buffer (context);
}

/**
 * gdk_gl_context_make_current:
 * @context: a #GdkGLContext
 *
 * Makes the @context the current one.
 *
 * Returns: %TRUE if the context is current
 *
 * Since: 3.14
 */
gboolean
gdk_gl_context_make_current (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), FALSE);

  return gdk_display_make_gl_context_current (priv->display, context, priv->window);
}

/**
 * gdk_gl_context_set_window:
 * @context: a #GdkGLContext
 * @window: (optional): a #GdkWindow, or %NULL
 *
 * Sets the #GdkWindow used to display the draw commands.
 *
 * If @window is %NULL, the @context is detached from the window.
 *
 * Since: 3.14
 */
void
gdk_gl_context_set_window (GdkGLContext *context,
                           GdkWindow    *window)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));
  g_return_if_fail (window == NULL || (GDK_IS_WINDOW (window) && !GDK_WINDOW_DESTROYED (window)));

  if (priv->window == window)
    return;

  if (priv->window != NULL)
    gdk_window_set_gl_context (priv->window, NULL);

  g_clear_object (&priv->window);

  if (window != NULL)
    {
      priv->window = g_object_ref (window);
      gdk_window_set_gl_context (window, context);
    }

  GDK_GL_CONTEXT_GET_CLASS (context)->set_window (context, window);
}

/**
 * gdk_gl_context_get_window:
 * @context: a #GdkGLContext
 *
 * Retrieves the #GdkWindow used by the @context.
 *
 * Returns: (transfer none): a #GdkWindow or %NULL
 *
 * Since: 3.14
 */
GdkWindow *
gdk_gl_context_get_window (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return priv->window;
}

/**
 * gdk_gl_context_update:
 * @context: a #GdkGLContext
 *
 * Updates the @context when the #GdkWindow used to display the
 * rendering changes size or position.
 *
 * Typically, you will call this function after calling
 * gdk_window_resize() or gdk_window_move_resize().
 *
 * Since: 3.14
 */
void
gdk_gl_context_update (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_GL_CONTEXT (context));

  if (priv->window == NULL)
    return;

  GDK_GL_CONTEXT_GET_CLASS (context)->update (context, priv->window);
}

/**
 * gdk_gl_context_clear_current:
 *
 * Clears the current #GdkGLContext.
 *
 * Any OpenGL call after this function returns will be ignored
 * until gdk_gl_context_make_current() is called.
 *
 * Since: 3.14
 */
void
gdk_gl_context_clear_current (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  gdk_display_make_gl_context_current (display, NULL, NULL);
}

/**
 * gdk_gl_context_get_current:
 *
 * Retrieves the current #GdkGLContext.
 *
 * Returns: (transfer none): the current #GdkGLContext, or %NULL
 *
 * Since: 3.14
 */
GdkGLContext *
gdk_gl_context_get_current (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  return gdk_display_get_current_gl_context (display);
}

/*< private >
 * gdk_gl_context_get_swap_interval:
 * @context: a #GdkGLContext
 *
 * Retrieves the swap interval of the context.
 *
 * Returns: the swap interval
 */
gboolean
gdk_gl_context_get_swap_interval (GdkGLContext *context)
{
  GdkGLContextPrivate *priv = gdk_gl_context_get_instance_private (context);

  return priv->swap_interval;
}

/*< private >
 * gdk_window_has_gl_context:
 * @window: a #GdkWindow
 *
 * Checks whether a #GdkWindow has a #GdkGLContext associated to it.
 *
 * Returns: %TRUE if the window has a GL context
 */
gboolean
gdk_window_has_gl_context (GdkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "-gdk-gl-context") != NULL;
}

/*< private >
 * gdk_window_set_gl_context:
 * @window: a #GdkWindow
 * @context: a #GdkGLContext
 *
 * Sets a back pointer to a #GdkGLContext on @window.
 *
 * This function should only be called by gdk_gl_context_set_window().
 */
void
gdk_window_set_gl_context (GdkWindow    *window,
                           GdkGLContext *context)
{
  g_object_set_data (G_OBJECT (window), "-gdk-gl-context", context);
}

/*< private >
 * gdk_window_get_gl_context:
 * @window: a #GdkWindow
 *
 * Retrieves a pointer to the #GdkGLContext associated to
 * the @window.
 *
 * Returns: (transfer none): a #GdkGLContext, or %NULL
 */
GdkGLContext *
gdk_window_get_gl_context (GdkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "-gdk-gl-context");
}
