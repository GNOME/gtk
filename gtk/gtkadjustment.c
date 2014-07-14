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
#include "gtkadjustment.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkadjustment
 * @Short_description: A representation of an adjustable bounded value
 * @Title: GtkAdjustment
 *
 * The #GtkAdjustment object represents a value which has an associated lower
 * and upper bound, together with step and page increments, and a page size.
 * It is used within several GTK+ widgets, including
 * #GtkSpinButton, #GtkViewport, and #GtkRange (which is a base class for
 * #GtkHScrollbar, #GtkVScrollbar, #GtkHScale, and #GtkVScale).
 *
 * The #GtkAdjustment object does not update the value itself. Instead
 * it is left up to the owner of the #GtkAdjustment to control the value.
 *
 * The owner of the #GtkAdjustment typically calls the
 * gtk_adjustment_value_changed() and gtk_adjustment_changed() functions
 * after changing the value and its bounds. This results in the emission of the
 * #GtkAdjustment::value_changed or #GtkAdjustment::changed signal respectively.
 */


struct _GtkAdjustmentPrivate {
  gdouble lower;
  gdouble upper;
  gdouble value;
  gdouble step_increment;
  gdouble page_increment;
  gdouble page_size;

  gdouble source;
  gdouble target;

  guint duration;
  gint64 start_time;
  gint64 end_time;
  guint tick_id;
  GdkFrameClock *clock;
};

enum
{
  PROP_0,
  PROP_VALUE,
  PROP_LOWER,
  PROP_UPPER,
  PROP_STEP_INCREMENT,
  PROP_PAGE_INCREMENT,
  PROP_PAGE_SIZE
};

enum
{
  CHANGED,
  VALUE_CHANGED,
  LAST_SIGNAL
};


static void gtk_adjustment_get_property                (GObject      *object,
                                                        guint         prop_id,
                                                        GValue       *value,
                                                        GParamSpec   *pspec);
static void gtk_adjustment_set_property                (GObject      *object,
                                                        guint         prop_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void gtk_adjustment_dispatch_properties_changed (GObject      *object,
                                                        guint         n_pspecs,
                                                        GParamSpec  **pspecs);

static guint adjustment_signals[LAST_SIGNAL] = { 0 };

static guint64 adjustment_changed_stamp = 0; /* protected by global gdk lock */

G_DEFINE_TYPE_WITH_PRIVATE (GtkAdjustment, gtk_adjustment, G_TYPE_INITIALLY_UNOWNED)

static void
gtk_adjustment_finalize (GObject *object)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  GtkAdjustmentPrivate *priv = adjustment->priv;

  if (priv->tick_id)
    g_signal_handler_disconnect (priv->clock, priv->tick_id);
  if (priv->clock)
    g_object_unref (priv->clock);

  G_OBJECT_CLASS (gtk_adjustment_parent_class)->finalize (object);
}

