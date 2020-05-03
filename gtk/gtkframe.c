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

#include "config.h"
#include <string.h>
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtkbuildable.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtklabel.h"

#include "a11y/gtkframeaccessible.h"

/**
 * SECTION:gtkframe
 * @Short_description: A widget with a decorative frame and optional label
 * @Title: GtkFrame
 *
 * The frame widget is a widget that surrounds its child with a decorative
 * frame and an optional label. If present, the label is drawn inside
 * the top edge of the frame. The horizontal position of the label can
 * be controlled with gtk_frame_set_label_align().
 *
 * # GtkFrame as GtkBuildable
 *
 * The GtkFrame implementation of the GtkBuildable interface supports
 * placing a child in the label position by specifying “label” as the
 * “type” attribute of a <child> element. A normal content child can
 * be specified without specifying a <child> type attribute.
 *
 * An example of a UI definition fragment with GtkFrame:
 * |[
 * <object class="GtkFrame">
 *   <child type="label">
 *     <object class="GtkLabel" id="frame_label"/>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="frame_content"/>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * frame
 * ├── <label widget>
 * ╰── <child>
 * ]|
 *
 * GtkFrame has a main CSS node with name “frame”, which is used to draw the
 * visible border. You can set the appearance of the border using CSS properties
 * like “border-style” on this node.
 */

typedef struct
{
  /* Properties */
  GtkWidget *label_widget;
  GtkWidget *child;

  guint has_frame : 1;
  gfloat label_xalign;
} GtkFramePrivate;

enum {
  PROP_0,
  PROP_LABEL,
  PROP_LABEL_XALIGN,
  PROP_LABEL_WIDGET,
  PROP_CHILD,
  LAST_PROP
};

static GParamSpec *frame_props[LAST_PROP];

static void gtk_frame_dispose      (GObject      *object);
static void gtk_frame_set_property (GObject      *object,
                                    guint         param_id,
                                    const GValue *value,
                                    GParamSpec   *pspec);
static void gtk_frame_get_property (GObject     *object,
                                    guint        param_id,
                                    GValue      *value,
                                    GParamSpec  *pspec);
static void gtk_frame_size_allocate (GtkWidget  *widget,
                                     int         width,
                                     int         height,
                                     int         baseline);

static void gtk_frame_compute_child_allocation      (GtkFrame      *frame,
                                                     GtkAllocation *child_allocation);
static void gtk_frame_real_compute_child_allocation (GtkFrame      *frame,
                                                     GtkAllocation *child_allocation);

/* GtkBuildable */
static void gtk_frame_buildable_init                (GtkBuildableIface *iface);
static void gtk_frame_buildable_add_child           (GtkBuildable *buildable,
                                                     GtkBuilder   *builder,
                                                     GObject      *child,
                                                     const gchar  *type);
static void     gtk_frame_measure (GtkWidget           *widget,
                                   GtkOrientation       orientation,
                                   gint                 for_size,
                                   gint                *minimum_size,
                                   gint                *natural_size,
                                   gint                *minimum_baseline,
                                   gint                *natural_baseline);
static void     gtk_frame_compute_expand (GtkWidget *widget,
                                          gboolean  *hexpand,
                                          gboolean  *vexpand);
static GtkSizeRequestMode gtk_frame_get_request_mode (GtkWidget *widget);

G_DEFINE_TYPE_WITH_CODE (GtkFrame, gtk_frame, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkFrame)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_frame_buildable_init))

