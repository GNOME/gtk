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

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate-win32.h"

/* We emulate the GDK_SELECTION window properties of windows (as used
 * in the X11 backend) by using a hash table from GdkWindows to
 * GdkSelProp structs.
 */

typedef struct {
  guchar *data;
  gint length;
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
}

void
_gdk_selection_property_store (GdkWindow *owner,
                               GdkAtom    type,
                               gint       format,
                               guchar    *data,
                               gint       length)
{
  GdkSelProp *prop;

  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (owner));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_hash_table_remove (sel_prop_table, GDK_WINDOW_HWND (owner));
    }
  prop = g_new (GdkSelProp, 1);
  prop->data = data;
  prop->length = length;
  prop->format = format;
  prop->type = type;
  g_hash_table_insert (sel_prop_table, GDK_WINDOW_HWND (owner), prop);
}

void
_gdk_dropfiles_store (gchar *data)
{
  if (data != NULL)
    {
      g_assert (dropfiles_prop == NULL);

      dropfiles_prop = g_new (GdkSelProp, 1);
      dropfiles_prop->data = data;
      dropfiles_prop->length = strlen (data);
      dropfiles_prop->format = 8;
      dropfiles_prop->type = text_uri_list;
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
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
  HWND xwindow;
  GdkEvent tmp_event;
  gchar *sel_name;

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     g_print ("gdk_selection_owner_set: %#x %#x (%s)\n",
		      (owner ? (guint) GDK_WINDOW_HWND (owner) : 0),
		      (guint) selection, sel_name),
	     g_free (sel_name)));

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

      xwindow = GDK_WINDOW_HWND (owner);
    }
  else
    xwindow = NULL;

  if (!OpenClipboard (xwindow))
    {
      WIN32_API_FAILED ("OpenClipboard");
      return FALSE;
    }
  if (!EmptyClipboard ())
    {
      WIN32_API_FAILED ("EmptyClipboard");
      CloseClipboard ();
      return FALSE;
    }
#if 0
  /* No delayed rendering */
  if (xwindow != NULL)
    SetClipboardData (CF_TEXT, NULL);
