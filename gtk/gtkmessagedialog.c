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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gtkalias.h"
#include "gtkmessagedialog.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkvbox.h"
#include "gtkimage.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include <string.h>

#define GTK_MESSAGE_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_MESSAGE_DIALOG, GtkMessageDialogPrivate))

typedef struct _GtkMessageDialogPrivate GtkMessageDialogPrivate;

struct _GtkMessageDialogPrivate
{
  GtkWidget *secondary_label;
  gboolean   has_primary_markup;
  gboolean   has_secondary_text;
};

static void gtk_message_dialog_class_init (GtkMessageDialogClass *klass);
static void gtk_message_dialog_init       (GtkMessageDialog      *dialog);
static void gtk_message_dialog_style_set  (GtkWidget             *widget,
                                           GtkStyle              *prev_style);

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

static void gtk_message_dialog_font_size_change (GtkWidget *widget,
						 GtkStyle  *prev_style,
						 gpointer   data);

enum {
  PROP_0,
  PROP_MESSAGE_TYPE,
  PROP_BUTTONS
};

static gpointer parent_class;

GType
gtk_message_dialog_get_type (void)
{
  static GType dialog_type = 0;

  if (!dialog_type)
    {
      static const GTypeInfo dialog_info =
      {
	sizeof (GtkMessageDialogClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_message_dialog_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkMessageDialog),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_message_dialog_init,
      };

      dialog_type = g_type_register_static (GTK_TYPE_DIALOG, "GtkMessageDialog",
					    &dialog_info, 0);
    }

  return dialog_type;
}

static void
gtk_message_dialog_class_init (GtkMessageDialogClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;

  widget_class = GTK_WIDGET_CLASS (class);
  gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  widget_class->style_set = gtk_message_dialog_style_set;

  gobject_class->set_property = gtk_message_dialog_set_property;
  gobject_class->get_property = gtk_message_dialog_get_property;
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("message_border",
                                                             P_("Image/label border"),
                                                             P_("Width of border around the label and image in the message dialog"),
                                                             0,
                                                             G_MAXINT,
                                                             12,
                                                             G_PARAM_READABLE));
  /**
   * GtkMessageDialog::use_separator
   *
   * Whether to draw a separator line between the message label and the buttons
   * in the dialog.
   *
   * Since: 2.4
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("use_separator",
								 P_("Use separator"),
								 P_("Whether to put a separator between the message dialog's text and the buttons"),
								 FALSE,
								 G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_MESSAGE_TYPE,
                                   g_param_spec_enum ("message_type",
						      P_("Message Type"),
						      P_("The type of message"),
						      GTK_TYPE_MESSAGE_TYPE,
                                                      GTK_MESSAGE_INFO,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
                                   PROP_BUTTONS,
                                   g_param_spec_enum ("buttons",
						      P_("Message Buttons"),
						      P_("The buttons shown in the message dialog"),
						      GTK_TYPE_BUTTONS_TYPE,
                                                      GTK_BUTTONS_NONE,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_type_class_add_private (gobject_class,
			    sizeof (GtkMessageDialogPrivate));
}

static void
gtk_message_dialog_init (GtkMessageDialog *dialog)
{
  GtkWidget *hbox, *vbox;
  GtkMessageDialogPrivate *priv;

  priv = GTK_MESSAGE_DIALOG_GET_PRIVATE (dialog);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  priv->has_primary_markup = FALSE;
  priv->has_secondary_text = FALSE;
  priv->secondary_label = gtk_label_new (NULL);
  gtk_widget_set_no_show_all (priv->secondary_label, TRUE);
  
  dialog->label = gtk_label_new (NULL);
  dialog->image = gtk_image_new_from_stock (NULL, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (dialog->image), 0.5, 0.0);
  
  gtk_label_set_line_wrap  (GTK_LABEL (dialog->label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (dialog->label), TRUE);
  gtk_misc_set_alignment   (GTK_MISC  (dialog->label), 0.0, 0.0);
  
  gtk_label_set_line_wrap  (GTK_LABEL (priv->secondary_label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (priv->secondary_label), TRUE);
  gtk_misc_set_alignment   (GTK_MISC  (priv->secondary_label), 0.0, 0.0);

  hbox = gtk_hbox_new (FALSE, 12);
  vbox = gtk_vbox_new (FALSE, 12);

  gtk_box_pack_start (GTK_BOX (vbox), dialog->label,
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), priv->secondary_label,
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), dialog->image,
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), vbox,
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      hbox,
                      FALSE, FALSE, 0);

  gtk_widget_show_all (hbox);

  _gtk_dialog_set_ignore_separator (GTK_DIALOG (dialog), TRUE);

  g_signal_connect (G_OBJECT (dialog), "style-set",
		    G_CALLBACK (gtk_message_dialog_font_size_change), NULL);
}

static GtkMessageType
gtk_message_dialog_get_message_type (GtkMessageDialog *dialog)
{
  const gchar* stock_id = NULL;

  g_return_val_if_fail (GTK_IS_MESSAGE_DIALOG (dialog), GTK_MESSAGE_INFO);
  g_return_val_if_fail (GTK_IS_IMAGE(dialog->image), GTK_MESSAGE_INFO);

  stock_id = GTK_IMAGE(dialog->image)->data.stock.stock_id;

  /* Look at the stock id of the image to guess the
   * GtkMessageType value that was used to choose it
   * in setup_type()
   */
  if (strcmp (stock_id, GTK_STOCK_DIALOG_INFO) == 0)
    return GTK_MESSAGE_INFO;
  else if (strcmp (stock_id, GTK_STOCK_DIALOG_QUESTION) == 0)
    return GTK_MESSAGE_QUESTION;
  else if (strcmp (stock_id, GTK_STOCK_DIALOG_WARNING) == 0)
    return GTK_MESSAGE_WARNING;
  else if (strcmp (stock_id, GTK_STOCK_DIALOG_ERROR) == 0)
    return GTK_MESSAGE_ERROR;
  else
    {
      g_assert_not_reached (); 
      return GTK_MESSAGE_INFO;
    }
}

