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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdlib.h>
#include "gtkhandlebox.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkintl.h"



struct _GtkHandleBoxPrivate
{
  /* Properties */
  GtkPositionType handle_position;
  GtkPositionType snap_edge;
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
  gint            deskoff_x;      /* Offset between root relative coords */
  gint            deskoff_y;      /* and deskrelative coords             */

  gint            orig_x;
  gint            orig_y;

  guint           float_window_mapped : 1;
  guint           in_drag : 1;
  guint           shrink_on_detach : 1;
};

enum {
  PROP_0,
  PROP_SHADOW,
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
 * 2) These rectangles must have one edge, the "snap_edge"
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
static void     gtk_handle_box_style_set     (GtkWidget      *widget,
                                              GtkStyle       *previous_style);
static void     gtk_handle_box_size_request  (GtkWidget      *widget,
                                              GtkRequisition *requisition);
static void     gtk_handle_box_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *real_allocation);
static void     gtk_handle_box_add           (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_remove        (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_draw_ghost    (GtkHandleBox   *hb);
static void     gtk_handle_box_paint         (GtkWidget      *widget,
                                              GdkEventExpose *event,
                                              GdkRectangle   *area);
static gboolean gtk_handle_box_expose        (GtkWidget      *widget,
                                              GdkEventExpose *event);
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

G_DEFINE_TYPE (GtkHandleBox, gtk_handle_box, GTK_TYPE_BIN)

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
                                   PROP_SHADOW,
                                   g_param_spec_enum ("shadow", NULL,
                                                      P_("Deprecated property, use shadow_type instead"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));
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
  widget_class->style_set = gtk_handle_box_style_set;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_press;
  widget_class->delete_event = gtk_handle_box_delete_event;

  container_class->add = gtk_handle_box_add;
  container_class->remove = gtk_handle_box_remove;

  class->child_attached = NULL;
  class->child_detached = NULL;

  handle_box_signals[SIGNAL_CHILD_ATTACHED] =
    g_signal_new (I_("child-attached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_attached),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  handle_box_signals[SIGNAL_CHILD_DETACHED] =
    g_signal_new (I_("child-detached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_detached),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  g_type_class_add_private (gobject_class, sizeof (GtkHandleBoxPrivate));
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  GtkHandleBoxPrivate *priv;

  handle_box->priv = G_TYPE_INSTANCE_GET_PRIVATE (handle_box,
                                                  GTK_TYPE_HANDLE_BOX,
                                                  GtkHandleBoxPrivate);
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
    case PROP_SHADOW:
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
    case PROP_SHADOW:
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
  gdk_window_show (widget->window);
}

static void
gtk_handle_box_unmap (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  gtk_widget_set_mapped (widget, FALSE);

  gdk_window_hide (widget->window);
  if (priv->float_window_mapped)
    {
      gdk_window_hide (priv->float_window);
      priv->float_window_mapped = FALSE;
    }
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkWidget *child;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget)
			   | GDK_EXPOSURE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON1_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK |
			   GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  priv->bin_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (priv->bin_window, widget);

  child = gtk_bin_get_child (GTK_BIN (hb));
  if (child)
    gtk_widget_set_parent_window (child, priv->bin_window);
  
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->requisition.width;
  attributes.height = widget->requisition.height;
  attributes.window_type = GDK_WINDOW_TOPLEVEL;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_KEY_PRESS_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_FOCUS_CHANGE_MASK |
			   GDK_STRUCTURE_MASK);
  attributes.type_hint = GDK_WINDOW_TYPE_HINT_TOOLBAR;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_TYPE_HINT;
  priv->float_window = gdk_window_new (gtk_widget_get_root_window (widget),
				     &attributes, attributes_mask);
  gdk_window_set_user_data (priv->float_window, widget);
  gdk_window_set_decorations (priv->float_window, 0);
  gdk_window_set_type_hint (priv->float_window, GDK_WINDOW_TYPE_HINT_TOOLBAR);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, gtk_widget_get_state (widget));
  gtk_style_set_background (widget->style, priv->bin_window, gtk_widget_get_state (widget));
  gtk_style_set_background (widget->style, priv->float_window, gtk_widget_get_state (widget));
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
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
gtk_handle_box_style_set (GtkWidget *widget,
			  GtkStyle  *previous_style)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;

  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
    {
      gtk_style_set_background (widget->style, widget->window,
				widget->state);
      gtk_style_set_background (widget->style, priv->bin_window, widget->state);
      gtk_style_set_background (widget->style, priv->float_window, widget->state);
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
    gtk_widget_size_request (child, &child_requisition);
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
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += widget->style->ythickness;
	  else
	    requisition->width += widget->style->xthickness;
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
    gtk_widget_get_child_requisition (child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      
      
  widget->allocation = *allocation;

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (widget->window,
			    widget->allocation.x,
			    widget->allocation.y,
			    widget->allocation.width,
			    widget->allocation.height);


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
	  child_allocation.width = MAX (1, (gint)widget->allocation.width - 2 * border_width);
	  child_allocation.height = MAX (1, (gint)widget->allocation.height - 2 * border_width);

	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    child_allocation.width -= DRAG_HANDLE_SIZE;
	  else
	    child_allocation.height -= DRAG_HANDLE_SIZE;
	  
	  if (gtk_widget_get_realized (widget))
	    gdk_window_move_resize (priv->bin_window,
				    0,
				    0,
				    widget->allocation.width,
				    widget->allocation.height);
	}

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
gtk_handle_box_draw_ghost (GtkHandleBox *hb)
{
  GtkWidget *widget;
  guint x;
  guint y;
  guint width;
  guint height;
  gint handle_position;

  widget = GTK_WIDGET (hb);
  
  handle_position = effective_handle_position (hb);
  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      x = handle_position == GTK_POS_LEFT ? 0 : widget->allocation.width - DRAG_HANDLE_SIZE;
      y = 0;
      width = DRAG_HANDLE_SIZE;
      height = widget->allocation.height;
    }
  else
    {
      x = 0;
      y = handle_position == GTK_POS_TOP ? 0 : widget->allocation.height - DRAG_HANDLE_SIZE;
      width = widget->allocation.width;
      height = DRAG_HANDLE_SIZE;
    }
  gtk_paint_shadow (widget->style,
		    widget->window,
		    gtk_widget_get_state (widget),
		    GTK_SHADOW_ETCHED_IN,
		    NULL, widget, "handle",
		    x,
		    y,
		    width,
		    height);
   if (handle_position == GTK_POS_LEFT ||
       handle_position == GTK_POS_RIGHT)
     gtk_paint_hline (widget->style,
		      widget->window,
		      gtk_widget_get_state (widget),
		      NULL, widget, "handlebox",
		      handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		      handle_position == GTK_POS_LEFT ? widget->allocation.width : widget->allocation.width - DRAG_HANDLE_SIZE,
		      widget->allocation.height / 2);
   else
     gtk_paint_vline (widget->style,
		      widget->window,
		      gtk_widget_get_state (widget),
		      NULL, widget, "handlebox",
		      handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		      handle_position == GTK_POS_TOP ? widget->allocation.height : widget->allocation.height - DRAG_HANDLE_SIZE,
		      widget->allocation.width / 2);
}

static void
draw_textured_frame (GtkWidget *widget, GdkWindow *window, GdkRectangle *rect, GtkShadowType shadow,
		     GdkRectangle *clip, GtkOrientation orientation)
{
   gtk_paint_handle (widget->style, window, GTK_STATE_NORMAL, shadow,
		     clip, widget, "handlebox",
		     rect->x, rect->y, rect->width, rect->height, 
		     orientation);
}

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
 * Return value: the type of shadow currently drawn around the handle box.
 **/
GtkShadowType
gtk_handle_box_get_shadow_type (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_SHADOW_ETCHED_OUT);

  return handle_box->priv->shadow_type;
}

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
 * Return value: the current handle position.
 **/
GtkPositionType
gtk_handle_box_get_handle_position (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_POS_LEFT);

  return handle_box->priv->handle_position;
}

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
 * Gets the edge used for determining reattachment of the handle box. See
 * gtk_handle_box_set_snap_edge().
 *
 * Return value: the edge used for determining reattachment, or (GtkPositionType)-1 if this
 *               is determined (as per default) from the handle position. 
 **/
GtkPositionType
gtk_handle_box_get_snap_edge (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), (GtkPositionType)-1);

  return handle_box->priv->snap_edge;
}

