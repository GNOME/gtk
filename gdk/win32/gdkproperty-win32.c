/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate-win32.h"

GdkAtom
gdk_atom_intern (const gchar *atom_name,
		 gint         only_if_exists)
{
  ATOM win32_atom;
  GdkAtom retval;
  static GHashTable *atom_hash = NULL;
  
  if (!atom_hash)
    atom_hash = g_hash_table_new (g_str_hash, g_str_equal);

  retval = g_hash_table_lookup (atom_hash, atom_name);
  if (!retval)
    {
      if (strcmp (atom_name, "PRIMARY") == 0)
	retval = GDK_SELECTION_PRIMARY;
      else if (strcmp (atom_name, "SECONDARY") == 0)
	retval = GDK_SELECTION_SECONDARY;
      else if (strcmp (atom_name, "CLIPBOARD") == 0)
	retval = GDK_SELECTION_CLIPBOARD;
      else if (strcmp (atom_name, "ATOM") == 0)
	retval = GDK_SELECTION_TYPE_ATOM;
      else if (strcmp (atom_name, "BITMAP") == 0)
	retval = GDK_SELECTION_TYPE_BITMAP;
      else if (strcmp (atom_name, "COLORMAP") == 0)
	retval = GDK_SELECTION_TYPE_COLORMAP;
      else if (strcmp (atom_name, "DRAWABLE") == 0)
	retval = GDK_SELECTION_TYPE_DRAWABLE;
      else if (strcmp (atom_name, "INTEGER") == 0)
	retval = GDK_SELECTION_TYPE_INTEGER;
      else if (strcmp (atom_name, "PIXMAP") == 0)
	retval = GDK_SELECTION_TYPE_PIXMAP;
      else if (strcmp (atom_name, "WINDOW") == 0)
	retval = GDK_SELECTION_TYPE_WINDOW;
      else if (strcmp (atom_name, "STRING") == 0)
	retval = GDK_SELECTION_TYPE_STRING;
      else
	{
	  win32_atom = GlobalAddAtom (atom_name);
	  retval = GUINT_TO_POINTER ((guint) win32_atom);
	}
      g_hash_table_insert (atom_hash, 
			   g_strdup (atom_name), 
			   retval);
    }

  return retval;
}

gchar *
gdk_atom_name (GdkAtom atom)
{
  ATOM win32_atom;
  gchar name[256];

  if (GDK_SELECTION_PRIMARY == atom) return g_strdup ("PRIMARY");
  else if (GDK_SELECTION_SECONDARY == atom) return g_strdup ("SECONDARY");
  else if (GDK_SELECTION_CLIPBOARD == atom) return g_strdup ("CLIPBOARD");
  else if (GDK_SELECTION_TYPE_ATOM == atom) return g_strdup ("ATOM");
  else if (GDK_SELECTION_TYPE_BITMAP == atom) return g_strdup ("BITMAP");
  else if (GDK_SELECTION_TYPE_COLORMAP == atom) return g_strdup ("COLORMAP");
  else if (GDK_SELECTION_TYPE_DRAWABLE == atom) return g_strdup ("DRAWABLE");
  else if (GDK_SELECTION_TYPE_INTEGER == atom) return g_strdup ("INTEGER");
  else if (GDK_SELECTION_TYPE_PIXMAP == atom) return g_strdup ("PIXMAP");
  else if (GDK_SELECTION_TYPE_WINDOW == atom) return g_strdup ("WINDOW");
  else if (GDK_SELECTION_TYPE_STRING == atom) return g_strdup ("STRING");
  
  win32_atom = GPOINTER_TO_UINT (atom);
  
  if (win32_atom < 0xC000)
    return g_strdup_printf ("#%p", atom);
  else if (GlobalGetAtomName (win32_atom, name, sizeof (name)) == 0)
    return NULL;
  return g_strdup (name);
}

gint
gdk_property_get (GdkWindow   *window,
		  GdkAtom      property,
		  GdkAtom      type,
		  gulong       offset,
		  gulong       length,
		  gint         pdelete,
		  GdkAtom     *actual_property_type,
		  gint        *actual_format_type,
		  gint        *actual_length,
		  guchar     **data)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  g_warning ("gdk_property_get: Not implemented");

  return FALSE;
}