#endif
  if (!CloseClipboard ())
    {
      WIN32_API_FAILED ("CloseClipboard");
      return FALSE;
    }

  if (owner != NULL)
    {
      /* Send ourselves a selection request message so that
       * gdk_property_change will be called to store the clipboard
       * data.
       */
      GDK_NOTE (DND, g_print ("...sending GDK_SELECTION_REQUEST to ourselves\n"));
      tmp_event.selection.type = GDK_SELECTION_REQUEST;
      tmp_event.selection.window = owner;
      tmp_event.selection.send_event = FALSE;
      tmp_event.selection.selection = selection;
      tmp_event.selection.target = GDK_TARGET_STRING;
      tmp_event.selection.property = _gdk_selection_property;
      tmp_event.selection.requestor = (guint32) xwindow;
      tmp_event.selection.time = time;

      gdk_event_put (&tmp_event);
    }

  return TRUE;
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  GdkWindow *window;
  gchar *sel_name;

  /* Return NULL for CLIPBOARD, because otherwise cut&paste
   * inside the same application doesn't work. We must pretend to gtk
   * that we don't have the selection, so that we always fetch it from
   * the Windows clipboard. See also comments in
   * gdk_selection_send_notify().
   */
  if (selection == GDK_SELECTION_CLIPBOARD)
    return NULL;

  window = gdk_window_lookup (g_hash_table_lookup (sel_owner_table, selection));

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     g_print ("gdk_selection_owner_get: %#x (%s) = %#x\n",
		      (guint) selection, sel_name,
		      (window ? (guint) GDK_WINDOW_HWND (window) : 0)),
	     g_free (sel_name)));

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
  gchar *sel_name, *tgt_name;

  g_return_if_fail (requestor != NULL);
  if (GDK_WINDOW_DESTROYED (requestor))
    return;

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     tgt_name = gdk_atom_name (target),
	     g_print ("gdk_selection_convert: %#x %#x (%s) %#x (%s)\n",
		      (guint) GDK_WINDOW_HWND (requestor),
		      (guint) selection, sel_name,
		      (guint) target, tgt_name),
	     g_free (sel_name),
	     g_free (tgt_name)));

  if (selection == GDK_SELECTION_CLIPBOARD &&
      target == gdk_atom_intern ("TARGETS", FALSE))
    {
      /* He wants to know what formats are on the clipboard.  If there
       * is some kind of text, tell him so.
       */
      if (!OpenClipboard (GDK_WINDOW_HWND (requestor)))
	{
	  WIN32_API_FAILED ("OpenClipboard");
	  return;
	}

      if (IsClipboardFormatAvailable (CF_UNICODETEXT) ||
	  IsClipboardFormatAvailable (cf_utf8_string) ||
	  IsClipboardFormatAvailable (CF_TEXT))
	{
	  GdkAtom *data = g_new (GdkAtom, 1);
	  *data = GDK_TARGET_STRING;
	  _gdk_selection_property_store (requestor, GDK_SELECTION_TYPE_ATOM,
					 32, data, 1 * sizeof (GdkAtom));
	}
      else
	property = GDK_NONE;
    }
  else if (selection == GDK_SELECTION_CLIPBOARD &&
	   (target == compound_text ||
	    target == GDK_TARGET_STRING))
    {
      /* Converting the CLIPBOARD selection means he wants the
       * contents of the clipboard. Get the clipboard data,
       * and store it for later.
       */
      if (!OpenClipboard (GDK_WINDOW_HWND (requestor)))
	{
	  WIN32_API_FAILED ("OpenClipboard");
	  return;
	}

      /* Try various formats. First the simplest, CF_UNICODETEXT. */
      if ((hdata = GetClipboardData (CF_UNICODETEXT)) != NULL)
	{
	  wchar_t *ptr, *wcs, *p, *q;
	  guchar *data;
	  gint length, wclen;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (DND, g_print ("...CF_UNICODETEXT: %d bytes\n",
				      length));

	      /* Strip out \r */
	      wcs = g_new (wchar_t, (length + 1) * 2);
	      p = ptr;
	      q = wcs;
	      wclen = 0;
	      while (*p)
		{
		  if (*p != '\r')
		    {
		      *q++ = *p;
		      wclen++;
		    }
		  p++;
		}

	      data = _gdk_ucs2_to_utf8 (wcs, wclen);
	      g_free (wcs);
	      
	      _gdk_selection_property_store (requestor, target, 8,
					     data, strlen (data) + 1);
	      GlobalUnlock (hdata);
	    }
	}
      else if ((hdata = GetClipboardData (cf_utf8_string)) != NULL)
	{
	  /* UTF8_STRING is a format we store ourselves when necessary */
	  guchar *ptr;
	  gint length;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (DND, g_print ("...UTF8_STRING: %d bytes: %.10s\n",
				      length, ptr));
	      
	      _gdk_selection_property_store (requestor, target, 8,
					     g_strdup (ptr), strlen (ptr) + 1);
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
	  gint length, wclen;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (DND, g_print ("...CF_TEXT: %d bytes: %.10s\n",
				       length, ptr));
	      
	      if ((hlcid = GetClipboardData (CF_LOCALE)) != NULL)
		{
		  gchar buf[10];
		  LCID *lcidptr = GlobalLock (hlcid);
		  if (GetLocaleInfo (*lcidptr, LOCALE_IDEFAULTANSICODEPAGE,
				     buf, sizeof (buf)))
		    {
		      cp = atoi (buf);
		      GDK_NOTE (DND, g_print ("...CF_LOCALE: %#lx cp:%d\n",
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
	      wclen = 0;
	      while (*p)
		{
		  if (*p != '\r')
		    {
		      *q++ = *p;
		      wclen++;
		    }
		  p++;
		}
	      g_free (wcs);

	      data = _gdk_ucs2_to_utf8 (wcs2, wclen);
	      g_free (wcs2);
	      
	      _gdk_selection_property_store (requestor, target, 8,
					     data, strlen (data) + 1);
	      GlobalUnlock (hdata);
	    }
	}
      else
	property = GDK_NONE;

      CloseClipboard ();
    }
  else if (selection == gdk_win32_dropfiles)
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

  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  if (GDK_WINDOW_DESTROYED (requestor))
    return 0;
  
  GDK_NOTE (DND, g_print ("gdk_selection_property_get: %#x\n",
			   (guint) GDK_WINDOW_HWND (requestor)));

  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (requestor));

  if (prop == NULL)
    {
      *data = NULL;
      return 0;
    }

  *data = g_malloc (prop->length);
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
  
  GDK_NOTE (DND, g_print ("_gdk_selection_property_delete: %#x\n",
			   (guint) GDK_WINDOW_HWND (window)));

  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (window));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_hash_table_remove (sel_prop_table, GDK_WINDOW_HWND (window));
    }
}

