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
#include "gtkbbox.h"
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
#include "gtkrender.h"
#include "gtktypebuiltins.h"
#include "deprecated/gtkstock.h"

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
 * A simple example for using a GtkInfoBar:
 * |[<!-- language="C" -->
 * // set up info bar
 * GtkWidget *widget;
 * GtkInfoBar *bar;
 *
 * widget = gtk_info_bar_new ();
 * bar = GTK_INFO_BAR (bar);
 *
 * gtk_widget_set_no_show_all (widget, TRUE);
 * message_label = gtk_label_new ("");
 * gtk_widget_show (message_label);
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
 * ...
 *
 * // show an error message
 * gtk_label_set_text (GTK_LABEL (message_label), message);
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
 */

enum
{
  PROP_0,
  PROP_MESSAGE_TYPE,
  PROP_SHOW_CLOSE_BUTTON
};

struct _GtkInfoBarPrivate
{
  GtkWidget *content_area;
  GtkWidget *action_area;
  GtkWidget *close_button;
  GtkWidget *revealer;

  gboolean show_close_button;
  GtkMessageType message_type;
};

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

static guint signals[LAST_SIGNAL];

#define ACTION_AREA_DEFAULT_BORDER 5
#define ACTION_AREA_DEFAULT_SPACING 6
#define CONTENT_AREA_DEFAULT_BORDER 8
#define CONTENT_AREA_DEFAULT_SPACING 16

static void     gtk_info_bar_set_property (GObject        *object,
                                           guint           prop_id,
                                           const GValue   *value,
                                           GParamSpec     *pspec);
static void     gtk_info_bar_get_property (GObject        *object,
                                           guint           prop_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static void     gtk_info_bar_get_preferred_width          (GtkWidget *widget,
                                                           gint      *minimum_width,
                                                           gint      *natural_width);
static void     gtk_info_bar_get_preferred_height         (GtkWidget *widget,
                                                           gint      *minimum_height,
                                                           gint      *natural_height);
static gboolean gtk_info_bar_draw                         (GtkWidget      *widget,
                                                           cairo_t        *cr);
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


G_DEFINE_TYPE_WITH_CODE (GtkInfoBar, gtk_info_bar, GTK_TYPE_BOX,
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_info_bar_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_info_bar_parent_class)->finalize (object);
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
  GList *children, *list;
  GtkWidget *child = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (info_bar->priv->action_area));

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
  if (!gtk_widget_get_visible (info_bar->priv->close_button)
      && !find_button (info_bar, GTK_RESPONSE_CANCEL))
    return;

  gtk_info_bar_response (GTK_INFO_BAR (info_bar),
                         GTK_RESPONSE_CANCEL);
}

static void
get_padding_and_border (GtkWidget *widget,
                        GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
gtk_info_bar_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum_width,
                                  gint      *natural_width)
{
  GtkBorder border;

  get_padding_and_border (widget, &border);

  GTK_WIDGET_CLASS (gtk_info_bar_parent_class)->get_preferred_width (widget,
                                                                     minimum_width,
                                                                     natural_width);

  *minimum_width += border.left + border.right;
  *natural_width += border.left + border.right;
}

static void
gtk_info_bar_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum_height,
                                   gint      *natural_height)
{
  GtkBorder border;

  get_padding_and_border (widget, &border);

  GTK_WIDGET_CLASS (gtk_info_bar_parent_class)->get_preferred_height (widget,
                                                                      minimum_height,
                                                                      natural_height);

  *minimum_height += border.top + border.bottom;
  *natural_height += border.top + border.bottom;
}

static gboolean
gtk_info_bar_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr, 0, 0,
                         gtk_widget_get_allocated_width (widget),
                         gtk_widget_get_allocated_height (widget));
  gtk_render_frame (context, cr, 0, 0,
                    gtk_widget_get_allocated_width (widget),
                    gtk_widget_get_allocated_height (widget));

  GTK_WIDGET_CLASS (gtk_info_bar_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gtk_info_bar_show (GtkWidget *widget)
{
  GtkInfoBarPrivate *priv = GTK_INFO_BAR (widget)->priv;

  GTK_WIDGET_CLASS (gtk_info_bar_parent_class)->show (widget);

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);
}

