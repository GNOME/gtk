/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationwindowprivate.h"

#include "gtkapplicationprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkpopovermenubar.h"
#include "gtksettings.h"
#include "deprecated/gtkshortcutswindowprivate.h"
#include "gtktooltipprivate.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

#include <glib/gi18n-lib.h>

/**
 * GtkApplicationWindow:
 *
 * A `GtkWindow` subclass that integrates with `GtkApplication`.
 *
 * Notably, `GtkApplicationWindow` can handle an application menubar.
 *
 * This class implements the [iface@Gio.ActionGroup] and [iface@Gio.ActionMap]
 * interfaces, to let you add window-specific actions that will be exported
 * by the associated [class@Gtk.Application], together with its application-wide
 * actions. Window-specific actions are prefixed with the “win.”
 * prefix and application-wide actions are prefixed with the “app.”
 * prefix. Actions must be addressed with the prefixed name when
 * referring to them from a menu model.
 *
 * Note that widgets that are placed inside a `GtkApplicationWindow`
 * can also activate these actions, if they implement the
 * [iface@Gtk.Actionable] interface.
 *
 * The settings [property@Gtk.Settings:gtk-shell-shows-app-menu] and
 * [property@Gtk.Settings:gtk-shell-shows-menubar] tell GTK whether the
 * desktop environment is showing the application menu and menubar
 * models outside the application as part of the desktop shell.
 * For instance, on OS X, both menus will be displayed remotely;
 * on Windows neither will be.
 *
 * If the desktop environment does not display the menubar, it can be shown in
 * the `GtkApplicationWindow` by setting the
 * [property@Gtk.ApplicationWindow:show-menubar] property to true. If the
 * desktop environment does not display the application menu, then it will
 * automatically be included in the menubar or in the window’s client-side
 * decorations.
 *
 * See [class@Gtk.PopoverMenu] for information about the XML language
 * used by `GtkBuilder` for menu models.
 *
 * See also: [method@Gtk.Application.set_menubar].
 *
 * ## A GtkApplicationWindow with a menubar
 *
 * The code sample below shows how to set up a `GtkApplicationWindow`
 * with a menu bar defined on the [class@Gtk.Application]:
 *
 * ```c
 * GtkApplication *app = gtk_application_new ("org.gtk.test", 0);
 *
 * GtkBuilder *builder = gtk_builder_new_from_string (
 *     "<interface>"
 *     "  <menu id='menubar'>"
 *     "    <submenu>"
 *     "      <attribute name='label' translatable='yes'>_Edit</attribute>"
 *     "      <item>"
 *     "        <attribute name='label' translatable='yes'>_Copy</attribute>"
 *     "        <attribute name='action'>win.copy</attribute>"
 *     "      </item>"
 *     "      <item>"
 *     "        <attribute name='label' translatable='yes'>_Paste</attribute>"
 *     "        <attribute name='action'>win.paste</attribute>"
 *     "      </item>"
 *     "    </submenu>"
 *     "  </menu>"
 *     "</interface>",
 *     -1);
 *
 * GMenuModel *menubar = G_MENU_MODEL (gtk_builder_get_object (builder, "menubar"));
 * gtk_application_set_menubar (GTK_APPLICATION (app), menubar);
 * g_object_unref (builder);
 *
 * // ...
 *
 * GtkWidget *window = gtk_application_window_new (app);
 * ```
 */

typedef GSimpleActionGroupClass GtkApplicationWindowActionsClass;
typedef struct
{
  GSimpleActionGroup parent_instance;
  GtkWindow *window;
} GtkApplicationWindowActions;

static GType gtk_application_window_actions_get_type   (void);
static void  gtk_application_window_actions_iface_init (GRemoteActionGroupInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkApplicationWindowActions, gtk_application_window_actions, G_TYPE_SIMPLE_ACTION_GROUP,
                         G_IMPLEMENT_INTERFACE (G_TYPE_REMOTE_ACTION_GROUP, gtk_application_window_actions_iface_init))

