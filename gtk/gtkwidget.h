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

#ifndef __GTK_WIDGET_H__
#define __GTK_WIDGET_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gsk/gsk.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkborder.h>
#include <gtk/gtktypes.h>
#include <atk/atk.h>

G_BEGIN_DECLS

/* Macro for casting a pointer to a GtkWidget or GtkWidgetClass pointer.
 * Macros for testing whether widget or klass are of type GTK_TYPE_WIDGET.
 */
#define GTK_TYPE_WIDGET                   (gtk_widget_get_type ())
#define GTK_WIDGET(widget)                (G_TYPE_CHECK_INSTANCE_CAST ((widget), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_WIDGET_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WIDGET, GtkWidgetClass))
#define GTK_IS_WIDGET(widget)             (G_TYPE_CHECK_INSTANCE_TYPE ((widget), GTK_TYPE_WIDGET))
#define GTK_IS_WIDGET_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WIDGET))
#define GTK_WIDGET_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WIDGET, GtkWidgetClass))

#define GTK_TYPE_REQUISITION              (gtk_requisition_get_type ())

typedef struct _GtkWidgetPrivate       GtkWidgetPrivate;
typedef struct _GtkWidgetClass         GtkWidgetClass;
typedef struct _GtkWidgetClassPrivate  GtkWidgetClassPrivate;

/**
 * GtkAllocation:
 * @x: the X position of the widget’s area relative to its parents allocation.
 * @y: the Y position of the widget’s area relative to its parents allocation.
 * @width: the width of the widget’s allocated area.
 * @height: the height of the widget’s allocated area.
 *
 * A #GtkAllocation-struct of a widget represents region
 * which has been allocated to the widget by its parent. It is a subregion
 * of its parents allocation. See
 * [GtkWidget’s geometry management section][geometry-management] for
 * more information.
 */
typedef         GdkRectangle       GtkAllocation;

/**
 * GtkCallback:
 * @widget: the widget to operate on
 * @data: (closure): user-supplied data
 *
 * The type of the callback functions used for e.g. iterating over
 * the children of a container, see gtk_container_foreach().
 */
typedef void    (*GtkCallback)     (GtkWidget        *widget,
                                    gpointer          data);

/**
 * GtkTickCallback:
 * @widget: the widget
 * @frame_clock: the frame clock for the widget (same as calling gtk_widget_get_frame_clock())
 * @user_data: user data passed to gtk_widget_add_tick_callback().
 *
 * Callback type for adding a function to update animations. See gtk_widget_add_tick_callback().
 *
 * Returns: %G_SOURCE_CONTINUE if the tick callback should continue to be called,
 *  %G_SOURCE_REMOVE if the tick callback should be removed.
 */
typedef gboolean (*GtkTickCallback) (GtkWidget     *widget,
                                     GdkFrameClock *frame_clock,
                                     gpointer       user_data);

/**
 * GtkRequisition:
 * @width: the widget’s desired width
 * @height: the widget’s desired height
 *
 * A #GtkRequisition-struct represents the desired size of a widget. See
 * [GtkWidget’s geometry management section][geometry-management] for
 * more information.
 */
struct _GtkRequisition
{
  gint width;
  gint height;
};

/* The widget is the base of the tree for displayable objects.
 *  (A displayable object is one which takes up some amount
 *  of screen real estate). It provides a common base and interface
 *  which actual widgets must adhere to.
 */
struct _GtkWidget
{
  GInitiallyUnowned parent_instance;

  /*< private >*/

  GtkWidgetPrivate *priv;
};

