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

#include "gtkfilesystem.h"

struct _GtkFileInfo
{
  GtkFileTime modification_time;
  gint64 size;
  gchar *display_name;
  gchar *display_key;
  gchar *mime_type;
  GdkPixbuf *icon;
  guint is_folder : 1;
  guint is_hidden : 1;
};

static void gtk_file_system_base_init (gpointer g_class);
static void gtk_file_folder_base_init (gpointer g_class);

GQuark
gtk_file_system_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("gtk-file-system-error-quark");
  return quark;
}

/*****************************************
 *             GtkFileInfo               *
 *****************************************/
GType
gtk_file_info_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GtkFileInfo",
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
  if (new_info->mime_type)
    new_info->mime_type = g_strdup (new_info->mime_type);
  if (new_info->icon)
    g_object_ref (new_info->icon);

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
  if (info->icon)
    g_object_unref (info->icon);
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
 * Returns results of g_utf8_collate_key() on the display name
 * for @info. This is useful when sorting a bunch of #GtkFileInfo
 * structures since the collate key will be only computed once.
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
      ((GtkFileInfo *)info)->display_key = g_utf8_collate_key (info->display_name, -1);
    }
	
  
  return info->display_key;
}

void
gtk_file_info_set_display_name (GtkFileInfo *info,
				const gchar *display_name)
{
  g_return_if_fail (info != NULL);

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

GdkPixbuf *
gtk_file_info_get_icon (const GtkFileInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);

  return info->icon;
}

void
gtk_file_info_set_icon (GtkFileInfo *info,
			GdkPixbuf   *icon)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (icon == NULL || GDK_IS_PIXBUF (icon));

  if (icon != info->icon)
    {
      if (info->icon)
	g_object_unref (info->icon);

      info->icon = icon;
      
      if (info->icon)
	g_object_ref (info->icon);
    }
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
      static const GTypeInfo file_system_info =
      {
	sizeof (GtkFileSystemIface),  /* class_size */
	gtk_file_system_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      file_system_type = g_type_register_static (G_TYPE_INTERFACE,
						 "GtkFileSystem",
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

      g_signal_new ("roots_changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileSystemIface, roots_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

GSList *
gtk_file_system_list_roots (GtkFileSystem  *file_system)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->list_roots (file_system);
}

GtkFileInfo *
gtk_file_system_get_root_info  (GtkFileSystem    *file_system,
				const gchar      *uri,
				GtkFileInfoType   types,
				GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_root_info (file_system, uri, types, error);
}

GtkFileFolder *
gtk_file_system_get_folder (GtkFileSystem    *file_system,
			    const gchar      *uri,
			    GtkFileInfoType   types,
			    GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_folder (file_system, uri, types, error);
}

gboolean
gtk_file_system_create_folder(GtkFileSystem    *file_system,
			      const gchar      *uri,
			      GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->create_folder (file_system, uri, error);
}

gboolean
gtk_file_system_get_parent (GtkFileSystem *file_system,
			    const gchar   *uri,
			    gchar         **parent,
			    GError        **error)
{
  gchar *tmp_parent = NULL;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  result = GTK_FILE_SYSTEM_GET_IFACE (file_system)->get_parent (file_system, uri, &tmp_parent, error);
  g_assert (result || tmp_parent == NULL);

  if (parent)
    *parent = tmp_parent;
  else
    g_free (tmp_parent);
  
  return result;
}

gchar *
gtk_file_system_make_uri (GtkFileSystem    *file_system,
			  const gchar      *base_uri,
			  const gchar      *display_name,
			  GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), NULL);
  g_return_val_if_fail (base_uri != NULL, NULL);
  g_return_val_if_fail (display_name != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return GTK_FILE_SYSTEM_GET_IFACE (file_system)->make_uri (file_system, base_uri, display_name, error);
}

/**
 * gtk_file_system_parse:
 * @file_system: a #GtkFileSystem
 * @base_uri: reference folder with respect to which relative
 *            paths should be interpreted.
 * @str: the string to parse
 * @folder: location to store folder portion of result, or %NULL
 * @file_part: location to store file portion of result, or %NULL
 * @error: location to store error, or %NULL
 * 
 * Given a string entered by a user, parse it (possibly using
 * heuristics) into a folder URI and a UTF-8 encoded
 * filename part. (Suitable for passing to gtk_file_system_make_uri())
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
 * %GTK_FILE_SYSTEM_ERROR_NONEXISTANT will be set in @error
 * and %FALSE returned. (This only applies to parsing relative paths,
 * not to interpretation of @file_part. No check is made as
 * to whether @file_part exists.)
 *
 * Return value: %TRUE if the parsing succeeds, otherwise, %FALSE.
 **/
gboolean
gtk_file_system_parse (GtkFileSystem   *file_system,
		       const gchar     *base_uri,
		       const gchar     *str,
		       gchar          **folder,
		       gchar          **file_part,
		       GError         **error)
{
  gchar *tmp_folder = NULL;
  gchar *tmp_file_part = NULL;
  gboolean result;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM (file_system), FALSE);
  g_return_val_if_fail (base_uri != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);


  result = GTK_FILE_SYSTEM_GET_IFACE (file_system)->parse (file_system, base_uri, str,
							    &tmp_folder, &tmp_file_part,
							    error);
  g_assert (result || (tmp_folder == NULL && tmp_file_part == NULL));

  if (folder)
    *folder = tmp_folder;
  else
    g_free (tmp_folder);

  if (file_part)
    *file_part = tmp_file_part;
  else
    g_free (tmp_file_part);

  return result;
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
      static const GTypeInfo file_folder_info =
      {
	sizeof (GtkFileFolderIface),  /* class_size */
	gtk_file_folder_base_init,    /* base_init */
	NULL,                         /* base_finalize */
      };

      file_folder_type = g_type_register_static (G_TYPE_INTERFACE,
						 "GtkFileFolder",
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

      g_signal_new ("deleted",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, deleted),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new ("files_added",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, files_added),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__POINTER,
		    G_TYPE_NONE, 1,
		    G_TYPE_POINTER);
      g_signal_new ("file_changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, file_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__STRING,
		    G_TYPE_NONE, 1,
		    G_TYPE_STRING);
      g_signal_new ("file_removed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileFolderIface, file_removed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__STRING,
		    G_TYPE_NONE, 1,
		    G_TYPE_STRING);

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
    {
      g_slist_foreach (tmp_children, (GFunc)g_free, NULL);
      g_slist_free (tmp_children);
    }

  return result;
}

GtkFileInfo *
gtk_file_folder_get_info (GtkFileFolder    *folder,
			  const gchar      *uri,
			  GError          **error)
{
  g_return_val_if_fail (GTK_IS_FILE_FOLDER (folder), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_FILE_FOLDER_GET_IFACE (folder)->get_info (folder, uri, error);
}
