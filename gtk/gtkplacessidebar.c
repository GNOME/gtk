/* GtkPlacesSidebar - sidebar widget for places in the filesystem
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * This code is originally from Nautilus.
 *
 * Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *           Cosimo Cecchi <cosimoc@gnome.org>
 *           Federico Mena Quintero <federico@gnome.org>
 *           Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include <gio/gio.h>
#ifdef HAVE_CLOUDPROVIDERS
#include <cloudproviders/cloudproviderscollector.h>
#include <cloudproviders/cloudprovidersaccount.h>
#include <cloudproviders/cloudprovidersprovider.h>
#endif

#include "gtkplacessidebarprivate.h"
#include "gtksidebarrowprivate.h"
#include "gdk/gdkkeysyms.h"
#include "gtkbookmarksmanager.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkfilesystem.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkmountoperation.h"
#include "gtkplacessidebar.h"
#include "gtkscrolledwindow.h"
#include "gtkseparatormenuitem.h"
#include "gtksettings.h"
#include "gtktrashmonitor.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"
#include "gtkpopover.h"
#include "gtkgrid.h"
#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtklistbox.h"
#include "gtkselection.h"
#include "gtkdragdest.h"
#include "gtkdnd.h"
#include "gtkseparator.h"
#include "gtkentry.h"
#include "gtkgesturelongpress.h"
#include "gtkbox.h"
#include "gtkmodelbutton.h"

/**
 * SECTION:gtkplacessidebar
 * @Short_description: Sidebar that displays frequently-used places in the file system
 * @Title: GtkPlacesSidebar
 * @See_also: #GtkFileChooser
 *
 * #GtkPlacesSidebar is a widget that displays a list of frequently-used places in the
 * file system:  the user’s home directory, the user’s bookmarks, and volumes and drives.
 * This widget is used as a sidebar in #GtkFileChooser and may be used by file managers
 * and similar programs.
 *
 * The places sidebar displays drives and volumes, and will automatically mount
 * or unmount them when the user selects them.
 *
 * Applications can hook to various signals in the places sidebar to customize
 * its behavior.  For example, they can add extra commands to the context menu
 * of the sidebar.
 *
 * While bookmarks are completely in control of the user, the places sidebar also
 * allows individual applications to provide extra shortcut folders that are unique
 * to each application.  For example, a Paint program may want to add a shortcut
 * for a Clipart folder.  You can do this with gtk_places_sidebar_add_shortcut().
 *
 * To make use of the places sidebar, an application at least needs to connect
 * to the #GtkPlacesSidebar::open-location signal.  This is emitted when the
 * user selects in the sidebar a location to open.  The application should also
 * call gtk_places_sidebar_set_location() when it changes the currently-viewed
 * location.
 *
 * # CSS nodes
 *
 * GtkPlacesSidebar uses a single CSS node with name placessidebar and style
 * class .sidebar.
 *
 * Among the children of the places sidebar, the following style classes can
 * be used:
 * - .sidebar-new-bookmark-row for the 'Add new bookmark' row
 * - .sidebar-placeholder-row for a row that is a placeholder
 * - .has-open-popup when a popup is open for a row
 */

/* These are used when a destination-side DND operation is taking place.
 * Normally, when a common drag action is taking place, the state will be
 * DROP_STATE_NEW_BOOKMARK_ARMED, however, if the client of GtkPlacesSidebar
 * wants to show hints about the valid targets, we sill set it as
 * DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT, so the sidebar will show drop hints
 * until the client says otherwise
 */
typedef enum {
  DROP_STATE_NORMAL,
  DROP_STATE_NEW_BOOKMARK_ARMED,
  DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT,
} DropState;

struct _GtkPlacesSidebar {
  GtkScrolledWindow parent;

  GtkWidget *list_box;
  GtkWidget *new_bookmark_row;

  GtkBookmarksManager     *bookmarks_manager;

#ifdef HAVE_CLOUDPROVIDERS
  CloudProvidersCollector *cloud_manager;
  GList *unready_accounts;
#endif

  GVolumeMonitor    *volume_monitor;
  GtkTrashMonitor   *trash_monitor;
  GtkSettings       *gtk_settings;
  GFile             *current_location;

  GtkWidget *rename_popover;
  GtkWidget *rename_entry;
  GtkWidget *rename_button;
  GtkWidget *rename_error;
  gchar *rename_uri;

  gulong trash_monitor_changed_id;
  GtkWidget *trash_row;

  /* DND */
  GList     *drag_list; /* list of GFile */
  gint       drag_data_info;
  gboolean   dragging_over;
  GtkTargetList *source_targets;
  GtkWidget *drag_row;
  gint drag_row_height;
  gint drag_row_x;
  gint drag_row_y;
  gint drag_root_x;
  gint drag_root_y;
  GtkWidget *row_placeholder;
  DropState drop_state;
  GtkGesture *long_press_gesture;

  /* volume mounting - delayed open process */
  GtkPlacesOpenFlags go_to_after_mount_open_flags;
  GCancellable *cancellable;

  GtkWidget *popover;
  GtkSidebarRow *context_row;
  GSList *shortcuts;

  GDBusProxy *hostnamed_proxy;
  GCancellable *hostnamed_cancellable;
  gchar *hostname;

  GtkPlacesOpenFlags open_flags;

  GActionGroup *action_group;

  guint mounting               : 1;
  guint  drag_data_received    : 1;
  guint drop_occurred          : 1;
  guint show_recent_set        : 1;
  guint show_recent            : 1;
  guint show_desktop_set       : 1;
  guint show_desktop           : 1;
  guint show_connect_to_server : 1;
  guint show_enter_location    : 1;
  guint show_other_locations   : 1;
  guint show_trash             : 1;
  guint show_starred_location  : 1;
  guint local_only             : 1;
  guint populate_all           : 1;
};

struct _GtkPlacesSidebarClass {
  GtkScrolledWindowClass parent;

  void    (* open_location)          (GtkPlacesSidebar   *sidebar,
                                      GFile              *location,
                                      GtkPlacesOpenFlags  open_flags);
  void    (* populate_popup)         (GtkPlacesSidebar   *sidebar,
                                      GtkMenu            *menu,
                                      GFile              *selected_item,
                                      GVolume            *selected_volume);
  void    (* show_error_message)     (GtkPlacesSidebar   *sidebar,
                                      const gchar        *primary,
                                      const gchar        *secondary);
  void    (* show_connect_to_server) (GtkPlacesSidebar   *sidebar);
  GdkDragAction (* drag_action_requested)  (GtkPlacesSidebar   *sidebar,
                                      GdkDragContext     *context,
                                      GFile              *dest_file,
                                      GList              *source_file_list);
  GdkDragAction (* drag_action_ask)  (GtkPlacesSidebar   *sidebar,
                                      GdkDragAction       actions);
  void    (* drag_perform_drop)      (GtkPlacesSidebar   *sidebar,
                                      GFile              *dest_file,
                                      GList              *source_file_list,
                                      GdkDragAction       action);
  void    (* show_enter_location)    (GtkPlacesSidebar   *sidebar);

  void    (* show_other_locations)   (GtkPlacesSidebar   *sidebar);

  void    (* show_other_locations_with_flags)   (GtkPlacesSidebar   *sidebar,
                                                 GtkPlacesOpenFlags  open_flags);

  void    (* show_starred_location)    (GtkPlacesSidebar   *sidebar);

  void    (* mount)                  (GtkPlacesSidebar   *sidebar,
                                      GMountOperation    *mount_operation);
  void    (* unmount)                (GtkPlacesSidebar   *sidebar,
                                      GMountOperation    *unmount_operation);
};

enum {
  OPEN_LOCATION,
  POPULATE_POPUP,
  SHOW_ERROR_MESSAGE,
  SHOW_CONNECT_TO_SERVER,
  SHOW_ENTER_LOCATION,
  DRAG_ACTION_REQUESTED,
  DRAG_ACTION_ASK,
  DRAG_PERFORM_DROP,
  SHOW_OTHER_LOCATIONS,
  SHOW_OTHER_LOCATIONS_WITH_FLAGS,
  SHOW_STARRED_LOCATION,
  MOUNT,
  UNMOUNT,
  LAST_SIGNAL
};

enum {
  PROP_LOCATION = 1,
  PROP_OPEN_FLAGS,
  PROP_SHOW_RECENT,
  PROP_SHOW_DESKTOP,
  PROP_SHOW_CONNECT_TO_SERVER,
  PROP_SHOW_ENTER_LOCATION,
  PROP_SHOW_TRASH,
  PROP_SHOW_STARRED_LOCATION,
  PROP_LOCAL_ONLY,
  PROP_SHOW_OTHER_LOCATIONS,
  PROP_POPULATE_ALL,
  NUM_PROPERTIES
};

/* Names for themed icons */
#define ICON_NAME_HOME     "user-home-symbolic"
#define ICON_NAME_DESKTOP  "user-desktop-symbolic"
#define ICON_NAME_FILESYSTEM     "drive-harddisk-symbolic"
#define ICON_NAME_EJECT    "media-eject-symbolic"
#define ICON_NAME_NETWORK  "network-workgroup-symbolic"
#define ICON_NAME_NETWORK_SERVER "network-server-symbolic"
#define ICON_NAME_FOLDER_NETWORK "folder-remote-symbolic"
#define ICON_NAME_OTHER_LOCATIONS "list-add-symbolic"

#define ICON_NAME_FOLDER                "folder-symbolic"
#define ICON_NAME_FOLDER_DESKTOP  "user-desktop-symbolic"
#define ICON_NAME_FOLDER_DOCUMENTS      "folder-documents-symbolic"
#define ICON_NAME_FOLDER_DOWNLOAD       "folder-download-symbolic"
#define ICON_NAME_FOLDER_MUSIC    "folder-music-symbolic"
#define ICON_NAME_FOLDER_PICTURES       "folder-pictures-symbolic"
#define ICON_NAME_FOLDER_PUBLIC_SHARE   "folder-publicshare-symbolic"
#define ICON_NAME_FOLDER_TEMPLATES      "folder-templates-symbolic"
#define ICON_NAME_FOLDER_VIDEOS   "folder-videos-symbolic"
#define ICON_NAME_FOLDER_SAVED_SEARCH   "folder-saved-search-symbolic"

