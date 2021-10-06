/* gdkiccprofile.h
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

#ifndef __GDK_ICC_PROFILE_H__
#define __GDK_ICC_PROFILE_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdkcolorprofile.h>

G_BEGIN_DECLS

#define GDK_TYPE_ICC_PROFILE (gdk_icc_profile_get_type ())

#define GDK_ICC_PROFILE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_ICC_PROFILE, GdkICCProfile))
#define GDK_IS_ICC_PROFILE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_ICC_PROFILE))
#define GDK_ICC_PROFILE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_ICC_PROFILE, GdkICCProfileClass))
#define GDK_IS_ICC_PROFILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_ICC_PROFILE))
#define GDK_ICC_PROFILE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_ICC_PROFILE, GdkICCProfileClass))


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkICCProfile, g_object_unref)

typedef struct _GdkICCProfileClass        GdkICCProfileClass;

GDK_AVAILABLE_IN_4_6
GType                        gdk_icc_profile_get_type                   (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_6
GdkICCProfile *              gdk_icc_profile_new_from_icc_bytes         (GBytes               *bytes,
                                                                         GError              **error);

GDK_AVAILABLE_IN_4_6
GBytes *                     gdk_icc_profile_get_icc_profile            (GdkICCProfile      *self);

G_END_DECLS

#endif /* __GDK_ICC_PROFILE_H__ */
