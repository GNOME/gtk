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


#ifndef __GSK_PATH_DASH_PRIVATE_H__
#define __GSK_PATH_DASH_PRIVATE_H__

#include <gsk/gskpath.h>

G_BEGIN_DECLS

GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_dash                           (GskPath                *path,
                                                                 GskStroke              *stroke,
                                                                 float                   tolerance,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);

#ifdef GTK_COMPILATION

#include "gskcontourprivate.h"

gboolean                gsk_contour_dash                        (const GskContour       *contour,
                                                                 GskStroke              *stroke,
                                                                 float                   tolerance,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);

#endif /* GTK_COMPILATION */

G_END_DECLS

#endif /* __GSK_PATH_DASH_PRIVATE_H__ */

