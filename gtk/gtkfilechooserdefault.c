/* GTK - The GIMP Toolkit
 * gtkfilechooserdefault.c: Default implementation of GtkFileChooser
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
#include "gdk/gdkkeysyms.h"
#include "gtkalias.h"
#include "gtkalignment.h"
#include "gtkbindings.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrenderertext.h"
#include "gtkcheckmenuitem.h"
#include "gtkcombobox.h"
#include "gtkentry.h"
#include "gtkeventbox.h"
#include "gtkexpander.h"
#include "gtkfilechooserdefault.h"
#include "gtkfilechooserembed.h"
#include "gtkfilechooserentry.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooser.h"
#include "gtkfilesystemmodel.h"
#include "gtkframe.h"
#include "gtkhbox.h"
#include "gtkhpaned.h"
#include "gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkmessagedialog.h"
#include "gtkpathbar.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "gtkseparatormenuitem.h"
#include "gtksizegroup.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtktreednd.h"
#include "gtktreeprivate.h"
#include "gtktreeview.h"
#include "gtktreemodelsort.h"
#include "gtktreeselection.h"
#include "gtktreestore.h"
#include "gtktooltips.h"
#include "gtktypebuiltins.h"
#include "gtkvbox.h"

#if defined (G_OS_UNIX)
#include "gtkfilesystemunix.h"
#elif defined (G_OS_WIN32)
#include "gtkfilesystemwin32.h"
#endif

#include <errno.h>
#include <string.h>
#include <time.h>

typedef struct _GtkFileChooserDefaultClass GtkFileChooserDefaultClass;

#define GTK_FILE_CHOOSER_DEFAULT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefaultClass))
#define GTK_IS_FILE_CHOOSER_DEFAULT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_DEFAULT))
#define GTK_FILE_CHOOSER_DEFAULT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_DEFAULT, GtkFileChooserDefaultClass))

typedef enum {
  LOAD_EMPTY,			/* There is no model */
  LOAD_PRELOAD,			/* Model is loading and a timer is running; model isn't inserted into the tree yet */
  LOAD_LOADING,			/* Timeout expired, model is inserted into the tree, but not fully loaded yet */
  LOAD_FINISHED			/* Model is fully loaded and inserted into the tree */
} LoadState;

#define MAX_LOADING_TIME 500

struct _GtkFileChooserDefaultClass
{
  GtkVBoxClass parent_class;
};

struct _GtkFileChooserDefault
{
  GtkVBox parent_instance;

  GtkFileChooserAction action;

  GtkFileSystem *file_system;

  /* Save mode widgets */
  GtkWidget *save_widgets;

  GtkWidget *save_file_name_entry;
  GtkWidget *save_folder_label;
  GtkWidget *save_folder_combo;
  GtkWidget *save_expander;

  /* The file browsing widgets */
  GtkWidget *browse_widgets;
  GtkWidget *browse_shortcuts_tree_view;
  GtkWidget *browse_shortcuts_add_button;
  GtkWidget *browse_shortcuts_remove_button;
  GtkWidget *browse_files_tree_view;
  GtkWidget *browse_files_popup_menu;
  GtkWidget *browse_files_popup_menu_add_shortcut_item;
  GtkWidget *browse_files_popup_menu_hidden_files_item;
  GtkWidget *browse_new_folder_button;
  GtkWidget *browse_path_bar;

  GtkFileSystemModel *browse_files_model;

  GtkWidget *filter_combo_hbox;
  GtkWidget *filter_combo;
  GtkWidget *preview_box;
  GtkWidget *preview_label;
  GtkWidget *preview_widget;
  GtkWidget *extra_align;
  GtkWidget *extra_widget;

  GtkListStore *shortcuts_model;
  GtkTreeModel *shortcuts_filter_model;

  GtkTreeModelSort *sort_model;

  LoadState load_state;
  guint load_timeout_id;

  GSList *pending_select_paths;

  GtkFileFilter *current_filter;
  GSList *filters;

  GtkTooltips *tooltips;

  gboolean has_home;
  gboolean has_desktop;

  int num_volumes;
  int num_shortcuts;
  int num_bookmarks;

  gulong volumes_changed_id;
  gulong bookmarks_changed_id;

  GtkFilePath *current_volume_path;
  GtkFilePath *current_folder;
  GtkFilePath *preview_path;
  char *preview_display_name;

  GtkTreeViewColumn *list_name_column;
  GtkCellRenderer *list_name_renderer;

  GSource *edited_idle;
  char *edited_new_text;

  gulong settings_signal_id;
  int icon_size;

  gulong toplevel_set_focus_id;
  GtkWidget *toplevel_last_focus_widget;

#if 0
  GdkDragContext *shortcuts_drag_context;
  GSource *shortcuts_drag_outside_idle;
#endif

  /* Flags */

  guint local_only : 1;
  guint preview_widget_active : 1;
  guint use_preview_label : 1;
  guint select_multiple : 1;
  guint show_hidden : 1;
  guint list_sort_ascending : 1;
  guint changing_folder : 1;
  guint shortcuts_current_folder_active : 1;

#if 0
  guint shortcuts_drag_outside : 1;
#endif
};

/* Signal IDs */
enum {
  LOCATION_POPUP,
  UP_FOLDER,
  DOWN_FOLDER,
  HOME_FOLDER,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Column numbers for the shortcuts tree.  Keep these in sync with shortcuts_model_create() */
enum {
  SHORTCUTS_COL_PIXBUF,
  SHORTCUTS_COL_NAME,
  SHORTCUTS_COL_DATA,
  SHORTCUTS_COL_IS_VOLUME,
  SHORTCUTS_COL_REMOVABLE,
  SHORTCUTS_COL_PIXBUF_VISIBLE,
  SHORTCUTS_COL_NUM_COLUMNS
};

/* Column numbers for the file list */
enum {
  FILE_LIST_COL_NAME,
  FILE_LIST_COL_SIZE,
  FILE_LIST_COL_MTIME,
  FILE_LIST_COL_NUM_COLUMNS
};

/* Identifiers for target types */
enum {
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry shortcuts_source_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

static const int num_shortcuts_source_targets = (sizeof (shortcuts_source_targets)
						 / sizeof (shortcuts_source_targets[0]));

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry shortcuts_dest_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
  { "text/uri-list", 0, TEXT_URI_LIST }
};

static const int num_shortcuts_dest_targets = (sizeof (shortcuts_dest_targets)
					       / sizeof (shortcuts_dest_targets[0]));

/* Target types for DnD from the file list */
static const GtkTargetEntry file_list_source_targets[] = {
  { "text/uri-list", 0, TEXT_URI_LIST }
};

static const int num_file_list_source_targets = (sizeof (file_list_source_targets)
						 / sizeof (file_list_source_targets[0]));

/* Interesting places in the shortcuts bar */
typedef enum {
  SHORTCUTS_HOME,
  SHORTCUTS_DESKTOP,
  SHORTCUTS_VOLUMES,
  SHORTCUTS_SHORTCUTS,
  SHORTCUTS_BOOKMARKS_SEPARATOR,
  SHORTCUTS_BOOKMARKS,
  SHORTCUTS_CURRENT_FOLDER_SEPARATOR,
  SHORTCUTS_CURRENT_FOLDER
} ShortcutsIndex;

/* Icon size for if we can't get it from the theme */
#define FALLBACK_ICON_SIZE 16

#define PREVIEW_HBOX_SPACING 12
#define NUM_LINES 40
#define NUM_CHARS 60

static void gtk_file_chooser_default_class_init       (GtkFileChooserDefaultClass *class);
static void gtk_file_chooser_default_iface_init       (GtkFileChooserIface        *iface);
static void gtk_file_chooser_embed_default_iface_init (GtkFileChooserEmbedIface   *iface);
static void gtk_file_chooser_default_init             (GtkFileChooserDefault      *impl);

static GObject* gtk_file_chooser_default_constructor  (GType                  type,
						       guint                  n_construct_properties,
						       GObjectConstructParam *construct_params);
static void     gtk_file_chooser_default_finalize     (GObject               *object);
static void     gtk_file_chooser_default_set_property (GObject               *object,
						       guint                  prop_id,
						       const GValue          *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_default_get_property (GObject               *object,
						       guint                  prop_id,
						       GValue                *value,
						       GParamSpec            *pspec);
static void     gtk_file_chooser_default_dispose      (GObject               *object);
static void     gtk_file_chooser_default_show_all       (GtkWidget             *widget);
static void     gtk_file_chooser_default_map            (GtkWidget             *widget);
static void     gtk_file_chooser_default_hierarchy_changed (GtkWidget          *widget,
							    GtkWidget          *previous_toplevel);
static void     gtk_file_chooser_default_style_set      (GtkWidget             *widget,
							 GtkStyle              *previous_style);
static void     gtk_file_chooser_default_screen_changed (GtkWidget             *widget,
							 GdkScreen             *previous_screen);

static gboolean       gtk_file_chooser_default_set_current_folder 	   (GtkFileChooser    *chooser,
									    const GtkFilePath *path,
									    GError           **error);
static GtkFilePath *  gtk_file_chooser_default_get_current_folder 	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_set_current_name   	   (GtkFileChooser    *chooser,
									    const gchar       *name);
static gboolean       gtk_file_chooser_default_select_path        	   (GtkFileChooser    *chooser,
									    const GtkFilePath *path,
									    GError           **error);
static void           gtk_file_chooser_default_unselect_path      	   (GtkFileChooser    *chooser,
									    const GtkFilePath *path);
static void           gtk_file_chooser_default_select_all         	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_unselect_all       	   (GtkFileChooser    *chooser);
static GSList *       gtk_file_chooser_default_get_paths          	   (GtkFileChooser    *chooser);
static GtkFilePath *  gtk_file_chooser_default_get_preview_path   	   (GtkFileChooser    *chooser);
static GtkFileSystem *gtk_file_chooser_default_get_file_system    	   (GtkFileChooser    *chooser);
static void           gtk_file_chooser_default_add_filter         	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static void           gtk_file_chooser_default_remove_filter      	   (GtkFileChooser    *chooser,
									    GtkFileFilter     *filter);
static GSList *       gtk_file_chooser_default_list_filters       	   (GtkFileChooser    *chooser);
static gboolean       gtk_file_chooser_default_add_shortcut_folder    (GtkFileChooser    *chooser,
								       const GtkFilePath *path,
								       GError           **error);
static gboolean       gtk_file_chooser_default_remove_shortcut_folder (GtkFileChooser    *chooser,
								       const GtkFilePath *path,
								       GError           **error);
static GSList *       gtk_file_chooser_default_list_shortcut_folders  (GtkFileChooser    *chooser);

static void           gtk_file_chooser_default_get_default_size       (GtkFileChooserEmbed *chooser_embed,
								       gint                *default_width,
								       gint                *default_height);
static void           gtk_file_chooser_default_get_resizable_hints    (GtkFileChooserEmbed *chooser_embed,
								       gboolean            *resize_horizontally,
								       gboolean            *resize_vertically);
static gboolean       gtk_file_chooser_default_should_respond         (GtkFileChooserEmbed *chooser_embed);
static void           gtk_file_chooser_default_initial_focus          (GtkFileChooserEmbed *chooser_embed);

static void location_popup_handler (GtkFileChooserDefault *impl,
				    const gchar           *path);
static void up_folder_handler      (GtkFileChooserDefault *impl);
static void down_folder_handler    (GtkFileChooserDefault *impl);
static void home_folder_handler    (GtkFileChooserDefault *impl);
static void update_appearance      (GtkFileChooserDefault *impl);

static void set_current_filter   (GtkFileChooserDefault *impl,
				  GtkFileFilter         *filter);
static void check_preview_change (GtkFileChooserDefault *impl);

static void filter_combo_changed       (GtkComboBox           *combo_box,
					GtkFileChooserDefault *impl);
static void     shortcuts_row_activated_cb (GtkTreeView           *tree_view,
					    GtkTreePath           *path,
					    GtkTreeViewColumn     *column,
					    GtkFileChooserDefault *impl);

static gboolean shortcuts_key_press_event_cb (GtkWidget             *widget,
					      GdkEventKey           *event,
					      GtkFileChooserDefault *impl);

static gboolean shortcuts_select_func   (GtkTreeSelection      *selection,
					 GtkTreeModel          *model,
					 GtkTreePath           *path,
					 gboolean               path_currently_selected,
					 gpointer               data);
static gboolean shortcuts_get_selected  (GtkFileChooserDefault *impl,
					 GtkTreeIter           *iter);
static void shortcuts_activate_iter (GtkFileChooserDefault *impl,
				     GtkTreeIter           *iter);
static int shortcuts_get_index (GtkFileChooserDefault *impl,
				ShortcutsIndex         where);
static int shortcut_find_position (GtkFileChooserDefault *impl,
				   const GtkFilePath     *path);

static void bookmarks_check_add_sensitivity (GtkFileChooserDefault *impl);

static gboolean list_select_func   (GtkTreeSelection      *selection,
				    GtkTreeModel          *model,
				    GtkTreePath           *path,
				    gboolean               path_currently_selected,
				    gpointer               data);

static void list_selection_changed     (GtkTreeSelection      *tree_selection,
					GtkFileChooserDefault *impl);
static void list_row_activated         (GtkTreeView           *tree_view,
					GtkTreePath           *path,
					GtkTreeViewColumn     *column,
					GtkFileChooserDefault *impl);

static void select_func (GtkFileSystemModel *model,
			 GtkTreePath        *path,
			 GtkTreeIter        *iter,
			 gpointer            user_data);

static void path_bar_clicked           (GtkPathBar            *path_bar,
					GtkFilePath           *file_path,
					gboolean               child_is_hidden,
					GtkFileChooserDefault *impl);

static void add_bookmark_button_clicked_cb    (GtkButton             *button,
					       GtkFileChooserDefault *impl);
static void remove_bookmark_button_clicked_cb (GtkButton             *button,
					       GtkFileChooserDefault *impl);

static void list_icon_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
static void list_name_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
#if 0
static void list_size_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *tree_model,
				 GtkTreeIter       *iter,
				 gpointer           data);
#endif
static void list_mtime_data_func (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *tree_model,
				  GtkTreeIter       *iter,
				  gpointer           data);

static const GtkFileInfo *get_list_file_info (GtkFileChooserDefault *impl,
					      GtkTreeIter           *iter);

static void load_remove_timer (GtkFileChooserDefault *impl);

static GObjectClass *parent_class;



/* Drag and drop interface declarations */

typedef struct {
  GtkTreeModelFilter parent;

  GtkFileChooserDefault *impl;
} ShortcutsModelFilter;

typedef struct {
  GtkTreeModelFilterClass parent_class;
} ShortcutsModelFilterClass;

#define SHORTCUTS_MODEL_FILTER_TYPE (_shortcuts_model_filter_get_type ())
#define SHORTCUTS_MODEL_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHORTCUTS_MODEL_FILTER_TYPE, ShortcutsModelFilter))

static void shortcuts_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (ShortcutsModelFilter,
			 _shortcuts_model_filter,
			 GTK_TYPE_TREE_MODEL_FILTER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						shortcuts_model_filter_drag_source_iface_init));

static GtkTreeModel *shortcuts_model_filter_new (GtkFileChooserDefault *impl,
						 GtkTreeModel          *child_model,
						 GtkTreePath           *root);



GType
_gtk_file_chooser_default_get_type (void)
{
  static GType file_chooser_default_type = 0;

  if (!file_chooser_default_type)
    {
      static const GTypeInfo file_chooser_default_info =
      {
	sizeof (GtkFileChooserDefaultClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_chooser_default_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileChooserDefault),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_chooser_default_init,
      };

      static const GInterfaceInfo file_chooser_info =
      {
	(GInterfaceInitFunc) gtk_file_chooser_default_iface_init, /* interface_init */
	NULL,			                                       /* interface_finalize */
	NULL			                                       /* interface_data */
      };

      static const GInterfaceInfo file_chooser_embed_info =
      {
	(GInterfaceInitFunc) gtk_file_chooser_embed_default_iface_init, /* interface_init */
	NULL,			                                       /* interface_finalize */
	NULL			                                       /* interface_data */
      };

      file_chooser_default_type = g_type_register_static (GTK_TYPE_VBOX, "GtkFileChooserDefault",
							 &file_chooser_default_info, 0);

      g_type_add_interface_static (file_chooser_default_type,
				   GTK_TYPE_FILE_CHOOSER,
				   &file_chooser_info);
      g_type_add_interface_static (file_chooser_default_type,
				   GTK_TYPE_FILE_CHOOSER_EMBED,
				   &file_chooser_embed_info);
    }

  return file_chooser_default_type;
}

static void
gtk_file_chooser_default_class_init (GtkFileChooserDefaultClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_file_chooser_default_finalize;
  gobject_class->constructor = gtk_file_chooser_default_constructor;
  gobject_class->set_property = gtk_file_chooser_default_set_property;
  gobject_class->get_property = gtk_file_chooser_default_get_property;
  gobject_class->dispose = gtk_file_chooser_default_dispose;

  widget_class->show_all = gtk_file_chooser_default_show_all;
  widget_class->map = gtk_file_chooser_default_map;
  widget_class->hierarchy_changed = gtk_file_chooser_default_hierarchy_changed;
  widget_class->style_set = gtk_file_chooser_default_style_set;
  widget_class->screen_changed = gtk_file_chooser_default_screen_changed;

  signals[LOCATION_POPUP] =
    _gtk_binding_signal_new ("location-popup",
			     G_OBJECT_CLASS_TYPE (class),
			     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			     G_CALLBACK (location_popup_handler),
			     NULL, NULL,
			     _gtk_marshal_VOID__STRING,
			     G_TYPE_NONE, 1, G_TYPE_STRING);
  signals[UP_FOLDER] =
    _gtk_binding_signal_new ("up-folder",
			     G_OBJECT_CLASS_TYPE (class),
			     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			     G_CALLBACK (up_folder_handler),
			     NULL, NULL,
			     _gtk_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
  signals[DOWN_FOLDER] =
    _gtk_binding_signal_new ("down-folder",
			     G_OBJECT_CLASS_TYPE (class),
			     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			     G_CALLBACK (down_folder_handler),
			     NULL, NULL,
			     _gtk_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
  signals[HOME_FOLDER] =
    _gtk_binding_signal_new ("home-folder",
			     G_OBJECT_CLASS_TYPE (class),
			     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			     G_CALLBACK (home_folder_handler),
			     NULL, NULL,
			     _gtk_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (class);

  gtk_binding_entry_add_signal (binding_set,
				GDK_l, GDK_CONTROL_MASK,
				"location-popup",
				1, G_TYPE_STRING, "");

  gtk_binding_entry_add_signal (binding_set,
				GDK_slash, 0,
				"location-popup",
				1, G_TYPE_STRING, "/");

  gtk_binding_entry_add_signal (binding_set,
				GDK_Up, GDK_MOD1_MASK,
				"up-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
		  		GDK_BackSpace, 0,
				"up-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Up, GDK_MOD1_MASK,
				"up-folder",
				0);

  gtk_binding_entry_add_signal (binding_set,
				GDK_Down, GDK_MOD1_MASK,
				"down-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Down, GDK_MOD1_MASK,
				"down-folder",
				0);

  gtk_binding_entry_add_signal (binding_set,
				GDK_Home, GDK_MOD1_MASK,
				"home-folder",
				0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Home, GDK_MOD1_MASK,
				"home-folder",
				0);

  _gtk_file_chooser_install_properties (gobject_class);

  gtk_settings_install_property (g_param_spec_string ("gtk-file-chooser-backend",
						      P_("Default file chooser backend"),
						      P_("Name of the GtkFileChooser backend to use by default"),
						      NULL,
						      G_PARAM_READWRITE));
}

static void
gtk_file_chooser_default_iface_init (GtkFileChooserIface *iface)
{
  iface->select_path = gtk_file_chooser_default_select_path;
  iface->unselect_path = gtk_file_chooser_default_unselect_path;
  iface->select_all = gtk_file_chooser_default_select_all;
  iface->unselect_all = gtk_file_chooser_default_unselect_all;
  iface->get_paths = gtk_file_chooser_default_get_paths;
  iface->get_preview_path = gtk_file_chooser_default_get_preview_path;
  iface->get_file_system = gtk_file_chooser_default_get_file_system;
  iface->set_current_folder = gtk_file_chooser_default_set_current_folder;
  iface->get_current_folder = gtk_file_chooser_default_get_current_folder;
  iface->set_current_name = gtk_file_chooser_default_set_current_name;
  iface->add_filter = gtk_file_chooser_default_add_filter;
  iface->remove_filter = gtk_file_chooser_default_remove_filter;
  iface->list_filters = gtk_file_chooser_default_list_filters;
  iface->add_shortcut_folder = gtk_file_chooser_default_add_shortcut_folder;
  iface->remove_shortcut_folder = gtk_file_chooser_default_remove_shortcut_folder;
  iface->list_shortcut_folders = gtk_file_chooser_default_list_shortcut_folders;
}

static void
gtk_file_chooser_embed_default_iface_init (GtkFileChooserEmbedIface *iface)
{
  iface->get_default_size = gtk_file_chooser_default_get_default_size;
  iface->get_resizable_hints = gtk_file_chooser_default_get_resizable_hints;
  iface->should_respond = gtk_file_chooser_default_should_respond;
  iface->initial_focus = gtk_file_chooser_default_initial_focus;
}
static void
gtk_file_chooser_default_init (GtkFileChooserDefault *impl)
{
  impl->local_only = TRUE;
  impl->preview_widget_active = TRUE;
  impl->use_preview_label = TRUE;
  impl->select_multiple = FALSE;
  impl->show_hidden = FALSE;
  impl->icon_size = FALLBACK_ICON_SIZE;
  impl->load_state = LOAD_EMPTY;
  impl->pending_select_paths = NULL;

  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (impl), TRUE);
  gtk_box_set_spacing (GTK_BOX (impl), 12);

  impl->tooltips = gtk_tooltips_new ();
  g_object_ref (impl->tooltips);
  gtk_object_sink (GTK_OBJECT (impl->tooltips));
}

/* Frees the data columns for the specified iter in the shortcuts model*/
static void
shortcuts_free_row_data (GtkFileChooserDefault *impl,
			 GtkTreeIter           *iter)
{
  gpointer col_data;
  gboolean is_volume;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_IS_VOLUME, &is_volume,
		      -1);
  if (!col_data)
    return;

  if (is_volume)
    {
      GtkFileSystemVolume *volume;

      volume = col_data;
      gtk_file_system_volume_free (impl->file_system, volume);
    }
  else
    {
      GtkFilePath *path;

      path = col_data;
      gtk_file_path_free (path);
    }
}

/* Frees all the data columns in the shortcuts model */
static void
shortcuts_free (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;

  if (!impl->shortcuts_model)
    return;

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    do
      {
	shortcuts_free_row_data (impl, &iter);
      }
    while (gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter));

  g_object_unref (impl->shortcuts_model);
  impl->shortcuts_model = NULL;
}