/**
 * GtkWidgetClass:
 * @parent_class: The object class structure needs to be the first
 *   element in the widget class structure in order for the class mechanism
 *   to work correctly. This allows a GtkWidgetClass pointer to be cast to
 *   a GObjectClass pointer.
 * @activate_signal: The signal to emit when a widget of this class is
 *   activated, gtk_widget_activate() handles the emission.
 *   Implementation of this signal is optional.
 * @destroy: Signals that all holders of a reference to the widget
 *   should release the reference that they hold.
 * @show: Signal emitted when widget is shown
 * @hide: Signal emitted when widget is hidden.
 * @map: Signal emitted when widget is going to be mapped, that is
 *   when the widget is visible (which is controlled with
 *   gtk_widget_set_visible()) and all its parents up to the toplevel
 *   widget are also visible.
 * @unmap: Signal emitted when widget is going to be unmapped, which
 *   means that either it or any of its parents up to the toplevel
 *   widget have been set as hidden.
 * @realize: Signal emitted when widget is associated with a
 *   #GdkSurface, which means that gtk_widget_realize() has been called or
 *   the widget has been mapped (that is, it is going to be drawn).
 * @unrealize: Signal emitted when the GdkSurface associated with
 *   widget is destroyed, which means that gtk_widget_unrealize() has
 *   been called or the widget has been unmapped (that is, it is going
 *   to be hidden).
 * @root: Called when the widget gets added to a #GtkRoot widget. Must chain up
 * @unroot: Called when the widget is about to be removed from its #GtkRoot widget. Must chain up
 * @size_allocate: Signal emitted to get the widget allocation.
 * @state_flags_changed: Signal emitted when the widget state changes,
 *   see gtk_widget_get_state_flags().
 * @direction_changed: Signal emitted when the text direction of a
 *   widget changes.
 * @grab_notify: Signal emitted when a widget becomes shadowed by a
 *   GTK+ grab (not a pointer or keyboard grab) on another widget, or
 *   when it becomes unshadowed due to a grab being removed.
 * @child_notify: Signal emitted for each child property that has
 *   changed on an object.
 * @get_request_mode: This allows a widget to tell its parent container whether
 *   it prefers to be allocated in %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH or
 *   %GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT mode.
 *   %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH means the widget prefers to have
 *   #GtkWidgetClass.measure() called first to get the default width (passing
 *   a for_size of -1), then again to get the height for said default width.
 *   %GTK_SIZE_REQUEST_CONSTANT_SIZE disables any height-for-width or
 *   width-for-height geometry management for said widget and is the
 *   default return.
 *   It’s important to note that any widget
 *   which trades height-for-width or width-for-height must respond properly
 *   to a for_size value >= -1 passed to #GtkWidgetClass.measure, for both
 *   possible orientations.
 * @measure: This is called by containers to obtain the minimum and natural
 *   size of the widget. Depending on the orientation parameter, the passed
 *   for_size can be interpreted as width or height. A widget will never be
 *   allocated less than its minimum size.
 * @mnemonic_activate: Activates the @widget if @group_cycling is
 *   %FALSE, and just grabs the focus if @group_cycling is %TRUE.
 * @grab_focus: Causes @widget to have the keyboard focus for the
 *   #GtkWindow it’s inside.
 * @focus:
 * @move_focus: Signal emitted when a change of focus is requested
 * @keynav_failed: Signal emitted if keyboard navigation fails.
 * @drag_begin: Signal emitted on the drag source when a drag is
 *   started.
 * @drag_end: Signal emitted on the drag source when a drag is
 *   finished.
 * @drag_data_get: Signal emitted on the drag source when the drop
 *   site requests the data which is dragged.
 * @drag_data_delete: Signal emitted on the drag source when a drag
 *   with the action %GDK_ACTION_MOVE is successfully completed.
 * @drag_leave: Signal emitted on the drop site when the cursor leaves
 *   the widget.
 * @drag_motion: signal emitted on the drop site when the user moves
 *   the cursor over the widget during a drag.
 * @drag_drop: Signal emitted on the drop site when the user drops the
 *   data onto the widget.
 * @drag_data_received: Signal emitted on the drop site when the
 *   dragged data has been received.
 * @drag_failed: Signal emitted on the drag source when a drag has
 *   failed.
 * @popup_menu: Signal emitted whenever a widget should pop up a
 *   context menu.
 * @get_accessible: Returns the accessible object that describes the
 *   widget to an assistive technology.
 * @can_activate_accel: Signal allows applications and derived widgets
 *   to override the default GtkWidget handling for determining whether
 *   an accelerator can be activated.
 * @query_tooltip: Signal emitted when “has-tooltip” is %TRUE and the
 *   hover timeout has expired with the cursor hovering “above”
 *   widget; or emitted when widget got focus in keyboard mode.
 * @compute_expand: Computes whether a container should give this
 *   widget extra space when possible.
 * @style_updated: Signal emitted when the GtkStyleContext of a widget
 *   is changed.
 * @snapshot: Vfunc for gtk_widget_snapshot().
 * @contains: Vfunc for gtk_widget_contains().
 */
struct _GtkWidgetClass
{
  GInitiallyUnownedClass parent_class;

  /*< public >*/

  guint activate_signal;

  /* basics */
  void (* destroy)             (GtkWidget        *widget);
  void (* show)                (GtkWidget        *widget);
  void (* hide)                (GtkWidget        *widget);
  void (* map)                 (GtkWidget        *widget);
  void (* unmap)               (GtkWidget        *widget);
  void (* realize)             (GtkWidget        *widget);
  void (* unrealize)           (GtkWidget        *widget);
  void (* root)                (GtkWidget        *widget);
  void (* unroot)              (GtkWidget        *widget);
  void (* size_allocate)       (GtkWidget           *widget,
                                int                  width,
                                int                  height,
                                int                  baseline);
  void (* state_flags_changed) (GtkWidget        *widget,
                                GtkStateFlags     previous_state_flags);
  void (* direction_changed)   (GtkWidget        *widget,
                                GtkTextDirection  previous_direction);
  void (* grab_notify)         (GtkWidget        *widget,
                                gboolean          was_grabbed);

