/*
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
 * Authors: Jürg Billeter <juerg.billeter@codethink.co.uk>
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

#include "gtksearchenginetracker.h"

#define DBUS_SERVICE_RESOURCES   "org.freedesktop.Tracker1"
#define DBUS_PATH_RESOURCES      "/org/freedesktop/Tracker1/Resources"
#define DBUS_INTERFACE_RESOURCES "org.freedesktop.Tracker1.Resources"

#define DBUS_SERVICE_STATUS      "org.freedesktop.Tracker1"
#define DBUS_PATH_STATUS         "/org/freedesktop/Tracker1/Status"
#define DBUS_INTERFACE_STATUS    "org.freedesktop.Tracker1.Status"

/* Time in second to wait for service before deciding it's not available */
#define WAIT_TIMEOUT_SECONDS 1

/* Time in second to wait for query results to come back */
#define QUERY_TIMEOUT_SECONDS 10

/* If defined, we use fts:match, this has to be enabled in Tracker to
 * work which it usually is. The alternative is to undefine it and
 * use filename matching instead. This doesn’t use the content of the
 * file however.
 */
#define FTS_MATCHING

struct _GtkSearchEngineTracker
{
  GtkSearchEngine parent;
  GDBusConnection *connection;
  GCancellable *cancellable;
  GtkQuery *query;
  gboolean query_pending;
  GPtrArray *indexed_locations;
};

struct _GtkSearchEngineTrackerClass
{
  GtkSearchEngineClass parent_class;
};

G_DEFINE_TYPE (GtkSearchEngineTracker, _gtk_search_engine_tracker, GTK_TYPE_SEARCH_ENGINE)

static void
finalize (GObject *object)
{
  GtkSearchEngineTracker *tracker;

  g_debug ("Finalizing GtkSearchEngineTracker");

  tracker = GTK_SEARCH_ENGINE_TRACKER (object);

  if (tracker->cancellable)
    {
      g_cancellable_cancel (tracker->cancellable);
      g_object_unref (tracker->cancellable);
    }

  g_clear_object (&tracker->query);
  g_clear_object (&tracker->connection);

  g_ptr_array_unref (tracker->indexed_locations);

  G_OBJECT_CLASS (_gtk_search_engine_tracker_parent_class)->finalize (object);
}

static GDBusConnection *
get_connection (void)
{
  GDBusConnection *connection;
  GError *error = NULL;
  GVariant *reply;

  /* Normally I hate sync calls with UIs, but we need to return NULL
   * or a GtkSearchEngine as a result of this function.
   */
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (error)
    {
      g_debug ("Couldn't connect to D-Bus session bus, %s", error->message);
      g_error_free (error);
      return NULL;
    }

  /* If connection is set, we know it worked. */
  g_debug ("Finding out if Tracker is available via D-Bus...");

  /* We only wait 1 second max, we expect it to be very fast. If we
   * don't get a response by then, clearly we're replaying a journal
   * or cleaning up the DB internally. Either way, services is not
   * available.
   *
   * We use the sync call here because we don't expect to be waiting
   * long enough to block UI painting.
   */
  reply = g_dbus_connection_call_sync (connection,
                                       DBUS_SERVICE_STATUS,
                                       DBUS_PATH_STATUS,
                                       DBUS_INTERFACE_STATUS,
                                       "Wait",
                                       NULL,
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NONE,
                                       WAIT_TIMEOUT_SECONDS * 1000,
                                       NULL,
                                       &error);

  if (error)
    {
      g_debug ("Tracker is not available, %s", error->message);
      g_error_free (error);
      g_object_unref (connection);
      return NULL;
    }

  g_variant_unref (reply);

  g_debug ("Tracker is ready");

  return connection;
}

static void
get_query_results (GtkSearchEngineTracker *engine,
                   const gchar            *sparql,
                   GAsyncReadyCallback     callback,
                   gpointer                user_data)
{
  g_dbus_connection_call (engine->connection,
                          DBUS_SERVICE_RESOURCES,
                          DBUS_PATH_RESOURCES,
                          DBUS_INTERFACE_RESOURCES,
                          "SparqlQuery",
                          g_variant_new ("(s)", sparql),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          QUERY_TIMEOUT_SECONDS * 1000,
                          engine->cancellable,
                          callback,
                          user_data);
}

/* Stolen from libtracker-sparql */
static gchar *
sparql_escape_string (const gchar *literal)
{
  GString *str;
  const gchar *p;

  g_return_val_if_fail (literal != NULL, NULL);

  str = g_string_new ("");
  p = literal;

  while (TRUE)
     {
      gsize len;

      if (!((*p) != '\0'))
        break;

      len = strcspn ((const gchar *) p, "\t\n\r\b\f\"\\");
      g_string_append_len (str, (const gchar *) p, (gssize) ((glong) len));
      p = p + len;

      switch (*p)
        {
        case '\t':
          g_string_append (str, "\\t");
          break;
        case '\n':
          g_string_append (str, "\\n");
          break;
        case '\r':
          g_string_append (str, "\\r");
          break;
        case '\b':
          g_string_append (str, "\\b");
          break;
        case '\f':
          g_string_append (str, "\\f");
          break;
        case '"':
          g_string_append (str, "\\\"");
          break;
        case '\\':
          g_string_append (str, "\\\\");
          break;
        default:
          continue;
        }

      p++;
     }
  return g_string_free (str, FALSE);
 }

