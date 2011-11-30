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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationwindow.h"

#include "gtkseparatormenuitem.h"
#include "gtkcheckmenuitem.h"
#include "gtkmenubar.h"
#include "gactionmuxer.h"

struct _GtkApplicationWindowPrivate
{
  GSimpleActionGroup *actions;
  GtkMenuBar *menubar;

  gboolean show_app_menu;
};

static gchar **
gtk_application_window_list_actions (GActionGroup *group)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  return g_action_group_list_actions (G_ACTION_GROUP (window->priv->actions));
}

static gboolean
gtk_application_window_query_action (GActionGroup        *group,
                                     const gchar         *action_name,
                                     gboolean            *enabled,
                                     const GVariantType **parameter_type,
                                     const GVariantType **state_type,
                                     GVariant           **state_hint,
                                     GVariant           **state)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (group);

  return g_action_group_query_action (G_ACTION_GROUP (window->priv->actions),
                                      action_name, enabled, parameter_type, state_type, state_hint, state);
}

static GAction *
gtk_application_window_lookup_action (GActionMap *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  return g_action_map_lookup_action (G_ACTION_MAP (window->priv->actions), action_name);
}

static void
gtk_application_window_add_action (GActionMap *action_map,
                                   GAction    *action)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  g_action_map_add_action (G_ACTION_MAP (window->priv->actions), action);
}

static void
gtk_application_window_remove_action (GActionMap  *action_map,
                                      const gchar *action_name)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (action_map);

  g_action_map_remove_action (G_ACTION_MAP (window->priv->actions), action_name);
}

static void
gtk_application_window_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = gtk_application_window_list_actions;
  iface->query_action = gtk_application_window_query_action;
}

static void
gtk_application_window_map_iface_init (GActionMapInterface *iface)
{
  iface->lookup_action = gtk_application_window_lookup_action;
  iface->add_action = gtk_application_window_add_action;
  iface->remove_action = gtk_application_window_remove_action;
}

G_DEFINE_TYPE_WITH_CODE (GtkApplicationWindow, gtk_application_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gtk_application_window_group_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, gtk_application_window_map_iface_init))

enum {
  PROP_0,
  PROP_SHOW_APP_MENU,
  N_PROPS
};
static GParamSpec *gtk_application_window_properties[N_PROPS];

static void
gtk_application_window_real_get_preferred_height (GtkWidget *widget,
                                                  gint      *minimum_height,
                                                  gint      *natural_height)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_height (widget, minimum_height, natural_height);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_height, menubar_nat_height;

      gtk_widget_get_preferred_height (GTK_WIDGET (window->priv->menubar), &menubar_min_height, &menubar_nat_height);
      *minimum_height += menubar_min_height;
      *natural_height += menubar_nat_height;
    }
}

static void
gtk_application_window_real_get_preferred_height_for_width (GtkWidget *widget,
                                                            gint       width,
                                                            gint      *minimum_height,
                                                            gint      *natural_height)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_height_for_width (widget, width, minimum_height, natural_height);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_height, menubar_nat_height;

      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (window->priv->menubar), width, &menubar_min_height, &menubar_nat_height);
      *minimum_height += menubar_min_height;
      *natural_height += menubar_nat_height;
    }
}

static void
gtk_application_window_real_get_preferred_width (GtkWidget *widget,
                                                 gint      *minimum_width,
                                                 gint      *natural_width)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_width (widget, minimum_width, natural_width);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_width, menubar_nat_width;

      gtk_widget_get_preferred_width (GTK_WIDGET (window->priv->menubar), &menubar_min_width, &menubar_nat_width);
      *minimum_width = MAX (*minimum_width, menubar_min_width);
      *natural_width = MAX (*natural_width, menubar_nat_width);
    }
}

static void
gtk_application_window_real_get_preferred_width_for_height (GtkWidget *widget,
                                                            gint       height,
                                                            gint      *minimum_width,
                                                            gint      *natural_width)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->get_preferred_width_for_height (widget, height, minimum_width, natural_width);

  if (window->priv->menubar != NULL)
    {
      gint menubar_min_width, menubar_nat_width;

      gtk_widget_get_preferred_width_for_height (GTK_WIDGET (window->priv->menubar), height, &menubar_min_width, &menubar_nat_width);
      *minimum_width = MAX (*minimum_width, menubar_min_width);
      *natural_width = MAX (*natural_width, menubar_nat_width);
    }
}

