/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_WIDGET_H__
#define __GTK_WIDGET_H__

#include <gdk/gdk.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkstyle.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* The flags that are used by GtkWidget on top of the
 * flags field of GtkObject.
 */
typedef enum
{
  GTK_TOPLEVEL         = 1 << 4,
  GTK_NO_WINDOW        = 1 << 5,
  GTK_REALIZED         = 1 << 6,
  GTK_MAPPED           = 1 << 7,
  GTK_VISIBLE          = 1 << 8,
  GTK_SENSITIVE        = 1 << 9,
  GTK_PARENT_SENSITIVE = 1 << 10,
  GTK_CAN_FOCUS        = 1 << 11,
  GTK_HAS_FOCUS        = 1 << 12,
 
  /* widget is allowed to receive the default via gtk_widget_grab_default
   * and will reserve space to draw the default if possible */
  GTK_CAN_DEFAULT      = 1 << 13,

  /* the widget currently is receiving the default action and should be drawn
   * appropriately if possible */
  GTK_HAS_DEFAULT      = 1 << 14,

  GTK_HAS_GRAB	       = 1 << 15,
  GTK_RC_STYLE	       = 1 << 16,
  GTK_COMPOSITE_CHILD  = 1 << 17,
  GTK_NO_REPARENT      = 1 << 18,
  GTK_APP_PAINTABLE    = 1 << 19,
 
  /* the widget when focused will receive the default action and have
   * HAS_DEFAULT set even if there is a different widget set as default */
  GTK_RECEIVES_DEFAULT = 1 << 20
} GtkWidgetFlags;

/* Macro for casting a pointer to a GtkWidget or GtkWidgetClass pointer.
 * Macros for testing whether `widget' or `klass' are of type GTK_TYPE_WIDGET.
 */
#define GTK_TYPE_WIDGET			  (gtk_widget_get_type ())
#define GTK_WIDGET(widget)		  (GTK_CHECK_CAST ((widget), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_WIDGET_CLASS(klass)		  (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_WIDGET, GtkWidgetClass))
#define GTK_IS_WIDGET(widget)		  (GTK_CHECK_TYPE ((widget), GTK_TYPE_WIDGET))
#define GTK_IS_WIDGET_CLASS(klass)	  (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WIDGET))

/* Macros for extracting various fields from GtkWidget and GtkWidgetClass.
 */
#define GTK_WIDGET_TYPE(wid)		  (GTK_OBJECT_TYPE (wid))
#define GTK_WIDGET_STATE(wid)		  (GTK_WIDGET (wid)->state)
#define GTK_WIDGET_SAVED_STATE(wid)	  (GTK_WIDGET (wid)->saved_state)

/* Macros for extracting the widget flags from GtkWidget.
 */
#define GTK_WIDGET_FLAGS(wid)		  (GTK_OBJECT_FLAGS (wid))
#define GTK_WIDGET_TOPLEVEL(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_TOPLEVEL) != 0)
#define GTK_WIDGET_NO_WINDOW(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_NO_WINDOW) != 0)
#define GTK_WIDGET_REALIZED(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_REALIZED) != 0)
#define GTK_WIDGET_MAPPED(wid)		  ((GTK_WIDGET_FLAGS (wid) & GTK_MAPPED) != 0)
#define GTK_WIDGET_VISIBLE(wid)		  ((GTK_WIDGET_FLAGS (wid) & GTK_VISIBLE) != 0)
#define GTK_WIDGET_DRAWABLE(wid)	  (GTK_WIDGET_VISIBLE (wid) && GTK_WIDGET_MAPPED (wid))
#define GTK_WIDGET_SENSITIVE(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_SENSITIVE) != 0)
#define GTK_WIDGET_PARENT_SENSITIVE(wid)  ((GTK_WIDGET_FLAGS (wid) & GTK_PARENT_SENSITIVE) != 0)
#define GTK_WIDGET_IS_SENSITIVE(wid)	  (GTK_WIDGET_SENSITIVE (wid) && \
					   GTK_WIDGET_PARENT_SENSITIVE (wid))
