/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <string.h>
#include <stdlib.h>

#include <gdk/gdk.h>
#include "gdkwin32.h"

/* We emulate the GDK_SELECTION property of windows (as used in the
 * X11 backend) by using a hash table from GdkWindows to GdkSelProp
 * structs.
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
gdk_win32_selection_init (void)
{
  sel_prop_table = g_hash_table_new (NULL, NULL);
  sel_owner_table = g_hash_table_new (NULL, NULL);
}

static void
sel_prop_store (GdkWindow *window,
		GdkAtom    type,
		gint       format,
		guchar    *data,
		gint       length)
{
  GdkSelProp *prop;

  prop = g_hash_table_lookup (sel_prop_table, GDK_DRAWABLE_XID (window));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_hash_table_remove (sel_prop_table, GDK_DRAWABLE_XID (window));
    }
  prop = g_new (GdkSelProp, 1);
  prop->data = data;
  prop->length = length;
  prop->format = format;
  prop->type = type;
  g_hash_table_insert (sel_prop_table, GDK_DRAWABLE_XID (window), prop);
}

void
gdk_win32_dropfiles_store (gchar *data)
{
  if (data != NULL)
    {
      g_assert (dropfiles_prop == NULL);

      dropfiles_prop = g_new (GdkSelProp, 1);
      dropfiles_prop->data = data;
      dropfiles_prop->length = strlen (data);
      dropfiles_prop->format = 8;
      dropfiles_prop->type = text_uri_list_atom;
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

gint
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gint       send_event)
{
  HWND xwindow;
  GdkEvent tmp_event;
  gchar *sel_name;

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     g_print ("gdk_selection_owner_set: %p %#x (%s)\n",
		      (owner ? GDK_DRAWABLE_XID (owner) : 0),
		      (guint) selection, sel_name),
	     g_free (sel_name)));

  if (selection != gdk_clipboard_atom)
    {
      if (owner != NULL)
	g_hash_table_insert (sel_owner_table, selection, GDK_DRAWABLE_XID (owner));
      else
	g_hash_table_remove (sel_owner_table, selection);
      return TRUE;
    }

  /* Rest of this function handles the CLIPBOARD selection */
  if (owner != NULL)
    xwindow = GDK_DRAWABLE_XID (owner);
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
      tmp_event.selection.property = gdk_selection_property;
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
  if (selection == gdk_clipboard_atom)
    return NULL;

  window = gdk_window_lookup (g_hash_table_lookup (sel_owner_table, selection));

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     g_print ("gdk_selection_owner_get: %#x (%s) = %p\n",
		      (guint) selection, sel_name,
		      (window ? GDK_DRAWABLE_XID (window) : 0)),
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
  gchar *sel_name, *tgt_name;
  GdkAtom property = gdk_selection_property;

  g_return_if_fail (requestor != NULL);
  if (GDK_DRAWABLE_DESTROYED (requestor))
    return;

  GDK_NOTE (DND,
	    (sel_name = gdk_atom_name (selection),
	     tgt_name = gdk_atom_name (target),
	     g_print ("gdk_selection_convert: %p %#x (%s) %#x (%s)\n",
		      GDK_DRAWABLE_XID (requestor),
		      (guint) selection, sel_name,
		      (guint) target, tgt_name),
	     g_free (sel_name),
	     g_free (tgt_name)));

  if (selection == gdk_clipboard_atom &&
      (target == compound_text_atom ||
       target == GDK_TARGET_STRING))
    {
      /* Converting the CLIPBOARD selection means he wants the
       * contents of the clipboard. Get the clipboard data,
       * and store it for later.
       */
      if (!OpenClipboard (GDK_DRAWABLE_XID (requestor)))
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

	      data = gdk_nwchar_ts_to_mbs (wcs, wclen);
	      g_free (wcs);
	      
	      sel_prop_store (requestor, target, 8,
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
	      
	      sel_prop_store (requestor, target, 8,
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
	      wclen = MultiByteToWideChar (cp, 0, ptr, -1,
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

	      data = gdk_nwchar_ts_to_mbs (wcs2, wclen);
	      g_free (wcs2);
	      
	      sel_prop_store (requestor, target, 8,
			      data, strlen (data) + 1);
	      GlobalUnlock (hdata);
	    }
	}
      else
	property = GDK_NONE;
	  
      CloseClipboard ();
    }
  else if (selection == gdk_win32_dropfiles_atom)
    {
      /* This means he wants the names of the dropped files.
       * gdk_dropfiles_filter already has stored the text/uri-list
       * data temporarily in dropfiles_prop.
       */
      if (dropfiles_prop != NULL)
	{
	  sel_prop_store (requestor, selection, dropfiles_prop->format,
			  dropfiles_prop->data, dropfiles_prop->length);
	  g_free (dropfiles_prop);
	  dropfiles_prop = NULL;
	}
    }
  else
    property = GDK_NONE;

  /* Generate a selection notify message so that we actually fetch
   * the data (if property == gdk_selection_property) or indicating failure
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

  if (GDK_DRAWABLE_DESTROYED (requestor))
    return 0;
  
  GDK_NOTE (DND, g_print ("gdk_selection_property_get: %p\n",
			   GDK_DRAWABLE_XID (requestor)));

  prop = g_hash_table_lookup (sel_prop_table, GDK_DRAWABLE_XID (requestor));

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
  
  GDK_NOTE (DND, g_print ("gdk_selection_property_delete: %p\n",
			   GDK_DRAWABLE_XID (window)));

  prop = g_hash_table_lookup (sel_prop_table, GDK_DRAWABLE_XID (window));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_hash_table_remove (sel_prop_table, GDK_DRAWABLE_XID (window));
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
	     g_print ("gdk_selection_send_notify: %p %#x (%s) %#x (%s) %#x (%s)\n",
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

  *list = g_new (gchar *, 1);
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
    *encoding = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

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
