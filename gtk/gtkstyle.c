/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
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
#include <math.h>
#include "gtkgc.h"
#include "gtkstyle.h"
#include "gtkwidget.h"


#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7


typedef struct _GtkStyleKey GtkStyleKey;

struct _GtkStyleKey
{
  GdkColor fg[5];
  GdkColor bg[5];
  GdkColor text[5];
  GdkColor base[5];

  GdkPixmap *bg_pixmap[5];

  GdkFont *font;

  gint depth;
  GdkColormap *colormap;
  GtkStyleClass *klass;
};


static void         gtk_style_init         (GtkStyle    *style);
static void         gtk_styles_init        (void);
static void         gtk_style_remove       (GtkStyle    *style);
static GtkStyle*    gtk_style_find         (GtkStyle    *style,
					    GdkColormap *cmap,
					    gint         depth);
static GtkStyle*    gtk_style_new_from_key (GtkStyleKey *key);
static GtkStyleKey* gtk_style_key_dup      (GtkStyleKey *key);
static void         gtk_style_destroy      (GtkStyle    *style);
static void         gtk_style_key_destroy  (GtkStyleKey *key);
static guint        gtk_style_key_hash     (GtkStyleKey *key);
static guint        gtk_style_value_hash   (GtkStyle    *style);
static gint         gtk_style_key_compare  (GtkStyleKey *a,
					    GtkStyleKey *b);

static void gtk_default_draw_hline   (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      gint           x1,
				      gint           x2,
				      gint           y);
static void gtk_default_draw_vline   (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      gint           y1,
				      gint           y2,
				      gint           x);
static void gtk_default_draw_shadow  (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      GtkShadowType  shadow_type,
				      gint           x,
				      gint           y,
				      gint           width,
				      gint           height);
static void gtk_default_draw_polygon (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      GtkShadowType  shadow_type,
				      GdkPoint      *points,
				      gint           npoints,
				      gint           fill);
static void gtk_default_draw_arrow   (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      GtkShadowType  shadow_type,
				      GtkArrowType   arrow_type,
				      gint           fill,
				      gint           x,
				      gint           y,
				      gint           width,
				      gint           height);
static void gtk_default_draw_diamond (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      GtkShadowType  shadow_type,
				      gint           x,
				      gint           y,
				      gint           width,
				      gint           height);
static void gtk_default_draw_oval    (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      GtkShadowType  shadow_type,
				      gint           x,
				      gint           y,
				      gint           width,
				      gint           height);
static void gtk_default_draw_string  (GtkStyle      *style,
				      GdkWindow     *window,
				      GtkStateType   state_type,
				      gint           x,
				      gint           y,
				      const gchar   *string);

static void gtk_style_shade (GdkColor *a, GdkColor *b, gdouble k);
static void rgb_to_hls (gdouble *r, gdouble *g, gdouble *b);
static void hls_to_rgb (gdouble *h, gdouble *l, gdouble *s);


static GtkStyleClass default_class =
{
  2,
  2,
  gtk_default_draw_hline,
  gtk_default_draw_vline,
  gtk_default_draw_shadow,
  gtk_default_draw_polygon,
  gtk_default_draw_arrow,
  gtk_default_draw_diamond,
  gtk_default_draw_oval,
  gtk_default_draw_string,
};

static GdkColor gtk_default_normal_fg =      { 0,      0,      0,      0 };
static GdkColor gtk_default_active_fg =      { 0,      0,      0,      0 };
static GdkColor gtk_default_prelight_fg =    { 0,      0,      0,      0 };
static GdkColor gtk_default_selected_fg =    { 0, 0xffff, 0xffff, 0xffff };
static GdkColor gtk_default_insensitive_fg = { 0, 0x7530, 0x7530, 0x7530 };

static GdkColor gtk_default_normal_bg =      { 0, 0xd6d6, 0xd6d6, 0xd6d6 };
static GdkColor gtk_default_active_bg =      { 0, 0xc350, 0xc350, 0xc350 };
static GdkColor gtk_default_prelight_bg =    { 0, 0xea60, 0xea60, 0xea60 };
static GdkColor gtk_default_selected_bg =    { 0,      0,      0, 0x9c40 };
static GdkColor gtk_default_insensitive_bg = { 0, 0xd6d6, 0xd6d6, 0xd6d6 };

static GdkFont *default_font = NULL;

static gint initialize = TRUE;
static GCache *style_cache = NULL;
static GSList *unattached_styles = NULL;

static GMemChunk *key_mem_chunk = NULL;

GtkStyle*
gtk_style_copy (GtkStyle     *style)
{
  GtkStyle *new_style;
  guint i;

  g_return_val_if_fail (style != NULL, NULL);

  new_style = gtk_style_new ();

  for (i = 0; i < 5; i++)
    {
      new_style->fg[i] = style->fg[i];
      new_style->bg[i] = style->bg[i];
      new_style->text[i] = style->text[i];
      new_style->base[i] = style->base[i];

      new_style->bg_pixmap[i] = style->bg_pixmap[i];
    }

  new_style->font = style->font;
  gdk_font_ref (new_style->font);

  return new_style;
}

