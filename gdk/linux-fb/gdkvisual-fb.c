/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdkvisual.h"
#include "gdkprivate-fb.h"
#include "gdkinternals.h"
#include <endian.h>

static GdkVisual *system_visual = NULL;

#ifdef G_ENABLE_DEBUG

#if 0
static const gchar* visual_names[] =
{
  "static gray",
  "grayscale",
  "static color",
  "pseudo color",
  "true color",
  "direct color",
};
#endif

#endif /* G_ENABLE_DEBUG */

void
gdk_visual_init (void)
{
  system_visual = g_new0(GdkVisual, 1);

  system_visual->depth = system_visual->bits_per_rgb = gdk_display->modeinfo.bits_per_pixel;
  system_visual->byte_order = GDK_LSB_FIRST;
  system_visual->colormap_size = 0;

  switch(gdk_display->sinfo.visual)
    {
    case FB_VISUAL_PSEUDOCOLOR:
      system_visual->colormap_size = 1 << gdk_display->modeinfo.bits_per_pixel;
      system_visual->type = GDK_VISUAL_PSEUDO_COLOR;
      break;
    case FB_VISUAL_DIRECTCOLOR:
      system_visual->colormap_size = 1 << gdk_display->modeinfo.bits_per_pixel;
      system_visual->type = GDK_VISUAL_DIRECT_COLOR;
    case FB_VISUAL_TRUECOLOR:
      if(gdk_display->sinfo.visual == GDK_VISUAL_TRUE_COLOR)
	system_visual->type = GDK_VISUAL_TRUE_COLOR;

      system_visual->red_prec = MIN(system_visual->depth / 3, 8);
      system_visual->red_shift = 0;
      system_visual->red_mask = ((1 << (system_visual->red_prec + 1)) - 1) << system_visual->red_shift;

      system_visual->green_shift = system_visual->red_prec;
      system_visual->green_prec = MIN(system_visual->depth / 3, 8);
      system_visual->green_mask = ((1 << (system_visual->green_prec + 1)) - 1) << system_visual->green_shift;

      system_visual->blue_shift = system_visual->green_prec + system_visual->green_shift;
      system_visual->blue_prec = MIN(system_visual->depth / 3, 8);
      system_visual->blue_mask = ((1 << (system_visual->blue_prec + 1)) - 1) << system_visual->blue_shift;
      break;
    case FB_VISUAL_STATIC_PSEUDOCOLOR:
      system_visual->type = GDK_VISUAL_STATIC_COLOR;
      system_visual->colormap_size = 1 << gdk_display->modeinfo.bits_per_pixel;
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

GdkVisual*
gdk_visual_ref (GdkVisual *visual)
{
  return visual;
}

void
gdk_visual_unref (GdkVisual *visual)
{
}

gint
gdk_visual_get_best_depth (void)
{
  return system_visual->depth;
}

GdkVisualType
gdk_visual_get_best_type (void)
{
  return system_visual->type;
}

GdkVisual*
gdk_visual_get_system (void)
{
  return system_visual;
}

GdkVisual*
gdk_visual_get_best (void)
{
  return system_visual;
}

GdkVisual*
gdk_visual_get_best_with_depth (gint depth)
{
  if(system_visual->depth != depth)
    return NULL;

  return system_visual;
}

GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  if(system_visual->type != visual_type)
    return NULL;

  return system_visual;
}

GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  if(system_visual->depth != depth)
    return NULL;

  if(system_visual->type != visual_type)
    return NULL;

  return system_visual;
}

void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  *count = 1;
  *depths = &system_visual->depth;
}

void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  *count = 1;
  *visual_types = &system_visual->type;
}

GList*
gdk_list_visuals (void)
{
  return g_list_append(NULL, gdk_visual_get_system());
}
