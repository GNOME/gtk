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

#include "gdkcolor.h"
#include "gdkgc.h"
#include "gdkpango.h"
#include "gdkrgb.h"
#include "gdkprivate.h"

#define GDK_INFO_KEY "gdk-info"

typedef struct _GdkPangoContextInfo GdkPangoContextInfo;

struct _GdkPangoContextInfo
{
  GdkColormap *colormap;
};

static PangoAttrType gdk_pango_attr_stipple_type;
static PangoAttrType gdk_pango_attr_embossed_type;

static void gdk_pango_get_item_properties (PangoItem      *item,
					   PangoUnderline *uline,
					   gboolean       *strikethrough,
                                           gint           *rise,
					   PangoColor     *fg_color,
					   gboolean       *fg_set,
					   PangoColor     *bg_color,
					   gboolean       *bg_set,
                                           gboolean       *embossed,
                                           GdkBitmap     **stipple,
					   gboolean       *shape_set,
					   PangoRectangle *ink_rect,
					   PangoRectangle *logical_rect);

static void
gdk_pango_context_destroy (GdkPangoContextInfo *info)
{
  if (info->colormap)
    gdk_colormap_unref (info->colormap);
  g_free (info);
}

static GdkPangoContextInfo *
gdk_pango_context_get_info (PangoContext *context, gboolean create)
{
  GdkPangoContextInfo *info =
    g_object_get_qdata (G_OBJECT (context),
                        g_quark_try_string (GDK_INFO_KEY));
  if (!info && create)
    {
      info = g_new (GdkPangoContextInfo, 1);
      info->colormap = NULL;

      g_object_set_qdata_full (G_OBJECT (context),
                               g_quark_from_static_string (GDK_INFO_KEY),
                               info, (GDestroyNotify)gdk_pango_context_destroy);
    }

  return info;
}

static GdkGC *
gdk_pango_get_gc (GdkDrawable    *drawable,
		  PangoContext   *context,
		  PangoColor     *fg_color,
                  GdkBitmap      *stipple,
		  GdkGC          *base_gc)
{
  GdkGC *result;
  GdkPangoContextInfo *info;
  
  g_return_val_if_fail (context != NULL, NULL);

  info = gdk_pango_context_get_info (context, FALSE);

  if (info == NULL || info->colormap == NULL)
    {
      g_warning ("you must set the colormap on a PangoContext before using it to draw a layout");
      return NULL;
    }

  result = gdk_gc_new (drawable);
  gdk_gc_copy (result, base_gc);
  
  if (fg_color)
    {
      GdkColor color;
      
      color.red = fg_color->red;
      color.green = fg_color->green;
      color.blue = fg_color->blue;

      gdk_rgb_find_color (info->colormap, &color);
      gdk_gc_set_foreground (result, &color);
    }

  if (stipple)
    {
      gdk_gc_set_fill (result, GDK_STIPPLED);
      gdk_gc_set_stipple (result, stipple);
    }
  
  return result;
}

static void
gdk_pango_free_gc (PangoContext *context,
		   GdkGC        *gc)
{
  gdk_gc_unref (gc);
}

/**
 * gdk_pango_context_set_colormap:
 * @context: a #PangoContext
 * @colormap: a #GdkColormap
 *
 * Sets the colormap to be used for drawing with @context.
 * If you obtained your context from gtk_widget_get_pango_context() or
 * gtk_widget_create_pango_context(), the colormap will already be set
 * to the colormap for the widget, so you shouldn't need this
 * function.
 * 
 **/
