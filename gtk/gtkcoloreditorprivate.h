/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#ifndef __GTK_COLOR_EDITOR_H__
#define __GTK_COLOR_EDITOR_H__

#include <gtk/gtkbox.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_EDITOR            (gtk_color_editor_get_type ())
#define GTK_COLOR_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_EDITOR, GtkColorEditor))
#define GTK_COLOR_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_COLOR_EDITOR, GtkColorEditorClass))
#define GTK_IS_COLOR_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_EDITOR))
#define GTK_IS_COLOR_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_COLOR_EDITOR))
#define GTK_COLOR_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_COLOR_EDITOR, GtkColorEditorClass))


typedef struct _GtkColorEditor         GtkColorEditor;

GType       gtk_color_editor_get_type (void) G_GNUC_CONST;
GtkWidget * gtk_color_editor_new      (void);

G_END_DECLS

#endif /* __GTK_COLOR_EDITOR_H__ */
