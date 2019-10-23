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

struct _GtkListBase
{
  GtkWidget parent_instance;
};

struct _GtkListBaseClass
{
  GtkWidgetClass parent_class;

  const char *         list_item_name;
  gsize                list_item_size;
  gsize                list_item_augment_size;
  GtkRbTreeAugmentFunc list_item_augment_func;

  void                 (* adjustment_value_changed)             (GtkListBase            *self,
                                                                 GtkOrientation          orientation);
};

GtkListItemManager *   gtk_list_base_get_manager                (GtkListBase            *self);
GtkScrollablePolicy    gtk_list_base_get_scroll_policy          (GtkListBase            *self,
                                                                 GtkOrientation          orientation);
void                   gtk_list_base_get_adjustment_values      (GtkListBase            *self,
                                                                 GtkOrientation          orientation,
                                                                 int                    *value,
                                                                 int                    *size,
                                                                 int                    *page_size);
int                    gtk_list_base_set_adjustment_values      (GtkListBase            *self,
                                                                 GtkOrientation          orientation,
                                                                 int                     value,
                                                                 int                     size,
                                                                 int                     page_size);

void                   gtk_list_base_select_item                (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 gboolean                modify,
                                                                 gboolean                extend);
gboolean               gtk_list_base_grab_focus_on_item         (GtkListBase            *self,
                                                                 guint                   pos,
                                                                 gboolean                select,
                                                                 gboolean                modify,
                                                                 gboolean                extend);

#endif /* __GTK_LIST_BASE_PRIVATE_H__ */
