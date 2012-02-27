/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __GTK_MODEL_MENU_H__
#define __GTK_MODEL_MENU_H__

#include <gtk/gactionobservable.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkaccelgroup.h>
#include <gio/gio.h>

G_GNUC_INTERNAL
GtkWidget * gtk_model_menu_create_menu_bar (GMenuModel        *model,
                                            GActionObservable *actions,
                                            GtkAccelGroup     *accels);

G_GNUC_INTERNAL
GtkWidget * gtk_model_menu_create_menu     (GMenuModel        *model,
                                            GActionObservable *actions,
                                            GtkAccelGroup     *accels);

#endif /* __GTK_MODEL_MENU_H__ */
