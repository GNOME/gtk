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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <locale.h>
#include <stdlib.h>

#include "gdk.h"		/* For gdk_flush() */
#include "gdkpixmap.h"
#include "gdkinternals.h"
#include "gdkprivate-x11.h"

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#  endif
#endif


/* If this variable is FALSE, it indicates that we should
 * avoid trying to use multibyte conversion functions and
 * assume everything is 1-byte per character
 */
static gboolean gdk_use_mb;

void
_gdk_x11_initialize_locale (void)
{
  wchar_t result;
  gchar *current_locale;
  static char *last_locale = NULL;

  gdk_use_mb = FALSE;

  current_locale = setlocale (LC_ALL, NULL);

  if (last_locale && strcmp (last_locale, current_locale) == 0)
    return;

  g_free (last_locale);
  last_locale = g_strdup (current_locale);

  if (!XSupportsLocale ())
    g_warning ("locale not supported by Xlib");
  
  if (!XSetLocaleModifiers (""))
    g_warning ("can not set locale modifiers");

  if ((strcmp (current_locale, "C")) && (strcmp (current_locale, "POSIX")))
    {
      gdk_use_mb = TRUE;

#ifndef X_LOCALE
      /* Detect ancient GNU libc, where mb == UTF8. Not useful unless it's
       * really a UTF8 locale. The below still probably will
       * screw up on Greek, Cyrillic, etc, encoded as UTF8.
       */
      
      if ((MB_CUR_MAX == 2) &&
	  (mbstowcs (&result, "\xdd\xa5", 1) > 0) &&
	  result == 0x765)
	{
	  if ((strlen (current_locale) < 4) ||
	      g_strcasecmp (current_locale + strlen(current_locale) - 4, "utf8"))
	    gdk_use_mb = FALSE;
	}
#endif /* X_LOCALE */
    }

  GDK_NOTE (XIM,
	    g_message ("%s multi-byte string functions.", 
		       gdk_use_mb ? "Using" : "Not using"));
  
  return;
}

gchar*
gdk_set_locale (void)
{
  if (!setlocale (LC_ALL,""))
    g_warning ("locale not supported by C library");

  _gdk_x11_initialize_locale ();
  
  return setlocale (LC_ALL, NULL);
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
      XTextProperty tpr;

      if (sizeof(wchar_t) != sizeof(GdkWChar))
	{
	  gint i;
	  wchar_t *src_alt;
	  for (i=0; src[i]; i++);
	  src_alt = g_new (wchar_t, i+1);
	  for (; i>=0; i--)
	    src_alt[i] = src[i];
	  if (XwcTextListToTextProperty (gdk_display, &src_alt, 1, XTextStyle, &tpr)
	      != Success)
	    {
	      g_free (src_alt);
	      return NULL;
	    }
	  g_free (src_alt);
	}
      else
	{
	  if (XwcTextListToTextProperty (gdk_display, (wchar_t**)&src, 1,
					 XTextStyle, &tpr) != Success)
	    {
	      return NULL;
	    }
	}
      /*
       * We must copy the string into an area allocated by glib, because
       * the string 'tpr.value' must be freed by XFree().
       */
      mbstr = g_strdup(tpr.value);
      XFree (tpr.value);
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
      XTextProperty tpr;
      wchar_t **wstrs, *wstr_src;
      gint num_wstrs;
      gint len_cpy;
      if (XmbTextListToTextProperty (gdk_display, (char **)&src, 1, XTextStyle,
				     &tpr)
	  != Success)
	{
	  /* NoMem or LocaleNotSupp */
	  return -1;
	}
      if (XwcTextPropertyToTextList (gdk_display, &tpr, &wstrs, &num_wstrs)
	  != Success)
	{
	  /* InvalidChar */
	  XFree(tpr.value);
	  return -1;
	}
      XFree(tpr.value);
      if (num_wstrs == 0)
	return 0;
      wstr_src = wstrs[0];
      for (len_cpy=0; len_cpy<dest_max && wstr_src[len_cpy]; len_cpy++)
	dest[len_cpy] = wstr_src[len_cpy];
      XwcFreeStringList (wstrs);
      return len_cpy;
    }
  else
    {
      gint i;

      for (i=0; i<dest_max && src[i]; i++)
	dest[i] = src[i];

      return i;
    }
}