static void
setup_primary_label_font (GtkMessageDialog *dialog)
{
  gint size;
  PangoFontDescription *font_desc;
  GtkMessageDialogPrivate *priv;

  priv = GTK_MESSAGE_DIALOG_GET_PRIVATE (dialog);

  if (priv->has_primary_markup)
    return;

  /* unset the font settings */
  gtk_widget_modify_font (dialog->label, NULL);

  if (priv->has_secondary_text)
    {
      size = pango_font_description_get_size (dialog->label->style->font_desc);
      font_desc = pango_font_description_new ();
      pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);
      pango_font_description_set_size (font_desc, size * PANGO_SCALE_LARGE);
      gtk_widget_modify_font (dialog->label, font_desc);
      pango_font_description_free (font_desc);
    }
}

static void
setup_type (GtkMessageDialog *dialog,
	    GtkMessageType    type)
{
  const gchar *stock_id = NULL;
  GtkStockItem item;
  
  switch (type)
    {
    case GTK_MESSAGE_INFO:
      stock_id = GTK_STOCK_DIALOG_INFO;
      break;

    case GTK_MESSAGE_QUESTION:
      stock_id = GTK_STOCK_DIALOG_QUESTION;
      break;

    case GTK_MESSAGE_WARNING:
      stock_id = GTK_STOCK_DIALOG_WARNING;
      break;
      
    case GTK_MESSAGE_ERROR:
      stock_id = GTK_STOCK_DIALOG_ERROR;
      break;

    default:
      g_warning ("Unknown GtkMessageType %d", type);
      break;
    }

  if (stock_id == NULL)
    stock_id = GTK_STOCK_DIALOG_INFO;

  if (gtk_stock_lookup (stock_id, &item))
    {
      gtk_image_set_from_stock (GTK_IMAGE (dialog->image), stock_id,
                                GTK_ICON_SIZE_DIALOG);
      
      gtk_window_set_title (GTK_WINDOW (dialog), item.label);
    }
  else
    g_warning ("Stock dialog ID doesn't exist?");  
}