static void
gtk_application_window_actions_activate_action_full (GRemoteActionGroup *remote,
                                                     const char         *action_name,
                                                     GVariant           *parameter,
                                                     GVariant           *platform_data)
{
  GtkApplicationWindowActions *actions = (GtkApplicationWindowActions *) remote;
  GApplication *application;
  GApplicationClass *class;

  application = G_APPLICATION (gtk_window_get_application (actions->window));
  class = G_APPLICATION_GET_CLASS (application);

  class->before_emit (application, platform_data);
  g_action_group_activate_action (G_ACTION_GROUP (actions), action_name, parameter);
  class->after_emit (application, platform_data);
}

static void
gtk_application_window_actions_change_action_state_full (GRemoteActionGroup *remote,
                                                         const char         *action_name,
                                                         GVariant           *value,
                                                         GVariant           *platform_data)
{
  GtkApplicationWindowActions *actions = (GtkApplicationWindowActions *) remote;
  GApplication *application;
  GApplicationClass *class;

  application = G_APPLICATION (gtk_window_get_application (actions->window));
  class = G_APPLICATION_GET_CLASS (application);

  class->before_emit (application, platform_data);
  g_action_group_change_action_state (G_ACTION_GROUP (actions), action_name, value);
  class->after_emit (application, platform_data);
}

static void
gtk_application_window_actions_init (GtkApplicationWindowActions *actions)
{
}

static void
gtk_application_window_actions_iface_init (GRemoteActionGroupInterface *iface)
{
  iface->activate_action_full = gtk_application_window_actions_activate_action_full;
  iface->change_action_state_full = gtk_application_window_actions_change_action_state_full;
}

static void
gtk_application_window_actions_class_init (GtkApplicationWindowActionsClass *class)
{
}

static GSimpleActionGroup *
gtk_application_window_actions_new (GtkApplicationWindow *window)
{
  GtkApplicationWindowActions *actions;

  actions = g_object_new (gtk_application_window_actions_get_type (), NULL);
  actions->window = GTK_WINDOW (window);

  return G_SIMPLE_ACTION_GROUP (actions);
}

/* Now onto GtkApplicationWindow... */

typedef struct _GtkApplicationWindowPrivate GtkApplicationWindowPrivate;
struct _GtkApplicationWindowPrivate
{
  GSimpleActionGroup *actions;
  GtkWidget *menubar;

  gboolean show_menubar;
  guint id;
  GMenu *menubar_section;

  GtkShortcutsWindow *help_overlay;
};

static void gtk_application_window_group_iface_init (GActionGroupInterface *iface);
static void gtk_application_window_map_iface_init (GActionMapInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkApplicationWindow, gtk_application_window, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (GtkApplicationWindow)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gtk_application_window_group_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, gtk_application_window_map_iface_init))

static void
gtk_application_window_update_menubar (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  gboolean should_have_menubar;
  gboolean have_menubar;

  have_menubar = priv->menubar != NULL;

  should_have_menubar = priv->show_menubar &&
                        g_menu_model_get_n_items (G_MENU_MODEL (priv->menubar_section));

  if (have_menubar && !should_have_menubar)
    {
      gtk_widget_unparent (priv->menubar);
      priv->menubar = NULL;
    }

  if (!have_menubar && should_have_menubar)
    {
      GMenu *combined;

      combined = g_menu_new ();
      g_menu_append_section (combined, NULL, G_MENU_MODEL (priv->menubar_section));

      priv->menubar = gtk_popover_menu_bar_new_from_model (G_MENU_MODEL (combined));
      gtk_accessible_update_property (GTK_ACCESSIBLE (priv->menubar),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, _("Menu bar"),
                                      -1);
      gtk_widget_set_parent (priv->menubar, GTK_WIDGET (window));
      g_object_unref (combined);
    }
}

static void
gtk_application_window_update_shell_shows_menubar (GtkApplicationWindow *window,
                                                   GtkSettings          *settings)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  gboolean shown_by_shell;

  g_object_get (settings, "gtk-shell-shows-menubar", &shown_by_shell, NULL);

  if (shown_by_shell)
    {
      /* the shell shows it, so don't show it locally */
      if (g_menu_model_get_n_items (G_MENU_MODEL (priv->menubar_section)) != 0)
        g_menu_remove (priv->menubar_section, 0);
    }
  else
    {
      /* the shell does not show it, so make sure we show it */
      if (g_menu_model_get_n_items (G_MENU_MODEL (priv->menubar_section)) == 0)
        {
          GMenuModel *menubar = NULL;

          if (gtk_window_get_application (GTK_WINDOW (window)) != NULL)
            menubar = gtk_application_get_menubar (gtk_window_get_application (GTK_WINDOW (window)));

          if (menubar != NULL)
            g_menu_append_section (priv->menubar_section, NULL, menubar);
        }
    }
}

