/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 * Based on nautilus-search-engine.c
 */

#include "config.h"
#include "gtksearchengine.h"
#include "gtksearchenginesimple.h"
#include "gtksearchenginetracker.h"
#include "gtksearchenginemodel.h"
#include "gtksearchenginequartz.h"
#include "gtkintl.h"

#include <gdk/gdk.h> /* for GDK_WINDOWING_QUARTZ */

#ifndef G_OS_WIN32  /* No tracker on Windows */
#define HAVE_TRACKER 1
#endif

struct _GtkSearchEnginePrivate {
  GtkSearchEngine *native;
  gboolean native_running;
  gchar *native_error;

  GtkSearchEngine *simple;
  gboolean simple_running;
  gchar *simple_error;

  GtkSearchEngine *model;
  gboolean model_running;
  gchar *model_error;

  gboolean running;
  gboolean recursive;
  GHashTable *hits;

  GtkQuery *query;
};

enum
{
  HITS_ADDED,
  FINISHED,
  ERROR,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (GtkSearchEngine, _gtk_search_engine, G_TYPE_OBJECT);

static void
set_query (GtkSearchEngine *engine,
           GtkQuery        *query)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  g_set_object (&priv->query, query);

  if (priv->native)
    _gtk_search_engine_set_query (priv->native, query);

  if (priv->simple)
    _gtk_search_engine_set_query (priv->simple, query);

  if (priv->model)
    _gtk_search_engine_set_query (priv->model, query);
}

static void
start (GtkSearchEngine *engine)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  g_hash_table_remove_all (priv->hits);

  if (priv->native)
    {
      g_clear_pointer (&priv->native_error, g_free);
      _gtk_search_engine_start (priv->native);
      priv->native_running = TRUE;
    }

  if (priv->simple)
    {
      g_clear_pointer (&priv->simple_error, g_free);
      _gtk_search_engine_start (priv->simple);
      priv->simple_running = TRUE;
    }

  if (priv->model)
    {
      g_clear_pointer (&priv->model_error, g_free);
      _gtk_search_engine_start (priv->model);
      priv->model_running = TRUE;
    }

  priv->running = TRUE;
}

static void
stop (GtkSearchEngine *engine)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  if (priv->native)
    {
      _gtk_search_engine_stop (priv->native);
      priv->native_running = FALSE;
    }

  if (priv->simple)
    {
      _gtk_search_engine_stop (priv->simple);
      priv->simple_running = FALSE;
    }

  if (priv->model)
    {
      _gtk_search_engine_stop (priv->model);
      priv->model_running = FALSE;
    }

  priv->running = FALSE;

  g_hash_table_remove_all (priv->hits);
}

static void
finalize (GObject *object)
{
  GtkSearchEngine *engine = GTK_SEARCH_ENGINE (object);
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  g_clear_object (&priv->native);
  g_free (priv->native_error);

  g_clear_object (&priv->simple);
  g_free (priv->simple_error);

  g_clear_object (&priv->model);
  g_free (priv->model_error);

  g_clear_pointer (&priv->hits, g_hash_table_unref);

  g_clear_object (&priv->query);

  G_OBJECT_CLASS (_gtk_search_engine_parent_class)->finalize (object);
}

static void
_gtk_search_engine_class_init (GtkSearchEngineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = finalize;

  class->set_query = set_query;
  class->start = start;
  class->stop = stop;

  signals[HITS_ADDED] =
    g_signal_new (I_("hits-added"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSearchEngineClass, hits_added),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  signals[FINISHED] =
    g_signal_new (I_("finished"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSearchEngineClass, finished),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  signals[ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSearchEngineClass, error),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
}

static void
_gtk_search_engine_init (GtkSearchEngine *engine)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  priv->recursive = TRUE;
}

static void
hits_added (GtkSearchEngine *engine,
            GList           *hits,
            gpointer         data)
{
  GtkSearchEngine *composite = GTK_SEARCH_ENGINE (data);
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (composite);
  GList *added, *l;
  GtkSearchHit *hit;

  added = NULL;

  for (l = hits; l; l = l->next)
    {
      hit = l->data;

      if (!g_hash_table_contains (priv->hits, hit))
        {
          hit = _gtk_search_hit_dup (hit);
          g_hash_table_add (priv->hits, hit);
          added = g_list_prepend (added, hit);
        }
    }

  if (added)
    {
      _gtk_search_engine_hits_added (composite, added);
      g_list_free (added);
    }
}

static void
update_status (GtkSearchEngine *engine)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);
  gboolean running;

  running = priv->native_running || priv->simple_running;

  if (running != priv->running)
    {
      priv->running = running;

      if (!running)
        {
          if (priv->native_error)
            _gtk_search_engine_error (engine, priv->native_error);
          else if (priv->simple_error)
            _gtk_search_engine_error (engine, priv->simple_error);
          else if (priv->model_error)
            _gtk_search_engine_error (engine, priv->model_error);
          else
            _gtk_search_engine_finished (engine);
        }
    }
}

static void
finished (GtkSearchEngine *engine,
          gpointer         data)
{
  GtkSearchEngine *composite = GTK_SEARCH_ENGINE (data);
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (composite);

  if (engine == priv->native)
    priv->native_running = FALSE;
  else if (engine == priv->simple)
    priv->simple_running = FALSE;
  else if (engine == priv->model)
    priv->model_running = FALSE;

  update_status (composite);
}

