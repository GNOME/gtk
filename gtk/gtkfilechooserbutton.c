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
#define DEFAULT_SPACING		0


/* ********************** *
 *  Private Enumerations  *
 * ********************** */

/* Property IDs */
enum
{
  PROP_0,

  PROP_DIALOG,
  PROP_TITLE,
  PROP_ACTIVE
};


/* ******************** *
 *  Private Structures  *
 * ******************** */

struct _GtkFileChooserButtonPrivate
{
  GtkWidget *dialog;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *button;

  gchar *filesystem;
  gulong entry_changed_id;
  gulong dialog_file_activated_id;
  gulong dialog_folder_changed_id;
  gulong dialog_selection_changed_id;
  guint update_id;
};


/* ************* *
 *  DnD Support  *
 * ************* */

enum
{
  TEXT_PLAIN,
  TEXT_URI_LIST
};

static const GtkTargetEntry drop_targets[] = {
  { "text/uri-list", 0, TEXT_URI_LIST }
};


/* ********************* *
 *  Function Prototypes  *
 * ********************* */

/* GObject Functions */
static void     gtk_file_chooser_button_set_property       (GObject          *object,
							    guint             id,
							    const GValue     *value,
							    GParamSpec       *pspec);
static void     gtk_file_chooser_button_get_property       (GObject          *object,
							    guint             id,
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

/* Child Widget Callbacks */
static void     dialog_update_preview_cb                   (GtkFileChooser   *dialog,
							    gpointer          user_data);
static void     dialog_selection_changed_cb                (GtkFileChooser   *dialog,
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
static void     entry_changed_cb                           (GtkEditable      *chooser_entry,
							    gpointer          user_data);

/* Utility Functions */
static void     gtk_file_chooser_button_set_dialog         (GObject          *object,
							    GtkWidget        *dialog);

static gboolean update_dialog                              (gpointer          user_data);
static gboolean update_dialog_idle                         (gpointer          user_data);
static void     update_entry                               (GtkFileChooserButton *button);


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

  gobject_class->set_property = gtk_file_chooser_button_set_property;
  gobject_class->get_property = gtk_file_chooser_button_get_property;

  gtkobject_class->destroy = gtk_file_chooser_button_destroy;

  widget_class->drag_data_received = gtk_file_chooser_button_drag_data_received;
  widget_class->show_all = gtk_file_chooser_button_show_all;
  widget_class->hide_all = gtk_file_chooser_button_hide_all;

  g_object_class_install_property (gobject_class, PROP_DIALOG,
				   g_param_spec_object ("dialog",
							P_("Dialog"),
							P_("The file chooser dialog to use."),
							GTK_TYPE_FILE_CHOOSER_DIALOG,
							(G_PARAM_WRITABLE |
							 G_PARAM_CONSTRUCT_ONLY)));
  g_object_class_install_property (gobject_class, PROP_TITLE,
				   g_param_spec_string ("title",
							P_("Title"),
							P_("The title of the file chooser dialog."),
							_("Select a File"),
							G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ACTIVE,
				   g_param_spec_boolean ("active",
							 P_("Active"),
							 P_("Whether the browse dialog is visible or not."),
							 FALSE, G_PARAM_READWRITE));

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserButtonPrivate));
}


static void
gtk_file_chooser_button_init (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GtkWidget *box, *image;

  gtk_box_set_spacing (GTK_BOX (button), DEFAULT_SPACING);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (button, GTK_TYPE_FILE_CHOOSER_BUTTON,
				      GtkFileChooserButtonPrivate);
  button->priv = priv;

  gtk_widget_push_composite_child ();

  priv->entry = _gtk_file_chooser_entry_new (FALSE);
  gtk_container_add (GTK_CONTAINER (button), priv->entry);

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

  priv->label = gtk_label_new (_(DEFAULT_FILENAME));
  gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_START);
  gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 2);
  gtk_widget_show (priv->label);

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
				    GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_end (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  priv->separator = gtk_vseparator_new ();
  gtk_box_pack_end (GTK_BOX (box), priv->separator, FALSE, FALSE, 0);
  gtk_widget_show (priv->separator);

  gtk_widget_pop_composite_child ();

  /* DnD */
  gtk_drag_dest_unset (priv->entry);
  gtk_drag_dest_set (GTK_WIDGET (button),
                     (GTK_DEST_DEFAULT_ALL),
		     drop_targets, G_N_ELEMENTS (drop_targets),
		     GDK_ACTION_COPY);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (button));
}


/* ******************* *
 *  GObject Functions  *
 * ******************* */