GtkStyle*
gtk_style_new (void)
{
  GtkStyle *style;
  gint i;

  style = g_new0 (GtkStyle, 1);

  if (!default_font)
    default_font =
      gdk_font_load ("-adobe-helvetica-medium-r-normal--*-120-*-*-*-*-*-*");

  style->font = default_font;
  gdk_font_ref (style->font);

  style->ref_count = 1;
  style->attach_count = 0;
  style->colormap = NULL;
  style->depth = -1;
  style->klass = &default_class;

  style->black.red = 0;
  style->black.green = 0;
  style->black.blue = 0;

  style->white.red = 65535;
  style->white.green = 65535;
  style->white.blue = 65535;

  style->black_gc = NULL;
  style->white_gc = NULL;

  style->fg[GTK_STATE_NORMAL] = gtk_default_normal_fg;
  style->fg[GTK_STATE_ACTIVE] = gtk_default_active_fg;
  style->fg[GTK_STATE_PRELIGHT] = gtk_default_prelight_fg;
  style->fg[GTK_STATE_SELECTED] = gtk_default_selected_fg;
  style->fg[GTK_STATE_INSENSITIVE] = gtk_default_insensitive_fg;

  style->bg[GTK_STATE_NORMAL] = gtk_default_normal_bg;
  style->bg[GTK_STATE_ACTIVE] = gtk_default_active_bg;
  style->bg[GTK_STATE_PRELIGHT] = gtk_default_prelight_bg;
  style->bg[GTK_STATE_SELECTED] = gtk_default_selected_bg;
  style->bg[GTK_STATE_INSENSITIVE] = gtk_default_insensitive_bg;

  for (i = 0; i < 4; i++)
    {
      style->text[i] = style->fg[i];
      style->base[i] = style->white;
    }

  style->base[GTK_STATE_INSENSITIVE] = gtk_default_insensitive_bg;
  style->text[GTK_STATE_INSENSITIVE] = gtk_default_insensitive_bg;

  for (i = 0; i < 5; i++)
    style->bg_pixmap[i] = NULL;

  for (i = 0; i < 5; i++)
    {
      style->fg_gc[i] = NULL;
      style->bg_gc[i] = NULL;
      style->light_gc[i] = NULL;
      style->dark_gc[i] = NULL;
      style->mid_gc[i] = NULL;
      style->text_gc[i] = NULL;
      style->base_gc[i] = NULL;
    }

  unattached_styles = g_slist_prepend (unattached_styles, style);

  return style;
}

GtkStyle*
gtk_style_attach (GtkStyle  *style,
		  GdkWindow *window)
{
  GtkStyle *new_style;
  GdkColormap *colormap;
  gint depth;

  g_return_val_if_fail (style != NULL, NULL);
  g_return_val_if_fail (window != NULL, NULL);

  colormap = gdk_window_get_colormap (window);
  depth = gdk_window_get_visual (window)->depth;

  new_style = gtk_style_find (style, colormap, depth);

  if (new_style && (new_style != style))
    {
      gtk_style_unref (style);
      style = new_style;
      gtk_style_ref (style);
    }

  if (style->attach_count == 0)
    unattached_styles = g_slist_remove (unattached_styles, style);

  style->attach_count += 1;

  return style;
}

void
gtk_style_detach (GtkStyle *style)
{
  gint i;

  g_return_if_fail (style != NULL);

  style->attach_count -= 1;
  if (style->attach_count == 0)
    {
      unattached_styles = g_slist_prepend (unattached_styles, style);

      gtk_gc_release (style->black_gc);
      gtk_gc_release (style->white_gc);

      style->black_gc = NULL;
      style->white_gc = NULL;

      for (i = 0; i < 5; i++)
	{
	  gtk_gc_release (style->fg_gc[i]);
	  gtk_gc_release (style->bg_gc[i]);
	  gtk_gc_release (style->light_gc[i]);
	  gtk_gc_release (style->dark_gc[i]);
	  gtk_gc_release (style->mid_gc[i]);
	  gtk_gc_release (style->text_gc[i]);
	  gtk_gc_release (style->base_gc[i]);

	  style->fg_gc[i] = NULL;
	  style->bg_gc[i] = NULL;
	  style->light_gc[i] = NULL;
	  style->dark_gc[i] = NULL;
	  style->mid_gc[i] = NULL;
	  style->text_gc[i] = NULL;
	  style->base_gc[i] = NULL;
	}

      style->depth = -1;
      style->colormap = NULL;
    }

  gtk_style_remove (style);
}

GtkStyle*
gtk_style_ref (GtkStyle *style)
{
  g_return_val_if_fail (style != NULL, NULL);

  style->ref_count += 1;
  return style;
}

void
gtk_style_unref (GtkStyle *style)
{
  g_return_if_fail (style != NULL);

  style->ref_count -= 1;
  if (style->ref_count == 0)
    gtk_style_destroy (style);
}

void
gtk_style_set_background (GtkStyle     *style,
			  GdkWindow    *window,
			  GtkStateType  state_type)
{
  GdkPixmap *pixmap;
  gint parent_relative;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if (style->bg_pixmap[state_type])
    {
      if (style->bg_pixmap[state_type] == (GdkPixmap*) GDK_PARENT_RELATIVE)
	{
	  pixmap = NULL;
	  parent_relative = TRUE;
	}
      else
	{
	  pixmap = style->bg_pixmap[state_type];
	  parent_relative = FALSE;
	}

      gdk_window_set_back_pixmap (window, pixmap, parent_relative);
    }
  else
    gdk_window_set_background (window, &style->bg[state_type]);
}


void
gtk_draw_hline (GtkStyle     *style,
		GdkWindow    *window,
		GtkStateType  state_type,
		gint          x1,
		gint          x2,
		gint          y)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_hline != NULL);

  (*style->klass->draw_hline) (style, window, state_type, x1, x2, y);
}


void
gtk_draw_vline (GtkStyle     *style,
		GdkWindow    *window,
		GtkStateType  state_type,
		gint          y1,
		gint          y2,
		gint          x)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_vline != NULL);

  (*style->klass->draw_vline) (style, window, state_type, y1, y2, x);
}


void
gtk_draw_shadow (GtkStyle      *style,
		 GdkWindow     *window,
		 GtkStateType   state_type,
		 GtkShadowType  shadow_type,
		 gint           x,
		 gint           y,
		 gint           width,
		 gint           height)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_shadow != NULL);

  (*style->klass->draw_shadow) (style, window, state_type, shadow_type, x, y, width, height);
}

void
gtk_draw_polygon (GtkStyle      *style,
		  GdkWindow     *window,
		  GtkStateType   state_type,
		  GtkShadowType  shadow_type,
		  GdkPoint      *points,
		  gint           npoints,
		  gint           fill)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_shadow != NULL);

  (*style->klass->draw_polygon) (style, window, state_type, shadow_type, points, npoints, fill);
}

