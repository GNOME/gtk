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

#include "gtkalias.h"
#include "gtkintl.h"
#include "gtkdnd.h"
#include "gtkentry.h"
#include "gtkhbox.h"
#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkstock.h"
#include "gtktogglebutton.h"
#include "gtkvseparator.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooserentry.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserutils.h"

#include "gtkfilechooserbutton.h"


/* **************** *
 *  Private Macros  *
 * **************** */

#define GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE(object) (GTK_FILE_CHOOSER_BUTTON ((object))->priv)

#define DEFAULT_FILENAME	N_("(None)")
#define MIN_LABEL_WIDTH		100
#define ENTRY_BUTTON_SPACING	0
#define FALLBACK_ICON_SIZE	20
#define FALLBACK_ICON_NAME	"stock_unknown"
#define NEW_FILE_ICON_NAME	"stock_new"
#define NEW_DIR_ICON_NAME	"stock_new-dir"

/* ********************** *
 *  Private Enumerations  *
 * ********************** */

/* Property IDs */
enum
{
  PROP_0,

  PROP_DIALOG,
  PROP_TITLE,
  PROP_ACTIVE,
  PROP_WIDTH_CHARS
};


/* ******************** *
 *  Private Structures  *
 * ******************** */

struct _GtkFileChooserButtonPrivate
{
  GtkWidget *dialog;
  GtkWidget *accept_button;
  GtkWidget *entry_box;
  GtkWidget *entry_image;
  GtkWidget *entry;
  GtkWidget *label_box;
  GtkWidget *label_image;
  GtkWidget *label;
  GtkWidget *button;

