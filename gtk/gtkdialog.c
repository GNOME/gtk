/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkhbbox.h"
#include "gtkhseparator.h"
#include "gtkmarshalers.h"
#include "gtkvbox.h"
#include "gtksignal.h"
#include "gdkkeysyms.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtkbindings.h"

static void gtk_dialog_class_init (GtkDialogClass *klass);
static void gtk_dialog_init       (GtkDialog      *dialog);

static void gtk_dialog_add_buttons_valist (GtkDialog   *dialog,
                                           const gchar *first_button_text,
                                           va_list      args);

static gint gtk_dialog_delete_event_handler (GtkWidget   *widget,
                                             GdkEventAny *event,
                                             gpointer     user_data);

static void gtk_dialog_set_property      (GObject          *object,
                                          guint             prop_id,
                                          const GValue     *value,
                                          GParamSpec       *pspec);
static void gtk_dialog_get_property      (GObject          *object,
                                          guint             prop_id,
                                          GValue           *value,
                                          GParamSpec       *pspec);
static void gtk_dialog_style_set         (GtkWidget        *widget,
                                          GtkStyle         *prev_style);
static void gtk_dialog_map               (GtkWidget        *widget);

static void gtk_dialog_close             (GtkDialog        *dialog);

enum {
  PROP_0,
  PROP_HAS_SEPARATOR
};

enum {
  RESPONSE,
  CLOSE,
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
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  
  gobject_class = G_OBJECT_CLASS (class);
  object_class = GTK_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);

  gobject_class->set_property = gtk_dialog_set_property;
  gobject_class->get_property = gtk_dialog_get_property;
  
  widget_class->map = gtk_dialog_map;
  widget_class->style_set = gtk_dialog_style_set;

  class->close = gtk_dialog_close;
  
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_SEPARATOR,
                                   g_param_spec_boolean ("has_separator",
							 _("Has separator"),
							 _("The dialog has a separator bar above its buttons"),
                                                         TRUE,
                                                         G_PARAM_READWRITE));
  
  dialog_signals[RESPONSE] =
    gtk_signal_new ("response",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkDialogClass, response),
                    _gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_INT);

  dialog_signals[CLOSE] =
    gtk_signal_new ("close",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkDialogClass, close),
                    _gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("content_area_border",
                                                             _("Content area border"),
                                                             _("Width of border around the main dialog area"),
                                                             0,
                                                             G_MAXINT,
                                                             2,
                                                             G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("button_spacing",
                                                             _("Button spacing"),
                                                             _("Spacing between buttons"),
                                                             0,
                                                             G_MAXINT,
                                                             10,
                                                             G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("action_area_border",
                                                             _("Action area border"),
                                                             _("Width of border around the button area at the bottom of the dialog"),
                                                             0,
                                                             G_MAXINT,
                                                             5,
                                                             G_PARAM_READABLE));

  binding_set = gtk_binding_set_by_class (class);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
                                "close", 0);
}

static void
update_spacings (GtkDialog *dialog)
{
  GtkWidget *widget;
  gint content_area_border;
  gint button_spacing;
  gint action_area_border;
  
  widget = GTK_WIDGET (dialog);

  gtk_widget_style_get (widget,
                        "content_area_border",
                        &content_area_border,
                        "button_spacing",
                        &button_spacing,
                        "action_area_border",
                        &action_area_border,
                        NULL);

  gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox),
                                  content_area_border);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area),
                       button_spacing);
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area),
                                  action_area_border);
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  /* To avoid breaking old code that prevents destroy on delete event
   * by connecting a handler, we have to have the FIRST signal
   * connection on the dialog.
   */
  gtk_signal_connect (GTK_OBJECT (dialog),
                      "delete_event",
                      GTK_SIGNAL_FUNC (gtk_dialog_delete_event_handler),
                      NULL);
  
  dialog->vbox = gtk_vbox_new (FALSE, 0);
  
  gtk_container_add (GTK_CONTAINER (dialog), dialog->vbox);
  gtk_widget_show (dialog->vbox);

  dialog->action_area = gtk_hbutton_box_new ();

  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->action_area),
                             GTK_BUTTONBOX_END);  

  gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->action_area,
                    FALSE, TRUE, 0);
  gtk_widget_show (dialog->action_area);

  dialog->separator = gtk_hseparator_new ();
  gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->separator, FALSE, TRUE, 0);
  gtk_widget_show (dialog->separator);

  gtk_window_set_type_hint (GTK_WINDOW (dialog),
			    GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
}


