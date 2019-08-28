/*
 * gtkinfobar.c
 * This file is part of GTK+
 *
 * Copyright (C) 2005 - Paolo Maggi
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a
 * list of people on the gtk Team.
 * See the gedit ChangeLog files for a list of changes.
 *
 * Modified by the GTK+ team, 2008-2009.
 */


#include "config.h"

#include <stdlib.h>

#include "gtkinfobar.h"
#include "gtkaccessible.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtkenums.h"
#include "gtkbindings.h"
#include "gtkdialog.h"
#include "gtkrevealer.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkorientable.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkbinlayout.h"

/**
 * SECTION:gtkinfobar
 * @short_description: Report important messages to the user
 * @include: gtk/gtk.h
 * @see_also: #GtkStatusbar, #GtkMessageDialog
 *
 * #GtkInfoBar is a widget that can be used to show messages to
 * the user without showing a dialog. It is often temporarily shown
 * at the top or bottom of a document. In contrast to #GtkDialog, which
 * has a action area at the bottom, #GtkInfoBar has an action area
 * at the side.
 *
 * The API of #GtkInfoBar is very similar to #GtkDialog, allowing you
 * to add buttons to the action area with gtk_info_bar_add_button() or
 * gtk_info_bar_new_with_buttons(). The sensitivity of action widgets
 * can be controlled with gtk_info_bar_set_response_sensitive().
 * To add widgets to the main content area of a #GtkInfoBar, use
 * gtk_info_bar_get_content_area() and add your widgets to the container.
 *
 * Similar to #GtkMessageDialog, the contents of a #GtkInfoBar can by
 * classified as error message, warning, informational message, etc,
 * by using gtk_info_bar_set_message_type(). GTK+ may use the message type
 * to determine how the message is displayed.
 *
 * A simple example for using a #GtkInfoBar:
 * |[<!-- language="C" -->
 * GtkWidget *widget, *message_label, *content_area;
 * GtkWidget *grid;
 * GtkInfoBar *bar;
 *
 * // set up info bar
 * widget = gtk_info_bar_new ();
 * bar = GTK_INFO_BAR (widget);
 * grid = gtk_grid_new ();
 *
 * message_label = gtk_label_new ("");
 * content_area = gtk_info_bar_get_content_area (bar);
 * gtk_container_add (GTK_CONTAINER (content_area),
 *                    message_label);
 * gtk_info_bar_add_button (bar,
 *                          _("_OK"),
 *                          GTK_RESPONSE_OK);
 * g_signal_connect (bar,
 *                   "response",
 *                   G_CALLBACK (gtk_widget_hide),
 *                   NULL);
 * gtk_grid_attach (GTK_GRID (grid),
 *                  widget,
 *                  0, 2, 1, 1);
 *
 * // ...
 *
 * // show an error message
 * gtk_label_set_text (GTK_LABEL (message_label), "An error occurred!");
 * gtk_info_bar_set_message_type (bar,
 *                                GTK_MESSAGE_ERROR);
 * gtk_widget_show (bar);
 * ]|
 *
 * # GtkInfoBar as GtkBuildable
 *
 * The GtkInfoBar implementation of the GtkBuildable interface exposes
 * the content area and action area as internal children with the names
 * “content_area” and “action_area”.
 *
 * GtkInfoBar supports a custom <action-widgets> element, which can contain
 * multiple <action-widget> elements. The “response” attribute specifies a
 * numeric response, and the content of the element is the id of widget
 * (which should be a child of the dialogs @action_area).
 *
 * # CSS nodes
 *
 * GtkInfoBar has a single CSS node with name infobar. The node may get
 * one of the style classes .info, .warning, .error or .question, depending
 * on the message type.
 * If the info bar shows a close button, that button will have the .close
 * style class applied.
 */

enum
{
  PROP_0,
  PROP_MESSAGE_TYPE,
  PROP_SHOW_CLOSE_BUTTON,
  PROP_REVEALED,
  LAST_PROP
};

typedef struct _GtkInfoBarClass GtkInfoBarClass;

struct _GtkInfoBar
{
  GtkContainer parent_instance;
};

