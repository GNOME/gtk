/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <math.h>
#include <string.h>
#include "gtkgc.h"
#include "gtkrc.h"
#include "gtkstyle.h"
#include "gtkwidget.h"
#include "gtkthemes.h"
#include "gtkiconfactory.h"

#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7


/* --- typedefs & structures --- */
typedef struct {
  GType       widget_type;
  GParamSpec *pspec;
  GValue      value;
} PropertyValue;


/* --- prototypes --- */
static void	 gtk_style_init			(GtkStyle	*style);
static void	 gtk_style_class_init		(GtkStyleClass	*klass);
static void	 gtk_style_finalize		(GObject	*object);
static void	 gtk_style_realize		(GtkStyle	*style,
						 GdkColormap	*colormap);
static void      gtk_style_real_realize        (GtkStyle	*style);
static void      gtk_style_real_unrealize      (GtkStyle	*style);
static void      gtk_style_real_copy           (GtkStyle	*style,
						GtkStyle	*src);
static void      gtk_style_real_set_background (GtkStyle	*style,
						GdkWindow	*window,
						GtkStateType	 state_type);
static GtkStyle *gtk_style_real_clone          (GtkStyle	*style);
static void      gtk_style_real_init_from_rc   (GtkStyle	*style,
                                                GtkRcStyle	*rc_style);
static GdkPixbuf *gtk_default_render_icon      (GtkStyle            *style,
                                                const GtkIconSource *source,
                                                GtkTextDirection     direction,
                                                GtkStateType         state,
                                                GtkIconSize          size,
                                                GtkWidget           *widget,
                                                const gchar         *detail);
static void gtk_default_draw_hline      (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x1,
					 gint             x2,
					 gint             y);
static void gtk_default_draw_vline      (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             y1,
					 gint             y2,
					 gint             x);
