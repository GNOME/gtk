/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 * Based on nautilus-search-engine-tracker.c
 */

#include "config.h"
#include <gmodule.h>
#include "gtksearchenginetracker.h"
#if 0
#include <tracker.h>
#endif

/* we dlopen() libtracker at runtime */

typedef struct _TrackerClient TrackerClient;

typedef enum
{
  TRACKER_0_6 = 1 << 0,
  TRACKER_0_7 = 1 << 1
} TrackerVersion;

typedef void (*TrackerArrayReply) (char **result, GError *error, gpointer user_data);

static TrackerClient * (*tracker_connect) (gboolean enable_warnings, gint timeout) = NULL;
static void	       (*tracker_disconnect) (TrackerClient *client) = NULL;
static int             (*tracker_get_version) (TrackerClient *client, GError **error) = NULL;
static void            (*tracker_cancel_last_call) (TrackerClient *client) = NULL;

static void (*tracker_search_metadata_by_text_async) (TrackerClient *client, 
						      const char *query, 
						      TrackerArrayReply callback, 
						      gpointer user_data) = NULL;
static void (*tracker_search_metadata_by_text_and_location_async) (TrackerClient *client, 
								   const char *query, 
								   const char *location, 
								   TrackerArrayReply callback, 
								   gpointer user_data) = NULL;

static struct TrackerDlMapping
{
  const char *fn_name;
  gpointer *fn_ptr_ref;
  TrackerVersion versions;
} tracker_dl_mapping[] =
{
#define MAP(a,v) { #a, (gpointer *)&a, v }
  MAP (tracker_connect, TRACKER_0_6 | TRACKER_0_7),
  MAP (tracker_disconnect, TRACKER_0_6 | TRACKER_0_7),
  MAP (tracker_get_version, TRACKER_0_6),
  MAP (tracker_cancel_last_call, TRACKER_0_6 | TRACKER_0_7),
  MAP (tracker_search_metadata_by_text_async, TRACKER_0_6 | TRACKER_0_7),
  MAP (tracker_search_metadata_by_text_and_location_async, TRACKER_0_6 | TRACKER_0_7),
#undef MAP
};

static TrackerVersion
open_libtracker (void)
{
  static gboolean done = FALSE;
  static TrackerVersion version = 0;

  if (!done)
    {
      int i;
      GModule *tracker;
      GModuleFlags flags;
      
      done = TRUE;
      flags = G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL;

      tracker = g_module_open ("libtracker-client-0.7.so.0", flags);
      version = TRACKER_0_7;

      if (!tracker)
        {
          tracker = g_module_open ("libtrackerclient.so.0", flags);
          version = TRACKER_0_6;
        }

      if (!tracker)
        {
          tracker = g_module_open ("libtracker.so.0", flags);
          version = TRACKER_0_6;
        }

      if (!tracker)
	return 0;
      
      for (i = 0; i < G_N_ELEMENTS (tracker_dl_mapping); i++)
	{
	  if ((tracker_dl_mapping[i].versions & version) == 0)
	    continue;

	  if (!g_module_symbol (tracker, tracker_dl_mapping[i].fn_name,
				tracker_dl_mapping[i].fn_ptr_ref))
	    {
	      g_warning ("Missing symbol '%s' in libtracker\n",
			 tracker_dl_mapping[i].fn_name);
	      g_module_close (tracker);

	      for (i = 0; i < G_N_ELEMENTS (tracker_dl_mapping); i++)
		tracker_dl_mapping[i].fn_ptr_ref = NULL;

	      return 0;
	    }
	}
    }

  return version;
}

struct _GtkSearchEngineTrackerPrivate 
{
  GtkQuery 	*query;
  TrackerClient *client;
  gboolean 	 query_pending;
  TrackerVersion version;
};

G_DEFINE_TYPE (GtkSearchEngineTracker, _gtk_search_engine_tracker, GTK_TYPE_SEARCH_ENGINE);