void
gtk_draw_arrow (GtkStyle      *style,
		GdkWindow     *window,
		GtkStateType   state_type,
		GtkShadowType  shadow_type,
		GtkArrowType   arrow_type,
		gint           fill,
		gint           x,
		gint           y,
		gint           width,
		gint           height)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_arrow != NULL);

  (*style->klass->draw_arrow) (style, window, state_type, shadow_type, arrow_type, fill, x, y, width, height);
}


void
gtk_draw_diamond (GtkStyle      *style,
		  GdkWindow     *window,
		  GtkStateType   state_type,
		  GtkShadowType  shadow_type,
		  gint           x,
		  gint           y,
		  gint           width,
		  gint           height)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_diamond != NULL);

  (*style->klass->draw_diamond) (style, window, state_type, shadow_type, x, y, width, height);
}


void
gtk_draw_oval (GtkStyle      *style,
	       GdkWindow     *window,
	       GtkStateType   state_type,
	       GtkShadowType  shadow_type,
	       gint           x,
	       gint           y,
	       gint           width,
	       gint           height)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_oval != NULL);

  (*style->klass->draw_oval) (style, window, state_type, shadow_type, x, y, width, height);
}

void
gtk_draw_string (GtkStyle      *style,
		 GdkWindow     *window,
		 GtkStateType   state_type,
		 gint           x,
		 gint           y,
		 const gchar   *string)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->klass != NULL);
  g_return_if_fail (style->klass->draw_oval != NULL);

  (*style->klass->draw_string) (style, window, state_type, x, y, string);
}