static void
pending_select_paths_free (GtkFileChooserDefault *impl)
{
  GSList *l;

  for (l = impl->pending_select_paths; l; l = l->next)
    {
      GtkFilePath *path;

      path = l->data;
      gtk_file_path_free (path);
    }

  g_slist_free (impl->pending_select_paths);
  impl->pending_select_paths = NULL;
}

static void
pending_select_paths_add (GtkFileChooserDefault *impl,
			  const GtkFilePath     *path)
{
  impl->pending_select_paths = g_slist_prepend (impl->pending_select_paths, gtk_file_path_copy (path));
}

/* Used from gtk_tree_selection_selected_foreach() */
static void
store_selection_foreach (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      data)
{
  GtkFileChooserDefault *impl;
  GtkTreeIter child_iter;
  const GtkFilePath *file_path;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model, &child_iter, iter);

  file_path = _gtk_file_system_model_get_path (impl->browse_files_model, &child_iter);
  pending_select_paths_add (impl, file_path);
}

/* Stores the current selection in the list of paths to select; this is used to
 * preserve the selection when reloading the current folder.
 */
static void
pending_select_paths_store_selection (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, store_selection_foreach, impl);
}

static void
gtk_file_chooser_default_finalize (GObject *object)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);
  GSList *l;

  if (impl->shortcuts_filter_model)
    g_object_unref (impl->shortcuts_filter_model);

  shortcuts_free (impl);

  g_signal_handler_disconnect (impl->file_system, impl->volumes_changed_id);
  impl->volumes_changed_id = 0;
  g_signal_handler_disconnect (impl->file_system, impl->bookmarks_changed_id);
  impl->bookmarks_changed_id = 0;
  g_object_unref (impl->file_system);

  for (l = impl->filters; l; l = l->next)
    {
      GtkFileFilter *filter;

      filter = GTK_FILE_FILTER (l->data);
      g_object_unref (filter);
    }
  g_slist_free (impl->filters);

  if (impl->current_filter)
    g_object_unref (impl->current_filter);

  if (impl->current_volume_path)
    gtk_file_path_free (impl->current_volume_path);

  if (impl->current_folder)
    gtk_file_path_free (impl->current_folder);

  if (impl->preview_path)
    gtk_file_path_free (impl->preview_path);

  pending_select_paths_free (impl);

  load_remove_timer (impl);

  /* Free all the Models we have */
  if (impl->browse_files_model)
    g_object_unref (impl->browse_files_model);

  if (impl->sort_model)
    g_object_unref (impl->sort_model);

  g_free (impl->preview_display_name);

  g_free (impl->edited_new_text);

  g_object_unref (impl->tooltips);

  G_OBJECT_CLASS (parent_class)->finalize (object);
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
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!GTK_WIDGET_TOPLEVEL (toplevel))
    return NULL;
  else
    return GTK_WINDOW (toplevel);
}

/* Shows an error dialog for the file chooser */
static void
error_message (GtkFileChooserDefault *impl,
	       const char            *msg,
	       const char            *detail)
{
  error_message_with_parent (get_toplevel (GTK_WIDGET (impl)), msg, detail);
}

/* Shows a simple error dialog relative to a path.  Frees the GError as well. */
static void
error_dialog (GtkFileChooserDefault *impl,
	      const char            *msg,
	      const GtkFilePath     *path,
	      GError                *error)
{
  if (error)
    {
      char *uri = NULL;
      char *text;

      if (path)
	uri = gtk_file_system_path_to_uri (impl->file_system, path);
      text = g_strdup_printf (msg, uri);
      error_message (impl, text, error->message);
      g_free (text);
      g_free (uri);
      g_error_free (error);
    }
}

/* Displays an error message about not being able to get information for a file.
 * Frees the GError as well.
 */
static void
error_getting_info_dialog (GtkFileChooserDefault *impl,
			   const GtkFilePath     *path,
			   GError                *error)
{
  error_dialog (impl,
		_("Could not retrieve information about the file"),
		path, error);
}

/* Shows an error dialog about not being able to add a bookmark */
static void
error_adding_bookmark_dialog (GtkFileChooserDefault *impl,
			      const GtkFilePath     *path,
			      GError                *error)
{
  error_dialog (impl,
		_("Could not add a bookmark"),
		path, error);
}

/* Shows an error dialog about not being able to remove a bookmark */
static void
error_removing_bookmark_dialog (GtkFileChooserDefault *impl,
				const GtkFilePath     *path,
				GError                *error)
{
  error_dialog (impl,
		_("Could not remove bookmark"),
		path, error);
}

/* Shows an error dialog about not being able to create a folder */
static void
error_creating_folder_dialog (GtkFileChooserDefault *impl,
			      const GtkFilePath     *path,
			      GError                *error)
{
  error_dialog (impl, 
		_("The folder could not be created"), 
		path, error);
}

/* Shows an error dialog about not being able to create a filename */
static void
error_building_filename_dialog (GtkFileChooserDefault *impl,
				const GtkFilePath     *folder_part,
				const char            *file_part,
				GError                *error)
{
  error_dialog (impl, _("Invalid file name"), 
		NULL, error);
}

/* Shows an error dialog when we cannot switch to a folder */
static void
error_changing_folder_dialog (GtkFileChooserDefault *impl,
			      const GtkFilePath     *path,
			      GError                *error)
{
  error_dialog (impl, _("The folder contents could not be displayed"),
		path, error);
}

/* Changes folders, displaying an error dialog if this fails */
static gboolean
change_folder_and_display_error (GtkFileChooserDefault *impl,
				 const GtkFilePath     *path)
{
  GError *error;
  gboolean result;
  GtkFilePath *path_copy;

  /* We copy the path because of this case:
   *
   * list_row_activated()
   *   fetches path from model; path belongs to the model (*)
   *   calls change_folder_and_display_error()
   *     calls _gtk_file_chooser_set_current_folder_path()
   *       changing folders fails, sets model to NULL, thus freeing the path in (*)
   */

  path_copy = gtk_file_path_copy (path);

  error = NULL;
  result = _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), path_copy, &error);

  if (!result)
    error_changing_folder_dialog (impl, path_copy, error);

  gtk_file_path_free (path_copy);

  return result;
}

static void
update_preview_widget_visibility (GtkFileChooserDefault *impl)
{
  if (impl->use_preview_label)
    {
      if (!impl->preview_label)
	{
	  impl->preview_label = gtk_label_new (impl->preview_display_name);
	  gtk_box_pack_start (GTK_BOX (impl->preview_box), impl->preview_label, FALSE, FALSE, 0);
	  gtk_box_reorder_child (GTK_BOX (impl->preview_box), impl->preview_label, 0);
	  gtk_widget_show (impl->preview_label);
	}
    }
  else
    {
      if (impl->preview_label)
	{
	  gtk_widget_destroy (impl->preview_label);
	  impl->preview_label = NULL;
	}
    }

  if (impl->preview_widget_active && impl->preview_widget)
    gtk_widget_show (impl->preview_box);
  else
    gtk_widget_hide (impl->preview_box);

  g_signal_emit_by_name (impl, "default-size-changed");
}

static void
set_preview_widget (GtkFileChooserDefault *impl,
		    GtkWidget             *preview_widget)
{
  if (preview_widget == impl->preview_widget)
    return;

  if (impl->preview_widget)
    gtk_container_remove (GTK_CONTAINER (impl->preview_box),
			  impl->preview_widget);

  impl->preview_widget = preview_widget;
  if (impl->preview_widget)
    {
      gtk_widget_show (impl->preview_widget);
      gtk_box_pack_start (GTK_BOX (impl->preview_box), impl->preview_widget, TRUE, TRUE, 0);
      gtk_box_reorder_child (GTK_BOX (impl->preview_box),
			     impl->preview_widget,
			     (impl->use_preview_label && impl->preview_label) ? 1 : 0);
    }

  update_preview_widget_visibility (impl);
}

/* Re-reads all the icons for the shortcuts, used when the theme changes */
static void
shortcuts_reload_icons (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    return;

  do {
    gpointer data;
    gboolean is_volume;
    gboolean pixbuf_visible;
    GdkPixbuf *pixbuf;

    gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			SHORTCUTS_COL_DATA, &data,
			SHORTCUTS_COL_IS_VOLUME, &is_volume,
			SHORTCUTS_COL_PIXBUF_VISIBLE, &pixbuf_visible,
			-1);

    if (pixbuf_visible && data)
      {
	if (is_volume)
	  {
	    GtkFileSystemVolume *volume;

	    volume = data;
	    pixbuf = gtk_file_system_volume_render_icon (impl->file_system, volume, GTK_WIDGET (impl),
							 impl->icon_size, NULL);
	  }
	else
	  {
	    const GtkFilePath *path;

	    path = data;
	    pixbuf = gtk_file_system_render_icon (impl->file_system, path, GTK_WIDGET (impl),
						  impl->icon_size, NULL);
	  }

	gtk_list_store_set (impl->shortcuts_model, &iter,
			    SHORTCUTS_COL_PIXBUF, pixbuf,
			    -1);
	if (pixbuf)
	  g_object_unref (pixbuf);
      }
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model),&iter));
}

static void 
shortcuts_find_folder (GtkFileChooserDefault *impl,
		       GtkFilePath           *folder)
{
  GtkTreeSelection *selection;
  int pos;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));

  g_assert (folder != NULL);
  pos = shortcut_find_position (impl, folder);
  if (pos == -1)
    {
      gtk_tree_selection_unselect_all (selection);
      return;
    }

  path = gtk_tree_path_new_from_indices (pos, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}

/* If a shortcut corresponds to the current folder, selects it */
static void
shortcuts_find_current_folder (GtkFileChooserDefault *impl)
{
  shortcuts_find_folder (impl, impl->current_folder);
}

/* Convenience function to get the display name and icon info for a path */
static GtkFileInfo *
get_file_info (GtkFileSystem      *file_system, 
	       const GtkFilePath  *path, 
	       gboolean            name_only, 
	       GError            **error)
{
  GtkFilePath *parent_path;
  GtkFileFolder *parent_folder;
  GtkFileInfo *info;
  GError *tmp = NULL;

  parent_path = NULL;
  info = NULL;

  if (!gtk_file_system_get_parent (file_system, path, &parent_path, &tmp))
    goto out;

  parent_folder = gtk_file_system_get_folder (file_system, parent_path ? parent_path : path,
					      GTK_FILE_INFO_DISPLAY_NAME
					      | (name_only ? 0 : GTK_FILE_INFO_IS_FOLDER),
					      &tmp);
  if (!parent_folder)
    goto out;

  info = gtk_file_folder_get_info (parent_folder, parent_path ? path : NULL, &tmp);
  g_object_unref (parent_folder);

 out:
  if (parent_path)
    gtk_file_path_free (parent_path);

  if (tmp)
    {
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		   _("Could not get information about '%s': %s"), 
		   gtk_file_path_get_string (path),
		   tmp->message);
      g_error_free (tmp);
    }

  return info;
}

/* Returns whether a path is a folder */
static gboolean
check_is_folder (GtkFileSystem      *file_system, 
		 const GtkFilePath  *path, 
		 GError            **error)
{
  GtkFileFolder *folder;

  folder = gtk_file_system_get_folder (file_system, path, 0, error);
  if (!folder)
    return FALSE;

  g_object_unref (folder);
  return TRUE;
}

/* Inserts a path in the shortcuts tree, making a copy of it; alternatively,
 * inserts a volume.  A position of -1 indicates the end of the tree.
 */
static gboolean
shortcuts_insert_path (GtkFileChooserDefault *impl,
		       int                    pos,
		       gboolean               is_volume,
		       GtkFileSystemVolume   *volume,
		       const GtkFilePath     *path,
		       const char            *label,
		       gboolean               removable,
		       GError               **error)
{
  char *label_copy;
  GdkPixbuf *pixbuf;
  gpointer data;
  GtkTreeIter iter;

  if (is_volume)
    {
      data = volume;
      label_copy = gtk_file_system_volume_get_display_name (impl->file_system, volume);
      pixbuf = gtk_file_system_volume_render_icon (impl->file_system, volume, GTK_WIDGET (impl),
						   impl->icon_size, NULL);
    }
  else
    {
      if (!check_is_folder (impl->file_system, path, error))
	return FALSE;

      if (label)
	label_copy = g_strdup (label);
      else
	{
	  GtkFileInfo *info = get_file_info (impl->file_system, path, TRUE, error);

	  if (!info)
	    return FALSE;

	  label_copy = g_strdup (gtk_file_info_get_display_name (info));
	  gtk_file_info_free (info);
	}

      data = gtk_file_path_copy (path);
      pixbuf = gtk_file_system_render_icon (impl->file_system, path, GTK_WIDGET (impl),
					    impl->icon_size, NULL);
    }

  if (pos == -1)
    gtk_list_store_append (impl->shortcuts_model, &iter);
  else
    gtk_list_store_insert (impl->shortcuts_model, &iter, pos);

  gtk_list_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, pixbuf,
		      SHORTCUTS_COL_PIXBUF_VISIBLE, TRUE,
		      SHORTCUTS_COL_NAME, label_copy,
		      SHORTCUTS_COL_DATA, data,
		      SHORTCUTS_COL_IS_VOLUME, is_volume,
		      SHORTCUTS_COL_REMOVABLE, removable,
		      -1);

  g_free (label_copy);

  if (pixbuf)
    g_object_unref (pixbuf);

  return TRUE;
}

/* Appends an item for the user's home directory to the shortcuts model */
static void
shortcuts_append_home (GtkFileChooserDefault *impl)
{
  const char *home;
  GtkFilePath *home_path;
  GError *error;

  home = g_get_home_dir ();
  if (home == NULL)
    return;

  home_path = gtk_file_system_filename_to_path (impl->file_system, home);

  error = NULL;
  impl->has_home = shortcuts_insert_path (impl, -1, FALSE, NULL, home_path, _("Home"), FALSE, &error);
  if (!impl->has_home)
    error_getting_info_dialog (impl, home_path, error);

  gtk_file_path_free (home_path);
}

/* Appends the ~/Desktop directory to the shortcuts model */
static void
shortcuts_append_desktop (GtkFileChooserDefault *impl)
{
  char *name;
  GtkFilePath *path;

#ifdef G_OS_WIN32
  name = _gtk_file_system_win32_get_desktop ();
#else
  const char *home = g_get_home_dir ();
  if (home == NULL)
    return;

  name = g_build_filename (home, "Desktop", NULL);
#endif

  path = gtk_file_system_filename_to_path (impl->file_system, name);
  g_free (name);

  impl->has_desktop = shortcuts_insert_path (impl, -1, FALSE, NULL, path, _("Desktop"), FALSE, NULL);
  /* We do not actually pop up an error dialog if there is no desktop directory
   * because some people may really not want to have one.
   */

  gtk_file_path_free (path);
}

/* Appends a list of GtkFilePath to the shortcuts model; returns how many were inserted */
static int
shortcuts_append_paths (GtkFileChooserDefault *impl,
			GSList                *paths)
{
  int start_row;
  int num_inserted;

  /* As there is no separator now, we want to start there.
   */
  start_row = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR);
  num_inserted = 0;

  for (; paths; paths = paths->next)
    {
      GtkFilePath *path;
      GError *error;

      path = paths->data;
      error = NULL;

      if (impl->local_only &&
	  !gtk_file_system_path_is_local (impl->file_system, path))
	continue;

      /* NULL GError, but we don't really want to show error boxes here */
      if (shortcuts_insert_path (impl, start_row + num_inserted, FALSE, NULL, path, NULL, TRUE, NULL))
	num_inserted++;
    }

  return num_inserted;
}

/* Returns the index for the corresponding item in the shortcuts bar */
static int
shortcuts_get_index (GtkFileChooserDefault *impl,
		     ShortcutsIndex         where)
{
  int n;

  n = 0;

  if (where == SHORTCUTS_HOME)
    goto out;

  n += impl->has_home ? 1 : 0;

  if (where == SHORTCUTS_DESKTOP)
    goto out;

  n += impl->has_desktop ? 1 : 0;

  if (where == SHORTCUTS_VOLUMES)
    goto out;

  n += impl->num_volumes;

  if (where == SHORTCUTS_SHORTCUTS)
    goto out;

  n += impl->num_shortcuts;

  if (where == SHORTCUTS_BOOKMARKS_SEPARATOR)
    goto out;

  /* If there are no bookmarks there won't be a separator */
  n += (impl->num_bookmarks > 0) ? 1 : 0;

  if (where == SHORTCUTS_BOOKMARKS)
    goto out;

  n += impl->num_bookmarks;

  if (where == SHORTCUTS_CURRENT_FOLDER_SEPARATOR)
    goto out;

  n += 1;

  if (where == SHORTCUTS_CURRENT_FOLDER)
    goto out;

  g_assert_not_reached ();

 out:

  return n;
}

/* Removes the specified number of rows from the shortcuts list */
static void
shortcuts_remove_rows (GtkFileChooserDefault *impl,
		       int                    start_row,
		       int                    n_rows)
{
  GtkTreePath *path;

  path = gtk_tree_path_new_from_indices (start_row, -1);

  for (; n_rows; n_rows--)
    {
      GtkTreeIter iter;

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->shortcuts_model), &iter, path))
	g_assert_not_reached ();

      shortcuts_free_row_data (impl, &iter);
      gtk_list_store_remove (impl->shortcuts_model, &iter);
    }

  gtk_tree_path_free (path);
}

/* Adds all the file system volumes to the shortcuts model */
static void
shortcuts_add_volumes (GtkFileChooserDefault *impl)
{
  int start_row;
  GSList *list, *l;
  int n;
  gboolean old_changing_folders;

  old_changing_folders = impl->changing_folder;
  impl->changing_folder = TRUE;

  start_row = shortcuts_get_index (impl, SHORTCUTS_VOLUMES);
  shortcuts_remove_rows (impl, start_row, impl->num_volumes);
  impl->num_volumes = 0;

  list = gtk_file_system_list_volumes (impl->file_system);

  n = 0;

  for (l = list; l; l = l->next)
    {
      GtkFileSystemVolume *volume;

      volume = l->data;

      if (impl->local_only)
	{
	  GtkFilePath *base_path = gtk_file_system_volume_get_base_path (impl->file_system, volume);
	  gboolean is_local = gtk_file_system_path_is_local (impl->file_system, base_path);
	  gtk_file_path_free (base_path);

	  if (!is_local)
	    {
	      gtk_file_system_volume_free (impl->file_system, volume);
	      continue;
	    }
	}

      if (shortcuts_insert_path (impl, start_row + n, TRUE, volume, NULL, NULL, FALSE, NULL))
	n++;
      else
	gtk_file_system_volume_free (impl->file_system, volume);
    }

  impl->num_volumes = n;
  g_slist_free (list);

  if (impl->shortcuts_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (impl->shortcuts_filter_model));

  impl->changing_folder = old_changing_folders;
}

