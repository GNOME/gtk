/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include "gdkfont.h"
#include "gdkx.h"

static GHashTable *font_name_hash = NULL;
static GHashTable *fontset_name_hash = NULL;

static void
gdk_font_hash_insert (GdkFontType type, GdkFont *font, const gchar *font_name)
{
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;
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
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;
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
  GdkFontPrivateX *private;
  XFontStruct *xfont;

  g_return_val_if_fail (font_name != NULL, NULL);

  font = gdk_font_hash_lookup (GDK_FONT_FONT, font_name);
  if (font)
    return font;

  xfont = XLoadQueryFont (gdk_display, font_name);
  if (xfont == NULL)
    return NULL;

  font = gdk_font_lookup (xfont->fid);
  if (font != NULL)
    {
      private = (GdkFontPrivateX *) font;
      if (xfont != private->xfont)
	XFreeFont (gdk_display, xfont);

      gdk_font_ref (font);
    }
  else
    {
      private = g_new (GdkFontPrivateX, 1);
      private->xdisplay = gdk_display;
      private->xfont = xfont;
      private->base.ref_count = 1;
      private->names = NULL;
 
      font = (GdkFont*) private;
      font->type = GDK_FONT_FONT;
      font->ascent =  xfont->ascent;
      font->descent = xfont->descent;

      gdk_xid_table_insert (&xfont->fid, font);
    }

  gdk_font_hash_insert (GDK_FONT_FONT, font, font_name);

  return font;
}

GdkFont*
gdk_fontset_load (gchar *fontset_name)
{
  GdkFont *font;
  GdkFontPrivateX *private;
  XFontSet fontset;
  gint  missing_charset_count;
  gchar **missing_charset_list;
  gchar *def_string;

  font = gdk_font_hash_lookup (GDK_FONT_FONTSET, fontset_name);
  if (font)
    return font;

  private = g_new (GdkFontPrivateX, 1);
  font = (GdkFont*) private;

  private->xdisplay = gdk_display;
  fontset = XCreateFontSet (gdk_display, fontset_name,
			    &missing_charset_list, &missing_charset_count,
			    &def_string);

  if (missing_charset_count)
    {
      gint i;
      g_warning ("Missing charsets in FontSet creation\n");
      for (i=0;i<missing_charset_count;i++)
	g_warning ("    %s\n", missing_charset_list[i]);
      XFreeStringList (missing_charset_list);
    }

  private->base.ref_count = 1;

  if (!fontset)
    {
      g_free (font);
      return NULL;
    }
  else
    {
      gint num_fonts;
      gint i;
      XFontStruct **font_structs;
      gchar **font_names;
      
      private->xfont = fontset;
      font->type = GDK_FONT_FONTSET;
      num_fonts = XFontsOfFontSet (fontset, &font_structs, &font_names);

      font->ascent = font->descent = 0;
      
      for (i = 0; i < num_fonts; i++)
	{
	  font->ascent = MAX (font->ascent, font_structs[i]->ascent);
	  font->descent = MAX (font->descent, font_structs[i]->descent);
	}

      private->names = NULL;
      gdk_font_hash_insert (GDK_FONT_FONTSET, font, fontset_name);
      
      return font;
    }
}

void
_gdk_font_destroy (GdkFont *font)
{
  GdkFontPrivateX *private = (GdkFontPrivateX *)font;
  
  gdk_font_hash_remove (font->type, font);
      
  switch (font->type)
    {
    case GDK_FONT_FONT:
      gdk_xid_table_remove (((XFontStruct *) private->xfont)->fid);
      XFreeFont (private->xdisplay, (XFontStruct *) private->xfont);
      break;
    case GDK_FONT_FONTSET:
      XFreeFontSet (private->xdisplay, (XFontSet) private->xfont);
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
  GdkFontPrivateX *font_private;
  gint length = 0;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (str != NULL, -1);

  font_private = (GdkFontPrivateX*) font;

  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) font_private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  length = strlen (str);
	}
      else
	{
	  guint16 *string_2b = (guint16 *)str;
	    
	  while (*(string_2b++))
	    length++;
	}
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
  const GdkFontPrivateX *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (const GdkFontPrivateX*) font;

  if (font->type == GDK_FONT_FONT)
    {
      return ((XFontStruct *) font_private->xfont)->fid;
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
  const GdkFontPrivateX *privatea;
  const GdkFontPrivateX *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (const GdkFontPrivateX*) fonta;
  privateb = (const GdkFontPrivateX*) fontb;

  if (fonta->type == GDK_FONT_FONT && fontb->type == GDK_FONT_FONT)
    {
      return (((XFontStruct *) privatea->xfont)->fid ==
	      ((XFontStruct *) privateb->xfont)->fid);
    }
  else if (fonta->type == GDK_FONT_FONTSET && fontb->type == GDK_FONT_FONTSET)
    {
      gchar *namea, *nameb;

      namea = XBaseFontNameListOfFontSet((XFontSet) privatea->xfont);
      nameb = XBaseFontNameListOfFontSet((XFontSet) privateb->xfont);
      
      return (strcmp(namea, nameb) == 0);
    }
  else
    /* fontset != font */
    return 0;
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  GdkFontPrivateX *private;
  gint width;
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  width = XTextWidth (xfont, text, text_length);
	}
      else
	{
	  width = XTextWidth16 (xfont, (XChar2b *) text, text_length / 2);
	}
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;
      width = XmbTextEscapement (fontset, text, text_length);
      break;
    default:
      width = 0;
    }
  return width;
}

gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
  GdkFontPrivateX *private;
  gint width;
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
        {
          gchar *text_8bit;
          gint i;
          text_8bit = g_new (gchar, text_length);
          for (i=0; i<text_length; i++) text_8bit[i] = text[i];
          width = XTextWidth (xfont, text_8bit, text_length);
          g_free (text_8bit);
        }
      else
        {
          width = 0;
        }
      break;
    case GDK_FONT_FONTSET:
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  fontset = (XFontSet) private->xfont;
	  width = XwcTextEscapement (fontset, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  fontset = (XFontSet) private->xfont;
	  text_wchar = g_new(wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  width = XwcTextEscapement (fontset, text_wchar, text_length);
	  g_free (text_wchar);
	}
      break;
    default:
      width = 0;
    }
  return width;
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
  GdkFontPrivateX *private;
  XCharStruct overall;
  XFontStruct *xfont;
  XFontSet    fontset;
  XRectangle  ink, logical;
  int direction;
  int font_ascent;
  int font_descent;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XTextExtents (xfont, text, text_length,
			&direction, &font_ascent, &font_descent,
			&overall);
	}
      else
	{
	  XTextExtents16 (xfont, (XChar2b *) text, text_length / 2,
			  &direction, &font_ascent, &font_descent,
			  &overall);
	}
      if (lbearing)
	*lbearing = overall.lbearing;
      if (rbearing)
	*rbearing = overall.rbearing;
      if (width)
	*width = overall.width;
      if (ascent)
	*ascent = overall.ascent;
      if (descent)
	*descent = overall.descent;
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;
      XmbTextExtents (fontset, text, text_length, &ink, &logical);
      if (lbearing)
	*lbearing = ink.x;
      if (rbearing)
	*rbearing = ink.x + ink.width;
      if (width)
	*width = logical.width;
      if (ascent)
	*ascent = -ink.y;
      if (descent)
	*descent = ink.y + ink.height;
      break;
    }

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
  GdkFontPrivateX *private;
  XCharStruct overall;
  XFontStruct *xfont;
  XFontSet    fontset;
  XRectangle  ink, logical;
  int direction;
  int font_ascent;
  int font_descent;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateX*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      {
	gchar *text_8bit;
	gint i;

	xfont = (XFontStruct *) private->xfont;
	g_return_if_fail ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0));

	text_8bit = g_new (gchar, text_length);
	for (i=0; i<text_length; i++) 
	  text_8bit[i] = text[i];

	XTextExtents (xfont, text_8bit, text_length,
		      &direction, &font_ascent, &font_descent,
		      &overall);
	g_free (text_8bit);
	
	if (lbearing)
	  *lbearing = overall.lbearing;
	if (rbearing)
	  *rbearing = overall.rbearing;
	if (width)
	  *width = overall.width;
	if (ascent)
	  *ascent = overall.ascent;
	if (descent)
	  *descent = overall.descent;
	break;
      }
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;

      if (sizeof(GdkWChar) == sizeof(wchar_t))
	XwcTextExtents (fontset, (wchar_t *)text, text_length, &ink, &logical);
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  
	  text_wchar = g_new (wchar_t, text_length);
	  for (i = 0; i < text_length; i++)
	    text_wchar[i] = text[i];
	  XwcTextExtents (fontset, text_wchar, text_length, &ink, &logical);
	  g_free (text_wchar);
	}
      if (lbearing)
	*lbearing = ink.x;
      if (rbearing)
	*rbearing = ink.x + ink.width;
      if (width)
	*width = logical.width;
      if (ascent)
	*ascent = -ink.y;
      if (descent)
	*descent = ink.y + ink.height;
      break;
    }

}
