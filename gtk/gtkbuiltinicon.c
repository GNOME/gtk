/*
 * Copyright © 2015 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkbuiltiniconprivate.h"

#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkrendericonprivate.h"

/* GtkBuiltinIcon is a gadget implementation that is meant to replace
 * all calls to gtk_render_ functions to render arrows, expanders, checks
 * radios, handles, separators, etc. See the GtkCssImageBuiltinType
 * enumeration for the full set of builtin icons that this gadget can
 * render.
 *
 * Use gtk_builtin_icon_set_image to set which of the builtin icons
 * is rendered.
 *
 * Use gtk_builtin_icon_set_default_size to set a non-zero default
 * size for the icon. If you need to support a legacy size style property,
 * use gtk_builtin_icon_set_default_size_property.
 *
 * Themes can override the acutal image that is used with the
 * -gtk-icon-source property. If it is not specified, a builtin
 * fallback is used.
 */

typedef struct _GtkBuiltinIconPrivate GtkBuiltinIconPrivate;
struct _GtkBuiltinIconPrivate {
  GtkCssImageBuiltinType        image_type;
  int                           default_size;
  int                           strikethrough;
  gboolean                      strikethrough_valid;
  char *                        default_size_property;
};

G_DEFINE_TYPE_WITH_CODE (GtkBuiltinIcon, gtk_builtin_icon, GTK_TYPE_CSS_GADGET,
                         G_ADD_PRIVATE (GtkBuiltinIcon))

static void
gtk_builtin_icon_get_preferred_size (GtkCssGadget   *gadget,
                                     GtkOrientation  orientation,
                                     gint            for_size,
                                     gint           *minimum,
                                     gint           *natural,
                                     gint           *minimum_baseline,
                                     gint           *natural_baseline)
{
  GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (gadget));
  double min_size;
  guint property;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    property = GTK_CSS_PROPERTY_MIN_WIDTH;
  else
    property = GTK_CSS_PROPERTY_MIN_HEIGHT;
  min_size = _gtk_css_number_value_get (gtk_css_style_get_value (gtk_css_gadget_get_style (gadget), property), 100);
  if (min_size > 0.0)
    {
      *minimum = *natural = min_size;
    }
  else if (priv->default_size_property)
    {
      GValue value = G_VALUE_INIT;

      /* Do it a bit more complicated here so we get warnings when
       * somebody sets a non-int proerty.
       */
      g_value_init (&value, G_TYPE_INT);
      gtk_widget_style_get_property (gtk_css_gadget_get_owner (gadget),
                                     priv->default_size_property,
                                     &value);
      *minimum = *natural = g_value_get_int (&value);
      g_value_unset (&value);
    }
  else
    {
      *minimum = *natural = priv->default_size;
    }

  if (minimum_baseline)
    {
      if (!priv->strikethrough_valid)
        {
          GtkWidget *widget;
          PangoContext *pango_context;
          const PangoFontDescription *font_desc;
          PangoFontMetrics *metrics;

          widget = gtk_css_gadget_get_owner (gadget);
          pango_context = gtk_widget_get_pango_context (widget);
          font_desc = pango_context_get_font_description (pango_context);

          metrics = pango_context_get_metrics (pango_context,
                                               font_desc,
                                               pango_context_get_language (pango_context));
          priv->strikethrough = pango_font_metrics_get_strikethrough_position (metrics);
          priv->strikethrough_valid = TRUE;

          pango_font_metrics_unref (metrics);
        }

      *minimum_baseline = *minimum * 0.5 + PANGO_PIXELS (priv->strikethrough);
    }

  if (natural_baseline)
    *natural_baseline = *minimum_baseline;
}

static void
gtk_builtin_icon_allocate (GtkCssGadget        *gadget,
                           const GtkAllocation *allocation,
                           int                  baseline,
                           GtkAllocation       *out_clip)
{
  GdkRectangle icon_clip;

  GTK_CSS_GADGET_CLASS (gtk_builtin_icon_parent_class)->allocate (gadget, allocation, baseline, out_clip);

  gtk_css_style_render_icon_get_extents (gtk_css_gadget_get_style (gadget),
                                         &icon_clip,
                                         allocation->x, allocation->y,
                                         allocation->width, allocation->height);
  gdk_rectangle_union (out_clip, &icon_clip, out_clip);
}

static gboolean
gtk_builtin_icon_draw (GtkCssGadget *gadget,
                       cairo_t      *cr,
                       int           x,
                       int           y,
                       int           width,
                       int           height)
{
  GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (gadget));

  gtk_css_style_render_icon (gtk_css_gadget_get_style (gadget),
                             cr,
                             x, y,
                             width, height,
                             priv->image_type);

  return FALSE;
}

static void
gtk_builtin_icon_style_changed (GtkCssGadget      *gadget,
                                GtkCssStyleChange *change)
{
  GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (gadget));

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_FONT))
    priv->strikethrough_valid = FALSE;

  GTK_CSS_GADGET_CLASS (gtk_builtin_icon_parent_class)->style_changed (gadget, change);
}