static void
gtk_application_window_shell_shows_menubar_changed (GObject    *object,
                                                    GParamSpec *pspec,
                                                    gpointer    user_data)
{
  GtkApplicationWindow *window = user_data;

  gtk_application_window_update_shell_shows_menubar (window, GTK_SETTINGS (object));
  gtk_application_window_update_menubar (window);
}

static char **
gtk_application_window_list_actions (GActionGroup *group)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  /* may be NULL after dispose has run */
  if (!priv->actions)
    return g_new0 (char *, 0 + 1);

  return g_action_group_list_actions (G_ACTION_GROUP (priv->actions));
}

static gboolean
gtk_application_window_query_action (GActionGroup        *group,
                                     const char          *action_name,
                                     gboolean            *enabled,
                                     const GVariantType **parameter_type,
                                     const GVariantType **state_type,
                                     GVariant           **state_hint,
                                     GVariant           **state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return FALSE;

  return g_action_group_query_action (G_ACTION_GROUP (priv->actions),
                                      action_name, enabled, parameter_type, state_type, state_hint, state);
}

static void
gtk_application_window_activate_action (GActionGroup *group,
                                        const char   *action_name,
                                        GVariant     *parameter)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_group_activate_action (G_ACTION_GROUP (priv->actions), action_name, parameter);
}

static void
gtk_application_window_change_action_state (GActionGroup *group,
                                            const char   *action_name,
                                            GVariant     *state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_group_change_action_state (G_ACTION_GROUP (priv->actions), action_name, state);
}

static GAction *
gtk_application_window_lookup_action (GActionMap  *action_map,
                                      const char *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return NULL;

  return g_action_map_lookup_action (G_ACTION_MAP (priv->actions), action_name);
}

static void
gtk_application_window_add_action (GActionMap *action_map,
                                   GAction    *action)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_map_add_action (G_ACTION_MAP (priv->actions), action);
}

static void
gtk_application_window_remove_action (GActionMap  *action_map,
                                      const char *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (!priv->actions)
    return;

  g_action_map_remove_action (G_ACTION_MAP (priv->actions), action_name);
}

static void
gtk_application_window_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = gtk_application_window_list_actions;
  iface->query_action = gtk_application_window_query_action;
  iface->activate_action = gtk_application_window_activate_action;
  iface->change_action_state = gtk_application_window_change_action_state;
}

static void
gtk_application_window_map_iface_init (GActionMapInterface *iface)
{
  iface->lookup_action = gtk_application_window_lookup_action;
  iface->add_action = gtk_application_window_add_action;
  iface->remove_action = gtk_application_window_remove_action;
}

enum {
  PROP_0,
  PROP_SHOW_MENUBAR,
  N_PROPS
};
static GParamSpec *gtk_application_window_properties[N_PROPS];

enum {
  SAVE_STATE,
  LAST_SIGNAL
};
static guint gtk_application_window_signals[LAST_SIGNAL] = { 0 };

static void
gtk_application_window_measure (GtkWidget      *widget,
                                GtkOrientation  orientation,
                                int             for_size,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->measure (widget,
                                                                   orientation,
                                                                   for_size,
                                                                   minimum, natural,
                                                                   minimum_baseline, natural_baseline);

  if (priv->menubar != NULL)
    {
      int menubar_min, menubar_nat;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          int menubar_height = 0;

          gtk_widget_measure (priv->menubar, GTK_ORIENTATION_VERTICAL,
                              -1, &menubar_height, NULL, NULL, NULL);


          GTK_WIDGET_CLASS (gtk_application_window_parent_class)->measure (widget,
                                                                           orientation,
                                                                           for_size > -1 ?
                                                                             for_size - menubar_height : -1,
                                                                           minimum, natural,
                                                                           minimum_baseline, natural_baseline);


          gtk_widget_measure (priv->menubar, orientation, menubar_height, &menubar_min, &menubar_nat, NULL, NULL);

          *minimum = MAX (*minimum, menubar_min);
          *natural = MAX (*natural, menubar_nat);

        }
      else /* VERTICAL */
        {
          gtk_widget_measure (priv->menubar, orientation, for_size, &menubar_min, &menubar_nat, NULL, NULL);
          *minimum += menubar_min;
          *natural += menubar_nat;
        }
    }
}

