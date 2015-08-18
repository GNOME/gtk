/* gtkplacesview.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkplacesviewprivate.h"
#include "gtkplacesviewrowprivate.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtkplacesview
 * @Short_description: Widget that displays persistent drives and manages mounted networks
 * @Title: GtkPlacesView
 * @See_also: #GtkFileChooser
 *
 * #GtkPlacesView is a stock widget that displays a list of persistent drives
 * such as harddisk partitions and networks.  #GtkPlacesView does not monitor
 * removable devices.
 *
 * The places view displays drives and networks, and will automatically mount
 * them when the user activates. Network addresses are stored even if they fail
 * to connect. When the connection is successful, the connected network is
 * shown at the network list.
 *
 * To make use of the places view, an application at least needs to connect
 * to the #GtkPlacesView::open-location signal. This is emitted when the user
 * selects a location to open in the view.
 */

struct _GtkPlacesViewPrivate
{
  GVolumeMonitor                *volume_monitor;
  GtkPlacesOpenFlags             open_flags;
  GtkPlacesOpenFlags             current_open_flags;

  GFile                         *server_list_file;
  GFileMonitor                  *server_list_monitor;

  GCancellable                  *cancellable;

  gchar                         *search_query;

  GtkWidget                     *actionbar;
  GtkWidget                     *address_entry;
  GtkWidget                     *connect_button;
  GtkWidget                     *listbox;
  GtkWidget                     *popup_menu;
  GtkWidget                     *recent_servers_listbox;
  GtkWidget                     *recent_servers_popover;
  GtkWidget                     *recent_servers_stack;
  GtkWidget                     *stack;

  GtkEntryCompletion            *address_entry_completion;
  GtkListStore                  *completion_store;

  guint                          local_only : 1;
  guint                          should_open_location : 1;
  guint                          should_pulse_entry : 1;
  guint                          entry_pulse_timeout_id;
  guint                          connecting_to_server : 1;
};

static void        mount_volume                                  (GtkPlacesView *view,
                                                                  GVolume       *volume);

static gboolean    on_button_press_event                         (GtkPlacesViewRow *row,
                                                                  GdkEventButton   *event);

static void        on_eject_button_clicked                       (GtkWidget        *widget,
                                                                  GtkPlacesViewRow *row);

static gboolean    on_row_popup_menu                             (GtkPlacesViewRow *row);

static void        populate_servers                              (GtkPlacesView *view);

G_DEFINE_TYPE_WITH_PRIVATE (GtkPlacesView, gtk_places_view, GTK_TYPE_BOX)

/* GtkPlacesView properties & signals */
enum {
  PROP_0,
  PROP_LOCAL_ONLY,
  PROP_OPEN_FLAGS,
  LAST_PROP
};

enum {
  OPEN_LOCATION,
  SHOW_ERROR_MESSAGE,
  LAST_SIGNAL
};

const gchar *unsupported_protocols [] =
{
  "file", "afc", "obex", "http",
  "trash", "burn", "computer",
  "archive", "recent", "localtest",
  NULL
};

static guint places_view_signals [LAST_SIGNAL] = { 0 };
static GParamSpec *properties [LAST_PROP];

static void
emit_open_location (GtkPlacesView      *view,
                    GFile              *location,
                    GtkPlacesOpenFlags  open_flags)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (view);

  if ((open_flags & priv->open_flags) == 0)
    open_flags = GTK_PLACES_OPEN_NORMAL;

  g_signal_emit (view, places_view_signals[OPEN_LOCATION], 0, location, open_flags);
}

static void
emit_show_error_message (GtkPlacesView *view,
                         gchar         *primary_message,
                         gchar         *secondary_message)
{
  g_signal_emit (view, places_view_signals[SHOW_ERROR_MESSAGE],
                         0, primary_message, secondary_message);
}

static void
server_file_changed_cb (GtkPlacesView *view)
{
  populate_servers (view);
}

static GBookmarkFile *
server_list_load (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;
  GBookmarkFile *bookmarks;
  GError *error = NULL;
  gchar *datadir;
  gchar *filename;

  priv = gtk_places_view_get_instance_private (view);
  bookmarks = g_bookmark_file_new ();
  datadir = g_build_filename (g_get_user_config_dir (), "gtk-3.0", NULL);
  filename = g_build_filename (datadir, "servers", NULL);

  g_mkdir_with_parents (datadir, 0700);
  g_bookmark_file_load_from_file (bookmarks, filename, &error);

  if (error)
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          /* only warn if the file exists */
          g_warning ("Unable to open server bookmarks: %s", error->message);
          g_clear_pointer (&bookmarks, g_bookmark_file_free);
        }

      g_clear_error (&error);
    }

  /* Monitor the file in case it's modified outside this code */
  if (!priv->server_list_monitor)
    {
      priv->server_list_file = g_file_new_for_path (filename);

      if (priv->server_list_file)
        {
          priv->server_list_monitor = g_file_monitor_file (priv->server_list_file,
                                                           G_FILE_MONITOR_NONE,
                                                           NULL,
                                                           &error);

          if (error)
            {
              g_warning ("Cannot monitor server file: %s", error->message);
              g_clear_error (&error);
            }
          else
            {
              g_signal_connect_swapped (priv->server_list_monitor,
                                        "changed",
                                        G_CALLBACK (server_file_changed_cb),
                                        view);
            }
        }

      g_clear_object (&priv->server_list_file);
    }

  g_free (datadir);
  g_free (filename);

  return bookmarks;
}

static void
server_list_save (GBookmarkFile *bookmarks)
{
  gchar *filename;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "servers", NULL);
  g_bookmark_file_to_file (bookmarks, filename, NULL);
  g_free (filename);
}

static void
server_list_add_server (GtkPlacesView *view,
                        GFile         *file)
{
  GBookmarkFile *bookmarks;
  GFileInfo *info;
  GError *error;
  gchar *title;
  gchar *uri;

  error = NULL;
  bookmarks = server_list_load (view);

  if (!bookmarks)
    return;

  uri = g_file_get_uri (file);

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  title = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

  g_bookmark_file_set_title (bookmarks, uri, title);
  g_bookmark_file_set_visited (bookmarks, uri, -1);
  g_bookmark_file_add_application (bookmarks, uri, NULL, NULL);

  server_list_save (bookmarks);

  g_bookmark_file_free (bookmarks);
  g_clear_object (&info);
  g_free (title);
  g_free (uri);
}

