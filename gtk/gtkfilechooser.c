/* GTK - The GIMP Toolkit
 * gtkfilechooser.c: Abstract interface for file selector GUIs
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

#include "gtkfilechooser.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserenums.h"
#include "gtkfilesystem.h"

#define _(str) (str)

static void gtk_file_chooser_base_init (gpointer g_class);

static GtkFilePath *gtk_file_chooser_get_path (GtkFileChooser *chooser);

GType
gtk_file_chooser_get_type (void)
{
  static GType file_chooser_type = 0;

  if (!file_chooser_type)
    {
      static const GTypeInfo file_chooser_info =
      {
	sizeof (GtkFileChooserIface),  /* class_size */
	gtk_file_chooser_base_init,    /* base_init */
	NULL,			       /* base_finalize */
      };

      file_chooser_type = g_type_register_static (G_TYPE_INTERFACE,
						  "GtkFileChooser",
						  &file_chooser_info, 0);

      g_type_interface_add_prerequisite (file_chooser_type, GTK_TYPE_WIDGET);
    }

  return file_chooser_type;
}

static void
gtk_file_chooser_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_class);

      g_signal_new ("current_folder_changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileChooserIface, current_folder_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
      g_signal_new ("selection_changed",
		    iface_type,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkFileChooserIface, selection_changed),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      g_object_interface_install_property (iface_type,
					   g_param_spec_enum ("action",
							      _("Action"),
							      _("The type of action that the file selector is performing"),
							      GTK_TYPE_FILE_CHOOSER_ACTION,
							      GTK_FILE_CHOOSER_ACTION_OPEN,
							      G_PARAM_READWRITE));
      g_object_interface_install_property (iface_type,
					   g_param_spec_object ("file_system",
								_("File System"),
								_("File system object to use"),
								GTK_TYPE_FILE_SYSTEM,
								G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
      g_object_interface_install_property (iface_type,
					   g_param_spec_boolean ("folder_mode",
								 _("Folder Mode"),
								 _("Whether to select folders rather than files"),
								 FALSE,
								 G_PARAM_READWRITE));
      g_object_interface_install_property (iface_type,
					   g_param_spec_boolean ("local_only",
								 _("Local Only"),
								 _("Whether the selected file(s) should be limited to local file: URLs"),
								 TRUE,
								 G_PARAM_READWRITE));
      g_object_interface_install_property (iface_type,
					   g_param_spec_object ("preview_widget",
								_("Preview widget"),
								_("Application supplied widget for custom previews."),
								GTK_TYPE_WIDGET,
								G_PARAM_READWRITE));
      g_object_interface_install_property (iface_type,
					   g_param_spec_boolean ("preview_widget_active",
								 _("Preview Widget Active"),
								 _("Whther the application supplied widget for custom previews should be shown."),
								 TRUE,
								 G_PARAM_READWRITE));
      g_object_interface_install_property (iface_type,
					   g_param_spec_boolean ("select_multiple",
								 _("Select Multiple"),
								 _("Whether to allow multiple files to be selected"),
								 FALSE,
								 G_PARAM_READWRITE));
      
      g_object_interface_install_property (iface_type,
					   g_param_spec_boolean ("show_hidden",
								 _("Show Hidden"),
								 _("Whether the hidden files and folders should be displayed"),
								 FALSE,
								 G_PARAM_READWRITE));
      initialized = TRUE;
    }
}

void
gtk_file_chooser_set_action (GtkFileChooser       *chooser,
			     GtkFileChooserAction  action)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "action", action, NULL);
}

GtkFileChooserAction
gtk_file_chooser_get_action (GtkFileChooser *chooser)
{
  GtkFileChooserAction action;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "action", &action, NULL);

  return action;
}

void
gtk_file_chooser_set_directory_mode (GtkFileChooser *chooser,
				      gboolean        directory_mode)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "directory_mode", directory_mode, NULL);
}

gboolean
gtk_file_chooser_get_directory_mode (GtkFileChooser *chooser)
{
  gboolean directory_mode;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "directory_mode", &directory_mode, NULL);

  return directory_mode;
}

void
gtk_file_chooser_set_local_only (GtkFileChooser *chooser,
				 gboolean        local_only)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "local_only", local_only, NULL);
}

gboolean
gtk_file_chooser_get_local_only (GtkFileChooser *chooser)
{
  gboolean local_only;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "local_only", &local_only, NULL);

  return local_only;
}

void
gtk_file_chooser_set_select_multiple (GtkFileChooser *chooser,
				      gboolean        select_multiple)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "select_multiple", select_multiple, NULL);
}

gboolean
gtk_file_chooser_get_select_multiple (GtkFileChooser *chooser)
{
  gboolean select_multiple;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "select_multiple", &select_multiple, NULL);

  return select_multiple;
}

gchar *
gtk_file_chooser_get_filename (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  path = gtk_file_chooser_get_path (chooser);
  if (path)
    {
      result = gtk_file_system_path_to_filename (file_system, path);
      gtk_file_path_free (path);
    }

  return result;
}

void
gtk_file_chooser_set_filename (GtkFileChooser *chooser,
			       const gchar    *filename)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  gtk_file_chooser_unselect_all (chooser);
  gtk_file_chooser_select_filename (chooser, filename);
}

void
gtk_file_chooser_select_filename (GtkFileChooser *chooser,
				  const gchar    *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      _gtk_file_chooser_select_path (chooser, path);
      gtk_file_path_free (path);
    }
}

void
gtk_file_chooser_unselect_filename (GtkFileChooser *chooser,
				    const char     *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      _gtk_file_chooser_unselect_path (chooser, path);
      gtk_file_path_free (path);
    }
}

