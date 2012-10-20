/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  GtkPlacesSidebar - sidebar widget for places in the filesystem
 *
 *  This code comes from Nautilus, GNOME's file manager.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *            Cosimo Cecchi <cosimoc@gnome.org>
 *            Federico Mena Quintero <federico@gnome.org>
 *
 */

/* TODO:
 *
 * * Fix instances of "#if DO_NOT_COMPILE"
 *
 * * Fix instances of "#if 0"
 *
 * * Fix FIXMEs
 *
 * * Grep for "NULL-GError" and see if they should be taken care of
 *
 * * Nautilus needs to use gtk_places_sidebar_set_uri() instead of built-in
 *   notification from the NautilusWindowSlot.
 *
 * * Nautilus needs to do the following for trash handling:
 *
 *     * Call gtk_places_sidebar_set_show_trash().
 *
 *     * Set up a NautilusTrashMonitor and when its state changes, call
 *       gtk_places_sidebar_set_trash_is_full ().
 *
 *     * Connect to the "empty-trash-requested" signal on the sidebar and empty the trash when it is emitted.
 */

#include "config.h"

#include <gio/gio.h>

#include "gdk/gdkkeysyms.h"
#include "gtkbookmarksmanager.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkicontheme.h"
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkmountoperation.h"
#include "gtkplacessidebar.h"
#include "gtkscrolledwindow.h"
#include "gtkseparatormenuitem.h"
#include "gtksettings.h"
#include "gtkstock.h"
#include "gtktreeselection.h"
#include "gtktreednd.h"
#include "gtktypebuiltins.h"
#include "gtkwindow.h"

#define EJECT_BUTTON_XPAD 6
#define ICON_CELL_XPAD 6

#define DO_NOT_COMPILE 0

struct _GtkPlacesSidebar {
	GtkScrolledWindow  parent;
	GtkTreeView        *tree_view;
	GtkCellRenderer    *eject_icon_cell_renderer;
	char 	           *uri;
	GtkListStore       *store;
	GtkBookmarksManager *bookmarks_manager;
	GVolumeMonitor *volume_monitor;

	gboolean devices_header_added;
	gboolean bookmarks_header_added;

	/* DnD */
	GList     *drag_list;
	gboolean  drag_data_received;
	int       drag_data_info;
	gboolean  drop_occured;

	GtkWidget *popup_menu;
	GtkWidget *popup_menu_open_in_new_tab_item;
	GtkWidget *popup_menu_add_shortcut_item;
	GtkWidget *popup_menu_remove_item;
	GtkWidget *popup_menu_rename_item;
	GtkWidget *popup_menu_separator_item;
	GtkWidget *popup_menu_mount_item;
	GtkWidget *popup_menu_unmount_item;
	GtkWidget *popup_menu_eject_item;
	GtkWidget *popup_menu_rescan_item;
	GtkWidget *popup_menu_empty_trash_item;
	GtkWidget *popup_menu_start_item;
	GtkWidget *popup_menu_stop_item;
	GtkWidget *popup_menu_properties_separator_item;
	GtkWidget *popup_menu_properties_item;

	/* volume mounting - delayed open process */
	gboolean mounting;
	GtkPlacesOpenMode go_to_after_mount_open_mode;

	GDBusProxy *hostnamed_proxy;
	char *hostname;

	guint multiple_tabs_supported : 1;
	guint multiple_windows_supported : 1;
	guint show_desktop : 1;
	guint show_properties : 1;
	guint show_trash : 1;
	guint trash_is_full : 1;
};

struct _GtkPlacesSidebarClass {
	GtkScrolledWindowClass parent;

	void (* location_selected)     (GtkPlacesSidebar *sidebar,
				        GFile            *location,
				        GtkPlacesOpenMode open_mode);
	void (* empty_trash_requested) (GtkPlacesSidebar *sidebar);
	void (* initiated_unmount)     (GtkPlacesSidebar *sidebar,
				        gboolean          initiated_unmount);
	void (* show_error_message)    (GtkPlacesSidebar *sidebar,
				        const char       *primary,
				        const char       *secondary);
};

enum {
	PLACES_SIDEBAR_COLUMN_ROW_TYPE,
	PLACES_SIDEBAR_COLUMN_URI,
	PLACES_SIDEBAR_COLUMN_DRIVE,
	PLACES_SIDEBAR_COLUMN_VOLUME,
	PLACES_SIDEBAR_COLUMN_MOUNT,
	PLACES_SIDEBAR_COLUMN_NAME,
	PLACES_SIDEBAR_COLUMN_GICON,
	PLACES_SIDEBAR_COLUMN_INDEX,
	PLACES_SIDEBAR_COLUMN_EJECT,
	PLACES_SIDEBAR_COLUMN_NO_EJECT,
	PLACES_SIDEBAR_COLUMN_BOOKMARK,
	PLACES_SIDEBAR_COLUMN_TOOLTIP,
	PLACES_SIDEBAR_COLUMN_EJECT_GICON,
	PLACES_SIDEBAR_COLUMN_SECTION_TYPE,
	PLACES_SIDEBAR_COLUMN_HEADING_TEXT,

	PLACES_SIDEBAR_COLUMN_COUNT
};

typedef enum {
	PLACES_BUILT_IN,
	PLACES_XDG_DIR,
	PLACES_MOUNTED_VOLUME,
	PLACES_BOOKMARK,
	PLACES_HEADING,
} PlaceType;

typedef enum {
	SECTION_DEVICES,
	SECTION_BOOKMARKS,
	SECTION_COMPUTER,
	SECTION_NETWORK,
} SectionType;

enum {
	LOCATION_SELECTED,
	EMPTY_TRASH_REQUESTED,
	INITIATED_UNMOUNT,
	SHOW_ERROR_MESSAGE,
	LAST_SIGNAL,
};

/* Names for themed icons */
#define ICON_NAME_HOME		"user-home-symbolic"
#define ICON_NAME_DESKTOP	"user-desktop"
#define ICON_NAME_FILESYSTEM	"drive-harddisk-symbolic"
#define ICON_NAME_EJECT		"media-eject-symbolic"
#define ICON_NAME_NETWORK	"network-workgroup-symbolic"
#define ICON_NAME_TRASH		"user-trash-symbolic"
#define ICON_NAME_TRASH_FULL	"user-trash-full-symbolic"

#define ICON_NAME_FOLDER_DESKTOP	"user-desktop"
#define ICON_NAME_FOLDER_DOCUMENTS	"folder-documents-symbolic"
#define ICON_NAME_FOLDER_DOWNLOAD	"folder-download-symbolic"
#define ICON_NAME_FOLDER_MUSIC		"folder-music-symbolic"
#define ICON_NAME_FOLDER_PICTURES	"folder-pictures-symbolic"
#define ICON_NAME_FOLDER_PUBLIC_SHARE	"folder-publicshare-symbolic"
#define ICON_NAME_FOLDER_TEMPLATES	"folder-templates-symbolic"
#define ICON_NAME_FOLDER_VIDEOS		"folder-videos-symbolic"
#define ICON_NAME_FOLDER_SAVED_SEARCH	"folder-saved-search-symbolic"

static guint places_sidebar_signals [LAST_SIGNAL] = { 0 };

static void  open_selected_bookmark                    (GtkPlacesSidebar        *sidebar,
							GtkTreeModel                 *model,
							GtkTreeIter                  *iter,
							GtkPlacesOpenMode             open_mode);
static void  gtk_places_sidebar_style_set         (GtkWidget                    *widget,
							GtkStyle                     *previous_style);
static gboolean eject_or_unmount_bookmark              (GtkPlacesSidebar *sidebar,
							GtkTreePath *path);
static gboolean eject_or_unmount_selection             (GtkPlacesSidebar *sidebar);
static void  check_unmount_and_eject                   (GMount *mount,
							GVolume *volume,
							GDrive *drive,
							gboolean *show_unmount,
							gboolean *show_eject);

static void bookmarks_check_popup_sensitivity          (GtkPlacesSidebar *sidebar);

/* Identifiers for target types */
enum {
	GTK_TREE_MODEL_ROW,
	TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry dnd_source_targets[] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry dnd_drop_targets [] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
	{ "text/uri-list", 0, TEXT_URI_LIST }
};

/* Drag and drop interface declarations */
typedef struct {
	GtkListStore parent;

	GtkPlacesSidebar *sidebar;
} ShortcutsModel;

typedef struct {
	GtkListStoreClass parent_class;
} ShortcutsModelClass;

#define SHORTCUTS_MODEL_TYPE (shortcuts_model_get_type ())
#define SHORTCUTS_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHORTCUTS_MODEL_TYPE, ShortcutsModel))

static GType shortcuts_model_get_type (void);
static void shortcuts_model_drag_source_iface_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (ShortcutsModel,
			 shortcuts_model,
			 GTK_TYPE_LIST_STORE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						shortcuts_model_drag_source_iface_init));

static GtkListStore *shortcuts_model_new (GtkPlacesSidebar *sidebar);

G_DEFINE_TYPE (GtkPlacesSidebar, gtk_places_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static void
emit_location_selected (GtkPlacesSidebar *sidebar, GFile *location, GtkPlacesOpenMode open_mode)
{
	if ((!sidebar->multiple_tabs_supported && open_mode == GTK_PLACES_OPEN_MODE_NEW_TAB)
	    || (!sidebar->multiple_windows_supported && open_mode == GTK_PLACES_OPEN_MODE_NEW_WINDOW))
		open_mode = GTK_PLACES_OPEN_MODE_NORMAL;

	g_signal_emit (sidebar, places_sidebar_signals[LOCATION_SELECTED], 0,
		       location, open_mode);
}

static void
emit_empty_trash_requested (GtkPlacesSidebar *sidebar)
{
	g_signal_emit (sidebar, places_sidebar_signals[EMPTY_TRASH_REQUESTED], 0);
}

static void
emit_initiated_unmount (GtkPlacesSidebar *sidebar, gboolean initiated_unmount)
{
	g_signal_emit (sidebar, places_sidebar_signals[INITIATED_UNMOUNT], 0,
		       initiated_unmount);
}

static void
emit_show_error_message (GtkPlacesSidebar *sidebar, const char *primary, const char *secondary)
{
	g_signal_emit (sidebar, places_sidebar_signals[SHOW_ERROR_MESSAGE], 0,
		       primary, secondary);
}

static gint
get_icon_size (GtkPlacesSidebar *sidebar)
{
	GtkSettings *settings;
	gint width, height;

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (sidebar)));

	if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &width, &height))
		return MAX (width, height);
	else
		return 16;
}

#if 0
/* FIXME: remove this?  Let's allow the user to bookmark whatever he damn well pleases */
static gboolean
is_built_in_bookmark (NautilusFile *file)
{
	gboolean built_in;
	gint idx;

	if (nautilus_file_is_home (file)) {
		return TRUE;
	}

	if (nautilus_file_is_desktop_directory (file) &&
	    !g_settings_get_boolean (gnome_background_preferences, NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
		return FALSE;
	}

	built_in = FALSE;

	for (idx = 0; idx < G_USER_N_DIRECTORIES; idx++) {
		/* PUBLIC_SHARE and TEMPLATES are not in our built-in list */
		if (nautilus_file_is_user_special_directory (file, idx)) {
			if (idx != G_USER_DIRECTORY_PUBLIC_SHARE &&  idx != G_USER_DIRECTORY_TEMPLATES) {
				built_in = TRUE;
			}

			break;
		}
	}

	return built_in;
}
#endif

static GtkTreeIter
add_heading (GtkPlacesSidebar *sidebar,
	     SectionType section_type,
	     const gchar *title)
{
	GtkTreeIter iter;

	gtk_list_store_append (sidebar->store, &iter);
	gtk_list_store_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_HEADING,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
			    PLACES_SIDEBAR_COLUMN_HEADING_TEXT, title,
			    PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
			    PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
			    -1);

	return iter;
}

