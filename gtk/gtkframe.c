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
#include "gtkwidgetpath.h"

#include "a11y/gtkframeaccessible.h"

/**
 * SECTION:gtkframe
 * @Short_description: A bin with a decorative frame and optional label
 * @Title: GtkFrame
 *
 * The frame widget is a Bin that surrounds its child
 * with a decorative frame and an optional label.
 * If present, the label is drawn in a gap in the
 * top side of the frame. The position of the
 * label can be controlled with gtk_frame_set_label_align().
 *
 * <refsect2 id="GtkFrame-BUILDER-UI">
 * <title>GtkFrame as GtkBuildable</title>
 * <para>
 * The GtkFrame implementation of the GtkBuildable interface
 * supports placing a child in the label position by specifying
 * "label" as the "type" attribute of a &lt;child&gt; element.
 * A normal content child can be specified without specifying
 * a &lt;child&gt; type attribute.
 * </para>
 * <example>
 * <title>A UI definition fragment with GtkFrame</title>
 * <programlisting><![CDATA[
 * <object class="GtkFrame">
 *   <child type="label">
 *     <object class="GtkLabel" id="frame-label"/>
 *   </child>
 *   <child>
 *     <object class="GtkEntry" id="frame-content"/>
 *   </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </refsect2>
 */


#define LABEL_PAD 1
#define LABEL_SIDE_PAD 2

struct _GtkFramePrivate
{
  /* Properties */
  GtkWidget *label_widget;

  gint16 shadow_type;
  gfloat label_xalign;
  gfloat label_yalign;
  /* Properties */

  GtkAllocation child_allocation;
  GtkAllocation label_allocation;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_LABEL_XALIGN,
  PROP_LABEL_YALIGN,
  PROP_SHADOW_TYPE,
  PROP_LABEL_WIDGET
};

static void gtk_frame_set_property (GObject      *object,
				    guint         param_id,
				    const GValue *value,
				    GParamSpec   *pspec);
static void gtk_frame_get_property (GObject     *object,
				    guint        param_id,
				    GValue      *value,
				    GParamSpec  *pspec);
static gboolean gtk_frame_draw      (GtkWidget      *widget,
				     cairo_t        *cr);
static void gtk_frame_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);
static void gtk_frame_remove        (GtkContainer   *container,
				     GtkWidget      *child);
static void gtk_frame_forall        (GtkContainer   *container,
				     gboolean	     include_internals,
			             GtkCallback     callback,
			             gpointer        callback_data);
static GtkWidgetPath * gtk_frame_get_path_for_child (GtkContainer *container,
                                                     GtkWidget    *child);

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

static void gtk_frame_get_preferred_width           (GtkWidget           *widget,
                                                     gint                *minimum_size,
						     gint                *natural_size);
static void gtk_frame_get_preferred_height          (GtkWidget           *widget,
						     gint                *minimum_size,
						     gint                *natural_size);
static void gtk_frame_get_preferred_height_for_width(GtkWidget           *layout,
						     gint                 width,
						     gint                *minimum_height,
						     gint                *natural_height);
static void gtk_frame_get_preferred_width_for_height(GtkWidget           *layout,
						     gint                 width,
						     gint                *minimum_height,
						     gint                *natural_height);


G_DEFINE_TYPE_WITH_CODE (GtkFrame, gtk_frame, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkFrame)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_frame_buildable_init))

