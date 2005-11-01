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

#include <config.h>
#include <string.h>
#include <stdlib.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkdisplay.h"
#include "gdkprivate-win32.h"

/* We emulate the GDK_SELECTION window properties of windows (as used
 * in the X11 backend) by using a hash table from GdkWindows to
 * GdkSelProp structs.
 */

typedef struct {
  guchar *data;
  gsize length;
  gint format;
  GdkAtom type;
} GdkSelProp;

static GHashTable *sel_prop_table = NULL;

static GdkSelProp *dropfiles_prop = NULL;

/* We store the owner of each selection in this table. Obviously, this only
 * is valid intra-app, and in fact it is necessary for the intra-app DND to work.
 */
static GHashTable *sel_owner_table = NULL;

void
_gdk_win32_selection_init (void)
{
  sel_prop_table = g_hash_table_new (NULL, NULL);
  sel_owner_table = g_hash_table_new (NULL, NULL);
  _format_atom_table = g_hash_table_new (NULL, NULL);
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
static gchar * 
sanitize_utf8 (const gchar *src,
	       gint         length)
{
  GString *result = g_string_sized_new (length + 1);
  const gchar *p = src;
  const gchar *endp = src + length;

  while (p < endp)
    {
      if (*p == '\r')
	{
	  p++;
	  if (*p == '\n')
	    p++;

	  g_string_append_c (result, '\n');
	}
      else
	{
	  gunichar ch = g_utf8_get_char (p);
	  char buf[7];
	  gint buflen;
	  
	  if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
	    {
	      buflen = g_unichar_to_utf8 (ch, buf);
	      g_string_append_len (result, buf, buflen);
	    }

	  p = g_utf8_next_char (p);
	}
    }
  g_string_append_c (result, '\0');

  return g_string_free (result, FALSE);
}

static gchar *
_gdk_utf8_to_string_target_internal (const gchar *str,
				     gint         length)
{
  GError *error = NULL;
  
  gchar *tmp_str = sanitize_utf8 (str, length);
  gchar *result =  g_convert_with_fallback (tmp_str, -1,
					    "ISO-8859-1", "UTF-8",
					    NULL, NULL, NULL, &error);
  if (!result)
    {
      g_warning ("Error converting from UTF-8 to STRING: %s",
		 error->message);
      g_error_free (error);
    }
  
  g_free (tmp_str);
  return result;
}

static void
_gdk_selection_property_store (GdkWindow *owner,
                               GdkAtom    type,
                               gint       format,
                               guchar    *data,
                               gint       length)
{
  GdkSelProp *prop;
  GSList *prop_list;

  prop = g_new (GdkSelProp, 1);

  if (type == GDK_TARGET_STRING)
    {
      /* We know that data is UTF-8 */
      prop->data = _gdk_utf8_to_string_target_internal (data, length);
      g_free (data);

      if (!prop->data)
	{
	  g_free (prop);

	  return;
	}
      else
	prop->length = strlen (prop->data) + 1;
    }
  else
    {
      prop->data = data;
      prop->length = length;
    }
  prop->format = format;
  prop->type = type;
  prop_list = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (owner));
  prop_list = g_slist_append (prop_list, prop);
  g_hash_table_insert (sel_prop_table, GDK_WINDOW_HWND (owner), prop_list);
}

void
_gdk_dropfiles_store (gchar *data)
{
  if (data != NULL)
    {
      g_assert (dropfiles_prop == NULL);

      dropfiles_prop = g_new (GdkSelProp, 1);
      dropfiles_prop->data = data;
      dropfiles_prop->length = strlen (data) + 1;
      dropfiles_prop->format = 8;
      dropfiles_prop->type = _text_uri_list;
    }
  else
    {
      if (dropfiles_prop != NULL)
	{
	  g_free (dropfiles_prop->data);
	  g_free (dropfiles_prop);
	}
      dropfiles_prop = NULL;
    }
}