static guint places_sidebar_signals [LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static gboolean eject_or_unmount_bookmark  (GtkSidebarRow *row);
static gboolean eject_or_unmount_selection (GtkPlacesSidebar *sidebar);
static void  check_unmount_and_eject       (GMount   *mount,
                                            GVolume  *volume,
                                            GDrive   *drive,
                                            gboolean *show_unmount,
                                            gboolean *show_eject);
static gboolean on_button_press_event (GtkWidget      *widget,
                                       GdkEventButton *event,
                                       GtkSidebarRow  *sidebar);
static gboolean on_button_release_event (GtkWidget      *widget,
                                         GdkEventButton *event,
                                         GtkSidebarRow  *sidebar);
static void popup_menu_cb    (GtkSidebarRow   *row);
static void long_press_cb    (GtkGesture      *gesture,
                              gdouble          x,
                              gdouble          y,
                              GtkPlacesSidebar *sidebar);
static void stop_drop_feedback (GtkPlacesSidebar *sidebar);
static GMountOperation * get_mount_operation (GtkPlacesSidebar *sidebar);
static GMountOperation * get_unmount_operation (GtkPlacesSidebar *sidebar);


/* Identifiers for target types */
enum {
  DND_UNKNOWN,
  DND_GTK_SIDEBAR_ROW,
  DND_TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry dnd_source_targets[] = {
  { "DND_GTK_SIDEBAR_ROW", GTK_TARGET_SAME_WIDGET, DND_GTK_SIDEBAR_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry dnd_drop_targets [] = {
  { "DND_GTK_SIDEBAR_ROW", GTK_TARGET_SAME_WIDGET, DND_GTK_SIDEBAR_ROW }
};

G_DEFINE_TYPE (GtkPlacesSidebar, gtk_places_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static void
emit_open_location (GtkPlacesSidebar   *sidebar,
                    GFile              *location,
                    GtkPlacesOpenFlags  open_flags)
{
  if ((open_flags & sidebar->open_flags) == 0)
    open_flags = GTK_PLACES_OPEN_NORMAL;

  g_signal_emit (sidebar, places_sidebar_signals[OPEN_LOCATION], 0,
                 location, open_flags);
}

static void
emit_show_error_message (GtkPlacesSidebar *sidebar,
                         const gchar      *primary,
                         const gchar      *secondary)
{
  g_signal_emit (sidebar, places_sidebar_signals[SHOW_ERROR_MESSAGE], 0,
                 primary, secondary);
}

static void
emit_show_connect_to_server (GtkPlacesSidebar *sidebar)
{
  g_signal_emit (sidebar, places_sidebar_signals[SHOW_CONNECT_TO_SERVER], 0);
}

static void
emit_show_enter_location (GtkPlacesSidebar *sidebar)
{
  g_signal_emit (sidebar, places_sidebar_signals[SHOW_ENTER_LOCATION], 0);
}

static void
emit_show_other_locations (GtkPlacesSidebar *sidebar)
{
  g_signal_emit (sidebar, places_sidebar_signals[SHOW_OTHER_LOCATIONS], 0);
}

static void
emit_show_other_locations_with_flags (GtkPlacesSidebar   *sidebar,
                                      GtkPlacesOpenFlags  open_flags)
{
  g_signal_emit (sidebar, places_sidebar_signals[SHOW_OTHER_LOCATIONS_WITH_FLAGS],
                 0, open_flags);
}

static void
emit_show_starred_location (GtkPlacesSidebar  *sidebar,
                            GtkPlacesOpenFlags open_flags)
{
  g_signal_emit (sidebar, places_sidebar_signals[SHOW_STARRED_LOCATION], 0,
                 open_flags);
}


static void
emit_mount_operation (GtkPlacesSidebar *sidebar,
                      GMountOperation  *mount_op)
{
  g_signal_emit (sidebar, places_sidebar_signals[MOUNT], 0, mount_op);
}

static void
emit_unmount_operation (GtkPlacesSidebar *sidebar,
                        GMountOperation  *mount_op)
{
  g_signal_emit (sidebar, places_sidebar_signals[UNMOUNT], 0, mount_op);
}

static GdkDragAction
emit_drag_action_requested (GtkPlacesSidebar *sidebar,
                            GdkDragContext   *context,
                            GFile            *dest_file,
                            GList            *source_file_list)
{
  GdkDragAction ret_action = 0;

  g_signal_emit (sidebar, places_sidebar_signals[DRAG_ACTION_REQUESTED], 0,
                 context, dest_file, source_file_list, &ret_action);

  return ret_action;
}

static GdkDragAction
emit_drag_action_ask (GtkPlacesSidebar *sidebar,
                      GdkDragAction     actions)
{
  GdkDragAction ret_action = 0;

  g_signal_emit (sidebar, places_sidebar_signals[DRAG_ACTION_ASK], 0,
                 actions, &ret_action);

  return ret_action;
}

static void
emit_drag_perform_drop (GtkPlacesSidebar *sidebar,
                        GFile            *dest_file,
                        GList            *source_file_list,
                        GdkDragAction     action)
{
  g_signal_emit (sidebar, places_sidebar_signals[DRAG_PERFORM_DROP], 0,
                 dest_file, source_file_list, action);
}
static void
list_box_header_func (GtkListBoxRow *row,
                      GtkListBoxRow *before,
                      gpointer       user_data)
{
  GtkPlacesSidebarSectionType row_section_type;
  GtkPlacesSidebarSectionType before_section_type;
  GtkWidget *separator;

  gtk_list_box_row_set_header (row, NULL);

  g_object_get (row, "section-type", &row_section_type, NULL);
  if (before)
    {
      g_object_get (before, "section-type", &before_section_type, NULL);
    }
  else
    {
      before_section_type = SECTION_INVALID;
      gtk_widget_set_margin_top (GTK_WIDGET (row), 4);
    }

  if (before && before_section_type != row_section_type)
    {
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_set_margin_top (separator, 4);
      gtk_widget_set_margin_bottom (separator, 4);
      gtk_list_box_row_set_header (row, separator);
    }
}

static GtkWidget*
add_place (GtkPlacesSidebar            *sidebar,
           GtkPlacesSidebarPlaceType    place_type,
           GtkPlacesSidebarSectionType  section_type,
           const gchar                 *name,
           GIcon                       *start_icon,
           GIcon                       *end_icon,
           const gchar                 *uri,
           GDrive                      *drive,
           GVolume                     *volume,
           GMount                      *mount,
#ifdef HAVE_CLOUDPROVIDERS
           CloudProvidersAccount       *cloud_provider_account,
#else
           gpointer                    *cloud_provider_account,
#endif
           const gint                   index,
           const gchar                 *tooltip)
{
  gboolean show_eject, show_unmount;
  gboolean show_eject_button;
  GtkWidget *row;
  GtkWidget *eject_button;
  GtkWidget *event_box;

  check_unmount_and_eject (mount, volume, drive,
                           &show_unmount, &show_eject);

  if (show_unmount || show_eject)
    g_assert (place_type != PLACES_BOOKMARK);

  show_eject_button = (show_unmount || show_eject);

  row = g_object_new (GTK_TYPE_SIDEBAR_ROW,
                      "sidebar", sidebar,
                      "start-icon", start_icon,
                      "end-icon", end_icon,
                      "label", name,
                      "tooltip", tooltip,
                      "ejectable", show_eject_button,
                      "order-index", index,
                      "section-type", section_type,
                      "place-type", place_type,
                      "uri", uri,
                      "drive", drive,
                      "volume", volume,
                      "mount", mount,
#ifdef HAVE_CLOUDPROVIDERS
                      "cloud-provider-account", cloud_provider_account,
#endif
                      NULL);

  eject_button = gtk_sidebar_row_get_eject_button (GTK_SIDEBAR_ROW (row));
  event_box = gtk_sidebar_row_get_event_box (GTK_SIDEBAR_ROW (row));

  g_signal_connect_swapped (eject_button, "clicked",
                            G_CALLBACK (eject_or_unmount_bookmark), row);
  g_signal_connect (event_box, "button-press-event",
                    G_CALLBACK (on_button_press_event), row);
  g_signal_connect (event_box, "button-release-event",
                    G_CALLBACK (on_button_release_event), row);

  gtk_container_add (GTK_CONTAINER (sidebar->list_box), GTK_WIDGET (row));
  gtk_widget_show_all (row);

  return row;
}

static GIcon *
special_directory_get_gicon (GUserDirectory directory)
{
#define ICON_CASE(x)                      \
  case G_USER_DIRECTORY_ ## x:                                    \
          return g_themed_icon_new_with_default_fallbacks (ICON_NAME_FOLDER_ ## x);

  switch (directory)
    {

    ICON_CASE (DESKTOP);
    ICON_CASE (DOCUMENTS);
    ICON_CASE (DOWNLOAD);
    ICON_CASE (MUSIC);
    ICON_CASE (PICTURES);
    ICON_CASE (PUBLIC_SHARE);
    ICON_CASE (TEMPLATES);
    ICON_CASE (VIDEOS);

    default:
      return g_themed_icon_new_with_default_fallbacks (ICON_NAME_FOLDER);
    }

#undef ICON_CASE
}

static gboolean
recent_files_setting_is_enabled (GtkPlacesSidebar *sidebar)
{
  GtkSettings *settings;
  gboolean enabled;

  settings = gtk_widget_get_settings (GTK_WIDGET (sidebar));
  g_object_get (settings, "gtk-recent-files-enabled", &enabled, NULL);

  return enabled;
}

static gboolean
recent_scheme_is_supported (void)
{
  const gchar * const *supported;

  supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
  if (supported != NULL)
    return g_strv_contains (supported, "recent");

  return FALSE;
}

static gboolean
should_show_recent (GtkPlacesSidebar *sidebar)
{
  return recent_files_setting_is_enabled (sidebar) &&
         ((sidebar->show_recent_set && sidebar->show_recent) ||
          (!sidebar->show_recent_set && recent_scheme_is_supported ()));
}

static gboolean
path_is_home_dir (const gchar *path)
{
  GFile *home_dir;
  GFile *location;
  const gchar *home_path;
  gboolean res;

  home_path = g_get_home_dir ();
  if (!home_path)
    return FALSE;

  home_dir = g_file_new_for_path (home_path);
  location = g_file_new_for_path (path);
  res = g_file_equal (home_dir, location);

  g_object_unref (home_dir);
  g_object_unref (location);

  return res;
}

static void
open_home (GtkPlacesSidebar *sidebar)
{
  const gchar *home_path;
  GFile       *home_dir;

  home_path = g_get_home_dir ();
  if (!home_path)
    return;

  home_dir = g_file_new_for_path (home_path);
  emit_open_location (sidebar, home_dir, 0);

  g_object_unref (home_dir);
}

static void
add_special_dirs (GtkPlacesSidebar *sidebar)
{
  GList *dirs;
  gint index;

  dirs = NULL;
  for (index = 0; index < G_USER_N_DIRECTORIES; index++)
    {
      const gchar *path;
      GFile *root;
      GIcon *start_icon;
      gchar *name;
      gchar *mount_uri;
      gchar *tooltip;

      if (!_gtk_bookmarks_manager_get_is_xdg_dir_builtin (index))
        continue;

      path = g_get_user_special_dir (index);

      /* XDG resets special dirs to the home directory in case
       * it's not finiding what it expects. We don't want the home
       * to be added multiple times in that weird configuration.
       */
      if (path == NULL ||
          path_is_home_dir (path) ||
          g_list_find_custom (dirs, path, (GCompareFunc) g_strcmp0) != NULL)
        continue;

      root = g_file_new_for_path (path);

      name = _gtk_bookmarks_manager_get_bookmark_label (sidebar->bookmarks_manager, root);
      if (!name)
        name = g_file_get_basename (root);

      start_icon = special_directory_get_gicon (index);
      mount_uri = g_file_get_uri (root);
      tooltip = g_file_get_parse_name (root);

      add_place (sidebar, PLACES_XDG_DIR,
                 SECTION_COMPUTER,
                 name, start_icon, NULL, mount_uri,
                 NULL, NULL, NULL, NULL, 0,
                 tooltip);
      g_free (name);
      g_object_unref (root);
      g_object_unref (start_icon);
      g_free (mount_uri);
      g_free (tooltip);

      dirs = g_list_prepend (dirs, (gchar *)path);
    }

  g_list_free (dirs);
}

static gchar *
get_home_directory_uri (void)
{
  const gchar *home;

  home = g_get_home_dir ();
  if (!home)
    return NULL;

  return g_filename_to_uri (home, NULL, NULL);
}

static gchar *
get_desktop_directory_uri (void)
{
  const gchar *name;

  name = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

  /* "To disable a directory, point it to the homedir."
   * See http://freedesktop.org/wiki/Software/xdg-user-dirs
   */
  if (path_is_home_dir (name))
    return NULL;

  return g_filename_to_uri (name, NULL, NULL);
}

static gboolean
should_show_file (GtkPlacesSidebar *sidebar,
                  GFile            *file)
{
  gchar *path;

  if (!sidebar->local_only)
    return TRUE;

  path = g_file_get_path (file);
  if (path)
    {
      g_free (path);
      return TRUE;
    }

  return FALSE;
}

static gboolean
file_is_shown (GtkPlacesSidebar *sidebar,
               GFile            *file)
{
  gchar *uri;
  GList *rows;
  GList *l;
  gboolean found = FALSE;

  rows = gtk_container_get_children (GTK_CONTAINER (sidebar->list_box));
  l = rows;
  while (l != NULL && !found)
    {
      g_object_get (l->data, "uri", &uri, NULL);
      if (uri)
        {
          GFile *other;
          other = g_file_new_for_uri (uri);
          found = g_file_equal (file, other);
          g_object_unref (other);
          g_free (uri);
        }
      l = l->next;
    }

  g_list_free (rows);

  return found;
}

static void
on_app_shortcuts_query_complete (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      data)
{
  GtkPlacesSidebar *sidebar = data;
  GFile *file = G_FILE (source);
  GFileInfo *info;

  info = g_file_query_info_finish (file, result, NULL);

  if (info)
    {
      gchar *uri;
      gchar *tooltip;
      const gchar *name;
      GIcon *start_icon;
      int pos = 0;

      name = g_file_info_get_display_name (info);
      start_icon = g_file_info_get_symbolic_icon (info);
      uri = g_file_get_uri (file);
      tooltip = g_file_get_parse_name (file);

      /* XXX: we could avoid this by using an ancillary closure
       * with the index coming from add_application_shortcuts(),
       * but in terms of algorithmic overhead, the application
       * shortcuts is not going to be really big
       */
      pos = g_slist_index (sidebar->shortcuts, file);

      add_place (sidebar, PLACES_BUILT_IN,
                 SECTION_COMPUTER,
                 name, start_icon, NULL, uri,
                 NULL, NULL, NULL, NULL,
                 pos,
                 tooltip);

      g_free (uri);
      g_free (tooltip);

      g_object_unref (info);
    }
}

static void
add_application_shortcuts (GtkPlacesSidebar *sidebar)
{
  GSList *l;

  for (l = sidebar->shortcuts; l; l = l->next)
    {
      GFile *file = l->data;

      if (!should_show_file (sidebar, file))
        continue;

      if (file_is_shown (sidebar, file))
        continue;

      g_file_query_info_async (file,
                               "standard::display-name,standard::symbolic-icon",
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               sidebar->cancellable,
                               on_app_shortcuts_query_complete,
                               sidebar);
    }
}

typedef struct {
  GtkPlacesSidebar *sidebar;
  int index;
  gboolean is_native;
} BookmarkQueryClosure;

static void
on_bookmark_query_info_complete (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      data)
{
  BookmarkQueryClosure *clos = data;
  GtkPlacesSidebar *sidebar = clos->sidebar;
  GFile *root = G_FILE (source);
  GError *error = NULL;
  GFileInfo *info;
  gchar *bookmark_name;
  gchar *mount_uri;
  gchar *tooltip;
  GIcon *start_icon;

  info = g_file_query_info_finish (root, result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    goto out;

  bookmark_name = _gtk_bookmarks_manager_get_bookmark_label (sidebar->bookmarks_manager, root);
  if (bookmark_name == NULL && info != NULL)
    bookmark_name = g_strdup (g_file_info_get_display_name (info));
  else if (bookmark_name == NULL)
    {
      /* Don't add non-UTF-8 bookmarks */
      bookmark_name = g_file_get_basename (root);
      if (bookmark_name == NULL)
        goto out;

      if (!g_utf8_validate (bookmark_name, -1, NULL))
        {
          g_free (bookmark_name);
          goto out;
        }
    }

  if (info)
    start_icon = g_object_ref (g_file_info_get_symbolic_icon (info));
  else
    start_icon = g_themed_icon_new_with_default_fallbacks (clos->is_native ? ICON_NAME_FOLDER : ICON_NAME_FOLDER_NETWORK);

  mount_uri = g_file_get_uri (root);
  tooltip = g_file_get_parse_name (root);

  add_place (sidebar, PLACES_BOOKMARK,
             SECTION_BOOKMARKS,
             bookmark_name, start_icon, NULL, mount_uri,
             NULL, NULL, NULL, NULL, clos->index,
             tooltip);

  g_free (mount_uri);
  g_free (tooltip);
  g_free (bookmark_name);
  g_object_unref (start_icon);

out:
  g_clear_object (&info);
  g_clear_error (&error);
  g_slice_free (BookmarkQueryClosure, clos);
}

static gboolean
is_external_volume (GVolume *volume)
{
  gboolean is_external;
  GDrive *drive;
  gchar *id;

  drive = g_volume_get_drive (volume);
  id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

  is_external = g_volume_can_eject (volume);

  /* NULL volume identifier only happens on removable devices */
  is_external |= !id;

  if (drive)
    is_external |= g_drive_is_removable (drive);

  g_clear_object (&drive);
  g_free (id);

  return is_external;
}

static void
update_trash_icon (GtkPlacesSidebar *sidebar)
{
  if (sidebar->trash_row)
    {
      GIcon *icon;

      icon = _gtk_trash_monitor_get_icon (sidebar->trash_monitor);
      gtk_sidebar_row_set_start_icon (GTK_SIDEBAR_ROW (sidebar->trash_row), icon);
      g_object_unref (icon);
    }
}

#ifdef HAVE_CLOUDPROVIDERS

static gboolean
create_cloud_provider_account_row (GtkPlacesSidebar      *sidebar,
                                   CloudProvidersAccount *account)
{
  GIcon *end_icon;
  GIcon *start_icon;
  gchar *mount_uri;
  gchar *name;
  gchar *tooltip;
  guint provider_account_status;

  start_icon = cloud_providers_account_get_icon (account);
  name = cloud_providers_account_get_name (account);
  provider_account_status = cloud_providers_account_get_status (account);
  mount_uri = cloud_providers_account_get_path (account);
  if (start_icon != NULL
      && name != NULL
      && provider_account_status != CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID
      && mount_uri != NULL)
    {
      switch (provider_account_status)
        {
          case CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE:
            end_icon = NULL;
          break;

          case CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING:
            end_icon = g_themed_icon_new ("emblem-synchronizing-symbolic");
          break;

          case CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR:
            end_icon = g_themed_icon_new ("dialog-warning-symbolic");
          break;

          default:
            return FALSE;
        }

      mount_uri = g_strconcat ("file://", mount_uri, NULL);

      /* translators: %s is the name of a cloud provider for files */
      tooltip = g_strdup_printf (_("Open %s"), name);

      add_place (sidebar, PLACES_BUILT_IN,
                 SECTION_CLOUD,
                 name, start_icon, end_icon, mount_uri,
                 NULL, NULL, NULL, account, 0,
                 tooltip);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
on_account_updated (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
    CloudProvidersAccount *account = CLOUD_PROVIDERS_ACCOUNT (object);
    GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);

    if (create_cloud_provider_account_row (sidebar, account))
      {
          g_signal_handlers_disconnect_by_data (account, sidebar);
          sidebar->unready_accounts = g_list_remove (sidebar->unready_accounts, account);
          g_object_unref (account);
      }
}

#endif

static void
update_places (GtkPlacesSidebar *sidebar)
{
  GList *mounts, *l, *ll;
  GMount *mount;
  GList *drives;
  GDrive *drive;
  GList *volumes;
  GVolume *volume;
  GSList *bookmarks, *sl;
  gint index;
  gchar *original_uri, *mount_uri, *name, *identifier;
  GtkListBoxRow *selected;
  gchar *home_uri;
  GIcon *start_icon;
  GFile *root;
  gchar *tooltip;
  GList *network_mounts, *network_volumes;
  GIcon *new_bookmark_icon;
#ifdef HAVE_CLOUDPROVIDERS
  GList *cloud_providers;
  GList *cloud_providers_accounts;
  CloudProvidersAccount *cloud_provider_account;
  CloudProvidersProvider *cloud_provider;
#endif
  GtkStyleContext *context;

  /* save original selection */
  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));
  if (selected)
    g_object_get (selected, "uri", &original_uri, NULL);
  else
    original_uri = NULL;

  g_cancellable_cancel (sidebar->cancellable);

  g_object_unref (sidebar->cancellable);
  sidebar->cancellable = g_cancellable_new ();

  /* Reset drag state, just in case we update the places while dragging or
   * ending a drag */
  stop_drop_feedback (sidebar);
  gtk_container_foreach (GTK_CONTAINER (sidebar->list_box),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

  network_mounts = network_volumes = NULL;

  /* add built-in places */
  if (should_show_recent (sidebar))
    {
      mount_uri = "recent:///";
      start_icon = g_themed_icon_new_with_default_fallbacks ("document-open-recent-symbolic");
      add_place (sidebar, PLACES_BUILT_IN,
                 SECTION_COMPUTER,
                 _("Recent"), start_icon, NULL, mount_uri,
                 NULL, NULL, NULL, NULL, 0,
                 _("Recent files"));
      g_object_unref (start_icon);
    }

  if (sidebar->show_starred_location)
    {
      mount_uri = "starred:///";
      start_icon = g_themed_icon_new_with_default_fallbacks ("starred-symbolic");
      add_place (sidebar, PLACES_STARRED_LOCATION,
                 SECTION_COMPUTER,
                 _("Starred"), start_icon, NULL, mount_uri,
                 NULL, NULL, NULL, NULL, 0,
                /* TODO: Rename to 'Starred files' */
                 _("Favorite files"));
      g_object_unref (start_icon);
    }

  /* home folder */
  home_uri = get_home_directory_uri ();
  start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_HOME);
  add_place (sidebar, PLACES_BUILT_IN,
             SECTION_COMPUTER,
             _("Home"), start_icon, NULL, home_uri,
             NULL, NULL, NULL, NULL, 0,
             _("Open your personal folder"));
  g_object_unref (start_icon);
  g_free (home_uri);

  /* desktop */
  if (sidebar->show_desktop)
    {
      mount_uri = get_desktop_directory_uri ();
      if (mount_uri)
        {
          start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_DESKTOP);
          add_place (sidebar, PLACES_BUILT_IN,
                     SECTION_COMPUTER,
                     _("Desktop"), start_icon, NULL, mount_uri,
                     NULL, NULL, NULL, NULL, 0,
                     _("Open the contents of your desktop in a folder"));
          g_object_unref (start_icon);
          g_free (mount_uri);
        }
    }

  /* XDG directories */
  add_special_dirs (sidebar);

  if (sidebar->show_enter_location)
    {
      start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_NETWORK_SERVER);
      add_place (sidebar, PLACES_ENTER_LOCATION,
                 SECTION_COMPUTER,
                 _("Enter Location"), start_icon, NULL, NULL,
                 NULL, NULL, NULL, NULL, 0,
                 _("Manually enter a location"));
      g_object_unref (start_icon);
    }

  /* Trash */
  if (!sidebar->local_only && sidebar->show_trash)
    {
      start_icon = _gtk_trash_monitor_get_icon (sidebar->trash_monitor);
      sidebar->trash_row = add_place (sidebar, PLACES_BUILT_IN,
                                      SECTION_COMPUTER,
                                      _("Trash"), start_icon, NULL, "trash:///",
                                      NULL, NULL, NULL, NULL, 0,
                                      _("Open the trash"));
      g_object_add_weak_pointer (G_OBJECT (sidebar->trash_row),
                                 (gpointer *) &sidebar->trash_row);
      g_object_unref (start_icon);
    }

  /* Application-side shortcuts */
  add_application_shortcuts (sidebar);

  /* Cloud providers */
#ifdef HAVE_CLOUDPROVIDERS
  cloud_providers = cloud_providers_collector_get_providers (sidebar->cloud_manager);
  for (l = sidebar->unready_accounts; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (l->data, sidebar);
    }
  g_list_free_full (sidebar->unready_accounts, g_object_unref);
  sidebar->unready_accounts = NULL;
  for (l = cloud_providers; l != NULL; l = l->next)
    {
      cloud_provider = CLOUD_PROVIDERS_PROVIDER (l->data);
      g_signal_connect_swapped (cloud_provider, "accounts-changed",
                                G_CALLBACK (update_places), sidebar);
      cloud_providers_accounts = cloud_providers_provider_get_accounts (cloud_provider);
      for (ll = cloud_providers_accounts; ll != NULL; ll = ll->next)
        {
          cloud_provider_account = CLOUD_PROVIDERS_ACCOUNT (ll->data);
          if (!create_cloud_provider_account_row (sidebar, cloud_provider_account))
            {

              g_signal_connect (cloud_provider_account, "notify::name",
                                G_CALLBACK (on_account_updated), sidebar);
              g_signal_connect (cloud_provider_account, "notify::status",
                                G_CALLBACK (on_account_updated), sidebar);
              g_signal_connect (cloud_provider_account, "notify::status-details",
                                G_CALLBACK (on_account_updated), sidebar);
              g_signal_connect (cloud_provider_account, "notify::path",
                                G_CALLBACK (on_account_updated), sidebar);
              sidebar->unready_accounts = g_list_append (sidebar->unready_accounts,
                                                         g_object_ref (cloud_provider_account));
              continue;
            }

        }
    }
#endif

  /* go through all connected drives */
  drives = g_volume_monitor_get_connected_drives (sidebar->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    {
      drive = l->data;

      volumes = g_drive_get_volumes (drive);
      if (volumes != NULL)
        {
          for (ll = volumes; ll != NULL; ll = ll->next)
            {
              volume = ll->data;
              identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

              if (g_strcmp0 (identifier, "network") == 0)
                {
                  g_free (identifier);
                  network_volumes = g_list_prepend (network_volumes, volume);
                  continue;
                }
              g_free (identifier);

              if (sidebar->show_other_locations && !is_external_volume (volume))
                {
                  g_object_unref (volume);
                  continue;
                }

              mount = g_volume_get_mount (volume);
              if (mount != NULL)
                {
                  /* Show mounted volume in the sidebar */
                  start_icon = g_mount_get_symbolic_icon (mount);
                  root = g_mount_get_default_location (mount);
                  mount_uri = g_file_get_uri (root);
                  name = g_mount_get_name (mount);
                  tooltip = g_file_get_parse_name (root);

                  add_place (sidebar, PLACES_MOUNTED_VOLUME,
                             SECTION_MOUNTS,
                             name, start_icon, NULL, mount_uri,
                             drive, volume, mount, NULL, 0, tooltip);
                  g_object_unref (root);
                  g_object_unref (mount);
                  g_object_unref (start_icon);
                  g_free (tooltip);
                  g_free (name);
                  g_free (mount_uri);
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
                  start_icon = g_volume_get_symbolic_icon (volume);
                  name = g_volume_get_name (volume);
                  tooltip = g_strdup_printf (_("Mount and open “%s”"), name);

                  add_place (sidebar, PLACES_MOUNTED_VOLUME,
                             SECTION_MOUNTS,
                             name, start_icon, NULL, NULL,
                             drive, volume, NULL, NULL, 0, tooltip);
                  g_object_unref (start_icon);
                  g_free (name);
                  g_free (tooltip);
                }
              g_object_unref (volume);
            }
          g_list_free (volumes);
        }
      else
        {
          if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive))
            {
              /* If the drive has no mountable volumes and we cannot detect media change.. we
               * display the drive in the sidebar so the user can manually poll the drive by
               * right clicking and selecting "Rescan..."
               *
               * This is mainly for drives like floppies where media detection doesn't
               * work.. but it's also for human beings who like to turn off media detection
               * in the OS to save battery juice.
               */
              start_icon = g_drive_get_symbolic_icon (drive);
              name = g_drive_get_name (drive);
              tooltip = g_strdup_printf (_("Mount and open “%s”"), name);

              add_place (sidebar, PLACES_BUILT_IN,
                         SECTION_MOUNTS,
                         name, start_icon, NULL, NULL,
                         drive, NULL, NULL, NULL, 0, tooltip);
              g_object_unref (start_icon);
              g_free (tooltip);
              g_free (name);
            }
        }
    }
  g_list_free_full (drives, g_object_unref);

  /* add all network volumes that are not associated with a drive, and
   * loop devices
   */
  volumes = g_volume_monitor_get_volumes (sidebar->volume_monitor);
  for (l = volumes; l != NULL; l = l->next)
    {
      gboolean is_loop = FALSE;
      volume = l->data;
      drive = g_volume_get_drive (volume);
      if (drive != NULL)
        {
          g_object_unref (volume);
          g_object_unref (drive);
          continue;
        }

      identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

      if (g_strcmp0 (identifier, "network") == 0)
        {
          g_free (identifier);
          network_volumes = g_list_prepend (network_volumes, volume);
          continue;
        }
      else if (g_strcmp0 (identifier, "loop") == 0)
        is_loop = TRUE;
      g_free (identifier);

      if (sidebar->show_other_locations &&
          !is_external_volume (volume) &&
          !is_loop)
        {
          g_object_unref (volume);
          continue;
        }

      mount = g_volume_get_mount (volume);
      if (mount != NULL)
        {
          char *mount_uri;
          start_icon = g_mount_get_symbolic_icon (mount);
          root = g_mount_get_default_location (mount);
          mount_uri = g_file_get_uri (root);
          tooltip = g_file_get_parse_name (root);
          name = g_mount_get_name (mount);
          add_place (sidebar, PLACES_MOUNTED_VOLUME,
                     SECTION_MOUNTS,
                     name, start_icon, NULL, mount_uri,
                     NULL, volume, mount, NULL, 0, tooltip);
          g_object_unref (mount);
          g_object_unref (root);
          g_object_unref (start_icon);
          g_free (name);
          g_free (tooltip);
          g_free (mount_uri);
        }
      else
        {
          /* see comment above in why we add an icon for an unmounted mountable volume */
          start_icon = g_volume_get_symbolic_icon (volume);
          name = g_volume_get_name (volume);
          add_place (sidebar, PLACES_MOUNTED_VOLUME,
                     SECTION_MOUNTS,
                     name, start_icon, NULL, NULL,
                     NULL, volume, NULL, NULL, 0, name);
          g_object_unref (start_icon);
          g_free (name);
        }
      g_object_unref (volume);
    }
  g_list_free (volumes);

  /* file system root */
  if (!sidebar->show_other_locations)
    {
      mount_uri = "file:///"; /* No need to strdup */
      start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_FILESYSTEM);
      add_place (sidebar, PLACES_BUILT_IN,
                 SECTION_MOUNTS,
                 sidebar->hostname, start_icon, NULL, mount_uri,
                 NULL, NULL, NULL, NULL, 0,
                 _("Open the contents of the file system"));
      g_object_unref (start_icon);
    }

  /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
  mounts = g_volume_monitor_get_mounts (sidebar->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      mount = l->data;
      if (g_mount_is_shadowed (mount))
        {
          g_object_unref (mount);
          continue;
        }
      volume = g_mount_get_volume (mount);
      if (volume != NULL)
        {
          g_object_unref (volume);
          g_object_unref (mount);
          continue;
        }
      root = g_mount_get_default_location (mount);

      if (!g_file_is_native (root))
        {
          network_mounts = g_list_prepend (network_mounts, mount);
          g_object_unref (root);
          continue;
        }

      start_icon = g_mount_get_symbolic_icon (mount);
      mount_uri = g_file_get_uri (root);
      name = g_mount_get_name (mount);
      tooltip = g_file_get_parse_name (root);
      add_place (sidebar, PLACES_MOUNTED_VOLUME,
                 SECTION_COMPUTER,
                 name, start_icon, NULL, mount_uri,
                 NULL, NULL, mount, NULL, 0, tooltip);
      g_object_unref (root);
      g_object_unref (mount);
      g_object_unref (start_icon);
      g_free (name);
      g_free (mount_uri);
      g_free (tooltip);
    }
  g_list_free (mounts);

  /* add bookmarks */
  bookmarks = _gtk_bookmarks_manager_list_bookmarks (sidebar->bookmarks_manager);

  for (sl = bookmarks, index = 0; sl; sl = sl->next, index++)
    {
      gboolean is_native;
      BookmarkQueryClosure *clos;

      root = sl->data;
      is_native = g_file_is_native (root);

      if (_gtk_bookmarks_manager_get_is_builtin (sidebar->bookmarks_manager, root))
        continue;

      if (sidebar->local_only && !is_native)
        continue;

      clos = g_slice_new (BookmarkQueryClosure);
      clos->sidebar = sidebar;
      clos->index = index;
      clos->is_native = is_native;
      g_file_query_info_async (root,
                               "standard::display-name,standard::symbolic-icon",
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               sidebar->cancellable,
                               on_bookmark_query_info_complete,
                               clos);
    }

  g_slist_free_full (bookmarks, g_object_unref);

  /* Add new bookmark row */
  new_bookmark_icon = g_themed_icon_new ("bookmark-new-symbolic");
  sidebar->new_bookmark_row = add_place (sidebar, PLACES_DROP_FEEDBACK,
                                         SECTION_BOOKMARKS,
                                         _("New bookmark"), new_bookmark_icon, NULL, NULL,
                                         NULL, NULL, NULL, NULL, 0,
                                         _("Add a new bookmark"));
  context = gtk_widget_get_style_context (sidebar->new_bookmark_row);
  gtk_style_context_add_class (context, "sidebar-new-bookmark-row");
  g_object_unref (new_bookmark_icon);

  /* network */
  if (!sidebar->local_only)
    {
      if (sidebar->show_connect_to_server)
        {
          start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_NETWORK_SERVER);
          add_place (sidebar, PLACES_CONNECT_TO_SERVER,
                     SECTION_MOUNTS,
                     _("Connect to Server"), start_icon, NULL,
                     NULL, NULL, NULL, NULL, NULL, 0,
                     _("Connect to a network server address"));
          g_object_unref (start_icon);
        }

      network_volumes = g_list_reverse (network_volumes);
      for (l = network_volumes; l != NULL; l = l->next)
        {
          volume = l->data;
          mount = g_volume_get_mount (volume);

          if (mount != NULL)
            {
              network_mounts = g_list_prepend (network_mounts, mount);
              continue;
            }
          else
            {
              start_icon = g_volume_get_symbolic_icon (volume);
              name = g_volume_get_name (volume);
              tooltip = g_strdup_printf (_("Mount and open “%s”"), name);

              add_place (sidebar, PLACES_MOUNTED_VOLUME,
                         SECTION_MOUNTS,
                         name, start_icon, NULL, NULL,
                         NULL, volume, NULL, NULL, 0, tooltip);
              g_object_unref (start_icon);
              g_free (name);
              g_free (tooltip);
            }
        }

      network_mounts = g_list_reverse (network_mounts);
      for (l = network_mounts; l != NULL; l = l->next)
        {
          mount = l->data;
          root = g_mount_get_default_location (mount);
          start_icon = g_mount_get_symbolic_icon (mount);
          mount_uri = g_file_get_uri (root);
          name = g_mount_get_name (mount);
          tooltip = g_file_get_parse_name (root);
          add_place (sidebar, PLACES_MOUNTED_VOLUME,
                     SECTION_MOUNTS,
                     name, start_icon, NULL, mount_uri,
                     NULL, NULL, mount, NULL, 0, tooltip);
          g_object_unref (root);
          g_object_unref (start_icon);
          g_free (name);
          g_free (mount_uri);
          g_free (tooltip);
        }
    }

  g_list_free_full (network_volumes, g_object_unref);
  g_list_free_full (network_mounts, g_object_unref);

  /* Other locations */
  if (sidebar->show_other_locations)
    {
      start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_OTHER_LOCATIONS);

      add_place (sidebar, PLACES_OTHER_LOCATIONS,
                 SECTION_OTHER_LOCATIONS,
                 _("Other Locations"), start_icon, NULL, "other-locations:///",
                 NULL, NULL, NULL, NULL, 0, _("Show other locations"));

      g_object_unref (start_icon);
    }

  gtk_widget_show_all (GTK_WIDGET (sidebar));
  /* We want this hidden by default, but need to do it after the show_all call */
  gtk_sidebar_row_hide (GTK_SIDEBAR_ROW (sidebar->new_bookmark_row), TRUE);

  /* restore original selection */
  if (original_uri)
    {
      GFile *restore;

      restore = g_file_new_for_uri (original_uri);
      gtk_places_sidebar_set_location (sidebar, restore);
      g_object_unref (restore);
      g_free (original_uri);
    }
}