#define GTK_WIDGET_CAN_FOCUS(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_CAN_FOCUS) != 0)
#define GTK_WIDGET_HAS_FOCUS(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_HAS_FOCUS) != 0)
#define GTK_WIDGET_CAN_DEFAULT(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_CAN_DEFAULT) != 0)
#define GTK_WIDGET_HAS_DEFAULT(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_HAS_DEFAULT) != 0)
#define GTK_WIDGET_HAS_GRAB(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_HAS_GRAB) != 0)
#define GTK_WIDGET_RC_STYLE(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_RC_STYLE) != 0)
#define GTK_WIDGET_COMPOSITE_CHILD(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_COMPOSITE_CHILD) != 0)
#define GTK_WIDGET_APP_PAINTABLE(wid)	  ((GTK_WIDGET_FLAGS (wid) & GTK_APP_PAINTABLE) != 0)
#define GTK_WIDGET_RECEIVES_DEFAULT(wid)  ((GTK_WIDGET_FLAGS (wid) & GTK_RECEIVES_DEFAULT) != 0)
  
/* Macros for setting and clearing widget flags.
 */
#define GTK_WIDGET_SET_FLAGS(wid,flag)	  G_STMT_START{ (GTK_WIDGET_FLAGS (wid) |= (flag)); }G_STMT_END
#define GTK_WIDGET_UNSET_FLAGS(wid,flag)  G_STMT_START{ (GTK_WIDGET_FLAGS (wid) &= ~(flag)); }G_STMT_END
  
  
  
typedef struct _GtkRequisition	  GtkRequisition;
typedef struct _GtkAllocation	  GtkAllocation;
typedef struct _GtkSelectionData GtkSelectionData;
typedef struct _GtkWidgetClass	  GtkWidgetClass;
typedef struct _GtkWidgetAuxInfo  GtkWidgetAuxInfo;
typedef struct _GtkWidgetShapeInfo GtkWidgetShapeInfo;

typedef void (*GtkCallback) (GtkWidget *widget,
			     gpointer	data);

/* A requisition is a desired amount of space which a
 *  widget may request.
 */
struct _GtkRequisition
{
  gint16 width;
  gint16 height;
};

/* An allocation is a size and position. Where a widget
 *  can ask for a desired size, it is actually given
 *  this amount of space at the specified position.
 */
struct _GtkAllocation
{
  gint16 x;
  gint16 y;
  guint16 width;
  guint16 height;
};

/* The contents of a selection are returned in a GtkSelectionData
   structure. selection/target identify the request. 
   type specifies the type of the return; if length < 0, and
   the data should be ignored. This structure has object semantics -
   no fields should be modified directly, they should not be created
   directly, and pointers to them should not be stored beyond the duration of
   a callback. (If the last is changed, we'll need to add reference
   counting.) The time field gives the timestamp at which the data was sent. */

struct _GtkSelectionData
{
  GdkAtom selection;
  GdkAtom target;
  GdkAtom type;
  gint	  format;
  guchar *data;
  gint	  length;
};

/* The widget is the base of the tree for displayable objects.
 *  (A displayable object is one which takes up some amount
 *  of screen real estate). It provides a common base and interface
 *  which actual widgets must adhere to.
 */
struct _GtkWidget
{
  /* The object structure needs to be the first
   *  element in the widget structure in order for
   *  the object mechanism to work correctly. This
   *  allows a GtkWidget pointer to be cast to a
   *  GtkObject pointer.
   */
  GtkObject object;
  
  /* 16 bits of internally used private flags.
   * this will be packed into the same 4 byte alignment frame that
   * state and saved_state go. we therefore don't waste any new
   * space on this.
   */
  guint16 private_flags;
  
  /* The state of the widget. There are actually only
   *  5 widget states (defined in "gtkenums.h").
   */
  guint8 state;
  
  /* The saved state of the widget. When a widgets state
   *  is changed to GTK_STATE_INSENSITIVE via
   *  "gtk_widget_set_state" or "gtk_widget_set_sensitive"
   *  the old state is kept around in this field. The state
   *  will be restored once the widget gets sensitive again.
   */
  guint8 saved_state;
  
  /* The widgets name. If the widget does not have a name
   *  (the name is NULL), then its name (as returned by
   *  "gtk_widget_get_name") is its classes name.
   * Among other things, the widget name is used to determine
   *  the style to use for a widget.
   */
  gchar *name;
  
  /* The style for the widget. The style contains the
   *  colors the widget should be drawn in for each state
   *  along with graphics contexts used to draw with and
   *  the font to use for text.
   */
  GtkStyle *style;
  
  /* The widgets desired size.
   */
  GtkRequisition requisition;
  
  /* The widgets allocated size.
   */
  GtkAllocation allocation;
  
  /* The widgets window or its parent window if it does
   *  not have a window. (Which will be indicated by the
   *  GTK_NO_WINDOW flag being set).
   */
  GdkWindow *window;
  
