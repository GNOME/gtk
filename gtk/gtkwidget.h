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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_WIDGET_H__
#define __GTK_WIDGET_H__


#include <gdk/gdk.h>
#include <gtk/gtkaccelerator.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkstyle.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* The flags that are used in the flags member of the GtkObject
 *  structure.
 */
enum
{
  GTK_VISIBLE          = 1 << 3,
  GTK_MAPPED           = 1 << 4,
  GTK_UNMAPPED         = 1 << 5,
  GTK_REALIZED         = 1 << 6,
  GTK_SENSITIVE        = 1 << 7,
  GTK_PARENT_SENSITIVE = 1 << 8,
  GTK_NO_WINDOW        = 1 << 9,
  GTK_HAS_FOCUS        = 1 << 10,
  GTK_CAN_FOCUS        = 1 << 11,
  GTK_HAS_DEFAULT      = 1 << 12,
  GTK_CAN_DEFAULT      = 1 << 13,
  GTK_PROPAGATE_STATE  = 1 << 14,
  GTK_ANCHORED         = 1 << 15,
  GTK_BASIC            = 1 << 16,
  GTK_USER_STYLE       = 1 << 17,
  GTK_HAS_GRAB         = 1 << 18,
  GTK_REDRAW_PENDING   = 1 << 19,
  GTK_RESIZE_PENDING   = 1 << 20,
  GTK_RESIZE_NEEDED    = 1 << 21,
  GTK_HAS_SHAPE_MASK   = 1 << 22,
  GTK_LEAVE_PENDING    = 1 << 23
};


/* Macro for casting a pointer to a GtkWidget pointer.
 */
#define GTK_WIDGET(obj)                   GTK_CHECK_CAST (obj, gtk_widget_get_type (), GtkWidget)

/* Macro for casting the "class" field of an object to
 *  a GtkWidgetClass pointer.
 */
#define GTK_WIDGET_CLASS(klass)           GTK_CHECK_CLASS_CAST (klass, gtk_widget_get_type (), GtkWidgetClass)

/* Macros for extracting various fields from GtkWidget and
 *  GtkWidgetClass structures.
 */
#define GTK_WIDGET_TYPE(obj)              (GTK_OBJECT_TYPE (obj))
#define GTK_WIDGET_STATE(obj)             (GTK_WIDGET (obj)->state)
#define GTK_WIDGET_SAVED_STATE(obj)       (GTK_WIDGET (obj)->saved_state)
#define GTK_WIDGET_VISIBLE(obj)           (GTK_OBJECT_FLAGS (obj) & GTK_VISIBLE)
#define GTK_WIDGET_MAPPED(obj)            (GTK_OBJECT_FLAGS (obj) & GTK_MAPPED)
#define GTK_WIDGET_UNMAPPED(obj)          (GTK_OBJECT_FLAGS (obj) & GTK_UNMAPPED)
#define GTK_WIDGET_REALIZED(obj)          (GTK_OBJECT_FLAGS (obj) & GTK_REALIZED)
#define GTK_WIDGET_SENSITIVE(obj)         (GTK_OBJECT_FLAGS (obj) & GTK_SENSITIVE)
#define GTK_WIDGET_PARENT_SENSITIVE(obj)  (GTK_OBJECT_FLAGS (obj) & GTK_PARENT_SENSITIVE)
#define GTK_WIDGET_IS_SENSITIVE(obj)      ((GTK_WIDGET_SENSITIVE (obj) && \
                                            GTK_WIDGET_PARENT_SENSITIVE (obj)) != 0)
