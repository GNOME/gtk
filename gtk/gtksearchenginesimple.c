/*
 * Copyright (C) 2005 Red Hat, Inc
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 * Based on nautilus-search-engine-simple.c
 */

#include "config.h"

#include <gio/gio.h>

#include <gdk/gdk.h>

#include "gtksearchenginesimple.h"
#include "gtkprivate.h"

#include <string.h>

#define BATCH_SIZE 500

typedef struct
{
  GtkSearchEngineSimple *engine;

  gchar *path;
  gchar **words;
  GList *found_list;

  gint n_processed_files;
  GList *uri_hits;

  /* accessed on both threads: */
  volatile gboolean cancelled;
} SearchThreadData;


struct _GtkSearchEngineSimplePrivate
{
  GtkQuery *query;

  SearchThreadData *active_search;

  gboolean query_finished;
};


G_DEFINE_TYPE_WITH_PRIVATE (GtkSearchEngineSimple, _gtk_search_engine_simple, GTK_TYPE_SEARCH_ENGINE)

static void
gtk_search_engine_simple_dispose (GObject *object)
{
  GtkSearchEngineSimple *simple;
  GtkSearchEngineSimplePrivate *priv;

  simple = GTK_SEARCH_ENGINE_SIMPLE (object);
  priv = simple->priv;

  if (priv->query)
    {
      g_object_unref (priv->query);
      priv->query = NULL;
    }

  if (priv->active_search)
    {
      priv->active_search->cancelled = TRUE;
      priv->active_search = NULL;
    }

  G_OBJECT_CLASS (_gtk_search_engine_simple_parent_class)->dispose (object);
}

static SearchThreadData *
search_thread_data_new (GtkSearchEngineSimple *engine,
			GtkQuery              *query)
{
  SearchThreadData *data;
  char *text, *lower, *uri;

  data = g_new0 (SearchThreadData, 1);

  data->engine = g_object_ref (engine);
  uri = _gtk_query_get_location (query);
  if (uri != NULL)
    {
      data->path = g_filename_from_uri (uri, NULL, NULL);
      g_free (uri);
    }
  if (data->path == NULL)
    data->path = g_strdup (g_get_home_dir ());

  text = _gtk_query_get_text (query);
  lower = g_utf8_casefold (text, -1);
  data->words = g_strsplit (lower, " ", -1);
  g_free (text);
  g_free (lower);

  return data;
}

static void
search_thread_data_free (SearchThreadData *data)
{
  g_object_unref (data->engine);
  g_free (data->path);
  g_strfreev (data->words);
  g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
  SearchThreadData *data;

  data = user_data;

  if (!data->cancelled)
    _gtk_search_engine_finished (GTK_SEARCH_ENGINE (data->engine));

  data->engine->priv->active_search = NULL;
  search_thread_data_free (data);

  return FALSE;
}

typedef struct
{
  GList *uris;
  SearchThreadData *thread_data;
} SearchHits;

static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
  SearchHits *hits;

  hits = user_data;

  if (!hits->thread_data->cancelled)
    {
      _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (hits->thread_data->engine),
				    hits->uris);
    }

  g_list_free_full (hits->uris, g_free);
  g_free (hits);

  return FALSE;
}

static void
send_batch (SearchThreadData *data)
{
  SearchHits *hits;

  data->n_processed_files = 0;

  if (data->uri_hits)
    {
      guint id;

      hits = g_new (SearchHits, 1);
      hits->uris = data->uri_hits;
      hits->thread_data = data;

      id = gdk_threads_add_idle (search_thread_add_hits_idle, hits);
      g_source_set_name_by_id (id, "[gtk+] search_thread_add_hits_idle");
    }

  data->uri_hits = NULL;
}

typedef gboolean (VisitFunc) (const char *fpath, gpointer user_data);

static gboolean process_dir    (GFile *dir, GList **new_root_dirs, VisitFunc func, gpointer user_data);
static GList *  process_dirs   (GList *root_dirs, VisitFunc func, gpointer user_data);
static void     breadth_search (gchar *root, VisitFunc func, gpointer user_data);

static void
breadth_search (gchar *root, VisitFunc func, gpointer user_data)
{
  GList *subdirs = NULL;

  subdirs = g_list_prepend (subdirs, g_file_new_for_path (root));

  while (subdirs)
    subdirs = process_dirs (subdirs, func, user_data);
}

static GList *
process_dirs (GList *root_dirs, VisitFunc func, gpointer user_data)
{
  SearchThreadData *data = (SearchThreadData*) user_data;
  GList *new_root_dirs = NULL;
  GList *root;
  gboolean keep_going = TRUE;

  for (root = root_dirs; root; root = g_list_next (root))
    {
      GFile *dir = (GFile *) root->data;

      /* Even if cancelled or stopped, we keep looping to unref the dirs */
      if (keep_going && !data->cancelled)
        keep_going = process_dir (dir, &new_root_dirs, func, user_data);

      g_object_unref (dir);
    }

  if (!keep_going || data->cancelled)
    {
      g_list_free_full (new_root_dirs, g_object_unref);
      new_root_dirs = NULL;
    }

  g_list_free (root_dirs);

  return g_list_reverse (new_root_dirs);
}

