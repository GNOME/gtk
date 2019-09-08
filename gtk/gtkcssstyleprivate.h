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
#include <gtk/css/gtkcss.h>

#include "gtk/gtkbitmaskprivate.h"
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
};

struct _GtkCssStyleClass
{
  GObjectClass parent_class;

  /* Get the value for the given property id. This needs to be FAST. */
  GtkCssValue *         (* get_value)                           (GtkCssStyle            *style,
                                                                 guint                   id);
  /* Get the section the value at the given id was declared at or NULL if unavailable.
   * Optional: default impl will just return NULL */
  GtkCssSection *       (* get_section)                         (GtkCssStyle            *style,
                                                                 guint                   id);
  /* TRUE if this style will require changes based on timestamp */
  gboolean              (* is_static)                           (GtkCssStyle            *style);
};

GType                   gtk_css_style_get_type                  (void) G_GNUC_CONST;

GtkCssValue *           gtk_css_style_get_value                 (GtkCssStyle            *style,
                                                                 guint                   id) G_GNUC_PURE;
GtkCssSection *         gtk_css_style_get_section               (GtkCssStyle            *style,
                                                                 guint                   id) G_GNUC_PURE;
gboolean                gtk_css_style_is_static                 (GtkCssStyle            *style) G_GNUC_PURE;

char *                  gtk_css_style_to_string                 (GtkCssStyle            *style);
gboolean                gtk_css_style_print                     (GtkCssStyle            *style,
                                                                 GString                *string,
                                                                 guint                   indent,
                                                                 gboolean                skip_initial);
PangoAttrList *         gtk_css_style_get_pango_attributes      (GtkCssStyle            *style);

PangoFontDescription *  gtk_css_style_get_pango_font            (GtkCssStyle            *style);

G_END_DECLS

#endif /* __GTK_CSS_STYLE_PRIVATE_H__ */
