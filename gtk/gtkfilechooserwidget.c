/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechooserwidget.c: Embeddable file selector widget
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* TODO:
 *
 * * Fix FIXME-places-sidebar
 */

#include "config.h"

#include "gtkfilechooserwidget.h"

#include "gtkbindings.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkcheckmenuitem.h"
#include "gtkclipboard.h"
#include "gtkcomboboxtext.h"
#include "gtkentry.h"
#include "gtkstack.h"
#include "gtkexpander.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserembed.h"
#include "gtkfilechooserentry.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilesystem.h"
#include "gtkfilesystemmodel.h"
#include "gtkframe.h"
#include "gtkgrid.h"
#include "deprecated/gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "deprecated/gtkimagemenuitem.h"
#include "gtkinfobar.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkmessagedialog.h"
#include "gtkmountoperation.h"
#include "gtkpaned.h"
#include "gtkpathbar.h"
#include "gtkplacessidebar.h"
#include "gtkprivate.h"
#include "gtkrecentmanager.h"
#include "gtkscrolledwindow.h"
#include "gtksearchentry.h"
#include "gtkseparatormenuitem.h"
#include "gtksettings.h"
#include "gtksizegroup.h"
#include "gtksizerequest.h"
#include "gtktogglebutton.h"
#include "gtktoolbar.h"
#include "gtktoolbutton.h"
#include "gtktooltip.h"
#include "gtktreednd.h"
#include "gtktreeprivate.h"
#include "gtktreeselection.h"
#include "gtkbox.h"
#include "gtkorientable.h"
#include "gtkwindowgroup.h"
#include "gtkintl.h"
#include "gtkshow.h"
#include "gtkmain.h"
#include "gtkscrollable.h"
#include "gtkadjustment.h"
#include "gtkpopover.h"

#include <cairo-gobject.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <locale.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

/**
 * SECTION:gtkfilechooserwidget
 * @Short_description: A file chooser widget
 * @Title: GtkFileChooserWidget
 * @See_also: #GtkFileChooserDialog
 *
 * #GtkFileChooserWidget is a widget for choosing files.
 * It exposes the #GtkFileChooser interface, and you should
 * use the methods of this interface to interact with the
 * widget.
 */


/* Values for GtkSelection-related "info" fields */
#define SELECTION_TEXT 0
#define SELECTION_URI  1

/* 150 mseconds of delay */
#define LOCATION_CHANGED_TIMEOUT 150

/* Profiling stuff */
#undef PROFILE_FILE_CHOOSER
#ifdef PROFILE_FILE_CHOOSER


#ifndef F_OK 
#define F_OK 0
#endif

#define PROFILE_INDENT 4

static int profile_indent;

static void
profile_add_indent (int indent)
{
  profile_indent += indent;
  if (profile_indent < 0)
    g_error ("You screwed up your indentation");
}

void
_gtk_file_chooser_profile_log (const char *func, int indent, const char *msg1, const char *msg2)
{
  char *str;

  if (indent < 0)
    profile_add_indent (indent);

  if (profile_indent == 0)
    str = g_strdup_printf ("MARK: %s %s %s", func ? func : "", msg1 ? msg1 : "", msg2 ? msg2 : "");
  else
    str = g_strdup_printf ("MARK: %*c %s %s %s", profile_indent - 1, ' ', func ? func : "", msg1 ? msg1 : "", msg2 ? msg2 : "");

  access (str, F_OK);
  g_free (str);

  if (indent > 0)
    profile_add_indent (indent);
}

#define profile_start(x, y) _gtk_file_chooser_profile_log (G_STRFUNC, PROFILE_INDENT, x, y)
#define profile_end(x, y) _gtk_file_chooser_profile_log (G_STRFUNC, -PROFILE_INDENT, x, y)
#define profile_msg(x, y) _gtk_file_chooser_profile_log (NULL, 0, x, y)
#else
#define profile_start(x, y)
#define profile_end(x, y)
#define profile_msg(x, y)
#endif

enum {
  PROP_SEARCH_MODE = 1
};

typedef enum {
  LOAD_EMPTY,			/* There is no model */
  LOAD_PRELOAD,			/* Model is loading and a timer is running; model isn't inserted into the tree yet */
  LOAD_LOADING,			/* Timeout expired, model is inserted into the tree, but not fully loaded yet */
  LOAD_FINISHED			/* Model is fully loaded and inserted into the tree */
} LoadState;

typedef enum {
  RELOAD_EMPTY,			/* No folder has been set */
  RELOAD_HAS_FOLDER		/* We have a folder, although it may not be completely loaded yet; no need to reload */
} ReloadState;

typedef enum {
  LOCATION_MODE_PATH_BAR,
  LOCATION_MODE_FILENAME_ENTRY
} LocationMode;

typedef enum {
  OPERATION_MODE_BROWSE,
  OPERATION_MODE_SEARCH,
  OPERATION_MODE_ENTER_LOCATION,
  OPERATION_MODE_RECENT
} OperationMode;

typedef enum {
  STARTUP_MODE_RECENT,
  STARTUP_MODE_CWD
} StartupMode;

struct _GtkFileChooserWidgetPrivate {
  GtkFileChooserAction action;

  GtkFileSystem *file_system;

  /* Save mode widgets */
  GtkWidget *save_widgets;
  GtkWidget *save_widgets_table;

  /* The file browsing widgets */
  GtkWidget *browse_widgets_box;
  GtkWidget *browse_widgets_hpaned;
  GtkWidget *browse_header_box;
  GtkWidget *browse_header_stack;
  GtkWidget *browse_files_box;
  GtkWidget *browse_files_stack;
  GtkWidget *browse_files_tree_view;
  GtkWidget *browse_files_popup_menu;
  GtkWidget *browse_files_popup_menu_add_shortcut_item;
  GtkWidget *browse_files_popup_menu_hidden_files_item;
  GtkWidget *browse_files_popup_menu_size_column_item;
  GtkWidget *browse_files_popup_menu_copy_file_location_item;
  GtkWidget *browse_files_popup_menu_visit_file_item;
  GtkWidget *browse_files_popup_menu_open_folder_item;
  GtkWidget *browse_files_popup_menu_sort_directories_item;
  GtkWidget *browse_new_folder_button;
  GtkWidget *browse_path_bar_hbox;
  GtkSizeGroup *browse_path_bar_size_group;
  GtkWidget *browse_path_bar;
  GtkWidget *new_folder_name_entry;
  GtkWidget *new_folder_create_button;
  GtkWidget *new_folder_error_label;
  GtkWidget *new_folder_popover;

  GtkFileSystemModel *browse_files_model;
  char *browse_files_last_selected_name;

  GtkWidget *places_sidebar;
  StartupMode startup_mode;

  /* OPERATION_MODE_SEARCH */
  GtkWidget *search_entry;
  GtkWidget *current_location_radio;
  GtkSearchEngine *search_engine;
  GtkQuery *search_query;
  GtkFileSystemModel *search_model;
  gboolean search_model_empty;

  /* OPERATION_MODE_RECENT */
  GtkRecentManager *recent_manager;
  GtkFileSystemModel *recent_model;
  guint load_recent_id;

  GtkWidget *extra_and_filters;
  GtkWidget *filter_combo_hbox;
  GtkWidget *filter_combo;
  GtkWidget *preview_box;
  GtkWidget *preview_label;
  GtkWidget *preview_widget;
  GtkWidget *extra_align;
  GtkWidget *extra_widget;

  GtkWidget *location_entry_box;
  GtkWidget *location_entry;
  LocationMode location_mode;

  /* Handles */
  GCancellable *file_list_drag_data_received_cancellable;
  GCancellable *update_current_folder_cancellable;
  GCancellable *should_respond_get_info_cancellable;
  GCancellable *file_exists_get_info_cancellable;

  LoadState load_state;
  ReloadState reload_state;
  guint load_timeout_id;

  OperationMode operation_mode;

  GSList *pending_select_files;

  GtkFileFilter *current_filter;
  GSList *filters;

  GtkBookmarksManager *bookmarks_manager;

  int num_volumes;
  int num_shortcuts;
  int num_bookmarks;

  gulong volumes_changed_id;
  gulong bookmarks_changed_id;

  GFile *current_volume_file;
  GFile *current_folder;
  GFile *preview_file;
  char *preview_display_name;

  GtkTreeViewColumn *list_name_column;
  GtkCellRenderer *list_name_renderer;
  GtkCellRenderer *list_pixbuf_renderer;
  GtkTreeViewColumn *list_mtime_column;
  GtkTreeViewColumn *list_size_column;
  GtkTreeViewColumn *list_location_column;

  guint location_changed_id;

  gulong settings_signal_id;
  int icon_size;

  GSource *focus_entry_idle;

  gulong toplevel_set_focus_id;
  GtkWidget *toplevel_last_focus_widget;

  gint sort_column;
  GtkSortType sort_order;

  /* Flags */

  guint local_only : 1;
  guint preview_widget_active : 1;
  guint use_preview_label : 1;
  guint select_multiple : 1;
  guint show_hidden : 1;
  guint sort_directories_first : 1;
  guint do_overwrite_confirmation : 1;
  guint list_sort_ascending : 1;
  guint shortcuts_current_folder_active : 1;
  guint show_size_column : 1;
  guint create_folders : 1;
  guint auto_selecting_first_row : 1;
};

#define MAX_LOADING_TIME 500

#define DEFAULT_NEW_FOLDER_NAME _("Type name of new folder")

/* Signal IDs */
enum {
  LOCATION_POPUP,
  LOCATION_POPUP_ON_PASTE,
  UP_FOLDER,
  DOWN_FOLDER,
  HOME_FOLDER,
  DESKTOP_FOLDER,
  QUICK_BOOKMARK,
  LOCATION_TOGGLE_POPUP,
  SHOW_HIDDEN,
  SEARCH_SHORTCUT,
  RECENT_SHORTCUT,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define MODEL_ATTRIBUTES "standard::name,standard::type,standard::display-name," \
                         "standard::is-hidden,standard::is-backup,standard::size," \
                         "standard::content-type,time::modified"
enum {
  /* the first 3 must be these due to settings caching sort column */
  MODEL_COL_NAME,
  MODEL_COL_SIZE,
  MODEL_COL_MTIME,
  MODEL_COL_FILE,
  MODEL_COL_NAME_COLLATED,
  MODEL_COL_IS_FOLDER,
  MODEL_COL_IS_SENSITIVE,
  MODEL_COL_SURFACE,
  MODEL_COL_SIZE_TEXT,
  MODEL_COL_MTIME_TEXT,
  MODEL_COL_LOCATION_TEXT,
  MODEL_COL_ELLIPSIZE,
  MODEL_COL_NUM_COLUMNS
};

/* This list of types is passed to _gtk_file_system_model_new*() */
#define MODEL_COLUMN_TYPES					\
	MODEL_COL_NUM_COLUMNS,					\
	G_TYPE_STRING,		  /* MODEL_COL_NAME */		\
	G_TYPE_INT64,		  /* MODEL_COL_SIZE */		\
	G_TYPE_LONG,		  /* MODEL_COL_MTIME */		\
	G_TYPE_FILE,		  /* MODEL_COL_FILE */		\
	G_TYPE_STRING,		  /* MODEL_COL_NAME_COLLATED */	\
	G_TYPE_BOOLEAN,		  /* MODEL_COL_IS_FOLDER */	\
	G_TYPE_BOOLEAN,		  /* MODEL_COL_IS_SENSITIVE */	\
	CAIRO_GOBJECT_TYPE_SURFACE,  /* MODEL_COL_SURFACE */	\
	G_TYPE_STRING,		  /* MODEL_COL_SIZE_TEXT */	\
	G_TYPE_STRING,		  /* MODEL_COL_MTIME_TEXT */	\
	G_TYPE_STRING,		  /* MODEL_COL_LOCATION_TEXT */	\
	PANGO_TYPE_ELLIPSIZE_MODE /* MODEL_COL_ELLIPSIZE */

/* Identifiers for target types */
enum {
  GTK_TREE_MODEL_ROW,
};

#define DEFAULT_RECENT_FILES_LIMIT 50

/* Icon size for if we can't get it from the theme */
#define FALLBACK_ICON_SIZE 16

#define PREVIEW_HBOX_SPACING 12
#define NUM_LINES 45
#define NUM_CHARS 60

static void gtk_file_chooser_widget_iface_init       (GtkFileChooserIface        *iface);
static void gtk_file_chooser_embed_default_iface_init (GtkFileChooserEmbedIface   *iface);

static void     gtk_file_chooser_widget_constructed  (GObject               *object);
static void     gtk_file_chooser_widget_finalize     (GObject               *object);
static void     gtk_file_chooser_widget_set_property (GObject               *object,
						       guint                  prop_id,
						       const GValue          *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_widget_get_property (GObject               *object,
						       guint                  prop_id,
						       GValue                *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_widget_dispose      (GObject               *object);
static void     gtk_file_chooser_widget_show_all       (GtkWidget             *widget);
static void     gtk_file_chooser_widget_realize        (GtkWidget             *widget);
static void     gtk_file_chooser_widget_map            (GtkWidget             *widget);
static void     gtk_file_chooser_widget_unmap          (GtkWidget             *widget);
static void     gtk_file_chooser_widget_hierarchy_changed (GtkWidget          *widget,
							    GtkWidget          *previous_toplevel);
static void     gtk_file_chooser_widget_style_updated  (GtkWidget             *widget);
static void     gtk_file_chooser_widget_screen_changed (GtkWidget             *widget,
                                                        GdkScreen             *previous_screen);

static gboolean       gtk_file_chooser_widget_set_current_folder 	   (GtkFileChooser    *chooser,
									    GFile             *folder,
									    GError           **error);
static gboolean       gtk_file_chooser_widget_update_current_folder 	   (GtkFileChooser    *chooser,
									    GFile             *folder,
									    gboolean           keep_trail,
									    gboolean           clear_entry,
									    GError           **error);
static GFile *        gtk_file_chooser_widget_get_current_folder 	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_widget_set_current_name   	   (GtkFileChooser    *chooser,
									    const gchar       *name);
static gchar *        gtk_file_chooser_widget_get_current_name   	   (GtkFileChooser    *chooser);
static gboolean       gtk_file_chooser_widget_select_file        	   (GtkFileChooser    *chooser,
									    GFile             *file,
									    GError           **error);
static void           gtk_file_chooser_widget_unselect_file      	   (GtkFileChooser    *chooser,
									    GFile             *file);
static void           gtk_file_chooser_widget_select_all         	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_widget_unselect_all       	   (GtkFileChooser    *chooser);
static GSList *       gtk_file_chooser_widget_get_files          	   (GtkFileChooser    *chooser);
static GFile *        gtk_file_chooser_widget_get_preview_file   	   (GtkFileChooser    *chooser);
static GtkFileSystem *gtk_file_chooser_widget_get_file_system    	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_widget_add_filter         	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static void           gtk_file_chooser_widget_remove_filter      	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static GSList *       gtk_file_chooser_widget_list_filters       	   (GtkFileChooser    *chooser);
static gboolean       gtk_file_chooser_widget_add_shortcut_folder    (GtkFileChooser    *chooser,
								       GFile             *file,
								       GError           **error);
static gboolean       gtk_file_chooser_widget_remove_shortcut_folder (GtkFileChooser    *chooser,
								       GFile             *file,
								       GError           **error);
static GSList *       gtk_file_chooser_widget_list_shortcut_folders  (GtkFileChooser    *chooser);

static void           gtk_file_chooser_widget_get_default_size       (GtkFileChooserEmbed *chooser_embed,
								       gint                *default_width,
								       gint                *default_height);
static gboolean       gtk_file_chooser_widget_should_respond         (GtkFileChooserEmbed *chooser_embed);
static void           gtk_file_chooser_widget_initial_focus          (GtkFileChooserEmbed *chooser_embed);

static void add_selection_to_recent_list (GtkFileChooserWidget *impl);

static void location_popup_handler  (GtkFileChooserWidget *impl,
				     const gchar           *path);
static void location_popup_on_paste_handler (GtkFileChooserWidget *impl);
static void location_toggle_popup_handler   (GtkFileChooserWidget *impl);
static void up_folder_handler       (GtkFileChooserWidget *impl);
static void down_folder_handler     (GtkFileChooserWidget *impl);
static void home_folder_handler     (GtkFileChooserWidget *impl);
static void desktop_folder_handler  (GtkFileChooserWidget *impl);
static void quick_bookmark_handler  (GtkFileChooserWidget *impl,
				     gint                   bookmark_index);
static void show_hidden_handler     (GtkFileChooserWidget *impl);
static void search_shortcut_handler (GtkFileChooserWidget *impl);
static void recent_shortcut_handler (GtkFileChooserWidget *impl);
static void update_appearance       (GtkFileChooserWidget *impl);

static void operation_mode_set (GtkFileChooserWidget *impl, OperationMode mode);
static void location_mode_set  (GtkFileChooserWidget *impl, LocationMode new_mode);

static void set_current_filter   (GtkFileChooserWidget *impl,
				  GtkFileFilter         *filter);
static void check_preview_change (GtkFileChooserWidget *impl);

static void filter_combo_changed       (GtkComboBox           *combo_box,
					GtkFileChooserWidget *impl);

static gboolean list_select_func   (GtkTreeSelection      *selection,
				    GtkTreeModel          *model,
				    GtkTreePath           *path,
				    gboolean               path_currently_selected,
				    gpointer               data);

static void list_selection_changed     (GtkTreeSelection      *tree_selection,
					GtkFileChooserWidget  *impl);
static void list_row_activated         (GtkTreeView           *tree_view,
					GtkTreePath           *path,
					GtkTreeViewColumn     *column,
					GtkFileChooserWidget  *impl);
static void list_cursor_changed        (GtkTreeView           *treeview,
                                        GtkFileChooserWidget  *impl);

static void path_bar_clicked (GtkPathBar            *path_bar,
			      GFile                 *file,
			      GFile                 *child,
                              gboolean               child_is_hidden,
                              GtkFileChooserWidget *impl);

static void update_cell_renderer_attributes (GtkFileChooserWidget *impl);

static void load_remove_timer (GtkFileChooserWidget *impl, LoadState new_load_state);
static void browse_files_center_selected_row (GtkFileChooserWidget *impl);

static void location_switch_to_path_bar (GtkFileChooserWidget *impl);

static void stop_loading_and_clear_list_model (GtkFileChooserWidget *impl,
                                               gboolean remove_from_treeview);

static void     search_setup_widgets         (GtkFileChooserWidget *impl);
static void     search_stop_searching        (GtkFileChooserWidget *impl,
                                              gboolean               remove_query);
static void     search_clear_model           (GtkFileChooserWidget *impl, 
					      gboolean               remove_from_treeview);
static GSList  *search_get_selected_files    (GtkFileChooserWidget *impl);
static void     search_entry_activate_cb     (GtkFileChooserWidget *impl);
static void     search_entry_stop_cb         (GtkFileChooserWidget *impl);
static void     settings_load                (GtkFileChooserWidget *impl);

static void     show_filters                 (GtkFileChooserWidget *impl,
                                              gboolean               show);

static void     recent_start_loading         (GtkFileChooserWidget *impl);
static void     recent_stop_loading          (GtkFileChooserWidget *impl);
static void     recent_clear_model           (GtkFileChooserWidget *impl,
                                              gboolean               remove_from_treeview);
static gboolean recent_should_respond        (GtkFileChooserWidget *impl);
static GSList * recent_get_selected_files    (GtkFileChooserWidget *impl);
static void     set_file_system_backend      (GtkFileChooserWidget *impl);
static void     unset_file_system_backend    (GtkFileChooserWidget *impl);



G_DEFINE_TYPE_WITH_CODE (GtkFileChooserWidget, gtk_file_chooser_widget, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GtkFileChooserWidget)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
						gtk_file_chooser_widget_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER_EMBED,
						gtk_file_chooser_embed_default_iface_init));						

static void
gtk_file_chooser_widget_iface_init (GtkFileChooserIface *iface)
{
  iface->select_file = gtk_file_chooser_widget_select_file;
  iface->unselect_file = gtk_file_chooser_widget_unselect_file;
  iface->select_all = gtk_file_chooser_widget_select_all;
  iface->unselect_all = gtk_file_chooser_widget_unselect_all;
  iface->get_files = gtk_file_chooser_widget_get_files;
  iface->get_preview_file = gtk_file_chooser_widget_get_preview_file;
  iface->get_file_system = gtk_file_chooser_widget_get_file_system;
  iface->set_current_folder = gtk_file_chooser_widget_set_current_folder;
  iface->get_current_folder = gtk_file_chooser_widget_get_current_folder;
  iface->set_current_name = gtk_file_chooser_widget_set_current_name;
  iface->get_current_name = gtk_file_chooser_widget_get_current_name;
  iface->add_filter = gtk_file_chooser_widget_add_filter;
  iface->remove_filter = gtk_file_chooser_widget_remove_filter;
  iface->list_filters = gtk_file_chooser_widget_list_filters;
  iface->add_shortcut_folder = gtk_file_chooser_widget_add_shortcut_folder;
  iface->remove_shortcut_folder = gtk_file_chooser_widget_remove_shortcut_folder;
  iface->list_shortcut_folders = gtk_file_chooser_widget_list_shortcut_folders;
}

static void
gtk_file_chooser_embed_default_iface_init (GtkFileChooserEmbedIface *iface)
{
  iface->get_default_size = gtk_file_chooser_widget_get_default_size;
  iface->should_respond = gtk_file_chooser_widget_should_respond;
  iface->initial_focus = gtk_file_chooser_widget_initial_focus;
}

static void
pending_select_files_free (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_slist_free_full (priv->pending_select_files, g_object_unref);
  priv->pending_select_files = NULL;
}

static void
pending_select_files_add (GtkFileChooserWidget *impl,
			  GFile                 *file)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  priv->pending_select_files =
    g_slist_prepend (priv->pending_select_files, g_object_ref (file));
}

static void
gtk_file_chooser_widget_finalize (GObject *object)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (object);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GSList *l;

  if (priv->location_changed_id > 0)
    g_source_remove (priv->location_changed_id);

  unset_file_system_backend (impl);

  g_free (priv->browse_files_last_selected_name);

  for (l = priv->filters; l; l = l->next)
    {
      GtkFileFilter *filter;

      filter = GTK_FILE_FILTER (l->data);
      g_object_unref (filter);
    }
  g_slist_free (priv->filters);

  if (priv->current_filter)
    g_object_unref (priv->current_filter);

  if (priv->current_volume_file)
    g_object_unref (priv->current_volume_file);

  if (priv->current_folder)
    g_object_unref (priv->current_folder);

  if (priv->preview_file)
    g_object_unref (priv->preview_file);

  if (priv->browse_path_bar_size_group)
    g_object_unref (priv->browse_path_bar_size_group);

  /* Free all the Models we have */
  stop_loading_and_clear_list_model (impl, FALSE);
  search_clear_model (impl, FALSE);
  recent_clear_model (impl, FALSE);

  /* stopping the load above should have cleared this */
  g_assert (priv->load_timeout_id == 0);

  g_free (priv->preview_display_name);

  impl->priv = NULL;

  G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->finalize (object);
}

/* Shows an error dialog set as transient for the specified window */
static void
error_message_with_parent (GtkWindow  *parent,
			   const char *msg,
			   const char *detail)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_OK,
				   "%s",
				   msg);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					    "%s", detail);

  if (parent && gtk_window_has_group (parent))
    gtk_window_group_add_window (gtk_window_get_group (parent),
                                 GTK_WINDOW (dialog));

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
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

/* Shows an error dialog for the file chooser */
static void
error_message (GtkFileChooserWidget *impl,
	       const char            *msg,
	       const char            *detail)
{
  error_message_with_parent (get_toplevel (GTK_WIDGET (impl)), msg, detail);
}

/* Shows a simple error dialog relative to a path.  Frees the GError as well. */
static void
error_dialog (GtkFileChooserWidget *impl,
	      const char            *msg,
	      GFile                 *file,
	      GError                *error)
{
  if (error)
    {
      char *uri = NULL;
      char *text;

      if (file)
	uri = g_file_get_uri (file);
      text = g_strdup_printf (msg, uri);
      error_message (impl, text, error->message);
      g_free (text);
      g_free (uri);
      g_error_free (error);
    }
}

/* Shows an error dialog about not being able to create a folder */
static void
error_creating_folder_dialog (GtkFileChooserWidget *impl,
			      GFile                 *file,
			      GError                *error)
{
  error_dialog (impl, 
		_("The folder could not be created"), 
		file, error);
}

/* Shows an error about not being able to create a folder because a file with
 * the same name is already there.
 */
static void
error_creating_folder_over_existing_file_dialog (GtkFileChooserWidget *impl,
						 GFile                 *file,
						 GError                *error)
{
  error_dialog (impl,
		_("The folder could not be created, as a file with the same "
                  "name already exists.  Try using a different name for the "
                  "folder, or rename the file first."),
		file, error);
}

