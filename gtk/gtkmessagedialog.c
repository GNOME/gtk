/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <string.h>

#include "gtkmessagedialog.h"
#include "gtkaccessible.h"
#include "gtkbuildable.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkbbox.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtkmessagedialog
 * @Short_description: A convenient message window
 * @Title: GtkMessageDialog
 * @See_also:#GtkDialog
 *
 * #GtkMessageDialog presents a dialog with some message text. It’s simply a
 * convenience widget; you could construct the equivalent of #GtkMessageDialog
 * from #GtkDialog without too much effort, but #GtkMessageDialog saves typing.
 *
 * One difference from #GtkDialog is that #GtkMessageDialog sets the
 * #GtkWindow:skip-taskbar-hint property to %TRUE, so that the dialog is hidden
 * from the taskbar by default.
 *
 * The easiest way to do a modal message dialog is to use gtk_dialog_run(), though
 * you can also pass in the %GTK_DIALOG_MODAL flag, gtk_dialog_run() automatically
 * makes the dialog modal and waits for the user to respond to it. gtk_dialog_run()
 * returns when any dialog button is clicked.
 *
 * An example for using a modal dialog:
 * |[<!-- language="C" -->
 *  GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 *  dialog = gtk_message_dialog_new (parent_window,
 *                                   flags,
 *                                   GTK_MESSAGE_ERROR,
 *                                   GTK_BUTTONS_CLOSE,
 *                                   "Error reading “%s”: %s",
 *                                   filename,
 *                                   g_strerror (errno));
 *  gtk_dialog_run (GTK_DIALOG (dialog));
 *  gtk_widget_destroy (dialog);
 * ]|
 *
 * You might do a non-modal #GtkMessageDialog as follows:
 *
 * An example for a non-modal dialog:
 * |[<!-- language="C" -->
 *  GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 *  dialog = gtk_message_dialog_new (parent_window,
 *                                   flags,
 *                                   GTK_MESSAGE_ERROR,
 *                                   GTK_BUTTONS_CLOSE,
 *                                   "Error reading “%s”: %s",
 *                                   filename,
 *                                   g_strerror (errno));
 *
 *  // Destroy the dialog when the user responds to it
 *  // (e.g. clicks a button)
 *
 *  g_signal_connect_swapped (dialog, "response",
 *                            G_CALLBACK (gtk_widget_destroy),
 *                            dialog);
 * ]|
 *
 * # GtkMessageDialog as GtkBuildable
 *
 * The GtkMessageDialog implementation of the GtkBuildable interface exposes
 * the message area as an internal child with the name “message_area”.
 */

struct _GtkMessageDialogPrivate
{
  GtkWidget     *image;
  GtkWidget     *label;
  GtkWidget     *message_area; /* vbox for the primary and secondary labels, and any extra content from the caller */
  GtkWidget     *secondary_label;

  guint          has_primary_markup : 1;
  guint          has_secondary_text : 1;
  guint          message_type       : 3;
};

static void gtk_message_dialog_style_updated (GtkWidget       *widget);

static void gtk_message_dialog_constructed  (GObject          *object);
static void gtk_message_dialog_set_property (GObject          *object,
					     guint             prop_id,
					     const GValue     *value,
					     GParamSpec       *pspec);
static void gtk_message_dialog_get_property (GObject          *object,
					     guint             prop_id,
					     GValue           *value,
					     GParamSpec       *pspec);
static void gtk_message_dialog_add_buttons  (GtkMessageDialog *message_dialog,
					     GtkButtonsType    buttons);
static void      gtk_message_dialog_buildable_interface_init     (GtkBuildableIface *iface);

enum {
  PROP_0,
  PROP_MESSAGE_TYPE,
  PROP_BUTTONS,
  PROP_TEXT,
  PROP_USE_MARKUP,
  PROP_SECONDARY_TEXT,
  PROP_SECONDARY_USE_MARKUP,
  PROP_IMAGE,
  PROP_MESSAGE_AREA
};

G_DEFINE_TYPE_WITH_CODE (GtkMessageDialog, gtk_message_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (GtkMessageDialog)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_message_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_message_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = parent_buildable_iface->custom_tag_start;
  iface->custom_finished = parent_buildable_iface->custom_finished;
}

