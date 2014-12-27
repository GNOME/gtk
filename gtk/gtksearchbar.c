/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
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
 * Modified by the GTK+ Team and others 2013.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkentry.h"
#include "gtkentryprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkrender.h"
#include "gtksearchbar.h"
#include "gtksearchentryprivate.h"

/**
 * SECTION:gtksearchbar
 * @Short_description: A toolbar to integrate a search entry with
 * @Title: GtkSearchBar
 *
 * #GtkSearchBar is a container made to have a search entry (possibly
 * with additional connex widgets, such as drop-down menus, or buttons)
 * built-in. The search bar would appear when a search is started through
 * typing on the keyboard, or the application’s search mode is toggled on.
 *
 * For keyboard presses to start a search, events will need to be
 * forwarded from the top-level window that contains the search bar.
 * See gtk_search_bar_handle_event() for example code. Common shortcuts
 * such as Ctrl+F should be handled as an application action, or through
 * the menu items.
 *
 * You will also need to tell the search bar about which entry you
 * are using as your search entry using gtk_search_bar_connect_entry().
 * The following example shows you how to create a more complex search
 * entry.
 *
 * ## Creating a search bar
 *
 * [A simple example](https://git.gnome.org/browse/gtk+/tree/examples/search-bar.c)
 *
 * Since: 3.10
 */

typedef struct {
  /* Template widgets */
  GtkWidget   *revealer;
  GtkWidget   *tool_box;
  GtkWidget   *box_center;
  GtkWidget   *close_button;

  GtkWidget   *entry;
  gboolean     reveal_child;
} GtkSearchBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSearchBar, gtk_search_bar, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_SEARCH_MODE_ENABLED,
  PROP_SHOW_CLOSE_BUTTON,
  LAST_PROPERTY
};

static GParamSpec *widget_props[LAST_PROPERTY] = { NULL, };

static void
stop_search_cb (GtkWidget    *entry,
                GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), FALSE);
}

static gboolean
entry_key_pressed_event_cb (GtkWidget    *widget,
                            GdkEvent     *event,
                            GtkSearchBar *bar)
{
  if (event->key.keyval == GDK_KEY_Escape)
    {
      stop_search_cb (widget, bar);
      return GDK_EVENT_STOP;
    }
  else
    return GDK_EVENT_PROPAGATE;
}

static void
preedit_changed_cb (GtkEntry  *entry,
                    GtkWidget *popup,
                    gboolean  *preedit_changed)
{
  *preedit_changed = TRUE;
}

static gboolean
gtk_search_bar_handle_event_for_entry (GtkSearchBar *bar,
                                       GdkEvent     *event)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gboolean handled;
  gboolean preedit_changed;
  guint preedit_change_id;
  gboolean res;
  char *old_text, *new_text;

  if (gtk_search_entry_is_keynav_event (event) ||
      event->key.keyval == GDK_KEY_space ||
      event->key.keyval == GDK_KEY_Menu)
    return GDK_EVENT_PROPAGATE;

  if (!gtk_widget_get_realized (priv->entry))
    gtk_widget_realize (priv->entry);

  handled = GDK_EVENT_PROPAGATE;
  preedit_changed = FALSE;
  preedit_change_id = g_signal_connect (priv->entry, "preedit-changed",
                                        G_CALLBACK (preedit_changed_cb), &preedit_changed);

  old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry)));
  res = gtk_widget_event (priv->entry, event);
  new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry)));

  g_signal_handler_disconnect (priv->entry, preedit_change_id);

  if ((res && g_strcmp0 (new_text, old_text) != 0) || preedit_changed)
    handled = GDK_EVENT_STOP;

  g_free (old_text);
  g_free (new_text);

  return handled;
}

/**
 * gtk_search_bar_handle_event:
 * @bar: a #GtkSearchBar
 * @event: a #GdkEvent containing key press events
 *
 * This function should be called when the top-level
 * window which contains the search bar received a key event.
 *
 * If the key event is handled by the search bar, the bar will
 * be shown, the entry populated with the entered text and %GDK_EVENT_STOP
 * will be returned. The caller should ensure that events are
 * not propagated further.
 *
 * If no entry has been connected to the search bar, using
 * gtk_search_bar_connect_entry(), this function will return
 * immediately with a warning.
 *
 * ## Showing the search bar on key presses
 *
 * |[<!-- language="C" -->
 * static gboolean
 * on_key_press_event (GtkWidget *widget,
 *                     GdkEvent  *event,
 *                     gpointer   user_data)
 * {
 *   GtkSearchBar *bar = GTK_SEARCH_BAR (user_data);
 *   return gtk_search_bar_handle_event (bar, event);
 * }
 *
 * g_signal_connect (window,
 *                  "key-press-event",
 *                   G_CALLBACK (on_key_press_event),
 *                   search_bar);
 * ]|
 *
 * Returns: %GDK_EVENT_STOP if the key press event resulted
 *     in text being entered in the search entry (and revealing
 *     the search bar if necessary), %GDK_EVENT_PROPAGATE otherwise.
 *
 * Since: 3.10
 */