static void gtk_default_draw_shadow     (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_polygon    (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 GdkPoint        *points,
					 gint             npoints,
					 gboolean         fill);
static void gtk_default_draw_arrow      (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 GtkArrowType     arrow_type,
					 gboolean         fill,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_diamond    (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_string     (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 const gchar     *string);
static void gtk_default_draw_box        (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_flat_box   (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_check      (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_option     (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_tab        (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_shadow_gap (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkPositionType  gap_side,
					 gint             gap_x,
					 gint             gap_width);
static void gtk_default_draw_box_gap    (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkPositionType  gap_side,
					 gint             gap_x,
					 gint             gap_width);
static void gtk_default_draw_extension  (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkPositionType  gap_side);
static void gtk_default_draw_focus      (GtkStyle        *style,
					 GdkWindow       *window,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_slider     (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkOrientation   orientation);
static void gtk_default_draw_handle     (GtkStyle        *style,
					 GdkWindow       *window,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GdkRectangle    *area,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkOrientation   orientation);
static void gtk_default_draw_expander   (GtkStyle        *style,
                                         GdkWindow       *window,
                                         GtkStateType     state_type,
                                         GdkRectangle    *area,
                                         GtkWidget       *widget,
                                         const gchar     *detail,
                                         gint             x,
                                         gint             y,
                                         gboolean         is_open);
static void gtk_default_draw_layout     (GtkStyle        *style,
                                         GdkWindow       *window,
                                         GtkStateType     state_type,
                                         GdkRectangle    *area,
                                         GtkWidget       *widget,
                                         const gchar     *detail,
                                         gint             x,
                                         gint             y,
                                         PangoLayout     *layout);
static void gtk_default_draw_resize_grip (GtkStyle       *style,
                                          GdkWindow      *window,
                                          GtkStateType    state_type,
                                          GdkRectangle   *area,
                                          GtkWidget      *widget,
                                          const gchar    *detail,
                                          GdkWindowEdge   edge,
                                          gint            x,
                                          gint            y,
                                          gint            width,
                                          gint            height);

static void gtk_style_shade		(GdkColor	 *a,
					 GdkColor	 *b,
					 gdouble	  k);
static void rgb_to_hls			(gdouble	 *r,
					 gdouble	 *g,
					 gdouble	 *b);
static void hls_to_rgb			(gdouble	 *h,
					 gdouble	 *l,
					 gdouble	 *s);


/* --- variables --- */
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

static gpointer parent_class = NULL;
static GdkFont *static_default_font = NULL;


/* --- functions --- */
GType
gtk_style_get_type (void)
{
  static GType style_type = 0;
  
  if (!style_type)
    {
      static const GTypeInfo style_info =
      {
        sizeof (GtkStyleClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_style_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkStyle),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_style_init,
      };
      
      style_type = g_type_register_static (G_TYPE_OBJECT,
					   "GtkStyle",
					   &style_info, 0);
    }
  
  return style_type;
}

static void
gtk_style_init (GtkStyle *style)
{
  gint i;
  
  style->font_desc = pango_font_description_from_string ("Sans 10");

  if (!static_default_font)
    {
      static_default_font = gdk_font_from_description (style->font_desc);

      if (!static_default_font) 
	static_default_font = gdk_font_load ("fixed");

      if (!static_default_font) 
	g_error ("Unable to load \"fixed\" font");
    }
  
  style->font = static_default_font;
  gdk_font_ref (style->font);

  style->attach_count = 0;
  style->colormap = NULL;
  style->depth = -1;
  
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
  
  style->base[GTK_STATE_INSENSITIVE] = gtk_default_prelight_bg;
  style->text[GTK_STATE_INSENSITIVE] = gtk_default_insensitive_fg;
  
  for (i = 0; i < 5; i++)
    style->bg_pixmap[i] = NULL;
  
  style->rc_style = NULL;
  
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

  style->xthickness = 2;
  style->ythickness = 2;

  style->property_cache = NULL;
}

static void
gtk_style_class_init (GtkStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gtk_style_finalize;

  klass->clone = gtk_style_real_clone;
  klass->copy = gtk_style_real_copy;
  klass->init_from_rc = gtk_style_real_init_from_rc;
  klass->realize = gtk_style_real_realize;
  klass->unrealize = gtk_style_real_unrealize;
  klass->set_background = gtk_style_real_set_background;
  klass->render_icon = gtk_default_render_icon;

  klass->draw_hline = gtk_default_draw_hline;
  klass->draw_vline = gtk_default_draw_vline;
  klass->draw_shadow = gtk_default_draw_shadow;
  klass->draw_polygon = gtk_default_draw_polygon;
  klass->draw_arrow = gtk_default_draw_arrow;
  klass->draw_diamond = gtk_default_draw_diamond;
  klass->draw_string = gtk_default_draw_string;
  klass->draw_box = gtk_default_draw_box;
  klass->draw_flat_box = gtk_default_draw_flat_box;
  klass->draw_check = gtk_default_draw_check;
  klass->draw_option = gtk_default_draw_option;
  klass->draw_tab = gtk_default_draw_tab;
  klass->draw_shadow_gap = gtk_default_draw_shadow_gap;
  klass->draw_box_gap = gtk_default_draw_box_gap;
  klass->draw_extension = gtk_default_draw_extension;
  klass->draw_focus = gtk_default_draw_focus;
  klass->draw_slider = gtk_default_draw_slider;
  klass->draw_handle = gtk_default_draw_handle;
  klass->draw_expander = gtk_default_draw_expander;
  klass->draw_layout = gtk_default_draw_layout;
  klass->draw_resize_grip = gtk_default_draw_resize_grip;
}

static void
gtk_style_finalize (GObject *object)
{
  GtkStyle *style = GTK_STYLE (object);

  g_return_if_fail (style->attach_count == 0);

  if (style->property_cache)
    {
      guint i;

      for (i = 0; i < style->property_cache->n_nodes; i++)
	{
	  PropertyValue *node = g_bsearch_array_get_nth (style->property_cache, i);

	  g_param_spec_unref (node->pspec);
	  g_value_unset (&node->value);
	}
      g_bsearch_array_destroy (style->property_cache);
      style->property_cache = NULL;
    }
  
  if (style->styles)
    {
      if (style->styles->data != style)
        g_slist_remove (style->styles, style);
      else
        {
          GSList *tmp_list = style->styles->next;
	  
          while (tmp_list)
            {
              GTK_STYLE (tmp_list->data)->styles = style->styles->next;
              tmp_list = tmp_list->next;
            }
          g_slist_free_1 (style->styles);
        }
    }
  
  gdk_font_unref (style->font);
  pango_font_description_free (style->font_desc);
  
  if (style->rc_style)
    gtk_rc_style_unref (style->rc_style);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


GtkStyle*
gtk_style_copy (GtkStyle *style)
{
  GtkStyle *new_style;
  
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  
  new_style = GTK_STYLE_GET_CLASS (style)->clone (style);
  GTK_STYLE_GET_CLASS (style)->copy (new_style, style);

  return new_style;
}

static GtkStyle*
gtk_style_duplicate (GtkStyle *style)
{
  GtkStyle *new_style;
  
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  
  new_style = gtk_style_copy (style);
  
  style->styles = g_slist_append (style->styles, new_style);
  new_style->styles = style->styles;  
  
  return new_style;
}

GtkStyle*
gtk_style_new (void)
{
  GtkStyle *style;
  
  style = g_object_new (GTK_TYPE_STYLE, NULL);
  
  return style;
}

/*************************************************************
 * gtk_style_attach:
 *     Attach a style to a window; this process allocates the
 *     colors and creates the GC's for the style - it specializes
 *     it to a particular visual and colormap. The process
 *     may involve the creation of a new style if the style
 *     has already been attached to a window with a different
 *     style and colormap.
 *   arguments:
 *     style:
 *     window: 
 *   results:
 *     Either the style parameter, or a newly created style.
 *     If the style is newly created, the style parameter
 *     will be dereferenced, and the new style will have
 *     a reference count belonging to the caller.
 *
 * FIXME: The sequence - 
 *    create a style => s1
 *    attach s1 to v1, c1 => s1
 *    attach s1 to v2, c2 => s2
 *    detach s1 from v1, c1
 *    attach s1 to v2, c2 => s3
 * results in two separate, unlinked styles s2 and s3 which
 * are identical and could be shared. To fix this, we would
 * want to never remove a style from the list of linked
 * styles as long as as it has a reference count. However, the 
 * disadvantage of doing it this way means that we would need two 
 * passes through the linked list when attaching (one to check for 
 * matching styles, one to look for empty unattached styles - but 
 * it will almost never be longer than 2 elements.
 *************************************************************/

GtkStyle*
gtk_style_attach (GtkStyle  *style,
                  GdkWindow *window)
{
  GSList *styles;
  GtkStyle *new_style = NULL;
  GdkColormap *colormap;
  
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (window != NULL, NULL);
  
  colormap = gdk_window_get_colormap (window);
  
  if (!style->styles)
    style->styles = g_slist_append (NULL, style);
  
  styles = style->styles;
  while (styles)
    {
      new_style = styles->data;
      
      if (new_style->attach_count == 0)
        {
          gtk_style_realize (new_style, colormap);
          break;
        }
      else if (new_style->colormap == colormap)
        break;
      
      new_style = NULL;
      styles = styles->next;
    }
  
  if (!new_style)
    {
      new_style = gtk_style_duplicate (style);
      gtk_style_realize (new_style, colormap);
    }

  /* A style gets a refcount from being attached */
  if (new_style->attach_count == 0)
    gtk_style_ref (new_style);

  /* Another refcount belongs to the parent */
  if (style != new_style) 
    {
      gtk_style_unref (style);
      gtk_style_ref (new_style);
    }
  
  new_style->attach_count++;
  
  return new_style;
}

void
gtk_style_detach (GtkStyle *style)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  
  style->attach_count -= 1;
  if (style->attach_count == 0)
    {
      GTK_STYLE_GET_CLASS (style)->unrealize (style);
      
      gdk_colormap_unref (style->colormap);
      style->colormap = NULL;
      
      gtk_style_unref (style);
    }
}

GtkStyle*
gtk_style_ref (GtkStyle *style)
{
  return (GtkStyle *) g_object_ref (G_OBJECT (style));
}

void
gtk_style_unref (GtkStyle *style)
{
  g_object_unref (G_OBJECT (style));
}

static void
gtk_style_realize (GtkStyle    *style,
                   GdkColormap *colormap)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  
  style->colormap = gdk_colormap_ref (colormap);
  style->depth = gdk_colormap_get_visual (colormap)->depth;

  GTK_STYLE_GET_CLASS (style)->realize (style);
}

GtkIconSet*
gtk_style_lookup_icon_set (GtkStyle   *style,
                           const char *stock_id)
{
  GSList *iter;

  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  
  iter = style->icon_factories;
  while (iter != NULL)
    {
      GtkIconSet *icon_set = gtk_icon_factory_lookup (GTK_ICON_FACTORY (iter->data),
						      stock_id);
      if (icon_set)
        return icon_set;
      
      iter = g_slist_next (iter);
    }

  return gtk_icon_factory_lookup_default (stock_id);
}

void
gtk_draw_hline (GtkStyle     *style,
                GdkWindow    *window,
                GtkStateType  state_type,
                gint          x1,
                gint          x2,
                gint          y)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_hline != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_hline (style, window, state_type, NULL, NULL, NULL, x1, x2, y);
}


void
gtk_draw_vline (GtkStyle     *style,
                GdkWindow    *window,
                GtkStateType  state_type,
                gint          y1,
                gint          y2,
                gint          x)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_vline != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_vline (style, window, state_type, NULL, NULL, NULL, y1, y2, x);
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_shadow (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}

void
gtk_draw_polygon (GtkStyle      *style,
                  GdkWindow     *window,
                  GtkStateType   state_type,
                  GtkShadowType  shadow_type,
                  GdkPoint      *points,
                  gint           npoints,
                  gboolean       fill)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_polygon != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_polygon (style, window, state_type, shadow_type, NULL, NULL, NULL, points, npoints, fill);
}

void
gtk_draw_arrow (GtkStyle      *style,
                GdkWindow     *window,
                GtkStateType   state_type,
                GtkShadowType  shadow_type,
                GtkArrowType   arrow_type,
                gboolean       fill,
                gint           x,
                gint           y,
                gint           width,
                gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_arrow != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_arrow (style, window, state_type, shadow_type, NULL, NULL, NULL, arrow_type, fill, x, y, width, height);
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_diamond != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_diamond (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}


void
gtk_draw_string (GtkStyle      *style,
                 GdkWindow     *window,
                 GtkStateType   state_type,
                 gint           x,
                 gint           y,
                 const gchar   *string)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_string != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_string (style, window, state_type, NULL, NULL, NULL, x, y, string);
}

void
gtk_draw_box (GtkStyle      *style,
              GdkWindow     *window,
              GtkStateType   state_type,
              GtkShadowType  shadow_type,
              gint           x,
              gint           y,
              gint           width,
              gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_box != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_box (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}

void
gtk_draw_flat_box (GtkStyle      *style,
                   GdkWindow     *window,
                   GtkStateType   state_type,
                   GtkShadowType  shadow_type,
                   gint           x,
                   gint           y,
                   gint           width,
                   gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_flat_box != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_flat_box (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}

void
gtk_draw_check (GtkStyle      *style,
                GdkWindow     *window,
                GtkStateType   state_type,
                GtkShadowType  shadow_type,
                gint           x,
                gint           y,
                gint           width,
                gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_check != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_check (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}

void
gtk_draw_option (GtkStyle      *style,
		 GdkWindow     *window,
		 GtkStateType   state_type,
		 GtkShadowType  shadow_type,
		 gint           x,
		 gint           y,
		 gint           width,
		 gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_option != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_option (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}

void
gtk_draw_tab (GtkStyle      *style,
	      GdkWindow     *window,
	      GtkStateType   state_type,
	      GtkShadowType  shadow_type,
	      gint           x,
	      gint           y,
	      gint           width,
	      gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_tab != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_tab (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height);
}

void
gtk_draw_shadow_gap (GtkStyle       *style,
                     GdkWindow      *window,
                     GtkStateType    state_type,
                     GtkShadowType   shadow_type,
                     gint            x,
                     gint            y,
                     gint            width,
                     gint            height,
                     GtkPositionType gap_side,
                     gint            gap_x,
                     gint            gap_width)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow_gap != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_shadow_gap (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height, gap_side, gap_x, gap_width);
}

void
gtk_draw_box_gap (GtkStyle       *style,
                  GdkWindow      *window,
                  GtkStateType    state_type,
                  GtkShadowType   shadow_type,
                  gint            x,
                  gint            y,
                  gint            width,
                  gint            height,
                  GtkPositionType gap_side,
                  gint            gap_x,
                  gint            gap_width)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_box_gap != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_box_gap (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height, gap_side, gap_x, gap_width);
}

void
gtk_draw_extension (GtkStyle       *style,
                    GdkWindow      *window,
                    GtkStateType    state_type,
                    GtkShadowType   shadow_type,
                    gint            x,
                    gint            y,
                    gint            width,
                    gint            height,
                    GtkPositionType gap_side)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_extension != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_extension (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height, gap_side);
}

void
gtk_draw_focus (GtkStyle      *style,
		GdkWindow     *window,
		gint           x,
		gint           y,
		gint           width,
		gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_focus != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_focus (style, window, NULL, NULL, NULL, x, y, width, height);
}

void 
gtk_draw_slider (GtkStyle      *style,
		 GdkWindow     *window,
		 GtkStateType   state_type,
		 GtkShadowType  shadow_type,
		 gint           x,
		 gint           y,
		 gint           width,
		 gint           height,
		 GtkOrientation orientation)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_slider != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_slider (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height, orientation);
}

void 
gtk_draw_handle (GtkStyle      *style,
		 GdkWindow     *window,
		 GtkStateType   state_type,
		 GtkShadowType  shadow_type,
		 gint           x,
		 gint           y,
		 gint           width,
		 gint           height,
		 GtkOrientation orientation)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_handle != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_handle (style, window, state_type, shadow_type, NULL, NULL, NULL, x, y, width, height, orientation);
}

void
gtk_draw_expander (GtkStyle        *style,
                   GdkWindow       *window,
                   GtkStateType     state_type,
                   gint             x,
                   gint             y,
                   gboolean         is_open)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_expander != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_expander (style, window, state_type,
                                              NULL, NULL, NULL,
                                              x, y, is_open);
}

void
gtk_draw_layout (GtkStyle        *style,
                 GdkWindow       *window,
                 GtkStateType     state_type,
                 gint             x,
                 gint             y,
                 PangoLayout     *layout)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_layout != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_layout (style, window, state_type,
                                            NULL, NULL, NULL,
                                            x, y, layout);
}

void
gtk_draw_resize_grip (GtkStyle     *style,
                      GdkWindow    *window,
                      GtkStateType  state_type,
                      GdkWindowEdge edge,
                      gint          x,
                      gint          y,
                      gint          width,
                      gint          height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_resize_grip != NULL);

  GTK_STYLE_GET_CLASS (style)->draw_resize_grip (style, window, state_type,
                                                 NULL, NULL, NULL,
                                                 edge,
                                                 x, y, width, height);
}


void
gtk_style_set_background (GtkStyle    *style,
                          GdkWindow   *window,
                          GtkStateType state_type)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  GTK_STYLE_GET_CLASS (style)->set_background (style, window, state_type);
}

/* Default functions */
static GtkStyle *
gtk_style_real_clone (GtkStyle *style)
{
  return GTK_STYLE (g_object_new (G_OBJECT_TYPE (style), NULL));
}

static void
gtk_style_real_copy (GtkStyle *style,
		     GtkStyle *src)
{
  gint i;
  
  for (i = 0; i < 5; i++)
    {
      style->fg[i] = src->fg[i];
      style->bg[i] = src->bg[i];
      style->text[i] = src->text[i];
      style->base[i] = src->base[i];
      
      style->bg_pixmap[i] = src->bg_pixmap[i];
    }

  if (style->font)
    gdk_font_unref (style->font);
  style->font = src->font;
  if (style->font)
    gdk_font_ref (style->font);

  if (style->font_desc)
    pango_font_description_free (style->font_desc);
  if (src->font_desc)
    style->font_desc = pango_font_description_copy (src->font_desc);
  else
    style->font_desc = NULL;
  
  style->xthickness = src->xthickness;
  style->ythickness = src->ythickness;

  if (style->rc_style)
    gtk_rc_style_unref (style->rc_style);
  style->rc_style = src->rc_style;
  if (src->rc_style)
    gtk_rc_style_ref (src->rc_style);

  /* don't copy, just clear cache */
  if (style->property_cache)
    {
      guint i;

      for (i = 0; i < style->property_cache->n_nodes; i++)
	{
	  PropertyValue *node = g_bsearch_array_get_nth (style->property_cache, i);

	  g_param_spec_unref (node->pspec);
	  g_value_unset (&node->value);
	}
      g_bsearch_array_destroy (style->property_cache);
      style->property_cache = NULL;
    }
}

static void
gtk_style_real_init_from_rc (GtkStyle   *style,
			     GtkRcStyle *rc_style)
{
  GdkFont *old_font;
  gint i;

  /* cache _should_ be still empty */
  if (style->property_cache)
    {
      guint i;

      for (i = 0; i < style->property_cache->n_nodes; i++)
	{
	  PropertyValue *node = g_bsearch_array_get_nth (style->property_cache, i);

	  g_param_spec_unref (node->pspec);
	  g_value_unset (&node->value);
	}
      g_bsearch_array_destroy (style->property_cache);
      style->property_cache = NULL;
    }

  if (rc_style->font_desc)
    {
      pango_font_description_free (style->font_desc);
      style->font_desc = pango_font_description_copy (rc_style->font_desc);

      old_font = style->font;
      style->font = gdk_font_from_description (style->font_desc);
      if (style->font)
	gdk_font_unref (old_font);
      else
	style->font = old_font;
    }
    
  for (i = 0; i < 5; i++)
    {
      if (rc_style->color_flags[i] & GTK_RC_FG)
	style->fg[i] = rc_style->fg[i];
      if (rc_style->color_flags[i] & GTK_RC_BG)
	style->bg[i] = rc_style->bg[i];
      if (rc_style->color_flags[i] & GTK_RC_TEXT)
	style->text[i] = rc_style->text[i];
      if (rc_style->color_flags[i] & GTK_RC_BASE)
	style->base[i] = rc_style->base[i];
    }

  if (rc_style->xthickness >= 0)
    style->xthickness = rc_style->xthickness;
  if (rc_style->ythickness >= 0)
    style->ythickness = rc_style->ythickness;

  if (rc_style->icon_factories)
    {
      GSList *iter;

      style->icon_factories = g_slist_copy (rc_style->icon_factories);
      
      iter = style->icon_factories;
      while (iter != NULL)
        {
          g_object_ref (G_OBJECT (iter->data));
          
          iter = g_slist_next (iter);
        }
    }
}

static gint
style_property_values_cmp (gconstpointer bsearch_node1,
			   gconstpointer bsearch_node2)
{
  const PropertyValue *val1 = bsearch_node1;
  const PropertyValue *val2 = bsearch_node2;
  gint cmp;

  cmp = G_BSEARCH_ARRAY_CMP (val1->widget_type, val2->widget_type);
  if (cmp == 0)
    cmp = G_BSEARCH_ARRAY_CMP (val1->pspec, val2->pspec);

  return cmp;
}

const GValue*
_gtk_style_peek_property_value (GtkStyle           *style,
				GType               widget_type,
				GParamSpec         *pspec,
				GtkRcPropertyParser parser)
{
  PropertyValue *pcache, key = { 0, NULL, { 0, } };
  const GtkRcProperty *rcprop = NULL;

  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (g_type_is_a (pspec->owner_type, GTK_TYPE_WIDGET), NULL);
  g_return_val_if_fail (g_type_is_a (widget_type, pspec->owner_type), NULL);

  /* need value cache array */
  if (!style->property_cache)
    style->property_cache = g_bsearch_array_new (sizeof (PropertyValue),
						 style_property_values_cmp,
						 0);
  /* lookup, or insert value if not yet present */
  key.widget_type = widget_type;
  key.pspec = pspec;
  pcache = g_bsearch_array_insert (style->property_cache, &key, FALSE);
  if (G_VALUE_TYPE (&pcache->value))
    return &pcache->value;

  /* cache miss, initialize value type, then set contents */
  g_value_init (&pcache->value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  /* value provided by rc style? */
  if (style->rc_style)
    {
      GQuark prop_quark = g_quark_from_string (pspec->name);

      do
	{
	  rcprop = _gtk_rc_style_lookup_rc_property (style->rc_style,
						     g_type_qname (widget_type),
						     prop_quark);
	  if (rcprop)
	    break;
	  widget_type = g_type_parent (widget_type);
	}
      while (g_type_is_a (widget_type, pspec->owner_type));
    }

  /* when supplied by rc style, we need to convert */
  if (rcprop)
    {
      if (G_VALUE_TYPE (&rcprop->value) == G_TYPE_GSTRING)
	{
	  GString *gstring;
	  
	  /* value still unparsed, need to revert to user supplied parser function */
	  
	  gstring = g_value_get_boxed (&rcprop->value);
	  
	  if (!parser || !parser (pspec, gstring, &pcache->value) ||
	      g_param_value_validate (pspec, &pcache->value))
	    {
	      gchar *contents = g_strescape (gstring->str, NULL);

	      g_message ("%s: failed to parse property `%s::%s' of type `%s' from rc file value \"%s\"",
			 rcprop->origin,
			 g_type_name (pspec->owner_type), pspec->name,
			 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
			 gstring->str);
	      g_free (contents);
	      rcprop = NULL;
	    }
	}
      else
	{
	  /* we use the normal conversion functionality of param specs */
	  if (!g_param_value_convert (pspec, &rcprop->value, &pcache->value, TRUE))
	    {
	      gchar *contents = g_strdup_value_contents (&rcprop->value);
	      
	      g_message ("%s: failed to retrive property `%s::%s' of type `%s' from rc file value \"%s\" of type `%s'",
			 rcprop->origin,
			 g_type_name (pspec->owner_type), pspec->name,
			 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
			 contents,
			 G_VALUE_TYPE_NAME (&rcprop->value));
	      g_free (contents);
	      rcprop = NULL;
	    }
	}
    }
  
  /* not supplied by rc style (or conversion failed), revert to default */
  if (!rcprop)
    g_param_value_set_default (pspec, &pcache->value);

  return &pcache->value;
}

static void
gtk_style_real_realize (GtkStyle *style)
{
  GdkGCValues gc_values;
  GdkGCValuesMask gc_values_mask;
  
  gint i;

  for (i = 0; i < 5; i++)
    {
      gtk_style_shade (&style->bg[i], &style->light[i], LIGHTNESS_MULT);
      gtk_style_shade (&style->bg[i], &style->dark[i], DARKNESS_MULT);
      
      style->mid[i].red = (style->light[i].red + style->dark[i].red) / 2;
      style->mid[i].green = (style->light[i].green + style->dark[i].green) / 2;
      style->mid[i].blue = (style->light[i].blue + style->dark[i].blue) / 2;
    }
  
  gdk_color_black (style->colormap, &style->black);
  gdk_color_white (style->colormap, &style->white);
  
  gc_values_mask = GDK_GC_FOREGROUND | GDK_GC_FONT;
  if (style->font->type == GDK_FONT_FONT)
    {
      gc_values.font = style->font;
    }
  else if (style->font->type == GDK_FONT_FONTSET)
    {
      gc_values.font = static_default_font;
    }
  
  gc_values.foreground = style->black;
  style->black_gc = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);
  
  gc_values.foreground = style->white;
  style->white_gc = gtk_gc_get (style->depth, style->colormap, &gc_values, gc_values_mask);
  
  for (i = 0; i < 5; i++)
    {
      if (style->rc_style && style->rc_style->bg_pixmap_name[i])
        style->bg_pixmap[i] = gtk_rc_load_image (style->colormap,
                                                 &style->bg[i],
                                                 style->rc_style->bg_pixmap_name[i]);
      
      if (!gdk_color_alloc (style->colormap, &style->fg[i]))
        g_warning ("unable to allocate color: ( %d %d %d )",
                   style->fg[i].red, style->fg[i].green, style->fg[i].blue);
      if (!gdk_color_alloc (style->colormap, &style->bg[i]))
        g_warning ("unable to allocate color: ( %d %d %d )",
                   style->bg[i].red, style->bg[i].green, style->bg[i].blue);
      if (!gdk_color_alloc (style->colormap, &style->light[i]))
        g_warning ("unable to allocate color: ( %d %d %d )",
                   style->light[i].red, style->light[i].green, style->light[i].blue);
      if (!gdk_color_alloc (style->colormap, &style->dark[i]))
        g_warning ("unable to allocate color: ( %d %d %d )",
                   style->dark[i].red, style->dark[i].green, style->dark[i].blue);
      if (!gdk_color_alloc (style->colormap, &style->mid[i]))
        g_warning ("unable to allocate color: ( %d %d %d )",
                   style->mid[i].red, style->mid[i].green, style->mid[i].blue);
      if (!gdk_color_alloc (style->colormap, &style->text[i]))
        g_warning ("unable to allocate color: ( %d %d %d )",
                   style->text[i].red, style->text[i].green, style->text[i].blue);
      if (!gdk_color_alloc (style->colormap, &style->base[i]))
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

static void
gtk_style_real_unrealize (GtkStyle *style)
{
  int i;

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

      if (style->bg_pixmap[i] &&  style->bg_pixmap[i] != (GdkPixmap*) GDK_PARENT_RELATIVE)
	gdk_pixmap_unref (style->bg_pixmap[i]);
    }
  
  gdk_colormap_free_colors (style->colormap, style->fg, 5);
  gdk_colormap_free_colors (style->colormap, style->bg, 5);
  gdk_colormap_free_colors (style->colormap, style->light, 5);
  gdk_colormap_free_colors (style->colormap, style->dark, 5);
  gdk_colormap_free_colors (style->colormap, style->mid, 5);
  gdk_colormap_free_colors (style->colormap, style->text, 5);
  gdk_colormap_free_colors (style->colormap, style->base, 5);
}

static void
gtk_style_real_set_background (GtkStyle    *style,
			       GdkWindow   *window,
			       GtkStateType state_type)
{
  GdkPixmap *pixmap;
  gint parent_relative;
  
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

GdkPixbuf *
gtk_style_render_icon (GtkStyle            *style,
                       const GtkIconSource *source,
                       GtkTextDirection     direction,
                       GtkStateType         state,
                       GtkIconSize          size,
                       GtkWidget           *widget,
                       const gchar         *detail)
{
  GdkPixbuf *pixbuf;
  
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (GTK_STYLE_GET_CLASS (style)->render_icon != NULL, NULL);
  
  pixbuf = GTK_STYLE_GET_CLASS (style)->render_icon (style, source, direction, state,
                                                     size, widget, detail);

  g_return_val_if_fail (pixbuf != NULL, NULL);

  return pixbuf;
}

/* Default functions */
void
gtk_style_apply_default_background (GtkStyle     *style,
                                    GdkWindow    *window,
                                    gboolean      set_bg,
                                    GtkStateType  state_type, 
                                    GdkRectangle *area, 
                                    gint          x, 
                                    gint          y, 
                                    gint          width, 
                                    gint          height)
{
  GdkRectangle new_rect, old_rect;
  
  if (area)
    {
      old_rect.x = x;
      old_rect.y = y;
      old_rect.width = width;
      old_rect.height = height;
      
      if (!gdk_rectangle_intersect (area, &old_rect, &new_rect))
        return;
    }
  else
    {
      new_rect.x = x;
      new_rect.y = y;
      new_rect.width = width;
      new_rect.height = height;
    }
  
  if (!style->bg_pixmap[state_type] ||
      GDK_IS_PIXMAP (window) ||
      (!set_bg && style->bg_pixmap[state_type] != (GdkPixmap*) GDK_PARENT_RELATIVE))
    {
      GdkGC *gc = style->bg_gc[state_type];
      
      if (style->bg_pixmap[state_type])
        {
          gdk_gc_set_fill (gc, GDK_TILED);
          gdk_gc_set_tile (gc, style->bg_pixmap[state_type]);
        }
      
      gdk_draw_rectangle (window, gc, TRUE, 
                          new_rect.x, new_rect.y, new_rect.width, new_rect.height);
      if (style->bg_pixmap[state_type])
        gdk_gc_set_fill (gc, GDK_SOLID);
    }
  else
    {
      if (set_bg)
        {
          if (style->bg_pixmap[state_type] == (GdkPixmap*) GDK_PARENT_RELATIVE)
            gdk_window_set_back_pixmap (window, NULL, TRUE);
          else
            gdk_window_set_back_pixmap (window, style->bg_pixmap[state_type], FALSE);
        }
      
      gdk_window_clear_area (window, 
                             new_rect.x, new_rect.y, 
                             new_rect.width, new_rect.height);
    }
}

static GdkPixbuf*
scale_or_ref (GdkPixbuf *src,
              gint width,
              gint height)
{
  if (width == gdk_pixbuf_get_width (src) &&
      height == gdk_pixbuf_get_height (src))
    {
      gdk_pixbuf_ref (src);
      return src;
    }
  else
    {
      return gdk_pixbuf_scale_simple (src,
                                      width, height,
                                      GDK_INTERP_BILINEAR);
    }
}

static GdkPixbuf *
gtk_default_render_icon (GtkStyle            *style,
                         const GtkIconSource *source,
                         GtkTextDirection     direction,
                         GtkStateType         state,
                         GtkIconSize          size,
                         GtkWidget           *widget,
                         const gchar         *detail)
{
  gint width = 1;
  gint height = 1;
  GdkPixbuf *scaled;
  GdkPixbuf *stated;
  GdkPixbuf *base_pixbuf;

  /* Oddly, style can be NULL in this function, because
   * GtkIconSet can be used without a style and if so
   * it uses this function.
   */

  base_pixbuf = gtk_icon_source_get_pixbuf (source);

  g_return_val_if_fail (base_pixbuf != NULL, NULL);
  
  if (!gtk_icon_size_lookup (size, &width, &height))
    {
      g_warning (G_STRLOC ": invalid icon size `%d'", size);
      return NULL;
    }

  /* If the size was wildcarded, then scale; otherwise, leave it
   * alone.
   */
  if (gtk_icon_source_get_size_wildcarded (source))
    scaled = scale_or_ref (base_pixbuf, width, height);
  else
    scaled = GDK_PIXBUF (g_object_ref (G_OBJECT (base_pixbuf)));

  /* If the state was wildcarded, then generate a state. */
  if (gtk_icon_source_get_state_wildcarded (source))
    {
      if (state == GTK_STATE_INSENSITIVE)
        {
          stated = gdk_pixbuf_copy (scaled);      
          
          gdk_pixbuf_saturate_and_pixelate (scaled, stated,
                                            0.8, TRUE);
          
          gdk_pixbuf_unref (scaled);
        }
      else if (state == GTK_STATE_PRELIGHT)
        {
          stated = gdk_pixbuf_copy (scaled);      
          
          gdk_pixbuf_saturate_and_pixelate (scaled, stated,
                                            1.2, FALSE);
          
          gdk_pixbuf_unref (scaled);
        }
      else
        {
          stated = scaled;
        }
    }
  else
    stated = scaled;
  
  return stated;
}

static void
gtk_default_draw_hline (GtkStyle     *style,
                        GdkWindow    *window,
                        GtkStateType  state_type,
                        GdkRectangle  *area,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        gint          x1,
                        gint          x2,
                        gint          y)
{
  gint thickness_light;
  gint thickness_dark;
  gint i;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  thickness_light = style->ythickness / 2;
  thickness_dark = style->ythickness - thickness_light;
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], area);
    }
  
  if (detail && !strcmp (detail, "label"))
    {
      if (state_type == GTK_STATE_INSENSITIVE)
        gdk_draw_line (window, style->white_gc, x1 + 1, y + 1, x2 + 1, y + 1);   
      gdk_draw_line (window, style->fg_gc[state_type], x1, y, x2, y);     
    }
  else
    {
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
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], NULL);
    }
}