/**
 * gtk_handle_box_get_child_detached:
 * @handle_box: a #GtkHandleBox
 *
 * Whether the handlebox's child is currently detached.
 *
 * Return value: %TRUE if the child is currently detached, otherwise %FALSE
 *
 * Since: 2.14
 **/
gboolean
gtk_handle_box_get_child_detached (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), FALSE);

  return handle_box->priv->child_detached;
}

static void
gtk_handle_box_paint (GtkWidget      *widget,
                      GdkEventExpose *event,
		      GdkRectangle   *area)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  GtkHandleBoxPrivate *priv = hb->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;
  gint width, height;
  GdkRectangle rect;
  GdkRectangle dest;
  gint handle_position;
  GtkOrientation handle_orientation;

  handle_position = effective_handle_position (hb);

  gdk_drawable_get_size (priv->bin_window, &width, &height);

  if (!event)
    gtk_paint_box (widget->style,
		   priv->bin_window,
		   gtk_widget_get_state (widget),
		   priv->shadow_type,
		   area, widget, "handlebox_bin",
		   0, 0, -1, -1);
  else
   gtk_paint_box (widget->style,
		  priv->bin_window,
		  gtk_widget_get_state (widget),
		  priv->shadow_type,
		  &event->area, widget, "handlebox_bin",
		  0, 0, -1, -1);

