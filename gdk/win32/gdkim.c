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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "gdkim.h"
#include "gdkpixmap.h"
#include "gdkprivate.h"
#include "gdki18n.h"
#include "gdkx.h"

/* If this variable is FALSE, it indicates that we should
 * avoid trying to use multibyte conversion functions and
 * assume everything is 1-byte per character
 */
static gboolean gdk_use_mb;

/*
 *--------------------------------------------------------------
 * gdk_set_locale
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gchar*
gdk_set_locale (void)
{
  wchar_t result;
  gchar *current_locale;

  gdk_use_mb = FALSE;

  if (!setlocale (LC_ALL,""))
    g_warning ("locale not supported by C library");
  
  current_locale = setlocale (LC_ALL, NULL);

  if (MB_CUR_MAX > 1)
    gdk_use_mb = TRUE;

  GDK_NOTE (XIM, g_message ("%s multi-byte string functions.", 
			    gdk_use_mb ? "Using" : "Not using"));
  
  return current_locale;
}

void 
gdk_im_begin (GdkIC *ic, GdkWindow* window)
{
}

void 
gdk_im_end (void)
{
}

GdkIMStyle
gdk_im_decide_style (GdkIMStyle supported_style)
{
  return GDK_IM_PREEDIT_NONE | GDK_IM_STATUS_NONE;
}

GdkIMStyle
gdk_im_set_best_style (GdkIMStyle style)
{
  return GDK_IM_PREEDIT_NONE | GDK_IM_STATUS_NONE;
}

gint 
gdk_im_ready (void)
{
  return FALSE;
}

GdkIC * 
gdk_ic_new (GdkICAttr *attr, GdkICAttributesType mask)
{
  return NULL;
}

void 
gdk_ic_destroy (GdkIC *ic)
{
}

GdkIMStyle
gdk_ic_get_style (GdkIC *ic)
{
  return GDK_IM_PREEDIT_NONE | GDK_IM_STATUS_NONE;
}

void 
gdk_ic_set_values (GdkIC *ic, ...)
{
}

void 
gdk_ic_get_values (GdkIC *ic, ...)
{
}

GdkICAttributesType 
gdk_ic_set_attr (GdkIC *ic, GdkICAttr *attr, GdkICAttributesType mask)
{
  return 0;
}

GdkICAttributesType 
gdk_ic_get_attr (GdkIC *ic, GdkICAttr *attr, GdkICAttributesType mask)
{
  return 0;
}

GdkEventMask 
gdk_ic_get_events (GdkIC *ic)
{
  return 0;
}

/*
 * gdk_wcstombs 
 *
 * Returns a multi-byte string converted from the specified array
 * of wide characters. The string is newly allocated. The array of
 * wide characters must be null-terminated. If the conversion is
 * failed, it returns NULL.
 */
gchar *
gdk_wcstombs (const GdkWChar *src)
{
  gchar *mbstr;

  if (gdk_use_mb)
    {
      gint i, wcsl, mbsl;
      wchar_t *src_alt;

      for (wcsl = 0; src[wcsl]; wcsl++)
	;
      src_alt = g_new (wchar_t, wcsl+1);
      for (i = wcsl; i >= 0; i--)
	src_alt[i] = src[i];
      mbsl = WideCharToMultiByte (CP_OEMCP, 0, src_alt, wcsl,
				  NULL, 0, NULL, NULL);
      mbstr = g_new (guchar, mbsl + 1);
      if (!WideCharToMultiByte (CP_OEMCP, 0, src_alt, wcsl,
				mbstr, mbsl, NULL, NULL))
	{
	  g_warning ("gdk_wcstombs: WideCharToMultiByte failed");
	  g_free (mbstr);
	  g_free (src_alt);
	  return NULL;
	}
      mbstr[mbsl] = '\0';
      g_free (src_alt);
    }
  else
    {
      gint length = 0;
      gint i;

      while (src[length] != 0)
	length++;
      
      mbstr = g_new (gchar, length + 1);

      for (i=0; i<length+1; i++)
	mbstr[i] = src[i];
    }

  return mbstr;
}

  
/*
 * gdk_mbstowcs
 *
 * Converts the specified string into wide characters, and, returns the
 * number of wide characters written. The string 'src' must be
 * null-terminated. If the conversion is failed, it returns -1.
 */
gint
gdk_mbstowcs (GdkWChar *dest, const gchar *src, gint dest_max)
{
  if (gdk_use_mb)
    {
      gint i, wcsl;
      wchar_t *wcstr;

      wcsl = MultiByteToWideChar (CP_OEMCP, 0, src, -1, NULL, 0);
      wcstr = g_new (wchar_t, wcsl);
      if (!MultiByteToWideChar (CP_OEMCP, 0, src, -1, wcstr, wcsl))
	{
	  g_warning ("gdk_mbstowcs: MultiByteToWideChar failed");
	  g_free (wcstr);
	  return -1;
	}
      if (wcsl > dest_max)
	wcsl = dest_max;
      for (i = 0; i < wcsl && wcstr[i]; i++)
	dest[i] = wcstr[i];
      g_free (wcstr);

      return i;
    }
  else
    {
      gint i;

      for (i=0; i<dest_max && src[i]; i++)
	dest[i] = src[i];

      return i;
    }
}
