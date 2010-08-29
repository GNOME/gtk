/* gdkcolor-quartz.c
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkcolor.h"
#include "gdkprivate-quartz.h"

GType
gdk_colormap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkColormapClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkColormap),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkColormap",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkColormap *
gdk_colormap_new (GdkVisual *visual,
		  gint       private_cmap)
{
  g_return_val_if_fail (visual != NULL, NULL);

  /* FIXME: Implement */
  return NULL;
}

void
gdk_colormap_free_colors (GdkColormap    *colormap,
                          const GdkColor *colors,
                          gint            n_colors)
{
  /* This function shouldn't do anything since colors are never allocated. */
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
			   GdkColor    *colors,
			   gint         ncolors,
			   gboolean     writeable,
			   gboolean     best_match,
			   gboolean    *success)
{
  int i;
  int alpha;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), ncolors);
  g_return_val_if_fail (colors != NULL, ncolors);
  g_return_val_if_fail (success != NULL, ncolors);

  if (gdk_colormap_get_visual (colormap)->depth == 32)
    alpha = 0xff;
  else
    alpha = 0;

  for (i = 0; i < ncolors; i++)
    {
      colors[i].pixel = alpha << 24 |
        ((colors[i].red >> 8) & 0xff) << 16 |
        ((colors[i].green >> 8) & 0xff) << 8 |
        ((colors[i].blue >> 8) & 0xff);
    }

  *success = TRUE;

  return 0;
}

GdkScreen*
gdk_colormap_get_screen (GdkColormap *cmap)
{
  g_return_val_if_fail (cmap != NULL, NULL);

  return gdk_screen_get_default ();
}

