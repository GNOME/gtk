/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkpaintable.h"

/**
 * SECTION:paintable
 * @Title: GdkPaintable
 * @Short_description: An interface for a paintable region
 * @See_also: #ClutterContent, #GtkImage, #GdkTexture, #GtkSnapshot
 *
 * #GdkPaintable is a simple interface used by GDK and GDK to represent
 * objects that can be painted anywhere at any size without requiring any
 * sort of layout. The interface is inspired by similar concepts elsewhere,
 * such as #ClutterContent, <ulink
 * linkend="https://www.w3.org/TR/css-images-4/#paint-source">HTML/CSS Paint
 * Sources</ulink> or <ulink linkend="https://www.w3.org/TR/SVG2/pservers.html">
 * SVG Paint Servers</ulink>.
 *
 * A #GdkPaintable can be snapshot at any time and size using
 * gdk_paintable_snapshot(). How the paintable interprets that size and if it
 * scales or centers itself into the given rectangle is implementation defined,
 * though if you are implementing a #GdkPaintable and don't know what to do, it
 * is suggested that you scale your paintable ignoring any potential aspect ratio.
 *
 * The contents that a #GdkPaintable produces may depend on the #GdkSnapshot passed
 * to it. For example, paintables may decide to use more detailed images on higher
 * resolution screens or when OpenGL is available. A #GdkPaintable will however
 * always produce the same output for the same snapshot.
 *
 * A #GdkPaintable may change its contents, meaning that it will now produce a
 * different output with the same snpashot. Once that happens, it will call
 * gdk_paintable_invalidate_contents() which will emit the
 * #GdkPaintable::invalidate-contents signal.  
 * If a paintable is known to never change its contents, it will set the
 * %GDK_PAINTABLE_STATIC_CONTENT flag. If a consumer cannot deal with changing
 * contents, it may call gdk_paintable_get_static_image() which will return a
 * static paintable and use that.
 *
 * A paintable can report an intrinsic (or preferred) size or aspect ratio it
 * wishes to be rendered at, though it doesn't have to. Consumers of the interface
 * can use this information to layout thepaintable appropriately.
 * Just like the contents, the size of a paintable can change. A paintable will
 * indicate this by calling gdk_paintable_invalidate_size() which will emit the
 * #GdkPaintable::invalidate-size signal.
 * And just like for contents, if a paintable is known to never change its size,
 * it will set the %GDK_PAINTABLE_STATIC_SIZE flag.
 */

G_DEFINE_INTERFACE (GdkPaintable, gdk_paintable, G_TYPE_OBJECT)

enum {
  INVALIDATE_CONTENTS,
  INVALIDATE_SIZE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
gdk_paintable_default_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  g_critical ("Paintable of type '%s' does not implement GdkPaintable::snapshot", G_OBJECT_TYPE_NAME (paintable));
}

static GdkPaintable *
gdk_paintable_default_get_current_image (GdkPaintable *paintable)
{
  g_warning ("FIXME: implement by snapshotting at default size and returning a GskRendererNodePaintable");

  return paintable;
}

static GdkPaintableFlags
gdk_paintable_default_get_flags (GdkPaintable *paintable)
{
  return 0;
}

static int
gdk_paintable_default_get_intrinsic_width (GdkPaintable *paintable)
{
  return 0;
}

static int
gdk_paintable_default_get_intrinsic_height (GdkPaintable *paintable)
{
  return 0;
}

static double gdk_paintable_default_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  return 0.0;
};

static void
gdk_paintable_default_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gdk_paintable_default_snapshot;
  iface->get_current_image = gdk_paintable_default_get_current_image;
  iface->get_flags = gdk_paintable_default_get_flags;
  iface->get_intrinsic_width = gdk_paintable_default_get_intrinsic_width;
  iface->get_intrinsic_height = gdk_paintable_default_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gdk_paintable_default_get_intrinsic_aspect_ratio;

  /**
   * GdkPaintable::invalidate-contents
   * @paintable: a #GdkPaintable
   *
   * Emitted when the contents of the @paintable change.
   *
   * Examples for such an event would be videos changing to the next frame or
   * the icon theme for an icon changing.
   *
   * Since: 4.0
   */
  signals[INVALIDATE_CONTENTS] =
    g_signal_new ("invalidate-contents",
                  GDK_TYPE_PAINTABLE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkPaintable::invalidate-size
   * @paintable: a #GdkPaintable
   *
   * Emitted when the intrinsic size of the @paintable changes. This means the values
   * reported by at least one of gdk_paintable_get_intrinsic_width(),
   * gdk_paintable_get_intrinsic_height() or gdk_paintable_get_intrinsic_aspect_ratio()
   * has changed.
   *
   * Examples for such an event would be a paintable displaying the contents of a toplevel
   * window being resized.
   *
   * Since: 4.0
   */
  signals[INVALIDATE_SIZE] =
    g_signal_new ("invalidate-size",
                  GDK_TYPE_PAINTABLE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/**
 * gdk_paintable_snapshot:
 * @paintable: a #GdkPaintable
 * @snapshot: a #GdkSnapshot to snapshot to
 * @width: width to snapshot in
 * @height: height to snapshot in
 *
 * Snapshots the given paintable with the given @width and @height at the
 * current (0,0) offset of the @snapshot. If @width and @height are not larger
 * than zero, this function will do nothing.
 *
 * Since: 4.0
 **/
void
gdk_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  GdkPaintableInterface *iface;

  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (snapshot != NULL);

  if (width <= 0.0 || height <= 0.0)
    return;

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  iface->snapshot (paintable, snapshot, width, height);
}

#define GDK_PAINTABLE_IMMUTABLE (GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS)
static inline gboolean
gdk_paintable_is_immutable (GdkPaintable *paintable)
{
  return (gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_IMMUTABLE) == GDK_PAINTABLE_IMMUTABLE;
}

/**
 * gdk_paintable_get_current_image:
 * @paintable: a #GdkPaintable
 *
 * Gets an immutable paintable for the current contents displayed by @paintable.
 *
 * This is useful when you want to retain the current state of an animation, for
 * example to take a screenshot of a running animation.
 *
 * If the @paintable is already immutable, it will return itself.
 *
 * Returns: (transfer full): An immutable paintable for the current
 *     contents of @paintable.
 **/
GdkPaintable *
gdk_paintable_get_current_image (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);

  if (gdk_paintable_is_immutable (paintable))
    return g_object_ref (paintable);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_current_image (paintable);
}