#define GTK_WIDGET_NO_WINDOW(obj)         (GTK_OBJECT_FLAGS (obj) & GTK_NO_WINDOW)
#define GTK_WIDGET_HAS_FOCUS(obj)         (GTK_OBJECT_FLAGS (obj) & GTK_HAS_FOCUS)
#define GTK_WIDGET_CAN_FOCUS(obj)         (GTK_OBJECT_FLAGS (obj) & GTK_CAN_FOCUS)
#define GTK_WIDGET_HAS_DEFAULT(obj)       (GTK_OBJECT_FLAGS (obj) & GTK_HAS_DEFAULT)
#define GTK_WIDGET_CAN_DEFAULT(obj)       (GTK_OBJECT_FLAGS (obj) & GTK_CAN_DEFAULT)
#define GTK_WIDGET_PROPAGATE_STATE(obj)   (GTK_OBJECT_FLAGS (obj) & GTK_PROPAGATE_STATE)
#define GTK_WIDGET_DRAWABLE(obj)          (GTK_WIDGET_VISIBLE (obj) && GTK_WIDGET_MAPPED (obj))
#define GTK_WIDGET_ANCHORED(obj)          (GTK_OBJECT_FLAGS (obj) & GTK_ANCHORED)
#define GTK_WIDGET_BASIC(obj)             (GTK_OBJECT_FLAGS (obj) & GTK_BASIC)
#define GTK_WIDGET_USER_STYLE(obj)        (GTK_OBJECT_FLAGS (obj) & GTK_USER_STYLE)
#define GTK_WIDGET_HAS_GRAB(obj)          (GTK_OBJECT_FLAGS (obj) & GTK_HAS_GRAB)
#define GTK_WIDGET_REDRAW_PENDING(obj)    (GTK_OBJECT_FLAGS (obj) & GTK_REDRAW_PENDING)
#define GTK_WIDGET_RESIZE_PENDING(obj)    (GTK_OBJECT_FLAGS (obj) & GTK_RESIZE_PENDING)
#define GTK_WIDGET_RESIZE_NEEDED(obj)     (GTK_OBJECT_FLAGS (obj) & GTK_RESIZE_NEEDED)
#define GTK_WIDGET_HAS_SHAPE_MASK(obj)    (GTK_OBJECT_FLAGS (obj) & GTK_HAS_SHAPE_MASK)
#define GTK_WIDGET_LEAVE_PENDING(obj)     (GTK_OBJECT_FLAGS (obj) & GTK_LEAVE_PENDING)

#define GTK_TYPE_WIDGET                   (gtk_widget_get_type ())

/* Macro for testing whether "obj" is of type GtkWidget.
 */
#define GTK_IS_WIDGET(obj)                GTK_CHECK_TYPE (obj, GTK_TYPE_WIDGET)

/* Macros for setting and clearing widget flags. Notice
 *  that these are only wrappers for the macros which
 *  set and clear the flags in the GtkObject structure.
 */
#define GTK_WIDGET_SET_FLAGS(obj,flag)    (GTK_OBJECT_SET_FLAGS (obj, flag))
#define GTK_WIDGET_UNSET_FLAGS(obj,flag)  (GTK_OBJECT_UNSET_FLAGS (obj, flag))



typedef struct _GtkRequisition    GtkRequisition;
typedef struct _GtkAllocation     GtkAllocation;
typedef struct _GtkSelectionData GtkSelectionData;
typedef struct _GtkWidget         GtkWidget;
typedef struct _GtkWidgetClass    GtkWidgetClass;
typedef struct _GtkWidgetAuxInfo  GtkWidgetAuxInfo;
typedef struct _GtkWidgetShapeInfo GtkWidgetShapeInfo;

typedef void (*GtkCallback) (GtkWidget *widget,
			     gpointer   data);

/* A requisition is a desired amount of space which a
 *  widget may request.
 */
struct _GtkRequisition
{
  guint16 width;
  guint16 height;
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
   counting) */

struct _GtkSelectionData
{
  GdkAtom selection;
  GdkAtom target;
  GdkAtom type;
  gint    format;
  guchar *data;
  gint    length;
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
   * The widget name is used to determine the style to
   *  use for a widget.
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

  /* The signal to emit when an object of this class is activated.
   *  This is used when activating the current focus widget and
   *  the default widget.
   */
  gint activate_signal;

