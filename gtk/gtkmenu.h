/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_MENU_H__
#define __GTK_MENU_H__


#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkmenushell.h>


G_BEGIN_DECLS

#define GTK_TYPE_MENU			(gtk_menu_get_type ())
#define GTK_MENU(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU, GtkMenu))
#define GTK_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MENU, GtkMenuClass))
#define GTK_IS_MENU(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU))
#define GTK_IS_MENU_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MENU))
#define GTK_MENU_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MENU, GtkMenuClass))


typedef struct _GtkMenu        GtkMenu;
typedef struct _GtkMenuClass   GtkMenuClass;
typedef struct _GtkMenuPrivate GtkMenuPrivate;

/**
 * GtkMenuPositionFunc:
 * @menu: a #GtkMenu.
 * @x: (out): address of the #gint representing the horizontal
 *     position where the menu shall be drawn.
 * @y: (out): address of the #gint representing the vertical position
 *     where the menu shall be drawn.  This is an output parameter.
 * @push_in: (out): This parameter controls how menus placed outside
 *     the monitor are handled.  If this is set to %TRUE and part of
 *     the menu is outside the monitor then GTK+ pushes the window
 *     into the visible area, effectively modifying the popup
 *     position.  Note that moving and possibly resizing the menu
 *     around will alter the scroll position to keep the menu items
 *     "in place", i.e. at the same monitor position they would have
 *     been without resizing.  In practice, this behavior is only
 *     useful for combobox popups or option menus and cannot be used
 *     to simply confine a menu to monitor boundaries.  In that case,
 *     changing the scroll offset is not desirable.
 * @user_data: the data supplied by the user in the gtk_menu_popup()
 *     @data parameter.
 *
 * A user function supplied when calling gtk_menu_popup() which
 * controls the positioning of the menu when it is displayed.  The
 * function sets the @x and @y parameters to the coordinates where the
 * menu is to be drawn.  To make the menu appear on a different
 * monitor than the mouse pointer, gtk_menu_set_monitor() must be
 * called.
 */
typedef void (*GtkMenuPositionFunc) (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gboolean  *push_in,
				     gpointer	user_data);

/**
 * GtkMenuDetachFunc:
 * @attach_widget: the #GtkWidget that the menu is being detached from.
 * @menu: the #GtkMenu being detached.
 *
 * A user function supplied when calling gtk_menu_attach_to_widget() which 
 * will be called when the menu is later detached from the widget.
 */
typedef void (*GtkMenuDetachFunc)   (GtkWidget *attach_widget,
				     GtkMenu   *menu);

struct _GtkMenu
{
  GtkMenuShell menu_shell;

  /*< private >*/
  GtkMenuPrivate *priv;
};

struct _GtkMenuClass
{
  GtkMenuShellClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType	   gtk_menu_get_type		  (void) G_GNUC_CONST;
GtkWidget* gtk_menu_new			  (void);
GDK_AVAILABLE_IN_3_4
GtkWidget* gtk_menu_new_from_model        (GMenuModel *model);

/* Display the menu onscreen */
void	   gtk_menu_popup		  (GtkMenu	       *menu,
					   GtkWidget	       *parent_menu_shell,
					   GtkWidget	       *parent_menu_item,
					   GtkMenuPositionFunc	func,
					   gpointer		data,
					   guint		button,
					   guint32		activate_time);
void       gtk_menu_popup_for_device      (GtkMenu             *menu,
                                           GdkDevice           *device,
                                           GtkWidget           *parent_menu_shell,
                                           GtkWidget           *parent_menu_item,
                                           GtkMenuPositionFunc  func,
                                           gpointer             data,
                                           GDestroyNotify       destroy,
                                           guint                button,
                                           guint32              activate_time);

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

/* set/get the accelerator group that holds global accelerators (should
 * be added to the corresponding toplevel with gtk_window_add_accel_group().
 */
void	       gtk_menu_set_accel_group	  (GtkMenu	       *menu,
					   GtkAccelGroup       *accel_group);
GtkAccelGroup* gtk_menu_get_accel_group	  (GtkMenu	       *menu);
void           gtk_menu_set_accel_path    (GtkMenu             *menu,
					   const gchar         *accel_path);
const gchar*   gtk_menu_get_accel_path    (GtkMenu             *menu);

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
gboolean   gtk_menu_get_tearoff_state     (GtkMenu             *menu);

/* This sets the window manager title for the window that
 * appears when a menu is torn off
 */
void          gtk_menu_set_title          (GtkMenu             *menu,
                                           const gchar         *title);
const gchar * gtk_menu_get_title          (GtkMenu             *menu);

void       gtk_menu_reorder_child         (GtkMenu             *menu,
                                           GtkWidget           *child,
                                           gint                position);

void	   gtk_menu_set_screen		  (GtkMenu	       *menu,
					   GdkScreen	       *screen);

void       gtk_menu_attach                (GtkMenu             *menu,
                                           GtkWidget           *child,
                                           guint                left_attach,
                                           guint                right_attach,
                                           guint                top_attach,
                                           guint                bottom_attach);

void       gtk_menu_set_monitor           (GtkMenu             *menu,
                                           gint                 monitor_num);
gint       gtk_menu_get_monitor           (GtkMenu             *menu);
GList*     gtk_menu_get_for_attach_widget (GtkWidget           *widget); 

void     gtk_menu_set_reserve_toggle_size (GtkMenu  *menu,
                                          gboolean   reserve_toggle_size);
gboolean gtk_menu_get_reserve_toggle_size (GtkMenu  *menu);


G_END_DECLS

#endif /* __GTK_MENU_H__ */
