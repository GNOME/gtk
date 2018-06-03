#include <gtk/gtk.h>
#include "benchmark.h"

/* Command line options */
const char *profile_benchmark_name = NULL;
static GOptionEntry options[] = {
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile_benchmark_name, "Benchmark name to profile using callgrind", NULL },
  { NULL }
};


static void
set_parent_benchmark (Benchmark *b,
                      gsize      size,
                      gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  GtkWidget **widgets;

  widgets = g_malloc (sizeof (GtkWidget *) * size);
  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    {
      widgets[i] = gtk_button_new ();
    }

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      gtk_widget_set_parent (widgets[i], w);
    }
  benchmark_stop (b);


  g_free (widgets);
}

static void
reorder_benchmark (Benchmark *b,
                   gsize      size,
                   gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  GtkWidget **widgets;

  widgets = g_malloc (sizeof (GtkWidget *) * size);
  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    {
      widgets[i] = gtk_button_new ();
      gtk_widget_set_parent (widgets[i], w);
    }

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      /* Move this child to the very end */
      gtk_widget_insert_before (widgets[i], w, NULL);
    }
  benchmark_stop (b);

  g_free (widgets);
}

static void
get_size_benchmark (Benchmark *b,
                    gsize      size,
                    gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  int width, height;
  int button_width;
  int button_height;

  w = gtk_button_new ();

  gtk_widget_measure (w, GTK_ORIENTATION_HORIZONTAL, -1, &width, NULL, NULL, NULL);
  gtk_widget_measure (w, GTK_ORIENTATION_VERTICAL, width, &height, NULL, NULL, NULL);

  button_width = 200 + width;
  button_height = 300 + height;

  gtk_widget_size_allocate (w,
                            &(GtkAllocation){0, 0, button_width, button_height}, -1);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      width = gtk_widget_get_width (w);
      height = gtk_widget_get_height (w);
    }
  benchmark_stop (b);

  g_assert_cmpint (width, <=, button_width);
  g_assert_cmpint (height, <=, button_height);
}


static void
compute_bounds_benchmark (Benchmark *b,
                          gsize      size,
                          gpointer   user_data)
{
  guint i;
  GtkWidget *w;
  int width, height;
  int button_width;
  int button_height;

  w = gtk_button_new ();

  gtk_widget_measure (w, GTK_ORIENTATION_HORIZONTAL, -1, &width, NULL, NULL, NULL);
  gtk_widget_measure (w, GTK_ORIENTATION_VERTICAL, width, &height, NULL, NULL, NULL);

  button_width = 200 + width;
  button_height = 300 + height;

  gtk_widget_size_allocate (w,
                            &(GtkAllocation){0, 0, button_width, button_height}, -1);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      graphene_rect_t r;

      gtk_widget_compute_bounds (w, w, &r);
    }
  benchmark_stop (b);
}

static void
translate_coords_benchmark (Benchmark *b,
                            gsize      size,
                            gpointer   user_data)
{
  guint i;
  GtkWidget *root;
  GtkWidget *widget_a;
  GtkWidget *widget_b;
  GtkWidget *iter;
  int x = 0;
  int y = 0;

  /* Create an unbalanced widget tree with depth @size on one side and
   * depth 1 on the other. */

  root = gtk_button_new ();
  widget_a = gtk_button_new ();
  widget_b = gtk_button_new ();

  iter = root;
  for (i = 0; i < size; i ++)
    {
      GtkWidget *w = gtk_button_new ();

      gtk_widget_set_parent (w, iter);
      iter = w;
    }

  gtk_widget_set_parent (widget_a, root);
  gtk_widget_set_parent (widget_b, iter);

  /* This will create all the CSS styles, which is the actual slow part... */
  gtk_widget_translate_coordinates (widget_a, widget_b, x, y, &x, &y);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      x = 0;
      y = 0;

      gtk_widget_translate_coordinates (widget_a, widget_b, x, y, &x, &y);
    }
  benchmark_stop (b);
}

static void
measure_benchmark (Benchmark *b,
                   gsize      size,
                   gpointer   user_data)
{
  guint i;
  GtkWidget *root;
  int min;

  /* Create an unbalanced widget tree with depth @size on one side and
   * depth 1 on the other. */

  root = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  for (i = 0; i < size; i ++)
    {
      GtkWidget *w = gtk_button_new ();

      gtk_container_add (GTK_CONTAINER (root), w);
    }

  gtk_widget_measure (root, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      gtk_widget_measure (root, GTK_ORIENTATION_HORIZONTAL, min + i, &min, NULL, NULL, NULL);
    }
  benchmark_stop (b);
}

static void
templates_benchmark (Benchmark *b,
                     gsize      size,
                     gpointer   user_data)
{
  guint i;
  GtkWidget **widgets = g_malloc (sizeof (GtkWidget *)  * size);

  /* Just load some widget using composite templates a bunch of times. */

  benchmark_start (b);
  for (i = 0; i < size; i ++)
    {
      widgets[i] = gtk_info_bar_new ();
    }
  benchmark_stop (b);

  g_free (widgets);
}

int
main (int argc, char **argv)
{
  BenchmarkSuite suite;
  GOptionContext *option_context;
  GError *error = NULL;

  option_context = g_option_context_new ("");
  g_option_context_add_main_entries (option_context, options, NULL);
  if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  benchmark_suite_init (&suite, profile_benchmark_name);
  gtk_init ();

  benchmark_suite_add (&suite, "set_parent", 10000, set_parent_benchmark, NULL);
  benchmark_suite_add (&suite, "reorder", 10000, reorder_benchmark, NULL);
  benchmark_suite_add (&suite, "get_size", 10000, get_size_benchmark, NULL);
  benchmark_suite_add (&suite, "compute_bounds", 10000, compute_bounds_benchmark, NULL);
  benchmark_suite_add (&suite, "translate_coords", 1000, translate_coords_benchmark, NULL);
  benchmark_suite_add (&suite, "measure", 10000, measure_benchmark, NULL);
  benchmark_suite_add (&suite, "templates", 10000, templates_benchmark, NULL);

  return benchmark_suite_run (&suite);
}