static void
gtk_adjustment_class_init (GtkAdjustmentClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize                    = gtk_adjustment_finalize;
  gobject_class->set_property                = gtk_adjustment_set_property;
  gobject_class->get_property                = gtk_adjustment_get_property;
  gobject_class->dispatch_properties_changed = gtk_adjustment_dispatch_properties_changed;

  class->changed = NULL;
  class->value_changed = NULL;

  /**
   * GtkAdjustment:value:
   * 
   * The value of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
							P_("Value"),
							P_("The value of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:lower:
   * 
   * The minimum value of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LOWER,
                                   g_param_spec_double ("lower",
							P_("Minimum Value"),
							P_("The minimum value of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:upper:
   * 
   * The maximum value of the adjustment. 
   * Note that values will be restricted by 
   * `upper - page-size` if the page-size 
   * property is nonzero.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_UPPER,
                                   g_param_spec_double ("upper",
							P_("Maximum Value"),
							P_("The maximum value of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:step-increment:
   * 
   * The step increment of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STEP_INCREMENT,
                                   g_param_spec_double ("step-increment",
							P_("Step Increment"),
							P_("The step increment of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:page-increment:
   * 
   * The page increment of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PAGE_INCREMENT,
                                   g_param_spec_double ("page-increment",
							P_("Page Increment"),
							P_("The page increment of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:page-size:
   * 
   * The page size of the adjustment. 
   * Note that the page-size is irrelevant and should be set to zero
   * if the adjustment is used for a simple scalar value, e.g. in a 
   * #GtkSpinButton.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PAGE_SIZE,
                                   g_param_spec_double ("page-size",
							P_("Page Size"),
							P_("The page size of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));

  /**
   * GtkAdjustment::changed:
   * @adjustment: the object which received the signal.
   *
   * Emitted when one or more of the #GtkAdjustment properties have been
   * changed, other than the #GtkAdjustment:value property.
   */
  adjustment_signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkAdjustment::value-changed:
   * @adjustment: the object which received the signal.
   *
   * Emitted when the #GtkAdjustment:value property has been changed.
   */
  adjustment_signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, value_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_adjustment_init (GtkAdjustment *adjustment)
{
  adjustment->priv = gtk_adjustment_get_instance_private (adjustment);
}

static void
gtk_adjustment_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  GtkAdjustmentPrivate *priv = adjustment->priv;

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_double (value, priv->value);
      break;
    case PROP_LOWER:
      g_value_set_double (value, priv->lower);
      break;
    case PROP_UPPER:
      g_value_set_double (value, priv->upper);
      break;
    case PROP_STEP_INCREMENT:
      g_value_set_double (value, priv->step_increment);
      break;
    case PROP_PAGE_INCREMENT:
      g_value_set_double (value, priv->page_increment);
      break;
    case PROP_PAGE_SIZE:
      g_value_set_double (value, priv->page_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_adjustment_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  gdouble double_value = g_value_get_double (value);
  GtkAdjustmentPrivate *priv = adjustment->priv;

  switch (prop_id)
    {
    case PROP_VALUE:
      gtk_adjustment_set_value (adjustment, double_value);
      break;
    case PROP_LOWER:
      priv->lower = double_value;
      break;
    case PROP_UPPER:
      priv->upper = double_value;
      break;
    case PROP_STEP_INCREMENT:
      priv->step_increment = double_value;
      break;
    case PROP_PAGE_INCREMENT:
      priv->page_increment = double_value;
      break;
    case PROP_PAGE_SIZE:
      priv->page_size = double_value;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_adjustment_dispatch_properties_changed (GObject     *object,
                                            guint        n_pspecs,
                                            GParamSpec **pspecs)
{
  gboolean changed = FALSE;
  gint i;

  G_OBJECT_CLASS (gtk_adjustment_parent_class)->dispatch_properties_changed (object, n_pspecs, pspecs);

  for (i = 0; i < n_pspecs; i++)
    switch (pspecs[i]->param_id)
      {
      case PROP_LOWER:
      case PROP_UPPER:
      case PROP_STEP_INCREMENT:
      case PROP_PAGE_INCREMENT:
      case PROP_PAGE_SIZE:
        changed = TRUE;
        break;
      default:
        break;
      }

  if (changed)
    {
      adjustment_changed_stamp++;
      gtk_adjustment_changed (GTK_ADJUSTMENT (object));
    }
}

/**
 * gtk_adjustment_new:
 * @value: the initial value.
 * @lower: the minimum value.
 * @upper: the maximum value.
 * @step_increment: the step increment.
 * @page_increment: the page increment.
 * @page_size: the page size.
 *
 * Creates a new #GtkAdjustment.
 *
 * Returns: a new #GtkAdjustment.
 */
GtkAdjustment *
gtk_adjustment_new (gdouble value,
		    gdouble lower,
		    gdouble upper,
		    gdouble step_increment,
		    gdouble page_increment,
		    gdouble page_size)
{
  return g_object_new (GTK_TYPE_ADJUSTMENT,
		       "lower", lower,
		       "upper", upper,
		       "step-increment", step_increment,
		       "page-increment", page_increment,
		       "page-size", page_size,
		       "value", value,
		       NULL);
}

/**
 * gtk_adjustment_get_value:
 * @adjustment: a #GtkAdjustment
 *
 * Gets the current value of the adjustment. See
 * gtk_adjustment_set_value ().
 *
 * Returns: The current value of the adjustment.
 **/
gdouble
gtk_adjustment_get_value (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return adjustment->priv->value;
}

gdouble
gtk_adjustment_get_target_value (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  if (adjustment->priv->tick_id)
    return adjustment->priv->target;
  else
    return adjustment->priv->value;
}

static void
adjustment_set_value (GtkAdjustment *adjustment,
                      gdouble        value)
{
  if (adjustment->priv->value != value)
    {
      adjustment->priv->value = value;
      gtk_adjustment_value_changed (adjustment);
    }
}

static void gtk_adjustment_on_frame_clock_update (GdkFrameClock *clock, 
                                                  GtkAdjustment *adjustment);

static void
gtk_adjustment_begin_updating (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = adjustment->priv;

  if (priv->tick_id == 0)
    {
      priv->tick_id = g_signal_connect (priv->clock, "update",
                                        G_CALLBACK (gtk_adjustment_on_frame_clock_update), adjustment);
      gdk_frame_clock_begin_updating (priv->clock);
    }
}

static void
gtk_adjustment_end_updating (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = adjustment->priv;

  if (priv->tick_id != 0)
    {
      g_signal_handler_disconnect (priv->clock, priv->tick_id);
      priv->tick_id = 0;
      gdk_frame_clock_end_updating (priv->clock);
    }
}

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static gdouble
ease_out_cubic (gdouble t)
{
  gdouble p = t - 1;

  return p * p * p + 1;
}

static void
gtk_adjustment_on_frame_clock_update (GdkFrameClock *clock, 
                                      GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv = adjustment->priv;
  gint64 now;

  now = gdk_frame_clock_get_frame_time (clock);

  if (now < priv->end_time)
    {
      gdouble t;

      t = (now - priv->start_time) / (gdouble) (priv->end_time - priv->start_time);
      t = ease_out_cubic (t);
      adjustment_set_value (adjustment, priv->source + t * (priv->target - priv->source));
    }
  else
    {
      adjustment_set_value (adjustment, priv->target);
      gtk_adjustment_end_updating (adjustment);
    }
}

static void
gtk_adjustment_set_value_internal (GtkAdjustment *adjustment,
                                   gdouble        value,
                                   gboolean       animate)
{
  GtkAdjustmentPrivate *priv = adjustment->priv;

  /* don't use CLAMP() so we don't end up below lower if upper - page_size
   * is smaller than lower
   */
  value = MIN (value, priv->upper - priv->page_size);
  value = MAX (value, priv->lower);

  if (animate && priv->duration != 0 && priv->clock != NULL)
    {
      if (priv->target == value)
        return;

      priv->source = priv->value;
      priv->target = value;
      priv->start_time = gdk_frame_clock_get_frame_time (priv->clock);
      priv->end_time = priv->start_time + 1000 * priv->duration;
      gtk_adjustment_begin_updating (adjustment);
    }
  else
    {
      gtk_adjustment_end_updating (adjustment);
      adjustment_set_value (adjustment, value);
    }
}

/**
 * gtk_adjustment_set_value:
 * @adjustment: a #GtkAdjustment.
 * @value: the new value.
 *
 * Sets the #GtkAdjustment value. The value is clamped to lie between
 * #GtkAdjustment:lower and #GtkAdjustment:upper.
 *
 * Note that for adjustments which are used in a #GtkScrollbar, the effective
 * range of allowed values goes from #GtkAdjustment:lower to
 * #GtkAdjustment:upper - #GtkAdjustment:page_size.
 */
void
gtk_adjustment_set_value (GtkAdjustment *adjustment,
			  gdouble        value)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  gtk_adjustment_set_value_internal (adjustment, value, FALSE);
}

void
gtk_adjustment_animate_to_value (GtkAdjustment *adjustment,
			         gdouble        value)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  gtk_adjustment_set_value_internal (adjustment, value, TRUE);
}

/**
 * gtk_adjustment_get_lower:
 * @adjustment: a #GtkAdjustment
 *
 * Retrieves the minimum value of the adjustment.
 *
 * Returns: The current minimum value of the adjustment.
 *
 * Since: 2.14
 **/
gdouble
gtk_adjustment_get_lower (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return adjustment->priv->lower;
}

/**
 * gtk_adjustment_set_lower:
 * @adjustment: a #GtkAdjustment
 * @lower: the new minimum value
 *
 * Sets the minimum value of the adjustment.
 *
 * When setting multiple adjustment properties via their individual
 * setters, multiple #GtkAdjustment::changed signals will be emitted. However, since
 * the emission of the #GtkAdjustment::changed signal is tied to the emission of the
 * #GObject::notify signals of the changed properties, it’s possible
 * to compress the #GtkAdjustment::changed signals into one by calling
 * g_object_freeze_notify() and g_object_thaw_notify() around the
 * calls to the individual setters.
 *
 * Alternatively, using a single g_object_set() for all the properties
 * to change, or using gtk_adjustment_configure() has the same effect
 * of compressing #GtkAdjustment::changed emissions.
 *
 * Since: 2.14
 **/
void
gtk_adjustment_set_lower (GtkAdjustment *adjustment,
                          gdouble        lower)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (lower != adjustment->priv->lower)
    g_object_set (adjustment, "lower", lower, NULL);
}

