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

#include <config.h>
#include "gtkfilesystem.h"
#include "gtkfilesystemwin32.h"
#include "gtkintl.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h> /* ExtractAssociatedIcon */
#include <direct.h>
#include <io.h>
#define mkdir(p,m) _mkdir(p)
#include <gdk/win32/gdkwin32.h> /* gdk_win32_hdc_get */
#else
#error "The implementation is win32 only."
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

  GHashTable *folder_hash;
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

  GtkFileSystemWin32 *system_win32;
  GtkFileInfoType types;
  gchar *filename;
};

static GObjectClass *system_parent_class;
static GObjectClass *folder_parent_class;

static void           gtk_file_system_win32_class_init       (GtkFileSystemWin32Class  *class);
static void           gtk_file_system_win32_iface_init       (GtkFileSystemIface       *iface);
static void           gtk_file_system_win32_init             (GtkFileSystemWin32       *impl);
static void           gtk_file_system_win32_finalize         (GObject                  *object);

static GSList *       gtk_file_system_win32_list_volumes     (GtkFileSystem      *file_system);
static GtkFileSystemVolume *gtk_file_system_win32_get_volume_for_path (GtkFileSystem     *file_system,
                                                                       const GtkFilePath *path);

static GtkFileFolder *gtk_file_system_win32_get_folder       (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GtkFileInfoType           types,
							      GError                  **error);
static gboolean       gtk_file_system_win32_create_folder    (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      GError                  **error);

