/* gtkcellrenderertext.h
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

#ifndef __GTK_CELL_RENDERER_TEXT_H__
#define __GTK_CELL_RENDERER_TEXT_H__

#include <pango/pango.h>
#include <gtk/gtkcellrenderer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_CELL_RENDERER_TEXT		(gtk_cell_renderer_text_get_type ())
#define GTK_CELL_RENDERER_TEXT(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_CELL_RENDERER_TEXT, GtkCellRendererText))
#define GTK_CELL_RENDERER_TEXT_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_TEXT, GtkCellRendererTextClass))
#define GTK_IS_CELL_RENDERER_TEXT(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TEXT))
#define GTK_IS_CELL_RENDERER_TEXT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TEXT))

typedef struct _GtkCellRendererText      GtkCellRendererText;
typedef struct _GtkCellRendererTextClass GtkCellRendererTextClass;

struct _GtkCellRendererText
{
  GtkCellRenderer parent;

  /*< private >*/
  gchar *text;
  PangoAttrList *attr_list;

  guint editable  : 1;
  guint underline : 1;
};

struct _GtkCellRendererTextClass
{
  GtkCellRendererClass parent_class;
};

GtkType          gtk_cell_renderer_text_get_type (void);
GtkCellRenderer *gtk_cell_renderer_text_new      (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CELL_RENDERER_TEXT_H__ */