static void
server_list_remove_server (GtkPlacesView *view,
                           const gchar   *uri)
{
  GBookmarkFile *bookmarks;

  bookmarks = server_list_load (view);

  if (!bookmarks)
    return;

  g_bookmark_file_remove_item (bookmarks, uri, NULL);
  server_list_save (bookmarks);

  g_bookmark_file_free (bookmarks);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return NULL;
  else
    return GTK_WINDOW (toplevel);
}

static void
set_busy_cursor (GtkPlacesView *view,
                 gboolean       busy)
{
  GtkWidget *widget;
  GtkWindow *toplevel;
  GdkDisplay *display;
  GdkCursor *cursor;

  toplevel = get_toplevel (GTK_WIDGET (view));
  widget = GTK_WIDGET (toplevel);
  if (!toplevel || !gtk_widget_get_realized (widget))
    return;

  display = gtk_widget_get_display (widget);

  if (busy)
    {
      cursor = gdk_cursor_new_from_name (display, "left_ptr_watch");
      if (cursor == NULL)
        cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
    }
  else
    cursor = NULL;

  gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
  gdk_display_flush (display);

  if (cursor)
    g_object_unref (cursor);
}

/* Activates the given row, with the given flags as parameter */
static void
activate_row (GtkPlacesView      *view,
              GtkPlacesViewRow   *row,
              GtkPlacesOpenFlags  flags)
{
  GtkPlacesViewPrivate *priv;
  GVolume *volume;
  GMount *mount;
  GFile *file;

  priv = gtk_places_view_get_instance_private (view);
  mount = gtk_places_view_row_get_mount (row);
  volume = gtk_places_view_row_get_volume (row);
  file = gtk_places_view_row_get_file (row);

  if (file)
    {
      emit_open_location (view, file, flags);
    }
  else if (mount)
    {
      GFile *location = g_mount_get_default_location (mount);

      emit_open_location (view, location, flags);

      g_object_unref (location);
    }
  else if (volume && g_volume_can_mount (volume))
    {
      /*
       * When the row is activated, the unmounted volume shall
       * be mounted and opened right after.
       */
      priv->should_open_location = TRUE;

      gtk_places_view_row_set_busy (row, TRUE);
      mount_volume (view, volume);
    }
}

static void update_places (GtkPlacesView *view);

static void
gtk_places_view_finalize (GObject *object)
{
  GtkPlacesView *self = (GtkPlacesView *)object;
  GtkPlacesViewPrivate *priv = gtk_places_view_get_instance_private (self);

  g_signal_handlers_disconnect_by_func (priv->volume_monitor, update_places, object);

  if (priv->entry_pulse_timeout_id > 0)
    g_source_remove (priv->entry_pulse_timeout_id);

  g_cancellable_cancel (priv->cancellable);

  g_clear_pointer (&priv->search_query, g_free);
  g_clear_object (&priv->server_list_file);
  g_clear_object (&priv->server_list_monitor);
  g_clear_object (&priv->volume_monitor);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (gtk_places_view_parent_class)->finalize (object);
}

static void
gtk_places_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkPlacesView *self = GTK_PLACES_VIEW (object);

  switch (prop_id)
    {
    case PROP_LOCAL_ONLY:
      g_value_set_boolean (value, gtk_places_view_get_local_only (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_places_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkPlacesView *self = GTK_PLACES_VIEW (object);

  switch (prop_id)
    {
    case PROP_LOCAL_ONLY:
      gtk_places_view_set_local_only (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
is_removable_volume (GVolume *volume)
{
  gboolean is_removable;
  GDrive *drive;
  GMount *mount;
  gchar *id;

  drive = g_volume_get_drive (volume);
  mount = g_volume_get_mount (volume);
  id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

  is_removable = g_volume_can_eject (volume);

  /* NULL volume identifier only happens on removable devices */
  is_removable |= !id;

  if (drive)
    is_removable |= g_drive_can_eject (drive);

  if (mount)
    is_removable |= (g_mount_can_eject (mount) && !g_mount_can_unmount (mount));

  g_clear_object (&drive);
  g_clear_object (&mount);
  g_free (id);

  return is_removable;
}

typedef struct
{
  gchar         *uri;
  GtkPlacesView *view;
} RemoveServerData;

static void
on_remove_server_button_clicked (RemoveServerData *data)
{
  server_list_remove_server (data->view, data->uri);

  populate_servers (data->view);
}

static void
populate_servers (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;
  GBookmarkFile *server_list;
  GList *children;
  gchar **uris;
  gsize num_uris;
  gint i;

  priv = gtk_places_view_get_instance_private (view);
  server_list = server_list_load (view);

  if (!server_list)
    return;

  uris = g_bookmark_file_get_uris (server_list, &num_uris);

  gtk_stack_set_visible_child_name (GTK_STACK (priv->recent_servers_stack),
                                    num_uris > 0 ? "list" : "empty");

  if (!uris)
    {
      g_bookmark_file_free (server_list);
      return;
    }

  /* clear previous items */
  children = gtk_container_get_children (GTK_CONTAINER (priv->recent_servers_listbox));
  g_list_free_full (children, (GDestroyNotify) gtk_widget_destroy);

  gtk_list_store_clear (priv->completion_store);

  for (i = 0; i < num_uris; i++)
    {
      RemoveServerData *data;
      GtkTreeIter iter;
      GtkWidget *row;
      GtkWidget *grid;
      GtkWidget *button;
      GtkWidget *label;
      gchar *name;
      gchar *dup_uri;

      name = g_bookmark_file_get_title (server_list, uris[i], NULL);
      dup_uri = g_strdup (uris[i]);

      /* add to the completion list */
      gtk_list_store_append (priv->completion_store, &iter);
      gtk_list_store_set (priv->completion_store,
                          &iter,
                          0, name,
                          1, uris[i],
                          -1);

      /* add to the recent servers listbox */
      row = gtk_list_box_row_new ();

      grid = g_object_new (GTK_TYPE_GRID,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           "border-width", 6,
                           NULL);

      /* name of the connected uri, if any */
      label = gtk_label_new (name);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_container_add (GTK_CONTAINER (grid), label);

      /* the uri itself */
      label = gtk_label_new (uris[i]);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
      gtk_container_add (GTK_CONTAINER (grid), label);

      /* remove button */
      button = gtk_button_new ();
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "sidebar-button");
      gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 2);
      gtk_container_add (GTK_CONTAINER (button),
                         gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON));

      gtk_container_add (GTK_CONTAINER (row), grid);
      gtk_container_add (GTK_CONTAINER (priv->recent_servers_listbox), row);

      /* custom data */
      data = g_new0 (RemoveServerData, 1);
      data->view = view;
      data->uri = dup_uri;

      g_object_set_data_full (G_OBJECT (row), "uri", dup_uri, g_free);
      g_object_set_data_full (G_OBJECT (row), "remove-server-data", data, g_free);

      g_signal_connect_swapped (button,
                                "clicked",
                                G_CALLBACK (on_remove_server_button_clicked),
                                data);

      gtk_widget_show_all (row);

      g_free (name);
    }

  g_strfreev (uris);
  g_bookmark_file_free (server_list);
}

static void
update_view_mode (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;
  GList *children;
  GList *l;
  gboolean show_listbox;

  priv = gtk_places_view_get_instance_private (view);
  show_listbox = FALSE;

  /* drives */
  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

  for (l = children; l; l = l->next)
    {
      /* GtkListBox filter rows by changing their GtkWidget::child-visible property */
      if (gtk_widget_get_child_visible (l->data))
        {
          show_listbox = TRUE;
          break;
        }
    }

  g_list_free (children);

  if (!show_listbox &&
      priv->search_query &&
      priv->search_query[0] != '\0')
    {
        gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "empty-search");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "browse");
    }
}

