#pragma once

#include "gskrectsnap.h"

#define gsk_rect_snap_new(...) _gsk_rect_snap_new(__VA_ARGS__)
static inline GskRectSnap
_gsk_rect_snap_new (GskSnapDirection top,
                    GskSnapDirection right,
                    GskSnapDirection bottom,
                    GskSnapDirection left)
{
  return GSK_RECT_SNAP_INIT (top, right, bottom, left);
}

#define gsk_rect_snap_get_direction(...) _gsk_rect_snap_get_direction(__VA_ARGS__)
static inline GskSnapDirection
_gsk_rect_snap_get_direction (GskRectSnap snap,
                              unsigned    dir)
{ 
  return (GskSnapDirection) ((snap >> (8 * dir)) & 0xFF);
}

