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

#include "config.h"

#include <string.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkinternals.h"
#include "gdkprivate.h"
#include "gdkprivate-win32.h"
#include "gdkwindow-win32.h"

/* We emulate the GDK_SELECTION window properties by storing
 * it's data in a per-window hashtable.
 */

typedef struct {
  guchar *data;
  gint length;
  gint format;
  GdkAtom type;
} GdkSelProp;

static GHashTable *sel_prop_table = NULL;

void
gdk_win32_selection_init (void)
{
  if (sel_prop_table == NULL)
    sel_prop_table = g_hash_table_new (g_int_hash, g_int_equal);
}

void
gdk_sel_prop_store (GdkWindow *owner,
		    GdkAtom    type,
		    gint       format,
		    guchar    *data,
		    gint       length)
{
  GdkSelProp *prop;

  prop = g_hash_table_lookup (sel_prop_table, &GDK_WINDOW_HWND (owner));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_hash_table_remove (sel_prop_table, &GDK_WINDOW_HWND (owner));
    }
  prop = g_new (GdkSelProp, 1);
  prop->data = data;
  prop->length = length;
  prop->format = format;
  prop->type = type;
  g_hash_table_insert (sel_prop_table, &GDK_WINDOW_HWND (owner), prop);
}

gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
  gchar *sel_name;
  HWND xwindow;

  GDK_NOTE (MISC,
	    (sel_name = gdk_atom_name (selection),
	     g_print ("gdk_selection_owner_set: %#x %#x (%s)\n",
		      (owner ? (guint) GDK_WINDOW_HWND (owner) : 0),
		      (guint) selection, sel_name),
	     g_free (sel_name)));

  if (selection != gdk_clipboard_atom)
    return FALSE;

  if (owner != NULL)
    xwindow = GDK_WINDOW_HWND (owner);
  else
    xwindow = NULL;

  GDK_NOTE (MISC, g_print ("...OpenClipboard(%#x)\n", (guint) xwindow));
  if (!OpenClipboard (xwindow))
    {
      WIN32_API_FAILED ("OpenClipboard");
      return FALSE;
    }
  GDK_NOTE (MISC, g_print ("...EmptyClipboard()\n"));
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
  GDK_NOTE (MISC, g_print ("...CloseClipboard()\n"));
  if (!CloseClipboard ())
    {
      WIN32_API_FAILED ("CloseClipboard");
      return FALSE;
    }
  if (owner != NULL)
    {
      /* Send ourselves an ersatz selection request message so that
       * gdk_property_change will be called to store the clipboard data.
       */
      SendMessage (xwindow, gdk_selection_request_msg,
		   selection, 0);
    }

  return TRUE;
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  GdkWindow *window;
  gchar *sel_name;

#if 1
  /* XXX Hmm, gtk selections seem to work best with this. This causes
   * gtk to always get the clipboard contents from Windows, and not
   * from the editable's own stashed-away copy.
   */
  return NULL;
#else
  if (selection != gdk_clipboard_atom)
    window = NULL;
  else
    window = gdk_win32_handle_table_lookup (GetClipboardOwner ());

