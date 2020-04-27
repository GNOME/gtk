/*
 * Copyright © 2020 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#import "GdkMacosWindow.h"

#include "gdktoplevelprivate.h"
#include "gdkmacostoplevelsurface-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosToplevelSurface
{
  GdkMacosSurface  parent_instance;

  GdkMacosSurface *transient_for;
  NSWindow        *window;

  guint            decorated : 1;
};

struct _GdkMacosToplevelSurfaceClass
{
  GdkMacosSurfaceClass parent_instance;
};

static void
toplevel_iface_init (GdkToplevelInterface *iface)
{
}

enum {
  PROP_0,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_CODE (GdkMacosToplevelSurface, _gdk_macos_toplevel_surface, GDK_TYPE_MACOS_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL, toplevel_iface_init))

static void
_gdk_macos_toplevel_surface_set_transient_for (GdkMacosToplevelSurface *self,
                                               GdkMacosSurface         *parent)
{
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));
  g_assert (!parent || GDK_IS_MACOS_SURFACE (parent));

  g_set_object (&self->transient_for, parent);
}

static void
_gdk_macos_toplevel_surface_set_decorated (GdkMacosToplevelSurface *self,
                                           gboolean                 decorated)
{
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  decorated = !!decorated;

  if (decorated != self->decorated)
    {
      self->decorated = decorated;
      [self->window setHasShadow:decorated ? YES : NO];
    }
}

static void
_gdk_macos_toplevel_surface_destroy (GdkSurface *surface,
                                     gboolean    foreign_destroy)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)surface;

  if (self->window != NULL)
    {
      GDK_BEGIN_MACOS_ALLOC_POOL;

      GdkMacosWindow *window = g_steal_pointer (&self->window);

      if (window != NULL)
        [window close];

      GDK_END_MACOS_ALLOC_POOL;
    }

  g_clear_object (&self->transient_for);

  GDK_SURFACE_CLASS (_gdk_macos_toplevel_surface_parent_class)->destroy (surface, foreign_destroy);
}

static void
_gdk_macos_toplevel_surface_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkMacosSurface *base = GDK_MACOS_SURFACE (surface);
  GdkMacosToplevelSurface *toplevel = GDK_MACOS_TOPLEVEL_SURFACE (base);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      g_value_set_string (value, _gdk_macos_surface_get_title (base));
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, toplevel->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, _gdk_macos_surface_get_modal_hint (base));
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      g_value_set_boolean (value, toplevel->decorated);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      g_value_set_enum (value, surface->fullscreen_mode);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gdk_macos_toplevel_surface_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkMacosSurface *base = GDK_MACOS_SURFACE (surface);
  GdkMacosToplevelSurface *toplevel = GDK_MACOS_TOPLEVEL_SURFACE (base);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      _gdk_macos_surface_set_title (base, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      _gdk_macos_toplevel_surface_set_transient_for (toplevel, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      _gdk_macos_surface_set_modal_hint (base, g_value_get_boolean (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      _gdk_macos_toplevel_surface_set_decorated (toplevel, g_value_get_boolean (value));
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gdk_macos_toplevel_surface_class_init (GdkMacosToplevelSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->get_property = _gdk_macos_toplevel_surface_get_property;
  object_class->set_property = _gdk_macos_toplevel_surface_set_property;

  surface_class->destroy = _gdk_macos_toplevel_surface_destroy;

  gdk_toplevel_install_properties (object_class, LAST_PROP);
}

static void
_gdk_macos_toplevel_surface_init (GdkMacosToplevelSurface *self)
{
  self->decorated = TRUE;
}

GdkMacosSurface *
_gdk_macos_toplevel_surface_new (GdkMacosDisplay *display,
                                 GdkSurface      *parent,
                                 int              x,
                                 int              y,
                                 int              width,
                                 int              height)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);
  g_return_val_if_fail (!parent || GDK_IS_MACOS_SURFACE (parent), NULL);

  return g_object_new (GDK_TYPE_MACOS_TOPLEVEL_SURFACE,
                       "display", display,
                       NULL);
}
