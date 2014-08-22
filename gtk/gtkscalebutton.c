/* GTK - The GIMP Toolkit
 * Copyright (C) 2005 Ronald S. Bultje
 * Copyright (C) 2006, 2007 Christian Persch
 * Copyright (C) 2006 Jan Arne Petersen
 * Copyright (C) 2005-2007 Red Hat, Inc.
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * Authors:
 * - Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * - Bastien Nocera <bnocera@redhat.com>
 * - Jan Arne Petersen <jpetersen@jpetersen.org>
 * - Christian Persch <chpe@svn.gnome.org>
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
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkscalebutton.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gtkadjustment.h"
#include "gtkbindings.h"
#include "gtkframe.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtkscale.h"
#include "gtkbox.h"
#include "gtkwindow.h"
#include "gtkwindowprivate.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "a11y/gtkscalebuttonaccessible.h"

/**
 * SECTION:gtkscalebutton
 * @Short_description: A button which pops up a scale
 * @Title: GtkScaleButton
 *
 * #GtkScaleButton provides a button which pops up a scale widget.
 * This kind of widget is commonly used for volume controls in multimedia
 * applications, and GTK+ provides a #GtkVolumeButton subclass that
 * is tailored for this use case.
 */


#define SCALE_SIZE 100
#define CLICK_TIMEOUT 250

enum
{
  VALUE_CHANGED,
  POPUP,
  POPDOWN,

  LAST_SIGNAL
};

enum
{
  PROP_0,

  PROP_ORIENTATION,
  PROP_VALUE,
  PROP_SIZE,
  PROP_ADJUSTMENT,
  PROP_ICONS
};

struct _GtkScaleButtonPrivate
{
  GtkWidget *plus_button;
  GtkWidget *minus_button;
  GtkWidget *dock;
  GtkWidget *box;
  GtkWidget *scale;
  GtkWidget *image;
  GtkWidget *active_button;

  GtkIconSize size;
  GtkOrientation orientation;
  GtkOrientation applied_orientation;

  guint click_id;

  gchar **icon_list;

  GtkAdjustment *adjustment; /* needed because it must be settable in init() */
};

