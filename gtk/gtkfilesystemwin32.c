/* GTK - The GIMP Toolkit
 * gtkfilesystemwin32.c: Default implementation of GtkFileSystem for Windows
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2004, Hans Breuer
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
#include "gtkalias.h"

#include <glib/gstdio.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#undef STRICT
#include <shlobj.h>
#include <shellapi.h>
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

  guint32 drives;		/* bitmask as returned by GetLogicalDrives() */
  GHashTable *folder_hash;
  guint timeout;
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

static gboolean       filename_is_drive_root                 (const char               *filename);
static gboolean       filename_is_server_share               (const char               *filename);
static gboolean       filename_is_some_root                  (const char               *filename);
static GtkFileInfo *  filename_get_info                      (const gchar              *filename,
							      GtkFileInfoType           types,
							      GError                  **error);

/* some info kept together for volumes */
struct _GtkFileSystemVolume
{
  gchar *drive;
  int drive_type;
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

static gboolean
check_volumes (gpointer data)
{
  GtkFileSystemWin32 *system_win32 = GTK_FILE_SYSTEM_WIN32 (data);

  g_return_val_if_fail (system_win32, FALSE);

#if 0
  printf("check_volumes: system_win32=%p\n", system_win32);
#endif
  if (system_win32->drives != GetLogicalDrives())
    g_signal_emit_by_name (system_win32, "volumes-changed", 0);

  return TRUE;
}

static void
gtk_file_system_win32_init (GtkFileSystemWin32 *system_win32)
{
#if 0
  printf("gtk_file_system_win32_init: %p\n", system_win32);
#endif

  /* set up an idle handler for volume changes, every second should be enough */
  system_win32->timeout = g_timeout_add_full (0, 1000, check_volumes, system_win32, NULL);

  system_win32->folder_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gtk_file_system_win32_finalize (GObject *object)
{
  GtkFileSystemWin32 *system_win32;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (object);

#if 0
  printf("gtk_file_system_win32_finalize: %p\n", system_win32);
#endif

  g_source_remove (system_win32->timeout);

  /* FIXME: assert that the hash is empty? */
  g_hash_table_destroy (system_win32->folder_hash);

  system_parent_class->finalize (object);
}

/* Lifted from GLib */

static gchar *
get_special_folder (int csidl)
{
  union {
    char c[MAX_PATH+1];
    wchar_t wc[MAX_PATH+1];
  } path;
  HRESULT hr;
  LPITEMIDLIST pidl = NULL;
  BOOL b;
  gchar *retval = NULL;

  hr = SHGetSpecialFolderLocation (NULL, csidl, &pidl);
  if (hr == S_OK)
    {
      if (G_WIN32_HAVE_WIDECHAR_API ())
	{
	  b = SHGetPathFromIDListW (pidl, path.wc);
	  if (b)
	    retval = g_utf16_to_utf8 (path.wc, -1, NULL, NULL, NULL);
	}
      else
	{
	  b = SHGetPathFromIDListA (pidl, path.c);
	  if (b)
	    retval = g_locale_to_utf8 (path.c, -1, NULL, NULL, NULL);
	}
      CoTaskMemFree (pidl);
    }
  return retval;
}

gchar *
_gtk_file_system_win32_get_desktop (void)
{
  return get_special_folder (CSIDL_DESKTOPDIRECTORY);
}

static GSList *
gtk_file_system_win32_list_volumes (GtkFileSystem *file_system)
{
  DWORD   drives;
  gchar   drive[4] = "A:\\";
  GSList *list = NULL;
  GtkFileSystemWin32 *system_win32 = (GtkFileSystemWin32 *)file_system;

  drives = GetLogicalDrives();

  system_win32->drives = drives;
  if (!drives)
    g_warning ("GetLogicalDrives failed.");

  while (drives && drive[0] <= 'Z')
    {
      if (drives & 1)
      {
	GtkFileSystemVolume *vol = g_new0 (GtkFileSystemVolume, 1);
	vol->drive = g_strdup (drive);
	vol->drive_type = GetDriveType (drive);
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
  const gchar *p;

  g_return_val_if_fail (path != NULL, NULL);

  p = gtk_file_path_get_string (path);

  if (!g_path_is_absolute (p))
    {
      if (g_ascii_isalpha (p[0]) && p[1] == ':')
	vol->drive = g_strdup_printf ("%c:\\", p[0]);
      else
	vol->drive = g_strdup ("\\");
      vol->drive_type = GetDriveType (vol->drive);
    }
  else
    {
      const gchar *q = g_path_skip_root (p);
      vol->drive = g_strndup (p, q - p);
      if (!G_IS_DIR_SEPARATOR (q[-1]))
	{
	  /* Make sure "drive" always ends in a slash */
	  gchar *tem = vol->drive;
	  vol->drive = g_strconcat (vol->drive, "\\", NULL);
	  g_free (tem);
	}
      
      if (filename_is_drive_root (vol->drive))
	{
	  vol->drive[0] = g_ascii_toupper (vol->drive[0]);
	  vol->drive_type = GetDriveType (vol->drive);
	}
      else if (G_WIN32_HAVE_WIDECHAR_API ())
	{
	  wchar_t *wdrive = g_utf8_to_utf16 (vol->drive, -1, NULL, NULL, NULL);
	  vol->drive_type = GetDriveTypeW (wdrive);
	  g_free (wdrive);
	}
      else
	{
	  gchar *cpdrive = g_locale_from_utf8 (vol->drive, -1, NULL, NULL, NULL);
	  vol->drive_type = GetDriveTypeA (cpdrive);
	  g_free (cpdrive);
	}
    }
  return vol;
}

static GtkFileFolder *
gtk_file_system_win32_get_folder (GtkFileSystem     *file_system,
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

  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      gchar *display_filename = g_filename_display_name (filename);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("Error getting information for '%s': %s"),
		   display_filename,
		   g_strerror (ENOENT));
      g_free (display_filename);
      return NULL;
    }
  if (!g_file_test (filename, G_FILE_TEST_IS_DIR))
    {
      gchar *display_filename = g_filename_display_name (filename);

      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NOT_FOLDER,
		   _("Error getting information for '%s': %s"),
		   display_filename,
		   g_strerror (ENOTDIR));
      g_free (display_filename);
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
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  result = g_mkdir (filename, 0777) == 0;

  if (!result)
    {
      int save_errno = errno;
      gchar *display_filename = g_filename_display_name (filename);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("Error creating directory '%s': %s"),
		   display_filename,
		   g_strerror (save_errno));
      g_free (display_filename);
    }
  else if (!filename_is_some_root (filename))
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
  return TRUE;
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

  g_return_val_if_fail (volume->drive != NULL, NULL);

  if (filename_is_drive_root (volume->drive) &&
      volume->drive_type == DRIVE_REMOTE)
    real_display_name = g_strdup (volume->drive);
  else if ((filename_is_drive_root (volume->drive) && volume->drive[0] >= 'C') ||
      volume->drive_type != DRIVE_REMOVABLE)
    {
      gchar *name = NULL;
      if (G_WIN32_HAVE_WIDECHAR_API ())
	{
	  gunichar2 *wdrive = g_utf8_to_utf16 (volume->drive, -1, NULL, NULL, NULL);
	  gunichar2 wname[80];
	  if (GetVolumeInformationW (wdrive,
				     wname, G_N_ELEMENTS(wname), 
				     NULL, /* serial number */
				     NULL, /* max. component length */
				     NULL, /* fs flags */
				     NULL, 0) /* fs type like FAT, NTFS */ &&
	      wname[0])
	    {
	      name = g_utf16_to_utf8 (wname, -1, NULL, NULL, NULL);
	    }
	  g_free (wdrive);
	}
      else
        {
          gchar *cpdrive = g_locale_from_utf8 (volume->drive, -1, NULL, NULL, NULL);
          gchar cpname[80];
          if (GetVolumeInformationA (cpdrive,
				     cpname, G_N_ELEMENTS(cpname), 
				     NULL, /* serial number */
				     NULL, /* max. component length */
				     NULL, /* fs flags */
				     NULL, 0) /* fs type like FAT, NTFS */ &&
	      cpname[0])
            {
              name = g_locale_to_utf8 (cpname, -1, NULL, NULL, NULL);
            }
          g_free (cpdrive);
        }
      if (name != NULL)
        {
	  real_display_name = g_strconcat (name, " (", volume->drive, ")", NULL);
	  g_free (name);
	}
      else
	{
	  real_display_name = g_strdup (volume->drive);
	}
    }
  else
    real_display_name = g_strdup (volume->drive);

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

  switch (volume->drive_type)
    {
    case DRIVE_REMOVABLE:
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_FLOPPY);
      break;
    case DRIVE_CDROM:
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_CDROM);
      break;
    case DRIVE_REMOTE:
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_NETWORK);
      break;
    case DRIVE_FIXED:
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_HARDDISK);
      break;
    case DRIVE_RAMDISK:
#if 0
      /*FIXME: need a ram stock icon?? */
      gtk_file_info_set_icon_type (info, GTK_STOCK_RAMDISK);
      break;
#endif
    default :
      /* Use network icon as a guess */ 
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_NETWORK);
      break;
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
  const char *filename;

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  if (filename_is_some_root (filename))
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
gtk_file_system_win32_make_path (GtkFileSystem     *file_system,
			         const GtkFilePath *base_path,
			         const gchar       *display_name,
			         GError           **error)
{
  const char *base_filename;
  gchar *full_filename;
  GtkFilePath *result;
  
  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (base_filename), NULL);

  full_filename = g_build_filename (base_filename, display_name, NULL);
  result = filename_to_path (full_filename);
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
  gchar *past_root;
  gboolean last_was_slash = FALSE;

#if 0
  printf("canonicalize_filename: %s ", filename);
#endif

  past_root = (gchar *) g_path_skip_root (filename);

  q = p = past_root;

  while (*p)
    {
      if (G_IS_DIR_SEPARATOR (*p))
	{
	  if (!last_was_slash)
	    *q++ = G_DIR_SEPARATOR;

	  last_was_slash = TRUE;
	}
      else
	{
	  if (last_was_slash && *p == '.')
	    {
	      if (G_IS_DIR_SEPARATOR (*(p + 1)) ||
		  *(p + 1) == '\0')
		{
		  if (*(p + 1) == '\0')
		    break;
		  
		  p += 1;
		}
	      else if (*(p + 1) == '.' &&
		       (G_IS_DIR_SEPARATOR (*(p + 2)) ||
			*(p + 2) == '\0'))
		{
		  if (q > past_root)
		    {
		      q--;
		      while (q > past_root &&
			     !G_IS_DIR_SEPARATOR (*(q - 1)))
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

  if (q > past_root && G_IS_DIR_SEPARATOR (*(q - 1)))
    q--;

  *q = '\0';
#if 0
  printf(" => %s\n", filename);
#endif
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
  gchar *last_backslash, *last_slash;
  gboolean result = FALSE;

#if 0
  printf("gtk_file_system_win32_parse: base_path=%s str=%s\n",(char*)base_path,str);
#endif

  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (base_filename), FALSE);
  
  last_backslash = strrchr (str, G_DIR_SEPARATOR);
  last_slash = strrchr (str, '/');
  if (last_slash == NULL ||
      (last_backslash != NULL && last_backslash > last_slash))
    last_slash = last_backslash;

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

      if (last_slash == str)
	{
	  if (g_ascii_isalpha (base_filename[0]) &&
	      base_filename[1] == ':')
	    folder_part = g_strdup_printf ("%c:" G_DIR_SEPARATOR_S, base_filename[0]);
	  else
	    folder_part = g_strdup (G_DIR_SEPARATOR_S);
	}
      else if (g_ascii_isalpha (str[0]) &&
	       str[1] == ':' &&
	       G_IS_DIR_SEPARATOR (str[2]))
	folder_part = g_strndup (str, last_slash - str + 1);
      else if (G_IS_DIR_SEPARATOR (str[0]) &&
	       G_IS_DIR_SEPARATOR (str[1]) &&
	       (!str[2] || !G_IS_DIR_SEPARATOR (str[2])))
	folder_part = g_strdup (str);
      else
	folder_part = g_strndup (str, last_slash - str);

      g_assert (folder_part);

      if (g_path_is_absolute (folder_part))
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

#if 0
  printf("gtk_file_system_win32_parse:returning folder=%s file_part=%s\n",(*folder?(char*)*folder:"NULL"),*file_part);
#endif

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
  GtkFilePath *path = NULL;
  gchar *filename = g_filename_from_uri (uri, NULL, NULL);
  if (filename)
    {
      path = filename_to_path (filename);
      g_free (filename);
    }

  return path;
}

static GtkFilePath *
gtk_file_system_win32_filename_to_path (GtkFileSystem *file_system,
				        const gchar   *filename)
{
  return filename_to_path (filename);
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
		  if (lines[i][0] && !g_slist_find_custom (list, lines[i], (GCompareFunc) _gtk_file_system_win32_path_compare))
		    list = g_slist_append (list, g_strdup (lines[i]));
		}
	      g_strfreev (lines);
	    }
	  else
	    ok = FALSE;
	}
      if (ok && (f = g_fopen (filename, "wb")) != NULL)
        {
	  entry = g_slist_find_custom (list, uri, (GCompareFunc) _gtk_file_system_win32_path_compare);
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
			       "'%s' already exists in the bookmarks list",
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
		       _("Bookmark saving failed: %s"),
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
  HICON hicon;
  ICONINFO ii;
  
  if (!filename || !filename[0])
    return NULL;

#if 0
  /* ExtractAssociatedIconW() is about twice as slow as SHGetFileInfoW() */

  /* The ugly ExtractAssociatedIcon modifies filename in place. It
   * doesn't even take any argument saying how large the buffer is?
   * Let's hope MAX_PATH will be large enough. What dork designed that
   * API?
   */
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      WORD iicon;
      wchar_t *wfn;
      wchar_t filename_copy[MAX_PATH];

      wfn = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      if (wcslen (wfn) >= MAX_PATH)
	{
	  g_free (wfn);
	  return NULL;
	}
      wcscpy (filename_copy, wfn);
      g_free (wfn);
      hicon = ExtractAssociatedIconW (GetModuleHandle (NULL), filename_copy, &iicon);
    }
  else
    {
      WORD iicon;
      char *cpfn;
      char filename_copy[MAX_PATH];

      cpfn = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      if (cpfn == NULL)
	return NULL;
      if (strlen (cpfn) >= MAX_PATH)
	{
	  g_free (cpfn);
	  return NULL;
	}
      strcpy (filename_copy, cpfn);
      g_free (cpfn);
      hicon = ExtractAssociatedIconA (GetModuleHandle (NULL), filename_copy, &iicon);
    }

  if (!hicon)
    {
      g_warning (G_STRLOC ":ExtractAssociatedIcon(%s) failed: %s", filename, g_win32_error_message (GetLastError ()));
      return NULL;
    }

#else
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      SHFILEINFOW shfi;
      wchar_t *wfn = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int rc;

      rc = (int) SHGetFileInfoW (wfn, 0, &shfi, sizeof (shfi),
				 SHGFI_ICON|SHGFI_LARGEICON);
      g_free (wfn);
      if (!rc)
	return NULL;
      hicon = shfi.hIcon;
    }
  else
    {
      SHFILEINFOA shfi;
      char *cpfn = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int rc;

      rc = (int) SHGetFileInfoA (cpfn, 0, &shfi, sizeof (shfi),
				 SHGFI_ICON|SHGFI_LARGEICON);
      g_free (cpfn);
      if (!rc)
	return NULL;
      hicon = shfi.hIcon;
    }