/* Inserts a separator node in the shortcuts list */
static void
shortcuts_insert_separator (GtkFileChooserDefault *impl,
			    ShortcutsIndex where)
{
  GtkTreeIter iter;

  g_assert (where == SHORTCUTS_BOOKMARKS_SEPARATOR || where == SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

  gtk_list_store_insert (impl->shortcuts_model, &iter,
			 shortcuts_get_index (impl, where));
  gtk_list_store_set (impl->shortcuts_model, &iter,
		      SHORTCUTS_COL_PIXBUF, NULL,
		      SHORTCUTS_COL_PIXBUF_VISIBLE, FALSE,
		      SHORTCUTS_COL_NAME, NULL,
		      SHORTCUTS_COL_DATA, NULL,
		      -1);
}

/* Updates the list of bookmarks */
static void
shortcuts_add_bookmarks (GtkFileChooserDefault *impl)
{
  GSList *bookmarks;
  gboolean old_changing_folders;
  GtkTreeIter iter;
  GtkFilePath *list_selected = NULL;
  GtkFilePath *combo_selected = NULL;
  gboolean is_volume;
  gpointer col_data;
        
  old_changing_folders = impl->changing_folder;
  impl->changing_folder = TRUE;

  if (shortcuts_get_selected (impl, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), 
			  &iter, 
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_IS_VOLUME, &is_volume,
			  -1);

      if (col_data && !is_volume)
	list_selected = gtk_file_path_copy (col_data);
    }

  if (impl->save_folder_combo &&
      gtk_combo_box_get_active_iter (GTK_COMBO_BOX (impl->save_folder_combo), 
				     &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), 
			  &iter, 
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_IS_VOLUME, &is_volume,
			  -1);
      
      if (col_data && !is_volume)
	combo_selected = gtk_file_path_copy (col_data);
    }

  if (impl->num_bookmarks > 0)
    shortcuts_remove_rows (impl,
			   shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR),
			   impl->num_bookmarks + 1);

  bookmarks = gtk_file_system_list_bookmarks (impl->file_system);
  impl->num_bookmarks = shortcuts_append_paths (impl, bookmarks);
  gtk_file_paths_free (bookmarks);

  if (impl->num_bookmarks > 0)
    shortcuts_insert_separator (impl, SHORTCUTS_BOOKMARKS_SEPARATOR);

  if (impl->shortcuts_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (impl->shortcuts_filter_model));

  if (list_selected)
    {
      shortcuts_find_folder (impl, list_selected);
      gtk_file_path_free (list_selected);
    }

  if (combo_selected)
    {
      gint pos;

      pos = shortcut_find_position (impl, combo_selected);
      if (pos != -1)
	gtk_combo_box_set_active (GTK_COMBO_BOX (impl->save_folder_combo), 
				  pos);
      gtk_file_path_free (combo_selected);
    }
  
  impl->changing_folder = old_changing_folders;
}

/* Appends a separator and a row to the shortcuts list for the current folder */
static void
shortcuts_add_current_folder (GtkFileChooserDefault *impl)
{
  int pos;
  gboolean success;

  g_assert (!impl->shortcuts_current_folder_active);

  success = TRUE;

  g_assert (impl->current_folder != NULL);

  pos = shortcut_find_position (impl, impl->current_folder);
  if (pos == -1)
    {
      GtkFileSystemVolume *volume;
      GtkFilePath *base_path;

      /* Separator */

      shortcuts_insert_separator (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

      /* Item */

      pos = shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER);

      volume = gtk_file_system_get_volume_for_path (impl->file_system, impl->current_folder);
      if (volume)
	base_path = gtk_file_system_volume_get_base_path (impl->file_system, volume);
      else
	base_path = NULL;

      if (base_path &&
	  strcmp (gtk_file_path_get_string (base_path), gtk_file_path_get_string (impl->current_folder)) == 0)
	{
	  success = shortcuts_insert_path (impl, pos, TRUE, volume, NULL, NULL, FALSE, NULL);
	  if (success)
	    volume = NULL;
	}
      else
	success = shortcuts_insert_path (impl, pos, FALSE, NULL, impl->current_folder, NULL, FALSE, NULL);

      if (volume)
	gtk_file_system_volume_free (impl->file_system, volume);

      if (base_path)
	gtk_file_path_free (base_path);

      if (!success)
	shortcuts_remove_rows (impl, pos - 1, 1); /* remove the separator */

      impl->shortcuts_current_folder_active = success;
    }

  if (success)
    gtk_combo_box_set_active (GTK_COMBO_BOX (impl->save_folder_combo), pos);
}

/* Updates the current folder row in the shortcuts model */
static void
shortcuts_update_current_folder (GtkFileChooserDefault *impl)
{
  int pos;

  pos = shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

  if (impl->shortcuts_current_folder_active)
    {
      shortcuts_remove_rows (impl, pos, 2);
      impl->shortcuts_current_folder_active = FALSE;
    }

  shortcuts_add_current_folder (impl);
}

/* Filter function used for the shortcuts filter model */
static gboolean
shortcuts_filter_cb (GtkTreeModel          *model,
		     GtkTreeIter           *iter,
		     gpointer               data)
{
  GtkFileChooserDefault *impl;
  GtkTreePath *path;
  int pos;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  path = gtk_tree_model_get_path (model, iter);
  if (!path)
    return FALSE;

  pos = *gtk_tree_path_get_indices (path);
  gtk_tree_path_free (path);

  return (pos < shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR));
}

/* Creates the list model for shortcuts */
static void
shortcuts_model_create (GtkFileChooserDefault *impl)
{
  /* Keep this order in sync with the SHORCUTS_COL_* enum values */
  impl->shortcuts_model = gtk_list_store_new (SHORTCUTS_COL_NUM_COLUMNS,
					      GDK_TYPE_PIXBUF,	/* pixbuf */
					      G_TYPE_STRING,	/* name */
					      G_TYPE_POINTER,	/* path or volume */
					      G_TYPE_BOOLEAN,   /* is the previous column a volume? */
					      G_TYPE_BOOLEAN,   /* removable */
					      G_TYPE_BOOLEAN);  /* pixbuf cell visibility */

  if (impl->file_system)
    {
      shortcuts_append_home (impl);
      shortcuts_append_desktop (impl);
      shortcuts_add_volumes (impl);
      shortcuts_add_bookmarks (impl);
    }

  impl->shortcuts_filter_model = shortcuts_model_filter_new (impl,
							     GTK_TREE_MODEL (impl->shortcuts_model),
							     NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (impl->shortcuts_filter_model),
					  shortcuts_filter_cb,
					  impl,
					  NULL);
}

/* Callback used when the "New Folder" button is clicked */
static void
new_folder_button_clicked (GtkButton             *button,
			   GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  if (!impl->browse_files_model)
    return; /* FIXME: this sucks.  Disable the New Folder button or something. */

  /* Prevent button from being clicked twice */
  gtk_widget_set_sensitive (impl->browse_new_folder_button, FALSE);

  _gtk_file_system_model_add_editable (impl->browse_files_model, &iter);

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->browse_files_model), &iter);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (impl->browse_files_tree_view),
				path, impl->list_name_column,
				FALSE, 0.0, 0.0);

  g_object_set (impl->list_name_renderer, "editable", TRUE, NULL);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view),
			    path,
			    impl->list_name_column,
			    TRUE);

  gtk_tree_path_free (path);
}

/* Idle handler for creating a new folder after editing its name cell, or for
 * canceling the editing.
 */
static gboolean
edited_idle_cb (GtkFileChooserDefault *impl)
{
  GDK_THREADS_ENTER ();
  
  g_source_destroy (impl->edited_idle);
  impl->edited_idle = NULL;

  _gtk_file_system_model_remove_editable (impl->browse_files_model);
  g_object_set (impl->list_name_renderer, "editable", FALSE, NULL);

  gtk_widget_set_sensitive (impl->browse_new_folder_button, TRUE);

  if (impl->edited_new_text) /* not cancelled? */
    {
      GError *error;
      GtkFilePath *file_path;

      error = NULL;
      file_path = gtk_file_system_make_path (impl->file_system, impl->current_folder, impl->edited_new_text,
					     &error);
      if (file_path)
	{
	  error = NULL;
	  if (gtk_file_system_create_folder (impl->file_system, file_path, &error))
	    change_folder_and_display_error (impl, file_path);
	  else
	    error_creating_folder_dialog (impl, file_path, error);

	  gtk_file_path_free (file_path);
	}
      else
	error_creating_folder_dialog (impl, file_path, error);

      g_free (impl->edited_new_text);
      impl->edited_new_text = NULL;
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
queue_edited_idle (GtkFileChooserDefault *impl,
		   const gchar           *new_text)
{
  /* We create the folder in an idle handler so that we don't modify the tree
   * just now.
   */

  g_assert (!impl->edited_idle);
  g_assert (!impl->edited_new_text);

  impl->edited_idle = g_idle_source_new ();
  g_source_set_closure (impl->edited_idle,
			g_cclosure_new_object (G_CALLBACK (edited_idle_cb),
					       G_OBJECT (impl)));
  g_source_attach (impl->edited_idle, NULL);

  if (new_text)
    impl->edited_new_text = g_strdup (new_text);
}

/* Callback used from the text cell renderer when the new folder is named */
static void
renderer_edited_cb (GtkCellRendererText   *cell_renderer_text,
		    const gchar           *path,
		    const gchar           *new_text,
		    GtkFileChooserDefault *impl)
{
 /* work around bug #154921 */
  g_object_set (cell_renderer_text, 
		"mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
   queue_edited_idle (impl, new_text);
}

/* Callback used from the text cell renderer when the new folder edition gets
 * canceled.
 */
static void
renderer_editing_canceled_cb (GtkCellRendererText   *cell_renderer_text,
			      GtkFileChooserDefault *impl)
{
 /* work around bug #154921 */
  g_object_set (cell_renderer_text, 
		"mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
   queue_edited_idle (impl, NULL);
}

/* Creates the widgets for the filter combo box */
static GtkWidget *
filter_create (GtkFileChooserDefault *impl)
{
  impl->filter_combo = gtk_combo_box_new_text ();
  g_signal_connect (impl->filter_combo, "changed",
		    G_CALLBACK (filter_combo_changed), impl);

  return impl->filter_combo;
}

static GtkWidget *
button_new (GtkFileChooserDefault *impl,
	    const char *text,
	    const char *stock_id,
	    gboolean    sensitive,
	    gboolean    show,
	    GCallback   callback)
{
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *widget;
  GtkWidget *align;

  button = gtk_button_new ();
  hbox = gtk_hbox_new (FALSE, 2);
  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

  gtk_container_add (GTK_CONTAINER (button), align);
  gtk_container_add (GTK_CONTAINER (align), hbox);
  widget = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);

  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  widget = gtk_label_new_with_mnemonic (text);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), GTK_WIDGET (button));
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  gtk_widget_set_sensitive (button, sensitive);
  g_signal_connect (button, "clicked", callback, impl);

  gtk_widget_show_all (align);

  if (show)
    gtk_widget_show (button);

  return button;
}

/* Looks for a path among the shortcuts; returns its index or -1 if it doesn't exist */
static int
shortcut_find_position (GtkFileChooserDefault *impl,
			const GtkFilePath     *path)
{
  GtkTreeIter iter;
  int i;
  int current_folder_separator_idx;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
    return -1;

  current_folder_separator_idx = shortcuts_get_index (impl, SHORTCUTS_CURRENT_FOLDER_SEPARATOR);

  for (i = 0; i < current_folder_separator_idx; i++)
    {
      gpointer col_data;
      gboolean is_volume;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_IS_VOLUME, &is_volume,
			  -1);

      if (col_data)
	{
	  if (is_volume)
	    {
	      GtkFileSystemVolume *volume;
	      GtkFilePath *base_path;
	      gboolean exists;

	      volume = col_data;
	      base_path = gtk_file_system_volume_get_base_path (impl->file_system, volume);

	      exists = strcmp (gtk_file_path_get_string (path),
			       gtk_file_path_get_string (base_path)) == 0;
	      g_free (base_path);

	      if (exists)
		return i;
	    }
	  else
	    {
	      GtkFilePath *model_path;

	      model_path = col_data;

	      if (model_path && gtk_file_path_compare (model_path, path) == 0)
		return i;
	    }
	}

      gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter);
    }

  return -1;
}

/* Tries to add a bookmark from a path name */
static gboolean
shortcuts_add_bookmark_from_path (GtkFileChooserDefault *impl,
				  const GtkFilePath     *path,
				  int                    pos)
{
  GError *error;

  if (shortcut_find_position (impl, path) != -1)
    return FALSE;

  /* FIXME: this check really belongs in gtk_file_system_insert_bookmark.  */
  error = NULL;
  if (!check_is_folder (impl->file_system, path, &error))
    {
      error_adding_bookmark_dialog (impl, path, error);
      return FALSE;
    }

  error = NULL;
  if (!gtk_file_system_insert_bookmark (impl->file_system, path, pos, &error))
    {
      error_adding_bookmark_dialog (impl, path, error);
      return FALSE;
    }

  return TRUE;
}

static void
add_bookmark_foreach_cb (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      data)
{
  GtkFileChooserDefault *impl;
  GtkFileSystemModel *fs_model;
  GtkTreeIter child_iter;
  const GtkFilePath *file_path;

  impl = (GtkFileChooserDefault *) data;

  fs_model = impl->browse_files_model;
  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model, &child_iter, iter);

  file_path = _gtk_file_system_model_get_path (fs_model, &child_iter);
  shortcuts_add_bookmark_from_path (impl, file_path, -1);
}

/* Adds a bookmark from the currently selected item in the file list */
static void
bookmarks_add_selected_folder (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

  if (gtk_tree_selection_count_selected_rows (selection) == 0)
    shortcuts_add_bookmark_from_path (impl, impl->current_folder, -1);
  else
    gtk_tree_selection_selected_foreach (selection,
					 add_bookmark_foreach_cb,
					 impl);
}

/* Callback used when the "Add bookmark" button is clicked */
static void
add_bookmark_button_clicked_cb (GtkButton *button,
				GtkFileChooserDefault *impl)
{
  bookmarks_add_selected_folder (impl);
}

/* Returns TRUE plus an iter in the shortcuts_model if a row is selected;
 * returns FALSE if no shortcut is selected.
 */
static gboolean
shortcuts_get_selected (GtkFileChooserDefault *impl,
			GtkTreeIter           *iter)
{
  GtkTreeSelection *selection;
  GtkTreeIter parent_iter;

  if (!impl->browse_shortcuts_tree_view)
    return FALSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));

  if (!gtk_tree_selection_get_selected (selection, NULL, &parent_iter))
    return FALSE;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (impl->shortcuts_filter_model),
						    iter,
						    &parent_iter);
  return TRUE;
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  gpointer col_data;
  gboolean is_volume;
  GtkFilePath *path;
  gboolean removable;
  GError *error;

  if (!shortcuts_get_selected (impl, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_IS_VOLUME, &is_volume,
		      SHORTCUTS_COL_REMOVABLE, &removable,
		      -1);
  g_assert (col_data != NULL);
  g_assert (!is_volume);

  if (!removable)
    return;

  path = col_data;

  error = NULL;
  if (!gtk_file_system_remove_bookmark (impl->file_system, path, &error))
    error_removing_bookmark_dialog (impl, path, error);
}

/* Callback used when the "Remove bookmark" button is clicked */
static void
remove_bookmark_button_clicked_cb (GtkButton *button,
				   GtkFileChooserDefault *impl)
{
  remove_selected_bookmarks (impl);
}

struct selection_check_closure {
  GtkFileChooserDefault *impl;
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
  GtkTreeIter child_iter;
  const GtkFileInfo *info;
  gboolean is_folder;

  closure = data;
  closure->num_selected++;

  gtk_tree_model_sort_convert_iter_to_child_iter (closure->impl->sort_model, &child_iter, iter);

  info = _gtk_file_system_model_get_info (closure->impl->browse_files_model, &child_iter);
  is_folder = info ? gtk_file_info_get_is_folder (info) : FALSE;

  closure->all_folders = closure->all_folders && is_folder;
  closure->all_files = closure->all_files && !is_folder;
}

/* Checks whether the selected items in the file list are all files or all folders */
static void
selection_check (GtkFileChooserDefault *impl,
		 gint                  *num_selected,
		 gboolean              *all_files,
		 gboolean              *all_folders)
{
  struct selection_check_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.num_selected = 0;
  closure.all_files = TRUE;
  closure.all_folders = TRUE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
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

struct get_selected_path_closure {
  GtkFileChooserDefault *impl;
  const GtkFilePath *path;
};

static void
get_selected_path_foreach_cb (GtkTreeModel *model,
			      GtkTreePath  *path,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
  struct get_selected_path_closure *closure;
  GtkTreeIter child_iter;

  closure = data;

  gtk_tree_model_sort_convert_iter_to_child_iter (closure->impl->sort_model, &child_iter, iter);
  closure->path = _gtk_file_system_model_get_path (closure->impl->browse_files_model, &child_iter);
}

/* Returns a selected path from the file list */
static const GtkFilePath *
get_selected_path (GtkFileChooserDefault *impl)
{
  struct get_selected_path_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.path = NULL;

  selection =  gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection,
				       get_selected_path_foreach_cb,
				       &closure);

  return closure.path;
}

typedef struct {
  GtkFileChooserDefault *impl;
  gchar *tip;
} UpdateTooltipData;

static void 
update_tooltip (GtkTreeModel      *model,
		GtkTreePath       *path,
		GtkTreeIter       *iter,
		gpointer           data)
{
  UpdateTooltipData *udata = data;
  GtkTreeIter child_iter;
  const GtkFileInfo *info;

  if (udata->tip == NULL)
    {
      gtk_tree_model_sort_convert_iter_to_child_iter (udata->impl->sort_model,
						      &child_iter,
						      iter);
  
      info = _gtk_file_system_model_get_info (udata->impl->browse_files_model, &child_iter);
      udata->tip = g_strdup_printf (_("Add the folder '%s' to the bookmarks"),
				    gtk_file_info_get_display_name (info));
    }
}


/* Sensitize the "add bookmark" button if all the selected items are folders, or
 * if there are no selected items *and* the current folder is not in the
 * bookmarks list.  De-sensitize the button otherwise.
 */
static void
bookmarks_check_add_sensitivity (GtkFileChooserDefault *impl)
{
  gint num_selected;
  gboolean all_folders;
  gboolean active;
  gchar *tip;

  selection_check (impl, &num_selected, NULL, &all_folders);

  if (num_selected == 0)
    active = (impl->current_folder != NULL) && (shortcut_find_position (impl, impl->current_folder) == -1);
  else if (num_selected == 1)
    {
      const GtkFilePath *path;

      path = get_selected_path (impl);
      active = all_folders && (shortcut_find_position (impl, path) == -1);
    }
  else
    active = all_folders;

  gtk_widget_set_sensitive (impl->browse_shortcuts_add_button, active);

  if (impl->browse_files_popup_menu_add_shortcut_item)
    gtk_widget_set_sensitive (impl->browse_files_popup_menu_add_shortcut_item,
			      (num_selected == 0) ? FALSE : active);

  if (active)
    {
      if (num_selected == 0)
	tip = g_strdup_printf (_("Add the current folder to the bookmarks"));    
      else if (num_selected > 1)
	tip = g_strdup_printf (_("Add the selected folders to the bookmarks"));
      else
	{
	  GtkTreeSelection *selection;
	  UpdateTooltipData data;
	  
	  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
	  data.impl = impl;
	  data.tip = NULL;
	  gtk_tree_selection_selected_foreach (selection, update_tooltip, &data);
	  tip = data.tip;
	  
	}
      gtk_tooltips_set_tip (impl->tooltips, impl->browse_shortcuts_add_button, tip, NULL);
      g_free (tip);
    }
}

/* Sets the sensitivity of the "remove bookmark" button depending on whether a
 * bookmark row is selected in the shortcuts tree.
 */
static void
bookmarks_check_remove_sensitivity (GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  gboolean removable = FALSE;
  gchar *name = NULL;
  
  if (shortcuts_get_selected (impl, &iter))
    gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			SHORTCUTS_COL_REMOVABLE, &removable,
			SHORTCUTS_COL_NAME, &name,
			-1);

  gtk_widget_set_sensitive (impl->browse_shortcuts_remove_button, removable);

  if (removable)
    {
      gchar *tip;

      tip = g_strdup_printf (_("Remove the bookmark '%s'"), name);
      gtk_tooltips_set_tip (impl->tooltips, impl->browse_shortcuts_remove_button,
			    tip, NULL);
      g_free (tip);
    }

  g_free (name);
}

/* GtkWidget::drag-begin handler for the shortcuts list. */
static void
shortcuts_drag_begin_cb (GtkWidget             *widget,
			 GdkDragContext        *context,
			 GtkFileChooserDefault *impl)
{
#if 0
  impl->shortcuts_drag_context = g_object_ref (context);
#endif
}

#if 0
/* Removes the idle handler for outside drags */
static void
shortcuts_cancel_drag_outside_idle (GtkFileChooserDefault *impl)
{
  if (!impl->shortcuts_drag_outside_idle)
    return;

  g_source_destroy (impl->shortcuts_drag_outside_idle);
  impl->shortcuts_drag_outside_idle = NULL;
}
#endif

/* GtkWidget::drag-end handler for the shortcuts list. */
static void
shortcuts_drag_end_cb (GtkWidget             *widget,
		       GdkDragContext        *context,
		       GtkFileChooserDefault *impl)
{
#if 0
  g_object_unref (impl->shortcuts_drag_context);

  shortcuts_cancel_drag_outside_idle (impl);

  if (!impl->shortcuts_drag_outside)
    return;

  gtk_button_clicked (GTK_BUTTON (impl->browse_shortcuts_remove_button));

  impl->shortcuts_drag_outside = FALSE;
#endif
}

/* GtkWidget::drag-data-delete handler for the shortcuts list. */
static void
shortcuts_drag_data_delete_cb (GtkWidget             *widget,
			       GdkDragContext        *context,
			       GtkFileChooserDefault *impl)
{
  g_signal_stop_emission_by_name (widget, "drag-data-delete");
}

#if 0
/* Creates a suitable drag cursor to indicate that the selected bookmark will be
 * deleted or not.
 */
