/* GTK - The GIMP Toolkit
 * gtkfilesystemwin32.c: Default implementation of GtkFileSystem for Windows
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
#include "gtkfilesystemwin32.h"
#include "gtkintl.h"
#include "gtkstock.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#define mkdir(p,m) _mkdir(p)
#else
#error "The implementation is win32 only yet."
#endif /* G_OS_WIN32 */

typedef struct _GtkFileSystemWin32Class GtkFileSystemWin32Class;

#define GTK_FILE_SYSTEM_WIN32_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_SYSTEM_WIN32, GtkFileSystemWin32Class))
#define GTK_IS_FILE_SYSTEM_WIN32_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_SYSTEM_WIN32))
#define GTK_FILE_SYSTEM_WIN32_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_SYSTEM_WIN32, GtkFileSystemWin32Class))

struct _GtkFileSystemWin32Class
{
  GObjectClass parent_class;
};

struct _GtkFileSystemWin32
{
  GObject parent_instance;
};

#define GTK_TYPE_FILE_FOLDER_WIN32             (gtk_file_folder_win32_get_type ())
#define GTK_FILE_FOLDER_WIN32(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_FOLDER_WIN32, GtkFileFolderWin32))
#define GTK_IS_FILE_FOLDER_WIN32(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_FOLDER_WIN32))
#define GTK_FILE_FOLDER_WIN32_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_FOLDER_WIN32, GtkFileFolderWin32Class))
#define GTK_IS_FILE_FOLDER_WIN32_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_FOLDER_WIN32))
#define GTK_FILE_FOLDER_WIN32_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_FOLDER_WIN32, GtkFileFolderWin32Class))

typedef struct _GtkFileFolderWin32      GtkFileFolderWin32;
typedef struct _GtkFileFolderWin32Class GtkFileFolderWin32Class;

struct _GtkFileFolderWin32Class
{
  GObjectClass parent_class;
};

struct _GtkFileFolderWin32
{
  GObject parent_instance;

  GtkFileInfoType types;
  gchar *filename;
};

static GObjectClass *system_parent_class;
static GObjectClass *folder_parent_class;

static void           gtk_file_system_win32_class_init       (GtkFileSystemWin32Class  *class);
static void           gtk_file_system_win32_iface_init       (GtkFileSystemIface       *iface);
static void           gtk_file_system_win32_init             (GtkFileSystemWin32       *impl);
static void           gtk_file_system_win32_finalize         (GObject                  *object);
static GSList *       gtk_file_system_win32_list_roots       (GtkFileSystem            *file_system);
static GtkFileInfo *  gtk_file_system_win32_get_root_info    (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GtkFileInfoType           types,
							      GError                  **error);
static GtkFileFolder *gtk_file_system_win32_get_folder       (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GtkFileInfoType           types,
							      GError                  **error);
static gboolean       gtk_file_system_win32_create_folder    (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GError                  **error);
static gboolean       gtk_file_system_win32_get_parent       (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GtkFilePath             **parent,
							      GError                  **error);
static GtkFilePath *  gtk_file_system_win32_make_path        (GtkFileSystem            *file_system,
							      const GtkFilePath        *base_path,
							      const gchar              *display_name,
							      GError                  **error);
static gboolean       gtk_file_system_win32_parse            (GtkFileSystem            *file_system,
							      const GtkFilePath        *base_path,
							      const gchar              *str,
							      GtkFilePath             **folder,
							      gchar                   **file_part,
							      GError                  **error);
static gchar *        gtk_file_system_win32_path_to_uri      (GtkFileSystem            *file_system,
							      const GtkFilePath        *path);
static gchar *        gtk_file_system_win32_path_to_filename (GtkFileSystem            *file_system,
							      const GtkFilePath        *path);
static GtkFilePath *  gtk_file_system_win32_uri_to_path      (GtkFileSystem            *file_system,
							      const gchar              *uri);
static GtkFilePath *  gtk_file_system_win32_filename_to_path (GtkFileSystem            *file_system,
							      const gchar              *filename);
