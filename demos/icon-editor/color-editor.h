/*
 * Copyright © 2025 Red Hat, Inc
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
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>


#define COLOR_EDITOR_TYPE (color_editor_get_type ())
G_DECLARE_FINAL_TYPE (ColorEditor, color_editor, COLOR, EDITOR, GtkWidget)


ColorEditor *    color_editor_new          (void);

void             color_editor_set_symbolic (ColorEditor    *self,
                                            unsigned int    symbolic);
unsigned int     color_editor_get_symbolic (ColorEditor    *self);

void             color_editor_set_color    (ColorEditor    *self,
                                            const GdkRGBA  *color);
const GdkRGBA   *color_editor_get_color    (ColorEditor    *self);
