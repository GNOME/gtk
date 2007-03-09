/* GTK - The GIMP Toolkit
 * gtkfilesystemwin32.c: Default implementation of GtkFileSystem for Windows
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2004, Hans Breuer
 * Copyright (C) 2007, Novell, Inc.
 * Copyright (C) 2007, Mathias Hasselmann
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

/* #define this if you want the program to crash when a file system gets
 * finalized while async handles are still outstanding.
 */
#undef HANDLE_ME_HARDER

#include <config.h>

#include "gtkfilesystem.h"
#include "gtkfilesystemwin32.h"
#include "gtkicontheme.h"
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

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include "gdk/win32/gdkwin32.h"
#undef STRICT
#include <shlobj.h>
#include <shellapi.h>

#define BOOKMARKS_FILENAME ".gtk-bookmarks"

#define FOLDER_CACHE_LIFETIME 2 /* seconds */

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

  GHashTable *handles;

  guint execute_callbacks_idle_id;
  GSList *callbacks;
};

/* Icon type, supplemented by MIME type
 */
typedef enum {
  ICON_UNDECIDED, /* Only used while we have not yet computed the icon in a struct stat_info_entry */
  ICON_NONE,      /* "Could not compute the icon type" */
  ICON_REGULAR,	  /* Use mime type for icon */
  ICON_DIRECTORY,
  ICON_EXECUTABLE,
  ICON_VOLUME
} IconType;


#define GTK_TYPE_FILE_FOLDER_WIN32             (_gtk_file_folder_win32_get_type ())
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
  GHashTable *stat_info;
  guint load_folder_id;
  guint have_stat : 1;
  guint have_mime_type : 1;
  guint is_network_dir : 1;
  guint is_finished_loading : 1;
  time_t asof;
};

struct stat_info_entry {
  WIN32_FILE_ATTRIBUTE_DATA wfad;
  char *mime_type;
  IconType icon_type;
  gboolean hidden;
};

static const GtkFileInfoType STAT_NEEDED_MASK = (GTK_FILE_INFO_IS_FOLDER |
						 GTK_FILE_INFO_MODIFICATION_TIME |
						 GTK_FILE_INFO_SIZE |
						 GTK_FILE_INFO_ICON);

static void gtk_file_system_win32_iface_init  (GtkFileSystemIface      *iface);
static void gtk_file_system_win32_dispose     (GObject                 *object);
static void gtk_file_system_win32_finalize    (GObject                 *object);

static GSList *             gtk_file_system_win32_list_volumes        (GtkFileSystem     *file_system);
static GtkFileSystemVolume *gtk_file_system_win32_get_volume_for_path (GtkFileSystem     *file_system,
                                                                       const GtkFilePath *path);

static GtkFileSystemHandle *gtk_file_system_win32_get_folder (GtkFileSystem                 *file_system,
							      const GtkFilePath             *path,
							      GtkFileInfoType                types,
							      GtkFileSystemGetFolderCallback callback,
							      gpointer                       data);
static GtkFileSystemHandle *gtk_file_system_win32_get_info (GtkFileSystem               *file_system,
							    const GtkFilePath           *path,
							    GtkFileInfoType              types,
							    GtkFileSystemGetInfoCallback callback,
							    gpointer                     data);
static GtkFileSystemHandle *gtk_file_system_win32_create_folder (GtkFileSystem                    *file_system,
								 const GtkFilePath                *path,
								 GtkFileSystemCreateFolderCallback callback,
								 gpointer data);
static void         gtk_file_system_win32_cancel_operation        (GtkFileSystemHandle *handle);

static void         gtk_file_system_win32_volume_free             (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static GtkFilePath *gtk_file_system_win32_volume_get_base_path    (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static gboolean     gtk_file_system_win32_volume_get_is_mounted   (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static GtkFileSystemHandle *gtk_file_system_win32_volume_mount    (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume,
								   GtkFileSystemVolumeMountCallback callback,
								   gpointer data);
static gchar *      gtk_file_system_win32_volume_get_display_name (GtkFileSystem       *file_system,
								   GtkFileSystemVolume *volume);
static gchar *      gtk_file_system_win32_volume_get_icon_name    (GtkFileSystem        *file_system,
								   GtkFileSystemVolume *volume,
								   GError             **error);

static gboolean       gtk_file_system_win32_get_parent   (GtkFileSystem      *file_system,
						          const GtkFilePath  *path,
						          GtkFilePath       **parent,
						          GError            **error);
static GtkFilePath *  gtk_file_system_win32_make_path    (GtkFileSystem      *file_system,
						          const GtkFilePath  *base_path,
						          const gchar        *display_name,
						          GError            **error);
static gboolean       gtk_file_system_win32_parse        (GtkFileSystem      *file_system,
						          const GtkFilePath  *base_path,
						          const gchar        *str,
						          GtkFilePath       **folder,
						          gchar             **file_part,
						          GError            **error);

static gchar *      gtk_file_system_win32_path_to_uri      (GtkFileSystem      *file_system,
							    const GtkFilePath  *path);
static gchar *      gtk_file_system_win32_path_to_filename (GtkFileSystem      *file_system,
							    const GtkFilePath  *path);
static GtkFilePath *gtk_file_system_win32_uri_to_path      (GtkFileSystem      *file_system,
							    const gchar        *uri);
static GtkFilePath *gtk_file_system_win32_filename_to_path (GtkFileSystem      *file_system,
							    const gchar        *filename);


static gboolean       gtk_file_system_win32_insert_bookmark  (GtkFileSystem     *file_system,
							      const GtkFilePath *path,
							      gint               position,
							      GError           **error);
static gboolean       gtk_file_system_win32_remove_bookmark  (GtkFileSystem     *file_system,
							      const GtkFilePath *path,
							      GError           **error);
static GSList       * gtk_file_system_win32_list_bookmarks     (GtkFileSystem     *file_system);
static gchar        * gtk_file_system_win32_get_bookmark_label (GtkFileSystem     *file_system,
								const GtkFilePath *path);
static void           gtk_file_system_win32_set_bookmark_label (GtkFileSystem     *file_system,
								const GtkFilePath *path,
								const gchar       *label);

static void           gtk_file_folder_win32_iface_init (GtkFileFolderIface       *iface);
static void           gtk_file_folder_win32_finalize   (GObject                  *object);

static GtkFileInfo   *gtk_file_folder_win32_get_info         (GtkFileFolder       *folder,
							      const GtkFilePath   *path,
							      GError             **error);
static gboolean       gtk_file_folder_win32_list_children    (GtkFileFolder       *folder,
							      GSList             **children,
							      GError             **error);

static gboolean       gtk_file_folder_win32_is_finished_loading (GtkFileFolder *folder);

static GtkFilePath *filename_to_path       (const gchar *filename);

static gboolean     filename_is_root       (const char *filename);

static gboolean     filename_is_drive_root (const char *filename);
static gboolean     filename_is_some_root  (const char *filename);

static gboolean     stat_with_error        (const char                *filename,
					    WIN32_FILE_ATTRIBUTE_DATA *wfad,
					    GError                   **error);
static GtkFileInfo *create_file_info       (GtkFileFolderWin32        *folder_win32,
					    const char                *filename,
					    GtkFileInfoType            types,
					    WIN32_FILE_ATTRIBUTE_DATA *wfad,
					    const char                *mime_type);

static gboolean execute_callbacks_idle (gpointer data);
static void execute_callbacks (gpointer data);

static gboolean fill_in_names        (GtkFileFolderWin32  *folder_win32,
				      GError             **error);
static void     fill_in_stats        (GtkFileFolderWin32  *folder_win32);
static void     fill_in_mime_type    (GtkFileFolderWin32  *folder_win32);

static gboolean cb_fill_in_stats     (gpointer key,
				      gpointer value,
				      gpointer user_data);
static gboolean cb_fill_in_mime_type (gpointer key,
				      gpointer value,
				      gpointer user_data);

/* some info kept together for volumes */
struct _GtkFileSystemVolume
{
  gchar *drive;
  int drive_type;
};

/*
 * GtkFileSystemWin32
 */
G_DEFINE_TYPE_WITH_CODE (GtkFileSystemWin32, gtk_file_system_win32, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_SYSTEM,
						gtk_file_system_win32_iface_init))

/*
 * GtkFileFolderWin32
 */
G_DEFINE_TYPE_WITH_CODE (GtkFileFolderWin32, _gtk_file_folder_win32, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_FOLDER,
						gtk_file_folder_win32_iface_init))

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

  gobject_class->dispose = gtk_file_system_win32_dispose;
  gobject_class->finalize = gtk_file_system_win32_finalize;
}