gboolean
gtk_search_bar_handle_event (GtkSearchBar *bar,
                             GdkEvent     *event)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gboolean handled;

  if (priv->reveal_child)
    return GDK_EVENT_PROPAGATE;

  if (priv->entry == NULL)
    {
      g_warning ("The search bar does not have an entry connected to it. Call gtk_search_bar_connect_entry() to connect one.");
      return GDK_EVENT_PROPAGATE;
    }

  if (GTK_IS_SEARCH_ENTRY (priv->entry))
    handled = gtk_search_entry_handle_event (GTK_SEARCH_ENTRY (priv->entry), event);
  else
    handled = gtk_search_bar_handle_event_for_entry (bar, event);

  if (handled == GDK_EVENT_STOP)
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);

  return handled;
}

static void
reveal_child_changed_cb (GObject      *object,
                         GParamSpec   *pspec,
                         GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gboolean reveal_child;

  g_object_get (object, "reveal-child", &reveal_child, NULL);
  if (reveal_child)
    gtk_widget_set_child_visible (priv->revealer, TRUE);

  if (reveal_child == priv->reveal_child)
    return;

  priv->reveal_child = reveal_child;

  if (priv->entry)
    {
      if (reveal_child)
        _gtk_entry_grab_focus (GTK_ENTRY (priv->entry), FALSE);
      else
        gtk_entry_set_text (GTK_ENTRY (priv->entry), "");
    }

  g_object_notify (G_OBJECT (bar), "search-mode-enabled");
}

static void
child_revealed_changed_cb (GObject      *object,
                           GParamSpec   *pspec,
                           GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gboolean val;

  g_object_get (object, "child-revealed", &val, NULL);
  if (!val)
    gtk_widget_set_child_visible (priv->revealer, FALSE);
}

static void
close_button_clicked_cb (GtkWidget    *button,
                         GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), FALSE);
}

static void
gtk_search_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (container);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  /* When constructing the widget, we want the revealer to be added
   * as the first child of the search bar, as an implementation detail.
   * After that, the child added by the application should be added
   * to box_center.
   */
  if (priv->box_center == NULL)
    {
      GTK_CONTAINER_CLASS (gtk_search_bar_parent_class)->add (container, child);
    }
  else
    {
      gtk_container_add (GTK_CONTAINER (priv->box_center), child);
      /* If an entry is the only child, save the developer a couple of
       * lines of code
       */
      if (GTK_IS_ENTRY (child))
        gtk_search_bar_connect_entry (bar, GTK_ENTRY (child));
    }
}

static void
gtk_search_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      gtk_search_bar_set_search_mode (bar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      gtk_search_bar_set_show_close_button (bar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      g_value_set_boolean (value, priv->reveal_child);
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_search_bar_get_show_close_button (bar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_bar_dispose (GObject *object)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  if (priv->entry)
    {
      g_signal_handlers_disconnect_by_func (priv->entry, entry_key_pressed_event_cb, bar);
      g_object_remove_weak_pointer (G_OBJECT (priv->entry), (gpointer *) &priv->entry);
      priv->entry = NULL;
    }

  G_OBJECT_CLASS (gtk_search_bar_parent_class)->dispose (object);
}

static gboolean
gtk_search_bar_draw (GtkWidget *widget,
                     cairo_t *cr)
{
  gint width, height;
  GtkStyleContext *context;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  GTK_WIDGET_CLASS (gtk_search_bar_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gtk_search_bar_class_init (GtkSearchBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->dispose = gtk_search_bar_dispose;
  object_class->set_property = gtk_search_bar_set_property;
  object_class->get_property = gtk_search_bar_get_property;
  widget_class->draw = gtk_search_bar_draw;

  container_class->add = gtk_search_bar_add;

  /**
   * GtkEntry:search-mode-enabled:
   *
   * Whether the search mode is on and the search bar shown.
   *
   * See gtk_search_bar_set_search_mode() for details.
   */
  widget_props[PROP_SEARCH_MODE_ENABLED] = g_param_spec_boolean ("search-mode-enabled",
                                                                 P_("Search Mode Enabled"),
                                                                 P_("Whether the search mode is on and the search bar shown"),
                                                                 FALSE,
                                                                 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntry:show-close-button:
   *
   * Whether to show the close button in the toolbar.
   */
  widget_props[PROP_SHOW_CLOSE_BUTTON] = g_param_spec_boolean ("show-close-button",
                                                               P_("Show Close Button"),
                                                               P_("Whether to show the close button in the toolbar"),
                                                               FALSE,
                                                               GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROPERTY, widget_props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtksearchbar.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkSearchBar, tool_box);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkSearchBar, revealer);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkSearchBar, box_center);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkSearchBar, close_button);
}

static void
gtk_search_bar_init (GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  GtkStyleContext *context;

  gtk_widget_init_template (GTK_WIDGET (bar));

  /* We use child-visible to avoid the unexpanded revealer
   * peaking out by 1 pixel
   */
  gtk_widget_set_child_visible (priv->revealer, FALSE);

  g_signal_connect (priv->revealer, "notify::reveal-child",
                    G_CALLBACK (reveal_child_changed_cb), bar);
  g_signal_connect (priv->revealer, "notify::child-revealed",
                    G_CALLBACK (child_revealed_changed_cb), bar);

  gtk_widget_set_no_show_all (priv->close_button, TRUE);
  g_signal_connect (priv->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), bar);

  context = gtk_widget_get_style_context (GTK_WIDGET (bar));
  gtk_style_context_add_class (context, "search-bar");
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);

};

