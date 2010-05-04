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

#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <gobject/gvaluecollector.h>
#include "gtkmarshalers.h"
#include "gtkpango.h"
#include "gtkrc.h"
#include "gtkspinbutton.h"
#include "gtkstyle.h"
#include "gtkwidget.h"
#include "gtkthemes.h"
#include "gtkiconfactory.h"
#include "gtksettings.h"	/* _gtk_settings_parse_convert() */
#include "gtkintl.h"
#include "gtkdebug.h"
#include "gtkspinner.h"


/**
 * SECTION:gtkstyle
 * @Short_description: An object that hold style information for widgets
 * @Title: GtkStyle
 *
 * A #GtkStyle object encapsulates the information that provides the look and
 * feel for a widget. Each #GtkWidget has an associated #GTkStyle object that
 * is used when rendering that widget. Also, a #GtkStyle holds information for
 * the five possible widget states though not every widget supports all five
 * states; see #GtkStateType.
 *
 * Usually the #GtkStyle for a widget is the same as the default style that is
 * set by GTK+ and modified the theme engine.
 *
 * Usually applications should not need to use or modify the #GtkStyle of their
 * widgets.
 */


#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7

/* --- typedefs & structures --- */
typedef struct {
  GType       widget_type;
  GParamSpec *pspec;
  GValue      value;
} PropertyValue;

#define GTK_STYLE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_STYLE, GtkStylePrivate))

typedef struct _GtkStylePrivate GtkStylePrivate;

struct _GtkStylePrivate {
  GSList *color_hashes;
};

/* --- prototypes --- */
static void	 gtk_style_finalize		(GObject	*object);
static void	 gtk_style_realize		(GtkStyle	*style,
						 GdkVisual      *visual);
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
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x1,
					 gint             x2,
					 gint             y);
static void gtk_default_draw_vline      (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             y1,
					 gint             y2,
					 gint             x);
