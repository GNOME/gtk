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

#ifndef __GDK_RGBA_PRIVATE_H__
#define __GDK_RGBA_PRIVATE_H__

#include "gdkrgba.h"

#include <gtk/css/gtkcss.h>

#include "gtk/css/gtkcssparserprivate.h"

#define _GDK_RGBA_DECODE(c) ((unsigned)(((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : \
                                        ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) : \
                                        ((c) >= '0' && (c) <= '9') ? ((c)-'0') : \
                                        -1))
#define _GDK_RGBA_SELECT_COLOR(_str, index3, index6) _GDK_RGBA_DECODE (sizeof(_str) <= 4 ? (_str)[index3] : (_str)[index6])
#define GDK_RGBA(str) ((GdkRGBA) {\
    ((_GDK_RGBA_SELECT_COLOR(str, 0, 0) << 4) | _GDK_RGBA_SELECT_COLOR(str, 0, 1)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 1, 2) << 4) | _GDK_RGBA_SELECT_COLOR(str, 1, 3)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 2, 4) << 4) | _GDK_RGBA_SELECT_COLOR(str, 2, 5)) / 255., \
    ((sizeof(str) % 4 == 1) ? ((_GDK_RGBA_SELECT_COLOR(str, 3, 6) << 4) | _GDK_RGBA_SELECT_COLOR(str, 3, 7)) : 0xFF) / 255 })


gboolean               gdk_rgba_parser_parse                    (GtkCssParser           *parser,
                                                                 GdkRGBA                *rgba);

G_END_DECLS

#endif
