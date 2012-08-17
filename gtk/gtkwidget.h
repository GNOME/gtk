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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_WIDGET_H__
#define __GTK_WIDGET_H__

#include <gdk/gdk.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkborder.h>
#include <gtk/gtktypes.h>
#include <atk/atk.h>

G_BEGIN_DECLS

/* Kinds of widget-specific help */
typedef enum
{
  GTK_WIDGET_HELP_TOOLTIP,
  GTK_WIDGET_HELP_WHATS_THIS
} GtkWidgetHelpType;

/* Macro for casting a pointer to a GtkWidget or GtkWidgetClass pointer.
 * Macros for testing whether `widget' or `klass' are of type GTK_TYPE_WIDGET.
 */
#define GTK_TYPE_WIDGET			  (gtk_widget_get_type ())
#define GTK_WIDGET(widget)		  (G_TYPE_CHECK_INSTANCE_CAST ((widget), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_WIDGET_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WIDGET, GtkWidgetClass))
#define GTK_IS_WIDGET(widget)		  (G_TYPE_CHECK_INSTANCE_TYPE ((widget), GTK_TYPE_WIDGET))
#define GTK_IS_WIDGET_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WIDGET))
#define GTK_WIDGET_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WIDGET, GtkWidgetClass))

#define GTK_TYPE_REQUISITION              (gtk_requisition_get_type ())

typedef struct _GtkWidgetPrivate       GtkWidgetPrivate;
typedef struct _GtkWidgetClass	       GtkWidgetClass;
typedef struct _GtkWidgetClassPrivate  GtkWidgetClassPrivate;
typedef struct _GtkWidgetAuxInfo       GtkWidgetAuxInfo;

/**
 * GtkAllocation:
 * @x: the X position of the widget's area relative to its parents allocation.
 * @y: the Y position of the widget's area relative to its parents allocation.
 * @width: the width of the widget's allocated area.
 * @height: the height of the widget's allocated area.
 *
 * A <structname>GtkAllocation</structname> of a widget represents region
 * which has been allocated to the widget by its parent. It is a subregion
 * of its parents allocation. See <xref linkend="geometry-management"/> for
 * more information.
 */