  gchar *backend;
  gulong entry_changed_id;
  gulong dialog_file_activated_id;
  gulong dialog_folder_changed_id;
  gulong dialog_selection_changed_id;
  gulong dialog_selection_changed_proxy_id;
  gulong settings_signal_id;
  guint update_id;
  gint icon_size;
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
static gboolean gtk_file_chooser_button_mnemonic_activate  (GtkWidget        *widget,
							    gboolean          group_cycling);
static void     gtk_file_chooser_button_style_set          (GtkWidget        *widget,
							    GtkStyle         *old_style);
static void     gtk_file_chooser_button_screen_changed     (GtkWidget        *widget,
							    GdkScreen        *old_screen);

/* Child Widget Callbacks */
static void     dialog_update_preview_cb                   (GtkFileChooser   *dialog,
							    gpointer          user_data);
static void     dialog_selection_changed_cb                (GtkFileChooser   *dialog,
							    gpointer          user_data);
static void     dialog_selection_changed_proxy_cb          (GtkFileChooser   *dialog,
							    gpointer          user_data);
static void     dialog_file_activated_cb                   (GtkFileChooser   *dialog,
							    gpointer          user_data);
static void     dialog_current_folder_changed_cb           (GtkFileChooser   *dialog,
							    gpointer          user_data);
static void     dialog_notify_cb                           (GObject          *dialog,
							    GParamSpec       *pspec,
							    gpointer          user_data);
static gboolean dialog_delete_event_cb                     (GtkWidget        *dialog,
							    GdkEvent         *event,
							    gpointer          user_data);
static void     dialog_response_cb                         (GtkFileChooser   *dialog,
							    gint              response,
							    gpointer          user_data);

static void     button_toggled_cb                          (GtkToggleButton  *real_button,
							    gpointer          user_data);
static void     button_notify_active_cb                    (GObject          *real_button,
							    GParamSpec       *pspec,
							    gpointer          user_data);

static void     entry_size_allocate_cb                     (GtkWidget        *entry,
							    GtkAllocation    *allocation,
							    gpointer          user_data);
static void     entry_changed_cb                           (GtkEditable      *editable,
							    gpointer          user_data);

/* Utility Functions */
static void     remove_settings_signal                     (GtkFileChooserButton *button,
							    GdkScreen            *screen);

static void     update_dialog                              (GtkFileChooserButton *button);
static void     update_entry                               (GtkFileChooserButton *button);
static void     update_label                               (GtkFileChooserButton *button);
static void     update_icons                               (GtkFileChooserButton *button);


/* ******************* *
 *  GType Declaration  *
 * ******************* */

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserButton, gtk_file_chooser_button, GTK_TYPE_HBOX, { \
    G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER, _gtk_file_chooser_delegate_iface_init) \
});


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

  gtkobject_class->destroy = gtk_file_chooser_button_destroy;

  widget_class->drag_data_received = gtk_file_chooser_button_drag_data_received;
  widget_class->show_all = gtk_file_chooser_button_show_all;
  widget_class->hide_all = gtk_file_chooser_button_hide_all;
  widget_class->show = gtk_file_chooser_button_show;
  widget_class->hide = gtk_file_chooser_button_hide;
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
							GTK_TYPE_FILE_CHOOSER_DIALOG,
							(G_PARAM_WRITABLE |
							 G_PARAM_CONSTRUCT_ONLY)));

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
							_("Select a File"),
							G_PARAM_READWRITE));

  /**
   * GtkFileChooserButton:active:
   * 
   * %TRUE, if the #GtkFileChooserDialog associated with the button has been
   * made visible.  This can also be set by the application, though it is
   * rarely useful to do so.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class, PROP_ACTIVE,
				   g_param_spec_boolean ("active",
							 P_("Active"),
							 P_("Whether the browse dialog is visible or not."),
							 FALSE, G_PARAM_READWRITE));

  /**
   * GtkFileChooserButton:
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
						     G_PARAM_READWRITE));

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserButtonPrivate));
}


static void
gtk_file_chooser_button_init (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GtkWidget *box, *image, *sep;

  gtk_box_set_spacing (GTK_BOX (button), ENTRY_BUTTON_SPACING);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (button, GTK_TYPE_FILE_CHOOSER_BUTTON,
				      GtkFileChooserButtonPrivate);
  button->priv = priv;

  priv->icon_size = FALLBACK_ICON_SIZE;

  gtk_widget_push_composite_child ();

  priv->entry_box = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (button), priv->entry_box);

  priv->entry_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (priv->entry_box), priv->entry_image,
		      FALSE, FALSE, 0);
  gtk_widget_show (priv->entry_image);

  priv->entry = _gtk_file_chooser_entry_new (FALSE);
  gtk_container_add (GTK_CONTAINER (priv->entry_box), priv->entry);
  gtk_widget_show (priv->entry);

  priv->button = gtk_toggle_button_new ();
  g_signal_connect (priv->button, "toggled",
		    G_CALLBACK (button_toggled_cb), button);
  g_signal_connect (priv->button, "notify::active",
		    G_CALLBACK (button_notify_active_cb), button);
  g_signal_connect (priv->entry, "size-allocate",
		    G_CALLBACK (entry_size_allocate_cb), priv->button);
  gtk_box_pack_start (GTK_BOX (button), priv->button, TRUE, TRUE, 0);
  gtk_widget_show (priv->button);

  box = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (priv->button), box);
  gtk_widget_show (box);
  
  priv->label_box = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (box), priv->label_box);
  gtk_widget_show (priv->label_box);

  priv->label_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (priv->label_box), priv->label_image,
		      FALSE, FALSE, 0);
  gtk_widget_show (priv->label_image);

  priv->label = gtk_label_new (_(DEFAULT_FILENAME));
  gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_START);
  gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (priv->label_box), priv->label);
  gtk_widget_show (priv->label);

  sep = gtk_vseparator_new ();
  gtk_box_pack_end (GTK_BOX (priv->label_box), sep, FALSE, FALSE, 0);
  gtk_widget_show (sep);

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
				    GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_end (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  gtk_widget_pop_composite_child ();

  /* DnD */
  gtk_drag_dest_unset (priv->entry);
  gtk_drag_dest_set (GTK_WIDGET (button),
                     (GTK_DEST_DEFAULT_ALL),
		     NULL, 0,
		     GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (button));
  gtk_target_list_add_uri_targets (gtk_drag_dest_get_target_list (GTK_WIDGET (button)), 
				   TEXT_URI_LIST);
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
  GtkFileChooserButtonPrivate *priv;
  GtkFilePath *path;

  object = (*G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->constructor) (type,
										  n_params,
										  params);
  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  if (!priv->dialog)
    {
      if (priv->backend)
	priv->dialog = gtk_file_chooser_dialog_new_with_backend (NULL, NULL,
								 GTK_FILE_CHOOSER_ACTION_OPEN,
								 priv->backend, NULL);
      else
	priv->dialog = gtk_file_chooser_dialog_new (NULL, NULL,
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    NULL);

      gtk_dialog_add_button (GTK_DIALOG (priv->dialog),
			     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
      priv->accept_button = gtk_dialog_add_button (GTK_DIALOG (priv->dialog),
						   GTK_STOCK_OPEN,
						   GTK_RESPONSE_ACCEPT);
      gtk_dialog_set_default_response (GTK_DIALOG (priv->dialog),
				       GTK_RESPONSE_ACCEPT);

      gtk_dialog_set_alternative_button_order (GTK_DIALOG (priv->dialog),
					       GTK_RESPONSE_ACCEPT,
					       GTK_RESPONSE_CANCEL,
					       -1);
    }

  g_free (priv->backend);
  priv->backend = NULL;

  g_signal_connect (priv->dialog, "delete-event",
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
  priv->dialog_selection_changed_proxy_id =
    g_signal_connect (priv->dialog, "selection-changed",
		      G_CALLBACK (dialog_selection_changed_proxy_cb), object);
  g_signal_connect (priv->dialog, "update-preview",
		    G_CALLBACK (dialog_update_preview_cb), object);
  g_signal_connect (priv->dialog, "notify",
		    G_CALLBACK (dialog_notify_cb), object);
  g_object_add_weak_pointer (G_OBJECT (priv->dialog),
			     (gpointer *) (&priv->dialog));

  _gtk_file_chooser_entry_set_file_system (GTK_FILE_CHOOSER_ENTRY (priv->entry),
					   _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog)));
  path = gtk_file_path_new_steal ("/");
  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (priv->entry),
					   path);
  priv->entry_changed_id = g_signal_connect (priv->entry, "changed",
					     G_CALLBACK (entry_changed_cb),
					     object);

  update_label (GTK_FILE_CHOOSER_BUTTON (object));

  return object;
}

