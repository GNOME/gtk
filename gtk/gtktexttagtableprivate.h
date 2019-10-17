/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_TEXT_TAG_TABLE_PRIVATE_H__
#define __GTK_TEXT_TAG_TABLE_PRIVATE_H__

#include <gtk/gtktexttagtable.h>

G_BEGIN_DECLS

void _gtk_text_tag_table_add_buffer    (GtkTextTagTable *table,
                                        gpointer         buffer);
void _gtk_text_tag_table_remove_buffer (GtkTextTagTable *table,
                                        gpointer         buffer);
void _gtk_text_tag_table_tag_changed   (GtkTextTagTable *table,
                                        GtkTextTag      *tag,
                                        gboolean         size_changed);

G_END_DECLS

#endif
