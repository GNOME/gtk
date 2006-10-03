/* GTK - The GIMP Toolkit
 * gtkfilesystem.c: Abstract file system interfaces
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
#include <gmodule.h>
#include "gtkfilesystem.h"
#include "gtkicontheme.h"
#include "gtkmodules.h"
#include "gtkintl.h"
#include "gtkstock.h"
#include "gtkalias.h"

#include <string.h>

struct _GtkFileInfo
{
  GtkFileTime modification_time;
  gint64 size;
  gchar *display_name;
  gchar *display_key;
  gchar *mime_type;
  gchar *icon_name;
  guint is_folder : 1;
  guint is_hidden : 1;
};

static void gtk_file_system_base_init (gpointer g_class);
static void gtk_file_folder_base_init (gpointer g_class);

GQuark
gtk_file_system_error_quark (void)
{
  return g_quark_from_static_string ("gtk-file-system-error-quark");
}

/*****************************************
 *             GtkFileInfo               *
 *****************************************/
GType
gtk_file_info_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (I_("GtkFileInfo"),
					     (GBoxedCopyFunc) gtk_file_info_copy,
					     (GBoxedFreeFunc) gtk_file_info_free);

  return our_type;
}

GtkFileInfo *
gtk_file_info_new  (void)
{
  GtkFileInfo *info;
  
  info = g_new0 (GtkFileInfo, 1);

  return info;
}

GtkFileInfo *
gtk_file_info_copy (GtkFileInfo *info)
{
  GtkFileInfo *new_info;

  g_return_val_if_fail (info != NULL, NULL);

  new_info = g_memdup (info, sizeof (GtkFileInfo));
  if (new_info->display_name)
    new_info->display_name = g_strdup (new_info->display_name);
  if (new_info->display_key)
    new_info->display_key = g_strdup (new_info->display_key);
  if (new_info->mime_type)
    new_info->mime_type = g_strdup (new_info->mime_type);
  if (new_info->icon_name)
    new_info->icon_name = g_strdup (new_info->icon_name);
  if (new_info->display_key)
    new_info->display_key = g_strdup (new_info->display_key);

  return new_info;
}

void
gtk_file_info_free (GtkFileInfo *info)
{
  g_return_if_fail (info != NULL);

  if (info->display_name)
    g_free (info->display_name);
  if (info->mime_type)
    g_free (info->mime_type);
  if (info->display_key)
    g_free (info->display_key);
  if (info->icon_name)
    g_free (info->icon_name);

  g_free (info);
}

G_CONST_RETURN gchar *
gtk_file_info_get_display_name (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->display_name;
}

/**
 * gtk_file_info_get_display_key:
 * @info: a #GtkFileInfo
 * 
 * Returns for the collation key for the display name for @info. 
 * This is useful when sorting a bunch of #GtkFileInfo structures 
 * since the collate key will be only computed once.
 * 
 * Return value: The collate key for the display name, or %NULL
 *   if the display name hasn't been set.
 **/
G_CONST_RETURN gchar *
gtk_file_info_get_display_key (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);

  if (!info->display_key && info->display_name)
    {
      /* Since info->display_key is only a cache, we cast off the const
       */
      ((GtkFileInfo *)info)->display_key = g_utf8_collate_key_for_filename (info->display_name, -1);
    }
	
  return info->display_key;
}

void
gtk_file_info_set_display_name (GtkFileInfo *info,
				const gchar *display_name)
{
  g_return_if_fail (info != NULL);

  if (display_name == info->display_name)
    return;

  if (info->display_name)
    g_free (info->display_name);
  if (info->display_key)
    {
      g_free (info->display_key);
      info->display_key = NULL;
    }

  info->display_name = g_strdup (display_name);
}

gboolean
gtk_file_info_get_is_folder (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  
  return info->is_folder;
}

void
gtk_file_info_set_is_folder (GtkFileInfo *info,
			     gboolean     is_folder)
{
  g_return_if_fail (info != NULL);

  info->is_folder = is_folder != FALSE;
}

gboolean
gtk_file_info_get_is_hidden (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  
  return info->is_hidden;
}