struct _GtkInfoBarClass
{
  GtkContainerClass parent_class;

  void (* response) (GtkInfoBar *info_bar, gint response_id);
  void (* close)    (GtkInfoBar *info_bar);
};

typedef struct
{
  GtkWidget *content_area;
  GtkWidget *action_area;
  GtkWidget *close_button;
  GtkWidget *revealer;

  gboolean show_close_button;
  GtkMessageType message_type;
} GtkInfoBarPrivate;

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};

enum
{
  RESPONSE,
  CLOSE,
  LAST_SIGNAL
};

static GParamSpec *props[LAST_PROP] = { NULL, };
static guint signals[LAST_SIGNAL];

static void     gtk_info_bar_set_property (GObject        *object,
                                           guint           prop_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static void     gtk_info_bar_get_property (GObject        *object,
                                           guint           prop_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void     gtk_info_bar_buildable_interface_init     (GtkBuildableIface *iface);
static gboolean  gtk_info_bar_buildable_custom_tag_start   (GtkBuildable  *buildable,
                                                            GtkBuilder    *builder,
                                                            GObject       *child,
                                                            const gchar   *tagname,
                                                            GMarkupParser *parser,
                                                            gpointer      *data);
static void      gtk_info_bar_buildable_custom_finished    (GtkBuildable  *buildable,
                                                            GtkBuilder    *builder,
                                                            GObject       *child,
                                                            const gchar   *tagname,
                                                            gpointer       user_data);
static void      gtk_info_bar_buildable_add_child          (GtkBuildable *buildable,
                                                            GtkBuilder   *builder,
                                                            GObject      *child,
                                                            const char   *type);



G_DEFINE_TYPE_WITH_CODE (GtkInfoBar, gtk_info_bar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkInfoBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_info_bar_buildable_interface_init))

static void
gtk_info_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (object);

  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      gtk_info_bar_set_message_type (info_bar, g_value_get_enum (value));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      gtk_info_bar_set_show_close_button (info_bar, g_value_get_boolean (value));
      break;
    case PROP_REVEALED:
      gtk_info_bar_set_revealed (info_bar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_info_bar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (object);

  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      g_value_set_enum (value, gtk_info_bar_get_message_type (info_bar));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_info_bar_get_show_close_button (info_bar));
      break;
    case PROP_REVEALED:
      g_value_set_boolean (value, gtk_info_bar_get_revealed (info_bar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
response_data_free (gpointer data)
{
  g_slice_free (ResponseData, data);
}

static ResponseData *
get_response_data (GtkWidget *widget,
                   gboolean   create)
{
  ResponseData *ad = g_object_get_data (G_OBJECT (widget),
                                        "gtk-info-bar-response-data");

  if (ad == NULL && create)
    {
      ad = g_slice_new (ResponseData);

      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-info-bar-response-data"),
                              ad,
                              response_data_free);
    }

  return ad;
}

static GtkWidget *
find_button (GtkInfoBar *info_bar,
             gint        response_id)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);
  GList *children, *list;
  GtkWidget *child = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (priv->action_area));

  for (list = children; list; list = list->next)
    {
      ResponseData *rd = get_response_data (list->data, FALSE);

      if (rd && rd->response_id == response_id)
        {
          child = list->data;
          break;
        }
    }

  g_list_free (children);

  return child;
}

static void
gtk_info_bar_close (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  if (!gtk_widget_get_visible (priv->close_button)
      && !find_button (info_bar, GTK_RESPONSE_CANCEL))
    return;

  gtk_info_bar_response (GTK_INFO_BAR (info_bar),
                         GTK_RESPONSE_CANCEL);
}

static void
gtk_info_bar_add (GtkContainer *container,
                  GtkWidget    *child)
{
  GtkInfoBar *self = GTK_INFO_BAR (container);
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (self);

  gtk_container_add (GTK_CONTAINER (priv->content_area), child);
}

static void
gtk_info_bar_remove (GtkContainer *container,
                     GtkWidget    *child)
{
  GtkInfoBar *self = GTK_INFO_BAR (container);
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (self);

  gtk_container_remove (GTK_CONTAINER (priv->content_area), child);
}

