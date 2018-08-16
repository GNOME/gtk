
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
#include "gtkpointerfocusprivate.h"

G_BEGIN_DECLS

void            _gtk_window_internal_set_focus (GtkWindow *window,
                                                GtkWidget *focus);
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

void            _gtk_window_unset_focus_and_default (GtkWindow *window,
                                                     GtkWidget *widget);

void            _gtk_window_set_allocation         (GtkWindow     *window,
                                                    int            width,
                                                    int            height,
                                                    GtkAllocation *allocation_out);
void            gtk_window_check_resize            (GtkWindow     *self);

typedef void (*GtkWindowKeysForeachFunc) (GtkWindow      *window,
                                          guint           keyval,
                                          GdkModifierType modifiers,
                                          gpointer        data);

gboolean gtk_window_emit_close_request (GtkWindow *window);
gboolean gtk_window_configure    (GtkWindow         *window,
                                  guint              width,
                                  guint              height);

/* --- internal (GtkAcceleratable) --- */
gboolean        _gtk_window_query_nonaccels     (GtkWindow      *window,
                                                 guint           accel_key,
                                                 GdkModifierType accel_mods);

gboolean         gtk_window_activate_key (GtkWindow   *window,
                                          GdkEventKey *event);

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
                                          GtkWidget                   *popover,
                                          GtkWidget                   *popover_parent,
                                          gboolean                     clamp_allocation);
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
void    _gtk_window_raise_popover        (GtkWindow                   *window,
                                          GtkWidget                   *popover);

GtkWidget * _gtk_window_get_popover_parent (GtkWindow *window,
                                            GtkWidget *popover);
gboolean    _gtk_window_is_popover_widget  (GtkWindow *window,
                                            GtkWidget *popover);

GdkTexture *     gtk_window_get_icon_for_size (GtkWindow *window,
					       int        size);

void       gtk_window_set_hardcoded_surface (GtkWindow *window,
					     GdkSurface *surface);

/* Exported handles */

typedef void (*GtkWindowHandleExported)  (GtkWindow               *window,
                                          const char              *handle,
                                          gpointer                 user_data);

gboolean      gtk_window_export_handle   (GtkWindow               *window,
                                          GtkWindowHandleExported  callback,
                                          gpointer                 user_data);
void          gtk_window_unexport_handle (GtkWindow               *window);

GtkWidget *      gtk_window_lookup_pointer_focus_widget (GtkWindow        *window,
                                                         GdkDevice        *device,
                                                         GdkEventSequence *sequence);
GtkWidget *      gtk_window_lookup_effective_pointer_focus_widget (GtkWindow        *window,
                                                                   GdkDevice        *device,
                                                                   GdkEventSequence *sequence);
GtkWidget *      gtk_window_lookup_pointer_focus_implicit_grab (GtkWindow        *window,
                                                                GdkDevice        *device,
                                                                GdkEventSequence *sequence);

void             gtk_window_update_pointer_focus (GtkWindow        *window,
                                                  GdkDevice        *device,
                                                  GdkEventSequence *sequence,
                                                  GtkWidget        *target,
                                                  gdouble           x,
                                                  gdouble           y);
void             gtk_window_set_pointer_focus_grab (GtkWindow        *window,
                                                    GdkDevice        *device,
                                                    GdkEventSequence *sequence,
                                                    GtkWidget        *grab_widget);

void             gtk_window_update_pointer_focus_on_state_change (GtkWindow *window,
                                                                  GtkWidget *widget);

void             gtk_window_maybe_revoke_implicit_grab (GtkWindow *window,
                                                        GdkDevice *device,
                                                        GtkWidget *grab_widget);
void             gtk_window_maybe_update_cursor (GtkWindow *window,
                                                 GtkWidget *widget,
                                                 GdkDevice *device);
GtkWidget *      gtk_window_pick_popover (GtkWindow   *window,
                                          double       x,
                                          double       y,
                                          GtkPickFlags flags);

G_END_DECLS

#endif /* __GTK_WINDOW_PRIVATE_H__ */
