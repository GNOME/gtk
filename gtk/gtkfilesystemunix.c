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

#include "xdgmime/xdgmime.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

static GSList *       gtk_file_system_unix_list_roots    (GtkFileSystem      *file_system);
static GtkFileInfo *  gtk_file_system_unix_get_root_info (GtkFileSystem      *file_system,
							  const GtkFilePath  *path,
							  GtkFileInfoType     types,
							  GError            **error);
static GtkFileFolder *gtk_file_system_unix_get_folder    (GtkFileSystem      *file_system,
							  const GtkFilePath  *path,
							  GtkFileInfoType     types,
							  GError            **error);
static gboolean       gtk_file_system_unix_create_folder (GtkFileSystem      *file_system,
							  const GtkFilePath  *path,
							  GError            **error);
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

static gboolean       gtk_file_system_unix_get_supports_bookmarks (GtkFileSystem *file_system);
static void           gtk_file_system_unix_set_bookmarks          (GtkFileSystem *file_system,
								   GSList        *bookmarks,
								   GError       **error);
static GSList *       gtk_file_system_unix_list_bookmarks         (GtkFileSystem *file_system);

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

static gchar *      filename_from_path (const GtkFilePath *path);
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
  iface->list_roots = gtk_file_system_unix_list_roots;
  iface->get_folder = gtk_file_system_unix_get_folder;
  iface->get_root_info = gtk_file_system_unix_get_root_info;
  iface->create_folder = gtk_file_system_unix_create_folder;
  iface->get_parent = gtk_file_system_unix_get_parent;
  iface->make_path = gtk_file_system_unix_make_path;
  iface->parse = gtk_file_system_unix_parse;
  iface->path_to_uri = gtk_file_system_unix_path_to_uri;
  iface->path_to_filename = gtk_file_system_unix_path_to_filename;
  iface->uri_to_path = gtk_file_system_unix_uri_to_path;
  iface->filename_to_path = gtk_file_system_unix_filename_to_path;
  iface->get_supports_bookmarks = gtk_file_system_unix_get_supports_bookmarks;
  iface->set_bookmarks = gtk_file_system_unix_set_bookmarks;
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

static GSList *
gtk_file_system_unix_list_roots (GtkFileSystem *file_system)
{
  return g_slist_append (NULL, gtk_file_path_new_dup ("/"));
}

static GtkFileInfo *
gtk_file_system_unix_get_root_info (GtkFileSystem    *file_system,
				    const GtkFilePath      *path,
				    GtkFileInfoType   types,
				    GError          **error)
{
  const gchar *filename = gtk_file_path_get_string (path);
  
  g_return_val_if_fail (strcmp (filename, "/") == 0, NULL);

  return filename_get_info ("/", types, error);
}

static GtkFileFolder *
gtk_file_system_unix_get_folder (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GtkFileInfoType    types,
				 GError           **error)
{
  GtkFileFolderUnix *folder_unix;
  gchar *filename;

  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, NULL);

  folder_unix = g_object_new (GTK_TYPE_FILE_FOLDER_UNIX, NULL);
  folder_unix->filename = filename;
  folder_unix->types = types;

  return GTK_FILE_FOLDER (folder_unix);
}

static gboolean
gtk_file_system_unix_create_folder (GtkFileSystem     *file_system,
				    const GtkFilePath *path,
				    GError           **error)
{
  gchar *filename;
  gboolean result;

  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  
  result = mkdir (filename, 0777) != 0;
  
  if (!result)
    {
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   "error creating directory '%s': %s",
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (errno));
      g_free (filename_utf8);
    }
  
  g_free (filename);
  
  return result;
}

static gboolean
gtk_file_system_unix_get_parent (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GtkFilePath      **parent,
				 GError           **error)
{
  gchar *filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, FALSE);

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

  g_free (filename);

  return TRUE;
}

