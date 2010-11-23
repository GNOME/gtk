/* gtktreemenu.h
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_TREE_MENU_H__
#define __GTK_TREE_MENU_H__

#include <gtk/gtkmenu.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellarea.h>

G_BEGIN_DECLS

#define GTK_TYPE_TREE_MENU		  (gtk_tree_menu_get_type ())
#define GTK_TREE_MENU(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_MENU, GtkTreeMenu))
#define GTK_TREE_MENU_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_MENU, GtkTreeMenuClass))
#define GTK_IS_TREE_MENU(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_MENU))
#define GTK_IS_TREE_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TREE_MENU))
#define GTK_TREE_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TREE_MENU, GtkTreeMenuClass))

typedef struct _GtkTreeMenu              GtkTreeMenu;
typedef struct _GtkTreeMenuClass         GtkTreeMenuClass;
typedef struct _GtkTreeMenuPrivate       GtkTreeMenuPrivate;

typedef gboolean (*GtkTreeMenuHeaderFunc) (GtkTreeModel      *model,
					   GtkTreeIter       *iter,
					   gpointer           data);

struct _GtkTreeMenu
{
  GtkMenu parent_instance;

  GtkTreeMenuPrivate *priv;
};

struct _GtkTreeMenuClass
{
  GtkMenuClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
  void (*_gtk_reserved8) (void);
};

GType                 gtk_tree_menu_get_type                       (void) G_GNUC_CONST;

GtkWidget            *gtk_tree_menu_new                            (void);
GtkWidget            *gtk_tree_menu_new_with_area                  (GtkCellArea         *area);
GtkWidget            *gtk_tree_menu_new_full                       (GtkCellArea         *area,
								    GtkTreeModel        *model,
								    GtkTreePath         *root);
void                  gtk_tree_menu_set_model                      (GtkTreeMenu         *menu,
								    GtkTreeModel        *model);
GtkTreeModel         *gtk_tree_menu_get_model                      (GtkTreeMenu         *menu);
void                  gtk_tree_menu_set_root                       (GtkTreeMenu         *menu,
								    GtkTreePath         *path);
GtkTreePath          *gtk_tree_menu_get_root                       (GtkTreeMenu         *menu);
gboolean              gtk_tree_menu_get_tearoff                    (GtkTreeMenu          *menu);
void                  gtk_tree_menu_set_tearoff                    (GtkTreeMenu          *menu,
								    gboolean              tearoff);

void                        gtk_tree_menu_set_row_separator_func   (GtkTreeMenu          *menu,
								    GtkTreeViewRowSeparatorFunc func,
								    gpointer              data,
								    GDestroyNotify        destroy);
GtkTreeViewRowSeparatorFunc gtk_tree_menu_get_row_separator_func   (GtkTreeMenu          *menu);

void                  gtk_tree_menu_set_header_func                (GtkTreeMenu          *menu,
								    GtkTreeMenuHeaderFunc func,
								    gpointer              data,
								    GDestroyNotify        destroy);
GtkTreeMenuHeaderFunc gtk_tree_menu_get_header_func                (GtkTreeMenu          *menu);

G_END_DECLS

#endif /* __GTK_TREE_MENU_H__ */
