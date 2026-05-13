/* SPDX-FileCopyrightText: 2026 Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <locale.h>

#include <gtk/gtk.h>
#include "../../gtk/gtkconstraintvflparserprivate.h"
#include "../../gtk/gtkconstrainttypesprivate.h"


static struct {
  const char *id;
  const char *expression;
  const char *views[5];
  const char *metrics[5];
} vfl_valid[] = {
  { "standard-space", "[button]-[text_field]", { "button", "text_field", NULL, }, { NULL, } },
  { "width-constraint", "[button(>=50)]", { "button", NULL, }, { NULL } },
  { "connection-superview", "|-50-[purpleBox]-50-|", { "purpleBox", NULL, }, { NULL, } },
  { "leading-superview", "|-[view]", { "view", NULL, }, { NULL, } },
  { "trailing-superview", "[view]|", { "view", NULL, }, { NULL, } },
  { "vertical-layout", "V:[topField]-10-[bottomField]", { "topField", "bottomField", NULL, }, { NULL, } },
  { "flush-views", "[maroonView][blueView]", { "maroonView", "blueView", NULL, }, { NULL, } },
  { "priority", "[button(100@strong)]", { "button", NULL, }, { NULL, } },
  { "equal-widths", "[button1(==button2)]", { "button1", "button2", NULL, }, { NULL, } },
  { "multiple-predicates", "[flexibleButton(>=70,<=100)]", { "flexibleButton", NULL, }, { NULL, } },
  { "complete-line", "|-[find]-[findNext]-[findField(>=20)]-|", { "find", "findNext", "findField", NULL, }, { NULL, } },
  { "grid-1", "H:|-8-[view1(==view2)]-12-[view2]-8-|", { "view1", "view2", NULL, }, { NULL, } },
  { "grid-2", "H:|-8-[view3]-8-|", { "view3", NULL, }, { NULL, } },
  { "grid-3", "V:|-8-[view1]-12-[view3(==view1,view2)]-8-|", { "view1", "view2", "view3", NULL, }, { NULL, } },
  { "predicate-spacing-1", "|-(>=0)-[view]-(>=0)-|", { "view", NULL, }, { NULL, } },
  { "predicate-numeric-priority", "[view(==0@500)]", { "view", NULL, }, { NULL, }, },
  { "predicate-spacing-priority", "[view1]-(==0@500)-[view2]", { "view1", "view2", NULL, }, { NULL, }, },
  { "predicate-view-operator", "[view1(view2 * 2.0 + 20)]", { "view1", "view2", NULL, }, { NULL, }, },
  { "predicate-metric-operator", "|-(metric1/2-20.0)-", { NULL, }, { "metric1", NULL, }, },
  { "predicate-attribute", "[view1(view1.height)]", { "view1", NULL, }, { NULL, }, },
};

static struct {
  const char *id;
  const char *expression;
  const char *views[5];
  GtkConstraintVflParserError error_id;
} vfl_invalid[] = {
  { "orientation-invalid", "V|[backgroundBox]|", { "backgroundBox", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL, },
  { "missing-view-terminator", "[backgroundBox)", { "backgroundBox", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL, },
  { "invalid-predicate", "[backgroundBox(]", { "backgroundBox", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL, },
  { "view-not-found", "[view]", { NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_VIEW, },
  { "trailing-spacing", "[view]-", { "view", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL, },
  { "leading-spacing", "-[view]", { "view", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL, },
  { "view-invalid-identifier-1", "[[", { NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_VIEW, },
  { "view-invalid-identifier-2", "[9ab]", { NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_VIEW, },
  { "view-invalid-identifier-3", "[-a]", { NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_VIEW, },
  { "predicate-wrong-relation", "[view(>30)]", { "view", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_RELATION, },
  { "predicate-wrong-priority", "[view(>=30@foo)]", { "view", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_PRIORITY, },
  { "predicate-wrong-operator", "[view(view + wrong)]", { "view", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_SYMBOL, },
  { "predicate-wrong-attribute", "[view(view.wrong)]", { "view", NULL, }, GTK_CONSTRAINT_VFL_PARSER_ERROR_INVALID_ATTRIBUTE, },
};

static void
vfl_parser_valid (gconstpointer data)
{
  int idx = GPOINTER_TO_INT (data);
  GtkConstraintVflParser *parser = gtk_constraint_vfl_parser_new ();
  GError *error = NULL;
  GtkConstraintVfl *constraints;
  int n_constraints = 0;
  GHashTable *views, *metrics;

  g_test_message ("Parsing [valid]: '%s'", vfl_valid[idx].expression);

  views = g_hash_table_new (g_str_hash, g_str_equal);
  for (int i = 0; vfl_valid[idx].views[i] != NULL; i++)
    g_hash_table_add (views, (char *) vfl_valid[idx].views[i]);

  gtk_constraint_vfl_parser_set_views (parser, views);

  metrics = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
  for (int i = 0; vfl_valid[idx].metrics[i] != NULL; i++)
    {
      double *v = g_new (double, 1);
      *v = g_random_double_range (0, 10);
      g_hash_table_insert (metrics, (char *) vfl_valid[idx].metrics[i], v);
    }

  gtk_constraint_vfl_parser_set_metrics (parser, metrics);

  gtk_constraint_vfl_parser_parse_line (parser, vfl_valid[idx].expression, -1, &error);
  g_assert_no_error (error);

  constraints = gtk_constraint_vfl_parser_get_constraints (parser, &n_constraints);
  g_assert_nonnull (constraints);
  g_assert_cmpint (n_constraints, !=, 0);

  g_free (constraints);

  gtk_constraint_vfl_parser_free (parser);

  g_hash_table_unref (metrics);
  g_hash_table_unref (views);
}

static void
vfl_parser_invalid (gconstpointer data)
{
  int idx = GPOINTER_TO_INT (data);
  GtkConstraintVflParser *parser = gtk_constraint_vfl_parser_new ();
  GError *error = NULL;
  GHashTable *views;

  g_test_message ("Parsing [invalid]: '%s'", vfl_invalid[idx].expression);

  views = g_hash_table_new (g_str_hash, g_str_equal);
  for (int i = 0; vfl_invalid[idx].views[i] != NULL; i++)
    g_hash_table_add (views, (char *) vfl_invalid[idx].views[i]);

  gtk_constraint_vfl_parser_set_views (parser, views);

  gtk_constraint_vfl_parser_parse_line (parser, vfl_invalid[idx].expression, -1, &error);

  g_assert_nonnull (error);
  g_test_message ("Error: %s", error->message);

  g_assert_error (error, GTK_CONSTRAINT_VFL_PARSER_ERROR, vfl_invalid[idx].error_id);

  g_error_free (error);
  gtk_constraint_vfl_parser_free (parser);
  g_hash_table_unref (views);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  for (int i = 0; i < G_N_ELEMENTS (vfl_invalid); i++)
    {
      char *path = g_strconcat ("/constraint-vfl/invalid/", vfl_invalid[i].id, NULL);

      g_test_add_data_func (path, GINT_TO_POINTER (i), vfl_parser_invalid);

      g_free (path);
    }

  for (int i = 0; i < G_N_ELEMENTS (vfl_valid); i++)
    {
      char *path = g_strconcat ("/constraint-vfl/valid/", vfl_valid[i].id, NULL);

      g_test_add_data_func (path, GINT_TO_POINTER (i), vfl_parser_valid);

      g_free (path);
    }

  return g_test_run ();
}
