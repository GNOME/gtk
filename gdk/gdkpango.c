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
#include <math.h>
#include "gdkcolor.h"
#include "gdkgc.h"
#include "gdkpango.h"
#include "gdkrgb.h"
#include "gdkprivate.h"
#include "gdkscreen.h"
#include "gdkalias.h"

/* This is for P_() ... a bit non-kosher, but works fine */
#include "gtk/gtkintl.h"

#define GDK_INFO_KEY "gdk-info"

/* We have various arrays indexed by render part; if PangoRenderPart
 * is extended, we want to make sure not to overwrite the end of
 * those arrays.
 */
#define MAX_RENDER_PART  PANGO_RENDER_PART_STRIKETHROUGH

struct _GdkPangoRendererPrivate
{
  GdkScreen *screen;

  /* GdkPangoRenderer specific state */
  PangoColor override_color[MAX_RENDER_PART + 1];
  gboolean override_color_set[MAX_RENDER_PART + 1];
  
  GdkBitmap *stipple[MAX_RENDER_PART + 1];
  gboolean embossed;

  /* When switching between the normal and shadow copies when
   * drawing shadows we can get unexpected recursion into the
   * drawing functions; the 'in_emboss' flag guards against that.
   */
  gboolean in_emboss;

  /* Current target */
  GdkDrawable *drawable;
  GdkGC *base_gc;

  /* Cached GC, derived from base_gc */
  GdkGC *gc;
  PangoColor gc_color;
  gboolean gc_color_set;
  GdkBitmap *gc_stipple;
  
  /* we accumulate trapezoids for the same PangoRenderPart */
  GArray *trapezoids;
  PangoRenderPart trapezoid_part;
};

static PangoAttrType gdk_pango_attr_stipple_type;
static PangoAttrType gdk_pango_attr_embossed_type;

static void flush_trapezoids (GdkPangoRenderer *gdk_renderer);

enum {
  PROP_0,
  PROP_SCREEN
};

G_DEFINE_TYPE (GdkPangoRenderer, gdk_pango_renderer, PANGO_TYPE_RENDERER)

static void
gdk_pango_renderer_finalize (GObject *object)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (object);
  GdkPangoRendererPrivate *priv = gdk_renderer->priv;
  int i;

  if (priv->gc)
    g_object_unref (priv->gc);
  if (priv->gc_stipple)
    g_object_unref (priv->gc_stipple);
  if (priv->base_gc)
    g_object_unref (priv->base_gc);
  if (priv->drawable)
    g_object_unref (priv->drawable);

  for (i = 0; i <= MAX_RENDER_PART; i++)
    if (priv->stipple[i])
      g_object_unref (priv->stipple[i]);

  g_array_free (priv->trapezoids, TRUE);

  G_OBJECT_CLASS (gdk_pango_renderer_parent_class)->finalize (object);
}

static GObject*
gdk_pango_renderer_constructor (GType                  type,
				guint                  n_construct_properties,
				GObjectConstructParam *construct_params)
{
  GObject *object;
  GdkPangoRenderer *gdk_renderer;

  object = (* G_OBJECT_CLASS (gdk_pango_renderer_parent_class)->constructor) (type,
									      n_construct_properties,
									      construct_params);

  gdk_renderer = GDK_PANGO_RENDERER (object);
  
  if (!gdk_renderer->priv->screen)
    {
      g_warning ("Screen must be specified at construct time for GdkPangoRenderer");
      gdk_renderer->priv->screen = gdk_screen_get_default ();
    }

  return object;
}

/* Adjusts matrix and color for the renderer to draw the secondar
 * "shadow" copy for embossed text */
static void
emboss_renderer (PangoRenderer   *renderer,
		 PangoRenderPart  part,
		 PangoMatrix    **save_matrix,
		 PangoColor     **save_color)
{
  GdkPangoRendererPrivate *priv = GDK_PANGO_RENDERER(renderer)->priv;
  static const PangoColor white = { 0xffff, 0xffff, 0xffff };
  PangoMatrix tmp_matrix = PANGO_MATRIX_INIT;

  priv->in_emboss = TRUE;
  
  *save_color = pango_renderer_get_color (renderer, part);
  if (*save_color)
    *save_color = pango_color_copy (*save_color);
  
  *save_matrix = renderer->matrix;
  if (*save_matrix)
    {
      *save_matrix = pango_matrix_copy (*save_matrix);
      tmp_matrix = **save_matrix;
    }
  
  /* The gymnastics here to adjust the matrix are because we want
   * to offset by +1,+1 in device-space, not in user-space,
   * so we can't just draw the layout at x + 1, y + 1
   */
  tmp_matrix.x0 += 1;
  tmp_matrix.y0 += 1;
  
  pango_renderer_set_matrix (renderer, &tmp_matrix);
  pango_renderer_set_color (renderer, part, &white);
}

/* Restores from emboss_renderer() */
static void
unemboss_renderer (PangoRenderer   *renderer,
		   PangoRenderPart  part,
		   PangoMatrix    **save_matrix,
		   PangoColor     **save_color)
{
  GdkPangoRendererPrivate *priv = GDK_PANGO_RENDERER(renderer)->priv;
  pango_renderer_set_matrix (renderer, *save_matrix);
  pango_renderer_set_color (renderer, part, *save_color);

  if (*save_matrix)
    pango_matrix_free (*save_matrix);
  if (*save_color)
    pango_color_free (*save_color);

  priv->in_emboss = FALSE;
}

/* Gets the GC for drawing @part. This make involve copying the base GC
 * for the renderer, in which case we keep a one-GC cache. */
