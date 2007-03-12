/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>

#include "gtkintl.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcombobox.h"
#include "gtkdnd.h"
#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkstock.h"
#include "gtktreemodelfilter.h"
#include "gtkvseparator.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserutils.h"

#include "gtkfilechooserbutton.h"

#ifdef G_OS_WIN32
#include "gtkfilesystemwin32.h"
#endif

#include "gtkprivate.h"
#include "gtkalias.h"

/* **************** *
 *  Private Macros  *
 * **************** */

#define GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_FILE_CHOOSER_BUTTON, GtkFileChooserButtonPrivate))

#define DEFAULT_TITLE		N_("Select A File")
#define DESKTOP_DISPLAY_NAME	N_("Desktop")
#define FALLBACK_DISPLAY_NAME	N_("(None)")
#define FALLBACK_ICON_NAME	"stock_unknown"
#define FALLBACK_ICON_SIZE	16


/* ********************** *
 *  Private Enumerations  *
 * ********************** */

/* Property IDs */
enum
{
  PROP_0,

  PROP_DIALOG,
  PROP_FOCUS_ON_CLICK,
  PROP_TITLE,
  PROP_WIDTH_CHARS
};

/* TreeModel Columns */
enum
{
  ICON_COLUMN,
  DISPLAY_NAME_COLUMN,
  TYPE_COLUMN,
  DATA_COLUMN,
  IS_FOLDER_COLUMN,
  HANDLE_COLUMN,
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

struct _GtkFileChooserButtonPrivate
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *combo_box;
  GtkCellRenderer *icon_cell;
  GtkCellRenderer *name_cell;

  GtkTreeModel *model;
  GtkTreeModel *filter_model;

  gchar *backend;
  GtkFileSystem *fs;
  GtkFilePath *old_path;

  gulong combo_box_changed_id;
  gulong dialog_file_activated_id;
  gulong dialog_folder_changed_id;
  gulong dialog_selection_changed_id;
  gulong fs_volumes_changed_id;
  gulong fs_bookmarks_changed_id;

  GtkFileSystemHandle *dnd_select_folder_handle;
  GtkFileSystemHandle *update_button_handle;
  GSList *change_icon_theme_handles;

  gint icon_size;

  guint8 n_special;
  guint8 n_volumes;
  guint8 n_shortcuts;
  guint8 n_bookmarks;
  guint8 has_bookmark_separator       : 1;
  guint8 has_current_folder_separator : 1;
  guint8 has_current_folder           : 1;
  guint8 has_other_separator          : 1;

  /* Used for hiding/showing the dialog when the button is hidden */
  guint8 active                       : 1;

  /* Used to remember whether a title has been set yet, so we can use the default if it has not been set. */
  guint8 has_title                    : 1;

  /* Used to track whether we need to set a default current folder on ::map() */
  guint8 folder_has_been_set          : 1;

  guint8 focus_on_click               : 1;
};


/* ************* *
 *  DnD Support  *
 * ************* */

enum
{
  TEXT_PLAIN,
  TEXT_URI_LIST
};


/* ********************* *
 *  Function Prototypes  *
 * ********************* */

/* GtkFileChooserIface Functions */
static void     gtk_file_chooser_button_file_chooser_iface_init (GtkFileChooserIface *iface);
static gboolean gtk_file_chooser_button_add_shortcut_folder     (GtkFileChooser      *chooser,
								 const GtkFilePath   *path,
								 GError             **error);
static gboolean gtk_file_chooser_button_remove_shortcut_folder  (GtkFileChooser      *chooser,
								 const GtkFilePath   *path,
								 GError             **error);

/* GObject Functions */
static GObject *gtk_file_chooser_button_constructor        (GType             type,
							    guint             n_params,
							    GObjectConstructParam *params);
static void     gtk_file_chooser_button_set_property       (GObject          *object,
							    guint             param_id,
							    const GValue     *value,
							    GParamSpec       *pspec);
static void     gtk_file_chooser_button_get_property       (GObject          *object,
							    guint             param_id,
							    GValue           *value,
							    GParamSpec       *pspec);
static void     gtk_file_chooser_button_finalize           (GObject          *object);

/* GtkObject Functions */
static void     gtk_file_chooser_button_destroy            (GtkObject        *object);

/* GtkWidget Functions */
static void     gtk_file_chooser_button_drag_data_received (GtkWidget        *widget,
							    GdkDragContext   *context,
							    gint              x,
							    gint              y,
							    GtkSelectionData *data,
							    guint             info,
							    guint             drag_time);
static void     gtk_file_chooser_button_show_all           (GtkWidget        *widget);
static void     gtk_file_chooser_button_hide_all           (GtkWidget        *widget);
static void     gtk_file_chooser_button_show               (GtkWidget        *widget);
static void     gtk_file_chooser_button_hide               (GtkWidget        *widget);
static void     gtk_file_chooser_button_map                (GtkWidget        *widget);
static gboolean gtk_file_chooser_button_mnemonic_activate  (GtkWidget        *widget,
							    gboolean          group_cycling);
static void     gtk_file_chooser_button_style_set          (GtkWidget        *widget,
							    GtkStyle         *old_style);
static void     gtk_file_chooser_button_screen_changed     (GtkWidget        *widget,
							    GdkScreen        *old_screen);

/* Utility Functions */
static GtkIconTheme *get_icon_theme               (GtkWidget            *widget);
static void          set_info_for_path_at_iter         (GtkFileChooserButton *fs,
							const GtkFilePath    *path,
							GtkTreeIter          *iter);

static gint          model_get_type_position      (GtkFileChooserButton *button,
						   RowType               row_type);
static void          model_free_row_data          (GtkFileChooserButton *button,
						   GtkTreeIter          *iter);
static inline void   model_add_special            (GtkFileChooserButton *button);
static inline void   model_add_other              (GtkFileChooserButton *button);
static void          model_add_volumes            (GtkFileChooserButton *button,
						   GSList               *volumes);
static void          model_add_bookmarks          (GtkFileChooserButton *button,
						   GSList               *bookmarks);
static void          model_update_current_folder  (GtkFileChooserButton *button,
						   const GtkFilePath    *path);
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
static void     fs_bookmarks_changed_cb          (GtkFileSystem  *fs,
						  gpointer        user_data);

static void     combo_box_changed_cb             (GtkComboBox    *combo_box,
						  gpointer        user_data);

static void     button_clicked_cb                (GtkButton      *real_button,
						  gpointer        user_data);

static void     dialog_update_preview_cb         (GtkFileChooser *dialog,
						  gpointer        user_data);
static void     dialog_selection_changed_cb      (GtkFileChooser *dialog,
						  gpointer        user_data);
static void     dialog_file_activated_cb         (GtkFileChooser *dialog,
						  gpointer        user_data);
static void     dialog_current_folder_changed_cb (GtkFileChooser *dialog,
						  gpointer        user_data);
static void     dialog_notify_cb                 (GObject        *dialog,
						  GParamSpec     *pspec,
						  gpointer        user_data);
static gboolean dialog_delete_event_cb           (GtkWidget      *dialog,
						  GdkEvent       *event,
						  gpointer        user_data);
static void     dialog_response_cb               (GtkDialog      *dialog,
						  gint            response,
						  gpointer        user_data);


/* ******************* *
 *  GType Declaration  *
 * ******************* */

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserButton, gtk_file_chooser_button, GTK_TYPE_HBOX, { \
    G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER, gtk_file_chooser_button_file_chooser_iface_init) \
})


/* ***************** *
 *  GType Functions  *
 * ***************** */

