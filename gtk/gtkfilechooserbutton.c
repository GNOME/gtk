/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */

/* GTK+: gtkfilechooserbutton.c
 *
 * Copyright (c) 2004 James M. Cape <jcape@ignore-your.tv>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>
#include <cairo-gobject.h>

#include "gtkintl.h"
#include "gtkbookmarksmanagerprivate.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcombobox.h"
#include "gtkdroptarget.h"
#include "gtkicontheme.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtktreemodelfilter.h"
#include "gtkseparator.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechoosernative.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserutils.h"
#include "gtkmarshalers.h"
#include "gtkbinlayout.h"

#include "gtkfilechooserbutton.h"

#include "gtkorientable.h"

#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkstylecontextprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkeventcontroller.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkfilechooserbutton
 * @Short_description: A button to launch a file chooser dialog
 * @Title: GtkFileChooserButton
 * @See_also:#GtkFileChooserDialog
 *
 * The #GtkFileChooserButton is a widget that lets the user select a
 * file.  It implements the #GtkFileChooser interface.  Visually, it is a
 * file name with a button to bring up a #GtkFileChooserDialog.
 * The user can then use that dialog to change the file associated with
 * that button.  This widget does not support setting the
 * #GtkFileChooser:select-multiple property to %TRUE.
 *
 * ## Create a button to let the user select a file in /etc
 *
 * |[<!-- language="C" -->
 * {
 *   GtkWidget *button;
 *   GFile *cwd = g_file_new_for_path ("/etc");
 *
 *   button = gtk_file_chooser_button_new (_("Select a file"),
 *                                         GTK_FILE_CHOOSER_ACTION_OPEN);
 *   gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (button), cwd);
 *   g_object_unref (cwd);
 * }
 * ]|
 *
 * The #GtkFileChooserButton supports the #GtkFileChooserActions
 * %GTK_FILE_CHOOSER_ACTION_OPEN and %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
 *
 * > The #GtkFileChooserButton will ellipsize the label, and will thus
 * > request little horizontal space.  To give the button more space,
 * > you should call gtk_widget_get_preferred_size(),
 * > gtk_file_chooser_button_set_width_chars(), or pack the button in
 * > such a way that other interface elements give space to the
 * > widget.
 *
 * # CSS nodes
 *
 * GtkFileChooserButton has a single CSS node with the name “filechooserbutton”.
 */


/* **************** *
 *  Private Macros  *
 * **************** */

#define ICON_SIZE	        16
#define DEFAULT_TITLE		N_("Select a File")
#define DESKTOP_DISPLAY_NAME	N_("Desktop")
#define FALLBACK_DISPLAY_NAME N_("(None)")


/* ********************** *
 *  Private Enumerations  *
 * ********************** */

/* Property IDs */
enum
{
  PROP_0,

  PROP_DIALOG,
  PROP_TITLE,
  PROP_WIDTH_CHARS,
  PROP_MODAL
};

/* Signals */
enum
{
  FILE_SET,
  LAST_SIGNAL
};

/* TreeModel Columns
 */
enum
{
  ICON_COLUMN,
  DISPLAY_NAME_COLUMN,
  TYPE_COLUMN,
  DATA_COLUMN,
  IS_FOLDER_COLUMN,
  CANCELLABLE_COLUMN,
  NUM_COLUMNS
};

/* TreeModel Row Types */
typedef enum
{
  ROW_TYPE_SPECIAL,
  ROW_TYPE_VOLUME,
  ROW_TYPE_SHORTCUT,
  ROW_TYPE_BOOKMARK_SEPARATOR,
  ROW_TYPE_BOOKMARK,
  ROW_TYPE_CURRENT_FOLDER_SEPARATOR,
  ROW_TYPE_CURRENT_FOLDER,
  ROW_TYPE_OTHER_SEPARATOR,
  ROW_TYPE_OTHER,

  ROW_TYPE_INVALID = -1
}
RowType;


/* ******************** *
 *  Private Structures  *
 * ******************** */

typedef struct _GtkFileChooserButtonClass   GtkFileChooserButtonClass;

struct _GtkFileChooserButton
{
  GtkWidget parent_instance;

  GtkFileChooser *chooser;      /* Points to either dialog or native, depending on which is set */
  GtkWidget *dialog;            /* Set if you explicitly enable */
  GtkFileChooserNative *native; /* Otherwise this is set */
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *combo_box;
  GtkCellRenderer *icon_cell;
  GtkCellRenderer *name_cell;

  GtkTreeModel *model;
  GtkTreeModel *filter_model;

  GtkFileSystem *fs;
  GFile *selection_while_inactive;
  GFile *current_folder_while_inactive;

  gulong fs_volumes_changed_id;

  GCancellable *dnd_select_folder_cancellable;
  GCancellable *update_button_cancellable;
  GSList *change_icon_theme_cancellables;

  GtkBookmarksManager *bookmarks_manager;

  guint8 n_special;
  guint8 n_volumes;
  guint8 n_shortcuts;
  guint8 n_bookmarks;
  guint  has_bookmark_separator       : 1;
  guint  has_current_folder_separator : 1;
  guint  has_current_folder           : 1;
  guint  has_other_separator          : 1;

  /* Used for hiding/showing the dialog when the button is hidden */
  guint  active                       : 1;

  /* Whether the next async callback from GIO should emit the "selection-changed" signal */
  guint  is_changing_selection        : 1;
};

struct _GtkFileChooserButtonClass
{
  GtkWidgetClass parent_class;

  void (* file_set) (GtkFileChooserButton *fc);
};


/* ********************* *
 *  Function Prototypes  *
 * ********************* */

/* GtkFileChooserIface Functions */
static void     gtk_file_chooser_button_file_chooser_iface_init (GtkFileChooserIface *iface);
static gboolean gtk_file_chooser_button_set_current_folder (GtkFileChooser    *chooser,
							    GFile             *file,
							    GError           **error);
static GFile *gtk_file_chooser_button_get_current_folder (GtkFileChooser    *chooser);
static gboolean gtk_file_chooser_button_select_file (GtkFileChooser *chooser,
						     GFile          *file,
						     GError        **error);
static void gtk_file_chooser_button_unselect_file (GtkFileChooser *chooser,
						   GFile          *file);
static void gtk_file_chooser_button_unselect_all (GtkFileChooser *chooser);
static GSList *gtk_file_chooser_button_get_files (GtkFileChooser *chooser);
static gboolean gtk_file_chooser_button_add_shortcut_folder     (GtkFileChooser      *chooser,
								 GFile               *file,
								 GError             **error);
static gboolean gtk_file_chooser_button_remove_shortcut_folder  (GtkFileChooser      *chooser,
								 GFile               *file,
								 GError             **error);

/* GObject Functions */
static void     gtk_file_chooser_button_constructed        (GObject          *object);
static void     gtk_file_chooser_button_set_property       (GObject          *object,
							    guint             param_id,
							    const GValue     *value,
							    GParamSpec       *pspec);
static void     gtk_file_chooser_button_get_property       (GObject          *object,
							    guint             param_id,
							    GValue           *value,
							    GParamSpec       *pspec);
static void     gtk_file_chooser_button_finalize           (GObject          *object);

/* GtkWidget Functions */
static void     gtk_file_chooser_button_destroy            (GtkWidget        *widget);
static void     gtk_file_chooser_button_show               (GtkWidget        *widget);
static void     gtk_file_chooser_button_hide               (GtkWidget        *widget);
static void     gtk_file_chooser_button_root               (GtkWidget *widget);
static void     gtk_file_chooser_button_map                (GtkWidget        *widget);
static gboolean gtk_file_chooser_button_mnemonic_activate  (GtkWidget        *widget,
							    gboolean          group_cycling);
static void     gtk_file_chooser_button_state_flags_changed     (GtkWidget       *widget,
                                                                 GtkStateFlags    previous_state);

/* Utility Functions */
static void          set_info_for_file_at_iter         (GtkFileChooserButton *fs,
							GFile                *file,
							GtkTreeIter          *iter);

static gint          model_get_type_position      (GtkFileChooserButton *button,
						   RowType               row_type);
static void          model_free_row_data          (GtkFileChooserButton *button,
						   GtkTreeIter          *iter);
static void          model_add_special            (GtkFileChooserButton *button);
static void          model_add_other              (GtkFileChooserButton *button);
static void          model_add_volumes            (GtkFileChooserButton *button,
						   GSList               *volumes);
static void          model_add_bookmarks          (GtkFileChooserButton *button,
						   GSList               *bookmarks);
static void          model_update_current_folder  (GtkFileChooserButton *button,
						   GFile                *file);
static void          model_remove_rows            (GtkFileChooserButton *button,
						   gint                  pos,
						   gint                  n_rows);

static gboolean      filter_model_visible_func    (GtkTreeModel         *model,
						   GtkTreeIter          *iter,
						   gpointer              user_data);

static gboolean      combo_box_row_separator_func (GtkTreeModel         *model,
						   GtkTreeIter          *iter,
						   gpointer              user_data);
static void          name_cell_data_func          (GtkCellLayout        *layout,
						   GtkCellRenderer      *cell,
						   GtkTreeModel         *model,
						   GtkTreeIter          *iter,
						   gpointer              user_data);
static void          open_dialog                  (GtkFileChooserButton *button);
static void          update_combo_box             (GtkFileChooserButton *button);
static void          update_label_and_image       (GtkFileChooserButton *button);