static gboolean
find_common_locale (const guchar  *data,
		    gint           nelements,
		    gint           nchars,
		    LCID          *lcidp,
		    guchar       **bufp,
		    gint          *sizep)
{
  static struct {
    LCID lcid;
    UINT cp;
  } locales[] = {
#define ENTRY(lang, sublang) \
 { MAKELCID (MAKELANGID (LANG_##lang, SUBLANG_##sublang), SORT_DEFAULT), 0 }
    ENTRY (ENGLISH, DEFAULT),
    ENTRY (POLISH, DEFAULT),
    ENTRY (CZECH, DEFAULT),
    ENTRY (LITHUANIAN, DEFAULT),
    ENTRY (RUSSIAN, DEFAULT),
    ENTRY (GREEK, DEFAULT),
    ENTRY (TURKISH, DEFAULT),
    ENTRY (HEBREW, DEFAULT),
    ENTRY (ARABIC, DEFAULT),
    ENTRY (THAI, DEFAULT),
    ENTRY (JAPANESE, DEFAULT),
    ENTRY (CHINESE, CHINESE_SIMPLIFIED),
    ENTRY (CHINESE, CHINESE_TRADITIONAL),
    ENTRY (KOREAN, DEFAULT),
#undef ENTRY
  };

  static gboolean been_here = FALSE;
  gint i;
  wchar_t *wcs;

  /* For each installed locale: Get the locale's default code page,
   * and store the list of locales and code pages.
   */
  if (!been_here)
    {
      been_here = TRUE;
      for (i = 0; i < G_N_ELEMENTS (locales); i++)
	if (IsValidLocale (locales[i].lcid, LCID_INSTALLED))
	  {
	    gchar buf[10];
	    if (GetLocaleInfo (locales[i].lcid, LOCALE_IDEFAULTANSICODEPAGE,
			       buf, sizeof (buf)))
	      {
		gchar name[100];
		locales[i].cp = atoi (buf);
		GDK_NOTE (DND, (GetLocaleInfo (locales[i].lcid,
					       LOCALE_SENGLANGUAGE,
					       name, sizeof (name)),
				g_print ("locale %#lx: %s: CP%d\n",
					 (gulong) locales[i].lcid, name,
					 locales[i].cp)));
	      }
	  }
    }
  
  /* Allocate bufp big enough to store data in any code page.  Two
   * bytes for each Unicode char should be enough, Windows code pages
   * are either single- or double-byte.
   */
  *bufp = g_malloc ((nchars+1) * 2);
  wcs = g_new (wchar_t, nchars+1);

  /* Convert to Windows wide chars into temp buf */
  _gdk_utf8_to_ucs2 (wcs, data, nelements, nchars);
  wcs[nchars] = 0;

  /* For each code page that is the default for an installed locale: */
  for (i = 0; i < G_N_ELEMENTS (locales); i++)
    {
      BOOL used_default;
      int nbytes;

      if (locales[i].cp == 0)
	continue;

      /* Convert to that code page into bufp */
      
      nbytes = WideCharToMultiByte (locales[i].cp, 0, wcs, -1,
				    *bufp, (nchars+1)*2,
				    NULL, &used_default);

      if (!used_default)
	{
	  /* This locale is good for the string */
	  g_free (wcs);
	  *lcidp = locales[i].lcid;
	  *sizep = nbytes;
	  return TRUE;
	}
    }

  g_free (*bufp);
  g_free (wcs);

  return FALSE;
}

void
gdk_property_change (GdkWindow    *window,
		     GdkAtom       property,
		     GdkAtom       type,
		     gint          format,
		     GdkPropMode   mode,
		     const guchar *data,
		     gint          nelements)
{
  HGLOBAL hdata, hlcid, hutf8;
  UINT cf = 0;
  LCID lcid;
  LCID *lcidptr;
  GString *rtf = NULL;
  gint i, size, nchars;
  gchar *prop_name, *type_name;
  guchar *ucptr, *buf = NULL;
  wchar_t *wcptr;
  enum { PLAIN_ASCII, UNICODE_TEXT, SINGLE_LOCALE, RICH_TEXT } method;
  gboolean ok = TRUE;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (DND,
	    (prop_name = gdk_atom_name (property),
	     type_name = gdk_atom_name (type),
	     g_print ("gdk_property_change: %#x %#x (%s) %#x (%s) %s %d*%d bytes %.10s\n",
		      (guint) GDK_WINDOW_HWND (window),
		      (guint) property, prop_name,
		      (guint) type, type_name,
		      (mode == GDK_PROP_MODE_REPLACE ? "REPLACE" :
		       (mode == GDK_PROP_MODE_PREPEND ? "PREPEND" :
			(mode == GDK_PROP_MODE_APPEND ? "APPEND" :
			 "???"))),
		      format, nelements, data),
	     g_free (prop_name),
	     g_free (type_name)));

  if (property == _gdk_selection_property
      && type == GDK_TARGET_STRING
      && format == 8
      && mode == GDK_PROP_MODE_REPLACE)
    {
      if (!OpenClipboard (GDK_WINDOW_HWND (window)))
	{
	  WIN32_API_FAILED ("OpenClipboard");
	  return;
	}

      /* Check if only ASCII */
      for (i = 0; i < nelements; i++)
	if (data[i] >= 0200)
	  break;

      if (i == nelements)
	nchars = nelements;
      else
	nchars = g_utf8_strlen (data, nelements);

      GDK_NOTE (DND, g_print ("...nchars:%d\n", nchars));
      
      if (i == nelements)
	{
	  /* If only ASCII, use CF_TEXT and the data as such. */
	  method = PLAIN_ASCII;
	  size = nelements;
	  for (i = 0; i < nelements; i++)
	    if (data[i] == '\n')
	      size++;
	  size++;
	  GDK_NOTE (DND, g_print ("...as text: %.40s\n", data));
	}
      else if (IS_WIN_NT ())
	{
	  /* On NT, use CF_UNICODETEXT if any non-ASCII char present */
	  method = UNICODE_TEXT;
	  size = (nchars + 1) * 2;
	  GDK_NOTE (DND, g_print ("...as Unicode\n"));
	}
      else if (find_common_locale (data, nelements, nchars, &lcid, &buf, &size))
	{
	  /* On Win9x, if all chars are in the default code page of
	   * some installed locale, use CF_TEXT and CF_LOCALE.
	   */
	  method = SINGLE_LOCALE;
	  GDK_NOTE (DND, g_print ("...as text in locale %#lx %d bytes\n",
				  (gulong) lcid, size));
	}
      else
	{
	  /* On Win9x, otherwise use RTF */

	  const guchar *p = data;

	  method = RICH_TEXT;
	  rtf = g_string_new ("{\\rtf1\\uc0 ");

	  while (p < data + nelements)
	    {
	      if (*p == '{' ||
		  *p == '\\' ||
		  *p == '}')
		{
		  rtf = g_string_append_c (rtf, '\\');
		  rtf = g_string_append_c (rtf, *p);
		  p++;
		}
	      else if (*p < 0200)
		{
		  rtf = g_string_append_c (rtf, *p);
		  p++;
		}
	      else
		{
		  guchar *q;
		  gint n;
		  
		  rtf = g_string_append (rtf, "\\uNNNNN ");
		  rtf->len -= 6; /* five digits and a space */
		  q = rtf->str + rtf->len;
		  n = sprintf (q, "%d ", g_utf8_get_char (p));
		  g_assert (n <= 6);
		  rtf->len += n;
		  
		  p = g_utf8_next_char (p);
		}
	    }
	  rtf = g_string_append (rtf, "}");
	  size = rtf->len + 1;
	  GDK_NOTE (DND, g_print ("...as RTF: %.40s\n", rtf->str));
	}
	  
      if (!(hdata = GlobalAlloc (GMEM_MOVEABLE, size)))
	{
	  WIN32_API_FAILED ("GlobalAlloc");
	  if (!CloseClipboard ())
	    WIN32_API_FAILED ("CloseClipboard");
	  if (buf != NULL)
	    g_free (buf);
	  if (rtf != NULL)
	    g_string_free (rtf, TRUE);
	  return;
	}

      ucptr = GlobalLock (hdata);

      switch (method)
	{
	case PLAIN_ASCII:
	  cf = CF_TEXT;
	  for (i = 0; i < nelements; i++)
	    {
	      if (*data == '\n')
		*ucptr++ = '\r';
	      *ucptr++ = data[i];
	    }
	  *ucptr++ = '\0';
	  break;

	case UNICODE_TEXT:
	  cf = CF_UNICODETEXT;
	  wcptr = (wchar_t *) ucptr;
	  if (_gdk_utf8_to_ucs2 (wcptr, data, nelements, nchars) == -1)
	    g_warning ("_gdk_utf8_to_ucs2() failed");
	  wcptr[nchars] = 0;
	  break;

	case SINGLE_LOCALE:
	  cf = CF_TEXT;
	  memmove (ucptr, buf, size);
	  g_free (buf);

	  /* Set the CF_LOCALE clipboard data, too */
	  if (!(hlcid = GlobalAlloc (GMEM_MOVEABLE, sizeof (LCID))))
	    WIN32_API_FAILED ("GlobalAlloc"), ok = FALSE;
	  if (ok)
	    {
	      lcidptr = GlobalLock (hlcid);
	      *lcidptr = lcid;
	      GlobalUnlock (hlcid);
	      if (!SetClipboardData (CF_LOCALE, hlcid))
		WIN32_API_FAILED ("SetClipboardData (CF_LOCALE)"), ok = FALSE;
	    }
	  break;

	case RICH_TEXT:
	  cf = cf_rtf;
	  memmove (ucptr, rtf->str, size);
	  g_string_free (rtf, TRUE);

	  /* Set the UTF8_STRING clipboard data, too, for other
	   * GTK+ apps to use (won't bother reading RTF).
	   */
	  if (!(hutf8 = GlobalAlloc (GMEM_MOVEABLE, nelements)))
	    WIN32_API_FAILED ("GlobalAlloc");
	  else
	    {
	      guchar *utf8ptr = GlobalLock (hutf8);
	      memmove (utf8ptr, data, nelements);
	      GlobalUnlock (hutf8);
	      if (!SetClipboardData (cf_utf8_string, hutf8))
		WIN32_API_FAILED ("SetClipboardData (UTF8_STRING)");
	    }
	  break;

	default:
	  g_assert_not_reached ();
	}

      GlobalUnlock (hdata);
      if (ok && !SetClipboardData (cf, hdata))
	WIN32_API_FAILED ("SetClipboardData"), ok = FALSE;
      
      if (!CloseClipboard ())
	WIN32_API_FAILED ("CloseClipboard");
    }
  else
    g_warning ("gdk_property_change: General case not implemented");
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  gchar *prop_name;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (DND,
	    (prop_name = gdk_atom_name (property),
	     g_print ("gdk_property_delete: %#x %#x (%s)\n",
		      (window ? (guint) GDK_WINDOW_HWND (window) : 0),
		      (guint) property, prop_name),
	     g_free (prop_name)));

  if (property == _gdk_selection_property)
    _gdk_selection_property_delete (window);
  else
    g_warning ("gdk_property_delete: General case not implemented");
}

gboolean
gdk_setting_get (const gchar *name,
		 GValue      *value)
{
  /*
   * XXX : if these values get changed through the Windoze UI the
   *       respective gdk_events are not generated yet.
   */
  if (strcmp ("double-click-timeout", name) == 0)
    {
      g_value_set_int (value, GetDoubleClickTime ());
      return TRUE;
    }
  else if (strcmp ("drag-threshold", name) == 0)
    {
      g_value_set_int (value, MAX(GetSystemMetrics (SM_CXDRAG), GetSystemMetrics (SM_CYDRAG)));
      return TRUE;
    }
  else
    return FALSE;
}