static void gtk_default_draw_shadow     (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_arrow      (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 GtkArrowType     arrow_type,
					 gboolean         fill,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_diamond    (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_box        (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_flat_box   (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_check      (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_option     (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_tab        (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_shadow_gap (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
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
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
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
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkPositionType  gap_side);
static void gtk_default_draw_focus      (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void gtk_default_draw_slider     (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkOrientation   orientation);
static void gtk_default_draw_handle     (GtkStyle        *style,
					 cairo_t         *cr,
					 GtkStateType     state_type,
					 GtkShadowType    shadow_type,
					 GtkWidget       *widget,
					 const gchar     *detail,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 GtkOrientation   orientation);
static void gtk_default_draw_expander   (GtkStyle        *style,
                                         cairo_t         *cr,
                                         GtkStateType     state_type,
                                         GtkWidget       *widget,
                                         const gchar     *detail,
                                         gint             x,
                                         gint             y,
					 GtkExpanderStyle expander_style);
static void gtk_default_draw_layout     (GtkStyle        *style,
                                         cairo_t         *cr,
                                         GtkStateType     state_type,
					 gboolean         use_text,
                                         GtkWidget       *widget,
                                         const gchar     *detail,
                                         gint             x,
                                         gint             y,
                                         PangoLayout     *layout);
static void gtk_default_draw_resize_grip (GtkStyle       *style,
                                          cairo_t        *cr,
                                          GtkStateType    state_type,
                                          GtkWidget      *widget,
                                          const gchar    *detail,
                                          GdkWindowEdge   edge,
                                          gint            x,
                                          gint            y,
                                          gint            width,
                                          gint            height);
static void gtk_default_draw_spinner     (GtkStyle       *style,
                                          cairo_t        *cr,
					  GtkStateType    state_type,
                                          GtkWidget      *widget,
                                          const gchar    *detail,
					  guint           step,
					  gint            x,
					  gint            y,
					  gint            width,
					  gint            height);

static void rgb_to_hls			(gdouble	 *r,
					 gdouble	 *g,
					 gdouble	 *b);
static void hls_to_rgb			(gdouble	 *h,
					 gdouble	 *l,
					 gdouble	 *s);

static void style_unrealize_cursors     (GtkStyle *style);

/*
 * Data for default check and radio buttons
 */

static const GtkRequisition default_option_indicator_size = { 7, 13 };
static const GtkBorder default_option_indicator_spacing = { 7, 5, 2, 2 };

#define GTK_GRAY		0xdcdc, 0xdada, 0xd5d5
#define GTK_DARK_GRAY		0xc4c4, 0xc2c2, 0xbdbd
#define GTK_LIGHT_GRAY		0xeeee, 0xebeb, 0xe7e7
#define GTK_WHITE		0xffff, 0xffff, 0xffff
#define GTK_BLUE		0x4b4b, 0x6969, 0x8383
#define GTK_VERY_DARK_GRAY	0x9c9c, 0x9a9a, 0x9494
#define GTK_BLACK		0x0000, 0x0000, 0x0000
#define GTK_WEAK_GRAY		0x7530, 0x7530, 0x7530

/* --- variables --- */
static const GdkColor gtk_default_normal_fg =      { 0, GTK_BLACK };
static const GdkColor gtk_default_active_fg =      { 0, GTK_BLACK };
static const GdkColor gtk_default_prelight_fg =    { 0, GTK_BLACK };
static const GdkColor gtk_default_selected_fg =    { 0, GTK_WHITE };
static const GdkColor gtk_default_insensitive_fg = { 0, GTK_WEAK_GRAY };

static const GdkColor gtk_default_normal_bg =      { 0, GTK_GRAY };
static const GdkColor gtk_default_active_bg =      { 0, GTK_DARK_GRAY };
static const GdkColor gtk_default_prelight_bg =    { 0, GTK_LIGHT_GRAY };
static const GdkColor gtk_default_selected_bg =    { 0, GTK_BLUE };
static const GdkColor gtk_default_insensitive_bg = { 0, GTK_GRAY };
static const GdkColor gtk_default_selected_base =  { 0, GTK_BLUE };
static const GdkColor gtk_default_active_base =    { 0, GTK_VERY_DARK_GRAY };

/* --- signals --- */
static guint realize_signal = 0;
static guint unrealize_signal = 0;

G_DEFINE_TYPE (GtkStyle, gtk_style, G_TYPE_OBJECT)

/* --- functions --- */

/**
 * _gtk_style_init_for_settings:
 * @style: a #GtkStyle
 * @settings: a #GtkSettings
 * 
 * Initializes the font description in @style according to the default
 * font name of @settings. This is called for gtk_style_new() with
 * the settings for the default screen (if any); if we are creating
 * a style for a particular screen, we then call it again in a
 * location where we know the correct settings.
 * The reason for this is that gtk_rc_style_create_style() doesn't
 * take the screen for an argument.
 **/
void
_gtk_style_init_for_settings (GtkStyle    *style,
			      GtkSettings *settings)
{
  const gchar *font_name = _gtk_rc_context_get_default_font_name (settings);

  if (style->font_desc)
    pango_font_description_free (style->font_desc);
  
  style->font_desc = pango_font_description_from_string (font_name);
      
  if (!pango_font_description_get_family (style->font_desc))
    {
      g_warning ("Default font does not have a family set");
      pango_font_description_set_family (style->font_desc, "Sans");
    }
  if (pango_font_description_get_size (style->font_desc) <= 0)
    {
      g_warning ("Default font does not have a positive size");
      pango_font_description_set_size (style->font_desc, 10 * PANGO_SCALE);
    }
}

static void
gtk_style_init (GtkStyle *style)
{
  gint i;
  
  GtkSettings *settings = gtk_settings_get_default ();
  
  if (settings)
    _gtk_style_init_for_settings (style, settings);
  else
    style->font_desc = pango_font_description_from_string ("Sans 10");
  
  style->attach_count = 0;
  
  style->black.red = 0;
  style->black.green = 0;
  style->black.blue = 0;
  
  style->white.red = 65535;
  style->white.green = 65535;
  style->white.blue = 65535;
  
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

  style->base[GTK_STATE_SELECTED] = gtk_default_selected_base;
  style->text[GTK_STATE_SELECTED] = style->white;
  style->base[GTK_STATE_ACTIVE] = gtk_default_active_base;
  style->text[GTK_STATE_ACTIVE] = style->white;
  style->base[GTK_STATE_INSENSITIVE] = gtk_default_prelight_bg;
  style->text[GTK_STATE_INSENSITIVE] = gtk_default_insensitive_fg;
  
  style->rc_style = NULL;
  
  style->xthickness = 2;
  style->ythickness = 2;

  style->property_cache = NULL;
}

static void
gtk_style_class_init (GtkStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
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
  klass->draw_arrow = gtk_default_draw_arrow;
  klass->draw_diamond = gtk_default_draw_diamond;
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
  klass->draw_spinner = gtk_default_draw_spinner;

  g_type_class_add_private (object_class, sizeof (GtkStylePrivate));

  /**
   * GtkStyle::realize:
   * @style: the object which received the signal
   *
   * Emitted when the style has been initialized for a particular
   * visual. Connecting to this signal is probably seldom
   * useful since most of the time applications and widgets only
   * deal with styles that have been already realized.
   *
   * Since: 2.4
   */
  realize_signal = g_signal_new (I_("realize"),
				 G_TYPE_FROM_CLASS (object_class),
				 G_SIGNAL_RUN_FIRST,
				 G_STRUCT_OFFSET (GtkStyleClass, realize),
				 NULL, NULL,
				 _gtk_marshal_VOID__VOID,
				 G_TYPE_NONE, 0);
  /**
   * GtkStyle::unrealize:
   * @style: the object which received the signal
   *
   * Emitted when the aspects of the style specific to a particular visual
   * is being cleaned up. A connection to this signal can be useful
   * if a widget wants to cache objects as object data on #GtkStyle.
   * This signal provides a convenient place to free such cached objects.
   *
   * Since: 2.4
   */
  unrealize_signal = g_signal_new (I_("unrealize"),
				   G_TYPE_FROM_CLASS (object_class),
				   G_SIGNAL_RUN_FIRST,
				   G_STRUCT_OFFSET (GtkStyleClass, unrealize),
				   NULL, NULL,
				   _gtk_marshal_VOID__VOID,
				   G_TYPE_NONE, 0);
}

static void
clear_property_cache (GtkStyle *style)
{
  if (style->property_cache)
    {
      guint i;

      for (i = 0; i < style->property_cache->len; i++)
	{
	  PropertyValue *node = &g_array_index (style->property_cache, PropertyValue, i);

	  g_param_spec_unref (node->pspec);
	  g_value_unset (&node->value);
	}
      g_array_free (style->property_cache, TRUE);
      style->property_cache = NULL;
    }
}

static void
gtk_style_finalize (GObject *object)
{
  GtkStyle *style = GTK_STYLE (object);
  GtkStylePrivate *priv = GTK_STYLE_GET_PRIVATE (style);

  g_return_if_fail (style->attach_count == 0);

  clear_property_cache (style);
  
  /* All the styles in the list have the same 
   * style->styles pointer. If we delete the 
   * *first* style from the list, we need to update
   * the style->styles pointers from all the styles.
   * Otherwise we simply remove the node from
   * the list.
   */
  if (style->styles)
    {
      if (style->styles->data != style)
        style->styles = g_slist_remove (style->styles, style);
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

  g_slist_foreach (style->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (style->icon_factories);

  g_slist_foreach (priv->color_hashes, (GFunc) g_hash_table_unref, NULL);
  g_slist_free (priv->color_hashes);

  pango_font_description_free (style->font_desc);

  if (style->private_font_desc)
    pango_font_description_free (style->private_font_desc);

  if (style->rc_style)
    g_object_unref (style->rc_style);

  G_OBJECT_CLASS (gtk_style_parent_class)->finalize (object);
}


/**
 * gtk_style_copy:
 * @style: a #GtkStyle
 *
 * Creates a copy of the passed in #GtkStyle object.
 *
 * Returns: (transfer full): a copy of @style
 */
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
  
  /* All the styles in the list have the same 
   * style->styles pointer. When we insert a new 
   * style, we append it to the list to avoid having 
   * to update the existing ones. 
   */
  style->styles = g_slist_append (style->styles, new_style);
  new_style->styles = style->styles;  
  
  return new_style;
}

/**
 * gtk_style_new:
 * @returns: a new #GtkStyle.
 *
 * Creates a new #GtkStyle.
 **/
GtkStyle*
gtk_style_new (void)
{
  GtkStyle *style;
  
  style = g_object_new (GTK_TYPE_STYLE, NULL);
  
  return style;
}

/**
 * gtk_style_attach:
 * @style: a #GtkStyle.
 * @window: a #GdkWindow.
 *
 * Attaches a style to a window; this process allocates the
 * colors and creates the GC's for the style - it specializes
 * it to a particular visual. The process may involve the creation
 * of a new style if the style has already been attached to a
 * window with a different style and visual.
 *
 * Since this function may return a new object, you have to use it
 * in the following way:
 * <literal>style = gtk_style_attach (style, window)</literal>
 *
 * Returns: Either @style, or a newly-created #GtkStyle.
 *   If the style is newly created, the style parameter
 *   will be unref'ed, and the new style will have
 *   a reference count belonging to the caller.
 */
GtkStyle*
gtk_style_attach (GtkStyle  *style,
                  GdkWindow *window)
{
  GSList *styles;
  GtkStyle *new_style = NULL;
  GdkVisual *visual;
  
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (window != NULL, NULL);
  
  visual = gdk_window_get_visual (window);
  
  if (!style->styles)
    style->styles = g_slist_append (NULL, style);
  
  styles = style->styles;
  while (styles)
    {
      new_style = styles->data;
      
      if (new_style->visual == visual)
        break;

      new_style = NULL;
      styles = styles->next;
    }

  if (!new_style)
    {
      styles = style->styles;
      
      while (styles)
	{
	  new_style = styles->data;
	  
	  if (new_style->attach_count == 0)
	    {
	      gtk_style_realize (new_style, visual);
	      break;
	    }
	  
	  new_style = NULL;
	  styles = styles->next;
	}
    }
  
  if (!new_style)
    {
      new_style = gtk_style_duplicate (style);
      gtk_style_realize (new_style, visual);
    }

  /* A style gets a refcount from being attached */
  if (new_style->attach_count == 0)
    g_object_ref (new_style);

  /* Another refcount belongs to the parent */
  if (style != new_style) 
    {
      g_object_unref (style);
      g_object_ref (new_style);
    }
  
  new_style->attach_count++;
  
  return new_style;
}

/**
 * gtk_style_detach:
 * @style: a #GtkStyle
 *
 * Detaches a style from a window. If the style is not attached
 * to any windows anymore, it is unrealized. See gtk_style_attach().
 * 
 */
void
gtk_style_detach (GtkStyle *style)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (style->attach_count > 0);
  
  style->attach_count -= 1;
  if (style->attach_count == 0)
    {
      g_signal_emit (style, unrealize_signal, 0);
      
      g_object_unref (style->visual);
      style->visual = NULL;

      if (style->private_font_desc)
	{
	  pango_font_description_free (style->private_font_desc);
	  style->private_font_desc = NULL;
	}

      g_object_unref (style);
    }
}

static void
gtk_style_realize (GtkStyle  *style,
                   GdkVisual *visual)
{
  style->visual = g_object_ref (visual);

  g_signal_emit (style, realize_signal, 0);
}

/**
 * gtk_style_lookup_icon_set:
 * @style: a #GtkStyle
 * @stock_id: an icon name
 *
 * Looks up @stock_id in the icon factories associated with @style
 * and the default icon factory, returning an icon set if found,
 * otherwise %NULL.
 *
 * Return value: icon set of @stock_id
 */
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

/**
 * gtk_style_lookup_color:
 * @style: a #GtkStyle
 * @color_name: the name of the logical color to look up
 * @color: the #GdkColor to fill in
 *
 * Looks up @color_name in the style's logical color mappings,
 * filling in @color and returning %TRUE if found, otherwise
 * returning %FALSE. Do not cache the found mapping, because
 * it depends on the #GtkStyle and might change when a theme
 * switch occurs.
 *
 * Return value: %TRUE if the mapping was found.
 *
 * Since: 2.10
 **/
gboolean
gtk_style_lookup_color (GtkStyle   *style,
                        const char *color_name,
                        GdkColor   *color)
{
  GtkStylePrivate *priv;
  GSList *iter;

  g_return_val_if_fail (GTK_IS_STYLE (style), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  priv = GTK_STYLE_GET_PRIVATE (style);

  for (iter = priv->color_hashes; iter != NULL; iter = iter->next)
    {
      GHashTable *hash    = iter->data;
      GdkColor   *mapping = g_hash_table_lookup (hash, color_name);

      if (mapping)
        {
          color->red = mapping->red;
          color->green = mapping->green;
          color->blue = mapping->blue;
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * gtk_style_set_background:
 * @style: a #GtkStyle
 * @window: a #GdkWindow
 * @state_type: a state
 * 
 * Sets the background of @window to the background color or pixmap
 * specified by @style for the given state.
 */
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
  return g_object_new (G_OBJECT_TYPE (style), NULL);
}

static void
gtk_style_real_copy (GtkStyle *style,
		     GtkStyle *src)
{
  GtkStylePrivate *priv = GTK_STYLE_GET_PRIVATE (style);
  GtkStylePrivate *src_priv = GTK_STYLE_GET_PRIVATE (src);
  gint i;
  
  for (i = 0; i < 5; i++)
    {
      style->fg[i] = src->fg[i];
      style->bg[i] = src->bg[i];
      style->text[i] = src->text[i];
      style->base[i] = src->base[i];

      if (style->background[i])
	cairo_pattern_destroy (style->background[i]),
      style->background[i] = src->background[i];
      if (style->background[i])
	cairo_pattern_reference (style->background[i]);
    }

  if (style->font_desc)
    pango_font_description_free (style->font_desc);
  if (src->font_desc)
    style->font_desc = pango_font_description_copy (src->font_desc);
  else
    style->font_desc = NULL;
  
  style->xthickness = src->xthickness;
  style->ythickness = src->ythickness;

  if (style->rc_style)
    g_object_unref (style->rc_style);
  style->rc_style = src->rc_style;
  if (src->rc_style)
    g_object_ref (src->rc_style);

  g_slist_foreach (style->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (style->icon_factories);
  style->icon_factories = g_slist_copy (src->icon_factories);
  g_slist_foreach (style->icon_factories, (GFunc) g_object_ref, NULL);

  g_slist_foreach (priv->color_hashes, (GFunc) g_hash_table_unref, NULL);
  g_slist_free (priv->color_hashes);
  priv->color_hashes = g_slist_copy (src_priv->color_hashes);
  g_slist_foreach (priv->color_hashes, (GFunc) g_hash_table_ref, NULL);

  /* don't copy, just clear cache */
  clear_property_cache (style);
}

static void
gtk_style_real_init_from_rc (GtkStyle   *style,
			     GtkRcStyle *rc_style)
{
  GtkStylePrivate *priv = GTK_STYLE_GET_PRIVATE (style);
  gint i;

  /* cache _should_ be still empty */
  clear_property_cache (style);

  if (rc_style->font_desc)
    pango_font_description_merge (style->font_desc, rc_style->font_desc, TRUE);
    
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

  style->icon_factories = g_slist_copy (rc_style->icon_factories);
  g_slist_foreach (style->icon_factories, (GFunc) g_object_ref, NULL);

  priv->color_hashes = g_slist_copy (_gtk_rc_style_get_color_hashes (rc_style));
  g_slist_foreach (priv->color_hashes, (GFunc) g_hash_table_ref, NULL);
}

static gint
style_property_values_cmp (gconstpointer bsearch_node1,
			   gconstpointer bsearch_node2)
{
  const PropertyValue *val1 = bsearch_node1;
  const PropertyValue *val2 = bsearch_node2;

  if (val1->widget_type == val2->widget_type)
    return val1->pspec < val2->pspec ? -1 : val1->pspec == val2->pspec ? 0 : 1;
  else
    return val1->widget_type < val2->widget_type ? -1 : 1;
}

/**
 * gtk_style_get_style_property:
 * @style: a #GtkStyle
 * @widget_type: the #GType of a descendant of #GtkWidget
 * @property_name: the name of the style property to get
 * @value: a #GValue where the value of the property being
 *     queried will be stored
 *
 * Queries the value of a style property corresponding to a
 * widget class is in the given style.
 *
 * Since: 2.16
 */
void 
gtk_style_get_style_property (GtkStyle     *style,
                              GType        widget_type,
                              const gchar *property_name,
                              GValue      *value)
{
  GtkWidgetClass *klass;
  GParamSpec *pspec;
  GtkRcPropertyParser parser;
  const GValue *peek_value;

  klass = g_type_class_ref (widget_type);
  pspec = gtk_widget_class_find_style_property (klass, property_name);
  g_type_class_unref (klass);

  if (!pspec)
    {
      g_warning ("%s: widget class `%s' has no property named `%s'",
                 G_STRLOC,
                 g_type_name (widget_type),
                 property_name);
      return;
    }

  parser = g_param_spec_get_qdata (pspec,
                                   g_quark_from_static_string ("gtk-rc-property-parser"));

  peek_value = _gtk_style_peek_property_value (style, widget_type, pspec, parser);

  if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
    g_value_copy (peek_value, value);
  else if (g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
    g_value_transform (peek_value, value);
  else
    g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
               pspec->name,
               g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
               G_VALUE_TYPE_NAME (value));
}

/**
 * gtk_style_get_valist:
 * @style: a #GtkStyle
 * @widget_type: the #GType of a descendant of #GtkWidget
 * @first_property_name: the name of the first style property to get
 * @var_args: a <type>va_list</type> of pairs of property names and
 *     locations to return the property values, starting with the
 *     location for @first_property_name.
 *
 * Non-vararg variant of gtk_style_get().
 * Used primarily by language bindings.
 *
 * Since: 2.16
 */
void 
gtk_style_get_valist (GtkStyle    *style,
                      GType        widget_type,
                      const gchar *first_property_name,
                      va_list      var_args)
{
  const char *property_name;
  GtkWidgetClass *klass;

  g_return_if_fail (GTK_IS_STYLE (style));

  klass = g_type_class_ref (widget_type);

  property_name = first_property_name;
  while (property_name)
    {
      GParamSpec *pspec;
      GtkRcPropertyParser parser;
      const GValue *peek_value;
      gchar *error;

      pspec = gtk_widget_class_find_style_property (klass, property_name);

      if (!pspec)
        {
          g_warning ("%s: widget class `%s' has no property named `%s'",
                     G_STRLOC,
                     g_type_name (widget_type),
                     property_name);
          break;
        }

      parser = g_param_spec_get_qdata (pspec,
                                       g_quark_from_static_string ("gtk-rc-property-parser"));

      peek_value = _gtk_style_peek_property_value (style, widget_type, pspec, parser);
      G_VALUE_LCOPY (peek_value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      property_name = va_arg (var_args, gchar*);
    }

  g_type_class_unref (klass);
}

/**
 * gtk_style_get:
 * @style: a #GtkStyle
 * @widget_type: the #GType of a descendant of #GtkWidget
 * @first_property_name: the name of the first style property to get
 * @Varargs: pairs of property names and locations to
 *   return the property values, starting with the location for
 *   @first_property_name, terminated by %NULL.
 *
 * Gets the values of a multiple style properties for @widget_type
 * from @style.
 *
 * Since: 2.16
 */
void
gtk_style_get (GtkStyle    *style,
               GType        widget_type,
               const gchar *first_property_name,
               ...)
{
  va_list var_args;

  va_start (var_args, first_property_name);
  gtk_style_get_valist (style, widget_type, first_property_name, var_args);
  va_end (var_args);
}

const GValue*
_gtk_style_peek_property_value (GtkStyle           *style,
				GType               widget_type,
				GParamSpec         *pspec,
				GtkRcPropertyParser parser)
{
  PropertyValue *pcache, key = { 0, NULL, { 0, } };
  const GtkRcProperty *rcprop = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (g_type_is_a (pspec->owner_type, GTK_TYPE_WIDGET), NULL);
  g_return_val_if_fail (g_type_is_a (widget_type, pspec->owner_type), NULL);

  key.widget_type = widget_type;
  key.pspec = pspec;

  /* need value cache array */
  if (!style->property_cache)
    style->property_cache = g_array_new (FALSE, FALSE, sizeof (PropertyValue));
  else
    {
      pcache = bsearch (&key,
			style->property_cache->data, style->property_cache->len,
			sizeof (PropertyValue), style_property_values_cmp);
      if (pcache)
	return &pcache->value;
    }

  i = 0;
  while (i < style->property_cache->len &&
	 style_property_values_cmp (&key, &g_array_index (style->property_cache, PropertyValue, i)) >= 0)
    i++;

  g_array_insert_val (style->property_cache, i, key);
  pcache = &g_array_index (style->property_cache, PropertyValue, i);

  /* cache miss, initialize value type, then set contents */
  g_param_spec_ref (pcache->pspec);
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
  if (rcprop && !_gtk_settings_parse_convert (parser, &rcprop->value,
					      pspec, &pcache->value))
    {
      gchar *contents = g_strdup_value_contents (&rcprop->value);
      
      g_message ("%s: failed to retrieve property `%s::%s' of type `%s' from rc file value \"%s\" of type `%s'",
		 rcprop->origin ? rcprop->origin : "(for origin information, set GTK_DEBUG)",
		 g_type_name (pspec->owner_type), pspec->name,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		 contents,
		 G_VALUE_TYPE_NAME (&rcprop->value));
      g_free (contents);
      rcprop = NULL; /* needs default */
    }
  
  /* not supplied by rc style (or conversion failed), revert to default */
  if (!rcprop)
    g_param_value_set_default (pspec, &pcache->value);

  return &pcache->value;
}

static cairo_pattern_t *
load_background (GdkVisual   *visual,
	         GdkColor    *bg_color,
	         const gchar *filename)
{
  if (filename == NULL)
    {
      return cairo_pattern_create_rgb (bg_color->red   / 65535.0,
                                       bg_color->green / 65535.0,
                                       bg_color->blue  / 65535.0);
    }
  if (strcmp (filename, "<parent>") == 0)
    return NULL;
  else
    {
      GdkPixbuf *pixbuf;
      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_t *cr;
      GdkScreen *screen = gdk_visual_get_screen (visual);
  
      pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
      if (!pixbuf)
        return NULL;

      surface = gdk_window_create_similar_surface (gdk_screen_get_root_window (screen),
                                                   CAIRO_CONTENT_COLOR,
                                                   gdk_pixbuf_get_width (pixbuf),
                                                   gdk_pixbuf_get_height (pixbuf));
  
      cr = cairo_create (surface);

      gdk_cairo_set_source_color (cr, bg_color);
      cairo_paint (cr);

      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);

      cairo_destroy (cr);
      g_object_unref (pixbuf);

      pattern = cairo_pattern_create_for_surface (surface);

      cairo_surface_destroy (surface);

      return pattern;
    }
}

static void
gtk_style_real_realize (GtkStyle *style)
{
  gint i;

  for (i = 0; i < 5; i++)
    {
      _gtk_style_shade (&style->bg[i], &style->light[i], LIGHTNESS_MULT);
      _gtk_style_shade (&style->bg[i], &style->dark[i], DARKNESS_MULT);

      style->mid[i].red = (style->light[i].red + style->dark[i].red) / 2;
      style->mid[i].green = (style->light[i].green + style->dark[i].green) / 2;
      style->mid[i].blue = (style->light[i].blue + style->dark[i].blue) / 2;

      style->text_aa[i].red = (style->text[i].red + style->base[i].red) / 2;
      style->text_aa[i].green = (style->text[i].green + style->base[i].green) / 2;
      style->text_aa[i].blue = (style->text[i].blue + style->base[i].blue) / 2;
    }

  style->black.red = 0x0000;
  style->black.green = 0x0000;
  style->black.blue = 0x0000;

  style->white.red = 0xffff;
  style->white.green = 0xffff;
  style->white.blue = 0xffff;

  for (i = 0; i < 5; i++)
    {
      const char *image_name;

      if (style->rc_style)
        image_name = style->rc_style->bg_pixmap_name[i];
      else
        image_name = NULL;

      style->background[i] = load_background (style->visual,
					      &style->bg[i],
					      image_name);
    }
}

static void
gtk_style_real_unrealize (GtkStyle *style)
{
  int i;

  for (i = 0; i < 5; i++)
    {
      if (style->background[i])
	{
	  cairo_pattern_destroy (style->background[i]);
	  style->background[i] = NULL;
	}
      
    }
  
  style_unrealize_cursors (style);
}

static void
gtk_style_real_set_background (GtkStyle    *style,
			       GdkWindow   *window,
			       GtkStateType state_type)
{
  gdk_window_set_background_pattern (window, style->background[state_type]);
}

/**
 * gtk_style_render_icon:
 * @style: a #GtkStyle
 * @source: the #GtkIconSource specifying the icon to render
 * @direction: a text direction
 * @state: a state
 * @size: (type int): the size to render the icon at. A size of
 *     (GtkIconSize)-1 means render at the size of the source and
 *     don't scale.
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 *
 * Renders the icon specified by @source at the given @size
 * according to the given parameters and returns the result in a
 * pixbuf.
 *
 * Return value: (transfer full): a newly-created #GdkPixbuf
 *     containing the rendered icon
 */
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

/**
 * gtk_style_apply_default_background:
 * @style:
 * @cr: 
 * @window:
 * @set_bg:
 * @state_type:
 * @area: (allow-none):
 * @x:
 * @y:
 * @width:
 * @height:
 */
void
gtk_style_apply_default_background (GtkStyle          *style,
                                    cairo_t           *cr,
                                    GdkWindow         *window,
                                    GtkStateType       state_type,
                                    gint               x,
                                    gint               y,
                                    gint               width,
                                    gint               height)
{
  cairo_save (cr);

  if (style->background[state_type] == NULL)
    {
      GdkWindow *parent = gdk_window_get_parent (window);
      int x_offset, y_offset;

      if (parent)
        {
          gdk_window_get_position (window, &x_offset, &y_offset);
          cairo_translate (cr, -x_offset, -y_offset);
          gtk_style_apply_default_background (style, cr,
                                              parent, state_type,
                                              x + x_offset, y + y_offset,
                                              width, height);
          goto out;
        }
      else
        gdk_cairo_set_source_color (cr, &style->bg[state_type]);
    }
  else
    cairo_set_source (cr, style->background[state_type]);

  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);

out:
  cairo_restore (cr);
}

static GdkPixbuf *
scale_or_ref (GdkPixbuf *src,
              gint       width,
              gint       height)
{
  if (width == gdk_pixbuf_get_width (src) &&
      height == gdk_pixbuf_get_height (src))
    {
      return g_object_ref (src);
    }
  else
    {
      return gdk_pixbuf_scale_simple (src,
                                      width, height,
                                      GDK_INTERP_BILINEAR);
    }
}

static gboolean
lookup_icon_size (GtkStyle    *style,
		  GtkWidget   *widget,
		  GtkIconSize  size,
		  gint        *width,
		  gint        *height)
{
  GdkScreen *screen;
  GtkSettings *settings;

  if (widget && gtk_widget_has_screen (widget))
    {
      screen = gtk_widget_get_screen (widget);
      settings = gtk_settings_get_for_screen (screen);
    }
  else if (style && style->visual)
    {
      screen = gdk_visual_get_screen (style->visual);
      settings = gtk_settings_get_for_screen (screen);
    }
  else
    {
      settings = gtk_settings_get_default ();
      GTK_NOTE (MULTIHEAD,
		g_warning ("Using the default screen for gtk_default_render_icon()"));
    }

  return gtk_icon_size_lookup_for_settings (settings, size, width, height);
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

  if (size != (GtkIconSize) -1 && !lookup_icon_size(style, widget, size, &width, &height))
    {
      g_warning (G_STRLOC ": invalid icon size '%d'", size);
      return NULL;
    }

  /* If the size was wildcarded, and we're allowed to scale, then scale; otherwise,
   * leave it alone.
   */
  if (size != (GtkIconSize)-1 && gtk_icon_source_get_size_wildcarded (source))
    scaled = scale_or_ref (base_pixbuf, width, height);
  else
    scaled = g_object_ref (base_pixbuf);

  /* If the state was wildcarded, then generate a state. */
  if (gtk_icon_source_get_state_wildcarded (source))
    {
      if (state == GTK_STATE_INSENSITIVE)
        {
          stated = gdk_pixbuf_copy (scaled);      
          
          gdk_pixbuf_saturate_and_pixelate (scaled, stated,
                                            0.8, TRUE);
          
          g_object_unref (scaled);
        }
      else if (state == GTK_STATE_PRELIGHT)
        {
          stated = gdk_pixbuf_copy (scaled);      
          
          gdk_pixbuf_saturate_and_pixelate (scaled, stated,
                                            1.2, FALSE);
          
          g_object_unref (scaled);
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
_cairo_draw_line (cairo_t  *cr,
                  GdkColor *color,
                  gint      x1,
                  gint      y1,
                  gint      x2,
                  gint      y2)
{
  cairo_save (cr);

  gdk_cairo_set_source_color (cr, color);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

  cairo_move_to (cr, x1 + 0.5, y1 + 0.5);
  cairo_line_to (cr, x2 + 0.5, y2 + 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);
}

static void
_cairo_draw_rectangle (cairo_t *cr,
                       GdkColor *color,
                       gboolean filled,
                       gint x,
                       gint y,
                       gint width,
                       gint height)
{
  gdk_cairo_set_source_color (cr, color);

  if (filled)
    {
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }
  else
    {
      cairo_rectangle (cr, x + 0.5, y + 0.5, width, height);
      cairo_stroke (cr);
    }
}

static void
_cairo_draw_point (cairo_t *cr,
                   GdkColor *color,
                   gint x,
                   gint y)
{
  gdk_cairo_set_source_color (cr, color);
  cairo_rectangle (cr, x, y, 1, 1);
  cairo_fill (cr);
}

static void
gtk_default_draw_hline (GtkStyle      *style,
                        cairo_t       *cr,
                        GtkStateType  state_type,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        gint          x1,
                        gint          x2,
                        gint          y)
{
  gint thickness_light;
  gint thickness_dark;
  gint i;
  
  thickness_light = style->ythickness / 2;
  thickness_dark = style->ythickness - thickness_light;
  
  cairo_set_line_width (cr, 1.0);

  if (detail && !strcmp (detail, "label"))
    {
      if (state_type == GTK_STATE_INSENSITIVE)
        _cairo_draw_line (cr, &style->white, x1 + 1, y + 1, x2 + 1, y + 1);
      _cairo_draw_line (cr, &style->fg[state_type], x1, y, x2, y);
    }
  else
    {
      for (i = 0; i < thickness_dark; i++)
        {
          _cairo_draw_line (cr, &style->dark[state_type], x1, y + i, x2 - i - 1, y + i);
          _cairo_draw_line (cr, &style->light[state_type], x2 - i, y + i, x2, y + i);
        }
      
      y += thickness_dark;
      for (i = 0; i < thickness_light; i++)
        {
          _cairo_draw_line (cr, &style->dark[state_type], x1, y + i, x1 + thickness_light - i - 1, y + i);
          _cairo_draw_line (cr, &style->light[state_type], x1 + thickness_light - i, y + i, x2, y + i);
        }
    }
}


static void
gtk_default_draw_vline (GtkStyle      *style,
                        cairo_t       *cr,
                        GtkStateType  state_type,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        gint          y1,
                        gint          y2,
                        gint          x)
{
  gint thickness_light;
  gint thickness_dark;
  gint i;
  
  thickness_light = style->xthickness / 2;
  thickness_dark = style->xthickness - thickness_light;

  cairo_set_line_width (cr, 1.0);

  for (i = 0; i < thickness_dark; i++)
    { 
      _cairo_draw_line (cr, &style->dark[state_type],
                        x + i, y1, x + i, y2 - i - 1);
      _cairo_draw_line (cr, &style->light[state_type],
                        x + i, y2 - i, x + i, y2);
    }
  
  x += thickness_dark;
  for (i = 0; i < thickness_light; i++)
    {
      _cairo_draw_line (cr, &style->dark[state_type],
                        x + i, y1, x + i, y1 + thickness_light - i - 1);
      _cairo_draw_line (cr, &style->light[state_type],
                        x + i, y1 + thickness_light - i, x + i, y2);
    }
}

static void
draw_thin_shadow (GtkStyle      *style,
		  cairo_t       *cr,
		  GtkStateType   state,
		  gint           x,
		  gint           y,
		  gint           width,
		  gint           height)
{
  GdkColor *gc1, *gc2;

  gc1 = &style->light[state];
  gc2 = &style->dark[state];

  _cairo_draw_line (cr, gc1,
                    x, y + height - 1, x + width - 1, y + height - 1);
  _cairo_draw_line (cr, gc1,
                    x + width - 1, y,  x + width - 1, y + height - 1);
      
  _cairo_draw_line (cr, gc2,
                    x, y, x + width - 2, y);
  _cairo_draw_line (cr, gc2,
                    x, y, x, y + height - 2);
}

static void
draw_spinbutton_shadow (GtkStyle        *style,
			cairo_t         *cr,
			GtkStateType     state,
			GtkTextDirection direction,
			gint             x,
			gint             y,
			gint             width,
			gint             height)
{

  if (direction == GTK_TEXT_DIR_LTR)
    {
      _cairo_draw_line (cr, &style->dark[state],
                        x, y, x + width - 1, y);
      _cairo_draw_line (cr, &style->black,
                        x, y + 1, x + width - 2, y + 1);
      _cairo_draw_line (cr, &style->black,
                        x + width - 2, y + 2, x + width - 2, y + height - 3);
      _cairo_draw_line (cr, &style->light[state],
                        x + width - 1, y + 1, x + width - 1, y + height - 2);
      _cairo_draw_line (cr, &style->light[state],
                        x, y + height - 1, x + width - 1, y + height - 1);
      _cairo_draw_line (cr, &style->bg[state],
                        x, y + height - 2, x + width - 2, y + height - 2);
      _cairo_draw_line (cr, &style->black,
                        x, y + 2, x, y + height - 3);
    }
  else
    {
      _cairo_draw_line (cr, &style->dark[state],
                        x, y, x + width - 1, y);
      _cairo_draw_line (cr, &style->dark[state],
                        x, y + 1, x, y + height - 1);
      _cairo_draw_line (cr, &style->black,
                        x + 1, y + 1, x + width - 1, y + 1);
      _cairo_draw_line (cr, &style->black,
                        x + 1, y + 2, x + 1, y + height - 2);
      _cairo_draw_line (cr, &style->black,
                        x + width - 1, y + 2, x + width - 1, y + height - 3);
      _cairo_draw_line (cr, &style->light[state],
                        x + 1, y + height - 1, x + width - 1, y + height - 1);
      _cairo_draw_line (cr, &style->bg[state],
                        x + 2, y + height - 2, x + width - 1, y + height - 2);
    }
}

static void
draw_menu_shadow (GtkStyle        *style,
		  cairo_t         *cr,
		  GtkStateType     state,
		  gint             x,
		  gint             y,
		  gint             width,
		  gint             height)
{
  if (style->ythickness > 0)
    {
      if (style->ythickness > 1)
	{
	  _cairo_draw_line (cr, &style->dark[state],
                            x + 1, y + height - 2,
                            x + width - 2, y + height - 2);
	  _cairo_draw_line (cr, &style->black,
                            x, y + height - 1, x + width - 1, y + height - 1);
	}
      else
	{
	  _cairo_draw_line (cr, &style->dark[state],
                            x + 1, y + height - 1, x + width - 1, y + height - 1);
	}
    }
  
  if (style->xthickness > 0)
    {
      if (style->xthickness > 1)
	{
	  _cairo_draw_line (cr, &style->dark[state],
                            x + width - 2, y + 1,
                            x + width - 2, y + height - 2);

	  _cairo_draw_line (cr, &style->black,
                            x + width - 1, y, x + width - 1, y + height - 1);
	}
      else
	{
	  _cairo_draw_line (cr, &style->dark[state],
                            x + width - 1, y + 1, x + width - 1, y + height - 1);
	}
    }
  
  /* Light around top and left */
  
  if (style->ythickness > 0)
    _cairo_draw_line (cr, &style->black,
		   x, y, x + width - 2, y);
  if (style->xthickness > 0)
    _cairo_draw_line (cr, &style->black,
                      x, y, x, y + height - 2);
  
  if (style->ythickness > 1)
    _cairo_draw_line (cr, &style->light[state],
                      x + 1, y + 1, x + width - 3, y + 1);
  if (style->xthickness > 1)
    _cairo_draw_line (cr, &style->light[state],
                      x + 1, y + 1, x + 1, y + height - 3);
}

static GtkTextDirection
get_direction (GtkWidget *widget)
{
  GtkTextDirection dir;
  
  if (widget)
    dir = gtk_widget_get_direction (widget);
  else
    dir = GTK_TEXT_DIR_LTR;
  
  return dir;
}


static void
gtk_default_draw_shadow (GtkStyle      *style,
                         cairo_t       *cr,
                         GtkStateType   state_type,
                         GtkShadowType  shadow_type,
                         GtkWidget     *widget,
                         const gchar   *detail,
                         gint           x,
                         gint           y,
                         gint           width,
                         gint           height)
{
  GdkColor *gc1 = NULL;
  GdkColor *gc2 = NULL;
  gint thickness_light;
  gint thickness_dark;
  gint i;

  cairo_set_line_width (cr, 1.0);

  if (shadow_type == GTK_SHADOW_IN)
    {
      if (detail && strcmp (detail, "buttondefault") == 0)
	{
          _cairo_draw_rectangle (cr, &style->black, FALSE,
                                 x, y, width - 1, height - 1);

	  return;
	}
      if (detail && strcmp (detail, "trough") == 0)
	{
          draw_thin_shadow (style, cr, state_type,
                            x, y, width, height);

	  return;
	}
      if (GTK_IS_SPIN_BUTTON (widget) &&
         detail && strcmp (detail, "spinbutton") == 0)
	{
	  draw_spinbutton_shadow (style, cr, state_type, 
				  get_direction (widget), x, y, width, height);
	  
	  return;
	}
    }

  if (shadow_type == GTK_SHADOW_OUT && detail && strcmp (detail, "menu") == 0)
    {
      draw_menu_shadow (style, cr, state_type, x, y, width, height);
      return;
    }
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      return;
    case GTK_SHADOW_IN:
    case GTK_SHADOW_ETCHED_IN:
      gc1 = &style->light[state_type];
      gc2 = &style->dark[state_type];
      break;
    case GTK_SHADOW_OUT:
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = &style->dark[state_type];
      gc2 = &style->light[state_type];
      break;
    }
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      break;
      
    case GTK_SHADOW_IN:
      /* Light around right and bottom edge */

      if (style->ythickness > 0)
        _cairo_draw_line (cr, gc1,
                          x, y + height - 1, x + width - 1, y + height - 1);
      if (style->xthickness > 0)
        _cairo_draw_line (cr, gc1,
                          x + width - 1, y, x + width - 1, y + height - 1);

      if (style->ythickness > 1)
        _cairo_draw_line (cr, &style->bg[state_type],
                          x + 1, y + height - 2, x + width - 2, y + height - 2);
      if (style->xthickness > 1)
        _cairo_draw_line (cr, &style->bg[state_type],
                          x + width - 2, y + 1, x + width - 2, y + height - 2);

      /* Dark around left and top */

      if (style->ythickness > 1)
        _cairo_draw_line (cr, &style->black,
                          x + 1, y + 1, x + width - 2, y + 1);
      if (style->xthickness > 1)
        _cairo_draw_line (cr, &style->black,
                          x + 1, y + 1, x + 1, y + height - 2);

      if (style->ythickness > 0)
        _cairo_draw_line (cr, gc2,
                          x, y, x + width - 1, y);
      if (style->xthickness > 0)
        _cairo_draw_line (cr, gc2,
                          x, y, x, y + height - 1);
      break;
      
    case GTK_SHADOW_OUT:
      /* Dark around right and bottom edge */

      if (style->ythickness > 0)
        {
          if (style->ythickness > 1)
            {
              _cairo_draw_line (cr, gc1,
                                x + 1, y + height - 2, x + width - 2, y + height - 2);
              _cairo_draw_line (cr, &style->black,
                                x, y + height - 1, x + width - 1, y + height - 1);
            }
          else
            {
              _cairo_draw_line (cr, gc1,
                                x + 1, y + height - 1, x + width - 1, y + height - 1);
            }
        }

      if (style->xthickness > 0)
        {
          if (style->xthickness > 1)
            {
              _cairo_draw_line (cr, gc1,
                                x + width - 2, y + 1, x + width - 2, y + height - 2);
              
              _cairo_draw_line (cr, &style->black,
                                x + width - 1, y, x + width - 1, y + height - 1);
            }
          else
            {
              _cairo_draw_line (cr, gc1,
                                x + width - 1, y + 1, x + width - 1, y + height - 1);
            }
        }
      
      /* Light around top and left */

      if (style->ythickness > 0)
        _cairo_draw_line (cr, gc2,
                          x, y, x + width - 2, y);
      if (style->xthickness > 0)
        _cairo_draw_line (cr, gc2,
                          x, y, x, y + height - 2);

      if (style->ythickness > 1)
        _cairo_draw_line (cr, &style->bg[state_type],
                          x + 1, y + 1, x + width - 3, y + 1);
      if (style->xthickness > 1)
        _cairo_draw_line (cr, &style->bg[state_type],
                          x + 1, y + 1, x + 1, y + height - 3);
      break;
      
    case GTK_SHADOW_ETCHED_IN:
    case GTK_SHADOW_ETCHED_OUT:
      if (style->xthickness > 0)
        {
          if (style->xthickness > 1)
            {
              thickness_light = 1;
              thickness_dark = 1;
      
              for (i = 0; i < thickness_dark; i++)
                {
                  _cairo_draw_line (cr, gc1,
                                    x + width - i - 1,
                                    y + i,
                                    x + width - i - 1,
                                    y + height - i - 1);
                  _cairo_draw_line (cr, gc2,
                                    x + i,
                                    y + i,
                                    x + i,
                                    y + height - i - 2);
                }
      
              for (i = 0; i < thickness_light; i++)
                {
                  _cairo_draw_line (cr, gc1,
                                    x + thickness_dark + i,
                                    y + thickness_dark + i,
                                    x + thickness_dark + i,
                                    y + height - thickness_dark - i - 1);
                  _cairo_draw_line (cr, gc2,
                                    x + width - thickness_light - i - 1,
                                    y + thickness_dark + i,
                                    x + width - thickness_light - i - 1,
                                    y + height - thickness_light - 1);
                }
            }
          else
            {
              _cairo_draw_line (cr,
                                &style->dark[state_type],
                                x, y, x, y + height);
              _cairo_draw_line (cr,
                                &style->dark[state_type],
                                x + width, y, x + width, y + height);
            }
        }

      if (style->ythickness > 0)
        {
          if (style->ythickness > 1)
            {
              thickness_light = 1;
              thickness_dark = 1;
      
              for (i = 0; i < thickness_dark; i++)
                {
                  _cairo_draw_line (cr, gc1,
                                    x + i,
                                    y + height - i - 1,
                                    x + width - i - 1,
                                    y + height - i - 1);
          
                  _cairo_draw_line (cr, gc2,
                                    x + i,
                                    y + i,
                                    x + width - i - 2,
                                    y + i);
                }
      
              for (i = 0; i < thickness_light; i++)
                {
                  _cairo_draw_line (cr, gc1,
                                    x + thickness_dark + i,
                                    y + thickness_dark + i,
                                    x + width - thickness_dark - i - 2,
                                    y + thickness_dark + i);
          
                  _cairo_draw_line (cr, gc2,
                                    x + thickness_dark + i,
                                    y + height - thickness_light - i - 1,
                                    x + width - thickness_light - 1,
                                    y + height - thickness_light - i - 1);
                }
            }
          else
            {
              _cairo_draw_line (cr,
                                &style->dark[state_type],
                                x, y, x + width, y);
              _cairo_draw_line (cr,
                                &style->dark[state_type],
                                x, y + height, x + width, y + height);
            }
        }
      
      break;
    }

  if (shadow_type == GTK_SHADOW_IN &&
      GTK_IS_SPIN_BUTTON (widget) &&
      detail && strcmp (detail, "entry") == 0)
    {
      if (get_direction (widget) == GTK_TEXT_DIR_LTR)
	{
          _cairo_draw_line (cr,
                            &style->base[state_type],
                            x + width - 1, y + 2,
                            x + width - 1, y + height - 3);
          _cairo_draw_line (cr,
                            &style->base[state_type],
                            x + width - 2, y + 2,
                            x + width - 2, y + height - 3);
          /* draw point */
          _cairo_draw_point (cr,
                             &style->black,
                             x + width - 1, y + 1);
          _cairo_draw_point (cr,
                             &style->bg[state_type],
                             x + width - 1, y + height - 2);
	}
      else
	{
          _cairo_draw_line (cr,
                            &style->base[state_type],
                            x, y + 2,
                            x, y + height - 3);
          _cairo_draw_line (cr,
                            &style->base[state_type],
                            x + 1, y + 2,
                            x + 1, y + height - 3);

          _cairo_draw_point (cr,
                             &style->black,
                             x, y + 1);

          _cairo_draw_line (cr,
                            &style->bg[state_type],
                            x, y + height - 2,
                            x + 1, y + height - 2);
          _cairo_draw_point (cr,
                             &style->light[state_type],
                             x, y + height - 1);
	}
    }
}

static void
draw_arrow (cairo_t       *cr,
	    GdkColor      *color,
	    GtkArrowType   arrow_type,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  gdk_cairo_set_source_color (cr, color);
  cairo_save (cr);
    
  if (arrow_type == GTK_ARROW_DOWN)
    {
      cairo_move_to (cr, x,              y);
      cairo_line_to (cr, x + width,      y);
      cairo_line_to (cr, x + width / 2., y + height);
    }
  else if (arrow_type == GTK_ARROW_UP)
    {
      cairo_move_to (cr, x,              y + height);
      cairo_line_to (cr, x + width / 2., y);
      cairo_line_to (cr, x + width,      y + height);
    }
  else if (arrow_type == GTK_ARROW_LEFT)
    {
      cairo_move_to (cr, x + width,      y);
      cairo_line_to (cr, x + width,      y + height);
      cairo_line_to (cr, x,              y + height / 2.);
    }
  else if (arrow_type == GTK_ARROW_RIGHT)
    {
      cairo_move_to (cr, x,              y);
      cairo_line_to (cr, x + width,      y + height / 2.);
      cairo_line_to (cr, x,              y + height);
    }

  cairo_close_path (cr);
  cairo_fill (cr);

  cairo_restore (cr);
}

static void
calculate_arrow_geometry (GtkArrowType  arrow_type,
			  gint         *x,
			  gint         *y,
			  gint         *width,
			  gint         *height)
{
  gint w = *width;
  gint h = *height;
  
  switch (arrow_type)
    {
    case GTK_ARROW_UP:
    case GTK_ARROW_DOWN:
      w += (w % 2) - 1;
      h = (w / 2 + 1);
      
      if (h > *height)
	{
	  h = *height;
	  w = 2 * h - 1;
	}
      
      if (arrow_type == GTK_ARROW_DOWN)
	{
	  if (*height % 2 == 1 || h % 2 == 0)
	    *height += 1;
	}
      else
	{
	  if (*height % 2 == 0 || h % 2 == 0)
	    *height -= 1;
	}
      break;

    case GTK_ARROW_RIGHT:
    case GTK_ARROW_LEFT:
      h += (h % 2) - 1;
      w = (h / 2 + 1);
      
      if (w > *width)
	{
	  w = *width;
	  h = 2 * w - 1;
	}
      
      if (arrow_type == GTK_ARROW_RIGHT)
	{
	  if (*width % 2 == 1 || w % 2 == 0)
	    *width += 1;
	}
      else
	{
	  if (*width % 2 == 0 || w % 2 == 0)
	    *width -= 1;
	}
      break;
      
    default:
      /* should not be reached */
      break;
    }

  *x += (*width - w) / 2;
  *y += (*height - h) / 2;
  *height = h;
  *width = w;
}

static void
gtk_default_draw_arrow (GtkStyle      *style,
			cairo_t       *cr,
			GtkStateType   state,
			GtkShadowType  shadow,
			GtkWidget     *widget,
			const gchar   *detail,
			GtkArrowType   arrow_type,
			gboolean       fill,
			gint           x,
			gint           y,
			gint           width,
			gint           height)
{
  calculate_arrow_geometry (arrow_type, &x, &y, &width, &height);

  if (detail && strcmp (detail, "menu_scroll_arrow_up") == 0)
    y++;

  if (state == GTK_STATE_INSENSITIVE)
    draw_arrow (cr, &style->white, arrow_type,
		x + 1, y + 1, width, height);
  draw_arrow (cr, &style->fg[state], arrow_type,
	      x, y, width, height);
}

static void
gtk_default_draw_diamond (GtkStyle      *style,
                          cairo_t       *cr,
                          GtkStateType   state_type,
                          GtkShadowType  shadow_type,
                          GtkWidget     *widget,
                          const gchar   *detail,
                          gint           x,
                          gint           y,
                          gint           width,
                          gint           height)
{
  gint half_width;
  gint half_height;
  GdkColor *outer_nw = NULL;
  GdkColor *outer_ne = NULL;
  GdkColor *outer_sw = NULL;
  GdkColor *outer_se = NULL;
  GdkColor *middle_nw = NULL;
  GdkColor *middle_ne = NULL;
  GdkColor *middle_sw = NULL;
  GdkColor *middle_se = NULL;
  GdkColor *inner_nw = NULL;
  GdkColor *inner_ne = NULL;
  GdkColor *inner_sw = NULL;
  GdkColor *inner_se = NULL;
  
  half_width = width / 2;
  half_height = height / 2;
  
  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      inner_sw = inner_se = &style->bg[state_type];
      middle_sw = middle_se = &style->light[state_type];
      outer_sw = outer_se = &style->light[state_type];
      inner_nw = inner_ne = &style->black;
      middle_nw = middle_ne = &style->dark[state_type];
      outer_nw = outer_ne = &style->dark[state_type];
      break;
          
    case GTK_SHADOW_OUT:
      inner_sw = inner_se = &style->dark[state_type];
      middle_sw = middle_se = &style->dark[state_type];
      outer_sw = outer_se = &style->black;
      inner_nw = inner_ne = &style->bg[state_type];
      middle_nw = middle_ne = &style->light[state_type];
      outer_nw = outer_ne = &style->light[state_type];
      break;

    case GTK_SHADOW_ETCHED_IN:
      inner_sw = inner_se = &style->bg[state_type];
      middle_sw = middle_se = &style->dark[state_type];
      outer_sw = outer_se = &style->light[state_type];
      inner_nw = inner_ne = &style->bg[state_type];
      middle_nw = middle_ne = &style->light[state_type];
      outer_nw = outer_ne = &style->dark[state_type];
      break;

    case GTK_SHADOW_ETCHED_OUT:
      inner_sw = inner_se = &style->bg[state_type];
      middle_sw = middle_se = &style->light[state_type];
      outer_sw = outer_se = &style->dark[state_type];
      inner_nw = inner_ne = &style->bg[state_type];
      middle_nw = middle_ne = &style->dark[state_type];
      outer_nw = outer_ne = &style->light[state_type];
      break;
      
    default:

      break;
    }

  if (inner_sw)
    {
      _cairo_draw_line (cr, inner_sw,
                        x + 2, y + half_height,
                        x + half_width, y + height - 2);
      _cairo_draw_line (cr, inner_se,
                        x + half_width, y + height - 2,
                        x + width - 2, y + half_height);
      _cairo_draw_line (cr, middle_sw,
                        x + 1, y + half_height,
                        x + half_width, y + height - 1);
      _cairo_draw_line (cr, middle_se,
                        x + half_width, y + height - 1,
                        x + width - 1, y + half_height);
      _cairo_draw_line (cr, outer_sw,
                        x, y + half_height,
                        x + half_width, y + height);
      _cairo_draw_line (cr, outer_se,
                        x + half_width, y + height,
                        x + width, y + half_height);
  
      _cairo_draw_line (cr, inner_nw,
                        x + 2, y + half_height,
                        x + half_width, y + 2);
      _cairo_draw_line (cr, inner_ne,
                        x + half_width, y + 2,
                        x + width - 2, y + half_height);
      _cairo_draw_line (cr, middle_nw,
                        x + 1, y + half_height,
                        x + half_width, y + 1);
      _cairo_draw_line (cr, middle_ne,
                        x + half_width, y + 1,
                        x + width - 1, y + half_height);
      _cairo_draw_line (cr, outer_nw,
                        x, y + half_height,
                        x + half_width, y);
      _cairo_draw_line (cr, outer_ne,
                        x + half_width, y,
                        x + width, y + half_height);
    }
}

static void
option_menu_get_props (GtkWidget      *widget,
		       GtkRequisition *indicator_size,
		       GtkBorder      *indicator_spacing)
{
  GtkRequisition *tmp_size = NULL;
  GtkBorder *tmp_spacing = NULL;

  if (tmp_size)
    {
      *indicator_size = *tmp_size;
      gtk_requisition_free (tmp_size);
    }
  else
    *indicator_size = default_option_indicator_size;

  if (tmp_spacing)
    {
      *indicator_spacing = *tmp_spacing;
      gtk_border_free (tmp_spacing);
    }
  else
    *indicator_spacing = default_option_indicator_spacing;
}

static gboolean
background_is_solid (GtkStyle     *style,
                     GtkStateType  type)
{
  if (style->background[type] == NULL)
    return FALSE;

  return cairo_pattern_get_type (style->background[type]) == CAIRO_PATTERN_TYPE_SOLID;
}

static void 
gtk_default_draw_box (GtkStyle      *style,
		      cairo_t       *cr,
		      GtkStateType   state_type,
		      GtkShadowType  shadow_type,
		      GtkWidget     *widget,
		      const gchar   *detail,
		      gint           x,
		      gint           y,
		      gint           width,
		      gint           height)
{
  gboolean is_spinbutton_box = FALSE;
  
  if (GTK_IS_SPIN_BUTTON (widget) && detail)
    {
      if (strcmp (detail, "spinbutton_up") == 0)
	{
	  y += 2;
	  width -= 3;
	  height -= 2;

	  if (get_direction (widget) == GTK_TEXT_DIR_RTL)
	    x += 2;
	  else
	    x += 1;

	  is_spinbutton_box = TRUE;
	}
      else if (strcmp (detail, "spinbutton_down") == 0)
	{
	  width -= 3;
	  height -= 2;

	  if (get_direction (widget) == GTK_TEXT_DIR_RTL)
	    x += 2;
	  else
	    x += 1;

	  is_spinbutton_box = TRUE;
	}
    }
  
  if (background_is_solid (style, state_type))
    {
      GdkColor *gc = &style->bg[state_type];

      if (state_type == GTK_STATE_SELECTED && detail && strcmp (detail, "paned") == 0)
	{
	  if (widget && !gtk_widget_has_focus (widget))
	    gc = &style->base[GTK_STATE_ACTIVE];
	}

      _cairo_draw_rectangle (cr, gc, TRUE,
                             x, y, width, height);
    }
  else
    gtk_style_apply_default_background (style, cr, gtk_widget_get_window (widget),
                                        state_type, x, y, width, height);


  if (is_spinbutton_box)
    {
      GdkColor *upper;
      GdkColor *lower;

      lower = &style->dark[state_type];
      if (shadow_type == GTK_SHADOW_OUT)
	upper = &style->light[state_type];
      else
	upper = &style->dark[state_type];

      _cairo_draw_line (cr, upper, x, y, x + width - 1, y);
      _cairo_draw_line (cr, lower, x, y + height - 1, x + width - 1, y + height - 1);

      return;
    }

  gtk_paint_shadow (style, cr, state_type, shadow_type, widget, detail,
                          x, y, width, height);

  if (detail && strcmp (detail, "optionmenu") == 0)
    {
      GtkRequisition indicator_size;
      GtkBorder indicator_spacing;
      gint vline_x;

      option_menu_get_props (widget, &indicator_size, &indicator_spacing);

      if (get_direction (widget) == GTK_TEXT_DIR_RTL)
	vline_x = x + indicator_size.width + indicator_spacing.left + indicator_spacing.right;
      else 
	vline_x = x + width - (indicator_size.width + indicator_spacing.left + indicator_spacing.right) - style->xthickness;

      gtk_paint_vline (style, cr, state_type, widget,
                             detail,
                             y + style->ythickness + 1,
                             y + height - style->ythickness - 3,
                             vline_x);
    }
}

static GdkColor *
get_darkened (const GdkColor *color,
                 gint            darken_count)
{
  GdkColor src = *color;
  GdkColor shaded = *color;
  
  while (darken_count)
    {
      _gtk_style_shade (&src, &shaded, 0.93);
      src = shaded;
      --darken_count;
    }
   
  return gdk_color_copy (&shaded);
}

static void 
gtk_default_draw_flat_box (GtkStyle      *style,
                           cairo_t       *cr,
                           GtkStateType   state_type,
                           GtkShadowType  shadow_type,
                           GtkWidget     *widget,
                           const gchar   *detail,
                           gint           x,
                           gint           y,
                           gint           width,
                           gint           height)
{
  GdkColor *gc1;
  GdkColor *freeme = NULL;
  
  cairo_set_line_width (cr, 1.0);

  if (detail)
    {
      int trimmed_len = strlen (detail);

      if (g_str_has_prefix (detail, "cell_"))
        {
          if (g_str_has_suffix (detail, "_start"))
            trimmed_len -= 6;
          else if (g_str_has_suffix (detail, "_middle"))
            trimmed_len -= 7;
          else if (g_str_has_suffix (detail, "_end"))
            trimmed_len -= 4;
        }

      if (state_type == GTK_STATE_SELECTED)
        {
          if (!strcmp ("text", detail))
            gc1 = &style->bg[GTK_STATE_SELECTED];
          else if (!strncmp ("cell_even", detail, trimmed_len) ||
                   !strncmp ("cell_odd", detail, trimmed_len) ||
                   !strncmp ("cell_even_ruled", detail, trimmed_len) ||
		   !strncmp ("cell_even_ruled_sorted", detail, trimmed_len))
            {
	      /* This has to be really broken; alex made me do it. -jrb */
	      if (widget && gtk_widget_has_focus (widget))
		gc1 = &style->base[state_type];
	      else
	        gc1 = &style->base[GTK_STATE_ACTIVE];
            }
	  else if (!strncmp ("cell_odd_ruled", detail, trimmed_len) ||
		   !strncmp ("cell_odd_ruled_sorted", detail, trimmed_len))
	    {
	      if (widget && gtk_widget_has_focus (widget))
	        freeme = get_darkened (&style->base[state_type], 1);
	      else
	        freeme = get_darkened (&style->base[GTK_STATE_ACTIVE], 1);
	      gc1 = freeme;
	    }
          else
            {
              gc1 = &style->bg[state_type];
            }
        }
      else
        {
          if (!strcmp ("viewportbin", detail))
            gc1 = &style->bg[GTK_STATE_NORMAL];
          else if (!strcmp ("entry_bg", detail))
            gc1 = &style->base[gtk_widget_get_state (widget)];

          /* For trees: even rows are base color, odd rows are a shade of
           * the base color, the sort column is a shade of the original color
           * for that row.
           */

          else if (!strncmp ("cell_even", detail, trimmed_len) ||
                   !strncmp ("cell_odd", detail, trimmed_len) ||
                   !strncmp ("cell_even_ruled", detail, trimmed_len))
            {
	      GdkColor *color = NULL;

	      gtk_widget_style_get (widget,
		                    "even-row-color", &color,
				    NULL);

	      if (color)
	        {
		  freeme = get_darkened (color, 0);
		  gc1 = freeme;

		  gdk_color_free (color);
		}
	      else
	        gc1 = &style->base[state_type];
            }
	  else if (!strncmp ("cell_odd_ruled", detail, trimmed_len))
	    {
	      GdkColor *color = NULL;

	      gtk_widget_style_get (widget,
		                    "odd-row-color", &color,
				    NULL);

	      if (color)
	        {
		  freeme = get_darkened (color, 0);
		  gc1 = freeme;

		  gdk_color_free (color);
		}
	      else
	        {
		  gtk_widget_style_get (widget,
		                        "even-row-color", &color,
					NULL);

		  if (color)
		    {
		      freeme = get_darkened (color, 1);
		      gdk_color_free (color);
		    }
		  else
		    freeme = get_darkened (&style->base[state_type], 1);
		  gc1 = freeme;
		}
	    }
          else if (!strncmp ("cell_even_sorted", detail, trimmed_len) ||
                   !strncmp ("cell_odd_sorted", detail, trimmed_len) ||
                   !strncmp ("cell_even_ruled_sorted", detail, trimmed_len))
            {
	      GdkColor *color = NULL;

	      if (!strncmp ("cell_odd_sorted", detail, trimmed_len))
	        gtk_widget_style_get (widget,
		                      "odd-row-color", &color,
				      NULL);
	      else
	        gtk_widget_style_get (widget,
		                      "even-row-color", &color,
				      NULL);

	      if (color)
	        {
		  freeme = get_darkened (color, 1);
		  gc1 = freeme;

		  gdk_color_free (color);
		}
	      else
		{
	          freeme = get_darkened (&style->base[state_type], 1);
                  gc1 = freeme;
		}
            }
          else if (!strncmp ("cell_odd_ruled_sorted", detail, trimmed_len))
            {
	      GdkColor *color = NULL;

	      gtk_widget_style_get (widget,
		                    "odd-row-color", &color,
				    NULL);

	      if (color)
	        {
		  freeme = get_darkened (color, 1);
		  gc1 = freeme;

		  gdk_color_free (color);
		}
	      else
	        {
		  gtk_widget_style_get (widget,
		                        "even-row-color", &color,
					NULL);

		  if (color)
		    {
		      freeme = get_darkened (color, 2);
		      gdk_color_free (color);
		    }
		  else
                    freeme = get_darkened (&style->base[state_type], 2);
                  gc1 = freeme;
	        }
            }
          else
            gc1 = &style->bg[state_type];
        }
    }
  else
    gc1 = &style->bg[state_type];
  
  if (background_is_solid (style, state_type) || gc1 != &style->bg[state_type])
    {
      _cairo_draw_rectangle (cr, gc1, TRUE,
                             x, y, width, height);

      if (detail && !strcmp ("tooltip", detail))
        _cairo_draw_rectangle (cr, &style->black, FALSE,
                               x, y, width - 1, height - 1);
    }
  else
    gtk_style_apply_default_background (style, cr, gtk_widget_get_window (widget),
                                        state_type, x, y, width, height);

  if (freeme)
    gdk_color_free (freeme);
}

static void 
gtk_default_draw_check (GtkStyle      *style,
			cairo_t       *cr,
			GtkStateType   state_type,
			GtkShadowType  shadow_type,
			GtkWidget     *widget,
			const gchar   *detail,
			gint           x,
			gint           y,
			gint           width,
			gint           height)
{
  enum { BUTTON, MENU, CELL } type = BUTTON;
  int exterior_size;
  int interior_size;
  int pad;
  
  if (detail)
    {
      if (strcmp (detail, "cellcheck") == 0)
	type = CELL;
      else if (strcmp (detail, "check") == 0)
	type = MENU;
    }
      
  exterior_size = MIN (width, height);
  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;

  pad = style->xthickness + MAX (1, (exterior_size - 2 * style->xthickness) / 9);
  interior_size = MAX (1, exterior_size - 2 * pad);

  if (interior_size < 7)
    {
      interior_size = 7;
      pad = MAX (0, (exterior_size - interior_size) / 2);
    }

  x -= (1 + exterior_size - width) / 2;
  y -= (1 + exterior_size - height) / 2;

  switch (type)
    {
    case BUTTON:
    case CELL:
      if (type == BUTTON)
	gdk_cairo_set_source_color (cr, &style->fg[state_type]);
      else
	gdk_cairo_set_source_color (cr, &style->text[state_type]);
	
      cairo_set_line_width (cr, 1.0);
      cairo_rectangle (cr, x + 0.5, y + 0.5, exterior_size - 1, exterior_size - 1);
      cairo_stroke (cr);

      gdk_cairo_set_source_color (cr, &style->base[state_type]);
      cairo_rectangle (cr, x + 1, y + 1, exterior_size - 2, exterior_size - 2);
      cairo_fill (cr);
      break;

    case MENU:
      break;
    }
      
  switch (type)
    {
    case BUTTON:
    case CELL:
      gdk_cairo_set_source_color (cr, &style->text[state_type]);
      break;
    case MENU:
      gdk_cairo_set_source_color (cr, &style->fg[state_type]);
      break;
    }

  if (shadow_type == GTK_SHADOW_IN)
    {
      cairo_translate (cr,
		       x + pad, y + pad);
      
      cairo_scale (cr, interior_size / 7., interior_size / 7.);
      
      cairo_move_to  (cr, 7.0, 0.0);
      cairo_line_to  (cr, 7.5, 1.0);
      cairo_curve_to (cr, 5.3, 2.0,
		      4.3, 4.0,
		      3.5, 7.0);
      cairo_curve_to (cr, 3.0, 5.7,
		      1.3, 4.7,
		      0.0, 4.7);
      cairo_line_to  (cr, 0.2, 3.5);
      cairo_curve_to (cr, 1.1, 3.5,
		      2.3, 4.3,
		      3.0, 5.0);
      cairo_curve_to (cr, 1.0, 3.9,
		      2.4, 4.1,
		      3.2, 4.9);
      cairo_curve_to (cr, 3.5, 3.1,
		      5.2, 2.0,
		      7.0, 0.0);
      
      cairo_fill (cr);
    }
  else if (shadow_type == GTK_SHADOW_ETCHED_IN) /* inconsistent */
    {
      int line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
		       x + pad,
		       y + pad + (1 + interior_size - line_thickness) / 2,
		       interior_size,
		       line_thickness);
      cairo_fill (cr);
    }
}

static void 
gtk_default_draw_option (GtkStyle      *style,
			 cairo_t       *cr,
			 GtkStateType   state_type,
			 GtkShadowType  shadow_type,
			 GtkWidget     *widget,
			 const gchar   *detail,
			 gint           x,
			 gint           y,
			 gint           width,
			 gint           height)
{
  enum { BUTTON, MENU, CELL } type = BUTTON;
  int exterior_size;
  
  if (detail)
    {
      if (strcmp (detail, "radio") == 0)
	type = CELL;
      else if (strcmp (detail, "option") == 0)
	type = MENU;
    }
      
  exterior_size = MIN (width, height);
  if (exterior_size % 2 == 0) /* Ensure odd */
    exterior_size -= 1;
  
  x -= (1 + exterior_size - width) / 2;
  y -= (1 + exterior_size - height) / 2;

  switch (type)
    {
    case BUTTON:
    case CELL:
      gdk_cairo_set_source_color (cr, &style->base[state_type]);
      
      cairo_arc (cr,
		 x + exterior_size / 2.,
		 y + exterior_size / 2.,
		 (exterior_size - 1) / 2.,
		 0, 2 * G_PI);

      cairo_fill_preserve (cr);

      if (type == BUTTON)
	gdk_cairo_set_source_color (cr, &style->fg[state_type]);
      else
	gdk_cairo_set_source_color (cr, &style->text[state_type]);
	
      cairo_set_line_width (cr, 1.);
      cairo_stroke (cr);
      break;

    case MENU:
      break;
    }
      
  switch (type)
    {
    case BUTTON:
      gdk_cairo_set_source_color (cr, &style->text[state_type]);
      break;
    case CELL:
      break;
    case MENU:
      gdk_cairo_set_source_color (cr, &style->fg[state_type]);
      break;
    }

  if (shadow_type == GTK_SHADOW_IN)
    {
      int pad = style->xthickness + MAX (1, 2 * (exterior_size - 2 * style->xthickness) / 9);
      int interior_size = MAX (1, exterior_size - 2 * pad);

      if (interior_size < 5)
	{
	  interior_size = 7;
	  pad = MAX (0, (exterior_size - interior_size) / 2);
	}

      cairo_arc (cr,
		 x + pad + interior_size / 2.,
		 y + pad + interior_size / 2.,
		 interior_size / 2.,
		 0, 2 * G_PI);
      cairo_fill (cr);
    }
  else if (shadow_type == GTK_SHADOW_ETCHED_IN) /* inconsistent */
    {
      int pad = style->xthickness + MAX (1, (exterior_size - 2 * style->xthickness) / 9);
      int interior_size = MAX (1, exterior_size - 2 * pad);
      int line_thickness;

      if (interior_size < 7)
	{
	  interior_size = 7;
	  pad = MAX (0, (exterior_size - interior_size) / 2);
	}

      line_thickness = MAX (1, (3 + interior_size * 2) / 7);

      cairo_rectangle (cr,
		       x + pad,
		       y + pad + (interior_size - line_thickness) / 2.,
		       interior_size,
		       line_thickness);
      cairo_fill (cr);
    }
}

static void
gtk_default_draw_tab (GtkStyle      *style,
		      cairo_t       *cr,
		      GtkStateType   state_type,
		      GtkShadowType  shadow_type,
		      GtkWidget     *widget,
		      const gchar   *detail,
		      gint           x,
		      gint           y,
		      gint           width,
		      gint           height)
{
#define ARROW_SPACE 4

  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;
  gint arrow_height;

  option_menu_get_props (widget, &indicator_size, &indicator_spacing);

  indicator_size.width += (indicator_size.width % 2) - 1;
  arrow_height = indicator_size.width / 2 + 1;

  x += (width - indicator_size.width) / 2;
  y += (height - (2 * arrow_height + ARROW_SPACE)) / 2;

  if (state_type == GTK_STATE_INSENSITIVE)
    {
      draw_arrow (cr, &style->white,
		  GTK_ARROW_UP, x + 1, y + 1,
		  indicator_size.width, arrow_height);
      
      draw_arrow (cr, &style->white,
		  GTK_ARROW_DOWN, x + 1, y + arrow_height + ARROW_SPACE + 1,
		  indicator_size.width, arrow_height);
    }
  
  draw_arrow (cr, &style->fg[state_type],
	      GTK_ARROW_UP, x, y,
	      indicator_size.width, arrow_height);
  
  
  draw_arrow (cr, &style->fg[state_type],
	      GTK_ARROW_DOWN, x, y + arrow_height + ARROW_SPACE,
	      indicator_size.width, arrow_height);
}

static void 
gtk_default_draw_shadow_gap (GtkStyle       *style,
                             cairo_t        *cr,
                             GtkStateType    state_type,
                             GtkShadowType   shadow_type,
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
  GdkColor *color1 = NULL;
  GdkColor *color2 = NULL;
  GdkColor *color3 = NULL;
  GdkColor *color4 = NULL;
  
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
    default:
      return;
    case GTK_SHADOW_IN:
      color1 = &style->dark[state_type];
      color2 = &style->black;
      color3 = &style->bg[state_type];
      color4 = &style->light[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      color1 = &style->dark[state_type];
      color2 = &style->light[state_type];
      color3 = &style->dark[state_type];
      color4 = &style->light[state_type];
      break;
    case GTK_SHADOW_OUT:
      color1 = &style->light[state_type];
      color2 = &style->bg[state_type];
      color3 = &style->dark[state_type];
      color4 = &style->black;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      color1 = &style->light[state_type];
      color2 = &style->dark[state_type];
      color3 = &style->light[state_type];
      color4 = &style->dark[state_type];
      break;
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
          _cairo_draw_line (cr, color1,
                            x, y, x, y + height - 1);
          _cairo_draw_line (cr, color2,
                            x + 1, y, x + 1, y + height - 2);
          
          _cairo_draw_line (cr, color3,
                            x + 1, y + height - 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, color3,
                            x + width - 2, y, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, color4,
                            x, y + height - 1, x + width - 1, y + height - 1);
          _cairo_draw_line (cr, color4,
                            x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, color1,
                                x, y, x + gap_x - 1, y);
              _cairo_draw_line (cr, color2,
                                x + 1, y + 1, x + gap_x - 1, y + 1);
              _cairo_draw_line (cr, color2,
                                x + gap_x, y, x + gap_x, y);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, color1,
                                x + gap_x + gap_width, y, x + width - 2, y);
              _cairo_draw_line (cr, color2,
                                x + gap_x + gap_width, y + 1, x + width - 3, y + 1);
              _cairo_draw_line (cr, color2,
                                x + gap_x + gap_width - 1, y, x + gap_x + gap_width - 1, y);
            }
          break;
        case GTK_POS_BOTTOM:
          _cairo_draw_line (cr, color1,
                            x, y, x + width - 1, y);
          _cairo_draw_line (cr, color1,
                            x, y, x, y + height - 1);
          _cairo_draw_line (cr, color2,
                            x + 1, y + 1, x + width - 2, y + 1);
          _cairo_draw_line (cr, color2,
                            x + 1, y + 1, x + 1, y + height - 1);
          
          _cairo_draw_line (cr, color3,
                            x + width - 2, y + 1, x + width - 2, y + height - 1);
          _cairo_draw_line (cr, color4,
                            x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, color4,
                                x, y + height - 1, x + gap_x - 1, y + height - 1);
              _cairo_draw_line (cr, color3,
                                x + 1, y + height - 2, x + gap_x - 1, y + height - 2);
              _cairo_draw_line (cr, color3,
                                x + gap_x, y + height - 1, x + gap_x, y + height - 1);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, color4,
                                x + gap_x + gap_width, y + height - 1, x + width - 2, y + height - 1);
              _cairo_draw_line (cr, color3,
                                x + gap_x + gap_width, y + height - 2, x + width - 2, y + height - 2);
              _cairo_draw_line (cr, color3,
                                x + gap_x + gap_width - 1, y + height - 1, x + gap_x + gap_width - 1, y + height - 1);
            }
          break;
        case GTK_POS_LEFT:
          _cairo_draw_line (cr, color1,
                            x, y, x + width - 1, y);
          _cairo_draw_line (cr, color2,
                            x, y + 1, x + width - 2, y + 1);
          
          _cairo_draw_line (cr, color3,
                            x, y + height - 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, color3,
                            x + width - 2, y + 1, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, color4,
                            x, y + height - 1, x + width - 1, y + height - 1);
          _cairo_draw_line (cr, color4,
                            x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, color1,
                                x, y, x, y + gap_x - 1);
              _cairo_draw_line (cr, color2,
                                x + 1, y + 1, x + 1, y + gap_x - 1);
              _cairo_draw_line (cr, color2,
                                x, y + gap_x, x, y + gap_x);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, color1,
                                x, y + gap_x + gap_width, x, y + height - 2);
              _cairo_draw_line (cr, color2,
                                x + 1, y + gap_x + gap_width, x + 1, y + height - 2);
              _cairo_draw_line (cr, color2,
                                x, y + gap_x + gap_width - 1, x, y + gap_x + gap_width - 1);
            }
          break;
        case GTK_POS_RIGHT:
          _cairo_draw_line (cr, color1,
                            x, y, x + width - 1, y);
          _cairo_draw_line (cr, color1,
                            x, y, x, y + height - 1);
          _cairo_draw_line (cr, color2,
                            x + 1, y + 1, x + width - 1, y + 1);
          _cairo_draw_line (cr, color2,
                            x + 1, y + 1, x + 1, y + height - 2);
          
          _cairo_draw_line (cr, color3,
                            x + 1, y + height - 2, x + width - 1, y + height - 2);
          _cairo_draw_line (cr, color4,
                            x, y + height - 1, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, color4,
                                x + width - 1, y, x + width - 1, y + gap_x - 1);
              _cairo_draw_line (cr, color3,
                                x + width - 2, y + 1, x + width - 2, y + gap_x - 1);
              _cairo_draw_line (cr, color3,
                                x + width - 1, y + gap_x, x + width - 1, y + gap_x);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, color4,
                                x + width - 1, y + gap_x + gap_width, x + width - 1, y + height - 2);
              _cairo_draw_line (cr, color3,
                                x + width - 2, y + gap_x + gap_width, x + width - 2, y + height - 2);
              _cairo_draw_line (cr, color3,
                                x + width - 1, y + gap_x + gap_width - 1, x + width - 1, y + gap_x + gap_width - 1);
            }
          break;
        }
    }
}

static void 
gtk_default_draw_box_gap (GtkStyle       *style,
                          cairo_t        *cr,
                          GtkStateType    state_type,
                          GtkShadowType   shadow_type,
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
  GdkColor color1;
  GdkColor color2;
  GdkColor color3;
  GdkColor color4;
  
  gtk_style_apply_default_background (style, cr, gtk_widget_get_window (widget),
                                      state_type, x, y, width, height);

  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      return;
    case GTK_SHADOW_IN:
      color1 = style->dark[state_type];
      color2 = style->black;
      color3 = style->bg[state_type];
      color4 = style->light[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      color1 = style->dark[state_type];
      color2 = style->light[state_type];
      color3 = style->dark[state_type];
      color4 = style->light[state_type];
      break;
    case GTK_SHADOW_OUT:
      color1 = style->light[state_type];
      color2 = style->bg[state_type];
      color3 = style->dark[state_type];
      color4 = style->black;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      color1 = style->light[state_type];
      color2 = style->dark[state_type];
      color3 = style->light[state_type];
      color4 = style->dark[state_type];
      break;
    }
  
  cairo_set_line_width (cr, 1.0);

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
          _cairo_draw_line (cr, &color1,
                            x, y, x, y + height - 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y, x + 1, y + height - 2);
          
          _cairo_draw_line (cr, &color3,
                            x + 1, y + height - 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color3,
                            x + width - 2, y, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color4,
                            x, y + height - 1, x + width - 1, y + height - 1);
          _cairo_draw_line (cr, &color4,
                            x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, &color1,
                                x, y, x + gap_x - 1, y);
              _cairo_draw_line (cr, &color2,
                                x + 1, y + 1, x + gap_x - 1, y + 1);
              _cairo_draw_line (cr, &color2,
                                x + gap_x, y, x + gap_x, y);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, &color1,
                                x + gap_x + gap_width, y, x + width - 2, y);
              _cairo_draw_line (cr, &color2,
                                x + gap_x + gap_width, y + 1, x + width - 2, y + 1);
              _cairo_draw_line (cr, &color2,
                                x + gap_x + gap_width - 1, y, x + gap_x + gap_width - 1, y);
            }
          break;
        case  GTK_POS_BOTTOM:
          _cairo_draw_line (cr, &color1,
                            x, y, x + width - 1, y);
          _cairo_draw_line (cr, &color1,
                            x, y, x, y + height - 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + width - 2, y + 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + 1, y + height - 1);
          
          _cairo_draw_line (cr, &color3,
                            x + width - 2, y + 1, x + width - 2, y + height - 1);
          _cairo_draw_line (cr, &color4,
                            x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, &color4,
                                x, y + height - 1, x + gap_x - 1, y + height - 1);
              _cairo_draw_line (cr, &color3,
                                x + 1, y + height - 2, x + gap_x - 1, y + height - 2);
              _cairo_draw_line (cr, &color3,
                                x + gap_x, y + height - 1, x + gap_x, y + height - 1);
            }
          if ((width - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, &color4,
                                x + gap_x + gap_width, y + height - 1, x + width - 2, y + height - 1);
              _cairo_draw_line (cr, &color3,
                                x + gap_x + gap_width, y + height - 2, x + width - 2, y + height - 2);
              _cairo_draw_line (cr, &color3,
                                x + gap_x + gap_width - 1, y + height - 1, x + gap_x + gap_width - 1, y + height - 1);
            }
          break;
        case GTK_POS_LEFT:
          _cairo_draw_line (cr, &color1,
                            x, y, x + width - 1, y);
          _cairo_draw_line (cr, &color2,
                            x, y + 1, x + width - 2, y + 1);
          
          _cairo_draw_line (cr, &color3,
                            x, y + height - 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color3,
                            x + width - 2, y + 1, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color4,
                            x, y + height - 1, x + width - 1, y + height - 1);
          _cairo_draw_line (cr, &color4,
                            x + width - 1, y, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, &color1,
                                x, y, x, y + gap_x - 1);
              _cairo_draw_line (cr, &color2,
                                x + 1, y + 1, x + 1, y + gap_x - 1);
              _cairo_draw_line (cr, &color2,
                                x, y + gap_x, x, y + gap_x);
            }
          if ((height - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, &color1,
                                x, y + gap_x + gap_width, x, y + height - 2);
              _cairo_draw_line (cr, &color2,
                                x + 1, y + gap_x + gap_width, x + 1, y + height - 2);
              _cairo_draw_line (cr, &color2,
                                x, y + gap_x + gap_width - 1, x, y + gap_x + gap_width - 1);
            }
          break;
        case GTK_POS_RIGHT:
          _cairo_draw_line (cr, &color1,
                            x, y, x + width - 1, y);
          _cairo_draw_line (cr, &color1,
                            x, y, x, y + height - 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + width - 1, y + 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + 1, y + height - 2);
          
          _cairo_draw_line (cr, &color3,
                            x + 1, y + height - 2, x + width - 1, y + height - 2);
          _cairo_draw_line (cr, &color4,
                            x, y + height - 1, x + width - 1, y + height - 1);
          if (gap_x > 0)
            {
              _cairo_draw_line (cr, &color4,
                                x + width - 1, y, x + width - 1, y + gap_x - 1);
              _cairo_draw_line (cr, &color3,
                                x + width - 2, y + 1, x + width - 2, y + gap_x - 1);
              _cairo_draw_line (cr, &color3,
                                x + width - 1, y + gap_x, x + width - 1, y + gap_x);
            }
          if ((height - (gap_x + gap_width)) > 0)
            {
              _cairo_draw_line (cr, &color4,
                                x + width - 1, y + gap_x + gap_width, x + width - 1, y + height - 2);
              _cairo_draw_line (cr, &color3,
                                x + width - 2, y + gap_x + gap_width, x + width - 2, y + height - 2);
              _cairo_draw_line (cr, &color3,
                                x + width - 1, y + gap_x + gap_width - 1, x + width - 1, y + gap_x + gap_width - 1);
            }
          break;
        }
    }
}

