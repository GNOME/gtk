/* GTK - The GIMP Toolkit
 * gtkfilesystemunix.c: Default implementation of GtkFileSystem for UNIX-like systems
 * Copyright (C) 2003, Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "gtkfilesystem.h"
#include "gtkfilesystemunix.h"
#include "gtkicontheme.h"
#include "gtkintl.h"

#define XDG_PREFIX _gtk_xdg
#include "xdgmime/xdgmime.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#define BOOKMARKS_FILENAME ".gtk-bookmarks"
#define BOOKMARKS_TMP_FILENAME ".gtk-bookmarks-XXXXXX"

#define FOLDER_CACHE_LIFETIME 2 /* seconds */

typedef struct _GtkFileSystemUnixClass GtkFileSystemUnixClass;

#define GTK_FILE_SYSTEM_UNIX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_SYSTEM_UNIX, GtkFileSystemUnixClass))
#define GTK_IS_FILE_SYSTEM_UNIX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_SYSTEM_UNIX))
#define GTK_FILE_SYSTEM_UNIX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_SYSTEM_UNIX, GtkFileSystemUnixClass))

struct _GtkFileSystemUnixClass
{
  GObjectClass parent_class;
};

struct _GtkFileSystemUnix
{
  GObject parent_instance;

  GHashTable *folder_hash;
};

/* Icon type, supplemented by MIME type
 */
typedef enum {
  ICON_UNDECIDED,
  ICON_NONE,
  ICON_REGULAR,	/* Use mime type for icon */
  ICON_BLOCK_DEVICE,
  ICON_BROKEN_SYMBOLIC_LINK,
  ICON_CHARACTER_DEVICE,
  ICON_DIRECTORY,
  ICON_EXECUTABLE,
  ICON_FIFO,
  ICON_SOCKET
} IconType;


#define GTK_TYPE_FILE_FOLDER_UNIX             (gtk_file_folder_unix_get_type ())
#define GTK_FILE_FOLDER_UNIX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_FOLDER_UNIX, GtkFileFolderUnix))
#define GTK_IS_FILE_FOLDER_UNIX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_FOLDER_UNIX))
#define GTK_FILE_FOLDER_UNIX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_FOLDER_UNIX, GtkFileFolderUnixClass))
#define GTK_IS_FILE_FOLDER_UNIX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_FOLDER_UNIX))
#define GTK_FILE_FOLDER_UNIX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_FOLDER_UNIX, GtkFileFolderUnixClass))

typedef struct _GtkFileFolderUnix      GtkFileFolderUnix;
typedef struct _GtkFileFolderUnixClass GtkFileFolderUnixClass;

struct _GtkFileFolderUnixClass
{
  GObjectClass parent_class;
};

struct _GtkFileFolderUnix
{
  GObject parent_instance;

  GtkFileSystemUnix *system_unix;
  GtkFileInfoType types;
  gchar *filename;
  GHashTable *stat_info;
  unsigned int have_stat : 1;
  unsigned int have_mime_type : 1;
  time_t asof;
};

struct stat_info_entry {
  struct stat statbuf;
  char *mime_type;
  IconType icon_type;
};

static const GtkFileInfoType STAT_NEEDED_MASK = (GTK_FILE_INFO_IS_FOLDER |
						 GTK_FILE_INFO_IS_HIDDEN |
						 GTK_FILE_INFO_MODIFICATION_TIME |
						 GTK_FILE_INFO_SIZE);

static GObjectClass *system_parent_class;
static GObjectClass *folder_parent_class;

static void gtk_file_system_unix_class_init   (GtkFileSystemUnixClass *class);
static void gtk_file_system_unix_iface_init   (GtkFileSystemIface     *iface);
static void gtk_file_system_unix_init         (GtkFileSystemUnix      *impl);
static void gtk_file_system_unix_finalize     (GObject                *object);

static GSList *             gtk_file_system_unix_list_volumes        (GtkFileSystem     *file_system);
static GtkFileSystemVolume *gtk_file_system_unix_get_volume_for_path (GtkFileSystem     *file_system,
								      const GtkFilePath *path);

static GtkFileFolder *gtk_file_system_unix_get_folder    (GtkFileSystem      *file_system,
							  const GtkFilePath  *path,
							  GtkFileInfoType     types,
							  GError            **error);
static gboolean       gtk_file_system_unix_create_folder (GtkFileSystem      *file_system,
							  const GtkFilePath  *path,
							  GError            **error);

