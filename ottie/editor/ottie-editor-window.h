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

#ifndef __OTTIE_EDITOR_WINDOW_H__
#define __OTTIE_EDITOR_WINDOW_H__

#include <gtk/gtk.h>

#include "ottie-editor-application.h"

#define OTTIE_EDITOR_WINDOW_TYPE (ottie_editor_window_get_type ())
#define OTTIE_EDITOR_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), OTTIE_EDITOR_WINDOW_TYPE, OttieEditorWindow))


typedef struct _OttieEditorWindow         OttieEditorWindow;
typedef struct _OttieEditorWindowClass    OttieEditorWindowClass;


GType                   ottie_editor_window_get_type            (void);

OttieEditorWindow *     ottie_editor_window_new                 (OttieEditorApplication *application);

void                    ottie_editor_window_load                (OttieEditorWindow      *self,
                                                                 GFile                  *file);

#endif /* __OTTIE_EDITOR_WINDOW_H__ */
