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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <gobject/gvaluecollector.h>
#include "gtkmarshalers.h"
#include "gtkpango.h"
#include "gtkrc.h"
#include "gtkspinbutton.h"
#include "gtkstyle.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include "gtkdebug.h"
#include "gtkrender.h"
#include "gtkborder.h"
#include "gtkwidgetpath.h"

/**
 * SECTION:gtkstyle
 * @Short_description: Deprecated object that holds style information
 *     for widgets
 * @Title: GtkStyle
 *
 * A #GtkStyle object encapsulates the information that provides the look and
 * feel for a widget.
 *
 * > In GTK+ 3.0, GtkStyle has been deprecated and replaced by
 * > #GtkStyleContext.
 *
 * Each #GtkWidget has an associated #GtkStyle object that is used when
 * rendering that widget. Also, a #GtkStyle holds information for the five
 * possible widget states though not every widget supports all five
 * states; see #GtkStateType.
 *
 * Usually the #GtkStyle for a widget is the same as the default style that
 * is set by GTK+ and modified the theme engine.
 *
 * Usually applications should not need to use or modify the #GtkStyle of
 * their widgets.
 */


#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7

/* --- typedefs & structures --- */
typedef struct {
  GType       widget_type;
  GParamSpec *pspec;
  GValue      value;
} PropertyValue;

typedef struct {
  GtkStyleContext *context;
  gulong context_changed_id;
} GtkStylePrivate;

#define GTK_STYLE_GET_PRIVATE(obj) ((GtkStylePrivate *) gtk_style_get_instance_private ((GtkStyle *) (obj)))

enum {
  PROP_0,
  PROP_CONTEXT
};

/* --- prototypes --- */
static void	 gtk_style_finalize		(GObject	*object);
static void	 gtk_style_constructed		(GObject	*object);
static void      gtk_style_set_property         (GObject        *object,
                                                 guint           prop_id,
                                                 const GValue   *value,
                                                 GParamSpec     *pspec);
static void      gtk_style_get_property         (GObject        *object,
                                                 guint           prop_id,
                                                 GValue         *value,
                                                 GParamSpec     *pspec);

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

static void transform_detail_string (const gchar     *detail,
                                     GtkStyleContext *context);

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

G_DEFINE_TYPE_WITH_PRIVATE (GtkStyle, gtk_style, G_TYPE_OBJECT)

/* --- functions --- */

static void
gtk_style_init (GtkStyle *style)
{
  gint i;

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
  object_class->set_property = gtk_style_set_property;
  object_class->get_property = gtk_style_get_property;
  object_class->constructed = gtk_style_constructed;

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

  g_object_class_install_property (object_class,
				   PROP_CONTEXT,
				   g_param_spec_object ("context",
 							P_("Style context"),
							P_("GtkStyleContext to get style from"),
                                                        GTK_TYPE_STYLE_CONTEXT,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

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
gtk_style_finalize (GObject *object)
{
  GtkStyle *style = GTK_STYLE (object);
  GtkStylePrivate *priv = GTK_STYLE_GET_PRIVATE (style);
  gint i;

  g_return_if_fail (style->attach_count == 0);

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

  pango_font_description_free (style->font_desc);

  if (style->private_font_desc)
    pango_font_description_free (style->private_font_desc);

  if (style->rc_style)
    g_object_unref (style->rc_style);

  if (priv->context)
    {
      if (priv->context_changed_id)
        g_signal_handler_disconnect (priv->context, priv->context_changed_id);

      g_object_unref (priv->context);
    }

  for (i = 0; i < 5; i++)
    {
      if (style->background[i])
        cairo_pattern_destroy (style->background[i]);
    }

  G_OBJECT_CLASS (gtk_style_parent_class)->finalize (object);
}

static void
gtk_style_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkStylePrivate *priv;

  priv = GTK_STYLE_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      priv->context = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_get_property (GObject      *object,
                        guint         prop_id,
                        GValue       *value,
                        GParamSpec   *pspec)
{
  GtkStylePrivate *priv;

  priv = GTK_STYLE_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, priv->context);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
set_color_from_context (GtkStyle *style,
                        GtkStateType state,
                        GtkStyleContext *context,
                        GtkStateFlags flags,
                        GtkRcFlags prop)
{
  GdkRGBA *color = NULL;
  GdkColor *dest = { 0 }; /* Shut up gcc */

  switch (prop)
    {
    case GTK_RC_BG:
      gtk_style_context_get (context, flags,
                             "background-color", &color,
                             NULL);
      dest = &style->bg[state];
      break;
    case GTK_RC_FG:
      gtk_style_context_get (context, flags,
                             "color", &color,
                             NULL);
      dest = &style->fg[state];
      break;
    case GTK_RC_TEXT:
      gtk_style_context_get (context, flags,
                             "color", &color,
                             NULL);
      dest = &style->text[state];
      break;
    case GTK_RC_BASE:
      gtk_style_context_get (context, flags,
                             "background-color", &color,
                             NULL);
      dest = &style->base[state];
      break;
    }

  if (!color)
    return FALSE;

  if (!(color->alpha > 0.01))
    {
      gdk_rgba_free (color);
      return FALSE;
    }

  dest->pixel = 0;
  dest->red = CLAMP ((guint) (color->red * 65535), 0, 65535);
  dest->green = CLAMP ((guint) (color->green * 65535), 0, 65535);
  dest->blue = CLAMP ((guint) (color->blue * 65535), 0, 65535);
  gdk_rgba_free (color);

  return TRUE;
}

static void
set_color (GtkStyle        *style,
           GtkStyleContext *context,
           GtkStateType     state,
           GtkRcFlags       prop)
{
  GtkStateFlags flags;

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      flags = 0;
    }

  /* Try to fill in the values from the associated GtkStyleContext.
   * Since fully-transparent black is a very common default (e.g. for 
   * background-color properties), and we must store the result in a GdkColor
   * to retain API compatibility, in case the fetched color is fully transparent
   * we give themes a fallback style class they can style, before using the
   * hardcoded default values.
   */
  if (!set_color_from_context (style, state, context, flags, prop))
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "gtkstyle-fallback");
      set_color_from_context (style, state, context, flags, prop);
      gtk_style_context_restore (context);
    }
}