/* Child Object Callbacks */
static void     fs_volumes_changed_cb            (GtkFileSystem  *fs,
						  gpointer        user_data);
static void     bookmarks_changed_cb             (gpointer        user_data);

static void     combo_box_changed_cb             (GtkComboBox    *combo_box,
						  gpointer        user_data);

static void     button_clicked_cb                (GtkButton      *real_button,
						  gpointer        user_data);

static void     dialog_response_cb               (GtkDialog      *dialog,
						  gint            response,
						  gpointer        user_data);
static void     native_response_cb               (GtkFileChooserNative *native,
                                                  gint            response,
                                                  gpointer        user_data);

static guint file_chooser_button_signals[LAST_SIGNAL] = { 0 };

/* ******************* *
 *  GType Declaration  *
 * ******************* */

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserButton, gtk_file_chooser_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
                                                gtk_file_chooser_button_file_chooser_iface_init))

struct DndSelectFolderData
{
  GtkFileSystem *file_system;
  GtkFileChooserButton *button;
  GtkFileChooserAction action;
  GFile *file;
  gchar **uris;
  guint i;
  gboolean selected;
};

static void
dnd_select_folder_get_info_cb (GCancellable *cancellable,
			       GFileInfo    *info,
			       const GError *error,
			       gpointer      user_data)
{
  struct DndSelectFolderData *data = user_data;
  GtkFileChooserButton *button = data->button;
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);

  if (cancellable != button->dnd_select_folder_cancellable)
    {
      g_object_unref (data->button);
      g_object_unref (data->file);
      g_strfreev (data->uris);
      g_free (data);

      g_object_unref (cancellable);
      return;
    }

  button->dnd_select_folder_cancellable = NULL;

  if (!cancelled && !error && info != NULL)
    {
      gboolean is_folder;

      is_folder = _gtk_file_info_consider_as_directory (info);

      data->selected =
	(((data->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER && is_folder) ||
	  (data->action == GTK_FILE_CHOOSER_ACTION_OPEN && !is_folder)) &&
	 gtk_file_chooser_select_file (GTK_FILE_CHOOSER (data->button), data->file, NULL));
    }
  else
    data->selected = FALSE;

  if (data->selected || data->uris[++data->i] == NULL)
    {
      g_signal_emit (data->button, file_chooser_button_signals[FILE_SET], 0);

      g_object_unref (data->button);
      g_object_unref (data->file);
      g_strfreev (data->uris);
      g_free (data);

      g_object_unref (cancellable);
      return;
    }

  if (data->file)
    g_object_unref (data->file);

  data->file = g_file_new_for_uri (data->uris[data->i]);

  button->dnd_select_folder_cancellable =
    _gtk_file_system_get_info (data->file_system, data->file,
                               "standard::type",
                               dnd_select_folder_get_info_cb, user_data);

  g_object_unref (cancellable);
}

static gboolean
gtk_file_chooser_button_drop (GtkDropTarget        *target,
                              const GValue         *value,
                              double                x,
                              double                y,
                              GtkFileChooserButton *button)
{
  struct DndSelectFolderData *info;
  GFile *file;

  file = g_value_get_object (value);

  info = g_new0 (struct DndSelectFolderData, 1);
  info->button = g_object_ref (button);
  info->i = 0;
  info->uris = g_new0 (char *, 2);
  info->selected = FALSE;
  info->file_system = button->fs;
  g_object_get (button->chooser, "action", &info->action, NULL);

  info->file = g_object_ref (file);

  if (button->dnd_select_folder_cancellable)
    g_cancellable_cancel (button->dnd_select_folder_cancellable);

  button->dnd_select_folder_cancellable =
    _gtk_file_system_get_info (button->fs, info->file,
                               "standard::type",
                               dnd_select_folder_get_info_cb, info);

  return TRUE;
}

static void
gtk_file_chooser_button_class_init (GtkFileChooserButtonClass * class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->constructed = gtk_file_chooser_button_constructed;
  gobject_class->set_property = gtk_file_chooser_button_set_property;
  gobject_class->get_property = gtk_file_chooser_button_get_property;
  gobject_class->finalize = gtk_file_chooser_button_finalize;

  widget_class->destroy = gtk_file_chooser_button_destroy;
  widget_class->show = gtk_file_chooser_button_show;
  widget_class->hide = gtk_file_chooser_button_hide;
  widget_class->map = gtk_file_chooser_button_map;
  widget_class->root = gtk_file_chooser_button_root;
  widget_class->mnemonic_activate = gtk_file_chooser_button_mnemonic_activate;
  widget_class->state_flags_changed = gtk_file_chooser_button_state_flags_changed;
  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;

  /**
   * GtkFileChooserButton::file-set:
   * @widget: the object which received the signal.
   *
   * The ::file-set signal is emitted when the user selects a file.
   *
   * Note that this signal is only emitted when the user
   * changes the file.
   */
  file_chooser_button_signals[FILE_SET] =
    g_signal_new (I_("file-set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkFileChooserButtonClass, file_set),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkFileChooserButton:dialog:
   *
   * Instance of the #GtkFileChooserDialog associated with the button.
   */
  g_object_class_install_property (gobject_class, PROP_DIALOG,
				   g_param_spec_object ("dialog",
							P_("Dialog"),
							P_("The file chooser dialog to use."),
							GTK_TYPE_FILE_CHOOSER,
							(GTK_PARAM_WRITABLE |
							 G_PARAM_CONSTRUCT_ONLY)));

  /**
   * GtkFileChooserButton:title:
   *
   * Title to put on the #GtkFileChooserDialog associated with the button.
   */
  g_object_class_install_property (gobject_class, PROP_TITLE,
				   g_param_spec_string ("title",
							P_("Title"),
							P_("The title of the file chooser dialog."),
							_(DEFAULT_TITLE),
							GTK_PARAM_READWRITE));

  /**
   * GtkFileChooserButton:width-chars:
   *
   * The width of the entry and label inside the button, in characters.
   */
  g_object_class_install_property (gobject_class, PROP_WIDTH_CHARS,
				   g_param_spec_int ("width-chars",
						     P_("Width In Characters"),
						     P_("The desired width of the button widget, in characters."),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MODAL,
				   g_param_spec_boolean ("modal",
						         P_("Modal"),
						         P_("Whether to make the dialog modal"),
						         TRUE,
						         GTK_PARAM_READWRITE));

  _gtk_file_chooser_install_properties (gobject_class);

  gtk_widget_class_set_css_name (widget_class, I_("filechooserbutton"));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_file_chooser_button_init (GtkFileChooserButton *button)
{
  GtkWidget *box;
  GtkWidget *icon;
  GtkDropTarget *target;

  button->button = gtk_button_new ();
  g_signal_connect (button->button, "clicked", G_CALLBACK (button_clicked_cb), button);
  button->image = gtk_image_new ();
  button->label = gtk_label_new (_(FALLBACK_DISPLAY_NAME));
  gtk_label_set_xalign (GTK_LABEL (button->label), 0.0f);
  gtk_widget_set_hexpand (button->label, TRUE);
  icon = gtk_image_new_from_icon_name ("document-open-symbolic");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_valign (button->image, GTK_ALIGN_BASELINE);
  gtk_container_add (GTK_CONTAINER (box), button->image);
  gtk_widget_set_valign (button->label, GTK_ALIGN_BASELINE);
  gtk_container_add (GTK_CONTAINER (box), button->label);
  gtk_widget_set_valign (icon, GTK_ALIGN_BASELINE);
  gtk_container_add (GTK_CONTAINER (box), icon);
  gtk_button_set_child (GTK_BUTTON (button->button), box);

  gtk_widget_set_parent (button->button, GTK_WIDGET (button));

  button->model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLUMNS,
                                                      G_TYPE_ICON,
                                                      G_TYPE_STRING,
                                                      G_TYPE_CHAR,
                                                      G_TYPE_POINTER,
                                                      G_TYPE_BOOLEAN,
                                                      G_TYPE_POINTER));

  button->combo_box = gtk_combo_box_new ();
  g_signal_connect (button->combo_box, "changed", G_CALLBACK (combo_box_changed_cb), button);
  button->icon_cell = gtk_cell_renderer_pixbuf_new ();
  button->name_cell = gtk_cell_renderer_text_new ();
  g_object_set (button->name_cell, "xpad", 6, NULL);

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button->combo_box), button->icon_cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button->combo_box),
                                  button->icon_cell, "gicon", ICON_COLUMN, NULL);

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button->combo_box), button->name_cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button->combo_box),
                                  button->name_cell, "text", DISPLAY_NAME_COLUMN, NULL);

  gtk_widget_hide (button->combo_box);
  gtk_widget_set_parent (button->combo_box, GTK_WIDGET (button));

  /* Bookmarks manager */
  button->bookmarks_manager = _gtk_bookmarks_manager_new (bookmarks_changed_cb, button);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (button->combo_box),
				      button->name_cell, name_cell_data_func,
				      NULL, NULL);

  /* DnD */
  target = gtk_drop_target_new (G_TYPE_FILE, GDK_ACTION_COPY);
  g_signal_connect (target, "drop", G_CALLBACK (gtk_file_chooser_button_drop), button);
  gtk_widget_add_controller (GTK_WIDGET (button), GTK_EVENT_CONTROLLER (target));
}


