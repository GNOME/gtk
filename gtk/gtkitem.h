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
#ifndef __GTK_ITEM_H__
#define __GTK_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_ITEM(obj)          GTK_CHECK_CAST (obj, gtk_item_get_type (), GtkItem)
#define GTK_ITEM_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_item_get_type (), GtkItemClass)
#define GTK_IS_ITEM(obj)       GTK_CHECK_TYPE (obj, gtk_item_get_type ())


typedef struct _GtkItem       GtkItem;
typedef struct _GtkItemClass  GtkItemClass;

struct _GtkItem
{
  GtkBin bin;
};

struct _GtkItemClass
{
  GtkBinClass parent_class;

  void (* select)   (GtkItem *item);
  void (* deselect) (GtkItem *item);
  void (* toggle)   (GtkItem *item);
};


guint  gtk_item_get_type (void);
void   gtk_item_select   (GtkItem *item);
void   gtk_item_deselect (GtkItem *item);
void   gtk_item_toggle   (GtkItem *item);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ITEM_H__ */
