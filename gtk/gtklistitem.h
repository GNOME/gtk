/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_LIST_ITEM_H__
#define __GTK_LIST_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkitem.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_LIST_ITEM(obj)          GTK_CHECK_CAST (obj, gtk_list_item_get_type (), GtkListItem)
#define GTK_LIST_ITEM_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_list_item_get_type (), GtkListItemClass)
#define GTK_IS_LIST_ITEM(obj)       GTK_CHECK_TYPE (obj, gtk_list_item_get_type ())


typedef struct _GtkListItem       GtkListItem;
typedef struct _GtkListItemClass  GtkListItemClass;

struct _GtkListItem
{
  GtkItem item;
};

struct _GtkListItemClass
{
  GtkItemClass parent_class;
};


guint      gtk_list_item_get_type       (void);
GtkWidget* gtk_list_item_new            (void);
GtkWidget* gtk_list_item_new_with_label (const gchar      *label);
void       gtk_list_item_select         (GtkListItem      *list_item);
void       gtk_list_item_deselect       (GtkListItem      *list_item);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LIST_ITEM_H__ */
