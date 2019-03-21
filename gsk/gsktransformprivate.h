/*
 * Copyright © 2019 Benjamin Otte
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


#ifndef __GSK_TRANSFORM_PRIVATE_H__
#define __GSK_TRANSFORM_PRIVATE_H__

#include "gsktransform.h"

#include "gsk/gskrendernodeprivate.h"

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssparserprivate.h"

G_BEGIN_DECLS

GskTransform *          gsk_transform_matrix_with_category      (GskTransform           *next,
                                                                 const graphene_matrix_t*matrix,
                                                                 GskTransformCategory    category);

gboolean                gsk_transform_parser_parse              (GtkCssParser           *parser,
                                                                 GskTransform          **out_transform);


G_END_DECLS

#endif /* __GSK_TRANSFORM_PRIVATE_H__ */

