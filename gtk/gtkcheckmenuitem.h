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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_MENU_CHECK_ITEM_H__
#define __GTK_MENU_CHECK_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkmenuitem.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_TYPE_CHECK_MENU_ITEM	    (gtk_check_menu_item_get_type ())
#define GTK_CHECK_MENU_ITEM(obj)	    (GTK_CHECK_CAST ((obj), GTK_TYPE_CHECK_MENU_ITEM, GtkCheckMenuItem))
#define GTK_CHECK_MENU_ITEM_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CHECK_MENU_ITEM, GtkCheckMenuItemClass))
#define GTK_IS_CHECK_MENU_ITEM(obj)	    (GTK_CHECK_TYPE ((obj), GTK_TYPE_CHECK_MENU_ITEM))
#define GTK_IS_CHECK_MENU_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CHECK_MENU_ITEM))


typedef struct _GtkCheckMenuItem       GtkCheckMenuItem;
typedef struct _GtkCheckMenuItemClass  GtkCheckMenuItemClass;

struct _GtkCheckMenuItem
{
  GtkMenuItem menu_item;
  
  guint active : 1;
  guint always_show_toggle : 1;
};

struct _GtkCheckMenuItemClass
{
  GtkMenuItemClass parent_class;
  
  void (* toggled)	  (GtkCheckMenuItem *check_menu_item);
  void (* draw_indicator) (GtkCheckMenuItem *check_menu_item,
			   GdkRectangle	    *area);
};


GtkType	   gtk_check_menu_item_get_type	      (void);
GtkWidget* gtk_check_menu_item_new	      (void);
GtkWidget* gtk_check_menu_item_new_with_label (const gchar	*label);
void	   gtk_check_menu_item_set_state      (GtkCheckMenuItem *check_menu_item,
					       gint		 state);
void	   gtk_check_menu_item_set_show_toggle(GtkCheckMenuItem *menu_item,
					       gboolean		 always);
void	   gtk_check_menu_item_toggled	      (GtkCheckMenuItem *check_menu_item);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CHECK_MENU_ITEM_H__ */
