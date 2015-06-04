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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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

#include "gtkscaleprivate.h"
#include "gtkrangeprivate.h"

#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"

#include "a11y/gtkscaleaccessible.h"


/**
 * SECTION:gtkscale
 * @Short_description: A slider widget for selecting a value from a range
 * @Title: GtkScale
 *
 * A GtkScale is a slider control used to select a numeric value. 
 * To use it, you’ll probably want to investigate the methods on
 * its base class, #GtkRange, in addition to the methods for GtkScale itself.
 * To set the value of a scale, you would normally use gtk_range_set_value().
 * To detect changes to the value, you would normally use the
 * #GtkRange::value-changed signal.
 *
 * Note that using the same upper and lower bounds for the #GtkScale (through
 * the #GtkRange methods) will hide the slider itself. This is useful for
 * applications that want to show an undeterminate value on the scale, without
 * changing the layout of the application (such as movie or music players).
 *
 * # GtkScale as GtkBuildable
 *
 * GtkScale supports a custom <marks> element, which can contain multiple
 * <mark> elements. The “value” and “position” attributes have the same
 * meaning as gtk_scale_add_mark() parameters of the same name. If the
 * element is not empty, its content is taken as the markup to show at
 * the mark. It can be translated with the usual ”translatable” and
 * “context” attributes.
 */


#define	MAX_DIGITS	(64)	/* don't change this,
				 * a) you don't need to and
				 * b) you might cause buffer owerflows in
				 *    unrelated code portions otherwise
				 */

typedef struct _GtkScaleMark GtkScaleMark;

struct _GtkScalePrivate
{
  PangoLayout  *layout;

  GSList       *marks;

  gint          digits;

  guint         draw_value : 1;
  guint         value_pos  : 2;
};

struct _GtkScaleMark
{
  gdouble          value;
  gchar           *markup;
  GtkPositionType  position; /* always GTK_POS_TOP or GTK_POS_BOTTOM */
};

enum {
  PROP_0,
  PROP_DIGITS,
  PROP_DRAW_VALUE,
  PROP_HAS_ORIGIN,
  PROP_VALUE_POS
};

enum {
  FORMAT_VALUE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void     gtk_scale_set_property            (GObject        *object,
                                                   guint           prop_id,
                                                   const GValue   *value,
                                                   GParamSpec     *pspec);
static void     gtk_scale_get_property            (GObject        *object,
                                                   guint           prop_id,
                                                   GValue         *value,
                                                   GParamSpec     *pspec);
static void     gtk_scale_get_preferred_width     (GtkWidget      *widget,
                                                   gint           *minimum,
                                                   gint           *natural);
static void     gtk_scale_get_preferred_height    (GtkWidget      *widget,
                                                   gint           *minimum,
                                                   gint           *natural);
static void     gtk_scale_style_updated           (GtkWidget      *widget);
static void     gtk_scale_get_range_border        (GtkRange       *range,
                                                   GtkBorder      *border);
static void     gtk_scale_get_mark_label_size     (GtkScale        *scale,
                                                   GtkPositionType  position,
                                                   gint            *count1,
                                                   gint            *width1,
                                                   gint            *height1,
                                                   gint            *count2,
                                                   gint            *width2,
                                                   gint            *height2);
static void     gtk_scale_finalize                (GObject        *object);
static void     gtk_scale_screen_changed          (GtkWidget      *widget,
                                                   GdkScreen      *old_screen);
static gboolean gtk_scale_draw                    (GtkWidget      *widget,
                                                   cairo_t        *cr);
static void     gtk_scale_real_get_layout_offsets (GtkScale       *scale,
                                                   gint           *x,
                                                   gint           *y);
static void     gtk_scale_buildable_interface_init   (GtkBuildableIface *iface);
static gboolean gtk_scale_buildable_custom_tag_start (GtkBuildable  *buildable,
                                                      GtkBuilder    *builder,
                                                      GObject       *child,
                                                      const gchar   *tagname,
                                                      GMarkupParser *parser,
                                                      gpointer      *data);
static void     gtk_scale_buildable_custom_finished  (GtkBuildable  *buildable,
                                                      GtkBuilder    *builder,
                                                      GObject       *child,
                                                      const gchar   *tagname,
                                                      gpointer       user_data);


G_DEFINE_TYPE_WITH_CODE (GtkScale, gtk_scale, GTK_TYPE_RANGE,
                         G_ADD_PRIVATE (GtkScale)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_scale_buildable_interface_init))

static gint
compare_marks (gconstpointer a, gconstpointer b, gpointer data)
{
  gboolean inverted = GPOINTER_TO_INT (data);
  gint val;
  const GtkScaleMark *ma, *mb;

  val = inverted ? -1 : 1;

  ma = a; mb = b;

  return (ma->value > mb->value) ? val : ((ma->value < mb->value) ? -val : 0);
}

static void
gtk_scale_notify (GObject    *object,
                  GParamSpec *pspec)
{
  if (strcmp (pspec->name, "inverted") == 0)
    {
      GtkScale *scale = GTK_SCALE (object);
      GtkScaleMark *mark;
      GSList *m;
      gint i, n;
      gdouble *values;

      scale->priv->marks = g_slist_sort_with_data (scale->priv->marks,
                                                   compare_marks,
                                                   GINT_TO_POINTER (gtk_range_get_inverted (GTK_RANGE (scale))));

      n = g_slist_length (scale->priv->marks);
      values = g_new (gdouble, n);
      for (m = scale->priv->marks, i = 0; m; m = m->next, i++)
        {
          mark = m->data;
          values[i] = mark->value;
        }

      _gtk_range_set_stop_values (GTK_RANGE (scale), values, n);

      g_free (values);
    }

  if (G_OBJECT_CLASS (gtk_scale_parent_class)->notify)
    G_OBJECT_CLASS (gtk_scale_parent_class)->notify (object, pspec);
}

