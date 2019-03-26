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

/**
 * SECTION:gtkalignment
 * @Short_description: A widget which controls the alignment and size of its child
 * @Title: GtkAlignment
 *
 * The #GtkAlignment widget controls the alignment and size of its child widget.
 * It has four settings: xscale, yscale, xalign, and yalign.
 *
 * The scale settings are used to specify how much the child widget should
 * expand to fill the space allocated to the #GtkAlignment.
 * The values can range from 0 (meaning the child doesn’t expand at all) to
 * 1 (meaning the child expands to fill all of the available space).
 *
 * The align settings are used to place the child widget within the available
 * area. The values range from 0 (top or left) to 1 (bottom or right).
 * Of course, if the scale settings are both set to 1, the alignment settings
 * have no effect.
 *
 * GtkAlignment has been deprecated in 3.14 and should not be used in
 * newly-written code. The desired effect can be achieved by using the
 * #GtkWidget:halign, #GtkWidget:valign and #GtkWidget:margin properties on the
 * child widget.
 */

#include "config.h"
#include "gtkalignment.h"
#include "gtksizerequest.h"
#include "gtkprivate.h"
#include "gtkintl.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS


struct _GtkAlignmentPrivate
{
  gfloat        xalign;
  gfloat        yalign;
  gfloat        xscale;
  gfloat        yscale;

  guint         padding_bottom;
  guint         padding_top;
  guint         padding_left;
  guint         padding_right;
};

enum {
  PROP_0,

  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XSCALE,
  PROP_YSCALE,

  PROP_TOP_PADDING,
  PROP_BOTTOM_PADDING,
  PROP_LEFT_PADDING,
  PROP_RIGHT_PADDING
};