static gboolean       gtk_file_system_win32_add_bookmark     (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GError                  **error);
static gboolean       gtk_file_system_win32_remove_bookmark  (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GError                  **error);
static GSList *       gtk_file_system_win32_list_bookmarks   (GtkFileSystem            *file_system);
static GType          gtk_file_folder_win32_get_type         (void);
static void           gtk_file_folder_win32_class_init       (GtkFileFolderWin32Class  *class);
static void           gtk_file_folder_win32_iface_init       (GtkFileFolderIface       *iface);
static void           gtk_file_folder_win32_init             (GtkFileFolderWin32       *impl);
static void           gtk_file_folder_win32_finalize         (GObject                  *object);
static GtkFileInfo *  gtk_file_folder_win32_get_info         (GtkFileFolder            *folder,
							      const GtkFilePath        *path,
							      GError                  **error);
static gboolean       gtk_file_folder_win32_list_children    (GtkFileFolder            *folder,
							      GSList                  **children,
							      GError                  **error);
static gchar *        filename_from_path                     (const GtkFilePath        *path);
static GtkFilePath *  filename_to_path                       (const gchar              *filename);
static gboolean       filename_is_root                       (const char               *filename);
static GtkFileInfo *  filename_get_info                      (const gchar              *filename,
							      GtkFileInfoType           types,
							      GError                  **error);


/*
 * GtkFileSystemWin32
 */
GType
gtk_file_system_win32_get_type (void)
{
  static GType file_system_win32_type = 0;

  if (!file_system_win32_type)
    {
      static const GTypeInfo file_system_win32_info =
      {
	sizeof (GtkFileSystemWin32Class),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_system_win32_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileSystemWin32),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_system_win32_init,
      };
      
      static const GInterfaceInfo file_system_info =
      {
	(GInterfaceInitFunc) gtk_file_system_win32_iface_init, /* interface_init */
	NULL,			                               /* interface_finalize */
	NULL			                               /* interface_data */
      };

      file_system_win32_type = g_type_register_static (G_TYPE_OBJECT,
						       "GtkFileSystemWin32",
						       &file_system_win32_info, 0);
      g_type_add_interface_static (file_system_win32_type,
				   GTK_TYPE_FILE_SYSTEM,
				   &file_system_info);
    }

  return file_system_win32_type;
}

/**
 * gtk_file_system_win32_new:
 * 
 * Creates a new #GtkFileSystemWin32 object. #GtkFileSystemWin32
 * implements the #GtkFileSystem interface with direct access to
 * the filesystem using Windows API calls
 * 
 * Return value: the new #GtkFileSystemWin32 object
 **/
GtkFileSystem *
gtk_file_system_win32_new (void)
{
  return g_object_new (GTK_TYPE_FILE_SYSTEM_WIN32, NULL);
}

static void
gtk_file_system_win32_class_init (GtkFileSystemWin32Class *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  system_parent_class = g_type_class_peek_parent (class);
  
  gobject_class->finalize = gtk_file_system_win32_finalize;
}

static void
gtk_file_system_win32_iface_init (GtkFileSystemIface *iface)
{
  iface->list_roots = gtk_file_system_win32_list_roots;
  iface->get_folder = gtk_file_system_win32_get_folder;
  iface->get_root_info = gtk_file_system_win32_get_root_info;
  iface->create_folder = gtk_file_system_win32_create_folder;
  iface->get_parent = gtk_file_system_win32_get_parent;
  iface->make_path = gtk_file_system_win32_make_path;
  iface->parse = gtk_file_system_win32_parse;
  iface->path_to_uri = gtk_file_system_win32_path_to_uri;
  iface->path_to_filename = gtk_file_system_win32_path_to_filename;
  iface->uri_to_path = gtk_file_system_win32_uri_to_path;
  iface->filename_to_path = gtk_file_system_win32_filename_to_path;
  iface->add_bookmark = gtk_file_system_win32_add_bookmark;
  iface->remove_bookmark = gtk_file_system_win32_remove_bookmark;
  iface->list_bookmarks = gtk_file_system_win32_list_bookmarks;
}

static void
gtk_file_system_win32_init (GtkFileSystemWin32 *system_win32)
{
}

static void
gtk_file_system_win32_finalize (GObject *object)
{
  system_parent_class->finalize (object);
}

static GSList *
gtk_file_system_win32_list_roots (GtkFileSystem *file_system)
{
  gchar   drives[26*4];
  guint   len;
  gchar  *p;
  GSList *list = NULL;

  len = GetLogicalDriveStrings(sizeof(drives), drives);

  if (len < 3)
    g_warning("No drive strings available!");

  p = drives;
  while ((len = strlen(p)) != 0)
    {
       /* skip floppy */
       if (p[0] != 'a' && p[0] != 'b')
         {
	   /*FIXME: gtk_file_path_compare() is case sensitive, we are not*/
	   p[0] = toupper (p[0]);
	   /* needed without the backslash */
	   p[2] = '\0';
           list = g_slist_append (list, gtk_file_path_new_dup (p));
	 }
       p += len + 1;
    }
  return list;
}

