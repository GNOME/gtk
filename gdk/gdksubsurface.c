/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2023 Red Hat, Inc.
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

#include "config.h"

#include "gdksubsurfaceprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktexture.h"
#include "gsk/gskrectprivate.h"

G_DEFINE_TYPE (GdkSubsurface, gdk_subsurface, G_TYPE_OBJECT)

static void
gdk_subsurface_init (GdkSubsurface *self)
{
}

static void remove_subsurface (GdkSubsurface *subsurface);

static void
gdk_subsurface_finalize (GObject *object)
{
  GdkSubsurface *subsurface = GDK_SUBSURFACE (object);

  remove_subsurface (subsurface);
  g_ptr_array_remove (subsurface->parent->subsurfaces, subsurface);
  g_clear_object (&subsurface->parent);

  G_OBJECT_CLASS (gdk_subsurface_parent_class)->finalize (object);
}

static void
gdk_subsurface_class_init (GdkSubsurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_subsurface_finalize;
}

/*< private >
 * gdk_subsurface_get_parent:
 * @subsurface: a `GdkSubsurface`
 *
 * Returns the parent surface of @subsurface.
 *
 * Returns: the parent surface
 */
GdkSurface *
gdk_subsurface_get_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return subsurface->parent;
}

static void
remove_subsurface (GdkSubsurface *subsurface)
{
  GdkSurface *parent = subsurface->parent;

  if (parent->subsurfaces_above == subsurface)
    parent->subsurfaces_above = subsurface->sibling_above;
  if (parent->subsurfaces_below == subsurface)
    parent->subsurfaces_below = subsurface->sibling_below;

  if (subsurface->sibling_above)
    subsurface->sibling_above->sibling_below = subsurface->sibling_below;
  if (subsurface->sibling_below)
    subsurface->sibling_below->sibling_above = subsurface->sibling_above;

  subsurface->sibling_above = NULL;
  subsurface->sibling_below = NULL;
}

static void
insert_subsurface (GdkSubsurface *subsurface,
                   gboolean       above,
                   GdkSubsurface *sibling)
{
  GdkSurface *parent = subsurface->parent;

  subsurface->above_parent = sibling->above_parent;

  if (above)
    {
      subsurface->sibling_above = sibling->sibling_above;
      sibling->sibling_above = subsurface;
      subsurface->sibling_below = sibling;
      if (subsurface->sibling_above)
        subsurface->sibling_above->sibling_below = subsurface;

      if (parent->subsurfaces_below == sibling)
        parent->subsurfaces_below = subsurface;
    }
  else
    {
      subsurface->sibling_below = sibling->sibling_below;
      sibling->sibling_below = subsurface;
      subsurface->sibling_above = sibling;
      if (subsurface->sibling_below)
        subsurface->sibling_below->sibling_above = subsurface;

      if (parent->subsurfaces_above == sibling)
        parent->subsurfaces_above = subsurface;
    }
}

/*< private >
 * gdk_subsurface_attach:
 * @subsurface: the `GdkSubsurface`
 * @texture: the texture to attach. This typically has to be a `GdkDmabufTexture`
 * @source: the source rectangle (i.e. the subset of the texture) to display
 * @dest: the dest rectangle, in application pixels, relative to the parent surface.
 *   It must be integral in application and device pixels, or attaching will fail
 * @transform: the transform to apply to the texture contents before displaying
 * @background: (nullable): the background rectangle, in application pixels relative
 *   to the parent surface. This tells GDK to put a black background of this
 *   size below the subsurface. It must be integral in application and device pixels,
 *   or attaching will fail
 * @above: whether the subsurface should be above its sibling
 * @sibling: (nullable): the sibling subsurface to stack relative to, or `NULL` to
 *   stack relative to the parent surface
 *
 * Attaches content to a subsurface.
 *
 * This function takes all the necessary arguments to determine the subsurface
 * configuration, including its position, size, content, background and stacking.
 *
 * Returns: `TRUE` if the attaching succeeded
 */
gboolean
gdk_subsurface_attach (GdkSubsurface         *subsurface,
                       GdkTexture            *texture,
                       const graphene_rect_t *source,
                       const graphene_rect_t *dest,
                       GdkDihedral            transform,
                       const graphene_rect_t *background,
                       gboolean               above,
                       GdkSubsurface         *sibling)
{
  GdkSurface *parent = subsurface->parent;
  gboolean result;

  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (source != NULL &&
                        gsk_rect_contains_rect (&GRAPHENE_RECT_INIT (0, 0,
                                                                     gdk_dihedral_swaps_xy (transform) ? gdk_texture_get_height (texture) : gdk_texture_get_width (texture),
                                                                     gdk_dihedral_swaps_xy (transform) ? gdk_texture_get_width (texture) : gdk_texture_get_height (texture)),
                                                source), FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);
  g_return_val_if_fail (sibling != subsurface, FALSE);
  g_return_val_if_fail (sibling == NULL || GDK_IS_SUBSURFACE (sibling), FALSE);
  g_return_val_if_fail (sibling == NULL || sibling->parent == subsurface->parent, FALSE);

  /* if the texture fully covers the background, ignore it */
  if (background &&
      gsk_rect_contains_rect (dest, background) &&
      gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_OPAQUE)
    background = NULL;

  result = GDK_SUBSURFACE_GET_CLASS (subsurface)->attach (subsurface,
                                                          texture,
                                                          source,
                                                          dest,
                                                          transform,
                                                          background,
                                                          above,
                                                          sibling);

  remove_subsurface (subsurface);

  if (sibling)
    {
      insert_subsurface (subsurface, above, sibling);
    }
  else
    {
      sibling = above ? parent->subsurfaces_above : parent->subsurfaces_below;

      if (sibling)
        {
          insert_subsurface (subsurface, !above, sibling);
        }
      else
        {
          subsurface->above_parent = above;

          if (above)
            parent->subsurfaces_above = subsurface;
          else
            parent->subsurfaces_below = subsurface;
        }
    }

  return result;
}

