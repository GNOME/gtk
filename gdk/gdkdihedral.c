
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
  GdkDihedral combine[8][8] = {
    { GDK_DIHEDRAL_NORMAL,       GDK_DIHEDRAL_90,          GDK_DIHEDRAL_180,         GDK_DIHEDRAL_270,         GDK_DIHEDRAL_FLIPPED,     GDK_DIHEDRAL_FLIPPED_90,  GDK_DIHEDRAL_FLIPPED_180, GDK_DIHEDRAL_FLIPPED_270 },
    { GDK_DIHEDRAL_90,           GDK_DIHEDRAL_180,         GDK_DIHEDRAL_270,         GDK_DIHEDRAL_NORMAL,      GDK_DIHEDRAL_FLIPPED_270, GDK_DIHEDRAL_FLIPPED,     GDK_DIHEDRAL_FLIPPED_90,  GDK_DIHEDRAL_FLIPPED_180 },
    { GDK_DIHEDRAL_180,          GDK_DIHEDRAL_270,         GDK_DIHEDRAL_NORMAL,      GDK_DIHEDRAL_90,          GDK_DIHEDRAL_FLIPPED_180, GDK_DIHEDRAL_180,         GDK_DIHEDRAL_FLIPPED,     GDK_DIHEDRAL_FLIPPED_90 },
    { GDK_DIHEDRAL_270,          GDK_DIHEDRAL_NORMAL,      GDK_DIHEDRAL_90,          GDK_DIHEDRAL_180,         GDK_DIHEDRAL_FLIPPED_90,  GDK_DIHEDRAL_FLIPPED_180, GDK_DIHEDRAL_FLIPPED_270, GDK_DIHEDRAL_FLIPPED },
    { GDK_DIHEDRAL_FLIPPED,      GDK_DIHEDRAL_FLIPPED_90,  GDK_DIHEDRAL_FLIPPED_180, GDK_DIHEDRAL_FLIPPED_270, GDK_DIHEDRAL_NORMAL,      GDK_DIHEDRAL_90,          GDK_DIHEDRAL_180,         GDK_DIHEDRAL_270 },
    { GDK_DIHEDRAL_FLIPPED_90,   GDK_DIHEDRAL_FLIPPED_180, GDK_DIHEDRAL_FLIPPED_270, GDK_DIHEDRAL_FLIPPED,     GDK_DIHEDRAL_270,         GDK_DIHEDRAL_NORMAL,      GDK_DIHEDRAL_90,          GDK_DIHEDRAL_180 },
    { GDK_DIHEDRAL_FLIPPED_180,  GDK_DIHEDRAL_FLIPPED_270, GDK_DIHEDRAL_FLIPPED,     GDK_DIHEDRAL_FLIPPED_90,  GDK_DIHEDRAL_180,         GDK_DIHEDRAL_270,         GDK_DIHEDRAL_NORMAL,      GDK_DIHEDRAL_90 },
    { GDK_DIHEDRAL_FLIPPED_270,  GDK_DIHEDRAL_FLIPPED,     GDK_DIHEDRAL_FLIPPED_90,  GDK_DIHEDRAL_FLIPPED_180, GDK_DIHEDRAL_90,          GDK_DIHEDRAL_180,         GDK_DIHEDRAL_270,         GDK_DIHEDRAL_NORMAL },
  };

  return combine[first][second];
}

GdkDihedral
gdk_dihedral_invert (GdkDihedral self)
{
  GdkDihedral inverse[] = {
    GDK_DIHEDRAL_NORMAL,
    GDK_DIHEDRAL_270,
    GDK_DIHEDRAL_180,
    GDK_DIHEDRAL_90,
    GDK_DIHEDRAL_FLIPPED,
    GDK_DIHEDRAL_FLIPPED_90,
    GDK_DIHEDRAL_FLIPPED_180,
    GDK_DIHEDRAL_FLIPPED_270,
  };

  return inverse[self];
}

gboolean
gdk_dihedral_flips_xy (GdkDihedral dihedral)
{
  switch (dihedral)
    {
    case GDK_DIHEDRAL_90:
    case GDK_DIHEDRAL_270:
    case GDK_DIHEDRAL_FLIPPED_90:
    case GDK_DIHEDRAL_FLIPPED_270:
      return TRUE;
    case GDK_DIHEDRAL_NORMAL:
    case GDK_DIHEDRAL_180:
    case GDK_DIHEDRAL_FLIPPED:
    case GDK_DIHEDRAL_FLIPPED_180:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

