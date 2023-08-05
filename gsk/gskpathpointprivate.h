#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"

G_BEGIN_DECLS

struct _GskRealPathPoint
{
  GskPath *path;
  const GskContour *contour;

  union {
    struct {
      unsigned int idx;
      float t;
    } std;
    struct {
      float distance;
    } rect;
    struct {
      float angle;
    } circle;
  } data;
};

GskPath          * gsk_path_point_get_path     (GskPathPoint     *self);
const GskContour * gsk_path_point_get_contour  (GskPathPoint     *self);
float              gsk_path_point_get_distance (GskPathPoint     *self,
                                                gpointer          measure_data);

G_END_DECLS