  /* basics */
  void (* show)                (GtkWidget      *widget);
  void (* hide)                (GtkWidget      *widget);
  void (* show_all)            (GtkWidget      *widget);
  void (* hide_all)            (GtkWidget      *widget);
  void (* map)                 (GtkWidget      *widget);
  void (* unmap)               (GtkWidget      *widget);
  void (* realize)             (GtkWidget      *widget);
  void (* unrealize)           (GtkWidget      *widget);
  void (* draw)                (GtkWidget      *widget,
				GdkRectangle   *area);
  void (* draw_focus)          (GtkWidget      *widget);
  void (* draw_default)        (GtkWidget      *widget);
  void (* size_request)        (GtkWidget      *widget,
				GtkRequisition *requisition);
  void (* size_allocate)       (GtkWidget      *widget,
				GtkAllocation  *allocation);
  void (* state_changed)       (GtkWidget      *widget,
				guint           previous_state);

  /* accelerators */
  gint (* install_accelerator) (GtkWidget      *widget,
				const gchar    *signal_name,
				gchar           key,
				guint8          modifiers);
  void (* remove_accelerator)  (GtkWidget      *widget,
				const gchar    *signal_name);

  /* events */
  gint (* event)                   (GtkWidget          *widget,
				    GdkEvent           *event);
  gint (* button_press_event)      (GtkWidget          *widget,
				    GdkEventButton     *event);
  gint (* button_release_event)    (GtkWidget          *widget,
				    GdkEventButton     *event);
  gint (* motion_notify_event)     (GtkWidget          *widget,
				    GdkEventMotion     *event);
  gint (* delete_event)            (GtkWidget          *widget,
				    GdkEventAny        *event);
  gint (* destroy_event)           (GtkWidget          *widget,
				    GdkEventAny        *event);
  gint (* expose_event)            (GtkWidget          *widget,
				    GdkEventExpose     *event);
  gint (* key_press_event)         (GtkWidget          *widget,
				    GdkEventKey        *event);
  gint (* key_release_event)       (GtkWidget          *widget,
				    GdkEventKey        *event);
  gint (* enter_notify_event)      (GtkWidget          *widget,
				    GdkEventCrossing   *event);
  gint (* leave_notify_event)      (GtkWidget          *widget,
				    GdkEventCrossing   *event);
  gint (* configure_event)         (GtkWidget          *widget,
				    GdkEventConfigure  *event);
  gint (* focus_in_event)          (GtkWidget          *widget,
				    GdkEventFocus      *event);
  gint (* focus_out_event)         (GtkWidget          *widget,
				    GdkEventFocus      *event);
  gint (* map_event)               (GtkWidget          *widget,
				    GdkEventAny        *event);
  gint (* unmap_event)             (GtkWidget          *widget,
				    GdkEventAny        *event);
  gint (* property_notify_event)   (GtkWidget          *widget,
				    GdkEventProperty   *event);
  gint (* selection_clear_event)   (GtkWidget          *widget,
				    GdkEventSelection  *event);
  gint (* selection_request_event) (GtkWidget          *widget,
				    GdkEventSelection  *event);
  gint (* selection_notify_event)  (GtkWidget          *widget,
				    GdkEventSelection  *event);
  gint (* proximity_in_event)      (GtkWidget          *widget,
				    GdkEventProximity  *event);
  gint (* proximity_out_event)     (GtkWidget          *widget,
				    GdkEventProximity  *event);
  gint (* drag_begin_event)        (GtkWidget          *widget,
				    GdkEventDragBegin  *event);
  gint (* drag_request_event)      (GtkWidget          *widget,
				    GdkEventDragRequest *event);
  gint (* drop_enter_event)        (GtkWidget          *widget,
				    GdkEventDropEnter  *event);
  gint (* drop_leave_event)        (GtkWidget          *widget,
				    GdkEventDropLeave  *event);
  gint (* drop_data_available_event)   (GtkWidget         *widget,
				    GdkEventDropDataAvailable *event);
  gint (* other_event)             (GtkWidget          *widget,
				    GdkEventOther      *event);
  gint (* visibility_notify_event) (GtkWidget          *widget,
				    GdkEventVisibility *event);