static void
error_with_file_under_nonfolder (GtkFileChooserWidget *impl,
				 GFile *parent_file)
{
  GError *error;

  error = NULL;
  g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY,
		       _("You need to choose a valid filename."));

  error_dialog (impl,
		_("Cannot create a file under %s as it is not a folder"),
		parent_file, error);
}

static void
error_filename_to_long_dialog (GtkFileChooserWidget *impl)
{
  error_message (impl,
                 _("Cannot create file as the filename is too long"),
                 _("Try using a shorter name."));
}

/* Shows an error about not being able to select a folder because a file with
 * the same name is already there.
 */
static void
error_selecting_folder_over_existing_file_dialog (GtkFileChooserWidget *impl)
{
  error_message (impl,
                 _("You may only select folders"),
                 _("The item that you selected is not a folder try using a different item."));
}

/* Shows an error dialog about not being able to create a filename */
static void
error_building_filename_dialog (GtkFileChooserWidget *impl,
				GError                *error)
{
  error_dialog (impl, _("Invalid file name"), 
		NULL, error);
}

/* Shows an error dialog when we cannot switch to a folder */
static void
error_changing_folder_dialog (GtkFileChooserWidget *impl,
			      GFile                 *file,
			      GError                *error)
{
  error_dialog (impl, _("The folder contents could not be displayed"),
		file, error);
}

/* Changes folders, displaying an error dialog if this fails */
static gboolean
change_folder_and_display_error (GtkFileChooserWidget *impl,
				 GFile                 *file,
				 gboolean               clear_entry)
{
  GError *error;
  gboolean result;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  /* We copy the path because of this case:
   *
   * list_row_activated()
   *   fetches path from model; path belongs to the model (*)
   *   calls change_folder_and_display_error()
   *     calls gtk_file_chooser_set_current_folder_file()
   *       changing folders fails, sets model to NULL, thus freeing the path in (*)
   */

  error = NULL;
  result = gtk_file_chooser_widget_update_current_folder (GTK_FILE_CHOOSER (impl), file, TRUE, clear_entry, &error);

  if (!result)
    error_changing_folder_dialog (impl, file, error);

  return result;
}

static void
emit_default_size_changed (GtkFileChooserWidget *impl)
{
  profile_msg ("    emit default-size-changed start", NULL);
  g_signal_emit_by_name (impl, "default-size-changed");
  profile_msg ("    emit default-size-changed end", NULL);
}

static void
update_preview_widget_visibility (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->use_preview_label)
    {
      if (!priv->preview_label)
	{
	  priv->preview_label = gtk_label_new (priv->preview_display_name);
	  gtk_box_pack_start (GTK_BOX (priv->preview_box), priv->preview_label, FALSE, FALSE, 0);
	  gtk_box_reorder_child (GTK_BOX (priv->preview_box), priv->preview_label, 0);
	  gtk_label_set_ellipsize (GTK_LABEL (priv->preview_label), PANGO_ELLIPSIZE_MIDDLE);
	  gtk_widget_show (priv->preview_label);
	}
    }
  else
    {
      if (priv->preview_label)
	{
	  gtk_widget_destroy (priv->preview_label);
	  priv->preview_label = NULL;
	}
    }

  if (priv->preview_widget_active && priv->preview_widget)
    gtk_widget_show (priv->preview_box);
  else
    gtk_widget_hide (priv->preview_box);

  if (!gtk_widget_get_mapped (GTK_WIDGET (impl)))
    emit_default_size_changed (impl);
}

static void
set_preview_widget (GtkFileChooserWidget *impl,
		    GtkWidget             *preview_widget)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (preview_widget == priv->preview_widget)
    return;

  if (priv->preview_widget)
    gtk_container_remove (GTK_CONTAINER (priv->preview_box),
			  priv->preview_widget);

  priv->preview_widget = preview_widget;
  if (priv->preview_widget)
    {
      gtk_widget_show (priv->preview_widget);
      gtk_box_pack_start (GTK_BOX (priv->preview_box), priv->preview_widget, TRUE, TRUE, 0);
      gtk_box_reorder_child (GTK_BOX (priv->preview_box),
			     priv->preview_widget,
			     (priv->use_preview_label && priv->preview_label) ? 1 : 0);
    }

  update_preview_widget_visibility (impl);
}

static void
new_folder_popover_active (GtkWidget            *button,
                           GParamSpec           *pspec,
                           GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->new_folder_name_entry), "");
  gtk_widget_set_sensitive (priv->new_folder_create_button, FALSE);
  gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label), "");
}

struct FileExistsData
{
  GtkFileChooserWidget *impl;
  gboolean file_exists_and_is_not_folder;
  GFile *parent_file;
  GFile *file;
};

static void
name_exists_get_info_cb (GCancellable *cancellable,
                         GFileInfo    *info,
                         const GError *error,
                         gpointer      user_data)
{
  struct FileExistsData *data = user_data;
  GtkFileChooserWidget *impl = data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (cancellable != priv->file_exists_get_info_cancellable)
    goto out;

  priv->file_exists_get_info_cancellable = NULL;

  if (g_cancellable_is_cancelled (cancellable))
    goto out;

  if (info != NULL)
    {
      const gchar *msg;

      if (_gtk_file_info_consider_as_directory (info))
        msg = _("A folder with that name already exists");
      else
        msg = _("A file with that name already exists");

      gtk_widget_set_sensitive (priv->new_folder_create_button, FALSE);
      gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label), msg);
    }
  else
    {
      gtk_widget_set_sensitive (priv->new_folder_create_button, TRUE);
      gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label), "");
    }

out:
  g_object_unref (impl);
  g_object_unref (data->file);
  g_object_unref (data->parent_file);
  g_free (data);
  g_object_unref (cancellable);
}

static void
check_valid_folder_name (GtkFileChooserWidget *impl,
                         const gchar          *name)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_widget_set_sensitive (priv->new_folder_create_button, FALSE);

  if (name[0] == '\0')
    gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label), "");
  else if (strcmp (name, ".") == 0)
    gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label),
                        _("A folder cannot be called “.”"));
  else if (strcmp (name, "..") == 0)
    gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label),
                        _("A folder cannot be called “..”"));
  else if (strchr (name, '/') != NULL)
    gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label),
                        _("Folder names cannot contain “/”"));
  else
    {
      GFile *file;
      GError *error = NULL;

      file = g_file_get_child_for_display_name (priv->current_folder, name, &error);
      if (file == NULL)
        {
          gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label), error->message);
          g_error_free (error);
        }
      else
        {
          struct FileExistsData *data;

          gtk_label_set_text (GTK_LABEL (priv->new_folder_error_label), "");

          data = g_new0 (struct FileExistsData, 1);
          data->impl = g_object_ref (impl);
          data->parent_file = g_object_ref (priv->current_folder);
          data->file = g_object_ref (file);

          if (priv->file_exists_get_info_cancellable)
            g_cancellable_cancel (priv->file_exists_get_info_cancellable);

          priv->file_exists_get_info_cancellable =
            _gtk_file_system_get_info (priv->file_system,
                                       file,
                                       "standard::type",
                                       name_exists_get_info_cb,
                                       data);

          g_object_unref (file);
        }
    }
}

static void
new_folder_name_changed (GtkEntry             *entry,
                         GtkFileChooserWidget *impl)
{
  check_valid_folder_name (impl, gtk_entry_get_text (entry));
}

static void
new_folder_create_clicked (GtkButton            *button,
                           GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GError *error = NULL;
  GFile *file;
  const gchar *name;

  name = gtk_entry_get_text (GTK_ENTRY (priv->new_folder_name_entry));
  file = g_file_get_child_for_display_name (priv->current_folder, name, &error);

  gtk_widget_hide (priv->new_folder_popover);

  if (file)
    {
      if (g_file_make_directory (file, NULL, &error))
        change_folder_and_display_error (impl, file, FALSE);
      else
        error_creating_folder_dialog (impl, file, error);
      g_object_unref (file);
    }
  else
    error_creating_folder_dialog (impl, file, error);
}

struct selection_check_closure {
  GtkFileChooserWidget *impl;
  int num_selected;
  gboolean all_files;
  gboolean all_folders;
};

/* Used from gtk_tree_selection_selected_foreach() */
static void
selection_check_foreach_cb (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  struct selection_check_closure *closure;
  gboolean is_folder;
  GFile *file;

  gtk_tree_model_get (model, iter,
                      MODEL_COL_FILE, &file,
                      MODEL_COL_IS_FOLDER, &is_folder,
                      -1);

  if (file == NULL)
    return;

  g_object_unref (file);

  closure = data;
  closure->num_selected++;

  closure->all_folders = closure->all_folders && is_folder;
  closure->all_files = closure->all_files && !is_folder;
}

/* Checks whether the selected items in the file list are all files or all folders */
static void
selection_check (GtkFileChooserWidget *impl,
		 gint                  *num_selected,
		 gboolean              *all_files,
		 gboolean              *all_folders)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  struct selection_check_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.num_selected = 0;
  closure.all_files = TRUE;
  closure.all_folders = TRUE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection,
				       selection_check_foreach_cb,
				       &closure);

  g_assert (closure.num_selected == 0 || !(closure.all_files && closure.all_folders));

  if (num_selected)
    *num_selected = closure.num_selected;

  if (all_files)
    *all_files = closure.all_files;

  if (all_folders)
    *all_folders = closure.all_folders;
}

static gboolean
file_is_recent_uri (GFile *file)
{
  GFile *recent;
  gboolean same;

  recent = g_file_new_for_uri ("recent:///");
  same = g_file_equal (file, recent);
  g_object_unref (recent);

  return same;
}

static void
places_sidebar_open_location_cb (GtkPlacesSidebar *sidebar, GFile *location, GtkPlacesOpenFlags open_flags, GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gboolean clear_entry;

  /* In the Save modes, we want to preserve what the uesr typed in the filename
   * entry, so that he may choose another folder without erasing his typed name.
   */
  if (priv->location_entry
      && !(priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
	   || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
    clear_entry = TRUE;
  else
    clear_entry = FALSE;

  /* FIXME-places-sidebar:
   *
   * GtkPlacesSidebar doesn't have a Search item anymore.  We should put that function in a toolbar-like button, like
   * in Nautilus, and do operation_mode_set (impl, OPERATION_MODE_SEARCH);
   */

  location_mode_set (impl, LOCATION_MODE_PATH_BAR);

  if (file_is_recent_uri (location))
    operation_mode_set (impl, OPERATION_MODE_RECENT);
  else
    change_folder_and_display_error (impl, location, clear_entry);
}

/* Callback used when the places sidebar needs us to display an error message */
static void
places_sidebar_show_error_message_cb (GtkPlacesSidebar *sidebar,
				      const char       *primary,
				      const char       *secondary,
				      GtkFileChooserWidget *impl)
{
  error_message (impl, primary, secondary);
}

static gboolean
key_is_left_or_right (GdkEventKey *event)
{
  guint modifiers;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  return ((event->keyval == GDK_KEY_Right
	   || event->keyval == GDK_KEY_KP_Right
	   || event->keyval == GDK_KEY_Left
	   || event->keyval == GDK_KEY_KP_Left)
	  && (event->state & modifiers) == 0);
}

/* Handles key press events on the file list, so that we can trap Enter to
 * activate the default button on our own.  Also, checks to see if “/” has been
 * pressed.
 */
static gboolean
browse_files_key_press_event_cb (GtkWidget   *widget,
				 GdkEventKey *event,
				 gpointer     data)
{
  GtkFileChooserWidget *impl = (GtkFileChooserWidget *) data;
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GdkModifierType no_text_input_mask;

  no_text_input_mask =
    gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_NO_TEXT_INPUT);

  if ((event->keyval == GDK_KEY_slash
       || event->keyval == GDK_KEY_KP_Divide
       || g_unichar_isalnum (gdk_keyval_to_unicode (event->keyval))
#ifdef G_OS_UNIX
       || event->keyval == GDK_KEY_asciitilde
#endif
       ) && !(event->state & no_text_input_mask))
    {
      location_popup_handler (impl, event->string);
      return TRUE;
    }

  if (key_is_left_or_right (event))
    {
      gtk_widget_grab_focus (priv->places_sidebar);
      return TRUE;
    }

  if ((event->keyval == GDK_KEY_Return
       || event->keyval == GDK_KEY_ISO_Enter
       || event->keyval == GDK_KEY_KP_Enter
       || event->keyval == GDK_KEY_space
       || event->keyval == GDK_KEY_KP_Space)
      && !(event->state & gtk_accelerator_get_default_mod_mask ())
      && !(priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
	   priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
    {
      GtkWindow *window;

      window = get_toplevel (widget);
      if (window)
        {
          GtkWidget *default_widget, *focus_widget;

          default_widget = gtk_window_get_default_widget (window);
          focus_widget = gtk_window_get_focus (window);

          if (widget != default_widget &&
              !(widget == focus_widget && (!default_widget || !gtk_widget_get_sensitive (default_widget))))
	    {
	      gtk_window_activate_default (window);

	      return TRUE;
	    }
        }
    }

  return FALSE;
}

/* Callback used when the file list's popup menu is detached */
static void
popup_menu_detach_cb (GtkWidget *attach_widget,
		      GtkMenu   *menu)
{
  GtkFileChooserWidget *impl = g_object_get_data (G_OBJECT (attach_widget), "GtkFileChooserWidget");
  GtkFileChooserWidgetPrivate *priv;

  g_assert (GTK_IS_FILE_CHOOSER_WIDGET (impl));

  priv = impl->priv;

  priv->browse_files_popup_menu = NULL;
  priv->browse_files_popup_menu_add_shortcut_item = NULL;
  priv->browse_files_popup_menu_hidden_files_item = NULL;
  priv->browse_files_popup_menu_copy_file_location_item = NULL;
}

/* Callback used from gtk_tree_selection_selected_foreach(); adds a bookmark for
 * each selected item in the file list.
 */
static void
add_bookmark_foreach_cb (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      data)
{
  GtkFileChooserWidget *impl = (GtkFileChooserWidget *) data;
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *file;

  gtk_tree_model_get (model, iter,
                      MODEL_COL_FILE, &file,
                      -1);

  _gtk_bookmarks_manager_insert_bookmark (priv->bookmarks_manager, file, 0, NULL); /* NULL-GError */

  g_object_unref (file);
}

/* Callback used when the "Add to Bookmarks" menu item is activated */
static void
add_to_shortcuts_cb (GtkMenuItem           *item,
		     GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));

  gtk_tree_selection_selected_foreach (selection,
				       add_bookmark_foreach_cb,
				       impl);
}

/* callback used to set data to clipboard */
static void
copy_file_get_cb  (GtkClipboard     *clipboard,
                   GtkSelectionData *selection_data,
                   guint             info,
                   gpointer          data)
{
  GSList *selected_files = data;

  if (selected_files)
    {
      gint num_files = g_slist_length (selected_files);
      gchar **uris;
      gint i;
      GSList *l;

      uris = g_new (gchar *, num_files + 1);
      uris[num_files] = NULL; /* null terminator */

      i = 0;

      for (l = selected_files; l; l = l->next)
        {
          GFile *file = (GFile *) l->data;

	  if (info == SELECTION_URI)
	    uris[i] = g_file_get_uri (file);
	  else /* if (info == SELECTION_TEXT) - let this be the fallback */
	    uris[i] = g_file_get_parse_name (file);

          i++;
        }

      if (info == SELECTION_URI)
	gtk_selection_data_set_uris (selection_data, uris);
      else /* if (info == SELECTION_TEXT) - let this be the fallback */
	{
	  char *str = g_strjoinv (" ", uris);
	  gtk_selection_data_set_text (selection_data, str, -1);
	  g_free (str);
	}

      g_strfreev (uris);
    }
}

/* callback used to clear the clipboard data */
static void
copy_file_clear_cb (GtkClipboard *clipboard,
                    gpointer      data)
{
  GSList *selected_files = data;

  g_slist_foreach (selected_files, (GFunc) g_object_unref, NULL);
  g_slist_free (selected_files);
}

/* Callback used when the "Copy file’s location" menu item is activated */
static void
copy_file_location_cb (GtkMenuItem           *item,
                       GtkFileChooserWidget *impl)
{
  GSList *selected_files = NULL;

  selected_files = search_get_selected_files (impl);

  if (selected_files)
    {
      GtkClipboard *clipboard;
      GtkTargetList *target_list;
      GtkTargetEntry *targets;
      int n_targets;

      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (impl), GDK_SELECTION_CLIPBOARD);

      target_list = gtk_target_list_new (NULL, 0);
      gtk_target_list_add_text_targets (target_list, SELECTION_TEXT);
      gtk_target_list_add_uri_targets (target_list, SELECTION_URI);

      targets = gtk_target_table_new_from_list (target_list, &n_targets);
      gtk_target_list_unref (target_list);

      gtk_clipboard_set_with_data (clipboard, targets, n_targets,
				   copy_file_get_cb,
                                   copy_file_clear_cb,
				   selected_files);

      gtk_target_table_free (targets, n_targets);
    }
}

/* Callback used when the "Visit this file" menu item is activated */
static void
visit_file_cb (GtkMenuItem *item,
	       GtkFileChooserWidget *impl)
{
  GSList *files;

  files = search_get_selected_files (impl);

  /* Sigh, just use the first one */
  if (files)
    {
      GFile *file = files->data;

      gtk_file_chooser_widget_select_file (GTK_FILE_CHOOSER (impl), file, NULL); /* NULL-GError */
    }

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);
}

/* Callback used when the "Open this folder" menu item is activated */
static void
open_folder_cb (GtkMenuItem          *item,
	        GtkFileChooserWidget *impl)
{
  GSList *files;

  files = search_get_selected_files (impl);

  /* Sigh, just use the first one */
  if (files)
    {
      GFile *file = files->data;
      gchar *uri;

      uri = g_file_get_uri (file);
      gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (impl)), uri, gtk_get_current_event_time (), NULL);
      g_free (uri);
    }

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);
}

/* callback used when the "Show Hidden Files" menu item is toggled */
static void
show_hidden_toggled_cb (GtkCheckMenuItem      *item,
			GtkFileChooserWidget *impl)
{
  g_object_set (impl,
		"show-hidden", gtk_check_menu_item_get_active (item),
		NULL);
}

/* Callback used when the "Show Size Column" menu item is toggled */
static void
show_size_column_toggled_cb (GtkCheckMenuItem *item,
                             GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  priv->show_size_column = gtk_check_menu_item_get_active (item);

  gtk_tree_view_column_set_visible (priv->list_size_column,
                                    priv->show_size_column);
}

static void
sort_directories_toggled_cb (GtkCheckMenuItem     *item,
                             GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSortable *sortable;

  priv->sort_directories_first = gtk_check_menu_item_get_active (item);

  /* force resorting */
  sortable = GTK_TREE_SORTABLE (priv->browse_files_model);
  if (sortable == NULL)
    return;

  gtk_tree_sortable_set_sort_column_id (sortable,
                                        GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
                                        priv->sort_order);
  gtk_tree_sortable_set_sort_column_id (sortable,
                                        priv->sort_column,
                                        priv->sort_order);
}

/* Shows an error dialog about not being able to select a dragged file */
static void
error_selecting_dragged_file_dialog (GtkFileChooserWidget *impl,
				     GFile                 *file,
				     GError                *error)
{
  error_dialog (impl,
		_("Could not select file"),
		file, error);
}

static void
file_list_drag_data_select_uris (GtkFileChooserWidget  *impl,
				 gchar                 **uris)
{
  int i;
  char *uri;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (impl);

  for (i = 1; uris[i]; i++)
    {
      GFile *file;
      GError *error = NULL;

      uri = uris[i];
      file = g_file_new_for_uri (uri);

      gtk_file_chooser_widget_select_file (chooser, file, &error);
      if (error)
	error_selecting_dragged_file_dialog (impl, file, error);

      g_object_unref (file);
    }
}

struct FileListDragData
{
  GtkFileChooserWidget *impl;
  gchar **uris;
  GFile *file;
};

static void
file_list_drag_data_received_get_info_cb (GCancellable *cancellable,
					  GFileInfo    *info,
					  const GError *error,
					  gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct FileListDragData *data = user_data;
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (data->impl);
  GtkFileChooserWidgetPrivate *priv = data->impl->priv;

  if (cancellable != priv->file_list_drag_data_received_cancellable)
    goto out;

  priv->file_list_drag_data_received_cancellable = NULL;

  if (cancelled || error)
    goto out;

  if ((priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
       priv->action == GTK_FILE_CHOOSER_ACTION_SAVE) &&
      data->uris[1] == 0 && !error && _gtk_file_info_consider_as_directory (info))
    change_folder_and_display_error (data->impl, data->file, FALSE);
  else
    {
      GError *error = NULL;

      gtk_file_chooser_widget_unselect_all (chooser);
      gtk_file_chooser_widget_select_file (chooser, data->file, &error);
      if (error)
	error_selecting_dragged_file_dialog (data->impl, data->file, error);
      else
	browse_files_center_selected_row (data->impl);
    }

  if (priv->select_multiple)
    file_list_drag_data_select_uris (data->impl, data->uris);

out:
  g_object_unref (data->impl);
  g_strfreev (data->uris);
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
file_list_drag_data_received_cb (GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 gint              x,
                                 gint              y,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint             time_,
                                 gpointer          data)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (data);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gchar **uris;
  char *uri;
  GFile *file;

  /* Allow only drags from other widgets; see bug #533891. */
  if (gtk_drag_get_source_widget (context) == widget)
    {
      g_signal_stop_emission_by_name (widget, "drag-data-received");
      return;
    }

  /* Parse the text/uri-list string, navigate to the first one */
  uris = gtk_selection_data_get_uris (selection_data);
  if (uris && uris[0])
    {
      struct FileListDragData *data;

      uri = uris[0];
      file = g_file_new_for_uri (uri);

      data = g_new0 (struct FileListDragData, 1);
      data->impl = g_object_ref (impl);
      data->uris = uris;
      data->file = file;

      if (priv->file_list_drag_data_received_cancellable)
        g_cancellable_cancel (priv->file_list_drag_data_received_cancellable);

      priv->file_list_drag_data_received_cancellable =
        _gtk_file_system_get_info (priv->file_system, file,
                                   "standard::type",
                                   file_list_drag_data_received_get_info_cb,
                                   data);
    }

  g_signal_stop_emission_by_name (widget, "drag-data-received");
}

/* Don't do anything with the drag_drop signal */
static gboolean
file_list_drag_drop_cb (GtkWidget             *widget,
			GdkDragContext        *context,
			gint                   x,
			gint                   y,
			guint                  time_,
			GtkFileChooserWidget *impl)
{
  g_signal_stop_emission_by_name (widget, "drag-drop");
  return TRUE;
}

/* Disable the normal tree drag motion handler, it makes it look like you're
   dropping the dragged item onto a tree item */
static gboolean
file_list_drag_motion_cb (GtkWidget             *widget,
                          GdkDragContext        *context,
                          gint                   x,
                          gint                   y,
                          guint                  time_,
                          GtkFileChooserWidget *impl)
{
  g_signal_stop_emission_by_name (widget, "drag-motion");
  return TRUE;
}

/* Sensitizes the "Copy file’s location" and other context menu items if there is actually
 * a selection active.
 */
static void
check_file_list_menu_sensitivity (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gint num_selected;
  gboolean all_files;
  gboolean all_folders;
  gboolean active;

  selection_check (impl, &num_selected, &all_files, &all_folders);

  active = (num_selected != 0);

  if (priv->browse_files_popup_menu_copy_file_location_item)
    gtk_widget_set_sensitive (priv->browse_files_popup_menu_copy_file_location_item, active);
  if (priv->browse_files_popup_menu_add_shortcut_item)
    gtk_widget_set_sensitive (priv->browse_files_popup_menu_add_shortcut_item, active && all_folders);
  if (priv->browse_files_popup_menu_visit_file_item)
    gtk_widget_set_sensitive (priv->browse_files_popup_menu_visit_file_item, active);

  if (priv->browse_files_popup_menu_open_folder_item)
    gtk_widget_set_visible (priv->browse_files_popup_menu_open_folder_item, (num_selected == 1) && all_folders);
}

static GtkWidget *
file_list_add_menu_item (GtkFileChooserWidget *impl,
                         const char *mnemonic_label,
                         GCallback callback)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *item;

  item = gtk_menu_item_new_with_mnemonic (mnemonic_label);
  g_signal_connect (item, "activate", callback, impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->browse_files_popup_menu), item);
  
  return item;
}

static GtkWidget *
file_list_add_check_menu_item (GtkFileChooserWidget *impl,
			       const char *mnemonic_label,
			       GCallback callback)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *item;

  item = gtk_check_menu_item_new_with_mnemonic (mnemonic_label);
  g_signal_connect (item, "toggled", callback, impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->browse_files_popup_menu), item);

  return item;
}

