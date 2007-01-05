/* GTK - The GIMP Toolkit
 * gtkpapersize.c: Paper Size
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
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#if defined(HAVE__NL_PAPER_HEIGHT) && defined(HAVE__NL_PAPER_WIDTH)
#include <langinfo.h>
#endif

#include "gtkpapersize.h"
#include "gtkprintutils.h"
#include "gtkintl.h"
#include "gtkalias.h"

#include "paper_names_offsets.c"

struct _GtkPaperSize
{
  const PaperInfo *info;

  /* If these are not set we fall back to info */
  gchar *name;
  gchar *display_name;
  gchar *ppd_name;
  
  gdouble width, height; /* Stored in mm */
  gboolean is_custom;
};

GType
gtk_paper_size_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GtkPaperSize",
					     (GBoxedCopyFunc)gtk_paper_size_copy,
					     (GBoxedFreeFunc)gtk_paper_size_free);
  return our_type;
}

static const PaperInfo *
lookup_paper_info (const gchar *name)
{
  int lower = 0;
  int upper = G_N_ELEMENTS (_gtk_standard_names_offsets) - 1;
  int mid;
  int cmp;

  do 
    {
       mid = (lower + upper) / 2;
       cmp = strcmp (name, _gtk_paper_names + _gtk_standard_names_offsets[mid].name);
       if (cmp < 0)
         upper = mid - 1;
       else if (cmp > 0)
         lower = mid + 1;
       else
	 return &_gtk_standard_names_offsets[mid];
    }
  while (lower <= upper);

  return NULL;
}

static gboolean
parse_media_size (const gchar *size,
		  gdouble     *width_mm, 
		  gdouble     *height_mm)
{
  const char *p;
  char *e;
  double short_dim, long_dim;

  p = size;
  
  short_dim = g_ascii_strtod (p, &e);

  if (p == e || *e != 'x')
    return FALSE;

  p = e + 1; /* Skip x */

  long_dim = g_ascii_strtod (p, &e);

  if (p == e)
    return FALSE;

  p = e;

  if (strcmp (p, "in") == 0)
    {
      short_dim = short_dim * MM_PER_INCH;
      long_dim = long_dim * MM_PER_INCH;
    }
  else if (strcmp (p, "mm") != 0)
    return FALSE;

  if (width_mm)
    *width_mm = short_dim;
  if (height_mm)
    *height_mm = long_dim;
  
  return TRUE;  
}

static gboolean
parse_full_media_size_name (const gchar  *full_name,
			    gchar       **name,
			    gdouble      *width_mm, 
			    gdouble      *height_mm)
{
  const char *p;
  const char *end_of_name;
  
  /* From the spec:
   media-size-self-describing-name =
        ( class-in "_" size-name "_" short-dim "x" long-dim "in" ) |
        ( class-mm "_" size-name "_" short-dim "x" long-dim "mm" )
   class-in = "custom" | "na" | "asme" | "roc" | "oe"
   class-mm = "custom" | "iso" | "jis" | "jpn" | "prc" | "om"
   size-name = ( lowalpha | digit ) *( lowalpha | digit | "-" )
   short-dim = dim
   long-dim = dim
   dim = integer-part [fraction-part] | "0" fraction-part
   integer-part = non-zero-digit *digit
   fraction-part = "." *digit non-zero-digit
   lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" |
              "j" | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" |
              "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z"
   non-zero-digit = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
   digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
 */

  p = strchr (full_name, '_');
  if (p == NULL)
    return FALSE;

  p++; /* Skip _ */
  
  p = strchr (p, '_');
  if (p == NULL)
    return FALSE;

  end_of_name = p;

  p++; /* Skip _ */

  if (!parse_media_size (p, width_mm, height_mm))
    return FALSE;

  if (name)
    *name = g_strndup (full_name, end_of_name - full_name);
  
  return TRUE;  
}

static GtkPaperSize *
gtk_paper_size_new_from_info (const PaperInfo *info)
{
  GtkPaperSize *size;
  
  size = g_new0 (GtkPaperSize, 1);
  size->info = info;
  size->width = info->width;
  size->height = info->height;
  
  return size;
}

/**
 * gtk_paper_size_new:
 * @name: a paper size name, or %NULL
 * 
 * Creates a new #GtkPaperSize object by parsing a 
 * PWG 5101.1-2002 PWG <!-- FIXME link here -->
 * paper name. 
 *
 * If @name is %NULL, the default paper size is returned,
 * see gtk_paper_size_get_default().
 * 
 * Return value: a new #GtkPaperSize, use gtk_paper_size_free()
 * to free it
 *
 * Since: 2.10
 */
