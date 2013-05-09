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

#include "gtkactionhelper.h"
#include "gtkactionobservable.h"

#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

#include <string.h>

typedef struct
{
  GActionGroup *group;

  GHashTable *watchers;
} GtkActionHelperGroup;

static void             gtk_action_helper_action_added                  (GtkActionHelper    *helper,
                                                                         gboolean            enabled,
                                                                         const GVariantType *parameter_type,
                                                                         GVariant           *state,
                                                                         gboolean            should_emit_signals);

static void             gtk_action_helper_action_removed                (GtkActionHelper    *helper);

static void             gtk_action_helper_action_enabled_changed        (GtkActionHelper    *helper,
                                                                         gboolean            enabled);

static void             gtk_action_helper_action_state_changed          (GtkActionHelper    *helper,
                                                                         GVariant           *new_state);

typedef GObjectClass GtkActionHelperClass;

struct _GtkActionHelper
{
  GObject parent_instance;

  GtkApplication *application;
  GtkWidget *widget;

  GtkActionHelperGroup *group;

  GtkActionMuxer *action_context;
  gchar *action_name;

  GVariant *target;

  GtkActionHelperRole role;
  gboolean can_activate;
  gboolean enabled;
  gboolean active;

  gint reporting;
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

static void gtk_action_helper_observer_iface_init (GtkActionObserverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkActionHelper, gtk_action_helper, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVER, gtk_action_helper_observer_iface_init))

static void
gtk_action_helper_report_change (GtkActionHelper *helper,
                                 guint            prop_id)
{
  helper->reporting++;

  if (!helper->application)
    {
      switch (prop_id)
        {
        case PROP_ENABLED:
          gtk_widget_set_sensitive (GTK_WIDGET (helper->widget), helper->enabled);
          break;

        case PROP_ACTIVE:
          {
            GParamSpec *pspec;

            pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (helper->widget), "active");

            if (pspec && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN)
              g_object_set (G_OBJECT (helper->widget), "active", helper->active, NULL);
          }
          break;

        case PROP_ROLE:
          {
            GParamSpec *pspec;

            pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (helper->widget), "action-role");

            if (pspec && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_UINT)
              g_object_set (G_OBJECT (helper->widget), "action-role", helper->role, NULL);
          }
          break;

        default:
          g_assert_not_reached ();
        }
    }

  g_object_notify_by_pspec (G_OBJECT (helper), gtk_action_helper_pspecs[prop_id]);
  helper->reporting--;
}

static void
gtk_action_helper_action_added (GtkActionHelper    *helper,
                                gboolean            enabled,
                                const GVariantType *parameter_type,
                                GVariant           *state,
                                gboolean            should_emit_signals)
{
  /* we can only activate if we have the correct type of parameter */
  helper->can_activate = (helper->target == NULL && parameter_type == NULL) ||
                          (helper->target != NULL && parameter_type != NULL &&
                          g_variant_is_of_type (helper->target, parameter_type));

  if (!helper->can_activate)
    return;

  helper->enabled = enabled;

  if (helper->target != NULL && state != NULL)
    {
      helper->active = g_variant_equal (state, helper->target);
      helper->role = GTK_ACTION_HELPER_ROLE_RADIO;
    }

  else if (state != NULL && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    {
      helper->active = g_variant_get_boolean (state);
      helper->role = GTK_ACTION_HELPER_ROLE_TOGGLE;
    }

  if (should_emit_signals)
    {
      if (helper->enabled)
        gtk_action_helper_report_change (helper, PROP_ENABLED);

      if (helper->active)
        gtk_action_helper_report_change (helper, PROP_ACTIVE);

      if (helper->role)
        gtk_action_helper_report_change (helper, PROP_ROLE);
    }
}

static void
gtk_action_helper_action_removed (GtkActionHelper *helper)
{
  if (!helper->can_activate)
    return;

  helper->can_activate = FALSE;

  if (helper->enabled)
    {
      helper->enabled = FALSE;
      gtk_action_helper_report_change (helper, PROP_ENABLED);
    }

  if (helper->active)
    {
      helper->active = FALSE;
      gtk_action_helper_report_change (helper, PROP_ACTIVE);
    }

  if (helper->role)
    {
      helper->role = GTK_ACTION_HELPER_ROLE_NORMAL;
      gtk_action_helper_report_change (helper, PROP_ROLE);
    }
}

static void
gtk_action_helper_action_enabled_changed (GtkActionHelper *helper,
                                          gboolean         enabled)
{
  if (!helper->can_activate)
    return;

  if (helper->enabled == enabled)
    return;

  helper->enabled = enabled;
  gtk_action_helper_report_change (helper, PROP_ENABLED);
}

static void
gtk_action_helper_action_state_changed (GtkActionHelper *helper,
                                        GVariant        *new_state)
{
  gboolean was_active;

  if (!helper->can_activate)
    return;

  was_active = helper->active;

  if (helper->target)
    helper->active = g_variant_equal (new_state, helper->target);

  else if (g_variant_is_of_type (new_state, G_VARIANT_TYPE_BOOLEAN))
    helper->active = g_variant_get_boolean (new_state);

  else
    helper->active = FALSE;

  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);
}

