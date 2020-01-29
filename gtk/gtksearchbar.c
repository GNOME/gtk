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

#include "gtksearchbar.h"

#include "gtkbutton.h"
#include "gtkcenterbox.h"
#include "gtkentryprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkrevealer.h"
#include "gtksearchentryprivate.h"
#include "gtksnapshot.h"
#include "gtkstylecontext.h"

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
 * For keyboard presses to start a search, the search bar must be told
 * of a widget to capture key events from through
 * gtk_search_bar_set_key_capture_widget(). This widget will typically
 * be the top-level window, or a parent container of the search bar. Common
 * shortcuts such as Ctrl+F should be handled as an application action, or
 * through the menu items.
 *
 * You will also need to tell the search bar about which entry you
 * are using as your search entry using gtk_search_bar_connect_entry().
 * The following example shows you how to create a more complex search
 * entry.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * searchbar
 * ╰── revealer
 *     ╰── box
 *          ├── [child]
 *          ╰── [button.close]
 * ]|
 *
 * GtkSearchBar has a main CSS node with name searchbar. It has a child node
 * with name revealer that contains a node with name box. The box node contains both the
 * CSS node of the child widget as well as an optional button node which gets the .close
 * style class applied.
 *
 * ## Creating a search bar
 *
 * [A simple example](https://gitlab.gnome.org/GNOME/gtk/tree/master/examples/search-bar.c)
 */

typedef struct _GtkSearchBarClass   GtkSearchBarClass;

struct _GtkSearchBar
{
  GtkBin parent;
};

struct _GtkSearchBarClass
{
  GtkBinClass parent_class;
};

typedef struct {
  GtkWidget   *revealer;
  GtkWidget   *box_center;
  GtkWidget   *close_button;

  GtkWidget   *entry;
  gboolean     reveal_child;

  GtkWidget   *capture_widget;
  GtkEventController *capture_widget_controller;
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

static void
reveal_child_changed_cb (GObject      *object,
                         GParamSpec   *pspec,
                         GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gboolean reveal_child;

  g_object_get (object, "reveal-child", &reveal_child, NULL);

  if (reveal_child == priv->reveal_child)
    return;

  priv->reveal_child = reveal_child;

  if (priv->entry)
    {
      if (reveal_child && GTK_IS_ENTRY (priv->entry))
        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (priv->entry));
      else if (GTK_IS_SEARCH_ENTRY (priv->entry))
        gtk_widget_grab_focus (priv->entry);
      else
        gtk_editable_set_text (GTK_EDITABLE (priv->entry), "");
    }

  g_object_notify_by_pspec (G_OBJECT (bar), widget_props[PROP_SEARCH_MODE_ENABLED]);
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

  gtk_center_box_set_center_widget (GTK_CENTER_BOX (priv->box_center), child);
  /* If an entry is the only child, save the developer a couple of
   * lines of code
   */
  if (GTK_IS_EDITABLE (child))
    gtk_search_bar_connect_entry (bar, GTK_EDITABLE (child));

  _gtk_bin_set_child (GTK_BIN (container), child);
}

static void
gtk_search_bar_remove (GtkContainer *container,
                       GtkWidget    *child)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (container);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  if (GTK_IS_EDITABLE (child))
    gtk_search_bar_connect_entry (bar, NULL);

  gtk_center_box_set_center_widget (GTK_CENTER_BOX (priv->box_center), NULL);
  _gtk_bin_set_child (GTK_BIN (container), NULL);
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

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      g_value_set_boolean (value, gtk_search_bar_get_search_mode (bar));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_search_bar_get_show_close_button (bar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_search_bar_set_entry (GtkSearchBar *bar,
                                      GtkEditable  *editable);

static void
gtk_search_bar_dispose (GObject *object)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  if (gtk_bin_get_child (GTK_BIN (bar)) != NULL)
    {
      gtk_center_box_set_center_widget (GTK_CENTER_BOX (priv->box_center), NULL);
      _gtk_bin_set_child (GTK_BIN (bar), NULL);
    }

  g_clear_pointer (&priv->revealer, gtk_widget_unparent);

  gtk_search_bar_set_entry (bar, NULL);
  gtk_search_bar_set_key_capture_widget (bar, NULL);

  G_OBJECT_CLASS (gtk_search_bar_parent_class)->dispose (object);
}

static void
gtk_search_bar_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (widget);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  gtk_widget_measure (priv->revealer, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_search_bar_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (widget);
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  gtk_widget_size_allocate (priv->revealer,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, baseline);
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

  widget_class->measure = gtk_search_bar_measure;
  widget_class->size_allocate = gtk_search_bar_size_allocate;

  container_class->add = gtk_search_bar_add;
  container_class->remove = gtk_search_bar_remove;

  /**
   * GtkSearchBar:search-mode-enabled:
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
   * GtkSearchBar:show-close-button:
   *
   * Whether to show the close button in the search bar.
   */
  widget_props[PROP_SHOW_CLOSE_BUTTON] = g_param_spec_boolean ("show-close-button",
                                                               P_("Show Close Button"),
                                                               P_("Whether to show the close button in the toolbar"),
                                                               FALSE,
                                                               GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROPERTY, widget_props);

  gtk_widget_class_set_css_name (widget_class, I_("searchbar"));
}

static void
gtk_search_bar_init (GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  priv->revealer = gtk_revealer_new ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), FALSE);
  gtk_widget_set_hexpand (priv->revealer, TRUE);
  gtk_widget_set_parent (priv->revealer, GTK_WIDGET (bar));

  priv->box_center = gtk_center_box_new ();
  gtk_widget_set_hexpand (priv->box_center, TRUE);

  priv->close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_widget_add_style_class (priv->close_button, "close");
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (priv->box_center), priv->close_button);
  gtk_widget_hide (priv->close_button);

  gtk_container_add (GTK_CONTAINER (priv->revealer), priv->box_center);


  g_signal_connect (priv->revealer, "notify::reveal-child",
                    G_CALLBACK (reveal_child_changed_cb), bar);

  g_signal_connect (priv->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), bar);
}

