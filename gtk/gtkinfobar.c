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
#include "gtkbbox.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtkenums.h"
#include "gtkbindings.h"
#include "gtkdialog.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkstock.h"
#include "gtkorientable.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtkinfobar
 * @short_description: Report important messages to the user
 * @include: gtk/gtk.h
 * @see_also: #GtkStatusbar, #GtkMessageDialog
 *
 * #GtkInfoBar is a widget that can be used to show messages to
 * the user without showing a dialog. It is often temporarily shown
 * at the top or bottom of a document. In contrast to #GtkDialog, which
 * has a horizontal action area at the bottom, #GtkInfoBar has a
 * vertical action area at the side.
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
 * by using gtk_info_bar_set_message_type(). GTK+ uses the message type
 * to determine the background color of the message area.
 *
 * <example>
 * <title>Simple GtkInfoBar usage.</title>
 * <programlisting>
 * /&ast; set up info bar &ast;/
 * info_bar = gtk_info_bar_new ();
 * gtk_widget_set_no_show_all (info_bar, TRUE);
 * message_label = gtk_label_new ("");
 * gtk_widget_show (message_label);
 * content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
 * gtk_container_add (GTK_CONTAINER (content_area), message_label);
 * gtk_info_bar_add_button (GTK_INFO_BAR (info_bar),
 *                          GTK_STOCK_OK, GTK_RESPONSE_OK);
 * g_signal_connect (info_bar, "response",
 *                   G_CALLBACK (gtk_widget_hide), NULL);
 * gtk_grid_attach (GTK_GRID (grid),
 *                  info_bar,
 *                  0, 2, 1, 1);
 *
 * /&ast; ... &ast;/
 *
 * /&ast; show an error message &ast;/
 * gtk_label_set_text (GTK_LABEL (message_label), error_message);
 * gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar),
 *                                GTK_MESSAGE_ERROR);
 * gtk_widget_show (info_bar);
 * </programlisting>
 * </example>
 *
 * <refsect2 id="GtkInfoBar-BUILDER-UI">
 * <title>GtkInfoBar as GtkBuildable</title>
 * <para>
 * The GtkInfoBar implementation of the GtkBuildable interface exposes
 * the content area and action area as internal children with the names
 * "content_area" and "action_area".
 * </para>
 * <para>
 * GtkInfoBar supports a custom &lt;action-widgets&gt; element, which
 * can contain multiple &lt;action-widget&gt; elements. The "response"
 * attribute specifies a numeric response, and the content of the element
 * is the id of widget (which should be a child of the dialogs @action_area).
 * </para>
 * </refsect2>
 */

enum
{
  PROP_0,
  PROP_MESSAGE_TYPE
};

struct _GtkInfoBarPrivate
{
  GtkWidget *content_area;
  GtkWidget *action_area;

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
static GObject *gtk_info_bar_buildable_get_internal_child (GtkBuildable  *buildable,
                                                           GtkBuilder    *builder,
                                                           const gchar   *childname);
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
  if (!find_button (info_bar, GTK_RESPONSE_CANCEL))
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

  if (minimum_width)
    *minimum_width += border.left + border.right;
  if (natural_width)
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

  if (minimum_height)
    *minimum_height += border.top + border.bottom;
  if (natural_height)
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

  klass->close = gtk_info_bar_close;

