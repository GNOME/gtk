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

#include <stdio.h>
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkrange.h"
#include "gtksignal.h"
#include "gtkintl.h"

#define SCROLL_INITIAL_DELAY 250  /* must hold button this long before ... */
#define SCROLL_LATER_DELAY   100  /* ... it starts repeating at this rate  */
#define UPDATE_DELAY         300  /* Delay for queued update */

enum {
  PROP_0,
  PROP_UPDATE_POLICY,
  PROP_ADJUSTMENT,
  PROP_INVERTED
};

enum {
  VALUE_CHANGED,
  MOVE_SLIDER,
  LAST_SIGNAL
};

typedef enum {
  MOUSE_OUTSIDE,
  MOUSE_STEPPER_A,
  MOUSE_STEPPER_B,
  MOUSE_STEPPER_C,
  MOUSE_STEPPER_D,
  MOUSE_TROUGH,
  MOUSE_SLIDER,
  MOUSE_WIDGET /* inside widget but not in any of the above GUI elements */
} MouseLocation;

struct _GtkRangeLayout
{
  /* These are in widget->window coordinates */
  GdkRectangle stepper_a;
  GdkRectangle stepper_b;
  GdkRectangle stepper_c;
  GdkRectangle stepper_d;
  /* The trough rectangle is the area the thumb can slide in, not the
   * entire range_rect
   */
  GdkRectangle trough;
  GdkRectangle slider;

  /* Layout-related state */
  
  MouseLocation mouse_location;
  /* last mouse coords we got, or -1 if mouse is outside the range */
  gint mouse_x;
  gint mouse_y;
  /* "grabbed" mouse location, OUTSIDE for no grab */
  MouseLocation grab_location;
  gint grab_button; /* 0 if none */
};


static void gtk_range_class_init     (GtkRangeClass    *klass);
static void gtk_range_init           (GtkRange         *range);
static void gtk_range_set_property   (GObject          *object,
                                      guint             prop_id,
                                      const GValue     *value,
                                      GParamSpec       *pspec);
static void gtk_range_get_property   (GObject          *object,
                                      guint             prop_id,
                                      GValue           *value,
                                      GParamSpec       *pspec);
static void gtk_range_destroy        (GtkObject        *object);
static void gtk_range_finalize       (GObject          *object);
static void gtk_range_size_request   (GtkWidget        *widget,
                                      GtkRequisition   *requisition);
static void gtk_range_size_allocate  (GtkWidget        *widget,
                                      GtkAllocation    *allocation);
static void gtk_range_realize        (GtkWidget        *widget);
static void gtk_range_unrealize      (GtkWidget        *widget);
static gint gtk_range_expose         (GtkWidget        *widget,
                                      GdkEventExpose   *event);
static gint gtk_range_button_press   (GtkWidget        *widget,
                                      GdkEventButton   *event);
static gint gtk_range_button_release (GtkWidget        *widget,
                                      GdkEventButton   *event);
static gint gtk_range_motion_notify  (GtkWidget        *widget,
                                      GdkEventMotion   *event);
static gint gtk_range_enter_notify   (GtkWidget        *widget,
                                      GdkEventCrossing *event);
static gint gtk_range_leave_notify   (GtkWidget        *widget,
                                      GdkEventCrossing *event);
static gint gtk_range_scroll_event   (GtkWidget        *widget,
                                      GdkEventScroll   *event);
static void gtk_range_style_set      (GtkWidget        *widget,
                                      GtkStyle         *previous_style);
static void update_slider_position   (GtkRange	       *range,
				      gint              mouse_x,
				      gint              mouse_y);


/* Range methods */

static void gtk_range_move_slider              (GtkRange         *range,
                                                GtkScrollType     scroll);

/* Internals */
static void          gtk_range_scroll                   (GtkRange      *range,
                                                         GtkScrollType  scroll);
static gboolean      gtk_range_update_mouse_location    (GtkRange      *range);
static void          gtk_range_calc_layout              (GtkRange      *range,
							 gdouble	adjustment_value);
static void          gtk_range_get_props                (GtkRange      *range,
                                                         gint          *slider_width,
                                                         gint          *stepper_size,
                                                         gint          *trough_border,
                                                         gint          *stepper_spacing);
static void          gtk_range_calc_request             (GtkRange      *range,
                                                         gint           slider_width,
                                                         gint           stepper_size,
                                                         gint           trough_border,
                                                         gint           stepper_spacing,
                                                         GdkRectangle  *range_rect,
                                                         GtkBorder     *border,
                                                         gint          *n_steppers_p,
                                                         gint          *slider_length_p);
static void          gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_adjustment_changed       (GtkAdjustment *adjustment,
                                                         gpointer       data);
static void          gtk_range_add_step_timer           (GtkRange      *range,
                                                         GtkScrollType  step);
static void          gtk_range_remove_step_timer        (GtkRange      *range);
static void          gtk_range_reset_update_timer       (GtkRange      *range);
static void          gtk_range_remove_update_timer      (GtkRange      *range);
static GdkRectangle* get_area                           (GtkRange      *range,
                                                         MouseLocation  location);
static void          gtk_range_internal_set_value       (GtkRange      *range,
                                                         gdouble        value);
static void          gtk_range_update_value             (GtkRange      *range);


static GtkWidgetClass *parent_class = NULL;
static guint signals[LAST_SIGNAL];


