/* gdkwindow-quartz.c
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


#include "config.h"

#include "gdkfont.h"


GdkFont *
gdk_fontset_load_for_display (GdkDisplay  *display,
			      const gchar *fontset_name)
{
  return NULL;
}

GdkFont*
gdk_fontset_load (const gchar *fontset_name)
{
  return NULL;
}

void
_gdk_font_destroy (GdkFont *font)
{
  /* FIXME: Implement */
}

gint
_gdk_font_strlen (GdkFont     *font,
		  const gchar *str)
{
  /* FIXME: Implement */
  return -1;
}

gint
gdk_font_id (const GdkFont *font)
{
  /* FIXME: Implement */
  return 0;
}

gboolean
gdk_font_equal (const GdkFont *fonta,
                const GdkFont *fontb)
{
  /* FIXME: Implement */
  return FALSE;
}

GdkDisplay* 
gdk_font_get_display (GdkFont* font)
{
  /* FIXME: Implement */ 
  return NULL;
}