/*< private >
 * gdk_subsurface_detach:
 * @subsurface: a `GdkSubsurface`
 *
 * Hides the subsurface.
 *
 * To show it again, you need to call gdk_subsurface_attach().
 */
void
gdk_subsurface_detach (GdkSubsurface *subsurface)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));

  remove_subsurface (subsurface);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->detach (subsurface);
}

/*< private >
 * gdk_subsurface_get_texture:
 * @subsurface: a `GdkSubsurface`
 *
 * Gets the texture that is currently displayed by the subsurface.
 *
 * Returns: (nullable): the texture that is displayed
 */
GdkTexture *
gdk_subsurface_get_texture (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->get_texture (subsurface);
}

/*< private >
 * gdk_subsurface_get_source_rect:
 * @subsurface: a `GdkSubsurface`
 * @rect: (out caller-allocates): return location for the rectangle
 *
 * Returns the source rect that was specified in the most recent
 * gdk_subsurface_attach() call for @subsurface.
 */
void
gdk_subsurface_get_source_rect (GdkSubsurface   *subsurface,
                                graphene_rect_t *rect)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (rect != NULL);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->get_source_rect (subsurface, rect);
}

/*< private >
 * gdk_subsurface_get_texture_rect:
 * @subsurface: a `GdkSubsurface`
 * @rect: (out caller-allocates): return location for the rectangle
 *
 * Returns the texture rect that was specified in the most recent
 * gdk_subsurface_attach() call for @subsurface.
 */
void
gdk_subsurface_get_texture_rect (GdkSubsurface   *subsurface,
                                 graphene_rect_t *rect)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (rect != NULL);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->get_texture_rect (subsurface, rect);
}

/*< private >
 * gdk_subsurface_is_above_parent:
 * @subsurface: a `GdkSubsurface`
 *
 * Returns whether the subsurface is above the parent surface
 * or below. Note that a subsurface can be above its parent
 * surface, and still be covered by sibling subsurfaces.
 *
 * Returns: `TRUE` if @subsurface is above its parent
 */
gboolean
gdk_subsurface_is_above_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), FALSE);

  return subsurface->above_parent;
}

/*< private>
 * gdk_subsurface_get_sibling:
 * @subsurface: the `GdkSubsurface`
 * @above: whether to get the subsurface above
 *
 * Returns the subsurface above (or below) @subsurface in
 * the stacking order.
 *
 * Returns: the sibling, or `NULL` if there is none.
 */
GdkSubsurface *
gdk_subsurface_get_sibling (GdkSubsurface *subsurface,
                            gboolean       above)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  if (above)
    return subsurface->sibling_above;
  else
    return subsurface->sibling_below;
}

/*< private >
 * gdk_subsurface_get_transform:
 * @subsurface: a `GdkSubsurface`
 *
 * Returns the transform that was specified in the most recent call to
 * gdk_subsurface_attach() call for @subsurface.
 *
 * Returns: the transform
 */
GdkDihedral
gdk_subsurface_get_transform (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), GDK_DIHEDRAL_NORMAL);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->get_transform (subsurface);
}

/*< private >
 * gdk_subsurface_get_background_rect:
 * @subsurface: a `GdkSubsurface`
 * @rect: (out caller-allocates): return location for the rectangle
 *
 * Obtains the background rect that was specified in the most recent
 * gdk_subsurface_attach() call for @subsurface.
 *
 * Returns: `TRUE` if @subsurface has a background
 */
gboolean
gdk_subsurface_get_background_rect (GdkSubsurface   *subsurface,
                                    graphene_rect_t *rect)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->get_background_rect (subsurface, rect);
}

/*< private >
 * gdk_subsurface_get_bounds:
 * @subsurface: a `GdkSubsurface`
 * @bounds: (out caller-allocates): return location for the bounds
 *
 * Returns the bounds of the subsurface.
 *
 * The bounds are the union of the texture and background rects.
 */
void
gdk_subsurface_get_bounds (GdkSubsurface   *subsurface,
                           graphene_rect_t *bounds)
{
  graphene_rect_t background;

  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (bounds != NULL);

  gdk_subsurface_get_texture_rect (subsurface, bounds);
  if (gdk_subsurface_get_background_rect (subsurface, &background))
    graphene_rect_union (bounds, &background, bounds);
}