/* ******************************* *
 *  GtkFileChooserIface Functions  *
 * ******************************* */
static void
gtk_file_chooser_button_file_chooser_iface_init (GtkFileChooserIface *iface)
{
  _gtk_file_chooser_delegate_iface_init (iface);

  iface->set_current_folder = gtk_file_chooser_button_set_current_folder;
  iface->get_current_folder = gtk_file_chooser_button_get_current_folder;
  iface->select_file = gtk_file_chooser_button_select_file;
  iface->unselect_file = gtk_file_chooser_button_unselect_file;
  iface->unselect_all = gtk_file_chooser_button_unselect_all;
  iface->get_files = gtk_file_chooser_button_get_files;
  iface->add_shortcut_folder = gtk_file_chooser_button_add_shortcut_folder;
  iface->remove_shortcut_folder = gtk_file_chooser_button_remove_shortcut_folder;
}

static void
emit_selection_changed_if_changing_selection (GtkFileChooserButton *button)
{
  if (button->is_changing_selection)
    {
      button->is_changing_selection = FALSE;
      g_signal_emit_by_name (button, "selection-changed");
    }
}

static gboolean
gtk_file_chooser_button_set_current_folder (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);

  if (button->current_folder_while_inactive)
    g_object_unref (button->current_folder_while_inactive);

  button->current_folder_while_inactive = g_object_ref (file);

  update_combo_box (button);

  g_signal_emit_by_name (button, "current-folder-changed");

  if (button->active)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (button->chooser), file, NULL);

  return TRUE;
}

static GFile *
gtk_file_chooser_button_get_current_folder (GtkFileChooser *chooser)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);

  if (button->current_folder_while_inactive)
    return g_object_ref (button->current_folder_while_inactive);
  else
    return NULL;
}

static gboolean
gtk_file_chooser_button_select_file (GtkFileChooser *chooser,
				     GFile          *file,
				     GError        **error)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);

  if (button->selection_while_inactive)
    g_object_unref (button->selection_while_inactive);

  button->selection_while_inactive = g_object_ref (file);

  button->is_changing_selection = TRUE;

  update_label_and_image (button);
  update_combo_box (button);

  if (button->active)
    gtk_file_chooser_select_file (GTK_FILE_CHOOSER (button->chooser), file, NULL);

  return TRUE;
}

static void
unselect_current_file (GtkFileChooserButton *button)
{
  if (button->selection_while_inactive)
    {
      g_object_unref (button->selection_while_inactive);
      button->selection_while_inactive = NULL;
    }

  button->is_changing_selection = TRUE;

  update_label_and_image (button);
  update_combo_box (button);
}

static void
gtk_file_chooser_button_unselect_file (GtkFileChooser *chooser,
				       GFile          *file)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);

  if (g_file_equal (button->selection_while_inactive, file))
    unselect_current_file (button);

  if (button->active)
    gtk_file_chooser_unselect_file (GTK_FILE_CHOOSER (button->chooser), file);
}

static void
gtk_file_chooser_button_unselect_all (GtkFileChooser *chooser)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);

  unselect_current_file (button);

  if (button->active)
    gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (button->chooser));
}

static GFile *
get_selected_file (GtkFileChooserButton *button)
{
  GFile *retval;

  retval = NULL;

  if (button->selection_while_inactive)
    retval = button->selection_while_inactive;
  else if (button->chooser && gtk_file_chooser_get_action (button->chooser) == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      /* If there is no "real" selection in SELECT_FOLDER mode, then we'll just return
       * the current folder, since that is what GtkFileChooserWidget would do.
       */
      if (button->current_folder_while_inactive)
	retval = button->current_folder_while_inactive;
    }

  if (retval)
    return g_object_ref (retval);
  else
    return NULL;
}

static GSList *
gtk_file_chooser_button_get_files (GtkFileChooser *chooser)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);
  GFile *file;

  file = get_selected_file (button);
  if (file)
    return g_slist_prepend (NULL, file);
  else
    return NULL;
}

static gboolean
gtk_file_chooser_button_add_shortcut_folder (GtkFileChooser  *chooser,
					     GFile           *file,
					     GError         **error)
{
  GtkFileChooser *delegate;
  gboolean retval;

  delegate = g_object_get_qdata (G_OBJECT (chooser),
				 GTK_FILE_CHOOSER_DELEGATE_QUARK);
  retval = gtk_file_chooser_add_shortcut_folder (delegate, file, error);

  if (retval)
    {
      GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);
      GtkTreeIter iter;
      gint pos;

      pos = model_get_type_position (button, ROW_TYPE_SHORTCUT);
      pos += button->n_shortcuts;

      gtk_list_store_insert (GTK_LIST_STORE (button->model), &iter, pos);
      gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
			  TYPE_COLUMN, ROW_TYPE_SHORTCUT,
			  DATA_COLUMN, g_object_ref (file),
			  IS_FOLDER_COLUMN, FALSE,
			  -1);
      set_info_for_file_at_iter (button, file, &iter);
      button->n_shortcuts++;

      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (button->filter_model));
    }

  return retval;
}

static gboolean
gtk_file_chooser_button_remove_shortcut_folder (GtkFileChooser  *chooser,
						GFile           *file,
						GError         **error)
{
  GtkFileChooser *delegate;
  gboolean retval;

  delegate = g_object_get_qdata (G_OBJECT (chooser),
				 GTK_FILE_CHOOSER_DELEGATE_QUARK);

  retval = gtk_file_chooser_remove_shortcut_folder (delegate, file, error);

  if (retval)
    {
      GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);
      GtkTreeIter iter;
      gint pos;
      gchar type;

      pos = model_get_type_position (button, ROW_TYPE_SHORTCUT);
      gtk_tree_model_iter_nth_child (button->model, &iter, NULL, pos);

      do
	{
	  gpointer data;

	  gtk_tree_model_get (button->model, &iter,
			      TYPE_COLUMN, &type,
			      DATA_COLUMN, &data,
			      -1);

	  if (type == ROW_TYPE_SHORTCUT &&
	      data && g_file_equal (data, file))
	    {
	      model_free_row_data (GTK_FILE_CHOOSER_BUTTON (chooser), &iter);
	      gtk_list_store_remove (GTK_LIST_STORE (button->model), &iter);
	      button->n_shortcuts--;
	      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (button->filter_model));
	      update_combo_box (GTK_FILE_CHOOSER_BUTTON (chooser));
	      break;
	    }
	}
      while (type == ROW_TYPE_SHORTCUT &&
	     gtk_tree_model_iter_next (button->model, &iter));
    }

  return retval;
}


/* ******************* *
 *  GObject Functions  *
 * ******************* */

static void
gtk_file_chooser_button_constructed (GObject *object)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (object);
  GSList *list;

  G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->constructed (object);

  if (!button->dialog)
    {
      button->native = gtk_file_chooser_native_new (NULL,
                                                    NULL,
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    NULL,
                                                    NULL);
      button->chooser = GTK_FILE_CHOOSER (button->native);
      gtk_file_chooser_button_set_title (button, _(DEFAULT_TITLE));
      gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (button->native), TRUE);

      g_signal_connect (button->native, "response",
                        G_CALLBACK (native_response_cb), object);
    }
  else /* dialog set */
    {
      button->chooser = GTK_FILE_CHOOSER (button->dialog);
      gtk_window_set_hide_on_close (GTK_WINDOW (button->chooser), TRUE);

      if (!gtk_window_get_title (GTK_WINDOW (button->dialog)))
        gtk_file_chooser_button_set_title (button, _(DEFAULT_TITLE));

      g_signal_connect (button->dialog, "response",
                        G_CALLBACK (dialog_response_cb), object);

      g_object_add_weak_pointer (G_OBJECT (button->dialog),
                                 (gpointer) (&button->dialog));
    }

  /* This is used, instead of the standard delegate, to ensure that signals are only
   * delegated when the OK button is pressed. */
  g_object_set_qdata (object, GTK_FILE_CHOOSER_DELEGATE_QUARK, button->chooser);

  button->fs =
    g_object_ref (_gtk_file_chooser_get_file_system (button->chooser));

  model_add_special (button);

  list = _gtk_file_system_list_volumes (button->fs);
  model_add_volumes (button, list);
  g_slist_free (list);

  list = _gtk_bookmarks_manager_list_bookmarks (button->bookmarks_manager);
  model_add_bookmarks (button, list);
  g_slist_free_full (list, g_object_unref);

  model_add_other (button);

  button->filter_model = gtk_tree_model_filter_new (button->model, NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (button->filter_model),
					  filter_model_visible_func,
					  object, NULL);

  gtk_combo_box_set_model (GTK_COMBO_BOX (button->combo_box), button->filter_model);
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (button->combo_box),
					combo_box_row_separator_func,
					NULL, NULL);

  /* set up the action for a user-provided dialog, this also updates
   * the label, image and combobox
   */
  g_object_set (object,
		"action", gtk_file_chooser_get_action (GTK_FILE_CHOOSER (button->chooser)),
		NULL);

  button->fs_volumes_changed_id =
    g_signal_connect (button->fs, "volumes-changed",
		      G_CALLBACK (fs_volumes_changed_cb), object);

  update_label_and_image (button);
  update_combo_box (button);
}