static void
gtk_info_bar_dispose (GObject *object)
{
  GtkInfoBar *self = GTK_INFO_BAR (object);
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (self);

  g_clear_pointer (&priv->revealer, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_info_bar_parent_class)->dispose (object);
}

static void
gtk_info_bar_class_init (GtkInfoBarClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_info_bar_get_property;
  object_class->set_property = gtk_info_bar_set_property;
  object_class->dispose = gtk_info_bar_dispose;

  container_class->add = gtk_info_bar_add;
  container_class->remove = gtk_info_bar_remove;

  klass->close = gtk_info_bar_close;

  /**
   * GtkInfoBar:message-type:
   *
   * The type of the message.
   *
   * The type may be used to determine the appearance of the info bar.
   */
  props[PROP_MESSAGE_TYPE] =
    g_param_spec_enum ("message-type",
                       P_("Message Type"),
                       P_("The type of message"),
                       GTK_TYPE_MESSAGE_TYPE,
                       GTK_MESSAGE_INFO,
                       GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkInfoBar:show-close-button:
   *
   * Whether to include a standard close button.
   */
  props[PROP_SHOW_CLOSE_BUTTON] =
    g_param_spec_boolean ("show-close-button",
                          P_("Show Close Button"),
                          P_("Whether to include a standard close button"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_REVEALED] =
    g_param_spec_boolean ("revealed",
                          P_("Reveal"),
                          P_("Controls whether the info bar shows its contents or not"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /**
   * GtkInfoBar::response:
   * @info_bar: the object on which the signal is emitted
   * @response_id: the response ID
   *
   * Emitted when an action widget is clicked or the application programmer
   * calls gtk_info_bar_response(). The @response_id depends on which action
   * widget was clicked.
   */
  signals[RESPONSE] = g_signal_new (I_("response"),
                                    G_OBJECT_CLASS_TYPE (klass),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GtkInfoBarClass, response),
                                    NULL, NULL,
                                    NULL,
                                    G_TYPE_NONE, 1,
                                    G_TYPE_INT);

  /**
   * GtkInfoBar::close:
   *
   * The ::close signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted when the user uses a keybinding to dismiss
   * the info bar.
   *
   * The default binding for this signal is the Escape key.
   */
  signals[CLOSE] =  g_signal_new (I_("close"),
                                  G_OBJECT_CLASS_TYPE (klass),
                                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                  G_STRUCT_OFFSET (GtkInfoBarClass, close),
                                  NULL, NULL,
                                  NULL,
                                  G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  gtk_widget_class_set_css_name (widget_class, I_("infobar"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
close_button_clicked_cb (GtkWidget  *button,
                         GtkInfoBar *info_bar)
{
  gtk_info_bar_response (GTK_INFO_BAR (info_bar),
                         GTK_RESPONSE_CLOSE);
}

static void
gtk_info_bar_init (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);
  GtkWidget *widget = GTK_WIDGET (info_bar);
  GtkWidget *main_box;

  /* message-type is a CONSTRUCT property, so we init to a value
   * different from its default to trigger its property setter
   * during construction */
  priv->message_type = GTK_MESSAGE_OTHER;

  priv->revealer = gtk_revealer_new ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);
  gtk_widget_set_parent (priv->revealer, widget);

  main_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (priv->revealer), main_box);

  priv->content_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (priv->content_area, TRUE);
  gtk_container_add (GTK_CONTAINER (main_box), priv->content_area);

  priv->action_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (priv->action_area, GTK_ALIGN_END);
  gtk_widget_set_valign (priv->action_area, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (main_box), priv->action_area);

  priv->close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_widget_hide (priv->close_button);
  gtk_widget_set_valign (priv->close_button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->close_button), "close");
  gtk_container_add (GTK_CONTAINER (main_box), priv->close_button);
  g_signal_connect (priv->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), info_bar);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_info_bar_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_info_bar_buildable_add_child;
  iface->custom_tag_start = gtk_info_bar_buildable_custom_tag_start;
  iface->custom_finished = gtk_info_bar_buildable_custom_finished;
}

static gint
get_response_for_widget (GtkInfoBar *info_bar,
                         GtkWidget  *widget)
{
  ResponseData *rd;

  rd = get_response_data (widget, FALSE);
  if (!rd)
    return GTK_RESPONSE_NONE;
  else
    return rd->response_id;
}

static void
action_widget_activated (GtkWidget  *widget,
                         GtkInfoBar *info_bar)
{
  gint response_id;

  response_id = get_response_for_widget (info_bar, widget);
  gtk_info_bar_response (info_bar, response_id);
}

/**
 * gtk_info_bar_add_action_widget:
 * @info_bar: a #GtkInfoBar
 * @child: an activatable widget
 * @response_id: response ID for @child
 *
 * Add an activatable widget to the action area of a #GtkInfoBar,
 * connecting a signal handler that will emit the #GtkInfoBar::response
 * signal on the message area when the widget is activated. The widget
 * is appended to the end of the message areas action area.
 */
void
gtk_info_bar_add_action_widget (GtkInfoBar *info_bar,
                                GtkWidget  *child,
                                gint        response_id)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);
  ResponseData *ad;
  guint signal_id;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));
  g_return_if_fail (GTK_IS_WIDGET (child));

  ad = get_response_data (child, TRUE);

  G_DEBUG_HERE();
  ad->response_id = response_id;

  if (GTK_IS_BUTTON (child))
    signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
  else
    signal_id = GTK_WIDGET_GET_CLASS (child)->activate_signal;

  if (signal_id)
    {
      GClosure *closure;

      closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                       G_OBJECT (info_bar));
      g_signal_connect_closure_by_id (child, signal_id, 0, closure, FALSE);
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkInfoBar");

  gtk_container_add (GTK_CONTAINER (priv->action_area), child);
}