static GtkFileInfo *
gtk_file_system_win32_get_root_info (GtkFileSystem    *file_system,
				    const GtkFilePath *path,
				    GtkFileInfoType    types,
				    GError           **error)
{
  /* needed _with_ the trailing backslash */
  gchar *filename = g_strconcat(gtk_file_path_get_string (path), "\\", NULL);
  GtkFileInfo *info;
  DWORD dt = GetDriveType (filename);

  info = filename_get_info (filename, types, error);

  /* additional info */
  if (GTK_FILE_INFO_DISPLAY_NAME & types)
    {
      gchar display_name[80];

      if (GetVolumeInformation (filename, 
                                display_name, sizeof(display_name),
                                NULL, /* serial number */
                                NULL, /* max. component length */
                                NULL, /* fs flags */
                                NULL, 0)) /* fs type like FAT, NTFS */
        {
          gchar* real_display_name = g_strconcat (display_name, " (", filename, ")", NULL);

          gtk_file_info_set_display_name (info, real_display_name);
          g_free (real_display_name);
        }
      else
        gtk_file_info_set_display_name (info, filename);
    }

  if (GTK_FILE_INFO_ICON & types)
    {
      switch (dt)
        {
        case DRIVE_REMOVABLE :
          /*gtk_file_info_set_icon_type (info, GTK_STOCK_FLOPPY);*/
          break;
        case DRIVE_CDROM :
          /*gtk_file_info_set_icon_type (info, GTK_STOCK_CDROM);*/
          break;
        case DRIVE_REMOTE :
          /*FIXME: need a network stock icon*/
        case DRIVE_FIXED :
          /*FIXME: need a hard disk stock icon*/
        case DRIVE_RAMDISK :
          /*FIXME: need a ram stock icon
            gtk_file_info_set_icon_type (info, GTK_STOCK_OPEN);*/
          break;
        default :
          g_assert_not_reached ();
        }
    }
  g_free (filename);
  return info;
}

static GtkFileFolder *
gtk_file_system_win32_get_folder (GtkFileSystem    *file_system,
				 const GtkFilePath *path,
				 GtkFileInfoType    types,
				 GError           **error)
{
  GtkFileFolderWin32 *folder_win32;
  gchar *filename;

  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, NULL);

  folder_win32 = g_object_new (GTK_TYPE_FILE_FOLDER_WIN32, NULL);
  folder_win32->filename = filename;
  folder_win32->types = types;

  return GTK_FILE_FOLDER (folder_win32);
}

static gboolean
gtk_file_system_win32_create_folder (GtkFileSystem     *file_system,
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
		   _("error creating directory '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_strerror (errno));
      g_free (filename_utf8);
    }
  
  g_free (filename);
  
  return result;
}