  /* size requests */
  GtkSizeRequestMode (* get_request_mode)               (GtkWidget      *widget);
  void              (* measure) (GtkWidget      *widget,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural,
                                 int            *minimum_baseline,
                                 int            *natural_baseline);

  /* Mnemonics */
  gboolean (* mnemonic_activate)        (GtkWidget           *widget,
                                         gboolean             group_cycling);

  /* explicit focus */
  gboolean (* grab_focus)               (GtkWidget           *widget);
  gboolean (* focus)                    (GtkWidget           *widget,
                                         GtkDirectionType     direction);

  /* keyboard navigation */
  void     (* move_focus)               (GtkWidget           *widget,
                                         GtkDirectionType     direction);
  gboolean (* keynav_failed)            (GtkWidget           *widget,
                                         GtkDirectionType     direction);

  /* Source side drag signals */
  void     (* drag_begin)          (GtkWidget          *widget,
                                    GdkDrag            *drag);
  void     (* drag_end)            (GtkWidget          *widget,
                                    GdkDrag            *drag);
  void     (* drag_data_get)       (GtkWidget          *widget,
                                    GdkDrag            *drag,
                                    GtkSelectionData   *selection_data);
  void     (* drag_data_delete)    (GtkWidget          *widget,
                                    GdkDrag            *drag);

  /* Target side drag signals */
  void     (* drag_leave)          (GtkWidget          *widget,
                                    GdkDrop            *drop);
  gboolean (* drag_motion)         (GtkWidget          *widget,
                                    GdkDrop            *drop,
                                    gint                x,
                                    gint                y);
  gboolean (* drag_drop)           (GtkWidget          *widget,
                                    GdkDrop            *drop,
                                    gint                x,
                                    gint                y);
  void     (* drag_data_received)  (GtkWidget          *widget,
                                    GdkDrop            *drop,
                                    GtkSelectionData   *selection_data);
  gboolean (* drag_failed)         (GtkWidget          *widget,
                                    GdkDrag            *drag,
                                    GtkDragResult       result);

  /* Signals used only for keybindings */
  gboolean (* popup_menu)          (GtkWidget          *widget);

  /* accessibility support
   */
  AtkObject *  (* get_accessible)     (GtkWidget       *widget);

  gboolean     (* can_activate_accel) (GtkWidget *widget,
                                       guint      signal_id);


  gboolean     (* query_tooltip)      (GtkWidget  *widget,
                                       gint        x,
                                       gint        y,
                                       gboolean    keyboard_tooltip,
                                       GtkTooltip *tooltip);

  void         (* compute_expand)     (GtkWidget  *widget,
                                       gboolean   *hexpand_p,
                                       gboolean   *vexpand_p);

  void         (* style_updated)          (GtkWidget *widget);

  void         (* snapshot)                    (GtkWidget            *widget,
                                                GtkSnapshot          *snapshot);

  gboolean     (* contains)                    (GtkWidget *widget,
                                                gdouble    x,
                                                gdouble    y);

  /*< private >*/

  GtkWidgetClassPrivate *priv;

  gpointer padding[8];
};


