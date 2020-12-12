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

#ifndef __OTTIE_SHAPE_PRIVATE_H__
#define __OTTIE_SHAPE_PRIVATE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_SHAPE         (ottie_shape_get_type ())
#define OTTIE_SHAPE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_SHAPE, OttieShape))
#define OTTIE_SHAPE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_SHAPE, OttieShapeClass))
#define OTTIE_IS_SHAPE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_SHAPE))
#define OTTIE_IS_SHAPE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_SHAPE))
#define OTTIE_SHAPE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_SHAPE, OttieShapeClass))

typedef struct _OttieShapeSnapshot OttieShapeSnapshot;
typedef struct _OttieShape OttieShape;
typedef struct _OttieShapeClass OttieShapeClass;

struct _OttieShapeSnapshot
{
  GSList *paths;
  GskPath *cached_path;
};

struct _OttieShape
{
  GObject parent;

  char *name;
  char *match_name;
  gboolean hidden;
};

struct _OttieShapeClass
{
  GObjectClass parent_class;

  void                  (* snapshot)                           (OttieShape              *self,
                                                                GtkSnapshot             *snapshot,
                                                                OttieShapeSnapshot      *snapshot_data,
                                                                double                   timestamp);
};

GType                   ottie_shape_get_type                   (void) G_GNUC_CONST;

void                    ottie_shape_snapshot                   (OttieShape              *self,
                                                                GtkSnapshot             *snapshot,
                                                                OttieShapeSnapshot      *snapshot_data,
                                                                double                   timestamp);


void                    ottie_shape_snapshot_init               (OttieShapeSnapshot     *data,
                                                                 OttieShapeSnapshot     *copy_from);
void                    ottie_shape_snapshot_clear              (OttieShapeSnapshot     *data);
void                    ottie_shape_snapshot_add_path           (OttieShapeSnapshot     *data,
                                                                 GskPath                *path);
GskPath *               ottie_shape_snapshot_get_path           (OttieShapeSnapshot     *data);

#define OTTIE_PARSE_OPTIONS_SHAPE \
    { "nm", ottie_parser_option_string, G_STRUCT_OFFSET (OttieShape, name) }, \
    { "mn", ottie_parser_option_string, G_STRUCT_OFFSET (OttieShape, match_name) }, \
    { "hd", ottie_parser_option_boolean, G_STRUCT_OFFSET (OttieShape, hidden) }, \
    { "ix", ottie_parser_option_skip_index, 0 }, \
    { "ty", ottie_parser_option_skip, 0 },

G_END_DECLS

#endif /* __OTTIE_SHAPE_PRIVATE_H__ */
