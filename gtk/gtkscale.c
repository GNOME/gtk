/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

#include <config.h>
#include <math.h>
#include "gtkintl.h"
#include "gtkscale.h"
#include "gtkmarshalers.h"
#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkalias.h"


#define	MAX_DIGITS	(64)	/* don't change this,
				 * a) you don't need to and
				 * b) you might cause buffer owerflows in
				 *    unrelated code portions otherwise
				 */

#define GTK_SCALE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_SCALE, GtkScalePrivate))

typedef struct _GtkScalePrivate GtkScalePrivate;

struct _GtkScalePrivate
{
  PangoLayout *layout;
};

enum {
  PROP_0,
  PROP_DIGITS,
  PROP_DRAW_VALUE,
  PROP_VALUE_POS
};

enum {
  FORMAT_VALUE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void gtk_scale_set_property     (GObject       *object,
                                        guint          prop_id,
                                        const GValue  *value,
                                        GParamSpec    *pspec);
static void gtk_scale_get_property     (GObject       *object,
                                        guint          prop_id,
                                        GValue        *value,
                                        GParamSpec    *pspec);
static void gtk_scale_style_set        (GtkWidget     *widget,
                                        GtkStyle      *previous);
static void gtk_scale_get_range_border (GtkRange      *range,
                                        GtkBorder     *border);
static void gtk_scale_finalize         (GObject       *object);
static void gtk_scale_screen_changed   (GtkWidget     *widget,
                                        GdkScreen     *old_screen);

G_DEFINE_ABSTRACT_TYPE (GtkScale, gtk_scale, GTK_TYPE_RANGE)

static gboolean
single_string_accumulator (GSignalInvocationHint *ihint,
                           GValue                *return_accu,
                           const GValue          *handler_return,
                           gpointer               dummy)
{
  gboolean continue_emission;
  const gchar *str;
  
  str = g_value_get_string (handler_return);
  g_value_set_string (return_accu, str);
  continue_emission = str == NULL;
  
  return continue_emission;
}


#define add_slider_binding(binding_set, keyval, mask, scroll)              \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,                 \
                                I_("move_slider"), 1, \
                                GTK_TYPE_SCROLL_TYPE, scroll)