static void
gtk_application_window_real_size_allocate (GtkWidget     *widget,
                                           GtkAllocation *allocation)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  if (window->priv->menubar != NULL)
    {
      GtkAllocation menubar_allocation = *allocation;
      gint menubar_min_height, menubar_nat_height;
      GtkWidget *child;

      gtk_widget_get_preferred_height_for_width (GTK_WIDGET (window->priv->menubar), allocation->width, &menubar_min_height, &menubar_nat_height);

      menubar_allocation.height = menubar_min_height;
      gtk_widget_size_allocate (GTK_WIDGET (window->priv->menubar), &menubar_allocation);

      child = gtk_bin_get_child (GTK_BIN (window));
      if (child != NULL && gtk_widget_get_visible (child))
        {
          GtkAllocation child_allocation = *allocation;
          gint border_width;

          child_allocation.height = MAX (1, child_allocation.height - menubar_min_height);

          border_width = gtk_container_get_border_width (GTK_CONTAINER (window));
          child_allocation.x += border_width;
          child_allocation.y += border_width + menubar_min_height;
          child_allocation.width -= border_width * 2;
          child_allocation.height -= border_width * 2 - menubar_min_height;
          gtk_widget_size_allocate (child, &child_allocation);
        }

      gtk_widget_set_allocation (widget, allocation);
    }
  else
    GTK_WIDGET_CLASS (gtk_application_window_parent_class)
      ->size_allocate (widget, allocation);
}

static void
gtk_application_window_real_map (GtkWidget *widget)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (widget);

  /* XXX could elimate this by tweaking gtk_window_map */
  if (window->priv->menubar)
    gtk_widget_map (GTK_WIDGET (window->priv->menubar));

  GTK_WIDGET_CLASS (gtk_application_window_parent_class)
    ->map (widget);
}

static void
gtk_application_window_real_forall_internal (GtkContainer *container,
                                             gboolean      include_internal,
                                             GtkCallback   callback,
                                             gpointer      user_data)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (container);

  if (window->priv->menubar)
    callback (GTK_WIDGET (window->priv->menubar), user_data);

  GTK_CONTAINER_CLASS (gtk_application_window_parent_class)
    ->forall (container, include_internal, callback, user_data);
}


static void
gtk_application_window_get_property (GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SHOW_APP_MENU:
      g_value_set_boolean (value, window->priv->show_app_menu);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_window_set_property (GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SHOW_APP_MENU:
      gtk_application_window_set_show_app_menu (window, g_value_get_boolean (value));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_application_window_finalize (GObject *object)
{
  GtkApplicationWindow *window = GTK_APPLICATION_WINDOW (object);

  if (window->priv->menubar)
    g_object_unref (window->priv->menubar);

  g_object_unref (window->priv->actions);

  G_OBJECT_CLASS (gtk_application_window_parent_class)
    ->finalize (object);
}

static void
gtk_application_window_init (GtkApplicationWindow *window)
{
  window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, GTK_TYPE_APPLICATION_WINDOW, GtkApplicationWindowPrivate);

  window->priv->actions = g_simple_action_group_new ();

  /* window->priv->actions is the one and only ref on the group, so when
   * we finalize, the action group will die, disconnecting all signals.
   */
  g_signal_connect_swapped (window->priv->actions, "action-added",
                            G_CALLBACK (g_action_group_action_added), window);
  g_signal_connect_swapped (window->priv->actions, "action-enabled-changed",
                            G_CALLBACK (g_action_group_action_enabled_changed), window);
  g_signal_connect_swapped (window->priv->actions, "action-state-changed",
                            G_CALLBACK (g_action_group_action_state_changed), window);
  g_signal_connect_swapped (window->priv->actions, "action-removed",
                            G_CALLBACK (g_action_group_action_removed), window);
}

static void
gtk_application_window_class_init (GtkApplicationWindowClass *class)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  container_class->forall = gtk_application_window_real_forall_internal;
  widget_class->get_preferred_height = gtk_application_window_real_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_application_window_real_get_preferred_height_for_width;
  widget_class->get_preferred_width = gtk_application_window_real_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_application_window_real_get_preferred_width_for_height;
  widget_class->size_allocate = gtk_application_window_real_size_allocate;
  widget_class->map = gtk_application_window_real_map;
  object_class->get_property = gtk_application_window_get_property;
  object_class->set_property = gtk_application_window_set_property;
  object_class->finalize = gtk_application_window_finalize;

  gtk_application_window_properties[PROP_SHOW_APP_MENU] =
    g_param_spec_boolean ("show-app-menu", "show application menu",
                          "TRUE if the application menu should be included in the menubar at the top of the window",
                          FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, N_PROPS, gtk_application_window_properties);
  g_type_class_add_private (class, sizeof (GtkApplicationWindowPrivate));
}