static void
gtk_frame_class_init (GtkFrameClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->dispose = gtk_frame_dispose;
  gobject_class->set_property = gtk_frame_set_property;
  gobject_class->get_property = gtk_frame_get_property;

  widget_class->size_allocate = gtk_frame_size_allocate;
  widget_class->measure = gtk_frame_measure;
  widget_class->compute_expand = gtk_frame_compute_expand;
  widget_class->get_request_mode = gtk_frame_get_request_mode;
  widget_class->grab_focus = gtk_widget_grab_focus_none;
  widget_class->focus = gtk_widget_focus_child;

  class->compute_child_allocation = gtk_frame_real_compute_child_allocation;

  frame_props[PROP_LABEL] =
      g_param_spec_string ("label",
                           P_("Label"),
                           P_("Text of the frame’s label"),
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  frame_props[PROP_LABEL_XALIGN] =
      g_param_spec_float ("label-xalign",
                          P_("Label xalign"),
                          P_("The horizontal alignment of the label"),
                          0.0, 1.0,
                          0.0,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  frame_props[PROP_LABEL_WIDGET] =
      g_param_spec_object ("label-widget",
                           P_("Label widget"),
                           P_("A widget to display in place of the usual frame label"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE);

  frame_props[PROP_CHILD] =
      g_param_spec_object ("child",
                           P_("Child"),
                           P_("The child widget"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, LAST_PROP, frame_props);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_FRAME_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("frame"));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_frame_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_frame_buildable_add_child;
}

static void
gtk_frame_buildable_add_child (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               GObject      *child,
                               const gchar  *type)
{
  if (type && strcmp (type, "label") == 0)
    gtk_frame_set_label_widget (GTK_FRAME (buildable), GTK_WIDGET (child));
  else if (GTK_IS_WIDGET (child))
    gtk_frame_set_child (GTK_FRAME (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_frame_init (GtkFrame *frame)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  priv->label_widget = NULL;
  priv->has_frame = TRUE;
  priv->label_xalign = 0.0;
}

static void
gtk_frame_dispose (GObject *object)
{
  GtkFrame *frame = GTK_FRAME (object);
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_clear_pointer (&priv->label_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_frame_parent_class)->dispose (object);
}

static void
gtk_frame_set_property (GObject         *object,
                        guint            prop_id,
                        const GValue    *value,
                        GParamSpec      *pspec)
{
  GtkFrame *frame = GTK_FRAME (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_frame_set_label (frame, g_value_get_string (value));
      break;
    case PROP_LABEL_XALIGN:
      gtk_frame_set_label_align (frame, g_value_get_float (value));
     break;
    case PROP_LABEL_WIDGET:
      gtk_frame_set_label_widget (frame, g_value_get_object (value));
      break;
    case PROP_CHILD:
      gtk_frame_set_child (frame, g_value_get_object (value));
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
  GtkFrame *frame = GTK_FRAME (object);
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_frame_get_label (frame));
      break;
    case PROP_LABEL_XALIGN:
      g_value_set_float (value, priv->label_xalign);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
                          priv->label_widget ?
                          G_OBJECT (priv->label_widget) : NULL);
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_frame_get_child (frame));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_frame_new:
 * @label: (allow-none): the text to use as the label of the frame
 * 
 * Creates a new #GtkFrame, with optional label @label.
 * If @label is %NULL, the label is omitted.
 * 
 * Returns: a new #GtkFrame widget
 **/
GtkWidget*
gtk_frame_new (const gchar *label)
{
  return g_object_new (GTK_TYPE_FRAME, "label", label, NULL);
}

/**
 * gtk_frame_set_label:
 * @frame: a #GtkFrame
 * @label: (allow-none): the text to use as the label of the frame
 *
 * Removes the current #GtkFrame:label-widget. If @label is not %NULL, creates a
 * new #GtkLabel with that text and adds it as the #GtkFrame:label-widget.
 **/
void
gtk_frame_set_label (GtkFrame *frame,
                     const gchar *label)
{
  g_return_if_fail (GTK_IS_FRAME (frame));

  if (!label)
    gtk_frame_set_label_widget (frame, NULL);
  else
    gtk_frame_set_label_widget (frame, gtk_label_new (label));
}

/**
 * gtk_frame_get_label:
 * @frame: a #GtkFrame
 * 
 * If the frame’s label widget is a #GtkLabel, returns the
 * text in the label widget. (The frame will have a #GtkLabel
 * for the label widget if a non-%NULL argument was passed
 * to gtk_frame_new().)
 * 
 * Returns: (nullable): the text in the label, or %NULL if there
 *               was no label widget or the lable widget was not
 *               a #GtkLabel. This string is owned by GTK+ and
 *               must not be modified or freed.
 **/
const gchar *
gtk_frame_get_label (GtkFrame *frame)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  if (GTK_IS_LABEL (priv->label_widget))
    return gtk_label_get_text (GTK_LABEL (priv->label_widget));
  else
    return NULL;
}

/**
 * gtk_frame_set_label_widget:
 * @frame: a #GtkFrame
 * @label_widget: (nullable): the new label widget
 * 
 * Sets the #GtkFrame:label-widget for the frame. This is the widget that
 * will appear embedded in the top edge of the frame as a title.
 **/
void
gtk_frame_set_label_widget (GtkFrame  *frame,
                            GtkWidget *label_widget)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);
  gboolean need_resize = FALSE;

  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || gtk_widget_get_parent (label_widget) == NULL);

  if (priv->label_widget == label_widget)
    return;

  if (priv->label_widget)
    {
      need_resize = gtk_widget_get_visible (priv->label_widget);
      gtk_widget_unparent (priv->label_widget);
    }

  priv->label_widget = label_widget;

  if (label_widget)
    {
      priv->label_widget = label_widget;
      gtk_widget_set_parent (label_widget, GTK_WIDGET (frame));
      need_resize |= gtk_widget_get_visible (label_widget);
    }

  if (gtk_widget_get_visible (GTK_WIDGET (frame)) && need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (frame));

  g_object_freeze_notify (G_OBJECT (frame));
  g_object_notify_by_pspec (G_OBJECT (frame), frame_props[PROP_LABEL_WIDGET]);
  g_object_notify_by_pspec (G_OBJECT (frame),  frame_props[PROP_LABEL]);
  g_object_thaw_notify (G_OBJECT (frame));
}

