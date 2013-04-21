
/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __GTK_WINDOW_PRIVATE_H__
#define __GTK_WINDOW_PRIVATE_H__

#include <gdk/gdk.h>

#include "gtkwindow.h"

G_BEGIN_DECLS

void            _gtk_window_internal_set_focus (GtkWindow *window,
                                                GtkWidget *focus);
void            _gtk_window_reposition         (GtkWindow *window,
                                                gint       x,
                                                gint       y);
void            _gtk_window_group_add_grab    (GtkWindowGroup *window_group,
                                               GtkWidget      *widget);
void            _gtk_window_group_remove_grab (GtkWindowGroup *window_group,
                                               GtkWidget      *widget);
void            _gtk_window_group_add_device_grab    (GtkWindowGroup   *window_group,
                                                      GtkWidget        *widget,
                                                      GdkDevice        *device,
                                                      gboolean          block_others);
void            _gtk_window_group_remove_device_grab (GtkWindowGroup   *window_group,
                                                      GtkWidget        *widget,
                                                      GdkDevice        *device);

gboolean        _gtk_window_group_widget_is_blocked_for_device (GtkWindowGroup *window_group,
                                                                GtkWidget      *widget,
                                                                GdkDevice      *device);

void            _gtk_window_set_has_toplevel_focus (GtkWindow *window,
                                                    gboolean   has_toplevel_focus);
void            _gtk_window_unset_focus_and_default (GtkWindow *window,
                                                     GtkWidget *widget);

void            _gtk_window_set_is_active          (GtkWindow *window,
                                                    gboolean   is_active);

void            _gtk_window_set_is_toplevel        (GtkWindow *window,
                                                    gboolean   is_toplevel);

void            _gtk_window_get_wmclass            (GtkWindow  *window,
                                                    gchar     **wmclass_name,
                                                    gchar     **wmclass_class);

void            _gtk_window_set_allocation         (GtkWindow           *window,
                                                    const GtkAllocation *allocation,
                                                    GtkAllocation       *allocation_out);

typedef void (*GtkWindowKeysForeachFunc) (GtkWindow      *window,
                                          guint           keyval,
                                          GdkModifierType modifiers,
                                          gboolean        is_mnemonic,
                                          gpointer        data);

void _gtk_window_keys_foreach (GtkWindow               *window,
                               GtkWindowKeysForeachFunc func,
                               gpointer                 func_data);

/* --- internal (GtkAcceleratable) --- */
gboolean        _gtk_window_query_nonaccels     (GtkWindow      *window,
                                                 guint           accel_key,
                                                 GdkModifierType accel_mods);

void            _gtk_window_schedule_mnemonics_visible (GtkWindow *window);

G_END_DECLS

#endif /* __GTK_WINDOW_PRIVATE_H__ */
