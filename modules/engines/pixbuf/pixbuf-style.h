/* GTK+ Pixbuf Engine
 * Copyright (C) 1998-2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by Owen Taylor <otaylor@redhat.com>, based on code by
 * Carsten Haitzler <raster@rasterman.com>
 */

#include <gtk/gtk.h>

typedef struct _PixbufStyle PixbufStyle;
typedef struct _PixbufStyleClass PixbufStyleClass;

extern G_GNUC_INTERNAL GType pixbuf_type_style;

#define PIXBUF_TYPE_STYLE              pixbuf_type_style
#define PIXBUF_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), PIXBUF_TYPE_STYLE, PixbufStyle))
#define PIXBUF_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), PIXBUF_TYPE_STYLE, PixbufStyleClass))
#define PIXBUF_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), PIXBUF_TYPE_STYLE))
#define PIXBUF_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), PIXBUF_TYPE_STYLE))
#define PIXBUF_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), PIXBUF_TYPE_STYLE, PixbufStyleClass))

struct _PixbufStyle
{
  GtkStyle parent_instance;
};

struct _PixbufStyleClass
{
  GtkStyleClass parent_class;
};

G_GNUC_INTERNAL void pixbuf_style_register_type (GTypeModule *module);