static void
gtk_default_draw_vline (GtkStyle     *style,
                        GdkWindow    *window,
                        GtkStateType  state_type,
                        GdkRectangle  *area,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        gint          y1,
                        gint          y2,
                        gint          x)
{
  gint thickness_light;
  gint thickness_dark;
  gint i;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  thickness_light = style->xthickness / 2;
  thickness_dark = style->xthickness - thickness_light;
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], area);
    }
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
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], NULL);
    }
}


static void
gtk_default_draw_shadow (GtkStyle      *style,
                         GdkWindow     *window,
                         GtkStateType   state_type,
                         GtkShadowType  shadow_type,
                         GdkRectangle  *area,
                         GtkWidget     *widget,
                         const gchar   *detail,
                         gint           x,
                         gint           y,
                         gint           width,
                         gint           height)
{
  GdkGC *gc1 = NULL;
  GdkGC *gc2 = NULL;
  gint thickness_light;
  gint thickness_dark;
  gint i;
  
  g_return_if_fail (GTK_IS_STYLE (style));
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
      return;
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
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, area);
      gdk_gc_set_clip_rectangle (gc2, area);
      if (shadow_type == GTK_SHADOW_IN || 
          shadow_type == GTK_SHADOW_OUT)
        {
          gdk_gc_set_clip_rectangle (style->black_gc, area);
          gdk_gc_set_clip_rectangle (style->bg_gc[state_type], area);
        }
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
  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, NULL);
      gdk_gc_set_clip_rectangle (gc2, NULL);
      if (shadow_type == GTK_SHADOW_IN || 
          shadow_type == GTK_SHADOW_OUT)
        {
          gdk_gc_set_clip_rectangle (style->black_gc, NULL);
          gdk_gc_set_clip_rectangle (style->bg_gc[state_type], NULL);
        }
    }
}

