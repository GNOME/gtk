
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

gboolean _gtk_window_check_handle_wm_event (GdkEvent  *event);

/* --- internal (GtkAcceleratable) --- */
gboolean        _gtk_window_query_nonaccels     (GtkWindow      *window,
                                                 guint           accel_key,
                                                 GdkModifierType accel_mods);

void            _gtk_window_schedule_mnemonics_visible (GtkWindow *window);

void            _gtk_window_notify_keys_changed (GtkWindow *window);

gboolean        _gtk_window_titlebar_shows_app_menu (GtkWindow *window);

void            _gtk_window_get_shadow_width (GtkWindow *window,
                                              GtkBorder *border);

void            _gtk_window_toggle_maximized (GtkWindow *window);

void            _gtk_window_request_csd (GtkWindow *window);

/* Window groups */

GtkWindowGroup *_gtk_window_get_window_group (GtkWindow *window);

void            _gtk_window_set_window_group (GtkWindow      *window,
                                              GtkWindowGroup *group);

/* Popovers */
void    _gtk_window_add_popover          (GtkWindow                   *window,
                                          GtkWidget                   *popover);
void    _gtk_window_remove_popover       (GtkWindow                   *window,
                                          GtkWidget                   *popover);
void    _gtk_window_set_popover_position (GtkWindow                   *window,
                                          GtkWidget                   *popover,
                                          GtkPositionType              pos,
                                          const cairo_rectangle_int_t *rect);
void    _gtk_window_get_popover_position (GtkWindow                   *window,
                                          GtkWidget                   *popover,
                                          GtkPositionType             *pos,
                                          cairo_rectangle_int_t       *rect);

GdkPixbuf *gtk_window_get_icon_for_size (GtkWindow *window,
                                         gint       size);

void       gtk_window_set_use_subsurface (GtkWindow *window,
                                          gboolean   use_subsurface);
void       gtk_window_set_hardcoded_window (GtkWindow *window,
                                            GdkWindow *gdk_window);

G_END_DECLS

#endif /* __GTK_WINDOW_PRIVATE_H__ */
