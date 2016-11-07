/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:GskRenderer
 * @Title: GskRenderer
 * @Short_description: Renders a scene
 *
 * #GskRenderer is a class that renders a scene graph defined via a
 * tree of #GskRenderNode instances.
 *
 * Typically you will use a #GskRenderer instance with a #GdkDrawingContext
 * associated to a #GdkWindow, and call gsk_renderer_render() with the
 * drawing context and the scene to be rendered.
 *
 * It is necessary to realize a #GskRenderer instance using gsk_renderer_realize()
 * before calling gsk_renderer_render(), in order to create the appropriate
 * windowing system resources needed to render the scene.
 */

#include "config.h"

#include "gskrendererprivate.h"

#include "gskcairorendererprivate.h"
#include "gskdebugprivate.h"
#include "gskglrendererprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktexture.h"

#include "gskenumtypes.h"

#include <graphene-gobject.h>
#include <cairo-gobject.h>
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

typedef struct
{
  GObject parent_instance;

  graphene_rect_t viewport;

  GskScalingFilter min_filter;
  GskScalingFilter mag_filter;

  GdkWindow *window;
  GdkDrawingContext *drawing_context;
  GskRenderNode *root_node;
  GdkDisplay *display;
  cairo_t *cairo_context;

  GskProfiler *profiler;

  int scale_factor;

  gboolean is_realized : 1;
} GskRendererPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GskRenderer, gsk_renderer, G_TYPE_OBJECT)

enum {
  PROP_VIEWPORT = 1,
  PROP_SCALE_FACTOR,
  PROP_WINDOW,
  PROP_DISPLAY,
  PROP_DRAWING_CONTEXT,

  N_PROPS
};

static GParamSpec *gsk_renderer_properties[N_PROPS];

#define GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Renderer of type '%s' does not implement GskRenderer::" # method, G_OBJECT_TYPE_NAME (obj))

static gboolean
gsk_renderer_real_realize (GskRenderer *self)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, realize);
  return FALSE;
}

static void
gsk_renderer_real_unrealize (GskRenderer *self)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, unrealize);
}

static void
gsk_renderer_real_render (GskRenderer *self,
                          GskRenderNode *root)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, render);
}

static cairo_surface_t *
gsk_renderer_real_create_cairo_surface (GskRenderer    *self,
                                        cairo_format_t  format,
                                        int             width,
                                        int             height)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);
  int scale_factor = priv->scale_factor > 0 ? priv->scale_factor : 1;
  int real_width = width * scale_factor;
  int real_height = height * scale_factor;

  cairo_surface_t *res = cairo_image_surface_create (format, real_width, real_height);

  cairo_surface_set_device_scale (res, scale_factor, scale_factor);

  return res;
}

static GskTexture *
gsk_renderer_real_texture_new_for_data (GskRenderer  *self,
                                        const guchar *data,
                                        int           width,
                                        int           height,
                                        int           stride)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, texture_new_for_data);

  return NULL;
}

static GskTexture *
gsk_renderer_real_texture_new_for_pixbuf (GskRenderer *renderer,
                                          GdkPixbuf   *pixbuf)
{
  GskTexture *texture;
  cairo_surface_t *surface;

  surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);

  texture = gsk_texture_new_for_data (renderer,
                                      cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_width (surface),
                                      cairo_image_surface_get_height (surface),
                                      cairo_image_surface_get_stride (surface));

  cairo_surface_destroy (surface);

  return texture;
}

static void
gsk_renderer_real_texture_destroy (GskTexture *texture)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (gsk_texture_get_renderer (texture), texture_destroy);
}

static void
gsk_renderer_dispose (GObject *gobject)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  gsk_renderer_unrealize (self);

  g_clear_pointer (&priv->cairo_context, cairo_destroy);

  g_clear_object (&priv->profiler);
  g_clear_object (&priv->window);
  g_clear_object (&priv->display);

  G_OBJECT_CLASS (gsk_renderer_parent_class)->dispose (gobject);
}

static void
gsk_renderer_set_property (GObject      *gobject,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_VIEWPORT:
      gsk_renderer_set_viewport (self, g_value_get_boxed (value));
      break;

    case PROP_SCALE_FACTOR:
      gsk_renderer_set_scale_factor (self, g_value_get_int (value));
      break;

    case PROP_WINDOW:
      gsk_renderer_set_window (self, g_value_get_object (value));
      break;

    case PROP_DISPLAY:
      /* Construct-only */
      priv->display = g_value_dup_object (value);
      break;
    }
}

