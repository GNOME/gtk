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

#ifndef __OTTIE_LAYER_PRIVATE_H__
#define __OTTIE_LAYER_PRIVATE_H__

#include "ottie/ottietransformprivate.h"
#include "ottie/ottieobjectprivate.h"
#include "ottie/ottieparserprivate.h"
#include "ottie/ottierenderprivate.h"

G_BEGIN_DECLS

#define OTTIE_TYPE_LAYER         (ottie_layer_get_type ())
#define OTTIE_LAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_LAYER, OttieLayer))
#define OTTIE_LAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_LAYER, OttieLayerClass))
#define OTTIE_IS_LAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_LAYER))
#define OTTIE_IS_LAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_LAYER))
#define OTTIE_LAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_LAYER, OttieLayerClass))

typedef struct _OttieLayer OttieLayer;
typedef struct _OttieLayerClass OttieLayerClass;

struct _OttieLayer
{
  OttieObject parent;

  OttieTransform *transform;
  gboolean auto_orient;
  GskBlendMode blend_mode;
  int index;
  int parent_index;
  char *layer_name;
  double start_frame;
  double end_frame;
  double start_time;
  double stretch;
};

struct _OttieLayerClass
{
  OttieObjectClass parent_class;

  void                  (* update)                           (OttieLayer                *layer,
                                                              GHashTable                *compositions);
  void                  (* render)                           (OttieLayer                *layer,
                                                              OttieRender               *render,
                                                              double                     timestamp);
};

GType                   ottie_layer_get_type                 (void) G_GNUC_CONST;

void                    ottie_layer_update                   (OttieLayer                *self,
                                                              GHashTable                *compositions);
void                    ottie_layer_render                   (OttieLayer                *self,
                                                              OttieRender               *render,
                                                              double                     timestamp);

#define OTTIE_PARSE_OPTIONS_LAYER \
    OTTIE_PARSE_OPTIONS_OBJECT, \
    { "ao", ottie_parser_option_boolean, G_STRUCT_OFFSET (OttieLayer, auto_orient) }, \
    { "bm", ottie_parser_option_blend_mode, G_STRUCT_OFFSET (OttieLayer, blend_mode) }, \
    { "ln", ottie_parser_option_string, G_STRUCT_OFFSET (OttieLayer, layer_name) }, \
    { "ks", ottie_parser_option_transform, G_STRUCT_OFFSET (OttieLayer, transform) }, \
    { "ip", ottie_parser_option_double, G_STRUCT_OFFSET (OttieLayer, start_frame) }, \
    { "ind", ottie_parser_option_int, G_STRUCT_OFFSET (OttieLayer, index) }, \
    { "parent", ottie_parser_option_int, G_STRUCT_OFFSET (OttieLayer, parent_index) }, \
    { "op", ottie_parser_option_double, G_STRUCT_OFFSET (OttieLayer, end_frame) }, \
    { "st", ottie_parser_option_double, G_STRUCT_OFFSET (OttieLayer, start_time) }, \
    { "sr", ottie_parser_option_double, G_STRUCT_OFFSET (OttieLayer, stretch) }, \
    { "ddd", ottie_parser_option_3d, 0 }, \
    { "ix",  ottie_parser_option_skip_index, 0 }, \
    { "ty", ottie_parser_option_skip, 0 }

G_END_DECLS

#endif /* __OTTIE_LAYER_PRIVATE_H__ */
