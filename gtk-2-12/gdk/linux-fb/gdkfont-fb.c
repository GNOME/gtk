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

#include <config.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gdkfont.h"
#include "gdkprivate-fb.h"
#include "gdkpango.h"

#include <pango/pango.h>

#include <freetype/freetype.h>
#if !defined(FREETYPE_MAJOR) || FREETYPE_MAJOR != 2
#error "We need Freetype 2.0"
#endif

#ifdef EMULATE_GDKFONT
static GHashTable *font_name_hash = NULL;
static GHashTable *fontset_name_hash = NULL;

static void
gdk_font_hash_insert (GdkFontType type, GdkFont *font)
{
  GdkFontPrivateFB *private = (GdkFontPrivateFB *)font;
  
  GHashTable **hashp = (type == GDK_FONT_FONT) ?
    &font_name_hash : &fontset_name_hash;

  if (!*hashp)
    *hashp = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_insert (*hashp, private->name, font);
}

static void
gdk_font_hash_remove (GdkFontType type, GdkFont *font)
{
  GdkFontPrivateFB *private = (GdkFontPrivateFB *)font;
  
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    font_name_hash : fontset_name_hash;

  g_hash_table_remove (hash, private->name);
}

static GdkFont *
gdk_font_hash_lookup (GdkFontType type, const gchar *font_name)
{
  GdkFont *result;
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    font_name_hash : fontset_name_hash;

  if (!hash)
    return NULL;
  else
    {
      result = g_hash_table_lookup (hash, font_name);
      if (result)
	gdk_font_ref (result);
      
      return result;
    }
}

GdkFont*
gdk_font_from_description_for_display (GdkDisplay           *display,
				       PangoFontDescription *desc)
{
  GdkFont *font;
  GdkFontPrivateFB *private;
  PangoFont *pango_font;
  PangoContext *context;
  PangoFontMetrics *metrics;
  PangoLanguage *lang;
  
  g_return_val_if_fail (desc, NULL);

  private = g_new0 (GdkFontPrivateFB, 1);
  private->base.ref_count = 1;
  private->name = NULL;
  
  font = (GdkFont*) private;
  font->type = GDK_FONT_FONT;

  context = gdk_pango_context_get ();
  pango_context_set_base_dir (context, PANGO_DIRECTION_LTR);
  pango_context_set_language (context, pango_language_from_string ("UNKNOWN"));

  pango_font = pango_context_load_font (context, desc);
  if (!pango_font)
    {
      desc = pango_font_description_copy (desc);
      pango_font_description_set_family (desc, "sans");
      pango_font = pango_context_load_font (context, desc);
      if (!pango_font)
	{
	  pango_font_description_set_style (desc, PANGO_STYLE_NORMAL);
	  pango_font_description_set_weight (desc, PANGO_WEIGHT_NORMAL);
	  pango_font_description_set_variant (desc, PANGO_VARIANT_NORMAL);
	  pango_font_description_set_stretch (desc, PANGO_STRETCH_NORMAL);
	  pango_font = pango_context_load_font (context, desc);
	}
      pango_font_description_free (desc);
    }
  
  g_assert (pango_font != NULL);

  if (pango_font == NULL)
    {
      g_free (private);
      return NULL;
    }
  
  lang = pango_context_get_language (context);
  metrics = pango_font_get_metrics (pango_font, lang);

  private->pango_font = pango_font;
  
  g_free (lang);
  g_object_unref (context);

  font->ascent = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics));
  font->descent = PANGO_PIXELS (pango_font_metrics_get_descent (metrics));

  g_assert ((font->ascent > 0) || (font->descent > 0));

  pango_font_metrics_unref (metrics);
  
  return font;
}


