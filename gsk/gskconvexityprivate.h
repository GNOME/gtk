#pragma once

#include "gskcontourprivate.h"

typedef enum
{
  GSK_CONVEXITY_UNKNOWN,
  GSK_CONVEXITY_CONVEX,
  GSK_CONVEXITY_CONCAVE,
} GskConvexity;

GskConvexity gsk_contour_compute_convexity (const GskContour *contour);


