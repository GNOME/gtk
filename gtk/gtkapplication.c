/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
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

#include "gtkapplication.h"

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>

#include "gtkapplicationprivate.h"
#include "gtkclipboard.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkrecentmanager.h"
#include "gtkaccelmapprivate.h"
#include "gtkicontheme.h"
#include "gtkbuilder.h"
#include "gtkintl.h"

/* NB: please do not add backend-specific GDK headers here.  This should
 * be abstracted via GtkApplicationImpl.
 */

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
 * uniqueness, session management, provides some basic scriptability and
 * desktop shell integration by exporting actions and menus and manages a
 * list of toplevel windows whose life-cycle is automatically tied to the
 * life-cycle of your application.
 *
 * While GtkApplication works fine with plain #GtkWindows, it is recommended
 * to use it together with #GtkApplicationWindow.
 *
 * When GDK threads are enabled, GtkApplication will acquire the GDK
 * lock when invoking actions that arrive from other processes.  The GDK
 * lock is not touched for local action invocations.  In order to have
 * actions invoked in a predictable context it is therefore recommended
 * that the GDK lock be held while invoking actions locally with
 * g_action_group_activate_action().  The same applies to actions
 * associated with #GtkApplicationWindow and to the “activate” and
 * 'open' #GApplication methods.
 *
 * ## Automatic resources ## {#automatic-resources}
 *
 * #GtkApplication will automatically load menus from the #GtkBuilder
 * file located at "gtk/menus.ui", relative to the application's
 * resource base path (see g_application_set_resource_base_path()).  The
 * menu with the ID "app-menu" is taken as the application's app menu
 * and the menu with the ID "menubar" is taken as the application's
 * menubar.  Additional menus (most interesting submenus) can be named
 * and accessed via gtk_application_get_menu_by_id() which allows for
 * dynamic population of a part of the menu structure.
 *
 * If the files "gtk/menus-appmenu.ui" or "gtk/menus-traditional.ui" are
 * present then these files will be used in preference, depending on the
 * value of gtk_application_prefers_app_menu().
 *
 * It is also possible to provide the menus manually using
 * gtk_application_set_app_menu() and gtk_application_set_menubar().
 *
 * #GtkApplication will also automatically setup an icon search path for
 * the default icon theme by appending "icons" to the resource base
 * path.  This allows your application to easily store its icons as
 * resources.  See gtk_icon_theme_add_resource_path() for more
 * information.
 *
 * ## A simple application ## {#gtkapplication}
 *
 * [A simple example](https://git.gnome.org/browse/gtk+/tree/examples/bp/bloatpad.c)
 *
 * GtkApplication optionally registers with a session manager
 * of the users session (if you set the #GtkApplication:register-session
 * property) and offers various functionality related to the session
 * life-cycle.
 *
 * An application can block various ways to end the session with
 * the gtk_application_inhibit() function. Typical use cases for
 * this kind of inhibiting are long-running, uninterruptible operations,
 * such as burning a CD or performing a disk backup. The session
 * manager may not honor the inhibitor, but it can be expected to
 * inform the user about the negative consequences of ending the
 * session while inhibitors are present.
 */

enum {
  WINDOW_ADDED,
  WINDOW_REMOVED,
  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL];

enum {
  PROP_ZERO,
  PROP_REGISTER_SESSION,
  PROP_APP_MENU,
  PROP_MENUBAR,
  PROP_ACTIVE_WINDOW
};

/* Accel handling */
typedef struct
{
  guint           key;
  GdkModifierType modifier;
} AccelKey;

typedef struct
{
  GHashTable *action_to_accels;
  GHashTable *accel_to_actions;
} Accels;

static AccelKey *
accel_key_copy (const AccelKey *source)
{
  AccelKey *dest;

  dest = g_slice_new (AccelKey);
  dest->key = source->key;
  dest->modifier = source->modifier;

  return dest;
}

static void
accel_key_free (gpointer data)
{
  AccelKey *key = data;

  g_slice_free (AccelKey, key);
}

static guint
accel_key_hash (gconstpointer data)
{
  const AccelKey *key = data;

  return key->key + (key->modifier << 16);
}

static gboolean
accel_key_equal (gconstpointer a,
                 gconstpointer b)
{
  const AccelKey *ak = a;
  const AccelKey *bk = b;

  return ak->key == bk->key && ak->modifier == bk->modifier;
}

static void
accels_foreach_key (Accels                   *accels,
                    GtkWindow                *window,
                    GtkWindowKeysForeachFunc  callback,
                    gpointer                  user_data)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init (&iter, accels->accel_to_actions);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      AccelKey *accel_key = key;

      (* callback) (window, accel_key->key, accel_key->modifier, FALSE, user_data);
    }
}

