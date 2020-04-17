/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAspectFrame: Ensure that the child window has a specified aspect ratio
 *    or, if obey_child, has the same aspect ratio as its requested size
 *
 *     Copyright Owen Taylor                          4/9/97
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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/**
 * SECTION:gtkaspectframe
 * @Short_description: A frame that constrains its child to a particular aspect ratio
 * @Title: GtkAspectFrame
 *
 * The #GtkAspectFrame is useful when you want
 * pack a widget so that it can resize but always retains
 * the same aspect ratio. For instance, one might be
 * drawing a small preview of a larger image. #GtkAspectFrame
 * derives from #GtkFrame, so it can draw a label and
 * a frame around the child. The frame will be
 * “shrink-wrapped” to the size of the child.
 *
 * # CSS nodes
 *
 * GtkAspectFrame uses a CSS node with name frame.
 */

#include "config.h"

#include "gtkaspectframe.h"
#include "gtkbin.h"

#include "gtksizerequest.h"

#include "gtkprivate.h"
#include "gtkintl.h"


typedef struct _GtkAspectFrameClass GtkAspectFrameClass;

struct _GtkAspectFrame
{
  GtkBin parent_instance;

  gboolean      obey_child;
  float         xalign;
  float         yalign;
  float         ratio;
};

struct _GtkAspectFrameClass
{
  GtkBinClass parent_class;
};

enum {
  PROP_0,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_RATIO,
  PROP_OBEY_CHILD
};

static void gtk_aspect_frame_set_property (GObject         *object,
					   guint            prop_id,
					   const GValue    *value,
					   GParamSpec      *pspec);
static void gtk_aspect_frame_get_property (GObject         *object,
					   guint            prop_id,
					   GValue          *value,
					   GParamSpec      *pspec);
static void gtk_aspect_frame_size_allocate (GtkWidget      *widget,
                                            int             width,
                                            int             height,
                                            int             baseline);

#define MAX_RATIO 10000.0
#define MIN_RATIO 0.0001

G_DEFINE_TYPE (GtkAspectFrame, gtk_aspect_frame, GTK_TYPE_BIN)

