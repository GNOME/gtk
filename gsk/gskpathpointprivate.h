#pragma once

#include "gskpathpoint.h"
#include "gskcontourprivate.h"

G_BEGIN_DECLS

#define GSK_PATH_POINT_INIT(c,i,tt) ((GskPathPoint){ .contour=(c), .idx=(i), .t=(tt) })

G_END_DECLS