GtkWidget *
gtk_application_window_new (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                       "application", application,
                       NULL);
}

gboolean
gtk_application_window_get_show_app_menu (GtkApplicationWindow *window)
{
  return window->priv->show_app_menu;
}

void
gtk_application_window_set_show_app_menu (GtkApplicationWindow *window,
                                          gboolean              show_app_menu)
{
  if (window->priv->show_app_menu != show_app_menu)
    {
      window->priv->show_app_menu = show_app_menu;
      g_object_notify_by_pspec (G_OBJECT (window), gtk_application_window_properties[PROP_SHOW_APP_MENU]);

      if (show_app_menu)
        {
          GtkWidget *menubar;
          GtkWidget *item;

          item = gtk_menu_item_new_with_label ("Application");
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), gtk_application_window_get_app_menu (window));

          menubar = gtk_menu_bar_new ();
          window->priv->menubar = g_object_ref_sink (menubar);
          gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
          gtk_widget_set_parent (menubar, GTK_WIDGET (window));
          gtk_widget_show_all (menubar);
        }
      else
        {
          gtk_widget_unparent (GTK_WIDGET (window->priv->menubar));
          g_object_unref (window->priv->menubar);
        }
    }
}

/* GtkMenu construction {{{1 */

typedef struct {
  GActionGroup *group;
  gchar        *name;
  gchar        *target;
  gulong        enabled_changed_id;
  gulong        state_changed_id;
  gulong        activate_handler;
} ActionData;

static void
action_data_free (gpointer data)
{
  ActionData *a = data;

  if (a->enabled_changed_id)
    g_signal_handler_disconnect (a->group, a->enabled_changed_id);

  if (a->state_changed_id)
    g_signal_handler_disconnect (a->group, a->state_changed_id);

  g_object_unref (a->group);
  g_free (a->name);
  g_free (a->target);

  g_free (a);
}

static void
enabled_changed (GActionGroup *group,
                 const gchar  *action_name,
                 gboolean      enabled,
                 GtkWidget    *widget)
{
  gtk_widget_set_sensitive (widget, enabled);
}

static void
item_activated (GtkWidget *w,
                gpointer   data)
{
  ActionData *a;
  GVariant *parameter;

  a = g_object_get_data (G_OBJECT (w), "action");
  if (a->target)
    parameter = g_variant_ref_sink (g_variant_new_string (a->target));
  else
    parameter = NULL;
  g_action_group_activate_action (a->group, a->name, parameter);
  if (parameter)
    g_variant_unref (parameter);
}

static void
toggle_state_changed (GActionGroup     *group,
                      const gchar      *name,
                      GVariant         *state,
                      GtkCheckMenuItem *w)
{
  ActionData *a;

  a = g_object_get_data (G_OBJECT (w), "action");
  g_signal_handler_block (w, a->activate_handler);
  gtk_check_menu_item_set_active (w, g_variant_get_boolean (state));
  g_signal_handler_unblock (w, a->activate_handler);
}

static void
radio_state_changed (GActionGroup     *group,
                     const gchar      *name,
                     GVariant         *state,
                     GtkCheckMenuItem *w)
{
  ActionData *a;
  gboolean b;

  a = g_object_get_data (G_OBJECT (w), "action");
  g_signal_handler_block (w, a->activate_handler);
  b = g_strcmp0 (a->target, g_variant_get_string (state, NULL)) == 0;
  gtk_check_menu_item_set_active (w, b);
  g_signal_handler_unblock (w, a->activate_handler);
}

