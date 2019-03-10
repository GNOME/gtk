/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkdiffpaintableprivate.h"

#include "gtkintl.h"
#include "gtksnapshot.h"

struct _GtkDiffPaintable
{
  GObject parent_instance;

  GdkPaintable *first;
  GdkPaintable *second;
};

struct _GtkDiffPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_FIRST,
  PROP_SECOND,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_diff_paintable_paintable_snapshot (GdkPaintable *paintable,
                                       GdkSnapshot  *snapshot,
                                       double        width,
                                       double        height)
{
  GtkDiffPaintable *self = GTK_DIFF_PAINTABLE (paintable);
  float amplify[16] = { 4, 0, 0, 0,
                        0, 4, 0, 0,
                        0, 0, 4, 0,
                        0, 0, 0, 1 };
  graphene_matrix_t matrix;

  graphene_matrix_init_from_float (&matrix, amplify);

  gtk_snapshot_push_color_matrix (snapshot, &matrix, graphene_vec4_zero ());

  gtk_snapshot_push_blend (snapshot, GSK_BLEND_MODE_DIFFERENCE);

  if (self->first)
    gdk_paintable_snapshot (self->first, snapshot, width, height);
  gtk_snapshot_pop (snapshot);

  if (self->second)
    gdk_paintable_snapshot (self->second, snapshot, width, height);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_pop (snapshot);
}

static int
gtk_diff_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkDiffPaintable *self = GTK_DIFF_PAINTABLE (paintable);
  int result;
  
  result = 0;

  if (self->first)
    result = MAX (result, gdk_paintable_get_intrinsic_width (self->first));
  if (self->second)
    result = MAX (result, gdk_paintable_get_intrinsic_width (self->second));

  return result;
}

static int
gtk_diff_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkDiffPaintable *self = GTK_DIFF_PAINTABLE (paintable);
  int result;
  
  result = 0;

  if (self->first)
    result = MAX (result, gdk_paintable_get_intrinsic_height (self->first));
  if (self->second)
    result = MAX (result, gdk_paintable_get_intrinsic_height (self->second));

  return result;
}

static void
gtk_diff_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_diff_paintable_paintable_snapshot;
  iface->get_intrinsic_width = gtk_diff_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_diff_paintable_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_EXTENDED (GtkDiffPaintable, gtk_diff_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_diff_paintable_paintable_init))

static void
gtk_diff_paintable_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)

{
  GtkDiffPaintable *self = GTK_DIFF_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FIRST:
      gtk_diff_paintable_set_first (self, g_value_get_object (value));
      break;

    case PROP_SECOND:
      gtk_diff_paintable_set_second (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_diff_paintable_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkDiffPaintable *self = GTK_DIFF_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FIRST:
      g_value_set_object (value, self->first);
      break;

    case PROP_SECOND:
      g_value_set_object (value, self->second);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_diff_paintable_unset_paintable (GtkDiffPaintable  *self,
                                    GdkPaintable     **paintable)
{
  guint flags;
  
  if (*paintable == NULL)
    return;

  flags = gdk_paintable_get_flags (*paintable);

  if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    g_signal_handlers_disconnect_by_func (*paintable,
                                          gdk_paintable_invalidate_contents,
                                          self);

  if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
    g_signal_handlers_disconnect_by_func (*paintable,
                                          gdk_paintable_invalidate_size,
                                          self);

  g_clear_object (paintable);
}

static void
gtk_diff_paintable_dispose (GObject *object)
{
  GtkDiffPaintable *self = GTK_DIFF_PAINTABLE (object);

  gtk_diff_paintable_unset_paintable (self, &self->first);
  gtk_diff_paintable_unset_paintable (self, &self->second);

  G_OBJECT_CLASS (gtk_diff_paintable_parent_class)->dispose (object);
}

static void
gtk_diff_paintable_class_init (GtkDiffPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_diff_paintable_get_property;
  gobject_class->set_property = gtk_diff_paintable_set_property;
  gobject_class->dispose = gtk_diff_paintable_dispose;

  properties[PROP_FIRST] =
    g_param_spec_object ("first",
                         P_("First"),
                         P_("First paintable to diff"),
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SECOND] =
    g_param_spec_object ("second",
                         P_("Second"),
                         P_("Second paintable to diff"),
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_diff_paintable_init (GtkDiffPaintable *self)
{
}

GdkPaintable *
gtk_diff_paintable_new (GdkPaintable *first,
                        GdkPaintable *second)
{
  g_return_val_if_fail (first == NULL || GDK_IS_PAINTABLE (first), NULL);
  g_return_val_if_fail (second == NULL || GDK_IS_PAINTABLE (second), NULL);

  return g_object_new (GTK_TYPE_DIFF_PAINTABLE,
                       "first", first,
                       "second", second,
                       NULL);
}

static gboolean
gtk_diff_paintable_set_paintable (GtkDiffPaintable  *self,
                                  GdkPaintable     **paintable,
                                  GdkPaintable      *new_paintable)
{
  if (*paintable == new_paintable)
    return FALSE;

  gtk_diff_paintable_unset_paintable (self, paintable);

  if (new_paintable)
    {
      const guint flags = gdk_paintable_get_flags (new_paintable);

      *paintable = g_object_ref (new_paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_connect_swapped (new_paintable,
                                  "invalidate-contents",
                                  G_CALLBACK (gdk_paintable_invalidate_contents),
                                  self);
      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_connect_swapped (new_paintable,
                                  "invalidate-size",
                                  G_CALLBACK (gdk_paintable_invalidate_size),
                                  self);
    }

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return TRUE;
}

void
gtk_diff_paintable_set_first (GtkDiffPaintable *self,
                              GdkPaintable     *paintable)
{
  g_return_if_fail (GTK_IS_DIFF_PAINTABLE (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (!gtk_diff_paintable_set_paintable (self, &self->first, paintable))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FIRST]);
}

GdkPaintable *
gtk_diff_paintable_get_first (GtkDiffPaintable *self)
{
  g_return_val_if_fail (GTK_IS_DIFF_PAINTABLE (self), NULL);

  return self->first;
}

void
gtk_diff_paintable_set_second (GtkDiffPaintable *self,
                               GdkPaintable     *paintable)
{
  g_return_if_fail (GTK_IS_DIFF_PAINTABLE (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (!gtk_diff_paintable_set_paintable (self, &self->second, paintable))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SECOND]);
}

GdkPaintable *
gtk_diff_paintable_get_second (GtkDiffPaintable *self)
{
  g_return_val_if_fail (GTK_IS_DIFF_PAINTABLE (self), NULL);

  return self->second;
}