static void
gtk_scale_update_style (GtkScale *scale)
{
  gint slider_length;
  GtkRange *range;

  range = GTK_RANGE (scale);

  gtk_widget_style_get (GTK_WIDGET (scale),
                        "slider-length", &slider_length,
                        NULL);

  gtk_range_set_min_slider_size (range, slider_length);
  _gtk_scale_clear_layout (scale);
}

#define add_slider_binding(binding_set, keyval, mask, scroll)              \
  gtk_binding_entry_add_signal (binding_set, keyval, mask,                 \
                                I_("move-slider"), 1, \
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
  gobject_class->notify = gtk_scale_notify;
  gobject_class->finalize = gtk_scale_finalize;

  widget_class->style_updated = gtk_scale_style_updated;
  widget_class->screen_changed = gtk_scale_screen_changed;
  widget_class->draw = gtk_scale_draw;
  widget_class->get_preferred_width = gtk_scale_get_preferred_width;
  widget_class->get_preferred_height = gtk_scale_get_preferred_height;

  range_class->get_range_border = gtk_scale_get_range_border;

  class->get_layout_offsets = gtk_scale_real_get_layout_offsets;

  /**
   * GtkScale::format-value:
   * @scale: the object which received the signal
   * @value: the value to format
   *
   * Signal which allows you to change how the scale value is displayed.
   * Connect a signal handler which returns an allocated string representing 
   * @value. That string will then be used to display the scale's value.
   *
   * Here's an example signal handler which displays a value 1.0 as
   * with "-->1.0<--".
   * |[<!-- language="C" -->
   * static gchar*
   * format_value_callback (GtkScale *scale,
   *                        gdouble   value)
   * {
   *   return g_strdup_printf ("-->\%0.*g<--",
   *                           gtk_scale_get_digits (scale), value);
   *  }
   * ]|
   *
   * Returns: allocated string representing @value
   */
  signals[FORMAT_VALUE] =
    g_signal_new (I_("format-value"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkScaleClass, format_value),
                  _gtk_single_string_accumulator, NULL,
                  _gtk_marshal_STRING__DOUBLE,
                  G_TYPE_STRING, 1,
                  G_TYPE_DOUBLE);

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_int ("digits",
                                                     P_("Digits"),
                                                     P_("The number of decimal places that are displayed in the value"),
                                                     -1, MAX_DIGITS, 1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_VALUE,
                                   g_param_spec_boolean ("draw-value",
                                                         P_("Draw Value"),
                                                         P_("Whether the current value is displayed as a string next to the slider"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_HAS_ORIGIN,
                                   g_param_spec_boolean ("has-origin",
                                                         P_("Has Origin"),
                                                         P_("Whether the scale has an origin"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_VALUE_POS,
                                   g_param_spec_enum ("value-pos",
                                                      P_("Value Position"),
                                                      P_("The position in which the current value is displayed"),
                                                      GTK_TYPE_POSITION_TYPE,
                                                      GTK_POS_TOP,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("slider-length",
                                                             P_("Slider Length"),
                                                             P_("Length of scale's slider"),
                                                             0, G_MAXINT, 31,
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

  add_slider_binding (binding_set, GDK_KEY_Left, 0,
                      GTK_SCROLL_STEP_LEFT);

  add_slider_binding (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_KP_Left, 0,
                      GTK_SCROLL_STEP_LEFT);

  add_slider_binding (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_Right, 0,
                      GTK_SCROLL_STEP_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_KP_Right, 0,
                      GTK_SCROLL_STEP_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_Up, 0,
                      GTK_SCROLL_STEP_UP);

  add_slider_binding (binding_set, GDK_KEY_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_KP_Up, 0,
                      GTK_SCROLL_STEP_UP);

  add_slider_binding (binding_set, GDK_KEY_KP_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_Down, 0,
                      GTK_SCROLL_STEP_DOWN);

  add_slider_binding (binding_set, GDK_KEY_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KEY_KP_Down, 0,
                      GTK_SCROLL_STEP_DOWN);

  add_slider_binding (binding_set, GDK_KEY_KP_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN);
   
  add_slider_binding (binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);  

  add_slider_binding (binding_set, GDK_KEY_Page_Up, 0,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Up, 0,
                      GTK_SCROLL_PAGE_UP);
  
  add_slider_binding (binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_Page_Down, 0,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Down, 0,
                      GTK_SCROLL_PAGE_DOWN);

  /* Logical bindings (vs. visual bindings above) */

  add_slider_binding (binding_set, GDK_KEY_plus, 0,
                      GTK_SCROLL_STEP_FORWARD);  

  add_slider_binding (binding_set, GDK_KEY_minus, 0,
                      GTK_SCROLL_STEP_BACKWARD);  

  add_slider_binding (binding_set, GDK_KEY_plus, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_FORWARD);  

  add_slider_binding (binding_set, GDK_KEY_minus, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_BACKWARD);


  add_slider_binding (binding_set, GDK_KEY_KP_Add, 0,
                      GTK_SCROLL_STEP_FORWARD);  

  add_slider_binding (binding_set, GDK_KEY_KP_Subtract, 0,
                      GTK_SCROLL_STEP_BACKWARD);  

  add_slider_binding (binding_set, GDK_KEY_KP_Add, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_FORWARD);  

  add_slider_binding (binding_set, GDK_KEY_KP_Subtract, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_BACKWARD);
  
  
  add_slider_binding (binding_set, GDK_KEY_Home, 0,
                      GTK_SCROLL_START);

  add_slider_binding (binding_set, GDK_KEY_KP_Home, 0,
                      GTK_SCROLL_START);

  add_slider_binding (binding_set, GDK_KEY_End, 0,
                      GTK_SCROLL_END);

  add_slider_binding (binding_set, GDK_KEY_KP_End, 0,
                      GTK_SCROLL_END);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SCALE_ACCESSIBLE);
}

static void
gtk_scale_init (GtkScale *scale)
{
  GtkScalePrivate *priv;
  GtkRange *range = GTK_RANGE (scale);
  GtkStyleContext *context;

  scale->priv = gtk_scale_get_instance_private (scale);
  priv = scale->priv;

  gtk_widget_set_can_focus (GTK_WIDGET (scale), TRUE);

  gtk_range_set_slider_size_fixed (range, TRUE);

  _gtk_range_set_has_origin (range, TRUE);

  priv->draw_value = TRUE;
  priv->value_pos = GTK_POS_TOP;
  priv->digits = 1;
  gtk_range_set_round_digits (range, priv->digits);

  gtk_range_set_flippable (range, TRUE);

  context = gtk_widget_get_style_context (GTK_WIDGET (scale));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SCALE);
  gtk_scale_update_style (scale);
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
    case PROP_HAS_ORIGIN:
      gtk_scale_set_has_origin (scale, g_value_get_boolean (value));
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
  GtkScale *scale = GTK_SCALE (object);
  GtkScalePrivate *priv = scale->priv;

  switch (prop_id)
    {
    case PROP_DIGITS:
      g_value_set_int (value, priv->digits);
      break;
    case PROP_DRAW_VALUE:
      g_value_set_boolean (value, priv->draw_value);
      break;
    case PROP_HAS_ORIGIN:
      g_value_set_boolean (value, gtk_scale_get_has_origin (scale));
      break;
    case PROP_VALUE_POS:
      g_value_set_enum (value, priv->value_pos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_scale_new:
 * @orientation: the scale’s orientation.
 * @adjustment: (allow-none): the #GtkAdjustment which sets the range
 *              of the scale, or %NULL to create a new adjustment.
 *
 * Creates a new #GtkScale.
 *
 * Returns: a new #GtkScale
 *
 * Since: 3.0
 **/
GtkWidget *
gtk_scale_new (GtkOrientation  orientation,
               GtkAdjustment  *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_SCALE,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}

/**
 * gtk_scale_new_with_range:
 * @orientation: the scale’s orientation.
 * @min: minimum value
 * @max: maximum value
 * @step: step increment (tick size) used with keyboard shortcuts
 *
 * Creates a new scale widget with the given orientation that lets the
 * user input a number between @min and @max (including @min and @max)
 * with the increment @step.  @step must be nonzero; it’s the distance
 * the slider moves when using the arrow keys to adjust the scale
 * value.
 *
 * Note that the way in which the precision is derived works best if @step
 * is a power of ten. If the resulting precision is not suitable for your
 * needs, use gtk_scale_set_digits() to correct it.
 *
 * Returns: a new #GtkScale
 *
 * Since: 3.0
 */
GtkWidget *
gtk_scale_new_with_range (GtkOrientation orientation,
                          gdouble        min,
                          gdouble        max,
                          gdouble        step)
{
  GtkAdjustment *adj;
  gint digits;

  g_return_val_if_fail (min < max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  if (fabs (step) >= 1.0 || step == 0.0)
    {
      digits = 0;
    }
  else
    {
      digits = abs ((gint) floor (log10 (fabs (step))));
      if (digits > 5)
        digits = 5;
    }

  return g_object_new (GTK_TYPE_SCALE,
                       "orientation", orientation,
                       "adjustment",  adj,
                       "digits",      digits,
                       NULL);
}

/**
 * gtk_scale_set_digits:
 * @scale: a #GtkScale
 * @digits: the number of decimal places to display,
 *     e.g. use 1 to display 1.0, 2 to display 1.00, etc
 *
 * Sets the number of decimal places that are displayed in the value.
 * Also causes the value of the adjustment to be rounded off to this
 * number of digits, so the retrieved value matches the value the user saw.
 */
void
gtk_scale_set_digits (GtkScale *scale,
		      gint      digits)
{
  GtkScalePrivate *priv;
  GtkRange *range;

  g_return_if_fail (GTK_IS_SCALE (scale));

  priv = scale->priv;
  range = GTK_RANGE (scale);
  
  digits = CLAMP (digits, -1, MAX_DIGITS);

  if (priv->digits != digits)
    {
      priv->digits = digits;
      if (priv->draw_value)
        gtk_range_set_round_digits (range, digits);

      _gtk_scale_clear_layout (scale);
      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "digits");
    }
}

/**
 * gtk_scale_get_digits:
 * @scale: a #GtkScale
 *
 * Gets the number of decimal places that are displayed in the value.
 *
 * Returns: the number of decimal places that are displayed
 */
gint
gtk_scale_get_digits (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), -1);

  return scale->priv->digits;
}

/**
 * gtk_scale_set_draw_value:
 * @scale: a #GtkScale
 * @draw_value: %TRUE to draw the value
 * 
 * Specifies whether the current value is displayed as a string next 
 * to the slider.
 */
void
gtk_scale_set_draw_value (GtkScale *scale,
			  gboolean  draw_value)
{
  GtkScalePrivate *priv;

  g_return_if_fail (GTK_IS_SCALE (scale));

  priv = scale->priv;

  draw_value = draw_value != FALSE;

  if (priv->draw_value != draw_value)
    {
      priv->draw_value = draw_value;
      if (draw_value)
        gtk_range_set_round_digits (GTK_RANGE (scale), priv->digits);
      else
        gtk_range_set_round_digits (GTK_RANGE (scale), -1);

      _gtk_scale_clear_layout (scale);

      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "draw-value");
    }
}

/**
 * gtk_scale_get_draw_value:
 * @scale: a #GtkScale
 *
 * Returns whether the current value is displayed as a string 
 * next to the slider.
 *
 * Returns: whether the current value is displayed as a string
 */
gboolean
gtk_scale_get_draw_value (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), FALSE);

  return scale->priv->draw_value;
}

/**
 * gtk_scale_set_has_origin:
 * @scale: a #GtkScale
 * @has_origin: %TRUE if the scale has an origin
 * 
 * If @has_origin is set to %TRUE (the default),
 * the scale will highlight the part of the scale
 * between the origin (bottom or left side) of the scale
 * and the current value.
 *
 * Since: 3.4
 */
void
gtk_scale_set_has_origin (GtkScale *scale,
                          gboolean  has_origin)
{
  g_return_if_fail (GTK_IS_SCALE (scale));

  has_origin = has_origin != FALSE;

  if (_gtk_range_get_has_origin (GTK_RANGE (scale)) != has_origin)
    {
      _gtk_range_set_has_origin (GTK_RANGE (scale), has_origin);

      gtk_widget_queue_draw (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "has-origin");
    }
}

/**
 * gtk_scale_get_has_origin:
 * @scale: a #GtkScale
 *
 * Returns whether the scale has an origin.
 *
 * Returns: %TRUE if the scale has an origin.
 * 
 * Since: 3.4
 */
gboolean
gtk_scale_get_has_origin (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), FALSE);

  return _gtk_range_get_has_origin (GTK_RANGE (scale));
}

/**
 * gtk_scale_set_value_pos:
 * @scale: a #GtkScale
 * @pos: the position in which the current value is displayed
 * 
 * Sets the position in which the current value is displayed.
 */
void
gtk_scale_set_value_pos (GtkScale        *scale,
			 GtkPositionType  pos)
{
  GtkScalePrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_SCALE (scale));

  priv = scale->priv;

  if (priv->value_pos != pos)
    {
      priv->value_pos = pos;
      widget = GTK_WIDGET (scale);

      _gtk_scale_clear_layout (scale);
      if (gtk_widget_get_visible (widget) && gtk_widget_get_mapped (widget))
	gtk_widget_queue_resize (widget);

      g_object_notify (G_OBJECT (scale), "value-pos");
    }
}