/**
 * gtk_frame_get_label_widget:
 * @frame: a #GtkFrame
 *
 * Retrieves the label widget for the frame. See
 * gtk_frame_set_label_widget().
 *
 * Returns: (nullable) (transfer none): the label widget, or %NULL if
 * there is none.
 **/
GtkWidget *
gtk_frame_get_label_widget (GtkFrame *frame)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  return priv->label_widget;
}

/**
 * gtk_frame_set_label_align:
 * @frame: a #GtkFrame
 * @xalign: The position of the label along the top edge
 *   of the widget. A value of 0.0 represents left alignment;
 *   1.0 represents right alignment.
 *
 * Sets the X alignment of the frame widget’s label. The
 * default value for a newly created frame is 0.0.
 **/
void
gtk_frame_set_label_align (GtkFrame *frame,
                           gfloat    xalign)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_return_if_fail (GTK_IS_FRAME (frame));

  xalign = CLAMP (xalign, 0.0, 1.0);

  g_object_freeze_notify (G_OBJECT (frame));
  if (xalign != priv->label_xalign)
    {
      priv->label_xalign = xalign;
      g_object_notify_by_pspec (G_OBJECT (frame), frame_props[PROP_LABEL_XALIGN]);
    }

  g_object_thaw_notify (G_OBJECT (frame));
  gtk_widget_queue_resize (GTK_WIDGET (frame));
}

/**
 * gtk_frame_get_label_align:
 * @frame: a #GtkFrame
 * 
 * Retrieves the X alignment of the frame’s label. See
 * gtk_frame_set_label_align().
 **/
gfloat
gtk_frame_get_label_align (GtkFrame *frame)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_return_val_if_fail (GTK_IS_FRAME (frame), 0.0);

  return priv->label_xalign;
}