static GdkGC *
get_gc (GdkPangoRenderer *gdk_renderer,
	PangoRenderPart   part)
{
  PangoRenderer *renderer = PANGO_RENDERER (gdk_renderer);
  PangoColor *color;
  GdkBitmap *stipple;
  GdkPangoRendererPrivate *priv = gdk_renderer->priv;

  color = pango_renderer_get_color (renderer, part);

  if (part <= MAX_RENDER_PART)
    stipple = priv->stipple[part];
  else
    stipple = NULL;

  if (!color && !stipple)	/* nothing override, use base_gc */
    return priv->base_gc;
  else
    {
      gboolean new_stipple = FALSE;
      gboolean new_color = FALSE;
      
      if (stipple != priv->gc_stipple)
	new_stipple = TRUE;

      if ((priv->gc_color_set && !color) ||
	  (!priv->gc_color_set && color) ||
	  priv->gc_color.red != color->red ||
	  priv->gc_color.green != color->green ||
	  priv->gc_color.blue != color->blue)
	new_color = TRUE;
      
      if (!priv->gc)
	{
	  priv->gc = gdk_gc_new (priv->drawable);
	  gdk_gc_copy (priv->gc, priv->base_gc);
	}
      else if (new_color && priv->gc_color_set && !color)
	{
	  /* We have to recopy the original GC onto the cached GC
	   * to get the default color */
	  new_stipple = TRUE;
	  gdk_gc_copy (priv->gc, priv->base_gc);
	}
      else if (new_stipple && priv->gc_stipple && !stipple)
	{
	  /* Similarly, we need to make a new copy to restore to the
	   * default stipple state (the caller may have set a stipple
	   * on the GC, and even if not, gdk_gc_set_stipple (gc, NULL)
	   * doesn't work currently to restore to the default X stipple) */
	  new_color = TRUE;
	  gdk_gc_copy (priv->gc, priv->base_gc);
	}

      if (new_color)
	{
	  if (color)
	    {
	      GdkColor gdk_color;

	      gdk_color.red = color->red;
	      gdk_color.green = color->green;
	      gdk_color.blue = color->blue;
	      
	      gdk_gc_set_rgb_fg_color (priv->gc, &gdk_color);

	      priv->gc_color = *color;
	      priv->gc_color_set = TRUE;
	    }
	  else
	    priv->gc_color_set = FALSE;
	}

      if (new_stipple)
	{
	  if (priv->gc_stipple)
	    g_object_unref (priv->gc_stipple);

	  if (stipple)
	    {
	      gdk_gc_set_stipple (priv->gc, stipple);
	      gdk_gc_set_fill (priv->gc, GDK_STIPPLED);
	      priv->gc_stipple = g_object_ref (stipple);
	    }
	  else
	    priv->gc_stipple = NULL;
	}

      return priv->gc;
    }
}

static void
gdk_pango_renderer_draw_glyphs (PangoRenderer    *renderer,
				PangoFont        *font,
				PangoGlyphString *glyphs,
				int               x,
				int               y)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  GdkPangoRendererPrivate *priv = gdk_renderer->priv;

  flush_trapezoids (gdk_renderer);

  if (!priv->in_emboss && priv->embossed)
    {
      PangoMatrix *save_matrix;
      PangoColor *save_color;

      emboss_renderer (renderer, PANGO_RENDER_PART_FOREGROUND, &save_matrix, &save_color);
      gdk_draw_glyphs_transformed (priv->drawable,
				   get_gc (gdk_renderer, PANGO_RENDER_PART_FOREGROUND),
				   renderer->matrix, font, x, y, glyphs);
      unemboss_renderer (renderer, PANGO_RENDER_PART_FOREGROUND, &save_matrix, &save_color);
    }

  gdk_draw_glyphs_transformed (priv->drawable,
			       get_gc (gdk_renderer, PANGO_RENDER_PART_FOREGROUND),
			       renderer->matrix, font, x, y, glyphs);
}

/* Outputs any pending trapezoids, we do this when the part or
 * part color changes, when we are about to draw text, etc. */
static void
flush_trapezoids (GdkPangoRenderer *gdk_renderer)
{
  GdkPangoRendererPrivate *priv = gdk_renderer->priv;

  if (!priv->trapezoids || priv->trapezoids->len == 0)
    return;

  gdk_draw_trapezoids (priv->drawable,
		       get_gc (gdk_renderer, priv->trapezoid_part),
		       (GdkTrapezoid *)priv->trapezoids->data,
		       priv->trapezoids->len);

  g_array_set_size (priv->trapezoids, 0);
}

/* Draws a single trapezoid ... we don't draw it immediately, but rather
 * cache it to join together with other trapezoids that form part of the
 * same logical shape */
static void
gdk_pango_renderer_draw_trapezoid (PangoRenderer   *renderer,
				   PangoRenderPart  part,
				   double           y1,
				   double           x11,
				   double           x21,
				   double           y2,
				   double           x12,
				   double           x22)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  GdkTrapezoid trap;

  if (!gdk_renderer->priv->trapezoids)
    gdk_renderer->priv->trapezoids = g_array_new (FALSE, FALSE,
						  sizeof (GdkTrapezoid));
  
  if (gdk_renderer->priv->trapezoids->len > 0 &&
      gdk_renderer->priv->trapezoid_part != part)
    flush_trapezoids (gdk_renderer);
  
  gdk_renderer->priv->trapezoid_part = part;

  trap.y1 = y1;
  trap.x11 = x11;
  trap.x21 = x21;
  trap.y2 = y2;
  trap.x12 = x12;
  trap.x22 = x22;

  g_array_append_val (gdk_renderer->priv->trapezoids, trap);
}