#endif
  
  if (GetIconInfo (hicon, &ii))
    {
      struct
      {
	BITMAPINFOHEADER bi;
	RGBQUAD colors[2];
      } bmi;
      HDC hdc;
      
      memset (&bmi, 0, sizeof (bmi));
      bmi.bi.biSize = sizeof (bmi.bi);
      hdc = CreateCompatibleDC (NULL);
      
      if (GetDIBits (hdc, ii.hbmColor, 0, 1, NULL, (BITMAPINFO *)&bmi, DIB_RGB_COLORS))
	{
	  gchar *pixels, *bits;
	  gint rowstride, x, y, w = bmi.bi.biWidth, h = bmi.bi.biHeight;
	  gboolean no_alpha;
	  
	  bmi.bi.biBitCount = 32;
	  bmi.bi.biCompression = BI_RGB;
	  bmi.bi.biHeight = -h;
	  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
	  bits = g_malloc0 (4 * w * h);
	  
	  /* color data */
	  if (!GetDIBits (hdc, ii.hbmColor, 0, h, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS))
	    g_warning (G_STRLOC ": Failed to get dibits");
	  
	  pixels = gdk_pixbuf_get_pixels (pixbuf);
	  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	  no_alpha = TRUE;
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  pixels[2] = bits[(x+y*w) * 4];
		  pixels[1] = bits[(x+y*w) * 4 + 1];
		  pixels[0] = bits[(x+y*w) * 4 + 2];
		  pixels[3] = bits[(x+y*w) * 4 + 3];
		  if (no_alpha && pixels[3] > 0) no_alpha = FALSE;
		  pixels += 4;
		}
	      pixels += (w * 4 - rowstride);
	    }
	  /* mask */
	  if (no_alpha) {
	    if (!GetDIBits (hdc, ii.hbmMask, 0, h, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS))
	      g_warning (G_STRLOC ": Failed to get dibits");
	    pixels = gdk_pixbuf_get_pixels (pixbuf);
	    for (y = 0; y < h; y++)
	      {
		for (x = 0; x < w; x++)
		  {
		    pixels[3] = 255 - bits[(x + y * w) * 4];
		    pixels += 4;
		  }
		pixels += (w * 4 - rowstride);
	      }
	    
	    /* release temporary resources */
	    g_free (bits);
	    if (!DeleteObject (ii.hbmColor) || !DeleteObject (ii.hbmMask))
	      g_warning (G_STRLOC ": Leaking Icon Bitmaps ?");
	  }
	}
      else
	g_warning (G_STRLOC ": GetDIBits () failed, %s", g_win32_error_message (GetLastError ()));
      
      DeleteDC (hdc);
    }
  else
    g_warning (G_STRLOC ": GetIconInfo failed: %s\n", g_win32_error_message (GetLastError ())); 
  
  if (!DestroyIcon (hicon))
    g_warning (G_STRLOC ": DestroyIcon failed: %s\n", g_win32_error_message (GetLastError ()));

  return pixbuf;
}

