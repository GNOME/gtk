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

#ifndef __OTTIE_TRIM_SHAPE_PRIVATE_H__
#define __OTTIE_TRIM_SHAPE_PRIVATE_H__

#include "ottieshapeprivate.h"

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_TRIM_SHAPE         (ottie_trim_shape_get_type ())
#define OTTIE_TRIM_SHAPE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_TRIM_SHAPE, OttieTrimShape))
#define OTTIE_TRIM_SHAPE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_TRIM_SHAPE, OttieTrimShapeClass))
#define OTTIE_IS_TRIM_SHAPE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_TRIM_SHAPE))
#define OTTIE_IS_TRIM_SHAPE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_TRIM_SHAPE))
#define OTTIE_TRIM_SHAPE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_TRIM_SHAPE, OttieTrimShapeClass))

typedef struct _OttieTrimShape OttieTrimShape;
typedef struct _OttieTrimShapeClass OttieTrimShapeClass;

GType                   ottie_trim_shape_get_type               (void) G_GNUC_CONST;

OttieShape *            ottie_trim_shape_parse                  (JsonReader             *reader);

G_END_DECLS

#endif /* __OTTIE_TRIM_SHAPE_PRIVATE_H__ */