static void
gtk_aspect_frame_class_init (GtkAspectFrameClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  
  gobject_class->set_property = gtk_aspect_frame_set_property;
  gobject_class->get_property = gtk_aspect_frame_get_property;

  widget_class->size_allocate = gtk_aspect_frame_size_allocate;

  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign",
                                                       P_("Horizontal Alignment"),
                                                       P_("X alignment of the child"),
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float ("yalign",
                                                       P_("Vertical Alignment"),
                                                       P_("Y alignment of the child"),
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_RATIO,
                                   g_param_spec_float ("ratio",
                                                       P_("Ratio"),
                                                       P_("Aspect ratio if obey_child is FALSE"),
                                                       MIN_RATIO, MAX_RATIO, 1.0,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_property (gobject_class,
                                   PROP_OBEY_CHILD,
                                   g_param_spec_boolean ("obey-child",
                                                         P_("Obey child"),
                                                         P_("Force aspect ratio to match that of the frame’s child"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), I_("aspectframe"));
}

static void
gtk_aspect_frame_init (GtkAspectFrame *self)
{
  self->xalign = 0.5;
  self->yalign = 0.5;
  self->ratio = 1.0;
  self->obey_child = TRUE;
}

static void
gtk_aspect_frame_set_property (GObject         *object,
			       guint            prop_id,
			       const GValue    *value,
			       GParamSpec      *pspec)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (object);
  
  switch (prop_id)
    {
      /* g_object_notify is handled by the _frame_set function */
    case PROP_XALIGN:
      gtk_aspect_frame_set_xalign (self, g_value_get_float (value));
      break;
    case PROP_YALIGN:
      gtk_aspect_frame_set_yalign (self, g_value_get_float (value));
      break;
    case PROP_RATIO:
      gtk_aspect_frame_set_ratio (self, g_value_get_float (value));
      break;
    case PROP_OBEY_CHILD:
      gtk_aspect_frame_set_obey_child (self, g_value_get_boolean (value));
      break;
    default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_aspect_frame_get_property (GObject         *object,
			       guint            prop_id,
			       GValue          *value,
			       GParamSpec      *pspec)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (object);
  
  switch (prop_id)
    {
    case PROP_XALIGN:
      g_value_set_float (value, self->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, self->yalign);
      break;
    case PROP_RATIO:
      g_value_set_float (value, self->ratio);
      break;
    case PROP_OBEY_CHILD:
      g_value_set_boolean (value, self->obey_child);
      break;
    default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_aspect_frame_new:
 * @xalign: Horizontal alignment of the child within the allocation of
 *  the #GtkAspectFrame. This ranges from 0.0 (left aligned)
 *  to 1.0 (right aligned)
 * @yalign: Vertical alignment of the child within the allocation of
 *  the #GtkAspectFrame. This ranges from 0.0 (top aligned)
 *  to 1.0 (bottom aligned)
 * @ratio: The desired aspect ratio.
 * @obey_child: If %TRUE, @ratio is ignored, and the aspect
 *  ratio is taken from the requistion of the child.
 *
 * Create a new #GtkAspectFrame.
 *
 * Returns: the new #GtkAspectFrame.
 */
GtkWidget *
gtk_aspect_frame_new (float    xalign,
		      float    yalign,
		      float    ratio,
		      gboolean obey_child)
{
  GtkAspectFrame *self;

  self = g_object_new (GTK_TYPE_ASPECT_FRAME, NULL);

  self->xalign = CLAMP (xalign, 0.0, 1.0);
  self->yalign = CLAMP (yalign, 0.0, 1.0);
  self->ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);
  self->obey_child = obey_child != FALSE;

  return GTK_WIDGET (self);
}

/**
 * gtk_aspect_frame_set_xalign:
 * @self: a #GtkAspectFrame
 * @xalign: horizontal alignment, from 0.0 (left aligned) to 1.0 (right aligned)
 *
 * Sets the horizontal alignment of the child within the allocation
 * of the #GtkAspectFrame.
 */
void
gtk_aspect_frame_set_xalign (GtkAspectFrame *self,
                             float           xalign)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  xalign = CLAMP (xalign, 0.0, 1.0);

  if (self->xalign == xalign)
    return;

  self->xalign = xalign;

  g_object_notify (G_OBJECT (self), "xalign");
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_aspect_frame_get_xalign:
 * @self: a #GtkAspectFrame
 *
 * Returns the horizontal alignment of the child within the
 * allocation of the #GtkAspectFrame.
 *
 * Returns: the horizontal alignment
 */
float
gtk_aspect_frame_get_xalign (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), 0.5);

  return self->xalign;
}

/**
 * gtk_aspect_frame_set_yalign:
 * @self: a #GtkAspectFrame
 * @yalign: horizontal alignment, from 0.0 (top aligned) to 1.0 (bottom aligned)
 *
 * Sets the vertical alignment of the child within the allocation
 * of the #GtkAspectFrame.
 */
void
gtk_aspect_frame_set_yalign (GtkAspectFrame *self,
                             float           yalign)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  yalign = CLAMP (yalign, 0.0, 1.0);

  if (self->yalign == yalign)
    return;

  self->yalign = yalign;

  g_object_notify (G_OBJECT (self), "yalign");
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_aspect_frame_get_yalign:
 * @self: a #GtkAspectFrame
 *
 * Returns the vertical alignment of the child within the
 * allocation of the #GtkAspectFrame.
 *
 * Returns: the vertical alignment
 */
float
gtk_aspect_frame_get_yalign (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), 0.5);

  return self->xalign;
}

/**
 * gtk_aspect_frame_set_ratio:
 * @self: a #GtkAspectFrame
 * @ratio: aspect ratio of the child
 *
 * Sets the desired aspect ratio of the child.
 */