static void
gtk_file_chooser_button_set_property (GObject      *object,
				      guint         param_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_DIALOG:
      /* Construct-only */
      priv->dialog = g_value_get_object (value);
      break;
    case PROP_ACTIVE:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
				    g_value_get_boolean (value));
      break;
    case PROP_WIDTH_CHARS:
      gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (object),
					       g_value_get_int (value));
      break;

    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_object_set_property (G_OBJECT (priv->dialog), pspec->name, value);
      _gtk_file_chooser_entry_set_action (GTK_FILE_CHOOSER_ENTRY (priv->entry),
					  (GtkFileChooserAction) g_value_get_enum (value));
      update_icons (GTK_FILE_CHOOSER_BUTTON (object));

      switch (g_value_get_enum (value))
	{
	case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
	  /* Fall through to set the widget states */
	case GTK_FILE_CHOOSER_ACTION_OPEN:
	  gtk_widget_hide (priv->entry_box);
	  gtk_widget_show (priv->label_box);
	  gtk_box_set_child_packing (GTK_BOX (object), priv->button,
				     TRUE, TRUE, 0, GTK_PACK_START);
	  gtk_button_set_label (GTK_BUTTON (priv->accept_button),
				GTK_STOCK_OPEN);
	  break;

	case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
	  gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
	  /* Fall through to set the widget states */
	case GTK_FILE_CHOOSER_ACTION_SAVE:
	  gtk_widget_show (priv->entry_box);
	  gtk_widget_hide (priv->label_box);
	  gtk_box_set_child_packing (GTK_BOX (object), priv->button,
				     FALSE, FALSE, 0, GTK_PACK_START);
	  gtk_button_set_label (GTK_BUTTON (priv->accept_button),
				GTK_STOCK_SAVE);
	  break;
	}
      break;

    case PROP_TITLE:
    case GTK_FILE_CHOOSER_PROP_FILTER:
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET:
    case GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE:
    case GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL:
    case GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET:
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN:
      g_object_set_property (G_OBJECT (priv->dialog), pspec->name, value);
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
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value,
			   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)));
      break;
    case PROP_WIDTH_CHARS:
      g_value_set_int (value,
		       gtk_entry_get_width_chars (GTK_ENTRY (priv->entry)));
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
      g_object_get_property (G_OBJECT (priv->dialog), pspec->name, value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


/* ********************* *
 *  GtkObject Functions  *
 * ********************* */

static void
gtk_file_chooser_button_destroy (GtkObject * object)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  if (priv->update_id)
    g_source_remove (priv->update_id);

  if (priv->dialog != NULL)
    gtk_widget_destroy (priv->dialog);
  
  remove_settings_signal (GTK_FILE_CHOOSER_BUTTON (object),
			  gtk_widget_get_screen (GTK_WIDGET (object)));

  if (GTK_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->destroy != NULL)
    (*GTK_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->destroy) (object);
}


