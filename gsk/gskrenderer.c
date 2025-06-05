/* GSK - The GTK Scene Kit
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
 * GskRenderer:
 *
 * Renders a scene graph defined via a tree of [class@Gsk.RenderNode] instances.
 *
 * Typically you will use a `GskRenderer` instance to repeatedly call
 * [method@Gsk.Renderer.render] to update the contents of its associated
 * [class@Gdk.Surface].
 *
 * It is necessary to realize a `GskRenderer` instance using
 * [method@Gsk.Renderer.realize] before calling [method@Gsk.Renderer.render],
 * in order to create the appropriate windowing system resources needed
 * to render the scene.
 */

#include "config.h"

#include "gskrendererprivate.h"

#include "gskcairorenderer.h"
#include "gskdebugprivate.h"
#include "gskrendernodeprivate.h"
#include "gskoffloadprivate.h"

#include "gskenumtypes.h"

#include "gpu/gskglrenderer.h"
#include "gpu/gskvulkanrenderer.h"
#include "gdk/gdkvulkancontextprivate.h"
#include "gdk/gdkdisplayprivate.h"

#include <graphene-gobject.h>
#include <cairo-gobject.h>
#include <gdk/gdk.h>
#include "gdk/gdkdebugprivate.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gskbroadwayrenderer.h"
#endif

typedef struct
{
  GObject parent_instance;

  GdkSurface *surface;
  GskRenderNode *prev_node;

  GskDebugFlags debug_flags;

  unsigned int is_realized : 1;
} GskRendererPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GskRenderer, gsk_renderer, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_REALIZED,
  PROP_SURFACE,

  N_PROPS
};

static GParamSpec *gsk_renderer_properties[N_PROPS];

#define GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Renderer of type '%s' does not implement GskRenderer::" # method, G_OBJECT_TYPE_NAME (obj))

static gboolean
gsk_renderer_real_realize (GskRenderer  *self,
                           GdkDisplay   *display,
                           GdkSurface   *surface,
                           gboolean      attach,
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

static GdkTexture *
gsk_renderer_real_render_texture (GskRenderer           *self,
                                  GskRenderNode         *root,
                                  const graphene_rect_t *viewport)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, render_texture);
  return NULL;
}

static void
gsk_renderer_real_render (GskRenderer          *self,
                          GskRenderNode        *root,
                          const cairo_region_t *region)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, render);
}

static void
gsk_renderer_dispose (GObject *gobject)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  G_GNUC_UNUSED GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  /* We can't just unrealize here because superclasses have already run dispose.
   * So we insist that unrealize must be called before unreffing. */
  g_assert (!priv->is_realized);

  G_OBJECT_CLASS (gsk_renderer_parent_class)->dispose (gobject);
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
    case PROP_REALIZED:
      g_value_set_boolean (value, priv->is_realized);
      break;

    case PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gsk_renderer_class_init (GskRendererClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->realize = gsk_renderer_real_realize;
  klass->unrealize = gsk_renderer_real_unrealize;
  klass->render = gsk_renderer_real_render;
  klass->render_texture = gsk_renderer_real_render_texture;

  gobject_class->get_property = gsk_renderer_get_property;
  gobject_class->dispose = gsk_renderer_dispose;

  /**
   * GskRenderer:realized: (getter is_realized)
   *
   * Whether the renderer has been associated with a surface or draw context.
   */
  gsk_renderer_properties[PROP_REALIZED] =
    g_param_spec_boolean ("realized", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GskRenderer:surface:
   *
   * The surface associated with renderer.
   */
  gsk_renderer_properties[PROP_SURFACE] =
    g_param_spec_object ("surface", NULL, NULL,
                         GDK_TYPE_SURFACE,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, gsk_renderer_properties);
}

static void
gsk_renderer_init (GskRenderer *self)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  priv->debug_flags = gsk_get_debug_flags ();
}

/**
 * gsk_renderer_get_surface:
 * @renderer: a renderer
 *
 * Retrieves the surface that the renderer is associated with.
 *
 * If the renderer has not been realized yet, `NULL` will be returned.
 *
 * Returns: (transfer none) (nullable): the surface
 */
GdkSurface *
gsk_renderer_get_surface (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->surface;
}

