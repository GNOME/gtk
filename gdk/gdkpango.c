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
gdk_pango_get_gc (PangoContext   *context,
		  PangoColor     *fg_color,
                  GdkBitmap      *stipple,
		  GdkGC          *base_gc)
{
  GdkColor color;
  GdkGC *result;
  GdkPangoContextInfo *info;
  
  g_return_val_if_fail (context != NULL, NULL);

  info = gdk_pango_context_get_info (context, FALSE);

  if (info == NULL || info->colormap == NULL)
    {
      g_warning ("you must set the colormap on a PangoContext before using it to draw a layout");
      return NULL;
    }
  
  color.red = fg_color->red;
  color.green = fg_color->green;
  color.blue = fg_color->blue;
  
  result = gdk_gc_new (gdk_parent_root);
  gdk_gc_copy (result, base_gc);
  gdk_rgb_find_color (info->colormap, &color);
  gdk_gc_set_foreground (result, &color);

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
  GSList *tmp_list = line->runs;
  PangoRectangle overall_rect;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  PangoContext *context;
  gint x_off = 0;
  gint rise = 0;
  gboolean embossed;
  GdkBitmap *stipple;
  
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (line != NULL);

  context = pango_layout_get_context (line->layout);
  
  pango_layout_line_get_extents (line,NULL, &overall_rect);
  
  while (tmp_list)
    {
      PangoUnderline uline = PANGO_UNDERLINE_NONE;
      PangoLayoutRun *run = tmp_list->data;
      PangoColor fg_color, bg_color;
      gboolean fg_set, bg_set, shape_set;
      GdkGC *fg_gc;
      gint risen_y;
      
      tmp_list = tmp_list->next;
      
      gdk_pango_get_item_properties (run->item, &uline,
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

      if (bg_set)
	{
	  GdkGC *bg_gc = gdk_pango_get_gc (context, &bg_color, stipple, gc);
          
	  gdk_draw_rectangle (drawable, bg_gc, TRUE,
			      x + (x_off + logical_rect.x) / PANGO_SCALE,
			      risen_y + overall_rect.y / PANGO_SCALE,
			      logical_rect.width / PANGO_SCALE,
			      overall_rect.height / PANGO_SCALE);

          if (stipple)
            gdk_gc_set_fill (bg_gc, GDK_SOLID);
          
	  gdk_pango_free_gc (context, bg_gc);
	}

      if (fg_set || stipple)
	fg_gc = gdk_pango_get_gc (context, &fg_color, stipple, gc);
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
              GdkGC *white_gc = gdk_pango_get_gc (context, &color, stipple, fg_gc);
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
      
      if (fg_gc != gc)
	gdk_pango_free_gc (context, fg_gc);

      x_off += logical_rect.width;
    }
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
  PangoLayoutIter *iter;
  
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (layout != NULL);

  iter = pango_layout_get_iter (layout);
  
  do
    {
      PangoRectangle logical_rect;
      PangoLayoutLine *line;
      int baseline;
      
      line = pango_layout_iter_get_line (iter);
      
      pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline (iter);
      
      gdk_draw_layout_line (drawable, gc,
			    x + logical_rect.x / PANGO_SCALE,
                            y + baseline / PANGO_SCALE,
			    line);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
}

static void
gdk_pango_get_item_properties (PangoItem      *item,
			       PangoUnderline *uline,
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
  GSList *tmp_list = item->extra_attrs;

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
              *embossed = ((GdkPangoAttrEmbossed*)attr);
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