static void
gtk_file_chooser_button_class_init (GtkFileChooserButtonClass * class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *gtkobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  gtkobject_class = GTK_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->constructor = gtk_file_chooser_button_constructor;
  gobject_class->set_property = gtk_file_chooser_button_set_property;
  gobject_class->get_property = gtk_file_chooser_button_get_property;
  gobject_class->finalize = gtk_file_chooser_button_finalize;

  gtkobject_class->destroy = gtk_file_chooser_button_destroy;

  widget_class->drag_data_received = gtk_file_chooser_button_drag_data_received;
  widget_class->show_all = gtk_file_chooser_button_show_all;
  widget_class->hide_all = gtk_file_chooser_button_hide_all;
  widget_class->show = gtk_file_chooser_button_show;
  widget_class->hide = gtk_file_chooser_button_hide;
  widget_class->map = gtk_file_chooser_button_map;
  widget_class->style_set = gtk_file_chooser_button_style_set;
  widget_class->screen_changed = gtk_file_chooser_button_screen_changed;
  widget_class->mnemonic_activate = gtk_file_chooser_button_mnemonic_activate;

  /**
   * GtkFileChooserButton:dialog:
   * 
   * Instance of the #GtkFileChooserDialog associated with the button.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class, PROP_DIALOG,
				   g_param_spec_object ("dialog",
							P_("Dialog"),
							P_("The file chooser dialog to use."),
							GTK_TYPE_FILE_CHOOSER,
							(GTK_PARAM_WRITABLE |
							 G_PARAM_CONSTRUCT_ONLY)));

  /**
   * GtkFileChooserButton:focus-on-click:
   * 
   * Whether the #GtkFileChooserButton button grabs focus when it is clicked
   * with the mouse.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FOCUS_ON_CLICK,
                                   g_param_spec_boolean ("focus-on-click",
							 P_("Focus on click"),
							 P_("Whether the button grabs focus when it is clicked with the mouse"),
							 TRUE,
							 GTK_PARAM_READWRITE));
  
  /**
   * GtkFileChooserButton:title:
   * 
   * Title to put on the #GtkFileChooserDialog associated with the button.
   *
   * Since: 2.6
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
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class, PROP_WIDTH_CHARS,
				   g_param_spec_int ("width-chars",
						     P_("Width In Characters"),
						     P_("The desired width of the button widget, in characters."),
						     -1, G_MAXINT, -1,
						     GTK_PARAM_READWRITE));

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserButtonPrivate));
}

static void
gtk_file_chooser_button_init (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GtkWidget *box, *image, *sep;
  GtkTargetList *target_list;

  priv = button->priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (button);

  priv->icon_size = FALLBACK_ICON_SIZE;
  priv->focus_on_click = TRUE;

  gtk_widget_push_composite_child ();

  /* Button */
  priv->button = gtk_button_new ();
  g_signal_connect (priv->button, "clicked", G_CALLBACK (button_clicked_cb),
		    button);
  gtk_container_add (GTK_CONTAINER (button), priv->button);
  gtk_widget_show (priv->button);

  box = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (priv->button), box);
  gtk_widget_show (box);

  priv->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), priv->image, FALSE, FALSE, 0);
  gtk_widget_show (priv->image);

  priv->label = gtk_label_new (_(FALLBACK_DISPLAY_NAME));
  gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (box), priv->label);
  gtk_widget_show (priv->label);

  sep = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), sep, FALSE, FALSE, 0);
  gtk_widget_show (sep);

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
				    GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  /* Combo Box */
  /* Keep in sync with columns enum, line 88 */
  priv->model =
    GTK_TREE_MODEL (gtk_list_store_new (NUM_COLUMNS,
					GDK_TYPE_PIXBUF, /* Icon */
					G_TYPE_STRING,	 /* Display Name */
					G_TYPE_CHAR,	 /* Row Type */
					G_TYPE_POINTER	 /* Volume || Path */,
					G_TYPE_BOOLEAN   /* Is Folder? */,
					G_TYPE_POINTER	 /* handle */));

  priv->combo_box = gtk_combo_box_new ();
  priv->combo_box_changed_id =
    g_signal_connect (priv->combo_box, "changed",
		      G_CALLBACK (combo_box_changed_cb), button);
  gtk_container_add (GTK_CONTAINER (button), priv->combo_box);

  priv->icon_cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->combo_box),
			      priv->icon_cell, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->combo_box),
				 priv->icon_cell, "pixbuf", ICON_COLUMN);

  priv->name_cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->combo_box),
			      priv->name_cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->combo_box),
				 priv->name_cell, "text", DISPLAY_NAME_COLUMN);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->combo_box),
				      priv->name_cell, name_cell_data_func,
				      NULL, NULL);

  gtk_widget_pop_composite_child ();

  /* DnD */
  gtk_drag_dest_set (GTK_WIDGET (button),
                     (GTK_DEST_DEFAULT_ALL),
		     NULL, 0,
		     GDK_ACTION_COPY);
  target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, TEXT_URI_LIST);
  gtk_target_list_add_text_targets (target_list, TEXT_PLAIN);
  gtk_drag_dest_set_target_list (GTK_WIDGET (button), target_list);
  gtk_target_list_unref (target_list);
}


/* ******************************* *
 *  GtkFileChooserIface Functions  *
 * ******************************* */
static void
gtk_file_chooser_button_file_chooser_iface_init (GtkFileChooserIface *iface)
{
  _gtk_file_chooser_delegate_iface_init (iface);

  iface->add_shortcut_folder = gtk_file_chooser_button_add_shortcut_folder;
  iface->remove_shortcut_folder = gtk_file_chooser_button_remove_shortcut_folder;
}

static gboolean
gtk_file_chooser_button_add_shortcut_folder (GtkFileChooser     *chooser,
					     const GtkFilePath  *path,
					     GError            **error)
{
  GtkFileChooser *delegate;
  gboolean retval;

  delegate = g_object_get_qdata (G_OBJECT (chooser),
				 GTK_FILE_CHOOSER_DELEGATE_QUARK);
  retval = _gtk_file_chooser_add_shortcut_folder (delegate, path, error);

  if (retval)
    {
      GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);
      GtkFileChooserButtonPrivate *priv = button->priv;
      GtkTreeIter iter;
      gint pos;

      pos = model_get_type_position (button, ROW_TYPE_SHORTCUT);
      pos += priv->n_shortcuts;

      gtk_list_store_insert (GTK_LIST_STORE (priv->model), &iter, pos);
      gtk_list_store_set (GTK_LIST_STORE (priv->model), &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
			  TYPE_COLUMN, ROW_TYPE_SHORTCUT,
			  DATA_COLUMN, gtk_file_path_copy (path),
			  IS_FOLDER_COLUMN, FALSE,
			  -1);
      set_info_for_path_at_iter (button, path, &iter);
      priv->n_shortcuts++;

      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));
    }

  return retval;
}

static gboolean
gtk_file_chooser_button_remove_shortcut_folder (GtkFileChooser     *chooser,
						const GtkFilePath  *path,
						GError            **error)
{
  GtkFileChooser *delegate;
  gboolean retval;

  delegate = g_object_get_qdata (G_OBJECT (chooser),
				 GTK_FILE_CHOOSER_DELEGATE_QUARK);

  retval = _gtk_file_chooser_remove_shortcut_folder (delegate, path, error);

  if (retval)
    {
      GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (chooser);
      GtkFileChooserButtonPrivate *priv = button->priv;
      GtkTreeIter iter;
      gint pos;
      gchar type;

      pos = model_get_type_position (button, ROW_TYPE_SHORTCUT);
      gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, pos);

      do
	{
	  gpointer data;

	  gtk_tree_model_get (priv->model, &iter,
			      TYPE_COLUMN, &type,
			      DATA_COLUMN, &data,
			      -1);

	  if (type == ROW_TYPE_SHORTCUT &&
	      data &&
	      gtk_file_path_compare (data, path) == 0)
	    {
	      model_free_row_data (GTK_FILE_CHOOSER_BUTTON (chooser), &iter);
	      gtk_list_store_remove (GTK_LIST_STORE (priv->model), &iter);
	      priv->n_shortcuts--;
	      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));
	      update_combo_box (GTK_FILE_CHOOSER_BUTTON (chooser));
	      break;
	    }
	}
      while (type == ROW_TYPE_SHORTCUT &&
	     gtk_tree_model_iter_next (priv->model, &iter));
    }

  return retval;
}


/* ******************* *
 *  GObject Functions  *
 * ******************* */

static GObject *
gtk_file_chooser_button_constructor (GType                  type,
				     guint                  n_params,
				     GObjectConstructParam *params)
{
  GObject *object;
  GtkFileChooserButton *button;
  GtkFileChooserButtonPrivate *priv;
  GSList *list;
  char *current_folder;

  object = (*G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->constructor) (type,
										  n_params,
										  params);
  button = GTK_FILE_CHOOSER_BUTTON (object);
  priv = button->priv;

  if (!priv->dialog)
    {
      if (priv->backend)
	priv->dialog = gtk_file_chooser_dialog_new_with_backend (NULL, NULL,
								 GTK_FILE_CHOOSER_ACTION_OPEN,
								 priv->backend,
								 GTK_STOCK_CANCEL,
								 GTK_RESPONSE_CANCEL,
								 GTK_STOCK_OPEN,
								 GTK_RESPONSE_ACCEPT,
								 NULL);
      else
	priv->dialog = gtk_file_chooser_dialog_new (NULL, NULL,
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    GTK_STOCK_CANCEL,
						    GTK_RESPONSE_CANCEL,
						    GTK_STOCK_OPEN,
						    GTK_RESPONSE_ACCEPT,
						    NULL);

      gtk_dialog_set_default_response (GTK_DIALOG (priv->dialog),
				       GTK_RESPONSE_ACCEPT);
      gtk_dialog_set_alternative_button_order (GTK_DIALOG (priv->dialog),
					       GTK_RESPONSE_ACCEPT,
					       GTK_RESPONSE_CANCEL,
					       -1);
    }

  /* Set the default title if necessary. We must wait until the dialog has been created to do this. */
  if (!priv->has_title)
    gtk_file_chooser_button_set_title (button, _(DEFAULT_TITLE));

  current_folder = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (priv->dialog));
  if (current_folder != NULL)
    {
      priv->folder_has_been_set = TRUE;
      g_free (current_folder);
    }

  g_free (priv->backend);
  priv->backend = NULL;

  g_signal_connect (priv->dialog, "delete_event",
		    G_CALLBACK (dialog_delete_event_cb), object);
  g_signal_connect (priv->dialog, "response",
		    G_CALLBACK (dialog_response_cb), object);

  /* This is used, instead of the standard delegate, to ensure that signals are only
   * delegated when the OK button is pressed. */
  g_object_set_qdata (object, GTK_FILE_CHOOSER_DELEGATE_QUARK, priv->dialog);
  priv->dialog_folder_changed_id =
    g_signal_connect (priv->dialog, "current-folder-changed",
		      G_CALLBACK (dialog_current_folder_changed_cb), object);
  priv->dialog_file_activated_id =
    g_signal_connect (priv->dialog, "file-activated",
		      G_CALLBACK (dialog_file_activated_cb), object);
  priv->dialog_selection_changed_id =
    g_signal_connect (priv->dialog, "selection-changed",
		      G_CALLBACK (dialog_selection_changed_cb), object);
  g_signal_connect (priv->dialog, "update-preview",
		    G_CALLBACK (dialog_update_preview_cb), object);
  g_signal_connect (priv->dialog, "notify",
		    G_CALLBACK (dialog_notify_cb), object);
  g_object_add_weak_pointer (G_OBJECT (priv->dialog),
			     (gpointer *) (&priv->dialog));

  priv->fs =
    g_object_ref (_gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog)));

  model_add_special (button);

  list = gtk_file_system_list_volumes (priv->fs);
  model_add_volumes (button, list);
  g_slist_free (list);

  list = gtk_file_system_list_bookmarks (priv->fs);
  model_add_bookmarks (button, list);
  gtk_file_paths_free (list);

  model_add_other (button);

  priv->filter_model = gtk_tree_model_filter_new (priv->model, NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter_model),
					  filter_model_visible_func,
					  object, NULL);

  gtk_combo_box_set_model (GTK_COMBO_BOX (priv->combo_box), priv->filter_model);
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (priv->combo_box),
					combo_box_row_separator_func,
					NULL, NULL);

  /* set up the action for a user-provided dialog, this also updates
   * the label, image and combobox
   */
  g_object_set (object, 
		"action", gtk_file_chooser_get_action (GTK_FILE_CHOOSER (priv->dialog)),
		NULL);

  priv->fs_volumes_changed_id =
    g_signal_connect (priv->fs, "volumes-changed",
		      G_CALLBACK (fs_volumes_changed_cb), object);
  priv->fs_bookmarks_changed_id =
    g_signal_connect (priv->fs, "bookmarks-changed",
		      G_CALLBACK (fs_bookmarks_changed_cb), object);

  return object;
}