/**
 * gsk_renderer_is_realized: (get-property realized)
 * @renderer: a renderer
 *
 * Checks whether the renderer is realized or not.
 *
 * Returns: true if the renderer was realized, false otherwise
 */
gboolean
gsk_renderer_is_realized (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  return priv->is_realized;
}

static gboolean
gsk_renderer_do_realize (GskRenderer  *renderer,
                         GdkDisplay   *display,
                         GdkSurface   *surface,
                         gboolean      attach,
                         GError      **error)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_assert (surface != NULL || !attach);

  if (surface)
    priv->surface = g_object_ref (surface);

  if (!GSK_RENDERER_GET_CLASS (renderer)->realize (renderer, display, surface, attach, error))
    {
      g_clear_object (&priv->surface);
      return FALSE;
    }

  priv->is_realized = TRUE;

  g_object_notify (G_OBJECT (renderer), "realized");
  if (surface)
    g_object_notify (G_OBJECT (renderer), "surface");

  return TRUE;
}

/**
 * gsk_renderer_realize:
 * @renderer: a renderer
 * @surface: (nullable): the surface that renderer will be used on
 * @error: return location for an error
 *
 * Creates the resources needed by the renderer.
 *
 * Since GTK 4.6, the surface may be `NULL`, which allows using
 * renderers without having to create a surface. Since GTK 4.14,
 * it is recommended to use [method@Gsk.Renderer.realize_for_display]
 * for this case.
 *
 * Note that it is mandatory to call [method@Gsk.Renderer.unrealize]
 * before destroying the renderer.
 *
 * Returns: whether the renderer was successfully realized
 */
gboolean
gsk_renderer_realize (GskRenderer  *renderer,
                      GdkSurface   *surface,
                      GError      **error)
{
  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);
  g_return_val_if_fail (!gsk_renderer_is_realized (renderer), FALSE);
  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (surface == NULL)
    {
      return gsk_renderer_do_realize (renderer,
                                      gdk_display_get_default (),
                                      NULL,
                                      FALSE,
                                      error);
    }
  else
    {
      return gsk_renderer_do_realize (renderer,
                                      gdk_surface_get_display (surface),
                                      surface,
                                      FALSE,
                                      error);
    }
}

/**
 * gsk_renderer_realize_for_display:
 * @renderer: a renderer
 * @display: the display that the renderer will be used on
 * @error: return location for an error
 *
 * Creates the resources needed by the renderer.
 *
 * Note that it is mandatory to call [method@Gsk.Renderer.unrealize]
 * before destroying the renderer.
 *
 * Returns: whether the renderer was successfully realized
 *
 * Since: 4.14
 */
gboolean
gsk_renderer_realize_for_display (GskRenderer  *renderer,
                                  GdkDisplay   *display,
                                  GError      **error)
{
  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);
  g_return_val_if_fail (!gsk_renderer_is_realized (renderer), FALSE);
  g_return_val_if_fail (display == NULL || GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return gsk_renderer_do_realize (renderer, display, NULL, FALSE,error);
}

/**
 * gsk_renderer_unrealize:
 * @renderer: a renderer
 *
 * Releases all the resources created by [method@Gsk.Renderer.realize].
 */
void
gsk_renderer_unrealize (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  gboolean has_surface;

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (!priv->is_realized)
    return;

  has_surface = priv->surface != NULL;

  GSK_RENDERER_GET_CLASS (renderer)->unrealize (renderer);

  g_clear_object (&priv->surface);
  g_clear_pointer (&priv->prev_node, gsk_render_node_unref);

  priv->is_realized = FALSE;

  g_object_notify (G_OBJECT (renderer), "realized");
  if (has_surface)
    g_object_notify (G_OBJECT (renderer), "surface");
}

/**
 * gsk_renderer_render_texture:
 * @renderer: a realized renderer
 * @root: the render node to render
 * @viewport: (nullable): the section to draw or `NULL` to use @root's bounds
 *
 * Renders a scene graph, described by a tree of `GskRenderNode` instances,
 * to a texture.
 *
 * The renderer will acquire a reference on the `GskRenderNode` tree while
 * the rendering is in progress.
 *
 * If you want to apply any transformations to @root, you should put it into a
 * transform node and pass that node instead.
 *
 * Returns: (transfer full): a texture with the rendered contents of @root
 */