/**
 * gtk_info_bar_get_action_area:
 * @info_bar: a #GtkInfoBar
 *
 * Returns the action area of @info_bar.
 *
 * Returns: (transfer none): the action area
 */
GtkWidget*
gtk_info_bar_get_action_area (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), NULL);

  return priv->action_area;
}

/**
 * gtk_info_bar_get_content_area:
 * @info_bar: a #GtkInfoBar
 *
 * Returns the content area of @info_bar.
 *
 * Returns: (transfer none): the content area
 */
GtkWidget*
gtk_info_bar_get_content_area (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), NULL);

  return priv->content_area;
}

/**
 * gtk_info_bar_add_button:
 * @info_bar: a #GtkInfoBar
 * @button_text: text of button
 * @response_id: response ID for the button
 *
 * Adds a button with the given text and sets things up so that
 * clicking the button will emit the “response” signal with the given
 * response_id. The button is appended to the end of the info bars's
 * action area. The button widget is returned, but usually you don't
 * need it.
 *
 * Returns: (transfer none) (type Gtk.Button): the #GtkButton widget
 * that was added
 */
GtkWidget*
gtk_info_bar_add_button (GtkInfoBar  *info_bar,
                         const gchar *button_text,
                         gint         response_id)
{
  GtkWidget *button;

  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), NULL);
  g_return_val_if_fail (button_text != NULL, NULL);

  button = gtk_button_new_with_label (button_text);
  gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);

  gtk_widget_show (button);

  gtk_info_bar_add_action_widget (info_bar, button, response_id);

  return button;
}

static void
add_buttons_valist (GtkInfoBar  *info_bar,
                    const gchar *first_button_text,
                    va_list      args)
{
  const gchar* text;
  gint response_id;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  if (first_button_text == NULL)
    return;

  text = first_button_text;
  response_id = va_arg (args, gint);

  while (text != NULL)
    {
      gtk_info_bar_add_button (info_bar, text, response_id);

      text = va_arg (args, gchar*);
      if (text == NULL)
        break;

      response_id = va_arg (args, int);
    }
}

/**
 * gtk_info_bar_add_buttons:
 * @info_bar: a #GtkInfoBar
 * @first_button_text: button text
 * @...: response ID for first button, then more text-response_id pairs,
 *     ending with %NULL
 *
 * Adds more buttons, same as calling gtk_info_bar_add_button()
 * repeatedly. The variable argument list should be %NULL-terminated
 * as with gtk_info_bar_new_with_buttons(). Each button must have both
 * text and response ID.
 */
