/* gtkpopoverbin.h: A single-child container with a popover
 *
 * SPDX-FileCopyrightText: 2025  Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtkpopover.h>

G_BEGIN_DECLS

#define GTK_TYPE_POPOVER_BIN (gtk_popover_bin_get_type())

GDK_AVAILABLE_IN_4_22
G_DECLARE_FINAL_TYPE (GtkPopoverBin, gtk_popover_bin, GTK, POPOVER_BIN, GtkWidget)

GDK_AVAILABLE_IN_4_22
GtkWidget *     gtk_popover_bin_new             (void);

GDK_AVAILABLE_IN_4_22
void            gtk_popover_bin_set_child       (GtkPopoverBin *self,
                                                 GtkWidget     *child);
GDK_AVAILABLE_IN_4_22
GtkWidget *     gtk_popover_bin_get_child       (GtkPopoverBin *self);

GDK_AVAILABLE_IN_4_22
void            gtk_popover_bin_set_menu_model  (GtkPopoverBin *self,
                                                 GMenuModel    *model);
GDK_AVAILABLE_IN_4_22
GMenuModel *    gtk_popover_bin_get_menu_model  (GtkPopoverBin *self);
GDK_AVAILABLE_IN_4_22
void            gtk_popover_bin_set_popover     (GtkPopoverBin *self,
                                                 GtkWidget     *popover);
GDK_AVAILABLE_IN_4_22
GtkWidget *     gtk_popover_bin_get_popover     (GtkPopoverBin *self);
GDK_AVAILABLE_IN_4_22
void            gtk_popover_bin_popup           (GtkPopoverBin *self);
GDK_AVAILABLE_IN_4_22
void            gtk_popover_bin_popdown         (GtkPopoverBin *self);

G_END_DECLS
