/* gdkcolor_space.h
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

#ifndef __GDK_COLOR_SPACE_H__
#define __GDK_COLOR_SPACE_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkenums.h>

G_BEGIN_DECLS

#define GDK_TYPE_COLOR_SPACE (gdk_color_space_get_type ())

#define GDK_COLOR_SPACE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_COLOR_SPACE, GdkColorSpace))
#define GDK_IS_COLOR_SPACE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_COLOR_SPACE))
#define GDK_COLOR_SPACE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_COLOR_SPACE, GdkColorSpaceClass))
#define GDK_IS_COLOR_SPACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_COLOR_SPACE))
#define GDK_COLOR_SPACE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_COLOR_SPACE, GdkColorSpaceClass))


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkColorSpace, g_object_unref)

typedef struct _GdkColorSpaceClass        GdkColorSpaceClass;

GDK_AVAILABLE_IN_4_10
GType                   gdk_color_space_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_10
GdkColorSpace *         gdk_color_space_get_srgb                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_10
GdkColorSpace *         gdk_color_space_new_from_icc_profile    (GBytes               *icc_profile,
                                                                 GError              **error);

GDK_AVAILABLE_IN_4_10
GdkColorSpace *         gdk_color_space_new_from_cicp           (int                   color_primaries,
                                                                 int                   transfer_characteristics,
                                                                 int                   matrix_coefficients,
                                                                 gboolean              full_range);


GDK_AVAILABLE_IN_4_10
gboolean                gdk_color_space_supports_format         (GdkColorSpace        *self,
                                                                 GdkMemoryFormat       format);
GDK_AVAILABLE_IN_4_10
GBytes *                gdk_color_space_save_to_icc_profile     (GdkColorSpace        *self,
                                                                 GError              **error);

GDK_AVAILABLE_IN_4_10
gboolean                gdk_color_space_is_linear               (GdkColorSpace        *self);

GDK_AVAILABLE_IN_4_10
int                     gdk_color_space_get_n_components        (GdkColorSpace        *self);

GDK_AVAILABLE_IN_4_10
gboolean                gdk_color_space_equal                   (gconstpointer         profile1,
                                                                 gconstpointer         profile2);

G_END_DECLS

#endif /* __GDK_COLOR_SPACE_H__ */
