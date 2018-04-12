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
 * @Short_description: Drawing context for GDK surfaces
 *
 * #GdkDrawingContext is an object that represents the current drawing
 * state of a #GdkSurface.
 *
 * It's possible to use a #GdkDrawingContext to draw on a #GdkSurface
 * via rendering API like Cairo or OpenGL.
 *
 * A #GdkDrawingContext can only be created by calling gdk_surface_begin_draw_frame()
 * and will be valid until a call to gdk_surface_end_draw_frame().
 *
 * #GdkDrawingContext is available since GDK 3.22
 */

/**
 * GdkDrawingContext:
 *
 * The GdkDrawingContext struct contains only private fields and should not
 * be accessed directly.
 */

#include "config.h"

#include <cairo-gobject.h>

#include "gdkdrawingcontextprivate.h"

#include "gdkrectangle.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdksurfaceimpl.h"
#include "gdk-private.h"

typedef struct _GdkDrawingContextPrivate GdkDrawingContextPrivate;

struct _GdkDrawingContextPrivate {
  GdkSurface *surface;
  GdkDrawContext *paint_context;

  cairo_region_t *clip;
  cairo_t *cr;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkDrawingContext, gdk_drawing_context, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_SURFACE,
  PROP_CLIP,
  PROP_PAINT_CONTEXT,

  N_PROPS
};

static GParamSpec *obj_property[N_PROPS];

static void
gdk_drawing_context_dispose (GObject *gobject)
{
  GdkDrawingContext *self = GDK_DRAWING_CONTEXT (gobject);
  GdkDrawingContextPrivate *priv = gdk_drawing_context_get_instance_private (self);

  g_clear_object (&priv->surface);
  g_clear_object (&priv->paint_context);
  g_clear_pointer (&priv->clip, cairo_region_destroy);
  g_clear_pointer (&priv->cr, cairo_destroy);

  G_OBJECT_CLASS (gdk_drawing_context_parent_class)->dispose (gobject);
}

static void
gdk_drawing_context_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GdkDrawingContext *self = GDK_DRAWING_CONTEXT (gobject);
  GdkDrawingContextPrivate *priv = gdk_drawing_context_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SURFACE:
      priv->surface = g_value_dup_object (value);
      if (priv->surface == NULL)
        {
          g_critical ("The drawing context of type %s does not have a surface "
                      "associated to it. Drawing contexts can only be created "
                      "using gdk_surface_begin_draw_frame().",
                      G_OBJECT_TYPE_NAME (gobject));
          return;
        }
      break;

    case PROP_PAINT_CONTEXT:
      priv->paint_context = g_value_dup_object (value);
      break;

    case PROP_CLIP:
      priv->clip = g_value_dup_boxed (value);
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
  GdkDrawingContextPrivate *priv = gdk_drawing_context_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SURFACE:
      g_value_set_object (value, priv->surface);
      break;

    case PROP_CLIP:
      g_value_set_boxed (value, priv->clip);
      break;

    case PROP_PAINT_CONTEXT:
      g_value_set_object (value, priv->paint_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gdk_drawing_context_class_init (GdkDrawingContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdk_drawing_context_set_property;
  gobject_class->get_property = gdk_drawing_context_get_property;
  gobject_class->dispose = gdk_drawing_context_dispose;

  /**
   * GdkDrawingContext:surface:
   *
   * The #GdkSurface that created the drawing context.
   */
  obj_property[PROP_SURFACE] =
    g_param_spec_object ("surface", "Surface", "The surface that created the context",
                         GDK_TYPE_SURFACE,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);
  /**
   * GdkDrawingContext:clip:
   *
   * The clip region applied to the drawing context.
   */
  obj_property[PROP_CLIP] =
    g_param_spec_boxed ("clip", "Clip", "The clip region of the context",
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);
  /**
   * GdkDrawingContext:paint-context:
   *
   * The #GdkDrawContext used to draw or %NULL if Cairo is used.
   */
  obj_property[PROP_PAINT_CONTEXT] =
    g_param_spec_object ("paint-context", "Paint context", "The context used to draw",
                         GDK_TYPE_DRAW_CONTEXT,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_property);
}

static void
gdk_drawing_context_init (GdkDrawingContext *self)
{
}

/**
 * gdk_drawing_context_get_paint_context:
 * @context: a #GdkDrawingContext
 *
 * Retrieves the paint context used to draw with.
 *
 * Returns: (transfer none): a #GdkDrawContext or %NULL
 */
GdkDrawContext *
gdk_drawing_context_get_paint_context (GdkDrawingContext *context)
{
  GdkDrawingContextPrivate *priv = gdk_drawing_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAWING_CONTEXT (context), NULL);

  return priv->paint_context;
}

/**
 * gdk_drawing_context_get_clip:
 * @context: a #GdkDrawingContext
 *
 * Retrieves a copy of the clip region used when creating the @context.
 *
 * Returns: (transfer full) (nullable): a Cairo region
 */
cairo_region_t *
gdk_drawing_context_get_clip (GdkDrawingContext *context)
{
  GdkDrawingContextPrivate *priv = gdk_drawing_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAWING_CONTEXT (context), NULL);

  if (priv->clip == NULL)
    return NULL;

  return cairo_region_copy (priv->clip);
}