/* ********************* *
 *  GtkWidget Functions  *
 * ********************* */

static void
gtk_file_chooser_button_drag_data_received (GtkWidget	     *widget,
					    GdkDragContext   *context,
					    gint	      x,
					    gint	      y,
					    GtkSelectionData *data,
					    guint	      info,
					    guint	      drag_time)
{
  GtkFileChooserButtonPrivate *priv;
  gchar *text;

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->drag_data_received != NULL)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->drag_data_received) (widget,
										    context,
										    x, y,
										    data, info,
										    drag_time);

  if (widget == NULL || context == NULL || data == NULL || data->length < 0)
    return;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (widget);

  switch (info)
    {
    case TEXT_URI_LIST:
      {
	gchar **uris;
	guint i;
	gboolean selected;

	uris = gtk_selection_data_get_uris (data);

	if (uris == NULL)
	  break;

	selected = FALSE;
	for (i = 0; !selected && uris[i] != NULL; i++)
	  {
	    GtkFileSystem *fs;
	    GtkFilePath *path, *base_path;

	    fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));
	    path = gtk_file_system_uri_to_path (fs, uris[i]);

	    base_path = NULL;
	    if (path != NULL &&
		gtk_file_system_get_parent (fs, path, &base_path, NULL))
	      {
		GtkFileFolder *folder;
		GtkFileInfo *info;

		folder = gtk_file_system_get_folder (fs, base_path,
						     GTK_FILE_INFO_IS_FOLDER,
						     NULL);

		info = gtk_file_folder_get_info (folder, path, NULL);

		if (info != NULL)
		  {
		    GtkFileChooserAction action;

		    g_object_get (priv->dialog, "action", &action, NULL);

		    selected = 
		      ((((action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER ||
			  action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) &&
			 gtk_file_info_get_is_folder (info)) ||
			((action == GTK_FILE_CHOOSER_ACTION_OPEN ||
			  action == GTK_FILE_CHOOSER_ACTION_SAVE) &&
			 !gtk_file_info_get_is_folder (info))) &&
			_gtk_file_chooser_select_path (GTK_FILE_CHOOSER (priv->dialog),
						       path, NULL));

		    gtk_file_info_free (info);
		  }
		else
		  selected = FALSE;

		gtk_file_path_free (base_path);
	      }

	    gtk_file_path_free (path);
	  }

	g_strfreev (uris);
      }
      break;

    case TEXT_PLAIN:
      text = gtk_selection_data_get_text (data);
      gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
      g_free (text);
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
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (widget);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show) (widget);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)))
    gtk_widget_show (priv->dialog);
}

static void
gtk_file_chooser_button_hide (GtkWidget *widget)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (widget);

  gtk_widget_hide (priv->dialog);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->hide)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->hide) (widget);
}

static gboolean
gtk_file_chooser_button_mnemonic_activate (GtkWidget *widget,
					   gboolean   group_cycling)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (widget);

  switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (priv->dialog)))
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      gtk_widget_grab_focus (priv->button);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
      gtk_widget_grab_focus (priv->entry);
      break;
    }

  return TRUE;
}

/* Changes the icons wherever it is needed */
static void
change_icon_theme (GtkFileChooserButton *button)
{
  GtkSettings *settings;
  gint width, height;

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (button)));

  if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_SMALL_TOOLBAR,
					 &width, &height))
    button->priv->icon_size = MAX (width, height);
  else
    button->priv->icon_size = FALLBACK_ICON_SIZE;

  update_icons (button);
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject    *object,
		    GParamSpec *pspec,
		    gpointer    user_data)
{
  const char *name;

  name = g_param_spec_get_name (pspec);

  if (strcmp (name, "gtk-icon-theme-name") == 0
      || strcmp (name, "gtk-icon-sizes") == 0)
    change_icon_theme (user_data);
}

/* Installs a signal handler for GtkSettings so that we can monitor changes in
 * the icon theme.
 */
