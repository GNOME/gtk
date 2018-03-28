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
 * Typically you will use a #GskRenderer instance to repeatedly call
 * gsk_renderer_render() to update the contents of its associated #GdkSurface.
 *
 * It is necessary to realize a #GskRenderer instance using gsk_renderer_realize()
 * before calling gsk_renderer_render(), in order to create the appropriate
 * windowing system resources needed to render the scene.
 */

#include "config.h"

#include "gskrendererprivate.h"

#include "gskcairorendererprivate.h"
#include "gskdebugprivate.h"
#include "gl/gskglrendererprivate.h"
#include "gskprofilerprivate.h"
#include "gskrendernodeprivate.h"

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
#ifdef GDK_WINDOWING_BROADWAY
#include "gskbroadwayrendererprivate.h"
#endif
#ifdef GDK_RENDERING_VULKAN
#include "vulkan/gskvulkanrendererprivate.h"
#endif

typedef struct
{
  GObject parent_instance;

  GdkSurface *surface;
  GskRenderNode *root_node;
  GdkDisplay *display;

  GskProfiler *profiler;

  GskDebugFlags debug_flags;

  gboolean is_realized : 1;
} GskRendererPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GskRenderer, gsk_renderer, G_TYPE_OBJECT)

enum {
  PROP_SURFACE = 1,
  PROP_DISPLAY,

  N_PROPS
};

static GParamSpec *gsk_renderer_properties[N_PROPS];

#define GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Renderer of type '%s' does not implement GskRenderer::" # method, G_OBJECT_TYPE_NAME (obj))

static gboolean
gsk_renderer_real_realize (GskRenderer  *self,
                           GdkSurface    *surface,
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
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  /* We can't just unrealize here because superclasses have already run dispose.
   * So we insist that unrealize must be called before unreffing. */
  g_assert (!priv->is_realized);

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
    case PROP_DISPLAY:
      /* Construct-only */
      priv->display = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
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
    case PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
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
  klass->render_texture = gsk_renderer_real_render_texture;

  gobject_class->constructed = gsk_renderer_constructed;
  gobject_class->set_property = gsk_renderer_set_property;
  gobject_class->get_property = gsk_renderer_get_property;
  gobject_class->dispose = gsk_renderer_dispose;

  /**
   * GskRenderer:display:
   *
   * The #GdkDisplay used by the #GskRenderer.
   */
  gsk_renderer_properties[PROP_DISPLAY] =
    g_param_spec_object ("display",
			 "Display",
			 "The GdkDisplay object used by the renderer",
			 GDK_TYPE_DISPLAY,
			 G_PARAM_READWRITE |
			 G_PARAM_CONSTRUCT_ONLY |
			 G_PARAM_STATIC_STRINGS);

  gsk_renderer_properties[PROP_SURFACE] =
    g_param_spec_object ("surface",
                         "Surface",
                         "The surface associated to the renderer",
                         GDK_TYPE_SURFACE,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, gsk_renderer_properties);
}

static void
gsk_renderer_init (GskRenderer *self)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  priv->profiler = gsk_profiler_new ();
  priv->debug_flags = gsk_get_debug_flags ();
}

/**
 * gsk_renderer_get_surface:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkSurface set using gsk_renderer_realize(). If the renderer
 * has not been realized yet, %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): a #GdkSurface
 */
GdkSurface *
gsk_renderer_get_surface (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->surface;
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

/**
 * gsk_renderer_get_display:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkDisplay used when creating the #GskRenderer.
 *
 * Returns: (transfer none): a #GdkDisplay
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
 * @surface: the #GdkSurface renderer will be used on
 * @error: return location for an error
 *
 * Creates the resources needed by the @renderer to render the scene
 * graph.
 */
gboolean
gsk_renderer_realize (GskRenderer  *renderer,
                      GdkSurface    *surface,
                      GError      **error)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);
  g_return_val_if_fail (!gsk_renderer_is_realized (renderer), FALSE);
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  priv->surface = g_object_ref (surface);

  if (!GSK_RENDERER_GET_CLASS (renderer)->realize (renderer, surface, error))
    {
      g_clear_object (&priv->surface);
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
 * to a #GdkTexture.
 *
 * The @renderer will acquire a reference on the #GskRenderNode tree while
 * the rendering is in progress, and will make the tree immutable.
 *
 * If you want to apply any transformations to @root, you should put it into a 
 * transform node and pass that node instead.
 *
 * Returns: (transfer full): a #GdkTexture with the rendered contents of @root.
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
  g_return_val_if_fail (priv->root_node == NULL, NULL);

  priv->root_node = gsk_render_node_ref (root);

  if (viewport == NULL)
    {
      gsk_render_node_get_bounds (root, &real_viewport);
      viewport = &real_viewport;
    }

  texture = GSK_RENDERER_GET_CLASS (renderer)->render_texture (renderer, root, viewport);

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (renderer, RENDERER))
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
 * @region: the #cairo_region_t that must be redrawn or %NULL for the whole
 *     window
 *
 * Renders the scene graph, described by a tree of #GskRenderNode instances,
 * ensuring that the given @region gets redrawn.
 *
 * Renderers must ensure that changes of the contents given by the @root
 * node as well as the area given by @region are redrawn. They are however
 * free to not redraw any pixel outside of @region if they can guarantee that
 * it didn't change.
 *
 * The @renderer will acquire a reference on the #GskRenderNode tree while
 * the rendering is in progress.
 */