  /* The widgets parent.
   */
  GtkWidget *parent;
};

struct _GtkWidgetClass
{
  /* The object class structure needs to be the first
   *  element in the widget class structure in order for
   *  the class mechanism to work correctly. This allows a
   *  GtkWidgetClass pointer to be cast to a GtkObjectClass
   *  pointer.
   */
  GtkObjectClass parent_class;
  
  /* The signal to emit when a widget of this class is activated,
   * gtk_widget_activate() handles the emission.
   * Implementation of this signal is optional.
   */
  guint activate_signal;

  /* This signal is emitted  when a widget of this class is added
   * to a scrolling aware parent, gtk_widget_set_scroll_adjustments()
   * handles the emission.
   * Implementation of this signal is optional.
   */
  guint set_scroll_adjustments_signal;
  
  /* basics */
  void (* show)		       (GtkWidget      *widget);
  void (* show_all)            (GtkWidget      *widget);
  void (* hide)		       (GtkWidget      *widget);
  void (* hide_all)            (GtkWidget      *widget);
  void (* map)		       (GtkWidget      *widget);
  void (* unmap)	       (GtkWidget      *widget);
  void (* realize)	       (GtkWidget      *widget);
  void (* unrealize)	       (GtkWidget      *widget);
  void (* draw)		       (GtkWidget      *widget,
				GdkRectangle   *area);
  void (* draw_focus)	       (GtkWidget      *widget);
  void (* draw_default)	       (GtkWidget      *widget);
  void (* size_request)	       (GtkWidget      *widget,
				GtkRequisition *requisition);
  void (* size_allocate)       (GtkWidget      *widget,
				GtkAllocation  *allocation);
  void (* state_changed)       (GtkWidget      *widget,
				GtkStateType	previous_state);
  void (* parent_set)	       (GtkWidget      *widget,
				GtkWidget      *previous_parent);
  void (* style_set)	       (GtkWidget      *widget,
				GtkStyle       *previous_style);
  
  /* accelerators */
  gint (* add_accelerator)     (GtkWidget      *widget,
				guint           accel_signal_id,
				GtkAccelGroup  *accel_group,
				guint           accel_key,
				GdkModifierType accel_mods,
				GtkAccelFlags   accel_flags);
  void (* remove_accelerator)  (GtkWidget      *widget,
				GtkAccelGroup  *accel_group,
				guint           accel_key,
				GdkModifierType accel_mods);

  /* explicit focus */
  void (* grab_focus)          (GtkWidget      *widget);
  
  /* events */
  gint (* event)		   (GtkWidget	       *widget,
				    GdkEvent	       *event);
  gint (* button_press_event)	   (GtkWidget	       *widget,
				    GdkEventButton     *event);
  gint (* button_release_event)	   (GtkWidget	       *widget,
				    GdkEventButton     *event);
  gint (* motion_notify_event)	   (GtkWidget	       *widget,
				    GdkEventMotion     *event);
  gint (* delete_event)		   (GtkWidget	       *widget,
				    GdkEventAny	       *event);
  gint (* destroy_event)	   (GtkWidget	       *widget,
				    GdkEventAny	       *event);
  gint (* expose_event)		   (GtkWidget	       *widget,
				    GdkEventExpose     *event);
  gint (* key_press_event)	   (GtkWidget	       *widget,
				    GdkEventKey	       *event);
  gint (* key_release_event)	   (GtkWidget	       *widget,
				    GdkEventKey	       *event);
  gint (* enter_notify_event)	   (GtkWidget	       *widget,
				    GdkEventCrossing   *event);
  gint (* leave_notify_event)	   (GtkWidget	       *widget,
				    GdkEventCrossing   *event);
  gint (* configure_event)	   (GtkWidget	       *widget,
				    GdkEventConfigure  *event);
  gint (* focus_in_event)	   (GtkWidget	       *widget,
				    GdkEventFocus      *event);
  gint (* focus_out_event)	   (GtkWidget	       *widget,
				    GdkEventFocus      *event);
  gint (* map_event)		   (GtkWidget	       *widget,
				    GdkEventAny	       *event);
  gint (* unmap_event)		   (GtkWidget	       *widget,
				    GdkEventAny	       *event);
  gint (* property_notify_event)   (GtkWidget	       *widget,
				    GdkEventProperty   *event);
  gint (* selection_clear_event)   (GtkWidget	       *widget,
				    GdkEventSelection  *event);
  gint (* selection_request_event) (GtkWidget	       *widget,
				    GdkEventSelection  *event);
  gint (* selection_notify_event)  (GtkWidget	       *widget,
				    GdkEventSelection  *event);
  gint (* proximity_in_event)	   (GtkWidget	       *widget,
				    GdkEventProximity  *event);
  gint (* proximity_out_event)	   (GtkWidget	       *widget,
				    GdkEventProximity  *event);
  gint (* visibility_notify_event)  (GtkWidget	       *widget,
				     GdkEventVisibility *event);
  gint (* client_event)		   (GtkWidget	       *widget,
				    GdkEventClient     *event);
  gint (* no_expose_event)	   (GtkWidget	       *widget,
				    GdkEventAny	       *event);