static void
gsk_renderer_get_property (GObject    *gobject,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_VIEWPORT:
      g_value_set_boxed (value, &priv->viewport);
      break;

    case PROP_SCALE_FACTOR:
      g_value_set_int (value, priv->scale_factor);
      break;

    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
      break;

    case PROP_DRAWING_CONTEXT:
      g_value_set_object (value, priv->drawing_context);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    }
}

static void
gsk_renderer_constructed (GObject *gobject)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  if (priv->display == NULL)
    {
      GdkDisplayManager *manager = gdk_display_manager_get ();

      priv->display = gdk_display_manager_get_default_display (manager);
      g_assert (priv->display != NULL);
    }

  G_OBJECT_CLASS (gsk_renderer_parent_class)->constructed (gobject);
}

static void
gsk_renderer_class_init (GskRendererClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->realize = gsk_renderer_real_realize;
  klass->unrealize = gsk_renderer_real_unrealize;
  klass->render = gsk_renderer_real_render;
  klass->create_cairo_surface = gsk_renderer_real_create_cairo_surface;
  klass->texture_new_for_data = gsk_renderer_real_texture_new_for_data;
  klass->texture_new_for_pixbuf = gsk_renderer_real_texture_new_for_pixbuf;
  klass->texture_destroy = gsk_renderer_real_texture_destroy;

  gobject_class->constructed = gsk_renderer_constructed;
  gobject_class->set_property = gsk_renderer_set_property;
  gobject_class->get_property = gsk_renderer_get_property;
  gobject_class->dispose = gsk_renderer_dispose;

  /**
   * GskRenderer:viewport:
   *
   * The visible area used by the #GskRenderer to render its contents.
   *
   * Since: 3.90
   */
  gsk_renderer_properties[PROP_VIEWPORT] =
    g_param_spec_boxed ("viewport",
			"Viewport",
			"The visible area used by the renderer",
			GRAPHENE_TYPE_RECT,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:display:
   *
   * The #GdkDisplay used by the #GskRenderer.
   *
   * Since: 3.90
   */
  gsk_renderer_properties[PROP_DISPLAY] =
    g_param_spec_object ("display",
			 "Display",
			 "The GdkDisplay object used by the renderer",
			 GDK_TYPE_DISPLAY,
			 G_PARAM_READWRITE |
			 G_PARAM_CONSTRUCT_ONLY |
			 G_PARAM_STATIC_STRINGS);

  gsk_renderer_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The window associated to the renderer",
                         GDK_TYPE_WINDOW,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GskRenderer:scale-factor:
   *
   * The scale factor used when rendering.
   *
   * Since: 3.90
   */
  gsk_renderer_properties[PROP_SCALE_FACTOR] =
    g_param_spec_int ("scale-factor",
                      "Scale Factor",
                      "The scaling factor of the renderer",
                      1, G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:drawing-context:
   *
   * The drawing context used when rendering.
   *
   * Since: 3.90
   */
  gsk_renderer_properties[PROP_DRAWING_CONTEXT] =
    g_param_spec_object ("drawing-context",
                         "Drawing Context",
                         "The drawing context used by the renderer",
                         GDK_TYPE_DRAWING_CONTEXT,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, gsk_renderer_properties);
}

static void
gsk_renderer_init (GskRenderer *self)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  priv->profiler = gsk_profiler_new ();

  priv->scale_factor = 1;
}

/**
 * gsk_renderer_set_viewport:
 * @renderer: a #GskRenderer
 * @viewport: (nullable): the viewport rectangle used by the @renderer
 *
 * Sets the visible rectangle to be used as the viewport for
 * the rendering.
 *
 * Since: 3.90
 */
void
gsk_renderer_set_viewport (GskRenderer           *renderer,
                           const graphene_rect_t *viewport)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (viewport == NULL)
    {
      graphene_rect_init (&priv->viewport, 0.f, 0.f, 0.f, 0.f);
      g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_VIEWPORT]);
      return;
    }

  if (graphene_rect_equal (viewport, &priv->viewport))
    return;

  graphene_rect_init_from_rect (&priv->viewport, viewport);

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_VIEWPORT]);
}

/**
 * gsk_renderer_get_viewport:
 * @renderer: a #GskRenderer
 * @viewport: (out caller-allocates): return location for the viewport rectangle
 *
 * Retrieves the viewport of the #GskRenderer.
 *
 * Since: 3.90
 */
void
gsk_renderer_get_viewport (GskRenderer     *renderer,
                           graphene_rect_t *viewport)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (viewport != NULL);

  graphene_rect_init_from_rect (viewport, &priv->viewport);
}

