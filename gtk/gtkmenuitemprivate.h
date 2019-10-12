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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_MENU_ITEM_PRIVATE_H__
#define __GTK_MENU_ITEM_PRIVATE_H__

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkactionhelperprivate.h>
#include <gtk/gtkcssnodeprivate.h>
#include <gtk/gtkeventcontrollermotion.h>

G_BEGIN_DECLS

struct _GtkMenuItemPrivate
{
  GtkWidget *submenu;

  guint16 toggle_size;
  guint16 accelerator_width;

  guint timer;

  const char *accel_path;

  GtkActionHelper *action_helper;

  GtkWidget *arrow_widget;

  guint submenu_placement      : 1;
  guint submenu_direction      : 1;
  guint from_menubar           : 1;
  guint reserve_indicator      : 1;
};

void     _gtk_menu_item_refresh_accel_path   (GtkMenuItem   *menu_item,
                                              const gchar   *prefix,
                                              GtkAccelGroup *accel_group,
                                              gboolean       group_changed);
gboolean _gtk_menu_item_is_selectable        (GtkWidget     *menu_item);
void     _gtk_menu_item_popup_submenu        (GtkWidget     *menu_item,
                                              gboolean       with_delay);
void     _gtk_menu_item_popdown_submenu      (GtkWidget     *menu_item);
GtkMenuShell *
         gtk_menu_item_get_menu_shell        (GtkMenuItem   *menu_item);

G_END_DECLS

#endif /* __GTK_MENU_ITEM_PRIVATE_H__ */
