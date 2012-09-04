/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkbookmarksmanager.c: Utilities to manage and monitor ~/.gtk-bookmarks
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Federico Mena Quintero <federico@gnome.org>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "gtkbookmarksmanager.h"
#include "gtkfilechooser.h" /* for the GError types */

static void
_gtk_bookmark_free (GtkBookmark *bookmark)
{
  g_object_unref (bookmark->file);
  g_free (bookmark->label);
  g_slice_free (GtkBookmark, bookmark);
}

static GFile *
get_legacy_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GFile *
get_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GSList *
read_bookmarks (GFile *file)
{
  gchar *contents;
  gchar **lines, *space;
  GSList *bookmarks = NULL;
  gint i;

  if (!g_file_load_contents (file, NULL, &contents,
			     NULL, NULL, NULL))
    return NULL;

  lines = g_strsplit (contents, "\n", -1);

  for (i = 0; lines[i]; i++)
    {
      GtkBookmark *bookmark;

      if (!*lines[i])
	continue;

      if (!g_utf8_validate (lines[i], -1, NULL))
	continue;

      bookmark = g_slice_new0 (GtkBookmark);

      if ((space = strchr (lines[i], ' ')) != NULL)
	{
	  space[0] = '\0';
	  bookmark->label = g_strdup (space + 1);
	}

      bookmark->file = g_file_new_for_uri (lines[i]);
      bookmarks = g_slist_prepend (bookmarks, bookmark);
    }

  bookmarks = g_slist_reverse (bookmarks);
  g_strfreev (lines);
  g_free (contents);

  return bookmarks;
}

static void
save_bookmarks (GFile  *bookmarks_file,
		GSList *bookmarks)
{
  GError *error = NULL;
  GString *contents;
  GSList *l;

  contents = g_string_new ("");

  for (l = bookmarks; l; l = l->next)
    {
      GtkBookmark *bookmark = l->data;
      gchar *uri;

      uri = g_file_get_uri (bookmark->file);
      if (!uri)
	continue;

      g_string_append (contents, uri);

      if (bookmark->label)
	g_string_append_printf (contents, " %s", bookmark->label);

      g_string_append_c (contents, '\n');
      g_free (uri);
    }

  if (!g_file_replace_contents (bookmarks_file,
				contents->str,
				strlen (contents->str),
				NULL, FALSE, 0, NULL,
				NULL, &error))
    {
      g_critical ("%s", error->message);
      g_error_free (error);
    }

  g_string_free (contents, TRUE);
}

static void
notify_changed (GtkBookmarksManager *manager)
{
  manager->changed_func (manager->changed_func_data);
}

static void
bookmarks_file_changed (GFileMonitor      *monitor,
			GFile             *file,
			GFile             *other_file,
			GFileMonitorEvent  event,
			gpointer           data)
{
  GtkBookmarksManager *manager = data;

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
      g_slist_foreach (manager->bookmarks, (GFunc) _gtk_bookmark_free, NULL);
      g_slist_free (manager->bookmarks);

      manager->bookmarks = read_bookmarks (file);

      gdk_threads_enter ();
      notify_changed (manager);
      gdk_threads_leave ();
      break;

    default:
      /* ignore at the moment */
      break;
    }
}

