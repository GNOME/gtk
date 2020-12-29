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

#include "ottiecharprivate.h"

static void
ottie_char_key_clear (OttieCharKey *key)
{
  g_free (key->ch);
  g_free (key->family);
  g_free (key->style);
}

void
ottie_char_key_free (OttieCharKey *key)
{
  ottie_char_key_clear (key);
  g_free (key);
}

guint
ottie_char_key_hash (const OttieCharKey *key)
{
  guint res = 0;

  res = 31 * res + g_str_hash (key->ch);
  res = 31 * res + g_str_hash (key->family);
  res = 31 * res + g_str_hash (key->style);

  return res;
}

gboolean
ottie_char_key_equal (const OttieCharKey *key1,
                      const OttieCharKey *key2)
{
  return strcmp (key1->ch, key2->ch) == 0 &&
         strcmp (key1->family, key2->family) == 0 &&
         strcmp (key1->style, key2->style) == 0;
}

OttieChar *
ottie_char_copy (OttieChar *ch)
{
  OttieChar *c;

  c = g_new0 (OttieChar, 1);
  c->key.ch = g_strdup (ch->key.ch);
  c->key.family = g_strdup (ch->key.family);
  c->key.style = g_strdup (ch->key.style);
  c->size = ch->size;
  c->width = ch->width;
  c->shapes = g_object_ref (ch->shapes);

  return c;
}

void
ottie_char_clear (OttieChar *ch)
{
  ottie_char_key_clear (&ch->key);
  g_object_unref (ch->shapes);
}

void
ottie_char_free (OttieChar *ch)
{
  ottie_char_clear (ch);
  g_free (ch);
}
