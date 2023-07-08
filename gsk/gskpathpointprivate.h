#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"

G_BEGIN_DECLS

struct _GskPathPoint
{
  guint ref_count;

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

GskPathPoint *     gsk_path_point_new          (GskPath          *path);

GskPath          * gsk_path_point_get_path     (GskPathPoint     *self);
const GskContour * gsk_path_point_get_contour  (GskPathPoint     *self);

G_END_DECLS

