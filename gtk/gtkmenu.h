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
#ifndef __GTK_MENU_H__
#define __GTK_MENU_H__


#include <gdk/gdk.h>
#include <gtk/gtkaccelerator.h>
#include <gtk/gtkmenushell.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_MENU(obj)          GTK_CHECK_CAST (obj, gtk_menu_get_type (), GtkMenu)
#define GTK_MENU_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_menu_get_type (), GtkMenuClass)
#define GTK_IS_MENU(obj)       GTK_CHECK_TYPE (obj, gtk_menu_get_type ())


typedef struct _GtkMenu       GtkMenu;
typedef struct _GtkMenuClass  GtkMenuClass;

typedef void (*GtkMenuPositionFunc) (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gpointer   user_data);
typedef void (*GtkMenuDetachFunc)   (GtkWidget *attach_widget,
				     GtkMenu   *menu);

struct _GtkMenu
{
  GtkMenuShell menu_shell;

  GList *children;

  GtkWidget *parent_menu_item;
  GtkWidget *old_active_menu_item;

  GtkAcceleratorTable *accelerator_table;
  GtkMenuPositionFunc position_func;
  gpointer position_func_data;
};

struct _GtkMenuClass
{
  GtkMenuShellClass parent_class;
};


guint      gtk_menu_get_type              (void);
GtkWidget* gtk_menu_new                   (void);
void       gtk_menu_append                (GtkMenu             *menu,
					   GtkWidget           *child);
void       gtk_menu_prepend               (GtkMenu             *menu,
					   GtkWidget           *child);
void       gtk_menu_insert                (GtkMenu             *menu,
					   GtkWidget           *child,
					   gint                 position);
void       gtk_menu_popup                 (GtkMenu             *menu,
					   GtkWidget           *parent_menu_shell,
					   GtkWidget           *parent_menu_item,
					   GtkMenuPositionFunc  func,
					   gpointer             data,
					   guint                button,
					   guint32              activate_time);
void       gtk_menu_popdown               (GtkMenu             *menu);
GtkWidget* gtk_menu_get_active            (GtkMenu             *menu);
void       gtk_menu_set_active            (GtkMenu             *menu,
					   guint                index);
void       gtk_menu_set_accelerator_table (GtkMenu             *menu,
					   GtkAcceleratorTable *table);
void	   gtk_menu_attach_to_widget	  (GtkMenu	       *menu,
					   GtkWidget	       *attach_widget,
					   GtkMenuDetachFunc	detacher);
GtkWidget* gtk_menu_get_attach_widget	  (GtkMenu	       *menu);
void	   gtk_menu_detach		  (GtkMenu	       *menu);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MENU_H__ */
