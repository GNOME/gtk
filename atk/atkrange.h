/* ATK -  Accessibility Toolkit
 * Copyright 2014 Igalia S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __ATK_RANGE_H__
#define __ATK_RANGE_H__

#if defined(ATK_DISABLE_SINGLE_INCLUDES) && !defined (__ATK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <atk/atk.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define ATK_TYPE_RANGE         (atk_range_get_type ())

typedef struct _AtkRange AtkRange;

/* AtkRange methods */
GDK_AVAILABLE_IN_ALL
GType atk_range_get_type (void);

GDK_AVAILABLE_IN_ALL
AtkRange*    atk_range_copy (AtkRange *src);
GDK_AVAILABLE_IN_ALL
void         atk_range_free (AtkRange *range);

GDK_AVAILABLE_IN_ALL
gdouble      atk_range_get_lower_limit  (AtkRange    *range);
GDK_AVAILABLE_IN_ALL
gdouble      atk_range_get_upper_limit  (AtkRange    *range);
GDK_AVAILABLE_IN_ALL
const gchar* atk_range_get_description  (AtkRange    *range);
GDK_AVAILABLE_IN_ALL
AtkRange*    atk_range_new              (gdouble      lower_limit,
                                         gdouble      upper_limit,
                                         const gchar *description);

G_END_DECLS

#endif /* __ATK_RANGE_H__ */
