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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate.h"
#include "gdkprivate-x11.h"

typedef struct _OwnerInfo OwnerInfo;

struct _OwnerInfo
{
  GdkAtom    selection;
  GdkWindow *owner;
  gulong     serial;
};

static GSList *owner_list;

/* When a window is destroyed we check if it is the owner
 * of any selections. This is somewhat inefficient, but
 * owner_list is typically short, and it is a low memory,
 * low code solution
 */
void
_gdk_selection_window_destroyed (GdkWindow *window)
{
  GSList *tmp_list = owner_list;
  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;
      tmp_list = tmp_list->next;
      
      if (info->owner == window)
	{
	  owner_list = g_slist_remove (owner_list, info);
	  g_free (info);
	}
    }
}

/* We only pass through those SelectionClear events that actually
 * reflect changes to the selection owner that we didn't make ourself.
 */
gboolean
_gdk_selection_filter_clear_event (XSelectionClearEvent *event)
{
  GSList *tmp_list = owner_list;

  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;
      if (info->selection == gdk_x11_xatom_to_atom (event->selection))
	{
	  if ((GDK_DRAWABLE_XID (info->owner) == event->window &&
	       event->serial >= info->serial))
	    {
	      owner_list = g_slist_remove (owner_list, info);
	      g_free (info);
	      return TRUE;
	    }
	  else
	    return FALSE;
	}
      tmp_list = tmp_list->next;
    }

  return FALSE;
}

gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
  Display *xdisplay;
  Window xwindow;
  Atom xselection;
  GSList *tmp_list;
  OwnerInfo *info;

  xselection = gdk_x11_atom_to_xatom (selection);
  
  if (owner)
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return FALSE;

      xdisplay = GDK_WINDOW_XDISPLAY (owner);
      xwindow = GDK_WINDOW_XID (owner);
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = None;
    }
  
  tmp_list = owner_list;
  while (tmp_list)
    {
      info = tmp_list->data;
      if (info->selection == selection)
	{
	  owner_list = g_slist_remove (owner_list, info);
	  g_free (info);
	  break;
	}
      tmp_list = tmp_list->next;
    }

  if (owner)
    {
      info = g_new (OwnerInfo, 1);
      info->owner = owner;
      info->serial = NextRequest (GDK_WINDOW_XDISPLAY (owner));
      info->selection = selection;

      owner_list = g_slist_prepend (owner_list, info);
    }

  XSetSelectionOwner (xdisplay, xselection, xwindow, time);

  return (XGetSelectionOwner (xdisplay, xselection) == xwindow);
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  Window xwindow;

  xwindow = XGetSelectionOwner (gdk_display, gdk_x11_atom_to_xatom (selection));
  if (xwindow == None)
    return NULL;

  return gdk_window_lookup (xwindow);
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
  if (GDK_WINDOW_DESTROYED (requestor))
    return;

  XConvertSelection (GDK_WINDOW_XDISPLAY (requestor),
		     gdk_x11_atom_to_xatom (selection),
		     gdk_x11_atom_to_xatom (target),
		     gdk_x11_atom_to_xatom (_gdk_selection_property),
		     GDK_WINDOW_XID (requestor), time);
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  gulong nitems;
  gulong nbytes;
  gulong length;
  Atom prop_type;
  gint prop_format;
  guchar *t = NULL;

  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  /* If retrieved chunks are typically small, (and the ICCCM says the
     should be) it would be a win to try first with a buffer of
     moderate length, to avoid two round trips to the server */

  if (GDK_WINDOW_DESTROYED (requestor))
    return 0;

  t = NULL;
  XGetWindowProperty (GDK_WINDOW_XDISPLAY (requestor),
		      GDK_WINDOW_XID (requestor),
		      gdk_x11_atom_to_xatom (_gdk_selection_property),
		      0, 0, False,
		      AnyPropertyType, &prop_type, &prop_format,
		      &nitems, &nbytes, &t);

  if (ret_type)
    *ret_type = gdk_x11_xatom_to_atom (prop_type);
  if (ret_format)
    *ret_format = prop_format;

  if (prop_type == None)
    {
      *data = NULL;
      return 0;
    }
  
  if (t)
    {
      XFree (t);
      t = NULL;
    }

  /* Add on an extra byte to handle null termination.  X guarantees
     that t will be 1 longer than nbytes and null terminated */
  length = nbytes + 1;

  /* We can't delete the selection here, because it might be the INCR
     protocol, in which case the client has to make sure they'll be
     notified of PropertyChange events _before_ the property is deleted.
     Otherwise there's no guarantee we'll win the race ... */
  XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (requestor),
		      GDK_DRAWABLE_XID (requestor),
		      gdk_x11_atom_to_xatom (_gdk_selection_property),
		      0, (nbytes + 3) / 4, False,
		      AnyPropertyType, &prop_type, &prop_format,
		      &nitems, &nbytes, &t);

  if (prop_type != None)
    {
      *data = g_new (guchar, length);

      if (prop_type == XA_ATOM)
	{
	  Atom* atoms = (Atom*) t;
	  GdkAtom* atoms_dest = (GdkAtom*) *data;
	  gint num_atom, i;
	  
	  num_atom = (length - 1) / sizeof (GdkAtom);
	  for (i=0; i < num_atom; i++)
	    atoms_dest[i] = gdk_x11_xatom_to_atom (atoms[i]);
	}
      else
	{
	  memcpy (*data, t, length);
	}
      
      if (t)
	XFree (t);
      return length-1;
    }
  else
    {
      *data = NULL;
      return 0;
    }
}