GdkFont*
gdk_font_load_for_display (GdkDisplay  *display,
			   const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivateFB *private;
  PangoFontDescription *desc;
  gchar **pieces;

  g_return_val_if_fail (font_name != NULL, NULL);

  font = gdk_font_hash_lookup (GDK_FONT_FONT, font_name);
  if (font)
    return font;

  desc = pango_font_description_new ();
  
  pieces = g_strsplit (font_name, "-", 8);

  do {
    if (!pieces[0])
      break;
    
    if (!pieces[1])
      break;
    
    if (!pieces[2])
      break;

    if (strcmp (pieces[2], "*")!=0)
      pango_font_description_set_family (desc, pieces[2]);
    
    if (!pieces[3])
      break;
    
    if (strcmp (pieces[3], "light")==0)
      pango_font_description_set_weight (desc, PANGO_WEIGHT_LIGHT);
    if (strcmp (pieces[3], "medium")==0)
      pango_font_description_set_weight (desc, PANGO_WEIGHT_NORMAL);
    if (strcmp (pieces[3], "bold")==0)
      pango_font_description_set_weight (desc, PANGO_WEIGHT_BOLD);
    
    if (!pieces[4])
      break;
    
    if (strcmp (pieces[4], "r")==0)
      pango_font_description_set_style (desc, PANGO_STYLE_NORMAL);
    if (strcmp (pieces[4], "i")==0)
      pango_font_description_set_style (desc, PANGO_STYLE_ITALIC);
    if (strcmp (pieces[4], "o")==0)
      pango_font_description_set_style (desc, PANGO_STYLE_OBLIQUE);
    
    if (!pieces[5])
      break;
    if (!pieces[6])
      break;
    if (!pieces[7])
      break;

    if (strcmp (pieces[7], "*")!=0)
      pango_font_description_set_size (desc, atoi (pieces[7]) * PANGO_SCALE);
    if (pango_font_description_get_size (desc) == 0)
      pango_font_description_set_size (desc, 12 * PANGO_SCALE);
    
  } while (0);
  
  font = gdk_font_from_description (desc);
  private = (GdkFontPrivateFB*) font;
  private->name = g_strdup (font_name);

  gdk_font_hash_insert (GDK_FONT_FONT, font);

  g_strfreev (pieces);

  pango_font_description_free (desc);
  
  return font;
}

GdkFont*
gdk_fontset_load (const gchar *fontset_name)
{
  return gdk_font_load(fontset_name);
}

void
_gdk_font_destroy (GdkFont *font)
{
  GdkFontPrivateFB *private = (GdkFontPrivateFB *)font;
  gdk_font_hash_remove (font->type, font);

  g_object_unref (private->pango_font);
  g_free (private->name);
  g_free (font);
}

gint
_gdk_font_strlen (GdkFont     *font,
		  const gchar *str)
{
  GdkFontPrivateFB *font_private;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (str != NULL, -1);

  font_private = (GdkFontPrivateFB*) font;
  
  return strlen (str);
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  gint width = -1;
  gdk_text_extents (font, text, text_length, NULL, NULL, &width, NULL, NULL);
  return width;
}

/* Assumes text is in Latin-1 for performance reasons.
   If you need another encoding, use pangofont */
void
gdk_text_extents (GdkFont     *font,
                  const gchar *text,
                  gint         text_length,
		  gint        *lbearing,
		  gint        *rbearing,
		  gint        *width,
		  gint        *ascent,
		  gint        *descent)
{
  GdkFontPrivateFB *private;
  guchar *utf8, *utf8_end;
  PangoGlyphString *glyphs = pango_glyph_string_new ();
  PangoEngineShape *shaper, *last_shaper;
  PangoAnalysis analysis;
  guchar *p, *start;
  int i;
  
  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateFB*) font;

  if(ascent)
    *ascent = 0;
  if(descent)
    *descent = 0;
  if(width)
    *width = 0;
  if(lbearing)
    *lbearing = 0;
  if(rbearing)
    *rbearing = 0;

  utf8 = alloca (text_length*2);

  /* Convert latin-1 to utf8 */
  p = utf8;
  for (i = 0; i < text_length; i++)
    {
      if (text[i]==0)
	*p++ = 1; /* Hack to handle embedded nulls */
      else
	{
	  if(((guchar)text[i])<128)
	    *p++ = text[i];
	  else
	    {
	      *p++ = ((((guchar)text[i])>>6) & 0x3f) | 0xC0;
	      *p++ = (((guchar)text[i]) & 0x3f) | 0x80;
	    }
	}
    }
  utf8_end = p;

  last_shaper = NULL;
  shaper = NULL;

  p = start = utf8;
  while (p < utf8_end)
    {
      gunichar wc = g_utf8_get_char (p);
      p = g_utf8_next_char (p);
      shaper = pango_font_find_shaper (private->pango_font, pango_language_from_string ("fr"), wc);
      if (shaper != last_shaper)
	{
	  analysis.shape_engine = shaper;
	  analysis.lang_engine = NULL;
	  analysis.font = private->pango_font;
	  analysis.level = 0;

	  pango_shape (start, p - start, &analysis, glyphs);

	  for (i = 0; i < glyphs->num_glyphs; i++)
	    {
	      PangoRectangle ink_rect;
	      PangoGlyphGeometry *geometry = &glyphs->glyphs[i].geometry;
	      
	      pango_font_get_glyph_extents (private->pango_font, glyphs->glyphs[i].glyph,
					    &ink_rect, NULL);
	      
	      if(ascent)
		*ascent = MAX (*ascent, ink_rect.y);
	      if(descent)
		*descent = MAX (*descent, ink_rect.height - ink_rect.y);
	      if(width)
		*width += geometry->width;
	      if(lbearing)
		*lbearing = 0;
	      if(rbearing)
		*rbearing = 0;
	      
	    }
	  
	  start = p;
	}

      last_shaper = shaper;
    }

  if (p > start)
    {
      analysis.shape_engine = shaper;
      analysis.lang_engine = NULL;
      analysis.font = private->pango_font;
      analysis.level = 0;
      
      pango_shape (start, p - start, &analysis, glyphs);
      
      for (i = 0; i < glyphs->num_glyphs; i++)
	{
	  PangoRectangle ink_rect;
	  PangoGlyphGeometry *geometry = &glyphs->glyphs[i].geometry;
	  
	  pango_font_get_glyph_extents (private->pango_font, glyphs->glyphs[i].glyph,
					&ink_rect, NULL);
	  
	  if(ascent)
	    *ascent = MAX (*ascent, ink_rect.y);
	  if(descent)
	    *descent = MAX (*descent, ink_rect.height - ink_rect.y);
	  if(width)
	    *width += geometry->width;
	  if(lbearing)
	    *lbearing = 0;
	  if(rbearing)
	    *rbearing = 0;
	}
    }


  
  pango_glyph_string_free (glyphs);

  if(ascent)
    *ascent = PANGO_PIXELS (*ascent);
  if(descent)
    *descent = PANGO_PIXELS(*descent);
  if(width)
    *width = PANGO_PIXELS (*width);
  if(lbearing)
    *lbearing = PANGO_PIXELS (*lbearing);
  if(rbearing)
    *rbearing = PANGO_PIXELS (*rbearing);
}