static void
check_heading_for_section (GtkPlacesSidebar *sidebar,
			   SectionType section_type)
{
	switch (section_type) {
	case SECTION_DEVICES:
		if (!sidebar->devices_header_added) {
			add_heading (sidebar, SECTION_DEVICES,
				     _("Devices"));
			sidebar->devices_header_added = TRUE;
		}

		break;
	case SECTION_BOOKMARKS:
		if (!sidebar->bookmarks_header_added) {
			add_heading (sidebar, SECTION_BOOKMARKS,
				     _("Bookmarks"));
			sidebar->bookmarks_header_added = TRUE;
		}

		break;
	default:
		break;
	}
}

static void
add_place (GtkPlacesSidebar *sidebar,
	   PlaceType place_type,
	   SectionType section_type,
	   const char *name,
	   GIcon *icon,
	   const char *uri,
	   GDrive *drive,
	   GVolume *volume,
	   GMount *mount,
	   const int index,
	   const char *tooltip)
{
	GtkTreeIter           iter;
	GIcon *eject;
	gboolean show_eject, show_unmount;
	gboolean show_eject_button;

	check_heading_for_section (sidebar, section_type);

	check_unmount_and_eject (mount, volume, drive,
				 &show_unmount, &show_eject);

	if (show_unmount || show_eject) {
		g_assert (place_type != PLACES_BOOKMARK);
	}

	if (mount == NULL) {
		show_eject_button = FALSE;
	} else {
		show_eject_button = (show_unmount || show_eject);
	}

	if (show_eject_button) {
		eject = g_themed_icon_new_with_default_fallbacks ("media-eject-symbolic");
	} else {
		eject = NULL;
	}

	gtk_list_store_append (sidebar->store, &iter);
	gtk_list_store_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_GICON, icon,
			    PLACES_SIDEBAR_COLUMN_NAME, name,
			    PLACES_SIDEBAR_COLUMN_URI, uri,
			    PLACES_SIDEBAR_COLUMN_DRIVE, drive,
			    PLACES_SIDEBAR_COLUMN_VOLUME, volume,
			    PLACES_SIDEBAR_COLUMN_MOUNT, mount,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
			    PLACES_SIDEBAR_COLUMN_INDEX, index,
			    PLACES_SIDEBAR_COLUMN_EJECT, show_eject_button,
			    PLACES_SIDEBAR_COLUMN_NO_EJECT, !show_eject_button,
			    PLACES_SIDEBAR_COLUMN_BOOKMARK, place_type != PLACES_BOOKMARK,
			    PLACES_SIDEBAR_COLUMN_TOOLTIP, tooltip,
			    PLACES_SIDEBAR_COLUMN_EJECT_GICON, eject,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
			    -1);

	if (eject != NULL) {
		g_object_unref (eject);
	}
}

typedef struct {
	const gchar *location;
	const gchar *last_uri;
	GtkPlacesSidebar *sidebar;
	GtkTreePath *path;
} RestoreLocationData;

static gboolean
restore_selection_foreach (GtkTreeModel *model,
			   GtkTreePath *path,
			   GtkTreeIter *iter,
			   gpointer user_data)
{
	RestoreLocationData *data = user_data;
	gchar *uri;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_URI, &uri,
			    -1);

	if (g_strcmp0 (uri, data->last_uri) == 0 ||
	    g_strcmp0 (uri, data->location) == 0) {
		data->path = gtk_tree_path_copy (path);
	}

	g_free (uri);

	return (data->path != NULL);
}

static void
sidebar_update_restore_selection (GtkPlacesSidebar *sidebar,
				  const gchar *location,
				  const gchar *last_uri)
{
	RestoreLocationData data;
	GtkTreeSelection *selection;

	data.location = location;
	data.last_uri = last_uri;
	data.sidebar = sidebar;
	data.path = NULL;

	gtk_tree_model_foreach (GTK_TREE_MODEL (sidebar->store),
				restore_selection_foreach, &data);

	if (data.path != NULL) {
		selection = gtk_tree_view_get_selection (sidebar->tree_view);
		gtk_tree_selection_select_path (selection, data.path);
		gtk_tree_path_free (data.path);
	}
}

