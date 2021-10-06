/* gdkderivedprofile.h
 *
 * Copyright 2021 (c) Red Hat, Inc.
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

#ifndef __GDK_DERIVED_PROFILE_H__
#define __GDK_DERIVED_PROFILE_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdkcolorprofile.h>
#include <gdk/gdkiccprofile.h>

G_BEGIN_DECLS

#define GDK_TYPE_DERIVED_PROFILE (gdk_derived_profile_get_type ())

#define GDK_DERIVED_PROFILE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_DERIVED_PROFILE, GdkDerivedProfile))
#define GDK_IS_DERIVED_PROFILE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_DERIVED_PROFILE))
#define GDK_DERIVED_PROFILE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DERIVED_PROFILE, GdkDerivedProfileClass))
#define GDK_IS_DERIVED_PROFILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DERIVED_PROFILE))
#define GDK_DERIVED_PROFILE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DERIVED_PROFILE, GdkDerivedProfileClass))


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkDerivedProfile, g_object_unref)

typedef struct _GdkDerivedProfileClass        GdkDerivedProfileClass;

GDK_AVAILABLE_IN_4_6
GType                        gdk_derived_profile_get_type  (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_6
GdkColorProfile *            gdk_derived_profile_get_base_profile (GdkDerivedProfile *profile);

typedef enum {
  GDK_COLOR_SPACE_HSL,
  GDK_COLOR_SPACE_HWB,
} GdkColorSpace;

GDK_AVAILABLE_IN_4_6
GdkColorSpace                gdk_derived_profile_get_color_space (GdkDerivedProfile *profile);

GDK_AVAILABLE_IN_4_6
void                         gdk_derived_profile_convert_to_base_profile   (GdkDerivedProfile *profile,
                                                                            const float       *in,
                                                                            float             *out);

GDK_AVAILABLE_IN_4_6
void                         gdk_derived_profile_convert_from_base_profile (GdkDerivedProfile *profile,
                                                                            const float       *in,
                                                                            float             *out);

G_END_DECLS

#endif /* __GDK_DERIVED_PROFILE_H__ */