static void         gtk_file_system_win32_volume_free             (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static GtkFilePath *gtk_file_system_win32_volume_get_base_path    (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static gboolean     gtk_file_system_win32_volume_get_is_mounted   (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static gboolean     gtk_file_system_win32_volume_mount            (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume,
								   GError             **error);
static gchar *      gtk_file_system_win32_volume_get_display_name (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static GdkPixbuf *  gtk_file_system_win32_volume_render_icon      (GtkFileSystem        *file_system,
								   GtkFileSystemVolume  *volume,
								   GtkWidget            *widget,
								   gint                  pixel_size,
								   GError              **error);

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
static GdkPixbuf *gtk_file_system_win32_render_icon (GtkFileSystem     *file_system,
                                                     const GtkFilePath *path,
                                                     GtkWidget         *widget,
                                                     gint               pixel_size,
                                                     GError           **error);

static gboolean       gtk_file_system_win32_insert_bookmark  (GtkFileSystem            *file_system,
							      const GtkFilePath        *path,
							      gint               position,
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

/* some info kept together for volumes */
struct _GtkFileSystemVolume
{
  gchar    *drive;
  gboolean  is_mounted;
};

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
  iface->list_volumes = gtk_file_system_win32_list_volumes;
  iface->get_volume_for_path = gtk_file_system_win32_get_volume_for_path;
  iface->get_folder = gtk_file_system_win32_get_folder;
  iface->create_folder = gtk_file_system_win32_create_folder;
  iface->volume_free = gtk_file_system_win32_volume_free;
  iface->volume_get_base_path = gtk_file_system_win32_volume_get_base_path;
  iface->volume_get_is_mounted = gtk_file_system_win32_volume_get_is_mounted;
  iface->volume_mount = gtk_file_system_win32_volume_mount;
  iface->volume_get_display_name = gtk_file_system_win32_volume_get_display_name;
  iface->volume_render_icon = gtk_file_system_win32_volume_render_icon;
  iface->get_parent = gtk_file_system_win32_get_parent;
  iface->make_path = gtk_file_system_win32_make_path;
  iface->parse = gtk_file_system_win32_parse;
  iface->path_to_uri = gtk_file_system_win32_path_to_uri;
  iface->path_to_filename = gtk_file_system_win32_path_to_filename;
  iface->uri_to_path = gtk_file_system_win32_uri_to_path;
  iface->filename_to_path = gtk_file_system_win32_filename_to_path;
  iface->render_icon = gtk_file_system_win32_render_icon;
  iface->insert_bookmark = gtk_file_system_win32_insert_bookmark;
  iface->remove_bookmark = gtk_file_system_win32_remove_bookmark;
  iface->list_bookmarks = gtk_file_system_win32_list_bookmarks;
}

static void
gtk_file_system_win32_init (GtkFileSystemWin32 *system_win32)
{
  system_win32->folder_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gtk_file_system_win32_finalize (GObject *object)
{
  GtkFileSystemWin32 *system_win32;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (object);

  /* FIXME: assert that the hash is empty? */
  g_hash_table_destroy (system_win32->folder_hash);

  system_parent_class->finalize (object);
}

static GSList *
gtk_file_system_win32_list_volumes (GtkFileSystem *file_system)
{
  DWORD   drives;
  gchar   drive[4] = "A:\\";
  GSList *list = NULL;

  drives = GetLogicalDrives();

  if (!drives)
    g_warning ("GetLogicalDrives failed.");

  while (drives && drive[0] <= 'Z')
    {
      if (drives & 1)
      {
	GtkFileSystemVolume *vol = g_new0 (GtkFileSystemVolume, 1);
	if (drive[0] == 'A' || drive[0] == 'B')
	  vol->is_mounted = FALSE; /* skip floppy */
	else
	  vol->is_mounted = TRUE; /* handle other removable drives special, too? */

	vol->drive = g_strdup (drive);
	list = g_slist_append (list, vol);
      }
      drives >>= 1;
      drive[0]++;
    }
  return list;
}

static GtkFileSystemVolume *
gtk_file_system_win32_get_volume_for_path (GtkFileSystem     *file_system,
                                           const GtkFilePath *path)
{
  GtkFileSystemVolume *vol = g_new0 (GtkFileSystemVolume, 1);
  gchar* p = g_strndup (gtk_file_path_get_string (path), 3);

  g_return_val_if_fail (p != NULL, NULL);

  /*FIXME: gtk_file_path_compare() is case sensitive, we are not*/
  p[0] = g_ascii_toupper (p[0]);
  vol->drive = p;
  vol->is_mounted = (p[0] != 'A' && p[0] != 'B');

  return vol;
}

static GtkFileFolder *
gtk_file_system_win32_get_folder (GtkFileSystem    *file_system,
				 const GtkFilePath *path,
				 GtkFileInfoType    types,
				 GError           **error)
{
  GtkFileSystemWin32 *system_win32;
  GtkFileFolderWin32 *folder_win32;
  gchar *filename;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (file_system);

  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, NULL);

  folder_win32 = g_hash_table_lookup (system_win32->folder_hash, filename);

  if (folder_win32)
    return g_object_ref (folder_win32);

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
      return NULL;
    }

  folder_win32 = g_object_new (GTK_TYPE_FILE_FOLDER_WIN32, NULL);
  folder_win32->system_win32 = system_win32;
  folder_win32->filename = filename;
  folder_win32->types = types;

  g_hash_table_insert (system_win32->folder_hash, folder_win32->filename, folder_win32);

  return GTK_FILE_FOLDER (folder_win32);
}

static gboolean
gtk_file_system_win32_create_folder (GtkFileSystem     *file_system,
				     const GtkFilePath *path,
				     GError           **error)
{
  GtkFileSystemWin32 *system_win32;
  gchar *filename;
  gboolean result;
  char *parent;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (file_system);

  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, FALSE);

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
  else if (!filename_is_root (filename))
    {
      parent = g_path_get_dirname (filename);
      if (parent)
	{
	  GtkFileFolderWin32 *folder_win32;

	  folder_win32 = g_hash_table_lookup (system_win32->folder_hash, parent);
	  if (folder_win32)
	    {
	      GSList *paths;

	      paths = g_slist_append (NULL, (GtkFilePath *) path);
	      g_signal_emit_by_name (folder_win32, "files-added", paths);
	      g_slist_free (paths);
	    }
	  g_free(parent);
	}
    }

  g_free (filename);

  return result;
}

static void
gtk_file_system_win32_volume_free (GtkFileSystem        *file_system,
				   GtkFileSystemVolume  *volume)
{
  g_free (volume->drive);
  g_free (volume);
}

static GtkFilePath *
gtk_file_system_win32_volume_get_base_path (GtkFileSystem        *file_system,
					    GtkFileSystemVolume  *volume)
{
  return (GtkFilePath *) g_strdup (volume->drive);
}

static gboolean
gtk_file_system_win32_volume_get_is_mounted (GtkFileSystem        *file_system,
					     GtkFileSystemVolume  *volume)
{
  return volume->is_mounted;
}

static gboolean
gtk_file_system_win32_volume_mount (GtkFileSystem        *file_system, 
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
gtk_file_system_win32_volume_get_display_name (GtkFileSystem       *file_system,
					      GtkFileSystemVolume *volume)
{
  gchar *real_display_name;
  gunichar2 *wdrive = g_utf8_to_utf16 (volume->drive, -1, NULL, NULL, NULL);
  gunichar2  wname[80];

  g_return_val_if_fail (wdrive != NULL, NULL);

  if (GetVolumeInformationW (wdrive,
			    wname, G_N_ELEMENTS(wname), 
                            NULL, /* serial number */
                            NULL, /* max. component length */
                            NULL, /* fs flags */
                            NULL, 0) /* fs type like FAT, NTFS */
			    && wname[0])
    {
      gchar *name = g_utf16_to_utf8 (wname, -1, NULL, NULL, NULL);
      real_display_name = g_strconcat (name, " (", volume->drive, ")", NULL);
      g_free (name);
    }
  else
    real_display_name = g_strdup (volume->drive);

  g_free (wdrive);

  return real_display_name;
}

static GdkPixbuf *
gtk_file_system_win32_volume_render_icon (GtkFileSystem        *file_system,
					 GtkFileSystemVolume  *volume,
					 GtkWidget            *widget,
					 gint                  pixel_size,
					 GError              **error)
{
  GtkIconSet *icon_set = NULL;
  DWORD dt = GetDriveType (volume->drive);

  switch (dt)
    {
    case DRIVE_REMOVABLE :
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_FLOPPY);
      break;
    case DRIVE_CDROM :
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_CDROM);
      break;
    case DRIVE_REMOTE :
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_NETWORK);
      break;
    case DRIVE_FIXED :
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_HARDDISK);
      break;
    case DRIVE_RAMDISK :
      /*FIXME: need a ram stock icon
      gtk_file_info_set_icon_type (info, GTK_STOCK_OPEN);*/
      break;
    default :
      g_assert_not_reached ();
    }

  return gtk_icon_set_render_icon (icon_set, 
                                   widget->style,
                                   gtk_widget_get_direction (widget),
                                   GTK_STATE_NORMAL,
                                   GTK_ICON_SIZE_BUTTON,
                                   widget, NULL); 
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
gtk_file_system_win32_parse (GtkFileSystem     *file_system,
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
                     gint      position,
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
      GSList *entry;
      FILE  *f;   
       
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          if (g_file_get_contents (filename, &contents, &len, error))
	    {
	      gchar **lines = g_strsplit (contents, "\n", -1);
	      gint    i;

	      for (i = 0; lines[i] != NULL; i++)
		{
		  if (lines[i][0] && !g_slist_find_custom (list, lines[i], (GCompareFunc) strcmp))
		    list = g_slist_append (list, g_strdup (lines[i]));
		}
	      g_strfreev (lines);
	    }
	  else
	    ok = FALSE;
	}
      if (ok && (f = fopen (filename, "wb")) != NULL)
        {
	  entry = g_slist_find_custom (list, uri, (GCompareFunc) strcmp);
	  if (add)
	    {
	      /* g_slist_insert() and our insert semantics are 
	       * compatible, but maybe we should check for 
	       * positon > length ?
	       * 
	       */
	      if (!entry)
                list = g_slist_insert (list, g_strdup (uri), position);
	      else
	        {
		  g_set_error (error,
			       GTK_FILE_SYSTEM_ERROR,
			       GTK_FILE_SYSTEM_ERROR_ALREADY_EXISTS,
			       "%s already exists in the bookmarks list",
			       uri);
		  ok = FALSE;
		}
	    }
	  else
	    {
	      /* to remove the given uri */
	      if (entry)
		list = g_slist_delete_link(list, entry);
	      for (entry = list; entry != NULL; entry = entry->next)
		{
		  fputs (entry->data, f);
		  fputs ("\n", f);
		}
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

static GdkPixbuf*
extract_icon (const char* filename)
{
  GdkPixbuf *pixbuf = NULL;
  WORD iicon;
  HICON hicon;
  
  if (!filename || !filename[0])
    return NULL;

  hicon = ExtractAssociatedIcon (GetModuleHandle (NULL), filename, &iicon);
  if (hicon > (HICON)1)
    {
      ICONINFO ii;

      if (GetIconInfo (hicon, &ii))
        {
          SIZE   size;
          GdkPixmap *pixmap;
          GdkGC *gc;
          HDC    hdc;

          if (!GetBitmapDimensionEx (ii.hbmColor, &size))
            g_warning ("GetBitmapDimensionEx failed.");

	  if (size.cx < 1) size.cx = 32;
	  if (size.cy < 1) size.cy = 32;
	    
          pixmap = gdk_pixmap_new (NULL, size.cx, size.cy, 
	                           gdk_screen_get_system_visual (gdk_screen_get_default ())->depth);
          gc = gdk_gc_new (pixmap);
          hdc = gdk_win32_hdc_get (GDK_DRAWABLE (pixmap), gc, 0);

          if (!DrawIcon (hdc, 0, 0, hicon))
            g_warning ("DrawIcon failed");

          gdk_win32_hdc_release (GDK_DRAWABLE (pixmap), gc, 0);

          pixbuf = gdk_pixbuf_get_from_drawable (
		     NULL, pixmap, 
		     gdk_screen_get_system_colormap (gdk_screen_get_default ()),
		     0, 0, 0, 0, size.cx, size.cy);
          g_object_unref (pixmap);
          g_object_unref (gc);
        }
      else
        g_print ("GetIconInfo failed: %s\n", g_win32_error_message (GetLastError ())); 

      if (!DestroyIcon (hicon))
        g_warning ("DestroyIcon failed");
    }
  else
    g_print ("ExtractAssociatedIcon failed: %s\n", g_win32_error_message (GetLastError ()));

  return pixbuf;
}

static GtkIconSet *
win32_pseudo_mime_lookup (const char* name)
{
  static GHashTable *mime_hash = NULL;
  GtkIconSet *is = NULL;
  char *p = strrchr(name, '.');
  char *extension = p ? g_ascii_strdown (p, -1) : g_strdup ("");

  if (!mime_hash)
    mime_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* do we already have it ? */
  is = g_hash_table_lookup (mime_hash, extension);
  if (is)
    {
      g_free (extension);
      return is;
    }
  /* create icon and set */
  {
    GdkPixbuf *pixbuf = extract_icon (name);
    if (pixbuf)
      {
        GtkIconSource* source = gtk_icon_source_new ();

        is = gtk_icon_set_new_from_pixbuf (pixbuf);
	gtk_icon_source_set_pixbuf (source, pixbuf);
	gtk_icon_set_add_source (is, source);

	gtk_icon_source_free (source);
      }

    g_hash_table_insert (mime_hash, extension, is);
    return is;
  }
}

static GdkPixbuf *
gtk_file_system_win32_render_icon (GtkFileSystem     *file_system,
                                   const GtkFilePath *path,
                                   GtkWidget         *widget,
                                   gint               pixel_size,
                                   GError           **error)
{
  GtkIconSet *icon_set = NULL;
  const char* filename = gtk_file_path_get_string (path);

  /* handle drives with stock icons */
  if (filename_is_root (filename))
    {
      gchar *filename2 = g_strconcat(filename, "\\", NULL);
      DWORD dt = GetDriveType (filename2);

      switch (dt)
        {
        case DRIVE_REMOVABLE :
          icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_FLOPPY);
          break;
        case DRIVE_CDROM :
          icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_CDROM);
          break;
        case DRIVE_FIXED : /* need a hard disk icon */
          icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_HARDDISK);
          break;
        default :
          break;
        }
      g_free (filename2);
    }
  else if (g_file_test (filename, G_FILE_TEST_IS_DIR))
    {
      if (0 == strcmp (g_get_home_dir(), filename))
        icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_HOME);
      else
        icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_OPEN);
    }
  else if (g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE))
    {
      /* don't lookup all executable icons */
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_EXECUTE);
    }
  else if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      icon_set = win32_pseudo_mime_lookup (filename);
    }

  if (!icon_set)
    {
       g_set_error (error,
     	          GTK_FILE_SYSTEM_ERROR,
     	          GTK_FILE_SYSTEM_ERROR_FAILED,
     	          _("This file system does not support icons for everything"));
       return NULL;
    }

  // FIXME : I'd like to get from pixel_size (=20) back to
  // icon size, which is an index, but there appears to be no way ?
  return gtk_icon_set_render_icon (icon_set, 
                                   widget->style,
                                   gtk_widget_get_direction (widget),
                                   GTK_STATE_NORMAL,
                                   GTK_ICON_SIZE_BUTTON,
				   widget, NULL); 
}