static gboolean
check_valid_drop_target (GtkPlacesSidebar *sidebar,
                         GtkSidebarRow    *row,
                         GdkDragContext   *context)
{
  GtkPlacesSidebarPlaceType place_type;
  GtkPlacesSidebarSectionType section_type;
  gboolean valid = FALSE;
  gchar *uri;
  GFile *dest_file;
  gint drag_action;

  if (row == NULL)
    return FALSE;

  g_object_get (row,
                "place-type", &place_type,
                "section_type", &section_type,
                "uri", &uri,
                NULL);

  if (place_type == PLACES_STARRED_LOCATION)
    {
      g_free (uri);
      return FALSE;
    }

  if (place_type == PLACES_CONNECT_TO_SERVER)
    {
      g_free (uri);
      return FALSE;
    }

  if (place_type == PLACES_DROP_FEEDBACK)
    {
      g_free (uri);
      return TRUE;
    }

  /* Disallow drops on recent:/// */
  if (place_type == PLACES_BUILT_IN)
    {
      if (g_strcmp0 (uri, "recent:///") == 0)
        {
          g_free (uri);
          return FALSE;
        }
    }

  /* Dragging a bookmark? */
  if (sidebar->drag_data_received &&
      sidebar->drag_data_info == DND_GTK_SIDEBAR_ROW)
    {
      /* Don't allow reordering bookmarks into non-bookmark areas */
      valid = section_type == SECTION_BOOKMARKS;
    }
  else
    {
      /* Dragging a file */
      if (context)
        {
          if (uri != NULL)
            {
              dest_file = g_file_new_for_uri (uri);
              drag_action = emit_drag_action_requested (sidebar, context, dest_file, sidebar->drag_list);
              valid = drag_action > 0;

              g_object_unref (dest_file);
            }
          else
            {
              valid = FALSE;
            }
        }
      else
        {
          /* We cannot discern if it is valid or not because there is not drag
           * context available to ask the client.
           * Simply make insensitive the drop targets we know are not valid for
           * files, that are the ones remaining.
           */
          valid = TRUE;
        }
    }

  g_free (uri);
  return valid;
}

static void
update_possible_drop_targets (GtkPlacesSidebar *sidebar,
                              gboolean          dragging,
                              GdkDragContext   *context)
{
  GList *rows;
  GList *l;
  gboolean sensitive;

  rows = gtk_container_get_children (GTK_CONTAINER (sidebar->list_box));

  for (l = rows; l != NULL; l = l->next)
    {
      sensitive = !dragging || check_valid_drop_target (sidebar, GTK_SIDEBAR_ROW (l->data), context);
      gtk_widget_set_sensitive (GTK_WIDGET (l->data), sensitive);
    }

  g_list_free (rows);
}

static gboolean
get_drag_data (GtkWidget      *list_box,
               GdkDragContext *context,
               guint           time)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (list_box, context, NULL);

  if (target == GDK_NONE)
    return FALSE;

  gtk_drag_get_data (list_box, context, target, time);

  return TRUE;
}

static void
free_drag_data (GtkPlacesSidebar *sidebar)
{
  sidebar->drag_data_received = FALSE;

  if (sidebar->drag_list)
    {
      g_list_free_full (sidebar->drag_list, g_object_unref);
      sidebar->drag_list = NULL;
    }

}

static void
start_drop_feedback (GtkPlacesSidebar *sidebar,
                     GtkSidebarRow    *row,
                     GdkDragContext   *context)
{
  if (sidebar->drag_data_info != DND_GTK_SIDEBAR_ROW)
    {
      gtk_sidebar_row_reveal (GTK_SIDEBAR_ROW (sidebar->new_bookmark_row));
      /* If the state is permanent, don't change it. The application controls it. */
      if (sidebar->drop_state != DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT)
        sidebar->drop_state = DROP_STATE_NEW_BOOKMARK_ARMED;
    }

  update_possible_drop_targets (sidebar, TRUE, context);
}

static void
stop_drop_feedback (GtkPlacesSidebar *sidebar)
{
  update_possible_drop_targets (sidebar, FALSE, NULL);

  free_drag_data (sidebar);

  if (sidebar->drop_state != DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT &&
      sidebar->new_bookmark_row != NULL)
    {
      gtk_sidebar_row_hide (GTK_SIDEBAR_ROW (sidebar->new_bookmark_row), FALSE);
      sidebar->drop_state = DROP_STATE_NORMAL;
    }

  if (sidebar->drag_row != NULL)
    {
      gtk_widget_show (sidebar->drag_row);
      sidebar->drag_row = NULL;
    }

  if (sidebar->row_placeholder != NULL)
    {
      gtk_widget_destroy (sidebar->row_placeholder);
      sidebar->row_placeholder = NULL;
    }

  sidebar->dragging_over = FALSE;
  sidebar->drag_data_info = DND_UNKNOWN;
}

static gboolean
on_motion_notify_event (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        user_data)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);

  if (sidebar->drag_row == NULL || sidebar->dragging_over)
    return FALSE;

  if (!(event->state & GDK_BUTTON1_MASK))
    return FALSE;

  if (gtk_drag_check_threshold (widget,
                                sidebar->drag_root_x, sidebar->drag_root_y,
                                event->x_root, event->y_root))
    {
      sidebar->dragging_over = TRUE;

      gtk_drag_begin_with_coordinates (widget, sidebar->source_targets, GDK_ACTION_MOVE,
                                       GDK_BUTTON_PRIMARY, (GdkEvent*)event,
                                       -1, -1);
    }

  return FALSE;
}

static void
drag_begin_callback (GtkWidget      *widget,
                     GdkDragContext *context,
                     gpointer        user_data)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);
  GtkAllocation allocation;
  GtkWidget *drag_widget;
  GtkWidget *window;

  gtk_widget_get_allocation (sidebar->drag_row, &allocation);
  gtk_widget_hide (sidebar->drag_row);

  drag_widget = GTK_WIDGET (gtk_sidebar_row_clone (GTK_SIDEBAR_ROW (sidebar->drag_row)));
  window = gtk_window_new (GTK_WINDOW_POPUP);
  sidebar->drag_row_height = allocation.height;
  gtk_widget_set_size_request (window, allocation.width, allocation.height);

  gtk_container_add (GTK_CONTAINER (window), drag_widget);
  gtk_widget_show_all (window);
  gtk_widget_set_opacity (window, 0.8);

  gtk_drag_set_icon_widget (context,
                            window,
                            sidebar->drag_row_x,
                            sidebar->drag_row_y);
}

static GtkWidget *
create_placeholder_row (GtkPlacesSidebar *sidebar)
{
  return 	g_object_new (GTK_TYPE_SIDEBAR_ROW,
                        "placeholder", TRUE,
                        NULL);
}

static gboolean
drag_motion_callback (GtkWidget      *widget,
                      GdkDragContext *context,
                      gint            x,
                      gint            y,
                      guint           time,
                      gpointer        user_data)
{
  gint action;
  GtkListBoxRow *row;
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);
  GtkPlacesSidebarPlaceType place_type;
  gchar *drop_target_uri = NULL;
  gint row_index;
  gint row_placeholder_index;

  sidebar->dragging_over = TRUE;
  action = 0;
  row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (sidebar->list_box), y);

  gtk_list_box_drag_unhighlight_row (GTK_LIST_BOX (sidebar->list_box));

  /* Nothing to do if no drag data */
  if (!sidebar->drag_data_received &&
      !get_drag_data (sidebar->list_box, context, time))
    goto out;

  /* Nothing to do if the target is not valid drop destination */
  if (!check_valid_drop_target (sidebar, GTK_SIDEBAR_ROW (row), context))
    goto out;

  if (sidebar->drag_data_received &&
      sidebar->drag_data_info == DND_GTK_SIDEBAR_ROW)
    {
      /* Dragging bookmarks always moves them to another position in the bookmarks list */
      action = GDK_ACTION_MOVE;
      if (sidebar->row_placeholder == NULL)
        {
          sidebar->row_placeholder = create_placeholder_row (sidebar);
          gtk_widget_show (sidebar->row_placeholder);
          g_object_ref_sink (sidebar->row_placeholder);
        }
      else if (GTK_WIDGET (row) == sidebar->row_placeholder)
        {
          goto out;
        }

      if (gtk_widget_get_parent (sidebar->row_placeholder) != NULL)
        {
          gtk_container_remove (GTK_CONTAINER (sidebar->list_box),
                                sidebar->row_placeholder);
        }

      if (row != NULL)
        {
          gint dest_y, dest_x;

          g_object_get (row, "order-index", &row_index, NULL);
          g_object_get (sidebar->row_placeholder, "order-index", &row_placeholder_index, NULL);
          /* We order the bookmarks sections based on the bookmark index that we
           * set on the row as order-index property, but we have to deal with
           * the placeholder row wanting to be between two consecutive bookmarks,
           * with two consecutive order-index values which is the usual case.
           * For that, in the list box sort func we give priority to the placeholder row,
           * that means that if the index-order is the same as another bookmark
           * the placeholder row goes before. However if we want to show it after
           * the current row, for instance when the cursor is in the lower half
           * of the row, we need to increase the order-index.
           */
          row_placeholder_index = row_index;
          gtk_widget_translate_coordinates (widget, GTK_WIDGET (row),
		                            x, y,
		                            &dest_x, &dest_y);

          if (dest_y > sidebar->drag_row_height / 2 && row_index > 0)
            row_placeholder_index++;
        }
      else
        {
          /* If the user is dragging over an area that has no row, place the row
           * placeholder in the last position
           */
          row_placeholder_index = G_MAXINT32;
        }

      g_object_set (sidebar->row_placeholder, "order-index", row_placeholder_index, NULL);

      gtk_list_box_prepend (GTK_LIST_BOX (sidebar->list_box),
                            sidebar->row_placeholder);
    }
  else
    {
      gtk_list_box_drag_highlight_row (GTK_LIST_BOX (sidebar->list_box), row);

      g_object_get (row,
                    "place-type", &place_type,
                    "uri", &drop_target_uri,
                    NULL);
      /* URIs are being dragged.  See if the caller wants to handle a
       * file move/copy operation itself, or if we should only try to
       * create bookmarks out of the dragged URIs.
       */
      if (sidebar->drag_list != NULL)
        {
          if (place_type == PLACES_DROP_FEEDBACK)
            {
              action = GDK_ACTION_COPY;
            }
          else
            {
              /* uri may be NULL for unmounted volumes, for example, so we don't allow drops there */
              if (drop_target_uri != NULL)
                {
                  GFile *dest_file = g_file_new_for_uri (drop_target_uri);

                  action = emit_drag_action_requested (sidebar, context, dest_file, sidebar->drag_list);

                  g_object_unref (dest_file);
                }
            }
        }

      g_free (drop_target_uri);
    }

 out:
  start_drop_feedback (sidebar, GTK_SIDEBAR_ROW (row), context);

  g_signal_stop_emission_by_name (sidebar->list_box, "drag-motion");

  gdk_drag_status (context, action, time);

  return TRUE;
}

/* Takes an array of URIs and turns it into a list of GFile */
static GList *
build_file_list_from_uris (const gchar **uris)
{
  GList *result;
  gint i;

  result = NULL;
  for (i = 0; uris && uris[i]; i++)
    {
      GFile *file;

      file = g_file_new_for_uri (uris[i]);
      result = g_list_prepend (result, file);
    }

  return g_list_reverse (result);
}

/* Reorders the bookmark to the specified position */
static void
reorder_bookmarks (GtkPlacesSidebar *sidebar,
                   GtkSidebarRow    *row,
                   gint              new_position)
{
  gchar *uri;
  GFile *file;

  g_object_get (row, "uri", &uri, NULL);
  file = g_file_new_for_uri (uri);
  _gtk_bookmarks_manager_reorder_bookmark (sidebar->bookmarks_manager, file, new_position, NULL);

  g_object_unref (file);
  g_free (uri);
}

