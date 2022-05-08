/* gdkcolor_profile.h
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

#ifndef __GDK_COLOR_PROFILE_H__
#define __GDK_COLOR_PROFILE_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_COLOR_PROFILE (gdk_color_profile_get_type ())

#define GDK_COLOR_PROFILE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_COLOR_PROFILE, GdkColorProfile))
#define GDK_IS_COLOR_PROFILE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_COLOR_PROFILE))
#define GDK_COLOR_PROFILE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_COLOR_PROFILE, GdkColorProfileClass))
#define GDK_IS_COLOR_PROFILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_COLOR_PROFILE))
#define GDK_COLOR_PROFILE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_COLOR_PROFILE, GdkColorProfileClass))


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkColorProfile, g_object_unref)

typedef struct _GdkColorProfileClass        GdkColorProfileClass;

GDK_AVAILABLE_IN_4_8
GType                        gdk_color_profile_get_type                   (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_8
GdkColorProfile *            gdk_color_profile_get_srgb                   (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_8
GdkColorProfile *            gdk_color_profile_new_from_icc_bytes         (GBytes               *bytes,
                                                                           GError              **error);

GDK_AVAILABLE_IN_4_8
GBytes *                     gdk_color_profile_get_icc_profile            (GdkColorProfile      *self);

GDK_AVAILABLE_IN_4_8
gboolean                     gdk_color_profile_is_linear                  (GdkColorProfile      *self) G_GNUC_PURE;

GDK_AVAILABLE_IN_4_8
gsize                        gdk_color_profile_get_n_components           (GdkColorProfile      *self) G_GNUC_PURE;


GDK_AVAILABLE_IN_4_8
gboolean                     gdk_color_profile_equal                      (gconstpointer         profile1,
                                                                           gconstpointer         profile2);

G_END_DECLS

#endif /* __GDK_COLOR_PROFILE_H__ */