typedef 	GdkRectangle	   GtkAllocation;

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
 * GtkRequisition:
 * @width: the widget's desired width
 * @height: the widget's desired height
 *
 * A <structname>GtkRequisition</structname> represents the desired size of a widget. See
 * <xref linkend="geometry-management"/> for more information.
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
 * @get_request_mode: This allows a widget to tell its parent container whether
 *   it prefers to be allocated in %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH or
 *   %GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT mode.
 *   %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH means the widget prefers to have
 *   #GtkWidgetClass.get_preferred_width() called and then
 *   #GtkWidgetClass.get_preferred_height_for_width().
 *   %GTK_SIZE_REQUEST_CONSTANT_SIZE disables any height-for-width or
 *   width-for-height geometry management for a said widget and is the
 *   default return.
 *   It's important to note (as described below) that any widget
 *   which trades height-for-width or width-for-height must respond properly 
 *   to both of the virtual methods #GtkWidgetClass.get_preferred_height_for_width()
 *   and #GtkWidgetClass.get_preferred_width_for_height() since it might be 
 *   queried in either #GtkSizeRequestMode by its parent container.
 * @get_preferred_height: This is called by containers to obtain the minimum
 *   and natural height of a widget. A widget that does not actually trade
 *   any height for width or width for height only has to implement these
 *   two virtual methods (#GtkWidgetClass.get_preferred_width() and
 *   #GtkWidgetClass.get_preferred_height()).
 * @get_preferred_width_for_height: This is analogous to
 *   #GtkWidgetClass.get_preferred_height_for_width() except that it
 *   operates in the oposite orientation. It's rare that a widget actually
 *   does %GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT requests but this can happen
 *   when, for example, a widget or container gets additional columns to
 *   compensate for a smaller allocated height.
 * @get_preferred_width: This is called by containers to obtain the minimum
 *   and natural width of a widget. A widget will never be allocated a width
 *   less than its minimum and will only ever be allocated a width greater
 *   than the natural width once all of the said widget's siblings have
 *   received their natural widths.
 *   Furthermore, a widget will only ever be allocated a width greater than
 *   its natural width if it was configured to receive extra expand space
 *   from its parent container.
 * @get_preferred_height_for_width: This is similar to
 *   #GtkWidgetClass.get_preferred_height() except that it is passed a
 *   contextual width to request height for. By implementing this virtual
 *   method it is possible for a #GtkLabel to tell its parent how much height
 *   would be required if the label were to be allocated a said width.
 * @adjust_size_request: Convert an initial size request from a widget's
 *   #GtkSizeRequestMode virtual method implementations into a size request to
 *   be used by parent containers in laying out the widget.
 *   adjust_size_request adjusts <emphasis>from</emphasis> a child widget's
 *   original request <emphasis>to</emphasis> what a parent container should
 *   use for layout. The @for_size argument will be -1 if the request should
 *   not be for a particular size in the opposing orientation, i.e. if the
 *   request is not height-for-width or width-for-height. If @for_size is
 *   greater than -1, it is the proposed allocation in the opposing
 *   orientation that we need the request for. Implementations of
 *   adjust_size_request should chain up to the default implementation,
 *   which applies #GtkWidget's margin properties and imposes any values
 *   from gtk_widget_set_size_request(). Chaining up should be last,
 *   <emphasis>after</emphasis> your subclass adjusts the request, so
 *   #GtkWidget can apply constraints and add the margin properly.
 * @adjust_size_allocation: Convert an initial size allocation assigned
 *   by a #GtkContainer using gtk_widget_size_allocate(), into an actual
 *   size allocation to be used by the widget. adjust_size_allocation
 *   adjusts <emphasis>to</emphasis> a child widget's actual allocation
 *   <emphasis>from</emphasis> what a parent container computed for the
 *   child. The adjusted allocation must be entirely within the original
 *   allocation. In any custom implementation, chain up to the default
 *   #GtkWidget implementation of this method, which applies the margin
 *   and alignment properties of #GtkWidget. Chain up
 *   <emphasis>before</emphasis> performing your own adjustments so your
 *   own adjustments remove more allocation after the #GtkWidget base
 *   class has already removed margin and alignment. The natural size
 *   passed in should be adjusted in the same way as the allocated size,
 *   which allows adjustments to perform alignments or other changes
 *   based on natural size.
 */
struct _GtkWidgetClass
{
  GInitiallyUnownedClass parent_class;

  /*< public >*/

  guint activate_signal;

  /* seldomly overidden */
  void (*dispatch_child_properties_changed) (GtkWidget   *widget,
					     guint        n_pspecs,
					     GParamSpec **pspecs);

  /* basics */
  void (* destroy)             (GtkWidget        *widget);
  void (* show)		       (GtkWidget        *widget);
  void (* show_all)            (GtkWidget        *widget);
  void (* hide)		       (GtkWidget        *widget);
  void (* map)		       (GtkWidget        *widget);
  void (* unmap)	       (GtkWidget        *widget);
  void (* realize)	       (GtkWidget        *widget);
  void (* unrealize)	       (GtkWidget        *widget);
  void (* size_allocate)       (GtkWidget        *widget,
				GtkAllocation    *allocation);
  void (* state_changed)       (GtkWidget        *widget,
				GtkStateType   	  previous_state);
  void (* state_flags_changed) (GtkWidget        *widget,
				GtkStateFlags  	  previous_state_flags);
  void (* parent_set)	       (GtkWidget        *widget,
				GtkWidget        *previous_parent);
  void (* hierarchy_changed)   (GtkWidget        *widget,
				GtkWidget        *previous_toplevel);
  void (* style_set)	       (GtkWidget        *widget,
				GtkStyle         *previous_style);
  void (* direction_changed)   (GtkWidget        *widget,
				GtkTextDirection  previous_direction);
  void (* grab_notify)         (GtkWidget        *widget,
				gboolean          was_grabbed);
  void (* child_notify)        (GtkWidget	 *widget,
				GParamSpec       *child_property);
  gboolean (* draw)	       (GtkWidget	 *widget,
                                cairo_t          *cr);

  /* size requests */
  GtkSizeRequestMode (* get_request_mode)               (GtkWidget      *widget);

  void               (* get_preferred_height)           (GtkWidget       *widget,
                                                         gint            *minimum_height,
                                                         gint            *natural_height);
  void               (* get_preferred_width_for_height) (GtkWidget       *widget,
                                                         gint             height,
                                                         gint            *minimum_width,
                                                         gint            *natural_width);
  void               (* get_preferred_width)            (GtkWidget       *widget,
                                                         gint            *minimum_width,
                                                         gint            *natural_width);
  void               (* get_preferred_height_for_width) (GtkWidget       *widget,
                                                         gint             width,
                                                         gint            *minimum_height,
                                                         gint            *natural_height);

  /* Mnemonics */
  gboolean (* mnemonic_activate)        (GtkWidget           *widget,
                                         gboolean             group_cycling);

  /* explicit focus */
  void     (* grab_focus)               (GtkWidget           *widget);
  gboolean (* focus)                    (GtkWidget           *widget,
                                         GtkDirectionType     direction);

  /* keyboard navigation */
  void     (* move_focus)               (GtkWidget           *widget,
                                         GtkDirectionType     direction);
  gboolean (* keynav_failed)            (GtkWidget           *widget,
                                         GtkDirectionType     direction);

  /* events */
  gboolean (* event)			(GtkWidget	     *widget,
					 GdkEvent	     *event);
  gboolean (* button_press_event)	(GtkWidget	     *widget,
					 GdkEventButton      *event);
  gboolean (* button_release_event)	(GtkWidget	     *widget,
					 GdkEventButton      *event);
  gboolean (* scroll_event)		(GtkWidget           *widget,
					 GdkEventScroll      *event);
  gboolean (* motion_notify_event)	(GtkWidget	     *widget,
					 GdkEventMotion      *event);
  gboolean (* delete_event)		(GtkWidget	     *widget,
					 GdkEventAny	     *event);
  gboolean (* destroy_event)		(GtkWidget	     *widget,
					 GdkEventAny	     *event);
  gboolean (* key_press_event)		(GtkWidget	     *widget,
					 GdkEventKey	     *event);
  gboolean (* key_release_event)	(GtkWidget	     *widget,
					 GdkEventKey	     *event);
  gboolean (* enter_notify_event)	(GtkWidget	     *widget,
					 GdkEventCrossing    *event);
  gboolean (* leave_notify_event)	(GtkWidget	     *widget,
					 GdkEventCrossing    *event);
  gboolean (* configure_event)		(GtkWidget	     *widget,
					 GdkEventConfigure   *event);
  gboolean (* focus_in_event)		(GtkWidget	     *widget,
					 GdkEventFocus       *event);
  gboolean (* focus_out_event)		(GtkWidget	     *widget,
					 GdkEventFocus       *event);
  gboolean (* map_event)		(GtkWidget	     *widget,
					 GdkEventAny	     *event);
  gboolean (* unmap_event)		(GtkWidget	     *widget,
					 GdkEventAny	     *event);
  gboolean (* property_notify_event)	(GtkWidget	     *widget,
					 GdkEventProperty    *event);
  gboolean (* selection_clear_event)	(GtkWidget	     *widget,
					 GdkEventSelection   *event);
  gboolean (* selection_request_event)	(GtkWidget	     *widget,
					 GdkEventSelection   *event);
  gboolean (* selection_notify_event)	(GtkWidget	     *widget,
					 GdkEventSelection   *event);
  gboolean (* proximity_in_event)	(GtkWidget	     *widget,
					 GdkEventProximity   *event);
  gboolean (* proximity_out_event)	(GtkWidget	     *widget,
					 GdkEventProximity   *event);
  gboolean (* visibility_notify_event)	(GtkWidget	     *widget,
					 GdkEventVisibility  *event);
  gboolean (* window_state_event)	(GtkWidget	     *widget,
					 GdkEventWindowState *event);
  gboolean (* damage_event)             (GtkWidget           *widget,
                                         GdkEventExpose      *event);
  gboolean (* grab_broken_event)        (GtkWidget           *widget,
                                         GdkEventGrabBroken  *event);

  /* selection */
  void     (* selection_get)       (GtkWidget          *widget,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint               time_);
  void     (* selection_received)  (GtkWidget          *widget,
				    GtkSelectionData   *selection_data,
				    guint               time_);

  /* Source side drag signals */
  void     (* drag_begin)          (GtkWidget         *widget,
				    GdkDragContext     *context);
  void     (* drag_end)	           (GtkWidget	       *widget,
				    GdkDragContext     *context);
  void     (* drag_data_get)       (GtkWidget          *widget,
				    GdkDragContext     *context,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint               time_);
  void     (* drag_data_delete)    (GtkWidget          *widget,
				    GdkDragContext     *context);

  /* Target side drag signals */
  void     (* drag_leave)          (GtkWidget          *widget,
				    GdkDragContext     *context,
				    guint               time_);
  gboolean (* drag_motion)         (GtkWidget	       *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    guint               time_);
  gboolean (* drag_drop)           (GtkWidget	       *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    guint               time_);
  void     (* drag_data_received)  (GtkWidget          *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint               time_);
  gboolean (* drag_failed)         (GtkWidget          *widget,
                                    GdkDragContext     *context,
                                    GtkDragResult       result);

  /* Signals used only for keybindings */
  gboolean (* popup_menu)          (GtkWidget          *widget);

  /* If a widget has multiple tooltips/whatsthis, it should show the
   * one for the current focus location, or if that doesn't make
   * sense, should cycle through them showing each tip alongside
   * whatever piece of the widget it applies to.
   */
  gboolean (* show_help)           (GtkWidget          *widget,
                                    GtkWidgetHelpType   help_type);

  /* accessibility support
   */
  AtkObject *  (* get_accessible)     (GtkWidget *widget);

  void         (* screen_changed)     (GtkWidget *widget,
                                       GdkScreen *previous_screen);
  gboolean     (* can_activate_accel) (GtkWidget *widget,
                                       guint      signal_id);


  void         (* composited_changed) (GtkWidget *widget);

  gboolean     (* query_tooltip)      (GtkWidget  *widget,
				       gint        x,
				       gint        y,
				       gboolean    keyboard_tooltip,
				       GtkTooltip *tooltip);

  void         (* compute_expand)     (GtkWidget  *widget,
                                       gboolean   *hexpand_p,
                                       gboolean   *vexpand_p);

  void         (* adjust_size_request)    (GtkWidget         *widget,
                                           GtkOrientation     orientation,
                                           gint              *minimum_size,
                                           gint              *natural_size);
  void         (* adjust_size_allocation) (GtkWidget         *widget,
                                           GtkOrientation     orientation,
                                           gint              *minimum_size,
                                           gint              *natural_size,
                                           gint              *allocated_pos,
                                           gint              *allocated_size);

  void         (* style_updated)          (GtkWidget *widget);

  gboolean     (* touch_event)            (GtkWidget     *widget,
                                           GdkEventTouch *event);

  /*< private >*/

  GtkWidgetClassPrivate *priv;

  /* Padding for future expansion */
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
};

struct _GtkWidgetAuxInfo
{
  gint width;
  gint height;

  guint   halign : 4;
  guint   valign : 4;

  GtkBorder margin;
};

GType	   gtk_widget_get_type		  (void) G_GNUC_CONST;
GtkWidget* gtk_widget_new		  (GType		type,
					   const gchar	       *first_property_name,
					   ...);
void	   gtk_widget_destroy		  (GtkWidget	       *widget);
void	   gtk_widget_destroyed		  (GtkWidget	       *widget,
					   GtkWidget	      **widget_pointer);
void	   gtk_widget_unparent		  (GtkWidget	       *widget);
void       gtk_widget_show                (GtkWidget           *widget);
void       gtk_widget_hide                (GtkWidget           *widget);
void       gtk_widget_show_now            (GtkWidget           *widget);
void       gtk_widget_show_all            (GtkWidget           *widget);
void       gtk_widget_set_no_show_all     (GtkWidget           *widget,
                                           gboolean             no_show_all);
gboolean   gtk_widget_get_no_show_all     (GtkWidget           *widget);
void	   gtk_widget_map		  (GtkWidget	       *widget);
void	   gtk_widget_unmap		  (GtkWidget	       *widget);
void	   gtk_widget_realize		  (GtkWidget	       *widget);
void	   gtk_widget_unrealize		  (GtkWidget	       *widget);

void       gtk_widget_draw                (GtkWidget           *widget,
                                           cairo_t             *cr);
/* Queuing draws */
void	   gtk_widget_queue_draw	  (GtkWidget	       *widget);
void	   gtk_widget_queue_draw_area	  (GtkWidget	       *widget,
					   gint                 x,
					   gint                 y,
					   gint                 width,
					   gint                 height);
void	   gtk_widget_queue_draw_region   (GtkWidget	       *widget,
                                           const cairo_region_t*region);
void	   gtk_widget_queue_resize	  (GtkWidget	       *widget);
void	   gtk_widget_queue_resize_no_redraw (GtkWidget *widget);
GDK_DEPRECATED_IN_3_0_FOR(gtk_widget_get_preferred_size)
void       gtk_widget_size_request        (GtkWidget           *widget,
                                           GtkRequisition      *requisition);
void	   gtk_widget_size_allocate	  (GtkWidget	       *widget,
					   GtkAllocation       *allocation);

GtkSizeRequestMode  gtk_widget_get_request_mode               (GtkWidget      *widget);
void                gtk_widget_get_preferred_width            (GtkWidget      *widget,
                                                               gint           *minimum_width,
                                                               gint           *natural_width);
void                gtk_widget_get_preferred_height_for_width (GtkWidget      *widget,
                                                               gint            width,
                                                               gint           *minimum_height,
                                                               gint           *natural_height);
void                gtk_widget_get_preferred_height           (GtkWidget      *widget,
                                                               gint           *minimum_height,
                                                               gint           *natural_height);
void                gtk_widget_get_preferred_width_for_height (GtkWidget      *widget,
                                                               gint            height,
                                                               gint           *minimum_width,
                                                               gint           *natural_width);
void                gtk_widget_get_preferred_size             (GtkWidget      *widget,
                                                               GtkRequisition *minimum_size,
                                                               GtkRequisition *natural_size);

GDK_DEPRECATED_IN_3_0_FOR(gtk_widget_get_preferred_size)
void       gtk_widget_get_child_requisition (GtkWidget         *widget,
                                             GtkRequisition    *requisition);
void	   gtk_widget_add_accelerator	  (GtkWidget           *widget,
					   const gchar         *accel_signal,
					   GtkAccelGroup       *accel_group,
					   guint                accel_key,
					   GdkModifierType      accel_mods,
					   GtkAccelFlags        accel_flags);
gboolean   gtk_widget_remove_accelerator  (GtkWidget           *widget,
					   GtkAccelGroup       *accel_group,
					   guint                accel_key,
					   GdkModifierType      accel_mods);
void       gtk_widget_set_accel_path      (GtkWidget           *widget,
					   const gchar         *accel_path,
					   GtkAccelGroup       *accel_group);
GList*     gtk_widget_list_accel_closures (GtkWidget	       *widget);
gboolean   gtk_widget_can_activate_accel  (GtkWidget           *widget,
                                           guint                signal_id);
gboolean   gtk_widget_mnemonic_activate   (GtkWidget           *widget,
					   gboolean             group_cycling);
gboolean   gtk_widget_event		  (GtkWidget	       *widget,
					   GdkEvent	       *event);
gint       gtk_widget_send_expose         (GtkWidget           *widget,
					   GdkEvent            *event);
gboolean   gtk_widget_send_focus_change   (GtkWidget           *widget,
                                           GdkEvent            *event);

gboolean   gtk_widget_activate		     (GtkWidget	       *widget);
     
void	   gtk_widget_reparent		  (GtkWidget	       *widget,
					   GtkWidget	       *new_parent);
gboolean   gtk_widget_intersect		  (GtkWidget	       *widget,
					   const GdkRectangle  *area,
					   GdkRectangle	       *intersection);
cairo_region_t *gtk_widget_region_intersect	  (GtkWidget	       *widget,
					   const cairo_region_t     *region);

void	gtk_widget_freeze_child_notify	  (GtkWidget	       *widget);
void	gtk_widget_child_notify		  (GtkWidget	       *widget,
					   const gchar	       *child_property);
void	gtk_widget_thaw_child_notify	  (GtkWidget	       *widget);

void       gtk_widget_set_can_focus       (GtkWidget           *widget,
                                           gboolean             can_focus);
gboolean   gtk_widget_get_can_focus       (GtkWidget           *widget);
gboolean   gtk_widget_has_focus           (GtkWidget           *widget);
gboolean   gtk_widget_is_focus            (GtkWidget           *widget);
GDK_AVAILABLE_IN_3_2
gboolean   gtk_widget_has_visible_focus   (GtkWidget           *widget);
void       gtk_widget_grab_focus          (GtkWidget           *widget);

void       gtk_widget_set_can_default     (GtkWidget           *widget,
                                           gboolean             can_default);
gboolean   gtk_widget_get_can_default     (GtkWidget           *widget);
gboolean   gtk_widget_has_default         (GtkWidget           *widget);
void       gtk_widget_grab_default        (GtkWidget           *widget);

void      gtk_widget_set_receives_default (GtkWidget           *widget,
                                           gboolean             receives_default);
gboolean  gtk_widget_get_receives_default (GtkWidget           *widget);

gboolean   gtk_widget_has_grab            (GtkWidget           *widget);

gboolean   gtk_widget_device_is_shadowed  (GtkWidget           *widget,
                                           GdkDevice           *device);


void                  gtk_widget_set_name               (GtkWidget    *widget,
							 const gchar  *name);
const gchar *         gtk_widget_get_name               (GtkWidget    *widget);

void                  gtk_widget_set_state              (GtkWidget    *widget,
							 GtkStateType  state);
GtkStateType          gtk_widget_get_state              (GtkWidget    *widget);

void                  gtk_widget_set_state_flags        (GtkWidget     *widget,
                                                         GtkStateFlags  flags,
                                                         gboolean       clear);
void                  gtk_widget_unset_state_flags      (GtkWidget     *widget,
                                                         GtkStateFlags  flags);
GtkStateFlags         gtk_widget_get_state_flags        (GtkWidget     *widget);

void                  gtk_widget_set_sensitive          (GtkWidget    *widget,
							 gboolean      sensitive);
gboolean              gtk_widget_get_sensitive          (GtkWidget    *widget);
gboolean              gtk_widget_is_sensitive           (GtkWidget    *widget);

void                  gtk_widget_set_visible            (GtkWidget    *widget,
                                                         gboolean      visible);
gboolean              gtk_widget_get_visible            (GtkWidget    *widget);

void                  gtk_widget_set_has_window         (GtkWidget    *widget,
                                                         gboolean      has_window);
gboolean              gtk_widget_get_has_window         (GtkWidget    *widget);

gboolean              gtk_widget_is_toplevel            (GtkWidget    *widget);
gboolean              gtk_widget_is_drawable            (GtkWidget    *widget);
void                  gtk_widget_set_realized           (GtkWidget    *widget,
                                                         gboolean      realized);
gboolean              gtk_widget_get_realized           (GtkWidget    *widget);
void                  gtk_widget_set_mapped             (GtkWidget    *widget,
                                                         gboolean      mapped);
gboolean              gtk_widget_get_mapped             (GtkWidget    *widget);

void                  gtk_widget_set_app_paintable      (GtkWidget    *widget,
							 gboolean      app_paintable);
gboolean              gtk_widget_get_app_paintable      (GtkWidget    *widget);

void                  gtk_widget_set_double_buffered    (GtkWidget    *widget,
							 gboolean      double_buffered);
gboolean              gtk_widget_get_double_buffered    (GtkWidget    *widget);

void                  gtk_widget_set_redraw_on_allocate (GtkWidget    *widget,
							 gboolean      redraw_on_allocate);

void                  gtk_widget_set_parent             (GtkWidget    *widget,
							 GtkWidget    *parent);
GtkWidget           * gtk_widget_get_parent             (GtkWidget    *widget);

void                  gtk_widget_set_parent_window      (GtkWidget    *widget,
							 GdkWindow    *parent_window);
GdkWindow           * gtk_widget_get_parent_window      (GtkWidget    *widget);

void                  gtk_widget_set_child_visible      (GtkWidget    *widget,
							 gboolean      is_visible);
gboolean              gtk_widget_get_child_visible      (GtkWidget    *widget);

void                  gtk_widget_set_window             (GtkWidget    *widget,
                                                         GdkWindow    *window);
GdkWindow           * gtk_widget_get_window             (GtkWidget    *widget);

int                   gtk_widget_get_allocated_width    (GtkWidget     *widget);
int                   gtk_widget_get_allocated_height   (GtkWidget     *widget);

void                  gtk_widget_get_allocation         (GtkWidget     *widget,
                                                         GtkAllocation *allocation);
void                  gtk_widget_set_allocation         (GtkWidget     *widget,
                                                         const GtkAllocation *allocation);

void                  gtk_widget_get_requisition        (GtkWidget     *widget,
                                                         GtkRequisition *requisition);

gboolean   gtk_widget_child_focus         (GtkWidget           *widget,
                                           GtkDirectionType     direction);
gboolean   gtk_widget_keynav_failed       (GtkWidget           *widget,
                                           GtkDirectionType     direction);
void       gtk_widget_error_bell          (GtkWidget           *widget);

void       gtk_widget_set_size_request    (GtkWidget           *widget,
                                           gint                 width,
                                           gint                 height);
void       gtk_widget_get_size_request    (GtkWidget           *widget,
                                           gint                *width,
                                           gint                *height);
void	   gtk_widget_set_events	  (GtkWidget	       *widget,
					   gint			events);
void       gtk_widget_add_events          (GtkWidget           *widget,
					   gint	                events);
void	   gtk_widget_set_device_events	  (GtkWidget	       *widget,
                                           GdkDevice           *device,
					   GdkEventMask		events);
void       gtk_widget_add_device_events   (GtkWidget           *widget,
                                           GdkDevice           *device,
					   GdkEventMask         events);

void       gtk_widget_set_device_enabled  (GtkWidget    *widget,
                                           GdkDevice    *device,
                                           gboolean      enabled);
gboolean   gtk_widget_get_device_enabled  (GtkWidget    *widget,
                                           GdkDevice    *device);

GtkWidget*   gtk_widget_get_toplevel	(GtkWidget	*widget);
GtkWidget*   gtk_widget_get_ancestor	(GtkWidget	*widget,
					 GType		 widget_type);
GdkVisual*   gtk_widget_get_visual	(GtkWidget	*widget);
void         gtk_widget_set_visual	(GtkWidget	*widget,
                                         GdkVisual      *visual);

GdkScreen *   gtk_widget_get_screen      (GtkWidget *widget);
gboolean      gtk_widget_has_screen      (GtkWidget *widget);
GdkDisplay *  gtk_widget_get_display     (GtkWidget *widget);
GdkWindow *   gtk_widget_get_root_window (GtkWidget *widget);
GtkSettings*  gtk_widget_get_settings    (GtkWidget *widget);
GtkClipboard *gtk_widget_get_clipboard   (GtkWidget *widget,
					  GdkAtom    selection);


/* Expand flags and related support */
gboolean gtk_widget_get_hexpand          (GtkWidget      *widget);
void     gtk_widget_set_hexpand          (GtkWidget      *widget,
                                          gboolean        expand);
gboolean gtk_widget_get_hexpand_set      (GtkWidget      *widget);
void     gtk_widget_set_hexpand_set      (GtkWidget      *widget,
                                          gboolean        set);
gboolean gtk_widget_get_vexpand          (GtkWidget      *widget);
void     gtk_widget_set_vexpand          (GtkWidget      *widget,
                                          gboolean        expand);
gboolean gtk_widget_get_vexpand_set      (GtkWidget      *widget);
void     gtk_widget_set_vexpand_set      (GtkWidget      *widget,
                                          gboolean        set);
void     gtk_widget_queue_compute_expand (GtkWidget      *widget);
gboolean gtk_widget_compute_expand       (GtkWidget      *widget,
                                          GtkOrientation  orientation);


/* Multidevice support */
gboolean         gtk_widget_get_support_multidevice (GtkWidget      *widget);
void             gtk_widget_set_support_multidevice (GtkWidget      *widget,
                                                     gboolean        support_multidevice);

/* Accessibility support */
GDK_AVAILABLE_IN_3_2
void             gtk_widget_class_set_accessible_type    (GtkWidgetClass     *widget_class,
                                                          GType               type);
GDK_AVAILABLE_IN_3_2
void             gtk_widget_class_set_accessible_role    (GtkWidgetClass     *widget_class,
                                                          AtkRole             role);
AtkObject*       gtk_widget_get_accessible               (GtkWidget          *widget);


/* Margin and alignment */
GtkAlign gtk_widget_get_halign        (GtkWidget *widget);
void     gtk_widget_set_halign        (GtkWidget *widget,
                                       GtkAlign   align);
GtkAlign gtk_widget_get_valign        (GtkWidget *widget);
void     gtk_widget_set_valign        (GtkWidget *widget,
                                       GtkAlign   align);
gint     gtk_widget_get_margin_left   (GtkWidget *widget);
void     gtk_widget_set_margin_left   (GtkWidget *widget,
                                       gint       margin);
gint     gtk_widget_get_margin_right  (GtkWidget *widget);
void     gtk_widget_set_margin_right  (GtkWidget *widget,
                                       gint       margin);
gint     gtk_widget_get_margin_top    (GtkWidget *widget);
void     gtk_widget_set_margin_top    (GtkWidget *widget,
                                       gint       margin);
gint     gtk_widget_get_margin_bottom (GtkWidget *widget);
void     gtk_widget_set_margin_bottom (GtkWidget *widget,
                                       gint       margin);


gint	     gtk_widget_get_events	(GtkWidget	*widget);
GdkEventMask gtk_widget_get_device_events (GtkWidget	*widget,
                                           GdkDevice    *device);
GDK_DEPRECATED_IN_3_4_FOR(gdk_window_get_device_position)
void	     gtk_widget_get_pointer	(GtkWidget	*widget,
					 gint		*x,
					 gint		*y);

gboolean     gtk_widget_is_ancestor	(GtkWidget	*widget,
					 GtkWidget	*ancestor);

gboolean     gtk_widget_translate_coordinates (GtkWidget  *src_widget,
					       GtkWidget  *dest_widget,
					       gint        src_x,
					       gint        src_y,
					       gint       *dest_x,
					       gint       *dest_y);

/* Hide widget and return TRUE.
 */
gboolean     gtk_widget_hide_on_delete	(GtkWidget	*widget);

/* Functions to override widget styling */
void         gtk_widget_override_color            (GtkWidget     *widget,
                                                   GtkStateFlags  state,
                                                   const GdkRGBA *color);
void         gtk_widget_override_background_color (GtkWidget     *widget,
                                                   GtkStateFlags  state,
                                                   const GdkRGBA *color);

void         gtk_widget_override_font             (GtkWidget                  *widget,
                                                   const PangoFontDescription *font_desc);

void         gtk_widget_override_symbolic_color   (GtkWidget     *widget,
                                                   const gchar   *name,
                                                   const GdkRGBA *color);
void         gtk_widget_override_cursor           (GtkWidget       *widget,
                                                   const GdkRGBA   *cursor,
                                                   const GdkRGBA   *secondary_cursor);

void       gtk_widget_reset_style       (GtkWidget      *widget);

PangoContext *gtk_widget_create_pango_context (GtkWidget   *widget);
PangoContext *gtk_widget_get_pango_context    (GtkWidget   *widget);
PangoLayout  *gtk_widget_create_pango_layout  (GtkWidget   *widget,
					       const gchar *text);

GdkPixbuf    *gtk_widget_render_icon_pixbuf   (GtkWidget   *widget,
                                               const gchar *stock_id,
                                               GtkIconSize  size);

/* handle composite names for GTK_COMPOSITE_CHILD widgets,
 * the returned name is newly allocated.
 */
void   gtk_widget_set_composite_name	(GtkWidget	*widget,
					 const gchar   	*name);
gchar* gtk_widget_get_composite_name	(GtkWidget	*widget);
     
/* Push/pop pairs, to change default values upon a widget's creation.
 * This will override the values that got set by the
 * gtk_widget_set_default_* () functions.
 */
void	     gtk_widget_push_composite_child (void);
void	     gtk_widget_pop_composite_child  (void);

/* widget style properties
 */
void gtk_widget_class_install_style_property        (GtkWidgetClass     *klass,
						     GParamSpec         *pspec);
void gtk_widget_class_install_style_property_parser (GtkWidgetClass     *klass,
						     GParamSpec         *pspec,
						     GtkRcPropertyParser parser);
GParamSpec*  gtk_widget_class_find_style_property   (GtkWidgetClass     *klass,
						     const gchar        *property_name);
GParamSpec** gtk_widget_class_list_style_properties (GtkWidgetClass     *klass,
						     guint              *n_properties);
void gtk_widget_style_get_property (GtkWidget	     *widget,
				    const gchar    *property_name,
				    GValue	     *value);
void gtk_widget_style_get_valist   (GtkWidget	     *widget,
				    const gchar    *first_property_name,
				    va_list         var_args);
void gtk_widget_style_get          (GtkWidget	     *widget,
				    const gchar    *first_property_name,
				    ...) G_GNUC_NULL_TERMINATED;

/* Functions for setting directionality for widgets */

void             gtk_widget_set_direction         (GtkWidget        *widget,
						   GtkTextDirection  dir);
GtkTextDirection gtk_widget_get_direction         (GtkWidget        *widget);

void             gtk_widget_set_default_direction (GtkTextDirection  dir);
GtkTextDirection gtk_widget_get_default_direction (void);

/* Compositing manager functionality */
gboolean gtk_widget_is_composited (GtkWidget *widget);

/* Counterpart to gdk_window_shape_combine_region.
 */
void	     gtk_widget_shape_combine_region (GtkWidget *widget,
                                              cairo_region_t *region);
void	     gtk_widget_input_shape_combine_region (GtkWidget *widget,
                                                    cairo_region_t *region);

GList* gtk_widget_list_mnemonic_labels  (GtkWidget *widget);
void   gtk_widget_add_mnemonic_label    (GtkWidget *widget,
					 GtkWidget *label);
void   gtk_widget_remove_mnemonic_label (GtkWidget *widget,
					 GtkWidget *label);

void                  gtk_widget_set_tooltip_window    (GtkWidget   *widget,
                                                        GtkWindow   *custom_window);
GtkWindow *gtk_widget_get_tooltip_window    (GtkWidget   *widget);
void       gtk_widget_trigger_tooltip_query (GtkWidget   *widget);
void       gtk_widget_set_tooltip_text      (GtkWidget   *widget,
                                             const gchar *text);
gchar *    gtk_widget_get_tooltip_text      (GtkWidget   *widget);
void       gtk_widget_set_tooltip_markup    (GtkWidget   *widget,
                                             const gchar *markup);
gchar *    gtk_widget_get_tooltip_markup    (GtkWidget   *widget);
void       gtk_widget_set_has_tooltip       (GtkWidget   *widget,
					     gboolean     has_tooltip);
gboolean   gtk_widget_get_has_tooltip       (GtkWidget   *widget);

gboolean   gtk_cairo_should_draw_window     (cairo_t     *cr,
                                             GdkWindow   *window);
void       gtk_cairo_transform_to_window    (cairo_t     *cr,
                                             GtkWidget   *widget,
                                             GdkWindow   *window);

GType           gtk_requisition_get_type (void) G_GNUC_CONST;
GtkRequisition *gtk_requisition_new      (void) G_GNUC_MALLOC;
GtkRequisition *gtk_requisition_copy     (const GtkRequisition *requisition);
void            gtk_requisition_free     (GtkRequisition       *requisition);

gboolean     gtk_widget_in_destruction (GtkWidget *widget);

GtkStyleContext * gtk_widget_get_style_context (GtkWidget *widget);

GtkWidgetPath *   gtk_widget_get_path (GtkWidget *widget);

GDK_AVAILABLE_IN_3_4
GdkModifierType   gtk_widget_get_modifier_mask (GtkWidget         *widget,
                                                GdkModifierIntent  intent);

GDK_AVAILABLE_IN_3_6
void                    gtk_widget_insert_action_group                  (GtkWidget    *widget,
                                                                         const gchar  *name,
                                                                         GActionGroup *group);

G_END_DECLS

#endif /* __GTK_WIDGET_H__ */