static void
insert_row (GtkPlacesView *view,
            GtkWidget     *row,
            gboolean       is_network)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (view);

  g_object_set_data (G_OBJECT (row), "is-network", GINT_TO_POINTER (is_network));

  g_signal_connect_swapped (gtk_places_view_row_get_event_box (GTK_PLACES_VIEW_ROW (row)),
                            "button-press-event",
                            G_CALLBACK (on_button_press_event),
                            row);

  g_signal_connect (row,
                    "popup-menu",
                    G_CALLBACK (on_row_popup_menu),
                    row);

  g_signal_connect (gtk_places_view_row_get_eject_button (GTK_PLACES_VIEW_ROW (row)),
                    "clicked",
                    G_CALLBACK (on_eject_button_clicked),
                    row);

  gtk_container_add (GTK_CONTAINER (priv->listbox), row);
}

static void
add_volume (GtkPlacesView *view,
            GVolume       *volume)
{
  gboolean is_network;
  GDrive *drive;
  GMount *mount;
  GFile *root;
  GIcon *icon;
  gchar *identifier;
  gchar *name;
  gchar *path;

  if (is_removable_volume (volume))
    return;

  drive = g_volume_get_drive (volume);

  if (drive)
    {
      gboolean is_removable;

      is_removable = g_drive_is_media_removable (drive) ||
                     g_volume_can_eject (volume);
      g_object_unref (drive);

      if (is_removable)
        return;
    }

  identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);
  is_network = g_strcmp0 (identifier, "network") == 0;

  mount = g_volume_get_mount (volume);
  root = mount ? g_mount_get_default_location (mount) : NULL;
  icon = g_volume_get_icon (volume);
  name = g_volume_get_name (volume);
  path = !is_network ? g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE) : NULL;

  if (!mount ||
      (mount && !g_mount_is_shadowed (mount)))
    {
      GtkWidget *row;

      row = g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                          "icon", icon,
                          "name", name,
                          "path", path ? path : "",
                          "volume", volume,
                          "mount", mount,
                          "file", NULL,
                          "is-network", is_network,
                          NULL);

      insert_row (view, row, is_network);
    }

  g_clear_object (&root);
  g_clear_object (&icon);
  g_clear_object (&mount);
  g_free (identifier);
  g_free (name);
  g_free (path);
}

static void
add_mount (GtkPlacesView *view,
           GMount        *mount)
{
  gboolean is_network;
  GFile *root;
  GIcon *icon;
  gchar *name;
  gchar *path;
  gchar *uri;
  gchar *schema;

  icon = g_mount_get_icon (mount);
  name = g_mount_get_name (mount);
  root = g_mount_get_default_location (mount);
  path = root ? g_file_get_parse_name (root) : NULL;
  uri = g_file_get_uri (root);
  schema = g_uri_parse_scheme (uri);
  is_network = g_strcmp0 (schema, "file") != 0;

  if (is_network)
    g_clear_pointer (&path, g_free);

  if (!g_mount_is_shadowed (mount))
    {
      GtkWidget *row;

      row = g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                          "icon", icon,
                          "name", name,
                          "path", path ? path : "",
                          "volume", NULL,
                          "mount", mount,
                          "file", NULL,
                          "is-network", is_network,
                          NULL);

      insert_row (view, row, is_network);
    }

  g_clear_object (&root);
  g_clear_object (&icon);
  g_free (name);
  g_free (path);
  g_free (uri);
  g_free (schema);
}

static void
add_drive (GtkPlacesView *view,
           GDrive        *drive)
{
  GList *volumes;
  GList *l;

  /* Removable devices won't appear here */
  if (g_drive_can_eject (drive))
    return;

  volumes = g_drive_get_volumes (drive);

  for (l = volumes; l != NULL; l = l->next)
    add_volume (view, l->data);

  g_list_free_full (volumes, g_object_unref);
}

static void
add_computer (GtkPlacesView *view)
{
  GtkWidget *row;
  GIcon *icon;
  GFile *file;

  file = g_file_new_for_path ("/");
  icon = g_themed_icon_new_with_default_fallbacks ("drive-harddisk");

  row = g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                      "icon", icon,
                      "name", _("Computer"),
                      "path", "/",
                      "volume", NULL,
                      "mount", NULL,
                      "file", file,
                      NULL);

  insert_row (view, row, FALSE);

  g_object_unref (icon);
  g_object_unref (file);
}

