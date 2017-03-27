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
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkanrendererprivate.h"
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
gsk_renderer_real_realize (GskRenderer  *self,
                           GdkWindow    *window,
                           GError      **error)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, realize);
  return FALSE;
}

static void
gsk_renderer_real_unrealize (GskRenderer *self)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, unrealize);
}

static GskTexture *
gsk_renderer_real_render_texture (GskRenderer           *self,
                                  GskRenderNode         *root,
                                  const graphene_rect_t *viewport)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, render_texture);
  return NULL;
}

static GdkDrawingContext *
gsk_renderer_real_begin_draw_frame (GskRenderer          *self,
                                    const cairo_region_t *region)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  return gdk_window_begin_draw_frame (priv->window,
                                      NULL,
                                      region);
}

static void
gsk_renderer_real_end_draw_frame (GskRenderer       *self,
                                  GdkDrawingContext *context)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  gdk_window_end_draw_frame (priv->window,
                             context);
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

static void
gsk_renderer_dispose (GObject *gobject)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  gsk_renderer_unrealize (self);

  g_clear_object (&priv->profiler);
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
  klass->begin_draw_frame = gsk_renderer_real_begin_draw_frame;
  klass->end_draw_frame = gsk_renderer_real_end_draw_frame;
  klass->render = gsk_renderer_real_render;
  klass->render_texture = gsk_renderer_real_render_texture;
  klass->create_cairo_surface = gsk_renderer_real_create_cairo_surface;

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
 * gsk_renderer_get_window:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkWindow set using gsk_renderer_realize(). If the renderer
 * has not been realized yet, %NULL will be returned.
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
 * @window: the #GdkWindow renderer will be used on
 * @error: return location for an error
 *
 * Creates the resources needed by the @renderer to render the scene
 * graph.
 *
 * Since: 3.90
 */
gboolean
gsk_renderer_realize (GskRenderer  *renderer,
                      GdkWindow    *window,
                      GError      **error)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);
  g_return_val_if_fail (!gsk_renderer_is_realized (renderer), FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  priv->window = g_object_ref (window);

  if (!GSK_RENDERER_GET_CLASS (renderer)->realize (renderer, window, error))
    {
      g_clear_object (&priv->window);
      return FALSE;
    }

  priv->is_realized = TRUE;
  return TRUE;
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
 * gsk_renderer_render_texture:
 * @renderer: a realized #GdkRenderer
 * @root: a #GskRenderNode
 * @viewport: (allow-none): the section to draw or %NULL to use @root's bounds
 *
 * Renders the scene graph, described by a tree of #GskRenderNode instances,
 * to a #GskTexture.
 *
 * The @renderer will acquire a reference on the #GskRenderNode tree while
 * the rendering is in progress, and will make the tree immutable.
 *
 * If you want to apply any transformations to @root, you should put it into a 
 * transform node and pass that node instead.
 *
 * Returns: (transfer full): a #GskTexture with the rendered contents of @root.
 *
 * Since: 3.90
 */
GskTexture *
gsk_renderer_render_texture (GskRenderer           *renderer,
                             GskRenderNode         *root,
                             const graphene_rect_t *viewport)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  graphene_rect_t real_viewport;
  GskTexture *texture;

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (priv->is_realized, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (root), NULL);
  g_return_val_if_fail (priv->root_node == NULL, NULL);

  priv->root_node = gsk_render_node_ref (root);

  if (viewport == NULL)
    {
      gsk_render_node_get_bounds (root, &real_viewport);
      viewport = &real_viewport;
    }

#ifdef G_ENABLE_DEBUG
  gsk_profiler_reset (priv->profiler);
#endif

  texture = GSK_RENDERER_GET_CLASS (renderer)->render_texture (renderer, root, viewport);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (RENDERER))
    {
      GString *buf = g_string_new ("*** Texture stats ***\n\n");

      gsk_profiler_append_counters (priv->profiler, buf);
      g_string_append_c (buf, '\n');

      gsk_profiler_append_timers (priv->profiler, buf);
      g_string_append_c (buf, '\n');

      g_print ("%s\n***\n\n", buf->str);

      g_string_free (buf, TRUE);
    }
#endif

  g_clear_pointer (&priv->root_node, gsk_render_node_unref);

  return texture;
}

