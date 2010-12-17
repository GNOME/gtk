/*
 * Copyright (C) 2009-2010 Nokia <ivan.frade@nokia.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *
 * Authors: Jürg Billeter <juerg.billeter@codethink.co.uk>
 *          Martyn Russell <martyn@lanedo.com>
 *
 * Based on nautilus-search-engine-tracker.c
 */

#include "config.h"

#include <gio/gio.h>
#include <gmodule.h>

#include "gtksearchenginetracker.h"

/* If defined, we use fts:match, this has to be enabled in Tracker to
 * work which it usually is. The alternative is to undefine it and
 * use filename matching instead. This doesn't use the content of the
 * file however.
 */
#undef FTS_MATCHING

#define MODULE_FILENAME "libtracker-sparql-0.10.so.0"

#define MODULE_MAP(a)   { #a, (gpointer *)&a }

/* Connection object */
typedef struct _TrackerSparqlConnection TrackerSparqlConnection;

#define TRACKER_SPARQL_TYPE_CONNECTION (tracker_sparql_connection_get_type ())
#define TRACKER_SPARQL_CONNECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_SPARQL_TYPE_CONNECTION, TrackerSparqlConnection))

/* Cursor object */
typedef struct _TrackerSparqlCursor TrackerSparqlCursor;

#define TRACKER_SPARQL_TYPE_CURSOR (tracker_sparql_cursor_get_type ())
#define TRACKER_SPARQL_CURSOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_SPARQL_TYPE_CURSOR, TrackerSparqlCursor))

/* API */
static GType                     (*tracker_sparql_connection_get_type)     (void) = NULL;
static TrackerSparqlConnection * (*tracker_sparql_connection_get)          (GCancellable             *cancellable,
                                                                            GError                  **error) = NULL;
static void                      (*tracker_sparql_connection_query_async)  (TrackerSparqlConnection  *self,
                                                                            const gchar              *sparql,
                                                                            GCancellable             *cancellable,
                                                                            GAsyncReadyCallback       callback,
                                                                            gpointer                  user_data) = NULL;
static TrackerSparqlCursor *     (*tracker_sparql_connection_query_finish) (TrackerSparqlConnection  *self,
                                                                            GAsyncResult             *_res_,
                                                                            GError                  **error) = NULL;
static GType                     (*tracker_sparql_cursor_get_type)         (void) = NULL;
static void                      (*tracker_sparql_cursor_next_async)       (TrackerSparqlCursor      *self,
                                                                            GCancellable             *cancellable,
                                                                            GAsyncReadyCallback       callback,
                                                                            gpointer                  user_data) = NULL;
static gboolean                  (*tracker_sparql_cursor_next_finish)      (TrackerSparqlCursor      *self,
                                                                            GAsyncResult             *_res_,
                                                                            GError                  **error) = NULL;
static const gchar *             (*tracker_sparql_cursor_get_string)       (TrackerSparqlCursor      *self,
                                                                            gint                     *column,
                                                                            glong                    *length) = NULL;
static gchar *                   (*tracker_sparql_escape_string)           (const gchar              *literal) = NULL;

static struct TrackerFunctions
{
	const char *name;
	gpointer *pointer;
} funcs[] = {
	MODULE_MAP (tracker_sparql_connection_get_type),
	MODULE_MAP (tracker_sparql_connection_get),
	MODULE_MAP (tracker_sparql_connection_query_async),
	MODULE_MAP (tracker_sparql_connection_query_finish),
	MODULE_MAP (tracker_sparql_cursor_get_type),
	MODULE_MAP (tracker_sparql_cursor_next_async),
	MODULE_MAP (tracker_sparql_cursor_next_finish),
	MODULE_MAP (tracker_sparql_cursor_get_string),
	MODULE_MAP (tracker_sparql_escape_string)
};

static gboolean
init (void)
{
  static gboolean inited = FALSE;
  gint i;
  GModule *m;
  GModuleFlags flags;

  if (inited)
	  return TRUE;

  flags = G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL;

  /* Only support 0.10 onwards */
  if ((m = g_module_open (MODULE_FILENAME, flags)) == NULL)
	  {
		  g_debug ("No tracker backend available or it is not new enough");
		  g_debug ("Only available using '%s'", MODULE_FILENAME);
		  return FALSE;
	  }

  inited = TRUE;

  /* Check for the symbols we need */
  for (i = 0; i < G_N_ELEMENTS (funcs); i++)
	  {
		  if (!g_module_symbol (m, funcs[i].name, funcs[i].pointer))
			  {
				  g_warning ("Missing symbol '%s' in libtracker-sparql\n",
				             funcs[i].name);
				  g_module_close (m);

				  for (i = 0; i < G_N_ELEMENTS (funcs); i++)
					  funcs[i].pointer = NULL;

				  return FALSE;
			  }
	    }

  g_debug ("Loaded Tracker library and all required symbols");

  return TRUE;
}

/*
 * GtkSearchEngineTracker object
 */
struct _GtkSearchEngineTrackerPrivate
{
  TrackerSparqlConnection *connection;
	GCancellable *cancellable;
  GtkQuery *query;
  gboolean query_pending;
};