static void
gtk_message_dialog_class_init (GtkMessageDialogClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;

  widget_class = GTK_WIDGET_CLASS (class);
  gobject_class = G_OBJECT_CLASS (class);
  
  widget_class->style_updated = gtk_message_dialog_style_updated;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_ALERT);

  gobject_class->constructed = gtk_message_dialog_constructed;
  gobject_class->set_property = gtk_message_dialog_set_property;
  gobject_class->get_property = gtk_message_dialog_get_property;
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("message-border",
                                                             P_("label border"),
                                                             P_("Width of border around the label in the message dialog"),
                                                             0,
                                                             G_MAXINT,
                                                             12,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkMessageDialog:message-type:
   *
   * The type of the message.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MESSAGE_TYPE,
                                   g_param_spec_enum ("message-type",
						      P_("Message Type"),
						      P_("The type of message"),
						      GTK_TYPE_MESSAGE_TYPE,
                                                      GTK_MESSAGE_INFO,
                                                      GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_BUTTONS,
                                   g_param_spec_enum ("buttons",
						      P_("Message Buttons"),
						      P_("The buttons shown in the message dialog"),
						      GTK_TYPE_BUTTONS_TYPE,
                                                      GTK_BUTTONS_NONE,
                                                      GTK_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkMessageDialog:text:
   * 
   * The primary text of the message dialog. If the dialog has 
   * a secondary text, this will appear as the title.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        P_("Text"),
                                                        P_("The primary text of the message dialog"),
                                                        "",
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMessageDialog:use-markup:
   * 
   * %TRUE if the primary text of the dialog includes Pango markup. 
   * See pango_parse_markup(). 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_USE_MARKUP,
				   g_param_spec_boolean ("use-markup",
							 P_("Use Markup"),
							 P_("The primary text of the title includes Pango markup."),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  /**
   * GtkMessageDialog:secondary-text:
   * 
   * The secondary text of the message dialog. 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SECONDARY_TEXT,
                                   g_param_spec_string ("secondary-text",
                                                        P_("Secondary Text"),
                                                        P_("The secondary text of the message dialog"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkMessageDialog:secondary-use-markup:
   * 
   * %TRUE if the secondary text of the dialog includes Pango markup. 
   * See pango_parse_markup(). 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_SECONDARY_USE_MARKUP,
				   g_param_spec_boolean ("secondary-use-markup",
							 P_("Use Markup in secondary"),
							 P_("The secondary text includes Pango markup."),
							 FALSE,
							 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkMessageDialog:image:
   *
   * The image for this dialog.
   *
   * Since: 2.10
   * Deprecated: 3.12: Use #GtkDialog to create dialogs with images
   */
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        P_("Image"),
                                                        P_("The image"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));

  /**
   * GtkMessageDialog:message-area:
   *
   * The #GtkVBox that corresponds to the message area of this dialog.  See
   * gtk_message_dialog_get_message_area() for a detailed description of this
   * area.
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class,
				   PROP_MESSAGE_AREA,
				   g_param_spec_object ("message-area",
							P_("Message area"),
							P_("GtkVBox that holds the dialog's primary and secondary labels"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READABLE));

  /* Setup Composite data */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkmessagedialog.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageDialog, label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageDialog, secondary_label);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkMessageDialog, message_area);
}

static void
gtk_message_dialog_init (GtkMessageDialog *dialog)
{
  GtkMessageDialogPrivate *priv;
  GtkWidget *action_area;

  dialog->priv = gtk_message_dialog_get_instance_private (dialog);
  priv = dialog->priv;

  priv->has_primary_markup = FALSE;
  priv->has_secondary_text = FALSE;
  priv->has_primary_markup = FALSE;
  priv->has_secondary_text = FALSE;
  priv->message_type = GTK_MESSAGE_OTHER;

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_message_dialog_style_updated (GTK_WIDGET (dialog));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_button_box_set_layout (GTK_BUTTON_BOX (action_area), GTK_BUTTONBOX_EXPAND);
}