/**
 * gtk_adjustment_get_upper:
 * @adjustment: a #GtkAdjustment
 *
 * Retrieves the maximum value of the adjustment.
 *
 * Returns: The current maximum value of the adjustment.
 *
 * Since: 2.14
 **/
gdouble
gtk_adjustment_get_upper (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return adjustment->priv->upper;
}

/**
 * gtk_adjustment_set_upper:
 * @adjustment: a #GtkAdjustment
 * @upper: the new maximum value
 *
 * Sets the maximum value of the adjustment.
 *
 * Note that values will be restricted by
 * `upper - page-size` if the page-size
 * property is nonzero.
 *
 * See gtk_adjustment_set_lower() about how to compress multiple
 * emissions of the #GtkAdjustment::changed signal when setting multiple adjustment
 * properties.
 *
 * Since: 2.14
 **/
void
gtk_adjustment_set_upper (GtkAdjustment *adjustment,
                          gdouble        upper)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (upper != adjustment->priv->upper)
    g_object_set (adjustment, "upper", upper, NULL);
}

/**
 * gtk_adjustment_get_step_increment:
 * @adjustment: a #GtkAdjustment
 *
 * Retrieves the step increment of the adjustment.
 *
 * Returns: The current step increment of the adjustment.
 *
 * Since: 2.14
 **/
