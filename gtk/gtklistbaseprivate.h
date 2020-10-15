/*
 * Copyright © 2019 Benjamin Otte
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

#ifndef __GTK_LIST_BASE_PRIVATE_H__
#define __GTK_LIST_BASE_PRIVATE_H__

#include "gtklistbase.h"

#include "gtklistitemmanagerprivate.h"
#include "gtkprivate.h"

struct _GtkListBase
{
  GtkWidget parent_instance;
};

struct _GtkListBaseClass
{
  GtkWidgetClass parent_class;

  const char *         list_item_name;
  GtkAccessibleRole    list_item_role;
  gsize                list_item_size;
  gsize                list_item_augment_size;
  GtkRbTreeAugmentFunc list_item_augment_func;

  void                 (* adjustment_value_changed)             (GtkListBase            *self,
                                                                 GtkOrientation          orientation);
  gboolean             (* get_allocation_along)                 (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 int                    *offset,
                                                                 int                    *size);
  gboolean             (* get_allocation_across)                (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 int                    *offset,
                                                                 int                    *size);
  gboolean             (* get_position_from_allocation)         (GtkListBase            *self,
                                                                 int                     across,
                                                                 int                     along,
                                                                 guint                  *pos,
                                                                 cairo_rectangle_int_t  *area);
  GtkBitset *          (* get_items_in_rect)                    (GtkListBase            *self,
                                                                 const cairo_rectangle_int_t *rect);
  guint                (* move_focus_along)                     (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 int                     steps);
  guint                (* move_focus_across)                    (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 int                     steps);
};

GtkOrientation         gtk_list_base_get_orientation            (GtkListBase            *self);
#define gtk_list_base_get_opposite_orientation(self) OPPOSITE_ORIENTATION(gtk_list_base_get_orientation(self))
guint                  gtk_list_base_get_focus_position         (GtkListBase            *self);
GtkListItemManager *   gtk_list_base_get_manager                (GtkListBase            *self);
GtkScrollablePolicy    gtk_list_base_get_scroll_policy          (GtkListBase            *self,
                                                                 GtkOrientation          orientation);
guint                  gtk_list_base_get_n_items                (GtkListBase            *self);
GtkSelectionModel *    gtk_list_base_get_model                  (GtkListBase            *self);
gboolean               gtk_list_base_set_model                  (GtkListBase            *self,
                                                                 GtkSelectionModel      *model);
void                   gtk_list_base_update_adjustments         (GtkListBase            *self,
                                                                 int                     total_across,
                                                                 int                     total_along,
                                                                 int                     page_across,
                                                                 int                     page_along,
                                                                 int                    *across,
                                                                 int                    *along);

guint                  gtk_list_base_get_anchor                 (GtkListBase            *self);
void                   gtk_list_base_set_anchor                 (GtkListBase            *self,
                                                                 guint                   anchor_pos,
                                                                 double                  anchor_align_across,
                                                                 GtkPackType             anchor_side_across,
                                                                 double                  anchor_align_along,
                                                                 GtkPackType             anchor_side_along);
void                   gtk_list_base_set_anchor_max_widgets     (GtkListBase            *self,
                                                                 guint                   n_center,
                                                                 guint                   n_above_below);
void                   gtk_list_base_select_item                (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 gboolean                modify,
                                                                 gboolean                extend);
gboolean               gtk_list_base_grab_focus_on_item         (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 gboolean                select,
                                                                 gboolean                modify,
                                                                 gboolean                extend);

void                   gtk_list_base_set_enable_rubberband      (GtkListBase            *self,
                                                                 gboolean                enable);
gboolean               gtk_list_base_get_enable_rubberband      (GtkListBase            *self);
void                   gtk_list_base_allocate_rubberband        (GtkListBase            *self);

void                   gtk_list_base_size_allocate_child        (GtkListBase            *self,
                                                                 GtkWidget              *child,
                                                                 int                     x,
                                                                 int                     y,
                                                                 int                     width,
                                                                 int                     height);

#endif /* __GTK_LIST_BASE_PRIVATE_H__ */