static void
sparql_append_string_literal (GString     *sparql,
                              const gchar *str,
                              gboolean     glob,
                              gboolean     is_dir_uri,
                              gboolean     quoted)
{
  gchar *s;

  s = sparql_escape_string (str);

  g_string_append_c (sparql, '"');
  if (quoted)
    g_string_append (sparql, "\\\"");
  g_string_append (sparql, s);

  if (is_dir_uri)
    g_string_append_c (sparql, '/');
  if (quoted)
    g_string_append (sparql, "\\\"");
  if (glob)
    g_string_append_c (sparql, '*');
  g_string_append_c (sparql, '"');

  g_free (s);
}

static void
sparql_append_string_literal_lower_case (GString     *sparql,
                                         const gchar *str)
{
  gchar *s;

  s = g_utf8_strdown (str, -1);
  sparql_append_string_literal (sparql, s, FALSE, FALSE, FALSE);
  g_free (s);
}

static void
query_callback (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GtkSearchEngineTracker *tracker;
  GList *hits;
  GVariant *reply;
  GVariant *r;
  GVariantIter iter;
  GError *error = NULL;
  gint i, n;
  GtkSearchHit *hit;

  tracker = GTK_SEARCH_ENGINE_TRACKER (user_data);

  tracker->query_pending = FALSE;

  reply = g_dbus_connection_call_finish (tracker->connection, res, &error);
  if (error)
    {
      _gtk_search_engine_error (GTK_SEARCH_ENGINE (tracker), error->message);
      g_error_free (error);
      g_object_unref (tracker);
      return;
    }

  if (!reply)
    {
      _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker), FALSE);
      g_object_unref (tracker);
      return;
    }

  r = g_variant_get_child_value (reply, 0);
  g_variant_iter_init (&iter, r);
  n = g_variant_iter_n_children (&iter);
  hit = g_new (GtkSearchHit, n);
  hits = NULL;
  for (i = 0; i < n; i++)
    {
      GVariant *v;
      const gchar **strv;

      v = g_variant_iter_next_value (&iter);
      strv = g_variant_get_strv (v, NULL);
      hit[i].file = g_file_new_for_uri (strv[0]);
      hit[i].info = NULL;
      g_free (strv);
      hits = g_list_prepend (hits, &hit[i]);
    }

  _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (tracker), hits);
  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker), i > 0);

  g_list_free (hits);
  for (i = 0; i < n; i++)
    g_object_unref (hit[i].file);
  g_free (hit);

  g_variant_unref (reply);
  g_variant_unref (r);

  g_object_unref (tracker);
}

static void
gtk_search_engine_tracker_start (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;
  const gchar *search_text;
  GFile *location;
  GString *sparql;
  gboolean recursive;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

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

  search_text = gtk_query_get_text (tracker->query);
  location = gtk_query_get_location (tracker->query);
  recursive = _gtk_search_engine_get_recursive (engine);

  sparql = g_string_new ("SELECT nie:url(?urn) "
                         "WHERE {"
                         "  ?urn a nfo:FileDataObject ;"
                         "  tracker:available true ; "
                         "  nfo:belongsToContainer ?parent; ");

#ifdef FTS_MATCHING
  /* Using FTS: */
  g_string_append (sparql, "fts:match ");
  sparql_append_string_literal (sparql, search_text, TRUE, FALSE, TRUE);
#endif

  g_string_append (sparql, ". FILTER (BOUND(nie:url(?urn)) && ");

  g_string_append (sparql, "fn:contains(fn:lower-case(nfo:fileName(?urn)),");
  sparql_append_string_literal_lower_case (sparql, search_text);
  g_string_append (sparql, ")");

  if (location)
    {
      gchar *location_uri = g_file_get_uri (location);
      g_string_append (sparql, " && ");
      if (recursive)
        {
          g_string_append (sparql, "fn:starts-with(nie:url(?urn),");
          sparql_append_string_literal (sparql, location_uri, FALSE, TRUE, FALSE);
          g_string_append (sparql, ")");
        }
      else
        {
          g_string_append (sparql, "nie:url(?parent) = ");
          sparql_append_string_literal (sparql, location_uri, FALSE, FALSE, FALSE);
        }
      g_free (location_uri);
    }

  g_string_append (sparql, ")");

#ifdef FTS_MATCHING
  g_string_append (sparql, " } ORDER BY DESC(fts:rank(?urn)) DESC(nie:url(?urn))");
#else  /* FTS_MATCHING */
  g_string_append (sparql, "} ORDER BY DESC(nie:url(?urn)) DESC(nfo:fileName(?urn))");
#endif /* FTS_MATCHING */

  tracker->query_pending = TRUE;

  g_debug ("SearchEngineTracker: query: %s", sparql->str);

  get_query_results (tracker, sparql->str, query_callback, g_object_ref (tracker));

  g_string_free (sparql, TRUE);
}

