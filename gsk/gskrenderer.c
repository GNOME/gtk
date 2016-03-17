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
 * @title: GskRenderer
 * @Short_desc: Renders a scene with a simplified graph
 *
 * TODO
 */

#include "config.h"

#include "gskrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskcairorendererprivate.h"
#include "gskglrendererprivate.h"
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

typedef struct
{
  GObject parent_instance;

  GdkDisplay *display;
  GdkWindow *window;

  graphene_rect_t viewport;
  graphene_matrix_t modelview;
  graphene_matrix_t projection;

  GskScalingFilter min_filter;
  GskScalingFilter mag_filter;

  GskRenderNode *root_node;

  cairo_surface_t *surface;
  cairo_t *draw_context;

  gboolean is_realized : 1;
  gboolean needs_viewport_resize : 1;
  gboolean needs_modelview_update : 1;
  gboolean needs_projection_update : 1;
  gboolean needs_tree_validation : 1;
  gboolean auto_clear : 1;
  gboolean use_alpha : 1;
} GskRendererPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GskRenderer, gsk_renderer, G_TYPE_OBJECT)

enum {
  PROP_VIEWPORT = 1,
  PROP_MODELVIEW,
  PROP_PROJECTION,
  PROP_MINIFICATION_FILTER,
  PROP_MAGNIFICATION_FILTER,
  PROP_AUTO_CLEAR,
  PROP_ROOT_NODE,
  PROP_DISPLAY,
  PROP_WINDOW,
  PROP_SURFACE,
  PROP_USE_ALPHA,

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
gsk_renderer_real_render (GskRenderer *self)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, render);
}

static void
gsk_renderer_real_resize_viewport (GskRenderer *self,
                                   const graphene_rect_t *viewport)
{
}

static void
gsk_renderer_real_update (GskRenderer *self,
                          const graphene_matrix_t *mv,
                          const graphene_matrix_t *proj)
{
}

static void
gsk_renderer_real_validate_tree (GskRenderer *self,
                                 GskRenderNode *root)
{
}