static gboolean
accels_activate (Accels          *accels,
                 GActionGroup    *action_group,
                 guint            key,
                 GdkModifierType  modifier)
{
  AccelKey accel_key = { key, modifier };
  const gchar **actions;
  gint i;

  actions = g_hash_table_lookup (accels->accel_to_actions, &accel_key);

  if (actions == NULL)
    return FALSE;

  /* We may have more than one action on a given accel.  This could be
   * the case if we have different types of windows with different
   * actions in each.
   *
   * Find the first one that will successfully activate and use it.
   */
  for (i = 0; actions[i]; i++)
    {
      const GVariantType *parameter_type;
      const gchar *action_name;
      const gchar *sep;
      gboolean enabled;
      GVariant *target;

      sep = strrchr (actions[i], '|');
      action_name = sep + 1;

      if (!g_action_group_query_action (action_group, action_name, &enabled, &parameter_type, NULL, NULL, NULL))
        continue;

      if (!enabled)
        continue;

      /* We found an action with the correct name and it's enabled.
       * This is the action that we are going to try to invoke.
       *
       * There is still the possibility that the target value doesn't
       * match the expected parameter type.  In that case, we will print
       * a warning.
       *
       * Note: we want to hold a ref on the target while we're invoking
       * the action to prevent trouble if someone uninstalls the accel
       * from the handler.  That's not a problem since we're parsing it.
       */
      if (actions[i] != sep) /* if it has a target... */
        {
          GError *error = NULL;

          if (parameter_type == NULL)
            {
              gchar *accel_str = gtk_accelerator_name (key, modifier);
              g_warning ("Accelerator '%s' tries to invoke action '%s' with target, but action has no parameter",
                         accel_str, action_name);
              g_free (accel_str);
              return TRUE;
            }

          target = g_variant_parse (NULL, actions[i], sep, NULL, &error);
          g_assert_no_error (error);
          g_assert (target);

          if (!g_variant_is_of_type (target, parameter_type))
            {
              gchar *accel_str = gtk_accelerator_name (key, modifier);
              gchar *typestr = g_variant_type_dup_string (parameter_type);
              gchar *targetstr = g_variant_print (target, TRUE);
              g_warning ("Accelerator '%s' tries to invoke action '%s' with target '%s',"
                         " but action expects parameter with type '%s'", accel_str, action_name, targetstr, typestr);
              g_variant_unref (target);
              g_free (targetstr);
              g_free (accel_str);
              g_free (typestr);
              return TRUE;
            }
        }
      else
        {
          if (parameter_type != NULL)
            {
              gchar *accel_str = gtk_accelerator_name (key, modifier);
              gchar *typestr = g_variant_type_dup_string (parameter_type);
              g_warning ("Accelerator '%s' tries to invoke action '%s' without target,"
                         " but action expects parameter with type '%s'", accel_str, action_name, typestr);
              g_free (accel_str);
              g_free (typestr);
              return TRUE;
            }

          target = NULL;
        }

      g_action_group_activate_action (action_group, action_name, target);

      if (target)
        g_variant_unref (target);

      return TRUE;
    }

  return FALSE;
}

static void
accels_add_entry (Accels         *accels,
                  AccelKey       *key,
                  const gchar    *action_and_target)
{
  const gchar **old;
  const gchar **new;
  gint n;

  old = g_hash_table_lookup (accels->accel_to_actions, key);
  if (old != NULL)
    for (n = 0; old[n]; n++)  /* find the length */
      ;
  else
    n = 0;

  new = g_new (const gchar *, n + 1 + 1);
  memcpy (new, old, n * sizeof (const gchar *));
  new[n] = action_and_target;
  new[n + 1] = NULL;

  g_hash_table_insert (accels->accel_to_actions, accel_key_copy (key), new);
}

static void
accels_remove_entry (Accels         *accels,
                     AccelKey       *key,
                     const gchar    *action_and_target)
{
  const gchar **old;
  const gchar **new;
  gint n, i;

  /* if we can't find the entry then something has gone very wrong... */
  old = g_hash_table_lookup (accels->accel_to_actions, key);
  g_assert (old != NULL);

  for (n = 0; old[n]; n++)  /* find the length */
    ;
  g_assert_cmpint (n, >, 0);

  if (n == 1)
    {
      /* The simple case of removing the last action for an accel. */
      g_assert_cmpstr (old[0], ==, action_and_target);
      g_hash_table_remove (accels->accel_to_actions, key);
      return;
    }

  for (i = 0; i < n; i++)
    if (g_str_equal (old[i], action_and_target))
      break;

  /* We must have found it... */
  g_assert_cmpint (i, <, n);

  new = g_new (const gchar *, n - 1 + 1);
  memcpy (new, old, i * sizeof (const gchar *));
  memcpy (new + i, old + i + 1, (n - (i + 1)) * sizeof (const gchar *));
  new[n - 1] = NULL;

  g_hash_table_insert (accels->accel_to_actions, accel_key_copy (key), new);
}

static void
accels_set_accels_for_action (Accels              *accels,
                              const gchar         *action_and_target,
                              const gchar * const *accelerators)
{
  AccelKey *keys, *old_keys;
  gint i, n;

  n = accelerators ? g_strv_length ((gchar **) accelerators) : 0;

  if (n > 0)
    {
      keys = g_new0 (AccelKey, n + 1);

      for (i = 0; i < n; i++)
        {
          gtk_accelerator_parse (accelerators[i], &keys[i].key, &keys[i].modifier);

          if (keys[i].key == 0)
            {
              g_warning ("Unable to parse accelerator '%s': ignored request to install %d accelerators",
                         accelerators[i], n);
              g_free (keys);
              return;
            }
        }
    }
  else
    keys = NULL;

  old_keys = g_hash_table_lookup (accels->action_to_accels, action_and_target);
  if (old_keys)
    {
      /* We need to remove accel entries from existing keys */
      for (i = 0; old_keys[i].key; i++)
        accels_remove_entry (accels, &old_keys[i], action_and_target);
    }

  if (keys)
    {
      gchar *my_key;
      gint i;

      my_key = g_strdup (action_and_target);

      g_hash_table_replace (accels->action_to_accels, my_key, keys);

      for (i = 0; i < n; i++)
        accels_add_entry (accels, &keys[i], my_key);
    }
  else
    g_hash_table_remove (accels->action_to_accels, action_and_target);
}

gchar **
accels_get_accels_for_action (Accels      *accels,
                              const gchar *action_and_target)
{
  AccelKey *keys;
  gchar **result;
  gint n, i = 0;

  keys = g_hash_table_lookup (accels->action_to_accels, action_and_target);
  if (!keys)
    return g_new0 (gchar *, 0 + 1);

  for (n = 0; keys[n].key; n++)
    ;

  result = g_new0 (gchar *, n + 1);

  for (i = 0; i < n; i++)
    result[i] = gtk_accelerator_name (keys[i].key, keys[i].modifier);

  return result;
}

