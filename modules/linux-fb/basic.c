/* Pango
 * basic.c:
 *
 * Copyright (C) 1999 Red Hat Software
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <gconvert.h>
#include <pango/pango.h>
#include <pango/pango-utils.h>
#include "gdkprivate-fb.h"

PangoGlyph
pango_fb_get_unknown_glyph(PangoFont *font)
{
  return FT_Get_Char_Index (PANGO_FB_FONT (font)->ftf, '~');
}

typedef struct _CharRange CharRange;
typedef struct _Charset Charset;
typedef struct _CharCache CharCache;
typedef struct _MaskTable MaskTable;

#define MAX_CHARSETS 32

typedef PangoGlyph (*ConvFunc) (CharCache *cache,
				Charset *charset,
				const gchar *input);
struct _CharRange
{
  guint16 start;
  guint16 end;
  guint16 charsets;
};

struct _MaskTable 
{
  int n_subfonts;

  Charset **charsets;
};

#define MAX_CHARSETS 32

struct _CharCache 
{
#if 0
  MaskTable *mask_tables[256];
#endif
  GIConv converters[MAX_CHARSETS];
};

struct _Charset
{
  int   index;
  char *id;
  char *x_charset;
  ConvFunc conv_func;
};

static PangoGlyph conv_8bit (CharCache  *cache,
			     Charset    *charset,
			     const char *input);
static PangoGlyph conv_euc (CharCache  *cache,
			    Charset    *charset,
			    const char *input);
static PangoGlyph conv_ucs4 (CharCache  *cache,
			     Charset    *charset,
			     const char *input);

#include "tables-big.i"

static PangoEngineInfo script_engines[] = {
  {
    "BasicScriptEngineLang",
    PANGO_ENGINE_TYPE_LANG,
    PANGO_RENDER_TYPE_NONE,
    basic_ranges, G_N_ELEMENTS(basic_ranges)
  },
  {
    "BasicScriptEngineFB",
    PANGO_ENGINE_TYPE_SHAPE,
    "PangoRenderTypeFB",
    basic_ranges, G_N_ELEMENTS(basic_ranges)
  }
};

static gint n_script_engines = G_N_ELEMENTS (script_engines);

/*
 * Language script engine
 */

static void 
basic_engine_break (const char     *text,
		    gint            len,
		    PangoAnalysis  *analysis,
		    PangoLogAttr   *attrs)
{
}

static PangoEngine *
basic_engine_lang_new ()
{
  PangoEngineLang *result;
  
  result = g_new (PangoEngineLang, 1);

  result->engine.id = "BasicScriptEngine";
  result->engine.type = PANGO_ENGINE_TYPE_LANG;
  result->engine.length = sizeof (result);
  result->script_break = basic_engine_break;

  return (PangoEngine *)result;
}

/*
 * FB window system script engine portion
 */

static CharCache *
char_cache_new (void)
{
  CharCache *result;
  int i;

  result = g_new0 (CharCache, 1);

  for (i=0; i < MAX_CHARSETS; i++)
    result->converters[i] = (GIConv)-1;

  return result;
}

static void
char_cache_free (CharCache *cache)
{
  int i;

#if 0
  for (i=0; i < 256; i++)
    if (cache->mask_tables[i])
      {
	g_free (cache->mask_tables[i]->subfonts);
	g_free (cache->mask_tables[i]->charsets);
	
	g_free (cache->mask_tables[i]);
      }
#endif

  for (i=0; i<MAX_CHARSETS; i++)
    if (cache->converters[i] != (GIConv)-1)
      g_iconv_close (cache->converters[i]);
  
  g_free (cache);
}

PangoGlyph 
find_char (CharCache *cache, PangoFont *font, gunichar wc, const char *input)
{
  return FT_Get_Char_Index(PANGO_FB_FONT(font)->ftf, wc);
}

