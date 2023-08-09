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

G_END_DECLS