GtkBookmarksManager *
_gtk_bookmarks_manager_new (GtkBookmarksChangedFunc changed_func, gpointer changed_func_data)
{
  GtkBookmarksManager *manager;
  GFile *bookmarks_file;
  GError *error;

  g_return_val_if_fail (changed_func != NULL, NULL);

  manager = g_new0 (GtkBookmarksManager, 1);

  manager->changed_func = changed_func;
  manager->changed_func_data = changed_func_data;

  bookmarks_file = get_bookmarks_file ();
  manager->bookmarks = read_bookmarks (bookmarks_file);
  if (!manager->bookmarks)
    {
      GFile *legacy_bookmarks_file;

      /* Read the legacy one and write it to the new one */
      legacy_bookmarks_file = get_legacy_bookmarks_file ();
      manager->bookmarks = read_bookmarks (legacy_bookmarks_file);
      save_bookmarks (bookmarks_file, manager->bookmarks);

      g_object_unref (legacy_bookmarks_file);
    }

  error = NULL;
  manager->bookmarks_monitor = g_file_monitor_file (bookmarks_file,
						    G_FILE_MONITOR_NONE,
						    NULL, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    manager->bookmarks_monitor_changed_id = g_signal_connect (manager->bookmarks_monitor, "changed",
							      G_CALLBACK (bookmarks_file_changed), manager);

  g_object_unref (bookmarks_file);

  return manager;
}

void
_gtk_bookmarks_manager_free (GtkBookmarksManager *manager)
{
  g_return_if_fail (manager != NULL);

  if (manager->bookmarks_monitor)
    {
      g_file_monitor_cancel (manager->bookmarks_monitor);
      g_signal_handler_disconnect (manager->bookmarks_monitor, manager->bookmarks_monitor_changed_id);
      manager->bookmarks_monitor_changed_id = 0;
      g_object_unref (manager->bookmarks_monitor);
    }

  if (manager->bookmarks)
    {
      g_slist_foreach (manager->bookmarks, (GFunc) _gtk_bookmark_free, NULL);
      g_slist_free (manager->bookmarks);
    }

  g_free (manager);
}

GSList *
_gtk_bookmarks_manager_list_bookmarks (GtkBookmarksManager *manager)
{
  GSList *bookmarks, *files = NULL;

  g_return_val_if_fail (manager != NULL, NULL);

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      GtkBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      files = g_slist_prepend (files, g_object_ref (bookmark->file));
    }

  return g_slist_reverse (files);
}

gboolean
_gtk_bookmarks_manager_insert_bookmark (GtkBookmarksManager *manager,
					GFile               *file,
					gint                 position,
					GError             **error)
{
  GSList *bookmarks;
  GtkBookmark *bookmark;
  gboolean result = TRUE;
  GFile *bookmarks_file;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (bookmark->file, file))
	{
	  /* File is already in bookmarks */
	  result = FALSE;
	  break;
	}
    }

  if (!result)
    {
      gchar *uri = g_file_get_uri (file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS,
		   "%s already exists in the bookmarks list",
		   uri);

      g_free (uri);

      return FALSE;
    }

  bookmark = g_slice_new0 (GtkBookmark);
  bookmark->file = g_object_ref (file);

  manager->bookmarks = g_slist_insert (manager->bookmarks, bookmark, position);

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  notify_changed (manager);

  return TRUE;
}

gboolean
_gtk_bookmarks_manager_remove_bookmark (GtkBookmarksManager *manager,
					GFile               *file,
					GError             **error)
{
  GtkBookmark *bookmark;
  GSList *bookmarks;
  gboolean result = FALSE;
  GFile *bookmarks_file;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!manager->bookmarks)
    return FALSE;

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      bookmark = bookmarks->data;

      if (g_file_equal (bookmark->file, file))
	{
	  result = TRUE;
	  manager->bookmarks = g_slist_remove_link (manager->bookmarks, bookmarks);
	  _gtk_bookmark_free (bookmark);
	  g_slist_free_1 (bookmarks);
	  break;
	}

      bookmarks = bookmarks->next;
    }

  if (!result)
    {
      gchar *uri = g_file_get_uri (file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
		   "%s does not exist in the bookmarks list",
		   uri);

      g_free (uri);

      return FALSE;
    }

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  notify_changed (manager);

  return TRUE;
}

gchar *
_gtk_bookmarks_manager_get_bookmark_label (GtkBookmarksManager *manager,
					   GFile               *file)
{
  GSList *bookmarks;
  gchar *label = NULL;

  g_return_val_if_fail (manager != NULL, NULL);
  g_return_val_if_fail (file != NULL, NULL);

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      GtkBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
	  label = g_strdup (bookmark->label);
	  break;
	}
    }

  return label;
}

void
_gtk_bookmarks_manager_set_bookmark_label (GtkBookmarksManager *manager,
					   GFile               *file,
					   const gchar         *label)
{
  gboolean changed = FALSE;
  GFile *bookmarks_file;
  GSList *bookmarks;

  g_return_if_fail (manager != NULL);
  g_return_if_fail (file != NULL);

  bookmarks = manager->bookmarks;

  while (bookmarks)
    {
      GtkBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
          g_free (bookmark->label);
	  bookmark->label = g_strdup (label);
	  changed = TRUE;
	  break;
	}
    }

  bookmarks_file = get_bookmarks_file ();
  save_bookmarks (bookmarks_file, manager->bookmarks);
  g_object_unref (bookmarks_file);

  if (changed)
    notify_changed (manager);
}