void
gtk_file_info_set_is_hidden (GtkFileInfo *info,
			     gboolean     is_hidden)
{
  g_return_if_fail (info != NULL);

  info->is_hidden = is_hidden != FALSE;
}

G_CONST_RETURN gchar *
gtk_file_info_get_mime_type (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->mime_type;
}

void
gtk_file_info_set_mime_type (GtkFileInfo *info,
			     const gchar *mime_type)
{
  g_return_if_fail (info != NULL);
  
  if (info->mime_type)
    g_free (info->mime_type);

  info->mime_type = g_strdup (mime_type);
}

GtkFileTime
gtk_file_info_get_modification_time (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, 0);

  return info->modification_time;
}

void
gtk_file_info_set_modification_time (GtkFileInfo *info,
				     GtkFileTime  modification_time)
{
  g_return_if_fail (info != NULL);
  
  info->modification_time = modification_time;
}

gint64
gtk_file_info_get_size (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, 0);
  
  return info->size;
}

void
gtk_file_info_set_size (GtkFileInfo *info,
			gint64       size)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (size >= 0);
  
  info->size = size;
}

void
gtk_file_info_set_icon_name (GtkFileInfo *info,
			     const gchar *icon_name)
{
  g_return_if_fail (info != NULL);
  
  if (info->icon_name)
    g_free (info->icon_name);

  info->icon_name = g_strdup (icon_name);
}

G_CONST_RETURN gchar *
gtk_file_info_get_icon_name (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->icon_name;
}

GdkPixbuf *
gtk_file_info_render_icon (const GtkFileInfo  *info,
			   GtkWidget          *widget,
			   gint                pixel_size,
			   GError            **error)
{
  GdkPixbuf *pixbuf = NULL;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (info->icon_name)
    {
      if (g_path_is_absolute (info->icon_name))
	pixbuf = gdk_pixbuf_new_from_file_at_size (info->icon_name,
						   pixel_size,
						   pixel_size,
						   NULL);
      else
        {
          GtkIconTheme *icon_theme;

	  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
          if (gtk_icon_theme_has_icon (icon_theme, info->icon_name))
            pixbuf = gtk_icon_theme_load_icon (icon_theme, info->icon_name,
                                               pixel_size, 0, NULL);
	}
    }

  if (!pixbuf)
    {
      /* load a fallback icon */
      pixbuf = gtk_widget_render_icon (widget,
                                       gtk_file_info_get_is_folder (info)
                                        ? GTK_STOCK_DIRECTORY : GTK_STOCK_FILE,
                                       GTK_ICON_SIZE_SMALL_TOOLBAR,
                                       NULL);
      if (!pixbuf && error)
        g_set_error (error,
		     GTK_FILE_SYSTEM_ERROR,
		     GTK_FILE_SYSTEM_ERROR_FAILED,
		     _("Could not get a stock icon for %s\n"),
		     info->icon_name);
    }

  return pixbuf;
}

/*****************************************
 *          GtkFileSystemHandle          *
 *****************************************/

enum
{
  PROP_0,
  PROP_CANCELLED
};

G_DEFINE_TYPE (GtkFileSystemHandle, gtk_file_system_handle, G_TYPE_OBJECT)

static void
gtk_file_system_handle_init (GtkFileSystemHandle *handle)
{
  handle->file_system = NULL;
  handle->cancelled = FALSE;
}

