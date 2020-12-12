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

#include "ottiepaintable.h"

#include "ottiecreationprivate.h"

#include <math.h>
#include <glib/gi18n.h>

struct _OttiePaintable
{
  GObject parent_instance;

  OttieCreation *creation;
  gint64 timestamp;
};

struct _OttiePaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_CREATION,
  PROP_DURATION,
  PROP_TIMESTAMP,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
ottie_paintable_paintable_snapshot (GdkPaintable *paintable,
                                    GdkSnapshot  *snapshot,
                                    double        width,
                                    double        height)
{
  OttiePaintable *self = OTTIE_PAINTABLE (paintable);
  double w, h, timestamp;

  if (!self->creation)
    return;

  w = ottie_creation_get_width (self->creation);
  h = ottie_creation_get_height (self->creation);
  timestamp = (double) self->timestamp / G_USEC_PER_SEC;

  if (w != width || h != height)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_scale (snapshot, width / w, height / h);
    }

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, w, h));
  ottie_creation_snapshot (self->creation, snapshot, timestamp);
  gtk_snapshot_pop (snapshot);

  if (w != width || h != height)
    gtk_snapshot_restore (snapshot);
}

static int
ottie_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  OttiePaintable *self = OTTIE_PAINTABLE (paintable);

  if (!self->creation)
    return 0;

  return ceil (ottie_creation_get_width (self->creation));
}

static int
ottie_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  OttiePaintable *self = OTTIE_PAINTABLE (paintable);

  if (!self->creation)
    return 0;

  return ceil (ottie_creation_get_height (self->creation));

}

static void
ottie_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = ottie_paintable_paintable_snapshot;
  iface->get_intrinsic_width = ottie_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = ottie_paintable_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (OttiePaintable, ottie_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               ottie_paintable_paintable_init))

static void
ottie_paintable_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)

{
  OttiePaintable *self = OTTIE_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_CREATION:
      ottie_paintable_set_creation (self, g_value_get_object (value));
      break;

    case PROP_TIMESTAMP:
      ottie_paintable_set_timestamp (self, g_value_get_int64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_paintable_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  OttiePaintable *self = OTTIE_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_CREATION:
      g_value_set_object (value, self->creation);
      break;

    case PROP_DURATION:
      g_value_set_int64 (value, ottie_paintable_get_duration (self));
      break;

    case PROP_TIMESTAMP:
      g_value_set_int64 (value, self->timestamp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_paintable_prepared_cb (OttieCreation  *creation,
                             GParamSpec     *pspec,
                             OttiePaintable *self)
{
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
}

static void
ottie_paintable_unset_creation (OttiePaintable *self)
{
  if (self->creation == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->creation, ottie_paintable_prepared_cb, self);
  g_clear_object (&self->creation);
}

static void
ottie_paintable_dispose (GObject *object)
{
  OttiePaintable *self = OTTIE_PAINTABLE (object);

  ottie_paintable_unset_creation (self);

  G_OBJECT_CLASS (ottie_paintable_parent_class)->dispose (object);
}

static void
ottie_paintable_class_init (OttiePaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = ottie_paintable_get_property;
  gobject_class->set_property = ottie_paintable_set_property;
  gobject_class->dispose = ottie_paintable_dispose;

  /**
   * OttiePaintable:creation
   *
   * The displayed creation or %NULL.
   */
  properties[PROP_CREATION] =
    g_param_spec_object ("creation",
                         _("Creation"),
                         _("The displayed creation"),
                         OTTIE_TYPE_CREATION,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttiePaintable:duration
   *
   * Duration of the displayed creation
   */
  properties[PROP_DURATION] =
    g_param_spec_int64 ("duration",
                        _("Duration"),
                        _("Duration of the displayed creation"),
                        0, G_MAXINT64, 0,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttiePaintable:timestamp
   *
   * At what timestamp to display the creation.
   */
  properties[PROP_TIMESTAMP] =
    g_param_spec_int64 ("timestmp",
                        _("Timestamp"),
                        _("At what timestamp to display the creation"),
                        0, G_MAXINT64, 0,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
ottie_paintable_init (OttiePaintable *self)
{
}

/**
 * ottie_paintable_new:
 * @creation: (allow-none) (transfer full): an #OttiePaintable or %NULL
 *
 * Creates a new Ottie paintable for the given @creation
 *
 * Returns: (transfer full) (type OttiePaintable): a new #OttiePaintable
 **/
OttiePaintable *
ottie_paintable_new (OttieCreation *creation)
{
  OttiePaintable *self;

  g_return_val_if_fail (creation == creation || OTTIE_IS_CREATION (creation), NULL);

  self = g_object_new (OTTIE_TYPE_PAINTABLE,
                       "creation", creation,
                       NULL);

  g_clear_object (&creation);

  return self;
}

/**
 * ottie_paintable_get_creation:
 * @self: an #OttiePaintable
 *
 * Returns the creation that shown or %NULL
 * if none.
 *
 * Returns: (transfer none) (nullable): the observed creation.
 **/
OttieCreation *
ottie_paintable_get_creation (OttiePaintable *self)
{
  g_return_val_if_fail (OTTIE_IS_PAINTABLE (self), NULL);

  return self->creation;
}

/**
 * ottie_paintable_set_creation:
 * @self: an #OttiePaintable
 * @creation: (allow-none): the creation to show or %NULL
 *
 * Sets the creation that should be shown.
 **/
void
ottie_paintable_set_creation (OttiePaintable *self,
                              OttieCreation  *creation)
{
  g_return_if_fail (OTTIE_IS_PAINTABLE (self));
  g_return_if_fail (creation == NULL || OTTIE_IS_CREATION (creation));

  if (self->creation == creation)
    return;

  ottie_paintable_unset_creation (self);

  self->creation = g_object_ref (creation);
  g_signal_connect (creation, "notify::prepared", G_CALLBACK (ottie_paintable_prepared_cb), self);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CREATION]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
}

/**
 * ottie_paintable_get_timestamp:
 * @self: an #OttiePaintable
 *
 * Gets the timestamp for the currently displayed image. 
 *
 * Returns: the timestamp
 **/
gint64
ottie_paintable_get_timestamp (OttiePaintable *self)
{
  g_return_val_if_fail (OTTIE_IS_PAINTABLE (self), 0);

  return self->timestamp;
}

/**
 * ottie_paintable_set_timestamp:
 * @self: an #OttiePaintable
 * @timestamp: the timestamp to display
 *
 * Sets the timestamp to display the creation at.
 **/
void
ottie_paintable_set_timestamp (OttiePaintable *self,
                               gint64          timestamp)
{
  g_return_if_fail (OTTIE_IS_PAINTABLE (self));
  g_return_if_fail (timestamp >= 0);

  if (self->timestamp == timestamp)
    return;

  self->timestamp = timestamp;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMESTAMP]);
}

/**
 * ottie_paintable_get_duration:
 * @self: an #OttiePaintable
 *
 * Gets the duration of the currently playing creation.
 *
 * Returns: The duration in usec.
 **/
gint64
ottie_paintable_get_duration (OttiePaintable *self)
{
  g_return_val_if_fail (OTTIE_IS_PAINTABLE (self), 0);

  if (self->creation == NULL)
    return 0;

  return ceil (G_USEC_PER_SEC * ottie_creation_get_end_frame (self->creation)
               / ottie_creation_get_frame_rate (self->creation));
}

