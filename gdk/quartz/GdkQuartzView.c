/* GdkQuartzView.m
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


#import "GdkQuartzView.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"

@implementation GdkQuartzView

-(void)setGdkWindow:(GdkWindow *)window
{
  gdk_window = window;
}

-(GdkWindow *)gdkWindow
{
  return gdk_window;
}

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)isOpaque
{
  /* A view is opaque if its GdkWindow doesn't have the RGBA colormap */
  return gdk_drawable_get_colormap (gdk_window) != gdk_screen_get_rgba_colormap (_gdk_screen);
}

-(void)drawRect:(NSRect)rect 
{
  GdkRectangle gdk_rect;
  GdkWindowObject *private = GDK_WINDOW_OBJECT (gdk_window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  GDK_QUARTZ_ALLOC_POOL;

  gdk_rect.x = rect.origin.x;
  gdk_rect.y = rect.origin.y;
  gdk_rect.width = rect.size.width;
  gdk_rect.height = rect.size.height;
  
  if (private->event_mask & GDK_EXPOSURE_MASK)
    {
      GdkEvent event;
      
      event.expose.type = GDK_EXPOSE;
      event.expose.window = g_object_ref (gdk_window);
      event.expose.send_event = FALSE;
      event.expose.count = 0;
      event.expose.region = gdk_region_rectangle (&gdk_rect);
      event.expose.area = gdk_rect;
    
      impl->in_paint_rect_count ++;

      (*_gdk_event_func) (&event, _gdk_event_data);

      impl->in_paint_rect_count --;

      g_object_unref (gdk_window);
      gdk_region_destroy (event.expose.region);
    }

  GDK_QUARTZ_RELEASE_POOL;
}

@end