/* We can't handle embossing at the level of trapezoids, because when an
 * underline is split into multiple trapezoids, the normal and shadow
 * trapezoids will be drawn mixed together. Instead, we have to emboss
 * and entire rectangle or error underline
 */

static void
gdk_pango_renderer_draw_rectangle (PangoRenderer    *renderer,
				   PangoRenderPart   part,
				   int               x,
				   int               y,
				   int               width,
				   int               height)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  GdkPangoRendererPrivate *priv = gdk_renderer->priv;

  if (!priv->in_emboss && priv->embossed && part != PANGO_RENDER_PART_BACKGROUND)
    {
      PangoMatrix *save_matrix;
      PangoColor *save_color;

      emboss_renderer (renderer, part, &save_matrix, &save_color);
      PANGO_RENDERER_CLASS (gdk_pango_renderer_parent_class)->draw_rectangle (renderer, part,
									      x, y, width, height);
      unemboss_renderer (renderer, part, &save_matrix, &save_color);
    }

  PANGO_RENDERER_CLASS (gdk_pango_renderer_parent_class)->draw_rectangle (renderer, part,
									  x, y, width, height);
}

static void
gdk_pango_renderer_draw_error_underline (PangoRenderer    *renderer,
					 int               x,
					 int               y,
					 int               width,
					 int               height)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  GdkPangoRendererPrivate *priv = gdk_renderer->priv;

  if (!priv->in_emboss && priv->embossed)
    {
      PangoMatrix *save_matrix;
      PangoColor *save_color;

      emboss_renderer (renderer, PANGO_RENDER_PART_UNDERLINE, &save_matrix, &save_color);
      PANGO_RENDERER_CLASS (gdk_pango_renderer_parent_class)->draw_error_underline (renderer,
										    x, y, width, height);
      unemboss_renderer (renderer, PANGO_RENDER_PART_UNDERLINE, &save_matrix, &save_color);
    }

  PANGO_RENDERER_CLASS (gdk_pango_renderer_parent_class)->draw_error_underline (renderer,
										x, y, width, height);
}

static void
gdk_pango_renderer_part_changed (PangoRenderer   *renderer,
				 PangoRenderPart  part)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);

  if (part == gdk_renderer->priv->trapezoid_part)
    flush_trapezoids (gdk_renderer);
}

static void
gdk_pango_renderer_begin (PangoRenderer *renderer)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);

  if (!gdk_renderer->priv->drawable || !gdk_renderer->priv->base_gc)
    {
      g_warning ("gdk_pango_renderer_set_drawable() and gdk_pango_renderer_set_drawable()"
		 "must be used to set the target drawable and GC before using the renderer\n");
    }
}

static void
gdk_pango_renderer_end (PangoRenderer *renderer)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);

  flush_trapezoids (gdk_renderer);
}

static void
gdk_pango_renderer_prepare_run (PangoRenderer  *renderer,
				PangoLayoutRun *run)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  gboolean embossed = FALSE;
  GdkBitmap *stipple = NULL;
  GSList *l;
  int i;
  
  embossed = FALSE;
  stipple = NULL;
  
  for (l = run->item->analysis.extra_attrs; l; l = l->next)
    {
      PangoAttribute *attr = l->data;

      /* stipple_type and embossed_type aren't necessarily
       * initialized, but they are 0, which is an
       * invalid type so won't occur. 
       */
      if (attr->klass->type == gdk_pango_attr_stipple_type)
	{
	  stipple = ((GdkPangoAttrStipple*)attr)->stipple;
	}
      else if (attr->klass->type == gdk_pango_attr_embossed_type)
	{
	  embossed = ((GdkPangoAttrEmbossed*)attr)->embossed;
	}
    }

  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_FOREGROUND, stipple);
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_BACKGROUND, stipple);
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_UNDERLINE, stipple);
  gdk_pango_renderer_set_stipple (gdk_renderer, PANGO_RENDER_PART_STRIKETHROUGH, stipple);

  if (embossed != gdk_renderer->priv->embossed)
    {
      gdk_renderer->priv->embossed = embossed;
      pango_renderer_part_changed (renderer, PANGO_RENDER_PART_FOREGROUND);
    }

  PANGO_RENDERER_CLASS (gdk_pango_renderer_parent_class)->prepare_run (renderer, run);

  for (i = 0; i <= MAX_RENDER_PART; i++)
    {
      if (gdk_renderer->priv->override_color_set[i])
	pango_renderer_set_color (renderer, i, &gdk_renderer->priv->override_color[i]);
    }
}

static void
gdk_pango_renderer_set_property (GObject         *object,
				 guint            prop_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      gdk_renderer->priv->screen = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_pango_renderer_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, gdk_renderer->priv->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_pango_renderer_init (GdkPangoRenderer *renderer)
{
  renderer->priv = G_TYPE_INSTANCE_GET_PRIVATE (renderer,
						GDK_TYPE_PANGO_RENDERER,
						GdkPangoRendererPrivate);
}

static void
gdk_pango_renderer_class_init (GdkPangoRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);
  
  renderer_class->draw_glyphs = gdk_pango_renderer_draw_glyphs;
  renderer_class->draw_trapezoid = gdk_pango_renderer_draw_trapezoid;
  renderer_class->draw_rectangle = gdk_pango_renderer_draw_rectangle;
  renderer_class->draw_error_underline = gdk_pango_renderer_draw_error_underline;
  renderer_class->part_changed = gdk_pango_renderer_part_changed;
  renderer_class->begin = gdk_pango_renderer_begin;
  renderer_class->end = gdk_pango_renderer_end;
  renderer_class->prepare_run = gdk_pango_renderer_prepare_run;

  object_class->finalize = gdk_pango_renderer_finalize;
  object_class->constructor = gdk_pango_renderer_constructor;
  object_class->set_property = gdk_pango_renderer_set_property;
  object_class->get_property = gdk_pango_renderer_get_property;
  
  g_object_class_install_property (object_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        P_("Screen"),
                                                        P_("the GdkScreen for the renderer"),
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GdkPangoRendererPrivate));  
}

