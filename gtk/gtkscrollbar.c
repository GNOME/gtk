/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

#include "gtkscrollbar.h"
#include "gtkrangeprivate.h"

#include "gtkadjustment.h"
#include "gtkintl.h"
#include "gtkorientable.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkscrollbar
 * @Short_description: A Scrollbar
 * @Title: GtkScrollbar
 * @See_also: #GtkAdjustment, #GtkScrolledWindow
 *
 * The #GtkScrollbar widget is a horizontal or vertical scrollbar,
 * depending on the value of the #GtkOrientable:orientation property.
 *
 * Its position and movement are controlled by the adjustment that is passed to
 * or created by gtk_scrollbar_new(). See #GtkAdjustment for more details. The
 * #GtkAdjustment:value field sets the position of the thumb and must be between
 * #GtkAdjustment:lower and #GtkAdjustment:upper - #GtkAdjustment:page-size. The
 * #GtkAdjustment:page-size represents the size of the visible scrollable area.
 * The fields #GtkAdjustment:step-increment and #GtkAdjustment:page-increment
 * fields are added to or subtracted from the #GtkAdjustment:value when the user
 * asks to move by a step (using e.g. the cursor arrow keys or, if present, the
 * stepper buttons) or by a page (using e.g. the Page Down/Up keys).
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * scrollbar[.fine-tune]
 * ╰── contents
 *     ├── [button.up]
 *     ├── [button.down]
 *     ├── trough
 *     │   ╰── slider
 *     ├── [button.up]
 *     ╰── [button.down]
 * ]|
 *
 * GtkScrollbar has a main CSS node with name scrollbar and a subnode for its
 * contents, with subnodes named trough and slider.
 *
 * The main node gets the style class .fine-tune added when the scrollbar is
 * in 'fine-tuning' mode.
 *
 * If steppers are enabled, they are represented by up to four additional
 * subnodes with name button. These get the style classes .up and .down to
 * indicate in which direction they are moving.
 *
 * Other style classes that may be added to scrollbars inside #GtkScrolledWindow
 * include the positional classes (.left, .right, .top, .bottom) and style
 * classes related to overlay scrolling (.overlay-indicator, .dragging, .hovering).
 */


static void gtk_scrollbar_style_updated (GtkWidget *widget);

G_DEFINE_TYPE (GtkScrollbar, gtk_scrollbar, GTK_TYPE_RANGE)

static void
gtk_scrollbar_class_init (GtkScrollbarClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->style_updated = gtk_scrollbar_style_updated;

  /**
   * GtkScrollbar:min-slider-length:
   *
   * Minimum length of scrollbar slider.
   *
   * Deprecated: 3.20: Use min-height/min-width CSS properties on the slider
   *   element instead. The value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("min-slider-length",
							     P_("Minimum Slider Length"),
							     P_("Minimum length of scrollbar slider"),
							     0,
							     G_MAXINT,
							     21,
							     GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("fixed-slider-length",
                                                                 P_("Fixed slider size"),
                                                                 P_("Don't change slider size, just lock it to the minimum length"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-backward-stepper",
                                                                 P_("Backward stepper"),
                                                                 P_("Display the standard backward arrow button"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-forward-stepper",
                                                                 P_("Forward stepper"),
                                                                 P_("Display the standard forward arrow button"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-secondary-backward-stepper",
                                                                 P_("Secondary backward stepper"),
                                                                 P_("Display a second backward arrow button on the opposite end of the scrollbar"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-secondary-forward-stepper",
                                                                 P_("Secondary forward stepper"),
                                                                 P_("Display a second forward arrow button on the opposite end of the scrollbar"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_SCROLL_BAR);
  gtk_widget_class_set_css_name (widget_class, "scrollbar");
}

static void
gtk_scrollbar_update_style (GtkScrollbar *scrollbar)
{
  gboolean fixed_size;
  gboolean has_a, has_b, has_c, has_d;
  GtkRange *range = GTK_RANGE (scrollbar);
  GtkWidget *widget = GTK_WIDGET (scrollbar);

  gtk_widget_style_get (widget,
                        "fixed-slider-length", &fixed_size,
                        "has-backward-stepper", &has_a,
                        "has-secondary-forward-stepper", &has_b,
                        "has-secondary-backward-stepper", &has_c,
                        "has-forward-stepper", &has_d,
                        NULL);

  gtk_range_set_slider_size_fixed (range, fixed_size);
  _gtk_range_set_steppers (range, has_a, has_b, has_c, has_d);
}

static void
gtk_scrollbar_init (GtkScrollbar *scrollbar)
{
  gtk_scrollbar_update_style (scrollbar);
  gtk_range_set_slider_use_min_size (GTK_RANGE (scrollbar), TRUE);
}

static void
gtk_scrollbar_style_updated (GtkWidget *widget)
{
  gtk_scrollbar_update_style (GTK_SCROLLBAR (widget));
  GTK_WIDGET_CLASS (gtk_scrollbar_parent_class)->style_updated (widget);
}

/**
 * gtk_scrollbar_new:
 * @orientation: the scrollbar’s orientation.
 * @adjustment: (allow-none): the #GtkAdjustment to use, or %NULL to create a new adjustment.
 *
 * Creates a new scrollbar with the given orientation.
 *
 * Returns:  the new #GtkScrollbar.
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_scrollbar_new (GtkOrientation  orientation,
                   GtkAdjustment  *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_SCROLLBAR,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}