static void
gtk_file_chooser_button_set_property (GObject      *object,
				      guint         param_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (object);
  GtkFileChooserButtonPrivate *priv = button->priv;

  switch (param_id)
    {
    case PROP_DIALOG:
      /* Construct-only */
      priv->dialog = g_value_get_object (value);
      break;
    case PROP_FOCUS_ON_CLICK:
      gtk_file_chooser_button_set_focus_on_click (button, g_value_get_boolean (value));
      break;
    case PROP_WIDTH_CHARS:
      gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (object),
					       g_value_get_int (value));
      break;
    case GTK_FILE_CHOOSER_PROP_ACTION:
      switch (g_value_get_enum (value))
	{
	case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
	case GTK_FILE_CHOOSER_ACTION_SAVE:
	  {
	    GEnumClass *eclass;
	    GEnumValue *eval;

	    eclass = g_type_class_peek (GTK_TYPE_FILE_CHOOSER_ACTION);
	    eval = g_enum_get_value (eclass, g_value_get_enum (value));
	    g_warning ("%s: Choosers of type `%s' do not support `%s'.",
		       G_STRFUNC, G_OBJECT_TYPE_NAME (object), eval->value_name);

	    g_value_set_enum ((GValue *) value, GTK_FILE_CHOOSER_ACTION_OPEN);
	  }
	  break;
	}

      g_object_set_property (G_OBJECT (priv->dialog), pspec->name, value);
      update_label_and_image (GTK_FILE_CHOOSER_BUTTON (object));
      update_combo_box (GTK_FILE_CHOOSER_BUTTON (object));

      switch (g_value_get_enum (value))
	{
	case GTK_FILE_CHOOSER_ACTION_OPEN:
	  gtk_widget_hide (priv->combo_box);
	  gtk_widget_show (priv->button);
	  break;
	case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  gtk_widget_hide (priv->button);
	  gtk_widget_show (priv->combo_box);
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
      break;

    case PROP_TITLE:
      /* Remember that a title has been set, so we do no try to set it to the default in _init(). */
      priv->has_title = TRUE;
      /* Intentionally fall through instead of breaking here, to actually set the property. */
    case GTK_FILE_CHOOSER_PROP_FILTER:
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
      g_object_set_property (G_OBJECT (priv->dialog), pspec->name, value);
      break;

    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
      g_object_set_property (G_OBJECT (priv->dialog), pspec->name, value);
      fs_volumes_changed_cb (priv->fs, button);
      fs_bookmarks_changed_cb (priv->fs, button);
      break;

    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      /* Construct-only */
      priv->backend = g_value_dup_string (value);
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_warning ("%s: Choosers of type `%s` do not support selecting multiple files.",
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
  GtkFileChooserButtonPrivate *priv = button->priv;

  switch (param_id)
    {
    case PROP_WIDTH_CHARS:
      g_value_set_int (value,
		       gtk_label_get_width_chars (GTK_LABEL (priv->label)));
      break;
    case PROP_FOCUS_ON_CLICK:
      g_value_set_boolean (value,
                           gtk_file_chooser_button_get_focus_on_click (button));
      break;

    case PROP_TITLE:
    case GTK_FILE_CHOOSER_PROP_ACTION:
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
    case GTK_FILE_CHOOSER_PROP_FILTER:
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
      g_object_get_property (G_OBJECT (priv->dialog), pspec->name, value);
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
  GtkFileChooserButtonPrivate *priv = button->priv;

  if (priv->old_path)
    gtk_file_path_free (priv->old_path);

  if (G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->finalize != NULL)
    (*G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->finalize) (object);
}

/* ********************* *
 *  GtkObject Functions  *
 * ********************* */

static void
gtk_file_chooser_button_destroy (GtkObject *object)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (object);
  GtkFileChooserButtonPrivate *priv = button->priv;
  GtkTreeIter iter;
  GSList *l;

  if (priv->dialog != NULL)
    {
      gtk_widget_destroy (priv->dialog);
      priv->dialog = NULL;
    }

  if (priv->model && gtk_tree_model_get_iter_first (priv->model, &iter)) do
    {
      model_free_row_data (button, &iter);
    }
  while (gtk_tree_model_iter_next (priv->model, &iter));

  if (priv->dnd_select_folder_handle)
    {
      gtk_file_system_cancel_operation (priv->dnd_select_folder_handle);
      priv->dnd_select_folder_handle = NULL;
    }

  if (priv->update_button_handle)
    {
      gtk_file_system_cancel_operation (priv->update_button_handle);
      priv->update_button_handle = NULL;
    }

  if (priv->change_icon_theme_handles)
    {
      for (l = priv->change_icon_theme_handles; l; l = l->next)
        {
          GtkFileSystemHandle *handle = GTK_FILE_SYSTEM_HANDLE (l->data);
          gtk_file_system_cancel_operation (handle);
        }
      g_slist_free (priv->change_icon_theme_handles);
      priv->change_icon_theme_handles = NULL;
    }

  if (priv->model)
    {
      g_object_unref (priv->model);
      priv->model = NULL;
    }

  if (priv->filter_model)
    {
      g_object_unref (priv->filter_model);
      priv->filter_model = NULL;
    }

  if (priv->fs)
    {
      g_signal_handler_disconnect (priv->fs, priv->fs_volumes_changed_id);
      g_signal_handler_disconnect (priv->fs, priv->fs_bookmarks_changed_id);
      g_object_unref (priv->fs);
      priv->fs = NULL;
    }

  if (GTK_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->destroy != NULL)
    (*GTK_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->destroy) (object);
}


/* ********************* *
 *  GtkWidget Functions  *
 * ********************* */

struct DndSelectFolderData
{
  GtkFileChooserButton *button;
  GtkFileChooserAction action;
  GtkFilePath *path;
  gchar **uris;
  guint i;
  gboolean selected;
};

static void
dnd_select_folder_get_info_cb (GtkFileSystemHandle *handle,
			       const GtkFileInfo   *info,
			       const GError        *error,
			       gpointer             user_data)
{
  gboolean cancelled = handle->cancelled;
  struct DndSelectFolderData *data = user_data;

  if (handle != data->button->priv->dnd_select_folder_handle)
    {
      g_object_unref (data->button);
      gtk_file_path_free (data->path);
      g_strfreev (data->uris);
      g_free (data);

      g_object_unref (handle);
      return;
    }

  data->button->priv->dnd_select_folder_handle = NULL;

  if (!cancelled && !error && info != NULL)
    {
      data->selected = 
	(((data->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER &&
	   gtk_file_info_get_is_folder (info)) ||
	  (data->action == GTK_FILE_CHOOSER_ACTION_OPEN &&
	   !gtk_file_info_get_is_folder (info))) &&
	 _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (data->button->priv->dialog),
					data->path, NULL));
    }
  else
    data->selected = FALSE;

  if (data->selected || data->uris[++data->i] == NULL)
    {
      g_object_unref (data->button);
      gtk_file_path_free (data->path);
      g_strfreev (data->uris);
      g_free (data);

      g_object_unref (handle);
      return;
    }

  if (data->path)
    gtk_file_path_free (data->path);

  data->path = gtk_file_system_uri_to_path (handle->file_system,
					    data->uris[data->i]);

  data->button->priv->dnd_select_folder_handle =
    gtk_file_system_get_info (handle->file_system, data->path,
			      GTK_FILE_INFO_IS_FOLDER,
			      dnd_select_folder_get_info_cb, user_data);

  g_object_unref (handle);
}

static void
gtk_file_chooser_button_drag_data_received (GtkWidget	     *widget,
					    GdkDragContext   *context,
					    gint	      x,
					    gint	      y,
					    GtkSelectionData *data,
					    guint	      info,
					    guint	      drag_time)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);
  GtkFileChooserButtonPrivate *priv = button->priv;
  GtkFilePath *path;
  gchar *text;

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->drag_data_received != NULL)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->drag_data_received) (widget,
										    context,
										    x, y,
										    data, info,
										    drag_time);

  if (widget == NULL || context == NULL || data == NULL || data->length < 0)
    return;

  switch (info)
    {
    case TEXT_URI_LIST:
      {
	gchar **uris;
	struct DndSelectFolderData *info;

	uris = gtk_selection_data_get_uris (data);
	
	if (uris == NULL)
	  break;

	info = g_new0 (struct DndSelectFolderData, 1);
	info->button = g_object_ref (button);
	info->i = 0;
	info->uris = uris;
	info->selected = FALSE;
	g_object_get (priv->dialog, "action", &info->action, NULL);

	info->path = gtk_file_system_uri_to_path (priv->fs,
						  info->uris[info->i]);

	if (priv->dnd_select_folder_handle)
	  gtk_file_system_cancel_operation (priv->dnd_select_folder_handle);

	priv->dnd_select_folder_handle =
	  gtk_file_system_get_info (priv->fs, info->path,
				    GTK_FILE_INFO_IS_FOLDER,
				    dnd_select_folder_get_info_cb, info);
      }
      break;

    case TEXT_PLAIN:
      text = (char*) gtk_selection_data_get_text (data);
      path = gtk_file_path_new_steal (text);
      _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (priv->dialog), path,
				     NULL);
      gtk_file_path_free (path);
      break;

    default:
      break;
    }

  gtk_drag_finish (context, TRUE, FALSE, drag_time);
}

