/* gtkiconcache.h
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
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
#ifndef __GTK_ICON_CACHE_PRIVATE_H__
#define __GTK_ICON_CACHE_PRIVATE_H__

#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct _GtkIconCache GtkIconCache;

GtkIconCache *gtk_icon_cache_new                        (const gchar  *data);
GtkIconCache *gtk_icon_cache_new_for_path               (const gchar  *path);
gint          gtk_icon_cache_get_directory_index        (GtkIconCache *cache,
                                                         const gchar  *directory);
gboolean      gtk_icon_cache_has_icon                   (GtkIconCache *cache,
                                                         const gchar  *icon_name);
gboolean      gtk_icon_cache_has_icon_in_directory      (GtkIconCache *cache,
                                                         const gchar  *icon_name,
                                                         const gchar  *directory);
GHashTable   *gtk_icon_cache_list_icons_in_directory    (GtkIconCache *cache,
                                                         const gchar  *directory);
gboolean      gtk_icon_cache_has_icons                  (GtkIconCache *cache,
                                                         const gchar  *directory);
void	      gtk_icon_cache_add_icons                  (GtkIconCache *cache,
                                                         const gchar  *directory,
                                                         GHashTable   *hash_table);

gint          gtk_icon_cache_get_icon_flags             (GtkIconCache *cache,
                                                         const gchar  *icon_name,
                                                         gint          directory_index);
GdkPixbuf    *gtk_icon_cache_get_icon                   (GtkIconCache *cache,
                                                         const gchar  *icon_name,
                                                         gint          directory_index);

GtkIconCache *gtk_icon_cache_ref                        (GtkIconCache *cache);
void          gtk_icon_cache_unref                      (GtkIconCache *cache);

G_END_DECLS

#endif /* __GTK_ICON_CACHE_PRIVATE_H__ */