static void 
gtk_dialog_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkDialog *dialog;
  
  dialog = GTK_DIALOG (object);
  
  switch (prop_id)
    {
    case PROP_HAS_SEPARATOR:
      gtk_dialog_set_has_separator (dialog, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_dialog_get_property (GObject     *object,
                         guint        prop_id,
                         GValue      *value,
                         GParamSpec  *pspec)
{
  GtkDialog *dialog;
  
  dialog = GTK_DIALOG (object);
  
  switch (prop_id)
    {
    case PROP_HAS_SEPARATOR:
      g_value_set_boolean (value, dialog->separator != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gint
gtk_dialog_delete_event_handler (GtkWidget   *widget,
                                 GdkEventAny *event,
                                 gpointer     user_data)
{
  /* emit response signal */
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_DELETE_EVENT);

  /* Do the destroy by default */
  return FALSE;
}

/* A far too tricky heuristic for getting the right initial
 * focus widget if none was set. What we do is we focus the first
 * widget in the tab chain, but if this results in the focus
 * ending up on one of the response widgets _other_ than the
 * default response, we focus the default response instead.
 */
static void
gtk_dialog_map (GtkWidget *widget)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkDialog *dialog = GTK_DIALOG (widget);
  
  GTK_WIDGET_CLASS (parent_class)->map (widget);

  if (!window->focus_widget)
    {
      GList *children, *tmp_list;
      
      g_signal_emit_by_name (window, "move_focus", GTK_DIR_TAB_FORWARD);

      tmp_list = children = gtk_container_get_children (GTK_CONTAINER (dialog->action_area));

      while (tmp_list)
	{
	  GtkWidget *child = tmp_list->data;

	  if (child == window->focus_widget && child != window->default_widget && window->default_widget)
	    {
	      gtk_widget_grab_focus (window->default_widget);
	      break;
	    }
	  
	  tmp_list = tmp_list->next;
	}

      g_list_free (children);
    }
}

static void
gtk_dialog_style_set (GtkWidget *widget,
                      GtkStyle  *prev_style)
{
  update_spacings (GTK_DIALOG (widget));
}

static void
gtk_dialog_close (GtkDialog *dialog)
{
  /* Synthesize delete_event to close dialog. */
  
  GdkEventAny event;
  GtkWidget *widget;

  widget = GTK_WIDGET (dialog);
  
  event.type = GDK_DELETE;
  event.window = widget->window;
  event.send_event = TRUE;
  
  g_object_ref (G_OBJECT (event.window));
  
  gtk_main_do_event ((GdkEvent*)&event);
  
  g_object_unref (G_OBJECT (event.window));
}

GtkWidget*
gtk_dialog_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_DIALOG));
}

static GtkWidget*
gtk_dialog_new_empty (const gchar     *title,
                      GtkWindow       *parent,
                      GtkDialogFlags   flags)
{
  GtkDialog *dialog;

  dialog = GTK_DIALOG (g_object_new (GTK_TYPE_DIALOG, NULL));

  if (title)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  
  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    gtk_dialog_set_has_separator (dialog, FALSE);
  
  return GTK_WIDGET (dialog);
}

/**
 * gtk_dialog_new_with_buttons:
 * @title: Title of the dialog, or %NULL
 * @parent: Transient parent of the dialog, or %NULL
 * @flags: from #GtkDialogFlags
 * @first_button_text: stock ID or text to go in first button, or %NULL
 * @Varargs: response ID for first button, then additional buttons, ending with %NULL
 * 
 * Creates a new #GtkDialog with title @title (or %NULL for the default
 * title; see gtk_window_set_title()) and transient parent @parent (or
 * %NULL for none; see gtk_window_set_transient_for()). The @flags
 * argument can be used to make the dialog modal (#GTK_DIALOG_MODAL)
 * and/or to have it destroyed along with its transient parent
 * (#GTK_DIALOG_DESTROY_WITH_PARENT). After @flags, button
 * text/response ID pairs should be listed, with a %NULL pointer ending
 * the list. Button text can be either a stock ID such as
 * #GTK_STOCK_OK, or some arbitrary text.  A response ID can be
 * any positive number, or one of the values in the #GtkResponseType
 * enumeration. If the user clicks one of these dialog buttons,
 * #GtkDialog will emit the "response" signal with the corresponding
 * response ID. If a #GtkDialog receives the "delete_event" signal, it
 * will emit "response" with a response ID of #GTK_RESPONSE_DELETE_EVENT.
 * However, destroying a dialog does not emit the "response" signal;
 * so be careful relying on "response" when using
 * the #GTK_DIALOG_DESTROY_WITH_PARENT flag. Buttons are from left to right,
 * so the first button in the list will be the leftmost button in the dialog.
 *
 * Here's a simple example:
 * <informalexample><programlisting>
 *  <!>GtkWidget *dialog = gtk_dialog_new_with_buttons ("My dialog",
 *  <!>                                                 main_app_window,
 *  <!>                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
 *  <!>                                                 GTK_STOCK_OK,
 *  <!>                                                 GTK_RESPONSE_ACCEPT,
 *  <!>                                                 GTK_STOCK_CANCEL,
 *  <!>                                                 GTK_RESPONSE_REJECT,
 *  <!>                                                 NULL);
 * </programlisting></informalexample>
 * 
 * Return value: a new #GtkDialog
 **/