GtkType
gtk_range_get_type (void)
{
  static GtkType range_type = 0;

  if (!range_type)
    {
      static const GtkTypeInfo range_info =
      {
	"GtkRange",
	sizeof (GtkRange),
	sizeof (GtkRangeClass),
	(GtkClassInitFunc) gtk_range_class_init,
	(GtkObjectInitFunc) gtk_range_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      range_type = gtk_type_unique (GTK_TYPE_WIDGET, &range_info);
    }

  return range_type;
}

static void
gtk_range_class_init (GtkRangeClass *class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = g_type_class_peek_parent (class);

  gobject_class->set_property = gtk_range_set_property;
  gobject_class->get_property = gtk_range_get_property;
  gobject_class->finalize = gtk_range_finalize;
  object_class->destroy = gtk_range_destroy;

  widget_class->size_request = gtk_range_size_request;
  widget_class->size_allocate = gtk_range_size_allocate;
  widget_class->realize = gtk_range_realize;
  widget_class->unrealize = gtk_range_unrealize;  
  widget_class->expose_event = gtk_range_expose;
  widget_class->button_press_event = gtk_range_button_press;
  widget_class->button_release_event = gtk_range_button_release;
  widget_class->motion_notify_event = gtk_range_motion_notify;
  widget_class->scroll_event = gtk_range_scroll_event;
  widget_class->enter_notify_event = gtk_range_enter_notify;
  widget_class->leave_notify_event = gtk_range_leave_notify;
  widget_class->style_set = gtk_range_style_set;

  class->move_slider = gtk_range_move_slider;

  class->slider_detail = "slider";
  class->stepper_detail = "stepper";

  signals[VALUE_CHANGED] =
    g_signal_new ("value_changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRangeClass, value_changed),
                  NULL, NULL,
                  gtk_marshal_NONE__NONE,
                  G_TYPE_NONE, 0);
  
  signals[MOVE_SLIDER] =
    g_signal_new ("move_slider",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkRangeClass, move_slider),
                  NULL, NULL,
                  gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);
  
  
  g_object_class_install_property (gobject_class,
                                   PROP_UPDATE_POLICY,
                                   g_param_spec_enum ("update_policy",
						      _("Update policy"),
						      _("How the range should be updated on the screen"),
						      GTK_TYPE_UPDATE_TYPE,
						      GTK_UPDATE_CONTINUOUS,
						      G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
							_("Adjustment"),
							_("The GtkAdjustment that contains the current value of this range object"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_INVERTED,
                                   g_param_spec_boolean ("inverted",
							_("Inverted"),
							_("Invert direction slider moves to increase range value"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("slider_width",
							     _("Slider Width"),
							     _("Width of scrollbar or scale thumb"),
							     0,
							     G_MAXINT,
							     14,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("trough_border",
                                                             _("Trough Border"),
                                                             _("Spacing between thumb/steppers and outer trough bevel"),
                                                             0,
                                                             G_MAXINT,
                                                             1,
                                                             G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("stepper_size",
							     _("Stepper Size"),
							     _("Length of step buttons at ends"),
							     0,
							     G_MAXINT,
							     14,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("stepper_spacing",
							     _("Stepper Spacing"),
							     _("Spacing between step buttons and thumb"),
                                                             0,
							     G_MAXINT,
							     0,
							     G_PARAM_READABLE));
}

static void
gtk_range_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkRange *range;

  range = GTK_RANGE (object);

  switch (prop_id)
    {
    case PROP_UPDATE_POLICY:
      gtk_range_set_update_policy (range, g_value_get_enum (value));
      break;
    case PROP_ADJUSTMENT:
      gtk_range_set_adjustment (range, g_value_get_object (value));
      break;
    case PROP_INVERTED:
      gtk_range_set_inverted (range, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_range_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkRange *range;

  range = GTK_RANGE (object);

  switch (prop_id)
    {
    case PROP_UPDATE_POLICY:
      g_value_set_enum (value, range->update_policy);
      break;
    case PROP_ADJUSTMENT:
      g_value_set_object (value, G_OBJECT (range->adjustment));
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, range->inverted);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_range_init (GtkRange *range)
{
  range->adjustment = NULL;
  range->update_policy = GTK_UPDATE_CONTINUOUS;
  range->inverted = FALSE;
  range->flippable = FALSE;
  range->min_slider_size = 1;
  range->has_stepper_a = FALSE;
  range->has_stepper_b = FALSE;
  range->has_stepper_c = FALSE;
  range->has_stepper_d = FALSE;
  range->need_recalc = TRUE;
  range->round_digits = -1;
  range->layout = g_new0 (GtkRangeLayout, 1);
  range->layout->grab_location = MOUSE_OUTSIDE;
  range->layout->grab_button = 0;
  range->timer = NULL;  
}

/**
 * gtk_range_get_adjustment:
 * @range: a #GtkRange
 * 
 * Get the #GtkAdjustment which is the "model" object for #GtkRange.
 * See gtk_range_set_adjustment() for details.
 * The return value does not have a reference added, so should not
 * be unreferenced.
 * 
 * Return value: a #GtkAdjustment
 **/
GtkAdjustment*
gtk_range_get_adjustment (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), NULL);

  if (!range->adjustment)
    gtk_range_set_adjustment (range, NULL);

  return range->adjustment;
}

/**
 * gtk_range_set_update_policy:
 * @range: a #GtkRange
 * @policy: update policy
 *
 * Sets the update policy for the range. #GTK_UPDATE_CONTINUOUS means that
 * anytime the range slider is moved, the range value will change and the
 * value_changed signal will be emitted. #GTK_UPDATE_DELAYED means that
 * the value will be updated after a brief timeout where no slider motion
 * occurs, so updates are spaced by a short time rather than
 * continuous. #GTK_UPDATE_DISCONTINUOUS means that the value will only
 * be updated when the user releases the button and ends the slider
 * drag operation.
 * 
 **/
void
gtk_range_set_update_policy (GtkRange      *range,
			     GtkUpdateType  policy)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  if (range->update_policy != policy)
    {
      range->update_policy = policy;
      g_object_notify (G_OBJECT (range), "update_policy");
    }
}

/**
 * gtk_range_get_update_policy:
 * @range: a #GtkRange
 *
 * Gets the update policy of @range. See gtk_range_set_update_policy().
 *
 * Return value: the current update policy
 **/
GtkUpdateType
gtk_range_get_update_policy (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), GTK_UPDATE_CONTINUOUS);

  return range->update_policy;
}

/**
 * gtk_range_set_adjustment:
 * @range: a #GtkRange
 * @adjustment: a #GtkAdjustment
 *
 * Sets the adjustment to be used as the "model" object for this range
 * widget. The adjustment indicates the current range value, the
 * minimum and maximum range values, the step/page increments used
 * for keybindings and scrolling, and the page size. The page size
 * is normally 0 for #GtkScale and nonzero for #GtkScrollbar, and
 * indicates the size of the visible area of the widget being scrolled.
 * The page size affects the size of the scrollbar slider.
 * 
 **/
void
gtk_range_set_adjustment (GtkRange      *range,
			  GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  
  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (range->adjustment != adjustment)
    {
      if (range->adjustment)
	{
	  gtk_signal_disconnect_by_func (GTK_OBJECT (range->adjustment),
                                         G_CALLBACK (gtk_range_adjustment_changed),
					 range);
          gtk_signal_disconnect_by_func (GTK_OBJECT (range->adjustment),
                                         G_CALLBACK (gtk_range_adjustment_value_changed),
					 range);
	  gtk_object_unref (GTK_OBJECT (range->adjustment));
	}

      range->adjustment = adjustment;
      gtk_object_ref (GTK_OBJECT (adjustment));
      gtk_object_sink (GTK_OBJECT (adjustment));
      
      gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			  (GtkSignalFunc) gtk_range_adjustment_changed,
			  range);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  (GtkSignalFunc) gtk_range_adjustment_value_changed,
			  range);
      
      gtk_range_adjustment_changed (adjustment, range);
      g_object_notify (G_OBJECT (range), "adjustment");
    }
}

/**
 * gtk_range_set_inverted:
 * @range: a #GtkRange
 * @setting: %TRUE to invert the range
 *
 * Ranges normally move from lower to higher values as the
 * slider moves from top to bottom or left to right. Inverted
 * ranges have higher values at the top or on the right rather than
 * on the bottom or left.
 * 
 **/
void
gtk_range_set_inverted (GtkRange *range,
                        gboolean  setting)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  
  setting = setting != FALSE;

  if (setting != range->inverted)
    {
      range->inverted = setting;
      g_object_notify (G_OBJECT (range), "inverted");
      gtk_widget_queue_resize (GTK_WIDGET (range));
    }
}

/**
 * gtk_range_get_inverted:
 * @range: a #GtkRange
 * 
 * Gets the value set by gtk_range_set_inverted().
 * 
 * Return value: %TRUE if the range is inverted
 **/
gboolean
gtk_range_get_inverted (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), FALSE);

  return range->inverted;
}