static void
gsk_renderer_dispose (GObject *gobject)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  gsk_renderer_unrealize (self);

  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  g_clear_pointer (&priv->draw_context, cairo_destroy);

  g_clear_object (&priv->window);
  g_clear_object (&priv->root_node);
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

    case PROP_MODELVIEW:
      gsk_renderer_set_modelview (self, g_value_get_boxed (value));
      break;

    case PROP_PROJECTION:
      gsk_renderer_set_projection (self, g_value_get_boxed (value));
      break;

    case PROP_MINIFICATION_FILTER:
      gsk_renderer_set_scaling_filters (self, g_value_get_enum (value), priv->mag_filter);
      break;

    case PROP_MAGNIFICATION_FILTER:
      gsk_renderer_set_scaling_filters (self, priv->min_filter, g_value_get_enum (value));
      break;

    case PROP_AUTO_CLEAR:
      gsk_renderer_set_auto_clear (self, g_value_get_boolean (value));
      break;

    case PROP_ROOT_NODE:
      gsk_renderer_set_root_node (self, g_value_get_object (value));
      break;

    case PROP_SURFACE:
      gsk_renderer_set_surface (self, g_value_get_boxed (value));
      break;

    case PROP_WINDOW:
      gsk_renderer_set_window (self, g_value_get_object (value));
      break;

    case PROP_DISPLAY:
      priv->display = g_value_dup_object (value);
      break;

    case PROP_USE_ALPHA:
      gsk_renderer_set_use_alpha (self, g_value_get_boolean (value));
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

    case PROP_MODELVIEW:
      g_value_set_boxed (value, &priv->modelview);
      break;

    case PROP_PROJECTION:
      g_value_set_boxed (value, &priv->projection);
      break;

    case PROP_MINIFICATION_FILTER:
      g_value_set_enum (value, priv->min_filter);
      break;

    case PROP_MAGNIFICATION_FILTER:
      g_value_set_enum (value, priv->mag_filter);
      break;

    case PROP_AUTO_CLEAR:
      g_value_set_boolean (value, priv->auto_clear);
      break;

    case PROP_ROOT_NODE:
      g_value_set_object (value, priv->root_node);
      break;

    case PROP_SURFACE:
      g_value_set_boxed (value, priv->surface);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;

    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
      break;

    case PROP_USE_ALPHA:
      g_value_set_boolean (value, priv->use_alpha);
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
  klass->resize_viewport = gsk_renderer_real_resize_viewport;
  klass->update = gsk_renderer_real_update;
  klass->validate_tree = gsk_renderer_real_validate_tree;
  klass->render = gsk_renderer_real_render;

  gobject_class->constructed = gsk_renderer_constructed;
  gobject_class->set_property = gsk_renderer_set_property;
  gobject_class->get_property = gsk_renderer_get_property;
  gobject_class->dispose = gsk_renderer_dispose;

  /**
   * GskRenderer:viewport:
   *
   * The visible area used by the #GskRenderer to render its contents.
   *
   * Since: 3.22
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
   * GskRenderer:modelview:
   *
   * The initial modelview matrix used by the #GskRenderer.
   *
   * If set to %NULL, the identity matrix:
   *
   * |[<!-- language="plain"
   *   | 1.0, 0.0, 0.0, 0.0 |
   *   | 0.0, 1.0, 0.0, 0.0 |
   *   | 0.0, 0.0, 1.0, 0.0 |
   *   | 0.0, 0.0, 0.0, 1.0 |
   * ]|
   *
   * Is used instead.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_MODELVIEW] =
    g_param_spec_boxed ("modelview",
			"Modelview",
			"The modelview matrix used by the renderer",
			GRAPHENE_TYPE_MATRIX,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:projection:
   *
   * The projection matrix used by the #GskRenderer.
   *
   * If set to %NULL, the identity matrix:
   *
   * |[<!-- language="plain"
   *   | 1.0, 0.0, 0.0, 0.0 |
   *   | 0.0, 1.0, 0.0, 0.0 |
   *   | 0.0, 0.0, 1.0, 0.0 |
   *   | 0.0, 0.0, 0.0, 1.0 |
   * ]|
   *
   * Is used instead.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_PROJECTION] =
    g_param_spec_boxed ("projection",
			"Projection",
			"The projection matrix used by the renderer",
			GRAPHENE_TYPE_MATRIX,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:minification-filter:
   *
   * The filter to be used when scaling textures down.
   *
   * See also: gsk_renderer_set_scaling_filters()
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_MINIFICATION_FILTER] =
    g_param_spec_enum ("minification-filter",
                       "Minification Filter",
                       "The minification filter used by the renderer for texture targets",
                       GSK_TYPE_SCALING_FILTER,
                       GSK_SCALING_FILTER_LINEAR,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:magnification-filter:
   *
   * The filter to be used when scaling textures up.
   *
   * See also: gsk_renderer_set_scaling_filters()
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_MAGNIFICATION_FILTER] =
    g_param_spec_enum ("magnification-filter",
                       "Magnification Filter",
                       "The magnification filter used by the renderer for texture targets",
                       GSK_TYPE_SCALING_FILTER,
                       GSK_SCALING_FILTER_LINEAR,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:auto-clear:
   *
   * Automatically clear the rendering surface when rendering.
   *
   * Setting this property to %FALSE assumes that the owner of the
   * rendering surface will have cleared it prior to calling
   * gsk_renderer_render().
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_AUTO_CLEAR] =
    g_param_spec_boolean ("auto-clear",
                          "Auto Clear",
                          "Automatically clears the rendering target on render",
                          TRUE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:root-node:
   *
   * The root #GskRenderNode of the scene to be rendered.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_ROOT_NODE] =
    g_param_spec_object ("root-node",
                         "Root Node",
                         "The root render node to render",
                         GSK_TYPE_RENDER_NODE,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:surface:
   *
   * The target rendering surface.
   *
   * See also: #GskRenderer:window.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_SURFACE] =
    g_param_spec_boxed ("surface",
			"Surface",
			"The Cairo surface used to render to",
			CAIRO_GOBJECT_TYPE_SURFACE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS |
			G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:display:
   *
   * The #GdkDisplay used by the #GskRenderer.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_DISPLAY] =
    g_param_spec_object ("display",
			 "Display",
			 "The GdkDisplay object used by the renderer",
			 GDK_TYPE_DISPLAY,
			 G_PARAM_READWRITE |
			 G_PARAM_CONSTRUCT_ONLY |
			 G_PARAM_STATIC_STRINGS);

  /**
   * GskRenderer:window:
   *
   * The #GdkWindow used to create a target surface, if #GskRenderer:surface
   * is not explicitly set.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "Window",
                         "The GdkWindow associated to the renderer",
                         GDK_TYPE_WINDOW,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GskRenderer:use-alpha:
   *
   * Whether the #GskRenderer should use the alpha channel when rendering.
   *
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_USE_ALPHA] =
    g_param_spec_boolean ("use-alpha",
                          "Use Alpha",
                          "Whether the renderer should use the alpha channel when rendering",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, gsk_renderer_properties);
}

static void
gsk_renderer_init (GskRenderer *self)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  graphene_matrix_init_identity (&priv->modelview);
  graphene_matrix_init_identity (&priv->projection);

  priv->auto_clear = TRUE;

  priv->min_filter = GSK_SCALING_FILTER_LINEAR;
  priv->mag_filter = GSK_SCALING_FILTER_LINEAR;
}

/**
 * gsk_renderer_set_viewport:
 * @renderer: a #GskRenderer
 * @viewport: (nullable): the viewport rectangle used by the @renderer
 *
 * Sets the visible rectangle to be used as the viewport for
 * the rendering.
 *
 * Since: 3.22
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
  priv->needs_viewport_resize = TRUE;
  priv->needs_modelview_update = TRUE;
  priv->needs_projection_update = TRUE;

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_VIEWPORT]);
}

/**
 * gsk_renderer_get_viewport:
 * @renderer: a #GskRenderer
 * @viewport: (out caller-allocates): return location for the viewport rectangle
 *
 * Retrieves the viewport of the #GskRenderer.
 *
 * Since: 3.22
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
 * gsk_renderer_set_modelview:
 * @renderer: a #GskRenderer
 * @modelview: the modelview matrix used by the @renderer
 *
 * Sets the initial modelview matrix used by the #GskRenderer.
 *
 * A modelview matrix defines the initial transformation imposed
 * on the scene graph.
 *
 * Since: 3.22
 */