/* Creates bookmarks for the specified files at the given position in the bookmarks list */
static void
drop_files_as_bookmarks (GtkPlacesSidebar *sidebar,
                         GList            *files,
                         gint              position)
{
  GList *l;

  for (l = files; l; l = l->next)
    {
      GFile *f = G_FILE (l->data);
      GFileInfo *info = g_file_query_info (f,
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           NULL,
                                           NULL);

      if (info)
        {
          if (_gtk_file_info_consider_as_directory (info))
            _gtk_bookmarks_manager_insert_bookmark (sidebar->bookmarks_manager, f, position++, NULL);

          g_object_unref (info);
        }
    }
}

static void
drag_data_get_callback (GtkWidget        *widget,
                        GdkDragContext   *context,
                        GtkSelectionData *data,
                        guint             info,
                        guint             time,
                        gpointer          user_data)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);
  GdkAtom target = gtk_selection_data_get_target (data);

  if (target == gdk_atom_intern_static_string ("DND_GTK_SIDEBAR_ROW"))
    {
      gtk_selection_data_set (data,
                              target,
                              8,
                              (void*)&sidebar->drag_row,
                              sizeof (gpointer));
    }
}

static void
drag_data_received_callback (GtkWidget        *list_box,
                             GdkDragContext   *context,
                             int               x,
                             int               y,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time,
                             gpointer          user_data)
{
  gint target_order_index;
  GtkPlacesSidebarPlaceType target_place_type;
  GtkPlacesSidebarSectionType target_section_type;
  gchar *target_uri;
  gboolean success;
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);
  GtkListBoxRow *target_row;

  if (!sidebar->drag_data_received)
    {
      if (gtk_selection_data_get_target (selection_data) != GDK_NONE &&
          info == DND_TEXT_URI_LIST)
        {
          gchar **uris;

          uris = gtk_selection_data_get_uris (selection_data);
          /* Free spurious drag data from previous drags if present */
          if (sidebar->drag_list != NULL)
            g_list_free_full (sidebar->drag_list, g_object_unref);
          sidebar->drag_list = build_file_list_from_uris ((const char **) uris);
          g_strfreev (uris);
        }
      else
        {
          sidebar->drag_list = NULL;
        }
      sidebar->drag_data_received = TRUE;
      sidebar->drag_data_info = info;
    }

  g_signal_stop_emission_by_name (list_box, "drag-data-received");

  if (!sidebar->drop_occurred)
    return;

  target_row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (sidebar->list_box), y);

  if (target_row == NULL)
    return;

  g_object_get (target_row,
                "place-type", &target_place_type,
                "section-type", &target_section_type,
                "order-index", &target_order_index,
                "uri", &target_uri,
                NULL);

  success = FALSE;

  if (!check_valid_drop_target (sidebar, GTK_SIDEBAR_ROW (target_row), context))
    goto out;

  if (sidebar->drag_data_info == DND_GTK_SIDEBAR_ROW)
    {
      GtkWidget **source_row;
      /* A bookmark got reordered */
      if (target_section_type != SECTION_BOOKMARKS)
        goto out;

      source_row = (void*) gtk_selection_data_get_data (selection_data);

      if (sidebar->row_placeholder != NULL)
        g_object_get (sidebar->row_placeholder, "order-index", &target_order_index, NULL);

      reorder_bookmarks (sidebar, GTK_SIDEBAR_ROW (*source_row), target_order_index);
      success = TRUE;
    }
  else
    {
      /* Dropping URIs! */
      GdkDragAction real_action;
      gchar **uris;
      GList *source_file_list;

      /* file transfer requested */
      real_action = gdk_drag_context_get_selected_action (context);

      if (real_action == GDK_ACTION_ASK)
        real_action = emit_drag_action_ask (sidebar, gdk_drag_context_get_actions (context));

      if (real_action > 0)
        {
          GFile *dest_file;

          uris = gtk_selection_data_get_uris (selection_data);
          source_file_list = build_file_list_from_uris ((const gchar **) uris);

          if (target_place_type == PLACES_DROP_FEEDBACK)
            {
                drop_files_as_bookmarks (sidebar, source_file_list, target_order_index);
            }
          else
            {
              dest_file = g_file_new_for_uri (target_uri);

              emit_drag_perform_drop (sidebar, dest_file, source_file_list, real_action);

              g_object_unref (dest_file);
            }

          success = TRUE;
          g_list_free_full (source_file_list, g_object_unref);
          g_strfreev (uris);
        }
    }

out:
  sidebar->drop_occurred = FALSE;
  gtk_drag_finish (context, success, FALSE, time);
  stop_drop_feedback (sidebar);
  g_free (target_uri);
}

static void
drag_end_callback (GtkWidget      *widget,
                   GdkDragContext *context,
                   gpointer        user_data)
{
  stop_drop_feedback (GTK_PLACES_SIDEBAR (user_data));
}

/* This functions is called every time the drag source leaves
 * the sidebar widget.
 * The problem is that, we start showing hints for drop when the source
 * start being above the sidebar or when the application request so show
 * drop hints, but at some moment we need to restore to normal
 * state.
 * One could think that here we could simply call stop_drop_feedback,
 * but that's not true, because this function is called also before drag_drop,
 * which needs the data from the drag so we cannot free the drag data here.
 * So now one could think we could just do nothing here, and wait for
 * drag-end or drag-failed signals and just stop_drop_feedback there. But that
 * is also not true, since when the drag comes from a diferent widget than the
 * sidebar, when the drag stops the last drag signal we receive is drag-leave.
 * So here what we will do is restore the state of the sidebar as if no drag
 * is being done (and if the application didnt request for permanent hints with
 * gtk_places_sidebar_show_drop_hints) and we will free the drag data next time
 * we build new drag data in drag_data_received.
 */
static void
drag_leave_callback (GtkWidget      *widget,
                     GdkDragContext *context,
                     guint           time,
                     gpointer        user_data)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);

  if (sidebar->drop_state != DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT)
    {
      update_possible_drop_targets (sidebar, FALSE, context);
      gtk_sidebar_row_hide (GTK_SIDEBAR_ROW (sidebar->new_bookmark_row), FALSE);
      sidebar->drop_state = DROP_STATE_NORMAL;
    }

  sidebar->drag_data_received = FALSE;
  sidebar->dragging_over = FALSE;
  sidebar->drag_data_info = DND_UNKNOWN;
}

static gboolean
drag_drop_callback (GtkWidget      *list_box,
                    GdkDragContext *context,
                    gint            x,
                    gint            y,
                    guint           time,
                    gpointer        user_data)
{
  gboolean retval = FALSE;
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);

  sidebar->drop_occurred = TRUE;
  retval = get_drag_data (sidebar->list_box, context, time);
  g_signal_stop_emission_by_name (sidebar->list_box, "drag-drop");

  return retval;
}

static void
check_unmount_and_eject (GMount   *mount,
                         GVolume  *volume,
                         GDrive   *drive,
                         gboolean *show_unmount,
                         gboolean *show_eject)
{
  *show_unmount = FALSE;
  *show_eject = FALSE;

  if (drive != NULL)
    *show_eject = g_drive_can_eject (drive);

  if (volume != NULL)
    *show_eject |= g_volume_can_eject (volume);

  if (mount != NULL)
    {
      *show_eject |= g_mount_can_eject (mount);
      *show_unmount = g_mount_can_unmount (mount) && !*show_eject;
    }
}

static void
check_visibility (GMount   *mount,
                  GVolume  *volume,
                  GDrive   *drive,
                  gboolean *show_mount,
                  gboolean *show_unmount,
                  gboolean *show_eject,
                  gboolean *show_rescan,
                  gboolean *show_start,
                  gboolean *show_stop)
{
  *show_mount = FALSE;
  *show_rescan = FALSE;
  *show_start = FALSE;
  *show_stop = FALSE;

  check_unmount_and_eject (mount, volume, drive, show_unmount, show_eject);

  if (drive != NULL)
    {
      if (g_drive_is_media_removable (drive) &&
          !g_drive_is_media_check_automatic (drive) &&
          g_drive_can_poll_for_media (drive))
        *show_rescan = TRUE;

      *show_start = g_drive_can_start (drive) || g_drive_can_start_degraded (drive);
      *show_stop  = g_drive_can_stop (drive);

      if (*show_stop)
        *show_unmount = FALSE;
    }

  if (volume != NULL)
    {
      if (mount == NULL)
        *show_mount = g_volume_can_mount (volume);
    }
}

typedef struct {
  GtkWidget *add_shortcut_item;
  GtkWidget *remove_item;
  GtkWidget *rename_item;
  GtkWidget *separator_item;
  GtkWidget *mount_item;
  GtkWidget *unmount_item;
  GtkWidget *eject_item;
  GtkWidget *rescan_item;
  GtkWidget *start_item;
  GtkWidget *stop_item;
} PopoverData;

static void
check_popover_sensitivity (GtkSidebarRow *row,
                           PopoverData   *data)
{
  gboolean show_mount;
  gboolean show_unmount;
  gboolean show_eject;
  gboolean show_rescan;
  gboolean show_start;
  gboolean show_stop;
  GtkPlacesSidebarPlaceType type;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;
  GtkWidget *sidebar;
  GActionGroup *actions;
  GAction *action;

  g_object_get (row,
                "sidebar", &sidebar,
                "place-type", &type,
                "drive", &drive,
                "volume", &volume,
                "mount", &mount,
                NULL);

  gtk_widget_set_visible (data->add_shortcut_item, (type == PLACES_MOUNTED_VOLUME));

  actions = gtk_widget_get_action_group (sidebar, "row");
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "remove");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), (type == PLACES_BOOKMARK));
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "rename");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), (type == PLACES_BOOKMARK ||
                                                          type == PLACES_XDG_DIR));
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "open");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (row)));

  check_visibility (mount, volume, drive,
                    &show_mount, &show_unmount, &show_eject, &show_rescan, &show_start, &show_stop);

  gtk_widget_set_visible (data->separator_item, show_mount || show_unmount || show_eject);
  gtk_widget_set_visible (data->mount_item, show_mount);
  gtk_widget_set_visible (data->unmount_item, show_unmount);
  gtk_widget_set_visible (data->eject_item, show_eject);
  gtk_widget_set_visible (data->rescan_item, show_rescan);
  gtk_widget_set_visible (data->start_item, show_start);
  gtk_widget_set_visible (data->stop_item, show_stop);

  /* Adjust start/stop items to reflect the type of the drive */
  g_object_set (data->start_item, "text", _("_Start"), NULL);
  g_object_set (data->stop_item, "text", _("_Stop"), NULL);
  if ((show_start || show_stop) && drive != NULL)
    {
      switch (g_drive_get_start_stop_type (drive))
        {
        case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
          /* start() for type G_DRIVE_START_STOP_TYPE_SHUTDOWN is normally not used */
          g_object_set (data->start_item, "text", _("_Power On"), NULL);
          g_object_set (data->stop_item, "text", _("_Safely Remove Drive"), NULL);
          break;

        case G_DRIVE_START_STOP_TYPE_NETWORK:
          g_object_set (data->start_item, "text", _("_Connect Drive"), NULL);
          g_object_set (data->stop_item, "text", _("_Disconnect Drive"), NULL);
          break;

        case G_DRIVE_START_STOP_TYPE_MULTIDISK:
          g_object_set (data->start_item, "text", _("_Start Multi-disk Device"), NULL);
          g_object_set (data->stop_item, "text", _("_Stop Multi-disk Device"), NULL);
          break;

        case G_DRIVE_START_STOP_TYPE_PASSWORD:
          /* stop() for type G_DRIVE_START_STOP_TYPE_PASSWORD is normally not used */
          g_object_set (data->start_item, "text", _("_Unlock Device"), NULL);
          g_object_set (data->stop_item, "text", _("_Lock Device"), NULL);
          break;

        default:
        case G_DRIVE_START_STOP_TYPE_UNKNOWN:
          /* uses defaults set above */
          break;
        }
    }

  if (drive)
    g_object_unref (drive);
  if (volume)
    g_object_unref (volume);
  if (mount)
    g_object_unref (mount);

  g_object_unref (sidebar);
}

static void
drive_start_from_bookmark_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = GTK_PLACES_SIDEBAR (user_data);

  error = NULL;
  if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to start “%s”"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }
}

static void
volume_mount_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GtkSidebarRow *row = GTK_SIDEBAR_ROW (user_data);
  GtkPlacesSidebar *sidebar;
  GVolume *volume;
  GError *error;
  gchar *primary;
  gchar *name;
  GMount *mount;

  volume = G_VOLUME (source_object);
  g_object_get (row, "sidebar", &sidebar, NULL);

  error = NULL;
  if (!g_volume_mount_finish (volume, result, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED &&
          error->code != G_IO_ERROR_ALREADY_MOUNTED)
        {
          name = g_volume_get_name (G_VOLUME (source_object));
          if (g_str_has_prefix (error->message, "Error unlocking"))
            primary = g_strdup_printf (_("Error unlocking “%s”"), name);
          else
            primary = g_strdup_printf (_("Unable to access “%s”"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  sidebar->mounting = FALSE;
  gtk_sidebar_row_set_busy (row, FALSE);

  mount = g_volume_get_mount (volume);
  if (mount != NULL)
    {
      GFile *location;

      location = g_mount_get_default_location (mount);
      emit_open_location (sidebar, location, sidebar->go_to_after_mount_open_flags);

      g_object_unref (G_OBJECT (location));
      g_object_unref (G_OBJECT (mount));
    }

  g_object_unref (row);
  g_object_unref (sidebar);
}

static void
mount_volume (GtkSidebarRow *row,
              GVolume       *volume)
{
  GtkPlacesSidebar *sidebar;
  GMountOperation *mount_op;

  g_object_get (row, "sidebar", &sidebar, NULL);

  mount_op = get_mount_operation (sidebar);
  g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);

  g_object_ref (row);
  g_object_ref (sidebar);
  g_volume_mount (volume, 0, mount_op, NULL, volume_mount_cb, row);
}

static void
open_drive (GtkSidebarRow      *row,
            GDrive             *drive,
            GtkPlacesOpenFlags  open_flags)
{
  GtkPlacesSidebar *sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  if (drive != NULL &&
      (g_drive_can_start (drive) || g_drive_can_start_degraded (drive)))
    {
      GMountOperation *mount_op;

      gtk_sidebar_row_set_busy (row, TRUE);
      mount_op = get_mount_operation (sidebar);
      g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_from_bookmark_cb, NULL);
      g_object_unref (mount_op);
    }
}

static void
open_volume (GtkSidebarRow      *row,
             GVolume            *volume,
             GtkPlacesOpenFlags  open_flags)
{
  GtkPlacesSidebar *sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  if (volume != NULL && !sidebar->mounting)
    {
      sidebar->mounting = TRUE;
      sidebar->go_to_after_mount_open_flags = open_flags;
      gtk_sidebar_row_set_busy (row, TRUE);
      mount_volume (row, volume);
    }
}

static void
open_uri (GtkPlacesSidebar   *sidebar,
          const gchar        *uri,
          GtkPlacesOpenFlags  open_flags)
{
  GFile *location;

  location = g_file_new_for_uri (uri);
  emit_open_location (sidebar, location, open_flags);
  g_object_unref (location);
}

static void
open_row (GtkSidebarRow      *row,
          GtkPlacesOpenFlags  open_flags)
{
  gchar *uri;
  GDrive *drive;
  GVolume *volume;
  GtkPlacesSidebarPlaceType place_type;
  GtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "uri", &uri,
                "place-type", &place_type,
                "drive", &drive,
                "volume", &volume,
                NULL);

  if (place_type == PLACES_OTHER_LOCATIONS)
    {
      emit_show_other_locations (sidebar);
      emit_show_other_locations_with_flags (sidebar, open_flags);
    }
  else if (place_type == PLACES_STARRED_LOCATION)
    {
      emit_show_starred_location (sidebar, open_flags);
    }
  else if (uri != NULL)
    {
      open_uri (sidebar, uri, open_flags);
    }
  else if (place_type == PLACES_CONNECT_TO_SERVER)
    {
      emit_show_connect_to_server (sidebar);
    }
  else if (place_type == PLACES_ENTER_LOCATION)
    {
      emit_show_enter_location (sidebar);
    }
  else if (volume != NULL)
    {
      open_volume (row, volume, open_flags);
    }
  else if (drive != NULL)
    {
      open_drive (row, drive, open_flags);
    }

  g_object_unref (sidebar);
  if (drive)
    g_object_unref (drive);
  if (volume)
    g_object_unref (volume);
  g_free (uri);
}

/* Callback used for the "Open" menu items in the context menu */
static void
open_shortcut_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GtkPlacesOpenFlags flags;

  flags = (GtkPlacesOpenFlags)g_variant_get_int32 (parameter);
  open_row (sidebar->context_row, flags);
}

/* Add bookmark for the selected item - just used from mount points */
static void
add_shortcut_cb (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  gchar *uri;
  gchar *name;
  GFile *location;

  g_object_get (sidebar->context_row,
                "uri", &uri,
                "label", &name,
                NULL);

  if (uri != NULL)
    {
      location = g_file_new_for_uri (uri);
      if (_gtk_bookmarks_manager_insert_bookmark (sidebar->bookmarks_manager, location, -1, NULL))
        _gtk_bookmarks_manager_set_bookmark_label (sidebar->bookmarks_manager, location, name, NULL);
      g_object_unref (location);
    }

  g_free (uri);
  g_free (name);
}

static void
rename_entry_changed (GtkEntry         *entry,
                      GtkPlacesSidebar *sidebar)
{
  GtkPlacesSidebarPlaceType type;
  gchar *name;
  gchar *uri;
  const gchar *new_name;
  gboolean found = FALSE;
  GList *rows;
  GList *l;

  new_name = gtk_entry_get_text (GTK_ENTRY (sidebar->rename_entry));

  if (strcmp (new_name, "") == 0)
    {
      gtk_widget_set_sensitive (sidebar->rename_button, FALSE);
      gtk_label_set_label (GTK_LABEL (sidebar->rename_error), "");
      return;
    }

  rows = gtk_container_get_children (GTK_CONTAINER (sidebar->list_box));
  for (l = rows; l && !found; l = l->next)
    {
      g_object_get (l->data,
                    "place-type", &type,
                    "uri", &uri,
                    "label", &name,
                    NULL);

      if ((type == PLACES_XDG_DIR || type == PLACES_BOOKMARK) &&
          strcmp (uri, sidebar->rename_uri) != 0 &&
          strcmp (new_name, name) == 0)
        found = TRUE;

      g_free (uri);
      g_free (name);
    }
  g_list_free (rows);

  gtk_widget_set_sensitive (sidebar->rename_button, !found);
  gtk_label_set_label (GTK_LABEL (sidebar->rename_error),
                       found ? _("This name is already taken") : "");
}

static void
do_rename (GtkButton        *button,
           GtkPlacesSidebar *sidebar)
{
  gchar *new_text;
  GFile *file;

  new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (sidebar->rename_entry)));

  file = g_file_new_for_uri (sidebar->rename_uri);
  if (!_gtk_bookmarks_manager_has_bookmark (sidebar->bookmarks_manager, file))
    _gtk_bookmarks_manager_insert_bookmark (sidebar->bookmarks_manager, file, -1, NULL);

  _gtk_bookmarks_manager_set_bookmark_label (sidebar->bookmarks_manager, file, new_text, NULL);

  g_object_unref (file);
  g_free (new_text);

  g_clear_pointer (&sidebar->rename_uri, g_free);

  if (sidebar->rename_popover)
    gtk_popover_popdown (GTK_POPOVER (sidebar->rename_popover));
}

static void
on_rename_popover_destroy (GtkWidget        *rename_popover,
                           GtkPlacesSidebar *sidebar)
{
  if (sidebar)
    {
      sidebar->rename_popover = NULL;
      sidebar->rename_entry = NULL;
      sidebar->rename_button = NULL;
      sidebar->rename_error = NULL;
    }
}