  /* selection */
  void (* selection_get)           (GtkWidget          *widget,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint               time);
  void (* selection_received)      (GtkWidget          *widget,
				    GtkSelectionData   *selection_data,
				    guint               time);

  /* Source side drag signals */
  void (* drag_begin)	           (GtkWidget	       *widget,
				    GdkDragContext     *context);
  void (* drag_end)	           (GtkWidget	       *widget,
				    GdkDragContext     *context);
  void (* drag_data_get)           (GtkWidget          *widget,
				    GdkDragContext     *context,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint               time);
  void (* drag_data_delete)        (GtkWidget	       *widget,
				    GdkDragContext     *context);

  /* Target side drag signals */
  void (* drag_leave)	           (GtkWidget	       *widget,
				    GdkDragContext     *context,
				    guint               time);
  gboolean (* drag_motion)         (GtkWidget	       *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    guint               time);
  gboolean (* drag_drop)           (GtkWidget	       *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    guint               time);
  void (* drag_data_received)      (GtkWidget          *widget,
				    GdkDragContext     *context,
				    gint                x,
				    gint                y,
				    GtkSelectionData   *selection_data,
				    guint               info,
				    guint               time);
  
  /* action signals */
  void (* debug_msg)		   (GtkWidget	       *widget,
				    const gchar	       *string);

  /* Padding for future expandsion */
  GtkFunction pad1;
  GtkFunction pad2;
  GtkFunction pad3;
  GtkFunction pad4;
};

struct _GtkWidgetAuxInfo
{
  gint16  x;
  gint16  y;
  gint16 width;
  gint16 height;
};

struct _GtkWidgetShapeInfo
{
  gint16     offset_x;
  gint16     offset_y;
  GdkBitmap *shape_mask;
};


GtkType	   gtk_widget_get_type		  (void);
GtkWidget* gtk_widget_new		  (GtkType		type,
					   const gchar	       *first_arg_name,
					   ...);
GtkWidget* gtk_widget_newv		  (GtkType		type,
					   guint		nargs,
					   GtkArg	       *args);
void	   gtk_widget_ref		  (GtkWidget	       *widget);
void	   gtk_widget_unref		  (GtkWidget	       *widget);
void	   gtk_widget_destroy		  (GtkWidget	       *widget);
void	   gtk_widget_destroyed		  (GtkWidget	       *widget,
					   GtkWidget	      **widget_pointer);
void	   gtk_widget_get		  (GtkWidget	       *widget,
					   GtkArg	       *arg);
void	   gtk_widget_getv		  (GtkWidget	       *widget,
					   guint		nargs,
					   GtkArg	       *args);
void	   gtk_widget_set		  (GtkWidget	       *widget,
					   const gchar         *first_arg_name,
					   ...);
void	   gtk_widget_setv		  (GtkWidget	       *widget,
					   guint		nargs,
					   GtkArg	       *args);
void	   gtk_widget_unparent		  (GtkWidget	       *widget);
void	   gtk_widget_show		  (GtkWidget	       *widget);
void       gtk_widget_show_now            (GtkWidget           *widget);
void	   gtk_widget_hide		  (GtkWidget	       *widget);
void	   gtk_widget_show_all		  (GtkWidget	       *widget);
void	   gtk_widget_hide_all		  (GtkWidget	       *widget);
void	   gtk_widget_map		  (GtkWidget	       *widget);
void	   gtk_widget_unmap		  (GtkWidget	       *widget);
void	   gtk_widget_realize		  (GtkWidget	       *widget);
void	   gtk_widget_unrealize		  (GtkWidget	       *widget);

