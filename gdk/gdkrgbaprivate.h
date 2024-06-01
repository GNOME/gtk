/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010, Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkrgba.h"

#include <gtk/css/gtkcss.h>

#include "gtk/css/gtkcssparserprivate.h"

#define _GDK_RGBA_DECODE(c) ((unsigned)(((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : \
                                        ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) : \
                                        ((c) >= '0' && (c) <= '9') ? ((c)-'0') : \
                                        -1))
#define _GDK_RGBA_SELECT_COLOR(_str, index3, index6) (sizeof(_str) <= 4 ? _GDK_RGBA_DECODE ((_str)[index3]) : _GDK_RGBA_DECODE ((_str)[index6]))
#define GDK_RGBA(str) ((GdkRGBA) {\
    ((_GDK_RGBA_SELECT_COLOR(str, 0, 0) << 4) | _GDK_RGBA_SELECT_COLOR(str, 0, 1)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 1, 2) << 4) | _GDK_RGBA_SELECT_COLOR(str, 1, 3)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 2, 4) << 4) | _GDK_RGBA_SELECT_COLOR(str, 2, 5)) / 255., \
    ((sizeof(str) % 4 == 1) ? ((_GDK_RGBA_SELECT_COLOR(str, 3, 6) << 4) | _GDK_RGBA_SELECT_COLOR(str, 3, 7)) : 0xFF) / 255. })

#define GDK_RGBA_INIT_ALPHA(rgba,opacity) ((GdkRGBA) { (rgba)->red, (rgba)->green, (rgba)->blue, (rgba)->alpha * (opacity) })

#define GDK_RGBA_TRANSPARENT ((GdkRGBA) { 0, 0, 0, 0 })
#define GDK_RGBA_BLACK ((GdkRGBA) { 0, 0, 0, 1 })
#define GDK_RGBA_WHITE ((GdkRGBA) { 1, 1, 1, 1 })

gboolean               gdk_rgba_parser_parse                    (GtkCssParser           *parser,
                                                                 GdkRGBA                *rgba);

#define gdk_rgba_is_clear(rgba) _gdk_rgba_is_clear (rgba)
#define gdk_rgba_is_opaque(rgba) _gdk_rgba_is_opaque (rgba)
#define gdk_rgba_equal(p1, p2) _gdk_rgba_equal (p1, p2)

static inline gboolean
_gdk_rgba_is_clear (const GdkRGBA *rgba)
{
  return rgba->alpha < ((float) 0x00ff / (float) 0xffff);
}

static inline gboolean
_gdk_rgba_is_opaque (const GdkRGBA *rgba)
{
  return rgba->alpha > ((float)0xff00 / (float)0xffff);
}

static inline gboolean
_gdk_rgba_equal (gconstpointer p1,
                 gconstpointer p2)
{
  const GdkRGBA *rgba1 = p1;
  const GdkRGBA *rgba2 = p2;

  return rgba1->red == rgba2->red &&
         rgba1->green == rgba2->green &&
         rgba1->blue == rgba2->blue &&
         rgba1->alpha == rgba2->alpha;
}

GString * gdk_rgba_print (const GdkRGBA *rgba,
                          GString       *string);

G_END_DECLS