static void
shortcuts_drag_set_delete_cursor (GtkFileChooserDefault *impl,
				  gboolean               delete)
{
  GtkTreeView *tree_view;
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkPixmap *row_pixmap;
  GdkBitmap *mask;
  int row_pixmap_y;
  int cell_y;

  tree_view = GTK_TREE_VIEW (impl->browse_shortcuts_tree_view);

  /* Find the selected path and get its drag pixmap */

  if (!shortcuts_get_selected (impl, &iter))
    g_assert_not_reached ();

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->shortcuts_model), &iter);

  row_pixmap = gtk_tree_view_create_row_drag_icon (tree_view, path);
  gtk_tree_path_free (path);

  mask = NULL;
  row_pixmap_y = 0;

  if (delete)
    {
      GdkPixbuf *pixbuf;

      pixbuf = gtk_widget_render_icon (impl->browse_shortcuts_tree_view,
				       GTK_STOCK_DELETE,
				       GTK_ICON_SIZE_DND,
				       NULL);
      if (pixbuf)
	{
	  GdkPixmap *composite;
	  int row_pixmap_width, row_pixmap_height;
	  int pixbuf_width, pixbuf_height;
	  int composite_width, composite_height;
	  int pixbuf_x, pixbuf_y;
	  GdkGC *gc, *mask_gc;
	  GdkColor color;
	  GdkBitmap *pixbuf_mask;

	  /* Create pixmap and mask for composite image */

	  gdk_drawable_get_size (row_pixmap, &row_pixmap_width, &row_pixmap_height);
	  pixbuf_width = gdk_pixbuf_get_width (pixbuf);
	  pixbuf_height = gdk_pixbuf_get_height (pixbuf);

	  composite_width = MAX (row_pixmap_width, pixbuf_width);
	  composite_height = MAX (row_pixmap_height, pixbuf_height);

	  row_pixmap_y = (composite_height - row_pixmap_height) / 2;

	  if (gtk_widget_get_direction (impl->browse_shortcuts_tree_view) == GTK_TEXT_DIR_RTL)
	    pixbuf_x = 0;
	  else
	    pixbuf_x = composite_width - pixbuf_width;

	  pixbuf_y = (composite_height - pixbuf_height) / 2;

	  composite = gdk_pixmap_new (row_pixmap, composite_width, composite_height, -1);
	  gc = gdk_gc_new (composite);

	  mask = gdk_pixmap_new (row_pixmap, composite_width, composite_height, 1);
	  mask_gc = gdk_gc_new (mask);
	  color.pixel = 0;
	  gdk_gc_set_foreground (mask_gc, &color);
	  gdk_draw_rectangle (mask, mask_gc, TRUE, 0, 0, composite_width, composite_height);

	  color.red = 0xffff;
	  color.green = 0xffff;
	  color.blue = 0xffff;
	  gdk_gc_set_rgb_fg_color (gc, &color);
	  gdk_draw_rectangle (composite, gc, TRUE, 0, 0, composite_width, composite_height);

	  /* Composite the row pixmap and the pixbuf */

	  gdk_pixbuf_render_pixmap_and_mask_for_colormap
	    (pixbuf,
	     gtk_widget_get_colormap (impl->browse_shortcuts_tree_view),
	     NULL, &pixbuf_mask, 128);
	  gdk_draw_drawable (mask, mask_gc, pixbuf_mask,
			     0, 0,
			     pixbuf_x, pixbuf_y,
			     pixbuf_width, pixbuf_height);
	  g_object_unref (pixbuf_mask);

	  gdk_draw_drawable (composite, gc, row_pixmap,
			     0, 0,
			     0, row_pixmap_y,
			     row_pixmap_width, row_pixmap_height);
	  color.pixel = 1;
	  gdk_gc_set_foreground (mask_gc, &color);
	  gdk_draw_rectangle (mask, mask_gc, TRUE, 0, row_pixmap_y, row_pixmap_width, row_pixmap_height);

	  gdk_draw_pixbuf (composite, gc, pixbuf,
			   0, 0,
			   pixbuf_x, pixbuf_y,
			   pixbuf_width, pixbuf_height,
			   GDK_RGB_DITHER_MAX,
			   0, 0);

	  g_object_unref (pixbuf);
	  g_object_unref (row_pixmap);

	  row_pixmap = composite;
	}
    }

  /* The hotspot offsets here are copied from gtk_tree_view_drag_begin(), ugh */

  gtk_tree_view_get_path_at_pos (tree_view,
                                 tree_view->priv->press_start_x,
                                 tree_view->priv->press_start_y,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &cell_y);

  gtk_drag_set_icon_pixmap (impl->shortcuts_drag_context,
			    gdk_drawable_get_colormap (row_pixmap),
			    row_pixmap,
			    mask,
			    tree_view->priv->press_start_x + 1,
			    row_pixmap_y + cell_y + 1);

  g_object_unref (row_pixmap);
  if (mask)
    g_object_unref (mask);
}

/* We set the delete cursor and the shortcuts_drag_outside flag in an idle
 * handler so that we can tell apart the drag_leave event that comes right
 * before a drag_drop, from a normal drag_leave.  We don't want to set the
 * cursor nor the flag in the latter case.
 */
static gboolean
shortcuts_drag_outside_idle_cb (GtkFileChooserDefault *impl)
{
  GDK_THREADS_ENTER ();
  
  shortcuts_drag_set_delete_cursor (impl, TRUE);
  impl->shortcuts_drag_outside = TRUE;

  shortcuts_cancel_drag_outside_idle (impl);

  GDK_THREADS_LEAVE ();

  return FALSE;
}
#endif

/* GtkWidget::drag-leave handler for the shortcuts list.  We unhighlight the
 * drop position.
 */
static void
shortcuts_drag_leave_cb (GtkWidget             *widget,
			 GdkDragContext        *context,
			 guint                  time_,
			 GtkFileChooserDefault *impl)
{
#if 0
  if (gtk_drag_get_source_widget (context) == widget && !impl->shortcuts_drag_outside_idle)
    {
      impl->shortcuts_drag_outside_idle = g_idle_source_new ();
      g_source_set_closure (impl->shortcuts_drag_outside_idle,
			    g_cclosure_new_object (G_CALLBACK (shortcuts_drag_outside_idle_cb),
						   G_OBJECT (impl)));
      g_source_attach (impl->shortcuts_drag_outside_idle, NULL);
    }
#endif

  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
				   NULL,
				   GTK_TREE_VIEW_DROP_BEFORE);

  g_signal_stop_emission_by_name (widget, "drag-leave");
}

/* Computes the appropriate row and position for dropping */
static void
shortcuts_compute_drop_position (GtkFileChooserDefault   *impl,
				 int                      x,
				 int                      y,
				 GtkTreePath            **path,
				 GtkTreeViewDropPosition *pos)
{
  GtkTreeView *tree_view;
  GtkTreeViewColumn *column;
  int cell_y;
  GdkRectangle cell;
  int row;
  int bookmarks_index;

  tree_view = GTK_TREE_VIEW (impl->browse_shortcuts_tree_view);

  bookmarks_index = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS);

  if (!gtk_tree_view_get_path_at_pos (tree_view,
                                      x,
				      y - TREE_VIEW_HEADER_HEIGHT (tree_view),
                                      path,
                                      &column,
                                      NULL,
                                      &cell_y))
    {
      row = bookmarks_index + impl->num_bookmarks - 1;
      *path = gtk_tree_path_new_from_indices (row, -1);
      *pos = GTK_TREE_VIEW_DROP_AFTER;
      return;
    }

  row = *gtk_tree_path_get_indices (*path);
  gtk_tree_view_get_background_area (tree_view, *path, column, &cell);
  gtk_tree_path_free (*path);

  if (row < bookmarks_index)
    {
      row = bookmarks_index;
      *pos = GTK_TREE_VIEW_DROP_BEFORE;
    }
  else if (row > bookmarks_index + impl->num_bookmarks - 1)
    {
      row = bookmarks_index + impl->num_bookmarks - 1;
      *pos = GTK_TREE_VIEW_DROP_AFTER;
    }
  else
    {
      if (cell_y < cell.height / 2)
	*pos = GTK_TREE_VIEW_DROP_BEFORE;
      else
	*pos = GTK_TREE_VIEW_DROP_AFTER;
    }

  *path = gtk_tree_path_new_from_indices (row, -1);
}

/* GtkWidget::drag-motion handler for the shortcuts list.  We basically
 * implement the destination side of DnD by hand, due to limitations in
 * GtkTreeView's DnD API.
 */
static gboolean
shortcuts_drag_motion_cb (GtkWidget             *widget,
			  GdkDragContext        *context,
			  gint                   x,
			  gint                   y,
			  guint                  time_,
			  GtkFileChooserDefault *impl)
{
  GtkTreePath *path;
  GtkTreeViewDropPosition pos;
  GdkDragAction action;

#if 0
  if (gtk_drag_get_source_widget (context) == widget)
    {
      shortcuts_cancel_drag_outside_idle (impl);

      if (impl->shortcuts_drag_outside)
	{
	  shortcuts_drag_set_delete_cursor (impl, FALSE);
	  impl->shortcuts_drag_outside = FALSE;
	}
    }
#endif

  if (context->suggested_action == GDK_ACTION_COPY || (context->actions & GDK_ACTION_COPY) != 0)
    action = GDK_ACTION_COPY;
  else if (context->suggested_action == GDK_ACTION_MOVE || (context->actions & GDK_ACTION_MOVE) != 0)
    action = GDK_ACTION_MOVE;
  else
    {
      action = 0;
      goto out;
    }

  shortcuts_compute_drop_position (impl, x, y, &path, &pos);
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), path, pos);
  gtk_tree_path_free (path);

 out:

  g_signal_stop_emission_by_name (widget, "drag-motion");

  if (action != 0)
    {
      gdk_drag_status (context, action, time_);
      return TRUE;
    }
  else
    return FALSE;
}

/* GtkWidget::drag-drop handler for the shortcuts list. */
static gboolean
shortcuts_drag_drop_cb (GtkWidget             *widget,
			GdkDragContext        *context,
			gint                   x,
			gint                   y,
			guint                  time_,
			GtkFileChooserDefault *impl)
{
#if 0
  shortcuts_cancel_drag_outside_idle (impl);
#endif

  g_signal_stop_emission_by_name (widget, "drag-drop");
  return TRUE;
}

/* Parses a "text/uri-list" string and inserts its URIs as bookmarks */
static void
shortcuts_drop_uris (GtkFileChooserDefault *impl,
		     const char            *data,
		     int                    position)
{
  gchar **uris;
  gint i;

  uris = g_uri_list_extract_uris (data);

  for (i = 0; uris[i]; i++)
    {
      char *uri;
      GtkFilePath *path;

      uri = uris[i];
      path = gtk_file_system_uri_to_path (impl->file_system, uri);

      if (path)
	{
	  if (shortcuts_add_bookmark_from_path (impl, path, position))
	    position++;

	  gtk_file_path_free (path);
	}
      else
	{
	  GError *error;

	  g_set_error (&error,
		       GTK_FILE_CHOOSER_ERROR,
		       GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		       _("Could not add a bookmark for '%s' "
			 "because it is an invalid path name."),
		       uri);
	  error_adding_bookmark_dialog (impl, path, error);
	}
    }

  g_strfreev (uris);
}

/* Reorders the selected bookmark to the specified position */
static void
shortcuts_reorder (GtkFileChooserDefault *impl,
		   int                    new_position)
{
  GtkTreeIter iter;
  gpointer col_data;
  gboolean is_volume;
  GtkTreePath *path;
  int old_position;
  int bookmarks_index;
  const GtkFilePath *file_path;
  GtkFilePath *file_path_copy;
  GError *error;

  /* Get the selected path */

  if (!shortcuts_get_selected (impl, &iter))
    g_assert_not_reached ();

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (impl->shortcuts_model), &iter);
  old_position = *gtk_tree_path_get_indices (path);
  gtk_tree_path_free (path);

  bookmarks_index = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS);
  old_position -= bookmarks_index;
  g_assert (old_position >= 0 && old_position < impl->num_bookmarks);

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_IS_VOLUME, &is_volume,
		      -1);
  g_assert (col_data != NULL);
  g_assert (!is_volume);

  file_path = col_data;
  file_path_copy = gtk_file_path_copy (file_path); /* removal below will free file_path, so we need a copy */

  /* Remove the path from the old position and insert it in the new one */

  if (new_position > old_position)
    new_position--;

  if (old_position == new_position)
    goto out;

  error = NULL;
  if (gtk_file_system_remove_bookmark (impl->file_system, file_path_copy, &error))
    shortcuts_add_bookmark_from_path (impl, file_path_copy, new_position);
  else
    error_adding_bookmark_dialog (impl, file_path_copy, error);

 out:

  gtk_file_path_free (file_path_copy);
}

/* Callback used when we get the drag data for the bookmarks list.  We add the
 * received URIs as bookmarks if they are folders.
 */
static void
shortcuts_drag_data_received_cb (GtkWidget          *widget,
				 GdkDragContext     *context,
				 gint                x,
				 gint                y,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time_,
				 gpointer            data)
{
  GtkFileChooserDefault *impl;
  GtkTreePath *tree_path;
  GtkTreeViewDropPosition tree_pos;
  int position;
  int bookmarks_index;

  impl = GTK_FILE_CHOOSER_DEFAULT (data);

  /* Compute position */

  bookmarks_index = shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS);

  shortcuts_compute_drop_position (impl, x, y, &tree_path, &tree_pos);
  position = *gtk_tree_path_get_indices (tree_path);
  gtk_tree_path_free (tree_path);

  if (tree_pos == GTK_TREE_VIEW_DROP_AFTER)
    position++;

  g_assert (position >= bookmarks_index);
  position -= bookmarks_index;

  if (selection_data->target == gdk_atom_intern ("text/uri-list", FALSE))
    shortcuts_drop_uris (impl, selection_data->data, position);
  else if (selection_data->target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))
    shortcuts_reorder (impl, position);

  g_signal_stop_emission_by_name (widget, "drag-data-received");
}

/* Callback used when the selection in the shortcuts tree changes */
static void
shortcuts_selection_changed_cb (GtkTreeSelection      *selection,
				GtkFileChooserDefault *impl)
{
  bookmarks_check_remove_sensitivity (impl);
}

static gboolean
shortcuts_row_separator_func (GtkTreeModel *model,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
  gint column = GPOINTER_TO_INT (data);
  gchar *text;

  gtk_tree_model_get (model, iter, column, &text, -1);
  
  if (!text)
    return TRUE;

  g_free (text);

  return FALSE;
}

/* Since GtkTreeView has a keybinding attached to '/', we need to catch
 * keypresses before the TreeView gets them.
 */
static gboolean
tree_view_keybinding_cb (GtkWidget             *tree_view,
			 GdkEventKey           *event,
			 GtkFileChooserDefault *impl)
{
  if (event->keyval == GDK_slash &&
      ! (event->state & (~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask ())))
    {
      location_popup_handler (impl, "/");
      return TRUE;
    }
  
  return FALSE;
}


