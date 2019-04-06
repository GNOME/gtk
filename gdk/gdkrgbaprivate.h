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


gboolean               gdk_rgba_parser_parse                    (GtkCssParser           *parser,
                                                                 GdkRGBA                *rgba);

G_END_DECLS

#endif