/**
 * gdk_pango_renderer_new:
 * @screen: a #GdkScreen
 * 
 * Creates a new #PangoRenderer for @screen. Normally you can use the
 * results of gdk_pango_renderer_get_default() rather than creating a new
 * renderer.
 * 
 * Return value: a newly created #PangoRenderer. Free with g_object_unref().
 *
 * Since: 2.6
 **/
PangoRenderer *
gdk_pango_renderer_new (GdkScreen *screen)
{
  g_return_val_if_fail (screen != NULL, NULL);
  
  return g_object_new (GDK_TYPE_PANGO_RENDERER,
		       "screen", screen,
		       NULL);
}

static void
on_renderer_display_closed (GdkDisplay       *display,
			    GdkPangoRenderer *renderer)
{
  g_signal_handlers_disconnect_by_func (renderer->priv->screen,
					(gpointer)on_renderer_display_closed,
					renderer);
  g_object_set_data (G_OBJECT (renderer->priv->screen), "gdk-pango-renderer", NULL);
}

/**
 * gdk_pango_renderer_get_default:
 * @screen: a #GdkScreen
 * 
 * Gets the default #PangoRenderer for a screen. This default renderer
 * is shared by all users of the display, so properties such as the color
 * or transformation matrix set for the renderer may be overwritten
 * by functions such as gdk_draw_layout().
 *
 * Before using the renderer, you need to call gdk_pango_renderer_set_drawable()
 * and gdk_pango_renderer_set_gc() to set the drawable and graphics context
 * to use for drawing.
 * 
 * Return value: the default #PangoRenderer for @screen. The
 *  renderer is owned by GTK+ and will be kept around until the
 *  screen is closed.
 *
 * Since: 2.6
 **/
PangoRenderer *
gdk_pango_renderer_get_default (GdkScreen *screen)
{
  PangoRenderer *renderer;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  
  renderer = g_object_get_data (G_OBJECT (screen), "gdk-pango-renderer");
  if (!renderer)
    {
      renderer = gdk_pango_renderer_new (screen);
      g_object_set_data_full (G_OBJECT (screen), "gdk-pango-renderer", renderer,
			      (GDestroyNotify)g_object_unref);

      g_signal_connect (gdk_screen_get_display (screen), "closed",
			G_CALLBACK (on_renderer_display_closed), renderer);
    }

  return renderer;
}

/**
 * gdk_pango_renderer_set_drawable:
 * @gdk_renderer: a #GdkPangoRenderer
 * @drawable: the new target drawable, or %NULL
 * 
 * Sets the drawable the renderer draws to.
 *
 * Since: 2.6
 **/
void
gdk_pango_renderer_set_drawable (GdkPangoRenderer *gdk_renderer,
				 GdkDrawable      *drawable)
{
  GdkPangoRendererPrivate *priv;
  
  g_return_if_fail (GDK_IS_PANGO_RENDERER (gdk_renderer));
  g_return_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable));

  priv = gdk_renderer->priv;
  
  flush_trapezoids (gdk_renderer);

  if (priv->drawable != drawable)
    {
      if (priv->drawable)
	g_object_unref (priv->drawable);
      priv->drawable = drawable;
      if (priv->drawable)
	g_object_ref (priv->drawable);
    }
}

/**
 * gdk_pango_renderer_set_gc:
 * @gdk_renderer: a #GdkPangoRenderer
 * @gc: the new GC to use for drawing, or %NULL
 * 
 * Sets the GC the renderer draws with. Note that the GC must not be
 * modified until it is unset by calling the function again with
 * %NULL for the @gc parameter, since GDK may make internal copies
 * of the GC which won't be updated to follow changes to the
 * original GC.
 *
 * Since: 2.6
 **/
void
gdk_pango_renderer_set_gc (GdkPangoRenderer *gdk_renderer,
			   GdkGC            *gc)
{
  GdkPangoRendererPrivate *priv;
  
  g_return_if_fail (GDK_IS_PANGO_RENDERER (gdk_renderer));
  g_return_if_fail (gc == NULL || GDK_IS_GC (gc));

  priv = gdk_renderer->priv;
  
  flush_trapezoids (gdk_renderer);

  if (priv->base_gc != gc)
    {
      if (priv->base_gc)
	g_object_unref (priv->base_gc);
      priv->base_gc = gc;
      if (priv->base_gc)
	g_object_ref (priv->base_gc);

      if (priv->gc)
	{
	  g_object_unref (priv->gc);
	  priv->gc = NULL;
	}
      
      priv->gc_color_set = FALSE;

      if (priv->gc_stipple)
	{
	  g_object_unref (priv->gc_stipple);
	  priv->gc_stipple = NULL;
	}
    }
}