GdkTexture *
gsk_renderer_render_texture (GskRenderer           *renderer,
                             GskRenderNode         *root,
                             const graphene_rect_t *viewport)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  graphene_rect_t real_viewport;
  GdkTexture *texture;

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);
  g_return_val_if_fail (priv->is_realized, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (root), NULL);

  if (viewport == NULL)
    {
      gsk_render_node_get_bounds (root, &real_viewport);
      viewport = &real_viewport;
    }
  g_return_val_if_fail (viewport->size.width > 0, NULL);
  g_return_val_if_fail (viewport->size.height > 0, NULL);

  texture = GSK_RENDERER_GET_CLASS (renderer)->render_texture (renderer, root, viewport);

  return texture;
}

/**
 * gsk_renderer_render:
 * @renderer: a realized renderer
 * @root: the render node to render
 * @region: (nullable): the `cairo_region_t` that must be redrawn or `NULL`
 *   for the whole surface
 *
 * Renders the scene graph, described by a tree of `GskRenderNode` instances
 * to the renderer's surface, ensuring that the given region gets redrawn.
 *
 * If the renderer has no associated surface, this function does nothing.
 *
 * Renderers must ensure that changes of the contents given by the @root
 * node as well as the area given by @region are redrawn. They are however
 * free to not redraw any pixel outside of @region if they can guarantee that
 * it didn't change.
 *
 * The renderer will acquire a reference on the `GskRenderNode` tree while
 * the rendering is in progress.
 */
void
gsk_renderer_render (GskRenderer          *renderer,
                     GskRenderNode        *root,
                     const cairo_region_t *region)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  GskRendererClass *renderer_class;
  cairo_region_t *clip;
  GskOffload *offload;

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (priv->is_realized);
  g_return_if_fail (GSK_IS_RENDER_NODE (root));

  if (priv->surface == NULL)
    return;

  renderer_class = GSK_RENDERER_GET_CLASS (renderer);

  clip = cairo_region_copy (region);

  if (renderer_class->supports_offload && gdk_has_feature (GDK_FEATURE_OFFLOAD))
    offload = gsk_offload_new (priv->surface, root, clip);
  else
    offload = NULL;

  if (region == NULL || priv->prev_node == NULL || GSK_RENDERER_DEBUG_CHECK (renderer, FULL_REDRAW))
    {
      cairo_region_union_rectangle (clip,
                                    &(GdkRectangle) {
                                        0, 0,
                                        gdk_surface_get_width (priv->surface),
                                        gdk_surface_get_height (priv->surface)
                                    });
    }
  else
    {
      gsk_render_node_diff (priv->prev_node, root, &(GskDiffData) { clip, priv->surface });
    }

  renderer_class->render (renderer, root, clip);

  g_clear_pointer (&priv->prev_node, gsk_render_node_unref);
  cairo_region_destroy (clip);
  g_clear_pointer (&offload, gsk_offload_free);
  priv->prev_node = gsk_render_node_ref (root);
}