/**
 * gtk_search_bar_new:
 *
 * Creates a #GtkSearchBar. You will need to tell it about
 * which widget is going to be your text entry using
 * gtk_search_bar_connect_entry().
 *
 * Returns: a new #GtkSearchBar
 *
 * Since: 3.10
 */
GtkWidget *
gtk_search_bar_new (void)
{
  return g_object_new (GTK_TYPE_SEARCH_BAR, NULL);
}

/**
 * gtk_search_bar_connect_entry:
 * @bar: a #GtkSearchBar
 * @entry: a #GtkEntry
 *
 * Connects the #GtkEntry widget passed as the one to be used in
 * this search bar. The entry should be a descendant of the search bar.
 * This is only required if the entry isn’t the direct child of the
 * search bar (as in our main example).
 *
 * Since: 3.10
 */
void
gtk_search_bar_connect_entry (GtkSearchBar *bar,
                              GtkEntry     *entry)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (entry == NULL || GTK_IS_ENTRY (entry));

  if (priv->entry != NULL)
    {
      if (GTK_IS_SEARCH_ENTRY (priv->entry))
        g_signal_handlers_disconnect_by_func (priv->entry, stop_search_cb, bar);
      else
        g_signal_handlers_disconnect_by_func (priv->entry, entry_key_pressed_event_cb, bar);
      g_object_remove_weak_pointer (G_OBJECT (priv->entry), (gpointer *) &priv->entry);
      priv->entry = NULL;
    }

  if (entry != NULL)
    {
      priv->entry = GTK_WIDGET (entry);
      g_object_add_weak_pointer (G_OBJECT (priv->entry), (gpointer *) &priv->entry);
      if (GTK_IS_SEARCH_ENTRY (priv->entry))
        g_signal_connect (priv->entry, "stop-search",
                          G_CALLBACK (stop_search_cb), bar);
      else
        g_signal_connect (priv->entry, "key-press-event",
                          G_CALLBACK (entry_key_pressed_event_cb), bar);
    }
}

/**
 * gtk_search_bar_get_search_mode:
 * @bar: a #GtkSearchBar
 *
 * Returns whether the search mode is on or off.
 *
 * Returns: whether search mode is toggled on
 *
 * Since: 3.10
 */
gboolean
gtk_search_bar_get_search_mode (GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), FALSE);

  return priv->reveal_child;
}

/**
 * gtk_search_bar_set_search_mode:
 * @bar: a #GtkSearchBar
 * @search_mode: the new state of the search mode
 *
 * Switches the search mode on or off.
 *
 * Since: 3.10
 */
void
gtk_search_bar_set_search_mode (GtkSearchBar *bar,
                                gboolean      search_mode)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), search_mode);
}

/**
 * gtk_search_bar_get_show_close_button:
 * @bar: a #GtkSearchBar
 *
 * Returns whether the close button is shown.
 *
 * Returns: whether the close button is shown
 *
 * Since: 3.10
 */
gboolean
gtk_search_bar_get_show_close_button (GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), FALSE);

  return gtk_widget_get_visible (priv->close_button);
}

/**
 * gtk_search_bar_set_show_close_button:
 * @bar: a #GtkSearchBar
 * @visible: whether the close button will be shown or not
 *
 * Shows or hides the close button. Applications that
 * already have a “search” toggle button should not show a close
 * button in their search bar, as it duplicates the role of the
 * toggle button.
 *
 * Since: 3.10
 */
void
gtk_search_bar_set_show_close_button (GtkSearchBar *bar,
                                      gboolean      visible)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));

  visible = visible != FALSE;

  if (gtk_widget_get_visible (priv->close_button) != visible)
    {
      gtk_widget_set_visible (priv->close_button, visible);
      g_object_notify (G_OBJECT (bar), "show-close-button");
    }
}
