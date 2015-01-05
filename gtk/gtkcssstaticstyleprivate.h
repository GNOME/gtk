/*
 * Copyright © 2012 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_CSS_STATIC_STYLE_PRIVATE_H__
#define __GTK_CSS_STATIC_STYLE_PRIVATE_H__

#include "gtk/gtkcssmatcherprivate.h"
#include "gtk/gtkcssstyleprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STATIC_STYLE           (gtk_css_static_style_get_type ())
#define GTK_CSS_STATIC_STYLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyle))
#define GTK_CSS_STATIC_STYLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyleClass))
#define GTK_IS_CSS_STATIC_STYLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STATIC_STYLE))
#define GTK_IS_CSS_STATIC_STYLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STATIC_STYLE))
#define GTK_CSS_STATIC_STYLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyleClass))

typedef struct _GtkCssStaticStyle           GtkCssStaticStyle;
typedef struct _GtkCssStaticStyleClass      GtkCssStaticStyleClass;

struct _GtkCssStaticStyle
{
  GtkCssStyle parent;

  GPtrArray             *values;               /* the values */
  GPtrArray             *sections;             /* sections the values are defined in */

  GtkBitmask            *depends_on_parent;    /* values that depend on parent values */
  GtkBitmask            *equals_parent;        /* values that equal their parent values */
  GtkBitmask            *depends_on_color;     /* values that depend on the color property */
  GtkBitmask            *depends_on_font_size; /* values that depend on the font-size property */

  GtkCssChange           change;               /* change as returned by value lookup */
};

struct _GtkCssStaticStyleClass
{
  GtkCssStyleClass parent_class;
};

GType                   gtk_css_static_style_get_type           (void) G_GNUC_CONST;

GtkCssStyle *           gtk_css_static_style_get_default        (GdkScreen              *screen);
GtkCssStyle *           gtk_css_static_style_new_compute        (GtkStyleProviderPrivate *provider,
                                                                 const GtkCssMatcher    *matcher,
                                                                 int                     scale,
                                                                 GtkCssStyle            *parent);
GtkCssStyle *           gtk_css_static_style_new_update         (GtkCssStaticStyle      *style,
                                                                 const GtkBitmask       *parent_changes,
                                                                 GtkStyleProviderPrivate *provider,
                                                                 const GtkCssMatcher    *matcher,
                                                                 int                     scale,
                                                                 GtkCssStyle            *parent);

void                    gtk_css_static_style_compute_value      (GtkCssStaticStyle      *style,
                                                                 GtkStyleProviderPrivate*provider,
								 int                     scale,
                                                                 GtkCssStyle            *parent_style,
                                                                 guint                   id,
                                                                 GtkCssValue            *specified,
                                                                 GtkCssSection          *section);

GtkCssChange            gtk_css_static_style_get_change         (GtkCssStaticStyle      *style);

G_END_DECLS

#endif /* __GTK_CSS_STATIC_STYLE_PRIVATE_H__ */