gboolean
gdk_selection_owner_set_for_display (GdkDisplay *display,
                                     GdkWindow  *owner,
                                     GdkAtom     selection,
                                     guint32     time,
                                     gboolean    send_event)
{
  HWND hwnd;
  GdkEvent tmp_event;

  g_return_val_if_fail (display == _gdk_display, FALSE);
  g_return_val_if_fail (selection != GDK_NONE, FALSE);

#ifdef G_ENABLE_DEBUG
  {
    gchar *sel_name;

    GDK_NOTE (DND,
	      (sel_name = gdk_atom_name (selection),
	       g_print ("gdk_selection_owner_set_for_display: %p %#x (%s)\n",
			(owner ? GDK_WINDOW_HWND (owner) : NULL),
			(guint) selection, sel_name),
	       g_free (sel_name)));
  }
#endif

  if (selection != GDK_SELECTION_CLIPBOARD)
    {
      if (owner != NULL)
	g_hash_table_insert (sel_owner_table, selection, GDK_WINDOW_HWND (owner));
      else
	g_hash_table_remove (sel_owner_table, selection);
      return TRUE;
    }

  /* Rest of this function handles the CLIPBOARD selection */
  if (owner != NULL)
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return FALSE;

      hwnd = GDK_WINDOW_HWND (owner);
    }
  else
    hwnd = NULL;

  if (!API_CALL (OpenClipboard, (hwnd)))
    return FALSE;

  _ignore_destroy_clipboard = TRUE;
  GDK_NOTE (DND, g_print ("... EmptyClipboard()\n"));
  if (!API_CALL (EmptyClipboard, ()))
    {
      _ignore_destroy_clipboard = FALSE;
      API_CALL (CloseClipboard, ());
      return FALSE;
    }
  _ignore_destroy_clipboard = FALSE;

  if (!API_CALL (CloseClipboard, ()))
    return FALSE;

  if (owner != NULL)
    {
      /* Send ourselves a selection request message so that
       * gdk_property_change will be called to store the clipboard
       * data.
       */
      GDK_NOTE (DND, g_print ("... sending GDK_SELECTION_REQUEST to ourselves\n"));
      tmp_event.selection.type = GDK_SELECTION_REQUEST;
      tmp_event.selection.window = owner;
      tmp_event.selection.send_event = FALSE;
      tmp_event.selection.selection = selection;
      tmp_event.selection.target = _utf8_string;
      tmp_event.selection.property = _gdk_selection_property;
      tmp_event.selection.requestor = (guint32) hwnd;
      tmp_event.selection.time = time;

      gdk_event_put (&tmp_event);
    }

  return TRUE;
}

GdkWindow*
gdk_selection_owner_get_for_display (GdkDisplay *display,
                                     GdkAtom     selection)
{
  GdkWindow *window;

  g_return_val_if_fail (display == _gdk_display, NULL);
  g_return_val_if_fail (selection != GDK_NONE, NULL);

  if (selection == GDK_SELECTION_CLIPBOARD)
    {
      HWND owner = GetClipboardOwner ();

      if (owner == NULL)
	return NULL;

      return gdk_win32_handle_table_lookup ((GdkNativeWindow) owner);
    }

  window = gdk_window_lookup ((GdkNativeWindow) g_hash_table_lookup (sel_owner_table, selection));

#ifdef G_ENABLE_DEBUG
  {
    gchar *sel_name;
    
    GDK_NOTE (DND,
	      (sel_name = gdk_atom_name (selection),
	       g_print ("gdk_selection_owner_get: %#x (%s) = %p\n",
			(guint) selection, sel_name,
			(window ? GDK_WINDOW_HWND (window) : NULL)),
	       g_free (sel_name)));
  }
#endif

  return window;
}

