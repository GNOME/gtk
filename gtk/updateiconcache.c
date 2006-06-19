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
#include <gdk-pixbuf/gdk-pixdata.h>
#include <glib/gi18n.h>

static gboolean force_update = FALSE;
static gboolean ignore_theme_index = FALSE;
static gboolean quiet = FALSE;
static gboolean index_only = FALSE;
static gchar *var_name = "-";

#define CACHE_NAME "icon-theme.cache"

#define HAS_SUFFIX_XPM (1 << 0)
#define HAS_SUFFIX_SVG (1 << 1)
#define HAS_SUFFIX_PNG (1 << 2)
#define HAS_ICON_FILE  (1 << 3)

#define CAN_CACHE_IMAGE_DATA(flags) (!index_only && (((flags) & HAS_SUFFIX_PNG) || ((flags) & HAS_SUFFIX_XPM)))

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

gboolean
has_theme_index (const gchar *path)
{
  gboolean result;
  gchar *index_path;

  index_path = g_build_filename (path, "index.theme", NULL);

  result = g_file_test (index_path, G_FILE_TEST_IS_REGULAR);
  
  g_free (index_path);

  return result;
}


typedef struct 
{
  gboolean has_pixdata;
  GdkPixdata pixdata;
  guint32 offset;
  guint pixel_data_size;
} ImageData;

static GHashTable *image_data_hash = NULL;