/* Constructs the popup menu for the file list if needed */
static void
file_list_build_popup_menu (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *item;

  if (priv->browse_files_popup_menu)
    return;

  priv->browse_files_popup_menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (priv->browse_files_popup_menu),
			     priv->browse_files_tree_view,
			     popup_menu_detach_cb);

  priv->browse_files_popup_menu_visit_file_item		= file_list_add_menu_item (impl, _("_Visit File"),
											 G_CALLBACK (visit_file_cb));

  priv->browse_files_popup_menu_open_folder_item = file_list_add_menu_item (impl, _("_Open With File Manager"),
											 G_CALLBACK (open_folder_cb));

  priv->browse_files_popup_menu_copy_file_location_item	= file_list_add_menu_item (impl, _("_Copy Location"),
											 G_CALLBACK (copy_file_location_cb));

  priv->browse_files_popup_menu_add_shortcut_item	= file_list_add_menu_item (impl, _("_Add to Bookmarks"),
											 G_CALLBACK (add_to_shortcuts_cb));

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->browse_files_popup_menu), item);

  priv->browse_files_popup_menu_hidden_files_item	= file_list_add_check_menu_item (impl, _("Show _Hidden Files"),
											 G_CALLBACK (show_hidden_toggled_cb));

  priv->browse_files_popup_menu_size_column_item	= file_list_add_check_menu_item (impl, _("Show _Size Column"),
											 G_CALLBACK (show_size_column_toggled_cb));

  priv->browse_files_popup_menu_sort_directories_item   = file_list_add_check_menu_item (impl, _("Sort _Folders before Files"),
                                                                                         G_CALLBACK (sort_directories_toggled_cb));

  check_file_list_menu_sensitivity (impl);
}

/* Updates the popup menu for the file list, creating it if necessary */
static void
file_list_update_popup_menu (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  file_list_build_popup_menu (impl);

  /* The sensitivity of the Add to Bookmarks item is set in
   * bookmarks_check_add_sensitivity()
   */

  /* 'Visit this file' */
  gtk_widget_set_visible (priv->browse_files_popup_menu_visit_file_item, (priv->operation_mode != OPERATION_MODE_BROWSE));

  /* 'Show Hidden Files' */
  g_signal_handlers_block_by_func (priv->browse_files_popup_menu_hidden_files_item,
				   G_CALLBACK (show_hidden_toggled_cb), impl);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->browse_files_popup_menu_hidden_files_item),
				  priv->show_hidden);
  g_signal_handlers_unblock_by_func (priv->browse_files_popup_menu_hidden_files_item,
				     G_CALLBACK (show_hidden_toggled_cb), impl);

  /* 'Show Size Column' */
  g_signal_handlers_block_by_func (priv->browse_files_popup_menu_size_column_item,
				   G_CALLBACK (show_size_column_toggled_cb), impl);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->browse_files_popup_menu_size_column_item),
				  priv->show_size_column);
  g_signal_handlers_unblock_by_func (priv->browse_files_popup_menu_size_column_item,
				     G_CALLBACK (show_size_column_toggled_cb), impl);

  g_signal_handlers_block_by_func (priv->browse_files_popup_menu_sort_directories_item,
                                   G_CALLBACK (sort_directories_toggled_cb), impl);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->browse_files_popup_menu_sort_directories_item),
                                  priv->sort_directories_first);
  g_signal_handlers_unblock_by_func (priv->browse_files_popup_menu_sort_directories_item,
                                     G_CALLBACK (sort_directories_toggled_cb), impl);
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (user_data);
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GtkRequisition req;
  gint monitor_num;
  GdkRectangle monitor;

  g_return_if_fail (gtk_widget_get_realized (widget));

  gdk_window_get_origin (gtk_widget_get_window (widget), x, y);

  gtk_widget_get_preferred_size (GTK_WIDGET (menu),
                                 &req, NULL);

  gtk_widget_get_allocation (widget, &allocation);
  *x += (allocation.width - req.width) / 2;
  *y += (allocation.height - req.height) / 2;

  monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);
  gtk_menu_set_monitor (menu, monitor_num);
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  *x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
  *y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));

  *push_in = FALSE;
}

static void
file_list_popup_menu (GtkFileChooserWidget *impl,
		      GdkEventButton        *event)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  file_list_update_popup_menu (impl);
  if (event)
    gtk_menu_popup (GTK_MENU (priv->browse_files_popup_menu),
		    NULL, NULL, NULL, NULL,
		    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (priv->browse_files_popup_menu),
		      NULL, NULL,
		      popup_position_func, priv->browse_files_tree_view,
		      0, GDK_CURRENT_TIME);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->browse_files_popup_menu),
				   FALSE);
    }

}

/* Callback used for the GtkWidget::popup-menu signal of the file list */
static gboolean
list_popup_menu_cb (GtkWidget *widget,
		    GtkFileChooserWidget *impl)
{
  file_list_popup_menu (impl, NULL);
  return TRUE;
}

/* Callback used when a button is pressed on the file list.  We trap button 3 to
 * bring up a popup menu.
 */
static gboolean
list_button_press_event_cb (GtkWidget             *widget,
			    GdkEventButton        *event,
			    GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  static gboolean in_press = FALSE;

  if (in_press)
    return FALSE;

  if (!gdk_event_triggers_context_menu ((GdkEvent *) event))
    return FALSE;

  in_press = TRUE;
  gtk_widget_event (priv->browse_files_tree_view, (GdkEvent *) event);
  in_press = FALSE;

  file_list_popup_menu (impl, event);
  return TRUE;
}

typedef struct {
  OperationMode operation_mode;
  gint general_column;
  gint model_column;
} ColumnMap;

/* Sets the sort column IDs for the file list; needs to be done whenever we
 * change the model on the treeview.
 */
static void
file_list_set_sort_column_ids (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->browse_files_tree_view), -1);

  gtk_tree_view_column_set_sort_column_id (priv->list_name_column, MODEL_COL_NAME);
  gtk_tree_view_column_set_sort_column_id (priv->list_mtime_column, MODEL_COL_MTIME);
  gtk_tree_view_column_set_sort_column_id (priv->list_size_column, MODEL_COL_SIZE);
  gtk_tree_view_column_set_sort_column_id (priv->list_location_column, MODEL_COL_LOCATION_TEXT);
}

static gboolean
file_list_query_tooltip_cb (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_tip,
                            GtkTooltip *tooltip,
                            gpointer    user_data)
{
  GtkFileChooserWidget *impl = user_data;
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GFile *file;
  gchar *filename;

  if (priv->operation_mode == OPERATION_MODE_BROWSE)
    return FALSE;


  if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (priv->browse_files_tree_view),
                                          &x, &y,
                                          keyboard_tip,
                                          &model, &path, &iter))
    return FALSE;
                                       
  gtk_tree_model_get (model, &iter,
                      MODEL_COL_FILE, &file,
                      -1);

  if (file == NULL)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  filename = g_file_get_path (file);
  gtk_tooltip_set_text (tooltip, filename);
  gtk_tree_view_set_tooltip_row (GTK_TREE_VIEW (priv->browse_files_tree_view),
                                 tooltip,
                                 path);

  g_free (filename);
  g_object_unref (file);
  gtk_tree_path_free (path);

  return TRUE;
}

static void
set_icon_cell_renderer_fixed_size (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gint xpad, ypad;

  gtk_cell_renderer_get_padding (priv->list_pixbuf_renderer, &xpad, &ypad);
  gtk_cell_renderer_set_fixed_size (priv->list_pixbuf_renderer, 
                                    xpad * 2 + priv->icon_size,
                                    ypad * 2 + priv->icon_size);
}

static gboolean
location_changed_timeout_cb (gpointer user_data)
{
  GtkFileChooserWidget *impl = user_data;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (impl));
  check_preview_change (impl);
  g_signal_emit_by_name (impl, "selection-changed", 0);

  priv->location_changed_id = 0;

  return G_SOURCE_REMOVE;
}

static void
reset_location_timeout (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->location_changed_id > 0)
    g_source_remove (priv->location_changed_id);
  priv->location_changed_id = g_timeout_add (LOCATION_CHANGED_TIMEOUT,
                                            location_changed_timeout_cb,
                                            impl);
  g_source_set_name_by_id (priv->location_changed_id, "[gtk+] location_changed_timeout_cb");
}

static void
location_entry_changed_cb (GtkEditable          *editable,
                           GtkFileChooserWidget *impl)
{
  if (impl->priv->action != GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    reset_location_timeout (impl);
}

static void
location_entry_create (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (!priv->location_entry)
    {
      priv->location_entry = _gtk_file_chooser_entry_new (TRUE);
      if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
          priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
        gtk_entry_set_placeholder_text (GTK_ENTRY (priv->location_entry), _("Location"));

      g_signal_connect (priv->location_entry, "changed",
                        G_CALLBACK (location_entry_changed_cb), impl);
    }

  _gtk_file_chooser_entry_set_local_only (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), priv->local_only);
  _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), priv->action);
  gtk_entry_set_width_chars (GTK_ENTRY (priv->location_entry), 45);
  gtk_entry_set_activates_default (GTK_ENTRY (priv->location_entry), TRUE);
}

/* Creates the widgets specific to Save mode */
static void
save_widgets_create (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *vbox;
  GtkWidget *widget;

  if (priv->save_widgets != NULL)
    return;

  location_switch_to_path_bar (impl);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_style_context_add_class (gtk_widget_get_style_context (vbox), "search-bar");

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

  priv->save_widgets_table = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (priv->save_widgets_table), 10);
  gtk_box_pack_start (GTK_BOX (vbox), priv->save_widgets_table, FALSE, FALSE, 0);
  gtk_widget_show (priv->save_widgets_table);
  gtk_grid_set_row_spacing (GTK_GRID (priv->save_widgets_table), 12);
  gtk_grid_set_column_spacing (GTK_GRID (priv->save_widgets_table), 12);

  /* Label */

  widget = gtk_label_new_with_mnemonic (_("_Name:"));
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (priv->save_widgets_table), widget, 0, 0, 1, 1);
  gtk_widget_show (widget);

  /* Location entry */

  location_entry_create (impl);
  gtk_widget_set_hexpand (priv->location_entry, TRUE);
  gtk_grid_attach (GTK_GRID (priv->save_widgets_table), priv->location_entry, 1, 0, 1, 1);
  gtk_widget_show (priv->location_entry);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), priv->location_entry);

  priv->save_widgets = vbox;
  gtk_box_pack_start (GTK_BOX (impl), priv->save_widgets, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (impl), priv->save_widgets, 0);
  gtk_widget_show (priv->save_widgets);
}

/* Destroys the widgets specific to Save mode */
static void
save_widgets_destroy (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->save_widgets == NULL)
    return;

  gtk_widget_destroy (priv->save_widgets);
  priv->save_widgets = NULL;
  priv->save_widgets_table = NULL;
  priv->location_entry = NULL;
}

/* Turns on the path bar widget.  Can be called even if we are already in that
 * mode.
 */
static void
location_switch_to_path_bar (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->location_entry)
    {
      gtk_widget_destroy (priv->location_entry);
      priv->location_entry = NULL;
    }

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_header_stack), "pathbar");
}

/* Turns on the location entry.  Can be called even if we are already in that
 * mode.
 */
static void
location_switch_to_filename_entry (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  /* when in search or recent files mode, we are not showing the
   * browse_header_box container, so there's no point in switching
   * to it.
   */
  if (priv->operation_mode == OPERATION_MODE_SEARCH ||
      priv->operation_mode == OPERATION_MODE_RECENT)
    return;

  /* Box */

  gtk_widget_show (priv->browse_header_box);

  /* Entry */

  if (!priv->location_entry)
    {
      location_entry_create (impl);
      gtk_box_pack_start (GTK_BOX (priv->location_entry_box), priv->location_entry, TRUE, TRUE, 0);
    }

  /* Configure the entry */

  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), priv->current_folder);

  /* Done */

  gtk_widget_show (priv->location_entry);

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_header_stack), "location");

  gtk_widget_grab_focus (priv->location_entry);
}

/* Sets a new location mode.
 */
static void
location_mode_set (GtkFileChooserWidget *impl,
		   LocationMode new_mode)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      GtkWindow *toplevel;
      GtkWidget *current_focus;
      gboolean switch_to_file_list;

      switch (new_mode)
	{
	case LOCATION_MODE_PATH_BAR:

	  /* The location_entry will disappear when we switch to path bar mode.  So,
	   * we'll focus the file list in that case, to avoid having a window with
	   * no focused widget.
	   */
	  toplevel = get_toplevel (GTK_WIDGET (impl));
	  switch_to_file_list = FALSE;
	  if (toplevel)
	    {
	      current_focus = gtk_window_get_focus (toplevel);
	      if (!current_focus || current_focus == priv->location_entry)
		switch_to_file_list = TRUE;
	    }

	  location_switch_to_path_bar (impl);

	  if (switch_to_file_list)
	    gtk_widget_grab_focus (priv->browse_files_tree_view);

	  break;

	case LOCATION_MODE_FILENAME_ENTRY:
	  location_switch_to_filename_entry (impl);
	  break;

	default:
	  g_assert_not_reached ();
	  return;
	}
    }

  priv->location_mode = new_mode;
}

/* Callback used when the places sidebar needs us to enter a location */
static void
places_sidebar_show_enter_location_cb (GtkPlacesSidebar *sidebar,
                                       GtkFileChooserWidget *impl)
{
  operation_mode_set (impl, OPERATION_MODE_ENTER_LOCATION);
}

static void
location_toggle_popup_handler (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->operation_mode == OPERATION_MODE_SEARCH)
    return;

  if (priv->operation_mode == OPERATION_MODE_RECENT &&
      (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
       priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER))
    operation_mode_set (impl, OPERATION_MODE_BROWSE);

  /* If the file entry is not visible, show it.
   * If it is visible, turn it off only if it is focused.  Otherwise, switch to the entry.
   */
  if (priv->location_mode == LOCATION_MODE_PATH_BAR)
    {
      location_mode_set (impl, LOCATION_MODE_FILENAME_ENTRY);
    }
  else if (priv->location_mode == LOCATION_MODE_FILENAME_ENTRY)
    {
      if (gtk_widget_has_focus (priv->location_entry))
        {
          location_mode_set (impl, LOCATION_MODE_PATH_BAR);
        }
      else
        {
          gtk_widget_grab_focus (priv->location_entry);
        }
    }
}

static void
gtk_file_chooser_widget_constructed (GObject *object)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (object);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  profile_start ("start", NULL);

  G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->constructed (object);

  g_assert (priv->file_system);

  update_appearance (impl);

  profile_end ("end", NULL);
}

static void
update_extra_and_filters (GtkFileChooserWidget *impl)
{
  gtk_widget_set_visible (impl->priv->extra_and_filters,
                          gtk_widget_get_visible (impl->priv->extra_align) ||
                          gtk_widget_get_visible (impl->priv->filter_combo_hbox));
}

/* Sets the extra_widget by packing it in the appropriate place */
static void
set_extra_widget (GtkFileChooserWidget *impl,
		  GtkWidget             *extra_widget)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (extra_widget)
    {
      g_object_ref (extra_widget);
      /* FIXME: is this right ? */
      gtk_widget_show (extra_widget);
    }

  if (priv->extra_widget)
    {
      gtk_container_remove (GTK_CONTAINER (priv->extra_align), priv->extra_widget);
      g_object_unref (priv->extra_widget);
    }

  priv->extra_widget = extra_widget;
  if (priv->extra_widget)
    {
      gtk_container_add (GTK_CONTAINER (priv->extra_align), priv->extra_widget);
      gtk_widget_show (priv->extra_align);
    }
  else
    gtk_widget_hide (priv->extra_align);

  /* Calls update_extra_and_filters */
  show_filters (impl, priv->filters != NULL);
}

static void
switch_to_home_dir (GtkFileChooserWidget *impl)
{
  const gchar *home = g_get_home_dir ();
  GFile *home_file;

  if (home == NULL)
    return;

  home_file = g_file_new_for_path (home);

  gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (impl), home_file, NULL); /* NULL-GError */

  g_object_unref (home_file);
}

static void
set_local_only (GtkFileChooserWidget *impl,
		gboolean               local_only)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (local_only != priv->local_only)
    {
      priv->local_only = local_only;

      if (priv->location_entry)
	_gtk_file_chooser_entry_set_local_only (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), local_only);

      gtk_places_sidebar_set_local_only (GTK_PLACES_SIDEBAR (priv->places_sidebar), local_only);

      if (local_only && priv->current_folder &&
           !_gtk_file_has_native_path (priv->current_folder))
	{
	  /* If we are pointing to a non-local folder, make an effort to change
	   * back to a local folder, but it's really up to the app to not cause
	   * such a situation, so we ignore errors.
	   */
	  switch_to_home_dir (impl);
	}
    }
}

/* Sets the file chooser to multiple selection mode */
static void
set_select_multiple (GtkFileChooserWidget *impl,
		     gboolean               select_multiple,
		     gboolean               property_notify)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;
  GtkSelectionMode mode;

  if (select_multiple == priv->select_multiple)
    return;

  mode = select_multiple ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  gtk_tree_selection_set_mode (selection, mode);

  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->browse_files_tree_view), select_multiple);

  priv->select_multiple = select_multiple;
  g_object_notify (G_OBJECT (impl), "select-multiple");

  check_preview_change (impl);
}

static void
set_file_system_backend (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  profile_start ("start for backend", "default");

  priv->file_system = _gtk_file_system_new ();

  profile_end ("end", NULL);
}

static void
unset_file_system_backend (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_object_unref (priv->file_system);

  priv->file_system = NULL;
}

/* Takes the folder stored in a row in the recent_model, and puts it in the pathbar */
static void
put_recent_folder_in_pathbar (GtkFileChooserWidget *impl, GtkTreeIter *iter)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *file;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->recent_model), iter,
		      MODEL_COL_FILE, &file,
		      -1);
  _gtk_path_bar_set_file (GTK_PATH_BAR (priv->browse_path_bar), file, FALSE);
  g_object_unref (file);
}

/* Sets the location bar in the appropriate mode according to the
 * current operation mode and action.  This is the central function
 * for dealing with the pathbar’s widgets; as long as impl->action and
 * impl->operation_mode are set correctly, then calling this function
 * will update all the pathbar’s widgets.
 */
static void
location_bar_update (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gboolean visible = TRUE;
  gboolean create_folder_visible = FALSE;

  switch (priv->operation_mode)
    {
    case OPERATION_MODE_ENTER_LOCATION:
      break;

    case OPERATION_MODE_BROWSE:
      break;

    case OPERATION_MODE_RECENT:
      if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  GtkTreeSelection *selection;
	  gboolean have_selected;
	  GtkTreeIter iter;

	  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));

	  /* Save mode means single-selection mode, so the following is valid */
	  have_selected = gtk_tree_selection_get_selected (selection, NULL, &iter);

	  if (have_selected)
	    {
	      put_recent_folder_in_pathbar (impl, &iter);
	    }
	}
      visible = FALSE;
      break;

    case OPERATION_MODE_SEARCH:
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  gtk_widget_set_visible (priv->browse_header_box, visible);

  if (visible)
    {
      if (priv->create_folders
          && priv->action != GTK_FILE_CHOOSER_ACTION_OPEN
          && priv->operation_mode != OPERATION_MODE_RECENT)
        create_folder_visible = TRUE;
    }

  gtk_widget_set_visible (priv->browse_new_folder_button, create_folder_visible);
}

/* Stops running operations like populating the browse model, searches, and the recent-files model */
static void
operation_mode_stop (GtkFileChooserWidget *impl, OperationMode mode)
{
  switch (mode)
    {
    case OPERATION_MODE_ENTER_LOCATION:
      stop_loading_and_clear_list_model (impl, TRUE);
      break;

    case OPERATION_MODE_BROWSE:
      stop_loading_and_clear_list_model (impl, TRUE);
      break;

    case OPERATION_MODE_SEARCH:
      search_stop_searching (impl, FALSE);
      search_clear_model (impl, TRUE);
      break;

    case OPERATION_MODE_RECENT:
      recent_stop_loading (impl);
      recent_clear_model (impl, TRUE);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
operation_mode_set_enter_location (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_files_stack), "list");
  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_header_stack), "location");
  location_bar_update (impl);
  gtk_widget_set_sensitive (priv->filter_combo, TRUE);
  location_mode_set (impl, LOCATION_MODE_FILENAME_ENTRY);
  gtk_tree_view_column_set_visible (priv->list_location_column, FALSE);
}

static void
operation_mode_set_browse (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_files_stack), "list");
  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_header_stack), "pathbar");
  location_bar_update (impl);
  gtk_widget_set_sensitive (priv->filter_combo, TRUE);
  gtk_tree_view_column_set_visible (priv->list_location_column, FALSE);
}

static void
operation_mode_set_search (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gchar *current;

  g_assert (priv->search_model == NULL);

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_files_stack), "list");
  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_header_stack), "search");
  location_bar_update (impl);
  search_setup_widgets (impl);
  gtk_entry_grab_focus_without_selecting (GTK_ENTRY (priv->search_entry));
  gtk_places_sidebar_set_location (GTK_PLACES_SIDEBAR (priv->places_sidebar), NULL);
  gtk_widget_set_sensitive (priv->filter_combo, FALSE);
  if (priv->current_folder)
    current = g_file_get_basename (priv->current_folder);
  else
    current = g_strdup (_("Home"));
  gtk_button_set_label (GTK_BUTTON (priv->current_location_radio), current);
  g_free (current);

  gtk_tree_view_column_set_visible (priv->list_location_column, TRUE);
}

static void
operation_mode_set_recent (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *file;

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_files_stack), "list");
  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_header_stack), "pathbar");
  location_bar_update (impl);
  recent_start_loading (impl);
  file = g_file_new_for_uri ("recent:///");
  gtk_places_sidebar_set_location (GTK_PLACES_SIDEBAR (priv->places_sidebar), file);
  g_object_unref (file);
  gtk_widget_set_sensitive (priv->filter_combo, TRUE);
  gtk_tree_view_column_set_visible (priv->list_location_column, FALSE);
}

static void
operation_mode_set (GtkFileChooserWidget *impl, OperationMode mode)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  OperationMode old_mode;

  operation_mode_stop (impl, priv->operation_mode);

  old_mode = priv->operation_mode;
  priv->operation_mode = mode;

  switch (priv->operation_mode)
    {
    case OPERATION_MODE_ENTER_LOCATION:
      operation_mode_set_enter_location (impl);
      break;

    case OPERATION_MODE_BROWSE:
      operation_mode_set_browse (impl);
      break;

    case OPERATION_MODE_SEARCH:
      operation_mode_set_search (impl);
      break;

    case OPERATION_MODE_RECENT:
      operation_mode_set_recent (impl);
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  if ((old_mode == OPERATION_MODE_SEARCH) != (mode == OPERATION_MODE_SEARCH))
    g_object_notify (G_OBJECT (impl), "search-mode");
}

/* This function is basically a do_all function.
 *
 * It sets the visibility on all the widgets based on the current state, and
 * moves the custom_widget if needed.
 */
static void
update_appearance (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
      priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      save_widgets_create (impl);
      gtk_places_sidebar_set_show_enter_location (GTK_PLACES_SIDEBAR (priv->places_sidebar), FALSE);

      if (priv->select_multiple)
	{
	  g_warning ("Save mode cannot be set in conjunction with multiple selection mode.  "
		     "Re-setting to single selection mode.");
	  set_select_multiple (impl, FALSE, TRUE);
	}
    }
  else if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
	   priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      save_widgets_destroy (impl);
      gtk_places_sidebar_set_show_enter_location (GTK_PLACES_SIDEBAR (priv->places_sidebar), TRUE);
      location_mode_set (impl, priv->location_mode);
    }

  if (priv->location_entry)
    _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), priv->action);

  location_bar_update (impl);

  /* This *is* needed; we need to redraw the file list because the "sensitivity"
   * of files may change depending whether we are in a file or folder-only mode.
   */
  gtk_widget_queue_draw (priv->browse_files_tree_view);

  emit_default_size_changed (impl);
}

static void
gtk_file_chooser_widget_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)

