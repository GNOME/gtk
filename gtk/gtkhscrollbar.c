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

#include "gtkhscrollbar.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"
#include "gtkintl.h"

static void     gtk_hscrollbar_class_init       (GtkHScrollbarClass *klass);
static void     gtk_hscrollbar_init             (GtkHScrollbar      *hscrollbar);

GtkType
gtk_hscrollbar_get_type (void)
{
  static GtkType hscrollbar_type = 0;
  
  if (!hscrollbar_type)
    {
      static const GtkTypeInfo hscrollbar_info =
      {
        "GtkHScrollbar",
        sizeof (GtkHScrollbar),
        sizeof (GtkHScrollbarClass),
        (GtkClassInitFunc) gtk_hscrollbar_class_init,
        (GtkObjectInitFunc) gtk_hscrollbar_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      hscrollbar_type = gtk_type_unique (GTK_TYPE_SCROLLBAR, &hscrollbar_info);
    }
  
  return hscrollbar_type;
}

static void
gtk_hscrollbar_class_init (GtkHScrollbarClass *class)
{
  GTK_RANGE_CLASS (class)->stepper_detail = "hscrollbar";
}

static void
gtk_hscrollbar_init (GtkHScrollbar *hscrollbar)
{
  GtkRange *range;

  range = GTK_RANGE (hscrollbar);

  range->orientation = GTK_ORIENTATION_HORIZONTAL;
}

GtkWidget*
gtk_hscrollbar_new (GtkAdjustment *adjustment)
{
  GtkWidget *hscrollbar;
  
  hscrollbar = gtk_widget_new (GTK_TYPE_HSCROLLBAR,
			       "adjustment", adjustment,
			       NULL);

  return hscrollbar;
}