typedef struct
{
  int flags;
  int dir_index;

  ImageData *image_data;
  guint pixel_data_size;

  int has_embedded_rect;
  int x0, y0, x1, y1;
  
  int n_attach_points;
  int *attach_points;
  
  int n_display_names;
  char **display_names;
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
      g_free (image->attach_points);
      g_strfreev (image->display_names);
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

static void
load_icon_data (Image *image, const char *path)
{
  GKeyFile *icon_file;
  char **split;
  gsize length;
  char *str;
  char *split_point;
  int i;
  gint *ivalues;
  GError *error = NULL;
  gchar **keys;
  gsize n_keys;
  
  icon_file = g_key_file_new ();
  g_key_file_set_list_separator (icon_file, ',');
  g_key_file_load_from_file (icon_file, path, G_KEY_FILE_KEEP_TRANSLATIONS, &error);
  if (error)
    {
      g_error_free (error);
      return;
    }

  ivalues = g_key_file_get_integer_list (icon_file, 
					 "Icon Data", "EmbeddedTextRectangle",
					 &length, NULL);
  if (ivalues)
    {
      if (length == 4)
	{
	  image->has_embedded_rect = TRUE;
	  image->x0 = ivalues[0];
	  image->y0 = ivalues[1];
	  image->x1 = ivalues[2];
	  image->y1 = ivalues[3];
	}
      
      g_free (ivalues);
    }
      
  str = g_key_file_get_string (icon_file, "Icon Data", "AttachPoints", NULL);
  if (str)
    {
      split = g_strsplit (str, "|", -1);
      
      image->n_attach_points = g_strv_length (split);
      image->attach_points = g_new (int, 2 * image->n_attach_points);

      i = 0;
      while (split[i] != NULL && i < image->n_attach_points)
	{
	  split_point = strchr (split[i], ',');
	  if (split_point)
	    {
	      *split_point = 0;
	      split_point++;
	      image->attach_points[2 * i] = atoi (split[i]);
	      image->attach_points[2 * i + 1] = atoi (split_point);
	    }
	  i++;
	}
      
      g_strfreev (split);
      g_free (str);
    }
      
  keys = g_key_file_get_keys (icon_file, "Icon Data", &n_keys, &error);
  image->display_names = g_new0 (gchar *, 2 * n_keys + 1); 
  image->n_display_names = 0;
  
  for (i = 0; i < n_keys; i++)
    {
      gchar *lang, *name;
      
      if (g_str_has_prefix (keys[i], "DisplayName"))
	{
	  gchar *open, *close = NULL;
	  
	  open = strchr (keys[i], '[');

	  if (open)
	    close = strchr (open, ']');

	  if (open && close)
	    {
	      lang = g_strndup (open + 1, close - open - 1);
	      name = g_key_file_get_locale_string (icon_file, 
						   "Icon Data", "DisplayName",
						   lang, NULL);
	    }
	  else
	    {
	      lang = g_strdup ("C");
	      name = g_key_file_get_string (icon_file, 
					    "Icon Data", "DisplayName",
					    NULL);
	    }
	  
	  image->display_names[2 * image->n_display_names] = lang;
	  image->display_names[2 * image->n_display_names + 1] = name;
	  image->n_display_names++;
	}
    }

  g_strfreev (keys);
  
  g_key_file_free (icon_file);
}

/*
 * This function was copied from gtkfilesystemunix.c, it should
 * probably go to GLib
 */
static void
canonicalize_filename (gchar *filename)
{
  gchar *p, *q;
  gboolean last_was_slash = FALSE;

  p = filename;
  q = filename;

  while (*p)
    {
      if (*p == G_DIR_SEPARATOR)
	{
	  if (!last_was_slash)
	    *q++ = G_DIR_SEPARATOR;

	  last_was_slash = TRUE;
	}
      else
	{
	  if (last_was_slash && *p == '.')
	    {
	      if (*(p + 1) == G_DIR_SEPARATOR ||
		  *(p + 1) == '\0')
		{
		  if (*(p + 1) == '\0')
		    break;

		  p += 1;
		}
	      else if (*(p + 1) == '.' &&
		       (*(p + 2) == G_DIR_SEPARATOR ||
			*(p + 2) == '\0'))
		{
		  if (q > filename + 1)
		    {
		      q--;
		      while (q > filename + 1 &&
			     *(q - 1) != G_DIR_SEPARATOR)
			q--;
		    }

		  if (*(p + 2) == '\0')
		    break;

		  p += 2;
		}
	      else
		{
		  *q++ = *p;
		  last_was_slash = FALSE;
		}
	    }
	  else
	    {
	      *q++ = *p;
	      last_was_slash = FALSE;
	    }
	}

      p++;
    }

  if (q > filename + 1 && *(q - 1) == G_DIR_SEPARATOR)
    q--;

  *q = '\0';
}

static gchar *
follow_links (const gchar *path)
{
  gchar *target;
  gchar *d, *s;
  gchar *path2 = NULL;

  path2 = g_strdup (path);
  while (g_file_test (path2, G_FILE_TEST_IS_SYMLINK))
    {
      target = g_file_read_link (path2, NULL);
      
      if (target)
	{
	  if (g_path_is_absolute (target))
	    path2 = target;
	  else
	    {
	      d = g_path_get_dirname (path2);
	      s = g_build_filename (d, target, NULL);
	      g_free (d);
	      g_free (target);
	      g_free (path2);
	      path2 = s;
	    }
	}
      else
	break;
    }

  if (strcmp (path, path2) == 0)
    {
      g_free (path2);
      path2 = NULL;
    }

  return path2;
}

static void
maybe_cache_image_data (Image       *image, 
			const gchar *path)
{
  if (CAN_CACHE_IMAGE_DATA(image->flags) && !image->image_data) 
    {
      GdkPixbuf *pixbuf;
      ImageData *idata;
      gchar *path2;

      idata = g_hash_table_lookup (image_data_hash, path);

      path2 = follow_links (path);

      if (path2)
	{
	  ImageData *idata2;

	  canonicalize_filename (path2);
  
	  idata2 = g_hash_table_lookup (image_data_hash, path2);

	  if (idata && idata2 && idata != idata2)
	    g_error (_("different idatas found for symlinked '%s' and '%s'\n"),
		     path, path2);

	  if (idata && !idata2)
	    g_hash_table_insert (image_data_hash, g_strdup (path2), idata);

	  if (!idata && idata2)
	    {
	      g_hash_table_insert (image_data_hash, g_strdup (path), idata2);
	      idata = idata2;
	    }
	}
      
      if (!idata)
	{
	  idata = g_new0 (ImageData, 1);
	  g_hash_table_insert (image_data_hash, g_strdup (path), idata);
	  if (path2)
	    g_hash_table_insert (image_data_hash, g_strdup (path2), idata);  
	}

      if (!idata->has_pixdata)
	{
	  pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	  
	  if (pixbuf) 
	    {
	      gdk_pixdata_from_pixbuf (&idata->pixdata, pixbuf, FALSE);
	      idata->pixel_data_size = idata->pixdata.length + 8;
	      idata->has_pixdata = TRUE;
	    }
	}

      image->image_data = idata;

      if (path2)
	g_free (path2);
    }
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
	    {
	      image->flags |= flags;
	      maybe_cache_image_data (image, path);
	    }
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
	      maybe_cache_image_data (image, path);

	      g_hash_table_insert (dir_hash, g_strdup (basename), image);
	    }

	  if (g_str_has_suffix (name, ".icon"))
	    load_icon_data (image, path);

	  g_free (basename);
	}

      g_free (path);
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