/**
 * gtk_scale_get_value_pos:
 * @scale: a #GtkScale
 *
 * Gets the position in which the current value is displayed.
 *
 * Returns: the position in which the current value is displayed
 */
GtkPositionType
gtk_scale_get_value_pos (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), 0);

  return scale->priv->value_pos;
}

static void
gtk_scale_get_range_border (GtkRange  *range,
                            GtkBorder *border)
{
  GtkScalePrivate *priv;
  GtkWidget *widget;
  GtkScale *scale;
  gint w, h;
  
  widget = GTK_WIDGET (range);
  scale = GTK_SCALE (range);
  priv = scale->priv;

  _gtk_scale_get_value_size (scale, &w, &h);

  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (priv->draw_value)
    {
      gint value_spacing;
      gtk_widget_style_get (widget, "value-spacing", &value_spacing, NULL);

      switch (priv->value_pos)
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

  if (priv->marks)
    {
      gint slider_width;
      gint value_spacing;
      gint n1, w1, h1, n2, w2, h2;
  
      gtk_widget_style_get (widget, 
                            "slider-width", &slider_width,
                            "value-spacing", &value_spacing, 
                            NULL);


      gtk_scale_get_mark_label_size (scale, GTK_POS_TOP, &n1, &w1, &h1, &n2, &w2, &h2);

      if (gtk_orientable_get_orientation (GTK_ORIENTABLE (scale)) == GTK_ORIENTATION_HORIZONTAL)
        {
          if (n1 > 0)
            border->top += h1 + value_spacing + slider_width / 4;
          if (n2 > 0)
            border->bottom += h2 + value_spacing + slider_width / 4;
        }
      else
        {
          if (n1 > 0)
            border->left += w1 + value_spacing + slider_width / 4;
          if (n2 > 0)
            border->right += w2 + value_spacing + slider_width / 4;
        }
    }
}

/* FIXME this could actually be static at the moment. */
void
_gtk_scale_get_value_size (GtkScale *scale,
                           gint     *width,
                           gint     *height)
{
  GtkScalePrivate *priv = scale->priv;
  GtkRange *range;

  if (priv->draw_value)
    {
      GtkAdjustment *adjustment;
      PangoLayout *layout;
      PangoRectangle logical_rect;
      gchar *txt;
      
      range = GTK_RANGE (scale);

      layout = gtk_widget_create_pango_layout (GTK_WIDGET (scale), NULL);
      adjustment = gtk_range_get_adjustment (range);

      txt = _gtk_scale_format_value (scale, gtk_adjustment_get_lower (adjustment));
      pango_layout_set_text (layout, txt, -1);
      g_free (txt);
      
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (width)
	*width = logical_rect.width;
      if (height)
	*height = logical_rect.height;

      txt = _gtk_scale_format_value (scale, gtk_adjustment_get_upper (adjustment));
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
gtk_scale_get_mark_label_size (GtkScale        *scale,
                               GtkPositionType  position,
                               gint            *count1,
                               gint            *width1,
                               gint            *height1,
                               gint            *count2,
                               gint            *width2,
                               gint            *height2)
{
  GtkScalePrivate *priv = scale->priv;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  GSList *m;
  gint w, h;

  *count1 = *count2 = 0;
  *width1 = *width2 = 0;
  *height1 = *height2 = 0;

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (scale), NULL);

  for (m = priv->marks; m; m = m->next)
    {
      GtkScaleMark *mark = m->data;

      if (mark->markup && *mark->markup)
        {
          pango_layout_set_markup (layout, mark->markup, -1);
          pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

          w = logical_rect.width;
          h = logical_rect.height;
        }
      else
        {
          w = 0;
          h = 0;
        }

      if (mark->position == position)
        {
          (*count1)++;
          *width1 = MAX (*width1, w);
          *height1 = MAX (*height1, h);
        }
      else
        {
          (*count2)++;
          *width2 = MAX (*width2, w);
          *height2 = MAX (*height2, h);
        }
    }

  g_object_unref (layout);
}

static void
gtk_scale_style_updated (GtkWidget *widget)
{
  gtk_scale_update_style (GTK_SCALE (widget));

  GTK_WIDGET_CLASS (gtk_scale_parent_class)->style_updated (widget);
}

static void
gtk_scale_screen_changed (GtkWidget *widget,
                          GdkScreen *old_screen)
{
  _gtk_scale_clear_layout (GTK_SCALE (widget));
}

static void
gtk_scale_get_preferred_width (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  GTK_WIDGET_CLASS (gtk_scale_parent_class)->get_preferred_width (widget, minimum, natural);
  
  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_HORIZONTAL)
    {
      gint n1, w1, h1, n2, w2, h2;
      gint slider_length;
      gint w;

      gtk_widget_style_get (widget, "slider-length", &slider_length, NULL);

      gtk_scale_get_mark_label_size (GTK_SCALE (widget), GTK_POS_TOP, &n1, &w1, &h1, &n2, &w2, &h2);

      w1 = (n1 - 1) * w1 + MAX (w1, slider_length);
      w2 = (n2 - 1) * w2 + MAX (w2, slider_length);
      w = MAX (w1, w2);

      *minimum = MAX (*minimum, w);
      *natural = MAX (*natural, w);
    }
}

static void
gtk_scale_get_preferred_height (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  GTK_WIDGET_CLASS (gtk_scale_parent_class)->get_preferred_height (widget, minimum, natural);


  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_VERTICAL)
    {
      gint n1, w1, h1, n2, w2, h2;
      gint slider_length;
      gint h;

      gtk_widget_style_get (widget, "slider-length", &slider_length, NULL);

      gtk_scale_get_mark_label_size (GTK_SCALE (widget), GTK_POS_TOP, &n1, &w1, &h1, &n2, &w2, &h2);
      h1 = (n1 - 1) * h1 + MAX (h1, slider_length);
      h2 = (n2 - 1) * h1 + MAX (h2, slider_length);
      h = MAX (h1, h2);

      *minimum = MAX (*minimum, h);
      *natural = MAX (*natural, h);
    }
}