void
gsk_renderer_set_modelview (GskRenderer             *renderer,
                            const graphene_matrix_t *modelview)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (modelview == NULL)
    graphene_matrix_init_identity (&priv->modelview);
  else
    graphene_matrix_init_from_matrix (&priv->modelview, modelview);

  priv->needs_modelview_update = TRUE;

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_MODELVIEW]);
}

/**
 * gsk_renderer_get_modelview:
 * @renderer: a #GskRenderer
 * @modelview: (out caller-allocates): return location for the modelview matrix
 *
 * Retrieves the modelview matrix used by the #GskRenderer.
 *
 * Since: 3.22
 */
void
gsk_renderer_get_modelview (GskRenderer       *renderer,
                            graphene_matrix_t *modelview)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (modelview != NULL);

  graphene_matrix_init_from_matrix (modelview, &priv->modelview);
}

/**
 * gsk_renderer_set_projection:
 * @renderer: a #GskRenderer
 * @projection: the projection matrix used by the @renderer
 *
 * Sets the projection matrix used by the #GskRenderer.
 *
 * Since: 3.22
 */
void
gsk_renderer_set_projection (GskRenderer             *renderer,
                             const graphene_matrix_t *projection)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (projection == NULL)
    graphene_matrix_init_identity (&priv->projection);
  else
    graphene_matrix_init_from_matrix (&priv->projection, projection);

  priv->needs_projection_update = TRUE;

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_PROJECTION]);
}

