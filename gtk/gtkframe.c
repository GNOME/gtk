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

#include <string.h>
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtkintl.h"

#define LABEL_PAD 1
#define LABEL_SIDE_PAD 2

enum {
  PROP_0,
  PROP_LABEL,
  PROP_LABEL_XALIGN,
  PROP_LABEL_YALIGN,
  PROP_SHADOW,
  PROP_LABEL_WIDGET
};


static void gtk_frame_class_init    (GtkFrameClass  *klass);
static void gtk_frame_init          (GtkFrame       *frame);
static void gtk_frame_set_property (GObject      *object,
				    guint         param_id,
				    const GValue *value,
				    GParamSpec   *pspec);
static void gtk_frame_get_property (GObject     *object,
				    guint        param_id,
				    GValue      *value,
				    GParamSpec  *pspec);
static void gtk_frame_paint         (GtkWidget      *widget,
				     GdkRectangle   *area);
static gint gtk_frame_expose        (GtkWidget      *widget,
				     GdkEventExpose *event);
static void gtk_frame_size_request  (GtkWidget      *widget,
				     GtkRequisition *requisition);
static void gtk_frame_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);
static void gtk_frame_remove        (GtkContainer   *container,
				     GtkWidget      *child);
static void gtk_frame_forall        (GtkContainer   *container,
				     gboolean	     include_internals,
			             GtkCallback     callback,
			             gpointer        callback_data);

static void gtk_frame_compute_child_allocation      (GtkFrame      *frame,
						     GtkAllocation *child_allocation);
static void gtk_frame_real_compute_child_allocation (GtkFrame      *frame,
						     GtkAllocation *child_allocation);

static GtkBinClass *parent_class = NULL;


