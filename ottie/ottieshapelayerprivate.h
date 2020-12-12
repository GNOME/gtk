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

#ifndef __OTTIE_SHAPE_LAYER_PRIVATE_H__
#define __OTTIE_SHAPE_LAYER_PRIVATE_H__

#include "ottielayerprivate.h"

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_SHAPE_LAYER         (ottie_shape_layer_get_type ())
#define OTTIE_SHAPE_LAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_SHAPE_LAYER, OttieShapeLayer))
#define OTTIE_SHAPE_LAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_SHAPE_LAYER, OttieShapeLayerClass))
#define OTTIE_IS_SHAPE_LAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_SHAPE_LAYER))
#define OTTIE_IS_SHAPE_LAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_SHAPE_LAYER))
#define OTTIE_SHAPE_LAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_SHAPE_LAYER, OttieShapeLayerClass))

typedef struct _OttieShapeLayer OttieShapeLayer;
typedef struct _OttieShapeLayerClass OttieShapeLayerClass;

GType                   ottie_shape_layer_get_type              (void) G_GNUC_CONST;

OttieShape *            ottie_shape_layer_get_shape             (OttieShapeLayer        *self);

OttieLayer *            ottie_shape_layer_parse                 (JsonReader             *reader);

G_END_DECLS

#endif /* __OTTIE_SHAPE_LAYER_PRIVATE_H__ */