{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (object);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  switch (prop_id)
    {
    case PROP_SEARCH_MODE:
      if (g_value_get_boolean (value))
        operation_mode_set (impl, OPERATION_MODE_SEARCH);
      else
        {
          operation_mode_set (impl, OPERATION_MODE_BROWSE);
          if (priv->current_folder)
            change_folder_and_display_error (impl, priv->current_folder, FALSE);
          else
            switch_to_home_dir (impl);
        }
      break;

    case GTK_FILE_CHOOSER_PROP_ACTION:
      {
	GtkFileChooserAction action = g_value_get_enum (value);

	if (action != priv->action)
	  {
	    gtk_file_chooser_widget_unselect_all (GTK_FILE_CHOOSER (impl));
	    
	    if ((action == GTK_FILE_CHOOSER_ACTION_SAVE ||
                 action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
		&& priv->select_multiple)
	      {
		g_warning ("Tried to change the file chooser action to SAVE or CREATE_FOLDER, but "
			   "this is not allowed in multiple selection mode.  Resetting the file chooser "
			   "to single selection mode.");
		set_select_multiple (impl, FALSE, TRUE);
	      }
	    priv->action = action;
            update_cell_renderer_attributes (impl);
	    update_appearance (impl);
	    settings_load (impl);
	  }
      }
      break;

    case GTK_FILE_CHOOSER_PROP_FILTER:
      set_current_filter (impl, g_value_get_object (value));
      break;

    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      set_local_only (impl, g_value_get_boolean (value));
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      set_preview_widget (impl, g_value_get_object (value));
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      priv->preview_widget_active = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;

    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
      priv->use_preview_label = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;

    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      set_extra_widget (impl, g_value_get_object (value));
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      {
	gboolean select_multiple = g_value_get_boolean (value);
	if ((priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
             priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    && select_multiple)
	  {
	    g_warning ("Tried to set the file chooser to multiple selection mode, but this is "
		       "not allowed in SAVE or CREATE_FOLDER modes.  Ignoring the change and "
		       "leaving the file chooser in single selection mode.");
	    return;
	  }

	set_select_multiple (impl, select_multiple, FALSE);
      }
      break;

    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      {
	gboolean show_hidden = g_value_get_boolean (value);
	if (show_hidden != priv->show_hidden)
	  {
	    priv->show_hidden = show_hidden;

	    if (priv->browse_files_model)
	      _gtk_file_system_model_set_show_hidden (priv->browse_files_model, show_hidden);
	  }
      }
      break;

    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
      {
	gboolean do_overwrite_confirmation = g_value_get_boolean (value);
	priv->do_overwrite_confirmation = do_overwrite_confirmation;
      }
      break;

    case GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS:
      {
        gboolean create_folders = g_value_get_boolean (value);
        priv->create_folders = create_folders;
        update_appearance (impl);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_widget_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (object);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  switch (prop_id)
    {
    case PROP_SEARCH_MODE:
      g_value_set_boolean (value, priv->operation_mode == OPERATION_MODE_SEARCH);
      break;

    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_value_set_enum (value, priv->action);
      break;

    case GTK_FILE_CHOOSER_PROP_FILTER:
      g_value_set_object (value, priv->current_filter);
      break;

    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, priv->local_only);
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      g_value_set_object (value, priv->preview_widget);
      break;

    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      g_value_set_boolean (value, priv->preview_widget_active);
      break;

    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
      g_value_set_boolean (value, priv->use_preview_label);
      break;

    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      g_value_set_object (value, priv->extra_widget);
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, priv->select_multiple);
      break;

    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, priv->show_hidden);
      break;

    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
      g_value_set_boolean (value, priv->do_overwrite_confirmation);
      break;

    case GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS:
      g_value_set_boolean (value, priv->create_folders);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* This cancels everything that may be going on in the background. */
static void
cancel_all_operations (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  pending_select_files_free (impl);

  if (priv->file_list_drag_data_received_cancellable)
    {
      g_cancellable_cancel (priv->file_list_drag_data_received_cancellable);
      priv->file_list_drag_data_received_cancellable = NULL;
    }

  if (priv->update_current_folder_cancellable)
    {
      g_cancellable_cancel (priv->update_current_folder_cancellable);
      priv->update_current_folder_cancellable = NULL;
    }

  if (priv->should_respond_get_info_cancellable)
    {
      g_cancellable_cancel (priv->should_respond_get_info_cancellable);
      priv->should_respond_get_info_cancellable = NULL;
    }

  if (priv->file_exists_get_info_cancellable)
    {
      g_cancellable_cancel (priv->file_exists_get_info_cancellable);
      priv->file_exists_get_info_cancellable = NULL;
    }

  search_stop_searching (impl, TRUE);
  recent_stop_loading (impl);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (GtkFileChooserWidget *impl,
			GdkScreen             *screen)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->settings_signal_id)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_for_screen (screen);
      g_signal_handler_disconnect (settings,
				   priv->settings_signal_id);
      priv->settings_signal_id = 0;
    }
}

static void
gtk_file_chooser_widget_dispose (GObject *object)
{
  GtkFileChooserWidget *impl = (GtkFileChooserWidget *) object;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  cancel_all_operations (impl);

  if (priv->extra_widget)
    {
      g_object_unref (priv->extra_widget);
      priv->extra_widget = NULL;
    }

  remove_settings_signal (impl, gtk_widget_get_screen (GTK_WIDGET (impl)));

  if (priv->bookmarks_manager)
    {
      _gtk_bookmarks_manager_free (priv->bookmarks_manager);
      priv->bookmarks_manager = NULL;
    }

  G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->dispose (object);
}

/* We override show-all since we have internal widgets that
 * shouldn’t be shown when you call show_all(), like the filter
 * combo box.
 */
static void
gtk_file_chooser_widget_show_all (GtkWidget *widget)
{
  GtkFileChooserWidget *impl = (GtkFileChooserWidget *) widget;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_widget_show (widget);

  if (priv->extra_widget)
    gtk_widget_show_all (priv->extra_widget);
}

/* Handler for GtkWindow::set-focus; this is where we save the last-focused
 * widget on our toplevel.  See gtk_file_chooser_widget_hierarchy_changed()
 */
static void
toplevel_set_focus_cb (GtkWindow             *window,
		       GtkWidget             *focus,
		       GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  priv->toplevel_last_focus_widget = gtk_window_get_focus (window);
}

/* We monitor the focus widget on our toplevel to be able to know which widget
 * was last focused at the time our “should_respond” method gets called.
 */
static void
gtk_file_chooser_widget_hierarchy_changed (GtkWidget *widget,
					    GtkWidget *previous_toplevel)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (widget);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);

  if (previous_toplevel && 
      priv->toplevel_set_focus_id != 0)
    {
      g_signal_handler_disconnect (previous_toplevel,
                                   priv->toplevel_set_focus_id);
      priv->toplevel_set_focus_id = 0;
      priv->toplevel_last_focus_widget = NULL;
    }

  if (gtk_widget_is_toplevel (toplevel))
    {
      g_assert (priv->toplevel_set_focus_id == 0);
      priv->toplevel_set_focus_id = g_signal_connect (toplevel, "set-focus",
						      G_CALLBACK (toplevel_set_focus_cb), impl);
      priv->toplevel_last_focus_widget = gtk_window_get_focus (GTK_WINDOW (toplevel));
    }
}

/* Changes the icons wherever it is needed */
static void
change_icon_theme (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gint width, height;

  profile_start ("start", NULL);

  if (gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height))
    priv->icon_size = MAX (width, height);
  else
    priv->icon_size = FALLBACK_ICON_SIZE;

  /* the first cell in the first column is the icon column, and we have a fixed size there */
  set_icon_cell_renderer_fixed_size (impl);

  if (priv->browse_files_model)
    _gtk_file_system_model_clear_cache (priv->browse_files_model, MODEL_COL_SURFACE);
  gtk_widget_queue_resize (priv->browse_files_tree_view);

  profile_end ("end", NULL);
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject               *object,
		    GParamSpec            *pspec,
		    GtkFileChooserWidget *impl)
{
  const char *name;

  profile_start ("start", NULL);

  name = g_param_spec_get_name (pspec);

  if (strcmp (name, "gtk-icon-theme-name") == 0)
    change_icon_theme (impl);

  profile_end ("end", NULL);
}

/* Installs a signal handler for GtkSettings so that we can monitor changes in
 * the icon theme.
 */
static void
check_icon_theme (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkSettings *settings;

  profile_start ("start", NULL);

  if (priv->settings_signal_id)
    {
      profile_end ("end", NULL);
      return;
    }

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    {
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));
      priv->settings_signal_id = g_signal_connect (settings, "notify",
						   G_CALLBACK (settings_notify_cb), impl);

      change_icon_theme (impl);
    }

  profile_end ("end", NULL);
}

static void
gtk_file_chooser_widget_style_updated (GtkWidget *widget)
{
  GtkFileChooserWidget *impl;

  profile_start ("start", NULL);

  impl = GTK_FILE_CHOOSER_WIDGET (widget);

  profile_msg ("    parent class style_udpated start", NULL);
  GTK_WIDGET_CLASS (gtk_file_chooser_widget_parent_class)->style_updated (widget);
  profile_msg ("    parent class style_updated end", NULL);

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    change_icon_theme (impl);

  emit_default_size_changed (impl);

  profile_end ("end", NULL);
}

static void
gtk_file_chooser_widget_screen_changed (GtkWidget *widget,
					 GdkScreen *previous_screen)
{
  GtkFileChooserWidget *impl;

  profile_start ("start", NULL);

  impl = GTK_FILE_CHOOSER_WIDGET (widget);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_widget_parent_class)->screen_changed)
    GTK_WIDGET_CLASS (gtk_file_chooser_widget_parent_class)->screen_changed (widget, previous_screen);

  remove_settings_signal (impl, previous_screen);
  check_icon_theme (impl);

  emit_default_size_changed (impl);

  profile_end ("end", NULL);
}

static void
set_sort_column (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSortable *sortable;

  sortable = GTK_TREE_SORTABLE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->browse_files_tree_view)));

  /* can happen when we're still populating the model */
  if (sortable == NULL)
    return;

  gtk_tree_sortable_set_sort_column_id (sortable,
                                        priv->sort_column,
                                        priv->sort_order);
}

static void
settings_load (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gboolean show_hidden;
  gboolean show_size_column;
  gboolean sort_directories_first;
  gint sort_column;
  GtkSortType sort_order;
  StartupMode startup_mode;
  gint sidebar_width;
  GSettings *settings;

  settings = _gtk_file_chooser_get_settings_for_widget (GTK_WIDGET (impl));

  show_hidden = g_settings_get_boolean (settings, SETTINGS_KEY_SHOW_HIDDEN);
  show_size_column = g_settings_get_boolean (settings, SETTINGS_KEY_SHOW_SIZE_COLUMN);
  sort_column = g_settings_get_enum (settings, SETTINGS_KEY_SORT_COLUMN);
  sort_order = g_settings_get_enum (settings, SETTINGS_KEY_SORT_ORDER);
  sidebar_width = g_settings_get_int (settings, SETTINGS_KEY_SIDEBAR_WIDTH);
  startup_mode = g_settings_get_enum (settings, SETTINGS_KEY_STARTUP_MODE);
  sort_directories_first = g_settings_get_boolean (settings, SETTINGS_KEY_SORT_DIRECTORIES_FIRST);

  gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (impl), show_hidden);

  priv->show_size_column = show_size_column;
  gtk_tree_view_column_set_visible (priv->list_size_column, show_size_column);

  priv->sort_column = sort_column;
  priv->sort_order = sort_order;
  priv->startup_mode = startup_mode;
  priv->sort_directories_first = sort_directories_first;

  /* We don't call set_sort_column() here as the models may not have been
   * created yet.  The individual functions that create and set the models will
   * call set_sort_column() themselves.
   */

  gtk_paned_set_position (GTK_PANED (priv->browse_widgets_hpaned), sidebar_width);
}

static void
settings_save (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GSettings *settings;

  settings = _gtk_file_chooser_get_settings_for_widget (GTK_WIDGET (impl));

  /* All the other state */

  g_settings_set_enum (settings, SETTINGS_KEY_LOCATION_MODE, priv->location_mode);
  g_settings_set_boolean (settings, SETTINGS_KEY_SHOW_HIDDEN,
                          gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (impl)));
  g_settings_set_boolean (settings, SETTINGS_KEY_SHOW_SIZE_COLUMN, priv->show_size_column);
  g_settings_set_boolean (settings, SETTINGS_KEY_SORT_DIRECTORIES_FIRST, priv->sort_directories_first);
  g_settings_set_enum (settings, SETTINGS_KEY_SORT_COLUMN, priv->sort_column);
  g_settings_set_enum (settings, SETTINGS_KEY_SORT_ORDER, priv->sort_order);
  g_settings_set_int (settings, SETTINGS_KEY_SIDEBAR_WIDTH,
                      gtk_paned_get_position (GTK_PANED (priv->browse_widgets_hpaned)));

  /* Now apply the settings */
  g_settings_apply (settings);
}

/* GtkWidget::realize method */
static void
gtk_file_chooser_widget_realize (GtkWidget *widget)
{
  GtkFileChooserWidget *impl;

  impl = GTK_FILE_CHOOSER_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_file_chooser_widget_parent_class)->realize (widget);

  emit_default_size_changed (impl);
}

/* Changes the current folder to $CWD */
static void
switch_to_cwd (GtkFileChooserWidget *impl)
{
  char *current_working_dir;

  current_working_dir = g_get_current_dir ();
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (impl), current_working_dir);
  g_free (current_working_dir);
}

static gboolean
recent_files_setting_is_enabled (GtkFileChooserWidget *impl)
{
  GtkSettings *settings;
  gboolean enabled;

  settings = gtk_widget_get_settings (GTK_WIDGET (impl));
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
can_show_recent (GtkFileChooserWidget *impl)
{
  return recent_files_setting_is_enabled (impl) && recent_scheme_is_supported ();
}

/* Sets the file chooser to showing Recent Files or $CWD, depending on the
 * user’s settings.
 */
static void
set_startup_mode (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  switch (priv->startup_mode)
    {
    case STARTUP_MODE_RECENT:
      if (can_show_recent (impl))
        {
          operation_mode_set (impl, OPERATION_MODE_RECENT);
          break;
        }
      /* else fall thru */

    case STARTUP_MODE_CWD:
      switch_to_cwd (impl);
      break;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
shortcut_exists (GtkFileChooserWidget *impl, GFile *needle)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GSList *haystack;
  GSList *l;
  gboolean exists;

  exists = FALSE;

  haystack = gtk_places_sidebar_list_shortcuts (GTK_PLACES_SIDEBAR (priv->places_sidebar));
  for (l = haystack; l; l = l->next)
    {
      GFile *hay;

      hay = G_FILE (l->data);
      if (g_file_equal (hay, needle))
	{
	  exists = TRUE;
	  break;
	}
    }
  g_slist_free_full (haystack, g_object_unref);

  return exists;
}

static void
add_cwd_to_sidebar_if_needed (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  char *cwd;
  GFile *cwd_file;
  GFile *home_file;

  cwd = g_get_current_dir (); 
  cwd_file = g_file_new_for_path (cwd);
  g_free (cwd);

  if (shortcut_exists (impl, cwd_file))
    goto out;

  home_file = g_file_new_for_path (g_get_home_dir ());

  /* We only add an item for $CWD if it is different from $HOME.  This way,
   * applications which get launched from a shell in a terminal (by someone who
   * knows what they are doing) will get an item for $CWD in the places sidebar,
   * and "normal" applications launched from the desktop shell (whose $CWD is
   * $HOME) won't get any extra clutter in the sidebar.
   */
  if (!g_file_equal (home_file, cwd_file))
    gtk_places_sidebar_add_shortcut (GTK_PLACES_SIDEBAR (priv->places_sidebar), cwd_file);

  g_object_unref (home_file);

 out:
  g_object_unref (cwd_file);
}

/* GtkWidget::map method */
static void
gtk_file_chooser_widget_map (GtkWidget *widget)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (widget);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  profile_start ("start", NULL);

  GTK_WIDGET_CLASS (gtk_file_chooser_widget_parent_class)->map (widget);

  settings_load (impl);

  add_cwd_to_sidebar_if_needed (impl);

  if (priv->operation_mode == OPERATION_MODE_BROWSE)
    {
      switch (priv->reload_state)
        {
        case RELOAD_EMPTY:
	  set_startup_mode (impl);
          break;
        
        case RELOAD_HAS_FOLDER:
          /* Nothing; we are already loading or loaded, so we
           * don't need to reload
           */
          break;

        default:
          g_assert_not_reached ();
      }
    }

  profile_end ("end", NULL);
}

/* GtkWidget::unmap method */
static void
gtk_file_chooser_widget_unmap (GtkWidget *widget)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (widget);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  settings_save (impl);

  cancel_all_operations (impl);
  priv->reload_state = RELOAD_EMPTY;

  GTK_WIDGET_CLASS (gtk_file_chooser_widget_parent_class)->unmap (widget);
}

#define COMPARE_DIRECTORIES										       \
  GtkFileChooserWidget *impl = user_data;								       \
  GtkFileChooserWidgetPrivate *priv = impl->priv;							       \
  GtkFileSystemModel *fs_model = GTK_FILE_SYSTEM_MODEL (model);                                                \
  gboolean dir_a, dir_b;										       \
													       \
  dir_a = g_value_get_boolean (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_IS_FOLDER));           \
  dir_b = g_value_get_boolean (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_IS_FOLDER));           \
													       \
  if (priv->sort_directories_first && dir_a != dir_b)											       \
    return priv->list_sort_ascending ? (dir_a ? -1 : 1) : (dir_a ? 1 : -1) /* Directories *always* go first */

/* Sort callback for the filename column */
static gint
name_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      const char *key_a, *key_b;
      gint result;

      key_a = g_value_get_string (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_NAME_COLLATED));
      key_b = g_value_get_string (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_NAME_COLLATED));

      if (key_a && key_b)
        result = strcmp (key_a, key_b);
      else if (key_a)
        result = 1;
      else if (key_b)
        result = -1;
      else
        result = 0;

      return result;
    }
}

/* Sort callback for the size column */
static gint
size_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      gint64 size_a, size_b;

      size_a = g_value_get_int64 (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_SIZE));
      size_b = g_value_get_int64 (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_SIZE));

      return size_a < size_b ? -1 : (size_a == size_b ? 0 : 1);
    }
}

/* Sort callback for the mtime column */
static gint
mtime_sort_func (GtkTreeModel *model,
		 GtkTreeIter  *a,
		 GtkTreeIter  *b,
		 gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    {
      glong ta, tb;

      ta = g_value_get_long (_gtk_file_system_model_get_value (fs_model, a, MODEL_COL_MTIME));
      tb = g_value_get_long (_gtk_file_system_model_get_value (fs_model, b, MODEL_COL_MTIME));

      return ta < tb ? -1 : (ta == tb ? 0 : 1);
    }
}

/* Callback used when the sort column changes.  We cache the sort order for use
 * in name_sort_func().
 */
static void
list_sort_column_changed_cb (GtkTreeSortable       *sortable,
			     GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gint sort_column_id;
  GtkSortType sort_type;

  if (gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &sort_type))
    {
      priv->list_sort_ascending = (sort_type == GTK_SORT_ASCENDING);
      priv->sort_column = sort_column_id;
      priv->sort_order = sort_type;
    }
}

static void
set_busy_cursor (GtkFileChooserWidget *impl,
		 gboolean               busy)
{
  GtkWidget *widget;
  GtkWindow *toplevel;
  GdkDisplay *display;
  GdkCursor *cursor;

  toplevel = get_toplevel (GTK_WIDGET (impl));
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

/* Creates a sort model to wrap the file system model and sets it on the tree view */
static void
load_set_model (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  profile_start ("start", NULL);

  g_assert (priv->browse_files_model != NULL);

  profile_msg ("    gtk_tree_view_set_model start", NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->browse_files_tree_view),
			   GTK_TREE_MODEL (priv->browse_files_model));
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (priv->browse_files_tree_view));
  file_list_set_sort_column_ids (impl);
  set_sort_column (impl);
  profile_msg ("    gtk_tree_view_set_model end", NULL);
  priv->list_sort_ascending = TRUE;

  profile_end ("end", NULL);
}

/* Timeout callback used when the loading timer expires */
static gboolean
load_timeout_cb (gpointer data)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (data);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  profile_start ("start", NULL);

  g_assert (priv->load_state == LOAD_PRELOAD);
  g_assert (priv->load_timeout_id != 0);
  g_assert (priv->browse_files_model != NULL);

  priv->load_timeout_id = 0;
  priv->load_state = LOAD_LOADING;

  load_set_model (impl);

  profile_end ("end", NULL);

  return FALSE;
}

/* Sets up a new load timer for the model and switches to the LOAD_PRELOAD state */
static void
load_setup_timer (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_assert (priv->load_timeout_id == 0);
  g_assert (priv->load_state != LOAD_PRELOAD);

  priv->load_timeout_id = gdk_threads_add_timeout (MAX_LOADING_TIME, load_timeout_cb, impl);
  g_source_set_name_by_id (priv->load_timeout_id, "[gtk+] load_timeout_cb");
  priv->load_state = LOAD_PRELOAD;
}

/* Removes the load timeout; changes the impl->load_state to the specified value. */
static void
load_remove_timer (GtkFileChooserWidget *impl, LoadState new_load_state)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->load_timeout_id != 0)
    {
      g_assert (priv->load_state == LOAD_PRELOAD);

      g_source_remove (priv->load_timeout_id);
      priv->load_timeout_id = 0;
    }
  else
    g_assert (priv->load_state == LOAD_EMPTY ||
	      priv->load_state == LOAD_LOADING ||
	      priv->load_state == LOAD_FINISHED);

  g_assert (new_load_state == LOAD_EMPTY ||
	    new_load_state == LOAD_LOADING ||
	    new_load_state == LOAD_FINISHED);
  priv->load_state = new_load_state;
}

/* Selects the first row in the file list */
static void
browse_files_select_first_row (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreePath *path;
  GtkTreeIter dummy_iter;
  GtkTreeModel *tree_model;

  tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->browse_files_tree_view));

  if (!tree_model)
    return;

  path = gtk_tree_path_new_from_indices (0, -1);

  /* If the list is empty, do nothing. */
  if (gtk_tree_model_get_iter (tree_model, &dummy_iter, path))
    {
      /* Although the following call to gtk_tree_view_set_cursor() is intended to
       * only change the focus to the first row (not select it), GtkTreeView *will*
       * select the row anyway due to bug #492206.  So, we'll use a flag to
       * keep our own callbacks from changing the location_entry when the selection
       * is changed.  This entire function, browse_files_select_first_row(), may
       * go away when that bug is fixed in GtkTreeView.
       */
      priv->auto_selecting_first_row = TRUE;

      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->browse_files_tree_view), path, NULL, FALSE);

      priv->auto_selecting_first_row = FALSE;
    }
  gtk_tree_path_free (path);
}

struct center_selected_row_closure {
  GtkFileChooserWidget *impl;
  gboolean already_centered;
};

/* Callback used from gtk_tree_selection_selected_foreach(); centers the
 * selected row in the tree view.
 */
static void
center_selected_row_foreach_cb (GtkTreeModel      *model,
				GtkTreePath       *path,
				GtkTreeIter       *iter,
				gpointer           data)
{
  struct center_selected_row_closure *closure;

  closure = data;
  if (closure->already_centered)
    return;

  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (closure->impl->priv->browse_files_tree_view), path, NULL, TRUE, 0.5, 0.0);
  closure->already_centered = TRUE;
}

/* Centers the selected row in the tree view */
static void
browse_files_center_selected_row (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  struct center_selected_row_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.already_centered = FALSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, center_selected_row_foreach_cb, &closure);
}

static gboolean
show_and_select_files (GtkFileChooserWidget *impl,
		       GSList                *files)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;
  GtkFileSystemModel *fsmodel;
  gboolean enabled_hidden, removed_filters;
  gboolean selected_a_file;
  GSList *walk;

  g_assert (priv->load_state == LOAD_FINISHED);
  g_assert (priv->browse_files_model != NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  fsmodel = GTK_FILE_SYSTEM_MODEL (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->browse_files_tree_view)));

  g_assert (fsmodel == priv->browse_files_model);

  enabled_hidden = priv->show_hidden;
  removed_filters = (priv->current_filter == NULL);

  selected_a_file = FALSE;

  for (walk = files; walk; walk = walk->next)
    {
      GFile *file = walk->data;
      GtkTreeIter iter;

      /* Is it a hidden file? */

      if (!_gtk_file_system_model_get_iter_for_file (fsmodel, &iter, file))
        continue;

      if (!_gtk_file_system_model_iter_is_visible (fsmodel, &iter))
        {
          GFileInfo *info = _gtk_file_system_model_get_info (fsmodel, &iter);

          if (!enabled_hidden &&
              (g_file_info_get_is_hidden (info) ||
               g_file_info_get_is_backup (info)))
            {
              g_object_set (impl, "show-hidden", TRUE, NULL);
              enabled_hidden = TRUE;
            }
        }

      /* Is it a filtered file? */

      if (!_gtk_file_system_model_get_iter_for_file (fsmodel, &iter, file))
        continue; /* re-get the iter as it may change when the model refilters */

      if (!_gtk_file_system_model_iter_is_visible (fsmodel, &iter))
        {
	  /* Maybe we should have a way to ask the fsmodel if it had filtered a file */
	  if (!removed_filters)
	    {
	      set_current_filter (impl, NULL);
	      removed_filters = TRUE;
	    }
	}

      /* Okay, can we select the file now? */
          
      if (!_gtk_file_system_model_get_iter_for_file (fsmodel, &iter, file))
        continue;

      if (_gtk_file_system_model_iter_is_visible (fsmodel, &iter))
        {
          GtkTreePath *path;

          gtk_tree_selection_select_iter (selection, &iter);

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (fsmodel), &iter);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->browse_files_tree_view),
                                    path, NULL, FALSE);
          gtk_tree_path_free (path);

          selected_a_file = TRUE;
        }
    }

  browse_files_center_selected_row (impl);

  return selected_a_file;
}

