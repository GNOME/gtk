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

#define BOOKMARKS_FILENAME ".gtk-bookmarks"
#define BOOKMARKS_TMP_FILENAME ".gtk-bookmarks-XXXXXX"

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
};

/* Icon type, supplemented by MIME type
 */
typedef enum {
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

  GtkFileInfoType types;
  gchar *filename;
};

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

static gboolean gtk_file_system_unix_add_bookmark    (GtkFileSystem     *file_system,
						      const GtkFilePath *path,
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
static GtkFileInfo *filename_get_info (const gchar      *filename,
				       GtkFileInfoType   types,
				       GError          **error);

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
  iface->add_bookmark = gtk_file_system_unix_add_bookmark;
  iface->remove_bookmark = gtk_file_system_unix_remove_bookmark;
  iface->list_bookmarks = gtk_file_system_unix_list_bookmarks;
}

static void
gtk_file_system_unix_init (GtkFileSystemUnix *system_unix)
{
}

static void
gtk_file_system_unix_finalize (GObject *object)
{
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

static GtkFileFolder *
gtk_file_system_unix_get_folder (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GtkFileInfoType    types,
				 GError           **error)
{
  GtkFileFolderUnix *folder_unix;
  const char *filename;

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  folder_unix = g_object_new (GTK_TYPE_FILE_FOLDER_UNIX, NULL);
  folder_unix->filename = g_strdup (filename);
  folder_unix->types = types;

  return GTK_FILE_FOLDER (folder_unix);
}

static gboolean
gtk_file_system_unix_create_folder (GtkFileSystem     *file_system,
				    const GtkFilePath *path,
				    GError           **error)
{
  const char *filename;
  gboolean result;

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  result = mkdir (filename, 0777) == 0;

  if (!result)
    {
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error creating directory '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (errno));
      g_free (filename_utf8);
    }

  return result;
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
get_icon_type (const char *filename,
	       GError    **error)
{
  struct stat statbuf;
  IconType icon_type;

  /* If stat fails, try to fall back to lstat to catch broken links
   */
  if (stat (filename, &statbuf) != 0 &&
      lstat (filename, &statbuf) != 0)
    {
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error getting information for '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (errno));
      g_free (filename_utf8);

      return ICON_NONE;
    }

  if (S_ISBLK (statbuf.st_mode))
    icon_type = ICON_BLOCK_DEVICE;
  else if (S_ISLNK (statbuf.st_mode))
    icon_type = ICON_BROKEN_SYMBOLIC_LINK; /* See above */
  else if (S_ISCHR (statbuf.st_mode))
    icon_type = ICON_CHARACTER_DEVICE;
  else if (S_ISDIR (statbuf.st_mode))
    icon_type = ICON_DIRECTORY;
  else if (S_ISFIFO (statbuf.st_mode))
    icon_type =  ICON_FIFO;
  else if (S_ISSOCK (statbuf.st_mode))
    icon_type = ICON_SOCKET;
  else
    {
      icon_type = ICON_REGULAR;

#if 0
      if ((types & GTK_FILE_INFO_ICON) && icon_type == GTK_FILE_ICON_REGULAR &&
	  (statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
	  (strcmp (mime_type, XDG_MIME_TYPE_UNKNOWN) == 0 ||
	   strcmp (mime_type, "application/x-executable") == 0 ||
	   strcmp (mime_type, "application/x-shellscript") == 0))
	gtk_file_info_set_icon_type (info, GTK_FILE_ICON_EXECUTABLE);
#endif
    }

  return icon_type;
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
      gchar *parent_filename = g_path_get_dirname (filename);
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

static GdkPixbuf *
gtk_file_system_unix_render_icon (GtkFileSystem     *file_system,
				  const GtkFilePath *path,
				  GtkWidget         *widget,
				  gint               pixel_size,
				  GError           **error)
{
  const char *filename;
  IconType icon_type;
  const char *mime_type;

  filename = gtk_file_path_get_string (path);
  icon_type = get_icon_type (filename, error);

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
	  name = "gnome-fs-directory";
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

  mime_type = xdg_mime_get_mime_type_for_file (filename);
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
gtk_file_system_unix_add_bookmark (GtkFileSystem     *file_system,
				   const GtkFilePath *path,
				   GError           **error)
{
  GSList *bookmarks;
  GSList *l;
  char *uri;
  gboolean result;

  if (!bookmark_list_read (&bookmarks, error))
    return FALSE;

  result = FALSE;

  uri = gtk_file_system_unix_path_to_uri (file_system, path);

  for (l = bookmarks; l; l = l->next)
    {
      const char *bookmark;

      bookmark = l->data;
      if (strcmp (bookmark, uri) == 0)
	break;
    }

  if (!l)
    {
      bookmarks = g_slist_append (bookmarks, g_strdup (uri));
      if (bookmark_list_write (bookmarks, error))
	{
	  result = TRUE;
	  g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
	}
    }

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
	break;
    }

  if (l)
    {
      g_free (l->data);
      bookmarks = g_slist_remove_link (bookmarks, l);
      g_slist_free_1 (l);

      if (bookmark_list_write (bookmarks, error))
	result = TRUE;
    }
  else
    result = TRUE;

  g_free (uri);
  bookmark_list_free (bookmarks);

  if (result)
    g_signal_emit_by_name (file_system, "bookmarks-changed", 0);

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

  g_free (folder_unix->filename);

  folder_parent_class->finalize (object);
}

static GtkFileInfo *
gtk_file_folder_unix_get_info (GtkFileFolder  *folder,
			       const GtkFilePath    *path,
			       GError        **error)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (folder);
  GtkFileInfo *info;
  gchar *dirname;
  const char *filename;

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  dirname = g_path_get_dirname (filename);
  g_return_val_if_fail (strcmp (dirname, folder_unix->filename) == 0, NULL);
  g_free (dirname);

  info = filename_get_info (filename, folder_unix->types, error);

  return info;
}

static gboolean
gtk_file_folder_unix_list_children (GtkFileFolder  *folder,
				    GSList        **children,
				    GError        **error)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (folder);
  GError *tmp_error = NULL;
  GDir *dir;

  *children = NULL;

  dir = g_dir_open (folder_unix->filename, 0, &tmp_error);
  if (!dir)
    {
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   "%s",
		   tmp_error->message);

      g_error_free (tmp_error);

      return FALSE;
    }

  while (TRUE)
    {
      const gchar *filename = g_dir_read_name (dir);
      gchar *fullname;

      if (!filename)
	break;

      fullname = g_build_filename (folder_unix->filename, filename, NULL);
      *children = g_slist_prepend (*children, filename_to_path (fullname));
      g_free (fullname);
    }

  g_dir_close (dir);

  *children = g_slist_reverse (*children);

  return TRUE;
}

static GtkFileInfo *
filename_get_info (const gchar     *filename,
		   GtkFileInfoType  types,
		   GError         **error)
{
  GtkFileInfo *info;
  struct stat statbuf;

  /* If stat fails, try to fall back to lstat to catch broken links
   */
  if (stat (filename, &statbuf) != 0 &&
      lstat (filename, &statbuf) != 0)
    {
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error getting information for '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (errno));
      g_free (filename_utf8);

      return NULL;
    }

  info = gtk_file_info_new ();

  if (filename_is_root (filename))
    {
      if (types & GTK_FILE_INFO_DISPLAY_NAME)
	gtk_file_info_set_display_name (info, "/");

      if (types & GTK_FILE_INFO_IS_HIDDEN)
	gtk_file_info_set_is_hidden (info, FALSE);
    }
  else
    {
      gchar *basename = g_path_get_basename (filename);

      if (types & GTK_FILE_INFO_DISPLAY_NAME)
	{
	  gchar *display_name = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
	  if (!display_name)
	    display_name = g_strescape (basename, NULL);

	  gtk_file_info_set_display_name (info, display_name);

	  g_free (display_name);
	}

      if (types & GTK_FILE_INFO_IS_HIDDEN)
	{
	  gtk_file_info_set_is_hidden (info, basename[0] == '.');
	}

      g_free (basename);
    }

  if (types & GTK_FILE_INFO_IS_FOLDER)
    {
      gtk_file_info_set_is_folder (info, S_ISDIR (statbuf.st_mode));
   }

  if (types & GTK_FILE_INFO_MIME_TYPE)
    {
      const char *mime_type = xdg_mime_get_mime_type_for_file (filename);
      gtk_file_info_set_mime_type (info, mime_type);
    }

  if (types & GTK_FILE_INFO_MODIFICATION_TIME)
    {
      gtk_file_info_set_modification_time (info, statbuf.st_mtime);
    }

  if (types & GTK_FILE_INFO_SIZE)
    {
      gtk_file_info_set_size (info, (gint64)statbuf.st_size);
    }

  return info;
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