void
gsk_renderer_render (GskRenderer          *renderer,
                     GskRenderNode        *root,
                     const cairo_region_t *region)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  cairo_region_t *real_region;

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (priv->is_realized);
  g_return_if_fail (GSK_IS_RENDER_NODE (root));
  g_return_if_fail (priv->root_node == NULL);

  priv->root_node = gsk_render_node_ref (root);

  if (region == NULL || GSK_RENDERER_DEBUG_CHECK (renderer, FULL_REDRAW))
    {
      real_region = cairo_region_create_rectangle (&(GdkRectangle) {
                                                       0, 0,
                                                       gdk_surface_get_width (priv->surface),
                                                       gdk_surface_get_height (priv->surface)
                                                   });

      GSK_RENDERER_GET_CLASS (renderer)->render (renderer, root, real_region);

      cairo_region_destroy (real_region);
    }
  else
    {
      GSK_RENDERER_GET_CLASS (renderer)->render (renderer, root, region);
    }

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (renderer, RENDERER))
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
  static GType env_var_type = G_TYPE_NONE;

  if (env_var_type == G_TYPE_NONE)
    {
      const char *renderer_name = g_getenv ("GSK_RENDERER");
      env_var_type = get_renderer_for_name (renderer_name);
    }

  return env_var_type;
}

static GType
get_renderer_for_backend (GdkSurface *surface)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_SURFACE (surface))
    return GSK_TYPE_GL_RENDERER; 
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_SURFACE (surface))
    return GSK_TYPE_GL_RENDERER;
#endif
#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_SURFACE (surface))
    return GSK_TYPE_BROADWAY_RENDERER;
#endif

  return G_TYPE_INVALID;
}

static GType
get_renderer_fallback (GdkSurface *surface)
{
  return GSK_TYPE_CAIRO_RENDERER;
}

static struct {
  gboolean verbose;
  GType (* get_renderer) (GdkSurface *surface);
} renderer_possibilities[] = {
  { TRUE,  get_renderer_for_display },
  { TRUE,  get_renderer_for_env_var },
  { FALSE, get_renderer_for_backend },
  { FALSE, get_renderer_fallback },
};

/**
 * gsk_renderer_new_for_surface:
 * @surface: a #GdkSurface
 *
 * Creates an appropriate #GskRenderer instance for the given @surface.
 *
 * The renderer will be realized when it is returned.
 *
 * Returns: (transfer full) (nullable): a #GskRenderer
 */
GskRenderer *
gsk_renderer_new_for_surface (GdkSurface *surface)
{
  GType renderer_type;
  GskRenderer *renderer;
  GError *error = NULL;
  gboolean verbose = FALSE;
  guint i;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  for (i = 0; i < G_N_ELEMENTS (renderer_possibilities); i++)
    {
      renderer_type = renderer_possibilities[i].get_renderer (surface);
      if (renderer_type == G_TYPE_INVALID)
        continue;

      /* If a renderer is selected that's marked as verbose, start printing
       * information to stdout.
       */
      verbose |= renderer_possibilities[i].verbose;
      renderer = g_object_new (renderer_type,
                               "display", gdk_surface_get_display (surface),
                               NULL);

      if (gsk_renderer_realize (renderer, surface, &error))
        {
          if (verbose || GSK_RENDERER_DEBUG_CHECK (renderer, RENDERER))
            {
              g_print ("Using renderer of type '%s' for surface '%s'\n",
                       G_OBJECT_TYPE_NAME (renderer),
                       G_OBJECT_TYPE_NAME (surface));
            }
          return renderer;
        }

      if (verbose || GSK_RENDERER_DEBUG_CHECK (renderer, RENDERER))
        {
          g_print ("Failed to realize renderer of type '%s' for surface '%s': %s\n",
                   G_OBJECT_TYPE_NAME (renderer),
                   G_OBJECT_TYPE_NAME (surface),
                   error->message);
        }
      g_object_unref (renderer);
      g_clear_error (&error);
    }

  g_assert_not_reached ();
  return NULL;
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