static void 
gtk_default_draw_extension (GtkStyle       *style,
                            cairo_t        *cr,
                            GtkStateType    state_type,
                            GtkShadowType   shadow_type,
                            GtkWidget      *widget,
                            const gchar    *detail,
                            gint            x,
                            gint            y,
                            gint            width,
                            gint            height,
                            GtkPositionType gap_side)
{
  GdkWindow *window = gtk_widget_get_window (widget);
  GdkColor color1;
  GdkColor color2;
  GdkColor color3;
  GdkColor color4;
  
  switch (gap_side)
    {
    case GTK_POS_TOP:
      gtk_style_apply_default_background (style, cr, window,
                                          state_type,
                                          x + 1,
                                          y,
                                          width - 2,
                                          height - 1);
      break;
    case GTK_POS_BOTTOM:
      gtk_style_apply_default_background (style, cr, window,
                                          state_type,
                                          x + 1,
                                          y + 1,
                                          width - 2,
                                          height - 1);
      break;
    case GTK_POS_LEFT:
      gtk_style_apply_default_background (style, cr, window,
                                          state_type,
                                          x,
                                          y + 1,
                                          width - 1,
                                          height - 2);
      break;
    case GTK_POS_RIGHT:
      gtk_style_apply_default_background (style, cr, window,
                                          state_type,
                                          x + 1,
                                          y + 1,
                                          width - 1,
                                          height - 2);
      break;
    }

  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      return;
    case GTK_SHADOW_IN:
      color1 = style->dark[state_type];
      color2 = style->black;
      color3 = style->bg[state_type];
      color4 = style->light[state_type];
      break;
    case GTK_SHADOW_ETCHED_IN:
      color1 = style->dark[state_type];
      color2 = style->light[state_type];
      color3 = style->dark[state_type];
      color4 = style->light[state_type];
      break;
    case GTK_SHADOW_OUT:
      color1 = style->light[state_type];
      color2 = style->bg[state_type];
      color3 = style->dark[state_type];
      color4 = style->black;
      break;
    case GTK_SHADOW_ETCHED_OUT:
      color1 = style->light[state_type];
      color2 = style->dark[state_type];
      color3 = style->light[state_type];
      color4 = style->dark[state_type];
      break;
    }

  cairo_set_line_width (cr, 1.0);

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
          _cairo_draw_line (cr, &color1,
                            x, y, x, y + height - 2);
          _cairo_draw_line (cr, &color2,
                            x + 1, y, x + 1, y + height - 2);
          
          _cairo_draw_line (cr, &color3,
                            x + 2, y + height - 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color3,
                            x + width - 2, y, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color4,
                            x + 1, y + height - 1, x + width - 2, y + height - 1);
          _cairo_draw_line (cr, &color4,
                            x + width - 1, y, x + width - 1, y + height - 2);
          break;
        case GTK_POS_BOTTOM:
          _cairo_draw_line (cr, &color1,
                            x + 1, y, x + width - 2, y);
          _cairo_draw_line (cr, &color1,
                            x, y + 1, x, y + height - 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + width - 2, y + 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + 1, y + height - 1);
          
          _cairo_draw_line (cr, &color3,
                            x + width - 2, y + 2, x + width - 2, y + height - 1);
          _cairo_draw_line (cr, &color4,
                            x + width - 1, y + 1, x + width - 1, y + height - 1);
          break;
        case GTK_POS_LEFT:
          _cairo_draw_line (cr, &color1,
                            x, y, x + width - 2, y);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + width - 2, y + 1);
          
          _cairo_draw_line (cr, &color3,
                            x, y + height - 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color3,
                            x + width - 2, y + 2, x + width - 2, y + height - 2);
          _cairo_draw_line (cr, &color4,
                            x, y + height - 1, x + width - 2, y + height - 1);
          _cairo_draw_line (cr, &color4,
                            x + width - 1, y + 1, x + width - 1, y + height - 2);
          break;
        case GTK_POS_RIGHT:
          _cairo_draw_line (cr, &color1,
                            x + 1, y, x + width - 1, y);
          _cairo_draw_line (cr, &color1,
                            x, y + 1, x, y + height - 2);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + width - 1, y + 1);
          _cairo_draw_line (cr, &color2,
                            x + 1, y + 1, x + 1, y + height - 2);
          
          _cairo_draw_line (cr, &color3,
                            x + 2, y + height - 2, x + width - 1, y + height - 2);
          _cairo_draw_line (cr, &color4,
                            x + 1, y + height - 1, x + width - 1, y + height - 1);
          break;
        }
    }
}

