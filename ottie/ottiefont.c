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

#include "ottiefontprivate.h"

OttieFont *
ottie_font_copy (OttieFont *font)
{
  OttieFont *f;

  f = g_new0 (OttieFont, 1);
  f->name = g_strdup (font->name);
  f->family = g_strdup (font->family);
  f->style = g_strdup (font->style);
  f->ascent = font->ascent;

  return f;
}

void
ottie_font_free (OttieFont *font)
{
  g_free (font->name);
  g_free (font->family);
  g_free (font->style);
  g_free (font);
}