static void
gtk_application_window_real_size_allocate (GtkWidget *widget,
                                           int        width,
                                           int        height,
                                           int        baseline)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (priv->menubar != NULL)
    {
      GtkAllocation menubar_allocation;
      GtkAllocation child_allocation;
      int menubar_height;
      GtkWidget *child;

      _gtk_window_set_allocation (GTK_WINDOW (widget), width, height, &child_allocation);
      menubar_allocation = child_allocation;

      gtk_widget_measure (priv->menubar, GTK_ORIENTATION_VERTICAL,
                          menubar_allocation.width,
                          &menubar_height, NULL, NULL, NULL);

      menubar_allocation.height = menubar_height;
      gtk_widget_size_allocate  (priv->menubar, &menubar_allocation, -1);

      child_allocation.y += menubar_height;
      child_allocation.height -= menubar_height;
      if (baseline != -1)
        baseline -= child_allocation.y;
      child = gtk_window_get_child (GTK_WINDOW (window));
      if (child != NULL && gtk_widget_get_visible (child))
        gtk_widget_size_allocate (child, &child_allocation, baseline);

      gtk_tooltip_maybe_allocate (GTK_NATIVE (widget));
    }
  else
    GTK_WIDGET_CLASS (gtk_application_window_parent_class)->size_allocate (widget,
                                                                           width,
                                                                           height,
                                                                           baseline);
}

static void
gtk_application_window_real_realize (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_connect (settings, "notify::gtk-shell-shows-menubar",
                    G_CALLBACK (gtk_application_window_shell_shows_menubar_changed), window);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->realize (widget);

  gtk_application_window_update_shell_shows_menubar (window, settings);
  gtk_application_window_update_menubar (window);
}

static void
gtk_application_window_real_unrealize (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, gtk_application_window_shell_shows_menubar_changed, widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->unrealize (widget);
}

GActionGroup *
gtk_application_window_get_action_group (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  return G_ACTION_GROUP (priv->actions);
}

static void
gtk_application_window_real_map (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  /* XXX could eliminate this by tweaking gtk_window_map */
  if (priv->menubar)
    gtk_widget_map (priv->menubar);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->map (widget);
}

static void
gtk_application_window_real_unmap (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  /* XXX could eliminate this by tweaking gtk_window_unmap */
  if (priv->menubar)
    gtk_widget_unmap (priv->menubar);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)->unmap (widget);
}

static void
gtk_application_window_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  switch (prop_id)
    {
    case PROP_SHOW_MENUBAR:
      g_value_set_boolean (value, priv->show_menubar);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_window_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SHOW_MENUBAR:
      gtk_application_window_set_show_menubar (window, g_value_get_boolean (value));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_window_dispose (GObject *object)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (priv->menubar)
    {
      gtk_widget_unparent (priv->menubar);
      priv->menubar = NULL;
    }

  g_clear_object (&priv->menubar_section);

  if (priv->help_overlay)
    {
      gtk_window_destroy (GTK_WINDOW (priv->help_overlay));
      g_clear_object (&priv->help_overlay);
    }

  G_OBJECT_CLASS (gtk_application_window_parent_class)->dispose (object);

  /* We do this below the chain-up above to give us a chance to be
   * removed from the GtkApplication (which is done in the dispose
   * handler of GtkWindow).
   *
   * That reduces our chances of being watched as a GActionGroup from a
   * muxer constructed by GtkApplication.
   */
  g_clear_object (&priv->actions);
}