static const gchar * const *
accels_get_actions_for_accel (Accels         *accels,
                              const AccelKey *accel_key)
{
  return g_hash_table_lookup (accels->accel_to_actions, accel_key);
}

static void
accels_init (Accels *accels)
{
  accels->accel_to_actions = g_hash_table_new_full (accel_key_hash, accel_key_equal,
                                                    accel_key_free, g_free);
  accels->action_to_accels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
accels_finalize (Accels *accels)
{
  g_hash_table_unref (accels->accel_to_actions);
  g_hash_table_unref (accels->action_to_accels);
}

struct _GtkApplicationPrivate
{
  GtkApplicationImpl *impl;

  GList *windows;

  GMenuModel      *app_menu;
  GMenuModel      *menubar;
  Accels           accels;
  guint            last_window_id;

  gboolean register_session;
  GtkActionMuxer  *muxer;
  GtkBuilder      *menus_builder;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

const gchar *
gtk_application_get_app_menu_object_path (GtkApplication *application)
{
  g_assert_not_reached (); /* XXX */
}

const gchar *
gtk_application_get_menubar_object_path (GtkApplication *application)
{
  g_assert_not_reached (); /* XXX */
}

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

  gtk_application_impl_active_window_changed (application->priv->impl, window);
  g_object_notify (G_OBJECT (application), "active-window");

  return FALSE;
}

static void
gtk_application_load_resources (GtkApplication *application)
{
  const gchar *base_path;

  base_path = g_application_get_resource_base_path (G_APPLICATION (application));

  if (base_path == NULL)
    return;

  /* Expand the icon search path */
  {
    GtkIconTheme *default_theme;
    gchar *iconspath;

    default_theme = gtk_icon_theme_get_default ();
    iconspath = g_strconcat (base_path, "/icons", NULL);
    gtk_icon_theme_add_resource_path (default_theme, iconspath);
    g_free (iconspath);
  }

  /* Load the menus */
  {
    gchar *menuspath;

    /* If the user has given a specific file for the variant of menu
     * that we are looking for, use it with preference.
     */
    if (gtk_application_prefers_app_menu (application))
      menuspath = g_strconcat (base_path, "/gtk/menus-appmenu.ui", NULL);
    else
      menuspath = g_strconcat (base_path, "/gtk/menus-traditional.ui", NULL);

    if (g_resources_get_info (menuspath, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
      application->priv->menus_builder = gtk_builder_new_from_resource (menuspath);
    g_free (menuspath);

    /* If we didn't get the specific file, fall back. */
    if (application->priv->menus_builder == NULL)
      {
        menuspath = g_strconcat (base_path, "/gtk/menus.ui", NULL);
        if (g_resources_get_info (menuspath, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
          application->priv->menus_builder = gtk_builder_new_from_resource (menuspath);
        g_free (menuspath);
      }

    /* Always load from -common as well, if we have it */
    menuspath = g_strconcat (base_path, "/gtk/menus-common.ui", NULL);
    if (g_resources_get_info (menuspath, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
      {
        GError *error = NULL;

        if (application->priv->menus_builder == NULL)
          application->priv->menus_builder = gtk_builder_new ();

        if (!gtk_builder_add_from_resource (application->priv->menus_builder, menuspath, &error))
          g_error ("failed to load menus-common.ui: %s", error->message);
      }
    g_free (menuspath);

    if (application->priv->menus_builder)
      {
        GObject *menu;

        menu = gtk_builder_get_object (application->priv->menus_builder, "app-menu");
        if (menu != NULL && G_IS_MENU_MODEL (menu))
          gtk_application_set_app_menu (application, G_MENU_MODEL (menu));
        menu = gtk_builder_get_object (application->priv->menus_builder, "menubar");
        if (menu != NULL && G_IS_MENU_MODEL (menu))
          gtk_application_set_menubar (application, G_MENU_MODEL (menu));
      }
  }
}


static void
gtk_application_startup (GApplication *g_application)
{
  GtkApplication *application = GTK_APPLICATION (g_application);

  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->startup (g_application);

  gtk_action_muxer_insert (application->priv->muxer, "app", G_ACTION_GROUP (application));

  gtk_init (0, 0);

  application->priv->impl = gtk_application_impl_new (application, gdk_display_get_default ());
  gtk_application_impl_startup (application->priv->impl, application->priv->register_session);

  gtk_application_load_resources (application);
}

static void
gtk_application_shutdown (GApplication *g_application)
{
  GtkApplication *application = GTK_APPLICATION (g_application);

  gtk_application_impl_shutdown (application->priv->impl);
  g_clear_object (&application->priv->impl);

  gtk_action_muxer_remove (application->priv->muxer, "app");

  /* Keep this section in sync with gtk_main() */

  /* Try storing all clipboard data we have */
  _gtk_clipboard_store_all ();

  /* Synchronize the recent manager singleton */
  _gtk_recent_manager_sync ();

  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->shutdown (g_application);
}

static gboolean
gtk_application_local_command_line (GApplication   *application,
                                    gchar        ***arguments,
                                    gint           *exit_status)
{
  g_application_add_option_group (application, gtk_get_option_group (FALSE));

  return G_APPLICATION_CLASS (gtk_application_parent_class)->local_command_line (application, arguments, exit_status);
}

static void
gtk_application_add_platform_data (GApplication    *application,
                                   GVariantBuilder *builder)
{
  const gchar *startup_id;

  /* This is slightly evil.
   *
   * We don't have an impl here because we're remote so we can't figure
   * out what to do on a per-display-server basis.
   *
   * So we do all the things... which currently is just one thing.
   */
  startup_id = getenv ("DESKTOP_STARTUP_ID");

  if (startup_id && g_utf8_validate (startup_id, -1, NULL))
    g_variant_builder_add (builder, "{sv}", "desktop-startup-id",
                           g_variant_new_string (startup_id));
}

static void
gtk_application_before_emit (GApplication *g_application,
                             GVariant     *platform_data)
{
  GtkApplication *application = GTK_APPLICATION (g_application);

  gdk_threads_enter ();

  gtk_application_impl_before_emit (application->priv->impl, platform_data);
}

static void
gtk_application_after_emit (GApplication *application,
                            GVariant     *platform_data)
{
  gdk_notify_startup_complete ();

  gdk_threads_leave ();
}

static void
gtk_application_init (GtkApplication *application)
{
  application->priv = gtk_application_get_instance_private (application);

  application->priv->muxer = gtk_action_muxer_new ();

  accels_init (&application->priv->accels);
}

static void
gtk_application_window_added (GtkApplication *application,
                              GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

  if (GTK_IS_APPLICATION_WINDOW (window))
    gtk_application_window_set_id (GTK_APPLICATION_WINDOW (window), ++application->priv->last_window_id);

  priv->windows = g_list_prepend (priv->windows, window);
  gtk_window_set_application (window, application);
  g_application_hold (G_APPLICATION (application));

  g_signal_connect (window, "focus-in-event",
                    G_CALLBACK (gtk_application_focus_in_event_cb),
                    application);

  gtk_application_impl_window_added (application->priv->impl, window);

  gtk_application_impl_active_window_changed (application->priv->impl, window);
  g_object_notify (G_OBJECT (application), "active-window");
}

static void
gtk_application_window_removed (GtkApplication *application,
                                GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;
  gpointer old_active;

  old_active = priv->windows;

  gtk_application_impl_window_removed (application->priv->impl, window);

  g_signal_handlers_disconnect_by_func (window,
                                        gtk_application_focus_in_event_cb,
                                        application);

  g_application_release (G_APPLICATION (application));
  priv->windows = g_list_remove (priv->windows, window);
  gtk_window_set_application (window, NULL);

  if (priv->windows != old_active)
    {
      gtk_application_impl_active_window_changed (application->priv->impl, priv->windows ? priv->windows->data : NULL);
      g_object_notify (G_OBJECT (application), "active-window");
    }
}

static void
extract_accel_from_menu_item (GMenuModel     *model,
                              gint            item,
                              GtkApplication *app)
{
  GMenuAttributeIter *iter;
  const gchar *key;
  GVariant *value;
  const gchar *accel = NULL;
  const gchar *action = NULL;
  GVariant *target = NULL;

  iter = g_menu_model_iterate_item_attributes (model, item);
  while (g_menu_attribute_iter_get_next (iter, &key, &value))
    {
      if (g_str_equal (key, "action") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        action = g_variant_get_string (value, NULL);
      else if (g_str_equal (key, "accel") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        accel = g_variant_get_string (value, NULL);
      else if (g_str_equal (key, "target"))
        target = g_variant_ref (value);
      g_variant_unref (value);
    }
  g_object_unref (iter);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (accel && action)
    gtk_application_add_accelerator (app, accel, action, target);
  G_GNUC_END_IGNORE_DEPRECATIONS

  if (target)
    g_variant_unref (target);
}

static void
extract_accels_from_menu (GMenuModel     *model,
                          GtkApplication *app)
{
  gint i;
  GMenuLinkIter *iter;
  const gchar *key;
  GMenuModel *m;

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    {
      extract_accel_from_menu_item (model, i, app);

      iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_get_next (iter, &key, &m))
        {
          extract_accels_from_menu (m, app);
          g_object_unref (m);
        }
      g_object_unref (iter);
    }
}

static void
gtk_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      g_value_set_boolean (value, application->priv->register_session);
      break;

    case PROP_APP_MENU:
      g_value_set_object (value, gtk_application_get_app_menu (application));
      break;

    case PROP_MENUBAR:
      g_value_set_object (value, gtk_application_get_menubar (application));
      break;

    case PROP_ACTIVE_WINDOW:
      g_value_set_object (value, gtk_application_get_active_window (application));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      application->priv->register_session = g_value_get_boolean (value);
      break;

    case PROP_APP_MENU:
      gtk_application_set_app_menu (application, g_value_get_object (value));
      break;

    case PROP_MENUBAR:
      gtk_application_set_menubar (application, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_finalize (GObject *object)
{
  GtkApplication *application = GTK_APPLICATION (object);

  g_clear_object (&application->priv->menus_builder);
  g_clear_object (&application->priv->app_menu);
  g_clear_object (&application->priv->menubar);
  g_clear_object (&application->priv->muxer);

  accels_finalize (&application->priv->accels);

  G_OBJECT_CLASS (gtk_application_parent_class)
    ->finalize (object);
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->get_property = gtk_application_get_property;
  object_class->set_property = gtk_application_set_property;
  object_class->finalize = gtk_application_finalize;

  application_class->local_command_line = gtk_application_local_command_line;
  application_class->add_platform_data = gtk_application_add_platform_data;
  application_class->before_emit = gtk_application_before_emit;
  application_class->after_emit = gtk_application_after_emit;
  application_class->startup = gtk_application_startup;
  application_class->shutdown = gtk_application_shutdown;

  class->window_added = gtk_application_window_added;
  class->window_removed = gtk_application_window_removed;

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

  /**
   * GtkApplication:register-session:
   *
   * Set this property to %TRUE to register with the session manager.
   *
   * Since: 3.4
   */
  g_object_class_install_property (object_class, PROP_REGISTER_SESSION,
    g_param_spec_boolean ("register-session",
                          P_("Register session"),
                          P_("Register with the session manager"),
                          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_APP_MENU,
    g_param_spec_object ("app-menu",
                         P_("Application menu"),
                         P_("The GMenuModel for the application menu"),
                         G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MENUBAR,
    g_param_spec_object ("menubar",
                         P_("Menubar"),
                         P_("The GMenuModel for the menubar"),
                         G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ACTIVE_WINDOW,
    g_param_spec_object ("active-window",
                         P_("Active window"),
                         P_("The window which most recently had focus"),
                         GTK_TYPE_WINDOW,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/**
 * gtk_application_new:
 * @application_id: (allow-none): The application ID.
 * @flags: the application flags
 *
 * Creates a new #GtkApplication instance.
 *
 * When using #GtkApplication, it is not necessary to call gtk_init()
 * manually. It is called as soon as the application gets registered as
 * the primary instance.
 *
 * Concretely, gtk_init() is called in the default handler for the
 * #GApplication::startup signal. Therefore, #GtkApplication subclasses should
 * chain up in their #GApplication::startup handler before using any GTK+ API.
 *
 * Note that commandline arguments are not passed to gtk_init().
 * All GTK+ functionality that is available via commandline arguments
 * can also be achieved by setting suitable environment variables
 * such as `G_DEBUG`, so this should not be a big
 * problem. If you absolutely must support GTK+ commandline arguments,
 * you can explicitly call gtk_init() before creating the application
 * instance.
 *
 * If non-%NULL, the application ID must be valid.  See
 * g_application_id_is_valid().
 *
 * If no application ID is given then some features (most notably application 
 * uniqueness) will be disabled. A null application ID is only allowed with 
 * GTK+ 3.6 or later.
 *
 * Returns: a new #GtkApplication instance
 *
 * Since: 3.0
 */
GtkApplication *
gtk_application_new (const gchar       *application_id,
                     GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id == NULL || g_application_id_is_valid (application_id), NULL);

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
 * Adds a window to @application.
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
 * Gets a list of the #GtkWindows associated with @application.
 *
 * The list is sorted by most recently focused window, such that the first
 * element is the currently focused window. (Useful for choosing a parent
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

/**
 * gtk_application_get_window_by_id:
 * @application: a #GtkApplication
 * @id: an identifier number
 *
 * Returns the #GtkApplicationWindow with the given ID.
 *
 * Returns: (transfer none): the window with ID @id, or
 *   %NULL if there is no window with this ID
 *
 * Since: 3.6
 */
GtkWindow *
gtk_application_get_window_by_id (GtkApplication *application,
                                  guint           id)
{
  GList *l;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  for (l = application->priv->windows; l != NULL; l = l->next) 
    {
      if (GTK_IS_APPLICATION_WINDOW (l->data) &&
          gtk_application_window_get_id (GTK_APPLICATION_WINDOW (l->data)) == id)
        return l->data;
    }

  return NULL;
}

/**
 * gtk_application_get_active_window:
 * @application: a #GtkApplication
 *
 * Gets the “active” window for the application.
 *
 * The active window is the one that was most recently focused (within
 * the application).  This window may not have the focus at the moment
 * if another application has it -- this is just the most
 * recently-focused window within this application.
 *
 * Returns: (transfer none): the active window
 *
 * Since: 3.6
 **/
GtkWindow *
gtk_application_get_active_window (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->windows ? application->priv->windows->data : NULL;
}

static void
gtk_application_update_accels (GtkApplication *application)
{
  GList *l;

  for (l = application->priv->windows; l != NULL; l = l->next)
    _gtk_window_notify_keys_changed (l->data);
}

/**
 * gtk_application_add_accelerator:
 * @application: a #GtkApplication
 * @accelerator: accelerator string
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameter to pass when activating the action,
 *   or %NULL if the action does not accept an activation parameter
 *
 * Installs an accelerator that will cause the named action
 * to be activated when the key combination specificed by @accelerator
 * is pressed.
 *
 * @accelerator must be a string that can be parsed by gtk_accelerator_parse(),
 * e.g. "<Primary>q" or “<Control><Alt>p”.
 *
 * @action_name must be the name of an action as it would be used
 * in the app menu, i.e. actions that have been added to the application
 * are referred to with an “app.” prefix, and window-specific actions
 * with a “win.” prefix.
 *
 * GtkApplication also extracts accelerators out of “accel” attributes
 * in the #GMenuModels passed to gtk_application_set_app_menu() and
 * gtk_application_set_menubar(), which is usually more convenient
 * than calling this function for each accelerator.
 *
 * Since: 3.4
 *
 * Deprecated: 3.14: Use gtk_application_set_accels_for_action() instead
 */
void
gtk_application_add_accelerator (GtkApplication *application,
                                 const gchar    *accelerator,
                                 const gchar    *action_name,
                                 GVariant       *parameter)
{
  const gchar *accelerators[2] = { accelerator, NULL };
  gchar *action_and_target;

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (action_name != NULL);
  g_return_if_fail (accelerator != NULL);

  action_and_target = gtk_print_action_and_target (NULL, action_name, parameter);
  accels_set_accels_for_action (&application->priv->accels, action_and_target, accelerators);
  gtk_action_muxer_set_primary_accel (application->priv->muxer, action_and_target, accelerator);
  gtk_application_update_accels (application);
  g_free (action_and_target);
}

/**
 * gtk_application_remove_accelerator:
 * @application: a #GtkApplication
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameter to pass when activating the action,
 *   or %NULL if the action does not accept an activation parameter
 *
 * Removes an accelerator that has been previously added
 * with gtk_application_add_accelerator().
 *
 * Since: 3.4
 *
 * Deprecated: 3.14: Use gtk_application_set_accels_for_action() instead
 */
void
gtk_application_remove_accelerator (GtkApplication *application,
                                    const gchar    *action_name,
                                    GVariant       *parameter)
{
  gchar *action_and_target;

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (action_name != NULL);

  action_and_target = gtk_print_action_and_target (NULL, action_name, parameter);
  accels_set_accels_for_action (&application->priv->accels, action_and_target, NULL);
  gtk_action_muxer_set_primary_accel (application->priv->muxer, action_and_target, NULL);
  gtk_application_update_accels (application);
  g_free (action_and_target);
}

/**
 * gtk_application_prefers_app_menu:
 * @application: a #GtkApplication
 *
 * Determines if the desktop environment in which the application is
 * running would prefer an application menu be shown.
 *
 * If this function returns %TRUE then the application should call
 * gtk_application_set_app_menu() with the contents of an application
 * menu, which will be shown by the desktop environment.  If it returns
 * %FALSE then you should consider using an alternate approach, such as
 * a menubar.
 *
 * The value returned by this function is purely advisory and you are
 * free to ignore it.  If you call gtk_application_set_app_menu() even
 * if the desktop environment doesn't support app menus, then a fallback
 * will be provided.
 *
 * Applications are similarly free not to set an app menu even if the
 * desktop environment wants to show one.  In that case, a fallback will
 * also be created by the desktop environment (GNOME, for example, uses
 * a menu with only a "Quit" item in it).
 *
 * The value returned by this function never changes.  Once it returns a
 * particular value, it is guaranteed to always return the same value.
 *
 * You may only call this function after the application has been
 * registered and after the base startup handler has run.  You're most
 * likely to want to use this from your own startup handler.  It may
 * also make sense to consult this function while constructing UI (in
 * activate, open or an action activation handler) in order to determine
 * if you should show a gear menu or not.
 *
 * This function will return %FALSE on Mac OS and a default app menu
 * will be created automatically with the "usual" contents of that menu
 * typical to most Mac OS applications.  If you call
 * gtk_application_set_app_menu() anyway, then this menu will be
 * replaced with your own.
 *
 * Returns: %TRUE if you should set an app menu
 *
 * Since: 3.14
 **/
gboolean
gtk_application_prefers_app_menu (GtkApplication *application)
{
  g_return_val_if_fail (application->priv->impl != NULL, FALSE);

  return gtk_application_impl_prefers_app_menu (application->priv->impl);
}

/**
 * gtk_application_set_app_menu:
 * @application: a #GtkApplication
 * @app_menu: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets or unsets the application menu for @application.
 *
 * This can only be done in the primary instance of the application,
 * after it has been registered.  #GApplication::startup is a good place
 * to call this.
 *
 * The application menu is a single menu containing items that typically
 * impact the application as a whole, rather than acting on a specific
 * window or document.  For example, you would expect to see
 * “Preferences” or “Quit” in an application menu, but not “Save” or
 * “Print”.
 *
 * If supported, the application menu will be rendered by the desktop
 * environment.
 *
 * Use the base #GActionMap interface to add actions, to respond to the user
 * selecting these menu items.
 *
 * Since: 3.4
 */
void
gtk_application_set_app_menu (GtkApplication *application,
                              GMenuModel     *app_menu)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (g_application_get_is_registered (G_APPLICATION (application)));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));

  if (app_menu != application->priv->app_menu)
    {
      if (application->priv->app_menu != NULL)
        g_object_unref (application->priv->app_menu);

      application->priv->app_menu = app_menu;

      if (application->priv->app_menu != NULL)
        g_object_ref (application->priv->app_menu);

      if (app_menu)
        extract_accels_from_menu (app_menu, application);

      gtk_application_impl_set_app_menu (application->priv->impl, app_menu);

      g_object_notify (G_OBJECT (application), "app-menu");
    }
}

/**
 * gtk_application_get_app_menu:
 * @application: a #GtkApplication
 *
 * Returns the menu model that has been set with
 * gtk_application_set_app_menu().
 *
 * Returns: (transfer none): the application menu of @application
 *
 * Since: 3.4
 */
GMenuModel *
gtk_application_get_app_menu (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->app_menu;
}

/**
 * gtk_application_set_menubar:
 * @application: a #GtkApplication
 * @menubar: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets or unsets the menubar for windows of @application.
 *
 * This is a menubar in the traditional sense.
 *
 * This can only be done in the primary instance of the application,
 * after it has been registered.  #GApplication::startup is a good place
 * to call this.
 *
 * Depending on the desktop environment, this may appear at the top of
 * each window, or at the top of the screen.  In some environments, if
 * both the application menu and the menubar are set, the application
 * menu will be presented as if it were the first item of the menubar.
 * Other environments treat the two as completely separate -- for
 * example, the application menu may be rendered by the desktop shell
 * while the menubar (if set) remains in each individual window.
 *
 * Use the base #GActionMap interface to add actions, to respond to the user
 * selecting these menu items.
 *
 * Since: 3.4
 */
void
gtk_application_set_menubar (GtkApplication *application,
                             GMenuModel     *menubar)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (g_application_get_is_registered (G_APPLICATION (application)));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));

  if (menubar != application->priv->menubar)
    {
      if (application->priv->menubar != NULL)
        g_object_unref (application->priv->menubar);

      application->priv->menubar = menubar;

      if (application->priv->menubar != NULL)
        g_object_ref (application->priv->menubar);

      if (menubar)
        extract_accels_from_menu (menubar, application);

      gtk_application_impl_set_menubar (application->priv->impl, menubar);

      g_object_notify (G_OBJECT (application), "menubar");
    }
}

/**
 * gtk_application_get_menubar:
 * @application: a #GtkApplication
 *
 * Returns the menu model that has been set with
 * gtk_application_set_menubar().
 *
 * Returns: (transfer none): the menubar for windows of @application
 *
 * Since: 3.4
 */
GMenuModel *
gtk_application_get_menubar (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->menubar;
}

/**
 * GtkApplicationInhibitFlags:
 * @GTK_APPLICATION_INHIBIT_LOGOUT: Inhibit ending the user session
 *     by logging out or by shutting down the computer
 * @GTK_APPLICATION_INHIBIT_SWITCH: Inhibit user switching
 * @GTK_APPLICATION_INHIBIT_SUSPEND: Inhibit suspending the
 *     session or computer
 * @GTK_APPLICATION_INHIBIT_IDLE: Inhibit the session being
 *     marked as idle (and possibly locked)
 *
 * Types of user actions that may be blocked by gtk_application_inhibit().
 *
 * Since: 3.4
 */

/**
 * gtk_application_inhibit:
 * @application: the #GtkApplication
 * @window: (allow-none): a #GtkWindow, or %NULL
 * @flags: what types of actions should be inhibited
 * @reason: (allow-none): a short, human-readable string that explains
 *     why these operations are inhibited
 *
 * Inform the session manager that certain types of actions should be
 * inhibited. This is not guaranteed to work on all platforms and for
 * all types of actions.
 *
 * Applications should invoke this method when they begin an operation
 * that should not be interrupted, such as creating a CD or DVD. The
 * types of actions that may be blocked are specified by the @flags
 * parameter. When the application completes the operation it should
 * call gtk_application_uninhibit() to remove the inhibitor. Note that
 * an application can have multiple inhibitors, and all of the must
 * be individually removed. Inhibitors are also cleared when the
 * application exits.
 *
 * Applications should not expect that they will always be able to block
 * the action. In most cases, users will be given the option to force
 * the action to take place.
 *
 * Reasons should be short and to the point.
 *
 * If @window is given, the session manager may point the user to
 * this window to find out more about why the action is inhibited.
 *
 * Returns: A non-zero cookie that is used to uniquely identify this
 *     request. It should be used as an argument to gtk_application_uninhibit()
 *     in order to remove the request. If the platform does not support
 *     inhibiting or the request failed for some reason, 0 is returned.
 *
 * Since: 3.4
 */
guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), 0);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), 0);

  return gtk_application_impl_inhibit (application->priv->impl, window, flags, reason);
}

/**
 * gtk_application_uninhibit:
 * @application: the #GtkApplication
 * @cookie: a cookie that was returned by gtk_application_inhibit()
 *
 * Removes an inhibitor that has been established with gtk_application_inhibit().
 * Inhibitors are also cleared when the application exits.
 *
 * Since: 3.4
 */
void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (cookie > 0);

  gtk_application_impl_uninhibit (application->priv->impl, cookie);
}

/**
 * gtk_application_is_inhibited:
 * @application: the #GtkApplication
 * @flags: what types of actions should be queried
 *
 * Determines if any of the actions specified in @flags are
 * currently inhibited (possibly by another application).
 *
 * Returns: %TRUE if any of the actions specified in @flags are inhibited
 *
 * Since: 3.4
 */
gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), FALSE);

  return gtk_application_impl_is_inhibited (application->priv->impl, flags);
}

GtkActionMuxer *
gtk_application_get_parent_muxer_for_window (GtkWindow *window)
{
  GtkApplication *application;

  application = gtk_window_get_application (window);

  if (!application)
    return NULL;

  return application->priv->muxer;
}

gboolean
gtk_application_activate_accel (GtkApplication  *application,
                                GActionGroup    *action_group,
                                guint            key,
                                GdkModifierType  modifier)
{
  return accels_activate (&application->priv->accels, action_group, key, modifier);
}

void
gtk_application_foreach_accel_keys (GtkApplication           *application,
                                    GtkWindow                *window,
                                    GtkWindowKeysForeachFunc  callback,
                                    gpointer                  user_data)
{
  accels_foreach_key (&application->priv->accels, window, callback, user_data);
}

/**
 * gtk_application_list_action_descriptions:
 * @application: a #GtkApplication
 *
 * Lists the detailed action names which have associated accelerators.
 * See gtk_application_set_accels_for_action().
 *
 * Returns: (transfer full): a %NULL-terminated array of strings,
 *     free with g_strfreev() when done
 *
 * Since: 3.12
 */
gchar **
gtk_application_list_action_descriptions (GtkApplication *application)
{
  GHashTableIter iter;
  gchar **result;
  gint n, i = 0;
  gpointer key;

  n = g_hash_table_size (application->priv->accels.action_to_accels);
  result = g_new (gchar *, n + 1);

  g_hash_table_iter_init (&iter, application->priv->accels.action_to_accels);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      const gchar *action_and_target = key;
      const gchar *sep;
      GVariant *target;

      sep = strrchr (action_and_target, '|');
      target = g_variant_parse (NULL, action_and_target, sep, NULL, NULL);
      result[i++] = g_action_print_detailed_name (sep + 1, target);
      if (target)
        g_variant_unref (target);
    }
  g_assert_cmpint (i, ==, n);
  result[i] = NULL;

  return result;
}

