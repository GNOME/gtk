#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"
#include "gskpathmeasure.h"

G_BEGIN_DECLS

GskPathPoint * gsk_path_point_new (GskPathMeasure *measure,
                                   const GskContour *contour,
                                   gpointer          measure_data,
                                   float             contour_offset,
                                   float             offset);

G_END_DECLS