static void
child_revealed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkWidget *widget = data;

  GTK_WIDGET_CLASS (gtk_info_bar_parent_class)->hide (widget);
  g_signal_handlers_disconnect_by_func (object, child_revealed, widget);
  g_object_notify (G_OBJECT (widget), "visible");
}

static void
gtk_info_bar_hide (GtkWidget *widget)
{
  GtkInfoBarPrivate *priv = GTK_INFO_BAR (widget)->priv;

  g_signal_connect_object (priv->revealer, "notify::child-revealed",
                           G_CALLBACK (child_revealed), widget, 0);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), FALSE);
}

static void
gtk_info_bar_class_init (GtkInfoBarClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;
  GtkBindingSet *binding_set;

  widget_class = GTK_WIDGET_CLASS (klass);
  object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_info_bar_get_property;
  object_class->set_property = gtk_info_bar_set_property;
  object_class->finalize = gtk_info_bar_finalize;

  widget_class->get_preferred_width = gtk_info_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_info_bar_get_preferred_height;
  widget_class->draw = gtk_info_bar_draw;
  widget_class->show = gtk_info_bar_show;
  widget_class->hide = gtk_info_bar_hide;

  klass->close = gtk_info_bar_close;

  /**
   * GtkInfoBar:message-type:
   *
   * The type of the message.
   *
   * The type may be used to determine the appearance of the info bar.
   *
   * Since: 2.18
   */
  g_object_class_install_property (object_class,
                                   PROP_MESSAGE_TYPE,
                                   g_param_spec_enum ("message-type",
                                                      P_("Message Type"),
                                                      P_("The type of message"),
                                                      GTK_TYPE_MESSAGE_TYPE,
                                                      GTK_MESSAGE_INFO,
                                                      GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkInfoBar:show-close-button:
   *
   * Whether to include a standard close button.
   *
   * Since: 3.10
   */
  g_object_class_install_property (object_class,
                                   PROP_SHOW_CLOSE_BUTTON,
                                   g_param_spec_boolean ("show-close-button",
                                                         P_("Show Close Button"),
                                                         P_("Whether to include a standard close button"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * GtkInfoBar::response:
   * @info_bar: the object on which the signal is emitted
   * @response_id: the response ID
   *
   * Emitted when an action widget is clicked or the application programmer
   * calls gtk_dialog_response(). The @response_id depends on which action
   * widget was clicked.
   *
   * Since: 2.18
   */
  signals[RESPONSE] = g_signal_new (I_("response"),
                                    G_OBJECT_CLASS_TYPE (klass),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GtkInfoBarClass, response),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__INT,
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
   *
   * Since: 2.18
   */
  signals[CLOSE] =  g_signal_new (I_("close"),
                                  G_OBJECT_CLASS_TYPE (klass),
                                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                  G_STRUCT_OFFSET (GtkInfoBarClass, close),
                                  NULL, NULL,
                                  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);

  /**
   * GtkInfoBar:content-area-border:
   *
   * The width of the border around the content
   * content area of the info bar.
   *
   * Since: 2.18
   * Deprecated: 3.6: Use gtk_container_set_border_width()
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("content-area-border",
                                                             P_("Content area border"),
                                                             P_("Width of border around the content area"),
                                                             0,
                                                             G_MAXINT,
                                                             CONTENT_AREA_DEFAULT_BORDER,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkInfoBar:content-area-spacing:
   *
   * The default spacing used between elements of the
   * content area of the info bar.
   *
   * Since: 2.18
   * Deprecated: 3.6: Use gtk_box_set_spacing()
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("content-area-spacing",
                                                             P_("Content area spacing"),
                                                             P_("Spacing between elements of the area"),
                                                             0,
                                                             G_MAXINT,
                                                             CONTENT_AREA_DEFAULT_SPACING,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkInfoBar:button-spacing:
   *
   * Spacing between buttons in the action area of the info bar.
   *
   * Since: 2.18
   * Deprecated: 3.6: Use gtk_box_set_spacing()
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("button-spacing",
                                                             P_("Button spacing"),
                                                             P_("Spacing between buttons"),
                                                             0,
                                                             G_MAXINT,
                                                             ACTION_AREA_DEFAULT_SPACING,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkInfoBar:action-area-border:
   *
   * Width of the border around the action area of the info bar.
   *
   * Since: 2.18
   * Deprecated: 3.6: Use gtk_container_set_border_width()
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("action-area-border",
                                                             P_("Action area border"),
                                                             P_("Width of border around the action area"),
                                                             0,
                                                             G_MAXINT,
                                                             ACTION_AREA_DEFAULT_BORDER,
                                                             GTK_PARAM_READABLE));

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkinfobar.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkInfoBar, content_area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkInfoBar, action_area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkInfoBar, close_button);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkInfoBar, revealer);
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
  GtkWidget *widget = GTK_WIDGET (info_bar);

  info_bar->priv = gtk_info_bar_get_instance_private (info_bar);

  gtk_widget_set_redraw_on_allocate (widget, TRUE);

  /* message-type is a CONSTRUCT property, so we init to a value
   * different from its default to trigger its property setter
   * during construction */
  info_bar->priv->message_type = GTK_MESSAGE_OTHER;

  gtk_widget_init_template (GTK_WIDGET (info_bar));

  gtk_widget_set_no_show_all (info_bar->priv->close_button, TRUE);
  g_signal_connect (info_bar->priv->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), info_bar);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_info_bar_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
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
 *
 * Since: 2.18
 */
void
gtk_info_bar_add_action_widget (GtkInfoBar *info_bar,
                                GtkWidget  *child,
                                gint        response_id)
{
  ResponseData *ad;
  guint signal_id;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));
  g_return_if_fail (GTK_IS_WIDGET (child));

  ad = get_response_data (child, TRUE);

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

  gtk_box_pack_end (GTK_BOX (info_bar->priv->action_area),
                    child, FALSE, FALSE, 0);
  if (response_id == GTK_RESPONSE_HELP)
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (info_bar->priv->action_area),
                                        child, TRUE);
}

/**
 * gtk_info_bar_get_action_area:
 * @info_bar: a #GtkInfoBar
 *
 * Returns the action area of @info_bar.
 *
 * Returns: (transfer none): the action area
 *
 * Since: 2.18
 */
GtkWidget*
gtk_info_bar_get_action_area (GtkInfoBar *info_bar)
{
  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), NULL);

  return info_bar->priv->action_area;
}

