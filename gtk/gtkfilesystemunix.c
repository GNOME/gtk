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

/* #define this if you want the program to crash when a file system gets
 * finalized while async handles are still outstanding.
 */
#undef HANDLE_ME_HARDER

#include <config.h>

#include "gtkfilesystem.h"
#include "gtkfilesystemunix.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkstock.h"
#include "gtkalias.h"

#define XDG_PREFIX _gtk_xdg
#include "xdgmime/xdgmime.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <time.h>

#define BOOKMARKS_FILENAME ".gtk-bookmarks"

#define HIDDEN_FILENAME ".hidden"

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

  /* For /afs and /net */
  struct stat afs_statbuf;
  struct stat net_statbuf;

  GHashTable *handles;

  guint execute_callbacks_idle_id;
  GSList *callbacks;

  guint have_afs : 1;
  guint have_net : 1;
};

/* Icon type, supplemented by MIME type
 */
typedef enum {
  ICON_UNDECIDED, /* Only used while we have not yet computed the icon in a struct stat_info_entry */
  ICON_NONE,      /* "Could not compute the icon type" */
  ICON_REGULAR,	  /* Use mime type for icon */
  ICON_BLOCK_DEVICE,
  ICON_BROKEN_SYMBOLIC_LINK,
  ICON_CHARACTER_DEVICE,
  ICON_DIRECTORY,
  ICON_EXECUTABLE,
  ICON_FIFO,
  ICON_SOCKET
} IconType;


#define GTK_TYPE_FILE_FOLDER_UNIX             (_gtk_file_folder_unix_get_type ())
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
  guint load_folder_id;
  guint have_stat : 1;
  guint have_mime_type : 1;
  guint is_network_dir : 1;
  guint have_hidden : 1;
  guint is_finished_loading : 1;
  time_t asof;
};

struct stat_info_entry {
  struct stat statbuf;
  char *mime_type;
  IconType icon_type;
  gboolean hidden;
};

static const GtkFileInfoType STAT_NEEDED_MASK = (GTK_FILE_INFO_IS_FOLDER |
						 GTK_FILE_INFO_MODIFICATION_TIME |
						 GTK_FILE_INFO_SIZE |
						 GTK_FILE_INFO_ICON);

static void gtk_file_system_unix_iface_init   (GtkFileSystemIface     *iface);
static void gtk_file_system_unix_dispose      (GObject                *object);
static void gtk_file_system_unix_finalize     (GObject                *object);

static GSList *             gtk_file_system_unix_list_volumes        (GtkFileSystem     *file_system);
static GtkFileSystemVolume *gtk_file_system_unix_get_volume_for_path (GtkFileSystem     *file_system,
								      const GtkFilePath *path);

static GtkFileSystemHandle *gtk_file_system_unix_get_folder (GtkFileSystem      *file_system,
							     const GtkFilePath  *path,
							     GtkFileInfoType     types,
							     GtkFileSystemGetFolderCallback  callback,
							     gpointer                        data);
static GtkFileSystemHandle *gtk_file_system_unix_get_info (GtkFileSystem *file_system,
							   const GtkFilePath *path,
							   GtkFileInfoType types,
							   GtkFileSystemGetInfoCallback callback,
							   gpointer data);
static GtkFileSystemHandle *gtk_file_system_unix_create_folder (GtkFileSystem      *file_system,
							        const GtkFilePath  *path,
								GtkFileSystemCreateFolderCallback callback,
								gpointer data);
static void         gtk_file_system_unix_cancel_operation        (GtkFileSystemHandle *handle);