static void
gtk_file_chooser_button_set_property (GObject      *object,
				      guint         id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  switch (id)
    {
    case PROP_DIALOG:
      {
	GtkWidget *widget;

	widget = g_value_get_object (value);

	if (widget == NULL)
	  {
	    widget = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
				   "file-system-backend", priv->filesystem, NULL);
	    g_free (priv->filesystem);
	    priv->filesystem = NULL;

	    gtk_dialog_add_button (GTK_DIALOG (widget),
				   GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
	    gtk_dialog_add_button (GTK_DIALOG (widget),
				   GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	  }

	gtk_file_chooser_button_set_dialog (object, widget);
      }
      break;
    case PROP_ACTIVE:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
				    g_value_get_boolean (value));
      break;

    case GTK_FILE_CHOOSER_PROP_ACTION:
      g_object_set_property (G_OBJECT (priv->dialog), pspec->name, value);

      switch (g_value_get_enum (value))
	{
	case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
	  /* Fall through to set the widget states */
	case GTK_FILE_CHOOSER_ACTION_OPEN:
	  gtk_widget_hide (priv->entry);
	  gtk_widget_show (priv->label);
	  gtk_widget_show (priv->separator);
	  gtk_box_set_child_packing (GTK_BOX (object), priv->button,
				     TRUE, TRUE, 0, GTK_PACK_START);
	  break;

	case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
	  gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->dialog));
	  /* Fall through to set the widget states */
	case GTK_FILE_CHOOSER_ACTION_SAVE:
	  gtk_widget_show (priv->entry);
	  gtk_widget_hide (priv->label);
	  gtk_widget_hide (priv->separator);
	  gtk_box_set_child_packing (GTK_BOX (object), priv->button,
				     FALSE, FALSE, 0, GTK_PACK_START);
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
      priv->filesystem = g_value_dup_string (value);
      break;

    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
      g_warning ("%s: Choosers of type `%s` do not support selecting multiple files.",
		 G_STRFUNC, G_OBJECT_TYPE_NAME (object));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
      break;
    }
}


static void
gtk_file_chooser_button_get_property (GObject    *object,
				      guint       id,
				      GValue     *value,
				      GParamSpec *pspec)
{
  GtkFileChooserButtonPrivate *priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  switch (id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button)));
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
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
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

  if (priv->dialog != NULL)
    gtk_widget_destroy (priv->dialog);
  
  if (priv->update_id != 0)
    g_source_remove (priv->update_id);

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

	uris = g_strsplit (data->data, "\r\n", -1);

	if (uris == NULL)
	  break;

	selected = FALSE;
	g_signal_handler_block (priv->entry, priv->entry_changed_id);
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
	g_signal_handler_unblock (priv->entry, priv->entry_changed_id);

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
  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON, "title", title, NULL);
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
  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON, "title", title,
		       "file-system-backend", backend, NULL);
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

  return g_object_new (GTK_TYPE_FILE_CHOOSER_BUTTON, "dialog", dialog, NULL);
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


/* ******************* *
 *  Utility Functions  *
 * ******************* */

static void
gtk_file_chooser_button_set_dialog (GObject   *object,
				    GtkWidget *dialog)
{
  GtkFileChooserButtonPrivate *priv;
  GtkFilePath *path;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

  priv->dialog = dialog;

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
  g_signal_connect (priv->dialog, "update-preview",
		    G_CALLBACK (dialog_update_preview_cb), object);
  g_signal_connect (priv->dialog, "notify",
		    G_CALLBACK (dialog_notify_cb), object);
  g_object_add_weak_pointer (G_OBJECT (priv->dialog),
			     (gpointer *) (&priv->dialog));

  /* Kinda ugly to set this here... */
  _gtk_file_chooser_entry_set_file_system (GTK_FILE_CHOOSER_ENTRY (priv->entry),
					   _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog)));
  path = gtk_file_path_new_steal ("/");
  _gtk_file_chooser_entry_set_base_folder (GTK_FILE_CHOOSER_ENTRY (priv->entry),
					   path);
  priv->entry_changed_id = g_signal_connect_after (priv->entry, "changed",
						   G_CALLBACK (entry_changed_cb),
						   object);
}


static gchar *
get_display_name (gchar *filename)
{
  const gchar *home_dir;
  gchar *tmp;
  gsize filename_len, home_dir_len;

  filename_len = strlen (filename);

  if (g_file_test (filename, G_FILE_TEST_IS_DIR))
    {
      tmp = g_new (gchar, filename_len + 2);
      strcpy (tmp, filename);
      tmp[filename_len] = '/';
      tmp[filename_len + 1] = '\0';
      g_free (filename);
      filename = tmp;
    }

  home_dir = g_get_home_dir ();
  if (home_dir != NULL)
    {
      home_dir_len = strlen (home_dir);

      if (strncmp (home_dir, filename, home_dir_len) == 0)
	{
	  tmp = g_build_filename ("~", filename + home_dir_len, NULL);
	  g_free (filename);
	  filename = tmp;
	}
    }

  return filename;
}


