/* gdkdrmdisplay-private.h
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

#pragma once

#include "gdkdrmdisplay.h"
#include "gdkdrmsurface.h"
#include "gdkdisplayprivate.h"

G_BEGIN_DECLS

struct _GdkDrmDisplay
{
  GdkDisplay  parent_instance;
  char       *name;
  GListStore *monitors;
  GdkKeymap  *keymap;

  /* DRM/GBM/EGL backend */
  int         drm_fd;
  void       *gbm_device;   /* struct gbm_device * */
  void       *egl_display;  /* EGLDisplay */

  /* Input */
  void       *libinput;     /* struct libinput * */
  GSource    *libinput_source;
  int         pointer_x;
  int         pointer_y;
  GdkModifierType keyboard_modifiers;
  GdkModifierType mouse_modifiers;

  /* Surface stacking (front to back); GdkDrmSurface * */
  GList      *surfaces;

  /* Full layout bounds (synthetic geometry across all monitors) */
  GdkRectangle layout_bounds;
};

struct _GdkDrmDisplayClass
{
  GdkDisplayClass parent_class;
};

GdkDisplay      *_gdk_drm_display_open                           (const char    *display_name);
GdkModifierType  _gdk_drm_display_get_current_keyboard_modifiers (GdkDrmDisplay *display);
GdkModifierType  _gdk_drm_display_get_current_mouse_modifiers    (GdkDrmDisplay *display);
void             _gdk_drm_display_to_display_coords              (GdkDrmDisplay *self,
                                                                  int            x,
                                                                  int            y,
                                                                  int           *out_x,
                                                                  int           *out_y);
void             _gdk_drm_display_from_display_coords            (GdkDrmDisplay *self,
                                                                  int            x,
                                                                  int            y,
                                                                  int           *out_x,
                                                                  int           *out_y);

GdkDrmSurface   *_gdk_drm_display_get_surface_at_display_coords  (GdkDrmDisplay *self,
                                                                  int            x,
                                                                  int            y,
                                                                  int           *out_surface_x,
                                                                  int           *out_surface_y);
void             _gdk_drm_display_add_surface                    (GdkDrmDisplay *self,
                                                                  GdkDrmSurface *surface);
void             _gdk_drm_display_remove_surface                 (GdkDrmDisplay *self,
                                                                  GdkDrmSurface *surface);
void             _gdk_drm_display_set_pointer_position           (GdkDrmDisplay *self,
                                                                  int            x,
                                                                  int            y);

G_END_DECLS
