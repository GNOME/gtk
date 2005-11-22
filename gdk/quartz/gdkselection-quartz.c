/* gdkselection-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include <config.h>

#include "gdkselection.h"

gboolean
gdk_selection_owner_set_for_display (GdkDisplay *display,
				     GdkWindow  *owner,
				     GdkAtom     selection,
				     guint32     time,
				     gint        send_event)
{
  /* FIXME: Implement */
  return TRUE;
}

GdkWindow*
gdk_selection_owner_get_for_display (GdkDisplay *display,
				     GdkAtom     selection)
{
  /* FIXME: Implement */
  return NULL;
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
  /* FIXME: Implement */
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  /* FIXME: Implement */
  return 0;
}

void
gdk_selection_send_notify_for_display (GdkDisplay *display,
				       guint32     requestor,
				       GdkAtom     selection,
				       GdkAtom     target,
				       GdkAtom     property,
				       guint32     time)
{
  /* FIXME: Implement */
}

gint
gdk_text_property_to_text_list_for_display (GdkDisplay   *display,
					    GdkAtom       encoding,
					    gint          format, 
					    const guchar *text,
					    gint          length,
					    gchar      ***list)
{
  /* FIXME: Implement */
  return 0;
}

gint
gdk_string_to_compound_text_for_display (GdkDisplay  *display,
					 const gchar *str,
					 GdkAtom     *encoding,
					 gint        *format,
					 guchar     **ctext,
					 gint        *length)
{
  /* FIXME: Implement */
  return 0;
}

void gdk_free_compound_text (guchar *ctext)
{
  /* FIXME: Implement */
}

gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  /* FIXME: Implement */
  return NULL;
}

gboolean
gdk_utf8_to_compound_text_for_display (GdkDisplay  *display,
				       const gchar *str,
				       GdkAtom     *encoding,
				       gint        *format,
				       guchar     **ctext,
				       gint        *length)
{
  /* FIXME: Implement */
  return 0;
}

gint 
gdk_text_property_to_utf8_list_for_display (GdkDisplay    *display,
					    GdkAtom        encoding,
					    gint           format,
					    const guchar  *text,
					    gint           length,
					    gchar       ***list)
{
  /* FIXME: Implement */
  return 0;
}
