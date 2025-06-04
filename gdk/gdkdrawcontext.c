/* GDK - The GIMP Drawing Kit
 *
 * gdkdrawcontext.c: base class for rendering system support
 *
 * Copyright © 2016  Benjamin Otte
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

#include "config.h"

#include "gdkdrawcontextprivate.h"

#include "gdkcairoprivate.h"
#include "gdkdebugprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprofilerprivate.h"
#include "gdksurfaceprivate.h"

/**
 * GdkDrawContext:
 *
 * Base class for objects implementing different rendering methods.
 *
 * `GdkDrawContext` is the base object used by contexts implementing different
 * rendering methods, such as [class@Gdk.CairoContext] or [class@Gdk.GLContext].
 * It provides shared functionality between those contexts.
 *
 * You will always interact with one of those subclasses.
 *
 * A `GdkDrawContext` is always associated with a single toplevel surface.
 */

typedef struct _GdkDrawContextPrivate GdkDrawContextPrivate;

struct _GdkDrawContextPrivate {
  GdkDisplay *display;
  GdkSurface *surface;

  cairo_region_t *render_region;
  GdkColorState *color_state;
  GdkMemoryDepth depth;
};

enum {
  PROP_0,

  PROP_DISPLAY,
  PROP_SURFACE,

  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkDrawContext, gdk_draw_context, G_TYPE_OBJECT)

static gboolean
gdk_draw_context_is_attached (GdkDrawContext *self)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  if (priv->surface == NULL)
    return FALSE;

  return gdk_surface_get_attached_context (priv->surface) == self;
}

static void
gdk_draw_context_default_surface_resized (GdkDrawContext *context)
{
}

static gboolean
gdk_draw_context_default_surface_attach (GdkDrawContext  *context,
                                         GError         **error)
{
  return TRUE;
}

static void
gdk_draw_context_default_surface_detach (GdkDrawContext *context)
{
}

static void
gdk_draw_context_default_empty_frame (GdkDrawContext *context)
{
  g_warning ("FIXME: Implement GdkDrawContext.empty_frame in %s", G_OBJECT_TYPE_NAME (context));
}

static void
gdk_draw_context_dispose (GObject *gobject)
{
  GdkDrawContext *self = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  if (priv->surface)
    {
      if (gdk_draw_context_is_attached (self))
        {
          g_warning ("%s %p is still attached for rendering on disposal, detaching it.",
                     G_OBJECT_TYPE_NAME (self), self);
          gdk_draw_context_detach (self);
        }
      g_clear_object (&priv->surface);
    }
  g_clear_object (&priv->display);

  G_OBJECT_CLASS (gdk_draw_context_parent_class)->dispose (gobject);
}

static void
gdk_draw_context_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      if (priv->display != NULL)
        {
          g_assert (g_value_get_object (value) == NULL);
        }
      else
        {
          priv->display = g_value_dup_object (value);
        }
      break;

    case PROP_SURFACE:
      priv->surface = g_value_dup_object (value);
      if (priv->surface)
        {
          if (priv->display)
            {
              g_assert (priv->display == gdk_surface_get_display (priv->surface));
            }
          else
            {
              priv->display = g_object_ref (gdk_surface_get_display (priv->surface));
            }
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_draw_context_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, gdk_draw_context_get_display (context));
      break;

    case PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_draw_context_class_init (GdkDrawContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdk_draw_context_set_property;
  gobject_class->get_property = gdk_draw_context_get_property;
  gobject_class->dispose = gdk_draw_context_dispose;

  klass->surface_resized = gdk_draw_context_default_surface_resized;
  klass->surface_attach = gdk_draw_context_default_surface_attach;
  klass->surface_detach = gdk_draw_context_default_surface_detach;
  klass->empty_frame = gdk_draw_context_default_empty_frame;

  /**
   * GdkDrawContext:display:
   *
   * The `GdkDisplay` used to create the `GdkDrawContext`.
   */
  pspecs[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkDrawContext:surface:
   *
   * The `GdkSurface` the context is bound to.
   */
  pspecs[PROP_SURFACE] =
    g_param_spec_object ("surface", NULL, NULL,
                         GDK_TYPE_SURFACE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, pspecs);
}

static guint pixels_counter;

static void
gdk_draw_context_init (GdkDrawContext *self)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  if (pixels_counter == 0)
    pixels_counter = gdk_profiler_define_int_counter ("frame pixels", "Pixels drawn per frame");

  priv->depth = GDK_N_DEPTHS;
}

