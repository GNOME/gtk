/* GTK - The GIMP Toolkit
 * gtklinkbutton.c - a hyperlink-enabled button
 *
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gmail.com>
 * All rights reserved.
 *
 * Based on gnome-href code by:
 *      James Henstridge <james@daa.com.au>
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

/**
 * SECTION:gtklinkbutton
 * @Title: GtkLinkButton
 * @Short_description: Create buttons bound to a URL
 * @See_also: #GtkButton
 *
 * A GtkLinkButton is a #GtkButton with a hyperlink, similar to the one
 * used by web browsers, which triggers an action when clicked. It is useful
 * to show quick links to resources.
 *
 * A link button is created by calling either gtk_link_button_new() or
 * gtk_link_button_new_with_label(). If using the former, the URI you pass
 * to the constructor is used as a label for the widget.
 *
 * The URI bound to a GtkLinkButton can be set specifically using
 * gtk_link_button_set_uri(), and retrieved using gtk_link_button_get_uri().
 *
 * By default, GtkLinkButton calls gtk_show_uri() when the button is
 * clicked. This behaviour can be overridden by connecting to the
 * #GtkLinkButton::activate-link signal and returning %TRUE from the
 * signal handler.
 *
 * # CSS nodes
 *
 * GtkLinkButton has a single CSS node with name button. To differentiate
 * it from a plain #GtkButton, it gets the .link style class.
 */

#include "config.h"

#include "gtklinkbutton.h"

#include "gtkdragsource.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtkshow.h"
#include "gtksizerequest.h"
#include "gtkstylecontext.h"
#include "gtktooltip.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtklinkbuttonaccessible.h"

#include <string.h>

typedef struct _GtkLinkButtonClass GtkLinkButtonClass;
typedef struct _GtkLinkButtonPrivate GtkLinkButtonPrivate;

struct _GtkLinkButton
{
  /*< private >*/
  GtkButton parent_instance;
};

struct _GtkLinkButtonClass
{
  /*< private >*/
  GtkButtonClass parent_class;

  /*< public >*/
  gboolean (* activate_link) (GtkLinkButton *button);
};

struct _GtkLinkButtonPrivate
{
  gchar *uri;

  gboolean visited;

  GtkWidget *popup_menu;
};

enum
{
  PROP_0,
  PROP_URI,
  PROP_VISITED
};

enum
{
  ACTIVATE_LINK,

  LAST_SIGNAL
};

static void     gtk_link_button_finalize     (GObject          *object);
static void     gtk_link_button_get_property (GObject          *object,
					      guint             prop_id,
					      GValue           *value,
					      GParamSpec       *pspec);
static void     gtk_link_button_set_property (GObject          *object,
					      guint             prop_id,
					      const GValue     *value,
					      GParamSpec       *pspec);
static void     gtk_link_button_clicked      (GtkButton        *button);
static void     gtk_link_button_popup_menu   (GtkWidget        *widget,
                                              const char       *action_name,
                                              GVariant         *parameters);
static gboolean gtk_link_button_query_tooltip_cb (GtkWidget    *widget,
                                                  gint          x,
                                                  gint          y,
                                                  gboolean      keyboard_tip,
                                                  GtkTooltip   *tooltip,
                                                  gpointer      data);
static void gtk_link_button_pressed_cb (GtkGestureClick *gesture,
                                        int                   n_press,
                                        double                x,
                                        double                y,
                                        gpointer              user_data);

static gboolean gtk_link_button_activate_link (GtkLinkButton *link_button);

static const char *link_drop_types[] = {
  "text/uri-list",
  "_NETSCAPE_URL"
};

static guint link_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkLinkButton, gtk_link_button, GTK_TYPE_BUTTON)

static void
gtk_link_button_activate_clipboard_copy (GtkWidget  *widget,
                                         const char *name,
                                         GVariant   *parameter)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (widget);
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), priv->uri);
}