static void
gtk_default_draw_polygon (GtkStyle      *style,
                          GdkWindow     *window,
                          GtkStateType   state_type,
                          GtkShadowType  shadow_type,
                          GdkRectangle  *area,
                          GtkWidget     *widget,
                          const gchar   *detail,
                          GdkPoint      *points,
                          gint           npoints,
                          gboolean       fill)
{
  static const gdouble pi_over_4 = G_PI_4;
  static const gdouble pi_3_over_4 = G_PI_4 * 3;
  GdkGC *gc1;
  GdkGC *gc2;
  GdkGC *gc3;
  GdkGC *gc4;
  gdouble angle;
  gint xadjust;
  gint yadjust;
  gint i;
  
  g_return_if_fail (GTK_IS_STYLE (style));
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
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->black_gc;
      gc4 = style->bg_gc[state_type];
      break;
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->dark_gc[state_type];
      break;
    default:
      return;
    }
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, area);
      gdk_gc_set_clip_rectangle (gc2, area);
      gdk_gc_set_clip_rectangle (gc3, area);
      gdk_gc_set_clip_rectangle (gc4, area);
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

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, NULL);
      gdk_gc_set_clip_rectangle (gc2, NULL);
      gdk_gc_set_clip_rectangle (gc3, NULL);
      gdk_gc_set_clip_rectangle (gc4, NULL);
    }
}

static void
gtk_default_draw_arrow (GtkStyle      *style,
                        GdkWindow     *window,
                        GtkStateType   state_type,
                        GtkShadowType  shadow_type,
                        GdkRectangle  *area,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        GtkArrowType   arrow_type,
                        gboolean       fill,
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
  
  g_return_if_fail (GTK_IS_STYLE (style));
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
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, area);
      gdk_gc_set_clip_rectangle (gc2, area);
      if ((gc3) && (gc4))
        {
          gdk_gc_set_clip_rectangle (gc3, area);
          gdk_gc_set_clip_rectangle (gc4, area);
        }
    }
  
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

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, NULL);
      gdk_gc_set_clip_rectangle (gc2, NULL);
      if (gc3)
        {
          gdk_gc_set_clip_rectangle (gc3, NULL);
          gdk_gc_set_clip_rectangle (gc4, NULL);
        }
    }
}

static void
gtk_default_draw_diamond (GtkStyle      *style,
                          GdkWindow     *window,
                          GtkStateType   state_type,
                          GtkShadowType  shadow_type,
                          GdkRectangle  *area,
                          GtkWidget     *widget,
                          const gchar   *detail,
                          gint           x,
                          gint           y,
                          gint           width,
                          gint           height)
{
  gint half_width;
  gint half_height;
  GdkGC *outer_nw = NULL;
  GdkGC *outer_ne = NULL;
  GdkGC *outer_sw = NULL;
  GdkGC *outer_se = NULL;
  GdkGC *middle_nw = NULL;
  GdkGC *middle_ne = NULL;
  GdkGC *middle_sw = NULL;
  GdkGC *middle_se = NULL;
  GdkGC *inner_nw = NULL;
  GdkGC *inner_ne = NULL;
  GdkGC *inner_sw = NULL;
  GdkGC *inner_se = NULL;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if ((width == -1) && (height == -1))
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  half_width = width / 2;
  half_height = height / 2;
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->bg_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->black_gc, area);
    }
  
  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      inner_sw = inner_se = style->bg_gc[state_type];
      middle_sw = middle_se = style->light_gc[state_type];
      outer_sw = outer_se = style->light_gc[state_type];
      inner_nw = inner_ne = style->black_gc;
      middle_nw = middle_ne = style->dark_gc[state_type];
      outer_nw = outer_ne = style->dark_gc[state_type];
      break;
          
    case GTK_SHADOW_OUT:
      inner_sw = inner_se = style->dark_gc[state_type];
      middle_sw = middle_se = style->dark_gc[state_type];
      outer_sw = outer_se = style->black_gc;
      inner_nw = inner_ne = style->bg_gc[state_type];
      middle_nw = middle_ne = style->light_gc[state_type];
      outer_nw = outer_ne = style->light_gc[state_type];
      break;

    case GTK_SHADOW_ETCHED_IN:
      inner_sw = inner_se = style->bg_gc[state_type];
      middle_sw = middle_se = style->dark_gc[state_type];
      outer_sw = outer_se = style->light_gc[state_type];
      inner_nw = inner_ne = style->bg_gc[state_type];
      middle_nw = middle_ne = style->light_gc[state_type];
      outer_nw = outer_ne = style->dark_gc[state_type];
      break;

    case GTK_SHADOW_ETCHED_OUT:
      inner_sw = inner_se = style->bg_gc[state_type];
      middle_sw = middle_se = style->light_gc[state_type];
      outer_sw = outer_se = style->dark_gc[state_type];
      inner_nw = inner_ne = style->bg_gc[state_type];
      middle_nw = middle_ne = style->dark_gc[state_type];
      outer_nw = outer_ne = style->light_gc[state_type];
      break;
      
    default:

      break;
    }

  if (inner_sw)
    {
      gdk_draw_line (window, inner_sw,
                     x + 2, y + half_height,
                     x + half_width, y + height - 2);
      gdk_draw_line (window, inner_se,
                     x + half_width, y + height - 2,
                     x + width - 2, y + half_height);
      gdk_draw_line (window, middle_sw,
                     x + 1, y + half_height,
                     x + half_width, y + height - 1);
      gdk_draw_line (window, middle_se,
                     x + half_width, y + height - 1,
                     x + width - 1, y + half_height);
      gdk_draw_line (window, outer_sw,
                     x, y + half_height,
                     x + half_width, y + height);
      gdk_draw_line (window, outer_se,
                     x + half_width, y + height,
                     x + width, y + half_height);
  
      gdk_draw_line (window, inner_nw,
                     x + 2, y + half_height,
                     x + half_width, y + 2);
      gdk_draw_line (window, inner_ne,
                     x + half_width, y + 2,
                     x + width - 2, y + half_height);
      gdk_draw_line (window, middle_nw,
                     x + 1, y + half_height,
                     x + half_width, y + 1);
      gdk_draw_line (window, middle_ne,
                     x + half_width, y + 1,
                     x + width - 1, y + half_height);
      gdk_draw_line (window, outer_nw,
                     x, y + half_height,
                     x + half_width, y);
      gdk_draw_line (window, outer_ne,
                     x + half_width, y,
                     x + width, y + half_height);
    }
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->bg_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->black_gc, NULL);
    }
}

