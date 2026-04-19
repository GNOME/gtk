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

#define gsk_rect_snap_can_shrink(...) _gsk_rect_snap_can_shrink(__VA_ARGS__)
static inline gboolean
_gsk_rect_snap_can_shrink (GskRectSnap snap)
{
  GskSnapDirection dir;

  dir = _gsk_rect_snap_get_direction (snap, 0);
  if (dir == GSK_SNAP_CEIL || dir == GSK_SNAP_ROUND)
    return TRUE;

  dir = _gsk_rect_snap_get_direction (snap, 1);
  if (dir == GSK_SNAP_FLOOR)
    return TRUE;

  dir = _gsk_rect_snap_get_direction (snap, 2);
  if (dir == GSK_SNAP_FLOOR)
    return TRUE;

  dir = _gsk_rect_snap_get_direction (snap, 3);
  if (dir == GSK_SNAP_CEIL || dir == GSK_SNAP_ROUND)
    return TRUE;

  return FALSE;
}

#define gsk_rect_snap_can_grow(...) _gsk_rect_snap_can_grow(__VA_ARGS__)
static inline gboolean
_gsk_rect_snap_can_grow (GskRectSnap snap)
{
  GskSnapDirection dir;

  dir = _gsk_rect_snap_get_direction (snap, 0);
  if (dir == GSK_SNAP_FLOOR)
    return TRUE;

  dir = _gsk_rect_snap_get_direction (snap, 1);
  if (dir == GSK_SNAP_CEIL || dir == GSK_SNAP_ROUND)
    return TRUE;

  dir = _gsk_rect_snap_get_direction (snap, 2);
  if (dir == GSK_SNAP_CEIL || dir == GSK_SNAP_ROUND)
    return TRUE;

  dir = _gsk_rect_snap_get_direction (snap, 3);
  if (dir == GSK_SNAP_FLOOR)
    return TRUE;

  return FALSE;
}

