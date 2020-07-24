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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_X11_VISUAL__
#define __GDK_X11_VISUAL__

G_BEGIN_DECLS

#include <gdk/gdk.h>
#include <gdk/x11/gdkx11screen.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_BEGIN_DECLS

#define GDK_TYPE_X11_VISUAL              (gdk_x11_visual_get_type ())
#define GDK_X11_VISUAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_X11_VISUAL, GdkX11Visual))
#define GDK_X11_VISUAL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_X11_VISUAL, GdkX11VisualClass))
#define GDK_IS_X11_VISUAL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_X11_VISUAL))
#define GDK_IS_X11_VISUAL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_X11_VISUAL))
#define GDK_X11_VISUAL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_X11_VISUAL, GdkX11VisualClass))

typedef struct _GdkX11Visual GdkX11Visual;
typedef struct _GdkX11VisualClass GdkX11VisualClass;

typedef enum
{
  GDK_VISUAL_STATIC_GRAY,
  GDK_VISUAL_GRAYSCALE,
  GDK_VISUAL_STATIC_COLOR,
  GDK_VISUAL_PSEUDO_COLOR,
  GDK_VISUAL_TRUE_COLOR,
  GDK_VISUAL_DIRECT_COLOR
} GdkVisualType;

typedef enum
{
  GDK_LSB_FIRST,
  GDK_MSB_FIRST
} GdkByteOrder;

struct _GdkX11Visual
{
  GObject parent_instance;

  GdkVisualType type;
  int depth;
  GdkByteOrder byte_order;
  int colormap_size;
  int bits_per_rgb;

  guint32 red_mask;
  guint32 green_mask;
  guint32 blue_mask;

  Visual *xvisual;
};

GType    gdk_x11_visual_get_type          (void);

Visual * gdk_x11_visual_get_xvisual       (GdkX11Visual *visual);

#define GDK_VISUAL_XVISUAL(visual)    (gdk_x11_visual_get_xvisual (visual))

GdkX11Visual* gdk_x11_screen_lookup_visual (GdkX11Screen *screen,
                                            VisualID      xvisualid);

G_END_DECLS

#endif