/* Processes the pending operation when a folder is finished loading */
static void
pending_select_files_process (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_assert (priv->load_state == LOAD_FINISHED);
  g_assert (priv->browse_files_model != NULL);

  if (priv->pending_select_files)
    {
      show_and_select_files (impl, priv->pending_select_files);
      pending_select_files_free (impl);
      browse_files_center_selected_row (impl);
    }
  else
    {
      /* We only select the first row if the chooser is actually mapped ---
       * selecting the first row is to help the user when he is interacting with
       * the chooser, but sometimes a chooser works not on behalf of the user,
       * but rather on behalf of something else like GtkFileChooserButton.  In
       * that case, the chooser's selection should be what the caller expects,
       * as the user can't see that something else got selected.  See bug #165264.
       */
      if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN &&
          gtk_widget_get_mapped (GTK_WIDGET (impl)))
	browse_files_select_first_row (impl);
    }

  g_assert (priv->pending_select_files == NULL);
}

static void
show_error_on_reading_current_folder (GtkFileChooserWidget *impl, GError *error)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFileInfo *info;
  char *msg;

  info = g_file_query_info (priv->current_folder,
			    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
			    G_FILE_QUERY_INFO_NONE,
			    NULL,
			    NULL);
  if (info)
    {
      msg = g_strdup_printf (_("Could not read the contents of %s"), g_file_info_get_display_name (info));
      g_object_unref (info);
    }
  else
    msg = g_strdup (_("Could not read the contents of the folder"));

  error_message (impl, msg, error->message);
  g_free (msg);
}

/* Callback used when the file system model finishes loading */
static void
browse_files_model_finished_loading_cb (GtkFileSystemModel    *model,
                                        GError                *error,
					GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  profile_start ("start", NULL);

  if (error)
    show_error_on_reading_current_folder (impl, error);

  if (priv->load_state == LOAD_PRELOAD)
    {
      load_remove_timer (impl, LOAD_FINISHED);
      load_set_model (impl);
    }
  else if (priv->load_state == LOAD_LOADING)
    {
      /* Nothing */
    }
  else
    {
      /* We can't g_assert_not_reached(), as something other than us may have
       *  initiated a folder reload.  See #165556.
       */
      profile_end ("end", NULL);
      return;
    }

  g_assert (priv->load_timeout_id == 0);

  priv->load_state = LOAD_FINISHED;

  pending_select_files_process (impl);
  set_busy_cursor (impl, FALSE);
#ifdef PROFILE_FILE_CHOOSER
  access ("MARK: *** FINISHED LOADING", F_OK);
#endif

  profile_end ("end", NULL);
}

static void
stop_loading_and_clear_list_model (GtkFileChooserWidget *impl,
                                   gboolean remove_from_treeview)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  load_remove_timer (impl, LOAD_EMPTY);
  
  if (priv->browse_files_model)
    {
      g_object_unref (priv->browse_files_model);
      priv->browse_files_model = NULL;
    }

  if (remove_from_treeview)
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->browse_files_tree_view), NULL);
}

/* Replace 'target' with 'replacement' in the input string. */
static gchar *
string_replace (const gchar *input,
                const gchar *target,
                const gchar *replacement)
{
  gchar **pieces;
  gchar *output;

  pieces = g_strsplit (input, target, -1);
  output = g_strjoinv (replacement, pieces);
  g_strfreev (pieces);

  return output;
}

static char *
my_g_format_time_for_display (GtkFileChooserWidget *impl,
                              glong secs)
{
  GDateTime *now, *time;
  GTimeSpan time_diff;
  gchar *clock_format;
  gboolean use_24 = TRUE;
  const gchar *format;
  gchar *date_str;
  GSettings *settings;

  now = g_date_time_new_now_local ();
  time = g_date_time_new_from_unix_local (secs);
  time_diff = g_date_time_difference (now, time);

  settings = _gtk_file_chooser_get_settings_for_widget (GTK_WIDGET (impl));
  clock_format = g_settings_get_string (settings, "clock-format");
  use_24 = g_strcmp0 (clock_format, "24h") == 0;
  g_free (clock_format);

  /* Translators: see g_date_time_format() for details on the format */
  if (time_diff >= 0 && time_diff < G_TIME_SPAN_DAY)
    format = use_24 ? _("%H:%M") : _("%-I:%M %P");
  else if (time_diff >= 0 && time_diff < 2 * G_TIME_SPAN_DAY)
    format = use_24 ? _("Yesterday at %H:%M") : _("Yesterday at %-I:%M %P");
  else if (time_diff >= 0 && time_diff < 7 * G_TIME_SPAN_DAY)
    format = "%A"; /* Days from last week */
  else
    format = "%x"; /* Any other date */

  date_str = g_date_time_format (time, format);

  if (g_get_charset (NULL))
    {
      gchar *ret;
      ret = string_replace (date_str, ":", "\xE2\x80\x8E∶");
      g_free (date_str);
      date_str = ret;
    }

  g_date_time_unref (time);
  g_date_time_unref (now);

  return date_str;
}

static void
copy_attribute (GFileInfo *to, GFileInfo *from, const char *attribute)
{
  GFileAttributeType type;
  gpointer value;

  if (g_file_info_get_attribute_data (from, attribute, &type, &value, NULL))
    g_file_info_set_attribute (to, attribute, type, value);
}

static void
file_system_model_got_thumbnail (GObject *object, GAsyncResult *res, gpointer data)
{
  GtkFileSystemModel *model = data; /* might be unreffed if operation was cancelled */
  GFile *file = G_FILE (object);
  GFileInfo *queried, *info;
  GtkTreeIter iter;

  queried = g_file_query_info_finish (file, res, NULL);
  if (queried == NULL)
    return;

  gdk_threads_enter ();

  /* now we know model is valid */

  /* file was deleted */
  if (!_gtk_file_system_model_get_iter_for_file (model, &iter, file))
    {
      g_object_unref (queried);
      gdk_threads_leave ();
      return;
    }

  info = g_file_info_dup (_gtk_file_system_model_get_info (model, &iter));

  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_STANDARD_ICON);

  _gtk_file_system_model_update_file (model, file, info);

  g_object_unref (info);
  g_object_unref (queried);

  gdk_threads_leave ();
}

static gboolean
file_system_model_set (GtkFileSystemModel *model,
                       GFile              *file,
                       GFileInfo          *info,
                       int                 column,
                       GValue             *value,
                       gpointer            data)
{
  GtkFileChooserWidget *impl = data;
  GtkFileChooserWidgetPrivate *priv = impl->priv;
 
  switch (column)
    {
    case MODEL_COL_FILE:
      g_value_set_object (value, file);
      break;
    case MODEL_COL_NAME:
      if (info == NULL)
        g_value_set_string (value, DEFAULT_NEW_FOLDER_NAME);
      else 
        g_value_set_string (value, g_file_info_get_display_name (info));
      break;
    case MODEL_COL_NAME_COLLATED:
      if (info == NULL)
        g_value_take_string (value, g_utf8_collate_key_for_filename (DEFAULT_NEW_FOLDER_NAME, -1));
      else 
        g_value_take_string (value, g_utf8_collate_key_for_filename (g_file_info_get_display_name (info), -1));
      break;
    case MODEL_COL_IS_FOLDER:
      g_value_set_boolean (value, info == NULL || _gtk_file_info_consider_as_directory (info));
      break;
    case MODEL_COL_IS_SENSITIVE:
      if (info)
        {
          gboolean sensitive = TRUE;

          if (!(priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
		|| priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
            {
              sensitive = TRUE; /* for file modes... */
            }
          else if (!_gtk_file_info_consider_as_directory (info))
            {
              sensitive = FALSE; /* for folder modes, files are not sensitive... */
            }
          else
            {
	      /* ... and for folder modes, folders are sensitive only if the filter says so */
              GtkTreeIter iter;
              if (!_gtk_file_system_model_get_iter_for_file (model, &iter, file))
                g_assert_not_reached ();
              sensitive = !_gtk_file_system_model_iter_is_filtered_out (model, &iter);
            }

          g_value_set_boolean (value, sensitive);
        }
      else
        g_value_set_boolean (value, TRUE);
      break;
    case MODEL_COL_SURFACE:
      if (info)
        {
          if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_ICON))
            {
              g_value_take_boxed (value, _gtk_file_info_render_icon (info, GTK_WIDGET (impl), priv->icon_size));
            }
          else
            {
              GtkTreeModel *tree_model;
              GtkTreePath *path, *start, *end;
              GtkTreeIter iter;

              if (priv->browse_files_tree_view == NULL ||
                  g_file_info_has_attribute (info, "filechooser::queried"))
                return FALSE;

              tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->browse_files_tree_view));
              if (tree_model != GTK_TREE_MODEL (model))
                return FALSE;

              if (!_gtk_file_system_model_get_iter_for_file (model,
                                                             &iter,
                                                             file))
                g_assert_not_reached ();
              if (!gtk_tree_view_get_visible_range (GTK_TREE_VIEW (priv->browse_files_tree_view), &start, &end))
                return FALSE;
              path = gtk_tree_model_get_path (tree_model, &iter);
              if (gtk_tree_path_compare (start, path) != 1 &&
                  gtk_tree_path_compare (path, end) != 1)
                {
                  g_file_info_set_attribute_boolean (info, "filechooser::queried", TRUE);
                  g_file_query_info_async (file,
                                           G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                                           G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                                           G_FILE_ATTRIBUTE_STANDARD_ICON,
                                           G_FILE_QUERY_INFO_NONE,
                                           G_PRIORITY_DEFAULT,
                                           _gtk_file_system_model_get_cancellable (model),
                                           file_system_model_got_thumbnail,
                                           model);
                }
              gtk_tree_path_free (path);
              gtk_tree_path_free (start);
              gtk_tree_path_free (end);
              return FALSE;
            }
        }
      else
        g_value_set_boxed (value, NULL);
      break;
    case MODEL_COL_SIZE:
      g_value_set_int64 (value, info ? g_file_info_get_size (info) : 0);
      break;
    case MODEL_COL_SIZE_TEXT:
      if (info == NULL || _gtk_file_info_consider_as_directory (info))
        g_value_set_string (value, NULL);
      else
        g_value_take_string (value, g_format_size (g_file_info_get_size (info)));
      break;
    case MODEL_COL_MTIME:
    case MODEL_COL_MTIME_TEXT:
      {
        GTimeVal tv;
        if (info == NULL)
          break;
        g_file_info_get_modification_time (info, &tv);
        if (column == MODEL_COL_MTIME)
          g_value_set_long (value, tv.tv_sec);
        else if (tv.tv_sec == 0)
          g_value_set_static_string (value, _("Unknown"));
        else
          g_value_take_string (value, my_g_format_time_for_display (impl, tv.tv_sec));
        break;
      }
    case MODEL_COL_ELLIPSIZE:
      g_value_set_enum (value, info ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE);
      break;
    case MODEL_COL_LOCATION_TEXT:
      {
        GFile *home_location;
        GFile *dir_location;
        gchar *location;

        home_location = g_file_new_for_path (g_get_home_dir ());
        if (file)
          dir_location = g_file_get_parent (file);
        else
          dir_location = NULL;

        if (dir_location && g_file_equal (home_location, dir_location))
          location = g_strdup (_("Home"));
        else if (dir_location && g_file_has_prefix (dir_location, home_location))
          {
            gchar *relative_path;

            relative_path = g_file_get_relative_path (home_location, dir_location);
            location = g_filename_display_name (relative_path);

            g_free (relative_path);
          }
        else
          location = g_strdup ("");

        g_value_take_string (value, location);

        if (dir_location)
          g_object_unref (dir_location);
        g_object_unref (home_location);
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

/* Gets rid of the old list model and creates a new one for the current folder */
static gboolean
set_list_model (GtkFileChooserWidget *impl,
		GError               **error)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_assert (priv->current_folder != NULL);

  profile_start ("start", NULL);

  stop_loading_and_clear_list_model (impl, TRUE);

  set_busy_cursor (impl, TRUE);

  priv->browse_files_model = 
    _gtk_file_system_model_new_for_directory (priv->current_folder,
					      MODEL_ATTRIBUTES,
					      file_system_model_set,
					      impl,
					      MODEL_COLUMN_TYPES);

  _gtk_file_system_model_set_show_hidden (priv->browse_files_model, priv->show_hidden);

  profile_msg ("    set sort function", NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->browse_files_model), MODEL_COL_NAME, name_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->browse_files_model), MODEL_COL_SIZE, size_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->browse_files_model), MODEL_COL_MTIME, mtime_sort_func, impl, NULL);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (priv->browse_files_model), NULL, NULL, NULL);
  set_sort_column (impl);
  priv->list_sort_ascending = TRUE;
  g_signal_connect (priv->browse_files_model, "sort-column-changed",
		    G_CALLBACK (list_sort_column_changed_cb), impl);

  load_setup_timer (impl); /* This changes the state to LOAD_PRELOAD */

  g_signal_connect (priv->browse_files_model, "finished-loading",
		    G_CALLBACK (browse_files_model_finished_loading_cb), impl);

  _gtk_file_system_model_set_filter (priv->browse_files_model, priv->current_filter);

  profile_end ("end", NULL);

  return TRUE;
}

struct update_chooser_entry_selected_foreach_closure {
  int num_selected;
  GtkTreeIter first_selected_iter;
};

static gint
compare_utf8_filenames (const gchar *a,
			const gchar *b)
{
  gchar *a_folded, *b_folded;
  gint retval;

  a_folded = g_utf8_strdown (a, -1);
  b_folded = g_utf8_strdown (b, -1);

  retval = strcmp (a_folded, b_folded);

  g_free (a_folded);
  g_free (b_folded);

  return retval;
}

static void
update_chooser_entry_selected_foreach (GtkTreeModel *model,
				       GtkTreePath *path,
				       GtkTreeIter *iter,
				       gpointer data)
{
  struct update_chooser_entry_selected_foreach_closure *closure;

  closure = data;
  closure->num_selected++;

  if (closure->num_selected == 1)
    closure->first_selected_iter = *iter;
}

static void
update_chooser_entry (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;
  struct update_chooser_entry_selected_foreach_closure closure;

  /* no need to update the file chooser's entry if there's no entry */
  if (priv->operation_mode == OPERATION_MODE_SEARCH ||
      !priv->location_entry)
    return;

  if (!(priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
        || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
        || ((priv->action == GTK_FILE_CHOOSER_ACTION_OPEN
             || priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
            && priv->location_mode == LOCATION_MODE_FILENAME_ENTRY)))
    return;

  g_assert (priv->location_entry != NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  closure.num_selected = 0;
  gtk_tree_selection_selected_foreach (selection, update_chooser_entry_selected_foreach, &closure);

  if (closure.num_selected == 0)
    {
      if (priv->operation_mode == OPERATION_MODE_RECENT)
	_gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), NULL);
      else
	goto maybe_clear_entry;
    }
  else if (closure.num_selected == 1)
    {
      if (priv->operation_mode == OPERATION_MODE_BROWSE)
        {
	  GFileInfo *info;
	  gboolean change_entry;

	  info = _gtk_file_system_model_get_info (priv->browse_files_model, &closure.first_selected_iter);

	  /* If the cursor moved to the row of the newly created folder,
	   * retrieving info will return NULL.
	   */
	  if (!info)
	    return;

	  g_free (priv->browse_files_last_selected_name);
	  priv->browse_files_last_selected_name =
	    g_strdup (g_file_info_get_display_name (info));

	  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
	      priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
	      priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    {
	      /* Don't change the name when clicking on a folder... */
	      change_entry = (! _gtk_file_info_consider_as_directory (info));
	    }
	  else
	    change_entry = TRUE; /* ... unless we are in SELECT_FOLDER mode */

	  if (change_entry && !priv->auto_selecting_first_row)
	    {
              g_signal_handlers_block_by_func (priv->location_entry, G_CALLBACK (location_entry_changed_cb), impl);
	      gtk_entry_set_text (GTK_ENTRY (priv->location_entry), priv->browse_files_last_selected_name);
              g_signal_handlers_unblock_by_func (priv->location_entry, G_CALLBACK (location_entry_changed_cb), impl);

	      if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
		_gtk_file_chooser_entry_select_filename (GTK_FILE_CHOOSER_ENTRY (priv->location_entry));
	    }

	  return;
        }
      else if (priv->operation_mode == OPERATION_MODE_RECENT
	       && priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  GFile *folder;

	  /* Set the base folder on the name entry, so it will do completion relative to the correct recent-folder */

	  gtk_tree_model_get (GTK_TREE_MODEL (priv->recent_model), &closure.first_selected_iter,
			      MODEL_COL_FILE, &folder,
			      -1);
	  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (priv->location_entry), folder);
	  g_object_unref (folder);
	  return;
	}
    }
  else
    {
      g_assert (!(priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
                  priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER));

      /* Multiple selection, so just clear the entry. */
      g_free (priv->browse_files_last_selected_name);
      priv->browse_files_last_selected_name = NULL;

      g_signal_handlers_block_by_func (priv->location_entry, G_CALLBACK (location_entry_changed_cb), impl);
      gtk_entry_set_text (GTK_ENTRY (priv->location_entry), "");
      g_signal_handlers_unblock_by_func (priv->location_entry, G_CALLBACK (location_entry_changed_cb), impl);
      return;
    }

 maybe_clear_entry:

  if ((priv->action == GTK_FILE_CHOOSER_ACTION_OPEN || priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
      && priv->browse_files_last_selected_name)
    {
      const char *entry_text;
      int len;
      gboolean clear_entry;

      entry_text = gtk_entry_get_text (GTK_ENTRY (priv->location_entry));
      len = strlen (entry_text);
      if (len != 0)
        {
          /* The file chooser entry may have appended a "/" to its text.
           * So take it out, and compare the result to the old selection.
           */
          if (entry_text[len - 1] == G_DIR_SEPARATOR)
            {
              gchar *tmp;

              tmp = g_strndup (entry_text, len - 1);
              clear_entry = (compare_utf8_filenames (priv->browse_files_last_selected_name, tmp) == 0);
              g_free (tmp);
            }
          else
            clear_entry = (compare_utf8_filenames (priv->browse_files_last_selected_name, entry_text) == 0);
        }
      else
        clear_entry = FALSE;

      if (clear_entry)
        {
          g_signal_handlers_block_by_func (priv->location_entry, G_CALLBACK (location_entry_changed_cb), impl);
          gtk_entry_set_text (GTK_ENTRY (priv->location_entry), "");
          g_signal_handlers_unblock_by_func (priv->location_entry, G_CALLBACK (location_entry_changed_cb), impl);
        }
    }
}

static gboolean
gtk_file_chooser_widget_set_current_folder (GtkFileChooser  *chooser,
					     GFile           *file,
					     GError         **error)
{
  return gtk_file_chooser_widget_update_current_folder (chooser, file, FALSE, FALSE, error);
}


struct UpdateCurrentFolderData
{
  GtkFileChooserWidget *impl;
  GFile *file;
  gboolean keep_trail;
  gboolean clear_entry;
  GFile *original_file;
  GError *original_error;
};

static void
update_current_folder_mount_enclosing_volume_cb (GCancellable        *cancellable,
                                                 GtkFileSystemVolume *volume,
                                                 const GError        *error,
                                                 gpointer             user_data)
{
  struct UpdateCurrentFolderData *data = user_data;
  GtkFileChooserWidget *impl = data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);

  if (cancellable != priv->update_current_folder_cancellable)
    goto out;

  priv->update_current_folder_cancellable = NULL;
  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  if (error)
    {
      error_changing_folder_dialog (data->impl, data->file, g_error_copy (error));
      priv->reload_state = RELOAD_EMPTY;
      goto out;
    }

  change_folder_and_display_error (impl, data->file, data->clear_entry);

out:
  g_object_unref (data->impl);
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
update_current_folder_get_info_cb (GCancellable *cancellable,
				   GFileInfo    *info,
				   const GError *error,
				   gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct UpdateCurrentFolderData *data = user_data;
  GtkFileChooserWidget *impl = data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (cancellable != priv->update_current_folder_cancellable)
    goto out;

  priv->update_current_folder_cancellable = NULL;
  priv->reload_state = RELOAD_EMPTY;

  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  if (error)
    {
      GFile *parent_file;

      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
        {
          GMountOperation *mount_operation;
          GtkWidget *toplevel;

          g_object_unref (cancellable);
          toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));

          mount_operation = gtk_mount_operation_new (GTK_WINDOW (toplevel));

          set_busy_cursor (impl, TRUE);

          priv->update_current_folder_cancellable =
            _gtk_file_system_mount_enclosing_volume (priv->file_system, data->file,
                                                     mount_operation,
                                                     update_current_folder_mount_enclosing_volume_cb,
                                                     data);

          return;
        }

      if (!data->original_file)
        {
	  data->original_file = g_object_ref (data->file);
	  data->original_error = g_error_copy (error);
	}

      parent_file = g_file_get_parent (data->file);

      /* get parent path and try to change the folder to that */
      if (parent_file)
        {
	  g_object_unref (data->file);
	  data->file = parent_file;

	  g_object_unref (cancellable);

	  /* restart the update current folder operation */
	  priv->reload_state = RELOAD_HAS_FOLDER;

	  priv->update_current_folder_cancellable =
	    _gtk_file_system_get_info (priv->file_system, data->file,
				       "standard::type",
				       update_current_folder_get_info_cb,
				       data);

	  set_busy_cursor (impl, TRUE);

	  return;
	}
      else
        {
          /* Error and bail out, ignoring "not found" errors since they're useless:
           * they only happen when a program defaults to a folder that has been (re)moved.
           */
          if (!g_error_matches (data->original_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            error_changing_folder_dialog (impl, data->original_file, data->original_error);
          else
            g_error_free (data->original_error);

	  g_object_unref (data->original_file);

	  goto out;
	}
    }

  if (data->original_file)
    {
      /* Error and bail out, ignoring "not found" errors since they're useless:
       * they only happen when a program defaults to a folder that has been (re)moved.
       */
      if (!g_error_matches (data->original_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        error_changing_folder_dialog (impl, data->original_file, data->original_error);
      else
        g_error_free (data->original_error);

      g_object_unref (data->original_file);
    }

  if (! _gtk_file_info_consider_as_directory (info))
    goto out;

  _gtk_path_bar_set_file (GTK_PATH_BAR (priv->browse_path_bar), data->file, data->keep_trail);

  if (priv->current_folder != data->file)
    {
      if (priv->current_folder)
	g_object_unref (priv->current_folder);

      priv->current_folder = g_object_ref (data->file);
    }

  priv->reload_state = RELOAD_HAS_FOLDER;

  /* Set the folder on the save entry */

  if (priv->location_entry)
    {
      _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (priv->location_entry),
					       priv->current_folder);

      if (data->clear_entry)
        gtk_entry_set_text (GTK_ENTRY (priv->location_entry), "");
    }

  /* Create a new list model.  This is slightly evil; we store the result value
   * but perform more actions rather than returning immediately even if it
   * generates an error.
   */
  set_list_model (impl, NULL);

  /* Refresh controls */

  gtk_places_sidebar_set_location (GTK_PLACES_SIDEBAR (priv->places_sidebar), priv->current_folder);

  g_signal_emit_by_name (impl, "current-folder-changed", 0);

  check_preview_change (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);

out:
  g_object_unref (data->impl);
  g_object_unref (data->file);
  g_free (data);

  g_object_unref (cancellable);
}

static gboolean
gtk_file_chooser_widget_update_current_folder (GtkFileChooser    *chooser,
						GFile             *file,
						gboolean           keep_trail,
						gboolean           clear_entry,
						GError           **error)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  struct UpdateCurrentFolderData *data;

  profile_start ("start", NULL);

  g_object_ref (file);

  operation_mode_set (impl, OPERATION_MODE_BROWSE);

  if (priv->local_only && !_gtk_file_has_native_path (file))
    {
      g_set_error_literal (error,
                           GTK_FILE_CHOOSER_ERROR,
                           GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
                           _("Cannot change to folder because it is not local"));

      g_object_unref (file);
      profile_end ("end - not local", NULL);
      return FALSE;
    }

  if (priv->update_current_folder_cancellable)
    g_cancellable_cancel (priv->update_current_folder_cancellable);

  /* Test validity of path here.  */
  data = g_new0 (struct UpdateCurrentFolderData, 1);
  data->impl = g_object_ref (impl);
  data->file = g_object_ref (file);
  data->keep_trail = keep_trail;
  data->clear_entry = clear_entry;

  priv->reload_state = RELOAD_HAS_FOLDER;

  priv->update_current_folder_cancellable =
    _gtk_file_system_get_info (priv->file_system, file,
			       "standard::type",
			       update_current_folder_get_info_cb,
			       data);

  set_busy_cursor (impl, TRUE);
  g_object_unref (file);

  profile_end ("end", NULL);
  return TRUE;
}

