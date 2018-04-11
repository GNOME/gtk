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
#include "gtkcontainer.h"
#include "gtkeventcontroller.h"
#include "gtkactionmuxer.h"
#include "gtksizerequestcacheprivate.h"

G_BEGIN_DECLS

#define GTK_STATE_FLAGS_BITS 13

struct _GtkWidgetPrivate
{
  /* The state of the widget. Needs to be able to hold all GtkStateFlags bits
   * (defined in "gtkenums.h").
   */
  guint state_flags : GTK_STATE_FLAGS_BITS;

  guint direction             : 2;

#ifdef G_ENABLE_DEBUG
  guint highlight_resize      : 1;
#endif

  guint in_destruction        : 1;
  guint toplevel              : 1;
  guint anchored              : 1;
  guint composite_child       : 1;
  guint no_window             : 1;
  guint realized              : 1;
  guint mapped                : 1;
  guint visible               : 1;
  guint sensitive             : 1;
  guint can_focus             : 1;
  guint has_focus             : 1;
  guint focus_on_click        : 1;
  guint can_default           : 1;
  guint has_default           : 1;
  guint receives_default      : 1;
  guint has_grab              : 1;
  guint shadowed              : 1;
  guint app_paintable         : 1;
  guint double_buffered       : 1;
  guint redraw_on_alloc       : 1;
  guint no_show_all           : 1;
  guint child_visible         : 1;
  guint multidevice           : 1;
  guint has_shape_mask        : 1;
  guint in_reparent           : 1;

  /* Queue-resize related flags */
  guint resize_needed         : 1; /* queue_resize() has been called but no get_preferred_size() yet */
  guint alloc_needed          : 1; /* this widget needs a size_allocate() call */
  guint alloc_needed_on_child : 1; /* 0 or more children - or this widget - need a size_allocate() call */

  /* Expand-related flags */
  guint need_compute_expand   : 1; /* Need to recompute computed_[hv]_expand */
  guint computed_hexpand      : 1; /* computed results (composite of child flags) */
  guint computed_vexpand      : 1;
  guint hexpand               : 1; /* application-forced expand */
  guint vexpand               : 1;
  guint hexpand_set           : 1; /* whether to use application-forced  */
  guint vexpand_set           : 1; /* instead of computing from children */
  guint has_tooltip           : 1;
  guint frameclock_connected  : 1;

  /* SizeGroup related flags */
  guint have_size_groups      : 1;

  /* Alignment */
  guint   halign              : 4;
  guint   valign              : 4;

  guint8 alpha;
  guint8 user_alpha;

#ifdef G_ENABLE_CONSISTENCY_CHECKS
  /* Number of gtk_widget_push_verify_invariants () */
  guint8 verifying_invariants_count;
#endif

  gint width;
  gint height;
  GtkBorder margin;

  /* Animations and other things to update on clock ticks */
  guint clock_tick_id;
  GList *tick_callbacks;

  /* The widget's name. If the widget does not have a name
   * (the name is NULL), then its name (as returned by
   * "gtk_widget_get_name") is its class's name.
   * Among other things, the widget name is used to determine
   * the style to use for a widget.
   */
  gchar *name;

  /* The list of attached windows to this widget.
   * We keep a list in order to call reset_style to all of them,
   * recursively.
   */
  GList *attached_windows;

  /* The style for the widget. The style contains the
   * colors the widget should be drawn in for each state
   * along with graphics contexts used to draw with and
   * the font to use for text.
   */
  GtkStyle *style;
  GtkCssNode *cssnode;
  GtkStyleContext *context;

  /* The widget's allocated size */
  GtkAllocation allocated_size;
  gint allocated_size_baseline;
  GtkAllocation allocation;
  GtkAllocation clip;
  gint allocated_baseline;

  /* The widget's requested sizes */
  SizeRequestCache requests;

  /* The widget's window or its parent window if it does
   * not have a window. (Which will be indicated by the
   * no_window field being set).
   */
  GdkWindow *window;
  GList *registered_windows;

  /* The widget's parent */
  GtkWidget *parent;

  GList *event_controllers;

  AtkObject *accessible;
};

GtkCssNode *  gtk_widget_get_css_node       (GtkWidget *widget);
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
gboolean     gtk_widget_needs_allocate      (GtkWidget *widget);
void         gtk_widget_queue_resize_on_widget (GtkWidget *widget);
void         gtk_widget_ensure_resize       (GtkWidget *widget);
void         gtk_widget_ensure_allocate     (GtkWidget *widget);
void         gtk_widget_draw_internal       (GtkWidget *widget,
					     cairo_t   *cr,
                                             gboolean   do_clip);
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

