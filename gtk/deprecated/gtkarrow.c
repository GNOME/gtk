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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkarrow
 * @Short_description: Displays an arrow
 * @Title: GtkArrow
 * @See_also: gtk_render_arrow()
 *
 * GtkArrow should be used to draw simple arrows that need to point in
 * one of the four cardinal directions (up, down, left, or right).  The
 * style of the arrow can be one of shadow in, shadow out, etched in, or
 * etched out.  Note that these directions and style types may be
 * amended in versions of GTK+ to come.
 *
 * GtkArrow will fill any space alloted to it, but since it is inherited
 * from #GtkMisc, it can be padded and/or aligned, to fill exactly the
 * space the programmer desires.
 *
 * Arrows are created with a call to gtk_arrow_new().  The direction or
 * style of an arrow can be changed after creation by using gtk_arrow_set().
 *
 * GtkArrow has been deprecated; you can simply use a #GtkImage with a
 * suitable icon name, such as “pan-down-symbolic“. When replacing 
 * GtkArrow by an image, pay attention to the fact that GtkArrow is
 * doing automatic flipping between #GTK_ARROW_LEFT and #GTK_ARROW_RIGHT,
 * depending on the text direction. To get the same effect with an image,
 * use the icon names “pan-start-symbolic“ and “pan-end-symbolic“, which
 * react to the text direction.
 */

#include "config.h"
#include <math.h>
#include "gtkarrow.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "a11y/gtkarrowaccessible.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define MIN_ARROW_SIZE  15

struct _GtkArrowPrivate
{
  gint16 arrow_type;
  gint16 shadow_type;
};

enum {
  PROP_0,
  PROP_ARROW_TYPE,
  PROP_SHADOW_TYPE
};


static void     gtk_arrow_set_property (GObject        *object,
                                        guint           prop_id,
                                        const GValue   *value,
                                        GParamSpec     *pspec);
static void     gtk_arrow_get_property (GObject        *object,
                                        guint           prop_id,
                                        GValue         *value,
                                        GParamSpec     *pspec);
static gboolean gtk_arrow_draw         (GtkWidget      *widget,
                                        cairo_t        *cr);

static void     gtk_arrow_get_preferred_width         (GtkWidget           *widget,
                                                       gint                *minimum_size,
                                                       gint                *natural_size);
static void     gtk_arrow_get_preferred_height        (GtkWidget           *widget,
                                                       gint                *minimum_size,
                                                       gint                *natural_size);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
G_DEFINE_TYPE_WITH_PRIVATE (GtkArrow, gtk_arrow, GTK_TYPE_MISC)
G_GNUC_END_IGNORE_DEPRECATIONS