static GIcon *
special_directory_get_gicon (GUserDirectory directory)
{
#define ICON_CASE(x)				\
	case G_USER_DIRECTORY_ ## x:					\
		return g_themed_icon_new (ICON_NAME_FOLDER_ ## x);

	switch (directory) {

		ICON_CASE (DOCUMENTS);
		ICON_CASE (DOWNLOAD);
		ICON_CASE (MUSIC);
		ICON_CASE (PICTURES);
		ICON_CASE (PUBLIC_SHARE);
		ICON_CASE (TEMPLATES);
		ICON_CASE (VIDEOS);

	default:
		return g_themed_icon_new ("folder-symbolic");
	}

#undef ICON_CASE
}

static gboolean
recent_is_supported (void)
{
	const char * const *supported;
	int i;

	supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
	if (!supported) {
		return FALSE;
	}

	for (i = 0; supported[i] != NULL; i++) {
		if (strcmp ("recent", supported[i]) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
add_special_dirs (GtkPlacesSidebar *sidebar)
{
	GList *dirs;
	int index;

	dirs = NULL;
	for (index = 0; index < G_USER_N_DIRECTORIES; index++) {
		const char *path;
		GFile *root;
		GIcon *icon;
		char *name;
		char *mount_uri;
		char *tooltip;

		if (index == G_USER_DIRECTORY_DESKTOP ||
		    index == G_USER_DIRECTORY_TEMPLATES ||
		    index == G_USER_DIRECTORY_PUBLIC_SHARE) {
			continue;
		}

		path = g_get_user_special_dir (index);

		/* xdg resets special dirs to the home directory in case
		 * it's not finiding what it expects. We don't want the home
		 * to be added multiple times in that weird configuration.
		 */
		if (path == NULL
		    || g_strcmp0 (path, g_get_home_dir ()) == 0
		    || g_list_find_custom (dirs, path, (GCompareFunc) g_strcmp0) != NULL) {
			continue;
		}

		root = g_file_new_for_path (path);
		name = g_file_get_basename (root);
		icon = special_directory_get_gicon (index);
		mount_uri = g_file_get_uri (root);
		tooltip = g_file_get_parse_name (root);

		add_place (sidebar, PLACES_XDG_DIR,
			   SECTION_COMPUTER,
			   name, icon, mount_uri,
			   NULL, NULL, NULL, 0,
			   tooltip);
		g_free (name);
		g_object_unref (root);
		g_object_unref (icon);
		g_free (mount_uri);
		g_free (tooltip);

		dirs = g_list_prepend (dirs, (char *)path);
	}

	g_list_free (dirs);
}

static char *
get_home_directory_uri (void)
{
	const char *home;

	home = g_get_home_dir ();
	if (!home)
		return NULL;

	return g_strconcat ("file://", home, NULL);
}

static char *
get_desktop_directory_uri (void)
{
	const char *name;

	name = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
	/* "To disable a directory, point it to the homedir."
	 * See http://freedesktop.org/wiki/Software/xdg-user-dirs
	 **/
	if (!g_strcmp0 (name, g_get_home_dir ())) {
		return NULL;
	} else {
		return g_strconcat ("file://", name, NULL);
	}
}

static void
update_places (GtkPlacesSidebar *sidebar)
{
	GtkTreeSelection *selection;
	GtkTreeIter last_iter;
	GtkTreeModel *model;
	GVolumeMonitor *volume_monitor;
	GList *mounts, *l, *ll;
	GMount *mount;
	GList *drives;
	GDrive *drive;
	GList *volumes;
	GVolume *volume;
	GSList *bookmarks, *sl;
	int index;
	char *location, *mount_uri, *name, *last_uri, *identifier;
	char *bookmark_name;
	GIcon *icon;
	GFile *root;
	char *tooltip;
	GList *network_mounts, *network_volumes;

	model = NULL;
	last_uri = NULL;

	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	if (gtk_tree_selection_get_selected (selection, &model, &last_iter)) {
		gtk_tree_model_get (model,
				    &last_iter,
				    PLACES_SIDEBAR_COLUMN_URI, &last_uri, -1);
	}
	gtk_list_store_clear (sidebar->store);

	sidebar->devices_header_added = FALSE;
	sidebar->bookmarks_header_added = FALSE;

#if 0
	/* FIXME: the "location" is used at the end of this function
	 * to restore the current location.  How do we do that now?
	 */
	slot = nautilus_window_get_active_slot (sidebar->window);
	location = nautilus_window_slot_get_current_uri (slot);
#else
	location = NULL;
#endif

	network_mounts = network_volumes = NULL;
	volume_monitor = sidebar->volume_monitor;

	/* add built in bookmarks */

	add_heading (sidebar, SECTION_COMPUTER,
		     _("Places"));

	if (recent_is_supported ()) {
		mount_uri = "recent:///"; /* No need to strdup */
		icon = g_themed_icon_new ("document-open-recent-symbolic");
		add_place (sidebar, PLACES_BUILT_IN,
			   SECTION_COMPUTER,
			   _("Recent"), icon, mount_uri,
			   NULL, NULL, NULL, 0,
			   _("Recent files"));
		g_object_unref (icon);
	}

	/* home folder */
	mount_uri = get_home_directory_uri ();
	icon = g_themed_icon_new (ICON_NAME_HOME);
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_COMPUTER,
		   _("Home"), icon,
		   mount_uri, NULL, NULL, NULL, 0,
		   _("Open your personal folder"));
	g_object_unref (icon);
	g_free (mount_uri);

	if (sidebar->show_desktop) {
		/* desktop */
		mount_uri = get_desktop_directory_uri ();
		icon = g_themed_icon_new (ICON_NAME_DESKTOP);
		add_place (sidebar, PLACES_BUILT_IN,
			   SECTION_COMPUTER,
			   _("Desktop"), icon,
			   mount_uri, NULL, NULL, NULL, 0,
			   _("Open the contents of your desktop in a folder"));
		g_object_unref (icon);
		g_free (mount_uri);
	}

	/* XDG directories */
	add_special_dirs (sidebar);

	if (sidebar->show_trash) {
		mount_uri = "trash:///"; /* No need to strdup */
		icon = g_themed_icon_new (sidebar->trash_is_full ? ICON_NAME_TRASH_FULL : ICON_NAME_TRASH);
		add_place (sidebar, PLACES_BUILT_IN,
			   SECTION_COMPUTER,
			   _("Trash"), icon, mount_uri,
			   NULL, NULL, NULL, 0,
			   _("Open the trash"));
		g_object_unref (icon);
	}

	/* go through all connected drives */
	drives = g_volume_monitor_get_connected_drives (volume_monitor);

	for (l = drives; l != NULL; l = l->next) {
		drive = l->data;

		volumes = g_drive_get_volumes (drive);
		if (volumes != NULL) {
			for (ll = volumes; ll != NULL; ll = ll->next) {
				volume = ll->data;
				identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

				if (g_strcmp0 (identifier, "network") == 0) {
					g_free (identifier);
					network_volumes = g_list_prepend (network_volumes, volume);
					continue;
				}
				g_free (identifier);

				mount = g_volume_get_mount (volume);
				if (mount != NULL) {
					/* Show mounted volume in the sidebar */
					icon = g_mount_get_symbolic_icon (mount);
					root = g_mount_get_default_location (mount);
					mount_uri = g_file_get_uri (root);
					name = g_mount_get_name (mount);
					tooltip = g_file_get_parse_name (root);

					add_place (sidebar, PLACES_MOUNTED_VOLUME,
						   SECTION_DEVICES,
						   name, icon, mount_uri,
						   drive, volume, mount, 0, tooltip);
					g_object_unref (root);
					g_object_unref (mount);
					g_object_unref (icon);
					g_free (tooltip);
					g_free (name);
					g_free (mount_uri);
				} else {
					/* Do show the unmounted volumes in the sidebar;
					 * this is so the user can mount it (in case automounting
					 * is off).
					 *
					 * Also, even if automounting is enabled, this gives a visual
					 * cue that the user should remember to yank out the media if
					 * he just unmounted it.
					 */
					icon = g_volume_get_symbolic_icon (volume);
					name = g_volume_get_name (volume);
					tooltip = g_strdup_printf (_("Mount and open %s"), name);

					add_place (sidebar, PLACES_MOUNTED_VOLUME,
						   SECTION_DEVICES,
						   name, icon, NULL,
						   drive, volume, NULL, 0, tooltip);
					g_object_unref (icon);
					g_free (name);
					g_free (tooltip);
				}
				g_object_unref (volume);
			}
			g_list_free (volumes);
		} else {
			if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive)) {
				/* If the drive has no mountable volumes and we cannot detect media change.. we
				 * display the drive in the sidebar so the user can manually poll the drive by
				 * right clicking and selecting "Rescan..."
				 *
				 * This is mainly for drives like floppies where media detection doesn't
				 * work.. but it's also for human beings who like to turn off media detection
				 * in the OS to save battery juice.
				 */
				icon = g_drive_get_symbolic_icon (drive);
				name = g_drive_get_name (drive);
				tooltip = g_strdup_printf (_("Mount and open %s"), name);

				add_place (sidebar, PLACES_BUILT_IN,
					   SECTION_DEVICES,
					   name, icon, NULL,
					   drive, NULL, NULL, 0, tooltip);
				g_object_unref (icon);
				g_free (tooltip);
				g_free (name);
			}
		}
		g_object_unref (drive);
	}
	g_list_free (drives);

	/* add all volumes that is not associated with a drive */
	volumes = g_volume_monitor_get_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		drive = g_volume_get_drive (volume);
		if (drive != NULL) {
		    	g_object_unref (volume);
			g_object_unref (drive);
			continue;
		}

		identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

		if (g_strcmp0 (identifier, "network") == 0) {
			g_free (identifier);
			network_volumes = g_list_prepend (network_volumes, volume);
			continue;
		}
		g_free (identifier);

		mount = g_volume_get_mount (volume);
		if (mount != NULL) {
			icon = g_mount_get_symbolic_icon (mount);
			root = g_mount_get_default_location (mount);
			mount_uri = g_file_get_uri (root);
			tooltip = g_file_get_parse_name (root);
			g_object_unref (root);
			name = g_mount_get_name (mount);
			add_place (sidebar, PLACES_MOUNTED_VOLUME,
				   SECTION_DEVICES,
				   name, icon, mount_uri,
				   NULL, volume, mount, 0, tooltip);
			g_object_unref (mount);
			g_object_unref (icon);
			g_free (name);
			g_free (tooltip);
			g_free (mount_uri);
		} else {
			/* see comment above in why we add an icon for an unmounted mountable volume */
			icon = g_volume_get_symbolic_icon (volume);
			name = g_volume_get_name (volume);
			add_place (sidebar, PLACES_MOUNTED_VOLUME,
				   SECTION_DEVICES,
				   name, icon, NULL,
				   NULL, volume, NULL, 0, name);
			g_object_unref (icon);
			g_free (name);
		}
		g_object_unref (volume);
	}
	g_list_free (volumes);

	/* file system root */

 	mount_uri = "file:///"; /* No need to strdup */
	icon = g_themed_icon_new (ICON_NAME_FILESYSTEM);
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_DEVICES,
		   sidebar->hostname, icon,
		   mount_uri, NULL, NULL, NULL, 0,
		   _("Open the contents of the File System"));
	g_object_unref (icon);

	/* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
	mounts = g_volume_monitor_get_mounts (volume_monitor);

	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;
		if (g_mount_is_shadowed (mount)) {
			g_object_unref (mount);
			continue;
		}
		volume = g_mount_get_volume (mount);
		if (volume != NULL) {
		    	g_object_unref (volume);
			g_object_unref (mount);
			continue;
		}
		root = g_mount_get_default_location (mount);

		if (!g_file_is_native (root)) {
			network_mounts = g_list_prepend (network_mounts, mount);
			g_object_unref (root);
			continue;
		}

		icon = g_mount_get_symbolic_icon (mount);
		mount_uri = g_file_get_uri (root);
		name = g_mount_get_name (mount);
		tooltip = g_file_get_parse_name (root);
		add_place (sidebar, PLACES_MOUNTED_VOLUME,
			   SECTION_COMPUTER,
			   name, icon, mount_uri,
			   NULL, NULL, mount, 0, tooltip);
		g_object_unref (root);
		g_object_unref (mount);
		g_object_unref (icon);
		g_free (name);
		g_free (mount_uri);
		g_free (tooltip);
	}
	g_list_free (mounts);

	/* add bookmarks */

	bookmarks = _gtk_bookmarks_manager_list_bookmarks (sidebar->bookmarks_manager);

	for (sl = bookmarks, index = 0; sl; sl = sl->next, index++) {
		GFileInfo *info;

		root = sl->data;

#if 0
		/* FIXME: remove this?  If we *do* show bookmarks for nonexistent files, the user will eventually clean them up */
		if (nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
			continue;
		}
#endif

#if 0
		/* FIXME: remove this?  Let's allow the user to bookmark whatever he damn well pleases */
		NautilusFile *file;
		file = nautilus_file_get (root);

		if (is_built_in_bookmark (file)) {
			g_object_unref (root);
			nautilus_file_unref (file);
			continue;
		}
		nautilus_file_unref (file);
#endif

		/* FIXME: we are getting file info synchronously.  We may want to do it async at some point. */
		info = g_file_query_info (root,
					  "standard::display-name,standard::icon",
					  G_FILE_QUERY_INFO_NONE,
					  NULL,
					  NULL); /* NULL-GError */

		if (info) {
			bookmark_name = _gtk_bookmarks_manager_get_bookmark_label (sidebar->bookmarks_manager, root);

			if (bookmark_name == NULL)
				bookmark_name = g_strdup (g_file_info_get_display_name (info));

			/* FIXME: in commit 0ed400b9c1692e42498bff3c10780073ec137f63, nautilus added the ability
			 * to get a symbolic icon for bookmarks.  We don't have that machinery.  Should we
			 * just copy that code?
			 */
			icon = g_file_info_get_icon (info);

			mount_uri = g_file_get_uri (root);
			tooltip = g_file_get_parse_name (root);

			add_place (sidebar, PLACES_BOOKMARK,
				   SECTION_BOOKMARKS,
				   bookmark_name, icon, mount_uri,
				   NULL, NULL, NULL, index,
				   tooltip);

			g_free (mount_uri);
			g_free (tooltip);
			g_free (bookmark_name);

			g_object_unref (info);
		}
	}

	g_slist_foreach (bookmarks, (GFunc) g_object_unref, NULL);
	g_slist_free (bookmarks);

	/* network */
	add_heading (sidebar, SECTION_NETWORK,
		     _("Network"));

 	mount_uri = "network:///"; /* No need to strdup */
	icon = g_themed_icon_new (ICON_NAME_NETWORK);
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_NETWORK,
		   _("Browse Network"), icon,
		   mount_uri, NULL, NULL, NULL, 0,
		   _("Browse the contents of the network"));
	g_object_unref (icon);

	network_volumes = g_list_reverse (network_volumes);
	for (l = network_volumes; l != NULL; l = l->next) {
		volume = l->data;
		mount = g_volume_get_mount (volume);

		if (mount != NULL) {
			network_mounts = g_list_prepend (network_mounts, mount);
			continue;
		} else {
			icon = g_volume_get_symbolic_icon (volume);
			name = g_volume_get_name (volume);
			tooltip = g_strdup_printf (_("Mount and open %s"), name);

			add_place (sidebar, PLACES_MOUNTED_VOLUME,
				   SECTION_NETWORK,
				   name, icon, NULL,
				   NULL, volume, NULL, 0, tooltip);
			g_object_unref (icon);
			g_free (name);
			g_free (tooltip);
		}
	}

	g_list_free_full (network_volumes, g_object_unref);

	network_mounts = g_list_reverse (network_mounts);
	for (l = network_mounts; l != NULL; l = l->next) {
		mount = l->data;
		root = g_mount_get_default_location (mount);
		icon = g_mount_get_symbolic_icon (mount);
		mount_uri = g_file_get_uri (root);
		name = g_mount_get_name (mount);
		tooltip = g_file_get_parse_name (root);
		add_place (sidebar, PLACES_MOUNTED_VOLUME,
			   SECTION_NETWORK,
			   name, icon, mount_uri,
			   NULL, NULL, mount, 0, tooltip);
		g_object_unref (root);
		g_object_unref (mount);
		g_object_unref (icon);
		g_free (name);
		g_free (mount_uri);
		g_free (tooltip);
	}

	g_list_free_full (network_mounts, g_object_unref);

	/* restore selection */
	sidebar_update_restore_selection (sidebar, location, last_uri);

	g_free (location);
	g_free (last_uri);
}

