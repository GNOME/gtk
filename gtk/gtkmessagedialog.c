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
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkmessagedialog.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkimage.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"

static void gtk_message_dialog_class_init (GtkMessageDialogClass *klass);
static void gtk_message_dialog_init       (GtkMessageDialog      *dialog);


GtkType
gtk_message_dialog_get_type (void)
{
  static GtkType dialog_type = 0;

  if (!dialog_type)
    {
      static const GtkTypeInfo dialog_info =
      {
	"GtkMessageDialog",
	sizeof (GtkMessageDialog),
	sizeof (GtkMessageDialogClass),
	(GtkClassInitFunc) gtk_message_dialog_class_init,
	(GtkObjectInitFunc) gtk_message_dialog_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      dialog_type = gtk_type_unique (GTK_TYPE_DIALOG, &dialog_info);
    }

  return dialog_type;
}

static void
gtk_message_dialog_class_init (GtkMessageDialogClass *class)
{
}

static void
gtk_message_dialog_init (GtkMessageDialog *dialog)
{
  GtkWidget *hbox;
  
  dialog->label = gtk_label_new (NULL);
  dialog->image = gtk_image_new_from_stock (NULL, GTK_ICON_SIZE_DIALOG);
  
  gtk_label_set_line_wrap (GTK_LABEL (dialog->label), TRUE);

  hbox = gtk_hbox_new (FALSE, 10);

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
  
  gtk_box_pack_start (GTK_BOX (hbox), dialog->image,
                      FALSE, FALSE, 2);

  gtk_box_pack_start (GTK_BOX (hbox), dialog->label,
                      TRUE, TRUE, 2);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      hbox,
                      FALSE, FALSE, 10);

  gtk_widget_show_all (hbox);
}

static void
setup_type(GtkMessageDialog *dialog, GtkMessageType type)
{
  /* Note: this function can be called more than once,
   * and after showing the dialog, due to object args
   */
  
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
 * user may want to see. If the button set you select with the @buttons
 * argument has positive buttons (OK, Yes) they will result in a response ID
 * of GTK_RESPONSE_ACCEPT. If it has negative buttons (Cancel, No) they will
 * result in a response ID of GTK_RESPONSE_REJECT. See #GtkDialog for more
 * details.
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
  gchar* msg;
  va_list args;
  
  widget = GTK_WIDGET (gtk_type_new (GTK_TYPE_MESSAGE_DIALOG));
  dialog = GTK_DIALOG (widget);

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_strdup_vprintf(message_format, args);
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
  
  setup_type (GTK_MESSAGE_DIALOG (dialog), type);
  
  switch (buttons)
    {
    case GTK_BUTTONS_NONE:
      /* nothing */
      break;

    case GTK_BUTTONS_OK:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_OK,
                             GTK_RESPONSE_ACCEPT);
      break;

    case GTK_BUTTONS_CLOSE:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_CLOSE,
                             GTK_RESPONSE_ACCEPT);
      break;

    case GTK_BUTTONS_CANCEL:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_CANCEL,
                             GTK_RESPONSE_REJECT);
      break;

    case GTK_BUTTONS_YES_NO:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_YES,
                             GTK_RESPONSE_ACCEPT);
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_NO,
                             GTK_RESPONSE_REJECT);
      break;

    case GTK_BUTTONS_OK_CANCEL:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_OK,
                             GTK_RESPONSE_ACCEPT);
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_BUTTON_CANCEL,
                             GTK_RESPONSE_REJECT);
      break;
      
    default:
      g_warning ("Unknown GtkButtonsType");
      break;
    }

  return widget;
}