static GtkFilePath *
gtk_file_system_unix_make_path (GtkFileSystem    *file_system,
			       const GtkFilePath *base_path,
			       const gchar       *display_name,
			       GError           **error)
{
  gchar *base_filename;
  gchar *filename;
  gchar *full_filename;
  GError *tmp_error = NULL;
  GtkFilePath *result;
  
  base_filename = filename_from_path (base_path);
  g_return_val_if_fail (base_filename != NULL, NULL);

  filename = g_filename_from_utf8 (display_name, -1, NULL, NULL, &tmp_error);
  if (!filename)
    {
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
		   "%s",
		   tmp_error->message);
      
      g_error_free (tmp_error);
      g_free (base_filename);

      return FALSE;
    }
    
  full_filename = g_build_filename (base_filename, filename, NULL);
  result = filename_to_path (full_filename);
  g_free (base_filename);
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
  char *base_filename;
  gchar *last_slash;
  gboolean result = FALSE;

  base_filename = filename_from_path (base_path);
  g_return_val_if_fail (base_filename != NULL, FALSE);
  
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

  g_free (base_filename);
  
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

static gboolean
gtk_file_system_unix_get_supports_bookmarks (GtkFileSystem *file_system)
{
  return FALSE;
}

static void
gtk_file_system_unix_set_bookmarks (GtkFileSystem *file_system,
				    GSList        *bookmarks,
				    GError       **error)
{
  g_set_error (error,
	       GTK_FILE_SYSTEM_ERROR,
	       GTK_FILE_SYSTEM_ERROR_FAILED,
	       "This file system does not support bookmarks"); 
}

static GSList *
gtk_file_system_unix_list_bookmarks (GtkFileSystem *file_system)
{
  return NULL;
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
  gchar *filename;
  
  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, NULL);

  dirname = g_path_get_dirname (filename);
  g_return_val_if_fail (strcmp (dirname, folder_unix->filename) == 0, NULL);
  g_free (dirname);

  info = filename_get_info (filename, folder_unix->types, error);

  g_free (filename);

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
  GtkFileIconType icon_type = GTK_FILE_ICON_REGULAR;
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
		   "error getting information for '%s': %s",
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

  if (types & GTK_FILE_INFO_ICON)
    {
      if (S_ISBLK (statbuf.st_mode))
	icon_type = GTK_FILE_ICON_BLOCK_DEVICE;
      else if (S_ISLNK (statbuf.st_mode))
	icon_type = GTK_FILE_ICON_BROKEN_SYMBOLIC_LINK;
      else if (S_ISCHR (statbuf.st_mode))
	icon_type = GTK_FILE_ICON_CHARACTER_DEVICE;
      else if (S_ISDIR (statbuf.st_mode))
	icon_type = GTK_FILE_ICON_DIRECTORY;
      else if (S_ISFIFO (statbuf.st_mode))
	icon_type =  GTK_FILE_ICON_FIFO;
      else if (S_ISSOCK (statbuf.st_mode))
	icon_type = GTK_FILE_ICON_SOCKET;

      gtk_file_info_set_icon_type (info, icon_type);
    }

  if ((types & GTK_FILE_INFO_MIME_TYPE) ||
      ((types & GTK_FILE_INFO_ICON) && icon_type == GTK_FILE_ICON_REGULAR))
    {
      const char *mime_type = xdg_mime_get_mime_type_for_file (filename);
      gtk_file_info_set_mime_type (info, mime_type);

      if ((types & GTK_FILE_INFO_ICON) && icon_type == GTK_FILE_ICON_REGULAR &&
	  (statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
	  (strcmp (mime_type, XDG_MIME_TYPE_UNKNOWN) == 0 ||
	   strcmp (mime_type, "application/x-executable") == 0 ||
	   strcmp (mime_type, "application/x-shellscript") == 0))
	gtk_file_info_set_icon_type (info, GTK_FILE_ICON_EXECUTABLE);
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

static gchar *
filename_from_path (const GtkFilePath *path)
{
  return g_strdup (gtk_file_path_get_string (path));
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
  
  return (*after_root == '\0');
}
