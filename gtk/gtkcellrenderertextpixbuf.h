/* gtkcellrenderertextpixbuf.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
 */

#ifndef __GTK_CELL_RENDERER_TEXT_PIXBUF_H__
#define __GTK_CELL_RENDERER_TEXT_PIXBUF_H__

#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_CELL_RENDERER_TEXT_PIXBUF		(gtk_cell_renderer_text_pixbuf_get_type ())
#define GTK_CELL_RENDERER_TEXT_PIXBUF(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_CELL_RENDERER_TEXT_PIXBUF, GtkCellRendererTextPixbuf))
#define GTK_CELL_RENDERER_TEXT_PIXBUF_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_TEXT_PIXBUF, GtkCellRendererTextPixbufClass))
#define GTK_IS_CELL_RENDERER_TEXT_PIXBUF(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TEXT_PIXBUF))
#define GTK_IS_CELL_RENDERER_TEXT_PIXBUF_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TEXT_PIXBUF))

typedef struct _GtkCellRendererTextPixbuf GtkCellRendererTextPixbuf;
typedef struct _GtkCellRendererTextPixbufClass GtkCellRendererTextPixbufClass;

struct _GtkCellRendererTextPixbuf
{
  GtkCellRendererText parent;

  /*< private >*/
  GtkCellRendererPixbuf *pixbuf;
  GtkPositionType pixbuf_pos;
};

struct _GtkCellRendererTextPixbufClass
{
  GtkCellRendererTextClass parent_class;
};

GtkType          gtk_cell_renderer_text_pixbuf_get_type (void);
GtkCellRenderer *gtk_cell_renderer_text_pixbuf_new      (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CELL_RENDERER_TEXT_PIXBUF_H__ */
