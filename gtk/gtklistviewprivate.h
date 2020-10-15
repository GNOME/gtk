/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_LIST_VIEW_PRIVATE_H__
#define __GTK_LIST_VIEW_PRIVATE_H__

#include <gtk/gtklistview.h>
#include <gtk/gtklistbaseprivate.h>

G_BEGIN_DECLS

struct _GtkListView
{
  GtkListBase parent_instance;

  GtkListItemManager *item_manager;
  gboolean show_separators;

  int list_width;
};

struct _GtkListViewClass
{
  GtkListBaseClass parent_class;
};

G_END_DECLS

#endif  /* __GTK_LIST_VIEW_H__ */