/* We currently draw the handle _above_ the relief of the handlebox.
 * it could also be drawn on the same level...

		 priv->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		 priv->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		 width,
		 height);*/

  switch (handle_position)
    {
    case GTK_POS_LEFT:
      rect.x = 0;
      rect.y = 0; 
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      handle_orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_POS_RIGHT:
      rect.x = width - DRAG_HANDLE_SIZE; 
      rect.y = 0;
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      handle_orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_POS_TOP:
      rect.x = 0;
      rect.y = 0; 
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      handle_orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    case GTK_POS_BOTTOM:
      rect.x = 0;
      rect.y = height - DRAG_HANDLE_SIZE;
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      handle_orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    default: 
      g_assert_not_reached ();
      break;
    }

  if (gdk_rectangle_intersect (event ? &event->area : area, &rect, &dest))
    draw_textured_frame (widget, priv->bin_window, &rect,
			 GTK_SHADOW_OUT,
			 event ? &event->area : area,
			 handle_orientation);

  child = gtk_bin_get_child (bin);
  if (child != NULL && gtk_widget_get_visible (child))
    GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->expose_event (widget, event);
}

static gboolean
gtk_handle_box_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  GtkHandleBox *hb;
  GtkHandleBoxPrivate *priv;

  if (gtk_widget_is_drawable (widget))
    {
      hb = GTK_HANDLE_BOX (widget);
      priv = hb->priv;

      if (event->window == widget->window)
	{
	  if (priv->child_detached)
	    gtk_handle_box_draw_ghost (hb);
	}
      else
	gtk_handle_box_paint (widget, event, NULL);
    }
  
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
  if ((event->button == 1) && 
      (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS))
    {
      GtkWidget *child;
      gboolean in_handle;
      
      if (event->window != priv->bin_window)
	return FALSE;

      child = gtk_bin_get_child (GTK_BIN (hb));

      if (child)
	{
          guint border_width;

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
	      in_handle = event->x > 2 * border_width + child->allocation.width;
	      break;
	    case GTK_POS_BOTTOM:
	      in_handle = event->y > 2 * border_width + child->allocation.height;
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
	      gint desk_x, desk_y;
	      gint root_x, root_y;
	      gint width, height;

              gtk_invisible_set_screen (GTK_INVISIBLE (invisible),
                                        gtk_widget_get_screen (GTK_WIDGET (hb)));
	      gdk_window_get_deskrelative_origin (priv->bin_window, &desk_x, &desk_y);
	      gdk_window_get_origin (priv->bin_window, &root_x, &root_y);
	      gdk_drawable_get_size (priv->bin_window, &width, &height);

	      priv->orig_x = event->x_root;
	      priv->orig_y = event->y_root;

	      priv->float_allocation.x = root_x - event->x_root;
	      priv->float_allocation.y = root_y - event->y_root;
	      priv->float_allocation.width = width;
	      priv->float_allocation.height = height;

	      priv->deskoff_x = desk_x - root_x;
	      priv->deskoff_y = desk_y - root_y;

	      if (gdk_window_is_viewable (widget->window))
		{
		  gdk_window_get_origin (widget->window, &root_x, &root_y);
		  gdk_drawable_get_size (widget->window, &width, &height);

		  priv->attach_allocation.x = root_x;
		  priv->attach_allocation.y = root_y;
		  priv->attach_allocation.width = width;
		  priv->attach_allocation.height = height;
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
                                   invisible->window,
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

	      gdk_cursor_unref (fleur);
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
  gdk_display_get_device_state (gdk_screen_get_display (screen),
                                event->device,
                                &pointer_screen,
                                &new_x, &new_y, NULL);
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
	  gdk_window_reparent (priv->bin_window, widget->window, 0, 0);
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

      gdk_drawable_get_size (priv->float_window, &width, &height);
      new_x += priv->deskoff_x;
      new_y += priv->deskoff_y;

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
	  gint width;
	  gint height;
          guint border_width;
	  GtkRequisition child_requisition;

	  priv->child_detached = TRUE;

	  if (child)
	    gtk_widget_get_child_requisition (child, &child_requisition);
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
	  gtk_handle_box_draw_ghost (hb);
	  
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
	  gdk_window_reparent (priv->bin_window, widget->window, 0, 0);

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