static void
gtk_file_chooser_button_set_property (GObject      *object,
				      guint         param_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (object);

  switch (param_id)
    {
    case PROP_DIALOG:
      /* Construct-only */
      button->dialog = g_value_get_object (value);
      break;
    case PROP_WIDTH_CHARS:
      gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (object),
					       g_value_get_int (value));
      break;
    case GTK_FILE_CHOOSER_PROP_ACTION:
      switch (g_value_get_enum (value))
	{
	case GTK_FILE_CHOOSER_ACTION_SAVE:
	  {
	    GEnumClass *eclass;
	    GEnumValue *eval;

	    eclass = g_type_class_peek (GTK_TYPE_FILE_CHOOSER_ACTION);
	    eval = g_enum_get_value (eclass, g_value_get_enum (value));
	    g_warning ("%s: Choosers of type '%s' do not support '%s'.",
		       G_STRFUNC, G_OBJECT_TYPE_NAME (object), eval->value_name);

	    g_value_set_enum ((GValue *) value, GTK_FILE_CHOOSER_ACTION_OPEN);
	  }
	  break;
        default:
          break;
	}

      g_object_set_property (G_OBJECT (button->chooser), pspec->name, value);
      update_label_and_image (GTK_FILE_CHOOSER_BUTTON (object));
      update_combo_box (GTK_FILE_CHOOSER_BUTTON (object));

      switch (g_value_get_enum (value))
        {
        case GTK_FILE_CHOOSER_ACTION_OPEN:
          gtk_widget_hide (button->combo_box);
          gtk_widget_show (button->button);
          gtk_widget_queue_resize (GTK_WIDGET (button));
          break;
        case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
          gtk_widget_show (button->combo_box);
          gtk_widget_hide (button->button);
          gtk_widget_queue_resize (GTK_WIDGET (button));
          break;
        default:
          g_assert_not_reached ();
          break;
        }
      break;

    case PROP_TITLE:
    case PROP_MODAL:
    case GTK_FILE_CHOOSER_PROP_FILTER:
    case GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS:
      g_object_set_property (G_OBJECT (button->chooser), pspec->name, value);
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_warning ("%s: Choosers of type '%s' do not support selecting multiple files.",
		 G_STRFUNC, G_OBJECT_TYPE_NAME (object));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_button_get_property (GObject    *object,
				      guint       param_id,
				      GValue     *value,
				      GParamSpec *pspec)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (object);

  switch (param_id)
    {
    case PROP_WIDTH_CHARS:
      g_value_set_int (value,
		       gtk_label_get_width_chars (GTK_LABEL (button->label)));
      break;

    case PROP_TITLE:
    case PROP_MODAL:
    case GTK_FILE_CHOOSER_PROP_ACTION:
    case GTK_FILE_CHOOSER_PROP_FILTER:
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
    case GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS:
      g_object_get_property (G_OBJECT (button->chooser), pspec->name, value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_button_finalize (GObject *object)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (object);

  g_clear_object (&button->selection_while_inactive);
  g_clear_object (&button->current_folder_while_inactive);

  gtk_widget_unparent (button->button);
  gtk_widget_unparent (button->combo_box);

  G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->finalize (object);
}

/* ********************* *
 *  GtkWidget Functions  *
 * ********************* */

static void
gtk_file_chooser_button_state_flags_changed (GtkWidget     *widget,
                                             GtkStateFlags  previous_state)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);

  if (gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_DROP_ACTIVE)
    {
      gtk_widget_set_state_flags (button->button, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
      gtk_widget_set_state_flags (button->combo_box, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
    }
  else
    {
      gtk_widget_unset_state_flags (button->button, GTK_STATE_FLAG_DROP_ACTIVE);
      gtk_widget_unset_state_flags (button->combo_box, GTK_STATE_FLAG_DROP_ACTIVE);
    }

  GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->state_flags_changed (widget, previous_state);
}

static void
gtk_file_chooser_button_destroy (GtkWidget *widget)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);
  GSList *l;

  if (button->model)
    {
      model_remove_rows (button, 0, gtk_tree_model_iter_n_children (button->model, NULL));
      g_clear_object (&button->model);
    }

  g_clear_pointer (&button->dialog, gtk_widget_destroy);

  if (button->native)
    gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (button->native));

  g_clear_object (&button->native);
  button->chooser = NULL; /* Was either button->dialog or button->native! */

  g_clear_pointer (&button->dnd_select_folder_cancellable, g_cancellable_cancel);
  g_clear_pointer (&button->update_button_cancellable, g_cancellable_cancel);

  if (button->change_icon_theme_cancellables)
    {
      for (l = button->change_icon_theme_cancellables; l; l = l->next)
        {
	  GCancellable *cancellable = G_CANCELLABLE (l->data);
	  g_cancellable_cancel (cancellable);
        }
      g_slist_free (button->change_icon_theme_cancellables);
      button->change_icon_theme_cancellables = NULL;
    }

  g_clear_object (&button->filter_model);

  if (button->fs)
    {
      g_signal_handler_disconnect (button->fs, button->fs_volumes_changed_id);
      g_clear_object (&button->fs);
    }

  g_clear_pointer (&button->bookmarks_manager, _gtk_bookmarks_manager_free);

  GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->destroy (widget);
}

static void
gtk_file_chooser_button_show (GtkWidget *widget)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show)
    GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show (widget);

  if (button->active)
    open_dialog (GTK_FILE_CHOOSER_BUTTON (widget));
}

static void
gtk_file_chooser_button_hide (GtkWidget *widget)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);

  if (button->dialog)
    gtk_widget_hide (button->dialog);
  else if (button->native)
    gtk_native_dialog_hide (GTK_NATIVE_DIALOG (button->native));

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->hide)
    GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->hide (widget);
}

static void
gtk_file_chooser_button_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->map (widget);
}

static gboolean
gtk_file_chooser_button_mnemonic_activate (GtkWidget *widget,
					   gboolean   group_cycling)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);

  switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (button->chooser)))
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      gtk_widget_grab_focus (button->button);
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      return gtk_widget_mnemonic_activate (button->combo_box, group_cycling);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

/* Changes the icons wherever it is needed */
struct ChangeIconThemeData
{
  GtkFileChooserButton *button;
  GtkTreeRowReference *row_ref;
};

static void
change_icon_theme_get_info_cb (GCancellable *cancellable,
			       GFileInfo    *info,
			       const GError *error,
			       gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GIcon *icon;
  struct ChangeIconThemeData *data = user_data;
  GtkFileChooserButton *button = data->button;

  if (!g_slist_find (button->change_icon_theme_cancellables, cancellable))
    goto out;

  button->change_icon_theme_cancellables =
    g_slist_remove (button->change_icon_theme_cancellables, cancellable);

  if (cancelled || error)
    goto out;

  icon = _gtk_file_info_get_icon (info, ICON_SIZE, gtk_widget_get_scale_factor (GTK_WIDGET (data->button)));
  if (icon)
    {
      gint width = 0;
      GtkTreeIter iter;
      GtkTreePath *path;

      width = MAX (width, ICON_SIZE);

      path = gtk_tree_row_reference_get_path (data->row_ref);
      if (path)
        {
          gtk_tree_model_get_iter (button->model, &iter, path);
          gtk_tree_path_free (path);

          gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
	  		      ICON_COLUMN, icon,
			      -1);

          g_object_set (button->icon_cell,
		        "width", width,
		        NULL);
        }
      g_object_unref (icon);
    }

out:
  g_object_unref (data->button);
  gtk_tree_row_reference_free (data->row_ref);
  g_free (data);

  g_object_unref (cancellable);
}

static void
change_icon_theme (GtkFileChooserButton *button)
{
  GtkTreeIter iter;
  GSList *l;
  gint width = 0;

  for (l = button->change_icon_theme_cancellables; l; l = l->next)
    {
      GCancellable *cancellable = G_CANCELLABLE (l->data);
      g_cancellable_cancel (cancellable);
    }
  g_slist_free (button->change_icon_theme_cancellables);
  button->change_icon_theme_cancellables = NULL;

  update_label_and_image (button);

  gtk_tree_model_get_iter_first (button->model, &iter);

  do
    {
      GIcon *icon = NULL;
      gchar type;
      gpointer data;

      type = ROW_TYPE_INVALID;
      gtk_tree_model_get (button->model, &iter,
			  TYPE_COLUMN, &type,
			  DATA_COLUMN, &data,
			  -1);

      switch (type)
	{
	case ROW_TYPE_SPECIAL:
	case ROW_TYPE_SHORTCUT:
	case ROW_TYPE_BOOKMARK:
	case ROW_TYPE_CURRENT_FOLDER:
	  if (data)
	    {
	      if (g_file_is_native (G_FILE (data)))
		{
		  GtkTreePath *path;
		  GCancellable *cancellable;
		  struct ChangeIconThemeData *info;

		  info = g_new0 (struct ChangeIconThemeData, 1);
		  info->button = g_object_ref (button);
		  path = gtk_tree_model_get_path (button->model, &iter);
		  info->row_ref = gtk_tree_row_reference_new (button->model, path);
		  gtk_tree_path_free (path);

		  cancellable =
		    _gtk_file_system_get_info (button->fs, data,
					       "standard::icon",
					       change_icon_theme_get_info_cb,
					       info);
                  button->change_icon_theme_cancellables =
                    g_slist_append (button->change_icon_theme_cancellables, cancellable);
                  icon = NULL;
		}
	      else
                {
                  /* Don't call get_info for remote paths to avoid latency and
                   * auth dialogs.
                   * If we switch to a better bookmarks file format (XBEL), we
                   * should use mime info to get a better icon.
                   */
                  icon = g_themed_icon_new ("folder-remote");
                }
	    }
	  break;
	case ROW_TYPE_VOLUME:
	  if (data)
            icon = _gtk_file_system_volume_get_icon (data);

	  break;
	default:
	  continue;
	  break;
	}

      if (icon)
	width = MAX (width, ICON_SIZE);

      gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
			  ICON_COLUMN, icon,
			  -1);

      if (icon)
	g_object_unref (icon);
    }
  while (gtk_tree_model_iter_next (button->model, &iter));

  g_object_set (button->icon_cell,
		"width", width,
		NULL);
}

