#include <gtk/gtk.h>

#include "../../gtk/gtkconstrainttypesprivate.h"
#include "../../gtk/gtkconstraintsolverprivate.h"
#include "../../gtk/gtkconstraintexpressionprivate.h"

typedef struct _Node Node;

static GtkConstraintSolver *solver;
static Node *tree;
static Node *drag_node;
static double drag_start_x;
static double drag_start_y;
static GtkConstraintVariable *width_var;
static GtkConstraintVariable *height_var;

struct _Node {
  double x;
  double y;
  Node *parent;
  Node *left;
  Node *right;

  GtkConstraintVariable *x_var;
  GtkConstraintVariable *y_var;
};

static Node *
make_tree (Node *parent,
           int   depth,
           int   x,
           int   y,
           int   dx,
           int   dy)
{
  Node *node;

  node = g_new0 (Node, 1);
  node->parent = parent;

  if (depth > 0)
    {
      node->left = make_tree (node, depth - 1, x - dx, y + dy, dx / 2, dy);
      node->right = make_tree (node, depth - 1, x  + dx, y + dy, dx / 2, dy);
    }

  node->x = x;
  node->y = y;

  node->x_var = gtk_constraint_solver_create_variable (solver, NULL, "x", x);
  node->y_var = gtk_constraint_solver_create_variable (solver, NULL, "y", y);

  /* weak stay for the current position */
  gtk_constraint_solver_add_stay_variable (solver, node->x_var, GTK_CONSTRAINT_WEIGHT_WEAK);
  gtk_constraint_solver_add_stay_variable (solver, node->y_var, GTK_CONSTRAINT_WEIGHT_WEAK);

  /* require to stay in area */
  gtk_constraint_solver_add_constraint (solver,
                                        node->x_var,
                                        GTK_CONSTRAINT_RELATION_GE,
                                        gtk_constraint_expression_new (0.0),
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_solver_add_constraint (solver,
                                        node->x_var,
                                        GTK_CONSTRAINT_RELATION_LE,
                                        gtk_constraint_expression_new (1600.0),
                                        //gtk_constraint_expression_new_from_variable (width_var),
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_solver_add_constraint (solver,
                                        node->y_var,
                                        GTK_CONSTRAINT_RELATION_GE,
                                        gtk_constraint_expression_new (0.0),
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_solver_add_constraint (solver,
                                        node->y_var,
                                        GTK_CONSTRAINT_RELATION_LE,
                                        gtk_constraint_expression_new (600.0),
                                        //gtk_constraint_expression_new_from_variable (height_var),
                                        GTK_CONSTRAINT_WEIGHT_REQUIRED);

  if (node->left)
    {
      GtkConstraintExpressionBuilder builder;

      /* left.y = right.y */
      gtk_constraint_solver_add_constraint (solver,
                                            node->left->y_var,
                                            GTK_CONSTRAINT_RELATION_EQ,
                                            gtk_constraint_expression_new_from_variable (node->right->y_var),
                                            GTK_CONSTRAINT_WEIGHT_REQUIRED);

      /* left.y >= parent.y + 10 */
      gtk_constraint_expression_builder_init (&builder, solver);
      gtk_constraint_expression_builder_term (&builder, node->y_var);
      gtk_constraint_expression_builder_plus (&builder);
      gtk_constraint_expression_builder_constant (&builder, 10.0);
      gtk_constraint_solver_add_constraint (solver,
                                            node->left->y_var,
                                            GTK_CONSTRAINT_RELATION_GE,
                                            gtk_constraint_expression_builder_finish (&builder),
                                            GTK_CONSTRAINT_WEIGHT_REQUIRED);

      /* right.y >= parent.y + 10 */
      gtk_constraint_expression_builder_init (&builder, solver);
      gtk_constraint_expression_builder_term (&builder, node->y_var);
      gtk_constraint_expression_builder_plus (&builder);
      gtk_constraint_expression_builder_constant (&builder, 10.0);
      gtk_constraint_solver_add_constraint (solver,
                                            node->right->y_var,
                                            GTK_CONSTRAINT_RELATION_GE,
                                            gtk_constraint_expression_builder_finish (&builder),
                                            GTK_CONSTRAINT_WEIGHT_REQUIRED);
      /* parent.x = (left.x + right.x) / 2 */
      gtk_constraint_expression_builder_init (&builder, solver);
      gtk_constraint_expression_builder_term (&builder, node->left->x_var);
      gtk_constraint_expression_builder_plus (&builder);
      gtk_constraint_expression_builder_term (&builder, node->right->x_var);
      gtk_constraint_expression_builder_divide_by (&builder);
      gtk_constraint_expression_builder_constant (&builder, 2.0);
      gtk_constraint_solver_add_constraint (solver,
                                            node->x_var,
                                            GTK_CONSTRAINT_RELATION_EQ,
                                            gtk_constraint_expression_builder_finish (&builder),
                                            GTK_CONSTRAINT_WEIGHT_REQUIRED);
    }

  return node;
}

static void
draw_node (Node *node, cairo_t *cr)
{
  if (node->left)
    draw_node (node->left, cr);
  if (node->right)
    draw_node (node->right, cr);

  if (node->parent)
    {
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_move_to (cr, node->parent->x, node->parent->y);
      cairo_line_to (cr, node->x, node->y);
      cairo_stroke (cr);
    }

  if (node == drag_node)
    cairo_set_source_rgb (cr, 1, 0, 0);
  else
    cairo_set_source_rgb (cr, 0, 0, 0);

  cairo_move_to (cr, node->x, node->y);
  cairo_arc (cr, node->x, node->y, 5, 0, 2*M_PI);
  cairo_close_path (cr);
  cairo_fill (cr);
}

static void
draw_func (GtkDrawingArea *da,
           cairo_t        *cr,
           int             width,
           int             height,
           gpointer        data)
{
  cairo_set_line_width (cr, 1);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  draw_node (tree, cr);
}

static Node *
find_node (Node   *node,
           double  x,
           double  y)
{
  Node *ret;

  double dx = x - node->x;
  double dy = y - node->y;

  if (dx*dx + dy*dy < 10*10)
    return node;

  if (node->left)
    {
      ret = find_node (node->left, x, y);
      if (ret)
        return ret;
    }

  if (node->right)
    {
      ret = find_node (node->right, x, y);
      if (ret)
        return ret;
    }

  return NULL;
}

static void
drag_begin (GtkGestureDrag *drag,
            double          start_x,
            double          start_y,
            gpointer        data)
{
  drag_node = find_node (tree, start_x, start_y);
  if (!drag_node)
    return;

  drag_start_x = start_x;
  drag_start_y = start_y;
  gtk_widget_queue_draw (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag)));

  gtk_constraint_solver_add_edit_variable (solver,
                                           drag_node->x_var,
                                           GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_solver_add_edit_variable (solver,
                                           drag_node->y_var,
                                           GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_solver_begin_edit (solver);
}

static void
update_tree (Node *node)
{
  if (!node)
    return;

  node->x = gtk_constraint_variable_get_value (node->x_var);
  node->y = gtk_constraint_variable_get_value (node->y_var);

  update_tree (node->left);
  update_tree (node->right);
}

static void
drag_update (GtkGestureDrag *drag,
             double          offset_x,
             double          offset_y,
             gpointer        data)
{
  if (!drag_node)
    return;

  gtk_constraint_solver_suggest_value (solver,
                                       drag_node->x_var,
                                       drag_start_x + offset_x);
  gtk_constraint_solver_suggest_value (solver,
                                       drag_node->y_var,
                                       drag_start_y + offset_y);
  gtk_constraint_solver_resolve (solver);

  update_tree (tree);

  gtk_widget_queue_draw (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag)));
}

static void
drag_end (GtkGestureDrag *drag,
          double          offset_x,
          double          offset_y,
          gpointer        data)
{
  if (!drag_node)
    return;

  gtk_constraint_solver_remove_edit_variable (solver, drag_node->x_var);
  gtk_constraint_solver_remove_edit_variable (solver, drag_node->y_var);
  gtk_constraint_solver_end_edit (solver);

  drag_node = NULL;

  gtk_widget_queue_draw (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag)));
}