static void
update_places (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;
  GList *children;
  GList *mounts;
  GList *volumes;
  GList *drives;
  GList *l;

  priv = gtk_places_view_get_instance_private (view);

  /* Clear all previously added items */
  children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));
  g_list_free_full (children, (GDestroyNotify) gtk_widget_destroy);

  /* Add "Computer" row */
  add_computer (view);

  /* Add currently connected drives */
  drives = g_volume_monitor_get_connected_drives (priv->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    add_drive (view, l->data);

  g_list_free_full (drives, g_object_unref);

  /*
   * Since all volumes with an associated GDrive were already added with
   * add_drive before, add all volumes that aren't associated with a
   * drive.
   */
  volumes = g_volume_monitor_get_volumes (priv->volume_monitor);

  for (l = volumes; l != NULL; l = l->next)
    {
      GVolume *volume;
      GDrive *drive;

      volume = l->data;
      drive = g_volume_get_drive (volume);

      if (drive)
        {
          g_object_unref (drive);
          continue;
        }

      add_volume (view, volume);
    }

  g_list_free_full (volumes, g_object_unref);

  /*
   * Now that all necessary drives and volumes were already added, add mounts
   * that have no volume, such as /etc/mtab mounts, ftp, sftp, etc.
   */
  mounts = g_volume_monitor_get_mounts (priv->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      GMount *mount;
      GVolume *volume;

      mount = l->data;
      volume = g_mount_get_volume (mount);

      if (volume)
        {
          g_object_unref (volume);
          continue;
        }

      add_mount (view, mount);
    }

  g_list_free_full (mounts, g_object_unref);

  /* load saved servers */
  populate_servers (view);

  update_view_mode (view);
}

static void
server_mount_ready_cb (GObject      *source_file,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkPlacesViewPrivate *priv;
  GtkPlacesView *view;
  gboolean should_show;
  GError *error;
  GFile *location;

  view = GTK_PLACES_VIEW (user_data);
  priv = gtk_places_view_get_instance_private (view);
  location = G_FILE (source_file);
  should_show = TRUE;
  error = NULL;

  priv->should_pulse_entry = FALSE;
  set_busy_cursor (view, FALSE);

  g_file_mount_enclosing_volume_finish (location, res, &error);
  /* Restore from Cancel to Connect */
  gtk_button_set_label (GTK_BUTTON (priv->connect_button), _("Con_nect"));
  gtk_widget_set_sensitive (priv->address_entry, TRUE);
  priv->connecting_to_server = FALSE;

  if (error)
    {
      should_show = FALSE;

      if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
        {
          /*
           * Already mounted volume is not a critical error
           * and we can still continue with the operation.
           */
          should_show = TRUE;
        }
      else if (error->domain != G_IO_ERROR ||
               (error->code != G_IO_ERROR_CANCELLED &&
                error->code != G_IO_ERROR_FAILED_HANDLED))
        {
          /* if it wasn't cancelled show a dialog */
          emit_show_error_message (view, _("Unable to access location"), error->message);
          should_show = FALSE;
        }

      g_clear_error (&error);
    }

  if (should_show)
    {
      server_list_add_server (view, location);

      /*
       * Only clear the entry if it successfully connects to the server.
       * Otherwise, the user would lost the typed address even if it fails
       * to connect.
       */
      gtk_entry_set_text (GTK_ENTRY (priv->address_entry), "");

      if (priv->should_open_location)
        {
          GMount *mount_point;
          GError *error;
          GFile *enclosing_location;

          error = NULL;
          mount_point = g_file_find_enclosing_mount (location, NULL, &error);

          if (error)
            {
              emit_show_error_message (view, _("Unable to access location"), error->message);
              g_clear_error (&error);
              goto out;
            }

          enclosing_location = g_mount_get_default_location (mount_point);

          emit_open_location (view, enclosing_location, priv->open_flags);

          g_object_unref (enclosing_location);
        }
    }

out:
  update_places (view);
}

static void
volume_mount_ready_cb (GObject      *source_volume,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkPlacesViewPrivate *priv;
  GtkPlacesView *view;
  gboolean should_show;
  GVolume *volume;
  GError *error;

  view = GTK_PLACES_VIEW (user_data);
  priv = gtk_places_view_get_instance_private (view);
  volume = G_VOLUME (source_volume);
  should_show = TRUE;
  error = NULL;

  set_busy_cursor (view, FALSE);

  g_volume_mount_finish (volume, res, &error);

  if (error)
    {
      should_show = FALSE;

      if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
        {
          /*
           * If the volume was already mounted, it's not a hard error
           * and we can still continue with the operation.
           */
          should_show = TRUE;
        }
      else if (error->domain != G_IO_ERROR ||
               (error->code != G_IO_ERROR_CANCELLED &&
                error->code != G_IO_ERROR_FAILED_HANDLED))
        {
          /* if it wasn't cancelled show a dialog */
          emit_show_error_message (GTK_PLACES_VIEW (user_data), _("Unable to access location"), error->message);
          should_show = FALSE;
        }

      g_clear_error (&error);
    }

  if (should_show)
    {
      GMount *mount;
      GFile *root;

      mount = g_volume_get_mount (volume);
      root = g_mount_get_default_location (mount);

      if (priv->should_open_location)
        emit_open_location (GTK_PLACES_VIEW (user_data), root, priv->open_flags);

      g_object_unref (mount);
      g_object_unref (root);
    }

  update_places (view);
}

static void
unmount_ready_cb (GObject      *source_mount,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GtkPlacesView *view;
  GMount *mount;
  GError *error;

  view = GTK_PLACES_VIEW (user_data);
  mount = G_MOUNT (source_mount);
  error = NULL;

  set_busy_cursor (view, FALSE);

  g_mount_unmount_with_operation_finish (mount, res, &error);

  if (error)
    {
      if (error->domain != G_IO_ERROR ||
          (error->code != G_IO_ERROR_CANCELLED &&
           error->code != G_IO_ERROR_FAILED_HANDLED))
        {
          /* if it wasn't cancelled show a dialog */
          emit_show_error_message (view, _("Unable to unmount volume"), error->message);
        }

      g_clear_error (&error);
    }
}

