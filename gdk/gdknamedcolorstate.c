/* gdknamedcolorstate.c
 *
 * Copyright 2024 (c) Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdknamedcolorstateprivate.h"

#include <glib/gi18n-lib.h>


struct _GdkNamedColorState
{
  GdkColorState parent_instance;

  GdkColorStateId id;
};

struct _GdkNamedColorStateClass
{
  GdkColorStateClass parent_class;
};

G_DEFINE_TYPE (GdkNamedColorState, gdk_named_color_state, GDK_TYPE_COLOR_STATE)

static gboolean
gdk_named_color_state_equal (GdkColorState *self,
                             GdkColorState *other)
{
  return ((GdkNamedColorState *) self)->id == ((GdkNamedColorState *) other)->id;
}

static GdkMemoryDepth
gdk_named_color_state_get_min_depth (GdkColorState *self)
{
  switch (((GdkNamedColorState *) self)->id)
    {
    case GDK_COLOR_STATE_SRGB_LINEAR:
    case GDK_COLOR_STATE_OKLAB:
    case GDK_COLOR_STATE_OKLCH:
      return GDK_MEMORY_U16;
    case GDK_COLOR_STATE_SRGB:
    case GDK_COLOR_STATE_HSL:
    case GDK_COLOR_STATE_HWB:
      return GDK_MEMORY_U8;
    default:
      g_assert_not_reached ();
    }
}

static const char *
gdk_named_color_state_get_name (GdkColorState *self)
{
  const char *names[] = {
    "srgb", "srgb-linear", "hsl", "hwb", "oklab", "oklch"
  };

  return names[((GdkNamedColorState *) self)->id];
}

static void
gdk_named_color_state_dispose (GObject *object)
{
  g_assert_not_reached ();

  G_OBJECT_CLASS (gdk_named_color_state_parent_class)->dispose (object);
}

static void
gdk_named_color_state_class_init (GdkNamedColorStateClass *klass)
{
  GdkColorStateClass *color_state_class = GDK_COLOR_STATE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_named_color_state_dispose;

  color_state_class->equal = gdk_named_color_state_equal;
  color_state_class->get_min_depth = gdk_named_color_state_get_min_depth;
  color_state_class->get_name = gdk_named_color_state_get_name;
}

static void
gdk_named_color_state_init (GdkNamedColorState *self)
{
}

static GdkColorState *
gdk_named_color_state_new (GdkColorStateId id)
{
  GdkNamedColorState *self;

  self = g_object_new (GDK_TYPE_NAMED_COLOR_STATE, NULL);

  self->id = id;

  return (GdkColorState *) self;
}

/**
 * gdk_color_state_get_srgb:
 *
 * Returns the object representing the sRGB color state.
 *
 * If you don't know anything about color states but need one for
 * use with some function, this one is most likely the right one.
 *
 * Returns: (transfer none): the object for the sRGB color state.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb (void)
{
  static GdkColorState *srgb_color_state;

  if (g_once_init_enter (&srgb_color_state))
    {
      GdkColorState *color_state;

      color_state = gdk_named_color_state_new (GDK_COLOR_STATE_SRGB);
      g_assert (color_state);

      g_once_init_leave (&srgb_color_state, color_state);
    }

  return srgb_color_state;
}

/**
 * gdk_color_state_get_srgb_linear:
 *
 * Returns the object representing the linear sRGB color state.
 *
 * Returns: (transfer none): the object for the linear sRGB color state.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb_linear (void)
{
  static GdkColorState *srgb_linear_color_state;

  if (g_once_init_enter (&srgb_linear_color_state))
    {
      GdkColorState *color_state;

      color_state = gdk_named_color_state_new (GDK_COLOR_STATE_SRGB);
      g_assert (color_state);

      g_once_init_leave (&srgb_linear_color_state, color_state);
    }

  return srgb_linear_color_state;
}

/**
 * gdk_color_state_get_hsl:
 *
 * Returns the object representing the HSL color state.
 *
 * Returns: (transfer none): the object for the HSL color state.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_hsl (void)
{
  static GdkColorState *hsl_color_state;

  if (g_once_init_enter (&hsl_color_state))
    {
      GdkColorState *color_state;

      color_state = gdk_named_color_state_new (GDK_COLOR_STATE_HSL);
      g_assert (color_state);

      g_once_init_leave (&hsl_color_state, color_state);
    }

  return hsl_color_state;
}

/**
 * gdk_color_state_get_hwb:
 *
 * Returns the object representing the HWB color state.
 *
 * Returns: (transfer none): the object for the HWB color state.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_hwb (void)
{
  static GdkColorState *hwb_color_state;

  if (g_once_init_enter (&hwb_color_state))
    {
      GdkColorState *color_state;

      color_state = gdk_named_color_state_new (GDK_COLOR_STATE_HWB);
      g_assert (color_state);

      g_once_init_leave (&hwb_color_state, color_state);
    }

  return hwb_color_state;
}

/**
 * gdk_color_state_get_oklab:
 *
 * Returns the object representing the OKLAB color state.
 *
 * Returns: (transfer none): the object for the OKLAB color state.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_oklab (void)
{
  static GdkColorState *oklab_color_state;

  if (g_once_init_enter (&oklab_color_state))
    {
      GdkColorState *color_state;

      color_state = gdk_named_color_state_new (GDK_COLOR_STATE_OKLAB);
      g_assert (color_state);

      g_once_init_leave (&oklab_color_state, color_state);
    }

  return oklab_color_state;
}

/**
 * gdk_color_state_get_oklcb:
 *
 * Returns the object representing the OKLCH color state.
 *
 * Returns: (transfer none): the object for the OKLCH color state.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_oklch (void)
{
  static GdkColorState *oklch_color_state;

  if (g_once_init_enter (&oklch_color_state))
    {
      GdkColorState *color_state;

      color_state = gdk_named_color_state_new (GDK_COLOR_STATE_OKLCH);
      g_assert (color_state);

      g_once_init_leave (&oklch_color_state, color_state);
    }

  return oklch_color_state;
}

GdkColorStateId
gdk_named_color_state_get_id (GdkColorState *self)
{
  return ((GdkNamedColorState *) self)->id;
}
