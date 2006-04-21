/* GTK - The GIMP Toolkit
 * gtkpagesetup.c: Page Setup
 * Copyright (C) 2006, Red Hat, Inc.
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

#include "config.h"

#include "gtkpagesetup.h"
#include "gtkalias.h"

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72

typedef struct _GtkPageSetupClass GtkPageSetupClass;

#define GTK_IS_PAGE_SETUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PAGE_SETUP))
#define GTK_PAGE_SETUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PAGE_SETUP, GtkPageSetupClass))
#define GTK_PAGE_SETUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PAGE_SETUP, GtkPageSetupClass))

struct _GtkPageSetup
{
  GObject parent_instance;

  GtkPageOrientation orientation;
  GtkPaperSize *paper_size;
  /* These are stored in mm */
  double top_margin, bottom_margin, left_margin, right_margin;
};

struct _GtkPageSetupClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkPageSetup, gtk_page_setup, G_TYPE_OBJECT)

static double
to_mm (double len, GtkUnit unit)
{
  switch (unit)
    {
    case GTK_UNIT_MM:
      return len;
    case GTK_UNIT_INCH:
      return len * MM_PER_INCH;
    default:
    case GTK_UNIT_PIXEL:
      g_warning ("Unsupported unit");
      /* Fall through */
    case GTK_UNIT_POINTS:
      return len * (MM_PER_INCH / POINTS_PER_INCH);
      break;
    }
}

static double
from_mm (double len, GtkUnit unit)
{
  switch (unit)
    {
    case GTK_UNIT_MM:
      return len;
    case GTK_UNIT_INCH:
      return len / MM_PER_INCH;
    default:
    case GTK_UNIT_PIXEL:
      g_warning ("Unsupported unit");
      /* Fall through */
    case GTK_UNIT_POINTS:
      return len / (MM_PER_INCH / POINTS_PER_INCH);
      break;
    }
}

static void
gtk_page_setup_finalize (GObject *object)
{
  GtkPageSetup *setup = GTK_PAGE_SETUP (object);
  
  gtk_paper_size_free (setup->paper_size);
  
  G_OBJECT_CLASS (gtk_page_setup_parent_class)->finalize (object);
}

static void
gtk_page_setup_init (GtkPageSetup *setup)
{
  setup->paper_size = gtk_paper_size_new (NULL);
  setup->orientation = GTK_PAGE_ORIENTATION_PORTRAIT;
  setup->top_margin = gtk_paper_size_get_default_top_margin (setup->paper_size, GTK_UNIT_MM);
  setup->bottom_margin = gtk_paper_size_get_default_bottom_margin (setup->paper_size, GTK_UNIT_MM);
  setup->left_margin = gtk_paper_size_get_default_left_margin (setup->paper_size, GTK_UNIT_MM);
  setup->right_margin = gtk_paper_size_get_default_right_margin (setup->paper_size, GTK_UNIT_MM);
}

static void
gtk_page_setup_class_init (GtkPageSetupClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->finalize = gtk_page_setup_finalize;
}
  
GtkPageSetup *
gtk_page_setup_new (void)
{
  return g_object_new (GTK_TYPE_PAGE_SETUP, NULL);
}

GtkPageSetup *
gtk_page_setup_copy (GtkPageSetup *other)
{
  GtkPageSetup *copy;

  copy = gtk_page_setup_new ();
  copy->orientation = other->orientation;
  copy->paper_size = gtk_paper_size_copy (other->paper_size);
  copy->top_margin = other->top_margin;
  copy->bottom_margin = other->bottom_margin;
  copy->left_margin = other->left_margin;
  copy->right_margin = other->right_margin;

  return copy;
}

GtkPageOrientation
gtk_page_setup_get_orientation (GtkPageSetup *setup)
{
  return setup->orientation;
}

void
gtk_page_setup_set_orientation (GtkPageSetup *setup,
				GtkPageOrientation orientation)
{
  setup->orientation = orientation;
}

