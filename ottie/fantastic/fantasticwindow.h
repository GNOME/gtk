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

#ifndef __FANTASTIC_WINDOW_H__
#define __FANTASTIC_WINDOW_H__

#include <gtk/gtk.h>

#include "fantasticapplication.h"

#define FANTASTIC_WINDOW_TYPE (fantastic_window_get_type ())
#define FANTASTIC_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FANTASTIC_WINDOW_TYPE, FantasticWindow))


typedef struct _FantasticWindow         FantasticWindow;
typedef struct _FantasticWindowClass    FantasticWindowClass;


GType                   fantastic_window_get_type               (void);

FantasticWindow *       fantastic_window_new                    (FantasticApplication   *application);

void                    fantastic_window_load                   (FantasticWindow        *self,
                                                                 GFile                  *file);

#endif /* __FANTASTIC_WINDOW_H__ */