static void
size_change (GtkWidget *da,
             int        width,
             int        height,
             int        baseline,
             gpointer   data)
{
  gtk_constraint_variable_set_value (width_var, width);
  gtk_constraint_variable_set_value (height_var, height);
  gtk_constraint_solver_resolve (solver);
}

static void
reset_tree (Node *node,
            int   x,
            int   y,
            int   dx,
            int   dy)
{
  node->x = x;
  node->y = y;

  gtk_constraint_solver_remove_stay_variable (solver, node->x_var);
  gtk_constraint_solver_remove_stay_variable (solver, node->y_var);
  gtk_constraint_variable_set_value (node->x_var, x);
  gtk_constraint_variable_set_value (node->y_var, y);
  gtk_constraint_solver_add_stay_variable (solver, node->x_var, GTK_CONSTRAINT_WEIGHT_WEAK);
  gtk_constraint_solver_add_stay_variable (solver, node->y_var, GTK_CONSTRAINT_WEIGHT_WEAK);

  if (node->left)
    reset_tree (node->left, x - dx, y + dy, dx / 2, dy);
  if (node->right)
    reset_tree (node->right, x  + dx, y + dy, dx / 2, dy);
}

static void
reset (GtkButton *button,
       GtkWidget *da)
{
  int width, height;

  width = gtk_widget_get_allocated_width (da);
  height = gtk_widget_get_allocated_height (da);

  gtk_constraint_solver_freeze (solver);
  reset_tree (tree, width / 2, 20, width / 4 - 40, (height - 40) / 7);
  gtk_constraint_solver_thaw (solver);

  gtk_widget_queue_draw (da);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *button;
  GtkWidget *da;
  GtkGesture *drag;
  int width = 1600;
  int height = 600;

  gtk_init ();

  da = gtk_drawing_area_new ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  header = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
  button = gtk_button_new_with_label ("Reset");
  g_signal_connect (button, "clicked", G_CALLBACK (reset), da);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), width);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), height);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), draw_func, NULL, NULL);

  gtk_container_add (GTK_CONTAINER (window), da);

  drag = gtk_gesture_drag_new ();
  g_signal_connect (drag, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (drag, "drag-update", G_CALLBACK (drag_update), NULL);
  g_signal_connect (drag, "drag-end", G_CALLBACK (drag_end), NULL);
  gtk_widget_add_controller (da, GTK_EVENT_CONTROLLER (drag));

  solver = g_object_new (g_type_from_name ("GtkConstraintSolver"), NULL);
  gtk_constraint_solver_freeze (solver);

  width_var = gtk_constraint_solver_create_variable (solver, NULL, "width", width);
  height_var = gtk_constraint_solver_create_variable (solver, NULL, "height", height);
  gtk_constraint_solver_add_stay_variable (solver, width_var, GTK_CONSTRAINT_WEIGHT_REQUIRED);
  gtk_constraint_solver_add_stay_variable (solver, height_var, GTK_CONSTRAINT_WEIGHT_REQUIRED);

  g_signal_connect (da, "size-allocate", G_CALLBACK (size_change), NULL);

  tree = make_tree (NULL, 7, width / 2, 20, width / 4 - 40, (height - 40) / 7);

  gtk_constraint_solver_thaw (solver);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
