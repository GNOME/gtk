/*
 * Copyright (c) 2016 Benjamin Otte <otte@gnome.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "cssruleview.h"

#include "cssruleviewrow.h"

#include "gtkcssrulelistprivate.h"
#include "gtkcssstyleruleprivate.h"

typedef struct _GtkInspectorCssRuleViewPrivate GtkInspectorCssRuleViewPrivate;

struct _GtkInspectorCssRuleViewPrivate
{
  GtkCssStyleSheet *style_sheet;

  GtkWidget *list_widget;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssRuleView, gtk_inspector_css_rule_view, GTK_TYPE_BIN)

static void
finalize (GObject *object)
{
  GtkInspectorCssRuleView *ruleview = GTK_INSPECTOR_CSS_RULE_VIEW (object);

  gtk_inspector_css_rule_view_set_style_sheet (ruleview, NULL);

  G_OBJECT_CLASS (gtk_inspector_css_rule_view_parent_class)->finalize (object);
}

static void
gtk_inspector_css_rule_view_class_init (GtkInspectorCssRuleViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/cssruleview.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssRuleView, list_widget);
}

static gint
sort_func (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       data)
{
  return - gtk_inspector_css_rule_view_row_compare_specificity (GTK_INSPECTOR_CSS_RULE_VIEW_ROW (row1),
                                                                GTK_INSPECTOR_CSS_RULE_VIEW_ROW (row2));
}

static void
gtk_inspector_css_rule_view_init (GtkInspectorCssRuleView *ruleview)
{
  GtkInspectorCssRuleViewPrivate *priv;

  gtk_widget_init_template (GTK_WIDGET (ruleview));

  priv = gtk_inspector_css_rule_view_get_instance_private (ruleview);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->list_widget),
                              sort_func,
                              ruleview,
                              NULL);
}

void
gtk_inspector_css_rule_view_set_style_sheet (GtkInspectorCssRuleView *ruleview,
                                             GtkCssStyleSheet        *style_sheet)
{
  GtkInspectorCssRuleViewPrivate *priv;
  
  g_return_if_fail (GTK_INSPECTOR_IS_CSS_RULE_VIEW (ruleview));
  g_return_if_fail (style_sheet == NULL || GTK_IS_CSS_STYLE_SHEET (style_sheet));
  
  priv = gtk_inspector_css_rule_view_get_instance_private (ruleview);

  if (priv->style_sheet == style_sheet)
    return;

  if (priv->style_sheet)
    {
      GtkListBoxRow *row;
      
      for (row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (priv->list_widget), 0);
           row != NULL;
           row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (priv->list_widget), 0))
        {
          gtk_container_remove (GTK_CONTAINER (priv->list_widget), GTK_WIDGET (row));
        }

      g_object_unref (priv->style_sheet);
    }

  priv->style_sheet = style_sheet;

  if (style_sheet)
    {
      GtkCssRuleList *rule_list;
      GtkCssRule *rule;
      guint i, j;

      g_object_ref (style_sheet);

      rule_list = gtk_css_style_sheet_get_css_rules (style_sheet);

      for (i = 0; i < gtk_css_rule_list_get_length (rule_list); i++)
        {
          rule = gtk_css_rule_list_get_item (rule_list, i);

          if (!GTK_IS_CSS_STYLE_RULE (rule))
            continue;

          for (j = 0; j < gtk_css_style_rule_get_n_selectors (GTK_CSS_STYLE_RULE (rule)); j++)
            {
              GtkWidget *row = gtk_inspector_css_rule_view_row_new (GTK_CSS_STYLE_RULE (rule), j);

              gtk_widget_show (row);
              gtk_container_add (GTK_CONTAINER (priv->list_widget), row);
            }
        }
    }
}

