/* gtklistview.h
 * Copyright (C) 2012 Benjamin Otte <otte@redhat.com>
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_LIST_VIEW_H__
#define __GTK_LIST_VIEW_H__

#include <gtk/gtkcontainer.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellarea.h>
#include <gtk/gtkselection.h>
#include <gtk/gtktooltip.h>

G_BEGIN_DECLS

#define GTK_TYPE_LIST_VIEW            (gtk_list_view_get_type ())
#define GTK_LIST_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LIST_VIEW, GtkListView))
#define GTK_LIST_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LIST_VIEW, GtkListViewClass))
#define GTK_IS_LIST_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LIST_VIEW))
#define GTK_IS_LIST_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LIST_VIEW))
#define GTK_LIST_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LIST_VIEW, GtkListViewClass))

typedef struct _GtkListView           GtkListView;
typedef struct _GtkListViewClass      GtkListViewClass;
typedef struct _GtkListViewPrivate    GtkListViewPrivate;

#if 0
/**
 * GtkListViewForeachFunc:
 * @list_view: a #GtkListView
 * @path: The #GtkTreePath of a selected row
 * @data: user data
 *
 * A function used by gtk_list_view_selected_foreach() to map all
 * selected rows.  It will be called on every selected row in the view.
 */
typedef void (* GtkListViewForeachFunc)     (GtkListView      *list_view,
                                             GtkTreePath      *path,
                                             gpointer          data);
#endif

struct _GtkListView
{
  GtkContainer parent;

  /*< private >*/
  GtkListViewPrivate *priv;
};

struct _GtkListViewClass
{
  GtkContainerClass parent_class;

  void    (* item_activated)         (GtkListView      *list_view,
                                      GtkTreePath      *path);
  void    (* selection_changed)      (GtkListView      *list_view);

  /* Key binding signals */
  void    (* select_all)             (GtkListView      *list_view);
  void    (* unselect_all)           (GtkListView      *list_view);
  void    (* select_cursor_item)     (GtkListView      *list_view);
  void    (* toggle_cursor_item)     (GtkListView      *list_view);
  gboolean (* move_cursor)           (GtkListView      *list_view,
                                      GtkMovementStep   step,
                                      gint              count);
  gboolean (* activate_cursor_item)  (GtkListView      *list_view);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType                   gtk_list_view_get_type                  (void) G_GNUC_CONST;

GtkWidget *             gtk_list_view_new                       (void);
GtkWidget *             gtk_list_view_new_with_model            (GtkTreeModel            *model);

void                    gtk_list_view_set_model                 (GtkListView             *list_view,
                                                                 GtkTreeModel            *model);
GtkTreeModel *          gtk_list_view_get_model                 (GtkListView             *list_view);
void                    gtk_list_view_set_selection_mode        (GtkListView             *list_view,
                                                                 GtkSelectionMode         mode);
GtkSelectionMode        gtk_list_view_get_selection_mode        (GtkListView             *list_view);

gboolean                gtk_list_view_get_visible_range         (GtkListView             *list_view,
                                                                 GtkTreePath            **start_path,
                                                                 GtkTreePath            **end_path);

#if 0
/* for when we support selections, we want to imitate Iconview API */
void                    gtk_list_view_selected_foreach          (GtkListView             *list_view,
                                                                 GtkListViewForeachFunc   func,
                                                                 gpointer                 data);
void                    gtk_list_view_select_path               (GtkListView             *list_view,
                                                                 GtkTreePath             *path);
void                    gtk_list_view_unselect_path             (GtkListView             *list_view,
                                                                 GtkTreePath             *path);
gboolean                gtk_list_view_path_is_selected          (GtkListView             *list_view,
                                                                 GtkTreePath             *path);
GList *                 gtk_list_view_get_selected_items        (GtkListView             *list_view);
void                    gtk_list_view_select_all                (GtkListView             *list_view);
void                    gtk_list_view_unselect_all              (GtkListView             *list_view);
#endif


G_END_DECLS

#endif /* __GTK_LIST_VIEW_H__ */
