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
#include "gtkprintutils.h"
#include "gtkalias.h"


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

/**
 * gtk_page_setup_new:
 *
 * Creates a new #GtkPageSetup. 
 * 
 * Return value: a new #GtkPageSetup.
 *
 * Since: 2.10
 */
GtkPageSetup *
gtk_page_setup_new (void)
{
  return g_object_new (GTK_TYPE_PAGE_SETUP, NULL);
}

/**
 * gtk_page_setup_copy:
 * @other: the #GtkPageSetup to copy
 * 
 * Copies a #GtkPageSetup.
 * 
 * Return value: a copy of @other
 *
 * Since: 2.10
 */
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

/**
 * gtk_page_setup_get_orientation:
 * @setup: a #GtkPageSetup
 * 
 * Gets the page orientation of the #GtkPageSetup.
 * 
 * Return value: the page orientation
 *
 * Since: 2.10
 */
GtkPageOrientation
gtk_page_setup_get_orientation (GtkPageSetup *setup)
{
  return setup->orientation;
}

/**
 * gtk_page_setup_set_orientation:
 * @setup: a #GtkPageSetup
 * @orientation: a #GtkPageOrientation value
 * 
 * Sets the page orientation of the #GtkPageSetup.
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_orientation (GtkPageSetup       *setup,
				GtkPageOrientation  orientation)
{
  setup->orientation = orientation;
}

/**
 * gtk_page_setup_get_paper_size:
 * @setup: a #GtkPageSetup
 * 
 * Gets the paper size of the #GtkPageSetup.
 * 
 * Return value: the paper size
 *
 * Since: 2.10
 */
GtkPaperSize *
gtk_page_setup_get_paper_size (GtkPageSetup *setup)
{
  g_return_val_if_fail (GTK_IS_PAGE_SETUP (setup), NULL);

  return setup->paper_size;
}

/**
 * gtk_page_setup_set_paper_size:
 * @setup: a #GtkPageSetup
 * @size: a #GtkPaperSize 
 * 
 * Sets the paper size of the #GtkPageSetup without
 * changing the margins. See 
 * gtk_page_setup_set_paper_size_and_default_margins().
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_paper_size (GtkPageSetup *setup,
			       GtkPaperSize *size)
{
  GtkPaperSize *old_size;

  g_return_if_fail (GTK_IS_PAGE_SETUP (setup));
  g_return_if_fail (size != NULL);

  old_size = setup->paper_size;

  setup->paper_size = gtk_paper_size_copy (size);

  if (old_size)
    gtk_paper_size_free (old_size);
}

/**
 * gtk_page_setup_set_paper_size_and_default_margins:
 * @setup: a #GtkPageSetup
 * @size: a #GtkPaperSize 
 * 
 * Sets the paper size of the #GtkPageSetup and modifies
 * the margins according to the new paper size.
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_paper_size_and_default_margins (GtkPageSetup *setup,
						   GtkPaperSize *size)
{
  gtk_page_setup_set_paper_size (setup, size);
  setup->top_margin = gtk_paper_size_get_default_top_margin (setup->paper_size, GTK_UNIT_MM);
  setup->bottom_margin = gtk_paper_size_get_default_bottom_margin (setup->paper_size, GTK_UNIT_MM);
  setup->left_margin = gtk_paper_size_get_default_left_margin (setup->paper_size, GTK_UNIT_MM);
  setup->right_margin = gtk_paper_size_get_default_right_margin (setup->paper_size, GTK_UNIT_MM);
}

/**
 * gtk_page_setup_get_top_margin:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Gets the top margin in units of @unit.
 * 
 * Return value: the top margin
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_top_margin (GtkPageSetup *setup,
			       GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->top_margin, unit);
}

/**
 * gtk_page_setup_set_top_margin:
 * @setup: a #GtkPageSetup
 * @margin: the new top margin in units of @unit
 * @unit: the units for @margin
 * 
 * Sets the top margin of the #GtkPageSetup.
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_top_margin (GtkPageSetup *setup,
			       gdouble       margin,
			       GtkUnit       unit)
{
  setup->top_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_bottom_margin:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Gets the bottom margin in units of @unit.
 * 
 * Return value: the bottom margin
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_bottom_margin (GtkPageSetup *setup,
				  GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->bottom_margin, unit);
}

/**
 * gtk_page_setup_set_bottom_margin:
 * @setup: a #GtkPageSetup
 * @margin: the new bottom margin in units of @unit
 * @unit: the units for @margin
 * 
 * Sets the bottom margin of the #GtkPageSetup.
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_bottom_margin (GtkPageSetup *setup,
				  gdouble       margin,
				  GtkUnit       unit)
{
  setup->bottom_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_left_margin:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Gets the left margin in units of @unit.
 * 
 * Return value: the left margin
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_left_margin (GtkPageSetup *setup,
				GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->left_margin, unit);
}

/**
 * gtk_page_setup_set_left_margin:
 * @setup: a #GtkPageSetup
 * @margin: the new left margin in units of @unit
 * @unit: the units for @margin
 * 
 * Sets the left margin of the #GtkPageSetup.
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_left_margin (GtkPageSetup *setup,
				gdouble       margin,
				GtkUnit       unit)
{
  setup->left_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_right_margin:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Gets the right margin in units of @unit.
 * 
 * Return value: the right margin
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_right_margin (GtkPageSetup *setup,
				 GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->right_margin, unit);
}

/**
 * gtk_page_setup_set_right_margin:
 * @setup: a #GtkPageSetup
 * @margin: the new right margin in units of @unit
 * @unit: the units for @margin
 * 
 * Sets the right margin of the #GtkPageSetup.
 *
 * Since: 2.10
 */