/**
 * gdk_draw_context_is_in_frame:
 * @context: a `GdkDrawContext`
 *
 * Returns %TRUE if @context is in the process of drawing to its surface.
 *
 * This is the case between calls to [method@Gdk.DrawContext.begin_frame]
 * and [method@Gdk.DrawContext.end_frame]. In this situation, drawing commands
 * may be effecting the contents of the @context's surface.
 *
 * Returns: %TRUE if the context is between [method@Gdk.DrawContext.begin_frame]
 *   and [method@Gdk.DrawContext.end_frame] calls.
 *
 * Deprecated: 4.16: Drawing directly to the surface is no longer recommended.
 *   Use `GskRenderNode` and `GskRenderer`.
 */
gboolean
gdk_draw_context_is_in_frame (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), FALSE);

  return priv->render_region != NULL;
}

/*< private >
 * gdk_draw_context_surface_resized:
 * @context: a `GdkDrawContext`
 *
 * Called by the surface the @context belongs to when the size of the surface
 * changes.
 */
void
gdk_draw_context_surface_resized (GdkDrawContext *context)
{
  GDK_DRAW_CONTEXT_GET_CLASS (context)->surface_resized (context);
}

/**
 * gdk_draw_context_get_display:
 * @context: a `GdkDrawContext`
 *
 * Retrieves the `GdkDisplay` the @context is created for
 *
 * Returns: (nullable) (transfer none): the `GdkDisplay`
 */
GdkDisplay *
gdk_draw_context_get_display (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->display;
}

/**
 * gdk_draw_context_get_surface:
 * @context: a `GdkDrawContext`
 *
 * Retrieves the surface that @context is bound to.
 *
 * Returns: (nullable) (transfer none): a `GdkSurface`
 */
GdkSurface *
gdk_draw_context_get_surface (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->surface;
}

/**
 * gdk_draw_context_begin_frame:
 * @context: the `GdkDrawContext` used to draw the frame. The context must
 *   have a surface.
 * @region: minimum region that should be drawn
 *
 * Indicates that you are beginning the process of redrawing @region
 * on the @context's surface.
 *
 * Calling this function begins a drawing operation using @context on the
 * surface that @context was created from. The actual requirements and
 * guarantees for the drawing operation vary for different implementations
 * of drawing, so a [class@Gdk.CairoContext] and a [class@Gdk.GLContext]
 * need to be treated differently.
 *
 * A call to this function is a requirement for drawing and must be
 * followed by a call to [method@Gdk.DrawContext.end_frame], which will
 * complete the drawing operation and ensure the contents become visible
 * on screen.
 *
 * Note that the @region passed to this function is the minimum region that
 * needs to be drawn and depending on implementation, windowing system and
 * hardware in use, it might be necessary to draw a larger region. Drawing
 * implementation must use [method@Gdk.DrawContext.get_frame_region] to
 * query the region that must be drawn.
 *
 * When using GTK, the widget system automatically places calls to
 * gdk_draw_context_begin_frame() and gdk_draw_context_end_frame() via the
 * use of [GskRenderer](../gsk4/class.Renderer.html)s, so application code
 * does not need to call these functions explicitly.
 *
 * Deprecated: 4.16: Drawing directly to the surface is no longer recommended.
 *   Use `GskRenderNode` and `GskRenderer`.
 */
