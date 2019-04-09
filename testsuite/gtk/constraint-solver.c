#include <locale.h>

#include "../../gtk/gtkconstrainttypesprivate.h"
#include "../../gtk/gtkconstraintsolverprivate.h"
#include "../../gtk/gtkconstraintexpressionprivate.h"

static void
constraint_solver_simple (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 167.0);
  GtkConstraintVariable *y = gtk_constraint_solver_create_variable (solver, NULL, "y", 2.0);

  GtkConstraintExpression *e = gtk_constraint_expression_new_from_variable (y);

  gtk_constraint_solver_add_constraint (solver,
                                        x, GTK_CONSTRAINT_RELATION_EQ, e,
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);

  double x_value = gtk_constraint_variable_get_value (x);
  double y_value = gtk_constraint_variable_get_value (y);

  g_assert_cmpfloat_with_epsilon (x_value, y_value, 0.001);
  g_assert_cmpfloat_with_epsilon (x_value, 0.0, 0.001);
  g_assert_cmpfloat_with_epsilon (y_value, 0.0, 0.001);

  gtk_constraint_expression_unref (e);
  gtk_constraint_variable_unref (y);
  gtk_constraint_variable_unref (x);

  g_object_unref (solver);
}

static void
constraint_solver_stay (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *x = gtk_constraint_solver_create_variable (solver, NULL, "x", 5.0);
  GtkConstraintVariable *y = gtk_constraint_solver_create_variable (solver, NULL, "y", 10.0);

  gtk_constraint_solver_add_stay_variable (solver, x, GTK_CONSTRAINT_WEIGHT_WEAK);
  gtk_constraint_solver_add_stay_variable (solver, y, GTK_CONSTRAINT_WEIGHT_WEAK);

  double x_value = gtk_constraint_variable_get_value (x);
  double y_value = gtk_constraint_variable_get_value (y);

  g_assert_cmpfloat_with_epsilon (x_value, 5.0, 0.001);
  g_assert_cmpfloat_with_epsilon (y_value, 10.0, 0.001);

  gtk_constraint_variable_unref (x);
  gtk_constraint_variable_unref (y);

  g_object_unref (solver);
}

static void
constraint_solver_paper (void)
{
  GtkConstraintSolver *solver = gtk_constraint_solver_new ();

  GtkConstraintVariable *left = gtk_constraint_solver_create_variable (solver, NULL, "left", 0.0);
  GtkConstraintVariable *middle = gtk_constraint_solver_create_variable (solver, NULL, "middle", 0.0);
  GtkConstraintVariable *right = gtk_constraint_solver_create_variable (solver, NULL, "right", 0.0);

  GtkConstraintExpressionBuilder builder;
  GtkConstraintExpression *expr;

  gtk_constraint_expression_builder_init (&builder, solver);
  gtk_constraint_expression_builder_term (&builder, left);
  gtk_constraint_expression_builder_plus (&builder);
  gtk_constraint_expression_builder_term (&builder, right);
  gtk_constraint_expression_builder_divide_by (&builder);
  gtk_constraint_expression_builder_constant (&builder, 2.0);
  expr = gtk_constraint_expression_builder_finish (&builder);
  gtk_constraint_solver_add_constraint (solver,
                                        middle, GTK_CONSTRAINT_RELATION_EQ, expr,
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_expression_unref (expr);

  gtk_constraint_expression_builder_init (&builder, solver);
  gtk_constraint_expression_builder_term (&builder, left);
  gtk_constraint_expression_builder_plus (&builder);
  gtk_constraint_expression_builder_constant (&builder, 10.0);
  expr = gtk_constraint_expression_builder_finish (&builder);
  gtk_constraint_solver_add_constraint (solver,
                                        right, GTK_CONSTRAINT_RELATION_EQ, expr,
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_expression_unref (expr);

  expr = gtk_constraint_expression_new (100.0);
  gtk_constraint_solver_add_constraint (solver,
                                        right, GTK_CONSTRAINT_RELATION_LE, expr,
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_expression_unref (expr);

  expr = gtk_constraint_expression_new (0.0);
  gtk_constraint_solver_add_constraint (solver,
                                        left, GTK_CONSTRAINT_RELATION_GE, expr,
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_expression_unref (expr);

  g_test_message ("Check constraints hold");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (middle),
                                  (gtk_constraint_variable_get_value (left) + gtk_constraint_variable_get_value (right)) / 2.0,
                                  0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (right),
                                  gtk_constraint_variable_get_value (left) + 10.0,
                                  0.001);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (right), <=, 100.0);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (left), >=, 0.0);

  gtk_constraint_variable_set_value (middle, 45.0);
  gtk_constraint_solver_add_stay_variable (solver, middle, GTK_CONSTRAINT_WEIGHT_WEAK);

  g_test_message ("Check constraints hold after setting middle");

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (middle),
                                  (gtk_constraint_variable_get_value (left) + gtk_constraint_variable_get_value (right)) / 2.0,
                                  0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (right),
                                  gtk_constraint_variable_get_value (left) + 10.0,
                                  0.001);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (right), <=, 100.0);
  g_assert_cmpfloat (gtk_constraint_variable_get_value (left), >=, 0.0);

  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (left), 40.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (middle), 45.0, 0.001);
  g_assert_cmpfloat_with_epsilon (gtk_constraint_variable_get_value (right), 50.0, 0.001);

  gtk_constraint_variable_unref (left);
  gtk_constraint_variable_unref (middle);
  gtk_constraint_variable_unref (right);

  g_object_unref (solver);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/constraint-solver/simple", constraint_solver_simple);
  g_test_add_func ("/constraint-solver/stay", constraint_solver_stay);
  g_test_add_func ("/constraint-solver/paper", constraint_solver_paper);

  return g_test_run ();
}