static void
gtk_arrow_class_init (GtkArrowClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_arrow_set_property;
  gobject_class->get_property = gtk_arrow_get_property;

  widget_class->draw = gtk_arrow_draw;
  widget_class->get_preferred_width  = gtk_arrow_get_preferred_width;
  widget_class->get_preferred_height = gtk_arrow_get_preferred_height;

  g_object_class_install_property (gobject_class,
                                   PROP_ARROW_TYPE,
                                   g_param_spec_enum ("arrow-type",
                                                      P_("Arrow direction"),
                                                      P_("The direction the arrow should point"),
						      GTK_TYPE_ARROW_TYPE,
						      GTK_ARROW_RIGHT,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Arrow shadow"),
                                                      P_("Appearance of the shadow surrounding the arrow"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_float ("arrow-scaling",
                                                               P_("Arrow Scaling"),
                                                               P_("Amount of space used up by arrow"),
                                                               0.0, 1.0, 0.7,
                                                               GTK_PARAM_READABLE));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_ARROW_ACCESSIBLE);
}

static void
gtk_arrow_set_property (GObject         *object,
			guint            prop_id,
			const GValue    *value,
			GParamSpec      *pspec)
{
  GtkArrow *arrow = GTK_ARROW (object);
  GtkArrowPrivate *priv = arrow->priv;

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      gtk_arrow_set (arrow,
		     g_value_get_enum (value),
		     priv->shadow_type);
      break;
    case PROP_SHADOW_TYPE:
      gtk_arrow_set (arrow,
		     priv->arrow_type,
		     g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_arrow_get_property (GObject         *object,
			guint            prop_id,
			GValue          *value,
			GParamSpec      *pspec)
{
  GtkArrow *arrow = GTK_ARROW (object);
  GtkArrowPrivate *priv = arrow->priv;

  switch (prop_id)
    {
    case PROP_ARROW_TYPE:
      g_value_set_enum (value, priv->arrow_type);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_arrow_init (GtkArrow *arrow)
{
  arrow->priv = gtk_arrow_get_instance_private (arrow);

  gtk_widget_set_has_window (GTK_WIDGET (arrow), FALSE);

  arrow->priv->arrow_type = GTK_ARROW_RIGHT;
  arrow->priv->shadow_type = GTK_SHADOW_OUT;
}

static void
gtk_arrow_get_preferred_width (GtkWidget *widget,
                               gint      *minimum_size,
                               gint      *natural_size)
{
  GtkBorder border;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _gtk_misc_get_padding_and_border (GTK_MISC (widget), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

  *minimum_size = MIN_ARROW_SIZE + border.left + border.right;
  *natural_size = MIN_ARROW_SIZE + border.left + border.right;
}

static void
gtk_arrow_get_preferred_height (GtkWidget *widget,
                                gint      *minimum_size,
                                gint      *natural_size)
{
  GtkBorder border;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _gtk_misc_get_padding_and_border (GTK_MISC (widget), &border);
G_GNUC_END_IGNORE_DEPRECATIONS

  *minimum_size = MIN_ARROW_SIZE + border.top + border.bottom;
  *natural_size = MIN_ARROW_SIZE + border.top + border.bottom;
}

/**
 * gtk_arrow_new:
 * @arrow_type: a valid #GtkArrowType.
 * @shadow_type: a valid #GtkShadowType.
 *
 * Creates a new #GtkArrow widget.
 *
 * Returns: the new #GtkArrow widget.
 *
 * Deprecated: 3.14: Use a #GtkImage with a suitable icon.
 */
GtkWidget*
gtk_arrow_new (GtkArrowType  arrow_type,
	       GtkShadowType shadow_type)
{
  GtkArrowPrivate *priv;
  GtkArrow *arrow;

  arrow = g_object_new (GTK_TYPE_ARROW, NULL);

  priv = arrow->priv;

  priv->arrow_type = arrow_type;
  priv->shadow_type = shadow_type;

  return GTK_WIDGET (arrow);
}

/**
 * gtk_arrow_set:
 * @arrow: a widget of type #GtkArrow.
 * @arrow_type: a valid #GtkArrowType.
 * @shadow_type: a valid #GtkShadowType.
 *
 * Sets the direction and style of the #GtkArrow, @arrow.
 *
 * Deprecated: 3.14: Use a #GtkImage with a suitable icon.
 */
void
gtk_arrow_set (GtkArrow      *arrow,
	       GtkArrowType   arrow_type,
	       GtkShadowType  shadow_type)
{
  GtkArrowPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_ARROW (arrow));

  priv = arrow->priv;

  if (priv->arrow_type != arrow_type
      || priv->shadow_type != shadow_type)
    {
      g_object_freeze_notify (G_OBJECT (arrow));

      if ((GtkArrowType) priv->arrow_type != arrow_type)
        {
          priv->arrow_type = arrow_type;
          g_object_notify (G_OBJECT (arrow), "arrow-type");
        }

      if (priv->shadow_type != shadow_type)
        {
          priv->shadow_type = shadow_type;
          g_object_notify (G_OBJECT (arrow), "shadow-type");
        }

      g_object_thaw_notify (G_OBJECT (arrow));

      widget = GTK_WIDGET (arrow);
      if (gtk_widget_is_drawable (widget))
	gtk_widget_queue_draw (widget);
    }
}

static gboolean
gtk_arrow_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkArrow *arrow = GTK_ARROW (widget);
  GtkArrowPrivate *priv = arrow->priv;
  GtkStyleContext *context;
  gdouble x, y;
  gint width, height;
  gint extent;
  GtkBorder border;
  gfloat xalign, yalign;
  GtkArrowType effective_arrow_type;
  gfloat arrow_scaling;
  gdouble angle;

  if (priv->arrow_type == GTK_ARROW_NONE)
    return FALSE;

  context = gtk_widget_get_style_context (widget);
  gtk_widget_style_get (widget, "arrow-scaling", &arrow_scaling, NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _gtk_misc_get_padding_and_border (GTK_MISC (widget), &border);
  gtk_misc_get_alignment (GTK_MISC (widget), &xalign, &yalign);
G_GNUC_END_IGNORE_DEPRECATIONS

  width = gtk_widget_get_allocated_width (widget) - border.left - border.right;
  height = gtk_widget_get_allocated_height (widget) - border.top - border.bottom;

  extent = MIN (width, height) * arrow_scaling;
  effective_arrow_type = priv->arrow_type;

  if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    {
      xalign = 1.0 - xalign;
      if (priv->arrow_type == GTK_ARROW_LEFT)
        effective_arrow_type = GTK_ARROW_RIGHT;
      else if (priv->arrow_type == GTK_ARROW_RIGHT)
        effective_arrow_type = GTK_ARROW_LEFT;
    }

  x = border.left + ((width - extent) * xalign);
  y = border.top + ((height - extent) * yalign);

  switch (effective_arrow_type)
    {
    case GTK_ARROW_UP:
      angle = 0;
      break;
    case GTK_ARROW_RIGHT:
      angle = G_PI / 2;
      break;
    case GTK_ARROW_DOWN:
      angle = G_PI;
      break;
    case GTK_ARROW_LEFT:
    default:
      angle = (3 * G_PI) / 2;
      break;
    }

  gtk_render_arrow (context, cr, angle, x, y, extent);

  return FALSE;
}