static void
gtk_style_update_from_context (GtkStyle *style)
{
  GtkStylePrivate *priv;
  GtkStateType state;
  GtkBorder padding;
  gint i;

  priv = GTK_STYLE_GET_PRIVATE (style);

  for (state = GTK_STATE_NORMAL; state <= GTK_STATE_INSENSITIVE; state++)
    {
      if (gtk_style_context_has_class (priv->context, "entry"))
        {
          gtk_style_context_save (priv->context);
          gtk_style_context_remove_class (priv->context, "entry");
          set_color (style, priv->context, state, GTK_RC_BG);
          set_color (style, priv->context, state, GTK_RC_FG);
          gtk_style_context_restore (priv->context);

          set_color (style, priv->context, state, GTK_RC_BASE);
          set_color (style, priv->context, state, GTK_RC_TEXT);
        }
      else
        {
          gtk_style_context_save (priv->context);
          gtk_style_context_add_class (priv->context, "entry");
          set_color (style, priv->context, state, GTK_RC_BASE);
          set_color (style, priv->context, state, GTK_RC_TEXT);
          gtk_style_context_restore (priv->context);

          set_color (style, priv->context, state, GTK_RC_BG);
          set_color (style, priv->context, state, GTK_RC_FG);
        }
    }

  if (style->font_desc)
    pango_font_description_free (style->font_desc);

  gtk_style_context_get (priv->context, 0,
                         "font", &style->font_desc,
                         NULL);
  gtk_style_context_get_padding (priv->context, 0, &padding);

  style->xthickness = padding.left;
  style->ythickness = padding.top;

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
      if (style->background[i])
        cairo_pattern_destroy (style->background[i]);

      style->background[i] = cairo_pattern_create_rgb (style->bg[i].red / 65535.0,
                                                       style->bg[i].green / 65535.0,
                                                       style->bg[i].blue / 65535.0);
    }
}

static void
style_context_changed (GtkStyleContext *context,
                       gpointer         user_data)
{
  gtk_style_update_from_context (GTK_STYLE (user_data));
}

static void
gtk_style_constructed (GObject *object)
{
  GtkStylePrivate *priv;

  priv = GTK_STYLE_GET_PRIVATE (object);

  if (priv->context)
    {
      gtk_style_update_from_context (GTK_STYLE (object));

      priv->context_changed_id = g_signal_connect (priv->context, "changed",
                                                   G_CALLBACK (style_context_changed), object);
    }
}

/**
 * gtk_style_copy:
 * @style: a #GtkStyle
 *
 * Creates a copy of the passed in #GtkStyle object.
 *
 * Returns: (transfer full): a copy of @style
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
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

GtkStyle*
_gtk_style_new_for_path (GdkScreen     *screen,
                         GtkWidgetPath *path)
{
  GtkStyleContext *context;
  GtkStyle *style;

  context = gtk_style_context_new ();

  if (screen)
    gtk_style_context_set_screen (context, screen);

  gtk_style_context_set_path (context, path);

  style = g_object_new (GTK_TYPE_STYLE,
                        "context", context,
                        NULL);

  g_object_unref (context);

  return style;
}

/**
 * gtk_style_new:
 *
 * Creates a new #GtkStyle.
 *
 * Returns: a new #GtkStyle.
 *
 * Deprecated: 3.0: Use #GtkStyleContext
 */
GtkStyle*
gtk_style_new (void)
{
  GtkWidgetPath *path;
  GtkStyle *style;

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_WIDGET);

  style = _gtk_style_new_for_path (gdk_screen_get_default (), path);

  gtk_widget_path_free (path);

  return style;
}

/**
 * gtk_style_has_context:
 * @style: a #GtkStyle
 *
 * Returns whether @style has an associated #GtkStyleContext.
 *
 * Returns: %TRUE if @style has a #GtkStyleContext
 *
 * Since: 3.0
 */
gboolean
gtk_style_has_context (GtkStyle *style)
{
  GtkStylePrivate *priv;

  priv = GTK_STYLE_GET_PRIVATE (style);

  return priv->context != NULL;
}

/**
 * gtk_style_attach: (skip)
 * @style: a #GtkStyle.
 * @window: a #GdkWindow.
 *
 * Attaches a style to a window; this process allocates the
 * colors and creates the GC’s for the style - it specializes
 * it to a particular visual. The process may involve the creation
 * of a new style if the style has already been attached to a
 * window with a different style and visual.
 *
 * Since this function may return a new object, you have to use it
 * in the following way:
 * `style = gtk_style_attach (style, window)`
 *
 * Returns: Either @style, or a newly-created #GtkStyle.
 *   If the style is newly created, the style parameter
 *   will be unref'ed, and the new style will have
 *   a reference count belonging to the caller.
 *
 * Deprecated:3.0: Use gtk_widget_style_attach() instead
 */
GtkStyle*
gtk_style_attach (GtkStyle  *style,
                  GdkWindow *window)
{
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (window != NULL, NULL);

  return style;
}

/**
 * gtk_style_detach:
 * @style: a #GtkStyle
 *
 * Detaches a style from a window. If the style is not attached
 * to any windows anymore, it is unrealized. See gtk_style_attach().
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
void
gtk_style_detach (GtkStyle *style)
{
  g_return_if_fail (GTK_IS_STYLE (style));
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
 * Returns: (transfer none): icon set of @stock_id
 *
 * Deprecated:3.0: Use gtk_style_context_lookup_icon_set() instead
 */
GtkIconSet*
gtk_style_lookup_icon_set (GtkStyle   *style,
                           const char *stock_id)
{
  GtkStylePrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  priv = GTK_STYLE_GET_PRIVATE (style);

  if (priv->context)
    return gtk_style_context_lookup_icon_set (priv->context, stock_id);

  return gtk_icon_factory_lookup_default (stock_id);
}

/**
 * gtk_style_lookup_color:
 * @style: a #GtkStyle
 * @color_name: the name of the logical color to look up
 * @color: (out): the #GdkColor to fill in
 *
 * Looks up @color_name in the style’s logical color mappings,
 * filling in @color and returning %TRUE if found, otherwise
 * returning %FALSE. Do not cache the found mapping, because
 * it depends on the #GtkStyle and might change when a theme
 * switch occurs.
 *
 * Returns: %TRUE if the mapping was found.
 *
 * Since: 2.10
 *
 * Deprecated:3.0: Use gtk_style_context_lookup_color() instead
 **/