static void
create_rename_popover (GtkPlacesSidebar *sidebar)
{
  GtkWidget *popover;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *error;
  gchar *str;

  if (sidebar->rename_popover)
    return;

  popover = gtk_popover_new (GTK_WIDGET (sidebar));
  /* Clean sidebar pointer when its destroyed, most of the times due to its
   * relative_to associated row being destroyed */
  g_signal_connect (popover, "destroy", G_CALLBACK (on_rename_popover_destroy), sidebar);
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_RIGHT);
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (popover), grid);
  g_object_set (grid,
                "margin", 10,
                "row-spacing", 6,
                "column-spacing", 6,
                NULL);
  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_signal_connect (entry, "changed", G_CALLBACK (rename_entry_changed), sidebar);
  str = g_strdup_printf ("<b>%s</b>", _("Name"));
  label = gtk_label_new (str);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  g_free (str);
  button = gtk_button_new_with_mnemonic (_("_Rename"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
  g_signal_connect (button, "clicked", G_CALLBACK (do_rename), sidebar);
  error = gtk_label_new ("");
  gtk_widget_set_halign (error, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), button,1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), error, 0, 2, 2, 1);
  gtk_widget_show_all (grid);
  gtk_popover_set_default_widget (GTK_POPOVER (popover), button);

  sidebar->rename_popover = popover;
  sidebar->rename_entry = entry;
  sidebar->rename_button = button;
  sidebar->rename_error = error;
}

/* Style the row differently while we show a popover for it.
 * Otherwise, the popover is 'pointing to nothing'. Since the
 * main popover and the rename popover interleave their hiding
 * and showing, we have to count to ensure that we don't loose
 * the state before the last popover is gone.
 *
 * This would be nicer as a state, but reusing hover for this
 * interferes with the normal handling of this state, so just
 * use a style class.
 */
static void
update_popover_shadowing (GtkWidget *row,
                          gboolean   shown)
{
  GtkStyleContext *context;
  gint count;

  count = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "popover-count"));
  count = shown ? count + 1 : count - 1;
  g_object_set_data (G_OBJECT (row), "popover-count", GINT_TO_POINTER (count));

  context = gtk_widget_get_style_context (row);
  if (count > 0)
    gtk_style_context_add_class (context, "has-open-popup");
  else
    gtk_style_context_remove_class (context, "has-open-popup");
}

static void
set_prelight (GtkPopover *popover)
{
  update_popover_shadowing (gtk_popover_get_relative_to (popover), TRUE);
}

static void
unset_prelight (GtkPopover *popover)
{
  update_popover_shadowing (gtk_popover_get_relative_to (popover), FALSE);
}

static void
setup_popover_shadowing (GtkWidget *popover)
{
  g_signal_connect (popover, "map", G_CALLBACK (set_prelight), NULL);
  g_signal_connect (popover, "unmap", G_CALLBACK (unset_prelight), NULL);
}

static void
show_rename_popover (GtkSidebarRow *row)
{
  gchar *name;
  gchar *uri;
  GtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "label", &name,
                "uri", &uri,
                NULL);

  create_rename_popover (sidebar);

  if (sidebar->rename_uri)
    g_free (sidebar->rename_uri);
  sidebar->rename_uri = g_strdup (uri);

  gtk_entry_set_text (GTK_ENTRY (sidebar->rename_entry), name);
  gtk_popover_set_relative_to (GTK_POPOVER (sidebar->rename_popover), GTK_WIDGET (row));
  setup_popover_shadowing (sidebar->rename_popover);

  gtk_popover_popup (GTK_POPOVER (sidebar->rename_popover));
  gtk_widget_grab_focus (sidebar->rename_entry);

  g_free (name);
  g_free (uri);
  g_object_unref (sidebar);
}

static void
rename_bookmark (GtkSidebarRow *row)
{
  GtkPlacesSidebarPlaceType type;

  g_object_get (row, "place-type", &type, NULL);

  if (type != PLACES_BOOKMARK && type != PLACES_XDG_DIR)
    return;

  show_rename_popover (row);
}

static void
rename_shortcut_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;

  rename_bookmark (sidebar->context_row);
}

static void
remove_bookmark (GtkSidebarRow *row)
{
  GtkPlacesSidebarPlaceType type;
  gchar *uri;
  GFile *file;
  GtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "place-type", &type,
                "uri", &uri,
                NULL);

  if (type == PLACES_BOOKMARK)
    {
      file = g_file_new_for_uri (uri);
      _gtk_bookmarks_manager_remove_bookmark (sidebar->bookmarks_manager, file, NULL);
      g_object_unref (file);
    }

  g_free (uri);
  g_object_unref (sidebar);
}

static void
remove_shortcut_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;

  remove_bookmark (sidebar->context_row);
}

static void
mount_shortcut_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GVolume *volume;

  g_object_get (sidebar->context_row,
                "volume", &volume,
                NULL);

  if (volume != NULL)
    mount_volume (sidebar->context_row, volume);

  g_object_unref (volume);
}

/* Callback used from g_mount_unmount_with_operation() */
static void
unmount_mount_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (user_data);
  GMount *mount;
  GError *error;

  mount = G_MOUNT (source_object);

  error = NULL;
  if (!g_mount_unmount_with_operation_finish (mount, result, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          gchar *name;
          gchar *primary;

          name = g_mount_get_name (mount);
          primary = g_strdup_printf (_("Unable to unmount “%s”"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }

      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static GMountOperation *
get_mount_operation (GtkPlacesSidebar *sidebar)
{
  GMountOperation *mount_op;

  mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));

  emit_mount_operation (sidebar, mount_op);

  return mount_op;
}

static GMountOperation *
get_unmount_operation (GtkPlacesSidebar *sidebar)
{
  GMountOperation *mount_op;

  mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));

  emit_unmount_operation (sidebar, mount_op);

  return mount_op;
}

/* Returns TRUE if file1 is prefix of file2 or if both files have the
 * same path
 */
static gboolean
file_prefix_or_same (GFile *file1,
                     GFile *file2)
{
  return g_file_has_prefix (file1, file2) ||
         g_file_equal (file1, file2);
}

static gboolean
is_current_location_on_volume (GtkPlacesSidebar *sidebar,
                               GMount           *mount,
                               GVolume          *volume,
                               GDrive           *drive)
{
  gboolean  current_location_on_volume;
  GFile    *mount_default_location;
  GMount   *mount_for_volume;
  GList    *volumes_for_drive;
  GList    *volume_for_drive;

  current_location_on_volume = FALSE;

  if (sidebar->current_location != NULL)
    {
      if (mount != NULL)
        {
          mount_default_location = g_mount_get_default_location (mount);
          current_location_on_volume = file_prefix_or_same (sidebar->current_location,
                                                            mount_default_location);

          g_object_unref (mount_default_location);
        }
      /* This code path is probably never reached since mount always exists,
       * and if it doesn't exists we don't offer a way to eject a volume or
       * drive in the UI. Do it for defensive programming
       */
      else if (volume != NULL)
        {
          mount_for_volume = g_volume_get_mount (volume);
          if (mount_for_volume != NULL)
            {
              mount_default_location = g_mount_get_default_location (mount_for_volume);
              current_location_on_volume = file_prefix_or_same (sidebar->current_location,
                                                                mount_default_location);

              g_object_unref (mount_default_location);
              g_object_unref (mount_for_volume);
            }
        }
      /* This code path is probably never reached since mount always exists,
       * and if it doesn't exists we don't offer a way to eject a volume or
       * drive in the UI. Do it for defensive programming
       */
      else if (drive != NULL)
        {
          volumes_for_drive = g_drive_get_volumes (drive);
          for (volume_for_drive = volumes_for_drive; volume_for_drive != NULL; volume_for_drive = volume_for_drive->next)
            {
              mount_for_volume = g_volume_get_mount (volume_for_drive->data);
              if (mount_for_volume != NULL)
                {
                  mount_default_location = g_mount_get_default_location (mount_for_volume);
                  current_location_on_volume = file_prefix_or_same (sidebar->current_location,
                                                                    mount_default_location);

                  g_object_unref (mount_default_location);
                  g_object_unref (mount_for_volume);

                  if (current_location_on_volume)
                    break;
                }
            }
          g_list_free_full (volumes_for_drive, g_object_unref);
        }
  }

  return current_location_on_volume;
}

static void
do_unmount (GMount           *mount,
            GtkPlacesSidebar *sidebar)
{
  if (mount != NULL)
    {
      GMountOperation *mount_op;

      if (is_current_location_on_volume (sidebar, mount, NULL, NULL))
        open_home (sidebar);

      mount_op = get_unmount_operation (sidebar);
      g_mount_unmount_with_operation (mount,
                                      0,
                                      mount_op,
                                      NULL,
                                      unmount_mount_cb,
                                      g_object_ref (sidebar));
      g_object_unref (mount_op);
    }
}

static void
unmount_shortcut_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GMount *mount;

  g_object_get (sidebar->context_row,
                "mount", &mount,
                NULL);

  do_unmount (mount, sidebar);

  if (mount)
    g_object_unref (mount);
}

static void
drive_stop_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = user_data;

  error = NULL;
  if (!g_drive_stop_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to stop “%s”"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
drive_eject_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = user_data;

  error = NULL;
  if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to eject “%s”"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
volume_eject_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = user_data;

  error = NULL;
  if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_volume_get_name (G_VOLUME (source_object));
          primary = g_strdup_printf (_("Unable to eject %s"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
mount_eject_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = user_data;

  error = NULL;
  if (!g_mount_eject_with_operation_finish (G_MOUNT (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_mount_get_name (G_MOUNT (source_object));
          primary = g_strdup_printf (_("Unable to eject %s"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
do_eject (GMount           *mount,
          GVolume          *volume,
          GDrive           *drive,
          GtkPlacesSidebar *sidebar)
{
  GMountOperation *mount_op;

  mount_op = get_unmount_operation (sidebar);

  if (is_current_location_on_volume (sidebar, mount, volume, drive))
    open_home (sidebar);

  if (mount != NULL)
    g_mount_eject_with_operation (mount, 0, mount_op, NULL, mount_eject_cb,
                                  g_object_ref (sidebar));
  /* This code path is probably never reached since mount always exists,
   * and if it doesn't exists we don't offer a way to eject a volume or
   * drive in the UI. Do it for defensive programming
   */
  else if (volume != NULL)
    g_volume_eject_with_operation (volume, 0, mount_op, NULL, volume_eject_cb,
                                   g_object_ref (sidebar));
  /* This code path is probably never reached since mount always exists,
   * and if it doesn't exists we don't offer a way to eject a volume or
   * drive in the UI. Do it for defensive programming
   */
  else if (drive != NULL)
    {
      if (g_drive_can_stop (drive))
        g_drive_stop (drive, 0, mount_op, NULL, drive_stop_cb,
                      g_object_ref (sidebar));
      else
        g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb,
                                      g_object_ref (sidebar));
    }
  g_object_unref (mount_op);
}

static void
eject_shortcut_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;

  g_object_get (sidebar->context_row,
                "mount", &mount,
                "volume", &volume,
                "drive", &drive,
                NULL);

  do_eject (mount, volume, drive, sidebar);

  if (mount)
    g_object_unref (mount);
  if (volume)
    g_object_unref (volume);
  if (drive)
    g_object_unref (drive);
}

static gboolean
eject_or_unmount_bookmark (GtkSidebarRow *row)
{
  gboolean can_unmount, can_eject;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;
  gboolean ret;
  GtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "mount", &mount,
                "volume", &volume,
                "drive", &drive,
                NULL);
  ret = FALSE;

  check_unmount_and_eject (mount, volume, drive, &can_unmount, &can_eject);
  /* if we can eject, it has priority over unmount */
  if (can_eject)
    {
      do_eject (mount, volume, drive, sidebar);
      ret = TRUE;
    }
  else if (can_unmount)
    {
      do_unmount (mount, sidebar);
      ret = TRUE;
    }

  g_object_unref (sidebar);
  if (mount)
    g_object_unref (mount);
  if (volume)
    g_object_unref (volume);
  if (drive)
    g_object_unref (drive);

  return ret;
}

static gboolean
eject_or_unmount_selection (GtkPlacesSidebar *sidebar)
{
  gboolean ret;
  GtkListBoxRow *row;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));
  ret = eject_or_unmount_bookmark (GTK_SIDEBAR_ROW (row));

  return ret;
}

static void
drive_poll_for_media_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = GTK_PLACES_SIDEBAR (user_data);

  error = NULL;
  if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to poll “%s” for media changes"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
rescan_shortcut_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GDrive *drive;

  g_object_get (sidebar->context_row,
                "drive", &drive,
                NULL);

  if (drive != NULL)
    {
      g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, g_object_ref (sidebar));
      g_object_unref (drive);
    }
}

static void
drive_start_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GtkPlacesSidebar *sidebar;
  GError *error;
  gchar *primary;
  gchar *name;

  sidebar = GTK_PLACES_SIDEBAR (user_data);

  error = NULL;
  if (!g_drive_start_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to start “%s”"), name);
          g_free (name);
          emit_show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
start_shortcut_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GDrive  *drive;

  g_object_get (sidebar->context_row,
                "drive", &drive,
                NULL);

  if (drive != NULL)
    {
      GMountOperation *mount_op;

      mount_op = get_mount_operation (sidebar);

      g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_cb, g_object_ref (sidebar));

      g_object_unref (mount_op);
      g_object_unref (drive);
    }
}

static void
stop_shortcut_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       data)
{
  GtkPlacesSidebar *sidebar = data;
  GDrive  *drive;

  g_object_get (sidebar->context_row,
                "drive", &drive,
                NULL);

  if (drive != NULL)
    {
      GMountOperation *mount_op;

      mount_op = get_unmount_operation (sidebar);
      g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
                    g_object_ref (sidebar));

      g_object_unref (mount_op);
      g_object_unref (drive);
    }
}

static gboolean
on_key_press_event (GtkWidget        *widget,
                    GdkEventKey      *event,
                    GtkPlacesSidebar *sidebar)
{
  guint modifiers;
  GtkListBoxRow *row;

  if (event)
    {
      row = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));
      if (row)
        {
          modifiers = gtk_accelerator_get_default_mod_mask ();

          if (event->keyval == GDK_KEY_Return ||
              event->keyval == GDK_KEY_KP_Enter ||
              event->keyval == GDK_KEY_ISO_Enter ||
              event->keyval == GDK_KEY_space)
            {
              GtkPlacesOpenFlags open_flags = GTK_PLACES_OPEN_NORMAL;

              if ((event->state & modifiers) == GDK_SHIFT_MASK)
                open_flags = GTK_PLACES_OPEN_NEW_TAB;
              else if ((event->state & modifiers) == GDK_CONTROL_MASK)
                open_flags = GTK_PLACES_OPEN_NEW_WINDOW;

              open_row (GTK_SIDEBAR_ROW (row), open_flags);

              return TRUE;
            }

          if (event->keyval == GDK_KEY_Down &&
              (event->state & modifiers) == GDK_MOD1_MASK)
            return eject_or_unmount_selection (sidebar);

          if ((event->keyval == GDK_KEY_Delete ||
               event->keyval == GDK_KEY_KP_Delete) &&
              (event->state & modifiers) == 0)
            {
              remove_bookmark (GTK_SIDEBAR_ROW (row));
              return TRUE;
            }

          if ((event->keyval == GDK_KEY_F2) &&
              (event->state & modifiers) == 0)
            {
              rename_bookmark (GTK_SIDEBAR_ROW (row));
              return TRUE;
            }

          if ((event->keyval == GDK_KEY_Menu) ||
              ((event->keyval == GDK_KEY_F10) &&
               (event->state & modifiers) == GDK_SHIFT_MASK))

            {
              popup_menu_cb (GTK_SIDEBAR_ROW (row));
              return TRUE;
            }
        }
    }

  return FALSE;
}

static GActionEntry entries[] = {
  { "open", open_shortcut_cb, "i", NULL, NULL },
  { "open-other", open_shortcut_cb, "i", NULL, NULL },
  { "bookmark", add_shortcut_cb, NULL, NULL, NULL },
  { "remove", remove_shortcut_cb, NULL, NULL, NULL },
  { "rename", rename_shortcut_cb, NULL, NULL, NULL },
  { "mount", mount_shortcut_cb, NULL, NULL, NULL },
  { "unmount", unmount_shortcut_cb, NULL, NULL, NULL },
  { "eject", eject_shortcut_cb, NULL, NULL, NULL },
  { "rescan", rescan_shortcut_cb, NULL, NULL, NULL },
  { "start", start_shortcut_cb, NULL, NULL, NULL },
  { "stop", stop_shortcut_cb, NULL, NULL, NULL },
};

static void
add_actions (GtkPlacesSidebar *sidebar)
{
  GActionGroup *actions;

  actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries, G_N_ELEMENTS (entries),
                                   sidebar);
  gtk_widget_insert_action_group (GTK_WIDGET (sidebar), "row", actions);
  g_object_unref (actions);
}

static GtkWidget *
append_separator (GtkWidget *box)
{
  GtkWidget *separator;

  separator = g_object_new (GTK_TYPE_SEPARATOR,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "visible", TRUE,
                            "margin-top", 6,
                            "margin-bottom", 6,
                            NULL);
  gtk_container_add (GTK_CONTAINER (box), separator);

  return separator;
}

static GtkWidget *
add_button (GtkWidget   *box,
            const gchar *label,
            const gchar *action)
{
  GtkWidget *item;

  item = g_object_new (GTK_TYPE_MODEL_BUTTON,
                       "visible", TRUE,
                       "action-name", action,
                       "text", label,
                       NULL);
  gtk_container_add (GTK_CONTAINER (box), item);

  return item;
}

static GtkWidget *
add_open_button (GtkWidget          *box,
                 const gchar        *label,
                 GtkPlacesOpenFlags  flags)
{
  GtkWidget *item;

  item = g_object_new (GTK_TYPE_MODEL_BUTTON,
                       "visible", TRUE,
                       "action-name", flags == GTK_PLACES_OPEN_NORMAL ? "row.open" : "row.open-other",
                       "action-target", g_variant_new_int32 (flags),
                       "text", label,
                       NULL);
  gtk_container_add (GTK_CONTAINER (box), item);

  return item;
}

static void
on_row_popover_destroy (GtkWidget        *row_popover,
                        GtkPlacesSidebar *sidebar)
{
  if (sidebar)
    sidebar->popover = NULL;
}

#ifdef HAVE_CLOUDPROVIDERS
static void
build_popup_menu_using_gmenu (GtkSidebarRow *row)
{
  CloudProvidersAccount *cloud_provider_account;
  GtkPlacesSidebar *sidebar;
  GMenuModel *cloud_provider_menu;
  GActionGroup *cloud_provider_action_group;

  g_object_get (row,
                "sidebar", &sidebar,
                "cloud-provider-account", &cloud_provider_account,
                NULL);

  /* Cloud provider account */
  if (cloud_provider_account)
    {
      GMenu *menu = g_menu_new ();
      GMenuItem *item;
      item = g_menu_item_new (_("_Open"), "row.open");
      g_menu_item_set_action_and_target_value (item, "row.open", g_variant_new_int32(GTK_PLACES_OPEN_NORMAL));
      g_menu_append_item (menu, item);
      if (sidebar->open_flags & GTK_PLACES_OPEN_NEW_TAB)
        {
          item = g_menu_item_new (_("Open in New _Tab"), "row.open-other");
          g_menu_item_set_action_and_target_value (item, "row.open-other", g_variant_new_int32(GTK_PLACES_OPEN_NEW_TAB));
          g_menu_append_item (menu, item);
        }
      if (sidebar->open_flags & GTK_PLACES_OPEN_NEW_WINDOW)
        {
          item = g_menu_item_new (_("Open in New _Window"), "row.open-other");
          g_menu_item_set_action_and_target_value (item, "row.open-other", g_variant_new_int32(GTK_PLACES_OPEN_NEW_WINDOW));
          g_menu_append_item (menu, item);
        }
      cloud_provider_menu = cloud_providers_account_get_menu_model (cloud_provider_account);
      cloud_provider_action_group = cloud_providers_account_get_action_group (cloud_provider_account);
      if (cloud_provider_menu != NULL && cloud_provider_action_group != NULL)
        {
          g_menu_append_section (menu, NULL, cloud_provider_menu);
          gtk_widget_insert_action_group (GTK_WIDGET (sidebar),
                                          "cloudprovider",
                                          G_ACTION_GROUP (cloud_provider_action_group));
        }
      add_actions (sidebar);
      if (sidebar->popover)
        gtk_widget_destroy (sidebar->popover);

      sidebar->popover = gtk_popover_new_from_model (GTK_WIDGET (sidebar),
                                                     G_MENU_MODEL (menu));
      g_signal_connect (sidebar->popover, "destroy",
                        G_CALLBACK (on_row_popover_destroy), sidebar);
      g_object_unref (sidebar);
      g_object_unref (cloud_provider_account);
    }
}
#endif

