#pragma once

#include <gdk/gdk.h>
#include <gsk/gsktypes.h>
#include <graphene.h>

#include "gskgputypesprivate.h"
#include "gsktransformprivate.h"

G_BEGIN_DECLS

typedef struct _GskGpuTransform GskGpuTransform;

struct _GskGpuTransform
{
  GdkDihedral dihedral;
  graphene_size_t scale; /* must be > 0 */
  graphene_point_t offset;
};

#define GSK_GPU_TRANSFORM_IDENTITIY (GskGpuTransform) { GDK_DIHEDRAL_NORMAL, { 1.0, 1.0 }, { 0.0, 0.0 } }

void                    gsk_gpu_transform_init                  (GskGpuTransform        *self,
                                                                 GdkDihedral             dihedral,
                                                                 const graphene_size_t  *size,
                                                                 const graphene_point_t *offset);

void                    gsk_gpu_transform_print                 (const GskGpuTransform  *self,
                                                                 GString                *str);
char *                  gsk_gpu_transform_to_string             (const GskGpuTransform  *self);

gboolean                gsk_gpu_transform_transform             (GskGpuTransform        *self,
                                                                 GskTransform           *transform);

void                    gsk_gpu_transform_transform_rect        (const GskGpuTransform  *self,
                                                                 const graphene_rect_t  *rect,
                                                                 graphene_rect_t        *result);
void                    gsk_gpu_transform_invert_rect           (const GskGpuTransform  *self,
                                                                 const graphene_rect_t  *rect,
                                                                 graphene_rect_t        *result);

G_END_DECLS

