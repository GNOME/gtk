/* updateiconcache.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#ifdef _MSC_VER
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

static gboolean force_update = FALSE;
static gboolean quiet = FALSE;

#define CACHE_NAME "icon-theme.cache"

#define HAS_SUFFIX_XPM (1 << 0)
#define HAS_SUFFIX_SVG (1 << 1)
#define HAS_SUFFIX_PNG (1 << 2)
#define HAS_ICON_FILE  (1 << 3)

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define HASH_OFFSET 12

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

gboolean
is_cache_up_to_date (const gchar *path)
{
  struct stat path_stat, cache_stat;
  gchar *cache_path;
  int retval;
  
  retval = g_stat (path, &path_stat);

  if (retval < 0)
    {
      /* We can't stat the path,
       * assume we have a updated cache */
      return TRUE;
    }

  cache_path = g_build_filename (path, CACHE_NAME, NULL);
  retval = g_stat (cache_path, &cache_stat);
  g_free (cache_path);
  
  if (retval < 0)
    {
      /* Cache file not found */
      return FALSE;
    }

  /* Check mtime */
  return cache_stat.st_mtime >= path_stat.st_mtime;
}

typedef struct
{
  int flags;
  int dir_index;
} Image;

static gboolean
foreach_remove_func (gpointer key, gpointer value, gpointer user_data)
{
  Image *image = (Image *)value;
  GHashTable *files = user_data;
  GList *list;
  gboolean free_key = FALSE;  

  if (image->flags == HAS_ICON_FILE)
    {
      g_free (key);
      g_free (image);

      return TRUE;
    }

  list = g_hash_table_lookup (files, key);
  if (list)
    free_key = TRUE;
  
  list = g_list_prepend (list, value);
  g_hash_table_insert (files, key, list);
  
  if (free_key)
    g_free (key);
  
  return TRUE;
}

GList *
scan_directory (const gchar *base_path, 
		const gchar *subdir, 
		GHashTable  *files, 
		GList       *directories,
		gint         depth)
{
  GHashTable *dir_hash;
  GDir *dir;
  const gchar *name;
  gchar *dir_path;
  gboolean dir_added = FALSE;
  guint dir_index = 0xffff;
  
  dir_path = g_build_filename (base_path, subdir, NULL);

  /* FIXME: Use the gerror */
  dir = g_dir_open (dir_path, 0, NULL);
  
  if (!dir)
    return directories;
  
  dir_hash = g_hash_table_new (g_str_hash, g_str_equal);

  while ((name = g_dir_read_name (dir)))
    {
      gchar *path;
      gboolean retval;
      int flags = 0;
      Image *image;
      gchar *basename, *dot;

      path = g_build_filename (dir_path, name, NULL);
      retval = g_file_test (path, G_FILE_TEST_IS_DIR);
      if (retval)
	{
	  gchar *subsubdir;

	  if (subdir)
	    subsubdir = g_build_filename (subdir, name, NULL);
	  else
	    subsubdir = g_strdup (name);
	  directories = scan_directory (base_path, subsubdir, files, 
					directories, depth + 1);
	  g_free (subsubdir);

	  continue;
	}

      retval = g_file_test (path, G_FILE_TEST_IS_REGULAR);
      g_free (path);
      
      if (retval)
	{
	  if (g_str_has_suffix (name, ".png"))
	    flags |= HAS_SUFFIX_PNG;
	  else if (g_str_has_suffix (name, ".svg"))
	    flags |= HAS_SUFFIX_SVG;
	  else if (g_str_has_suffix (name, ".xpm"))
	    flags |= HAS_SUFFIX_XPM;
	  else if (g_str_has_suffix (name, ".icon"))
	    flags |= HAS_ICON_FILE;
	  
	  if (flags == 0)
	    continue;
	  
	  basename = g_strdup (name);
	  dot = strrchr (basename, '.');
	  *dot = '\0';
	  
	  image = g_hash_table_lookup (dir_hash, basename);
	  if (image)
	    image->flags |= flags;
	  else
	    {
	      if (!dir_added) 
		{
		  dir_added = TRUE;
		  if (subdir)
		    {
		      dir_index = g_list_length (directories);
		      directories = g_list_append (directories, g_strdup (subdir));
		    }
		  else
		    dir_index = 0xffff;
		}
		
	      image = g_new0 (Image, 1);
	      image->flags = flags;
	      image->dir_index = dir_index;
	      
	      g_hash_table_insert (dir_hash, g_strdup (basename), image);
	    }

	  g_free (basename);
	}
    }

  g_dir_close (dir);

  /* Move dir into the big file hash */
  g_hash_table_foreach_remove (dir_hash, foreach_remove_func, files);
  
  g_hash_table_destroy (dir_hash);

  return directories;
}