/**
 * gsk_renderer_get_projection:
 * @renderer: a #GskRenderer
 * @projection: (out caller-allocates): return location for the projection matrix
 *
 * Retrieves the projection matrix used by the #GskRenderer.
 *
 * Since: 3.22
 */
void
gsk_renderer_get_projection (GskRenderer       *renderer,
                             graphene_matrix_t *projection)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (projection != NULL);

  graphene_matrix_init_from_matrix (projection, &priv->projection);
}

static void
gsk_renderer_invalidate_tree (GskRenderNode *node,
                              gpointer       data)
{
  GskRenderer *self = data;
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  GSK_NOTE (RENDERER, g_print ("Invalidating tree.\n"));

  /* Since the scene graph has changed in some way, we need to re-validate it. */
  priv->needs_tree_validation = TRUE;
}

/**
 * gsk_renderer_set_root_node:
 * @renderer: a #GskRenderer
 * @root: (nullable): a #GskRenderNode
 *
 * Sets the root node of the scene graph to be rendered.
 *
 * The #GskRenderer will acquire a reference on @root.
 *
 * Since: 3.22
 */
void
gsk_renderer_set_root_node (GskRenderer   *renderer,
                            GskRenderNode *root)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  GskRenderNode *old_root;

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (GSK_IS_RENDER_NODE (root));

  old_root = priv->root_node != NULL ? g_object_ref (priv->root_node) : NULL;

  if (g_set_object (&priv->root_node, root))
    {
      /* We need to unset the invalidate function on the old instance */
      if (old_root != NULL)
        {
          gsk_render_node_set_invalidate_func (old_root, NULL, NULL, NULL);
          g_object_unref (old_root);
        }

      if (priv->root_node != NULL)
        gsk_render_node_set_invalidate_func (priv->root_node,
                                             gsk_renderer_invalidate_tree,
                                             renderer,
                                             NULL);

      /* If we don't have a root node, there's really no point in validating a
       * tree that it's not going to be drawn
       */
      priv->needs_tree_validation = priv->root_node != NULL;

      g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_ROOT_NODE]);
    }
}

/**
 * gsk_renderer_set_scaling_filters:
 * @renderer: a #GskRenderer
 * @min_filter: the minification scaling filter
 * @mag_filter: the magnification scaling filter
 *
 * Sets the scaling filters to be applied when scaling textures
 * up and down.
 *
 * Since: 3.22
 */
void
gsk_renderer_set_scaling_filters (GskRenderer      *renderer,
                                  GskScalingFilter  min_filter,
                                  GskScalingFilter  mag_filter)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);
  GObject *gobject;

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  gobject = G_OBJECT (renderer);

  g_object_freeze_notify (gobject);

  if (priv->min_filter != min_filter)
    {
      priv->min_filter = min_filter;
      g_object_notify_by_pspec (gobject, gsk_renderer_properties[PROP_MINIFICATION_FILTER]);
    }

  if (priv->mag_filter != mag_filter)
    {
      priv->mag_filter = mag_filter;
      g_object_notify_by_pspec (gobject, gsk_renderer_properties[PROP_MAGNIFICATION_FILTER]);
    }

  g_object_thaw_notify (gobject);
}

/**
 * gsk_renderer_get_scaling_filters:
 * @renderer: a #GskRenderer
 * @min_filter: (out) (nullable): return location for the minification filter
 * @mag_filter: (out) (nullable): return location for the magnification filter
 *
 * Retrieves the minification and magnification filters used by the #GskRenderer.
 *
 * Since: 3.22
 */
void
gsk_renderer_get_scaling_filters (GskRenderer      *renderer,
                                  GskScalingFilter *min_filter,
                                  GskScalingFilter *mag_filter)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (min_filter != NULL)
    *min_filter = priv->min_filter;

  if (mag_filter != NULL)
    *mag_filter = priv->mag_filter;
}

