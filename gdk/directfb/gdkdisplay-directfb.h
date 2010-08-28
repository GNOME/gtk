/*
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

#ifndef GDK_DISPLAY_DFB_H
#define GDK_DISPLAY_DFB_H

#include <directfb.h>
#include <gdk/gdkdisplay.h>
#include <gdk/gdkkeys.h>

G_BEGIN_DECLS

typedef struct _GdkDisplayDFB GdkDisplayDFB;
typedef struct _GdkDisplayDFBClass GdkDisplayDFBClass;


#define GDK_TYPE_DISPLAY_DFB              (gdk_display_dfb_get_type())
#define GDK_DISPLAY_DFB(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_DFB, GdkDisplayDFB))
#define GDK_DISPLAY_DFB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_DFB, GdkDisplayDFBClass))
#define GDK_IS_DISPLAY_DFB(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_DFB))
#define GDK_IS_DISPLAY_DFB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_DFB))
#define GDK_DISPLAY_DFB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_DFB, GdkDisplayDFBClass))

struct _GdkDisplayDFB
{
  GdkDisplay             parent;
  IDirectFB             *directfb;
  IDirectFBDisplayLayer *layer;
  IDirectFBEventBuffer  *buffer;
  IDirectFBInputDevice  *keyboard;
  GdkKeymap             *keymap;
};

struct _GdkDisplayDFBClass
{
  GdkDisplayClass parent;
};

GType      gdk_display_dfb_get_type            (void);

IDirectFBSurface *gdk_display_dfb_create_surface (GdkDisplayDFB *display,
                                                  int format,
                                                  int width, int height);

G_END_DECLS

#endif /* GDK_DISPLAY_DFB_H */
