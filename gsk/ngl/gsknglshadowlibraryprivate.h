/* gsknglshadowlibraryprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GSK_NGL_SHADOW_LIBRARY_PRIVATE_H__
#define __GSK_NGL_SHADOW_LIBRARY_PRIVATE_H__

#include "gskngltexturelibraryprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_SHADOW_LIBRARY (gsk_ngl_shadow_library_get_type())

G_DECLARE_FINAL_TYPE (GskNglShadowLibrary, gsk_ngl_shadow_library, GSK, NGL_SHADOW_LIBRARY, GObject)

GskNglShadowLibrary *gsk_ngl_shadow_library_new         (GskNglDriver         *driver);
void                 gsk_ngl_shadow_library_begin_frame (GskNglShadowLibrary  *self);
guint                gsk_ngl_shadow_library_lookup      (GskNglShadowLibrary  *self,
                                                         const GskRoundedRect *outline,
                                                         float                 blur_radius);
void                 gsk_ngl_shadow_library_insert      (GskNglShadowLibrary  *self,
                                                         const GskRoundedRect *outline,
                                                         float                 blur_radius,
                                                         guint                 texture_id);

G_END_DECLS

#endif /* __GSK_NGL_SHADOW_LIBRARY_PRIVATE_H__ */
