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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 * Based on nautilus-search-engine-beagle.c
 */

#include "config.h"
#include <gmodule.h>
#include "gtksearchenginebeagle.h"

#if 0
#include <beagle/beagle.h>
#endif

/* We dlopen() all the following from libbeagle at runtime */

typedef struct _BeagleHit BeagleHit;
typedef struct _BeagleQuery BeagleQuery;
typedef struct _BeagleClient BeagleClient;
typedef struct _BeagleRequest BeagleRequest;
typedef struct _BeagleFinishedResponse BeagleFinishedResponse;
typedef struct _BeagleHitsAddedResponse BeagleHitsAddedResponse;
typedef struct _BeagleHitsSubtractedResponse BeagleHitsSubtractedResponse;
typedef struct _BeagleQueryPartProperty BeagleQueryPartProperty;
typedef struct _BeagleQueryPart BeagleQueryPart;

#define BEAGLE_HIT(x) ((BeagleHit *)(x))
#define BEAGLE_REQUEST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), beagle_request_get_type(), BeagleRequest))
#define BEAGLE_QUERY_PART(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), beagle_query_part_get_type(), BeagleQueryPart))

typedef enum 
{
  BEAGLE_QUERY_PART_LOGIC_REQUIRED   = 1,
  BEAGLE_QUERY_PART_LOGIC_PROHIBITED = 2
} BeagleQueryPartLogic;

typedef enum 
  {
    BEAGLE_PROPERTY_TYPE_UNKNOWN = 0,
    BEAGLE_PROPERTY_TYPE_TEXT    = 1,
    BEAGLE_PROPERTY_TYPE_KEYWORD = 2,
    BEAGLE_PROPERTY_TYPE_DATE    = 3,
    BEAGLE_PROPERTY_TYPE_LAST    = 4
} BeaglePropertyType;

/* *static* wrapper function pointers */
static gboolean (*beagle_client_send_request_async) (BeagleClient   *client,
						     BeagleRequest  *request,
						     GError        **err) = NULL;
static const char *(*beagle_hit_get_uri) (BeagleHit *hit) = NULL;
static GSList *(*beagle_hits_added_response_get_hits) (BeagleHitsAddedResponse *response) = NULL;
static GSList *(*beagle_hits_subtracted_response_get_uris) (BeagleHitsSubtractedResponse *response) = NULL;
static BeagleQuery *(*beagle_query_new) (void) = NULL;
static void (*beagle_query_add_text) (BeagleQuery     *query,
				      const char      *str) = NULL;
static void (*beagle_query_set_max_hits) (BeagleQuery *query,
					  gint         max_hits) = NULL;
static BeagleQueryPartProperty *(*beagle_query_part_property_new) (void) = NULL;
static void (*beagle_query_part_set_logic) (BeagleQueryPart      *part, 
					    BeagleQueryPartLogic  logic) = NULL;
static void (*beagle_query_part_property_set_key) (BeagleQueryPartProperty *part, 
						   const char              *key) = NULL;
static void (*beagle_query_part_property_set_value) (BeagleQueryPartProperty *part, 
						     const char *             value) = NULL;
static void (*beagle_query_part_property_set_property_type) (BeagleQueryPartProperty *part, 
							     BeaglePropertyType       prop_type) = NULL;
static void (*beagle_query_add_part) (BeagleQuery     *query, 
				      BeagleQueryPart *part) = NULL;
static GType (*beagle_request_get_type) (void) = NULL;
static GType (*beagle_query_part_get_type) (void) = NULL;
static gboolean (*beagle_util_daemon_is_running) (void) = NULL;
static BeagleClient *(*beagle_client_new) (const char *client_name) = NULL;

static struct BeagleDlMapping
{
  const char *fn_name;
  gpointer *fn_ptr_ref;
} beagle_dl_mapping[] =
{
#define MAP(a) { #a, (gpointer *)&a }
  MAP (beagle_client_send_request_async),
  MAP (beagle_hit_get_uri),
  MAP (beagle_hits_added_response_get_hits),
  MAP (beagle_hits_subtracted_response_get_uris),
  MAP (beagle_query_new),
  MAP (beagle_query_add_text),
  MAP (beagle_query_set_max_hits),
  MAP (beagle_query_part_property_new),
  MAP (beagle_query_part_set_logic),
  MAP (beagle_query_part_property_set_key),
  MAP (beagle_query_part_property_set_value),
  MAP (beagle_query_part_property_set_property_type),
  MAP (beagle_query_add_part),
  MAP (beagle_request_get_type),
  MAP (beagle_query_part_get_type),
  MAP (beagle_util_daemon_is_running),
  MAP (beagle_client_new)
#undef MAP
};

