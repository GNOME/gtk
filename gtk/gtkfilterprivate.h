/*
 * Copyright © 2025 Igalia S.L.
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
 * Authors: Georges Basile Stavracas Neto <feaneron@igalia.com>
 */

#pragma once

#include "gtkfilter.h"

#include <gtk/gtkexpression.h>

G_BEGIN_DECLS

/*<private>
 * GtkFilterWatchCallback:
 * @item: (type GObject): the item to be watched
 * @user_data: user data
 *
 * User function that is called @item changes while being watches.
 *
 * Since: 4.20
 */
typedef void (*GtkFilterWatchCallback) (gpointer item,
                                        gpointer user_data);

typedef struct _GtkFilterClassPrivate
{
  /* private vfuncs */
  gpointer              (* watch)                               (GtkFilter              *self,
                                                                 gpointer                item,
                                                                 GtkFilterWatchCallback  watch_func,
                                                                 gpointer                user_data,
                                                                 GDestroyNotify          destroy);

  void                  (* unwatch)                             (GtkFilter              *self,
                                                                 gpointer                watch);
} GtkFilterClassPrivate;

gpointer gtk_filter_watch (GtkFilter              *self,
                           gpointer                item,
                           GtkFilterWatchCallback  watch_func,
                           gpointer                user_data,
                           GDestroyNotify          destroy);

void gtk_filter_unwatch (GtkFilter *self,
                         gpointer   watch);

G_END_DECLS