static void     gtk_scale_button_constructed    (GObject             *object);
static void	gtk_scale_button_dispose	(GObject             *object);
static void     gtk_scale_button_finalize       (GObject             *object);
static void	gtk_scale_button_set_property	(GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void	gtk_scale_button_get_property	(GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);
static void gtk_scale_button_set_orientation_private (GtkScaleButton *button,
                                                      GtkOrientation  orientation);
static gboolean	gtk_scale_button_scroll		(GtkWidget           *widget,
						 GdkEventScroll      *event);
static void     gtk_scale_button_clicked        (GtkButton           *button);
static void     gtk_scale_button_popup          (GtkWidget           *widget);
static void     gtk_scale_button_popdown        (GtkWidget           *widget);
static gboolean cb_button_press			(GtkWidget           *widget,
						 GdkEventButton      *event,
						 gpointer             user_data);
static gboolean cb_button_release		(GtkWidget           *widget,
						 GdkEventButton      *event,
						 gpointer             user_data);
static void     cb_button_clicked               (GtkWidget           *button,
                                                 gpointer             user_data);
static void     gtk_scale_button_update_icon    (GtkScaleButton      *button);
static void     cb_scale_value_changed          (GtkRange            *range,
                                                 gpointer             user_data);
static void     cb_popup_mapped                 (GtkWidget           *popup,
                                                 gpointer             user_data);

G_DEFINE_TYPE_WITH_CODE (GtkScaleButton, gtk_scale_button, GTK_TYPE_BUTTON,
                         G_ADD_PRIVATE (GtkScaleButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))

static guint signals[LAST_SIGNAL] = { 0, };

static void
gtk_scale_button_class_init (GtkScaleButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);
  GtkBindingSet *binding_set;

  gobject_class->constructed = gtk_scale_button_constructed;
  gobject_class->finalize = gtk_scale_button_finalize;
  gobject_class->dispose = gtk_scale_button_dispose;
  gobject_class->set_property = gtk_scale_button_set_property;
  gobject_class->get_property = gtk_scale_button_get_property;

  widget_class->scroll_event = gtk_scale_button_scroll;

  button_class->clicked = gtk_scale_button_clicked;

  /**
   * GtkScaleButton:orientation:
   *
   * The orientation of the #GtkScaleButton's popup window.
   *
   * Note that since GTK+ 2.16, #GtkScaleButton implements the
   * #GtkOrientable interface which has its own @orientation
   * property. However we redefine the property here in order to
   * override its default horizontal orientation.
   *
   * Since: 2.14
   **/
  g_object_class_override_property (gobject_class,
				    PROP_ORIENTATION,
				    "orientation");

  g_object_class_install_property (gobject_class,
				   PROP_VALUE,
				   g_param_spec_double ("value",
							P_("Value"),
							P_("The value of the scale"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0,
							GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
				   PROP_SIZE,
				   g_param_spec_enum ("size",
						      P_("Icon size"),
						      P_("The icon size"),
						      GTK_TYPE_ICON_SIZE,
						      GTK_ICON_SIZE_SMALL_TOOLBAR,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
							P_("Adjustment"),
							P_("The GtkAdjustment that contains the current value of this scale button object"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkScaleButton:icons:
   *
   * The names of the icons to be used by the scale button.
   * The first item in the array will be used in the button
   * when the current value is the lowest value, the second
   * item for the highest value. All the subsequent icons will
   * be used for all the other values, spread evenly over the
   * range of values.
   *
   * If there's only one icon name in the @icons array, it will
   * be used for all the values. If only two icon names are in
   * the @icons array, the first one will be used for the bottom
   * 50% of the scale, and the second one for the top 50%.
   *
   * It is recommended to use at least 3 icons so that the
   * #GtkScaleButton reflects the current value of the scale
   * better for the users.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ICONS,
                                   g_param_spec_boxed ("icons",
                                                       P_("Icons"),
                                                       P_("List of icon names"),
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkScaleButton::value-changed:
   * @button: the object which received the signal
   * @value: the new value
   *
   * The ::value-changed signal is emitted when the value field has
   * changed.
   *
   * Since: 2.12
   */
  signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkScaleButtonClass, value_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__DOUBLE,
		  G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /**
   * GtkScaleButton::popup:
   * @button: the object which received the signal
   *
   * The ::popup signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to popup the scale widget.
   *
   * The default bindings for this signal are Space, Enter and Return.
   *
   * Since: 2.12
   */
  signals[POPUP] =
    g_signal_new_class_handler (I_("popup"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_scale_button_popup),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkScaleButton::popdown:
   * @button: the object which received the signal
   *
   * The ::popdown signal is a
   * [keybinding signal][GtkBindingSignal]
   * which gets emitted to popdown the scale widget.
   *
   * The default binding for this signal is Escape.
   *
   * Since: 2.12
   */
  signals[POPDOWN] =
    g_signal_new_class_handler (I_("popdown"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_scale_button_popdown),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /* Key bindings */
  binding_set = gtk_binding_set_by_class (widget_class);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0,
				"popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0,
				"popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
				"popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
				"popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
				"popup", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
				"popdown", 0);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkscalebutton.ui");

  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkScaleButton, plus_button);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkScaleButton, minus_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, dock);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, scale);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkScaleButton, adjustment);

  gtk_widget_class_bind_template_callback (widget_class, cb_button_press);
  gtk_widget_class_bind_template_callback (widget_class, cb_button_release);
  gtk_widget_class_bind_template_callback (widget_class, cb_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, cb_scale_value_changed);
  gtk_widget_class_bind_template_callback (widget_class, cb_popup_mapped);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SCALE_BUTTON_ACCESSIBLE);
}

static void
gtk_scale_button_init (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv;

  button->priv = priv = gtk_scale_button_get_instance_private (button);

  priv->click_id = 0;
  priv->orientation = GTK_ORIENTATION_VERTICAL;
  priv->applied_orientation = GTK_ORIENTATION_VERTICAL;

  gtk_widget_init_template (GTK_WIDGET (button));
  gtk_popover_set_relative_to (GTK_POPOVER (priv->dock), GTK_WIDGET (button));

  /* Need a local reference to the adjustment */
  g_object_ref (priv->adjustment);

  gtk_widget_add_events (GTK_WIDGET (button), GDK_SMOOTH_SCROLL_MASK);
}

static void
gtk_scale_button_constructed (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = button->priv;

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->constructed (object);

  /* set button text and size */
  priv->size = GTK_ICON_SIZE_SMALL_TOOLBAR;
  gtk_scale_button_update_icon (button);
}

static void
gtk_scale_button_set_property (GObject       *object,
			       guint          prop_id,
			       const GValue  *value,
			       GParamSpec    *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_scale_button_set_orientation_private (button, g_value_get_enum (value));
      break;
    case PROP_VALUE:
      gtk_scale_button_set_value (button, g_value_get_double (value));
      break;
    case PROP_SIZE:
      if (button->priv->size != g_value_get_enum (value))
        {
          button->priv->size = g_value_get_enum (value);
          gtk_scale_button_update_icon (button);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_ADJUSTMENT:
      gtk_scale_button_set_adjustment (button, g_value_get_object (value));
      break;
    case PROP_ICONS:
      gtk_scale_button_set_icons (button,
                                  (const gchar **)g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_button_get_property (GObject     *object,
			       guint        prop_id,
			       GValue      *value,
			       GParamSpec  *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = button->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_VALUE:
      g_value_set_double (value, gtk_scale_button_get_value (button));
      break;
    case PROP_SIZE:
      g_value_set_enum (value, priv->size);
      break;
    case PROP_ADJUSTMENT:
      g_value_set_object (value, gtk_scale_button_get_adjustment (button));
      break;
    case PROP_ICONS:
      g_value_set_boxed (value, priv->icon_list);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_button_finalize (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = button->priv;

  if (priv->icon_list)
    {
      g_strfreev (priv->icon_list);
      priv->icon_list = NULL;
    }

  if (priv->adjustment)
    {
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->finalize (object);
}

static void
gtk_scale_button_dispose (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = button->priv;

  if (priv->dock)
    {
      gtk_widget_destroy (priv->dock);
      priv->dock = NULL;
    }

  if (priv->click_id != 0)
    {
      g_source_remove (priv->click_id);
      priv->click_id = 0;
    }

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->dispose (object);
}

/**
 * gtk_scale_button_new:
 * @size: (type int): a stock icon size
 * @min: the minimum value of the scale (usually 0)
 * @max: the maximum value of the scale (usually 100)
 * @step: the stepping of value when a scroll-wheel event,
 *        or up/down arrow event occurs (usually 2)
 * @icons: (allow-none) (array zero-terminated=1): a %NULL-terminated
 *         array of icon names, or %NULL if you want to set the list
 *         later with gtk_scale_button_set_icons()
 *
 * Creates a #GtkScaleButton, with a range between @min and @max, with
 * a stepping of @step.
 *
 * Returns: a new #GtkScaleButton
 *
 * Since: 2.12
 */
GtkWidget *
gtk_scale_button_new (GtkIconSize   size,
		      gdouble       min,
		      gdouble       max,
		      gdouble       step,
		      const gchar **icons)
{
  GtkScaleButton *button;
  GtkAdjustment *adjustment;

  adjustment = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  button = g_object_new (GTK_TYPE_SCALE_BUTTON,
                         "adjustment", adjustment,
                         "icons", icons,
                         "size", size,
                         NULL);

  return GTK_WIDGET (button);
}

/**
 * gtk_scale_button_get_value:
 * @button: a #GtkScaleButton
 *
 * Gets the current value of the scale button.
 *
 * Returns: current value of the scale button
 *
 * Since: 2.12
 */
gdouble
gtk_scale_button_get_value (GtkScaleButton * button)
{
  GtkScaleButtonPrivate *priv;

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), 0);

  priv = button->priv;

  return gtk_adjustment_get_value (priv->adjustment);
}

/**
 * gtk_scale_button_set_value:
 * @button: a #GtkScaleButton
 * @value: new value of the scale button
 *
 * Sets the current value of the scale; if the value is outside
 * the minimum or maximum range values, it will be clamped to fit
 * inside them. The scale button emits the #GtkScaleButton::value-changed
 * signal if the value changes.
 *
 * Since: 2.12
 */
void
gtk_scale_button_set_value (GtkScaleButton *button,
			    gdouble         value)
{
  GtkScaleButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  priv = button->priv;

  gtk_range_set_value (GTK_RANGE (priv->scale), value);
  g_object_notify (G_OBJECT (button), "value");
}

/**
 * gtk_scale_button_set_icons:
 * @button: a #GtkScaleButton
 * @icons: (array zero-terminated=1): a %NULL-terminated array of icon names
 *
 * Sets the icons to be used by the scale button.
 * For details, see the #GtkScaleButton:icons property.
 *
 * Since: 2.12
 */
void
gtk_scale_button_set_icons (GtkScaleButton  *button,
			    const gchar    **icons)
{
  GtkScaleButtonPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  priv = button->priv;

  tmp = priv->icon_list;
  priv->icon_list = g_strdupv ((gchar **) icons);
  g_strfreev (tmp);
  gtk_scale_button_update_icon (button);

  g_object_notify (G_OBJECT (button), "icons");
}

/**
 * gtk_scale_button_get_adjustment:
 * @button: a #GtkScaleButton
 *
 * Gets the #GtkAdjustment associated with the #GtkScaleButton’s scale.
 * See gtk_range_get_adjustment() for details.
 *
 * Returns: (transfer none): the adjustment associated with the scale
 *
 * Since: 2.12
 */
GtkAdjustment*
gtk_scale_button_get_adjustment	(GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return button->priv->adjustment;
}

/**
 * gtk_scale_button_set_adjustment:
 * @button: a #GtkScaleButton
 * @adjustment: a #GtkAdjustment
 *
 * Sets the #GtkAdjustment to be used as a model
 * for the #GtkScaleButton’s scale.
 * See gtk_range_set_adjustment() for details.
 *
 * Since: 2.12
 */
void
gtk_scale_button_set_adjustment	(GtkScaleButton *button,
				 GtkAdjustment  *adjustment)
{
  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (button->priv->adjustment != adjustment)
    {
      if (button->priv->adjustment)
        g_object_unref (button->priv->adjustment);
      button->priv->adjustment = g_object_ref_sink (adjustment);

      if (button->priv->scale)
        gtk_range_set_adjustment (GTK_RANGE (button->priv->scale), adjustment);

      g_object_notify (G_OBJECT (button), "adjustment");
    }
}

/**
 * gtk_scale_button_get_plus_button:
 * @button: a #GtkScaleButton
 *
 * Retrieves the plus button of the #GtkScaleButton.
 *
 * Returns: (transfer none): the plus button of the #GtkScaleButton
 *
 * Since: 2.14
 */
GtkWidget *
gtk_scale_button_get_plus_button (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return button->priv->plus_button;
}

/**
 * gtk_scale_button_get_minus_button:
 * @button: a #GtkScaleButton
 *
 * Retrieves the minus button of the #GtkScaleButton.
 *
 * Returns: (transfer none): the minus button of the #GtkScaleButton
 *
 * Since: 2.14
 */
GtkWidget *
gtk_scale_button_get_minus_button (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return button->priv->minus_button;
}

/**
 * gtk_scale_button_get_popup:
 * @button: a #GtkScaleButton
 *
 * Retrieves the popup of the #GtkScaleButton.
 *
 * Returns: (transfer none): the popup of the #GtkScaleButton
 *
 * Since: 2.14
 */
GtkWidget *
gtk_scale_button_get_popup (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return button->priv->dock;
}

static void
apply_orientation (GtkScaleButton *button,
                   GtkOrientation  orientation)
{
  GtkScaleButtonPrivate *priv = button->priv;

  if (priv->applied_orientation != orientation)
    {
      priv->applied_orientation = orientation;
      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->box), orientation);
      gtk_container_child_set (GTK_CONTAINER (priv->box),
                               priv->plus_button,
                               "pack-type",
                               orientation == GTK_ORIENTATION_VERTICAL ?
                               GTK_PACK_START : GTK_PACK_END,
                               NULL);
      gtk_container_child_set (GTK_CONTAINER (priv->box),
                               priv->minus_button,
                               "pack-type",
                               orientation == GTK_ORIENTATION_VERTICAL ?
                               GTK_PACK_END : GTK_PACK_START,
                               NULL);

      gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->scale), orientation);

      if (orientation == GTK_ORIENTATION_VERTICAL)
        {
          gtk_widget_set_size_request (GTK_WIDGET (priv->scale), -1, SCALE_SIZE);
          gtk_range_set_inverted (GTK_RANGE (priv->scale), TRUE);
        }
      else
        {
          gtk_widget_set_size_request (GTK_WIDGET (priv->scale), SCALE_SIZE, -1);
          gtk_range_set_inverted (GTK_RANGE (priv->scale), FALSE);
        }
    }
}

static void
gtk_scale_button_set_orientation_private (GtkScaleButton *button,
                                          GtkOrientation  orientation)
{
  GtkScaleButtonPrivate *priv = button->priv;

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;
      g_object_notify (G_OBJECT (button), "orientation");
    }
}

