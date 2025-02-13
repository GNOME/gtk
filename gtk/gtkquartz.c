/* gtkquartz.c: Utility functions used by the Quartz port
 *
 * Copyright (C) 2006 Imendio AB
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

#include "config.h"

#include "gtkprivate.h"

#include <gdk/macos/gdkmacos.h>

#ifdef QUARTZ_RELOCATION

/* Bundle-based functions for various directories. These almost work
 * even when the application isn’t in a bundle, because mainBundle
 * paths point to the bin directory in that case. It’s a simple matter
 * to test for that and remove the last element.
 */

static const char *
get_bundle_path (void)
{
  static char *path = NULL;

  if (path == NULL)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      char *resource_path = g_strdup ([[[NSBundle mainBundle] resourcePath] UTF8String]);
      char *base;
      [pool drain];

      base = g_path_get_basename (resource_path);
      if (strcmp (base, "bin") == 0)
	path = g_path_get_dirname (resource_path);
      else
	path = strdup (resource_path);

      g_free (resource_path);
      g_free (base);
    }

  return path;
}

const char *
_gtk_get_datadir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", NULL);

  return path;
}

const char *
_gtk_get_libdir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "lib", NULL);

  return path;
}

const char *
_gtk_get_localedir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", "locale", NULL);

  return path;
}

const char *
_gtk_get_sysconfdir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "etc", NULL);

  return path;
}

const char *
_gtk_get_data_prefix (void)
{
  return get_bundle_path ();
}

#endif /* QUARTZ_RELOCATION */