static void
gtk_scale_class_init (GtkScaleClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass  *range_class;
  GtkBindingSet  *binding_set;
  
  gobject_class = G_OBJECT_CLASS (class);
  range_class = (GtkRangeClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  gobject_class->set_property = gtk_scale_set_property;
  gobject_class->get_property = gtk_scale_get_property;
  gobject_class->finalize = gtk_scale_finalize;

  widget_class->style_set = gtk_scale_style_set;
  widget_class->screen_changed = gtk_scale_screen_changed;

  range_class->get_range_border = gtk_scale_get_range_border;
  
  signals[FORMAT_VALUE] =
    g_signal_new (I_("format_value"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkScaleClass, format_value),
                  single_string_accumulator, NULL,
                  _gtk_marshal_STRING__DOUBLE,
                  G_TYPE_STRING, 1,
                  G_TYPE_DOUBLE);

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_int ("digits",
						     P_("Digits"),
						     P_("The number of decimal places that are displayed in the value"),
						     -1,
						     MAX_DIGITS,
						     1,
						     GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_VALUE,
                                   g_param_spec_boolean ("draw-value",
							 P_("Draw Value"),
							 P_("Whether the current value is displayed as a string next to the slider"),
							 TRUE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE_POS,
                                   g_param_spec_enum ("value-pos",
						      P_("Value Position"),
						      P_("The position in which the current value is displayed"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_TOP,
						      GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("slider-length",
							     P_("Slider Length"),
							     P_("Length of scale's slider"),
							     0,
							     G_MAXINT,
							     31,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("value-spacing",
							     P_("Value spacing"),
							     P_("Space between value text and the slider/trough area"),
							     0,
							     G_MAXINT,
							     2,
							     GTK_PARAM_READABLE));
  
  /* All bindings (even arrow keys) are on both h/v scale, because
   * blind users etc. don't care about scale orientation.
   */
  
  binding_set = gtk_binding_set_by_class (class);

  add_slider_binding (binding_set, GDK_Left, 0,
                      GTK_SCROLL_STEP_LEFT);

  add_slider_binding (binding_set, GDK_Left, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KP_Left, 0,
                      GTK_SCROLL_STEP_LEFT);

  add_slider_binding (binding_set, GDK_KP_Left, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_Right, 0,
                      GTK_SCROLL_STEP_RIGHT);

  add_slider_binding (binding_set, GDK_Right, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KP_Right, 0,
                      GTK_SCROLL_STEP_RIGHT);

  add_slider_binding (binding_set, GDK_KP_Right, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_Up, 0,
                      GTK_SCROLL_STEP_UP);

  add_slider_binding (binding_set, GDK_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KP_Up, 0,
                      GTK_SCROLL_STEP_UP);

  add_slider_binding (binding_set, GDK_KP_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_Down, 0,
                      GTK_SCROLL_STEP_DOWN);

  add_slider_binding (binding_set, GDK_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KP_Down, 0,
                      GTK_SCROLL_STEP_DOWN);

  add_slider_binding (binding_set, GDK_KP_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN);
   
  add_slider_binding (binding_set, GDK_Page_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KP_Page_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);  

  add_slider_binding (binding_set, GDK_Page_Up, 0,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KP_Page_Up, 0,
                      GTK_SCROLL_PAGE_UP);
  
  add_slider_binding (binding_set, GDK_Page_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KP_Page_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_Page_Down, 0,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KP_Page_Down, 0,
                      GTK_SCROLL_PAGE_DOWN);

  /* Logical bindings (vs. visual bindings above) */

  add_slider_binding (binding_set, GDK_plus, 0,
                      GTK_SCROLL_STEP_FORWARD);  

  add_slider_binding (binding_set, GDK_minus, 0,
                      GTK_SCROLL_STEP_BACKWARD);  

  add_slider_binding (binding_set, GDK_plus, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_FORWARD);  

  add_slider_binding (binding_set, GDK_minus, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_BACKWARD);


  add_slider_binding (binding_set, GDK_KP_Add, 0,
                      GTK_SCROLL_STEP_FORWARD);  

  add_slider_binding (binding_set, GDK_KP_Subtract, 0,
                      GTK_SCROLL_STEP_BACKWARD);  

  add_slider_binding (binding_set, GDK_KP_Add, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_FORWARD);  

  add_slider_binding (binding_set, GDK_KP_Subtract, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_BACKWARD);
  
  
  add_slider_binding (binding_set, GDK_Home, 0,
                      GTK_SCROLL_START);

  add_slider_binding (binding_set, GDK_KP_Home, 0,
                      GTK_SCROLL_START);

  add_slider_binding (binding_set, GDK_End, 0,
                      GTK_SCROLL_END);

  add_slider_binding (binding_set, GDK_KP_End, 0,
                      GTK_SCROLL_END);

  g_type_class_add_private (gobject_class, sizeof (GtkScalePrivate));
}

static void
gtk_scale_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (prop_id)
    {
    case PROP_DIGITS:
      gtk_scale_set_digits (scale, g_value_get_int (value));
      break;
    case PROP_DRAW_VALUE:
      gtk_scale_set_draw_value (scale, g_value_get_boolean (value));
      break;
    case PROP_VALUE_POS:
      gtk_scale_set_value_pos (scale, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (prop_id)
    {
    case PROP_DIGITS:
      g_value_set_int (value, scale->digits);
      break;
    case PROP_DRAW_VALUE:
      g_value_set_boolean (value, scale->draw_value);
      break;
    case PROP_VALUE_POS:
      g_value_set_enum (value, scale->value_pos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_init (GtkScale *scale)
{
  GtkRange *range;

  range = GTK_RANGE (scale);
  
  GTK_WIDGET_SET_FLAGS (scale, GTK_CAN_FOCUS);

  range->slider_size_fixed = TRUE;
  range->has_stepper_a = FALSE;
  range->has_stepper_b = FALSE;
  range->has_stepper_c = FALSE;
  range->has_stepper_d = FALSE;
  
  scale->draw_value = TRUE;
  scale->value_pos = GTK_POS_TOP;
  scale->digits = 1;
  range->round_digits = scale->digits;
}

void
gtk_scale_set_digits (GtkScale *scale,
		      gint      digits)
{
  GtkRange *range;
  
  g_return_if_fail (GTK_IS_SCALE (scale));

  range = GTK_RANGE (scale);
  
  digits = CLAMP (digits, -1, MAX_DIGITS);

  if (scale->digits != digits)
    {
      scale->digits = digits;
      if (scale->draw_value)
	range->round_digits = digits;
      
      _gtk_scale_clear_layout (scale);
      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "digits");
    }
}

gint
gtk_scale_get_digits (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), -1);

  return scale->digits;
}

void
gtk_scale_set_draw_value (GtkScale *scale,
			  gboolean  draw_value)
{
  g_return_if_fail (GTK_IS_SCALE (scale));

  draw_value = draw_value != FALSE;

  if (scale->draw_value != draw_value)
    {
      scale->draw_value = draw_value;
      if (draw_value)
	GTK_RANGE (scale)->round_digits = scale->digits;
      else
	GTK_RANGE (scale)->round_digits = -1;

      _gtk_scale_clear_layout (scale);

      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "draw-value");
    }
}

gboolean
gtk_scale_get_draw_value (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), FALSE);

  return scale->draw_value;
}

void
gtk_scale_set_value_pos (GtkScale        *scale,
			 GtkPositionType  pos)
{
  g_return_if_fail (GTK_IS_SCALE (scale));

  if (scale->value_pos != pos)
    {
      scale->value_pos = pos;

      _gtk_scale_clear_layout (scale);
      if (GTK_WIDGET_VISIBLE (scale) && GTK_WIDGET_MAPPED (scale))
	gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "value-pos");
    }
}

GtkPositionType
gtk_scale_get_value_pos (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), 0);

  return scale->value_pos;
}

