/*
 * Copyright © 2012 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkactionhelperprivate.h"
#include "gtkactionobservableprivate.h"
#include "gtkactionobserverprivate.h"
#include "gtkactiontreeprivate.h"

#include "gtkdebug.h"
#include "gtkmodelbuttonprivate.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include <string.h>

typedef GObjectClass GtkActionHelperClass;

struct _GtkActionHelper
{
  GObject parent_instance;

  GtkWidget        *widget;
  GtkActionMuxer   *action_context;
  GtkActionNode    *action_node;
  GtkActionBinding *binding;
  char             *action_name;
  GVariant         *target;

  GtkButtonRole     role;

  guint             can_activate : 1;
  guint             enabled : 1;
  guint             active : 1;
  guint             changing : 1;
  guint             legacy_subscribed : 1;

  int               reporting;
};

enum
{
  PROP_0,
  PROP_ENABLED,
  PROP_ACTIVE,
  PROP_ROLE,
  N_PROPS
};

static GParamSpec *gtk_action_helper_pspecs[N_PROPS];
static GParamSpec *action_name_pspec;
static GParamSpec *action_target_pspec;

static void gtk_action_helper_report_change (GtkActionHelper *helper,
                                             guint            prop_id);
static void gtk_action_helper_observer_iface_init (GtkActionObserverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkActionHelper, gtk_action_helper, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVER,
                                                gtk_action_helper_observer_iface_init))

static void
gtk_action_helper_apply_legacy_state (GtkActionHelper    *helper,
                                      gboolean            enabled,
                                      const GVariantType *parameter_type,
                                      GVariant           *state,
                                      gboolean            notify)
{
  gboolean was_enabled = helper->enabled;
  gboolean was_active = helper->active;
  GtkButtonRole was_role = helper->role;

  helper->can_activate = ((helper->target == NULL && parameter_type == NULL) ||
                          (helper->target != NULL && parameter_type != NULL &&
                           g_variant_is_of_type (helper->target, parameter_type)));
  helper->enabled = helper->can_activate && enabled;
  helper->active = FALSE;
  helper->role = GTK_BUTTON_ROLE_NORMAL;

  if (helper->can_activate && helper->target != NULL && state != NULL)
    {
      helper->active = g_variant_equal (state, helper->target);
      helper->role = GTK_BUTTON_ROLE_RADIO;
    }
  else if (helper->can_activate && state != NULL &&
           g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    {
      helper->active = g_variant_get_boolean (state);
      helper->role = GTK_BUTTON_ROLE_CHECK;
    }

  if (!notify)
    return;
  if (helper->enabled != was_enabled)
    gtk_action_helper_report_change (helper, PROP_ENABLED);
  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);
  if (helper->role != was_role)
    gtk_action_helper_report_change (helper, PROP_ROLE);
}

static void
gtk_action_helper_observer_action_added (GtkActionObserver   *observer,
                                         GtkActionObservable *observable,
                                         const char          *action_name,
                                         const GVariantType  *parameter_type,
                                         gboolean             enabled,
                                         GVariant            *state)
{
  gtk_action_helper_apply_legacy_state (GTK_ACTION_HELPER (observer),
                                        enabled, parameter_type, state, TRUE);
}

static void
gtk_action_helper_observer_action_removed (GtkActionObserver   *observer,
                                           GtkActionObservable *observable,
                                           const char          *action_name)
{
  GtkActionHelper *helper = GTK_ACTION_HELPER (observer);

  gtk_action_helper_apply_legacy_state (helper, FALSE, NULL, NULL, TRUE);
}

static void
gtk_action_helper_observer_action_enabled_changed (GtkActionObserver   *observer,
                                                   GtkActionObservable *observable,
                                                   const char          *action_name,
                                                   gboolean             enabled)
{
  GtkActionHelper *helper = GTK_ACTION_HELPER (observer);

  if (helper->can_activate && helper->enabled != enabled)
    {
      helper->enabled = enabled;
      gtk_action_helper_report_change (helper, PROP_ENABLED);
    }
}

static void
gtk_action_helper_observer_action_state_changed (GtkActionObserver   *observer,
                                                 GtkActionObservable *observable,
                                                 const char          *action_name,
                                                 GVariant            *state)
{
  GtkActionHelper *helper = GTK_ACTION_HELPER (observer);
  const GVariantType *parameter_type;
  gboolean enabled;
  GVariant *current_state = NULL;

  if (gtk_action_muxer_query_action (helper->action_context, helper->action_name,
                                     &enabled, &parameter_type, NULL, NULL,
                                     &current_state))
    gtk_action_helper_apply_legacy_state (helper, enabled, parameter_type,
                                          current_state, TRUE);
  g_clear_pointer (&current_state, g_variant_unref);
}

static void
gtk_action_helper_observer_iface_init (GtkActionObserverInterface *iface)
{
  iface->action_added = gtk_action_helper_observer_action_added;
  iface->action_removed = gtk_action_helper_observer_action_removed;
  iface->action_enabled_changed = gtk_action_helper_observer_action_enabled_changed;
  iface->action_state_changed = gtk_action_helper_observer_action_state_changed;
}

static void
gtk_action_helper_report_change (GtkActionHelper *helper,
                                 guint            prop_id)
{
  helper->reporting++;

  switch (prop_id)
    {
    case PROP_ENABLED:
      gtk_widget_set_sensitive (helper->widget, helper->enabled);
      break;

    case PROP_ACTIVE:
      {
        GParamSpec *pspec;

        pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (helper->widget), "active");
        if (pspec != NULL && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN)
          g_object_set (helper->widget, "active", helper->active, NULL);
      }
      break;

    case PROP_ROLE:
      {
        GParamSpec *pspec;

        pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (helper->widget), "role");
        if (pspec != NULL && G_PARAM_SPEC_VALUE_TYPE (pspec) == GTK_TYPE_BUTTON_ROLE)
          g_object_set (helper->widget, "role", helper->role, NULL);
      }
      break;

    default:
      g_assert_not_reached ();
    }

  g_object_notify_by_pspec (G_OBJECT (helper), gtk_action_helper_pspecs[prop_id]);
  helper->reporting--;
}

static void
gtk_action_helper_binding_changed (GtkActionBinding            *binding,
                                   GtkActionChange              changed,
                                   const GtkActionBindingState *state,
                                   gpointer                     user_data)
{
  GtkActionHelper *helper = user_data;
  gboolean was_enabled = helper->enabled;
  gboolean was_active = helper->active;
  GtkButtonRole was_role = helper->role;

  helper->can_activate = state->activatable;
  helper->enabled = state->enabled;
  helper->active = state->active;
  helper->role = state->role;

  if (state->present && !state->activatable &&
      (changed & (GTK_ACTION_CHANGE_PRESENT | GTK_ACTION_CHANGE_SIGNATURE)) != 0)
    {
      const GtkActionSnapshot *snapshot;
      GtkActionProvider *provider;

      provider = gtk_action_node_resolve (helper->action_node,
                                          gtk_action_binding_get_key (binding));
      snapshot = provider != NULL ? gtk_action_provider_get_snapshot (provider) : NULL;
      g_warning ("actionhelper: action %s can't be activated due to parameter type mismatch "
                 "(parameter type %s, target type %s)",
                 helper->action_name,
                 snapshot != NULL && snapshot->parameter_type != NULL
                   ? g_variant_type_peek_string (snapshot->parameter_type) : "NULL",
                 helper->target != NULL ? g_variant_get_type_string (helper->target) : "NULL");
    }

  if (helper->changing)
    return;

  if (helper->enabled != was_enabled)
    gtk_action_helper_report_change (helper, PROP_ENABLED);
  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);
  if (helper->role != was_role)
    gtk_action_helper_report_change (helper, PROP_ROLE);
}

static void
gtk_action_helper_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkActionHelper *helper = GTK_ACTION_HELPER (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, helper->enabled);
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, helper->active);
      break;

    case PROP_ROLE:
      g_value_set_enum (value, helper->role);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_action_helper_finalize (GObject *object)
{
  GtkActionHelper *helper = GTK_ACTION_HELPER (object);

  if (helper->legacy_subscribed)
    gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (helper->action_context),
                                               helper->action_name,
                                               GTK_ACTION_OBSERVER (helper));
  g_clear_pointer (&helper->binding, gtk_action_binding_cancel);
  g_clear_pointer (&helper->target, g_variant_unref);
  g_clear_pointer (&helper->action_name, g_free);

  G_OBJECT_CLASS (gtk_action_helper_parent_class)->finalize (object);
}

static void
gtk_action_helper_init (GtkActionHelper *helper)
{
  helper->role = GTK_BUTTON_ROLE_NORMAL;
}

static void
gtk_action_helper_class_init (GtkActionHelperClass *class)
{
  gpointer actionable_iface = g_type_default_interface_ref (GTK_TYPE_ACTIONABLE);

  action_name_pspec = g_object_interface_find_property (actionable_iface, "action-name");
  action_target_pspec = g_object_interface_find_property (actionable_iface, "action-target");
  g_type_default_interface_unref (actionable_iface);

  class->get_property = gtk_action_helper_get_property;
  class->finalize = gtk_action_helper_finalize;

  gtk_action_helper_pspecs[PROP_ENABLED] =
    g_param_spec_boolean ("enabled", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_NAME));
  gtk_action_helper_pspecs[PROP_ACTIVE] =
    g_param_spec_boolean ("active", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_NAME));
  gtk_action_helper_pspecs[PROP_ROLE] =
    g_param_spec_enum ("role", NULL, NULL, GTK_TYPE_BUTTON_ROLE,
                       GTK_BUTTON_ROLE_NORMAL,
                       (G_PARAM_READABLE | G_PARAM_STATIC_NAME));
  g_object_class_install_properties (class, N_PROPS, gtk_action_helper_pspecs);
}

GtkActionHelper *
gtk_action_helper_new (GtkActionable *widget)
{
  GtkActionHelper *helper;
  GParamSpec *pspec;
  gboolean active = FALSE;

  g_return_val_if_fail (GTK_IS_ACTIONABLE (widget), NULL);

  helper = g_object_new (GTK_TYPE_ACTION_HELPER, NULL);
  helper->widget = GTK_WIDGET (widget);
  helper->enabled = gtk_widget_get_sensitive (helper->widget);

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (helper->widget), "active");
  if (pspec != NULL && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN)
    {
      g_object_get (helper->widget, "active", &active, NULL);
      helper->active = active;
    }

  helper->action_node = _gtk_widget_get_action_node (helper->widget, TRUE);
  helper->action_context = _gtk_widget_get_action_muxer (helper->widget, TRUE);

  return helper;
}

void
gtk_action_helper_set_action_name (GtkActionHelper *helper,
                                   const char      *action_name)
{
  GtkActionKey *key = NULL;
  GtkActionBinding *old_binding;
  gboolean was_enabled;
  gboolean was_active;

  g_return_if_fail (GTK_IS_ACTION_HELPER (helper));

  if (g_strcmp0 (action_name, helper->action_name) == 0)
    return;

  if (GTK_DEBUG_CHECK (ACTIONS) &&
      (action_name == NULL || strchr (action_name, '.') == NULL))
    gdk_debug_message ("actionhelper: action name %s doesn't look like 'app.' or 'win.'; "
                       "it is unlikely to work", action_name);

  was_enabled = helper->enabled;
  was_active = helper->active;
  old_binding = helper->binding;
  helper->binding = NULL;
  helper->changing = TRUE;
  if (helper->legacy_subscribed)
    {
      gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (helper->action_context),
                                                 helper->action_name,
                                                 GTK_ACTION_OBSERVER (helper));
      helper->legacy_subscribed = FALSE;
    }
  g_free (helper->action_name);
  helper->action_name = g_strdup (action_name);

  if (action_name != NULL && (key = gtk_action_key_new (action_name)) != NULL)
    {
      helper->binding = gtk_action_node_bind (helper->action_node,
                                              key,
                                              helper->target != NULL
                                                ? g_variant_ref (helper->target) : NULL,
                                              (GTK_ACTION_INTEREST_PRESENT |
                                               GTK_ACTION_INTEREST_ENABLED |
                                               GTK_ACTION_INTEREST_ACTIVE |
                                               GTK_ACTION_INTEREST_ROLE),
                                              gtk_action_helper_binding_changed,
                                              helper,
                                              NULL);
      if (helper->binding != NULL)
        gtk_action_binding_set_owner_location (helper->binding, &helper->binding);
    }
  else
    {
      helper->can_activate = FALSE;
      helper->enabled = FALSE;
      helper->active = FALSE;
      helper->role = GTK_BUTTON_ROLE_NORMAL;
    }

  if (helper->binding != NULL &&
      !gtk_action_binding_get_state (helper->binding)->present)
    {
      const GVariantType *parameter_type = NULL;
      GVariant *state = NULL;
      gboolean enabled = FALSE;

      if (gtk_action_observable_subscribe (GTK_ACTION_OBSERVABLE (helper->action_context),
                                           helper->action_name,
                                           GTK_ACTION_OBSERVER (helper),
                                           &enabled, &parameter_type, &state))
        {
          gtk_action_helper_apply_legacy_state (helper, enabled, parameter_type, state, FALSE);
        }
      gtk_action_binding_cancel (helper->binding);
      helper->binding = NULL;
      helper->legacy_subscribed = TRUE;
      g_clear_pointer (&state, g_variant_unref);
    }

  if (old_binding != NULL)
    gtk_action_binding_cancel (old_binding);
  helper->changing = FALSE;

  if (helper->enabled != was_enabled)
    gtk_action_helper_report_change (helper, PROP_ENABLED);
  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);

  g_object_notify_by_pspec (G_OBJECT (helper->widget), action_name_pspec);

  g_clear_pointer (&key, gtk_action_key_unref);
}

void
gtk_action_helper_set_action_target_value (GtkActionHelper *helper,
                                           GVariant        *target_value)
{
  gboolean was_enabled;
  gboolean was_active;

  g_return_if_fail (GTK_IS_ACTION_HELPER (helper));

  if (target_value == helper->target)
    return;
  if (target_value != NULL && helper->target != NULL &&
      g_variant_equal (target_value, helper->target))
    {
      g_variant_unref (g_variant_ref_sink (target_value));
      return;
    }

  was_enabled = helper->enabled;
  was_active = helper->active;
  g_clear_pointer (&helper->target, g_variant_unref);
  helper->target = target_value != NULL ? g_variant_ref_sink (target_value) : NULL;

  if (helper->binding != NULL)
    {
      helper->changing = TRUE;
      gtk_action_binding_set_target (helper->binding,
                                     helper->target != NULL
                                       ? g_variant_ref (helper->target) : NULL);
      helper->changing = FALSE;
    }
  else if (helper->legacy_subscribed)
    {
      const GVariantType *parameter_type = NULL;
      GVariant *state = NULL;
      gboolean enabled = FALSE;

      if (gtk_action_muxer_query_action (helper->action_context, helper->action_name,
                                         &enabled, &parameter_type, NULL, NULL, &state))
        gtk_action_helper_apply_legacy_state (helper, enabled, parameter_type, state, FALSE);
      g_clear_pointer (&state, g_variant_unref);
    }

  if (helper->enabled != was_enabled)
    gtk_action_helper_report_change (helper, PROP_ENABLED);
  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);

  g_object_notify_by_pspec (G_OBJECT (helper->widget), action_target_pspec);
}

const char *
gtk_action_helper_get_action_name (GtkActionHelper *helper)
{
  return helper != NULL ? helper->action_name : NULL;
}

GVariant *
gtk_action_helper_get_action_target_value (GtkActionHelper *helper)
{
  return helper != NULL ? helper->target : NULL;
}

gboolean
gtk_action_helper_get_enabled (GtkActionHelper *helper)
{
  g_return_val_if_fail (GTK_IS_ACTION_HELPER (helper), FALSE);

  return helper->enabled;
}

gboolean
gtk_action_helper_get_active (GtkActionHelper *helper)
{
  g_return_val_if_fail (GTK_IS_ACTION_HELPER (helper), FALSE);

  return helper->active;
}

void
gtk_action_helper_activate (GtkActionHelper *helper)
{
  g_return_if_fail (GTK_IS_ACTION_HELPER (helper));

  if (helper->reporting != 0)
    return;

  if (helper->binding != NULL)
    gtk_action_binding_activate (helper->binding);
  else if (helper->legacy_subscribed && helper->can_activate)
    gtk_action_muxer_activate_action (helper->action_context,
                                      helper->action_name,
                                      helper->target);
}

GtkButtonRole
gtk_action_helper_get_role (GtkActionHelper *helper)
{
  g_return_val_if_fail (GTK_IS_ACTION_HELPER (helper), GTK_BUTTON_ROLE_NORMAL);

  return helper->role;
}