/**
 * gtk_search_bar_new:
 *
 * Creates a #GtkSearchBar. You will need to tell it about
 * which widget is going to be your text entry using
 * gtk_search_bar_connect_entry().
 *
 * Returns: a new #GtkSearchBar
 */
GtkWidget *
gtk_search_bar_new (void)
{
  return g_object_new (GTK_TYPE_SEARCH_BAR, NULL);
}

static void
gtk_search_bar_set_entry (GtkSearchBar *bar,
                          GtkEditable  *entry)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  if (priv->entry != NULL)
    {
      if (GTK_IS_SEARCH_ENTRY (priv->entry))
        {
          gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (priv->entry), NULL);
          g_signal_handlers_disconnect_by_func (priv->entry, stop_search_cb, bar);
        }
      g_object_remove_weak_pointer (G_OBJECT (priv->entry), (gpointer *) &priv->entry);
    }

  priv->entry = GTK_WIDGET (entry);

  if (priv->entry != NULL)
    {
      g_object_add_weak_pointer (G_OBJECT (priv->entry), (gpointer *) &priv->entry);
      if (GTK_IS_SEARCH_ENTRY (priv->entry))
        {
          g_signal_connect (priv->entry, "stop-search",
                            G_CALLBACK (stop_search_cb), bar);
          gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (priv->entry),
                                                   GTK_WIDGET (bar));
        }

    }
}

/**
 * gtk_search_bar_connect_entry:
 * @bar: a #GtkSearchBar
 * @entry: a #GtkEditable
 *
 * Connects the #GtkEntry widget passed as the one to be used in
 * this search bar. The entry should be a descendant of the search bar.
 * This is only required if the entry isn’t the direct child of the
 * search bar (as in our main example).
 */
void
gtk_search_bar_connect_entry (GtkSearchBar *bar,
                              GtkEditable  *entry)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (entry == NULL || GTK_IS_EDITABLE (entry));

  gtk_search_bar_set_entry (bar, entry);
}

