/*
 * Copyright (C) 2020 Red Hat Inc
 * Copyright (C) 2009-2011 Nokia <ivan.frade@nokia.com>
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
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Jürg Billeter <juerg.billeter@codethink.co.uk>
 *          Martyn Russell <martyn@lanedo.com>
 *
 * Based on nautilus-search-engine-tracker.c
 */

#include "config.h"

#include <string.h>

#include <gio/gio.h>
#include <gmodule.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "gtksearchenginetracker3private.h"

#define MINER_FS_BUS_NAME "org.freedesktop.Tracker3.Miner.Files"

#define SEARCH_QUERY_BASE(__PATTERN__)                                 \
  "SELECT ?url "                                                       \
  "       nfo:fileName(?urn) "					       \
  "       nie:mimeType(?urn)"					       \
  "       nfo:fileSize(?urn)"					       \
  "       nfo:fileLastModified(?urn)"				       \
  "FROM tracker:FileSystem "                                           \
  "WHERE {"                                                            \
  "  ?urn a nfo:FileDataObject ;"                                      \
  "       nie:url ?url ; "                                             \
  "       fts:match ~match . "                                         \
  __PATTERN__                                                          \
  "} "                                                                 \
  "ORDER BY DESC(fts:rank(?urn)) DESC(?url)"

#define SEARCH_QUERY SEARCH_QUERY_BASE("")
#define SEARCH_RECURSIVE_QUERY SEARCH_QUERY_BASE("?urn (nfo:belongsToContainer/nie:isStoredAs)+/nie:url ~location")
#define SEARCH_LOCATION_QUERY SEARCH_QUERY_BASE("?urn nfo:belongsToContainer/nie:isStoredAs/nie:url ~location")

struct _GtkSearchEngineTracker3
{
  GtkSearchEngine parent;
  TrackerSparqlConnection *sparql_conn;
  TrackerSparqlStatement *search_query;
  TrackerSparqlStatement *search_recursive_query;
  TrackerSparqlStatement *search_location_query;
  GCancellable *cancellable;
  GtkQuery *query;
  gboolean query_pending;
};

struct _GtkSearchEngineTracker3Class
{
  GtkSearchEngineClass parent_class;
};

static void gtk_search_engine_tracker3_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSearchEngineTracker3,
                         gtk_search_engine_tracker3,
                         GTK_TYPE_SEARCH_ENGINE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gtk_search_engine_tracker3_initable_iface_init))

static void
finalize (GObject *object)
{
  GtkSearchEngineTracker3 *engine;

  g_debug ("Finalizing GtkSearchEngineTracker3");

  engine = GTK_SEARCH_ENGINE_TRACKER3 (object);

  if (engine->cancellable)
    {
      g_cancellable_cancel (engine->cancellable);
      g_object_unref (engine->cancellable);
    }

  g_clear_object (&engine->search_query);
  g_clear_object (&engine->search_location_query);
  tracker_sparql_connection_close (engine->sparql_conn);
  g_clear_object (&engine->sparql_conn);

  G_OBJECT_CLASS (gtk_search_engine_tracker3_parent_class)->finalize (object);
}

static void
free_hit (gpointer data)
{
  GtkSearchHit *hit = data;

  g_clear_object (&hit->file);
  g_clear_object (&hit->info);
  g_slice_free (GtkSearchHit, hit);
}

static GFileInfo *
create_file_info (GFile               *file,
                  TrackerSparqlCursor *cursor)
{
  GFileInfo *info;
  const char *str;
  GDateTime *creation;

  info = g_file_info_new ();
  str = tracker_sparql_cursor_get_string (cursor, 1, NULL);
  if (str)
    g_file_info_set_display_name (info, str);

  str = tracker_sparql_cursor_get_string (cursor, 2, NULL);
  if (str)
    g_file_info_set_content_type (info, str);

  g_file_info_set_size (info,
                        tracker_sparql_cursor_get_integer (cursor, 3));

  str = tracker_sparql_cursor_get_string (cursor, 4, NULL);
  if (str)
    {
      creation = g_date_time_new_from_iso8601 (str, NULL);
      g_file_info_set_modification_date_time (info, creation);
      g_date_time_unref (creation);
    }

  g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
  g_file_info_set_attribute_boolean (info, "filechooser::filtered-out", FALSE);
  g_file_info_set_attribute_boolean (info, "filechooser::visible", TRUE);

  return info;
}

static void
query_callback (TrackerSparqlStatement *statement,
                GAsyncResult           *res,
                gpointer                user_data)
{
  GtkSearchEngineTracker3 *engine;
  TrackerSparqlCursor *cursor;
  GList *hits = NULL;
  GError *error = NULL;
  GtkSearchHit *hit;

  engine = GTK_SEARCH_ENGINE_TRACKER3 (user_data);

  engine->query_pending = FALSE;

  cursor = tracker_sparql_statement_execute_finish (statement, res, &error);

  if (!cursor)
    {
      _gtk_search_engine_error (GTK_SEARCH_ENGINE (engine), error->message);
      g_error_free (error);
      g_object_unref (engine);
      return;
    }

  while (tracker_sparql_cursor_next (cursor, NULL, NULL))
    {
      const char *url;

      url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
      hit = g_slice_new0 (GtkSearchHit);
      hit->file = g_file_new_for_uri (url);
      hit->info = create_file_info (hit->file, cursor);
      hits = g_list_prepend (hits, hit);
    }

  tracker_sparql_cursor_close (cursor);

  _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (engine), hits);
  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (engine), hits != NULL);

  g_list_free_full (hits, free_hit);
  g_object_unref (engine);
  g_object_unref (cursor);
}