typedef struct _HashNode HashNode;

struct _HashNode
{
  HashNode *next;
  gchar *name;
  GList *image_list;
};

static guint
icon_name_hash (gconstpointer key)
{
  const signed char *p = key;
  guint32 h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}

typedef struct {
  gint size;
  HashNode **nodes;
} HashContext;

static gboolean
convert_to_hash (gpointer key, gpointer value, gpointer user_data)
{
  HashContext *context = user_data;
  guint hash;
  HashNode *node;
  
  hash = icon_name_hash (key) % context->size;

  node = g_new0 (HashNode, 1);
  node->next = NULL;
  node->name = key;
  node->image_list = value;

  if (context->nodes[hash] != NULL)
    node->next = context->nodes[hash];

  context->nodes[hash] = node;
  
  return TRUE;
}

gboolean
write_string (FILE *cache, const gchar *n)
{
  gchar *s;
  int i, l;
  
  l = ALIGN_VALUE (strlen (n) + 1, 4);
  
  s = g_malloc0 (l);
  strcpy (s, n);

  i = fwrite (s, l, 1, cache);

  return i == 1;
  
}

gboolean
write_card16 (FILE *cache, guint16 n)
{
  int i;

  n = GUINT16_TO_BE (n);
  
  i = fwrite ((char *)&n, 2, 1, cache);

  return i == 1;
}

gboolean
write_card32 (FILE *cache, guint32 n)
{
  int i;

  n = GUINT32_TO_BE (n);
  
  i = fwrite ((char *)&n, 4, 1, cache);

  return i == 1;
}

static gboolean
write_header (FILE *cache, guint32 dir_list_offset)
{
  return (write_card16 (cache, MAJOR_VERSION) &&
	  write_card16 (cache, MINOR_VERSION) &&
	  write_card32 (cache, HASH_OFFSET) &&
	  write_card32 (cache, dir_list_offset));
}


guint
get_single_node_size (HashNode *node)
{
  int len = 0;

  /* Node pointers */
  len += 12;

  /* Name */
  len += ALIGN_VALUE (strlen (node->name) + 1, 4);

  /* Image list */
  len += 4 + g_list_length (node->image_list) * 8;

  return len;
}

guint
get_bucket_size (HashNode *node)
{
  int len = 0;

  while (node)
    {
      len += get_single_node_size (node);

      node = node->next;
    }

  return len;
}

gboolean
write_bucket (FILE *cache, HashNode *node, int *offset)
{
  while (node != NULL)
    {
      int next_offset = *offset + get_single_node_size (node);
      int i, len;
      GList *list;
	  
      /* Chain offset */
      if (node->next != NULL)
	{
	  if (!write_card32 (cache, next_offset))
	    return FALSE;
	}
      else
	{
	  if (!write_card32 (cache, 0xffffffff))
	    return FALSE;
	}
      
      /* Icon name offset */
      if (!write_card32 (cache, *offset + 12))
	return FALSE;
      
      /* Image list offset */
      if (!write_card32 (cache, *offset + 12 + ALIGN_VALUE (strlen (node->name) + 1, 4)))
	return FALSE;
      
      /* Icon name */
      if (!write_string (cache, node->name))
	return FALSE;
      
      /* Image list */
      len = g_list_length (node->image_list);
      if (!write_card32 (cache, len))
	return FALSE;
      
      list = node->image_list;
      for (i = 0; i < len; i++)
	{
	  Image *image = list->data;
	  
	  /* Directory index */
	  if (!write_card16 (cache, image->dir_index))
	    return FALSE;
	  
	  /* Flags */
	  if (!write_card16 (cache, image->flags))
	    return FALSE;
	  
	  /* Image data offset */
	  if (!write_card32 (cache, 0))
	    return FALSE;
	  
	  list = list->next;
	}

      *offset = next_offset;
      node = node->next;
    }
  
  return TRUE;
}

gboolean
write_hash_table (FILE *cache, HashContext *context, int *new_offset)
{
  int offset = HASH_OFFSET;
  int node_offset;
  int i;

  if (!(write_card32 (cache, context->size)))
    return FALSE;

  /* Size int + size * 4 */
  node_offset = offset + 4 + context->size * 4;
  
  for (i = 0; i < context->size; i++)
    {
      if (context->nodes[i] != NULL)
	{
	  if (!write_card32 (cache, node_offset))
	    return FALSE;
	  
	  node_offset += get_bucket_size (context->nodes[i]);
	}
      else
	{
	  if (!write_card32 (cache, 0xffffffff))
	    {
	      return FALSE;
	    }
	}
    }

  *new_offset = node_offset;

  /* Now write the buckets */
  node_offset = offset + 4 + context->size * 4;
  
  for (i = 0; i < context->size; i++)
    {
      if (!context->nodes[i])
	continue;

      if (!write_bucket (cache, context->nodes[i], &node_offset))
	return FALSE;
    }

  return TRUE;
}