GtkType
gtk_frame_get_type (void)
{
  static GtkType frame_type = 0;

  if (!frame_type)
    {
      static const GtkTypeInfo frame_info =
      {
	"GtkFrame",
	sizeof (GtkFrame),
	sizeof (GtkFrameClass),
	(GtkClassInitFunc) gtk_frame_class_init,
	(GtkObjectInitFunc) gtk_frame_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      frame_type = gtk_type_unique (gtk_bin_get_type (), &frame_info);
    }

  return frame_type;
}

static void
gtk_frame_class_init (GtkFrameClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) class;
  object_class = GTK_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);

  parent_class = gtk_type_class (gtk_bin_get_type ());

  gobject_class->set_property = gtk_frame_set_property;
  gobject_class->get_property = gtk_frame_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        _("Label"),
                                                        _("Text of the frame's label."),
                                                        NULL,
                                                        G_PARAM_READABLE |
							G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
				   PROP_LABEL_XALIGN,
				   g_param_spec_double ("label_xalign",
                                                        _("Label xalign"),
                                                        _("The horizontal alignment of the label."),
                                                        0.0,
                                                        1.0,
                                                        0.5,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
				   PROP_LABEL_YALIGN,
				   g_param_spec_double ("label_yalign",
                                                        _("Label yalign"),
                                                        _("The vertical alignment of the label."),
                                                        0.0,
                                                        1.0,
                                                        0.5,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW,
                                   g_param_spec_enum ("shadow",
                                                      _("Frame shadow"),
                                                      _("Appearance of the frame border."),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_ETCHED_IN,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_WIDGET,
                                   g_param_spec_object ("label_widget",
                                                        _("Label widget"),
                                                        _("A widget to display in place of the usual frame label."),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  widget_class->expose_event = gtk_frame_expose;
  widget_class->size_request = gtk_frame_size_request;
  widget_class->size_allocate = gtk_frame_size_allocate;

  container_class->remove = gtk_frame_remove;
  container_class->forall = gtk_frame_forall;

  class->compute_child_allocation = gtk_frame_real_compute_child_allocation;
}

static void
gtk_frame_init (GtkFrame *frame)
{
  frame->label_widget = NULL;
  frame->shadow_type = GTK_SHADOW_ETCHED_IN;
  frame->label_xalign = 0.0;
  frame->label_yalign = 0.5;
}

static void 
gtk_frame_set_property (GObject         *object,
			guint            prop_id,
			const GValue    *value,
			GParamSpec      *pspec)
{
  GtkFrame *frame;

  frame = GTK_FRAME (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_frame_set_label (frame, g_value_get_string (value));
      break;
    case PROP_LABEL_XALIGN:
      gtk_frame_set_label_align (frame, g_value_get_double (value), 
				 frame->label_yalign);
      break;
    case PROP_LABEL_YALIGN:
      gtk_frame_set_label_align (frame, frame->label_xalign, 
				 g_value_get_double (value));
      break;
    case PROP_SHADOW:
      gtk_frame_set_shadow_type (frame, g_value_get_enum (value));
      break;
    case PROP_LABEL_WIDGET:
      gtk_frame_set_label_widget (frame, g_value_get_object (value));
      break;
    default:      
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_frame_get_property (GObject         *object,
			guint            prop_id,
			GValue          *value,
			GParamSpec      *pspec)
{
  GtkFrame *frame;

  frame = GTK_FRAME (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_frame_get_label (frame));
      break;
    case PROP_LABEL_XALIGN:
      g_value_set_double (value, frame->label_xalign);
      break;
    case PROP_LABEL_YALIGN:
      g_value_set_double (value, frame->label_yalign);
      break;
    case PROP_SHADOW:
      g_value_set_enum (value, frame->shadow_type);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
                          frame->label_widget ?
                          G_OBJECT (frame->label_widget) : NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
gtk_frame_new (const gchar *label)
{
  GtkFrame *frame;

  frame = gtk_type_new (gtk_frame_get_type ());

  gtk_frame_set_label (frame, label);

  return GTK_WIDGET (frame);
}

static void
gtk_frame_remove (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkFrame *frame = GTK_FRAME (container);

  if (frame->label_widget == child)
    gtk_frame_set_label_widget (frame, NULL);
  else
    GTK_CONTAINER_CLASS (parent_class)->remove (container, child);
}

static void
gtk_frame_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkFrame *frame = GTK_FRAME (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);

  if (frame->label_widget)
    (* callback) (frame->label_widget, callback_data);
}

void
gtk_frame_set_label (GtkFrame *frame,
		     const gchar *label)
{
  g_return_if_fail (GTK_IS_FRAME (frame));

  if (!label)
    {
      gtk_frame_set_label_widget (frame, NULL);
    }
  else
    {
      GtkWidget *child = gtk_label_new (label);
      gtk_widget_show (child);

      gtk_frame_set_label_widget (frame, child);
    }

  g_object_notify (G_OBJECT (frame), "label");
}

/**
 * gtk_frame_get_label:
 * @frame: a #GtkFrame
 * 
 * If the frame's label widget is a #GtkLabel, return the
 * text in the label widget. (The frame will have a #GtkLabel
 * for the label widget if a non-%NULL argument was passed
 * to gtk_frame_new().)
 * 
 * Return value: the text in the label, or %NULL if there
 *               was no label widget or the lable widget was not
 *               a #GtkLabel. This value must be freed with g_free().
 **/
G_CONST_RETURN gchar *
gtk_frame_get_label (GtkFrame *frame)
{
  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  if (frame->label_widget && GTK_IS_LABEL (frame->label_widget))
    return gtk_label_get_text (GTK_LABEL (frame->label_widget));
  else
    return NULL;
}

/**
 * gtk_frame_set_label_widget:
 * @frame: a #GtkFrame
 * @label_widget: the new label widget
 * 
 * Set the label widget for the frame. This is the widget that
 * will appear embedded in the top edge of the frame as a
 * title.
 **/
void
gtk_frame_set_label_widget (GtkFrame  *frame,
			    GtkWidget *label_widget)
{
  gboolean need_resize = FALSE;
  
  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || label_widget->parent == NULL);
  
  if (frame->label_widget == label_widget)
    return;
  
  if (frame->label_widget)
    {
      need_resize = GTK_WIDGET_VISIBLE (frame->label_widget);
      gtk_widget_unparent (frame->label_widget);
    }

  frame->label_widget = label_widget;
    
  if (label_widget)
    {
      frame->label_widget = label_widget;
      gtk_widget_set_parent (label_widget, GTK_WIDGET (frame));
      need_resize |= GTK_WIDGET_VISIBLE (label_widget);
    }
  
  if (GTK_WIDGET_VISIBLE (frame) && need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (frame));

  g_object_notify (G_OBJECT (frame), "label_widget");
}

/**
 * gtk_frame_get_label_widget:
 * @frame: a #GtkFrame
 *
 * Retrieves the label widget for the frame. See
 * gtk_frame_set_label_widget().
 *
 * Return value: the label widget, or %NULL if there is none.
 **/
GtkWidget *
gtk_frame_get_label_widget (GtkFrame *frame)
{
  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  return frame->label_widget;
}

void
gtk_frame_set_label_align (GtkFrame *frame,
			   gfloat    xalign,
			   gfloat    yalign)
{
  g_return_if_fail (GTK_IS_FRAME (frame));

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);

  if (xalign != frame->label_xalign)
    {
      frame->label_xalign = xalign;
      g_object_notify (G_OBJECT (frame), "label_xalign");
    }

  if (yalign != frame->label_yalign)
    {
      frame->label_yalign = yalign;
      g_object_notify (G_OBJECT (frame), "label_yalign");
    }

  gtk_widget_queue_resize (GTK_WIDGET (frame));
}

/**
 * gtk_frame_get_label_align:
 * @frame: a #GtkFrame
 * @xalign: location to store X alignment of frame's label, or %NULL
 * @yalign: location to store X alignment of frame's label, or %NULL
 * 
 * Retrieves the X and Y alignment of the frame's label. See
 * gtk_frame_set_label_align().
 **/
void
gtk_frame_get_label_align (GtkFrame *frame,
		           gfloat   *xalign,
			   gfloat   *yalign)
{
  g_return_if_fail (GTK_IS_FRAME (frame));

  if (xalign)
    *xalign = frame->label_xalign;
  if (yalign)
    *yalign = frame->label_yalign;
}

void
gtk_frame_set_shadow_type (GtkFrame      *frame,
			   GtkShadowType  type)
{
  g_return_if_fail (GTK_IS_FRAME (frame));

  if ((GtkShadowType) frame->shadow_type != type)
    {
      frame->shadow_type = type;
      g_object_notify (G_OBJECT (frame), "shadow");

      if (GTK_WIDGET_DRAWABLE (frame))
	{
	  gtk_widget_queue_clear (GTK_WIDGET (frame));
	}
      
      gtk_widget_queue_resize (GTK_WIDGET (frame));
    }
}

/**
 * gtk_frame_get_shadow_type:
 * @frame: a #GtkFrame
 *
 * Retrieves the shadow type of the frame. See
 * gtk_frame_set_shadow_type().
 *
 * Return value: the current shadow type of the frame.
 **/
GtkShadowType
gtk_frame_get_shadow_type (GtkFrame *frame)
{
  g_return_val_if_fail (GTK_IS_FRAME (frame), GTK_SHADOW_ETCHED_IN);

  return frame->shadow_type;
}

static void
gtk_frame_paint (GtkWidget    *widget,
		 GdkRectangle *area)
{
  GtkFrame *frame;
  gint x, y, width, height;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);

      x = frame->child_allocation.x - widget->style->xthickness;
      y = frame->child_allocation.y - widget->style->ythickness;
      width = frame->child_allocation.width + 2 * widget->style->xthickness;
      height =  frame->child_allocation.height + 2 * widget->style->ythickness;

      if (frame->label_widget)
	{
	  GtkRequisition child_requisition;
	  gfloat xalign;
	  gint height_extra;
	  gint x2;

	  gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);

	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	    xalign = frame->label_xalign;
	  else
	    xalign = 1 - frame->label_xalign;

	  height_extra = MAX (0, child_requisition.height - widget->style->xthickness);
	  y -= height_extra * (1 - frame->label_yalign);
	  height += height_extra * (1 - frame->label_yalign);
	  
	  x2 = widget->style->xthickness + (frame->child_allocation.width - child_requisition.width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD) * xalign + LABEL_SIDE_PAD;

	  
	  gtk_paint_shadow_gap (widget->style, widget->window,
				GTK_STATE_NORMAL, frame->shadow_type,
				area, widget, "frame",
				x, y, width, height,
				GTK_POS_TOP, 
				x2, child_requisition.width + 2 * LABEL_PAD);
	}
       else
	 gtk_paint_shadow (widget->style, widget->window,
			   GTK_STATE_NORMAL, frame->shadow_type,
			   area, widget, "frame",
			   x, y, width, height);
    }
}