static void
check_icon_theme (GtkFileChooserButton *button)
{
  GtkSettings *settings;

  if (button->priv->settings_signal_id)
    return;

  if (gtk_widget_has_screen (GTK_WIDGET (button)))
    {
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (button)));
      button->priv->settings_signal_id = g_signal_connect (settings, "notify",
							   G_CALLBACK (settings_notify_cb),
							   button);

      change_icon_theme (button);
    }
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
  GtkFileChooserButton *button;

  button = GTK_FILE_CHOOSER_BUTTON (widget);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->screen_changed)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->screen_changed) (widget,
										old_screen);

  remove_settings_signal (button, old_screen);
  check_icon_theme (button); 
}


/* ************************************************************************** *
 *  Public API                                                                *
 * ************************************************************************** */

/**
 * gtk_file_chooser_button_new:
 * @title: the title of the browse dialog.
 * 
 * Creates a new file-selecting button widget.
 * 
 * Returns: a new button widget.
 * 
 * Since: 2.6
 **/
GtkWidget *
gtk_file_chooser_button_new (const gchar *title)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON,
		       "title", title,
		       NULL);
}

/**
 * gtk_file_chooser_button_new_with_backend:
 * @title: the title of the browse dialog.
 * @backend: the name of the #GtkFileSystem backend to use.
 * 
 * Creates a new file-selecting button widget using @backend.
 * 
 * Returns: a new button widget.
 * 
 * Since: 2.6
 **/
GtkWidget *
gtk_file_chooser_button_new_with_backend (const gchar *title,
					  const gchar *backend)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON,
		       "title", title,
		       "file-system-backend", backend,
		       NULL);
}

/**
 * gtk_file_chooser_button_new_with_dialog:
 * @dialog: the #GtkDialog widget to use.
 * 
 * Creates a #GtkFileChooserButton widget which uses @dialog as it's
 * file-picking window. Note that @dialog must be a #GtkFileChooserDialog (or
 * subclass).
 * 
 * Returns: a new button widget.
 * 
 * Since: 2.6
 **/
GtkWidget *
gtk_file_chooser_button_new_with_dialog (GtkWidget *dialog)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), NULL);

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
 * gtk_file_chooser_button_set_active:
 * @button: the button widget to modify.
 * @is_active: whether or not the dialog is visible.
 * 
 * Modifies whether or not the dialog attached to @button is visible or not.
 * 
 * Since: 2.6
 **/
void
gtk_file_chooser_button_set_active (GtkFileChooserButton *button,
				    gboolean		  is_active)
{
  g_return_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->priv->button), is_active);
}

/**
 * gtk_file_chooser_button_get_active:
 * @button: the button widget to examine.
 * 
 * Retrieves whether or not the dialog attached to @button is visible.
 * 
 * Returns: a boolean whether the dialog is visible or not.
 * 
 * Since: 2.6
 **/
gboolean
gtk_file_chooser_button_get_active (GtkFileChooserButton *button)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_BUTTON (button), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button->priv->button));
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

  return gtk_entry_get_width_chars (GTK_ENTRY (button->priv->entry));
}

/**
 * gtk_file_chooser_button_set_width_chars:
 * @button: the button widget to examine.
 * @n_chars: the new width, in chracters.
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

  gtk_entry_set_width_chars (GTK_ENTRY (button->priv->entry), n_chars);
  gtk_label_set_width_chars (GTK_LABEL (button->priv->label), n_chars);
  g_object_notify (G_OBJECT (button), "width-chars");
}


/* ******************* *
 *  Utility Functions  *
 * ******************* */

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (GtkFileChooserButton *button,
			GdkScreen            *screen)
{
  if (button->priv->settings_signal_id)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_for_screen (screen);
      g_signal_handler_disconnect (settings,
				   button->priv->settings_signal_id);
      button->priv->settings_signal_id = 0;
    }
}

static GtkIconTheme *
get_icon_theme (GtkWidget *widget)
{
  if (gtk_widget_has_screen (widget))
    return gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

  return gtk_icon_theme_get_default ();
}