static void
mount_added_callback (GVolumeMonitor *volume_monitor,
		      GMount *mount,
		      GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_added_callback (GVolumeMonitor *volume_monitor,
		       GVolume *volume,
		       GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_removed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_changed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_disconnected_callback (GVolumeMonitor *volume_monitor,
			     GDrive         *drive,
			     GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_connected_callback (GVolumeMonitor *volume_monitor,
			  GDrive         *drive,
			  GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_changed_callback (GVolumeMonitor *volume_monitor,
			GDrive         *drive,
			GtkPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static gboolean
over_eject_button (GtkPlacesSidebar *sidebar,
		   gint x,
		   gint y,
		   GtkTreePath **path)
{
	GtkTreeViewColumn *column;
	int width, x_offset, hseparator;
	int eject_button_size;
	gboolean show_eject;
	GtkTreeIter iter;
	GtkTreeModel *model;

	*path = NULL;
	model = gtk_tree_view_get_model (sidebar->tree_view);

	if (gtk_tree_view_get_path_at_pos (sidebar->tree_view,
					   x, y,
					   path, &column, NULL, NULL)) {

		gtk_tree_model_get_iter (model, &iter, *path);
		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_EJECT, &show_eject,
				    -1);

		if (!show_eject) {
			goto out;
		}


		gtk_widget_style_get (GTK_WIDGET (sidebar->tree_view),
				      "horizontal-separator", &hseparator,
				      NULL);

		/* Reload cell attributes for this particular row */
		gtk_tree_view_column_cell_set_cell_data (column,
							 model, &iter, FALSE, FALSE);

		gtk_tree_view_column_cell_get_position (column,
							sidebar->eject_icon_cell_renderer,
							&x_offset, &width);

		eject_button_size = get_icon_size (sidebar);

		/* This is kinda weird, but we have to do it to workaround gtk+ expanding
		 * the eject cell renderer (even thought we told it not to) and we then
		 * had to set it right-aligned */
		x_offset += width - hseparator - EJECT_BUTTON_XPAD - eject_button_size;

		if (x - x_offset >= 0 &&
		    x - x_offset <= eject_button_size) {
			return TRUE;
		}
	}

 out:
	if (*path != NULL) {
		gtk_tree_path_free (*path);
		*path = NULL;
	}

	return FALSE;
}

static gboolean
clicked_eject_button (GtkPlacesSidebar *sidebar,
		      GtkTreePath **path)
{
	GdkEvent *event = gtk_get_current_event ();
	GdkEventButton *button_event = (GdkEventButton *) event;

	if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
	     over_eject_button (sidebar, button_event->x, button_event->y, path)) {
		return TRUE;
	}

	return FALSE;
}

/* Computes the appropriate row and position for dropping */
static gboolean
compute_drop_position (GtkTreeView             *tree_view,
		       int                      x,
		       int                      y,
		       GtkTreePath            **path,
		       GtkTreeViewDropPosition *pos,
		       GtkPlacesSidebar        *sidebar)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	PlaceType place_type;
	SectionType section_type;

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
						x, y,
						path, pos)) {
		return FALSE;
	}

	model = gtk_tree_view_get_model (tree_view);

	gtk_tree_model_get_iter (model, &iter, *path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
			    -1);

	if (section_type != SECTION_BOOKMARKS &&
	    place_type == PLACES_HEADING) {
		/* never drop on headings, but special case the bookmarks heading,
		 * so we can drop bookmarks in between it and the first item when
		 * reordering.
		 */

		gtk_tree_path_free (*path);
		*path = NULL;

		return FALSE;
	}

	if (section_type != SECTION_BOOKMARKS &&
	    sidebar->drag_data_received &&
	    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
		/* don't allow dropping bookmarks into non-bookmark areas */
		gtk_tree_path_free (*path);
		*path = NULL;

		return FALSE;
	}

	if (sidebar->drag_data_received &&
	    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
		/* bookmark rows can only be reordered */
		*pos = GTK_TREE_VIEW_DROP_AFTER;
	} else {
		*pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
	}

	return TRUE;
}

static gboolean
get_drag_data (GtkTreeView *tree_view,
	       GdkDragContext *context,
	       unsigned int time)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target (GTK_WIDGET (tree_view),
					    context,
					    NULL);

	if (target == GDK_NONE) {
		return FALSE;
	}

	gtk_drag_get_data (GTK_WIDGET (tree_view),
			   context, target, time);

	return TRUE;
}

static void
free_drag_data (GtkPlacesSidebar *sidebar)
{
	sidebar->drag_data_received = FALSE;

	if (sidebar->drag_list) {
#if DO_NOT_COMPILE
		nautilus_drag_destroy_selection_list (sidebar->drag_list);
#endif
		sidebar->drag_list = NULL;
	}
}

static gboolean
drag_motion_callback (GtkTreeView *tree_view,
		      GdkDragContext *context,
		      int x,
		      int y,
		      unsigned int time,
		      GtkPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;
	int action;
	GtkTreeIter iter;
	char *uri;
	gboolean res;

	if (!sidebar->drag_data_received) {
		if (!get_drag_data (tree_view, context, time)) {
			return FALSE;
		}
	}

	path = NULL;
	res = compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

	if (!res) {
		goto out;
	}

	if (pos == GTK_TREE_VIEW_DROP_AFTER ) {
		if (sidebar->drag_data_received &&
		    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
			action = GDK_ACTION_MOVE;
		} else {
			action = 0;
		}
	} else {
		if (sidebar->drag_list == NULL) {
			action = 0;
		} else {
			gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
						 &iter, path);
			gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store),
					    &iter,
					    PLACES_SIDEBAR_COLUMN_URI, &uri,
					    -1);
#if DO_NOT_COMPILE
			nautilus_drag_default_drop_action_for_icons (context, uri,
								     sidebar->drag_list,
								     &action);
#endif
			g_free (uri);
		}
	}

	if (action != 0) {
		gtk_tree_view_set_drag_dest_row (tree_view, path, pos);
	}

	if (path != NULL) {
		gtk_tree_path_free (path);
	}

 out:
	g_signal_stop_emission_by_name (tree_view, "drag-motion");

	if (action != 0) {
		gdk_drag_status (context, action, time);
	} else {
		gdk_drag_status (context, 0, time);
	}

	return TRUE;
}

static void
drag_leave_callback (GtkTreeView *tree_view,
		     GdkDragContext *context,
		     unsigned int time,
		     GtkPlacesSidebar *sidebar)
{
	free_drag_data (sidebar);
	gtk_tree_view_set_drag_dest_row (tree_view, NULL, 0);
	g_signal_stop_emission_by_name (tree_view, "drag-leave");
}

static GList *
uri_list_from_selection (GList *selection)
{
#if DO_NOT_COMPILE
	NautilusDragSelectionItem *item;
#endif
	GList *ret;
	GList *l;

	ret = NULL;
#if DO_NOT_COMPILE
	for (l = selection; l != NULL; l = l->next) {
		item = l->data;
		ret = g_list_prepend (ret, item->uri);
	}
#endif

	return g_list_reverse (ret);
}

static GList*
build_selection_list (const char *data)
{
#if DO_NOT_COMPILE
	NautilusDragSelectionItem *item;
#endif
	GList *result;
	char **uris;
	char *uri;
	int i;

	uris = g_uri_list_extract_uris (data);

	result = NULL;
#if DO_NOT_COMPILE
	for (i = 0; uris[i]; i++) {
		uri = uris[i];
		item = nautilus_drag_selection_item_new ();
		item->uri = g_strdup (uri);
		item->got_icon_position = FALSE;
		result = g_list_prepend (result, item);
	}
#endif

	g_strfreev (uris);

	return g_list_reverse (result);
}

static gboolean
get_selected_iter (GtkPlacesSidebar *sidebar,
		   GtkTreeIter *iter)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (sidebar->tree_view);

	return gtk_tree_selection_get_selected (selection, NULL, iter);
}

/* Reorders the selected bookmark to the specified position */
static void
reorder_bookmarks (GtkPlacesSidebar *sidebar,
		   int                   new_position)
{
	GtkTreeIter iter;
	char *uri;
	GFile *file;

	/* Get the selected path */
	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_URI, &uri,
			    -1);

	file = g_file_new_for_uri (uri);
	_gtk_bookmarks_manager_reorder_bookmark (sidebar->bookmarks_manager, file, new_position, NULL); /* NULL-GError */

	g_object_unref (file);
	g_free (uri);
}

