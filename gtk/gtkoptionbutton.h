/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_OPTION_BUTTON_H__
#define __GTK_OPTION_BUTTON_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_OPTION_BUTTON                 (gtk_option_button_get_type ())
#define GTK_OPTION_BUTTON(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPTION_BUTTON, GtkOptionButton))
#define GTK_OPTION_BUTTON_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_OPTION_BUTTON, GtkOptionButtonClass))
#define GTK_IS_OPTION_BUTTON(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPTION_BUTTON))
#define GTK_IS_OPTION_BUTTON_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_OPTION_BUTTON))
#define GTK_OPTION_BUTTON_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OPTION_BUTTON, GtkOptionButtonClass))

typedef struct _GtkOptionButton             GtkOptionButton;
typedef struct _GtkOptionButtonClass        GtkOptionButtonClass;

GDK_AVAILABLE_IN_3_16
GType         gtk_option_button_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_16
GtkWidget *   gtk_option_button_new             (void);

GDK_AVAILABLE_IN_3_16
GtkWidget *   gtk_option_button_get_option_list        (GtkOptionButton *button);

GDK_AVAILABLE_IN_3_16
void          gtk_option_button_set_placeholder_text   (GtkOptionButton *button,
                                                        const gchar     *text);
GDK_AVAILABLE_IN_3_16
const gchar * gtk_option_button_get_placeholder_text   (GtkOptionButton *button);


G_END_DECLS

#endif /* __GTK_OPTION_BUTTON_H__ */
