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
#ifndef __GTK_OPTION_MENU_H__
#define __GTK_OPTION_MENU_H__


#include <gdk/gdk.h>
#include <gtk/gtkbutton.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_OPTION_MENU(obj)          GTK_CHECK_CAST (obj, gtk_option_menu_get_type (), GtkOptionMenu)
#define GTK_OPTION_MENU_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_option_menu_get_type (), GtkOptionMenuClass)
#define GTK_IS_OPTION_MENU(obj)       GTK_CHECK_TYPE (obj, gtk_option_menu_get_type ())


typedef struct _GtkOptionMenu       GtkOptionMenu;
typedef struct _GtkOptionMenuClass  GtkOptionMenuClass;

struct _GtkOptionMenu
{
  GtkButton button;

  GtkWidget *menu;
  GtkWidget *menu_item;

  guint16 width;
  guint16 height;
};

struct _GtkOptionMenuClass
{
  GtkButtonClass parent_class;
};


guint      gtk_option_menu_get_type    (void);
GtkWidget* gtk_option_menu_new         (void);
GtkWidget* gtk_option_menu_get_menu    (GtkOptionMenu *option_menu);
void       gtk_option_menu_set_menu    (GtkOptionMenu *option_menu,
					GtkWidget     *menu);
void       gtk_option_menu_remove_menu (GtkOptionMenu *option_menu);
void       gtk_option_menu_set_history (GtkOptionMenu *option_menu,
					guint          index);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_OPTION_MENU_H__ */