void
gdk_pango_context_set_colormap (PangoContext *context,
				GdkColormap  *colormap)
{
  GdkPangoContextInfo *info;
  
  g_return_if_fail (context != NULL);

  info = gdk_pango_context_get_info (context, TRUE);
  g_return_if_fail (info != NULL);
  
  if (info->colormap != colormap)
    {
      if (info->colormap)
	gdk_colormap_unref (info->colormap);

      info->colormap = colormap;
      
      if (info->colormap)
   	gdk_colormap_ref (info->colormap);
    }
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
 */
void 
gdk_draw_layout_line_with_colors (GdkDrawable      *drawable,
                                  GdkGC            *gc,
                                  gint              x, 
                                  gint              y,
                                  PangoLayoutLine  *line,
                                  GdkColor         *foreground,
                                  GdkColor         *background)
{
  GSList *tmp_list = line->runs;
  PangoRectangle overall_rect;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  PangoContext *context;
  gint x_off = 0;
  gint rise = 0;
  gboolean embossed;
  GdkBitmap *stipple;

  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (line != NULL);

  context = pango_layout_get_context (line->layout);
  
  pango_layout_line_get_extents (line,NULL, &overall_rect);
  
  while (tmp_list)
    {
      PangoUnderline uline = PANGO_UNDERLINE_NONE;
      PangoLayoutRun *run = tmp_list->data;
      PangoColor fg_color, bg_color;
      gboolean strike, fg_set, bg_set, shape_set;
      GdkGC *fg_gc;
      gint risen_y;
      
      tmp_list = tmp_list->next;
      
      gdk_pango_get_item_properties (run->item, &uline,
				     &strike,
                                     &rise,
                                     &fg_color, &fg_set,
                                     &bg_color, &bg_set,
                                     &embossed,
                                     &stipple,
                                     &shape_set,
                                     &ink_rect,
				     &logical_rect);

      /* we subtract the rise because X coordinates are upside down */
      risen_y = y - rise / PANGO_SCALE;
      
      if (!shape_set)
	{
	  if (uline == PANGO_UNDERLINE_NONE)
	    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					NULL, &logical_rect);
	  else
	    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
					&ink_rect, &logical_rect);
	}

      if (bg_set || background)
	{
	  GdkGC *bg_gc;
          PangoColor tmp = bg_color;
          
          if (background)
            {
              tmp.red = background->red;
              tmp.blue = background->blue;
              tmp.green = background->green;
            }
          
          bg_gc = gdk_pango_get_gc (drawable, context, &tmp, stipple, gc);
          
	  gdk_draw_rectangle (drawable, bg_gc, TRUE,
			      x + (x_off + logical_rect.x) / PANGO_SCALE,
			      risen_y + overall_rect.y / PANGO_SCALE,
			      logical_rect.width / PANGO_SCALE,
			      overall_rect.height / PANGO_SCALE);

          if (stipple)
            gdk_gc_set_fill (bg_gc, GDK_SOLID);
          
	  gdk_pango_free_gc (context, bg_gc);
	}

      if (fg_set || stipple || foreground)
        {
          PangoColor tmp = fg_color;
          
          if (foreground)
            {
              tmp.red = foreground->red;
              tmp.blue = foreground->blue;
              tmp.green = foreground->green;
            }
          
          fg_gc = gdk_pango_get_gc (drawable, context, fg_set ? &tmp : NULL,
                                    stipple, gc);
        }
      else
	fg_gc = gc;
      
      if (!shape_set)
        {
          gint gx, gy;

          gx = x + x_off / PANGO_SCALE;
          gy = risen_y;
          
          if (embossed)
            {
              PangoColor color = { 65535, 65535, 65535 };
              GdkGC *white_gc = gdk_pango_get_gc (drawable, context, &color, stipple, fg_gc);
              gdk_draw_glyphs (drawable, white_gc, run->item->analysis.font,
                               gx + 1,
                               gy + 1,
                               run->glyphs);
              gdk_pango_free_gc (context, white_gc);
            }
          
          gdk_draw_glyphs (drawable, fg_gc, run->item->analysis.font,
                           gx, gy,
                           run->glyphs);
        }
      
      switch (uline)
	{
	case PANGO_UNDERLINE_NONE:
	  break;
	case PANGO_UNDERLINE_DOUBLE:
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1,
                         risen_y + 3,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE,
                         risen_y + 3);
	  /* Fall through */
	case PANGO_UNDERLINE_SINGLE:
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1,
                         risen_y + 1,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE,
                         risen_y + 1);
	  break;
	case PANGO_UNDERLINE_LOW:
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1,
                         risen_y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 1,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE,
                         risen_y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 1);
	  break;
	}

      if (strike)
      {
	  int centerline = logical_rect.y + logical_rect.height / 2;
	  
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + logical_rect.x) / PANGO_SCALE - 1,
			 risen_y + centerline / PANGO_SCALE,
			 x + (x_off + logical_rect.x + logical_rect.width) / PANGO_SCALE + 1,
			 risen_y + centerline / PANGO_SCALE);
      }
      
      if (fg_gc != gc)
	gdk_pango_free_gc (context, fg_gc);

      x_off += logical_rect.width;
    }
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
 */
