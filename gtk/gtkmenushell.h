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
#ifndef __GTK_MENU_SHELL_H__
#define __GTK_MENU_SHELL_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_MENU_SHELL(obj)          GTK_CHECK_CAST (obj, gtk_menu_shell_get_type (), GtkMenuShell)
#define GTK_MENU_SHELL_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_menu_shell_get_type (), GtkMenuShellClass)
#define GTK_IS_MENU_SHELL(obj)       GTK_CHECK_TYPE (obj, gtk_menu_shell_get_type ())


typedef struct _GtkMenuShell       GtkMenuShell;
typedef struct _GtkMenuShellClass  GtkMenuShellClass;

struct _GtkMenuShell
{
  GtkContainer container;

  GList *children;
  GtkWidget *active_menu_item;
  GtkWidget *parent_menu_shell;

  guint active : 1;
  guint have_grab : 1;
  guint have_xgrab : 1;
  guint button : 2;
  guint ignore_leave : 1;
  guint menu_flag : 1;

  guint32 activate_time;
};

struct _GtkMenuShellClass
{
  GtkContainerClass parent_class;

  guint submenu_placement : 1;

  void (*deactivate) (GtkMenuShell *menu_shell);
};


guint gtk_menu_shell_get_type   (void);
void  gtk_menu_shell_append     (GtkMenuShell *menu_shell,
				 GtkWidget    *child);
void  gtk_menu_shell_prepend    (GtkMenuShell *menu_shell,
				 GtkWidget    *child);
void  gtk_menu_shell_insert     (GtkMenuShell *menu_shell,
				 GtkWidget    *child,
				 gint          position);
void  gtk_menu_shell_deactivate (GtkMenuShell *menu_shell);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_MENU_SHELL_H__ */