static void         gtk_file_system_unix_volume_free             (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static GtkFilePath *gtk_file_system_unix_volume_get_base_path    (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static gboolean     gtk_file_system_unix_volume_get_is_mounted   (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static GtkFileSystemHandle *gtk_file_system_unix_volume_mount    (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume,
								  GtkFileSystemVolumeMountCallback callback,
								  gpointer data);
static gchar *      gtk_file_system_unix_volume_get_display_name (GtkFileSystem       *file_system,
								  GtkFileSystemVolume *volume);
static gchar *      gtk_file_system_unix_volume_get_icon_name    (GtkFileSystem        *file_system,
								  GtkFileSystemVolume  *volume,
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


static gboolean gtk_file_system_unix_insert_bookmark (GtkFileSystem     *file_system,
						      const GtkFilePath *path,
						      gint               position,
						      GError           **error);
static gboolean gtk_file_system_unix_remove_bookmark (GtkFileSystem     *file_system,
						      const GtkFilePath *path,
						      GError           **error);
static GSList * gtk_file_system_unix_list_bookmarks  (GtkFileSystem     *file_system);
static gchar  * gtk_file_system_unix_get_bookmark_label (GtkFileSystem     *file_system,
							 const GtkFilePath *path);
static void     gtk_file_system_unix_set_bookmark_label (GtkFileSystem     *file_system,
							 const GtkFilePath *path,
							 const gchar       *label);

static void  gtk_file_folder_unix_iface_init (GtkFileFolderIface     *iface);
static void  gtk_file_folder_unix_finalize   (GObject                *object);

static GtkFileInfo *gtk_file_folder_unix_get_info      (GtkFileFolder  *folder,
							const GtkFilePath    *path,
							GError        **error);
static gboolean     gtk_file_folder_unix_list_children (GtkFileFolder  *folder,
							GSList        **children,
							GError        **error);

static gboolean     gtk_file_folder_unix_is_finished_loading (GtkFileFolder *folder);

static GtkFilePath *filename_to_path   (const gchar       *filename);

static gboolean     filename_is_root  (const char       *filename);

static gboolean get_is_hidden_for_file (const char *filename,
					const char *basename);
static gboolean file_is_hidden (GtkFileFolderUnix *folder_unix,
				const char        *basename);

static GtkFileInfo *file_info_for_root_with_error (const char *root_name,
						   GError **error);
static gboolean     stat_with_error               (const char *filename,
						   struct stat *statbuf,
						   GError **error);
static GtkFileInfo *create_file_info              (GtkFileFolderUnix *folder_unix,
						   const char *filename,
						   const char *basename,
						   GtkFileInfoType types,
						   struct stat *statbuf,
						   const char *mime_type);

static gboolean execute_callbacks_idle (gpointer data);
static void execute_callbacks (gpointer data);

static gboolean fill_in_names     (GtkFileFolderUnix  *folder_unix,
				   GError            **error);
static void     fill_in_stats     (GtkFileFolderUnix  *folder_unix);
static void     fill_in_mime_type (GtkFileFolderUnix  *folder_unix);
static void     fill_in_hidden    (GtkFileFolderUnix  *folder_unix);

static gboolean cb_fill_in_stats     (gpointer key,
				      gpointer value,
				      gpointer user_data);
static gboolean cb_fill_in_mime_type (gpointer key,
				      gpointer value,
				      gpointer user_data);

static char *       get_parent_dir    (const char       *filename);

/*
 * GtkFileSystemUnix
 */
G_DEFINE_TYPE_WITH_CODE (GtkFileSystemUnix, gtk_file_system_unix, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_SYSTEM,
						gtk_file_system_unix_iface_init))

/*
 * GtkFileFolderUnix
 */
G_DEFINE_TYPE_WITH_CODE (GtkFileFolderUnix, _gtk_file_folder_unix, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_FOLDER,
						gtk_file_folder_unix_iface_init))


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

  gobject_class->dispose = gtk_file_system_unix_dispose;
  gobject_class->finalize = gtk_file_system_unix_finalize;
}

static void
gtk_file_system_unix_iface_init (GtkFileSystemIface *iface)
{
  iface->list_volumes = gtk_file_system_unix_list_volumes;
  iface->get_volume_for_path = gtk_file_system_unix_get_volume_for_path;
  iface->get_folder = gtk_file_system_unix_get_folder;
  iface->get_info = gtk_file_system_unix_get_info;
  iface->create_folder = gtk_file_system_unix_create_folder;
  iface->cancel_operation = gtk_file_system_unix_cancel_operation;
  iface->volume_free = gtk_file_system_unix_volume_free;
  iface->volume_get_base_path = gtk_file_system_unix_volume_get_base_path;
  iface->volume_get_is_mounted = gtk_file_system_unix_volume_get_is_mounted;
  iface->volume_mount = gtk_file_system_unix_volume_mount;
  iface->volume_get_display_name = gtk_file_system_unix_volume_get_display_name;
  iface->volume_get_icon_name = gtk_file_system_unix_volume_get_icon_name;
  iface->get_parent = gtk_file_system_unix_get_parent;
  iface->make_path = gtk_file_system_unix_make_path;
  iface->parse = gtk_file_system_unix_parse;
  iface->path_to_uri = gtk_file_system_unix_path_to_uri;
  iface->path_to_filename = gtk_file_system_unix_path_to_filename;
  iface->uri_to_path = gtk_file_system_unix_uri_to_path;
  iface->filename_to_path = gtk_file_system_unix_filename_to_path;
  iface->insert_bookmark = gtk_file_system_unix_insert_bookmark;
  iface->remove_bookmark = gtk_file_system_unix_remove_bookmark;
  iface->list_bookmarks = gtk_file_system_unix_list_bookmarks;
  iface->get_bookmark_label = gtk_file_system_unix_get_bookmark_label;
  iface->set_bookmark_label = gtk_file_system_unix_set_bookmark_label;
}

static void
gtk_file_system_unix_init (GtkFileSystemUnix *system_unix)
{
  system_unix->folder_hash = g_hash_table_new (g_str_hash, g_str_equal);

  if (stat ("/afs", &system_unix->afs_statbuf) == 0)
    system_unix->have_afs = TRUE;
  else
    system_unix->have_afs = FALSE;

  if (stat ("/net", &system_unix->net_statbuf) == 0)
    system_unix->have_net = TRUE;
  else
    system_unix->have_net = FALSE;

  system_unix->handles = g_hash_table_new (g_direct_hash, g_direct_equal);

  system_unix->execute_callbacks_idle_id = 0;
  system_unix->callbacks = NULL;
}

static void
check_handle_fn (gpointer key, gpointer value, gpointer data)
{
  GtkFileSystemHandle *handle;
  int *num_live_handles;

  handle = key;
  num_live_handles = data;

  (*num_live_handles)++;

  g_warning ("file_system_unix=%p still has handle=%p at finalization which is %s!",
	     handle->file_system,
	     handle,
	     handle->cancelled ? "CANCELLED" : "NOT CANCELLED");
}

static void
check_handles_at_finalization (GtkFileSystemUnix *system_unix)
{
  int num_live_handles;

  num_live_handles = 0;

  g_hash_table_foreach (system_unix->handles, check_handle_fn, &num_live_handles);
#ifdef HANDLE_ME_HARDER
  g_assert (num_live_handles == 0);
#endif

  g_hash_table_destroy (system_unix->handles);
  system_unix->handles = NULL;
}

#define GTK_TYPE_FILE_SYSTEM_HANDLE_UNIX (_gtk_file_system_handle_unix_get_type ())

typedef struct _GtkFileSystemHandle GtkFileSystemHandleUnix;
typedef struct _GtkFileSystemHandleClass GtkFileSystemHandleUnixClass;

G_DEFINE_TYPE (GtkFileSystemHandleUnix, _gtk_file_system_handle_unix, GTK_TYPE_FILE_SYSTEM_HANDLE)

static void
_gtk_file_system_handle_unix_init (GtkFileSystemHandleUnix *handle)
{
}

static void 
_gtk_file_system_handle_unix_finalize (GObject *object)
{
  GtkFileSystemHandleUnix *handle;
  GtkFileSystemUnix *system_unix;

  handle = (GtkFileSystemHandleUnix *)object;

  system_unix = GTK_FILE_SYSTEM_UNIX (GTK_FILE_SYSTEM_HANDLE (handle)->file_system);

  g_assert (g_hash_table_lookup (system_unix->handles, handle) != NULL);
  g_hash_table_remove (system_unix->handles, handle);

  if (G_OBJECT_CLASS (_gtk_file_system_handle_unix_parent_class)->finalize)
    G_OBJECT_CLASS (_gtk_file_system_handle_unix_parent_class)->finalize (object);
}

static void
_gtk_file_system_handle_unix_class_init (GtkFileSystemHandleUnixClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *) class;

  gobject_class->finalize = _gtk_file_system_handle_unix_finalize;
}