static void
generate_selection_notify (GdkWindow *requestor,
			   GdkAtom    selection,
			   GdkAtom    target,
			   GdkAtom    property,
			   guint32    time)
{
  GdkEvent tmp_event;

  tmp_event.selection.type = GDK_SELECTION_NOTIFY;
  tmp_event.selection.window = requestor;
  tmp_event.selection.send_event = FALSE;
  tmp_event.selection.selection = selection;
  tmp_event.selection.target = target;
  tmp_event.selection.property = property;
  tmp_event.selection.requestor = 0;
  tmp_event.selection.time = time;

  gdk_event_put (&tmp_event);
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
  HGLOBAL hdata;
  GdkAtom property = _gdk_selection_property;

  g_return_if_fail (selection != GDK_NONE);
  g_return_if_fail (requestor != NULL);

  if (GDK_WINDOW_DESTROYED (requestor))
    return;

#ifdef G_ENABLE_DEBUG
  {
    gchar *sel_name, *tgt_name;
    
    GDK_NOTE (DND,
	      (sel_name = gdk_atom_name (selection),
	       tgt_name = gdk_atom_name (target),
	       g_print ("gdk_selection_convert: %p %#x (%s) %#x (%s)\n",
			GDK_WINDOW_HWND (requestor),
			(guint) selection, sel_name,
			(guint) target, tgt_name),
	       g_free (sel_name),
	       g_free (tgt_name)));
  }
#endif

  if (selection == GDK_SELECTION_CLIPBOARD && target == _targets)
    {
      gint formats_cnt, i, fmt;
      GdkAtom *data;
      gboolean has_bmp = FALSE;

      /* He wants to know what formats are on the clipboard. If there
       * is some kind of text, tell him so.
       */
      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      formats_cnt = CountClipboardFormats ();
      data = g_new (GdkAtom, formats_cnt + 2);
      i = 0;

      if (IsClipboardFormatAvailable (CF_UNICODETEXT) ||
	  IsClipboardFormatAvailable (_cf_utf8_string) ||
	  IsClipboardFormatAvailable (CF_TEXT))
	{
	  data[i++] = _utf8_string;
	}
      if (formats_cnt > 0)
        {
          /* If there is anything else in the clipboard, enum it all
           * although we don't offer special conversion services.
           */
          for (fmt = 0; 0 != (fmt = EnumClipboardFormats (fmt)); )
            {
              gchar sFormat[80];

              if (GetClipboardFormatName (fmt, sFormat, 80) > 0 &&
		  strcmp (sFormat, "UTF8_STRING"))
                {
		  GdkAtom atom;

		  if (!has_bmp &&
		      (!strcmp (sFormat, "image/bmp") ||
		       !strcmp (sFormat, "image/x-bmp") ||
		       !strcmp (sFormat, "image/x-MS-bmp")))
		    has_bmp = TRUE;
                  atom = gdk_atom_intern (sFormat, FALSE);
                  data[i++] = atom;
                }
            }
        }
      if (!has_bmp && (IsClipboardFormatAvailable (CF_BITMAP) ||
		       IsClipboardFormatAvailable (CF_DIB)))
	data[i++] = _image_bmp;

      if (i > 0)
	_gdk_selection_property_store (requestor, GDK_SELECTION_TYPE_ATOM,
				       32, (guchar *) data, i * sizeof (GdkAtom));
      else             
	property = GDK_NONE;

      API_CALL (CloseClipboard, ());
    }
  else if (selection == GDK_SELECTION_CLIPBOARD && target == _utf8_string)
    {
      /* Converting the CLIPBOARD selection means he wants the
       * contents of the clipboard. Get the clipboard data, and store
       * it for later.
       */
      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      /* Try various formats. First the simplest, CF_UNICODETEXT. */
      if (G_WIN32_IS_NT_BASED () && (hdata = GetClipboardData (CF_UNICODETEXT)) != NULL)
	{
	  wchar_t *ptr, *wcs, *p, *q;
	  guchar *data;
	  glong length, wclen;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (DND, g_print ("... CF_UNICODETEXT: %ld bytes\n",
				      length));

	      /* Strip out \r */
	      wcs = g_new (wchar_t, length / 2 + 1);
	      p = ptr;
	      q = wcs;
	      wclen = 0;
	      while (p < ptr + length / 2)
		{
		  if (*p != '\r')
		    {
		      *q++ = *p;
		      wclen++;
		    }
		  p++;
		}

	      data = g_utf16_to_utf8 (wcs, wclen, NULL, NULL, NULL);
	      g_free (wcs);

	      if (data)
		_gdk_selection_property_store (requestor, target, 8,
					       data, strlen (data) + 1);
	      GlobalUnlock (hdata);
	    }
	}
      else if ((hdata = GetClipboardData (_cf_utf8_string)) != NULL)
	{
	  /* UTF8_STRING is a format we store ourselves when necessary */
	  guchar *ptr;
	  gint length;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (DND, g_print ("... UTF8_STRING: %d bytes: %.10s\n",
				      length, ptr));
	      
	      _gdk_selection_property_store (requestor, target, 8,
					     g_memdup (ptr, length), length);
	      GlobalUnlock (hdata);
	    }
	}
      else if ((hdata = GetClipboardData (CF_TEXT)) != NULL)
	{
	  /* We must always assume the data can contain non-ASCII
	   * in either the current code page, or if there is CF_LOCALE
	   * data, in that locale's default code page.
	   */
	  HGLOBAL hlcid;
	  UINT cp = CP_ACP;
	  wchar_t *wcs, *wcs2, *p, *q;
	  guchar *ptr, *data;
	  glong length, wclen, wclen2;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (DND, g_print ("... CF_TEXT: %ld bytes: %.10s\n",
				       length, ptr));
	      
	      if ((hlcid = GetClipboardData (CF_LOCALE)) != NULL)
		{
		  gchar buf[10];
		  LCID *lcidptr = GlobalLock (hlcid);
		  if (GetLocaleInfo (*lcidptr, LOCALE_IDEFAULTANSICODEPAGE,
				     buf, sizeof (buf)))
		    {
		      cp = atoi (buf);
		      GDK_NOTE (DND, g_print ("... CF_LOCALE: %#lx cp:%d\n",
					      *lcidptr, cp));
		    }
		  GlobalUnlock (hlcid);
		}

	      wcs = g_new (wchar_t, length + 1);
	      wclen = MultiByteToWideChar (cp, 0, ptr, length,
					   wcs, length + 1);

	      /* Strip out \r */
	      wcs2 = g_new (wchar_t, wclen);
	      p = wcs;
	      q = wcs2;
	      wclen2 = 0;
	      while (p < wcs + wclen)
		{
		  if (*p != '\r')
		    {
		      *q++ = *p;
		      wclen2++;
		    }
		  p++;
		}
	      g_free (wcs);

	      data = g_utf16_to_utf8 (wcs2, wclen2, NULL, &length, NULL);
	      g_free (wcs2);

	      if (data)
		_gdk_selection_property_store (requestor, target, 8,
					       data, length + 1);
	      GlobalUnlock (hdata);
	    }
	}
      else
	property = GDK_NONE;

      API_CALL (CloseClipboard, ());
    }
  else if (selection == GDK_SELECTION_CLIPBOARD && target == _image_bmp)
    {
      guchar *data;

      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;
      if ((hdata = GetClipboardData (_cf_image_bmp)) != NULL)
	{
	  /* "image/bmp" is the first choice. */
	  guchar *ptr;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      gint length = GlobalSize (hdata);
      
	      GDK_NOTE (DND, g_print ("...BITMAP (from \"image/bmp\": %d bytes\n",
				      length));
      
	      _gdk_selection_property_store (requestor, target, 8,
					     g_memdup (ptr, length), length);
	      GlobalUnlock (hdata);
	    }
	}
      else if ((hdata = GetClipboardData (CF_DIB)) != NULL)
        {
	  /* If there's CF_DIB but not "image/bmp", the clipboard
	   * owner is probably a native Win32 application.
	   */
          BITMAPINFOHEADER *ptr;

          if ((ptr = GlobalLock (hdata)) != NULL)
            {
              BITMAPFILEHEADER *hdr; /* Need to add a file header so gdk-pixbuf can load it */
	      gint length = GlobalSize (hdata) + sizeof (BITMAPFILEHEADER);
	      
	      GDK_NOTE (DND, g_print ("... BITMAP (from CF_DIB): %d bytes\n", length));
	      
              data = g_try_malloc (length);
              if (data)
                {
                  hdr = (BITMAPFILEHEADER *)data;
                  hdr->bfType = 0x4d42; /* 0x42 = "B" 0x4d = "M" */
                  /* Compute the size of the entire file. */
                  hdr->bfSize = (DWORD) (sizeof (BITMAPFILEHEADER)
                        + ptr->biSize + ptr->biClrUsed
			* sizeof (RGBQUAD) + ptr->biSizeImage);
                  hdr->bfReserved1 = 0;
                  hdr->bfReserved2 = 0;
                  /* Compute the offset to the array of color indices. */
                  hdr->bfOffBits = (DWORD) sizeof (BITMAPFILEHEADER)
                        + ptr->biSize + ptr->biClrUsed * sizeof (RGBQUAD);
                  /* Copy the data behind it */
                  memcpy (data + sizeof (BITMAPFILEHEADER), ptr, length - sizeof (BITMAPFILEHEADER));
	          _gdk_selection_property_store (requestor, target, 8,
					         data, length);
                }
	      GlobalUnlock (hdata);
            }

      }

      API_CALL (CloseClipboard, ());
    }
  else if (selection == GDK_SELECTION_CLIPBOARD)
    {
      char *target_name;
      UINT fmt = 0;

      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      target_name = gdk_atom_name (target);

      /* Check if it's available. In fact, we can simply call
       * GetClipboardData (RegisterClipboardFormat (targetname)), but
       * the global custom format ID space is limited,
       * (0xC000~0xFFFF), and we better not waste an format ID if we
       * are just a requestor.
       */
      for ( ; 0 != (fmt = EnumClipboardFormats (fmt)); )
        {
          char sFormat[80];

          if (GetClipboardFormatName (fmt, sFormat, 80) > 0 && 
              strcmp (sFormat, target_name) == 0)
            {
              if ((hdata = GetClipboardData (fmt)) != NULL)
	        {
	          /* Simply get it without conversion */
                  guchar *ptr;
                  gint length;

                  if ((ptr = GlobalLock (hdata)) != NULL)
                    {
                      length = GlobalSize (hdata);
	      
                      GDK_NOTE (DND, g_print ("... %s: %d bytes\n", target_name, length));
	      
                      _gdk_selection_property_store (requestor, target, 8,
					             g_memdup (ptr, length), length);
	              GlobalUnlock (hdata);
                      break;
                    }
                }
            }
        }
      g_free (target_name);
      API_CALL (CloseClipboard, ());
    }
  else if (selection == _gdk_win32_dropfiles)
    {
      /* This means he wants the names of the dropped files.
       * gdk_dropfiles_filter already has stored the text/uri-list
       * data temporarily in dropfiles_prop.
       */
      if (dropfiles_prop != NULL)
	{
	  _gdk_selection_property_store
	    (requestor, selection, dropfiles_prop->format,
	     dropfiles_prop->data, dropfiles_prop->length);
	  g_free (dropfiles_prop);
	  dropfiles_prop = NULL;
	}
    }
  else
    property = GDK_NONE;

  /* Generate a selection notify message so that we actually fetch
   * the data (if property == _gdk_selection_property) or indicating failure
   * (if property == GDK_NONE).
   */
  generate_selection_notify (requestor, selection, target, property, time);
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  GdkSelProp *prop;
  GSList *prop_list;

  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  if (GDK_WINDOW_DESTROYED (requestor))
    return 0;
  
  GDK_NOTE (DND, g_print ("gdk_selection_property_get: %p\n",
			   GDK_WINDOW_HWND (requestor)));

  prop_list = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (requestor));
  prop = prop_list ? (GdkSelProp *) prop_list->data : NULL;

  if (prop == NULL)
    {
      *data = NULL;
      return 0;
    }

  *data = g_malloc (prop->length + 1);
  (*data)[prop->length] = '\0';
  if (prop->length > 0)
    memmove (*data, prop->data, prop->length);

  if (ret_type)
    *ret_type = prop->type;

  if (ret_format)
    *ret_format = prop->format;

  return prop->length;
}

