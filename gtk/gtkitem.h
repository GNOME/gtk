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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_ITEM_H__
#define __GTK_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_ITEM                  (gtk_item_get_type ())
#define GTK_ITEM(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_ITEM, GtkItem))
#define GTK_ITEM_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_ITEM, GtkItemClass))
#define GTK_IS_ITEM(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_ITEM))
#define GTK_IS_ITEM_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ITEM))


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


GtkType gtk_item_get_type (void);
void    gtk_item_select   (GtkItem *item);
void    gtk_item_deselect (GtkItem *item);
void    gtk_item_toggle   (GtkItem *item);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ITEM_H__ */