static gboolean
gtk_frame_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_frame_paint (widget, &event->area);

      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

static void
gtk_frame_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkRequisition child_requisition;
  
  if (frame->label_widget && GTK_WIDGET_VISIBLE (frame->label_widget))
    {
      gtk_widget_size_request (frame->label_widget, &child_requisition);

      requisition->width = child_requisition.width + 2 * LABEL_PAD + 2 * LABEL_SIDE_PAD;
      requisition->height =
	MAX (0, child_requisition.height - GTK_WIDGET (widget)->style->xthickness);
    }
  else
    {
      requisition->width = 0;
      requisition->height = 0;
    }
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height;
    }

  requisition->width += (GTK_CONTAINER (widget)->border_width +
			 GTK_WIDGET (widget)->style->xthickness) * 2;
  requisition->height += (GTK_CONTAINER (widget)->border_width +
			  GTK_WIDGET (widget)->style->ythickness) * 2;
}

static void
gtk_frame_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation new_allocation;

  widget->allocation = *allocation;

  gtk_frame_compute_child_allocation (frame, &new_allocation);
  
  /* If the child allocation changed, that means that the frame is drawn
   * in a new place, so we must redraw the entire widget.
   */
  if (GTK_WIDGET_MAPPED (widget) &&
      (new_allocation.x != frame->child_allocation.x ||
       new_allocation.y != frame->child_allocation.y ||
       new_allocation.width != frame->child_allocation.width ||
       new_allocation.height != frame->child_allocation.height))
    gtk_widget_queue_clear (widget);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, &new_allocation);
  
  frame->child_allocation = new_allocation;
  
  if (frame->label_widget && GTK_WIDGET_VISIBLE (frame->label_widget))
    {
      GtkRequisition child_requisition;
      GtkAllocation child_allocation;
      gfloat xalign;

      gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = frame->label_xalign;
      else
	xalign = 1 - frame->label_xalign;
      
      child_allocation.x = frame->child_allocation.x + LABEL_SIDE_PAD +
	(frame->child_allocation.width - child_requisition.width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD) * xalign + LABEL_PAD;
      child_allocation.width = child_requisition.width;

      child_allocation.y = frame->child_allocation.y - child_requisition.height;
      child_allocation.height = child_requisition.height;

      gtk_widget_size_allocate (frame->label_widget, &child_allocation);
    }
}

