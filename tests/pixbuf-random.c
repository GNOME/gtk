/* -*- Mode: C; c-basic-offset: 2; -*- */
/* GdkPixbuf library - assault loaders with random data
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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static void
assault (const guchar *header, gsize header_size, int n_images)
{
  FILE *f;
  enum { N_CHARACTERS = 10000 };

  int j;

  for (j = 0; j < n_images; ++j)
    {
      GError *err = NULL;
      int i;
      GdkPixbufLoader *loader;
      
      f = fopen ("pixbuf-random-image", "w");
      if (!f)
	{
	  perror ("fopen");
	  exit (EXIT_FAILURE);
	}
  
      loader = gdk_pixbuf_loader_new ();
      
      gdk_pixbuf_loader_write (loader, header, header_size, &err);
      if (err)
	{
	  g_error_free (err);
	  continue;
	}
      
      for (i = 0; i < N_CHARACTERS; ++i)
	{
	  int r = g_random_int ();

	  fwrite (&r, 1, sizeof (r), f);
	  if (ferror (f))
	    {
	      perror ("fwrite");
	      exit (EXIT_FAILURE);
	    }
	  
	  gdk_pixbuf_loader_write (loader, (guchar *)&r, sizeof (r), &err);
	  if (err)
	    {
	      g_error_free (err);
	      err = NULL;
	      break;
	    }
	}
      
      fclose (f);
      
      gdk_pixbuf_loader_close (loader, &err);
      if (err)
	{
	  g_error_free (err);
	  err = NULL;
	}
      
      g_object_unref (loader);
    }
}

static void
write_seed (int seed)
{
  FILE *f;
  /* write this so you can reproduce failed tests */
  f = fopen ("pixbuf-random-seed", "w");
  if (!f)
    {
      perror ("fopen");
      exit (EXIT_FAILURE);
    }
  if (fprintf (f, "%d\n", seed) < 0)
    {
      perror ("fprintf");
      exit (EXIT_FAILURE);
    }
  if (fclose (f) < 0)
    {
      perror ("fclose");
      exit (EXIT_FAILURE);
    }
  g_print ("seed: %d\n", seed);
}

int
main (int argc, char **argv)
{
  int seed;

  if (argc > 1)
    seed = atoi (argv[1]);
  else
    {
      seed = time (NULL);
      write_seed (seed);
    }
  g_print ("the last tested image is saved to the file \"pixbuf-random-image\"\n\n");

  g_type_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
  
  g_random_set_seed (seed);

#define GIF_HEADER 'G', 'I', 'F', '8', '9', 'a'
#define PNG_HEADER 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
#define TIFF1_HEADER 'M', 'M', 0x00, 0x2a
#define TIFF2_HEADER 'I', 'I', 0x2a, 0x00
#define JPEG_HEADER 0xFF, 0xd8
#define PNM_HEADER 'P', '6'
#define XBM_HEADER '#', 'd', 'e', 'f', 'i', 'n', 'e', ' '  
#define BMP_HEADER 'B', 'M'  
#define XPM_HEADER '/', '*', ' ', 'X', 'P', 'M', ' ', '*', '/'
#define RAS_HEADER 0x59, 0xA6, 0x6A, 0x95

#define TEST_RANDOM(header, n_img)				\
do {								\
	static guchar h[] = { header };				\
	g_print (#header);					\
	fflush (stdout);					\
	assault (h, sizeof (h), n_img);				\
	g_print ("\t\tpassed\n");				\
} while (0)

  for (;;)
    {
      TEST_RANDOM (GIF_HEADER,   150);
      TEST_RANDOM (PNG_HEADER,   110);
      TEST_RANDOM (JPEG_HEADER,  800);
      TEST_RANDOM (TIFF1_HEADER, 150);
      TEST_RANDOM (TIFF2_HEADER, 150);
      TEST_RANDOM (PNM_HEADER,   150);
      TEST_RANDOM (XBM_HEADER,   150);
      TEST_RANDOM (BMP_HEADER,   150);
      TEST_RANDOM (XPM_HEADER,   150);
      TEST_RANDOM (RAS_HEADER,   300);
      g_print ("===========================\n");
    }
  
  return 0;
}