static GFile *
gtk_file_chooser_widget_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->operation_mode == OPERATION_MODE_SEARCH ||
      priv->operation_mode == OPERATION_MODE_RECENT)
    return NULL;
 
  if (priv->current_folder)
    return g_object_ref (priv->current_folder);

  return NULL;
}

static void
gtk_file_chooser_widget_set_current_name (GtkFileChooser *chooser,
					   const gchar    *name)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_return_if_fail (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
		    priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

  pending_select_files_free (impl);
  gtk_entry_set_text (GTK_ENTRY (priv->location_entry), name);
}

static gchar *
gtk_file_chooser_widget_get_current_name (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_return_val_if_fail (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
	                priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
	                NULL);

  return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->location_entry)));
}

static gboolean
gtk_file_chooser_widget_select_file (GtkFileChooser  *chooser,
				      GFile           *file,
				      GError         **error)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *parent_file;
  gboolean same_path;

  parent_file = g_file_get_parent (file);

  if (!parent_file)
    return gtk_file_chooser_set_current_folder_file (chooser, file, error);

  if (priv->operation_mode == OPERATION_MODE_SEARCH ||
      priv->operation_mode == OPERATION_MODE_RECENT ||
      priv->load_state == LOAD_EMPTY)
    {
      same_path = FALSE;
    }
  else
    {
      g_assert (priv->current_folder != NULL);

      same_path = g_file_equal (parent_file, priv->current_folder);
    }

  if (same_path && priv->load_state == LOAD_FINISHED)
    {
      gboolean result;
      GSList files;

      files.data = (gpointer) file;
      files.next = NULL;

      result = show_and_select_files (impl, &files);
      g_object_unref (parent_file);
      return result;
    }

  pending_select_files_add (impl, file);

  if (!same_path)
    {
      gboolean result;

      result = gtk_file_chooser_set_current_folder_file (chooser, parent_file, error);
      g_object_unref (parent_file);
      return result;
    }

  g_object_unref (parent_file);
  return TRUE;
}

static void
gtk_file_chooser_widget_unselect_file (GtkFileChooser *chooser,
					GFile          *file)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeView *tree_view = GTK_TREE_VIEW (priv->browse_files_tree_view);
  GtkTreeIter iter;

  if (!priv->browse_files_model)
    return;

  if (!_gtk_file_system_model_get_iter_for_file (priv->browse_files_model,
                                                 &iter,
                                                 file))
    return;

  gtk_tree_selection_unselect_iter (gtk_tree_view_get_selection (tree_view),
                                    &iter);
}

static gboolean
maybe_select (GtkTreeModel *model, 
	      GtkTreePath  *path, 
	      GtkTreeIter  *iter, 
	      gpointer     data)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (data);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;
  gboolean is_sensitive;
  gboolean is_folder;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  
  gtk_tree_model_get (model, iter,
                      MODEL_COL_IS_FOLDER, &is_folder,
                      MODEL_COL_IS_SENSITIVE, &is_sensitive,
                      -1);

  if (is_sensitive &&
      ((is_folder && priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ||
       (!is_folder && priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)))
    gtk_tree_selection_select_iter (selection, iter);
  else
    gtk_tree_selection_unselect_iter (selection, iter);
    
  return FALSE;
}

static void
gtk_file_chooser_widget_select_all (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->operation_mode == OPERATION_MODE_SEARCH ||
      priv->operation_mode == OPERATION_MODE_RECENT)
    {
      GtkTreeSelection *selection;
      
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
      gtk_tree_selection_select_all (selection);
      return;
    }

  if (priv->select_multiple)
    gtk_tree_model_foreach (GTK_TREE_MODEL (priv->browse_files_model), 
			    maybe_select, impl);
}

static void
gtk_file_chooser_widget_unselect_all (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));

  gtk_tree_selection_unselect_all (selection);
  pending_select_files_free (impl);
}

/* Checks whether the filename entry for the Save modes contains a well-formed filename.
 *
 * is_well_formed_ret - whether what the user typed passes gkt_file_system_make_path()
 *
 * is_empty_ret - whether the file entry is totally empty
 *
 * is_file_part_empty_ret - whether the file part is empty (will be if user types "foobar/", and
 *                          the path will be “$cwd/foobar”)
 */
static void
check_save_entry (GtkFileChooserWidget *impl,
		  GFile                **file_ret,
		  gboolean              *is_well_formed_ret,
		  gboolean              *is_empty_ret,
		  gboolean              *is_file_part_empty_ret,
		  gboolean              *is_folder)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkFileChooserEntry *chooser_entry;
  GFile *current_folder;
  const char *file_part;
  GFile *file;
  GError *error;

  g_assert (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
	    || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
	    || ((priv->action == GTK_FILE_CHOOSER_ACTION_OPEN
		 || priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		&& priv->location_mode == LOCATION_MODE_FILENAME_ENTRY));

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (priv->location_entry);

  if (strlen (gtk_entry_get_text (GTK_ENTRY (chooser_entry))) == 0)
    {
      *file_ret = NULL;
      *is_well_formed_ret = TRUE;
      *is_empty_ret = TRUE;
      *is_file_part_empty_ret = TRUE;
      *is_folder = FALSE;

      return;
    }

  *is_empty_ret = FALSE;

  current_folder = _gtk_file_chooser_entry_get_current_folder (chooser_entry);
  if (!current_folder)
    {
      *file_ret = NULL;
      *is_well_formed_ret = FALSE;
      *is_file_part_empty_ret = FALSE;
      *is_folder = FALSE;

      return;
    }

  file_part = _gtk_file_chooser_entry_get_file_part (chooser_entry);

  if (!file_part || file_part[0] == '\0')
    {
      *file_ret = current_folder;
      *is_well_formed_ret = TRUE;
      *is_file_part_empty_ret = TRUE;
      *is_folder = TRUE;

      return;
    }

  *is_file_part_empty_ret = FALSE;

  error = NULL;
  file = g_file_get_child_for_display_name (current_folder, file_part, &error);
  g_object_unref (current_folder);

  if (!file)
    {
      error_building_filename_dialog (impl, error);
      *file_ret = NULL;
      *is_well_formed_ret = FALSE;
      *is_folder = FALSE;

      return;
    }

  *file_ret = file;
  *is_well_formed_ret = TRUE;
  *is_folder = _gtk_file_chooser_entry_get_is_folder (chooser_entry, file);
}

struct get_files_closure {
  GtkFileChooserWidget *impl;
  GSList *result;
  GFile *file_from_entry;
};

static void
get_files_foreach (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer      data)
{
  struct get_files_closure *info;
  GFile *file;
  GtkFileSystemModel *fs_model;

  info = data;
  fs_model = info->impl->priv->browse_files_model;

  file = _gtk_file_system_model_get_file (fs_model, iter);
  if (!file)
    return; /* We are on the editable row */

  if (!info->file_from_entry || !g_file_equal (info->file_from_entry, file))
    info->result = g_slist_prepend (info->result, g_object_ref (file));
}

static GSList *
gtk_file_chooser_widget_get_files (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  struct get_files_closure info;
  GtkWindow *toplevel;
  GtkWidget *current_focus;
  gboolean file_list_seen;

  info.impl = impl;
  info.result = NULL;
  info.file_from_entry = NULL;

  if (priv->operation_mode == OPERATION_MODE_SEARCH)
    return search_get_selected_files (impl);

  if (priv->operation_mode == OPERATION_MODE_RECENT)
    {
      if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  file_list_seen = TRUE;
	  goto file_entry;
	}
      else
	return recent_get_selected_files (impl);
    }

  toplevel = get_toplevel (GTK_WIDGET (impl));
  if (toplevel)
    current_focus = gtk_window_get_focus (toplevel);
  else
    current_focus = NULL;

  file_list_seen = FALSE;
  if (current_focus == priv->browse_files_tree_view)
    {
      GtkTreeSelection *selection;

    file_list:

      file_list_seen = TRUE;
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
      gtk_tree_selection_selected_foreach (selection, get_files_foreach, &info);

      /* If there is no selection in the file list, we probably have this situation:
       *
       * 1. The user typed a filename in the SAVE filename entry ("foo.txt").
       * 2. He then double-clicked on a folder ("bar") in the file list
       *
       * So we want the selection to be "bar/foo.txt".  Jump to the case for the
       * filename entry to see if that is the case.
       */
      if (info.result == NULL && priv->location_entry)
	goto file_entry;
    }
  else if (priv->location_entry && current_focus == priv->location_entry)
    {
      gboolean is_well_formed, is_empty, is_file_part_empty, is_folder;

    file_entry:

      check_save_entry (impl, &info.file_from_entry, &is_well_formed, &is_empty, &is_file_part_empty, &is_folder);

      if (is_empty)
	goto out;

      if (!is_well_formed)
	return NULL;

      if (is_file_part_empty && priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
	  g_object_unref (info.file_from_entry);
	  return NULL;
	}

      if (info.file_from_entry)
        info.result = g_slist_prepend (info.result, info.file_from_entry);
      else if (!file_list_seen) 
        goto file_list;
      else
        return NULL;
    }
  else if (priv->toplevel_last_focus_widget == priv->browse_files_tree_view)
    goto file_list;
  else if (priv->location_entry && priv->toplevel_last_focus_widget == priv->location_entry)
    goto file_entry;
  else
    {
      /* The focus is on a dialog's action area button or something else */
      if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
          priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	goto file_entry;
      else
	goto file_list; 
    }

 out:

  /* If there's no folder selected, and we're in SELECT_FOLDER mode, then we
   * fall back to the current directory */
  if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER &&
      info.result == NULL)
    {
      GFile *current_folder;

      current_folder = gtk_file_chooser_get_current_folder_file (chooser);

      if (current_folder)
        info.result = g_slist_prepend (info.result, current_folder);
    }

  return g_slist_reverse (info.result);
}

GFile *
gtk_file_chooser_widget_get_preview_file (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->preview_file)
    return g_object_ref (priv->preview_file);
  else
    return NULL;
}

static GtkFileSystem *
gtk_file_chooser_widget_get_file_system (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  return priv->file_system;
}

/* Shows or hides the filter widgets */
static void
show_filters (GtkFileChooserWidget *impl,
	      gboolean               show)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (show)
    gtk_widget_show (priv->filter_combo_hbox);
  else
    gtk_widget_hide (priv->filter_combo_hbox);

  update_extra_and_filters (impl);
}

static void
gtk_file_chooser_widget_add_filter (GtkFileChooser *chooser,
				     GtkFileFilter  *filter)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  const gchar *name;

  if (g_slist_find (priv->filters, filter))
    {
      g_warning ("gtk_file_chooser_add_filter() called on filter already in list\n");
      return;
    }

  g_object_ref_sink (filter);
  priv->filters = g_slist_append (priv->filters, filter);

  name = gtk_file_filter_get_name (filter);
  if (!name)
    name = "Untitled filter";	/* Place-holder, doesn't need to be marked for translation */

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->filter_combo), name);

  if (!g_slist_find (priv->filters, priv->current_filter))
    set_current_filter (impl, filter);

  show_filters (impl, TRUE);
}

static void
gtk_file_chooser_widget_remove_filter (GtkFileChooser *chooser,
					GtkFileFilter  *filter)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint filter_index;

  filter_index = g_slist_index (priv->filters, filter);

  if (filter_index < 0)
    {
      g_warning ("gtk_file_chooser_remove_filter() called on filter not in list\n");
      return;
    }

  priv->filters = g_slist_remove (priv->filters, filter);

  if (filter == priv->current_filter)
    {
      if (priv->filters)
	set_current_filter (impl, priv->filters->data);
      else
	set_current_filter (impl, NULL);
    }

  /* Remove row from the combo box */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->filter_combo));
  if (!gtk_tree_model_iter_nth_child  (model, &iter, NULL, filter_index))
    g_assert_not_reached ();

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  g_object_unref (filter);

  if (!priv->filters)
    show_filters (impl, FALSE);
}

static GSList *
gtk_file_chooser_widget_list_filters (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  return g_slist_copy (priv->filters);
}

static gboolean
gtk_file_chooser_widget_add_shortcut_folder (GtkFileChooser  *chooser,
					      GFile           *file,
					      GError         **error)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_places_sidebar_add_shortcut (GTK_PLACES_SIDEBAR (priv->places_sidebar), file);
  return TRUE;
}

static gboolean
gtk_file_chooser_widget_remove_shortcut_folder (GtkFileChooser  *chooser,
						 GFile           *file,
						 GError         **error)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_places_sidebar_remove_shortcut (GTK_PLACES_SIDEBAR (priv->places_sidebar), file);
  return TRUE;
}

static GSList *
gtk_file_chooser_widget_list_shortcut_folders (GtkFileChooser *chooser)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser);
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  return gtk_places_sidebar_list_shortcuts (GTK_PLACES_SIDEBAR (priv->places_sidebar));
}

/* Guesses a size based upon font sizes */
static void
find_good_size_from_style (GtkWidget *widget,
                           gint      *width,
                           gint      *height)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  double font_size;
  GdkScreen *screen;
  double resolution;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  screen = gtk_widget_get_screen (widget);
  if (screen)
    {
      resolution = gdk_screen_get_resolution (screen);
      if (resolution < 0.0) /* will be -1 if the resolution is not defined in the GdkScreen */
        resolution = 96.0;
    }
  else
    resolution = 96.0; /* wheeee */

  gtk_style_context_get (context, state, "font-size", &font_size, NULL);
  font_size = font_size * resolution / 72.0 + 0.5;

  *width = font_size * NUM_CHARS;
  *height = font_size * NUM_LINES;
}

static void
gtk_file_chooser_widget_get_default_size (GtkFileChooserEmbed *chooser_embed,
					   gint                *default_width,
					   gint                *default_height)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser_embed);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkRequisition req;
  int x, y, width, height;
  GSettings *settings;

  settings = _gtk_file_chooser_get_settings_for_widget (GTK_WIDGET (impl));

  g_settings_get (settings, SETTINGS_KEY_WINDOW_POSITION, "(ii)", &x, &y);
  g_settings_get (settings, SETTINGS_KEY_WINDOW_SIZE, "(ii)", &width, &height);

  if (x >= 0 && y >= 0 && width > 0 && height > 0)
    {
      *default_width = width;
      *default_height = height;
      return;
    }

  find_good_size_from_style (GTK_WIDGET (chooser_embed), default_width, default_height);

  if (priv->preview_widget_active &&
      priv->preview_widget &&
      gtk_widget_get_visible (priv->preview_widget))
    {
      gtk_widget_get_preferred_size (priv->preview_box,
				     &req, NULL);
      *default_width += PREVIEW_HBOX_SPACING + req.width;
    }

  if (priv->extra_widget &&
      gtk_widget_get_visible (priv->extra_widget))
    {
      gtk_widget_get_preferred_size (priv->extra_align,
				     &req, NULL);
      *default_height += gtk_box_get_spacing (GTK_BOX (chooser_embed)) + req.height;
    }
}

struct switch_folder_closure {
  GtkFileChooserWidget *impl;
  GFile *file;
  int num_selected;
};

/* Used from gtk_tree_selection_selected_foreach() in switch_to_selected_folder() */
static void
switch_folder_foreach_cb (GtkTreeModel      *model,
			  GtkTreePath       *path,
			  GtkTreeIter       *iter,
			  gpointer           data)
{
  struct switch_folder_closure *closure;

  closure = data;

  closure->file = _gtk_file_system_model_get_file (GTK_FILE_SYSTEM_MODEL (model), iter);
  closure->num_selected++;
}

/* Changes to the selected folder in the list view */
static void
switch_to_selected_folder (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;
  struct switch_folder_closure closure;

  /* We do this with foreach() rather than get_selected() as we may be in
   * multiple selection mode
   */

  closure.impl = impl;
  closure.file = NULL;
  closure.num_selected = 0;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, switch_folder_foreach_cb, &closure);

  g_assert (closure.file && closure.num_selected == 1);

  change_folder_and_display_error (impl, closure.file, FALSE);
}

/* Gets the GFileInfo for the selected row in the file list; assumes single
 * selection mode.
 */
static GFileInfo *
get_selected_file_info_from_file_list (GtkFileChooserWidget *impl,
				       gboolean              *had_selection)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GFileInfo *info;

  g_assert (!priv->select_multiple);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      *had_selection = FALSE;
      return NULL;
    }

  *had_selection = TRUE;

  info = _gtk_file_system_model_get_info (priv->browse_files_model, &iter);
  return info;
}

/* Gets the display name of the selected file in the file list; assumes single
 * selection mode and that something is selected.
 */
static const gchar *
get_display_name_from_file_list (GtkFileChooserWidget *impl)
{
  GFileInfo *info;
  gboolean had_selection;

  info = get_selected_file_info_from_file_list (impl, &had_selection);
  g_assert (had_selection);
  g_assert (info != NULL);

  return g_file_info_get_display_name (info);
}

static void
add_custom_button_to_dialog (GtkDialog   *dialog,
			     const gchar *mnemonic_label,
			     gint         response_id)
{
  GtkWidget *button;

  button = gtk_button_new_with_mnemonic (mnemonic_label);
  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_show (button);

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
}

/* Presents an overwrite confirmation dialog; returns whether we should accept
 * the filename.
 */
static gboolean
confirm_dialog_should_accept_filename (GtkFileChooserWidget *impl,
				       const gchar           *file_part,
				       const gchar           *folder_display_name)
{
  GtkWindow *toplevel;
  GtkWidget *dialog;
  int response;

  toplevel = get_toplevel (GTK_WIDGET (impl));

  dialog = gtk_message_dialog_new (toplevel,
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_QUESTION,
				   GTK_BUTTONS_NONE,
				   _("A file named “%s” already exists.  Do you want to replace it?"),
				   file_part);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					    _("The file already exists in “%s”.  Replacing it will "
					      "overwrite its contents."),
					    folder_display_name);

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  add_custom_button_to_dialog (GTK_DIALOG (dialog), _("_Replace"), GTK_RESPONSE_ACCEPT);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  if (gtk_window_has_group (toplevel))
    gtk_window_group_add_window (gtk_window_get_group (toplevel),
                                 GTK_WINDOW (dialog));

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return (response == GTK_RESPONSE_ACCEPT);
}

struct GetDisplayNameData
{
  GtkFileChooserWidget *impl;
  gchar *file_part;
};

/* Every time we request a response explicitly, we need to save the selection to the recently-used list,
 * as requesting a response means, “the dialog is confirmed”.
 */
static void
request_response_and_add_to_recent_list (GtkFileChooserWidget *impl)
{
  g_signal_emit_by_name (impl, "response-requested");
  add_selection_to_recent_list (impl);
}

static void
confirmation_confirm_get_info_cb (GCancellable *cancellable,
				  GFileInfo    *info,
				  const GError *error,
				  gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  gboolean should_respond = FALSE;
  struct GetDisplayNameData *data = user_data;
  GtkFileChooserWidgetPrivate *priv = data->impl->priv;

  if (cancellable != priv->should_respond_get_info_cancellable)
    goto out;

  priv->should_respond_get_info_cancellable = NULL;

  if (cancelled)
    goto out;

  if (error)
    /* Huh?  Did the folder disappear?  Let the caller deal with it */
    should_respond = TRUE;
  else
    should_respond = confirm_dialog_should_accept_filename (data->impl, data->file_part, g_file_info_get_display_name (info));

  set_busy_cursor (data->impl, FALSE);
  if (should_respond)
    request_response_and_add_to_recent_list (data->impl);

out:
  g_object_unref (data->impl);
  g_free (data->file_part);
  g_free (data);

  g_object_unref (cancellable);
}

/* Does overwrite confirmation if appropriate, and returns whether the dialog
 * should respond.  Can get the file part from the file list or the save entry.
 */
static gboolean
should_respond_after_confirm_overwrite (GtkFileChooserWidget *impl,
					const gchar           *file_part,
					GFile                 *parent_file)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkFileChooserConfirmation conf;

  if (!priv->do_overwrite_confirmation)
    return TRUE;

  conf = GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;

  g_signal_emit_by_name (impl, "confirm-overwrite", &conf);

  switch (conf)
    {
    case GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM:
      {
	struct GetDisplayNameData *data;

	g_assert (file_part != NULL);

	data = g_new0 (struct GetDisplayNameData, 1);
	data->impl = g_object_ref (impl);
	data->file_part = g_strdup (file_part);

	if (priv->should_respond_get_info_cancellable)
	  g_cancellable_cancel (priv->should_respond_get_info_cancellable);

	priv->should_respond_get_info_cancellable =
	  _gtk_file_system_get_info (priv->file_system, parent_file,
				     "standard::display-name",
				     confirmation_confirm_get_info_cb,
				     data);
	set_busy_cursor (data->impl, TRUE);
	return FALSE;
      }

    case GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME:
      return TRUE;

    case GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN:
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static void
name_entry_get_parent_info_cb (GCancellable *cancellable,
			       GFileInfo    *info,
			       const GError *error,
			       gpointer      user_data)
{
  gboolean parent_is_folder;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  struct FileExistsData *data = user_data;
  GtkFileChooserWidget *impl = data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (cancellable != priv->should_respond_get_info_cancellable)
    goto out;

  priv->should_respond_get_info_cancellable = NULL;

  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  if (!info)
    parent_is_folder = FALSE;
  else
    parent_is_folder = _gtk_file_info_consider_as_directory (info);

  if (parent_is_folder)
    {
      if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
	  request_response_and_add_to_recent_list (impl); /* even if the file doesn't exist, apps can make good use of that (e.g. Emacs) */
	}
      else if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
        {
          if (data->file_exists_and_is_not_folder)
	    {
	      gboolean retval;
	      char *file_part;

              /* Dup the string because the string may be modified
               * depending on what clients do in the confirm-overwrite
               * signal and this corrupts the pointer
               */
              file_part = g_strdup (_gtk_file_chooser_entry_get_file_part (GTK_FILE_CHOOSER_ENTRY (priv->location_entry)));
	      retval = should_respond_after_confirm_overwrite (impl, file_part, data->parent_file);
              g_free (file_part);

	      if (retval)
		request_response_and_add_to_recent_list (impl);
	    }
	  else
	    request_response_and_add_to_recent_list (impl);
	}
      else if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
	       || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
        {
	  GError *mkdir_error = NULL;

	  /* In both cases (SELECT_FOLDER and CREATE_FOLDER), if you type
	   * "/blah/nonexistent" you *will* want a folder created.
	   */

	  set_busy_cursor (impl, TRUE);
	  g_file_make_directory (data->file, NULL, &mkdir_error);
	  set_busy_cursor (impl, FALSE);

	  if (!mkdir_error)
	    request_response_and_add_to_recent_list (impl);
	  else
	    error_creating_folder_dialog (impl, data->file, mkdir_error);
        }
      else
	g_assert_not_reached ();
    }
  else
    {
      if (info)
	{
	  /* The parent exists, but it's not a folder!  Someone probably typed existing_file.txt/subfile.txt */
	  error_with_file_under_nonfolder (impl, data->parent_file);
	}
      else
	{
	  GError *error_copy;

	  /* The parent folder is not readable for some reason */

	  error_copy = g_error_copy (error);
	  error_changing_folder_dialog (impl, data->parent_file, error_copy);
	}
    }

out:
  g_object_unref (data->impl);
  g_object_unref (data->file);
  g_object_unref (data->parent_file);
  g_free (data);

  g_object_unref (cancellable);
}

