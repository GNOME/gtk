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
#undef FTS_MATCHING

/*
 * GtkSearchEngineTracker object
 */
struct _GtkSearchEngineTrackerPrivate
{
  GDBusConnection *connection;
  GCancellable *cancellable;
  GtkQuery *query;
  gboolean query_pending;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSearchEngineTracker, _gtk_search_engine_tracker, GTK_TYPE_SEARCH_ENGINE)

static void
finalize (GObject *object)
{
  GtkSearchEngineTracker *tracker;

  g_debug ("Finalizing GtkSearchEngineTracker");

  tracker = GTK_SEARCH_ENGINE_TRACKER (object);

  if (tracker->priv->cancellable)
    {
      g_cancellable_cancel (tracker->priv->cancellable);
      g_object_unref (tracker->priv->cancellable);
      tracker->priv->cancellable = NULL;
    }

  if (tracker->priv->query)
    {
      g_object_unref (tracker->priv->query);
      tracker->priv->query = NULL;
    }

  if (tracker->priv->connection)
    {
      g_object_unref (tracker->priv->connection);
      tracker->priv->connection = NULL;
    }

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
  g_dbus_connection_call (engine->priv->connection,
                          DBUS_SERVICE_RESOURCES,
                          DBUS_PATH_RESOURCES,
                          DBUS_INTERFACE_RESOURCES,
                          "SparqlQuery",
                          g_variant_new ("(s)", sparql),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          QUERY_TIMEOUT_SECONDS * 1000,
                          engine->priv->cancellable,
                          callback,
                          user_data);
}

/* Stolen from libtracker-common */
static GList *
string_list_to_gslist (gchar **strv)
{
  GList *list;
  gsize i;

  list = NULL;

  for (i = 0; strv[i]; i++)
    list = g_list_prepend (list, g_strdup (strv[i]));

  return g_list_reverse (list);
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
                              const gchar *str)
{
  gchar *s;

  s = sparql_escape_string (str);

  g_string_append_c (sparql, '"');
  g_string_append (sparql, s);
  g_string_append_c (sparql, '"');

  g_free (s);
}

static void
sparql_append_string_literal_lower_case (GString     *sparql,
                                         const gchar *str)
{
  gchar *s;

  s = g_utf8_strdown (str, -1);
  sparql_append_string_literal (sparql, s);
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
  gchar **result;
  GError *error = NULL;
  gint i, n;

  gdk_threads_enter ();

  tracker = GTK_SEARCH_ENGINE_TRACKER (user_data);

  tracker->priv->query_pending = FALSE;

  reply = g_dbus_connection_call_finish (tracker->priv->connection, res, &error);
  if (error)
    {
      _gtk_search_engine_error (GTK_SEARCH_ENGINE (tracker), error->message);
      g_error_free (error);
      gdk_threads_leave ();
      return;
    }

  if (!reply)
    {
      _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker));
      gdk_threads_leave ();
      return;
    }

  r = g_variant_get_child_value (reply, 0);
  g_variant_iter_init (&iter, r);
  n = g_variant_iter_n_children (&iter);
  result = g_new0 (gchar *, n + 1);
  for (i = 0; i < n; i++)
    {
      GVariant *v;
      const gchar **strv;

      v = g_variant_iter_next_value (&iter);
      strv = g_variant_get_strv (v, NULL);
      result[i] = (gchar*)strv[0];
      g_free (strv);
    }

  /* We iterate result by result, not n at a time. */
  hits = string_list_to_gslist (result);
  _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (tracker), hits);
  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker));
  g_list_free (hits);
  g_free (result);
  g_variant_unref (reply);
  g_variant_unref (r);

  gdk_threads_leave ();
}

static void
gtk_search_engine_tracker_start (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;
  gchar *search_text;
#ifdef FTS_MATCHING
  gchar *location_uri;
#endif
  GString *sparql;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  if (tracker->priv->query_pending)
    {
      g_debug ("Attempt to start a new search while one is pending, doing nothing");
      return;
    }

  if (tracker->priv->query == NULL)
    {
      g_debug ("Attempt to start a new search with no GtkQuery, doing nothing");
      return;
    }

  search_text = _gtk_query_get_text (tracker->priv->query);

#ifdef FTS_MATCHING
  location_uri = _gtk_query_get_location (tracker->priv->query);
  /* Using FTS: */
  sparql = g_string_new ("SELECT nie:url(?urn) "
                         "WHERE {"
                         "  ?urn a nfo:FileDataObject ;"
                         "  tracker:available true ; "
                         "  fts:match ");
  sparql_append_string_literal (sparql, search_text);

  if (location_uri)
    {
      g_string_append (sparql, " . FILTER (fn:starts-with(nie:url(?urn),");
      sparql_append_string_literal (sparql, location_uri);
      g_string_append (sparql, "))");
    }

  g_string_append (sparql, " } ORDER BY DESC(fts:rank(?urn)) ASC(nie:url(?urn))");
#else  /* FTS_MATCHING */
  /* Using filename matching: */
  sparql = g_string_new ("SELECT nie:url(?urn) "
                         "WHERE {"
                         "  ?urn a nfo:FileDataObject ;"
                         "    tracker:available true ."
                         "  FILTER (fn:contains(fn:lower-case(nfo:fileName(?urn)),");
  sparql_append_string_literal_lower_case (sparql, search_text);

  g_string_append (sparql,
                   "))"
                   "} ORDER BY DESC(nie:url(?urn)) DESC(nfo:fileName(?urn))");
#endif /* FTS_MATCHING */

  tracker->priv->query_pending = TRUE;

  get_query_results (tracker, sparql->str, query_callback, tracker);

  g_string_free (sparql, TRUE);
  g_free (search_text);
}

static void
gtk_search_engine_tracker_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  if (tracker->priv->query && tracker->priv->query_pending)
    {
      g_cancellable_cancel (tracker->priv->cancellable);
      tracker->priv->query_pending = FALSE;
    }
}

static gboolean
gtk_search_engine_tracker_is_indexed (GtkSearchEngine *engine)
{
  return TRUE;
}

static void
gtk_search_engine_tracker_set_query (GtkSearchEngine *engine,
                                     GtkQuery        *query)
{
  GtkSearchEngineTracker *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  if (query)
    g_object_ref (query);

  if (tracker->priv->query)
    g_object_unref (tracker->priv->query);

  tracker->priv->query = query;
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
  engine_class->is_indexed = gtk_search_engine_tracker_is_indexed;
}

static void
_gtk_search_engine_tracker_init (GtkSearchEngineTracker *engine)
{
  engine->priv = _gtk_search_engine_tracker_get_instance_private (engine);
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

  engine->priv->connection = connection;
  engine->priv->cancellable = g_cancellable_new ();
  engine->priv->query_pending = FALSE;

  return GTK_SEARCH_ENGINE (engine);
}
