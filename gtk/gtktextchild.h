/* GTK - The GIMP Toolkit
 * gtktextchild.h Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef GTK_TEXT_CHILD_H
#define GTK_TEXT_CHILD_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* A GtkTextChildAnchor is a spot in the buffer where child widgets
 * can be "anchored" (inserted inline, as if they were characters).
 * The anchor can have multiple widgets anchored, to allow for multiple
 * views.
 */

typedef struct _GtkTextChildAnchor GtkTextChildAnchor;

void     gtk_text_child_anchor_ref         (GtkTextChildAnchor *anchor);
void     gtk_text_child_anchor_unref       (GtkTextChildAnchor *anchor);
GList*   gtk_text_child_anchor_get_widgets (GtkTextChildAnchor *anchor);
gboolean gtk_text_child_anchor_get_deleted (GtkTextChildAnchor *anchor);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