static gboolean
pulse_entry_cb (gpointer user_data)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (user_data));

  if (priv->should_pulse_entry)
    {
      gtk_entry_progress_pulse (GTK_ENTRY (priv->address_entry));

      return G_SOURCE_CONTINUE;
    }
  else
    {
      gtk_entry_set_progress_pulse_step (GTK_ENTRY (priv->address_entry), 0.0);
      gtk_entry_set_progress_fraction (GTK_ENTRY (priv->address_entry), 0.0);
      priv->entry_pulse_timeout_id = 0;

      return G_SOURCE_REMOVE;
    }
}

static void
unmount_mount (GtkPlacesView *view,
               GMount        *mount)
{
  GtkPlacesViewPrivate *priv;
  GMountOperation *operation;
  GtkWidget *toplevel;

  priv = gtk_places_view_get_instance_private (view);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

  set_busy_cursor (GTK_PLACES_VIEW (view), TRUE);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));
  g_mount_unmount_with_operation (mount,
                                  0,
                                  operation,
                                  priv->cancellable,
                                  unmount_ready_cb,
                                  view);
  g_object_unref (operation);
}

static void
mount_server (GtkPlacesView *view,
              GFile         *location)
{
  GtkPlacesViewPrivate *priv;
  GMountOperation *operation;
  GtkWidget *toplevel;

  priv = gtk_places_view_get_instance_private (view);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  /* User cliked when the operation was ongoing, so wanted to cancel it */
  if (priv->connecting_to_server)
    return;

  priv->cancellable = g_cancellable_new ();
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

  set_busy_cursor (view, TRUE);
  priv->should_pulse_entry = TRUE;
  gtk_entry_set_progress_pulse_step (GTK_ENTRY (priv->address_entry), 0.1);
  /* Allow to cancel the operation */
  gtk_button_set_label (GTK_BUTTON (priv->connect_button), _("Cance_l"));
  gtk_widget_set_sensitive (priv->address_entry, FALSE);
  priv->connecting_to_server = TRUE;

  if (priv->entry_pulse_timeout_id == 0)
    priv->entry_pulse_timeout_id = g_timeout_add (100, (GSourceFunc) pulse_entry_cb, view);

  g_mount_operation_set_password_save (operation, G_PASSWORD_SAVE_FOR_SESSION);

  g_file_mount_enclosing_volume (location,
                                 0,
                                 operation,
                                 priv->cancellable,
                                 server_mount_ready_cb,
                                 view);

  /* unref operation here - g_file_mount_enclosing_volume() does ref for itself */
  g_object_unref (operation);
}

static void
mount_volume (GtkPlacesView *view,
              GVolume       *volume)
{
  GtkPlacesViewPrivate *priv;
  GMountOperation *operation;
  GtkWidget *toplevel;

  priv = gtk_places_view_get_instance_private (view);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

  set_busy_cursor (view, TRUE);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  g_mount_operation_set_password_save (operation, G_PASSWORD_SAVE_FOR_SESSION);

  g_volume_mount (volume,
                  0,
                  operation,
                  priv->cancellable,
                  volume_mount_ready_cb,
                  view);

  /* unref operation here - g_file_mount_enclosing_volume() does ref for itself */
  g_object_unref (operation);
}

/* Callback used when the file list's popup menu is detached */
static void
popup_menu_detach_cb (GtkWidget *attach_widget,
                      GtkMenu   *menu)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (attach_widget));
  priv->popup_menu = NULL;
}

static void
get_view_and_file (GtkPlacesViewRow  *row,
                   GtkWidget        **view,
                   GFile            **file)
{
  if (view)
    *view = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW);

  if (file)
    {
      GVolume *volume;
      GMount *mount;

      volume = gtk_places_view_row_get_volume (row);
      mount = gtk_places_view_row_get_mount (row);

      if (mount)
        *file = g_mount_get_default_location (mount);
      else if (volume)
        *file = g_volume_get_activation_root (volume);
      else
        *file = NULL;
    }
}

static void
open_cb (GtkMenuItem      *item,
         GtkPlacesViewRow *row)
{
  GtkWidget *view;
  GFile *file;

  get_view_and_file (row, &view, &file);

  if (file)
    emit_open_location (GTK_PLACES_VIEW (view), file, GTK_PLACES_OPEN_NORMAL);

  g_clear_object (&file);
}

static void
open_in_new_tab_cb (GtkMenuItem      *item,
                    GtkPlacesViewRow *row)
{
  GtkWidget *view;
  GFile *file;

  get_view_and_file (row, &view, &file);

  if (file)
    emit_open_location (GTK_PLACES_VIEW (view), file, GTK_PLACES_OPEN_NEW_TAB);

  g_clear_object (&file);
}

static void
open_in_new_window_cb (GtkMenuItem      *item,
                       GtkPlacesViewRow *row)
{
  GtkWidget *view;
  GFile *file;

  get_view_and_file (row, &view, &file);

  if (file)
    emit_open_location (GTK_PLACES_VIEW (view), file, GTK_PLACES_OPEN_NEW_WINDOW);

  g_clear_object (&file);
}

static void
mount_cb (GtkMenuItem      *item,
          GtkPlacesViewRow *row)
{
  GtkPlacesViewPrivate *priv;
  GtkWidget *view;
  GVolume *volume;

  view = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW);
  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (view));
  volume = gtk_places_view_row_get_volume (row);

  /*
   * When the mount item is activated, it's expected that
   * the volume only gets mounted, without opening it after
   * the operation is complete.
   */
  priv->should_open_location = FALSE;

  gtk_places_view_row_set_busy (row, TRUE);
  mount_volume (GTK_PLACES_VIEW (view), volume);
}

static void
unmount_cb (GtkMenuItem      *item,
            GtkPlacesViewRow *row)
{
  GtkWidget *view;
  GMount *mount;

  view = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW);
  mount = gtk_places_view_row_get_mount (row);

  gtk_places_view_row_set_busy (row, TRUE);

  unmount_mount (GTK_PLACES_VIEW (view), mount);
}

