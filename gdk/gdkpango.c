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

static void gdk_pango_get_item_properties (PangoItem      *item,
					   PangoUnderline *uline,
					   PangoAttrColor *fg_color,
					   gboolean       *fg_set,
					   PangoAttrColor *bg_color,
					   gboolean       *bg_set,
					   gboolean       *shape_set);

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
		  PangoAttrColor *fg_color,
		  GdkGC          *base_gc)
{
  GdkPangoContextInfo *info;
  GdkColormap *colormap;
  GdkColor color;
  
  g_return_val_if_fail (context != NULL, NULL);

  info = gdk_pango_context_get_info (context, FALSE);

  if (info && info->colormap)
    colormap = info->colormap;
  else
    colormap = gdk_colormap_get_system();

  /* FIXME. FIXME. FIXME. Only works for true color */

  color.red = fg_color->red;
  color.green = fg_color->green;
  color.blue = fg_color->blue;
  
  if (gdk_colormap_alloc_color (colormap, &color, FALSE, TRUE))
    {
      GdkGC *result = gdk_gc_new (gdk_parent_root);
      gdk_gc_copy (result, base_gc);
      gdk_gc_set_foreground (result, &color);

      return result;
    }
  else
    return gdk_gc_ref (base_gc);
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

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (line != NULL);

  context = pango_layout_get_context (line->layout);
  
  pango_layout_line_get_extents (line,NULL, &overall_rect);
  
  while (tmp_list)
    {
      PangoUnderline uline = PANGO_UNDERLINE_NONE;
      PangoLayoutRun *run = tmp_list->data;
      PangoAttrColor fg_color, bg_color;
      gboolean fg_set, bg_set, shape_set;
      GdkGC *fg_gc;
      
      tmp_list = tmp_list->next;

      gdk_pango_get_item_properties (run->item, &uline, &fg_color, &fg_set, &bg_color, &bg_set, &shape_set);

      if (uline == PANGO_UNDERLINE_NONE)
	pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
				    NULL, &logical_rect);
      else
	pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
				    &ink_rect, &logical_rect);

      if (bg_set)
	{
	  GdkGC *bg_gc = gdk_pango_get_gc (context, &bg_color, gc);

	  gdk_draw_rectangle (drawable, bg_gc, TRUE,
			      x + (x_off + logical_rect.x) / PANGO_SCALE,
			      y + overall_rect.y / PANGO_SCALE,
			      logical_rect.width / PANGO_SCALE,
			      overall_rect.height / PANGO_SCALE);

	  gdk_pango_free_gc (context, bg_gc);
	}

      if (shape_set)
	continue;

      if (fg_set)
	fg_gc = gdk_pango_get_gc (context, &fg_color, gc);
      else
	fg_gc = gc;

      gdk_draw_glyphs (drawable, fg_gc, run->item->analysis.font,
		       x + x_off / PANGO_SCALE, y, run->glyphs);

      switch (uline)
	{
	case PANGO_UNDERLINE_NONE:
	  break;
	case PANGO_UNDERLINE_DOUBLE:
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + 4,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + 4);
	  /* Fall through */
	case PANGO_UNDERLINE_SINGLE:
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + 2,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + 2);
	  break;
	case PANGO_UNDERLINE_LOW:
	  gdk_draw_line (drawable, fg_gc,
			 x + (x_off + ink_rect.x) / PANGO_SCALE - 1, y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 2,
			 x + (x_off + ink_rect.x + ink_rect.width) / PANGO_SCALE, y + (ink_rect.y + ink_rect.height) / PANGO_SCALE + 2);
	  break;
	}

      if (fg_set)
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
  PangoRectangle logical_rect;
  GSList *tmp_list;
  PangoAlignment align;
  gint indent;
  gint width;
  gint y_offset = 0;
  gboolean first = FALSE;
  
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (layout != NULL);

  indent = pango_layout_get_indent (layout);
  width = pango_layout_get_width (layout);
  align = pango_layout_get_alignment (layout);

  if (width == -1 && align != PANGO_ALIGN_LEFT)
    {
      pango_layout_get_extents (layout, NULL, &logical_rect);
      width = logical_rect.width;
    }
  
  tmp_list = pango_layout_get_lines (layout);
  while (tmp_list)
    {
      PangoLayoutLine *line = tmp_list->data;
      int x_offset;
      
      pango_layout_line_get_extents (line, NULL, &logical_rect);

      if (width != -1 && align == PANGO_ALIGN_RIGHT)
	x_offset = width - logical_rect.width;
      else if (width != -1 && align == PANGO_ALIGN_CENTER)
	x_offset = (width - logical_rect.width) / 2;
      else
	x_offset = 0;

      if (first)
	{
	  if (indent > 0)
	    {
	      if (align == PANGO_ALIGN_LEFT)
		x_offset += indent;
	      else
		x_offset -= indent;
	    }

	  first = FALSE;
	}
      else
	{
	  if (indent < 0)
	    {
	      if (align == PANGO_ALIGN_LEFT)
		x_offset -= indent;
	      else
		x_offset += indent;
	    }
	}
	  
      gdk_draw_layout_line (drawable, gc,
			    x + x_offset / PANGO_SCALE, y + (y_offset - logical_rect.y) / PANGO_SCALE,
			    line);

      y_offset += logical_rect.height;
      tmp_list = tmp_list->next;
    }
}

static void
gdk_pango_get_item_properties (PangoItem      *item,
			       PangoUnderline *uline,
			       PangoAttrColor *fg_color,
			       gboolean       *fg_set,
			       PangoAttrColor *bg_color,
			       gboolean       *bg_set,
			       gboolean       *shape_set)
{
  GSList *tmp_list = item->extra_attrs;

  if (fg_set)
    *fg_set = FALSE;
  
  if (bg_set)
    *bg_set = FALSE;

  if (shape_set)
    *shape_set = FALSE;
  
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
	    *fg_color = *((PangoAttrColor *)attr);
	  if (fg_set)
	    *fg_set = TRUE;
	  
	  break;
	  
	case PANGO_ATTR_BACKGROUND:
	  if (bg_color)
	    *bg_color = *((PangoAttrColor *)attr);
	  if (bg_set)
	    *bg_set = TRUE;
	  
	  break;

	case PANGO_ATTR_SHAPE:
	  if (shape_set)
	    *shape_set = TRUE;
	  break;
	  
	default:
	  break;
	}
      tmp_list = tmp_list->next;
    }
}