static void
gtk_file_system_handle_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gtk_file_system_handle_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
  GtkFileSystemHandle *handle = GTK_FILE_SYSTEM_HANDLE (object);

  switch (prop_id)
    {
      case PROP_CANCELLED:
	g_value_set_boolean (value, handle->cancelled);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void
gtk_file_system_handle_class_init (GtkFileSystemHandleClass *klass)
{
  GObjectClass *o_class;

  o_class = (GObjectClass *)klass;
  o_class->set_property = gtk_file_system_handle_set_property;
  o_class->get_property = gtk_file_system_handle_get_property;

  g_object_class_install_property (o_class,
				   PROP_CANCELLED,
				   g_param_spec_boolean ("cancelled",
							 P_("Cancelled"),
							 P_("Whether or not the operation has been successfully cancelled"),
							 FALSE,
							 G_PARAM_READABLE));
}

/*****************************************
 *             GtkFileSystem             *
 *****************************************/
GType
gtk_file_system_get_type (void)
{
  static GType file_system_type = 0;

  if (!file_system_type)
    {
      const GTypeInfo file_system_info =
      {
	sizeof (GtkFileSystemIface),  /* class_size */
	gtk_file_system_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      file_system_type = g_type_register_static (G_TYPE_INTERFACE,
						 I_("GtkFileSystem"),
						 &file_system_info, 0);

      g_type_interface_add_prerequisite (file_system_type, G_TYPE_OBJECT);
    }

  return file_system_type;
}

static void
gtk_file_system_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_class);

      g_signal_new (I_("volumes-changed"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileSystemIface, volumes_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new (I_("bookmarks-changed"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileSystemIface, bookmarks_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

GSList *
gtk_file_system_list_volumes (GtkFileSystem  *file_system)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->list_volumes (file_system);
}

GtkFileSystemHandle *
gtk_file_system_get_folder (GtkFileSystem                  *file_system,
			    const GtkFilePath              *path,
			    GtkFileInfoType                 types,
			    GtkFileSystemGetFolderCallback  callback,
			    gpointer                        data)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_folder (file_system, path, types, callback, data);
}

GtkFileSystemHandle *
gtk_file_system_get_info (GtkFileSystem *file_system,
			  const GtkFilePath *path,
			  GtkFileInfoType types,
			  GtkFileSystemGetInfoCallback callback,
			  gpointer data)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_info (file_system, path, types, callback, data);
}

GtkFileSystemHandle *
gtk_file_system_create_folder (GtkFileSystem                     *file_system,
			       const GtkFilePath                 *path,
			       GtkFileSystemCreateFolderCallback  callback,
			       gpointer                           data)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->create_folder (file_system, path, callback, data);
}

void
gtk_file_system_cancel_operation (GtkFileSystemHandle *handle)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM_HANDLE (handle));

  GTK_FILE_SYSTEM_GET_IFACE (handle->file_system)->cancel_operation (handle);
}

/**
 * gtk_file_system_get_volume_for_path:
 * @file_system: a #GtkFileSystem
 * @path: a #GtkFilePath
 * 
 * Queries the file system volume that corresponds to a specific path.
 * There might not be a volume for all paths (consinder for instance remote
 * shared), so this can return NULL.
 * 
 * Return value: the #GtkFileSystemVolume that corresponds to the specified
 * @path, or NULL if there is no such volume. You should free this value with
 * gtk_file_system_volume_free().
 **/
GtkFileSystemVolume *
gtk_file_system_get_volume_for_path (GtkFileSystem     *file_system,
				     const GtkFilePath *path)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_volume_for_path (file_system, path);
}

/**
 * gtk_file_system_volume_free:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * 
 * Frees a #GtkFileSystemVolume structure as returned by
 * gtk_file_system_list_volumes().
 **/
void
gtk_file_system_volume_free (GtkFileSystem       *file_system,
			     GtkFileSystemVolume *volume)
{
  g_return_if_fail (GTK_IS_FILE_SYSTEM (file_system));
  g_return_if_fail (volume != NULL);

  GTK_FILE_SYSTEM_GET_IFACE (file_system)->volume_free (file_system, volume);
}

/**
 * gtk_file_system_volume_get_base_path:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * 
 * Queries the base path for a volume.  For example, a CD-ROM device may yield a
 * path of "/mnt/cdrom".
 * 
 * Return value: a #GtkFilePath with the base mount path of the specified
 * @volume.
 **/
GtkFilePath *
gtk_file_system_volume_get_base_path (GtkFileSystem       *file_system,
				      GtkFileSystemVolume *volume)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (volume != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->volume_get_base_path (file_system, volume);
}

/**
 * gtk_file_system_volume_get_is_mounted:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * 
 * Queries whether a #GtkFileSystemVolume is mounted or not.  If it is not, it
 * can be mounted with gtk_file_system_volume_mount().
 * 
 * Return value: TRUE if the @volume is mounted, FALSE otherwise.
 **/
gboolean
gtk_file_system_volume_get_is_mounted (GtkFileSystem       *file_system,
				       GtkFileSystemVolume *volume)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (volume != NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->volume_get_is_mounted (file_system, volume);
}

