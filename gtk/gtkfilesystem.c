/* GTK - The GIMP Toolkit
 * gtkfilesystem.c: Filesystem abstraction functions.
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2007-2008 Carlos Garnacho
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Garnacho <carlos@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "gtkfilechooser.h"
#include "gtkfilesystem.h"
#include "gtkicontheme.h"
#include "gtkprivate.h"

/* #define DEBUG_MODE */
#ifdef DEBUG_MODE
#define DEBUG(x) g_debug (x);
#else
#define DEBUG(x)
#endif

#define FILES_PER_QUERY 100

/* The pointers we return for a GtkFileSystemVolume are opaque tokens; they are
 * really pointers to GDrive, GVolume or GMount objects.  We need an extra
 * token for the fake "File System" volume.  So, we'll return a pointer to
 * this particular string.
 */
static const gchar *root_volume_token = N_("File System");
#define IS_ROOT_VOLUME(volume) ((gpointer) (volume) == (gpointer) root_volume_token)

enum {
  PROP_0,
  PROP_FILE,
  PROP_ENUMERATOR,
  PROP_ATTRIBUTES
};

enum {
  VOLUMES_CHANGED,
  FS_LAST_SIGNAL
};

enum {
  FILES_ADDED,
  FILES_REMOVED,
  FILES_CHANGED,
  FINISHED_LOADING,
  DELETED,
  FOLDER_LAST_SIGNAL
};

static guint fs_signals [FS_LAST_SIGNAL] = { 0, };

typedef struct AsyncFuncData AsyncFuncData;

struct GtkFileSystemPrivate
{
  GVolumeMonitor *volume_monitor;

  /* This list contains elements that can be
   * of type GDrive, GVolume and GMount
   */
  GSList *volumes;
};

struct AsyncFuncData
{
  GtkFileSystem *file_system;
  GFile *file;
  GCancellable *cancellable;
  gchar *attributes;

  gpointer callback;
  gpointer data;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkFileSystem, _gtk_file_system, G_TYPE_OBJECT)


/* GtkFileSystem methods */
static void
volumes_changed (GVolumeMonitor *volume_monitor,
		 gpointer        volume,
		 gpointer        user_data)
{
  GtkFileSystem *file_system;

  gdk_threads_enter ();

  file_system = GTK_FILE_SYSTEM (user_data);
  g_signal_emit (file_system, fs_signals[VOLUMES_CHANGED], 0, volume);
  gdk_threads_leave ();
}

static void
gtk_file_system_dispose (GObject *object)
{
  GtkFileSystem *file_system = GTK_FILE_SYSTEM (object);
  GtkFileSystemPrivate *priv = file_system->priv;

  DEBUG ("dispose");

  if (priv->volumes)
    {
      g_slist_foreach (priv->volumes, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->volumes);
      priv->volumes = NULL;
    }

  if (priv->volume_monitor)
    {
      g_signal_handlers_disconnect_by_func (priv->volume_monitor, volumes_changed, object);
      g_object_unref (priv->volume_monitor);
      priv->volume_monitor = NULL;
    }

  G_OBJECT_CLASS (_gtk_file_system_parent_class)->dispose (object);
}