static void
gtk_style_init (GtkStyle *style)
{
  GdkGCValues gc_values;
  GdkGCValuesMask gc_values_mask;
  GdkColormap *colormap;
  gint i;

  g_return_if_fail (style != NULL);

  if (style->attach_count == 0)
    {
      for (i = 0; i < 5; i++)
	{
	  gtk_style_shade (&style->bg[i], &style->light[i], LIGHTNESS_MULT);
	  gtk_style_shade (&style->bg[i], &style->dark[i], DARKNESS_MULT);

	  style->mid[i].red = (style->light[i].red + style->dark[i].red) / 2;
	  style->mid[i].green = (style->light[i].green + style->dark[i].green) / 2;
	  style->mid[i].blue = (style->light[i].blue + style->dark[i].blue) / 2;
	}

      colormap = style->colormap;

      gdk_color_black (colormap, &style->black);
      gdk_color_white (colormap, &style->white);

      gc_values_mask = GDK_GC_FOREGROUND | GDK_GC_FONT;
      if (style->font->type == GDK_FONT_FONT)
	{
	  gc_values.font = style->font;
	}
      else if (style->font->type == GDK_FONT_FONTSET)
	{
	  gc_values.font = default_font;
	}

      gc_values.foreground = style->black;
      style->black_gc = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

      gc_values.foreground = style->white;
      style->white_gc = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

      for (i = 0; i < 5; i++)
	{
	  if (!gdk_color_alloc (colormap, &style->fg[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->fg[i].red, style->fg[i].green, style->fg[i].blue);
	  if (!gdk_color_alloc (colormap, &style->bg[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->bg[i].red, style->bg[i].green, style->bg[i].blue);
	  if (!gdk_color_alloc (colormap, &style->light[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->light[i].red, style->light[i].green, style->light[i].blue);
	  if (!gdk_color_alloc (colormap, &style->dark[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->dark[i].red, style->dark[i].green, style->dark[i].blue);
	  if (!gdk_color_alloc (colormap, &style->mid[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->mid[i].red, style->mid[i].green, style->mid[i].blue);
	  if (!gdk_color_alloc (colormap, &style->text[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->text[i].red, style->text[i].green, style->text[i].blue);
	  if (!gdk_color_alloc (colormap, &style->base[i]))
	    g_warning ("unable to allocate color: ( %d %d %d )",
		       style->base[i].red, style->base[i].green, style->base[i].blue);

	  gc_values.foreground = style->fg[i];
	  style->fg_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

	  gc_values.foreground = style->bg[i];
	  style->bg_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

	  gc_values.foreground = style->light[i];
	  style->light_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

	  gc_values.foreground = style->dark[i];
	  style->dark_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

	  gc_values.foreground = style->mid[i];
	  style->mid_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

	  gc_values.foreground = style->text[i];
	  style->text_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);

	  gc_values.foreground = style->base[i];
	  style->base_gc[i] = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);
	}
    }
}

static void
gtk_styles_init (void)
{
  if (initialize)
    {
      initialize = FALSE;

      style_cache = g_cache_new ((GCacheNewFunc) gtk_style_new_from_key,
				 (GCacheDestroyFunc) gtk_style_unref,
				 (GCacheDupFunc) gtk_style_key_dup,
				 (GCacheDestroyFunc) gtk_style_key_destroy,
				 (GHashFunc) gtk_style_key_hash,
				 (GHashFunc) gtk_style_value_hash,
				 (GCompareFunc) gtk_style_key_compare);
    }
}

static void
gtk_style_remove (GtkStyle *style)
{
  if (initialize)
    gtk_styles_init ();
  g_cache_remove (style_cache, style);
}

static GtkStyle*
gtk_style_find (GtkStyle    *style,
		GdkColormap *cmap,
		gint         depth)
{
  GtkStyleKey key;
  gint i;

  if (initialize)
    gtk_styles_init ();

  for (i = 0; i < 5; i++)
    {
      key.fg[i] = style->fg[i];
      key.bg[i] = style->bg[i];
      key.text[i] = style->text[i];
      key.base[i] = style->base[i];
      key.bg_pixmap[i] = style->bg_pixmap[i];
    }

  key.font = style->font;
  key.klass = style->klass;
  key.depth = depth;
  key.colormap = cmap;

  style = g_cache_insert (style_cache, &key);

  return style;
}

static GtkStyle*
gtk_style_new_from_key (GtkStyleKey *key)
{
  GtkStyle *style;
  GSList *list;
  gint i;

  style = NULL;
  list = unattached_styles;

  while (list)
    {
      style = list->data;
      list = list->next;

      if ((style->depth != -1) && (style->depth != key->depth))
	{
	  style = NULL;
	  continue;
	}
      if (style->colormap && (style->colormap != key->colormap))
	{
	  style = NULL;
	  continue;
	}
      if (style->klass != key->klass)
	{
	  style = NULL;
	  continue;
	}
      if (!gdk_font_equal (style->font, key->font))
	{
	  style = NULL;
	  continue;
	}

      for (i = 0; style && (i < 5); i++)
	{
	  if (style->bg_pixmap[i] != key->bg_pixmap[i])
	    {
	      style = NULL;
	      continue;
	    }

	  if ((style->fg[i].red != key->fg[i].red) ||
	      (style->fg[i].green != key->fg[i].green) ||
	      (style->fg[i].blue != key->fg[i].blue))
	    {
	      style = NULL;
	      continue;
	    }

	  if ((style->bg[i].red != key->bg[i].red) ||
	      (style->bg[i].green != key->bg[i].green) ||
	      (style->bg[i].blue != key->bg[i].blue))
	    {
	      style = NULL;
	      continue;
	    }

	  if ((style->text[i].red != key->text[i].red) ||
	      (style->text[i].green != key->text[i].green) ||
	      (style->text[i].blue != key->text[i].blue))
	    {
	      style = NULL;
	      continue;
	    }

	  if ((style->base[i].red != key->base[i].red) ||
	      (style->base[i].green != key->base[i].green) ||
	      (style->base[i].blue != key->base[i].blue))
	    {
	      style = NULL;
	      continue;
	    }
	}

      if (style)
	{
	  gtk_style_ref (style);
	  break;
	}
    }

  if (!style)
    {
      style = g_new0 (GtkStyle, 1);

      style->ref_count = 1;
      style->attach_count = 0;

      style->font = key->font;
      gdk_font_ref (style->font);

      style->depth = key->depth;
      style->colormap = key->colormap;
      style->klass = key->klass;

      style->black.red = 0;
      style->black.green = 0;
      style->black.blue = 0;

      style->white.red = 65535;
      style->white.green = 65535;
      style->white.blue = 65535;

      style->black_gc = NULL;
      style->white_gc = NULL;

      for (i = 0; i < 5; i++)
	{
	  style->fg[i] = key->fg[i];
	  style->bg[i] = key->bg[i];
	  style->text[i] = key->text[i];
	  style->base[i] = key->base[i];
	}

      for (i = 0; i < 5; i++)
	style->bg_pixmap[i] = key->bg_pixmap[i];

      for (i = 0; i < 5; i++)
	{
	  style->fg_gc[i] = NULL;
	  style->bg_gc[i] = NULL;
	  style->light_gc[i] = NULL;
	  style->dark_gc[i] = NULL;
	  style->mid_gc[i] = NULL;
	  style->text_gc[i] = NULL;
	  style->base_gc[i] = NULL;
	}
    }

  if (style->depth == -1)
    style->depth = key->depth;
  if (!style->colormap)
    style->colormap = key->colormap;

  gtk_style_init (style);

  return style;
}

static GtkStyleKey*
gtk_style_key_dup (GtkStyleKey *key)
{
  GtkStyleKey *new_key;

  if (!key_mem_chunk)
    key_mem_chunk = g_mem_chunk_new ("key mem chunk", sizeof (GtkStyleKey),
				     1024, G_ALLOC_AND_FREE);

  new_key = g_chunk_new (GtkStyleKey, key_mem_chunk);

  *new_key = *key;

  return new_key;
}

static void
gtk_style_destroy (GtkStyle *style)
{
  gint i;

  if (style->attach_count > 0)
    {
      gtk_gc_release (style->black_gc);
      gtk_gc_release (style->white_gc);

      for (i = 0; i < 5; i++)
	{
	  gtk_gc_release (style->fg_gc[i]);
	  gtk_gc_release (style->bg_gc[i]);
	  gtk_gc_release (style->light_gc[i]);
	  gtk_gc_release (style->dark_gc[i]);
	  gtk_gc_release (style->mid_gc[i]);
	  gtk_gc_release (style->text_gc[i]);
	  gtk_gc_release (style->base_gc[i]);
	}
    }

  unattached_styles = g_slist_remove (unattached_styles, style);

  gdk_font_unref (style->font);
  g_free (style);
}

static void
gtk_style_key_destroy (GtkStyleKey *key)
{
  g_mem_chunk_free (key_mem_chunk, key);
}

static guint
gtk_style_key_hash (GtkStyleKey *key)
{
  guint hash_val;
  gint i;

  hash_val = 0;

  for (i = 0; i < 5; i++)
    {
      hash_val += key->fg[i].red + key->fg[i].green + key->fg[i].blue;
      hash_val += key->bg[i].red + key->bg[i].green + key->bg[i].blue;
      hash_val += key->text[i].red + key->text[i].green + key->text[i].blue;
      hash_val += key->base[i].red + key->base[i].green + key->base[i].blue;
    }

  hash_val += (guint) gdk_font_id (key->font);
  hash_val += (guint) key->depth;
  hash_val += (gulong) key->colormap;
  hash_val += (gulong) key->klass;

  return hash_val;
}

static guint
gtk_style_value_hash (GtkStyle *style)
{
  guint hash_val;
  gint i;

  hash_val = 0;

  for (i = 0; i < 5; i++)
    {
      hash_val += style->fg[i].red + style->fg[i].green + style->fg[i].blue;
      hash_val += style->bg[i].red + style->bg[i].green + style->bg[i].blue;
      hash_val += style->text[i].red + style->text[i].green + style->text[i].blue;
      hash_val += style->base[i].red + style->base[i].green + style->base[i].blue;
    }

  hash_val += (guint) gdk_font_id (style->font);
  hash_val += (gulong) style->klass;

  return hash_val;
}

static gint
gtk_style_key_compare (GtkStyleKey *a,
		       GtkStyleKey *b)
{
  gint i;

  if (a->depth != b->depth)
    return FALSE;
  if (a->colormap != b->colormap)
    return FALSE;
  if (a->klass != b->klass)
    return FALSE;
  if (!gdk_font_equal (a->font, b->font))
    return FALSE;

  for (i = 0; i < 5; i++)
    {
      if (a->bg_pixmap[i] != b->bg_pixmap[i])
	return FALSE;

      if ((a->fg[i].red != b->fg[i].red) ||
	  (a->fg[i].green != b->fg[i].green) ||
	  (a->fg[i].blue != b->fg[i].blue))
	return FALSE;
      if ((a->bg[i].red != b->bg[i].red) ||
	  (a->bg[i].green != b->bg[i].green) ||
	  (a->bg[i].blue != b->bg[i].blue))
	return FALSE;
      if ((a->text[i].red != b->text[i].red) ||
	  (a->text[i].green != b->text[i].green) ||
	  (a->text[i].blue != b->text[i].blue))
	return FALSE;
      if ((a->base[i].red != b->base[i].red) ||
	  (a->base[i].green != b->base[i].green) ||
	  (a->base[i].blue != b->base[i].blue))
	return FALSE;
    }

  return TRUE;
}


static void
gtk_default_draw_hline (GtkStyle     *style,
			GdkWindow    *window,
			GtkStateType  state_type,
			gint          x1,
			gint          x2,
			gint          y)
{
  gint thickness_light;
  gint thickness_dark;
  gint i;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  thickness_light = style->klass->ythickness / 2;
  thickness_dark = style->klass->ythickness - thickness_light;

  for (i = 0; i < thickness_dark; i++)
    {
      gdk_draw_line (window, style->light_gc[state_type], x2 - i - 1, y + i, x2, y + i);
      gdk_draw_line (window, style->dark_gc[state_type], x1, y + i, x2 - i - 1, y + i);
    }

  y += thickness_dark;
  for (i = 0; i < thickness_light; i++)
    {
      gdk_draw_line (window, style->dark_gc[state_type], x1, y + i, x1 + thickness_light - i - 1, y + i);
      gdk_draw_line (window, style->light_gc[state_type], x1 + thickness_light - i - 1, y + i, x2, y + i);
    }
}


static void
gtk_default_draw_vline (GtkStyle     *style,
			GdkWindow    *window,
			GtkStateType  state_type,
			gint          y1,
			gint          y2,
			gint          x)
{
  gint thickness_light;
  gint thickness_dark;
  gint i;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  thickness_light = style->klass->xthickness / 2;
  thickness_dark = style->klass->xthickness - thickness_light;

  for (i = 0; i < thickness_dark; i++)
    {
      gdk_draw_line (window, style->light_gc[state_type], x + i, y2 - i - 1, x + i, y2);
      gdk_draw_line (window, style->dark_gc[state_type], x + i, y1, x + i, y2 - i - 1);
    }

  x += thickness_dark;
  for (i = 0; i < thickness_light; i++)
    {
      gdk_draw_line (window, style->dark_gc[state_type], x + i, y1, x + i, y1 + thickness_light - i);
      gdk_draw_line (window, style->light_gc[state_type], x + i, y1 + thickness_light - i, x + i, y2);
    }
}


static void
gtk_default_draw_shadow (GtkStyle      *style,
			 GdkWindow     *window,
			 GtkStateType   state_type,
			 GtkShadowType  shadow_type,
			 gint           x,
			 gint           y,
			 gint           width,
			 gint           height)
{
  GdkGC *gc1;
  GdkGC *gc2;
  gint thickness_light;
  gint thickness_dark;
  gint i;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);

  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      gc1 = NULL;
      gc2 = NULL;
      break;
    case GTK_SHADOW_IN:
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      break;
    }

  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      break;

    case GTK_SHADOW_IN:
      gdk_draw_line (window, gc1,
                     x, y + height - 1, x + width - 1, y + height - 1);
      gdk_draw_line (window, gc1,
                     x + width - 1, y, x + width - 1, y + height - 1);

      gdk_draw_line (window, style->bg_gc[state_type],
                     x + 1, y + height - 2, x + width - 2, y + height - 2);
      gdk_draw_line (window, style->bg_gc[state_type],
                     x + width - 2, y + 1, x + width - 2, y + height - 2);

      gdk_draw_line (window, style->black_gc,
                     x + 1, y + 1, x + width - 2, y + 1);
      gdk_draw_line (window, style->black_gc,
                     x + 1, y + 1, x + 1, y + height - 2);

      gdk_draw_line (window, gc2,
                     x, y, x + width - 1, y);
      gdk_draw_line (window, gc2,
                     x, y, x, y + height - 1);
      break;

    case GTK_SHADOW_OUT:
      gdk_draw_line (window, gc1,
                     x + 1, y + height - 2, x + width - 2, y + height - 2);
      gdk_draw_line (window, gc1,
                     x + width - 2, y + 1, x + width - 2, y + height - 2);

      gdk_draw_line (window, gc2,
                     x, y, x + width - 1, y);
      gdk_draw_line (window, gc2,
                     x, y, x, y + height - 1);

      gdk_draw_line (window, style->bg_gc[state_type],
                     x + 1, y + 1, x + width - 2, y + 1);
      gdk_draw_line (window, style->bg_gc[state_type],
                     x + 1, y + 1, x + 1, y + height - 2);

      gdk_draw_line (window, style->black_gc,
                     x, y + height - 1, x + width - 1, y + height - 1);
      gdk_draw_line (window, style->black_gc,
                     x + width - 1, y, x + width - 1, y + height - 1);
      break;

    case GTK_SHADOW_ETCHED_IN:
    case GTK_SHADOW_ETCHED_OUT:
      thickness_light = 1;
      thickness_dark = 1;

      for (i = 0; i < thickness_dark; i++)
        {
          gdk_draw_line (window, gc1,
                         x + i,
                         y + height - i - 1,
                         x + width - i - 1,
                         y + height - i - 1);
          gdk_draw_line (window, gc1,
                         x + width - i - 1,
                         y + i,
                         x + width - i - 1,
                         y + height - i - 1);

          gdk_draw_line (window, gc2,
                         x + i,
                         y + i,
                         x + width - i - 2,
                         y + i);
          gdk_draw_line (window, gc2,
                         x + i,
                         y + i,
                         x + i,
                         y + height - i - 2);
        }

      for (i = 0; i < thickness_light; i++)
        {
          gdk_draw_line (window, gc1,
                         x + thickness_dark + i,
                         y + thickness_dark + i,
                         x + width - thickness_dark - i - 1,
                         y + thickness_dark + i);
          gdk_draw_line (window, gc1,
                         x + thickness_dark + i,
                         y + thickness_dark + i,
                         x + thickness_dark + i,
                         y + height - thickness_dark - i - 1);

          gdk_draw_line (window, gc2,
                         x + thickness_dark + i,
                         y + height - thickness_light - i - 1,
                         x + width - thickness_light - 1,
                         y + height - thickness_light - i - 1);
          gdk_draw_line (window, gc2,
                         x + width - thickness_light - i - 1,
                         y + thickness_dark + i,
                         x + width - thickness_light - i - 1,
                         y + height - thickness_light - 1);
        }
      break;
    }
}


static void
gtk_default_draw_polygon (GtkStyle      *style,
			  GdkWindow     *window,
			  GtkStateType   state_type,
			  GtkShadowType  shadow_type,
			  GdkPoint      *points,
			  gint           npoints,
			  gint           fill)
{
#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif /* M_PI */
#ifndef M_PI_4
#define M_PI_4  0.78539816339744830962
#endif /* M_PI_4 */

  static const gdouble pi_over_4 = M_PI_4;
  static const gdouble pi_3_over_4 = M_PI_4 * 3;

  GdkGC *gc1;
  GdkGC *gc2;
  GdkGC *gc3;
  GdkGC *gc4;
  gdouble angle;
  gint xadjust;
  gint yadjust;
  gint i;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);
  g_return_if_fail (points != NULL);

  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      gc1 = style->bg_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->black_gc;
      gc4 = style->bg_gc[state_type];
      break;
    default:
      return;
    }

  if (fill)
    gdk_draw_polygon (window, style->bg_gc[state_type], TRUE, points, npoints);

  npoints--;

  for (i = 0; i < npoints; i++)
    {
      if ((points[i].x == points[i+1].x) &&
	  (points[i].y == points[i+1].y))
	{
	  angle = 0;
	}
      else
	{
	  angle = atan2 (points[i+1].y - points[i].y,
			 points[i+1].x - points[i].x);
	}

      if ((angle > -pi_3_over_4) && (angle < pi_over_4))
	{
	  if (angle > -pi_over_4)
	    {
	      xadjust = 0;
	      yadjust = 1;
	    }
	  else
	    {
	      xadjust = 1;
	      yadjust = 0;
	    }

	  gdk_draw_line (window, gc1,
			 points[i].x-xadjust, points[i].y-yadjust,
			 points[i+1].x-xadjust, points[i+1].y-yadjust);
	  gdk_draw_line (window, gc3,
			 points[i].x, points[i].y,
			 points[i+1].x, points[i+1].y);
	}
      else
	{
	  if ((angle < -pi_3_over_4) || (angle > pi_3_over_4))
	    {
	      xadjust = 0;
	      yadjust = 1;
	    }
	  else
	    {
	      xadjust = 1;
	      yadjust = 0;
	    }

	  gdk_draw_line (window, gc4,
			 points[i].x+xadjust, points[i].y+yadjust,
			 points[i+1].x+xadjust, points[i+1].y+yadjust);
	  gdk_draw_line (window, gc2,
			 points[i].x, points[i].y,
			 points[i+1].x, points[i+1].y);
	}
    }
}

static void
gtk_default_draw_arrow (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			GtkArrowType   arrow_type,
			gint           fill,
			gint           x,
			gint           y,
			gint           width,
			gint           height)
{
  GdkGC *gc1;
  GdkGC *gc2;
  GdkGC *gc3;
  GdkGC *gc4;
  gint half_width;
  gint half_height;
  GdkPoint points[3];

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      gc1 = style->bg_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->black_gc;
      gc4 = style->bg_gc[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = NULL;
      gc4 = NULL;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = NULL;
      gc4 = NULL;
      break;
    default:
      return;
    }

  if ((width == -1) && (height == -1))
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);

  half_width = width / 2;
  half_height = height / 2;

  switch (arrow_type)
    {
    case GTK_ARROW_UP:
      if (fill)
        {
          points[0].x = x + half_width;
          points[0].y = y;
          points[1].x = x;
          points[1].y = y + height - 1;
          points[2].x = x + width - 1;
          points[2].y = y + height - 1;

          gdk_draw_polygon (window, style->bg_gc[state_type], TRUE, points, 3);
        }

      switch (shadow_type)
	{
	case GTK_SHADOW_IN:
	case GTK_SHADOW_OUT:
	  
	  gdk_draw_line (window, gc1,
			 x + 1, y + height - 2,
			 x + width - 2, y + height - 2);
	  gdk_draw_line (window, gc3,
			 x + 0, y + height - 1,
			 x + width - 1, y + height - 1);
	  
	  gdk_draw_line (window, gc1,
			 x + width - 2, y + height - 1,
			 x + half_width, y + 1);
	  gdk_draw_line (window, gc3,
			 x + width - 1, y + height - 1,
			 x + half_width, y);
	  
	  gdk_draw_line (window, gc4,
			 x + half_width, y + 1,
			 x + 1, y + height - 1);
	  gdk_draw_line (window, gc2,
			 x + half_width, y,
			 x, y + height - 1);
	  break;

	case GTK_SHADOW_ETCHED_IN:
	case GTK_SHADOW_ETCHED_OUT:
	  gdk_draw_line (window, gc1,
			 x + half_width, y + 1,
			 x + 1, y + height - 1);
	  gdk_draw_line (window, gc1,
			 x + 1, y + height - 1,
			 x + width - 1, y + height - 1);
	  gdk_draw_line (window, gc1,
			 x + width - 1, y + height - 1,
			 x + half_width + 1, y + 1);
	  
          points[0].x = x + half_width;
          points[0].y = y;
          points[1].x = x;
          points[1].y = y + height - 2;
          points[2].x = x + width - 2;
          points[2].y = y + height - 2;

          gdk_draw_polygon (window, gc2, FALSE, points, 3);
	  break;

	default:
	  break;
	}
      break;

    case GTK_ARROW_DOWN:
      if (fill)
        {
          points[0].x = x + width - 1;
          points[0].y = y;
          points[1].x = x;
          points[1].y = y;
          points[2].x = x + half_width;
          points[2].y = y + height - 1;

          gdk_draw_polygon (window, style->bg_gc[state_type], TRUE, points, 3);
        }
      switch (shadow_type)
	{
	case GTK_SHADOW_IN:
	case GTK_SHADOW_OUT:
	  gdk_draw_line (window, gc4,
			 x + width - 2,
			 y + 1, x + 1, y + 1);
	  gdk_draw_line (window, gc2,
			 x + width - 1, y,
			 x, y);
	  
	  gdk_draw_line (window, gc4,
			 x + 1, y,
			 x + half_width, y + height - 2);
	  gdk_draw_line (window, gc2,
			 x, y,
			 x + half_width, y + height - 1);

	  gdk_draw_line (window, gc1,
			 x + half_width, y + height - 2,
			 x + width - 2, y);
	  gdk_draw_line (window, gc3,
			 x + half_width, y + height - 1,
			 x + width - 1, y);
	  break;

	case GTK_SHADOW_ETCHED_IN:
	case GTK_SHADOW_ETCHED_OUT:
	  gdk_draw_line (window, gc1,
			 x + width - 1, y + 1,
			 x + 1, y + 1);
	  gdk_draw_line (window, gc1,
			 x + 1, y + 1,
			 x + half_width + 1, y + height - 1);
	  gdk_draw_line (window, gc1,
			 x + half_width + 1, y + height - 2,
			 x + width - 1, y);
	  
          points[0].x = x + width - 2;
          points[0].y = y;
          points[1].x = x;
          points[1].y = y;
          points[2].x = x + half_width;
          points[2].y = y + height - 2;

          gdk_draw_polygon (window, gc2, FALSE, points, 3);
	  break;

	default:
	  break;
	}
      break;
    case GTK_ARROW_LEFT:
      if (fill)
        {
          points[0].x = x;
          points[0].y = y + half_height;
          points[1].x = x + width - 1;
          points[1].y = y + height - 1;
          points[2].x = x + width - 1;
          points[2].y = y;

          gdk_draw_polygon (window, style->bg_gc[state_type], TRUE, points, 3);
        }

      switch (shadow_type)
	{
	case GTK_SHADOW_IN:
	case GTK_SHADOW_OUT:
	  gdk_draw_line (window, gc1,
			 x + 1, y + half_height,
			 x + width - 1, y + height - 1);
	  gdk_draw_line (window, gc3,
			 x, y + half_height,
			 x + width - 1, y + height - 1);

	  gdk_draw_line (window, gc1,
			 x + width - 2, y + height - 1,
			 x + width - 2, y + 1);
	  gdk_draw_line (window, gc3,
			 x + width - 1, y + height - 1,
			 x + width - 1, y);

	  gdk_draw_line (window, gc4,
			 x + width - 1, y + 1,
			 x + 1, y + half_height);
	  gdk_draw_line (window, gc2,
			 x + width - 1, y,
			 x, y + half_height);
	  break;

	case GTK_SHADOW_ETCHED_IN:
	case GTK_SHADOW_ETCHED_OUT:
	  gdk_draw_line (window, gc1,
			 x + width - 1, y + 1,
			 x + 1, y + half_height);
	  gdk_draw_line (window, gc1,
			 x + 1, y + half_height + 1,
			 x + width - 1, y + height - 1);
	  gdk_draw_line (window, gc1,
			 x + width - 1, y + height - 1,
			 x + width - 1, y + 1);
	  
          points[0].x = x + width - 2;
          points[0].y = y;
          points[1].x = x;
          points[1].y = y + half_height;
          points[2].x = x + width - 2;
          points[2].y = y + height - 2;

          gdk_draw_polygon (window, gc2, FALSE, points, 3);
	  break;

	default:
	  break;
	}
      break;
    case GTK_ARROW_RIGHT:
      if (fill)
        {
          points[0].x = x + width - 1;
          points[0].y = y + half_height;
          points[1].x = x;
          points[1].y = y;
          points[2].x = x;
          points[2].y = y + height - 1;

          gdk_draw_polygon (window, style->bg_gc[state_type], TRUE, points, 3);
        }

      switch (shadow_type)
	{
	case GTK_SHADOW_IN:
	case GTK_SHADOW_OUT:
	  gdk_draw_line (window, gc4,
			 x + width - 1, y + half_height,
			 x + 1, y + 1);
	  gdk_draw_line (window, gc2,
			 x + width - 1, y + half_height,
			 x, y);

	  gdk_draw_line (window, gc4,
			 x + 1, y + 1,
			 x + 1, y + height - 2);
	  gdk_draw_line (window, gc2,
			 x, y,
			 x, y + height - 1);

	  gdk_draw_line (window, gc1,
			 x + 1, y + height - 2,
			 x + width - 1, y + half_height);
	  gdk_draw_line (window, gc3,
			 x, y + height - 1,
			 x + width - 1, y + half_height);
	  break;

	case GTK_SHADOW_ETCHED_IN:
	case GTK_SHADOW_ETCHED_OUT:
	  gdk_draw_line (window, gc1,
			 x + width - 1, y + half_height + 1,
			 x + 1, y + 1);
	  gdk_draw_line (window, gc1,
			 x + 1, y + 1,
			 x + 1, y + height - 1);
	  gdk_draw_line (window, gc1,
			 x + 1, y + height - 1,
			 x + width - 1, y + half_height + 1);
	  
          points[0].x = x + width - 2;
          points[0].y = y + half_height;
          points[1].x = x;
          points[1].y = y;
          points[2].x = x;
          points[2].y = y + height - 1;

          gdk_draw_polygon (window, gc2, FALSE, points, 3);
	  break;

	default:
	  break;
	}
      break;
    }
}

static void
gtk_default_draw_diamond (GtkStyle      *style,
			  GdkWindow     *window,
			  GtkStateType   state_type,
			  GtkShadowType  shadow_type,
			  gint           x,
			  gint           y,
			  gint           width,
			  gint           height)
{
  gint half_width;
  gint half_height;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);

  half_width = width / 2;
  half_height = height / 2;

  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      gdk_draw_line (window, style->bg_gc[state_type],
                     x + 2, y + half_height,
                     x + half_width, y + height - 2);
      gdk_draw_line (window, style->bg_gc[state_type],
                     x + half_width, y + height - 2,
                     x + width - 2, y + half_height);
      gdk_draw_line (window, style->light_gc[state_type],
                     x + 1, y + half_height,
                     x + half_width, y + height - 1);
      gdk_draw_line (window, style->light_gc[state_type],
                     x + half_width, y + height - 1,
                     x + width - 1, y + half_height);
      gdk_draw_line (window, style->light_gc[state_type],
                     x, y + half_height,
                     x + half_width, y + height);
      gdk_draw_line (window, style->light_gc[state_type],
                     x + half_width, y + height,
                     x + width, y + half_height);

      gdk_draw_line (window, style->black_gc,
                     x + 2, y + half_height,
                     x + half_width, y + 2);
      gdk_draw_line (window, style->black_gc,
                     x + half_width, y + 2,
                     x + width - 2, y + half_height);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + 1, y + half_height,
                     x + half_width, y + 1);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + half_width, y + 1,
                     x + width - 1, y + half_height);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x, y + half_height,
                     x + half_width, y);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + half_width, y,
                     x + width, y + half_height);
      break;
    case GTK_SHADOW_OUT:
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + 2, y + half_height,
                     x + half_width, y + height - 2);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + half_width, y + height - 2,
                     x + width - 2, y + half_height);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + 1, y + half_height,
                     x + half_width, y + height - 1);
      gdk_draw_line (window, style->dark_gc[state_type],
                     x + half_width, y + height - 1,
                     x + width - 1, y + half_height);
      gdk_draw_line (window, style->black_gc,
                     x, y + half_height,
                     x + half_width, y + height);
      gdk_draw_line (window, style->black_gc,
                     x + half_width, y + height,
                     x + width, y + half_height);

      gdk_draw_line (window, style->bg_gc[state_type],
                     x + 2, y + half_height,
                     x + half_width, y + 2);
      gdk_draw_line (window, style->bg_gc[state_type],
                     x + half_width, y + 2,
                     x + width - 2, y + half_height);
      gdk_draw_line (window, style->light_gc[state_type],
                     x + 1, y + half_height,
                     x + half_width, y + 1);
      gdk_draw_line (window, style->light_gc[state_type],
                     x + half_width, y + 1,
                     x + width - 1, y + half_height);
      gdk_draw_line (window, style->light_gc[state_type],
                     x, y + half_height,
                     x + half_width, y);
      gdk_draw_line (window, style->light_gc[state_type],
                     x + half_width, y,
                     x + width, y + half_height);
      break;
    default:
      break;
    }
}


