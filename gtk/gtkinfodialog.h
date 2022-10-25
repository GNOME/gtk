/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2022 Red Hat, Inc.
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_INFO_DIALOG (gtk_info_dialog_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkInfoDialog, gtk_info_dialog, GTK, INFO_DIALOG, GObject)

GDK_AVAILABLE_IN_4_10
GtkInfoDialog * gtk_info_dialog_new                 (void);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_info_dialog_get_modal           (GtkInfoDialog       *self);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_modal           (GtkInfoDialog       *self,
                                                     gboolean             modal);

GDK_AVAILABLE_IN_4_10
const char *    gtk_info_dialog_get_heading         (GtkInfoDialog       *self);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_heading         (GtkInfoDialog       *self,
                                                     const char          *text);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_heading_markup  (GtkInfoDialog       *self,
                                                     const char          *text);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_info_dialog_get_heading_use_markup
                                                    (GtkInfoDialog       *self);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_heading_use_markup
                                                    (GtkInfoDialog       *self,
                                                     gboolean             use_markup);

GDK_AVAILABLE_IN_4_10
const char *    gtk_info_dialog_get_body            (GtkInfoDialog       *self);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_body            (GtkInfoDialog       *self,
                                                     const char          *text);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_body_markup     (GtkInfoDialog       *self,
                                                     const char          *text);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_info_dialog_get_body_use_markup (GtkInfoDialog       *self);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_body_use_markup (GtkInfoDialog       *self,
                                                     gboolean             use_markup);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_set_buttons         (GtkInfoDialog       *self,
                                                     const char * const  *labels);

GDK_AVAILABLE_IN_4_10
const char * const *
                gtk_info_dialog_get_buttons         (GtkInfoDialog       *self);

GDK_AVAILABLE_IN_4_10
void            gtk_info_dialog_present             (GtkInfoDialog       *self,
                                                     GtkWindow           *parent,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_info_dialog_present_finish      (GtkInfoDialog       *self,
                                                     GAsyncResult        *result,
                                                     int                 *button,
                                                     GError             **error);

G_END_DECLS
