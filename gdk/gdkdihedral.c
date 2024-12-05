
#include "config.h"

#include "gdkdihedralprivate.h"

/*< private >
 * gdk_dihedral_get_mat2:
 * transform: a dihedral
 * @xx: return location for xx component
 * @xy: return location for xy component
 * @yx: return location for yx component
 * @yy: return location for yy component
 *
 * Gets a 2x2 matrix representing the dihedral transform.
 */
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
      {  0.0,  1.0 },
      {  1.0,  0.0 }
    },
    [GDK_DIHEDRAL_FLIPPED_180] = {
      {  1.0,  0.0 },
      {  0.0, -1.0 }
    },
    [GDK_DIHEDRAL_FLIPPED_270] = {
      {  0.0, -1.0 },
      { -1.0,  0.0 }
    },
  };

  *xx = mat[transform][0][0];
  *xy = mat[transform][1][0];
  *yx = mat[transform][0][1];
  *yy = mat[transform][1][1];
}

/*< private >
 * gdk_dihedral_combine:
 * @first: a dihedral transform
 * @second: another dihedral transform
 *
 * Combines two dihedral transforms.
 *
 * Returns: the dihedral transform that applies @first, then @second
 */
GdkDihedral
gdk_dihedral_combine (GdkDihedral first,
                      GdkDihedral second)
{
  return ((second & 4) ^ (first & 4)) |
         ((((second & 3) * (((first & 4) >> 1) + 1)) + first) & 3);
}

/*< private >
 * gdk_dihedral_invert:
 * @self: a dihedral transform
 *
 * Inverts a dihedral transform.
 *
 * Returns: the inverse of @self
 */
GdkDihedral
gdk_dihedral_invert (GdkDihedral self)
{
  return ((4 - self) * (((self & 4) >> 1) + 1) & 3) | (self & 4);
}

/*< private >
 * gdk_dihedral_swaps_xy:
 * @self: a dihedral transform
 *
 * Returns whether the transform exchanges width and height.
 *
 * Returns: true if the transform exchanges width and height
 */
gboolean
gdk_dihedral_swaps_xy (GdkDihedral self)
{
  return (self & 1) ? TRUE : FALSE;
}

/*< private >
 * gdk_dihedral_get_name:
 * @self: a dihedral transform
 *
 * Returns a name for the transform.
 *
 * This is meant for debug messages.
 *
 * Returns: the name of the transform
 */
const char *
gdk_dihedral_get_name (GdkDihedral self)
{
  const char *name[] = {
    "normal", "90", "180", "270", "flipped", "flipped-90", "flipped-180", "flipped-270"
  };

  return name[self];
}