gboolean
gtk_style_lookup_color (GtkStyle   *style,
                        const char *color_name,
                        GdkColor   *color)
{
  GtkStylePrivate *priv;
  gboolean result;
  GdkRGBA rgba;

  g_return_val_if_fail (GTK_IS_STYLE (style), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  priv = GTK_STYLE_GET_PRIVATE (style);

  if (!priv->context)
    return FALSE;

  result = gtk_style_context_lookup_color (priv->context, color_name, &rgba);

  if (color)
    {
      color->red = (guint16) (rgba.red * 65535);
      color->green = (guint16) (rgba.green * 65535);
      color->blue = (guint16) (rgba.blue * 65535);
      color->pixel = 0;
    }

  return result;
}

/**
 * gtk_style_set_background:
 * @style: a #GtkStyle
 * @window: a #GdkWindow
 * @state_type: a state
 * 
 * Sets the background of @window to the background color or pixmap
 * specified by @style for the given state.
 *
 * Deprecated:3.0: Use gtk_style_context_set_background() instead
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
  GtkStylePrivate *priv;

  priv = GTK_STYLE_GET_PRIVATE (style);

  return g_object_new (G_OBJECT_TYPE (style),
                       "context", priv->context,
                       NULL);
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
}

static void
gtk_style_real_init_from_rc (GtkStyle   *style,
			     GtkRcStyle *rc_style)
{
}

/**
 * gtk_style_get_style_property:
 * @style: a #GtkStyle
 * @widget_type: the #GType of a descendant of #GtkWidget
 * @property_name: the name of the style property to get
 * @value: (out): a #GValue where the value of the property being
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
  GtkStylePrivate *priv;
  GtkWidgetClass *klass;
  GParamSpec *pspec;
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

  priv = GTK_STYLE_GET_PRIVATE (style);
  peek_value = _gtk_style_context_peek_style_property (priv->context,
                                                       widget_type,
                                                       pspec);

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
 * @var_args: a va_list of pairs of property names and
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
  GtkStylePrivate *priv;
  const char *property_name;
  GtkWidgetClass *klass;

  g_return_if_fail (GTK_IS_STYLE (style));

  klass = g_type_class_ref (widget_type);

  priv = GTK_STYLE_GET_PRIVATE (style);
  property_name = first_property_name;
  while (property_name)
    {
      GParamSpec *pspec;
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

      peek_value = _gtk_style_context_peek_style_property (priv->context, widget_type,
                                                           pspec);
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
 * @...: pairs of property names and locations to
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

static void
gtk_style_real_realize (GtkStyle *style)
{
}

static void
gtk_style_real_unrealize (GtkStyle *style)
{
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
 *     don’t scale.
 * @widget: (allow-none): the widget
 * @detail: (allow-none): a style detail
 *
 * Renders the icon specified by @source at the given @size
 * according to the given parameters and returns the result in a
 * pixbuf.
 *
 * Returns: (transfer full): a newly-created #GdkPixbuf
 *     containing the rendered icon
 *
 * Deprecated:3.0: Use gtk_render_icon_pixbuf() instead
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
 * @state_type:
 * @x:
 * @y:
 * @width:
 * @height:
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
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
gtk_default_render_icon (GtkStyle            *style,
                         const GtkIconSource *source,
                         GtkTextDirection     direction,
                         GtkStateType         state,
                         GtkIconSize          size,
                         GtkWidget           *widget,
                         const gchar         *detail)
{
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;
  GdkPixbuf *pixbuf;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  if (!context)
    return NULL;

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  switch (state)
    {
    case GTK_STATE_PRELIGHT:
      flags |= GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_INSENSITIVE:
      flags |= GTK_STATE_FLAG_INSENSITIVE;
      break;
    default:
      break;
    }

  gtk_style_context_set_state (context, flags);

  pixbuf = gtk_render_icon_pixbuf (context, source, size);

  gtk_style_context_restore (context);

  return pixbuf;
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
transform_detail_string (const gchar     *detail,
			 GtkStyleContext *context)
{
  if (!detail)
    return;

  if (strcmp (detail, "arrow") == 0)
    gtk_style_context_add_class (context, "arrow");
  else if (strcmp (detail, "button") == 0)
    gtk_style_context_add_class (context, "button");
  else if (strcmp (detail, "buttondefault") == 0)
    {
      gtk_style_context_add_class (context, "button");
      gtk_style_context_add_class (context, "default");
    }
  else if (strcmp (detail, "calendar") == 0)
    gtk_style_context_add_class (context, "calendar");
  else if (strcmp (detail, "cellcheck") == 0)
    {
      gtk_style_context_add_class (context, "cell");
      gtk_style_context_add_class (context, "check");
    }
  else if (strcmp (detail, "cellradio") == 0)
    {
      gtk_style_context_add_class (context, "cell");
      gtk_style_context_add_class (context, "radio");
    }
  else if (strcmp (detail, "checkbutton") == 0)
    gtk_style_context_add_class (context, "check");
  else if (strcmp (detail, "check") == 0)
    {
      gtk_style_context_add_class (context, "check");
      gtk_style_context_add_class (context, "menu");
    }
  else if (strcmp (detail, "radiobutton") == 0)
    {
      gtk_style_context_add_class (context, "radio");
    }
  else if (strcmp (detail, "option") == 0)
    {
      gtk_style_context_add_class (context, "radio");
      gtk_style_context_add_class (context, "menu");
    }
  else if (strcmp (detail, "entry") == 0 ||
           strcmp (detail, "entry_bg") == 0)
    gtk_style_context_add_class (context, "entry");
  else if (strcmp (detail, "expander") == 0)
    gtk_style_context_add_class (context, "expander");
  else if (strcmp (detail, "tooltip") == 0)
    gtk_style_context_add_class (context, "tooltip");
  else if (strcmp (detail, "frame") == 0)
    gtk_style_context_add_class (context, "frame");
  else if (strcmp (detail, "scrolled_window") == 0)
    gtk_style_context_add_class (context, "scrolled-window");
  else if (strcmp (detail, "viewport") == 0 ||
	   strcmp (detail, "viewportbin") == 0)
    gtk_style_context_add_class (context, "viewport");
  else if (strncmp (detail, "trough", 6) == 0)
    gtk_style_context_add_class (context, "trough");
  else if (strcmp (detail, "spinbutton") == 0)
    gtk_style_context_add_class (context, "spinbutton");
  else if (strcmp (detail, "spinbutton_up") == 0)
    {
      gtk_style_context_add_class (context, "spinbutton");
      gtk_style_context_add_class (context, "button");
      gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
    }
  else if (strcmp (detail, "spinbutton_down") == 0)
    {
      gtk_style_context_add_class (context, "spinbutton");
      gtk_style_context_add_class (context, "button");
      gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);
    }
  else if ((detail[0] == 'h' || detail[0] == 'v') &&
           strncmp (&detail[1], "scrollbar_", 10) == 0)
    {
      gtk_style_context_add_class (context, "button");
      gtk_style_context_add_class (context, "scrollbar");
    }
  else if (strcmp (detail, "slider") == 0)
    {
      gtk_style_context_add_class (context, "slider");
      gtk_style_context_add_class (context, "scrollbar");
    }
  else if (strcmp (detail, "vscale") == 0 ||
           strcmp (detail, "hscale") == 0)
    {
      gtk_style_context_add_class (context, "slider");
      gtk_style_context_add_class (context, "scale");
    }
  else if (strcmp (detail, "menuitem") == 0)
    {
      gtk_style_context_add_class (context, "menuitem");
      gtk_style_context_add_class (context, "menu");
    }
  else if (strcmp (detail, "menu") == 0)
    {
      gtk_style_context_add_class (context, "popup");
      gtk_style_context_add_class (context, "menu");
    }
  else if (strcmp (detail, "accellabel") == 0)
    gtk_style_context_add_class (context, "accelerator");
  else if (strcmp (detail, "menubar") == 0)
    gtk_style_context_add_class (context, "menubar");
  else if (strcmp (detail, "base") == 0)
    gtk_style_context_add_class (context, "background");
  else if (strcmp (detail, "bar") == 0 ||
           strcmp (detail, "progressbar") == 0)
    gtk_style_context_add_class (context, "progressbar");
  else if (strcmp (detail, "toolbar") == 0)
    gtk_style_context_add_class (context, "toolbar");
  else if (strcmp (detail, "handlebox_bin") == 0)
    gtk_style_context_add_class (context, "dock");
  else if (strcmp (detail, "notebook") == 0)
    gtk_style_context_add_class (context, "notebook");
  else if (strcmp (detail, "tab") == 0)
    {
      gtk_style_context_add_class (context, "notebook");
      gtk_style_context_add_region (context, GTK_STYLE_REGION_TAB, 0);
    }
  else if (g_str_has_prefix (detail, "cell"))
    {
      GtkRegionFlags row, col;
      gboolean ruled = FALSE;
      GStrv tokens;
      guint i;

      tokens = g_strsplit (detail, "_", -1);
      row = col = 0;
      i = 0;

      while (tokens[i])
        {
          if (strcmp (tokens[i], "even") == 0)
            row |= GTK_REGION_EVEN;
          else if (strcmp (tokens[i], "odd") == 0)
            row |= GTK_REGION_ODD;
          else if (strcmp (tokens[i], "start") == 0)
            col |= GTK_REGION_FIRST;
          else if (strcmp (tokens[i], "end") == 0)
            col |= GTK_REGION_LAST;
          else if (strcmp (tokens[i], "ruled") == 0)
            ruled = TRUE;
          else if (strcmp (tokens[i], "sorted") == 0)
            col |= GTK_REGION_SORTED;

          i++;
        }

      if (!ruled)
        row &= ~(GTK_REGION_EVEN | GTK_REGION_ODD);

      gtk_style_context_add_class (context, "cell");
      gtk_style_context_add_region (context, "row", row);
      gtk_style_context_add_region (context, "column", col);

      g_strfreev (tokens);
    }
}

static void
gtk_default_draw_hline (GtkStyle     *style,
                        cairo_t       *cr,
                        GtkStateType  state_type,
                        GtkWidget     *widget,
                        const gchar   *detail,
                        gint          x1,
                        gint          x2,
                        gint          y)
{
  GtkStyleContext *context;
  GtkStylePrivate *priv;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  cairo_save (cr);

  gtk_render_line (context, cr,
                   x1, y, x2, y);

  cairo_restore (cr);

  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  cairo_save (cr);

  gtk_render_line (context, cr,
                   x, y1, x, y2);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;

  if (shadow_type == GTK_SHADOW_NONE)
    return;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  cairo_save (cr);

  gtk_render_frame (context, cr,
                    (gdouble) x,
                    (gdouble) y,
                    (gdouble) width,
                    (gdouble) height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;
  gdouble angle, size;

  if (arrow_type == GTK_ARROW_NONE)
    return;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  switch (arrow_type)
    {
    case GTK_ARROW_UP:
      angle = 0;
      size = width;
      break;
    case GTK_ARROW_RIGHT:
      angle = G_PI / 2;
      size = height;
      break;
    case GTK_ARROW_DOWN:
      angle = G_PI;
      size = width;
      break;
    case GTK_ARROW_LEFT:
      angle = 3 * (G_PI / 2);
      size = height;
      break;
    default:
      g_assert_not_reached ();
    }

  switch (state)
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

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_arrow (context,
                    cr, angle,
                    (gdouble) x,
                    (gdouble) y,
                    size);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  if (shadow_type == GTK_SHADOW_IN)
    flags |= GTK_STATE_FLAG_ACTIVE;

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_background (context, cr, x, y, width, height);

  if (shadow_type != GTK_SHADOW_NONE)
    gtk_render_frame (context, cr, x, y, width, height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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
    case GTK_STATE_FOCUSED:
      flags |= GTK_STATE_FLAG_FOCUSED;
      break;
    default:
      break;
    }

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_background (context, cr,
                         (gdouble) x,
                         (gdouble) y,
                         (gdouble) width,
                         (gdouble) height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_check (context,
                    cr, x, y,
                    width, height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);
  gtk_render_option (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     (gdouble) width,
                     (gdouble) height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (shadow_type == GTK_SHADOW_NONE)
    return;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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
  gtk_render_frame_gap (context, cr,
                        (gdouble) x,
                        (gdouble) y,
                        (gdouble) width,
                        (gdouble) height,
                        gap_side,
                        (gdouble) gap_x,
                        (gdouble) gap_x + gap_width);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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
  gtk_render_background (context, cr,
                         (gdouble) x,
                         (gdouble) y,
                         (gdouble) width,
                         (gdouble) height);


  if (shadow_type != GTK_SHADOW_NONE)
    gtk_render_frame_gap (context, cr,
			  (gdouble) x,
			  (gdouble) y,
			  (gdouble) width,
			  (gdouble) height,
			  gap_side,
			  (gdouble) gap_x,
			  (gdouble) gap_x + gap_width);
  
  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  gtk_render_extension (context, cr,
                        (gdouble) x,
                        (gdouble) y,
                        (gdouble) width,
                        (gdouble) height,
                        gap_side);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  cairo_save (cr);

  gtk_render_focus (context, cr,
                    (gdouble) x,
                    (gdouble) y,
                    (gdouble) width,
                    (gdouble) height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_slider (context, cr,  x, y, width, height, orientation);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_handle (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     (gdouble) width,
                     (gdouble) height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;
  gint size;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  gtk_style_context_add_class (context, "expander");

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

  if (widget &&
      gtk_widget_class_find_style_property (GTK_WIDGET_GET_CLASS (widget),
                                            "expander-size"))
    gtk_widget_style_get (widget, "expander-size", &size, NULL);
  else
    size = 12;

  if (expander_style == GTK_EXPANDER_EXPANDED)
    flags |= GTK_STATE_FLAG_ACTIVE;

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_expander (context, cr,
                       (gdouble) x - (size / 2),
                       (gdouble) y - (size / 2),
                       (gdouble) size,
                       (gdouble) size);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

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

  gtk_style_context_set_state (context, flags);

  cairo_save (cr);

  gtk_render_layout (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     layout);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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
  GtkStyleContext *context;
  GtkStylePrivate *priv;
  GtkStateFlags flags = 0;
  GtkJunctionSides sides = 0;

  if (widget)
    context = gtk_widget_get_style_context (widget);
  else
    {
      priv = GTK_STYLE_GET_PRIVATE (style);
      context = priv->context;
    }

  gtk_style_context_save (context);

  if (detail)
    transform_detail_string (detail, context);

  gtk_style_context_add_class (context, "grip");

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

  gtk_style_context_set_state (context, flags);

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      sides = GTK_JUNCTION_CORNER_TOPLEFT;
      break;
    case GDK_WINDOW_EDGE_NORTH:
      sides = GTK_JUNCTION_TOP;
      break;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      sides = GTK_JUNCTION_CORNER_TOPRIGHT;
      break;
    case GDK_WINDOW_EDGE_WEST:
      sides = GTK_JUNCTION_LEFT;
      break;
    case GDK_WINDOW_EDGE_EAST:
      sides = GTK_JUNCTION_RIGHT;
      break;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      sides = GTK_JUNCTION_CORNER_BOTTOMLEFT;
      break;
    case GDK_WINDOW_EDGE_SOUTH:
      sides = GTK_JUNCTION_BOTTOM;
      break;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      sides = GTK_JUNCTION_CORNER_BOTTOMRIGHT;
      break;
    }

  gtk_style_context_set_junction_sides (context, sides);

  cairo_save (cr);

  gtk_render_handle (context, cr,
                     (gdouble) x,
                     (gdouble) y,
                     (gdouble) width,
                     (gdouble) height);

  cairo_restore (cr);
  gtk_style_context_restore (context);
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

  num_steps = 12;
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
 *
 * Deprecated:3.0: Use gtk_render_line() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_hline != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_hline (style, cr, state_type,
                                           widget, detail,
                                           x1, x2, y);

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
 *
 * Deprecated:3.0: Use gtk_render_line() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_vline != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_vline (style, cr, state_type,
                                           widget, detail,
                                           y1_, y2_, x);

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
 *
 * Deprecated:3.0: Use gtk_render_frame() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_shadow (style, cr, state_type, shadow_type,
                                            widget, detail,
                                            x, y, width, height);

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
 *
 * Deprecated:3.0: Use gtk_render_arrow() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_arrow != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_arrow (style, cr, state_type, shadow_type,
                                           widget, detail,
                                           arrow_type, fill, x, y, width, height);

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
 *
 * Deprecated:3.0: Use cairo instead
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
 *
 * Deprecated:3.0: Use gtk_render_frame() and gtk_render_background() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_box != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_box (style, cr, state_type, shadow_type,
                                         widget, detail,
                                         x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_paint_flat_box:
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
 * Draws a flat box on @cr with the given parameters.
 *
 * Deprecated:3.0: Use gtk_render_frame() and gtk_render_background() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_flat_box != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_flat_box (style, cr, state_type, shadow_type,
                                              widget, detail,
                                              x, y, width, height);

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
 *
 * Deprecated:3.0: Use gtk_render_check() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_check != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_check (style, cr, state_type, shadow_type,
                                           widget, detail,
                                           x, y, width, height);

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
 *
 * Deprecated:3.0: Use gtk_render_option() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_option != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_option (style, cr, state_type, shadow_type,
                                            widget, detail,
                                            x, y, width, height);

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
 *
 * Deprecated:3.0: Use cairo instead
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
 *
 * Deprecated:3.0: Use gtk_render_frame_gap() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_shadow_gap != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_shadow_gap (style, cr, state_type, shadow_type,
                                                widget, detail,
                                                x, y, width, height, gap_side, gap_x, gap_width);

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
 *
 * Deprecated:3.0: Use gtk_render_frame_gap() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_box_gap != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_box_gap (style, cr, state_type, shadow_type,
                                             widget, detail,
                                             x, y, width, height, gap_side, gap_x, gap_width);

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
 *
 * Deprecated:3.0: Use gtk_render_extension() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_extension != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_extension (style, cr, state_type, shadow_type,
                                               widget, detail,
                                               x, y, width, height, gap_side);

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
 *
 * Deprecated:3.0: Use gtk_render_focus() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_focus != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_focus (style, cr, state_type,
                                           widget, detail,
                                           x, y, width, height);

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
 *
 * Deprecated:3.0: Use gtk_render_slider() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_slider != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_slider (style, cr, state_type, shadow_type,
                                            widget, detail,
                                            x, y, width, height, orientation);

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
 *
 * Deprecated:3.0: Use gtk_render_handle() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_handle != NULL);
  g_return_if_fail (cr != NULL);
  g_return_if_fail (width >= 0);
  g_return_if_fail (height >= 0);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_handle (style, cr, state_type, shadow_type,
                                            widget, detail,
                                            x, y, width, height, orientation);

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
 * “expander-size” style property of @widget.  (If widget is not
 * specified or doesn’t have an “expander-size” property, an
 * unspecified default size will be used, since the caller doesn't
 * have sufficient information to position the expander, this is
 * likely not useful.) The expander is expander_size pixels tall
 * in the collapsed position and expander_size pixels wide in the
 * expanded position.
 *
 * Deprecated:3.0: Use gtk_render_expander() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_expander != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_expander (style, cr, state_type,
                                              widget, detail,
                                              x, y, expander_style);

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
 *
 * Deprecated:3.0: Use gtk_render_layout() instead
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
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (GTK_STYLE_GET_CLASS (style)->draw_layout != NULL);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  GTK_STYLE_GET_CLASS (style)->draw_layout (style, cr, state_type, use_text,
                                            widget, detail,
                                            x, y, layout);

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
 *
 * Deprecated:3.0: Use gtk_render_handle() instead
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
 * @step: the nth step
 * @x: the x origin of the rectangle in which to draw the spinner
 * @y: the y origin of the rectangle in which to draw the spinner
 * @width: the width of the rectangle in which to draw the spinner
 * @height: the height of the rectangle in which to draw the spinner
 *
 * Draws a spinner on @window using the given parameters.
 *
 * Deprecated:3.0: Use gtk_render_activity() instead
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

static GtkStyle *
gtk_widget_get_default_style_for_screen (GdkScreen *screen)
{
  GtkStyle *default_style;

  default_style = g_object_get_data (G_OBJECT (screen), "gtk-legacy-default-style");
  if (default_style == NULL)
    {
      default_style = gtk_style_new ();
      g_object_set_data_full (G_OBJECT (screen),
                              I_("gtk-legacy-default-style"),
                              default_style,
                              g_object_unref);
    }

  return default_style;
}

/**
 * gtk_widget_get_default_style:
 *
 * Returns the default style used by all widgets initially.
 *
 * Returns: (transfer none): the default style. This #GtkStyle
 *     object is owned by GTK+ and should not be modified or freed.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead, and
 *     gtk_css_provider_get_default() to obtain a #GtkStyleProvider
 *     with the default widget style information.
 */
GtkStyle *
gtk_widget_get_default_style (void)
{
  static GtkStyle *default_style = NULL;
  GtkStyle *style = NULL;
  GdkScreen *screen = gdk_screen_get_default ();

  if (screen)
    style = gtk_widget_get_default_style_for_screen (screen);
  else
    {
      if (default_style == NULL)
        default_style = gtk_style_new ();
      style = default_style;
    }

  return style;
}

/**
 * gtk_widget_style_attach:
 * @widget: a #GtkWidget
 *
 * This function attaches the widget’s #GtkStyle to the widget's
 * #GdkWindow. It is a replacement for
 *
 * |[
 * widget->style = gtk_style_attach (widget->style, widget->window);
 * ]|
 *
 * and should only ever be called in a derived widget’s “realize”
 * implementation which does not chain up to its parent class'
 * “realize” implementation, because one of the parent classes
 * (finally #GtkWidget) would attach the style itself.
 *
 * Since: 2.20
 *
 * Deprecated: 3.0: This step is unnecessary with #GtkStyleContext.
 **/
void
gtk_widget_style_attach (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_realized (widget));
}

