#ifndef _SNAP_
#define _SNAP_

#define GskSnapDirection uint

float
gsk_snap (float            value,
          GskSnapDirection snap)
{
  switch (snap)
    {
    case GSK_SNAP_FLOOR:
      return floor (value);
    case GSK_SNAP_CEIL:
      return ceil (value);
    case GSK_SNAP_ROUND:
      return round (value);
    case GSK_SNAP_NONE:
    default:
      return value;
    }
}

#endif
