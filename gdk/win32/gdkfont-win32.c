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

#include "config.h"

#include <stdio.h>
#include <ctype.h>

#include "gdk.h"
#include "gdkprivate.h"

GdkFont*
gdk_font_load (const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivate *private;
  LOGFONT logfont;
  HGDIOBJ oldfont;
  TEXTMETRIC textmetric;
  HANDLE *f;
  DWORD fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet,
    fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily;
  const char *lpszFace;

  int numfields, n1, n2, tries;
  char foundry[32], family[100], weight[32], slant[32], set_width[32],
    spacing[32], registry[32], encoding[32];
  char pixel_size[10], point_size[10], res_x[10], res_y[10], avg_width[10];
  int c;
  char *p;
  int nHeight, nWidth, nEscapement, nOrientation, fnWeight;
  int logpixelsy;

  private = g_new (GdkFontPrivate, 1);
  font = (GdkFont*) private;

  numfields = sscanf (font_name,
		      "-%30[^-]-%100[^-]-%30[^-]-%30[^-]-%30[^-]-%n",
		      foundry,
		      family,
		      weight,
		      slant,
		      set_width,
		      &n1);
  if (numfields == 0)
    {
      /* Probably a plain Windows font name */
      nHeight = 0;
      nWidth = 0;
      nEscapement = 0;
      nOrientation = 0;
      fnWeight = FW_DONTCARE;
      fdwItalic = FALSE;
      fdwUnderline = FALSE;
      fdwStrikeOut = FALSE;
      fdwCharSet = ANSI_CHARSET;
      fdwOutputPrecision = OUT_TT_PRECIS;
      fdwClipPrecision = CLIP_DEFAULT_PRECIS;
      fdwQuality = PROOF_QUALITY;
      fdwPitchAndFamily = DEFAULT_PITCH;
      lpszFace = font_name;
    }
  else if (numfields != 5)
    {
      g_warning ("gdk_font_load: font name %s illegal", font_name);
      g_free (font);
      return NULL;
    }
  else
    {
      /* It must be a XLFD name */

      /* Check for hex escapes in the font family,
       * put in there by gtkfontsel.
       */
      p = family;
      while (*p)
	{
	  if (*p == '%' && isxdigit (p[1]) && isxdigit (p[2]))
	    {
	      sscanf (p+1, "%2x", &c);
	      *p = c;
	      strcpy (p+1, p+3);
	    }
	  p++;
	}

      /* Skip add_style which often is empty in the requested font name */
      while (font_name[n1] && font_name[n1] != '-')
	n1++;
      numfields++;

      numfields += sscanf (font_name + n1,
			   "-%8[^-]-%8[^-]-%8[^-]-%8[^-]-%30[^-]-%8[^-]-%30[^-]-%30[^-]%n",
			   pixel_size,
			   point_size,
			   res_x,
			   res_y,
			   spacing,
			   avg_width,
			   registry,
			   encoding,
			   &n2);

      if (numfields != 14 || font_name[n1 + n2] != '\0')
	{
	  g_warning ("gdk_font_load: font name %s illegal", font_name);
	  g_free (font);
	  return NULL;
	}

      logpixelsy = GetDeviceCaps (gdk_DC, LOGPIXELSY);

      if (strcmp (pixel_size, "*") == 0)
	if (strcmp (point_size, "*") == 0)
	  nHeight = 0;
	else
	  nHeight = (int) (((double) atoi (point_size))/720.*logpixelsy);
      else
	nHeight = atoi (pixel_size);

      nWidth = 0;
      nEscapement = 0;
      nOrientation = 0;

      if (g_strcasecmp (weight, "thin") == 0)
	fnWeight = FW_THIN;
      else if (g_strcasecmp (weight, "extralight") == 0)
	fnWeight = FW_EXTRALIGHT;
      else if (g_strcasecmp (weight, "ultralight") == 0)
	fnWeight = FW_ULTRALIGHT;
      else if (g_strcasecmp (weight, "light") == 0)
	fnWeight = FW_LIGHT;
      else if (g_strcasecmp (weight, "normal") == 0)
	fnWeight = FW_NORMAL;
      else if (g_strcasecmp (weight, "regular") == 0)
	fnWeight = FW_REGULAR;
      else if (g_strcasecmp (weight, "medium") == 0)
	fnWeight = FW_MEDIUM;
      else if (g_strcasecmp (weight, "semibold") == 0)
	fnWeight = FW_SEMIBOLD;
      else if (g_strcasecmp (weight, "demibold") == 0)
	fnWeight = FW_DEMIBOLD;
      else if (g_strcasecmp (weight, "bold") == 0)
	fnWeight = FW_BOLD;
      else if (g_strcasecmp (weight, "extrabold") == 0)
	fnWeight = FW_EXTRABOLD;
      else if (g_strcasecmp (weight, "ultrabold") == 0)
	fnWeight = FW_ULTRABOLD;
      else if (g_strcasecmp (weight, "heavy") == 0)
	fnWeight = FW_HEAVY;
      else if (g_strcasecmp (weight, "black") == 0)
	fnWeight = FW_BLACK;
      else
	fnWeight = FW_DONTCARE;

      if (g_strcasecmp (slant, "italic") == 0
	  || g_strcasecmp (slant, "oblique") == 0
	  || g_strcasecmp (slant, "i") == 0
	  || g_strcasecmp (slant, "o") == 0)
	fdwItalic = TRUE;
      else
	fdwItalic = FALSE;
      fdwUnderline = FALSE;
      fdwStrikeOut = FALSE;
      if (g_strcasecmp (registry, "iso8859") == 0)
	if (strcmp (encoding, "1") == 0)
	  fdwCharSet = ANSI_CHARSET;
	else
	  fdwCharSet = ANSI_CHARSET; /* XXX ??? */
      else if (g_strcasecmp (registry, "windows") == 0)
	if (g_strcasecmp (encoding, "symbol") == 0)
	  fdwCharSet = SYMBOL_CHARSET;
	else if (g_strcasecmp (encoding, "shiftjis") == 0)
	  fdwCharSet = SHIFTJIS_CHARSET;
	else if (g_strcasecmp (encoding, "gb2312") == 0)
	  fdwCharSet = GB2312_CHARSET;
	else if (g_strcasecmp (encoding, "hangeul") == 0)
	  fdwCharSet = HANGEUL_CHARSET;
	else if (g_strcasecmp (encoding, "chinesebig5") == 0)
	  fdwCharSet = CHINESEBIG5_CHARSET;
	else if (g_strcasecmp (encoding, "johab") == 0)
	  fdwCharSet = JOHAB_CHARSET;
	else if (g_strcasecmp (encoding, "hebrew") == 0)
	  fdwCharSet = HEBREW_CHARSET;
	else if (g_strcasecmp (encoding, "arabic") == 0)
	  fdwCharSet = ARABIC_CHARSET;
	else if (g_strcasecmp (encoding, "greek") == 0)
	  fdwCharSet = GREEK_CHARSET;
	else if (g_strcasecmp (encoding, "turkish") == 0)
	  fdwCharSet = TURKISH_CHARSET;
	else if (g_strcasecmp (encoding, "easteurope") == 0)
	  fdwCharSet = EASTEUROPE_CHARSET;
	else if (g_strcasecmp (encoding, "russian") == 0)
	  fdwCharSet = RUSSIAN_CHARSET;
	else if (g_strcasecmp (encoding, "mac") == 0)
	  fdwCharSet = MAC_CHARSET;
	else if (g_strcasecmp (encoding, "baltic") == 0)
	  fdwCharSet = BALTIC_CHARSET;
	else
	  fdwCharSet = ANSI_CHARSET; /* XXX ??? */
      else
	fdwCharSet = ANSI_CHARSET; /* XXX ??? */
      fdwOutputPrecision = OUT_TT_PRECIS;
      fdwClipPrecision = CLIP_DEFAULT_PRECIS;
      fdwQuality = PROOF_QUALITY;
      if (g_strcasecmp (spacing, "m") == 0)
	fdwPitchAndFamily = FIXED_PITCH;
      else if (g_strcasecmp (spacing, "p") == 0)
	fdwPitchAndFamily = VARIABLE_PITCH;
      else 
	fdwPitchAndFamily = DEFAULT_PITCH;
      lpszFace = family;
    }

  for (tries = 0; ; tries++)
    {
      GDK_NOTE (MISC, g_print ("gdk_font_load: trying CreateFont(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%#.02x,\"%s\")\n",
			       nHeight, nWidth, nEscapement, nOrientation,
			       fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
			       fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
			       fdwQuality, fdwPitchAndFamily, lpszFace));
      if ((private->xfont =
	   CreateFont (nHeight, nWidth, nEscapement, nOrientation,
		       fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
		       fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
		       fdwQuality, fdwPitchAndFamily, lpszFace)) != NULL)
	break;

      /* If we fail, try some similar fonts often found on Windows. */

      if (tries == 0)
	{
	  if (g_strcasecmp (family, "helvetica") == 0)
	    lpszFace = "arial";
	  else if (g_strcasecmp (family, "new century schoolbook") == 0)
	    lpszFace = "century schoolbook";
	  else if (g_strcasecmp (family, "courier") == 0)
	    lpszFace = "courier new";
	  else if (g_strcasecmp (family, "lucida") == 0)
	    lpszFace = "lucida sans unicode";
	  else if (g_strcasecmp (family, "lucidatypewriter") == 0)
	    lpszFace = "lucida console";
	  else if (g_strcasecmp (family, "times") == 0)
	    lpszFace = "times new roman";
	}
      else if (tries == 1)
	{
	  if (g_strcasecmp (family, "courier") == 0)
	    {
	      lpszFace = "";
	      fdwPitchAndFamily |= FF_MODERN;
	    }
	  else if (g_strcasecmp (family, "times new roman") == 0)
	    {
	      lpszFace = "";
	      fdwPitchAndFamily |= FF_ROMAN;
	    }
	  else if (g_strcasecmp (family, "helvetica") == 0
		   || g_strcasecmp (family, "lucida") == 0)
	    {
	      lpszFace = "";
	      fdwPitchAndFamily |= FF_SWISS;
	    }
	  else
	    {
	      lpszFace = "";
	      fdwPitchAndFamily = (fdwPitchAndFamily & 0x0F) | FF_DONTCARE;
	    }
	}
      else
	break;
      tries++;
    }
  
  if (!private->xfont)
    {
      g_warning ("gdk_font_load: font %s not found", font_name);
      g_free (font);
      return NULL;
    }
      
  private->ref_count = 1;
  font->type = GDK_FONT_FONT;
  GetObject (private->xfont, sizeof (logfont), &logfont);
  oldfont = SelectObject (gdk_DC, private->xfont);
  GetTextMetrics (gdk_DC, &textmetric);
  SelectObject (gdk_DC, oldfont);
  font->ascent = textmetric.tmAscent;
  font->descent = textmetric.tmDescent;

  GDK_NOTE (MISC, g_print ("gdk_font_load: %s = %#x asc %d desc %d\n",
			   font_name, private->xfont,
			   font->ascent, font->descent));

  /* This memory is leaked, so shoot me. */
  f = g_new (HANDLE, 1);
  *f = (HANDLE) ((guint) private->xfont + HFONT_DITHER);
  gdk_xid_table_insert (f, font);

  return font;
}

