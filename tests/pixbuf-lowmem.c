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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define PRETEND_MEM_SIZE (16 * 1024 * 1024)
#define REMAINING_MEM_SIZE 100000


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

static void
mem_test (const gchar *bytes, gsize len)
{
  gboolean did_fail = FALSE;
  GError *err = NULL;
  GdkPixbufLoader *loader; 
  GList *loaders = NULL;
  GList *i;
  
  do {
    loader = gdk_pixbuf_loader_new ();
    gdk_pixbuf_loader_write (loader, (guchar *) bytes, len, &err);
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

static void
almost_exhaust_memory (void)
{
  gpointer x = g_malloc (REMAINING_MEM_SIZE);
  while (g_try_malloc (REMAINING_MEM_SIZE / 10))
    ;
  g_free (x);
}

static void
usage (void)
{
  g_print ("usage: pixbuf-lowmem <pretend_memory_size> <files>\n");
  exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  int i;
  char *endptr;

  if (argc <= 2)
    usage();
  
  max_allocation = strtol (argv[1], &endptr, 10);
  if (endptr == argv[1])
    usage();

  /* Set a malloc which emulates low mem */
  g_mem_set_vtable (&limited_table);
  
  g_type_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
  
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

  almost_exhaust_memory ();

  g_print ("Allocated %dK of %dK, %dK free during tests\n",
           current_allocation / 1024, max_allocation / 1024,
           (max_allocation - current_allocation) / 1024);

  for (i = 2; i < argc; ++i)
    {
      gchar *contents;
      gsize size;
      GError *err = NULL;

      if (!g_file_get_contents (argv[i], &contents, &size, &err))
	{
	  g_print ("couldn't read %s: %s\n", argv[i], err->message);
	  exit (EXIT_FAILURE);
	}
      else
	{
	  g_print ("%-40s memory            ", argv[i]);
	  fflush (stdout);
	  mem_test (contents, size);
	  g_print ("\tpassed\n");
	  g_free (contents);
	}
    }
  
  return 0;
}