/**
 * gdk_paintable_get_flags:
 * @paintable: a #GdkPaintable
 *
 * Get flags for the paintable. This is oftentimes useful for optimizations.
 *
 * See #GdkPaintableFlags for the flags and what they mean.
 *
 * Returns: The #GdkPaintableFlags for this paintable.
 **/
GdkPaintableFlags
gdk_paintable_get_flags (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_flags (paintable);
}

/**
 * gdk_paintable_get_intrinsic_width:
 * @paintable: a #GdkPaintable
 *
 * Gets the preferred width the @paintable would like to be displayed at.
 * Consumers of this interface can use this to reserve enough space to draw
 * the paintable.
 *
 * This is a purely informational value and does not in any way limit the values
 * that may be passed to gdk_paintable_snapshot().
 *
 * If the @paintable does not have a preferred width, it returns 0. Negative
 * values are never returned.
 *
 * Returns: the intrinsic width of @paintable or 0 if none.
 **/
int
gdk_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_intrinsic_width (paintable);
}

/**
 * gdk_paintable_get_intrinsic_height:
 * @paintable: a #GdkPaintable
 *
 * Gets the preferred height the @paintable would like to be displayed at.
 * Consumers of this interface can use this to reserve enough space to draw
 * the paintable.
 *
 * This is a purely informational value and does not in any way limit the values
 * that may be passed to gdk_paintable_snapshot().
 *
 * If the @paintable does not have a preferred height, it returns 0. Negative
 * values are never returned.
 *
 * Returns: the intrinsic height of @paintable or 0 if none.
 **/
int
gdk_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_intrinsic_height (paintable);
}

/**
 * gdk_paintable_get_intrinsic_aspect_ratio:
 * @paintable: a #GdkPaintable
 *
 * Gets the preferred aspect ratio the @paintable would like to be displayed at.
 * The aspect ration is the width divided by the height, so a value of 0.5 means
 * that the @paintable prefers to be displayed twice as high as it is wide.
 * Consumers of this interface can use this to preserve aspect ratio when displaying
 * this paintable.
 *
 * This is a purely informational value and does not in any way limit the values
 * that may be passed to gdk_paintable_snapshot().
 *
 * Usually when a @paintable returns non-0 values from
 * gdk_paintable_get_intrinsic_width() and gdk_paintable_get_intrinsic_height()
 * the aspect ratio should conform to those values, though that is not required.
 *
 * If the @paintable does not have a preferred aspect ratio, it returns 0.0.
 * Negative values are never returned.
 *
 * Returns: the intrinsic aspect ratio of @paintable or 0.0 if none.
 **/
double
gdk_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GdkPaintableInterface *iface;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), 0);

  iface = GDK_PAINTABLE_GET_IFACE (paintable);
  return iface->get_intrinsic_aspect_ratio (paintable);
}

/**
 * gdk_paintable_invalidate_contents:
 * @paintable: a #GdkPaintable
 *
 * Called by implementations of #GdkPaintable to invalidate their contents.  
 * Unless the contents are invalidated, implementations must guarantee that
 * multiple calls to GdkPaintable::snapshot produce the same output.
 *
 * This function will emit the GdkPaintable::invalidate-contents signal.
 *
 * If a @paintable reports the %GDK_PAINTABLE_STATIC_CONTENTS flag,
 * it must not call this function.
 **/
void
gdk_paintable_invalidate_contents (GdkPaintable *paintable)
{
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_STATIC_CONTENTS);

  g_signal_emit (paintable, signals[INVALIDATE_CONTENTS], 0);
}

/**
 * gdk_paintable_invalidate_size:
 * @paintable: a #GdkPaintable
 *
 * Called by implementations of #GdkPaintable to invalidate their size.  
 * As long as the size is not invalidated, @paintable must return the same values
 * for its width, height and intrinsic height.
 *
 * This function will emit the GdkPaintable::invalidate-size signal.
 *
 * If a @paintable reports the %GDK_PAINTABLE_STATIC_SIZE flag,
 * it must not call this function.
 **/
void
gdk_paintable_invalidate_size (GdkPaintable *paintable)
{
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));
  g_return_if_fail (gdk_paintable_get_flags (paintable) & GDK_PAINTABLE_STATIC_SIZE);

  g_signal_emit (paintable, signals[INVALIDATE_SIZE], 0);
}