static void 
gtk_message_dialog_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  GtkMessageDialog *dialog;
  
  dialog = GTK_MESSAGE_DIALOG (object);
  
  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      setup_type (dialog, g_value_get_enum (value));
      break;
    case PROP_BUTTONS:
      gtk_message_dialog_add_buttons (dialog, g_value_get_enum (value));
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
  GtkMessageDialog *dialog;
  
  dialog = GTK_MESSAGE_DIALOG (object);
  
  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      g_value_set_enum (value, gtk_message_dialog_get_message_type (dialog));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_message_dialog_font_size_change (GtkWidget *widget,
				     GtkStyle  *prev_style,
				     gpointer   data)
{
  setup_primary_label_font (GTK_MESSAGE_DIALOG (widget));
}


/**
 * gtk_message_dialog_new:
 * @parent: transient parent, or NULL for none 
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: printf()-style format string, or NULL
 * @Varargs: arguments for @message_format
 * 
 * Creates a new message dialog, which is a simple dialog with an icon
 * indicating the dialog type (error, warning, etc.) and some text the
 * user may want to see. When the user clicks a button a "response"
 * signal is emitted with response IDs from #GtkResponseType. See
 * #GtkDialog for more details.
 * 
 * Return value: a new #GtkMessageDialog
 **/
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
			 "message_type", type,
			 "buttons", buttons,
			 NULL);
  dialog = GTK_DIALOG (widget);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    {
      g_warning ("The GTK_DIALOG_NO_SEPARATOR flag cannot be used for GtkMessageDialog");
      flags &= ~GTK_DIALOG_NO_SEPARATOR;
    }

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);

      gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (widget)->label),
                          msg);

      g_free (msg);
    }

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (widget),
                                  GTK_WINDOW (parent));
  
  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    gtk_dialog_set_has_separator (dialog, FALSE);

  return widget;
}

/**
 * gtk_message_dialog_new_with_markup:
 * @parent: transient parent, or %NULL for none 
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: printf()-style format string, or %NULL
 * @Varargs: arguments for @message_format
 * 
 * Creates a new message dialog, which is a simple dialog with an icon
 * indicating the dialog type (error, warning, etc.) and some text which
 * is marked up with the <link linkend="PangoMarkupFormat">Pango text markup language</link>.
 * When the user clicks a button a "response" signal is emitted with
 * response IDs from #GtkResponseType. See #GtkDialog for more details.
 *
 * Special XML characters in the printf() arguments passed to this
 * function will automatically be escaped as necessary.
 * (See g_markup_printf_escaped() for how this is implemented.)
 * Usually this is what you want, but if you have an existing
 * Pango markup string that you want to use literally as the
 * label, then you need to use gtk_message_dialog_set_markup()
 * instead, since you can't pass the markup string either
 * as the format (it might contain '%' characters) or as a string
 * argument.
 *
 * <informalexample><programlisting>
 *  GtkWidget *dialog;
 *  dialog = gtk_message_dialog_new (main_application_window,
 *                                   GTK_DIALOG_DESTROY_WITH_PARENT,
 *                                   GTK_MESSAGE_ERROR,
 *                                   GTK_BUTTON_CLOSE,
 *                                   NULL);
 *  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
 *                                 markup);
 * </programlisting></informalexample>
 * 
 * Return value: a new #GtkMessageDialog
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
 * gtk_message_dialog_set_markup:
 * @message_dialog: a #GtkMessageDialog
 * @str: markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Sets the text of the message dialog to be @str, which is marked
 * up with the <link linkend="PangoMarkupFormat">Pango text markup
 * language</link>.
 *
 * Since: 2.4
 **/
void
gtk_message_dialog_set_markup (GtkMessageDialog *message_dialog,
                               const gchar      *str)
{
  GtkMessageDialogPrivate *priv;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv = GTK_MESSAGE_DIALOG_GET_PRIVATE (message_dialog);
  priv->has_primary_markup = TRUE;
  gtk_label_set_markup (GTK_LABEL (message_dialog->label), str);
}

