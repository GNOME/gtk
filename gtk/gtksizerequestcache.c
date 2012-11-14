/* gtksizerequest.c
 * Copyright (C) 2007-2010 Openismus GmbH
 *
 * Authors:
 *      Mathias Hasselmann <mathias@openismus.com>
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "gtksizerequestcacheprivate.h"

#include <string.h>

void
_gtk_size_request_cache_init (SizeRequestCache *cache)
{
  memset (cache, 0, sizeof (SizeRequestCache));
}

static void
free_sizes (SizeRequest **sizes)
{
  gint i;

  for (i = 0; i < GTK_SIZE_REQUEST_CACHED_SIZES && sizes[i] != NULL; i++)
    g_slice_free (SizeRequest, sizes[i]);
      
  g_slice_free1 (sizeof (SizeRequest *) * GTK_SIZE_REQUEST_CACHED_SIZES, sizes);
}

void
_gtk_size_request_cache_free (SizeRequestCache *cache)
{
  guint i;

  for (i = 0; i < 2; i++)
    {
      if (cache->requests[i])
        free_sizes (cache->requests[i]);
    }
}

void
_gtk_size_request_cache_clear (SizeRequestCache *cache)
                               
{
  _gtk_size_request_cache_free (cache);
  _gtk_size_request_cache_init (cache);
}

void
_gtk_size_request_cache_commit (SizeRequestCache *cache,
                                GtkOrientation    orientation,
                                gint              for_size,
                                gint              minimum_size,
                                gint              natural_size)
{
  SizeRequest **cached_sizes;
  SizeRequest  *cached_size;
  guint         i, n_sizes;

  /* First handle caching of the base requests */
  if (for_size < 0)
    {
      cache->cached_size[orientation].minimum_size = minimum_size;
      cache->cached_size[orientation].natural_size = natural_size;
      cache->flags[orientation].cached_size_valid = TRUE;
      return;
    }

  /* Check if the minimum_size and natural_size is already
   * in the cache and if this result can be used to extend
   * that cache entry 
   */
  cached_sizes = cache->requests[orientation];
  n_sizes = cache->flags[orientation].n_cached_requests;

  for (i = 0; i < n_sizes; i++)
    {
      if (cached_sizes[i]->cached_size.minimum_size == minimum_size &&
	  cached_sizes[i]->cached_size.natural_size == natural_size)
	{
	  cached_sizes[i]->lower_for_size = MIN (cached_sizes[i]->lower_for_size, for_size);
	  cached_sizes[i]->upper_for_size = MAX (cached_sizes[i]->upper_for_size, for_size);
	  return;
	}
    }

  /* If not found, pull a new size from the cache, the returned size cache
   * will immediately be used to cache the new computed size so we go ahead
   * and increment the last_cached_request right away */
  if (n_sizes < GTK_SIZE_REQUEST_CACHED_SIZES)
    {
      cache->flags[orientation].n_cached_requests++;
      cache->flags[orientation].last_cached_request = cache->flags[orientation].n_cached_requests - 1;
    }
  else
    {
      if (++cache->flags[orientation].last_cached_request == GTK_SIZE_REQUEST_CACHED_SIZES)
        cache->flags[orientation].last_cached_request = 0;
    }

  if (cache->requests[orientation] == NULL)
    cache->requests[orientation] = g_slice_alloc0 (sizeof (SizeRequest *) * GTK_SIZE_REQUEST_CACHED_SIZES);

  if (cache->requests[orientation][cache->flags[orientation].last_cached_request] == NULL)
    cache->requests[orientation][cache->flags[orientation].last_cached_request] = g_slice_new (SizeRequest);

  cached_size = cache->requests[orientation][cache->flags[orientation].last_cached_request];
  cached_size->lower_for_size = for_size;
  cached_size->upper_for_size = for_size;
  cached_size->cached_size.minimum_size = minimum_size;
  cached_size->cached_size.natural_size = natural_size;
}

/* looks for a cached size request for this for_size.
 *
 * Note that this caching code was originally derived from
 * the Clutter toolkit but has evolved for other GTK+ requirements.
 */
gboolean
_gtk_size_request_cache_lookup (SizeRequestCache *cache,
                                GtkOrientation    orientation,
                                gint              for_size,
                                gint             *minimum,
                                gint             *natural)
{
  CachedSize *result = NULL;

  if (for_size < 0)
    {
      if (cache->flags[orientation].cached_size_valid)
	result = &cache->cached_size[orientation];
    }
  else
    {
      guint i;

      /* Search for an already cached size */
      for (i = 0; i < cache->flags[orientation].n_cached_requests; i++)
        {
          SizeRequest *cur = cache->requests[orientation][i];

          if (cur->lower_for_size <= for_size &&
              cur->upper_for_size >= for_size)
            {
              result = &cur->cached_size;
              break;
            }
        }
    }

  if (result)
    {
      *minimum = result->minimum_size;
      *natural = result->natural_size;
      return TRUE;
    }
  else
    return FALSE;
}