/**
 * gtk_file_system_volume_mount:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * @error: location to store error, or %NULL
 * 
 * Tries to mount an unmounted volume.  This may cause the "volumes-changed"
 * signal in the @file_system to be emitted.
 * 
 * Return value: TRUE if the @volume was mounted successfully, FALSE otherwise.
 **/
/* FIXME XXX: update documentation above */
GtkFileSystemHandle *
gtk_file_system_volume_mount (GtkFileSystem                    *file_system,
			      GtkFileSystemVolume              *volume,
			      GtkFileSystemVolumeMountCallback  callback,
			      gpointer                          data)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (volume != NULL, NULL);
  g_return_val_if_fail (callback != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->volume_mount (file_system, volume, callback, data);
}

/**
 * gtk_file_system_volume_get_display_name:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * 
 * Queries the human-readable name for a @volume.  This string can be displayed
 * in a list of volumes that can be accessed, for example.
 * 
 * Return value: A string with the human-readable name for a #GtkFileSystemVolume.
 **/
char *
gtk_file_system_volume_get_display_name (GtkFileSystem        *file_system, 
					 GtkFileSystemVolume  *volume)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (volume != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->volume_get_display_name (file_system, volume);
}

/**
 * gtk_file_system_volume_render_icon:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * @widget: Reference widget to render icons.
 * @pixel_size: Size of the icon.
 * @error: location to store error, or %NULL
 * 
 * Renders an icon suitable for a file #GtkFileSystemVolume.
 * 
 * Return value: A #GdkPixbuf containing an icon, or NULL if the icon could not
 * be rendered.  In the latter case, the @error value will be set as
 * appropriate.
 **/