void
gdk_draw_context_begin_frame (GdkDrawContext       *context,
                              const cairo_region_t *region)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (priv->surface != NULL);
  g_return_if_fail (region != NULL);

  gdk_draw_context_begin_frame_full (context, NULL, GDK_MEMORY_U8, region, NULL);
}

/*
 * @depth: best depth to render in
 * @opaque: (nullable): opaque region of the rendering
 *
 * If the given depth is not `GDK_MEMORY_U8`, GDK will see about providing a
 * rendering target that supports a higher bit depth than 8 bits per channel.
 * Typically this means a target supporting 16bit floating point pixels, but
 * that is not guaranteed.
 *
 * This is only a request and if the GDK backend does not support HDR rendering
 * or does not consider it worthwhile, it may choose to not honor the request.
 * It may also choose to provide a different depth even if it was not requested.
 * Typically the steps undertaken by a backend are:
 * 1. Check if high depth is supported by this drawing backend.
 * 2. Check if the compositor supports high depth.
 * 3. Check if the compositor prefers regular bit depth. This is usually the case
 *    when the attached monitors do not support high depth content or when the
 *    system is resource constrained.
 * In either of those cases, the context will usually choose to not honor the request.
 *
 * The rendering code must be able to deal with content in any bit depth, no matter
 * the preference. The depth argument is only a hint and GDK is free
 * to choose.
 */
void
gdk_draw_context_begin_frame_full (GdkDrawContext        *context,
                                   gpointer               context_data,
                                   GdkMemoryDepth         depth,
                                   const cairo_region_t  *region,
                                   const graphene_rect_t *opaque)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);
  double scale;
  guint buffer_width, buffer_height;

  if (GDK_SURFACE_DESTROYED (priv->surface))
    return;

  if (!gdk_draw_context_is_attached (context))
    {
      GdkDrawContext *prev = gdk_surface_get_attached_context (priv->surface);
      GError *error = NULL;

      /* This should be a return_if_fail() but we handle it somewhat gracefully
       * for backwards compat */

      if (prev)
        {
          g_warning ("%s %p is already rendered to by %s %p. "
                     "Replacing it to render with %s %p now.",
                     G_OBJECT_TYPE_NAME (priv->surface), priv->surface,
                     G_OBJECT_TYPE_NAME (prev), prev,
                     G_OBJECT_TYPE_NAME (context), context);
          gdk_draw_context_detach (prev);
        }
      else
        {
          g_warning ("%s %p has not been set up for rendering. "
                     "Attaching %s %p for rendering now.",
                     G_OBJECT_TYPE_NAME (priv->surface), priv->surface,
                     G_OBJECT_TYPE_NAME (context), context);
        }
      if (!gdk_draw_context_attach (context, &error))
        {
          g_critical ("Failed to attach context: %s", error->message);
          return;
        }
    }

  if (priv->surface->paint_context != NULL)
    {
      if (priv->surface->paint_context == context)
        {
          g_critical ("The surface %p is already drawing. You must finish the "
                      "previous drawing operation with gdk_draw_context_end_frame() first.",
                      priv->surface);
        }
      else
        {
          g_critical ("The surface %p is already being drawn by %s %p. "
                      "You cannot draw a surface with multiple contexts at the same time.",
                      priv->surface,
                      G_OBJECT_TYPE_NAME (priv->surface->paint_context), priv->surface->paint_context);
        }
      return;
    }

  gdk_surface_set_opaque_rect (priv->surface, opaque);

  if (gdk_display_get_debug_flags (priv->display) & GDK_DEBUG_HIGH_DEPTH)
    depth = GDK_MEMORY_FLOAT32;

  scale = gdk_surface_get_scale (priv->surface);
  priv->render_region = gdk_cairo_region_scale_grow (region, scale, scale);
  priv->surface->paint_context = g_object_ref (context);

  g_assert (priv->color_state == NULL);

  GDK_DRAW_CONTEXT_GET_CLASS (context)->begin_frame (context,
                                                     context_data,
                                                     depth,
                                                     priv->render_region,
                                                     &priv->color_state,
                                                     &priv->depth);

  /* The callback is meant to set them. Note that it does not return a ref */
  g_assert (priv->color_state != NULL);
  g_assert (priv->depth < GDK_N_DEPTHS);

  gdk_draw_context_get_buffer_size (context, &buffer_width, &buffer_height);
  cairo_region_intersect_rectangle (priv->render_region,
                                    &(cairo_rectangle_int_t) {
                                      0, 0,
                                      buffer_width, buffer_height
                                    });
}

