
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
#include "gtkrendericonprivate.h"

/* GtkIcon was a minimal widget wrapped around a GtkBuiltinIcon gadget,
 * It should be used whenever builtin-icon functionality is desired
 * but a widget is needed for other reasons.
 */

G_DEFINE_TYPE (GtkIcon, gtk_icon, GTK_TYPE_WIDGET)

static void
gtk_icon_size_allocate (GtkWidget     *widget,
                        GtkAllocation *allocation)
{
  gtk_widget_set_clip (widget, allocation);
}

static void
gtk_icon_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  GtkAllocation alloc;

  gtk_widget_get_content_allocation (widget, &alloc);

  gtk_css_style_snapshot_icon (style,
                               snapshot,
                               alloc.width, alloc.height,
                               GTK_CSS_IMAGE_BUILTIN_NONE);
}

static void
gtk_icon_class_init (GtkIconClass *klass)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  wclass->size_allocate = gtk_icon_size_allocate;
  wclass->snapshot = gtk_icon_snapshot;
}

static void
gtk_icon_init (GtkIcon *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

GtkWidget *
gtk_icon_new (const char *css_name)
{
  return g_object_new (GTK_TYPE_ICON,
                       "css-name", css_name,
                       NULL);
}