static void
gtk_application_window_init (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  priv->actions = gtk_application_window_actions_new (window);
  priv->menubar_section = g_menu_new ();

  gtk_widget_insert_action_group (GTK_WIDGET (window), "win", G_ACTION_GROUP (priv->actions));

  /* priv->actions is the one and only ref on the group, so when
   * we dispose, the action group will die, disconnecting all signals.
   */
  g_signal_connect_swapped (priv->actions, "action-added",
                            G_CALLBACK (g_action_group_action_added), window);
  g_signal_connect_swapped (priv->actions, "action-enabled-changed",
                            G_CALLBACK (g_action_group_action_enabled_changed), window);
  g_signal_connect_swapped (priv->actions, "action-state-changed",
                            G_CALLBACK (g_action_group_action_state_changed), window);
  g_signal_connect_swapped (priv->actions, "action-removed",
                            G_CALLBACK (g_action_group_action_removed), window);
}

static void
gtk_application_window_keys_changed (GtkWindow *window)
{
  GtkApplicationWindow *self = GTK_APPLICATION_WINDOW (window);
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (self);

  GTK_WINDOW_CLASS (gtk_application_window_parent_class)->keys_changed (window);

  /* Notify key changes on the help overlay */
  if (priv->help_overlay != NULL)
    _gtk_window_notify_keys_changed (GTK_WINDOW (priv->help_overlay));
}

static void
gtk_application_window_class_init (GtkApplicationWindowClass *class)
{
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  window_class->keys_changed = gtk_application_window_keys_changed;

  widget_class->measure = gtk_application_window_measure;
  widget_class->size_allocate = gtk_application_window_real_size_allocate;
  widget_class->realize = gtk_application_window_real_realize;
  widget_class->unrealize = gtk_application_window_real_unrealize;
  widget_class->map = gtk_application_window_real_map;
  widget_class->unmap = gtk_application_window_real_unmap;

  object_class->get_property = gtk_application_window_get_property;
  object_class->set_property = gtk_application_window_set_property;
  object_class->dispose = gtk_application_window_dispose;

  /**
   * GtkApplicationWindow:show-menubar:
   *
   * If this property is true, the window will display a menubar
   * unless it is shown by the desktop shell.
   *
   * See [method@Gtk.Application.set_menubar].
   *
   * If false, the window will not display a menubar, regardless
   * of whether the desktop shell is showing it or not.
   */
  gtk_application_window_properties[PROP_SHOW_MENUBAR] =
    g_param_spec_boolean ("show-menubar", NULL, NULL,
                          FALSE, G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, gtk_application_window_properties);

  /**
   * GtkApplicationWindow::save-state:
   * @window: the window on which the signal is emitted
   * @dict: a dictionary of type `a{sv}`
   *
   * The handler for this signal should persist any
   * application-specific state of @window into @dict.
   *
   * Note that window management state such as maximized,
   * fullscreen, or window size should not be saved as
   * part of this, they are handled by GTK.
   *
   * See [signal@Gtk.Application::restore-window].
   *
   * Returns: true to stop stop further handlers from running
   *
   * Since: 4.22
   */
  gtk_application_window_signals[SAVE_STATE] =
    g_signal_new (I_("save-state"),
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkApplicationWindowClass, save_state),
                  _gtk_boolean_handled_accumulator, NULL,
                  NULL,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_VARIANT_DICT);
}

/**
 * gtk_application_window_new:
 * @application: an application
 *
 * Creates a new `GtkApplicationWindow`.
 *
 * Returns: a newly created `GtkApplicationWindow`
 */
GtkWidget *
gtk_application_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                       "application", application,
                       NULL);
}

/**
 * gtk_application_window_get_show_menubar:
 * @window: an application window
 *
 * Returns whether the window will display a menubar for the app menu
 * and menubar as needed.
 *
 * Returns: True if the window will display a menubar when needed
 */
gboolean
gtk_application_window_get_show_menubar (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  return priv->show_menubar;
}

/**
 * gtk_application_window_set_show_menubar:
 * @window: an application window
 * @show_menubar: whether to show a menubar when needed
 *
 * Sets whether the window will display a menubar for the app menu
 * and menubar as needed.
 */