static void
set_glyph (PangoFont *font, PangoGlyphString *glyphs, int i, int offset, PangoGlyph glyph)
{
  PangoRectangle logical_rect;

  glyphs->glyphs[i].glyph = glyph;
  
  glyphs->glyphs[i].geometry.x_offset = 0;
  glyphs->glyphs[i].geometry.y_offset = 0;

  glyphs->log_clusters[i] = offset;

  pango_font_get_glyph_extents (font, glyphs->glyphs[i].glyph, NULL, &logical_rect);
  glyphs->glyphs[i].geometry.width = logical_rect.width;
}

static GIConv
find_converter (CharCache *cache, Charset *charset)
{
  GIConv cd = cache->converters[charset->index];
  if (cd == (GIConv)-1)
    {
      cd = g_iconv_open  (charset->id, "UTF-8");
      g_assert (cd != (GIConv)-1);
      cache->converters[charset->index] = cd;
    }

  return cd;
}

static PangoGlyph
conv_8bit (CharCache  *cache,
	   Charset *charset,
	   const char *input)
{
  GIConv cd;
  char outbuf;
  
  const char *inptr = input;
  size_t inbytesleft;
  char *outptr = &outbuf;
  size_t outbytesleft = 1;

  inbytesleft = g_utf8_next_char(input) - input;
  
  cd = find_converter (cache, charset);

  g_iconv (cd, (gchar **)&inptr, &inbytesleft, &outptr, &outbytesleft);

  return (guchar)outbuf;
}

static PangoGlyph
conv_euc (CharCache  *cache,
	  Charset     *charset,
	  const char *input)
{
  GIConv cd;
  char outbuf[2];

  const char *inptr = input;
  size_t inbytesleft;
  char *outptr = outbuf;
  size_t outbytesleft = 2;

  inbytesleft = g_utf8_next_char(input) - input;
  
  cd = find_converter (cache, charset);

  g_iconv (cd, (gchar **)&inptr, &inbytesleft, &outptr, &outbytesleft);

  if ((guchar)outbuf[0] < 128)
    return outbuf[0];
  else
    return ((guchar)outbuf[0] & 0x7f) * 256 + ((guchar)outbuf[1] & 0x7f);
}

static PangoGlyph
conv_ucs4 (CharCache  *cache,
	   Charset     *charset,
	   const char *input)
{
  return g_utf8_get_char (input);
}

static void
swap_range (PangoGlyphString *glyphs, int start, int end)
{
  int i, j;
  
  for (i = start, j = end - 1; i < j; i++, j--)
    {
      PangoGlyphInfo glyph_info;
      gint log_cluster;
      
      glyph_info = glyphs->glyphs[i];
      glyphs->glyphs[i] = glyphs->glyphs[j];
      glyphs->glyphs[j] = glyph_info;
      
      log_cluster = glyphs->log_clusters[i];
      glyphs->log_clusters[i] = glyphs->log_clusters[j];
      glyphs->log_clusters[j] = log_cluster;
    }
}

static CharCache *
get_char_cache (PangoFont *font)
{
  GQuark cache_id = g_quark_from_string ("basic-char-cache");
  
  CharCache *cache = g_object_get_qdata (G_OBJECT (font), cache_id);
  if (!cache)
    {
      cache = char_cache_new ();
      g_object_set_qdata_full (G_OBJECT (font), cache_id, 
			       cache, (GDestroyNotify)char_cache_free);
    }

  return cache;
}

