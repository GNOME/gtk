/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"
#include "gtk/gtk.h"
#include "gtkmagnifierprivate.h"
#include "gtkintl.h"

enum {
  PROP_INSPECTED = 1,
  PROP_MAGNIFICATION
};

typedef struct _GtkMagnifierPrivate GtkMagnifierPrivate;

struct _GtkMagnifierPrivate
{
  GtkWidget *inspected;
  gdouble magnification;
  gint x;
  gint y;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkMagnifier, _gtk_magnifier,
                            GTK_TYPE_WIDGET)

static void
_gtk_magnifier_set_property (GObject      *object,
                             guint         param_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (param_id)
    {
    case PROP_INSPECTED:
      _gtk_magnifier_set_inspected (GTK_MAGNIFIER (object),
                                    g_value_get_object (value));
      break;
    case PROP_MAGNIFICATION:
      _gtk_magnifier_set_magnification (GTK_MAGNIFIER (object),
                                        g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
_gtk_magnifier_get_property (GObject    *object,
                             guint       param_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkMagnifier *magnifier;
  GtkMagnifierPrivate *priv;

  magnifier = GTK_MAGNIFIER (object);
  priv = _gtk_magnifier_get_instance_private (magnifier);

  switch (param_id)
    {
    case PROP_INSPECTED:
      g_value_set_object (value, priv->inspected);
      break;
    case PROP_MAGNIFICATION:
      g_value_set_double (value, priv->magnification);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static gboolean
_gtk_magnifier_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkAllocation allocation, inspected_alloc;
  GtkMagnifier *magnifier;
  GtkMagnifierPrivate *priv;
  gdouble x, y;

  magnifier = GTK_MAGNIFIER (widget);
  priv = _gtk_magnifier_get_instance_private (magnifier);

  if (!gtk_widget_is_visible (priv->inspected))
    return FALSE;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_get_allocation (priv->inspected, &inspected_alloc);
  cairo_translate (cr, allocation.width / 2, allocation.height / 2);

  x = CLAMP (priv->x, 0, inspected_alloc.width);
  y = CLAMP (priv->y, 0, inspected_alloc.height);

  cairo_save (cr);
  cairo_scale (cr, priv->magnification, priv->magnification);
  cairo_translate (cr, -x, -y);
  gtk_widget_draw (priv->inspected, cr);
  cairo_restore (cr);

  return TRUE;
}

static void
_gtk_magnifier_class_init (GtkMagnifierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = _gtk_magnifier_set_property;
  object_class->get_property = _gtk_magnifier_get_property;

  widget_class->draw = _gtk_magnifier_draw;

  g_object_class_install_property (object_class,
                                   PROP_INSPECTED,
                                   g_param_spec_object ("inspected",
                                                        P_("Inspected"),
                                                        P_("Inspected widget"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_MAGNIFICATION,
                                   g_param_spec_double ("magnification",
                                                        P_("magnification"),
                                                        P_("magnification"),
                                                        1, G_MAXDOUBLE, 1,
                                                        G_PARAM_READWRITE));
}

static void
_gtk_magnifier_init (GtkMagnifier *magnifier)
{
  GtkWidget *widget = GTK_WIDGET (magnifier);
  GtkMagnifierPrivate *priv;

  priv = _gtk_magnifier_get_instance_private (magnifier);

  gtk_widget_set_events (widget,
                         gtk_widget_get_events (widget) |
                         GDK_EXPOSURE_MASK);

  gtk_widget_set_has_window (widget, FALSE);
  priv->magnification = 1;
}

GtkWidget *
_gtk_magnifier_new (GtkWidget *inspected)
{
  g_return_val_if_fail (GTK_IS_WIDGET (inspected), NULL);

  return g_object_new (GTK_TYPE_MAGNIFIER,
                       "inspected", inspected,
                       NULL);
}

GtkWidget *
_gtk_magnifier_get_inspected (GtkMagnifier *magnifier)
{
  GtkMagnifierPrivate *priv;

  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), NULL);

  priv = _gtk_magnifier_get_instance_private (magnifier);

  return priv->inspected;
}

void
_gtk_magnifier_set_inspected (GtkMagnifier *magnifier,
                              GtkWidget    *inspected)
{
  GtkMagnifierPrivate *priv;

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  priv = _gtk_magnifier_get_instance_private (magnifier);

  if (priv->inspected == inspected)
    return;

  priv->inspected = inspected;
  g_object_notify (G_OBJECT (magnifier), "inspected");
}

void
_gtk_magnifier_set_coords (GtkMagnifier *magnifier,
                           gdouble       x,
                           gdouble       y)
{
  GtkMagnifierPrivate *priv;

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  priv = _gtk_magnifier_get_instance_private (magnifier);

  if (priv->x == x && priv->y == y)
    return;

  priv->x = x;
  priv->y = y;

  if (gtk_widget_is_visible (GTK_WIDGET (magnifier)))
    gtk_widget_queue_draw (GTK_WIDGET (magnifier));
}

void
_gtk_magnifier_get_coords (GtkMagnifier *magnifier,
                           gdouble      *x,
                           gdouble      *y)
{
  GtkMagnifierPrivate *priv;

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  priv = _gtk_magnifier_get_instance_private (magnifier);

  if (x)
    *x = priv->x;

  if (y)
    *y = priv->y;
}

void
_gtk_magnifier_set_magnification (GtkMagnifier *magnifier,
                                  gdouble       magnification)
{
  GtkMagnifierPrivate *priv;

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  priv = _gtk_magnifier_get_instance_private (magnifier);

  if (priv->magnification == magnification)
    return;

  priv->magnification = magnification;
  g_object_notify (G_OBJECT (magnifier), "magnification");

  if (gtk_widget_is_visible (GTK_WIDGET (magnifier)))
    gtk_widget_queue_draw (GTK_WIDGET (magnifier));
}

gdouble
_gtk_magnifier_get_magnification (GtkMagnifier *magnifier)
{
  GtkMagnifierPrivate *priv;

  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), 1);

  priv = _gtk_magnifier_get_instance_private (magnifier);

  return priv->magnification;
}