static void         gtk_file_system_unix_volume_free             (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static GtkFilePath *gtk_file_system_unix_volume_get_base_path    (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static gboolean     gtk_file_system_unix_volume_get_is_mounted   (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static gboolean     gtk_file_system_unix_volume_mount            (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume,
								  GError             **error);
static gchar *      gtk_file_system_unix_volume_get_display_name (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static GdkPixbuf *  gtk_file_system_unix_volume_render_icon      (GtkFileSystem        *file_system,
								  GtkFileSystemVolume  *volume,
								  GtkWidget            *widget,
								  gint                  pixel_size,
								  GError              **error);

static gboolean       gtk_file_system_unix_get_parent    (GtkFileSystem      *file_system,
							  const GtkFilePath  *path,
							  GtkFilePath       **parent,
							  GError            **error);
static GtkFilePath *  gtk_file_system_unix_make_path     (GtkFileSystem      *file_system,
							  const GtkFilePath  *base_path,
							  const gchar        *display_name,
							  GError            **error);
static gboolean       gtk_file_system_unix_parse         (GtkFileSystem      *file_system,
							  const GtkFilePath  *base_path,
							  const gchar        *str,
							  GtkFilePath       **folder,
							  gchar             **file_part,
							  GError            **error);

static gchar *      gtk_file_system_unix_path_to_uri      (GtkFileSystem     *file_system,
							   const GtkFilePath *path);
static gchar *      gtk_file_system_unix_path_to_filename (GtkFileSystem     *file_system,
							   const GtkFilePath *path);
static GtkFilePath *gtk_file_system_unix_uri_to_path      (GtkFileSystem     *file_system,
							   const gchar       *uri);
static GtkFilePath *gtk_file_system_unix_filename_to_path (GtkFileSystem     *file_system,
							   const gchar       *filename);

static GdkPixbuf *gtk_file_system_unix_render_icon (GtkFileSystem     *file_system,
						    const GtkFilePath *path,
						    GtkWidget         *widget,
						    gint               pixel_size,
						    GError           **error);

static gboolean gtk_file_system_unix_insert_bookmark (GtkFileSystem     *file_system,
						      const GtkFilePath *path,
						      gint               position,
						      GError           **error);
static gboolean gtk_file_system_unix_remove_bookmark (GtkFileSystem     *file_system,
						      const GtkFilePath *path,
						      GError           **error);
static GSList * gtk_file_system_unix_list_bookmarks  (GtkFileSystem *file_system);

static GType gtk_file_folder_unix_get_type   (void);
static void  gtk_file_folder_unix_class_init (GtkFileFolderUnixClass *class);
static void  gtk_file_folder_unix_iface_init (GtkFileFolderIface     *iface);
static void  gtk_file_folder_unix_init       (GtkFileFolderUnix      *impl);
static void  gtk_file_folder_unix_finalize   (GObject                *object);

static GtkFileInfo *gtk_file_folder_unix_get_info      (GtkFileFolder  *folder,
							const GtkFilePath    *path,
							GError        **error);
static gboolean     gtk_file_folder_unix_list_children (GtkFileFolder  *folder,
							GSList        **children,
							GError        **error);

static GtkFilePath *filename_to_path   (const gchar       *filename);

static gboolean     filename_is_root  (const char       *filename);

static gboolean fill_in_names (GtkFileFolderUnix *folder_unix, GError **error);
static gboolean fill_in_stats (GtkFileFolderUnix *folder_unix, GError **error);
static gboolean fill_in_mime_type (GtkFileFolderUnix *folder_unix, GError **error);

static char *       get_parent_dir    (const char       *filename);

/*
 * GtkFileSystemUnix
 */
GType
gtk_file_system_unix_get_type (void)
{
  static GType file_system_unix_type = 0;

  if (!file_system_unix_type)
    {
      static const GTypeInfo file_system_unix_info =
      {
	sizeof (GtkFileSystemUnixClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_system_unix_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileSystemUnix),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_system_unix_init,
      };

      static const GInterfaceInfo file_system_info =
      {
	(GInterfaceInitFunc) gtk_file_system_unix_iface_init, /* interface_init */
	NULL,			                              /* interface_finalize */
	NULL			                              /* interface_data */
      };

      file_system_unix_type = g_type_register_static (G_TYPE_OBJECT,
						      "GtkFileSystemUnix",
						      &file_system_unix_info, 0);
      g_type_add_interface_static (file_system_unix_type,
				   GTK_TYPE_FILE_SYSTEM,
				   &file_system_info);
    }

  return file_system_unix_type;
}

/**
 * gtk_file_system_unix_new:
 *
 * Creates a new #GtkFileSystemUnix object. #GtkFileSystemUnix
 * implements the #GtkFileSystem interface with direct access to
 * the filesystem using Unix/Linux API calls
 *
 * Return value: the new #GtkFileSystemUnix object
 **/
GtkFileSystem *
gtk_file_system_unix_new (void)
{
  return g_object_new (GTK_TYPE_FILE_SYSTEM_UNIX, NULL);
}

static void
gtk_file_system_unix_class_init (GtkFileSystemUnixClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  system_parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_system_unix_finalize;
}

static void
gtk_file_system_unix_iface_init   (GtkFileSystemIface *iface)
{
  iface->list_volumes = gtk_file_system_unix_list_volumes;
  iface->get_volume_for_path = gtk_file_system_unix_get_volume_for_path;
  iface->get_folder = gtk_file_system_unix_get_folder;
  iface->create_folder = gtk_file_system_unix_create_folder;
  iface->volume_free = gtk_file_system_unix_volume_free;
  iface->volume_get_base_path = gtk_file_system_unix_volume_get_base_path;
  iface->volume_get_is_mounted = gtk_file_system_unix_volume_get_is_mounted;
  iface->volume_mount = gtk_file_system_unix_volume_mount;
  iface->volume_get_display_name = gtk_file_system_unix_volume_get_display_name;
  iface->volume_render_icon = gtk_file_system_unix_volume_render_icon;
  iface->get_parent = gtk_file_system_unix_get_parent;
  iface->make_path = gtk_file_system_unix_make_path;
  iface->parse = gtk_file_system_unix_parse;
  iface->path_to_uri = gtk_file_system_unix_path_to_uri;
  iface->path_to_filename = gtk_file_system_unix_path_to_filename;
  iface->uri_to_path = gtk_file_system_unix_uri_to_path;
  iface->filename_to_path = gtk_file_system_unix_filename_to_path;
  iface->render_icon = gtk_file_system_unix_render_icon;
  iface->insert_bookmark = gtk_file_system_unix_insert_bookmark;
  iface->remove_bookmark = gtk_file_system_unix_remove_bookmark;
  iface->list_bookmarks = gtk_file_system_unix_list_bookmarks;
}

static void
gtk_file_system_unix_init (GtkFileSystemUnix *system_unix)
{
  system_unix->folder_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gtk_file_system_unix_finalize (GObject *object)
{
  GtkFileSystemUnix *system_unix;

  system_unix = GTK_FILE_SYSTEM_UNIX (object);

  /* FIXME: assert that the hash is empty? */
  g_hash_table_destroy (system_unix->folder_hash);

  system_parent_class->finalize (object);
}

/* Returns our single root volume */
static GtkFileSystemVolume *
get_root_volume (void)
{
  return (GtkFileSystemVolume *) gtk_file_path_new_dup ("/");
}

static GSList *
gtk_file_system_unix_list_volumes (GtkFileSystem *file_system)
{
  return g_slist_append (NULL, get_root_volume ());
}

static GtkFileSystemVolume *
gtk_file_system_unix_get_volume_for_path (GtkFileSystem     *file_system,
					  const GtkFilePath *path)
{
  return get_root_volume ();
}

static char *
remove_trailing_slash (const char *filename)
{
  int len;

  len = strlen (filename);

  if (len > 1 && filename[len - 1] == '/')
    return g_strndup (filename, len - 1);
  else
    return g_memdup (filename, len + 1);
}

static GtkFileFolder *
gtk_file_system_unix_get_folder (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GtkFileInfoType    types,
				 GError           **error)
{
  GtkFileSystemUnix *system_unix;
  GtkFileFolderUnix *folder_unix;
  const char *filename;
  char *filename_copy;
  time_t now = time (NULL);

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  filename_copy = remove_trailing_slash (filename);
  folder_unix = g_hash_table_lookup (system_unix->folder_hash, filename_copy);

  if (folder_unix)
    {
      g_free (filename_copy);
      if (now - folder_unix->asof >= FOLDER_CACHE_LIFETIME &&
	  folder_unix->stat_info)
	{
#if 0
	  g_print ("Cleaning out cached directory %s\n", filename);
#endif
	  g_hash_table_destroy (folder_unix->stat_info);
	  folder_unix->stat_info = NULL;
	  folder_unix->have_mime_type = FALSE;
	  folder_unix->have_stat = FALSE;
	}

      g_object_ref (folder_unix);
      folder_unix->types |= types;
      types = folder_unix->types;
    }
  else
    {
      if (!g_file_test (filename, G_FILE_TEST_IS_DIR))
	{
	  int save_errno = errno;
	  gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

	  /* If g_file_test() returned FALSE but not due to an error, it means
	   * that the filename is not a directory.
	   */
	  if (save_errno == 0)
	    /* ENOTDIR */
	    g_set_error (error,
			 GTK_FILE_SYSTEM_ERROR,
			 GTK_FILE_SYSTEM_ERROR_NOT_FOLDER,
			 _("%s: %s"),
			 filename_utf8 ? filename_utf8 : "???",
			 g_strerror (ENOTDIR));
	  else
	    g_set_error (error,
			 GTK_FILE_SYSTEM_ERROR,
			 GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
			 _("error getting information for '%s': %s"),
			 filename_utf8 ? filename_utf8 : "???",
			 g_strerror (save_errno));

	  g_free (filename_utf8);
	  g_free (filename_copy);
	  return NULL;
	}

      folder_unix = g_object_new (GTK_TYPE_FILE_FOLDER_UNIX, NULL);
      folder_unix->system_unix = system_unix;
      folder_unix->filename = filename_copy;
      folder_unix->types = types;
      folder_unix->stat_info = NULL;
      folder_unix->asof = now;
      folder_unix->have_mime_type = FALSE;
      folder_unix->have_stat = FALSE;

      g_hash_table_insert (system_unix->folder_hash,
			   folder_unix->filename,
			   folder_unix);
    }

  if ((types & STAT_NEEDED_MASK) && !fill_in_stats (folder_unix, error))
    {
      g_object_unref (folder_unix);
      return NULL;
    }
  if ((types & GTK_FILE_INFO_MIME_TYPE) && !fill_in_mime_type (folder_unix, error))
    {
      g_object_unref (folder_unix);
      return NULL;
    }

  return GTK_FILE_FOLDER (folder_unix);
}

static gboolean
gtk_file_system_unix_create_folder (GtkFileSystem     *file_system,
				    const GtkFilePath *path,
				    GError           **error)
{
  GtkFileSystemUnix *system_unix;
  const char *filename;
  gboolean result;
  char *parent, *tmp;

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  tmp = remove_trailing_slash (filename);
  result = mkdir (tmp, 0777) == 0;
  g_free (tmp);

  if (!result)
    {
      int save_errno = errno;
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error creating directory '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (save_errno));
      g_free (filename_utf8);
      return FALSE;
    }

  if (filename_is_root (filename))
    return TRUE; /* hmmm, but with no notification */

  parent = get_parent_dir (filename);
  if (parent)
    {
      GtkFileFolderUnix *folder_unix;

      folder_unix = g_hash_table_lookup (system_unix->folder_hash, parent);
      if (folder_unix)
	{
	  GSList *paths;

	  paths = g_slist_append (NULL, (GtkFilePath *) path);
	  g_signal_emit_by_name (folder_unix, "files-added", paths);
	  g_slist_free (paths);
	}
    }

  return TRUE;
}

static void
gtk_file_system_unix_volume_free (GtkFileSystem        *file_system,
				  GtkFileSystemVolume  *volume)
{
  GtkFilePath *path;

  path = (GtkFilePath *) volume;
  gtk_file_path_free (path);
}

static GtkFilePath *
gtk_file_system_unix_volume_get_base_path (GtkFileSystem        *file_system,
					   GtkFileSystemVolume  *volume)
{
  return gtk_file_path_new_dup ("/");
}

static gboolean
gtk_file_system_unix_volume_get_is_mounted (GtkFileSystem        *file_system,
					    GtkFileSystemVolume  *volume)
{
  return TRUE;
}

static gboolean
gtk_file_system_unix_volume_mount (GtkFileSystem        *file_system,
				   GtkFileSystemVolume  *volume,
				   GError              **error)
{
  g_set_error (error,
	       GTK_FILE_SYSTEM_ERROR,
	       GTK_FILE_SYSTEM_ERROR_FAILED,
	       _("This file system does not support mounting"));
  return FALSE;
}

static gchar *
gtk_file_system_unix_volume_get_display_name (GtkFileSystem       *file_system,
					      GtkFileSystemVolume *volume)
{
  return g_strdup (_("Filesystem")); /* Same as Nautilus */
}

static IconType
get_icon_type_from_stat (struct stat *statp)
{
  if (S_ISBLK (statp->st_mode))
    return ICON_BLOCK_DEVICE;
  else if (S_ISLNK (statp->st_mode))
    return ICON_BROKEN_SYMBOLIC_LINK; /* See get_icon_type */
  else if (S_ISCHR (statp->st_mode))
    return ICON_CHARACTER_DEVICE;
  else if (S_ISDIR (statp->st_mode))
    return ICON_DIRECTORY;
  else if (S_ISFIFO (statp->st_mode))
    return  ICON_FIFO;
  else if (S_ISSOCK (statp->st_mode))
    return ICON_SOCKET;
  else
    return ICON_REGULAR;
}

static IconType
get_icon_type (const char *filename,
	       GError    **error)
{
  struct stat statbuf;

  /* If stat fails, try to fall back to lstat to catch broken links
   */
  if (stat (filename, &statbuf) != 0)
    {
      if (errno != ENOENT || lstat (filename, &statbuf) != 0)
	{
	  int save_errno = errno;
	  gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		       _("error getting information for '%s': %s"),
		       filename_utf8 ? filename_utf8 : "???",
		       g_strerror (save_errno));
	  g_free (filename_utf8);

	  return ICON_NONE;
	}
    }

  return get_icon_type_from_stat (&statbuf);
}

typedef struct
{
  gint size;
  GdkPixbuf *pixbuf;
} IconCacheElement;

static void
icon_cache_element_free (IconCacheElement *element)
{
  if (element->pixbuf)
    g_object_unref (element->pixbuf);
  g_free (element);
}

static void
icon_theme_changed (GtkIconTheme *icon_theme)
{
  GHashTable *cache;

  /* Difference from the initial creation is that we don't
   * reconnect the signal
   */
  cache = g_hash_table_new_full (g_str_hash, g_str_equal,
				 (GDestroyNotify)g_free,
				 (GDestroyNotify)icon_cache_element_free);
  g_object_set_data_full (G_OBJECT (icon_theme), "gtk-file-icon-cache",
			  cache, (GDestroyNotify)g_hash_table_destroy);
}

static GdkPixbuf *
get_cached_icon (GtkWidget   *widget,
		 const gchar *name,
		 gint         pixel_size)
{
  GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  GHashTable *cache = g_object_get_data (G_OBJECT (icon_theme), "gtk-file-icon-cache");
  IconCacheElement *element;

  if (!cache)
    {
      cache = g_hash_table_new_full (g_str_hash, g_str_equal,
				     (GDestroyNotify)g_free,
				     (GDestroyNotify)icon_cache_element_free);

      g_object_set_data_full (G_OBJECT (icon_theme), "gtk-file-icon-cache",
			      cache, (GDestroyNotify)g_hash_table_destroy);
      g_signal_connect (icon_theme, "changed",
			G_CALLBACK (icon_theme_changed), NULL);
    }

  element = g_hash_table_lookup (cache, name);
  if (!element)
    {
      element = g_new0 (IconCacheElement, 1);
      g_hash_table_insert (cache, g_strdup (name), element);
    }

  if (element->size != pixel_size)
    {
      if (element->pixbuf)
	g_object_unref (element->pixbuf);
      element->size = pixel_size;
      element->pixbuf = gtk_icon_theme_load_icon (icon_theme, name,
						  pixel_size, 0, NULL);
    }

  return element->pixbuf ? g_object_ref (element->pixbuf) : NULL;
}

static GdkPixbuf *
gtk_file_system_unix_volume_render_icon (GtkFileSystem        *file_system,
					 GtkFileSystemVolume  *volume,
					 GtkWidget            *widget,
					 gint                  pixel_size,
					 GError              **error)
{
  /* FIXME: set the GError if we can't load the icon */
  return get_cached_icon (widget, "gnome-fs-blockdev", pixel_size);
}

static char *
get_parent_dir (const char *filename)
{
  int len;

  len = strlen (filename);

  /* Ignore trailing slashes */
  if (len > 1 && filename[len - 1] == '/')
    {
      char *tmp, *parent;

      tmp = g_strndup (filename, len - 1);

      parent = g_path_get_dirname (tmp);
      g_free (tmp);

      return parent;
    }
  else
    return g_path_get_dirname (filename);
}

static gboolean
gtk_file_system_unix_get_parent (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GtkFilePath      **parent,
				 GError           **error)
{
  const char *filename;

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  if (filename_is_root (filename))
    {
      *parent = NULL;
    }
  else
    {
      gchar *parent_filename = get_parent_dir (filename);
      *parent = filename_to_path (parent_filename);
      g_free (parent_filename);
    }

  return TRUE;
}

static GtkFilePath *
gtk_file_system_unix_make_path (GtkFileSystem    *file_system,
			       const GtkFilePath *base_path,
			       const gchar       *display_name,
			       GError           **error)
{
  const char *base_filename;
  gchar *filename;
  gchar *full_filename;
  GError *tmp_error = NULL;
  GtkFilePath *result;

  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (base_filename), NULL);

  filename = g_filename_from_utf8 (display_name, -1, NULL, NULL, &tmp_error);
  if (!filename)
    {
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
		   "%s",
		   tmp_error->message);

      g_error_free (tmp_error);

      return NULL;
    }

  full_filename = g_build_filename (base_filename, filename, NULL);
  result = filename_to_path (full_filename);
  g_free (filename);
  g_free (full_filename);

  return result;
}

/* If this was a publically exported function, it should return
 * a dup'ed result, but we make it modify-in-place for efficiency
 * here, and because it works for us.
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

static gboolean
gtk_file_system_unix_parse (GtkFileSystem     *file_system,
			    const GtkFilePath *base_path,
			    const gchar       *str,
			    GtkFilePath      **folder,
			    gchar            **file_part,
			    GError           **error)
{
  const char *base_filename;
  gchar *last_slash;
  gboolean result = FALSE;

  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (base_filename), FALSE);

  last_slash = strrchr (str, G_DIR_SEPARATOR);
  if (!last_slash)
    {
      *folder = gtk_file_path_copy (base_path);
      *file_part = g_strdup (str);
      result = TRUE;
    }
  else
    {
      gchar *folder_part;
      gchar *folder_path;
      GError *tmp_error = NULL;

      if (last_slash == str)
	folder_part = g_strdup ("/");
      else
	folder_part = g_filename_from_utf8 (str, last_slash - str,
					    NULL, NULL, &tmp_error);

      if (!folder_part)
	{
	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
		       "%s",
		       tmp_error->message);
	  g_error_free (tmp_error);
	}
      else
	{
	  if (folder_part[0] == G_DIR_SEPARATOR)
	    folder_path = folder_part;
	  else
	    {
	      folder_path = g_build_filename (base_filename, folder_part, NULL);
	      g_free (folder_part);
	    }

	  canonicalize_filename (folder_path);

	  *folder = filename_to_path (folder_path);
	  *file_part = g_strdup (last_slash + 1);

	  g_free (folder_path);

	  result = TRUE;
	}
    }

  return result;
}

static gchar *
gtk_file_system_unix_path_to_uri (GtkFileSystem     *file_system,
				  const GtkFilePath *path)
{
  return g_filename_to_uri (gtk_file_path_get_string (path), NULL, NULL);
}

static gchar *
gtk_file_system_unix_path_to_filename (GtkFileSystem     *file_system,
				       const GtkFilePath *path)
{
  return g_strdup (gtk_file_path_get_string (path));
}

static GtkFilePath *
gtk_file_system_unix_uri_to_path (GtkFileSystem     *file_system,
				  const gchar       *uri)
{
  gchar *filename = g_filename_from_uri (uri, NULL, NULL);
  if (filename)
    return gtk_file_path_new_steal (filename);
  else
    return NULL;
}

static GtkFilePath *
gtk_file_system_unix_filename_to_path (GtkFileSystem *file_system,
				       const gchar   *filename)
{
  return gtk_file_path_new_dup (filename);
}

static const char *
get_icon_for_directory (const char *path)
{
  static char *desktop_path = NULL;

  if (!g_get_home_dir ())
    return "gnome-fs-directory";

  if (!desktop_path)
      desktop_path = g_build_filename (g_get_home_dir (), "Desktop", NULL);

  if (strcmp (g_get_home_dir (), path) == 0)
    return "gnome-fs-home";
  else if (strcmp (desktop_path, path) == 0)
    return "gnome-fs-desktop";
  else
    return "gnome-fs-directory";
}

static GdkPixbuf *
gtk_file_system_unix_render_icon (GtkFileSystem     *file_system,
				  const GtkFilePath *path,
				  GtkWidget         *widget,
				  gint               pixel_size,
				  GError           **error)
{
  const char *filename;
  IconType icon_type;
  const char *mime_type = NULL;
  char *dirname;
  GtkFileSystemUnix *system_unix;
  GtkFileFolderUnix *folder_unix;

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);
  filename = gtk_file_path_get_string (path);
  dirname = g_path_get_dirname (filename);
  folder_unix = g_hash_table_lookup (system_unix->folder_hash, dirname);
  g_free (dirname);

  if (folder_unix)
    {
      char *basename;
      struct stat_info_entry *entry;

      if (!fill_in_stats (folder_unix, error))
	return NULL;

      basename = g_path_get_basename (filename);
      entry = g_hash_table_lookup (folder_unix->stat_info, basename);
      g_free (basename);
      if (entry)
	{
	  if (entry->icon_type == ICON_UNDECIDED)
	    entry->icon_type = get_icon_type_from_stat (&entry->statbuf);
	  icon_type = entry->icon_type;
	  if (icon_type == ICON_REGULAR)
	    {
	      (void)fill_in_mime_type (folder_unix, NULL);
	      mime_type = entry->mime_type;
	    }
	}
      else
	icon_type = ICON_NONE;
    }
  else
    {
#if 0
      g_print ("No folder open for %s\n", filename);
#endif

      icon_type = get_icon_type (filename, error);
      if (icon_type == ICON_REGULAR)
	mime_type = xdg_mime_get_mime_type_for_file (filename);
    }


  /* FIXME: this function should not return NULL without setting the GError; we
   * should perhaps provide a "never fails" generic stock icon for when all else
   * fails.
   */

  if (icon_type == ICON_NONE)
    return NULL;

  if (icon_type != ICON_REGULAR)
    {
      const char *name;

      switch (icon_type)
	{
	case ICON_BLOCK_DEVICE:
          name = "gnome-fs-blockdev";
	  break;
	case ICON_BROKEN_SYMBOLIC_LINK:
	  name = "gnome-fs-symlink";
	  break;
	case ICON_CHARACTER_DEVICE:
	  name = "gnome-fs-chardev";
	  break;
	case ICON_DIRECTORY:
	  name = get_icon_for_directory (filename);
	  break;
	case ICON_EXECUTABLE:
	  name ="gnome-fs-executable";
	  break;
	case ICON_FIFO:
	  name = "gnome-fs-fifo";
	  break;
	case ICON_SOCKET:
	  name = "gnome-fs-socket";
	  break;
	default:
	  g_assert_not_reached ();
	  return NULL;
	}

      return get_cached_icon (widget, name, pixel_size);
    }

  if (mime_type)
    {
      const char *separator;
      GString *icon_name;
      GdkPixbuf *pixbuf;

      separator = strchr (mime_type, '/');
      if (!separator)
	return NULL;

      icon_name = g_string_new ("gnome-mime-");
      g_string_append_len (icon_name, mime_type, separator - mime_type);
      g_string_append_c (icon_name, '-');
      g_string_append (icon_name, separator + 1);
      pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
      g_string_free (icon_name, TRUE);
      if (pixbuf)
	return pixbuf;

      icon_name = g_string_new ("gnome-mime-");
      g_string_append_len (icon_name, mime_type, separator - mime_type);
      pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
      g_string_free (icon_name, TRUE);
      if (pixbuf)
	return pixbuf;
    }

  return get_cached_icon (widget, "gnome-fs-regular", pixel_size);
}

static void
bookmark_list_free (GSList *list)
{
  GSList *l;

  for (l = list; l; l = l->next)
    g_free (l->data);

  g_slist_free (list);
}

/* Returns whether a URI is a local file:// */
static gboolean
is_local_uri (const char *uri)
{
  char *filename;
  char *hostname;
  gboolean result;

  /* This is rather crude, but hey */
  filename = g_filename_from_uri (uri, &hostname, NULL);

  result = (filename && !hostname);

  g_free (filename);
  g_free (hostname);

  return result;
}

static char *
bookmark_get_filename (gboolean tmp_file)
{
  char *filename;

  filename = g_build_filename (g_get_home_dir (),
			       tmp_file ? BOOKMARKS_TMP_FILENAME : BOOKMARKS_FILENAME,
			       NULL);
  g_assert (filename != NULL);
  return filename;
}

static gboolean
bookmark_list_read (GSList **bookmarks, GError **error)
{
  gchar *filename;
  gchar *contents;
  gboolean result = FALSE;

  filename = bookmark_get_filename (FALSE);
  *bookmarks = NULL;

  if (g_file_get_contents (filename, &contents, NULL, error))
    {
      gchar **lines = g_strsplit (contents, "\n", -1);
      int i;
      GHashTable *table;

      table = g_hash_table_new (g_str_hash, g_str_equal);

      for (i = 0; lines[i]; i++)
	{
	  if (lines[i][0] && !g_hash_table_lookup (table, lines[i]))
	    {
	      *bookmarks = g_slist_prepend (*bookmarks, g_strdup (lines[i]));
	      g_hash_table_insert (table, lines[i], lines[i]);
	    }
	}

      g_free (contents);
      g_hash_table_destroy (table);
      g_strfreev (lines);

      *bookmarks = g_slist_reverse (*bookmarks);
      result = TRUE;
    }

  g_free (filename);

  return result;
}

static gboolean
bookmark_list_write (GSList *bookmarks, GError **error)
{
  char *tmp_filename;
  char *filename;
  gboolean result = TRUE;
  FILE *file;
  int fd;
  int saved_errno;

  /* First, write a temporary file */

  tmp_filename = bookmark_get_filename (TRUE);
  filename = bookmark_get_filename (FALSE);

  fd = g_mkstemp (tmp_filename);
  if (fd == -1)
    {
      saved_errno = errno;
      goto io_error;
    }

  if ((file = fdopen (fd, "w")) != NULL)
    {
      GSList *l;

      for (l = bookmarks; l; l = l->next)
	if (fputs (l->data, file) == EOF
	    || fputs ("\n", file) == EOF)
	  {
	    saved_errno = errno;
	    goto io_error;
	  }

      if (fclose (file) == EOF)
	{
	  saved_errno = errno;
	  goto io_error;
	}

      if (rename (tmp_filename, filename) == -1)
	{
	  saved_errno = errno;
	  goto io_error;
	}

      result = TRUE;
      goto out;
    }
  else
    {
      saved_errno = errno;

      /* fdopen() failed, so we can't do much error checking here anyway */
      close (fd);
    }

 io_error:

  g_set_error (error,
	       GTK_FILE_SYSTEM_ERROR,
	       GTK_FILE_SYSTEM_ERROR_FAILED,
	       _("Bookmark saving failed (%s)"),
	       g_strerror (saved_errno));
  result = FALSE;

  if (fd != -1)
    unlink (tmp_filename); /* again, not much error checking we can do here */

 out:

  g_free (filename);
  g_free (tmp_filename);

  return result;
}

static gboolean
gtk_file_system_unix_insert_bookmark (GtkFileSystem     *file_system,
				      const GtkFilePath *path,
				      gint               position,
				      GError           **error)
{
  GSList *bookmarks;
  int num_bookmarks;
  GSList *l;
  char *uri;
  gboolean result;
  GError *err;

  err = NULL;
  if (!bookmark_list_read (&bookmarks, &err) && err->code != G_FILE_ERROR_NOENT)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  num_bookmarks = g_slist_length (bookmarks);
  g_return_val_if_fail (position >= -1 && position <= num_bookmarks, FALSE);

  result = FALSE;

  uri = gtk_file_system_unix_path_to_uri (file_system, path);

  for (l = bookmarks; l; l = l->next)
    {
      const char *bookmark;

      bookmark = l->data;
      if (strcmp (bookmark, uri) == 0)
	{
	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_ALREADY_EXISTS,
		       "%s already exists in the bookmarks list",
		       uri);
	  goto out;
	}
    }

  bookmarks = g_slist_insert (bookmarks, g_strdup (uri), position);
  if (bookmark_list_write (bookmarks, error))
    {
      result = TRUE;
      g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
    }

 out:

  g_free (uri);
  bookmark_list_free (bookmarks);

  return result;
}