static void
gtk_frame_class_init (GtkFrameClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) class;
  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);

  gobject_class->set_property = gtk_frame_set_property;
  gobject_class->get_property = gtk_frame_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("Text of the frame's label"),
                                                        NULL,
                                                        GTK_PARAM_READABLE |
							GTK_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
				   PROP_LABEL_XALIGN,
				   g_param_spec_float ("label-xalign",
						       P_("Label xalign"),
						       P_("The horizontal alignment of the label"),
						       0.0,
						       1.0,
						       0.0,
						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_LABEL_YALIGN,
				   g_param_spec_float ("label-yalign",
						       P_("Label yalign"),
						       P_("The vertical alignment of the label"),
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Frame shadow"),
                                                      P_("Appearance of the frame border"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_ETCHED_IN,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_WIDGET,
                                   g_param_spec_object ("label-widget",
                                                        P_("Label widget"),
                                                        P_("A widget to display in place of the usual frame label"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  widget_class->draw                           = gtk_frame_draw;
  widget_class->size_allocate                  = gtk_frame_size_allocate;
  widget_class->get_preferred_width            = gtk_frame_get_preferred_width;
  widget_class->get_preferred_height           = gtk_frame_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_frame_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_frame_get_preferred_width_for_height;

  container_class->remove = gtk_frame_remove;
  container_class->forall = gtk_frame_forall;
  container_class->get_path_for_child = gtk_frame_get_path_for_child;

  class->compute_child_allocation = gtk_frame_real_compute_child_allocation;

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_FRAME_ACCESSIBLE);
}

static void
gtk_frame_buildable_init (GtkBuildableIface *iface)
{
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
  else if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_FRAME (buildable), type);
}

static void
gtk_frame_init (GtkFrame *frame)
{
  GtkFramePrivate *priv;
  GtkStyleContext *context;

  frame->priv = gtk_frame_get_instance_private (frame); 
  priv = frame->priv;

  priv->label_widget = NULL;
  priv->shadow_type = GTK_SHADOW_ETCHED_IN;
  priv->label_xalign = 0.0;
  priv->label_yalign = 0.5;

  context = gtk_widget_get_style_context (GTK_WIDGET (frame));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
}

static void 
gtk_frame_set_property (GObject         *object,
			guint            prop_id,
			const GValue    *value,
			GParamSpec      *pspec)
{
  GtkFrame *frame = GTK_FRAME (object);
  GtkFramePrivate *priv = frame->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_frame_set_label (frame, g_value_get_string (value));
      break;
    case PROP_LABEL_XALIGN:
      gtk_frame_set_label_align (frame, g_value_get_float (value), 
				 priv->label_yalign);
      break;
    case PROP_LABEL_YALIGN:
      gtk_frame_set_label_align (frame, priv->label_xalign,
				 g_value_get_float (value));
      break;
    case PROP_SHADOW_TYPE:
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
  GtkFrame *frame = GTK_FRAME (object);
  GtkFramePrivate *priv = frame->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_frame_get_label (frame));
      break;
    case PROP_LABEL_XALIGN:
      g_value_set_float (value, priv->label_xalign);
      break;
    case PROP_LABEL_YALIGN:
      g_value_set_float (value, priv->label_yalign);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
                          priv->label_widget ?
                          G_OBJECT (priv->label_widget) : NULL);
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
 * Return value: a new #GtkFrame widget
 **/
GtkWidget*
gtk_frame_new (const gchar *label)
{
  return g_object_new (GTK_TYPE_FRAME, "label", label, NULL);
}

static void
gtk_frame_remove (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkFrame *frame = GTK_FRAME (container);
  GtkFramePrivate *priv = frame->priv;

  if (priv->label_widget == child)
    gtk_frame_set_label_widget (frame, NULL);
  else
    GTK_CONTAINER_CLASS (gtk_frame_parent_class)->remove (container, child);
}

static void
gtk_frame_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkFrame *frame = GTK_FRAME (container);
  GtkFramePrivate *priv = frame->priv;
  GtkWidget *child;

  child = gtk_bin_get_child (bin);
  if (child)
    (* callback) (child, callback_data);

  if (priv->label_widget)
    (* callback) (priv->label_widget, callback_data);
}

static GtkWidgetPath *
gtk_frame_get_path_for_child (GtkContainer *container,
                              GtkWidget    *child)
{
  GtkFramePrivate *priv = GTK_FRAME (container)->priv;
  GtkWidgetPath *path;

  path = GTK_CONTAINER_CLASS (gtk_frame_parent_class)->get_path_for_child (container, child);

  if (child == priv->label_widget)
    gtk_widget_path_iter_add_class (path,
                                    gtk_widget_path_length (path) - 2,
                                    GTK_STYLE_CLASS_FRAME);

  return path;
}

/**
 * gtk_frame_set_label:
 * @frame: a #GtkFrame
 * @label: (allow-none): the text to use as the label of the frame
 *
 * Sets the text of the label. If @label is %NULL,
 * the current label is removed.
 **/
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
}

/**
 * gtk_frame_get_label:
 * @frame: a #GtkFrame
 * 
 * If the frame's label widget is a #GtkLabel, returns the
 * text in the label widget. (The frame will have a #GtkLabel
 * for the label widget if a non-%NULL argument was passed
 * to gtk_frame_new().)
 * 
 * Return value: the text in the label, or %NULL if there
 *               was no label widget or the lable widget was not
 *               a #GtkLabel. This string is owned by GTK+ and
 *               must not be modified or freed.
 **/