void
gtk_page_setup_set_right_margin (GtkPageSetup *setup,
				 gdouble       margin,
				 GtkUnit       unit)
{
  setup->right_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_paper_width:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Returns the paper width in units of @unit.
 * 
 * Note that this function takes orientation, but 
 * not margins into consideration. 
 * See gtk_page_setup_get_page_width().
 *
 * Return value: the paper width.
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_paper_width (GtkPageSetup *setup,
				GtkUnit       unit)
{
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    return gtk_paper_size_get_width (setup->paper_size, unit);
  else
    return gtk_paper_size_get_height (setup->paper_size, unit);
}

/**
 * gtk_page_setup_get_paper_height:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Returns the paper height in units of @unit.
 * 
 * Note that this function takes orientation, but 
 * not margins into consideration.
 * See gtk_page_setup_get_page_height().
 *
 * Return value: the paper height.
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_paper_height (GtkPageSetup *setup,
				 GtkUnit       unit)
{
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    return gtk_paper_size_get_height (setup->paper_size, unit);
  else
    return gtk_paper_size_get_width (setup->paper_size, unit);
}

/**
 * gtk_page_setup_get_page_width:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Returns the page width in units of @unit.
 * 
 * Note that this function takes orientation and
 * margins into consideration. 
 * See gtk_page_setup_get_paper_width().
 *
 * Return value: the page width.
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_page_width (GtkPageSetup *setup,
			       GtkUnit       unit)
{
  gdouble width;
  
  width = gtk_page_setup_get_paper_width (setup, GTK_UNIT_MM);
  width -= setup->left_margin + setup->right_margin;
  
  return _gtk_print_convert_from_mm (width, unit);
}

/**
 * gtk_page_setup_get_page_height:
 * @setup: a #GtkPageSetup
 * @unit: the unit for the return value
 * 
 * Returns the page height in units of @unit.
 * 
 * Note that this function takes orientation and
 * margins into consideration. 
 * See gtk_page_setup_get_paper_height().
 *
 * Return value: the page height.
 *
 * Since: 2.10
 */
gdouble
gtk_page_setup_get_page_height (GtkPageSetup *setup,
				GtkUnit       unit)
{
  gdouble height;
  
  height = gtk_page_setup_get_paper_height (setup, GTK_UNIT_MM);
  height -= setup->top_margin + setup->bottom_margin;
  
  return _gtk_print_convert_from_mm (height, unit);
}


#define __GTK_PAGE_SETUP_C__
#include "gtkaliasdef.c"
