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

#include "gskdebugprivate.h"
#include "gskglrendererprivate.h"
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

typedef struct
{
  GObject parent_instance;

  graphene_rect_t viewport;
  graphene_matrix_t modelview;
  graphene_matrix_t projection;

  GskScalingFilter min_filter;
  GskScalingFilter mag_filter;

  GdkWindow *window;
  GdkDrawingContext *drawing_context;
  GskRenderNode *root_node;
  GdkDisplay *display;

  GskProfiler *profiler;

  int scale_factor;

  gboolean is_realized : 1;
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
  PROP_USE_ALPHA,
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
                          GskRenderNode *root,
                          GdkDrawingContext *context)
{
  GSK_RENDERER_WARN_NOT_IMPLEMENTED_METHOD (self, render);
}

static void
gsk_renderer_dispose (GObject *gobject)
{
  GskRenderer *self = GSK_RENDERER (gobject);
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (self);

  gsk_renderer_unrealize (self);

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

    case PROP_USE_ALPHA:
      gsk_renderer_set_use_alpha (self, g_value_get_boolean (value));
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

    case PROP_USE_ALPHA:
      g_value_set_boolean (value, priv->use_alpha);
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
   * Since: 3.22
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
   * Since: 3.22
   */
  gsk_renderer_properties[PROP_DRAWING_CONTEXT] =
    g_param_spec_object ("drawing-context",
                         "Drawing Context",
                         "The drawing context used by the renderer",
                         GDK_TYPE_DRAWING_CONTEXT,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

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

  priv->profiler = gsk_profiler_new ();

  priv->auto_clear = TRUE;
  priv->scale_factor = 1;

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

int
gsk_renderer_get_scale_factor (GskRenderer *renderer)
{
  GskRendererPrivate *priv = gsk_renderer_get_instance_private (renderer);

  g_return_val_if_fail (GSK_IS_RENDERER (renderer), 1);

  return priv->scale_factor;
}

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
 * Since: 3.22
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
 * Since: 3.22
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
 * Since: 3.22
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
  g_return_if_fail (GDK_IS_DRAWING_CONTEXT (context));
  g_return_if_fail (priv->drawing_context == NULL);
  g_return_if_fail (priv->root_node == NULL);
  g_return_if_fail (root->renderer == renderer);

  priv->drawing_context = g_object_ref (context);
  priv->root_node = gsk_render_node_ref (root);
  gsk_render_node_make_immutable (priv->root_node);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_reset (priv->profiler);
#endif

  GSK_RENDERER_GET_CLASS (renderer)->render (renderer, root, context);

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
 * gsk_renderer_create_render_node:
 * @renderer: a #GskRenderer
 *
 * Creates a new #GskRenderNode instance tied to the given @renderer.
 *
 * Returns: (transfer full): the new #GskRenderNode
 *
 * Since: 3.22
 */
GskRenderNode *
gsk_renderer_create_render_node (GskRenderer *renderer)
{
  g_return_val_if_fail (GSK_IS_RENDERER (renderer), NULL);

  return gsk_render_node_new (renderer);
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
    return NULL;

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
    return NULL;

  GSK_NOTE (RENDERER, g_print ("Creating renderer of type '%s' for display '%s'\n",
                               g_type_name (renderer_type),
                               G_OBJECT_TYPE_NAME (display)));

  g_assert (renderer_type != G_TYPE_INVALID);

  return g_object_new (renderer_type, "display", display, NULL);
}