static void 
gtk_default_draw_focus (GtkStyle      *style,
			cairo_t       *cr,
			GtkStateType   state_type,
			GtkWidget     *widget,
			const gchar   *detail,
			gint           x,
			gint           y,
			gint           width,
			gint           height)
{
  gboolean free_dash_list = FALSE;
  gint line_width = 1;
  gint8 *dash_list = (gint8 *) "\1\1";

  if (widget)
    {
      gtk_widget_style_get (widget,
			    "focus-line-width", &line_width,
			    "focus-line-pattern", (gchar *)&dash_list,
			    NULL);

      free_dash_list = TRUE;
  }

  if (detail && !strcmp (detail, "add-mode"))
    {
      if (free_dash_list)
	g_free (dash_list);

      dash_list = (gint8 *) "\4\4";
      free_dash_list = FALSE;
    }

  if (detail && !strcmp (detail, "colorwheel_light"))
    cairo_set_source_rgb (cr, 0., 0., 0.);
  else if (detail && !strcmp (detail, "colorwheel_dark"))
    cairo_set_source_rgb (cr, 1., 1., 1.);
  else
    gdk_cairo_set_source_color (cr, &style->fg[state_type]);

  cairo_set_line_width (cr, line_width);

  if (dash_list[0])
    {
      gint n_dashes = strlen ((const gchar *) dash_list);
      gdouble *dashes = g_new (gdouble, n_dashes);
      gdouble total_length = 0;
      gdouble dash_offset;
      gint i;

      for (i = 0; i < n_dashes; i++)
	{
	  dashes[i] = dash_list[i];
	  total_length += dash_list[i];
	}

      /* The dash offset here aligns the pattern to integer pixels
       * by starting the dash at the right side of the left border
       * Negative dash offsets in cairo don't work
       * (https://bugs.freedesktop.org/show_bug.cgi?id=2729)
       */
      dash_offset = - line_width / 2.;
      while (dash_offset < 0)
	dash_offset += total_length;
      
      cairo_set_dash (cr, dashes, n_dashes, dash_offset);
      g_free (dashes);
    }

  cairo_rectangle (cr,
		   x + line_width / 2.,
		   y + line_width / 2.,
		   width - line_width,
		   height - line_width);
  cairo_stroke (cr);

  if (free_dash_list)
    g_free (dash_list);
}