void
gdk_selection_send_notify (guint32  requestor,
			   GdkAtom  selection,
			   GdkAtom  target,
			   GdkAtom  property,
			   guint32  time)
{
  GdkEvent tmp_event;
  gchar *sel_name, *tgt_name, *prop_name;

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     tgt_name = gdk_atom_name (target),
	     prop_name = gdk_atom_name (property),
	     g_print ("gdk_selection_send_notify: %#x %#x (%s) %#x (%s) %#x (%s)\n",
		      requestor,
		      (guint) selection, sel_name,
		      (guint) target, tgt_name,
		      (guint) property, prop_name),
	     g_free (sel_name),
	     g_free (tgt_name),
	     g_free (prop_name)));

  /* Send ourselves a selection clear message so that gtk thinks we don't
   * have the selection, and will claim it anew when needed, and
   * we thus get a chance to store data in the Windows clipboard.
   * Otherwise, if a gtkeditable does a copy to CLIPBOARD several times
   * only the first one actually gets copied to the Windows clipboard,
   * as only the first one causes a call to gdk_property_change().
   *
   * Hmm, there is something fishy with this. Cut and paste inside the
   * same app didn't work, the gtkeditable immediately forgot the
   * clipboard contents in gtk_editable_selection_clear() as a result
   * of this message. OTOH, when I changed gdk_selection_owner_get to
   * return NULL for CLIPBOARD, it works. Sigh.
   */

  tmp_event.selection.type = GDK_SELECTION_CLEAR;
  tmp_event.selection.window = gdk_window_lookup (requestor);
  tmp_event.selection.send_event = FALSE;
  tmp_event.selection.selection = selection;
  tmp_event.selection.target = 0;
  tmp_event.selection.property = 0;
  tmp_event.selection.requestor = 0;
  tmp_event.selection.time = time;

  gdk_event_put (&tmp_event);
}

/* Simplistic implementations of text list and compound text functions */

gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
  gchar *enc_name;

  GDK_NOTE (DND, (enc_name = gdk_atom_name (encoding),
		  g_print ("gdk_text_property_to_text_list: %s %d %.20s %d\n",
			   enc_name, format, text, length),
		  g_free (enc_name)));

  if (!list)
    return 0;

  *list = g_new (gchar **, 1);
  **list = g_strdup (text);
  
  return 1;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  g_free (*list);
  g_free (list);
}

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
  GDK_NOTE (DND, g_print ("gdk_string_to_compound_text: %.20s\n", str));

  if (encoding)
    *encoding = compound_text;

  if (format)
    *format = 8;

  if (ctext)
    *ctext = g_strdup (str);

  if (length)
    *length = strlen (str);

  return 0;
}

void
gdk_free_compound_text (guchar *ctext)
{
  g_free (ctext);
}

/* These are lifted from gdkselection-x11.c, just to get GTK+ to build.
 * These functions probably don't make much sense at all in Windows.
 */

/* FIXME */

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

/**
 * gdk_text_property_to_utf8_list:
 * @encoding: an atom representing the encoding of the text
 * @format:   the format of the property
 * @text:     the text to convert
 * @length:   the length of @text, in bytes
 * @list:     location to store the list of strings or %NULL. The
 *            list should be freed with g_strfreev().
 * 
 * Converts a text property in the giving encoding to
 * a list of UTF-8 strings. 
 * 
 * Return value: the number of strings in the resulting
 *               list.
 **/