static void
gtk_search_engine_tracker_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  if (tracker->query && tracker->query_pending)
    {
      g_cancellable_cancel (tracker->cancellable);
      tracker->query_pending = FALSE;
    }
}

static void
gtk_search_engine_tracker_set_query (GtkSearchEngine *engine,
                                     GtkQuery        *query)
{
  GtkSearchEngineTracker *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  if (query)
    g_object_ref (query);

  if (tracker->query)
    g_object_unref (tracker->query);

  tracker->query = query;
}

static void
_gtk_search_engine_tracker_class_init (GtkSearchEngineTrackerClass *class)
{
  GObjectClass *gobject_class;
  GtkSearchEngineClass *engine_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = finalize;

  engine_class = GTK_SEARCH_ENGINE_CLASS (class);
  engine_class->set_query = gtk_search_engine_tracker_set_query;
  engine_class->start = gtk_search_engine_tracker_start;
  engine_class->stop = gtk_search_engine_tracker_stop;
}

static void get_indexed_locations (GtkSearchEngineTracker *engine);

static void
_gtk_search_engine_tracker_init (GtkSearchEngineTracker *engine)
{
  engine->cancellable = g_cancellable_new ();
  engine->query_pending = FALSE;
  engine->indexed_locations = g_ptr_array_new_with_free_func (g_object_unref);

  get_indexed_locations (engine);
}

GtkSearchEngine *
_gtk_search_engine_tracker_new (void)
{
  GtkSearchEngineTracker *engine;
  GDBusConnection *connection;

  g_debug ("--");

  connection = get_connection ();
  if (!connection)
    return NULL;

  g_debug ("Creating GtkSearchEngineTracker...");

  engine = g_object_new (GTK_TYPE_SEARCH_ENGINE_TRACKER, NULL);

  engine->connection = connection;

  return GTK_SEARCH_ENGINE (engine);
}

#define TRACKER_SCHEMA "org.freedesktop.Tracker.Miner.Files"
#define TRACKER_KEY_RECURSIVE_DIRECTORIES "index-recursive-directories"

static const gchar *
get_user_special_dir_if_not_home (GUserDirectory idx)
{
  const gchar *path;
  path = g_get_user_special_dir (idx);
  if (g_strcmp0 (path, g_get_home_dir ()) == 0)
    return NULL;

  return path;
}

static const gchar *
path_from_tracker_dir (const gchar *value)
{
  const gchar *path;

  if (g_strcmp0 (value, "&DESKTOP") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DESKTOP);
  else if (g_strcmp0 (value, "&DOCUMENTS") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOCUMENTS);
  else if (g_strcmp0 (value, "&DOWNLOAD") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_DOWNLOAD);
  else if (g_strcmp0 (value, "&MUSIC") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_MUSIC);
  else if (g_strcmp0 (value, "&PICTURES") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_PICTURES);
  else if (g_strcmp0 (value, "&PUBLIC_SHARE") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_PUBLIC_SHARE);
  else if (g_strcmp0 (value, "&TEMPLATES") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_TEMPLATES);
  else if (g_strcmp0 (value, "&VIDEOS") == 0)
    path = get_user_special_dir_if_not_home (G_USER_DIRECTORY_VIDEOS);
  else if (g_strcmp0 (value, "$HOME") == 0)
    path = g_get_home_dir ();
  else
    path = value;

  return path;
}

static void
get_indexed_locations (GtkSearchEngineTracker *engine)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  GSettings *settings;
  gchar **locations;
  gint i;
  GFile *location;
  const gchar *path;

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, TRACKER_SCHEMA, FALSE);
  if (!schema)
    return;

  settings = g_settings_new_full (schema, NULL, NULL);
  g_settings_schema_unref (schema);

  locations = g_settings_get_strv (settings, TRACKER_KEY_RECURSIVE_DIRECTORIES);

  for (i = 0; locations[i] != NULL; i++)
    {
      path = path_from_tracker_dir (locations[i]);
      if (path == NULL)
        continue;

      location = g_file_new_for_path (path);
      g_ptr_array_add (engine->indexed_locations, location);
    }

  g_strfreev (locations);
  g_object_unref (settings);
}

gboolean
_gtk_search_engine_tracker_is_indexed (GFile    *location,
                                       gpointer  data)
{
  GtkSearchEngineTracker *engine = data;
  gint i;
  GFile *place;

  for (i = 0; i < engine->indexed_locations->len; i++)
    {
      place = g_ptr_array_index (engine->indexed_locations, i);
      if (g_file_equal (location, place) || g_file_has_prefix (location, place))
        return TRUE;
    }

  return FALSE;
}