void
gtk_info_bar_add_buttons (GtkInfoBar  *info_bar,
                          const gchar *first_button_text,
                          ...)
{
  va_list args;

  va_start (args, first_button_text);
  add_buttons_valist (info_bar, first_button_text, args);
  va_end (args);
}

/**
 * gtk_info_bar_new:
 *
 * Creates a new #GtkInfoBar object.
 *
 * Returns: a new #GtkInfoBar object
 */
GtkWidget *
gtk_info_bar_new (void)
{
   return g_object_new (GTK_TYPE_INFO_BAR, NULL);
}

/**
 * gtk_info_bar_new_with_buttons:
 * @first_button_text: (allow-none): ext to go in first button, or %NULL
 * @...: response ID for first button, then additional buttons, ending
 *    with %NULL
 *
 * Creates a new #GtkInfoBar with buttons. Button text/response ID
 * pairs should be listed, with a %NULL pointer ending the list.
 * A response ID can be any positive number,
 * or one of the values in the #GtkResponseType enumeration. If the
 * user clicks one of these dialog buttons, GtkInfoBar will emit
 * the “response” signal with the corresponding response ID.
 *
 * Returns: a new #GtkInfoBar
 */
GtkWidget*
gtk_info_bar_new_with_buttons (const gchar *first_button_text,
                               ...)
{
  GtkInfoBar *info_bar;
  va_list args;

  info_bar = GTK_INFO_BAR (gtk_info_bar_new ());

  va_start (args, first_button_text);
  add_buttons_valist (info_bar, first_button_text, args);
  va_end (args);

  return GTK_WIDGET (info_bar);
}

/**
 * gtk_info_bar_set_response_sensitive:
 * @info_bar: a #GtkInfoBar
 * @response_id: a response ID
 * @setting: TRUE for sensitive
 *
 * Calls gtk_widget_set_sensitive (widget, setting) for each
 * widget in the info bars’s action area with the given response_id.
 * A convenient way to sensitize/desensitize dialog buttons.
 */
void
gtk_info_bar_set_response_sensitive (GtkInfoBar *info_bar,
                                     gint        response_id,
                                     gboolean    setting)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);
  GList *children, *list;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  children = gtk_container_get_children (GTK_CONTAINER (priv->action_area));

  for (list = children; list; list = list->next)
    {
      GtkWidget *widget = list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        gtk_widget_set_sensitive (widget, setting);
    }

  g_list_free (children);
}

/**
 * gtk_info_bar_set_default_response:
 * @info_bar: a #GtkInfoBar
 * @response_id: a response ID
 *
 * Sets the last widget in the info bar’s action area with
 * the given response_id as the default widget for the dialog.
 * Pressing “Enter” normally activates the default widget.
 *
 * Note that this function currently requires @info_bar to
 * be added to a widget hierarchy. 
 */
void
gtk_info_bar_set_default_response (GtkInfoBar *info_bar,
                                   gint        response_id)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);
  GList *children, *list;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  children = gtk_container_get_children (GTK_CONTAINER (priv->action_area));

  for (list = children; list; list = list->next)
    {
      GtkWidget *widget = list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        {
          GtkWidget *window;

          window = gtk_widget_get_ancestor (GTK_WIDGET (info_bar), GTK_TYPE_WINDOW);
          gtk_window_set_default_widget (GTK_WINDOW (window), widget);
        }
    }

  g_list_free (children);
}

/**
 * gtk_info_bar_response:
 * @info_bar: a #GtkInfoBar
 * @response_id: a response ID
 *
 * Emits the “response” signal with the given @response_id.
 */
void
gtk_info_bar_response (GtkInfoBar *info_bar,
                       gint        response_id)
{
  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  g_signal_emit (info_bar, signals[RESPONSE], 0, response_id);
}

typedef struct
{
  gchar *name;
  gint response_id;
  gint line;
  gint col;
} ActionWidgetInfo;

