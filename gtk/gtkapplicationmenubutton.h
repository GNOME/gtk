/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __GTK_APPLICATION_MENU_BUTTON_H__
#define __GTK_APPLICATION_MENU_BUTTON_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_APPLICATION_MENU_BUTTON                    (gtk_application_menu_button_get_type ())
#define GTK_APPLICATION_MENU_BUTTON(inst)                   (G_TYPE_CHECK_INSTANCE_CAST ((inst),                      \
                                                             GTK_TYPE_APPLICATION_MENU_BUTTON,                        \
                                                             GtkApplicationMenuButton))
#define GTK_IS_APPLICATION_MENU_BUTTON(inst)                (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                      \
                                                             GTK_TYPE_APPLICATION_MENU_BUTTON))

typedef struct _GtkApplicationMenuButton                    GtkApplicationMenuButton;

GType                   gtk_application_menu_button_get_type           (void) G_GNUC_CONST;

GtkWidget *             gtk_application_menu_button_new                (void);

G_END_DECLS

#endif /* __GTK_APPLICATION_MENU_BUTTON_H__ */