static GtkIconSet *
win32_pseudo_mime_lookup (const char* name)
{
  gboolean use_cache = TRUE;
  static GHashTable *mime_hash = NULL;
  GtkIconSet *is = NULL;
  char *p = strrchr(name, '.');
  char *extension = p ? g_utf8_casefold (p, -1) : g_strdup ("");
  GdkPixbuf *pixbuf;

  /* Don't cache icons for files that might have embedded icons */
  if (strcmp (extension, ".lnk") == 0 ||
      strcmp (extension, ".exe") == 0 ||
      strcmp (extension, ".dll") == 0)
    {
      use_cache = FALSE;
      g_free (extension);
    }
  else
    {
      if (!mime_hash)
	mime_hash = g_hash_table_new (g_str_hash, g_str_equal);

      /* do we already have it ? */
      is = g_hash_table_lookup (mime_hash, extension);
      if (is)
	{
	  g_free (extension);
	  return is;
	}
    }

  /* create icon and set */
  pixbuf = extract_icon (name);
  if (pixbuf)
    {
      GtkIconSource* source = gtk_icon_source_new ();
      
      is = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_source_set_pixbuf (source, pixbuf);
      gtk_icon_set_add_source (is, source);
      
      gtk_icon_source_free (source);
    }
  
  if (use_cache)
    g_hash_table_insert (mime_hash, extension, is);

  return is;
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
  if (filename_is_drive_root (filename))
    {
      gchar filename2[] = "?:\\";
      DWORD dt;

      filename2[0] = filename[0];
      dt = GetDriveType (filename2);

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
        default :
          break;
        }
    }
  else if (filename_is_server_share (filename))
    {
      icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_NETWORK);
    }
  else if (g_file_test (filename, G_FILE_TEST_IS_DIR))
    {
      const gchar *home = g_get_home_dir ();

      if (home != NULL && 0 == _gtk_file_system_win32_path_compare (home, filename))
        icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_HOME);
      else
        icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_DIRECTORY);
    }
  else if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      icon_set = win32_pseudo_mime_lookup (filename);
    }

  if (!icon_set)
    {
      if (g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE))
        icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_EXECUTE);
      else
        icon_set = gtk_style_lookup_icon_set (widget->style, GTK_STOCK_FILE);
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
				       GError            **error)
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
  gchar *filename;
  
  if (!path)
    {
      g_return_val_if_fail (filename_is_some_root (folder_win32->filename), NULL);

      /* ??? */
      info = filename_get_info (folder_win32->filename, folder_win32->types, error);

      return info;
    }

  filename = filename_from_path (path);
  g_return_val_if_fail (filename != NULL, NULL);

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
  GDir *dir;

  *children = NULL;

  dir = g_dir_open (folder_win32->filename, 0, error);
  if (!dir)
    return FALSE;

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
#if 0 /* it's dead in gtkfilesystemunix.c, too */
  GtkFileIconType icon_type = GTK_FILE_ICON_REGULAR;
