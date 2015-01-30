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

#ifndef __GTK_WIDGET_PRIVATE_H__
#define __GTK_WIDGET_PRIVATE_H__

#include "gtkcsstypesprivate.h"
#include "gtkwidget.h"
#include "gtkeventcontroller.h"
#include "gtkactionmuxer.h"

G_BEGIN_DECLS

void         _gtk_widget_set_visible_flag   (GtkWidget *widget,
                                             gboolean   visible);
gboolean     _gtk_widget_get_in_reparent    (GtkWidget *widget);
void         _gtk_widget_set_in_reparent    (GtkWidget *widget,
                                             gboolean   in_reparent);
gboolean     _gtk_widget_get_anchored       (GtkWidget *widget);
void         _gtk_widget_set_anchored       (GtkWidget *widget,
                                             gboolean   anchored);
gboolean     _gtk_widget_get_shadowed       (GtkWidget *widget);
void         _gtk_widget_set_shadowed       (GtkWidget *widget,
                                             gboolean   shadowed);
gboolean     _gtk_widget_get_alloc_needed   (GtkWidget *widget);
void         _gtk_widget_set_alloc_needed   (GtkWidget *widget,
                                             gboolean   alloc_needed);
void         _gtk_widget_draw               (GtkWidget *widget,
					     cairo_t   *cr);
void          _gtk_widget_scale_changed     (GtkWidget *widget);


void         _gtk_widget_add_sizegroup         (GtkWidget    *widget,
						gpointer      group);
void         _gtk_widget_remove_sizegroup      (GtkWidget    *widget,
						gpointer      group);
GSList      *_gtk_widget_get_sizegroups        (GtkWidget    *widget);

void         _gtk_widget_add_attached_window    (GtkWidget    *widget,
                                                 GtkWindow    *window);
void         _gtk_widget_remove_attached_window (GtkWidget    *widget,
                                                 GtkWindow    *window);

void _gtk_widget_override_size_request (GtkWidget *widget,
                                        int        width,
                                        int        height,
                                        int       *old_width,
                                        int       *old_height);
void _gtk_widget_restore_size_request  (GtkWidget *widget,
                                        int        old_width,
                                        int        old_height);
void _gtk_widget_get_preferred_size_for_size   (GtkWidget         *widget,
                                                GtkOrientation     orientation,
                                                gint               size,
                                                gint              *minimum,
                                                gint              *natural,
                                                gint              *minimum_baseline,
                                                gint              *natural_baseline);
void _gtk_widget_get_preferred_size_and_baseline(GtkWidget        *widget,
                                                GtkRequisition    *minimum_size,
                                                GtkRequisition    *natural_size,
                                                gint              *minimum_baseline,
                                                gint              *natural_baseline);
gboolean _gtk_widget_has_baseline_support (GtkWidget *widget);

gboolean _gtk_widget_get_translation_to_window (GtkWidget      *widget,
                                                GdkWindow      *window,
                                                int            *x,
                                                int            *y);

const gchar*      _gtk_widget_get_accel_path               (GtkWidget *widget,
                                                            gboolean  *locked);

AtkObject *       _gtk_widget_peek_accessible              (GtkWidget *widget);

GdkWindow *       _gtk_cairo_get_event_window              (cairo_t *cr);
GdkEventExpose *  _gtk_cairo_get_event                     (cairo_t *cr);

void              _gtk_widget_set_has_default              (GtkWidget *widget,
                                                            gboolean   has_default);
void              _gtk_widget_set_has_grab                 (GtkWidget *widget,
                                                            gboolean   has_grab);
void              _gtk_widget_set_is_toplevel              (GtkWidget *widget,
                                                            gboolean   is_toplevel);

void              _gtk_widget_grab_notify                  (GtkWidget *widget,
                                                            gboolean   was_grabbed);

void              _gtk_widget_propagate_hierarchy_changed  (GtkWidget *widget,
                                                            GtkWidget *previous_toplevel);
void              _gtk_widget_propagate_screen_changed     (GtkWidget *widget,
                                                            GdkScreen *previous_screen);
void              _gtk_widget_propagate_composited_changed (GtkWidget *widget);

void              _gtk_widget_set_device_window            (GtkWidget *widget,
                                                            GdkDevice *device,
                                                            GdkWindow *pointer_window);
GdkWindow *       _gtk_widget_get_device_window            (GtkWidget *widget,
                                                            GdkDevice *device);
GList *           _gtk_widget_list_devices                 (GtkWidget *widget);

void              _gtk_widget_synthesize_crossing          (GtkWidget       *from,
                                                            GtkWidget       *to,
                                                            GdkDevice       *device,
                                                            GdkCrossingMode  mode);

gpointer          _gtk_widget_peek_request_cache           (GtkWidget *widget);

void              _gtk_widget_buildable_finish_accelerator (GtkWidget *widget,
                                                            GtkWidget *toplevel,
                                                            gpointer   user_data);
GtkStyle *        _gtk_widget_get_style                    (GtkWidget *widget);
void              _gtk_widget_set_style                    (GtkWidget *widget,
                                                            GtkStyle  *style);
gboolean          _gtk_widget_supports_clip                (GtkWidget *widget);
void              _gtk_widget_set_simple_clip              (GtkWidget *widget,
                                                            GtkAllocation *content_clip);

typedef gboolean (*GtkCapturedEventHandler) (GtkWidget *widget, GdkEvent *event);

void              _gtk_widget_set_captured_event_handler (GtkWidget               *widget,
                                                          GtkCapturedEventHandler  handler);

gboolean          _gtk_widget_captured_event               (GtkWidget *widget,
                                                            GdkEvent  *event);

GtkWidgetPath *   _gtk_widget_create_path                  (GtkWidget    *widget);
void              _gtk_widget_invalidate_style_context     (GtkWidget    *widget,
                                                            GtkCssChange  change);
void              _gtk_widget_style_context_invalidated    (GtkWidget    *widget);

void              _gtk_widget_update_parent_muxer          (GtkWidget    *widget);
GtkActionMuxer *  _gtk_widget_get_action_muxer             (GtkWidget    *widget);
gchar **          _gtk_widget_list_action_prefixes         (GtkWidget    *widget);
GActionGroup *    _gtk_widget_get_action_group             (GtkWidget    *widget,
                                                            const gchar  *prefix);

void              _gtk_widget_add_controller               (GtkWidget           *widget,
                                                            GtkEventController  *controller);
void              _gtk_widget_remove_controller            (GtkWidget           *widget,
                                                            GtkEventController  *controller);
GList *           _gtk_widget_list_controllers             (GtkWidget           *widget,
                                                            GtkPropagationPhase  phase);

void              gtk_drag_cancel                          (GdkDragContext *context);

G_END_DECLS

#endif /* __GTK_WIDGET_PRIVATE_H__ */
