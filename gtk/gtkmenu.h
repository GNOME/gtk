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

#ifndef __GTK_MENU_H__
#define __GTK_MENU_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkmenushell.h>

G_BEGIN_DECLS

#define GTK_TYPE_MENU			(gtk_menu_get_type ())
#define GTK_MENU(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU, GtkMenu))
#define GTK_IS_MENU(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU))


typedef struct _GtkMenu GtkMenu;

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

GDK_AVAILABLE_IN_ALL
GType	   gtk_menu_get_type		  (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_menu_new			  (void);
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_menu_new_from_model        (GMenuModel *model);

GDK_AVAILABLE_IN_ALL
void       gtk_menu_popup_at_rect         (GtkMenu             *menu,
                                           GdkSurface          *rect_surface,
                                           const GdkRectangle  *rect,
                                           GdkGravity           rect_anchor,
                                           GdkGravity           menu_anchor,
                                           const GdkEvent      *trigger_event);
GDK_AVAILABLE_IN_ALL
void       gtk_menu_popup_at_widget       (GtkMenu             *menu,
                                           GtkWidget           *widget,
                                           GdkGravity           widget_anchor,
                                           GdkGravity           menu_anchor,
                                           const GdkEvent      *trigger_event);
GDK_AVAILABLE_IN_ALL
void       gtk_menu_popup_at_pointer      (GtkMenu             *menu,
                                           const GdkEvent      *trigger_event);

GDK_AVAILABLE_IN_ALL
void	   gtk_menu_reposition		  (GtkMenu	       *menu);

GDK_AVAILABLE_IN_ALL
void	   gtk_menu_popdown		  (GtkMenu	       *menu);

GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_menu_get_active		  (GtkMenu	       *menu);
GDK_AVAILABLE_IN_ALL
void	   gtk_menu_set_active		  (GtkMenu	       *menu,
					   guint		index);

GDK_AVAILABLE_IN_ALL
void	       gtk_menu_set_accel_group	  (GtkMenu	       *menu,
					   GtkAccelGroup       *accel_group);
GDK_AVAILABLE_IN_ALL
GtkAccelGroup* gtk_menu_get_accel_group	  (GtkMenu	       *menu);

GDK_AVAILABLE_IN_ALL
void	   gtk_menu_attach_to_widget	  (GtkMenu	       *menu,
					   GtkWidget	       *attach_widget,
					   GtkMenuDetachFunc	detacher);
GDK_AVAILABLE_IN_ALL
void	   gtk_menu_detach		  (GtkMenu	       *menu);

GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_menu_get_attach_widget	  (GtkMenu	       *menu);

GDK_AVAILABLE_IN_ALL
void       gtk_menu_reorder_child         (GtkMenu             *menu,
                                           GtkWidget           *child,
                                           gint                position);

GDK_AVAILABLE_IN_ALL
void       gtk_menu_set_monitor           (GtkMenu             *menu,
                                           gint                 monitor_num);
GDK_AVAILABLE_IN_ALL
gint       gtk_menu_get_monitor           (GtkMenu             *menu);

GDK_AVAILABLE_IN_ALL
void       gtk_menu_place_on_monitor      (GtkMenu             *menu,
                                           GdkMonitor          *monitor);

GDK_AVAILABLE_IN_ALL
GList*     gtk_menu_get_for_attach_widget (GtkWidget           *widget); 

GDK_AVAILABLE_IN_ALL
void     gtk_menu_set_reserve_toggle_size (GtkMenu  *menu,
                                          gboolean   reserve_toggle_size);
GDK_AVAILABLE_IN_ALL
gboolean gtk_menu_get_reserve_toggle_size (GtkMenu  *menu);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkMenu, g_object_unref)

G_END_DECLS

#endif /* __GTK_MENU_H__ */