GtkPaperSize *
gtk_paper_size_new (const gchar *name)
{
  GtkPaperSize *size;
  char *short_name;
  double width, height;
  const PaperInfo *info;

  if (name == NULL)
    name = gtk_paper_size_get_default ();
  
  if (parse_full_media_size_name (name, &short_name, &width, &height))
    {
      size = g_new0 (GtkPaperSize, 1);

      size->width = width;
      size->height = height;
      size->name = short_name;
      size->display_name = g_strdup (short_name);
    }
  else
    {
      info = lookup_paper_info (name);
      if (info != NULL)
	size = gtk_paper_size_new_from_info (info);
      else
	{
	  g_warning ("Unknown paper size %s\n", name);
	  size = g_new0 (GtkPaperSize, 1);
	  size->name = g_strdup (name);
	  size->display_name = g_strdup (name);
	  /* Default to A4 size */
	  size->width = 210;
	  size->height = 297;
	}
    }
  
  return size;
}

/**
 * gtk_paper_size_new_from_ppd:
 * @ppd_name: a PPD paper name
 * @ppd_display_name: the corresponding human-readable name
 * @width: the paper width, in points
 * @height: the paper height in points
 * 
 * Creates a new #GtkPaperSize object by using 
 * PPD information. 
 * 
 * If @ppd_name is not a recognized PPD paper name, 
 * @ppd_display_name, @width and @height are used to 
 * construct a custom #GtkPaperSize object.
 *
 * Return value: a new #GtkPaperSize, use gtk_paper_size_free()
 * to free it
 *
 * Since: 2.10
 */
GtkPaperSize *
gtk_paper_size_new_from_ppd (const gchar *ppd_name,
			     const gchar *ppd_display_name,
			     gdouble      width,
			     gdouble      height)
{
  char *name;
  const char *lookup_ppd_name;
  char *freeme;
  GtkPaperSize *size;
  int i;

  lookup_ppd_name = ppd_name;
  
  freeme = NULL;
  /* Strip out Traverse suffix in matching. */
  if (g_str_has_suffix (ppd_name, ".Transverse"))
    {
      lookup_ppd_name = freeme =
	g_strndup (ppd_name, strlen (ppd_name) - strlen (".Transverse"));
    }
  
  for (i = 0; i < G_N_ELEMENTS (_gtk_standard_names_offsets); i++)
    {
      if (_gtk_standard_names_offsets[i].ppd_name != -1 &&
	  strcmp (_gtk_paper_names + _gtk_standard_names_offsets[i].ppd_name, lookup_ppd_name) == 0)
	{
	  size = gtk_paper_size_new_from_info (&_gtk_standard_names_offsets[i]);
	  goto out;
	}
    }
  
  for (i = 0; i < G_N_ELEMENTS (_gtk_extra_ppd_names_offsets); i++)
    {
      if (strcmp (_gtk_paper_names + _gtk_extra_ppd_names_offsets[i].ppd_name, lookup_ppd_name) == 0)
	{
	  size = gtk_paper_size_new (_gtk_paper_names + _gtk_extra_ppd_names_offsets[i].standard_name);
	  goto out;
	}
    }

  name = g_strconcat ("ppd_", ppd_name, NULL);
  size = gtk_paper_size_new_custom (name, ppd_display_name, width, height, GTK_UNIT_POINTS);
  g_free (name);

 out:

  if (size->info == NULL ||
      size->info->ppd_name == -1 ||
      strcmp (_gtk_paper_names + size->info->ppd_name, ppd_name) != 0)
    size->ppd_name = g_strdup (ppd_name);
  
  g_free (freeme);
  
  return size;
}

/**
 * gtk_paper_size_new_custom:
 * @name: the paper name 
 * @display_name: the human-readable name
 * @width: the paper width, in units of @unit
 * @height: the paper height, in units of @unit
 * @unit: the unit for @width and @height
 * 
 * Creates a new #GtkPaperSize object with the
 * given parameters.
 * 
 * Return value: a new #GtkPaperSize object, use gtk_paper_size_free()
 * to free it
 *
 * Since: 2.10
 */
GtkPaperSize *
gtk_paper_size_new_custom (const gchar *name, 
			   const gchar *display_name,
			   gdouble      width, 
			   gdouble      height, 
			   GtkUnit      unit)
{
  GtkPaperSize *size;
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (unit != GTK_UNIT_PIXEL, NULL);

  size = g_new0 (GtkPaperSize, 1);
  
  size->name = g_strdup (name);
  size->display_name = g_strdup (display_name);
  size->is_custom = TRUE;
  
  size->width = _gtk_print_convert_to_mm (width, unit);
  size->height = _gtk_print_convert_to_mm (height, unit);
  
  return size;
}