  /* selection */
  void (* selection_received)  (GtkWidget      *widget,
				GtkSelectionData *selection_data);

 gint (* client_event)		   (GtkWidget	      *widget,
				    GdkEventClient    *event);
};

struct _GtkWidgetAuxInfo
{
  gint16  x;
  gint16  y;
  guint16 width;
  guint16 height;
};

struct _GtkWidgetShapeInfo
{
  gint16     offset_x;
  gint16     offset_y;
  GdkBitmap *shape_mask;
};


guint      gtk_widget_get_type            (void);
GtkWidget* gtk_widget_new                 (guint                type,
					   ...);
GtkWidget* gtk_widget_newv                (guint                type,
					   guint                nargs,
					   GtkArg              *args);
void       gtk_widget_get                 (GtkWidget           *widget,
					   GtkArg	       *arg);
void       gtk_widget_getv                (GtkWidget           *widget,
					   guint                nargs,
					   GtkArg              *args);
void       gtk_widget_set                 (GtkWidget           *widget,
					   ...);
void       gtk_widget_setv                (GtkWidget           *widget,
					   guint                nargs,
					   GtkArg              *args);
void       gtk_widget_destroy             (GtkWidget           *widget);
void       gtk_widget_unparent            (GtkWidget           *widget);
void       gtk_widget_show                (GtkWidget           *widget);
void       gtk_widget_hide                (GtkWidget           *widget);
void       gtk_widget_show_all            (GtkWidget           *widget);
void       gtk_widget_hide_all            (GtkWidget           *widget);
void       gtk_widget_map                 (GtkWidget           *widget);
void       gtk_widget_unmap               (GtkWidget           *widget);
void       gtk_widget_realize             (GtkWidget           *widget);
void       gtk_widget_unrealize           (GtkWidget           *widget);
void       gtk_widget_queue_draw          (GtkWidget           *widget);
void       gtk_widget_queue_resize        (GtkWidget           *widget);
void       gtk_widget_draw                (GtkWidget           *widget,
					   GdkRectangle        *area);
void       gtk_widget_draw_focus          (GtkWidget           *widget);
void       gtk_widget_draw_default        (GtkWidget           *widget);
void       gtk_widget_draw_children       (GtkWidget           *widget);
void       gtk_widget_size_request        (GtkWidget           *widget,
					   GtkRequisition      *requisition);
void       gtk_widget_size_allocate       (GtkWidget           *widget,
					   GtkAllocation       *allocation);
void       gtk_widget_install_accelerator (GtkWidget           *widget,
					   GtkAcceleratorTable *table,
					   const gchar         *signal_name,
					   gchar                key,
					   guint8               modifiers);
void       gtk_widget_remove_accelerator  (GtkWidget           *widget,
					   GtkAcceleratorTable *table,
					   const gchar         *signal_name);
gint       gtk_widget_event               (GtkWidget           *widget,
					   GdkEvent            *event);

void       gtk_widget_activate            (GtkWidget           *widget);
void       gtk_widget_reparent            (GtkWidget           *widget,
					   GtkWidget           *new_parent);
void       gtk_widget_popup               (GtkWidget           *widget,
					   gint                 x,
					   gint                 y);
gint       gtk_widget_intersect           (GtkWidget           *widget,
					   GdkRectangle        *area,
					   GdkRectangle        *intersection);
gint       gtk_widget_basic               (GtkWidget           *widget);

void       gtk_widget_grab_focus          (GtkWidget           *widget);
void       gtk_widget_grab_default        (GtkWidget           *widget);

void       gtk_widget_set_name            (GtkWidget           *widget,
					   const gchar         *name);
gchar*     gtk_widget_get_name            (GtkWidget           *widget);
void       gtk_widget_set_state           (GtkWidget           *widget,
					   GtkStateType         state);