static gint
find_next_pos (GtkWidget       *widget,
               GSList          *list,
               gint            *marks,
               GtkPositionType  pos)
{
  GtkAllocation allocation;
  GSList *m;
  gint i;

  for (m = list->next, i = 1; m; m = m->next, i++)
    {
      GtkScaleMark *mark = m->data;

      if (mark->position == pos)
        return marks[i];
    }

  gtk_widget_get_allocation (widget, &allocation);
  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_HORIZONTAL)
    return allocation.width;
  else
    return allocation.height;
}

static gboolean
gtk_scale_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkScale *scale = GTK_SCALE (widget);
  GtkScalePrivate *priv = scale->priv;
  GtkRange *range = GTK_RANGE (scale);
  GtkStyleContext *context;
  gint *marks;
  gint slider_width;
  gint value_spacing;
  gint min_sep = 4;

  context = gtk_widget_get_style_context (widget);
  gtk_widget_style_get (widget,
                        "slider-width", &slider_width,
                        "value-spacing", &value_spacing,
                        NULL);

  if (priv->marks)
    {
      GtkOrientation orientation;
      GdkRectangle range_rect;
      gint i;
      gint x1, x2, x3, y1, y2, y3;
      PangoLayout *layout;
      PangoRectangle logical_rect;
      GSList *m;
      gint min_pos_before, min_pos_after;
      gint min_pos, max_pos;

      orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (range));
      _gtk_range_get_stop_positions (range, &marks);

      layout = gtk_widget_create_pango_layout (widget, NULL);
      gtk_range_get_range_rect (range, &range_rect);

      min_pos_before = min_pos_after = 0;

      for (m = priv->marks, i = 0; m; m = m->next, i++)
        {
          GtkScaleMark *mark = m->data;

          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              x1 = marks[i];
              if (mark->position == GTK_POS_TOP)
                {
                  y1 = range_rect.y + slider_width / 4;
                  y2 = range_rect.y;
                  min_pos = min_pos_before;
                  max_pos = find_next_pos (widget, m, marks + i, GTK_POS_TOP) - min_sep;
                }
              else
                {
                  y1 = range_rect.y + range_rect.height - slider_width / 4;
                  y2 = range_rect.y + range_rect.height;
                  min_pos = min_pos_after;
                  max_pos = find_next_pos (widget, m, marks + i, GTK_POS_BOTTOM) - min_sep;
                }

              gtk_style_context_save (context);
              gtk_style_context_add_class (context, GTK_STYLE_CLASS_MARK);

              gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);
              gtk_render_line (context, cr, x1, y1, x1, y2);
              gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SEPARATOR);

              if (mark->markup)
                {
                  pango_layout_set_markup (layout, mark->markup, -1);
                  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

                  x3 = x1 - logical_rect.width / 2;
                  if (x3 < min_pos)
                    x3 = min_pos;
                  if (x3 + logical_rect.width > max_pos)
                        x3 = max_pos - logical_rect.width;
                  if (x3 < 0)
                     x3 = 0;
                  if (mark->position == GTK_POS_TOP)
                    {
                      y3 = y2 - value_spacing - logical_rect.height;
                      min_pos_before = x3 + logical_rect.width + min_sep;
                    }
                  else
                    {
                      y3 = y2 + value_spacing;
                      min_pos_after = x3 + logical_rect.width + min_sep;
                    }

                  gtk_render_layout (context, cr, x3, y3, layout);
                }

              gtk_style_context_restore (context);
            }
          else
            {
              if (mark->position == GTK_POS_TOP)
                {
                  x1 = range_rect.x + slider_width / 4;
                  x2 = range_rect.x;
                  min_pos = min_pos_before;
                  max_pos = find_next_pos (widget, m, marks + i, GTK_POS_TOP) - min_sep;
                }
              else
                {
                  x1 = range_rect.x + range_rect.width - slider_width / 4;
                  x2 = range_rect.x + range_rect.width;
                  min_pos = min_pos_after;
                  max_pos = find_next_pos (widget, m, marks + i, GTK_POS_BOTTOM) - min_sep;
                }
              y1 = marks[i];

              gtk_style_context_save (context);
              gtk_style_context_add_class (context, GTK_STYLE_CLASS_MARK);

              gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);
              gtk_render_line (context, cr, x1, y1, x2, y1);
              gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SEPARATOR);

              if (mark->markup)
                {
                  pango_layout_set_markup (layout, mark->markup, -1);
                  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

                  y3 = y1 - logical_rect.height / 2;
                  if (y3 < min_pos)
                    y3 = min_pos;
                  if (y3 + logical_rect.height > max_pos)
                    y3 = max_pos - logical_rect.height;
                  if (y3 < 0)
                    y3 = 0;
                  if (mark->position == GTK_POS_TOP)
                    {
                      x3 = x2 - value_spacing - logical_rect.width;
                      min_pos_before = y3 + logical_rect.height + min_sep;
                    }
                  else
                    {
                      x3 = x2 + value_spacing;
                      min_pos_after = y3 + logical_rect.height + min_sep;
                    }

                  gtk_render_layout (context, cr, x3, y3, layout);
                }

              gtk_style_context_restore (context);
            }
        }

      g_object_unref (layout);
      g_free (marks);
    }

  GTK_WIDGET_CLASS (gtk_scale_parent_class)->draw (widget, cr);

  if (priv->draw_value)
    {
      GtkAllocation allocation;

      PangoLayout *layout;
      gint x, y;

      layout = gtk_scale_get_layout (scale);
      gtk_scale_get_layout_offsets (scale, &x, &y);
      gtk_widget_get_allocation (widget, &allocation);

      gtk_render_layout (context, cr,
                         x - allocation.x,
                         y - allocation.y,
                         layout);
    }

  return FALSE;
}