static GtkWidget *
create_menuitem_from_model (GMenuModel   *model,
                            gint          item,
                            GActionGroup *group)
{
  GtkWidget *w;
  gchar *label;
  gchar *action;
  gchar *target;
  gchar *s;
  ActionData *a;
  const GVariantType *type;
  GVariant *v;

  label = NULL;
  g_menu_model_get_item_attribute (model, item, G_MENU_ATTRIBUTE_LABEL, "s", &label);

  action = NULL;
  g_menu_model_get_item_attribute (model, item, G_MENU_ATTRIBUTE_ACTION, "s", &action);

  if (action != NULL)
    type = g_action_group_get_action_state_type (group, action);
  else
    type = NULL;

  if (type == NULL)
    w = gtk_menu_item_new_with_mnemonic (label);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    w = gtk_check_menu_item_new_with_label (label);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      w = gtk_check_menu_item_new_with_label (label);
      gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (w), TRUE);
    }
  else
    g_assert_not_reached ();

  if (action != NULL)
    {
      a = g_new0 (ActionData, 1);
      a->group = g_object_ref (group);
      a->name = g_strdup (action);
      g_object_set_data_full (G_OBJECT (w), "action", a, action_data_free);

      if (!g_action_group_get_action_enabled (group, action))
        gtk_widget_set_sensitive (w, FALSE);

      s = g_strconcat ("action-enabled-changed::", action, NULL);
      a->enabled_changed_id = g_signal_connect (group, s,
                                                G_CALLBACK (enabled_changed), w);
      g_free (s);
      a->activate_handler = g_signal_connect (w, "activate",
                                              G_CALLBACK (item_activated), NULL);

      if (type == NULL)
        {
          /* all set */
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
        {
          s = g_strconcat ("action-state-changed::", action, NULL);
          a->state_changed_id = g_signal_connect (group, s,
                                                  G_CALLBACK (toggle_state_changed), w);
          g_free (s);
          v = g_action_group_get_action_state (group, action);
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w),
                                          g_variant_get_boolean (v));
          g_variant_unref (v);
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
        {
          s = g_strconcat ("action-state-changed::", action, NULL);
          a->state_changed_id = g_signal_connect (group, s,
                                                  G_CALLBACK (radio_state_changed), w);
          g_free (s);
          g_menu_model_get_item_attribute (model, item, G_MENU_ATTRIBUTE_TARGET, "s", &target);
          a->target = g_strdup (target);
          v = g_action_group_get_action_state (group, action);
          gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w),
                                          g_strcmp0 (g_variant_get_string (v, NULL), target) == 0);
          g_variant_unref (v);
          g_free (target);
        }
      else
        g_assert_not_reached ();
    }

  g_free (label);
  g_free (action);

  return w;
}

static void populate_menu_from_model (GtkMenuShell *menu,
                                      GMenuModel   *model,
                                      GActionGroup *group);

static void
append_items_from_model (GtkMenuShell *menu,
                         GMenuModel   *model,
                         GActionGroup *group,
                         gboolean     *need_separator,
                         const gchar  *heading)
{
  gint n;
  gint i;
  GtkWidget *w;
  GtkWidget *menuitem;
  GtkWidget *submenu;
  GMenuModel *m;
  gchar *label;

  n = g_menu_model_get_n_items (model);

  if (*need_separator && n > 0)
    {
      w = gtk_separator_menu_item_new ();
      gtk_widget_show (w);
      gtk_menu_shell_append (menu, w);
      *need_separator = FALSE;
    }

  if (heading != NULL)
    {
      w = gtk_menu_item_new_with_label (heading);
      gtk_widget_show (w);
      gtk_widget_set_sensitive (w, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
    }

  for (i = 0; i < n; i++)
    {
      if ((m = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION)))
        {
          label = NULL;
          g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label);
          append_items_from_model (menu, m, group, need_separator, label);
          g_object_unref (m);
          g_free (label);
          continue;
        }

      if (*need_separator)
        {
          w = gtk_separator_menu_item_new ();
          gtk_widget_show (w);
          gtk_menu_shell_append (menu, w);
          *need_separator = FALSE;
        }

      menuitem = create_menuitem_from_model (model, i, group);

      if ((m = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU)))
        {
          submenu = gtk_menu_new ();
          populate_menu_from_model (GTK_MENU_SHELL (submenu), m, group);
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
          g_object_unref (m);
        }

      gtk_widget_show (menuitem);
      gtk_menu_shell_append (menu, menuitem);

      *need_separator = TRUE;
    }
}