/* Creates the widgets for the shortcuts and bookmarks tree */
static GtkWidget *
shortcuts_list_create (GtkFileChooserDefault *impl)
{
  GtkWidget *swin;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  /* Scrolled window */

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
				       GTK_SHADOW_IN);
  gtk_widget_show (swin);

  /* Tree */

  impl->browse_shortcuts_tree_view = gtk_tree_view_new ();
  g_signal_connect (impl->browse_shortcuts_tree_view, "key-press-event",
		    G_CALLBACK (tree_view_keybinding_cb), impl);
  atk_object_set_name (gtk_widget_get_accessible (impl->browse_shortcuts_tree_view), _("Shortcuts"));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), FALSE);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), impl->shortcuts_filter_model);

  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
					  GDK_BUTTON1_MASK,
					  shortcuts_source_targets,
					  num_shortcuts_source_targets,
					  GDK_ACTION_MOVE);

  gtk_drag_dest_set (impl->browse_shortcuts_tree_view,
		     GTK_DEST_DEFAULT_ALL,
		     shortcuts_dest_targets,
		     num_shortcuts_dest_targets,
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  gtk_tree_selection_set_select_function (selection,
					  shortcuts_select_func,
					  impl, NULL);

  g_signal_connect (selection, "changed",
		    G_CALLBACK (shortcuts_selection_changed_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "row-activated",
		    G_CALLBACK (shortcuts_row_activated_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "key-press-event",
		    G_CALLBACK (shortcuts_key_press_event_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-begin",
		    G_CALLBACK (shortcuts_drag_begin_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-end",
		    G_CALLBACK (shortcuts_drag_end_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-data-delete",
		    G_CALLBACK (shortcuts_drag_data_delete_cb), impl);

  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-leave",
		    G_CALLBACK (shortcuts_drag_leave_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-motion",
		    G_CALLBACK (shortcuts_drag_motion_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-drop",
		    G_CALLBACK (shortcuts_drag_drop_cb), impl);
  g_signal_connect (impl->browse_shortcuts_tree_view, "drag-data-received",
		    G_CALLBACK (shortcuts_drag_data_received_cb), impl);

  gtk_container_add (GTK_CONTAINER (swin), impl->browse_shortcuts_tree_view);
  gtk_widget_show (impl->browse_shortcuts_tree_view);

  /* Column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Folder"));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "pixbuf", SHORTCUTS_COL_PIXBUF,
				       "visible", SHORTCUTS_COL_PIXBUF_VISIBLE,
				       NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "text", SHORTCUTS_COL_NAME,
				       NULL);

  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view),
					shortcuts_row_separator_func,
					GINT_TO_POINTER (SHORTCUTS_COL_NAME),
					NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_shortcuts_tree_view), column);

  return swin;
}

/* Creates the widgets for the shortcuts/bookmarks pane */
static GtkWidget *
shortcuts_pane_create (GtkFileChooserDefault *impl,
		       GtkSizeGroup          *size_group)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *widget;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  /* Shortcuts tree */

  widget = shortcuts_list_create (impl);
  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);

  /* Box for buttons */

  hbox = gtk_hbox_new (TRUE, 6);
  gtk_size_group_add_widget (size_group, hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* Add bookmark button */

  impl->browse_shortcuts_add_button = button_new (impl,
						  _("_Add"),
						  GTK_STOCK_ADD,
						  FALSE,
						  TRUE,
						  G_CALLBACK (add_bookmark_button_clicked_cb));
  gtk_box_pack_start (GTK_BOX (hbox), impl->browse_shortcuts_add_button, TRUE, TRUE, 0);
  gtk_tooltips_set_tip (impl->tooltips, impl->browse_shortcuts_add_button,
                        _("Add the selected folder to the bookmarks"), NULL);

  /* Remove bookmark button */

  impl->browse_shortcuts_remove_button = button_new (impl,
						     _("_Remove"),
						     GTK_STOCK_REMOVE,
						     FALSE,
						     TRUE,
						     G_CALLBACK (remove_bookmark_button_clicked_cb));
  gtk_box_pack_start (GTK_BOX (hbox), impl->browse_shortcuts_remove_button, TRUE, TRUE, 0);
  gtk_tooltips_set_tip (impl->tooltips, impl->browse_shortcuts_remove_button,
                        _("Remove the selected bookmark"), NULL);

  return vbox;
}

/* Handles key press events on the file list, so that we can trap Enter to
 * activate the default button on our own.  Also, checks to see if '/' has been
 * pressed.  See comment by tree_view_keybinding_cb() for more details.
 */
static gboolean
trap_activate_cb (GtkWidget   *widget,
		  GdkEventKey *event,
		  gpointer     data)
{
  GtkFileChooserDefault *impl;

  impl = (GtkFileChooserDefault *) data;
  
  if (event->keyval == GDK_slash &&
      ! (event->state & (~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask ())))
    {
      location_popup_handler (impl, "/");
      return TRUE;
    }

  if ((event->keyval == GDK_Return
       || event->keyval == GDK_ISO_Enter
       || event->keyval == GDK_KP_Enter
       || event->keyval == GDK_space)
      && !(impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
	   impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
    {
      GtkWindow *window;

      window = get_toplevel (widget);
      if (window
	  && widget != window->default_widget
	  && !(widget == window->focus_widget &&
	       (!window->default_widget || !GTK_WIDGET_SENSITIVE (window->default_widget))))
	{
	  gtk_window_activate_default (window);
	  return TRUE;
	}
    }

  return FALSE;
}

/* Callback used when the file list's popup menu is detached */
static void
popup_menu_detach_cb (GtkWidget *attach_widget,
		      GtkMenu   *menu)
{
  GtkFileChooserDefault *impl;

  impl = g_object_get_data (G_OBJECT (attach_widget), "GtkFileChooserDefault");
  g_assert (GTK_IS_FILE_CHOOSER_DEFAULT (impl));

  impl->browse_files_popup_menu = NULL;
  impl->browse_files_popup_menu_add_shortcut_item = NULL;
  impl->browse_files_popup_menu_hidden_files_item = NULL;
}

/* Callback used when the "Add to Shortcuts" menu item is activated */
static void
add_to_shortcuts_cb (GtkMenuItem           *item,
		     GtkFileChooserDefault *impl)
{
  bookmarks_add_selected_folder (impl);
}

/* Callback used when the "Open Location" menu item is activated */
static void
open_location_cb (GtkMenuItem           *item,
		  GtkFileChooserDefault *impl)
{
  location_popup_handler (impl, "");
}

/* Callback used when the "Show Hidden Files" menu item is toggled */
static void
show_hidden_toggled_cb (GtkCheckMenuItem      *item,
			GtkFileChooserDefault *impl)
{
  g_object_set (impl,
		"show-hidden", gtk_check_menu_item_get_active (item),
		NULL);
}

/* Constructs the popup menu for the file list if needed */
static void
file_list_build_popup_menu (GtkFileChooserDefault *impl)
{
  GtkWidget *item;

  if (impl->browse_files_popup_menu)
    return;

  impl->browse_files_popup_menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (impl->browse_files_popup_menu),
			     impl->browse_files_tree_view,
			     popup_menu_detach_cb);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Add to Shortcuts"));
  impl->browse_files_popup_menu_add_shortcut_item = item;
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
  gtk_widget_set_sensitive (item, FALSE);
  g_signal_connect (item, "activate",
		    G_CALLBACK (add_to_shortcuts_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  item = gtk_image_menu_item_new_with_mnemonic (_("Open _Location"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
		    G_CALLBACK (open_location_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);

  item = gtk_check_menu_item_new_with_mnemonic (_("Show _Hidden Files"));
  impl->browse_files_popup_menu_hidden_files_item = item;
  g_signal_connect (item, "toggled",
		    G_CALLBACK (show_hidden_toggled_cb), impl);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (impl->browse_files_popup_menu), item);
}

/* Updates the popup menu for the file list, creating it if necessary */
static void
file_list_update_popup_menu (GtkFileChooserDefault *impl)
{
  file_list_build_popup_menu (impl);

  /* The sensitivity of the Add to Shortcuts item is set in
   * bookmarks_check_add_sensitivity()
   */

  g_signal_handlers_block_by_func (impl->browse_files_popup_menu_hidden_files_item,
				   G_CALLBACK (show_hidden_toggled_cb), impl);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (impl->browse_files_popup_menu_hidden_files_item),
				  impl->show_hidden);
  g_signal_handlers_unblock_by_func (impl->browse_files_popup_menu_hidden_files_item,
				     G_CALLBACK (show_hidden_toggled_cb), impl);
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  GdkScreen *screen = gtk_widget_get_screen (widget);
  GtkRequisition req;
  gint monitor_num;
  GdkRectangle monitor;

  g_return_if_fail (GTK_WIDGET_REALIZED (widget));

  gdk_window_get_origin (widget->window, x, y);

  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  *x += (widget->allocation.width - req.width) / 2;
  *y += (widget->allocation.height - req.height) / 2;

  monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);
  gtk_menu_set_monitor (menu, monitor_num);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  *x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
  *y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));

  *push_in = FALSE;
}

static void
file_list_popup_menu (GtkFileChooserDefault *impl,
		      GdkEventButton        *event)
{
  file_list_update_popup_menu (impl);
  if (event)
    gtk_menu_popup (GTK_MENU (impl->browse_files_popup_menu),
		    NULL, NULL, NULL, NULL,
		    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (impl->browse_files_popup_menu),
		      NULL, NULL,
		      popup_position_func, impl->browse_files_tree_view,
		      0, GDK_CURRENT_TIME);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (impl->browse_files_popup_menu),
				   FALSE);
    }

}

/* Callback used for the GtkWidget::popup-menu signal of the file list */
static gboolean
list_popup_menu_cb (GtkWidget *widget,
		    GtkFileChooserDefault *impl)
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
			    GtkFileChooserDefault *impl)
{
  if (event->button != 3)
    return FALSE;

  file_list_popup_menu (impl, event);
  return TRUE;
}

/* Creates the widgets for the file list */
static GtkWidget *
create_file_list (GtkFileChooserDefault *impl)
{
  GtkWidget *swin;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  /* Scrolled window */

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
				       GTK_SHADOW_IN);

  /* Tree/list view */

  impl->browse_files_tree_view = gtk_tree_view_new ();
  g_object_set_data (G_OBJECT (impl->browse_files_tree_view), "GtkFileChooserDefault", impl);
  atk_object_set_name (gtk_widget_get_accessible (impl->browse_files_tree_view), _("Files"));

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (impl->browse_files_tree_view), TRUE);
  gtk_container_add (GTK_CONTAINER (swin), impl->browse_files_tree_view);
  g_signal_connect (impl->browse_files_tree_view, "row-activated",
		    G_CALLBACK (list_row_activated), impl);
  g_signal_connect (impl->browse_files_tree_view, "key-press-event",
    		    G_CALLBACK (trap_activate_cb), impl);
  g_signal_connect (impl->browse_files_tree_view, "popup-menu",
		    G_CALLBACK (list_popup_menu_cb), impl);
  g_signal_connect (impl->browse_files_tree_view, "button-press-event",
		    G_CALLBACK (list_button_press_event_cb), impl);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_set_select_function (selection,
					  list_select_func,
					  impl, NULL);
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (impl->browse_files_tree_view),
					  GDK_BUTTON1_MASK,
					  file_list_source_targets,
					  num_file_list_source_targets,
					  GDK_ACTION_COPY);

  g_signal_connect (selection, "changed",
		    G_CALLBACK (list_selection_changed), impl);

  /* Filename column */

  impl->list_name_column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (impl->list_name_column, TRUE);
  gtk_tree_view_column_set_resizable (impl->list_name_column, TRUE);
  gtk_tree_view_column_set_title (impl->list_name_column, _("Name"));
  gtk_tree_view_column_set_sort_column_id (impl->list_name_column, FILE_LIST_COL_NAME);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (impl->list_name_column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (impl->list_name_column, renderer,
					   list_icon_data_func, impl, NULL);

  impl->list_name_renderer = gtk_cell_renderer_text_new ();
  g_object_set (impl->list_name_renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
  g_signal_connect (impl->list_name_renderer, "edited",
		    G_CALLBACK (renderer_edited_cb), impl);
  g_signal_connect (impl->list_name_renderer, "editing-canceled",
		    G_CALLBACK (renderer_editing_canceled_cb), impl);
  gtk_tree_view_column_pack_start (impl->list_name_column, impl->list_name_renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (impl->list_name_column, impl->list_name_renderer,
					   list_name_data_func, impl, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_files_tree_view), impl->list_name_column);
#if 0
  /* Size column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Size"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   list_size_data_func, impl, NULL);
  gtk_tree_view_column_set_sort_column_id (column, FILE_LIST_COL_SIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_files_tree_view), column);
#endif
  /* Modification time column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_title (column, _("Modified"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
					   list_mtime_data_func, impl, NULL);
  gtk_tree_view_column_set_sort_column_id (column, FILE_LIST_COL_MTIME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (impl->browse_files_tree_view), column);
  gtk_widget_show_all (swin);

  return swin;
}

static GtkWidget *
create_path_bar (GtkFileChooserDefault *impl)
{
  GtkWidget *path_bar;

  path_bar = g_object_new (GTK_TYPE_PATH_BAR, NULL);
  _gtk_path_bar_set_file_system (GTK_PATH_BAR (path_bar), impl->file_system);

  return path_bar;
}

static void
set_filter_tooltip (GtkWidget *widget, 
		    gpointer   data)
{
  GtkTooltips *tooltips = (GtkTooltips *)data;

  if (GTK_IS_BUTTON (widget))
    gtk_tooltips_set_tip (tooltips, widget,
			  _("Select which types of files are shown"), 
			  NULL);
}

static void
realize_filter_combo (GtkWidget *combo,
		      gpointer   data)
{
  GtkFileChooserDefault *impl = (GtkFileChooserDefault *)data;

  gtk_container_forall (GTK_CONTAINER (combo),
			set_filter_tooltip,
			impl->tooltips);
}

/* Creates the widgets for the files/folders pane */
static GtkWidget *
file_pane_create (GtkFileChooserDefault *impl,
		  GtkSizeGroup          *size_group)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *widget;

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  /* The path bar and 'Create Folder' button */
  hbox = gtk_hbox_new (FALSE, 12);
  gtk_widget_show (hbox);
  impl->browse_path_bar = create_path_bar (impl);
  g_signal_connect (impl->browse_path_bar, "path-clicked", G_CALLBACK (path_bar_clicked), impl);
  gtk_widget_show_all (impl->browse_path_bar);
  gtk_box_pack_start (GTK_BOX (hbox), impl->browse_path_bar, TRUE, TRUE, 0);

  /* Create Folder */
  impl->browse_new_folder_button = gtk_button_new_with_mnemonic (_("Create Fo_lder"));
  g_signal_connect (impl->browse_new_folder_button, "clicked",
		    G_CALLBACK (new_folder_button_clicked), impl);
  gtk_box_pack_end (GTK_BOX (hbox), impl->browse_new_folder_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* Box for lists and preview */

  hbox = gtk_hbox_new (FALSE, PREVIEW_HBOX_SPACING);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  /* File list */

  widget = create_file_list (impl);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);

  /* Preview */

  impl->preview_box = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (hbox), impl->preview_box, FALSE, FALSE, 0);
  /* Don't show preview box initially */

  /* Filter combo */

  impl->filter_combo_hbox = gtk_hbox_new (FALSE, 12);

  widget = filter_create (impl);

  g_signal_connect (widget, "realize",
		    G_CALLBACK (realize_filter_combo), impl);

  gtk_widget_show (widget);
  gtk_box_pack_end (GTK_BOX (impl->filter_combo_hbox), widget, FALSE, FALSE, 0);

  gtk_size_group_add_widget (size_group, impl->filter_combo_hbox);
  gtk_box_pack_end (GTK_BOX (vbox), impl->filter_combo_hbox, FALSE, FALSE, 0);

  return vbox;
}
/* Callback used when the "Browse for more folders" expander is toggled */
static void
expander_changed_cb (GtkExpander           *expander,
		     GParamSpec            *pspec,
		     GtkFileChooserDefault *impl)
{
  update_appearance (impl);
}

/* Callback used when the selection changes in the save folder combo box */
static void
save_folder_combo_changed_cb (GtkComboBox           *combo,
			      GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;

  if (impl->changing_folder)
    return;

  if (gtk_combo_box_get_active_iter (combo, &iter))
    shortcuts_activate_iter (impl, &iter);
}

/* Creates the combo box with the save folders */
static GtkWidget *
save_folder_combo_create (GtkFileChooserDefault *impl)
{
  GtkWidget *combo;
  GtkCellRenderer *cell;

  combo = g_object_new (GTK_TYPE_COMBO_BOX,
			"model", impl->shortcuts_model,
			"focus-on-click", FALSE,
                        NULL);
  gtk_widget_show (combo);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
				  "pixbuf", SHORTCUTS_COL_PIXBUF,
				  "visible", SHORTCUTS_COL_PIXBUF_VISIBLE,
				  "sensitive", SHORTCUTS_COL_PIXBUF_VISIBLE,
				  NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
				  "text", SHORTCUTS_COL_NAME,
				  "sensitive", SHORTCUTS_COL_PIXBUF_VISIBLE,
				  NULL);

  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
					shortcuts_row_separator_func,
					GINT_TO_POINTER (SHORTCUTS_COL_NAME),
					NULL);

  g_signal_connect (combo, "changed",
		    G_CALLBACK (save_folder_combo_changed_cb), impl);

  return combo;
}

/* Creates the widgets specific to Save mode */
static GtkWidget *
save_widgets_create (GtkFileChooserDefault *impl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *widget;
  GtkWidget *alignment;

  vbox = gtk_vbox_new (FALSE, 12);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);

  /* Name entry */

  widget = gtk_label_new_with_mnemonic (_("_Name:"));
  gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), widget,
		    0, 1, 0, 1,
		    GTK_FILL, GTK_FILL,
		    0, 0);
  gtk_widget_show (widget);

  impl->save_file_name_entry = _gtk_file_chooser_entry_new (TRUE);
  _gtk_file_chooser_entry_set_file_system (GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry),
					   impl->file_system);
  gtk_entry_set_width_chars (GTK_ENTRY (impl->save_file_name_entry), 45);
  gtk_entry_set_activates_default (GTK_ENTRY (impl->save_file_name_entry), TRUE);
  gtk_table_attach (GTK_TABLE (table), impl->save_file_name_entry,
		    1, 2, 0, 1,
		    GTK_EXPAND | GTK_FILL, 0,
		    0, 0);
  gtk_widget_show (impl->save_file_name_entry);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), impl->save_file_name_entry);

  /* Folder combo */
  impl->save_folder_label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (impl->save_folder_label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), impl->save_folder_label,
		    0, 1, 1, 2,
		    GTK_FILL, GTK_FILL,
		    0, 0);
  gtk_widget_show (impl->save_folder_label);

  impl->save_folder_combo = save_folder_combo_create (impl);
  gtk_table_attach (GTK_TABLE (table), impl->save_folder_combo,
		    1, 2, 1, 2,
		    GTK_EXPAND | GTK_FILL, GTK_FILL,
		    0, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (impl->save_folder_label), impl->save_folder_combo);

  /* Expander */
  alignment = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

  impl->save_expander = gtk_expander_new_with_mnemonic (_("_Browse for other folders"));
  gtk_container_add (GTK_CONTAINER (alignment), impl->save_expander);
  g_signal_connect (impl->save_expander, "notify::expanded",
		    G_CALLBACK (expander_changed_cb),
		    impl);
  gtk_widget_show_all (alignment);

  return vbox;
}

/* Creates the main hpaned with the widgets shared by Open and Save mode */
static GtkWidget *
browse_widgets_create (GtkFileChooserDefault *impl)
{
  GtkWidget *vbox;
  GtkWidget *hpaned;
  GtkWidget *widget;
  GtkSizeGroup *size_group;

  /* size group is used by the [+][-] buttons and the filter combo */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  vbox = gtk_vbox_new (FALSE, 12);

  /* Paned widget */
  hpaned = gtk_hpaned_new ();
  gtk_widget_show (hpaned);
  gtk_paned_set_position (GTK_PANED (hpaned), 200); /* FIXME: this sucks */
  gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);

  widget = shortcuts_pane_create (impl, size_group);
  gtk_paned_pack1 (GTK_PANED (hpaned), widget, FALSE, FALSE);
  widget = file_pane_create (impl, size_group);
  gtk_paned_pack2 (GTK_PANED (hpaned), widget, TRUE, FALSE);

  g_object_unref (size_group);

  return vbox;
}

static GObject*
gtk_file_chooser_default_constructor (GType                  type,
				      guint                  n_construct_properties,
				      GObjectConstructParam *construct_params)
{
  GtkFileChooserDefault *impl;
  GObject *object;

  object = parent_class->constructor (type,
				      n_construct_properties,
				      construct_params);
  impl = GTK_FILE_CHOOSER_DEFAULT (object);

  g_assert (impl->file_system);

  gtk_widget_push_composite_child ();

  /* Shortcuts model */

  shortcuts_model_create (impl);

  /* Widgets for Save mode */
  impl->save_widgets = save_widgets_create (impl);
  gtk_box_pack_start (GTK_BOX (impl), impl->save_widgets, FALSE, FALSE, 0);

  /* The browse widgets */
  impl->browse_widgets = browse_widgets_create (impl);
  gtk_box_pack_start (GTK_BOX (impl), impl->browse_widgets, TRUE, TRUE, 0);

  /* Alignment to hold extra widget */
  impl->extra_align = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (impl), impl->extra_align, FALSE, FALSE, 0);

  gtk_widget_pop_composite_child ();
  update_appearance (impl);

  return object;
}

/* Sets the extra_widget by packing it in the appropriate place */
static void
set_extra_widget (GtkFileChooserDefault *impl,
		  GtkWidget             *extra_widget)
{
  if (extra_widget)
    {
      g_object_ref (extra_widget);
      /* FIXME: is this right ? */
      gtk_widget_show (extra_widget);
    }

  if (impl->extra_widget)
    {
      gtk_container_remove (GTK_CONTAINER (impl->extra_align), impl->extra_widget);
      g_object_unref (impl->extra_widget);
    }

  impl->extra_widget = extra_widget;
  if (impl->extra_widget)
    {
      gtk_container_add (GTK_CONTAINER (impl->extra_align), impl->extra_widget);
      gtk_widget_show (impl->extra_align);
    }
  else
    gtk_widget_hide (impl->extra_align);
}

static void
set_local_only (GtkFileChooserDefault *impl,
		gboolean               local_only)
{
  if (local_only != impl->local_only)
    {
      impl->local_only = local_only;

      if (impl->shortcuts_model && impl->file_system)
	{
	  shortcuts_add_volumes (impl);
	  shortcuts_add_bookmarks (impl);
	}

      if (local_only &&
	  !gtk_file_system_path_is_local (impl->file_system, impl->current_folder))
	{
	  /* If we are pointing to a non-local folder, make an effort to change
	   * back to a local folder, but it's really up to the app to not cause
	   * such a situation, so we ignore errors.
	   */
	  const gchar *home = g_get_home_dir ();
	  GtkFilePath *home_path;

	  if (home == NULL)
	    return;

	  home_path = gtk_file_system_filename_to_path (impl->file_system, home);

	  _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (impl), home_path, NULL);

	  gtk_file_path_free (home_path);
	}
    }
}

static void
volumes_changed_cb (GtkFileSystem         *file_system,
		    GtkFileChooserDefault *impl)
{
  shortcuts_add_volumes (impl);
}

/* Callback used when the set of bookmarks changes in the file system */
static void
bookmarks_changed_cb (GtkFileSystem         *file_system,
		      GtkFileChooserDefault *impl)
{
  shortcuts_add_bookmarks (impl);

  bookmarks_check_add_sensitivity (impl);
  bookmarks_check_remove_sensitivity (impl);
}

/* Sets the file chooser to multiple selection mode */
static void
set_select_multiple (GtkFileChooserDefault *impl,
		     gboolean               select_multiple,
		     gboolean               property_notify)
{
  GtkTreeSelection *selection;
  GtkSelectionMode mode;

  if (select_multiple == impl->select_multiple)
    return;

  mode = select_multiple ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_BROWSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_set_mode (selection, mode);

  impl->select_multiple = select_multiple;
  g_object_notify (G_OBJECT (impl), "select-multiple");

  check_preview_change (impl);
}

static void
set_file_system_backend (GtkFileChooserDefault *impl,
			 const char *backend)
{
  if (impl->file_system)
    {
      g_signal_handler_disconnect (impl->file_system, impl->volumes_changed_id);
      impl->volumes_changed_id = 0;
      g_signal_handler_disconnect (impl->file_system, impl->bookmarks_changed_id);
      impl->bookmarks_changed_id = 0;
      g_object_unref (impl->file_system);
    }

  impl->file_system = NULL;
  if (backend)
    impl->file_system = _gtk_file_system_create (backend);
  else
    {
      GtkSettings *settings = gtk_settings_get_default ();
      gchar *default_backend = NULL;

      g_object_get (settings, "gtk-file-chooser-backend", &default_backend, NULL);
      if (default_backend)
	{
	  impl->file_system = _gtk_file_system_create (default_backend);
	  g_free (default_backend);
	}
    }

  if (!impl->file_system)
    {
#if defined (G_OS_UNIX)
      impl->file_system = gtk_file_system_unix_new ();
#elif defined (G_OS_WIN32)
      impl->file_system = gtk_file_system_win32_new ();
#else
#error "No default filesystem implementation on the platform"
#endif
    }

  if (impl->file_system)
    {
      impl->volumes_changed_id = g_signal_connect (impl->file_system, "volumes-changed",
						   G_CALLBACK (volumes_changed_cb),
						   impl);
      impl->bookmarks_changed_id = g_signal_connect (impl->file_system, "bookmarks-changed",
						     G_CALLBACK (bookmarks_changed_cb),
						     impl);
    }
}

/* This function is basically a do_all function.
 *
 * It sets the visibility on all the widgets based on the current state, and
 * moves the custom_widget if needed.
 */
static void
update_appearance (GtkFileChooserDefault *impl)
{
  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      const char *text;

      gtk_widget_show (impl->save_widgets);

      if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
	text = _("Save in _folder:");
      else
	text = _("Create in _folder:");

      gtk_label_set_text_with_mnemonic (GTK_LABEL (impl->save_folder_label), text);

      if (gtk_expander_get_expanded (GTK_EXPANDER (impl->save_expander)))
	{
	  gtk_widget_set_sensitive (impl->save_folder_label, FALSE);
	  gtk_widget_set_sensitive (impl->save_folder_combo, FALSE);
	  gtk_widget_show (impl->browse_widgets);
	}
      else
	{
	  gtk_widget_set_sensitive (impl->save_folder_label, TRUE);
	  gtk_widget_set_sensitive (impl->save_folder_combo, TRUE);
	  gtk_widget_hide (impl->browse_widgets);
	}

      gtk_widget_show (impl->browse_new_folder_button);

      if (impl->select_multiple)
	{
	  g_warning ("Save mode cannot be set in conjunction with multiple selection mode.  "
		     "Re-setting to single selection mode.");
	  set_select_multiple (impl, FALSE, TRUE);
	}
    }
  else if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
	   impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      gtk_widget_hide (impl->save_widgets);
      gtk_widget_show (impl->browse_widgets);
    }

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    gtk_widget_hide (impl->browse_new_folder_button);
  else
    gtk_widget_show (impl->browse_new_folder_button);

  gtk_widget_queue_draw (impl->browse_files_tree_view);

  g_signal_emit_by_name (impl, "default-size-changed");
}

static void
gtk_file_chooser_default_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)