void
_gdk_selection_property_delete (GdkWindow *window)
{
  GdkSelProp *prop;
  GSList *prop_list;
  
  GDK_NOTE (DND, g_print ("_gdk_selection_property_delete: %p\n",
			   GDK_WINDOW_HWND (window)));

  prop_list = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (window));
  if (prop_list && (prop = (GdkSelProp *) prop_list->data) != NULL)
    {
      g_free (prop->data);
      prop_list = g_slist_remove (prop_list, prop);
      g_free (prop);
      g_hash_table_insert (sel_prop_table, GDK_WINDOW_HWND (window), prop_list);
    }
}

void
gdk_selection_send_notify_for_display (GdkDisplay *display,
                                       guint32     requestor,
                                       GdkAtom     selection,
                                       GdkAtom     target,
                                       GdkAtom     property,
                                       guint32     time)
{
  g_return_if_fail (display == _gdk_display);

#ifdef G_ENABLE_DEBUG
  {
    gchar *sel_name, *tgt_name, *prop_name;

    GDK_NOTE (DND,
	      (sel_name = gdk_atom_name (selection),
	       tgt_name = gdk_atom_name (target),
	       prop_name = gdk_atom_name (property),
	       g_print ("gdk_selection_send_notify_for_display: %p %#x (%s) %#x (%s) %#x (%s)\n",
			(gpointer) requestor,
			(guint) selection, sel_name,
			(guint) target, tgt_name,
			(guint) property, prop_name),
	       g_free (sel_name),
	       g_free (tgt_name),
	       g_free (prop_name)));
  }
#endif
}