/**
 * gtk_widget_has_rc_style:
 * @widget: a #GtkWidget
 *
 * Determines if the widget style has been looked up through the rc mechanism.
 *
 * Returns: %TRUE if the widget has been looked up through the rc
 *   mechanism, %FALSE otherwise.
 *
 * Since: 2.20
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
gboolean
gtk_widget_has_rc_style (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return FALSE;
}

/**
 * gtk_widget_set_style:
 * @widget: a #GtkWidget
 * @style: (allow-none): a #GtkStyle, or %NULL to remove the effect
 *     of a previous call to gtk_widget_set_style() and go back to
 *     the default style
 *
 * Used to set the #GtkStyle for a widget (@widget->style). Since
 * GTK 3, this function does nothing, the passed in style is ignored.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
void
gtk_widget_set_style (GtkWidget *widget,
                      GtkStyle  *style)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
}

/**
 * gtk_widget_ensure_style:
 * @widget: a #GtkWidget
 *
 * Ensures that @widget has a style (@widget->style).
 *
 * Not a very useful function; most of the time, if you
 * want the style, the widget is realized, and realized
 * widgets are guaranteed to have a style already.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
void
gtk_widget_ensure_style (GtkWidget *widget)
{
  GtkStyle *style;
  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_style (widget);
  if (style == gtk_widget_get_default_style ())
    {
      g_object_unref (style);
      _gtk_widget_set_style (widget, NULL);

      g_signal_emit_by_name (widget, "style-set", 0, NULL);
    }
}

/**
 * gtk_widget_get_style:
 * @widget: a #GtkWidget
 *
 * Simply an accessor function that returns @widget->style.
 *
 * Returns: (transfer none): the widget’s #GtkStyle
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
GtkStyle*
gtk_widget_get_style (GtkWidget *widget)
{
  GtkStyle *style;
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  style = _gtk_widget_get_style (widget);

  if (style == NULL)
    {
      style = g_object_new (GTK_TYPE_STYLE,
                            "context", gtk_widget_get_style_context (widget),
                            NULL);
      _gtk_widget_set_style (widget, style);
    }

  return style;
}

/**
 * gtk_widget_modify_style:
 * @widget: a #GtkWidget
 * @style: the #GtkRcStyle-struct holding the style modifications
 *
 * Modifies style values on the widget.
 *
 * Modifications made using this technique take precedence over
 * style values set via an RC file, however, they will be overridden
 * if a style is explicitly set on the widget using gtk_widget_set_style().
 * The #GtkRcStyle-struct is designed so each field can either be
 * set or unset, so it is possible, using this function, to modify some
 * style values and leave the others unchanged.
 *
 * Note that modifications made with this function are not cumulative
 * with previous calls to gtk_widget_modify_style() or with such
 * functions as gtk_widget_modify_fg(). If you wish to retain
 * previous values, you must first call gtk_widget_get_modifier_style(),
 * make your modifications to the returned style, then call
 * gtk_widget_modify_style() with that style. On the other hand,
 * if you first call gtk_widget_modify_style(), subsequent calls
 * to such functions gtk_widget_modify_fg() will have a cumulative
 * effect with the initial modifications.
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 */
void
gtk_widget_modify_style (GtkWidget      *widget,
                         GtkRcStyle     *style)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_RC_STYLE (style));

  g_object_set_data_full (G_OBJECT (widget),
                          "gtk-rc-style",
                           gtk_rc_style_copy (style),
                           (GDestroyNotify) g_object_unref);
}

