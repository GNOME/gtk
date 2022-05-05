/*
 * Copyright © 2018 Benjamin Otte
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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_WIDGET_PAINTABLE_H__
#define __GTK_WIDGET_PAINTABLE_H__

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_WIDGET_PAINTABLE (gtk_widget_paintable_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkWidgetPaintable, gtk_widget_paintable, GTK, WIDGET_PAINTABLE, GObject)

GDK_AVAILABLE_IN_ALL
GdkPaintable *  gtk_widget_paintable_new                (GtkWidget              *widget);

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_widget_paintable_get_widget         (GtkWidgetPaintable     *self);
GDK_AVAILABLE_IN_ALL
void            gtk_widget_paintable_set_widget         (GtkWidgetPaintable     *self,
                                                         GtkWidget              *widget);

/**
 * GtkWidgetPaintableArea:
 * @GTK_WIDGET_PAINTABLE_AREA_WIDGET: Restrict the paintable to the widget bounds
 * @GTK_WIDGET_PAINTABLE_AREA_RENDERED: Include all render nodes produced by the
 *   widget in the paintable area
 *
 * Used to indicate what area should be captured by a widget paintable.
 */
typedef enum {
  GTK_WIDGET_PAINTABLE_AREA_WIDGET,
  GTK_WIDGET_PAINTABLE_AREA_RENDERED
} GtkWidgetPaintableArea;

GDK_AVAILABLE_IN_4_8
void            gtk_widget_paintable_set_observed_area  (GtkWidgetPaintable     *self,
                                                         GtkWidgetPaintableArea  area);

GDK_AVAILABLE_IN_4_8
GtkWidgetPaintableArea
                gtk_widget_paintable_get_observed_area  (GtkWidgetPaintable     *self);

G_END_DECLS

#endif /* __GTK_WIDGET_PAINTABLE_H__ */