static void
gtk_file_chooser_button_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}

static void
gtk_file_chooser_button_hide_all (GtkWidget *widget)
{
  gtk_widget_hide (widget);
}

static void
gtk_file_chooser_button_show (GtkWidget *widget)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);
  GtkFileChooserButtonPrivate *priv = button->priv;

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show) (widget);

  if (priv->active)
    open_dialog (GTK_FILE_CHOOSER_BUTTON (widget));
}

static void
gtk_file_chooser_button_hide (GtkWidget *widget)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);
  GtkFileChooserButtonPrivate *priv = button->priv;

  gtk_widget_hide (priv->dialog);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->hide)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->hide) (widget);
}

static void
gtk_file_chooser_button_map (GtkWidget *widget)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);
  GtkFileChooserButtonPrivate *priv = button->priv;

  if (!priv->folder_has_been_set)
    {
      char *current_working_dir;

      current_working_dir = g_get_current_dir ();
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget), current_working_dir);
      g_free (current_working_dir);

      priv->folder_has_been_set = TRUE;
    }

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->map)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->map) (widget);
}

static gboolean
gtk_file_chooser_button_mnemonic_activate (GtkWidget *widget,
					   gboolean   group_cycling)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (widget);
  GtkFileChooserButtonPrivate *priv = button->priv;

  switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (priv->dialog)))
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      gtk_widget_grab_focus (priv->button);
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      return gtk_widget_mnemonic_activate (priv->combo_box, group_cycling);
      break;
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
change_icon_theme_get_info_cb (GtkFileSystemHandle *handle,
			       const GtkFileInfo   *info,
			       const GError        *error,
			       gpointer             user_data)
{
  gboolean cancelled = handle->cancelled;
  GdkPixbuf *pixbuf;
  struct ChangeIconThemeData *data = user_data;

  if (!g_slist_find (data->button->priv->change_icon_theme_handles, handle))
    goto out;

  data->button->priv->change_icon_theme_handles =
    g_slist_remove (data->button->priv->change_icon_theme_handles, handle);

  if (cancelled || error)
    goto out;

  pixbuf = gtk_file_info_render_icon (info, GTK_WIDGET (data->button),
				      data->button->priv->icon_size, NULL);

  if (pixbuf)
    {
      gint width = 0;
      GtkTreeIter iter;
      GtkTreePath *path;

      width = MAX (width, gdk_pixbuf_get_width (pixbuf));

      path = gtk_tree_row_reference_get_path (data->row_ref);
      if (path) 
        {
          gtk_tree_model_get_iter (data->button->priv->model, &iter, path);
          gtk_tree_path_free (path);

          gtk_list_store_set (GTK_LIST_STORE (data->button->priv->model), &iter,
	  		      ICON_COLUMN, pixbuf,
			      -1);

          g_object_set (data->button->priv->icon_cell,
		        "width", width,
		        NULL);
        }
      g_object_unref (pixbuf);
    }

out:
  g_object_unref (data->button);
  gtk_tree_row_reference_free (data->row_ref);
  g_free (data);

  g_object_unref (handle);
}

static void
change_icon_theme (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv = button->priv;
  GtkSettings *settings;
  GtkIconTheme *theme;
  GtkTreeIter iter;
  GSList *l;
  gint width = 0, height = 0;

  for (l = button->priv->change_icon_theme_handles; l; l = l->next)
    {
      GtkFileSystemHandle *handle = GTK_FILE_SYSTEM_HANDLE (l->data);
      gtk_file_system_cancel_operation (handle);
    }
  g_slist_free (button->priv->change_icon_theme_handles);
  button->priv->change_icon_theme_handles = NULL;

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (button)));

  if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
					 &width, &height))
    priv->icon_size = MAX (width, height);
  else
    priv->icon_size = FALLBACK_ICON_SIZE;

  update_label_and_image (button);

  gtk_tree_model_get_iter_first (priv->model, &iter);

  theme = get_icon_theme (GTK_WIDGET (button));

  do
    {
      GdkPixbuf *pixbuf;
      gchar type;
      gpointer data;

      type = ROW_TYPE_INVALID;
      gtk_tree_model_get (priv->model, &iter,
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
	      if (gtk_file_system_path_is_local (priv->fs, (GtkFilePath *)data))
		{
		  GtkTreePath *path;
		  GtkFileSystemHandle *handle;
		  struct ChangeIconThemeData *info;		  
		  
		  info = g_new0 (struct ChangeIconThemeData, 1);
		  info->button = g_object_ref (button);
		  path = gtk_tree_model_get_path (priv->model, &iter);
		  info->row_ref = gtk_tree_row_reference_new (priv->model, path);
		  gtk_tree_path_free (path);
		  
		  handle =
		    gtk_file_system_get_info (priv->fs, data, GTK_FILE_INFO_ICON,
					      change_icon_theme_get_info_cb,
					      info);
		  button->priv->change_icon_theme_handles =
		    g_slist_append (button->priv->change_icon_theme_handles, handle);
		  pixbuf = NULL;
		}
	      else
		/* Don't call get_info for remote paths to avoid latency and
		 * auth dialogs.
		 * If we switch to a better bookmarks file format (XBEL), we
		 * should use mime info to get a better icon.
		 */
		pixbuf = gtk_icon_theme_load_icon (theme, "gnome-fs-regular",
						   priv->icon_size, 0, NULL);
	    }
	  else
	    pixbuf = gtk_icon_theme_load_icon (theme, FALLBACK_ICON_NAME,
					       priv->icon_size, 0, NULL);
	  break;
	case ROW_TYPE_VOLUME:
	  if (data)
	    pixbuf = gtk_file_system_volume_render_icon (priv->fs, data,
							 GTK_WIDGET (button),
							 priv->icon_size,
							 NULL);
	  else
	    pixbuf = gtk_icon_theme_load_icon (theme, FALLBACK_ICON_NAME,
					       priv->icon_size, 0, NULL);
	  break;
	default:
	  continue;
	  break;
	}

      if (pixbuf)
	width = MAX (width, gdk_pixbuf_get_width (pixbuf));

      gtk_list_store_set (GTK_LIST_STORE (priv->model), &iter,
			  ICON_COLUMN, pixbuf,
			  -1);

      if (pixbuf)
	g_object_unref (pixbuf);
    }
  while (gtk_tree_model_iter_next (priv->model, &iter));

  g_object_set (button->priv->icon_cell,
		"width", width,
		NULL);
}

static void
gtk_file_chooser_button_style_set (GtkWidget *widget,
				   GtkStyle  *old_style)
{
  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->style_set)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->style_set) (widget,
									   old_style);

  if (gtk_widget_has_screen (widget))
    change_icon_theme (GTK_FILE_CHOOSER_BUTTON (widget));
}

static void
gtk_file_chooser_button_screen_changed (GtkWidget *widget,
					GdkScreen *old_screen)
{
  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->screen_changed)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->screen_changed) (widget,
										old_screen);

  change_icon_theme (GTK_FILE_CHOOSER_BUTTON (widget)); 
}


/* ******************* *
 *  Utility Functions  *
 * ******************* */

