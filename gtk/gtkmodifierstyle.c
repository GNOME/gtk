/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
#include "gtkmodifierstyle.h"
#include "gtkstyleproviderprivate.h"
#include "gtkintl.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct StylePropertyValue StylePropertyValue;

struct _GtkModifierStylePrivate
{
  GtkStyleProperties *style;
  GHashTable *color_properties;
};

static void gtk_modifier_style_provider_init         (GtkStyleProviderIface            *iface);
static void gtk_modifier_style_provider_private_init (GtkStyleProviderPrivateInterface *iface);
static void gtk_modifier_style_finalize              (GObject                          *object);

G_DEFINE_TYPE_EXTENDED (GtkModifierStyle, _gtk_modifier_style, G_TYPE_OBJECT, 0,
                        G_ADD_PRIVATE (GtkModifierStyle)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_modifier_style_provider_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER_PRIVATE,
                                               gtk_modifier_style_provider_private_init));

static void
_gtk_modifier_style_class_init (GtkModifierStyleClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_modifier_style_finalize;
}

static void
_gtk_modifier_style_init (GtkModifierStyle *modifier_style)
{
  GtkModifierStylePrivate *priv;

  priv = modifier_style->priv = _gtk_modifier_style_get_instance_private (modifier_style);

  priv->color_properties = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) gdk_rgba_free);
  priv->style = gtk_style_properties_new ();
}

static gboolean
gtk_modifier_style_get_style_property (GtkStyleProvider *provider,
                                       GtkWidgetPath    *path,
                                       GtkStateFlags     state,
                                       GParamSpec       *pspec,
                                       GValue           *value)
{
  GtkModifierStylePrivate *priv;
  GdkRGBA *rgba;
  GdkColor color;
  gchar *str;

  /* Reject non-color types for now */
  if (pspec->value_type != GDK_TYPE_COLOR)
    return FALSE;

  priv = GTK_MODIFIER_STYLE (provider)->priv;
  str = g_strdup_printf ("-%s-%s",
                         g_type_name (pspec->owner_type),
                         pspec->name);

  rgba = g_hash_table_lookup (priv->color_properties, str);
  g_free (str);

  if (!rgba)
    return FALSE;

  color.red = (guint) (rgba->red * 65535.) + 0.5;
  color.green = (guint) (rgba->green * 65535.) + 0.5;
  color.blue = (guint) (rgba->blue * 65535.) + 0.5;

  g_value_set_boxed (value, &color);
  return TRUE;
}

static void
gtk_modifier_style_provider_init (GtkStyleProviderIface *iface)
{
  iface->get_style_property = gtk_modifier_style_get_style_property;
}

static GtkCssValue *
gtk_modifier_style_provider_get_color (GtkStyleProviderPrivate *provider,
                                       const char              *name)
{
  GtkModifierStyle *style = GTK_MODIFIER_STYLE (provider);

  return _gtk_style_provider_private_get_color (GTK_STYLE_PROVIDER_PRIVATE (style->priv->style), name);
}

static void
gtk_modifier_style_provider_lookup (GtkStyleProviderPrivate *provider,
                                    const GtkCssMatcher     *matcher,
                                    GtkCssLookup            *lookup)
{
  GtkModifierStyle *style = GTK_MODIFIER_STYLE (provider);

  _gtk_style_provider_private_lookup (GTK_STYLE_PROVIDER_PRIVATE (style->priv->style),
                                      matcher,
                                      lookup);
}

static GtkCssChange
gtk_modifier_style_provider_get_change (GtkStyleProviderPrivate *provider,
                                        const GtkCssMatcher     *matcher)
{
  GtkModifierStyle *style = GTK_MODIFIER_STYLE (provider);

  return _gtk_style_provider_private_get_change (GTK_STYLE_PROVIDER_PRIVATE (style->priv->style),
                                                 matcher);
}

static void
gtk_modifier_style_provider_private_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_color = gtk_modifier_style_provider_get_color;
  iface->lookup = gtk_modifier_style_provider_lookup;
  iface->get_change = gtk_modifier_style_provider_get_change;
}

static void
gtk_modifier_style_finalize (GObject *object)
{
  GtkModifierStylePrivate *priv;

  priv = GTK_MODIFIER_STYLE (object)->priv;
  g_hash_table_destroy (priv->color_properties);
  g_object_unref (priv->style);

  G_OBJECT_CLASS (_gtk_modifier_style_parent_class)->finalize (object);
}

GtkModifierStyle *
_gtk_modifier_style_new (void)
{
  return g_object_new (GTK_TYPE_MODIFIER_STYLE, NULL);
}

static void
modifier_style_set_color (GtkModifierStyle *style,
                          const gchar      *prop,
                          GtkStateFlags     state,
                          const GdkRGBA    *color)
{
  GtkModifierStylePrivate *priv;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  priv = style->priv;

  if (color)
    gtk_style_properties_set (priv->style, state,
                              prop, color,
                              NULL);
  else
    gtk_style_properties_unset_property (priv->style, prop, state);

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (style));
}

void
_gtk_modifier_style_set_background_color (GtkModifierStyle *style,
                                          GtkStateFlags     state,
                                          const GdkRGBA    *color)
{
  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  modifier_style_set_color (style, "background-color", state, color);
}

void
_gtk_modifier_style_set_color (GtkModifierStyle *style,
                               GtkStateFlags     state,
                               const GdkRGBA    *color)
{
  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  modifier_style_set_color (style, "color", state, color);
}

void
_gtk_modifier_style_set_font (GtkModifierStyle           *style,
                              const PangoFontDescription *font_desc)
{
  GtkModifierStylePrivate *priv;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  priv = style->priv;

  if (font_desc)
    gtk_style_properties_set (priv->style, 0,
                              "font", font_desc,
                              NULL);
  else
    gtk_style_properties_unset_property (priv->style, "font", 0);

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (style));
}

void
_gtk_modifier_style_map_color (GtkModifierStyle *style,
                               const gchar      *name,
                               const GdkRGBA    *color)
{
  GtkModifierStylePrivate *priv;
  GtkSymbolicColor *symbolic_color = NULL;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));
  g_return_if_fail (name != NULL);

  priv = style->priv;

  if (color)
    symbolic_color = gtk_symbolic_color_new_literal (color);

  gtk_style_properties_map_color (priv->style,
                                  name, symbolic_color);

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (style));
}

void
_gtk_modifier_style_set_color_property (GtkModifierStyle *style,
                                        GType             widget_type,
                                        const gchar      *prop_name,
                                        const GdkRGBA    *color)
{
  GtkModifierStylePrivate *priv;
  const GdkRGBA *old_color;
  gchar *str;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));
  g_return_if_fail (g_type_is_a (widget_type, GTK_TYPE_WIDGET));
  g_return_if_fail (prop_name != NULL);

  priv = style->priv;
  str = g_strdup_printf ("-%s-%s", g_type_name (widget_type), prop_name);

  old_color = g_hash_table_lookup (priv->color_properties, str);

  if ((!color && !old_color) ||
      (color && old_color && gdk_rgba_equal (color, old_color)))
    {
      g_free (str);
      return;
    }

  if (color)
    {
      g_hash_table_insert (priv->color_properties, str,
                           gdk_rgba_copy (color));
    }
  else
    {
      g_hash_table_remove (priv->color_properties, str);
      g_free (str);
    }

  _gtk_style_provider_private_changed (GTK_STYLE_PROVIDER_PRIVATE (style));
}

G_GNUC_END_IGNORE_DEPRECATIONS