static void
_gtk_file_system_class_init (GtkFileSystemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_file_system_dispose;

  fs_signals[VOLUMES_CHANGED] =
    g_signal_new ("volumes-changed",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkFileSystemClass, volumes_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static gboolean
mount_referenced_by_volume_activation_root (GList *volumes, GMount *mount)
{
  GList *l;
  GFile *mount_root;
  gboolean ret;

  ret = FALSE;

  mount_root = g_mount_get_root (mount);

  for (l = volumes; l != NULL; l = l->next)
    {
      GVolume *volume = G_VOLUME (l->data);
      GFile *volume_activation_root;

      volume_activation_root = g_volume_get_activation_root (volume);
      if (volume_activation_root != NULL)
        {
          if (g_file_has_prefix (volume_activation_root, mount_root))
            {
              ret = TRUE;
              g_object_unref (volume_activation_root);
              break;
            }
          g_object_unref (volume_activation_root);
        }
    }

  g_object_unref (mount_root);
  return ret;
}

static void
get_volumes_list (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv = file_system->priv;
  GList *l, *ll;
  GList *drives;
  GList *volumes;
  GList *mounts;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;

  if (priv->volumes)
    {
      g_slist_foreach (priv->volumes, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->volumes);
      priv->volumes = NULL;
    }

  /* first go through all connected drives */
  drives = g_volume_monitor_get_connected_drives (priv->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    {
      drive = l->data;
      volumes = g_drive_get_volumes (drive);

      if (volumes)
        {
          for (ll = volumes; ll != NULL; ll = ll->next)
            {
              volume = ll->data;
              mount = g_volume_get_mount (volume);

              if (mount)
                {
                  /* Show mounted volume */
                  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
                  g_object_unref (mount);
                }
              else
                {
                  /* Do show the unmounted volumes in the sidebar;
                   * this is so the user can mount it (in case automounting
                   * is off).
                   *
                   * Also, even if automounting is enabled, this gives a visual
                   * cue that the user should remember to yank out the media if
                   * he just unmounted it.
                   */
                  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (volume));
                }

	      g_object_unref (volume);
            }
  
           g_list_free (volumes);
        }
      else if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive))
	{
	  /* If the drive has no mountable volumes and we cannot detect media change.. we
	   * display the drive in the sidebar so the user can manually poll the drive by
	   * right clicking and selecting "Rescan..."
	   *
	   * This is mainly for drives like floppies where media detection doesn't
	   * work.. but it's also for human beings who like to turn off media detection
	   * in the OS to save battery juice.
	   */

	  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (drive));
	}

      g_object_unref (drive);
    }

  g_list_free (drives);

  /* add all volumes that is not associated with a drive */
  volumes = g_volume_monitor_get_volumes (priv->volume_monitor);

  for (l = volumes; l != NULL; l = l->next)
    {
      volume = l->data;
      drive = g_volume_get_drive (volume);

      if (drive)
        {
          g_object_unref (drive);
          continue;
        }

      mount = g_volume_get_mount (volume);

      if (mount)
        {
          /* show this mount */
          priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
          g_object_unref (mount);
        }
      else
        {
          /* see comment above in why we add an icon for a volume */
          priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (volume));
        }

      g_object_unref (volume);
    }

  /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
  mounts = g_volume_monitor_get_mounts (priv->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      mount = l->data;
      volume = g_mount_get_volume (mount);

      if (volume)
        {
          g_object_unref (volume);
          continue;
        }

      /* if there's exists one or more volumes with an activation root inside the mount,
       * don't display the mount
       */
      if (mount_referenced_by_volume_activation_root (volumes, mount))
        {
          g_object_unref (mount);
          continue;
        }

      /* show this mount */
      priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
      g_object_unref (mount);
    }

  g_list_free (volumes);

  g_list_free (mounts);
}

static void
_gtk_file_system_init (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv;

  DEBUG ("init");

  file_system->priv = priv = _gtk_file_system_get_instance_private (file_system);

  /* Volumes */
  priv->volume_monitor = g_volume_monitor_get ();

  g_signal_connect (priv->volume_monitor, "mount-added",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "mount-removed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "mount-changed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-added",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-removed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-changed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-connected",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-disconnected",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-changed",
		    G_CALLBACK (volumes_changed), file_system);
}

/* GtkFileSystem public methods */
GtkFileSystem *
_gtk_file_system_new (void)
{
  return g_object_new (GTK_TYPE_FILE_SYSTEM, NULL);
}

GSList *
_gtk_file_system_list_volumes (GtkFileSystem *file_system)
{
  GtkFileSystemPrivate *priv = file_system->priv;
  GSList *list;

  DEBUG ("list_volumes");

  get_volumes_list (file_system);

  list = g_slist_copy (priv->volumes);

#ifndef G_OS_WIN32
  /* Prepend root volume */
  list = g_slist_prepend (list, (gpointer) root_volume_token);
#endif

  return list;
}

static void
free_async_data (AsyncFuncData *async_data)
{
  g_object_unref (async_data->file_system);
  g_object_unref (async_data->file);
  g_object_unref (async_data->cancellable);

  g_free (async_data->attributes);
  g_free (async_data);
}

static void
query_info_callback (GObject      *source_object,
		     GAsyncResult *result,
		     gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;
  GFileInfo *file_info;
  GFile *file;

  DEBUG ("query_info_callback");

  file = G_FILE (source_object);
  async_data = (AsyncFuncData *) user_data;
  file_info = g_file_query_info_finish (file, result, &error);

  if (async_data->callback)
    {
      gdk_threads_enter ();
      ((GtkFileSystemGetInfoCallback) async_data->callback) (async_data->cancellable,
							     file_info, error, async_data->data);
      gdk_threads_leave ();
    }

  if (file_info)
    g_object_unref (file_info);

  if (error)
    g_error_free (error);

  free_async_data (async_data);
}

GCancellable *
_gtk_file_system_get_info (GtkFileSystem                *file_system,
			   GFile                        *file,
			   const gchar                  *attributes,
			   GtkFileSystemGetInfoCallback  callback,
			   gpointer                      data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  g_file_query_info_async (file,
			   attributes,
			   G_FILE_QUERY_INFO_NONE,
			   G_PRIORITY_DEFAULT,
			   cancellable,
			   query_info_callback,
			   async_data);

  return cancellable;
}