static gboolean
check_if_path_exists (GtkFileSystem     *fs,
		      const GtkFilePath *path)
{
  gboolean path_exists;
  GtkFilePath *parent_path;

  path_exists = FALSE;
  parent_path = NULL;

  if (gtk_file_system_get_parent (fs, path, &parent_path, NULL))
    {
      GtkFileFolder *folder;
    
      folder = gtk_file_system_get_folder (fs, parent_path, 0, NULL);
      if (folder)
	{
	  GtkFileInfo *info;
	
	  info = gtk_file_folder_get_info (folder, path, NULL);
	  if (info)
	    {
	      path_exists = TRUE;
	      gtk_file_info_free (info);
	    }

	  g_object_unref (folder);
	}
    
      gtk_file_path_free (parent_path);
    }

  return path_exists;
}

static void
update_icons (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GdkPixbuf *pixbuf;
  GSList *paths;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (button);
  pixbuf = NULL;
  paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (priv->dialog));

  if (paths)
    {
      GtkFilePath *path;
      GtkFileSystem *fs;

      path = paths->data;
      fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));

      switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (priv->dialog)))
	{
	case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  {
	    GtkFileSystemVolume *volume;
      
	    volume = gtk_file_system_get_volume_for_path (fs, path);
	    if (volume)
	      {
		GtkFilePath *base_path;

		base_path = gtk_file_system_volume_get_base_path (fs, volume);
		
		if (base_path && gtk_file_path_compare (base_path, path) == 0)
		  pixbuf = gtk_file_system_volume_render_icon (fs, volume,
							       GTK_WIDGET (button),
							       priv->icon_size,
							       NULL);

		if (base_path)
		  gtk_file_path_free (base_path);

		gtk_file_system_volume_free (fs, volume);
	      }
	  }
	
	case GTK_FILE_CHOOSER_ACTION_OPEN:
	  if (!pixbuf)
	    pixbuf = gtk_file_system_render_icon (fs, path, GTK_WIDGET (button),
						  priv->icon_size, NULL);
	  break;

	case GTK_FILE_CHOOSER_ACTION_SAVE:
	  if (check_if_path_exists (fs, path))
	    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (button)),
					       GTK_STOCK_DIALOG_WARNING,
					       priv->icon_size, 0, NULL);
	  else
	    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (button)),
					       NEW_FILE_ICON_NAME,
					       priv->icon_size, 0, NULL);
	  break;
	case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
	  if (check_if_path_exists (fs, path))
	    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (button)),
					       GTK_STOCK_DIALOG_WARNING,
					       priv->icon_size, 0, NULL);
	  else
	    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (button)),
					       NEW_DIR_ICON_NAME,
					       priv->icon_size, 0, NULL);
	  break;
	}

      gtk_file_paths_free (paths);
    }

  if (!pixbuf)
    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (button)),
				       FALLBACK_ICON_NAME,
				       priv->icon_size, 0, NULL);

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->entry_image), pixbuf);
  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->label_image), pixbuf);

  if (pixbuf)
    g_object_unref (pixbuf);
}


static void
update_label (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  gchar *label_text;
  GSList *paths;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (button);
  paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (button->priv->dialog));
  label_text = NULL;

  if (paths)
    {
      GtkFileSystem *fs;
      GtkFilePath *path;
      GtkFileSystemVolume *volume;
    
      path = paths->data;

      fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));

      volume = gtk_file_system_get_volume_for_path (fs, path);
      if (volume)
	{
	  GtkFilePath *base_path;

	  base_path = gtk_file_system_volume_get_base_path (fs, volume);
	  if (base_path && gtk_file_path_compare (base_path, path) == 0)
	    label_text = gtk_file_system_volume_get_display_name (fs, volume);

	  if (base_path)
	    gtk_file_path_free (base_path);

	  gtk_file_system_volume_free (fs, volume);

	  if (label_text)
	    goto out;
	}
    
      if (gtk_file_system_path_is_local (fs, path))
	{
	  const gchar *home;
	  gchar *tmp;
	  gchar *filename;

          filename = gtk_file_system_path_to_filename (fs, path);

	  if (!filename)
	    goto out;

	  home = g_get_home_dir ();

	  /* Munging for psuedo-volumes and files in the user's home tree */
	  if (home)
	    {
	      if (strcmp (filename, home) == 0)
		{
		  label_text = g_strdup (_("Home"));
		  goto localout;
		}

	      tmp = g_build_filename (home, "Desktop", NULL);

	      if (strcmp (filename, tmp) == 0)
		label_text = g_strdup (_("Desktop"));

	      g_free (tmp);

	      if (label_text)
		goto out;

	      if (g_str_has_prefix (filename, home))
		{
		  label_text = g_strconcat ("~", filename + strlen (home), NULL);
		  goto localout;
		}
	    }

	  if (!label_text)
	    label_text = g_strdup (filename);

	 localout:
	  g_free (filename);
	}
      else
	{
	  gchar *uri;

	  uri = gtk_file_system_path_to_uri (fs, path);

	  if (uri)
	    label_text = uri;
	}

     out:
      gtk_file_paths_free (paths);
    }

  if (label_text)
    {
      gtk_label_set_text (GTK_LABEL (priv->label), label_text);
      g_free (label_text);
    }
  else
    gtk_label_set_text (GTK_LABEL (priv->label), _(DEFAULT_FILENAME));
}

