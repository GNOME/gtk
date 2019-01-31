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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#ifndef __GTK_PANGO_H__
#define __GTK_PANGO_H__


#include <pango/pangocairo.h>
#include <atk/atk.h>

G_BEGIN_DECLS

void             _gtk_pango_fill_layout            (cairo_t         *cr,
                                                    PangoLayout     *layout);


AtkAttributeSet *_gtk_pango_get_default_attributes (AtkAttributeSet *attributes,
                                                    PangoLayout     *layout);

AtkAttributeSet *_gtk_pango_get_run_attributes     (AtkAttributeSet *attributes,
                                                    PangoLayout     *layout,
                                                    gint             offset,
                                                    gint            *start_offset,
                                                    gint            *end_offset);

gint _gtk_pango_move_chars     (PangoLayout  *layout,
                                gint          offset,
                                gint          count);
gint _gtk_pango_move_words     (PangoLayout  *layout,
                                gint          offset,
                                gint          count);
gint _gtk_pango_move_sentences (PangoLayout  *layout,
                                gint          offset,
                                gint          count);
gint _gtk_pango_move_lines     (PangoLayout  *layout,
                                gint          offset,
                                gint          count);

gboolean _gtk_pango_is_inside_word     (PangoLayout  *layout,
                                        gint          offset);
gboolean _gtk_pango_is_inside_sentence (PangoLayout  *layout,
                                        gint          offset);


gchar *_gtk_pango_get_text_before (PangoLayout     *layout,
                                   AtkTextBoundary  boundary_type,
                                   gint             offset,
                                   gint            *start_offset,
                                   gint            *end_offset);
gchar *_gtk_pango_get_text_at     (PangoLayout     *layout,
                                   AtkTextBoundary  boundary_type,
                                   gint             offset,
                                   gint            *start_offset,
                                   gint            *end_offset);
gchar *_gtk_pango_get_text_after  (PangoLayout     *layout,
                                   AtkTextBoundary  boundary_type,
                                   gint             offset,
                                   gint            *start_offset,
                                   gint            *end_offset);

PangoAttrList *_gtk_pango_attr_list_merge (PangoAttrList *into,
                                           PangoAttrList *from);

PangoDirection _gtk_pango_find_base_dir (const gchar *text,
                                         gint         length);

G_END_DECLS

#endif /* __GTK_PANGO_H__ */
