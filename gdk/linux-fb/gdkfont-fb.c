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

#include <math.h>
#include <stdlib.h>
#include "gdkfont.h"
#include "gdkprivate-fb.h"
#include "gdkpango.h"

#include <pango/pango.h>

#include <freetype/freetype.h>
#if !defined(FREETYPE_MAJOR) || FREETYPE_MAJOR != 2
#error "We need Freetype 2.0 (beta?)"
#endif

GdkFont*
gdk_font_from_description (PangoFontDescription *font_desc)
{
  GdkFont *font;
  GdkFontPrivateFB *private;

  g_return_val_if_fail(font_desc, NULL);

  private = g_new0(GdkFontPrivateFB, 1);
  font = (GdkFont *)private;
  private->base.ref_count = 1;

  return font;
}

/* ********************* */
#if 0
static GHashTable *font_name_hash = NULL;
static GHashTable *fontset_name_hash = NULL;

static void
gdk_font_hash_insert (GdkFontType type, GdkFont *font, const gchar *font_name)
{
  GdkFontPrivateFB *private = (GdkFontPrivateFB *)font;
  GHashTable **hashp = (type == GDK_FONT_FONT) ?
    &font_name_hash : &fontset_name_hash;

  if (!*hashp)
    *hashp = g_hash_table_new (g_str_hash, g_str_equal);

  private->names = g_slist_prepend (private->names, g_strdup (font_name));
  g_hash_table_insert (*hashp, private->names->data, font);
}

static void
gdk_font_hash_remove (GdkFontType type, GdkFont *font)
{
  GdkFontPrivateFB *private = (GdkFontPrivateFB *)font;
  GSList *tmp_list;
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    font_name_hash : fontset_name_hash;

  tmp_list = private->names;
  while (tmp_list)
    {
      g_hash_table_remove (hash, tmp_list->data);
      g_free (tmp_list->data);
      
      tmp_list = tmp_list->next;
    }

  g_slist_free (private->names);
  private->names = NULL;
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
gdk_font_load (const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivateFB *private;

  g_return_val_if_fail (font_name != NULL, NULL);

  font = gdk_font_hash_lookup (GDK_FONT_FONTSET, font_name);
  if (font)
    return font;

  {
    char **pieces;
    BBox bb;
    
    private = g_new0 (GdkFontPrivateFB, 1);
    private->base.ref_count = 1;
    private->names = NULL;

    pieces = g_strsplit(font_name, "-", 2);
    if(pieces[1])
      {
	private->size = atof(pieces[1]);
	private->t1_font_id = T1_AddFont(pieces[0]);
      }
    else
      private->t1_font_id = T1_AddFont((char *)font_name);
    g_strfreev(pieces);

    T1_LoadFont(private->t1_font_id);
    CreateNewFontSize(private->t1_font_id, private->size, FALSE);

    font = (GdkFont*) private;
    font->type = GDK_FONT_FONTSET;

    bb = T1_GetFontBBox(private->t1_font_id);

    font->ascent = ((double)bb.ury) / 1000.0 * private->size;
    font->descent = ((double)bb.lly) / -1000.0 * private->size;
  }

  gdk_font_hash_insert (GDK_FONT_FONTSET, font, font_name);

  return font;
}

GdkFont*
gdk_fontset_load (const gchar *fontset_name)
{
  return gdk_font_load(fontset_name);
}
#endif

void
_gdk_font_destroy (GdkFont *font)
{
#if 0
  gdk_font_hash_remove (font->type, font);
#endif
      
  switch (font->type)
    {
    case GDK_FONT_FONT:
      break;
    case GDK_FONT_FONTSET:
      break;
    default:
      g_error ("unknown font type.");
      break;
    }
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

  if (font->type == GDK_FONT_FONT)
    {
      guint16 *string_2b = (guint16 *)str;
	    
      while (*(string_2b++))
	length++;
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      length = strlen (str);
    }
  else
    g_error("undefined font type\n");

  return length;
}

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
#if 0
  if(privatea->t1_font_id == privateb->t1_font_id
     && privatea->size == privateb->size)
    return TRUE;
#endif

  return FALSE;
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
#if 0
  GdkFontPrivateFB *private;
  gint width;
  double n;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivateFB*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
    case GDK_FONT_FONTSET:
      n = private->size / 1000.0;
      n *= T1_GetStringWidth(private->t1_font_id, (char *)text, text_length, 0, T1_KERNING);
      width = ceil(n);
      break;
    default:
      width = 0;
      break;
    }

  return width;
#else
  return 0;
#endif
}

gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
#if 0
  char *realstr;
  int i;

  realstr = alloca(text_length + 1);
  for(i = 0; i < text_length; i++)
    realstr[i] = text[i];
  realstr[i] = '\0';

  return gdk_text_width(font, realstr, text_length);
#else
  return 0;
#endif
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
#if 0
  GdkFontPrivateFB *private;
  METRICSINFO mi;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateFB*) font;

  mi = T1_GetMetricsInfo(private->t1_font_id, (char *)text, text_length, 0, T1_KERNING);

  if(ascent)
    *ascent = ((double)mi.bbox.ury) / 1000.0 * private->size;
  if(descent)
    *descent = ((double)mi.bbox.lly) / -1000.0 * private->size;
  if(width)
    *width = ((double)mi.width) / 1000.0 * private->size;
  if(lbearing)
    *lbearing = ((double)mi.bbox.llx) / 1000.0 * private->size;
  if(rbearing)
    *rbearing = ((double)mi.bbox.urx) / 1000.0 * private->size;
#else
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
#endif
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
  char *realstr;
  int i;

  realstr = alloca(text_length + 1);
  for(i = 0; i < text_length; i++)
    realstr[i] = text[i];
  realstr[i] = '\0';

  return gdk_text_extents(font, realstr, text_length, lbearing, rbearing, width, ascent, descent);
}