void 
gdk_draw_layout_with_colors (GdkDrawable     *drawable,
                             GdkGC           *gc,
                             int              x, 
                             int              y,
                             PangoLayout     *layout,
                             GdkColor        *foreground,
                             GdkColor        *background)
{
  PangoLayoutIter *iter;
  
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  iter = pango_layout_get_iter (layout);
  
  do
    {
      PangoRectangle logical_rect;
      PangoLayoutLine *line;
      int baseline;
      
      line = pango_layout_iter_get_line (iter);
      
      pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline (iter);
      
      gdk_draw_layout_line_with_colors (drawable, gc,
                                        x + logical_rect.x / PANGO_SCALE,
                                        y + baseline / PANGO_SCALE,
                                        line,
                                        foreground,
                                        background);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
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

static void
gdk_pango_get_item_properties (PangoItem      *item,
			       PangoUnderline *uline,
			       gboolean       *strikethrough,
                               gint           *rise,
			       PangoColor     *fg_color,
			       gboolean       *fg_set,
			       PangoColor     *bg_color,
			       gboolean       *bg_set,
                               gboolean       *embossed,
                               GdkBitmap     **stipple,
			       gboolean       *shape_set,
			       PangoRectangle *ink_rect,
			       PangoRectangle *logical_rect)
{
  GSList *tmp_list = item->analysis.extra_attrs;

  if (strikethrough)
      *strikethrough = FALSE;
  
  if (fg_set)
    *fg_set = FALSE;
  
  if (bg_set)
    *bg_set = FALSE;

  if (shape_set)
    *shape_set = FALSE;

  if (rise)
    *rise = 0;

  if (embossed)
    *embossed = FALSE;

  if (stipple)
    *stipple = NULL;
  
  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      switch (attr->klass->type)
	{
	case PANGO_ATTR_UNDERLINE:
	  if (uline)
	    *uline = ((PangoAttrInt *)attr)->value;
	  break;

	case PANGO_ATTR_STRIKETHROUGH:
	    if (strikethrough)
		*strikethrough = ((PangoAttrInt *)attr)->value;
	    break;
	    
	case PANGO_ATTR_FOREGROUND:
	  if (fg_color)
	    *fg_color = ((PangoAttrColor *)attr)->color;
	  if (fg_set)
	    *fg_set = TRUE;
	  
	  break;
	  
	case PANGO_ATTR_BACKGROUND:
	  if (bg_color)
	    *bg_color = ((PangoAttrColor *)attr)->color;
	  if (bg_set)
	    *bg_set = TRUE;
	  
	  break;

	case PANGO_ATTR_SHAPE:
	  if (shape_set)
	    *shape_set = TRUE;
	  if (logical_rect)
	    *logical_rect = ((PangoAttrShape *)attr)->logical_rect;
	  if (ink_rect)
	    *ink_rect = ((PangoAttrShape *)attr)->ink_rect;
	  break;

        case PANGO_ATTR_RISE:
          if (rise)
            *rise = ((PangoAttrInt *)attr)->value;
          break;
          
	default:
          /* stipple_type and embossed_type aren't necessarily
           * initialized, but they are 0, which is an
           * invalid type so won't occur. 
           */
          if (stipple && attr->klass->type == gdk_pango_attr_stipple_type)
            {
              *stipple = ((GdkPangoAttrStipple*)attr)->stipple;
            }
          else if (embossed && attr->klass->type == gdk_pango_attr_embossed_type)
            {
              *embossed = ((GdkPangoAttrEmbossed*)attr)->embossed;
            }
	  break;
	}
      tmp_list = tmp_list->next;
    }
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
    g_object_unref (G_OBJECT (st->stipple));
  
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
      pango_layout_line_get_x_ranges (line,
                                      index_ranges[i*2],
                                      index_ranges[i*2+1],
                                      &pixel_ranges, &n_pixel_ranges);
  
      for (j=0; j < n_pixel_ranges; j++)
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
