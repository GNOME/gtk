/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_PANGO_H__
#define __GDK_PANGO_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Pango interaction */

/* FIXME: The following function needs more parameters so that we can
 * properly deal with different visuals, the potential for multiple
 * screens in the future, etc. The PangoContext needs enough information
 * in it to create new GC's and to set the colors on those GC's.
 * A colormap is not sufficient.
 */
PangoContext *gdk_pango_context_get            (void);
void          gdk_pango_context_set_colormap   (PangoContext *context,
                                                GdkColormap  *colormap);


/* Get a clip region to draw only part of a layout or
 * line. index_ranges contains alternating range starts/stops. The
 * region is the region which contains the given ranges, i.e. if you
 * draw with the region as clip, only the given ranges are drawn.
 */

GdkRegion    *gdk_pango_layout_line_get_clip_region (PangoLayoutLine *line,
                                                     gint             x_origin,
                                                     gint             y_origin,
                                                     gint            *index_ranges,
                                                     gint             n_ranges);
GdkRegion    *gdk_pango_layout_get_clip_region      (PangoLayout     *layout,
                                                     gint             x_origin,
                                                     gint             y_origin,
                                                     gint            *index_ranges,
                                                     gint             n_ranges);



/* Attributes use to render insensitive text in GTK+. */

typedef struct _GdkPangoAttrStipple GdkPangoAttrStipple;
typedef struct _GdkPangoAttrEmbossed GdkPangoAttrEmbossed;

struct _GdkPangoAttrStipple
{
  PangoAttribute attr;
  GdkBitmap *stipple;
};

struct _GdkPangoAttrEmbossed
{
  PangoAttribute attr;
  gboolean embossed;
};

PangoAttribute *gdk_pango_attr_stipple_new  (GdkBitmap *bitmap);
PangoAttribute *gdk_pango_attr_embossed_new (gboolean embossed);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_FONT_H__ */
