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

#ifndef __GTK_TIM2_SORT_MODEL_H__
#define __GTK_TIM2_SORT_MODEL_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksorter.h>


G_BEGIN_DECLS

#define GTK_TYPE_TIM2_SORT_MODEL (gtk_tim2_sort_model_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkTim2SortModel, gtk_tim2_sort_model, GTK, TIM2_SORT_MODEL, GObject)

GDK_AVAILABLE_IN_ALL
GtkTim2SortModel *      gtk_tim2_sort_model_new                 (GListModel            *model,
                                                                 GtkSorter             *sorter);
GDK_AVAILABLE_IN_ALL
void                    gtk_tim2_sort_model_set_sorter          (GtkTim2SortModel       *self,
                                                                 GtkSorter              *sorter);
GDK_AVAILABLE_IN_ALL
GtkSorter *             gtk_tim2_sort_model_get_sorter          (GtkTim2SortModel       *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_tim2_sort_model_set_model           (GtkTim2SortModel       *self,
                                                                 GListModel             *model);
GDK_AVAILABLE_IN_ALL
GListModel *            gtk_tim2_sort_model_get_model           (GtkTim2SortModel       *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_tim2_sort_model_set_incremental     (GtkTim2SortModel       *self,
                                                                 gboolean                incremental);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_tim2_sort_model_get_incremental     (GtkTim2SortModel       *self);

G_END_DECLS

#endif /* __GTK_TIM2_SORT_MODEL_H__ */