/**
 * gsk_renderer_set_scale_factor:
 * @renderer: a #GskRenderer
 * @scale_factor: the new scale factor
 *
 * Sets the scale factor for the renderer.
 *
 * Since: 3.90
 */
void
gsk_renderer_set_scale_factor (GskRenderer *renderer,
                               int          scale_factor)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (priv->scale_factor != scale_factor)
    {
      priv->scale_factor = scale_factor;

      g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_SCALE_FACTOR]);
    }
}

/**
 * gsk_renderer_get_scale_factor:
 * @renderer: a #GskRenderer
 *
 * Gets the scale factor for the @renderer.
 *
 * Returns: the scale factor
 *
 * Since: 3.90
 */
int
gsk_renderer_get_scale_factor (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), 1);

  return priv->scale_factor;
}

/**
 * gsk_renderer_set_window:
 * @renderer: a #GskRenderer
 * @window: the window to set
 *
 * Sets the window on which the @renderer is rendering.
 *
 * Since: 3.90
 */
void
gsk_renderer_set_window (GskRenderer *renderer,
                         GdkWindow   *window)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (!priv->is_realized);
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (g_set_object (&priv->window, window))
    g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_WINDOW]);
}

/**
 * gsk_renderer_get_window:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkWindow set using gsk_renderer_set_window().
 *
 * Returns: (transfer none) (nullable): a #GdkWindow
 *
 * Since: 3.90
 */
GdkWindow *
gsk_renderer_get_window (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->window;
}

/*< private >
 * gsk_renderer_get_root_node:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GskRenderNode used by @renderer.
 *
 * Returns: (transfer none) (nullable): a #GskRenderNode
 */
GskRenderNode *
gsk_renderer_get_root_node (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->root_node;
}

/*< private >
 * gsk_renderer_get_drawing_context:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkDrawingContext used by @renderer.
 *
 * Returns: (transfer none) (nullable): a #GdkDrawingContext
 */
GdkDrawingContext *
gsk_renderer_get_drawing_context (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->drawing_context;
}

/**
 * gsk_renderer_get_display:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkDisplay used when creating the #GskRenderer.
 *
 * Returns: (transfer none): a #GdkDisplay
 *
 * Since: 3.90
 */
GdkDisplay *
gsk_renderer_get_display (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->display;
}

/*< private >
 * gsk_renderer_is_realized:
 * @renderer: a #GskRenderer
 *
 * Checks whether the @renderer is realized or not.
 *
 * Returns: %TRUE if the #GskRenderer was realized, and %FALSE otherwise
 *
 * Since: 3.90
 */
gboolean
gsk_renderer_is_realized (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  return priv->is_realized;
}

/**
 * gsk_renderer_realize:
 * @renderer: a #GskRenderer
 *
 * Creates the resources needed by the @renderer to render the scene
 * graph.
 *
 * Since: 3.90
 */
gboolean
gsk_renderer_realize (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  if (priv->is_realized)
    return TRUE;

  priv->is_realized = GSK_RENDERER_GET_CLASS (renderer)->realize (renderer);

  return priv->is_realized;
}

/**
 * gsk_renderer_unrealize:
 * @renderer: a #GskRenderer
 *
 * Releases all the resources created by gsk_renderer_realize().
 *
 * Since: 3.90
 */
void
gsk_renderer_unrealize (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (!priv->is_realized)
    return;

  GSK_RENDERER_GET_CLASS (renderer)->unrealize (renderer);

  priv->is_realized = FALSE;
}

/**
 * gsk_renderer_render:
 * @renderer: a #GskRenderer
 * @root: a #GskRenderNode
 * @context: a #GdkDrawingContext
 *
 * Renders the scene graph, described by a tree of #GskRenderNode instances,
 * using the given #GdkDrawingContext.
 *
 * The @renderer will acquire a reference on the #GskRenderNode tree while
 * the rendering is in progress, and will make the tree immutable.
 *
 * Since: 3.90
 */
void
gsk_renderer_render (GskRenderer       *renderer,
                     GskRenderNode     *root,
                     GdkDrawingContext *context)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (priv->is_realized);
  g_return_if_fail (GSK_IS_RENDER_NODE (root));
  g_return_if_fail (context == NULL || GDK_IS_DRAWING_CONTEXT (context));
  g_return_if_fail (priv->drawing_context == NULL);
  g_return_if_fail (priv->root_node == NULL);

  if (context != NULL)
    priv->drawing_context = g_object_ref (context);
  else
    {
      if (priv->cairo_context == NULL)
        {
          g_critical ("The given GskRenderer instance was not created using "
                      "gsk_renderer_create_fallback(), but you forgot to pass "
                      "a GdkDrawingContext.");
          return;
        }
    }

  priv->root_node = gsk_render_node_ref (root);
  gsk_render_node_make_immutable (priv->root_node);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_reset (priv->profiler);
