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

#ifndef __GTK_LIST_ITEM_WIDGET_PRIVATE_H__
#define __GTK_LIST_ITEM_WIDGET_PRIVATE_H__

#include "gtklistitemfactory.h"
#include "gtkwidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_LIST_ITEM_WIDGET         (gtk_list_item_widget_get_type ())
#define GTK_LIST_ITEM_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_LIST_ITEM_WIDGET, GtkListItemWidget))
#define GTK_LIST_ITEM_WIDGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_LIST_ITEM_WIDGET, GtkListItemWidgetClass))
#define GTK_IS_LIST_ITEM_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_LIST_ITEM_WIDGET))
#define GTK_IS_LIST_ITEM_WIDGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_LIST_ITEM_WIDGET))
#define GTK_LIST_ITEM_WIDGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_LIST_ITEM_WIDGET, GtkListItemWidgetClass))

typedef struct _GtkListItemWidget GtkListItemWidget;
typedef struct _GtkListItemWidgetClass GtkListItemWidgetClass;

struct _GtkListItemWidget
{
  GtkWidget parent_instance;
};

struct _GtkListItemWidgetClass
{
  GtkWidgetClass parent_class;

  void          (* activate_signal)                             (GtkListItemWidget            *self);
};

GType                   gtk_list_item_widget_get_type           (void) G_GNUC_CONST;

GtkWidget *             gtk_list_item_widget_new                (GtkListItemFactory     *factory,
                                                                 const char             *css_name,
                                                                 GtkAccessibleRole       role);

void                    gtk_list_item_widget_update             (GtkListItemWidget      *self,
                                                                 guint                   position,
                                                                 gpointer                item,
                                                                 gboolean                selected);
GtkListItem *           gtk_list_item_widget_get_list_item      (GtkListItemWidget      *self);

void                    gtk_list_item_widget_default_setup      (GtkListItemWidget      *self,
                                                                 GtkListItem            *list_item);
void                    gtk_list_item_widget_default_teardown   (GtkListItemWidget      *self,
                                                                 GtkListItem            *list_item);
void                    gtk_list_item_widget_default_update     (GtkListItemWidget      *self,
                                                                 GtkListItem            *list_item,
                                                                 guint                   position,
                                                                 gpointer                item,
                                                                 gboolean                selected);

void                    gtk_list_item_widget_set_factory        (GtkListItemWidget      *self,
                                                                 GtkListItemFactory     *factory);
void                    gtk_list_item_widget_set_single_click_activate
                                                                (GtkListItemWidget     *self,
                                                                 gboolean               single_click_activate);
void                    gtk_list_item_widget_add_child          (GtkListItemWidget      *self,
                                                                 GtkWidget              *child);
void                    gtk_list_item_widget_reorder_child      (GtkListItemWidget      *self,
                                                                 GtkWidget              *child,
                                                                 guint                   position);
void                    gtk_list_item_widget_remove_child       (GtkListItemWidget      *self,
                                                                 GtkWidget              *child);

guint                   gtk_list_item_widget_get_position       (GtkListItemWidget      *self);
gpointer                gtk_list_item_widget_get_item           (GtkListItemWidget      *self);
gboolean                gtk_list_item_widget_get_selected       (GtkListItemWidget      *self);

G_END_DECLS

#endif  /* __GTK_LIST_ITEM_WIDGET_PRIVATE_H__ */