static gboolean
gtk_file_system_unix_remove_bookmark (GtkFileSystem     *file_system,
				      const GtkFilePath *path,
				      GError           **error)
{
  GSList *bookmarks;
  char *uri;
  GSList *l;
  gboolean result;

  if (!bookmark_list_read (&bookmarks, error))
    return FALSE;

  result = FALSE;

  uri = gtk_file_system_path_to_uri (file_system, path);

  for (l = bookmarks; l; l = l->next)
    {
      const char *bookmark;

      bookmark = l->data;
      if (strcmp (bookmark, uri) == 0)
	{
	  g_free (l->data);
	  bookmarks = g_slist_remove_link (bookmarks, l);
	  g_slist_free_1 (l);

	  if (bookmark_list_write (bookmarks, error))
	    {
	      result = TRUE;
	      g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
	    }

	  goto out;
	}
    }

  g_set_error (error,
	       GTK_FILE_SYSTEM_ERROR,
	       GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
	       "%s does not exist in the bookmarks list",
	       uri);

 out:

  g_free (uri);
  bookmark_list_free (bookmarks);

  return result;
}

static GSList *
gtk_file_system_unix_list_bookmarks (GtkFileSystem *file_system)
{
  GSList *bookmarks;
  GSList *result;
  GSList *l;

  if (!bookmark_list_read (&bookmarks, NULL))
    return NULL;

  result = NULL;

  for (l = bookmarks; l; l = l->next)
    {
      const char *name;

      name = l->data;

      if (is_local_uri (name))
	result = g_slist_prepend (result, gtk_file_system_unix_uri_to_path (file_system, name));
    }

  bookmark_list_free (bookmarks);

  result = g_slist_reverse (result);
  return result;
}