gdouble
gtk_adjustment_get_step_increment (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return adjustment->priv->step_increment;
}

/**
 * gtk_adjustment_set_step_increment:
 * @adjustment: a #GtkAdjustment
 * @step_increment: the new step increment
 *
 * Sets the step increment of the adjustment.
 *
 * See gtk_adjustment_set_lower() about how to compress multiple
 * emissions of the #GtkAdjustment::changed signal when setting multiple adjustment
 * properties.
 *
 * Since: 2.14
 **/
void
gtk_adjustment_set_step_increment (GtkAdjustment *adjustment,
                                   gdouble        step_increment)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (step_increment != adjustment->priv->step_increment)
    g_object_set (adjustment, "step-increment", step_increment, NULL);
}

/**
 * gtk_adjustment_get_page_increment:
 * @adjustment: a #GtkAdjustment
 *
 * Retrieves the page increment of the adjustment.
 *
 * Returns: The current page increment of the adjustment.
 *
 * Since: 2.14
 **/
gdouble
gtk_adjustment_get_page_increment (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return adjustment->priv->page_increment;
}

/**
 * gtk_adjustment_set_page_increment:
 * @adjustment: a #GtkAdjustment
 * @page_increment: the new page increment
 *
 * Sets the page increment of the adjustment.
 *
 * See gtk_adjustment_set_lower() about how to compress multiple
 * emissions of the #GtkAdjustment::changed signal when setting multiple adjustment
 * properties.
 *
 * Since: 2.14
 **/
