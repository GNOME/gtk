#pragma once

#include "gsk/gskpath.h"
#include "gsk/gsktransform.h"
#include "gsk/gskroundedrect.h"
#include "gtk/gtksnapshot.h"
#include "gtk/gtkenums.h"


GskPath * svg_transform_path (GskTransform *transform,
                              GskPath      *path);

typedef enum
{
  PATH_EMPTY,
  PATH_RECT,
  PATH_ROUNDED_RECT,
  PATH_CIRCLE,
  PATH_GENERAL,
} SvgPathClassification;

SvgPathClassification svg_path_classify (GskPath        *path,
                                         GskRoundedRect *rect);

void svg_snapshot_push_fill (GtkSnapshot *snapshot,
                             GskPath     *path,
                             GskFillRule  rule);
