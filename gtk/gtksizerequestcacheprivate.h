/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_SIZE_REQUEST_CACHE_PRIVATE_H__
#define __GTK_SIZE_REQUEST_CACHE_PRIVATE_H__

#include <glib.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

/* Cache as many ranges of height-for-width
 * (or width-for-height) as can be rational
 * for a said widget to have, if a label can
 * only wrap to 3 lines, only 3 caches will
 * ever be allocated for it.
 */
#define GTK_SIZE_REQUEST_CACHED_SIZES   (5)

typedef struct {
  gint minimum_size;
  gint natural_size;
} CachedSize;

typedef struct
{
  gint       lower_for_size; /* The minimum for_size with the same result */
  gint       upper_for_size; /* The maximum for_size with the same result */
  CachedSize cached_size;
} SizeRequest;

typedef struct {
  SizeRequest **requests[2];

  CachedSize  cached_size[2];

  GtkSizeRequestMode request_mode   : 3;
  guint       request_mode_valid    : 1;
  struct {
    guint       n_cached_requests   : 3;
    guint       last_cached_request : 3;
    guint       cached_size_valid   : 1;
  }           flags[2];
} SizeRequestCache;

void            _gtk_size_request_cache_init                    (SizeRequestCache       *cache);
void            _gtk_size_request_cache_free                    (SizeRequestCache       *cache);

void            _gtk_size_request_cache_clear                   (SizeRequestCache       *cache);
void            _gtk_size_request_cache_commit                  (SizeRequestCache       *cache,
                                                                 GtkOrientation          orientation,
                                                                 gint                    for_size,
                                                                 gint                    minimum_size,
                                                                 gint                    natural_size);
gboolean        _gtk_size_request_cache_lookup                  (SizeRequestCache       *cache,
                                                                 GtkOrientation          orientation,
                                                                 gint                    for_size,
                                                                 gint                   *minimum,
                                                                 gint                   *natural);

G_END_DECLS

#endif /* __GTK_SIZE_REQUEST_CACHE_PRIVATE_H__ */