static void
gtk_default_draw_string (GtkStyle      *style,
                         GdkWindow     *window,
                         GtkStateType   state_type,
                         GdkRectangle  *area,
                         GtkWidget     *widget,
                         const gchar   *detail,
                         gint           x,
                         gint           y,
                         const gchar   *string)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->white_gc, area);
      gdk_gc_set_clip_rectangle (style->fg_gc[state_type], area);
    }

  if (state_type == GTK_STATE_INSENSITIVE)
    gdk_draw_string (window, style->font, style->white_gc, x + 1, y + 1, string);

  gdk_draw_string (window, style->font, style->fg_gc[state_type], x, y, string);

  if (area)
    {
      gdk_gc_set_clip_rectangle (style->white_gc, NULL);
      gdk_gc_set_clip_rectangle (style->fg_gc[state_type], NULL);
    }
}

static void 
gtk_default_draw_box (GtkStyle      *style,
		      GdkWindow     *window,
		      GtkStateType   state_type,
		      GtkShadowType  shadow_type,
		      GdkRectangle  *area,
		      GtkWidget     *widget,
		      const gchar   *detail,
		      gint           x,
		      gint           y,
		      gint           width,
		      gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  if (!style->bg_pixmap[state_type] || 
      GDK_IS_PIXMAP (window))
    {
      if (area)
	gdk_gc_set_clip_rectangle (style->bg_gc[state_type], area);

      gdk_draw_rectangle (window, style->bg_gc[state_type], TRUE,
                          x, y, width, height);
      if (area)
	gdk_gc_set_clip_rectangle (style->bg_gc[state_type], NULL);
    }
  else
    gtk_style_apply_default_background (style, window,
                                        widget && !GTK_WIDGET_NO_WINDOW (widget),
                                        state_type, area, x, y, width, height);
  
  gtk_paint_shadow (style, window, state_type, shadow_type, area, widget, detail,
                    x, y, width, height);
}

static GdkGC*
get_darkened_gc (GdkWindow *window,
                 GdkColor  *color,
                 gint       darken_count)
{
  GdkColor src = *color;
  GdkColor shaded;
  GdkGC *gc;
  
  gc = gdk_gc_new (window);

  while (darken_count)
    {
      gtk_style_shade (&src, &shaded, 0.93);
      src = shaded;
      --darken_count;
    }
  
  gdk_gc_set_rgb_fg_color (gc, &shaded);

  return gc;
}

static void 
gtk_default_draw_flat_box (GtkStyle      *style,
                           GdkWindow     *window,
                           GtkStateType   state_type,
                           GtkShadowType  shadow_type,
                           GdkRectangle  *area,
                           GtkWidget     *widget,
                           const gchar   *detail,
                           gint           x,
                           gint           y,
                           gint           width,
                           gint           height)
{
  GdkGC *gc1;
  GdkGC *freeme = NULL;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  if (detail)
    {
      if (state_type == GTK_STATE_SELECTED)
        {
          if (!strcmp ("text", detail))
            gc1 = style->bg_gc[GTK_STATE_SELECTED];
          else if (!strcmp ("cell_even_sorted", detail) ||
                   !strcmp ("cell_odd_sorted", detail) ||
                   !strcmp ("cell_even_ruled_sorted", detail) ||
                   !strcmp ("cell_odd_ruled_sorted", detail))
            {
              freeme = get_darkened_gc (window, &style->bg[state_type], 1);
              gc1 = freeme;
            }
          else
            {
              gc1 = style->bg_gc[state_type];
            }
        }
      else
        {
          if (!strcmp ("viewportbin", detail))
            gc1 = style->bg_gc[GTK_STATE_NORMAL];
          else if (!strcmp ("entry_bg", detail))
            gc1 = style->base_gc[state_type];

          /* For trees: even rows are base color, odd rows are a shade of
           * the base color, the sort column is a shade of the original color
           * for that row.
           */

          /* FIXME when we have style properties, clean this up.
           */
          
          else if (!strcmp ("cell_even", detail) ||
                   !strcmp ("cell_odd", detail) ||
                   !strcmp ("cell_even_ruled", detail))
            {
              gc1 = style->base_gc[state_type];
            }
          else if (!strcmp ("cell_even_sorted", detail) ||
                   !strcmp ("cell_odd_sorted", detail) ||
                   !strcmp ("cell_odd_ruled", detail) ||
                   !strcmp ("cell_even_ruled_sorted", detail))
            {
              freeme = get_darkened_gc (window, &style->base[state_type], 1);
              gc1 = freeme;
            }
          else if (!strcmp ("cell_odd_ruled_sorted", detail))
            {
              freeme = get_darkened_gc (window, &style->base[state_type], 2);
              gc1 = freeme;
            }
          else
            gc1 = style->bg_gc[state_type];
        }
    }
  else
    gc1 = style->bg_gc[state_type];
  
  if (!style->bg_pixmap[state_type] || gc1 != style->bg_gc[state_type] ||
      GDK_IS_PIXMAP (window))
    {
      if (area)
	gdk_gc_set_clip_rectangle (gc1, area);

      gdk_draw_rectangle (window, gc1, TRUE,
                          x, y, width, height);

      if (detail && !strcmp ("tooltip", detail))
        gdk_draw_rectangle (window, style->black_gc, FALSE,
                            x, y, width - 1, height - 1);

      if (area)
	gdk_gc_set_clip_rectangle (gc1, NULL);
    }
  else
    gtk_style_apply_default_background (style, window,
                                        widget && !GTK_WIDGET_NO_WINDOW (widget),
                                        state_type, area, x, y, width, height);


  if (freeme)
    g_object_unref (G_OBJECT (freeme));
}

static void 
gtk_default_draw_check (GtkStyle      *style,
			GdkWindow     *window,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			GdkRectangle  *area,
			GtkWidget     *widget,
			const gchar   *detail,
			gint           x,
			gint           y,
			gint           width,
			gint           height)
{
  if (detail && strcmp (detail, "cellcheck") == 0)
    {
      gdk_draw_rectangle (window,
			  widget->style->fg_gc[state_type],
			  FALSE,
                          x, y,
			  width, height);

      if (shadow_type == GTK_SHADOW_IN)
	{
	  gdk_draw_line (window,
			 widget->style->fg_gc[state_type],
                         x, y,
                         x + width,
                         y + height);
	  gdk_draw_line (window,
			 widget->style->fg_gc[state_type],
                         x + width,
                         y,
                         x,
                         y + height);
	}
    }
  else
    {
      gtk_paint_box (style, window, state_type, shadow_type, area, widget, detail,
                     x, y, width, height);
    }
}

static void 
gtk_default_draw_option (GtkStyle      *style,
			 GdkWindow     *window,
			 GtkStateType   state_type,
			 GtkShadowType  shadow_type,
			 GdkRectangle  *area,
			 GtkWidget     *widget,
			 const gchar   *detail,
			 gint           x,
			 gint           y,
			 gint           width,
			 gint           height)
{
  if (detail && strcmp (detail, "cellradio") == 0)
    {
      gdk_draw_arc (window,
		    widget->style->fg_gc[state_type],
		    FALSE,
                    x, y,
		    width,
		    height,
		    0, 360*64);

      if (shadow_type == GTK_SHADOW_IN)
	{
	  gdk_draw_arc (window,
			widget->style->fg_gc[state_type],
			TRUE,
                        x + 2,
                        y + 2,
			width - 4,
			height - 4,
			0, 360*64);
	}
    }
  else
    {
      gtk_paint_diamond (style, window, state_type, shadow_type, area, widget, 
                         detail, x, y, width, height);
    }
}

static void
gtk_default_draw_tab (GtkStyle      *style,
		      GdkWindow     *window,
		      GtkStateType   state_type,
		      GtkShadowType  shadow_type,
		      GdkRectangle  *area,
		      GtkWidget     *widget,
		      const gchar   *detail,
		      gint           x,
		      gint           y,
		      gint           width,
		      gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  gtk_paint_box (style, window, state_type, shadow_type, area, widget, detail,
                 x, y, width, height);
}

static void 
gtk_default_draw_shadow_gap (GtkStyle       *style,
                             GdkWindow      *window,
                             GtkStateType    state_type,
                             GtkShadowType   shadow_type,
                             GdkRectangle   *area,
                             GtkWidget      *widget,
                             const gchar    *detail,
                             gint            x,
                             gint            y,
                             gint            width,
                             gint            height,
                             GtkPositionType gap_side,
                             gint            gap_x,
                             gint            gap_width)
{
  GdkGC *gc1 = NULL;
  GdkGC *gc2 = NULL;
  GdkGC *gc3 = NULL;
  GdkGC *gc4 = NULL;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      return;
    case GTK_SHADOW_IN:
      gc1 = style->dark_gc[state_type];
      gc2 = style->black_gc;
      gc3 = style->bg_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->light_gc[state_type];
      gc2 = style->bg_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->dark_gc[state_type];
      break;
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, area);
      gdk_gc_set_clip_rectangle (gc2, area);
      gdk_gc_set_clip_rectangle (gc3, area);
      gdk_gc_set_clip_rectangle (gc4, area);
    }
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
    case GTK_SHADOW_IN:
    case GTK_SHADOW_OUT:
    case GTK_SHADOW_ETCHED_IN:
    case GTK_SHADOW_ETCHED_OUT:
      switch (gap_side)
        {
        case GTK_POS_TOP:
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y, x + 1, y + height - 2);
          
          gdk_draw_line (window, gc3,
                         x + 1, y + height - 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc3,
                         x + width - 2, y, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 1, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc1,
                             x, y, x + gap_x - 1, y);
              gdk_draw_line (window, gc2,
                             x + 1, y + 1, x + gap_x - 1, y + 1);
              gdk_draw_line (window, gc2,
                             x + gap_x, y, x + gap_x, y);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc1,
                             x + gap_x + gap_width, y, x + width - 2, y);
              gdk_draw_line (window, gc2,
                             x + gap_x + gap_width, y + 1, x + width - 2, y + 1);
              gdk_draw_line (window, gc2,
                             x + gap_x + gap_width - 1, y, x + gap_x + gap_width - 1, y);
            }
          break;
        case GTK_POS_BOTTOM:
          gdk_draw_line (window, gc1,
                         x, y, x + width - 1, y);
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 2, y + 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + 1, y + height - 1);
          
          gdk_draw_line (window, gc3,
                         x + width - 2, y + 1, x + width - 2, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc4,
                             x, y + height - 1, x + gap_x - 1, y + height - 1);
              gdk_draw_line (window, gc3,
                             x + 1, y + height - 2, x + gap_x - 1, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + gap_x, y + height - 1, x + gap_x, y + height - 1);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc4,
                             x + gap_x + gap_width, y + height - 1, x + width - 2, y + height - 1);
              gdk_draw_line (window, gc3,
                             x + gap_x + gap_width, y + height - 2, x + width - 2, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + gap_x + gap_width - 1, y + height - 1, x + gap_x + gap_width - 1, y + height - 1);
            }
          break;
        case GTK_POS_LEFT:
          gdk_draw_line (window, gc1,
                         x, y, x + width - 1, y);
          gdk_draw_line (window, gc2,
                         x, y + 1, x + width - 2, y + 1);
          
          gdk_draw_line (window, gc3,
                         x, y + height - 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc3,
                         x + width - 2, y + 1, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 1, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc1,
                             x, y, x, y + gap_x - 1);
              gdk_draw_line (window, gc2,
                             x + 1, y + 1, x + 1, y + gap_x - 1);
              gdk_draw_line (window, gc2,
                             x, y + gap_x, x, y + gap_x);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc1,
                             x, y + gap_x + gap_width, x, y + height - 2);
              gdk_draw_line (window, gc2,
                             x + 1, y + gap_x + gap_width, x + 1, y + height - 2);
              gdk_draw_line (window, gc2,
                             x, y + gap_x + gap_width - 1, x, y + gap_x + gap_width - 1);
            }
          break;
        case GTK_POS_RIGHT:
          gdk_draw_line (window, gc1,
                         x, y, x + width - 1, y);
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 1, y + 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + 1, y + height - 2);
          
          gdk_draw_line (window, gc3,
                         x + 1, y + height - 2, x + width - 1, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc4,
                             x + width - 1, y, x + width - 1, y + gap_x - 1);
              gdk_draw_line (window, gc3,
                             x + width - 2, y + 1, x + width - 2, y + gap_x - 1);
              gdk_draw_line (window, gc3,
                             x + width - 1, y + gap_x, x + width - 1, y + gap_x);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc4,
                             x + width - 1, y + gap_x + gap_width, x + width - 1, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + width - 2, y + gap_x + gap_width, x + width - 2, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + width - 1, y + gap_x + gap_width - 1, x + width - 1, y + gap_x + gap_width - 1);
            }
          break;
        }
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, NULL);
      gdk_gc_set_clip_rectangle (gc2, NULL);
      gdk_gc_set_clip_rectangle (gc3, NULL);
      gdk_gc_set_clip_rectangle (gc4, NULL);
    }
}

