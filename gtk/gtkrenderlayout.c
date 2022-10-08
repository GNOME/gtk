/* GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
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

#include "gtkrenderlayoutprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkpangoprivate.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"


void
gtk_css_style_snapshot_layout (GtkCssBoxes *boxes,
                               GtkSnapshot *snapshot,
                               int          x,
                               int          y,
                               PangoLayout *layout)
{
  GtkCssStyle *style;
  const GdkRGBA *color;
  gboolean has_shadow;

  gtk_snapshot_push_debug (snapshot, "Layout");

  if (x != 0 || y != 0)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
    }

  style = boxes->style;
  color = gtk_css_color_value_get_rgba (style->core->color);
  has_shadow = gtk_css_shadow_value_push_snapshot (style->font->text_shadow, snapshot);

  gtk_snapshot_append_layout (snapshot, layout, color);

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  if (x != 0 || y != 0)
    gtk_snapshot_restore (snapshot);

  gtk_snapshot_pop (snapshot);
}

