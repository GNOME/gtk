#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH_POINT (gsk_path_point_get_type ())

GType                   gsk_path_point_get_type        (void) G_GNUC_CONST;
GskPathPoint *          gsk_path_point_ref             (GskPathPoint *self);
void                    gsk_path_point_unref           (GskPathPoint *self);
GskPathMeasure *        gsk_path_point_get_measure     (GskPathPoint     *self);
float                   gsk_path_point_get_distance    (GskPathPoint     *self);
void                    gsk_path_point_get_position    (GskPathPoint     *self,
                                                        graphene_point_t *position);
void                    gsk_path_point_get_tangent     (GskPathPoint     *self,
                                                        GskPathDirection  direction,
                                                        graphene_vec2_t  *tangent);
float                   gsk_path_point_get_curvature   (GskPathPoint     *self,
                                                        graphene_point_t *center);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPathPoint, gsk_path_point_unref)

G_END_DECLS