const gchar *
gtk_frame_get_label (GtkFrame *frame)
{
  GtkFramePrivate *priv;

  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  priv = frame->priv;

  if (GTK_IS_LABEL (priv->label_widget))
    return gtk_label_get_text (GTK_LABEL (priv->label_widget));
  else
    return NULL;
}

/**
 * gtk_frame_set_label_widget:
 * @frame: a #GtkFrame
 * @label_widget: the new label widget
 * 
 * Sets the label widget for the frame. This is the widget that
 * will appear embedded in the top edge of the frame as a
 * title.
 **/
void
gtk_frame_set_label_widget (GtkFrame  *frame,
			    GtkWidget *label_widget)
{
  GtkFramePrivate *priv;
  gboolean need_resize = FALSE;

  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || gtk_widget_get_parent (label_widget) == NULL);

  priv = frame->priv;

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
  g_object_notify (G_OBJECT (frame), "label-widget");
  g_object_notify (G_OBJECT (frame), "label");
  g_object_thaw_notify (G_OBJECT (frame));
}

/**
 * gtk_frame_get_label_widget:
 * @frame: a #GtkFrame
 *
 * Retrieves the label widget for the frame. See
 * gtk_frame_set_label_widget().
 *
 * Return value: (transfer none): the label widget, or %NULL if there is none.
 **/
GtkWidget *
gtk_frame_get_label_widget (GtkFrame *frame)
{
  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  return frame->priv->label_widget;
}

/**
 * gtk_frame_set_label_align:
 * @frame: a #GtkFrame
 * @xalign: The position of the label along the top edge
 *   of the widget. A value of 0.0 represents left alignment;
 *   1.0 represents right alignment.
 * @yalign: The y alignment of the label. A value of 0.0 aligns under 
 *   the frame; 1.0 aligns above the frame. If the values are exactly
 *   0.0 or 1.0 the gap in the frame won't be painted because the label
 *   will be completely above or below the frame.
 * 
 * Sets the alignment of the frame widget's label. The
 * default values for a newly created frame are 0.0 and 0.5.
 **/
void
gtk_frame_set_label_align (GtkFrame *frame,
			   gfloat    xalign,
			   gfloat    yalign)
{
  GtkFramePrivate *priv;

  g_return_if_fail (GTK_IS_FRAME (frame));

  priv = frame->priv;

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);

  g_object_freeze_notify (G_OBJECT (frame));
  if (xalign != priv->label_xalign)
    {
      priv->label_xalign = xalign;
      g_object_notify (G_OBJECT (frame), "label-xalign");
    }

  if (yalign != priv->label_yalign)
    {
      priv->label_yalign = yalign;
      g_object_notify (G_OBJECT (frame), "label-yalign");
    }

  g_object_thaw_notify (G_OBJECT (frame));
  gtk_widget_queue_resize (GTK_WIDGET (frame));
}

/**
 * gtk_frame_get_label_align:
 * @frame: a #GtkFrame
 * @xalign: (out) (allow-none): location to store X alignment of
 *     frame's label, or %NULL
 * @yalign: (out) (allow-none): location to store X alignment of
 *     frame's label, or %NULL
 * 
 * Retrieves the X and Y alignment of the frame's label. See
 * gtk_frame_set_label_align().
 **/
void
gtk_frame_get_label_align (GtkFrame *frame,
		           gfloat   *xalign,
			   gfloat   *yalign)
{
  GtkFramePrivate *priv;

  g_return_if_fail (GTK_IS_FRAME (frame));

  priv = frame->priv;

  if (xalign)
    *xalign = priv->label_xalign;
  if (yalign)
    *yalign = priv->label_yalign;
}

/**
 * gtk_frame_set_shadow_type:
 * @frame: a #GtkFrame
 * @type: the new #GtkShadowType
 * 
 * Sets the shadow type for @frame.
 **/
void
gtk_frame_set_shadow_type (GtkFrame      *frame,
			   GtkShadowType  type)
{
  GtkFramePrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_FRAME (frame));

  priv = frame->priv;

  if ((GtkShadowType) priv->shadow_type != type)
    {
      widget = GTK_WIDGET (frame);
      priv->shadow_type = type;
      g_object_notify (G_OBJECT (frame), "shadow-type");

      if (gtk_widget_is_drawable (widget))
	{
	  gtk_widget_queue_draw (widget);
	}
      
      gtk_widget_queue_resize (widget);
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

  return frame->priv->shadow_type;
}

