#pragma once

#include <glib.h>
#include "gdkhdrmetadata.h"

struct _GdkHdrMetadata
{
  gatomicrefcount ref_count;

  float rx, ry;
  float gx, gy;
  float bx, by;
  float wx, wy;
  float min_lum;
  float max_lum;
  float max_cll;
  float max_fall;
};

static inline char *
gdk_hdr_metadata_to_string (GdkHdrMetadata *hdr)
{
  return g_strdup_printf ("r %f %f, g %f %f, b %f %f, w %f %f, lum %f %f, avg %f %f",
                          hdr->rx, hdr->ry,
                          hdr->gx, hdr->gy,
                          hdr->bx, hdr->by,
                          hdr->wx, hdr->wy,
                          hdr->min_lum, hdr->max_lum,
                          hdr->max_cll, hdr->max_fall);
}
