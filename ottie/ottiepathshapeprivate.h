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

#ifndef __OTTIE_PATH_SHAPE_PRIVATE_H__
#define __OTTIE_PATH_SHAPE_PRIVATE_H__

#include "ottieshapeprivate.h"

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_PATH_SHAPE         (ottie_path_shape_get_type ())
#define OTTIE_PATH_SHAPE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_PATH_SHAPE, OttiePathShape))
#define OTTIE_PATH_SHAPE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_PATH_SHAPE, OttiePathShapeClass))
#define OTTIE_IS_PATH_SHAPE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_PATH_SHAPE))
#define OTTIE_IS_PATH_SHAPE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_PATH_SHAPE))
#define OTTIE_PATH_SHAPE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_PATH_SHAPE, OttiePathShapeClass))

typedef struct _OttiePathShape OttiePathShape;
typedef struct _OttiePathShapeClass OttiePathShapeClass;

GType                   ottie_path_shape_get_type               (void) G_GNUC_CONST;

OttieShape *            ottie_path_shape_parse                  (JsonReader             *reader);

G_END_DECLS

#endif /* __OTTIE_PATH_SHAPE_PRIVATE_H__ */