GdkFont*
gdk_fontset_load (gchar *fontset_name)
{
  g_warning ("gdk_fontset_load: Not implemented");
  return NULL;
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
	  GDK_NOTE (MISC, g_print ("gdk_font_unref %#x\n", private->xfont));

	  gdk_xid_table_remove ((HANDLE) ((guint) private->xfont + HFONT_DITHER));
	  DeleteObject (private->xfont);
	  break;

	default:
	  g_assert_not_reached ();
	}
      g_free (font);
    }
}

gint
gdk_font_id (const GdkFont *font)
{
  const GdkFontPrivate *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (const GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
    return (gint) font_private->xfont;

  g_assert_not_reached ();
  return 0;
}

gint
gdk_font_equal (const GdkFont *fonta,
                const GdkFont *fontb)
{
  const GdkFontPrivate *privatea;
  const GdkFontPrivate *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (const GdkFontPrivate*) fonta;
  privateb = (const GdkFontPrivate*) fontb;

  if (fonta->type == GDK_FONT_FONT && fontb->type == GDK_FONT_FONT)
    return (privatea->xfont == privateb->xfont);

  g_assert_not_reached ();
  return 0;
}

gint
gdk_string_width (GdkFont     *font,
		  const gchar *string)
{
  return gdk_text_width (font, string, strlen (string));
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  gint width;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      oldfont = SelectObject (gdk_DC, private->xfont);
      GetTextExtentPoint32A (gdk_DC, text, text_length, &size);
      SelectObject (gdk_DC, oldfont);
      width = size.cx;
      break;

    default:
      g_assert_not_reached ();
    }
  return width;
}

gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  wchar_t *wcstr;
  gint i, width;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
      oldfont = SelectObject (gdk_DC, private->xfont);
      GetTextExtentPoint32W (gdk_DC, wcstr, text_length, &size);
      g_free (wcstr);
      SelectObject (gdk_DC, oldfont);
      width = size.cx;
      break;

    default:
      width = 0;
    }
  return width;
}

gint
gdk_char_width (GdkFont *font,
		gchar    character)
{
  return gdk_text_width (font, &character, 1);
}

gint
gdk_char_width_wc (GdkFont *font,
		   GdkWChar character)
{
  return gdk_text_width_wc (font, &character, 1);
}

gint
gdk_string_measure (GdkFont     *font,
                    const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_measure (font, string, strlen (string));
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
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      oldfont = SelectObject (gdk_DC, private->xfont);
      GetTextExtentPoint32A (gdk_DC, text, text_length, &size);
      SelectObject (gdk_DC, oldfont);
      /* XXX This is all quite bogus */
      if (lbearing)
	*lbearing = 0;
      if (rbearing)
	*rbearing = 0;
      if (width)
	*width = size.cx;
      if (ascent)
	*ascent = size.cy + 1;
      if (descent)
	*descent = font->descent + 1;
      break;

    default:
      g_assert_not_reached ();
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
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  wchar_t *wcstr;
  gint i;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
      oldfont = SelectObject (gdk_DC, private->xfont);
      GetTextExtentPoint32W (gdk_DC, wcstr, text_length, &size);
      g_free (wcstr);
      SelectObject (gdk_DC, oldfont);

      /* XXX This is all quite bogus */
      if (lbearing)
	*lbearing = 0;
      if (rbearing)
	*rbearing = 0;
      if (width)
	*width = size.cx;
      if (ascent)
	*ascent = size.cy + 1;
      if (descent)
	*descent = font->descent + 1;
      break;

    default:
      g_assert_not_reached ();
    }
}