static void
gtk_link_button_class_init (GtkLinkButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  gobject_class->set_property = gtk_link_button_set_property;
  gobject_class->get_property = gtk_link_button_get_property;
  gobject_class->finalize = gtk_link_button_finalize;

  button_class->clicked = gtk_link_button_clicked;

  klass->activate_link = gtk_link_button_activate_link;

  /**
   * GtkLinkButton:uri:
   *
   * The URI bound to this button.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_URI,
                                   g_param_spec_string ("uri",
                                                        P_("URI"),
                                                        P_("The URI bound to this button"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkLinkButton:visited:
   *
   * The 'visited' state of this button. A visited link is drawn in a
   * different color.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VISITED,
                                   g_param_spec_boolean ("visited",
                                                         P_("Visited"),
                                                         P_("Whether this link has been visited."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkLinkButton::activate-link:
   * @button: the #GtkLinkButton that emitted the signal
   *
   * The ::activate-link signal is emitted each time the #GtkLinkButton
   * has been clicked.
   *
   * The default handler will call gtk_show_uri() with the URI stored inside
   * the #GtkLinkButton:uri property.
   *
   * To override the default behavior, you can connect to the ::activate-link
   * signal and stop the propagation of the signal by returning %TRUE from
   * your handler.
   */
  link_signals[ACTIVATE_LINK] =
    g_signal_new (I_("activate-link"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkLinkButtonClass, activate_link),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_LINK_BUTTON_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("button"));

  /**
   * GtkLinkButton|clipboard.copy:
   *
   * Copies the url to the clipboard.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL,
                                   gtk_link_button_activate_clipboard_copy);

  /**
   * GtkLinkButton|menu.popup:
   *
   * Opens the context menu. 
   */
  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, gtk_link_button_popup_menu);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);
}

static GMenuModel *
gtk_link_button_get_menu_model (void)
{
  GMenu *menu, *section;

  menu = g_menu_new ();

  section = g_menu_new ();
  g_menu_append (section, _("_Copy URL"), "clipboard.copy");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  return G_MENU_MODEL (menu);
}

#define GTK_TYPE_LINK_CONTENT            (gtk_link_content_get_type ())
#define GTK_LINK_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LINK_CONTENT, GtkLinkContent))

typedef struct _GtkLinkContent GtkLinkContent;
typedef struct _GtkLinkContentClass GtkLinkContentClass;

struct _GtkLinkContent
{
  GdkContentProvider parent;
  GtkLinkButton *link;
};

struct _GtkLinkContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_link_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkLinkContent, gtk_link_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_link_content_ref_formats (GdkContentProvider *provider)
{
  GtkLinkContent *content = GTK_LINK_CONTENT (provider);

  if (content->link)
    return gdk_content_formats_union (gdk_content_formats_new_for_gtype (G_TYPE_STRING),
                                      gdk_content_formats_new (link_drop_types, G_N_ELEMENTS (link_drop_types)));
  else
    return gdk_content_formats_new (NULL, 0);
}

static gboolean
gtk_link_content_get_value (GdkContentProvider  *provider,
                             GValue              *value,
                             GError             **error)
{
  GtkLinkContent *content = GTK_LINK_CONTENT (provider);

  if (G_VALUE_HOLDS (value, G_TYPE_STRING) &&
      content->link != NULL)
    {
      GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (content->link);
      char *uri;
  
      uri = g_strdup_printf ("%s\r\n", priv->uri);
      g_value_set_string (value, uri);
      g_free (uri);

      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gtk_link_content_parent_class)->get_value (provider, value, error);
}

static void
gtk_link_content_class_init (GtkLinkContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = gtk_link_content_ref_formats;
  provider_class->get_value = gtk_link_content_get_value;
}

static void
gtk_link_content_init (GtkLinkContent *content)
{
}

static void
gtk_link_button_init (GtkLinkButton *link_button)
{
  GtkGesture *gesture;
  GdkContentProvider *content;
  GtkDragSource *source;

  gtk_button_set_has_frame (GTK_BUTTON (link_button), FALSE);
  gtk_widget_set_state_flags (GTK_WIDGET (link_button), GTK_STATE_FLAG_LINK, FALSE);
  gtk_widget_set_has_tooltip (GTK_WIDGET (link_button), TRUE);

  g_signal_connect (link_button, "query-tooltip",
                    G_CALLBACK (gtk_link_button_query_tooltip_cb), NULL);

  source = gtk_drag_source_new ();
  content = g_object_new (GTK_TYPE_LINK_CONTENT, NULL);
  GTK_LINK_CONTENT (content)->link = link_button;
  gtk_drag_source_set_content (source, content);
  g_object_unref (content);
  gtk_widget_add_controller (GTK_WIDGET (link_button), GTK_EVENT_CONTROLLER (source));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (gtk_link_button_pressed_cb),
                    link_button);
  gtk_widget_add_controller (GTK_WIDGET (link_button), GTK_EVENT_CONTROLLER (gesture));

  gtk_widget_add_css_class (GTK_WIDGET (link_button), "link");

  gtk_widget_set_cursor_from_name (GTK_WIDGET (link_button), "pointer");
}