static void
gtk_search_engine_tracker3_start (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker3 *tracker;
  TrackerSparqlStatement *statement;
  const char *search_text;
  char *match;
  GFile *location;
  gboolean recursive;

  tracker = GTK_SEARCH_ENGINE_TRACKER3 (engine);

  if (tracker->query_pending)
    {
      g_debug ("Attempt to start a new search while one is pending, doing nothing");
      return;
    }

  if (tracker->query == NULL)
    {
      g_debug ("Attempt to start a new search with no GtkQuery, doing nothing");
      return;
    }

  tracker->query_pending = TRUE;
  search_text = gtk_query_get_text (tracker->query);
  location = gtk_query_get_location (tracker->query);
  recursive = TRUE;

  if (location)
    {
      char *location_uri = g_file_get_uri (location);

      if (recursive)
        {
          g_debug ("Recursive search query in location: %s", location_uri);
          statement = tracker->search_recursive_query;
        }
      else
        {
          g_debug ("Search query in location: %s", location_uri);
          statement = tracker->search_location_query;
        }

      tracker_sparql_statement_bind_string (statement,
                                            "location",
                                            location_uri);
      g_free (location_uri);
    }
  else
    {
      g_debug ("Search query");
      statement = tracker->search_query;
    }

  match = g_strdup_printf ("%s*", search_text);
  tracker_sparql_statement_bind_string (statement, "match", match);
  g_debug ("search text: %s\n", match);
  tracker_sparql_statement_execute_async (statement, tracker->cancellable,
                                          (GAsyncReadyCallback) query_callback,
                                          g_object_ref (tracker));
  g_free (match);
}

static void
gtk_search_engine_tracker3_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker3 *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER3 (engine);

  if (tracker->query && tracker->query_pending)
    {
      g_cancellable_cancel (tracker->cancellable);
      tracker->query_pending = FALSE;
    }
}

static void
gtk_search_engine_tracker3_set_query (GtkSearchEngine *engine,
                                      GtkQuery        *query)
{
  GtkSearchEngineTracker3 *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER3 (engine);

  if (query)
    g_object_ref (query);

  if (tracker->query)
    g_object_unref (tracker->query);

  tracker->query = query;
}

static void
gtk_search_engine_tracker3_class_init (GtkSearchEngineTracker3Class *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkSearchEngineClass *engine_class = GTK_SEARCH_ENGINE_CLASS (class);

  gobject_class->finalize = finalize;

  engine_class->set_query = gtk_search_engine_tracker3_set_query;
  engine_class->start = gtk_search_engine_tracker3_start;
  engine_class->stop = gtk_search_engine_tracker3_stop;
}

static void
gtk_search_engine_tracker3_init (GtkSearchEngineTracker3 *engine)
{
  engine->cancellable = g_cancellable_new ();
  engine->query_pending = FALSE;
}

static gboolean
gtk_search_engine_tracker3_initable_init (GInitable     *initable,
                                          GCancellable  *cancellable,
                                          GError       **error)
{
  GtkSearchEngineTracker3 *engine;

  engine = GTK_SEARCH_ENGINE_TRACKER3 (initable);

  engine->sparql_conn = tracker_sparql_connection_bus_new (MINER_FS_BUS_NAME,
                                                           NULL, NULL,
                                                           error);
  if (!engine->sparql_conn)
    return FALSE;

  engine->search_query =
    tracker_sparql_connection_query_statement (engine->sparql_conn,
                                               SEARCH_QUERY,
                                               cancellable,
                                               error);
  if (!engine->search_query)
    return FALSE;

  engine->search_recursive_query =
    tracker_sparql_connection_query_statement (engine->sparql_conn,
                                               SEARCH_RECURSIVE_QUERY,
                                               cancellable,
                                               error);
  if (!engine->search_recursive_query)
    return FALSE;

  engine->search_location_query =
    tracker_sparql_connection_query_statement (engine->sparql_conn,
                                               SEARCH_LOCATION_QUERY,
                                               cancellable,
                                               error);
  if (!engine->search_location_query)
    return FALSE;

  return TRUE;
}

static void
gtk_search_engine_tracker3_initable_iface_init (GInitableIface *iface)
{
  iface->init = gtk_search_engine_tracker3_initable_init;
}

GtkSearchEngine *
gtk_search_engine_tracker3_new (void)
{
  GtkSearchEngineTracker3 *engine;
  GError *error = NULL;

  g_debug ("Creating GtkSearchEngineTracker3...");

  engine = g_initable_new (GTK_TYPE_SEARCH_ENGINE_TRACKER3,
                           NULL, &error, NULL);
  if (!engine)
    {
      g_critical ("Could not init tracker3 search engine: %s",
                  error->message);
      g_error_free (error);
    }

  return GTK_SEARCH_ENGINE (engine);
}
