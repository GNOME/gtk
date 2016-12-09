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

#include "gdkinternals.h"
#include "gdkintl.h"

/**
 * SECTION:gdkdrawcontext
 * @Title: GdkDrawContext
 * @Short_description: Drawing context base class
 *
 * #GdkDrawContext is the base object used by contexts implementing different
 * rendering methods, such as #GdkGLContext or #GdkVulkanContext. It provides
 * shared functionality between those contexts.
 *
 * You will always interact with one of those subclasses.
 */
typedef struct _GdkDrawContextPrivate GdkDrawContextPrivate;

struct _GdkDrawContextPrivate {
  GdkWindow *window;

  guint is_drawing : 1;
};

enum {
  PROP_0,

  PROP_DISPLAY,
  PROP_WINDOW,

  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkDrawContext, gdk_draw_context, G_TYPE_OBJECT)

static void
gdk_draw_context_dispose (GObject *gobject)
{
  GdkDrawContext *context = GDK_DRAW_CONTEXT (gobject);
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_clear_object (&priv->window);

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
    case PROP_WINDOW:
      priv->window = g_value_dup_object (value);
      g_assert (priv->window != NULL);
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

    case PROP_WINDOW:
      g_value_set_object (value, priv->window);
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

  /**
   * GdkDrawContext:display:
   *
   * The #GdkDisplay used to create the #GdkDrawContext.
   *
   * Since: 3.90
   */
  pspecs[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         P_("Display"),
                         P_("The GDK display used to create the context"),
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GdkDrawContext:window:
   *
   * The #GdkWindow the gl context is bound to.
   *
   * Since: 3.90
   */
  pspecs[PROP_WINDOW] =
    g_param_spec_object ("window",
                         P_("Window"),
                         P_("The GDK window bound to the context"),
                         GDK_TYPE_WINDOW,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, pspecs);
}

static void
gdk_draw_context_init (GdkDrawContext *self)
{
}

/*< private >
 * gdk_draw_context_is_drawing:
 * @context: a #GdkDrawContext
 *
 * Returns %TRUE if @context is in the process of drawing to its window. In such
 * cases, it will have access to the window's backbuffer to render the new frame
 * onto it.
 *
 * Returns: %TRUE if the context is between begin_frame() and end_frame() calls.
 *
 * Since: 3.90
 */
gboolean
gdk_draw_context_is_drawing (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  return priv->is_drawing;
}

/*< private >
 * gdk_draw_context_begin_frame:
 * @context: a #GdkDrawContext
 * @region: (inout): The clip region that needs to be repainted
 *
 * Sets up @context and @drawing for a new drawing.
 *
 * The @context is free to update @region to the size that actually needs to
 * be repainted. Contexts that do not support partial blits for example may
 * want to invalidate the whole window instead.
 *
 * The function does not clear the background. Clearing the backgroud is the
 * job of the renderer. The contents of the backbuffer are undefined after this
 * function call.
 *
 * Since: 3.90
 */
void
gdk_draw_context_begin_frame (GdkDrawContext *context,
                              cairo_region_t *region)
{
  GdkDrawContextPrivate *priv;

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));
  g_return_if_fail (region != NULL);

  priv = gdk_draw_context_get_instance_private (context);
  priv->is_drawing = TRUE;

  GDK_DRAW_CONTEXT_GET_CLASS (context)->begin_frame (context, region);
}

/*< private >
 * gdk_draw_context_end_frame:
 * @context: a #GdkDrawContext
 * @painted: The area that has been redrawn this frame
 * @damage: The area that we know is actually different from the last frame
 *
 * Copies the back buffer to the front buffer.
 *
 * This function may call `glFlush()` implicitly before returning; it
 * is not recommended to call `glFlush()` explicitly before calling
 * this function.
 *
 * Since: 3.16
 */
void
gdk_draw_context_end_frame (GdkDrawContext *context,
                            cairo_region_t *painted,
                            cairo_region_t *damage)
{
  GdkDrawContextPrivate *priv;

  g_return_if_fail (GDK_IS_DRAW_CONTEXT (context));

  GDK_DRAW_CONTEXT_GET_CLASS (context)->end_frame (context, painted, damage);

  priv = gdk_draw_context_get_instance_private (context);
  priv->is_drawing = FALSE;
}

/**
 * gdk_draw_context_get_display:
 * @context: a #GdkDrawContext
 *
 * Retrieves the #GdkDisplay the @context is created for
 *
 * Returns: (nullable) (transfer none): a #GdkDisplay or %NULL
 *
 * Since: 3.90
 */
GdkDisplay *
gdk_draw_context_get_display (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->window ? gdk_window_get_display (priv->window) : NULL;
}

/**
 * gdk_draw_context_get_window:
 * @context: a #GdkDrawContext
 *
 * Retrieves the #GdkWindow used by the @context.
 *
 * Returns: (nullable) (transfer none): a #GdkWindow or %NULL
 *
 * Since: 3.90
 */
GdkWindow *
gdk_draw_context_get_window (GdkDrawContext *context)
{
  GdkDrawContextPrivate *priv = gdk_draw_context_get_instance_private (context);

  g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (context), NULL);

  return priv->window;
}

