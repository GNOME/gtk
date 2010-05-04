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
#include <gdk/gdk.h>
#include "gtkenums.h"

/* GtkBorder is defined there */
#include "gtkstyle.h"

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

/* Functions to register style properties */
void     gtk_style_set_register_property (const gchar  *property_name,
                                          GType         type,
                                          const GValue *default_value);
gboolean gtk_style_set_lookup_property   (const gchar  *property_name,
                                          GType        *type,
                                          GValue       *default_value);

void gtk_style_set_register_property_color  (const gchar *property_name,
                                             GdkColor    *default_value);
void gtk_style_set_register_property_font   (const gchar          *property_name,
                                             PangoFontDescription *initial_value);
void gtk_style_set_register_property_border (const gchar *property_name,
                                             GtkBorder   *initial_value);
void gtk_style_set_register_property_int    (const gchar *property_name,
                                             gint         default_value);
void gtk_style_set_register_property_uint   (const gchar *property_name,
                                             guint        default_value);
void gtk_style_set_register_property_double (const gchar *property_name,
                                             gdouble      default_value);


GtkStyleSet * gtk_style_set_new (void);

void     gtk_style_set_set_default  (GtkStyleSet  *set,
                                     const gchar  *property,
                                     const GValue *value);
void     gtk_style_set_set_property (GtkStyleSet  *set,
                                     const gchar  *property,
                                     GtkStateType  state,
                                     const GValue *value);
void     gtk_style_set_set_valist   (GtkStyleSet  *set,
                                     GtkStateType  state,
                                     va_list       args);
void     gtk_style_set_set          (GtkStyleSet  *set,
                                     GtkStateType  state,
                                     ...) G_GNUC_NULL_TERMINATED;

gboolean gtk_style_set_get_property (GtkStyleSet  *set,
                                     const gchar  *property,
                                     GtkStateType  state,
                                     GValue       *value);
void     gtk_style_set_get_valist   (GtkStyleSet  *set,
                                     GtkStateType  state,
                                     va_list       args);
void     gtk_style_set_get          (GtkStyleSet  *set,
                                     GtkStateType  state,
                                     ...) G_GNUC_NULL_TERMINATED;

void     gtk_style_set_unset_property (GtkStyleSet  *set,
                                       const gchar  *property,
                                       GtkStateType  state);

void     gtk_style_set_clear          (GtkStyleSet  *set);

void     gtk_style_set_merge          (GtkStyleSet       *set,
                                       const GtkStyleSet *set_to_merge,
                                       gboolean           replace);

G_END_DECLS

#endif /* __GTK_STYLE_PROPERTY_SET_H__ */