static void
gtk_link_button_finalize (GObject *object)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (object);
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  g_free (priv->uri);

  g_clear_pointer (&priv->popup_menu, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_link_button_parent_class)->finalize (object);
}

static void
gtk_link_button_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (object);
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);
  
  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;
    case PROP_VISITED:
      g_value_set_boolean (value, priv->visited);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_link_button_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (object);
  
  switch (prop_id)
    {
    case PROP_URI:
      gtk_link_button_set_uri (link_button, g_value_get_string (value));
      break;
    case PROP_VISITED:
      gtk_link_button_set_visited (link_button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_link_button_do_popup (GtkLinkButton *link_button,
                          double         x,
                          double         y)
{
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  if (!priv->popup_menu)
    {
      GMenuModel *model;

      model = gtk_link_button_get_menu_model ();
      priv->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_widget_set_parent (priv->popup_menu, GTK_WIDGET (link_button));
      gtk_popover_set_position (GTK_POPOVER (priv->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (priv->popup_menu), FALSE);
      gtk_widget_set_halign (priv->popup_menu, GTK_ALIGN_START);

      g_object_unref (model);
    }

  if (x != -1 && y != -1)
    {
      GdkRectangle rect = { x, y, 1, 1 };
      gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup_menu), &rect);
    }
  else
    gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup_menu), NULL);

  gtk_popover_popup (GTK_POPOVER (priv->popup_menu));
}

static void
gtk_link_button_pressed_cb (GtkGestureClick *gesture,
                            int              n_press,
                            double           x,
                            double           y,
                            gpointer         user_data)
{
  GtkLinkButton *link_button = user_data;
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);
  GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  GdkEvent *event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (!gtk_widget_has_focus (GTK_WIDGET (link_button)))
    gtk_widget_grab_focus (GTK_WIDGET (link_button));

  if (gdk_event_triggers_context_menu (event) &&
      priv->uri != NULL)
    {
      gtk_link_button_do_popup (link_button, x, y);
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    }
}

static gboolean
gtk_link_button_activate_link (GtkLinkButton *link_button)
{
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (link_button)));

  gtk_show_uri (GTK_WINDOW (toplevel), priv->uri, GDK_CURRENT_TIME);
  gtk_link_button_set_visited (link_button, TRUE);

  return TRUE;
}

static void
gtk_link_button_clicked (GtkButton *button)
{
  gboolean retval = FALSE;

  g_signal_emit (button, link_signals[ACTIVATE_LINK], 0, &retval);
}

static void
gtk_link_button_popup_menu (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *parameters)
{
  gtk_link_button_do_popup (GTK_LINK_BUTTON (widget), -1, -1);
}

/**
 * gtk_link_button_new:
 * @uri: a valid URI
 *
 * Creates a new #GtkLinkButton with the URI as its text.
 *
 * Returns: a new link button widget.
 */
GtkWidget *
gtk_link_button_new (const gchar *uri)
{
  gchar *utf8_uri = NULL;
  GtkWidget *retval;
  
  g_return_val_if_fail (uri != NULL, NULL);
  
  if (g_utf8_validate (uri, -1, NULL))
    {
      utf8_uri = g_strdup (uri);
    }
  else
    {
      GError *conv_err = NULL;
    
      utf8_uri = g_locale_to_utf8 (uri, -1, NULL, NULL, &conv_err);
      if (conv_err)
        {
          g_warning ("Attempting to convert URI '%s' to UTF-8, but failed "
                     "with error: %s",
                     uri,
                     conv_err->message);
          g_error_free (conv_err);
        
          utf8_uri = g_strdup (_("Invalid URI"));
        }
    }
  
  retval = g_object_new (GTK_TYPE_LINK_BUTTON,
  			 "label", utf8_uri,
  			 "uri", uri,
  			 NULL);
  
  g_free (utf8_uri);
  
  return retval;
}

/**
 * gtk_link_button_new_with_label:
 * @uri: a valid URI
 * @label: (allow-none): the text of the button
 *
 * Creates a new #GtkLinkButton containing a label.
 *
 * Returns: (transfer none): a new link button widget.
 */