static void
drive_poll_for_media_cb (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;

  g_drive_poll_for_media_finish (G_DRIVE (source_object), result, &error);
  async_data = (AsyncFuncData *) user_data;

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable,
							     (GtkFileSystemVolume *) source_object,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);
}

static void
volume_mount_cb (GObject      *source_object,
		 GAsyncResult *result,
		 gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;

  g_volume_mount_finish (G_VOLUME (source_object), result, &error);
  async_data = (AsyncFuncData *) user_data;

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable,
							     (GtkFileSystemVolume *) source_object,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);
}

GCancellable *
_gtk_file_system_mount_volume (GtkFileSystem                    *file_system,
			       GtkFileSystemVolume              *volume,
			       GMountOperation                  *mount_operation,
			       GtkFileSystemVolumeMountCallback  callback,
			       gpointer                          data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;
  gboolean handled = FALSE;

  DEBUG ("volume_mount");

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  if (G_IS_DRIVE (volume))
    {
      /* this path happens for drives that are not polled by the OS and where the last media
       * check indicated that no media was available. So the thing to do here is to
       * invoke poll_for_media() on the drive
       */
      g_drive_poll_for_media (G_DRIVE (volume), cancellable, drive_poll_for_media_cb, async_data);
      handled = TRUE;
    }
  else if (G_IS_VOLUME (volume))
    {
      g_volume_mount (G_VOLUME (volume), G_MOUNT_MOUNT_NONE, mount_operation, cancellable, volume_mount_cb, async_data);
      handled = TRUE;
    }

  if (!handled)
    free_async_data (async_data);

  return cancellable;
}

static void
enclosing_volume_mount_cb (GObject      *source_object,
			   GAsyncResult *result,
			   gpointer      user_data)
{
  GtkFileSystemVolume *volume;
  AsyncFuncData *async_data;
  GError *error = NULL;

  async_data = (AsyncFuncData *) user_data;
  g_file_mount_enclosing_volume_finish (G_FILE (source_object), result, &error);
  volume = _gtk_file_system_get_volume_for_file (async_data->file_system, G_FILE (source_object));

  /* Silently drop G_IO_ERROR_ALREADY_MOUNTED error for gvfs backends without visible mounts. */
  /* Better than doing query_info with additional I/O every time. */
  if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED))
    g_clear_error (&error);

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable, volume,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);

  _gtk_file_system_volume_unref (volume);
}

GCancellable *
_gtk_file_system_mount_enclosing_volume (GtkFileSystem                     *file_system,
					 GFile                             *file,
					 GMountOperation                   *mount_operation,
					 GtkFileSystemVolumeMountCallback   callback,
					 gpointer                           data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  DEBUG ("mount_enclosing_volume");

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  g_file_mount_enclosing_volume (file,
				 G_MOUNT_MOUNT_NONE,
				 mount_operation,
				 cancellable,
				 enclosing_volume_mount_cb,
				 async_data);
  return cancellable;
}

GtkFileSystemVolume *
_gtk_file_system_get_volume_for_file (GtkFileSystem *file_system,
				      GFile         *file)
{
  GMount *mount;

  DEBUG ("get_volume_for_file");

  mount = g_file_find_enclosing_mount (file, NULL, NULL);

  if (!mount && g_file_is_native (file))
    return (GtkFileSystemVolume *) root_volume_token;

  return (GtkFileSystemVolume *) mount;
}

/* GtkFileSystemVolume public methods */
gchar *
_gtk_file_system_volume_get_display_name (GtkFileSystemVolume *volume)
{
  DEBUG ("volume_get_display_name");

  if (IS_ROOT_VOLUME (volume))
    return g_strdup (_(root_volume_token));
  if (G_IS_DRIVE (volume))
    return g_drive_get_name (G_DRIVE (volume));
  else if (G_IS_MOUNT (volume))
    return g_mount_get_name (G_MOUNT (volume));
  else if (G_IS_VOLUME (volume))
    return g_volume_get_name (G_VOLUME (volume));

  return NULL;
}

gboolean
_gtk_file_system_volume_is_mounted (GtkFileSystemVolume *volume)
{
  gboolean mounted;

  DEBUG ("volume_is_mounted");

  if (IS_ROOT_VOLUME (volume))
    return TRUE;

  mounted = FALSE;

  if (G_IS_MOUNT (volume))
    mounted = TRUE;
  else if (G_IS_VOLUME (volume))
    {
      GMount *mount;

      mount = g_volume_get_mount (G_VOLUME (volume));

      if (mount)
        {
          mounted = TRUE;
          g_object_unref (mount);
        }
    }

  return mounted;
}

