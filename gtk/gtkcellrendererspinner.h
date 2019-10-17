/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2009 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2009 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CELL_RENDERER_SPINNER_H__
#define __GTK_CELL_RENDERER_SPINNER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcellrenderer.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_RENDERER_SPINNER            (gtk_cell_renderer_spinner_get_type ())
#define GTK_CELL_RENDERER_SPINNER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_RENDERER_SPINNER, GtkCellRendererSpinner))
#define GTK_IS_CELL_RENDERER_SPINNER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_RENDERER_SPINNER))

typedef struct _GtkCellRendererSpinner        GtkCellRendererSpinner;

GDK_AVAILABLE_IN_ALL
GType            gtk_cell_renderer_spinner_get_type (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkCellRenderer *gtk_cell_renderer_spinner_new      (void);

G_END_DECLS

#endif /* __GTK_CELL_RENDERER_SPINNER_H__ */