/**
 * gtk_search_bar_get_search_mode:
 * @bar: a #GtkSearchBar
 *
 * Returns whether the search mode is on or off.
 *
 * Returns: whether search mode is toggled on
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
      g_object_notify_by_pspec (G_OBJECT (bar), widget_props[PROP_SHOW_CLOSE_BUTTON]);
    }
}

static void
changed_cb (gboolean *changed)
{
  *changed = TRUE;
}

static gboolean
capture_widget_key_handled (GtkEventControllerKey *controller,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkSearchBar          *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);
  gboolean handled;

  if (!gtk_widget_get_mapped (GTK_WIDGET (bar)))
    return GDK_EVENT_PROPAGATE;

  if (priv->reveal_child)
    return GDK_EVENT_PROPAGATE;

  if (priv->entry == NULL)
    {
      g_warning ("The search bar does not have an entry connected to it. Call gtk_search_bar_connect_entry() to connect one.");
      return GDK_EVENT_PROPAGATE;
    }

  if (GTK_IS_SEARCH_ENTRY (priv->entry))
    {
      /* The search entry was told to listen to events from the search bar, so
       * just forward the event to self, so the search entry has an opportunity
       * to intercept those.
       */
      handled = gtk_event_controller_key_forward (controller, GTK_WIDGET (bar));
    }
  else
    {
      gboolean preedit_changed, buffer_changed;
      guint preedit_change_id, buffer_change_id;
      gboolean res;

      if (gtk_search_entry_is_keynav (keyval, state) ||
          keyval == GDK_KEY_space ||
          keyval == GDK_KEY_Menu)
        return GDK_EVENT_PROPAGATE;

      if (keyval == GDK_KEY_Escape)
        {
          if (gtk_revealer_get_reveal_child (GTK_REVEALER (priv->revealer)))
            {
              stop_search_cb (priv->entry, bar);
              return GDK_EVENT_STOP;
            }

          return GDK_EVENT_PROPAGATE;
        }

      handled = GDK_EVENT_PROPAGATE;
      preedit_changed = buffer_changed = FALSE;
      preedit_change_id = g_signal_connect_swapped (priv->entry, "preedit-changed",
                                                    G_CALLBACK (changed_cb), &preedit_changed);
      buffer_change_id = g_signal_connect_swapped (priv->entry, "changed",
                                                   G_CALLBACK (changed_cb), &buffer_changed);

      res = gtk_event_controller_key_forward (controller, priv->entry);

      g_signal_handler_disconnect (priv->entry, preedit_change_id);
      g_signal_handler_disconnect (priv->entry, buffer_change_id);

      if ((res && buffer_changed) || preedit_changed)
        handled = GDK_EVENT_STOP;
    }

  if (handled == GDK_EVENT_STOP)
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);

  return handled;
}

/**
 * gtk_search_bar_set_key_capture_widget:
 * @bar: a #GtkSearchBar
 * @widget: (nullable) (transfer none): a #GtkWidget
 *
 * Sets @widget as the widget that @bar will capture key events from.
 *
 * If key events are handled by the search bar, the bar will
 * be shown, and the entry populated with the entered text.
 **/
void
gtk_search_bar_set_key_capture_widget (GtkSearchBar *bar,
				       GtkWidget    *widget)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  if (priv->capture_widget == widget)
    return;

  if (priv->capture_widget)
    {
      gtk_widget_remove_controller (priv->capture_widget,
                                    priv->capture_widget_controller);
      g_object_remove_weak_pointer (G_OBJECT (priv->capture_widget),
                                    (gpointer *) &priv->capture_widget);
    }

  priv->capture_widget = widget;

  if (widget)
    {
      g_object_add_weak_pointer (G_OBJECT (priv->capture_widget),
                                 (gpointer *) &priv->capture_widget);

      priv->capture_widget_controller = gtk_event_controller_key_new ();
      gtk_event_controller_set_propagation_phase (priv->capture_widget_controller,
                                                  GTK_PHASE_CAPTURE);
      g_signal_connect (priv->capture_widget_controller, "key-pressed",
                        G_CALLBACK (capture_widget_key_handled), bar);
      g_signal_connect (priv->capture_widget_controller, "key-released",
                        G_CALLBACK (capture_widget_key_handled), bar);
      gtk_widget_add_controller (widget, priv->capture_widget_controller);
    }
}

/**
 * gtk_search_bar_get_key_capture_widget:
 * @bar: a #GtkSearchBar
 *
 * Gets the widget that @bar is capturing key events from.
 *
 * Returns: (transfer none): The key capture widget.
 **/
GtkWidget *
gtk_search_bar_get_key_capture_widget (GtkSearchBar *bar)
{
  GtkSearchBarPrivate *priv = gtk_search_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), NULL);

  return priv->capture_widget;
}