/**
 * gtk_range_set_increments:
 * @range: a #GtkRange
 * @step: step size
 * @page: page size
 *
 * Sets the step and page sizes for the range.
 * The step size is used when the user clicks the #GtkScrollbar
 * arrows or moves #GtkScale via arrow keys. The page size
 * is used for example when moving via Page Up or Page Down keys.
 * 
 **/
void
gtk_range_set_increments (GtkRange *range,
                          gdouble   step,
                          gdouble   page)
{
  g_return_if_fail (GTK_IS_RANGE (range));

  range->adjustment->step_increment = step;
  range->adjustment->page_increment = page;

  gtk_adjustment_changed (range->adjustment);
}

/**
 * gtk_range_set_range:
 * @range: a #GtkRange
 * @min: minimum range value
 * @max: maximum range value
 * 
 * Sets the allowable values in the #GtkRange, and clamps the range
 * value to be between min and max.
 **/
void
gtk_range_set_range (GtkRange *range,
                     gdouble   min,
                     gdouble   max)
{
  gdouble value;
  
  g_return_if_fail (GTK_IS_RANGE (range));
  g_return_if_fail (min < max);
  
  range->adjustment->lower = min;
  range->adjustment->upper = max;

  value = CLAMP (range->adjustment->value,
                 range->adjustment->lower,
                 (range->adjustment->upper - range->adjustment->page_size));
  
  gtk_adjustment_changed (range->adjustment);  
}

/**
 * gtk_range_set_value:
 * @range: a #GtkRange
 * @value: new value of the range
 *
 * Sets the current value of the range; if the value is outside the
 * minimum or maximum range values, it will be clamped to fit inside
 * them. The range emits the "value_changed" signal if the value
 * changes.
 * 
 **/
void
gtk_range_set_value (GtkRange *range,
                     gdouble   value)
{
  g_return_if_fail (GTK_IS_RANGE (range));
  
  value = CLAMP (value, range->adjustment->lower,
                 (range->adjustment->upper - range->adjustment->page_size));

  gtk_adjustment_set_value (range->adjustment, value);
}

/**
 * gtk_range_get_value:
 * @range: a #GtkRange
 * 
 * Gets the current value of the range.
 * 
 * Return value: current value of the range.
 **/
gdouble
gtk_range_get_value (GtkRange *range)
{
  g_return_val_if_fail (GTK_IS_RANGE (range), 0.0);

  return range->adjustment->value;
}

static gboolean
should_invert (GtkRange *range)
{  
  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
    return
      (range->inverted && !range->flippable) ||
      (range->inverted && range->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_LTR) ||
      (!range->inverted && range->flippable && gtk_widget_get_direction (GTK_WIDGET (range)) == GTK_TEXT_DIR_RTL);
  else
    return range->inverted;
}

static void
gtk_range_finalize (GObject *object)
{
  GtkRange *range;

  g_return_if_fail (GTK_IS_RANGE (object));

  range = GTK_RANGE (object);

  g_free (range->layout);

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_range_destroy (GtkObject *object)
{
  GtkRange *range;

  g_return_if_fail (GTK_IS_RANGE (object));

  range = GTK_RANGE (object);

  gtk_range_remove_step_timer (range);
  gtk_range_remove_update_timer (range);
  
  if (range->adjustment)
    {
      if (range->adjustment)
	gtk_signal_disconnect_by_data (GTK_OBJECT (range->adjustment),
				       (gpointer) range);
      gtk_object_unref (GTK_OBJECT (range->adjustment));
      range->adjustment = NULL;
    }

  (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_range_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
  GtkRange *range;
  gint slider_width, stepper_size, trough_border, stepper_spacing;
  GdkRectangle range_rect;
  GtkBorder border;
  
  range = GTK_RANGE (widget);
  
  gtk_range_get_props (range,
                       &slider_width, &stepper_size, &trough_border, &stepper_spacing);

  gtk_range_calc_request (range, 
                          slider_width, stepper_size, trough_border, stepper_spacing,
                          &range_rect, &border, NULL, NULL);

  requisition->width = range_rect.width + border.left + border.right;
  requisition->height = range_rect.height + border.top + border.bottom;
}

static void
gtk_range_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkRange *range;

  range = GTK_RANGE (widget);

  range->need_recalc = TRUE;
  gtk_range_calc_layout (range, range->adjustment->value);

  (* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);
}

static void
gtk_range_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;  

  range = GTK_RANGE (widget);

  gtk_range_calc_layout (range, range->adjustment->value);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, range);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, widget->state);
}

static void
gtk_range_unrealize (GtkWidget *widget)
{
  GtkRange *range;

  g_return_if_fail (GTK_IS_RANGE (widget));

  range = GTK_RANGE (widget);

  gtk_range_remove_step_timer (range);
  gtk_range_remove_update_timer (range);
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
draw_stepper (GtkRange     *range,
              GdkRectangle *rect,
              GtkArrowType  arrow_type,
              gboolean      clicked,
              gboolean      prelighted,
              GdkRectangle *area)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GdkRectangle intersection;

  /* More to get the right clip region than for efficiency */
  if (!gdk_rectangle_intersect (area, rect, &intersection))
    return;
  
  if (!GTK_WIDGET_IS_SENSITIVE (range))
    state_type = GTK_STATE_INSENSITIVE;
  else if (clicked)
    state_type = GTK_STATE_ACTIVE;
  else if (prelighted)
    state_type = GTK_STATE_PRELIGHT;
  else 
    state_type = GTK_STATE_NORMAL;
  
  if (clicked)
    shadow_type = GTK_SHADOW_IN;
  else
    shadow_type = GTK_SHADOW_OUT;
  
  gtk_paint_arrow (GTK_WIDGET (range)->style,
                   GTK_WIDGET (range)->window,
                   state_type, shadow_type, 
                   &intersection, GTK_WIDGET (range),
                   GTK_RANGE_GET_CLASS (range)->stepper_detail,
                   arrow_type,
                   TRUE, rect->x, rect->y, rect->width, rect->height);
}

