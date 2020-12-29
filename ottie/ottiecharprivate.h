
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

#ifndef __OTTIE_CHAR_PRIVATE_H__
#define __OTTIE_CHAR_PRIVATE_H__

#include <glib.h>

#include "ottieshapeprivate.h"

G_BEGIN_DECLS

typedef struct
{
  char *ch;
  char *family;
  char *style;
} OttieCharKey;

typedef struct
{
  OttieCharKey key;
  double size;
  double width;
  OttieShape *shapes;
} OttieChar;

void        ottie_char_key_free  (OttieCharKey       *key);
guint       ottie_char_key_hash  (const OttieCharKey *key);
gboolean    ottie_char_key_equal (const OttieCharKey *key1,
                                  const OttieCharKey *key2);
OttieChar * ottie_char_copy      (OttieChar          *ch);
void        ottie_char_clear     (OttieChar          *ch);
void        ottie_char_free      (OttieChar          *ch);

G_END_DECLS

#endif /* __OTTIE_CHAR_PRIVATE_H__ */