  /**
   * GtkInfoBar:message-type:
   *
   * The type of the message.
   *
   * The type is used to determine the colors to use in the info bar.
   * The following symbolic color names can by used to customize
   * these colors:
   * "info_fg_color", "info_bg_color",
   * "warning_fg_color", "warning_bg_color",
   * "question_fg_color", "question_bg_color",
   * "error_fg_color", "error_bg_color".
   * "other_fg_color", "other_bg_color".
   *
   * If the type is #GTK_MESSAGE_OTHER, no info bar is painted but the
   * colors are still set.
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
                                                      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
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
   * <link linkend="keybinding-signals">keybinding signal</link>
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

  g_type_class_add_private (object_class, sizeof (GtkInfoBarPrivate));
}

static void
gtk_info_bar_init (GtkInfoBar *info_bar)
{
  GtkWidget *widget = GTK_WIDGET (info_bar);
  GtkWidget *content_area;
  GtkWidget *action_area;

  gtk_widget_push_composite_child ();

  info_bar->priv = G_TYPE_INSTANCE_GET_PRIVATE (info_bar,
                                                GTK_TYPE_INFO_BAR,
                                                GtkInfoBarPrivate);

  content_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (content_area);
  gtk_box_pack_start (GTK_BOX (info_bar), content_area, TRUE, TRUE, 0);

  action_area = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (action_area), GTK_BUTTONBOX_END);
  gtk_box_pack_start (GTK_BOX (info_bar), action_area, FALSE, TRUE, 0);

  gtk_widget_set_app_paintable (widget, TRUE);
  gtk_widget_set_redraw_on_allocate (widget, TRUE);

  info_bar->priv->content_area = content_area;
  info_bar->priv->action_area = action_area;

  /* set default spacings */
  gtk_box_set_spacing (GTK_BOX (info_bar->priv->action_area), ACTION_AREA_DEFAULT_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (info_bar->priv->action_area), ACTION_AREA_DEFAULT_BORDER);
  gtk_box_set_spacing (GTK_BOX (info_bar->priv->content_area), CONTENT_AREA_DEFAULT_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (info_bar->priv->content_area), CONTENT_AREA_DEFAULT_BORDER);

  /* message-type is a CONSTRUCT property, so we init to a value
   * different from its default to trigger its property setter
   * during construction */
  info_bar->priv->message_type = GTK_MESSAGE_OTHER;

  gtk_widget_pop_composite_child ();
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_info_bar_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_info_bar_buildable_get_internal_child;
  iface->custom_tag_start = gtk_info_bar_buildable_custom_tag_start;
  iface->custom_finished = gtk_info_bar_buildable_custom_finished;
}

static GObject *
gtk_info_bar_buildable_get_internal_child (GtkBuildable *buildable,
                                           GtkBuilder   *builder,
                                           const gchar  *childname)
{
  if (strcmp (childname, "content_area") == 0)
    return G_OBJECT (GTK_INFO_BAR (buildable)->priv->content_area);
  else if (strcmp (childname, "action_area") == 0)
    return G_OBJECT (GTK_INFO_BAR (buildable)->priv->action_area);

  return parent_buildable_iface->get_internal_child (buildable,
                                                     builder,
                                                     childname);
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
 * @button_text: text of button, or stock ID
 * @response_id: response ID for the button
 *
 * Adds a button with the given text (or a stock button, if button_text
 * is a stock ID) and sets things up so that clicking the button will emit
 * the "response" signal with the given response_id. The button is appended
 * to the end of the info bars's action area. The button widget is
 * returned, but usually you don't need it.
 *
 * Returns: (transfer none): the #GtkButton widget that was added
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

  button = gtk_button_new_from_stock (button_text);

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
 * the "response" signal with the corresponding response ID.
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
 * widget in the info bars's action area with the given response_id.
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
 * Sets the last widget in the info bar's action area with
 * the given response_id as the default widget for the dialog.
 * Pressing "Enter" normally activates the default widget.
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
 * Emits the 'response' signal with the given @response_id.
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
  gchar *widget_name;
  gchar *response_id;
} ActionWidgetInfo;

typedef struct
{
  GtkInfoBar *info_bar;
  GtkBuilder *builder;
  GSList *items;
  gchar *response;
} ActionWidgetsSubParserData;