static void 
gtk_default_draw_box_gap (GtkStyle       *style,
                          GdkWindow      *window,
                          GtkStateType    state_type,
                          GtkShadowType   shadow_type,
                          GdkRectangle   *area,
                          GtkWidget      *widget,
                          const gchar    *detail,
                          gint            x,
                          gint            y,
                          gint            width,
                          gint            height,
                          GtkPositionType gap_side,
                          gint            gap_x,
                          gint            gap_width)
{
  GdkGC *gc1 = NULL;
  GdkGC *gc2 = NULL;
  GdkGC *gc3 = NULL;
  GdkGC *gc4 = NULL;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  gtk_style_apply_default_background (style, window,
                                      widget && !GTK_WIDGET_NO_WINDOW (widget),
                                      state_type, area, x, y, width, height);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      return;
    case GTK_SHADOW_IN:
      gc1 = style->dark_gc[state_type];
      gc2 = style->black_gc;
      gc3 = style->bg_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->light_gc[state_type];
      gc2 = style->bg_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->dark_gc[state_type];
      break;
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, area);
      gdk_gc_set_clip_rectangle (gc2, area);
      gdk_gc_set_clip_rectangle (gc3, area);
      gdk_gc_set_clip_rectangle (gc4, area);
    }
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
    case GTK_SHADOW_IN:
    case GTK_SHADOW_OUT:
    case GTK_SHADOW_ETCHED_IN:
    case GTK_SHADOW_ETCHED_OUT:
      switch (gap_side)
        {
        case GTK_POS_TOP:
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y, x + 1, y + height - 2);
          
          gdk_draw_line (window, gc3,
                         x + 1, y + height - 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc3,
                         x + width - 2, y, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 1, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc1,
                             x, y, x + gap_x - 1, y);
              gdk_draw_line (window, gc2,
                             x + 1, y + 1, x + gap_x - 1, y + 1);
              gdk_draw_line (window, gc2,
                             x + gap_x, y, x + gap_x, y);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc1,
                             x + gap_x + gap_width, y, x + width - 2, y);
              gdk_draw_line (window, gc2,
                             x + gap_x + gap_width, y + 1, x + width - 2, y + 1);
              gdk_draw_line (window, gc2,
                             x + gap_x + gap_width - 1, y, x + gap_x + gap_width - 1, y);
            }
          break;
        case  GTK_POS_BOTTOM:
          gdk_draw_line (window, gc1,
                         x, y, x + width - 1, y);
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 2, y + 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + 1, y + height - 1);
          
          gdk_draw_line (window, gc3,
                         x + width - 2, y + 1, x + width - 2, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc4,
                             x, y + height - 1, x + gap_x - 1, y + height - 1);
              gdk_draw_line (window, gc3,
                             x + 1, y + height - 2, x + gap_x - 1, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + gap_x, y + height - 1, x + gap_x, y + height - 1);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc4,
                             x + gap_x + gap_width, y + height - 1, x + width - 2, y + height - 1);
              gdk_draw_line (window, gc3,
                             x + gap_x + gap_width, y + height - 2, x + width - 2, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + gap_x + gap_width - 1, y + height - 1, x + gap_x + gap_width - 1, y + height - 1);
            }
          break;
        case GTK_POS_LEFT:
          gdk_draw_line (window, gc1,
                         x, y, x + width - 1, y);
          gdk_draw_line (window, gc2,
                         x, y + 1, x + width - 2, y + 1);
          
          gdk_draw_line (window, gc3,
                         x, y + height - 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc3,
                         x + width - 2, y + 1, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 1, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc1,
                             x, y, x, y + gap_x - 1);
              gdk_draw_line (window, gc2,
                             x + 1, y + 1, x + 1, y + gap_x - 1);
              gdk_draw_line (window, gc2,
                             x, y + gap_x, x, y + gap_x);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc1,
                             x, y + gap_x + gap_width, x, y + height - 2);
              gdk_draw_line (window, gc2,
                             x + 1, y + gap_x + gap_width, x + 1, y + height - 2);
              gdk_draw_line (window, gc2,
                             x, y + gap_x + gap_width - 1, x, y + gap_x + gap_width - 1);
            }
          break;
        case GTK_POS_RIGHT:
          gdk_draw_line (window, gc1,
                         x, y, x + width - 1, y);
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 1, y + 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + 1, y + height - 2);
          
          gdk_draw_line (window, gc3,
                         x + 1, y + height - 2, x + width - 1, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              gdk_draw_line (window, gc4,
                             x + width - 1, y, x + width - 1, y + gap_x - 1);
              gdk_draw_line (window, gc3,
                             x + width - 2, y + 1, x + width - 2, y + gap_x - 1);
              gdk_draw_line (window, gc3,
                             x + width - 1, y + gap_x, x + width - 1, y + gap_x);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              gdk_draw_line (window, gc4,
                             x + width - 1, y + gap_x + gap_width, x + width - 1, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + width - 2, y + gap_x + gap_width, x + width - 2, y + height - 2);
              gdk_draw_line (window, gc3,
                             x + width - 1, y + gap_x + gap_width - 1, x + width - 1, y + gap_x + gap_width - 1);
            }
          break;
        }
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, NULL);
      gdk_gc_set_clip_rectangle (gc2, NULL);
      gdk_gc_set_clip_rectangle (gc3, NULL);
      gdk_gc_set_clip_rectangle (gc4, NULL);
    }
}

static void 
gtk_default_draw_extension (GtkStyle       *style,
                            GdkWindow      *window,
                            GtkStateType    state_type,
                            GtkShadowType   shadow_type,
                            GdkRectangle   *area,
                            GtkWidget      *widget,
                            const gchar    *detail,
                            gint            x,
                            gint            y,
                            gint            width,
                            gint            height,
                            GtkPositionType gap_side)
{
  GdkGC *gc1 = NULL;
  GdkGC *gc2 = NULL;
  GdkGC *gc3 = NULL;
  GdkGC *gc4 = NULL;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  gtk_style_apply_default_background (style, window,
                                      widget && !GTK_WIDGET_NO_WINDOW (widget),
                                      GTK_STATE_NORMAL, area, x, y, width, height);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      return;
    case GTK_SHADOW_IN:
      gc1 = style->dark_gc[state_type];
      gc2 = style->black_gc;
      gc3 = style->bg_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->light_gc[state_type];
      gc2 = style->bg_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->dark_gc[state_type];
      break;
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, area);
      gdk_gc_set_clip_rectangle (gc2, area);
      gdk_gc_set_clip_rectangle (gc3, area);
      gdk_gc_set_clip_rectangle (gc4, area);
    }

  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
    case GTK_SHADOW_IN:
    case GTK_SHADOW_OUT:
    case GTK_SHADOW_ETCHED_IN:
    case GTK_SHADOW_ETCHED_OUT:
      switch (gap_side)
        {
        case GTK_POS_TOP:
          gtk_style_apply_default_background (style, window,
                                              widget && !GTK_WIDGET_NO_WINDOW (widget),
                                              state_type, area,
                                              x + style->xthickness, 
                                              y, 
                                              width - (2 * style->xthickness), 
                                              height - (style->ythickness));
          gdk_draw_line (window, gc1,
                         x, y, x, y + height - 2);
          gdk_draw_line (window, gc2,
                         x + 1, y, x + 1, y + height - 2);
          
          gdk_draw_line (window, gc3,
                         x + 2, y + height - 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc3,
                         x + width - 2, y, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc4,
                         x + 1, y + height - 1, x + width - 2, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y, x + width - 1, y + height - 2);
          break;
        case GTK_POS_BOTTOM:
          gtk_style_apply_default_background (style, window,
                                              widget && !GTK_WIDGET_NO_WINDOW (widget),
                                              state_type, area,
                                              x + style->xthickness, 
                                              y + style->ythickness, 
                                              width - (2 * style->xthickness), 
                                              height - (style->ythickness));
          gdk_draw_line (window, gc1,
                         x + 1, y, x + width - 2, y);
          gdk_draw_line (window, gc1,
                         x, y + 1, x, y + height - 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 2, y + 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + 1, y + height - 1);
          
          gdk_draw_line (window, gc3,
                         x + width - 2, y + 2, x + width - 2, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y + 1, x + width - 1, y + height - 1);
          break;
        case GTK_POS_LEFT:
          gtk_style_apply_default_background (style, window,
                                              widget && !GTK_WIDGET_NO_WINDOW (widget),
                                              state_type, area,
                                              x, 
                                              y + style->ythickness, 
                                              width - (style->xthickness), 
                                              height - (2 * style->ythickness));
          gdk_draw_line (window, gc1,
                         x, y, x + width - 2, y);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 2, y + 1);
          
          gdk_draw_line (window, gc3,
                         x, y + height - 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc3,
                         x + width - 2, y + 2, x + width - 2, y + height - 2);
          gdk_draw_line (window, gc4,
                         x, y + height - 1, x + width - 2, y + height - 1);
          gdk_draw_line (window, gc4,
                         x + width - 1, y + 1, x + width - 1, y + height - 2);
          break;
        case GTK_POS_RIGHT:
          gtk_style_apply_default_background (style, window,
                                              widget && !GTK_WIDGET_NO_WINDOW (widget),
                                              state_type, area,
                                              x + style->xthickness, 
                                              y + style->ythickness, 
                                              width - (style->xthickness), 
                                              height - (2 * style->ythickness));
          gdk_draw_line (window, gc1,
                         x + 1, y, x + width - 1, y);
          gdk_draw_line (window, gc1,
                         x, y + 1, x, y + height - 2);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + width - 1, y + 1);
          gdk_draw_line (window, gc2,
                         x + 1, y + 1, x + 1, y + height - 2);
          
          gdk_draw_line (window, gc3,
                         x + 2, y + height - 2, x + width - 1, y + height - 2);
          gdk_draw_line (window, gc4,
                         x + 1, y + height - 1, x + width - 1, y + height - 1);
          break;
        }
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle (gc1, NULL);
      gdk_gc_set_clip_rectangle (gc2, NULL);
      gdk_gc_set_clip_rectangle (gc3, NULL);
      gdk_gc_set_clip_rectangle (gc4, NULL);
    }
}