void
gdk_string_extents (GdkFont     *font,
		    const gchar *string,
		    gint        *lbearing,
		    gint        *rbearing,
		    gint        *width,
		    gint        *ascent,
		    gint        *descent)
{
  g_return_if_fail (font != NULL);
  g_return_if_fail (string != NULL);

  gdk_text_extents (font, string, strlen (string),
		    lbearing, rbearing, width, ascent, descent);
}


gint
gdk_text_measure (GdkFont     *font,
                  const gchar *text,
                  gint         text_length)
{
  GdkFontPrivate *private;
  gint width;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      return gdk_text_width (font, text, text_length); /* ??? */
      break;

    default:
      g_assert_not_reached ();
    }
  return 0;
}

gint
gdk_char_measure (GdkFont *font,
                  gchar    character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_measure (font, &character, 1);
}

gint
gdk_string_height (GdkFont     *font,
		   const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_height (font, string, strlen (string));
}

gint
gdk_text_height (GdkFont     *font,
		 const gchar *text,
		 gint         text_length)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  gint height;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  private = (GdkFontPrivate*) font;

  switch (font->type)
    {
    case GDK_FONT_FONT:
      oldfont = SelectObject (gdk_DC, private->xfont);
      GetTextExtentPoint32 (gdk_DC, text, text_length, &size);
      SelectObject (gdk_DC, oldfont);
      height = size.cy;
      break;

    default:
      g_error ("font->type = %d", font->type);
    }
  return height;
}

gint
gdk_char_height (GdkFont *font,
		 gchar    character)
{
  g_return_val_if_fail (font != NULL, -1);

  return gdk_text_height (font, &character, 1);
}
