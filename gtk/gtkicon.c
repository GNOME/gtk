
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

#include "gtkcssnodeprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkiconprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkrendericonprivate.h"
#include "gtksnapshot.h"

/* GtkIcon was a minimal widget wrapped around a GtkBuiltinIcon gadget,
 * It should be used whenever builtin-icon functionality is desired
 * but a widget is needed for other reasons.
 */

struct _GtkIcon
{
  GtkWidget parent;

  GtkCssImageBuiltinType image;
};

G_DEFINE_TYPE (GtkIcon, gtk_icon, GTK_TYPE_WIDGET)

static void
gtk_icon_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkIcon *self = GTK_ICON (widget);
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  int width, height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  if (width > 0 && height > 0)
    gtk_css_style_snapshot_icon (style,
                                 snapshot,
                                 width, height,
                                 self->image);
}

static void
gtk_icon_measure (GtkWidget      *widget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkCssValue *icon_size;

  icon_size = _gtk_style_context_peek_property (gtk_widget_get_style_context (widget), GTK_CSS_PROPERTY_ICON_SIZE);
  *minimum = *natural = _gtk_css_number_value_get (icon_size, 100);
}

static void
gtk_icon_class_init (GtkIconClass *klass)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  wclass->snapshot = gtk_icon_snapshot;
  wclass->measure = gtk_icon_measure;
}

static void
gtk_icon_init (GtkIcon *self)
{
  self->image = GTK_CSS_IMAGE_BUILTIN_NONE;
}

GtkWidget *
gtk_icon_new (const char *css_name)
{
  return g_object_new (GTK_TYPE_ICON,
                       "css-name", css_name,
                       NULL);
}

void
gtk_icon_set_image (GtkIcon                *self,
                    GtkCssImageBuiltinType  image)
{
  self->image = image;
}

void
gtk_icon_set_css_name (GtkIcon    *self,
                       const char *css_name)
{
  gtk_css_node_set_name (gtk_widget_get_css_node (GTK_WIDGET (self)),
                         g_intern_string (css_name));
}