static void
gtk_file_chooser_button_root (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->root (widget);

  change_icon_theme (GTK_FILE_CHOOSER_BUTTON (widget));
}

/* ******************* *
 *  Utility Functions  *
 * ******************* */

/* General */

struct SetDisplayNameData
{
  GtkFileChooserButton *button;
  char *label;
  GtkTreeRowReference *row_ref;
};

static void
set_info_get_info_cb (GCancellable *cancellable,
		      GFileInfo    *info,
		      const GError *error,
		      gpointer      callback_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GIcon *icon;
  GtkTreePath *path;
  GtkTreeIter iter;
  GCancellable *model_cancellable = NULL;
  struct SetDisplayNameData *data = callback_data;
  GtkFileChooserButton *button = data->button;
  gboolean is_folder;

  if (!button->model)
    /* button got destroyed */
    goto out;

  path = gtk_tree_row_reference_get_path (data->row_ref);
  if (!path)
    /* Cancellable doesn't exist anymore in the model */
    goto out;

  gtk_tree_model_get_iter (button->model, &iter, path);
  gtk_tree_path_free (path);

  /* Validate the cancellable */
  gtk_tree_model_get (button->model, &iter,
		      CANCELLABLE_COLUMN, &model_cancellable,
		      -1);
  if (cancellable != model_cancellable)
    goto out;

  gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
		      CANCELLABLE_COLUMN, NULL,
		      -1);

  if (cancelled || error)
    /* There was an error, leave the fallback name in there */
    goto out;

  icon = _gtk_file_info_get_icon (info, ICON_SIZE, gtk_widget_get_scale_factor (GTK_WIDGET (data->button)));

  if (!data->label)
    data->label = g_strdup (g_file_info_get_display_name (info));

  is_folder = _gtk_file_info_consider_as_directory (info);

  gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
		      ICON_COLUMN, icon,
		      DISPLAY_NAME_COLUMN, data->label,
		      IS_FOLDER_COLUMN, is_folder,
		      -1);

  if (icon)
    g_object_unref (icon);

out:
  g_object_unref (data->button);
  g_free (data->label);
  gtk_tree_row_reference_free (data->row_ref);
  g_free (data);

  g_object_unref (cancellable);
}

static void
set_info_for_file_at_iter (GtkFileChooserButton *button,
			   GFile                *file,
			   GtkTreeIter          *iter)
{
  struct SetDisplayNameData *data;
  GtkTreePath *tree_path;
  GCancellable *cancellable;

  data = g_new0 (struct SetDisplayNameData, 1);
  data->button = g_object_ref (button);
  data->label = _gtk_bookmarks_manager_get_bookmark_label (button->bookmarks_manager, file);

  tree_path = gtk_tree_model_get_path (button->model, iter);
  data->row_ref = gtk_tree_row_reference_new (button->model, tree_path);
  gtk_tree_path_free (tree_path);

  cancellable = _gtk_file_system_get_info (button->fs, file,
					   "standard::type,standard::icon,standard::display-name",
					   set_info_get_info_cb, data);

  gtk_list_store_set (GTK_LIST_STORE (button->model), iter,
		      CANCELLABLE_COLUMN, cancellable,
		      -1);
}

/* Shortcuts Model */
static gint
model_get_type_position (GtkFileChooserButton *button,
			 RowType               row_type)
{
  gint retval = 0;

  if (row_type == ROW_TYPE_SPECIAL)
    return retval;

  retval += button->n_special;

  if (row_type == ROW_TYPE_VOLUME)
    return retval;

  retval += button->n_volumes;

  if (row_type == ROW_TYPE_SHORTCUT)
    return retval;

  retval += button->n_shortcuts;

  if (row_type == ROW_TYPE_BOOKMARK_SEPARATOR)
    return retval;

  retval += button->has_bookmark_separator;

  if (row_type == ROW_TYPE_BOOKMARK)
    return retval;

  retval += button->n_bookmarks;

  if (row_type == ROW_TYPE_CURRENT_FOLDER_SEPARATOR)
    return retval;

  retval += button->has_current_folder_separator;

  if (row_type == ROW_TYPE_CURRENT_FOLDER)
    return retval;

  retval += button->has_current_folder;

  if (row_type == ROW_TYPE_OTHER_SEPARATOR)
    return retval;

  retval += button->has_other_separator;

  if (row_type == ROW_TYPE_OTHER)
    return retval;

  retval++;

  g_assert_not_reached ();
  return -1;
}

static void
model_free_row_data (GtkFileChooserButton *button,
		     GtkTreeIter          *iter)
{
  gchar type;
  gpointer data;
  GCancellable *cancellable;

  gtk_tree_model_get (button->model, iter,
		      TYPE_COLUMN, &type,
		      DATA_COLUMN, &data,
		      CANCELLABLE_COLUMN, &cancellable,
		      -1);

  if (cancellable)
    g_cancellable_cancel (cancellable);

  switch (type)
    {
    case ROW_TYPE_SPECIAL:
    case ROW_TYPE_SHORTCUT:
    case ROW_TYPE_BOOKMARK:
    case ROW_TYPE_CURRENT_FOLDER:
      g_object_unref (data);
      break;
    case ROW_TYPE_VOLUME:
      _gtk_file_system_volume_unref (data);
      break;
    default:
      break;
    }
}

static void
model_add_special_get_info_cb (GCancellable *cancellable,
			       GFileInfo    *info,
			       const GError *error,
			       gpointer      user_data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GtkTreeIter iter;
  GtkTreePath *path;
  GIcon *icon;
  GCancellable *model_cancellable = NULL;
  struct ChangeIconThemeData *data = user_data;
  GtkFileChooserButton *button = data->button;
  gchar *name;

  if (!button->model)
    /* button got destroyed */
    goto out;

  path = gtk_tree_row_reference_get_path (data->row_ref);
  if (!path)
    /* Cancellable doesn't exist anymore in the model */
    goto out;

  gtk_tree_model_get_iter (button->model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (button->model, &iter,
		      CANCELLABLE_COLUMN, &model_cancellable,
		      -1);
  if (cancellable != model_cancellable)
 
    goto out;

  gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
		      CANCELLABLE_COLUMN, NULL,
		      -1);

  if (cancelled || error)
    goto out;

  icon = _gtk_file_info_get_icon (info, ICON_SIZE, gtk_widget_get_scale_factor (GTK_WIDGET (data->button)));
  if (icon)
    {
      gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
			  ICON_COLUMN, icon,
			  -1);
      g_object_unref (icon);
    }

  gtk_tree_model_get (button->model, &iter,
                      DISPLAY_NAME_COLUMN, &name,
                      -1);
  if (!name)
    gtk_list_store_set (GTK_LIST_STORE (button->model), &iter,
  		        DISPLAY_NAME_COLUMN, g_file_info_get_display_name (info),
		        -1);
  g_free (name);

out:
  g_object_unref (data->button);
  gtk_tree_row_reference_free (data->row_ref);
  g_free (data);

  g_object_unref (cancellable);
}

static void
model_add_special (GtkFileChooserButton *button)
{
  const gchar *homedir;
  const gchar *desktopdir;
  GtkListStore *store;
  GtkTreeIter iter;
  GFile *file;
  gint pos;

  store = GTK_LIST_STORE (button->model);
  pos = model_get_type_position (button, ROW_TYPE_SPECIAL);

  homedir = g_get_home_dir ();

  if (homedir)
    {
      GtkTreePath *tree_path;
      GCancellable *cancellable;
      struct ChangeIconThemeData *info;

      file = g_file_new_for_path (homedir);
      gtk_list_store_insert (store, &iter, pos);
      pos++;

      info = g_new0 (struct ChangeIconThemeData, 1);
      info->button = g_object_ref (button);
      tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
      info->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store),
						  tree_path);
      gtk_tree_path_free (tree_path);

      cancellable = _gtk_file_system_get_info (button->fs, file,
					       "standard::icon,standard::display-name",
					       model_add_special_get_info_cb, info);

      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, NULL,
			  TYPE_COLUMN, ROW_TYPE_SPECIAL,
			  DATA_COLUMN, file,
			  IS_FOLDER_COLUMN, TRUE,
			  CANCELLABLE_COLUMN, cancellable,
			  -1);

      button->n_special++;
    }

  desktopdir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

  /* "To disable a directory, point it to the homedir."
   * See http://freedesktop.org/wiki/Software/xdg-user-dirs
   */
  if (g_strcmp0 (desktopdir, g_get_home_dir ()) != 0)
    {
      GtkTreePath *tree_path;
      GCancellable *cancellable;
      struct ChangeIconThemeData *info;

      file = g_file_new_for_path (desktopdir);
      gtk_list_store_insert (store, &iter, pos);
      pos++;

      info = g_new0 (struct ChangeIconThemeData, 1);
      info->button = g_object_ref (button);
      tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
      info->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store),
						  tree_path);
      gtk_tree_path_free (tree_path);

      cancellable = _gtk_file_system_get_info (button->fs, file,
					       "standard::icon,standard::display-name",
					       model_add_special_get_info_cb, info);

      gtk_list_store_set (store, &iter,
			  TYPE_COLUMN, ROW_TYPE_SPECIAL,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, _(DESKTOP_DISPLAY_NAME),
			  DATA_COLUMN, file,
			  IS_FOLDER_COLUMN, TRUE,
			  CANCELLABLE_COLUMN, cancellable,
			  -1);

      button->n_special++;
    }
}

