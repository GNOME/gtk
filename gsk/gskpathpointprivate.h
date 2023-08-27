#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"

G_BEGIN_DECLS

struct _GskRealPathPoint
{
  gsize contour;
  gsize idx;
  float t;
};

G_STATIC_ASSERT (sizeof (GskRealPathPoint) <= sizeof (GskPathPoint));

const char * gsk_path_point_to_string (GskPathPoint *point);

G_END_DECLS