GFile *
_gtk_file_system_volume_get_root (GtkFileSystemVolume *volume)
{
  GFile *file = NULL;

  DEBUG ("volume_get_base");

  if (IS_ROOT_VOLUME (volume))
    return g_file_new_for_uri ("file:///");

  if (G_IS_MOUNT (volume))
    file = g_mount_get_root (G_MOUNT (volume));
  else if (G_IS_VOLUME (volume))
    {
      GMount *mount;

      mount = g_volume_get_mount (G_VOLUME (volume));

      if (mount)
	{
	  file = g_mount_get_root (mount);
	  g_object_unref (mount);
	}
    }

  return file;
}

static cairo_surface_t *
get_surface_from_gicon (GIcon      *icon,
			GtkWidget  *widget,
			gint        icon_size,
			GError    **error)
{
  GdkScreen *screen;
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;
  cairo_surface_t *surface;

  screen = gtk_widget_get_screen (GTK_WIDGET (widget));
  icon_theme = gtk_icon_theme_get_for_screen (screen);

  icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
							icon,
							icon_size,
							gtk_widget_get_scale_factor (widget),
							GTK_ICON_LOOKUP_USE_BUILTIN);

  if (!icon_info)
    return NULL;

  surface = gtk_icon_info_load_surface (icon_info,
					gtk_widget_get_window (widget), error);

  g_object_unref (icon_info);

  return surface;
}

cairo_surface_t *
_gtk_file_system_volume_render_icon (GtkFileSystemVolume  *volume,
				     GtkWidget            *widget,
				     gint                  icon_size,
				     GError              **error)
{
  GIcon *icon = NULL;
  cairo_surface_t *surface;

  DEBUG ("volume_get_icon_name");

  if (IS_ROOT_VOLUME (volume))
    icon = g_themed_icon_new ("drive-harddisk");
  else if (G_IS_DRIVE (volume))
    icon = g_drive_get_icon (G_DRIVE (volume));
  else if (G_IS_VOLUME (volume))
    icon = g_volume_get_icon (G_VOLUME (volume));
  else if (G_IS_MOUNT (volume))
    icon = g_mount_get_icon (G_MOUNT (volume));

  if (!icon)
    return NULL;

  surface = get_surface_from_gicon (icon, widget, icon_size, error);

  g_object_unref (icon);

  return surface;
}

GtkFileSystemVolume *
_gtk_file_system_volume_ref (GtkFileSystemVolume *volume)
{
  if (IS_ROOT_VOLUME (volume))
    return volume;

  if (G_IS_MOUNT (volume)  ||
      G_IS_VOLUME (volume) ||
      G_IS_DRIVE (volume))
    g_object_ref (volume);

  return volume;
}

void
_gtk_file_system_volume_unref (GtkFileSystemVolume *volume)
{
  /* Root volume doesn't need to be freed */
  if (IS_ROOT_VOLUME (volume))
    return;

  if (G_IS_MOUNT (volume)  ||
      G_IS_VOLUME (volume) ||
      G_IS_DRIVE (volume))
    g_object_unref (volume);
}

/* GFileInfo helper functions */
cairo_surface_t *
_gtk_file_info_render_icon (GFileInfo *info,
			    GtkWidget *widget,
			    gint       icon_size)
{
  GIcon *icon;
  GdkPixbuf *pixbuf;
  const gchar *thumbnail_path;
  cairo_surface_t *surface = NULL;
  int scale;

  thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

  if (thumbnail_path)
    {
      scale = gtk_widget_get_scale_factor (widget);
      pixbuf = gdk_pixbuf_new_from_file_at_size (thumbnail_path,
						 icon_size*scale, icon_size*scale,
						 NULL);

      if (pixbuf != NULL)
        {
          surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale,
                                                          gtk_widget_get_window (widget));
          g_object_unref (pixbuf);
        }
    }

  if (!surface)
    {
      icon = g_file_info_get_icon (info);

      if (icon)
	surface = get_surface_from_gicon (icon, widget, icon_size, NULL);

      if (!surface)
	{
	   /* Use general fallback for all files without icon */
	  icon = g_themed_icon_new ("text-x-generic");
	  surface = get_surface_from_gicon (icon, widget, icon_size, NULL);
	  g_object_unref (icon);
	}
    }

  return surface;
}

gboolean
_gtk_file_info_consider_as_directory (GFileInfo *info)
{
  GFileType type = g_file_info_get_file_type (info);
  
  return (type == G_FILE_TYPE_DIRECTORY ||
          type == G_FILE_TYPE_MOUNTABLE ||
          type == G_FILE_TYPE_SHORTCUT);
}

gboolean
_gtk_file_has_native_path (GFile *file)
{
  char *local_file_path;
  gboolean has_native_path;

  /* Don't use g_file_is_native(), as we want to support FUSE paths if available */
  local_file_path = g_file_get_path (file);
  has_native_path = (local_file_path != NULL);
  g_free (local_file_path);

  return has_native_path;
}