static void
model_add_volumes (GtkFileChooserButton *button,
                   GSList               *volumes)
{
  GtkListStore *store;
  gint pos;
  GSList *l;

  if (!volumes)
    return;

  store = GTK_LIST_STORE (button->model);
  pos = model_get_type_position (button, ROW_TYPE_VOLUME);

  for (l = volumes; l; l = l->next)
    {
      GtkFileSystemVolume *volume;
      GtkTreeIter iter;
      GIcon *icon;
      gchar *display_name;

      volume = l->data;
      icon = _gtk_file_system_volume_get_icon (volume);
      display_name = _gtk_file_system_volume_get_display_name (volume);

      gtk_list_store_insert (store, &iter, pos);
      gtk_list_store_set (store, &iter,
                          ICON_COLUMN, icon,
                          DISPLAY_NAME_COLUMN, display_name,
                          TYPE_COLUMN, ROW_TYPE_VOLUME,
                          DATA_COLUMN, _gtk_file_system_volume_ref (volume),
                          IS_FOLDER_COLUMN, TRUE,
                          -1);

      if (icon)
        g_object_unref (icon);
      g_free (display_name);

      button->n_volumes++;
      pos++;
    }
}

static void
model_add_bookmarks (GtkFileChooserButton *button,
		     GSList               *bookmarks)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint pos;
  GSList *l;

  if (!bookmarks)
    return;

  store = GTK_LIST_STORE (button->model);
  pos = model_get_type_position (button, ROW_TYPE_BOOKMARK);

  for (l = bookmarks; l; l = l->next)
    {
      GFile *file;

      file = l->data;

      if (_gtk_file_has_native_path (file))
	{
	  gtk_list_store_insert (store, &iter, pos);
	  gtk_list_store_set (store, &iter,
			      ICON_COLUMN, NULL,
			      DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
			      TYPE_COLUMN, ROW_TYPE_BOOKMARK,
			      DATA_COLUMN, g_object_ref (file),
			      IS_FOLDER_COLUMN, FALSE,
			      -1);
	  set_info_for_file_at_iter (button, file, &iter);
	}
      else
	{
	  gchar *label;
	  GIcon *icon;

	  /* Don't call get_info for remote paths to avoid latency and
	   * auth dialogs.
	   * If we switch to a better bookmarks file format (XBEL), we
	   * should use mime info to get a better icon.
	   */
	  label = _gtk_bookmarks_manager_get_bookmark_label (button->bookmarks_manager, file);
	  if (!label)
	    label = _gtk_file_chooser_label_for_file (file);

          icon = g_themed_icon_new ("folder-remote");

	  gtk_list_store_insert (store, &iter, pos);
	  gtk_list_store_set (store, &iter,
			      ICON_COLUMN, icon,
			      DISPLAY_NAME_COLUMN, label,
			      TYPE_COLUMN, ROW_TYPE_BOOKMARK,
			      DATA_COLUMN, g_object_ref (file),
			      IS_FOLDER_COLUMN, TRUE,
			      -1);

	  g_free (label);
	  if (icon)
	    g_object_unref (icon);
	}

      button->n_bookmarks++;
      pos++;
    }

  if (button->n_bookmarks > 0 &&
      !button->has_bookmark_separator)
    {
      pos = model_get_type_position (button, ROW_TYPE_BOOKMARK_SEPARATOR);

      gtk_list_store_insert (store, &iter, pos);
      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, NULL,
			  TYPE_COLUMN, ROW_TYPE_BOOKMARK_SEPARATOR,
			  DATA_COLUMN, NULL,
			  IS_FOLDER_COLUMN, FALSE,
			  -1);
      button->has_bookmark_separator = TRUE;
    }
}

static void
model_update_current_folder (GtkFileChooserButton *button,
			     GFile                *file)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint pos;

  if (!file)
    return;

  store = GTK_LIST_STORE (button->model);

  if (!button->has_current_folder_separator)
    {
      pos = model_get_type_position (button, ROW_TYPE_CURRENT_FOLDER_SEPARATOR);
      gtk_list_store_insert (store, &iter, pos);
      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, NULL,
			  TYPE_COLUMN, ROW_TYPE_CURRENT_FOLDER_SEPARATOR,
			  DATA_COLUMN, NULL,
			  IS_FOLDER_COLUMN, FALSE,
			  -1);
      button->has_current_folder_separator = TRUE;
    }

  pos = model_get_type_position (button, ROW_TYPE_CURRENT_FOLDER);
  if (!button->has_current_folder)
    {
      gtk_list_store_insert (store, &iter, pos);
      button->has_current_folder = TRUE;
    }
  else
    {
      gtk_tree_model_iter_nth_child (button->model, &iter, NULL, pos);
      model_free_row_data (button, &iter);
    }

  if (g_file_is_native (file))
    {
      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
			  TYPE_COLUMN, ROW_TYPE_CURRENT_FOLDER,
			  DATA_COLUMN, g_object_ref (file),
			  IS_FOLDER_COLUMN, FALSE,
			  -1);
      set_info_for_file_at_iter (button, file, &iter);
    }
  else
    {
      gchar *label;
      GIcon *icon;

      /* Don't call get_info for remote paths to avoid latency and
       * auth dialogs.
       * If we switch to a better bookmarks file format (XBEL), we
       * should use mime info to get a better icon.
       */
      label = _gtk_bookmarks_manager_get_bookmark_label (button->bookmarks_manager, file);
      if (!label)
	label = _gtk_file_chooser_label_for_file (file);

      if (g_file_is_native (file))
	icon = g_themed_icon_new ("folder");
      else
	icon = g_themed_icon_new ("folder-remote");

      gtk_list_store_set (store, &iter,
                          ICON_COLUMN, icon,
                          DISPLAY_NAME_COLUMN, label,
                          TYPE_COLUMN, ROW_TYPE_CURRENT_FOLDER,
                          DATA_COLUMN, g_object_ref (file),
                          IS_FOLDER_COLUMN, TRUE,
                          -1);

      g_free (label);
      if (icon)
	g_object_unref (icon);
    }
}

static void
model_add_other (GtkFileChooserButton *button)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint pos;
  GIcon *icon;

  store = GTK_LIST_STORE (button->model);
  pos = model_get_type_position (button, ROW_TYPE_OTHER_SEPARATOR);
  icon = g_themed_icon_new ("document-open-symbolic");

  gtk_list_store_insert (store, &iter, pos);
  gtk_list_store_set (store, &iter,
                      ICON_COLUMN, icon,
                      DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
                      TYPE_COLUMN, ROW_TYPE_OTHER_SEPARATOR,
                      DATA_COLUMN, NULL,
                      IS_FOLDER_COLUMN, FALSE,
                      -1);
  button->has_other_separator = TRUE;
  pos++;

  gtk_list_store_insert (store, &iter, pos);
  gtk_list_store_set (store, &iter,
		      ICON_COLUMN, NULL,
		      DISPLAY_NAME_COLUMN, _("Other…"),
		      TYPE_COLUMN, ROW_TYPE_OTHER,
		      DATA_COLUMN, NULL,
		      IS_FOLDER_COLUMN, FALSE,
		      -1);

  g_object_unref (icon);
}

static void
model_remove_rows (GtkFileChooserButton *button,
		   gint                  pos,
		   gint                  n_rows)
{
  GtkListStore *store;

  if (!n_rows)
    return;

  store = GTK_LIST_STORE (button->model);

  do
    {
      GtkTreeIter iter;

      if (!gtk_tree_model_iter_nth_child (button->model, &iter, NULL, pos))
	g_assert_not_reached ();

      model_free_row_data (button, &iter);
      gtk_list_store_remove (store, &iter);
      n_rows--;
    }
  while (n_rows);
}

/* Filter Model */
static gboolean
test_if_file_is_visible (GtkFileSystem *fs,
			 GFile         *file,
			 gboolean       is_folder)
{
  if (!file)
    return FALSE;

  if (!is_folder)
    return FALSE;

  return TRUE;
}

static gboolean
filter_model_visible_func (GtkTreeModel *model,
			   GtkTreeIter  *iter,
			   gpointer      user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  gchar type;
  gpointer data;
  gboolean retval, is_folder;

  type = ROW_TYPE_INVALID;
  data = NULL;

  gtk_tree_model_get (model, iter,
		      TYPE_COLUMN, &type,
		      DATA_COLUMN, &data,
		      IS_FOLDER_COLUMN, &is_folder,
		      -1);

  switch (type)
    {
    case ROW_TYPE_CURRENT_FOLDER:
      retval = TRUE;
      break;
    case ROW_TYPE_SPECIAL:
    case ROW_TYPE_SHORTCUT:
    case ROW_TYPE_BOOKMARK:
      retval = test_if_file_is_visible (button->fs, data, is_folder);
      break;
    case ROW_TYPE_VOLUME:
      retval = TRUE;
      break;
    default:
      retval = TRUE;
      break;
    }

  return retval;
}

