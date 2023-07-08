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


#ifndef __GSK_SPLINE_PRIVATE_H__
#define __GSK_SPLINE_PRIVATE_H__

#include "gskpath.h"

G_BEGIN_DECLS

typedef gboolean (* GskSplineAddCurveFunc) (const graphene_point_t curve[4],
                                            gpointer               user_data);
gboolean                gsk_spline_decompose_arc                (const graphene_point_t *center,
                                                                 float                   radius,
                                                                 float                   tolerance,
                                                                 float                   start_angle,
                                                                 float                   end_angle,
                                                                 GskSplineAddCurveFunc   curve_func,
                                                                 gpointer                user_data);

G_END_DECLS

#endif /* __GSK_SPLINE_PRIVATE_H__ */

