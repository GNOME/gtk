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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include "gdk.h"
#include "gdkprivate.h"
#include "gdkx.h"


gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gint       send_event)
{
  Display *xdisplay;
  Window xwindow;

  if (owner)
    {
      GdkWindowPrivate *private;

      private = (GdkWindowPrivate*) owner;
      if (private->destroyed)
	return FALSE;

      xdisplay = private->xdisplay;
      xwindow = private->xwindow;
    }
  else
    {
      xdisplay = gdk_display;
      xwindow = None;
    }

  XSetSelectionOwner (xdisplay, selection, xwindow, time);

  return (XGetSelectionOwner (xdisplay, selection) == xwindow);
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  Window xwindow;

  xwindow = XGetSelectionOwner (gdk_display, selection);
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
  GdkWindowPrivate *private;

  g_return_if_fail (requestor != NULL);

  private = (GdkWindowPrivate*) requestor;
  if (private->destroyed)
    return;

  XConvertSelection (private->xdisplay, selection, target,
		     gdk_selection_property, private->xwindow, time);
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  GdkWindowPrivate *private;
  gulong nitems;
  gulong nbytes;
  gulong length;
  GdkAtom prop_type;
  gint prop_format;
  guchar *t = NULL;

  g_return_val_if_fail (requestor != NULL, 0);

  /* If retrieved chunks are typically small, (and the ICCCM says the
     should be) it would be a win to try first with a buffer of
     moderate length, to avoid two round trips to the server */

  private = (GdkWindowPrivate*) requestor;
  if (private->destroyed)
    return 0;

  t = NULL;
  XGetWindowProperty (private->xdisplay, private->xwindow,
		      gdk_selection_property, 0, 0, False,
		      AnyPropertyType, &prop_type, &prop_format,
		      &nitems, &nbytes, &t);

  if (ret_type)
    *ret_type = prop_type;
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
  XGetWindowProperty (private->xdisplay, private->xwindow,
		      gdk_selection_property, 0, (nbytes + 3) / 4, False,
		      AnyPropertyType, &prop_type, &prop_format,
		      &nitems, &nbytes, &t);

  if (prop_type != None)
    {
      *data = g_new (guchar, length);
      memcpy (*data, t, length);
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
  xevent.selection = selection;
  xevent.target = target;
  xevent.property = property;
  xevent.time = time;

  gdk_send_xevent (requestor, False, NoEventMask, (XEvent*) &xevent);
}


/* The output of XmbTextPropertyToTextList may include stuff not valid
 * for COMPOUND_TEXT. This routine tries to correct this by:
 *
 * a) Canonicalizing CR LF and CR to LF
 * b) Stripping out all other non-allowed control characters
 *
 * See the COMPOUND_TEXT spec distributed with X for explanations
 * what is allowed.
 */
static gchar *
sanitize_ctext (const char *str,
		gint       *length)
{
  gchar *result = g_malloc (*length + 1);
  gint out_length = 0;
  gint i;
  const guchar *ustr = (const guchar *)str;

  for (i=0; i < *length; i++)
    {
      guchar c = ustr[i];
      
      if (c == '\r')
	{
	  result[out_length++] = '\n';
	  if (i + 1 < *length && ustr[i + 1] == '\n')
	    i++;
	}
      else if (c == 27 /* ESC */)
	{
	  /* Check for "extended segments, which can contain arbitrary
	   * octets. See CTEXT spec, section 6.
	   */

	  if (i + 5 < *length &&
	      ustr[i + 1] == '%' &&
	      ustr[i + 2] == '/' &&
	      (ustr[i + 3] >= 48 && ustr[i + 3] <= 52) &&
	      ustr[i + 4] >= 128 &&
	      ustr[i + 5] >= 128)
	    {
	      int extra_len = 6 + (ustr[i + 4] - 128) * 128 + ustr[i + 5] - 128;
	      extra_len = MAX (extra_len, *length - i);

	      memcpy (result + out_length, ustr + i, extra_len);
	      out_length += extra_len;
	      i += extra_len - 1;
	    }
	  else
	    result[out_length++] = c;	    
	}
      else if (c == '\n' || c == '\t' || c == 27 /* ESC */ ||
	       (c >= 32 && c <= 127) ||	/* GL */
	       c == 155 /* CONTROL SEQUENCE INTRODUCER */ ||	
	       (c >= 160 && c <= 255)) /* GR */
	{
	  result[out_length++] = c;
	}
    }

  result[out_length] = '\0';
  *length = out_length;
  
  return result;
}

gint
gdk_text_property_to_text_list (GdkAtom encoding, gint format, 
				guchar *text, gint length,
				gchar ***list)
{
  XTextProperty property;
  gint count = 0;
  gint res;
  gchar *sanitized_text = NULL;

  if (!list) 
    return 0;

  property.encoding = encoding;
  property.format = format;

  if (encoding == gdk_atom_intern ("COMPOUND_TEXT", FALSE) && format == 8)
    {
      gint sanitized_text_length = length;
      
      property.value = sanitized_text = sanitize_ctext (text, &sanitized_text_length);
      property.nitems = sanitized_text_length;
    }
  else
    {
      property.value = text;
      property.nitems = length;
    }
  
  res = XmbTextPropertyToTextList (GDK_DISPLAY(), &property, list, &count);

  if (sanitized_text)
    g_free (sanitized_text);

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

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom *encoding, gint *format,
			     guchar **ctext, gint *length)
{
  gint res;
  XTextProperty property;
  gint sanitized_text_length;
  gchar *sanitized_text;

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

  g_assert (property.encoding == gdk_atom_intern ("COMPOUND_TEXT", FALSE) && property.format == 8);

  if (encoding)
    *encoding = property.encoding;
  if (format)
    *format = property.format;

  sanitized_text_length = property.nitems;
  sanitized_text = sanitize_ctext (property.value, &sanitized_text_length);

  if (ctext)
    *ctext = sanitized_text;
  else
    g_free (sanitized_text);
  
  if (length)
    *length = sanitized_text_length;

  if (property.value)
    XFree (property.value);

  return res;
}

void gdk_free_compound_text (guchar *ctext)
{
  if (ctext)
    g_free (ctext);
}
