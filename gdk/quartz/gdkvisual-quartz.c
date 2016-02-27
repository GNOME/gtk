/* gdkvisual-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "gdkvisualprivate.h"
#include "gdkquartzvisual.h"
#include "gdkprivate-quartz.h"


struct _GdkQuartzVisual
{
  GdkVisual visual;
};

struct _GdkQuartzVisualClass
{
  GdkVisualClass visual_class;
};


static GdkVisual *system_visual;
static GdkVisual *rgba_visual;
static GdkVisual *gray_visual;

static GdkVisual *
create_standard_visual (GdkScreen *screen,
                        gint       depth)
{
  GdkVisual *visual = g_object_new (GDK_TYPE_QUARTZ_VISUAL, NULL);

  visual->screen = screen;

  visual->depth = depth;
  visual->byte_order = GDK_MSB_FIRST; /* FIXME: Should this be different on intel macs? */
  visual->colormap_size = 0;

  visual->type = GDK_VISUAL_TRUE_COLOR;

  visual->red_mask = 0xff0000;
  visual->green_mask = 0xff00;
  visual->blue_mask = 0xff;

  return visual;
}

static GdkVisual *
create_gray_visual (GdkScreen *screen)
{
  GdkVisual *visual = g_object_new (GDK_TYPE_QUARTZ_VISUAL, NULL);

  visual->screen = screen;

  visual->depth = 1;
  visual->byte_order = GDK_MSB_FIRST;
  visual->colormap_size = 0;

  visual->type = GDK_VISUAL_STATIC_GRAY;

  return visual;
}


G_DEFINE_TYPE (GdkQuartzVisual, gdk_quartz_visual, GDK_TYPE_VISUAL)

static void
gdk_quartz_visual_init (GdkQuartzVisual *quartz_visual)
{
}

static void
gdk_quartz_visual_class_init (GdkQuartzVisualClass *class)
{
}

/* We prefer the system visual for now ... */
gint
_gdk_quartz_screen_visual_get_best_depth (GdkScreen *screen)
{
  return system_visual->depth;
}

GdkVisualType
_gdk_quartz_screen_visual_get_best_type (GdkScreen *screen)
{
  return system_visual->type;
}

GdkVisual *
_gdk_quartz_screen_get_rgba_visual (GdkScreen *screen)
{
  return rgba_visual;
}

GdkVisual*
_gdk_quartz_screen_get_system_visual (GdkScreen *screen)
{
  return system_visual;
}

GdkVisual*
_gdk_quartz_screen_visual_get_best (GdkScreen *screen)
{
  return system_visual;
}

GdkVisual*
_gdk_quartz_screen_visual_get_best_with_depth (GdkScreen *screen,
                                               gint       depth)
{
  GdkVisual *visual = NULL;

  switch (depth)
    {
      case 32:
        visual = rgba_visual;
        break;

      case 24:
        visual = system_visual;
        break;

      case 1:
        visual = gray_visual;
        break;

      default:
        visual = NULL;
    }

  return visual;
}

GdkVisual*
_gdk_quartz_screen_visual_get_best_with_type (GdkScreen     *screen,
                                              GdkVisualType  visual_type)
{
  if (system_visual->type == visual_type)
    return system_visual;
  else if (gray_visual->type == visual_type)
    return gray_visual;

  return NULL;
}

GdkVisual*
_gdk_quartz_screen_visual_get_best_with_both (GdkScreen     *screen,
                                              gint           depth,
                                              GdkVisualType  visual_type)
{
  if (system_visual->depth == depth
      && system_visual->type == visual_type)
    return system_visual;
  else if (rgba_visual->depth == depth
           && rgba_visual->type == visual_type)
    return rgba_visual;
  else if (gray_visual->depth == depth
           && gray_visual->type == visual_type)
    return gray_visual;

  return NULL;
}

/* For these, we also prefer the system visual */
void
_gdk_quartz_screen_query_depths  (GdkScreen  *screen,
                                  gint      **depths,
                                  gint       *count)
{
  *count = 1;
  *depths = &system_visual->depth;
}

void
_gdk_quartz_screen_query_visual_types (GdkScreen      *screen,
                                       GdkVisualType **visual_types,
                                       gint           *count)
{
  *count = 1;
  *visual_types = &system_visual->type;
}

void
_gdk_quartz_screen_init_visuals (GdkScreen *screen)
{
  system_visual = create_standard_visual (screen, 24);
  rgba_visual = create_standard_visual (screen, 32);
  gray_visual = create_gray_visual (screen);
}

GList*
_gdk_quartz_screen_list_visuals (GdkScreen *screen)
{
  GList *visuals = NULL;

  visuals = g_list_append (visuals, system_visual);
  visuals = g_list_append (visuals, rgba_visual);
  visuals = g_list_append (visuals, gray_visual);

  return visuals;
}
