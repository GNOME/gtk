/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*< private >
 * GdkDihedral:
 * @GDK_DIHEDRAL_NORMAL: Identity,
 *   equivalent to the CSS transform "none"
 * @GDK_DIHEDRAL_90: Clockwise rotation by 90°,
 *   equivalent to the CSS transform "rotate(90deg)"
 * @GDK_DIHEDRAL_180: Clockwise rotation by 180°,
 *   equivalent to the CSS transform "rotate(180deg)"
 * @GDK_DIHEDRAL_270: Clockwise rotation by 270°,
 *   equivalent to the CSS transform "rotate(270deg)"
 * @GDK_DIHEDRAL_FLIPPED: Horizontal flip,
 *   equivalent to the CSS transform "scale(-1, 1)"
 * @GDK_DIHEDRAL_FLIPPED_90: Clockwise 90° rotation, followed by a horizontal flip,
 *   equivalent to the CSS transform "rotate(90deg) scale(-1, 1)"
 * @GDK_DIHEDRAL_FLIPPED_180: Clockwise 180° rotation, followed by a horizontal flip,
 *   equivalent to the CSS transform "rotate(180deg) scale(-1, 1)"
 * @GDK_DIHEDRAL_FLIPPED_270: Clockwise 270° rotation, followed by a horizontal flip,
 *   equivalent to the CSS transform "rotate(270deg) scale(-1, 1)"
 *
 * The transforms that make up the symmetry group of the square,
 * also known as D₄.
 *
 * Note that this enumeration is intentionally set up to encode the *inverses*
 * of the corresponding wl_output_transform values. E.g. `WL_OUTPUT_TRANSFORM_FLIPPED_90`
 * is defined as a horizontal flip, followed by a 90° counterclockwise rotation, which
 * is the inverse of @GDK_DIHEDRAL_FLIPPED_90.
 */
typedef enum {
  GDK_DIHEDRAL_NORMAL,
  GDK_DIHEDRAL_90,
  GDK_DIHEDRAL_180,
  GDK_DIHEDRAL_270,
  GDK_DIHEDRAL_FLIPPED,
  GDK_DIHEDRAL_FLIPPED_90,
  GDK_DIHEDRAL_FLIPPED_180,
  GDK_DIHEDRAL_FLIPPED_270,
} GdkDihedral;

void            gdk_dihedral_get_mat2                           (GdkDihedral            transform,
                                                                 float                 *xx,
                                                                 float                 *xy,
                                                                 float                 *yx,
                                                                 float                 *yy);

GdkDihedral     gdk_dihedral_combine                            (GdkDihedral            first,
                                                                 GdkDihedral            second);
GdkDihedral     gdk_dihedral_invert                             (GdkDihedral            self);
gboolean        gdk_dihedral_swaps_xy                           (GdkDihedral            self);

const char *    gdk_dihedral_get_name                           (GdkDihedral            self);

G_END_DECLS