static void
gtk_action_helper_get_property (GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
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
      g_value_set_uint (value, helper->role);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gtk_action_helper_finalize (GObject *object)
{
  GtkActionHelper *helper = GTK_ACTION_HELPER (object);

  if (helper->application)
    {
      g_signal_handlers_disconnect_by_data (helper->application, helper);
      g_object_unref (helper->action_context);
      g_object_unref (helper->application);

      if (helper->widget)
          g_object_unref (helper->widget);
    }

  g_free (helper->action_name);

  if (helper->target)
    g_variant_unref (helper->target);

  G_OBJECT_CLASS (gtk_action_helper_parent_class)
    ->finalize (object);
}

static void
gtk_action_helper_observer_action_added (GtkActionObserver   *observer,
                                         GtkActionObservable *observable,
                                         const gchar         *action_name,
                                         const GVariantType  *parameter_type,
                                         gboolean             enabled,
                                         GVariant            *state)
{
  gtk_action_helper_action_added (GTK_ACTION_HELPER (observer), enabled, parameter_type, state, TRUE);
}

static void
gtk_action_helper_observer_action_enabled_changed (GtkActionObserver   *observer,
                                                   GtkActionObservable *observable,
                                                   const gchar         *action_name,
                                                   gboolean             enabled)
{
  gtk_action_helper_action_enabled_changed (GTK_ACTION_HELPER (observer), enabled);
}

static void
gtk_action_helper_observer_action_state_changed (GtkActionObserver   *observer,
                                                 GtkActionObservable *observable,
                                                 const gchar         *action_name,
                                                 GVariant            *state)
{
  gtk_action_helper_action_state_changed (GTK_ACTION_HELPER (observer), state);
}

static void
gtk_action_helper_observer_action_removed (GtkActionObserver   *observer,
                                           GtkActionObservable *observable,
                                           const gchar         *action_name)
{
  gtk_action_helper_action_removed (GTK_ACTION_HELPER (observer));
}

static void
gtk_action_helper_init (GtkActionHelper *helper)
{
}

static void
gtk_action_helper_class_init (GtkActionHelperClass *class)
{
  class->get_property = gtk_action_helper_get_property;
  class->finalize = gtk_action_helper_finalize;

  gtk_action_helper_pspecs[PROP_ENABLED] = g_param_spec_boolean ("enabled", "enabled", "enabled", FALSE,
                                                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  gtk_action_helper_pspecs[PROP_ACTIVE] = g_param_spec_boolean ("active", "active", "active", FALSE,
                                                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  gtk_action_helper_pspecs[PROP_ROLE] = g_param_spec_uint ("role", "role", "role", 0, 2, 0,
                                                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (class, N_PROPS, gtk_action_helper_pspecs);
}

static void
gtk_action_helper_observer_iface_init (GtkActionObserverInterface *iface)
{
  iface->action_added = gtk_action_helper_observer_action_added;
  iface->action_enabled_changed = gtk_action_helper_observer_action_enabled_changed;
  iface->action_state_changed = gtk_action_helper_observer_action_state_changed;
  iface->action_removed = gtk_action_helper_observer_action_removed;
}

/*< private >
 * gtk_action_helper_new:
 * @widget: a #GtkWidget implementing #GtkActionable
 *
 * Creates a helper to track the state of a named action.  This will
 * usually be used by widgets implementing #GtkActionable.
 *
 * This helper class is usually used by @widget itself.  In order to
 * avoid reference cycles, the helper does not hold a reference on
 * @widget, but will assume that it continues to exist for the duration
 * of the life of the helper.  If you are using the helper from outside
 * of the widget, you should take a ref on @widget for each ref you hold
 * on the helper.
 *
 * Returns: a new #GtkActionHelper
 */
GtkActionHelper *
gtk_action_helper_new (GtkActionable *widget)
{
  GtkActionHelper *helper;

  g_return_val_if_fail (GTK_IS_ACTIONABLE (widget), NULL);
  helper = g_object_new (GTK_TYPE_ACTION_HELPER, NULL);

  helper->widget = GTK_WIDGET (widget);

  if (helper->widget)
    {
      GParamSpec *pspec;

      helper->enabled = gtk_widget_get_sensitive (GTK_WIDGET (helper->widget));

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (helper->widget), "active");
      if (pspec && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN)
        g_object_get (G_OBJECT (helper->widget), "active", &helper->active, NULL);
    }

  helper->action_context = _gtk_widget_get_action_muxer (GTK_WIDGET (widget));

  return helper;
}

static void
gtk_action_helper_active_window_changed (GObject    *object,
                                         GParamSpec *pspec,
                                         gpointer    user_data)
{
  GtkActionHelper *helper = user_data;
  GtkActionMuxer *parent;

  if (helper->widget)
    g_object_unref (helper->widget);

  helper->widget = GTK_WIDGET (gtk_application_get_active_window (helper->application));

  if (helper->widget)
    {
      parent = g_object_ref (_gtk_widget_get_action_muxer (GTK_WIDGET (helper->widget)));
      g_object_ref (helper->widget);
    }
  else
    {
      parent = gtk_action_muxer_new ();
      gtk_action_muxer_insert (parent, "app", G_ACTION_GROUP (helper->application));
    }

  gtk_action_muxer_set_parent (helper->action_context, parent);
  g_object_unref (parent);
}

GtkActionHelper *
gtk_action_helper_new_with_application (GtkApplication *application)
{
  GtkActionHelper *helper;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  helper = g_object_new (GTK_TYPE_ACTION_HELPER, NULL);
  helper->application = g_object_ref (application);

  helper->action_context = gtk_action_muxer_new ();
  g_signal_connect (application, "notify::active-window", G_CALLBACK (gtk_action_helper_active_window_changed), helper);
  gtk_action_helper_active_window_changed (NULL, NULL, helper);

  return helper;
}

void
gtk_action_helper_set_action_name (GtkActionHelper *helper,
                                   const gchar     *action_name)
{
  gboolean was_enabled, was_active;
  GtkActionHelperRole old_role;
  const GVariantType *parameter_type;
  gboolean enabled;
  GVariant *state;

  if (g_strcmp0 (action_name, helper->action_name) == 0)
    return;

  if (helper->action_name)
    {
      gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (helper->action_context),
                                                 helper->action_name,
                                                 GTK_ACTION_OBSERVER (helper));
      g_free (helper->action_name);
    }

  helper->action_name = g_strdup (action_name);

  gtk_action_observable_register_observer (GTK_ACTION_OBSERVABLE (helper->action_context),
                                           helper->action_name,
                                           GTK_ACTION_OBSERVER (helper));

  /* Start by recording the current state of our properties so we know
   * what notify signals we will need to send.
   */
  was_enabled = helper->enabled;
  was_active = helper->active;
  old_role = helper->role;

  if (g_action_group_query_action (G_ACTION_GROUP (helper->action_context), helper->action_name,
                                   &enabled, &parameter_type, NULL, NULL, &state))
    {
      gtk_action_helper_action_added (helper, enabled, parameter_type, state, FALSE);

      if (state)
        g_variant_unref (state);
    }
  else
    {
      helper->enabled = FALSE;
    }

  /* Send the notifies for the properties that changed.
   *
   * When called during construction, widget is NULL.  We don't need to
   * report in that case.
   */
  if (helper->enabled != was_enabled)
    gtk_action_helper_report_change (helper, PROP_ENABLED);

  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);

  if (helper->role != old_role)
    gtk_action_helper_report_change (helper, PROP_ROLE);

  if (!helper->application)
    g_object_notify (G_OBJECT (helper->widget), "action-name");
}

/*< private >
 * gtk_action_helper_set_action_target_value:
 * @helper: a #GtkActionHelper
 * @target_value: an action target, as per #GtkActionable
 *
 * This function consumes @action_target if it is floating.
 */
void
gtk_action_helper_set_action_target_value (GtkActionHelper *helper,
                                           GVariant        *target_value)
{
  gboolean was_enabled;
  gboolean was_active;

  if (target_value == helper->target)
    return;

  if (target_value && helper->target && g_variant_equal (target_value, helper->target))
    {
      g_variant_unref (g_variant_ref_sink (target_value));
      return;
    }

  if (helper->target)
    {
      g_variant_unref (helper->target);
      helper->target = NULL;
    }

  if (target_value)
    helper->target = g_variant_ref_sink (target_value);

  /* The action_name has not yet been set.  Don't do anything yet. */
  if (helper->action_name == NULL)
    return;

  was_enabled = helper->enabled;
  was_active = helper->active;

  /* If we are attached to an action group then it is possible that this
   * change of the target value could impact our properties (including
   * changes to 'can_activate' and therefore 'enabled', due to resolving
   * a parameter type mismatch).
   *
   * Start over again by pretending the action gets re-added.
   */
  helper->can_activate = FALSE;
  helper->enabled = FALSE;
  helper->active = FALSE;

  if (helper->action_context)
    {
      const GVariantType *parameter_type;
      gboolean enabled;
      GVariant *state;

      if (g_action_group_query_action (G_ACTION_GROUP (helper->action_context),
                                       helper->action_name, &enabled, &parameter_type,
                                       NULL, NULL, &state))
        {
          gtk_action_helper_action_added (helper, enabled, parameter_type, state, FALSE);

          if (state)
            g_variant_unref (state);
        }
    }

  if (helper->enabled != was_enabled)
    gtk_action_helper_report_change (helper, PROP_ENABLED);

  if (helper->active != was_active)
    gtk_action_helper_report_change (helper, PROP_ACTIVE);

  if (!helper->application)
    g_object_notify (G_OBJECT (helper->widget), "action-target");
}

const gchar *
gtk_action_helper_get_action_name (GtkActionHelper *helper)
{
  if (helper == NULL)
    return NULL;

  return helper->action_name;
}

GVariant *
gtk_action_helper_get_action_target_value (GtkActionHelper *helper)
{
  if (helper == NULL)
    return NULL;

  return helper->target;
}

GtkActionHelperRole
gtk_action_helper_get_role (GtkActionHelper *helper)
{
  if (helper == NULL)
    return GTK_ACTION_HELPER_ROLE_NORMAL;

  return helper->role;
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

  if (!helper->can_activate || helper->reporting)
    return;

  g_action_group_activate_action (G_ACTION_GROUP (helper->action_context),
                                  helper->action_name, helper->target);
}