void
gdk_selection_send_notify (guint32  requestor,
			   GdkAtom  selection,
			   GdkAtom  target,
			   GdkAtom  property,
			   guint32  time)
{
  XSelectionEvent xevent;

  xevent.type = SelectionNotify;
  xevent.serial = 0;
  xevent.send_event = True;
  xevent.display = gdk_display;
  xevent.requestor = requestor;
  xevent.selection = gdk_x11_atom_to_xatom (selection);
  xevent.target = gdk_x11_atom_to_xatom (target);
  xevent.property = gdk_x11_atom_to_xatom (property);
  xevent.time = time;

  gdk_send_xevent (requestor, False, NoEventMask, (XEvent*) &xevent);
}

gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
  XTextProperty property;
  gint count = 0;
  gint res;

  if (!list) 
    return 0;

  property.value = (guchar *)text;
  property.encoding = gdk_x11_atom_to_xatom (encoding);
  property.format = format;
  property.nitems = length;
  res = XmbTextPropertyToTextList (GDK_DISPLAY(), &property, list, &count);

  if (res == XNoMemory || res == XLocaleNotSupported || 
      res == XConverterNotFound)
    return 0;
  else
    return count;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  XFreeStringList (list);
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
  else if (encoding == gdk_atom_intern ("UTF8_STRING", FALSE))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar **local_list;
      gint local_count;
      gint i;
      const gchar *charset = NULL;
      gboolean need_conversion = !g_get_charset (&charset);
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

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
  gint res;
  XTextProperty property;

  res = XmbTextListToTextProperty (GDK_DISPLAY(), 
				   (char **)&str, 1, XCompoundTextStyle,
                               	   &property);
  if (res != Success)
    {
      property.encoding = None;
      property.format = None;
      property.value = NULL;
      property.nitems = 0;
    }

  if (encoding)
    *encoding = gdk_x11_xatom_to_atom (property.encoding);
  if (format)
    *format = property.format;
  if (ctext)
    *ctext = property.value;
  if (length)
    *length = property.nitems;

  return res;
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
 * Return value: the newly-allocated string, or %NULL if the
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
	    *encoding = None;
	  if (format)
	    *format = None;
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
  result = (result == Success? TRUE : FALSE);
  
  g_free (locale_str);

  return result;
}

void gdk_free_compound_text (guchar *ctext)
{
  if (ctext)
    XFree (ctext);
}