/* Constructs the popup menu if needed */
static void
build_popup_menu (GtkPlacesView    *view,
                  GtkPlacesViewRow *row)
{
  GtkPlacesViewPrivate *priv;
  GtkWidget *item;
  GMount *mount;
  GFile *file;
  gboolean is_network;

  priv = gtk_places_view_get_instance_private (view);
  mount = gtk_places_view_row_get_mount (row);
  file = gtk_places_view_row_get_file (row);
  is_network = gtk_places_view_row_get_is_network (row);

  priv->popup_menu = gtk_menu_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->popup_menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);

  gtk_menu_attach_to_widget (GTK_MENU (priv->popup_menu),
                             GTK_WIDGET (view),
                             popup_menu_detach_cb);

  /* Open item is always present */
  item = gtk_menu_item_new_with_mnemonic (_("_Open"));
  g_signal_connect (item,
                    "activate",
                    G_CALLBACK (open_cb),
                    row);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

  if (priv->open_flags & GTK_PLACES_OPEN_NEW_TAB)
    {
      item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (open_in_new_tab_cb),
                        row);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
    }

  if (priv->open_flags & GTK_PLACES_OPEN_NEW_WINDOW)
    {
      item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (open_in_new_window_cb),
                        row);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
    }

  /*
   * The only item that contains a file up to now is the Computer
   * item, which cannot be mounted or unmounted.
   */
  if (file)
    return;

  /* Separator */
  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_insert (GTK_MENU_SHELL (priv->popup_menu), item, -1);

  /* Mount/Unmount items */
  if (mount)
    {
      item = gtk_menu_item_new_with_mnemonic (is_network ? P_("_Disconnect") : P_("_Unmount"));
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (unmount_cb),
                        row);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
    }
  else
    {
      item = gtk_menu_item_new_with_mnemonic (is_network ? P_("_Connect") : P_("_Mount"));
      g_signal_connect (item,
                        "activate",
                        G_CALLBACK (mount_cb),
                        row);
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
    }
}

static void
popup_menu (GtkPlacesViewRow *row,
            GdkEventButton   *event)
{
  GtkPlacesViewPrivate *priv;
  GtkWidget *view;
  gint button;

  view = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW);
  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (view));

  g_clear_pointer (&priv->popup_menu, gtk_widget_destroy);

  build_popup_menu (GTK_PLACES_VIEW (view), row);

  /* The event button needs to be 0 if we're popping up this menu from
   * a button release, else a 2nd click outside the menu with any button
   * other than the one that invoked the menu will be ignored (instead
   * of dismissing the menu). This is a subtle fragility of the GTK menu code.
   */
  if (event)
    {
      if (event->type == GDK_BUTTON_PRESS)
        button = 0;
      else
        button = event->button;
    }
  else
    {
      button = 0;
    }

  gtk_menu_popup (GTK_MENU (priv->popup_menu),
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  button,
                  event ? event->time : gtk_get_current_event_time ());
}

static gboolean
on_row_popup_menu (GtkPlacesViewRow *row)
{
  popup_menu (row, NULL);
  return TRUE;
}

static gboolean
on_button_press_event (GtkPlacesViewRow *row,
                       GdkEventButton   *event)
{
  if (row &&
      gdk_event_triggers_context_menu ((GdkEvent*) event) &&
      event->type == GDK_BUTTON_PRESS)
    {
      popup_menu (row, event);

      return TRUE;
    }

  return FALSE;
}

static gboolean
on_key_press_event (GtkWidget     *widget,
                    GdkEventKey   *event,
                    GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (view);

  if (event)
    {
      guint modifiers;

      modifiers = gtk_accelerator_get_default_mod_mask ();

      if (event->keyval == GDK_KEY_Return ||
          event->keyval == GDK_KEY_KP_Enter ||
          event->keyval == GDK_KEY_ISO_Enter ||
          event->keyval == GDK_KEY_space)
        {
          GtkWidget *focus_widget;
          GtkWindow *toplevel;

          priv->current_open_flags = GTK_PLACES_OPEN_NORMAL;
          toplevel = get_toplevel (GTK_WIDGET (view));

          if (!toplevel)
            return FALSE;

          focus_widget = gtk_window_get_focus (toplevel);

          if (!GTK_IS_PLACES_VIEW_ROW (focus_widget))
            return FALSE;

          if ((event->state & modifiers) == GDK_SHIFT_MASK)
            priv->current_open_flags = GTK_PLACES_OPEN_NEW_TAB;
          else if ((event->state & modifiers) == GDK_CONTROL_MASK)
            priv->current_open_flags = GTK_PLACES_OPEN_NEW_WINDOW;

          activate_row (view, GTK_PLACES_VIEW_ROW (focus_widget), priv->current_open_flags);

          return TRUE;
        }
    }

  return FALSE;
}

static void
on_eject_button_clicked (GtkWidget        *widget,
                         GtkPlacesViewRow *row)
{
  if (row)
    {
      GtkWidget *view = gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_PLACES_VIEW);

      unmount_mount (GTK_PLACES_VIEW (view), gtk_places_view_row_get_mount (row));
    }
}

static void
on_connect_button_clicked (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;
  const gchar *uri;
  GFile *file;

  priv = gtk_places_view_get_instance_private (view);
  file = NULL;

  /*
   * Since the 'Connect' button is updated whenever the typed
   * address changes, it is sufficient to check if it's sensitive
   * or not, in order to determine if the given address is valid.
   */
  if (!gtk_widget_get_sensitive (priv->connect_button))
    return;

  uri = gtk_entry_get_text (GTK_ENTRY (priv->address_entry));

  if (uri != NULL && uri[0] != '\0')
    file = g_file_new_for_commandline_arg (uri);

  if (file)
    {
      priv->should_open_location = TRUE;

      mount_server (view, file);
    }
  else
    {
      emit_show_error_message (view, _("Unable to get remote server location"), NULL);
    }
}

static void
on_address_entry_text_changed (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;
  const gchar* const *supported_protocols;
  gchar *address, *scheme;
  gboolean supported;

  priv = gtk_places_view_get_instance_private (view);
  supported = FALSE;
  supported_protocols = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

  if (!supported_protocols)
    return;

  address = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->address_entry)));
  scheme = g_uri_parse_scheme (address);

  if (!scheme)
    goto out;

  supported = g_strv_contains (supported_protocols, scheme) &&
              !g_strv_contains (unsupported_protocols, scheme);

out:
  gtk_widget_set_sensitive (priv->connect_button, supported);
  g_free (address);
}

