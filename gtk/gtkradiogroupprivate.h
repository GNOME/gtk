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

#ifndef __GTK_RADIO_GROUP_PRIVATE_H__
#define __GTK_RADIO_GROUP_PRIVATE_H__

#include <gtk/gtkradiogroup.h>

G_BEGIN_DECLS

void     _gtk_radio_group_add_item            (GtkRadioGroup *radio_group,
					       GObject       *item);
void     _gtk_radio_group_remove_item         (GtkRadioGroup *radio_group,
					       GObject       *item);
gboolean _gtk_radio_group_is_empty            (GtkRadioGroup *radio_group);
GObject *_gtk_radio_group_get_singleton       (GtkRadioGroup *radio_group);
void     _gtk_radio_group_set_active_item     (GtkRadioGroup *radio_group,
					       GObject       *item);
void     _gtk_radio_group_emit_active_changed (GtkRadioGroup *radio_group);

G_END_DECLS

#endif /* __GTK_RADIO_GROUP_PRIVATE_H__ */