static gint
gtk_range_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkRange *range;
  gboolean sensitive;
  GtkStateType state;
  GdkRectangle area;
  
  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  gtk_range_calc_layout (range, range->adjustment->value);

  sensitive = GTK_WIDGET_IS_SENSITIVE (widget);
  
  /* Just to be confusing, we draw the trough for the whole
   * range rectangle, not the trough rectangle (the trough
   * rectangle is just for hit detection)
   */
  /* The gdk_rectangle_intersect is more to get the right
   * clip region (limited to range_rect) than for efficiency
   */
  if (gdk_rectangle_intersect (&event->area, &range->range_rect,
                               &area))
    {
      gtk_paint_box (widget->style,
                     widget->window,
                     sensitive ? GTK_STATE_ACTIVE : GTK_STATE_INSENSITIVE,
                     GTK_SHADOW_IN,
                     &area, GTK_WIDGET(range), "trough",
                     range->range_rect.x,
                     range->range_rect.y,
                     range->range_rect.width,
                     range->range_rect.height);
      
                 
      if (sensitive &&
          GTK_WIDGET_HAS_FOCUS (range))
        gtk_paint_focus (widget->style,
                         widget->window,
                         &area, widget, "trough",
                         range->range_rect.x,
                         range->range_rect.y,
                         range->range_rect.width,
                         range->range_rect.height);
    }

  if (!sensitive)
    state = GTK_STATE_INSENSITIVE;
  else if (range->layout->mouse_location == MOUSE_SLIDER)
    state = GTK_STATE_PRELIGHT;
  else
    state = GTK_STATE_NORMAL;

  if (gdk_rectangle_intersect (&event->area,
                               &range->layout->slider,
                               &area))
    {
      gtk_paint_slider (widget->style,
                        widget->window,
                        state,
                        GTK_SHADOW_OUT,
                        &event->area,
                        widget,
                        GTK_RANGE_GET_CLASS (range)->slider_detail,
                        range->layout->slider.x,
                        range->layout->slider.y,
                        range->layout->slider.width,
                        range->layout->slider.height,
                        range->orientation);
    }
  
  if (range->has_stepper_a)
    draw_stepper (range, &range->layout->stepper_a,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_UP : GTK_ARROW_LEFT,
                  range->layout->grab_location == MOUSE_STEPPER_A,
                  range->layout->mouse_location == MOUSE_STEPPER_A,
                  &event->area);

  if (range->has_stepper_b)
    draw_stepper (range, &range->layout->stepper_b,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  range->layout->grab_location == MOUSE_STEPPER_B,
                  range->layout->mouse_location == MOUSE_STEPPER_B,
                  &event->area);

  if (range->has_stepper_c)
    draw_stepper (range, &range->layout->stepper_c,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_UP : GTK_ARROW_LEFT,
                  range->layout->grab_location == MOUSE_STEPPER_C,
                  range->layout->mouse_location == MOUSE_STEPPER_C,
                  &event->area);

  if (range->has_stepper_d)
    draw_stepper (range, &range->layout->stepper_d,
                  range->orientation == GTK_ORIENTATION_VERTICAL ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                  range->layout->grab_location == MOUSE_STEPPER_D,
                  range->layout->mouse_location == MOUSE_STEPPER_D,
                  &event->area);
  
  return FALSE;
}

static void
range_grab_add (GtkRange      *range,
                MouseLocation  location,
                gint           button)
{
  /* we don't actually gtk_grab, since a button is down */

  range->layout->grab_location = location;
  range->layout->grab_button = button;
  
  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (GTK_WIDGET (range));
}

static void
range_grab_remove (GtkRange *range)
{
  range->layout->grab_location = MOUSE_OUTSIDE;
  range->layout->grab_button = 0;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (GTK_WIDGET (range));
}

static GtkScrollType
range_get_scroll_for_grab (GtkRange      *range)
{  
  switch (range->layout->grab_location)
    {
      /* Backward stepper */
    case MOUSE_STEPPER_A:
    case MOUSE_STEPPER_C:
      switch (range->layout->grab_button)
        {
        case 1:
          return GTK_SCROLL_STEP_BACKWARD;
          break;
        case 2:
          return GTK_SCROLL_PAGE_BACKWARD;
          break;
        case 3:
          return GTK_SCROLL_START;
          break;
        }
      break;

      /* Forward stepper */
    case MOUSE_STEPPER_B:
    case MOUSE_STEPPER_D:
      switch (range->layout->grab_button)
        {
        case 1:
          return GTK_SCROLL_STEP_FORWARD;
          break;
        case 2:
          return GTK_SCROLL_PAGE_FORWARD;
          break;
        case 3:
          return GTK_SCROLL_END;
          break;
        }
      break;

      /* In the trough */
    case MOUSE_TROUGH:
      {
        if (range->trough_click_forward)
          {
            return range->layout->grab_button == 3
              ? GTK_SCROLL_PAGE_FORWARD : GTK_SCROLL_STEP_FORWARD;
          }
        else
          {
            return range->layout->grab_button == 3
              ? GTK_SCROLL_PAGE_BACKWARD : GTK_SCROLL_STEP_BACKWARD;
          }
      }
      break;

    case MOUSE_OUTSIDE:
    case MOUSE_SLIDER:
    case MOUSE_WIDGET:
      break;
    }

  return GTK_SCROLL_NONE;
}

static gdouble
coord_to_value (GtkRange *range,
                gint      coord)
{
  gdouble frac;
  gdouble value;
  
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    if (range->layout->trough.height == range->layout->slider.height)
      frac = 1.0;
    else 
      frac = ((coord - range->layout->trough.y) /
	      (gdouble) (range->layout->trough.height - range->layout->slider.height));
  else
    if (range->layout->trough.width == range->layout->slider.width)
      frac = 1.0;
    else
      frac = ((coord - range->layout->trough.x) /
	      (gdouble) (range->layout->trough.width - range->layout->slider.width));

  if (should_invert (range))
    frac = 1.0 - frac;
  
  value = range->adjustment->lower +
    frac * (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size);

  return value;
}

static gint
gtk_range_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkRange *range;

  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  /* ignore presses when we're already doing something else. */
  if (range->layout->grab_location != MOUSE_OUTSIDE)
    return FALSE;

  range->layout->mouse_x = event->x;
  range->layout->mouse_y = event->y;
  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);
    
  if (range->layout->mouse_location == MOUSE_TROUGH  &&
      (event->button == 1 || event->button == 3))
    {
      /* button 1 steps by step increment, as with button 1 on a stepper,
       * button 3 steps by page increment, as with button 2 on a stepper
       */
      GtkScrollType scroll;
      gdouble click_value;
      
      click_value = coord_to_value (range,
                                    range->orientation == GTK_ORIENTATION_VERTICAL ?
                                    event->y : event->x);
      
      range->trough_click_forward = click_value > range->adjustment->value;
      range_grab_add (range, MOUSE_TROUGH, event->button);
      
      scroll = range_get_scroll_for_grab (range);
      
      gtk_range_add_step_timer (range, scroll);

      return TRUE;
    }
  else if ((range->layout->mouse_location == MOUSE_STEPPER_A ||
            range->layout->mouse_location == MOUSE_STEPPER_B ||
            range->layout->mouse_location == MOUSE_STEPPER_C ||
            range->layout->mouse_location == MOUSE_STEPPER_D) &&
           (event->button == 1 || event->button == 2 || event->button == 3))
    {
      GdkRectangle *stepper_area;
      GtkScrollType scroll;
      
      range_grab_add (range, range->layout->mouse_location, event->button);

      stepper_area = get_area (range, range->layout->mouse_location);
      gtk_widget_queue_draw_area (widget,
                                  stepper_area->x,
                                  stepper_area->y,
                                  stepper_area->width,
                                  stepper_area->height);

      scroll = range_get_scroll_for_grab (range);
      if (scroll != GTK_SCROLL_NONE)
        gtk_range_add_step_timer (range, scroll);
      
      return TRUE;
    }
  else if ((range->layout->mouse_location == MOUSE_TROUGH &&
            event->button == 2) ||
           range->layout->mouse_location == MOUSE_SLIDER)
    {
      gboolean need_value_update = FALSE;

      /* Any button can be used to drag the slider, but you can start
       * dragging the slider with a trough click using button 2;
       * On button 2 press, we warp the slider to mouse position,
       * then begin the slider drag.
       */
      if (event->button == 2)
        {
          gdouble slider_low_value, slider_high_value, new_value;
          
          slider_high_value =
            coord_to_value (range,
                            range->orientation == GTK_ORIENTATION_VERTICAL ?
                            event->y : event->x);
          slider_low_value =
            coord_to_value (range,
                            range->orientation == GTK_ORIENTATION_VERTICAL ?
                            event->y - range->layout->slider.height :
                            event->x - range->layout->slider.width);
          
          /* compute new value for warped slider */
          new_value = slider_low_value + (slider_high_value - slider_low_value) / 2;

	  /* recalc slider, so we can set slide_initial_slider_position
           * properly
           */
	  range->need_recalc = TRUE;
          gtk_range_calc_layout (range, new_value);

	  /* defer adjustment updates to update_slider_position() in order
	   * to keep pixel quantisation
	   */
	  need_value_update = TRUE;
        }
      
      if (range->orientation == GTK_ORIENTATION_VERTICAL)
        {
          range->slide_initial_slider_position = range->layout->slider.y;
          range->slide_initial_coordinate = event->y;
        }
      else
        {
          range->slide_initial_slider_position = range->layout->slider.x;
          range->slide_initial_coordinate = event->x;
        }

      if (need_value_update)
        update_slider_position (range, event->x, event->y);

      range_grab_add (range, MOUSE_SLIDER, event->button);
      
      return TRUE;
    }
  
  return FALSE;
}