/**
 * gtk_widget_get_modifier_style:
 * @widget: a #GtkWidget
 *
 * Returns the current modifier style for the widget. (As set by
 * gtk_widget_modify_style().) If no style has previously set, a new
 * #GtkRcStyle will be created with all values unset, and set as the
 * modifier style for the widget. If you make changes to this rc
 * style, you must call gtk_widget_modify_style(), passing in the
 * returned rc style, to make sure that your changes take effect.
 *
 * Caution: passing the style back to gtk_widget_modify_style() will
 * normally end up destroying it, because gtk_widget_modify_style() copies
 * the passed-in style and sets the copy as the new modifier style,
 * thus dropping any reference to the old modifier style. Add a reference
 * to the modifier style if you want to keep it alive.
 *
 * Returns: (transfer none): the modifier style for the widget.
 *     This rc style is owned by the widget. If you want to keep a
 *     pointer to value this around, you must add a refcount using
 *     g_object_ref().
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 */
GtkRcStyle *
gtk_widget_get_modifier_style (GtkWidget *widget)
{
  GtkRcStyle *rc_style;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  rc_style = g_object_get_data (G_OBJECT (widget), "gtk-rc-style");

  if (!rc_style)
    {
      rc_style = gtk_rc_style_new ();
      g_object_set_data_full (G_OBJECT (widget),
                              "gtk-rc-style",
                              rc_style,
                              (GDestroyNotify) g_object_unref);
    }

  return rc_style;
}