static void
on_recent_servers_listbox_row_activated (GtkPlacesView    *view,
                                         GtkPlacesViewRow *row,
                                         GtkWidget        *listbox)
{
  GtkPlacesViewPrivate *priv;
  gchar *uri;

  priv = gtk_places_view_get_instance_private (view);
  uri = g_object_get_data (G_OBJECT (row), "uri");

  gtk_entry_set_text (GTK_ENTRY (priv->address_entry), uri);

  gtk_widget_hide (priv->recent_servers_popover);
}

static void
on_listbox_row_activated (GtkPlacesView    *view,
                          GtkPlacesViewRow *row,
                          GtkWidget        *listbox)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (view);

  activate_row (view, row, priv->current_open_flags);
}

static gboolean
listbox_filter_func (GtkListBoxRow *row,
                     gpointer       user_data)
{
  GtkPlacesViewPrivate *priv;
  gboolean is_network;
  gboolean retval;
  gchar *name;
  gchar *path;

  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (user_data));
  retval = FALSE;

  is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-network"));

  if (is_network && priv->local_only)
    return FALSE;

  if (!priv->search_query || priv->search_query[0] == '\0')
    return TRUE;

  g_object_get (row,
                "name", &name,
                "path", &path,
                NULL);

  if (name)
    retval |= strstr (name, priv->search_query) != NULL;

  if (path)
    retval |= strstr (path, priv->search_query) != NULL;

  g_free (name);
  g_free (path);

  return retval;
}

static void
listbox_header_func (GtkListBoxRow *row,
                     GtkListBoxRow *before,
                     gpointer       user_data)
{
  gboolean row_is_network;
  gchar *text;

  text = NULL;
  row_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-network"));

  if (!before)
    {
      text = g_strdup_printf ("<b>%s</b>", row_is_network ? _("Networks") : _("On This Computer"));
    }
  else
    {
      gboolean before_is_network;

      before_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (before), "is-network"));

      if (before_is_network != row_is_network)
        text = g_strdup_printf ("<b>%s</b>", row_is_network ? _("Networks") : _("On This Computer"));
    }

  if (text)
    {
      GtkWidget *header;
      GtkWidget *label;
      GtkWidget *separator;

      header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

      label = g_object_new (GTK_TYPE_LABEL,
                            "hexpand", TRUE,
                            "use_markup", TRUE,
                            "label", text,
                            "xalign", 0.0f,
                            NULL);

      g_object_set (label,
                    "margin-top", 6,
                    "margin-start", 12,
                    "margin-end", 12,
                    NULL);

      gtk_container_add (GTK_CONTAINER (header), label);
      gtk_container_add (GTK_CONTAINER (header), separator);
      gtk_widget_show_all (header);

      gtk_list_box_row_set_header (row, header);

      g_free (text);
    }
  else
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

static gint
listbox_sort_func (GtkListBoxRow *row1,
                   GtkListBoxRow *row2,
                   gpointer       user_data)
{
  gboolean row1_is_network;
  gboolean row2_is_network;
  gchar *location1;
  gchar *location2;
  gint retval;

  row1_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row1), "is-network"));
  row2_is_network = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row2), "is-network"));

  retval = row1_is_network - row2_is_network;

  if (retval != 0)
    return retval;

  g_object_get (row1, "path", &location1, NULL);
  g_object_get (row2, "path", &location2, NULL);

  retval = g_strcmp0 (location1, location2);

  g_free (location1);
  g_free (location2);

  return retval;
}

static void
gtk_places_view_constructed (GObject *object)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (object));

  G_OBJECT_CLASS (gtk_places_view_parent_class)->constructed (object);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->listbox),
                              listbox_sort_func,
                              object,
                              NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->listbox),
                                listbox_filter_func,
                                object,
                                NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->listbox),
                                listbox_header_func,
                                object,
                                NULL);

  /* load drives */
  update_places (GTK_PLACES_VIEW (object));

  g_signal_connect_swapped (priv->volume_monitor,
                            "mount-added",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (priv->volume_monitor,
                            "mount-changed",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (priv->volume_monitor,
                            "mount-removed",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (priv->volume_monitor,
                            "volume-added",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (priv->volume_monitor,
                            "volume-changed",
                            G_CALLBACK (update_places),
                            object);
  g_signal_connect_swapped (priv->volume_monitor,
                            "volume-removed",
                            G_CALLBACK (update_places),
                            object);
}

static void
gtk_places_view_map (GtkWidget *widget)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (GTK_PLACES_VIEW (widget));

  gtk_entry_set_text (GTK_ENTRY (priv->address_entry), "");

  GTK_WIDGET_CLASS (gtk_places_view_parent_class)->map (widget);
}

static void
gtk_places_view_class_init (GtkPlacesViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_places_view_finalize;
  object_class->constructed = gtk_places_view_constructed;
  object_class->get_property = gtk_places_view_get_property;
  object_class->set_property = gtk_places_view_set_property;

  widget_class->map = gtk_places_view_map;

  /**
   * GtkPlacesView::open-location:
   * @view: the object which received the signal.
   * @location: (type Gio.File): #GFile to which the caller should switch.
   * @open_flags: a single value from #GtkPlacesOpenFlags specifying how the @location
   * should be opened.
   *
   * The places view emits this signal when the user selects a location
   * in it. The calling application should display the contents of that
   * location; for example, a file manager should show a list of files in
   * the specified location.
   *
   * Since: 3.18
   */
  places_view_signals [OPEN_LOCATION] =
          g_signal_new (I_("open-location"),
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesViewClass, open_location),
                        NULL, NULL, NULL,
                        G_TYPE_NONE, 2,
                        G_TYPE_OBJECT,
                        GTK_TYPE_PLACES_OPEN_FLAGS);

  /**
   * GtkPlacesView::show-error-message:
   * @view: the object which received the signal.
   * @primary: primary message with a summary of the error to show.
   * @secondary: secondary message with details of the error to show.
   *
   * The places view emits this signal when it needs the calling
   * application to present an error message.  Most of these messages
   * refer to mounting or unmounting media, for example, when a drive
   * cannot be started for some reason.
   *
   * Since: 3.18
   */
  places_view_signals [SHOW_ERROR_MESSAGE] =
          g_signal_new (I_("show-error-message"),
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesViewClass, show_error_message),
                        NULL, NULL,
                        _gtk_marshal_VOID__STRING_STRING,
                        G_TYPE_NONE, 2,
                        G_TYPE_STRING,
                        G_TYPE_STRING);

  properties[PROP_LOCAL_ONLY] =
          g_param_spec_boolean ("local-only",
                                P_("Local Only"),
                                P_("Whether the sidebar only includes local files"),
                                FALSE,
                                G_PARAM_READWRITE);

  properties[PROP_OPEN_FLAGS] =
          g_param_spec_flags ("open-flags",
                              P_("Open Flags"),
                              P_("Modes in which the calling application can open locations selected in the sidebar"),
                              GTK_TYPE_PLACES_OPEN_FLAGS,
                              GTK_PLACES_OPEN_NORMAL,
                              G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkplacesview.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, actionbar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, address_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, address_entry_completion);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, completion_store);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, connect_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, recent_servers_listbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, recent_servers_popover);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, recent_servers_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPlacesView, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_address_entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_connect_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_recent_servers_listbox_row_activated);
}

