/* GTK - The GIMP Toolkit
 * gtktextviewchild.h Copyright (C) 2019 Red Hat, Inc.
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

#ifndef __GTK_TEXT_VIEW_CHILD_H__
#define __GTK_TEXT_VIEW_CHILD_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>
#include <gtk/gtktextview.h>

#define GTK_TYPE_TEXT_VIEW_CHILD (gtk_text_view_child_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkTextViewChild, gtk_text_view_child, GTK, TEXT_VIEW_CHILD, GtkContainer)

G_GNUC_INTERNAL
GtkWidget         *gtk_text_view_child_new             (GtkTextWindowType  window_type);
G_GNUC_INTERNAL
GtkTextWindowType  gtk_text_view_child_get_window_type (GtkTextViewChild *self);
G_GNUC_INTERNAL
void               gtk_text_view_child_add_overlay     (GtkTextViewChild  *self,
                                                        GtkWidget         *widget,
                                                        int                xpos,
                                                        int                ypos);
G_GNUC_INTERNAL
void               gtk_text_view_child_move_overlay    (GtkTextViewChild  *self,
                                                        GtkWidget         *widget,
                                                        int                xpos,
                                                        int                ypos);

#endif /* __GTK_TEXT_VIEW_CHILD_H__ */