{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      {
	GtkFileChooserAction action = g_value_get_enum (value);

	if (action != impl->action)
	  {
	    gtk_file_chooser_default_unselect_all (GTK_FILE_CHOOSER (impl));
	    
	    if (action == GTK_FILE_CHOOSER_ACTION_SAVE && impl->select_multiple)
	      {
		g_warning ("Multiple selection mode is not allowed in Save mode");
		set_select_multiple (impl, FALSE, TRUE);
	      }
	    impl->action = action;
	    update_appearance (impl);
	  }
	
	if (impl->save_file_name_entry)
	  _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry),
					      action);
      }
      break;
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      set_file_system_backend (impl, g_value_get_string (value));
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
      impl->preview_widget_active = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;
    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
      impl->use_preview_label = g_value_get_boolean (value);
      update_preview_widget_visibility (impl);
      break;
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      set_extra_widget (impl, g_value_get_object (value));
      break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      {
	gboolean select_multiple = g_value_get_boolean (value);
	if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE && select_multiple)
	  {
	    g_warning ("Multiple selection mode is not allowed in Save mode");
	    return;
	  }

	set_select_multiple (impl, select_multiple, FALSE);
      }
      break;
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      {
	gboolean show_hidden = g_value_get_boolean (value);
	if (show_hidden != impl->show_hidden)
	  {
	    impl->show_hidden = show_hidden;

	    if (impl->browse_files_model)
	      _gtk_file_system_model_set_show_hidden (impl->browse_files_model, show_hidden);
	  }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_default_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_value_set_enum (value, impl->action);
      break;
    case GTK_FILE_CHOOSER_PROP_FILTER:
      g_value_set_object (value, impl->current_filter);
      break;
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, impl->local_only);
      break;
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
      g_value_set_object (value, impl->preview_widget);
      break;
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
      g_value_set_boolean (value, impl->preview_widget_active);
      break;
    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
      g_value_set_boolean (value, impl->use_preview_label);
      break;
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
      g_value_set_object (value, impl->extra_widget);
      break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, impl->select_multiple);
      break;
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, impl->show_hidden);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (GtkFileChooserDefault *impl,
			GdkScreen             *screen)
{
  if (impl->settings_signal_id)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_for_screen (screen);
      g_signal_handler_disconnect (settings,
				   impl->settings_signal_id);
      impl->settings_signal_id = 0;
    }
}

static void
gtk_file_chooser_default_dispose (GObject *object)
{
  GtkFileChooserDefault *impl = (GtkFileChooserDefault *) object;

  if (impl->extra_widget)
    {
      g_object_unref (impl->extra_widget);
      impl->extra_widget = NULL;
    }

  remove_settings_signal (impl, gtk_widget_get_screen (GTK_WIDGET (impl)));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/* We override show-all since we have internal widgets that
 * shouldn't be shown when you call show_all(), like the filter
 * combo box.
 */
static void
gtk_file_chooser_default_show_all (GtkWidget *widget)
{
  GtkFileChooserDefault *impl = (GtkFileChooserDefault *) widget;

  gtk_widget_show (widget);

  if (impl->extra_widget)
    gtk_widget_show_all (impl->extra_widget);
}

/* Handler for GtkWindow::set-focus; this is where we save the last-focused
 * widget on our toplevel.  See gtk_file_chooser_default_hierarchy_changed()
 */
static void
toplevel_set_focus_cb (GtkWindow             *window,
		       GtkWidget             *focus,
		       GtkFileChooserDefault *impl)
{
  impl->toplevel_last_focus_widget = gtk_window_get_focus (window);
}

/* We monitor the focus widget on our toplevel to be able to know which widget
 * was last focused at the time our "should_respond" method gets called.
 */
static void
gtk_file_chooser_default_hierarchy_changed (GtkWidget *widget,
					    GtkWidget *previous_toplevel)
{
  GtkFileChooserDefault *impl;
  GtkWidget *toplevel;

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  if (previous_toplevel)
    {
      g_assert (impl->toplevel_set_focus_id != 0);
      g_signal_handler_disconnect (previous_toplevel, impl->toplevel_set_focus_id);
      impl->toplevel_set_focus_id = 0;
      impl->toplevel_last_focus_widget = NULL;
    }
  else
    g_assert (impl->toplevel_set_focus_id == 0);

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel))
    {
      impl->toplevel_set_focus_id = g_signal_connect (toplevel, "set-focus",
						      G_CALLBACK (toplevel_set_focus_cb), impl);
      impl->toplevel_last_focus_widget = gtk_window_get_focus (GTK_WINDOW (toplevel));
    }
}

/* Changes the icons wherever it is needed */
static void
change_icon_theme (GtkFileChooserDefault *impl)
{
  GtkSettings *settings;
  gint width, height;

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));

  if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &width, &height))
    impl->icon_size = MAX (width, height);
  else
    impl->icon_size = FALLBACK_ICON_SIZE;

  shortcuts_reload_icons (impl);
  gtk_widget_queue_resize (impl->browse_files_tree_view);
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject               *object,
		    GParamSpec            *pspec,
		    GtkFileChooserDefault *impl)
{
  const char *name;

  name = g_param_spec_get_name (pspec);

  if (strcmp (name, "gtk-icon-theme-name") == 0
      || strcmp (name, "gtk-icon-sizes") == 0)
    change_icon_theme (impl);
}

/* Installs a signal handler for GtkSettings so that we can monitor changes in
 * the icon theme.
 */
static void
check_icon_theme (GtkFileChooserDefault *impl)
{
  GtkSettings *settings;

  if (impl->settings_signal_id)
    return;

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    {
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (impl)));
      impl->settings_signal_id = g_signal_connect (settings, "notify",
						   G_CALLBACK (settings_notify_cb), impl);

      change_icon_theme (impl);
    }
}

static void
gtk_file_chooser_default_style_set (GtkWidget *widget,
				    GtkStyle  *previous_style)
{
  GtkFileChooserDefault *impl;

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);

  if (gtk_widget_has_screen (GTK_WIDGET (impl)))
    change_icon_theme (impl);

  g_signal_emit_by_name (widget, "default-size-changed");
}

static void
gtk_file_chooser_default_screen_changed (GtkWidget *widget,
					 GdkScreen *previous_screen)
{
  GtkFileChooserDefault *impl;

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  if (GTK_WIDGET_CLASS (parent_class)->screen_changed)
    GTK_WIDGET_CLASS (parent_class)->screen_changed (widget, previous_screen);

  remove_settings_signal (impl, previous_screen);
  check_icon_theme (impl);

  g_signal_emit_by_name (widget, "default-size-changed");
}

static gboolean
get_is_file_filtered (GtkFileChooserDefault *impl,
		      const GtkFilePath     *path,
		      GtkFileInfo           *file_info)
{
  GtkFileFilterInfo filter_info;
  GtkFileFilterFlags needed;
  gboolean result;

  if (!impl->current_filter)
    return FALSE;

  filter_info.contains = GTK_FILE_FILTER_DISPLAY_NAME | GTK_FILE_FILTER_MIME_TYPE;

  needed = gtk_file_filter_get_needed (impl->current_filter);

  filter_info.display_name = gtk_file_info_get_display_name (file_info);
  filter_info.mime_type = gtk_file_info_get_mime_type (file_info);

  if (needed & GTK_FILE_FILTER_FILENAME)
    {
      filter_info.filename = gtk_file_system_path_to_filename (impl->file_system, path);
      if (filter_info.filename)
	filter_info.contains |= GTK_FILE_FILTER_FILENAME;
    }
  else
    filter_info.filename = NULL;

  if (needed & GTK_FILE_FILTER_URI)
    {
      filter_info.uri = gtk_file_system_path_to_uri (impl->file_system, path);
      if (filter_info.uri)
	filter_info.contains |= GTK_FILE_FILTER_URI;
    }
  else
    filter_info.uri = NULL;

  result = gtk_file_filter_filter (impl->current_filter, &filter_info);

  if (filter_info.filename)
    g_free ((gchar *)filter_info.filename);
  if (filter_info.uri)
    g_free ((gchar *)filter_info.uri);

  return !result;
}

/* GtkWidget::map method */
static void
gtk_file_chooser_default_map (GtkWidget *widget)
{
  GtkFileChooserDefault *impl;

  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  GTK_WIDGET_CLASS (parent_class)->map (widget);

  if (impl->current_folder)
    {
      pending_select_paths_store_selection (impl);
      change_folder_and_display_error (impl, impl->current_folder);
    }

  bookmarks_changed_cb (impl->file_system, impl);
}

static gboolean
list_model_filter_func (GtkFileSystemModel *model,
			GtkFilePath        *path,
			const GtkFileInfo  *file_info,
			gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;

  if (!impl->current_filter)
    return TRUE;

  if (gtk_file_info_get_is_folder (file_info))
    return TRUE;

  return !get_is_file_filtered (impl, path, (GtkFileInfo *) file_info);
}

static void
install_list_model_filter (GtkFileChooserDefault *impl)
{
  GtkFileSystemModelFilter filter;
  gpointer data;

  g_assert (impl->browse_files_model != NULL);

  if (impl->current_filter)
    {
      filter = list_model_filter_func;
      data   = impl;
    }
  else
    {
      filter = NULL;
      data   = NULL;
    }
  
  _gtk_file_system_model_set_filter (impl->browse_files_model,
				     filter,
				     data);
}

#define COMPARE_DIRECTORIES										       \
  GtkFileChooserDefault *impl = user_data;								       \
  const GtkFileInfo *info_a = _gtk_file_system_model_get_info (impl->browse_files_model, a);			       \
  const GtkFileInfo *info_b = _gtk_file_system_model_get_info (impl->browse_files_model, b);			       \
  gboolean dir_a, dir_b;										       \
													       \
  if (info_a)												       \
    dir_a = gtk_file_info_get_is_folder (info_a);							       \
  else													       \
    return impl->list_sort_ascending ? -1 : 1;								       \
													       \
  if (info_b)												       \
    dir_b = gtk_file_info_get_is_folder (info_b);							       \
  else													       \
    return impl->list_sort_ascending ? 1 : -1;  							       \
													       \
  if (dir_a != dir_b)											       \
    return impl->list_sort_ascending ? (dir_a ? -1 : 1) : (dir_a ? 1 : -1) /* Directories *always* go first */

/* Sort callback for the filename column */
static gint
name_sort_func (GtkTreeModel *model,
		GtkTreeIter  *a,
		GtkTreeIter  *b,
		gpointer      user_data)
{
  COMPARE_DIRECTORIES;
  else
    return strcmp (gtk_file_info_get_display_key (info_a), gtk_file_info_get_display_key (info_b));
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
      gint64 size_a = gtk_file_info_get_size (info_a);
      gint64 size_b = gtk_file_info_get_size (info_b);

      return size_a > size_b ? -1 : (size_a == size_b ? 0 : 1);
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
      GtkFileTime ta = gtk_file_info_get_modification_time (info_a);
      GtkFileTime tb = gtk_file_info_get_modification_time (info_b);

      return ta > tb ? -1 : (ta == tb ? 0 : 1);
    }
}

/* Callback used when the sort column changes.  We cache the sort order for use
 * in name_sort_func().
 */
static void
list_sort_column_changed_cb (GtkTreeSortable       *sortable,
			     GtkFileChooserDefault *impl)
{
  GtkSortType sort_type;

  if (gtk_tree_sortable_get_sort_column_id (sortable, NULL, &sort_type))
    impl->list_sort_ascending = (sort_type == GTK_SORT_ASCENDING);
}

static void
set_busy_cursor (GtkFileChooserDefault *impl,
		 gboolean               busy)
{
  GtkWindow *toplevel;
  GdkDisplay *display;
  GdkCursor *cursor;

  toplevel = get_toplevel (GTK_WIDGET (impl));
  if (!toplevel || !GTK_WIDGET_REALIZED (toplevel))
    return;

  display = gtk_widget_get_display (GTK_WIDGET (toplevel));

  if (busy)
    cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
  else
    cursor = NULL;

  gdk_window_set_cursor (GTK_WIDGET (toplevel)->window, cursor);
  gdk_display_flush (display);

  if (cursor)
    gdk_cursor_unref (cursor);
}

/* Creates a sort model to wrap the file system model and sets it on the tree view */
static void
load_set_model (GtkFileChooserDefault *impl)
{
  g_assert (impl->browse_files_model != NULL);
  g_assert (impl->sort_model == NULL);

  impl->sort_model = (GtkTreeModelSort *)gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (impl->browse_files_model));
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_NAME, name_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_SIZE, size_sort_func, impl, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_MTIME, mtime_sort_func, impl, NULL);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (impl->sort_model), NULL, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (impl->sort_model), FILE_LIST_COL_NAME, GTK_SORT_ASCENDING);
  impl->list_sort_ascending = TRUE;

  g_signal_connect (impl->sort_model, "sort-column-changed",
		    G_CALLBACK (list_sort_column_changed_cb), impl);

  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view),
			   GTK_TREE_MODEL (impl->sort_model));
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (impl->browse_files_tree_view),
				   GTK_FILE_SYSTEM_MODEL_DISPLAY_NAME);
}

/* Timeout callback used when the loading timer expires */
static gboolean
load_timeout_cb (gpointer data)
{
  GtkFileChooserDefault *impl;

  GDK_THREADS_ENTER ();

  impl = GTK_FILE_CHOOSER_DEFAULT (data);
  g_assert (impl->load_state == LOAD_PRELOAD);
  g_assert (impl->load_timeout_id != 0);
  g_assert (impl->browse_files_model != NULL);

  impl->load_timeout_id = 0;
  impl->load_state = LOAD_LOADING;

  load_set_model (impl);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

/* Sets up a new load timer for the model and switches to the LOAD_LOADING state */
static void
load_setup_timer (GtkFileChooserDefault *impl)
{
  g_assert (impl->load_timeout_id == 0);
  g_assert (impl->load_state != LOAD_PRELOAD);

  impl->load_timeout_id = g_timeout_add (MAX_LOADING_TIME, load_timeout_cb, impl);
  impl->load_state = LOAD_PRELOAD;
}

/* Removes the load timeout and switches to the LOAD_FINISHED state */
static void
load_remove_timer (GtkFileChooserDefault *impl)
{
  if (impl->load_timeout_id != 0)
    {
      g_assert (impl->load_state == LOAD_PRELOAD);

      g_source_remove (impl->load_timeout_id);
      impl->load_timeout_id = 0;
      impl->load_state = LOAD_EMPTY;
    }
  else
    g_assert (impl->load_state == LOAD_EMPTY ||
	      impl->load_state == LOAD_LOADING ||
	      impl->load_state == LOAD_FINISHED);
}

/* Selects the first row in the file list */
static void
browse_files_select_first_row (GtkFileChooserDefault *impl)
{
  GtkTreePath *path;

  if (!impl->sort_model)
    return;

  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view), path, NULL, FALSE);
  gtk_tree_path_free (path);
}

struct center_selected_row_closure {
  GtkFileChooserDefault *impl;
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

  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (closure->impl->browse_files_tree_view), path, NULL, TRUE, 0.5, 0.0);
  closure->already_centered = TRUE;
}

/* Centers the selected row in the tree view */
static void
browse_files_center_selected_row (GtkFileChooserDefault *impl)
{
  struct center_selected_row_closure closure;
  GtkTreeSelection *selection;

  closure.impl = impl;
  closure.already_centered = FALSE;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, center_selected_row_foreach_cb, &closure);
}

static gboolean
show_and_select_paths (GtkFileChooserDefault *impl,
		       const GtkFilePath     *parent_path,
		       const GtkFilePath     *only_one_path,
		       GSList                *paths,
		       GError               **error)
{
  GtkFileFolder *folder;
  gboolean success;
  gboolean have_hidden;
  gboolean have_filtered;

  if (!only_one_path && !paths)
    return TRUE;

  folder = gtk_file_system_get_folder (impl->file_system, parent_path, GTK_FILE_INFO_IS_HIDDEN, error);
  if (!folder)
    return FALSE;

  success = FALSE;
  have_hidden = FALSE;
  have_filtered = FALSE;

  if (only_one_path)
    {
      GtkFileInfo *info;

      info = gtk_file_folder_get_info (folder, only_one_path, error);
      if (info)
	{
	  success = TRUE;
	  have_hidden = gtk_file_info_get_is_hidden (info);
	  have_filtered = get_is_file_filtered (impl, only_one_path, info);
	  gtk_file_info_free (info);
	}
    }
  else
    {
      GSList *l;

      for (l = paths; l; l = l->next)
	{
	  const GtkFilePath *path;
	  GtkFileInfo *info;

	  path = l->data;

	  /* NULL GError */
	  info = gtk_file_folder_get_info (folder, path, NULL);
	  if (info)
	    {
	      if (!have_hidden)
		have_hidden = gtk_file_info_get_is_hidden (info);

	      if (!have_filtered)
		have_filtered = get_is_file_filtered (impl, path, info);

	      gtk_file_info_free (info);

	      if (have_hidden && have_filtered)
		break; /* we now have all the information we need */
	    }
	}

      success = TRUE;
    }

  g_object_unref (folder);

  if (!success)
    return FALSE;

  if (have_hidden)
    g_object_set (impl, "show-hidden", TRUE, NULL);

  if (have_filtered)
    set_current_filter (impl, NULL);

  if (only_one_path)
    _gtk_file_system_model_path_do (impl->browse_files_model, only_one_path, select_func, impl);
  else
    {
      GSList *l;

      for (l = paths; l; l = l->next)
	{
	  const GtkFilePath *path;

	  path = l->data;
	  _gtk_file_system_model_path_do (impl->browse_files_model, path, select_func, impl);
	}
    }

  return TRUE;
}

/* Processes the pending operation when a folder is finished loading */
static void
pending_select_paths_process (GtkFileChooserDefault *impl)
{
  g_assert (impl->load_state == LOAD_FINISHED);
  g_assert (impl->browse_files_model != NULL);
  g_assert (impl->sort_model != NULL);

  if (impl->pending_select_paths)
    {
      /* NULL GError */
      show_and_select_paths (impl, impl->current_folder, NULL, impl->pending_select_paths, NULL);
      pending_select_paths_free (impl);
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
       *
       * Also, we don't select the first file if we are in SAVE or CREATE_FOLDER
       * modes.  Doing so would change the contents of the filename entry.
       */
      if (GTK_WIDGET_MAPPED (impl)
	  && !(impl->action == GTK_FILE_CHOOSER_ACTION_SAVE || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
	browse_files_select_first_row (impl);
    }

  g_assert (impl->pending_select_paths == NULL);
}

/* Callback used when the file system model finishes loading */
static void
browse_files_model_finished_loading_cb (GtkFileSystemModel    *model,
					GtkFileChooserDefault *impl)
{
  if (impl->load_state == LOAD_PRELOAD)
    {
      load_remove_timer (impl);
      load_set_model (impl);
    }
  else if (impl->load_state == LOAD_LOADING)
    {
      /* Nothing */
    }
  else
    {
      /* We can't g_assert_not_reached(), as something other than us may have
       *  initiated a folder reload.  See #165556.
       */
      return;
    }

  g_assert (impl->load_timeout_id == 0);

  impl->load_state = LOAD_FINISHED;

  pending_select_paths_process (impl);
  set_busy_cursor (impl, FALSE);
}

/* Gets rid of the old list model and creates a new one for the current folder */
static gboolean
set_list_model (GtkFileChooserDefault *impl,
		GError               **error)
{
  g_assert (impl->current_folder != NULL);

  load_remove_timer (impl); /* This changes the state to LOAD_EMPTY */

  if (impl->browse_files_model)
    {
      g_object_unref (impl->browse_files_model);
      impl->browse_files_model = NULL;
    }

  if (impl->sort_model)
    {
      g_object_unref (impl->sort_model);
      impl->sort_model = NULL;
    }

  set_busy_cursor (impl, TRUE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (impl->browse_files_tree_view), NULL);

  impl->browse_files_model = _gtk_file_system_model_new (impl->file_system,
							 impl->current_folder, 0,
							 GTK_FILE_INFO_ALL,
							 error);
  if (!impl->browse_files_model)
    {
      set_busy_cursor (impl, FALSE);
      return FALSE;
    }

  load_setup_timer (impl); /* This changes the state to LOAD_PRELOAD */

  g_signal_connect (impl->browse_files_model, "finished-loading",
		    G_CALLBACK (browse_files_model_finished_loading_cb), impl);

  _gtk_file_system_model_set_show_hidden (impl->browse_files_model, impl->show_hidden);

  install_list_model_filter (impl);

  return TRUE;
}

static void
update_chooser_entry (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  const GtkFileInfo *info;
  GtkTreeIter iter;
  GtkTreeIter child_iter;

  if (impl->action != GTK_FILE_CHOOSER_ACTION_SAVE)
    return;

  g_assert (!impl->select_multiple);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  &iter);

  info = _gtk_file_system_model_get_info (impl->browse_files_model, &child_iter);

  if (!gtk_file_info_get_is_folder (info))
    _gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry),
					   gtk_file_info_get_display_name (info));
}