static void 
gtk_default_draw_focus (GtkStyle      *style,
                        GdkWindow     *window,
                        GdkRectangle  *area,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        gint           x,
                        gint           y,
                        gint           width,
                        gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (width == -1 && height == -1)
    {
      gdk_window_get_size (window, &width, &height);
      width -= 1;
      height -= 1;
    }
  else if (width == -1)
    {
      gdk_window_get_size (window, &width, NULL);
      width -= 1;
    }
  else if (height == -1)
    {
      gdk_window_get_size (window, NULL, &height);
      height -= 1;
    }

  if (area)
    gdk_gc_set_clip_rectangle (style->black_gc, area);

  if (detail && !strcmp (detail, "add-mode"))
    {
      gdk_gc_set_line_attributes (style->black_gc, 1, GDK_LINE_ON_OFF_DASH, 0, 0);
      gdk_gc_set_dashes (style->black_gc, 0, "\4\4", 2);
      
      gdk_draw_rectangle (window,
                          style->black_gc, FALSE,
                          x, y, width, height);
      
      gdk_gc_set_line_attributes (style->black_gc, 1, GDK_LINE_SOLID, 0, 0);
    }
  else if (detail && strcmp (detail, "treeview") == 0)
    {
      
      gdk_gc_set_background (style->black_gc, &style->white);
      gdk_gc_set_line_attributes (style->black_gc, 1, GDK_LINE_DOUBLE_DASH, 0, 0);
      gdk_gc_set_dashes (style->black_gc, 0, "\4\4", 2);
      
      gdk_draw_rectangle (window,
                          style->black_gc, FALSE,
                          x, y, width, height);
      
      gdk_gc_set_line_attributes (style->black_gc, 1, GDK_LINE_SOLID, 0, 0);
    }
  else
    {
      gdk_draw_rectangle (window,
                          style->black_gc, FALSE,
                          x, y, width, height);
    }

  if (area)
    gdk_gc_set_clip_rectangle (style->black_gc, NULL);
}

static void 
gtk_default_draw_slider (GtkStyle      *style,
                         GdkWindow     *window,
                         GtkStateType   state_type,
                         GtkShadowType  shadow_type,
                         GdkRectangle  *area,
                         GtkWidget     *widget,
                         const gchar   *detail,
                         gint           x,
                         gint           y,
                         gint           width,
                         gint           height,
                         GtkOrientation orientation)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  gtk_paint_box (style, window, state_type, shadow_type,
                 area, widget, detail, x, y, width, height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_paint_vline (style, window, state_type, area, widget, detail, 
                     style->ythickness, 
                     height - style->ythickness - 1, width / 2);
  else
    gtk_paint_hline (style, window, state_type, area, widget, detail, 
                     style->xthickness, 
                     width - style->xthickness - 1, height / 2);
}

static void
draw_dot (GdkWindow    *window,
	  GdkGC        *light_gc,
	  GdkGC        *dark_gc,
	  gint          x,
	  gint          y,
	  gushort       size)
{
  
  size = CLAMP (size, 2, 3);

  if (size == 2)
    {
      gdk_draw_point (window, light_gc, x, y);
      gdk_draw_point (window, light_gc, x+1, y+1);
    }
  else if (size == 3);
    {
      gdk_draw_point (window, light_gc, x, y);
      gdk_draw_point (window, light_gc, x+1, y);
      gdk_draw_point (window, light_gc, x, y+1);
      gdk_draw_point (window, dark_gc, x+1, y+2);
      gdk_draw_point (window, dark_gc, x+2, y+1);
      gdk_draw_point (window, dark_gc, x+2, y+2);
    }
}

static void 
gtk_default_draw_handle (GtkStyle      *style,
			 GdkWindow     *window,
			 GtkStateType   state_type,
			 GtkShadowType  shadow_type,
			 GdkRectangle  *area,
			 GtkWidget     *widget,
			 const gchar   *detail,
			 gint           x,
			 gint           y,
			 gint           width,
			 gint           height,
			 GtkOrientation orientation)
{
  gint xx, yy;
  gint xthick, ythick;
  GdkGC *light_gc, *dark_gc;
  GdkRectangle rect;
  GdkRectangle dest;
  gint intersect;
  
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (width == -1 && height == -1)
    gdk_window_get_size (window, &width, &height);
  else if (width == -1)
    gdk_window_get_size (window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size (window, NULL, &height);
  
  gtk_paint_box (style, window, state_type, shadow_type, area, widget, 
                 detail, x, y, width, height);
  
  
  if (!strcmp (detail, "paned"))
    {
      /* we want to ignore the shadow border in paned widgets */
      xthick = 0;
      ythick = 0;

      light_gc = style->light_gc[state_type];
      dark_gc = style->black_gc;
    }
  else
    {
      xthick = style->xthickness;
      ythick = style->ythickness;

      light_gc = style->light_gc[state_type];
      dark_gc = style->dark_gc[state_type];
    }
  
  rect.x = x + xthick;
  rect.y = y + ythick;
  rect.width = width - (xthick * 2);
  rect.height = height - (ythick * 2);

  if (area)
      intersect = gdk_rectangle_intersect (area, &rect, &dest);
  else
    {
      intersect = TRUE;
      dest = rect;
    }

  if (!intersect)
    return;

  gdk_gc_set_clip_rectangle (light_gc, &dest);
  gdk_gc_set_clip_rectangle (dark_gc, &dest);

  if (!strcmp (detail, "paned"))
    {
      gint window_width;
      gint window_height;

      gdk_window_get_size (window, &window_width, &window_height);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	for (xx = window_width/2 - 15; xx <= window_width/2 + 15; xx += 5)
	  draw_dot (window, light_gc, dark_gc, xx, window_height/2 - 1, 3);
      else
	for (yy = window_height/2 - 15; yy <= window_height/2 + 15; yy += 5)
	  draw_dot (window, light_gc, dark_gc, window_width/2 - 1, yy, 3);
    }
  else
    {
      for (yy = y + ythick; yy < (y + height - ythick); yy += 3)
	for (xx = x + xthick; xx < (x + width - xthick); xx += 6)
	  {
	    draw_dot (window, light_gc, dark_gc, xx, yy, 2);
	    draw_dot (window, light_gc, dark_gc, xx + 3, yy + 1, 2);
	  }
    }

  gdk_gc_set_clip_rectangle (light_gc, NULL);
  gdk_gc_set_clip_rectangle (dark_gc, NULL);
}

static void
gtk_default_draw_expander (GtkStyle        *style,
                           GdkWindow       *window,
                           GtkStateType     state_type,
                           GdkRectangle    *area,
                           GtkWidget       *widget,
                           const gchar     *detail,
                           gint             x,
                           gint             y,
                           gboolean         is_open)
{
  /* FIXME replace macro with a style property */
#define PM_SIZE 8
  
  GdkPoint points[3];

  if (area)
    {
      gdk_gc_set_clip_rectangle (style->fg_gc[GTK_STATE_NORMAL], area);
      gdk_gc_set_clip_rectangle (style->base_gc[GTK_STATE_NORMAL], area);
    }

  if (is_open)
    {
      points[0].x = x;
      points[0].y = y + (PM_SIZE + 2) / 6;
      points[1].x = points[0].x + 1 * (PM_SIZE + 2);
      points[1].y = points[0].y;
      points[2].x = (points[0].x + 1 * (PM_SIZE + 2) / 2);
      points[2].y = y + 2 * (PM_SIZE + 2) / 3;
    }
  else
    {
      points[0].x = x + 1 * ((PM_SIZE + 2) / 6 + 2);
      points[0].y = y - 1;
      points[1].x = points[0].x;
      points[1].y = points[0].y + (PM_SIZE + 2);
      points[2].x = (points[0].x + 1 * (2 * (PM_SIZE + 2) / 3 - 1));
      points[2].y = points[0].y + (PM_SIZE + 2) / 2;
    }

  gdk_draw_polygon (window, style->base_gc[GTK_STATE_NORMAL],
                    TRUE, points, 3);
  gdk_draw_polygon (window, style->fg_gc[GTK_STATE_NORMAL],
                    FALSE, points, 3);
  

  if (area)
    {
      gdk_gc_set_clip_rectangle (style->fg_gc[GTK_STATE_NORMAL], NULL);
      gdk_gc_set_clip_rectangle (style->base_gc[GTK_STATE_NORMAL], NULL);
    }
  
#undef PM_SIZE
}

typedef struct _ByteRange ByteRange;

struct _ByteRange
{
  guint start;
  guint end;
};

static ByteRange*
range_new (guint start,
           guint end)
{
  ByteRange *br = g_new (ByteRange, 1);

  br->start = start;
  br->end = end;
  
  return br;
}

static PangoLayout*
get_insensitive_layout (PangoLayout *layout)
{
  GSList *embossed_ranges = NULL;
  GSList *stippled_ranges = NULL;
  PangoLayoutIter *iter;
  GSList *tmp_list = NULL;
  PangoLayout *new_layout;
  PangoAttrList *attrs;
  GdkBitmap *stipple = NULL;
  
  iter = pango_layout_get_iter (layout);
  
  do
    {
      PangoLayoutRun *run;
      PangoAttribute *attr;
      gboolean need_stipple = FALSE;
      ByteRange *br;
      
      run = pango_layout_iter_get_run (iter);

      if (run)
        {
          tmp_list = run->item->extra_attrs;

          while (tmp_list != NULL)
            {
              attr = tmp_list->data;
              switch (attr->klass->type)
                {
                case PANGO_ATTR_FOREGROUND:
                case PANGO_ATTR_BACKGROUND:
                  need_stipple = TRUE;
                  break;
              
                default:
                  break;
                }

              if (need_stipple)
                break;
          
              tmp_list = g_slist_next (tmp_list);
            }

          br = range_new (run->item->offset, run->item->offset + run->item->length);
      
          if (need_stipple)
            stippled_ranges = g_slist_prepend (stippled_ranges, br);
          else
            embossed_ranges = g_slist_prepend (embossed_ranges, br);
        }
    }
  while (pango_layout_iter_next_run (iter));

  pango_layout_iter_free (iter);

  new_layout = pango_layout_copy (layout);

  attrs = pango_layout_get_attributes (new_layout);

  if (attrs == NULL)
    {
      /* Create attr list if there wasn't one */
      attrs = pango_attr_list_new ();
      pango_layout_set_attributes (new_layout, attrs);
      pango_attr_list_unref (attrs);
    }
  
  tmp_list = embossed_ranges;
  while (tmp_list != NULL)
    {
      PangoAttribute *attr;
      ByteRange *br = tmp_list->data;

      attr = gdk_pango_attr_embossed_new (TRUE);

      attr->start_index = br->start;
      attr->end_index = br->end;
      
      pango_attr_list_change (attrs, attr);

      g_free (br);
      
      tmp_list = g_slist_next (tmp_list);
    }

  g_slist_free (embossed_ranges);
  
  tmp_list = stippled_ranges;
  while (tmp_list != NULL)
    {
      PangoAttribute *attr;
      ByteRange *br = tmp_list->data;

      if (stipple == NULL)
        {
#define gray50_width 2
#define gray50_height 2
          static char gray50_bits[] = {
            0x02, 0x01
          };

          stipple = gdk_bitmap_create_from_data (NULL,
                                                 gray50_bits, gray50_width,
                                                 gray50_height);
        }
      
      attr = gdk_pango_attr_stipple_new (stipple);

      attr->start_index = br->start;
      attr->end_index = br->end;
      
      pango_attr_list_change (attrs, attr);

      g_free (br);
      
      tmp_list = g_slist_next (tmp_list);
    }

  g_slist_free (stippled_ranges);
  
  if (stipple)
    g_object_unref (G_OBJECT (stipple));

  return new_layout;
}

static void
gtk_default_draw_layout (GtkStyle        *style,
                         GdkWindow       *window,
                         GtkStateType     state_type,
                         GdkRectangle    *area,
                         GtkWidget       *widget,
                         const gchar     *detail,
                         gint             x,
                         gint             y,
                         PangoLayout     *layout)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state_type], area);

  if (state_type == GTK_STATE_INSENSITIVE)
    {
      PangoLayout *ins;

      ins = get_insensitive_layout (layout);
      
      gdk_draw_layout (window, style->fg_gc[state_type], x, y, ins);

      g_object_unref (G_OBJECT (ins));
    }
  else
    {
      gdk_draw_layout (window, style->fg_gc[state_type], x, y, layout);
    }

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state_type], NULL);
}