GtkWidget*
gtk_dialog_new_with_buttons (const gchar    *title,
                             GtkWindow      *parent,
                             GtkDialogFlags  flags,
                             const gchar    *first_button_text,
                             ...)
{
  GtkDialog *dialog;
  va_list args;
  
  dialog = GTK_DIALOG (gtk_dialog_new_empty (title, parent, flags));

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
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
                                          "gtk-dialog-response-data");

  if (ad == NULL)
    {
      ad = g_new (ResponseData, 1);
      
      gtk_object_set_data_full (GTK_OBJECT (widget),
                                "gtk-dialog-response-data",
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

  g_assert (ad != NULL);
  
  response_id = ad->response_id;

  gtk_dialog_response (dialog, response_id);
}
/**
 * gtk_dialog_add_action_widget:
 * @dialog: a #GtkDialog
 * @child: an activatable widget
 * @response_id: response ID for @child
 * 
 * Adds an activatable widget to the action area of a #GtkDialog,
 * connecting a signal handler that will emit the "response" signal on
 * the dialog when the widget is activated.  The widget is appended to
 * the end of the dialog's action area.  If you want to add a
 * non-activatable widget, simply pack it into the
 * <literal>action_area</literal> field of the #GtkDialog struct.
 **/
void
gtk_dialog_add_action_widget  (GtkDialog *dialog,
                               GtkWidget *child,
                               gint       response_id)
{
  ResponseData *ad;
  gint signal_id = 0;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_WIDGET (child));

  ad = get_response_data (child);

  ad->response_id = response_id;

  if (GTK_IS_BUTTON (child))
    {
      signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
    }
  else
    signal_id = GTK_WIDGET_GET_CLASS (child)->activate_signal != 0;

  if (signal_id)
    {
      const gchar* name = gtk_signal_name (signal_id);

      gtk_signal_connect_while_alive (GTK_OBJECT (child),
                                      name,
                                      GTK_SIGNAL_FUNC (action_widget_activated),
                                      dialog,
                                      GTK_OBJECT (dialog));
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkDialog");

  gtk_box_pack_end (GTK_BOX (dialog->action_area),
                    child,
                    FALSE, TRUE, 0);
  
  if (response_id == GTK_RESPONSE_HELP)
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (dialog->action_area), child, TRUE);
}

/**
 * gtk_dialog_add_button:
 * @dialog: a #GtkDialog
 * @button_text: text of button, or stock ID
 * @response_id: response ID for the button
 * 
 * Adds a button with the given text (or a stock button, if @button_text is a
 * stock ID) and sets things up so that clicking the button will emit the
 * "response" signal with the given @response_id. The button is appended to the
 * end of the dialog's action area. The button widget is returned, but usually
 * you don't need it.
 *
 * Return value: the button widget that was added
 **/
GtkWidget*
gtk_dialog_add_button (GtkDialog   *dialog,
                       const gchar *button_text,
                       gint         response_id)
{
  GtkWidget *button;
  
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);
  g_return_val_if_fail (button_text != NULL, NULL);

  button = gtk_button_new_from_stock (button_text);

  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  
  gtk_widget_show (button);
  
  gtk_dialog_add_action_widget (dialog,
                                button,
                                response_id);

  return button;
}

static void
gtk_dialog_add_buttons_valist(GtkDialog      *dialog,
                              const gchar    *first_button_text,
                              va_list         args)
{
  const gchar* text;
  gint response_id;

  g_return_if_fail (GTK_IS_DIALOG (dialog));
  
  if (first_button_text == NULL)
    return;
  
  text = first_button_text;
  response_id = va_arg (args, gint);

  while (text != NULL)
    {
      gtk_dialog_add_button (dialog, text, response_id);

      text = va_arg (args, gchar*);
      if (text == NULL)
        break;
      response_id = va_arg (args, int);
    }
}