static void
gtk_widget_modify_color_component (GtkWidget      *widget,
                                   GtkRcFlags      component,
                                   GtkStateType    state,
                                   const GdkColor *color)
{
  GtkRcStyle *rc_style = gtk_widget_get_modifier_style (widget);

  if (color)
    {
      switch (component)
        {
        case GTK_RC_FG:
          rc_style->fg[state] = *color;
          break;
        case GTK_RC_BG:
          rc_style->bg[state] = *color;
          break;
        case GTK_RC_TEXT:
          rc_style->text[state] = *color;
          break;
        case GTK_RC_BASE:
          rc_style->base[state] = *color;
          break;
        default:
          g_assert_not_reached();
        }

      rc_style->color_flags[state] |= component;
    }
  else
    rc_style->color_flags[state] &= ~component;

  gtk_widget_modify_style (widget, rc_style);
}

/**
 * gtk_widget_modify_fg:
 * @widget: a #GtkWidget
 * @state: the state for which to set the foreground color
 * @color: (allow-none): the color to assign (does not need to be allocated),
 *     or %NULL to undo the effect of previous calls to
 *     of gtk_widget_modify_fg().
 *
 * Sets the foreground color for a widget in a particular state.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Deprecated:3.0: Use gtk_widget_override_color() instead
 */