static void
gtk_file_system_unix_dispose (GObject *object)
{
  GtkFileSystemUnix *system_unix;

  system_unix = GTK_FILE_SYSTEM_UNIX (object);

  if (system_unix->execute_callbacks_idle_id)
    {
      g_source_remove (system_unix->execute_callbacks_idle_id);
      system_unix->execute_callbacks_idle_id = 0;

      /* call pending callbacks */
      execute_callbacks (system_unix);
    }

  G_OBJECT_CLASS (gtk_file_system_unix_parent_class)->dispose (object);
}

static void
gtk_file_system_unix_finalize (GObject *object)
{
  GtkFileSystemUnix *system_unix;

  system_unix = GTK_FILE_SYSTEM_UNIX (object);

  check_handles_at_finalization  (system_unix);

  /* FIXME: assert that the hash is empty? */
  g_hash_table_destroy (system_unix->folder_hash);

  G_OBJECT_CLASS (gtk_file_system_unix_parent_class)->finalize (object);
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

/* Delay callback dispatching
 */

enum callback_types
{
  CALLBACK_GET_INFO,
  CALLBACK_GET_FOLDER,
  CALLBACK_CREATE_FOLDER,
  CALLBACK_VOLUME_MOUNT
};

static void queue_callback (GtkFileSystemUnix *system_unix, enum callback_types type, gpointer data);

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

  queue_callback (GTK_FILE_SYSTEM_UNIX (handle->file_system), CALLBACK_GET_INFO, info);
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

  queue_callback (GTK_FILE_SYSTEM_UNIX (handle->file_system), CALLBACK_GET_FOLDER, info);
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

  queue_callback (GTK_FILE_SYSTEM_UNIX (handle->file_system), CALLBACK_CREATE_FOLDER, info);
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

  queue_callback (GTK_FILE_SYSTEM_UNIX (handle->file_system), CALLBACK_VOLUME_MOUNT, info);
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
  GtkFileSystemUnix *system_unix = GTK_FILE_SYSTEM_UNIX (data);

  if (!system_unix->execute_callbacks_idle_id)
    unref_file_system = FALSE;
  else
    g_object_ref (system_unix);

  for (l = system_unix->callbacks; l; l = l->next)
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

  g_slist_free (system_unix->callbacks);
  system_unix->callbacks = NULL;

  if (unref_file_system)
    g_object_unref (system_unix);

  system_unix->execute_callbacks_idle_id = 0;
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
queue_callback (GtkFileSystemUnix   *system_unix,
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

  system_unix->callbacks = g_slist_append (system_unix->callbacks, info);

  if (!system_unix->execute_callbacks_idle_id)
    system_unix->execute_callbacks_idle_id = g_idle_add (execute_callbacks_idle, system_unix);
}

static GtkFileSystemHandle *
create_handle (GtkFileSystem *file_system)
{
  GtkFileSystemUnix *system_unix;
  GtkFileSystemHandle *handle;

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);

  handle = g_object_new (GTK_TYPE_FILE_SYSTEM_HANDLE_UNIX, NULL);
  handle->file_system = file_system;

  g_assert (g_hash_table_lookup (system_unix->handles, handle) == NULL);
  g_hash_table_insert (system_unix->handles, handle, handle);

  return handle;
}



static GtkFileSystemHandle *
gtk_file_system_unix_get_info (GtkFileSystem                *file_system,
			       const GtkFilePath            *path,
			       GtkFileInfoType               types,
			       GtkFileSystemGetInfoCallback  callback,
			       gpointer                      data)
{
  GError *error = NULL;
  GtkFileSystemUnix *system_unix;
  GtkFileSystemHandle *handle;
  const char *filename;
  GtkFileInfo *info;
  gchar *basename;
  struct stat statbuf;
  const char *mime_type;

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);
  handle = create_handle (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  if (!stat_with_error (filename, &statbuf, &error))
    {
      g_object_ref (handle);
      queue_get_info_callback (callback, handle, NULL, error, data);
      return handle;
    }

  if ((types & GTK_FILE_INFO_MIME_TYPE) != 0)
    mime_type = xdg_mime_get_mime_type_for_file (filename, &statbuf);
  else
    mime_type = NULL;

  basename = g_path_get_basename (filename);

  info = create_file_info (NULL, filename, basename, types, &statbuf,
                           mime_type);
  g_free (basename);
  g_object_ref (handle);
  queue_get_info_callback (callback, handle, info, NULL, data);

  return handle;
}