static void
populate_menu_from_model (GtkMenuShell *menu,
                          GMenuModel   *model,
                          GActionGroup *group)
{
  gboolean need_separator;

  need_separator = FALSE;
  append_items_from_model (menu, model, group, &need_separator, NULL);
}

typedef struct {
  GtkApplication *application;
  GtkMenuShell   *menu;
  guint           update_idle;
  GHashTable     *connected;
} ItemsChangedData;

static void
free_items_changed_data (gpointer data)
{
  ItemsChangedData *d = data;

  g_object_unref (d->application);

  if (d->update_idle != 0)
    g_source_remove (d->update_idle);

  g_hash_table_unref (d->connected);

  g_free (d);
}

static gboolean
repopulate_menu (gpointer data)
{
  ItemsChangedData *d = data;
  GList *children, *l;
  GtkWidget *child;
  GMenuModel *model;

  /* remove current children */
  children = gtk_container_get_children (GTK_CONTAINER (d->menu));
  for (l = children; l; l = l->next)
    {
      child = l->data;
      gtk_container_remove (GTK_CONTAINER (d->menu), child);
    }
  g_list_free (children);

  /* repopulate */
  model = g_application_get_menu (G_APPLICATION (d->application));
  populate_menu_from_model (d->menu, model, G_ACTION_GROUP (d->application));

  d->update_idle = 0;

  return FALSE;
}

static void
connect_to_items_changed (GMenuModel *model,
                          GCallback   callback,
                          gpointer    data)
{
  ItemsChangedData *d = data;
  gint i;
  GMenuModel *m;
  GMenuLinkIter *iter;

  if (!g_hash_table_lookup (d->connected, model))
    {
      g_signal_connect (model, "items-changed", callback, data);
      g_hash_table_insert (d->connected, model, model);
    }

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    {
      iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_next (iter))
        {
          m = g_menu_link_iter_get_value (iter);
          connect_to_items_changed (m, callback, data);
          g_object_unref (m);
        }
      g_object_unref (iter);
    }
}

static void
items_changed (GMenuModel *model,
               gint        position,
               gint        removed,
               gint        added,
               gpointer    data)
{
  ItemsChangedData *d = data;

  if (d->update_idle == 0)
    d->update_idle = gdk_threads_add_idle (repopulate_menu, data);
  connect_to_items_changed (model, G_CALLBACK (items_changed), data);
}

/**
 * gtk_application_window_get_app_menu:
 * @application: a #GtkApplication
 *
 * Populates a menu widget from a menu model that is
 * associated with @application. See g_application_set_menu().
 * The menu items will be connected to action of @application,
 * as indicated by the menu model. The menus contents will be
 * updated automatically in response to menu model changes.
 *
 * It is the callers responsibility to add the menu at a
 * suitable place in the widget hierarchy.
 *
 * This function returns %NULL if @application has no associated
 * menu model. It also returns %NULL if the menu model is
 * represented outside the application, e.g. by an application
 * menu in the desktop shell.
 *
 * @menu may be a #GtkMenu or a #GtkMenuBar.
 *
 * Returns: A #GtkMenu that has been populated from the
 *     #GMenuModel that is associated with @application,
 *     or %NULL
 */
GtkWidget *
gtk_application_window_get_app_menu (GtkApplicationWindow *window)
{
  GtkApplication *application;
  GtkWidget *menu;
  GMenuModel *model;
  ItemsChangedData *data;
  GActionMuxer *muxer;

  application = gtk_window_get_application (GTK_WINDOW (window));

  model = g_application_get_menu (G_APPLICATION (application));

  if (!model)
    return NULL;

  menu = gtk_menu_new ();

  muxer = g_action_muxer_new ();
  g_action_muxer_insert (muxer, "app", G_ACTION_GROUP (application));
  g_action_muxer_insert (muxer, "win", G_ACTION_GROUP (window));
  populate_menu_from_model (GTK_MENU_SHELL (menu), model, G_ACTION_GROUP (muxer));
  g_object_unref (muxer);

  data = g_new (ItemsChangedData, 1);
  data->application = g_object_ref (application);
  data->menu = GTK_MENU_SHELL (menu);
  data->update_idle = 0;
  data->connected = g_hash_table_new (NULL, NULL);

  g_object_set_data_full (G_OBJECT (menu), "gtk-application-menu-data",
                          data, free_items_changed_data);

  connect_to_items_changed (model, G_CALLBACK (items_changed), data);

  return menu;
}
