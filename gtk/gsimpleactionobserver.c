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

#include "gsimpleactionobserver.h"
#include "gactionobservable.h"

typedef GObjectClass GSimpleActionObserverClass;
struct _GSimpleActionObserver
{
  GObject parent_instance;

  GActionGroup *action_group;
  gchar *action_name;
  GVariant *target;

  gboolean can_activate;
  gboolean active;
  gboolean enabled;

  gint reporting;
};

static void g_simple_action_observer_init_iface (GActionObserverInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GSimpleActionObserver, g_simple_action_observer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_OBSERVER, g_simple_action_observer_init_iface));

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *g_simple_action_observer_pspecs[N_PROPS];

static void
g_simple_action_observer_action_added (GActionObserver    *g_observer,
                                       GActionObservable  *observable,
                                       const gchar        *action_name,
                                       const GVariantType *parameter_type,
                                       gboolean            enabled,
                                       GVariant           *state)
{
  GSimpleActionObserver *observer = G_SIMPLE_ACTION_OBSERVER (g_observer);
  gboolean active;

  /* we can only activate if we have the correct type of parameter */
  observer->can_activate = (observer->target == NULL && parameter_type == NULL) ||
                            (observer->target != NULL && parameter_type != NULL &&
                             g_variant_is_of_type (observer->target, parameter_type));

  if (observer->can_activate)
    {
      if (observer->target != NULL && state != NULL)
        active = g_variant_equal (state, observer->target);

      else if (state != NULL && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
        active = g_variant_get_boolean (state);

      else
        active = FALSE;

      if (active != observer->active)
        {
          observer->active = active;
          observer->reporting++;
          g_object_notify_by_pspec (G_OBJECT (observer), g_simple_action_observer_pspecs[PROP_ACTIVE]);
          observer->reporting--;
        }

      if (enabled != observer->enabled)
        {
          observer->enabled = enabled;
          g_object_notify_by_pspec (G_OBJECT (observer), g_simple_action_observer_pspecs[PROP_ENABLED]);
        }
    }
}

static void
g_simple_action_observer_action_enabled_changed (GActionObserver   *g_observer,
                                                 GActionObservable *observable,
                                                 const gchar       *action_name,
                                                 gboolean           enabled)
{
  GSimpleActionObserver *observer = G_SIMPLE_ACTION_OBSERVER (g_observer);

  if (!observer->can_activate)
    return;

  if (enabled != observer->enabled)
    {
      observer->enabled = enabled;
      g_object_notify_by_pspec (G_OBJECT (observer), g_simple_action_observer_pspecs[PROP_ENABLED]);
    }
}

static void
g_simple_action_observer_action_state_changed (GActionObserver   *g_observer,
                                               GActionObservable *observable,
                                               const gchar       *action_name,
                                               GVariant          *state)
{
  GSimpleActionObserver *observer = G_SIMPLE_ACTION_OBSERVER (g_observer);
  gboolean active = FALSE;

  if (!observer->can_activate)
    return;

  if (observer->target)
    active = g_variant_equal (state, observer->target);

  else if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    active = g_variant_get_boolean (state);

  if (active != observer->active)
    {
      observer->active = active;
      observer->reporting++;
      g_object_notify_by_pspec (G_OBJECT (observer), g_simple_action_observer_pspecs[PROP_ACTIVE]);
      observer->reporting--;
    }
}

static void
g_simple_action_observer_action_removed (GActionObserver   *g_observer,
                                         GActionObservable *observable,
                                         const gchar       *action_name)
{
  GSimpleActionObserver *observer = G_SIMPLE_ACTION_OBSERVER (g_observer);

  if (!observer->can_activate)
    return;

  observer->can_activate = FALSE;

  if (observer->active)
    {
      observer->active = FALSE;
      observer->reporting++;
      g_object_notify_by_pspec (G_OBJECT (observer), g_simple_action_observer_pspecs[PROP_ACTIVE]);
      observer->reporting--;
    }

  if (observer->enabled)
    {
      observer->enabled = FALSE;
      g_object_notify_by_pspec (G_OBJECT (observer), g_simple_action_observer_pspecs[PROP_ENABLED]);
    }
}

static void
g_simple_action_observer_get_property (GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec)
{
  GSimpleActionObserver *observer = G_SIMPLE_ACTION_OBSERVER (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, observer->active);
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, observer->enabled);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_simple_action_observer_finalize (GObject *object)
{
  GSimpleActionObserver *observer = G_SIMPLE_ACTION_OBSERVER (object);

  g_object_unref (observer->action_group);
  g_free (observer->action_name);

  if (observer->target)
    g_variant_unref (observer->target);

  G_OBJECT_CLASS (g_simple_action_observer_parent_class)
    ->finalize (object);
}

static void
g_simple_action_observer_init (GSimpleActionObserver *observer)
{
}

static void
g_simple_action_observer_init_iface (GActionObserverInterface *iface)
{
  iface->action_added = g_simple_action_observer_action_added;
  iface->action_enabled_changed = g_simple_action_observer_action_enabled_changed;
  iface->action_state_changed = g_simple_action_observer_action_state_changed;
  iface->action_removed = g_simple_action_observer_action_removed;
}

static void
g_simple_action_observer_class_init (GObjectClass *class)
{
  class->get_property = g_simple_action_observer_get_property;
  class->finalize = g_simple_action_observer_finalize;

  g_simple_action_observer_pspecs[PROP_ACTIVE] = g_param_spec_boolean ("active", "active", "active", FALSE,
                                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_simple_action_observer_pspecs[PROP_ENABLED] = g_param_spec_boolean ("enabled", "enabled", "enabled", FALSE,
                                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (class, N_PROPS, g_simple_action_observer_pspecs);
}

GSimpleActionObserver *
g_simple_action_observer_new (GActionObservable *observable,
                              const gchar       *action_name,
                              GVariant          *target)
{
  GSimpleActionObserver *observer;
  const GVariantType *type;
  gboolean enabled;
  GVariant *state;

  observer = g_object_new (G_TYPE_SIMPLE_ACTION_OBSERVER, NULL);
  observer->action_group = g_object_ref (observable);
  observer->action_name = g_strdup (action_name);
  if (target)
    observer->target = g_variant_ref_sink (target);

  g_action_observable_register_observer (observable, action_name, G_ACTION_OBSERVER (observer));

  if (g_action_group_query_action (observer->action_group, action_name, &enabled, &type, NULL, NULL, &state))
    {
      g_simple_action_observer_action_added (G_ACTION_OBSERVER (observer), observable,
                                             action_name, type, enabled, state);
      if (state)
        g_variant_unref (state);
    }

  return observer;
}

void
g_simple_action_observer_activate (GSimpleActionObserver *observer)
{
  g_return_if_fail (G_IS_SIMPLE_ACTION_OBSERVER (observer));

  if (observer->can_activate && !observer->reporting)
    g_action_group_activate_action (G_ACTION_GROUP (observer->action_group),
                                    observer->action_name, observer->target);
}

gboolean
g_simple_action_observer_get_active (GSimpleActionObserver *observer)
{
  g_return_val_if_fail (G_IS_SIMPLE_ACTION_OBSERVER (observer), FALSE);

  return observer->active;
}

gboolean
g_simple_action_observer_get_enabled (GSimpleActionObserver *observer)
{
  g_return_val_if_fail (G_IS_SIMPLE_ACTION_OBSERVER (observer), FALSE);

  return observer->enabled;
}