/**
 * gsk_renderer_set_surface:
 * @renderer: a #GskRenderer
 * @surface: (nullable): a Cairo surface
 *
 * Sets the #cairo_surface_t used as the target rendering surface.
 *
 * This function will acquire a reference to @surface.
 *
 * See also: gsk_renderer_set_window()
 *
 * Since: 3.22
 */
void
gsk_renderer_set_surface (GskRenderer     *renderer,
                          cairo_surface_t *surface)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (priv->surface == surface)
    return;

  g_clear_pointer (&priv->surface, cairo_surface_destroy);

  if (surface != NULL)
    priv->surface = cairo_surface_reference (surface);

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_SURFACE]);
}

/**
 * gsk_renderer_get_surface:
 * @renderer: a #GskRenderer
 *
 * Retrieve the target rendering surface used by @renderer.
 *
 * If you did not use gsk_renderer_set_surface(), a compatible surface
 * will be created by using the #GdkWindow passed to gsk_renderer_set_window().
 *
 * Returns: (transfer none) (nullable): a Cairo surface
 *
 * Since: 3.22
 */
cairo_surface_t *
gsk_renderer_get_surface (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  if (priv->surface != NULL)
    return priv->surface;

  if (priv->window != NULL)
    {
      int scale = gdk_window_get_scale_factor (priv->window);
      int width = gdk_window_get_width (priv->window);
      int height = gdk_window_get_height (priv->window);
      cairo_content_t content;

      if (priv->use_alpha)
        content = CAIRO_CONTENT_COLOR_ALPHA;
      else
        content = CAIRO_CONTENT_COLOR;

      GSK_NOTE (RENDERER, g_print ("Creating surface from window [%p] (w:%d, h:%d, s:%d, a:%s)\n",
                                   priv->window,
                                   width, height, scale,
                                   priv->use_alpha ? "y" : "n"));

      priv->surface = gdk_window_create_similar_surface (priv->window,
                                                         content,
                                                         width, height);
    }

  return priv->surface;
}

void
gsk_renderer_set_draw_context (GskRenderer *renderer,
                               cairo_t     *cr)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  if (priv->draw_context == cr)
    return;

  g_clear_pointer (&priv->draw_context, cairo_destroy);
  priv->draw_context = cr != NULL ? cairo_reference (cr) : NULL;
}

cairo_t *
gsk_renderer_get_draw_context (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  if (priv->draw_context != NULL)
    return priv->draw_context;

  return cairo_create (gsk_renderer_get_surface (renderer));
}

/**
 * gsk_renderer_get_display:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkDisplay used when creating the #GskRenderer.
 *
 * Returns: (transfer none): a #GdkDisplay
 *
 * Since: 3.22
 */
GdkDisplay *
gsk_renderer_get_display (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->display;
}

/**
 * gsk_renderer_get_root_node:
 * @renderer: a #GskRenderer
 *
 * Retrieves the root node of the scene graph.
 *
 * Returns: (transfer none) (nullable): a #GskRenderNode
 *
 * Since: 3.22
 */
GskRenderNode *
gsk_renderer_get_root_node (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->root_node;
}

/*< private >
 * gsk_renderer_is_realized:
 * @renderer: a #GskRenderer
 *
 * Checks whether the @renderer is realized or not.
 *
 * Returns: %TRUE if the #GskRenderer was realized, and %FALSE otherwise
 *
 * Since: 3.22
 */
gboolean
gsk_renderer_is_realized (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  return priv->is_realized;
}

/**
 * gsk_renderer_set_window:
 * @renderer: a #GskRenderer
 * @window: (nullable): a #GdkWindow
 *
 * Sets the #GdkWindow used to create the target rendering surface.
 *
 * See also: gsk_renderer_set_surface()
 *
 * Since: 3.22
 */
void
gsk_renderer_set_window (GskRenderer *renderer,
                         GdkWindow   *window)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  g_return_if_fail (!priv->is_realized);

  if (g_set_object (&priv->window, window))
    g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_WINDOW]);
}