/**
 * gdk_pango_renderer_set_stipple:
 * @gdk_renderer: a #GdkPangoRenderer
 * @part: the part to render with the stipple
 * @stipple: the new stipple value.
 * 
 * Sets the stipple for one render part (foreground, background, underline,
 * etc.) Note that this is overwritten when iterating through the individual
 * styled runs of a #PangoLayout or #PangoLayoutLine. This function is thus
 * only useful when you call low level functions like pango_renderer_draw_glyphs()
 * directly, or in the 'prepare_run' virtual function of a subclass of
 * #GdkPangoRenderer.
 *
 * Since: 2.6
 **/
void
gdk_pango_renderer_set_stipple (GdkPangoRenderer *gdk_renderer,
				PangoRenderPart   part,
				GdkBitmap        *stipple)
{
  g_return_if_fail (GDK_IS_PANGO_RENDERER (gdk_renderer));

  if (part > MAX_RENDER_PART)	/* Silently ignore unknown parts */
    return;

  if (stipple != gdk_renderer->priv->stipple[part])
    {
      if (gdk_renderer->priv->stipple[part])
	g_object_unref (gdk_renderer->priv->stipple[part]);

      gdk_renderer->priv->stipple[part] = stipple;
      
      if (gdk_renderer->priv->stipple[part])
	g_object_ref (gdk_renderer->priv->stipple[part]);

      pango_renderer_part_changed (PANGO_RENDERER (gdk_renderer), part);
    }
}

/**
 * gdk_pango_renderer_set_override_color:
 * @gdk_renderer: a #GdkPangoRenderer
 * @part: the part to render to set the color of
 * @color: the color to use, or %NULL to unset a previously
 *         set override color.
 * 
 * Sets the color for a particular render part (foreground,
 * background, underline, etc.), overriding any attributes on the layouts
 * renderered with this renderer.
 * 
 * Since: 2.6
 **/
void
gdk_pango_renderer_set_override_color (GdkPangoRenderer *gdk_renderer,
				       PangoRenderPart   part,
				       const GdkColor   *color)
{
  GdkPangoRendererPrivate *priv;
  
  g_return_if_fail (GDK_IS_PANGO_RENDERER (gdk_renderer));

  priv = gdk_renderer->priv;
  
  if (part > MAX_RENDER_PART)	/* Silently ignore unknown parts */
    return;

  if (color)
    {
      priv->override_color[part].red = color->red;
      priv->override_color[part].green = color->green;
      priv->override_color[part].blue = color->blue;
      priv->override_color_set[part] = TRUE;
    }
  else
    priv->override_color_set[part] = FALSE;
}

/**
 * gdk_pango_context_set_colormap:
 * @context: a #PangoContext
 * @colormap: a #GdkColormap
 *
 * This function used to set the colormap to be used for drawing with
 * @context. The colormap is now always derived from the graphics
 * context used for drawing, so calling this function is no longer
 * necessary.
 **/
void
gdk_pango_context_set_colormap (PangoContext *context,
				GdkColormap  *colormap)
{
  g_return_if_fail (PANGO_IS_CONTEXT (context));
  g_return_if_fail (colormap == NULL || GDK_IS_COLORMAP (colormap));
}

/* Gets a renderer to draw with, setting the properties of the
 * renderer and activating it. Note that since we activate the
 * renderer here, the implicit setting of the matrix that
 * pango_renderer_draw_layout_[line] normally do when they
 * activate the renderer is suppressed. */
static PangoRenderer *
get_renderer (GdkDrawable     *drawable,
	      GdkGC           *gc,
	      const GdkColor  *foreground,
	      const GdkColor  *background)
{
  GdkScreen *screen = gdk_drawable_get_screen (drawable);
  PangoRenderer *renderer = gdk_pango_renderer_get_default (screen);
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);

  gdk_pango_renderer_set_drawable (gdk_renderer, drawable);
  gdk_pango_renderer_set_gc (gdk_renderer, gc);  

  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_FOREGROUND,
					 foreground);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_UNDERLINE,
					 foreground);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_STRIKETHROUGH,
					 foreground);

  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_BACKGROUND,
					 background);

  pango_renderer_activate (renderer);

  return renderer;
}

/* Cleans up the renderer obtained with get_renderer() */
static void
release_renderer (PangoRenderer *renderer)
{
  GdkPangoRenderer *gdk_renderer = GDK_PANGO_RENDERER (renderer);
  
  pango_renderer_deactivate (renderer);
  
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_FOREGROUND,
					 NULL);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_UNDERLINE,
					 NULL);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_STRIKETHROUGH,
					 NULL);
  gdk_pango_renderer_set_override_color (gdk_renderer,
					 PANGO_RENDER_PART_BACKGROUND,
					 NULL);
  
  gdk_pango_renderer_set_drawable (gdk_renderer, NULL);
  gdk_pango_renderer_set_gc (gdk_renderer, NULL);
}

/**
 * gdk_draw_layout_line_with_colors:
 * @drawable:  the drawable on which to draw the line
 * @gc:        base graphics to use
 * @x:         the x position of start of string (in pixels)
 * @y:         the y position of baseline (in pixels)
 * @line:      a #PangoLayoutLine
 * @foreground: foreground override color, or %NULL for none
 * @background: background override color, or %NULL for none
 *
 * Render a #PangoLayoutLine onto a #GdkDrawable, overriding the
 * layout's normal colors with @foreground and/or @background.
 * @foreground and @background need not be allocated.
 *
 * If the layout's #PangoContext has a transformation matrix set, then
 * @x and @y specify the position of the left edge of the baseline
 * (left is in before-tranform user coordinates) in after-transform
 * device coordinates.
 */