static gboolean
load_folder (gpointer data)
{
  GtkFileFolderUnix *folder_unix = data;
  GSList *children;

  GDK_THREADS_ENTER ();

  if ((folder_unix->types & STAT_NEEDED_MASK) != 0)
    fill_in_stats (folder_unix);

  if ((folder_unix->types & GTK_FILE_INFO_MIME_TYPE) != 0)
    fill_in_mime_type (folder_unix);

  if (gtk_file_folder_unix_list_children (GTK_FILE_FOLDER (folder_unix), &children, NULL))
    {
      folder_unix->is_finished_loading = TRUE;
      g_signal_emit_by_name (folder_unix, "files-added", children);
      gtk_file_paths_free (children);
    }

  folder_unix->load_folder_id = 0;

  g_signal_emit_by_name (folder_unix, "finished-loading", 0);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static GtkFileSystemHandle *
gtk_file_system_unix_get_folder (GtkFileSystem                  *file_system,
				 const GtkFilePath              *path,
				 GtkFileInfoType                 types,
				 GtkFileSystemGetFolderCallback  callback,
				 gpointer                        data)
{
  GError *error = NULL;
  GtkFileSystemUnix *system_unix;
  GtkFileFolderUnix *folder_unix;
  GtkFileSystemHandle *handle;
  const char *filename;
  char *filename_copy;
  gboolean set_asof = FALSE;

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  handle = create_handle (file_system);

  filename_copy = remove_trailing_slash (filename);
  folder_unix = g_hash_table_lookup (system_unix->folder_hash, filename_copy);

  if (folder_unix)
    {
      g_free (filename_copy);
      if (folder_unix->stat_info &&
	  time (NULL) - folder_unix->asof >= FOLDER_CACHE_LIFETIME)
	{
#if 0
	  g_print ("Cleaning out cached directory %s\n", filename);
#endif
	  g_hash_table_destroy (folder_unix->stat_info);
	  folder_unix->stat_info = NULL;
	  folder_unix->have_mime_type = FALSE;
	  folder_unix->have_stat = FALSE;
	  folder_unix->have_hidden = FALSE;
	  set_asof = TRUE;
	}

      g_object_ref (folder_unix);
      folder_unix->types |= types;
      types = folder_unix->types;
    }
  else
    {
      struct stat statbuf;
      int result;
      int code;
      int my_errno;

      code = my_errno = 0; /* shut up GCC */

      result = stat (filename, &statbuf);

      if (result == 0)
	{
	  if (!S_ISDIR (statbuf.st_mode))
	    {
	      result = -1;
	      code = GTK_FILE_SYSTEM_ERROR_NOT_FOLDER;
	      my_errno = ENOTDIR;
	    }
	}
      else
	{
	  my_errno = errno;

	  if (my_errno == ENOENT)
	    code = GTK_FILE_SYSTEM_ERROR_NONEXISTENT;
	  else
	    code = GTK_FILE_SYSTEM_ERROR_FAILED;
	}

      if (result != 0)
	{
	  gchar *display_name = g_filename_display_name (filename);
	  g_set_error (&error,
		       GTK_FILE_SYSTEM_ERROR,
		       code,
		       _("Error getting information for '%s': %s"),
		       display_name,
		       g_strerror (my_errno));

	  g_object_ref (handle);
	  queue_get_folder_callback (callback, handle, NULL, error, data);

	  g_free (display_name);
	  g_free (filename_copy);
	  return handle;
	}

      folder_unix = g_object_new (GTK_TYPE_FILE_FOLDER_UNIX, NULL);
      folder_unix->system_unix = system_unix;
      folder_unix->filename = filename_copy;
      folder_unix->types = types;
      folder_unix->stat_info = NULL;
      folder_unix->load_folder_id = 0;
      folder_unix->have_mime_type = FALSE;
      folder_unix->have_stat = FALSE;
      folder_unix->have_hidden = FALSE;
      folder_unix->is_finished_loading = FALSE;
      set_asof = TRUE;
	  
      if ((system_unix->have_afs &&
	   system_unix->afs_statbuf.st_dev == statbuf.st_dev &&
	   system_unix->afs_statbuf.st_ino == statbuf.st_ino) ||
	  (system_unix->have_net &&
	   system_unix->net_statbuf.st_dev == statbuf.st_dev &&
	   system_unix->net_statbuf.st_ino == statbuf.st_ino))
	folder_unix->is_network_dir = TRUE;
      else
	folder_unix->is_network_dir = FALSE;

      g_hash_table_insert (system_unix->folder_hash,
			   folder_unix->filename,
			   folder_unix);
    }

  if (set_asof)
    folder_unix->asof = time (NULL);

  g_object_ref (handle);
  queue_get_folder_callback (callback, handle, GTK_FILE_FOLDER (folder_unix), NULL, data);

  /* Start loading the folder contents in an idle */
  if (!folder_unix->load_folder_id)
    folder_unix->load_folder_id =
      g_idle_add ((GSourceFunc) load_folder, folder_unix);

  return handle;
}

static GtkFileSystemHandle *
gtk_file_system_unix_create_folder (GtkFileSystem                     *file_system,
				    const GtkFilePath                 *path,
				    GtkFileSystemCreateFolderCallback  callback,
				    gpointer                           data)
{
  GError *error = NULL;
  GtkFileSystemUnix *system_unix;
  GtkFileSystemHandle *handle;
  const char *filename;
  gboolean result;
  char *parent, *tmp;
  int save_errno = errno;

  system_unix = GTK_FILE_SYSTEM_UNIX (file_system);

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  handle = create_handle (file_system);

  tmp = remove_trailing_slash (filename);
  errno = 0;
  result = mkdir (tmp, 0777) == 0;
  save_errno = errno;
  g_free (tmp);

  if (!result)
    {
      gchar *display_name = g_filename_display_name (filename);
      g_set_error (&error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		   _("Error creating directory '%s': %s"),
		   display_name,
		   g_strerror (save_errno));
      
      g_object_ref (handle);
      queue_create_folder_callback (callback, handle, path, error, data);

      g_free (display_name);
      return handle;
    }

  g_object_ref (handle);
  queue_create_folder_callback (callback, handle, path, NULL, data);

  parent = get_parent_dir (filename);
  if (parent)
    {
      GtkFileFolderUnix *folder_unix;

      folder_unix = g_hash_table_lookup (system_unix->folder_hash, parent);
      if (folder_unix)
	{
	  GSList *paths;
	  char *basename;
	  struct stat_info_entry *entry;

	  /* Make sure the new folder exists in the parent's folder */
	  entry = g_new0 (struct stat_info_entry, 1);
	  if (folder_unix->is_network_dir)
	    {
	      entry->statbuf.st_mode = S_IFDIR;
	      entry->mime_type = g_strdup ("x-directory/normal");
	    }

	  basename = g_path_get_basename (filename);
	  g_hash_table_insert (folder_unix->stat_info,
			       basename,
			       entry);

	  if (folder_unix->have_stat)
	    {
	      /* Cheating */
	      if ((folder_unix->types & STAT_NEEDED_MASK) != 0)
		cb_fill_in_stats (basename, entry, folder_unix);

	      if ((folder_unix->types & GTK_FILE_INFO_MIME_TYPE) != 0)
		cb_fill_in_mime_type (basename, entry, folder_unix);
	    }

	  paths = g_slist_append (NULL, (GtkFilePath *) path);
	  g_signal_emit_by_name (folder_unix, "files-added", paths);
	  g_slist_free (paths);
	}

      g_free (parent);
    }

  return handle;
}

static void
gtk_file_system_unix_cancel_operation (GtkFileSystemHandle *handle)
{
  /* We don't set "cancelled" to TRUE here, since the actual operation
   * is executed in the function itself and not in a callback.  So
   * the operations can never be cancelled (since they will be already
   * completed at this point.
   */
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

static GtkFileSystemHandle *
gtk_file_system_unix_volume_mount (GtkFileSystem                    *file_system,
				   GtkFileSystemVolume              *volume,
				   GtkFileSystemVolumeMountCallback  callback,
				   gpointer                          data)
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
gtk_file_system_unix_volume_get_display_name (GtkFileSystem       *file_system,
					      GtkFileSystemVolume *volume)
{
  return g_strdup (_("File System")); /* Same as Nautilus */
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
#ifdef S_ISFIFO
  else if (S_ISFIFO (statp->st_mode))
    return  ICON_FIFO;
#endif
#ifdef S_ISSOCK
  else if (S_ISSOCK (statp->st_mode))
    return ICON_SOCKET;
#endif
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
	  gchar *display_name = g_filename_display_name (filename);
	  g_set_error (error,
		       GTK_FILE_SYSTEM_ERROR,
		       GTK_FILE_SYSTEM_ERROR_NONEXISTENT,
		       _("Error getting information for '%s': %s"),
		       display_name,
		       g_strerror (save_errno));
	  g_free (display_name);

	  return ICON_NONE;
	}
    }

  return get_icon_type_from_stat (&statbuf);
}