static void
drag_data_received_callback (GtkWidget *widget,
			     GdkDragContext *context,
			     int x,
			     int y,
			     GtkSelectionData *selection_data,
			     unsigned int info,
			     unsigned int time,
			     GtkPlacesSidebar *sidebar)
{
#if DO_NOT_COMPILE
	GtkTreeView *tree_view;
	GtkTreePath *tree_path;
	GtkTreeViewDropPosition tree_pos;
	GtkTreeIter iter;
	int position;
	GtkTreeModel *model;
	char *drop_uri;
	GList *selection_list, *uris;
	PlaceType place_type;
	SectionType section_type;
	gboolean success;

	tree_view = GTK_TREE_VIEW (widget);

	if (!sidebar->drag_data_received) {
		if (gtk_selection_data_get_target (selection_data) != GDK_NONE &&
		    info == TEXT_URI_LIST) {
			sidebar->drag_list = build_selection_list ((const gchar *) gtk_selection_data_get_data (selection_data));
		} else {
			sidebar->drag_list = NULL;
		}
		sidebar->drag_data_received = TRUE;
		sidebar->drag_data_info = info;
	}

	g_signal_stop_emission_by_name (widget, "drag-data-received");

	if (!sidebar->drop_occured) {
		return;
	}

	/* Compute position */
	success = compute_drop_position (tree_view, x, y, &tree_path, &tree_pos, sidebar);
	if (!success) {
		goto out;
	}

	success = FALSE;

	if (tree_pos == GTK_TREE_VIEW_DROP_AFTER) {
		model = gtk_tree_view_get_model (tree_view);

		if (!gtk_tree_model_get_iter (model, &iter, tree_path)) {
			goto out;
		}

		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
    				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
				    PLACES_SIDEBAR_COLUMN_INDEX, &position,
				    -1);

		if (section_type != SECTION_BOOKMARKS) {
			goto out;
		}

		if (tree_pos == GTK_TREE_VIEW_DROP_AFTER && place_type != PLACES_HEADING) {
			/* heading already has position 0 */
			position++;
		}

		switch (info) {
		case GTK_TREE_MODEL_ROW:
			reorder_bookmarks (sidebar, position);
			success = TRUE;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else {
		GdkDragAction real_action;

		/* file transfer requested */
		real_action = gdk_drag_context_get_selected_action (context);

		if (real_action == GDK_ACTION_ASK) {
			real_action =
				nautilus_drag_drop_action_ask (GTK_WIDGET (tree_view),
							       gdk_drag_context_get_actions (context));
		}

		if (real_action > 0) {
			model = gtk_tree_view_get_model (tree_view);

			gtk_tree_model_get_iter (model, &iter, tree_path);
			gtk_tree_model_get (model, &iter,
					    PLACES_SIDEBAR_COLUMN_URI, &drop_uri,
					    -1);

			switch (info) {
			case TEXT_URI_LIST:
				selection_list = build_selection_list ((const gchar *) gtk_selection_data_get_data (selection_data));
				uris = uri_list_from_selection (selection_list);
				nautilus_file_operations_copy_move (uris, NULL, drop_uri,
								    real_action, GTK_WIDGET (tree_view),
								    NULL, NULL);
				nautilus_drag_destroy_selection_list (selection_list);
				g_list_free (uris);
				success = TRUE;
				break;
			case GTK_TREE_MODEL_ROW:
				success = FALSE;
				break;
			default:
				g_assert_not_reached ();
				break;
			}

			g_free (drop_uri);
		}
	}

out:
	sidebar->drop_occured = FALSE;
	free_drag_data (sidebar);
	gtk_drag_finish (context, success, FALSE, time);

	gtk_tree_path_free (tree_path);
#endif
}

static gboolean
drag_drop_callback (GtkTreeView *tree_view,
		    GdkDragContext *context,
		    int x,
		    int y,
		    unsigned int time,
		    GtkPlacesSidebar *sidebar)
{
	gboolean retval = FALSE;
	sidebar->drop_occured = TRUE;
	retval = get_drag_data (tree_view, context, time);
	g_signal_stop_emission_by_name (tree_view, "drag-drop");
	return retval;
}

/* Callback used when the file list's popup menu is detached */
static void
bookmarks_popup_menu_detach_cb (GtkWidget *attach_widget,
				GtkMenu   *menu)
{
	GtkPlacesSidebar *sidebar;

	sidebar = GTK_PLACES_SIDEBAR (attach_widget);
	g_assert (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->popup_menu = NULL;
	sidebar->popup_menu_add_shortcut_item = NULL;
	sidebar->popup_menu_remove_item = NULL;
	sidebar->popup_menu_rename_item = NULL;
	sidebar->popup_menu_separator_item = NULL;
	sidebar->popup_menu_mount_item = NULL;
	sidebar->popup_menu_unmount_item = NULL;
	sidebar->popup_menu_eject_item = NULL;
	sidebar->popup_menu_rescan_item = NULL;
	sidebar->popup_menu_start_item = NULL;
	sidebar->popup_menu_stop_item = NULL;
	sidebar->popup_menu_empty_trash_item = NULL;
	sidebar->popup_menu_properties_separator_item = NULL;
	sidebar->popup_menu_properties_item = NULL;
}

static void
check_unmount_and_eject (GMount *mount,
			 GVolume *volume,
			 GDrive *drive,
			 gboolean *show_unmount,
			 gboolean *show_eject)
{
	*show_unmount = FALSE;
	*show_eject = FALSE;

	if (drive != NULL) {
		*show_eject = g_drive_can_eject (drive);
	}

	if (volume != NULL) {
		*show_eject |= g_volume_can_eject (volume);
	}
	if (mount != NULL) {
		*show_eject |= g_mount_can_eject (mount);
		*show_unmount = g_mount_can_unmount (mount) && !*show_eject;
	}
}

static void
check_visibility (GMount           *mount,
		  GVolume          *volume,
		  GDrive           *drive,
		  gboolean         *show_mount,
		  gboolean         *show_unmount,
		  gboolean         *show_eject,
		  gboolean         *show_rescan,
		  gboolean         *show_start,
		  gboolean         *show_stop)
{
	*show_mount = FALSE;
	*show_rescan = FALSE;
	*show_start = FALSE;
	*show_stop = FALSE;

	check_unmount_and_eject (mount, volume, drive, show_unmount, show_eject);

	if (drive != NULL) {
		if (g_drive_is_media_removable (drive) &&
		    !g_drive_is_media_check_automatic (drive) &&
		    g_drive_can_poll_for_media (drive))
			*show_rescan = TRUE;

		*show_start = g_drive_can_start (drive) || g_drive_can_start_degraded (drive);
		*show_stop  = g_drive_can_stop (drive);

		if (*show_stop)
			*show_unmount = FALSE;
	}

	if (volume != NULL) {
		if (mount == NULL)
			*show_mount = g_volume_can_mount (volume);
	}
}

static void
bookmarks_check_popup_sensitivity (GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	PlaceType type;
	GDrive *drive = NULL;
	GVolume *volume = NULL;
	GMount *mount = NULL;
	GFile *location;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_rescan;
	gboolean show_start;
	gboolean show_stop;
	gboolean show_empty_trash;
	gboolean show_properties;
	char *uri = NULL;

	type = PLACES_BUILT_IN;

	if (sidebar->popup_menu == NULL) {
		return;
	}

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
				    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
 				    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
				    PLACES_SIDEBAR_COLUMN_URI, &uri,
				    -1);
	}

	gtk_widget_set_visible (sidebar->popup_menu_add_shortcut_item, (type == PLACES_MOUNTED_VOLUME));

	gtk_widget_set_sensitive (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
	gtk_widget_set_sensitive (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));
	gtk_widget_set_sensitive (sidebar->popup_menu_empty_trash_item, sidebar->trash_is_full);

 	check_visibility (mount, volume, drive,
 			  &show_mount, &show_unmount, &show_eject, &show_rescan, &show_start, &show_stop);

	if (sidebar->show_trash) {
		show_empty_trash = ((uri != NULL) &&
				    (!strcmp (uri, "trash:///")));
	} else
		show_empty_trash = FALSE;

	/* Only show properties for local mounts */
	if (sidebar->show_properties) {
		show_properties = (mount != NULL);
		if (mount != NULL) {
			location = g_mount_get_default_location (mount);
			show_properties = g_file_is_native (location);
			g_object_unref (location);
		}
	} else
		show_properties = FALSE;

	gtk_widget_set_visible (sidebar->popup_menu_separator_item,
		      show_mount || show_unmount || show_eject || show_empty_trash);
	gtk_widget_set_visible (sidebar->popup_menu_mount_item, show_mount);
	gtk_widget_set_visible (sidebar->popup_menu_unmount_item, show_unmount);
	gtk_widget_set_visible (sidebar->popup_menu_eject_item, show_eject);
	gtk_widget_set_visible (sidebar->popup_menu_rescan_item, show_rescan);
	gtk_widget_set_visible (sidebar->popup_menu_start_item, show_start);
	gtk_widget_set_visible (sidebar->popup_menu_stop_item, show_stop);
	gtk_widget_set_visible (sidebar->popup_menu_empty_trash_item, show_empty_trash);
	gtk_widget_set_visible (sidebar->popup_menu_properties_separator_item, show_properties);
	gtk_widget_set_visible (sidebar->popup_menu_properties_item, show_properties);

	/* Adjust start/stop items to reflect the type of the drive */
	gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start"));
	gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop"));
	if ((show_start || show_stop) && drive != NULL) {
		switch (g_drive_get_start_stop_type (drive)) {
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			/* start() for type G_DRIVE_START_STOP_TYPE_SHUTDOWN is normally not used */
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Power On"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Safely Remove Drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Connect Drive"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Disconnect Drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start Multi-disk Device"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop Multi-disk Device"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			/* stop() for type G_DRIVE_START_STOP_TYPE_PASSWORD is normally not used */
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Unlock Drive"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Lock Drive"));
			break;

		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			/* uses defaults set above */
			break;
		}
	}


	g_free (uri);
}

/* Callback used when the selection in the shortcuts tree changes */
static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
				GtkPlacesSidebar *sidebar)
{
	bookmarks_check_popup_sensitivity (sidebar);
}

static void
volume_mounted_cb (GVolume *volume,
		   GObject *user_data)
{
	GMount *mount;
	GtkPlacesSidebar *sidebar;

	sidebar = GTK_PLACES_SIDEBAR (user_data);

	sidebar->mounting = FALSE;

	mount = g_volume_get_mount (volume);
	if (mount != NULL) {
		GFile *location;

		location = g_mount_get_default_location (mount);
		emit_location_selected (sidebar, location, sidebar->go_to_after_mount_open_mode);

		g_object_unref (G_OBJECT (location));
		g_object_unref (G_OBJECT (mount));
	}
}

static void
drive_start_from_bookmark_cb (GObject      *source_object,
			      GAsyncResult *res,
			      gpointer      user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = GTK_PLACES_SIDEBAR (user_data);

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to start %s"), name);
			g_free (name);
			emit_show_error_message (sidebar, primary, error->message);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
open_selected_bookmark (GtkPlacesSidebar *sidebar,
			GtkTreeModel	      *model,
			GtkTreeIter	      *iter,
			GtkPlacesOpenMode      open_mode)
{
	GFile *location;
	char *uri;

	if (!iter) {
		return;
	}

	gtk_tree_model_get (model, iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

	if (uri != NULL) {
		location = g_file_new_for_uri (uri);
		emit_location_selected (sidebar, location, open_mode);

		g_object_unref (location);
		g_free (uri);

	} else {
		GDrive *drive;
		GVolume *volume;

		gtk_tree_model_get (model, iter,
				    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
				    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
				    -1);

		if (volume != NULL && !sidebar->mounting) {
			sidebar->mounting = TRUE;

			sidebar->go_to_after_mount_open_mode = open_mode;

#if DO_NOT_COMPILE
			nautilus_file_operations_mount_volume_full (NULL, volume,
								    volume_mounted_cb,
								    G_OBJECT (sidebar));
#endif
		} else if (volume == NULL && drive != NULL &&
			   (g_drive_can_start (drive) || g_drive_can_start_degraded (drive))) {
			GMountOperation *mount_op;

			mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
			g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_from_bookmark_cb, sidebar);
			g_object_unref (mount_op);
		}

		if (drive != NULL)
			g_object_unref (drive);
		if (volume != NULL)
			g_object_unref (volume);
	}
}

static void
open_shortcut_from_menu (GtkPlacesSidebar *sidebar,
			 GtkPlacesOpenMode open_mode)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;

	model = gtk_tree_view_get_model (sidebar->tree_view);
	gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

	if (path != NULL && gtk_tree_model_get_iter (model, &iter, path)) {
		open_selected_bookmark (sidebar, model, &iter, open_mode);
	}

	gtk_tree_path_free (path);
}

static void
open_shortcut_cb (GtkMenuItem      *item,
		  GtkPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, GTK_PLACES_OPEN_MODE_NORMAL);
}

static void
open_shortcut_in_new_window_cb (GtkMenuItem      *item,
				GtkPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, GTK_PLACES_OPEN_MODE_NEW_WINDOW);
}

static void
open_shortcut_in_new_tab_cb (GtkMenuItem      *item,
			     GtkPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, GTK_PLACES_OPEN_MODE_NEW_TAB);
}

/* Add bookmark for the selected item */
static void
add_bookmark (GtkPlacesSidebar *sidebar)
{
#if DO_NOT_COMPILE
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;
	char *name;
	GFile *location;
	NautilusBookmark *bookmark;

	model = gtk_tree_view_get_model (sidebar->tree_view);

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_URI, &uri,
				    PLACES_SIDEBAR_COLUMN_NAME, &name,
				    -1);

		if (uri == NULL) {
			return;
		}

		location = g_file_new_for_uri (uri);
		bookmark = nautilus_bookmark_new (location, name);

		if (!nautilus_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
			nautilus_bookmark_list_append (sidebar->bookmarks, bookmark);
		}

		g_object_unref (location);
		g_object_unref (bookmark);
		g_free (uri);
		g_free (name);
	}
#endif
}

static void
add_shortcut_cb (GtkMenuItem           *item,
		 GtkPlacesSidebar *sidebar)
{
	add_bookmark (sidebar);
}

/* Rename the selected bookmark */
static void
rename_selected_bookmark (GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GList *renderers;
	PlaceType type;

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    -1);

		if (type != PLACES_BOOKMARK) {
			return;
		}

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (sidebar->tree_view), 0);
		renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
		cell = g_list_nth_data (renderers, 6);
		g_list_free (renderers);
		g_object_set (cell, "editable", TRUE, NULL);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (sidebar->tree_view),
						path, column, cell, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
rename_shortcut_cb (GtkMenuItem           *item,
		    GtkPlacesSidebar *sidebar)
{
	rename_selected_bookmark (sidebar);
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	PlaceType type;
	char *uri;
	GFile *file;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type != PLACES_BOOKMARK) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_URI, &uri,
			    -1);

	file = g_file_new_for_uri (uri);
	_gtk_bookmarks_manager_remove_bookmark (sidebar->bookmarks_manager, file, NULL); /* NULL-GError */

	g_object_unref (file);
	g_free (uri);
}

static void
remove_shortcut_cb (GtkMenuItem           *item,
		    GtkPlacesSidebar *sidebar)
{
	remove_selected_bookmarks (sidebar);
}

static void
mount_shortcut_cb (GtkMenuItem           *item,
		   GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GVolume *volume;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    -1);

	if (volume != NULL) {
#if DO_NOT_COMPILE
		nautilus_file_operations_mount_volume (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))), volume);
		g_object_unref (volume);
#endif
	}
}

