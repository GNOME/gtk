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
#include "gtkbutton.h"
#include "gtkdnd.h"
#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkstock.h"
#include "gtkvseparator.h"
#include "gtkfilechooserdialog.h"
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

/* ********************** *
 *  Private Enumerations  *
 * ********************** */

/* Property IDs */
enum
{
  PROP_0,

  PROP_DIALOG,
  PROP_TITLE,
  PROP_WIDTH_CHARS
};


/* ******************** *
 *  Private Structures  *
 * ******************** */

struct _GtkFileChooserButtonPrivate
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;

  GtkFilePath *old_path;
  gchar *backend;
  gulong dialog_file_activated_id;
  gulong dialog_folder_changed_id;
  gulong dialog_selection_changed_id;
  gint icon_size;

  /* Used for hiding/showing the dialog when the button is hidden */
  guint8 active : 1;
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
static void     dialog_response_cb                         (GtkDialog        *dialog,
							    gint              response,
							    gpointer          user_data);

static void     button_clicked_cb                          (GtkButton        *real_button,
							    gpointer          user_data);

/* Utility Functions */
static void     update_label_and_image                     (GtkFileChooserButton *button);


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
						     G_PARAM_READWRITE));

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserButtonPrivate));
}


static void
gtk_file_chooser_button_init (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GtkWidget *box, *image, *sep;
  GtkTargetList *target_list;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (button, GTK_TYPE_FILE_CHOOSER_BUTTON,
				      GtkFileChooserButtonPrivate);
  button->priv = priv;

  priv->icon_size = FALLBACK_ICON_SIZE;

  gtk_widget_push_composite_child ();

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

  priv->label = gtk_label_new (_(DEFAULT_FILENAME));
  gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_START);
  gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (box), priv->label);
  gtk_widget_show (priv->label);

  sep = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), sep, FALSE, FALSE, 0);
  gtk_widget_show (sep);

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
				    GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

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

  object = (*G_OBJECT_CLASS (gtk_file_chooser_button_parent_class)->constructor) (type,
										  n_params,
										  params);
  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (object);

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
  g_signal_connect (priv->dialog, "update-preview",
		    G_CALLBACK (dialog_update_preview_cb), object);
  g_signal_connect (priv->dialog, "notify",
		    G_CALLBACK (dialog_notify_cb), object);
  g_object_add_weak_pointer (G_OBJECT (priv->dialog),
			     (gpointer *) (&priv->dialog));

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
    case PROP_WIDTH_CHARS:
      g_value_set_int (value,
		       gtk_label_get_width_chars (GTK_LABEL (priv->label)));
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

  if (priv->dialog != NULL)
    gtk_widget_destroy (priv->dialog);

  if (priv->old_path)
    gtk_file_path_free (priv->old_path);

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
  GtkFileSystem *fs;
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

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (widget);

  fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));

  switch (info)
    {
    case TEXT_URI_LIST:
      {
	gchar **uris;
	GtkFilePath *base_path;
	guint i;
	gboolean selected;

	uris = gtk_selection_data_get_uris (data);
	
	if (uris == NULL)
	  break;

	selected = FALSE;
	for (i = 0; !selected && uris[i] != NULL; i++)
	  {
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
		      (((action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER &&
			 gtk_file_info_get_is_folder (info)) ||
			(action == GTK_FILE_CHOOSER_ACTION_OPEN &&
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
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (widget);

  if (GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show)
    (*GTK_WIDGET_CLASS (gtk_file_chooser_button_parent_class)->show) (widget);

  if (priv->active)
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
  gtk_widget_grab_focus (priv->button);

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

  update_label_and_image (button);
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

  change_icon_theme (button); 
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

  gtk_label_set_width_chars (GTK_LABEL (button->priv->label), n_chars);
  g_object_notify (G_OBJECT (button), "width-chars");
}


/* ******************* *
 *  Utility Functions  *
 * ******************* */

static inline GtkIconTheme *
get_icon_theme (GtkWidget *widget)
{
  if (gtk_widget_has_screen (widget))
    return gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

  return gtk_icon_theme_get_default ();
}

static void
update_label_and_image (GtkFileChooserButton *button)
{
  GtkFileChooserButtonPrivate *priv;
  GdkPixbuf *pixbuf;
  gchar *label_text;
  GSList *paths;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (button);
  paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (button->priv->dialog));
  label_text = NULL;
  pixbuf = NULL;

  if (paths)
    {
      GtkFileSystem *fs;
      GtkFilePath *path, *parent_path;
      GtkFileSystemVolume *volume;
      GtkFileFolder *folder;
    
      path = paths->data;

      fs = _gtk_file_chooser_get_file_system (GTK_FILE_CHOOSER (priv->dialog));

      volume = gtk_file_system_get_volume_for_path (fs, path);
      if (volume)
	{
	  GtkFilePath *base_path;

	  base_path = gtk_file_system_volume_get_base_path (fs, volume);
	  if (base_path && gtk_file_path_compare (base_path, path) == 0)
	    {
	      label_text = gtk_file_system_volume_get_display_name (fs, volume);
	      pixbuf = gtk_file_system_volume_render_icon (fs, volume,
							   GTK_WIDGET (button),
							   priv->icon_size,
							   NULL);
	    }

	  if (base_path)
	    gtk_file_path_free (base_path);

	  gtk_file_system_volume_free (fs, volume);

	  if (label_text)
	    goto out;
	}

      if (!pixbuf)
	pixbuf = gtk_file_system_render_icon (fs, path, GTK_WIDGET (button),
					      priv->icon_size, NULL);

      parent_path = NULL;
      gtk_file_system_get_parent (fs, path, &parent_path, NULL);

      folder = gtk_file_system_get_folder (fs, parent_path ? parent_path : path,
					   GTK_FILE_INFO_DISPLAY_NAME, NULL);
      if (folder)
	{
	  GtkFileInfo *info;

	  info = gtk_file_folder_get_info (folder, path, NULL);
	  g_object_unref (folder);

	  if (info)
	    {
	      label_text = g_strdup (gtk_file_info_get_display_name (info));
	      gtk_file_info_free (info);
	    }
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
  
  if (!pixbuf)
    pixbuf = gtk_icon_theme_load_icon (get_icon_theme (GTK_WIDGET (button)),
				       FALLBACK_ICON_NAME,
				       priv->icon_size, 0, NULL);

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), pixbuf);
  if (pixbuf)
    g_object_unref (pixbuf);
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
  update_label_and_image (user_data);
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
dialog_response_cb (GtkDialog *dialog,
		    gint       response,
		    gpointer   user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

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

  if (priv->old_path)
    {
      gtk_file_path_free (priv->old_path);
      priv->old_path = NULL;
    }

  update_label_and_image (user_data);

  g_signal_handler_unblock (priv->dialog,
			    priv->dialog_folder_changed_id);
  g_signal_handler_unblock (priv->dialog,
			    priv->dialog_file_activated_id);
  g_signal_handler_unblock (priv->dialog,
			    priv->dialog_selection_changed_id);
  priv->active = FALSE;
  gtk_widget_hide (priv->dialog);
}


static void
button_clicked_cb (GtkButton *real_button,
		   gpointer   user_data)
{
  GtkFileChooserButtonPrivate *priv;

  priv = GTK_FILE_CHOOSER_BUTTON_GET_PRIVATE (user_data);

  if (!priv->active)
    {
      GSList *paths;

      /* Setup the dialog parent to be chooser button's toplevel, and be modal
	 as needed. */
      if (!GTK_WIDGET_VISIBLE (priv->dialog))
	{
	  GtkWidget *toplevel;

	  toplevel = gtk_widget_get_toplevel (user_data);

	  if (GTK_WIDGET_TOPLEVEL (toplevel) && GTK_IS_WINDOW (toplevel))
	    {
	      if (GTK_WINDOW (toplevel) != gtk_window_get_transient_for (GTK_WINDOW (priv->dialog)))
		gtk_window_set_transient_for (GTK_WINDOW (priv->dialog),
					      GTK_WINDOW (toplevel));
	      
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
      paths = _gtk_file_chooser_get_paths (GTK_FILE_CHOOSER (priv->dialog));
      if (paths)
	{
	  if (paths->data)
	    priv->old_path = gtk_file_path_copy (paths->data);

	  gtk_file_paths_free (paths);
	}

      priv->active = TRUE;
    }

  gtk_window_present (GTK_WINDOW (priv->dialog));
}
