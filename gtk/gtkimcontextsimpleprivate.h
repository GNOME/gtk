/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
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

#ifndef __GTK_IM_CONTEXT_SIMPLE_PRIVATE_H__
#define __GTK_IM_CONTEXT_SIMPLE_PRIVATE_H__

#include <glib.h>

#include "gdk/gdkkeysyms.h"

G_BEGIN_DECLS

extern const GtkComposeTableCompact gtk_compose_table_compact;

static const guint16 gtk_compose_ignore[] = {
  GDK_KEY_Shift_L,
  GDK_KEY_Shift_R,
  GDK_KEY_Control_L,
  GDK_KEY_Control_R,
  GDK_KEY_Caps_Lock,
  GDK_KEY_Shift_Lock,
  GDK_KEY_Meta_L,
  GDK_KEY_Meta_R,
  GDK_KEY_Alt_L,
  GDK_KEY_Alt_R,
  GDK_KEY_Super_L,
  GDK_KEY_Super_R,
  GDK_KEY_Hyper_L,
  GDK_KEY_Hyper_R,
  GDK_KEY_Mode_switch,
  GDK_KEY_ISO_Level3_Shift
};

gboolean gtk_check_algorithmically (const guint16                *compose_buffer,
                                    gint                          n_compose,
                                    gunichar                     *output);
gboolean gtk_check_compact_table   (const GtkComposeTableCompact *table,
                                    guint16                      *compose_buffer,
                                    gint                          n_compose,
                                    gboolean                     *compose_finish,
                                    gboolean                     *compose_match,
                                    gunichar                     *output_char);

G_END_DECLS


#endif /* __GTK_IM_CONTEXT_SIMPLE_PRIVATE_H__ */
