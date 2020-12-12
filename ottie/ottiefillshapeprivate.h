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

#ifndef __OTTIE_FILL_SHAPE_PRIVATE_H__
#define __OTTIE_FILL_SHAPE_PRIVATE_H__

#include "ottieshapeprivate.h"

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_FILL_SHAPE         (ottie_fill_shape_get_type ())
#define OTTIE_FILL_SHAPE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_FILL_SHAPE, OttieFillShape))
#define OTTIE_FILL_SHAPE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_FILL_SHAPE, OttieFillShapeClass))
#define OTTIE_IS_FILL_SHAPE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_FILL_SHAPE))
#define OTTIE_IS_FILL_SHAPE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_FILL_SHAPE))
#define OTTIE_FILL_SHAPE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_FILL_SHAPE, OttieFillShapeClass))

typedef struct _OttieFillShape OttieFillShape;
typedef struct _OttieFillShapeClass OttieFillShapeClass;

GType                   ottie_fill_shape_get_type               (void) G_GNUC_CONST;

OttieShape *            ottie_fill_shape_parse                  (JsonReader             *reader);

G_END_DECLS

#endif /* __OTTIE_FILL_SHAPE_PRIVATE_H__ */
