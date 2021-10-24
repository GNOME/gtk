/* gdkcolor.h
 *
 * Copyright 2021 (c) Benjamin Otte
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

#ifndef __GDK_COLOR_H__
#define __GDK_COLOR_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_COLOR (gdk_color_get_type ())

GDK_AVAILABLE_IN_4_6
GType                   gdk_color_get_type                      (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_6
GdkColor *              gdk_color_new                           (GdkColorSpace          *space,
                                                                 float                   alpha,
                                                                 float                  *components,
                                                                 gsize                   n_components);
GDK_AVAILABLE_IN_4_6
GdkColor *              gdk_color_new_from_rgba                 (const GdkRGBA          *rgba);
GDK_AVAILABLE_IN_4_6
GdkColor *              gdk_color_copy                          (const GdkColor         *self);
GDK_AVAILABLE_IN_4_6
void                    gdk_color_free                          (GdkColor               *self);

GDK_AVAILABLE_IN_4_6
GdkColorSpace *         gdk_color_get_color_space               (const GdkColor         *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_6
float                   gdk_color_get_alpha                     (const GdkColor         *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_6
const float *           gdk_color_get_components                (const GdkColor         *self) G_GNUC_PURE;
GDK_AVAILABLE_IN_4_6
gsize                   gdk_color_get_n_components              (const GdkColor         *self) G_GNUC_PURE;

GDK_AVAILABLE_IN_4_6
GdkColor *              gdk_color_convert                       (const GdkColor         *self,
                                                                 GdkColorSpace          *space);
/*
GDK_AVAILABLE_IN_4_6
GdkColor *              gdk_color_mix                           (const GdkColor         *self,
                                                                 const GdkColor         *other,
                                                                 double                  progress,
                                                                 GdkColorSpace          *space);
*/

G_END_DECLS

#endif /* __GDK_COLOR_H__ */
