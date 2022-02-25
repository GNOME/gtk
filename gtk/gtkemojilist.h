/*
 * Copyright © 2022 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GTK_EMOJI_LIST_H__
#define __GTK_EMOJI_LIST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gio/gio.h>
/* for GDK_AVAILABLE_IN_ALL */
#include <gdk/gdk.h>


G_BEGIN_DECLS

#define GTK_TYPE_EMOJI_OBJECT (gtk_emoji_object_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkEmojiObject, gtk_emoji_object, GTK, EMOJI_OBJECT, GObject)

GDK_AVAILABLE_IN_ALL
void                   gtk_emoji_object_get_text (GtkEmojiObject *self,
                                                  char           *buffer,
                                                  int             length,
                                                  gunichar        modifier);

typedef enum {
 GTK_EMOJI_GROUP_RECENT,
 GTK_EMOJI_GROUP_SMILEYS,
 GTK_EMOJI_GROUP_BODY,
 GTK_EMOJI_GROUP_COMPONENT,
 GTK_EMOJI_GROUP_NATURE,
 GTK_EMOJI_GROUP_FOOD,
 GTK_EMOJI_GROUP_PLACES,
 GTK_EMOJI_GROUP_ACTIVITIES,
 GTK_EMOJI_GROUP_OBJECTS,
 GTK_EMOJI_GROUP_SYMBOLS,
 GTK_EMOJI_GROUP_FLAGS
} GtkEmojiGroup;

GDK_AVAILABLE_IN_ALL
GtkEmojiGroup           gtk_emoji_object_get_group      (GtkEmojiObject *self);

GDK_AVAILABLE_IN_ALL
const char *            gtk_emoji_object_get_name       (GtkEmojiObject *self);

GDK_AVAILABLE_IN_ALL
const char **           gtk_emoji_object_get_keywords   (GtkEmojiObject *self);

#define GTK_TYPE_EMOJI_LIST (gtk_emoji_list_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkEmojiList, gtk_emoji_list, GTK, EMOJI_LIST, GObject)

GDK_AVAILABLE_IN_ALL
GtkEmojiList * gtk_emoji_list_new             (void);

G_END_DECLS

#endif /* __GTK_EMOJI_LIST_H__ */