static void
file_exists_get_info_cb (GCancellable *cancellable,
			 GFileInfo    *info,
			 const GError *error,
			 gpointer      user_data)
{
  gboolean data_ownership_taken = FALSE;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  gboolean file_exists;
  gboolean is_folder;
  gboolean needs_parent_check = FALSE;
  struct FileExistsData *data = user_data;
  GtkFileChooserWidget *impl = data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (cancellable != priv->file_exists_get_info_cancellable)
    goto out;

  priv->file_exists_get_info_cancellable = NULL;

  set_busy_cursor (impl, FALSE);

  if (cancelled)
    goto out;

  file_exists = (info != NULL);
  is_folder = (file_exists && _gtk_file_info_consider_as_directory (info));

  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    {
      if (is_folder)
	change_folder_and_display_error (impl, data->file, TRUE);
      else
	{
	  if (file_exists)
	    request_response_and_add_to_recent_list (impl); /* user typed an existing filename; we are done */
	  else
	    needs_parent_check = TRUE; /* file doesn't exist; see if its parent exists */
	}
    }
  else if (priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      if (file_exists && !is_folder)
        {
          /* Oops, the user typed the name of an existing path which is not
           * a folder
           */
          error_creating_folder_over_existing_file_dialog (impl, data->file,
						           g_error_copy (error));
        }
      else
        {
          needs_parent_check = TRUE;
        }
    }
  else if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (!file_exists)
        {
	  needs_parent_check = TRUE;
        }
      else
	{
	  if (is_folder)
	    {
	      /* User typed a folder; we are done */
	      request_response_and_add_to_recent_list (impl);
	    }
	  else
	    error_selecting_folder_over_existing_file_dialog (impl);
	}
    }
  else if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      if (is_folder)
	change_folder_and_display_error (impl, data->file, TRUE);
      else
        if (!file_exists && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FILENAME_TOO_LONG))
          error_filename_to_long_dialog (data->impl);
        else
          needs_parent_check = TRUE;
    }
  else
    {
      g_assert_not_reached();
    }

  if (needs_parent_check) {
    /* check that everything up to the last path component exists (i.e. the parent) */

    data->file_exists_and_is_not_folder = file_exists && !is_folder;
    data_ownership_taken = TRUE;

    if (priv->should_respond_get_info_cancellable)
      g_cancellable_cancel (priv->should_respond_get_info_cancellable);

      priv->should_respond_get_info_cancellable =
	_gtk_file_system_get_info (priv->file_system,
				   data->parent_file,
				   "standard::type",
				   name_entry_get_parent_info_cb,
				   data);
      set_busy_cursor (impl, TRUE);
    }

out:
  if (!data_ownership_taken)
    {
      g_object_unref (impl);
      g_object_unref (data->file);
      g_object_unref (data->parent_file);
      g_free (data);
    }

  g_object_unref (cancellable);
}

static void
paste_text_received (GtkClipboard          *clipboard,
		     const gchar           *text,
		     GtkFileChooserWidget *impl)
{
  GFile *file;

  if (!text)
    return;

  file = g_file_new_for_uri (text);

  if (!gtk_file_chooser_widget_select_file (GTK_FILE_CHOOSER (impl), file, NULL))
    location_popup_handler (impl, text);

  g_object_unref (file);
}

/* Handler for the "location-popup-on-paste" keybinding signal */
static void
location_popup_on_paste_handler (GtkFileChooserWidget *impl)
{
  GtkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (impl),
		  				      GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text (clipboard,
		  	      (GtkClipboardTextReceivedFunc) paste_text_received,
			      impl);
}

/* Implementation for GtkFileChooserEmbed::should_respond() */
static void
add_selection_to_recent_list (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GSList *files;
  GSList *l;

  files = gtk_file_chooser_widget_get_files (GTK_FILE_CHOOSER (impl));

  for (l = files; l; l = l->next)
    {
      GFile *file = l->data;
      char *uri;

      uri = g_file_get_uri (file);
      if (uri)
	{
	  gtk_recent_manager_add_item (priv->recent_manager, uri);
	  g_free (uri);
	}
    }

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);
}

static gboolean
gtk_file_chooser_widget_should_respond (GtkFileChooserEmbed *chooser_embed)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser_embed);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *toplevel;
  GtkWidget *current_focus;
  gboolean retval;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));
  g_assert (GTK_IS_WINDOW (toplevel));

  retval = FALSE;

  current_focus = gtk_window_get_focus (GTK_WINDOW (toplevel));

  if (current_focus == priv->browse_files_tree_view)
    {
      /* The following array encodes what we do based on the priv->action and the
       * number of files selected.
       */
      typedef enum {
	NOOP,			/* Do nothing (don't respond) */
	RESPOND,		/* Respond immediately */
	RESPOND_OR_SWITCH,      /* Respond immediately if the selected item is a file; switch to it if it is a folder */
	ALL_FILES,		/* Respond only if everything selected is a file */
	ALL_FOLDERS,		/* Respond only if everything selected is a folder */
	SAVE_ENTRY,		/* Go to the code for handling the save entry */
	NOT_REACHED		/* Sanity check */
      } ActionToTake;
      static const ActionToTake what_to_do[4][3] = {
	/*				  0 selected		1 selected		many selected */
	/* ACTION_OPEN */		{ NOOP,			RESPOND_OR_SWITCH,	ALL_FILES   },
	/* ACTION_SAVE */		{ SAVE_ENTRY,		RESPOND_OR_SWITCH,	NOT_REACHED },
	/* ACTION_SELECT_FOLDER */	{ RESPOND,		ALL_FOLDERS,		ALL_FOLDERS },
	/* ACTION_CREATE_FOLDER */	{ SAVE_ENTRY,		ALL_FOLDERS,		NOT_REACHED }
      };

      int num_selected;
      gboolean all_files, all_folders;
      int k;
      ActionToTake action;

    file_list:

      g_assert (priv->action >= GTK_FILE_CHOOSER_ACTION_OPEN && priv->action <= GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

      if (priv->operation_mode == OPERATION_MODE_RECENT)
	{
	  if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    goto save_entry;
	  else
	    {
	      retval = recent_should_respond (impl);
	      goto out;
	    }
	}

      selection_check (impl, &num_selected, &all_files, &all_folders);

      if (num_selected > 2)
	k = 2;
      else
	k = num_selected;

      action = what_to_do [priv->action] [k];

      switch (action)
	{
	case NOOP:
	  return FALSE;

	case RESPOND:
	  retval = TRUE;
	  goto out;

	case RESPOND_OR_SWITCH:
	  g_assert (num_selected == 1);

	  if (all_folders)
	    {
	      switch_to_selected_folder (impl);
	      return FALSE;
	    }
	  else if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    {
	      retval = should_respond_after_confirm_overwrite (impl,
							       get_display_name_from_file_list (impl),
							       priv->current_folder);
	      goto out;
	    }
	  else
	    {
	      retval = TRUE;
	      goto out;
	    }

	case ALL_FILES:
	  retval = all_files;
	  goto out;

	case ALL_FOLDERS:
	  retval = all_folders;
	  goto out;

	case SAVE_ENTRY:
	  goto save_entry;

	default:
	  g_assert_not_reached ();
	}
    }
  else if ((priv->location_entry != NULL) && (current_focus == priv->location_entry))
    {
      GFile *file;
      gboolean is_well_formed, is_empty, is_file_part_empty;
      gboolean is_folder;
      GtkFileChooserEntry *entry;
      GError *error;

    save_entry:

      g_assert (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
		|| priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
		|| ((priv->action == GTK_FILE_CHOOSER_ACTION_OPEN
		     || priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		    && priv->location_mode == LOCATION_MODE_FILENAME_ENTRY));

      entry = GTK_FILE_CHOOSER_ENTRY (priv->location_entry);
      check_save_entry (impl, &file, &is_well_formed, &is_empty, &is_file_part_empty, &is_folder);

      if (!is_well_formed)
	{
	  if (!is_empty
	      && priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
	      && priv->operation_mode == OPERATION_MODE_RECENT)
	    {
	      /* FIXME: ERROR_NO_FOLDER */
#if 0
	      /* We'll #ifdef this out, as the fucking treeview selects its first row,
	       * thus changing our assumption that no selection is present - setting
	       * a selection causes the error message from path_bar_set_mode() to go away,
	       * but we want the user to see that message!
	       */
	      gtk_widget_grab_focus (priv->browse_files_tree_view);
#endif
	    }
	  /* FIXME: else show an "invalid filename" error as the pathbar mode? */

	  return FALSE;
	}

      if (is_empty)
        {
          if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
	      || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    {
	      /* FIXME: ERROR_NO_FILENAME */
	      gtk_widget_grab_focus (priv->location_entry);
	      return FALSE;
	    }

          goto file_list;
        }

      g_assert (file != NULL);

      error = NULL;
      if (is_folder)
	{
	  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
	      priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	    {
	      change_folder_and_display_error (impl, file, TRUE);
	    }
	  else if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
		   priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    {
	      /* The folder already exists, so we do not need to create it.
	       * Just respond to terminate the dialog.
	       */
	      retval = TRUE;
	    }
	  else
	    {
	      g_assert_not_reached ();
	    }
	}
      else
	{
	  struct FileExistsData *data;

	  /* We need to check whether file exists and whether it is a folder -
	   * the GtkFileChooserEntry *does* report is_folder==FALSE as a false
	   * negative (it doesn't know yet if your last path component is a
	   * folder).
	   */

	  data = g_new0 (struct FileExistsData, 1);
	  data->impl = g_object_ref (impl);
	  data->file = g_object_ref (file);
	  data->parent_file = _gtk_file_chooser_entry_get_current_folder (entry);

	  if (priv->file_exists_get_info_cancellable)
	    g_cancellable_cancel (priv->file_exists_get_info_cancellable);

	  priv->file_exists_get_info_cancellable =
	    _gtk_file_system_get_info (priv->file_system, file,
				       "standard::type",
				       file_exists_get_info_cb,
				       data);

	  set_busy_cursor (impl, TRUE);

	  if (error != NULL)
	    g_error_free (error);
	}

      g_object_unref (file);
    }
  else if (priv->toplevel_last_focus_widget == priv->browse_files_tree_view)
    {
      /* The focus is on a dialog's action area button, *and* the widget that
       * was focused immediately before it is the file list.  
       */
      goto file_list;
    }
  else if (priv->operation_mode == OPERATION_MODE_SEARCH && priv->toplevel_last_focus_widget == priv->search_entry)
    {
      search_entry_activate_cb (impl);
      return FALSE;
    }
  else if (priv->location_entry && priv->toplevel_last_focus_widget == priv->location_entry)
    {
      /* The focus is on a dialog's action area button, *and* the widget that
       * was focused immediately before it is the location entry.
       */
      goto save_entry;
    }
  else
    /* The focus is on a dialog's action area button or something else */
    if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
	|| priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
      goto save_entry;
    else
      goto file_list; 

 out:

  if (retval)
    add_selection_to_recent_list (impl);

  return retval;
}

/* Implementation for GtkFileChooserEmbed::initial_focus() */
static void
gtk_file_chooser_widget_initial_focus (GtkFileChooserEmbed *chooser_embed)
{
  GtkFileChooserWidget *impl = GTK_FILE_CHOOSER_WIDGET (chooser_embed);
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkWidget *widget;

  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (priv->location_mode == LOCATION_MODE_PATH_BAR
	  || priv->operation_mode == OPERATION_MODE_RECENT)
	widget = priv->browse_files_tree_view;
      else
	widget = priv->location_entry;
    }
  else if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
	   priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    widget = priv->location_entry;
  else
    {
      g_assert_not_reached ();
      widget = NULL;
    }

  g_assert (widget != NULL);
  gtk_widget_grab_focus (widget);
}

/* Callback used from gtk_tree_selection_selected_foreach(); gets the selected GFiles */
static void
search_selected_foreach_get_file_cb (GtkTreeModel *model,
				     GtkTreePath  *path,
				     GtkTreeIter  *iter,
				     gpointer      data)
{
  GSList **list;
  GFile *file;

  list = data;

  gtk_tree_model_get (model, iter, MODEL_COL_FILE, &file, -1);
  *list = g_slist_prepend (*list, file); /* The file already has a new ref courtesy of gtk_tree_model_get(); this will be unreffed by the caller */
}

/* Constructs a list of the selected paths in search mode */
static GSList *
search_get_selected_files (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GSList *result;
  GtkTreeSelection *selection;

  result = NULL;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, search_selected_foreach_get_file_cb, &result);
  result = g_slist_reverse (result);

  return result;
}

/* Adds one hit from the search engine to the search_model */
static void
search_add_hit (GtkFileChooserWidget *impl,
		gchar                 *uri)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *file;

  file = g_file_new_for_uri (uri);
  if (!file)
    return;

  priv->search_model_empty = FALSE;

  _gtk_file_system_model_add_and_query_file (priv->search_model,
                                             file,
                                             MODEL_ATTRIBUTES);

  g_object_unref (file);
}

/* Callback used from GtkSearchEngine when we get new hits */
static void
search_engine_hits_added_cb (GtkSearchEngine *engine,
			     GList           *hits,
			     gpointer         data)
{
  GtkFileChooserWidget *impl;
  GList *l;

  impl = GTK_FILE_CHOOSER_WIDGET (data);

  for (l = hits; l; l = l->next)
    search_add_hit (impl, (gchar*)l->data);
}

/* Callback used from GtkSearchEngine when the query is done running */
static void
search_engine_finished_cb (GtkSearchEngine *engine,
			   gpointer         data)
{
  GtkFileChooserWidget *impl;

  impl = GTK_FILE_CHOOSER_WIDGET (data);

  set_busy_cursor (impl, FALSE);

  if (impl->priv->search_model_empty)
    gtk_stack_set_visible_child_name (GTK_STACK (impl->priv->browse_files_stack), "empty");
}

/* Displays a generic error when we cannot create a GtkSearchEngine.  
 * It would be better if _gtk_search_engine_new() gave us a GError 
 * with a better message, but it doesn’t do that right now.
 */
static void
search_error_could_not_create_client (GtkFileChooserWidget *impl)
{
  error_message (impl,
		 _("Could not start the search process"),
		 _("The program was not able to create a connection to the indexer "
		   "daemon. Please make sure it is running."));
}

static void
search_engine_error_cb (GtkSearchEngine *engine,
			const gchar     *message,
			gpointer         data)
{
  GtkFileChooserWidget *impl;
  
  impl = GTK_FILE_CHOOSER_WIDGET (data);

  search_stop_searching (impl, TRUE);
  error_message (impl, _("Could not send the search request"), message);

  set_busy_cursor (impl, FALSE);
}

/* Frees the data in the search_model */
static void
search_clear_model (GtkFileChooserWidget *impl, 
		    gboolean               remove_from_treeview)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (!priv->search_model)
    return;

  g_object_unref (priv->search_model);
  priv->search_model = NULL;
  
  if (remove_from_treeview)
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->browse_files_tree_view), NULL);
}

/* Stops any ongoing searches; does not touch the search_model */
static void
search_stop_searching (GtkFileChooserWidget *impl,
                       gboolean               remove_query)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (remove_query && priv->search_query)
    {
      g_object_unref (priv->search_query);
      priv->search_query = NULL;
    }
  
  if (priv->search_engine)
    {
      _gtk_search_engine_stop (priv->search_engine);
      g_signal_handlers_disconnect_by_data (priv->search_engine, impl);
      g_object_unref (priv->search_engine);
      priv->search_engine = NULL;
    }
}

/* Creates the search_model and puts it in the tree view */
static void
search_setup_model (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_assert (priv->search_model == NULL);

  priv->search_model = _gtk_file_system_model_new (file_system_model_set,
                                                   impl,
						   MODEL_COLUMN_TYPES);
  priv->search_model_empty = TRUE;

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->search_model),
				   MODEL_COL_NAME,
				   name_sort_func,
				   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->search_model),
				   MODEL_COL_MTIME,
				   mtime_sort_func,
				   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->search_model),
				   MODEL_COL_SIZE,
				   size_sort_func,
				   impl, NULL);
  set_sort_column (impl);

  /* EB: setting the model here will make the hits list update feel
   * more "alive" than setting the model at the end of the search
   * run
   */
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->browse_files_tree_view),
                           GTK_TREE_MODEL (priv->search_model));
  file_list_set_sort_column_ids (impl);
}

/* Creates a new query with the specified text and launches it */
static void
search_start_query (GtkFileChooserWidget *impl,
		    const gchar          *query_text)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  search_stop_searching (impl, FALSE);
  search_clear_model (impl, TRUE);
  search_setup_model (impl);
  set_busy_cursor (impl, TRUE);

  gtk_stack_set_visible_child_name (GTK_STACK (priv->browse_files_stack), "list");

  if (priv->search_engine == NULL)
    priv->search_engine = _gtk_search_engine_new ();

  if (!priv->search_engine)
    {
      set_busy_cursor (impl, FALSE);
      search_error_could_not_create_client (impl); /* lame; we don't get an error code or anything */
      return;
    }

  if (!priv->search_query)
    {
      priv->search_query = gtk_query_new ();
      gtk_query_set_text (priv->search_query, query_text);
    }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->current_location_radio)) &&
      priv->current_folder)
    {
      gchar *location;
      location = g_file_get_uri (priv->current_folder);
      gtk_query_set_location (priv->search_query, location);
      g_free (location);
    }

  _gtk_search_engine_set_query (priv->search_engine, priv->search_query);

  g_signal_connect (priv->search_engine, "hits-added",
		    G_CALLBACK (search_engine_hits_added_cb), impl);
  g_signal_connect (priv->search_engine, "finished",
		    G_CALLBACK (search_engine_finished_cb), impl);
  g_signal_connect (priv->search_engine, "error",
		    G_CALLBACK (search_engine_error_cb), impl);

  _gtk_search_engine_start (priv->search_engine);
}

/* Callback used when the user presses Enter while typing on the search
 * entry; starts the query
 */
static void
search_entry_activate_cb (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  const char *text;

  if (priv->operation_mode != OPERATION_MODE_SEARCH)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));

  /* reset any existing query object */
  if (priv->search_query)
    {
      g_object_unref (priv->search_query);
      priv->search_query = NULL;
    }

  if (strlen (text) == 0)
    return;

  search_start_query (impl, text);
}

static void
search_entry_stop_cb (GtkFileChooserWidget *impl)
{
  g_object_set (impl, "search-mode", FALSE, NULL);
}

/* Hides the path bar and creates the search entry */
static void
search_setup_widgets (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  /* if there already is a query, restart it */
  if (priv->search_query)
    {
      gchar *query = gtk_query_get_text (priv->search_query);

      if (query)
        {
          gtk_entry_set_text (GTK_ENTRY (priv->search_entry), query);
          search_start_query (impl, query);

          g_free (query);
        }
      else
        {
          g_object_unref (priv->search_query);
          priv->search_query = NULL;
        }
    }

  /* FMQ: hide the filter combo? */
}

/*
 * Recent files support
 */

/* Frees the data in the recent_model */
static void
recent_clear_model (GtkFileChooserWidget *impl,
                    gboolean               remove_from_treeview)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (!priv->recent_model)
    return;

  if (remove_from_treeview)
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->browse_files_tree_view), NULL);

  g_object_unref (priv->recent_model);
  priv->recent_model = NULL;
}

/* Stops any ongoing loading of the recent files list; does
 * not touch the recent_model
 */
static void
recent_stop_loading (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->load_recent_id)
    {
      g_source_remove (priv->load_recent_id);
      priv->load_recent_id = 0;
    }
}

static void
recent_setup_model (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_assert (priv->recent_model == NULL);

  priv->recent_model = _gtk_file_system_model_new (file_system_model_set,
                                                   impl,
						   MODEL_COLUMN_TYPES);

  _gtk_file_system_model_set_filter (priv->recent_model,
                                     priv->current_filter);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->recent_model),
				   MODEL_COL_NAME,
				   name_sort_func,
				   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->recent_model),
                                   MODEL_COL_SIZE,
                                   size_sort_func,
                                   impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->recent_model),
                                   MODEL_COL_MTIME,
                                   mtime_sort_func,
                                   impl, NULL);
  set_sort_column (impl);
}

typedef struct
{
  GtkFileChooserWidget *impl;
  GList *items;
} RecentLoadData;

static void
recent_idle_cleanup (gpointer data)
{
  RecentLoadData *load_data = data;
  GtkFileChooserWidget *impl = load_data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->browse_files_tree_view),
                           GTK_TREE_MODEL (priv->recent_model));
  file_list_set_sort_column_ids (impl);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->recent_model), MODEL_COL_MTIME, GTK_SORT_DESCENDING);

  set_busy_cursor (impl, FALSE);
  
  priv->load_recent_id = 0;
  
  g_free (load_data);
}

/* Populates the file system model with the GtkRecentInfo* items in the provided list; frees the items */
static void
populate_model_with_recent_items (GtkFileChooserWidget *impl, GList *items)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gint limit;
  GList *l;
  int n;

  limit = DEFAULT_RECENT_FILES_LIMIT;

  n = 0;

  for (l = items; l; l = l->next)
    {
      GtkRecentInfo *info = l->data;
      GFile *file;

      file = g_file_new_for_uri (gtk_recent_info_get_uri (info));
      _gtk_file_system_model_add_and_query_file (priv->recent_model,
                                                 file,
                                                 MODEL_ATTRIBUTES);
      g_object_unref (file);

      n++;
      if (limit != -1 && n >= limit)
	break;
    }
}

static void
populate_model_with_folders (GtkFileChooserWidget *impl, GList *items)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GList *folders;
  GList *l;

  folders = _gtk_file_chooser_extract_recent_folders (items);

  for (l = folders; l; l = l->next)
    {
      GFile *folder = l->data;

      _gtk_file_system_model_add_and_query_file (priv->recent_model,
                                                 folder,
                                                 MODEL_ATTRIBUTES);
    }

  g_list_foreach (folders, (GFunc) g_object_unref, NULL);
  g_list_free (folders);
}

static gboolean
recent_idle_load (gpointer data)
{
  RecentLoadData *load_data = data;
  GtkFileChooserWidget *impl = load_data->impl;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (!priv->recent_manager)
    return FALSE;

  load_data->items = gtk_recent_manager_get_items (priv->recent_manager);
  if (!load_data->items)
    return FALSE;

  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    populate_model_with_recent_items (impl, load_data->items);
  else
    populate_model_with_folders (impl, load_data->items);

  g_list_foreach (load_data->items, (GFunc) gtk_recent_info_unref, NULL);
  g_list_free (load_data->items);
  load_data->items = NULL;

  return FALSE;
}

static void
recent_start_loading (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  RecentLoadData *load_data;

  recent_stop_loading (impl);
  recent_clear_model (impl, TRUE);
  recent_setup_model (impl);
  set_busy_cursor (impl, TRUE);

  g_assert (priv->load_recent_id == 0);

  load_data = g_new (RecentLoadData, 1);
  load_data->impl = impl;
  load_data->items = NULL;

  /* begin lazy loading the recent files into the model */
  priv->load_recent_id = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT,
                                                    recent_idle_load,
                                                    load_data,
                                                    recent_idle_cleanup);
  g_source_set_name_by_id (priv->load_recent_id, "[gtk+] recent_idle_load");
}

static void
recent_selected_foreach_get_file_cb (GtkTreeModel *model,
				     GtkTreePath  *path,
				     GtkTreeIter  *iter,
				     gpointer      data)
{
  GSList **list;
  GFile *file;

  list = data;

  gtk_tree_model_get (model, iter, MODEL_COL_FILE, &file, -1);
  *list = g_slist_prepend (*list, file);
}

/* Constructs a list of the selected paths in recent files mode */
static GSList *
recent_get_selected_files (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GSList *result;
  GtkTreeSelection *selection;

  result = NULL;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, recent_selected_foreach_get_file_cb, &result);
  result = g_slist_reverse (result);

  return result;
}

/* Called from ::should_respond().  We return whether there are selected
 * files in the recent files list.
 */
static gboolean
recent_should_respond (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeSelection *selection;

  g_assert (priv->operation_mode == OPERATION_MODE_RECENT);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  return (gtk_tree_selection_count_selected_rows (selection) != 0);
}

static void
set_current_filter (GtkFileChooserWidget *impl,
		    GtkFileFilter         *filter)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->current_filter != filter)
    {
      int filter_index;

      /* NULL filters are allowed to reset to non-filtered status
       */
      filter_index = g_slist_index (priv->filters, filter);
      if (priv->filters && filter && filter_index < 0)
	return;

      if (priv->current_filter)
	g_object_unref (priv->current_filter);
      priv->current_filter = filter;
      if (priv->current_filter)
	{
	  g_object_ref_sink (priv->current_filter);
	}

      if (priv->filters)
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->filter_combo),
				  filter_index);

      if (priv->browse_files_model)
        {
          _gtk_file_system_model_set_filter (priv->browse_files_model, priv->current_filter);
          _gtk_file_system_model_clear_cache (priv->browse_files_model, MODEL_COL_IS_SENSITIVE);
        }

      if (priv->search_model)
        {
          _gtk_file_system_model_set_filter (priv->search_model, filter);
          _gtk_file_system_model_clear_cache (priv->search_model, MODEL_COL_IS_SENSITIVE);
        }

      if (priv->recent_model)
        {
          _gtk_file_system_model_set_filter (priv->recent_model, filter);
          _gtk_file_system_model_clear_cache (priv->recent_model, MODEL_COL_IS_SENSITIVE);
        }

      g_object_notify (G_OBJECT (impl), "filter");
    }
}