void       gtk_widget_set_sensitive       (GtkWidget           *widget,
					   gint                 sensitive);
void       gtk_widget_set_parent          (GtkWidget           *widget,
					   GtkWidget           *parent);
void       gtk_widget_set_parent_window   (GtkWidget           *widget,
					   GdkWindow           *parent_window);
GdkWindow *gtk_widget_get_parent_window   (GtkWidget           *widget);
void       gtk_widget_set_style           (GtkWidget           *widget,
					   GtkStyle            *style);
void       gtk_widget_set_uposition       (GtkWidget           *widget,
					   gint                 x,
					   gint                 y);
void       gtk_widget_set_usize           (GtkWidget           *widget,
					   gint                 width,
					   gint                 height);
void       gtk_widget_set_events          (GtkWidget           *widget,
					   gint                 events);
void       gtk_widget_set_extension_events (GtkWidget           *widget,
					    GdkExtensionMode    mode);

GtkWidget*   gtk_widget_get_toplevel  (GtkWidget *widget);
GtkWidget*   gtk_widget_get_ancestor  (GtkWidget *widget,
				       gint       type);
GdkColormap* gtk_widget_get_colormap  (GtkWidget *widget);
GdkVisual*   gtk_widget_get_visual    (GtkWidget *widget);
GtkStyle*    gtk_widget_get_style     (GtkWidget *widget);
gint         gtk_widget_get_events    (GtkWidget *widget);
GdkExtensionMode gtk_widget_get_extension_events    (GtkWidget *widget);
void         gtk_widget_get_pointer   (GtkWidget *widget,
				       gint      *x,
				       gint      *y);

gint         gtk_widget_is_ancestor (GtkWidget *widget,
				     GtkWidget *ancestor);
gint         gtk_widget_is_child    (GtkWidget *widget,
				     GtkWidget *child);

void         gtk_widget_push_colormap    (GdkColormap   *cmap);
void         gtk_widget_push_visual      (GdkVisual     *visual);
void         gtk_widget_push_style       (GtkStyle      *style);

void         gtk_widget_pop_colormap     (void);
void         gtk_widget_pop_visual       (void);
void         gtk_widget_pop_style        (void);

void         gtk_widget_set_default_colormap (GdkColormap *colormap);
void         gtk_widget_set_default_visual   (GdkVisual   *visual);
void         gtk_widget_set_default_style    (GtkStyle    *style);
/* Tells other Gtk applications to use the same default style */
void         gtk_widget_propagate_default_style(void);
GdkColormap* gtk_widget_get_default_colormap (void);
GdkVisual*   gtk_widget_get_default_visual   (void);
GtkStyle*    gtk_widget_get_default_style    (void);

/* 
 * see gdk_window_shape_combine_mask
 */
void         gtk_widget_shape_combine_mask (GtkWidget *widget,
					    GdkBitmap *shape_mask,
					    gint       offset_x,
					    gint       offset_y);

/* 
 * When you get a drag_enter event, you can use this to tell Gtk of other
 *  items that are to be dragged as well...
 */
void         gtk_widget_dnd_drag_add (GtkWidget *widget);

/* 
 * These two functions enable drag and/or drop on a widget,
 * and also let Gtk know what data types will be accepted (use MIME type
 *  naming, plus tacking "URL:" on the front for link dragging)
 */
void         gtk_widget_dnd_drag_set (GtkWidget     *widget,
				      guint8         drag_enable,
				      gchar        **type_accept_list,
				      guint          numtypes);
void         gtk_widget_dnd_drop_set (GtkWidget     *widget,
				      guint8         drop_enable,
				      gchar        **type_accept_list,
				      guint          numtypes,
				      guint8         is_destructive_operation);

/* 
 * used to reply to a DRAG_REQUEST event - if you don't want to 
 * give the data then pass in NULL for it 
 */
void         gtk_widget_dnd_data_set (GtkWidget     *widget,
				      GdkEvent      *event,
				      gpointer       data,
				      gulong         data_numbytes);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_WIDGET_H__ */