/* It's hard to say whether implementing this actually is of any use
 * on the Win32 platform? gtk calls only
 * gdk_text_property_to_utf8_list_for_display().
 */
gint
gdk_text_property_to_text_list_for_display (GdkDisplay   *display,
					    GdkAtom       encoding,
					    gint          format, 
					    const guchar *text,
					    gint          length,
					    gchar      ***list)
{
  gchar *result;
  const gchar *charset;
  gchar *source_charset;

  g_return_val_if_fail (display == _gdk_display, 0);

#ifdef G_ENABLE_DEBUG
  {
    gchar *enc_name;

    GDK_NOTE (DND, (enc_name = gdk_atom_name (encoding),
		    g_print ("gdk_text_property_to_text_list_for_display: %s %d %.20s %d\n",
			     enc_name, format, text, length),
		    g_free (enc_name)));
  }
#endif
    
  if (!list)
    return 0;

  if (encoding == GDK_TARGET_STRING)
    source_charset = g_strdup ("ISO-8859-1");
  else if (encoding == _utf8_string)
    source_charset = g_strdup ("UTF-8");
  else
    source_charset = gdk_atom_name (encoding);
    
  g_get_charset (&charset);

  result = g_convert (text, length, charset, source_charset,
		      NULL, NULL, NULL);
  g_free (source_charset);

  if (!result)
    return 0;

  *list = g_new (gchar *, 1);
  **list = result;
  
  return 1;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  g_free (*list);
  g_free (list);
}

