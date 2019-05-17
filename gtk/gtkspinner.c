/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2007 John Stowers, Neil Jagdish Patel.
 * Copyright (C) 2009 Bastien Nocera, David Zeuthen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Code adapted from egg-spinner
 * by Christian Hergert <christian.hergert@gmail.com>
 */

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkspinner.h"

#include "gtkimage.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "a11y/gtkspinneraccessible.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkrendericonprivate.h"


/**
 * SECTION:gtkspinner
 * @Short_description: Show a spinner animation
 * @Title: GtkSpinner
 * @See_also: #GtkCellRendererSpinner, #GtkProgressBar
 *
 * A GtkSpinner widget displays an icon-size spinning animation.
 * It is often used as an alternative to a #GtkProgressBar for
 * displaying indefinite activity, instead of actual progress.
 *
 * To start the animation, use gtk_spinner_start(), to stop it
 * use gtk_spinner_stop().
 *
 * # CSS nodes
 *
 * GtkSpinner has a single CSS node with the name spinner. When the animation is
 * active, the :checked pseudoclass is added to this node.
 */

typedef struct _GtkSpinnerClass GtkSpinnerClass;

struct _GtkSpinner
{
  GtkWidget parent;
};

struct _GtkSpinnerClass
{
  GtkWidgetClass parent_class;
};


enum {
  PROP_0,
  PROP_ACTIVE
};

G_DEFINE_TYPE (GtkSpinner, gtk_spinner, GTK_TYPE_WIDGET)

#define DEFAULT_SIZE 16

static void
gtk_spinner_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  double min_size;
  guint property;
  GtkCssStyle *style;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    property = GTK_CSS_PROPERTY_MIN_WIDTH;
  else
    property = GTK_CSS_PROPERTY_MIN_HEIGHT;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  min_size = _gtk_css_number_value_get (gtk_css_style_get_value (style, property), 100);

  if (min_size > 0.0)
    *minimum = *natural = min_size;
  else
    *minimum = *natural = DEFAULT_SIZE;
}

static void
gtk_spinner_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));

  gtk_css_style_snapshot_icon (style,
                               snapshot,
                               gtk_widget_get_width (widget),
                               gtk_widget_get_height (widget),
                               GTK_CSS_IMAGE_BUILTIN_SPINNER);
}

static gboolean
gtk_spinner_get_active (GtkSpinner *spinner)
{
  return (gtk_widget_get_state_flags ((GtkWidget *)spinner) & GTK_STATE_FLAG_CHECKED) > 0;
}

static void
gtk_spinner_set_active (GtkSpinner *spinner,
                        gboolean    active)
{
  gboolean current_active = gtk_spinner_get_active (spinner);

  active = !!active;

  if (active != current_active)
    {
      g_object_notify (G_OBJECT (spinner), "active");

      if (active)
        gtk_widget_set_state_flags (GTK_WIDGET (spinner),
                                    GTK_STATE_FLAG_CHECKED, FALSE);
      else
        gtk_widget_unset_state_flags (GTK_WIDGET (spinner),
                                      GTK_STATE_FLAG_CHECKED);
    }
}

static void
gtk_spinner_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (param_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, gtk_spinner_get_active (GTK_SPINNER (object)));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (param_id)
    {
      case PROP_ACTIVE:
        gtk_spinner_set_active (GTK_SPINNER (object), g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_class_init (GtkSpinnerClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->get_property = gtk_spinner_get_property;
  gobject_class->set_property = gtk_spinner_set_property;

  widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->snapshot = gtk_spinner_snapshot;
  widget_class->measure = gtk_spinner_measure;

  /* GtkSpinner:active:
   *
   * Whether the spinner is active
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the spinner is active"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SPINNER_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("spinner"));
}

static void
gtk_spinner_init (GtkSpinner *spinner)
{
}

/**
 * gtk_spinner_new:
 *
 * Returns a new spinner widget. Not yet started.
 *
 * Returns: a new #GtkSpinner
 */
GtkWidget *
gtk_spinner_new (void)
{
  return g_object_new (GTK_TYPE_SPINNER, NULL);
}

/**
 * gtk_spinner_start:
 * @spinner: a #GtkSpinner
 *
 * Starts the animation of the spinner.
 */
void
gtk_spinner_start (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_active (spinner, TRUE);
}

/**
 * gtk_spinner_stop:
 * @spinner: a #GtkSpinner
 *
 * Stops the animation of the spinner.
 */
void
gtk_spinner_stop (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_active (spinner, FALSE);
}