#endif

  GDK_NOTE (MISC,
	    (sel_name = gdk_atom_name (selection),
	     g_print ("gdk_selection_owner_get: %#x (%s) = %#x\n",
		      (guint) selection, sel_name,
		      (window ? (guint) GDK_WINDOW_HWND (window) : 0)),
	     g_free (sel_name)));

  return window;
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
  HGLOBAL hdata;
  guchar *ptr, *data, *datap, *p;
  guint i, length, slength;
  gchar *sel_name, *tgt_name;

  g_return_if_fail (requestor != NULL);
  if (GDK_WINDOW_DESTROYED (requestor))
    return;

  GDK_NOTE (MISC,
	    (sel_name = gdk_atom_name (selection),
	     tgt_name = gdk_atom_name (target),
	     g_print ("gdk_selection_convert: %#x %#x (%s) %#x (%s)\n",
		      (guint) GDK_WINDOW_HWND (requestor),
		      (guint) selection, sel_name,
		      (guint) target, tgt_name),
	     g_free (sel_name),
	     g_free (tgt_name)));

  if (selection == gdk_clipboard_atom)
    {
      /* Converting the CLIPBOARD selection means he wants the
       * contents of the clipboard. Get the clipboard data,
       * and store it for later.
       */
      GDK_NOTE (MISC, g_print ("...OpenClipboard(%#x)\n",
			       (guint) GDK_WINDOW_HWND (requestor)));
      if (!OpenClipboard (GDK_WINDOW_HWND (requestor)))
	{
	  WIN32_API_FAILED ("OpenClipboard");
	  return;
	}

      GDK_NOTE (MISC, g_print ("...GetClipboardData(CF_TEXT)\n"));
      if ((hdata = GetClipboardData (CF_TEXT)) != NULL)
	{
	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);
	      
	      GDK_NOTE (MISC, g_print ("...got data: %d bytes: %.10s\n",
				       length, ptr));
	      
	      slength = 0;
	      p = ptr;
	      for (i = 0; i < length; i++)
		{
		  if (*p == '\0')
		    break;
		  else if (*p != '\r')
		    slength++;
		  p++;
		}
	      
	      data = datap = g_malloc (slength + 1);
	      p = ptr;
	      for (i = 0; i < length; i++)
		{
		  if (*p == '\0')
		    break;
		  else if (*p != '\r')
		    *datap++ = *p;
		  p++;
		}
	      *datap++ = '\0';
	      gdk_sel_prop_store (requestor, GDK_TARGET_STRING, 8,
				  data, strlen (data) + 1);
	      
	      GlobalUnlock (hdata);
	    }
	}
      GDK_NOTE (MISC, g_print ("...CloseClipboard()\n"));
      CloseClipboard ();


      /* Send ourselves an ersatz selection notify message so that we actually
       * fetch the data.
       */
      SendMessage (GDK_WINDOW_HWND (requestor), gdk_selection_notify_msg, selection, target);
    }
  else if (selection == gdk_win32_dropfiles_atom)
    {
      /* This means he wants the names of the dropped files.
       * gdk_dropfiles_filter already has stored the text/uri-list
       * data, tempoarily on gdk_root_parent's selection "property".
       */
      GdkSelProp *prop;

      prop = g_hash_table_lookup (sel_prop_table,
				  &GDK_WINDOW_HWND (gdk_parent_root));

      if (prop != NULL)
	{
	  g_hash_table_remove (sel_prop_table,
			       &GDK_WINDOW_HWND (gdk_parent_root));
	  gdk_sel_prop_store (requestor, prop->type, prop->format,
			      prop->data, prop->length);
	  g_free (prop);
	  SendMessage (GDK_WINDOW_HWND (requestor), gdk_selection_notify_msg, selection, target);
	}
    }
  else
    {
      g_warning ("gdk_selection_convert: General case not implemented");
    }
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
  
  GDK_NOTE (MISC, g_print ("gdk_selection_property_get: %#x\n",
			   (guint) GDK_WINDOW_HWND (requestor)));

  prop = g_hash_table_lookup (sel_prop_table, &GDK_WINDOW_HWND (requestor));

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
gdk_selection_property_delete (GdkWindow *window)
{
  GdkSelProp *prop;
  
  prop = g_hash_table_lookup (sel_prop_table, &GDK_WINDOW_HWND (window));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_hash_table_remove (sel_prop_table, &GDK_WINDOW_HWND (window));
    }
  else
    g_warning ("huh?");
}

void
gdk_selection_send_notify (guint32  requestor,
			   GdkAtom  selection,
			   GdkAtom  target,
			   GdkAtom  property,
			   guint32  time)
{
  gchar *sel_name, *tgt_name, *prop_name;

  GDK_NOTE (MISC,
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
   * Otherwise, if a gtkeditable does a copy to clipboard several times
   * only the first one actually gets copied to the Windows clipboard,
   * as only he first one causes a call to gdk_property_change.
   *
   * Hmm, there is something fishy with this. Cut and paste inside the
   * same app didn't work, the gtkeditable immediately forgot the
   * clipboard contents in gtk_editable_selection_clear as a result of
   * this message. OTOH, when I changed gdk_selection_owner_get to
   * always return NULL, it works. Sigh.
   */

  SendMessage ((HWND) requestor, gdk_selection_clear_msg, selection, 0);
}

gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
  GDK_NOTE (MISC,
	    g_print ("gdk_text_property_to_text_list not implemented\n"));
  
  return 0;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  /* ??? */
}

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
  g_warning ("gdk_string_to_compound_text: Not implemented");

  return 0;
}

void
gdk_free_compound_text (guchar *ctext)
{
  g_warning ("gdk_free_compound_text: Not implemented");
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
 * Convert a text property in the giving encoding to
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
  else if (encoding == gdk_atom_intern ("UTF8_STRING", FALSE))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar **local_list;
      gint local_count;
      gint i;
      gchar *charset = NULL;
      gboolean need_conversion= g_get_charset (&charset);
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
 * Convert an UTF-8 string into the best possible representation
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
  GError *error = NULL;
  
  gchar *tmp_str = sanitize_utf8 (str);
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

/**
 * gdk_utf8_to_compound_text:
 * @str:      a UTF-8 string
 * @encoding: location to store resulting encoding
 * @format:   location to store format of the result
 * @ctext:    location to store the data of the result
 * @length:   location to store the length of the data
 *            stored in @ctext
 * 
 * Convert from UTF-8 to compound text. 
 * 
 * Return value: %TRUE if the conversion succeeded, otherwise
 *               false.
 **/
gboolean
gdk_utf8_to_compound_text (const gchar *str,
			   GdkAtom     *encoding,
			   gint        *format,
			   guchar     **ctext,
			   gint        *length)
{
  gboolean need_conversion;
  gchar *charset;
  gchar *locale_str, *tmp_str;
  GError *error = NULL;
  gboolean result;

  g_return_val_if_fail (str != NULL, FALSE);

  need_conversion = g_get_charset (&charset);

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
	    *format = GDK_NONE;
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