static void 
gtk_default_draw_slider (GtkStyle      *style,
                         cairo_t       *cr,
                         GtkStateType   state_type,
                         GtkShadowType  shadow_type,
                         GtkWidget     *widget,
                         const gchar   *detail,
                         gint           x,
                         gint           y,
                         gint           width,
                         gint           height,
                         GtkOrientation orientation)
{
  gtk_paint_box (style, cr, state_type, shadow_type,
                       widget, detail, x, y, width, height);

  if (detail &&
      (strcmp ("hscale", detail) == 0 ||
       strcmp ("vscale", detail) == 0))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_paint_vline (style, cr, state_type, widget, detail, 
                               y + style->ythickness, 
                               y + height - style->ythickness - 1, x + width / 2);
      else
        gtk_paint_hline (style, cr, state_type, widget, detail, 
                               x + style->xthickness, 
                               x + width - style->xthickness - 1, y + height / 2);
    }
}

static void
draw_dot (cairo_t    *cr,
	  GdkColor   *light,
	  GdkColor   *dark,
	  gint        x,
	  gint        y,
	  gushort     size)
{
  size = CLAMP (size, 2, 3);

  if (size == 2)
    {
      _cairo_draw_point (cr, light, x, y);
      _cairo_draw_point (cr, light, x+1, y+1);
    }
  else if (size == 3)
    {
      _cairo_draw_point (cr, light, x, y);
      _cairo_draw_point (cr, light, x+1, y);
      _cairo_draw_point (cr, light, x, y+1);
      _cairo_draw_point (cr, dark, x+1, y+2);
      _cairo_draw_point (cr, dark, x+2, y+1);
      _cairo_draw_point (cr, dark, x+2, y+2);
    }
}

