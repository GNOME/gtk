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
free_sizes_x (SizeRequestX **sizes)
{
  gint i;

  for (i = 0; i < GTK_SIZE_REQUEST_CACHED_SIZES && sizes[i] != NULL; i++)
    g_slice_free (SizeRequestX, sizes[i]);

  g_slice_free1 (sizeof (SizeRequestY *) * GTK_SIZE_REQUEST_CACHED_SIZES, sizes);
}

static void
free_sizes_y (SizeRequestY **sizes)
{
  gint i;

  for (i = 0; i < GTK_SIZE_REQUEST_CACHED_SIZES && sizes[i] != NULL; i++)
    g_slice_free (SizeRequestY, sizes[i]);

  g_slice_free1 (sizeof (SizeRequestY *) * GTK_SIZE_REQUEST_CACHED_SIZES, sizes);
}

void
_gtk_size_request_cache_free (SizeRequestCache *cache)
{
  if (cache->requests_x)
    free_sizes_x (cache->requests_x);
  if (cache->requests_y)
    free_sizes_y (cache->requests_y);
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
                                gint              natural_size,
				gint              minimum_baseline,
				gint              natural_baseline)
{
  guint         i, n_sizes;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      g_assert (minimum_baseline == -1);
      g_assert (natural_baseline == -1);
    }

  /* First handle caching of the base requests */
  if (for_size < 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  cache->cached_size_x.minimum_size = minimum_size;
	  cache->cached_size_x.natural_size = natural_size;
	}
      else
	{
	  cache->cached_size_y.minimum_size = minimum_size;
	  cache->cached_size_y.natural_size = natural_size;
	  cache->cached_size_y.minimum_baseline = minimum_baseline;
	  cache->cached_size_y.natural_baseline = natural_baseline;
	}

      cache->flags[orientation].cached_size_valid = TRUE;
      return;
    }

  /* Check if the minimum_size and natural_size is already
   * in the cache and if this result can be used to extend
   * that cache entry 
   */
  n_sizes = cache->flags[orientation].n_cached_requests;


  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      SizeRequestX **cached_sizes;
      SizeRequestX  *cached_size;
      cached_sizes = cache->requests_x;

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

      if (cache->requests_x == NULL)
	cache->requests_x = g_slice_alloc0 (sizeof (SizeRequestX *) * GTK_SIZE_REQUEST_CACHED_SIZES);

      if (cache->requests_x[cache->flags[orientation].last_cached_request] == NULL)
	cache->requests_x[cache->flags[orientation].last_cached_request] = g_slice_new (SizeRequestX);

      cached_size = cache->requests_x[cache->flags[orientation].last_cached_request];
      cached_size->lower_for_size = for_size;
      cached_size->upper_for_size = for_size;
      cached_size->cached_size.minimum_size = minimum_size;
      cached_size->cached_size.natural_size = natural_size;
    }
  else
    {
      SizeRequestY **cached_sizes;
      SizeRequestY  *cached_size;
      cached_sizes = cache->requests_y;

      for (i = 0; i < n_sizes; i++)
	{
	  if (cached_sizes[i]->cached_size.minimum_size == minimum_size &&
	      cached_sizes[i]->cached_size.natural_size == natural_size &&
	      cached_sizes[i]->cached_size.minimum_baseline == minimum_baseline &&
	      cached_sizes[i]->cached_size.natural_baseline == natural_baseline)
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

      if (cache->requests_y == NULL)
	cache->requests_y = g_slice_alloc0 (sizeof (SizeRequestY *) * GTK_SIZE_REQUEST_CACHED_SIZES);

      if (cache->requests_y[cache->flags[orientation].last_cached_request] == NULL)
	cache->requests_y[cache->flags[orientation].last_cached_request] = g_slice_new (SizeRequestY);

      cached_size = cache->requests_y[cache->flags[orientation].last_cached_request];
      cached_size->lower_for_size = for_size;
      cached_size->upper_for_size = for_size;
      cached_size->cached_size.minimum_size = minimum_size;
      cached_size->cached_size.natural_size = natural_size;
      cached_size->cached_size.minimum_baseline = minimum_baseline;
      cached_size->cached_size.natural_baseline = natural_baseline;
    }
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
                                gint             *natural,
				gint             *minimum_baseline,
				gint             *natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      CachedSizeX *result = NULL;

      if (for_size < 0)
	{
	  if (cache->flags[orientation].cached_size_valid)
	    result = &cache->cached_size_x;
	}
      else
	{
	  guint i;

	  /* Search for an already cached size */
	  for (i = 0; i < cache->flags[orientation].n_cached_requests; i++)
	    {
	      SizeRequestX *cur = cache->requests_x[i];

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
	  *minimum_baseline = -1;
	  *natural_baseline = -1;
	  return TRUE;
	}
      else
	return FALSE;
    }
  else
    {
      CachedSizeY *result = NULL;

      if (for_size < 0)
	{
	  if (cache->flags[orientation].cached_size_valid)
	    result = &cache->cached_size_y;
	}
      else
	{
	  guint i;

	  /* Search for an already cached size */
	  for (i = 0; i < cache->flags[orientation].n_cached_requests; i++)
	    {
	      SizeRequestY *cur = cache->requests_y[i];

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
	  *minimum_baseline = result->minimum_baseline;
	  *natural_baseline = result->natural_baseline;
	  return TRUE;
	}
      else
	return FALSE;
    }
}