#endif
  WIN32_FILE_ATTRIBUTE_DATA wfad;
  int rc = 0;

  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, error);

      if (!wfilename)
	return NULL;

      rc = GetFileAttributesExW (wfilename, GetFileExInfoStandard, &wfad);
      g_free (wfilename);
    }
  else
    {
      char *cpfilename = g_locale_from_utf8 (filename, -1, NULL, NULL, error);

      if (!cpfilename)
	return NULL;

      rc = GetFileAttributesExA (cpfilename, GetFileExInfoStandard, &wfad);
      g_free (cpfilename);
    }

  if (!rc)
    {
      gchar *display_filename = g_filename_display_name (filename);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("Error getting information for '%s': %s"),
		   display_filename,
		   g_win32_error_message (GetLastError ()));
      g_free (display_filename);
      
      return NULL;
    }

  info = gtk_file_info_new ();

  if (filename_is_some_root (filename))
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
	  gchar *display_basename = g_filename_display_name (basename);
	  
	  gtk_file_info_set_display_name (info, display_basename);
	  g_free (display_basename);
	}
      
      if (types & GTK_FILE_INFO_IS_HIDDEN)
	{
	  /* Unix dot convention or the Windows hidden attribute */
	  gboolean is_hidden = basename[0] == '.' ||
	    !!(wfad.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
	  gtk_file_info_set_is_hidden (info, is_hidden);
	}

      g_free (basename);
    }

  if (types & GTK_FILE_INFO_IS_FOLDER)
    {
      gtk_file_info_set_is_folder (info, !!(wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
   }

#if 0 /* it's dead in gtkfilesystemunix.c, too */
  if (types & GTK_FILE_INFO_ICON)
    {
      if (wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	icon_type = GTK_FILE_ICON_DIRECTORY;

      gtk_file_info_set_icon_type (info, icon_type);
    }
#endif

  if (types & GTK_FILE_INFO_MIME_TYPE)
    {
      if (g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE))
	gtk_file_info_set_mime_type (info, "application/x-executable");
      else
	{
	  const char *extension = strrchr (filename, '.');
	  HKEY key = NULL;
	  DWORD type, nbytes = 0;
	  char *value;

	  if (extension != NULL &&
	      extension[1] != '\0' &&
	      RegOpenKeyEx (HKEY_CLASSES_ROOT, extension, 0,
			    KEY_QUERY_VALUE, &key) == ERROR_SUCCESS &&
	      RegQueryValueEx (key, "Content Type", 0,
			       &type, NULL, &nbytes) == ERROR_SUCCESS &&
	      type == REG_SZ &&
	      (value = g_try_malloc (nbytes + 1)) &&
	      RegQueryValueEx (key, "Content Type", 0,
			       &type, value, &nbytes) == ERROR_SUCCESS)
	    {
	      value[nbytes] = '\0';
	      gtk_file_info_set_mime_type (info, value);
	      g_free (value);
	    }
	  else
	    gtk_file_info_set_mime_type (info, "application/octet-stream");
	  if (key != NULL)
	    RegCloseKey (key);
	}
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
filename_is_drive_root (const char *filename)
{
  guint len = strlen (filename);

  return (len == 3 &&
	  g_ascii_isalpha (filename[0]) &&
	  filename[1] == ':' &&
	  G_IS_DIR_SEPARATOR (filename[2]));
}

static gboolean
filename_is_server_share (const char *filename)
{
  /* Check if filename is of the form \\server\share or \\server\share\ */

  const char *p, *q, *r;

  if (!(G_IS_DIR_SEPARATOR (filename[0]) &&
	filename[1] == filename[0]))
    return FALSE;

  p = strchr (filename + 2, '\\');
  q = strchr (filename + 2, '/');

  if (p == NULL || (q != NULL && q < p))
    p = q;

  if (p == NULL)
    return FALSE;

  if (!p[1] || G_IS_DIR_SEPARATOR (p[1]))
    return FALSE;

  q = strchr (p + 1, '\\');
  r = strchr (p + 1, '/');

  if (q == NULL || (r != NULL && r < q))
    q = r;

  if (q == NULL ||
      q[1] == '\0')
    return TRUE;

  return FALSE;
}

static gboolean
filename_is_some_root (const char *filename)
{
#if 0
  return ((G_IS_DIR_SEPARATOR (filename[0]) && filename[1] == '\0') ||
	  filename_is_server_share (filename) ||
	  filename_is_drive_root (filename));
#else
  return (g_path_is_absolute (filename) &&
	  *(g_path_skip_root (filename)) == '\0');
#endif
}    

int
_gtk_file_system_win32_path_compare (const gchar *path1,
				     const gchar *path2)
{
  int retval;
  gchar *folded_path1 = g_utf8_casefold (path1, -1);
  gchar *folded_path2 = g_utf8_casefold (path2, -1);

  retval = strcmp (folded_path1, folded_path2);

  g_free (folded_path1);
  g_free (folded_path2);

  return retval;
}

#define __GTK_FILE_SYSTEM_WIN32_C__
#include "gtkaliasdef.c"
