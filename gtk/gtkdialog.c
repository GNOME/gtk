/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkhbbox.h"
#include "gtkhseparator.h"
#include "gtkvbox.h"
#include "gtksignal.h"
#include "gdkkeysyms.h"
#include "gtkmain.h"

static void gtk_dialog_class_init (GtkDialogClass *klass);
static void gtk_dialog_init       (GtkDialog      *dialog);
static gint gtk_dialog_key_press  (GtkWidget      *widget,
                                   GdkEventKey    *key);                                   

static void gtk_dialog_add_buttons_valist(GtkDialog   *dialog,
                                          const gchar *first_button_text,
                                          gint         first_response_id,
                                          va_list      args);

enum {
  RESPONSE,
  LAST_SIGNAL
};

static gpointer parent_class;
static guint dialog_signals[LAST_SIGNAL];

GtkType
gtk_dialog_get_type (void)
{
  static GtkType dialog_type = 0;

  if (!dialog_type)
    {
      static const GtkTypeInfo dialog_info =
      {
	"GtkDialog",
	sizeof (GtkDialog),
	sizeof (GtkDialogClass),
	(GtkClassInitFunc) gtk_dialog_class_init,
	(GtkObjectInitFunc) gtk_dialog_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      dialog_type = gtk_type_unique (GTK_TYPE_WINDOW, &dialog_info);
    }

  return dialog_type;
}

static void
gtk_dialog_class_init (GtkDialogClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = g_type_class_peek_parent (class);
  
  dialog_signals[RESPONSE] =
    gtk_signal_new ("response",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkDialogClass, response),
                    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, dialog_signals, LAST_SIGNAL);

  widget_class->key_press_event = gtk_dialog_key_press;
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  GtkWidget *separator;

  dialog->vbox = gtk_vbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox), 2);
  
  gtk_container_add (GTK_CONTAINER (dialog), dialog->vbox);
  gtk_widget_show (dialog->vbox);

  dialog->action_area = gtk_hbutton_box_new ();

  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->action_area),
                             GTK_BUTTONBOX_SPREAD);
  
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 10);
  gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->action_area, FALSE, TRUE, 0);
  gtk_widget_show (dialog->action_area);

  separator = gtk_hseparator_new ();
  gtk_box_pack_end (GTK_BOX (dialog->vbox), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);
}

static gint
gtk_dialog_key_press (GtkWidget   *widget,
                      GdkEventKey *key)
{
  GdkEventAny event = { GDK_DELETE, widget->window, FALSE };

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, key))
    return TRUE;

  if (key->keyval != GDK_Escape)
    return FALSE;

  g_object_ref (G_OBJECT (event.window));
  
  gtk_main_do_event ((GdkEvent*)&event);
  
  g_object_unref (G_OBJECT (event.window));

  return TRUE;
}

GtkWidget*
gtk_dialog_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_DIALOG));
}

GtkWidget*
gtk_dialog_new_empty (const gchar     *title,
                      GtkWindow       *parent,
                      GtkDialogFlags   flags)
{
  GtkDialog *dialog;

  dialog = GTK_DIALOG (g_type_create_instance (GTK_TYPE_DIALOG));

  if (title)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  return GTK_WIDGET (dialog);
}

GtkWidget*
gtk_dialog_new_with_buttons (const gchar    *title,
                             GtkWindow      *parent,
                             GtkDialogFlags  flags,
                             const gchar    *first_button_text,
                             gint            first_response_id,
                             ...)
{
  GtkDialog *dialog;
  va_list args;
  
  dialog = GTK_DIALOG (gtk_dialog_new_empty (title, parent, flags));

  va_start (args, first_response_id);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text, first_response_id,
                                 args);
  
  va_end (args);

  return GTK_WIDGET (dialog);
}

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};

static ResponseData*
get_response_data (GtkWidget *widget)
{
  ResponseData *ad = gtk_object_get_data (GTK_OBJECT (widget),
                                          "___gtk_dialog_response_data");

  if (ad == NULL)
    {
      ad = g_new (ResponseData, 1);

      gtk_object_set_data_full (GTK_OBJECT (widget),
                                "___gtk_dialog_response_data",
                                ad,
                                g_free);
    }

  return ad;
}

static void
action_widget_activated (GtkWidget *widget, GtkDialog *dialog)
{
  ResponseData *ad;
  gint response_id;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  response_id = GTK_RESPONSE_NONE;
  
  ad = get_response_data (widget);

  if (ad != NULL)
    response_id = ad->response_id;

  gtk_dialog_response (dialog, response_id);
}

