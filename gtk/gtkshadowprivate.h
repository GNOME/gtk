/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SHADOW_H__
#define __GTK_SHADOW_H__

#include <glib-object.h>

#include "gtkstyleproperties.h"
#include "gtksymboliccolor.h"
#include "gtkicontheme.h"
#include "gtkcsstypesprivate.h"
#include "gtkroundedboxprivate.h"

G_BEGIN_DECLS

typedef struct _GtkShadow GtkShadow;

#define GTK_TYPE_SHADOW (_gtk_shadow_get_type ())

GType      _gtk_shadow_get_type       (void) G_GNUC_CONST;

GtkShadow *_gtk_shadow_new            (void);
GtkShadow *_gtk_shadow_ref            (GtkShadow          *shadow);
void       _gtk_shadow_unref          (GtkShadow          *shadow);

void       _gtk_shadow_append         (GtkShadow          *shadow,
                                       gdouble             hoffset,
                                       gdouble             voffset,
                                       gdouble             radius,
                                       gdouble             spread,
                                       gboolean            inset,
                                       GtkSymbolicColor   *color);

void       _gtk_shadow_print          (GtkShadow          *shadow,
                                       GString            *string);

GtkShadow *_gtk_shadow_resolve        (GtkShadow          *shadow,
                                       GtkStyleProperties *props);
gboolean   _gtk_shadow_get_resolved   (GtkShadow          *shadow);

void       _gtk_text_shadow_paint_layout (GtkShadow       *shadow,
                                          cairo_t         *cr,
                                          PangoLayout     *layout);

void       _gtk_icon_shadow_paint        (GtkShadow *shadow,
					  cairo_t *cr);

void       _gtk_icon_shadow_paint_spinner (GtkShadow *shadow,
                                           cairo_t   *cr,
                                           gdouble    radius,
                                           gdouble    progress);
void       _gtk_box_shadow_render         (GtkShadow           *shadow,
                                           cairo_t             *cr,
                                           const GtkRoundedBox *padding_box);

G_END_DECLS

#endif /* __GTK_SHADOW_H__ */