static void 
open_libbeagle (void)
{
  static gboolean done = FALSE;

  if (!done)
    {
      int i;
      GModule *beagle;
      
      done = TRUE;
 
      beagle = g_module_open ("libbeagle.so.1", G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
      if (!beagle)
        beagle = g_module_open ("libbeagle.so.0", G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

      if (!beagle)
	return;
      
      for (i = 0; i < G_N_ELEMENTS (beagle_dl_mapping); i++)
	{
	  if (!g_module_symbol (beagle, beagle_dl_mapping[i].fn_name,
				beagle_dl_mapping[i].fn_ptr_ref))
	    {
	      g_warning ("Missing symbol '%s' in libbeagle\n",
			 beagle_dl_mapping[i].fn_name);
	      g_module_close (beagle);

	      for (i = 0; i < G_N_ELEMENTS (beagle_dl_mapping); i++)
		beagle_dl_mapping[i].fn_ptr_ref = NULL;

	      return;
	    }
	}
    }
}


struct _GtkSearchEngineBeaglePrivate 
{
  BeagleClient *client;
  GtkQuery *query;

  BeagleQuery *current_query;
  char *current_query_uri_prefix;
  gboolean query_finished;
};


G_DEFINE_TYPE (GtkSearchEngineBeagle, _gtk_search_engine_beagle, GTK_TYPE_SEARCH_ENGINE);

static void
finalize (GObject *object)
{
  GtkSearchEngineBeagle *beagle;
  
  beagle = GTK_SEARCH_ENGINE_BEAGLE (object);
  
  if (beagle->priv->current_query) 
    {
      g_object_unref (beagle->priv->current_query);
      beagle->priv->current_query = NULL;
      g_free (beagle->priv->current_query_uri_prefix);
      beagle->priv->current_query_uri_prefix = NULL;
    }

  if (beagle->priv->query) 
    {
      g_object_unref (beagle->priv->query);
      beagle->priv->query = NULL;
    }

  if (beagle->priv->client) 
    {
      g_object_unref (beagle->priv->client);
      beagle->priv->client = NULL;
    }

  G_OBJECT_CLASS (_gtk_search_engine_beagle_parent_class)->finalize (object);
}

static void
beagle_hits_added (BeagleQuery             *query, 
		   BeagleHitsAddedResponse *response, 
		   GtkSearchEngineBeagle   *engine)
{
  GSList *hits, *list;
  GList *hit_uris;
  const gchar *uri;
  
  hit_uris = NULL;
  
  hits = beagle_hits_added_response_get_hits (response);
  
  for (list = hits; list != NULL; list = list->next) 
    {
      BeagleHit *hit = BEAGLE_HIT (list->data);
      
      uri = beagle_hit_get_uri (hit);
      
      if (engine->priv->current_query_uri_prefix &&
	  !g_str_has_prefix (uri, engine->priv->current_query_uri_prefix)) 
	continue;
		
      hit_uris = g_list_prepend (hit_uris, (char *)uri);
    }

  _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (engine), hit_uris);
  g_list_free (hit_uris);
}

static void
beagle_hits_subtracted (BeagleQuery                  *query, 
			BeagleHitsSubtractedResponse *response, 
			GtkSearchEngineBeagle        *engine)
{
  GSList *uris, *list;
  GList *hit_uris;
  
  hit_uris = NULL;
  
  uris = beagle_hits_subtracted_response_get_uris (response);
  
  for (list = uris; list != NULL; list = list->next) 
    {
      hit_uris = g_list_prepend (hit_uris, (char *)list->data);
    }

  _gtk_search_engine_hits_subtracted (GTK_SEARCH_ENGINE (engine), hit_uris);
  g_list_free (hit_uris);
}

static void
beagle_finished (BeagleQuery            *query, 
		 BeagleFinishedResponse *response,
		 GtkSearchEngineBeagle  *engine)
{
  /* For some reason we keep getting finished events,
   * only emit finished once */
  if (engine->priv->query_finished) 
    return;
  
  engine->priv->query_finished = TRUE;
  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (engine));
}

static void
beagle_error (BeagleQuery           *query,
	      GError                *error,
	      GtkSearchEngineBeagle *engine)
{
  _gtk_search_engine_error (GTK_SEARCH_ENGINE (engine), error->message);
}

