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
#include <pango/pangoxft.h>
#include <pango/pangoxft-render.h>
#include "gdkalias.h"

#include <math.h>

typedef struct _GdkX11Renderer      GdkX11Renderer;
typedef struct _GdkX11RendererClass GdkX11RendererClass;

#define GDK_TYPE_X11_RENDERER            (_gdk_x11_renderer_get_type())
#define GDK_X11_RENDERER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_X11_RENDERER, GdkX11Renderer))
#define GDK_IS_X11_RENDERER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_X11_RENDERER))
#define GDK_X11_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_X11_RENDERER, GdkX11RendererClass))
#define GDK_IS_X11_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_X11_RENDERER))
#define GDK_X11_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_X11_RENDERER, GdkX11RendererClass))

#define MAX_RENDER_PART  PANGO_RENDER_PART_STRIKETHROUGH

struct _GdkX11Renderer
{
  PangoXftRenderer parent_instance;

  XRenderPictFormat *mask_format;
  
  GdkDrawable *drawable;
  GdkGC *gc;
};

struct _GdkX11RendererClass
{
  PangoXftRendererClass parent_class;
};

G_DEFINE_TYPE (GdkX11Renderer, _gdk_x11_renderer, PANGO_TYPE_XFT_RENDERER)

static void
gdk_x11_renderer_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gdk_x11_renderer_parent_class)->finalize (object);
}
     
static void
gdk_x11_renderer_composite_trapezoids (PangoXftRenderer *xftrenderer,
				       PangoRenderPart   part,
				       XTrapezoid       *trapezoids,
				       int               n_trapezoids)
{
  /* Because we only use this renderer for "draw_glyphs()" calls, we
   * won't hit this code path much. However, it is hit for drawing
   * the "unknown glyph" hex squares. We can safely ignore the part,
   */
  GdkX11Renderer *x11_renderer = GDK_X11_RENDERER (xftrenderer);

  _gdk_x11_drawable_draw_xtrapezoids (x11_renderer->drawable,
				      x11_renderer->gc,
				      trapezoids, n_trapezoids);

}

static void
gdk_x11_renderer_composite_glyphs (PangoXftRenderer *xftrenderer,
				   XftFont          *xft_font,
				   XftGlyphSpec     *glyphs,
				   gint              n_glyphs)
{
  GdkX11Renderer *x11_renderer = GDK_X11_RENDERER (xftrenderer);

  _gdk_x11_drawable_draw_xft_glyphs (x11_renderer->drawable,
				     x11_renderer->gc,
				     xft_font, glyphs, n_glyphs);
}

static void
_gdk_x11_renderer_init (GdkX11Renderer *renderer)
{
}

static void
_gdk_x11_renderer_class_init (GdkX11RendererClass *klass)
{
  PangoXftRendererClass *xftrenderer_class = PANGO_XFT_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  xftrenderer_class->composite_glyphs = gdk_x11_renderer_composite_glyphs;
  xftrenderer_class->composite_trapezoids = gdk_x11_renderer_composite_trapezoids;

  object_class->finalize = gdk_x11_renderer_finalize;
}

PangoRenderer *
_gdk_x11_renderer_get (GdkDrawable *drawable,
		       GdkGC       *gc)
{
  GdkScreen *screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  GdkX11Renderer *x11_renderer;

  if (!screen_x11->renderer)
    {
      screen_x11->renderer = g_object_new (GDK_TYPE_X11_RENDERER,
					   "display", GDK_SCREEN_XDISPLAY (screen),
					   "screen",  GDK_SCREEN_XNUMBER (screen),
					   NULL);
    }

  x11_renderer = GDK_X11_RENDERER (screen_x11->renderer);

  x11_renderer->drawable = drawable;
  x11_renderer->gc = gc;

  return screen_x11->renderer;
}

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
 *
 * Since: 2.2
 **/
PangoContext *
gdk_pango_context_get_for_screen (GdkScreen *screen)
{
  PangoContext *context;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (screen->closed)
    return NULL;
  
  context = pango_xft_get_context (GDK_SCREEN_XDISPLAY (screen),
				   GDK_SCREEN_X11 (screen)->screen_num);
  
  g_object_set_data (G_OBJECT (context), "gdk-pango-screen", screen);
  
  return context;
}

#define __GDK_PANGO_X11_C__
#include "gdkaliasdef.c"