static void
attributes_start_element (GMarkupParseContext  *context,
                          const gchar          *element_name,
                          const gchar         **names,
                          const gchar         **values,
                          gpointer              user_data,
                          GError              **error)
{
  ActionWidgetsSubParserData *parser_data = (ActionWidgetsSubParserData*)user_data;
  guint i;

  if (strcmp (element_name, "action-widget") == 0)
    {
      for (i = 0; names[i]; i++)
        if (strcmp (names[i], "response") == 0)
          parser_data->response = g_strdup (values[i]);
    }
  else if (strcmp (element_name, "action-widgets") == 0)
    return;
  else
    g_warning ("Unsupported tag for GtkInfoBar: %s\n", element_name);
}

static void
attributes_text_element (GMarkupParseContext  *context,
                         const gchar          *text,
                         gsize                 text_len,
                         gpointer              user_data,
                         GError              **error)
{
  ActionWidgetsSubParserData *parser_data = (ActionWidgetsSubParserData*)user_data;
  ActionWidgetInfo *item;

  if (!parser_data->response)
    return;

  item = g_new (ActionWidgetInfo, 1);
  item->widget_name = g_strndup (text, text_len);
  item->response_id = parser_data->response;
  parser_data->items = g_slist_prepend (parser_data->items, item);
  parser_data->response = NULL;
}

static const GMarkupParser attributes_parser =
{
  attributes_start_element,
  NULL,
  attributes_text_element,
};

gboolean
gtk_info_bar_buildable_custom_tag_start (GtkBuildable  *buildable,
                                         GtkBuilder    *builder,
                                         GObject       *child,
                                         const gchar   *tagname,
                                         GMarkupParser *parser,
                                         gpointer      *data)
{
  ActionWidgetsSubParserData *parser_data;

  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  if (!child && strcmp (tagname, "action-widgets") == 0)
    {
      parser_data = g_slice_new0 (ActionWidgetsSubParserData);
      parser_data->info_bar = GTK_INFO_BAR (buildable);
      parser_data->items = NULL;

      *parser = attributes_parser;
      *data = parser_data;
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
  ActionWidgetsSubParserData *parser_data;
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
  parser_data = (ActionWidgetsSubParserData*)user_data;
  parser_data->items = g_slist_reverse (parser_data->items);

  for (l = parser_data->items; l; l = l->next)
    {
      ActionWidgetInfo *item = l->data;

      object = gtk_builder_get_object (builder, item->widget_name);
      if (!object)
        {
          g_warning ("Unknown object %s specified in action-widgets of %s",
                     item->widget_name,
                     gtk_buildable_get_name (GTK_BUILDABLE (buildable)));
          continue;
        }

      ad = get_response_data (GTK_WIDGET (object), TRUE);
      ad->response_id = atoi (item->response_id);

      if (GTK_IS_BUTTON (object))
        signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
      else
        signal_id = GTK_WIDGET_GET_CLASS (object)->activate_signal;

      if (signal_id)
        {
          GClosure *closure;

          closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                           G_OBJECT (info_bar));
          g_signal_connect_closure_by_id (object,
                                          signal_id,
                                          0,
                                          closure,
                                          FALSE);
        }

      if (ad->response_id == GTK_RESPONSE_HELP)
        gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (info_bar->priv->action_area),
                                            GTK_WIDGET (object), TRUE);

      g_free (item->widget_name);
      g_free (item->response_id);
      g_free (item);
    }
  g_slist_free (parser_data->items);
  g_slice_free (ActionWidgetsSubParserData, parser_data);
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
          GtkStockItem item;
          const char *stock_id = NULL;

          atk_object_set_role (atk_obj, ATK_ROLE_ALERT);

          switch (message_type)
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

            case GTK_MESSAGE_OTHER:
              break;

            default:
              g_warning ("Unknown GtkMessageType %u", message_type);
              break;
            }

          if (stock_id)
            {
              gtk_stock_lookup (stock_id, &item);
              atk_object_set_name (atk_obj, item.label);
            }
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
