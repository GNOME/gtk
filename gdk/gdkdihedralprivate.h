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
 * @GDK_DIHEDRAL_NORMAL: Identity
 * @GDK_DIHEDRAL_90: Rotation by 90°
 * @GDK_DIHEDRAL_180: Rotation by 180°
 * @GDK_DIHEDRAL_270: Rotation by 270°
 * @GDK_DIHEDRAL_FLIPPED: Horizontal flip
 * @GDK_DIHEDRAL_FLIPPED_90: Horizontal flip, followed by 90° rotation
 * @GDK_DIHEDRAL_FLIPPED_180: Horizontal flip, followed by 180° rotation
 * @GDK_DIHEDRAL_FLIPPED_270: Horizontal flip, followed by 270° rotation
 *
 * The transforms that make up the symmetry group of the square,
 * also known as D₄.
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

