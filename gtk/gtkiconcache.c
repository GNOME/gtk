/* gtkiconcache.c
 * Copyright (C) 2004  Anders Carlsson <andersca@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gtkdebug.h"
#include "gtkiconcache.h"
#include <glib/gstdio.h>

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <string.h>

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#define GET_UINT16(cache, offset) (GUINT16_FROM_BE (*(guint16 *)((cache) + (offset))))
#define GET_UINT32(cache, offset) (GUINT32_FROM_BE (*(guint32 *)((cache) + (offset))))

struct _GtkIconCache {
  gint ref_count;

  gsize size;
  gchar *buffer;
#ifdef G_OS_WIN32
  HANDLE handle;
#endif
};

GtkIconCache *
_gtk_icon_cache_ref (GtkIconCache *cache)
{
  cache->ref_count ++;
  return cache;
}

void
_gtk_icon_cache_unref (GtkIconCache *cache)
{
  cache->ref_count --;

  if (cache->ref_count == 0)
    {
      GTK_NOTE (ICONTHEME, 
		g_print ("unmapping icon cache\n"));
#ifdef HAVE_MMAP
      munmap (cache->buffer, cache->size);
#endif
#ifdef G_OS_WIN32
      UnmapViewOfFile (cache->buffer);
      CloseHandle (cache->handle);
#endif
      g_free (cache);
    }
}

GtkIconCache *
_gtk_icon_cache_new_for_path (const gchar *path)
{
  GtkIconCache *cache = NULL;

#if defined(HAVE_MMAP) || defined(G_OS_WIN32)
  gchar *cache_filename;
  gint fd = -1;
  struct stat st;
  struct stat path_st;
  gchar *buffer = NULL;
#ifdef G_OS_WIN32
  HANDLE handle = NULL;
#endif

  if (g_getenv ("GTK_NO_ICON_CACHE"))
    return NULL;

  /* Check if we have a cache file */
  cache_filename = g_build_filename (path, "icon-theme.cache", NULL);

  GTK_NOTE (ICONTHEME, 
	    g_print ("look for cache in %s\n", path));

  if (!g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR))
    goto done;

  if (g_stat (path, &path_st) < 0)
    goto done;

  /* Open the file and map it into memory */
  fd = g_open (cache_filename, O_RDONLY|_O_BINARY, 0);

  if (fd < 0)
    {
      g_free (cache_filename);
      return NULL;
    }
  
  if (fstat (fd, &st) < 0 || st.st_size < 4)
    goto done;

  /* Verify cache is uptodate */
  if (st.st_mtime < path_st.st_mtime)
    {
      GTK_NOTE (ICONTHEME, 
		g_print ("cache outdated\n"));
      goto done; 
    }

#ifndef G_OS_WIN32
  buffer = (gchar *) mmap (NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if (buffer == MAP_FAILED)
    goto done;
#else
  handle = CreateFileMapping (_get_osfhandle (fd), NULL, PAGE_READONLY,
			      0, 0, NULL);
  if (handle == NULL)
    goto done;

  buffer = MapViewOfFile (handle, FILE_MAP_READ, 0, 0, 0);

  if (buffer == NULL)
    {
      CloseHandle (handle);
      goto done;
    }
#endif

  /* Verify version */
  if (GET_UINT16 (buffer, 0) != MAJOR_VERSION ||
      GET_UINT16 (buffer, 2) != MINOR_VERSION)
    {
#ifndef G_OS_WIN32
      munmap (buffer, st.st_size);
#else
      UnmapViewOfFile (buffer);
      CloseHandle (handle);
#endif
      GTK_NOTE (ICONTHEME, 
		g_print ("wrong cache version\n"));
      goto done;
    }
  
  GTK_NOTE (ICONTHEME, 
	    g_print ("found cache for %s\n", path));

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->buffer = buffer;
#ifdef G_OS_WIN32
  cache->handle = handle;
#endif
  cache->size = st.st_size;
 done:
  g_free (cache_filename);  
  if (fd != -1)
    close (fd);

#endif  /* HAVE_MMAP || G_OS_WIN32 */

  return cache;
}

static int
get_directory_index (GtkIconCache *cache,
		     const gchar *directory)
{
  guint32 dir_list_offset;
  int n_dirs;
  int i;
  
  dir_list_offset = GET_UINT32 (cache->buffer, 8);

  n_dirs = GET_UINT32 (cache->buffer, dir_list_offset);

  for (i = 0; i < n_dirs; i++)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, dir_list_offset + 4 + 4 * i);
      gchar *name = cache->buffer + name_offset;
      if (strcmp (name, directory) == 0)
	return i;
    }
  
  return -1;
}

gboolean
_gtk_icon_cache_has_directory (GtkIconCache *cache,
			       const gchar *directory)
{
  return get_directory_index (cache, directory) != -1;
}

static guint
icon_name_hash (gconstpointer key)
{
  const char *p = key;
  guint h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

gint
_gtk_icon_cache_get_icon_flags (GtkIconCache *cache,
				const gchar  *icon_name,
				const gchar  *directory)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  int hash, directory_index;
  guint32 image_list_offset, n_images;
  gboolean found = FALSE;
  int i;
  
  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      gchar *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
	{
	  found = TRUE;
	  break;
	}
	  
      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  if (!found)
    return 0;

  /* We've found an icon list, now check if we have the right icon in it */
  directory_index = get_directory_index (cache, directory);
  image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
  n_images = GET_UINT32 (cache->buffer, image_list_offset);
  
  for (i = 0; i < n_images; i++)
    {
      if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * i) ==
	  directory_index)
	return GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * i + 2);
    }
  
  return 0;
}

void
_gtk_icon_cache_add_icons (GtkIconCache *cache,
			   const gchar  *directory,
			   GHashTable   *hash_table)
{
  int directory_index;
  guint32 hash_offset, n_buckets;
  guint32 chain_offset;
  guint32 image_list_offset, n_images;
  int i, j;
  
  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return;
  
  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);
  
  for (i = 0; i < n_buckets; i++)
    {
      chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * i);
      while (chain_offset != 0xffffffff)
	{
	  guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
	  gchar *name = cache->buffer + name_offset;
	  
	  image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
	  n_images = GET_UINT32 (cache->buffer, image_list_offset);
  
	  for (j = 0; j < n_images; j++)
	    {
	      if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j) ==
		  directory_index)
		g_hash_table_insert (hash_table, name, NULL);
	    }

	  chain_offset = GET_UINT32 (cache->buffer, chain_offset);
	}
    }  
}

gboolean
_gtk_icon_cache_has_icon (GtkIconCache *cache,
			  const gchar  *icon_name)
{
  guint32 hash_offset;
  guint32 n_buckets;
  guint32 chain_offset;
  gint hash;
  
  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  hash = icon_name_hash (icon_name) % n_buckets;

  chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * hash);
  while (chain_offset != 0xffffffff)
    {
      guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
      gchar *name = cache->buffer + name_offset;

      if (strcmp (name, icon_name) == 0)
	return TRUE;
	  
      chain_offset = GET_UINT32 (cache->buffer, chain_offset);
    }

  return FALSE;
}
			  