#ifdef HAVE_SYSPROF
static gint64
region_get_pixels (cairo_region_t *region)
{
  int i, n;
  cairo_rectangle_int_t rect;
  gint64 pixels = 0;

  n = cairo_region_num_rectangles (region);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);
      pixels += rect.width * rect.height;
    }

  return pixels;
}
#endif

void
gdk_draw_context_end_frame_full (GdkDrawContext *context,
                                 gpointer        context_data)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  GDK_DRAW_CONTEXT_GET_CLASS (context)->end_frame (context, context_data, priv->render_region);

  gdk_profiler_set_int_counter (pixels_counter, region_get_pixels (priv->render_region));

  priv->color_state = NULL;
  g_clear_pointer (&priv->render_region, cairo_region_destroy);
  g_clear_object (&priv->surface->paint_context);
  priv->depth = GDK_N_DEPTHS;
}

/**
 * gdk_draw_context_end_frame:
 * @context: a `GdkDrawContext`
 *
 * Ends a drawing operation started with gdk_draw_context_begin_frame().
 *
 * This makes the drawing available on screen.
 * See [method@Gdk.DrawContext.begin_frame] for more details about drawing.
 *
 * When using a [class@Gdk.GLContext], this function may call `glFlush()`
 * implicitly before returning; it is not recommended to call `glFlush()`
 * explicitly before calling this function.
 *
 * Deprecated: 4.16: Drawing directly to the surface is no longer recommended.
 *   Use `GskRenderNode` and `GskRenderer`.
 */
void
gdk_draw_context_end_frame (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (priv->surface != NULL);

  if (GDK_SURFACE_DESTROYED (priv->surface))
    return;

  if (priv->surface->paint_context == NULL)
    {
      g_critical ("The surface %p has no drawing context. You must call "
                  "gdk_draw_context_begin_frame() before calling "
                  "gdk_draw_context_end_frame().", priv->surface);
      return;
    }
  else if (priv->surface->paint_context != context)
    {
      g_critical ("The surface %p is not drawn by this context but by %s %p.",
                  priv->surface, 
                  G_OBJECT_TYPE_NAME (priv->surface->paint_context), priv->surface->paint_context);
      return;
    }

  gdk_draw_context_end_frame_full (context, NULL);
}

/**
 * gdk_draw_context_get_frame_region:
 * @context: a `GdkDrawContext`
 *
 * Retrieves the region that is currently being repainted.
 *
 * After a call to [method@Gdk.DrawContext.begin_frame] this function will
 * return a union of the region passed to that function and the area of the
 * surface that the @context determined needs to be repainted.
 *
 * If @context is not in between calls to [method@Gdk.DrawContext.begin_frame]
 * and [method@Gdk.DrawContext.end_frame], %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): a Cairo region
 *
 * Deprecated: 4.16: Drawing directly to the surface is no longer recommended.
 *   Use `GskRenderNode` and `GskRenderer`.
 */
const cairo_region_t *
gdk_draw_context_get_frame_region (GdkDrawContext *self)
{
  return NULL;
}