void
gtk_adjustment_set_page_increment (GtkAdjustment *adjustment,
                                   gdouble        page_increment)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (page_increment != adjustment->priv->page_increment)
    g_object_set (adjustment, "page-increment", page_increment, NULL);
}

/**
 * gtk_adjustment_get_page_size:
 * @adjustment: a #GtkAdjustment
 *
 * Retrieves the page size of the adjustment.
 *
 * Returns: The current page size of the adjustment.
 *
 * Since: 2.14
 **/
gdouble
gtk_adjustment_get_page_size (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.0);

  return adjustment->priv->page_size;
}

/**
 * gtk_adjustment_set_page_size:
 * @adjustment: a #GtkAdjustment
 * @page_size: the new page size
 *
 * Sets the page size of the adjustment.
 *
 * See gtk_adjustment_set_lower() about how to compress multiple
 * emissions of the GtkAdjustment::changed signal when setting multiple adjustment
 * properties.
 *
 * Since: 2.14
 **/
void
gtk_adjustment_set_page_size (GtkAdjustment *adjustment,
                              gdouble        page_size)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (page_size != adjustment->priv->page_size)
    g_object_set (adjustment, "page-size", page_size, NULL);
}

/**
 * gtk_adjustment_configure:
 * @adjustment: a #GtkAdjustment
 * @value: the new value
 * @lower: the new minimum value
 * @upper: the new maximum value
 * @step_increment: the new step increment
 * @page_increment: the new page increment
 * @page_size: the new page size
 *
 * Sets all properties of the adjustment at once.
 *
 * Use this function to avoid multiple emissions of the #GtkAdjustment::changed
 * signal. See gtk_adjustment_set_lower() for an alternative way
 * of compressing multiple emissions of #GtkAdjustment::changed into one.
 *
 * Since: 2.14
 **/
void
gtk_adjustment_configure (GtkAdjustment *adjustment,
                          gdouble        value,
                          gdouble        lower,
                          gdouble        upper,
                          gdouble        step_increment,
                          gdouble        page_increment,
                          gdouble        page_size)
{
  GtkAdjustmentPrivate *priv;
  gboolean value_changed = FALSE;
  guint64 old_stamp = adjustment_changed_stamp;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  priv = adjustment->priv;

  g_object_freeze_notify (G_OBJECT (adjustment));

  g_object_set (adjustment,
                "lower", lower,
                "upper", upper,
                "step-increment", step_increment,
                "page-increment", page_increment,
                "page-size", page_size,
                NULL);

  /* don't use CLAMP() so we don't end up below lower if upper - page_size
   * is smaller than lower
   */
  value = MIN (value, upper - page_size);
  value = MAX (value, lower);

  if (value != priv->value)
    {
      /* set value manually to make sure "changed" is emitted with the
       * new value in place and is emitted before "value-changed"
       */
      priv->value = value;
      value_changed = TRUE;
    }

  g_object_thaw_notify (G_OBJECT (adjustment));

  if (old_stamp == adjustment_changed_stamp)
    gtk_adjustment_changed (adjustment); /* force emission before ::value-changed */

  if (value_changed)
    gtk_adjustment_value_changed (adjustment);
}

/**
 * gtk_adjustment_changed:
 * @adjustment: a #GtkAdjustment
 *
 * Emits a #GtkAdjustment::changed signal from the #GtkAdjustment.
 * This is typically called by the owner of the #GtkAdjustment after it has
 * changed any of the #GtkAdjustment properties other than the value.
 */
