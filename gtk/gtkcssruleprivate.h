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

#ifndef __GTK_CSS_RULE_PRIVATE_H__
#define __GTK_CSS_RULE_PRIVATE_H__

#include "gtk/gtkcsstokensourceprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_RULE           (gtk_css_rule_get_type ())
#define GTK_CSS_RULE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_RULE, GtkCssRule))
#define GTK_CSS_RULE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_RULE, GtkCssRuleClass))
#define GTK_IS_CSS_RULE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_RULE))
#define GTK_IS_CSS_RULE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_RULE))
#define GTK_CSS_RULE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_RULE, GtkCssRuleClass))

/* forward declaration */
typedef struct _GtkCssStyleSheet     GtkCssStyleSheet;

typedef struct _GtkCssRule           GtkCssRule;
typedef struct _GtkCssRuleClass      GtkCssRuleClass;

struct _GtkCssRule
{
  GObject parent;
};

struct _GtkCssRuleClass
{
  GObjectClass parent_class;

  /* gets the cssText for this rule */
  void                 (* get_css_text)                    (GtkCssRule                  *rule,
                                                            GString                     *string);
};

GType                  gtk_css_rule_get_type               (void) G_GNUC_CONST;

GtkCssRule *           gtk_css_rule_new_from_at_rule       (GtkCssTokenSource           *source,
                                                            GtkCssRule                  *parent_rule,
                                                            GtkCssStyleSheet            *parent_style_sheet);

void                   gtk_css_rule_print_css_text         (GtkCssRule                  *rule,
                                                            GString                     *string);
char *                 gtk_css_rule_get_css_text           (GtkCssRule                  *rule);

GtkCssRule *           gtk_css_rule_get_parent_rule        (GtkCssRule                  *rule);
GtkCssStyleSheet *     gtk_css_rule_get_parent_style_sheet (GtkCssRule                  *rule);


G_END_DECLS

#endif /* __GTK_CSS_RULE_PRIVATE_H__ */
