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

#include <config.h>
#include "gdk-pixbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include "test-images.h"
#include <time.h>
#include <string.h>

#define PRETEND_MEM_SIZE (16 * 1024 * 1024)
#define REMAINING_MEM_SIZE 5000


static int current_allocation = 0;
static int max_allocation = 0;

#define HEADER_SPACE sizeof(void*)

static gpointer
record_bytes (gpointer mem, gsize bytes)
{
  if (mem == NULL ||
      (current_allocation + bytes) > max_allocation)
    {
      if (mem)
        free (mem);
      
      return NULL;
    }
  
  *(void **)mem = GINT_TO_POINTER (bytes);

  g_assert (GPOINTER_TO_INT (*(void**)mem) == bytes);
  
  g_assert (current_allocation >= 0);
  current_allocation += bytes;
  g_assert (current_allocation >= 0);
  
  g_assert ( mem == (void*) ((((char*)mem) + HEADER_SPACE) - HEADER_SPACE) );
  return ((char*)mem) + HEADER_SPACE;
}

static gpointer
limited_try_malloc (gsize n_bytes)
{
  return record_bytes (malloc (n_bytes + HEADER_SPACE), n_bytes);
}

static gpointer
limited_malloc (gsize n_bytes)
{
  return limited_try_malloc (n_bytes);
}

static gpointer
limited_calloc (gsize n_blocks,
                gsize n_block_bytes)
{
  int bytes = n_blocks * n_block_bytes + HEADER_SPACE;
  gpointer mem = malloc (bytes);
  memset (mem, 0, bytes);
  return record_bytes (mem, n_blocks * n_block_bytes);
}

static void
limited_free (gpointer mem)
{
  gpointer real = ((char*)mem) - HEADER_SPACE;

  g_assert (current_allocation >= 0);
  current_allocation -= GPOINTER_TO_INT (*(void**)real);
  g_assert (current_allocation >= 0);
  
  free (real);
}

static gpointer
limited_try_realloc (gpointer mem,
                     gsize    n_bytes)
{
  if (mem == NULL)
    {
      return limited_try_malloc (n_bytes);
    }
  else
    {
      gpointer real;

      g_assert (mem);

      real = ((char*)mem) - HEADER_SPACE;
      
      g_assert (current_allocation >= 0);
      current_allocation -= GPOINTER_TO_INT (*(void**)real);
      g_assert (current_allocation >= 0);

      return record_bytes (realloc (real, n_bytes + HEADER_SPACE), n_bytes);
    }
}

static gpointer
limited_realloc (gpointer mem,
                 gsize    n_bytes)
{
  return limited_try_realloc (mem, n_bytes);
}

static GMemVTable limited_table = {
  limited_malloc,
  limited_realloc,
  limited_free,
  limited_calloc,
  limited_try_malloc,
  limited_try_realloc
};

static gboolean
test_loader (const guchar *bytes, gsize len, gboolean data_is_ok)
{
  GdkPixbufLoader *loader;
  GError *err = NULL;
  gboolean did_fail = FALSE;
  
  loader = gdk_pixbuf_loader_new ();
  gdk_pixbuf_loader_write (loader, bytes, len, &err);
  if (err)
    {
      g_error_free (err);
      err = NULL;
      did_fail = TRUE;
    }
  gdk_pixbuf_loader_close (loader, &err);
  if (err)
    {
      g_error_free (err);
      err = NULL;
      did_fail = TRUE;
    }
  g_object_unref (loader);
  
  if (data_is_ok == did_fail)
    return FALSE;
  else 
    return TRUE;
}

static void
mem_test (const guchar *bytes, gsize len)
{
  gboolean did_fail = FALSE;
  GError *err = NULL;
  GdkPixbufLoader *loader; 
  GList *loaders = NULL;
  GList *i;
  
  do {
    loader = gdk_pixbuf_loader_new ();
    gdk_pixbuf_loader_write (loader, bytes, len, &err);
    if (err)
      {
	g_error_free (err);
	err = NULL;
	did_fail = TRUE;
      }
    gdk_pixbuf_loader_close (loader, NULL);
    if (err)
      {
	g_error_free (err);
	err = NULL;
	did_fail = TRUE;
      }
    loaders = g_list_prepend (loaders, loader);
  } while (!did_fail);
  
  for (i = loaders; i != NULL; i = i->next)
    g_object_unref (i->data);
  g_list_free (loaders);
}