static void 
gtk_default_draw_handle (GtkStyle      *style,
			 cairo_t       *cr,
			 GtkStateType   state_type,
			 GtkShadowType  shadow_type,
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
  GdkColor light, dark;
  
  gtk_paint_box (style, cr, state_type, shadow_type, widget, 
                       detail, x, y, width, height);
  
  if (detail && !strcmp (detail, "paned"))
    {
      /* we want to ignore the shadow border in paned widgets */
      xthick = 0;
      ythick = 0;

      if (state_type == GTK_STATE_SELECTED && widget && !gtk_widget_has_focus (widget))
	  _gtk_style_shade (&style->base[GTK_STATE_ACTIVE], &light,
                            LIGHTNESS_MULT);
      else
	light = style->light[state_type];

      dark = style->black;
    }
  else
    {
      xthick = style->xthickness;
      ythick = style->ythickness;

      light = style->light[state_type];
      dark = style->dark[state_type];
    }
  
  cairo_rectangle(cr, x + xthick, y + ythick,
                  width - (xthick * 2), height - (ythick * 2));
  cairo_clip (cr);

  if (detail && !strcmp (detail, "paned"))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	for (xx = x + width/2 - 15; xx <= x + width/2 + 15; xx += 5)
	  draw_dot (cr, &light, &dark, xx, y + height/2 - 1, 3);
      else
	for (yy = y + height/2 - 15; yy <= y + height/2 + 15; yy += 5)
	  draw_dot (cr, &light, &dark, x + width/2 - 1, yy, 3);
    }
  else
    {
      for (yy = y + ythick; yy < (y + height - ythick); yy += 3)
	for (xx = x + xthick; xx < (x + width - xthick); xx += 6)
	  {
	    draw_dot (cr, &light, &dark, xx, yy, 2);
	    draw_dot (cr, &light, &dark, xx + 3, yy + 1, 2);
	  }
    }
}

static void
gtk_default_draw_expander (GtkStyle        *style,
                           cairo_t         *cr,
                           GtkStateType     state_type,
                           GtkWidget       *widget,
                           const gchar     *detail,
                           gint             x,
                           gint             y,
			   GtkExpanderStyle expander_style)
{
#define DEFAULT_EXPANDER_SIZE 12

  gint expander_size;
  gint line_width;
  double vertical_overshoot;
  int diameter;
  double radius;
  double interp;		/* interpolation factor for center position */
  double x_double_horz, y_double_horz;
  double x_double_vert, y_double_vert;
  double x_double, y_double;
  gint degrees = 0;

  if (widget &&
      gtk_widget_class_find_style_property (GTK_WIDGET_GET_CLASS (widget),
					    "expander-size"))
    {
      gtk_widget_style_get (widget,
			    "expander-size", &expander_size,
			    NULL);
    }
  else
    expander_size = DEFAULT_EXPANDER_SIZE;
    
  line_width = MAX (1, expander_size/9);

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
      degrees = (get_direction (widget) == GTK_TEXT_DIR_RTL) ? 180 : 0;
      interp = 0.0;
      break;
    case GTK_EXPANDER_SEMI_COLLAPSED:
      degrees = (get_direction (widget) == GTK_TEXT_DIR_RTL) ? 150 : 30;
      interp = 0.25;
      break;
    case GTK_EXPANDER_SEMI_EXPANDED:
      degrees = (get_direction (widget) == GTK_TEXT_DIR_RTL) ? 120 : 60;
      interp = 0.75;
      break;
    case GTK_EXPANDER_EXPANDED:
      degrees = 90;
      interp = 1.0;
      break;
    default:
      g_assert_not_reached ();
    }

  /* Compute distance that the stroke extends beyonds the end
   * of the triangle we draw.
   */
  vertical_overshoot = line_width / 2.0 * (1. / tan (G_PI / 8));

  /* For odd line widths, we end the vertical line of the triangle
   * at a half pixel, so we round differently.
   */
  if (line_width % 2 == 1)
    vertical_overshoot = ceil (0.5 + vertical_overshoot) - 0.5;
  else
    vertical_overshoot = ceil (vertical_overshoot);

  /* Adjust the size of the triangle we draw so that the entire stroke fits
   */
  diameter = MAX (3, expander_size - 2 * vertical_overshoot);

  /* If the line width is odd, we want the diameter to be even,
   * and vice versa, so force the sum to be odd. This relationship
   * makes the point of the triangle look right.
   */
  diameter -= (1 - (diameter + line_width) % 2);
  
  radius = diameter / 2.;

  /* Adjust the center so that the stroke is properly aligned with
   * the pixel grid. The center adjustment is different for the
   * horizontal and vertical orientations. For intermediate positions
   * we interpolate between the two.
   */
  x_double_vert = floor (x - (radius + line_width) / 2.) + (radius + line_width) / 2.;
  y_double_vert = y - 0.5;

  x_double_horz = x - 0.5;
  y_double_horz = floor (y - (radius + line_width) / 2.) + (radius + line_width) / 2.;

  x_double = x_double_vert * (1 - interp) + x_double_horz * interp;
  y_double = y_double_vert * (1 - interp) + y_double_horz * interp;
  
  cairo_translate (cr, x_double, y_double);
  cairo_rotate (cr, degrees * G_PI / 180);

  cairo_move_to (cr, - radius / 2., - radius);
  cairo_line_to (cr,   radius / 2.,   0);
  cairo_line_to (cr, - radius / 2.,   radius);
  cairo_close_path (cr);
  
  cairo_set_line_width (cr, line_width);

  if (state_type == GTK_STATE_PRELIGHT)
    gdk_cairo_set_source_color (cr,
				&style->fg[GTK_STATE_PRELIGHT]);
  else if (state_type == GTK_STATE_ACTIVE)
    gdk_cairo_set_source_color (cr,
				&style->light[GTK_STATE_ACTIVE]);
  else
    gdk_cairo_set_source_color (cr,
				&style->base[GTK_STATE_NORMAL]);
  
  cairo_fill_preserve (cr);
  
  gdk_cairo_set_source_color (cr, &style->fg[state_type]);
  cairo_stroke (cr);
}

