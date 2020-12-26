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

#ifndef __OTTIE_EDITOR_APPLICATION_H__
#define __OTTIE_EDITOR_APPLICATION_H__

#include <gtk/gtk.h>


#define OTTIE_EDITOR_APPLICATION_TYPE (ottie_editor_application_get_type ())
#define OTTIE_EDITOR_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), OTTIE_EDITOR_APPLICATION_TYPE, OttieEditorApplication))


typedef struct _OttieEditorApplication       OttieEditorApplication;
typedef struct _OttieEditorApplicationClass  OttieEditorApplicationClass;


GType                   ottie_editor_application_get_type       (void);
OttieEditorApplication *ottie_editor_application_new            (void);


#endif /* __OTTIE_EDITOR_APPLICATION_H__ */
