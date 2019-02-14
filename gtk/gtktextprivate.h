/* gtkentryprivate.h
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TEXT_PRIVATE_H__
#define __GTK_TEXT_PRIVATE_H__

#include "gtktext.h"

#include "gtkeventcontroller.h"

G_BEGIN_DECLS

gchar*   _gtk_text_get_display_text       (GtkText *entry,
                                            gint      start_pos,
                                            gint      end_pos);
GtkIMContext* _gtk_text_get_im_context    (GtkText  *entry);
void     gtk_text_enter_text              (GtkText   *entry,
                                            const char *text);
void     gtk_text_set_positions           (GtkText   *entry,
                                            int         current_pos,
                                            int         selection_bound);

GtkEventController * gtk_text_get_key_controller (GtkText *entry);

G_END_DECLS

#endif /* __GTK_TEXT_PRIVATE_H__ */
