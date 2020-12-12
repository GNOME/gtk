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
#include "ottieparamspecprivate.h"

enum {
  PROP_0,
  PROP_MATCH_NAME,
  PROP_NAME,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
ottie_type_register_static_simple (GType             parent_type,
                                   const gchar      *type_name,
                                   guint             class_size,
                                   GClassInitFunc    class_init,
                                   GBaseInitFunc     base_init,
                                   guint             instance_size,
                                   GInstanceInitFunc instance_init,
                                   GTypeFlags        flags)
{
  GTypeInfo info;

  /* Instances are not allowed to be larger than this. If you have a big
   * fixed-length array or something, point to it instead.
   */
  g_return_val_if_fail (class_size <= G_MAXUINT16, G_TYPE_INVALID);
  g_return_val_if_fail (instance_size <= G_MAXUINT16, G_TYPE_INVALID);

  info.class_size = class_size;
  info.base_init = base_init;
  info.base_finalize = NULL;
  info.class_init = class_init;
  info.class_finalize = NULL;
  info.class_data = NULL;
  info.instance_size = instance_size;
  info.n_preallocs = 0;
  info.instance_init = instance_init;
  info.value_table = NULL;

  return g_type_register_static (parent_type, type_name, &info, flags);
}

static void
ottie_object_base_init (gpointer g_class)
{
  OttieObjectClass *klass = OTTIE_OBJECT_CLASS (g_class);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ottie_param_spec_get_property;
  object_class->set_property = ottie_param_spec_set_property;
}

#define g_type_register_static_simple(parent_type, type_name, class_size, class_init, instance_size, instance_init, flags) \
  ottie_type_register_static_simple (parent_type, type_name, class_size, class_init, ottie_object_base_init, instance_size, instance_init, flags)

G_DEFINE_TYPE (OttieObject, ottie_object, G_TYPE_OBJECT)

static void
ottie_object_dispose (GObject *object)
{
  OttieObject *self = OTTIE_OBJECT (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->match_name, g_free);

  G_OBJECT_CLASS (ottie_object_parent_class)->dispose (object);
}

static void
ottie_object_class_init (OttieObjectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = ottie_object_dispose;

  properties[PROP_NAME] =
    ottie_param_spec_string (gobject_class,
                             "name",
                             P_("Name"),
                             P_("User-given name"),
                             NULL,
                             (void *) ottie_object_get_name,
                             (void *) ottie_object_set_name);

  properties[PROP_MATCH_NAME] =
    ottie_param_spec_string (gobject_class,
                             "match-name",
                             P_("Match name"),
                             P_("Name for matching in scripts"),
                             NULL,
                             (void *) ottie_object_get_match_name,
                             (void *) ottie_object_set_match_name);
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