G_DEFINE_TYPE (GtkSearchEngineTracker, _gtk_search_engine_tracker, GTK_TYPE_SEARCH_ENGINE);

static void cursor_callback (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data);

static void
finalize (GObject *object)
{
  GtkSearchEngineTracker *tracker;

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

static void
cursor_next (GtkSearchEngineTracker *tracker,
             TrackerSparqlCursor    *cursor)
{
	tracker_sparql_cursor_next_async (cursor,
	                                  tracker->priv->cancellable,
	                                  cursor_callback,
	                                  tracker);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GtkSearchEngineTracker *tracker;
  GError *error = NULL;
  TrackerSparqlCursor *cursor;
	GList *hits;
  gboolean success;

  tracker = GTK_SEARCH_ENGINE_TRACKER (user_data);

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

  if (error)
    {
      _gtk_search_engine_error (GTK_SEARCH_ENGINE (tracker), error->message);

      g_error_free (error);

      if (cursor)
	      g_object_unref (cursor);

      return;
    }

  if (!success)
	  {
		  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker));

		  if (cursor)
			  g_object_unref (cursor);

		  return;
	  }

  /* We iterate result by result, not n at a time. */
  hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
  _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (tracker), hits);
  g_list_free (hits);

  /* Get next */
  cursor_next (tracker, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GtkSearchEngineTracker *tracker;
  TrackerSparqlConnection *connection;
  TrackerSparqlCursor *cursor;
  GError *error = NULL;

  tracker = GTK_SEARCH_ENGINE_TRACKER (user_data);

  tracker->priv->query_pending = FALSE;

  connection = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (connection,
	                                                 result,
	                                                 &error);

	g_debug ("Query returned cursor:%p", cursor);

  if (error)
    {
      _gtk_search_engine_error (GTK_SEARCH_ENGINE (tracker), error->message);
      g_error_free (error);
      return;
    }

  if (!cursor)
	  {
		  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker));
		  return;
	  }

  cursor_next (tracker, cursor);
}

static void
sparql_append_string_literal (GString     *sparql,
                              const gchar *str)
{
  gchar *s;

  s = tracker_sparql_escape_string (str);

  g_string_append_c (sparql, '"');
  g_string_append (sparql, s);
  g_string_append_c (sparql, '"');

  g_free (s);
}

static void
gtk_search_engine_tracker_start (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;
  gchar	*search_text, *location_uri;
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
  location_uri = _gtk_query_get_location (tracker->priv->query);

  g_debug ("Query starting, search criteria:'%s', location:'%s'", search_text, location_uri);

  /* Using FTS: */
  sparql = g_string_new ("SELECT nie:url(?urn) "
                         "WHERE {"
                         "  ?urn a nfo:FileDataObject ;"
                         "  tracker:available true ; "
                         "  fts:match ");
  sparql_append_string_literal (sparql, search_text);

#ifdef FTS_MATCHING
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
                         "  FILTER (fn:contains(nfo:fileName(?urn),");
  sparql_append_string_literal (sparql, search_text);

  g_string_append (sparql, 
                   "))"
                   "} ORDER BY DESC(nie:url(?urn)) DESC(nfo:fileName(?urn))");
#endif /* FTS_MATCHING */

  tracker_sparql_connection_query_async (tracker->priv->connection,
                                         sparql->str,
                                         tracker->priv->cancellable,
                                         query_callback,
                                         tracker);
  g_string_free (sparql, TRUE);

  tracker->priv->query_pending = TRUE;
  g_free (search_text);
}

static void
gtk_search_engine_tracker_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  g_debug ("Query stopping");

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

  g_type_class_add_private (gobject_class, sizeof (GtkSearchEngineTrackerPrivate));
}

static void
_gtk_search_engine_tracker_init (GtkSearchEngineTracker *engine)
{
  engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
                                              GTK_TYPE_SEARCH_ENGINE_TRACKER,
                                              GtkSearchEngineTrackerPrivate);
}


GtkSearchEngine *
_gtk_search_engine_tracker_new (void)
{
  GtkSearchEngineTracker *engine;
  TrackerSparqlConnection *connection;
  GCancellable *cancellable;
  GError *error = NULL;

  if (!init ())
	  return NULL;

  g_debug ("Creating GtkSearchEngineTracker...");

  cancellable = g_cancellable_new ();
	connection = tracker_sparql_connection_get (cancellable, &error);

	if (error)
		{
			g_warning ("Could not establish a connection to Tracker: %s", error->message);
			g_error_free (error);
			return NULL;
		}
	else if (!connection)
	  {
		  g_warning ("Could not establish a connection to Tracker, no TrackerSparqlConnection was returned");
		  return NULL;
	  }

  engine = g_object_new (GTK_TYPE_SEARCH_ENGINE_TRACKER, NULL);

  engine->priv->connection = connection;
  engine->priv->cancellable = cancellable;
  engine->priv->query_pending = FALSE;

  return GTK_SEARCH_ENGINE (engine);
}