static void
connect_action_widget (GtkDialog *dialog, GtkWidget *child)
{
  if (GTK_WIDGET_GET_CLASS (child)->activate_signal != 0)
    {
      const gchar* name =
        gtk_signal_name (GTK_WIDGET_GET_CLASS (child)->activate_signal);

      gtk_signal_connect_while_alive (GTK_OBJECT (child),
                                      name,
                                      GTK_SIGNAL_FUNC (action_widget_activated),
                                      dialog,
                                      GTK_OBJECT (dialog));
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkDialog");
}
  
void
gtk_dialog_add_action_widget  (GtkDialog *dialog,
                               GtkWidget *widget,
                               gint       response_id)
{
  ResponseData *ad;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  ad = get_response_data (widget);

  ad->response_id = response_id;

  connect_action_widget (dialog, widget);

  gtk_box_pack_end (GTK_BOX (dialog->action_area),
                    widget,
                    FALSE, TRUE, 5);  
}

void
gtk_dialog_add_button (GtkDialog   *dialog,
                       const gchar *button_text,
                       gint         response_id)
{
  GtkWidget *button;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (button_text != NULL);

  button = gtk_button_new_stock (button_text, NULL);

  gtk_widget_show (button);
  
  gtk_dialog_add_action_widget (dialog,
                                button,
                                response_id);
}

static void
gtk_dialog_add_buttons_valist(GtkDialog      *dialog,
                              const gchar    *first_button_text,
                              gint            first_response_id,
                              va_list         args)
{
  const gchar* text;
  gint response_id;

  text = first_button_text;
  response_id = first_response_id;

  while (text != NULL)
    {
      gtk_dialog_add_button (dialog, text, response_id);

      text = va_arg (args, gchar*);
      if (text == NULL)
        break;
      response_id = va_arg (args, int);
    }
}

void
gtk_dialog_add_buttons (GtkDialog   *dialog,
                        const gchar *first_button_text,
                        gint         first_response_id,
                        ...)
{
  
  va_list args;

  va_start (args, first_response_id);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text, first_response_id,
                                 args);
  
  va_end (args);
}

void
gtk_dialog_response (GtkDialog *dialog,
                     gint       response_id)
{
  g_return_if_fail (dialog != NULL);
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  gtk_signal_emit (GTK_OBJECT (dialog),
                   dialog_signals[RESPONSE],
                   response_id);
}


typedef struct {
  GtkDialog *dialog;
  gint response_id;
  GMainLoop *loop;
  guint response_handler;
  guint destroy_handler;
  guint delete_handler;
} RunInfo;

static void
shutdown_loop (RunInfo *ri)
{
  if (ri->loop != NULL)
    {
      g_main_quit (ri->loop);
      g_main_destroy (ri->loop);
      ri->loop = NULL;
    }
}

static void
run_destroy_handler (GtkDialog *dialog, gpointer data)
{
  RunInfo *ri = data;

  shutdown_loop (ri);
}

static void
run_response_handler (GtkDialog *dialog,
                      gint response_id,
                      gpointer data)
{
  RunInfo *ri;

  ri = data;

  ri->response_id = response_id;

  shutdown_loop (ri);
}

static gint
run_delete_handler (GtkDialog *dialog,
                    GdkEventAny *event,
                    gpointer data)
{
  RunInfo *ri;
    
  ri = data;

  shutdown_loop (ri);

  /* emit response signal */
  gtk_dialog_response (dialog, GTK_RESPONSE_NONE);
  
  return TRUE; /* Do not destroy */
}

gint
gtk_dialog_run (GtkDialog *dialog)
{
  RunInfo ri = { NULL, GTK_RESPONSE_NONE, NULL, 0, 0 };
  gboolean was_modal;
  
  g_return_val_if_fail (dialog != NULL, -1);
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), -1);

  gtk_object_ref (GTK_OBJECT (dialog));
  
  was_modal = GTK_WINDOW (dialog)->modal;
  if (!was_modal)
    gtk_window_set_modal(GTK_WINDOW (dialog),TRUE);


  ri.response_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "response",
                        GTK_SIGNAL_FUNC (run_response_handler),
                        &ri);

  ri.destroy_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "destroy",
                        GTK_SIGNAL_FUNC (run_destroy_handler),
                        &ri);

  ri.delete_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "delete_event",
                        GTK_SIGNAL_FUNC (run_delete_handler),
                        &ri);
  
  ri.loop = g_main_new (FALSE);

  g_main_run (ri.loop);
  
  g_assert (ri.loop == NULL);
  
  if (!GTK_OBJECT_DESTROYED (dialog))
    {
      if (!was_modal)
        gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);
      
      gtk_signal_disconnect (GTK_OBJECT (dialog), ri.destroy_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), ri.response_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), ri.delete_handler);
    }

  gtk_object_unref (GTK_OBJECT (dialog));

  return ri.response_id;
}