const gchar*      _gtk_widget_get_accel_path               (GtkWidget *widget,
                                                            gboolean  *locked);

AtkObject *       _gtk_widget_peek_accessible              (GtkWidget *widget);

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

static inline gpointer _gtk_widget_peek_request_cache           (GtkWidget *widget);

void              _gtk_widget_buildable_finish_accelerator (GtkWidget *widget,
                                                            GtkWidget *toplevel,
                                                            gpointer   user_data);
GtkStyleContext * _gtk_widget_peek_style_context           (GtkWidget *widget);
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
void              gtk_widget_clear_path                    (GtkWidget    *widget);
void              _gtk_widget_invalidate_style_context     (GtkWidget    *widget,
                                                            GtkCssChange  change);
void              _gtk_widget_style_context_invalidated    (GtkWidget    *widget);

void              _gtk_widget_update_parent_muxer          (GtkWidget    *widget);
GtkActionMuxer *  _gtk_widget_get_action_muxer             (GtkWidget    *widget,
                                                            gboolean      create);

void              _gtk_widget_add_controller               (GtkWidget           *widget,
                                                            GtkEventController  *controller);
void              _gtk_widget_remove_controller            (GtkWidget           *widget,
                                                            GtkEventController  *controller);
GList *           _gtk_widget_list_controllers             (GtkWidget           *widget,
                                                            GtkPropagationPhase  phase);
gboolean          _gtk_widget_consumes_motion              (GtkWidget           *widget,
                                                            GdkEventSequence    *sequence);

gboolean          gtk_widget_has_tick_callback             (GtkWidget *widget);

void              gtk_widget_set_csd_input_shape           (GtkWidget            *widget,
                                                            const cairo_region_t *region);

gboolean          gtk_widget_has_size_request              (GtkWidget *widget);

void              gtk_widget_reset_controllers             (GtkWidget *widget);

gboolean          gtk_widget_query_tooltip                 (GtkWidget  *widget,
                                                            gint        x,
                                                            gint        y,
                                                            gboolean    keyboard_mode,
                                                            GtkTooltip *tooltip);

void              gtk_widget_render                        (GtkWidget            *widget,
                                                            GdkWindow            *window,
                                                            const cairo_region_t *region);


/* inline getters */

static inline gboolean
gtk_widget_get_resize_needed (GtkWidget *widget)
{
  return widget->priv->resize_needed;
}

static inline GtkWidget *
_gtk_widget_get_parent (GtkWidget *widget)
{
  return widget->priv->parent;
}

static inline gboolean
_gtk_widget_get_visible (GtkWidget *widget)
{
  return widget->priv->visible;
}

static inline gboolean
_gtk_widget_get_child_visible (GtkWidget *widget)
{
  return widget->priv->child_visible;
}

static inline gboolean
_gtk_widget_get_mapped (GtkWidget *widget)
{
  return widget->priv->mapped;
}

static inline gboolean
_gtk_widget_is_drawable (GtkWidget *widget)
{
  return widget->priv->visible && widget->priv->mapped;
}

static inline gboolean
_gtk_widget_get_has_window (GtkWidget *widget)
{
  return !widget->priv->no_window;
}

static inline gboolean
_gtk_widget_get_realized (GtkWidget *widget)
{
  return widget->priv->realized;
}

static inline gboolean
_gtk_widget_is_toplevel (GtkWidget *widget)
{
  return widget->priv->toplevel;
}

static inline GtkStateFlags
_gtk_widget_get_state_flags (GtkWidget *widget)
{
  return widget->priv->state_flags;
}

extern GtkTextDirection gtk_default_direction;

static inline GtkTextDirection
_gtk_widget_get_direction (GtkWidget *widget)
{
  if (widget->priv->direction == GTK_TEXT_DIR_NONE)
    return gtk_default_direction;
  else
    return widget->priv->direction;
}

static inline GtkWidget *
_gtk_widget_get_toplevel (GtkWidget *widget)
{
  while (widget->priv->parent)
    widget = widget->priv->parent;

  return widget;
}

static inline GtkStyleContext *
_gtk_widget_get_style_context (GtkWidget *widget)
{
  if (G_LIKELY (widget->priv->context))
    return widget->priv->context;

  return gtk_widget_get_style_context (widget);
}

static inline gpointer
_gtk_widget_peek_request_cache (GtkWidget *widget)
{
  return &widget->priv->requests;
}

static inline GdkWindow *
_gtk_widget_get_window (GtkWidget *widget)
{
  return widget->priv->window;
}

static inline void
_gtk_widget_get_allocation (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  *allocation = widget->priv->allocation;
}

G_END_DECLS

#endif /* __GTK_WIDGET_PRIVATE_H__ */