/*
 * button callbacks.
 */

static gboolean
gtk_scale_button_scroll (GtkWidget      *widget,
			 GdkEventScroll *event)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;
  GtkAdjustment *adjustment;
  gdouble d;

  button = GTK_SCALE_BUTTON (widget);
  priv = button->priv;
  adjustment = priv->adjustment;

  if (event->type != GDK_SCROLL)
    return FALSE;

  d = gtk_scale_button_get_value (button);
  if (event->direction == GDK_SCROLL_UP)
    {
      d += gtk_adjustment_get_step_increment (adjustment);
      if (d > gtk_adjustment_get_upper (adjustment))
	d = gtk_adjustment_get_upper (adjustment);
    }
  else if (event->direction == GDK_SCROLL_DOWN)
    {
      d -= gtk_adjustment_get_step_increment (adjustment);
      if (d < gtk_adjustment_get_lower (adjustment))
	d = gtk_adjustment_get_lower (adjustment);
    }
  else if (event->direction == GDK_SCROLL_SMOOTH)
    {
      d += event->delta_y * gtk_adjustment_get_step_increment (adjustment);
      d = CLAMP (d, gtk_adjustment_get_lower (adjustment),
                 gtk_adjustment_get_upper (adjustment));
    }
  gtk_scale_button_set_value (button, d);

  return TRUE;
}