static void
gtk_scale_get_range_border (GtkRange  *range,
                            GtkBorder *border)
{
  GtkWidget *widget;
  GtkScale *scale;
  gint w, h;
  
  widget = GTK_WIDGET (range);
  scale = GTK_SCALE (range);

  _gtk_scale_get_value_size (scale, &w, &h);

  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (scale->draw_value)
    {
      gint value_spacing;
      gtk_widget_style_get (widget, "value-spacing", &value_spacing, NULL);

      switch (scale->value_pos)
        {
        case GTK_POS_LEFT:
          border->left += w + value_spacing;
          break;
        case GTK_POS_RIGHT:
          border->right += w + value_spacing;
          break;
        case GTK_POS_TOP:
          border->top += h + value_spacing;
          break;
        case GTK_POS_BOTTOM:
          border->bottom += h + value_spacing;
          break;
        }
    }
}

/* FIXME this could actually be static at the moment. */
void
_gtk_scale_get_value_size (GtkScale *scale,
                           gint     *width,
                           gint     *height)
{
  GtkRange *range;

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (scale->draw_value)
    {
      PangoLayout *layout;
      PangoRectangle logical_rect;
      gchar *txt;
      
      range = GTK_RANGE (scale);

      layout = gtk_widget_create_pango_layout (GTK_WIDGET (scale), NULL);

      txt = _gtk_scale_format_value (scale, range->adjustment->lower);
      pango_layout_set_text (layout, txt, -1);
      g_free (txt);
      
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (width)
	*width = logical_rect.width;
      if (height)
	*height = logical_rect.height;

      txt = _gtk_scale_format_value (scale, range->adjustment->upper);
      pango_layout_set_text (layout, txt, -1);
      g_free (txt);
      
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (width)
	*width = MAX (*width, logical_rect.width);
      if (height)
	*height = MAX (*height, logical_rect.height);

      g_object_unref (layout);
    }
  else
    {
      if (width)
	*width = 0;
      if (height)
	*height = 0;
    }

}