static void
gtk_scale_real_get_layout_offsets (GtkScale *scale,
                                   gint     *x,
                                   gint     *y)
{
  GtkScalePrivate *priv = scale->priv;
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (scale);
  GtkRange *range = GTK_RANGE (widget);
  GdkRectangle range_rect;
  PangoLayout *layout = gtk_scale_get_layout (scale);
  PangoRectangle logical_rect;
  gint slider_start, slider_end;
  gint value_spacing;

  if (!layout)
    {
      *x = 0;
      *y = 0;

      return;
    }

  gtk_widget_style_get (widget, "value-spacing", &value_spacing, NULL);

  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  gtk_widget_get_allocation (widget, &allocation);
  gtk_range_get_range_rect (range, &range_rect);
  gtk_range_get_slider_range (range,
                              &slider_start,
                              &slider_end);

  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (range)) == GTK_ORIENTATION_HORIZONTAL)
    {
      switch (priv->value_pos)
        {
        case GTK_POS_LEFT:
          *x = range_rect.x - value_spacing - logical_rect.width;
          *y = range_rect.y + (range_rect.height - logical_rect.height) / 2;
          break;

        case GTK_POS_RIGHT:
          *x = range_rect.x + range_rect.width + value_spacing;
          *y = range_rect.y + (range_rect.height - logical_rect.height) / 2;
          break;

        case GTK_POS_TOP:
          *x = slider_start + (slider_end - slider_start - logical_rect.width) / 2;
          *x = CLAMP (*x, 0, allocation.width - logical_rect.width);
          *y = range_rect.y - logical_rect.height - value_spacing;
          break;

        case GTK_POS_BOTTOM:
          *x = slider_start + (slider_end - slider_start - logical_rect.width) / 2;
          *x = CLAMP (*x, 0, allocation.width - logical_rect.width);
          *y = range_rect.y + range_rect.height + value_spacing;
          break;

        default:
          g_return_if_reached ();
          break;
        }
    }
  else
    {
      switch (priv->value_pos)
        {
        case GTK_POS_LEFT:
          *x = range_rect.x - logical_rect.width - value_spacing;
          *y = slider_start + (slider_end - slider_start - logical_rect.height) / 2;
          *y = CLAMP (*y, 0, allocation.height - logical_rect.height);
          break;

        case GTK_POS_RIGHT:
          *x = range_rect.x + range_rect.width + value_spacing;
          *y = slider_start + (slider_end - slider_start - logical_rect.height) / 2;
          *y = CLAMP (*y, 0, allocation.height - logical_rect.height);
          break;

        case GTK_POS_TOP:
          *x = range_rect.x + (range_rect.width - logical_rect.width) / 2;
          *y = range_rect.y - logical_rect.height - value_spacing;
          break;

        case GTK_POS_BOTTOM:
          *x = range_rect.x + (range_rect.width - logical_rect.width) / 2;
          *y = range_rect.y + range_rect.height + value_spacing;
          break;

        default:
          g_return_if_reached ();
        }
    }

  *x += allocation.x;
  *y += allocation.y;
}