gboolean
write_dir_index (FILE *cache, int offset, GList *directories)
{
  int n_dirs;
  GList *d;
  char *dir;

  n_dirs = g_list_length (directories);

  if (!write_card32 (cache, n_dirs))
    return FALSE;

  offset += 4 + n_dirs * 4;

  for (d = directories; d; d = d->next)
    {
      dir = d->data;
      if (!write_card32 (cache, offset))
	return FALSE;
      
      offset += ALIGN_VALUE (strlen (dir) + 1, 4);
    }

  for (d = directories; d; d = d->next)
    {
      dir = d->data;

      if (!write_string (cache, dir))
	return FALSE;
    }
  
  return TRUE;
}

gboolean
write_file (FILE *cache, GHashTable *files, GList *directories)
{
  HashContext context;
  int new_offset;

  /* Convert the hash table into something looking a bit more
   * like what we want to write to disk.
   */
  context.size = g_spaced_primes_closest (g_hash_table_size (files) / 3);
  context.nodes = g_new0 (HashNode *, context.size);
  
  g_hash_table_foreach_remove (files, convert_to_hash, &context);

  /* Now write the file */
  /* We write 0 as the directory list offset and go
   * back and change it later */
  if (!write_header (cache, 0))
    {
      g_printerr ("Failed to write header\n");
      return FALSE;
    }

  if (!write_hash_table (cache, &context, &new_offset))
    {
      g_printerr ("Failed to write hash table\n");
      return FALSE;
    }

  if (!write_dir_index (cache, new_offset, directories))
    {
      g_printerr ("Failed to write directory index\n");
      return FALSE;
    }
  
  rewind (cache);

  if (!write_header (cache, new_offset))
    {
      g_printerr ("Failed to rewrite header\n");
      return FALSE;
    }
    
  return TRUE;
}

void
build_cache (const gchar *path)
{
  gchar *cache_path, *tmp_cache_path;
  GHashTable *files;
  gboolean retval;
  FILE *cache;
  struct stat path_stat, cache_stat;
  struct utimbuf utime_buf;
  GList *directories = NULL;
  
  tmp_cache_path = g_build_filename (path, "."CACHE_NAME, NULL);
  cache = g_fopen (tmp_cache_path, "wb");
  
  if (!cache)
    {
      g_printerr ("Failed to write cache file: %s\n", g_strerror (errno));
      exit (1);
    }

  files = g_hash_table_new (g_str_hash, g_str_equal);
  
  directories = scan_directory (path, NULL, files, NULL, 0);

  if (g_hash_table_size (files) == 0)
    {
      /* Empty table, just close and remove the file */

      fclose (cache);
      g_unlink (tmp_cache_path);
      exit (0);
    }
    
  /* FIXME: Handle failure */
  retval = write_file (cache, files, directories);
  fclose (cache);

  g_list_foreach (directories, (GFunc)g_free, NULL);
  g_list_free (directories);
  
  if (!retval)
    {
      g_unlink (tmp_cache_path);
      exit (1);
    }

  cache_path = g_build_filename (path, CACHE_NAME, NULL);

  if (g_rename (tmp_cache_path, cache_path) == -1)
    {
      g_unlink (tmp_cache_path);
      exit (1);
    }

  /* Update time */
  /* FIXME: What do do if an error occurs here? */
  if (g_stat (path, &path_stat) < 0 ||
      g_stat (cache_path, &cache_stat))
    exit (1);

  utime_buf.actime = path_stat.st_atime;
  utime_buf.modtime = cache_stat.st_mtime;
  utime (path, &utime_buf);
  
  if (!quiet)
    g_printerr ("Cache file created successfully.\n");
}

static GOptionEntry args[] = {
  { "force", 'f', 0, G_OPTION_ARG_NONE, &force_update, "Overwrite an existing cache, even if uptodate", NULL },
  { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, "Turn off verbose output", NULL },
  { NULL }
};

int
main (int argc, char **argv)
{
  gchar *path;
  GOptionContext *context;

  if (argc < 2)
    return 0;

  context = g_option_context_new ("ICONPATH");
  g_option_context_add_main_entries (context, args, NULL);

  g_option_context_parse (context, &argc, &argv, NULL);
  
  path = argv[1];
#ifdef G_OS_WIN32
  path = g_locale_to_utf8 (path, -1, NULL, NULL, NULL);
#endif
  
  if (!force_update && is_cache_up_to_date (path))
    return 0;

  build_cache (path);
  
  return 0;
}
