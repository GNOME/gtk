/*
 * gtkemojiitemprivate.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_EMOJI_DATABASE (gtk_emoji_database_get_type())
#define GTK_TYPE_EMOJI_ITEM (gtk_emoji_item_get_type())

G_DECLARE_FINAL_TYPE (GtkEmojiDatabase, gtk_emoji_database, GTK, EMOJI_DATABASE, GObject)
G_DECLARE_FINAL_TYPE (GtkEmojiItem, gtk_emoji_item, GTK, EMOJI_ITEM, GObject)

GVariant         *gtk_emoji_database_dup_record        (GtkEmojiDatabase *self,
                                                        guint             id);
char             *gtk_emoji_database_dup_text          (GtkEmojiDatabase *self,
                                                        guint             id,
                                                        gunichar          modifier);
void              gtk_emoji_database_get_group_range   (GtkEmojiDatabase *self,
                                                        guint             group,
                                                        guint            *out_offset,
                                                        guint            *out_size);
guint             gtk_emoji_database_get_item_calls    (GtkEmojiDatabase *self);
guint             gtk_emoji_database_get_n_live_items  (GtkEmojiDatabase *self);
gboolean          gtk_emoji_database_is_owner          (GtkEmojiDatabase *self);
GtkEmojiDatabase *gtk_emoji_database_new               (void);
GtkEmojiDatabase *gtk_emoji_item_get_database          (GtkEmojiItem     *self);
guint             gtk_emoji_item_get_id                (GtkEmojiItem     *self);
GtkEmojiItem     *gtk_emoji_item_new                   (GVariant         *record,
                                                        gunichar          modifier);
GVariant         *gtk_emoji_item_dup_record            (GtkEmojiItem     *self);
char             *gtk_emoji_item_dup_text              (GtkEmojiItem     *self);
gunichar          gtk_emoji_item_get_modifier          (GtkEmojiItem     *self);
void              gtk_emoji_recent_add                 (GListStore       *store,
                                                        GtkEmojiItem     *item);
void              gtk_emoji_recent_load                (GListStore       *store,
                                                        GVariant         *saved);
GVariant         *gtk_emoji_recent_serialize           (GListStore       *store);

G_END_DECLS