static void
gtk_scale_style_set (GtkWidget *widget,
                     GtkStyle  *previous)
{
  gint slider_length;
  GtkRange *range;

  range = GTK_RANGE (widget);
  
  gtk_widget_style_get (widget,
                        "slider-length", &slider_length,
                        NULL);
  
  range->min_slider_size = slider_length;
  
  _gtk_scale_clear_layout (GTK_SCALE (widget));

  (* GTK_WIDGET_CLASS (gtk_scale_parent_class)->style_set) (widget, previous);
}

static void
gtk_scale_screen_changed (GtkWidget *widget,
                          GdkScreen *old_screen)
{
  _gtk_scale_clear_layout (GTK_SCALE (widget));
}

/**
 * _gtk_scale_format_value:
 * @scale: a #GtkScale
 * @value: adjustment value
 * 
 * Emits "format_value" signal to format the value, if no user
 * signal handlers, falls back to a default format.
 * 
 * Return value: formatted value
 **/
gchar*
_gtk_scale_format_value (GtkScale *scale,
                         gdouble   value)
{
  gchar *fmt = NULL;

  g_signal_emit (scale,
                 signals[FORMAT_VALUE],
                 0,
                 value,
                 &fmt);

  if (fmt)
    return fmt;
  else
    /* insert a LRM, to prevent -20 to come out as 20- in RTL locales */
    return g_strdup_printf ("\342\200\216%0.*f", scale->digits, value);
}

static void
gtk_scale_finalize (GObject *object)
{
  GtkScale *scale;

  g_return_if_fail (GTK_IS_SCALE (object));

  scale = GTK_SCALE (object);

  _gtk_scale_clear_layout (scale);

  G_OBJECT_CLASS (gtk_scale_parent_class)->finalize (object);
}

/**
 * gtk_scale_get_layout:
 * @scale: A #GtkScale
 *
 * Gets the #PangoLayout used to display the scale. The returned object
 * is owned by the scale so does not need to be freed by the caller. 
 *
 * Return value: the #PangoLayout for this scale, or %NULL if the draw_value property
 *    is %FALSE.
 *   
 * Since: 2.4
 **/
PangoLayout *
gtk_scale_get_layout (GtkScale *scale)
{
  GtkScalePrivate *priv = GTK_SCALE_GET_PRIVATE (scale);
  gchar *txt;

  g_return_val_if_fail (GTK_IS_SCALE (scale), NULL);

  if (!priv->layout)
    {
      if (scale->draw_value)
	priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (scale), NULL);
    }

  if (scale->draw_value) 
    {
      txt = _gtk_scale_format_value (scale,
				     GTK_RANGE (scale)->adjustment->value);
      pango_layout_set_text (priv->layout, txt, -1);
      g_free (txt);
    }

  return priv->layout;
}

/**
 * gtk_scale_get_layout_offsets:
 * @scale: a #GtkScale
 * @x: location to store X offset of layout, or %NULL
 * @y: location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the scale will draw the #PangoLayout
 * representing the text in the scale. Remember
 * when using the #PangoLayout function you need to convert to
 * and from pixels using PANGO_PIXELS() or #PANGO_SCALE. 
 *
 * If the draw_value property is %FALSE, the return values are 
 * undefined.
 *
 * Since: 2.4
 **/
void 
gtk_scale_get_layout_offsets (GtkScale *scale,
                              gint     *x,
                              gint     *y)
{
  gint local_x = 0; 
  gint local_y = 0;

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (GTK_SCALE_GET_CLASS (scale)->get_layout_offsets)
    (GTK_SCALE_GET_CLASS (scale)->get_layout_offsets) (scale, &local_x, &local_y);

  if (x)
    *x = local_x;
  
  if (y)
    *y = local_y;
}

void _gtk_scale_clear_layout (GtkScale *scale)
{
  GtkScalePrivate *priv = GTK_SCALE_GET_PRIVATE (scale);

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (priv->layout)
    {
      g_object_unref (priv->layout);
      priv->layout = NULL;
    }
}

#define __GTK_SCALE_C__
#include "gtkaliasdef.c"
