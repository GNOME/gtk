/* GTK - The GIMP Toolkit
 * gtkprintsettings.c: Print Settings
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

#include "gtkprintsettings.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72

typedef struct _GtkPrintSettingsClass GtkPrintSettingsClass;

#define GTK_IS_PRINT_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_SETTINGS))
#define GTK_PRINT_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_SETTINGS, GtkPrintSettingsClass))
#define GTK_PRINT_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_SETTINGS, GtkPrintSettingsClass))

struct _GtkPrintSettings
{
  GObject parent_instance;
  
  GHashTable *hash;
};

struct _GtkPrintSettingsClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkPrintSettings, gtk_print_settings, G_TYPE_OBJECT)

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
gtk_print_settings_finalize (GObject *object)
{
  GtkPrintSettings *settings = GTK_PRINT_SETTINGS (object);

  g_hash_table_destroy (settings->hash);

  G_OBJECT_CLASS (gtk_print_settings_parent_class)->finalize (object);
}

static void
gtk_print_settings_init (GtkPrintSettings *settings)
{
  settings->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					  g_free, g_free);
}

static void
gtk_print_settings_class_init (GtkPrintSettingsClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->finalize = gtk_print_settings_finalize;
}

GtkPrintSettings *
gtk_print_settings_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_SETTINGS, NULL);
}

static void
copy_hash_entry  (gpointer  key,
		  gpointer  value,
		  gpointer  user_data)
{
  GtkPrintSettings *settings = user_data;

  g_hash_table_insert (settings->hash, 
		       g_strdup (key), 
		       g_strdup (value));
}



GtkPrintSettings *
gtk_print_settings_copy (GtkPrintSettings *other)
{
  GtkPrintSettings *settings;

  if (other == NULL)
    return NULL;
  
  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (other), NULL);

  settings = gtk_print_settings_new ();

  g_hash_table_foreach (other->hash,
			copy_hash_entry,
			settings);

  return settings;
}

const char *        
gtk_print_settings_get (GtkPrintSettings *settings,
			const char *key)
{
  return g_hash_table_lookup (settings->hash, key);
}

void
gtk_print_settings_set (GtkPrintSettings *settings,
			const char *key,
			const char *value)
{
  if (value == NULL)
    gtk_print_settings_unset (settings, key);
  else
    g_hash_table_insert (settings->hash, 
			 g_strdup (key), 
			 g_strdup (value));
}

void
gtk_print_settings_unset (GtkPrintSettings *settings,
			  const char *key)
{
  g_hash_table_remove (settings->hash, key);
}

gboolean        
gtk_print_settings_has_key (GtkPrintSettings *settings,
			    const char *key)
{
  return gtk_print_settings_get (settings, key) != NULL;
}


gboolean
gtk_print_settings_get_bool (GtkPrintSettings *settings,
			     const char *key)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (val != NULL && strcmp (val, "true") == 0)
    return TRUE;
  
  return FALSE;
}

static gboolean
gtk_print_settings_get_bool_with_default (GtkPrintSettings *settings,
					  const char *key,
					  gboolean default_val)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (val != NULL && strcmp (val, "true") == 0)
    return TRUE;

  if (val != NULL && strcmp (val, "false") == 0)
    return FALSE;
  
  return default_val;
}

void
gtk_print_settings_set_bool (GtkPrintSettings *settings,
			     const char *key,
			     gboolean value)
{
  if (value)
    gtk_print_settings_set (settings, key, "true");
  else
    gtk_print_settings_set (settings, key, "false");
}

double
gtk_print_settings_get_double_with_default (GtkPrintSettings *settings,
					    const char *key,
					    double def)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (val == NULL)
    return def;

  return g_ascii_strtod (val, NULL);
}

double
gtk_print_settings_get_double (GtkPrintSettings *settings,
			       const char *key)
{
  return gtk_print_settings_get_double_with_default (settings, key, 0.0);
}

void
gtk_print_settings_set_double (GtkPrintSettings *settings,
			       const char *key,
			       double value)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  
  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, value);
  gtk_print_settings_set (settings, key, buf);
}

double
gtk_print_settings_get_length (GtkPrintSettings *settings,
			       const char *key,
			       GtkUnit unit)
{
  double length = gtk_print_settings_get_double (settings, key);
  return from_mm (length, unit);
}

void
gtk_print_settings_set_length (GtkPrintSettings *settings,
			       const char *key,
			       double length, 
			       GtkUnit unit)
{
  gtk_print_settings_set_double (settings, key,
				 to_mm (length, unit));
}

int
gtk_print_settings_get_int_with_default (GtkPrintSettings *settings,
					 const char *key,
					 int def)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (val == NULL)
    return def;

  return atoi (val);
}

int
gtk_print_settings_get_int (GtkPrintSettings *settings,
			    const char *key)
{
  return gtk_print_settings_get_int_with_default (settings, key, 0);
}

void
gtk_print_settings_set_int (GtkPrintSettings *settings,
			    const char *key,
			    int value)
{
  char buf[128];
  g_sprintf(buf, "%d", value);
  gtk_print_settings_set (settings, key, buf);
}

void
gtk_print_settings_foreach (GtkPrintSettings *settings,
			    GtkPrintSettingsFunc func,
			    gpointer user_data)
{
  g_hash_table_foreach (settings->hash,
			(GHFunc)func, 
			user_data);
}

const char *       
gtk_print_settings_get_printer (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PRINTER);
}

void
gtk_print_settings_set_printer (GtkPrintSettings *settings,
				const char *printer)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PRINTER, printer);
}

GtkPageOrientation
gtk_print_settings_get_orientation (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_ORIENTATION);

  if (val == NULL || strcmp (val, "portrait") == 0)
    return GTK_PAGE_ORIENTATION_PORTRAIT;

  if (strcmp (val, "landscape") == 0)
    return GTK_PAGE_ORIENTATION_LANDSCAPE;
  
  if (strcmp (val, "reverse_portrait") == 0)
    return GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT;
  
  if (strcmp (val, "reverse_landscape") == 0)
    return GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE;
  
  return GTK_PAGE_ORIENTATION_PORTRAIT;
}

void
gtk_print_settings_set_orientation (GtkPrintSettings *settings,
				    GtkPageOrientation orientation)
{
  const char *val;

  switch (orientation)
    {
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
      val = "landscape";
      break;
    default:
    case GTK_PAGE_ORIENTATION_PORTRAIT:
      val = "portrait";
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
      val = "reverse_landscape";
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
      val = "reverse_portrait";
      break;
    }
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_ORIENTATION, val);
}

GtkPaperSize *     
gtk_print_settings_get_paper_size (GtkPrintSettings *settings)
{
  const char *val;
  const char *name;
  double w, h;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT);
  if (val == NULL)
    return NULL;

  if (g_str_has_prefix (val, "custom-")) 
    {
      name = val + strlen ("custom-");
      w = gtk_print_settings_get_paper_width (settings, GTK_UNIT_MM);
      h = gtk_print_settings_get_paper_height (settings, GTK_UNIT_MM);
      return gtk_paper_size_new_custom (name, name, w, h, GTK_UNIT_MM);
    }

  return gtk_paper_size_new (val);
}

void
gtk_print_settings_set_paper_size (GtkPrintSettings *settings,
				   GtkPaperSize *paper_size)
{
  char *custom_name;

  if (paper_size == NULL) 
    {
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT, NULL);
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_WIDTH, NULL);
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_HEIGHT, NULL);
    }
  else if (gtk_paper_size_is_custom (paper_size)) 
    {
      custom_name = g_strdup_printf ("custom-%s", 
				     gtk_paper_size_get_name (paper_size));
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT, custom_name);
      g_free (custom_name);
      gtk_print_settings_set_paper_width (settings, 
					  gtk_paper_size_get_width (paper_size, 
								    GTK_UNIT_MM),
					  GTK_UNIT_MM);
      gtk_print_settings_set_paper_height (settings, 
					   gtk_paper_size_get_height (paper_size, 
								      GTK_UNIT_MM),
					   GTK_UNIT_MM);
    } 
  else
    gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT, 
			    gtk_paper_size_get_name (paper_size));
}

double
gtk_print_settings_get_paper_width (GtkPrintSettings *settings,
				    GtkUnit unit)
{
  return gtk_print_settings_get_length (settings, GTK_PRINT_SETTINGS_PAPER_WIDTH, unit);
}

void
gtk_print_settings_set_paper_width (GtkPrintSettings *settings,
				    double width, 
				    GtkUnit unit)
{
  gtk_print_settings_set_length (settings, GTK_PRINT_SETTINGS_PAPER_WIDTH, width, unit);
}

double
gtk_print_settings_get_paper_height (GtkPrintSettings *settings,
				     GtkUnit unit)
{
  return gtk_print_settings_get_length (settings, 
					GTK_PRINT_SETTINGS_PAPER_HEIGHT,
					unit);
}

void
gtk_print_settings_set_paper_height (GtkPrintSettings *settings,
				     double height, 
				     GtkUnit unit)
{
  gtk_print_settings_set_length (settings, 
				 GTK_PRINT_SETTINGS_PAPER_HEIGHT, 
				 height, unit);
}

gboolean
gtk_print_settings_get_use_color (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool_with_default (settings, 
						   GTK_PRINT_SETTINGS_USE_COLOR,
						   TRUE);
}

void
gtk_print_settings_set_use_color (GtkPrintSettings *settings,
				  gboolean use_color)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_USE_COLOR, 
			       use_color);
}

gboolean
gtk_print_settings_get_collate (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool (settings, 
				      GTK_PRINT_SETTINGS_COLLATE);
}

void
gtk_print_settings_set_collate (GtkPrintSettings *settings,
				gboolean collate)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_COLLATE, 
			       collate);
}

gboolean
gtk_print_settings_get_reverse (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool (settings, 
				      GTK_PRINT_SETTINGS_REVERSE);
}

void
gtk_print_settings_set_reverse (GtkPrintSettings *settings,
				  gboolean reverse)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_REVERSE, 
			       reverse);
}

GtkPrintDuplex
gtk_print_settings_get_duplex (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_DUPLEX);

  if (val == NULL || (strcmp (val, "simplex") == 0))
    return GTK_PRINT_DUPLEX_SIMPLEX;

  if (strcmp (val, "horizontal") == 0)
    return GTK_PRINT_DUPLEX_HORIZONTAL;
  
  if (strcmp (val, "vertical") == 0)
    return GTK_PRINT_DUPLEX_HORIZONTAL;
  
  return GTK_PRINT_DUPLEX_SIMPLEX;
}

void
gtk_print_settings_set_duplex (GtkPrintSettings *settings,
			       GtkPrintDuplex      duplex)
{
  const char *str;

  switch (duplex)
    {
    default:
    case GTK_PRINT_DUPLEX_SIMPLEX:
      str = "simplex";
      break;
    case GTK_PRINT_DUPLEX_HORIZONTAL:
      str = "horizontal";
      break;
    case GTK_PRINT_DUPLEX_VERTICAL:
      str = "vertical";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_DUPLEX, str);
}

GtkPrintQuality
gtk_print_settings_get_quality (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_QUALITY);

  if (val == NULL || (strcmp (val, "normal") == 0))
    return GTK_PRINT_QUALITY_NORMAL;

  if (strcmp (val, "high") == 0)
    return GTK_PRINT_QUALITY_HIGH;
  
  if (strcmp (val, "low") == 0)
    return GTK_PRINT_QUALITY_LOW;
  
  if (strcmp (val, "draft") == 0)
    return GTK_PRINT_QUALITY_DRAFT;
  
  return GTK_PRINT_QUALITY_NORMAL;
}

void
gtk_print_settings_set_quality (GtkPrintSettings *settings,
				GtkPrintQuality     quality)
{
  const char *str;

  switch (quality)
    {
    default:
    case GTK_PRINT_QUALITY_NORMAL:
      str = "normal";
      break;
    case GTK_PRINT_QUALITY_HIGH:
      str = "high";
      break;
    case GTK_PRINT_QUALITY_LOW:
      str = "low";
      break;
    case GTK_PRINT_QUALITY_DRAFT:
      str = "draft";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_QUALITY, str);
}

GtkPageSet
gtk_print_settings_get_page_set (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PAGE_SET);

  if (val == NULL || (strcmp (val, "all") == 0))
    return GTK_PAGE_SET_ALL;

  if (strcmp (val, "even") == 0)
    return GTK_PAGE_SET_EVEN;
  
  if (strcmp (val, "odd") == 0)
    return GTK_PAGE_SET_ODD;
  
  return GTK_PAGE_SET_ALL;
}

void
gtk_print_settings_set_page_set (GtkPrintSettings *settings,
				 GtkPageSet          page_set)
{
  const char *str;

  switch (page_set)
    {
    default:
    case GTK_PAGE_SET_ALL:
      str = "all";
      break;
    case GTK_PAGE_SET_EVEN:
      str = "even";
      break;
    case GTK_PAGE_SET_ODD:
      str = "odd";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAGE_SET, str);
}

int
gtk_print_settings_get_num_copies (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_NUM_COPIES, 1);
}

void
gtk_print_settings_set_num_copies (GtkPrintSettings *settings,
				   int                 num_copies)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_NUM_COPIES,
			      num_copies);
}

int
gtk_print_settings_get_number_up (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int (settings, GTK_PRINT_SETTINGS_NUMBER_UP);
}

void
gtk_print_settings_set_number_up (GtkPrintSettings *settings,
				    int                 number_up)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_NUMBER_UP,
				number_up);
}

int
gtk_print_settings_get_resolution (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int (settings, GTK_PRINT_SETTINGS_RESOLUTION);
}

void
gtk_print_settings_set_resolution (GtkPrintSettings *settings,
				   int                 resolution)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION,
			      resolution);
}

double
gtk_print_settings_get_scale (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_double_with_default (settings,
						     GTK_PRINT_SETTINGS_SCALE,
						     100.0);
}

void
gtk_print_settings_set_scale (GtkPrintSettings *settings,
			      double scale)
{
  gtk_print_settings_set_double (settings, GTK_PRINT_SETTINGS_SCALE,
				 scale);
}

gboolean
gtk_print_settings_get_print_to_file (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool (settings, 
				      GTK_PRINT_SETTINGS_PRINT_TO_FILE);
}

void
gtk_print_settings_set_print_to_file (GtkPrintSettings *settings,
				      gboolean print_to_file)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_PRINT_TO_FILE, 
			       print_to_file);
}

GtkPrintPages
gtk_print_settings_get_print_pages (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PRINT_PAGES);

  if (val == NULL || (strcmp (val, "all") == 0))
    return GTK_PRINT_PAGES_ALL;

  if (strcmp (val, "current") == 0)
    return GTK_PRINT_PAGES_CURRENT;
  
  if (strcmp (val, "ranges") == 0)
    return GTK_PRINT_PAGES_RANGES;
  
  return GTK_PRINT_PAGES_ALL;
}

void
gtk_print_settings_set_print_pages (GtkPrintSettings *settings,
				    GtkPrintPages       print_pages)
{
  const char *str;

  switch (print_pages)
    {
    default:
    case GTK_PRINT_PAGES_ALL:
      str = "all";
      break;
    case GTK_PRINT_PAGES_CURRENT:
      str = "current";
      break;
    case GTK_PRINT_PAGES_RANGES:
      str = "ranges";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PRINT_PAGES, str);
}
     


GtkPageRange *
gtk_print_settings_get_page_ranges (GtkPrintSettings *settings,
				    int                *num_ranges)
{
  const char *val;
  gchar **range_strs;
  GtkPageRange *ranges;
  int i, n;
  
  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PAGE_RANGES);

  if (val == NULL)
    {
      *num_ranges = 0;
      return NULL;
    }
  
  range_strs = g_strsplit (val, ",", 0);

  for (i = 0; range_strs[i] != NULL; i++)
    ;

  n = i;

  ranges = g_new0 (GtkPageRange, n);

  for (i = 0; i < n; i++)
    {
      int start, end;
      char *str;

      start = (int)strtol (range_strs[i], &str, 10);
      end = start;

      if (*str == '-')
	{
	  str++;
	  end = (int)strtol (str, NULL, 10);
	  if (end < start)
	    end = start;
	}

      ranges[i].start = start;
      ranges[i].end = end;
    }

  *num_ranges = n;
  return ranges;
}

void
gtk_print_settings_set_page_ranges  (GtkPrintSettings *settings,
				     GtkPageRange       *page_ranges,
				     int                 num_ranges)
{
  GString *s;
  int i;
  
  s = g_string_new ("");

  for (i = 0; i < num_ranges; i++)
    {
      if (page_ranges[i].start == page_ranges[i].end)
	g_string_append_printf (s, "%d", page_ranges[i].start);
      else
	g_string_append_printf (s, "%d-%d",
				page_ranges[i].start,
				page_ranges[i].end);
      if (i < num_ranges - 1)
	g_string_append (s, ",");
    }

  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAGE_RANGES, 
			  s->str);

  g_string_free (s, TRUE);
}

const char *
gtk_print_settings_get_default_source (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE);
}

void
gtk_print_settings_set_default_source (GtkPrintSettings *settings,
				       const char *default_source)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE, default_source);
}
     
const char *
gtk_print_settings_get_media_type (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_MEDIA_TYPE);
}

/* The set of media types is defined in PWG 5101.1-2002 PWG */
void
gtk_print_settings_set_media_type (GtkPrintSettings *settings,
				   const char *media_type)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_MEDIA_TYPE, media_type);
}


const char *
gtk_print_settings_get_dither (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_DITHER);
}

void
gtk_print_settings_set_dither (GtkPrintSettings *settings,
			       const char *dither)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_DITHER, dither);
}
     
const char *
gtk_print_settings_get_finishings (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_FINISHINGS);
}

void
gtk_print_settings_set_finishings (GtkPrintSettings *settings,
				   const char *finishings)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_FINISHINGS, finishings);
}
     
const char *
gtk_print_settings_get_output_bin (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_BIN);
}

void
gtk_print_settings_set_output_bin (GtkPrintSettings *settings,
				   const char *output_bin)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_BIN, output_bin);
}
   