GdkPixbuf *
gtk_file_system_volume_render_icon (GtkFileSystem        *file_system,
				    GtkFileSystemVolume  *volume,
				    GtkWidget            *widget,
				    gint                  pixel_size,
				    GError              **error)
{
  gchar *icon_name;
  GdkPixbuf *pixbuf = NULL;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (volume != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (pixel_size > 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  icon_name = gtk_file_system_volume_get_icon_name (file_system, volume,
						    error);
  if (icon_name)
    {
      GtkIconTheme *icon_theme;

      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
      if (gtk_icon_theme_has_icon (icon_theme, icon_name))
        pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                           icon_name, pixel_size, 0, NULL);
      g_free (icon_name);
    }

  if (!pixbuf)
    pixbuf = gtk_widget_render_icon (widget,
                                     GTK_STOCK_HARDDISK,
                                     GTK_ICON_SIZE_SMALL_TOOLBAR,
                                     NULL);

  return pixbuf;
}

/**
 * gtk_file_system_volume_get_icon_name:
 * @file_system: a #GtkFileSystem
 * @volume: a #GtkFileSystemVolume
 * @error: location to store error, or %NULL
 * 
 * Gets an icon name suitable for a #GtkFileSystemVolume.
 * 
 * Return value: An icon name which can be used for rendering an icon for
 * this volume, or %NULL if no icon name could be found.  In the latter
 * case, the @error value will be set as appropriate.
 **/
gchar *
gtk_file_system_volume_get_icon_name (GtkFileSystem        *file_system,
				      GtkFileSystemVolume  *volume,
				      GError              **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (volume != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->volume_get_icon_name (file_system,
								        volume,
								        error);
}

/**
 * gtk_file_system_get_parent:
 * @file_system: a #GtkFileSystem
 * @path: base path name
 * @parent: location to store parent path name
 * @error: location to store error, or %NULL
 * 
 * Gets the name of the parent folder of a path.  If the path has no parent, as when
 * you request the parent of a file system root, then @parent will be set to %NULL.
 * 
 * Return value: %TRUE if the operation was successful:  @parent will be set to
 * the name of the @path's parent, or to %NULL if @path is already a file system
 * root.  If the operation fails, this function returns %FALSE, sets @parent to
 * NULL, and sets the @error value if it is specified.
 **/
gboolean
gtk_file_system_get_parent (GtkFileSystem     *file_system,
			    const GtkFilePath *path,
			    GtkFilePath      **parent,
			    GError           **error)
{
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (parent != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  *parent = NULL;

  result = GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_parent (file_system, path, parent, error);
  g_assert (result || *parent == NULL);

  return result;
}

GtkFilePath *
gtk_file_system_make_path (GtkFileSystem    *file_system,
			  const GtkFilePath *base_path,
			  const gchar       *display_name,
			  GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (base_path != NULL, NULL);
  g_return_val_if_fail (display_name != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->make_path (file_system, base_path, display_name, error);
}

/**
 * gtk_file_system_parse:
 * @file_system: a #GtkFileSystem
 * @base_path: reference folder with respect to which relative
 *            paths should be interpreted.
 * @str: the string to parse
 * @folder: location to store folder portion of result, or %NULL
 * @file_part: location to store file portion of result, or %NULL
 * @error: location to store error, or %NULL
 * 
 * Given a string entered by a user, parse it (possibly using
 * heuristics) into a folder path and a UTF-8 encoded
 * filename part. (Suitable for passing to gtk_file_system_make_path())
 *
 * Note that the returned filename point may point to a subfolder
 * of the returned folder. Adding a trailing path separator is needed
 * to enforce the interpretation as a folder name.
 *
 * If parsing fails because the syntax of @str is not understood,
 * and error of type GTK_FILE_SYSTEM_ERROR_BAD_FILENAME will
 * be set in @error and %FALSE returned.
 *
 * If parsing fails because a path was encountered that doesn't
 * exist on the filesystem, then an error of type
 * %GTK_FILE_SYSTEM_ERROR_NONEXISTENT will be set in @error
 * and %FALSE returned. (This only applies to parsing relative paths,
 * not to interpretation of @file_part. No check is made as
 * to whether @file_part exists.)
 *
 * Return value: %TRUE if the parsing succeeds, otherwise, %FALSE.
 **/
gboolean
gtk_file_system_parse (GtkFileSystem     *file_system,
		       const GtkFilePath *base_path,
		       const gchar       *str,
		       GtkFilePath      **folder,
		       gchar            **file_part,
		       GError           **error)
{
  GtkFilePath *tmp_folder = NULL;
  gchar *tmp_file_part = NULL;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (base_path != NULL, FALSE);
  g_return_val_if_fail (str != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  result = GTK_FILE_SYSTEM_GET_IFACE (file_system)->parse (file_system, base_path, str,
							   &tmp_folder, &tmp_file_part,
							   error);
  g_assert (result || (tmp_folder == NULL && tmp_file_part == NULL));

  if (folder)
    *folder = tmp_folder;
  else
    gtk_file_path_free (tmp_folder);

  if (file_part)
    *file_part = tmp_file_part;
  else
    g_free (tmp_file_part);

  return result;
}


gchar *
gtk_file_system_path_to_uri (GtkFileSystem     *file_system,
			     const GtkFilePath *path)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  
  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->path_to_uri (file_system, path);
}

gchar *
gtk_file_system_path_to_filename (GtkFileSystem     *file_system,
				  const GtkFilePath *path)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  
  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->path_to_filename (file_system, path);
}

GtkFilePath *
gtk_file_system_uri_to_path (GtkFileSystem *file_system,
			     const gchar    *uri)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  
  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->uri_to_path (file_system, uri);
}

GtkFilePath *
gtk_file_system_filename_to_path (GtkFileSystem *file_system,
				  const gchar   *filename)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->filename_to_path (file_system, filename);
}

/**
 * gtk_file_system_path_is_local:
 * @filesystem: a #GtkFileSystem
 * @path: A #GtkFilePath from that filesystem
 * 
 * Checks whether a #GtkFilePath is local; that is whether
 * gtk_file_system_path_to_filename would return non-%NULL.
 * 
 * Return value: %TRUE if the path is loca
 **/
gboolean
gtk_file_system_path_is_local (GtkFileSystem     *file_system,
			       const GtkFilePath *path)
{
  gchar *filename;
  gboolean result;
    
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  filename = gtk_file_system_path_to_filename (file_system, path);
  result = filename != NULL;
  g_free (filename);

  return result;
}

/**
 * gtk_file_system_insert_bookmark:
 * @file_system: a #GtkFileSystem
 * @path: path of the bookmark to add
 * @position: index in the bookmarks list at which the @path should be inserted; use 0
 * for the beginning, and -1 or the number of bookmarks itself for the end of the list.
 * @error: location to store error, or %NULL
 * 
 * Adds a path for a folder to the user's bookmarks list.  If the operation
 * succeeds, the "bookmarks_changed" signal will be emitted.  Bookmark paths are
 * unique; if you try to insert a @path that already exists, the operation will
 * fail and the @error will be set to #GTK_FILE_SYSTEM_ERROR_ALREADY_EXISTS.  To
 * reorder the list of bookmarks, use gtk_file_system_remove_bookmark() to
 * remove the path in question, and call gtk_file_system_insert_bookmark() with
 * the new position for the path.
 * 
 * Return value: TRUE if the operation succeeds, FALSE otherwise.  In the latter case,
 * the @error value will be set.
 **/
gboolean
gtk_file_system_insert_bookmark (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 gint               position,
				 GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->insert_bookmark (file_system, path, position, error);
}

/**
 * gtk_file_system_remove_bookmark:
 * @file_system: a #GtkFileSystem
 * @path: path of the bookmark to remove
 * @error: location to store error, or %NULL
 * 
 * Removes a bookmark folder from the user's bookmarks list.  If the operation
 * succeeds, the "bookmarks_changed" signal will be emitted.  If you try to remove
 * a @path which does not exist in the bookmarks list, the operation will fail
 * and the @error will be set to GTK_FILE_SYSTEM_ERROR_NONEXISTENT.
 * 
 * Return value: TRUE if the operation succeeds, FALSE otherwise.  In the latter
 * case, the @error value will be set.
 **/
gboolean
gtk_file_system_remove_bookmark (GtkFileSystem     *file_system,
				 const GtkFilePath *path,
				 GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->remove_bookmark (file_system, path, error);
}

/**
 * gtk_file_system_list_bookmarks:
 * @file_system: a #GtkFileSystem
 * 
 * Queries the list of bookmarks in the file system.
 * 
 * Return value: A list of #GtkFilePath, or NULL if there are no configured
 * bookmarks.  You should use gtk_file_paths_free() to free this list.
 *
 * See also: gtk_file_system_get_supports_bookmarks()
 **/
GSList *
gtk_file_system_list_bookmarks (GtkFileSystem *file_system)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->list_bookmarks (file_system);
}