static void
gtk_search_engine_beagle_start (GtkSearchEngine *engine)
{
  GtkSearchEngineBeagle *beagle;
  GError *error;
  gchar *text;
  gchar *query;

  error = NULL;
  beagle = GTK_SEARCH_ENGINE_BEAGLE (engine);
  
  g_return_if_fail (beagle->priv->query != NULL);

  if (beagle->priv->current_query)
    return;

  beagle->priv->query_finished = FALSE;
  beagle->priv->current_query = beagle_query_new ();
  g_signal_connect (beagle->priv->current_query,
		    "hits-added", G_CALLBACK (beagle_hits_added), engine);
  g_signal_connect (beagle->priv->current_query,
		    "hits-subtracted", G_CALLBACK (beagle_hits_subtracted), engine);
  g_signal_connect (beagle->priv->current_query,
		    "finished", G_CALLBACK (beagle_finished), engine);
  g_signal_connect (beagle->priv->current_query,
		    "error", G_CALLBACK (beagle_error), engine);
  
  /* We only want files */
  
 
  text = _gtk_query_get_text (beagle->priv->query);
  query = g_strconcat (text, " type:File", NULL);
                          
  beagle_query_set_max_hits (beagle->priv->current_query, 1000);
  beagle_query_add_text (beagle->priv->current_query, query);
  
  beagle->priv->current_query_uri_prefix = _gtk_query_get_location (beagle->priv->query);
  
  if (!beagle_client_send_request_async (beagle->priv->client,
					 BEAGLE_REQUEST (beagle->priv->current_query), &error)) 
    {
      _gtk_search_engine_error (engine, error->message);
      g_error_free (error);
    }

  /* These must live during the lifetime of the query */
  g_free (text);
  g_free (query);
}

static void
gtk_search_engine_beagle_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineBeagle *beagle;
  
  beagle = GTK_SEARCH_ENGINE_BEAGLE (engine);
  
  if (beagle->priv->current_query) 
    {
      g_object_unref (beagle->priv->current_query);
      beagle->priv->current_query = NULL;

      g_free (beagle->priv->current_query_uri_prefix);
      beagle->priv->current_query_uri_prefix = NULL;
    }
}

static gboolean
gtk_search_engine_beagle_is_indexed (GtkSearchEngine *engine)
{
  return TRUE;
}

static void
gtk_search_engine_beagle_set_query (GtkSearchEngine *engine, 
				    GtkQuery        *query)
{
  GtkSearchEngineBeagle *beagle;
  
  beagle = GTK_SEARCH_ENGINE_BEAGLE (engine);
  
  if (query)
    g_object_ref (query);

  if (beagle->priv->query)
    g_object_unref (beagle->priv->query);

  beagle->priv->query = query;
}

static void
_gtk_search_engine_beagle_class_init (GtkSearchEngineBeagleClass *class)
{
  GObjectClass *gobject_class;
  GtkSearchEngineClass *engine_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = finalize;
  
  engine_class = GTK_SEARCH_ENGINE_CLASS (class);
  engine_class->set_query = gtk_search_engine_beagle_set_query;
  engine_class->start = gtk_search_engine_beagle_start;
  engine_class->stop = gtk_search_engine_beagle_stop;
  engine_class->is_indexed = gtk_search_engine_beagle_is_indexed;

  g_type_class_add_private (gobject_class, sizeof (GtkSearchEngineBeaglePrivate));
}

static void
_gtk_search_engine_beagle_init (GtkSearchEngineBeagle *engine)
{
  engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine, GTK_TYPE_SEARCH_ENGINE_BEAGLE, GtkSearchEngineBeaglePrivate);
}


GtkSearchEngine *
_gtk_search_engine_beagle_new (void)
{
  GtkSearchEngineBeagle *engine;
  BeagleClient *client;

  open_libbeagle ();

  if (!beagle_util_daemon_is_running)
    return NULL;

  /* check whether daemon is running as beagle_client_new
   * doesn't fail when a stale socket file exists */
  if (!beagle_util_daemon_is_running ()) 
      return NULL;

  client = beagle_client_new (NULL);
  
  if (client == NULL)
    return NULL;
	
  engine = g_object_new (GTK_TYPE_SEARCH_ENGINE_BEAGLE, NULL);
  
  engine->priv->client = client;
  
  return GTK_SEARCH_ENGINE (engine);
}
