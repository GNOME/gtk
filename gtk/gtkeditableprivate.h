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

#ifndef __GTK_EDITABLE_PRIVATE_H__
#define __GTK_EDITABLE_PRIVATE_H__


#include <gtk/gtkeditable.h>

G_BEGIN_DECLS

enum {
  GTK_EDITABLE_PROP_TEXT = 0x1000,
  GTK_EDITABLE_PROP_CURSOR_POSITION,
  GTK_EDITABLE_PROP_SELECTION_BOUND,
  GTK_EDITABLE_PROP_EDITABLE,
  GTK_EDITABLE_PROP_WIDTH_CHARS,
  GTK_EDITABLE_PROP_MAX_WIDTH_CHARS,
  GTK_EDITABLE_PROP_XALIGN
};

void         gtk_editable_install_properties  (GObjectClass         *class);
void         gtk_editable_delegate_iface_init (GtkEditableInterface *iface);
void         gtk_editable_set_delegate        (GtkEditable          *editable,
                                               GtkEditable          *delegate);
gboolean     gtk_editable_delegate_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);
gboolean     gtk_editable_delegate_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);

G_END_DECLS

#endif /* __GTK_EDITABLE_PRIVATE_H__ */