static void 
basic_engine_shape (PangoFont        *font,
		    const char       *text,
		    gint              length,
		    PangoAnalysis    *analysis,
		    PangoGlyphString *glyphs)
{
  int n_chars;
  int i;
  const char *p;

  CharCache *cache;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);
  g_return_if_fail (length >= 0);
  g_return_if_fail (analysis != NULL);

  cache = get_char_cache (font);

  n_chars = g_utf8_strlen (text, length);
  pango_glyph_string_set_size (glyphs, n_chars);

  p = text;
  for (i=0; i < n_chars; i++)
    {
      gunichar wc;
      gunichar mirrored_ch;
      PangoGlyph index;
      char buf[6];
      const char *input;

      wc = g_utf8_get_char(p);

      input = p;
      if (analysis->level % 2)
	if (pango_get_mirror_char (wc, &mirrored_ch))
	  {
	    wc = mirrored_ch;
	    
	    g_unichar_to_utf8 (wc, buf);
	    input = buf;
	  }

      if (wc == 0x200B || wc == 0x200E || wc == 0x200F)	/* Zero-width characters */
	{
	  set_glyph (font, glyphs, i, p - text, 0);
	}
      else
	{
	  index = find_char (cache, font, wc, input);
	  if (index)
	    {
	      set_glyph (font, glyphs, i, p - text, index);
	      
	      if (g_unichar_type (wc) == G_UNICODE_NON_SPACING_MARK)
		{
		  if (i > 0)
		    {
		      PangoRectangle logical_rect, ink_rect;
		      
		      glyphs->glyphs[i].geometry.width = MAX (glyphs->glyphs[i-1].geometry.width,
							      glyphs->glyphs[i].geometry.width);
		      glyphs->glyphs[i-1].geometry.width = 0;
		      glyphs->log_clusters[i] = glyphs->log_clusters[i-1];

		      /* Some heuristics to try to guess how overstrike glyphs are
		       * done and compensate
		       */
		      pango_font_get_glyph_extents (font, glyphs->glyphs[i].glyph, &ink_rect, &logical_rect);
		      if (logical_rect.width == 0 && ink_rect.x == 0)
			glyphs->glyphs[i].geometry.x_offset = (glyphs->glyphs[i].geometry.width - ink_rect.width) / 2;
		    }
		}
	    }
	  else
	    set_glyph (font, glyphs, i, p - text, pango_fb_get_unknown_glyph (font));
	}
      
      p = g_utf8_next_char(p);
    }

  /* Simple bidi support... may have separate modules later */

  if (analysis->level % 2)
    {
      int start, end;

      /* Swap all glyphs */
      swap_range (glyphs, 0, n_chars);
      
      /* Now reorder glyphs within each cluster back to LTR */
      for (start=0; start<n_chars;)
	{
	  end = start;
	  while (end < n_chars &&
		 glyphs->log_clusters[end] == glyphs->log_clusters[start])
	    end++;
	  
	  swap_range (glyphs, start, end);
	  start = end;
	}
    }
}

static PangoCoverage *
basic_engine_get_coverage (PangoFont  *font,
			   const char *lang)
{
  CharCache *cache = get_char_cache (font);
  PangoCoverage *result = pango_coverage_new ();
  gunichar wc;

  for (wc = 0; wc < 65536; wc++)
    {
      char buf[6];

      g_unichar_to_utf8 (wc, buf);
      if (find_char (cache, font, wc, buf))
	pango_coverage_set (result, wc, PANGO_COVERAGE_EXACT);
    }

  return result;
}

static PangoEngine *
basic_engine_fb_new ()
{
  PangoEngineShape *result;
  
  result = g_new (PangoEngineShape, 1);

  result->engine.id = "BasicScriptEngineFB";
  result->engine.type = PANGO_ENGINE_TYPE_LANG;
  result->engine.length = sizeof (result);
  result->script_shape = basic_engine_shape;
  result->get_coverage = basic_engine_get_coverage;

  return (PangoEngine *)result;
}

/* The following three functions provide the public module API for
 * Pango
 */
#ifdef MODULE_PREFIX
#define MODULE_ENTRY(func) _pango_basic_##func
#else
#define MODULE_ENTRY(func) func
#endif

void 
MODULE_ENTRY(script_engine_list) (PangoEngineInfo **engines, gint *n_engines)
{
  *engines = script_engines;
  *n_engines = n_script_engines;
}

PangoEngine *
MODULE_ENTRY(script_engine_load) (const char *id)
{
  if (!strcmp (id, "BasicScriptEngineFB"))
    return basic_engine_fb_new ();
  else if (!strcmp (id, "BasicScriptEngineLang"))
    return basic_engine_lang_new ();
  else
    return NULL;
}

void 
MODULE_ENTRY(script_engine_unload) (PangoEngine *engine)
{
}

