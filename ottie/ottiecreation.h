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

#ifndef __OTTIE_CREATION_H__
#define __OTTIE_CREATION_H__

#if !defined (__OTTIE_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ottie/ottie.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_CREATION         (ottie_creation_get_type ())
#define OTTIE_CREATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), OTTIE_TYPE_CREATION, OttieCreation))
#define OTTIE_CREATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), OTTIE_TYPE_CREATION, OttieCreationClass))
#define OTTIE_IS_CREATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OTTIE_TYPE_CREATION))
#define OTTIE_IS_CREATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), OTTIE_TYPE_CREATION))
#define OTTIE_CREATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), OTTIE_TYPE_CREATION, OttieCreationClass))

typedef struct _OttieCreation OttieCreation;
typedef struct _OttieCreationClass OttieCreationClass;

GDK_AVAILABLE_IN_ALL
GType                   ottie_creation_get_type                 (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
OttieCreation *         ottie_creation_new                      (void);
GDK_AVAILABLE_IN_ALL
OttieCreation *         ottie_creation_new_for_file             (GFile                  *file);
GDK_AVAILABLE_IN_ALL
OttieCreation *         ottie_creation_new_for_filename         (const char             *filename);

GDK_AVAILABLE_IN_ALL
void                    ottie_creation_load_file                (OttieCreation          *self,
                                                                 GFile                  *file);
GDK_AVAILABLE_IN_ALL
void                    ottie_creation_load_filename            (OttieCreation          *self,
                                                                 const char             *filename);
GDK_AVAILABLE_IN_ALL
void                    ottie_creation_load_bytes               (OttieCreation          *self,
                                                                 GBytes                 *bytes);

GDK_AVAILABLE_IN_ALL
gboolean                ottie_creation_is_loading               (OttieCreation          *self);
GDK_AVAILABLE_IN_ALL
gboolean                ottie_creation_is_prepared              (OttieCreation          *self);

GDK_AVAILABLE_IN_ALL
const char *            ottie_creation_get_name                 (OttieCreation          *self);
GDK_AVAILABLE_IN_ALL
double                  ottie_creation_get_frame_rate           (OttieCreation          *self);
GDK_AVAILABLE_IN_ALL
double                  ottie_creation_get_start_frame          (OttieCreation          *self);
GDK_AVAILABLE_IN_ALL
double                  ottie_creation_get_end_frame            (OttieCreation          *self);
GDK_AVAILABLE_IN_ALL
double                  ottie_creation_get_width                (OttieCreation          *self);
GDK_AVAILABLE_IN_ALL
double                  ottie_creation_get_height               (OttieCreation          *self);



G_END_DECLS

#endif /* __OTTIE_CREATION_H__ */