static void
gtk_file_system_win32_iface_init (GtkFileSystemIface *iface)
{
  iface->list_volumes = gtk_file_system_win32_list_volumes;
  iface->get_volume_for_path = gtk_file_system_win32_get_volume_for_path;
  iface->get_folder = gtk_file_system_win32_get_folder;
  iface->get_info = gtk_file_system_win32_get_info;
  iface->create_folder = gtk_file_system_win32_create_folder;
  iface->cancel_operation = gtk_file_system_win32_cancel_operation;
  iface->volume_free = gtk_file_system_win32_volume_free;
  iface->volume_get_base_path = gtk_file_system_win32_volume_get_base_path;
  iface->volume_get_is_mounted = gtk_file_system_win32_volume_get_is_mounted;
  iface->volume_mount = gtk_file_system_win32_volume_mount;
  iface->volume_get_display_name = gtk_file_system_win32_volume_get_display_name;
  iface->volume_get_icon_name = gtk_file_system_win32_volume_get_icon_name;
  iface->get_parent = gtk_file_system_win32_get_parent;
  iface->make_path = gtk_file_system_win32_make_path;
  iface->parse = gtk_file_system_win32_parse;
  iface->path_to_uri = gtk_file_system_win32_path_to_uri;
  iface->path_to_filename = gtk_file_system_win32_path_to_filename;
  iface->uri_to_path = gtk_file_system_win32_uri_to_path;
  iface->filename_to_path = gtk_file_system_win32_filename_to_path;
  iface->insert_bookmark = gtk_file_system_win32_insert_bookmark;
  iface->remove_bookmark = gtk_file_system_win32_remove_bookmark;
  iface->list_bookmarks = gtk_file_system_win32_list_bookmarks;
  iface->get_bookmark_label = gtk_file_system_win32_get_bookmark_label;
  iface->set_bookmark_label = gtk_file_system_win32_set_bookmark_label;
}

static gboolean
check_volumes (gpointer data)
{
  GtkFileSystemWin32 *system_win32 = GTK_FILE_SYSTEM_WIN32 (data);

  g_return_val_if_fail (system_win32, FALSE);

  if (system_win32->drives != GetLogicalDrives())
    g_signal_emit_by_name (system_win32, "volumes-changed", 0);

  return TRUE;
}

static guint
casefolded_hash (gconstpointer v)
{
  const gchar *p = (const gchar *) v;
  guint32 h = 0;

  while (*p)
    {
      h = (h << 5) - h + g_unichar_toupper (g_utf8_get_char (p));
      p = g_utf8_next_char (p);
    }

  return h;
}

static gboolean casefolded_equal (gconstpointer v1,
				  gconstpointer v2)
{
  return (_gtk_file_system_win32_path_compare ((const gchar *) v1, (const gchar *) v2) == 0);
}

static void
gtk_file_system_win32_init (GtkFileSystemWin32 *system_win32)
{
  system_win32->folder_hash = g_hash_table_new (casefolded_hash, casefolded_equal);

  /* Set up an idle handler for volume changes. Once a second should
   * be enough.
   */
  system_win32->timeout = g_timeout_add_full (0, 1000, check_volumes, system_win32, NULL);

  system_win32->handles = g_hash_table_new (g_direct_hash, g_direct_equal);

  system_win32->execute_callbacks_idle_id = 0;
  system_win32->callbacks = NULL;
}

static void
check_handle_fn (gpointer key, gpointer value, gpointer data)
{
  GtkFileSystemHandle *handle;
  int *num_live_handles;

  handle = key;
  num_live_handles = data;

  (*num_live_handles)++;

  g_warning ("file_system_win32=%p still has handle=%p at finalization which is %s!",
	     handle->file_system,
	     handle,
	     handle->cancelled ? "CANCELLED" : "NOT CANCELLED");
}

static void
check_handles_at_finalization (GtkFileSystemWin32 *system_win32)
{
  int num_live_handles;

  num_live_handles = 0;

  g_hash_table_foreach (system_win32->handles, check_handle_fn, &num_live_handles);
#ifdef HANDLE_ME_HARDER
  g_assert (num_live_handles == 0);
#endif

  g_hash_table_destroy (system_win32->handles);
  system_win32->handles = NULL;
}

#define GTK_TYPE_FILE_SYSTEM_HANDLE_WIN32 (_gtk_file_system_handle_win32_get_type ())

typedef struct _GtkFileSystemHandle GtkFileSystemHandleWin32;
typedef struct _GtkFileSystemHandleClass GtkFileSystemHandleWin32Class;

G_DEFINE_TYPE (GtkFileSystemHandleWin32, _gtk_file_system_handle_win32, GTK_TYPE_FILE_SYSTEM_HANDLE)

static void
_gtk_file_system_handle_win32_init (GtkFileSystemHandleWin32 *handle)
{
}

static void
_gtk_file_system_handle_win32_finalize (GObject *object)
{
  GtkFileSystemHandleWin32 *handle;
  GtkFileSystemWin32 *system_win32;

  handle = (GtkFileSystemHandleWin32 *)object;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (GTK_FILE_SYSTEM_HANDLE (handle)->file_system);

  g_assert (g_hash_table_lookup (system_win32->handles, handle) != NULL);
  g_hash_table_remove (system_win32->handles, handle);

  if (G_OBJECT_CLASS (_gtk_file_system_handle_win32_parent_class)->finalize)
    G_OBJECT_CLASS (_gtk_file_system_handle_win32_parent_class)->finalize (object);
}

static void
_gtk_file_system_handle_win32_class_init (GtkFileSystemHandleWin32Class *class)
{
  GObjectClass *gobject_class = (GObjectClass *) class;

  gobject_class->finalize = _gtk_file_system_handle_win32_finalize;
}

static void
gtk_file_system_win32_dispose (GObject *object)
{
  GtkFileSystemWin32 *system_win32;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (object);

  if (system_win32->execute_callbacks_idle_id)
    {
      g_source_remove (system_win32->execute_callbacks_idle_id);
      system_win32->execute_callbacks_idle_id = 0;

      /* call pending callbacks */
      execute_callbacks (system_win32);
    }

  G_OBJECT_CLASS (gtk_file_system_win32_parent_class)->dispose (object);
}

static void
gtk_file_system_win32_finalize (GObject *object)
{
  GtkFileSystemWin32 *system_win32;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (object);

  g_source_remove (system_win32->timeout);

  check_handles_at_finalization  (system_win32);

  /* FIXME: assert that the hash is empty? */
  g_hash_table_destroy (system_win32->folder_hash);

  G_OBJECT_CLASS (gtk_file_system_win32_parent_class)->finalize (object);
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
      b = SHGetPathFromIDListW (pidl, path.wc);
      if (b)
	retval = g_utf16_to_utf8 (path.wc, -1, NULL, NULL, NULL);
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
      else
	{
	  wchar_t *wdrive = g_utf8_to_utf16 (vol->drive, -1, NULL, NULL, NULL);
	  vol->drive_type = GetDriveTypeW (wdrive);
	  g_free (wdrive);
	}
    }
  return vol;
}