typedef struct
{
  GtkInfoBar *info_bar;
  GtkBuilder *builder;
  GSList *items;
  gint response_id;
  gboolean is_text;
  GString *string;
  gint line;
  gint col;
} SubParserData;

static void
action_widget_info_free (gpointer data)
{
  ActionWidgetInfo *item = data;

  g_free (item->name);
  g_free (item);
}

static void
parser_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "action-widget") == 0)
    {
      const gchar *response;
      GValue gvalue = G_VALUE_INIT;

      if (!_gtk_builder_check_parent (data->builder, context, "action-widgets", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "response", &response,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!gtk_builder_value_from_string_type (data->builder, GTK_TYPE_RESPONSE_TYPE, response, &gvalue, error))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->response_id = g_value_get_enum (&gvalue);
      data->is_text = TRUE;
      g_string_set_size (data->string, 0);
      g_markup_parse_context_get_position (context, &data->line, &data->col);
    }
  else if (strcmp (element_name, "action-widgets") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkInfoBar", element_name,
                                        error);
    }
}

static void
parser_text_element (GMarkupParseContext  *context,
                     const gchar          *text,
                     gsize                 text_len,
                     gpointer              user_data,
                     GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
parser_end_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->is_text)
    {
      ActionWidgetInfo *item;

      item = g_new (ActionWidgetInfo, 1);
      item->name = g_strdup (data->string->str);
      item->response_id = data->response_id;
      item->line = data->line;
      item->col = data->col;

      data->items = g_slist_prepend (data->items, item);
      data->is_text = FALSE;
    }
}

static const GMarkupParser sub_parser =
{
  parser_start_element,
  parser_end_element,
  parser_text_element,
};

gboolean
gtk_info_bar_buildable_custom_tag_start (GtkBuildable  *buildable,
                                         GtkBuilder    *builder,
                                         GObject       *child,
                                         const gchar   *tagname,
                                         GMarkupParser *parser,
                                         gpointer      *parser_data)
{
  SubParserData *data;

  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, parser_data))
    return TRUE;

  if (!child && strcmp (tagname, "action-widgets") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->info_bar = GTK_INFO_BAR (buildable);
      data->builder = builder;
      data->string = g_string_new ("");
      data->items = NULL;

      *parser = sub_parser;
      *parser_data = data;
      return TRUE;
    }

  return FALSE;
}

static void
gtk_info_bar_buildable_custom_finished (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const gchar  *tagname,
                                        gpointer      user_data)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (buildable);
  GSList *l;
  SubParserData *data;
  GObject *object;
  ResponseData *ad;
  guint signal_id;

  if (strcmp (tagname, "action-widgets"))
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  data = (SubParserData*)user_data;
  data->items = g_slist_reverse (data->items);

  for (l = data->items; l; l = l->next)
    {
      ActionWidgetInfo *item = l->data;

      object = _gtk_builder_lookup_object (builder, item->name, item->line, item->col);
      if (!object)
        continue;

      ad = get_response_data (GTK_WIDGET (object), TRUE);
      ad->response_id = item->response_id;

      if (GTK_IS_BUTTON (object))
        signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
      else
        signal_id = GTK_WIDGET_GET_CLASS (object)->activate_signal;

      if (signal_id)
        {
          GClosure *closure;

          closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                           G_OBJECT (info_bar));
          g_signal_connect_closure_by_id (object, signal_id, 0, closure, FALSE);
        }
    }

  g_slist_free_full (data->items, action_widget_info_free);
  g_string_free (data->string, TRUE);
  g_slice_free (SubParserData, data);
}