void 
gdk_draw_layout_line_with_colors (GdkDrawable      *drawable,
                                  GdkGC            *gc,
                                  gint              x, 
                                  gint              y,
                                  PangoLayoutLine  *line,
                                  const GdkColor   *foreground,
                                  const GdkColor   *background)
{
  PangoRenderer *renderer;
  const PangoMatrix *matrix;
  
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (line != NULL);

  renderer = get_renderer (drawable, gc, foreground, background);

  /* When we have a matrix, we do positioning by adjusting the matrix, and
   * clamp just pass x=0, y=0 to the lower levels. We don't want to introduce
   * a matrix when the caller didn't provide one, however, since that adds
   * lots of floating point arithmetic for each glyph.
   */
  matrix = pango_context_get_matrix (pango_layout_get_context (line->layout));
  if (matrix)
    {
      PangoMatrix tmp_matrix;
      
      tmp_matrix = *matrix;
      tmp_matrix.x0 = x;
      tmp_matrix.y0 = y;
      pango_renderer_set_matrix (renderer, &tmp_matrix);

      x = 0;
      y = 0;
    }
  else
    pango_renderer_set_matrix (renderer, NULL);

  pango_renderer_draw_layout_line (renderer, line, x * PANGO_SCALE, y * PANGO_SCALE);

  release_renderer (renderer);
}

/* Gets the bounds of a layout in device coordinates. Note cut-and-paste
 * between here and gtklabel.c */
static void
get_rotated_layout_bounds (PangoLayout  *layout,
			   GdkRectangle *rect)
{
  PangoContext *context = pango_layout_get_context (layout);
  const PangoMatrix *matrix = pango_context_get_matrix (context);
  gdouble x_min = 0, x_max = 0, y_min = 0, y_max = 0; /* quiet gcc */
  PangoRectangle logical_rect;
  gint i, j;

  pango_layout_get_extents (layout, NULL, &logical_rect);
  
  for (i = 0; i < 2; i++)
    {
      gdouble x = (i == 0) ? logical_rect.x : logical_rect.x + logical_rect.width;
      for (j = 0; j < 2; j++)
	{
	  gdouble y = (j == 0) ? logical_rect.y : logical_rect.y + logical_rect.height;
	  
	  gdouble xt = (x * matrix->xx + y * matrix->xy) / PANGO_SCALE + matrix->x0;
	  gdouble yt = (x * matrix->yx + y * matrix->yy) / PANGO_SCALE + matrix->y0;
	  
	  if (i == 0 && j == 0)
	    {
	      x_min = x_max = xt;
	      y_min = y_max = yt;
	    }
	  else
	    {
	      if (xt < x_min)
		x_min = xt;
	      if (yt < y_min)
		y_min = yt;
	      if (xt > x_max)
		x_max = xt;
	      if (yt > y_max)
		y_max = yt;
	    }
	}
    }
  
  rect->x = floor (x_min);
  rect->width = ceil (x_max) - rect->x;
  rect->y = floor (y_min);
  rect->height = floor (y_max) - rect->y;
}

/**
 * gdk_draw_layout_with_colors:
 * @drawable:  the drawable on which to draw string
 * @gc:        base graphics context to use
 * @x:         the X position of the left of the layout (in pixels)
 * @y:         the Y position of the top of the layout (in pixels)
 * @layout:    a #PangoLayout
 * @foreground: foreground override color, or %NULL for none
 * @background: background override color, or %NULL for none
 *
 * Render a #PangoLayout onto a #GdkDrawable, overriding the
 * layout's normal colors with @foreground and/or @background.
 * @foreground and @background need not be allocated.
 *
 * If the layout's #PangoContext has a transformation matrix set, then
 * @x and @y specify the position of the top left corner of the
 * bounding box (in device space) of the transformed layout.
 *
 * If you're using GTK+, the ususal way to obtain a #PangoLayout
 * is gtk_widget_create_pango_layout().
 */
void 
gdk_draw_layout_with_colors (GdkDrawable     *drawable,
                             GdkGC           *gc,
                             int              x, 
                             int              y,
                             PangoLayout     *layout,
                             const GdkColor  *foreground,
                             const GdkColor  *background)
{
  PangoRenderer *renderer;
  const PangoMatrix *matrix;
  
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  renderer = get_renderer (drawable, gc, foreground, background);

  /* When we have a matrix, we do positioning by adjusting the matrix, and
   * clamp just pass x=0, y=0 to the lower levels. We don't want to introduce
   * a matrix when the caller didn't provide one, however, since that adds
   * lots of floating point arithmetic for each glyph.
   */
  matrix = pango_context_get_matrix (pango_layout_get_context (layout));
  if (matrix)
    {
      PangoMatrix tmp_matrix;
      GdkRectangle rect;

      get_rotated_layout_bounds (layout, &rect);
      
      tmp_matrix = *matrix;
      tmp_matrix.x0 += x - rect.x;
      tmp_matrix.y0 += y - rect.y;
      pango_renderer_set_matrix (renderer, &tmp_matrix);
      
      x = 0;
      y = 0;
    }
  else
    pango_renderer_set_matrix (renderer, NULL);
  
  pango_renderer_draw_layout (renderer, layout, x * PANGO_SCALE, y * PANGO_SCALE);
  
  release_renderer (renderer);
}