static void gtk_alignment_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void gtk_alignment_set_property (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_alignment_get_property (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

static void gtk_alignment_get_preferred_width          (GtkWidget           *widget,
                                                        gint                *minimum_size,
                                                        gint                *natural_size);
static void gtk_alignment_get_preferred_height         (GtkWidget           *widget,
                                                        gint                *minimum_size,
                                                        gint                *natural_size);
static void gtk_alignment_get_preferred_width_for_height (GtkWidget           *widget,
							  gint                 for_size,
							  gint                *minimum_size,
							  gint                *natural_size);
static void gtk_alignment_get_preferred_height_for_width (GtkWidget           *widget,
							  gint                 for_size,
							  gint                *minimum_size,
							  gint                *natural_size);
static void gtk_alignment_get_preferred_height_and_baseline_for_width (GtkWidget           *widget,
								       gint                 for_size,
								       gint                *minimum_size,
								       gint                *natural_size,
								       gint                *minimum_baseline,
								       gint                *natural_baseline);

G_DEFINE_TYPE_WITH_PRIVATE (GtkAlignment, gtk_alignment, GTK_TYPE_BIN)

static void
gtk_alignment_class_init (GtkAlignmentClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  gobject_class->set_property = gtk_alignment_set_property;
  gobject_class->get_property = gtk_alignment_get_property;

  widget_class->size_allocate        = gtk_alignment_size_allocate;
  widget_class->get_preferred_width  = gtk_alignment_get_preferred_width;
  widget_class->get_preferred_height = gtk_alignment_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_alignment_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_alignment_get_preferred_height_for_width;
  widget_class->get_preferred_height_and_baseline_for_width = gtk_alignment_get_preferred_height_and_baseline_for_width;

  /**
   * GtkAlignment:xalign:
   *
   * Horizontal position of child in available space. A value of 0.0
   * will flush the child left (or right, in RTL locales); a value
   * of 1.0 will flush the child right (or left, in RTL locales).
   *
   * Deprecated: 3.14: Use gtk_widget_set_halign() on the child instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float("xalign",
                                                      P_("Horizontal alignment"),
                                                      P_("Horizontal position of child in available space. 0.0 is left aligned, 1.0 is right aligned"),
                                                      0.0,
                                                      1.0,
                                                      0.5,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));
   
  /**
   * GtkAlignment:yalign:
   *
   * Vertical position of child in available space. A value of 0.0
   * will flush the child to the top; a value of 1.0 will flush the
   * child to the bottom.
   *
   * Deprecated: 3.14: Use gtk_widget_set_valign() on the child instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float("yalign",
                                                      P_("Vertical alignment"),
                                                      P_("Vertical position of child in available space. 0.0 is top aligned, 1.0 is bottom aligned"),
                                                      0.0,
                                                      1.0,
						      0.5,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));
  /**
   * GtkAlignment:xscale:
   *
   * If available horizontal space is bigger than needed, how much
   * of it to use for the child. A value of 0.0 means none; a value
   * of 1.0 means all.
   *
   * Deprecated: 3.14: Use gtk_widget_set_hexpand() on the child instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_XSCALE,
                                   g_param_spec_float("xscale",
                                                      P_("Horizontal scale"),
                                                      P_("If available horizontal space is bigger than needed for the child, how much of it to use for the child. 0.0 means none, 1.0 means all"),
                                                      0.0,
                                                      1.0,
                                                      1.0,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));
  /**
   * GtkAlignment:yscale:
   *
   * If available vertical space is bigger than needed, how much
   * of it to use for the child. A value of 0.0 means none; a value
   * of 1.0 means all.
   *
   * Deprecated: 3.14: Use gtk_widget_set_vexpand() on the child instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YSCALE,
                                   g_param_spec_float("yscale",
                                                      P_("Vertical scale"),
                                                      P_("If available vertical space is bigger than needed for the child, how much of it to use for the child. 0.0 means none, 1.0 means all"),
                                                      0.0,
                                                      1.0,
                                                      1.0,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));


/**
 * GtkAlignment:top-padding:
 *
 * The padding to insert at the top of the widget.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Use gtk_widget_set_margin_top() instead
 */
  g_object_class_install_property (gobject_class,
                                   PROP_TOP_PADDING,
                                   g_param_spec_uint("top-padding",
                                                      P_("Top Padding"),
                                                      P_("The padding to insert at the top of the widget."),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));

/**
 * GtkAlignment:bottom-padding:
 *
 * The padding to insert at the bottom of the widget.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Use gtk_widget_set_margin_bottom() instead
 */
  g_object_class_install_property (gobject_class,
                                   PROP_BOTTOM_PADDING,
                                   g_param_spec_uint("bottom-padding",
                                                      P_("Bottom Padding"),
                                                      P_("The padding to insert at the bottom of the widget."),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));

/**
 * GtkAlignment:left-padding:
 *
 * The padding to insert at the left of the widget.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Use gtk_widget_set_margin_start() instead
 */
  g_object_class_install_property (gobject_class,
                                   PROP_LEFT_PADDING,
                                   g_param_spec_uint("left-padding",
                                                      P_("Left Padding"),
                                                      P_("The padding to insert at the left of the widget."),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));

/**
 * GtkAlignment:right-padding:
 *
 * The padding to insert at the right of the widget.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Use gtk_widget_set_margin_end() instead
 */
  g_object_class_install_property (gobject_class,
                                   PROP_RIGHT_PADDING,
                                   g_param_spec_uint("right-padding",
                                                      P_("Right Padding"),
                                                      P_("The padding to insert at the right of the widget."),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE|G_PARAM_DEPRECATED));
}

static void
gtk_alignment_init (GtkAlignment *alignment)
{
  GtkAlignmentPrivate *priv;

  alignment->priv = gtk_alignment_get_instance_private (alignment);
  priv = alignment->priv;

  gtk_widget_set_has_window (GTK_WIDGET (alignment), FALSE);

  priv->xalign = 0.5;
  priv->yalign = 0.5;
  priv->xscale = 1.0;
  priv->yscale = 1.0;

  /* Initialize padding with default values: */
  priv->padding_top = 0;
  priv->padding_bottom = 0;
  priv->padding_left = 0;
  priv->padding_right = 0;
}

/**
 * gtk_alignment_new:
 * @xalign: the horizontal alignment of the child widget, from 0 (left) to 1
 *  (right).
 * @yalign: the vertical alignment of the child widget, from 0 (top) to 1
 *  (bottom).
 * @xscale: the amount that the child widget expands horizontally to fill up
 *  unused space, from 0 to 1.
 *  A value of 0 indicates that the child widget should never expand.
 *  A value of 1 indicates that the child widget will expand to fill all of the
 *  space allocated for the #GtkAlignment.
 * @yscale: the amount that the child widget expands vertically to fill up
 *  unused space, from 0 to 1. The values are similar to @xscale.
 *
 * Creates a new #GtkAlignment.
 *
 * Returns: the new #GtkAlignment
 *
 * Deprecated: 3.14: Use #GtkWidget alignment and margin properties
 */
GtkWidget*
gtk_alignment_new (gfloat xalign,
		   gfloat yalign,
		   gfloat xscale,
		   gfloat yscale)
{
  GtkAlignment *alignment;
  GtkAlignmentPrivate *priv;

  alignment = g_object_new (GTK_TYPE_ALIGNMENT, NULL);

  priv = alignment->priv;

  priv->xalign = CLAMP (xalign, 0.0, 1.0);
  priv->yalign = CLAMP (yalign, 0.0, 1.0);
  priv->xscale = CLAMP (xscale, 0.0, 1.0);
  priv->yscale = CLAMP (yscale, 0.0, 1.0);

  return GTK_WIDGET (alignment);
}

static void
gtk_alignment_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkAlignment *alignment = GTK_ALIGNMENT (object);
  GtkAlignmentPrivate *priv = alignment->priv;

  switch (prop_id)
    {
    case PROP_XALIGN:
      gtk_alignment_set (alignment,
			 g_value_get_float (value),
			 priv->yalign,
			 priv->xscale,
			 priv->yscale);
      break;
    case PROP_YALIGN:
      gtk_alignment_set (alignment,
			 priv->xalign,
			 g_value_get_float (value),
			 priv->xscale,
			 priv->yscale);
      break;
    case PROP_XSCALE:
      gtk_alignment_set (alignment,
			 priv->xalign,
			 priv->yalign,
			 g_value_get_float (value),
			 priv->yscale);
      break;
    case PROP_YSCALE:
      gtk_alignment_set (alignment,
			 priv->xalign,
			 priv->yalign,
			 priv->xscale,
			 g_value_get_float (value));
      break;
      
    /* Padding: */
    case PROP_TOP_PADDING:
      gtk_alignment_set_padding (alignment,
			 g_value_get_uint (value),
			 priv->padding_bottom,
			 priv->padding_left,
			 priv->padding_right);
      break;
    case PROP_BOTTOM_PADDING:
      gtk_alignment_set_padding (alignment,
			 priv->padding_top,
			 g_value_get_uint (value),
			 priv->padding_left,
			 priv->padding_right);
      break;
    case PROP_LEFT_PADDING:
      gtk_alignment_set_padding (alignment,
			 priv->padding_top,
			 priv->padding_bottom,
			 g_value_get_uint (value),
			 priv->padding_right);
      break;
    case PROP_RIGHT_PADDING:
      gtk_alignment_set_padding (alignment,
			 priv->padding_top,
			 priv->padding_bottom,
			 priv->padding_left,
			 g_value_get_uint (value));
      break;
    
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_alignment_get_property (GObject         *object,
			    guint            prop_id,
			    GValue          *value,
			    GParamSpec      *pspec)
{
  GtkAlignment *alignment = GTK_ALIGNMENT (object);
  GtkAlignmentPrivate *priv = alignment->priv;

  switch (prop_id)
    {
    case PROP_XALIGN:
      g_value_set_float(value, priv->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float(value, priv->yalign);
      break;
    case PROP_XSCALE:
      g_value_set_float(value, priv->xscale);
      break;
    case PROP_YSCALE:
      g_value_set_float(value, priv->yscale);
      break;

    /* Padding: */
    case PROP_TOP_PADDING:
      g_value_set_uint (value, priv->padding_top);
      break;
    case PROP_BOTTOM_PADDING:
      g_value_set_uint (value, priv->padding_bottom);
      break;
    case PROP_LEFT_PADDING:
      g_value_set_uint (value, priv->padding_left);
      break;
    case PROP_RIGHT_PADDING:
      g_value_set_uint (value, priv->padding_right);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_alignment_set:
 * @alignment: a #GtkAlignment.
 * @xalign: the horizontal alignment of the child widget, from 0 (left) to 1
 *  (right).
 * @yalign: the vertical alignment of the child widget, from 0 (top) to 1
 *  (bottom).
 * @xscale: the amount that the child widget expands horizontally to fill up
 *  unused space, from 0 to 1.
 *  A value of 0 indicates that the child widget should never expand.
 *  A value of 1 indicates that the child widget will expand to fill all of the
 *  space allocated for the #GtkAlignment.
 * @yscale: the amount that the child widget expands vertically to fill up
 *  unused space, from 0 to 1. The values are similar to @xscale.
 *
 * Sets the #GtkAlignment values.
 *
 * Deprecated: 3.14: Use #GtkWidget alignment and margin properties
 */
void
gtk_alignment_set (GtkAlignment *alignment,
		   gfloat        xalign,
		   gfloat        yalign,
		   gfloat        xscale,
		   gfloat        yscale)
{
  GtkAlignmentPrivate *priv;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_ALIGNMENT (alignment));

  priv = alignment->priv;

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);
  xscale = CLAMP (xscale, 0.0, 1.0);
  yscale = CLAMP (yscale, 0.0, 1.0);

  if (   (priv->xalign != xalign)
      || (priv->yalign != yalign)
      || (priv->xscale != xscale)
      || (priv->yscale != yscale))
    {
      g_object_freeze_notify (G_OBJECT (alignment));
      if (priv->xalign != xalign)
        {
           priv->xalign = xalign;
           g_object_notify (G_OBJECT (alignment), "xalign");
        }
      if (priv->yalign != yalign)
        {
           priv->yalign = yalign;
           g_object_notify (G_OBJECT (alignment), "yalign");
        }
      if (priv->xscale != xscale)
        {
           priv->xscale = xscale;
           g_object_notify (G_OBJECT (alignment), "xscale");
        }
      if (priv->yscale != yscale)
        {
           priv->yscale = yscale;
           g_object_notify (G_OBJECT (alignment), "yscale");
        }
      g_object_thaw_notify (G_OBJECT (alignment));

      child = gtk_bin_get_child (GTK_BIN (alignment));
      if (child)
        gtk_widget_queue_resize (child);
      gtk_widget_queue_draw (GTK_WIDGET (alignment));
    }
}


static void
gtk_alignment_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkAlignment *alignment = GTK_ALIGNMENT (widget);
  GtkAlignmentPrivate *priv = alignment->priv;
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkWidget *child;
  gint width, height;
  gint border_width;
  gint baseline;

  gtk_widget_set_allocation (widget, allocation);
  bin = GTK_BIN (widget);

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      gint padding_horizontal, padding_vertical;
      gint child_nat_width;
      gint child_nat_height;
      gint child_width, child_height;
      double yalign, yscale;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (alignment));

      padding_horizontal = priv->padding_left + priv->padding_right;
      padding_vertical = priv->padding_top + priv->padding_bottom;

      width  = MAX (1, allocation->width - padding_horizontal - 2 * border_width);
      height = MAX (1, allocation->height - padding_vertical - 2 * border_width);

      baseline = gtk_widget_get_allocated_baseline (widget);
      if (baseline != -1)
	baseline -= border_width + priv->padding_top;

      /* If we get a baseline set that means we're baseline aligned, and the parent
	 honored that. In that case we have to ignore yalign/yscale as we need
	 yalign based on the baseline and always FILL mode to ensure we can place
	 the baseline anywhere */
      if (baseline != -1)
	{
	  yalign = 0;
	  yscale = 1.0;
	}
      else
	{
	  yalign = priv->yalign;
	  yscale = priv->yscale;
	}

      if (gtk_widget_get_request_mode (child) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
	{
	  gtk_widget_get_preferred_width (child, NULL, &child_nat_width);

	  child_width = MIN (width, child_nat_width);

	  gtk_widget_get_preferred_height_for_width (child, child_width, NULL, &child_nat_height);

	  child_height = MIN (height, child_nat_height);
	}
      else
	{
	  gtk_widget_get_preferred_height (child, NULL, &child_nat_height);

	  child_height = MIN (height, child_nat_height);

	  gtk_widget_get_preferred_width_for_height (child, child_height, NULL, &child_nat_width);

	  child_width = MIN (width, child_nat_width);
	}

      if (width > child_width)
	child_allocation.width = (child_width *
				  (1.0 - priv->xscale) +
				  width * priv->xscale);
      else
	child_allocation.width = width;

      if (height > child_height)
	child_allocation.height = (child_height *
				   (1.0 - yscale) +
				   height * yscale);
      else
	child_allocation.height = height;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	child_allocation.x = (1.0 - priv->xalign) * (width - child_allocation.width) + allocation->x + border_width + priv->padding_right;
      else 
	child_allocation.x = priv->xalign * (width - child_allocation.width) + allocation->x + border_width + priv->padding_left;

      child_allocation.y = yalign * (height - child_allocation.height) + allocation->y + border_width + priv->padding_top;

      gtk_widget_size_allocate_with_baseline (child, &child_allocation, baseline);
    }
}


static void
gtk_alignment_get_preferred_size (GtkWidget      *widget,
                                  GtkOrientation  orientation,
				  gint            for_size,
                                  gint           *minimum_size,
                                  gint           *natural_size,
                                  gint           *minimum_baseline,
                                  gint           *natural_baseline)
{
  GtkAlignment *alignment = GTK_ALIGNMENT (widget);
  GtkAlignmentPrivate *priv = alignment->priv;
  GtkWidget *child;
  guint minimum, natural;
  guint top_offset;
  guint border;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  border = gtk_container_get_border_width (GTK_CONTAINER (widget));
  natural = minimum = border * 2;
  top_offset = border;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;
      gint child_min_baseline = -1, child_nat_baseline = -1;

      /* Request extra space for the padding: */
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  minimum += (priv->padding_left + priv->padding_right);

	  if (for_size < 0)
	    gtk_widget_get_preferred_width (child, &child_min, &child_nat);
	  else
	    {
	      gint min_height;

	      gtk_widget_get_preferred_height (child, &min_height, NULL);

	      for_size -= (priv->padding_top + priv->padding_bottom);

	      if (for_size > min_height)
		for_size = (min_height * (1.0 - priv->yscale) +
			    for_size * priv->yscale);

	      gtk_widget_get_preferred_width_for_height (child, for_size, &child_min, &child_nat);
	    }
	}
      else
	{
	  minimum += (priv->padding_top + priv->padding_bottom);
	  top_offset += priv->padding_top;

	  if (for_size < 0)
	    gtk_widget_get_preferred_height_and_baseline_for_width (child, -1, &child_min, &child_nat, &child_min_baseline, &child_nat_baseline);
	  else
	    {
	      gint min_width;

	      gtk_widget_get_preferred_width (child, &min_width, NULL);

	      for_size -= (priv->padding_left + priv->padding_right);

	      if (for_size > min_width)
		for_size = (min_width * (1.0 - priv->xscale) +
			    for_size * priv->xscale);

	      gtk_widget_get_preferred_height_and_baseline_for_width (child, for_size, &child_min, &child_nat, &child_min_baseline, &child_nat_baseline);
	    }

	  if (minimum_baseline && child_min_baseline >= 0)
	    *minimum_baseline = child_min_baseline + top_offset;
	  if (natural_baseline && child_nat_baseline >= 0)
	    *natural_baseline = child_nat_baseline + top_offset;
	}

      natural = minimum;

      minimum += child_min;
      natural += child_nat;
    }

  *minimum_size = minimum;
  *natural_size = natural;
}

