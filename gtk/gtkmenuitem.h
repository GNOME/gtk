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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_MENU_ITEM_H__
#define __GTK_MENU_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkitem.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define	GTK_TYPE_MENU_ITEM		(gtk_menu_item_get_type ())
#define GTK_MENU_ITEM(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_MENU_ITEM, GtkMenuItem))
#define GTK_MENU_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MENU_ITEM, GtkMenuItemClass))
#define GTK_IS_MENU_ITEM(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_MENU_ITEM))
#define GTK_IS_MENU_ITEM_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MENU_ITEM))


typedef struct _GtkMenuItem	  GtkMenuItem;
typedef struct _GtkMenuItemClass  GtkMenuItemClass;

struct _GtkMenuItem
{
  GtkItem item;
  
  GtkWidget *submenu;
  
  guint	  accelerator_signal;
  guint16 toggle_size;
  guint16 accelerator_width;
  
  guint show_toggle_indicator : 1;
  guint show_submenu_indicator : 1;
  guint submenu_placement : 1;
  guint submenu_direction : 1;
  guint right_justify: 1;
  guint timer;
};

struct _GtkMenuItemClass
{
  GtkItemClass parent_class;
  
  guint toggle_size;
  /* If the following flag is true, then we should always hide
   * the menu when the MenuItem is activated. Otherwise, the 
   * it is up to the caller. For instance, when navigating
   * a menu with the keyboard, <Space> doesn't hide, but
   * <Return> does.
   */
  guint hide_on_activate : 1;
  
  void (* activate)      (GtkMenuItem *menu_item);
  void (* activate_item) (GtkMenuItem *menu_item);
};


GtkType	   gtk_menu_item_get_type	  (void);
GtkWidget* gtk_menu_item_new		  (void);
GtkWidget* gtk_menu_item_new_with_label	  (const gchar	       *label);
void	   gtk_menu_item_set_submenu	  (GtkMenuItem	       *menu_item,
					   GtkWidget	       *submenu);
void	   gtk_menu_item_remove_submenu	  (GtkMenuItem	       *menu_item);
void	   gtk_menu_item_set_placement	  (GtkMenuItem	       *menu_item,
					   GtkSubmenuPlacement	placement);
void	   gtk_menu_item_configure	  (GtkMenuItem	       *menu_item,
					   gint			show_toggle_indicator,
					   gint			show_submenu_indicator);
void	   gtk_menu_item_select		  (GtkMenuItem	       *menu_item);
void	   gtk_menu_item_deselect	  (GtkMenuItem	       *menu_item);
void	   gtk_menu_item_activate	  (GtkMenuItem	       *menu_item);
void	   gtk_menu_item_right_justify	  (GtkMenuItem	       *menu_item);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MENU_ITEM_H__ */