static void
setup_primary_label_font (GtkMessageDialog *dialog)
{
  GtkMessageDialogPrivate *priv = dialog->priv;

  if (!priv->has_primary_markup)
    {
      PangoAttrList *attributes;
      PangoAttribute *attr;

      attributes = pango_attr_list_new ();

      attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
      pango_attr_list_insert (attributes, attr);

      if (priv->has_secondary_text)
        {
          attr = pango_attr_scale_new (PANGO_SCALE_LARGE);
          pango_attr_list_insert (attributes, attr);
        }

      gtk_label_set_attributes (GTK_LABEL (priv->label), attributes);
      pango_attr_list_unref (attributes);
    }
  else
    {
      /* unset the font settings */
      gtk_label_set_attributes (GTK_LABEL (priv->label), NULL);
    }
}

static void
setup_type (GtkMessageDialog *dialog,
	    GtkMessageType    type)
{
  GtkMessageDialogPrivate *priv = dialog->priv;
  const gchar *name = NULL;
  AtkObject *atk_obj;

  if (priv->message_type == type)
    return;

  priv->message_type = type;

  switch (type)
    {
    case GTK_MESSAGE_INFO:
      name = _("Information");
      break;

    case GTK_MESSAGE_QUESTION:
      name = _("Question");
      break;

    case GTK_MESSAGE_WARNING:
      name = _("Warning");
      break;

    case GTK_MESSAGE_ERROR:
      name = _("Error");
      break;

    case GTK_MESSAGE_OTHER:
      break;

    default:
      g_warning ("Unknown GtkMessageType %u", type);
      break;
    }

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (dialog));
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      atk_object_set_role (atk_obj, ATK_ROLE_ALERT);
      if (name)
        atk_object_set_name (atk_obj, name);
    }

  g_object_notify (G_OBJECT (dialog), "message-type");
}

static void
update_title (GObject    *dialog,
              GParamSpec *pspec,
              GtkWidget  *label)
{
  const gchar *title;

  title = gtk_window_get_title (GTK_WINDOW (dialog));
  gtk_label_set_label (GTK_LABEL (label), title);
  gtk_widget_set_visible (label, title && title[0]);
}

static void
gtk_message_dialog_constructed (GObject *object)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  gboolean use_header;

  G_OBJECT_CLASS (gtk_message_dialog_parent_class)->constructed (object);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (dialog)),
                "gtk-dialogs-use-header", &use_header,
                NULL);

  if (use_header)
    {
      GtkWidget *box;
      GtkWidget *label;

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_show (box);
      gtk_widget_set_size_request (box, -1, 16);
      label = gtk_label_new ("");
      gtk_widget_set_margin_top (label, 6);
      gtk_widget_set_margin_bottom (label, 6);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "title");
      gtk_box_set_center_widget (GTK_BOX (box), label);
      g_signal_connect_object (dialog, "notify::title", G_CALLBACK (update_title), label, 0);

      gtk_window_set_titlebar (GTK_WINDOW (dialog), box);
    }
}

