/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#ifndef __OTTIE_TEXT_VALUE_PRIVATE_H__
#define __OTTIE_TEXT_VALUE_PRIVATE_H__

#include <json-glib/json-glib.h>
#include "ottie/ottieparserprivate.h"
#include "ottie/ottieprinterprivate.h"

G_BEGIN_DECLS

typedef struct _OttieTextItem OttieTextItem;

struct _OttieTextItem
{
  const char *font;
  const char *text;
  GdkRGBA color;
  double size;
  OttieTextJustify justify;
  double line_height;
  double line_shift;
  double tracking;
};

typedef struct _OttieTextValue OttieTextValue;

struct _OttieTextValue
{
  gboolean is_static;
  union {
    OttieTextItem static_value;
    gpointer keyframes;
  };
};

void                      ottie_text_value_init               (OttieTextValue       *self,
                                                               const OttieTextItem  *item);

void                      ottie_text_value_clear              (OttieTextValue       *self);

static inline gboolean    ottie_text_value_is_static          (OttieTextValue       *self);
void                      ottie_text_value_get                (OttieTextValue       *self,
                                                               double                timestamp,
                                                               OttieTextItem        *item);

gboolean                  ottie_text_value_parse              (JsonReader           *reader,
                                                               gsize                 offset,
                                                               gpointer              data);

void                      ottie_text_value_print              (OttieTextValue       *self,
                                                               const char           *name,
                                                               OttiePrinter         *printer);

static inline gboolean
ottie_text_value_is_static (OttieTextValue *self)
{
  return self->is_static;
}

G_END_DECLS

#endif /* __OTTIE_TEXT_VALUE_PRIVATE_H__ */