static void
unmount_done (gpointer data)
{
	GtkPlacesSidebar *sidebar;

	sidebar = data;
	g_object_unref (sidebar);
}

static void
do_unmount (GMount *mount,
	    GtkPlacesSidebar *sidebar)
{
	if (mount != NULL) {
#if DO_NOT_COMPILE
		nautilus_file_operations_unmount_mount_full (NULL, mount, FALSE, TRUE,
							     unmount_done,
							     g_object_ref (sidebar));
#endif
	}
}

static void
do_unmount_selection (GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    -1);

	if (mount != NULL) {
		do_unmount (mount, sidebar);
		g_object_unref (mount);
	}
}

static void
unmount_shortcut_cb (GtkMenuItem           *item,
		     GtkPlacesSidebar *sidebar)
{
	do_unmount_selection (sidebar);
}

static void
show_unmount_progress_cb (GMountOperation *op,
			  const gchar *message,
			  gint64 time_left,
			  gint64 bytes_left,
			  gpointer user_data)
{
#if DO_NOT_COMPILE
	NautilusApplication *app = NAUTILUS_APPLICATION (g_application_get_default ());

	if (bytes_left == 0) {
		nautilus_application_notify_unmount_done (app, message);
	} else {
		nautilus_application_notify_unmount_show (app, message);
	}
#endif
}

static void
show_unmount_progress_aborted_cb (GMountOperation *op,
				  gpointer user_data)
{
#if DO_NOT_COMPILE
	NautilusApplication *app = NAUTILUS_APPLICATION (g_application_get_default ());
	nautilus_application_notify_unmount_done (app, NULL);
#endif
}

static void
drive_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = user_data;

	error = NULL;
	if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
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
volume_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = user_data;

	error = NULL;
	if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
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
mount_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = user_data;

	error = NULL;
	if (!g_mount_eject_with_operation_finish (G_MOUNT (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
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
do_eject (GMount *mount,
	  GVolume *volume,
	  GDrive *drive,
	  GtkPlacesSidebar *sidebar)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
	if (mount != NULL) {
		g_mount_eject_with_operation (mount, 0, mount_op, NULL, mount_eject_cb,
					      g_object_ref (sidebar));
	} else if (volume != NULL) {
		g_volume_eject_with_operation (volume, 0, mount_op, NULL, volume_eject_cb,
					      g_object_ref (sidebar));
	} else if (drive != NULL) {
		g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb,
					      g_object_ref (sidebar));
	}

	g_signal_connect (mount_op, "show-unmount-progress",
			  G_CALLBACK (show_unmount_progress_cb), sidebar);
	g_signal_connect (mount_op, "aborted",
			  G_CALLBACK (show_unmount_progress_aborted_cb), sidebar);
	g_object_unref (mount_op);
}

static void
eject_shortcut_cb (GtkMenuItem           *item,
		   GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;
	GVolume *volume;
	GDrive *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	do_eject (mount, volume, drive, sidebar);
}

static gboolean
eject_or_unmount_bookmark (GtkPlacesSidebar *sidebar,
			   GtkTreePath *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean can_unmount, can_eject;
	GMount *mount;
	GVolume *volume;
	GDrive *drive;
	gboolean ret;

	model = GTK_TREE_MODEL (sidebar->store);

	if (!path) {
		return FALSE;
	}
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return FALSE;
	}

	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	ret = FALSE;

	check_unmount_and_eject (mount, volume, drive, &can_unmount, &can_eject);
	/* if we can eject, it has priority over unmount */
	if (can_eject) {
		do_eject (mount, volume, drive, sidebar);
		ret = TRUE;
	} else if (can_unmount) {
		do_unmount (mount, sidebar);
		ret = TRUE;
	}

	if (mount != NULL)
		g_object_unref (mount);
	if (volume != NULL)
		g_object_unref (volume);
	if (drive != NULL)
		g_object_unref (drive);

	return ret;
}

static gboolean
eject_or_unmount_selection (GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean ret;

	if (!get_selected_iter (sidebar, &iter)) {
		return FALSE;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
	if (path == NULL) {
		return FALSE;
	}

	ret = eject_or_unmount_bookmark (sidebar, path);

	gtk_tree_path_free (path);

	return ret;
}

static void
drive_poll_for_media_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = GTK_PLACES_SIDEBAR (user_data);

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to poll %s for media changes"), name);
			g_free (name);
			emit_show_error_message (sidebar, primary, error->message);
			g_free (primary);
		}
		g_error_free (error);
	}

	/* FIXME: drive_stop_cb() gets a reffed sidebar, and unrefs it.  Do we need to do the same here? */
}

static void
rescan_shortcut_cb (GtkMenuItem           *item,
		    GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, sidebar);
	}
	g_object_unref (drive);
}

static void
drive_start_cb (GObject      *source_object,
		GAsyncResult *res,
		gpointer      user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = GTK_PLACES_SIDEBAR (user_data);

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to start %s"), name);
			g_free (name);
			emit_show_error_message (sidebar, primary, error->message);
			g_free (primary);
		}
		g_error_free (error);
	}

	/* FIXME: drive_stop_cb() gets a reffed sidebar, and unrefs it.  Do we need to do the same here? */
}

static void
start_shortcut_cb (GtkMenuItem           *item,
		   GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		GMountOperation *mount_op;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));

		g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_cb, sidebar);

		g_object_unref (mount_op);
	}
	g_object_unref (drive);
}

static void
drive_stop_cb (GObject *source_object,
	       GAsyncResult *res,
	       gpointer user_data)
{
	GtkPlacesSidebar *sidebar;
	GError *error;
	char *primary;
	char *name;

	sidebar = user_data;

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to stop %s"), name);
			g_free (name);
			emit_show_error_message (sidebar, primary, error->message);
			g_free (primary);
		}
		g_error_free (error);
	}

	g_object_unref (sidebar);
}

static void
stop_shortcut_cb (GtkMenuItem           *item,
		  GtkPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		GMountOperation *mount_op;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
		g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
			      g_object_ref (sidebar));
		g_object_unref (mount_op);
	}
	g_object_unref (drive);
}

static void
empty_trash_cb (GtkMenuItem      *item,
		GtkPlacesSidebar *sidebar)
{
	emit_empty_trash_requested (sidebar);
}

static gboolean
find_prev_or_next_row (GtkPlacesSidebar *sidebar,
		       GtkTreeIter *iter,
		       gboolean go_up)
{
	GtkTreeModel *model = GTK_TREE_MODEL (sidebar->store);
	gboolean res;
	int place_type;

	if (go_up) {
		res = gtk_tree_model_iter_previous (model, iter);
	} else {
		res = gtk_tree_model_iter_next (model, iter);
	}

	if (res) {
		gtk_tree_model_get (model, iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
				    -1);
		if (place_type == PLACES_HEADING) {
			if (go_up) {
				res = gtk_tree_model_iter_previous (model, iter);
			} else {
				res = gtk_tree_model_iter_next (model, iter);
			}
		}
	}

	return res;
}

static gboolean
find_prev_row (GtkPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	return find_prev_or_next_row (sidebar, iter, TRUE);
}

static gboolean
find_next_row (GtkPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	return find_prev_or_next_row (sidebar, iter, FALSE);
}

static void
properties_cb (GtkMenuItem      *item,
	       GtkPlacesSidebar *sidebar)
{
#if DO_NOT_COMPILE
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GList *list;
	NautilusFile *file;
	char *uri;

	model = gtk_tree_view_get_model (sidebar->tree_view);
	gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

	if (path == NULL || !gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

	if (uri != NULL) {

		file = nautilus_file_get_by_uri (uri);
		list = g_list_prepend (NULL, nautilus_file_ref (file));

		nautilus_properties_window_present (list, GTK_WIDGET (sidebar), NULL);

		nautilus_file_list_free (list);
		g_free (uri);
	}

	gtk_tree_path_free (path);
#endif
}

static gboolean
gtk_places_sidebar_focus (GtkWidget *widget,
		          GtkDirectionType direction)
{
	GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (widget);
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean res;

	res = get_selected_iter (sidebar, &iter);

	if (!res) {
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store), &iter);
		res = find_next_row (sidebar, &iter);
		if (res) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
			gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
	}

	return GTK_WIDGET_CLASS (gtk_places_sidebar_parent_class)->focus (widget, direction);
}

