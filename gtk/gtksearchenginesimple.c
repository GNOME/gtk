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
  GCancellable *cancellable;

  GQueue *directories;

  gint n_processed_files;
  GList *hits;

  GtkQuery *query;
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
      g_cancellable_cancel (priv->active_search->cancellable);
      priv->active_search = NULL;
    }

  G_OBJECT_CLASS (_gtk_search_engine_simple_parent_class)->dispose (object);
}

static SearchThreadData *
search_thread_data_new (GtkSearchEngineSimple *engine,
			GtkQuery              *query)
{
  SearchThreadData *data;
  const gchar *uri;
  GFile *location;

  data = g_new0 (SearchThreadData, 1);

  data->engine = g_object_ref (engine);
  data->directories = g_queue_new ();
  data->query = g_object_ref (query);
  uri = _gtk_query_get_location (query);
  if (uri != NULL)
    location = g_file_new_for_uri (uri);
  else
    location = g_file_new_for_path (g_get_home_dir ());
  g_queue_push_tail (data->directories, location);

  data->cancellable = g_cancellable_new ();

  return data;
}

static void
search_thread_data_free (SearchThreadData *data)
{
  g_queue_foreach (data->directories, (GFunc)g_object_unref, NULL);
  g_queue_free (data->directories);
  g_object_unref (data->cancellable);
  g_object_unref (data->query);
  g_object_unref (data->engine);

  g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
  SearchThreadData *data;

  data = user_data;

  if (!g_cancellable_is_cancelled (data->cancellable))
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
  SearchHits *hits = user_data;

  if (!g_cancellable_is_cancelled (hits->thread_data->cancellable))
    _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (hits->thread_data->engine), hits->uris);

  g_list_free_full (hits->uris, g_free);
  g_free (hits);

  return FALSE;
}

static void
send_batch (SearchThreadData *data)
{
  SearchHits *hits;

  data->n_processed_files = 0;

  if (data->hits)
    {
      guint id;

      hits = g_new (SearchHits, 1);
      hits->uris = data->hits;
      hits->thread_data = data;

      id = gdk_threads_add_idle (search_thread_add_hits_idle, hits);
      g_source_set_name_by_id (id, "[gtk+] search_thread_add_hits_idle");
    }

  data->hits = NULL;
}

static void
visit_directory (GFile *dir, SearchThreadData *data)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GFile *child;
  const gchar *display_name;

  enumerator = g_file_enumerate_children (dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                          G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          data->cancellable, NULL);
  if (enumerator == NULL)
    return;

  while (g_file_enumerator_iterate (enumerator, &info, &child, data->cancellable, NULL))
    {
      if (info == NULL)
        break;

      display_name = g_file_info_get_display_name (info);
      if (display_name == NULL)
        continue;

      if (g_file_info_get_is_hidden (info))
        continue;

      if (gtk_query_matches_string (data->query, display_name))
        data->hits = g_list_prepend (data->hits, g_file_get_uri (child));

      data->n_processed_files++;
      if (data->n_processed_files > BATCH_SIZE)
        send_batch (data);

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        g_queue_push_tail (data->directories, g_object_ref (child));
    }

  g_object_unref (enumerator);
}

static gpointer
search_thread_func (gpointer user_data)
{
  SearchThreadData *data;
  GFile *dir;
  guint id;

  data = user_data;

  while (!g_cancellable_is_cancelled (data->cancellable) &&
         (dir = g_queue_pop_head (data->directories)) != NULL)
    {
      visit_directory (dir, data);
      g_object_unref (dir);
    }

  if (!g_cancellable_is_cancelled (data->cancellable))
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
      g_cancellable_cancel (simple->priv->active_search->cancellable);
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
