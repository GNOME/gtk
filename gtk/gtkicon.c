
/*
 * Copyright © 2015 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#include "config.h"

#include "gtkbuiltiniconprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkiconprivate.h"
#include "gtkwidgetprivate.h"

/* GtkIcon is a minimal widget wrapped around a GtkBuiltinIcon gadget,
 * It should be used whenever builtin-icon functionality is desired
 * but a widget is needed for other reasons.
 */
enum {
  PROP_0,
  PROP_CSS_NAME,
  NUM_PROPERTIES
};

static GParamSpec *icon_props[NUM_PROPERTIES] = { NULL, };

typedef struct _GtkIconPrivate GtkIconPrivate;
struct _GtkIconPrivate {
  GtkCssGadget *gadget;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkIcon, gtk_icon, GTK_TYPE_WIDGET)

static void
gtk_icon_finalize (GObject *object)
{
  GtkIcon *self = GTK_ICON (object);
  GtkIconPrivate *priv = gtk_icon_get_instance_private (self);

  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_icon_parent_class)->finalize (object);
}

static void
gtk_icon_get_property (GObject      *object,
                       guint         property_id,
                       GValue       *value,
                       GParamSpec   *pspec)
{
  GtkIcon *self = GTK_ICON (object);

  switch (property_id)
    {
    case PROP_CSS_NAME:
      g_value_set_string (value, gtk_icon_get_css_name (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_icon_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkIcon *self = GTK_ICON (object);

  switch (property_id)
    {
    case PROP_CSS_NAME:
      gtk_icon_set_css_name (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_icon_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
                                                      gint       for_width,
                                                      gint      *minimum,
                                                      gint      *natural,
                                                      gint      *minimum_baseline,
                                                      gint      *natural_baseline)
{
  GtkIcon *self = GTK_ICON (widget);
  GtkIconPrivate *priv = gtk_icon_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     for_width,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_icon_get_preferred_height (GtkWidget *widget,
                               gint      *minimum,
                               gint      *natural)
{
  gtk_icon_get_preferred_height_and_baseline_for_width (widget, -1,
                                                        minimum, natural,
                                                        NULL, NULL);
}

static void
gtk_icon_get_preferred_width (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
  GtkIcon *self = GTK_ICON (widget);
  GtkIconPrivate *priv = gtk_icon_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_icon_size_allocate (GtkWidget     *widget,
                        GtkAllocation *allocation)
{
  GtkIcon *self = GTK_ICON (widget);
  GtkIconPrivate *priv = gtk_icon_get_instance_private (self);
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);
  gtk_css_gadget_allocate (priv->gadget, allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static gboolean
gtk_icon_draw (GtkWidget *widget,
               cairo_t   *cr)
{
  GtkIcon *self = GTK_ICON (widget);
  GtkIconPrivate *priv = gtk_icon_get_instance_private (self);

  gtk_css_gadget_draw (priv->gadget, cr);

  return FALSE;
}

static void
gtk_icon_class_init (GtkIconClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  oclass->get_property = gtk_icon_get_property;
  oclass->set_property = gtk_icon_set_property;
  oclass->finalize = gtk_icon_finalize;

  wclass->size_allocate = gtk_icon_size_allocate;
  wclass->get_preferred_width = gtk_icon_get_preferred_width;
  wclass->get_preferred_height = gtk_icon_get_preferred_height;
  wclass->get_preferred_height_and_baseline_for_width = gtk_icon_get_preferred_height_and_baseline_for_width;
  wclass->draw = gtk_icon_draw;

  icon_props[PROP_CSS_NAME] =
    g_param_spec_string ("css-name", "CSS name",
                         "CSS name",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, NUM_PROPERTIES, icon_props);
}

static void
gtk_icon_init (GtkIcon *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkIconPrivate *priv = gtk_icon_get_instance_private (self);
  GtkCssNode *widget_node;

  gtk_widget_set_has_window (widget, FALSE);

  widget_node = gtk_widget_get_css_node (widget);
  priv->gadget = gtk_builtin_icon_new_for_node (widget_node, widget);
}

GtkWidget *
gtk_icon_new (const char *css_name)
{
  return g_object_new (GTK_TYPE_ICON,
                       "css-name", css_name,
                       NULL);
}

const char *
gtk_icon_get_css_name (GtkIcon *self)
{
  GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  return gtk_css_node_get_name (widget_node);
}

void
gtk_icon_set_css_name (GtkIcon    *self,
                       const char *css_name)
{
  GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  gtk_css_node_set_name (widget_node, g_intern_string (css_name));
}