/* Constructs the popover for the sidebar row if needed */
static void
create_row_popover (GtkPlacesSidebar *sidebar,
                    GtkSidebarRow    *row)
{
  PopoverData data;
  GtkWidget *box;

#ifdef HAVE_CLOUDPROVIDERS
  CloudProvidersAccount *cloud_provider_account;

  g_object_get (row, "cloud-provider-account", &cloud_provider_account, NULL);

  if (cloud_provider_account)
    {
      build_popup_menu_using_gmenu (row);
      return;
    }
#endif

  sidebar->popover = gtk_popover_new (GTK_WIDGET (sidebar));
  /* Clean sidebar pointer when its destroyed, most of the times due to its
   * relative_to associated row being destroyed */
  g_signal_connect (sidebar->popover, "destroy", G_CALLBACK (on_row_popover_destroy), sidebar);
  setup_popover_shadowing (sidebar->popover);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_set (box, "margin", 10, NULL);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (sidebar->popover), box);

  add_open_button (box, _("_Open"), GTK_PLACES_OPEN_NORMAL);

  if (sidebar->open_flags & GTK_PLACES_OPEN_NEW_TAB)
    add_open_button (box, _("Open in New _Tab"), GTK_PLACES_OPEN_NEW_TAB);

  if (sidebar->open_flags & GTK_PLACES_OPEN_NEW_WINDOW)
    add_open_button (box, _("Open in New _Window"), GTK_PLACES_OPEN_NEW_WINDOW);

  append_separator (box);

  data.add_shortcut_item = add_button (box, _("_Add Bookmark"), "row.bookmark");
  data.remove_item = add_button (box, _("_Remove"), "row.remove");
  data.rename_item = add_button (box, _("Rename…"), "row.rename");

  data.separator_item = append_separator (box);

  data.mount_item = add_button (box, _("_Mount"), "row.mount");
  data.unmount_item = add_button (box, _("_Unmount"), "row.unmount");
  data.eject_item = add_button (box, _("_Eject"), "row.eject");
  data.rescan_item = add_button (box, _("_Detect Media"), "row.rescan");
  data.start_item = add_button (box, _("_Start"), "row.start");
  data.stop_item = add_button (box, _("_Stop"), "row.stop");

  /* Update everything! */
  check_popover_sensitivity (row, &data);

  if (sidebar->populate_all)
    {
      gchar *uri;
      GVolume *volume;
      GFile *file;

      g_object_get (row,
                    "uri", &uri,
                    "volume", &volume,
                    NULL);

      if (uri)
        file = g_file_new_for_uri (uri);
      else
        file = NULL;

      g_signal_emit (sidebar, places_sidebar_signals[POPULATE_POPUP], 0,
                     box, file, volume);

      if (file)
        g_object_unref (file);

      g_free (uri);
      if (volume)
        g_object_unref (volume);
    }
}

static void
show_row_popover (GtkSidebarRow *row)
{
  GtkPlacesSidebar *sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  if (sidebar->popover)
    gtk_widget_destroy (sidebar->popover);

  create_row_popover (sidebar, row);

  gtk_popover_set_relative_to (GTK_POPOVER (sidebar->popover), GTK_WIDGET (row));

  sidebar->context_row = row;
  gtk_popover_popup (GTK_POPOVER (sidebar->popover));

  g_object_unref (sidebar);
}

static void
on_row_activated (GtkListBox    *list_box,
                  GtkListBoxRow *row,
                  gpointer       user_data)
{
  GtkSidebarRow *selected_row;

  /* Avoid to open a location if the user is dragging. Changing the location
   * while dragging usually makes clients changing the view of the files, which
   * is confusing while the user has the attention on the drag
   */
  if (GTK_PLACES_SIDEBAR (user_data)->dragging_over)
    return;

  selected_row = GTK_SIDEBAR_ROW (gtk_list_box_get_selected_row (list_box));
  open_row (selected_row, 0);
}

static gboolean
on_button_press_event (GtkWidget      *widget,
                       GdkEventButton *event,
                       GtkSidebarRow  *row)
{
  GtkPlacesSidebar *sidebar;
  GtkPlacesSidebarSectionType section_type;

  g_object_get (GTK_SIDEBAR_ROW (row),
                "sidebar", &sidebar,
                "section_type", &section_type,
                NULL);

  if (section_type == SECTION_BOOKMARKS)
    {
      sidebar->drag_row = GTK_WIDGET (row);
      sidebar->drag_row_x = (gint)event->x;
      sidebar->drag_row_y = (gint)event->y;

      sidebar->drag_root_x = event->x_root;
      sidebar->drag_root_y = event->y_root;
    }

  g_object_unref (sidebar);

  return FALSE;
}

static gboolean
on_button_release_event (GtkWidget      *widget,
                         GdkEventButton *event,
                         GtkSidebarRow  *row)
{
  gboolean ret = FALSE;
  GtkPlacesSidebarPlaceType row_type;

  if (event && row)
    {
      g_object_get (row, "place-type", &row_type, NULL);

      if (event->button == 1)
        ret = FALSE;
      else if (event->button == 2)
        {
          GtkPlacesOpenFlags open_flags = GTK_PLACES_OPEN_NORMAL;

          open_flags = (event->state & GDK_CONTROL_MASK) ?
                        GTK_PLACES_OPEN_NEW_WINDOW :
                        GTK_PLACES_OPEN_NEW_TAB;

          open_row (GTK_SIDEBAR_ROW (row), open_flags);
          ret = TRUE;
        }
      else if (event->button == 3)
        {
          if (row_type != PLACES_CONNECT_TO_SERVER)
            show_row_popover (GTK_SIDEBAR_ROW (row));
        }
    }

  return ret;
}

static void
popup_menu_cb (GtkSidebarRow *row)
{
  GtkPlacesSidebarPlaceType row_type;

  g_object_get (row, "place-type", &row_type, NULL);

  if (row_type != PLACES_CONNECT_TO_SERVER)
    show_row_popover (row);
}

static void
long_press_cb (GtkGesture       *gesture,
               gdouble           x,
               gdouble           y,
               GtkPlacesSidebar *sidebar)
{
  GtkWidget *row;

  row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (sidebar->list_box), y));
  if (GTK_IS_SIDEBAR_ROW (row))
    popup_menu_cb (GTK_SIDEBAR_ROW (row));
}

static gint
list_box_sort_func (GtkListBoxRow *row1,
                    GtkListBoxRow *row2,
                    gpointer       user_data)
{
  GtkPlacesSidebarSectionType section_type_1, section_type_2;
  GtkPlacesSidebarPlaceType place_type_1, place_type_2;
  gchar *label_1, *label_2;
  gint index_1, index_2;
  gint retval = 0;

  g_object_get (row1,
                "label", &label_1,
                "place-type", &place_type_1,
                "section-type", &section_type_1,
                "order-index", &index_1,
                NULL);
  g_object_get (row2,
                "label", &label_2,
                "place-type", &place_type_2,
                "section-type", &section_type_2,
                "order-index", &index_2,
                NULL);

  /* Always last position for "connect to server" */
  if (place_type_1 == PLACES_CONNECT_TO_SERVER)
    {
      retval = 1;
    }
  else if (place_type_2 == PLACES_CONNECT_TO_SERVER)
    {
      retval = -1;
    }
  else
    {
      if (section_type_1 == section_type_2)
        {
          if ((section_type_1 == SECTION_COMPUTER &&
               place_type_1 == place_type_2 &&
               place_type_1 == PLACES_XDG_DIR) ||
              section_type_1 == SECTION_MOUNTS)
            {
              retval = g_utf8_collate (label_1, label_2);
            }
          else if ((place_type_1 == PLACES_BOOKMARK || place_type_2 == PLACES_DROP_FEEDBACK) &&
                   (place_type_1 == PLACES_DROP_FEEDBACK || place_type_2 == PLACES_BOOKMARK))
            {
              retval = index_1 - index_2;
            }
          /* We order the bookmarks sections based on the bookmark index that we
           * set on the row as a order-index property, but we have to deal with
           * the placeholder row wanted to be between two consecutive bookmarks,
           * with two consecutive order-index values which is the usual case.
           * For that, in the list box sort func we give priority to the placeholder row,
           * that means that if the index-order is the same as another bookmark
           * the placeholder row goes before. However if we want to show it after
           * the current row, for instance when the cursor is in the lower half
           * of the row, we need to increase the order-index.
           */
          else if (place_type_1 == PLACES_BOOKMARK_PLACEHOLDER && place_type_2 == PLACES_BOOKMARK)
            {
              if (index_1 == index_2)
                retval =  index_1 - index_2 - 1;
              else
                retval = index_1 - index_2;
            }
          else if (place_type_1 == PLACES_BOOKMARK && place_type_2 == PLACES_BOOKMARK_PLACEHOLDER)
            {
              if (index_1 == index_2)
                retval =  index_1 - index_2 + 1;
              else
                retval = index_1 - index_2;
            }
        }
      else
        {
          /* Order by section. That means the order in the enum of section types
           * define the actual order of them in the list */
          retval = section_type_1 - section_type_2;
        }
    }

  g_free (label_1);
  g_free (label_2);

  return retval;
}

static void
update_hostname (GtkPlacesSidebar *sidebar)
{
  GVariant *variant;
  gsize len;
  const gchar *hostname;

  if (sidebar->hostnamed_proxy == NULL)
    return;

  variant = g_dbus_proxy_get_cached_property (sidebar->hostnamed_proxy,
                                              "PrettyHostname");
  if (variant == NULL)
    return;

  hostname = g_variant_get_string (variant, &len);
  if (len > 0 &&
      g_strcmp0 (sidebar->hostname, hostname) != 0)
    {
      g_free (sidebar->hostname);
      sidebar->hostname = g_strdup (hostname);
      update_places (sidebar);
    }

  g_variant_unref (variant);
}

static void
hostname_proxy_new_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkPlacesSidebar *sidebar = user_data;
  GError *error = NULL;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  sidebar->hostnamed_proxy = proxy;
  g_clear_object (&sidebar->hostnamed_cancellable);

  if (error != NULL)
    {
      g_debug ("Failed to create D-Bus proxy: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect_swapped (sidebar->hostnamed_proxy,
                            "g-properties-changed",
                            G_CALLBACK (update_hostname),
                            sidebar);
  update_hostname (sidebar);
}

static void
create_volume_monitor (GtkPlacesSidebar *sidebar)
{
  g_assert (sidebar->volume_monitor == NULL);

  sidebar->volume_monitor = g_volume_monitor_get ();

  g_signal_connect_object (sidebar->volume_monitor, "volume_added",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "mount_added",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
}

static void
shell_shows_desktop_changed (GtkSettings *settings,
                             GParamSpec  *pspec,
                             gpointer     user_data)
{
  GtkPlacesSidebar *sidebar = user_data;
  gboolean show_desktop;

  g_assert (settings == sidebar->gtk_settings);

  /* Check if the user explicitly set this and, if so, don't change it. */
  if (sidebar->show_desktop_set)
    return;

  g_object_get (settings, "gtk-shell-shows-desktop", &show_desktop, NULL);

  if (show_desktop != sidebar->show_desktop)
    {
      sidebar->show_desktop = show_desktop;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_DESKTOP]);
    }
}

static void
gtk_places_sidebar_init (GtkPlacesSidebar *sidebar)
{
  GtkTargetList *target_list;
  gboolean show_desktop;
  GtkStyleContext *context;

  sidebar->cancellable = g_cancellable_new ();

  sidebar->show_trash = TRUE;

  create_volume_monitor (sidebar);

  sidebar->open_flags = GTK_PLACES_OPEN_NORMAL;

  sidebar->bookmarks_manager = _gtk_bookmarks_manager_new ((GtkBookmarksChangedFunc)update_places, sidebar);

  sidebar->trash_monitor = _gtk_trash_monitor_get ();
  sidebar->trash_monitor_changed_id = g_signal_connect_swapped (sidebar->trash_monitor, "trash-state-changed",
                                                                G_CALLBACK (update_trash_icon), sidebar);

  gtk_widget_set_size_request (GTK_WIDGET (sidebar), 140, 280);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);

  context = gtk_widget_get_style_context (GTK_WIDGET (sidebar));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SIDEBAR);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_RIGHT | GTK_JUNCTION_LEFT);

  /* list box */
  sidebar->list_box = gtk_list_box_new ();

  gtk_list_box_set_header_func (GTK_LIST_BOX (sidebar->list_box),
                                list_box_header_func, sidebar, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (sidebar->list_box),
                              list_box_sort_func, NULL, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (sidebar->list_box), GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (sidebar->list_box), TRUE);

  g_signal_connect (sidebar->list_box, "row-activated",
                    G_CALLBACK (on_row_activated), sidebar);
  g_signal_connect (sidebar->list_box, "key-press-event",
                    G_CALLBACK (on_key_press_event), sidebar);

  sidebar->long_press_gesture = gtk_gesture_long_press_new (GTK_WIDGET (sidebar));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (sidebar->long_press_gesture), TRUE);
  g_signal_connect (sidebar->long_press_gesture, "pressed",
                    G_CALLBACK (long_press_cb), sidebar);

  /* DND support */
  gtk_drag_dest_set (sidebar->list_box,
                     0,
                     NULL, 0,
                     GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
  target_list = gtk_target_list_new  (dnd_drop_targets, G_N_ELEMENTS (dnd_drop_targets));
  gtk_target_list_add_uri_targets (target_list, DND_TEXT_URI_LIST);
  gtk_drag_dest_set_target_list (sidebar->list_box, target_list);
  gtk_target_list_unref (target_list);
  sidebar->source_targets = gtk_target_list_new (dnd_source_targets, G_N_ELEMENTS (dnd_source_targets));
  gtk_target_list_add_text_targets (sidebar->source_targets, 0);

  g_signal_connect (sidebar->list_box, "motion-notify-event",
                    G_CALLBACK (on_motion_notify_event), sidebar);
  g_signal_connect (sidebar->list_box, "drag-begin",
                    G_CALLBACK (drag_begin_callback), sidebar);
  g_signal_connect (sidebar->list_box, "drag-motion",
                    G_CALLBACK (drag_motion_callback), sidebar);
  g_signal_connect (sidebar->list_box, "drag-data-get",
                    G_CALLBACK (drag_data_get_callback), sidebar);
  g_signal_connect (sidebar->list_box, "drag-data-received",
                    G_CALLBACK (drag_data_received_callback), sidebar);
  g_signal_connect (sidebar->list_box, "drag-drop",
                    G_CALLBACK (drag_drop_callback), sidebar);
  g_signal_connect (sidebar->list_box, "drag-end",
                    G_CALLBACK (drag_end_callback), sidebar);
  g_signal_connect (sidebar->list_box, "drag-leave",
                    G_CALLBACK (drag_leave_callback), sidebar);
  sidebar->drag_row = NULL;
  sidebar->row_placeholder = NULL;
  sidebar->dragging_over = FALSE;
  sidebar->drag_data_info = DND_UNKNOWN;

  gtk_container_add (GTK_CONTAINER (sidebar), sidebar->list_box);

  sidebar->hostname = g_strdup (_("Computer"));
  sidebar->hostnamed_cancellable = g_cancellable_new ();
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                            NULL,
                            "org.freedesktop.hostname1",
                            "/org/freedesktop/hostname1",
                            "org.freedesktop.hostname1",
                            sidebar->hostnamed_cancellable,
                            hostname_proxy_new_cb,
                            sidebar);

  sidebar->drop_state = DROP_STATE_NORMAL;

  /* Don't bother trying to trace this across hierarchy changes... */
  sidebar->gtk_settings = gtk_settings_get_default ();
  g_signal_connect (sidebar->gtk_settings, "notify::gtk-shell-shows-desktop",
                    G_CALLBACK (shell_shows_desktop_changed), sidebar);
  g_object_get (sidebar->gtk_settings, "gtk-shell-shows-desktop", &show_desktop, NULL);
  sidebar->show_desktop = show_desktop;

  /* Cloud providers */
#ifdef HAVE_CLOUDPROVIDERS
  sidebar->cloud_manager = cloud_providers_collector_dup_singleton ();
  g_signal_connect_swapped (sidebar->cloud_manager,
                            "providers-changed",
                            G_CALLBACK (update_places),
                            sidebar);
#endif

  /* populate the sidebar */
  update_places (sidebar);

  add_actions (sidebar);
}