/**
 * gtk_message_dialog_format_secondary_text:
 * @message_dialog: a #GtkMessageDialog
 * @message_format: printf()-style format string, or %NULL
 * @Varargs: arguments for @message_format
 * 
 * Sets the secondary text of the message dialog to be @message_format 
 * (with printf()-style).
 *
 * Note that setting a secondary text makes the primary text become
 * bold, unless you have provided explicit markup.
 *
 * Since: 2.6
 **/
void
gtk_message_dialog_format_secondary_text (GtkMessageDialog *message_dialog,
                                          const gchar      *message_format,
                                          ...)
{
  va_list args;
  gchar *msg = NULL;
  GtkMessageDialogPrivate *priv;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv = GTK_MESSAGE_DIALOG_GET_PRIVATE (message_dialog);

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
     <link linkend="PangoMarkupFormat">Pango markup format</link>), or %NULL
 * @Varargs: arguments for @message_format
 * 
 * Sets the secondary text of the message dialog to be @message_format (with 
 * printf()-style), which is marked up with the 
 * <link linkend="PangoMarkupFormat">Pango text markup language</link>.
 *
 * Note that setting a secondary text makes the primary text become
 * bold, unless you have provided explicit markup.
 *
 * Since: 2.6
 **/
void
gtk_message_dialog_format_secondary_markup (GtkMessageDialog *message_dialog,
                                            const gchar      *message_format,
                                            ...)
{
  va_list args;
  gchar *msg = NULL;
  GtkMessageDialogPrivate *priv;

  g_return_if_fail (GTK_IS_MESSAGE_DIALOG (message_dialog));

  priv = GTK_MESSAGE_DIALOG_GET_PRIVATE (message_dialog);

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

static void
gtk_message_dialog_add_buttons (GtkMessageDialog* message_dialog,
				GtkButtonsType buttons)
{
  GtkDialog* dialog = GTK_DIALOG (message_dialog);

  switch (buttons)
    {
    case GTK_BUTTONS_NONE:
      /* nothing */
      break;

    case GTK_BUTTONS_OK:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_OK,
                             GTK_RESPONSE_OK);
      break;

    case GTK_BUTTONS_CLOSE:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_CLOSE,
                             GTK_RESPONSE_CLOSE);
      break;

    case GTK_BUTTONS_CANCEL:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_CANCEL,
                             GTK_RESPONSE_CANCEL);
      break;

    case GTK_BUTTONS_YES_NO:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_NO,
                             GTK_RESPONSE_NO);
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_YES,
                             GTK_RESPONSE_YES);
      gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
					       GTK_RESPONSE_YES,
					       GTK_RESPONSE_NO,
					       -1);
      break;

    case GTK_BUTTONS_OK_CANCEL:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_CANCEL,
                             GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_OK,
                             GTK_RESPONSE_OK);
      gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
					       GTK_RESPONSE_OK,
					       GTK_RESPONSE_CANCEL,
					       -1);
      break;
      
    default:
      g_warning ("Unknown GtkButtonsType");
      break;
    } 

  g_object_notify (G_OBJECT (message_dialog), "buttons");
}

static void
gtk_message_dialog_style_set (GtkWidget *widget,
                              GtkStyle  *prev_style)
{
  GtkWidget *parent;
  gint border_width = 0;
  gboolean use_separator;

  parent = GTK_WIDGET (GTK_MESSAGE_DIALOG (widget)->image->parent);

  if (parent)
    {
      gtk_widget_style_get (widget, "message_border",
                            &border_width, NULL);
      
      gtk_container_set_border_width (GTK_CONTAINER (parent),
                                      border_width);
    }

  gtk_widget_style_get (widget,
			"use_separator", &use_separator,
			NULL);
  _gtk_dialog_set_ignore_separator (GTK_DIALOG (widget), FALSE);
  gtk_dialog_set_has_separator (GTK_DIALOG (widget), use_separator);
  _gtk_dialog_set_ignore_separator (GTK_DIALOG (widget), TRUE);

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    (GTK_WIDGET_CLASS (parent_class)->style_set) (widget, prev_style);
}