void
gtk_widget_modify_fg (GtkWidget      *widget,
                      GtkStateType    state,
                      const GdkColor *color)
{
  GtkStateFlags flags;
  GdkRGBA rgba;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
    }

  if (color)
    {
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1;

      gtk_widget_override_color (widget, flags, &rgba);
    }
  else
    gtk_widget_override_color (widget, flags, NULL);
}

/**
 * gtk_widget_modify_bg:
 * @widget: a #GtkWidget
 * @state: the state for which to set the background color
 * @color: (allow-none): the color to assign (does not need
 *     to be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_bg().
 *
 * Sets the background color for a widget in a particular state.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * > Note that “no window” widgets (which have the %GTK_NO_WINDOW
 * > flag set) draw on their parent container’s window and thus may
 * > not draw any background themselves. This is the case for e.g.
 * > #GtkLabel.
 * >
 * > To modify the background of such widgets, you have to set the
 * > background color on their parent; if you want to set the background
 * > of a rectangular area around a label, try placing the label in
 * > a #GtkEventBox widget and setting the background color on that.
 *
 * Deprecated:3.0: Use gtk_widget_override_background_color() instead
 */
void
gtk_widget_modify_bg (GtkWidget      *widget,
                      GtkStateType    state,
                      const GdkColor *color)
{
  GtkStateFlags flags;
  GdkRGBA rgba;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
    }

  if (color)
    {
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1;

      gtk_widget_override_background_color (widget, flags, &rgba);
    }
  else
    gtk_widget_override_background_color (widget, flags, NULL);
}

/**
 * gtk_widget_modify_text:
 * @widget: a #GtkWidget
 * @state: the state for which to set the text color
 * @color: (allow-none): the color to assign (does not need to
 *     be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_text().
 *
 * Sets the text color for a widget in a particular state.
 *
 * All other style values are left untouched.
 * The text color is the foreground color used along with the
 * base color (see gtk_widget_modify_base()) for widgets such
 * as #GtkEntry and #GtkTextView.
 * See also gtk_widget_modify_style().
 *
 * Deprecated:3.0: Use gtk_widget_override_color() instead
 */
void
gtk_widget_modify_text (GtkWidget      *widget,
                        GtkStateType    state,
                        const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_TEXT, state, color);
}

/**
 * gtk_widget_modify_base:
 * @widget: a #GtkWidget
 * @state: the state for which to set the base color
 * @color: (allow-none): the color to assign (does not need to
 *     be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_base().
 *
 * Sets the base color for a widget in a particular state.
 * All other style values are left untouched. The base color
 * is the background color used along with the text color
 * (see gtk_widget_modify_text()) for widgets such as #GtkEntry
 * and #GtkTextView. See also gtk_widget_modify_style().
 *
 * > Note that “no window” widgets (which have the %GTK_NO_WINDOW
 * > flag set) draw on their parent container’s window and thus may
 * > not draw any background themselves. This is the case for e.g.
 * > #GtkLabel.
 * >
 * > To modify the background of such widgets, you have to set the
 * > base color on their parent; if you want to set the background
 * > of a rectangular area around a label, try placing the label in
 * > a #GtkEventBox widget and setting the base color on that.
 *
 * Deprecated:3.0: Use gtk_widget_override_background_color() instead
 */
void
gtk_widget_modify_base (GtkWidget      *widget,
                        GtkStateType    state,
                        const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_BASE, state, color);
}

/**
 * gtk_widget_modify_cursor:
 * @widget: a #GtkWidget
 * @primary: (nullable): the color to use for primary cursor (does not
 *     need to be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_cursor().
 * @secondary: (nullable): the color to use for secondary cursor (does
 *     not need to be allocated), or %NULL to undo the effect of
 *     previous calls to of gtk_widget_modify_cursor().
 *
 * Sets the cursor color to use in a widget, overriding the #GtkWidget
 * cursor-color and secondary-cursor-color
 * style properties.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Since: 2.12
 *
 * Deprecated: 3.0: Use gtk_widget_override_cursor() instead.
 */
void
gtk_widget_modify_cursor (GtkWidget      *widget,
                          const GdkColor *primary,
                          const GdkColor *secondary)
{
  GdkRGBA primary_rgba, secondary_rgba;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  primary_rgba.red = primary->red / 65535.;
  primary_rgba.green = primary->green / 65535.;
  primary_rgba.blue = primary->blue / 65535.;
  primary_rgba.alpha = 1;

  secondary_rgba.red = secondary->red / 65535.;
  secondary_rgba.green = secondary->green / 65535.;
  secondary_rgba.blue = secondary->blue / 65535.;
  secondary_rgba.alpha = 1;

  gtk_widget_override_cursor (widget, &primary_rgba, &secondary_rgba);
}