static gboolean
gtk_file_system_win32_get_parent (GtkFileSystem     *file_system,
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
gtk_file_system_win32_make_path (GtkFileSystem     *file_system,
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
gtk_file_system_win32_parse (GtkFileSystem     *file_system,
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
	  if (folder_part[1] == ':')
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
gtk_file_system_win32_path_to_uri (GtkFileSystem     *file_system,
				   const GtkFilePath *path)
{
  return g_filename_to_uri (gtk_file_path_get_string (path), NULL, NULL);
}

static gchar *
gtk_file_system_win32_path_to_filename (GtkFileSystem     *file_system,
				        const GtkFilePath *path)
{
  return g_strdup (gtk_file_path_get_string (path));
}

static GtkFilePath *
gtk_file_system_win32_uri_to_path (GtkFileSystem     *file_system,
				   const gchar       *uri)
{
  gchar *filename = g_filename_from_uri (uri, NULL, NULL);
  if (filename)
    return gtk_file_path_new_steal (filename);
  else
    return NULL;
}

static GtkFilePath *
gtk_file_system_win32_filename_to_path (GtkFileSystem *file_system,
				        const gchar   *filename)
{
  return gtk_file_path_new_dup (filename);
}

static gboolean
bookmarks_serialize (GSList  **bookmarks,
                     gchar    *uri,
                     gboolean  add,
		     GError  **error)
{
  gchar   *filename;
  gboolean ok = TRUE;
  GSList  *list = *bookmarks;

  filename = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);

  if (filename)
    {
      gchar *contents = NULL;
      gsize  len = 0;
      GList *entry;
      FILE  *f;   
       
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          if (g_file_get_contents (filename, &contents, &len, error))
	    {
	      gchar **lines = g_strsplit (contents, "\n", -1);
	      gint    i;

	      for (i = 0; lines[i] != NULL; i++)
		{
		  if (lines[i][0] && !g_slist_find_custom (list, lines[i], strcmp))
		    list = g_slist_append (list, g_strdup (lines[i]));
		}
	      g_strfreev (lines);
	    }
	  else
	    ok = FALSE;
	}
      if (ok && (f = fopen (filename, "wb")) != NULL)
        {
	  for (entry = list; entry != NULL; entry = entry->next)
	    {
	      gchar *line = entry->data;

	      if (strcmp (line, uri) != 0)
	        {
	          fputs (line, f);
		  fputs ("\n", f);
		}
	    }
	  if (add)
	    {
	      fputs (uri, f);
	      fputs ("\n", f);
	    }
	  fclose (f);
	}
      else if (ok && error)
        {
	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_FAILED,
		       _("Bookmark saving failed (%s)"),
		       g_strerror (errno));
	}
    }
  *bookmarks = list;
  return ok;
}

static GSList *_bookmarks = NULL;

static gboolean
gtk_file_system_win32_add_bookmark (GtkFileSystem     *file_system,
				    const GtkFilePath *path,
				    GError           **error)
{
  gchar *uri = gtk_file_system_win32_path_to_uri (file_system, path);
  gboolean ret = bookmarks_serialize (&_bookmarks, uri, TRUE, error);
  g_free (uri);
  return ret;
                               
}

static gboolean
gtk_file_system_win32_remove_bookmark (GtkFileSystem     *file_system,
				       const GtkFilePath *path,
				       GError           **error)
{
  gchar *uri = gtk_file_system_win32_path_to_uri (file_system, path);
  gboolean ret = bookmarks_serialize (&_bookmarks, uri, FALSE, error);
  g_free (uri);
  return ret;
}

static GSList *
gtk_file_system_win32_list_bookmarks (GtkFileSystem *file_system)
{
  GSList *list = NULL;
  GSList *entry;


  if (bookmarks_serialize (&_bookmarks, "", FALSE, NULL))
    {
      for (entry = _bookmarks; entry != NULL; entry = entry->next)
        {
	  GtkFilePath *path = gtk_file_system_win32_uri_to_path (
			        file_system, (gchar *)entry->data);

          list = g_slist_append (list, path);
	}
    }

  return list;
}

/*
 * GtkFileFolderWin32
 */
static GType
gtk_file_folder_win32_get_type (void)
{
  static GType file_folder_win32_type = 0;

  if (!file_folder_win32_type)
    {
      static const GTypeInfo file_folder_win32_info =
      {
	sizeof (GtkFileFolderWin32Class),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_folder_win32_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileFolderWin32),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_folder_win32_init,
      };
      
      static const GInterfaceInfo file_folder_info =
      {
	(GInterfaceInitFunc) gtk_file_folder_win32_iface_init, /* interface_init */
	NULL,			                              /* interface_finalize */
	NULL			                              /* interface_data */
      };

      file_folder_win32_type = g_type_register_static (G_TYPE_OBJECT,
						      "GtkFileFolderWin32",
						      &file_folder_win32_info, 0);
      g_type_add_interface_static (file_folder_win32_type,
				   GTK_TYPE_FILE_FOLDER,
				   &file_folder_info);
    }

  return file_folder_win32_type;
}

static void
gtk_file_folder_win32_class_init (GtkFileFolderWin32Class *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  folder_parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_folder_win32_finalize;
}

static void
gtk_file_folder_win32_iface_init (GtkFileFolderIface *iface)
{
  iface->get_info = gtk_file_folder_win32_get_info;
  iface->list_children = gtk_file_folder_win32_list_children;
}

static void
gtk_file_folder_win32_init (GtkFileFolderWin32 *impl)
{
}