static void
gtk_places_sidebar_set_property (GObject      *obj,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (obj);

  switch (property_id)
    {
    case PROP_LOCATION:
      gtk_places_sidebar_set_location (sidebar, g_value_get_object (value));
      break;

    case PROP_OPEN_FLAGS:
      gtk_places_sidebar_set_open_flags (sidebar, g_value_get_flags (value));
      break;

    case PROP_SHOW_RECENT:
      gtk_places_sidebar_set_show_recent (sidebar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_DESKTOP:
      gtk_places_sidebar_set_show_desktop (sidebar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_CONNECT_TO_SERVER:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_places_sidebar_set_show_connect_to_server (sidebar, g_value_get_boolean (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_SHOW_ENTER_LOCATION:
      gtk_places_sidebar_set_show_enter_location (sidebar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_OTHER_LOCATIONS:
      gtk_places_sidebar_set_show_other_locations (sidebar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_TRASH:
      gtk_places_sidebar_set_show_trash (sidebar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_STARRED_LOCATION:
      gtk_places_sidebar_set_show_starred_location (sidebar, g_value_get_boolean (value));
      break;

    case PROP_LOCAL_ONLY:
      gtk_places_sidebar_set_local_only (sidebar, g_value_get_boolean (value));
      break;

    case PROP_POPULATE_ALL:
      if (sidebar->populate_all != g_value_get_boolean (value))
        {
          sidebar->populate_all = g_value_get_boolean (value);
          g_object_notify_by_pspec (obj, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_places_sidebar_get_property (GObject    *obj,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (obj);

  switch (property_id)
    {
    case PROP_LOCATION:
      g_value_take_object (value, gtk_places_sidebar_get_location (sidebar));
      break;

    case PROP_OPEN_FLAGS:
      g_value_set_flags (value, gtk_places_sidebar_get_open_flags (sidebar));
      break;

    case PROP_SHOW_RECENT:
      g_value_set_boolean (value, gtk_places_sidebar_get_show_recent (sidebar));
      break;

    case PROP_SHOW_DESKTOP:
      g_value_set_boolean (value, gtk_places_sidebar_get_show_desktop (sidebar));
      break;

    case PROP_SHOW_CONNECT_TO_SERVER:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_boolean (value, gtk_places_sidebar_get_show_connect_to_server (sidebar));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    case PROP_SHOW_ENTER_LOCATION:
      g_value_set_boolean (value, gtk_places_sidebar_get_show_enter_location (sidebar));
      break;

    case PROP_SHOW_OTHER_LOCATIONS:
      g_value_set_boolean (value, gtk_places_sidebar_get_show_other_locations (sidebar));
      break;

    case PROP_SHOW_TRASH:
      g_value_set_boolean (value, gtk_places_sidebar_get_show_trash (sidebar));
      break;

    case PROP_SHOW_STARRED_LOCATION:
      g_value_set_boolean (value, gtk_places_sidebar_get_show_starred_location (sidebar));
      break;

    case PROP_LOCAL_ONLY:
      g_value_set_boolean (value, gtk_places_sidebar_get_local_only (sidebar));
      break;

    case PROP_POPULATE_ALL:
      g_value_set_boolean (value, sidebar->populate_all);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_places_sidebar_dispose (GObject *object)
{
  GtkPlacesSidebar *sidebar;
#ifdef HAVE_CLOUDPROVIDERS
  GList *l;
#endif

  sidebar = GTK_PLACES_SIDEBAR (object);

  if (sidebar->cancellable)
    {
      g_cancellable_cancel (sidebar->cancellable);
      g_object_unref (sidebar->cancellable);
      sidebar->cancellable = NULL;
    }

  free_drag_data (sidebar);

  if (sidebar->bookmarks_manager != NULL)
    {
      _gtk_bookmarks_manager_free (sidebar->bookmarks_manager);
      sidebar->bookmarks_manager = NULL;
    }

  if (sidebar->popover)
    {
      gtk_widget_destroy (sidebar->popover);
      sidebar->popover = NULL;
    }

  if (sidebar->rename_popover)
    {
      gtk_widget_destroy (sidebar->rename_popover);
      sidebar->rename_popover = NULL;
      sidebar->rename_entry = NULL;
      sidebar->rename_button = NULL;
      sidebar->rename_error = NULL;
    }

  if (sidebar->trash_monitor)
    {
      g_signal_handler_disconnect (sidebar->trash_monitor, sidebar->trash_monitor_changed_id);
      sidebar->trash_monitor_changed_id = 0;
      g_clear_object (&sidebar->trash_monitor);
    }

  if (sidebar->trash_row)
    {
      g_object_remove_weak_pointer (G_OBJECT (sidebar->trash_row),
                                    (gpointer *) &sidebar->trash_row);
      sidebar->trash_row = NULL;
    }

  if (sidebar->volume_monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
                                            update_places, sidebar);
      g_clear_object (&sidebar->volume_monitor);
    }

  if (sidebar->hostnamed_cancellable != NULL)
    {
      g_cancellable_cancel (sidebar->hostnamed_cancellable);
      g_clear_object (&sidebar->hostnamed_cancellable);
    }

  g_clear_object (&sidebar->hostnamed_proxy);
  g_free (sidebar->hostname);
  sidebar->hostname = NULL;

  if (sidebar->gtk_settings)
    {
      g_signal_handlers_disconnect_by_func (sidebar->gtk_settings, shell_shows_desktop_changed, sidebar);
      sidebar->gtk_settings = NULL;
    }

  g_clear_object (&sidebar->current_location);
  g_clear_pointer (&sidebar->rename_uri, g_free);

  g_clear_object (&sidebar->long_press_gesture);

  if (sidebar->source_targets)
    {
      gtk_target_list_unref (sidebar->source_targets);
      sidebar->source_targets = NULL;
    }

  g_slist_free_full (sidebar->shortcuts, g_object_unref);
  sidebar->shortcuts = NULL;

#ifdef HAVE_CLOUDPROVIDERS
  for (l = sidebar->unready_accounts; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (l->data, sidebar);
    }
  g_list_free_full (sidebar->unready_accounts, g_object_unref);
  sidebar->unready_accounts = NULL;

  if (sidebar->cloud_manager)
    {
      g_signal_handlers_disconnect_by_data (sidebar->cloud_manager, sidebar);
      for (l = cloud_providers_collector_get_providers (sidebar->cloud_manager);
           l != NULL; l = l->next)
        {
          g_signal_handlers_disconnect_by_data (l->data, sidebar);
        }
      g_object_unref (sidebar->cloud_manager);
      sidebar->cloud_manager = NULL;
    }
#endif

  G_OBJECT_CLASS (gtk_places_sidebar_parent_class)->dispose (object);
}

static void
gtk_places_sidebar_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_places_sidebar_parent_class)->finalize (object);
}

static void
gtk_places_sidebar_class_init (GtkPlacesSidebarClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);


  gobject_class->dispose = gtk_places_sidebar_dispose;
  gobject_class->finalize = gtk_places_sidebar_finalize;
  gobject_class->set_property = gtk_places_sidebar_set_property;
  gobject_class->get_property = gtk_places_sidebar_get_property;

  /**
   * GtkPlacesSidebar::open-location:
   * @sidebar: the object which received the signal.
   * @location: (type Gio.File): #GFile to which the caller should switch.
   * @open_flags: a single value from #GtkPlacesOpenFlags specifying how the @location should be opened.
   *
   * The places sidebar emits this signal when the user selects a location
   * in it.  The calling application should display the contents of that
   * location; for example, a file manager should show a list of files in
   * the specified location.
   *
   * Since: 3.10
   */
  places_sidebar_signals [OPEN_LOCATION] =
          g_signal_new (I_("open-location"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, open_location),
                        NULL, NULL,
                        _gtk_marshal_VOID__OBJECT_FLAGS,
                        G_TYPE_NONE, 2,
                        G_TYPE_OBJECT,
                        GTK_TYPE_PLACES_OPEN_FLAGS);

  /**
   * GtkPlacesSidebar::populate-popup:
   * @sidebar: the object which received the signal.
   * @container: (type Gtk.Widget): a #GtkMenu or another #GtkContainer
   * @selected_item: (type Gio.File) (nullable): #GFile with the item to which
   *     the popup should refer, or %NULL in the case of a @selected_volume.
   * @selected_volume: (type Gio.Volume) (nullable): #GVolume if the selected
   *     item is a volume, or %NULL if it is a file.
   *
   * The places sidebar emits this signal when the user invokes a contextual
   * popup on one of its items. In the signal handler, the application may
   * add extra items to the menu as appropriate. For example, a file manager
   * may want to add a "Properties" command to the menu.
   *
   * It is not necessary to store the @selected_item for each menu item;
   * during their callbacks, the application can use gtk_places_sidebar_get_location()
   * to get the file to which the item refers.
   *
   * The @selected_item argument may be %NULL in case the selection refers to
   * a volume. In this case, @selected_volume will be non-%NULL. In this case,
   * the calling application will have to g_object_ref() the @selected_volume and
   * keep it around to use it in the callback.
   *
   * The @container and all its contents are destroyed after the user
   * dismisses the popup. The popup is re-created (and thus, this signal is
   * emitted) every time the user activates the contextual menu.
   *
   * Before 3.18, the @container always was a #GtkMenu, and you were expected
   * to add your items as #GtkMenuItems. Since 3.18, the popup may be implemented
   * as a #GtkPopover, in which case @container will be something else, e.g. a
   * #GtkBox, to which you may add #GtkModelButtons or other widgets, such as
   * #GtkEntries, #GtkSpinButtons, etc. If your application can deal with this
   * situation, you can set #GtkPlacesSidebar::populate-all to %TRUE to request
   * that this signal is emitted for populating popovers as well.
   *
   * Since: 3.10
   */
  places_sidebar_signals [POPULATE_POPUP] =
          g_signal_new (I_("populate-popup"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, populate_popup),
                        NULL, NULL,
                        _gtk_marshal_VOID__OBJECT_OBJECT_OBJECT,
                        G_TYPE_NONE, 3,
                        GTK_TYPE_WIDGET,
                        G_TYPE_FILE,
                        G_TYPE_VOLUME);

  /**
   * GtkPlacesSidebar::show-error-message:
   * @sidebar: the object which received the signal.
   * @primary: primary message with a summary of the error to show.
   * @secondary: secondary message with details of the error to show.
   *
   * The places sidebar emits this signal when it needs the calling
   * application to present an error message.  Most of these messages
   * refer to mounting or unmounting media, for example, when a drive
   * cannot be started for some reason.
   *
   * Since: 3.10
   */
  places_sidebar_signals [SHOW_ERROR_MESSAGE] =
          g_signal_new (I_("show-error-message"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, show_error_message),
                        NULL, NULL,
                        _gtk_marshal_VOID__STRING_STRING,
                        G_TYPE_NONE, 2,
                        G_TYPE_STRING,
                        G_TYPE_STRING);

  /**
   * GtkPlacesSidebar::show-connect-to-server:
   * @sidebar: the object which received the signal.
   *
   * The places sidebar emits this signal when it needs the calling
   * application to present an way to connect directly to a network server.
   * For example, the application may bring up a dialog box asking for
   * a URL like "sftp://ftp.example.com".  It is up to the application to create
   * the corresponding mount by using, for example, g_file_mount_enclosing_volume().
   *
   * Deprecated: 3.18: use the #GtkPlacesSidebar::show-other-locations signal
   *     to connect to network servers.
   */
  places_sidebar_signals [SHOW_CONNECT_TO_SERVER] =
          g_signal_new (I_("show-connect-to-server"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, show_connect_to_server),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE, 0);

  /**
   * GtkPlacesSidebar::show-enter-location:
   * @sidebar: the object which received the signal.
   *
   * The places sidebar emits this signal when it needs the calling
   * application to present an way to directly enter a location.
   * For example, the application may bring up a dialog box asking for
   * a URL like "http://http.example.com".
   *
   * Since: 3.14
   */
  places_sidebar_signals [SHOW_ENTER_LOCATION] =
          g_signal_new (I_("show-enter-location"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, show_enter_location),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE, 0);

  /**
   * GtkPlacesSidebar::drag-action-requested:
   * @sidebar: the object which received the signal.
   * @context: (type Gdk.DragContext): #GdkDragContext with information about the drag operation
   * @dest_file: (type Gio.File): #GFile with the tentative location that is being hovered for a drop
   * @source_file_list: (type GLib.List) (element-type GFile) (transfer none):
	 *   List of #GFile that are being dragged
   *
   * When the user starts a drag-and-drop operation and the sidebar needs
   * to ask the application for which drag action to perform, then the
   * sidebar will emit this signal.
   *
   * The application can evaluate the @context for customary actions, or
   * it can check the type of the files indicated by @source_file_list against the
   * possible actions for the destination @dest_file.
   *
   * The drag action to use must be the return value of the signal handler.
   *
   * Returns: The drag action to use, for example, #GDK_ACTION_COPY
   * or #GDK_ACTION_MOVE, or 0 if no action is allowed here (i.e. drops
   * are not allowed in the specified @dest_file).
   *
   * Since: 3.10
   */
  places_sidebar_signals [DRAG_ACTION_REQUESTED] =
          g_signal_new (I_("drag-action-requested"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, drag_action_requested),
                        NULL, NULL,
                        _gtk_marshal_INT__OBJECT_OBJECT_POINTER,
                        G_TYPE_INT, 3,
                        GDK_TYPE_DRAG_CONTEXT,
                        G_TYPE_OBJECT,
                        G_TYPE_POINTER /* GList of GFile */ );

  /**
   * GtkPlacesSidebar::drag-action-ask:
   * @sidebar: the object which received the signal.
   * @actions: Possible drag actions that need to be asked for.
   *
   * The places sidebar emits this signal when it needs to ask the application
   * to pop up a menu to ask the user for which drag action to perform.
   *
   * Returns: the final drag action that the sidebar should pass to the drag side
   * of the drag-and-drop operation.
   *
   * Since: 3.10
   */
  places_sidebar_signals [DRAG_ACTION_ASK] =
          g_signal_new (I_("drag-action-ask"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, drag_action_ask),
                        NULL, NULL,
                        _gtk_marshal_INT__INT,
                        G_TYPE_INT, 1,
                        G_TYPE_INT);

  /**
   * GtkPlacesSidebar::drag-perform-drop:
   * @sidebar: the object which received the signal.
   * @dest_file: (type Gio.File): Destination #GFile.
   * @source_file_list: (type GLib.List) (element-type GFile) (transfer none):
   *   #GList of #GFile that got dropped.
   * @action: Drop action to perform.
   *
   * The places sidebar emits this signal when the user completes a
   * drag-and-drop operation and one of the sidebar's items is the
   * destination.  This item is in the @dest_file, and the
   * @source_file_list has the list of files that are dropped into it and
   * which should be copied/moved/etc. based on the specified @action.
   *
   * Since: 3.10
   */
  places_sidebar_signals [DRAG_PERFORM_DROP] =
          g_signal_new (I_("drag-perform-drop"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, drag_perform_drop),
                        NULL, NULL,
                        _gtk_marshal_VOID__OBJECT_POINTER_INT,
                        G_TYPE_NONE, 3,
                        G_TYPE_OBJECT,
                        G_TYPE_POINTER, /* GList of GFile */
                        G_TYPE_INT);

  /**
   * GtkPlacesSidebar::show-other-locations:
   * @sidebar: the object which received the signal.
   *
   * The places sidebar emits this signal when it needs the calling
   * application to present a way to show other locations e.g. drives
   * and network access points.
   * For example, the application may bring up a page showing persistent
   * volumes and discovered network addresses.
   *
   * Deprecated: 3.20: use the #GtkPlacesSidebar::show-other-locations-with-flags
   * which includes the open flags in order to allow the user to specify to open
   * in a new tab or window, in a similar way than #GtkPlacesSidebar::open-location
   *
   * Since: 3.18
   */
  places_sidebar_signals [SHOW_OTHER_LOCATIONS] =
          g_signal_new (I_("show-other-locations"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST | G_SIGNAL_DEPRECATED,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, show_other_locations),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE, 0);

  /**
   * GtkPlacesSidebar::show-other-locations-with-flags:
   * @sidebar: the object which received the signal.
   * @open_flags: a single value from #GtkPlacesOpenFlags specifying how it should be opened.
   *
   * The places sidebar emits this signal when it needs the calling
   * application to present a way to show other locations e.g. drives
   * and network access points.
   * For example, the application may bring up a page showing persistent
   * volumes and discovered network addresses.
   *
   * Since: 3.20
   */
  places_sidebar_signals [SHOW_OTHER_LOCATIONS_WITH_FLAGS] =
          g_signal_new (I_("show-other-locations-with-flags"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, show_other_locations_with_flags),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE, 1,
                        GTK_TYPE_PLACES_OPEN_FLAGS);

  /**
   * GtkPlacesSidebar::mount:
   * @sidebar: the object which received the signal.
   * @mount_operation: the #GMountOperation that is going to start.
   *
   * The places sidebar emits this signal when it starts a new operation
   * because the user clicked on some location that needs mounting.
   * In this way the application using the #GtkPlacesSidebar can track the
   * progress of the operation and, for example, show a notification.
   *
   * Since: 3.20
   */
  places_sidebar_signals [MOUNT] =
          g_signal_new (I_("mount"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, mount),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE,
                        1,
                        G_TYPE_MOUNT_OPERATION);
  /**
   * GtkPlacesSidebar::unmount:
   * @sidebar: the object which received the signal.
   * @mount_operation: the #GMountOperation that is going to start.
   *
   * The places sidebar emits this signal when it starts a new operation
   * because the user for example ejected some drive or unmounted a mount.
   * In this way the application using the #GtkPlacesSidebar can track the
   * progress of the operation and, for example, show a notification.
   *
   * Since: 3.20
   */
  places_sidebar_signals [UNMOUNT] =
          g_signal_new (I_("unmount"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, unmount),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE,
                        1,
                        G_TYPE_MOUNT_OPERATION);

  /**
   * GtkPlacesSidebar::show-starred-location:
   * @sidebar: the object which received the signal.
   * @open_flags: a single value from #GtkPlacesOpenFlags specifying how the
   *   starred file should be opened.
   *
   * The places sidebar emits this signal when it needs the calling
   * application to present a way to show the starred files. In GNOME,
   * starred files are implemented by setting the nao:predefined-tag-favorite
   * tag in the tracker database.
   *
   * Since: 3.22.26
   */
  places_sidebar_signals [SHOW_STARRED_LOCATION] =
          g_signal_new (I_("show-starred-location"),
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPlacesSidebarClass, show_starred_location),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE, 1,
                        GTK_TYPE_PLACES_OPEN_FLAGS);

  properties[PROP_LOCATION] =
          g_param_spec_object ("location",
                               P_("Location to Select"),
                               P_("The location to highlight in the sidebar"),
                               G_TYPE_FILE,
                               G_PARAM_READWRITE);
  properties[PROP_OPEN_FLAGS] =
          g_param_spec_flags ("open-flags",
                              P_("Open Flags"),
                              P_("Modes in which the calling application can open locations selected in the sidebar"),
                              GTK_TYPE_PLACES_OPEN_FLAGS,
                              GTK_PLACES_OPEN_NORMAL,
                              G_PARAM_READWRITE);
  properties[PROP_SHOW_RECENT] =
          g_param_spec_boolean ("show-recent",
                                P_("Show recent files"),
                                P_("Whether the sidebar includes a builtin shortcut for recent files"),
                                TRUE,
                                G_PARAM_READWRITE);
  properties[PROP_SHOW_DESKTOP] =
          g_param_spec_boolean ("show-desktop",
                                P_("Show 'Desktop'"),
                                P_("Whether the sidebar includes a builtin shortcut to the Desktop folder"),
                                TRUE,
                                G_PARAM_READWRITE);
  properties[PROP_SHOW_CONNECT_TO_SERVER] =
          g_param_spec_boolean ("show-connect-to-server",
                                P_("Show 'Connect to Server'"),
                                P_("Whether the sidebar includes a builtin shortcut to a 'Connect to server' dialog"),
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_DEPRECATED);
  properties[PROP_SHOW_ENTER_LOCATION] =
          g_param_spec_boolean ("show-enter-location",
                                P_("Show 'Enter Location'"),
                                P_("Whether the sidebar includes a builtin shortcut to manually enter a location"),
                                FALSE,
                                G_PARAM_READWRITE);
  properties[PROP_LOCAL_ONLY] =
          g_param_spec_boolean ("local-only",
                                P_("Local Only"),
                                P_("Whether the sidebar only includes local files"),
                                FALSE,
                                G_PARAM_READWRITE);
  properties[PROP_SHOW_TRASH] =
          g_param_spec_boolean ("show-trash",
                                P_("Show 'Trash'"),
                                P_("Whether the sidebar includes a builtin shortcut to the Trash location"),
                                TRUE,
                                G_PARAM_READWRITE);
  properties[PROP_SHOW_OTHER_LOCATIONS] =
          g_param_spec_boolean ("show-other-locations",
                                P_("Show 'Other locations'"),
                                P_("Whether the sidebar includes an item to show external locations"),
                                FALSE,
                                G_PARAM_READWRITE);
  properties[PROP_SHOW_STARRED_LOCATION] =
          g_param_spec_boolean ("show-starred-location",
                                P_("Show “Starred Location”"),
                                P_("Whether the sidebar includes an item to show starred files"),
                                FALSE,
                                G_PARAM_READWRITE);


  /**
   * GtkPlacesSidebar:populate-all:
   *
   * If :populate-all is %TRUE, the #GtkPlacesSidebar::populate-popup signal
   * is also emitted for popovers.
   *
   * Since: 3.18
   */
  properties[PROP_POPULATE_ALL] =
          g_param_spec_boolean ("populate-all",
                                P_("Populate all"),
                                P_("Whether to emit ::populate-popup for popups that are not menus"),
                                FALSE,
                                G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_css_name (widget_class, "placessidebar");
}

/**
 * gtk_places_sidebar_new:
 *
 * Creates a new #GtkPlacesSidebar widget.
 *
 * The application should connect to at least the
 * #GtkPlacesSidebar::open-location signal to be notified
 * when the user makes a selection in the sidebar.
 *
 * Returns: a newly created #GtkPlacesSidebar
 *
 * Since: 3.10
 */
GtkWidget *
gtk_places_sidebar_new (void)
{
  return GTK_WIDGET (g_object_new (gtk_places_sidebar_get_type (), NULL));
}

/* Public methods for GtkPlacesSidebar */

/**
 * gtk_places_sidebar_set_open_flags:
 * @sidebar: a places sidebar
 * @flags: Bitmask of modes in which the calling application can open locations
 *
 * Sets the way in which the calling application can open new locations from
 * the places sidebar.  For example, some applications only open locations
 * “directly” into their main view, while others may support opening locations
 * in a new notebook tab or a new window.
 *
 * This function is used to tell the places @sidebar about the ways in which the
 * application can open new locations, so that the sidebar can display (or not)
 * the “Open in new tab” and “Open in new window” menu items as appropriate.
 *
 * When the #GtkPlacesSidebar::open-location signal is emitted, its flags
 * argument will be set to one of the @flags that was passed in
 * gtk_places_sidebar_set_open_flags().
 *
 * Passing 0 for @flags will cause #GTK_PLACES_OPEN_NORMAL to always be sent
 * to callbacks for the “open-location” signal.
 *
 * Since: 3.10
 */
void
gtk_places_sidebar_set_open_flags (GtkPlacesSidebar   *sidebar,
                                   GtkPlacesOpenFlags  flags)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  if (sidebar->open_flags != flags)
    {
      sidebar->open_flags = flags;
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_OPEN_FLAGS]);
    }
}

/**
 * gtk_places_sidebar_get_open_flags:
 * @sidebar: a #GtkPlacesSidebar
 *
 * Gets the open flags.
 *
 * Returns: the #GtkPlacesOpenFlags of @sidebar
 *
 * Since: 3.10
 */
GtkPlacesOpenFlags
gtk_places_sidebar_get_open_flags (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), 0);

  return sidebar->open_flags;
}

/**
 * gtk_places_sidebar_set_location:
 * @sidebar: a places sidebar
 * @location: (nullable): location to select, or %NULL for no current path
 *
 * Sets the location that is being shown in the widgets surrounding the
 * @sidebar, for example, in a folder view in a file manager.  In turn, the
 * @sidebar will highlight that location if it is being shown in the list of
 * places, or it will unhighlight everything if the @location is not among the
 * places in the list.
 *
 * Since: 3.10
 */
void
gtk_places_sidebar_set_location (GtkPlacesSidebar *sidebar,
                                 GFile            *location)
{
  GList *children;
  GList *child;
  gchar *row_uri;
  gchar *uri;
  gboolean found = FALSE;

  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  gtk_list_box_unselect_all (GTK_LIST_BOX (sidebar->list_box));

  if (sidebar->current_location != NULL)
    g_object_unref (sidebar->current_location);
  sidebar->current_location = location;
  if (sidebar->current_location != NULL)
    g_object_ref (sidebar->current_location);

  if (location == NULL)
    goto out;

  uri = g_file_get_uri (location);

  children = gtk_container_get_children (GTK_CONTAINER (sidebar->list_box));
  for (child = children; child != NULL && !found; child = child->next)
    {
      g_object_get (child->data, "uri", &row_uri, NULL);
      if (row_uri != NULL && g_strcmp0 (row_uri, uri) == 0)
        {
          gtk_list_box_select_row (GTK_LIST_BOX (sidebar->list_box),
                                   GTK_LIST_BOX_ROW (child->data));
          found = TRUE;
        }

      g_free (row_uri);
    }

  g_free (uri);
  g_list_free (children);

 out:
  g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_LOCATION]);
}

/**
 * gtk_places_sidebar_get_location:
 * @sidebar: a places sidebar
 *
 * Gets the currently selected location in the @sidebar. This can be %NULL when
 * nothing is selected, for example, when gtk_places_sidebar_set_location() has
 * been called with a location that is not among the sidebar’s list of places to
 * show.
 *
 * You can use this function to get the selection in the @sidebar.  Also, if you
 * connect to the #GtkPlacesSidebar::populate-popup signal, you can use this
 * function to get the location that is being referred to during the callbacks
 * for your menu items.
 *
 * Returns: (nullable) (transfer full): a #GFile with the selected location, or
 * %NULL if nothing is visually selected.
 *
 * Since: 3.10
 */
GFile *
gtk_places_sidebar_get_location (GtkPlacesSidebar *sidebar)
{
  GtkListBoxRow *selected;
  GFile *file;

  g_return_val_if_fail (sidebar != NULL, NULL);

  file = NULL;
  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));

  if (selected)
    {
      gchar *uri;

      g_object_get (selected, "uri", &uri, NULL);
      file = g_file_new_for_uri (uri);
      g_free (uri);
    }

  return file;
}