static char *
remove_trailing_slash (const char *filename)
{
  int root_len, len;

  len = strlen (filename);

  if (g_path_is_absolute (filename))
    root_len = g_path_skip_root (filename) - filename;
  else
    root_len = 1;
  if (len > root_len && G_IS_DIR_SEPARATOR (filename[len - 1]))
    return g_strndup (filename, len - 1);
  else
    return g_memdup (filename, len + 1);
}

/* Delay callback dispatching
 */

enum callback_types
{
  CALLBACK_GET_INFO,
  CALLBACK_GET_FOLDER,
  CALLBACK_CREATE_FOLDER,
  CALLBACK_VOLUME_MOUNT
};

static void queue_callback (GtkFileSystemWin32 *system_win32, enum callback_types type, gpointer data);

struct get_info_callback
{
  GtkFileSystemGetInfoCallback callback;
  GtkFileSystemHandle *handle;
  GtkFileInfo *file_info;
  GError *error;
  gpointer data;
};

static inline void
dispatch_get_info_callback (struct get_info_callback *info)
{
  (* info->callback) (info->handle, info->file_info, info->error, info->data);

  if (info->file_info)
    gtk_file_info_free (info->file_info);

  if (info->error)
    g_error_free (info->error);

  g_object_unref (info->handle);

  g_free (info);
}

static inline void
queue_get_info_callback (GtkFileSystemGetInfoCallback  callback,
			 GtkFileSystemHandle          *handle,
			 GtkFileInfo                  *file_info,
			 GError                       *error,
			 gpointer                      data)
{
  struct get_info_callback *info;

  info = g_new (struct get_info_callback, 1);
  info->callback = callback;
  info->handle = handle;
  info->file_info = file_info;
  info->error = error;
  info->data = data;

  queue_callback (GTK_FILE_SYSTEM_WIN32 (handle->file_system), CALLBACK_GET_INFO, info);
}


struct get_folder_callback
{
  GtkFileSystemGetFolderCallback callback;
  GtkFileSystemHandle *handle;
  GtkFileFolder *folder;
  GError *error;
  gpointer data;
};

static inline void
dispatch_get_folder_callback (struct get_folder_callback *info)
{
  (* info->callback) (info->handle, info->folder, info->error, info->data);

  if (info->error)
    g_error_free (info->error);

  g_object_unref (info->handle);

  g_free (info);
}

static inline void
queue_get_folder_callback (GtkFileSystemGetFolderCallback  callback,
			   GtkFileSystemHandle            *handle,
			   GtkFileFolder                  *folder,
			   GError                         *error,
			   gpointer                        data)
{
  struct get_folder_callback *info;

  info = g_new (struct get_folder_callback, 1);
  info->callback = callback;
  info->handle = handle;
  info->folder = folder;
  info->error = error;
  info->data = data;

  queue_callback (GTK_FILE_SYSTEM_WIN32 (handle->file_system), CALLBACK_GET_FOLDER, info);
}


struct create_folder_callback
{
  GtkFileSystemCreateFolderCallback callback;
  GtkFileSystemHandle *handle;
  GtkFilePath *path;
  GError *error;
  gpointer data;
};

static inline void
dispatch_create_folder_callback (struct create_folder_callback *info)
{
  (* info->callback) (info->handle, info->path, info->error, info->data);

  if (info->error)
    g_error_free (info->error);

  if (info->path)
    gtk_file_path_free (info->path);

  g_object_unref (info->handle);

  g_free (info);
}

static inline void
queue_create_folder_callback (GtkFileSystemCreateFolderCallback  callback,
			      GtkFileSystemHandle               *handle,
			      const GtkFilePath                 *path,
			      GError                            *error,
			      gpointer                           data)
{
  struct create_folder_callback *info;

  info = g_new (struct create_folder_callback, 1);
  info->callback = callback;
  info->handle = handle;
  info->path = gtk_file_path_copy (path);
  info->error = error;
  info->data = data;

  queue_callback (GTK_FILE_SYSTEM_WIN32 (handle->file_system), CALLBACK_CREATE_FOLDER, info);
}


struct volume_mount_callback
{
  GtkFileSystemVolumeMountCallback callback;
  GtkFileSystemHandle *handle;
  GtkFileSystemVolume *volume;
  GError *error;
  gpointer data;
};

static inline void
dispatch_volume_mount_callback (struct volume_mount_callback *info)
{
  (* info->callback) (info->handle, info->volume, info->error, info->data);

  if (info->error)
    g_error_free (info->error);

  g_object_unref (info->handle);

  g_free (info);
}

static inline void
queue_volume_mount_callback (GtkFileSystemVolumeMountCallback  callback,
			     GtkFileSystemHandle              *handle,
			     GtkFileSystemVolume              *volume,
			     GError                           *error,
			     gpointer                          data)
{
  struct volume_mount_callback *info;

  info = g_new (struct volume_mount_callback, 1);
  info->callback = callback;
  info->handle = handle;
  info->volume = volume;
  info->error = error;
  info->data = data;

  queue_callback (GTK_FILE_SYSTEM_WIN32 (handle->file_system), CALLBACK_VOLUME_MOUNT, info);
}


struct callback_info
{
  enum callback_types type;

  union
  {
    struct get_info_callback *get_info;
    struct get_folder_callback *get_folder;
    struct create_folder_callback *create_folder;
    struct volume_mount_callback *volume_mount;
  } info;
};



static void
execute_callbacks (gpointer data)
{
  GSList *l;
  gboolean unref_file_system = TRUE;
  GtkFileSystemWin32 *system_win32 = GTK_FILE_SYSTEM_WIN32 (data);

  if (!system_win32->execute_callbacks_idle_id)
    unref_file_system = FALSE;
  else
    g_object_ref (system_win32);

  for (l = system_win32->callbacks; l; l = l->next)
    {
      struct callback_info *info = l->data;

      switch (info->type)
        {
	  case CALLBACK_GET_INFO:
	    dispatch_get_info_callback (info->info.get_info);
	    break;

	  case CALLBACK_GET_FOLDER:
	    dispatch_get_folder_callback (info->info.get_folder);
	    break;

	  case CALLBACK_CREATE_FOLDER:
	    dispatch_create_folder_callback (info->info.create_folder);
	    break;

	  case CALLBACK_VOLUME_MOUNT:
	    dispatch_volume_mount_callback (info->info.volume_mount);
	    break;
        }

      g_free (info);
    }

  g_slist_free (system_win32->callbacks);
  system_win32->callbacks = NULL;

  if (unref_file_system)
    g_object_unref (system_win32);

  system_win32->execute_callbacks_idle_id = 0;
}