static void 
gtk_message_dialog_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  GtkMessageDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      setup_type (dialog, g_value_get_enum (value));
      break;
    case PROP_BUTTONS:
      gtk_message_dialog_add_buttons (dialog, g_value_get_enum (value));
      break;
    case PROP_TEXT:
      if (priv->has_primary_markup)
	gtk_label_set_markup (GTK_LABEL (priv->label),
			      g_value_get_string (value));
      else
	gtk_label_set_text (GTK_LABEL (priv->label),
			    g_value_get_string (value));
      break;
    case PROP_USE_MARKUP:
      if (priv->has_primary_markup != g_value_get_boolean (value))
        {
          priv->has_primary_markup = g_value_get_boolean (value);
          gtk_label_set_use_markup (GTK_LABEL (priv->label), priv->has_primary_markup);
          g_object_notify_by_pspec (object, pspec);
        }
        setup_primary_label_font (dialog);
      break;
    case PROP_SECONDARY_TEXT:
      {
	const gchar *txt = g_value_get_string (value);

	if (gtk_label_get_use_markup (GTK_LABEL (priv->secondary_label)))
	  gtk_label_set_markup (GTK_LABEL (priv->secondary_label), txt);
	else
	  gtk_label_set_text (GTK_LABEL (priv->secondary_label), txt);

	if (txt)
	  {
	    priv->has_secondary_text = TRUE;
	    gtk_widget_show (priv->secondary_label);
	  }
	else
	  {
	    priv->has_secondary_text = FALSE;
	    gtk_widget_hide (priv->secondary_label);
	  }
	setup_primary_label_font (dialog);
      }
      break;
    case PROP_SECONDARY_USE_MARKUP:
      if (gtk_label_get_use_markup (GTK_LABEL (priv->secondary_label)) != g_value_get_boolean (value))
        {
          gtk_label_set_use_markup (GTK_LABEL (priv->secondary_label), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_IMAGE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_message_dialog_set_image (dialog, g_value_get_object (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_message_dialog_get_property (GObject     *object,
				 guint        prop_id,
				 GValue      *value,
				 GParamSpec  *pspec)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  GtkMessageDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      g_value_set_enum (value, (GtkMessageType) priv->message_type);
      break;
    case PROP_TEXT:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->label)));
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, priv->has_primary_markup);
      break;
    case PROP_SECONDARY_TEXT:
      if (priv->has_secondary_text)
      g_value_set_string (value, 
			  gtk_label_get_label (GTK_LABEL (priv->secondary_label)));
      else
	g_value_set_string (value, NULL);
      break;
    case PROP_SECONDARY_USE_MARKUP:
      if (priv->has_secondary_text)
	g_value_set_boolean (value, 
			     gtk_label_get_use_markup (GTK_LABEL (priv->secondary_label)));
      else
	g_value_set_boolean (value, FALSE);
      break;
    case PROP_IMAGE:
      g_value_set_object (value, priv->image);
      break;
    case PROP_MESSAGE_AREA:
      g_value_set_object (value, priv->message_area);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_message_dialog_new:
 * @parent: (allow-none): transient parent, or %NULL for none
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: (allow-none): printf()-style format string, or %NULL
 * @...: arguments for @message_format
 *
 * Creates a new message dialog, which is a simple dialog with an icon
 * indicating the dialog type (error, warning, etc.) and some text the
 * user may want to see. When the user clicks a button a “response”
 * signal is emitted with response IDs from #GtkResponseType. See
 * #GtkDialog for more details.
 *
 * Returns: (transfer none): a new #GtkMessageDialog
 */
GtkWidget*
gtk_message_dialog_new (GtkWindow     *parent,
                        GtkDialogFlags flags,
                        GtkMessageType type,
                        GtkButtonsType buttons,
                        const gchar   *message_format,
                        ...)
{
  GtkWidget *widget;
  GtkDialog *dialog;
  gchar* msg = NULL;
  va_list args;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  widget = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "use-header-bar", FALSE,
			 "message-type", type,
			 "buttons", buttons,
			 NULL);
  dialog = GTK_DIALOG (widget);

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (widget)->priv->label), msg);

      g_free (msg);
    }

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (parent));
  
  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  return widget;
}

/**
 * gtk_message_dialog_new_with_markup:
 * @parent: (allow-none): transient parent, or %NULL for none
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: (allow-none): printf()-style format string, or %NULL
 * @...: arguments for @message_format
 *
 * Creates a new message dialog, which is a simple dialog with an icon
 * indicating the dialog type (error, warning, etc.) and some text which
 * is marked up with the [Pango text markup language][PangoMarkupFormat].
 * When the user clicks a button a “response” signal is emitted with
 * response IDs from #GtkResponseType. See #GtkDialog for more details.
 *
 * Special XML characters in the printf() arguments passed to this
 * function will automatically be escaped as necessary.
 * (See g_markup_printf_escaped() for how this is implemented.)
 * Usually this is what you want, but if you have an existing
 * Pango markup string that you want to use literally as the
 * label, then you need to use gtk_message_dialog_set_markup()
 * instead, since you can’t pass the markup string either
 * as the format (it might contain “%” characters) or as a string
 * argument.
 * |[<!-- language="C" -->
 *  GtkWidget *dialog;
 *  GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 *  dialog = gtk_message_dialog_new (parent_window,
 *                                   flags,
 *                                   GTK_MESSAGE_ERROR,
 *                                   GTK_BUTTONS_CLOSE,
 *                                   NULL);
 *  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
 *                                 markup);
 * ]|
 * 
 * Returns: a new #GtkMessageDialog
 *
 * Since: 2.4
 **/