static void
gtk_default_draw_layout (GtkStyle        *style,
                         cairo_t         *cr,
                         GtkStateType     state_type,
			 gboolean         use_text,
                         GtkWidget       *widget,
                         const gchar     *detail,
                         gint             x,
                         gint             y,
                         PangoLayout     *layout)
{
  GdkColor *gc;
  const PangoMatrix *matrix;

  matrix = pango_context_get_matrix (pango_layout_get_context (layout));
  if (matrix)
    {
      cairo_matrix_t cairo_matrix;
      PangoMatrix tmp_matrix;
      PangoRectangle rect;
      
      cairo_matrix_init (&cairo_matrix,
                         matrix->xx, matrix->yx,
                         matrix->xy, matrix->yy,
                         matrix->x0, matrix->y0);

      pango_layout_get_extents (layout, NULL, &rect);
      pango_matrix_transform_rectangle (matrix, &rect);
      pango_extents_to_pixels (&rect, NULL);
                                          
      tmp_matrix = *matrix;
      cairo_matrix.x0 += x - rect.x;
      cairo_matrix.y0 += y - rect.y;

      cairo_set_matrix (cr, &cairo_matrix);
    }
  else
    cairo_translate (cr, x, y);

  cairo_new_path (cr);

  if (state_type == GTK_STATE_INSENSITIVE)
    {
      gdk_cairo_set_source_color (cr, &style->white);
      cairo_move_to (cr, 1, 1);
      _gtk_pango_fill_layout (cr, layout);
      cairo_new_path (cr);
    }

  gc = use_text ? &style->text[state_type] : &style->fg[state_type];

  gdk_cairo_set_source_color (cr, gc);

  pango_cairo_show_layout (cr, layout);
}

static void
gtk_default_draw_resize_grip (GtkStyle       *style,
                              cairo_t        *cr,
                              GtkStateType    state_type,
                              GtkWidget      *widget,
                              const gchar    *detail,
                              GdkWindowEdge   edge,
                              gint            x,
                              gint            y,
                              gint            width,
                              gint            height)
{
  gint skip;

  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  cairo_set_line_width (cr, 1.0);

  skip = -1;
  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      /* make it square */
      if (width < height)
	height = width;
      else if (height < width)
	width = height;
      skip = 2;
      break;
    case GDK_WINDOW_EDGE_NORTH:
      if (width < height)
	height = width;
      break;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      /* make it square, aligning to top right */
      if (width < height)
	height = width;
      else if (height < width)
	{
	  x += (width - height);
	  width = height;
	}
      skip = 3;
      break;
    case GDK_WINDOW_EDGE_WEST:
      if (height < width)
	width = height;
      break;
    case GDK_WINDOW_EDGE_EAST:
      /* aligning to right */
      if (height < width)
	{
	  x += (width - height);
	  width = height;
	}
      break;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      /* make it square, aligning to bottom left */
      if (width < height)
	{
	  y += (height - width);
	  height = width;
	}
      else if (height < width)
	width = height;
      skip = 1;
      break;
    case GDK_WINDOW_EDGE_SOUTH:
      /* align to bottom */
      if (width < height)
	{
	  y += (height - width);
	  height = width;
	}
      break;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      /* make it square, aligning to bottom right */
      if (width < height)
	{
	  y += (height - width);
	  height = width;
	}
      else if (height < width)
	{
	  x += (width - height);
	  width = height;
	}
      skip = 0;
      break;
    default:
      g_assert_not_reached ();
    }
  
  switch (edge)
    {
    case GDK_WINDOW_EDGE_WEST:
    case GDK_WINDOW_EDGE_EAST:
      {
	gint xi;

	xi = x;

	while (xi < x + width)
	  {
	    _cairo_draw_line (cr,
			      &style->light[state_type],
			      xi, y,
			      xi, y + height);

	    xi++;
	    _cairo_draw_line (cr,
			      &style->dark[state_type],
			      xi, y,
			      xi, y + height);

	    xi += 2;
	  }
      }
      break;
    case GDK_WINDOW_EDGE_NORTH:
    case GDK_WINDOW_EDGE_SOUTH:
      {
	gint yi;

	yi = y;

	while (yi < y + height)
	  {
	    _cairo_draw_line (cr,
			      &style->light[state_type],
			      x, yi,
			      x + width, yi);

	    yi++;
	    _cairo_draw_line (cr,
			      &style->dark[state_type],
			      x, yi,
			      x + width, yi);

	    yi+= 2;
	  }
      }
      break;
    case GDK_WINDOW_EDGE_NORTH_WEST:
      {
	gint xi, yi;

	xi = x + width;
	yi = y + height;

	while (xi > x + 3)
	  {
	    _cairo_draw_line (cr,
			      &style->dark[state_type],
			      xi, y,
			      x, yi);

	    --xi;
	    --yi;

	    _cairo_draw_line (cr,
			      &style->dark[state_type],
			      xi, y,
			      x, yi);

	    --xi;
	    --yi;

	    _cairo_draw_line (cr,
			      &style->light[state_type],
			      xi, y,
			      x, yi);

	    xi -= 3;
	    yi -= 3;
	    
	  }
      }
      break;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      {
        gint xi, yi;

        xi = x;
        yi = y + height;

        while (xi < (x + width - 3))
          {
            _cairo_draw_line (cr,
                              &style->light[state_type],
                              xi, y,
                              x + width, yi);                           

            ++xi;
            --yi;
            
            _cairo_draw_line (cr,
                              &style->dark[state_type],
                              xi, y,
                              x + width, yi);                           

            ++xi;
            --yi;
            
            _cairo_draw_line (cr,
                              &style->dark[state_type],
                              xi, y,
                              x + width, yi);

            xi += 3;
            yi -= 3;
          }
      }
      break;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      {
	gint xi, yi;

	xi = x + width;
	yi = y;

	while (xi > x + 3)
	  {
	    _cairo_draw_line (cr,
			      &style->dark[state_type],
			      x, yi,
			      xi, y + height);

	    --xi;
	    ++yi;

	    _cairo_draw_line (cr,
			      &style->dark[state_type],
			      x, yi,
			      xi, y + height);

	    --xi;
	    ++yi;

	    _cairo_draw_line (cr,
			      &style->light[state_type],
			      x, yi,
			      xi, y + height);

	    xi -= 3;
	    yi += 3;
	    
	  }
      }
      break;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      {
        gint xi, yi;

        xi = x;
        yi = y;

        while (xi < (x + width - 3))
          {
            _cairo_draw_line (cr,
                              &style->light[state_type],
                              xi, y + height,
                              x + width, yi);                           

            ++xi;
            ++yi;
            
            _cairo_draw_line (cr,
                              &style->dark[state_type],
                              xi, y + height,
                              x + width, yi);                           

            ++xi;
            ++yi;
            
            _cairo_draw_line (cr,
                              &style->dark[state_type],
                              xi, y + height,
                              x + width, yi);

            xi += 3;
            yi += 3;
          }
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_default_draw_spinner (GtkStyle     *style,
                          cairo_t      *cr,
                          GtkStateType  state_type,
                          GtkWidget    *widget,
                          const gchar  *detail,
                          guint         step,
                          gint          x,
                          gint          y,
                          gint          width,
                          gint          height)
{
  GdkColor *color;
  guint num_steps;
  gdouble dx, dy;
  gdouble radius;
  gdouble half;
  gint i;
  guint real_step;

  gtk_style_get (style, GTK_TYPE_SPINNER,
                 "num-steps", &num_steps,
                 NULL);
  real_step = step % num_steps;

  /* set a clip region for the expose event */
  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  cairo_translate (cr, x, y);

  /* draw clip region */
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  color = &style->fg[state_type];
  dx = width / 2;
  dy = height / 2;
  radius = MIN (width / 2, height / 2);
  half = num_steps / 2;

  for (i = 0; i < num_steps; i++)
    {
      gint inset = 0.7 * radius;

      /* transparency is a function of time and intial value */
      gdouble t = (gdouble) ((i + num_steps - real_step)
                             % num_steps) / num_steps;

      cairo_save (cr);

      cairo_set_source_rgba (cr,
                             color->red / 65535.,
                             color->green / 65535.,
                             color->blue / 65535.,
                             t);

      cairo_set_line_width (cr, 2.0);
      cairo_move_to (cr,
                     dx + (radius - inset) * cos (i * G_PI / half),
                     dy + (radius - inset) * sin (i * G_PI / half));
      cairo_line_to (cr,
                     dx + radius * cos (i * G_PI / half),
                     dy + radius * sin (i * G_PI / half));
      cairo_stroke (cr);

      cairo_restore (cr);
    }
}

void
_gtk_style_shade (const GdkColor *a,
                  GdkColor       *b,
                  gdouble         k)
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


/**
 * gtk_paint_hline:
 * @style: a #GtkStyle
 * @cr: a #caio_t
 * @state_type: a state
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x1: the starting x coordinate
 * @x2: the ending x coordinate
 * @y: the y coordinate
 *
 * Draws a horizontal line from (@x1, @y) to (@x2, @y) in @cr
 * using the given style and state.
 **/ 
void 
gtk_paint_hline (GtkStyle           *style,
                 cairo_t            *cr,
                 GtkStateType        state_type,
                 GtkWidget          *widget,
                 const gchar        *detail,
                 gint                x1,
                 gint                x2,
                 gint                y)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  cairo_save (cr);
  gtk_render_line (context, cr, x1, y, x2, y);
  cairo_restore (cr);
}

/**
 * gtk_paint_vline:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @y1_: the starting y coordinate
 * @y2_: the ending y coordinate
 * @x: the x coordinate
 *
 * Draws a vertical line from (@x, @y1_) to (@x, @y2_) in @cr
 * using the given style and state.
 */
void
gtk_paint_vline (GtkStyle           *style,
                 cairo_t            *cr,
                 GtkStateType        state_type,
                 GtkWidget          *widget,
                 const gchar        *detail,
                 gint                y1_,
                 gint                y2_,
                 gint                x)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  cairo_save (cr);
  gtk_render_line (context, cr, x, y1_, x, y2_);
  cairo_restore (cr);
}

/**
 * gtk_paint_shadow:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle
 * @y: y origin of the rectangle
 * @width: width of the rectangle
 * @height: width of the rectangle
 *
 * Draws a shadow around the given rectangle in @cr 
 * using the given style and state and shadow type.
 */
void
gtk_paint_shadow (GtkStyle           *style,
                  cairo_t            *cr,
                  GtkStateType        state_type,
                  GtkShadowType       shadow_type,
                  GtkWidget          *widget,
                  const gchar        *detail,
                  gint                x,
                  gint                y,
                  gint                width,
                  gint                height)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  if (width < 0 || height < 0)
    {
      gint w_width, w_height;

      gdk_drawable_get_size (GDK_DRAWABLE (window), &w_width, &w_height);

      if (width < 0)
        width = w_width;

      if (height < 0)
        height = w_height;
    }

  cairo_save (cr);
  gtk_render_frame (context, cr,
                    (gdouble) x,
                    (gdouble) y,
                    (gdouble) width,
                    (gdouble) height);
  cairo_restore (cr);
}

/**
 * gtk_paint_arrow:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @arrow_type: the type of arrow to draw
 * @fill: %TRUE if the arrow tip should be filled
 * @x: x origin of the rectangle to draw the arrow in
 * @y: y origin of the rectangle to draw the arrow in
 * @width: width of the rectangle to draw the arrow in
 * @height: height of the rectangle to draw the arrow in
 * 
 * Draws an arrow in the given rectangle on @cr using the given 
 * parameters. @arrow_type determines the direction of the arrow.
 */
void
gtk_paint_arrow (GtkStyle           *style,
                 cairo_t            *cr,
                 GtkStateType        state_type,
                 GtkShadowType       shadow_type,
                 GtkWidget          *widget,
                 const gchar        *detail,
                 GtkArrowType        arrow_type,
                 gboolean            fill,
                 gint                x,
                 gint                y,
                 gint                width,
                 gint                height)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;
  gdouble angle;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  if (arrow_type == GTK_ARROW_UP)
    angle = 0;
  else if (arrow_type == GTK_ARROW_RIGHT)
    angle = G_PI / 2;
  else if (arrow_type == GTK_ARROW_DOWN)
    angle = G_PI;
  else
    angle = 3 * (G_PI / 2);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_ACTIVE:
      flags |= GTK_STATE_FLAG_ACTIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_arrow (context,
                    cr, angle,
                    (gdouble) x,
                    (gdouble) y,
                    MIN ((gdouble) width, (gdouble) height));

  cairo_restore (cr);
}

/**
 * gtk_paint_diamond:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle to draw the diamond in
 * @y: y origin of the rectangle to draw the diamond in
 * @width: width of the rectangle to draw the diamond in
 * @height: height of the rectangle to draw the diamond in
 *
 * Draws a diamond in the given rectangle on @window using the given
 * parameters.
 */
void
gtk_paint_diamond (GtkStyle           *style,
                   cairo_t            *cr,
                   GtkStateType        state_type,
                   GtkShadowType       shadow_type,
                   GtkWidget          *widget,
                   const gchar        *detail,
                   gint                x,
                   gint                y,
                   gint                width,
                   gint                height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_diamond != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_diamond (style, cr, state_type, shadow_type,
                                             widget, detail,
                                             x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_paint_box:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the box
 * @y: y origin of the box
 * @width: the width of the box
 * @height: the height of the box
 * 
 * Draws a box on @cr with the given parameters.
 */
void
gtk_paint_box (GtkStyle           *style,
               cairo_t            *cr,
               GtkStateType        state_type,
               GtkShadowType       shadow_type,
               GtkWidget          *widget,
               const gchar        *detail,
               gint                x,
               gint                y,
               gint                width,
               gint                height)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_ACTIVE:
      flags |= GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);
  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);
  cairo_restore (cr);
}

/**
 * gtk_paint_flat_box:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @area: (allow-none): clip rectangle, or %NULL if the
 *        output should not be clipped
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the box
 * @y: y origin of the box
 * @width: the width of the box
 * @height: the height of the box
 * 
 * Draws a flat box on @cr with the given parameters.
 */
void
gtk_paint_flat_box (GtkStyle           *style,
                    cairo_t            *cr,
                    GtkStateType        state_type,
                    GtkShadowType       shadow_type,
                    GtkWidget          *widget,
                    const gchar        *detail,
                    gint                x,
                    gint                y,
                    gint                width,
                    gint                height)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_background (context, cr,
                         (gdouble) x,
                         (gdouble) y,
                         (gdouble) width,
                         (gdouble) height);

  cairo_restore (cr);
}

/**
 * gtk_paint_check:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle to draw the check in
 * @y: y origin of the rectangle to draw the check in
 * @width: the width of the rectangle to draw the check in
 * @height: the height of the rectangle to draw the check in
 * 
 * Draws a check button indicator in the given rectangle on @cr with 
 * the given parameters.
 */
void
gtk_paint_check (GtkStyle           *style,
                 cairo_t            *cr,
                 GtkStateType        state_type,
                 GtkShadowType       shadow_type,
                 GtkWidget          *widget,
                 const gchar        *detail,
                 gint                x,
                 gint                y,
                 gint                width,
                 gint                height)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  if (shadow_type == GTK_SHADOW_IN)
    flags |= GTK_STATE_FLAG_ACTIVE;
  else if (shadow_type == GTK_SHADOW_ETCHED_IN)
    flags |= GTK_STATE_FLAG_INCONSISTENT;

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_check (context,
                    cr, x, y,
                    width, height);

  cairo_restore (cr);
}

/**
 * gtk_paint_option:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle to draw the option in
 * @y: y origin of the rectangle to draw the option in
 * @width: the width of the rectangle to draw the option in
 * @height: the height of the rectangle to draw the option in
 *
 * Draws a radio button indicator in the given rectangle on @cr with 
 * the given parameters.
 */
void
gtk_paint_option (GtkStyle           *style,
                  cairo_t            *cr,
                  GtkStateType        state_type,
                  GtkShadowType       shadow_type,
                  GtkWidget          *widget,
                  const gchar        *detail,
                  gint                x,
                  gint                y,
                  gint                width,
                  gint                height)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  if (shadow_type == GTK_SHADOW_IN)
    flags |= GTK_STATE_FLAG_ACTIVE;
  else if (shadow_type == GTK_SHADOW_ETCHED_IN)
    flags |= GTK_STATE_FLAG_INCONSISTENT;

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_option (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     (gdouble) width,
                     (gdouble) height);

  cairo_restore (cr);
}

/**
 * gtk_paint_tab:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: the type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle to draw the tab in
 * @y: y origin of the rectangle to draw the tab in
 * @width: the width of the rectangle to draw the tab in
 * @height: the height of the rectangle to draw the tab in
 *
 * Draws an option menu tab (i.e. the up and down pointing arrows)
 * in the given rectangle on @cr using the given parameters.
 */ 