gchar *
normalise_detailed_name (const gchar *detailed_action_name)
{
  GError *error = NULL;
  gchar *action_and_target;
  gchar *action_name;
  GVariant *target;

  g_action_parse_detailed_name (detailed_action_name, &action_name, &target, &error);
  g_assert_no_error (error);

  action_and_target = gtk_print_action_and_target (NULL, action_name, target);

  if (target)
    g_variant_unref (target);

  g_free (action_name);

  return action_and_target;
}

/**
 * gtk_application_set_accels_for_action:
 * @application: a #GtkApplication
 * @detailed_action_name: a detailed action name, specifying an action
 *     and target to associate accelerators with
 * @accels: (array zero-terminated=1): a list of accelerators in the format understood by
 *     gtk_accelerator_parse()
 *
 * Sets zero or more keyboard accelerators that will trigger the
 * given action. The first item in @accels will be the primary 
 * accelerator, which may be displayed in the UI.
 *
 * To remove all accelerators for an action, use an empty, zero-terminated
 * array for @accels.
 *
 * Since: 3.12
 */
void
gtk_application_set_accels_for_action (GtkApplication      *application,
                                       const gchar         *detailed_action_name,
                                       const gchar * const *accels)
{
  gchar *action_and_target;

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (detailed_action_name != NULL);

  action_and_target = normalise_detailed_name (detailed_action_name);
  accels_set_accels_for_action (&application->priv->accels, action_and_target, accels);
  gtk_action_muxer_set_primary_accel (application->priv->muxer, action_and_target, accels[0]);
  gtk_application_update_accels (application);
  g_free (action_and_target);
}