/* During a slide, move the slider as required given new mouse position */
static void
update_slider_position (GtkRange *range,
                        gint      mouse_x,
                        gint      mouse_y)
{
  gint delta;
  gint c;
  gdouble new_value;
  
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    delta = mouse_y - range->slide_initial_coordinate;
  else
    delta = mouse_x - range->slide_initial_coordinate;

  c = range->slide_initial_slider_position + delta;

  new_value = coord_to_value (range, c);
  
  gtk_range_internal_set_value (range, new_value);
}

static gint
gtk_range_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkRange *range;

  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  range->layout->mouse_x = event->x;
  range->layout->mouse_y = event->y;
  
  if (range->layout->grab_button == event->button)
    {
      GtkScrollType scroll;
      MouseLocation grab_location;

      grab_location = range->layout->grab_location;

      scroll = range_get_scroll_for_grab (range);
      
      range_grab_remove (range);
      gtk_range_remove_step_timer (range);
      
      /* We only do the move if we're still on top of the button at
       * release
       */
      if (grab_location == range->layout->mouse_location &&
          scroll != GTK_SCROLL_NONE)
        {
          gtk_range_scroll (range, scroll);
        }

      if (grab_location == MOUSE_SLIDER)
        update_slider_position (range, event->x, event->y);

      /* Flush any pending discontinuous/delayed updates */
      gtk_range_update_value (range);
      
      /* Just be lazy about this, if we scrolled it will all redraw anyway,
       * so no point optimizing the button deactivate case
       */
      gtk_widget_queue_draw (widget);
      
      return TRUE;
    }

  return FALSE;
}

static gint
gtk_range_scroll_event (GtkWidget      *widget,
			GdkEventScroll *event)
{
  GtkRange *range;

  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  if (GTK_WIDGET_REALIZED (range))
    {
      GtkAdjustment *adj = GTK_RANGE (range)->adjustment;
      gdouble new_value = adj->value + ((event->direction == GDK_SCROLL_UP ||
                                         event->direction == GDK_SCROLL_LEFT) ? 
					-adj->page_increment / 2: 
					adj->page_increment / 2);

      gtk_range_internal_set_value (range, new_value);

      /* Policy DELAYED makes sense with scroll events,
       * but DISCONTINUOUS doesn't, so we update immediately
       * for DISCONTINOUS
       */
      if (range->update_policy == GTK_UPDATE_DISCONTINUOUS)
        gtk_range_update_value (range);
    }

  return TRUE;
}

static gint
gtk_range_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkRange *range;
  gint x, y;

  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  gdk_window_get_pointer (widget->window, &x, &y, NULL);
  
  range->layout->mouse_x = x;
  range->layout->mouse_y = y;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);

  if (range->layout->grab_location == MOUSE_SLIDER)
    update_slider_position (range, x, y);

  /* We handled the event if the mouse was in the range_rect */
  return range->layout->mouse_location != MOUSE_OUTSIDE;
}

static gint
gtk_range_enter_notify (GtkWidget        *widget,
			GdkEventCrossing *event)
{
  GtkRange *range;

  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  range->layout->mouse_x = event->x;
  range->layout->mouse_y = event->y;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);
  
  return TRUE;
}