GtkPaperSize *
gtk_page_setup_get_paper_size (GtkPageSetup *setup)
{
  return setup->paper_size;
}

void
gtk_page_setup_set_paper_size (GtkPageSetup *setup,
			       GtkPaperSize *size)
{
  setup->paper_size = gtk_paper_size_copy (size);
}

void
gtk_page_setup_set_paper_size_and_default_margins (GtkPageSetup *setup,
						   GtkPaperSize *size)
{
  setup->paper_size = gtk_paper_size_copy (size);
  setup->top_margin = gtk_paper_size_get_default_top_margin (setup->paper_size, GTK_UNIT_MM);
  setup->bottom_margin = gtk_paper_size_get_default_bottom_margin (setup->paper_size, GTK_UNIT_MM);
  setup->left_margin = gtk_paper_size_get_default_left_margin (setup->paper_size, GTK_UNIT_MM);
  setup->right_margin = gtk_paper_size_get_default_right_margin (setup->paper_size, GTK_UNIT_MM);
}

double
gtk_page_setup_get_top_margin (GtkPageSetup *setup,
			       GtkUnit       unit)
{
  return from_mm (setup->top_margin, unit);
}

void
gtk_page_setup_set_top_margin (GtkPageSetup *setup,
			       double        margin,
			       GtkUnit       unit)
{
  setup->top_margin = to_mm (margin, unit);
}

double
gtk_page_setup_get_bottom_margin (GtkPageSetup *setup,
				  GtkUnit       unit)
{
  return from_mm (setup->bottom_margin, unit);
}

void
gtk_page_setup_set_bottom_margin (GtkPageSetup *setup,
				  double        margin,
				  GtkUnit       unit)
{
  setup->bottom_margin = to_mm (margin, unit);
}

double
gtk_page_setup_get_left_margin (GtkPageSetup    *setup,
				GtkUnit          unit)
{
  return from_mm (setup->left_margin, unit);
}

void
gtk_page_setup_set_left_margin (GtkPageSetup    *setup,
				double           margin,
				GtkUnit          unit)
{
  setup->left_margin = to_mm (margin, unit);
}

double
gtk_page_setup_get_right_margin (GtkPageSetup    *setup,
				 GtkUnit          unit)
{
  return from_mm (setup->right_margin, unit);
}

void
gtk_page_setup_set_right_margin (GtkPageSetup    *setup,
				 double           margin,
				 GtkUnit          unit)
{
  setup->right_margin = to_mm (margin, unit);
}

/* These take orientation, but not margins into consideration */
double
gtk_page_setup_get_paper_width (GtkPageSetup *setup,
				GtkUnit       unit)
{
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    return gtk_paper_size_get_width (setup->paper_size, unit);
  else
    return gtk_paper_size_get_height (setup->paper_size, unit);
}

double
gtk_page_setup_get_paper_height (GtkPageSetup  *setup,
				 GtkUnit        unit)
{
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    return gtk_paper_size_get_height (setup->paper_size, unit);
  else
    return gtk_paper_size_get_width (setup->paper_size, unit);
}

/* These take orientation, and margins into consideration */
double
gtk_page_setup_get_page_width (GtkPageSetup    *setup,
			       GtkUnit          unit)
{
  double width;
  
  width = gtk_page_setup_get_paper_width (setup, GTK_UNIT_MM);
  width -= setup->left_margin + setup->right_margin;
  
  return from_mm (width, unit);
}

double
gtk_page_setup_get_page_height (GtkPageSetup    *setup,
				GtkUnit          unit)
{
  double height;
  
  height = gtk_page_setup_get_paper_height (setup, GTK_UNIT_MM);
  height -= setup->top_margin + setup->bottom_margin;
  
  return from_mm (height, unit);
}


#define __GTK_PAGE_SETUP_C__
#include "gtkaliasdef.c"