/**
 * gtk_file_system_get_bookmark_label:
 * @file_system: a #GtkFileSystem
 * @path: path of the bookmark 
 *
 * Gets the label to display for a bookmark, or %NULL.
 *
 * Returns: the label for the bookmark @path
 *
 * Since: 2.8
 */
gchar *
gtk_file_system_get_bookmark_label (GtkFileSystem     *file_system,
				    const GtkFilePath *path)
{
  GtkFileSystemIface *iface;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  iface = GTK_FILE_SYSTEM_GET_IFACE (file_system);
  if (iface->get_bookmark_label)
    return iface->get_bookmark_label (file_system, path);

  return NULL;
}

/**
 * gtk_file_system_set_bookmark_label:
 * @file_system: a #GtkFileSystem
 * @path: path of the bookmark 
 * @label: the label for the bookmark, or %NULL to display
 *   the path itself
 *
 * Sets the label to display for a bookmark.
 *
 * Since: 2.8
 */
void
gtk_file_system_set_bookmark_label (GtkFileSystem     *file_system,
				    const GtkFilePath *path,
				    const gchar       *label)
{
  GtkFileSystemIface *iface;

  g_return_if_fail (GTK_IS_FILE_SYSTEM (file_system));
  g_return_if_fail (path != NULL);

  iface = GTK_FILE_SYSTEM_GET_IFACE (file_system);
  if (iface->set_bookmark_label)
    iface->set_bookmark_label (file_system, path, label);
}

/*****************************************
 *             GtkFileFolder             *
 *****************************************/