/* Renders a fallback icon from the stock system */
static const gchar *
get_fallback_icon_name (IconType icon_type)
{
  const char *stock_name;

  switch (icon_type)
    {
    case ICON_BLOCK_DEVICE:
      stock_name = GTK_STOCK_HARDDISK;
      break;

    case ICON_DIRECTORY:
      stock_name = GTK_STOCK_DIRECTORY;
      break;

    case ICON_EXECUTABLE:
      stock_name = GTK_STOCK_EXECUTE;
      break;

    default:
      stock_name = GTK_STOCK_FILE;
      break;
    }

  return stock_name;
}

static gchar *
gtk_file_system_unix_volume_get_icon_name (GtkFileSystem        *file_system,
					   GtkFileSystemVolume  *volume,
					   GError              **error)
{
  /* FIXME: maybe we just always want to return GTK_STOCK_HARDDISK here?
   * or the new tango icon name?
   */
  return g_strdup ("gnome-dev-harddisk");
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

  if (strchr (display_name, G_DIR_SEPARATOR))
    {
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
		   _("The name \"%s\" is not valid because it contains the character \"%s\". "
		     "Please use a different name."),
		   display_name,
		   G_DIR_SEPARATOR_S);
      return NULL;
    }

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

/* Takes a user-typed filename and expands a tilde at the beginning of the string */
static char *
expand_tilde (const char *filename)
{
  const char *notilde;
  const char *slash;
  const char *home;

  if (filename[0] != '~')
    return g_strdup (filename);

  notilde = filename + 1;

  slash = strchr (notilde, G_DIR_SEPARATOR);
  if (!slash)
    return NULL;

  if (slash == notilde)
    {
      home = g_get_home_dir ();

      if (!home)
	return g_strdup (filename);
    }
  else
    {
      char *username;
      struct passwd *passwd;

      username = g_strndup (notilde, slash - notilde);
      passwd = getpwnam (username);
      g_free (username);

      if (!passwd)
	return g_strdup (filename);

      home = passwd->pw_dir;
    }

  return g_build_filename (home, G_DIR_SEPARATOR_S, slash + 1, NULL);
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
  gchar *filename;
  gchar *last_slash;
  gboolean result = FALSE;

  base_filename = gtk_file_path_get_string (base_path);
  g_return_val_if_fail (base_filename != NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (base_filename), FALSE);

  filename = expand_tilde (str);
  if (!filename)
    {
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_BAD_FILENAME,
		   "%s", ""); /* nothing for now, as we are string-frozen */
      return FALSE;
    }

  last_slash = strrchr (filename, G_DIR_SEPARATOR);
  if (!last_slash)
    {
      *folder = gtk_file_path_copy (base_path);
      *file_part = g_strdup (filename);
      result = TRUE;
    }
  else
    {
      gchar *folder_part;
      gchar *folder_path;
      GError *tmp_error = NULL;

      if (last_slash == filename)
	folder_part = g_strdup ("/");
      else
	folder_part = g_filename_from_utf8 (filename, last_slash - filename,
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

  g_free (filename);

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
  GtkFilePath *path;
  gchar *filename = g_filename_from_uri (uri, NULL, NULL);

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
gtk_file_system_unix_filename_to_path (GtkFileSystem *file_system,
				       const gchar   *filename)
{
  return filename_to_path (filename);
}

/* Returns the name of the icon to be used for a path which is known to be a
 * directory.  This can vary for Home, Desktop, etc.
 */
static const char *
get_icon_name_for_directory (const char *path)
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

  return NULL;
}

/* Computes our internal icon type based on a path name; also returns the MIME
 * type in case we come up with ICON_REGULAR.
 */
static IconType
get_icon_type_from_path (GtkFileFolderUnix *folder_unix,
			 struct stat       *statbuf,
			 const char        *filename,
			 const char       **mime_type)
{
  IconType icon_type;

  *mime_type = NULL;

  if (folder_unix && folder_unix->have_stat)
    {
      char *basename;
      struct stat_info_entry *entry;

      g_assert (folder_unix->stat_info != NULL);

      basename = g_path_get_basename (filename);
      entry = g_hash_table_lookup (folder_unix->stat_info, basename);
      g_free (basename);
      if (entry)
	{
	  if (entry->icon_type == ICON_UNDECIDED)
	    {
	      entry->icon_type = get_icon_type_from_stat (&entry->statbuf);
	      g_assert (entry->icon_type != ICON_UNDECIDED);
	    }
	  icon_type = entry->icon_type;
	  if (icon_type == ICON_REGULAR)
	    {
	      fill_in_mime_type (folder_unix);
	      *mime_type = entry->mime_type;
	    }

	  return icon_type;
	}
    }

  if (statbuf)
    return get_icon_type_from_stat (statbuf);

  icon_type = get_icon_type (filename, NULL);
  if (icon_type == ICON_REGULAR)
    *mime_type = xdg_mime_get_mime_type_for_file (filename, NULL);

  return icon_type;
}

/* Renders an icon for a non-ICON_REGULAR file */
static const gchar *
get_special_icon_name (IconType           icon_type,
		       const gchar       *filename)
{
  const char *name;

  g_assert (icon_type != ICON_REGULAR);

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
      /* get_icon_name_for_directory() returns a dupped string */
      return get_icon_name_for_directory (filename);
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

  return name;
}

static gchar *
get_icon_name_for_mime_type (const char *mime_type)
{
  char *name;
  const char *separator;
  GString *icon_name;

  if (!mime_type)
    return NULL;

  separator = strchr (mime_type, '/');
  if (!separator)
    return NULL; /* maybe we should return a GError with "invalid MIME-type" */

  /* FIXME: we default to the gnome icon naming for now.  Some question
   * as below, how are we going to handle a second attempt?
   */
#if 0
  icon_name = g_string_new ("");
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  g_string_append_c (icon_name, '-');
  g_string_append (icon_name, separator + 1);
  pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
  g_string_free (icon_name, TRUE);
  if (pixbuf)
    return pixbuf;

  icon_name = g_string_new ("");
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  g_string_append (icon_name, "-x-generic");
  pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
  g_string_free (icon_name, TRUE);
  if (pixbuf)
    return pixbuf;
#endif

  icon_name = g_string_new ("gnome-mime-");
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  g_string_append_c (icon_name, '-');
  g_string_append (icon_name, separator + 1);
  name = icon_name->str;
  g_string_free (icon_name, FALSE);

  return name;

  /* FIXME: how are we going to implement a second attempt? */
#if 0
  if (pixbuf)
    return pixbuf;

  icon_name = g_string_new ("gnome-mime-");
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  pixbuf = get_cached_icon (widget, icon_name->str, pixel_size);
  g_string_free (icon_name, TRUE);

  return pixbuf;
#endif
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
      char *bookmark, *space;

      bookmark = (char *)l->data;
      space = strchr (bookmark, ' ');
      if (space)
	*space = '\0';

      if (is_local_uri (bookmark))
	result = g_slist_prepend (result, gtk_file_system_unix_uri_to_path (file_system, bookmark));
    }

  bookmark_list_free (bookmarks);

  result = g_slist_reverse (result);
  return result;
}