/* General */
static GtkIconTheme *
get_icon_theme (GtkWidget *widget)
{
  if (gtk_widget_has_screen (widget))
    return gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

  return gtk_icon_theme_get_default ();
}


struct SetDisplayNameData
{
  GtkFileChooserButton *button;
  char *label;
  GtkTreeRowReference *row_ref;
};

static void
set_info_get_info_cb (GtkFileSystemHandle *handle,
		      const GtkFileInfo   *info,
		      const GError        *error,
		      gpointer             callback_data)
{
  gboolean cancelled = handle->cancelled;
  GdkPixbuf *pixbuf;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkFileSystemHandle *model_handle;
  struct SetDisplayNameData *data = callback_data;

  if (!data->button->priv->model)
    /* button got destroyed */
    goto out;

  path = gtk_tree_row_reference_get_path (data->row_ref);
  if (!path)
    /* Handle doesn't exist anymore in the model */
    goto out;

  gtk_tree_model_get_iter (data->button->priv->model, &iter, path);
  gtk_tree_path_free (path);

  /* Validate the handle */
  gtk_tree_model_get (data->button->priv->model, &iter,
		      HANDLE_COLUMN, &model_handle,
		      -1);
  if (handle != model_handle)
    goto out;

  gtk_list_store_set (GTK_LIST_STORE (data->button->priv->model), &iter,
		      HANDLE_COLUMN, NULL,
		      -1);

  if (cancelled || error)
    /* There was an error, leave the fallback name in there */
    goto out;

  pixbuf = gtk_file_info_render_icon (info, GTK_WIDGET (data->button),
				      data->button->priv->icon_size, NULL);

  if (!data->label)
    data->label = g_strdup (gtk_file_info_get_display_name (info));

  gtk_list_store_set (GTK_LIST_STORE (data->button->priv->model), &iter,
		      ICON_COLUMN, pixbuf,
		      DISPLAY_NAME_COLUMN, data->label,
		      IS_FOLDER_COLUMN, gtk_file_info_get_is_folder (info),
		      -1);

  if (pixbuf)
    g_object_unref (pixbuf);

out:
  g_object_unref (data->button);
  g_free (data->label);
  gtk_tree_row_reference_free (data->row_ref);
  g_free (data);

  g_object_unref (handle);
}

static void
set_info_for_path_at_iter (GtkFileChooserButton *button,
			   const GtkFilePath    *path,
			   GtkTreeIter          *iter)
{
  struct SetDisplayNameData *data;
  GtkTreePath *tree_path;
  GtkFileSystemHandle *handle;

  data = g_new0 (struct SetDisplayNameData, 1);
  data->button = g_object_ref (button);
  data->label = gtk_file_system_get_bookmark_label (button->priv->fs, path);

  tree_path = gtk_tree_model_get_path (button->priv->model, iter);
  data->row_ref = gtk_tree_row_reference_new (button->priv->model, tree_path);
  gtk_tree_path_free (tree_path);

  handle = gtk_file_system_get_info (button->priv->fs, path,
				     GTK_FILE_INFO_DISPLAY_NAME | GTK_FILE_INFO_IS_FOLDER | GTK_FILE_INFO_ICON,
				     set_info_get_info_cb, data);

  gtk_list_store_set (GTK_LIST_STORE (button->priv->model), iter,
		      HANDLE_COLUMN, handle,
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

  retval += button->priv->n_special;

  if (row_type == ROW_TYPE_VOLUME)
    return retval;

  retval += button->priv->n_volumes;

  if (row_type == ROW_TYPE_SHORTCUT)
    return retval;

  retval += button->priv->n_shortcuts;

  if (row_type == ROW_TYPE_BOOKMARK_SEPARATOR)
    return retval;

  retval += button->priv->has_bookmark_separator;

  if (row_type == ROW_TYPE_BOOKMARK)
    return retval;

  retval += button->priv->n_bookmarks;

  if (row_type == ROW_TYPE_CURRENT_FOLDER_SEPARATOR)
    return retval;

  retval += button->priv->has_current_folder_separator;

  if (row_type == ROW_TYPE_CURRENT_FOLDER)
    return retval;

  retval += button->priv->has_current_folder;

  if (row_type == ROW_TYPE_OTHER_SEPARATOR)
    return retval;

  retval += button->priv->has_other_separator;

  if (row_type == ROW_TYPE_OTHER)
    return retval;

  g_assert_not_reached ();
  return -1;
}

static void
model_free_row_data (GtkFileChooserButton *button,
		     GtkTreeIter          *iter)
{
  gchar type;
  gpointer data;
  GtkFileSystemHandle *handle;

  gtk_tree_model_get (button->priv->model, iter,
		      TYPE_COLUMN, &type,
		      DATA_COLUMN, &data,
		      HANDLE_COLUMN, &handle,
		      -1);

  if (handle)
    gtk_file_system_cancel_operation (handle);

  switch (type)
    {
    case ROW_TYPE_SPECIAL:
    case ROW_TYPE_SHORTCUT:
    case ROW_TYPE_BOOKMARK:
    case ROW_TYPE_CURRENT_FOLDER:
      gtk_file_path_free (data);
      break;
    case ROW_TYPE_VOLUME:
      gtk_file_system_volume_free (button->priv->fs, data);
      break;
    default:
      break;
    }
}

static void
model_add_special_get_info_cb (GtkFileSystemHandle *handle,
			       const GtkFileInfo   *info,
			       const GError        *error,
			       gpointer             user_data)
{
  gboolean cancelled = handle->cancelled;
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkPixbuf *pixbuf;
  GtkFileSystemHandle *model_handle;
  struct ChangeIconThemeData *data = user_data;

  if (!data->button->priv->model)
    /* button got destroyed */
    goto out;

  path = gtk_tree_row_reference_get_path (data->row_ref);
  if (!path)
    /* Handle doesn't exist anymore in the model */
    goto out;

  gtk_tree_model_get_iter (data->button->priv->model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (data->button->priv->model, &iter,
		      HANDLE_COLUMN, &model_handle,
		      -1);
  if (handle != model_handle)
    goto out;

  gtk_list_store_set (GTK_LIST_STORE (data->button->priv->model), &iter,
		      HANDLE_COLUMN, NULL,
		      -1);

  if (cancelled || error)
    goto out;

  pixbuf = gtk_file_info_render_icon (info, GTK_WIDGET (data->button),
				      data->button->priv->icon_size, NULL);

  if (pixbuf)
    {
      gtk_list_store_set (GTK_LIST_STORE (data->button->priv->model), &iter,
			  ICON_COLUMN, pixbuf,
			  -1);
      g_object_unref (pixbuf);
    }

  gtk_list_store_set (GTK_LIST_STORE (data->button->priv->model), &iter,
		      DISPLAY_NAME_COLUMN, gtk_file_info_get_display_name (info),
		      -1);

out:
  g_object_unref (data->button);
  gtk_tree_row_reference_free (data->row_ref);
  g_free (data);

  g_object_unref (handle);
}

static inline void
model_add_special (GtkFileChooserButton *button)
{
  const gchar *homedir;
  gchar *desktopdir = NULL;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkFilePath *path;
  gint pos;

  store = GTK_LIST_STORE (button->priv->model);
  pos = model_get_type_position (button, ROW_TYPE_SPECIAL);

  homedir = g_get_home_dir ();

  if (homedir)
    {
      GtkTreePath *tree_path;
      GtkFileSystemHandle *handle;
      struct ChangeIconThemeData *info;

      path = gtk_file_system_filename_to_path (button->priv->fs, homedir);
      gtk_list_store_insert (store, &iter, pos);
      pos++;

      info = g_new0 (struct ChangeIconThemeData, 1);
      info->button = g_object_ref (button);
      tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
      info->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store),
						  tree_path);
      gtk_tree_path_free (tree_path);

      handle = gtk_file_system_get_info (button->priv->fs, path,
					 GTK_FILE_INFO_DISPLAY_NAME | GTK_FILE_INFO_ICON,
					 model_add_special_get_info_cb, info);

      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, NULL,
			  TYPE_COLUMN, ROW_TYPE_SPECIAL,
			  DATA_COLUMN, path,
			  IS_FOLDER_COLUMN, TRUE,
			  HANDLE_COLUMN, handle,
			  -1);

      button->priv->n_special++;

#ifndef G_OS_WIN32
      desktopdir = g_build_filename (homedir, DESKTOP_DISPLAY_NAME, NULL);
#endif
    }

#ifdef G_OS_WIN32
  desktopdir = _gtk_file_system_win32_get_desktop ();
#endif

  if (desktopdir)
    {
      GtkTreePath *tree_path;
      GtkFileSystemHandle *handle;
      struct ChangeIconThemeData *info;

      path = gtk_file_system_filename_to_path (button->priv->fs, desktopdir);
      g_free (desktopdir);
      gtk_list_store_insert (store, &iter, pos);
      pos++;

      info = g_new0 (struct ChangeIconThemeData, 1);
      info->button = g_object_ref (button);
      tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
      info->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store),
						  tree_path);
      gtk_tree_path_free (tree_path);

      handle = gtk_file_system_get_info (button->priv->fs, path,
					 GTK_FILE_INFO_DISPLAY_NAME | GTK_FILE_INFO_ICON,
					 model_add_special_get_info_cb, info);

      gtk_list_store_set (store, &iter,
			  TYPE_COLUMN, ROW_TYPE_SPECIAL,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, _(DESKTOP_DISPLAY_NAME),
			  DATA_COLUMN, path,
			  IS_FOLDER_COLUMN, TRUE,
			  HANDLE_COLUMN, handle,
			  -1);

      button->priv->n_special++;
    }
}

