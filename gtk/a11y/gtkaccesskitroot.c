/* gtkaccesskitroot.c: AccessKit root object
 *
 * Copyright 2024  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkaccesskitrootprivate.h"

struct _GtkAccessKitRoot
{
  GObject parent_instance;

  GdkSurface *surface;

  /* TODO */
};

enum
{
  PROP_SURFACE = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAccessKitRoot, gtk_accesskit_root, G_TYPE_OBJECT)

static void
gtk_accesskit_root_finalize (GObject *gobject)
{
  /* TODO */

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->finalize (gobject);
}

static void
gtk_accesskit_root_dispose (GObject *gobject)
{
  /* TODO */

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->dispose (gobject);
}

static void
gtk_accesskit_root_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_SURFACE:
      self->surface = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_accesskit_root_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkAccessKitRoot *self = GTK_ACCESSKIT_ROOT (gobject);

  switch (prop_id)
    {
    case PROP_SURFACE:
      g_value_set_object (value, self->surface);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_accesskit_root_constructed (GObject *gobject)
{
  /* TODO */

  G_OBJECT_CLASS (gtk_accesskit_root_parent_class)->constructed (gobject);
}

static void
gtk_accesskit_root_class_init (GtkAccessKitRootClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_accesskit_root_constructed;
  gobject_class->set_property = gtk_accesskit_root_set_property;
  gobject_class->get_property = gtk_accesskit_root_get_property;
  gobject_class->dispose = gtk_accesskit_root_dispose;
  gobject_class->finalize = gtk_accesskit_root_finalize;

  obj_props[PROP_SURFACE] =
    g_param_spec_object ("surface", NULL, NULL,
                         GDK_TYPE_SURFACE,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_accesskit_root_init (GtkAccessKitRoot *self)
{
}

/* TODO */