GtkWidget *
gtk_link_button_new_with_label (const gchar *uri,
				const gchar *label)
{
  GtkWidget *retval;
  
  g_return_val_if_fail (uri != NULL, NULL);
  
  if (!label)
    return gtk_link_button_new (uri);

  retval = g_object_new (GTK_TYPE_LINK_BUTTON,
		         "label", label,
			 "uri", uri,
			 NULL);

  return retval;
}

static gboolean 
gtk_link_button_query_tooltip_cb (GtkWidget    *widget,
                                  gint          x,
                                  gint          y,
                                  gboolean      keyboard_tip,
                                  GtkTooltip   *tooltip,
                                  gpointer      data)
{
  GtkLinkButton *link_button = GTK_LINK_BUTTON (widget);
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);
  const gchar *label, *uri;
  const char *text, *markup;

  label = gtk_button_get_label (GTK_BUTTON (link_button));
  uri = priv->uri;
  text = gtk_widget_get_tooltip_text (widget);
  markup = gtk_widget_get_tooltip_markup (widget);

  if (text == NULL &&
      markup == NULL &&
      label && *label != '\0' && uri && strcmp (label, uri) != 0)
    {
      gtk_tooltip_set_text (tooltip, uri);
      return TRUE;
    }

  return FALSE;
}



/**
 * gtk_link_button_set_uri:
 * @link_button: a #GtkLinkButton
 * @uri: a valid URI
 *
 * Sets @uri as the URI where the #GtkLinkButton points. As a side-effect
 * this unsets the “visited” state of the button.
 */
void
gtk_link_button_set_uri (GtkLinkButton *link_button,
			 const gchar   *uri)
{
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  g_return_if_fail (GTK_IS_LINK_BUTTON (link_button));
  g_return_if_fail (uri != NULL);

  g_free (priv->uri);
  priv->uri = g_strdup (uri);

  g_object_notify (G_OBJECT (link_button), "uri");

  gtk_link_button_set_visited (link_button, FALSE);
}

/**
 * gtk_link_button_get_uri:
 * @link_button: a #GtkLinkButton
 *
 * Retrieves the URI set using gtk_link_button_set_uri().
 *
 * Returns: a valid URI.  The returned string is owned by the link button
 *   and should not be modified or freed.
 */
const gchar *
gtk_link_button_get_uri (GtkLinkButton *link_button)
{
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  g_return_val_if_fail (GTK_IS_LINK_BUTTON (link_button), NULL);

  return priv->uri;
}

/**
 * gtk_link_button_set_visited:
 * @link_button: a #GtkLinkButton
 * @visited: the new “visited” state
 *
 * Sets the “visited” state of the URI where the #GtkLinkButton
 * points.  See gtk_link_button_get_visited() for more details.
 */
void
gtk_link_button_set_visited (GtkLinkButton *link_button,
                             gboolean       visited)
{
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  g_return_if_fail (GTK_IS_LINK_BUTTON (link_button));

  visited = visited != FALSE;

  if (priv->visited != visited)
    {
      priv->visited = visited;

      if (visited)
        {
          gtk_widget_unset_state_flags (GTK_WIDGET (link_button), GTK_STATE_FLAG_LINK);
          gtk_widget_set_state_flags (GTK_WIDGET (link_button), GTK_STATE_FLAG_VISITED, FALSE);
        }
      else
        {
          gtk_widget_unset_state_flags (GTK_WIDGET (link_button), GTK_STATE_FLAG_VISITED);
          gtk_widget_set_state_flags (GTK_WIDGET (link_button), GTK_STATE_FLAG_LINK, FALSE);
        }

      g_object_notify (G_OBJECT (link_button), "visited");
    }
}

/**
 * gtk_link_button_get_visited:
 * @link_button: a #GtkLinkButton
 *
 * Retrieves the “visited” state of the URI where the #GtkLinkButton
 * points. The button becomes visited when it is clicked. If the URI
 * is changed on the button, the “visited” state is unset again.
 *
 * The state may also be changed using gtk_link_button_set_visited().
 *
 * Returns: %TRUE if the link has been visited, %FALSE otherwise
 */
gboolean
gtk_link_button_get_visited (GtkLinkButton *link_button)
{
  GtkLinkButtonPrivate *priv = gtk_link_button_get_instance_private (link_button);

  g_return_val_if_fail (GTK_IS_LINK_BUTTON (link_button), FALSE);

  return priv->visited;
}
