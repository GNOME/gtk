/* eggiconlist.h
 * Copyright (C) 2002, 2004  Anders Carlsson <andersca@gnome.org>
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
#ifndef __EGG_ICON_LIST_H__
#define __EGG_ICON_LIST_H__

#include <gtk/gtkcontainer.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define EGG_TYPE_ICON_LIST		(egg_icon_list_get_type ())
#define EGG_ICON_LIST(obj)		(GTK_CHECK_CAST ((obj), EGG_TYPE_ICON_LIST, EggIconList))
#define EGG_ICON_LIST_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EGG_TYPE_ICON_LIST, EggIconListClass))
#define EGG_IS_ICON_LIST(obj)		(GTK_CHECK_TYPE ((obj), EGG_TYPE_ICON_LIST))
#define EGG_IS_ICON_LIST_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), EGG_TYPE_ICON_LIST))
#define EGG_ICON_LIST_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), EGG_TYPE_ICON_LIST, EggIconListClass))

typedef struct _EggIconList           EggIconList;
typedef struct _EggIconListClass      EggIconListClass;
typedef struct _EggIconListPrivate    EggIconListPrivate;
typedef struct _EggIconListItem       EggIconListItem;

typedef void (* EggIconListForeachFunc)     (EggIconList      *icon_list,
					     GtkTreePath      *path,
					     gpointer          data);

struct _EggIconList
{
  GtkContainer parent;

  EggIconListPrivate *priv;
};

struct _EggIconListClass
{
  GtkContainerClass parent_class;

  void    (* set_scroll_adjustments) (EggIconList      *icon_list,
				      GtkAdjustment    *hadjustment,
				      GtkAdjustment    *vadjustment);
  
  void    (* item_activated)         (EggIconList      *icon_list,
				      GtkTreePath      *path);
  void    (* selection_changed)      (EggIconList      *icon_list);

  /* Key binding signals */
  void    (* select_all)             (EggIconList      *icon_list);
  void    (* unselect_all)           (EggIconList      *icon_list);
  void    (* select_cursor_item)     (EggIconList      *icon_list);
  void    (* toggle_cursor_item)     (EggIconList      *icon_list);
  gboolean (* move_cursor)           (EggIconList      *icon_list,
				      GtkMovementStep   step,
				      gint              count);
};

GType      egg_icon_list_get_type       (void);
GtkWidget *egg_icon_list_new            (void);
GtkWidget *egg_icon_list_new_with_model (GtkTreeModel *model);

void          egg_icon_list_set_model         (EggIconList  *icon_list,
					       GtkTreeModel *model);
GtkTreeModel *egg_icon_list_get_model         (EggIconList  *icon_list);
void          egg_icon_list_set_text_column   (EggIconList  *icon_list,
					       gint          column);
gint          egg_icon_list_get_text_column   (EggIconList  *icon_list);
void          egg_icon_list_set_markup_column (EggIconList  *icon_list,
					       int           column);
gint          egg_icon_list_get_markup_column (EggIconList  *icon_list);
void          egg_icon_list_set_pixbuf_column (EggIconList  *icon_list,
					       gint          column);
gint          egg_icon_list_get_pixbuf_column (EggIconList  *icon_list);


GtkTreePath *    egg_icon_list_get_path_at_pos    (EggIconList            *icon_list,
						   gint                    x,
						   gint                    y);
void             egg_icon_list_selected_foreach   (EggIconList            *icon_list,
						   EggIconListForeachFunc  func,
						   gpointer                data);
void             egg_icon_list_set_selection_mode (EggIconList            *icon_list,
						   GtkSelectionMode        mode);
GtkSelectionMode egg_icon_list_get_selection_mode (EggIconList            *icon_list);
void             egg_icon_list_select_path        (EggIconList            *icon_list,
						   GtkTreePath            *path);
void             egg_icon_list_unselect_path      (EggIconList            *icon_list,
						   GtkTreePath            *path);
gboolean         egg_icon_list_path_is_selected   (EggIconList            *icon_list,
						   GtkTreePath            *path);
GList           *egg_icon_list_get_selected_items (EggIconList            *icon_list);
void             egg_icon_list_select_all         (EggIconList            *icon_list);
void             egg_icon_list_unselect_all       (EggIconList            *icon_list);
void             egg_icon_list_item_activated     (EggIconList            *icon_list,
						   GtkTreePath            *path);



G_END_DECLS

#endif /* __EGG_ICON_LIST_H__ */
