/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gdkprivate-x11.h"
#include "gdkpango.h"
#include <pango/pangox.h>

#define GDK_INFO_KEY "gdk-info"

typedef struct _GdkPangoXInfo GdkPangoXInfo;

struct _GdkPangoXInfo
{
  GdkColormap *colormap;
};

static void
gdk_pango_context_destroy (GdkPangoXInfo *info)
{
  gdk_colormap_unref (info->colormap);
  g_free (info);
}

GC
duplicate_gc (Display *display,
	      Drawable drawable,
	      GC       orig)
{
  GC result = XCreateGC (display, drawable, 0, NULL);

  XCopyGC (display, orig, ~((~1) << GCLastBit), result);

  return result;
}

GC
gdk_pango_get_gc_func (PangoContext   *context,
		       PangoAttrColor *fg_color,
		       GC              base_gc)
{
  GdkPangoXInfo *info;
  GdkColormap *colormap;
  GdkColor color;
  GC result;
  
  g_return_val_if_fail (context != NULL, NULL);

  info = pango_context_get_data (context, GDK_INFO_KEY);
  g_return_val_if_fail (info != NULL, NULL);

  if (info->colormap)
    colormap = info->colormap;
  else
    colormap = gdk_colormap_get_system();

  /* FIXME. FIXME. FIXME. Only works for true color */

  color.red = fg_color->red;
  color.green = fg_color->green;
  color.blue = fg_color->blue;
  
  if (gdk_colormap_alloc_color (colormap, &color, FALSE, TRUE))
    {
      GC result = duplicate_gc (GDK_DISPLAY (), GDK_ROOT_WINDOW (), base_gc);
      XSetForeground (GDK_DISPLAY (), result, color.pixel);

      return result;
    }
  else
    return duplicate_gc (GDK_DISPLAY (), GDK_ROOT_WINDOW (), base_gc);
  
  return result;
}

void
gdk_pango_free_gc_func (PangoContext *context,
			GC            gc)
{
  XFreeGC (GDK_DISPLAY (), gc);
}

PangoContext *
gdk_pango_context_get (void)
{
  GdkPangoXInfo *info = g_new (GdkPangoXInfo, 1);
  
  PangoContext *result = pango_x_get_context (GDK_DISPLAY ());
  pango_context_set_data (result, GDK_INFO_KEY,
			  info, (GDestroyNotify)gdk_pango_context_destroy);

  pango_x_context_set_funcs (result, gdk_pango_get_gc_func, gdk_pango_free_gc_func);
  info->colormap = NULL;

  return result;
}

void
gdk_pango_context_set_colormap (PangoContext *context,
				GdkColormap  *colormap)
{
  GdkPangoXInfo *info;
  
  g_return_if_fail (context != NULL);

  info = pango_context_get_data (context, GDK_INFO_KEY);
  g_return_if_fail (info != NULL);
  
  if (info->colormap != colormap)
    {
      if (info->colormap)
	gdk_colormap_unref (info->colormap);

      info->colormap = colormap;
      
      if (info->colormap)
   	gdk_colormap_ref (info->colormap);
    }
}