static void
get_padding_and_border (GtkFrame *frame,
                        GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (GTK_WIDGET (frame));
  state = gtk_widget_get_state_flags (GTK_WIDGET (frame));

  gtk_style_context_get_padding (context, state, border);

  if (frame->priv->shadow_type != GTK_SHADOW_NONE)
    {
      GtkBorder tmp;

      gtk_style_context_get_border (context, state, &tmp);
      border->top += tmp.top;
      border->right += tmp.right;
      border->bottom += tmp.bottom;
      border->left += tmp.left;
    }
}

static gboolean
gtk_frame_draw (GtkWidget *widget,
		cairo_t   *cr)
{
  GtkFrame *frame;
  GtkFramePrivate *priv;
  GtkStyleContext *context;
  gint x, y, width, height;
  GtkAllocation allocation;
  GtkBorder padding;

  frame = GTK_FRAME (widget);
  priv = frame->priv;

  gtk_widget_get_allocation (widget, &allocation);
  get_padding_and_border (frame, &padding);
  context = gtk_widget_get_style_context (widget);

  x = priv->child_allocation.x - allocation.x - padding.left;
  y = priv->child_allocation.y - allocation.y - padding.top;
  width = priv->child_allocation.width + padding.left + padding.right;
  height =  priv->child_allocation.height + padding.top + padding.bottom;

  if (priv->shadow_type != GTK_SHADOW_NONE)
    {
      if (priv->label_widget)
        {
          gfloat xalign;
          gint height_extra;
          gint x2;

          if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
            xalign = priv->label_xalign;
          else
            xalign = 1 - priv->label_xalign;

          height_extra = MAX (0, priv->label_allocation.height - padding.top)
            - priv->label_yalign * priv->label_allocation.height;
          y -= height_extra;
          height += height_extra;

          x2 = padding.left + (priv->child_allocation.width - priv->label_allocation.width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD) * xalign + LABEL_SIDE_PAD;

          gtk_render_background (context, cr, x, y, width, height);

          /* If the label is completely over or under the frame we can omit the gap */
          if (priv->label_yalign == 0.0 || priv->label_yalign == 1.0)
            gtk_render_frame (context, cr, x, y, width, height);
          else
            gtk_render_frame_gap (context, cr,
                                  x, y, width, height,
                                  GTK_POS_TOP, x2,
                                  x2 + priv->label_allocation.width + 2 * LABEL_PAD);
        }
      else
        {
          gtk_render_background (context, cr, x, y, width, height);
          gtk_render_frame (context, cr, x, y, width, height);
        }
    }
  else
    {
      gtk_render_background (context, cr, x, y, width, height);
    }

  GTK_WIDGET_CLASS (gtk_frame_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gtk_frame_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkFramePrivate *priv = frame->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation new_allocation;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  gtk_frame_compute_child_allocation (frame, &new_allocation);
  
  /* If the child allocation changed, that means that the frame is drawn
   * in a new place, so we must redraw the entire widget.
   */
  if (gtk_widget_get_mapped (widget))
    {
      gdk_window_invalidate_rect (gtk_widget_get_window (widget), allocation, FALSE);
    }

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &new_allocation);

  priv->child_allocation = new_allocation;

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      GtkBorder padding;
      gint nat_width, width, height;
      gfloat xalign;

      get_padding_and_border (frame, &padding);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = priv->label_xalign;
      else
	xalign = 1 - priv->label_xalign;

      gtk_widget_get_preferred_width (priv->label_widget, NULL, &nat_width);
      width = new_allocation.width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD;
      width = MIN (width, nat_width);

      gtk_widget_get_preferred_height_for_width (priv->label_widget, width,
                                                 &height, NULL);


      priv->label_allocation.x = priv->child_allocation.x + LABEL_SIDE_PAD +
	(new_allocation.width - width - 2 * LABEL_PAD - 2 * LABEL_SIDE_PAD) * xalign + LABEL_PAD;

      priv->label_allocation.width = width;

      priv->label_allocation.y = priv->child_allocation.y - MAX (height, padding.top);
      priv->label_allocation.height = height;

      gtk_widget_size_allocate (priv->label_widget, &priv->label_allocation);
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
  GtkFramePrivate *priv = frame->priv;
  GtkWidget *widget = GTK_WIDGET (frame);
  GtkAllocation allocation;
  GtkBorder padding;
  gint top_margin;
  gint border_width;

  gtk_widget_get_allocation (widget, &allocation);
  get_padding_and_border (frame, &padding);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (frame));

  if (priv->label_widget)
    {
      gint nat_width, width, height;

      gtk_widget_get_preferred_width (priv->label_widget, NULL, &nat_width);

      width = allocation.width;
      width -= 2 * LABEL_PAD + 2 * LABEL_SIDE_PAD;
      width -= (border_width * 2) + padding.left + padding.right;

      width = MIN (width, nat_width);

      gtk_widget_get_preferred_height_for_width (priv->label_widget, width,
                                                 &height, NULL);

      top_margin = MAX (height, padding.top);
    }
  else
    top_margin = padding.top;

  child_allocation->x = border_width + padding.left;
  child_allocation->y = border_width + top_margin;
  child_allocation->width = MAX (1, (gint) (allocation.width - (border_width * 2) -
					    padding.left - padding.right));
  child_allocation->height = MAX (1, (gint) (allocation.height - child_allocation->y -
					     border_width - padding.bottom));

  child_allocation->x += allocation.x;
  child_allocation->y += allocation.y;
}