void
gtk_aspect_frame_set_ratio (GtkAspectFrame *self,
                            float           ratio)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);

  if (self->ratio == ratio)
    return;

  self->ratio = ratio;

  g_object_notify (G_OBJECT (self), "ratio");
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_aspect_frame_get_ratio:
 * @self: a #GtkAspectFrame
 *
 * Returns the desired aspect ratio of the child.
 *
 * Returns: the desired aspect ratio
 */
float
gtk_aspect_frame_get_ratio (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), 1.0);

  return self->ratio;
}

/**
 * gtk_aspect_frame_set_obey_child:
 * @self: a #GtkAspectFrame
 * @obey_child: If %TRUE, @ratio is ignored, and the aspect
 *    ratio is taken from the requistion of the child.
 *
 * Sets whether the aspect ratio of the childs size
 * request should override the set aspect ratio of
 * the #GtkAspectFrame.
 */
void
gtk_aspect_frame_set_obey_child (GtkAspectFrame *self,
                                 gboolean        obey_child)
{
  g_return_if_fail (GTK_IS_ASPECT_FRAME (self));

  if (self->obey_child == obey_child)
    return;

  self->obey_child = obey_child;

  g_object_notify (G_OBJECT (self), "obey-child");
  gtk_widget_queue_resize (GTK_WIDGET (self));

}

/**
 * gtk_aspect_frame_get_obey_child:
 * @self: a #GtkAspectFrame
 *
 * Returns whether the childs size request should override
 * the set aspect ratio of the #GtkAspectFrame.
 *
 * Returns: whether to obey the childs size request
 */
gboolean
gtk_aspect_frame_get_obey_child (GtkAspectFrame *self)
{
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (self), TRUE);

  return self->obey_child;
}

static void
get_full_allocation (GtkAspectFrame *self,
                     GtkAllocation  *child_allocation)
{
  child_allocation->x = 0;
  child_allocation->y = 0;
  child_allocation->width = gtk_widget_get_width (GTK_WIDGET (self));
  child_allocation->height = gtk_widget_get_height (GTK_WIDGET (self));
}

static void
compute_child_allocation (GtkAspectFrame *self,
                          GtkAllocation  *child_allocation)
{
  GtkBin *bin = GTK_BIN (self);
  GtkWidget *child;
  gdouble ratio;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation full_allocation;
      
      if (self->obey_child)
	{
	  GtkRequisition child_requisition;

          gtk_widget_get_preferred_size (child, &child_requisition, NULL);
	  if (child_requisition.height != 0)
	    {
	      ratio = ((gdouble) child_requisition.width /
		       child_requisition.height);
	      if (ratio < MIN_RATIO)
		ratio = MIN_RATIO;
	    }
	  else if (child_requisition.width != 0)
	    ratio = MAX_RATIO;
	  else
	    ratio = 1.0;
	}
      else
	ratio = self->ratio;

      get_full_allocation (self, &full_allocation);
      
      if (ratio * full_allocation.height > full_allocation.width)
	{
	  child_allocation->width = full_allocation.width;
	  child_allocation->height = full_allocation.width / ratio + 0.5;
	}
      else
	{
	  child_allocation->width = ratio * full_allocation.height + 0.5;
	  child_allocation->height = full_allocation.height;
	}
      
      child_allocation->x = full_allocation.x + self->xalign * (full_allocation.width - child_allocation->width);
      child_allocation->y = full_allocation.y + self->yalign * (full_allocation.height - child_allocation->height);
    }
  else
    get_full_allocation (self, child_allocation);
}

static void
gtk_aspect_frame_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkAspectFrame *self = GTK_ASPECT_FRAME (widget);
  GtkWidget *child;
  GtkAllocation new_allocation;

  compute_child_allocation (self, &new_allocation);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &new_allocation, -1);
}