gchar *
gtk_places_sidebar_get_location_title (GtkPlacesSidebar *sidebar)
{
  GtkListBoxRow *selected;
  gchar *title;

  g_return_val_if_fail (sidebar != NULL, NULL);

  title = NULL;
  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));

  if (selected)
    g_object_get (selected, "label", &title, NULL);

  return title;
}

/**
 * gtk_places_sidebar_set_show_recent:
 * @sidebar: a places sidebar
 * @show_recent: whether to show an item for recent files
 *
 * Sets whether the @sidebar should show an item for recent files.
 * The default value for this option is determined by the desktop
 * environment, but this function can be used to override it on a
 * per-application basis.
 *
 * Since: 3.18
 */
void
gtk_places_sidebar_set_show_recent (GtkPlacesSidebar *sidebar,
                                    gboolean          show_recent)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  sidebar->show_recent_set = TRUE;

  show_recent = !!show_recent;
  if (sidebar->show_recent != show_recent)
    {
      sidebar->show_recent = show_recent;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_RECENT]);
    }
}

/**
 * gtk_places_sidebar_get_show_recent:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_recent()
 *
 * Returns: %TRUE if the sidebar will display a builtin shortcut for recent files
 *
 * Since: 3.18
 */
gboolean
gtk_places_sidebar_get_show_recent (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->show_recent;
}

/**
 * gtk_places_sidebar_set_show_desktop:
 * @sidebar: a places sidebar
 * @show_desktop: whether to show an item for the Desktop folder
 *
 * Sets whether the @sidebar should show an item for the Desktop folder.
 * The default value for this option is determined by the desktop
 * environment and the user’s configuration, but this function can be
 * used to override it on a per-application basis.
 *
 * Since: 3.10
 */
void
gtk_places_sidebar_set_show_desktop (GtkPlacesSidebar *sidebar,
                                     gboolean          show_desktop)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  /* Don't bother disconnecting from the GtkSettings -- it will just
   * complicate things.  Besides, it's highly unlikely that this will
   * change while we're running, but we can ignore it if it does.
   */
  sidebar->show_desktop_set = TRUE;

  show_desktop = !!show_desktop;
  if (sidebar->show_desktop != show_desktop)
    {
      sidebar->show_desktop = show_desktop;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_DESKTOP]);
    }
}

/**
 * gtk_places_sidebar_get_show_desktop:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_desktop()
 *
 * Returns: %TRUE if the sidebar will display a builtin shortcut to the desktop folder.
 *
 * Since: 3.10
 */
gboolean
gtk_places_sidebar_get_show_desktop (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->show_desktop;
}

/**
 * gtk_places_sidebar_set_show_connect_to_server:
 * @sidebar: a places sidebar
 * @show_connect_to_server: whether to show an item for the Connect to Server command
 *
 * Sets whether the @sidebar should show an item for connecting to a network server;
 * this is off by default. An application may want to turn this on if it implements
 * a way for the user to connect to network servers directly.
 *
 * If you enable this, you should connect to the
 * #GtkPlacesSidebar::show-connect-to-server signal.
 *
 * Since: 3.10
 *
 * Deprecated: 3.18: It is recommended to group this functionality with the drives
 *     and network location under the new 'Other Location' item
 */
void
gtk_places_sidebar_set_show_connect_to_server (GtkPlacesSidebar *sidebar,
                                               gboolean          show_connect_to_server)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  show_connect_to_server = !!show_connect_to_server;
  if (sidebar->show_connect_to_server != show_connect_to_server)
    {
      sidebar->show_connect_to_server = show_connect_to_server;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_CONNECT_TO_SERVER]);
    }
}

/**
 * gtk_places_sidebar_get_show_connect_to_server:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_connect_to_server()
 *
 * Returns: %TRUE if the sidebar will display a “Connect to Server” item.
 *
 * Deprecated: 3.18: It is recommended to group this functionality with the drives
 *     and network location under the new 'Other Location' item
 */
gboolean
gtk_places_sidebar_get_show_connect_to_server (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->show_connect_to_server;
}

/**
 * gtk_places_sidebar_set_show_enter_location:
 * @sidebar: a places sidebar
 * @show_enter_location: whether to show an item to enter a location
 *
 * Sets whether the @sidebar should show an item for entering a location;
 * this is off by default. An application may want to turn this on if manually
 * entering URLs is an expected user action.
 *
 * If you enable this, you should connect to the
 * #GtkPlacesSidebar::show-enter-location signal.
 *
 * Since: 3.14
 */
void
gtk_places_sidebar_set_show_enter_location (GtkPlacesSidebar *sidebar,
                                            gboolean          show_enter_location)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  show_enter_location = !!show_enter_location;
  if (sidebar->show_enter_location != show_enter_location)
    {
      sidebar->show_enter_location = show_enter_location;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_ENTER_LOCATION]);
    }
}

/**
 * gtk_places_sidebar_get_show_enter_location:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_enter_location()
 *
 * Returns: %TRUE if the sidebar will display an “Enter Location” item.
 *
 * Since: 3.14
 */
gboolean
gtk_places_sidebar_get_show_enter_location (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->show_enter_location;
}

/**
 * gtk_places_sidebar_set_show_other_locations:
 * @sidebar: a places sidebar
 * @show_other_locations: whether to show an item for the Other Locations view
 *
 * Sets whether the @sidebar should show an item for the application to show
 * an Other Locations view; this is off by default. When set to %TRUE, persistent
 * devices such as hard drives are hidden, otherwise they are shown in the sidebar.
 * An application may want to turn this on if it implements a way for the user to
 * see and interact with drives and network servers directly.
 *
 * If you enable this, you should connect to the
 * #GtkPlacesSidebar::show-other-locations signal.
 *
 * Since: 3.18
 */
void
gtk_places_sidebar_set_show_other_locations (GtkPlacesSidebar *sidebar,
                                             gboolean          show_other_locations)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  show_other_locations = !!show_other_locations;
  if (sidebar->show_other_locations != show_other_locations)
    {
      sidebar->show_other_locations = show_other_locations;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_OTHER_LOCATIONS]);
    }
  }

/**
 * gtk_places_sidebar_get_show_other_locations:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_other_locations()
 *
 * Returns: %TRUE if the sidebar will display an “Other Locations” item.
 *
 * Since: 3.18
 */
gboolean
gtk_places_sidebar_get_show_other_locations (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->show_other_locations;
}

/**
 * gtk_places_sidebar_set_show_trash:
 * @sidebar: a places sidebar
 * @show_trash: whether to show an item for the Trash location
 *
 * Sets whether the @sidebar should show an item for the Trash location.
 *
 * Since: 3.18
 */
void
gtk_places_sidebar_set_show_trash (GtkPlacesSidebar *sidebar,
                                   gboolean          show_trash)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  show_trash = !!show_trash;
  if (sidebar->show_trash != show_trash)
    {
      sidebar->show_trash = show_trash;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_TRASH]);
    }
}

/**
 * gtk_places_sidebar_get_show_trash:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_trash()
 *
 * Returns: %TRUE if the sidebar will display a “Trash” item.
 *
 * Since: 3.18
 */
gboolean
gtk_places_sidebar_get_show_trash (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), TRUE);

  return sidebar->show_trash;
}

/**
 * gtk_places_sidebar_set_local_only:
 * @sidebar: a places sidebar
 * @local_only: whether to show only local files
 *
 * Sets whether the @sidebar should only show local files.
 *
 * Since: 3.12
 */
void
gtk_places_sidebar_set_local_only (GtkPlacesSidebar *sidebar,
                                   gboolean          local_only)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  local_only = !!local_only;
  if (sidebar->local_only != local_only)
    {
      sidebar->local_only = local_only;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_LOCAL_ONLY]);
    }
}

/**
 * gtk_places_sidebar_get_local_only:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_local_only().
 *
 * Returns: %TRUE if the sidebar will only show local files.
 *
 * Since: 3.12
 */
gboolean
gtk_places_sidebar_get_local_only (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->local_only;
}

static GSList *
find_shortcut_link (GtkPlacesSidebar *sidebar,
                    GFile            *location)
{
  GSList *l;

  for (l = sidebar->shortcuts; l; l = l->next)
    {
      GFile *shortcut;

      shortcut = G_FILE (l->data);
      if (g_file_equal (shortcut, location))
        return l;
    }

  return NULL;
}

/**
 * gtk_places_sidebar_add_shortcut:
 * @sidebar: a places sidebar
 * @location: location to add as an application-specific shortcut
 *
 * Applications may want to present some folders in the places sidebar if
 * they could be immediately useful to users.  For example, a drawing
 * program could add a “/usr/share/clipart” location when the sidebar is
 * being used in an “Insert Clipart” dialog box.
 *
 * This function adds the specified @location to a special place for immutable
 * shortcuts.  The shortcuts are application-specific; they are not shared
 * across applications, and they are not persistent.  If this function
 * is called multiple times with different locations, then they are added
 * to the sidebar’s list in the same order as the function is called.
 *
 * Since: 3.10
 */
void
gtk_places_sidebar_add_shortcut (GtkPlacesSidebar *sidebar,
                                 GFile            *location)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));
  g_return_if_fail (G_IS_FILE (location));

  g_object_ref (location);
  sidebar->shortcuts = g_slist_append (sidebar->shortcuts, location);

  update_places (sidebar);
}

/**
 * gtk_places_sidebar_remove_shortcut:
 * @sidebar: a places sidebar
 * @location: location to remove
 *
 * Removes an application-specific shortcut that has been previously been
 * inserted with gtk_places_sidebar_add_shortcut().  If the @location is not a
 * shortcut in the sidebar, then nothing is done.
 *
 * Since: 3.10
 */
void
gtk_places_sidebar_remove_shortcut (GtkPlacesSidebar *sidebar,
                                    GFile            *location)
{
  GSList *link;
  GFile *shortcut;

  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));
  g_return_if_fail (G_IS_FILE (location));

  link = find_shortcut_link (sidebar, location);
  if (!link)
    return;

  shortcut = G_FILE (link->data);
  g_object_unref (shortcut);

  sidebar->shortcuts = g_slist_delete_link (sidebar->shortcuts, link);
  update_places (sidebar);
}

/**
 * gtk_places_sidebar_list_shortcuts:
 * @sidebar: a places sidebar
 *
 * Gets the list of shortcuts.
 *
 * Returns: (element-type GFile) (transfer full):
 *     A #GSList of #GFile of the locations that have been added as
 *     application-specific shortcuts with gtk_places_sidebar_add_shortcut().
 *     To free this list, you can use
 * |[<!-- language="C" -->
 * g_slist_free_full (list, (GDestroyNotify) g_object_unref);
 * ]|
 *
 * Since: 3.10
 */
GSList *
gtk_places_sidebar_list_shortcuts (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), NULL);

  return g_slist_copy_deep (sidebar->shortcuts, (GCopyFunc) g_object_ref, NULL);
}

/**
 * gtk_places_sidebar_get_nth_bookmark:
 * @sidebar: a places sidebar
 * @n: index of the bookmark to query
 *
 * This function queries the bookmarks added by the user to the places sidebar,
 * and returns one of them.  This function is used by #GtkFileChooser to implement
 * the “Alt-1”, “Alt-2”, etc. shortcuts, which activate the cooresponding bookmark.
 *
 * Returns: (nullable) (transfer full): The bookmark specified by the index @n, or
 * %NULL if no such index exist.  Note that the indices start at 0, even though
 * the file chooser starts them with the keyboard shortcut "Alt-1".
 *
 * Since: 3.10
 */
GFile *
gtk_places_sidebar_get_nth_bookmark (GtkPlacesSidebar *sidebar,
                                     gint              n)
{
  GList *rows;
  GList *l;
  int k;
  GFile *file;

  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), NULL);

  file = NULL;
  rows = gtk_container_get_children (GTK_CONTAINER (sidebar->list_box));
  l = rows;

  k = 0;
  while (l != NULL)
    {
      GtkPlacesSidebarPlaceType place_type;
      gchar *uri;

      g_object_get (l->data,
                    "place-type", &place_type,
                    "uri", &uri,
                    NULL);
      if (place_type == PLACES_BOOKMARK)
        {
          if (k == n)
            {
              file = g_file_new_for_uri (uri);
              g_free (uri);
              break;
            }
          k++;
        }
      g_free (uri);
      l = l->next;
    }

  g_list_free (rows);

  return file;
}

/**
 * gtk_places_sidebar_set_drop_targets_visible:
 * @sidebar: a places sidebar.
 * @visible: whether to show the valid targets or not.
 * @context: drag context used to ask the source about the action that wants to
 *     perform, so hints are more accurate.
 *
 * Make the GtkPlacesSidebar show drop targets, so it can show the available
 * drop targets and a "new bookmark" row. This improves the Drag-and-Drop
 * experience of the user and allows applications to show all available
 * drop targets at once.
 *
 * This needs to be called when the application is aware of an ongoing drag
 * that might target the sidebar. The drop-targets-visible state will be unset
 * automatically if the drag finishes in the GtkPlacesSidebar. You only need
 * to unset the state when the drag ends on some other widget on your application.
 *
 * Since: 3.18
 */
void
gtk_places_sidebar_set_drop_targets_visible (GtkPlacesSidebar *sidebar,
                                             gboolean          visible,
                                             GdkDragContext   *context)
{
  if (visible)
    {
      sidebar->drop_state = DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT;
      start_drop_feedback (sidebar, NULL, context);
    }
  else
    {
      if (sidebar->drop_state == DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT ||
          sidebar->drop_state == DROP_STATE_NEW_BOOKMARK_ARMED)
        {
          if (!sidebar->dragging_over)
            {
              sidebar->drop_state = DROP_STATE_NORMAL;
              stop_drop_feedback (sidebar);
            }
          else
            {
              /* In case this is called while we are dragging we need to mark the
               * drop state as no permanent so the leave timeout can do its job.
               * This will only happen in applications that call this in a wrong
               * time */
              sidebar->drop_state = DROP_STATE_NEW_BOOKMARK_ARMED;
            }
        }
    }
}

/**
 * gtk_places_sidebar_set_show_starred_location:
 * @sidebar: a places sidebar
 * @show_starred_location: whether to show an item for Starred files
 *
 * If you enable this, you should connect to the
 * #GtkPlacesSidebar::show-starred-location signal.
 *
 * Since: 3.22.26
 */
void
gtk_places_sidebar_set_show_starred_location (GtkPlacesSidebar *sidebar,
                                              gboolean          show_starred_location)
{
  g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

  show_starred_location = !!show_starred_location;
  if (sidebar->show_starred_location != show_starred_location)
    {
      sidebar->show_starred_location = show_starred_location;
      update_places (sidebar);
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_SHOW_STARRED_LOCATION]);
    }
}

/**
 * gtk_places_sidebar_get_show_starred_location:
 * @sidebar: a places sidebar
 *
 * Returns the value previously set with gtk_places_sidebar_set_show_starred_location()
 *
 * Returns: %TRUE if the sidebar will display a Starred item.
 *
 * Since: 3.22.26
 */
gboolean
gtk_places_sidebar_get_show_starred_location (GtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar), FALSE);

  return sidebar->show_starred_location;
}
