/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012, Red Hat, Inc.
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

#include "config.h"

#include "gtkcolorchooser.h"
#include "gtkcolorchooserprivate.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkcolorchooser
 * @Short_description: Interface implemented by widgets for choosing colors
 * @Title: GtkColorChooser
 * @See_also: #GtkColorChooserDialog, #GtkColorChooserWidget, #GtkColorButton
 *
 * #GtkColorChooser is an interface that is implemented by widgets
 * for choosing colors. Depending on the situation, colors may be
 * allowed to have alpha (translucency).
 *
 * In GTK+, the main widgets that implement this interface are
 * #GtkColorChooserWidget, #GtkColorChooserDialog and #GtkColorButton.
 *
 * Since: 3.4
 */

enum
{
  COLOR_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (GtkColorChooser, gtk_color_chooser, G_TYPE_OBJECT);

static void
gtk_color_chooser_default_init (GtkColorChooserInterface *iface)
{
  /**
   * GtkColorChooser:rgba:
   *
   * The ::rgba property contains the currently selected color,
   * as a #GdkRGBA struct. The property can be set to change
   * the current selection programmatically.
   *
   * Since: 3.4
   */
  g_object_interface_install_property (iface,
      g_param_spec_boxed ("rgba",
                          P_("Color"),
                          P_("Current color, as a GdkRGBA"),
                          GDK_TYPE_RGBA,
                          GTK_PARAM_READWRITE));

  /**
   * GtkColorChooser:use-alpha:
   *
   * When ::use-alpha is %TRUE, colors may have alpha (translucency)
   * information. When it is %FALSE, the #GdkRGBA struct obtained
   * via the #GtkColorChooser:rgba property will be forced to have
   * alpha == 1.
   *
   * Implementations are expected to show alpha by rendering the color
   * over a non-uniform background (like a checkerboard pattern).
   *
   * Since: 3.4
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("use-alpha",
                            P_("Use alpha"),
                            P_("Whether alpha should be shown"),
                            TRUE,
                            GTK_PARAM_READWRITE));

  /**
   * GtkColorChooser::color-activated:
   * @chooser: the object which received the signal
   * @color: the color
   *
   * Emitted when a color is activated from the color chooser.
   * This usually happens when the user clicks a color swatch,
   * or a color is selected and the user presses one of the keys
   * Space, Shift+Space, Return or Enter.
   *
   * Since: 3.4
   */
  signals[COLOR_ACTIVATED] =
    g_signal_new ("color-activated",
                  GTK_TYPE_COLOR_CHOOSER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkColorChooserInterface, color_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

/**
 * gtk_color_chooser_get_rgba:
 * @chooser: a #GtkColorChooser
 * @color: return location for the color
 *
 * Gets the currently-selected color.
 *
 * Since: 3.4
 */
void
gtk_color_chooser_get_rgba (GtkColorChooser *chooser,
                            GdkRGBA         *color)
{
  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));

  GTK_COLOR_CHOOSER_GET_IFACE (chooser)->get_rgba (chooser, color);
}

/**
 * gtk_color_chooser_set_rgba:
 * @chooser: a #GtkColorChooser
 * @color: the new color
 *
 * Sets the color.
 */
void
gtk_color_chooser_set_rgba (GtkColorChooser *chooser,
                            const GdkRGBA   *color)
{
  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));
  g_return_if_fail (color != NULL);

  GTK_COLOR_CHOOSER_GET_IFACE (chooser)->set_rgba (chooser, color);
}

void
_gtk_color_chooser_color_activated (GtkColorChooser *chooser,
                                    const GdkRGBA   *color)
{
  g_signal_emit (chooser, signals[COLOR_ACTIVATED], 0, color);
}

/**
 * gtk_color_chooser_get_use_alpha:
 * @chooser: a #GtkColorChooser
 *
 * Returns whether the color chooser shows the alpha channel.
 *
 * Returns: %TRUE if the color chooser uses the alpha channel,
 *     %FALSE if not
 *
 * Since: 3.4
 */
gboolean
gtk_color_chooser_get_use_alpha (GtkColorChooser *chooser)
{
  gboolean use_alpha;

  g_return_val_if_fail (GTK_IS_COLOR_CHOOSER (chooser), TRUE);

  g_object_get (chooser, "use-alpha", &use_alpha, NULL);

  return use_alpha;
}

/**
 * gtk_color_chooser_set_use_alpha:
 * @chooser: a #GtkColorChooser
 * @use_alpha: %TRUE if color chooser should use alpha channel, %FALSE if not
 *
 * Sets whether or not the color chooser should use the alpha channel.
 *
 * Since: 3.4
 */
void
gtk_color_chooser_set_use_alpha (GtkColorChooser *chooser,
                                 gboolean         use_alpha)
{

  g_return_if_fail (GTK_IS_COLOR_CHOOSER (chooser));

  g_object_set (chooser, "use-alpha", use_alpha, NULL);
}
