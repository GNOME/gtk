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

#include "gdkx.h"
#include "gdkdisplay-x11.h"
#include "gdkpango.h"
#include <pango/pangox.h>
#ifdef HAVE_XFT
#include <pango/pangoxft.h>
#endif

/**
 * gdk_pango_context_get_for_screen:
 * @screen: the #GdkScreen for which the context is to be created.
 * 
 * Creates a #PangoContext for @screen.
 *
 * The context must be freed when you're finished with it.
 * 
 * When using GTK+, normally you should use gtk_widget_get_pango_context()
 * instead of this function, to get the appropriate context for
 * the widget you intend to render text onto.
 * 
 * Return value: a new #PangoContext for @screen
 **/
PangoContext *
gdk_pango_context_get_for_screen (GdkScreen *screen)
{
  PangoContext *context;
  GdkDisplayX11 *display_x11;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (screen->closed)
    return NULL;
  
  display_x11 = GDK_DISPLAY_X11 (GDK_SCREEN_DISPLAY (screen));
  
#ifdef HAVE_XFT
  if (display_x11->use_xft == -1)
    {
      const char *val = g_getenv ("GDK_USE_XFT");

      display_x11->use_xft = val && (atoi (val) != 0) && 
	_gdk_x11_have_render (GDK_SCREEN_DISPLAY (screen));
    }
  
  if (display_x11->use_xft)
    context = pango_xft_get_context (GDK_SCREEN_XDISPLAY (screen),
				     GDK_SCREEN_X11 (screen)->screen_num);
  else
#endif /* HAVE_XFT */
    context = pango_x_get_context (GDK_SCREEN_XDISPLAY (screen));
  
  g_object_set_data (G_OBJECT (context), "gdk-pango-screen", screen);
  
  return context;
}