static void
gtk_frame_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);
  GtkAllocation new_allocation;

  gtk_frame_compute_child_allocation (frame, &new_allocation);

  if (priv->label_widget &&
      gtk_widget_get_visible (priv->label_widget))
    {
      GtkAllocation label_allocation;
      int nat_width, label_width, label_height;
      float xalign;

      if (_gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        xalign = priv->label_xalign;
      else
        xalign = 1 - priv->label_xalign;

      gtk_widget_measure (priv->label_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          NULL, &nat_width, NULL, NULL);
      label_width = MIN (new_allocation.width, nat_width);
      gtk_widget_measure (priv->label_widget, GTK_ORIENTATION_VERTICAL, width,
                          &label_height, NULL, NULL, NULL);

      label_allocation.x = new_allocation.x + (new_allocation.width - width) * xalign;
      label_allocation.y = new_allocation.y - label_height;
      label_allocation.height = label_height;
      label_allocation.width = label_width;

      gtk_widget_size_allocate (priv->label_widget, &label_allocation, -1);
    }

  if (priv->child && gtk_widget_get_visible (priv->child))
    gtk_widget_size_allocate (priv->child, &new_allocation, -1);
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
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);
  int frame_width, frame_height;
  gint height;

  frame_width = gtk_widget_get_width (GTK_WIDGET (frame));
  frame_height = gtk_widget_get_height (GTK_WIDGET (frame));

  if (priv->label_widget)
    {
      gint nat_width, width;

      gtk_widget_measure (priv->label_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          NULL, &nat_width, NULL, NULL);
      width = MIN (frame_width, nat_width);
      gtk_widget_measure (priv->label_widget, GTK_ORIENTATION_VERTICAL, width,
                          &height, NULL, NULL, NULL);
    }
  else
    height = 0;

  child_allocation->x = 0;
  child_allocation->y = height;
  child_allocation->width = MAX (1, frame_width);
  child_allocation->height = MAX (1, frame_height - height);
}

static void
gtk_frame_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   gint            for_size,
                   gint            *minimum,
                   gint            *natural,
                   gint            *minimum_baseline,
                   gint            *natural_baseline)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);
  int child_min, child_nat;

  if (priv->child && gtk_widget_get_visible (priv->child))
    {
      gtk_widget_measure (priv->child,
                          orientation, for_size,
                          &child_min, &child_nat,
                          NULL, NULL);

      *minimum = child_min;
      *natural = child_nat;
    }
  else
    {
      *minimum = 0;
      *natural = 0;
    }

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_measure (priv->label_widget,
                              orientation, -1,
                              &child_min, &child_nat,
                              NULL, NULL);

          *minimum = MAX (child_min, *minimum);
          *natural = MAX (child_nat, *natural);
        }
      else
        {
          gtk_widget_measure (priv->label_widget,
                              orientation, for_size,
                              &child_min, &child_nat,
                              NULL, NULL);

          *minimum += child_min;
          *natural += child_nat;
        }
    }
}

static void
gtk_frame_compute_expand (GtkWidget *widget,
                          gboolean  *hexpand,
                          gboolean  *vexpand)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  if (priv->child)
    {
      *hexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (priv->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static GtkSizeRequestMode
gtk_frame_get_request_mode (GtkWidget *widget)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  if (priv->child)
    return gtk_widget_get_request_mode (priv->child);
  else
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

/**
 * gtk_frame_set_child:
 * @frame: a #GtkFrame
 * @child: (allow-none): the child widget
 *
 * Sets the child widget of @frame.
 */
void
gtk_frame_set_child (GtkFrame  *frame,
                     GtkWidget *child)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (priv->child == child)
    return;

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  if (child)
    {
      priv->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (frame));
    }

  g_object_notify_by_pspec (G_OBJECT (frame), frame_props[PROP_CHILD]);
}

/**
 * gtk_frame_get_child:
 * @frame: a #GtkFrame
 *
 * Gets the child widget of @frame.
 *
 * Returns: (nullable) (transfer none): the child widget of @frame
 */
GtkWidget *
gtk_frame_get_child (GtkFrame *frame)
{
  GtkFramePrivate *priv = gtk_frame_get_instance_private (frame);

  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  return priv->child;
}
