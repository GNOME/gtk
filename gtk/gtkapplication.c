/*
 * Copyright © 2010 Codethink Limited
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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

#include "gtkapplication.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkwindow.h"
#include "gtkcheckmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkmenu.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/**
 * SECTION:gtkapplication
 * @title: GtkApplication
 * @short_description: Application class
 *
 * #GtkApplication is a class that handles many important aspects
 * of a GTK+ application in a convenient fashion, without enforcing
 * a one-size-fits-all application model.
 *
 * Currently, GtkApplication handles GTK+ initialization, application
 * uniqueness, provides some basic scriptability by exporting 'actions',
 * and manages a list of toplevel windows whose life-cycle is automatically
 * tied to the life-cycle of your application.
 *
 * <example id="gtkapplication"><title>A simple application</title>
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/bloatpad.c">
 *  <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 * </example>
 */

enum {
  WINDOW_ADDED,
  WINDOW_REMOVED,
  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

struct _GtkApplicationPrivate
{
  GList *windows;
};

static gboolean
gtk_application_focus_in_event_cb (GtkWindow      *window,
                                   GdkEventFocus  *event,
                                   GtkApplication *application)
{
  GtkApplicationPrivate *priv = application->priv;
  GList *link;

  /* Keep the window list sorted by most-recently-focused. */
  link = g_list_find (priv->windows, window);
  if (link != NULL && link != priv->windows)
    {
      priv->windows = g_list_remove_link (priv->windows, link);
      priv->windows = g_list_concat (link, priv->windows);
    }

  return FALSE;
}

static void
gtk_application_startup (GApplication *application)
{
  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->startup (application);

  gtk_init (0, 0);
}

static void
gtk_application_add_platform_data (GApplication    *application,
                                   GVariantBuilder *builder)
{
  const gchar *startup_id;

  startup_id = getenv ("DESKTOP_STARTUP_ID");
  
  if (startup_id && g_utf8_validate (startup_id, -1, NULL))
    g_variant_builder_add (builder, "{sv}", "desktop-startup-id",
                           g_variant_new_string (startup_id));
}

static void
gtk_application_before_emit (GApplication *application,
                             GVariant     *platform_data)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
#ifdef GDK_WINDOWING_X11
      if (strcmp (key, "desktop-startup-id") == 0)
        {
          GdkDisplay *display;
          const gchar *id;

          display = gdk_display_get_default ();
          id = g_variant_get_string (value, NULL);
          gdk_x11_display_set_startup_notification_id (display, id);
       }
#endif
    }
}

static void
gtk_application_after_emit (GApplication *application,
                            GVariant     *platform_data)
{
  gdk_notify_startup_complete ();
}

static void
gtk_application_init (GtkApplication *application)
{
  application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                   GTK_TYPE_APPLICATION,
                                                   GtkApplicationPrivate);
}

static void
gtk_application_window_added (GtkApplication *application,
                              GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

  priv->windows = g_list_prepend (priv->windows, window);
  gtk_window_set_application (window, application);
  g_application_hold (G_APPLICATION (application));

  g_signal_connect (window, "focus-in-event",
                    G_CALLBACK (gtk_application_focus_in_event_cb),
                    application);
}

static void
gtk_application_window_removed (GtkApplication *application,
                                GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

  g_signal_handlers_disconnect_by_func (window,
                                        gtk_application_focus_in_event_cb,
                                        application);

  g_application_release (G_APPLICATION (application));
  priv->windows = g_list_remove (priv->windows, window);
  gtk_window_set_application (window, NULL);
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->add_platform_data = gtk_application_add_platform_data;
  application_class->before_emit = gtk_application_before_emit;
  application_class->after_emit = gtk_application_after_emit;
  application_class->startup = gtk_application_startup;

  class->window_added = gtk_application_window_added;
  class->window_removed = gtk_application_window_removed;

  g_type_class_add_private (class, sizeof (GtkApplicationPrivate));

  /**
   * GtkApplication::window-added:
   * @application: the #GtkApplication which emitted the signal
   * @window: the newly-added #GtkWindow
   *
   * Emitted when a #GtkWindow is added to @application through
   * gtk_application_add_window().
   *
   * Since: 3.2
   */
  gtk_application_signals[WINDOW_ADDED] =
    g_signal_new ("window-added", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);

  /**
   * GtkApplication::window-removed:
   * @application: the #GtkApplication which emitted the signal
   * @window: the #GtkWindow that is being removed
   *
   * Emitted when a #GtkWindow is removed from @application,
   * either as a side-effect of being destroyed or explicitly
   * through gtk_application_remove_window().
   *
   * Since: 3.2
   */
  gtk_application_signals[WINDOW_REMOVED] =
    g_signal_new ("window-removed", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);
}

