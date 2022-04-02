/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#ifndef __GSK_PATH_PRIVATE_H__
#define __GSK_PATH_PRIVATE_H__

#include "gskpath.h"

#include "gskcontourprivate.h"
#include "gskpathopprivate.h"

G_BEGIN_DECLS

/* Same as Skia, so looks like a good value. ¯\_(ツ)_/¯ */
#define GSK_PATH_TOLERANCE_DEFAULT (0.5)

GskPath *               gsk_path_new_from_contours              (const GSList           *contours);

gsize                   gsk_path_get_n_contours                 (GskPath                *path);
const GskContour *      gsk_path_get_contour                    (GskPath                *path,
                                                                 gsize                   i);

GskPathFlags            gsk_path_get_flags                      (GskPath                *self);

gboolean                gsk_path_foreach_with_tolerance         (GskPath                *self,
                                                                 GskPathForeachFlags     flags,
                                                                 double                  tolerance,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);


void                    gsk_path_builder_add_contour            (GskPathBuilder         *builder,
                                                                 GskContour             *contour);

void                    gsk_path_builder_svg_arc_to             (GskPathBuilder         *builder,
                                                                 float                   rx,
                                                                 float                   ry,
                                                                 float                   x_axis_rotation,
                                                                 gboolean                large_arc,
                                                                 gboolean                positive_sweep,
                                                                 float                   x,
                                                                 float                   y);

G_END_DECLS

#endif /* __GSK_PATH_PRIVATE_H__ */