/**
 * gsk_renderer_get_window:
 * @renderer: a #GskRenderer
 *
 * Retrieves the #GdkWindow set with gsk_renderer_set_window().
 *
 * Returns: (transfer none) (nullable): a #GdkWindow
 *
 * Since: 3.22
 */
GdkWindow *
gsk_renderer_get_window (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return priv->window;
}

/**
 * gsk_renderer_realize:
 * @renderer: a #GskRenderer
 *
 * Creates the resources needed by the @renderer to render the scene
 * graph.
 *
 * Since: 3.22
 */
gboolean
gsk_renderer_realize (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  if (priv->is_realized)
    return TRUE;

  if (priv->window == NULL && priv->surface == NULL)
    {
      g_critical ("No rendering surface has been set.");
      return FALSE;
    }

  priv->is_realized = GSK_RENDERER_GET_CLASS (renderer)->realize (renderer);

  return priv->is_realized;
}

/**
 * gsk_renderer_unrealize:
 * @renderer: a #GskRenderer
 *
 * Releases all the resources created by gsk_renderer_realize().
 *
 * Since: 3.22
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

/*< private >
 * gsk_renderer_maybe_resize_viewport:
 * @renderer: a #GskRenderer
 *
 * Optionally resize the viewport of @renderer.
 *
 * This function should be called by gsk_renderer_render().
 *
 * This function may call @GskRendererClass.resize_viewport().
 */
void
gsk_renderer_maybe_resize_viewport (GskRenderer *renderer)
{
  GskRendererClass *renderer_class = GSK_RENDERER_GET_CLASS (renderer);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  if (priv->needs_viewport_resize)
    {
      renderer_class->resize_viewport (renderer, &priv->viewport);
      priv->needs_viewport_resize = FALSE;

      GSK_NOTE (RENDERER, g_print ("Viewport size: %g x %g\n",
                                   priv->viewport.size.width,
                                   priv->viewport.size.height));

      /* If the target surface has been created from a window, we need
       * to clear it, so that it gets recreated with the right size
       */
      if (priv->window != NULL && priv->surface != NULL)
        g_clear_pointer (&priv->surface, cairo_surface_destroy);
    }
}

/*< private >
 * gsk_renderer_maybe_update:
 * @renderer: a #GskRenderer
 *
 * Optionally recomputes the modelview-projection matrix used by
 * the @renderer.
 *
 * This function should be called by gsk_renderer_render().
 *
 * This function may call @GskRendererClass.update().
 */
void
gsk_renderer_maybe_update (GskRenderer *renderer)
{
  GskRendererClass *renderer_class = GSK_RENDERER_GET_CLASS (renderer);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  if (priv->needs_modelview_update || priv->needs_projection_update)
    {
      renderer_class->update (renderer, &priv->modelview, &priv->projection);
      priv->needs_modelview_update = FALSE;
      priv->needs_projection_update = FALSE;
    }
}

/*< private >
 * gsk_renderer_maybe_validate_tree:
 * @renderer: a #GskRenderer
 *
 * Optionally validates the #GskRenderNode scene graph, and uses it
 * to generate more efficient intermediate representations depending
 * on the type of @renderer.
 *
 * This function should be called by gsk_renderer_render().
 *
 * This function may call @GskRendererClas.validate_tree().
 */
void
gsk_renderer_maybe_validate_tree (GskRenderer *renderer)
{
  GskRendererClass *renderer_class = GSK_RENDERER_GET_CLASS (renderer);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  if (priv->root_node == NULL)
    return;

  /* Ensure that the render nodes are valid; this will change the
   * needs_tree_validation flag on the renderer, if needed
   */
  gsk_render_node_validate (priv->root_node);

  if (priv->needs_tree_validation)
    {
      /* Ensure that the Renderer can update itself */
      renderer_class->validate_tree (renderer, priv->root_node);
      priv->needs_tree_validation = FALSE;
    }
}