/**
 * gdk_draw_layout_line:
 * @drawable:  the drawable on which to draw the line
 * @gc:        base graphics to use
 * @x:         the x position of start of string (in pixels)
 * @y:         the y position of baseline (in pixels)
 * @line:      a #PangoLayoutLine
 *
 * Render a #PangoLayoutLine onto an GDK drawable
 *
 * If the layout's #PangoContext has a transformation matrix set, then
 * @x and @y specify the position of the left edge of the baseline
 * (left is in before-tranform user coordinates) in after-transform
 * device coordinates.
 */
void 
gdk_draw_layout_line (GdkDrawable      *drawable,
		      GdkGC            *gc,
		      gint              x, 
		      gint              y,
		      PangoLayoutLine  *line)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (line != NULL);
  
  gdk_draw_layout_line_with_colors (drawable, gc, x, y, line, NULL, NULL);
}

/**
 * gdk_draw_layout:
 * @drawable:  the drawable on which to draw string
 * @gc:        base graphics context to use
 * @x:         the X position of the left of the layout (in pixels)
 * @y:         the Y position of the top of the layout (in pixels)
 * @layout:    a #PangoLayout
 *
 * Render a #PangoLayout onto a GDK drawable
 *
 * If the layout's #PangoContext has a transformation matrix set, then
 * @x and @y specify the position of the top left corner of the
 * bounding box (in device space) of the transformed layout.
 *
 * If you're using GTK+, the usual way to obtain a #PangoLayout
 * is gtk_widget_create_pango_layout().
 */
void 
gdk_draw_layout (GdkDrawable     *drawable,
		 GdkGC           *gc,
		 int              x, 
		 int              y,
		 PangoLayout     *layout)
{
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  gdk_draw_layout_with_colors (drawable, gc, x, y, layout, NULL, NULL);
}

static PangoAttribute *
gdk_pango_attr_stipple_copy (const PangoAttribute *attr)
{
  const GdkPangoAttrStipple *src = (const GdkPangoAttrStipple*) attr;

  return gdk_pango_attr_stipple_new (src->stipple);
}

static void
gdk_pango_attr_stipple_destroy (PangoAttribute *attr)
{
  GdkPangoAttrStipple *st = (GdkPangoAttrStipple*) attr;

  if (st->stipple)
    g_object_unref (st->stipple);
  
  g_free (attr);
}

static gboolean
gdk_pango_attr_stipple_compare (const PangoAttribute *attr1,
                                    const PangoAttribute *attr2)
{
  const GdkPangoAttrStipple *a = (const GdkPangoAttrStipple*) attr1;
  const GdkPangoAttrStipple *b = (const GdkPangoAttrStipple*) attr2;

  return a->stipple == b->stipple;
}

/**
 * gdk_pango_attr_stipple_new:
 * @stipple: a bitmap to be set as stipple
 *
 * Creates a new attribute containing a stipple bitmap to be used when
 * rendering the text.
 *
 * Return value: new #PangoAttribute
 **/

PangoAttribute *
gdk_pango_attr_stipple_new (GdkBitmap *stipple)
{
  GdkPangoAttrStipple *result;
  
  static PangoAttrClass klass = {
    0,
    gdk_pango_attr_stipple_copy,
    gdk_pango_attr_stipple_destroy,
    gdk_pango_attr_stipple_compare
  };

  if (!klass.type)
    klass.type = gdk_pango_attr_stipple_type =
      pango_attr_type_register ("GdkPangoAttrStipple");

  result = g_new (GdkPangoAttrStipple, 1);
  result->attr.klass = &klass;

  if (stipple)
    g_object_ref (stipple);
  
  result->stipple = stipple;

  return (PangoAttribute *)result;
}

static PangoAttribute *
gdk_pango_attr_embossed_copy (const PangoAttribute *attr)
{
  const GdkPangoAttrEmbossed *e = (const GdkPangoAttrEmbossed*) attr;

  return gdk_pango_attr_embossed_new (e->embossed);
}

static void
gdk_pango_attr_embossed_destroy (PangoAttribute *attr)
{
  g_free (attr);
}

static gboolean
gdk_pango_attr_embossed_compare (const PangoAttribute *attr1,
                                 const PangoAttribute *attr2)
{
  const GdkPangoAttrEmbossed *e1 = (const GdkPangoAttrEmbossed*) attr1;
  const GdkPangoAttrEmbossed *e2 = (const GdkPangoAttrEmbossed*) attr2;

  return e1->embossed == e2->embossed;
}

/**
 * gdk_pango_attr_embossed_new:
 * @embossed: a bitmap to be set as embossed
 *
 * Creates a new attribute containing a embossed bitmap to be used when
 * rendering the text.
 *
 * Return value: new #PangoAttribute
 **/

PangoAttribute *
gdk_pango_attr_embossed_new (gboolean embossed)
{
  GdkPangoAttrEmbossed *result;
  
  static PangoAttrClass klass = {
    0,
    gdk_pango_attr_embossed_copy,
    gdk_pango_attr_embossed_destroy,
    gdk_pango_attr_embossed_compare
  };

  if (!klass.type)
    klass.type = gdk_pango_attr_embossed_type =
      pango_attr_type_register ("GdkPangoAttrEmbossed");

  result = g_new (GdkPangoAttrEmbossed, 1);
  result->attr.klass = &klass;
  result->embossed = embossed;
  
  return (PangoAttribute *)result;
}

/* Get a clip region to draw only part of a layout. index_ranges
 * contains alternating range starts/stops. The region is the
 * region which contains the given ranges, i.e. if you draw with the
 * region as clip, only the given ranges are drawn.
 */

