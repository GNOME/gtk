/* GTK - The GIMP Toolkit
 * gtkfilechooserutils.c: Private utility functions useful for
 *                        implementing a GtkFileChooser interface
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

#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilechooserenums.h"

static void    delegate_set_current_folder (GtkFileChooser *chooser,
					    const char     *uri);
static char *  delegate_get_current_folder (GtkFileChooser *chooser);
static void    delegate_select_uri         (GtkFileChooser *chooser,
					    const char     *uri);
static void    delegate_unselect_uri       (GtkFileChooser *chooser,
					    const char     *uri);
static void    delegate_select_all         (GtkFileChooser *chooser);
static void    delegate_unselect_all       (GtkFileChooser *chooser);
static GSList *delegate_get_uris           (GtkFileChooser *chooser);

static void    delegate_current_folder_changed (GtkFileChooser *chooser,
						gpointer        data);
static void    delegate_selection_changed      (GtkFileChooser *chooser,
						gpointer        data);

void
_gtk_file_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_ACTION,
				   g_param_spec_override ("action",
							  GTK_TYPE_FILE_CHOOSER_ACTION,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_FOLDER_MODE,
				   g_param_spec_override ("folder_mode",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_LOCAL_ONLY,
				   g_param_spec_override ("local_only",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET,
				   g_param_spec_override ("preview_widget",
							  GTK_TYPE_WIDGET,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE,
				   g_param_spec_override ("preview_widget_active",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
				   g_param_spec_override ("select_multiple",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
  g_object_class_install_property (klass,
				   GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN,
				   g_param_spec_override ("show_hidden",
							  G_TYPE_BOOLEAN,
							  G_PARAM_READWRITE));
}

void
_gtk_file_chooser_delegate_iface_init (GtkFileChooserIface *iface)
{
  iface->set_current_folder = delegate_set_current_folder;
  iface->get_current_folder = delegate_get_current_folder;
  iface->select_uri = delegate_select_uri;
  iface->unselect_uri = delegate_unselect_uri;
  iface->select_all = delegate_select_all;
  iface->unselect_all = delegate_unselect_all;
  iface->get_uris = delegate_get_uris;
}

void
_gtk_file_chooser_set_delegate (GtkFileChooser *receiver,
				GtkFileChooser *delegate)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER (receiver));
  g_return_if_fail (GTK_IS_FILE_CHOOSER (delegate));
  
  g_object_set_data (G_OBJECT (receiver), "gtk-file-chooser-delegate", delegate);

  g_signal_connect (delegate, "current_folder_changed",
		    G_CALLBACK (delegate_current_folder_changed), receiver);
  g_signal_connect (delegate, "selection_changed",
		    G_CALLBACK (delegate_selection_changed), receiver);
}

GtkFileChooser *
get_delegate (GtkFileChooser *receiver)
{
  return g_object_get_data (G_OBJECT (receiver), "gtk-file-chooser-delegate");
}

static void
delegate_select_uri (GtkFileChooser *chooser,
		     const char     *uri)
{
  gtk_file_chooser_select_uri (get_delegate (chooser), uri);
}

static void
delegate_unselect_uri (GtkFileChooser *chooser,
				      const char     *uri)
{
  gtk_file_chooser_unselect_uri (get_delegate (chooser), uri);
}

static void
delegate_select_all (GtkFileChooser *chooser)
{
  gtk_file_chooser_select_all (get_delegate (chooser));
}

static void
delegate_unselect_all (GtkFileChooser *chooser)
{
  gtk_file_chooser_unselect_all (get_delegate (chooser));
}

static GSList *
delegate_get_uris (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_uris (get_delegate (chooser));
}

static void
delegate_set_current_folder (GtkFileChooser *chooser,
			     const char     *uri)
{
  gtk_file_chooser_set_current_folder_uri (chooser, uri);
}

static char *
delegate_get_current_folder (GtkFileChooser *chooser)
{
  return gtk_file_chooser_get_current_folder_uri (get_delegate (chooser));
}

static void
delegate_selection_changed (GtkFileChooser *chooser,
			    gpointer        data)
{
  g_signal_emit_by_name (data, "selection_changed", 0);
}

static void
delegate_current_folder_changed (GtkFileChooser *chooser,
				 gpointer        data)
{
  g_signal_emit_by_name (data, "current_folder_changed", 0);
}