/**
 * gtk_application_new:
 * @application_id: the application id
 * @flags: the application flags
 *
 * Creates a new #GtkApplication instance.
 *
 * This function calls g_type_init() for you. gtk_init() is called
 * as soon as the application gets registered as the primary instance.
 *
 * The application id must be valid. See g_application_id_is_valid().
 *
 * Returns: a new #GtkApplication instance
 */
GtkApplication *
gtk_application_new (const gchar       *application_id,
                     GApplicationFlags  flags)
{
  g_return_val_if_fail (g_application_id_is_valid (application_id), NULL);

  g_type_init ();

  return g_object_new (GTK_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

/**
 * gtk_application_add_window:
 * @application: a #GtkApplication
 * @window: a #GtkWindow
 *
 * Adds a window from @application.
 *
 * This call is equivalent to setting the #GtkWindow:application
 * property of @window to @application.
 *
 * Normally, the connection between the application and the window
 * will remain until the window is destroyed, but you can explicitly
 * remove it with gtk_application_remove_window().
 *
 * GTK+ will keep the application running as long as it has
 * any windows.
 *
 * Since: 3.0
 **/
void
gtk_application_add_window (GtkApplication *application,
                            GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (!g_list_find (application->priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_ADDED], 0, window);
}

/**
 * gtk_application_remove_window:
 * @application: a #GtkApplication
 * @window: a #GtkWindow
 *
 * Remove a window from @application.
 *
 * If @window belongs to @application then this call is equivalent to
 * setting the #GtkWindow:application property of @window to
 * %NULL.
 *
 * The application may stop running as a result of a call to this
 * function.
 *
 * Since: 3.0
 **/
void
gtk_application_remove_window (GtkApplication *application,
                               GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (g_list_find (application->priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_REMOVED], 0, window);
}

/**
 * gtk_application_get_windows:
 * @application: a #GtkApplication
 *
 * Gets a list of the #GtkWindow<!-- -->s associated with @application.
 *
 * The list is sorted by most recently focused window, such that the first
 * element is the currently focused window.  (Useful for choosing a parent
 * for a transient window.)
 *
 * The list that is returned should not be modified in any way. It will
 * only remain valid until the next focus change or window creation or
 * deletion.
 *
 * Returns: (element-type GtkWindow) (transfer none): a #GList of #GtkWindow
 *
 * Since: 3.0
 **/
GList *
gtk_application_get_windows (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->windows;
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
 * gtk_application_get_menu:
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
GtkMenu *
gtk_application_get_menu (GtkApplication *application)
{
  GtkWidget *menu;
  GMenuModel *model;
  ItemsChangedData *data;

  model = g_application_get_menu (G_APPLICATION (application));

  if (!model)
    return NULL;

  /* FIXME: find out if external menu is available. If yes, return NULL */

  menu = gtk_menu_new ();

  populate_menu_from_model (GTK_MENU_SHELL (menu), model, G_ACTION_GROUP (application));

  data = g_new (ItemsChangedData, 1);
  data->application = g_object_ref (application);
  data->menu = GTK_MENU_SHELL (menu);
  data->update_idle = 0;
  data->connected = g_hash_table_new (NULL, NULL);

  g_object_set_data_full (G_OBJECT (menu), "gtk-application-menu-data",
                          data, free_items_changed_data);

  connect_to_items_changed (model, G_CALLBACK (items_changed), data);

  return GTK_MENU (menu);
}