/*<private>
 * gdk_draw_context_get_render_region:
 * @self: a `GdkDrawContext`
 *
 * Retrieves the region that is currently being repainted.
 *
 * After a call to [method@Gdk.DrawContext.begin_frame] this function will
 * return the area of the current buffer that the @context determined needs
 * to be repainted.
 * This region is created by a union of the region passed to
 * [method@Gdk.DrawContext.begin_frame] converted to device pixels and any
 * additional pixels the context has determined need to be repainted.
 *
 * The region will never extend the buffer's size.
 *
 * If @context is not in between calls to [method@Gdk.DrawContext.begin_frame]
 * and [method@Gdk.DrawContext.end_frame], %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): a Cairo region
 *
 * Returns:
 **/
const cairo_region_t *
gdk_draw_context_get_render_region (GdkDrawContext *self)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  return priv->render_region;
}

/*<private>
 * gdk_draw_context_get_color_state:
 * @self: a `GdkDrawContext`
 *
 * Gets the target color state while rendering. If no rendering is going on, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): the target color state
 **/
GdkColorState *
gdk_draw_context_get_color_state (GdkDrawContext *self)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  return priv->color_state;
}

/*<private>
 * gdk_draw_context_get_depth:
 * @self: a `GdkDrawContext`
 *
 * Gets the target depth while rendering. If no rendering is going on, the return value is undefined.
 *
 * Returns: the target depth
 **/
GdkMemoryDepth
gdk_draw_context_get_depth (GdkDrawContext *self)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  return priv->depth;
}

void
gdk_draw_context_empty_frame (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (priv->surface != NULL);

  if (GDK_SURFACE_DESTROYED (priv->surface))
    return;

  GDK_DRAW_CONTEXT_GET_CLASS (context)->empty_frame (context);
}

/*<private>
 * gdk_draw_context_get_buffer_size:
 * @self: the draw context
 * @out_width: (out) the width of the buffer in pixels
 * @out_height: (out) the height of the buffer in pixels
 *
 * Queries the size that is used (for contexts where the system creates the
 * buffer) or should be used (for contexts where the buffer is created by GDK)
 * for the buffer.
 *
 * This function must only be called on draw context with a
 * surface.
 *
 * Implementation detail:
 * The vfunc for this function is part of GdkSurface as most
 * backends share the size implementation across different contexts.
 **/
void
gdk_draw_context_get_buffer_size (GdkDrawContext *self,
                                  guint          *out_width,
                                  guint          *out_height)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (self));
  g_return_if_fail (priv->surface != NULL);

  GDK_SURFACE_GET_CLASS (priv->surface)->get_buffer_size (priv->surface,
                                                          self,
                                                          out_width, out_height);
}

/*<private>
 * gdk_draw_context_attach:
 * @self: the context 
 * @error: Return location for an error
 *
 * Makes the context the one used for drawing to its surface.
 * Surface must not have an attached context.
 *
 * gdk_draw_context_detach() must be called to undo this operation.
 * Implementations can rely on that.
 *
 * This function is intended to set up window rendering resources like swapchains.
 *
 * Only one context can be used for drawing to a surface at any given
 * point in time.
 *
 * Returns: TRUE if attaching was successful
 **/
gboolean
gdk_draw_context_attach (GdkDrawContext  *self,
                         GError         **error)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  g_return_val_if_fail (priv->surface != NULL, FALSE);
  g_return_val_if_fail (gdk_surface_get_attached_context (priv->surface) == NULL, FALSE);

  if (!GDK_DRAW_CONTEXT_GET_CLASS (self)->surface_attach (self, error))
    return FALSE;

  gdk_surface_set_attached_context (priv->surface, self);
  return TRUE;
}

/*<private>
 * gdk_draw_context_detach:
 * @self: the context
 *
 * Undoes a previous successful call to gdk_draw_context_attach().
 *
 * If the draw context is not attached, this function does nothing.
 **/
void
gdk_draw_context_detach (GdkDrawContext *self)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (self);

  if (!gdk_draw_context_is_attached (self))
    return;

  GDK_DRAW_CONTEXT_GET_CLASS (self)->surface_detach (self);
  gdk_surface_set_attached_context (priv->surface, NULL);
}
