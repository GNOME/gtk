/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2021 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 */

#ifndef __G_NEWLINE_CONVERTER_H__
#define __G_NEWLINE_CONVERTER_H__

#if 0
#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/gconverter.h>
#else
#include <gio/gio.h>
#endif

G_BEGIN_DECLS

#define G_TYPE_NEWLINE_CONVERTER         (g_newline_converter_get_type ())
#define G_NEWLINE_CONVERTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NEWLINE_CONVERTER, GNewlineConverter))
#define G_NEWLINE_CONVERTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_NEWLINE_CONVERTER, GNewlineConverterClass))
#define G_IS_NEWLINE_CONVERTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NEWLINE_CONVERTER))
#define G_IS_NEWLINE_CONVERTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_NEWLINE_CONVERTER))
#define G_NEWLINE_CONVERTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_NEWLINE_CONVERTER, GNewlineConverterClass))

typedef struct _GNewlineConverter   GNewlineConverter;
typedef struct _GNewlineConverterClass   GNewlineConverterClass;

struct _GNewlineConverterClass
{
  GObjectClass parent_class;
};

GLIB_AVAILABLE_IN_ALL
GType                   g_newline_converter_get_type            (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_ALL
GNewlineConverter *     g_newline_converter_new                 (GDataStreamNewlineType to_newline,
						                 GDataStreamNewlineType from_newline);

G_END_DECLS

#endif /* __G_NEWLINE_CONVERTER_H__ */