/*
 * GtkFileFolderUnix
 */
static GType
gtk_file_folder_unix_get_type (void)
{
  static GType file_folder_unix_type = 0;

  if (!file_folder_unix_type)
    {
      static const GTypeInfo file_folder_unix_info =
      {
	sizeof (GtkFileFolderUnixClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_folder_unix_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileFolderUnix),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_folder_unix_init,
      };

      static const GInterfaceInfo file_folder_info =
      {
	(GInterfaceInitFunc) gtk_file_folder_unix_iface_init, /* interface_init */
	NULL,			                              /* interface_finalize */
	NULL			                              /* interface_data */
      };

      file_folder_unix_type = g_type_register_static (G_TYPE_OBJECT,
						      "GtkFileFolderUnix",
						      &file_folder_unix_info, 0);
      g_type_add_interface_static (file_folder_unix_type,
				   GTK_TYPE_FILE_FOLDER,
				   &file_folder_info);
    }

  return file_folder_unix_type;
}

static void
gtk_file_folder_unix_class_init (GtkFileFolderUnixClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  folder_parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_folder_unix_finalize;
}

static void
gtk_file_folder_unix_iface_init (GtkFileFolderIface *iface)
{
  iface->get_info = gtk_file_folder_unix_get_info;
  iface->list_children = gtk_file_folder_unix_list_children;
}

