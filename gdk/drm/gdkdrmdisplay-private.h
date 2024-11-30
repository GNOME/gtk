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
#include "gdkdisplayprivate.h"

G_BEGIN_DECLS

struct _GdkDrmDisplay
{
  GdkDisplay  parent_instance;
  char       *name;
  GListStore *monitors;
  GdkKeymap  *keymap;
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

G_END_DECLS
