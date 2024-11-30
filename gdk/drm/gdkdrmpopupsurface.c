/* gdkdrmpopupsurface.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkdrmpopupsurface-private.h"

#include "gdkpopupprivate.h"
#include "gdkseatprivate.h"

#include "gdkdrmdisplay-private.h"
#include "gdkdrmmonitor-private.h"

struct _GdkDrmPopupSurface
{
  GdkDrmSurface parent_instance;
  GdkPopupLayout *layout;
  guint attached : 1;
};

struct _GdkDrmPopupSurfaceClass
{
  GdkDrmSurfaceClass parent_class;
};

static void
gdk_drm_popup_surface_layout (GdkDrmPopupSurface *self,
                              int                 width,
                              int                 height,
                              GdkPopupLayout     *layout)
{
  GdkRectangle final_rect;
  GdkRectangle bounds;
  GdkMonitor *monitor;
  int x, y;
  int shadow_left, shadow_right, shadow_top, shadow_bottom;

  g_assert (GDK_IS_DRM_POPUP_SURFACE (self));
  g_assert (layout != NULL);
  g_assert (GDK_SURFACE (self)->parent);

  gdk_popup_layout_ref (layout);
  g_clear_pointer (&self->layout, gdk_popup_layout_unref);
  self->layout = layout;

  monitor = gdk_surface_get_layout_monitor (GDK_SURFACE (self),
                                            self->layout,
                                            _gdk_drm_monitor_get_workarea);

  if (monitor == NULL)
    monitor = _gdk_drm_surface_get_best_monitor (GDK_DRM_SURFACE (self));

  gdk_monitor_get_geometry (GDK_MONITOR (monitor), &bounds);

  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);

  gdk_surface_layout_popup_helper (GDK_SURFACE (self),
                                   width,
                                   height,
                                   shadow_left,
                                   shadow_right,
                                   shadow_top,
                                   shadow_bottom,
                                   monitor,
                                   &bounds,
                                   self->layout,
                                   &final_rect);

  gdk_surface_get_origin (GDK_SURFACE (self)->parent, &x, &y);

  GDK_SURFACE (self)->x = final_rect.x;
  GDK_SURFACE (self)->y = final_rect.y;

  x += final_rect.x;
  y += final_rect.y;

  if (final_rect.width != GDK_SURFACE (self)->width ||
      final_rect.height != GDK_SURFACE (self)->height)
    _gdk_drm_surface_move_resize (GDK_DRM_SURFACE (self),
                                  x,
                                  y,
                                  final_rect.width,
                                  final_rect.height);
  else if (x != GDK_DRM_SURFACE (self)->root_x ||
           y != GDK_DRM_SURFACE (self)->root_y)
    _gdk_drm_surface_move (GDK_DRM_SURFACE (self), x, y);
  else
    return;

  gdk_surface_invalidate_rect (GDK_SURFACE (self), NULL);
}

static void
show_popup (GdkDrmPopupSurface *self)
{
  _gdk_drm_surface_show (GDK_DRM_SURFACE (self));
}

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  show_popup (GDK_DRM_POPUP_SURFACE (surface));
}

static gboolean
gdk_drm_popup_surface_present (GdkPopup       *popup,
                               int             width,
                               int             height,
                               GdkPopupLayout *layout)
{
  GdkDrmPopupSurface *self = (GdkDrmPopupSurface *)popup;

  g_assert (GDK_IS_DRM_POPUP_SURFACE (self));

  gdk_drm_popup_surface_layout (self, width, height, layout);

  if (GDK_SURFACE_IS_MAPPED (GDK_SURFACE (self)))
    return TRUE;

  return FALSE;
}

static GdkGravity
gdk_drm_popup_surface_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_drm_popup_surface_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_drm_popup_surface_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_drm_popup_surface_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
popup_interface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_drm_popup_surface_present;
  iface->get_surface_anchor = gdk_drm_popup_surface_get_surface_anchor;
  iface->get_rect_anchor = gdk_drm_popup_surface_get_rect_anchor;
  iface->get_position_x = gdk_drm_popup_surface_get_position_x;
  iface->get_position_y = gdk_drm_popup_surface_get_position_y;
}

G_DEFINE_TYPE_WITH_CODE (GdkDrmPopupSurface, _gdk_drm_popup_surface, GDK_TYPE_DRM_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP, popup_interface_init))

enum {
  PROP_0,
  LAST_PROP,
};

static void
_gdk_drm_popup_surface_hide (GdkSurface *surface)
{
  GDK_SURFACE_CLASS (_gdk_drm_popup_surface_parent_class)->hide (surface);
}

static void
_gdk_drm_popup_surface_finalize (GObject *object)
{
  GdkDrmPopupSurface *self = (GdkDrmPopupSurface *)object;
  GdkSurface *parent = GDK_SURFACE (self)->parent;

  if (parent != NULL)
    parent->children = g_list_remove (parent->children, self);

  g_clear_object (&GDK_SURFACE (self)->parent);
  g_clear_pointer (&self->layout, gdk_popup_layout_unref);

  G_OBJECT_CLASS (_gdk_drm_popup_surface_parent_class)->finalize (object);
}

static void
_gdk_drm_popup_surface_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gdk_drm_popup_surface_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gdk_drm_popup_surface_constructed (GObject *object)
{
  G_OBJECT_CLASS (_gdk_drm_popup_surface_parent_class)->constructed (object);
}

static void
_gdk_drm_popup_surface_class_init (GdkDrmPopupSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = _gdk_drm_popup_surface_constructed;
  object_class->finalize = _gdk_drm_popup_surface_finalize;
  object_class->get_property = _gdk_drm_popup_surface_get_property;
  object_class->set_property = _gdk_drm_popup_surface_set_property;

  surface_class->hide = _gdk_drm_popup_surface_hide;

  gdk_popup_install_properties (object_class, LAST_PROP);
}

static void
_gdk_drm_popup_surface_init (GdkDrmPopupSurface *self)
{
}