static void
finalize (GObject *object)
{
  GtkSearchEngineTracker *tracker;
  
  tracker = GTK_SEARCH_ENGINE_TRACKER (object);
  
  if (tracker->priv->query) 
    {
      g_object_unref (tracker->priv->query);
      tracker->priv->query = NULL;
    }

  tracker_disconnect (tracker->priv->client);

  G_OBJECT_CLASS (_gtk_search_engine_tracker_parent_class)->finalize (object);
}


static void
search_callback (gchar  **results, 
		 GError  *error, 
		 gpointer user_data)
{
  GtkSearchEngineTracker *tracker;
  gchar **results_p;
  GList *hit_uris;
  
  tracker = GTK_SEARCH_ENGINE_TRACKER (user_data);
  hit_uris = NULL;
  
  tracker->priv->query_pending = FALSE;

  if (error) 
    {
      _gtk_search_engine_error ( GTK_SEARCH_ENGINE (tracker), error->message);
      g_error_free (error);
      return;
    }

  if (!results)
    return;
	
  for (results_p = results; *results_p; results_p++) 
    {
      gchar *uri;

      if (tracker->priv->version == TRACKER_0_6)
        uri = g_filename_to_uri (*results_p, NULL, NULL);
      else
        uri = *results_p;

      if (uri)
	hit_uris = g_list_prepend (hit_uris, uri);
    }

  _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (tracker), hit_uris);
  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (tracker));

  g_strfreev  (results);
  if (tracker->priv->version == TRACKER_0_6)
    g_list_foreach (hit_uris, (GFunc)g_free, NULL);
  g_list_free (hit_uris);
}


static void
gtk_search_engine_tracker_start (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;
  gchar	*search_text, *location, *location_uri;

  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);

  if (tracker->priv->query_pending)
    return;

  if (tracker->priv->query == NULL)
    return;
	
  search_text = _gtk_query_get_text (tracker->priv->query);
  location_uri = _gtk_query_get_location (tracker->priv->query);

  location = NULL;
  if (location_uri)
    {
      if (tracker->priv->version == TRACKER_0_6)
        {
          location = g_filename_from_uri (location_uri, NULL, NULL);
          g_free (location_uri);
        }
      else
        location = location_uri;
    }

  if (location) 
    {
      tracker_search_metadata_by_text_and_location_async (tracker->priv->client,
							  search_text, 
                                                          location, 
                                                          search_callback,
                                                          tracker);
      g_free (location);
    }
  else 
    {
      tracker_search_metadata_by_text_async (tracker->priv->client,
                                             search_text, 
                                             search_callback,
                                             tracker);
    }

  tracker->priv->query_pending = TRUE;
  g_free (search_text);
}

static void
gtk_search_engine_tracker_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineTracker *tracker;
  
  tracker = GTK_SEARCH_ENGINE_TRACKER (engine);
  
  if (tracker->priv->query && tracker->priv->query_pending) 
    {
      tracker_cancel_last_call (tracker->priv->client);
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
  engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine, GTK_TYPE_SEARCH_ENGINE_TRACKER, GtkSearchEngineTrackerPrivate);
}


GtkSearchEngine *
_gtk_search_engine_tracker_new (void)
{
  GtkSearchEngineTracker *engine;
  TrackerClient *tracker_client;
  TrackerVersion version;
  GError *err = NULL;

  version = open_libtracker ();

  if (!tracker_connect)
    return NULL;

  tracker_client = tracker_connect (FALSE, -1);
  
  if (!tracker_client)
    return NULL;

  if (version == TRACKER_0_6)
    {
      if (!tracker_get_version)
        return NULL;

      tracker_get_version (tracker_client, &err);

      if (err != NULL)
        {
          g_error_free (err);
          tracker_disconnect (tracker_client);
          return NULL;
        }
    }

  engine = g_object_new (GTK_TYPE_SEARCH_ENGINE_TRACKER, NULL);

  engine->priv->client = tracker_client;
  engine->priv->query_pending = FALSE;
  engine->priv->version = version;
  
  return GTK_SEARCH_ENGINE (engine);
}
