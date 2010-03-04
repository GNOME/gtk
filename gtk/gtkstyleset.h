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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_STYLE_SET_H__
#define __GTK_STYLE_SET_H__

#include <glib-object.h>
#include "gtkenums.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_SET         (gtk_style_set_get_type ())
#define GTK_STYLE_SET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_SET, GtkStyleSet))
#define GTK_STYLE_SET_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_STYLE_SET, GtkStyleSetClass))
#define GTK_IS_STYLE_SET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_SET))
#define GTK_IS_STYLE_SET_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_STYLE_SET))
#define GTK_STYLE_SET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_STYLE_SET, GtkStyleSetClass))

typedef struct GtkStyleSet GtkStyleSet;
typedef struct GtkStyleSetClass GtkStyleSetClass;

struct GtkStyleSet
{
  GObject parent_object;
};

struct GtkStyleSetClass
{
  GObjectClass parent_class;
};

GType gtk_style_set_get_type (void) G_GNUC_CONST;

GtkStyleSet * gtk_style_set_new (void);

void     gtk_style_set_set_property (GtkStyleSet  *set,
                                     const gchar  *property,
                                     GtkStateType  state,
                                     const GValue *value);

gboolean gtk_style_set_get_property (GtkStyleSet  *set,
                                     const gchar  *property,
                                     GtkStateType  state,
                                     GValue       *value);

void     gtk_style_set_unset_property (GtkStyleSet  *set,
                                       const gchar  *property,
                                       GtkStateType  state);

void     gtk_style_set_clear          (GtkStyleSet  *set);

G_END_DECLS

#endif /* __GTK_STYLE_PROPERTY_SET_H__ */