#else

/* Don't emulate GdkFont */
static GdkFont *
gdk_fb_bogus_font (gint height)
{
  GdkFont *font;
  GdkFontPrivateFB *private;

  private = g_new0 (GdkFontPrivateFB, 1);
  font = (GdkFont *)private;
  
  font->type = GDK_FONT_FONT;
  font->ascent = height*3/4;
  font->descent = height/4;
  private->size = height;
  private->base.ref_count = 1;
  return font;
}

GdkFont*
gdk_font_from_description (PangoFontDescription *font_desc)
{
  g_return_val_if_fail (font_desc, NULL);

  return gdk_fb_bogus_font (PANGO_PIXELS (pango_font_description_get_size (font_desc)));
}

GdkFont*
gdk_fontset_load (const gchar *fontset_name)
{
  return gdk_fb_bogus_font (10);
}

GdkFont *
gdk_font_load (const gchar *font_name)
{
  return gdk_fb_bogus_font (10);
}

void
_gdk_font_destroy (GdkFont *font)
{
  g_free (font);
}

gint
_gdk_font_strlen (GdkFont     *font,
		  const gchar *str)
{
  GdkFontPrivateFB *font_private;
  gint length = 0;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (str != NULL, -1);

  font_private = (GdkFontPrivateFB*) font;
  
  return strlen (str);
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  GdkFontPrivateFB *private;

  private = (GdkFontPrivateFB*) font;

  return (text_length * private->size) / 2;
}

void
gdk_text_extents (GdkFont     *font,
                  const gchar *text,
                  gint         text_length,
		  gint        *lbearing,
		  gint        *rbearing,
		  gint        *width,
		  gint        *ascent,
		  gint        *descent)
{
  if(ascent)
    *ascent = font->ascent;
  if(descent)
    *descent = font->descent;
  if(width)
    *width = gdk_text_width(font, text, text_length);
  if(lbearing)
    *lbearing = 0;
  if(rbearing)
    *rbearing = 0;
}


#endif

gint
gdk_font_id (const GdkFont *font)
{
  const GdkFontPrivateFB *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (const GdkFontPrivateFB*) font;

  if (font->type == GDK_FONT_FONT)
    {
      return -1;
    }
  else
    {
      return 0;
    }
}

gint
gdk_font_equal (const GdkFont *fonta,
                const GdkFont *fontb)
{
  const GdkFontPrivateFB *privatea;
  const GdkFontPrivateFB *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (const GdkFontPrivateFB*) fonta;
  privateb = (const GdkFontPrivateFB*) fontb;

  if(fonta == fontb)
    return TRUE;

  return FALSE;
}


gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
  return 0;
}


void
gdk_text_extents_wc (GdkFont        *font,
		     const GdkWChar *text,
		     gint            text_length,
		     gint           *lbearing,
		     gint           *rbearing,
		     gint           *width,
		     gint           *ascent,
		     gint           *descent)
{
  g_warning ("gdk_text_extents_wc() is not implemented\n");
  return;
}
