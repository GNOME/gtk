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
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include "gdk.h"
#include "gdkprivate.h"

GdkFont*
gdk_font_load (const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivate *private;

  private = g_new (GdkFontPrivate, 1);
  font = (GdkFont*) private;

  private->xdisplay = gdk_display;
  private->xfont = XLoadQueryFont (private->xdisplay, font_name);
  private->ref_count = 1;

  if (!private->xfont)
    {
      g_free (font);
      return NULL;
    }
  else
    {
      font->type = GDK_FONT_FONT;
      font->ascent =  ((XFontStruct *) private->xfont)->ascent;
      font->descent = ((XFontStruct *) private->xfont)->descent;
    }

  gdk_xid_table_insert (&((XFontStruct *) private->xfont)->fid, font);

  return font;
}

GdkFont*
gdk_fontset_load (gchar *fontset_name)
{
  GdkFont *font;
  GdkFontPrivate *private;
  XFontSet fontset;
  gint  missing_charset_count;
  gchar **missing_charset_list;
  gchar *def_string;

  private = g_new (GdkFontPrivate, 1);
  font = (GdkFont*) private;

  private->xdisplay = gdk_display;
  fontset = XCreateFontSet (gdk_display, fontset_name,
			    &missing_charset_list, &missing_charset_count,
			    &def_string);

  if (missing_charset_count)
    {
      gint i;
      g_print ("Missing charsets in FontSet creation\n");
      for (i=0;i<missing_charset_count;i++)
	g_print ("    %s\n", missing_charset_list[i]);
      XFreeStringList (missing_charset_list);
    }

  private->ref_count = 1;

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
    }
  return font;
}

GdkFont*
gdk_font_ref (GdkFont *font)
{
  GdkFontPrivate *private;

  g_return_val_if_fail (font != NULL, NULL);

  private = (GdkFontPrivate*) font;
  private->ref_count += 1;
  return font;
}

void
gdk_font_unref (GdkFont *font)
{
  GdkFontPrivate *private;

  g_return_if_fail (font != NULL);

  private = (GdkFontPrivate*) font;

  private->ref_count -= 1;
  if (private->ref_count == 0)
    {
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
}

gint
gdk_font_id (GdkFont *font)
{
  GdkFontPrivate *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (GdkFontPrivate*) font;

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
gdk_font_equal (GdkFont *fonta,
                GdkFont *fontb)
{
  GdkFontPrivate *privatea;
  GdkFontPrivate *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (GdkFontPrivate*) fonta;
  privateb = (GdkFontPrivate*) fontb;

  if (fonta->type == GDK_FONT_FONT && fontb->type == GDK_FONT_FONT)
    {
      return (((XFontStruct *) privatea->xfont)->fid ==
	      ((XFontStruct *) privateb->xfont)->fid);
    }
  else if (fonta->type == GDK_FONT_FONTSET && fontb->type == GDK_FONT_FONTSET)
    {
      /* how to compare two fontsets ?? by basename or XFontSet ?? */
      return (((XFontSet) privatea->xfont) == ((XFontSet) privateb->xfont));
    }
  else
    /* fontset != font */
    return 0;
}

gint
gdk_string_width (GdkFont     *font,
		  const gchar *string)
{
  GdkFontPrivate *font_private;
  gint width;
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  font_private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      xfont = (XFontStruct *) font_private->xfont;
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  width = XTextWidth (xfont, string, strlen (string));
	}
      else
	{
	  width = XTextWidth16 (xfont, (XChar2b *) string, strlen (string) / 2);
	}
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) font_private->xfont;
      width = XmbTextEscapement (fontset, string, strlen(string));
      break;
    default:
      width = 0;
    }

  return width;
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  GdkFontPrivate *private;
  gint width;
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivate*) font;

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

/* Problem: What if a character is a 16 bits character ?? */
gint
gdk_char_width (GdkFont *font,
		gchar    character)
{
  GdkFontPrivate *private;
  XCharStruct *chars;
  gint width;
  guint ch = character & 0xff;  /* get rid of sign-extension */
  XFontStruct *xfont;
  XFontSet fontset;

  g_return_val_if_fail (font != NULL, -1);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      /* only 8 bits characters are considered here */
      xfont = (XFontStruct *) private->xfont;
      if ((xfont->min_byte1 == 0) &&
	  (xfont->max_byte1 == 0) &&
	  (ch >= xfont->min_char_or_byte2) &&
	  (ch <= xfont->max_char_or_byte2))
	{
	  chars = xfont->per_char;
	  if (chars)
	    width = chars[ch - xfont->min_char_or_byte2].width;
	  else
	    width = xfont->min_bounds.width;
	}
      else
	{
	  width = XTextWidth (xfont, &character, 1);
	}
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;
      width = XmbTextEscapement (fontset, &character, 1) ;
      break;
    default:
      width = 0;
    }
  return width;
}

gint
gdk_string_measure (GdkFont     *font,
                    const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_measure (font, string, strlen (string));
}

gint
gdk_text_measure (GdkFont     *font,
                  const gchar *text,
                  gint         text_length)
{
  GdkFontPrivate *private;
  XCharStruct overall;
  XFontStruct *xfont;
  XFontSet    fontset;
  XRectangle  ink, log;
  int direction;
  int font_ascent;
  int font_descent;
  gint width;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivate*) font;

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
      width = overall.rbearing;
      break;
    case GDK_FONT_FONTSET:
      fontset = (XFontSet) private->xfont;
      XmbTextExtents (fontset, text, text_length, &ink, &log);
      width = log.width;
      break;
    default:
      width = 0;
    }
  return width;
}

gint
gdk_char_measure (GdkFont *font,
                  gchar    character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_measure (font, &character, 1);
}