static void
gtk_file_folder_unix_init (GtkFileFolderUnix *impl)
{
}

static void
gtk_file_folder_unix_finalize (GObject *object)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (object);

  g_hash_table_remove (folder_unix->system_unix->folder_hash, folder_unix->filename);

  if (folder_unix->stat_info)
    {
#if 0
      g_print ("Releasing information for directory %s\n", folder_unix->filename);
#endif
      g_hash_table_destroy (folder_unix->stat_info);
    }

  g_free (folder_unix->filename);

  folder_parent_class->finalize (object);
}

static GtkFileInfo *
gtk_file_folder_unix_get_info (GtkFileFolder      *folder,
			       const GtkFilePath  *path,
			       GError            **error)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (folder);
  GtkFileInfo *info;
  gchar *dirname, *basename;
  const char *filename;
  struct stat_info_entry *entry;
  gboolean file_must_exist;
  GtkFileInfoType types;

  if (!path)
    {
      struct stat buf;

      g_return_val_if_fail (filename_is_root (folder_unix->filename), NULL);

      if (stat (folder_unix->filename, &buf) != 0)
	return NULL;

      info = gtk_file_info_new ();
      gtk_file_info_set_display_name (info, "/");
      gtk_file_info_set_is_folder (info, TRUE);
      gtk_file_info_set_is_hidden (info, FALSE);
      gtk_file_info_set_mime_type (info, "x-directory/normal");
      gtk_file_info_set_modification_time (info, buf.st_mtime);
      gtk_file_info_set_size (info, buf.st_size);

      return info;
    }

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  dirname = get_parent_dir (filename);
  g_return_val_if_fail (strcmp (dirname, folder_unix->filename) == 0, NULL);
  g_free (dirname);

  basename = g_path_get_basename (filename);
  types = folder_unix->types;
  file_must_exist = (types & ~GTK_FILE_INFO_DISPLAY_NAME) != 0;
  entry = file_must_exist
    ? g_hash_table_lookup (folder_unix->stat_info, basename)
    : NULL;
  /* basename freed later.  */

  if (!file_must_exist || entry)
    {
      info = gtk_file_info_new ();
    }
  else
    {
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error getting information for '%s'"),
		   filename_utf8 ? filename_utf8 : "???");
      g_free (filename_utf8);
      info = NULL;
      types = 0;
    }

  if (types & GTK_FILE_INFO_DISPLAY_NAME)
    {
      gchar *display_name = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
      if (!display_name)
	display_name = g_strescape (basename, NULL);

      gtk_file_info_set_display_name (info, display_name);

      g_free (display_name);
    }

  if (types & GTK_FILE_INFO_IS_HIDDEN)
    gtk_file_info_set_is_hidden (info, basename[0] == '.');

  if (types & GTK_FILE_INFO_IS_FOLDER)
    gtk_file_info_set_is_folder (info, S_ISDIR (entry->statbuf.st_mode));

  if (types & GTK_FILE_INFO_MIME_TYPE)
    gtk_file_info_set_mime_type (info, entry->mime_type);

  if (types & GTK_FILE_INFO_MODIFICATION_TIME)
    gtk_file_info_set_modification_time (info, entry->statbuf.st_mtime);

  if (types & GTK_FILE_INFO_SIZE)
    gtk_file_info_set_size (info, (gint64)entry->statbuf.st_size);

  g_free (basename);

  return info;
}


