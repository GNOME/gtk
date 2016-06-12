/* GDK - The GIMP Drawing Kit
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
 * SECTION:gdkdrawingcontext
 * @Title: GdkDrawingContext
 * @Short_description: Drawing context for GDK windows
 *
 * #GdkDrawingContext is an object that represents the current drawing
 * state of a #GdkWindow.
 *
 * It's possible to use a #GdkDrawingContext to draw on a #GdkWindow
 * via rendering API like Cairo or OpenGL.
 *
 * A #GdkDrawingContext can only be created by calling gdk_window_begin_draw_frame()
 * and will be valid until a call to gdk_window_end_draw_frame().
 *
 * #GdkDrawingContext is available since GDK 3.22
 */

#include "config.h"

#include <cairo-gobject.h>

#include "gdkdrawingcontextprivate.h"

#include "gdkrectangle.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkframeclockidle.h"
#include "gdkwindowimpl.h"
#include "gdkglcontextprivate.h"
#include "gdk-private.h"

G_DEFINE_TYPE (GdkDrawingContext, gdk_drawing_context, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_WINDOW,
  PROP_CLIP,

  N_PROPS
};

static GParamSpec *obj_property[N_PROPS];

static void
gdk_drawing_context_dispose (GObject *gobject)
{
  GdkDrawingContext *self = GDK_DRAWING_CONTEXT (gobject);

  /* Unset the drawing context, in case somebody is holding
   * onto the Cairo context
   */
  if (self->cr != NULL)
    gdk_cairo_set_drawing_context (self->cr, NULL);

  g_clear_object (&self->window);
  g_clear_pointer (&self->clip, cairo_region_destroy);
  g_clear_pointer (&self->cr, cairo_destroy);

  G_OBJECT_CLASS (gdk_drawing_context_parent_class)->dispose (gobject);
}

static void
gdk_drawing_context_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GdkDrawingContext *self = GDK_DRAWING_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_WINDOW:
      self->window = g_value_dup_object (value);
      break;

    case PROP_CLIP:
      self->clip = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_drawing_context_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GdkDrawingContext *self = GDK_DRAWING_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_value_set_object (value, self->window);
      break;

    case PROP_CLIP:
      g_value_set_boxed (value, self->clip);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_drawing_context_constructed (GObject *gobject)
{
  GdkDrawingContext *self = GDK_DRAWING_CONTEXT (gobject);

  if (self->window == NULL)
    {
      g_critical ("The drawing context of type %s does not have a window "
                  "associated to it. Drawing contexts can only be created "
                  "using gdk_window_begin_draw_frame().",
                  G_OBJECT_TYPE_NAME (gobject));
    }

  G_OBJECT_CLASS (gdk_drawing_context_parent_class)->constructed (gobject);
}

static void
gdk_drawing_context_class_init (GdkDrawingContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gdk_drawing_context_constructed;
  gobject_class->set_property = gdk_drawing_context_set_property;
  gobject_class->get_property = gdk_drawing_context_get_property;
  gobject_class->dispose = gdk_drawing_context_dispose;

  /**
   * GdkDrawingContext:window:
   *
   * The #GdkWindow that created the drawing context.
   *
   * Since: 3.22
   */
  obj_property[PROP_WINDOW] =
    g_param_spec_object ("window", "Window", "The window that created the context",
                         GDK_TYPE_WINDOW,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);
  /**
   * GdkDrawingContext:clip:
   *
   * The clip region applied to the drawing context.
   *
   * Since: 3.22
   */
  obj_property[PROP_CLIP] =
    g_param_spec_boxed ("clip", "Clip", "The clip region of the context",
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_property);
}

static void
gdk_drawing_context_init (GdkDrawingContext *self)
{
}

static const cairo_user_data_key_t draw_context_key;

void
gdk_cairo_set_drawing_context (cairo_t           *cr,
                               GdkDrawingContext *context)
{
  cairo_set_user_data (cr, &draw_context_key, context, NULL);
}

/**
 * gdk_cairo_get_drawing_context:
 * @cr: a Cairo context
 *
 * Retrieves the #GdkDrawingContext that created the Cairo
 * context @cr.
 *
 * Returns: (transfer none) (nullable): a #GdkDrawingContext, if any is set
 *
 * Since: 3.22
 */
GdkDrawingContext *
gdk_cairo_get_drawing_context (cairo_t *cr)
{
  g_return_val_if_fail (cr != NULL, NULL);

  return cairo_get_user_data (cr, &draw_context_key);
}

/**
 * gdk_drawing_context_get_cairo_context:
 * @context:
 *
 * Retrieves a Cairo context to be used to draw on the #GdkWindow
 * that created the #GdkDrawingContext.
 *
 * The returned context is guaranteed to be valid as long as the
 * #GdkDrawingContext is valid, that is between a call to
 * gdk_window_begin_draw_frame() and gdk_window_end_draw_frame().
 *
 * Returns: (transfer none): a Cairo context to be used to draw
 *   the contents of the #GdkWindow. The context is owned by the
 *   #GdkDrawingContext and should not be destroyed
 *
 * Since: 3.22
 */
cairo_t *
gdk_drawing_context_get_cairo_context (GdkDrawingContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAWING_CONTEXT (context), NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (context->window), NULL);

  if (context->cr == NULL)
    {
      cairo_region_t *region;
      cairo_surface_t *surface;

      surface = _gdk_window_ref_cairo_surface (context->window);
      context->cr = cairo_create (surface);

      gdk_cairo_set_drawing_context (context->cr, context);

      region = gdk_window_get_current_paint_region (context->window);
      cairo_region_union (region, context->clip);
      gdk_cairo_region (context->cr, region);
      cairo_clip (context->cr);

      cairo_region_destroy (region);
      cairo_surface_destroy (surface);
    }

  return context->cr;
}

/**
 * gdk_drawing_context_get_window:
 * @context: a #GdkDrawingContext
 *
 * Retrieves the window that created the drawing @context.
 *
 * Returns: (transfer none): a #GdkWindow
 *
 * Since: 3.22
 */
GdkWindow *
gdk_drawing_context_get_window (GdkDrawingContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAWING_CONTEXT (context), NULL);

  return context->window;
}

/**
 * gdk_drawing_context_get_clip:
 * @context: a #GdkDrawingContext
 *
 * Retrieves a copy of the clip region used when creating the @context.
 *
 * Returns: (transfer full) (nullable): a Cairo region
 *
 * Since: 3.22
 */
cairo_region_t *
gdk_drawing_context_get_clip (GdkDrawingContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAWING_CONTEXT (context), NULL);

  if (context->clip == NULL)
    return NULL;

  return cairo_region_copy (context->clip);
}

/**
 * gdk_drawing_context_is_valid:
 * @context: a #GdkDrawingContext
 *
 * Checks whether the given #GdkDrawingContext is valid.
 *
 * Returns: %TRUE if the context is valid
 *
 * Since: 3.22
 */
gboolean
gdk_drawing_context_is_valid (GdkDrawingContext *context)
{
  g_return_val_if_fail (GDK_IS_DRAWING_CONTEXT (context), FALSE);

  if (context->window == NULL)
    return FALSE;

  if (gdk_window_get_drawing_context (context->window) != context)
    return FALSE;

  return TRUE;
}
