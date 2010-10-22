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
#include "gtksymboliccolor.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_SET         (gtk_style_set_get_type ())
#define GTK_STYLE_SET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_SET, GtkStyleSet))
#define GTK_STYLE_SET_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_STYLE_SET, GtkStyleSetClass))
#define GTK_IS_STYLE_SET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_SET))
#define GTK_IS_STYLE_SET_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_STYLE_SET))
#define GTK_STYLE_SET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_STYLE_SET, GtkStyleSetClass))

typedef struct _GtkStyleSet GtkStyleSet;
typedef struct _GtkStyleSetClass GtkStyleSetClass;

struct _GtkStyleSet
{
  GObject parent_object;
  gpointer priv;
};

struct _GtkStyleSetClass
{
  GObjectClass parent_class;
};

typedef gboolean (* GtkStylePropertyParser) (const gchar  *string,
                                             GValue       *value,
                                             GError      **error);

GType gtk_style_set_get_type (void) G_GNUC_CONST;

/* Functions to register style properties */
void     gtk_style_set_register_property (const gchar            *property_name,
                                          GType                   type,
                                          const GValue           *default_value,
                                          GtkStylePropertyParser  parse_func);
gboolean gtk_style_set_lookup_property   (const gchar            *property_name,
                                          GType                  *type,
                                          GtkStylePropertyParser *parse_func);

GtkStyleSet * gtk_style_set_new (void);

void               gtk_style_set_map_color    (GtkStyleSet      *set,
                                               const gchar      *name,
                                               GtkSymbolicColor *color);
GtkSymbolicColor * gtk_style_set_lookup_color (GtkStyleSet *set,
                                               const gchar *name);

void     gtk_style_set_set_property (GtkStyleSet   *set,
                                     const gchar   *property,
                                     GtkStateFlags  state,
                                     const GValue  *value);
void     gtk_style_set_set_valist   (GtkStyleSet   *set,
                                     GtkStateFlags  state,
                                     va_list        args);
void     gtk_style_set_set          (GtkStyleSet   *set,
                                     GtkStateFlags  state,
                                     ...) G_GNUC_NULL_TERMINATED;

gboolean gtk_style_set_get_property (GtkStyleSet   *set,
                                     const gchar   *property,
                                     GtkStateFlags  state,
                                     GValue        *value);
void     gtk_style_set_get_valist   (GtkStyleSet   *set,
                                     GtkStateFlags  state,
                                     va_list        args);
void     gtk_style_set_get          (GtkStyleSet   *set,
                                     GtkStateFlags  state,
                                     ...) G_GNUC_NULL_TERMINATED;

void     gtk_style_set_unset_property (GtkStyleSet   *set,
                                       const gchar   *property,
                                       GtkStateFlags  state);

void     gtk_style_set_clear          (GtkStyleSet  *set);

void     gtk_style_set_merge          (GtkStyleSet       *set,
                                       const GtkStyleSet *set_to_merge,
                                       gboolean           replace);

gboolean gtk_symbolic_color_resolve (GtkSymbolicColor    *color,
				     GtkStyleSet         *style_set,
                                     GdkRGBA             *resolved_color);
gboolean gtk_gradient_resolve (GtkGradient      *gradient,
                               GtkStyleSet      *style_set,
                               cairo_pattern_t **resolved_gradient);

G_END_DECLS

#endif /* __GTK_STYLE_PROPERTY_SET_H__ */