static void
filter_combo_changed (GtkComboBox           *combo_box,
		      GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  gint new_index = gtk_combo_box_get_active (combo_box);
  GtkFileFilter *new_filter = g_slist_nth_data (priv->filters, new_index);

  set_current_filter (impl, new_filter);
}

static void
check_preview_change (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreePath *path;
  GFile *new_file;
  char *new_display_name;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->browse_files_tree_view));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->browse_files_tree_view));
  if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE ||
      gtk_tree_selection_get_mode (selection) == GTK_SELECTION_BROWSE)
    {
      GtkTreeIter iter;

      if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        path = gtk_tree_model_get_path (model, &iter);
      else
        path = NULL;
    }
  else
    {
      gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->browse_files_tree_view), &path, NULL);
      if (path && !gtk_tree_selection_path_is_selected (selection, path))
        {
          gtk_tree_path_free (path);
          path = NULL;
        }
    }

  if (path)
    {
      GtkTreeIter iter;

      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          MODEL_COL_FILE, &new_file,
                          MODEL_COL_NAME, &new_display_name,
                          -1);
      
      gtk_tree_path_free (path);
    }
  else
    {
      new_file = NULL;
      new_display_name = NULL;
    }

  if (new_file != priv->preview_file &&
      !(new_file && priv->preview_file &&
	g_file_equal (new_file, priv->preview_file)))
    {
      if (priv->preview_file)
	{
	  g_object_unref (priv->preview_file);
	  g_free (priv->preview_display_name);
	}

      if (new_file)
	{
	  priv->preview_file = new_file;
	  priv->preview_display_name = new_display_name;
	}
      else
	{
	  priv->preview_file = NULL;
	  priv->preview_display_name = NULL;
          g_free (new_display_name);
	}

      if (priv->use_preview_label && priv->preview_label)
	gtk_label_set_text (GTK_LABEL (priv->preview_label), priv->preview_display_name);

      g_signal_emit_by_name (impl, "update-preview");
    }
  else
    {
      if (new_file)
        g_object_unref (new_file);

      g_free (new_display_name);
    }
}

static gboolean
list_select_func  (GtkTreeSelection  *selection,
		   GtkTreeModel      *model,
		   GtkTreePath       *path,
		   gboolean           path_currently_selected,
		   gpointer           data)
{
  GtkFileChooserWidget *impl = data;
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      GtkTreeIter iter;
      gboolean is_sensitive;
      gboolean is_folder;

      if (!gtk_tree_model_get_iter (model, &iter, path))
        return FALSE;
      gtk_tree_model_get (model, &iter,
                          MODEL_COL_IS_SENSITIVE, &is_sensitive,
                          MODEL_COL_IS_FOLDER, &is_folder,
                          -1);
      if (!is_sensitive || !is_folder)
        return FALSE;
    }
    
  return TRUE;
}

static void
list_selection_changed (GtkTreeSelection      *selection,
			GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  /* See if we are in the new folder editable row for Save mode */
  if (priv->operation_mode == OPERATION_MODE_BROWSE &&
      priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      GFileInfo *info;
      gboolean had_selection;

      info = get_selected_file_info_from_file_list (impl, &had_selection);
      if (!had_selection)
	goto out; /* normal processing */

      if (!info)
	return; /* We are on the editable row for New Folder */
    }

 out:

  if (priv->location_entry)
    update_chooser_entry (impl);

  location_bar_update (impl);

  check_preview_change (impl);
  check_file_list_menu_sensitivity (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);
}

static void
list_cursor_changed (GtkTreeView          *list,
                     GtkFileChooserWidget *impl)
{
  check_preview_change (impl);
}

/* Callback used when a row in the file list is activated */
static void
list_row_activated (GtkTreeView           *tree_view,
		    GtkTreePath           *path,
		    GtkTreeViewColumn     *column,
		    GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *file;
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean is_folder;
  gboolean is_sensitive;

  model = gtk_tree_view_get_model (tree_view);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter,
                      MODEL_COL_FILE, &file,
                      MODEL_COL_IS_FOLDER, &is_folder,
                      MODEL_COL_IS_SENSITIVE, &is_sensitive,
                      -1);
        
  if (is_sensitive && is_folder && file)
    {
      change_folder_and_display_error (impl, file, FALSE);
      goto out;
    }

  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      priv->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    g_signal_emit_by_name (impl, "file-activated");

 out:

  if (file)
    g_object_unref (file);
}

static void
path_bar_clicked (GtkPathBar            *path_bar,
		  GFile                 *file,
		  GFile                 *child_file,
		  gboolean               child_is_hidden,
		  GtkFileChooserWidget *impl)
{
  if (child_file)
    pending_select_files_add (impl, child_file);

  if (!change_folder_and_display_error (impl, file, FALSE))
    return;

  /* Say we have "/foo/bar/[.baz]" and the user clicks on "bar".  We should then
   * show hidden files so that ".baz" appears in the file list, as it will still
   * be shown in the path bar: "/foo/[bar]/.baz"
   */
  if (child_is_hidden)
    g_object_set (impl, "show-hidden", TRUE, NULL);
}

static void
update_cell_renderer_attributes (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GList *walk, *list;

  /* Keep the following column numbers in sync with create_file_list() */

  /* name */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->browse_files_tree_view), 0);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  for (walk = list; walk; walk = walk->next)
    {
      renderer = walk->data;
      if (GTK_IS_CELL_RENDERER_PIXBUF (renderer))
        {
          gtk_tree_view_column_set_attributes (column, renderer, 
                                               "surface", MODEL_COL_SURFACE,
                                               NULL);
        }
      else
        {
          gtk_tree_view_column_set_attributes (column, renderer, 
                                               "text", MODEL_COL_NAME,
                                               "ellipsize", MODEL_COL_ELLIPSIZE,
                                               NULL);
        }

      gtk_tree_view_column_add_attribute (column, renderer, "sensitive", MODEL_COL_IS_SENSITIVE);
    }
  g_list_free (list);

  /* size */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->browse_files_tree_view), 1);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  renderer = list->data;
  gtk_tree_view_column_set_attributes (column, renderer, 
                                       "text", MODEL_COL_SIZE_TEXT,
                                       NULL);

  gtk_tree_view_column_add_attribute (column, renderer, "sensitive", MODEL_COL_IS_SENSITIVE);
  g_list_free (list);

  /* mtime */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->browse_files_tree_view), 2);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  renderer = list->data;
  gtk_tree_view_column_set_attributes (column, renderer, 
                                       "text", MODEL_COL_MTIME_TEXT,
                                       NULL);
  gtk_tree_view_column_add_attribute (column, renderer, "sensitive", MODEL_COL_IS_SENSITIVE);
  g_list_free (list);

  /* location */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->browse_files_tree_view), 3);
  list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  renderer = list->data;
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_START, NULL);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", MODEL_COL_LOCATION_TEXT,
                                       "sensitive", MODEL_COL_IS_SENSITIVE,
                                       NULL);
  g_list_free (list);

}

static void
location_set_user_text (GtkFileChooserWidget *impl,
			const gchar           *path)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->location_entry), path);
  gtk_editable_set_position (GTK_EDITABLE (priv->location_entry), -1);
}

static void
location_popup_handler (GtkFileChooserWidget *impl,
			const gchar           *path)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  if (priv->operation_mode != OPERATION_MODE_BROWSE)
    {
      operation_mode_set (impl, OPERATION_MODE_BROWSE);
      if (priv->current_folder)
        change_folder_and_display_error (impl, priv->current_folder, FALSE);
      else
        switch_to_home_dir (impl);
    }

  if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      if (!path)
	return;

      location_mode_set (impl, LOCATION_MODE_FILENAME_ENTRY);
      location_set_user_text (impl, path);
    }
  else if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
	   priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      gtk_widget_grab_focus (priv->location_entry);
      if (path != NULL)
	location_set_user_text (impl, path);
    }
  else
    g_assert_not_reached ();
}

/* Handler for the "up-folder" keybinding signal */
static void
up_folder_handler (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  _gtk_path_bar_up (GTK_PATH_BAR (priv->browse_path_bar));
}

/* Handler for the "down-folder" keybinding signal */
static void
down_folder_handler (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  _gtk_path_bar_down (GTK_PATH_BAR (priv->browse_path_bar));
}

/* Handler for the "home-folder" keybinding signal */
static void
home_folder_handler (GtkFileChooserWidget *impl)
{
  switch_to_home_dir (impl);
}

/* Handler for the "desktop-folder" keybinding signal */
static void
desktop_folder_handler (GtkFileChooserWidget *impl)
{
  const char *name;

  name = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  /* "To disable a directory, point it to the homedir."
   * See http://freedesktop.org/wiki/Software/xdg-user-dirs
   **/
  if (!g_strcmp0 (name, g_get_home_dir ())) {
    return;
  }

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (impl), name);
}

/* Handler for the "search-shortcut" keybinding signal */
static void
search_shortcut_handler (GtkFileChooserWidget *impl)
{
  operation_mode_set (impl, OPERATION_MODE_SEARCH);
}

/* Handler for the "recent-shortcut" keybinding signal */
static void
recent_shortcut_handler (GtkFileChooserWidget *impl)
{
  operation_mode_set (impl, OPERATION_MODE_RECENT);
}

static void
quick_bookmark_handler (GtkFileChooserWidget *impl,
			gint bookmark_index)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;
  GFile *file;

  file = gtk_places_sidebar_get_nth_bookmark (GTK_PLACES_SIDEBAR (priv->places_sidebar), bookmark_index);

  if (file)
    {
      change_folder_and_display_error (impl, file, FALSE);
      g_object_unref (file);
    }
}

static void
show_hidden_handler (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv = impl->priv;

  g_object_set (impl,
		"show-hidden", !priv->show_hidden,
		NULL);
}

static void
add_normal_and_shifted_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				GdkModifierType modifiers,
				const gchar    *signal_name)
{
  gtk_binding_entry_add_signal (binding_set,
				keyval, modifiers,
				signal_name, 0);

  gtk_binding_entry_add_signal (binding_set,
				keyval, modifiers | GDK_SHIFT_MASK,
				signal_name, 0);
}

static void
gtk_file_chooser_widget_class_init (GtkFileChooserWidgetClass *class)
{
  static const guint quick_bookmark_keyvals[10] = {
    GDK_KEY_1, GDK_KEY_2, GDK_KEY_3, GDK_KEY_4, GDK_KEY_5, GDK_KEY_6, GDK_KEY_7, GDK_KEY_8, GDK_KEY_9, GDK_KEY_0
  };
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;
  int i;

  gobject_class->finalize = gtk_file_chooser_widget_finalize;
  gobject_class->constructed = gtk_file_chooser_widget_constructed;
  gobject_class->set_property = gtk_file_chooser_widget_set_property;
  gobject_class->get_property = gtk_file_chooser_widget_get_property;
  gobject_class->dispose = gtk_file_chooser_widget_dispose;

  widget_class->show_all = gtk_file_chooser_widget_show_all;
  widget_class->realize = gtk_file_chooser_widget_realize;
  widget_class->map = gtk_file_chooser_widget_map;
  widget_class->unmap = gtk_file_chooser_widget_unmap;
  widget_class->hierarchy_changed = gtk_file_chooser_widget_hierarchy_changed;
  widget_class->style_updated = gtk_file_chooser_widget_style_updated;
  widget_class->screen_changed = gtk_file_chooser_widget_screen_changed;

  /*
   * Signals
   */

  /**
   * GtkFileChooserWidget::location-popup:
   * @widget: the object which received the signal.
   * @path: a string that gets put in the text entry for the file
   * name.
   *
   * The ::location-popup signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser show a "Location"
   * prompt which the user can use to manually type the name of
   * the file he wishes to select.
   *
   * The default bindings for this signal are
   * `Control + L`
   * with a @path string of "" (the empty
   * string).  It is also bound to `/` with a
   * @path string of "`/`"
   * (a slash):  this lets you type `/` and
   * immediately type a path name.  On Unix systems, this is bound to
   * `~` (tilde) with a @path string
   * of "~" itself for access to home directories.
   */
  signals[LOCATION_POPUP] =
    g_signal_new_class_handler (I_("location-popup"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (location_popup_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__STRING,
                                G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * GtkFileChooserWidget::location-popup-on-paste:
   * @widget: the object which received the signal.
   *
   * The ::location-popup-on-paste signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser show a "Location"
   * prompt when the user pastes into a #GtkFileChooserWidget.
   *
   * The default binding for this signal is `Control + V`.
   */
  signals[LOCATION_POPUP_ON_PASTE] =
    g_signal_new_class_handler (I_("location-popup-on-paste"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (location_popup_on_paste_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::location-toggle-popup:
   * @widget: the object which received the signal.
   *
   * The ::location-toggle-popup signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to toggle the visibility of a "Location"
   * prompt which the user can use to manually type the name of
   * the file he wishes to select.
   *
   * The default binding for this signal is `Control + L`.
   */
  signals[LOCATION_TOGGLE_POPUP] =
    g_signal_new_class_handler (I_("location-toggle-popup"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (location_toggle_popup_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::up-folder:
   * @widget: the object which received the signal.
   *
   * The ::up-folder signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser go to the parent of
   * the current folder in the file hierarchy.
   *
   * The default binding for this signal is `Alt + Up`.
   */
  signals[UP_FOLDER] =
    g_signal_new_class_handler (I_("up-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (up_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::down-folder:
   * @widget: the object which received the signal.
   *
   * The ::down-folder signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser go to a child of the
   * current folder in the file hierarchy.  The subfolder that
   * will be used is displayed in the path bar widget of the file
   * chooser.  For example, if the path bar is showing
   * "/foo/bar/baz", with bar currently displayed, then this will cause
   * the file chooser to switch to the "baz" subfolder.
   *
   * The default binding for this signal is `Alt + Down`.
   */
  signals[DOWN_FOLDER] =
    g_signal_new_class_handler (I_("down-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (down_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::home-folder:
   * @widget: the object which received the signal.
   *
   * The ::home-folder signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser show the user's home
   * folder in the file list.
   *
   * The default binding for this signal is `Alt + Home`.
   */
  signals[HOME_FOLDER] =
    g_signal_new_class_handler (I_("home-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (home_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::desktop-folder:
   * @widget: the object which received the signal.
   *
   * The ::desktop-folder signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser show the user's Desktop
   * folder in the file list.
   *
   * The default binding for this signal is `Alt + D`.
   */
  signals[DESKTOP_FOLDER] =
    g_signal_new_class_handler (I_("desktop-folder"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (desktop_folder_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::quick-bookmark:
   * @widget: the object which received the signal.
   * @bookmark_index: the number of the bookmark to switch to
   *
   * The ::quick-bookmark signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser switch to the bookmark
   * specified in the @bookmark_index parameter.
   * For example, if you have three bookmarks, you can pass 0, 1, 2 to
   * this signal to switch to each of them, respectively.
   *
   * The default binding for this signal is `Alt + 1`, `Alt + 2`,
   * etc. until `Alt + 0`.  Note that in the default binding, that
   * `Alt + 1` is actually defined to switch to the bookmark at index
   * 0, and so on successively; `Alt + 0` is defined to switch to the
   * bookmark at index 10.
   */
  signals[QUICK_BOOKMARK] =
    g_signal_new_class_handler (I_("quick-bookmark"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (quick_bookmark_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__INT,
                                G_TYPE_NONE, 1, G_TYPE_INT);

  /**
   * GtkFileChooserWidget::show-hidden:
   * @widget: the object which received the signal.
   *
   * The ::show-hidden signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser display hidden files.
   *
   * The default binding for this signal is `Control + H`.
   */
  signals[SHOW_HIDDEN] =
    g_signal_new_class_handler (I_("show-hidden"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (show_hidden_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::search-shortcut:
   * @widget: the object which received the signal.
   *
   * The ::search-shortcut signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser show the search entry.
   *
   * The default binding for this signal is `Alt + S`.
   */
  signals[SEARCH_SHORTCUT] =
    g_signal_new_class_handler (I_("search-shortcut"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (search_shortcut_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkFileChooserWidget::recent-shortcut:
   * @widget: the object which received the signal.
   *
   * The ::recent-shortcut signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user asks for it.
   *
   * This is used to make the file chooser show the Recent location.
   *
   * The default binding for this signal is `Alt + R`.
   */
  signals[RECENT_SHORTCUT] =
    g_signal_new_class_handler (I_("recent-shortcut"),
                                G_OBJECT_CLASS_TYPE (class),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (recent_shortcut_handler),
                                NULL, NULL,
                                _gtk_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (class);

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_l, GDK_CONTROL_MASK,
				"location-toggle-popup",
				0);

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_slash, 0,
				"location-popup",
				1, G_TYPE_STRING, "/");
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Divide, 0,
				"location-popup",
				1, G_TYPE_STRING, "/");

#ifdef G_OS_UNIX
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_asciitilde, 0,
				"location-popup",
				1, G_TYPE_STRING, "~");
#endif

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_v, GDK_CONTROL_MASK,
				"location-popup-on-paste",
				0);

  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_Up, GDK_MOD1_MASK,
				  "up-folder");

  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_KP_Up, GDK_MOD1_MASK,
				  "up-folder");

  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_Down, GDK_MOD1_MASK,
				  "down-folder");
  add_normal_and_shifted_binding (binding_set,
				  GDK_KEY_KP_Down, GDK_MOD1_MASK,
				  "down-folder");

  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_Home, GDK_MOD1_MASK,
				"home-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_KP_Home, GDK_MOD1_MASK,
				"home-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_d, GDK_MOD1_MASK,
				"desktop-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KEY_h, GDK_CONTROL_MASK,
                                "show-hidden",
                                0);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_s, GDK_MOD1_MASK,
                                "search-shortcut",
                                0);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_r, GDK_MOD1_MASK,
                                "recent-shortcut",
                                0);

  for (i = 0; i < 10; i++)
    gtk_binding_entry_add_signal (binding_set,
				  quick_bookmark_keyvals[i], GDK_MOD1_MASK,
				  "quick-bookmark",
				  1, G_TYPE_INT, i);

  g_object_class_install_property (gobject_class, PROP_SEARCH_MODE,
                                   g_param_spec_boolean ("search-mode",
                                                         P_("Search mode"),
                                                         P_("Search mode"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  _gtk_file_chooser_install_properties (gobject_class);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkfilechooserwidget.ui");

  /* A *lot* of widgets that we need to handle .... */
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_widgets_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_widgets_hpaned);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_files_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_files_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_widgets_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, places_sidebar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_files_tree_view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_header_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_header_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_new_folder_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_path_bar_hbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_path_bar_size_group);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, browse_path_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, filter_combo_hbox);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, filter_combo);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, preview_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, extra_align);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, extra_and_filters);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, location_entry_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, current_location_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, list_name_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, list_pixbuf_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, list_name_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, list_mtime_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, list_size_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, list_location_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, new_folder_name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, new_folder_create_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, new_folder_error_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkFileChooserWidget, new_folder_popover);

  /* And a *lot* of callbacks to bind ... */
  gtk_widget_class_bind_template_callback (widget_class, browse_files_key_press_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, file_list_drag_drop_cb);
  gtk_widget_class_bind_template_callback (widget_class, file_list_drag_data_received_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_popup_menu_cb);
  gtk_widget_class_bind_template_callback (widget_class, file_list_query_tooltip_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_button_press_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, file_list_drag_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, list_cursor_changed);
  gtk_widget_class_bind_template_callback (widget_class, filter_combo_changed);
  gtk_widget_class_bind_template_callback (widget_class, path_bar_clicked);
  gtk_widget_class_bind_template_callback (widget_class, places_sidebar_open_location_cb);
  gtk_widget_class_bind_template_callback (widget_class, places_sidebar_show_error_message_cb);
  gtk_widget_class_bind_template_callback (widget_class, places_sidebar_show_enter_location_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_stop_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_folder_popover_active);
  gtk_widget_class_bind_template_callback (widget_class, new_folder_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, new_folder_create_clicked);
}

static void
post_process_ui (GtkFileChooserWidget *impl)
{
  GtkTreeSelection *selection;
  GtkCellRenderer  *cell;
  GList            *cells;
  GFile            *file;

  /* Some qdata, qdata can't be set with GtkBuilder */
  g_object_set_data (G_OBJECT (impl->priv->browse_files_tree_view), "fmq-name", "file_list");
  g_object_set_data (G_OBJECT (impl->priv->browse_files_tree_view), I_("GtkFileChooserWidget"), impl);

  /* Setup file list treeview */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->priv->browse_files_tree_view));
  gtk_tree_selection_set_select_function (selection,
					  list_select_func,
					  impl, NULL);
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (impl->priv->browse_files_tree_view),
					  GDK_BUTTON1_MASK,
					  NULL, 0,
					  GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_source_add_uri_targets (impl->priv->browse_files_tree_view);

  gtk_drag_dest_set (impl->priv->browse_files_tree_view,
                     GTK_DEST_DEFAULT_ALL,
                     NULL, 0,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_uri_targets (impl->priv->browse_files_tree_view);

  /* File browser treemodel columns are shared between GtkFileChooser implementations,
   * so we don't set cell renderer attributes in GtkBuilder, but rather keep that
   * in code.
   */
  file_list_set_sort_column_ids (impl);
  update_cell_renderer_attributes (impl);

  /* Get the combo's text renderer and set ellipsize parameters,
   * perhaps GtkComboBoxText should declare the cell renderer
   * as an 'internal-child', then we could configure it in GtkBuilder
   * instead of hard coding it here.
   */
  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (impl->priv->filter_combo));
  g_assert (cells);
  cell = cells->data;
  g_object_set (G_OBJECT (cell),
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);

  g_list_free (cells);

  /* Set the GtkPathBar file system backend */
  _gtk_path_bar_set_file_system (GTK_PATH_BAR (impl->priv->browse_path_bar), impl->priv->file_system);
  file = g_file_new_for_path ("/");
  _gtk_path_bar_set_file (GTK_PATH_BAR (impl->priv->browse_path_bar), file, FALSE);
  g_object_unref (file);

  /* Set the fixed size icon renderer, this requires
   * that priv->icon_size be already setup.
   */
  set_icon_cell_renderer_fixed_size (impl);

  gtk_popover_set_default_widget (GTK_POPOVER (impl->priv->new_folder_popover), impl->priv->new_folder_create_button);
}

static void
gtk_file_chooser_widget_init (GtkFileChooserWidget *impl)
{
  GtkFileChooserWidgetPrivate *priv;

  profile_start ("start", NULL);
#ifdef PROFILE_FILE_CHOOSER
  access ("MARK: *** CREATE FILE CHOOSER", F_OK);
#endif
  impl->priv = gtk_file_chooser_widget_get_instance_private (impl);
  priv = impl->priv;

  priv->local_only = TRUE;
  priv->preview_widget_active = TRUE;
  priv->use_preview_label = TRUE;
  priv->select_multiple = FALSE;
  priv->show_hidden = FALSE;
  priv->show_size_column = TRUE;
  priv->icon_size = FALLBACK_ICON_SIZE;
  priv->load_state = LOAD_EMPTY;
  priv->reload_state = RELOAD_EMPTY;
  priv->pending_select_files = NULL;
  priv->location_mode = LOCATION_MODE_PATH_BAR;
  priv->operation_mode = OPERATION_MODE_BROWSE;
  priv->sort_column = MODEL_COL_NAME;
  priv->sort_order = GTK_SORT_ASCENDING;
  priv->recent_manager = gtk_recent_manager_get_default ();
  priv->create_folders = TRUE;
  priv->auto_selecting_first_row = FALSE;

  /* Ensure GTK+ private types used by the template
   * definition before calling gtk_widget_init_template()
   */
  g_type_ensure (GTK_TYPE_PATH_BAR);
  gtk_widget_init_template (GTK_WIDGET (impl));
  gtk_widget_set_size_request (priv->browse_files_tree_view, 280, -1);

  set_file_system_backend (impl);

  priv->bookmarks_manager = _gtk_bookmarks_manager_new (NULL, NULL);

  /* Setup various attributes and callbacks in the UI 
   * which cannot be done with GtkBuilder.
   */
  post_process_ui (impl);

  profile_end ("end", NULL);
}

/**
 * gtk_file_chooser_widget_new:
 * @action: Open or save mode for the widget
 *
 * Creates a new #GtkFileChooserWidget.  This is a file chooser widget that can
 * be embedded in custom windows, and it is the same widget that is used by
 * #GtkFileChooserDialog.
 *
 * Returns: a new #GtkFileChooserWidget
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_widget_new (GtkFileChooserAction action)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
		       "action", action,
		       NULL);
}
