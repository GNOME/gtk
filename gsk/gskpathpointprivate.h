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

char * gsk_path_point_to_string (GskPathPoint *point);
void   gsk_path_point_print     (GskPathPoint *point);

G_END_DECLS

