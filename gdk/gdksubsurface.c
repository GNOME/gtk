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
#include "gdkdebugprivate.h"

G_DEFINE_TYPE (GdkSubsurface, gdk_subsurface, G_TYPE_OBJECT)

static void
gdk_subsurface_init (GdkSubsurface *self)
{
}

static void
gdk_subsurface_finalize (GObject *object)
{
  GdkSubsurface *subsurface = GDK_SUBSURFACE (object);

  g_ptr_array_remove (subsurface->parent->subsurfaces, subsurface);
  g_clear_object (&subsurface->parent);
  g_clear_pointer (&subsurface->dmabuf_formats, gdk_dmabuf_formats_unref);

  G_OBJECT_CLASS (gdk_subsurface_parent_class)->finalize (object);
}

static void
gdk_subsurface_class_init (GdkSubsurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_subsurface_finalize;
}

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

static inline gboolean
is_topmost_subsurface (GdkSubsurface *subsurface)
{
  GdkSurface *parent = subsurface->parent;

  return !subsurface->sibling_above && parent->subsurfaces_below != subsurface;
}

static inline GdkSubsurface *
find_topmost_subsurface (GdkSurface *surface)
{
  GdkSubsurface *top = surface->subsurfaces_above;

  if (top)
    {
      while (top->sibling_above)
        top = top->sibling_above;
    }

  return top;
}

gboolean
gdk_subsurface_attach (GdkSubsurface         *subsurface,
                       GdkTexture            *texture,
                       const graphene_rect_t *rect,
                       gboolean               above,
                       GdkSubsurface         *sibling)
{
  GdkSurface *parent = subsurface->parent;
  gboolean was_topmost, is_topmost;

  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);
  g_return_val_if_fail (sibling != subsurface, FALSE);
  g_return_val_if_fail (sibling == NULL || GDK_IS_SUBSURFACE (sibling), FALSE);
  g_return_val_if_fail (sibling == NULL || sibling->parent == subsurface->parent, FALSE);

  remove_subsurface (subsurface);

  was_topmost = is_topmost_subsurface (subsurface);

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

  is_topmost = is_topmost_subsurface (subsurface);

  if (!was_topmost && is_topmost)
    {
      GDK_DISPLAY_DEBUG (parent->display, DMABUF, "Using formats of topmost subsurface %p", subsurface);
      gdk_surface_set_effective_dmabuf_formats (parent, subsurface->dmabuf_formats);
    }
  else if (was_topmost && !is_topmost)
    {
      GdkSubsurface *top = find_topmost_subsurface (parent);

      if (top)
        {
          GDK_DISPLAY_DEBUG (parent->display, DMABUF, "Using formats of topmost subsurface %p", top);
          gdk_surface_set_effective_dmabuf_formats (parent, top->dmabuf_formats);
        }
      else
        {
          GDK_DISPLAY_DEBUG (parent->display, DMABUF, "Using formats of main surface");
          gdk_surface_set_effective_dmabuf_formats (parent, parent->dmabuf_formats);
        }
    }

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->attach (subsurface, texture, rect, above, sibling);
}

void
gdk_subsurface_detach (GdkSubsurface *subsurface)
{
  gboolean was_topmost;

  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));

  was_topmost = is_topmost_subsurface (subsurface);

  remove_subsurface (subsurface);

  if (was_topmost)
    {
      GdkSurface *parent = subsurface->parent;
      GdkSubsurface *top = find_topmost_subsurface (parent);

      if (top)
        gdk_surface_set_effective_dmabuf_formats (parent, top->dmabuf_formats);
      else
        gdk_surface_set_effective_dmabuf_formats (parent, parent->dmabuf_formats);
    }

  GDK_SUBSURFACE_GET_CLASS (subsurface)->detach (subsurface);
}

GdkTexture *
gdk_subsurface_get_texture (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->get_texture (subsurface);
}

void
gdk_subsurface_get_rect (GdkSubsurface   *subsurface,
                         graphene_rect_t *rect)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (rect != NULL);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->get_rect (subsurface, rect);
}

gboolean
gdk_subsurface_is_above_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), TRUE);

  return subsurface->above_parent;
}

void
gdk_subsurface_set_dmabuf_formats (GdkSubsurface    *subsurface,
                                   GdkDmabufFormats *formats)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (formats != NULL);

  g_clear_pointer (&subsurface->dmabuf_formats, gdk_dmabuf_formats_unref);
  subsurface->dmabuf_formats = gdk_dmabuf_formats_ref (formats);

  if (subsurface->parent && is_topmost_subsurface (subsurface))
    gdk_surface_set_effective_dmabuf_formats (subsurface->parent, formats);
}