static void
model_add_volumes (GtkFileChooserButton *button,
		   GSList               *volumes)
{
  GtkListStore *store;
  gint pos;
  gboolean local_only;
  GtkFileSystem *file_system;
  GSList *l;
  
  if (!volumes)
    return;

  store = GTK_LIST_STORE (button->priv->model);
  pos = model_get_type_position (button, ROW_TYPE_VOLUME);
  local_only = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (button->priv->dialog));
  file_system = button->priv->fs;

  for (l = volumes; l; l = l->next)
    {
      GtkFileSystemVolume *volume;
      GtkTreeIter iter;
      GdkPixbuf *pixbuf;
      gchar *display_name;

      volume = l->data;

      if (local_only)
	{
	  if (gtk_file_system_volume_get_is_mounted (file_system, volume))
	    {
	      GtkFilePath *base_path;

	      base_path = gtk_file_system_volume_get_base_path (file_system, volume);
	      if (base_path != NULL)
		{
		  gboolean is_local = gtk_file_system_path_is_local (file_system, base_path);
		  gtk_file_path_free (base_path);

		  if (!is_local)
		    {
		      gtk_file_system_volume_free (file_system, volume);
		      continue;
		    }
		}
	    }
	}

      pixbuf = gtk_file_system_volume_render_icon (file_system,
						   volume,
						   GTK_WIDGET (button),
						   button->priv->icon_size,
						   NULL);
      display_name = gtk_file_system_volume_get_display_name (file_system, volume);

      gtk_list_store_insert (store, &iter, pos);
      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, pixbuf,
			  DISPLAY_NAME_COLUMN, display_name,
			  TYPE_COLUMN, ROW_TYPE_VOLUME,
			  DATA_COLUMN, volume,
			  IS_FOLDER_COLUMN, TRUE,
			  -1);

      if (pixbuf)
	g_object_unref (pixbuf);
      g_free (display_name);

      button->priv->n_volumes++;
      pos++;
    }
}

extern gchar * _gtk_file_chooser_label_for_uri (const gchar *uri);

static void
model_add_bookmarks (GtkFileChooserButton *button,
		     GSList               *bookmarks)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint pos;
  gboolean local_only;
  GSList *l;

  if (!bookmarks)
    return;

  store = GTK_LIST_STORE (button->priv->model);
  pos = model_get_type_position (button, ROW_TYPE_BOOKMARK);
  local_only = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (button->priv->dialog));

  for (l = bookmarks; l; l = l->next)
    {
      GtkFilePath *path;

      path = l->data;

      if (gtk_file_system_path_is_local (button->priv->fs, path))
	{
	  gtk_list_store_insert (store, &iter, pos);
	  gtk_list_store_set (store, &iter,
			      ICON_COLUMN, NULL,
			      DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
			      TYPE_COLUMN, ROW_TYPE_BOOKMARK,
			      DATA_COLUMN, gtk_file_path_copy (path),
			      IS_FOLDER_COLUMN, FALSE,
			      -1);
	  set_info_for_path_at_iter (button, path, &iter);
	}
      else
	{
	  gchar *label;
	  GtkIconTheme *icon_theme;
	  GdkPixbuf *pixbuf;

	  if (local_only)
	    continue;

	  /* Don't call get_info for remote paths to avoid latency and
	   * auth dialogs.
	   * If we switch to a better bookmarks file format (XBEL), we
	   * should use mime info to get a better icon.
	   */
	  label = gtk_file_system_get_bookmark_label (button->priv->fs, path);
	  if (!label)
	    {
	      gchar *uri;

	      uri = gtk_file_system_path_to_uri (button->priv->fs, path);
	      label = _gtk_file_chooser_label_for_uri (uri);
	      g_free (uri);
	    }

	  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (button)));
	  pixbuf = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-directory", 
					     button->priv->icon_size, 0, NULL);

	  gtk_list_store_insert (store, &iter, pos);
	  gtk_list_store_set (store, &iter,
			      ICON_COLUMN, pixbuf,
			      DISPLAY_NAME_COLUMN, label,
			      TYPE_COLUMN, ROW_TYPE_BOOKMARK,
			      DATA_COLUMN, gtk_file_path_copy (path),
			      IS_FOLDER_COLUMN, TRUE,
			      -1);

	  g_free (label);
	  g_object_unref (pixbuf);
	}

      button->priv->n_bookmarks++;
      pos++;
    }

  if (button->priv->n_bookmarks > 0 && 
      !button->priv->has_bookmark_separator)
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
      button->priv->has_bookmark_separator = TRUE;
    }
}

static void
model_update_current_folder (GtkFileChooserButton *button,
			     const GtkFilePath    *path)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint pos;

  if (!path) 
    return;

  store = GTK_LIST_STORE (button->priv->model);

  if (!button->priv->has_current_folder_separator)
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
      button->priv->has_current_folder_separator = TRUE;
    }

  pos = model_get_type_position (button, ROW_TYPE_CURRENT_FOLDER);
  if (!button->priv->has_current_folder)
    {
      gtk_list_store_insert (store, &iter, pos);
      button->priv->has_current_folder = TRUE;
    }
  else
    {
      gtk_tree_model_iter_nth_child (button->priv->model, &iter, NULL, pos);
      model_free_row_data (button, &iter);
    }

  if (gtk_file_system_path_is_local (button->priv->fs, path))
    {
      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, NULL,
			  DISPLAY_NAME_COLUMN, _(FALLBACK_DISPLAY_NAME),
			  TYPE_COLUMN, ROW_TYPE_CURRENT_FOLDER,
			  DATA_COLUMN, gtk_file_path_copy (path),
			  IS_FOLDER_COLUMN, FALSE,
			  -1);
      set_info_for_path_at_iter (button, path, &iter);
    }
  else
    {
      gchar *label;
      GtkIconTheme *icon_theme;
      GdkPixbuf *pixbuf;

      /* Don't call get_info for remote paths to avoid latency and
       * auth dialogs.
       * If we switch to a better bookmarks file format (XBEL), we
       * should use mime info to get a better icon.
       */
      label = gtk_file_system_get_bookmark_label (button->priv->fs, path);
      if (!label)
	{
	  gchar *uri;
	  
	  uri = gtk_file_system_path_to_uri (button->priv->fs, path);
	  label = _gtk_file_chooser_label_for_uri (uri);
	  g_free (uri);
	}
      
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (button)));
      pixbuf = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-directory", 
					 button->priv->icon_size, 0, NULL);
      
      gtk_list_store_set (store, &iter,
			  ICON_COLUMN, pixbuf,
			  DISPLAY_NAME_COLUMN, label,
			  TYPE_COLUMN, ROW_TYPE_CURRENT_FOLDER,
			  DATA_COLUMN, gtk_file_path_copy (path),
			  IS_FOLDER_COLUMN, TRUE,
			  -1);
      
      g_free (label);
      g_object_unref (pixbuf);
    }
}

static inline void
model_add_other (GtkFileChooserButton *button)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint pos;
  
  store = GTK_LIST_STORE (button->priv->model);
  pos = model_get_type_position (button, ROW_TYPE_OTHER_SEPARATOR);

  gtk_list_store_insert (store, &iter, pos);
  gtk_list_store_set (store, &iter,
		      ICON_COLUMN, NULL,
		      DISPLAY_NAME_COLUMN, NULL,
		      TYPE_COLUMN, ROW_TYPE_OTHER_SEPARATOR,
		      DATA_COLUMN, NULL,
		      IS_FOLDER_COLUMN, FALSE,
		      -1);
  button->priv->has_other_separator = TRUE;
  pos++;

  gtk_list_store_insert (store, &iter, pos);
  gtk_list_store_set (store, &iter,
		      ICON_COLUMN, NULL,
		      DISPLAY_NAME_COLUMN, _("Other..."),
		      TYPE_COLUMN, ROW_TYPE_OTHER,
		      DATA_COLUMN, NULL,
		      IS_FOLDER_COLUMN, FALSE,
		      -1);
}

static void
model_remove_rows (GtkFileChooserButton *button,
		   gint                  pos,
		   gint                  n_rows)
{
  GtkListStore *store;

  if (!n_rows)
    return;

  store = GTK_LIST_STORE (button->priv->model);

  do
    {
      GtkTreeIter iter;

      if (!gtk_tree_model_iter_nth_child (button->priv->model, &iter, NULL, pos))
	g_assert_not_reached ();

      model_free_row_data (button, &iter);
      gtk_list_store_remove (store, &iter);
      n_rows--;
    }
  while (n_rows);
}