/**
 * gsk_renderer_render:
 * @renderer: a #GskRenderer
 * @root: a #GskRenderNode
 * @context: The drawing context created via gsk_renderer_begin_draw_frame()
 *
 * Renders the scene graph, described by a tree of #GskRenderNode instances,
 * using the given #GdkDrawingContext.
 *
 * The @renderer will acquire a reference on the #GskRenderNode tree while
 * the rendering is in progress.
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
  g_return_if_fail (priv->root_node == NULL);
  g_return_if_fail (GDK_IS_DRAWING_CONTEXT (context));
  g_return_if_fail (context == priv->drawing_context);

  priv->root_node = gsk_render_node_ref (root);

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

  g_clear_pointer (&priv->root_node, gsk_render_node_unref);
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

static GType
get_renderer_for_name (const char *renderer_name)
{
  if (renderer_name == NULL)
    return G_TYPE_INVALID;
  else if (g_ascii_strcasecmp (renderer_name, "cairo") == 0)
    return GSK_TYPE_CAIRO_RENDERER;
  else if (g_ascii_strcasecmp (renderer_name, "opengl") == 0
           || g_ascii_strcasecmp (renderer_name, "gl") == 0)
    return GSK_TYPE_GL_RENDERER;
#ifdef GDK_RENDERING_VULKAN
  else if (g_ascii_strcasecmp (renderer_name, "vulkan") == 0)
    return GSK_TYPE_VULKAN_RENDERER;
#endif
  else if (g_ascii_strcasecmp (renderer_name, "help") == 0)
    {
      g_print ("Supported arguments for GSK_RENDERER environment variable:\n");
      g_print ("   cairo - Use the Cairo fallback renderer\n");
      g_print ("  opengl - Use the default OpenGL renderer\n");
#ifdef GDK_RENDERING_VULKAN
      g_print ("  vulkan - Use the Vulkan renderer\n");
#endif
      g_print ("    help - Print this help\n\n");
      g_print ("Other arguments will cause a warning and be ignored.\n");
    }
  else
    {
      g_warning ("Unrecognized renderer \"%s\". Try GSK_RENDERER=help", renderer_name);
    }

  return G_TYPE_INVALID;
}

static GType
get_renderer_for_display (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  const char *renderer_name;

  renderer_name = g_object_get_data (G_OBJECT (display), "gsk-renderer");
  return get_renderer_for_name (renderer_name);
}

static GType
get_renderer_for_env_var (GdkWindow *window)
{
  static GType env_var_type = G_TYPE_NONE;

  if (env_var_type == G_TYPE_NONE)
    {
      const char *renderer_name = g_getenv ("GSK_RENDERER");
      env_var_type = get_renderer_for_name (renderer_name);
    }

  return env_var_type;
}

static GType
get_renderer_for_backend (GdkWindow *window)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_WINDOW (window))
    return GSK_TYPE_GL_RENDERER; 
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_WINDOW (window))
    return GSK_TYPE_GL_RENDERER;
#endif

  return G_TYPE_INVALID;
}

static GType
get_renderer_fallback (GdkWindow *window)
{
  return GSK_TYPE_CAIRO_RENDERER;
}

static struct {
  gboolean verbose;
  GType (* get_renderer) (GdkWindow *window);
} renderer_possibilities[] = {
  { TRUE,  get_renderer_for_display },
  { TRUE,  get_renderer_for_env_var },
  { FALSE, get_renderer_for_backend },
  { FALSE, get_renderer_fallback },
};

/**
 * gsk_renderer_new_for_window:
 * @window: a #GdkWindow
 *
 * Creates an appropriate #GskRenderer instance for the given @window.
 *
 * The renderer will be realized when it is returned.
 *
 * Returns: (transfer full) (nullable): a #GskRenderer
 *
 * Since: 3.90
 */
GskRenderer *
gsk_renderer_new_for_window (GdkWindow *window)
{
  GType renderer_type;
  GskRenderer *renderer;
  GError *error = NULL;
  gboolean verbose = FALSE;
  guint i;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  for (i = 0; i < G_N_ELEMENTS (renderer_possibilities); i++)
    {
      renderer_type = renderer_possibilities[i].get_renderer (window);
      if (renderer_type == G_TYPE_INVALID)
        continue;

      /* If a renderer is selected that's marked as verbose, start printing
       * information to stdout.
       */
      verbose |= renderer_possibilities[i].verbose;
      renderer = g_object_new (renderer_type,
                               "display", gdk_window_get_display (window),
                               NULL);

      if (gsk_renderer_realize (renderer, window, &error))
        {
          if (verbose || GSK_DEBUG_CHECK (RENDERER))
            {
              g_print ("Using renderer of type '%s' for display '%s'\n",
                       G_OBJECT_TYPE_NAME (renderer),
                       G_OBJECT_TYPE_NAME (window));
            }
          return renderer;
        }

      if (verbose || GSK_DEBUG_CHECK (RENDERER))
        {
          g_print ("Failed to realize renderer of type '%s' for window '%s': %s\n",
                   G_OBJECT_TYPE_NAME (renderer),
                   G_OBJECT_TYPE_NAME (window),
                   error->message);
        }
      g_object_unref (renderer);
      g_clear_error (&error);
    }

  g_assert_not_reached ();
  return NULL;
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

/**
 * gsk_renderer_begin_draw_frame:
 * @renderer: a #GskRenderer
 * @region: the #cairo_region_t that you wish to draw
 *
 * Indicates that you are beginning the process of redrawing @region using
 * @renderer, and provides you with a #GdkDrawingContext to use for this.
 *
 * Returns: (transfer none): a #GdkDrawingContext context that should be used to
 * draw the contents of the @renderer. This context is owned by GDK.
 *
 * Since: 3.90
 */
GdkDrawingContext *
gsk_renderer_begin_draw_frame (GskRenderer          *renderer,
                               const cairo_region_t *region)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (region != NULL, NULL);
  g_return_val_if_fail (priv->drawing_context == NULL, NULL);

  if (GSK_RENDER_MODE_CHECK (FULL_REDRAW))
    {
      cairo_region_t *full_window;
      
      full_window = cairo_region_create_rectangle (&(GdkRectangle) {
                                                       0, 0,
                                                       gdk_window_get_width (priv->window),
                                                       gdk_window_get_height (priv->window)
                                                   });
      
      priv->drawing_context = GSK_RENDERER_GET_CLASS (renderer)->begin_draw_frame (renderer, full_window);

      cairo_region_destroy (full_window);
    }
  else
    {
      priv->drawing_context = GSK_RENDERER_GET_CLASS (renderer)->begin_draw_frame (renderer, region);
    }

  return priv->drawing_context;
}

void
gsk_renderer_end_draw_frame (GskRenderer       *renderer,
                             GdkDrawingContext *context)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (GDK_IS_DRAWING_CONTEXT (context));
  g_return_if_fail (priv->drawing_context == context);

  priv->drawing_context = NULL;

  GSK_RENDERER_GET_CLASS (renderer)->end_draw_frame (renderer, context);
}