/* Queuing draws */
void	   gtk_widget_queue_draw	  (GtkWidget	       *widget);
void	   gtk_widget_queue_draw_area	  (GtkWidget	       *widget,
					   gint                 x,
					   gint                 y,
					   gint                 width,
					   gint                 height);
void	   gtk_widget_queue_clear	  (GtkWidget	       *widget);
void	   gtk_widget_queue_clear_area	  (GtkWidget	       *widget,
					   gint                 x,
					   gint                 y,
					   gint                 width,
					   gint                 height);


void	   gtk_widget_queue_resize	  (GtkWidget	       *widget);
void	   gtk_widget_draw		  (GtkWidget	       *widget,
					   GdkRectangle	       *area);
void	   gtk_widget_draw_focus	  (GtkWidget	       *widget);
void	   gtk_widget_draw_default	  (GtkWidget	       *widget);
void	   gtk_widget_size_request	  (GtkWidget	       *widget,
					   GtkRequisition      *requisition);
void	   gtk_widget_size_allocate	  (GtkWidget	       *widget,
					   GtkAllocation       *allocation);
void       gtk_widget_get_child_requisition (GtkWidget	       *widget,
					     GtkRequisition    *requisition);
void	   gtk_widget_add_accelerator	  (GtkWidget           *widget,
					   const gchar         *accel_signal,
					   GtkAccelGroup       *accel_group,
					   guint                accel_key,
					   guint                accel_mods,
					   GtkAccelFlags        accel_flags);
void	   gtk_widget_remove_accelerator  (GtkWidget           *widget,
					   GtkAccelGroup       *accel_group,
					   guint                accel_key,
					   guint                accel_mods);
void	   gtk_widget_remove_accelerators (GtkWidget           *widget,
					   const gchar	       *accel_signal,
					   gboolean		visible_only);
guint	   gtk_widget_accelerator_signal  (GtkWidget           *widget,
					   GtkAccelGroup       *accel_group,
					   guint                accel_key,
					   guint                accel_mods);
void	   gtk_widget_lock_accelerators   (GtkWidget	       *widget);
void	   gtk_widget_unlock_accelerators (GtkWidget	       *widget);
gboolean   gtk_widget_accelerators_locked (GtkWidget	       *widget);
gint	   gtk_widget_event		  (GtkWidget	       *widget,
					   GdkEvent	       *event);

gboolean   gtk_widget_activate		     (GtkWidget	       *widget);
gboolean   gtk_widget_set_scroll_adjustments (GtkWidget        *widget,
					      GtkAdjustment    *hadjustment,
					      GtkAdjustment    *vadjustment);
     
void	   gtk_widget_reparent		  (GtkWidget	       *widget,
					   GtkWidget	       *new_parent);
void	   gtk_widget_popup		  (GtkWidget	       *widget,
					   gint			x,
					   gint			y);
gint	   gtk_widget_intersect		  (GtkWidget	       *widget,
					   GdkRectangle	       *area,
					   GdkRectangle	       *intersection);

void	   gtk_widget_grab_focus	  (GtkWidget	       *widget);
void	   gtk_widget_grab_default	  (GtkWidget	       *widget);

void	   gtk_widget_set_name		  (GtkWidget	       *widget,
					   const gchar	       *name);
gchar*	   gtk_widget_get_name		  (GtkWidget	       *widget);
void	   gtk_widget_set_state		  (GtkWidget	       *widget,
					   GtkStateType		state);
void	   gtk_widget_set_sensitive	  (GtkWidget	       *widget,
					   gboolean		sensitive);
void	   gtk_widget_set_app_paintable	  (GtkWidget	       *widget,
					   gboolean		app_paintable);
void	   gtk_widget_set_parent	  (GtkWidget	       *widget,
					   GtkWidget	       *parent);
void	   gtk_widget_set_parent_window	  (GtkWidget	       *widget,
					   GdkWindow	       *parent_window);
GdkWindow *gtk_widget_get_parent_window	  (GtkWidget	       *widget);
void	   gtk_widget_set_uposition	  (GtkWidget	       *widget,
					   gint			x,
					   gint			y);
void	   gtk_widget_set_usize		  (GtkWidget	       *widget,
					   gint			width,
					   gint			height);
void	   gtk_widget_set_events	  (GtkWidget	       *widget,
					   gint			events);
void       gtk_widget_add_events          (GtkWidget           *widget,
					   gint	                events);
void	   gtk_widget_set_extension_events (GtkWidget		*widget,
					    GdkExtensionMode	mode);