static gchar *
gtk_file_system_unix_get_bookmark_label (GtkFileSystem     *file_system,
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
gtk_file_system_unix_set_bookmark_label (GtkFileSystem     *file_system,
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
_gtk_file_folder_unix_class_init (GtkFileFolderUnixClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gtk_file_folder_unix_finalize;
}

static void
gtk_file_folder_unix_iface_init (GtkFileFolderIface *iface)
{
  iface->get_info = gtk_file_folder_unix_get_info;
  iface->list_children = gtk_file_folder_unix_list_children;
  iface->is_finished_loading = gtk_file_folder_unix_is_finished_loading;
}

static void
_gtk_file_folder_unix_init (GtkFileFolderUnix *impl)
{
}

static void
gtk_file_folder_unix_finalize (GObject *object)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (object);

  if (folder_unix->load_folder_id)
    {
      g_source_remove (folder_unix->load_folder_id);
      folder_unix->load_folder_id = 0;
    }

  g_hash_table_remove (folder_unix->system_unix->folder_hash, folder_unix->filename);

  if (folder_unix->stat_info)
    {
#if 0
      g_print ("Releasing information for directory %s\n", folder_unix->filename);
#endif
      g_hash_table_destroy (folder_unix->stat_info);
    }

  g_free (folder_unix->filename);

  G_OBJECT_CLASS (_gtk_file_folder_unix_parent_class)->finalize (object);
}

/* Creates a GtkFileInfo for "/" by stat()ing it */
static GtkFileInfo *
file_info_for_root_with_error (const char  *root_name,
			       GError     **error)
{
  struct stat statbuf;
  GtkFileInfo *info;

  if (stat (root_name, &statbuf) != 0)
    {
      int saved_errno;

      saved_errno = errno; 
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   GTK_FILE_SYSTEM_ERROR_FAILED,
		   _("Error getting information for '%s': %s"),
		   "/", g_strerror (saved_errno));

      return NULL;
    }

  info = gtk_file_info_new ();
  gtk_file_info_set_display_name (info, "/");
  gtk_file_info_set_is_folder (info, TRUE);
  gtk_file_info_set_is_hidden (info, FALSE);
  gtk_file_info_set_mime_type (info, "x-directory/normal");
  gtk_file_info_set_modification_time (info, statbuf.st_mtime);
  gtk_file_info_set_size (info, statbuf.st_size);

  return info;
}

