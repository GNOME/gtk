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

#include "cssruleviewrow.h"

#include "gtkcssdeclarationprivate.h"
#include "gtkcssstylesheetprivate.h"

typedef struct _GtkInspectorCssRuleViewRowPrivate GtkInspectorCssRuleViewRowPrivate;

struct _GtkInspectorCssRuleViewRowPrivate
{
  GtkCssStyleRule *rule;
  guint selector_id;

  GtkWidget *selector_label;
  GtkWidget *location_button;
  GtkWidget *style_label;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssRuleViewRow, gtk_inspector_css_rule_view_row, GTK_TYPE_LIST_BOX_ROW)

static gboolean
location_button_clicked (GtkLabel *label,
                         char     *unused,
                         gpointer  data)
{
  g_print ("Link to %s was clicked.\n", gtk_label_get_text (label));

  return TRUE;
}

static void
finalize (GObject *object)
{
  GtkInspectorCssRuleViewRow *row = GTK_INSPECTOR_CSS_RULE_VIEW_ROW (object);
  GtkInspectorCssRuleViewRowPrivate *priv = gtk_inspector_css_rule_view_row_get_instance_private (row);

  g_object_unref (priv->rule);

  G_OBJECT_CLASS (gtk_inspector_css_rule_view_row_parent_class)->finalize (object);
}

static void
gtk_inspector_css_rule_view_row_class_init (GtkInspectorCssRuleViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/cssruleviewrow.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssRuleViewRow, selector_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssRuleViewRow, location_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssRuleViewRow, style_label);
  gtk_widget_class_bind_template_callback (widget_class, location_button_clicked);
}

static void
gtk_inspector_css_rule_view_row_init (GtkInspectorCssRuleViewRow *ce)
{
  gtk_widget_init_template (GTK_WIDGET (ce));
}

static GtkCssSelector *
gtk_inspector_css_rule_view_row_get_selector (GtkInspectorCssRuleViewRow *row)
{
  GtkInspectorCssRuleViewRowPrivate *priv = gtk_inspector_css_rule_view_row_get_instance_private (row);

  return gtk_css_style_rule_get_selector (GTK_CSS_STYLE_RULE (priv->rule), priv->selector_id);
}

static void
update_selector_label (GtkInspectorCssRuleViewRow *row)
{
  GtkInspectorCssRuleViewRowPrivate *priv;
  GtkCssSelector *selector;
  GString *string;

  priv = gtk_inspector_css_rule_view_row_get_instance_private (row);
  selector = gtk_css_style_rule_get_selector (GTK_CSS_STYLE_RULE (priv->rule), priv->selector_id);
  string = g_string_new (NULL);
  _gtk_css_selector_print (selector, string);
  g_string_append (string, " {");
  gtk_label_set_text (GTK_LABEL (priv->selector_label), string->str);
  g_string_free (string, TRUE);
}

static void
update_style_label (GtkInspectorCssRuleViewRow *row)
{
  GtkInspectorCssRuleViewRowPrivate *priv;
  GtkCssStyleDeclaration *style;
  GString *string;
  guint i;

  priv = gtk_inspector_css_rule_view_row_get_instance_private (row);
  style = gtk_css_style_rule_get_style (GTK_CSS_STYLE_RULE (priv->rule));
  string = g_string_new (NULL);
  for (i = 0; i < gtk_css_style_declaration_get_length (style); i++)
    {
      GtkCssDeclaration *decl = gtk_css_style_declaration_get_declaration (style, i);

      if (i > 0)
        g_string_append (string, "\n");
      g_string_append (string, gtk_css_declaration_get_name (decl));
      g_string_append (string, ": ");
      gtk_css_declaration_print_value (decl, string);
      g_string_append (string, ";");
    }
  gtk_label_set_text (GTK_LABEL (priv->style_label), string->str);
  g_string_free (string, TRUE);
}

static void
update_location_label (GtkInspectorCssRuleViewRow *row)
{
  GtkInspectorCssRuleViewRowPrivate *priv;
  GtkCssStyleSheet *style_sheet;
  GString *string;
  GFile *file;

  priv = gtk_inspector_css_rule_view_row_get_instance_private (row);
  string = g_string_new ("<a href=\"foo\">");
  style_sheet = gtk_css_rule_get_parent_style_sheet (GTK_CSS_RULE (priv->rule));
  file = gtk_css_style_sheet_get_file (style_sheet);
  if (file)
    {
      GFileInfo *info;

      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

      if (info)
        {
          char *escaped = g_markup_escape_text (g_file_info_get_display_name (info), -1);
          g_string_append (string, escaped);
          g_free (escaped);
          g_object_unref (info);
        }
      else
        {
          g_string_append (string, "&lt;broken file&gt;");
        }
    }
  else
    {
      g_string_append (string, "&lt;data&gt;");
    }

  g_string_append (string, "</a>");
  gtk_label_set_markup (GTK_LABEL (priv->location_button), string->str);
  g_string_free (string, TRUE);
}

GtkWidget *
gtk_inspector_css_rule_view_row_new (GtkCssStyleRule *rule,
                                     guint            selector_id)
{
  GtkInspectorCssRuleViewRowPrivate *priv;
  GtkInspectorCssRuleViewRow *row;

  row = g_object_new (GTK_TYPE_INSPECTOR_CSS_RULE_VIEW_ROW, NULL);
  priv = gtk_inspector_css_rule_view_row_get_instance_private (row);

  priv->rule = g_object_ref (rule);
  priv->selector_id = selector_id;

  update_selector_label (row);
  update_location_label (row);
  update_style_label (row);

  return GTK_WIDGET (row);
}

gint
gtk_inspector_css_rule_view_row_compare_specificity (GtkInspectorCssRuleViewRow *row1,
                                                     GtkInspectorCssRuleViewRow *row2)
{
  GtkCssSelector *selector1 = gtk_inspector_css_rule_view_row_get_selector (row1);
  GtkCssSelector *selector2 = gtk_inspector_css_rule_view_row_get_selector (row2);

  return _gtk_css_selector_compare (selector1, selector2);
}