static gint
gtk_range_leave_notify (GtkWidget        *widget,
			GdkEventCrossing *event)
{
  GtkRange *range;

  g_return_val_if_fail (GTK_IS_RANGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  range = GTK_RANGE (widget);

  range->layout->mouse_x = -1;
  range->layout->mouse_y = -1;

  if (gtk_range_update_mouse_location (range))
    gtk_widget_queue_draw (widget);
  
  return TRUE;
}

static void
gtk_range_adjustment_changed (GtkAdjustment *adjustment,
			      gpointer       data)
{
  GtkRange *range;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  range = GTK_RANGE (data);

  range->need_recalc = TRUE;
  gtk_widget_queue_draw (GTK_WIDGET (range));

  /* Note that we don't round off to range->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */
}

static void
gtk_range_adjustment_value_changed (GtkAdjustment *adjustment,
				    gpointer       data)
{
  GtkRange *range;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  range = GTK_RANGE (data);

  range->need_recalc = TRUE;

  gtk_widget_queue_draw (GTK_WIDGET (range));
  /* This is so we don't lag the widget being scrolled. */
  if (GTK_WIDGET_REALIZED (range))
    gdk_window_process_updates (GTK_WIDGET (range)->window, TRUE);
  
  /* Note that we don't round off to range->round_digits here.
   * that's because it's really broken to change a value
   * in response to a change signal on that value; round_digits
   * is therefore defined to be a filter on what the GtkRange
   * can input into the adjustment, not a filter that the GtkRange
   * will enforce on the adjustment.
   */

  g_signal_emit (G_OBJECT (range), signals[VALUE_CHANGED], 0);
}

static void
gtk_range_style_set (GtkWidget *widget,
                     GtkStyle  *previous_style)
{
  GtkRange *range;

  g_return_if_fail (GTK_IS_RANGE (widget));

  range = GTK_RANGE (widget);

  range->need_recalc = TRUE;

  (* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous_style);
}

static void
step_back (GtkRange *range)
{
  gdouble newval;
  
  newval = range->adjustment->value - range->adjustment->step_increment;
  gtk_range_internal_set_value (range, newval);
}

static void
step_forward (GtkRange *range)
{
  gdouble newval;

  newval = range->adjustment->value + range->adjustment->step_increment;

  gtk_range_internal_set_value (range, newval);
}


static void
page_back (GtkRange *range)
{
  gdouble newval;

  newval = range->adjustment->value - range->adjustment->page_increment;
  gtk_range_internal_set_value (range, newval);
}

static void
page_forward (GtkRange *range)
{
  gdouble newval;

  newval = range->adjustment->value + range->adjustment->page_increment;

  gtk_range_internal_set_value (range, newval);
}

static void
gtk_range_scroll (GtkRange     *range,
                  GtkScrollType scroll)
{
  switch (scroll)
    {
    case GTK_SCROLL_STEP_LEFT:
      if (should_invert (range))
        step_forward (range);
      else
        step_back (range);
      break;
                    
    case GTK_SCROLL_STEP_UP:
      if (should_invert (range))
        step_forward (range);
      else
        step_back (range);
      break;

    case GTK_SCROLL_STEP_RIGHT:
      if (should_invert (range))
        step_back (range);
      else
        step_forward (range);
      break;
                    
    case GTK_SCROLL_STEP_DOWN:
      if (should_invert (range))
        step_back (range);
      else
        step_forward (range);
      break;
                  
    case GTK_SCROLL_STEP_BACKWARD:
      step_back (range);
      break;
                  
    case GTK_SCROLL_STEP_FORWARD:
      step_forward (range);
      break;

    case GTK_SCROLL_PAGE_LEFT:
      if (should_invert (range))
        page_forward (range);
      else
        page_back (range);
      break;
                    
    case GTK_SCROLL_PAGE_UP:
      if (should_invert (range))
        page_forward (range);
      else
        page_back (range);
      break;

    case GTK_SCROLL_PAGE_RIGHT:
      if (should_invert (range))
        page_back (range);
      else
        page_forward (range);
      break;
                    
    case GTK_SCROLL_PAGE_DOWN:
      if (should_invert (range))
        page_back (range);
      else
        page_forward (range);
      break;
                  
    case GTK_SCROLL_PAGE_BACKWARD:
      page_back (range);
      break;
                  
    case GTK_SCROLL_PAGE_FORWARD:
      page_forward (range);
      break;

    case GTK_SCROLL_START:
      gtk_range_internal_set_value (range,
                                    range->adjustment->lower);
      break;

    case GTK_SCROLL_END:
      gtk_range_internal_set_value (range,
                                    range->adjustment->upper - range->adjustment->page_size);
      break;

    case GTK_SCROLL_JUMP:
      /* Used by CList, range doesn't use it. */
      break;

    case GTK_SCROLL_NONE:
      break;
    }
}

static void
gtk_range_move_slider (GtkRange     *range,
                       GtkScrollType scroll)
{
  gtk_range_scroll (range, scroll);

  /* Policy DELAYED makes sense with key events,
   * but DISCONTINUOUS doesn't, so we update immediately
   * for DISCONTINOUS
   */
  if (range->update_policy == GTK_UPDATE_DISCONTINUOUS)
    gtk_range_update_value (range);
}

static void
gtk_range_get_props (GtkRange  *range,
                     gint      *slider_width,
                     gint      *stepper_size,
                     gint      *trough_border,
                     gint      *stepper_spacing)
{
  GtkWidget *widget =  GTK_WIDGET (range);
  gint tmp_slider_width, tmp_stepper_size, tmp_trough_border, tmp_stepper_spacing;
  
  gtk_widget_style_get (widget,
                        "slider_width", &tmp_slider_width,
                        "trough_border", &tmp_trough_border,
                        "stepper_size", &tmp_stepper_size,
                        "stepper_spacing", &tmp_stepper_spacing,
                        NULL);
  
  if (slider_width)
    *slider_width = tmp_slider_width;

  if (trough_border)
    *trough_border = tmp_trough_border;

  if (stepper_size)
    *stepper_size = tmp_stepper_size;

  if (stepper_spacing)
    *stepper_spacing = tmp_stepper_spacing;
}

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

/* Update mouse location, return TRUE if it changes */
static gboolean
gtk_range_update_mouse_location (GtkRange *range)
{
  gint x, y;
  MouseLocation old;
  GtkWidget *widget;

  widget = GTK_WIDGET (range);
  
  old = range->layout->mouse_location;
  
  x = range->layout->mouse_x;
  y = range->layout->mouse_y;

  if (range->layout->grab_location != MOUSE_OUTSIDE)
    range->layout->mouse_location = range->layout->grab_location;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_a))
    range->layout->mouse_location = MOUSE_STEPPER_A;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_b))
    range->layout->mouse_location = MOUSE_STEPPER_B;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_c))
    range->layout->mouse_location = MOUSE_STEPPER_C;
  else if (POINT_IN_RECT (x, y, range->layout->stepper_d))
    range->layout->mouse_location = MOUSE_STEPPER_D;
  else if (POINT_IN_RECT (x, y, range->layout->slider))
    range->layout->mouse_location = MOUSE_SLIDER;
  else if (POINT_IN_RECT (x, y, range->layout->trough))
    range->layout->mouse_location = MOUSE_TROUGH;
  else if (POINT_IN_RECT (x, y, widget->allocation))
    range->layout->mouse_location = MOUSE_WIDGET;
  else
    range->layout->mouse_location = MOUSE_OUTSIDE;

  return old != range->layout->mouse_location;
}

/* Clamp rect, border inside widget->allocation, such that we prefer
 * to take space from border not rect in all directions, and prefer to
 * give space to border over rect in one direction.
 */
static void
clamp_dimensions (GtkWidget    *widget,
                  GdkRectangle *rect,
                  GtkBorder    *border,
                  gboolean      border_expands_horizontally)
{
  gint extra, shortage;
  
  g_return_if_fail (rect->x == 0);
  g_return_if_fail (rect->y == 0);  
  g_return_if_fail (rect->width >= 0);
  g_return_if_fail (rect->height >= 0);

  /* Width */
  
  extra = widget->allocation.width - border->left - border->right - rect->width;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          border->left += extra / 2;
          border->right += extra / 2 + extra % 2;
        }
      else
        {
          rect->width += extra;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = rect->width - widget->allocation.width;
  if (shortage > 0)
    {
      rect->width = widget->allocation.width;
      /* lose the border */
      border->left = 0;
      border->right = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->width + border->left + border->right -
        widget->allocation.width;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->left -= shortage / 2;
          border->right -= shortage / 2 + shortage % 2;
        }
    }

  /* Height */
  
  extra = widget->allocation.height - border->top - border->bottom - rect->height;
  if (extra > 0)
    {
      if (border_expands_horizontally)
        {
          /* don't expand border vertically */
          rect->height += extra;
        }
      else
        {
          border->top += extra / 2;
          border->bottom += extra / 2 + extra % 2;
        }
    }
  
  /* See if we can fit rect, if not kill the border */
  shortage = rect->height - widget->allocation.height;
  if (shortage > 0)
    {
      rect->height = widget->allocation.height;
      /* lose the border */
      border->top = 0;
      border->bottom = 0;
    }
  else
    {
      /* See if we can fit rect with borders */
      shortage = rect->height + border->top + border->bottom -
        widget->allocation.height;
      if (shortage > 0)
        {
          /* Shrink borders */
          border->top -= shortage / 2;
          border->bottom -= shortage / 2 + shortage % 2;
        }
    }
}

