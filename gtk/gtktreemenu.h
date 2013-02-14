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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TREE_MENU_H__
#define __GTK_TREE_MENU_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkmenu.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellarea.h>

G_BEGIN_DECLS

#define GTK_TYPE_TREE_MENU		  (_gtk_tree_menu_get_type ())
#define GTK_TREE_MENU(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TREE_MENU, GtkTreeMenu))
#define GTK_TREE_MENU_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_MENU, GtkTreeMenuClass))
#define GTK_IS_TREE_MENU(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TREE_MENU))
#define GTK_IS_TREE_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TREE_MENU))
#define GTK_TREE_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TREE_MENU, GtkTreeMenuClass))

typedef struct _GtkTreeMenu              GtkTreeMenu;
typedef struct _GtkTreeMenuClass         GtkTreeMenuClass;
typedef struct _GtkTreeMenuPrivate       GtkTreeMenuPrivate;

/**
 * GtkTreeMenuHeaderFunc:
 * @model: a #GtkTreeModel
 * @iter: the #GtkTreeIter pointing at a row in @model
 * @data: user data
 *
 * Function type for determining whether the row pointed to by @iter
 * which has children should be replicated as a header item in the
 * child menu.
 *
 * Return value: %TRUE if @iter should have an activatable header menu
 * item created for it in a submenu.
 */
typedef gboolean (*GtkTreeMenuHeaderFunc) (GtkTreeModel      *model,
                                           GtkTreeIter       *iter,
                                           gpointer           data);

struct _GtkTreeMenu
{
  GtkMenu parent_instance;

  /*< private >*/
  GtkTreeMenuPrivate *priv;
};

struct _GtkTreeMenuClass
{
  GtkMenuClass parent_class;

  /*< private >*/
  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
};

GType                 _gtk_tree_menu_get_type                       (void) G_GNUC_CONST;

GtkWidget            *_gtk_tree_menu_new                            (void);
GtkWidget            *_gtk_tree_menu_new_with_area                  (GtkCellArea         *area);
GtkWidget            *_gtk_tree_menu_new_full                       (GtkCellArea         *area,
                                                                     GtkTreeModel        *model,
                                                                     GtkTreePath         *root);
void                  _gtk_tree_menu_set_model                      (GtkTreeMenu         *menu,
                                                                     GtkTreeModel        *model);
GtkTreeModel         *_gtk_tree_menu_get_model                      (GtkTreeMenu         *menu);
void                  _gtk_tree_menu_set_root                       (GtkTreeMenu         *menu,
                                                                     GtkTreePath         *path);
GtkTreePath          *_gtk_tree_menu_get_root                       (GtkTreeMenu         *menu);
gboolean              _gtk_tree_menu_get_tearoff                    (GtkTreeMenu         *menu);
void                  _gtk_tree_menu_set_tearoff                    (GtkTreeMenu         *menu,
                                                                     gboolean             tearoff);
gint                  _gtk_tree_menu_get_wrap_width                 (GtkTreeMenu         *menu);
void                  _gtk_tree_menu_set_wrap_width                 (GtkTreeMenu         *menu,
                                                                     gint                 width);
gint                  _gtk_tree_menu_get_row_span_column            (GtkTreeMenu         *menu);
void                  _gtk_tree_menu_set_row_span_column            (GtkTreeMenu         *menu,
                                                                     gint                 row_span);
gint                  _gtk_tree_menu_get_column_span_column         (GtkTreeMenu         *menu);
void                  _gtk_tree_menu_set_column_span_column         (GtkTreeMenu         *menu,
                                                                     gint                 column_span);

GtkTreeViewRowSeparatorFunc _gtk_tree_menu_get_row_separator_func   (GtkTreeMenu          *menu);
void                        _gtk_tree_menu_set_row_separator_func   (GtkTreeMenu          *menu,
                                                                     GtkTreeViewRowSeparatorFunc func,
                                                                     gpointer              data,
                                                                     GDestroyNotify        destroy);

GtkTreeMenuHeaderFunc _gtk_tree_menu_get_header_func                (GtkTreeMenu          *menu);
void                  _gtk_tree_menu_set_header_func                (GtkTreeMenu          *menu,
                                                                     GtkTreeMenuHeaderFunc func,
                                                                     gpointer              data,
                                                                     GDestroyNotify        destroy);

G_END_DECLS

#endif /* __GTK_TREE_MENU_H__ */
