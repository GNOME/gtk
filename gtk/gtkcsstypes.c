/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsstypesprivate.h"

#define DEFINE_BOXED_TYPE_WITH_COPY_FUNC(TypeName, type_name) \
\
static TypeName * \
type_name ## _copy (const TypeName *foo) \
{ \
  return g_memdup (foo, sizeof (TypeName)); \
} \
\
G_DEFINE_BOXED_TYPE (TypeName, type_name, type_name ## _copy, g_free)

DEFINE_BOXED_TYPE_WITH_COPY_FUNC (GtkCssBorderCornerRadius, _gtk_css_border_corner_radius)
DEFINE_BOXED_TYPE_WITH_COPY_FUNC (GtkCssBorderRadius, _gtk_css_border_radius)
DEFINE_BOXED_TYPE_WITH_COPY_FUNC (GtkCssBorderImageRepeat, _gtk_css_border_image_repeat)