static void
gtk_default_draw_oval (GtkStyle      *style,
		       GdkWindow     *window,
		       GtkStateType   state_type,
		       GtkShadowType  shadow_type,
		       gint           x,
		       gint           y,
		       gint           width,
		       gint           height)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);
}

static void
gtk_default_draw_string (GtkStyle      *style,
			 GdkWindow     *window,
			 GtkStateType   state_type,
			 gint           x,
			 gint           y,
			 const gchar   *string)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if (state_type == GTK_STATE_INSENSITIVE)
    gdk_draw_string (window, style->font, style->white_gc, x + 1, y + 1, string);
  gdk_draw_string (window, style->font, style->fg_gc[state_type], x, y, string);
}


static void
gtk_style_shade (GdkColor *a,
		 GdkColor *b,
		 gdouble   k)
{
  gdouble red;
  gdouble green;
  gdouble blue;

  red = (gdouble) a->red / 65535.0;
  green = (gdouble) a->green / 65535.0;
  blue = (gdouble) a->blue / 65535.0;

  rgb_to_hls (&red, &green, &blue);

  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;

  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;

  hls_to_rgb (&red, &green, &blue);

  b->red = red * 65535.0;
  b->green = green * 65535.0;
  b->blue = blue * 65535.0;
}

static void
rgb_to_hls (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;

  red = *r;
  green = *g;
  blue = *b;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  l = (max + min) / 2;
  s = 0;
  h = 0;

  if (max != min)
    {
      if (l <= 0.5)
	s = (max - min) / (max + min);
      else
	s = (max - min) / (2 - max - min);

      delta = max -min;
      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2 + (blue - red) / delta;
      else if (blue == max)
	h = 4 + (red - green) / delta;

      h *= 60;
      if (h < 0.0)
	h += 360;
    }

  *r = h;
  *g = l;
  *b = s;
}

static void
hls_to_rgb (gdouble *h,
	    gdouble *l,
	    gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;

  lightness = *l;
  saturation = *s;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;

  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	r = m2;
      else if (hue < 240)
	r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	r = m1;

      hue = *h;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	g = m2;
      else if (hue < 240)
	g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	g = m1;

      hue = *h - 120;
      while (hue > 360)
	hue -= 360;
      while (hue < 0)
	hue += 360;

      if (hue < 60)
	b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
	b = m2;
      else if (hue < 240)
	b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
	b = m1;

      *h = r;
      *l = g;
      *s = b;
    }
}