void
gtk_application_window_set_show_menubar (GtkApplicationWindow *window,
                                         gboolean              show_menubar)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));

  show_menubar = !!show_menubar;

  if (priv->show_menubar != show_menubar)
    {
      priv->show_menubar = show_menubar;

      gtk_application_window_update_menubar (window);

      g_object_notify_by_pspec (G_OBJECT (window), gtk_application_window_properties[PROP_SHOW_MENUBAR]);
    }
}

/**
 * gtk_application_window_get_id:
 * @window: an application window
 *
 * Returns the unique ID of the window.
 *
 *  If the window has not yet been added to a `GtkApplication`, returns `0`.
 *
 * Returns: the unique ID for the window, or `0` if the window
 *   has not yet been added to an application
 */
guint
gtk_application_window_get_id (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_val_if_fail (GTK_IS_APPLICATION_WINDOW (window), 0);

  return priv->id;
}

void
gtk_application_window_set_id (GtkApplicationWindow *window,
                               guint                 id)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));
  priv->id = id;
}

static void
show_help_overlay (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GtkApplicationWindow *window = user_data;
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);

  if (priv->help_overlay)
    gtk_window_present (GTK_WINDOW (priv->help_overlay));
}

/**
 * gtk_application_window_set_help_overlay:
 * @window: an application window
 * @help_overlay: (nullable): a shortcuts window
 *
 * Associates a shortcuts window with the application window.
 *
 * Additionally, sets up an action with the name
 * `win.show-help-overlay` to present it.
 *
 * The window takes responsibility for destroying the help overlay.
 *
 * Deprecated: 4.18: `GtkShortcutsWindow` will be removed in GTK 5
 */
void
gtk_application_window_set_help_overlay (GtkApplicationWindow *window,
                                         GtkShortcutsWindow   *help_overlay)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));
  g_return_if_fail (help_overlay == NULL || GTK_IS_SHORTCUTS_WINDOW (help_overlay));

  if (priv->help_overlay)
    gtk_window_destroy (GTK_WINDOW (priv->help_overlay));
  g_set_object (&priv->help_overlay, help_overlay);

  if (!priv->help_overlay)
    return;

  gtk_window_set_modal (GTK_WINDOW (help_overlay), TRUE);
  gtk_window_set_hide_on_close (GTK_WINDOW (help_overlay), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (help_overlay), GTK_WINDOW (window));
  gtk_shortcuts_window_set_window (help_overlay, GTK_WINDOW (window));

  if (!g_action_map_lookup_action (G_ACTION_MAP (priv->actions), "show-help-overlay"))
    {
      GSimpleAction *action;

      action = g_simple_action_new ("show-help-overlay", NULL);
      g_signal_connect (action, "activate", G_CALLBACK (show_help_overlay), window);

      g_action_map_add_action (G_ACTION_MAP (priv->actions), G_ACTION (action));
      g_object_unref (G_OBJECT (action));
    }
}

/**
 * gtk_application_window_get_help_overlay:
 * @window: an application window
 *
 * Gets the `GtkShortcutsWindow` that is associated with @window.
 *
 * See [method@Gtk.ApplicationWindow.set_help_overlay].
 *
 * Returns: (transfer none) (nullable): the help overlay associated
 *   with the window
 *
 * Deprecated: 4.18: `GtkShortcutsWindow` will be removed in GTK 5
 */
GtkShortcutsWindow *
gtk_application_window_get_help_overlay (GtkApplicationWindow *window)
{
  GtkApplicationWindowPrivate *priv = gtk_application_window_get_instance_private (window);
  g_return_val_if_fail (GTK_IS_APPLICATION_WINDOW (window), NULL);

  return priv->help_overlay;
}

/*< private >
 * gtk_application_window_save:
 * @window: a `GtkApplicationWindow`
 * @state: a `GVariantDict` to add state to
 *
 * Save the state of @window and its children to a `GVariant`.
 *
 * See [signal@Gtk.ApplicationWindow::save-state] for how to override
 * what state is saved.
 */
void
gtk_application_window_save (GtkApplicationWindow *window,
                             GVariantDict         *state)
{
  gboolean ret;

  g_signal_emit (window, gtk_application_window_signals[SAVE_STATE], 0, state, &ret);
}