static void
gtk_frame_compute_child_allocation (GtkFrame      *frame,
				    GtkAllocation *child_allocation)
{
  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (child_allocation != NULL);

  GTK_FRAME_GET_CLASS (frame)->compute_child_allocation (frame, child_allocation);
}

static void
gtk_frame_real_compute_child_allocation (GtkFrame      *frame,
					 GtkAllocation *child_allocation)
{
  GtkWidget *widget = GTK_WIDGET (frame);
  GtkAllocation *allocation = &widget->allocation;
  GtkRequisition child_requisition;
  gint top_margin;

  if (frame->label_widget)
    {
      gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);
      top_margin = MAX (child_requisition.height, widget->style->ythickness);
    }
  else
    top_margin = widget->style->ythickness;
  
  child_allocation->x = (GTK_CONTAINER (frame)->border_width +
			 widget->style->xthickness);
  child_allocation->width = MAX(1, (gint)allocation->width - child_allocation->x * 2);
  
  child_allocation->y = (GTK_CONTAINER (frame)->border_width + top_margin);
  child_allocation->height = MAX (1, ((gint)allocation->height - child_allocation->y -
				      (gint)GTK_CONTAINER (frame)->border_width -
				      (gint)widget->style->ythickness));
  
  child_allocation->x += allocation->x;
  child_allocation->y += allocation->y;
}