GtkWidget*
gtk_message_dialog_new_with_markup (GtkWindow     *parent,
                                    GtkDialogFlags flags,
                                    GtkMessageType type,
                                    GtkButtonsType buttons,
                                    const gchar   *message_format,
                                    ...)
{
  GtkWidget *widget;
  va_list args;
  gchar *msg = NULL;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

  widget = gtk_message_dialog_new (parent, flags, type, buttons, NULL);

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_markup_vprintf_escaped (message_format, args);
      va_end (args);

      gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (widget), msg);

      g_free (msg);
    }

  return widget;
}

/**
 * gtk_message_dialog_set_image:
 * @dialog: a #GtkMessageDialog
 * @image: the image
 * 
 * Sets the dialog’s image to @image.
 *
 * Since: 2.10
 * Deprecated: 3.12: Use #GtkDialog to create dialogs with images
 **/
void
gtk_message_dialog_set_image (GtkMessageDialog *dialog,
			      GtkWidget        *image)
{
  GtkMessageDialogPrivate *priv;
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (dialog));
  g_return_if_fail (image == NULL || GTK_IS_WIDGET (image));

  priv = dialog->priv;
  
  if (priv->image)
    gtk_widget_destroy (priv->image);

  priv->image = image;
 
  if (priv->image)
    { 
      gtk_widget_set_halign (priv->image, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (priv->image, GTK_ALIGN_START);
      parent = gtk_widget_get_parent (priv->message_area);
      gtk_container_add (GTK_CONTAINER (parent), priv->image);
      gtk_box_reorder_child (GTK_BOX (parent), priv->image, 0);
    }

  priv->message_type = GTK_MESSAGE_OTHER;

  g_object_notify (G_OBJECT (dialog), "image");
  g_object_notify (G_OBJECT (dialog), "message-type");
}

/**
 * gtk_message_dialog_get_image:
 * @dialog: a #GtkMessageDialog
 *
 * Gets the dialog’s image.
 *
 * Returns: (transfer none): the dialog’s image
 *
 * Since: 2.14
 * Deprecated: 3.12: Use #GtkDialog for dialogs with images
 **/
GtkWidget *
gtk_message_dialog_get_image (GtkMessageDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_MESSAGE_DIALOG (dialog), NULL);

  return dialog->priv->image;
}

/**
 * gtk_message_dialog_set_markup:
 * @message_dialog: a #GtkMessageDialog
 * @str: markup string (see [Pango markup format][PangoMarkupFormat])
 * 
 * Sets the text of the message dialog to be @str, which is marked
 * up with the [Pango text markup language][PangoMarkupFormat].
 *
 * Since: 2.4
 **/
void
gtk_message_dialog_set_markup (GtkMessageDialog *message_dialog,
                               const gchar      *str)
{
  GtkMessageDialogPrivate *priv;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv = message_dialog->priv;

  priv->has_primary_markup = TRUE;
  gtk_label_set_markup (GTK_LABEL (priv->label), str);
}

/**
 * gtk_message_dialog_format_secondary_text:
 * @message_dialog: a #GtkMessageDialog
 * @message_format: (allow-none): printf()-style format string, or %NULL
 * @...: arguments for @message_format
 *
 * Sets the secondary text of the message dialog to be @message_format
 * (with printf()-style).
 *
 * Since: 2.6
 */
void
gtk_message_dialog_format_secondary_text (GtkMessageDialog *message_dialog,
                                          const gchar      *message_format,
                                          ...)
{
  va_list args;
  gchar *msg = NULL;
  GtkMessageDialogPrivate *priv;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv = message_dialog->priv;

  if (message_format)
    {
      priv->has_secondary_text = TRUE;

      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_widget_show (priv->secondary_label);
      gtk_label_set_text (GTK_LABEL (priv->secondary_label), msg);

      g_free (msg);
    }
  else
    {
      priv->has_secondary_text = FALSE;
      gtk_widget_hide (priv->secondary_label);
    }

  setup_primary_label_font (message_dialog);
}

