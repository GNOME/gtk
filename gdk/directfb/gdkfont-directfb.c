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
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#undef GDK_DISABLE_DEPRECATED

#include "config.h"
#include "gdk.h"

#include <string.h>

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"

#include "gdkfont.h"
#include "gdkalias.h"


typedef struct _GdkFontDirectFB  GdkFontDirectFB;

struct _GdkFontDirectFB
{
  GdkFontPrivate    base;
  gint              size;
  IDirectFBFont    *dfbfont;
};


static GdkFont *
gdk_directfb_bogus_font (gint height)
{
  GdkFont         *font;
  GdkFontDirectFB *private;

  private = g_new0 (GdkFontDirectFB, 1);
  font = (GdkFont *) private;

  font->type              = GDK_FONT_FONT;
  font->ascent            = height * 3 / 4;
  font->descent           = height / 4;
  private->size           = height;
  private->base.ref_count = 1;
  return font;
}

GdkFont *
gdk_font_from_description_for_display (GdkDisplay *display,
                                       PangoFontDescription *font_desc)
{
  gint size;

  g_return_val_if_fail (font_desc, NULL);

  size = pango_font_description_get_size (font_desc);

  return gdk_directfb_bogus_font (PANGO_PIXELS (size));
}

/* ********************* */

GdkFont *
gdk_fontset_load (const gchar *fontset_name)
{
  return gdk_directfb_bogus_font (10);
}

GdkFont *
gdk_fontset_load_for_display (GdkDisplay *display,const gchar *font_name)
{
  return gdk_directfb_bogus_font (10);
}

GdkFont *
gdk_font_load_for_display (GdkDisplay *display, const gchar *font_name)
{
  return gdk_directfb_bogus_font (10);
}

void
_gdk_font_destroy (GdkFont *font)
{
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
  GdkFontDirectFB *font_private;
  gint             length = 0;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (str != NULL, -1);

  font_private = (GdkFontDirectFB*) font;

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
  const GdkFontDirectFB *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (const GdkFontDirectFB*) font;

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
  const GdkFontDirectFB *privatea;
  const GdkFontDirectFB *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (const GdkFontDirectFB*) fonta;
  privateb = (const GdkFontDirectFB*) fontb;

  if (fonta == fontb)
    return TRUE;

  return FALSE;
}

gint
gdk_text_width (GdkFont      *font,
                const gchar  *text,
                gint          text_length)
{
  GdkFontDirectFB *private;

  private = (GdkFontDirectFB*) font;

  return (text_length * private->size) / 2;
}

gint
gdk_text_width_wc (GdkFont        *font,
                   const GdkWChar *text,
                   gint            text_length)
{
  return 0;
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
  if (ascent)
    *ascent = font->ascent;
  if (descent)
    *descent = font->descent;
  if (width)
    *width = gdk_text_width (font, text, text_length);
  if (lbearing)
    *lbearing = 0;
  if (rbearing)
    *rbearing = 0;
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

  realstr = alloca (text_length + 1);

  for(i = 0; i < text_length; i++)
    realstr[i] = text[i];

  realstr[i] = '\0';

  return gdk_text_extents (font,
                           realstr,
                           text_length,
                           lbearing,
                           rbearing,
                           width,
                           ascent,
                           descent);
}

GdkFont *
gdk_font_lookup (GdkNativeWindow xid)
{
  g_warning(" gdk_font_lookup unimplemented \n");
  return NULL;
}

GdkDisplay *
gdk_font_get_display (GdkFont* font)
{
  g_warning(" gdk_font_get_display unimplemented \n");
  return NULL;
}

#define __GDK_FONT_X11_C__
#include "gdkaliasdef.c"
