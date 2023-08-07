#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"

G_BEGIN_DECLS

struct _GskRealPathPoint
{
  GskPath *path;
  gsize contour;

  union {
    struct {
      unsigned int idx;
      float t;
    } std;
    struct {
      float distance;
    } rect;
  } data;
};

G_STATIC_ASSERT (sizeof (GskRealPathPoint) <= sizeof (GskPathPoint));

G_END_DECLS

