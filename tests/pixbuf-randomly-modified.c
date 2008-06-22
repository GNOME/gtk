/* -*- Mode: C; c-basic-offset: 2; -*- */
/* GdkPixbuf library - test loaders
 *
 * Copyright (C) 2001 Søren Sandmann (sandmann@daimi.au.dk)
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
#include "glib.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static void
disaster (const char *what)
{
  perror (what);
  exit (EXIT_FAILURE);
}

static void
randomly_modify (const gchar *image, guint size)
{
  int i;

  guchar *img_copy = g_malloc (size);
  g_memmove (img_copy, image, size);
  
  for (i = 0; i < size / 4; i++)
    {
      FILE *f;
      GdkPixbufLoader *loader;
      
      guint index = g_random_int_range (0, size);
      guchar byte = g_random_int_range (0, 256);
      
      img_copy[index] = byte;
      f = fopen ("pixbuf-randomly-modified-image", "w");
      if (!f)
	disaster ("fopen");
      fwrite (img_copy, size, 1, f);
      if (ferror (f))
	disaster ("fwrite");
      fclose (f);

      loader = gdk_pixbuf_loader_new ();
      gdk_pixbuf_loader_write (loader, img_copy, size, NULL);
      gdk_pixbuf_loader_close (loader, NULL);
      g_object_unref (loader);
    }
  g_free (img_copy);
}

static void
write_seed (int seed)
{
  FILE *f;
  /* write this so you can reproduce failed tests */
  f = fopen ("pixbuf-randomly-modified-seed", "w");

  if (!f)
    disaster ("fopen");

  if (fprintf (f, "%d\n", seed) < 0)
    disaster ("fprintf");

  if (fclose (f) < 0)
    disaster ("fclose");

  g_print ("seed: %d\n", seed);
}

static void
usage (void)
{
  g_print ("usage: pixbuf-randomly-modified [-s <seed>] <files> ... \n");
  exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  int seed, i;
  gboolean got_seed = FALSE;
  GPtrArray *files = g_ptr_array_new ();

  if (argc == 1)
    usage ();
  
  seed = time (NULL);

  for (i = 1; i < argc; ++i)
    {
      if (strncmp (argv[i], "-s", 2) == 0)
	{
	  if (strlen (argv[i]) > 2)
	    usage();
	  if (i+1 < argc)
	    {
	      seed = atoi (argv[i+1]);
	      got_seed = TRUE;
	      ++i;
	    }
	  else
	    usage();
	}
      else
	g_ptr_array_add (files, strdup (argv[i]));
    }

  if (!got_seed)
    write_seed (seed);

  g_random_set_seed (seed);

  g_print ("the last tested image is saved to pixbuf-randomly-modified-image\n");
  
  g_type_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

  for (;;)
    for (i = 0; i < files->len; ++i)
      {
	gchar *contents;
	gsize size;
	GError *err = NULL;
	
	fflush (stdout);
	if (!g_file_get_contents (files->pdata[i], &contents, &size, &err))
	  {
	    g_print ("%s: error: %s\n", (char *)files->pdata[i], err->message);
	  }
	else
	  {
	    g_print ("%s\t\t", (char *)files->pdata[i]);
	    randomly_modify (contents, size);
	    g_print ("done\n");
	    
	    g_free (contents);
	  }
      }
  
  return 0;
}