static gboolean
gtk_scale_popup (GtkWidget *widget)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (widget);
  GtkScaleButtonPrivate *priv = button->priv;
  GtkWidget *toplevel;
  GtkBorder border;
  GtkRequisition req;
  gint w, h;
  gint size;

  gtk_widget_show (priv->dock);

  toplevel = gtk_widget_get_toplevel (widget);
  _gtk_window_get_shadow_width (GTK_WINDOW (toplevel), &border);
  w = gtk_widget_get_allocated_width (toplevel) - border.left - border.right;
  h = gtk_widget_get_allocated_height (toplevel) - border.top - border.bottom;
  gtk_widget_get_preferred_size (priv->dock, NULL, &req);
  size = MAX (req.width, req.height);

  if (size > w)
    apply_orientation (button, GTK_ORIENTATION_VERTICAL);
  else if (size > h)
    apply_orientation (button, GTK_ORIENTATION_HORIZONTAL);
  else
    apply_orientation (button, priv->orientation);

  return TRUE;
}

static void
gtk_scale_button_popdown (GtkWidget *widget)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (widget);
  GtkScaleButtonPrivate *priv = button->priv;

  gtk_widget_hide (priv->dock);
}

static void
gtk_scale_button_clicked (GtkButton *button)
{
  gtk_scale_popup (GTK_WIDGET (button));
}