/**
 * _gtk_scale_format_value:
 * @scale: a #GtkScale
 * @value: adjustment value
 * 
 * Emits #GtkScale::format-value signal to format the value, 
 * if no user signal handlers, falls back to a default format.
 * 
 * Returns: formatted value
 */
gchar*
_gtk_scale_format_value (GtkScale *scale,
                         gdouble   value)
{
  GtkScalePrivate *priv = scale->priv;
  gchar *fmt = NULL;

  g_signal_emit (scale,
                 signals[FORMAT_VALUE],
                 0,
                 value,
                 &fmt);

  if (fmt)
    return fmt;
  else
    return g_strdup_printf ("%0.*f", priv->digits, value);
}

static void
gtk_scale_finalize (GObject *object)
{
  GtkScale *scale = GTK_SCALE (object);

  _gtk_scale_clear_layout (scale);
  gtk_scale_clear_marks (scale);

  G_OBJECT_CLASS (gtk_scale_parent_class)->finalize (object);
}

/**
 * gtk_scale_get_layout:
 * @scale: A #GtkScale
 *
 * Gets the #PangoLayout used to display the scale. The returned
 * object is owned by the scale so does not need to be freed by
 * the caller.
 *
 * Returns: (transfer none): the #PangoLayout for this scale,
 *     or %NULL if the #GtkScale:draw-value property is %FALSE.
 *
 * Since: 2.4
 */
