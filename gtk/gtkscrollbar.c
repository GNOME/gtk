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

#include <config.h>
#include "gtkscrollbar.h"
#include "gtkintl.h"
#include "gtkalias.h"

static void gtk_scrollbar_class_init (GtkScrollbarClass *klass);
static void gtk_scrollbar_init       (GtkScrollbar      *scrollbar);
static void gtk_scrollbar_style_set  (GtkWidget         *widget,
                                      GtkStyle          *previous);

static gpointer parent_class;

GType
gtk_scrollbar_get_type (void)
{
  static GType scrollbar_type = 0;

  if (!scrollbar_type)
    {
      static const GTypeInfo scrollbar_info =
      {
	sizeof (GtkScrollbarClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_scrollbar_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkScrollbar),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_scrollbar_init,
	NULL,		/* value_table */
      };

      scrollbar_type =
	g_type_register_static (GTK_TYPE_RANGE, "GtkScrollbar",
			        &scrollbar_info, G_TYPE_FLAG_ABSTRACT);
    }

  return scrollbar_type;
}

static void
gtk_scrollbar_class_init (GtkScrollbarClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  widget_class->style_set = gtk_scrollbar_style_set;
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("min_slider_length",
							     P_("Minimum Slider Length"),
							     P_("Minimum length of scrollbar slider"),
							     0,
							     G_MAXINT,
							     21,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("fixed_slider_length",
                                                                 P_("Fixed slider size"),
                                                                 P_("Don't change slider size, just lock it to the minimum length"),
                                                                 FALSE,
                                                                 
                                                                 G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has_backward_stepper",
                                                                 P_("Backward stepper"),
                                                                 P_("Display the standard backward arrow button"),
                                                                 TRUE,
                                                                 
                                                                 G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_boolean ("has_forward_stepper",
                                                                   P_("Forward stepper"),
                                                                   P_("Display the standard forward arrow button"),
                                                                   TRUE,
                                                                   
                                                                   G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has_secondary_backward_stepper",
                                                                 P_("Secondary backward stepper"),
                                                                 P_("Display a second backward arrow button on the opposite end of the scrollbar"),
                                                                 FALSE,
                                                                 
                                                                 G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_boolean ("has_secondary_forward_stepper",
                                                                   P_("Secondary forward stepper"),
                                                                   P_("Display a secondary forward arrow button on the opposite end of the scrollbar"),
                                                                   FALSE,
                                                                   
                                                                   G_PARAM_READABLE));
}

static void
gtk_scrollbar_init (GtkScrollbar *scrollbar)
{
  GtkRange *range;

  range = GTK_RANGE (scrollbar);
}

static void
gtk_scrollbar_style_set  (GtkWidget *widget,
                          GtkStyle  *previous)
{
  gint slider_length;
  gboolean fixed_size;
  gboolean has_a, has_b, has_c, has_d;
  GtkRange *range;

  range = GTK_RANGE (widget);
  
  gtk_widget_style_get (widget,
                        "min_slider_length", &slider_length,
                        "fixed_slider_length", &fixed_size,
                        "has_backward_stepper", &has_a,
                        "has_secondary_forward_stepper", &has_b,
                        "has_secondary_backward_stepper", &has_c,
                        "has_forward_stepper", &has_d,
                        NULL);
  
  range->min_slider_size = slider_length;
  range->slider_size_fixed = fixed_size;

  range->has_stepper_a = has_a;
  range->has_stepper_b = has_b;
  range->has_stepper_c = has_c;
  range->has_stepper_d = has_d;
  
  (* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous);
}

#define __GTK_SCROLLBAR_C__
#include "gtkaliasdef.c"
