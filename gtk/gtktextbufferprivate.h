/* GTK - The GIMP Toolkit
 * gtktextbufferprivate.h Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TEXT_BUFFER_PRIVATE_H__
#define __GTK_TEXT_BUFFER_PRIVATE_H__

#include <gtk/gtktextbuffer.h>

G_BEGIN_DECLS


void            _gtk_text_buffer_spew                  (GtkTextBuffer      *buffer);

GtkTextBTree*   _gtk_text_buffer_get_btree             (GtkTextBuffer      *buffer);

const PangoLogAttr* _gtk_text_buffer_get_line_log_attrs (GtkTextBuffer     *buffer,
                                                         const GtkTextIter *anywhere_in_line,
                                                         gint              *char_len);

void _gtk_text_buffer_notify_will_remove_tag (GtkTextBuffer *buffer,
                                              GtkTextTag    *tag);

void _gtk_text_buffer_get_text_before (GtkTextBuffer   *buffer,
                                       AtkTextBoundary  boundary_type,
                                       GtkTextIter     *position,
                                       GtkTextIter     *start,
                                       GtkTextIter     *end);
void _gtk_text_buffer_get_text_at     (GtkTextBuffer   *buffer,
                                       AtkTextBoundary  boundary_type,
                                       GtkTextIter     *position,
                                       GtkTextIter     *start,
                                       GtkTextIter     *end);
void _gtk_text_buffer_get_text_after  (GtkTextBuffer   *buffer,
                                       AtkTextBoundary  boundary_type,
                                       GtkTextIter     *position,
                                       GtkTextIter     *start,
                                       GtkTextIter     *end);

G_END_DECLS

#endif