static gint
make_list (const gchar  *text,
	   gint          length,
	   gboolean      latin1,
	   gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;
      
      q = p;
      while (*q && q < text + length)
	q++;

      if (latin1)
	{
	  str = g_convert (p, q - p,
			   "UTF-8", "ISO-8859-1",
			   NULL, NULL, &error);

	  if (!str)
	    {
	      g_warning ("Error converting selection from STRING: %s",
			 error->message);
	      g_error_free (error);
	    }
	}
      else
	str = g_strndup (p, q - p);

      if (str)
	{
	  strings = g_slist_prepend (strings, str);
	  n_strings++;
	}

      p = q + 1;
    }

  if (list)
    *list = g_new (gchar *, n_strings + 1);

  (*list)[n_strings] = NULL;
  
  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
	(*list)[--i] = tmp_list->data;
      else
	g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint 
gdk_text_property_to_utf8_list_for_display (GdkDisplay    *display,
                                            GdkAtom        encoding,
                                            gint           format,
                                            const guchar  *text,
                                            gint           length,
                                            gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (display == _gdk_display, 0);

  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == _utf8_string)
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar *enc_name = gdk_atom_name (encoding);

      g_warning ("gdk_text_property_to_utf8_list_for_display: encoding %s not handled\n", enc_name);
      g_free (enc_name);

      if (list)
	*list = NULL;

      return 0;
    }
}