/**
 * gtk_application_get_accels_for_action:
 * @application: a #GtkApplication
 * @detailed_action_name: a detailed action name, specifying an action
 *     and target to obtain accelerators for
 *
 * Gets the accelerators that are currently associated with
 * the given action.
 *
 * Returns: (transfer full): accelerators for @detailed_action_name, as
 *     a %NULL-terminated array. Free with g_strfreev() when no longer needed
 *
 * Since: 3.12
 */
gchar **
gtk_application_get_accels_for_action (GtkApplication *application,
                                       const gchar    *detailed_action_name)
{
  gchar *action_and_target;
  gchar **accels;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (detailed_action_name != NULL, NULL);

  action_and_target = normalise_detailed_name (detailed_action_name);
  accels = accels_get_accels_for_action (&application->priv->accels, action_and_target);
  g_free (action_and_target);

  return accels;
}

/**
 * gtk_application_get_actions_for_accel:
 * @application: a #GtkApplication
 * @accel: an accelerator that can be parsed by gtk_accelerator_parse()
 *
 * Returns the list of actions (possibly empty) that @accel maps to.
 * Each item in the list is a detailed action name in the usual form.
 *
 * This might be useful to discover if an accel already exists in
 * order to prevent installation of a conflicting accelerator (from
 * an accelerator editor or a plugin system, for example). Note that
 * having more than one action per accelerator may not be a bad thing
 * and might make sense in cases where the actions never appear in the
 * same context.
 *
 * In case there are no actions for a given accelerator, an empty array
 * is returned.  %NULL is never returned.
 *
 * It is a programmer error to pass an invalid accelerator string.
 * If you are unsure, check it with gtk_accelerator_parse() first.
 *
 * Returns: (transfer full): a %NULL-terminated array of actions for @accel
 *
 * Since: 3.14
 */