static gboolean
execute_callbacks_idle (gpointer data)
{
  GDK_THREADS_ENTER ();

  execute_callbacks(data);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
queue_callback (GtkFileSystemWin32  *system_win32,
		enum callback_types  type,
		gpointer             data)
{
  struct callback_info *info;

  info = g_new (struct callback_info, 1);
  info->type = type;

  switch (type)
    {
      case CALLBACK_GET_INFO:
	info->info.get_info = data;
	break;

      case CALLBACK_GET_FOLDER:
	info->info.get_folder = data;
	break;

      case CALLBACK_CREATE_FOLDER:
	info->info.create_folder = data;
	break;

      case CALLBACK_VOLUME_MOUNT:
	info->info.volume_mount = data;
	break;
    }

  system_win32->callbacks = g_slist_append (system_win32->callbacks, info);

  if (!system_win32->execute_callbacks_idle_id)
    system_win32->execute_callbacks_idle_id = g_idle_add (execute_callbacks_idle, system_win32);
}

static GtkFileSystemHandle *
create_handle (GtkFileSystem *file_system)
{
  GtkFileSystemWin32 *system_win32;
  GtkFileSystemHandle *handle;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (file_system);

  handle = g_object_new (GTK_TYPE_FILE_SYSTEM_HANDLE_WIN32, NULL);
  handle->file_system = file_system;

  g_assert (g_hash_table_lookup (system_win32->handles, handle) == NULL);
  g_hash_table_insert (system_win32->handles, handle, handle);

  return handle;
}

static char *
get_mime_type_for_file (const char                      *filename,
			const WIN32_FILE_ATTRIBUTE_DATA *wfad)
{
  const char *extension;
  HKEY key = NULL;
  DWORD type, nbytes = 0;
  char *value = NULL;

  if (wfad->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    return g_strdup ("x-directory/normal");

  extension = strrchr (filename, '.');

  if (extension != NULL &&
      (stricmp (extension, ".exe") == 0 ||
       stricmp (extension, ".com") == 0))
    return g_strdup ("application/x-executable");

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
    }
  else
    value = g_strdup ("application/octet-stream");
  if (key != NULL)
    RegCloseKey (key);

  return value;
}

static GtkFileSystemHandle *
gtk_file_system_win32_get_info (GtkFileSystem               *file_system,
				const GtkFilePath           *path,
				GtkFileInfoType              types,
				GtkFileSystemGetInfoCallback callback,
				gpointer                     data)
{
  GError *error = NULL;
  GtkFileSystemWin32 *system_win32;
  GtkFileSystemHandle *handle;
  const char *filename;
  GtkFileInfo *info;
  WIN32_FILE_ATTRIBUTE_DATA wfad;
  const char *mime_type;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (file_system);
  handle = create_handle (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  if (!stat_with_error (filename, &wfad, &error))
    {
      g_object_ref (handle);
      queue_get_info_callback (callback, handle, NULL, error, data);
      return handle;
    }

  if ((types & GTK_FILE_INFO_MIME_TYPE) != 0)
    mime_type = get_mime_type_for_file (filename, &wfad);
  else
    mime_type = NULL;

  info = create_file_info (NULL, filename, types, &wfad, mime_type);
  g_object_ref (handle);
  queue_get_info_callback (callback, handle, info, NULL, data);

  return handle;
}

static gboolean
load_folder (gpointer data)
{
  GtkFileFolderWin32 *folder_win32 = data;
  GSList *children;

  GDK_THREADS_ENTER ();

  if ((folder_win32->types & STAT_NEEDED_MASK) != 0)
    fill_in_stats (folder_win32);

  if ((folder_win32->types & GTK_FILE_INFO_MIME_TYPE) != 0)
    fill_in_mime_type (folder_win32);

  if (gtk_file_folder_win32_list_children (GTK_FILE_FOLDER (folder_win32), &children, NULL))
    {
      folder_win32->is_finished_loading = TRUE;
      g_signal_emit_by_name (folder_win32, "files-added", children);
      gtk_file_paths_free (children);
    }

  folder_win32->load_folder_id = 0;

  g_signal_emit_by_name (folder_win32, "finished-loading", 0);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static GtkFileSystemHandle *
gtk_file_system_win32_get_folder (GtkFileSystem                 *file_system,
				  const GtkFilePath             *path,
				  GtkFileInfoType                types,
				  GtkFileSystemGetFolderCallback callback,
				  gpointer                       data)
{
  GError *error = NULL;
  GtkFileSystemWin32 *system_win32;
  GtkFileFolderWin32 *folder_win32;
  GtkFileSystemHandle *handle;
  const gchar *filename;
  char *filename_copy;
  gboolean set_asof = FALSE;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  handle = create_handle (file_system);

  filename_copy = remove_trailing_slash (filename);
  folder_win32 = g_hash_table_lookup (system_win32->folder_hash, filename_copy);

  if (folder_win32)
    {
      g_free (filename_copy);
      if (folder_win32->stat_info &&
	  time (NULL) - folder_win32->asof >= FOLDER_CACHE_LIFETIME)
	{
#if 0
	  g_print ("Cleaning out cached directory %s\n", filename);
#endif
	  g_hash_table_destroy (folder_win32->stat_info);
	  folder_win32->stat_info = NULL;
	  folder_win32->have_mime_type = FALSE;
	  folder_win32->have_stat = FALSE;
	  set_asof = TRUE;
	}

      g_object_ref (folder_win32);
      folder_win32->types |= types;
      types = folder_win32->types;
    }
  else
    {
      WIN32_FILE_ATTRIBUTE_DATA wfad;

      if (!stat_with_error (filename, &wfad, &error))
	{
	  g_object_ref (handle);
	  queue_get_folder_callback (callback, handle, NULL, error, data);

	  g_free (filename_copy);
	  return handle;
	}

      if (!wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
	  gchar *display_name = g_filename_display_name (filename);

	  g_set_error (&error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_NOT_FOLDER,
		       _("Path is not a folder: '%s'"),
		       display_name);

	  g_object_ref (handle);
	  queue_get_folder_callback (callback, handle, NULL, error, data);

	  g_free (display_name);
	  g_free (filename_copy);
	  return handle;
	}

      folder_win32 = g_object_new (GTK_TYPE_FILE_FOLDER_WIN32, NULL);
      folder_win32->system_win32 = system_win32;
      folder_win32->filename = filename_copy;
      folder_win32->types = types;
      folder_win32->stat_info = NULL;
      folder_win32->load_folder_id = 0;
      folder_win32->have_mime_type = FALSE;
      folder_win32->have_stat = FALSE;
      folder_win32->is_finished_loading = FALSE;
      set_asof = TRUE;

      /* Browsing for shares not yet implemented */
      folder_win32->is_network_dir = FALSE;

      g_hash_table_insert (system_win32->folder_hash,
			   folder_win32->filename,
			   folder_win32);
    }

  if (set_asof)
    folder_win32->asof = time (NULL);

  g_object_ref (handle);
  queue_get_folder_callback (callback, handle, GTK_FILE_FOLDER (folder_win32), NULL, data);

  /* Start loading the folder contents in an idle */
  if (!folder_win32->load_folder_id)
    folder_win32->load_folder_id =
      g_idle_add ((GSourceFunc) load_folder, folder_win32);

  return handle;
}

static GtkFileSystemHandle *
gtk_file_system_win32_create_folder (GtkFileSystem                    *file_system,
				     const GtkFilePath                *path,
				     GtkFileSystemCreateFolderCallback callback,
				     gpointer                          data)
{
  GError *error = NULL;
  GtkFileSystemWin32 *system_win32;
  GtkFileSystemHandle *handle;
  const char *filename;
  gboolean result;
  char *parent, *tmp;
  int save_errno;

  system_win32 = GTK_FILE_SYSTEM_WIN32 (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  handle = create_handle (file_system);

  tmp = remove_trailing_slash (filename);
  result = g_mkdir (tmp, 0777) == 0;
  save_errno = errno;
  g_free (tmp);

  if (!result)
    {
      gchar *display_filename = g_filename_display_name (filename);
      g_set_error (&error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("Error creating directory '%s': %s"),
		   display_filename,
		   g_strerror (save_errno));

      g_object_ref (handle);
      queue_create_folder_callback (callback, handle, path, error, data);

      g_free (display_filename);
      return handle;
    }

  if (!filename_is_some_root (filename))
    {
      parent = g_path_get_dirname (filename);
      if (parent)
	{
	  GtkFileFolderWin32 *folder_win32;

	  folder_win32 = g_hash_table_lookup (system_win32->folder_hash, parent);
	  if (folder_win32)
	    {
	      GSList *paths;
	      char *basename;
	      struct stat_info_entry *entry;

	      /* Make sure the new folder exists in the parent's folder */
	      entry = g_new0 (struct stat_info_entry, 1);
	      if (folder_win32->is_network_dir)
		{
		  entry->wfad.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		  entry->mime_type = g_strdup ("x-directory/normal");
		}

	      basename = g_path_get_basename (filename);
	      g_hash_table_insert (folder_win32->stat_info,
				   basename,
				   entry);

	      if (folder_win32->have_stat)
		{
		  /* Cheating */
		  if ((folder_win32->types & STAT_NEEDED_MASK) != 0)
		    cb_fill_in_stats (basename, entry, folder_win32);

		  if ((folder_win32->types & GTK_FILE_INFO_MIME_TYPE) != 0)
		    cb_fill_in_mime_type (basename, entry, folder_win32);
		}

	      paths = g_slist_append (NULL, (GtkFilePath *) path);
	      g_signal_emit_by_name (folder_win32, "files-added", paths);
	      g_slist_free (paths);
	    }

	  g_free(parent);
	}
    }

  return handle;
}

static void
gtk_file_system_win32_cancel_operation (GtkFileSystemHandle *handle)
{
  /* We don't set "cancelled" to TRUE here, since the actual operation
   * is executed in the function itself and not in a callback.  So
   * the operations can never be cancelled (since they will be already
   * completed at this point.
   */
}

static void
gtk_file_system_win32_volume_free (GtkFileSystem        *file_system,
				   GtkFileSystemVolume  *volume)
{
  GtkFilePath *path;

  g_free (volume->drive);
  path = (GtkFilePath *) volume;
  gtk_file_path_free (path);
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

static GtkFileSystemHandle *
gtk_file_system_win32_volume_mount (GtkFileSystem                   *file_system,
				    GtkFileSystemVolume             *volume,
				    GtkFileSystemVolumeMountCallback callback,
				    gpointer                         data)
{
  GError *error = NULL;
  GtkFileSystemHandle *handle = create_handle (file_system);

  g_set_error (&error,
	       GTK_FILE_SYSTEM_ERROR,
	       GTK_FILE_SYSTEM_ERROR_FAILED,
	       _("This file system does not support mounting"));

  g_object_ref (handle);
  queue_volume_mount_callback (callback, handle, volume, error, data);

  return handle;
}

static gchar *
gtk_file_system_win32_volume_get_display_name (GtkFileSystem       *file_system,
					       GtkFileSystemVolume *volume)
{
  gchar *real_display_name;

  g_return_val_if_fail (volume->drive != NULL, NULL);

  if (filename_is_drive_root (volume->drive) &&
      volume->drive_type == DRIVE_REMOTE)
    real_display_name = g_strdup_printf (_("Network Drive (%s)"), volume->drive);
  else if ((filename_is_drive_root (volume->drive) && volume->drive[0] >= 'C') ||
      volume->drive_type != DRIVE_REMOVABLE)
    {
      gchar *name = NULL;
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

      if (name != NULL)
        {
	  real_display_name = g_strdup_printf (_("%s (%s)"), name, volume->drive);
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

static IconType
get_icon_type_from_stat (WIN32_FILE_ATTRIBUTE_DATA *wfad)
{
  if (wfad->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    return ICON_DIRECTORY;
  else
    return ICON_REGULAR;
}

/* Computes our internal icon type based on a path name; also returns the MIME
 * type in case we come up with ICON_REGULAR.
 */
static IconType
get_icon_type_from_path (GtkFileFolderWin32        *folder_win32,
			 WIN32_FILE_ATTRIBUTE_DATA *wfad,
			 const char      	   *filename)
{
  IconType icon_type;

  if (folder_win32 && folder_win32->have_stat)
    {
      char *basename;
      struct stat_info_entry *entry;

      g_assert (folder_win32->stat_info != NULL);

      basename = g_path_get_basename (filename);
      entry = g_hash_table_lookup (folder_win32->stat_info, basename);
      g_free (basename);
      if (entry)
	{
	  if (entry->icon_type == ICON_UNDECIDED)
	    {
	      entry->icon_type = get_icon_type_from_stat (&entry->wfad);
	      g_assert (entry->icon_type != ICON_UNDECIDED);
	    }
	  icon_type = entry->icon_type;
	  if (icon_type == ICON_REGULAR)
	    {
	      fill_in_mime_type (folder_win32);
	    }

	  return icon_type;
	}
    }

  icon_type = get_icon_type_from_stat (wfad);

  return icon_type;
}

static const gchar *
get_fallback_icon_name (IconType icon_type)
{
  switch (icon_type)
    {
    case ICON_VOLUME:
      return GTK_STOCK_HARDDISK;

    case ICON_DIRECTORY:
      return GTK_STOCK_DIRECTORY;

    case ICON_EXECUTABLE:
      return GTK_STOCK_EXECUTE;

    default:
      return GTK_STOCK_FILE;
    }
}

static gchar *
get_icon_path (const gchar *filename,
               IconType     icon_type,
	       gint        *index)
{
  gchar *path = NULL;
  SHFILEINFOW shfi;
  wchar_t *wfn;

  g_return_val_if_fail (NULL != filename, NULL);
  g_return_val_if_fail ('\0' != *filename, NULL);
  g_return_val_if_fail (NULL != index, NULL);

  wfn = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);

  if (SHGetFileInfoW (wfn, 0, &shfi, sizeof (shfi), SHGFI_ICONLOCATION))
    {
      path = g_utf16_to_utf8 (shfi.szDisplayName, -1, NULL, NULL, NULL);
      *index = shfi.iIcon;
    }

  g_free (wfn);
  return path;
}

static gboolean
create_builtin_icon (const gchar *filename,
		     const gchar *icon_name,
		     IconType     icon_type)
{
  static const DWORD attributes[] =
    {
      SHGFI_ICON | SHGFI_LARGEICON,
      SHGFI_ICON | SHGFI_SMALLICON,
      0
    };

  GdkPixbuf *pixbuf = NULL;
  SHFILEINFOW shfi;
  DWORD_PTR rc;
  wchar_t *wfn;
  int i;

  g_return_val_if_fail (NULL != filename, FALSE);
  g_return_val_if_fail ('\0' != *filename, FALSE);

  wfn = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);

  for(i = 0; attributes[i]; ++i)
    {
      rc = SHGetFileInfoW (wfn, 0, &shfi, sizeof (shfi), attributes[i]);

      if (rc && shfi.hIcon)
        {
          pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (shfi.hIcon);

          if (!DestroyIcon (shfi.hIcon))
            g_warning (G_STRLOC ": DestroyIcon failed: %s\n",
                       g_win32_error_message (GetLastError ()));

          gtk_icon_theme_add_builtin_icon (icon_name,
                                           gdk_pixbuf_get_height (pixbuf),
                                           pixbuf);
          g_object_unref (pixbuf);
        }
    }

  g_free (wfn);
  return (NULL != pixbuf); /* at least one icon was created */
}

gchar *
get_icon_name (const gchar *filename,
	       IconType icon_type)
{
  gchar *icon_name = NULL;
  gchar *icon_path = NULL;
  gint icon_index = -1;

  icon_path = get_icon_path(filename, icon_type, &icon_index);

  if (icon_path)
    icon_name = g_strdup_printf ("gtk-win32-shell-icon;%s;%d",
     				 icon_path, icon_index);
  else
    icon_name = g_strdup_printf ("gtk-win32-shell-icon;%s",
                                 filename);

  if (!gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), icon_name) &&
      !create_builtin_icon (filename, icon_name, icon_type))
    {
      g_free (icon_name);
      icon_name = g_strdup (get_fallback_icon_name (icon_type));
    }

  g_free (icon_path);
  return icon_name;
}

static gchar *
gtk_file_system_win32_volume_get_icon_name (GtkFileSystem        *file_system,
					    GtkFileSystemVolume  *volume,
					    GError              **error)
{
  return get_icon_name (volume->drive, ICON_VOLUME);
}

#if 0 /* Unused, see below */

static char *
get_parent_dir (const char *filename)
{
  int len;

  len = strlen (filename);

  /* Ignore trailing slashes */
  if (len > 1 && G_IS_DIR_SEPARATOR (filename[len - 1]))
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

#endif

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
#if DEBUGGING_OUTPUT
  printf ("%s: %s => %s\n", __FUNCTION__, (char*)path, (*parent)?(char*)*parent:"NULL"), fflush(stdout);
#endif
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
  char *p;

  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (base_filename), NULL);

  if ((p = strpbrk (display_name, "<>\"/\\|")))
    {
      gchar badchar[2];

      badchar[0] = *p;		/* We know it is a single-byte char */
      badchar[1] = '\0';
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
		   _("The name \"%s\" is not valid because it contains the character \"%s\". "
		     "Please use a different name."),
		   display_name,
		   badchar);
      return NULL;
    }

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

#if DEBUGGING_OUTPUT
  printf("%s: %s ", __FUNCTION__, filename), fflush (stdout);
#endif

  past_root = (gchar *) g_path_skip_root (filename);

  q = p = past_root;

  while (*p)
    {
      if (G_IS_DIR_SEPARATOR (*p))
	{
	  if (!last_was_slash)
	    *q++ = '\\';

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

#if DEBUGGING_OUTPUT
  printf(" => %s\n", filename), fflush (stdout);
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

#if DEBUGGING_OUTPUT
  printf("%s: base_path=%s str=%s\n",__FUNCTION__,(char*)base_path,str),fflush(stdout);
#endif

  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (base_filename), FALSE);

  last_backslash = strrchr (str, '\\');
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
	    folder_part = g_strdup_printf ("%c:\\", base_filename[0]);
	  else
	    folder_part = g_strdup ("\\");
	}
      else if (g_ascii_isalpha (str[0]) &&
	       str[1] == ':' &&
	       last_slash == str + 2)
	folder_part = g_strndup (str, last_slash - str + 1);
#if 0
      /* Hmm, what the heck was this case supposed to do? It splits up
       * \\server\share\foo\bar into folder_part
       * \\server\share\foo\bar and file_path bar. Not good. As far as
       * I can see, this isn't needed.
       */
      else if (G_IS_DIR_SEPARATOR (str[0]) &&
	       G_IS_DIR_SEPARATOR (str[1]) &&
	       (!str[2] || !G_IS_DIR_SEPARATOR (str[2])))
	folder_part = g_strdup (str);
#endif
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

#if DEBUGGING_OUTPUT
  printf ("%s:returning folder=%s file_part=%s\n", __FUNCTION__, (*folder?(char*)*folder:"NULL"), *file_part), fflush(stdout);
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
  GtkFilePath *path;
  gchar *filename = g_filename_from_uri (uri, NULL, NULL);

#if DEBUGGING_OUTPUT
  printf ("%s: %s -> %s\n", __FUNCTION__, uri, filename?filename:"NULL"), fflush (stdout);
#endif

  if (filename)
    {
      path = filename_to_path (filename);
      g_free (filename);
    }
  else
    path = NULL;

  return path;
}

static GtkFilePath *
gtk_file_system_win32_filename_to_path (GtkFileSystem *file_system,
				        const gchar   *filename)
{
  return filename_to_path (filename);
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
bookmark_get_filename (void)
{
  char *filename;

  filename = g_build_filename (g_get_home_dir (),
			       BOOKMARKS_FILENAME, NULL);
  g_assert (filename != NULL);
  return filename;
}

static gboolean
bookmark_list_read (GSList **bookmarks, GError **error)
{
  gchar *filename;
  gchar *contents;
  gboolean result = FALSE;

  filename = bookmark_get_filename ();
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
bookmark_list_write (GSList  *bookmarks,
		     GError **error)
{
  GSList *l;
  GString *string;
  char *filename;
  GError *tmp_error = NULL;
  gboolean result;

  string = g_string_new ("");

  for (l = bookmarks; l; l = l->next)
    {
      g_string_append (string, l->data);
      g_string_append_c (string, '\n');
    }

  filename = bookmark_get_filename ();

  result = g_file_set_contents (filename, string->str, -1, &tmp_error);

  g_free (filename);
  g_string_free (string, TRUE);

  if (!result)
    {
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_FAILED,
		   _("Bookmark saving failed: %s"),
		   tmp_error->message);

      g_error_free (tmp_error);
    }

  return result;
}

static gboolean
gtk_file_system_win32_insert_bookmark (GtkFileSystem     *file_system,
				       const GtkFilePath *path,
				       gint               position,
				       GError            **error)
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

  uri = gtk_file_system_win32_path_to_uri (file_system, path);

  for (l = bookmarks; l; l = l->next)
    {
      char *bookmark, *space;

      bookmark = l->data;

      space = strchr (bookmark, ' ');
      if (space)
	*space = '\0';
      if (strcmp (bookmark, uri) != 0)
	{
	  if (space)
	    *space = ' ';
	}
      else
	{
	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_ALREADY_EXISTS,
		       _("'%s' already exists in the bookmarks list"),
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
gtk_file_system_win32_remove_bookmark (GtkFileSystem     *file_system,
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
      char *bookmark, *space;

      bookmark = (char *)l->data;
      space = strchr (bookmark, ' ');
      if (space)
	*space = '\0';

      if (strcmp (bookmark, uri) != 0)
	{
	  if (space)
	    *space = ' ';
	}
      else
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
	       _("'%s' does not exist in the bookmarks list"),
	       uri);

 out:

  g_free (uri);
  bookmark_list_free (bookmarks);

  return result;
}

static GSList *
gtk_file_system_win32_list_bookmarks (GtkFileSystem *file_system)
{
  GSList *bookmarks;
  GSList *result;
  GSList *l;

  if (!bookmark_list_read (&bookmarks, NULL))
    return NULL;

  result = NULL;

  for (l = bookmarks; l; l = l->next)
    {
      char *bookmark, *space;

      bookmark = (char *)l->data;
      space = strchr (bookmark, ' ');
      if (space)
	*space = '\0';

      if (is_local_uri (bookmark))
	result = g_slist_prepend (result, gtk_file_system_win32_uri_to_path (file_system, bookmark));
    }

  bookmark_list_free (bookmarks);

  result = g_slist_reverse (result);
  return result;
}

static gchar *
gtk_file_system_win32_get_bookmark_label (GtkFileSystem     *file_system,
					 const GtkFilePath *path)
{
  GSList *labels;
  gchar *label;
  GSList *l;
  char *bookmark, *space, *uri;

  labels = NULL;
  label = NULL;

  uri = gtk_file_system_path_to_uri (file_system, path);
  bookmark_list_read (&labels, NULL);

  for (l = labels; l && !label; l = l->next)
    {
      bookmark = (char *)l->data;
      space = strchr (bookmark, ' ');
      if (!space)
	continue;

      *space = '\0';

      if (strcmp (uri, bookmark) == 0)
	label = g_strdup (space + 1);
    }

  bookmark_list_free (labels);
  g_free (uri);

  return label;
}

static void
gtk_file_system_win32_set_bookmark_label (GtkFileSystem     *file_system,
					 const GtkFilePath *path,
					 const gchar       *label)
{
  GSList *labels;
  GSList *l;
  gchar *bookmark, *space, *uri;
  gboolean found;

  labels = NULL;

  uri = gtk_file_system_path_to_uri (file_system, path);
  bookmark_list_read (&labels, NULL);

  found = FALSE;
  for (l = labels; l && !found; l = l->next)
    {
      bookmark = (gchar *)l->data;
      space = strchr (bookmark, ' ');
      if (space)
	*space = '\0';

      if (strcmp (bookmark, uri) != 0)
	{
	  if (space)
	    *space = ' ';
	}
      else
	{
	  g_free (bookmark);

	  if (label && *label)
	    l->data = g_strdup_printf ("%s %s", uri, label);
	  else
	    l->data = g_strdup (uri);

	  found = TRUE;
	  break;
	}
    }

  if (found)
    {
      if (bookmark_list_write (labels, NULL))
	g_signal_emit_by_name (file_system, "bookmarks-changed", 0);
    }

  bookmark_list_free (labels);
  g_free (uri);
}

static void
_gtk_file_folder_win32_class_init (GtkFileFolderWin32Class *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_folder_win32_finalize;
}

static void
gtk_file_folder_win32_iface_init (GtkFileFolderIface *iface)
{
  iface->get_info = gtk_file_folder_win32_get_info;
  iface->list_children = gtk_file_folder_win32_list_children;
  iface->is_finished_loading = gtk_file_folder_win32_is_finished_loading;
}

static void
_gtk_file_folder_win32_init (GtkFileFolderWin32 *impl)
{
}

static void
gtk_file_folder_win32_finalize (GObject *object)
{
  GtkFileFolderWin32 *folder_win32 = GTK_FILE_FOLDER_WIN32 (object);

  if (folder_win32->load_folder_id)
    {
      g_source_remove (folder_win32->load_folder_id);
      folder_win32->load_folder_id = 0;
    }

  g_hash_table_remove (folder_win32->system_win32->folder_hash, folder_win32->filename);

  if (folder_win32->stat_info)
    {
#if 0
      g_print ("Releasing information for directory %s\n", folder_win32->filename);
#endif
      g_hash_table_destroy (folder_win32->stat_info);
    }

  g_free (folder_win32->filename);

  G_OBJECT_CLASS (_gtk_file_folder_win32_parent_class)->finalize (object);
}

/* Creates a GtkFileInfo for a volume root by stat()ing it */
static GtkFileInfo *
file_info_for_root_with_error (const char  *root_name,
			       GError     **error)
{
  struct stat statbuf;
  GtkFileInfo *info;

  if (g_stat (root_name, &statbuf) != 0)
    {
      int saved_errno;
      char *display_name;

      saved_errno = errno;
      display_name = g_filename_display_name (root_name);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_FAILED,
		   _("Error getting information for '%s': %s"),
		   display_name,
		   g_strerror (saved_errno));

      g_free (display_name);
      return NULL;
    }

  info = gtk_file_info_new ();
  gtk_file_info_set_display_name (info, root_name);
  gtk_file_info_set_is_folder (info, TRUE);
  gtk_file_info_set_is_hidden (info, FALSE);
  gtk_file_info_set_mime_type (info, "x-directory/normal");
  gtk_file_info_set_modification_time (info, statbuf.st_mtime);
  gtk_file_info_set_size (info, statbuf.st_size);

  return info;
}

static gboolean
stat_with_error (const char                *filename,
		 WIN32_FILE_ATTRIBUTE_DATA *wfad,
		 GError                   **error)
{
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, error);
  int rc;

  if (wfilename == NULL)
    return FALSE;

  rc = GetFileAttributesExW (wfilename, GetFileExInfoStandard, wfad);

  if (rc == 0)
    {
      if (error)
	{
	  int error_number = GetLastError ();
	  char *emsg = g_win32_error_message (error_number);
	  gchar *display_name = g_filename_display_name (filename);
	  int code;

	  if (error_number == ERROR_FILE_NOT_FOUND ||
	      error_number == ERROR_PATH_NOT_FOUND)
	    code = GTK_FILE_SYSTEM_ERROR_NONEXISTENT;
	  else
	    code = GTK_FILE_SYSTEM_ERROR_FAILED;

	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       code,
		       _("Error getting information for '%s': %s"),
		       display_name,
		       emsg);
	  g_free (display_name);
	  g_free (wfilename);
	  g_free (emsg);
	}
      return FALSE;
    }

  g_free (wfilename);
  return TRUE;
}

/* Creates a new GtkFileInfo from the specified data */
static GtkFileInfo *
create_file_info (GtkFileFolderWin32 	    *folder_win32,
                  const char         	    *filename,
		  GtkFileInfoType    	     types,
		  WIN32_FILE_ATTRIBUTE_DATA *wfad,
		  const char                *mime_type)
{
  GtkFileInfo *info;

  info = gtk_file_info_new ();

  if (types & GTK_FILE_INFO_DISPLAY_NAME)
    {
      gchar *display_name;

      if (filename_is_root (filename))
	display_name = g_filename_display_name (filename);
      else
	display_name = g_filename_display_basename (filename);

      gtk_file_info_set_display_name (info, display_name);
      g_free (display_name);
    }

  if (types & GTK_FILE_INFO_IS_HIDDEN)
    {
      gboolean is_hidden = !!(wfad->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
      gtk_file_info_set_is_hidden (info, is_hidden);
    }

  if (types & GTK_FILE_INFO_IS_FOLDER)
    gtk_file_info_set_is_folder (info, !!(wfad->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));

  if (types & GTK_FILE_INFO_MIME_TYPE)
    gtk_file_info_set_mime_type (info, mime_type);

  if (types & GTK_FILE_INFO_MODIFICATION_TIME)
    {
      GtkFileTime time = (wfad->ftLastWriteTime.dwLowDateTime
			  | ((guint64)wfad->ftLastWriteTime.dwHighDateTime) << 32);
      /* 100-nanosecond intervals since January 1, 1601, urgh! */
      time /= G_GINT64_CONSTANT (10000000); /* now seconds */
      time -= G_GINT64_CONSTANT (134774) * 24 * 3600; /* good old Unix time */
      gtk_file_info_set_modification_time (info, time);
    }

  if (types & GTK_FILE_INFO_SIZE)
    {
      gint64 size = wfad->nFileSizeLow | ((guint64)wfad->nFileSizeHigh) << 32;
      gtk_file_info_set_size (info, size);
    }

  if (types & GTK_FILE_INFO_ICON)
    {
      IconType icon_type = get_icon_type_from_path (folder_win32, wfad, filename);
      gchar *icon_name = get_icon_name (filename, icon_type);
      gtk_file_info_set_icon_name (info, icon_name);
      g_free (icon_name);
    }

  return info;
}

static struct stat_info_entry *
create_stat_info_entry_and_emit_add (GtkFileFolderWin32        *folder_win32,
				     const char                *filename,
				     const char                *basename,
				     WIN32_FILE_ATTRIBUTE_DATA *wfad)
{
  GSList *paths;
  GtkFilePath *path;
  struct stat_info_entry *entry;

  entry = g_new0 (struct stat_info_entry, 1);

  if ((folder_win32->types & STAT_NEEDED_MASK) != 0)
    entry->wfad = *wfad;

  if ((folder_win32->types & GTK_FILE_INFO_MIME_TYPE) != 0)
    entry->mime_type = get_mime_type_for_file (filename, wfad);

  g_hash_table_insert (folder_win32->stat_info,
		       g_strdup (basename),
		       entry);

  path = gtk_file_path_new_dup (filename);
  paths = g_slist_append (NULL, path);
  g_signal_emit_by_name (folder_win32, "files-added", paths);
  gtk_file_path_free (path);
  g_slist_free (paths);

  return entry;
}

static GtkFileInfo *
gtk_file_folder_win32_get_info (GtkFileFolder      *folder,
			        const GtkFilePath  *path,
				GError            **error)
{
  GtkFileFolderWin32 *folder_win32 = GTK_FILE_FOLDER_WIN32 (folder);
  GtkFileInfo *info;
  const char *filename;
  GtkFileInfoType types;
  WIN32_FILE_ATTRIBUTE_DATA wfad;
  const char *mime_type;

  /* Get_info for "/" */
  if (!path)
    {
      g_return_val_if_fail (filename_is_root (folder_win32->filename), NULL);
      return file_info_for_root_with_error (folder_win32->filename, error);
    }

  /* Get_info for normal files */

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

#if 0
  /* Skip this sanity check, as it fails for server share roots, where
   * dirname gets set to \\server\share\ and folder_win32->filename is
   * \\server\share. Also, should we do a casefolded comparison here,
   * too, anyway?
   */
  {
    gchar *dirname = get_parent_dir (filename);
    g_return_val_if_fail (strcmp (dirname, folder_win32->filename) == 0, NULL);
    g_free (dirname);
  }
#endif

  types = folder_win32->types;

  if (folder_win32->have_stat)
    {
      struct stat_info_entry *entry;
      gchar *basename;

      g_assert (folder_win32->stat_info != NULL);

      basename = g_path_get_basename (filename);
      entry = g_hash_table_lookup (folder_win32->stat_info, basename);

      if (!entry)
	{
	  if (!stat_with_error (filename, &wfad, error))
	    {
	      g_free (basename);
	      return NULL;
	    }

	  entry = create_stat_info_entry_and_emit_add (folder_win32, filename, basename, &wfad);
	}
      g_free (basename);

      info = create_file_info (folder_win32, filename, types, &entry->wfad, entry->mime_type);
      return info;
    }
  else
    {
      if (!stat_with_error (filename, &wfad, error))
	return NULL;

      if ((types & GTK_FILE_INFO_MIME_TYPE) != 0)
	mime_type = get_mime_type_for_file (filename, &wfad);
      else
	mime_type = NULL;

      info = create_file_info (folder_win32, filename, types, &wfad, mime_type);

      return info;
    }
}

static void
cb_list_children (gpointer key, gpointer value, gpointer user_data)
{
  GSList **children = user_data;
  *children = g_slist_prepend (*children, key);
}

static gboolean
gtk_file_folder_win32_list_children (GtkFileFolder  *folder,
				     GSList        **children,
				     GError        **error)
{
  GtkFileFolderWin32 *folder_win32 = GTK_FILE_FOLDER_WIN32 (folder);
  GSList *l;

  *children = NULL;

  /* Get the list of basenames.  */
  if (folder_win32->stat_info)
    g_hash_table_foreach (folder_win32->stat_info, cb_list_children, children);

  /* Turn basenames into GFilePaths.  */
  for (l = *children; l; l = l->next)
    {
      const char *basename = l->data;
      char *fullname = g_build_filename (folder_win32->filename, basename, NULL);
      l->data = filename_to_path (fullname);
      g_free (fullname);
    }

  return TRUE;
}

static gboolean
gtk_file_folder_win32_is_finished_loading (GtkFileFolder *folder)
{
  return GTK_FILE_FOLDER_WIN32 (folder)->is_finished_loading;
}

static void
free_stat_info_entry (struct stat_info_entry *entry)
{
  g_free (entry->mime_type);
  g_free (entry);
}

static gboolean
fill_in_names (GtkFileFolderWin32 *folder_win32, GError **error)
{
  GDir *dir;

  if (folder_win32->stat_info)
    return TRUE;

  dir = g_dir_open (folder_win32->filename, 0, error);
  if (!dir)
    return FALSE;

  folder_win32->stat_info = g_hash_table_new_full (casefolded_hash, casefolded_equal,
						  (GDestroyNotify) g_free,
						  (GDestroyNotify) free_stat_info_entry);

  while (TRUE)
    {
      struct stat_info_entry *entry;
      const gchar *basename;

      basename = g_dir_read_name (dir);
      if (!basename)
	break;

      entry = g_new0 (struct stat_info_entry, 1);
      if (folder_win32->is_network_dir)
	{
	  g_assert_not_reached ();
	  entry->wfad.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	  entry->mime_type = g_strdup ("x-directory/normal");
	}

      g_hash_table_insert (folder_win32->stat_info,
			   g_strdup (basename),
			   entry);
    }

  g_dir_close (dir);

  folder_win32->asof = time (NULL);
  return TRUE;
}

static gboolean
cb_fill_in_stats (gpointer key, gpointer value, gpointer user_data)
{
  const char *basename = key;
  struct stat_info_entry *entry = value;
  GtkFileFolderWin32 *folder_win32 = user_data;
  char *fullname = g_build_filename (folder_win32->filename, basename, NULL);
  gboolean result;

  if (!stat_with_error (fullname, &entry->wfad, NULL))
    result = TRUE;  /* Couldn't stat -- remove from hash.  */
  else
    result = FALSE;

  g_free (fullname);
  return result;
}


static void
fill_in_stats (GtkFileFolderWin32 *folder_win32)
{
  if (folder_win32->have_stat)
    return;

  if (!fill_in_names (folder_win32, NULL))
    return;

  if (!folder_win32->is_network_dir)
    g_hash_table_foreach_remove (folder_win32->stat_info,
				 cb_fill_in_stats,
				 folder_win32);

  folder_win32->have_stat = TRUE;
}

static gboolean
cb_fill_in_mime_type (gpointer key, gpointer value, gpointer user_data)
{
  const char *basename = key;
  struct stat_info_entry *entry = value;
  GtkFileFolderWin32 *folder_win32 = user_data;
  char *fullname = g_build_filename (folder_win32->filename, basename, NULL);

  entry->mime_type = get_mime_type_for_file (fullname, &entry->wfad);

  g_free (fullname);

  return FALSE;
}

static void
fill_in_mime_type (GtkFileFolderWin32 *folder_win32)
{
  if (folder_win32->have_mime_type)
    return;

  if (!folder_win32->have_stat)
    return;

  g_assert (folder_win32->stat_info != NULL);

  if (!folder_win32->is_network_dir)
    g_hash_table_foreach_remove (folder_win32->stat_info,
				 cb_fill_in_mime_type,
				 folder_win32);

  folder_win32->have_mime_type = TRUE;
}

static GtkFilePath *
filename_to_path (const char *filename)
{
  char *tmp;

  tmp = remove_trailing_slash (filename);
  return gtk_file_path_new_steal (tmp);
}

static gboolean
filename_is_root (const char *filename)
{
  const gchar *after_root;

  after_root = g_path_skip_root (filename);

  return (after_root != NULL && *after_root == '\0');
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
filename_is_some_root (const char *filename)
{
  return (g_path_is_absolute (filename) &&
	  *(g_path_skip_root (filename)) == '\0');
}

int
_gtk_file_system_win32_path_compare (const gchar *path1,
				     const gchar *path2)
{
  while (*path1 && *path2)
    {
      gunichar c1 = g_utf8_get_char (path1);
      gunichar c2 = g_utf8_get_char (path2);
      if (c1 == c2 ||
	  (G_IS_DIR_SEPARATOR (c1) && G_IS_DIR_SEPARATOR (c1)) ||
	  g_unichar_toupper (c1) == g_unichar_toupper (c2))
	{
	  path1 = g_utf8_next_char (path1);
	  path2 = g_utf8_next_char (path2);
	}
      else
	break;
    }
  if (!*path1 && !*path2)
    return 0;
  else if (!*path1)
    return -1;
  else if (!*path2)
    return 1;
  else
    return g_unichar_toupper (g_utf8_get_char (path1)) - g_unichar_toupper (g_utf8_get_char (path2));
}

#define __GTK_FILE_SYSTEM_WIN32_C__
#include "gtkaliasdef.c"
