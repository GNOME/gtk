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
 * Based on nautilus-query.c
 */

#include "config.h"
#include <string.h>

#include "gtkquery.h"

struct _GtkQueryPrivate
{
  gchar *text;
  gchar *location_uri;
  GList *mime_types;
  gchar **words;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkQuery, gtk_query, G_TYPE_OBJECT)

static void
finalize (GObject *object)
{
  GtkQuery *query;

  query = GTK_QUERY (object);

  g_free (query->priv->text);
  g_free (query->priv->location_uri);
  g_strfreev (query->priv->words);

  G_OBJECT_CLASS (gtk_query_parent_class)->finalize (object);
}

static void
gtk_query_class_init (GtkQueryClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = finalize;
}

static void
gtk_query_init (GtkQuery *query)
{
  query->priv = gtk_query_get_instance_private (query);
}

GtkQuery *
gtk_query_new (void)
{
  return g_object_new (GTK_TYPE_QUERY,  NULL);
}


gchar *
gtk_query_get_text (GtkQuery *query)
{
  return g_strdup (query->priv->text);
}

void
gtk_query_set_text (GtkQuery    *query,
                    const gchar *text)
{
  g_free (query->priv->text);
  query->priv->text = g_strdup (text);

  g_strfreev (query->priv->words);
  query->priv->words = NULL;
}

gchar *
gtk_query_get_location (GtkQuery *query)
{
  return g_strdup (query->priv->location_uri);
}

void
gtk_query_set_location (GtkQuery    *query,
                        const gchar *uri)
{
  g_free (query->priv->location_uri);
  query->priv->location_uri = g_strdup (uri);
}

static gchar *
prepare_string_for_compare (const gchar *string)
{
  gchar *normalized, *res;

  normalized = g_utf8_normalize (string, -1, G_NORMALIZE_NFD);
  res = g_utf8_strdown (normalized, -1);
  g_free (normalized);

  return res;
}

gboolean
gtk_query_matches_string (GtkQuery    *query,
                          const gchar *string)
{
  gchar *prepared;
  gboolean found;
  gint i;

  if (!query->priv->text)
    return FALSE;

  if (!query->priv->words)
    {
      prepared = prepare_string_for_compare (query->priv->text);
      query->priv->words = g_strsplit (prepared, " ", -1);
      g_free (prepared);
    }

  prepared = prepare_string_for_compare (string);

  found = TRUE;
  for (i = 0; query->priv->words[i]; i++)
    {
      if (strstr (prepared, query->priv->words[i]) == NULL)
        {
          found = FALSE;
          break;
        }
    }

  g_free (prepared);

  return found;
}