/**
 * gtk_dialog_add_buttons:
 * @dialog: a #GtkDialog
 * @first_button_text: button text or stock ID
 * @Varargs: response ID for first button, then more text-response_id pairs
 * 
 * Adds more buttons, same as calling gtk_dialog_add_button()
 * repeatedly.  The variable argument list should be %NULL-terminated
 * as with gtk_dialog_new_with_buttons(). Each button must have both
 * text and response ID.
 **/
void
gtk_dialog_add_buttons (GtkDialog   *dialog,
                        const gchar *first_button_text,
                        ...)
{  
  va_list args;

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);
  
  va_end (args);
}

/**
 * gtk_dialog_set_response_sensitive:
 * @dialog: a #GtkDialog
 * @response_id: a response ID
 * @setting: %TRUE for sensitive
 *
 * Calls <literal>gtk_widget_set_sensitive (widget, @setting)</literal> 
 * for each widget in the dialog's action area with the given @response_id.
 * A convenient way to sensitize/desensitize dialog buttons.
 **/
void
gtk_dialog_set_response_sensitive (GtkDialog *dialog,
                                   gint       response_id,
                                   gboolean   setting)
{
  GList *children;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  children = gtk_container_get_children (GTK_CONTAINER (dialog->action_area));

  tmp_list = children;
  while (tmp_list != NULL)
    {
      GtkWidget *widget = tmp_list->data;
      ResponseData *rd = g_object_get_data (G_OBJECT (widget),
                                            "gtk-dialog-response-data");

      if (rd && rd->response_id == response_id)
        gtk_widget_set_sensitive (widget, setting);

      tmp_list = g_list_next (tmp_list);
    }

  g_list_free (children);
}

/**
 * gtk_dialog_set_default_response:
 * @dialog: a #GtkDialog
 * @response_id: a response ID
 * 
 * Sets the last widget in the dialog's action area with the given @response_id
 * as the default widget for the dialog. Pressing "Enter" normally activates
 * the default widget.
 **/
void
gtk_dialog_set_default_response (GtkDialog *dialog,
                                 gint       response_id)
{
  GList *children;
  GList *tmp_list;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  children = gtk_container_get_children (GTK_CONTAINER (dialog->action_area));

  tmp_list = children;
  while (tmp_list != NULL)
    {
      GtkWidget *widget = tmp_list->data;
      ResponseData *rd = g_object_get_data (G_OBJECT (widget),
                                            "gtk-dialog-response-data");

      if (rd && rd->response_id == response_id)
	{
	  gtk_widget_grab_default (widget);
	  
	  if (!GTK_WINDOW (dialog)->focus_widget)
	    gtk_widget_grab_focus (widget);
	}
	    
      tmp_list = g_list_next (tmp_list);
    }

  g_list_free (children);
}

/**
 * gtk_dialog_set_has_separator:
 * @dialog: a #GtkDialog
 * @setting: %TRUE to have a separator
 *
 * Sets whether the dialog has a separator above the buttons.
 * %TRUE by default.
 **/
void
gtk_dialog_set_has_separator (GtkDialog *dialog,
                              gboolean   setting)
{
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  /* this might fail if we get called before _init() somehow */
  g_assert (dialog->vbox != NULL);
  
  if (setting && dialog->separator == NULL)
    {
      dialog->separator = gtk_hseparator_new ();
      gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->separator, FALSE, TRUE, 0);

      /* The app programmer could screw this up, but, their own fault.
       * Moves the separator just above the action area.
       */
      gtk_box_reorder_child (GTK_BOX (dialog->vbox), dialog->separator, 1);
      gtk_widget_show (dialog->separator);
    }
  else if (!setting && dialog->separator != NULL)
    {
      gtk_widget_destroy (dialog->separator);
      dialog->separator = NULL;
    }

  g_object_notify (G_OBJECT (dialog), "has_separator");
}

/**
 * gtk_dialog_get_has_separator:
 * @dialog: a #GtkDialog
 * 
 * Accessor for whether the dialog has a separator.
 * 
 * Return value: %TRUE if the dialog has a separator
 **/
gboolean
gtk_dialog_get_has_separator (GtkDialog *dialog)
{
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), FALSE);

  return dialog->separator != NULL;
}

/**
 * gtk_dialog_response:
 * @dialog: a #GtkDialog
 * @response_id: response ID 
 * 
 * Emits the "response" signal with the given response ID. Used to
 * indicate that the user has responded to the dialog in some way;
 * typically either you or gtk_dialog_run() will be monitoring the
 * "response" signal and take appropriate action.
 **/