gint 
gdk_text_property_to_utf8_list (GdkAtom        encoding,
				gint           format,
				const guchar  *text,
				gint           length,
				gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  
  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == utf8_string)
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar **local_list;
      gint local_count;
      gint i;
      const gchar *charset = NULL;
      gboolean need_conversion = g_get_charset (&charset);
      gint count = 0;
      GError *error = NULL;
      
      /* Probably COMPOUND text, we fall back to Xlib routines
       */
      local_count = gdk_text_property_to_text_list (encoding,
						    format, 
						    text,
						    length,
						    &local_list);
      if (list)
	*list = g_new (gchar *, local_count + 1);
      
      for (i=0; i<local_count; i++)
	{
	  /* list contains stuff in our default encoding
	   */
	  if (need_conversion)
	    {
	      gchar *utf = g_convert (local_list[i], -1,
				      "UTF-8", charset,
				      NULL, NULL, &error);
	      if (utf)
		{
		  if (list)
		    (*list)[count++] = utf;
		  else
		    g_free (utf);
		}
	      else
		{
		  g_warning ("Error converting to UTF-8 from '%s': %s",
			     charset, error->message);
		  g_error_free (error);
		  error = NULL;
		}
	    }
	  else
	    {
	      if (list)
		(*list)[count++] = g_strdup (local_list[i]);
	    }
	}
      
      gdk_free_text_list (local_list);
      (*list)[count] = NULL;

      return count;
    }
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, \r\n, and \n\r to \n
 */
static gchar * 
sanitize_utf8 (const gchar *src)
{
  gint len = strlen (src);
  GString *result = g_string_sized_new (len);
  const gchar *p = src;

  while (*p)
    {
      if (*p == '\r' || *p == '\n')
	{
	  p++;
	  if (*p == '\r' || *p == '\n')
	    p++;

	  g_string_append_c (result, '\n');
	}
      else
	{
	  gunichar ch = g_utf8_get_char (p);
	  char buf[7];
	  gint buflen;
	  
	  if (!((ch < 0x20 && ch != '\t') || (ch >= 0x7f && ch < 0xa0)))
	    {
	      buflen = g_unichar_to_utf8 (ch, buf);
	      g_string_append_len (result, buf, buflen);
	    }

	  p = g_utf8_next_char (p);
	}
    }

  return g_string_free (result, FALSE);
}

/**
 * gdk_utf8_to_string_target:
 * @str: a UTF-8 string
 * 
 * Converts an UTF-8 string into the best possible representation
 * as a STRING. The representation of characters not in STRING
 * is not specified; it may be as pseudo-escape sequences
 * \x{ABCD}, or it may be in some other form of approximation.
 * 
 * Return value: the newly allocated string, or %NULL if the
 *               conversion failed. (It should not fail for
 *               any properly formed UTF-8 string.)
 **/
gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  return sanitize_utf8 (str);
}

/**
 * gdk_utf8_to_compound_text:
 * @str:      a UTF-8 string
 * @encoding: location to store resulting encoding
 * @format:   location to store format of the result
 * @ctext:    location to store the data of the result
 * @length:   location to store the length of the data
 *            stored in @ctext
 * 
 * Converts from UTF-8 to compound text. 
 * 
 * Return value: %TRUE if the conversion succeeded, otherwise
 *               %FALSE.
 **/
gboolean
gdk_utf8_to_compound_text (const gchar *str,
			   GdkAtom     *encoding,
			   gint        *format,
			   guchar     **ctext,
			   gint        *length)
{
  gboolean need_conversion;
  const gchar *charset;
  gchar *locale_str, *tmp_str;
  GError *error = NULL;
  gboolean result;

  g_return_val_if_fail (str != NULL, FALSE);

  need_conversion = !g_get_charset (&charset);

  tmp_str = sanitize_utf8 (str);

  if (need_conversion)
    {
      locale_str = g_convert_with_fallback (tmp_str, -1,
					    charset, "UTF-8",
					    NULL, NULL, NULL, &error);
      g_free (tmp_str);

      if (!locale_str)
	{
	  g_warning ("Error converting from UTF-8 to '%s': %s",
		     charset, error->message);
	  g_error_free (error);

	  if (encoding)
	    *encoding = GDK_NONE;
	  if (format)
	    *format = GPOINTER_TO_UINT (GDK_ATOM_TO_POINTER (GDK_NONE));
	  if (ctext)
	    *ctext = NULL;
	  if (length)
	    *length = 0;

	  return FALSE;
	}
    }
  else
    locale_str = tmp_str;
    
  result = gdk_string_to_compound_text (locale_str,
					encoding, format, ctext, length);
  
  g_free (locale_str);

  return result;
}