static gboolean
stat_with_error (const char   *filename,
		 struct stat  *statbuf,
		 GError      **error)
{
  if (stat (filename, statbuf) == -1 &&
      (errno != ENOENT || lstat (filename, statbuf) == -1))
    {
      int saved_errno;
      int code;
      char *display_name;

      saved_errno = errno;

      if (saved_errno == ENOENT)
	code = GTK_FILE_SYSTEM_ERROR_NONEXISTENT;
      else
	code = GTK_FILE_SYSTEM_ERROR_FAILED;

      display_name = g_filename_display_name (filename);
      g_set_error (error,
		   GTK_FILE_SYSTEM_ERROR,
		   code,
		   _("Error getting information for '%s': %s"),
		   display_name,
		   g_strerror (saved_errno));

      g_free (display_name);
      return FALSE;
    }

  return TRUE;
}

/* Creates a new GtkFileInfo from the specified data */
static GtkFileInfo *
create_file_info (GtkFileFolderUnix *folder_unix,
                  const char        *filename,
		  const char        *basename,
		  GtkFileInfoType    types,
		  struct stat       *statbuf,
		  const char        *mime_type)
{
  GtkFileInfo *info;

  info = gtk_file_info_new ();
  
  if (types & GTK_FILE_INFO_DISPLAY_NAME)
    {
      gchar *display_name = g_filename_display_basename (filename);
      gtk_file_info_set_display_name (info, display_name);
      g_free (display_name);
    }

  if (types & GTK_FILE_INFO_IS_HIDDEN)
    {
      if (folder_unix)
        {
          if (file_is_hidden (folder_unix, basename))
	    gtk_file_info_set_is_hidden (info, TRUE);
	}
      else
        {
	  if (get_is_hidden_for_file (filename, basename))
	    gtk_file_info_set_is_hidden (info, TRUE);
        }
    }

  if (types & GTK_FILE_INFO_IS_FOLDER)
    gtk_file_info_set_is_folder (info, S_ISDIR (statbuf->st_mode));

  if (types & GTK_FILE_INFO_MIME_TYPE)
    gtk_file_info_set_mime_type (info, mime_type);

  if (types & GTK_FILE_INFO_MODIFICATION_TIME)
    gtk_file_info_set_modification_time (info, statbuf->st_mtime);

  if (types & GTK_FILE_INFO_SIZE)
    gtk_file_info_set_size (info, (gint64) statbuf->st_size);

  if (types & GTK_FILE_INFO_ICON)
    {
      IconType icon_type;
      gboolean free_icon_name = FALSE;
      const char *icon_name;
      const char *icon_mime_type;

      icon_type = get_icon_type_from_path (folder_unix, statbuf, filename, &icon_mime_type);

      switch (icon_type)
        {
          case ICON_NONE:
	    icon_name = get_fallback_icon_name (icon_type);
	    break;

          case ICON_REGULAR:
	    free_icon_name = TRUE;
	    if (icon_mime_type)
	      icon_name = get_icon_name_for_mime_type (icon_mime_type);
	    else
	      icon_name = get_icon_name_for_mime_type (mime_type);
            break;

          default:
	    icon_name = get_special_icon_name (icon_type, filename);
	    break;
        }

      gtk_file_info_set_icon_name (info, icon_name);

      if (free_icon_name)
	g_free ((char *) icon_name);
    }

  return info;
}

static struct stat_info_entry *
create_stat_info_entry_and_emit_add (GtkFileFolderUnix *folder_unix,
				     const char        *filename,
				     const char        *basename,
				     struct stat       *statbuf)
{
  GSList *paths;
  GtkFilePath *path;
  struct stat_info_entry *entry;

  entry = g_new0 (struct stat_info_entry, 1);

  if ((folder_unix->types & STAT_NEEDED_MASK) != 0)
    entry->statbuf = *statbuf;

  if ((folder_unix->types & GTK_FILE_INFO_MIME_TYPE) != 0)
    entry->mime_type = g_strdup (xdg_mime_get_mime_type_for_file (filename, statbuf));

  g_hash_table_insert (folder_unix->stat_info,
		       g_strdup (basename),
		       entry);

  path = gtk_file_path_new_dup (filename);
  paths = g_slist_append (NULL, path);
  g_signal_emit_by_name (folder_unix, "files-added", paths);
  gtk_file_path_free (path);
  g_slist_free (paths);

  return entry;
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
  GtkFileInfoType types;
  struct stat statbuf;
  const char *mime_type;

  /* Get_info for "/" */
  if (!path)
    {
      g_return_val_if_fail (filename_is_root (folder_unix->filename), NULL);
      return file_info_for_root_with_error (folder_unix->filename, error);
    }

  /* Get_info for normal files */