static void
cb_list_children (gpointer key, gpointer value, gpointer user_data)
{
  GSList **children = user_data;
  *children = g_slist_prepend (*children, key);
}

static gboolean
gtk_file_folder_unix_list_children (GtkFileFolder  *folder,
				    GSList        **children,
				    GError        **error)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (folder);
  GSList *l;

  if (!fill_in_names (folder_unix, error))
    return FALSE;

  *children = NULL;

  /* Get the list of basenames.  */
  g_hash_table_foreach (folder_unix->stat_info, cb_list_children, children);

  /* Turn basenames into GFilePaths.  */
  for (l = *children; l; l = l->next)
    {
      const char *basename = l->data;
      char *fullname = g_build_filename (folder_unix->filename, basename, NULL);
      l->data = filename_to_path (fullname);
      g_free (fullname);
    }
  return TRUE;
}


static void
free_stat_info_entry (struct stat_info_entry *entry)
{
  g_free (entry->mime_type);
  g_free (entry);
}

static gboolean
fill_in_names (GtkFileFolderUnix *folder_unix, GError **error)
{
  GDir *dir;

  if (folder_unix->stat_info)
    return TRUE;

#if 0
  g_print ("Reading directory %s\n", folder_unix->filename);
#endif

  folder_unix->stat_info = g_hash_table_new_full (g_str_hash, g_str_equal,
						  (GDestroyNotify)g_free,
						  (GDestroyNotify)free_stat_info_entry);
  dir = g_dir_open (folder_unix->filename, 0, error);
  if (!dir)
    {
      int save_errno = errno;
      gchar *filename_utf8 = g_filename_to_utf8 (folder_unix->filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error getting information for '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (save_errno));
      g_free (filename_utf8);
      return FALSE;
    }

  while (TRUE)
    {
      const gchar *basename = g_dir_read_name (dir);
      if (!basename)
	break;

      g_hash_table_insert (folder_unix->stat_info,
			   g_strdup (basename),
			   g_new0 (struct stat_info_entry, 1));
    }

  g_dir_close (dir);

  folder_unix->asof = time (NULL);
  return TRUE;
}

static gboolean
cb_fill_in_stats (gpointer key, gpointer value, gpointer user_data)
{
  const char *basename = key;
  struct stat_info_entry *entry = value;
  GtkFileFolderUnix *folder_unix = user_data;
  char *fullname = g_build_filename (folder_unix->filename, basename, NULL);
  gboolean result;

  if (stat (fullname, &entry->statbuf) == -1 &&
      (errno != ENOENT || lstat (fullname, &entry->statbuf) == -1))
    result = TRUE;  /* Couldn't stat -- remove from hash.  */
  else
    result = FALSE;

  g_free (fullname);
  return result;
}


static gboolean
fill_in_stats (GtkFileFolderUnix *folder_unix, GError **error)
{
  if (folder_unix->have_stat)
    return TRUE;

  if (!fill_in_names (folder_unix, error))
    return FALSE;

#if 0
  g_print ("Stating directory %s\n", folder_unix->filename);
#endif
  g_hash_table_foreach_remove (folder_unix->stat_info,
			       cb_fill_in_stats,
			       folder_unix);

  folder_unix->have_stat = TRUE;
  return TRUE;
}


static gboolean
cb_fill_in_mime_type (gpointer key, gpointer value, gpointer user_data)
{
  const char *basename = key;
  struct stat_info_entry *entry = value;
  GtkFileFolderUnix *folder_unix = user_data;
  char *fullname = g_build_filename (folder_unix->filename, basename, NULL);

  /* FIXME: Should not need to re-stat.  */
  const char *mime_type = xdg_mime_get_mime_type_for_file (fullname);
  entry->mime_type = g_strdup (mime_type);

  g_free (fullname);
  /* FIXME: free on NULL?  */
  return FALSE;
}

static gboolean
fill_in_mime_type (GtkFileFolderUnix *folder_unix, GError **error)
{
  if (folder_unix->have_mime_type)
    return TRUE;

#if 0
  g_print ("Getting mime types for directory %s\n", folder_unix->filename);
#endif
  g_hash_table_foreach_remove (folder_unix->stat_info,
			       cb_fill_in_mime_type,
			       folder_unix);

  folder_unix->have_mime_type = TRUE;
  return TRUE;
}

static GtkFilePath *
filename_to_path (const char *filename)
{
  return gtk_file_path_new_dup (filename);
}

static gboolean
filename_is_root (const char *filename)
{
  const gchar *after_root;

  after_root = g_path_skip_root (filename);

  return (after_root != NULL && *after_root == '\0');
}