gint
gdk_string_to_compound_text_for_display (GdkDisplay  *display,
					 const gchar *str,
					 GdkAtom     *encoding,
					 gint        *format,
					 guchar     **ctext,
					 gint        *length)
{
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (display == _gdk_display, 0);

  GDK_NOTE (DND, g_print ("gdk_string_to_compound_text_for_display: %.20s\n", str));

  /* Always fail on Win32. No COMPOUND_TEXT support. */

  if (encoding)
    *encoding = GDK_NONE;

  if (format)
    *format = 0;

  if (ctext)
    *ctext = NULL;

  if (length)
    *length = 0;

  return -1;
}

gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  return _gdk_utf8_to_string_target_internal (str, strlen (str));
}

gboolean
gdk_utf8_to_compound_text_for_display (GdkDisplay  *display,
                                       const gchar *str,
                                       GdkAtom     *encoding,
                                       gint        *format,
                                       guchar     **ctext,
                                       gint        *length)
{
  g_return_val_if_fail (str != NULL, FALSE);
  g_return_val_if_fail (display == _gdk_display, FALSE);

  GDK_NOTE (DND, g_print ("gdk_utf8_to_compound_text_for_display: %.20s\n", str));

  /* Always fail on Win32. No COMPOUND_TEXT support. */

  if (encoding)
    *encoding = GDK_NONE;

  if (format)
    *format = 0;
  
  if (ctext)
    *ctext = NULL;

  if (length)
    *length = 0;

  return FALSE;
}

void
gdk_free_compound_text (guchar *ctext)
{
  /* As we never generate anything claimed to be COMPOUND_TEXT, this
   * should never be called. Or if it is called, ctext should be the
   * NULL returned for conversions to COMPOUND_TEXT above.
   */
  g_return_if_fail (ctext == NULL);
}

void
gdk_win32_selection_add_targets (GdkWindow  *owner,
				 GdkAtom     selection,
				 gint	     n_targets,
				 GdkAtom    *targets)
{
  HWND hwnd;
  guint formatid;
  gint i;
  GSList *convertable_formats, *format;
  gboolean has_set_dib = FALSE, has_real_dib = FALSE;

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_DND)
    {
      gchar *sel_name = gdk_atom_name (selection);
      
      g_print ("gdk_win32_selection_add_targets: %p: %s: ",
	       owner ? GDK_WINDOW_HWND (owner) : NULL,
	       sel_name);
      g_free (sel_name);

      for (i = 0; i < n_targets; i++)
	{
	  gchar *tgt_name = gdk_atom_name (targets[i]);

	  g_print ("%s ", tgt_name);
	  g_free (tgt_name);
	}
      g_print ("\n");
    }
#endif

  if (selection != GDK_SELECTION_CLIPBOARD)
    return;

  if (owner != NULL)
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return;
      hwnd = GDK_WINDOW_HWND (owner);
    }

  if (!API_CALL (OpenClipboard, (hwnd)))
    return;

  convertable_formats = gdk_pixbuf_get_formats ();
  for (i = 0; i < n_targets; ++i)
    {
      gchar *target_name;

      if (targets[i] == _utf8_string ||
	  targets[i] == GDK_TARGET_STRING ||
	  targets[i] == _text ||
	  targets[i] == _compound_text ||
	  targets[i] == _save_targets)
	continue;

      target_name = gdk_atom_name (targets[i]);
      if (!(formatid = RegisterClipboardFormat (target_name)))
	{
	  WIN32_API_FAILED ("RegisterClipboardFormat");
	  API_CALL (CloseClipboard, ());
	  g_free (target_name);
	  return;
	}
      g_hash_table_replace (_format_atom_table, GINT_TO_POINTER (formatid), targets[i]);

      GDK_NOTE (DND, g_print ("... SetClipboardData(%s,NULL)\n",
			      _gdk_win32_cf_to_string (formatid)));
      SetClipboardData (formatid, NULL);

      /* We should replace the previous image format associated with
       * CF_DIB with "image/bmp" if we find "image/bmp", "image/x-bmp"
       * or "image/x-MS-bmp" is available.
       */
      if (!has_real_dib &&
	  (!strcmp (target_name, "image/bmp") ||
	   !strcmp (target_name, "image/x-bmp") ||
	   !strcmp (target_name, "image/x-MS-bmp")))
	{
	  g_hash_table_replace (_format_atom_table,
				GINT_TO_POINTER (CF_DIB),
				targets[i]);
	  if (!has_set_dib)
	    {
	      GDK_NOTE (DND, g_print ("... SetClipboardData(CF_DIB,NULL)\n"));
	      SetClipboardData (CF_DIB, NULL);
	      has_set_dib = TRUE;
	    }
	  has_real_dib = TRUE;
	  g_free (target_name);
	  continue;
	}

      for (format = convertable_formats; !has_set_dib && format; format = format->next)
	{
	  gchar **mime_types =
	    gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) format->data);

	  for (; *mime_types; ++mime_types)
	    {
	      if (!strcmp (target_name, *mime_types))
	        {
		  g_hash_table_replace (_format_atom_table,
					GINT_TO_POINTER (CF_DIB),
					targets[i]);
		  GDK_NOTE (DND, g_print ("... SetClipboardData(CF_DIB,NULL)\n"));
		  SetClipboardData (CF_DIB, NULL);
		  has_set_dib = TRUE;
		  break;
		}
	    }
	}
      g_free (target_name);
    }
  g_slist_free (convertable_formats);

  API_CALL (CloseClipboard, ());
}

