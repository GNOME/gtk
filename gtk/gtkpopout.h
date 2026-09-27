/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2026 Benjamin Otte
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
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_POPOUT (gtk_popout_get_type ())

GDK_AVAILABLE_IN_4_26
G_DECLARE_FINAL_TYPE (GtkPopout, gtk_popout, GTK, POPOUT, GtkWidget)

GDK_AVAILABLE_IN_4_26
GtkWidget *       gtk_popout_new                    (GtkWidget         *child);

GDK_AVAILABLE_IN_4_26
void              gtk_popout_set_child              (GtkPopout         *self,
                                                     GtkWidget         *child);

GDK_AVAILABLE_IN_4_26
GtkWidget *       gtk_popout_get_child              (GtkPopout         *self);

GDK_AVAILABLE_IN_4_26
void             gtk_popout_set_popped_out          (GtkPopout         *self,
                                                     gboolean           popped_out);
GDK_AVAILABLE_IN_4_26
gboolean         gtk_popout_get_popped_out          (GtkPopout         *self);


G_END_DECLS
