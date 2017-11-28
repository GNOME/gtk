/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte
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

#ifndef __GTK_CONTENT_FORMATS_H__
#define __GTK_CONTENT_FORMATS_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif


#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_CONTENT_FORMATS    (gdk_content_formats_get_type ())

GDK_AVAILABLE_IN_3_94
const char *            gdk_intern_mime_type                    (const char                     *string);

GDK_AVAILABLE_IN_3_94
GType                   gdk_content_formats_get_type            (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_94
GdkContentFormats *     gdk_content_formats_new                 (const char                    **mime_types,
                                                                 guint                           n_mime_types);
GDK_AVAILABLE_IN_3_94
GdkContentFormats *     gdk_content_formats_new_for_gtype       (GType                           type);
GDK_AVAILABLE_IN_3_94
GdkContentFormats *     gdk_content_formats_ref                 (GdkContentFormats              *formats);
GDK_AVAILABLE_IN_3_94
void                    gdk_content_formats_unref               (GdkContentFormats              *formats);

GDK_AVAILABLE_IN_3_94
void                    gdk_content_formats_print               (GdkContentFormats              *formats,
                                                                 GString                        *string);
GDK_AVAILABLE_IN_3_94
char *                  gdk_content_formats_to_string           (GdkContentFormats              *formats);

GDK_AVAILABLE_IN_3_94
const GType *           gdk_content_formats_get_gtypes          (GdkContentFormats              *formats,
                                                                 gsize                          *n_gtypes);
GDK_AVAILABLE_IN_3_94
const char * const *    gdk_content_formats_get_mime_types      (GdkContentFormats              *formats,
                                                                 gsize                          *n_mime_types);

GDK_AVAILABLE_IN_3_94
GdkContentFormats *     gdk_content_formats_union               (GdkContentFormats              *first,
                                                                 const GdkContentFormats        *second) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_3_94
gboolean                gdk_content_formats_match               (const GdkContentFormats        *first,
                                                                 const GdkContentFormats        *second);
GDK_AVAILABLE_IN_3_94
GType                   gdk_content_formats_match_gtype         (const GdkContentFormats        *first,
                                                                 const GdkContentFormats        *second);
GDK_AVAILABLE_IN_3_94
const char *            gdk_content_formats_match_mime_type     (const GdkContentFormats        *first,
                                                                 const GdkContentFormats        *second);
GDK_AVAILABLE_IN_3_94
gboolean                gdk_content_formats_contain_gtype       (const GdkContentFormats        *formats,
                                                                 GType                           type);
GDK_AVAILABLE_IN_3_94
gboolean                gdk_content_formats_contain_mime_type   (const GdkContentFormats        *formats,
                                                                 const char                     *mime_type);

typedef struct _GdkContentFormatsBuilder GdkContentFormatsBuilder;

GDK_AVAILABLE_IN_3_94
GdkContentFormatsBuilder*gdk_content_formats_builder_new        (void);
GDK_AVAILABLE_IN_3_94
GdkContentFormats *     gdk_content_formats_builder_free        (GdkContentFormatsBuilder       *builder) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_3_94
void                    gdk_content_formats_builder_add_formats (GdkContentFormatsBuilder       *builder,
                                                                 const GdkContentFormats        *formats);
GDK_AVAILABLE_IN_3_94
void                    gdk_content_formats_builder_add_mime_type(GdkContentFormatsBuilder      *builder,
                                                                 const char                     *mime_type);
GDK_AVAILABLE_IN_3_94
void                    gdk_content_formats_builder_add_gtype   (GdkContentFormatsBuilder       *builder,
                                                                 GType                           type);

G_END_DECLS

#endif /* __GTK_CONTENT_FORMATS_H__ */