void
gtk_adjustment_changed (GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  g_signal_emit (adjustment, adjustment_signals[CHANGED], 0);
}

/**
 * gtk_adjustment_value_changed:
 * @adjustment: a #GtkAdjustment
 *
 * Emits a #GtkAdjustment::value_changed signal from the #GtkAdjustment.
 * This is typically called by the owner of the #GtkAdjustment after it has
 * changed the #GtkAdjustment:value property.
 */
void
gtk_adjustment_value_changed (GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  g_signal_emit (adjustment, adjustment_signals[VALUE_CHANGED], 0);
  g_object_notify (G_OBJECT (adjustment), "value");
}

/**
 * gtk_adjustment_clamp_page:
 * @adjustment: a #GtkAdjustment.
 * @lower: the lower value.
 * @upper: the upper value.
 *
 * Updates the #GtkAdjustment:value property to ensure that the range
 * between @lower and @upper is in the current page (i.e. between
 * #GtkAdjustment:value and #GtkAdjustment:value + #GtkAdjustment:page_size).
 * If the range is larger than the page size, then only the start of it will
 * be in the current page.
 * A #GtkAdjustment::changed signal will be emitted if the value is changed.
 */
void
gtk_adjustment_clamp_page (GtkAdjustment *adjustment,
			   gdouble        lower,
			   gdouble        upper)
{
  GtkAdjustmentPrivate *priv;
  gboolean need_emission;

  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  priv = adjustment->priv;

  lower = CLAMP (lower, priv->lower, priv->upper);
  upper = CLAMP (upper, priv->lower, priv->upper);

  need_emission = FALSE;

  if (priv->value + priv->page_size < upper)
    {
      priv->value = upper - priv->page_size;
      need_emission = TRUE;
    }
  if (priv->value > lower)
    {
      priv->value = lower;
      need_emission = TRUE;
    }

  if (need_emission)
    gtk_adjustment_value_changed (adjustment);
}

/**
 * gtk_adjustment_get_minimum_increment:
 * @adjustment: a #GtkAdjustment
 *
 * Gets the smaller of step increment and page increment.
 *
 * Returns: the minimum increment of @adjustment
 *
 * Since: 3.2
 */
gdouble
gtk_adjustment_get_minimum_increment (GtkAdjustment *adjustment)
{
  GtkAdjustmentPrivate *priv;
  gdouble minimum_increment;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0);

  priv = adjustment->priv;

    if (priv->step_increment != 0 && priv->page_increment != 0)
    {
      if (ABS (priv->step_increment) < ABS (priv->page_increment))
        minimum_increment = priv->step_increment;
      else
        minimum_increment = priv->page_increment;
    }
  else if (priv->step_increment == 0 && priv->page_increment == 0)
    {
      minimum_increment = 0;
    }
  else if (priv->step_increment == 0)
    {
      minimum_increment = priv->page_increment;
    }
  else
    {
      minimum_increment = priv->step_increment;
    }

  return minimum_increment;
}

void
gtk_adjustment_enable_animation (GtkAdjustment *adjustment,
                                 GdkFrameClock *clock,
                                 guint          duration)
{
  GtkAdjustmentPrivate *priv = adjustment->priv;

  if (priv->clock != clock)
    {
      if (priv->tick_id)
        {
          adjustment_set_value (adjustment, priv->target);

          g_signal_handler_disconnect (priv->clock, priv->tick_id);
          priv->tick_id = 0;
          gdk_frame_clock_end_updating (priv->clock);
        }

      if (priv->clock)
        g_object_unref (priv->clock);

      priv->clock = clock;

      if (priv->clock)
        g_object_ref (priv->clock);
    }

  priv->duration = duration; 
}

guint
gtk_adjustment_get_animation_duration (GtkAdjustment *adjustment)
{
  return adjustment->priv->duration;
}