/* Filter Model */
static inline gboolean
test_if_path_is_visible (GtkFileSystem     *fs,
			 const GtkFilePath *path,
			 gboolean           local_only,
			 gboolean           is_folder)
{
  if (!path)
    return FALSE;

  if (local_only && !gtk_file_system_path_is_local (fs, path))
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
  GtkFileChooserButtonPrivate *priv = button->priv;
  gchar type;
  gpointer data;
  gboolean local_only, retval, is_folder;

  type = ROW_TYPE_INVALID;
  data = NULL;
  local_only = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (priv->dialog));

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
      retval = test_if_path_is_visible (priv->fs, data, local_only, is_folder);
      break;
    case ROW_TYPE_VOLUME:
      {
	retval = TRUE;
	if (local_only)
	  {
	    if (gtk_file_system_volume_get_is_mounted (priv->fs, data))
	      {
		GtkFilePath *base_path;
		
		base_path = gtk_file_system_volume_get_base_path (priv->fs, data);
		if (base_path)
		  {
		    gboolean is_local = gtk_file_system_path_is_local (priv->fs, base_path);
		    
		    gtk_file_path_free (base_path);

		    if (!is_local)
		      retval = FALSE;
		  }
		else
		  retval = FALSE;
	      }
	  }
      }
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
update_combo_box (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv = button->priv;
  GSList *paths;
  GtkTreeIter iter;
  gboolean row_found;

  gtk_tree_model_get_iter_first (priv->filter_model, &iter);

  paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (priv->dialog));

  row_found = FALSE;

  do
    {
      gchar type;
      gpointer data;

      type = ROW_TYPE_INVALID;
      data = NULL;

      gtk_tree_model_get (priv->filter_model, &iter,
			  TYPE_COLUMN, &type,
			  DATA_COLUMN, &data,
			  -1);
    
      switch (type)
	{
	case ROW_TYPE_SPECIAL:
	case ROW_TYPE_SHORTCUT:
	case ROW_TYPE_BOOKMARK:
	case ROW_TYPE_CURRENT_FOLDER:
	  row_found = (paths &&
		       paths->data &&
		       gtk_file_path_compare (data, paths->data) == 0);
	  break;
	case ROW_TYPE_VOLUME:
	  {
	    GtkFilePath *base_path;

	    base_path = gtk_file_system_volume_get_base_path (priv->fs, data);
            if (base_path)
              {
	        row_found = (paths &&
		  	     paths->data &&
			     gtk_file_path_compare (base_path, paths->data) == 0);
	        gtk_file_path_free (base_path);
              }
	  }
	  break;
	default:
	  row_found = FALSE;
	  break;
	}

      if (row_found)
	{
	  g_signal_handler_block (priv->combo_box, priv->combo_box_changed_id);
	  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->combo_box),
					 &iter);
	  g_signal_handler_unblock (priv->combo_box,
				    priv->combo_box_changed_id);
	}
    }
  while (!row_found && gtk_tree_model_iter_next (priv->filter_model, &iter));

  /* If it hasn't been found already, update & select the current-folder row. */
  if (!row_found && paths && paths->data)
    {
      GtkTreeIter filter_iter;
      gint pos;
    
      model_update_current_folder (button, paths->data);
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));

      pos = model_get_type_position (button, ROW_TYPE_CURRENT_FOLDER);
      gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, pos);

      gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
							&filter_iter, &iter);

      g_signal_handler_block (priv->combo_box, priv->combo_box_changed_id);
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->combo_box), &filter_iter);
      g_signal_handler_unblock (priv->combo_box, priv->combo_box_changed_id);
    }

  gtk_file_paths_free (paths);
}

/* Button */
static void
update_label_get_info_cb (GtkFileSystemHandle *handle,
			  const GtkFileInfo   *info,
			  const GError        *error,
			  gpointer             data)
{
  gboolean cancelled = handle->cancelled;
  GdkPixbuf *pixbuf;
  GtkFileChooserButton *button = data;
  GtkFileChooserButtonPrivate *priv = button->priv;

  if (handle != priv->update_button_handle)
    goto out;

  priv->update_button_handle = NULL;

  if (cancelled || error)
    goto out;

  gtk_label_set_text (GTK_LABEL (priv->label), gtk_file_info_get_display_name (info));

  pixbuf = gtk_file_info_render_icon (info, GTK_WIDGET (priv->image),
				      priv->icon_size, NULL);
  if (!pixbuf)
    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (priv->image)),
				       FALLBACK_ICON_NAME,
				       priv->icon_size, 0, NULL);

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);

out:
  g_object_unref (button);
  g_object_unref (handle);
}

static void
update_label_and_image (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv = button->priv;
  GdkPixbuf *pixbuf;
  gchar *label_text;
  GSList *paths;

  paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (priv->dialog));
  label_text = NULL;
  pixbuf = NULL;

  if (paths && paths->data)
    {
      GtkFilePath *path;
      GtkFileSystemVolume *volume = NULL;
    
      path = paths->data;

      volume = gtk_file_system_get_volume_for_path (priv->fs, path);
      if (volume)
	{
	  GtkFilePath *base_path;

	  base_path = gtk_file_system_volume_get_base_path (priv->fs, volume);
	  if (base_path && gtk_file_path_compare (base_path, path) == 0)
	    {
	      label_text = gtk_file_system_volume_get_display_name (priv->fs,
								    volume);
	      pixbuf = gtk_file_system_volume_render_icon (priv->fs, volume,
							   GTK_WIDGET (button),
							   priv->icon_size,
							   NULL);
	    }

	  if (base_path)
	    gtk_file_path_free (base_path);

	  gtk_file_system_volume_free (priv->fs, volume);

	  if (label_text)
	    goto out;
	}

      if (priv->update_button_handle)
	{
	  gtk_file_system_cancel_operation (priv->update_button_handle);
	  priv->update_button_handle = NULL;
	}
	  
      if (gtk_file_system_path_is_local (priv->fs, path))
	{
	  priv->update_button_handle =
	    gtk_file_system_get_info (priv->fs, path,
				      GTK_FILE_INFO_DISPLAY_NAME | GTK_FILE_INFO_ICON,
				      update_label_get_info_cb,
				      g_object_ref (button));
	}
      else
	{
	  GdkPixbuf *pixbuf;

	  label_text = gtk_file_system_get_bookmark_label (button->priv->fs, path);
	  
	  pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (priv->image)), 
					     "gnome-fs-regular",
					     priv->icon_size, 0, NULL);
	  
	  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), pixbuf);

	  if (pixbuf)
	    g_object_unref (pixbuf);
	}
    }
out:
  gtk_file_paths_free (paths);

  if (label_text)
    {
      gtk_label_set_text (GTK_LABEL (priv->label), label_text);
      g_free (label_text);
    }
  else
    gtk_label_set_text (GTK_LABEL (priv->label), _(FALLBACK_DISPLAY_NAME));
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
  GtkFileChooserButtonPrivate *priv = button->priv;
  GSList *volumes;

  model_remove_rows (user_data,
		     model_get_type_position (user_data, ROW_TYPE_VOLUME),
		     priv->n_volumes);

  priv->n_volumes = 0;

  volumes = gtk_file_system_list_volumes (fs);
  model_add_volumes (user_data, volumes);
  g_slist_free (volumes);

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));

  update_label_and_image (user_data);
  update_combo_box (user_data);
}

static void
fs_bookmarks_changed_cb (GtkFileSystem *fs,
			 gpointer       user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  GtkFileChooserButtonPrivate *priv = button->priv;
  GSList *bookmarks;

  bookmarks = gtk_file_system_list_bookmarks (fs);
  model_remove_rows (user_data,
		     model_get_type_position (user_data,
					      ROW_TYPE_BOOKMARK_SEPARATOR),
		     (priv->n_bookmarks + priv->has_bookmark_separator));
  priv->has_bookmark_separator = FALSE;
  priv->n_bookmarks = 0;
  model_add_bookmarks (user_data, bookmarks);
  gtk_file_paths_free (bookmarks);

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));

  update_label_and_image (user_data);
  update_combo_box (user_data);
}

/* Dialog */
static void
open_dialog (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv = button->priv;

  /* Setup the dialog parent to be chooser button's toplevel, and be modal
     as needed. */
  if (!GTK_WIDGET_VISIBLE (priv->dialog))
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));

      if (GTK_WIDGET_TOPLEVEL (toplevel) && GTK_IS_WINDOW (toplevel))
        {
          if (GTK_WINDOW (toplevel) != gtk_window_get_transient_for (GTK_WINDOW (priv->dialog)))
 	    gtk_window_set_transient_for (GTK_WINDOW (priv->dialog),
					  GTK_WINDOW (toplevel));
	      
	  gtk_window_set_modal (GTK_WINDOW (priv->dialog),
				gtk_window_get_modal (GTK_WINDOW (toplevel)));
	}
    }

  if (!priv->active)
    {
      GSList *paths;

      g_signal_handler_block (priv->dialog,
			      priv->dialog_folder_changed_id);
      g_signal_handler_block (priv->dialog,
			      priv->dialog_file_activated_id);
      g_signal_handler_block (priv->dialog,
			      priv->dialog_selection_changed_id);
      paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (priv->dialog));
      if (paths)
	{
	  if (paths->data)
	    priv->old_path = gtk_file_path_copy (paths->data);

	  gtk_file_paths_free (paths);
	}

      priv->active = TRUE;
    }

  gtk_widget_set_sensitive (priv->combo_box, FALSE);
  gtk_window_present (GTK_WINDOW (priv->dialog));
}

