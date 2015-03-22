/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
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

#include "config.h"

#include <stdlib.h>

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include "gtkhandlebox.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkwindow.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkhandlebox
 * @Short_description: a widget for detachable window portions
 * @Title: GtkHandleBox
 *
 * The #GtkHandleBox widget allows a portion of a window to be "torn
 * off". It is a bin widget which displays its child and a handle that
 * the user can drag to tear off a separate window (the “float
 * window”) containing the child widget. A thin
 * “ghost” is drawn in the original location of the
 * handlebox. By dragging the separate window back to its original
 * location, it can be reattached.
 *
 * When reattaching, the ghost and float window, must be aligned
 * along one of the edges, the “snap edge”.
 * This either can be specified by the application programmer
 * explicitly, or GTK+ will pick a reasonable default based
 * on the handle position.
 *
 * To make detaching and reattaching the handlebox as minimally confusing
 * as possible to the user, it is important to set the snap edge so that
 * the snap edge does not move when the handlebox is deattached. For
 * instance, if the handlebox is packed at the bottom of a VBox, then
 * when the handlebox is detached, the bottom edge of the handlebox's
 * allocation will remain fixed as the height of the handlebox shrinks,
 * so the snap edge should be set to %GTK_POS_BOTTOM.
 *
 * > #GtkHandleBox has been deprecated. It is very specialized, lacks features
 * > to make it useful and most importantly does not fit well into modern
 * > application design. Do not use it. There is no replacement.
 */


struct _GtkHandleBoxPrivate
{
  /* Properties */
  GtkPositionType handle_position;
  gint snap_edge;
  GtkShadowType   shadow_type;
  gboolean        child_detached;
  /* Properties */

  GtkAllocation   attach_allocation;
  GtkAllocation   float_allocation;

  GdkDevice      *grab_device;

  GdkWindow      *bin_window;     /* parent window for children */
  GdkWindow      *float_window;

  /* Variables used during a drag
   */
  gint            orig_x;
  gint            orig_y;

  guint           float_window_mapped : 1;
  guint           in_drag : 1;
  guint           shrink_on_detach : 1;
};

enum {
  PROP_0,
  PROP_SHADOW_TYPE,
  PROP_HANDLE_POSITION,
  PROP_SNAP_EDGE,
  PROP_SNAP_EDGE_SET,
  PROP_CHILD_DETACHED
};

#define DRAG_HANDLE_SIZE 10
#define CHILDLESS_SIZE	25
#define GHOST_HEIGHT 3
#define TOLERANCE 5

enum {
  SIGNAL_CHILD_ATTACHED,
  SIGNAL_CHILD_DETACHED,
  SIGNAL_LAST
};

/* The algorithm for docking and redocking implemented here
 * has a couple of nice properties:
 *
 * 1) During a single drag, docking always occurs at the
 *    the same cursor position. This means that the users
 *    motions are reversible, and that you won't
 *    undock/dock oscillations.
 *
 * 2) Docking generally occurs at user-visible features.
 *    The user, once they figure out to redock, will
 *    have useful information about doing it again in
 *    the future.
 *
 * Please try to preserve these properties if you
 * change the algorithm. (And the current algorithm
 * is far from ideal). Briefly, the current algorithm
 * for deciding whether the handlebox is docked or not:
 *
 * 1) The decision is done by comparing two rectangles - the
 *    allocation if the widget at the start of the drag,
 *    and the boundary of hb->bin_window at the start of
 *    of the drag offset by the distance that the cursor
 *    has moved.
 *
 * 2) These rectangles must have one edge, the “snap_edge”
 *    of the handlebox, aligned within TOLERANCE.
 * 
 * 3) On the other dimension, the extents of one rectangle
 *    must be contained in the extents of the other,
 *    extended by tolerance. That is, either we can have:
 *
 * <-TOLERANCE-|--------bin_window--------------|-TOLERANCE->
 *         <--------float_window-------------------->
 *
 * or we can have:
 *
 * <-TOLERANCE-|------float_window--------------|-TOLERANCE->
 *          <--------bin_window-------------------->
 */

static void     gtk_handle_box_set_property  (GObject        *object,
                                              guint           param_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void     gtk_handle_box_get_property  (GObject        *object,
                                              guint           param_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);
static void     gtk_handle_box_map           (GtkWidget      *widget);
static void     gtk_handle_box_unmap         (GtkWidget      *widget);
static void     gtk_handle_box_realize       (GtkWidget      *widget);
static void     gtk_handle_box_unrealize     (GtkWidget      *widget);
static void     gtk_handle_box_style_updated (GtkWidget      *widget);
static void     gtk_handle_box_size_request  (GtkWidget      *widget,
                                              GtkRequisition *requisition);
static void     gtk_handle_box_get_preferred_width (GtkWidget *widget,
						    gint      *minimum,
						    gint      *natural);
static void     gtk_handle_box_get_preferred_height (GtkWidget *widget,
						    gint      *minimum,
						    gint      *natural);
static void     gtk_handle_box_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *real_allocation);
static void     gtk_handle_box_add           (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_remove        (GtkContainer   *container,
                                              GtkWidget      *widget);
static gboolean gtk_handle_box_draw          (GtkWidget      *widget,
                                              cairo_t        *cr);
static gboolean gtk_handle_box_button_press  (GtkWidget      *widget,
                                              GdkEventButton *event);
static gboolean gtk_handle_box_motion        (GtkWidget      *widget,
                                              GdkEventMotion *event);
static gboolean gtk_handle_box_delete_event  (GtkWidget      *widget,
                                              GdkEventAny    *event);
static void     gtk_handle_box_reattach      (GtkHandleBox   *hb);
static void     gtk_handle_box_end_drag      (GtkHandleBox   *hb,
                                              guint32         time);

static guint handle_box_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkHandleBox, gtk_handle_box, GTK_TYPE_BIN)