static void
gtk_scale_button_popup (GtkWidget *widget)
{
  gtk_scale_popup (widget);
}

/*
 * +/- button callbacks.
 */
static gboolean
button_click (GtkScaleButton *button,
              GtkWidget      *active)
{
  GtkScaleButtonPrivate *priv = button->priv;
  GtkAdjustment *adjustment = priv->adjustment;
  gboolean can_continue = TRUE;
  gdouble val;

  val = gtk_scale_button_get_value (button);

  if (active == priv->plus_button)
    val += gtk_adjustment_get_page_increment (adjustment);
  else
    val -= gtk_adjustment_get_page_increment (adjustment);

  if (val <= gtk_adjustment_get_lower (adjustment))
    {
      can_continue = FALSE;
      val = gtk_adjustment_get_lower (adjustment);
    }
  else if (val > gtk_adjustment_get_upper (adjustment))
    {
      can_continue = FALSE;
      val = gtk_adjustment_get_upper (adjustment);
    }

  gtk_scale_button_set_value (button, val);

  return can_continue;
}

static gboolean
cb_button_timeout (gpointer user_data)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (user_data);
  GtkScaleButtonPrivate *priv = button->priv;
  gboolean res;

  if (priv->click_id == 0)
    return G_SOURCE_REMOVE;

  res = button_click (button, priv->active_button);
  if (!res)
    {
      g_source_remove (priv->click_id);
      priv->click_id = 0;
    }

  return res;
}

