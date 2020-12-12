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

#ifndef __OTTIE_OBJECT_PRIVATE_H__
#define __OTTIE_OBJECT_PRIVATE_H__

#include <glib-object.h>

#include "ottie/ottierenderprivate.h"

G_BEGIN_DECLS

#define OTTIE_TYPE_OBJECT         (ottie_object_get_type ())
#define OTTIE_OBJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_OBJECT, OttieObject))
#define OTTIE_OBJECT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_OBJECT, OttieObjectClass))
#define OTTIE_IS_OBJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_OBJECT))
#define OTTIE_IS_OBJECT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_OBJECT))
#define OTTIE_OBJECT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_OBJECT, OttieObjectClass))

typedef struct _OttieObject OttieObject;
typedef struct _OttieObjectClass OttieObjectClass;

struct _OttieObject
{
  GObject parent;

  char *name;
  char *match_name;
};

struct _OttieObjectClass
{
  GObjectClass parent_class;
};

GType                   ottie_object_get_type                   (void) G_GNUC_CONST;

void                    ottie_object_set_name                   (OttieObject            *self,
                                                                 const char             *name);
const char *            ottie_object_get_name                   (OttieObject            *self);

void                    ottie_object_set_match_name             (OttieObject            *self,
                                                                 const char             *match_name);
const char *            ottie_object_get_match_name             (OttieObject            *self);

#define OTTIE_PARSE_OPTIONS_OBJECT \
    { "nm", ottie_parser_option_string, G_STRUCT_OFFSET (OttieObject, name) }, \
    { "mn", ottie_parser_option_string, G_STRUCT_OFFSET (OttieObject, match_name) }

G_END_DECLS

#endif /* __OTTIE_OBJECT_PRIVATE_H__ */