/* Combo Box */
static void
combo_box_changed_cb (GtkComboBox *combo_box,
		      gpointer     user_data)
{
  GtkTreeIter iter;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
      GtkFileChooserButtonPrivate *priv = button->priv;
      gchar type;
      gpointer data;

      type = ROW_TYPE_INVALID;
      data = NULL;

      gtk_tree_model_get (priv->filter_model, &iter,
			  TYPE_COLUMN, &type,
			  DATA_COLUMN, &data,
			  -1);

      switch (type)
	{
	case ROW_TYPE_SPECIAL:
	case ROW_TYPE_SHORTCUT:
	case ROW_TYPE_BOOKMARK:
	case ROW_TYPE_CURRENT_FOLDER:
	  gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
	  if (data)
	    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						       data, NULL);
	  break;
	case ROW_TYPE_VOLUME:
	  {
	    GtkFilePath *base_path;

	    gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
	    base_path = gtk_file_system_volume_get_base_path (priv->fs, data);
	    if (base_path)
	      {
		_gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
							   base_path, NULL);
		gtk_file_path_free (base_path);
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
dialog_current_folder_changed_cb (GtkFileChooser *dialog,
				  gpointer        user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  GtkFileChooserButtonPrivate *priv = button->priv;

  priv->folder_has_been_set = TRUE;

  g_signal_emit_by_name (button, "current-folder-changed");
}

static void
dialog_file_activated_cb (GtkFileChooser *dialog,
			  gpointer        user_data)
{
  g_signal_emit_by_name (user_data, "file-activated");
}

static void
dialog_selection_changed_cb (GtkFileChooser *dialog,
			     gpointer        user_data)
{
  update_label_and_image (user_data);
  update_combo_box (user_data);
  g_signal_emit_by_name (user_data, "selection-changed");
}

static void
dialog_update_preview_cb (GtkFileChooser *dialog,
			  gpointer        user_data)
{
  g_signal_emit_by_name (user_data, "update-preview");
}

static void
dialog_notify_cb (GObject    *dialog,
		  GParamSpec *pspec,
		  gpointer    user_data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (dialog)),
				 GTK_TYPE_FILE_CHOOSER);
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify (user_data, pspec->name);

  if (g_ascii_strcasecmp (pspec->name, "local-only") == 0)
    {
      GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
      GtkFileChooserButtonPrivate *priv = button->priv;

      if (priv->has_current_folder)
	{
	  GtkTreeIter iter;
	  gint pos;
	  gpointer data;

	  pos = model_get_type_position (user_data,
					 ROW_TYPE_CURRENT_FOLDER);
	  gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, pos);

	  data = NULL;
	  gtk_tree_model_get (priv->model, &iter, DATA_COLUMN, &data, -1);

	  /* If the path isn't local but we're in local-only mode now, remove
	   * the custom-folder row */
	  if (data &&
	      (!gtk_file_system_path_is_local (priv->fs, data) &&
	       gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (priv->dialog))))
	    {
	      pos--;
	      model_remove_rows (user_data, pos, 2);
	    }
	}

      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter_model));
      update_combo_box (user_data);
    }
}

static gboolean
dialog_delete_event_cb (GtkWidget *dialog,
			GdkEvent  *event,
		        gpointer   user_data)
{
  g_signal_emit_by_name (dialog, "response", GTK_RESPONSE_DELETE_EVENT);

  return TRUE;
}

static void
dialog_response_cb (GtkDialog *dialog,
		    gint       response,
		    gpointer   user_data)
{
  GtkFileChooserButton *button = GTK_FILE_CHOOSER_BUTTON (user_data);
  GtkFileChooserButtonPrivate *priv = button->priv;

  if (response == GTK_RESPONSE_ACCEPT)
    {
      g_signal_emit_by_name (user_data, "current-folder-changed");
      g_signal_emit_by_name (user_data, "selection-changed");
    }
  else if (priv->old_path)
    {
      switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog)))
	{
	case GTK_FILE_CHOOSER_ACTION_OPEN:
	  _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (dialog), priv->old_path,
					 NULL);
	  break;
	case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (dialog),
						     priv->old_path, NULL);
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
    }
  else
    gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (dialog));

  if (priv->old_path)
    {
      gtk_file_path_free (priv->old_path);
      priv->old_path = NULL;
    }

  update_label_and_image (user_data);
  update_combo_box (user_data);
  
  if (priv->active)
    {
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_folder_changed_id);
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_file_activated_id);
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_selection_changed_id);
      priv->active = FALSE;
    }

  gtk_widget_set_sensitive (priv->combo_box, TRUE);
  gtk_widget_hide (priv->dialog);
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
 * 
 * Since: 2.6
 **/
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
 * gtk_file_chooser_button_new_with_backend:
 * @title: the title of the browse dialog.
 * @action: the open mode for the widget.
 * @backend: the name of the #GtkFileSystem backend to use.
 * 
 * Creates a new file-selecting button widget using @backend.
 * 
 * Returns: a new button widget.
 * 
 * Since: 2.6
 **/
GtkWidget *
gtk_file_chooser_button_new_with_backend (const gchar          *title,
					  GtkFileChooserAction  action,
					  const gchar          *backend)
{
  g_return_val_if_fail (action == GTK_FILE_CHOOSER_ACTION_OPEN ||
			action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL);

  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON,
		       "action", action,
		       "title", (title ? title : _(DEFAULT_TITLE)),
		       "file-system-backend", backend,
		       NULL);
}

/**
 * gtk_file_chooser_button_new_with_dialog:
 * @dialog: the widget to use as dialog
 * 
 * Creates a #GtkFileChooserButton widget which uses @dialog as it's
 * file-picking window. Note that @dialog must be a #GtkDialog (or
 * subclass) which implements the #GtkFileChooser interface and must 
 * not have %GTK_DIALOG_DESTROY_WITH_PARENT set.
 * 
 * Returns: a new button widget.
 * 
 * Since: 2.6
 **/
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
 * 
 * Since: 2.6
 **/
void
gtk_file_chooser_button_set_title (GtkFileChooserButton *button,
				   const gchar          *title)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  gtk_window_set_title (GTK_WINDOW (button->priv->dialog), title);
  g_object_notify (G_OBJECT (button), "title");
}

/**
 * gtk_file_chooser_button_get_title:
 * @button: the button widget to examine.
 * 
 * Retrieves the title of the browse dialog used by @button. The returned value
 * should not be modified or freed.
 * 
 * Returns: a pointer to the browse dialog's title.
 * 
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_file_chooser_button_get_title (GtkFileChooserButton *button)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), NULL);

  return gtk_window_get_title (GTK_WINDOW (button->priv->dialog));
}

/**
 * gtk_file_chooser_button_get_width_chars:
 * @button: the button widget to examine.
 * 
 * Retrieves the width in characters of the @button widget's entry and/or label.
 * 
 * Returns: an integer width (in characters) that the button will use to size itself.
 * 
 * Since: 2.6
 **/
gint
gtk_file_chooser_button_get_width_chars (GtkFileChooserButton *button)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), -1);

  return gtk_label_get_width_chars (GTK_LABEL (button->priv->label));
}

/**
 * gtk_file_chooser_button_set_width_chars:
 * @button: the button widget to examine.
 * @n_chars: the new width, in characters.
 * 
 * Sets the width (in characters) that @button will use to @n_chars.
 * 
 * Since: 2.6
 **/
void
gtk_file_chooser_button_set_width_chars (GtkFileChooserButton *button,
					 gint                  n_chars)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  gtk_label_set_width_chars (GTK_LABEL (button->priv->label), n_chars);
  g_object_notify (G_OBJECT (button), "width-chars");
}

/**
 * gtk_file_chooser_button_set_focus_on_click:
 * @button: a #GtkFileChooserButton
 * @focus_on_click: whether the button grabs focus when clicked with the mouse
 * 
 * Sets whether the button will grab focus when it is clicked with the mouse.
 * Making mouse clicks not grab focus is useful in places like toolbars where
 * you don't want the keyboard focus removed from the main area of the
 * application.
 *
 * Since: 2.10
 **/
void
gtk_file_chooser_button_set_focus_on_click (GtkFileChooserButton *button,
					    gboolean              focus_on_click)
{
  GtkFileChooserButtonPrivate *priv;

  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  priv = button->priv;

  focus_on_click = focus_on_click != FALSE;

  if (priv->focus_on_click != focus_on_click)
    {
      priv->focus_on_click = focus_on_click;
      gtk_button_set_focus_on_click (GTK_BUTTON (priv->button), focus_on_click);
      gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (priv->combo_box), focus_on_click);
      
      g_object_notify (G_OBJECT (button), "focus-on-click");
    }
}

/**
 * gtk_file_chooser_button_get_focus_on_click:
 * @button: a #GtkFileChooserButton
 * 
 * Returns whether the button grabs focus when it is clicked with the mouse.
 * See gtk_file_chooser_button_set_focus_on_click().
 *
 * Return value: %TRUE if the button grabs focus when it is clicked with
 *               the mouse.
 *
 * Since: 2.10
 **/
gboolean
gtk_file_chooser_button_get_focus_on_click (GtkFileChooserButton *button)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), FALSE);
  
  return button->priv->focus_on_click;
}

#define __GTK_FILE_CHOOSER_BUTTON_C__
#include "gtkaliasdef.c"
