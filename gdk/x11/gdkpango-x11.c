/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc. 
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

#include <config.h>
#include <stdlib.h>

#include "gdkprivate-x11.h"
#include "gdkpango.h"
#include <pango/pangox.h>
#ifdef HAVE_XFT
#include <pango/pangoxft.h>
#endif

PangoContext *
gdk_pango_context_get (void)
{
#ifdef HAVE_XFT
  static gint use_xft = -1;
  if (use_xft == -1)
    {
      char *val = g_getenv ("GDK_USE_XFT");

      use_xft = val && (atoi (val) != 0);
    }
  
  if (use_xft)
    return pango_xft_get_context (GDK_DISPLAY (), DefaultScreen (GDK_DISPLAY ()));
  else
#endif /* HAVE_XFT */
    return pango_x_get_context (GDK_DISPLAY ());
}
