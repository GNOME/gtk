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

#ifndef __GTK_CSS_STYLE_PRIVATE_H__
#define __GTK_CSS_STYLE_PRIVATE_H__

#include <glib-object.h>

#include "gtk/gtkbitmaskprivate.h"
#include "gtk/gtkcsssection.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STYLE           (gtk_css_style_get_type ())
#define GTK_CSS_STYLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STYLE, GtkCssStyle))
#define GTK_CSS_STYLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STYLE, GtkCssStyleClass))
#define GTK_IS_CSS_STYLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STYLE))
#define GTK_IS_CSS_STYLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STYLE))
#define GTK_CSS_STYLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STYLE, GtkCssStyleClass))

/* typedef struct _GtkCssStyle           GtkCssStyle; */
typedef struct _GtkCssStyleClass      GtkCssStyleClass;

struct _GtkCssStyle
{
  GObject parent;

  GPtrArray             *values;               /* the unanimated (aka intrinsic) values */
  GPtrArray             *sections;             /* sections the values are defined in */

  GPtrArray             *animated_values;      /* NULL or array of animated values/NULL if not animated */
  gint64                 current_time;         /* the current time in our world */
  GSList                *animations;           /* the running animations, least important one first */

  GtkBitmask            *depends_on_parent;    /* for intrinsic values */
  GtkBitmask            *equals_parent;        /* dito */
  GtkBitmask            *depends_on_color;     /* dito */
  GtkBitmask            *depends_on_font_size; /* dito */
};

struct _GtkCssStyleClass
{
  GObjectClass parent_class;
};

GType                   gtk_css_style_get_type                  (void) G_GNUC_CONST;

GtkCssStyle *           gtk_css_style_new                       (void);

void                    gtk_css_style_compute_value             (GtkCssStyle            *style,
                                                                 GtkStyleProviderPrivate*provider,
								 int                     scale,
                                                                 GtkCssStyle            *parent_style,
                                                                 guint                   id,
                                                                 GtkCssValue            *specified,
                                                                 GtkCssSection          *section);
void                    gtk_css_style_set_animated_value        (GtkCssStyle            *style,
                                                                 guint                   id,
                                                                 GtkCssValue            *value);
                                                                        
GtkCssValue *           gtk_css_style_get_value                 (GtkCssStyle            *style,
                                                                 guint                   id);
GtkCssSection *         gtk_css_style_get_section               (GtkCssStyle            *style,
                                                                 guint                   id);
GtkCssValue *           gtk_css_style_get_intrinsic_value       (GtkCssStyle            *style,
                                                                 guint                   id);
GtkBitmask *            gtk_css_style_get_difference            (GtkCssStyle            *style,
                                                                 GtkCssStyle            *other);
GtkBitmask *            gtk_css_style_compute_dependencies      (GtkCssStyle            *style,
                                                                 const GtkBitmask       *parent_changes);

void                    gtk_css_style_create_animations         (GtkCssStyle            *style,
                                                                 GtkCssStyle            *parent_style,
                                                                 gint64                  timestamp,
                                                                 GtkStyleProviderPrivate*provider,
								 int                     scale,
                                                                 GtkCssStyle            *source);
GtkBitmask *            gtk_css_style_advance                   (GtkCssStyle            *style,
                                                                 gint64                  timestamp);
void                    gtk_css_style_cancel_animations         (GtkCssStyle            *style);
gboolean                gtk_css_style_is_static                 (GtkCssStyle            *style);

char *                  gtk_css_style_to_string                 (GtkCssStyle            *style);
void                    gtk_css_style_print                     (GtkCssStyle            *style,
                                                                 GString                *string);

G_END_DECLS

#endif /* __GTK_CSS_STYLE_PRIVATE_H__ */