static gboolean
gtk_file_chooser_default_set_current_folder (GtkFileChooser    *chooser,
					     const GtkFilePath *path,
					     GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  gboolean result;

  g_assert (path != NULL);

  if (impl->local_only &&
      !gtk_file_system_path_is_local (impl->file_system, path))
    {
      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		   _("Cannot change to folder because it is not local"));

      return FALSE;
    }

  /* Test validity of path here.  */
  if (!check_is_folder (impl->file_system, path, error))
    return FALSE;

  if (!_gtk_path_bar_set_path (GTK_PATH_BAR (impl->browse_path_bar), path, error))
    return FALSE;

  if (impl->current_folder != path)
    {
      if (impl->current_folder)
	gtk_file_path_free (impl->current_folder);

      impl->current_folder = gtk_file_path_copy (path);
    }

  /* Update the widgets that may trigger a folder change themselves.  */

  if (!impl->changing_folder)
    {
      impl->changing_folder = TRUE;

      shortcuts_update_current_folder (impl);

      impl->changing_folder = FALSE;
    }

  /* Set the folder on the save entry */

  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry),
					   impl->current_folder);

  /* Create a new list model.  This is slightly evil; we store the result value
   * but perform more actions rather than returning immediately even if it
   * generates an error.
   */
  result = set_list_model (impl, error);

  /* Refresh controls */

  shortcuts_find_current_folder (impl);

  g_signal_emit_by_name (impl, "current-folder-changed", 0);

  check_preview_change (impl);
  bookmarks_check_add_sensitivity (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);

  return result;
}

static GtkFilePath *
gtk_file_chooser_default_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return gtk_file_path_copy (impl->current_folder);
}

static void
gtk_file_chooser_default_set_current_name (GtkFileChooser *chooser,
					   const gchar    *name)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  g_return_if_fail (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
		    || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

  _gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry), name);
}

static void
select_func (GtkFileSystemModel *model,
	     GtkTreePath        *path,
	     GtkTreeIter        *iter,
	     gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkTreeSelection *selection;
  GtkTreeIter sorted_iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

  gtk_tree_model_sort_convert_child_iter_to_iter (impl->sort_model, &sorted_iter, iter);
  gtk_tree_selection_select_iter (selection, &sorted_iter);
}

static gboolean
gtk_file_chooser_default_select_path (GtkFileChooser    *chooser,
				      const GtkFilePath *path,
				      GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkFilePath *parent_path;
  gboolean same_path;

  if (!gtk_file_system_get_parent (impl->file_system, path, &parent_path, error))
    return FALSE;

  if (!parent_path)
    return _gtk_file_chooser_set_current_folder_path (chooser, path, error);

  if (impl->load_state == LOAD_EMPTY)
    same_path = FALSE;
  else
    {
      g_assert (impl->current_folder != NULL);

      same_path = gtk_file_path_compare (parent_path, impl->current_folder) == 0;
    }

  if (same_path && impl->load_state == LOAD_FINISHED)
    {
      gboolean result;

      result = show_and_select_paths (impl, parent_path, path, NULL, error);
      gtk_file_path_free (parent_path);
      return result;
    }

  pending_select_paths_add (impl, path);

  if (!same_path)
    {
      gboolean result;

      result = _gtk_file_chooser_set_current_folder_path (chooser, parent_path, error);
      gtk_file_path_free (parent_path);
      return result;
    }

  gtk_file_path_free (parent_path);
  return TRUE;
}

static void
unselect_func (GtkFileSystemModel *model,
	       GtkTreePath        *path,
	       GtkTreeIter        *iter,
	       gpointer            user_data)
{
  GtkFileChooserDefault *impl = user_data;
  GtkTreeView *tree_view = GTK_TREE_VIEW (impl->browse_files_tree_view);
  GtkTreePath *sorted_path;

  sorted_path = gtk_tree_model_sort_convert_child_path_to_path (impl->sort_model,
								path);
  gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (tree_view),
				    sorted_path);
  gtk_tree_path_free (sorted_path);
}

static void
gtk_file_chooser_default_unselect_path (GtkFileChooser    *chooser,
					const GtkFilePath *path)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  if (!impl->browse_files_model)
    return;

  _gtk_file_system_model_path_do (impl->browse_files_model, path,
				  unselect_func, impl);
}

static gboolean
maybe_select (GtkTreeModel *model, 
	      GtkTreePath  *path, 
	      GtkTreeIter  *iter, 
	      gpointer     data)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (data);
  GtkTreeSelection *selection;
  const GtkFileInfo *info;
  gboolean is_folder;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  
  info = get_list_file_info (impl, iter);
  is_folder = gtk_file_info_get_is_folder (info);

  if ((is_folder && impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ||
      (!is_folder && impl->action == GTK_FILE_CHOOSER_ACTION_OPEN))
    gtk_tree_selection_select_iter (selection, iter);
  else
    gtk_tree_selection_unselect_iter (selection, iter);
    
  return FALSE;
}

static void
gtk_file_chooser_default_select_all (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  if (impl->select_multiple)
    gtk_tree_model_foreach (GTK_TREE_MODEL (impl->sort_model), 
			    maybe_select, impl);
}

static void
gtk_file_chooser_default_unselect_all (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));

  gtk_tree_selection_unselect_all (selection);
}

/* Checks whether the filename entry for the Save modes contains a valid filename */
static GtkFilePath *
check_save_entry (GtkFileChooserDefault *impl,
		  gboolean              *is_valid,
		  gboolean              *is_empty)
{
  GtkFileChooserEntry *chooser_entry;
  const GtkFilePath *current_folder;
  const char *file_part;
  GtkFilePath *path;
  GError *error;

  g_assert (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	    || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

  chooser_entry = GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry);

  current_folder = _gtk_file_chooser_entry_get_current_folder (chooser_entry);
  file_part = _gtk_file_chooser_entry_get_file_part (chooser_entry);

  if (!file_part || file_part[0] == '\0')
    {
      *is_valid = FALSE;
      *is_empty = TRUE;
      return NULL;
    }

  *is_empty = FALSE;

  error = NULL;
  path = gtk_file_system_make_path (impl->file_system, current_folder, file_part, &error);

  if (!path)
    {
      error_building_filename_dialog (impl, current_folder, file_part, error);
      *is_valid = FALSE;
      return NULL;
    }

  *is_valid = TRUE;
  return path;
}

struct get_paths_closure {
  GtkFileChooserDefault *impl;
  GSList *result;
  GtkFilePath *path_from_entry;
};

static void
get_paths_foreach (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer      data)
{
  struct get_paths_closure *info;
  const GtkFilePath *file_path;
  GtkFileSystemModel *fs_model;
  GtkTreeIter sel_iter;

  info = data;
  fs_model = info->impl->browse_files_model;
  gtk_tree_model_sort_convert_iter_to_child_iter (info->impl->sort_model, &sel_iter, iter);

  file_path = _gtk_file_system_model_get_path (fs_model, &sel_iter);
  if (!file_path)
    return; /* We are on the editable row */

  if (!info->path_from_entry
      || gtk_file_path_compare (info->path_from_entry, file_path) != 0)
    info->result = g_slist_prepend (info->result, gtk_file_path_copy (file_path));
}

static GSList *
gtk_file_chooser_default_get_paths (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  struct get_paths_closure info;

  info.impl = impl;
  info.result = NULL;
  info.path_from_entry = NULL;

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
      || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      gboolean is_valid, is_empty;

      info.path_from_entry = check_save_entry (impl, &is_valid, &is_empty);
      if (!is_valid && !is_empty)
	return NULL;
    }

  if (!info.path_from_entry || impl->select_multiple)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
      gtk_tree_selection_selected_foreach (selection, get_paths_foreach, &info);
    }

  if (info.path_from_entry)
    info.result = g_slist_prepend (info.result, info.path_from_entry);

  /* If there's no folder selected, and we're in SELECT_FOLDER mode, then we
   * fall back to the current directory */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER &&
      info.result == NULL)
    {
      info.result = g_slist_prepend (info.result, gtk_file_path_copy (impl->current_folder));
    }

  return g_slist_reverse (info.result);
}

static GtkFilePath *
gtk_file_chooser_default_get_preview_path (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  if (impl->preview_path)
    return gtk_file_path_copy (impl->preview_path);
  else
    return NULL;
}

static GtkFileSystem *
gtk_file_chooser_default_get_file_system (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return impl->file_system;
}

/* Shows or hides the filter widgets */
static void
show_filters (GtkFileChooserDefault *impl,
	      gboolean               show)
{
  if (show)
    gtk_widget_show (impl->filter_combo_hbox);
  else
    gtk_widget_hide (impl->filter_combo_hbox);
}

static void
gtk_file_chooser_default_add_filter (GtkFileChooser *chooser,
				     GtkFileFilter  *filter)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  const gchar *name;

  if (g_slist_find (impl->filters, filter))
    {
      g_warning ("gtk_file_chooser_add_filter() called on filter already in list\n");
      return;
    }

  g_object_ref (filter);
  gtk_object_sink (GTK_OBJECT (filter));
  impl->filters = g_slist_append (impl->filters, filter);

  name = gtk_file_filter_get_name (filter);
  if (!name)
    name = "Untitled filter";	/* Place-holder, doesn't need to be marked for translation */

  gtk_combo_box_append_text (GTK_COMBO_BOX (impl->filter_combo), name);

  if (!g_slist_find (impl->filters, impl->current_filter))
    set_current_filter (impl, filter);

  show_filters (impl, TRUE);
}

static void
gtk_file_chooser_default_remove_filter (GtkFileChooser *chooser,
					GtkFileFilter  *filter)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint filter_index;

  filter_index = g_slist_index (impl->filters, filter);

  if (filter_index < 0)
    {
      g_warning ("gtk_file_chooser_remove_filter() called on filter not in list\n");
      return;
    }

  impl->filters = g_slist_remove (impl->filters, filter);

  if (filter == impl->current_filter)
    {
      if (impl->filters)
	set_current_filter (impl, impl->filters->data);
      else
	set_current_filter (impl, NULL);
    }

  /* Remove row from the combo box */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (impl->filter_combo));
  gtk_tree_model_iter_nth_child  (model, &iter, NULL, filter_index);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  g_object_unref (filter);

  if (!impl->filters)
    show_filters (impl, FALSE);
}

static GSList *
gtk_file_chooser_default_list_filters (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);

  return g_slist_copy (impl->filters);
}

/* Returns the position in the shortcuts tree where the nth specified shortcut would appear */
static int
shortcuts_get_pos_for_shortcut_folder (GtkFileChooserDefault *impl,
				       int                    pos)
{
  return pos + shortcuts_get_index (impl, SHORTCUTS_SHORTCUTS);
}

static gboolean
gtk_file_chooser_default_add_shortcut_folder (GtkFileChooser    *chooser,
					      const GtkFilePath *path,
					      GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  gboolean result;
  int pos;

  /* Test validity of path here.  */
  if (!check_is_folder (impl->file_system, path, error))
    return FALSE;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, impl->num_shortcuts);

  result = shortcuts_insert_path (impl, pos, FALSE, NULL, path, NULL, FALSE, error);

  if (result)
    impl->num_shortcuts++;

  if (impl->shortcuts_filter_model)
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (impl->shortcuts_filter_model));

  return result;
}

static gboolean
gtk_file_chooser_default_remove_shortcut_folder (GtkFileChooser    *chooser,
						 const GtkFilePath *path,
						 GError           **error)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  int pos;
  GtkTreeIter iter;
  char *uri;
  int i;

  if (impl->num_shortcuts == 0)
    goto out;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, 0);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  for (i = 0; i < impl->num_shortcuts; i++)
    {
      gpointer col_data;
      gboolean is_volume;
      GtkFilePath *shortcut;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_IS_VOLUME, &is_volume,
			  -1);
      g_assert (col_data != NULL);
      g_assert (!is_volume);

      shortcut = col_data;
      if (gtk_file_path_compare (shortcut, path) == 0)
	{
	  shortcuts_remove_rows (impl, pos + i, 1);
	  impl->num_shortcuts--;
	  return TRUE;
	}

      if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	g_assert_not_reached ();
    }

 out:

  uri = gtk_file_system_path_to_uri (impl->file_system, path);
  g_set_error (error,
	       GTK_FILE_CHOOSER_ERROR,
	       GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
	       _("Shortcut %s does not exist"),
	       uri);
  g_free (uri);

  return FALSE;
}

static GSList *
gtk_file_chooser_default_list_shortcut_folders (GtkFileChooser *chooser)
{
  GtkFileChooserDefault *impl = GTK_FILE_CHOOSER_DEFAULT (chooser);
  int pos;
  GtkTreeIter iter;
  int i;
  GSList *list;

  if (impl->num_shortcuts == 0)
    return NULL;

  pos = shortcuts_get_pos_for_shortcut_folder (impl, 0);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  list = NULL;

  for (i = 0; i < impl->num_shortcuts; i++)
    {
      gpointer col_data;
      gboolean is_volume;
      GtkFilePath *shortcut;

      gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), &iter,
			  SHORTCUTS_COL_DATA, &col_data,
			  SHORTCUTS_COL_IS_VOLUME, &is_volume,
			  -1);
      g_assert (col_data != NULL);
      g_assert (!is_volume);

      shortcut = col_data;
      list = g_slist_prepend (list, gtk_file_path_copy (shortcut));

      if (i != impl->num_shortcuts - 1)
	{
	  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (impl->shortcuts_model), &iter))
	    g_assert_not_reached ();
	}
    }

  return g_slist_reverse (list);
}

/* Guesses a size based upon font sizes */
static void
find_good_size_from_style (GtkWidget *widget,
			   gint      *width,
			   gint      *height)
{
  GtkFileChooserDefault *impl;
  gint default_width, default_height;
  int font_size;
  GtkRequisition req;
  GtkRequisition preview_req;

  g_assert (widget->style != NULL);
  impl = GTK_FILE_CHOOSER_DEFAULT (widget);

  font_size = pango_font_description_get_size (widget->style->font_desc);
  font_size = PANGO_PIXELS (font_size);

  default_width = font_size * NUM_CHARS;
  default_height = font_size * NUM_LINES;

  /* Use at least the requisition size not including the preview widget */
  gtk_widget_size_request (widget, &req);

  if (impl->preview_widget_active && impl->preview_widget)
    gtk_widget_size_request (impl->preview_box, &preview_req);
  else
    preview_req.width = 0;

  default_width = MAX (default_width, (req.width - (preview_req.width + PREVIEW_HBOX_SPACING)));
  default_height = MAX (default_height, req.height);

  *width = default_width;
  *height = default_height;
}

static void
gtk_file_chooser_default_get_default_size (GtkFileChooserEmbed *chooser_embed,
					   gint                *default_width,
					   gint                *default_height)
{
  GtkFileChooserDefault *impl;

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  find_good_size_from_style (GTK_WIDGET (chooser_embed), default_width, default_height);

  if (impl->preview_widget_active && impl->preview_widget)
    *default_width += impl->preview_box->requisition.width + PREVIEW_HBOX_SPACING;
}

static void
gtk_file_chooser_default_get_resizable_hints (GtkFileChooserEmbed *chooser_embed,
					      gboolean            *resize_horizontally,
					      gboolean            *resize_vertically)
{
  GtkFileChooserDefault *impl;

  g_return_if_fail (resize_horizontally != NULL);
  g_return_if_fail (resize_vertically != NULL);

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  *resize_horizontally = TRUE;
  *resize_vertically = TRUE;

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      if (! gtk_expander_get_expanded (GTK_EXPANDER (impl->save_expander)))
	{
	  *resize_horizontally = FALSE;
	  *resize_vertically = FALSE;
	}
    }
}

struct switch_folder_closure {
  GtkFileChooserDefault *impl;
  const GtkFilePath *path;
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
  GtkTreeIter child_iter;

  closure = data;

  gtk_tree_model_sort_convert_iter_to_child_iter (closure->impl->sort_model, &child_iter, iter);

  closure->path = _gtk_file_system_model_get_path (closure->impl->browse_files_model, &child_iter);
  closure->num_selected++;
}

/* Changes to the selected folder in the list view */
static void
switch_to_selected_folder (GtkFileChooserDefault *impl)
{
  GtkTreeSelection *selection;
  struct switch_folder_closure closure;

  /* We do this with foreach() rather than get_selected() as we may be in
   * multiple selection mode
   */

  closure.impl = impl;
  closure.path = NULL;
  closure.num_selected = 0;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
  gtk_tree_selection_selected_foreach (selection, switch_folder_foreach_cb, &closure);

  g_assert (closure.path && closure.num_selected == 1);

  change_folder_and_display_error (impl, closure.path);
}

/* Implementation for GtkFileChooserEmbed::should_respond() */
static gboolean
gtk_file_chooser_default_should_respond (GtkFileChooserEmbed *chooser_embed)
{
  GtkFileChooserDefault *impl;
  GtkWidget *toplevel;
  GtkWidget *current_focus;

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (impl));
  g_assert (GTK_IS_WINDOW (toplevel));

  current_focus = gtk_window_get_focus (GTK_WINDOW (toplevel));

  if (current_focus == impl->browse_files_tree_view)
    {
      int num_selected;
      gboolean all_files, all_folders;

    file_list:

      selection_check (impl, &num_selected, &all_files, &all_folders);

      if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	{
	  if (num_selected != 1) 	 
	    return TRUE; /* zero means current folder; more than one means use the whole selection */ 	 
	  else if (current_focus != impl->browse_files_tree_view) 	 
	    {
	      /* a single folder is selected and a button was clicked */
	      switch_to_selected_folder (impl); 	 
	      return TRUE;
	    }
	}

      if (num_selected == 0)
	{
	  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	      || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    goto save_entry; /* it makes sense to use the typed name */
	  else
	    return FALSE;
	}

      if (num_selected == 1 && all_folders)
	{
	  switch_to_selected_folder (impl);
	  return FALSE;
	}
      else
	return all_files;
    }
  else if (current_focus == impl->save_file_name_entry)
    {
      GtkFilePath *path;
      gboolean is_valid, is_empty;
      gboolean is_folder;
      gboolean retval;
      GtkFileChooserEntry *entry;  

    save_entry:

      g_assert (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
		|| impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

      entry = GTK_FILE_CHOOSER_ENTRY (impl->save_file_name_entry);
      path = check_save_entry (impl, &is_valid, &is_empty);

      if (!is_empty && !is_valid)
	return FALSE;

      if (is_empty)
	path = gtk_file_path_copy (_gtk_file_chooser_entry_get_current_folder (entry));
      
      is_folder = check_is_folder (impl->file_system, path, NULL);
      if (is_folder)
	{
	  _gtk_file_chooser_entry_set_file_part (entry, "");
	  change_folder_and_display_error (impl, path);
	  retval = FALSE;
	}
      else
	{
	  /* check that everything up to the last component exists */
	  gtk_file_path_free (path);
	  path = gtk_file_path_copy (_gtk_file_chooser_entry_get_current_folder (entry));
	  is_folder = check_is_folder (impl->file_system, path, NULL);
	  if (!is_folder)
	    {
	      change_folder_and_display_error (impl, path);
	      retval = FALSE;
	    }
	  else
	    retval = TRUE;
	}

      gtk_file_path_free (path);
      return retval;
    }
  else if (impl->toplevel_last_focus_widget == impl->browse_shortcuts_tree_view)
    {
      /* The focus is on a dialog's action area button, *and* the widget that
       * was focused immediately before it is the shortcuts list.  Switch to the
       * selected shortcut and tell the caller not to respond.
       */
      GtkTreeIter iter;

      if (shortcuts_get_selected (impl, &iter))
	{
	  shortcuts_activate_iter (impl, &iter);
	  
	  gtk_widget_grab_focus (impl->browse_files_tree_view);
	}
      else
	goto file_list;

      return FALSE;
    }
  else if (impl->toplevel_last_focus_widget == impl->browse_files_tree_view)
    {
      /* The focus is on a dialog's action area button, *and* the widget that
       * was focused immediately before it is the file list.  
       */
      goto file_list;
    }
  else
    /* The focus is on a dialog's action area button or something else */
    if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	|| impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
      goto save_entry;
    else
      goto file_list; 
  
  g_assert_not_reached ();
  return FALSE;
}

/* Implementation for GtkFileChooserEmbed::initial_focus() */
static void
gtk_file_chooser_default_initial_focus (GtkFileChooserEmbed *chooser_embed)
{
  GtkFileChooserDefault *impl;
  GtkWidget *widget;

  impl = GTK_FILE_CHOOSER_DEFAULT (chooser_embed);

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
      || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    widget = impl->browse_files_tree_view;
  else if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	   || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    widget = impl->save_file_name_entry;
  else
    {
      g_assert_not_reached ();
      widget = NULL;
    }

  gtk_widget_grab_focus (widget);
}

static void
set_current_filter (GtkFileChooserDefault *impl,
		    GtkFileFilter         *filter)
{
  if (impl->current_filter != filter)
    {
      int filter_index;

      /* NULL filters are allowed to reset to non-filtered status
       */
      filter_index = g_slist_index (impl->filters, filter);
      if (impl->filters && filter && filter_index < 0)
	return;

      if (impl->current_filter)
	g_object_unref (impl->current_filter);
      impl->current_filter = filter;
      if (impl->current_filter)
	{
	  g_object_ref (impl->current_filter);
	  gtk_object_sink (GTK_OBJECT (filter));
	}

      if (impl->filters)
	gtk_combo_box_set_active (GTK_COMBO_BOX (impl->filter_combo),
				  filter_index);

      if (impl->browse_files_model)
	install_list_model_filter (impl);

      g_object_notify (G_OBJECT (impl), "filter");
    }
}

static void
filter_combo_changed (GtkComboBox           *combo_box,
		      GtkFileChooserDefault *impl)
{
  gint new_index = gtk_combo_box_get_active (combo_box);
  GtkFileFilter *new_filter = g_slist_nth_data (impl->filters, new_index);

  set_current_filter (impl, new_filter);
}