static void
gtk_alignment_get_preferred_width (GtkWidget      *widget,
                                   gint           *minimum_size,
                                   gint           *natural_size)
{
  gtk_alignment_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, -1, minimum_size, natural_size, NULL, NULL);
}

static void
gtk_alignment_get_preferred_height (GtkWidget      *widget,
                                    gint           *minimum_size,
                                    gint           *natural_size)
{
  gtk_alignment_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, -1, minimum_size, natural_size, NULL, NULL);
}


static void 
gtk_alignment_get_preferred_width_for_height (GtkWidget           *widget,
					      gint                 for_size,
					      gint                *minimum_size,
					      gint                *natural_size)
{
  gtk_alignment_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, for_size, minimum_size, natural_size, NULL, NULL);
}

static void
gtk_alignment_get_preferred_height_for_width (GtkWidget           *widget,
					      gint                 for_size,
					      gint                *minimum_size,
					      gint                *natural_size)
{
  gtk_alignment_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, for_size, minimum_size, natural_size, NULL, NULL);
}

static void
gtk_alignment_get_preferred_height_and_baseline_for_width (GtkWidget           *widget,
							   gint                 for_size,
							   gint                *minimum_size,
							   gint                *natural_size,
							   gint                *minimum_baseline,
							   gint                *natural_baseline)
{
  gtk_alignment_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, for_size, minimum_size, natural_size, minimum_baseline, natural_baseline);
}