GSList *
gtk_file_chooser_get_filenames (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GSList *paths;
  GSList *tmp_list;
  GSList *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  paths = _gtk_file_chooser_get_paths (chooser);

  for (tmp_list = paths; tmp_list; tmp_list = tmp_list->next)
    {
      gchar *filename = gtk_file_system_path_to_filename (file_system, tmp_list->data);
      if (filename)
	result = g_slist_prepend (result, filename);
    }

  gtk_file_paths_free (paths);

  return g_slist_reverse (result);
}

void
gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
				     const gchar    *filename)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (filename != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_filename_to_path (file_system, filename);
  if (path)
    {
      _gtk_file_chooser_set_current_folder (chooser, path);
      gtk_file_path_free (path);
    }
}

gchar *
gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *filename;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = _gtk_file_chooser_get_current_folder (chooser);
  filename = gtk_file_system_path_to_filename (file_system, path);
  gtk_file_path_free (path);

  return filename;
}

gchar *
gtk_file_chooser_get_uri (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  path = gtk_file_chooser_get_path (chooser);
  if (path)
    {
      result = gtk_file_system_path_to_uri (file_system, path);
      gtk_file_path_free (path);
    }

  return result;
}

void
gtk_file_chooser_set_uri (GtkFileChooser *chooser,
			  const char     *uri)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  gtk_file_chooser_unselect_all (chooser);
  gtk_file_chooser_select_uri (chooser, uri);
}

void
gtk_file_chooser_select_uri (GtkFileChooser *chooser,
			     const char     *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      _gtk_file_chooser_select_path (chooser, path);
      gtk_file_path_free (path);
    }
}

void
gtk_file_chooser_unselect_uri (GtkFileChooser *chooser,
			       const char     *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      _gtk_file_chooser_unselect_path (chooser, path);
      gtk_file_path_free (path);
    }
}

void
gtk_file_chooser_select_all (GtkFileChooser *chooser)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_all (chooser);
}

void
gtk_file_chooser_unselect_all (GtkFileChooser *chooser)
{

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

GSList *
gtk_file_chooser_get_uris (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GSList *paths;
  GSList *tmp_list;
  GSList *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);
  paths = _gtk_file_chooser_get_paths (chooser);

  for (tmp_list = paths; tmp_list; tmp_list = tmp_list->next)
    {
      gchar *uri = gtk_file_system_path_to_uri (file_system, tmp_list->data);
      if (uri)
	result = g_slist_prepend (result, uri);
    }

  gtk_file_paths_free (paths);

  return g_slist_reverse (result);
}

void
gtk_file_chooser_set_current_folder_uri (GtkFileChooser *chooser,
					 const gchar    *uri)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (uri != NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = gtk_file_system_uri_to_path (file_system, uri);
  if (path)
    {
      _gtk_file_chooser_set_current_folder (chooser, path);
      gtk_file_path_free (path);
    }
}

gchar *
gtk_file_chooser_get_current_folder_uri (GtkFileChooser *chooser)
{
  GtkFileSystem *file_system;
  GtkFilePath *path;
  gchar *uri;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  file_system = _gtk_file_chooser_get_file_system (chooser);

  path = _gtk_file_chooser_get_current_folder (chooser);
  uri = gtk_file_system_path_to_uri (file_system, path);
  gtk_file_path_free (path);

  return uri;
}


void
_gtk_file_chooser_set_current_folder (GtkFileChooser    *chooser,
				      const GtkFilePath *path)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  g_return_if_fail (path != NULL);

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->set_current_folder (chooser, path);
}

GtkFilePath *
_gtk_file_chooser_get_current_folder (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_current_folder (chooser);  
}

void
_gtk_file_chooser_select_path (GtkFileChooser    *chooser,
			       const GtkFilePath *path)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->select_path (chooser, path);
}

void
_gtk_file_chooser_unselect_path (GtkFileChooser    *chooser,
				 const GtkFilePath *path)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  GTK_FILE_CHOOSER_GET_IFACE (chooser)->unselect_path (chooser, path);
}

GSList *
_gtk_file_chooser_get_paths (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_paths (chooser);
}

static GtkFilePath *
gtk_file_chooser_get_path (GtkFileChooser *chooser)
{
  GSList *list;
  GtkFilePath *result = NULL;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  list = _gtk_file_chooser_get_paths (chooser);
  if (list)
    {
      result = list->data;
      list = g_slist_delete_link (list, list);
      gtk_file_paths_free (list);
    }

  return result;
}

GtkFileSystem *
_gtk_file_chooser_get_file_system (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return GTK_FILE_CHOOSER_GET_IFACE (chooser)->get_file_system (chooser);
}

/* Preview widget
 */
void
gtk_file_chooser_set_preview_widget (GtkFileChooser *chooser,
				     GtkWidget      *preview_widget)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  g_object_set (chooser, "preview_widget", preview_widget, NULL);
}

GtkWidget *
gtk_file_chooser_get_preview_widget (GtkFileChooser *chooser)
{
  GtkWidget *preview_widget;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  g_object_get (chooser, "preview_widget", &preview_widget, NULL);

  return preview_widget;
}

void
gtk_file_chooser_set_preview_widget_active (GtkFileChooser *chooser,
					    gboolean        active)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
  
  g_object_set (chooser, "preview_widget_active", active, NULL);
}

gboolean
gtk_file_chooser_get_preview_widget_active (GtkFileChooser *chooser)
{
  gboolean active;
  
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "preview_widget_active", &active, NULL);

  return active;
}

const char *
gtk_file_chooser_get_preview_filename (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return NULL;
}

const char *
gtk_file_chooser_get_preview_uri (GtkFileChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  return NULL;
}
