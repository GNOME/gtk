/* GDK - The GIMP Drawing Kit
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

#undef GDK_DISABLE_DEPRECATED

#include "config.h"
#include "gdkdisplay.h"
#include "gdkfont.h"
#include "gdkinternals.h"
#include "gdkalias.h"

GType
gdk_font_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static (g_intern_static_string ("GdkFont"),
					     (GBoxedCopyFunc)gdk_font_ref,
					     (GBoxedFreeFunc)gdk_font_unref);
  return our_type;
}

/**
 * gdk_font_ref:
 * @font: a #GdkFont
 * 
 * Increases the reference count of a font by one.
 * 
 * Return value: @font
 **/
GdkFont*
gdk_font_ref (GdkFont *font)
{
  GdkFontPrivate *private;

  g_return_val_if_fail (font != NULL, NULL);

  private = (GdkFontPrivate*) font;
  private->ref_count += 1;
  return font;
}

/**
 * gdk_font_unref:
 * @font: a #GdkFont
 * 
 * Decreases the reference count of a font by one.
 * If the result is zero, destroys the font.
 **/
void
gdk_font_unref (GdkFont *font)
{
  GdkFontPrivate *private;
  private = (GdkFontPrivate*) font;

  g_return_if_fail (font != NULL);
  g_return_if_fail (private->ref_count > 0);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    _gdk_font_destroy (font);
}

/**
 * gdk_font_from_description:
 * @font_desc: a #PangoFontDescription.
 * 
 * Load a #GdkFont based on a Pango font description. This font will
 * only be an approximation of the Pango font, and
 * internationalization will not be handled correctly. This function
 * should only be used for legacy code that cannot be easily converted
 * to use Pango. Using Pango directly will produce better results.
 * 
 * Return value: the newly loaded font, or %NULL if the font
 * cannot be loaded.
 **/
GdkFont*
gdk_font_from_description (PangoFontDescription *font_desc)
{
  return gdk_font_from_description_for_display (gdk_display_get_default (),font_desc);
}

/**
 * gdk_font_load:
 * @font_name: a XLFD describing the font to load.
 * 
 * Loads a font.
 * 
 * The font may be newly loaded or looked up the font in a cache. 
 * You should make no assumptions about the initial reference count.
 * 
 * Return value: a #GdkFont, or %NULL if the font could not be loaded.
 **/
GdkFont*
gdk_font_load (const gchar *font_name)
{  
   return gdk_font_load_for_display (gdk_display_get_default(), font_name);
}

#define __GDK_FONT_C__
#include "gdkaliasdef.c"