/**
 * gtk_paper_size_copy:
 * @other: a #GtkPaperSize
 * 
 * Copies an existing #GtkPaperSize.
 * 
 * Return value: a copy of @other
 *
 * Since: 2.10
 */
GtkPaperSize *
gtk_paper_size_copy (GtkPaperSize *other)
{
  GtkPaperSize *size;

  size = g_new0 (GtkPaperSize, 1);

  size->info = other->info;
  if (other->name)
    size->name = g_strdup (other->name);
  if (other->display_name)
    size->display_name = g_strdup (other->display_name);
  if (other->ppd_name)
    size->ppd_name = g_strdup (other->ppd_name);
  
  size->width = other->width;
  size->height = other->height;
  size->is_custom = other->is_custom;

  return size;
}

/**
 * gtk_paper_size_free:
 * @size: a #GtkPaperSize
 * 
 * Free the given #GtkPaperSize object.
 *
 * Since: 2.10
 */
void
gtk_paper_size_free (GtkPaperSize *size)
{
  g_free (size->name);
  g_free (size->display_name);
  g_free (size->ppd_name);
  g_free (size);
}

/**
 * gtk_paper_size_is_equal:
 * @size1: a #GtkPaperSize object
 * @size2: another #GtkPaperSize object
 * 
 * Compares two #GtkPaperSize objects.
 * 
 * Return value: %TRUE, if @size1 and @size2 
 * represent the same paper size
 *
 * Since: 2.10
 */
gboolean
gtk_paper_size_is_equal (GtkPaperSize *size1,
			 GtkPaperSize *size2)
{
  if (size1->info != NULL && size2->info != NULL)
    return size1->info == size2->info;
  
  return strcmp (gtk_paper_size_get_name (size1),
		 gtk_paper_size_get_name (size2)) == 0;
}

/**
 * gtk_paper_size_get_name:
 * @size: a #GtkPaperSize object
 * 
 * Gets the name of the #GtkPaperSize.
 * 
 * Return value: the name of @size
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_paper_size_get_name (GtkPaperSize *size)
{
  if (size->name)
    return size->name;
  g_assert (size->info != NULL);
  return _gtk_paper_names + size->info->name;
}

/**
 * gtk_paper_size_get_display_name:
 * @size: a #GtkPaperSize object
 * 
 * Gets the human-readable name of the #GtkPaperSize.
 * 
 * Return value: the human-readable name of @size
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_paper_size_get_display_name (GtkPaperSize *size)
{
  const gchar *display_name;

  if (size->display_name)
    return size->display_name;

  g_assert (size->info != NULL);

  display_name = _gtk_paper_names + size->info->display_name;
  return g_strip_context (display_name, gettext (display_name));
}

/**
 * gtk_paper_size_get_ppd_name:
 * @size: a #GtkPaperSize object
 * 
 * Gets the PPD name of the #GtkPaperSize, which
 * may be %NULL.
 * 
 * Return value: the PPD name of @size
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_paper_size_get_ppd_name (GtkPaperSize *size)
{
  if (size->ppd_name)
    return size->ppd_name;
  if (size->info)
    return _gtk_paper_names + size->info->ppd_name;
  return NULL;
}

/**
 * gtk_paper_size_get_width:
 * @size: a #GtkPaperSize object
 * @unit: the unit for the return value
 * 
 * Gets the paper width of the #GtkPaperSize, in 
 * units of @unit.
 * 
 * Return value: the paper width 
 *
 * Since: 2.10
 */
gdouble
gtk_paper_size_get_width (GtkPaperSize *size, 
			  GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (size->width, unit);
}

/**
 * gtk_paper_size_get_height:
 * @size: a #GtkPaperSize object
 * @unit: the unit for the return value
 * 
 * Gets the paper height of the #GtkPaperSize, in 
 * units of @unit.
 * 
 * Return value: the paper height 
 *
 * Since: 2.10
 */
gdouble
gtk_paper_size_get_height (GtkPaperSize *size, 
			   GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (size->height, unit);
}

/**
 * gtk_paper_size_is_custom:
 * @size: a #GtkPaperSize object
 * 
 * Returns %TRUE if @size is not a standard paper size.
 * 
 * Return value: whether @size is a custom paper size.
 **/
gboolean
gtk_paper_size_is_custom (GtkPaperSize *size)
{
  return size->is_custom;
}

/**
 * gtk_paper_size_set_size:
 * @size: a custom #GtkPaperSize object
 * @width: the new width in units of @unit
 * @height: the new height in units of @unit
 * @unit: the unit for @width and @height
 * 
 * Changes the dimensions of a @size to @width x @height.
 *
 * Since: 2.10
 */