/* Combo Box */
static void
name_cell_data_func (GtkCellLayout   *layout,
		     GtkCellRenderer *cell,
		     GtkTreeModel    *model,
		     GtkTreeIter     *iter,
		     gpointer         user_data)
{
  gchar type;

  type = 0;
  gtk_tree_model_get (model, iter,
		      TYPE_COLUMN, &type,
		      -1);

  if (type == ROW_TYPE_CURRENT_FOLDER)
    g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  else if (type == ROW_TYPE_BOOKMARK || type == ROW_TYPE_SHORTCUT)
    g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
  else
    g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_NONE, NULL);
}

static gboolean
combo_box_row_separator_func (GtkTreeModel *model,
			      GtkTreeIter  *iter,
			      gpointer      user_data)
{
  gchar type = ROW_TYPE_INVALID;

  gtk_tree_model_get (model, iter, TYPE_COLUMN, &type, -1);

  return (type == ROW_TYPE_BOOKMARK_SEPARATOR ||
	  type == ROW_TYPE_CURRENT_FOLDER_SEPARATOR ||
	  type == ROW_TYPE_OTHER_SEPARATOR);
}

static void
select_combo_box_row_no_notify (GtkFileChooserButton *button, int pos)
{
  GtkTreeIter iter, filter_iter;

  gtk_tree_model_iter_nth_child (button->model, &iter, NULL, pos);
  gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (button->filter_model),
						    &filter_iter, &iter);

  g_signal_handlers_block_by_func (button->combo_box, combo_box_changed_cb, button);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (button->combo_box), &filter_iter);
  g_signal_handlers_unblock_by_func (button->combo_box, combo_box_changed_cb, button);
}

static void
update_combo_box (GtkFileChooserButton *button)
{
  GFile *file;
  GtkTreeIter iter;
  gboolean row_found;

  file = get_selected_file (button);

  row_found = FALSE;

  gtk_tree_model_get_iter_first (button->filter_model, &iter);

  do
    {
      gchar type;
      gpointer data;

      type = ROW_TYPE_INVALID;
      data = NULL;

      gtk_tree_model_get (button->filter_model, &iter,
			  TYPE_COLUMN, &type,
			  DATA_COLUMN, &data,
			  -1);

      switch (type)
	{
	case ROW_TYPE_SPECIAL:
	case ROW_TYPE_SHORTCUT:
	case ROW_TYPE_BOOKMARK:
	case ROW_TYPE_CURRENT_FOLDER:
	  row_found = (file && g_file_equal (data, file));
	  break;
	case ROW_TYPE_VOLUME:
	  {
	    GFile *base_file;

	    base_file = _gtk_file_system_volume_get_root (data);
            if (base_file)
              {
	        row_found = (file && g_file_equal (base_file, file));
		g_object_unref (base_file);
              }
	  }
	  break;
	default:
	  row_found = FALSE;
	  break;
	}

      if (row_found)
	{
	  g_signal_handlers_block_by_func (button->combo_box, combo_box_changed_cb, button);
	  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (button->combo_box),
					 &iter);
	  g_signal_handlers_unblock_by_func (button->combo_box, combo_box_changed_cb, button);
	}
    }
  while (!row_found && gtk_tree_model_iter_next (button->filter_model, &iter));

  if (!row_found)
    {
      gint pos;

      /* If it hasn't been found already, update & select the current-folder row. */
      if (file)
	{
	  model_update_current_folder (button, file);
	  pos = model_get_type_position (button, ROW_TYPE_CURRENT_FOLDER);
	}
      else
	{
	  /* No selection; switch to that row */

	  pos = model_get_type_position (button, ROW_TYPE_OTHER_SEPARATOR);
	}

      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (button->filter_model));

      select_combo_box_row_no_notify (button, pos);
    }

  if (file)
    g_object_unref (file);
}

/* Button */
static void
update_label_get_info_cb (GCancellable *cancellable,
			  GFileInfo    *info,
			  const GError *error,
			  gpointer      data)
{
  gboolean cancelled = g_cancellable_is_cancelled (cancellable);
  GIcon *icon;
  GtkFileChooserButton *button = data;

  if (cancellable != button->update_button_cancellable)
    goto out;

  button->update_button_cancellable = NULL;

  if (cancelled || error)
    goto out;

  gtk_label_set_text (GTK_LABEL (button->label), g_file_info_get_display_name (info));

  icon = _gtk_file_info_get_icon (info, ICON_SIZE, gtk_widget_get_scale_factor (GTK_WIDGET (button)));
  gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon);
  gtk_image_set_pixel_size (GTK_IMAGE (button->image), ICON_SIZE);
  if (icon)
    g_object_unref (icon);

out:
  emit_selection_changed_if_changing_selection (button);

  g_object_unref (button);
  g_object_unref (cancellable);
}

static void
update_label_and_image (GtkFileChooserButton *button)
{
  gchar *label_text;
  GFile *file;
  gboolean done_changing_selection;

  file = get_selected_file (button);

  label_text = NULL;
  done_changing_selection = FALSE;

  if (button->update_button_cancellable)
    {
      g_cancellable_cancel (button->update_button_cancellable);
      button->update_button_cancellable = NULL;
    }

  if (file)
    {
      GtkFileSystemVolume *volume = NULL;

      volume = _gtk_file_system_get_volume_for_file (button->fs, file);
      if (volume)
        {
          GFile *base_file;

          base_file = _gtk_file_system_volume_get_root (volume);
          if (base_file && g_file_equal (base_file, file))
            {
              GIcon *icon;

              label_text = _gtk_file_system_volume_get_display_name (volume);
              icon = _gtk_file_system_volume_get_icon (volume);
              gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon);
              gtk_image_set_pixel_size (GTK_IMAGE (button->image), ICON_SIZE);
              if (icon)
                g_object_unref (icon);
            }

          if (base_file)
            g_object_unref (base_file);

          _gtk_file_system_volume_unref (volume);

          if (label_text)
	    {
	      done_changing_selection = TRUE;
	      goto out;
	    }
        }

      if (g_file_is_native (file))
        {
          button->update_button_cancellable =
            _gtk_file_system_get_info (button->fs, file,
                                       "standard::icon,standard::display-name",
                                       update_label_get_info_cb,
                                       g_object_ref (button));
        }
      else
        {
          GIcon *icon;

          label_text = _gtk_bookmarks_manager_get_bookmark_label (button->bookmarks_manager, file);
          icon = g_themed_icon_new ("text-x-generic");
          gtk_image_set_from_gicon (GTK_IMAGE (button->image), icon);
          gtk_image_set_pixel_size (GTK_IMAGE (button->image), ICON_SIZE);
          if (icon)
            g_object_unref (icon);

	  done_changing_selection = TRUE;
        }
    }
  else
    {
      /* We know the selection is empty */
      done_changing_selection = TRUE;
    }

out:

  if (file)
    g_object_unref (file);

  if (label_text)
    {
      gtk_label_set_text (GTK_LABEL (button->label), label_text);
      g_free (label_text);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (button->label), _(FALLBACK_DISPLAY_NAME));
      gtk_image_set_from_gicon (GTK_IMAGE (button->image), NULL);
    }

  if (done_changing_selection)
    emit_selection_changed_if_changing_selection (button);
}


/* ************************ *
 *  Child Object Callbacks  *
 * ************************ */

/* File System */
static void
fs_volumes_changed_cb (GtkFileSystem *fs,
		       gpointer       user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  GSList *volumes;

  model_remove_rows (user_data,
		     model_get_type_position (user_data, ROW_TYPE_VOLUME),
		     button->n_volumes);

  button->n_volumes = 0;

  volumes = _gtk_file_system_list_volumes (fs);
  model_add_volumes (user_data, volumes);
  g_slist_free (volumes);

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (button->filter_model));

  update_label_and_image (user_data);
  update_combo_box (user_data);
}

static void
bookmarks_changed_cb (gpointer user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  GSList *bookmarks;

  bookmarks = _gtk_bookmarks_manager_list_bookmarks (button->bookmarks_manager);
  model_remove_rows (user_data,
		     model_get_type_position (user_data, ROW_TYPE_BOOKMARK_SEPARATOR),
		     button->n_bookmarks + button->has_bookmark_separator);
  button->has_bookmark_separator = FALSE;
  button->n_bookmarks = 0;
  model_add_bookmarks (user_data, bookmarks);
  g_slist_free_full (bookmarks, g_object_unref);

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (button->filter_model));

  update_label_and_image (user_data);
  update_combo_box (user_data);
}

static void
save_inactive_state (GtkFileChooserButton *button)
{
  if (button->current_folder_while_inactive)
    g_object_unref (button->current_folder_while_inactive);

  if (button->selection_while_inactive)
    g_object_unref (button->selection_while_inactive);

  button->current_folder_while_inactive = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (button->chooser));
  button->selection_while_inactive = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button->chooser));
}