gchar **
gtk_application_get_actions_for_accel (GtkApplication *application,
                                       const gchar    *accel)
{
  const gchar * const *actions_and_targets;
  gchar **detailed_actions;
  AccelKey accel_key;
  guint i, n;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (accel != NULL, NULL);

  gtk_accelerator_parse (accel, &accel_key.key, &accel_key.modifier);

  if (accel_key.key == 0)
    {
      g_critical ("invalid accelerator string '%s'", accel);
      g_return_val_if_fail (accel_key.key != 0, NULL);
    }

  actions_and_targets = accels_get_actions_for_accel (&application->priv->accels, &accel_key);
  n = actions_and_targets ? g_strv_length ((gchar **) actions_and_targets) : 0;

  detailed_actions = g_new0 (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      const gchar *action_and_target = actions_and_targets[i];
      const gchar *sep;
      GVariant *target;

      sep = strrchr (action_and_target, '|');
      target = g_variant_parse (NULL, action_and_target, sep, NULL, NULL);
      detailed_actions[i] = g_action_print_detailed_name (sep + 1, target);
      if (target)
        g_variant_unref (target);
    }

  detailed_actions[n] = NULL;

  return detailed_actions;
}

GtkActionMuxer *
gtk_application_get_action_muxer (GtkApplication *application)
{
  g_assert (application->priv->muxer);

  return application->priv->muxer;
}