static void
update_entry (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GSList *paths;
  gchar *filename;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (button);

  paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (priv->dialog));

  if (paths)
    {
      GtkFileSystem *fs;
      GtkFilePath *path;
    
      path = paths->data;
      fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));
    
      if (gtk_file_system_path_is_local (fs, path))
	{
	  filename = gtk_file_system_path_to_filename (fs, path);

	  if (filename)
	    {
	      const gchar *home;
	      gchar *tmp;

	      if (g_file_test (filename, G_FILE_TEST_IS_DIR))
		{
		  tmp = g_strconcat (filename, "/", NULL);
		  g_free (filename);
		  filename = tmp;
		}

	      home = g_get_home_dir ();

	      if (home && g_str_has_prefix (filename, home))
		{
		  tmp = g_strconcat ("~", filename + strlen (home), NULL);
		  g_free (filename);
		  filename = tmp;
		}
	    }
	}
      else
	filename = gtk_file_system_path_to_uri (fs, path);
    }
  else
    filename = NULL;

  if (filename)
    {
      gchar *entry_text;
    
      entry_text = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_free (filename);

      gtk_entry_set_text (GTK_ENTRY (priv->entry), entry_text);
      g_free (entry_text);
    }
  else
    gtk_entry_set_text (GTK_ENTRY (priv->entry), "");
}

static void
update_dialog (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GtkFilePath *current_folder;
  GtkFileSystem *fs;
  GtkFilePath *folder_part, *full_path;
  gchar *file_part;
  const gchar *text;
  GtkFilePath *base_path;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (button);
  file_part = NULL;
  folder_part = NULL;
  fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));

  text = gtk_entry_get_text (GTK_ENTRY (priv->entry));

  base_path = gtk_file_path_new_dup ("/");
  gtk_file_system_parse (fs, base_path, text, &folder_part, &file_part, NULL);
  gtk_file_path_free (base_path);

  switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (priv->dialog)))
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
      if (folder_part)
	{
	  GtkFileFolder *folder;
	  GtkFileInfo *info;

	  folder = gtk_file_system_get_folder (fs, folder_part,
					       GTK_FILE_INFO_IS_FOLDER, NULL);

	  full_path = gtk_file_system_make_path (fs, folder_part, file_part, NULL);
	  info = gtk_file_folder_get_info (folder, full_path, NULL);

	  /* Entry contents don't exist. */
	  if (info == NULL)
	    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						       folder_part, NULL);
	  /* Entry contents are a folder */
	  else if (gtk_file_info_get_is_folder (info))
	    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						       full_path, NULL);
	  /* Entry contents must be a file. */
	  else
	    _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (priv->dialog),
					   full_path, NULL);

	  if (info)
	    gtk_file_info_free (info);

	  gtk_file_path_free (full_path);
	}
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
      if (folder_part)
	{
	  full_path = gtk_file_system_make_path (fs, folder_part, file_part, NULL);

	  /* Entry contents don't exist. */
	  if (full_path)
	    _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (priv->dialog),
					   full_path, NULL);
	  else
	    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						       folder_part, NULL);

	  gtk_file_path_free (full_path);
	}
      break;

    case GTK_FILE_CHOOSER_ACTION_SAVE:
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
      gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
      if (folder_part)
	{
	  current_folder = _gtk_file_chooser_get_current_folder_path (GTK_FILE_CHOOSER (priv->dialog));

	  if (!current_folder ||
	      gtk_file_path_compare (current_folder, folder_part) != 0)
	    {
	      _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
							 folder_part, NULL);
	      g_signal_emit_by_name (button, "current-folder-changed");
	    }

	  if (current_folder)
	    gtk_file_path_free (current_folder);
	}

      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (priv->dialog),
					 file_part);
      g_signal_emit_by_name (button, "selection-changed");
      break;
    }
}