/**
 * gtk_alignment_set_padding:
 * @alignment: a #GtkAlignment
 * @padding_top: the padding at the top of the widget
 * @padding_bottom: the padding at the bottom of the widget
 * @padding_left: the padding at the left of the widget
 * @padding_right: the padding at the right of the widget.
 *
 * Sets the padding on the different sides of the widget.
 * The padding adds blank space to the sides of the widget. For instance,
 * this can be used to indent the child widget towards the right by adding
 * padding on the left.
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Use #GtkWidget alignment and margin properties
 */
void
gtk_alignment_set_padding (GtkAlignment    *alignment,
			   guint            padding_top,
			   guint            padding_bottom,
			   guint            padding_left,
			   guint            padding_right)
{
  GtkAlignmentPrivate *priv;
  GtkWidget *child;
  
  g_return_if_fail (GTK_IS_ALIGNMENT (alignment));

  priv = alignment->priv;

  g_object_freeze_notify (G_OBJECT (alignment));

  if (priv->padding_top != padding_top)
    {
      priv->padding_top = padding_top;
      g_object_notify (G_OBJECT (alignment), "top-padding");
    }
  if (priv->padding_bottom != padding_bottom)
    {
      priv->padding_bottom = padding_bottom;
      g_object_notify (G_OBJECT (alignment), "bottom-padding");
    }
  if (priv->padding_left != padding_left)
    {
      priv->padding_left = padding_left;
      g_object_notify (G_OBJECT (alignment), "left-padding");
    }
  if (priv->padding_right != padding_right)
    {
      priv->padding_right = padding_right;
      g_object_notify (G_OBJECT (alignment), "right-padding");
    }

  g_object_thaw_notify (G_OBJECT (alignment));
  
  /* Make sure that the widget and children are redrawn with the new setting: */
  child = gtk_bin_get_child (GTK_BIN (alignment));
  if (child)
    gtk_widget_queue_resize (child);

  gtk_widget_queue_draw (GTK_WIDGET (alignment));
}