GDK_AVAILABLE_IN_ALL
GType      gtk_widget_get_type            (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_widget_new                 (GType                type,
                                           const gchar         *first_property_name,
                                           ...);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_destroy             (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_destroyed           (GtkWidget           *widget,
                                           GtkWidget          **widget_pointer);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_unparent            (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_show                (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_hide                (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_map                 (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_unmap               (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_realize             (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_unrealize           (GtkWidget           *widget);

/* Queuing draws */
GDK_AVAILABLE_IN_ALL
void       gtk_widget_queue_draw          (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_queue_resize        (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_queue_allocate      (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
GdkFrameClock* gtk_widget_get_frame_clock (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
void       gtk_widget_size_allocate       (GtkWidget           *widget,
                                           const GtkAllocation *allocation,
                                           int                  baseline);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_allocate            (GtkWidget               *widget,
                                           int                      width,
                                           int                      height,
                                           int                      baseline,
                                           GskTransform            *transform);

GDK_AVAILABLE_IN_ALL
GtkSizeRequestMode  gtk_widget_get_request_mode               (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void gtk_widget_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline);
GDK_AVAILABLE_IN_ALL
void                gtk_widget_get_preferred_size             (GtkWidget      *widget,
                                                               GtkRequisition *minimum_size,
                                                               GtkRequisition *natural_size);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_set_layout_manager   (GtkWidget        *widget,
                                                         GtkLayoutManager *layout_manager);
GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_widget_get_layout_manager   (GtkWidget        *widget);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_class_set_layout_manager_type        (GtkWidgetClass *widget_class,
                                                                         GType           type);
GDK_AVAILABLE_IN_ALL
GType                   gtk_widget_class_get_layout_manager_type        (GtkWidgetClass *widget_class);

GDK_AVAILABLE_IN_ALL
void       gtk_widget_add_accelerator     (GtkWidget           *widget,
                                           const gchar         *accel_signal,
                                           GtkAccelGroup       *accel_group,
                                           guint                accel_key,
                                           GdkModifierType      accel_mods,
                                           GtkAccelFlags        accel_flags);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_remove_accelerator  (GtkWidget           *widget,
                                           GtkAccelGroup       *accel_group,
                                           guint                accel_key,
                                           GdkModifierType      accel_mods);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_accel_path      (GtkWidget           *widget,
                                           const gchar         *accel_path,
                                           GtkAccelGroup       *accel_group);
GDK_AVAILABLE_IN_ALL
GList*     gtk_widget_list_accel_closures (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_can_activate_accel  (GtkWidget           *widget,
                                           guint                signal_id);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_mnemonic_activate   (GtkWidget           *widget,
                                           gboolean             group_cycling);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_event               (GtkWidget           *widget,
                                           GdkEvent            *event);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_activate               (GtkWidget        *widget);

GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_can_focus       (GtkWidget           *widget,
                                           gboolean             can_focus);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_get_can_focus       (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_has_focus           (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_is_focus            (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_has_visible_focus   (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_grab_focus          (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_focus_on_click  (GtkWidget           *widget,
                                           gboolean             focus_on_click);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_get_focus_on_click  (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_can_target      (GtkWidget           *widget,
                                           gboolean             can_target);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_get_can_target      (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_has_default         (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
void      gtk_widget_set_receives_default (GtkWidget           *widget,
                                           gboolean             receives_default);
GDK_AVAILABLE_IN_ALL
gboolean  gtk_widget_get_receives_default (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_has_grab            (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_device_is_shadowed  (GtkWidget           *widget,
                                           GdkDevice           *device);


GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_name               (GtkWidget    *widget,
                                                         const gchar  *name);
GDK_AVAILABLE_IN_ALL
const gchar *         gtk_widget_get_name               (GtkWidget    *widget);


GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_state_flags        (GtkWidget     *widget,
                                                         GtkStateFlags  flags,
                                                         gboolean       clear);
GDK_AVAILABLE_IN_ALL
void                  gtk_widget_unset_state_flags      (GtkWidget     *widget,
                                                         GtkStateFlags  flags);
GDK_AVAILABLE_IN_ALL
GtkStateFlags         gtk_widget_get_state_flags        (GtkWidget     *widget);

GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_sensitive          (GtkWidget    *widget,
                                                         gboolean      sensitive);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_get_sensitive          (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_is_sensitive           (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_visible            (GtkWidget    *widget,
                                                         gboolean      visible);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_get_visible            (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_is_visible             (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_is_drawable            (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_get_realized           (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_get_mapped             (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_parent             (GtkWidget    *widget,
                                                         GtkWidget    *parent);
GDK_AVAILABLE_IN_ALL
GtkWidget *           gtk_widget_get_parent             (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
GtkRoot *             gtk_widget_get_root               (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
GtkNative *           gtk_widget_get_native             (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_child_visible      (GtkWidget    *widget,
                                                         gboolean      child_visible);
GDK_AVAILABLE_IN_ALL
gboolean              gtk_widget_get_child_visible      (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
int                   gtk_widget_get_allocated_width    (GtkWidget     *widget);
GDK_AVAILABLE_IN_ALL
int                   gtk_widget_get_allocated_height   (GtkWidget     *widget);
GDK_AVAILABLE_IN_ALL
int                   gtk_widget_get_allocated_baseline (GtkWidget     *widget);

GDK_AVAILABLE_IN_ALL
void                  gtk_widget_get_allocation         (GtkWidget     *widget,
                                                         GtkAllocation *allocation);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_widget_compute_transform            (GtkWidget              *widget,
                                                                 GtkWidget              *target,
                                                                 graphene_matrix_t      *out_transform) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_ALL
gboolean                gtk_widget_compute_bounds               (GtkWidget              *widget,
                                                                 GtkWidget              *target,
                                                                 graphene_rect_t        *out_bounds) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_ALL
gboolean                gtk_widget_compute_point                (GtkWidget              *widget,
                                                                 GtkWidget              *target,
                                                                 const graphene_point_t *point,
                                                                 graphene_point_t       *out_point) G_GNUC_WARN_UNUSED_RESULT;

GDK_AVAILABLE_IN_ALL
int                   gtk_widget_get_width              (GtkWidget     *widget);
GDK_AVAILABLE_IN_ALL
int                   gtk_widget_get_height             (GtkWidget     *widget);

GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_child_focus         (GtkWidget           *widget,
                                           GtkDirectionType     direction);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_keynav_failed       (GtkWidget           *widget,
                                           GtkDirectionType     direction);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_error_bell          (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_size_request    (GtkWidget           *widget,
                                           gint                 width,
                                           gint                 height);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_get_size_request    (GtkWidget           *widget,
                                           gint                *width,
                                           gint                *height);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_opacity         (GtkWidget           *widget,
                                           double               opacity);
GDK_AVAILABLE_IN_ALL
double     gtk_widget_get_opacity         (GtkWidget           *widget);
GDK_AVAILABLE_IN_ALL
void         gtk_widget_set_overflow      (GtkWidget           *widget,
                                           GtkOverflow          overflow);
GDK_AVAILABLE_IN_ALL
GtkOverflow  gtk_widget_get_overflow      (GtkWidget           *widget);

GDK_AVAILABLE_IN_ALL
GtkWidget*   gtk_widget_get_ancestor    (GtkWidget      *widget,
                                         GType           widget_type);

GDK_AVAILABLE_IN_ALL
gint          gtk_widget_get_scale_factor (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GdkDisplay *  gtk_widget_get_display     (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GtkSettings*  gtk_widget_get_settings    (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GdkClipboard *gtk_widget_get_clipboard   (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GdkClipboard *gtk_widget_get_primary_clipboard (GtkWidget *widget);


/* Expand flags and related support */
GDK_AVAILABLE_IN_ALL
gboolean gtk_widget_get_hexpand          (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_hexpand          (GtkWidget      *widget,
                                          gboolean        expand);
GDK_AVAILABLE_IN_ALL
gboolean gtk_widget_get_hexpand_set      (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_hexpand_set      (GtkWidget      *widget,
                                          gboolean        set);
GDK_AVAILABLE_IN_ALL
gboolean gtk_widget_get_vexpand          (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_vexpand          (GtkWidget      *widget,
                                          gboolean        expand);
GDK_AVAILABLE_IN_ALL
gboolean gtk_widget_get_vexpand_set      (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_vexpand_set      (GtkWidget      *widget,
                                          gboolean        set);
GDK_AVAILABLE_IN_ALL
gboolean gtk_widget_compute_expand       (GtkWidget      *widget,
                                          GtkOrientation  orientation);


/* Multidevice support */
GDK_AVAILABLE_IN_ALL
gboolean         gtk_widget_get_support_multidevice (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void             gtk_widget_set_support_multidevice (GtkWidget      *widget,
                                                     gboolean        support_multidevice);

/* Accessibility support */
GDK_AVAILABLE_IN_ALL
void             gtk_widget_class_set_accessible_type    (GtkWidgetClass     *widget_class,
                                                          GType               type);
GDK_AVAILABLE_IN_ALL
void             gtk_widget_class_set_accessible_role    (GtkWidgetClass     *widget_class,
                                                          AtkRole             role);
GDK_AVAILABLE_IN_ALL
AtkObject*       gtk_widget_get_accessible               (GtkWidget          *widget);


/* Margin and alignment */
GDK_AVAILABLE_IN_ALL
GtkAlign gtk_widget_get_halign        (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_halign        (GtkWidget *widget,
                                       GtkAlign   align);
GDK_AVAILABLE_IN_ALL
GtkAlign gtk_widget_get_valign        (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_valign        (GtkWidget *widget,
                                       GtkAlign   align);
GDK_AVAILABLE_IN_ALL
gint     gtk_widget_get_margin_start  (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_margin_start  (GtkWidget *widget,
                                       gint       margin);
GDK_AVAILABLE_IN_ALL
gint     gtk_widget_get_margin_end    (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_margin_end    (GtkWidget *widget,
                                       gint       margin);
GDK_AVAILABLE_IN_ALL
gint     gtk_widget_get_margin_top    (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_margin_top    (GtkWidget *widget,
                                       gint       margin);
GDK_AVAILABLE_IN_ALL
gint     gtk_widget_get_margin_bottom (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void     gtk_widget_set_margin_bottom (GtkWidget *widget,
                                       gint       margin);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_widget_is_ancestor     (GtkWidget      *widget,
                                         GtkWidget      *ancestor);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_widget_translate_coordinates (GtkWidget  *src_widget,
                                               GtkWidget  *dest_widget,
                                               gint        src_x,
                                               gint        src_y,
                                               gint       *dest_x,
                                               gint       *dest_y);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_widget_contains              (GtkWidget  *widget,
                                               gdouble     x,
                                               gdouble     y);
GDK_AVAILABLE_IN_ALL
GtkWidget *  gtk_widget_pick                  (GtkWidget   *widget,
                                               gdouble      x,
                                               gdouble      y,
                                               GtkPickFlags flags);

GDK_AVAILABLE_IN_ALL
void         gtk_widget_add_controller        (GtkWidget          *widget,
                                               GtkEventController *controller);
GDK_AVAILABLE_IN_ALL
void         gtk_widget_remove_controller     (GtkWidget          *widget,
                                               GtkEventController *controller);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_reset_style       (GtkWidget      *widget);

GDK_AVAILABLE_IN_ALL
PangoContext *gtk_widget_create_pango_context (GtkWidget   *widget);
GDK_AVAILABLE_IN_ALL
PangoContext *gtk_widget_get_pango_context    (GtkWidget   *widget);
GDK_AVAILABLE_IN_ALL
void gtk_widget_set_font_options (GtkWidget                  *widget,
                                  const cairo_font_options_t *options);
GDK_AVAILABLE_IN_ALL
const cairo_font_options_t *gtk_widget_get_font_options (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
PangoLayout  *gtk_widget_create_pango_layout  (GtkWidget   *widget,
                                               const gchar *text);

/* Functions for setting directionality for widgets */

GDK_AVAILABLE_IN_ALL
void             gtk_widget_set_direction         (GtkWidget        *widget,
                                                   GtkTextDirection  dir);
GDK_AVAILABLE_IN_ALL
GtkTextDirection gtk_widget_get_direction         (GtkWidget        *widget);

GDK_AVAILABLE_IN_ALL
void             gtk_widget_set_default_direction (GtkTextDirection  dir);
GDK_AVAILABLE_IN_ALL
GtkTextDirection gtk_widget_get_default_direction (void);

/* Counterpart to gdk_surface_shape_combine_region.
 */
GDK_AVAILABLE_IN_ALL
void         gtk_widget_input_shape_combine_region (GtkWidget *widget,
                                                    cairo_region_t *region);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_set_cursor                   (GtkWidget              *widget,
                                                                 GdkCursor              *cursor);
GDK_AVAILABLE_IN_ALL
void                    gtk_widget_set_cursor_from_name         (GtkWidget              *widget,
                                                                 const char             *name);
GDK_AVAILABLE_IN_ALL
GdkCursor *             gtk_widget_get_cursor                   (GtkWidget              *widget);

GDK_AVAILABLE_IN_ALL
GList* gtk_widget_list_mnemonic_labels  (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void   gtk_widget_add_mnemonic_label    (GtkWidget *widget,
                                         GtkWidget *label);
GDK_AVAILABLE_IN_ALL
void   gtk_widget_remove_mnemonic_label (GtkWidget *widget,
                                         GtkWidget *label);

GDK_AVAILABLE_IN_ALL
void                  gtk_widget_set_tooltip_window    (GtkWidget   *widget,
                                                        GtkWindow   *custom_window);
GDK_AVAILABLE_IN_ALL
GtkWindow *gtk_widget_get_tooltip_window    (GtkWidget   *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_trigger_tooltip_query (GtkWidget   *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_tooltip_text      (GtkWidget   *widget,
                                             const gchar *text);
GDK_AVAILABLE_IN_ALL
gchar *    gtk_widget_get_tooltip_text      (GtkWidget   *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_tooltip_markup    (GtkWidget   *widget,
                                             const gchar *markup);
GDK_AVAILABLE_IN_ALL
gchar *    gtk_widget_get_tooltip_markup    (GtkWidget   *widget);
GDK_AVAILABLE_IN_ALL
void       gtk_widget_set_has_tooltip       (GtkWidget   *widget,
                                             gboolean     has_tooltip);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_widget_get_has_tooltip       (GtkWidget   *widget);

GDK_AVAILABLE_IN_ALL
GType           gtk_requisition_get_type (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkRequisition *gtk_requisition_new      (void) G_GNUC_MALLOC;
GDK_AVAILABLE_IN_ALL
GtkRequisition *gtk_requisition_copy     (const GtkRequisition *requisition);
GDK_AVAILABLE_IN_ALL
void            gtk_requisition_free     (GtkRequisition       *requisition);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_widget_in_destruction (GtkWidget *widget);

GDK_AVAILABLE_IN_ALL
GtkStyleContext * gtk_widget_get_style_context (GtkWidget *widget);

GDK_AVAILABLE_IN_ALL
GtkWidgetPath *   gtk_widget_get_path (GtkWidget *widget);

GDK_AVAILABLE_IN_ALL
void              gtk_widget_class_set_css_name (GtkWidgetClass *widget_class,
                                                 const char     *name);
GDK_AVAILABLE_IN_ALL
const char *      gtk_widget_class_get_css_name (GtkWidgetClass *widget_class);

GDK_AVAILABLE_IN_ALL
GdkModifierType   gtk_widget_get_modifier_mask (GtkWidget         *widget,
                                                GdkModifierIntent  intent);

GDK_AVAILABLE_IN_ALL
guint gtk_widget_add_tick_callback (GtkWidget       *widget,
                                    GtkTickCallback  callback,
                                    gpointer         user_data,
                                    GDestroyNotify   notify);

GDK_AVAILABLE_IN_ALL
void gtk_widget_remove_tick_callback (GtkWidget       *widget,
                                      guint            id);

/**
 * gtk_widget_class_bind_template_callback:
 * @widget_class: a #GtkWidgetClass
 * @callback: the callback symbol
 *
 * Binds a callback function defined in a template to the @widget_class.
 *
 * This macro is a convenience wrapper around the
 * gtk_widget_class_bind_template_callback_full() function.
 */
#define gtk_widget_class_bind_template_callback(widget_class, callback) \
  gtk_widget_class_bind_template_callback_full (GTK_WIDGET_CLASS (widget_class), \
                                                #callback, \
                                                G_CALLBACK (callback))

/**
 * gtk_widget_class_bind_template_child:
 * @widget_class: a #GtkWidgetClass
 * @TypeName: the type name of this widget
 * @member_name: name of the instance member in the instance struct for @data_type
 *
 * Binds a child widget defined in a template to the @widget_class.
 *
 * This macro is a convenience wrapper around the
 * gtk_widget_class_bind_template_child_full() function.
 *
 * This macro will use the offset of the @member_name inside the @TypeName
 * instance structure.
 */
#define gtk_widget_class_bind_template_child(widget_class, TypeName, member_name) \
  gtk_widget_class_bind_template_child_full (widget_class, \
                                             #member_name, \
                                             FALSE, \
                                             G_STRUCT_OFFSET (TypeName, member_name))

/**
 * gtk_widget_class_bind_template_child_internal:
 * @widget_class: a #GtkWidgetClass
 * @TypeName: the type name, in CamelCase
 * @member_name: name of the instance member in the instance struct for @data_type
 *
 * Binds a child widget defined in a template to the @widget_class, and
 * also makes it available as an internal child in GtkBuilder, under the
 * name @member_name.
 *
 * This macro is a convenience wrapper around the
 * gtk_widget_class_bind_template_child_full() function.
 *
 * This macro will use the offset of the @member_name inside the @TypeName
 * instance structure.
 */
#define gtk_widget_class_bind_template_child_internal(widget_class, TypeName, member_name) \
  gtk_widget_class_bind_template_child_full (widget_class, \
                                             #member_name, \
                                             TRUE, \
                                             G_STRUCT_OFFSET (TypeName, member_name))

/**
 * gtk_widget_class_bind_template_child_private:
 * @widget_class: a #GtkWidgetClass
 * @TypeName: the type name of this widget
 * @member_name: name of the instance private member in the private struct for @data_type
 *
 * Binds a child widget defined in a template to the @widget_class.
 *
 * This macro is a convenience wrapper around the
 * gtk_widget_class_bind_template_child_full() function.
 *
 * This macro will use the offset of the @member_name inside the @TypeName
 * private data structure (it uses G_PRIVATE_OFFSET(), so the private struct
 * must be added with G_ADD_PRIVATE()).
 */
#define gtk_widget_class_bind_template_child_private(widget_class, TypeName, member_name) \
  gtk_widget_class_bind_template_child_full (widget_class, \
                                             #member_name, \
                                             FALSE, \
                                             G_PRIVATE_OFFSET (TypeName, member_name))

/**
 * gtk_widget_class_bind_template_child_internal_private:
 * @widget_class: a #GtkWidgetClass
 * @TypeName: the type name, in CamelCase
 * @member_name: name of the instance private member on the private struct for @data_type
 *
 * Binds a child widget defined in a template to the @widget_class, and
 * also makes it available as an internal child in GtkBuilder, under the
 * name @member_name.
 *
 * This macro is a convenience wrapper around the
 * gtk_widget_class_bind_template_child_full() function.
 *
 * This macro will use the offset of the @member_name inside the @TypeName
 * private data structure.
 */
#define gtk_widget_class_bind_template_child_internal_private(widget_class, TypeName, member_name) \
  gtk_widget_class_bind_template_child_full (widget_class, \
                                             #member_name, \
                                             TRUE, \
                                             G_PRIVATE_OFFSET (TypeName, member_name))

GDK_AVAILABLE_IN_ALL
void    gtk_widget_init_template                        (GtkWidget             *widget);
GDK_AVAILABLE_IN_ALL
GObject *gtk_widget_get_template_child                  (GtkWidget             *widget,
                                                         GType                  widget_type,
                                                         const gchar           *name);
GDK_AVAILABLE_IN_ALL
void    gtk_widget_class_set_template                   (GtkWidgetClass        *widget_class,
                                                         GBytes                *template_bytes);
GDK_AVAILABLE_IN_ALL
void    gtk_widget_class_set_template_from_resource     (GtkWidgetClass        *widget_class,
                                                         const gchar           *resource_name);
GDK_AVAILABLE_IN_ALL
void    gtk_widget_class_bind_template_callback_full    (GtkWidgetClass        *widget_class,
                                                         const gchar           *callback_name,
                                                         GCallback              callback_symbol);
GDK_AVAILABLE_IN_ALL
void    gtk_widget_class_set_connect_func               (GtkWidgetClass        *widget_class,
                                                         GtkBuilderConnectFunc  connect_func,
                                                         gpointer               connect_data,
                                                         GDestroyNotify         connect_data_destroy);
GDK_AVAILABLE_IN_ALL
void    gtk_widget_class_bind_template_child_full       (GtkWidgetClass        *widget_class,
                                                         const gchar           *name,
                                                         gboolean               internal_child,
                                                         gssize                 struct_offset);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_insert_action_group  (GtkWidget    *widget,
                                                         const gchar  *name,
                                                         GActionGroup *group);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_widget_activate_action      (GtkWidget  *widget,
                                                         const char *name,
                                                         const char *format_string,
                                                         ...);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_widget_activate_action_variant (GtkWidget  *widget,
                                                            const char *name,
                                                            GVariant   *args);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_activate_default     (GtkWidget *widget);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_set_font_map         (GtkWidget             *widget,
                                                         PangoFontMap          *font_map);
GDK_AVAILABLE_IN_ALL
PangoFontMap *          gtk_widget_get_font_map         (GtkWidget             *widget);

GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_widget_get_first_child      (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_widget_get_last_child       (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_widget_get_next_sibling     (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_widget_get_prev_sibling     (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GListModel *            gtk_widget_observe_children     (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
GListModel *            gtk_widget_observe_controllers  (GtkWidget *widget);
GDK_AVAILABLE_IN_ALL
void                    gtk_widget_insert_after         (GtkWidget *widget,
                                                         GtkWidget *parent,
                                                         GtkWidget *previous_sibling);
GDK_AVAILABLE_IN_ALL
void                    gtk_widget_insert_before        (GtkWidget *widget,
                                                         GtkWidget *parent,
                                                         GtkWidget *next_sibling);
GDK_AVAILABLE_IN_ALL
void                    gtk_widget_set_focus_child      (GtkWidget *widget,
                                                         GtkWidget *child);
GDK_AVAILABLE_IN_ALL
GtkWidget *             gtk_widget_get_focus_child      (GtkWidget *widget);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_snapshot_child       (GtkWidget   *widget,
                                                         GtkWidget   *child,
                                                         GtkSnapshot *snapshot);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_widget_should_layout        (GtkWidget   *widget);


/**
 * GtkWidgetActionActivateFunc:
 * @widget: the widget to which the action belongs
 * @action_name: the action name
 * @parameter: parameter for activation
 *
 * The type of the callback functions used for activating
 * actions installed with gtk_widget_class_install_action().
 *
 * The @parameter must match the @parameter_type of the action.
 */
typedef void (* GtkWidgetActionActivateFunc) (GtkWidget  *widget,
                                              const char *action_name,
                                              GVariant   *parameter);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_class_install_action (GtkWidgetClass              *widget_class,
                                                         const char                  *action_name,
                                                         const char                  *parameter_type,
                                                         GtkWidgetActionActivateFunc  activate);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_class_install_property_action (GtkWidgetClass *widget_class,
                                                                  const char     *action_name,
                                                                  const char     *property_name);

GDK_AVAILABLE_IN_ALL
gboolean               gtk_widget_class_query_action  (GtkWidgetClass      *widget_class,
                                                       guint                index_,
                                                       GType               *owner,
                                                       const char         **action_name,
                                                       const GVariantType **parameter_type,
                                                       const char         **property_name);

GDK_AVAILABLE_IN_ALL
void                    gtk_widget_action_set_enabled (GtkWidget  *widget,
                                                       const char *action_name,
                                                       gboolean    enabled);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkWidget, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkRequisition, gtk_requisition_free)

G_END_DECLS

#endif /* __GTK_WIDGET_H__ */