gboolean
write_pixdata (FILE *cache, GdkPixdata *pixdata)
{
  guint8 *s;
  guint len;
  gint i;

  /* Type 0 is GdkPixdata */
  if (!write_card32 (cache, 0))
    return FALSE;

  s = gdk_pixdata_serialize (pixdata, &len);

  if (!write_card32 (cache, len))
    {
      g_free (s);
      return FALSE;
    }

  i = fwrite (s, len, 1, cache);
  
  g_free (s);

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
get_image_meta_data_size (Image *image)
{
  gint i;
  guint len = 0;

  if (image->has_embedded_rect ||
      image->attach_points > 0 ||
      image->n_display_names > 0)
    len += 12;

  if (image->has_embedded_rect)
    len += 8;

  if (image->n_attach_points > 0)
    len += 4 + image->n_attach_points * 4;

  if (image->n_display_names > 0)
    {
      len += 4 + 8 * image->n_display_names;

      for (i = 0; image->display_names[i]; i++)
	len += ALIGN_VALUE (strlen (image->display_names[i]) + 1, 4);
    }

  return len;
}

guint
get_image_pixel_data_size (Image *image)
{
  if (image->pixel_data_size == 0)
    {
      if (image->image_data && 
	  image->image_data->has_pixdata)
	{
	  image->pixel_data_size = image->image_data->pixel_data_size;
	  image->image_data->pixel_data_size = 0;
	}
    }

  return image->pixel_data_size;
}

guint
get_image_data_size (Image *image)
{
  guint len;
  
  len = 0;

  len += get_image_pixel_data_size (image);
  len += get_image_meta_data_size (image);
  
  if (len > 0 || (image->image_data && image->image_data->has_pixdata))
    len += 8;

  return len;
}

guint
get_single_node_size (HashNode *node, gboolean include_image_data)
{
  int len = 0;
  GList *list;

  /* Node pointers */
  len += 12;

  /* Name */
  len += ALIGN_VALUE (strlen (node->name) + 1, 4);

  /* Image list */
  len += 4 + g_list_length (node->image_list) * 8;
 
  /* Image data */
  if (include_image_data)
    for (list = node->image_list; list; list = list->next)
      {
	Image *image = list->data;

	len += get_image_data_size (image);
      }
  
  return len;
}

guint
get_bucket_size (HashNode *node)
{
  int len = 0;
  while (node)
    {
      len += get_single_node_size (node, TRUE);

      node = node->next;
    }

  return len;
}

gboolean
write_bucket (FILE *cache, HashNode *node, int *offset)
{
  while (node != NULL)
    {
      int next_offset = *offset + get_single_node_size (node, TRUE);
      int image_data_offset = *offset + get_single_node_size (node, FALSE);
      int data_offset;
      int tmp;
      int i, j, len;
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
      tmp = *offset + 12 + ALIGN_VALUE (strlen (node->name) + 1, 4);
      if (!write_card32 (cache, tmp))
	return FALSE;
      
      /* Icon name */
      if (!write_string (cache, node->name))
	return FALSE;
      
      /* Image list */
      len = g_list_length (node->image_list);
      if (!write_card32 (cache, len))
	return FALSE;
      
      /* Image data goes right after the image list */
      tmp += 4 + len * 8;

      list = node->image_list;
      data_offset = image_data_offset;
      for (i = 0; i < len; i++)
	{
	  Image *image = list->data;
	  int image_data_size = get_image_data_size (image);

	  /* Directory index */
	  if (!write_card16 (cache, image->dir_index))
	    return FALSE;
	  
	  /* Flags */
	  if (!write_card16 (cache, image->flags))
	    return FALSE;

	  /* Image data offset */
	  if (image_data_size > 0)
	    {
	      if (!write_card32 (cache, data_offset))
		return FALSE;
	      data_offset += image_data_size;
	    }
	  else 
	    {
	      if (!write_card32 (cache, 0))
		return FALSE;
	    }

	  list = list->next;
	}

      /* Now write the image data */
      list = node->image_list;
      for (i = 0; i < len; i++, list = list->next)
	{
	  Image *image = list->data;
	  int pixel_data_size = get_image_pixel_data_size (image);
	  int meta_data_size = get_image_meta_data_size (image);

	  if (get_image_data_size (image) == 0)
	    continue;

	  /* Pixel data */
	  if (pixel_data_size > 0) 
	    {
	      if (!write_card32 (cache, image_data_offset + 8))
		return FALSE;

	      image->image_data->offset = image_data_offset + 8;
	    }
	  else
	    {
	      gint offset;

	      if (image->image_data)
		offset = image->image_data->offset;
	      else
		offset = 0;

	      if (!write_card32 (cache, offset))
		return FALSE;
	    }

	  if (meta_data_size > 0)
	    {
	      if (!write_card32 (cache, image_data_offset + pixel_data_size + 8))
		return FALSE;
	    }
	  else
	    {
	      if (!write_card32 (cache, 0))
		return FALSE;
	    }

	  if (pixel_data_size > 0)
	    {
	      if (!write_pixdata (cache, &image->image_data->pixdata))
		return FALSE;
	    }
	  
	  if (meta_data_size > 0)
	    {
	      int ofs = image_data_offset + pixel_data_size + 20;

	      if (image->has_embedded_rect)
		{
		  if (!write_card32 (cache, ofs))
		    return FALSE;
	      
		  ofs += 8;
		}	      
	      else
		{
		  if (!write_card32 (cache, 0))
		    return FALSE;
		}
	      
	      if (image->n_attach_points > 0)
		{
		  if (!write_card32 (cache, ofs))
		    return FALSE;

		  ofs += 4 + 4 * image->n_attach_points;
		}
	      else
		{
		  if (!write_card32 (cache, 0))
		    return FALSE;
		}

	      if (image->n_display_names > 0)
		{
		  if (!write_card32 (cache, ofs))
		    return FALSE;
		}
	      else
		{
		  if (!write_card32 (cache, 0))
		    return FALSE;
		}

	      if (image->has_embedded_rect)
		{
		  if (!write_card16 (cache, image->x0) ||
		      !write_card16 (cache, image->y0) ||
		      !write_card16 (cache, image->x1) ||
		      !write_card16 (cache, image->y1))
		    return FALSE;
		}

	      if (image->n_attach_points > 0)
		{
		  if (!write_card32 (cache, image->n_attach_points))
		    return FALSE;
		  
		  for (j = 0; j < 2 * image->n_attach_points; j++)
		    {
		      if (!write_card16 (cache, image->attach_points[j]))
			return FALSE;
		    }		  
		}

	      if (image->n_display_names > 0)
		{
		  if (!write_card32 (cache, image->n_display_names))
		    return FALSE;

		  ofs += 4 + 8 * image->n_display_names;

		  for (j = 0; j < 2 * image->n_display_names; j++)
		    {
		      if (!write_card32 (cache, ofs))
			return FALSE;

		      ofs += ALIGN_VALUE (strlen (image->display_names[j]) + 1, 4);
		    }

		  for (j = 0; j < 2 * image->n_display_names; j++)
		    {
		      if (!write_string (cache, image->display_names[j]))
			return FALSE;
		    }	     
		}
	    }

	  image_data_offset += pixel_data_size + meta_data_size + 8;
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
      g_printerr (_("Failed to write header\n"));
      return FALSE;
    }

  if (!write_hash_table (cache, &context, &new_offset))
    {
      g_printerr (_("Failed to write hash table\n"));
      return FALSE;
    }

  if (!write_dir_index (cache, new_offset, directories))
    {
      g_printerr (_("Failed to write directory index\n"));
      return FALSE;
    }
  
  rewind (cache);

  if (!write_header (cache, new_offset))
    {
      g_printerr (_("Failed to rewrite header\n"));
      return FALSE;
    }
    
  return TRUE;
}

void
build_cache (const gchar *path)
{
  gchar *cache_path, *tmp_cache_path;
#ifdef G_OS_WIN32
  gchar *bak_cache_path = NULL;
#endif
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
      g_printerr (_("Failed to write cache file: %s\n"), g_strerror (errno));
      exit (1);
    }

  files = g_hash_table_new (g_str_hash, g_str_equal);
  image_data_hash = g_hash_table_new (g_str_hash, g_str_equal);
  
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

#ifdef G_OS_WIN32
  if (g_file_test (cache_path, G_FILE_TEST_EXISTS))
    {
      bak_cache_path = g_strconcat (cache_path, ".bak", NULL);
      g_unlink (bak_cache_path);
      if (g_rename (cache_path, bak_cache_path) == -1)
	{
	  g_printerr (_("Could not rename %s to %s: %s, removing %s then.\n"),
		      cache_path, bak_cache_path,
		      g_strerror (errno),
		      cache_path);
	  g_unlink (cache_path);
	  bak_cache_path = NULL;
	}
    }
#endif

  if (g_rename (tmp_cache_path, cache_path) == -1)
    {
      g_printerr (_("Could not rename %s to %s: %s\n"),
		  tmp_cache_path, cache_path,
		  g_strerror (errno));
      g_unlink (tmp_cache_path);
#ifdef G_OS_WIN32
      if (bak_cache_path != NULL)
	if (g_rename (bak_cache_path, cache_path) == -1)
	  g_printerr (_("Could not rename %s back to %s: %s.\n"),
		      bak_cache_path, cache_path,
		      g_strerror (errno));
#endif
      exit (1);
    }
#ifdef G_OS_WIN32
  if (bak_cache_path != NULL)
    g_unlink (bak_cache_path);
#endif

  /* Update time */
  /* FIXME: What do do if an error occurs here? */
  if (g_stat (path, &path_stat) < 0 ||
      g_stat (cache_path, &cache_stat))
    exit (1);

  utime_buf.actime = path_stat.st_atime;
  utime_buf.modtime = cache_stat.st_mtime;
  utime (path, &utime_buf);
  
  if (!quiet)
    g_printerr (_("Cache file created successfully.\n"));
}

void
write_csource (const gchar *path)
{
  gchar *cache_path;
  gchar *data;
  gsize len;
  gint i;

  cache_path = g_build_filename (path, CACHE_NAME, NULL);
  if (!g_file_get_contents (cache_path, &data, &len, NULL))
    exit (1);
  
  g_printf ("#ifdef __SUNPRO_C\n");
  g_printf ("#pragma align 4 (%s)\n", var_name);   
  g_printf ("#endif\n");
  
  g_printf ("#ifdef __GNUC__\n");
  g_printf ("static const guint8 %s[] __attribute__ ((__aligned__ (4))) = \n", var_name);
  g_printf ("#else\n");
  g_printf ("static const guint8 %s[] = \n", var_name);
  g_printf ("#endif\n");

  g_printf ("{\n");
  for (i = 0; i < len - 1; i++)
    {
      if (i %12 == 0)
	g_printf ("  ");
      g_printf ("0x%02x, ", (guint8)data[i]);
      if (i % 12 == 11)
        g_printf ("\n");
    }
  
  g_printf ("0x%02x\n};\n", (guint8)data[i]);
}

static GOptionEntry args[] = {
  { "force", 'f', 0, G_OPTION_ARG_NONE, &force_update, N_("Overwrite an existing cache, even if uptodate"), NULL },
  { "ignore-theme-index", 't', 0, G_OPTION_ARG_NONE, &ignore_theme_index, N_("Don't check for the existence of index.theme"), NULL },
  { "index-only", 'i', 0, G_OPTION_ARG_NONE, &index_only, N_("Don't include image data in the cache"), NULL },
  { "source", 'c', 0, G_OPTION_ARG_STRING, &var_name, N_("Output a C header file"), "NAME" },
  { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, N_("Turn off verbose output"), NULL },
  { NULL }
};

int
main (int argc, char **argv)
{
  gchar *path;
  GOptionContext *context;

  if (argc < 2)
    return 0;
  
  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  context = g_option_context_new ("ICONPATH");
  g_option_context_add_main_entries (context, args, GETTEXT_PACKAGE);

  g_option_context_parse (context, &argc, &argv, NULL);
  
  path = argv[1];
#ifdef G_OS_WIN32
  path = g_locale_to_utf8 (path, -1, NULL, NULL, NULL);
#endif
  
  if (!ignore_theme_index && !has_theme_index (path))
    {
      g_printerr (_("No theme index file in '%s'.\n"
		    "If you really want to create an icon cache here, use --ignore-theme-index.\n"), path);
      return 1;
    }
  
  if (!force_update && is_cache_up_to_date (path))
    return 0;

  g_type_init ();
  build_cache (path);

  if (strcmp (var_name, "-") != 0)
    write_csource (path);

  return 0;
}