/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
bookmarks_key_press_event_cb (GtkWidget             *widget,
			      GdkEventKey           *event,
			      GtkPlacesSidebar *sidebar)
{
	guint modifiers;
	GtkTreeIter selected_iter;
	GtkTreePath *path;

	if (!get_selected_iter (sidebar, &selected_iter)) {
		return FALSE;
	}

	modifiers = gtk_accelerator_get_default_mod_mask ();

	if ((event->keyval == GDK_KEY_Return ||
		event->keyval == GDK_KEY_KP_Enter ||
		event->keyval == GDK_KEY_ISO_Enter ||
		event->keyval == GDK_KEY_space)) {

		GtkPlacesOpenMode open_mode = GTK_PLACES_OPEN_MODE_NORMAL;

		if ((event->state & modifiers) == GDK_SHIFT_MASK) {
			open_mode = GTK_PLACES_OPEN_MODE_NEW_TAB;
		} else if ((event->state & modifiers) == GDK_CONTROL_MASK) {
			open_mode = GTK_PLACES_OPEN_MODE_NEW_WINDOW;
		}

		open_selected_bookmark (sidebar, GTK_TREE_MODEL (sidebar->store),
					&selected_iter, open_mode);

		return TRUE;
	}

	if (event->keyval == GDK_KEY_Down &&
	    (event->state & modifiers) == GDK_MOD1_MASK) {
		return eject_or_unmount_selection (sidebar);
	}

	if (event->keyval == GDK_KEY_Up) {
		if (find_prev_row (sidebar, &selected_iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &selected_iter);
			gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		return TRUE;
	}

	if (event->keyval == GDK_KEY_Down) {
		if (find_next_row (sidebar, &selected_iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &selected_iter);
			gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		return TRUE;
	}

	if ((event->keyval == GDK_KEY_Delete
	     || event->keyval == GDK_KEY_KP_Delete)
	    && (event->state & modifiers) == 0) {
		remove_selected_bookmarks (sidebar);
		return TRUE;
	}

	if ((event->keyval == GDK_KEY_F2)
	    && (event->state & modifiers) == 0) {
		rename_selected_bookmark (sidebar);
		return TRUE;
	}

	return FALSE;
}

static GtkMenuItem *
append_menu_separator (GtkMenu *menu)
{
	GtkWidget *menu_item;

	menu_item = gtk_separator_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menu_item, -1);

	return GTK_MENU_ITEM (menu_item);
}

/* Constructs the popup menu for the file list if needed */
static void
bookmarks_build_popup_menu (GtkPlacesSidebar *sidebar)
{
	GtkWidget *item;

	if (sidebar->popup_menu) {
		return;
	}

	sidebar->popup_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (sidebar->popup_menu),
			           GTK_WIDGET (sidebar),
			           bookmarks_popup_menu_detach_cb);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				       gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
	sidebar->popup_menu_open_in_new_tab_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_in_new_tab_cb), sidebar);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	if (sidebar->multiple_tabs_supported) {
		gtk_widget_show (item);
	}

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_in_new_window_cb), sidebar);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	if (sidebar->multiple_windows_supported) {
		gtk_widget_show (item);
	}

	append_menu_separator (GTK_MENU (sidebar->popup_menu));

	item = gtk_menu_item_new_with_mnemonic (_("_Add Bookmark"));
	sidebar->popup_menu_add_shortcut_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (add_shortcut_cb), sidebar);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_image_menu_item_new_with_label (_("Remove"));
	sidebar->popup_menu_remove_item = item;
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		    G_CALLBACK (remove_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_label (_("Rename..."));
	sidebar->popup_menu_rename_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (rename_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	/* Mount/Unmount/Eject menu items */

	sidebar->popup_menu_separator_item = GTK_WIDGET (append_menu_separator (GTK_MENU (sidebar->popup_menu)));

	item = gtk_menu_item_new_with_mnemonic (_("_Mount"));
	sidebar->popup_menu_mount_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (mount_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Unmount"));
	sidebar->popup_menu_unmount_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (unmount_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Eject"));
	sidebar->popup_menu_eject_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (eject_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Detect Media"));
	sidebar->popup_menu_rescan_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (rescan_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Start"));
	sidebar->popup_menu_start_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (start_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Stop"));
	sidebar->popup_menu_stop_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (stop_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	/* Empty Trash menu item */

	item = gtk_menu_item_new_with_mnemonic (_("Empty _Trash"));
	sidebar->popup_menu_empty_trash_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (empty_trash_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	/* Properties menu item */

	sidebar->popup_menu_properties_separator_item = GTK_WIDGET (append_menu_separator (GTK_MENU (sidebar->popup_menu)));

	item = gtk_menu_item_new_with_mnemonic (_("_Properties"));
	sidebar->popup_menu_properties_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (properties_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	bookmarks_check_popup_sensitivity (sidebar);
}

static void
bookmarks_update_popup_menu (GtkPlacesSidebar *sidebar)
{
	bookmarks_build_popup_menu (sidebar);
}

static void
bookmarks_popup_menu (GtkPlacesSidebar *sidebar,
		      GdkEventButton   *event)
{
	int button;

	bookmarks_update_popup_menu (sidebar);

	/* The event button needs to be 0 if we're popping up this menu from
	 * a button release, else a 2nd click outside the menu with any button
	 * other than the one that invoked the menu will be ignored (instead
	 * of dismissing the menu). This is a subtle fragility of the GTK menu code.
	 */
	if (event) {
		if (event->type == GDK_BUTTON_RELEASE)
			button = 0;
		else
			button = event->button;
	} else {
		button = 0;
	}

	gtk_menu_popup (GTK_MENU (sidebar->popup_menu),		/* menu */
			NULL,					/* parent_menu_shell */
			NULL,					/* parent_menu_item */
			NULL,					/* popup_position_func */
			NULL,					/* popup_position_user_data */
			button,					/* button */
			event ? event->time : gtk_get_current_event_time ()); /* activate_time */

	g_object_ref_sink (sidebar->popup_menu); /* FIXME: is this right?  It was gtk_object_sink() */
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
bookmarks_popup_menu_cb (GtkWidget *widget,
			 GtkPlacesSidebar *sidebar)
{
	bookmarks_popup_menu (sidebar, NULL);
	return TRUE;
}

static gboolean
bookmarks_button_release_event_cb (GtkWidget *widget,
				   GdkEventButton *event,
				   GtkPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	gboolean ret = FALSE;
	gboolean res;

	path = NULL;

	if (event->type != GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	if (clicked_eject_button (sidebar, &path)) {
		eject_or_unmount_bookmark (sidebar, path);
		gtk_tree_path_free (path);

		return FALSE;
	}

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
		return FALSE;
 	}

	res = gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y,
					     &path, NULL, NULL, NULL);

	if (!res || path == NULL) {
		return FALSE;
	}

	res = gtk_tree_model_get_iter (model, &iter, path);
	if (!res) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	if (event->button == 1) {
		open_selected_bookmark (sidebar, model, &iter, 0);
	} else if (event->button == 2) {
		GtkPlacesOpenMode open_mode = GTK_PLACES_OPEN_MODE_NORMAL;

		open_mode = ((event->state & GDK_CONTROL_MASK) ?
			     GTK_PLACES_OPEN_MODE_NEW_WINDOW :
			     GTK_PLACES_OPEN_MODE_NEW_TAB);

		open_selected_bookmark (sidebar, model, &iter, open_mode);
		ret = TRUE;
	} else if (event->button == 3) {
		PlaceType row_type;

		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &row_type,
				    -1);

		if (row_type != PLACES_HEADING) {
			bookmarks_popup_menu (sidebar, event);
		}
	}

	gtk_tree_path_free (path);

	return ret;
}

static void
bookmarks_edited (GtkCellRenderer       *cell,
		  gchar                 *path_string,
		  gchar                 *new_text,
		  GtkPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	char *uri;
	GFile *file;

	g_object_set (cell, "editable", FALSE, NULL);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
		            PLACES_SIDEBAR_COLUMN_URI, &uri,
		            -1);
	gtk_tree_path_free (path);

	file = g_file_new_for_uri (uri);
	_gtk_bookmarks_manager_set_bookmark_label (sidebar->bookmarks_manager, file, new_text, NULL); /* NULL-GError */

	g_object_unref (file);
	g_free (uri);
}

static void
bookmarks_editing_canceled (GtkCellRenderer       *cell,
			    GtkPlacesSidebar *sidebar)
{
	g_object_set (cell, "editable", FALSE, NULL);
}

static gboolean
tree_selection_func (GtkTreeSelection *selection,
		     GtkTreeModel *model,
		     GtkTreePath *path,
		     gboolean path_currently_selected,
		     gpointer user_data)
{
	GtkTreeIter iter;
	PlaceType row_type;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &row_type,
			    -1);

	if (row_type == PLACES_HEADING) {
		return FALSE;
	}

	return TRUE;
}

static void
icon_cell_renderer_func (GtkTreeViewColumn *column,
			 GtkCellRenderer *cell,
			 GtkTreeModel *model,
			 GtkTreeIter *iter,
			 gpointer user_data)
{
	PlaceType type;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type == PLACES_HEADING) {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
	} else {
		g_object_set (cell,
			      "visible", TRUE,
			      NULL);
	}
}

static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
	PlaceType type;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type == PLACES_HEADING) {
		g_object_set (cell,
			      "visible", FALSE,
			      "xpad", 0,
			      "ypad", 0,
			      NULL);
	} else {
		g_object_set (cell,
			      "visible", TRUE,
			      "xpad", 3,
			      "ypad", 3,
			      NULL);
	}
}

static void
heading_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
	PlaceType type;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type == PLACES_HEADING) {
		g_object_set (cell,
			      "visible", TRUE,
			      NULL);
	} else {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
	}
}

static gint
places_sidebar_sort_func (GtkTreeModel *model,
			  GtkTreeIter *iter_a,
			  GtkTreeIter *iter_b,
			  gpointer user_data)
{
	SectionType section_type_a, section_type_b;
	PlaceType place_type_a, place_type_b;
	gint retval = 0;

	gtk_tree_model_get (model, iter_a,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type_a,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type_b,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type_b,
			    -1);

	/* fall back to the default order if we're not in the
	 * XDG part of the computer section.
	 */
	if ((section_type_a == section_type_b) &&
	    (section_type_a == SECTION_COMPUTER) &&
	    (place_type_a == place_type_b) &&
	    (place_type_a == PLACES_XDG_DIR)) {
		gchar *name_a, *name_b;

		gtk_tree_model_get (model, iter_a,
				    PLACES_SIDEBAR_COLUMN_NAME, &name_a,
				    -1);
		gtk_tree_model_get (model, iter_b,
				    PLACES_SIDEBAR_COLUMN_NAME, &name_b,
				    -1);

		retval = g_utf8_collate (name_a, name_b);

		g_free (name_a);
		g_free (name_b);
	}

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
	if (variant == NULL) {
		return;
	}

	hostname = g_variant_get_string (variant, &len);
	if (len > 0 &&
	    g_strcmp0 (sidebar->hostname, hostname) != 0) {
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

	sidebar->hostnamed_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (error != NULL) {
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
				 G_CALLBACK (volume_added_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
				 G_CALLBACK (volume_removed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
				 G_CALLBACK (volume_changed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
				 G_CALLBACK (mount_changed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
				 G_CALLBACK (drive_disconnected_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
				 G_CALLBACK (drive_connected_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
				 G_CALLBACK (drive_changed_callback), sidebar, 0);
}

static void
bookmarks_changed_cb (gpointer data)
{
	GtkPlacesSidebar *sidebar = GTK_PLACES_SIDEBAR (data);

	update_places (sidebar);
}

static gboolean
tree_view_button_press_callback (GtkWidget *tree_view,
				 GdkEventButton *event,
				 gpointer data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view),
						   event->x, event->y,
						   &path,
						   &column,
						   NULL,
						   NULL)) {
			gtk_tree_view_row_activated (GTK_TREE_VIEW (tree_view), path, column);
		}
	}

	return FALSE;
}

static void
tree_view_set_activate_on_single_click (GtkTreeView *tree_view)
{
	g_signal_connect (tree_view, "button_press_event",
			  G_CALLBACK (tree_view_button_press_callback),
			  NULL);
}


static void
gtk_places_sidebar_init (GtkPlacesSidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeSelection  *selection;

	create_volume_monitor (sidebar);

	sidebar->bookmarks_manager = _gtk_bookmarks_manager_new (bookmarks_changed_cb, sidebar);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (sidebar)),
					      GTK_JUNCTION_RIGHT | GTK_JUNCTION_LEFT);

  	/* tree view */
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());

	/* initial padding */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	g_object_set (cell,
		      "xpad", 6,
		      NULL);

	/* headings */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_HEADING_TEXT,
					     NULL);
	g_object_set (cell,
		      "weight", PANGO_WEIGHT_BOLD,
		      "weight-set", TRUE,
		      "ypad", 6,
		      "xpad", 0,
		      NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 heading_cell_renderer_func,
						 sidebar, NULL);

	/* icon padding */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 padding_cell_renderer_func,
						 sidebar, NULL);

	/* icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, "follow-state", TRUE, NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "gicon", PLACES_SIDEBAR_COLUMN_GICON,
					     NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 icon_cell_renderer_func,
						 sidebar, NULL);

	/* eject text renderer */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_NAME,
					     "visible", PLACES_SIDEBAR_COLUMN_EJECT,
					     NULL);
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);

	/* eject icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	sidebar->eject_icon_cell_renderer = cell;
	g_object_set (cell,
		      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "stock-size", GTK_ICON_SIZE_MENU,
		      "xpad", EJECT_BUTTON_XPAD,
		      /* align right, because for some reason gtk+ expands
			 this even though we tell it not to. */
		      "xalign", 1.0,
		      "follow-state", TRUE,
		      NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "visible", PLACES_SIDEBAR_COLUMN_EJECT,
					     "gicon", PLACES_SIDEBAR_COLUMN_EJECT_GICON,
					     NULL);

	/* normal text renderer */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_NAME,
					     "visible", PLACES_SIDEBAR_COLUMN_NO_EJECT,
					     "editable-set", PLACES_SIDEBAR_COLUMN_BOOKMARK,
					     NULL);
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);

	g_signal_connect (cell, "edited",
			  G_CALLBACK (bookmarks_edited), sidebar);
	g_signal_connect (cell, "editing-canceled",
			  G_CALLBACK (bookmarks_editing_canceled), sidebar);

	/* this is required to align the eject buttons to the right */
	gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), 24);
	gtk_tree_view_append_column (tree_view, col);

	sidebar->store = shortcuts_model_new (sidebar);
	gtk_tree_view_set_tooltip_column (tree_view, PLACES_SIDEBAR_COLUMN_TOOLTIP);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sidebar->store),
					      PLACES_SIDEBAR_COLUMN_NAME,
					      GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sidebar->store),
					 PLACES_SIDEBAR_COLUMN_NAME,
					 places_sidebar_sort_func,
					 sidebar, NULL);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->store));
	gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));
	gtk_tree_view_set_enable_search (tree_view, FALSE);

	gtk_widget_show (GTK_WIDGET (sidebar));
	sidebar->tree_view = tree_view;

	gtk_tree_view_set_search_column (tree_view, PLACES_SIDEBAR_COLUMN_NAME);
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	gtk_tree_selection_set_select_function (selection,
						tree_selection_func,
						sidebar,
						NULL);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK,
						dnd_source_targets, G_N_ELEMENTS (dnd_source_targets),
						GDK_ACTION_MOVE);
	gtk_drag_dest_set (GTK_WIDGET (tree_view),
			   0,
			   dnd_drop_targets, G_N_ELEMENTS (dnd_drop_targets),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);

	g_signal_connect (tree_view, "key-press-event",
			  G_CALLBACK (bookmarks_key_press_event_cb), sidebar);

	g_signal_connect (tree_view, "drag-motion",
			  G_CALLBACK (drag_motion_callback), sidebar);
	g_signal_connect (tree_view, "drag-leave",
			  G_CALLBACK (drag_leave_callback), sidebar);
	g_signal_connect (tree_view, "drag-data-received",
			  G_CALLBACK (drag_data_received_callback), sidebar);
	g_signal_connect (tree_view, "drag-drop",
			  G_CALLBACK (drag_drop_callback), sidebar);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (bookmarks_selection_changed_cb), sidebar);
	g_signal_connect (tree_view, "popup-menu",
			  G_CALLBACK (bookmarks_popup_menu_cb), sidebar);
	g_signal_connect (tree_view, "button-release-event",
			  G_CALLBACK (bookmarks_button_release_event_cb), sidebar);

	tree_view_set_activate_on_single_click (sidebar->tree_view);

	sidebar->hostname = g_strdup (_("Computer"));
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
				  NULL,
				  "org.freedesktop.hostname1",
				  "/org/freedesktop/hostname1",
				  "org.freedesktop.hostname1",
				  NULL,
				  hostname_proxy_new_cb,
 				  sidebar);
}

