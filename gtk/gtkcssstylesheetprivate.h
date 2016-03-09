/*
 * Copyright © 2016 Red Hat Inc.
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

#ifndef __GTK_CSS_STYLE_SHEET_PRIVATE_H__
#define __GTK_CSS_STYLE_SHEET_PRIVATE_H__

#include "gtk/gtkcssruleprivate.h"
#include "gtk/gtkcssrulelistprivate.h"
#include "gtk/gtkcsstokensourceprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STYLE_SHEET           (gtk_css_style_sheet_get_type ())
#define GTK_CSS_STYLE_SHEET(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STYLE_SHEET, GtkCssStyleSheet))
#define GTK_CSS_STYLE_SHEET_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STYLE_SHEET, GtkCssStyleSheetClass))
#define GTK_IS_CSS_STYLE_SHEET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STYLE_SHEET))
#define GTK_IS_CSS_STYLE_SHEET_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STYLE_SHEET))
#define GTK_CSS_STYLE_SHEET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STYLE_SHEET, GtkCssStyleSheetClass))

/* declared with GtkCssRule */
/* typedef struct _GtkCssStyleSheet           GtkCssStyleSheet; */
typedef struct _GtkCssStyleSheetClass      GtkCssStyleSheetClass;

struct _GtkCssStyleSheet
{
  GObject parent;
};

struct _GtkCssStyleSheetClass
{
  GObjectClass parent_class;
};

GType                  gtk_css_style_sheet_get_type                     (void) G_GNUC_CONST;

GtkCssStyleSheet *     gtk_css_style_sheet_new                          (void);

void                   gtk_css_style_sheet_parse                        (GtkCssStyleSheet       *style_sheet,
                                                                         GtkCssTokenSource      *source);

/* StyleSheet interface */
GtkCssStyleSheet *     gtk_css_style_sheet_get_parent_style_sheet       (GtkCssStyleSheet       *style_sheet);

/* CSSStyleSheet interface */
GtkCssRule *           gtk_css_style_sheet_get_parent_rule              (GtkCssStyleSheet       *style_sheet);
GtkCssRuleList *       gtk_css_style_sheet_get_css_rules                (GtkCssStyleSheet       *style_sheet);


G_END_DECLS

#endif /* __GTK_CSS_STYLE_SHEET_PRIVATE_H__ */
