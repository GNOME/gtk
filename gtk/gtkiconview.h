/* eggiconlist.h
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
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

G_BEGIN_DECLS

#define EGG_TYPE_ICON_LIST		(egg_icon_list_get_type ())
#define EGG_ICON_LIST(obj)		(GTK_CHECK_CAST ((obj), EGG_TYPE_ICON_LIST, EggIconList))
#define EGG_ICON_LIST_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EGG_TYPE_ICON_LIST, EggIconListClass))
#define EGG_IS_ICON_LIST(obj)		(GTK_CHECK_TYPE ((obj), EGG_TYPE_ICON_LIST))
#define EGG_IS_ICON_LIST_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), EGG_TYPE_ICON_LIST))
#define EGG_ICON_LIST_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), EGG_TYPE_ICON_LIST, EggIconListClass))

#define EGG_TYPE_ICON_LIST_ITEM		(egg_icon_list_item_get_type ())

typedef struct _EggIconList           EggIconList;
typedef struct _EggIconListClass      EggIconListClass;
typedef struct _EggIconListPrivate    EggIconListPrivate;
typedef struct _EggIconListItem       EggIconListItem;

typedef void (* EggIconListForeachFunc)     (EggIconList      *icon_list,
					     EggIconListItem  *item,
					     gpointer          data);
typedef gint (* EggIconListItemCompareFunc) (EggIconList      *icon_list,
					     EggIconListItem  *a,
					     EggIconListItem  *b,
					     gpointer          user_data);

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
				      EggIconListItem  *item);
  void    (* selection_changed)      (EggIconList      *icon_list);
  void    (* item_added)             (EggIconList      *icon_list,
				      EggIconListItem  *item);
  void    (* item_removed)           (EggIconList      *icon_list,
				      EggIconListItem  *item);
  void    (* item_changed)           (EggIconList      *icon_list,
				      EggIconListItem  *item);
  /* Key binding signals */
  void    (* select_all)             (EggIconList      *icon_list);
  void    (* unselect_all)           (EggIconList      *icon_list);
  void    (* select_cursor_item)     (EggIconList      *icon_list);
  void    (* toggle_cursor_item)     (EggIconList      *icon_list);
  gboolean (* move_cursor)           (EggIconList      *icon_list,
				      GtkMovementStep   step,
				      gint              count);
};

GType      egg_icon_list_get_type      (void);
GType      egg_icon_list_item_get_type (void);
GtkWidget *egg_icon_list_new           (void);

EggIconListItem *     egg_icon_list_item_new           (GdkPixbuf                  *icon,
							const gchar                *label);
void                  egg_icon_list_item_ref           (EggIconListItem            *item);
void                  egg_icon_list_item_unref         (EggIconListItem            *item);
void                  egg_icon_list_item_set_data      (EggIconListItem            *item,
							gpointer                    data);
void                  egg_icon_list_item_set_data_full (EggIconListItem            *item,
							gpointer                    data,
							GDestroyNotify              destroy_notify);
gpointer              egg_icon_list_item_get_data      (EggIconListItem            *item);
void                  egg_icon_list_item_set_label     (EggIconListItem            *item,
							const char                 *label);
G_CONST_RETURN gchar *egg_icon_list_item_get_label     (EggIconListItem            *item);
void                  egg_icon_list_item_set_icon      (EggIconListItem            *item,
							GdkPixbuf                  *icon);
GdkPixbuf *           egg_icon_list_item_get_icon      (EggIconListItem            *item);
void                  egg_icon_list_append_item        (EggIconList                *icon_list,
							EggIconListItem            *item);
void                  egg_icon_list_prepend_item       (EggIconList                *icon_list,
							EggIconListItem            *item);
void                  egg_icon_list_insert_item_before (EggIconList                *icon_list,
							EggIconListItem            *sibling,
							EggIconListItem            *item);
void                  egg_icon_list_insert_item_after  (EggIconList                *icon_list,
							EggIconListItem            *sibling,
							EggIconListItem            *item);
void                  egg_icon_list_remove_item        (EggIconList                *icon_list,
							EggIconListItem            *item);
void                  egg_icon_list_clear              (EggIconList                *icon_list);
EggIconListItem *     egg_icon_list_get_item_at_pos    (EggIconList                *icon_list,
							gint                        x,
							gint                        y);
gint                  egg_icon_list_get_item_count     (EggIconList                *icon_list);
void                  egg_icon_list_foreach            (EggIconList                *icon_list,
							EggIconListForeachFunc      func,
							gpointer                    data);
GList *               egg_icon_list_get_selected       (EggIconList                *icon_list);
void                  egg_icon_list_selected_foreach   (EggIconList                *icon_list,
							EggIconListForeachFunc      func,
							gpointer                    data);
void                  egg_icon_list_set_selection_mode (EggIconList                *icon_list,
							GtkSelectionMode            mode);
GtkSelectionMode      egg_icon_list_get_selection_mode (EggIconList                *icon_list);
void                  egg_icon_list_select_item        (EggIconList                *icon_list,
							EggIconListItem            *item);
void                  egg_icon_list_unselect_item      (EggIconList                *icon_list,
							EggIconListItem            *item);
gboolean              egg_icon_list_item_is_selected   (EggIconListItem            *item);
void                  egg_icon_list_select_all         (EggIconList                *icon_list);
void                  egg_icon_list_unselect_all       (EggIconList                *icon_list);
void                  egg_icon_list_set_sorted         (EggIconList                *icon_list,
							gboolean                    sorted);
gboolean              egg_icon_list_get_sorted         (EggIconList                *icon_list);
void                  egg_icon_list_set_sort_func      (EggIconList                *icon_list,
							EggIconListItemCompareFunc  func,
							gpointer                    data,
							GDestroyNotify              destroy_notify);
void                  egg_icon_list_set_sort_order     (EggIconList                *icon_list,
							GtkSortType                 order);
GtkSortType           egg_icon_list_get_sort_order     (EggIconList                *icon_list);
void                  egg_icon_list_item_activated     (EggIconList                *icon_list,
							EggIconListItem            *item);

/* For accessibility */
GList                *egg_icon_list_get_items          (EggIconList                *icon_list);

G_END_DECLS

#endif /* __EGG_ICON_LIST_H__ */