static void
check_preview_change (GtkFileChooserDefault *impl)
{
  GtkTreePath *cursor_path;
  const GtkFilePath *new_path;
  const GtkFileInfo *new_info;

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (impl->browse_files_tree_view), &cursor_path, NULL);
  if (cursor_path && impl->sort_model)
    {
      GtkTreeIter iter;
      GtkTreeIter child_iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->sort_model), &iter, cursor_path);
      gtk_tree_path_free (cursor_path);

      gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model, &child_iter, &iter);

      new_path = _gtk_file_system_model_get_path (impl->browse_files_model, &child_iter);
      new_info = _gtk_file_system_model_get_info (impl->browse_files_model, &child_iter);
    }
  else
    {
      new_path = NULL;
      new_info = NULL;
    }

  if (new_path != impl->preview_path &&
      !(new_path && impl->preview_path &&
	gtk_file_path_compare (new_path, impl->preview_path) == 0))
    {
      if (impl->preview_path)
	{
	  gtk_file_path_free (impl->preview_path);
	  g_free (impl->preview_display_name);
	}

      if (new_path)
	{
	  impl->preview_path = gtk_file_path_copy (new_path);
	  impl->preview_display_name = g_strdup (gtk_file_info_get_display_name (new_info));
	}
      else
	{
	  impl->preview_path = NULL;
	  impl->preview_display_name = NULL;
	}

      if (impl->use_preview_label && impl->preview_label)
	gtk_label_set_text (GTK_LABEL (impl->preview_label), impl->preview_display_name);

      g_signal_emit_by_name (impl, "update-preview");
    }
}

/* Activates a volume by mounting it if necessary and then switching to its
 * base path.
 */
static void
shortcuts_activate_volume (GtkFileChooserDefault *impl,
			   GtkFileSystemVolume   *volume)
{
  GtkFilePath *path;

  /* We ref the file chooser since volume_mount() may run a main loop, and the
   * user could close the file chooser window in the meantime.
   */
  g_object_ref (impl);

  if (!gtk_file_system_volume_get_is_mounted (impl->file_system, volume))
    {
      GError *error;
      gboolean result;

      set_busy_cursor (impl, TRUE);

      error = NULL;
      result = gtk_file_system_volume_mount (impl->file_system, volume, &error);

      if (!result)
	{
	  char *msg;

	  msg = g_strdup_printf (_("Could not mount %s"),
				 gtk_file_system_volume_get_display_name (impl->file_system, volume));
	  error_message (impl, msg, error->message);
	  g_free (msg);
	  g_error_free (error);
	}

      set_busy_cursor (impl, FALSE);

      if (!result)
	goto out;
    }

  path = gtk_file_system_volume_get_base_path (impl->file_system, volume);
  change_folder_and_display_error (impl, path);
  gtk_file_path_free (path);

 out:

  g_object_unref (impl);
}

/* Opens the folder or volume at the specified iter in the shortcuts model */
static void
shortcuts_activate_iter (GtkFileChooserDefault *impl,
			 GtkTreeIter           *iter)
{
  gpointer col_data;
  gboolean is_volume;

  gtk_tree_model_get (GTK_TREE_MODEL (impl->shortcuts_model), iter,
		      SHORTCUTS_COL_DATA, &col_data,
		      SHORTCUTS_COL_IS_VOLUME, &is_volume,
		      -1);

  if (!col_data)
    return; /* We are on a separator */

  if (is_volume)
    {
      GtkFileSystemVolume *volume;

      volume = col_data;

      shortcuts_activate_volume (impl, volume);
    }
  else
    {
      const GtkFilePath *file_path;

      file_path = col_data;
      change_folder_and_display_error (impl, file_path);
    }
}

/* Callback used when a row in the shortcuts list is activated */
static void
shortcuts_row_activated_cb (GtkTreeView           *tree_view,
			    GtkTreePath           *path,
			    GtkTreeViewColumn     *column,
			    GtkFileChooserDefault *impl)
{
  GtkTreeIter iter;
  GtkTreeIter child_iter;

  if (!gtk_tree_model_get_iter (impl->shortcuts_filter_model, &iter, path))
    return;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (impl->shortcuts_filter_model),
						    &child_iter,
						    &iter);
  shortcuts_activate_iter (impl, &child_iter);

  gtk_widget_grab_focus (impl->browse_files_tree_view);
}

/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
shortcuts_key_press_event_cb (GtkWidget             *widget,
			      GdkEventKey           *event,
			      GtkFileChooserDefault *impl)
{
  guint modifiers;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  if ((event->keyval == GDK_BackSpace
      || event->keyval == GDK_Delete
      || event->keyval == GDK_KP_Delete)
      && (event->state & modifiers) == 0)
    {
      remove_selected_bookmarks (impl);
      return TRUE;
    }

  return FALSE;
}

static gboolean
shortcuts_select_func  (GtkTreeSelection  *selection,
			GtkTreeModel      *model,
			GtkTreePath       *path,
			gboolean           path_currently_selected,
			gpointer           data)
{
  GtkFileChooserDefault *impl = data;

  return (*gtk_tree_path_get_indices (path) != shortcuts_get_index (impl, SHORTCUTS_BOOKMARKS_SEPARATOR));
}

static gboolean
list_select_func  (GtkTreeSelection  *selection,
		   GtkTreeModel      *model,
		   GtkTreePath       *path,
		   gboolean           path_currently_selected,
		   gpointer           data)
{
  GtkFileChooserDefault *impl = data;

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      GtkTreeIter iter, child_iter;
      const GtkFileInfo *info;

      if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->sort_model), &iter, path))
	return FALSE;
      
      gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model, &child_iter, &iter);

      info = _gtk_file_system_model_get_info (impl->browse_files_model, &child_iter);

      if (info && !gtk_file_info_get_is_folder (info))
	return FALSE;
    }
    
  return TRUE;
}

static void
list_selection_changed (GtkTreeSelection      *selection,
			GtkFileChooserDefault *impl)
{
  /* See if we are in the new folder editable row for Save mode */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      GtkTreeSelection *selection;
      GtkTreeIter iter, child_iter;
      const GtkFileInfo *info;

      g_assert (!impl->select_multiple);
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (impl->browse_files_tree_view));
      if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
	return;

      gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						      &child_iter,
						      &iter);

      info = _gtk_file_system_model_get_info (impl->browse_files_model, &child_iter);
      if (!info)
	return; /* We are on the editable row for New Folder */
    }

  update_chooser_entry (impl);
  check_preview_change (impl);
  bookmarks_check_add_sensitivity (impl);

  g_signal_emit_by_name (impl, "selection-changed", 0);
}

/* Callback used when a row in the file list is activated */
static void
list_row_activated (GtkTreeView           *tree_view,
		    GtkTreePath           *path,
		    GtkTreeViewColumn     *column,
		    GtkFileChooserDefault *impl)
{
  GtkTreeIter iter, child_iter;
  const GtkFileInfo *info;

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (impl->sort_model), &iter, path))
    return;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model, &child_iter, &iter);

  info = _gtk_file_system_model_get_info (impl->browse_files_model, &child_iter);

  if (gtk_file_info_get_is_folder (info))
    {
      const GtkFilePath *file_path;

      file_path = _gtk_file_system_model_get_path (impl->browse_files_model, &child_iter);
      change_folder_and_display_error (impl, file_path);

      return;
    }

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN ||
      impl->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    g_signal_emit_by_name (impl, "file-activated");
}

static void
path_bar_clicked (GtkPathBar            *path_bar,
		  GtkFilePath           *file_path,
		  gboolean               child_is_hidden,
		  GtkFileChooserDefault *impl)
{
  if (!change_folder_and_display_error (impl, file_path))
    return;

  /* Say we have "/foo/bar/[.baz]" and the user clicks on "bar".  We should then
   * show hidden files so that ".baz" appears in the file list, as it will still
   * be shown in the path bar: "/foo/[bar]/.baz"
   */
  if (child_is_hidden)
    g_object_set (impl, "show-hidden", TRUE, NULL);
}

static const GtkFileInfo *
get_list_file_info (GtkFileChooserDefault *impl,
		    GtkTreeIter           *iter)
{
  GtkTreeIter child_iter;

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  iter);

  return _gtk_file_system_model_get_info (impl->browse_files_model, &child_iter);
}

static void
list_icon_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  GtkTreeIter child_iter;
  const GtkFilePath *path;
  GdkPixbuf *pixbuf;
  const GtkFileInfo *info; 
  gboolean sensitive = TRUE;
  
  info = get_list_file_info (impl, iter);

  gtk_tree_model_sort_convert_iter_to_child_iter (impl->sort_model,
						  &child_iter,
						  iter);
  path = _gtk_file_system_model_get_path (impl->browse_files_model, &child_iter);

  if (path)
    {
      /* FIXME: NULL GError */
      pixbuf = gtk_file_system_render_icon (impl->file_system, path, GTK_WIDGET (impl),
					    impl->icon_size, NULL);
    }
  else
    {
      /* We are on the editable row */
      pixbuf = NULL;
    }

  if (info && (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
	       impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER))
    sensitive =  gtk_file_info_get_is_folder (info);    
    
  g_object_set (cell,
		"pixbuf", pixbuf,
		"sensitive", sensitive,
		NULL);
    
  if (pixbuf)
    g_object_unref (pixbuf);
}

static void
list_name_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  const GtkFileInfo *info = get_list_file_info (impl, iter);
  gboolean sensitive = TRUE;

  if (!info)
    {
      g_object_set (cell,
		    "text", _("Type name of new folder"),
		    NULL);

      return;
    }


  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
	 || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      sensitive = gtk_file_info_get_is_folder (info);
    } 
    
  g_object_set (cell,
		"text", gtk_file_info_get_display_name (info),
		"sensitive", sensitive,
		NULL);
}

#if 0
static void
list_size_data_func (GtkTreeViewColumn *tree_column,
		     GtkCellRenderer   *cell,
		     GtkTreeModel      *tree_model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
  GtkFileChooserDefault *impl = data;
  const GtkFileInfo *info = get_list_file_info (impl, iter);
  gint64 size;
  gchar *str;
  gboolean sensitive = TRUE;

  if (!info || gtk_file_info_get_is_folder (info)) 
    {
      g_object_set (cell,"sensitive", sensitive, NULL);
      return;
    }

  size = gtk_file_info_get_size (info);

  if (size < (gint64)1024)
    str = g_strdup_printf (ngettext ("%d byte", "%d bytes", (gint)size), (gint)size);
  else if (size < (gint64)1024*1024)
    str = g_strdup_printf (_("%.1f K"), size / (1024.));
  else if (size < (gint64)1024*1024*1024)
    str = g_strdup_printf (_("%.1f M"), size / (1024.*1024.));
  else
    str = g_strdup_printf (_("%.1f G"), size / (1024.*1024.*1024.));

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    sensitive = FALSE;

  g_object_set (cell,
  		"text", str,
		"sensitive", sensitive,
		NULL);

  g_free (str);
}
#endif

/* Tree column data callback for the file list; fetches the mtime of a file */
static void
list_mtime_data_func (GtkTreeViewColumn *tree_column,
		      GtkCellRenderer   *cell,
		      GtkTreeModel      *tree_model,
		      GtkTreeIter       *iter,
		      gpointer           data)
{
  GtkFileChooserDefault *impl;
  const GtkFileInfo *info;
  GtkFileTime time_mtime, time_now;
  GDate mtime, now;
  int days_diff;
  char buf[256];
  gboolean sensitive = TRUE;

  impl = data;

  info = get_list_file_info (impl, iter);
  if (!info)
    {
      g_object_set (cell,
		    "text", "",
		    "sensitive", TRUE,
		    NULL);
      return;
    }

  time_mtime = gtk_file_info_get_modification_time (info);
  g_date_set_time (&mtime, (GTime) time_mtime);

  time_now = (GTime ) time (NULL);
  g_date_set_time (&now, (GTime) time_now);

  days_diff = g_date_get_julian (&now) - g_date_get_julian (&mtime);

  if (days_diff == 0)
    strcpy (buf, _("Today"));
  else if (days_diff == 1)
    strcpy (buf, _("Yesterday"));
  else
    {
      char *format;

      if (days_diff > 1 && days_diff < 7)
	format = "%A"; /* Days from last week */
      else
	format = "%x"; /* Any other date */

      if (g_date_strftime (buf, sizeof (buf), format, &mtime) == 0)
	strcpy (buf, _("Unknown"));
    }

  if (impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
      impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    sensitive = gtk_file_info_get_is_folder (info);

  g_object_set (cell,
		"text", buf,
		"sensitive", sensitive,
		NULL);
}

GtkWidget *
_gtk_file_chooser_default_new (const char *file_system)
{
  return  g_object_new (GTK_TYPE_FILE_CHOOSER_DEFAULT,
			"file-system-backend", file_system,
			NULL);
}

static GtkWidget *
location_entry_create (GtkFileChooserDefault *impl,
		       const gchar           *path)
{
  GtkWidget *entry;

  entry = _gtk_file_chooser_entry_new (TRUE);
  /* Pick a good width for the entry */
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 30);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  _gtk_file_chooser_entry_set_file_system (GTK_FILE_CHOOSER_ENTRY (entry), impl->file_system);
  _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (entry), impl->action);
  if (path[0])
    {
      _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (entry), 
					       gtk_file_path_new_steal ((gchar *)path));
      _gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (entry), path);
    }
  else
    {
      _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (entry), impl->current_folder);
      if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
	  || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	_gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (entry), "");
      else if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	       || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	_gtk_file_chooser_entry_set_file_part (GTK_FILE_CHOOSER_ENTRY (entry),
					       gtk_entry_get_text (GTK_ENTRY (impl->save_file_name_entry)));
      else
	g_assert_not_reached ();
    }

  return GTK_WIDGET (entry);
}

static gboolean
update_from_entry (GtkFileChooserDefault *impl,
		   GtkWindow             *parent,
		   GtkFileChooserEntry   *chooser_entry)
{
  const GtkFilePath *folder_path;
  const char *file_part;

  folder_path = _gtk_file_chooser_entry_get_current_folder (chooser_entry);
  file_part = _gtk_file_chooser_entry_get_file_part (chooser_entry);

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN && !folder_path)
    {
      error_message_with_parent (parent,
				 _("Cannot change folder"),
				 _("The folder you specified is an invalid path."));
      return FALSE;
    }

  if (file_part[0] == '\0')
    return change_folder_and_display_error (impl, folder_path);
  else
    {
      GtkFileFolder *folder = NULL;
      GtkFilePath *subfolder_path = NULL;
      GtkFileInfo *info = NULL;
      GError *error;
      gboolean result;

      result = FALSE;

      /* If the file part is non-empty, we need to figure out if it refers to a
       * folder within folder. We could optimize the case here where the folder
       * is already loaded for one of our tree models.
       */

      error = NULL;
      folder = gtk_file_system_get_folder (impl->file_system, folder_path, GTK_FILE_INFO_IS_FOLDER, &error);

      if (!folder)
	{
	  error_getting_info_dialog (impl, folder_path, error);
	  goto out;
	}

      error = NULL;
      subfolder_path = gtk_file_system_make_path (impl->file_system, folder_path, file_part, &error);

      if (!subfolder_path)
	{
	  char *msg;
	  char *uri;

	  uri = gtk_file_system_path_to_uri (impl->file_system, folder_path);
	  msg = g_strdup_printf (_("Could not build file name from '%s' and '%s'"),
				 uri, file_part);
	  error_message (impl, msg, error->message);
	  g_free (uri);
	  g_free (msg);
	  goto out;
	}

      error = NULL;
      info = gtk_file_folder_get_info (folder, subfolder_path, &error);

      if (!info)
	{
	  if (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
	      || impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	    {
	      if (!change_folder_and_display_error (impl, folder_path))
		goto out;

	      gtk_file_chooser_default_set_current_name (GTK_FILE_CHOOSER (impl), file_part);
	    }
	  else
	    error_getting_info_dialog (impl, subfolder_path, error);

	  goto out;
	}

      if (gtk_file_info_get_is_folder (info))
	result = change_folder_and_display_error (impl, subfolder_path);
      else
	{
	  GError *error;

	  error = NULL;
	  result = _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (impl), subfolder_path, &error);
	  if (!result)
	    error_dialog (impl, _("Could not select item"),
			  subfolder_path, error);
	}

    out:

      if (folder)
	g_object_unref (folder);

      gtk_file_path_free (subfolder_path);

      if (info)
	gtk_file_info_free (info);

      return result;
    }

  g_assert_not_reached ();
}

static void
location_popup_handler (GtkFileChooserDefault *impl,
			const gchar           *path)
{
  GtkWidget *dialog;
  GtkWindow *toplevel;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  gboolean refocus;
  const char *title;
  const char *accept_stock;

  /* Create dialog */

  toplevel = get_toplevel (GTK_WIDGET (impl));

  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
      || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      title = _("Open Location");
      accept_stock = GTK_STOCK_OPEN;
    }
  else
    {
      g_assert (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
		|| impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);
      title = _("Save in Location");
      accept_stock = GTK_STOCK_SAVE;
    }

  dialog = gtk_dialog_new_with_buttons (title,
					toplevel,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					accept_stock, GTK_RESPONSE_ACCEPT,
					NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
					   GTK_RESPONSE_ACCEPT,
					   GTK_RESPONSE_CANCEL,
					   -1);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);

  label = gtk_label_new_with_mnemonic (_("_Location:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = location_entry_create (impl, path);

  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

  /* Run */

  gtk_widget_show_all (dialog);
  /* If the dialog is brought up by typing the first characters
   * of a path, unselect the text in the entry, so that you can
   * just type on without erasing the initial part.
   */
  if (path[0])
    gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);

  refocus = TRUE;

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      if (update_from_entry (impl, GTK_WINDOW (dialog), GTK_FILE_CHOOSER_ENTRY (entry)))
	{
	  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN
	      || impl->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	    {
	      gtk_widget_grab_focus (impl->browse_files_tree_view);
	    }
	  else
	    {
	      g_assert (impl->action == GTK_FILE_CHOOSER_ACTION_SAVE
			|| impl->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);
	      gtk_widget_grab_focus (impl->save_file_name_entry);
	    }
	  refocus = FALSE;
	}
    }

  if (refocus)
    {
      GtkWindow *toplevel;

      toplevel = get_toplevel (GTK_WIDGET (impl));
      if (toplevel && toplevel->focus_widget)
	gtk_widget_grab_focus (toplevel->focus_widget);
    }

  gtk_widget_destroy (dialog);
}

/* Handler for the "up-folder" keybinding signal */
static void
up_folder_handler (GtkFileChooserDefault *impl)
{
  pending_select_paths_add (impl, impl->current_folder);
  _gtk_path_bar_up (GTK_PATH_BAR (impl->browse_path_bar));
}

/* Handler for the "down-folder" keybinding signal */
static void
down_folder_handler (GtkFileChooserDefault *impl)
{
  _gtk_path_bar_down (GTK_PATH_BAR (impl->browse_path_bar));
}

/* Handler for the "home-folder" keybinding signal */
static void
home_folder_handler (GtkFileChooserDefault *impl)
{
  int pos;
  GtkTreeIter iter;

  if (!impl->has_home)
    return; /* Should we put up an error dialog? */

  pos = shortcuts_get_index (impl, SHORTCUTS_HOME);
  if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (impl->shortcuts_model), &iter, NULL, pos))
    g_assert_not_reached ();

  shortcuts_activate_iter (impl, &iter);
}



/* Drag and drop interfaces */

static void
_shortcuts_model_filter_class_init (ShortcutsModelFilterClass *class)
{
}

static void
_shortcuts_model_filter_init (ShortcutsModelFilter *model)
{
  model->impl = NULL;
}

/* GtkTreeDragSource::row_draggable implementation for the shortcuts filter model */
static gboolean
shortcuts_model_filter_row_draggable (GtkTreeDragSource *drag_source,
				      GtkTreePath       *path)
{
  ShortcutsModelFilter *model;
  int pos;
  int bookmarks_pos;

  model = SHORTCUTS_MODEL_FILTER (drag_source);

  pos = *gtk_tree_path_get_indices (path);
  bookmarks_pos = shortcuts_get_index (model->impl, SHORTCUTS_BOOKMARKS);

  return (pos >= bookmarks_pos && pos < bookmarks_pos + model->impl->num_bookmarks);
}

/* GtkTreeDragSource::drag_data_get implementation for the shortcuts filter model */
static gboolean
shortcuts_model_filter_drag_data_get (GtkTreeDragSource *drag_source,
				      GtkTreePath       *path,
				      GtkSelectionData  *selection_data)
{
  ShortcutsModelFilter *model;

  model = SHORTCUTS_MODEL_FILTER (drag_source);

  /* FIXME */

  return FALSE;
}

/* Fill the GtkTreeDragSourceIface vtable */
static void
shortcuts_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = shortcuts_model_filter_row_draggable;
  iface->drag_data_get = shortcuts_model_filter_drag_data_get;
}

#if 0
/* Fill the GtkTreeDragDestIface vtable */
static void
shortcuts_model_filter_drag_dest_iface_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = shortcuts_model_filter_drag_data_received;
  iface->row_drop_possible = shortcuts_model_filter_row_drop_possible;
}
#endif

static GtkTreeModel *
shortcuts_model_filter_new (GtkFileChooserDefault *impl,
			    GtkTreeModel          *child_model,
			    GtkTreePath           *root)
{
  ShortcutsModelFilter *model;

  model = g_object_new (SHORTCUTS_MODEL_FILTER_TYPE,
			"child_model", child_model,
			"virtual_root", root,
			NULL);

  model->impl = impl;

  return GTK_TREE_MODEL (model);
}
