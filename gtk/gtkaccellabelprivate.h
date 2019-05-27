/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1998, 2001 Tim Janik
 * Copyright (C) Javier Jardón <jjardon@gnome.org>
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

#ifndef __GTK_ACCEL_LABEL_PRIVATE_H__
#define __GTK_ACCEL_LABEL_PRIVATE_H__


#include <gtk/gtkaccellabel.h>

G_BEGIN_DECLS

#define GTK_ACCEL_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ACCEL_LABEL, GtkAccelLabelClass))

typedef struct _GtkAccelLabelClass   GtkAccelLabelClass;

char *    _gtk_accel_label_class_get_accelerator_label (GtkAccelLabelClass *klass,
                                                        guint               accelerator_key,
                                                        GdkModifierType     accelerator_mods);


G_END_DECLS

#endif /* __GTK_ACCEL_LABEL_PRIVATE_H__ */