PangoLayout *
gtk_scale_get_layout (GtkScale *scale)
{
  GtkScalePrivate *priv;
  gchar *txt;

  g_return_val_if_fail (GTK_IS_SCALE (scale), NULL);

  priv = scale->priv;

  if (!priv->layout)
    {
      if (priv->draw_value)
	priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (scale), NULL);
    }

  if (priv->draw_value)
    {
      txt = _gtk_scale_format_value (scale,
				     gtk_adjustment_get_value (gtk_range_get_adjustment (GTK_RANGE (scale))));
      pango_layout_set_text (priv->layout, txt, -1);
      g_free (txt);
    }

  return priv->layout;
}

/**
 * gtk_scale_get_layout_offsets:
 * @scale: a #GtkScale
 * @x: (out) (allow-none): location to store X offset of layout, or %NULL
 * @y: (out) (allow-none): location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the scale will draw the 
 * #PangoLayout representing the text in the scale. Remember
 * when using the #PangoLayout function you need to convert to
 * and from pixels using PANGO_PIXELS() or #PANGO_SCALE. 
 *
 * If the #GtkScale:draw-value property is %FALSE, the return 
 * values are undefined.
 *
 * Since: 2.4
 */
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

void
_gtk_scale_clear_layout (GtkScale *scale)
{
  GtkScalePrivate *priv = scale->priv;

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (priv->layout)
    {
      g_object_unref (priv->layout);
      priv->layout = NULL;
    }
}

static void
gtk_scale_mark_free (gpointer data)
{
  GtkScaleMark *mark = data;

  g_free (mark->markup);
  g_free (mark);
}

/**
 * gtk_scale_clear_marks:
 * @scale: a #GtkScale
 * 
 * Removes any marks that have been added with gtk_scale_add_mark().
 *
 * Since: 2.16
 */
void
gtk_scale_clear_marks (GtkScale *scale)
{
  GtkScalePrivate *priv;
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_SCALE (scale));

  priv = scale->priv;

  g_slist_free_full (priv->marks, gtk_scale_mark_free);
  priv->marks = NULL;

  context = gtk_widget_get_style_context (GTK_WIDGET (scale));
  gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW);
  gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE);

  _gtk_range_set_stop_values (GTK_RANGE (scale), NULL, 0);

  gtk_widget_queue_resize (GTK_WIDGET (scale));
}

/**
 * gtk_scale_add_mark:
 * @scale: a #GtkScale
 * @value: the value at which the mark is placed, must be between
 *   the lower and upper limits of the scales’ adjustment
 * @position: where to draw the mark. For a horizontal scale, #GTK_POS_TOP
 *   and %GTK_POS_LEFT are drawn above the scale, anything else below.
 *   For a vertical scale, #GTK_POS_LEFT and %GTK_POS_TOP are drawn to
 *   the left of the scale, anything else to the right.
 * @markup: (allow-none): Text to be shown at the mark, using [Pango markup][PangoMarkupFormat], or %NULL
 *
 *
 * Adds a mark at @value.
 *
 * A mark is indicated visually by drawing a tick mark next to the scale,
 * and GTK+ makes it easy for the user to position the scale exactly at the
 * marks value.
 *
 * If @markup is not %NULL, text is shown next to the tick mark.
 *
 * To remove marks from a scale, use gtk_scale_clear_marks().
 *
 * Since: 2.16
 */
