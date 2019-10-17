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
#include "gtkwidgetprivate.h"
#include "gtksnapshot.h"
#include "gtkintl.h"

enum {
  PROP_INSPECTED = 1,
  PROP_RESIZE,
  PROP_MAGNIFICATION
};

typedef struct _GtkMagnifierPrivate GtkMagnifierPrivate;

struct _GtkMagnifierPrivate
{
  GdkPaintable *paintable;
  gdouble magnification;
  gint x;
  gint y;
  gboolean resize;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkMagnifier, gtk_magnifier,
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
    case PROP_RESIZE:
      _gtk_magnifier_set_resize (GTK_MAGNIFIER (object),
                                 g_value_get_boolean (value));
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
  priv = gtk_magnifier_get_instance_private (magnifier);

  switch (param_id)
    {
    case PROP_INSPECTED:
      g_value_set_object (value, gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (priv->paintable)));
      break;
    case PROP_MAGNIFICATION:
      g_value_set_double (value, priv->magnification);
      break;
    case PROP_RESIZE:
      g_value_set_boolean (value, priv->resize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_magnifier_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (widget);
  GtkMagnifierPrivate *priv = gtk_magnifier_get_instance_private (magnifier);
  double width, height, paintable_width, paintable_height;

  if (gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (priv->paintable)) == NULL)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  paintable_width = gdk_paintable_get_intrinsic_width (priv->paintable);
  paintable_height = gdk_paintable_get_intrinsic_height (priv->paintable);
  if (paintable_width <= 0.0 || paintable_height <= 0.0)
    return;

  gtk_snapshot_save (snapshot);
  if (!priv->resize)
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2, height / 2));
  gtk_snapshot_scale (snapshot, priv->magnification, priv->magnification);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
                          - CLAMP (priv->x, 0, paintable_width),
                          - CLAMP (priv->y, 0, paintable_height)));

  gdk_paintable_snapshot (priv->paintable, snapshot, paintable_width, paintable_height);
  gtk_snapshot_restore (snapshot);
}

static void
gtk_magnifier_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkMagnifier *magnifier = GTK_MAGNIFIER (widget);
  GtkMagnifierPrivate *priv = gtk_magnifier_get_instance_private (magnifier);
  gint size;

  if (priv->resize)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        size = priv->magnification * gdk_paintable_get_intrinsic_width (priv->paintable);
      else
        size = priv->magnification * gdk_paintable_get_intrinsic_height (priv->paintable);
    }
  else
    size = 0;

  *minimum = size;
  *natural = size;
}

static void
gtk_magnifier_dispose (GObject *object)
{
  GtkMagnifier *self = GTK_MAGNIFIER (object);
  GtkMagnifierPrivate *priv = gtk_magnifier_get_instance_private (self);

  _gtk_magnifier_set_inspected (self, NULL);
  g_clear_object (&priv->paintable);

  G_OBJECT_CLASS (gtk_magnifier_parent_class)->dispose (object);
}

static void
gtk_magnifier_class_init (GtkMagnifierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = _gtk_magnifier_set_property;
  object_class->get_property = _gtk_magnifier_get_property;
  object_class->dispose = gtk_magnifier_dispose;

  widget_class->snapshot = gtk_magnifier_snapshot;
  widget_class->measure = gtk_magnifier_measure;

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
  g_object_class_install_property (object_class,
                                   PROP_RESIZE,
                                   g_param_spec_boolean ("resize",
                                                         P_("resize"),
                                                         P_("resize"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
gtk_magnifier_init (GtkMagnifier *self)
{
  GtkMagnifierPrivate *priv = gtk_magnifier_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  priv->magnification = 1;
  priv->resize = FALSE;
  priv->paintable = gtk_widget_paintable_new (NULL);
  g_signal_connect_swapped (priv->paintable, "invalidate-contents", G_CALLBACK (gtk_widget_queue_draw), self);
  g_signal_connect_swapped (priv->paintable, "invalidate-size", G_CALLBACK (gtk_widget_queue_resize), self);
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
  GtkMagnifierPrivate *priv = gtk_magnifier_get_instance_private (magnifier);

  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), NULL);

  return gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (priv->paintable));
}

void
_gtk_magnifier_set_inspected (GtkMagnifier *magnifier,
                              GtkWidget    *inspected)
{
  GtkMagnifierPrivate *priv = gtk_magnifier_get_instance_private (magnifier);

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));
  g_return_if_fail (inspected == NULL || GTK_IS_WIDGET (inspected));

  gtk_widget_paintable_set_widget (GTK_WIDGET_PAINTABLE (priv->paintable), inspected);

  g_object_notify (G_OBJECT (magnifier), "inspected");
}

void
_gtk_magnifier_set_coords (GtkMagnifier *magnifier,
                           gdouble       x,
                           gdouble       y)
{
  GtkMagnifierPrivate *priv;

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  priv = gtk_magnifier_get_instance_private (magnifier);

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

  priv = gtk_magnifier_get_instance_private (magnifier);

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

  priv = gtk_magnifier_get_instance_private (magnifier);

  if (priv->magnification == magnification)
    return;

  priv->magnification = magnification;
  g_object_notify (G_OBJECT (magnifier), "magnification");

  if (priv->resize)
    gtk_widget_queue_resize (GTK_WIDGET (magnifier));

  if (gtk_widget_is_visible (GTK_WIDGET (magnifier)))
    gtk_widget_queue_draw (GTK_WIDGET (magnifier));
}

gdouble
_gtk_magnifier_get_magnification (GtkMagnifier *magnifier)
{
  GtkMagnifierPrivate *priv;

  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), 1);

  priv = gtk_magnifier_get_instance_private (magnifier);

  return priv->magnification;
}

void
_gtk_magnifier_set_resize (GtkMagnifier *magnifier,
                           gboolean      resize)
{
  GtkMagnifierPrivate *priv;

  g_return_if_fail (GTK_IS_MAGNIFIER (magnifier));

  priv = gtk_magnifier_get_instance_private (magnifier);

  if (priv->resize == resize)
    return;

  priv->resize = resize;

  gtk_widget_queue_resize (GTK_WIDGET (magnifier));
}

gboolean
_gtk_magnifier_get_resize (GtkMagnifier *magnifier)
{
  GtkMagnifierPrivate *priv;

  g_return_val_if_fail (GTK_IS_MAGNIFIER (magnifier), FALSE);

  priv = gtk_magnifier_get_instance_private (magnifier);

  return priv->resize;
}