static void
gtk_range_calc_request (GtkRange      *range,
                        gint           slider_width,
                        gint           stepper_size,
                        gint           trough_border,
                        gint           stepper_spacing,
                        GdkRectangle  *range_rect,
                        GtkBorder     *border,
                        gint          *n_steppers_p,
                        gint          *slider_length_p)
{
  gint slider_length;
  gint n_steppers;
  
  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (GTK_RANGE_GET_CLASS (range)->get_range_border)
    (* GTK_RANGE_GET_CLASS (range)->get_range_border) (range, border);
  
  n_steppers = 0;
  if (range->has_stepper_a)
    n_steppers += 1;
  if (range->has_stepper_b)
    n_steppers += 1;
  if (range->has_stepper_c)
    n_steppers += 1;
  if (range->has_stepper_d)
    n_steppers += 1;

  slider_length = range->min_slider_size;

  range_rect->x = 0;
  range_rect->y = 0;
  
  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      range_rect->width = trough_border * 2 + slider_width;
      range_rect->height = stepper_size * n_steppers + stepper_spacing * 2 + trough_border * 2 + slider_length;
    }
  else
    {
      range_rect->width = stepper_size * n_steppers + stepper_spacing * 2 + trough_border * 2 + slider_length;
      range_rect->height = trough_border * 2 + slider_width;
    }

  if (n_steppers_p)
    *n_steppers_p = n_steppers;

  if (slider_length_p)
    *slider_length_p = slider_length;
}

static void
gtk_range_calc_layout (GtkRange *range,
		       gdouble   adjustment_value)
{
  gint slider_width, stepper_size, trough_border, stepper_spacing;
  gint slider_length;
  GtkBorder border;
  gint n_steppers;
  GdkRectangle range_rect;
  GtkRangeLayout *layout;
  GtkWidget *widget;
  
  if (!range->need_recalc)
    return;

  /* If we have a too-small allocation, we prefer the steppers over
   * the trough/slider, probably the steppers are a more useful
   * feature in small spaces.
   *
   * Also, we prefer to draw the range itself rather than the border
   * areas if there's a conflict, since the borders will be decoration
   * not controls. Though this depends on subclasses cooperating by
   * not drawing on range->range_rect.
   */

  widget = GTK_WIDGET (range);
  layout = range->layout;
  
  gtk_range_get_props (range,
                       &slider_width, &stepper_size, &trough_border, &stepper_spacing);

  gtk_range_calc_request (range, 
                          slider_width, stepper_size, trough_border, stepper_spacing,
                          &range_rect, &border, &n_steppers, &slider_length);
  
  /* We never expand to fill available space in the small dimension
   * (i.e. vertical scrollbars are always a fixed width)
   */
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      clamp_dimensions (widget, &range_rect, &border, TRUE);
    }
  else
    {
      clamp_dimensions (widget, &range_rect, &border, FALSE);
    }
  
  range_rect.x = border.left;
  range_rect.y = border.top;
  
  range->range_rect = range_rect;
  
  if (range->orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint stepper_width, stepper_height;

      /* Steppers are the width of the range, and stepper_size in
       * height, or if we don't have enough height, divided equally
       * among available space.
       */
      stepper_width = range_rect.width - trough_border * 2;

      if (stepper_width < 1)
        stepper_width = range_rect.width; /* screw the trough border */

      if (n_steppers == 0)
        stepper_height = 0; /* avoid divide by n_steppers */
      else
        stepper_height = MIN (stepper_size, (range_rect.height / n_steppers));

      /* Stepper A */
      
      layout->stepper_a.x = range_rect.x + trough_border;
      layout->stepper_a.y = range_rect.y + trough_border;

      if (range->has_stepper_a)
        {
          layout->stepper_a.width = stepper_width;
          layout->stepper_a.height = stepper_height;
        }
      else
        {
          layout->stepper_a.width = 0;
          layout->stepper_a.height = 0;
        }

      /* Stepper B */
      
      layout->stepper_b.x = layout->stepper_a.x;
      layout->stepper_b.y = layout->stepper_a.y + layout->stepper_a.height;

      if (range->has_stepper_b)
        {
          layout->stepper_b.width = stepper_width;
          layout->stepper_b.height = stepper_height;
        }
      else
        {
          layout->stepper_b.width = 0;
          layout->stepper_b.height = 0;
        }

      /* Stepper D */

      if (range->has_stepper_d)
        {
          layout->stepper_d.width = stepper_width;
          layout->stepper_d.height = stepper_height;
        }
      else
        {
          layout->stepper_d.width = 0;
          layout->stepper_d.height = 0;
        }
      
      layout->stepper_d.x = layout->stepper_a.x;
      layout->stepper_d.y = range_rect.y + range_rect.height - layout->stepper_d.height - trough_border;

      /* Stepper C */

      if (range->has_stepper_c)
        {
          layout->stepper_c.width = stepper_width;
          layout->stepper_c.height = stepper_height;
        }
      else
        {
          layout->stepper_c.width = 0;
          layout->stepper_c.height = 0;
        }
      
      layout->stepper_c.x = layout->stepper_a.x;
      layout->stepper_c.y = layout->stepper_d.y - layout->stepper_c.height;

      /* Now the trough is the remaining space between steppers B and C,
       * if any
       */
      layout->trough.x = range_rect.x;
      layout->trough.y = layout->stepper_b.y + layout->stepper_b.height;
      layout->trough.width = range_rect.width;
      layout->trough.height = layout->stepper_c.y - (layout->stepper_b.y + layout->stepper_b.height);
      
      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      layout->slider.x = layout->trough.x + trough_border;
      layout->slider.width = layout->trough.width - trough_border * 2;

      /* Compute slider position/length */
      {
        gint y, bottom, top, height;
        
        top = layout->trough.y + stepper_spacing;
        bottom = layout->trough.y + layout->trough.height - stepper_spacing;
        
        /* slider height is the fraction (page_size /
         * total_adjustment_range) times the trough height in pixels
         */
        height = ((bottom - top) * (range->adjustment->page_size /
                                    (range->adjustment->upper - range->adjustment->lower)));
        
        if (height < range->min_slider_size ||
            range->slider_size_fixed)
          height = range->min_slider_size;
        
        height = MIN (height, (layout->trough.height - stepper_spacing * 2));
        
        y = top;
        
	if (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size != 0)
		y += (bottom - top - height) * ((adjustment_value - range->adjustment->lower) /
						(range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));
        
        y = CLAMP (y, top, bottom);
        
        if (should_invert (range))
          y = bottom - (y - top + height);
        
        layout->slider.y = y;
        layout->slider.height = height;

        /* These are publically exported */
        range->slider_start = layout->slider.y;
        range->slider_end = layout->slider.y + layout->slider.height;
      }
    }
  else
    {
      gint stepper_width, stepper_height;

      /* Steppers are the height of the range, and stepper_size in
       * width, or if we don't have enough width, divided equally
       * among available space.
       */
      stepper_height = range_rect.height - trough_border * 2;

      if (stepper_height < 1)
        stepper_height = range_rect.height; /* screw the trough border */

      if (n_steppers == 0)
        stepper_width = 0; /* avoid divide by n_steppers */
      else
        stepper_width = MIN (stepper_size, (range_rect.width / n_steppers));

      /* Stepper A */
      
      layout->stepper_a.x = range_rect.x + trough_border;
      layout->stepper_a.y = range_rect.y + trough_border;

      if (range->has_stepper_a)
        {
          layout->stepper_a.width = stepper_width;
          layout->stepper_a.height = stepper_height;
        }
      else
        {
          layout->stepper_a.width = 0;
          layout->stepper_a.height = 0;
        }

      /* Stepper B */
      
      layout->stepper_b.x = layout->stepper_a.x + layout->stepper_a.width;
      layout->stepper_b.y = layout->stepper_a.y;

      if (range->has_stepper_b)
        {
          layout->stepper_b.width = stepper_width;
          layout->stepper_b.height = stepper_height;
        }
      else
        {
          layout->stepper_b.width = 0;
          layout->stepper_b.height = 0;
        }

      /* Stepper D */

      if (range->has_stepper_d)
        {
          layout->stepper_d.width = stepper_width;
          layout->stepper_d.height = stepper_height;
        }
      else
        {
          layout->stepper_d.width = 0;
          layout->stepper_d.height = 0;
        }

      layout->stepper_d.x = range_rect.x + range_rect.width - layout->stepper_d.width - trough_border;
      layout->stepper_d.y = layout->stepper_a.y;


      /* Stepper C */

      if (range->has_stepper_c)
        {
          layout->stepper_c.width = stepper_width;
          layout->stepper_c.height = stepper_height;
        }
      else
        {
          layout->stepper_c.width = 0;
          layout->stepper_c.height = 0;
        }
      
      layout->stepper_c.x = layout->stepper_d.x - layout->stepper_c.width;
      layout->stepper_c.y = layout->stepper_a.y;

      /* Now the trough is the remaining space between steppers B and C,
       * if any
       */
      layout->trough.x = layout->stepper_b.x + layout->stepper_b.width;
      layout->trough.y = range_rect.y;

      layout->trough.width = layout->stepper_c.x - (layout->stepper_b.x + layout->stepper_b.width);
      layout->trough.height = range_rect.height;
      
      /* Slider fits into the trough, with stepper_spacing on either side,
       * and the size/position based on the adjustment or fixed, depending.
       */
      layout->slider.y = layout->trough.y + trough_border;
      layout->slider.height = layout->trough.height - trough_border * 2;

      /* Compute slider position/length */
      {
        gint x, left, right, width;
        
        left = layout->trough.x + stepper_spacing;
        right = layout->trough.x + layout->trough.width - stepper_spacing;
        
        /* slider width is the fraction (page_size /
         * total_adjustment_range) times the trough width in pixels
         */
        width = ((right - left) * (range->adjustment->page_size /
                                   (range->adjustment->upper - range->adjustment->lower)));
        
        if (width < range->min_slider_size ||
            range->slider_size_fixed)
          width = range->min_slider_size;
        
        width = MIN (width, (layout->trough.width - stepper_spacing * 2));
        
        x = left;
        
        x += (right - left - width) * ((adjustment_value - range->adjustment->lower) /
                                       (range->adjustment->upper - range->adjustment->lower - range->adjustment->page_size));
        
        x = CLAMP (x, left, right);
        
        if (should_invert (range))
          x = right - (x - left + width);
        
        layout->slider.x = x;
        layout->slider.width = width;

        /* These are publically exported */
        range->slider_start = layout->slider.x;
        range->slider_end = layout->slider.x + layout->slider.width;
      }
    }
  
  gtk_range_update_mouse_location (range);
}