void
assault (const gchar *header, gsize header_size, 
	 int n_images, gboolean verbose)
{
  enum { N_CHARACTERS = 10000 };
  int j;
  for (j = 0; j < n_images; ++j)
    {
      GError *err = NULL;
      int i;
      GdkPixbufLoader *loader;
      
      if (verbose)
	g_print ("'img' no: %d\n", j);
      
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
	  
	  if (verbose)
	    {
	      int j;
	      for (j = 0; j < sizeof (r); j++)
		g_print ("%u, ", ((guchar *)&r)[j]);
	    }
	  
	  gdk_pixbuf_loader_write (loader, (guchar *)&r, sizeof (r), &err);
	  if (err)
	    {
	      g_error_free (err);
	      err = NULL;
	      break;
	    }
	}
      if (verbose)
	g_print ("\n");
      
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
randomly_modify (const guchar *image, guint size, gboolean verbose)
{
  int i;
  guchar *img_copy = g_malloc (size);
  for (i = 0; i < size; i++)
    img_copy [i] = image[i];
  
  for (i = 0; i < size / 4; i++)
    {
      int j;
      
      guint index = g_random_int_range (0, size);
      guchar byte = g_random_int_range (0, 256);
      
      img_copy[index] = byte;
      
      if (verbose)
	{
	  g_print ("img no %d\n", i);
	  for (j = 0; j < size; j++)
	    g_print ("%u, ", img_copy[j]);
	  g_print ("\n\n");
	}
      
      test_loader (img_copy, size, FALSE);
    }
  g_free (img_copy);
}

#define TEST(bytes, data_is_ok)					\
do {								\
	g_print ("%-40s", "                  " #bytes " ");	\
	fflush (stdout);					\
	if (test_loader (bytes, sizeof (bytes), data_is_ok))	\
	    g_print ("\tpassed\n");				\
	else							\
	    g_print ("\tFAILED\n");				\
} while (0)

#define LOWMEMTEST(bytes)					\
do {								\
	g_print ("%-40s", "memory            " #bytes " ");	\
	fflush (stdout);					\
	mem_test (bytes, sizeof (bytes));			\
	g_print ("\tpassed\n");					\
} while (0)

#define TEST_RANDOM(header, n_img, verbose)			\
do {								\
	static guchar h[] = { header };				\
	g_print ("%-40s", "random            " #header " ");	\
	fflush (stdout);					\
	assault (h, sizeof (h), n_img, verbose);		\
	g_print ("\tpassed\n");					\
} while (0)

#define TEST_RANDOMLY_MODIFIED(image, verbose)			\
do {								\
	g_print ("%-40s", "randomly modified " #image " ");	\
	fflush (stdout);					\
	randomly_modify (image, sizeof (image), verbose);	\
	g_print ("\tpassed\n");					\
} while (0)



static void
almost_exhaust_memory (void)
{
  gpointer x = g_malloc (REMAINING_MEM_SIZE);
  while (g_try_malloc (REMAINING_MEM_SIZE / 10))
    ;
  g_free (x);
}

static void
write_seed (int seed)
{
  FILE *f;
  /* write this so you can reproduce failed tests */
  f = fopen ("test-loaders-seed", "w");
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

  /* Set a malloc which emulates low mem */
  max_allocation = G_MAXINT;
  g_mem_set_vtable (&limited_table);
  
  if (argc > 1)
    seed = atoi (argv[1]);
  else
    {
      seed = time (NULL);
      write_seed (seed);
    }
  g_random_set_seed (seed);
  
  g_type_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
  
  putenv ("GDK_PIXBUF_MODULEDIR="BUILT_MODULES_DIR);

  TEST (valid_ppm_1, TRUE);
  TEST (valid_ppm_2, TRUE);
  TEST (valid_ppm_3, FALSE); /* image is valid, but we don't handle maxval > 255 */
  TEST (valid_ppm_4, TRUE);

  TEST (invalid_ppm_1, FALSE); /* this test fails to fail, because it's shorter than LOADER_HEADER_SIZE */
  TEST (invalid_ppm_2, FALSE);
  TEST (invalid_ppm_3, FALSE);
  TEST (invalid_ppm_4, FALSE);
  TEST (invalid_ppm_5, FALSE);
  TEST (invalid_ppm_6, FALSE);
  TEST (invalid_ppm_7, FALSE);
  TEST (invalid_ppm_8, FALSE);

  TEST (valid_gif_test, TRUE); 
  TEST (gif_test_1, FALSE);   
  TEST (gif_test_2, FALSE);   
  TEST (gif_test_3, FALSE);   
  TEST (gif_test_4, FALSE);   
  
  TEST (valid_png_test, TRUE);
  TEST (png_test_1, FALSE);   
  TEST (png_test_2, FALSE);


#if 0
  TEST (valid_ico_test, TRUE);
#endif
  
  TEST (ico_test_1, FALSE);
  
  TEST (valid_jpeg_test, TRUE);
  
  TEST (valid_tiff1_test, TRUE);
  TEST (tiff1_test_1, FALSE);
  TEST (tiff1_test_2, FALSE);

  TEST (valid_tga_test, TRUE);
  TEST (tga_test_1, FALSE);

  TEST (xpm_test_1, FALSE);
  
  TEST_RANDOM (GIF_HEADER, 150, FALSE);
  TEST_RANDOM (PNG_HEADER, 1100, FALSE);
  TEST_RANDOM (JPEG_HEADER, 800, FALSE);
  TEST_RANDOM (TIFF1_HEADER, 150, FALSE);
  TEST_RANDOM (TIFF2_HEADER, 150, FALSE);
#define PNM_HEADER 'P', '6'
  TEST_RANDOM (PNM_HEADER, 150, FALSE);
  
  TEST_RANDOMLY_MODIFIED (valid_tiff1_test, FALSE);
  TEST_RANDOMLY_MODIFIED (valid_gif_test, FALSE);
  TEST_RANDOMLY_MODIFIED (valid_png_test, FALSE);
  TEST_RANDOMLY_MODIFIED (valid_tga_test, FALSE);
  TEST_RANDOMLY_MODIFIED (valid_jpeg_test, FALSE);  /* The jpeg loader does not break */
#if 0
  TEST_RANDOMLY_MODIFIED (valid_ico_test, TRUE);    /* The ico loader does not seem to
						     * break, but the image tend to 
						     * mutate into a wbmp image, and
						     * the wbmp loader is broken
						     */
#endif
#if 0
  TEST (wbmp_test_1, FALSE); 
  TEST (wbmp_test_2, FALSE);
#endif
  /* memory tests */

  /* How do the loaders behave when memory is low?
     It depends on the state the above tests left the 
     memory in.

     - Sometimes the png loader tries to report an 
       "out of memory", but then g_strdup_printf() calls
       g_malloc(), which fails.
       
     - There are unchecked realloc()s inside libtiff, which means it
       will never work with low memory, unless something drastic is
       done, like allocating a lot of memory upfront and release it
       before entering libtiff.  Also, some TIFFReadRGBAImage calls
       returns successfully, even though they have called the error
       handler with an 'out of memory' message.
  */

  max_allocation = PRETEND_MEM_SIZE;
  almost_exhaust_memory ();

  g_print ("Allocated %dK of %dK, %dK free during tests\n",
           current_allocation / 1024, max_allocation / 1024,
           (max_allocation - current_allocation) / 1024);
  
#if 0
  LOWMEMTEST (valid_tiff1_test);  
#endif
  LOWMEMTEST (valid_gif_test);
  LOWMEMTEST (valid_png_test);
  LOWMEMTEST (valid_jpeg_test);
  
  return 0;
}
