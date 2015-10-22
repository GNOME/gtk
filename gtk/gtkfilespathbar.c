/* gtkfilespathbar.c
 *
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include "gtkfilespathbar.h"
#include "gtkpathbar.h"
#include "gtkpopover.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtktypebuiltins.h"

#define LOCAL_FILESYSTEM_ROOT_URI "file:///"
#define OTHER_LOCATIONS_URI "other-locations:///"
/**
 * SECTION:gtkfilespathbar
 * @Short_description: Widget that displays a path in UNIX format in a button-like manner
 * @Title: GtkFilesPathBar
 * @See_also: #GtkPathBar, #GtkFileChooser
 *
 * #GtkFilesPathBar is a stock widget that displays a path in UNIX format in a way that
 * the user can interact with it, selecting part of it or providing menus for
 * every part of the path.
 *
 * Given the usual big lenght of paths, it conveniently manages the overflow of it,
 * hiding the parts of the path that doesn't have snouegh space to be displayed
 * in a overflow popover
 */

struct _GtkFilesPathBarPrivate
{
  GtkWidget *path_bar;

  GFile *file;
  GCancellable *cancellable;
  GFileMonitor *monitor;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkFilesPathBar, gtk_files_path_bar, GTK_TYPE_BIN)

enum {
  POPULATE_POPUP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

static GParamSpec *files_path_bar_properties[LAST_PROP] = { NULL, };
static guint files_path_bar_signals[LAST_SIGNAL] = { 0 };

typedef struct {
  GtkFilesPathBar *path_bar;
  GString *display_path;
  GFile *root_file;
  gchar *root_label;
  GIcon *root_icon;
  GCancellable *cancellable;
} PathFilesInfo;

static GMount*
get_mounted_mount_for_root (GFile *file)
{
  GVolumeMonitor *volume_monitor;
  GList *mounts;
  GList *l;
  GMount *mount;
  GMount *result = NULL;
  GFile *root = NULL;
  GFile *default_location = NULL;

  volume_monitor = g_volume_monitor_get ();
  mounts = g_volume_monitor_get_mounts (volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      mount = l->data;

      if (g_mount_is_shadowed (mount))
        continue;

      root = g_mount_get_root (mount);
      if (g_file_equal (file, root))
        {
          result = g_object_ref (mount);
          break;
        }

      default_location = g_mount_get_default_location (mount);
      if (!g_file_equal (default_location, root) &&
          g_file_equal (file, default_location))
        {
          result = g_object_ref (mount);
          break;
        }
    }

  g_clear_object (&root);
  g_clear_object (&default_location);
  g_list_free_full (mounts, g_object_unref);

  return result;
}

static gboolean
file_is_home_dir (GFile *file)
{
  gboolean is_home_dir;
  gchar *path;

  path = g_file_get_path (file);

  is_home_dir = g_strcmp0 (path, g_get_home_dir ()) == 0;

  if (path)
    g_free (path);

  return is_home_dir;
}

static gboolean
file_is_absolute_root (GFile *file)
{
  gboolean is_filesystem_root;
  gchar *file_basename;

  file_basename = g_file_get_basename (file);
  is_filesystem_root = g_strcmp0 (file_basename, G_DIR_SEPARATOR_S) == 0;

  g_free (file_basename);

  return is_filesystem_root;
}

static gboolean
file_is_root (GFile *file)
{
  GMount *mount;
  gboolean is_root = FALSE;

  mount = get_mounted_mount_for_root (file);

  is_root = file_is_absolute_root (file) || file_is_home_dir (file) || mount;

  g_clear_object (&mount);

  return is_root;
}

static GIcon*
get_root_icon (GFile *file)
{
  GIcon *icon = NULL;
  GFile *local_filesystem_file;

  local_filesystem_file = g_file_new_for_uri (LOCAL_FILESYSTEM_ROOT_URI);
  if (g_file_equal (file, local_filesystem_file))
    icon = g_themed_icon_new ("drive-harddisk");

  g_object_unref (local_filesystem_file);

  return icon;
}

static gchar*
get_root_label (GFile *file)
{
  gchar *label = NULL;
  gchar *path;
  GMount *mount;
  GFile *other_locations;

  path = g_file_get_path (file);
  mount = get_mounted_mount_for_root (file);
  other_locations = g_file_new_for_uri (OTHER_LOCATIONS_URI);
  if (g_strcmp0 (path, g_get_home_dir ()) == 0)
    label = g_strdup (_("Home"));
  else if (g_file_equal (file, other_locations))
    label = g_strdup (_("Other Locations"));
  else if (mount)
    label = g_mount_get_name (mount);

  if (path)
    g_free (path);
  g_clear_object (&mount);
  g_object_unref (other_locations);

  return label;
}

static void
free_path_files_info (PathFilesInfo *path_files_info)
{
  if (path_files_info->display_path)
    g_string_free (path_files_info->display_path, TRUE);
  if (path_files_info->root_label)
    g_free (path_files_info->root_label);

  g_clear_object (&path_files_info->cancellable);
  g_clear_object (&path_files_info->root_file);
  g_clear_object (&path_files_info->root_icon);

  g_slice_free (PathFilesInfo, path_files_info);
}

static void
on_all_path_files_info_queried (PathFilesInfo *path_files_info)
{
  GtkFilesPathBarPrivate *priv = gtk_files_path_bar_get_instance_private (path_files_info->path_bar);
  gchar *root_uri;

  root_uri = g_file_get_uri (path_files_info->root_file);
  gtk_path_bar_set_path_extended (GTK_PATH_BAR (priv->path_bar),
                                  path_files_info->display_path->str, root_uri,
                                  path_files_info->root_label, path_files_info->root_icon);

  g_free (root_uri);
  free_path_files_info (path_files_info);
}

static void
on_queried_file_info (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GFile *file = G_FILE (object);
  GError *error = NULL;
  PathFilesInfo *path_files_info = (PathFilesInfo *) user_data;
  GFileInfo *file_info = NULL;
  gchar *uri;

  uri = g_file_get_uri (file);
  if (g_cancellable_is_cancelled (path_files_info->cancellable))
    {
      free_path_files_info (path_files_info);
      goto out;
    }

  file_info = g_file_query_info_finish (file, result, &error);
  if (!error || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    {
      if (file_is_root (file))
        {

          uri = g_file_get_uri (file);
          path_files_info->root_icon = get_root_icon (file);
          path_files_info->root_label = get_root_label (file);
          path_files_info->root_file = g_object_ref (file);
          /* If it is not specific root managed by us, just query display name */
          if (!path_files_info->root_label && !path_files_info->root_icon)
            {
              if (!error)
                {
                  path_files_info->root_label = g_strdup (g_file_info_get_display_name (file_info));
                }
              else
                {
                  g_warning ("Error trying to get info from %s, location not supported", uri);

                  free_path_files_info (path_files_info);

                  goto out;
                }
            }

          g_string_prepend (path_files_info->display_path, uri);
          on_all_path_files_info_queried (path_files_info);
        }
      else
        {
          GFile *parent;

          parent = g_file_get_parent (file);

          g_string_prepend (path_files_info->display_path, g_file_info_get_display_name (file_info));
          g_string_prepend (path_files_info->display_path, G_DIR_SEPARATOR_S);
          g_file_query_info_async (parent,
                                   "standard::display-name",
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT,
                                   path_files_info->cancellable,
                                   on_queried_file_info,
                                   path_files_info);
        }
    }
  else
    {
      g_warning ("%s", error->message);

      free_path_files_info (path_files_info);
    }

out:
  g_object_unref (file);
  g_clear_object (&file_info);
  if (error)
    g_error_free (error);
  g_free (uri);
}

static void
on_path_bar_populate_popup (GtkPathBar      *path_bar,
                            GtkWidget       *container,
                            const gchar     *selected_path,
                            GtkFilesPathBar *self)
{
  GFile *file;

  file = g_file_new_for_path (selected_path);
  g_signal_emit (self, files_path_bar_signals[POPULATE_POPUP], 0,
                 container, file);
}

static void
on_path_bar_selected_path (GtkPathBar      *path_bar,
                           GParamSpec      *pspec,
                           GtkFilesPathBar *self)
{
  GFile *file;

  file = g_file_new_for_uri (gtk_path_bar_get_selected_path (path_bar));

  gtk_files_path_bar_set_file (self, file);

  g_object_unref (file);
}

static void
gtk_files_path_bar_dispose (GObject *object)
{
  GtkFilesPathBar *self = (GtkFilesPathBar *)object;
  GtkFilesPathBarPrivate *priv = gtk_files_path_bar_get_instance_private (self);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->file);

  G_OBJECT_CLASS (gtk_files_path_bar_parent_class)->dispose (object);
}

static void
gtk_files_path_bar_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkFilesPathBar *self = GTK_FILES_PATH_BAR (object);
  GtkFilesPathBarPrivate *priv = gtk_files_path_bar_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, priv->file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_files_path_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkFilesPathBar *self = GTK_FILES_PATH_BAR (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gtk_files_path_bar_set_file (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_files_path_bar_class_init (GtkFilesPathBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_files_path_bar_dispose;
  object_class->get_property = gtk_files_path_bar_get_property;
  object_class->set_property = gtk_files_path_bar_set_property;

  /**
   * GtkFilesPathBar::populate-popup:
   * @files_path_bar: the object which received the signal.
   * @container: (type Gtk.Widget): a #GtkContainer
   * @path: (type const gchar*): string of the path where the user performed a right click.
   *
   * The path bar emits this signal when the user invokes a contextual
   * popup on one of its items. In the signal handler, the application may
   * add extra items to the menu as appropriate. For example, a file manager
   * may want to add a "Properties" command to the menu.
   *
   * The @container and all its contents are destroyed after the user
   * dismisses the popup. The popup is re-created (and thus, this signal is
   * emitted) every time the user activates the contextual menu.
   *
   * Since: 3.20
   */
  files_path_bar_signals [POPULATE_POPUP] =
          g_signal_new (I_("populate-popup"),
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkFilesPathBarClass, populate_popup),
                        NULL, NULL,
                        _gtk_marshal_VOID__OBJECT_STRING,
                        G_TYPE_NONE, 2,
                        GTK_TYPE_WIDGET,
                        G_TYPE_STRING);

  files_path_bar_properties[PROP_FILE] =
          g_param_spec_object ("file",
                               P_("File"),
                               P_("The file set in the path bar."),
                               G_TYPE_FILE,
                               G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, files_path_bar_properties);

  gtk_widget_class_set_css_name (widget_class, "files-path-bar");
}

static void
gtk_files_path_bar_init (GtkFilesPathBar *self)
{
  GtkFilesPathBarPrivate *priv = gtk_files_path_bar_get_instance_private (self);

  g_type_ensure (GTK_TYPE_FILES_PATH_BAR);

  priv->path_bar = gtk_path_bar_new ();
  gtk_path_bar_set_inverted (GTK_PATH_BAR (priv->path_bar), TRUE);
  gtk_widget_show (priv->path_bar);
  g_signal_connect (GTK_PATH_BAR (priv->path_bar), "populate-popup",
                    G_CALLBACK (on_path_bar_populate_popup), self);
  g_signal_connect (GTK_PATH_BAR (priv->path_bar), "notify::selected-path",
                    G_CALLBACK (on_path_bar_selected_path), self);
  gtk_container_add (GTK_CONTAINER (self), priv->path_bar);
}

/**
 * gtk_files_path_bar_get_file:
 * @files_path_bar: a #GtkFilesPathBar
 *
 * Get the path represented by the files path bar
 *
 * Returns: (transfer none): The current #GFile.
 *
 * Since: 3.20
 */
GFile*
gtk_files_path_bar_get_file (GtkFilesPathBar *self)
{
  GtkFilesPathBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_FILES_PATH_BAR (self), NULL);

  priv = gtk_files_path_bar_get_instance_private (GTK_FILES_PATH_BAR (self));

  return priv->file;
}

/**
 * gtk_files_path_bar_set_file:
 * @files_path_bar: a #GtkFilesPathBar
 * @file: the #GFile.
 *
 * Set the #GFile represented by the path bar.
 *
 * Since: 3.20
 */
void
gtk_files_path_bar_set_file (GtkFilesPathBar *self,
                             GFile           *file)
{
  GtkFilesPathBarPrivate *priv;
  PathFilesInfo *path_files_info;

  g_return_if_fail (GTK_IS_FILES_PATH_BAR (self));

  priv = gtk_files_path_bar_get_instance_private (GTK_FILES_PATH_BAR (self));

  if (priv->file && g_file_equal (priv->file, file))
    return;

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  g_clear_object (&priv->file);
  priv->file = g_object_ref (file);

  path_files_info = g_slice_new (PathFilesInfo);
  path_files_info->path_bar = self;
  path_files_info->display_path = g_string_new ("");
  path_files_info->root_file = NULL;
  path_files_info->root_label = NULL;
  path_files_info->root_icon = NULL;
  path_files_info->cancellable = g_object_ref (priv->cancellable);

  g_file_query_info_async (g_object_ref (file),
                           "standard::display-name",
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           path_files_info->cancellable,
                           on_queried_file_info,
                           path_files_info);

  g_object_notify_by_pspec (G_OBJECT (self), files_path_bar_properties[PROP_FILE]);
}

GtkWidget *
gtk_files_path_bar_new (void)
{
  return g_object_new (GTK_TYPE_FILES_PATH_BAR, NULL);
}
