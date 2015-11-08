/* GTK - The GIMP Toolkit
 * Copyright 2015  Emmanuele Bassi 
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_TOOLTIP_WINDOW_PRIVATE_H__
#define __GTK_TOOLTIP_WINDOW_PRIVATE_H__

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_TOOLTIP_WINDOW (gtk_tooltip_window_get_type ())

G_DECLARE_FINAL_TYPE (GtkTooltipWindow, gtk_tooltip_window, GTK, TOOLTIP_WINDOW, GtkWindow)

GtkWidget *     gtk_tooltip_window_new                          (void);

void            gtk_tooltip_window_set_label_markup             (GtkTooltipWindow *window,
                                                                 const char       *markup);
void            gtk_tooltip_window_set_label_text               (GtkTooltipWindow *window,
                                                                 const char       *text);
void            gtk_tooltip_window_set_image_icon               (GtkTooltipWindow *window,
                                                                 GdkPixbuf        *pixbuf);
void            gtk_tooltip_window_set_image_icon_from_stock    (GtkTooltipWindow *window,
                                                                 const char       *stock_id,
                                                                 GtkIconSize       icon_size);
void            gtk_tooltip_window_set_image_icon_from_name     (GtkTooltipWindow *window,
                                                                 const char       *icon_name,
                                                                 GtkIconSize       icon_size);
void            gtk_tooltip_window_set_image_icon_from_gicon    (GtkTooltipWindow *window,
                                                                 GIcon            *gicon,
                                                                 GtkIconSize       icon_size);
void            gtk_tooltip_window_set_custom_widget            (GtkTooltipWindow *window,
                                                                 GtkWidget        *custom_widget);

G_END_DECLS

#endif /* __GTK_TOOLTIP_WINDOW_PRIVATE_H__ */
