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

#ifndef __OTTIE_DOUBLE_VALUE_PRIVATE_H__
#define __OTTIE_DOUBLE_VALUE_PRIVATE_H__

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

typedef struct _OttieDoubleValue OttieDoubleValue;

struct _OttieDoubleValue
{
  gboolean is_static;
  union {
    double static_value;
    gpointer keyframes;
  };
};

void                      ottie_double_value_init               (OttieDoubleValue       *self,
                                                                 double                  value);
void                      ottie_double_value_clear              (OttieDoubleValue       *self);

static inline gboolean    ottie_double_value_is_static          (OttieDoubleValue       *self);
double                    ottie_double_value_get                (OttieDoubleValue       *self,
                                                                 double                  timestamp);

gboolean                  ottie_double_value_parse              (JsonReader             *reader,
                                                                 gsize                   offset,
                                                                 gpointer                data);

static inline gboolean
ottie_double_value_is_static (OttieDoubleValue *self)
{
  return self->is_static;
}

G_END_DECLS

#endif /* __OTTIE_DOUBLE_VALUE_PRIVATE_H__ */