static gboolean
process_dir (GFile *dir, GList **new_root_dirs, VisitFunc func, gpointer user_data)
{
  GFileEnumerator *direnum;
  gchar *dirpath;
  SearchThreadData *data = (SearchThreadData*) user_data;

  direnum = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                       G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL, NULL);
  if (direnum == NULL || data->cancelled)
    return FALSE;

  dirpath = g_file_get_path (dir);

  while (TRUE)
    {
      GFileInfo *info;
      gchar *fullname;
      const gchar *basename;
      gboolean keep_going;

      keep_going = g_file_enumerator_iterate (direnum, &info, NULL, NULL, NULL);

      if (!keep_going || info == NULL || data->cancelled)
        {
          g_object_unref (direnum);
          g_free (dirpath);
          return keep_going;
        }

      if (g_file_info_get_is_hidden (info))
        continue;

      basename = g_file_info_get_name (info);
      fullname = g_build_filename (dirpath, basename, NULL);

      keep_going = func ((const char *) fullname, user_data);

      g_free (fullname);

      if (!keep_going)
        {
          g_object_unref (direnum);
          g_free (dirpath);
          return FALSE;
        }

      if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
        continue;

      *new_root_dirs = g_list_prepend (*new_root_dirs,
                                       g_file_get_child (dir, basename));
    }
}

static int
search_visit_func (const char *fpath, gpointer user_data)
{
  SearchThreadData *data;
  gint i;
  gchar *display_basename;
  gchar *lower_name;
  gchar *uri;
  gboolean hit;

  data = (SearchThreadData*) user_data;

  if (data->cancelled)
    return FALSE;

  display_basename = g_filename_display_basename (fpath);
  lower_name = g_utf8_casefold (display_basename, -1);
  g_free (display_basename);

  hit = TRUE;
  for (i = 0; data->words[i] != NULL; i++)
    {
      if (strstr (lower_name, data->words[i]) == NULL)
        {
          hit = FALSE;
          break;
        }
    }

  g_free (lower_name);

  if (hit)
    {
      uri = g_filename_to_uri (fpath, NULL, NULL);
      data->uri_hits = g_list_prepend (data->uri_hits, uri);
    }

  data->n_processed_files++;

  if (data->n_processed_files > BATCH_SIZE)
    send_batch (data);

  return TRUE;
}

static gpointer
search_thread_func (gpointer user_data)
{
  guint id;
  SearchThreadData *data;

  data = user_data;

  breadth_search (data->path, search_visit_func, data);

  send_batch (data);

  id = gdk_threads_add_idle (search_thread_done_idle, data);
  g_source_set_name_by_id (id, "[gtk+] search_thread_done_idle");

  return NULL;
}

static void
gtk_search_engine_simple_start (GtkSearchEngine *engine)
{
  GtkSearchEngineSimple *simple;
  SearchThreadData *data;

  simple = GTK_SEARCH_ENGINE_SIMPLE (engine);

  if (simple->priv->active_search != NULL)
    return;

  if (simple->priv->query == NULL)
    return;

  data = search_thread_data_new (simple, simple->priv->query);

  g_thread_unref (g_thread_new ("file-search", search_thread_func, data));

  simple->priv->active_search = data;
}

static void
gtk_search_engine_simple_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineSimple *simple;

  simple = GTK_SEARCH_ENGINE_SIMPLE (engine);

  if (simple->priv->active_search != NULL)
    {
      simple->priv->active_search->cancelled = TRUE;
      simple->priv->active_search = NULL;
    }
}

static void
gtk_search_engine_simple_set_query (GtkSearchEngine *engine,
				    GtkQuery        *query)
{
  GtkSearchEngineSimple *simple;

  simple = GTK_SEARCH_ENGINE_SIMPLE (engine);

  if (query)
    g_object_ref (query);

  if (simple->priv->query)
    g_object_unref (simple->priv->query);

  simple->priv->query = query;
}

static void
_gtk_search_engine_simple_class_init (GtkSearchEngineSimpleClass *class)
{
  GObjectClass *gobject_class;
  GtkSearchEngineClass *engine_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->dispose = gtk_search_engine_simple_dispose;

  engine_class = GTK_SEARCH_ENGINE_CLASS (class);
  engine_class->set_query = gtk_search_engine_simple_set_query;
  engine_class->start = gtk_search_engine_simple_start;
  engine_class->stop = gtk_search_engine_simple_stop;
}

static void
_gtk_search_engine_simple_init (GtkSearchEngineSimple *engine)
{
  engine->priv = _gtk_search_engine_simple_get_instance_private (engine);
}

GtkSearchEngine *
_gtk_search_engine_simple_new (void)
{
  return g_object_new (GTK_TYPE_SEARCH_ENGINE_SIMPLE, NULL);
}