void
gtk_paper_size_set_size (GtkPaperSize *size, 
			 gdouble       width, 
			 gdouble       height, 
			 GtkUnit       unit)
{
  g_return_if_fail (size != NULL);
  g_return_if_fail (size->is_custom);

  size->width = _gtk_print_convert_to_mm (width, unit);
  size->height = _gtk_print_convert_to_mm (height, unit);
}

#define NL_PAPER_GET(x)         \
  ((union { char *string; unsigned int word; })nl_langinfo(x)).word

/**
 * gtk_paper_size_get_default:
 *
 * Returns the name of the default paper size, which 
 * depends on the current locale.  
 * 
 * Return value: the name of the default paper size. The string
 * is owned by GTK+ and should not be modified.
 * 
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_paper_size_get_default (void)
{
  char *locale, *freeme = NULL;
  const char *paper_size;

#if defined(HAVE__NL_PAPER_HEIGHT) && defined(HAVE__NL_PAPER_WIDTH)
  {
    int width = NL_PAPER_GET (_NL_PAPER_WIDTH);
    int height = NL_PAPER_GET (_NL_PAPER_HEIGHT);
    
    if (width == 210 && height == 297)
      return GTK_PAPER_NAME_A4;
    
    if (width == 216 && height == 279)
      return GTK_PAPER_NAME_LETTER;
  }
#endif

#ifdef G_OS_WIN32
  freeme = locale = g_win32_getlocale ();
#elif defined(LC_PAPER)
  locale = setlocale(LC_PAPER, NULL);
#else
  locale = setlocale(LC_MESSAGES, NULL);
#endif

  if (!locale)
    return GTK_PAPER_NAME_A4;

  if (g_str_has_prefix (locale, "en_CA") ||
      g_str_has_prefix (locale, "en_US") ||
      g_str_has_prefix (locale, "es_PR") ||
      g_str_has_prefix (locale, "es_US"))
    paper_size = GTK_PAPER_NAME_LETTER;
  else
    paper_size = GTK_PAPER_NAME_A4;

  g_free (freeme);
  return paper_size;
}

/* These get the default margins used for the paper size. Its
 * larger than most printers margins, so that it will be within
 * the imageble area on any printer.
 *
 * I've taken the actual values used from the OSX page setup dialog.
 * I'm not sure exactly where they got these values for, but might
 * correspond to this (from ghostscript docs):
 * 
 * All DeskJets have 0.5 inches (1.27cm) of unprintable bottom margin,
 * due to the mechanical arrangement used to grab the paper. Side margins
 * are approximately 0.25 inches (0.64cm) for U.S. letter paper, and 0.15
 * inches (0.38cm) for A4.
 */

/**
 * gtk_paper_size_get_default_top_margin:
 * @size: a #GtkPaperSize object
 * @unit: the unit for the return value
 * 
 * Gets the default top margin for the #GtkPaperSize.
 * 
 * Return value: the default top margin
 *
 * Since: 2.10
 */
gdouble
gtk_paper_size_get_default_top_margin (GtkPaperSize *size, 
				       GtkUnit       unit)
{
  gdouble margin;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);
  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_get_default_bottom_margin:
 * @size: a #GtkPaperSize object
 * @unit: the unit for the return value
 * 
 * Gets the default bottom margin for the #GtkPaperSize.
 * 
 * Return value: the default bottom margin
 *
 * Since: 2.10
 */
gdouble
gtk_paper_size_get_default_bottom_margin (GtkPaperSize *size, 
					  GtkUnit       unit)
{
  gdouble margin;
  const gchar *name;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);

  name = gtk_paper_size_get_name (size);
  if (strcmp (name, "na_letter") == 0 ||
      strcmp (name, "na_legal") == 0 ||
      strcmp (name, "iso_a4") == 0)
    margin = _gtk_print_convert_to_mm (0.56, GTK_UNIT_INCH);
  
  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_get_default_left_margin:
 * @size: a #GtkPaperSize object
 * @unit: the unit for the return value
 * 
 * Gets the default left margin for the #GtkPaperSize.
 * 
 * Return value: the default left margin
 *
 * Since: 2.10
 */
gdouble
gtk_paper_size_get_default_left_margin (GtkPaperSize *size, 
					GtkUnit       unit)
{
  gdouble margin;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);
  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_get_default_right_margin:
 * @size: a #GtkPaperSize object
 * @unit: the unit for the return value
 * 
 * Gets the default right margin for the #GtkPaperSize.
 * 
 * Return value: the default right margin
 *
 * Since: 2.10
 */
gdouble
gtk_paper_size_get_default_right_margin (GtkPaperSize *size, 
					 GtkUnit       unit)
{
  gdouble margin;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);
  return _gtk_print_convert_from_mm (margin, unit);
}


#define __GTK_PAPER_SIZE_C__
#include "gtkaliasdef.c"