static gboolean
cb_button_press (GtkWidget      *widget,
		 GdkEventButton *event,
		 gpointer        user_data)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (user_data);
  GtkScaleButtonPrivate *priv = button->priv;
  gint double_click_time;

  if (priv->click_id != 0)
    g_source_remove (priv->click_id);

  priv->active_button = widget;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-double-click-time", &double_click_time,
                NULL);
  priv->click_id = gdk_threads_add_timeout (double_click_time,
                                            cb_button_timeout,
                                            button);
  g_source_set_name_by_id (priv->click_id, "[gtk+] cb_button_timeout");
  cb_button_timeout (button);

  return TRUE;
}

static gboolean
cb_button_release (GtkWidget      *widget,
		   GdkEventButton *event,
		   gpointer        user_data)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (user_data);
  GtkScaleButtonPrivate *priv = button->priv;

  if (priv->click_id != 0)
    {
      g_source_remove (priv->click_id);
      priv->click_id = 0;
    }

  return TRUE;
}

static void
cb_button_clicked (GtkWidget *widget,
                   gpointer   user_data)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (user_data);
  GtkScaleButtonPrivate *priv = button->priv;

  if (priv->click_id != 0)
    return;

  button_click (button, widget);
}

static void
gtk_scale_button_update_icon (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = button->priv;
  GtkAdjustment *adjustment;
  gdouble value;
  const gchar *name;
  guint num_icons;

  if (!priv->icon_list || priv->icon_list[0] == '\0')
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                    "image-missing",
                                    priv->size);
      return;
    }

  num_icons = g_strv_length (priv->icon_list);

  /* The 1-icon special case */
  if (num_icons == 1)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                    priv->icon_list[0],
                                    priv->size);
      return;
    }

  adjustment = priv->adjustment;
  value = gtk_scale_button_get_value (button);

  /* The 2-icons special case */
  if (num_icons == 2)
    {
      gdouble limit;

      limit = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) / 2 + gtk_adjustment_get_lower (adjustment);
      if (value < limit)
        name = priv->icon_list[0];
      else
        name = priv->icon_list[1];

      gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                    name,
                                    priv->size);
      return;
    }

  /* With 3 or more icons */
  if (value == gtk_adjustment_get_lower (adjustment))
    {
      name = priv->icon_list[0];
    }
  else if (value == gtk_adjustment_get_upper (adjustment))
    {
      name = priv->icon_list[1];
    }
  else
    {
      gdouble step;
      guint i;

      step = (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) / (num_icons - 2); i = (guint) ((value - gtk_adjustment_get_lower (adjustment)) / step) + 2;
      g_assert (i < num_icons);
      name = priv->icon_list[i];
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                name,
                                priv->size);
}

static void
cb_scale_value_changed (GtkRange *range,
                        gpointer  user_data)
{
  GtkScaleButton *button = user_data;
  gdouble value;
  gdouble upper, lower;

  value = gtk_range_get_value (range);
  upper = gtk_adjustment_get_upper (button->priv->adjustment);
  lower = gtk_adjustment_get_lower (button->priv->adjustment);

  gtk_scale_button_update_icon (button);

  gtk_widget_set_sensitive (button->priv->plus_button, value < upper);
  gtk_widget_set_sensitive (button->priv->minus_button, lower < value);

  g_signal_emit (button, signals[VALUE_CHANGED], 0, value);
  g_object_notify (G_OBJECT (button), "value");
}

static void
cb_popup_mapped (GtkWidget *popup,
                 gpointer   user_data)
{
  GtkScaleButton *button = user_data;
  GtkScaleButtonPrivate *priv = button->priv;

  gtk_widget_grab_focus (priv->scale);
}