/**
 * gtk_message_dialog_format_secondary_markup:
 * @message_dialog: a #GtkMessageDialog
 * @message_format: printf()-style markup string (see
     [Pango markup format][PangoMarkupFormat]), or %NULL
 * @...: arguments for @message_format
 *
 * Sets the secondary text of the message dialog to be @message_format (with
 * printf()-style), which is marked up with the
 * [Pango text markup language][PangoMarkupFormat].
 *
 * Due to an oversight, this function does not escape special XML characters
 * like gtk_message_dialog_new_with_markup() does. Thus, if the arguments
 * may contain special XML characters, you should use g_markup_printf_escaped()
 * to escape it.

 * |[<!-- language="C" -->
 * gchar *msg;
 *
 * msg = g_markup_printf_escaped (message_format, ...);
 * gtk_message_dialog_format_secondary_markup (message_dialog,
 *                                             "%s", msg);
 * g_free (msg);
 * ]|
 *
 * Since: 2.6
 */
void
gtk_message_dialog_format_secondary_markup (GtkMessageDialog *message_dialog,
                                            const gchar      *message_format,
                                            ...)
{
  va_list args;
  gchar *msg = NULL;
  GtkMessageDialogPrivate *priv;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv = message_dialog->priv;

  if (message_format)
    {
      priv->has_secondary_text = TRUE;

      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_widget_show (priv->secondary_label);
      gtk_label_set_markup (GTK_LABEL (priv->secondary_label), msg);

      g_free (msg);
    }
  else
    {
      priv->has_secondary_text = FALSE;
      gtk_widget_hide (priv->secondary_label);
    }

  setup_primary_label_font (message_dialog);
}

/**
 * gtk_message_dialog_get_message_area:
 * @message_dialog: a #GtkMessageDialog
 *
 * Returns the message area of the dialog. This is the box where the
 * dialog’s primary and secondary labels are packed. You can add your
 * own extra content to that box and it will appear below those labels.
 * See gtk_dialog_get_content_area() for the corresponding
 * function in the parent #GtkDialog.
 *
 * Returns: (transfer none): A #GtkVBox corresponding to the
 *     “message area” in the @message_dialog.
 *
 * Since: 2.22
 **/
GtkWidget *
gtk_message_dialog_get_message_area (GtkMessageDialog *message_dialog)
{
  g_return_val_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog), NULL);

  return message_dialog->priv->message_area;
}

static void
gtk_message_dialog_add_buttons (GtkMessageDialog* message_dialog,
				GtkButtonsType buttons)
{
  GtkDialog* dialog = GTK_DIALOG (message_dialog);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  switch (buttons)
    {
    case GTK_BUTTONS_NONE:
      /* nothing */
      break;

    case GTK_BUTTONS_OK:
      gtk_dialog_add_button (dialog, _("_OK"), GTK_RESPONSE_OK);
      break;

    case GTK_BUTTONS_CLOSE:
      gtk_dialog_add_button (dialog, _("_Close"), GTK_RESPONSE_CLOSE);
      break;

    case GTK_BUTTONS_CANCEL:
      gtk_dialog_add_button (dialog, _("_Cancel"), GTK_RESPONSE_CANCEL);
      break;

    case GTK_BUTTONS_YES_NO:
      gtk_dialog_add_button (dialog, _("_No"), GTK_RESPONSE_NO);
      gtk_dialog_add_button (dialog, _("_Yes"), GTK_RESPONSE_YES);
      gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
					       GTK_RESPONSE_YES,
					       GTK_RESPONSE_NO,
					       -1);
      break;

    case GTK_BUTTONS_OK_CANCEL:
      gtk_dialog_add_button (dialog, _("_Cancel"), GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (dialog, _("_OK"), GTK_RESPONSE_OK);
      gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
					       GTK_RESPONSE_OK,
					       GTK_RESPONSE_CANCEL,
					       -1);
      break;
      
    default:
      g_warning ("Unknown GtkButtonsType");
      break;
    } 
G_GNUC_END_IGNORE_DEPRECATIONS

  g_object_notify (G_OBJECT (message_dialog), "buttons");
}

static void
gtk_message_dialog_style_updated (GtkWidget *widget)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (widget);
  GtkWidget *parent;
  gint border_width;

  parent = gtk_widget_get_parent (dialog->priv->message_area);

  if (parent)
    {
      gtk_widget_style_get (widget, "message-border",
                            &border_width, NULL);

      gtk_container_set_border_width (GTK_CONTAINER (parent),
                                      MAX (0, border_width - 7));
    }

  GTK_WIDGET_CLASS (gtk_message_dialog_parent_class)->style_updated (widget);
}