void
gtk_application_insert_action_group (GtkApplication *application,
                                     const gchar    *name,
                                     GActionGroup   *action_group)
{
  gtk_action_muxer_insert (application->priv->muxer, name, action_group);
}

void
gtk_application_handle_window_realize (GtkApplication *application,
                                       GtkWindow      *window)
{
  gtk_application_impl_handle_window_realize (application->priv->impl, window);
}

void
gtk_application_handle_window_map (GtkApplication *application,
                                   GtkWindow      *window)
{
  gtk_application_impl_handle_window_map (application->priv->impl, window);
}

/**
 * gtk_application_get_menu_by_id:
 * @application: a #GtkApplication
 * @id: the id of the menu to look up
 *
 * Gets a menu from automatically loaded resources.
 * See [Automatic resources][automatic-resources]
 * for more information.
 *
 * Returns: (transfer full): Gets the menu with the
 *     given id from the automatically loaded resources
 *
 * Since: 3.14
 */
GMenu *
gtk_application_get_menu_by_id (GtkApplication *application,
                                const gchar    *id)
{
  GObject *object;

  if (!application->priv->menus_builder)
    return NULL;

  object = gtk_builder_get_object (application->priv->menus_builder, id);

  if (!object || !G_IS_MENU (object))
    return NULL;

  return G_MENU (object);
}
