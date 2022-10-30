/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <gtk/gtkwindow.h>
#include <gtk/gtkbutton.h>

G_BEGIN_DECLS

#define GTK_TYPE_MESSAGE_WINDOW (gtk_message_window_get_type ())

G_DECLARE_FINAL_TYPE (GtkMessageWindow, gtk_message_window, GTK, MESSAGE_WINDOW, GtkWindow)

GtkMessageWindow *      gtk_message_window_new                  (void);

void                    gtk_message_window_set_message          (GtkMessageWindow *self,
                                                                 const char       *message);

void                    gtk_message_window_set_detail           (GtkMessageWindow *self,
                                                                 const char       *detail);

void                    gtk_message_window_add_button           (GtkMessageWindow *self,
                                                                 GtkWidget        *button);

void                    gtk_message_window_add_extra_widget     (GtkMessageWindow *self,
                                                                 GtkWidget        *extra);

G_END_DECLS
