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

#include "ottieparamspecprivate.h"

G_DEFINE_TYPE (OttieParamSpec, ottie_param_spec, G_TYPE_PARAM);

static void
ottie_param_spec_class_init (OttieParamSpecClass *klass)
{
}

static void
ottie_param_spec_init (OttieParamSpec *self)
{
}

void
ottie_param_spec_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  OttieParamSpec *ospec = OTTIE_PARAM_SPEC (pspec);
  OttieParamSpecClass *ospec_class = OTTIE_PARAM_SPEC_GET_CLASS (ospec);

  ospec_class->set_property (ospec, object, value);
}

void
ottie_param_spec_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  OttieParamSpec *ospec = OTTIE_PARAM_SPEC (pspec);
  OttieParamSpecClass *ospec_class = OTTIE_PARAM_SPEC_GET_CLASS (ospec);

  ospec_class->get_property (ospec, object, value);
}

static void
ottie_param_install_pspec (GObjectClass *oclass,
                           GParamSpec   *pspec)
{
  guint n_properties;

  g_free (g_object_class_list_properties (oclass, &n_properties));

  g_object_class_install_property (oclass, n_properties + 1, pspec);
}

/*** STRING ***/

typedef struct _OttieParamSpecString OttieParamSpecString;
typedef struct _OttieParamSpecStringClass OttieParamSpecStringClass;

struct _OttieParamSpecString
{
  OttieParamSpec parent_instance;
  
  gchar *default_value;
  const char   *(* getter) (gpointer);
  void          (* setter) (gpointer, const char *);
};

struct _OttieParamSpecStringClass
{
  OttieParamSpecClass parent_class;
};

G_DEFINE_TYPE (OttieParamSpecString, ottie_param_spec_string, OTTIE_TYPE_PARAM_SPEC);

static void
ottie_param_spec_string_get_property (OttieParamSpec *pspec,
                                      GObject        *object,
                                      GValue         *value)
{
  OttieParamSpecString *self = OTTIE_PARAM_SPEC_STRING (pspec);

  g_value_set_string (value, self->getter (object));
}

static void
ottie_param_spec_string_set_property (OttieParamSpec *pspec,
                                      GObject        *object,
                                      const GValue   *value)
{
  OttieParamSpecString *self = OTTIE_PARAM_SPEC_STRING (pspec);

  self->setter (object, g_value_get_string (value));
}

static void
ottie_param_spec_string_finalize (GParamSpec *pspec)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_STRING));
  
  g_clear_pointer (&sspec->default_value, g_free);
  
  parent_class->finalize (pspec);
}

static void
ottie_param_spec_string_set_default (GParamSpec *pspec,
                                     GValue     *value)
{
  g_value_set_string (value, G_PARAM_SPEC_STRING (pspec)->default_value);
}

static gboolean
ottie_param_spec_string_validate (GParamSpec *pspec,
		                  GValue     *value)
{
  return FALSE;
}

static int
ottie_param_spec_string_values_cmp (GParamSpec   *pspec,
                                    const GValue *value1,
                                    const GValue *value2)
{
  return g_strcmp0 (g_value_get_string (value1),
                    g_value_get_string (value2));
}

static void
ottie_param_spec_string_class_init (OttieParamSpecStringClass *klass)
{
  GParamSpecClass *gparam_class = G_PARAM_SPEC_CLASS (klass);
  OttieParamSpecClass *oparam_class = OTTIE_PARAM_SPEC_CLASS (klass);

  oparam_class->get_property = ottie_param_spec_string_get_property;
  oparam_class->set_property = ottie_param_spec_string_set_property;

  gparam_class->value_type = G_TYPE_STRING;
  gparam_class->finalize = ottie_param_spec_string_finalize;
  gparam_class->value_set_default = ottie_param_spec_string_set_default;
  gparam_class->value_validate = ottie_param_spec_string_validate;
  gparam_class->values_cmp = ottie_param_spec_string_values_cmp;
}

static void
ottie_param_spec_string_init (OttieParamSpecString *self)
{
  self->default_value = NULL;
}

GParamSpec *
ottie_param_spec_string (GObjectClass *klass,
                         const char   *name,
                         const char   *nick,
                         const char   *blurb,
                         const char   *default_value,
                         const char   *(* getter) (gpointer),
                         void          (* setter) (gpointer, const char *))
{
  OttieParamSpecString *self;
  GParamFlags flags;

  flags = G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;
  if (getter)
    flags |= G_PARAM_READABLE;
  if (setter)
    flags |= G_PARAM_WRITABLE;

  self = g_param_spec_internal (OTTIE_TYPE_PARAM_SPEC_STRING,
                                name,
                                nick,
                                blurb,
                                flags);
  if (self == NULL)
    return NULL;

  g_free (self->default_value);
  self->default_value = g_strdup (default_value);
  self->getter = getter;
  self->setter = setter;
  
  ottie_param_install_pspec (klass, G_PARAM_SPEC (self));

  return G_PARAM_SPEC (self);
}
