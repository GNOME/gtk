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

#ifndef __OTTIE_PLAYER_H__
#define __OTTIE_PLAYER_H__

#if !defined (__OTTIE_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ottie/ottie.h> can be included directly."
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_PLAYER (ottie_player_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (OttiePlayer, ottie_player, OTTIE, PLAYER, GtkMediaStream)

GDK_AVAILABLE_IN_ALL
OttiePlayer *           ottie_player_new                        (void);
GDK_AVAILABLE_IN_ALL
OttiePlayer *           ottie_player_new_for_file               (GFile                  *file);
GDK_AVAILABLE_IN_ALL
OttiePlayer *           ottie_player_new_for_filename           (const char             *filename);
GDK_AVAILABLE_IN_ALL
OttiePlayer *           ottie_player_new_for_resource           (const char             *resource_path);

GDK_AVAILABLE_IN_ALL
void                    ottie_player_set_file                   (OttiePlayer            *self,
                                                                 GFile                  *file);
GDK_AVAILABLE_IN_ALL
GFile *                 ottie_player_get_file                   (OttiePlayer            *self);
GDK_AVAILABLE_IN_ALL
void                    ottie_player_set_filename               (OttiePlayer            *self,
                                                                 const char             *filename);
GDK_AVAILABLE_IN_ALL
void                    ottie_player_set_resource               (OttiePlayer            *self,
                                                                 const char             *resource_path);

G_END_DECLS

#endif /* __OTTIE_PLAYER_H__ */
