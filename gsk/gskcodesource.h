/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#ifndef __GSK_CODE_SOURCE_H__
#define __GSK_CODE_SOURCE_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_CODE_SOURCE (gsk_code_source_get_type ())

G_DECLARE_FINAL_TYPE (GskCodeSource, gsk_code_source, GSK, CODE_SOURCE, GObject)

GDK_AVAILABLE_IN_3_92
const char *            gsk_code_source_get_name                (GskCodeSource       *source);
GDK_AVAILABLE_IN_3_92
GFile *                 gsk_code_source_get_file                (GskCodeSource       *source);

G_END_DECLS

#endif /* __GSK_CODE_SOURCE_H__ */