/**
 * gtk_widget_modify_font:
 * @widget: a #GtkWidget
 * @font_desc: (allow-none): the font description to use, or %NULL
 *     to undo the effect of previous calls to gtk_widget_modify_font()
 *
 * Sets the font to use for a widget.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Deprecated:3.0: Use gtk_widget_override_font() instead
 */
void
gtk_widget_modify_font (GtkWidget            *widget,
                        PangoFontDescription *font_desc)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_override_font (widget, font_desc);
}

/**
 * gtk_widget_reset_rc_styles:
 * @widget: a #GtkWidget.
 *
 * Reset the styles of @widget and all descendents, so when
 * they are looked up again, they get the correct values
 * for the currently loaded RC file settings.
 *
 * This function is not useful for applications.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead, and gtk_widget_reset_style()
 */
void
gtk_widget_reset_rc_styles (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_reset_style (widget);
}

/**
 * gtk_widget_path:
 * @widget: a #GtkWidget
 * @path_length: (out) (allow-none): location to store length of the path,
 *     or %NULL
 * @path: (out) (allow-none): location to store allocated path string,
 *     or %NULL
 * @path_reversed: (out) (allow-none): location to store allocated reverse
 *     path string, or %NULL
 *
 * Obtains the full path to @widget. The path is simply the name of a
 * widget and all its parents in the container hierarchy, separated by
 * periods. The name of a widget comes from
 * gtk_widget_get_name(). Paths are used to apply styles to a widget
 * in gtkrc configuration files. Widget names are the type of the
 * widget by default (e.g. “GtkButton”) or can be set to an
 * application-specific value with gtk_widget_set_name(). By setting
 * the name of a widget, you allow users or theme authors to apply
 * styles to that specific widget in their gtkrc
 * file. @path_reversed_p fills in the path in reverse order,
 * i.e. starting with @widget’s name instead of starting with the name
 * of @widget’s outermost ancestor.
 *
 * Deprecated:3.0: Use gtk_widget_get_path() instead
 **/
void
gtk_widget_path (GtkWidget *widget,
                 guint     *path_length,
                 gchar    **path,
                 gchar    **path_reversed)
{
  static gchar *rev_path = NULL;
  static guint tmp_path_len = 0;
  guint len;

#define INIT_PATH_SIZE (512)

  g_return_if_fail (GTK_IS_WIDGET (widget));

  len = 0;
  do
    {
      const gchar *string;
      const gchar *s;
      gchar *d;
      guint l;

      string = gtk_widget_get_name (widget);
      l = strlen (string);
      while (tmp_path_len <= len + l + 1)
        {
          tmp_path_len += INIT_PATH_SIZE;
          rev_path = g_realloc (rev_path, tmp_path_len);
        }
      s = string + l - 1;
      d = rev_path + len;
      while (s >= string)
        *(d++) = *(s--);
      len += l;

      widget = gtk_widget_get_parent (widget);

      if (widget)
        rev_path[len++] = '.';
      else
        rev_path[len++] = 0;
    }
  while (widget);

  if (path_length)
    *path_length = len - 1;
  if (path_reversed)
    *path_reversed = g_strdup (rev_path);
  if (path)
    {
      *path = g_strdup (rev_path);
      g_strreverse (*path);
    }
}

/**
 * gtk_widget_class_path:
 * @widget: a #GtkWidget
 * @path_length: (out) (optional): location to store the length of the
 *     class path, or %NULL
 * @path: (out) (optional): location to store the class path as an
 *     allocated string, or %NULL
 * @path_reversed: (out) (optional): location to store the reverse
 *     class path as an allocated string, or %NULL
 *
 * Same as gtk_widget_path(), but always uses the name of a widget’s type,
 * never uses a custom name set with gtk_widget_set_name().
 *
 * Deprecated:3.0: Use gtk_widget_get_path() instead
 **/
void
gtk_widget_class_path (GtkWidget *widget,
                       guint     *path_length,
                       gchar    **path,
                       gchar    **path_reversed)
{
  static gchar *rev_path = NULL;
  static guint tmp_path_len = 0;
  guint len;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  len = 0;
  do
    {
      const gchar *string;
      const gchar *s;
      gchar *d;
      guint l;

      string = g_type_name (G_OBJECT_TYPE (widget));
      l = strlen (string);
      while (tmp_path_len <= len + l + 1)
        {
          tmp_path_len += INIT_PATH_SIZE;
          rev_path = g_realloc (rev_path, tmp_path_len);
        }
      s = string + l - 1;
      d = rev_path + len;
      while (s >= string)
        *(d++) = *(s--);
      len += l;

      widget = gtk_widget_get_parent (widget);

      if (widget)
        rev_path[len++] = '.';
      else
        rev_path[len++] = 0;
    }
  while (widget);

  if (path_length)
    *path_length = len - 1;
  if (path_reversed)
    *path_reversed = g_strdup (rev_path);
  if (path)
    {
      *path = g_strdup (rev_path);
      g_strreverse (*path);
    }
}

/**
 * gtk_widget_render_icon:
 * @widget: a #GtkWidget
 * @stock_id: a stock ID
 * @size: (type int): a stock size. A size of (GtkIconSize)-1 means
 *     render at the size of the source and don’t scale (if there are
 *     multiple source sizes, GTK+ picks one of the available sizes).
 * @detail: (allow-none): render detail to pass to theme engine
 *
 * A convenience function that uses the theme settings for @widget
 * to look up @stock_id and render it to a pixbuf. @stock_id should
 * be a stock icon ID such as #GTK_STOCK_OPEN or #GTK_STOCK_OK. @size
 * should be a size such as #GTK_ICON_SIZE_MENU. @detail should be a
 * string that identifies the widget or code doing the rendering, so
 * that theme engines can special-case rendering for that widget or
 * code.
 *
 * The pixels in the returned #GdkPixbuf are shared with the rest of
 * the application and should not be modified. The pixbuf should be
 * freed after use with g_object_unref().
 *
 * Returns: (transfer full): a new pixbuf, or %NULL if the
 *     stock ID wasn’t known
 *
 * Deprecated: 3.0: Use gtk_widget_render_icon_pixbuf() instead.
 **/
GdkPixbuf*
gtk_widget_render_icon (GtkWidget      *widget,
                        const gchar    *stock_id,
                        GtkIconSize     size,
                        const gchar    *detail)
{
  gtk_widget_ensure_style (widget);

  return gtk_widget_render_icon_pixbuf (widget, stock_id, size);
}
