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

#ifndef __GTK_CSS_KEYFRAME_RULE_PRIVATE_H__
#define __GTK_CSS_KEYFRAME_RULE_PRIVATE_H__

#include "gtk/gtkcssruleprivate.h"
#include "gtk/gtkcssstyledeclarationprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_KEYFRAME_RULE           (gtk_css_keyframe_rule_get_type ())
#define GTK_CSS_KEYFRAME_RULE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_KEYFRAME_RULE, GtkCssKeyframeRule))
#define GTK_CSS_KEYFRAME_RULE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_KEYFRAME_RULE, GtkCssKeyframeRuleClass))
#define GTK_IS_CSS_KEYFRAME_RULE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_KEYFRAME_RULE))
#define GTK_IS_CSS_KEYFRAME_RULE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_KEYFRAME_RULE))
#define GTK_CSS_KEYFRAME_RULE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_KEYFRAME_RULE, GtkCssKeyframeRuleClass))

typedef struct _GtkCssKeyframeRule           GtkCssKeyframeRule;
typedef struct _GtkCssKeyframeRuleClass      GtkCssKeyframeRuleClass;

struct _GtkCssKeyframeRule
{
  GtkCssRule parent;
};

struct _GtkCssKeyframeRuleClass
{
  GtkCssRuleClass parent_class;
};

GType                   gtk_css_keyframe_rule_get_type          (void) G_GNUC_CONST;

GtkCssRule *            gtk_css_keyframe_rule_new_parse         (GtkCssTokenSource      *source,
                                                                 GtkCssRule             *parent_rule,
                                                                 GtkCssStyleSheet       *parent_style_sheet);

gsize                   gtk_css_keyframe_rule_get_n_offsets     (GtkCssKeyframeRule     *rule);
double                  gtk_css_keyframe_rule_get_offset        (GtkCssKeyframeRule     *rule,
                                                                 gsize                   id);

GtkCssStyleDeclaration *gtk_css_keyframe_rule_get_style         (GtkCssKeyframeRule     *rule);


G_END_DECLS

#endif /* __GTK_CSS_KEYFRAME_RULE_PRIVATE_H__ */