/*< private >
 * gsk_renderer_maybe_clear:
 * @renderer: a #GskRenderer
 *
 * Optionally calls @GskRendererClass.clear(), depending on the value
 * of #GskRenderer:auto-clear.
 *
 * This function should be called by gsk_renderer_render().
 */
void
gsk_renderer_maybe_clear (GskRenderer *renderer)
{
  GskRendererClass *renderer_class = GSK_RENDERER_GET_CLASS (renderer);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  if (priv->auto_clear)
    renderer_class->clear (renderer);
}

/**
 * gsk_renderer_render:
 * @renderer: a#GskRenderer
 *
 * Renders the scene graph associated to @renderer, using the
 * given target surface.
 *
 * Since: 3.22
 */
void
gsk_renderer_render (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (priv->is_realized);
  g_return_if_fail (priv->root_node != NULL);

  /* We need to update the viewport and the modelview, to allow renderers
   * to update their clip region and/or frustum; this allows them to cull
   * render nodes in the tree validation phase
   */
  gsk_renderer_maybe_resize_viewport (renderer);

  gsk_renderer_maybe_update (renderer);

  gsk_renderer_maybe_validate_tree (renderer);

  /* Clear the output surface */
  gsk_renderer_maybe_clear (renderer);

  GSK_RENDERER_GET_CLASS (renderer)->render (renderer);
}

/**
 * gsk_renderer_set_auto_clear:
 * @renderer: a #GskRenderer
 * @clear: whether the target surface should be cleared prior
 *   to rendering to it
 *
 * Sets whether the target surface used by @renderer should be cleared
 * before rendering.
 *
 * If you pass a custom surface to gsk_renderer_set_surface(), you may
 * want to manage the clearing manually; this is possible by passing
 * %FALSE to this function.
 *
 * Since: 3.22
 */
void
gsk_renderer_set_auto_clear (GskRenderer *renderer,
                             gboolean     clear)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));

  clear = !!clear;

  if (clear == priv->auto_clear)
    return;

  priv->auto_clear = clear;

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_AUTO_CLEAR]);
}

/**
 * gsk_renderer_get_auto_clear:
 * @renderer: a #GskRenderer
 *
 * Retrieves the value set using gsk_renderer_set_auto_clear().
 *
 * Returns: %TRUE if the target surface should be cleared prior to rendering
 *
 * Since: 3.22
 */
gboolean
gsk_renderer_get_auto_clear (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  return priv->auto_clear;
}

/**
 * gsk_renderer_set_use_alpha:
 * @renderer: a #GskRenderer
 * @use_alpha: whether to use the alpha channel of the target surface or not
 *
 * Sets whether the @renderer should use the alpha channel of the target surface
 * or not.
 *
 * Since: 3.22
 */
void
gsk_renderer_set_use_alpha (GskRenderer *renderer,
                            gboolean     use_alpha)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_if_fail (GSK_IS_RENDERER (renderer));
  g_return_if_fail (!priv->is_realized);

  use_alpha = !!use_alpha;

  if (use_alpha == priv->use_alpha)
    return;

  priv->use_alpha = use_alpha;

  g_object_notify_by_pspec (G_OBJECT (renderer), gsk_renderer_properties[PROP_USE_ALPHA]);
}

/**
 * gsk_renderer_get_use_alpha:
 * @renderer: a #GskRenderer
 *
 * Retrieves the value set using gsk_renderer_set_use_alpha().
 *
 * Returns: %TRUE if the target surface should use an alpha channel
 *
 * Since: 3.22
 */
gboolean
gsk_renderer_get_use_alpha (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), FALSE);

  return priv->use_alpha;
}

/**
 * gsk_renderer_get_for_display:
 * @display: a #GdkDisplay
 *
 * Creates an appropriate #GskRenderer instance for the given @display.
 *
 * Returns: (transfer full): a #GskRenderer
 *
 * Since: 3.22
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
    {
      renderer_type = GSK_TYPE_CAIRO_RENDERER;
      goto out;
    }

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

out:
  return g_object_new (renderer_type, "display", display, NULL);
}