static void
gtk_places_sidebar_dispose (GObject *object)
{
	GtkPlacesSidebar *sidebar;

	sidebar = GTK_PLACES_SIDEBAR (object);

	sidebar->tree_view = NULL;

	g_free (sidebar->uri);
	sidebar->uri = NULL;

	free_drag_data (sidebar);

	if (sidebar->bookmarks_manager != NULL) {
		_gtk_bookmarks_manager_free (sidebar->bookmarks_manager);
		sidebar->bookmarks_manager = NULL;
	}

	g_clear_object (&sidebar->store);

	if (sidebar->volume_monitor != NULL) {
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      volume_added_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      volume_removed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      volume_changed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      mount_added_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      mount_removed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      mount_changed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      drive_disconnected_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      drive_connected_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor, 
						      drive_changed_callback, sidebar);

		g_clear_object (&sidebar->volume_monitor);
	}

	g_clear_object (&sidebar->hostnamed_proxy);
	g_free (sidebar->hostname);
	sidebar->hostname = NULL;

	G_OBJECT_CLASS (gtk_places_sidebar_parent_class)->dispose (object);
}

static void
gtk_places_sidebar_class_init (GtkPlacesSidebarClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = (GObjectClass *) class;

	gobject_class->dispose = gtk_places_sidebar_dispose;

	GTK_WIDGET_CLASS (class)->style_set = gtk_places_sidebar_style_set;
	GTK_WIDGET_CLASS (class)->focus = gtk_places_sidebar_focus;

	/* FIXME: add docstrings for the signals */

	places_sidebar_signals [LOCATION_SELECTED] =
		g_signal_new (I_("location-selected"),
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkPlacesSidebarClass, location_selected),
			      NULL, NULL,
			      _gtk_marshal_VOID__OBJECT_ENUM,
			      G_TYPE_NONE, 2,
			      G_TYPE_OBJECT,
			      GTK_TYPE_PLACES_OPEN_MODE);

	places_sidebar_signals [EMPTY_TRASH_REQUESTED] =
		g_signal_new (I_("empty-trash-requested"),
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkPlacesSidebarClass, empty_trash_requested),
			      NULL, NULL,
			      _gtk_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	places_sidebar_signals [INITIATED_UNMOUNT] =
		g_signal_new (I_("initiated-unmount"),
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkPlacesSidebarClass, initiated_unmount),
			      NULL, NULL,
			      _gtk_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1,
			      G_TYPE_BOOLEAN);

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
}

static void
gtk_places_sidebar_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	GtkPlacesSidebar *sidebar;

	sidebar = GTK_PLACES_SIDEBAR (widget);

	update_places (sidebar);
}

GtkWidget *
gtk_places_sidebar_new (void)
{
	return GTK_WIDGET (g_object_new (gtk_places_sidebar_get_type (), NULL));
}



/* Drag and drop interfaces */

/* GtkTreeDragSource::row_draggable implementation for the shortcuts model */
static gboolean
shortcuts_model_row_draggable (GtkTreeDragSource *drag_source,
			       GtkTreePath       *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	PlaceType place_type;
	SectionType section_type;

	model = GTK_TREE_MODEL (drag_source);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
			    -1);

	if (place_type != PLACES_HEADING && section_type == SECTION_BOOKMARKS)
		return TRUE;

	return FALSE;
}

/* Fill the GtkTreeDragSourceIface vtable */
static void
shortcuts_model_class_init (ShortcutsModelClass *klass)
{
}

static void
shortcuts_model_init (ShortcutsModel *model)
{
	model->sidebar = NULL;
}

static void
shortcuts_model_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = shortcuts_model_row_draggable;
}

static GtkListStore *
shortcuts_model_new (GtkPlacesSidebar *sidebar)
{
	ShortcutsModel *model;
	GType model_types[PLACES_SIDEBAR_COLUMN_COUNT] = {
		G_TYPE_INT, 
		G_TYPE_STRING,
		G_TYPE_DRIVE,
		G_TYPE_VOLUME,
		G_TYPE_MOUNT,
		G_TYPE_STRING,
		G_TYPE_ICON,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_STRING,
		G_TYPE_ICON,
		G_TYPE_INT,
		G_TYPE_STRING
	};

	model = g_object_new (shortcuts_model_get_type (), NULL);
	model->sidebar = sidebar;

	gtk_list_store_set_column_types (GTK_LIST_STORE (model),
					 PLACES_SIDEBAR_COLUMN_COUNT,
					 model_types);

	return GTK_LIST_STORE (model);
}



/* Public methods for GtkPlacesSidebar */

/**
 * gtk_places_sidebar_set_current_uri:
 * @sidebar: a places sidebar
 * @uri: URI to select, or #NULL for no current path
 *
 * Sets the URI that is being shown in the widgets surrounding the @sidebar.  In turn,
 * it will highlight that URI if it is being shown in the list of places, or it will
 * unhighlight everything if the URI is not among the places in the list.
 */
void
gtk_places_sidebar_set_current_uri (GtkPlacesSidebar *sidebar, const char *uri)
{
	GtkTreeSelection *selection;
	GtkTreeIter 	 iter;
	gboolean 	 valid;
	char  		 *iter_uri;

	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	selection = gtk_tree_view_get_selection (sidebar->tree_view);

	if (uri == NULL) {
		g_free (sidebar->uri);
		sidebar->uri = NULL;

		gtk_tree_selection_unselect_all (selection);
		return;
	}

        if (sidebar->uri == NULL || strcmp (sidebar->uri, uri) != 0) {
		g_free (sidebar->uri);
                sidebar->uri = g_strdup (uri);

		/* set selection if any place matches the uri */
		gtk_tree_selection_unselect_all (selection);
  		valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store), &iter);

		while (valid) {
			gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
		 		       	    PLACES_SIDEBAR_COLUMN_URI, &iter_uri,
					    -1);
			if (iter_uri != NULL) {
				if (strcmp (iter_uri, uri) == 0) {
					g_free (iter_uri);
					gtk_tree_selection_select_iter (selection, &iter);
					break;
				}
				g_free (iter_uri);
			}
        	 	valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (sidebar->store), &iter);
		}
    	}
}

/**
 * gtk_places_sidebar_set_multiple_tabs_supported:
 * @sidebar: a places sidebar
 * @supported: whether the appliacation supports multiple notebook tabs for file browsing
 *
 * Sets whether the calling appliacation supports multiple tabs for file
 * browsing; this is off by default.  When @supported is #TRUE, the context menu
 * for the @sidebar's items will show items relevant to opening folders in new
 * tabs.
 */
void
gtk_places_sidebar_set_multiple_tabs_supported (GtkPlacesSidebar *sidebar, gboolean supported)
{
	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->multiple_tabs_supported = !!supported;
	bookmarks_popup_menu_detach_cb (GTK_WIDGET (sidebar), NULL);
}

/**
 * gtk_places_sidebar_set_multiple_windows_supported:
 * @sidebar: a places sidebar
 * @supported: whether the appliacation supports multiple windows for file browsing
 *
 * Sets whether the calling appliacation supports multiple windows for file
 * browsing; this is off by default.  When @supported is #TRUE, the context menu
 * for the @sidebar's items will show items relevant to opening folders in new
 * windows.
 */
void
gtk_places_sidebar_set_multiple_windows_supported (GtkPlacesSidebar *sidebar, gboolean supported)
{
	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->multiple_windows_supported = !!supported;
	bookmarks_popup_menu_detach_cb (GTK_WIDGET (sidebar), NULL);
}

/**
 * gtk_places_sidebar_set_show_desktop:
 * @sidebar: a places sidebar
 * @show_desktop: whether to show an item for the Desktop folder
 *
 * Sets whether the @sidebar should show an item for the Desktop folder; this is off by default.
 * An application may want to turn this on if the desktop environment actually supports the
 * notion of a desktop.
 */
void
gtk_places_sidebar_set_show_desktop (GtkPlacesSidebar *sidebar, gboolean show_desktop)
{
	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->show_desktop = !!show_desktop;
	update_places (sidebar);
}

void
gtk_places_sidebar_set_show_properties (GtkPlacesSidebar *sidebar, gboolean show_properties)
{
	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->show_properties = !!show_properties;
	bookmarks_check_popup_sensitivity (sidebar);
}

void
gtk_places_sidebar_set_show_trash (GtkPlacesSidebar *sidebar, gboolean show_trash)
{
	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->show_trash = !!show_trash;
	update_places (sidebar);
}

void
gtk_places_sidebar_set_trash_is_full (GtkPlacesSidebar *sidebar, gboolean is_full)
{
	g_return_if_fail (GTK_IS_PLACES_SIDEBAR (sidebar));

	sidebar->trash_is_full = !!is_full;
	update_places (sidebar);
	bookmarks_check_popup_sensitivity (sidebar);
}
