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
  if (cache->widths)
    free_sizes (cache->widths);
  if (cache->heights)
    free_sizes (cache->heights);
}

void
_gtk_size_request_cache_clear (SizeRequestCache *cache)
                               
{
  _gtk_size_request_cache_free (cache);
  _gtk_size_request_cache_init (cache);
}