/**
 * gtk_info_bar_get_content_area:
 * @info_bar: a #GtkInfoBar
 *
 * Returns the content area of @info_bar.
 *
 * Returns: (transfer none): the content area
 *
 * Since: 2.18
 */
GtkWidget*
gtk_info_bar_get_content_area (GtkInfoBar *info_bar)
{
  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), NULL);

  return info_bar->priv->content_area;
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
 *
 * Since: 2.18
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

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (button_text)
    {
      GtkStockItem item;
      if (gtk_stock_lookup (button_text, &item))
        g_object_set (button, "use-stock", TRUE, NULL);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;

  gtk_widget_set_can_default (button, TRUE);

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
 * @first_button_text: button text or stock ID
 * @...: response ID for first button, then more text-response_id pairs,
 *     ending with %NULL
 *
 * Adds more buttons, same as calling gtk_info_bar_add_button()
 * repeatedly. The variable argument list should be %NULL-terminated
 * as with gtk_info_bar_new_with_buttons(). Each button must have both
 * text and response ID.
 *
 * Since: 2.18
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
 *
 * Since: 2.18
 */
GtkWidget *
gtk_info_bar_new (void)
{
   return g_object_new (GTK_TYPE_INFO_BAR, NULL);
}

/**
 * gtk_info_bar_new_with_buttons:
 * @first_button_text: (allow-none): stock ID or text to go in first button, or %NULL
 * @...: response ID for first button, then additional buttons, ending
 *    with %NULL
 *
 * Creates a new #GtkInfoBar with buttons. Button text/response ID
 * pairs should be listed, with a %NULL pointer ending the list.
 * Button text can be either a stock ID such as %GTK_STOCK_OK, or
 * some arbitrary text. A response ID can be any positive number,
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
 *
 * Since: 2.18
 */
void
gtk_info_bar_set_response_sensitive (GtkInfoBar *info_bar,
                                     gint        response_id,
                                     gboolean    setting)
{
  GList *children, *list;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  children = gtk_container_get_children (GTK_CONTAINER (info_bar->priv->action_area));

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
 *
 * Since: 2.18
 */
void
gtk_info_bar_set_default_response (GtkInfoBar *info_bar,
                                   gint        response_id)
{
  GList *children, *list;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  children = gtk_container_get_children (GTK_CONTAINER (info_bar->priv->action_area));

  for (list = children; list; list = list->next)
    {
      GtkWidget *widget = list->data;
      ResponseData *rd = get_response_data (widget, FALSE);

      if (rd && rd->response_id == response_id)
        gtk_widget_grab_default (widget);
    }

  g_list_free (children);
}

/**
 * gtk_info_bar_response:
 * @info_bar: a #GtkInfoBar
 * @response_id: a response ID
 *
 * Emits the “response” signal with the given @response_id.
 *
 * Since: 2.18
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
  GSList *l;
  SubParserData *data;
  GObject *object;
  GtkInfoBar *info_bar;
  ResponseData *ad;
  guint signal_id;

  if (strcmp (tagname, "action-widgets"))
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  info_bar = GTK_INFO_BAR (buildable);
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

      if (ad->response_id == GTK_RESPONSE_HELP)
        gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (info_bar->priv->action_area),
                                            GTK_WIDGET (object), TRUE);

      g_free (item->name);
      g_free (item);
    }

  g_slist_free (data->items);
  g_string_free (data->string, TRUE);
  g_slice_free (SubParserData, data);
}

/**
 * gtk_info_bar_set_message_type:
 * @info_bar: a #GtkInfoBar
 * @message_type: a #GtkMessageType
 *
 * Sets the message type of the message area.
 * GTK+ uses this type to determine what color to use
 * when drawing the message area.
 *
 * Since: 2.18
 */
void
gtk_info_bar_set_message_type (GtkInfoBar     *info_bar,
                               GtkMessageType  message_type)
{
  GtkInfoBarPrivate *priv;

  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  priv = info_bar->priv;

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

      g_object_notify (G_OBJECT (info_bar), "message-type");
    }
}

/**
 * gtk_info_bar_get_message_type:
 * @info_bar: a #GtkInfoBar
 *
 * Returns the message type of the message area.
 *
 * Returns: the message type of the message area.
 *
 * Since: 2.18
 */
GtkMessageType
gtk_info_bar_get_message_type (GtkInfoBar *info_bar)
{
  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), GTK_MESSAGE_OTHER);

  return info_bar->priv->message_type;
}


/**
 * gtk_info_bar_set_show_close_button:
 * @info_bar: a #GtkInfoBar
 * @setting: %TRUE to include a close button
 *
 * If true, a standard close button is shown. When clicked it emits
 * the response %GTK_RESPONSE_CLOSE.
 *
 * Since: 3.10
 */
void
gtk_info_bar_set_show_close_button (GtkInfoBar *info_bar,
                                    gboolean    setting)
{
  g_return_if_fail (GTK_IS_INFO_BAR (info_bar));

  if (setting != info_bar->priv->show_close_button)
    {
      info_bar->priv->show_close_button = setting;
      gtk_widget_set_visible (info_bar->priv->close_button, setting);
      g_object_notify (G_OBJECT (info_bar), "show-close-button");
    }
}

/**
 * gtk_info_bar_get_show_close_button:
 * @info_bar: a #GtkInfoBar
 *
 * Returns whether the widget will display a standard close button.
 *
 * Returns: %TRUE if the widget displays standard close button
 *
 * Since: 3.10
 */
gboolean
gtk_info_bar_get_show_close_button (GtkInfoBar *info_bar)
{
  g_return_val_if_fail (GTK_IS_INFO_BAR (info_bar), FALSE);

  return info_bar->priv->show_close_button;
}
