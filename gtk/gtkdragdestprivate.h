
/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Matthias Clasen
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

#ifndef __GTK_DRAG_DEST_PRIVATE_H__
#define __GTK_DRAG_DEST_PRIVATE_H__

#include "gtkdragdest.h"

G_BEGIN_DECLS

void gtk_drop_target_emit_drag_data_received (GtkDropTarget    *dest,
                                              GdkDrop          *drop,
                                              GtkSelectionData *sdata);
void gtk_drop_target_emit_drag_leave         (GtkDropTarget    *dest,
                                              GdkDrop          *drop,
                                              guint             time);
gboolean gtk_drop_target_emit_drag_motion    (GtkDropTarget    *dest,
                                              GdkDrop          *drop,
                                              int               x,
                                              int               y);
gboolean gtk_drop_target_emit_drag_drop      (GtkDropTarget    *dest,
                                              GdkDrop          *drop,
                                              int               x,
                                              int               y);

const char * gtk_drop_target_match           (GtkDropTarget *dest,
                                              GdkDrop       *drop);

G_END_DECLS

#endif