static void
gtk_info_bar_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const char   *type)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (buildable);
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  if (!type)
    gtk_container_add (GTK_CONTAINER (priv->content_area), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "action") == 0)
    gtk_container_add (GTK_CONTAINER (priv->action_area), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

/**
 * gtk_info_bar_set_message_type:
 * @info_bar: a #GtkInfoBar
 * @message_type: a #GtkMessageType
 *
 * Sets the message type of the message area.
 *
 * GTK+ uses this type to determine how the message is displayed.
 */
void
gtk_info_bar_set_message_type (GtkInfoBar     *info_bar,
                               GtkMessageType  message_type)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  if (priv->message_type != message_type)
    {
      GtkStyleContext *context;
      AtkObject *atk_obj;
      const char *type_class[] = {
        GTK_STYLE_CLASS_INFO,
        GTK_STYLE_CLASS_WARNING,
        GTK_STYLE_CLASS_QUESTION,
        GTK_STYLE_CLASS_ERROR,
        NULL
      };

      context = gtk_widget_get_style_context (GTK_WIDGET (info_bar));

      if (type_class[priv->message_type])
        gtk_style_context_remove_class (context, type_class[priv->message_type]);

      priv->message_type = message_type;

      gtk_widget_queue_draw (GTK_WIDGET (info_bar));

      atk_obj = gtk_widget_get_accessible (GTK_WIDGET (info_bar));
      if (GTK_IS_ACCESSIBLE (atk_obj))
        {
          const char *name = NULL;

          atk_object_set_role (atk_obj, ATK_ROLE_INFO_BAR);

          switch (message_type)
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
              g_warning ("Unknown GtkMessageType %u", message_type);
              break;
            }

          if (name)
            atk_object_set_name (atk_obj, name);
        }

      if (type_class[priv->message_type])
        gtk_style_context_add_class (context, type_class[priv->message_type]);

      g_object_notify_by_pspec (G_OBJECT (info_bar), props[PROP_MESSAGE_TYPE]);
    }
}

/**
 * gtk_info_bar_get_message_type:
 * @info_bar: a #GtkInfoBar
 *
 * Returns the message type of the message area.
 *
 * Returns: the message type of the message area.
 */
GtkMessageType
gtk_info_bar_get_message_type (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), GTK_MESSAGE_OTHER);

  return priv->message_type;
}


/**
 * gtk_info_bar_set_show_close_button:
 * @info_bar: a #GtkInfoBar
 * @setting: %TRUE to include a close button
 *
 * If true, a standard close button is shown. When clicked it emits
 * the response %GTK_RESPONSE_CLOSE.
 */
void
gtk_info_bar_set_show_close_button (GtkInfoBar *info_bar,
                                    gboolean    setting)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  if (setting != priv->show_close_button)
    {
      priv->show_close_button = setting;
      gtk_widget_set_visible (priv->close_button, setting);
      g_object_notify_by_pspec (G_OBJECT (info_bar), props[PROP_SHOW_CLOSE_BUTTON]);
    }
}

/**
 * gtk_info_bar_get_show_close_button:
 * @info_bar: a #GtkInfoBar
 *
 * Returns whether the widget will display a standard close button.
 *
 * Returns: %TRUE if the widget displays standard close button
 */
gboolean
gtk_info_bar_get_show_close_button (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), FALSE);

  return priv->show_close_button;
}

/**
 * gtk_info_bar_set_revealed:
 * @info_bar: a #GtkInfoBar
 * @revealed: The new value of the property
 *
 * Sets the #GtkInfoBar:revealed property to @revealed. Changing this will make
 * @info_bar reveal (%TRUE) or conceal (%FALSE) itself via a sliding transition.
 *
 * Note: this does not show or hide @info_bar in the #GtkWidget:visible sense,
 * so revealing has no effect if #GtkWidget:visible is %FALSE.
 */
void
gtk_info_bar_set_revealed (GtkInfoBar *info_bar,
                           gboolean    revealed)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  revealed = !!revealed;
  if (revealed != gtk_revealer_get_reveal_child (GTK_REVEALER (priv->revealer)))
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), revealed);
      g_object_notify_by_pspec (G_OBJECT (info_bar), props[PROP_REVEALED]);
    }
}

/**
 * gtk_info_bar_get_revealed:
 * @info_bar: a #GtkInfoBar
 *
 * Returns whether the info bar is currently revealed.
 *
 * Returns: the current value of the #GtkInfoBar:revealed property
 */
gboolean
gtk_info_bar_get_revealed (GtkInfoBar *info_bar)
{
  GtkInfoBarPrivate *priv = gtk_info_bar_get_instance_private (info_bar);

  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), FALSE);

  return gtk_revealer_get_reveal_child (GTK_REVEALER (priv->revealer));
}