static void
gtk_default_draw_resize_grip (GtkStyle       *style,
                              GdkWindow      *window,
                              GtkStateType    state_type,
                              GdkRectangle   *area,
                              GtkWidget      *widget,
                              const gchar    *detail,
                              GdkWindowEdge   edge,
                              gint            x,
                              gint            y,
                              gint            width,
                              gint            height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], area);
    }

  /* make it square */
  if (width != height)
    width = height = MIN (width, height);
  
  switch (edge)
    {
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      {
        gint xi, yi;

        xi = x;
        yi = y;

        while (xi < (x + width - 3))
          {
            gdk_draw_line (window,
                           style->light_gc[state_type],
                           xi, y + height,
                           x + width, yi);                           

            ++xi;
            ++yi;
            
            gdk_draw_line (window,
                           style->dark_gc[state_type],
                           xi, y + height,
                           x + width, yi);                           

            ++xi;
            ++yi;
            
            gdk_draw_line (window,
                           style->dark_gc[state_type],
                           xi, y + height,
                           x + width, yi);

            xi += 3;
            yi += 3;
          }
      }
      break;

    }
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], NULL);
    }
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

void 
gtk_paint_hline (GtkStyle      *style,
                 GdkWindow     *window,
                 GtkStateType   state_type,
                 GdkRectangle  *area,
                 GtkWidget     *widget,
                 const gchar   *detail,
                 gint          x1,
                 gint          x2,
                 gint          y)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_hline != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_hline (style, window, state_type, area, widget, detail, x1, x2, y);
}

void
gtk_paint_vline (GtkStyle      *style,
                 GdkWindow     *window,
                 GtkStateType   state_type,
                 GdkRectangle  *area,
                 GtkWidget     *widget,
                 const gchar   *detail,
                 gint          y1,
                 gint          y2,
                 gint          x)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_vline != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_vline (style, window, state_type, area, widget, detail, y1, y2, x);
}

void
gtk_paint_shadow (GtkStyle     *style,
                  GdkWindow    *window,
                  GtkStateType  state_type,
                  GtkShadowType shadow_type,
                  GdkRectangle *area,
                  GtkWidget    *widget,
                  const gchar  *detail,
                  gint          x,
                  gint          y,
                  gint          width,
                  gint          height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_shadow (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_polygon (GtkStyle      *style,
                   GdkWindow     *window,
                   GtkStateType   state_type,
                   GtkShadowType  shadow_type,
                   GdkRectangle  *area,
                   GtkWidget     *widget,
                   const gchar   *detail,
                   GdkPoint      *points,
                   gint           npoints,
                   gboolean       fill)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_polygon (style, window, state_type, shadow_type, area, widget, detail, points, npoints, fill);
}

void
gtk_paint_arrow (GtkStyle      *style,
                 GdkWindow     *window,
                 GtkStateType   state_type,
                 GtkShadowType  shadow_type,
                 GdkRectangle  *area,
                 GtkWidget     *widget,
                 const gchar   *detail,
                 GtkArrowType   arrow_type,
                 gboolean       fill,
                 gint           x,
                 gint           y,
                 gint           width,
                 gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_arrow != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_arrow (style, window, state_type, shadow_type, area, widget, detail, arrow_type, fill, x, y, width, height);
}

void
gtk_paint_diamond (GtkStyle      *style,
                   GdkWindow     *window,
                   GtkStateType   state_type,
                   GtkShadowType  shadow_type,
                   GdkRectangle  *area,
                   GtkWidget     *widget,
                   const gchar   *detail,
                   gint        x,
                   gint        y,
                   gint        width,
                   gint        height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_diamond != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_diamond (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_string (GtkStyle      *style,
                  GdkWindow     *window,
                  GtkStateType   state_type,
                  GdkRectangle  *area,
                  GtkWidget     *widget,
                  const gchar   *detail,
                  gint           x,
                  gint           y,
                  const gchar   *string)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_string != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_string (style, window, state_type, area, widget, detail, x, y, string);
}

void
gtk_paint_box (GtkStyle      *style,
               GdkWindow     *window,
               GtkStateType   state_type,
               GtkShadowType  shadow_type,
               GdkRectangle  *area,
               GtkWidget     *widget,
               const gchar   *detail,
               gint           x,
               gint           y,
               gint           width,
               gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_box != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_box (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_flat_box (GtkStyle      *style,
                    GdkWindow     *window,
                    GtkStateType   state_type,
                    GtkShadowType  shadow_type,
                    GdkRectangle  *area,
                    GtkWidget     *widget,
                    const gchar   *detail,
                    gint           x,
                    gint           y,
                    gint           width,
                    gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_flat_box != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_flat_box (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_check (GtkStyle      *style,
                 GdkWindow     *window,
                 GtkStateType   state_type,
                 GtkShadowType  shadow_type,
                 GdkRectangle  *area,
                 GtkWidget     *widget,
                 const gchar   *detail,
                 gint           x,
                 gint           y,
                 gint           width,
                 gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_check != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_check (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_option (GtkStyle      *style,
                  GdkWindow     *window,
                  GtkStateType   state_type,
                  GtkShadowType  shadow_type,
                  GdkRectangle  *area,
                  GtkWidget     *widget,
                  const gchar   *detail,
                  gint           x,
                  gint           y,
                  gint           width,
                  gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_option != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_option (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_tab (GtkStyle      *style,
               GdkWindow     *window,
               GtkStateType   state_type,
               GtkShadowType  shadow_type,
               GdkRectangle  *area,
               GtkWidget     *widget,
               const gchar   *detail,
               gint           x,
               gint           y,
               gint           width,
               gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_tab != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_tab (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
}

void
gtk_paint_shadow_gap (GtkStyle       *style,
                      GdkWindow      *window,
                      GtkStateType    state_type,
                      GtkShadowType   shadow_type,
                      GdkRectangle   *area,
                      GtkWidget      *widget,
                      gchar          *detail,
                      gint            x,
                      gint            y,
                      gint            width,
                      gint            height,
                      GtkPositionType gap_side,
                      gint            gap_x,
                      gint            gap_width)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow_gap != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_shadow_gap (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height, gap_side, gap_x, gap_width);
}


void
gtk_paint_box_gap (GtkStyle       *style,
                   GdkWindow      *window,
                   GtkStateType    state_type,
                   GtkShadowType   shadow_type,
                   GdkRectangle   *area,
                   GtkWidget      *widget,
                   gchar          *detail,
                   gint            x,
                   gint            y,
                   gint            width,
                   gint            height,
                   GtkPositionType gap_side,
                   gint            gap_x,
                   gint            gap_width)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_box_gap != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_box_gap (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height, gap_side, gap_x, gap_width);
}

void
gtk_paint_extension (GtkStyle       *style,
                     GdkWindow      *window,
                     GtkStateType    state_type,
                     GtkShadowType   shadow_type,
                     GdkRectangle   *area,
                     GtkWidget      *widget,
                     gchar          *detail,
                     gint            x,
                     gint            y,
                     gint            width,
                     gint            height,
                     GtkPositionType gap_side)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_extension != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_extension (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height, gap_side);
}

void
gtk_paint_focus (GtkStyle      *style,
                 GdkWindow     *window,
                 GdkRectangle  *area,
                 GtkWidget     *widget,
                 const gchar   *detail,
                 gint           x,
                 gint           y,
                 gint           width,
                 gint           height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_focus != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_focus (style, window, area, widget, detail, x, y, width, height);
}

void
gtk_paint_slider (GtkStyle      *style,
                  GdkWindow     *window,
                  GtkStateType   state_type,
                  GtkShadowType  shadow_type,
                  GdkRectangle  *area,
                  GtkWidget     *widget,
                  const gchar   *detail,
                  gint           x,
                  gint           y,
                  gint           width,
                  gint           height,
                  GtkOrientation orientation)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_slider != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_slider (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height, orientation);
}

void
gtk_paint_handle (GtkStyle      *style,
                  GdkWindow     *window,
                  GtkStateType   state_type,
                  GtkShadowType  shadow_type,
                  GdkRectangle  *area,
                  GtkWidget     *widget,
                  const gchar   *detail,
                  gint           x,
                  gint           y,
                  gint           width,
                  gint           height,
                  GtkOrientation orientation)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_handle != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_handle (style, window, state_type, shadow_type, area, widget, detail, x, y, width, height, orientation);
}

void
gtk_paint_expander (GtkStyle        *style,
                    GdkWindow       *window,
                    GtkStateType     state_type,
                    GdkRectangle    *area,
                    GtkWidget       *widget,
                    const gchar     *detail,
                    gint             x,
                    gint             y,
                    gboolean         is_open)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_expander != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_expander (style, window, state_type, area,
                                              widget, detail, x, y, is_open);
}

void
gtk_paint_layout (GtkStyle        *style,
                  GdkWindow       *window,
                  GtkStateType     state_type,
                  GdkRectangle    *area,
                  GtkWidget       *widget,
                  const gchar     *detail,
                  gint             x,
                  gint             y,
                  PangoLayout     *layout)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_layout != NULL);
  
  GTK_STYLE_GET_CLASS (style)->draw_layout (style, window, state_type, area,
                                            widget, detail, x, y, layout);
}

void
gtk_paint_resize_grip (GtkStyle      *style,
                       GdkWindow     *window,
                       GtkStateType   state_type,
                       GdkRectangle  *area,
                       GtkWidget     *widget,
                       const gchar   *detail,
                       GdkWindowEdge  edge,
                       gint           x,
                       gint           y,
                       gint           width,
                       gint           height)

{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_resize_grip != NULL);

  GTK_STYLE_GET_CLASS (style)->draw_resize_grip (style, window, state_type,
                                                 area, widget, detail,
                                                 edge, x, y, width, height);
}