static void
restore_inactive_state (GtkFileChooserButton *button)
{
  if (button->current_folder_while_inactive)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (button->chooser), button->current_folder_while_inactive, NULL);

  if (button->selection_while_inactive)
    gtk_file_chooser_select_file (GTK_FILE_CHOOSER (button->chooser), button->selection_while_inactive, NULL);
  else
    gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (button->chooser));
}

/* Dialog */
static void
open_dialog (GtkFileChooserButton *button)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));

  /* Setup the dialog parent to be chooser button's toplevel, and be modal
     as needed. */
  if (button->dialog != NULL)
    {
      if (!gtk_widget_get_visible (button->dialog))
        {
          if (GTK_IS_WINDOW (toplevel))
            {
              if (GTK_WINDOW (toplevel) != gtk_window_get_transient_for (GTK_WINDOW (button->dialog)))
                gtk_window_set_transient_for (GTK_WINDOW (button->dialog),
                                              GTK_WINDOW (toplevel));
              if (gtk_window_get_modal (GTK_WINDOW (toplevel)))
                gtk_window_set_modal (GTK_WINDOW (button->dialog), TRUE);
            }
        }
    }
  else
    {
      if (!gtk_native_dialog_get_visible (GTK_NATIVE_DIALOG (button->native)))
        {
          if (GTK_IS_WINDOW (toplevel))
            {
              if (GTK_WINDOW (toplevel) != gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (button->native)))
                gtk_native_dialog_set_transient_for (GTK_NATIVE_DIALOG (button->native),
                                                     GTK_WINDOW (toplevel));

              if (gtk_window_get_modal (GTK_WINDOW (toplevel)))
                gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (button->native), TRUE);
            }
        }
    }

  if (!button->active)
    {
      restore_inactive_state (button);
      button->active = TRUE;
    }

  gtk_widget_set_sensitive (button->combo_box, FALSE);
  if (button->dialog)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_window_present (GTK_WINDOW (button->dialog));
      G_GNUC_END_IGNORE_DEPRECATIONS
    }
  else
    gtk_native_dialog_show (GTK_NATIVE_DIALOG (button->native));
}

/* Combo Box */
static void
combo_box_changed_cb (GtkComboBox *combo_box,
		      gpointer     user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  GtkTreeIter iter;
  gboolean file_was_set;

  file_was_set = FALSE;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      gchar type;
      gpointer data;

      type = ROW_TYPE_INVALID;
      data = NULL;

      gtk_tree_model_get (button->filter_model, &iter,
			  TYPE_COLUMN, &type,
			  DATA_COLUMN, &data,
			  -1);

      switch (type)
	{
	case ROW_TYPE_SPECIAL:
	case ROW_TYPE_SHORTCUT:
	case ROW_TYPE_BOOKMARK:
	case ROW_TYPE_CURRENT_FOLDER:
	  if (data)
	    {
	      gtk_file_chooser_button_select_file (GTK_FILE_CHOOSER (button), data, NULL);
	      file_was_set = TRUE;
	    }
	  break;
	case ROW_TYPE_VOLUME:
	  {
	    GFile *base_file;

	    base_file = _gtk_file_system_volume_get_root (data);
	    if (base_file)
	      {
		gtk_file_chooser_button_select_file (GTK_FILE_CHOOSER (button), base_file, NULL);
		file_was_set = TRUE;
		g_object_unref (base_file);
	      }
	  }
	  break;
	case ROW_TYPE_OTHER:
	  open_dialog (user_data);
	  break;
	default:
	  break;
	}
    }

  if (file_was_set)
    g_signal_emit (button, file_chooser_button_signals[FILE_SET], 0);
}

/* Button */
static void
button_clicked_cb (GtkButton *real_button,
		   gpointer   user_data)
{
  open_dialog (user_data);
}

/* Dialog */

static void
common_response_cb (GtkFileChooserButton *button,
		    gint       response)
{
  if (response == GTK_RESPONSE_ACCEPT ||
      response == GTK_RESPONSE_OK)
    {
      save_inactive_state (button);

      g_signal_emit_by_name (button, "current-folder-changed");
      g_signal_emit_by_name (button, "selection-changed");
    }
  else
    {
      restore_inactive_state (button);
    }

  if (button->active)
    button->active = FALSE;

  update_label_and_image (button);
  update_combo_box (button);

  gtk_widget_set_sensitive (button->combo_box, TRUE);
}


static void
dialog_response_cb (GtkDialog *dialog,
		    gint       response,
		    gpointer   user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);

  common_response_cb (button, response);

  gtk_widget_hide (button->dialog);

  if (response == GTK_RESPONSE_ACCEPT ||
      response == GTK_RESPONSE_OK)
    g_signal_emit (button, file_chooser_button_signals[FILE_SET], 0);
}

static void
native_response_cb (GtkFileChooserNative *native,
		    gint       response,
		    gpointer   user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);

  common_response_cb (button, response);

  /* dialog already hidden */

  if (response == GTK_RESPONSE_ACCEPT ||
      response == GTK_RESPONSE_OK)
    g_signal_emit (button, file_chooser_button_signals[FILE_SET], 0);
}


/* ************************************************************************** *
 *  Public API                                                                *
 * ************************************************************************** */

/**
 * gtk_file_chooser_button_new:
 * @title: the title of the browse dialog.
 * @action: the open mode for the widget.
 *
 * Creates a new file-selecting button widget.
 *
 * Returns: a new button widget.
 */
GtkWidget *
gtk_file_chooser_button_new (const gchar          *title,
			     GtkFileChooserAction  action)
{
  g_return_val_if_fail (action == GTK_FILE_CHOOSER_ACTION_OPEN ||
			action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL);

  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON,
		       "action", action,
		       "title", (title ? title : _(DEFAULT_TITLE)),
		       NULL);
}

/**
 * gtk_file_chooser_button_new_with_dialog:
 * @dialog: (type Gtk.Dialog): the widget to use as dialog
 *
 * Creates a #GtkFileChooserButton widget which uses @dialog as its
 * file-picking window.
 *
 * Note that @dialog must be a #GtkDialog (or subclass) which
 * implements the #GtkFileChooser interface and must not have
 * %GTK_DIALOG_DESTROY_WITH_PARENT set.
 *
 * Also note that the dialog needs to have its confirmative button
 * added with response %GTK_RESPONSE_ACCEPT or %GTK_RESPONSE_OK in
 * order for the button to take over the file selected in the dialog.
 *
 * Returns: a new button widget.
 */
GtkWidget *
gtk_file_chooser_button_new_with_dialog (GtkWidget *dialog)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (dialog) && GTK_IS_DIALOG (dialog), NULL);

  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON,
		       "dialog", dialog,
		       NULL);
}

/**
 * gtk_file_chooser_button_set_title:
 * @button: the button widget to modify.
 * @title: the new browse dialog title.
 *
 * Modifies the @title of the browse dialog used by @button.
 */
void
gtk_file_chooser_button_set_title (GtkFileChooserButton *button,
				   const gchar          *title)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  if (button->dialog)
    gtk_window_set_title (GTK_WINDOW (button->dialog), title);
  else
    gtk_native_dialog_set_title (GTK_NATIVE_DIALOG (button->native), title);
  g_object_notify (G_OBJECT (button), "title");
}

/**
 * gtk_file_chooser_button_get_title:
 * @button: the button widget to examine.
 *
 * Retrieves the title of the browse dialog used by @button. The returned value
 * should not be modified or freed.
 *
 * Returns: a pointer to the browse dialog’s title.
 */
const gchar *
gtk_file_chooser_button_get_title (GtkFileChooserButton *button)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), NULL);

  if (button->dialog)
    return gtk_window_get_title (GTK_WINDOW (button->dialog));
  else
    return gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (button->native));
}

/**
 * gtk_file_chooser_button_get_width_chars:
 * @button: the button widget to examine.
 *
 * Retrieves the width in characters of the @button widget’s entry and/or label.
 *
 * Returns: an integer width (in characters) that the button will use to size itself.
 */
gint
gtk_file_chooser_button_get_width_chars (GtkFileChooserButton *button)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), -1);

  return gtk_label_get_width_chars (GTK_LABEL (button->label));
}

/**
 * gtk_file_chooser_button_set_width_chars:
 * @button: the button widget to examine.
 * @n_chars: the new width, in characters.
 *
 * Sets the width (in characters) that @button will use to @n_chars.
 */
void
gtk_file_chooser_button_set_width_chars (GtkFileChooserButton *button,
					 gint                  n_chars)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  gtk_label_set_width_chars (GTK_LABEL (button->label), n_chars);
  g_object_notify (G_OBJECT (button), "width-chars");
}

/**
 * gtk_file_chooser_button_set_modal:
 * @button: a #GtkFileChooserButton
 * @modal: %TRUE to make the dialog modal
 *
 * Sets whether the dialog should be modal.
 */
void
gtk_file_chooser_button_set_modal (GtkFileChooserButton *button,
                                   gboolean              modal)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  g_object_set (button, "modal", modal, NULL);
}

/**
 * gtk_file_chooser_button_get_modal:
 * @button: a #GtkFileChooserButton
 *
 * Gets whether the dialog is modal.
 *
 * Returns: %TRUE if the dialog is modal
 */
gboolean
gtk_file_chooser_button_get_modal (GtkFileChooserButton *button)
{
  gboolean modal;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), FALSE);

  g_object_get (button, "modal", &modal, NULL);

  return modal;
}
