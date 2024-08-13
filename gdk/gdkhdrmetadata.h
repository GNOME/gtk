#pragma once

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_HDR_METADATA (gdk_hdr_metadata_get_type ())

GDK_AVAILABLE_IN_4_16
GType            gdk_hdr_metadata_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_16
GdkHdrMetadata * gdk_hdr_metadata_new   (float rx, float ry,
                                         float gx, float gy,
                                         float bx, float by,
                                         float wx, float wy,
                                         float min_lum, float max_lum,
                                         float max_cll, float max_fall);

GDK_AVAILABLE_IN_4_16
GdkHdrMetadata * gdk_hdr_metadata_ref      (GdkHdrMetadata *self);

GDK_AVAILABLE_IN_4_16
void             gdk_hdr_metadata_unref    (GdkHdrMetadata *self);

GDK_AVAILABLE_IN_4_16
gboolean         gdk_hdr_metadata_equal (const GdkHdrMetadata *v1,
                                         const GdkHdrMetadata *v2);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkHdrMetadata, gdk_hdr_metadata_unref);


G_END_DECLS
