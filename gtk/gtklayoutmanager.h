/* gtklayoutmanager.h: Layout manager base class
 * Copyright 2018  The GNOME Foundation
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
 * Author: Emmanuele Bassi
 */
#pragma once

#include <gtk/gtktypes.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_LAYOUT_MANAGER (gtk_layout_manager_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkLayoutManager, gtk_layout_manager, GTK, LAYOUT_MANAGER, GObject)

/**
 * GtkLayoutManagerClass:
 * @get_request_mode: a virtual function, used to return the preferred
 *   request mode for the layout manager; for instance, "width for height"
 *   or "height for width"; see #GtkSizeRequestMode
 * @measure: a virtual function, used to measure the minimum and preferred
 *   sizes of the widget using the layout manager for a given orientation
 * @allocate: a virtual function, used to allocate the size of the widget
 *   using the layout manager
 *
 * The `GtkLayoutManagerClass` structure contains only private data, and
 * should only be accessed through the provided API, or when subclassing
 * #GtkLayoutManager.
 */
struct _GtkLayoutManagerClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  GtkSizeRequestMode (* get_request_mode) (GtkLayoutManager *manager,
                                           GtkWidget        *widget);

  void               (* measure)          (GtkLayoutManager *manager,
                                           GtkWidget        *widget,
                                           GtkOrientation    orientation,
                                           int               for_size,
                                           int              *minimum,
                                           int              *natural,
                                           int              *minimum_baseline,
                                           int              *natural_baseline);

  void               (* allocate)         (GtkLayoutManager *manager,
                                           GtkWidget        *widget,
                                           int               width,
                                           int               height,
                                           int               baseline);

  /*< private >*/
  gpointer _padding[16];
};

GDK_AVAILABLE_IN_ALL
void                    gtk_layout_manager_measure              (GtkLayoutManager *manager,
                                                                 GtkWidget        *widget,
                                                                 GtkOrientation    orientation,
                                                                 int               for_size,
                                                                 int              *minimum,
                                                                 int              *natural,
                                                                 int              *minimum_baseline,
                                                                 int              *natural_baseline);
GDK_AVAILABLE_IN_ALL
void                    gtk_layout_manager_allocate             (GtkLayoutManager *manager,
                                                                 GtkWidget        *widget,
                                                                 int               width,
                                                                 int               height,
                                                                 int               baseline);
GDK_AVAILABLE_IN_ALL
GtkSizeRequestMode      gtk_layout_manager_get_request_mode     (GtkLayoutManager *manager,
                                                                 GtkWidget        *widget);

GDK_AVAILABLE_IN_ALL
void                    gtk_layout_manager_layout_changed       (GtkLayoutManager *manager);

G_END_DECLS