static void
gtk_builtin_icon_finalize (GObject *object)
{
  GtkBuiltinIconPrivate *priv = gtk_builtin_icon_get_instance_private (GTK_BUILTIN_ICON (object));

  g_free (priv->default_size_property);

  G_OBJECT_CLASS (gtk_builtin_icon_parent_class)->finalize (object);
}

static void
gtk_builtin_icon_class_init (GtkBuiltinIconClass *klass)
{
  GtkCssGadgetClass *gadget_class = GTK_CSS_GADGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_builtin_icon_finalize;

  gadget_class->get_preferred_size = gtk_builtin_icon_get_preferred_size;
  gadget_class->allocate = gtk_builtin_icon_allocate;
  gadget_class->draw = gtk_builtin_icon_draw;
  gadget_class->style_changed = gtk_builtin_icon_style_changed;
}

static void
gtk_builtin_icon_init (GtkBuiltinIcon *custom_gadget)
{
}

GtkCssGadget *
gtk_builtin_icon_new_for_node (GtkCssNode *node,
                               GtkWidget  *owner)
{
  return g_object_new (GTK_TYPE_BUILTIN_ICON,
                       "node", node,
                       "owner", owner,
                       NULL);
}

GtkCssGadget *
gtk_builtin_icon_new (const char   *name,
                      GtkWidget    *owner,
                      GtkCssGadget *parent,
                      GtkCssGadget *next_sibling)
{
  GtkCssNode *node;
  GtkCssGadget *result;

  node = gtk_css_node_new ();
  gtk_css_node_set_name (node, g_intern_string (name));
  if (parent)
    gtk_css_node_insert_before (gtk_css_gadget_get_node (parent),
                                node,
                                next_sibling ? gtk_css_gadget_get_node (next_sibling) : NULL);

  result = gtk_builtin_icon_new_for_node (node, owner);

  g_object_unref (node);

  return result;
}

void
gtk_builtin_icon_set_image (GtkBuiltinIcon         *icon,
                            GtkCssImageBuiltinType  image)
{
  GtkBuiltinIconPrivate *priv;
  
  g_return_if_fail (GTK_IS_BUILTIN_ICON (icon));

  priv = gtk_builtin_icon_get_instance_private (icon);

  if (priv->image_type != image)
    {
      priv->image_type = image;
      gtk_widget_queue_draw (gtk_css_gadget_get_owner (GTK_CSS_GADGET (icon)));
    }
}

GtkCssImageBuiltinType
gtk_builtin_icon_get_image (GtkBuiltinIcon *icon)
{
  GtkBuiltinIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUILTIN_ICON (icon), GTK_CSS_IMAGE_BUILTIN_NONE);

  priv = gtk_builtin_icon_get_instance_private (icon);

  return priv->image_type;
}

void
gtk_builtin_icon_set_default_size (GtkBuiltinIcon *icon,
                                   int             default_size)
{
  GtkBuiltinIconPrivate *priv;
  
  g_return_if_fail (GTK_IS_BUILTIN_ICON (icon));

  priv = gtk_builtin_icon_get_instance_private (icon);

  if (priv->default_size != default_size)
    {
      priv->default_size = default_size;
      gtk_widget_queue_resize (gtk_css_gadget_get_owner (GTK_CSS_GADGET (icon)));
    }
}

int
gtk_builtin_icon_get_default_size (GtkBuiltinIcon *icon)
{
  GtkBuiltinIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUILTIN_ICON (icon), GTK_CSS_IMAGE_BUILTIN_NONE);

  priv = gtk_builtin_icon_get_instance_private (icon);

  return priv->default_size;
}

/**
 * gtk_builtin_icon_set_default_size_property:
 * @icon: icon to set the property for
 * @property_name: Name of the style property
 *
 * Sets the name of a widget style property to use to compute the default size
 * of the icon. If it is set to no %NULL, it will be used instead of the value
 * set via gtk_builtin_icon_set_default_size() to set the default size of the
 * icon.
 *
 * @property_name must refer to a style property that is of integer type.
 *
 * This function is intended strictly for backwards compatibility reasons.
 */
void
gtk_builtin_icon_set_default_size_property (GtkBuiltinIcon *icon,
                                            const char     *property_name)
{
  GtkBuiltinIconPrivate *priv;
  
  g_return_if_fail (GTK_IS_BUILTIN_ICON (icon));

  priv = gtk_builtin_icon_get_instance_private (icon);

  if (g_strcmp0 (priv->default_size_property, property_name))
    {
      priv->default_size_property = g_strdup (property_name);
      gtk_widget_queue_resize (gtk_css_gadget_get_owner (GTK_CSS_GADGET (icon)));
    }
}

const char *
gtk_builtin_icon_get_default_size_property (GtkBuiltinIcon *icon)
{
  GtkBuiltinIconPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUILTIN_ICON (icon), NULL);

  priv = gtk_builtin_icon_get_instance_private (icon);

  return priv->default_size_property;
}