static void
update_entry (GtkFileChooserButton *button)
{
  gchar *filename;

  switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (button->priv->dialog)))
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button->priv->dialog));
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
      filename = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (button->priv->dialog));
      break;
    default:
      g_assert_not_reached ();
      filename = NULL;
      break;
    }

  if (filename != NULL)
    filename = get_display_name (filename);

  g_signal_handler_block (button->priv->entry, button->priv->entry_changed_id);
  if (filename != NULL)
    gtk_entry_set_text (GTK_ENTRY (button->priv->entry), filename);
  else
    gtk_entry_set_text (GTK_ENTRY (button->priv->entry), "");
  g_signal_handler_unblock (button->priv->entry, button->priv->entry_changed_id);

  if (filename != NULL)
    gtk_label_set_text (GTK_LABEL (button->priv->label), filename);
  else
    gtk_label_set_text (GTK_LABEL (button->priv->label), _(DEFAULT_FILENAME));
  g_free (filename);
}


static gboolean
update_dialog (gpointer user_data)
{
  GtkFileChooserButtonPrivate *priv;
  const GtkFilePath *folder_path;
  const gchar *file_part;
  gchar *full_uri;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);
  folder_path =
    _gtk_file_chooser_entry_get_current_folder (GTK_FILE_CHOOSER_ENTRY (priv->entry));
  file_part =
    _gtk_file_chooser_entry_get_file_part (GTK_FILE_CHOOSER_ENTRY (priv->entry));

  if (folder_path != NULL)
    full_uri = g_build_filename (gtk_file_path_get_string (folder_path),
				 file_part, NULL);
  else if (file_part != NULL)
    full_uri = g_build_filename ("file://", file_part, NULL);
  else
    full_uri = NULL;

  if (full_uri != NULL)
    {
      gchar *display_name;

      display_name = g_filename_from_uri (full_uri, NULL, NULL);
      if (display_name)
	{
	  display_name = get_display_name (display_name);
	  gtk_label_set_text (GTK_LABEL (priv->label), display_name);
	  g_free (display_name);
	}
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->label), _(DEFAULT_FILENAME));
    }

  switch (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (priv->dialog)))
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      if (folder_path != NULL)
	{
	  GtkFileSystem *fs;
	  GtkFileFolder *folder;
	  GtkFilePath *full_path;
	  GtkFileInfo *info;

	  fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));
	  folder = gtk_file_system_get_folder (fs, folder_path,
					       GTK_FILE_INFO_IS_FOLDER, NULL);

	  full_path = gtk_file_system_make_path (fs, folder_path, file_part, NULL);
	  info = gtk_file_folder_get_info (folder, full_path, NULL);

	  /* Entry contents don't exist. */
	  if (info == NULL)
	    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						       folder_path, NULL);
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
      else
	g_free (full_uri);
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      if (folder_path != NULL)
	{
	  GtkFileSystem *fs;
	  GtkFilePath *full_path;

	  fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));
	  full_path = gtk_file_system_make_path (fs, folder_path, file_part, NULL);

	  /* Entry contents don't exist. */
	  if (full_path != NULL)
	    _gtk_file_chooser_select_path (GTK_FILE_CHOOSER (priv->dialog),
					   full_path, NULL);
	  else
	    _gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						       folder_path, NULL);

	  gtk_file_path_free (full_path);
	}
      else
	g_free (full_uri);
      break;

    case GTK_FILE_CHOOSER_ACTION_SAVE:
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
      if (folder_path != NULL)
	_gtk_file_chooser_set_current_folder_path (GTK_FILE_CHOOSER (priv->dialog),
						   folder_path, NULL);

      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (priv->dialog),
					 file_part);
      g_free (full_uri);
      break;
    }
  
  priv->update_id = 0;
  return FALSE;
}

static gboolean
update_dialog_idle (gpointer user_data)
{
  gboolean result;

  GDK_THREADS_ENTER ();
  result = update_dialog (user_data);
  GDK_THREADS_LEAVE ();

  return result;

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
  update_entry (user_data);
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
      update_entry (user_data);

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
			      priv->dialog_selection_changed_id);
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
				priv->dialog_selection_changed_id);
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


/* Ensure the button height == entry height */
static void
entry_size_allocate_cb (GtkWidget     *entry,
			GtkAllocation *allocation,
			gpointer       user_data)
{
  gtk_widget_set_size_request (user_data, -1, allocation->height);
}


static void
entry_changed_cb (GtkEditable *chooser_entry,
		  gpointer     user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  /* We do this in an idle handler to avoid totally screwing up chooser_entry's
   * completion */
  if (priv->update_id != 0)
    priv->update_id = g_idle_add (update_dialog_idle, user_data);
}