static void
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  gobject_class->set_property = gtk_handle_box_set_property;
  gobject_class->get_property = gtk_handle_box_get_property;
  
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Shadow type"),
                                                      P_("Appearance of the shadow that surrounds the container"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_HANDLE_POSITION,
                                   g_param_spec_enum ("handle-position",
                                                      P_("Handle position"),
                                                      P_("Position of the handle relative to the child widget"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_LEFT,
                                                      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_EDGE,
                                   g_param_spec_enum ("snap-edge",
                                                      P_("Snap edge"),
                                                      P_("Side of the handlebox that's lined up with the docking point to dock the handlebox"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_TOP,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_EDGE_SET,
                                   g_param_spec_boolean ("snap-edge-set",
							 P_("Snap edge set"),
							 P_("Whether to use the value from the snap_edge property or a value derived from handle_position"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CHILD_DETACHED,
                                   g_param_spec_boolean ("child-detached",
							 P_("Child Detached"),
							 P_("A boolean value indicating whether the handlebox's child is attached or detached."),
							 FALSE,
							 GTK_PARAM_READABLE));

  widget_class->map = gtk_handle_box_map;
  widget_class->unmap = gtk_handle_box_unmap;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->unrealize = gtk_handle_box_unrealize;
  widget_class->style_updated = gtk_handle_box_style_updated;
  widget_class->get_preferred_width = gtk_handle_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_handle_box_get_preferred_height;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->draw = gtk_handle_box_draw;
  widget_class->button_press_event = gtk_handle_box_button_press;
  widget_class->delete_event = gtk_handle_box_delete_event;

  container_class->add = gtk_handle_box_add;
  container_class->remove = gtk_handle_box_remove;

  class->child_attached = NULL;
  class->child_detached = NULL;

  /**
   * GtkHandleBox::child-attached:
   * @handlebox: the object which received the signal.
   * @widget: the child widget of the handlebox.
   *   (this argument provides no extra information
   *   and is here only for backwards-compatibility)
   *
   * This signal is emitted when the contents of the
   * handlebox are reattached to the main window.
   *
   * Deprecated: 3.4: #GtkHandleBox has been deprecated.
   */
  handle_box_signals[SIGNAL_CHILD_ATTACHED] =
    g_signal_new (I_("child-attached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_attached),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  /**
   * GtkHandleBox::child-detached:
   * @handlebox: the object which received the signal.
   * @widget: the child widget of the handlebox.
   *   (this argument provides no extra information
   *   and is here only for backwards-compatibility)
   *
   * This signal is emitted when the contents of the
   * handlebox are detached from the main window.
   *
   * Deprecated: 3.4: #GtkHandleBox has been deprecated.
   */
  handle_box_signals[SIGNAL_CHILD_DETACHED] =
    g_signal_new (I_("child-detached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_detached),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  GtkHandleBoxPrivate *priv;
  GtkStyleContext *context;

  handle_box->priv = gtk_handle_box_get_instance_private (handle_box);
  priv = handle_box->priv;

  gtk_widget_set_has_window (GTK_WIDGET (handle_box), TRUE);

  priv->bin_window = NULL;
  priv->float_window = NULL;
  priv->shadow_type = GTK_SHADOW_OUT;
  priv->handle_position = GTK_POS_LEFT;
  priv->float_window_mapped = FALSE;
  priv->child_detached = FALSE;
  priv->in_drag = FALSE;
  priv->shrink_on_detach = TRUE;
  priv->snap_edge = -1;

  context = gtk_widget_get_style_context (GTK_WIDGET (handle_box));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_DOCK);
}

static void 
gtk_handle_box_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);

  switch (prop_id)
    {
    case PROP_SHADOW_TYPE:
      gtk_handle_box_set_shadow_type (handle_box, g_value_get_enum (value));
      break;
    case PROP_HANDLE_POSITION:
      gtk_handle_box_set_handle_position (handle_box, g_value_get_enum (value));
      break;
    case PROP_SNAP_EDGE:
      gtk_handle_box_set_snap_edge (handle_box, g_value_get_enum (value));
      break;
    case PROP_SNAP_EDGE_SET:
      if (!g_value_get_boolean (value))
	gtk_handle_box_set_snap_edge (handle_box, (GtkPositionType)-1);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_handle_box_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);
  GtkHandleBoxPrivate *priv = handle_box->priv;

  switch (prop_id)
    {
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;
    case PROP_HANDLE_POSITION:
      g_value_set_enum (value, priv->handle_position);
      break;
    case PROP_SNAP_EDGE:
      g_value_set_enum (value,
			(priv->snap_edge == -1 ?
			 GTK_POS_TOP : priv->snap_edge));
      break;
    case PROP_SNAP_EDGE_SET:
      g_value_set_boolean (value, priv->snap_edge != -1);
      break;
    case PROP_CHILD_DETACHED:
      g_value_set_boolean (value, priv->child_detached);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_handle_box_new:
 *
 * Create a new handle box.
 *
 * Returns: a new #GtkHandleBox.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 */
GtkWidget*
gtk_handle_box_new (void)
{
  return g_object_new (GTK_TYPE_HANDLE_BOX, NULL);
}

static void
gtk_handle_box_map (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  gtk_widget_set_mapped (widget, TRUE);

  child = gtk_bin_get_child (bin);
  if (child != NULL &&
      gtk_widget_get_visible (child) &&
      !gtk_widget_get_mapped (child))
    gtk_widget_map (child);

  if (priv->child_detached && !priv->float_window_mapped)
    {
      gdk_window_show (priv->float_window);
      priv->float_window_mapped = TRUE;
    }

  gdk_window_show (priv->bin_window);
  gdk_window_show (gtk_widget_get_window (widget));
}

static void
gtk_handle_box_unmap (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  gtk_widget_set_mapped (widget, FALSE);

  gdk_window_hide (gtk_widget_get_window (widget));
  if (priv->float_window_mapped)
    {
      gdk_window_hide (priv->float_window);
      priv->float_window_mapped = FALSE;
    }

  GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->unmap (widget);
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkAllocation allocation;
  GtkRequisition requisition;
  GtkStyleContext *context;
  GtkWidget *child;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = (gtk_widget_get_events (widget)
                           | GDK_EXPOSURE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = (gtk_widget_get_events (widget)
                           | GDK_EXPOSURE_MASK
                           | GDK_BUTTON1_MOTION_MASK
                           | GDK_POINTER_MOTION_HINT_MASK
                           | GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  priv->bin_window = gdk_window_new (window,
                                     &attributes, attributes_mask);
  gdk_window_set_user_data (priv->bin_window, widget);

  child = gtk_bin_get_child (GTK_BIN (hb));
  if (child)
    gtk_widget_set_parent_window (child, priv->bin_window);

  gtk_widget_get_preferred_size (widget, &requisition, NULL);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = requisition.width;
  attributes.height = requisition.height;
  attributes.window_type = GDK_WINDOW_TOPLEVEL;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = (gtk_widget_get_events (widget)
                           | GDK_KEY_PRESS_MASK
                           | GDK_ENTER_NOTIFY_MASK
                           | GDK_LEAVE_NOTIFY_MASK
                           | GDK_FOCUS_CHANGE_MASK
                           | GDK_STRUCTURE_MASK);
  attributes.type_hint = GDK_WINDOW_TYPE_HINT_TOOLBAR;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_TYPE_HINT;
  priv->float_window = gdk_window_new (gdk_screen_get_root_window (gtk_widget_get_screen (widget)),
                                       &attributes, attributes_mask);
  gdk_window_set_user_data (priv->float_window, widget);
  gdk_window_set_decorations (priv->float_window, 0);
  gdk_window_set_type_hint (priv->float_window, GDK_WINDOW_TYPE_HINT_TOOLBAR);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, window);
  gtk_style_context_set_background (context, priv->bin_window);
  gtk_style_context_set_background (context, priv->float_window);
}

static void
gtk_handle_box_unrealize (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  gdk_window_set_user_data (priv->bin_window, NULL);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;
  gdk_window_set_user_data (priv->float_window, NULL);
  gdk_window_destroy (priv->float_window);
  priv->float_window = NULL;

  GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->unrealize (widget);
}

static void
gtk_handle_box_style_updated (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->style_updated (widget);

  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
    {
      GtkStateFlags state;
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);

      gtk_style_context_save (context);
      gtk_style_context_set_state (context, state);

      gtk_style_context_set_background (context, gtk_widget_get_window (widget));
      gtk_style_context_set_background (context, priv->bin_window);
      gtk_style_context_set_background (context, priv->float_window);

      gtk_style_context_restore (context);
    }
}

static int
effective_handle_position (GtkHandleBox *hb)
{
  GtkHandleBoxPrivate *priv = hb->priv;
  int handle_position;

  if (gtk_widget_get_direction (GTK_WIDGET (hb)) == GTK_TEXT_DIR_LTR)
    handle_position = priv->handle_position;
  else
    {
      switch (priv->handle_position)
	{
	case GTK_POS_LEFT:
	  handle_position = GTK_POS_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  handle_position = GTK_POS_LEFT;
	  break;
	default:
	  handle_position = priv->handle_position;
	  break;
	}
    }

  return handle_position;
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkRequisition child_requisition;
  GtkWidget *child;
  gint handle_position;

  handle_position = effective_handle_position (hb);

  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      requisition->width = DRAG_HANDLE_SIZE;
      requisition->height = 0;
    }
  else
    {
      requisition->width = 0;
      requisition->height = DRAG_HANDLE_SIZE;
    }

  child = gtk_bin_get_child (bin);
  /* if our child is not visible, we still request its size, since we
   * won't have any useful hint for our size otherwise.
   */
  if (child)
    {
      gtk_widget_get_preferred_size (child, &child_requisition, NULL);
    }
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      

  if (priv->child_detached)
    {
      /* FIXME: This doesn't work currently */
      if (!priv->shrink_on_detach)
	{
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += child_requisition.height;
	  else
	    requisition->width += child_requisition.width;
	}
      else
	{
          GtkStyleContext *context;
          GtkStateFlags state;
          GtkBorder padding;

          context = gtk_widget_get_style_context (widget);
          state = gtk_widget_get_state_flags (widget);
          gtk_style_context_get_padding (context, state, &padding);

	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += padding.top;
	  else
	    requisition->width += padding.left;
	}
    }
  else
    {
      guint border_width;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      requisition->width += border_width * 2;
      requisition->height += border_width * 2;
      
      if (child)
	{
	  requisition->width += child_requisition.width;
	  requisition->height += child_requisition.height;
	}
      else
	{
	  requisition->width += CHILDLESS_SIZE;
	  requisition->height += CHILDLESS_SIZE;
	}
    }
}

static void
gtk_handle_box_get_preferred_width (GtkWidget *widget,
				     gint      *minimum,
				     gint      *natural)
{
  GtkRequisition requisition;

  gtk_handle_box_size_request (widget, &requisition);

  *minimum = *natural = requisition.width;
}

static void
gtk_handle_box_get_preferred_height (GtkWidget *widget,
				     gint      *minimum,
				     gint      *natural)
{
  GtkRequisition requisition;

  gtk_handle_box_size_request (widget, &requisition);

  *minimum = *natural = requisition.height;
}


static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkRequisition child_requisition;
  GtkWidget *child;
  gint handle_position;

  handle_position = effective_handle_position (hb);

  child = gtk_bin_get_child (bin);

  if (child)
    {
      gtk_widget_get_preferred_size (child, &child_requisition, NULL);
    }
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  if (child != NULL && gtk_widget_get_visible (child))
    {
      GtkAllocation child_allocation;
      guint border_width;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

      child_allocation.x = border_width;
      child_allocation.y = border_width;
      if (handle_position == GTK_POS_LEFT)
	child_allocation.x += DRAG_HANDLE_SIZE;
      else if (handle_position == GTK_POS_TOP)
	child_allocation.y += DRAG_HANDLE_SIZE;

      if (priv->child_detached)
	{
	  guint float_width;
	  guint float_height;
	  
	  child_allocation.width = child_requisition.width;
	  child_allocation.height = child_requisition.height;
	  
	  float_width = child_allocation.width + 2 * border_width;
	  float_height = child_allocation.height + 2 * border_width;
	  
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    float_width += DRAG_HANDLE_SIZE;
	  else
	    float_height += DRAG_HANDLE_SIZE;

	  if (gtk_widget_get_realized (widget))
	    {
	      gdk_window_resize (priv->float_window,
				 float_width,
				 float_height);
	      gdk_window_move_resize (priv->bin_window,
				      0,
				      0,
				      float_width,
				      float_height);
	    }
	}
      else
	{
	  child_allocation.width = MAX (1, (gint) allocation->width - 2 * border_width);
	  child_allocation.height = MAX (1, (gint) allocation->height - 2 * border_width);

	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    child_allocation.width -= DRAG_HANDLE_SIZE;
	  else
	    child_allocation.height -= DRAG_HANDLE_SIZE;
	  
	  if (gtk_widget_get_realized (widget))
	    gdk_window_move_resize (priv->bin_window,
				    0,
				    0,
				    allocation->width,
				    allocation->height);
	}

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
gtk_handle_box_draw_ghost (GtkHandleBox *hb,
                           cairo_t      *cr)
{
  GtkWidget *widget = GTK_WIDGET (hb);
  GtkStateFlags state;
  GtkStyleContext *context;
  guint x;
  guint y;
  guint width;
  guint height;
  gint allocation_width;
  gint allocation_height;
  gint handle_position;

  handle_position = effective_handle_position (hb);
  allocation_width = gtk_widget_get_allocated_width (widget);
  allocation_height = gtk_widget_get_allocated_height (widget);

  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      x = handle_position == GTK_POS_LEFT ? 0 : allocation_width - DRAG_HANDLE_SIZE;
      y = 0;
      width = DRAG_HANDLE_SIZE;
      height = allocation_height;
    }
  else
    {
      x = 0;
      y = handle_position == GTK_POS_TOP ? 0 : allocation_height - DRAG_HANDLE_SIZE;
      width = allocation_width;
      height = DRAG_HANDLE_SIZE;
    }

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    gtk_render_line (context, cr,
                     handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
                     allocation_height / 2,
                     handle_position == GTK_POS_LEFT ? allocation_width : allocation_width - DRAG_HANDLE_SIZE,
                     allocation_height / 2);
  else
    gtk_render_line (context, cr,
                     allocation_width / 2,
                     handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
                     allocation_width / 2,
                     handle_position == GTK_POS_TOP ? allocation_height : allocation_height - DRAG_HANDLE_SIZE);

  gtk_style_context_restore (context);
}

/**
 * gtk_handle_box_set_shadow_type:
 * @handle_box: a #GtkHandleBox
 * @type: the shadow type.
 *
 * Sets the type of shadow to be drawn around the border
 * of the handle box.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 */
void
gtk_handle_box_set_shadow_type (GtkHandleBox  *handle_box,
				GtkShadowType  type)
{
  GtkHandleBoxPrivate *priv;

  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  priv = handle_box->priv;

  if ((GtkShadowType) priv->shadow_type != type)
    {
      priv->shadow_type = type;
      g_object_notify (G_OBJECT (handle_box), "shadow-type");
      gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

/**
 * gtk_handle_box_get_shadow_type:
 * @handle_box: a #GtkHandleBox
 *
 * Gets the type of shadow drawn around the handle box. See
 * gtk_handle_box_set_shadow_type().
 *
 * Returns: the type of shadow currently drawn around the handle box.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 **/
GtkShadowType
gtk_handle_box_get_shadow_type (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_SHADOW_ETCHED_OUT);

  return handle_box->priv->shadow_type;
}

/**
 * gtk_handle_box_set_handle_position:
 * @handle_box: a #GtkHandleBox
 * @position: the side of the handlebox where the handle should be drawn.
 *
 * Sets the side of the handlebox where the handle is drawn.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 */
void        
gtk_handle_box_set_handle_position  (GtkHandleBox    *handle_box,
				     GtkPositionType  position)
{
  GtkHandleBoxPrivate *priv;

  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  priv = handle_box->priv;

  if ((GtkPositionType) priv->handle_position != position)
    {
      priv->handle_position = position;
      g_object_notify (G_OBJECT (handle_box), "handle-position");
      gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

/**
 * gtk_handle_box_get_handle_position:
 * @handle_box: a #GtkHandleBox
 *
 * Gets the handle position of the handle box. See
 * gtk_handle_box_set_handle_position().
 *
 * Returns: the current handle position.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 **/
GtkPositionType
gtk_handle_box_get_handle_position (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_POS_LEFT);

  return handle_box->priv->handle_position;
}

/**
 * gtk_handle_box_set_snap_edge:
 * @handle_box: a #GtkHandleBox
 * @edge: the snap edge, or -1 to unset the value; in which
 *   case GTK+ will try to guess an appropriate value
 *   in the future.
 *
 * Sets the snap edge of a handlebox. The snap edge is
 * the edge of the detached child that must be aligned
 * with the corresponding edge of the “ghost” left
 * behind when the child was detached to reattach
 * the torn-off window. Usually, the snap edge should
 * be chosen so that it stays in the same place on
 * the screen when the handlebox is torn off.
 *
 * If the snap edge is not set, then an appropriate value
 * will be guessed from the handle position. If the
 * handle position is %GTK_POS_RIGHT or %GTK_POS_LEFT,
 * then the snap edge will be %GTK_POS_TOP, otherwise
 * it will be %GTK_POS_LEFT.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 */
void
gtk_handle_box_set_snap_edge        (GtkHandleBox    *handle_box,
				     GtkPositionType  edge)
{
  GtkHandleBoxPrivate *priv;

  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  priv = handle_box->priv;

  if (priv->snap_edge != edge)
    {
      priv->snap_edge = edge;

      g_object_freeze_notify (G_OBJECT (handle_box));
      g_object_notify (G_OBJECT (handle_box), "snap-edge");
      g_object_notify (G_OBJECT (handle_box), "snap-edge-set");
      g_object_thaw_notify (G_OBJECT (handle_box));
    }
}

/**
 * gtk_handle_box_get_snap_edge:
 * @handle_box: a #GtkHandleBox
 *
 * Gets the edge used for determining reattachment of the handle box.
 * See gtk_handle_box_set_snap_edge().
 *
 * Returns: the edge used for determining reattachment, or
 *   (GtkPositionType)-1 if this is determined (as per default)
 *   from the handle position.
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 **/
GtkPositionType
gtk_handle_box_get_snap_edge (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), (GtkPositionType)-1);

  return (GtkPositionType)handle_box->priv->snap_edge;
}

/**
 * gtk_handle_box_get_child_detached:
 * @handle_box: a #GtkHandleBox
 *
 * Whether the handlebox’s child is currently detached.
 *
 * Returns: %TRUE if the child is currently detached, otherwise %FALSE
 *
 * Since: 2.14
 *
 * Deprecated: 3.4: #GtkHandleBox has been deprecated.
 **/
gboolean
gtk_handle_box_get_child_detached (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), FALSE);

  return handle_box->priv->child_detached;
}

static void
gtk_handle_box_paint (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkWidget *child;
  gint width, height;
  GdkRectangle rect;
  gint handle_position;

  handle_position = effective_handle_position (hb);

  width = gdk_window_get_width (priv->bin_window);
  height = gdk_window_get_height (priv->bin_window);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  switch (handle_position)
    {
    case GTK_POS_LEFT:
      rect.x = 0;
      rect.y = 0;
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      break;
    case GTK_POS_RIGHT:
      rect.x = width - DRAG_HANDLE_SIZE;
      rect.y = 0;
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      break;
    case GTK_POS_TOP:
      rect.x = 0;
      rect.y = 0;
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      break;
    case GTK_POS_BOTTOM:
      rect.x = 0;
      rect.y = height - DRAG_HANDLE_SIZE;
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  gtk_render_handle (context, cr,
                     rect.x, rect.y, rect.width, rect.height);

  child = gtk_bin_get_child (bin);
  if (child != NULL && gtk_widget_get_visible (child))
    GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->draw (widget, cr);

  gtk_style_context_restore (context);
}

static gboolean
gtk_handle_box_draw (GtkWidget *widget,
		     cairo_t   *cr)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    {
      if (priv->child_detached)
        gtk_handle_box_draw_ghost (hb, cr);
    }
  else if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    gtk_handle_box_paint (widget, cr);
  
  return FALSE;
}

static GtkWidget *
gtk_handle_box_get_invisible (void)
{
  static GtkWidget *handle_box_invisible = NULL;

  if (!handle_box_invisible)
    {
      handle_box_invisible = gtk_invisible_new ();
      gtk_widget_show (handle_box_invisible);
    }
  
  return handle_box_invisible;
}

static gboolean
gtk_handle_box_grab_event (GtkWidget    *widget,
			   GdkEvent     *event,
			   GtkHandleBox *hb)
{
  GtkHandleBoxPrivate *priv = hb->priv;

  switch (event->type)
    {
    case GDK_BUTTON_RELEASE:
      if (priv->in_drag)		/* sanity check */
	{
	  gtk_handle_box_end_drag (hb, event->button.time);
	  return TRUE;
	}
      break;

    case GDK_MOTION_NOTIFY:
      return gtk_handle_box_motion (GTK_WIDGET (hb), (GdkEventMotion *)event);
      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
gtk_handle_box_button_press (GtkWidget      *widget,
                             GdkEventButton *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  gboolean event_handled;
  GdkCursor *fleur;
  gint handle_position;

  handle_position = effective_handle_position (hb);

  event_handled = FALSE;
  if ((event->button == GDK_BUTTON_PRIMARY) &&
      (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS))
    {
      GtkWidget *child;
      gboolean in_handle;
      
      if (event->window != priv->bin_window)
	return FALSE;

      child = gtk_bin_get_child (GTK_BIN (hb));

      if (child)
	{
          GtkAllocation child_allocation;
          guint border_width;

          gtk_widget_get_allocation (child, &child_allocation);
          border_width = gtk_container_get_border_width (GTK_CONTAINER (hb));

	  switch (handle_position)
	    {
	    case GTK_POS_LEFT:
	      in_handle = event->x < DRAG_HANDLE_SIZE;
	      break;
	    case GTK_POS_TOP:
	      in_handle = event->y < DRAG_HANDLE_SIZE;
	      break;
	    case GTK_POS_RIGHT:
	      in_handle = event->x > 2 * border_width + child_allocation.width;
	      break;
	    case GTK_POS_BOTTOM:
	      in_handle = event->y > 2 * border_width + child_allocation.height;
	      break;
	    default:
	      in_handle = FALSE;
	      break;
	    }
	}
      else
	{
	  in_handle = FALSE;
	  event_handled = TRUE;
	}
      
      if (in_handle)
	{
	  if (event->type == GDK_BUTTON_PRESS) /* Start a drag */
	    {
	      GtkWidget *invisible = gtk_handle_box_get_invisible ();
              GdkWindow *window;
	      gint root_x, root_y;

              gtk_invisible_set_screen (GTK_INVISIBLE (invisible),
                                        gtk_widget_get_screen (GTK_WIDGET (hb)));
	      gdk_window_get_origin (priv->bin_window, &root_x, &root_y);

	      priv->orig_x = event->x_root;
	      priv->orig_y = event->y_root;

	      priv->float_allocation.x = root_x - event->x_root;
	      priv->float_allocation.y = root_y - event->y_root;
	      priv->float_allocation.width = gdk_window_get_width (priv->bin_window);
	      priv->float_allocation.height = gdk_window_get_height (priv->bin_window);

              window = gtk_widget_get_window (widget);
	      if (gdk_window_is_viewable (window))
		{
		  gdk_window_get_origin (window, &root_x, &root_y);

		  priv->attach_allocation.x = root_x;
		  priv->attach_allocation.y = root_y;
		  priv->attach_allocation.width = gdk_window_get_width (window);
		  priv->attach_allocation.height = gdk_window_get_height (window);
		}
	      else
		{
		  priv->attach_allocation.x = -1;
		  priv->attach_allocation.y = -1;
		  priv->attach_allocation.width = 0;
		  priv->attach_allocation.height = 0;
		}
	      priv->in_drag = TRUE;
              priv->grab_device = event->device;
	      fleur = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						  GDK_FLEUR);
	      if (gdk_device_grab (event->device,
                                   gtk_widget_get_window (invisible),
                                   GDK_OWNERSHIP_WINDOW,
                                   FALSE,
                                   (GDK_BUTTON1_MOTION_MASK |
                                    GDK_POINTER_MOTION_HINT_MASK |
                                    GDK_BUTTON_RELEASE_MASK),
                                   fleur,
                                   event->time) != GDK_GRAB_SUCCESS)
		{
		  priv->in_drag = FALSE;
                  priv->grab_device = NULL;
		}
	      else
		{
                  gtk_device_grab_add (invisible, priv->grab_device, TRUE);
		  g_signal_connect (invisible, "event",
				    G_CALLBACK (gtk_handle_box_grab_event), hb);
		}

	      g_object_unref (fleur);
	      event_handled = TRUE;
	    }
	  else if (priv->child_detached) /* Double click */
	    {
	      gtk_handle_box_reattach (hb);
	    }
	}
    }
  
  return event_handled;
}

static gboolean
gtk_handle_box_motion (GtkWidget      *widget,
		       GdkEventMotion *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkWidget *child;
  gint new_x, new_y;
  gint snap_edge;
  gboolean is_snapped = FALSE;
  gint handle_position;
  GdkGeometry geometry;
  GdkScreen *screen, *pointer_screen;

  if (!priv->in_drag)
    return FALSE;
  handle_position = effective_handle_position (hb);

  /* Calculate the attachment point on the float, if the float
   * were detached
   */
  new_x = 0;
  new_y = 0;
  screen = gtk_widget_get_screen (widget);
  gdk_device_get_position (event->device,
                           &pointer_screen,
                           &new_x, &new_y);
  if (pointer_screen != screen)
    {
      new_x = priv->orig_x;
      new_y = priv->orig_y;
    }

  new_x += priv->float_allocation.x;
  new_y += priv->float_allocation.y;

  snap_edge = priv->snap_edge;
  if (snap_edge == -1)
    snap_edge = (handle_position == GTK_POS_LEFT ||
		 handle_position == GTK_POS_RIGHT) ?
      GTK_POS_TOP : GTK_POS_LEFT;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
    switch (snap_edge) 
      {
      case GTK_POS_LEFT:
	snap_edge = GTK_POS_RIGHT;
	break;
      case GTK_POS_RIGHT:
	snap_edge = GTK_POS_LEFT;
	break;
      default:
	break;
      }

  /* First, check if the snapped edge is aligned
   */
  switch (snap_edge)
    {
    case GTK_POS_TOP:
      is_snapped = abs (priv->attach_allocation.y - new_y) < TOLERANCE;
      break;
    case GTK_POS_BOTTOM:
      is_snapped = abs (priv->attach_allocation.y + (gint)priv->attach_allocation.height -
			new_y - (gint)priv->float_allocation.height) < TOLERANCE;
      break;
    case GTK_POS_LEFT:
      is_snapped = abs (priv->attach_allocation.x - new_x) < TOLERANCE;
      break;
    case GTK_POS_RIGHT:
      is_snapped = abs (priv->attach_allocation.x + (gint)priv->attach_allocation.width -
			new_x - (gint)priv->float_allocation.width) < TOLERANCE;
      break;
    }

  /* Next, check if coordinates in the other direction are sufficiently
   * aligned
   */
  if (is_snapped)
    {
      gint float_pos1 = 0;	/* Initialize to suppress warnings */
      gint float_pos2 = 0;
      gint attach_pos1 = 0;
      gint attach_pos2 = 0;
      
      switch (snap_edge)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  attach_pos1 = priv->attach_allocation.x;
	  attach_pos2 = priv->attach_allocation.x + priv->attach_allocation.width;
	  float_pos1 = new_x;
	  float_pos2 = new_x + priv->float_allocation.width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  attach_pos1 = priv->attach_allocation.y;
	  attach_pos2 = priv->attach_allocation.y + priv->attach_allocation.height;
	  float_pos1 = new_y;
	  float_pos2 = new_y + priv->float_allocation.height;
	  break;
	}

      is_snapped = ((attach_pos1 - TOLERANCE < float_pos1) && 
		    (attach_pos2 + TOLERANCE > float_pos2)) ||
	           ((float_pos1 - TOLERANCE < attach_pos1) &&
		    (float_pos2 + TOLERANCE > attach_pos2));
    }

  child = gtk_bin_get_child (GTK_BIN (hb));

  if (is_snapped)
    {
      if (priv->child_detached)
	{
	  priv->child_detached = FALSE;
	  gdk_window_hide (priv->float_window);
	  gdk_window_reparent (priv->bin_window, gtk_widget_get_window (widget), 0, 0);
	  priv->float_window_mapped = FALSE;
	  g_signal_emit (hb,
			 handle_box_signals[SIGNAL_CHILD_ATTACHED],
			 0,
			 child);
	  
	  gtk_widget_queue_resize (widget);
	}
    }
  else
    {
      gint width, height;

      width = gdk_window_get_width (priv->float_window);
      height = gdk_window_get_height (priv->float_window);

      switch (handle_position)
	{
	case GTK_POS_LEFT:
	  new_y += ((gint)priv->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_RIGHT:
	  new_x += (gint)priv->float_allocation.width - width;
	  new_y += ((gint)priv->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_TOP:
	  new_x += ((gint)priv->float_allocation.width - width) / 2;
	  break;
	case GTK_POS_BOTTOM:
	  new_x += ((gint)priv->float_allocation.width - width) / 2;
	  new_y += (gint)priv->float_allocation.height - height;
	  break;
	}

      if (priv->child_detached)
	{
	  gdk_window_move (priv->float_window, new_x, new_y);
	  gdk_window_raise (priv->float_window);
	}
      else
	{
          guint border_width;
	  GtkRequisition child_requisition;

	  priv->child_detached = TRUE;

	  if (child)
            {
              gtk_widget_get_preferred_size (child, &child_requisition, NULL);
            }
	  else
	    {
	      child_requisition.width = 0;
	      child_requisition.height = 0;
	    }      

          border_width = gtk_container_get_border_width (GTK_CONTAINER (hb));
	  width = child_requisition.width + 2 * border_width;
	  height = child_requisition.height + 2 * border_width;

	  if (handle_position == GTK_POS_LEFT || handle_position == GTK_POS_RIGHT)
	    width += DRAG_HANDLE_SIZE;
	  else
	    height += DRAG_HANDLE_SIZE;
	  
	  gdk_window_move_resize (priv->float_window, new_x, new_y, width, height);
	  gdk_window_reparent (priv->bin_window, priv->float_window, 0, 0);
	  gdk_window_set_geometry_hints (priv->float_window, &geometry, GDK_HINT_POS);
	  gdk_window_show (priv->float_window);
	  priv->float_window_mapped = TRUE;
#if	0
	  /* this extra move is necessary if we use decorations, or our
	   * window manager insists on decorations.
	   */
	  gdk_display_sync (gtk_widget_get_display (widget));
	  gdk_window_move (priv->float_window, new_x, new_y);
	  gdk_display_sync (gtk_widget_get_display (widget));
#endif	/* 0 */
	  g_signal_emit (hb,
			 handle_box_signals[SIGNAL_CHILD_DETACHED],
			 0,
			 child);
	  
	  gtk_widget_queue_resize (widget);
	}
    }

  return TRUE;
}

static void
gtk_handle_box_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  GtkHandleBoxPrivate *priv = GTK_HANDLE_BOX (container)->priv;

  gtk_widget_set_parent_window (widget, priv->bin_window);
  GTK_CONTAINER_CLASS (gtk_handle_box_parent_class)->add (container, widget);
}

static void
gtk_handle_box_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (gtk_handle_box_parent_class)->remove (container, widget);

  gtk_handle_box_reattach (GTK_HANDLE_BOX (container));
}

static gint
gtk_handle_box_delete_event (GtkWidget *widget,
			     GdkEventAny  *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  if (event->window == priv->float_window)
    {
      gtk_handle_box_reattach (hb);
      
      return TRUE;
    }

  return FALSE;
}

static void
gtk_handle_box_reattach (GtkHandleBox *hb)
{
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkWidget *child;
  GtkWidget *widget = GTK_WIDGET (hb);
  
  if (priv->child_detached)
    {
      priv->child_detached = FALSE;
      if (gtk_widget_get_realized (widget))
	{
	  gdk_window_hide (priv->float_window);
          gdk_window_reparent (priv->bin_window, gtk_widget_get_window (widget),
                               0, 0);

          child = gtk_bin_get_child (GTK_BIN (hb));
	  if (child)
	    g_signal_emit (hb,
			   handle_box_signals[SIGNAL_CHILD_ATTACHED],
			   0,
			   child);

	}
      priv->float_window_mapped = FALSE;
    }
  if (priv->in_drag)
    gtk_handle_box_end_drag (hb, GDK_CURRENT_TIME);

  gtk_widget_queue_resize (GTK_WIDGET (hb));
}

static void
gtk_handle_box_end_drag (GtkHandleBox *hb,
			 guint32       time)
{
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkWidget *invisible = gtk_handle_box_get_invisible ();

  priv->in_drag = FALSE;

  gtk_device_grab_remove (invisible, priv->grab_device);
  gdk_device_ungrab (priv->grab_device, time);
  g_signal_handlers_disconnect_by_func (invisible,
					G_CALLBACK (gtk_handle_box_grab_event),
					hb);

  priv->grab_device = NULL;
}
