
#include "config.h"

#include "gdkdihedralprivate.h"

void
gdk_dihedral_get_mat2 (GdkDihedral  transform,
                       float       *xx,
                       float       *xy,
                       float       *yx,
                       float       *yy)
{
  const float mat[8][2][2] = {
    [GDK_DIHEDRAL_NORMAL] = {
      {  1.0,  0.0 },
      {  0.0,  1.0 }
    },
    [GDK_DIHEDRAL_90] = {
      {  0.0,  1.0 },
      { -1.0,  0.0 }
    },
    [GDK_DIHEDRAL_180] = {
      { -1.0,  0.0 },
      {  0.0, -1.0 }
    },
    [GDK_DIHEDRAL_270] = {
      {  0.0, -1.0 },
      {  1.0,  0.0 }
    },
    [GDK_DIHEDRAL_FLIPPED] = {
      { -1.0,  0.0 },
      {  0.0,  1.0 }
    },
    [GDK_DIHEDRAL_FLIPPED_90] = {
      {  0.0, -1.0 },
      { -1.0,  0.0 }
    },
    [GDK_DIHEDRAL_FLIPPED_180] = {
      {  1.0,  0.0 },
      {  0.0, -1.0 }
    },
    [GDK_DIHEDRAL_FLIPPED_270] = {
      {  0.0,  1.0 },
      {  1.0,  0.0 }
    },
  };

  *xx = mat[transform][0][0];
  *xy = mat[transform][1][0];
  *yx = mat[transform][0][1];
  *yy = mat[transform][1][1];
}

GdkDihedral
gdk_dihedral_combine (GdkDihedral first,
                      GdkDihedral second)
{
  return ((first & 4) ^ (second & 4)) |
         ((((first & 3) * (((second & 4) >> 1) + 1)) + second) & 3);
}

GdkDihedral
gdk_dihedral_invert (GdkDihedral self)
{
  return ((4 - self) * (((self & 4) >> 1) + 1) & 3) | (self & 4);
}

gboolean
gdk_dihedral_swaps_xy (GdkDihedral self)
{
  return (self & 1) ? TRUE : FALSE;
}

const char *
gdk_dihedral_get_name (GdkDihedral self)
{
  const char *name[] = {
    "normal", "90", "180", "270", "flipped", "flipped-90", "flipped-180", "flipped-270"
  };

  return name[self];
}