/**
 * gtk_alignment_get_padding:
 * @alignment: a #GtkAlignment
 * @padding_top: (out) (allow-none): location to store the padding for
 *     the top of the widget, or %NULL
 * @padding_bottom: (out) (allow-none): location to store the padding
 *     for the bottom of the widget, or %NULL
 * @padding_left: (out) (allow-none): location to store the padding
 *     for the left of the widget, or %NULL
 * @padding_right: (out) (allow-none): location to store the padding
 *     for the right of the widget, or %NULL
 *
 * Gets the padding on the different sides of the widget.
 * See gtk_alignment_set_padding ().
 *
 * Since: 2.4
 *
 * Deprecated: 3.14: Use #GtkWidget alignment and margin properties
 */
void
gtk_alignment_get_padding (GtkAlignment    *alignment,
			   guint           *padding_top,
			   guint           *padding_bottom,
			   guint           *padding_left,
			   guint           *padding_right)
{
  GtkAlignmentPrivate *priv;

  g_return_if_fail (GTK_IS_ALIGNMENT (alignment));

  priv = alignment->priv;

  if(padding_top)
    *padding_top = priv->padding_top;
  if(padding_bottom)
    *padding_bottom = priv->padding_bottom;
  if(padding_left)
    *padding_left = priv->padding_left;
  if(padding_right)
    *padding_right = priv->padding_right;
}