/* Convert from types such as "image/jpg" or "image/png" to DIB using
 * gdk-pixbuf so that image copied from GTK+ apps can be pasted in
 * native apps like mspaint.exe
 */
HGLOBAL
_gdk_win32_selection_convert_to_dib (HGLOBAL  hdata,
				     GdkAtom  target)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  gchar *target_name;
  guchar *ptr;
  gchar *bmp_buf;
  gsize size;
  gboolean ok;

  if (!(target_name = gdk_atom_name (target)))
    {
      GlobalFree (hdata);
      return NULL;
    }

  if (!strcmp (target_name, "image/bmp") ||
      !strcmp (target_name, "image/x-bmp") ||
      !strcmp (target_name, "image/x-MS-bmp"))
    {
      /* No conversion is needed, just strip the BITMAPFILEHEADER */
      HGLOBAL hdatanew;

      g_free (target_name);
      size = GlobalSize (hdata) - 1 - sizeof (BITMAPFILEHEADER);
      ptr = GlobalLock (hdata);
      memmove (ptr, ptr + sizeof (BITMAPFILEHEADER), size);
      GlobalUnlock (hdata);
      if (!(hdatanew = GlobalReAlloc (hdata, size, 0)))
	{
	  WIN32_API_FAILED ("GlobalReAlloc");
	  GlobalFree (hdata); /* the old hdata is not freed if error */
	}
      return hdatanew;
    }

  /* We actually provide image formats -other than- "image/bmp" etc
   * and the requestor is either a native Win32 application or a GTK+
   * client that requested "image/bmp".
   */
  if (!(loader = gdk_pixbuf_loader_new_with_mime_type (target_name, NULL)))
    {
      GlobalFree (hdata);
      g_free (target_name);
      return NULL;
    }
  g_free (target_name);

  ptr = GlobalLock (hdata);
  ok = gdk_pixbuf_loader_write (loader, ptr, GlobalSize (hdata) - 1, NULL) &&
       gdk_pixbuf_loader_close (loader, NULL);

  GlobalUnlock (hdata);
  GlobalFree (hdata);
  hdata = NULL;

  if (ok && (pixbuf = gdk_pixbuf_loader_get_pixbuf (loader)) != NULL)
    g_object_ref (pixbuf);

  g_object_unref (loader);

  if (ok && gdk_pixbuf_save_to_buffer (pixbuf, &bmp_buf, &size, "bmp", NULL, NULL))
    {
      size -= sizeof (BITMAPFILEHEADER);
      if (!(hdata = GlobalAlloc (GMEM_MOVEABLE, size)))
	{
	  WIN32_API_FAILED ("GlobalAlloc");
	  ok = FALSE;
	}

      if (ok)
	{
	  ptr = GlobalLock (hdata);
	  memcpy (ptr, bmp_buf + sizeof (BITMAPFILEHEADER), size);
	  GlobalUnlock (hdata);
	}

      g_free (bmp_buf);
      g_object_unref (pixbuf);
    }

  return hdata;
}