  filename = gtk_file_path_get_string (path);
  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);

  dirname = get_parent_dir (filename);
  g_return_val_if_fail (strcmp (dirname, folder_unix->filename) == 0, NULL);
  g_free (dirname);

  basename = g_path_get_basename (filename);
  types = folder_unix->types;

  if (folder_unix->have_stat)
    {
      struct stat_info_entry *entry;

      g_assert (folder_unix->stat_info != NULL);
      entry = g_hash_table_lookup (folder_unix->stat_info, basename);

      if (!entry)
	{
	  if (!stat_with_error (filename, &statbuf, error))
	    {
	      g_free (basename);
	      return NULL;
	    }

	  entry = create_stat_info_entry_and_emit_add (folder_unix, filename, basename, &statbuf);
	}

      info = create_file_info (folder_unix, filename, basename, types, &entry->statbuf, entry->mime_type);
      g_free (basename);
      return info;
    }
  else
    {
      if (!stat_with_error (filename, &statbuf, error))
	{
	  g_free (basename);
	  return NULL;
	}

      if ((types & GTK_FILE_INFO_MIME_TYPE) != 0)
	mime_type = xdg_mime_get_mime_type_for_file (filename, &statbuf);
      else
	mime_type = NULL;

      info = create_file_info (folder_unix, filename, basename, types, &statbuf, mime_type);
      g_free (basename);
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
gtk_file_folder_unix_list_children (GtkFileFolder  *folder,
				    GSList        **children,
				    GError        **error)
{
  GtkFileFolderUnix *folder_unix = GTK_FILE_FOLDER_UNIX (folder);
  GSList *l;

  *children = NULL;

  /* Get the list of basenames.  */
  if (folder_unix->stat_info)
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

static gboolean
gtk_file_folder_unix_is_finished_loading (GtkFileFolder *folder)
{
  return GTK_FILE_FOLDER_UNIX (folder)->is_finished_loading;
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

  dir = g_dir_open (folder_unix->filename, 0, error);
  if (!dir)
    return FALSE;

  folder_unix->stat_info = g_hash_table_new_full (g_str_hash, g_str_equal,
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
      if (folder_unix->is_network_dir)
	{
	  entry->statbuf.st_mode = S_IFDIR;
	  entry->mime_type = g_strdup ("x-directory/normal");
	}

      g_hash_table_insert (folder_unix->stat_info,
			   g_strdup (basename),
			   entry);
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


static void
fill_in_stats (GtkFileFolderUnix *folder_unix)
{
  if (folder_unix->have_stat)
    return;

  if (!fill_in_names (folder_unix, NULL))
    return;

  if (!folder_unix->is_network_dir)
    g_hash_table_foreach_remove (folder_unix->stat_info,
				 cb_fill_in_stats,
				 folder_unix);

  folder_unix->have_stat = TRUE;
}


static gboolean
cb_fill_in_mime_type (gpointer key, gpointer value, gpointer user_data)
{
  const char *basename = key;
  struct stat_info_entry *entry = value;
  GtkFileFolderUnix *folder_unix = user_data;
  char *fullname = g_build_filename (folder_unix->filename, basename, NULL);
  struct stat *statbuf = NULL;
  const char *mime_type;

  if (folder_unix->have_stat)
    statbuf = &entry->statbuf;

  mime_type = xdg_mime_get_mime_type_for_file (fullname, statbuf);
  entry->mime_type = g_strdup (mime_type);

  g_free (fullname);

  return FALSE;
}

static void
fill_in_mime_type (GtkFileFolderUnix *folder_unix)
{
  if (folder_unix->have_mime_type)
    return;

  if (!folder_unix->have_stat)
    return;

  g_assert (folder_unix->stat_info != NULL);

  if (!folder_unix->is_network_dir)
    g_hash_table_foreach_remove (folder_unix->stat_info,
				 cb_fill_in_mime_type,
				 folder_unix);

  folder_unix->have_mime_type = TRUE;
}

static gchar **
read_hidden_file (const char *dirname)
{
  gchar **lines = NULL;
  gchar *contents;
  gchar *hidden_file;

  hidden_file = g_build_filename (dirname, HIDDEN_FILENAME, NULL);

  if (g_file_get_contents (hidden_file, &contents, NULL, NULL))
    {
      lines = g_strsplit (contents, "\n", -1);
      g_free (contents);
    }

  g_free (hidden_file);

  return lines;
}

static void
fill_in_hidden (GtkFileFolderUnix *folder_unix)
{
  gchar **lines;
  
  if (folder_unix->have_hidden)
    return;

  lines = read_hidden_file (folder_unix->filename);

  if (lines)
    {
      int i;
      
      for (i = 0; lines[i]; i++)
	{
	  if (lines[i][0])
	    {
	      struct stat_info_entry *entry;
	      
	      entry = g_hash_table_lookup (folder_unix->stat_info, lines[i]);
	      if (entry != NULL)
		entry->hidden = TRUE;
	    }
	}
      
      g_strfreev (lines);
    }

  folder_unix->have_hidden = TRUE;
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
get_is_hidden_for_file (const char *filename,
			const char *basename)
{
  gchar *dirname;
  gchar **lines;
  gboolean hidden = FALSE;

  dirname = g_path_get_dirname (filename);
  lines = read_hidden_file (dirname);
  g_free (dirname);

  if (lines)
    {
      int i;
      
      for (i = 0; lines[i]; i++)
	{
	  if (lines[i][0] && strcmp (lines[i], basename) == 0)
	    {
	      hidden = TRUE;
	      break;
	    }
	}
      
      g_strfreev (lines);
    }

  return hidden;
}

static gboolean
file_is_hidden (GtkFileFolderUnix *folder_unix,
		const char        *basename)
{
 struct stat_info_entry *entry;

  if (basename[0] == '.' || basename[strlen (basename) - 1] == '~')
    return TRUE;
 
  if (folder_unix->have_stat)
    {
      fill_in_hidden (folder_unix);
      
      entry = g_hash_table_lookup (folder_unix->stat_info, basename);
  
      if (entry)
	return entry->hidden;
    }

  return FALSE;
}

#define __GTK_FILE_SYSTEM_UNIX_C__
#include "gtkaliasdef.c"