static void
gtk_frame_get_preferred_size (GtkWidget      *request,
                              GtkOrientation  orientation,
                              gint           *minimum_size,
                              gint           *natural_size)
{
  GtkFrame *frame = GTK_FRAME (request);
  GtkFramePrivate *priv = frame->priv;
  GtkBorder padding;
  GtkWidget *widget = GTK_WIDGET (request);
  GtkWidget *child;
  GtkBin *bin = GTK_BIN (widget);
  gint child_min, child_nat;
  gint minimum, natural;
  guint border_width;

  get_padding_and_border (frame, &padding);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_get_preferred_width (priv->label_widget,
                                          &child_min, &child_nat);
          minimum = child_min + 2 * LABEL_PAD + 2 * LABEL_SIDE_PAD;
          natural = child_nat + 2 * LABEL_PAD + 2 * LABEL_SIDE_PAD;
        }
      else
        {
          gtk_widget_get_preferred_height (priv->label_widget,
                                           &child_min, &child_nat);
          minimum = MAX (0, child_min - padding.top);
          natural = MAX (0, child_nat - padding.top);
        }
    }
  else
    {
      minimum = 0;
      natural = 0;
    }

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_get_preferred_width (child,
                                          &child_min, &child_nat);
          minimum = MAX (minimum, child_min);
          natural = MAX (natural, child_nat);
        }
      else
        {
          gtk_widget_get_preferred_height (child,
                                           &child_min, &child_nat);
          minimum += child_min;
          natural += child_nat;
        }
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum += (border_width * 2) + padding.left + padding.right;
      natural += (border_width * 2) + padding.left + padding.right;
    }
  else
    {
      minimum += (border_width * 2) + padding.top + padding.bottom;
      natural += (border_width * 2) + padding.top + padding.bottom;
    }

 if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void
gtk_frame_get_preferred_width (GtkWidget *widget,
                               gint      *minimum_size,
                               gint      *natural_size)
{
  gtk_frame_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size);
}

static void
gtk_frame_get_preferred_height (GtkWidget *widget,
                                gint      *minimum_size,
                                gint      *natural_size)
{
  gtk_frame_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size);
}


static void
gtk_frame_get_preferred_height_for_width (GtkWidget *request,
                                          gint       width,
                                          gint      *minimum_height,
                                          gint      *natural_height)
{
  GtkWidget *widget = GTK_WIDGET (request);
  GtkWidget *child;
  GtkFrame *frame = GTK_FRAME (widget);
  GtkFramePrivate *priv = frame->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkBorder padding;
  gint child_min, child_nat, label_width;
  gint minimum, natural;
  guint border_width;

  get_padding_and_border (frame, &padding);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  minimum = (border_width * 2) + padding.top + padding.bottom;
  natural = (border_width * 2) + padding.top + padding.bottom;

  width -= (border_width * 2) + padding.left + padding.right;
  label_width = width - 2 * LABEL_PAD + 2 * LABEL_SIDE_PAD;

  if (priv->label_widget && gtk_widget_get_visible (priv->label_widget))
    {
      gtk_widget_get_preferred_height_for_width (priv->label_widget,
                                                 label_width, &child_min, &child_nat);
      minimum += child_min;
      natural += child_nat;
    }

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_get_preferred_height_for_width (child,
                                                 width, &child_min, &child_nat);
      minimum += child_min;
      natural += child_nat;
    }

 if (minimum_height)
    *minimum_height = minimum;

  if (natural_height)
    *natural_height = natural;
}

static void
gtk_frame_get_preferred_width_for_height (GtkWidget *widget,
                                          gint       height,
                                          gint      *minimum_width,
                                          gint      *natural_width)
{
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_width, natural_width);
}