static void
gtk_places_view_init (GtkPlacesView *self)
{
  GtkPlacesViewPrivate *priv;

  priv = gtk_places_view_get_instance_private (self);

  priv->volume_monitor = g_volume_monitor_get ();
  priv->open_flags = GTK_PLACES_OPEN_NORMAL;

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtk_places_view_new:
 *
 * Creates a new #GtkPlacesView widget.
 *
 * The application should connect to at least the
 * #GtkPlacesView::open-location signal to be notified
 * when the user makes a selection in the view.
 *
 * Returns: a newly created #GtkPlacesView
 *
 * Since: 3.18
 */
GtkWidget *
gtk_places_view_new (void)
{
  return g_object_new (GTK_TYPE_PLACES_VIEW, NULL);
}

/**
 * gtk_places_view_set_open_flags:
 * @view: a #GtkPlacesView
 * @flags: Bitmask of modes in which the calling application can open locations
 *
 * Sets the way in which the calling application can open new locations from
 * the places view.  For example, some applications only open locations
 * “directly” into their main view, while others may support opening locations
 * in a new notebook tab or a new window.
 *
 * This function is used to tell the places @view about the ways in which the
 * application can open new locations, so that the view can display (or not)
 * the “Open in new tab” and “Open in new window” menu items as appropriate.
 *
 * When the #GtkPlacesView::open-location signal is emitted, its flags
 * argument will be set to one of the @flags that was passed in
 * gtk_places_view_set_open_flags().
 *
 * Passing 0 for @flags will cause #GTK_PLACES_OPEN_NORMAL to always be sent
 * to callbacks for the “open-location” signal.
 *
 * Since: 3.18
 */
void
gtk_places_view_set_open_flags (GtkPlacesView      *view,
                                GtkPlacesOpenFlags  flags)
{
  GtkPlacesViewPrivate *priv;

  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  priv = gtk_places_view_get_instance_private (view);

  if (priv->open_flags != flags)
    {
      priv->open_flags = flags;
      g_object_notify_by_pspec (G_OBJECT (view), properties[PROP_OPEN_FLAGS]);
    }
}

/**
 * gtk_places_view_get_open_flags:
 * @view: a #GtkPlacesSidebar
 *
 * Gets the open flags.
 *
 * Returns: the #GtkPlacesOpenFlags of @view
 *
 * Since: 3.18
 */
GtkPlacesOpenFlags
gtk_places_view_get_open_flags (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;

  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), 0);

  priv = gtk_places_view_get_instance_private (view);

  return priv->open_flags;
}

/**
 * gtk_places_view_get_search_query:
 * @view: a #GtkPlacesView
 *
 * Retrieves the current search query from @view.
 *
 * Returns: (transfer none): the current search query.
 */
const gchar*
gtk_places_view_get_search_query (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;

  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), NULL);

  priv = gtk_places_view_get_instance_private (view);

  return priv->search_query;
}

/**
 * gtk_places_view_set_search_query:
 * @view: a #GtkPlacesView
 * @query_text: the query, or NULL.
 *
 * Sets the search query of @view. The search is immediately performed
 * once the query is set.
 *
 * Returns:
 */
void
gtk_places_view_set_search_query (GtkPlacesView *view,
                                  const gchar   *query_text)
{
  GtkPlacesViewPrivate *priv;

  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  priv = gtk_places_view_get_instance_private (view);

  if (g_strcmp0 (priv->search_query, query_text) != 0)
    {
      g_clear_pointer (&priv->search_query, g_free);
      priv->search_query = g_strdup (query_text);

      gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->listbox));
      gtk_list_box_invalidate_headers (GTK_LIST_BOX (priv->listbox));

      update_view_mode (view);
    }
}

/**
 * gtk_places_view_get_local_only:
 * @view: a #GtkPlacesView
 *
 * Returns %TRUE if only local volumes are shown, i.e. no networks
 * are displayed.
 *
 * Returns: %TRUE if only local volumes are shown, %FALSE otherwise.
 *
 * Since: 3.18
 */
gboolean
gtk_places_view_get_local_only (GtkPlacesView *view)
{
  GtkPlacesViewPrivate *priv;

  g_return_val_if_fail (GTK_IS_PLACES_VIEW (view), FALSE);

  priv = gtk_places_view_get_instance_private (view);

  return priv->local_only;
}

/**
 * gtk_places_view_set_local_only:
 * @view: a #GtkPlacesView
 * @local_only: %TRUE to hide remote locations, %FALSE to show.
 *
 * Sets the #GtkPlacesView::local-only property to @local_only.
 *
 * Returns:
 *
 * Since: 3.18
 */
void
gtk_places_view_set_local_only (GtkPlacesView *view,
                                gboolean       local_only)
{
  GtkPlacesViewPrivate *priv;

  g_return_if_fail (GTK_IS_PLACES_VIEW (view));

  priv = gtk_places_view_get_instance_private (view);

  if (priv->local_only != local_only)
    {
      priv->local_only = local_only;

      gtk_widget_set_visible (priv->actionbar, !local_only);
      gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->listbox));

      update_view_mode (view);

      g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_LOCAL_ONLY]);
    }
}
