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
#include "gactionmuxer.h"

G_BEGIN_DECLS

/* Cache as many ranges of height-for-width
 * (or width-for-height) as can be rational
 * for a said widget to have, if a label can
 * only wrap to 3 lines, only 3 caches will
 * ever be allocated for it.
 */
#define GTK_SIZE_REQUEST_CACHED_SIZES   (5)

typedef struct {
  gint minimum_size;
  gint natural_size;
} CachedSize;

typedef struct
{
  gint       lower_for_size; /* The minimum for_size with the same result */
  gint       upper_for_size; /* The maximum for_size with the same result */
  CachedSize cached_size;
} SizeRequest;

typedef struct {
  SizeRequest **widths;
  SizeRequest **heights;

  CachedSize  cached_width;
  CachedSize  cached_height;

  guint       cached_widths      : 3;
  guint       cached_heights     : 3;
  guint       last_cached_width  : 3;
  guint       last_cached_height : 3;
  guint       cached_base_width  : 1;
  guint       cached_base_height : 1;
} SizeRequestCache;

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
gboolean     _gtk_widget_get_width_request_needed  (GtkWidget *widget);
void         _gtk_widget_set_width_request_needed  (GtkWidget *widget,
                                                    gboolean   width_request_needed);
gboolean     _gtk_widget_get_height_request_needed (GtkWidget *widget);
void         _gtk_widget_set_height_request_needed (GtkWidget *widget,
                                                    gboolean   height_request_needed);

gboolean     _gtk_widget_get_sizegroup_visited (GtkWidget    *widget);
void         _gtk_widget_set_sizegroup_visited (GtkWidget    *widget,
						gboolean      visited);
gboolean     _gtk_widget_get_sizegroup_bumping (GtkWidget    *widget);
void         _gtk_widget_set_sizegroup_bumping (GtkWidget    *widget,
						gboolean      bumping);
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

gboolean _gtk_widget_get_translation_to_window (GtkWidget      *widget,
                                                GdkWindow      *window,
                                                int            *x,
                                                int            *y);
void     _gtk_widget_free_cached_sizes (GtkWidget *widget);

const gchar*      _gtk_widget_get_accel_path               (GtkWidget *widget,
                                                            gboolean  *locked);

AtkObject *       _gtk_widget_peek_accessible              (GtkWidget *widget);

GdkEventExpose *  _gtk_cairo_get_event                     (cairo_t *cr);

void              _gtk_widget_draw_internal                (GtkWidget *widget,
                                                            cairo_t   *cr,
                                                            gboolean   clip_to_size);
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
GActionMuxer *    _gtk_widget_get_action_muxer             (GtkWidget    *widget);

G_END_DECLS

#endif /* __GTK_WIDGET_PRIVATE_H__ */
