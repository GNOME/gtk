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
#include <gtk/print/gtkprintsettings.h>
#include <gtk/print/gtkpagesetup.h>

G_BEGIN_DECLS

#define GTK_TYPE_PRINT_DIALOG (gtk_print_dialog_get_type ())

GDK_AVAILABLE_IN_4_14
G_DECLARE_FINAL_TYPE (GtkPrintDialog, gtk_print_dialog, GTK, PRINT_DIALOG, GObject)

GDK_AVAILABLE_IN_4_14
GtkPrintDialog *gtk_print_dialog_new                    (void);

GDK_AVAILABLE_IN_4_14
const char *    gtk_print_dialog_get_title              (GtkPrintDialog       *self);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_set_title              (GtkPrintDialog       *self,
                                                         const char           *title);

GDK_AVAILABLE_IN_4_14
const char *    gtk_print_dialog_get_accept_label       (GtkPrintDialog       *self);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_set_accept_label       (GtkPrintDialog       *self,
                                                         const char           *accept_label);

GDK_AVAILABLE_IN_4_14
gboolean        gtk_print_dialog_get_modal              (GtkPrintDialog       *self);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_set_modal              (GtkPrintDialog       *self,
                                                         gboolean              modal);

GDK_AVAILABLE_IN_4_14
GtkPageSetup *  gtk_print_dialog_get_default_page_setup (GtkPrintDialog       *self);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_set_default_page_setup (GtkPrintDialog       *self,
                                                         GtkPageSetup         *default_page_setup);

GDK_AVAILABLE_IN_4_14
GtkPrintSettings * gtk_print_dialog_get_print_settings  (GtkPrintDialog       *self);

GDK_AVAILABLE_IN_4_14
void               gtk_print_dialog_set_print_settings  (GtkPrintDialog       *self,
                                                         GtkPrintSettings     *print_settings);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_prepare_print          (GtkPrintDialog       *self,
                                                         GtkWindow            *parent,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);

GDK_AVAILABLE_IN_4_14
gboolean        gtk_print_dialog_prepare_print_finish   (GtkPrintDialog       *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_print_stream           (GtkPrintDialog       *self,
                                                         GtkWindow            *parent,
                                                         GInputStream         *content,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);

GDK_AVAILABLE_IN_4_14
gboolean        gtk_print_dialog_print_stream_finish    (GtkPrintDialog       *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

GDK_AVAILABLE_IN_4_14
void            gtk_print_dialog_print_file             (GtkPrintDialog       *self,
                                                         GtkWindow            *parent,
                                                         GFile                *file,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);

GDK_AVAILABLE_IN_4_14
gboolean        gtk_print_dialog_print_file_finish      (GtkPrintDialog       *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

G_END_DECLS
