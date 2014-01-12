/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkbookmarksmanager.h: Utilities to manage and monitor ~/.gtk-bookmarks
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2007-2008 Carlos Garnacho
 * Copyright (C) 2011 Suse
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Federico Mena Quintero <federico@gnome.org>
 */

#ifndef __GTK_BOOKMARKS_MANAGER_H__
#define __GTK_BOOKMARKS_MANAGER_H__

#include <gio/gio.h>

typedef void (* GtkBookmarksChangedFunc) (gpointer data);

typedef struct
{
  /* This list contains GtkBookmark structs */
  GSList *bookmarks;

  GFileMonitor *bookmarks_monitor;
  gulong bookmarks_monitor_changed_id;

  gpointer changed_func_data;
  GtkBookmarksChangedFunc changed_func;
} GtkBookmarksManager;

typedef struct
{
  GFile *file;
  gchar *label;
} GtkBookmark;

GtkBookmarksManager *_gtk_bookmarks_manager_new (GtkBookmarksChangedFunc changed_func,
						 gpointer                changed_func_data);


void _gtk_bookmarks_manager_free (GtkBookmarksManager *manager);

GSList *_gtk_bookmarks_manager_list_bookmarks (GtkBookmarksManager *manager);

gboolean _gtk_bookmarks_manager_insert_bookmark (GtkBookmarksManager *manager,
						 GFile               *file,
						 gint                 position,
						 GError             **error);

gboolean _gtk_bookmarks_manager_remove_bookmark (GtkBookmarksManager *manager,
						 GFile               *file,
						 GError             **error);

gboolean _gtk_bookmarks_manager_reorder_bookmark (GtkBookmarksManager *manager,
						  GFile               *file,
						  gint                 new_position,
						  GError             **error);

gboolean _gtk_bookmarks_manager_has_bookmark (GtkBookmarksManager *manager,
                                              GFile               *file);

gchar * _gtk_bookmarks_manager_get_bookmark_label (GtkBookmarksManager *manager,
						   GFile               *file);

gboolean _gtk_bookmarks_manager_set_bookmark_label (GtkBookmarksManager *manager,
						    GFile               *file,
						    const gchar         *label,
						    GError             **error);

gboolean _gtk_bookmarks_manager_get_xdg_type (GtkBookmarksManager *manager,
                                              GFile               *file,
                                              GUserDirectory      *directory);
gboolean _gtk_bookmarks_manager_get_is_builtin (GtkBookmarksManager *manager,
                                                GFile               *file);

gboolean _gtk_bookmarks_manager_get_is_xdg_dir_builtin (GUserDirectory xdg_type);

#endif /* __GTK_BOOKMARKS_MANAGER_H__ */
