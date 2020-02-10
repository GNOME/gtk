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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkdebug.h"
#include "gtkiconcacheprivate.h"
#include "gtkiconcachevalidatorprivate.h"

#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>


#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#define GET_UINT16(cache, offset) (GUINT16_FROM_BE (*(guint16 *)((cache) + (offset))))
#define GET_UINT32(cache, offset) (GUINT32_FROM_BE (*(guint32 *)((cache) + (offset))))

struct _GtkIconCache {
  gint ref_count;

  GMappedFile *map;
  gchar *buffer;

  guint32 last_chain_offset;
};

GtkIconCache *
gtk_icon_cache_ref (GtkIconCache *cache)
{
  cache->ref_count++;
  return cache;
}

void
gtk_icon_cache_unref (GtkIconCache *cache)
{
  cache->ref_count --;

  if (cache->ref_count == 0)
    {
      GTK_NOTE (ICONTHEME, g_message ("unmapping icon cache"));

      if (cache->map)
        g_mapped_file_unref (cache->map);
      g_free (cache);
    }
}

GtkIconCache *
gtk_icon_cache_new_for_path (const gchar *path)
{
  GtkIconCache *cache = NULL;
  GMappedFile *map;

  gchar *cache_filename;
  gint fd = -1;
  GStatBuf st;
  GStatBuf path_st;

   /* Check if we have a cache file */
  cache_filename = g_build_filename (path, "icon-theme.cache", NULL);

  GTK_NOTE (ICONTHEME, g_message ("look for icon cache in %s", path));

  if (g_stat (path, &path_st) < 0)
    goto done;

  /* Open the file and map it into memory */
  fd = g_open (cache_filename, O_RDONLY|_O_BINARY, 0);

  if (fd < 0)
    goto done;

#ifdef G_OS_WIN32

/* Bug 660730: _fstat32 is only defined in msvcrt80.dll+/VS 2005+ */
/*             or possibly in the msvcrt.dll linked to by the Windows DDK */
/*             (will need to check on the Windows DDK part later) */
#if ((defined (_MSC_VER) && (_MSC_VER >= 1400 || __MSVCRT_VERSION__ >= 0x0800)) || defined (__MINGW64_VERSION_MAJOR)) && !defined(_WIN64)
#undef fstat /* Just in case */
#define fstat _fstat32
#endif
#endif

  if (fstat (fd, &st) < 0 || st.st_size < 4)
    goto done;

  /* Verify cache is uptodate */
  if (st.st_mtime < path_st.st_mtime)
    {
      GTK_NOTE (ICONTHEME, g_message ("icon cache outdated"));
      goto done;
    }

  map = g_mapped_file_new (cache_filename, FALSE, NULL);

  if (!map)
    goto done;

#ifdef G_ENABLE_DEBUG
  if (GTK_DEBUG_CHECK (ICONTHEME))
    {
      CacheInfo info;

      info.cache = g_mapped_file_get_contents (map);
      info.cache_size = g_mapped_file_get_length (map);
      info.n_directories = 0;
      info.flags = CHECK_OFFSETS|CHECK_STRINGS;

      if (!gtk_icon_cache_validate (&info))
        {
          g_mapped_file_unref (map);
          g_warning ("Icon cache '%s' is invalid", cache_filename);

          goto done;
        }
    }
#endif

  GTK_NOTE (ICONTHEME, g_message ("found icon cache for %s", path));

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->map = map;
  cache->buffer = g_mapped_file_get_contents (map);

 done:
  g_free (cache_filename);
  if (fd >= 0)
    close (fd);

  return cache;
}

GtkIconCache *
gtk_icon_cache_new (const gchar *data)
{
  GtkIconCache *cache;

  cache = g_new0 (GtkIconCache, 1);
  cache->ref_count = 1;
  cache->map = NULL;
  cache->buffer = (gchar *)data;

  return cache;
}

static gint
get_directory_index (GtkIconCache *cache,
                     const gchar *directory)
{
  guint32 dir_list_offset;
  gint n_dirs;
  gint i;

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

GHashTable *
gtk_icon_cache_list_icons_in_directory (GtkIconCache *cache,
                                        const gchar  *directory,
                                        GtkStringSet *set)
{
  gint directory_index;
  guint32 hash_offset, n_buckets;
  guint32 chain_offset;
  guint32 image_list_offset, n_images;
  int i, j;
  GHashTable *icons = NULL;

  directory_index = get_directory_index (cache, directory);

  if (directory_index == -1)
    return NULL;

  hash_offset = GET_UINT32 (cache->buffer, 4);
  n_buckets = GET_UINT32 (cache->buffer, hash_offset);

  for (i = 0; i < n_buckets; i++)
    {
      chain_offset = GET_UINT32 (cache->buffer, hash_offset + 4 + 4 * i);
      while (chain_offset != 0xffffffff)
        {
          guint32 flags = 0;

          image_list_offset = GET_UINT32 (cache->buffer, chain_offset + 8);
          n_images = GET_UINT32 (cache->buffer, image_list_offset);

          for (j = 0; j < n_images; j++)
            {
              if (GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j) ==
                  directory_index)
                {
                  flags = GET_UINT16 (cache->buffer, image_list_offset + 4 + 8 * j + 2);
                  break;
                }
            }

          if (flags != 0)
            {
              guint32 name_offset = GET_UINT32 (cache->buffer, chain_offset + 4);
              const char *name = cache->buffer + name_offset;
              const char *interned_name;
              guint32 hash_flags = 0;

              /* Icons named foo.symbolic.png are stored in the cache as "foo.symbolic" with ICON_CACHE_FLAG_PNG,
               * but we convert it internally to ICON_CACHE_FLAG_SYMBOLIC_PNG.
               * Otherwise we use the same enum values and names as on disk. */
              if (g_str_has_suffix (name, ".symbolic") && (flags & ICON_CACHE_FLAG_PNG_SUFFIX) != 0)
                {
                  char *converted_name = g_strndup (name, strlen (name) - 9);
                  interned_name = gtk_string_set_add (set, converted_name);
                  g_free (converted_name);
                  flags |= ICON_CACHE_FLAG_SYMBOLIC_PNG_SUFFIX;
                  flags &= ~ICON_CACHE_FLAG_PNG_SUFFIX;
                }
              else
                interned_name = gtk_string_set_add (set, name);

              if (!icons)
                icons = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

              hash_flags = GPOINTER_TO_INT (g_hash_table_lookup (icons, interned_name));
              g_hash_table_replace (icons, (char *)interned_name, GUINT_TO_POINTER (hash_flags|flags));
            }

          chain_offset = GET_UINT32 (cache->buffer, chain_offset);
        }
    }

  return icons;
}