#endif

  GSK_RENDERER_GET_CLASS (renderer)->render (renderer, root);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (RENDERER))
    {
      GString *buf = g_string_new ("*** Frame stats ***\n\n");

      gsk_profiler_append_counters (priv->profiler, buf);
      g_string_append_c (buf, '\n');

      gsk_profiler_append_timers (priv->profiler, buf);
      g_string_append_c (buf, '\n');

      g_print ("%s\n***\n\n", buf->str);

      g_string_free (buf, TRUE);
    }
#endif

  g_clear_object (&priv->drawing_context);
  g_clear_pointer (&priv->root_node, gsk_render_node_unref);
}

/**
 * gsk_renderer_create_render_node:
 * @renderer: a #GskRenderer
 *
 * Creates a new #GskRenderNode instance tied to the given @renderer.
 *
 * Returns: (transfer full): the new #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_renderer_create_render_node (GskRenderer *renderer)
{
  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return gsk_render_node_new ();
}

/*< private >
 * gsk_renderer_get_profiler:
 * @renderer: a #GskRenderer
 *
 * Retrieves a pointer to the GskProfiler instance of the renderer.
 *
 * Returns: (transfer none): the profiler
 */
GskProfiler *
gsk_renderer_get_profiler (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->profiler;
}

/**
 * gsk_renderer_get_for_display:
 * @display: a #GdkDisplay
 *
 * Creates an appropriate #GskRenderer instance for the given @display.
 *
 * Returns: (transfer full) (nullable): a #GskRenderer
 *
 * Since: 3.90
 */
GskRenderer *
gsk_renderer_get_for_display (GdkDisplay *display)
{
  static const char *use_software;

  GType renderer_type = G_TYPE_INVALID;

  if (use_software == NULL)
    {
      use_software = g_getenv ("GSK_USE_SOFTWARE");
      if (use_software == NULL)
        use_software = "0";
    }

  if (use_software[0] != '0')
    return g_object_new (GSK_TYPE_CAIRO_RENDERER, "display", display, NULL);

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    renderer_type = GSK_TYPE_GL_RENDERER; 
  else
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    renderer_type = GSK_TYPE_GL_RENDERER;
  else
#endif
    renderer_type = GSK_TYPE_CAIRO_RENDERER;

  GSK_NOTE (RENDERER, g_print ("Creating renderer of type '%s' for display '%s'\n",
                               g_type_name (renderer_type),
                               G_OBJECT_TYPE_NAME (display)));

  g_assert (renderer_type != G_TYPE_INVALID);

  return g_object_new (renderer_type, "display", display, NULL);
}

cairo_surface_t *
gsk_renderer_create_cairo_surface (GskRenderer    *renderer,
                                   cairo_format_t  format,
                                   int             width,
                                   int             height)
{
  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  return GSK_RENDERER_GET_CLASS (renderer)->create_cairo_surface (renderer, format, width, height);
}

static void
gsk_renderer_set_cairo_context (GskRenderer *renderer,
                                cairo_t     *cr)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_clear_pointer (&priv->cairo_context, cairo_destroy);

  if (cr != NULL)
    priv->cairo_context = cairo_reference (cr);
}

cairo_t *
gsk_renderer_get_cairo_context (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  return priv->cairo_context;
}

/**
 * gsk_renderer_create_fallback:
 * @renderer: a #GskRenderer
 * @viewport: the viewport for the fallback renderer
 * @cr: a Cairo context
 *
 * Creates a fallback #GskRenderer using the same display and window of
 * the given @renderer, and instructs it to render to a given Cairo
 * context.
 *
 * Typically, you'll use this function to implement fallback rendering
 * of #GskRenderNodes on an intermediate Cairo context, instead of using
 * the drawing context associated to a #GdkWindow's rendering buffer.
 *
 * Returns: (transfer full): a newly created fallback #GskRenderer instance
 *
 * Since: 3.90
 */
GskRenderer *
gsk_renderer_create_fallback (GskRenderer           *renderer,
                              const graphene_rect_t *viewport,
                              cairo_t               *cr)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  GskRenderer *res;

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (cr != NULL, NULL);

  res = g_object_new (GSK_TYPE_CAIRO_RENDERER,
                      "display", priv->display,
                      "window", priv->window,
                      "scale-factor", priv->scale_factor,
                      "viewport", viewport,
                      NULL);

  gsk_renderer_set_cairo_context (res, cr);
  gsk_renderer_realize (res);

  return res;
}