static GType
get_renderer_for_name (const char *renderer_name)
{
  if (renderer_name == NULL)
    return G_TYPE_INVALID;
#ifdef GDK_WINDOWING_BROADWAY
  else if (g_ascii_strcasecmp (renderer_name, "broadway") == 0)
    return GSK_TYPE_BROADWAY_RENDERER;
#endif
  else if (g_ascii_strcasecmp (renderer_name, "cairo") == 0)
    return GSK_TYPE_CAIRO_RENDERER;
  else if (g_ascii_strcasecmp (renderer_name, "gl") == 0 ||
           g_ascii_strcasecmp (renderer_name, "opengl") == 0)
    return GSK_TYPE_GL_RENDERER;
  else if (g_ascii_strcasecmp (renderer_name, "ngl") == 0)
    {
      g_warning ("The new GL renderer has been renamed to gl. Try GSK_RENDERER=help");
      return GSK_TYPE_GL_RENDERER;
    }
#ifdef GDK_RENDERING_VULKAN
  else if (g_ascii_strcasecmp (renderer_name, "vulkan") == 0)
    return GSK_TYPE_VULKAN_RENDERER;
#endif
  else if (g_ascii_strcasecmp (renderer_name, "help") == 0)
    {
      gdk_help_message ("Supported arguments for GSK_RENDERER environment variable:\n"
#ifdef GDK_WINDOWING_BROADWAY
                        "  broadway - Use the Broadway specific renderer\n"
#else
                        "  broadway - Disabled during GTK build\n"
#endif
                        "     cairo - Use the Cairo fallback renderer\n"
                        "    opengl - Use the OpenGL renderer\n"
                        "        gl - Use the OpenGL renderer\n"
#ifdef GDK_RENDERING_VULKAN
                        "    vulkan - Use the Vulkan renderer\n"
#else
                        "    vulkan - Disabled during GTK build\n"
#endif
                        "      help - Print this help\n\n"
                        "The old OpenGL renderer has been removed in GTK 4.18, so using\n"
                        "GSK_RENDERER=gl will cause a warning and use the new OpenGL renderer.\n\n"
                        "Other arguments will cause a warning and be ignored.");
    }
  else
    {
      g_warning ("Unrecognized renderer \"%s\". Try GSK_RENDERER=help", renderer_name);
    }

  return G_TYPE_INVALID;
}

static GType
get_renderer_for_display (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  const char *renderer_name;

  renderer_name = g_object_get_data (G_OBJECT (display), "gsk-renderer");
  return get_renderer_for_name (renderer_name);
}

static GType
get_renderer_for_env_var (GdkSurface *surface)
{
  static GType env_var_type = G_TYPE_INVALID;

  if (env_var_type == G_TYPE_INVALID)
    {
      const char *renderer_name = g_getenv ("GSK_RENDERER");
      env_var_type = get_renderer_for_name (renderer_name);
      if (env_var_type != G_TYPE_INVALID)
        GSK_DEBUG (RENDERER,
                   "Environment variable GSK_RENDERER=%s set, trying %s",
                   renderer_name,
                   g_type_name (env_var_type));
    }

  return env_var_type;
}

static GType
get_renderer_for_backend (GdkSurface *surface)
{
#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_SURFACE (surface))
    return GSK_TYPE_BROADWAY_RENDERER;
#endif

  return G_TYPE_INVALID;
}

static gboolean
gl_supported_platform (GdkSurface *surface,
                       gboolean    as_fallback)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkGLContext *context;
  GError *error = NULL;

  if (!gdk_display_prepare_gl (display, &error))
    {
      GSK_DEBUG (RENDERER, "Not using GL%s: %s",
                 as_fallback ? " as fallback" : "",
                 error->message);
      g_clear_error (&error);
      return FALSE;
    }

  if (as_fallback)
    return TRUE;

  context = gdk_display_get_gl_context (display);
  gdk_gl_context_make_current (context);

  if (strstr ((const char *) glGetString (GL_RENDERER), "llvmpipe") != NULL)
    {
      GSK_DEBUG (RENDERER, "Not using GL: renderer is llvmpipe");
      return FALSE;
    }

  return TRUE;
}

static GType
get_renderer_for_gl (GdkSurface *surface)
{
  if (!gl_supported_platform (surface, FALSE))
    return G_TYPE_INVALID;

  return GSK_TYPE_GL_RENDERER;
}

static GType
get_renderer_for_gl_fallback (GdkSurface *surface)
{
  if (!gl_supported_platform (surface, TRUE))
    return G_TYPE_INVALID;

  return GSK_TYPE_GL_RENDERER;
}

#ifdef GDK_RENDERING_VULKAN
static gboolean
vulkan_supported_platform (GdkSurface *surface,
                           gboolean    as_fallback)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  VkPhysicalDeviceProperties props;
  GError *error = NULL;
  gboolean platform_is_wayland;

#ifdef GDK_WINDOWING_WAYLAND
  platform_is_wayland = GDK_IS_WAYLAND_DISPLAY (display);
#else
  platform_is_wayland = FALSE;
