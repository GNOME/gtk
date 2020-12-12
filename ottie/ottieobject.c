/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "ottieobjectprivate.h"

#include "ottieintl.h"

enum {
  PROP_0,
  PROP_MATCH_NAME,
  PROP_NAME,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (OttieObject, ottie_object, G_TYPE_OBJECT)

static void
ottie_object_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)

{
  OttieObject *self = OTTIE_OBJECT (object);

  switch (prop_id)
    {
    case PROP_MATCH_NAME:
      ottie_object_set_match_name (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      ottie_object_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_object_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  OttieObject *self = OTTIE_OBJECT (object);

  switch (prop_id)
    {
    case PROP_MATCH_NAME:
      g_value_set_string (value, self->match_name);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_object_dispose (GObject *object)
{
  OttieObject *self = OTTIE_OBJECT (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->match_name, g_free);

  G_OBJECT_CLASS (ottie_object_parent_class)->dispose (object);
}

static void
ottie_object_class_init (OttieObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = ottie_object_set_property;
  gobject_class->get_property = ottie_object_get_property;
  gobject_class->dispose = ottie_object_dispose;

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         P_("Name"),
                         P_("User-given name"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_MATCH_NAME] =
    g_param_spec_string ("match-name",
                         P_("Match name"),
                         P_("Name for matching in scripts"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
ottie_object_init (OttieObject *self)
{
}

void
ottie_object_set_name (OttieObject *self,
                       const char  *name)
{
  g_return_if_fail (OTTIE_IS_OBJECT (self));

  if (g_strcmp0 (self->name, name) == 0)
    return;

  g_free (self->name);
  self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

const char *
ottie_object_get_name (OttieObject *self)
{
  g_return_val_if_fail (OTTIE_IS_OBJECT (self), NULL);

  return self->name;
}

void
ottie_object_set_match_name (OttieObject *self,
                             const char  *match_name)
{
  g_return_if_fail (OTTIE_IS_OBJECT (self));

  if (g_strcmp0 (self->match_name, match_name) == 0)
    return;

  g_free (self->match_name);
  self->match_name = g_strdup (match_name);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATCH_NAME]);
}

const char *
ottie_object_get_match_name (OttieObject *self)
{
  g_return_val_if_fail (OTTIE_IS_OBJECT (self), NULL);

  return self->match_name;
}


