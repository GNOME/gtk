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

#ifndef __GTK_CSS_RULE_LIST_PRIVATE_H__
#define __GTK_CSS_RULE_LIST_PRIVATE_H__

#include "gtk/gtkcssruleprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_RULE_LIST           (gtk_css_rule_list_get_type ())
#define GTK_CSS_RULE_LIST(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_RULE_LIST, GtkCssRuleList))
#define GTK_CSS_RULE_LIST_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_RULE_LIST, GtkCssRuleListClass))
#define GTK_IS_CSS_RULE_LIST(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_RULE_LIST))
#define GTK_IS_CSS_RULE_LIST_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_RULE_LIST))
#define GTK_CSS_RULE_LIST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_RULE_LIST, GtkCssRuleListClass))

typedef struct _GtkCssRuleList           GtkCssRuleList;
typedef struct _GtkCssRuleListClass      GtkCssRuleListClass;

struct _GtkCssRuleList
{
  GObject parent;
};

struct _GtkCssRuleListClass
{
  GObjectClass parent_class;
};

GType                  gtk_css_rule_list_get_type               (void) G_GNUC_CONST;

GtkCssRuleList *       gtk_css_rule_list_new                    (void);

void                   gtk_css_rule_list_parse                  (GtkCssRuleList         *rule_list,
                                                                 GtkCssTokenSource      *source,
                                                                 GtkCssRule             *parent_rule,
                                                                 GtkCssStyleSheet       *parent_style_sheet);
void                   gtk_css_rule_list_insert                 (GtkCssRuleList         *rule_list,
                                                                 gsize                   id,
                                                                 GtkCssRule             *rule);
void                   gtk_css_rule_list_append                 (GtkCssRuleList         *rule_list,
                                                                 GtkCssRule             *rule);

/* GtkCSSRule DOM */
GtkCssRule *           gtk_css_rule_list_get_item               (GtkCssRuleList         *rule_list,
                                                                 gsize                   id);
gsize                  gtk_css_rule_list_get_length             (GtkCssRuleList         *rule_list);


G_END_DECLS

#endif /* __GTK_CSS_RULE_LIST_PRIVATE_H__ */