GType
gtk_file_folder_get_type (void)
{
  static GType file_folder_type = 0;

  if (!file_folder_type)
    {
      const GTypeInfo file_folder_info =
      {
	sizeof (GtkFileFolderIface),  /* class_size */
	gtk_file_folder_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      file_folder_type = g_type_register_static (G_TYPE_INTERFACE,
						 I_("GtkFileFolder"),
						 &file_folder_info, 0);
      
      g_type_interface_add_prerequisite (file_folder_type, G_TYPE_OBJECT);
    }

  return file_folder_type;
}

static void
gtk_file_folder_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_class);

      g_signal_new (I_("deleted"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, deleted),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new (I_("files-added"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_added),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);
      g_signal_new (I_("files-changed"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);
      g_signal_new (I_("files-removed"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_removed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);
      g_signal_new (I_("finished-loading"),
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, finished_loading),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

gboolean
gtk_file_folder_list_children (GtkFileFolder    *folder,
			       GSList          **children,
			       GError          **error)
{
  gboolean result;
  GSList *tmp_children = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_FOLDER (folder), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  result = GTK_FILE_FOLDER_GET_IFACE (folder)->list_children (folder, &tmp_children, error);
  g_assert (result || tmp_children == NULL);

  if (children)
    *children = tmp_children;
  else
    gtk_file_paths_free (tmp_children);

  return result;
}

GtkFileInfo *
gtk_file_folder_get_info (GtkFileFolder     *folder,
			  const GtkFilePath *path,
			  GError           **error)
{
  g_return_val_if_fail (GTK_IS_FILE_FOLDER (folder), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_FOLDER_GET_IFACE (folder)->get_info (folder, path, error);
}

gboolean
gtk_file_folder_is_finished_loading (GtkFileFolder *folder)
{
  GtkFileFolderIface *iface;

  g_return_val_if_fail (GTK_IS_FILE_FOLDER (folder), TRUE);

  iface = GTK_FILE_FOLDER_GET_IFACE (folder);
  if (!iface->is_finished_loading)
    return TRUE;
  else
    return iface->is_finished_loading (folder);
}


/*****************************************
 *         GtkFilePath modules           *
 *****************************************/

/* We make these real functions in case either copy or free are implemented as macros
 */
static gpointer
gtk_file_path_real_copy (gpointer boxed)
{
  return gtk_file_path_copy ((GtkFilePath *) boxed);
}

static void
gtk_file_path_real_free	(gpointer boxed)
{
  gtk_file_path_free (boxed);
}

GType
gtk_file_path_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (I_("GtkFilePath"),
					     (GBoxedCopyFunc) gtk_file_path_real_copy,
					     (GBoxedFreeFunc) gtk_file_path_real_free);

  return our_type;
}


GSList *
gtk_file_paths_sort (GSList *paths)
{
#ifndef G_OS_WIN32
  return g_slist_sort (paths, (GCompareFunc)strcmp);
#else
  return g_slist_sort (paths, (GCompareFunc)_gtk_file_system_win32_path_compare);
#endif
}

/**
 * gtk_file_paths_copy:
 * @paths: A #GSList of #GtkFilePath structures.
 * 
 * Copies a list of #GtkFilePath structures.
 * 
 * Return value: A copy of @paths.  Since the contents of the list are copied as
 * well, you should use gtk_file_paths_free() to free the result.
 **/
GSList *
gtk_file_paths_copy (GSList *paths)
{
  GSList *head, *tail, *l;

  head = tail = NULL;

  for (l = paths; l; l = l->next)
    {
      GtkFilePath *path;
      GSList *node;

      path = l->data;
      node = g_slist_alloc ();

      if (tail)
	tail->next = node;
      else
	head = node;

      node->data = gtk_file_path_copy (path);
      tail = node;
    }

  return head;
}

void
gtk_file_paths_free (GSList *paths)
{
  GSList *tmp_list;

  for (tmp_list = paths; tmp_list; tmp_list = tmp_list->next)
    gtk_file_path_free (tmp_list->data);

  g_slist_free (paths);
}

/*****************************************
 *         GtkFileSystem modules         *
 *****************************************/

typedef struct _GtkFileSystemModule GtkFileSystemModule;
typedef struct _GtkFileSystemModuleClass GtkFileSystemModuleClass;

struct _GtkFileSystemModule
{
  GTypeModule parent_instance;
  
  GModule *library;

  void            (*init)     (GTypeModule    *module);
  void            (*exit)     (void);
  GtkFileSystem * (*create)   (void);

  gchar *path;
};

struct _GtkFileSystemModuleClass
{
  GTypeModuleClass parent_class;
};

G_DEFINE_TYPE (GtkFileSystemModule, _gtk_file_system_module, G_TYPE_TYPE_MODULE)
#define GTK_TYPE_FILE_SYSTEM_MODULE       (_gtk_file_system_module_get_type ())
#define GTK_FILE_SYSTEM_MODULE(module)	  (G_TYPE_CHECK_INSTANCE_CAST ((module), GTK_TYPE_FILE_SYSTEM_MODULE, GtkFileSystemModule))


static GSList *loaded_file_systems;

static gboolean
gtk_file_system_module_load (GTypeModule *module)
{
  GtkFileSystemModule *fs_module = GTK_FILE_SYSTEM_MODULE (module);
  
  fs_module->library = g_module_open (fs_module->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  if (!fs_module->library)
    {
      g_warning (g_module_error());
      return FALSE;
    }
  
  /* extract symbols from the lib */
  if (!g_module_symbol (fs_module->library, "fs_module_init",
			(gpointer *)&fs_module->init) ||
      !g_module_symbol (fs_module->library, "fs_module_exit", 
			(gpointer *)&fs_module->exit) ||
      !g_module_symbol (fs_module->library, "fs_module_create", 
			(gpointer *)&fs_module->create))
    {
      g_warning (g_module_error());
      g_module_close (fs_module->library);
      
      return FALSE;
    }
	    
  /* call the filesystems's init function to let it */
  /* setup anything it needs to set up. */
  fs_module->init (module);

  return TRUE;
}

static void
gtk_file_system_module_unload (GTypeModule *module)
{
  GtkFileSystemModule *fs_module = GTK_FILE_SYSTEM_MODULE (module);
  
  fs_module->exit();

  g_module_close (fs_module->library);
  fs_module->library = NULL;

  fs_module->init = NULL;
  fs_module->exit = NULL;
  fs_module->create = NULL;
}

/* This only will ever be called if an error occurs during
 * initialization
 */
static void
gtk_file_system_module_finalize (GObject *object)
{
  GtkFileSystemModule *module = GTK_FILE_SYSTEM_MODULE (object);

  g_free (module->path);

  G_OBJECT_CLASS (_gtk_file_system_module_parent_class)->finalize (object);
}

static void
_gtk_file_system_module_class_init (GtkFileSystemModuleClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  
  module_class->load = gtk_file_system_module_load;
  module_class->unload = gtk_file_system_module_unload;

  gobject_class->finalize = gtk_file_system_module_finalize;
}

static void
_gtk_file_system_module_init (GtkFileSystemModule *fs_module)
{
}


static GtkFileSystem *
_gtk_file_system_module_create (GtkFileSystemModule *fs_module)
{
  GtkFileSystem *fs;
  
  if (g_type_module_use (G_TYPE_MODULE (fs_module)))
    {
      fs = fs_module->create ();
      g_type_module_unuse (G_TYPE_MODULE (fs_module));
      return fs;
    }
  return NULL;
}


GtkFileSystem *
gtk_file_system_create (const char *file_system_name)
{
  GSList *l;
  char *module_path;
  GtkFileSystemModule *fs_module;
  GtkFileSystem *fs;

  for (l = loaded_file_systems; l != NULL; l = l->next)
    {
      fs_module = l->data;
      
      if (strcmp (G_TYPE_MODULE (fs_module)->name, file_system_name) == 0)
	return _gtk_file_system_module_create (fs_module);
    }

  fs = NULL;
  if (g_module_supported ())
    {
      module_path = _gtk_find_module (file_system_name, "filesystems");

      if (module_path)
	{
	  fs_module = g_object_new (GTK_TYPE_FILE_SYSTEM_MODULE, NULL);

	  g_type_module_set_name (G_TYPE_MODULE (fs_module), file_system_name);
	  fs_module->path = g_strdup (module_path);

	  loaded_file_systems = g_slist_prepend (loaded_file_systems,
						 fs_module);

	  fs = _gtk_file_system_module_create (fs_module);
	}
      
      g_free (module_path);
    }
  
  return fs;
}

#define __GTK_FILE_SYSTEM_C__
#include "gtkaliasdef.c"
