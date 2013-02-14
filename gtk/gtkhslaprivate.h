/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_HSLA_PRIVATE_H__
#define __GTK_HSLA_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GtkHSLA GtkHSLA;

struct _GtkHSLA {
  double hue;
  double saturation;
  double lightness;
  double alpha;
};

void            _gtk_hsla_init              (GtkHSLA          *hsla,
                                             double            hue,
                                             double            saturation,
                                             double            lightness,
                                             double            alpha);
void            _gtk_hsla_init_from_rgba    (GtkHSLA          *hsla,
                                             const GdkRGBA    *rgba);
/* Yes, I can name that function like this! */
void            _gdk_rgba_init_from_hsla    (GdkRGBA          *rgba,
                                             const GtkHSLA    *hsla);

void            _gtk_hsla_shade             (GtkHSLA          *dest,
                                             const GtkHSLA    *src,
                                             double            factor);

G_END_DECLS

#endif /* __GTK_HSLA_PRIVATE_H__ */
