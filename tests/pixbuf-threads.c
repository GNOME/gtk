/* -*- Mode: C; c-basic-offset: 2; -*- */
/* GdkPixbuf library - test loaders
 *
 * Copyright (C) 2004 Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gdk-pixbuf/gdk-pixbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gboolean verbose = FALSE;

static void
load_image (gpointer  data, 
	    gpointer user_data)
{
  gchar *filename = data;
  FILE *file;
  int nbytes;
  guchar buf[1024];
  size_t bufsize = 1024;
  GdkPixbufLoader *loader;
  GError *error = NULL;
  GThread *self;

  self = g_thread_self ();
  loader = gdk_pixbuf_loader_new ();

  file = fopen (filename, "r");
  g_assert (file);

  if (verbose) g_print ("%p start image %s\n", self, filename);
  while (!feof (file)) 
    {
      nbytes = fread (buf, 1, bufsize, file);
      if (!gdk_pixbuf_loader_write (loader, buf, nbytes, &error)) 
	{
	  g_warning ("Error writing %s to loader: %s", filename, error->message);
	  g_error_free (error);
          error = NULL;
	  break;
	}
      if (verbose) g_print ("%p read %d bytes\n", self, nbytes);

      g_thread_yield ();      
    }

  fclose (file);

  if (verbose) g_print ("%p finish image %s\n", self, filename);

  if (!gdk_pixbuf_loader_close (loader, &error)) 
    {
      g_warning ("Error closing loader for %s: %s", filename, error->message);
      g_error_free (error);
    }

  g_object_unref (loader);
}

static void
usage (void)
{
  g_print ("usage: pixbuf-threads [--verbose] <files>\n");
  exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  int i, start;
  GThreadPool *pool;
  
  g_type_init ();

  if (!g_thread_supported ())
    g_thread_init (NULL);

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

  if (argc == 1)
    usage();

  start = 1;
  if (strcmp (argv[1], "--verbose") == 0)
    {
      verbose = TRUE;
      start = 2;
    }
  
  pool = g_thread_pool_new (load_image, NULL, 20, FALSE, NULL);

  i = start;
  while (1) {
    i++;
    if (i == argc)
      i = start;
    g_thread_pool_push (pool, argv[i], NULL);
  }
  
  return 0;
}