/**
 * gdk_pango_layout_line_get_clip_region:
 * @line: a #PangoLayoutLine 
 * @x_origin: X pixel where you intend to draw the layout line with this clip
 * @y_origin: baseline pixel where you intend to draw the layout line with this clip
 * @index_ranges: array of byte indexes into the layout, where even members of array are start indexes and odd elements are end indexes
 * @n_ranges: number of ranges in @index_ranges, i.e. half the size of @index_ranges
 * 
 * Obtains a clip region which contains the areas where the given
 * ranges of text would be drawn. @x_origin and @y_origin are the same
 * position you would pass to gdk_draw_layout_line(). @index_ranges
 * should contain ranges of bytes in the layout's text. The clip
 * region will include space to the left or right of the line (to the
 * layout bounding box) if you have indexes above or below the indexes
 * contained inside the line. This is to draw the selection all the way
 * to the side of the layout. However, the clip region is in line coordinates,
 * not layout coordinates.
 * 
 * Return value: a clip region containing the given ranges
 **/
GdkRegion*
gdk_pango_layout_line_get_clip_region (PangoLayoutLine *line,
                                       gint             x_origin,
                                       gint             y_origin,
                                       gint            *index_ranges,
                                       gint             n_ranges)
{
  GdkRegion *clip_region;
  gint i;
  PangoRectangle logical_rect;
  PangoLayoutIter *iter;
  gint baseline;
  
  g_return_val_if_fail (line != NULL, NULL);
  g_return_val_if_fail (index_ranges != NULL, NULL);
  
  clip_region = gdk_region_new ();

  iter = pango_layout_get_iter (line->layout);
  while (pango_layout_iter_get_line (iter) != line)
    pango_layout_iter_next_line (iter);
  
  pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
  baseline = pango_layout_iter_get_baseline (iter);
  
  pango_layout_iter_free (iter);
  
  i = 0;
  while (i < n_ranges)
    {  
      gint *pixel_ranges = NULL;
      gint n_pixel_ranges = 0;
      gint j;

      /* Note that get_x_ranges returns layout coordinates
       */
      if (index_ranges[i*2+1] >= line->start_index &&
	  index_ranges[i*2] < line->start_index + line->length)
	pango_layout_line_get_x_ranges (line,
					index_ranges[i*2],
					index_ranges[i*2+1],
					&pixel_ranges, &n_pixel_ranges);
      
      for (j = 0; j < n_pixel_ranges; j++)
        {
          GdkRectangle rect;
          
          rect.x = x_origin + pixel_ranges[2*j] / PANGO_SCALE - logical_rect.x / PANGO_SCALE;
          rect.y = y_origin - (baseline / PANGO_SCALE - logical_rect.y / PANGO_SCALE);
          rect.width = (pixel_ranges[2*j + 1] - pixel_ranges[2*j]) / PANGO_SCALE;
          rect.height = logical_rect.height / PANGO_SCALE;
          
          gdk_region_union_with_rect (clip_region, &rect);
        }

      g_free (pixel_ranges);
      ++i;
    }

  return clip_region;
}

/**
 * gdk_pango_layout_get_clip_region:
 * @layout: a #PangoLayout 
 * @x_origin: X pixel where you intend to draw the layout with this clip
 * @y_origin: Y pixel where you intend to draw the layout with this clip
 * @index_ranges: array of byte indexes into the layout, where even members of array are start indexes and odd elements are end indexes
 * @n_ranges: number of ranges in @index_ranges, i.e. half the size of @index_ranges
 * 
 * Obtains a clip region which contains the areas where the given ranges
 * of text would be drawn. @x_origin and @y_origin are the same position
 * you would pass to gdk_draw_layout_line(). @index_ranges should contain
 * ranges of bytes in the layout's text.
 * 
 * Return value: a clip region containing the given ranges
 **/
GdkRegion*
gdk_pango_layout_get_clip_region (PangoLayout *layout,
                                  gint         x_origin,
                                  gint         y_origin,
                                  gint        *index_ranges,
                                  gint         n_ranges)
{
  PangoLayoutIter *iter;  
  GdkRegion *clip_region;
  
  g_return_val_if_fail (PANGO_IS_LAYOUT (layout), NULL);
  g_return_val_if_fail (index_ranges != NULL, NULL);
  
  clip_region = gdk_region_new ();
  
  iter = pango_layout_get_iter (layout);
  
  do
    {
      PangoRectangle logical_rect;
      PangoLayoutLine *line;
      GdkRegion *line_region;
      gint baseline;
      
      line = pango_layout_iter_get_line (iter);      

      pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline (iter);      

      line_region = gdk_pango_layout_line_get_clip_region (line,
                                                           x_origin + logical_rect.x / PANGO_SCALE,
                                                           y_origin + baseline / PANGO_SCALE,
                                                           index_ranges,
                                                           n_ranges);

      gdk_region_union (clip_region, line_region);
      gdk_region_destroy (line_region);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);

  return clip_region;
}

/**
 * gdk_pango_context_get:
 * 
 * Creates a #PangoContext for the default GDK screen.
 *
 * The context must be freed when you're finished with it.
 * 
 * When using GTK+, normally you should use gtk_widget_get_pango_context()
 * instead of this function, to get the appropriate context for
 * the widget you intend to render text onto.
 * 
 * Return value: a new #PangoContext for the default display
 **/
PangoContext *
gdk_pango_context_get (void)
{
  return gdk_pango_context_get_for_screen (gdk_screen_get_default ());
}

#define __GDK_PANGO_C__
#include "gdkaliasdef.c"