#endif

  if (!platform_is_wayland && !as_fallback)
    {
      GSK_DEBUG (RENDERER, "Not using Vulkan: platform is not Wayland");
      return FALSE;
    }

  if (!gdk_display_prepare_vulkan (display, &error))
    {
      GSK_DEBUG (RENDERER, "Not using Vulkan%s: %s",
                 as_fallback ? " as fallback" : "",
                 error->message);
      g_clear_error (&error);
      return FALSE;
    }

  if (as_fallback)
    return TRUE;

  vkGetPhysicalDeviceProperties (display->vk_physical_device, &props);

  if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
    {
      GSK_DEBUG (RENDERER, "Not using Vulkan: device is CPU");
      return FALSE;
    }

#ifdef HAVE_DMABUF
  gdk_vulkan_init_dmabuf (display);
  if (!display->vk_dmabuf_formats ||
      gdk_dmabuf_formats_get_n_formats (display->vk_dmabuf_formats) == 0)
    {
      GSK_DEBUG (RENDERER, "Not using Vulkan: no dmabuf support");
      return FALSE;
    }
#endif

  return TRUE;
}

static GType
get_renderer_for_vulkan (GdkSurface *surface)
{
  if (!vulkan_supported_platform (surface, FALSE))
    return G_TYPE_INVALID;

  return GSK_TYPE_VULKAN_RENDERER;
}

static GType
get_renderer_for_vulkan_fallback (GdkSurface *surface)
{
  if (!vulkan_supported_platform (surface, TRUE))
    return G_TYPE_INVALID;

  return GSK_TYPE_VULKAN_RENDERER;
}
#endif

static GType
get_renderer_fallback (GdkSurface *surface)
{
  return GSK_TYPE_CAIRO_RENDERER;
}

static struct {
  GType (* get_renderer) (GdkSurface *surface);
} renderer_possibilities[] = {
  { get_renderer_for_display },
  { get_renderer_for_env_var },
  { get_renderer_for_backend },
#ifdef GDK_RENDERING_VULKAN
  { get_renderer_for_vulkan },
#endif
  { get_renderer_for_gl },
#ifdef GDK_RENDERING_VULKAN
  { get_renderer_for_vulkan_fallback },
#endif
  { get_renderer_for_gl_fallback },
  { get_renderer_fallback },
};

GskRenderer *
gsk_renderer_new_for_surface_full (GdkSurface *surface,
                                   gboolean    attach)
{
  GType renderer_type;
  GskRenderer *renderer;
  GError *error = NULL;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (renderer_possibilities); i++)
    {
      renderer_type = renderer_possibilities[i].get_renderer (surface);
      if (renderer_type == G_TYPE_INVALID)
        continue;

      renderer = g_object_new (renderer_type, NULL);

      if (gsk_renderer_do_realize (renderer, gdk_surface_get_display (surface), surface, attach, &error))
        {
          GSK_DEBUG (RENDERER,
                     "Using renderer '%s' for surface '%s'",
                     G_OBJECT_TYPE_NAME (renderer),
                     G_OBJECT_TYPE_NAME (surface));
          return renderer;
        }

      GSK_DEBUG (RENDERER,
                 "Failed to realize renderer '%s' for surface '%s': %s",
                 G_OBJECT_TYPE_NAME (renderer),
                 G_OBJECT_TYPE_NAME (surface),
                 error->message);

      g_object_unref (renderer);
      g_clear_error (&error);
    }

  g_assert_not_reached ();
  return NULL;
}

/**
 * gsk_renderer_new_for_surface:
 * @surface: a surface
 *
 * Creates an appropriate `GskRenderer` instance for the given surface.
 *
 * If the `GSK_RENDERER` environment variable is set, GSK will
 * try that renderer first, before trying the backend-specific
 * default. The ultimate fallback is the cairo renderer.
 *
 * The renderer will be realized before it is returned.
 *
 * Returns: (transfer full) (nullable): the realized renderer
 */
GskRenderer *
gsk_renderer_new_for_surface (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return gsk_renderer_new_for_surface_full (surface, FALSE);
}

GskDebugFlags
gsk_renderer_get_debug_flags (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), 0);

  return priv->debug_flags;
}

void
gsk_renderer_set_debug_flags (GskRenderer   *renderer,
                              GskDebugFlags  flags)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  priv->debug_flags = flags;
}
