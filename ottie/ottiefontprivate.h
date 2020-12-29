
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

#ifndef __OTTIE_FONT_PRIVATE_H__
#define __OTTIE_FONT_PRIVATE_H__

#include <json-glib/json-glib.h>

#include "ottieprinterprivate.h"
#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct
{
  char *name;
  char *family;
  char *style;
  double ascent;
} OttieFont;

OttieFont * ottie_font_copy (OttieFont *font);
void        ottie_font_free (OttieFont *font);


#endif /* __OTTIE_FONT_PRIVATE_H__ */