/* ************************ *
 *  Child-Widget Callbacks  *
 * ************************ */

static void
dialog_current_folder_changed_cb (GtkFileChooser *dialog,
				  gpointer        user_data)
{
  g_signal_emit_by_name (user_data, "current-folder-changed");
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
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  g_signal_handler_block (priv->entry, priv->entry_changed_id);
  update_entry (user_data);
  g_signal_handler_unblock (priv->entry, priv->entry_changed_id);
  update_icons (user_data);
  update_label (user_data);
}

static void
dialog_selection_changed_proxy_cb (GtkFileChooser *dialog,
				   gpointer        user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

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
dialog_response_cb (GtkFileChooser *dialog,
		    gint            response,
		    gpointer        user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      g_signal_handler_block (priv->entry, priv->entry_changed_id);
      update_entry (user_data);
      g_signal_handler_unblock (priv->entry, priv->entry_changed_id);
      update_label (user_data);
      update_icons (user_data);

      g_signal_emit_by_name (user_data, "current-folder-changed");
      g_signal_emit_by_name (user_data, "selection-changed");
    }
  else
    {
      update_dialog (user_data);
    }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button), FALSE);
}


static void
button_toggled_cb (GtkToggleButton *real_button,
		   gpointer         user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  if (gtk_toggle_button_get_active (real_button))
    {
      /* Setup the dialog parent to be chooser button's toplevel, and be modal
	 as needed. */
      if (!GTK_WIDGET_VISIBLE (priv->dialog))
	{
	  GtkWidget *toplevel;

	  toplevel = gtk_widget_get_toplevel (user_data);

	  if (GTK_WIDGET_TOPLEVEL (toplevel) && GTK_IS_WINDOW (toplevel))
	    {
	      if (GTK_WINDOW (toplevel) != gtk_window_get_transient_for (GTK_WINDOW (priv->dialog)))
		gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), GTK_WINDOW (toplevel));
	      
	      gtk_window_set_modal (GTK_WINDOW (priv->dialog),
				    gtk_window_get_modal (GTK_WINDOW (toplevel)));
	    }
	}

      g_signal_handler_block (priv->dialog,
			      priv->dialog_folder_changed_id);
      g_signal_handler_block (priv->dialog,
			      priv->dialog_file_activated_id);
      g_signal_handler_block (priv->dialog,
			      priv->dialog_selection_changed_proxy_id);
      gtk_widget_set_sensitive (priv->entry, FALSE);
      gtk_window_present (GTK_WINDOW (priv->dialog));
    }
  else
    {
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_folder_changed_id);
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_file_activated_id);
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_selection_changed_proxy_id);
      gtk_widget_set_sensitive (priv->entry, TRUE);
      gtk_widget_hide (priv->dialog);
    }
}

static void
button_notify_active_cb (GObject    *real_button,
			 GParamSpec *pspec,
			 gpointer    user_data)
{
  g_object_notify (user_data, "active");
}


static gboolean
update_idler (gpointer user_data)
{
  GtkFileChooserButtonPrivate *priv;
  gboolean retval;

  GDK_THREADS_ENTER ();

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (priv->entry),
					  NULL, NULL))
    {
      g_signal_handler_block (priv->dialog,
			      priv->dialog_selection_changed_id);
      update_dialog (user_data);
      g_signal_handler_unblock (priv->dialog,
				priv->dialog_selection_changed_id);
      update_icons (user_data);
      update_label (user_data);
      priv->update_id = 0;
      retval = FALSE;
    }
  else
    retval = TRUE;
  
  GDK_THREADS_LEAVE ();

  return retval;
}

static void
entry_changed_cb (GtkEditable *editable,
		  gpointer     user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  if (priv->update_id)
    g_source_remove (priv->update_id);

  priv->update_id = g_idle_add_full (G_PRIORITY_LOW, update_idler,
				     user_data, NULL);
}

/* Ensure the button height == entry height */
static void
entry_size_allocate_cb (GtkWidget     *entry,
			GtkAllocation *allocation,
			gpointer       user_data)
{
  gtk_widget_set_size_request (user_data, -1, allocation->height);
}