GdkExtensionMode gtk_widget_get_extension_events (GtkWidget	*widget);
GtkWidget*   gtk_widget_get_toplevel	(GtkWidget	*widget);
GtkWidget*   gtk_widget_get_ancestor	(GtkWidget	*widget,
					 GtkType	widget_type);
GdkColormap* gtk_widget_get_colormap	(GtkWidget	*widget);
GdkVisual*   gtk_widget_get_visual	(GtkWidget	*widget);

/* The following functions must not be called on an already
 * realized widget. Because it is possible that somebody
 * can call get_colormap() or get_visual() and save the
 * result, these functions are probably only safe to
 * call in a widget's init() function.
 */
void         gtk_widget_set_colormap    (GtkWidget      *widget,
					 GdkColormap    *colormap);
void         gtk_widget_set_visual      (GtkWidget      *widget, 
					 GdkVisual      *visual);


gint	     gtk_widget_get_events	(GtkWidget	*widget);
void	     gtk_widget_get_pointer	(GtkWidget	*widget,
					 gint		*x,
					 gint		*y);

gboolean     gtk_widget_is_ancestor	(GtkWidget	*widget,
					 GtkWidget	*ancestor);

/* Hide widget and return TRUE.
 */
gint	   gtk_widget_hide_on_delete	(GtkWidget	*widget);

/* Widget styles.
 */
void	   gtk_widget_set_style		(GtkWidget	*widget,
					 GtkStyle	*style);
void	   gtk_widget_set_rc_style	(GtkWidget	*widget);
void	   gtk_widget_ensure_style	(GtkWidget	*widget);
GtkStyle*  gtk_widget_get_style		(GtkWidget	*widget);
void	   gtk_widget_restore_default_style (GtkWidget	*widget);

void       gtk_widget_modify_style      (GtkWidget      *widget,
					 GtkRcStyle     *style);

/* handle composite names for GTK_COMPOSITE_CHILD widgets,
 * the returned name is newly allocated.
 */
void   gtk_widget_set_composite_name	(GtkWidget	*widget,
					 const gchar   	*name);
gchar* gtk_widget_get_composite_name	(GtkWidget	*widget);
     
/* Descend recursively and set rc-style on all widgets without user styles */
void       gtk_widget_reset_rc_styles   (GtkWidget      *widget);

/* Push/pop pairs, to change default values upon a widget's creation.
 * This will override the values that got set by the
 * gtk_widget_set_default_* () functions.
 */
void	     gtk_widget_push_style	     (GtkStyle	 *style);
void	     gtk_widget_push_colormap	     (GdkColormap *cmap);
void	     gtk_widget_push_visual	     (GdkVisual	 *visual);
void	     gtk_widget_push_composite_child (void);
void	     gtk_widget_pop_composite_child  (void);
void	     gtk_widget_pop_style	     (void);
void	     gtk_widget_pop_colormap	     (void);
void	     gtk_widget_pop_visual	     (void);

/* Set certain default values to be used at widget creation time.
 */
void	     gtk_widget_set_default_style    (GtkStyle	  *style);
void	     gtk_widget_set_default_colormap (GdkColormap *colormap);
void	     gtk_widget_set_default_visual   (GdkVisual	  *visual);
GtkStyle*    gtk_widget_get_default_style    (void);
GdkColormap* gtk_widget_get_default_colormap (void);
GdkVisual*   gtk_widget_get_default_visual   (void);

/* Counterpart to gdk_window_shape_combine_mask.
 */
void	     gtk_widget_shape_combine_mask (GtkWidget *widget,
					    GdkBitmap *shape_mask,
					    gint       offset_x,
					    gint       offset_y);

/* internal function */
void	     gtk_widget_reset_shapes	   (GtkWidget *widget);

/* Compute a widget's path in the form "GtkWindow.MyLabel", and
 * return newly alocated strings.
 */
void	     gtk_widget_path		   (GtkWidget *widget,
					    guint     *path_length,
					    gchar    **path,
					    gchar    **path_reversed);
void	     gtk_widget_class_path	   (GtkWidget *widget,
					    guint     *path_length,
					    gchar    **path,
					    gchar    **path_reversed);

#if	defined (GTK_TRACE_OBJECTS) && defined (__GNUC__)
#  define gtk_widget_ref gtk_object_ref
#  define gtk_widget_unref gtk_object_unref
#endif	/* GTK_TRACE_OBJECTS && __GNUC__ */



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_WIDGET_H__ */