static void
error (GtkSearchEngine *engine,
       const gchar     *message,
       gpointer         data)
{
  GtkSearchEngine *composite = GTK_SEARCH_ENGINE (data);
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (composite);

  if (engine == priv->native)
    {
      g_free (priv->native_error);
      priv->native_error = g_strdup (message);
      priv->native_running = FALSE;
    }
  else if (engine == priv->simple)
    {
      g_free (priv->simple_error);
      priv->simple_error = g_strdup (message);
      priv->simple_running = FALSE;
    }
  else if (engine == priv->model)
    {
      g_free (priv->model_error);
      priv->model_error = g_strdup (message);
      priv->model_running = FALSE;
    }

  update_status (composite);
}

static gboolean
search_hit_equal (gconstpointer a, gconstpointer b)
{
  const GtkSearchHit *ha = (const GtkSearchHit *)a;
  const GtkSearchHit *hb = (const GtkSearchHit *)b;

  return g_file_equal (ha->file, hb->file);
}


static guint
search_hit_hash (gconstpointer a)
{
  const GtkSearchHit *ha = (const GtkSearchHit *)a;

  return g_file_hash (ha->file);
}

GtkSearchHit *
_gtk_search_hit_dup (GtkSearchHit *hit)
{
  GtkSearchHit *dup;

  dup = g_new (GtkSearchHit, 1);
  dup->file = g_object_ref (hit->file);
  if (hit->info)
    dup->info = g_object_ref (hit->info);
  else
    dup->info = NULL;

  return dup;
}

void
_gtk_search_hit_free (GtkSearchHit *hit)
{
  g_clear_object (&hit->file);
  g_clear_object (&hit->info);
  g_free (hit);
}

static void
connect_engine_signals (GtkSearchEngine *engine,
                        gpointer         data)
{
  g_signal_connect_object (engine, "hits-added", G_CALLBACK (hits_added), data, 0);
  g_signal_connect_object (engine, "finished", G_CALLBACK (finished), data, 0);
  g_signal_connect_object (engine, "error", G_CALLBACK (error), data, 0);
}

GtkSearchEngine *
_gtk_search_engine_new (void)
{
  GtkSearchEngine *engine;
  GtkSearchEnginePrivate *priv;

  engine = g_object_new (GTK_TYPE_SEARCH_ENGINE, NULL);
  priv = _gtk_search_engine_get_instance_private (engine);

  priv->simple = _gtk_search_engine_simple_new ();
  g_debug ("Using simple search engine");
  connect_engine_signals (priv->simple, engine);

#ifdef HAVE_TRACKER
  priv->native = _gtk_search_engine_tracker_new ();
  if (priv->native)
    {
      g_debug ("Using Tracker search engine");
      connect_engine_signals (priv->native, engine);
      _gtk_search_engine_simple_set_indexed_cb (GTK_SEARCH_ENGINE_SIMPLE (priv->simple),
                                                _gtk_search_engine_tracker_is_indexed,
                                                g_object_ref (priv->native),
                                                g_object_unref);
    }
#endif

#ifdef GDK_WINDOWING_QUARTZ
  priv->native = _gtk_search_engine_quartz_new ();
  if (priv->native)
    {
      g_debug ("Using Quartz search engine");
      connect_engine_signals (priv->native, engine);
    }
#endif

  priv->hits = g_hash_table_new_full (search_hit_hash, search_hit_equal,
                                      (GDestroyNotify)_gtk_search_hit_free, NULL);

  return engine;
}

void
_gtk_search_engine_set_query (GtkSearchEngine *engine,
                              GtkQuery        *query)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  g_return_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->set_query != NULL);

  GTK_SEARCH_ENGINE_GET_CLASS (engine)->set_query (engine, query);
}

void
_gtk_search_engine_start (GtkSearchEngine *engine)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  g_return_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->start != NULL);

  GTK_SEARCH_ENGINE_GET_CLASS (engine)->start (engine);
}

void
_gtk_search_engine_stop (GtkSearchEngine *engine)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));
  g_return_if_fail (GTK_SEARCH_ENGINE_GET_CLASS (engine)->stop != NULL);

  GTK_SEARCH_ENGINE_GET_CLASS (engine)->stop (engine);
}

void
_gtk_search_engine_hits_added (GtkSearchEngine *engine,
                               GList           *hits)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));

  g_signal_emit (engine, signals[HITS_ADDED], 0, hits);
}

void
_gtk_search_engine_finished (GtkSearchEngine *engine)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));

  g_signal_emit (engine, signals[FINISHED], 0);
}

void
_gtk_search_engine_error (GtkSearchEngine *engine,
                          const gchar     *error_message)
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));

  g_signal_emit (engine, signals[ERROR], 0, error_message);
}

void
_gtk_search_engine_set_recursive (GtkSearchEngine *engine,
                                  gboolean         recursive)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  g_return_if_fail (GTK_IS_SEARCH_ENGINE (engine));

  g_assert (!priv->running);

  priv->recursive = recursive;

  if (priv->native)
    _gtk_search_engine_set_recursive (priv->native, recursive);

  if (priv->simple)
    _gtk_search_engine_set_recursive (priv->simple, recursive);
}

gboolean
_gtk_search_engine_get_recursive (GtkSearchEngine *engine)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  g_return_val_if_fail (GTK_IS_SEARCH_ENGINE (engine), TRUE);

  return priv->recursive;
}

void
_gtk_search_engine_set_model (GtkSearchEngine    *engine,
                              GtkFileSystemModel *model)
{
  GtkSearchEnginePrivate *priv = _gtk_search_engine_get_instance_private (engine);

  g_clear_object (&priv->model);
  if (model)
    {
      priv->model = _gtk_search_engine_model_new (model);
      connect_engine_signals (priv->model, engine);
      if (priv->query)
        _gtk_search_engine_set_query (priv->model, priv->query);
    }
}