static GdkRectangle*
get_area (GtkRange     *range,
          MouseLocation location)
{
  switch (location)
    {
    case MOUSE_STEPPER_A:
      return &range->layout->stepper_a;
    case MOUSE_STEPPER_B:
      return &range->layout->stepper_b;
    case MOUSE_STEPPER_C:
      return &range->layout->stepper_c;
    case MOUSE_STEPPER_D:
      return &range->layout->stepper_d;
    case MOUSE_TROUGH:
      return &range->layout->trough;
    case MOUSE_SLIDER:
      return &range->layout->slider;
    case MOUSE_WIDGET:
    case MOUSE_OUTSIDE:
      break;
    }

  g_warning (G_STRLOC": bug");
  return NULL;
}

static void
gtk_range_internal_set_value (GtkRange *range,
                              gdouble   value)
{
  value = CLAMP (value, range->adjustment->lower,
                 (range->adjustment->upper - range->adjustment->page_size));

  if (range->round_digits >= 0)
    {
      char buffer[128];

      /* This is just so darn lame. */
      g_snprintf (buffer, 128, "%0.*f",
                  range->round_digits, value);
      sscanf (buffer, "%lf", &value);
    }
  
  if (range->adjustment->value != value)
    {
      range->need_recalc = TRUE;

      gtk_widget_queue_draw (GTK_WIDGET (range));
      
      switch (range->update_policy)
        {
        case GTK_UPDATE_CONTINUOUS:
          gtk_adjustment_set_value (range->adjustment, value);
          break;

          /* Delayed means we update after a period of inactivity */
        case GTK_UPDATE_DELAYED:
          gtk_range_reset_update_timer (range);
          /* FALL THRU */

          /* Discontinuous means we update on button release */
        case GTK_UPDATE_DISCONTINUOUS:
          /* don't emit value_changed signal */
          range->adjustment->value = value;
          range->update_pending = TRUE;
          break;
        }
    }
}

static void
gtk_range_update_value (GtkRange *range)
{
  gtk_range_remove_update_timer (range);
  
  if (range->update_pending)
    {
      gtk_adjustment_value_changed (range->adjustment);

      range->update_pending = FALSE;
    }
}

struct _GtkRangeStepTimer
{
  guint timeout_id;
  GtkScrollType step;
};

static gboolean
second_timeout (gpointer data)
{
  GtkRange *range;

  GDK_THREADS_ENTER ();
  range = GTK_RANGE (data);
  gtk_range_scroll (range, range->timer->step);
  GDK_THREADS_LEAVE ();
  
  return TRUE;
}

static gboolean
initial_timeout (gpointer data)
{
  GtkRange *range;

  GDK_THREADS_ENTER ();
  range = GTK_RANGE (data);
  range->timer->timeout_id = 
    g_timeout_add (SCROLL_LATER_DELAY,
                   second_timeout,
                   range);
  GDK_THREADS_LEAVE ();

  /* remove self */
  return FALSE;
}

static void
gtk_range_add_step_timer (GtkRange      *range,
                          GtkScrollType  step)
{
  g_return_if_fail (range->timer == NULL);
  g_return_if_fail (step != GTK_SCROLL_NONE);
  
  range->timer = g_new (GtkRangeStepTimer, 1);

  range->timer->timeout_id =
    g_timeout_add (SCROLL_INITIAL_DELAY,
                   initial_timeout,
                   range);
  range->timer->step = step;
}

static void
gtk_range_remove_step_timer (GtkRange *range)
{
  if (range->timer)
    {
      if (range->timer->timeout_id != 0)
        g_source_remove (range->timer->timeout_id);

      g_free (range->timer);

      range->timer = NULL;
    }
}

static gboolean
update_timeout (gpointer data)
{
  GtkRange *range;

  GDK_THREADS_ENTER ();
  range = GTK_RANGE (data);
  gtk_range_update_value (range);
  range->update_timeout_id = 0;
  GDK_THREADS_LEAVE ();

  /* self-remove */
  return FALSE;
}

static void
gtk_range_reset_update_timer (GtkRange *range)
{
  gtk_range_remove_update_timer (range);

  range->update_timeout_id = g_timeout_add (UPDATE_DELAY,
                                            update_timeout,
                                            range);
}

static void
gtk_range_remove_update_timer (GtkRange *range)
{
  if (range->update_timeout_id != 0)
    {
      g_source_remove (range->update_timeout_id);
      range->update_timeout_id = 0;
    }
}