void
gtk_paint_tab (GtkStyle           *style,
               cairo_t            *cr,
               GtkStateType        state_type,
               GtkShadowType       shadow_type,
               GtkWidget          *widget,
               const gchar        *detail,
               gint                x,
               gint                y,
               gint                width,
               gint                height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_tab != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_tab (style, cr, state_type, shadow_type,
                                         widget, detail,
                                         x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_paint_shadow_gap:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle
 * @y: y origin of the rectangle
 * @width: width of the rectangle
 * @height: width of the rectangle
 * @gap_side: side in which to leave the gap
 * @gap_x: starting position of the gap
 * @gap_width: width of the gap
 *
 * Draws a shadow around the given rectangle in @cr
 * using the given style and state and shadow type, leaving a 
 * gap in one side.
*/
void
gtk_paint_shadow_gap (GtkStyle           *style,
                      cairo_t            *cr,
                      GtkStateType        state_type,
                      GtkShadowType       shadow_type,
                      GtkWidget          *widget,
                      const gchar        *detail,
                      gint                x,
                      gint                y,
                      gint                width,
                      gint                height,
                      GtkPositionType     gap_side,
                      gint                gap_x,
                      gint                gap_width)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_ACTIVE:
      flags |= GTK_STATE_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_frame_gap (context, cr,
                        (gdouble) x,
                        (gdouble) y,
                        (gdouble) width,
                        (gdouble) height,
                        gap_side,
                        (gdouble) gap_x,
                        (gdouble) gap_x + gap_width);

  cairo_restore (cr);
}

/**
 * gtk_paint_box_gap:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the rectangle
 * @y: y origin of the rectangle
 * @width: width of the rectangle
 * @height: width of the rectangle
 * @gap_side: side in which to leave the gap
 * @gap_x: starting position of the gap
 * @gap_width: width of the gap
 *
 * Draws a box in @cr using the given style and state and shadow type, 
 * leaving a gap in one side.
 */
void
gtk_paint_box_gap (GtkStyle           *style,
                   cairo_t            *cr,
                   GtkStateType        state_type,
                   GtkShadowType       shadow_type,
                   GtkWidget          *widget,
                   const gchar        *detail,
                   gint                x,
                   gint                y,
                   gint                width,
                   gint                height,
                   GtkPositionType     gap_side,
                   gint                gap_x,
                   gint                gap_width)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_ACTIVE:
      flags |= GTK_STATE_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_background (context, cr,
                         (gdouble) x,
                         (gdouble) y,
                         (gdouble) width,
                         (gdouble) height);

  gtk_render_frame_gap (context, cr,
                        (gdouble) x,
                        (gdouble) y,
                        (gdouble) width,
                        (gdouble) height,
                        gap_side,
                        (gdouble) gap_x,
                        (gdouble) gap_x + gap_width);

  cairo_restore (cr);
}

/**
 * gtk_paint_extension: 
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the extension
 * @y: y origin of the extension
 * @width: width of the extension
 * @height: width of the extension
 * @gap_side: the side on to which the extension is attached
 * 
 * Draws an extension, i.e. a notebook tab.
 **/
void
gtk_paint_extension (GtkStyle           *style,
                     cairo_t            *cr,
                     GtkStateType        state_type,
                     GtkShadowType       shadow_type,
                     GtkWidget          *widget,
                     const gchar        *detail,
                     gint                x,
                     gint                y,
                     gint                width,
                     gint                height,
                     GtkPositionType     gap_side)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_ACTIVE:
      flags |= GTK_STATE_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_extension (context, cr,
                        (gdouble) x,
                        (gdouble) y,
                        (gdouble) width,
                        (gdouble) height,
                        gap_side);

  cairo_restore (cr);
}

/**
 * gtk_paint_focus:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: the x origin of the rectangle around which to draw a focus indicator
 * @y: the y origin of the rectangle around which to draw a focus indicator
 * @width: the width of the rectangle around which to draw a focus indicator
 * @height: the height of the rectangle around which to draw a focus indicator
 *
 * Draws a focus indicator around the given rectangle on @cr using the
 * given style.
 */
void
gtk_paint_focus (GtkStyle           *style,
                 cairo_t            *cr,
                 GtkStateType        state_type,
                 GtkWidget          *widget,
                 const gchar        *detail,
                 gint                x,
                 gint                y,
                 gint                width,
                 gint                height)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  context = gtk_widget_get_style_context (widget);

  gtk_render_focus (context, cr,
                    (gdouble) x,
                    (gdouble) y,
                    (gdouble) width,
                    (gdouble) height);

  cairo_restore (cr);
}

/**
 * gtk_paint_slider:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: a shadow
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: the x origin of the rectangle in which to draw a slider
 * @y: the y origin of the rectangle in which to draw a slider
 * @width: the width of the rectangle in which to draw a slider
 * @height: the height of the rectangle in which to draw a slider
 * @orientation: the orientation to be used
 *
 * Draws a slider in the given rectangle on @cr using the
 * given style and orientation.
 **/
void
gtk_paint_slider (GtkStyle           *style,
                  cairo_t            *cr,
                  GtkStateType        state_type,
                  GtkShadowType       shadow_type,
                  GtkWidget          *widget,
                  const gchar        *detail,
                  gint                x,
                  gint                y,
                  gint                width,
                  gint                height,
                  GtkOrientation      orientation)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_ACTIVE:
      flags |= GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_slider (context, cr,  x, y, width, height, orientation);

  cairo_restore (cr);
}

/**
 * gtk_paint_handle:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @shadow_type: type of shadow to draw
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin of the handle
 * @y: y origin of the handle
 * @width: with of the handle
 * @height: height of the handle
 * @orientation: the orientation of the handle
 * 
 * Draws a handle as used in #GtkHandleBox and #GtkPaned.
 **/
void
gtk_paint_handle (GtkStyle           *style,
                  cairo_t            *cr,
                  GtkStateType        state_type,
                  GtkShadowType       shadow_type,
                  GtkWidget          *widget,
                  const gchar        *detail,
                  gint                x,
                  gint                y,
                  gint                width,
                  gint                height,
                  GtkOrientation      orientation)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_handle (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     (gdouble) width,
                     (gdouble) height,
                     orientation);

  cairo_restore (cr);
}

/**
 * gtk_paint_expander:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: the x position to draw the expander at
 * @y: the y position to draw the expander at
 * @expander_style: the style to draw the expander in; determines
 *   whether the expander is collapsed, expanded, or in an
 *   intermediate state.
 * 
 * Draws an expander as used in #GtkTreeView. @x and @y specify the
 * center the expander. The size of the expander is determined by the
 * "expander-size" style property of @widget.  (If widget is not
 * specified or doesn't have an "expander-size" property, an
 * unspecified default size will be used, since the caller doesn't
 * have sufficient information to position the expander, this is
 * likely not useful.) The expander is expander_size pixels tall
 * in the collapsed position and expander_size pixels wide in the
 * expanded position.
 **/
void
gtk_paint_expander (GtkStyle           *style,
                    cairo_t            *cr,
                    GtkStateType        state_type,
                    GtkWidget          *widget,
                    const gchar        *detail,
                    gint                x,
                    gint                y,
                    GtkExpanderStyle    expander_style)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;
  gint size;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  if (widget)
    gtk_widget_style_get (widget, "expander-size", &size, NULL);
  else
    size = 10;

  if (expander_style == GTK_EXPANDER_EXPANDED)
    flags |= GTK_STATE_FLAG_ACTIVE;

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_expander (context, cr,
                       (gdouble) x - (size / 2),
                       (gdouble) y - (size / 2),
                       (gdouble) size,
                       (gdouble) size);

  cairo_restore (cr);
}

/**
 * gtk_paint_layout:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @use_text: whether to use the text or foreground
 *            graphics context of @style
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @x: x origin
 * @y: y origin
 * @layout: the layout to draw
 *
 * Draws a layout on @cr using the given parameters.
 **/
void
gtk_paint_layout (GtkStyle           *style,
                  cairo_t            *cr,
                  GtkStateType        state_type,
                  gboolean            use_text,
                  GtkWidget          *widget,
                  const gchar        *detail,
                  gint                x,
                  gint                y,
                  PangoLayout        *layout)
{
  GtkStyleContext *context;
  GtkStateFlags flags = 0;

  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);

  context = gtk_widget_get_style_context (widget);

  switch (state_type)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags |= GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  cairo_save (cr);

  gtk_style_context_set_state (context, flags);
  gtk_render_layout (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     layout);

  cairo_restore (cr);
}

/**
 * gtk_paint_resize_grip:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 * @edge: the edge in which to draw the resize grip
 * @x: the x origin of the rectangle in which to draw the resize grip
 * @y: the y origin of the rectangle in which to draw the resize grip
 * @width: the width of the rectangle in which to draw the resize grip
 * @height: the height of the rectangle in which to draw the resize grip
 *
 * Draws a resize grip in the given rectangle on @cr using the given
 * parameters. 
 */
void
gtk_paint_resize_grip (GtkStyle           *style,
                       cairo_t            *cr,
                       GtkStateType        state_type,
                       GtkWidget          *widget,
                       const gchar        *detail,
                       GdkWindowEdge       edge,
                       gint                x,
                       gint                y,
                       gint                width,
                       gint                height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_resize_grip != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_resize_grip (style, cr, state_type,
                                                 widget, detail,
                                                 edge, x, y, width, height);
  cairo_restore (cr);
}

/**
 * gtk_paint_spinner:
 * @style: a #GtkStyle
 * @cr: a #cairo_t
 * @state_type: a state
 * @widget: (allow-none): the widget (may be %NULL)
 * @detail: (allow-none): a style detail (may be %NULL)
 * @step: the nth step, a value between 0 and #GtkSpinner:num-steps
 * @x: the x origin of the rectangle in which to draw the spinner
 * @y: the y origin of the rectangle in which to draw the spinner
 * @width: the width of the rectangle in which to draw the spinner
 * @height: the height of the rectangle in which to draw the spinner
 *
 * Draws a spinner on @window using the given parameters.
 */
void
gtk_paint_spinner (GtkStyle           *style,
                   cairo_t            *cr,
                   GtkStateType        state_type,
                   GtkWidget          *widget,
                   const gchar        *detail,
                   guint               step,
                   gint                x,
                   gint                y,
                   gint                width,
                   gint                height)
{
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_spinner != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_spinner (style, cr, state_type,
                                             widget, detail,
					     step, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_border_new:
 *
 * Allocates a new #GtkBorder structure and initializes its elements to zero.
 * 
 * Returns: a new empty #GtkBorder. The newly allocated #GtkBorder should be 
 *     freed with gtk_border_free()
 *
 * Since: 2.14
 **/
GtkBorder *
gtk_border_new (void)
{
  return g_slice_new0 (GtkBorder);
}

/**
 * gtk_border_copy:
 * @border_: a #GtkBorder.
 * @returns: a copy of @border_.
 *
 * Copies a #GtkBorder structure.
 **/
GtkBorder *
gtk_border_copy (const GtkBorder *border)
{
  g_return_val_if_fail (border != NULL, NULL);

  return g_slice_dup (GtkBorder, border);
}

/**
 * gtk_border_free:
 * @border_: a #GtkBorder.
 * 
 * Frees a #GtkBorder structure.
 **/
void
gtk_border_free (GtkBorder *border)
{
  g_slice_free (GtkBorder, border);
}

G_DEFINE_BOXED_TYPE (GtkBorder, gtk_border,
                     gtk_border_copy,
                     gtk_border_free)

typedef struct _CursorInfo CursorInfo;

struct _CursorInfo
{
  GType for_type;
  GdkColor primary;
  GdkColor secondary;
};

static void
style_unrealize_cursors (GtkStyle *style)
{
  CursorInfo *
  
  cursor_info = g_object_get_data (G_OBJECT (style), "gtk-style-cursor-info");
  if (cursor_info)
    {
      g_free (cursor_info);
      g_object_set_data (G_OBJECT (style), I_("gtk-style-cursor-info"), NULL);
    }
}

static const GdkColor *
get_insertion_cursor_color (GtkWidget *widget,
			    gboolean   is_primary)
{
  CursorInfo *cursor_info;
  GtkStyle *style;
  GdkColor *cursor_color;

  style = gtk_widget_get_style (widget);

  cursor_info = g_object_get_data (G_OBJECT (style), "gtk-style-cursor-info");
  if (!cursor_info)
    {
      cursor_info = g_new0 (CursorInfo, 1);
      g_object_set_data (G_OBJECT (style), I_("gtk-style-cursor-info"), cursor_info);
      cursor_info->for_type = G_TYPE_INVALID;
    }

  /* We have to keep track of the type because gtk_widget_style_get()
   * can return different results when called on the same property and
   * same style but for different widgets. :-(. That is,
   * GtkEntry::cursor-color = "red" in a style will modify the cursor
   * color for entries but not for text view.
   */
  if (cursor_info->for_type != G_OBJECT_TYPE (widget))
    {
      cursor_info->for_type = G_OBJECT_TYPE (widget);

      /* Cursors in text widgets are drawn only in NORMAL state,
       * so we can use text[GTK_STATE_NORMAL] as text color here */
      gtk_widget_style_get (widget, "cursor-color", &cursor_color, NULL);
      if (cursor_color)
        {
          cursor_info->primary = *cursor_color;
          gdk_color_free (cursor_color);
        }
      else
        {
          cursor_info->primary = style->text[GTK_STATE_NORMAL];
        }

      gtk_widget_style_get (widget, "secondary-cursor-color", &cursor_color, NULL);
      if (cursor_color)
        {
          cursor_info->secondary = *cursor_color;
          gdk_color_free (cursor_color);
        }
      else
        {
          /* text_aa is the average of text and base colors,
           * in usual black-on-white case it's grey. */
          cursor_info->secondary = style->text_aa[GTK_STATE_NORMAL];
        }
    }

  if (is_primary)
    return &cursor_info->primary;
  else
    return &cursor_info->secondary;
}

void
_gtk_widget_get_cursor_color (GtkWidget *widget,
			      GdkColor  *color)
{
  GdkColor *style_color;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (color != NULL);

  gtk_widget_style_get (widget, "cursor-color", &style_color, NULL);

  if (style_color)
    {
      *color = *style_color;
      gdk_color_free (style_color);
    }
  else
    *color = gtk_widget_get_style (widget)->text[GTK_STATE_NORMAL];
}

/**
 * gtk_draw_insertion_cursor:
 * @widget:  a #GtkWidget
 * @cr: cairo context to draw to
 * @location: location where to draw the cursor (@location->width is ignored)
 * @is_primary: if the cursor should be the primary cursor color.
 * @direction: whether the cursor is left-to-right or
 *             right-to-left. Should never be #GTK_TEXT_DIR_NONE
 * @draw_arrow: %TRUE to draw a directional arrow on the
 *        cursor. Should be %FALSE unless the cursor is split.
 * 
 * Draws a text caret on @cr at @location. This is not a style function
 * but merely a convenience function for drawing the standard cursor shape.
 *
 * Since: 3.0
 **/
void
gtk_draw_insertion_cursor (GtkWidget          *widget,
                           cairo_t            *cr,
                           const GdkRectangle *location,
                           gboolean            is_primary,
                           GtkTextDirection    direction,
                           gboolean            draw_arrow)
{
  gint stem_width;
  gint arrow_width;
  gint x, y;
  gfloat cursor_aspect_ratio;
  gint offset;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (location != NULL);
  g_return_if_fail (direction != GTK_TEXT_DIR_NONE);

  gdk_cairo_set_source_color (cr, get_insertion_cursor_color (widget, is_primary));

  /* When changing the shape or size of the cursor here,
   * propagate the changes to gtktextview.c:text_window_invalidate_cursors().
   */

  gtk_widget_style_get (widget, "cursor-aspect-ratio", &cursor_aspect_ratio, NULL);
  
  stem_width = location->height * cursor_aspect_ratio + 1;
  arrow_width = stem_width + 1;

  /* put (stem_width % 2) on the proper side of the cursor */
  if (direction == GTK_TEXT_DIR_LTR)
    offset = stem_width / 2;
  else
    offset = stem_width - stem_width / 2;
  
  cairo_rectangle (cr, 
                   location->x - offset, location->y,
                   stem_width, location->height);
  cairo_fill (cr);

  if (draw_arrow)
    {
      if (direction == GTK_TEXT_DIR_RTL)
        {
          x = location->x - offset - 1;
          y = location->y + location->height - arrow_width * 2 - arrow_width + 1;
  
          cairo_move_to (cr, x, y + 1);
          cairo_line_to (cr, x - arrow_width, y + arrow_width);
          cairo_line_to (cr, x, y + 2 * arrow_width);
          cairo_fill (cr);
        }
      else if (direction == GTK_TEXT_DIR_LTR)
        {
          x = location->x + stem_width - offset;
          y = location->y + location->height - arrow_width * 2 - arrow_width + 1;
  
          cairo_move_to (cr, x, y + 1);
          cairo_line_to (cr, x + arrow_width, y + arrow_width);
          cairo_line_to (cr, x, y + 2 * arrow_width);
          cairo_fill (cr);
        }
    }
}