static void
gtk_file_folder_win32_finalize (GObject *object)
{
  GtkFileFolderWin32 *folder_win32 = GTK_FILE_FOLDER_WIN32 (object);

  g_free (folder_win32->filename);
  
  folder_parent_class->finalize (object);
}

static GtkFileInfo *
gtk_file_folder_win32_get_info (GtkFileFolder     *folder,
			        const GtkFilePath *path,
			        GError           **error)
{
  GtkFileFolderWin32 *folder_win32 = GTK_FILE_FOLDER_WIN32 (folder);
  GtkFileInfo *info;
  gchar *dirname;
  gchar *filename;
  
  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, NULL);

#if 0
  dirname = g_path_get_dirname (filename);
  g_return_val_if_fail (strcmp (dirname, folder_win32->filename) == 0, NULL);
  g_free (dirname);
#endif

  info = filename_get_info (filename, folder_win32->types, error);

  g_free (filename);

  return info;
}

static gboolean
gtk_file_folder_win32_list_children (GtkFileFolder  *folder,
				     GSList        **children,
				     GError        **error)
{
  GtkFileFolderWin32 *folder_win32 = GTK_FILE_FOLDER_WIN32 (folder);
  GError *tmp_error = NULL;
  GDir *dir;

  *children = NULL;

  dir = g_dir_open (folder_win32->filename, 0, &tmp_error);
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

      fullname = g_build_filename (folder_win32->filename, filename, NULL);
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
  WIN32_FILE_ATTRIBUTE_DATA wfad;

  if (!GetFileAttributesEx (filename, GetFileExInfoStandard, &wfad))
    {
      gchar *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("error getting information for '%s': %s"),
		   filename_utf8 ? filename_utf8 : "???",
		   g_win32_error_message (GetLastError ()));
      g_free (filename_utf8);
      
      return NULL;
    }

  info = gtk_file_info_new ();

  if (filename_is_root (filename))
    {
      if (types & GTK_FILE_INFO_DISPLAY_NAME)
	gtk_file_info_set_display_name (info, filename);
      
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
            /* win32 convention ... */
            gboolean is_hidden = basename[0] == '.';
            /* ... _and_ windoze attribute */
            is_hidden = is_hidden || !!(wfad.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
	  gtk_file_info_set_is_hidden (info, is_hidden);
	}

      g_free (basename);
    }

  if (types & GTK_FILE_INFO_IS_FOLDER)
    {
      gtk_file_info_set_is_folder (info, !!(wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
   }

  if (types & GTK_FILE_INFO_ICON)
    {
      if (wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	icon_type = GTK_FILE_ICON_DIRECTORY;

      gtk_file_info_set_icon_type (info, icon_type);
    }

  if ((types & GTK_FILE_INFO_MIME_TYPE) ||
      ((types & GTK_FILE_INFO_ICON) && icon_type == GTK_FILE_ICON_REGULAR))
    {
#if 0
      const char *mime_type = xdg_mime_get_mime_type_for_file (filename);
      gtk_file_info_set_mime_type (info, mime_type);

      if ((types & GTK_FILE_INFO_ICON) && icon_type == GTK_FILE_ICON_REGULAR &&
	  (statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
	  (strcmp (mime_type, XDG_MIME_TYPE_UNKNOWN) == 0 ||
	   strcmp (mime_type, "application/x-executable") == 0 ||
	   strcmp (mime_type, "application/x-shellscript") == 0))
	gtk_file_info_set_icon_type (info, GTK_FILE_ICON_EXECUTABLE);
#endif
    }

  if (types & GTK_FILE_INFO_MODIFICATION_TIME)
    {
      GtkFileTime time = wfad.ftLastWriteTime.dwLowDateTime 
                       | ((guint64)wfad.ftLastWriteTime.dwHighDateTime) << 32;
      /* 100-nanosecond intervals since January 1, 1601, urgh! */
      time /= 10000000I64; /* now seconds */
      time -= 134774I64 * 24 * 3600; /* good old Unix time */
      gtk_file_info_set_modification_time (info, time);
    }

  if (types & GTK_FILE_INFO_SIZE)
    {
      gint64 size = wfad.nFileSizeLow | ((guint64)wfad.nFileSizeHigh) << 32;
      gtk_file_info_set_size (info, size);
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
  guint len = strlen(filename);

  /* accept both forms */

  return (   (len == 2 && filename[1] == ':')
          || (len == 3 && filename[1] == ':' && filename[2] == '\\'));
}