static GSList *_bookmarks = NULL;

static gboolean
gtk_file_system_win32_insert_bookmark (GtkFileSystem     *file_system,
				    const GtkFilePath *path,
				    gint               position,
				    GError           **error)
{
  gchar *uri = gtk_file_system_win32_path_to_uri (file_system, path);
  gboolean ret = bookmarks_serialize (&_bookmarks, uri, TRUE, position, error);
  if (ret)
    g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
  g_free (uri);
  return ret;
                               
}

static gboolean
gtk_file_system_win32_remove_bookmark (GtkFileSystem     *file_system,
				       const GtkFilePath *path,
				       GError           **error)
{
  gchar *uri = gtk_file_system_win32_path_to_uri (file_system, path);
  gboolean ret = bookmarks_serialize (&_bookmarks, uri, FALSE, 0, error);
  if (ret)
    g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
  g_free (uri);
  return ret;
}

static GSList *
gtk_file_system_win32_list_bookmarks (GtkFileSystem *file_system)
{
  GSList *list = NULL;
  GSList *entry;


  if (bookmarks_serialize (&_bookmarks, "", FALSE, 0, NULL))
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

  g_hash_table_remove (folder_win32->system_win32->folder_hash, folder_win32->filename);

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
  
  if (!path)
    {
      g_return_val_if_fail (filename_is_root (folder_win32->filename), NULL);

      /* ??? */
      info = filename_get_info (folder_win32->filename, folder_win32->types, error);

      return info;
    }

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
#if 0 /* it's dead in GtkFileSystemUnix.c, too */
  GtkFileIconType icon_type = GTK_FILE_ICON_REGULAR;
#endif
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

#if 0 /* it's dead in GtkFileSystemUnix.c, too */
  if (types & GTK_FILE_INFO_ICON)
    {
      if (wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	icon_type = GTK_FILE_ICON_DIRECTORY;

      gtk_file_info_set_icon_type (info, icon_type);
    }
#endif

  if ((types & GTK_FILE_INFO_MIME_TYPE)
#if 0 /* it's dead in GtkFileSystemUnix.c, too */
      || ((types & GTK_FILE_INFO_ICON) && icon_type == GTK_FILE_ICON_REGULAR)
#endif
     )
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
      time /= G_GINT64_CONSTANT (10000000); /* now seconds */
      time -= G_GINT64_CONSTANT (134774) * 24 * 3600; /* good old Unix time */
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
          || (len == 3 && filename[1] == ':' && (filename[2] == '\\' || filename[2] == '/')));
}

