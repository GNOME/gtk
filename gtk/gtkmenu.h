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

#ifndef __GTK_MENU_H__
#define __GTK_MENU_H__


#include <gdk/gdk.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkmenushell.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_MENU			(gtk_menu_get_type ())
#define GTK_MENU(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_MENU, GtkMenu))
#define GTK_MENU_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_MENU, GtkMenuClass))
#define GTK_IS_MENU(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_MENU))
#define GTK_IS_MENU_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MENU))


typedef struct _GtkMenu	      GtkMenu;
typedef struct _GtkMenuClass  GtkMenuClass;

typedef void (*GtkMenuPositionFunc) (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gpointer	user_data);
typedef void (*GtkMenuDetachFunc)   (GtkWidget *attach_widget,
				     GtkMenu   *menu);

struct _GtkMenu
{
  GtkMenuShell menu_shell;
  
  GtkWidget *parent_menu_item;
  GtkWidget *old_active_menu_item;
  
  GtkAccelGroup *accel_group;
  GtkMenuPositionFunc position_func;
  gpointer position_func_data;

  /* Do _not_ touch these widgets directly. We hide the reference
   * count from the toplevel to the menu, so it must be restored
   * before operating on these widgets
   */
  GtkWidget *toplevel;
  GtkWidget *tearoff_window;

  guint torn_off : 1;
};

struct _GtkMenuClass
{
  GtkMenuShellClass parent_class;
};


GtkType	   gtk_menu_get_type		  (void);
GtkWidget* gtk_menu_new			  (void);

/* Wrappers for the Menu Shell operations */
void	   gtk_menu_append		  (GtkMenu	       *menu,
					   GtkWidget	       *child);
void	   gtk_menu_prepend		  (GtkMenu	       *menu,
					   GtkWidget	       *child);
void	   gtk_menu_insert		  (GtkMenu	       *menu,
					   GtkWidget	       *child,
					   gint			position);

/* Display the menu onscreen */
void	   gtk_menu_popup		  (GtkMenu	       *menu,
					   GtkWidget	       *parent_menu_shell,
					   GtkWidget	       *parent_menu_item,
					   GtkMenuPositionFunc	func,
					   gpointer		data,
					   guint		button,
					   guint32		activate_time);

/* Position the menu according to its position function. Called
 * from gtkmenuitem.c when a menu-item changes its allocation
 */
void	   gtk_menu_reposition		  (GtkMenu	       *menu);

void	   gtk_menu_popdown		  (GtkMenu	       *menu);

/* Keep track of the last menu item selected. (For the purposes
 * of the option menu
 */
GtkWidget* gtk_menu_get_active		  (GtkMenu	       *menu);
void	   gtk_menu_set_active		  (GtkMenu	       *menu,
					   guint		index);

/* set/get the acclerator group that holds global accelerators (should
 * be added to the corresponding toplevel with gtk_window_add_accel_group().
 */
void	       gtk_menu_set_accel_group	  (GtkMenu	       *menu,
					   GtkAccelGroup       *accel_group);
GtkAccelGroup* gtk_menu_get_accel_group	  (GtkMenu	       *menu);

/* get the accelerator group that is used internally by the menu for
 * underline accelerators while the menu is popped up.
 */
GtkAccelGroup* gtk_menu_get_uline_accel_group    (GtkMenu         *menu);
GtkAccelGroup* gtk_menu_ensure_uline_accel_group (GtkMenu         *menu);


/* A reference count is kept for a widget when it is attached to
 * a particular widget. This is typically a menu item; it may also
 * be a widget with a popup menu - for instance, the Notebook widget.
 */
void	   gtk_menu_attach_to_widget	  (GtkMenu	       *menu,
					   GtkWidget	       *attach_widget,
					   GtkMenuDetachFunc	detacher);
void	   gtk_menu_detach		  (GtkMenu	       *menu);

/* This should be dumped in favor of data set when the menu is popped
 * up - that is currently in the ItemFactory code, but should be
 * in the Menu code.
 */
GtkWidget* gtk_menu_get_attach_widget	  (GtkMenu	       *menu);

void       gtk_menu_set_tearoff_state     (GtkMenu             *menu,
					   gboolean             torn_off);

/* This sets the window manager title for the window that
 * appears when a menu is torn off
 */
void       gtk_menu_set_title             (GtkMenu             *menu,
					   const gchar         *title);

void       gtk_menu_reorder_child         (GtkMenu             *menu,
                                           GtkWidget           *child,
                                           gint                position);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MENU_H__ */