void
gtk_scale_add_mark (GtkScale        *scale,
                    gdouble          value,
                    GtkPositionType  position,
                    const gchar     *markup)
{
  GtkScalePrivate *priv;
  GtkScaleMark *mark;
  GSList *m;
  gdouble *values;
  gint n, i;
  GtkStyleContext *context;
  int all_pos;

  g_return_if_fail (GTK_IS_SCALE (scale));

  priv = scale->priv;

  mark = g_new (GtkScaleMark, 1);
  mark->value = value;
  mark->markup = g_strdup (markup);
  if (position == GTK_POS_LEFT ||
      position == GTK_POS_TOP)
    mark->position = GTK_POS_TOP;
  else
    mark->position = GTK_POS_BOTTOM;

  priv->marks = g_slist_insert_sorted_with_data (priv->marks, mark,
                                                 compare_marks,
                                                 GINT_TO_POINTER (gtk_range_get_inverted (GTK_RANGE (scale))));

#define MARKS_ABOVE 1
#define MARKS_BELOW 2

  all_pos = 0;
  n = g_slist_length (priv->marks);
  values = g_new (gdouble, n);
  for (m = priv->marks, i = 0; m; m = m->next, i++)
    {
      mark = m->data;
      values[i] = mark->value;
      if (mark->position == GTK_POS_TOP)
        all_pos |= MARKS_ABOVE;
      else
        all_pos |= MARKS_BELOW;
    }

  _gtk_range_set_stop_values (GTK_RANGE (scale), values, n);

  g_free (values);

  /* Set the style classes for the slider, so it could
   * point to the right direction when marks are present
   */
  context = gtk_widget_get_style_context (GTK_WIDGET (scale));

  if (all_pos & MARKS_ABOVE)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE);
  if (all_pos & MARKS_BELOW)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW);

  gtk_widget_queue_resize (GTK_WIDGET (scale));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_scale_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = gtk_scale_buildable_custom_tag_start;
  iface->custom_finished = gtk_scale_buildable_custom_finished;
}

typedef struct
{
  GtkScale *scale;
  GtkBuilder *builder;
  GSList *marks;
} MarksSubparserData;

typedef struct
{
  gdouble value;
  GtkPositionType position;
  GString *markup;
  gchar *context;
  gboolean translatable;
} MarkData;

static void
mark_data_free (MarkData *data)
{
  g_string_free (data->markup, TRUE);
  g_free (data->context);
  g_slice_free (MarkData, data);
}

static void
marks_start_element (GMarkupParseContext *context,
                     const gchar         *element_name,
                     const gchar        **names,
                     const gchar        **values,
                     gpointer             user_data,
                     GError             **error)
{
  MarksSubparserData *data = (MarksSubparserData*)user_data;

  if (strcmp (element_name, "marks") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "mark") == 0)
    {
      const gchar *value_str;
      gdouble value = 0;
      const gchar *position_str = NULL;
      GtkPositionType position = GTK_POS_BOTTOM;
      const gchar *msg_context = NULL;
      gboolean translatable = FALSE;
      MarkData *mark;

      if (!_gtk_builder_check_parent (data->builder, context, "marks", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "value", &value_str,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "position", &position_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (value_str != NULL)
        {
          GValue gvalue = G_VALUE_INIT;

          if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_DOUBLE, value_str, &gvalue, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }

          value = g_value_get_double (&gvalue);
        }

      if (position_str != NULL)
        {
          GValue gvalue = G_VALUE_INIT;

          if (!gtk_builder_value_from_string_type (data->builder, GTK_TYPE_POSITION_TYPE, position_str, &gvalue, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }

          position = g_value_get_enum (&gvalue);
        }

      mark = g_slice_new (MarkData);
      mark->value = value;
      if (position == GTK_POS_LEFT || position == GTK_POS_TOP)
        mark->position = GTK_POS_TOP;
      else
        mark->position = GTK_POS_BOTTOM;
      mark->markup = g_string_new ("");
      mark->context = g_strdup (msg_context);
      mark->translatable = translatable;

      data->marks = g_slist_prepend (data->marks, mark);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkScale", element_name,
                                        error);
    }
}

static void
marks_text (GMarkupParseContext  *context,
            const gchar          *text,
            gsize                 text_len,
            gpointer              user_data,
            GError              **error)
{
  MarksSubparserData *data = (MarksSubparserData*)user_data;

  if (strcmp (g_markup_parse_context_get_element (context), "mark") == 0)
    {
      MarkData *mark = data->marks->data;

      g_string_append_len (mark->markup, text, text_len);
    }
}

static const GMarkupParser marks_parser =
  {
    marks_start_element,
    NULL,
    marks_text,
  };


static gboolean
gtk_scale_buildable_custom_tag_start (GtkBuildable  *buildable,
                                      GtkBuilder    *builder,
                                      GObject       *child,
                                      const gchar   *tagname,
                                      GMarkupParser *parser,
                                      gpointer      *parser_data)
{
  MarksSubparserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "marks") == 0)
    {
      data = g_slice_new0 (MarksSubparserData);
      data->scale = GTK_SCALE (buildable);
      data->builder = builder;
      data->marks = NULL;

      *parser = marks_parser;
      *parser_data = data;

      return TRUE;
    }

  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                   tagname, parser, parser_data);
}

static void
gtk_scale_buildable_custom_finished (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const gchar  *tagname,
                                     gpointer      user_data)
{
  GtkScale *scale = GTK_SCALE (buildable);
  MarksSubparserData *marks_data;

  if (strcmp (tagname, "marks") == 0)
    {
      GSList *m;
      const gchar *markup;

      marks_data = (MarksSubparserData *)user_data;

      for (m = marks_data->marks; m; m = m->next)
        {
          MarkData *mdata = m->data;

          if (mdata->translatable && mdata->markup->len)
            markup = _gtk_builder_parser_translate (gtk_builder_get_translation_domain (builder),
                                                    mdata->context,
                                                    mdata->markup->str);
          else
            markup = mdata->markup->str;

          gtk_scale_add_mark (scale, mdata->value, mdata->position, markup);

          mark_data_free (mdata);
        }

      g_slist_free (marks_data->marks);
      g_slice_free (MarksSubparserData, marks_data);
    }
  else
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
					       tagname, user_data);
    }

}