void
gtk_dialog_response (GtkDialog *dialog,
                     gint       response_id)
{
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  gtk_signal_emit (GTK_OBJECT (dialog),
                   dialog_signals[RESPONSE],
                   response_id);
}

typedef struct
{
  GtkDialog *dialog;
  gint response_id;
  GMainLoop *loop;
  gboolean destroyed;
} RunInfo;

static void
shutdown_loop (RunInfo *ri)
{
  if (g_main_loop_is_running (ri->loop))
    g_main_loop_quit (ri->loop);
}

static void
run_unmap_handler (GtkDialog *dialog, gpointer data)
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
  RunInfo *ri = data;
    
  shutdown_loop (ri);
  
  return TRUE; /* Do not destroy */
}

static void
run_destroy_handler (GtkDialog *dialog, gpointer data)
{
  RunInfo *ri = data;

  /* shutdown_loop will be called by run_unmap_handler */
  
  ri->destroyed = TRUE;
}

/**
 * gtk_dialog_run:
 * @dialog: a #GtkDialog
 * 
 * Blocks in a recursive main loop until the @dialog either emits the
 * response signal, or is destroyed. If the dialog is destroyed,
 * gtk_dialog_run() returns #GTK_RESPONSE_NONE. Otherwise, it returns
 * the response ID from the "response" signal emission. Before
 * entering the recursive main loop, gtk_dialog_run() calls
 * gtk_widget_show() on the dialog for you. Note that you still
 * need to show any children of the dialog yourself.
 *
 * During gtk_dialog_run(), the default behavior of "delete_event" is
 * disabled; if the dialog receives "delete_event", it will not be
 * destroyed as windows usually are, and gtk_dialog_run() will return
 * #GTK_RESPONSE_DELETE_EVENT. Also, during gtk_dialog_run() the dialog will be
 * modal. You can force gtk_dialog_run() to return at any time by
 * calling gtk_dialog_response() to emit the "response"
 * signal. Destroying the dialog during gtk_dialog_run() is a very bad
 * idea, because your post-run code won't know whether the dialog was
 * destroyed or not.
 *
 * After gtk_dialog_run() returns, you are responsible for hiding or
 * destroying the dialog if you wish to do so.
 *
 * Typical usage of this function might be:
 * <informalexample><programlisting>
 * <!>  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
 * <!>  switch (result)
 * <!>    {
 * <!>      case GTK_RESPONSE_ACCEPT:
 * <!>         do_application_specific_something (<!-- -->);
 * <!>         break;
 * <!>      default:
 * <!>         do_nothing_since_dialog_was_cancelled (<!-- -->);
 * <!>         break;
 * <!>    }
 * <!>  gtk_widget_destroy (dialog);
 * </programlisting></informalexample>
 * 
 * Return value: response ID
 **/
gint
gtk_dialog_run (GtkDialog *dialog)
{
  RunInfo ri = { NULL, GTK_RESPONSE_NONE, NULL };
  gboolean was_modal;
  guint response_handler;
  guint unmap_handler;
  guint destroy_handler;
  guint delete_handler;
  
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), -1);

  gtk_object_ref (GTK_OBJECT (dialog));

  if (!GTK_WIDGET_VISIBLE (dialog))
    gtk_widget_show (GTK_WIDGET (dialog));
  
  was_modal = GTK_WINDOW (dialog)->modal;
  if (!was_modal)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  response_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "response",
                        GTK_SIGNAL_FUNC (run_response_handler),
                        &ri);
  
  unmap_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "unmap",
                        GTK_SIGNAL_FUNC (run_unmap_handler),
                        &ri);
  
  delete_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "delete_event",
                        GTK_SIGNAL_FUNC (run_delete_handler),
                        &ri);
  
  destroy_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "destroy",
                        GTK_SIGNAL_FUNC (run_destroy_handler),
                        &ri);
  
  ri.loop = g_main_new (FALSE);

  GDK_THREADS_LEAVE ();  
  g_main_loop_run (ri.loop);
  GDK_THREADS_ENTER ();  

  g_main_loop_unref (ri.loop);

  ri.loop = NULL;
  ri.destroyed = FALSE;
  
  if (!ri.destroyed)
    {
      if (!was_modal)
        gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);
      
      gtk_signal_disconnect (GTK_OBJECT (dialog), response_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), unmap_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), delete_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), destroy_handler);
    }

  gtk_object_unref (GTK_OBJECT (dialog));

  return ri.response_id;
}
