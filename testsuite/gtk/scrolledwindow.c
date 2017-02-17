#include <gtk/gtk.h>

#define EXPECTED_MIN_SIZE 150
#define EXPECTED_MAX_SIZE 300
#define EXPECTED_BOX_SIZE 600

typedef enum
{
  MINIMUM_CONTENT = 1 << 0,
  MAXIMUM_CONTENT = 1 << 1
} TestProperty;

static void
test_size (GtkOrientation orientation,
           gboolean       overlay,
           GtkPolicyType  policy,
           TestProperty   prop)
{
  GtkWidget *scrolledwindow, *box;
  int min_size, max_size, child_size;
  int scrollbar_size = 0;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scrolledwindow), overlay);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), policy, policy);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), box);
  gtk_widget_show_all (scrolledwindow);

  /* Testing the content-width property */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (prop & MINIMUM_CONTENT)
        {
          gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolledwindow), EXPECTED_MIN_SIZE);

          gtk_widget_get_preferred_width (scrolledwindow, &min_size, NULL);
        }

      if (prop & MAXIMUM_CONTENT)
        {
          gtk_scrolled_window_set_max_content_width (GTK_SCROLLED_WINDOW (scrolledwindow), EXPECTED_MAX_SIZE);
          gtk_widget_set_size_request (box, EXPECTED_BOX_SIZE, -1);

          /*
           * Here, the content is purposely bigger than the scrolled window,
           * so it should grow up to max-content-width.
           */
          gtk_widget_get_preferred_width (scrolledwindow, NULL, &max_size);
          gtk_widget_get_preferred_width (box, &child_size, NULL);
        }

      /* If the relevant scrollbar is non-overlay and always shown, it is added
       * to the preferred size. When comparing to the expected size, we need to
       * to exclude that extra, as we are only interested in the content size */
      if (!overlay && policy == GTK_POLICY_ALWAYS)
        {
          GtkWidget *scrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolledwindow));
          gtk_widget_get_preferred_width (scrollbar, &scrollbar_size, NULL);
        }
    }
  /* Testing the content-height property */
  else
    {
      if (prop & MINIMUM_CONTENT)
        {
          gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), EXPECTED_MIN_SIZE);

          gtk_widget_get_preferred_height (scrolledwindow, &min_size, NULL);
        }

      if (prop & MAXIMUM_CONTENT)
        {
          gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), EXPECTED_MAX_SIZE);
          gtk_widget_set_size_request (box, -1, EXPECTED_BOX_SIZE);

          /*
           * Here, the content is purposely bigger than the scrolled window,
           * so it should grow up to max-content-height.
           */
          gtk_widget_get_preferred_height (scrolledwindow, NULL, &max_size);
          gtk_widget_get_preferred_height (box, &child_size, NULL);
        }

      if (!overlay && policy == GTK_POLICY_ALWAYS)
        {
          GtkWidget *scrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (scrolledwindow));
          gtk_widget_get_preferred_height (scrollbar, &scrollbar_size, NULL);
        }
    }

  /* Account for the scrollbar, if it was included in the preferred size */
  min_size -= scrollbar_size;
  max_size -= scrollbar_size;

  if (prop & MINIMUM_CONTENT)
    g_assert_cmpint (min_size, ==, EXPECTED_MIN_SIZE);

  if (prop & MAXIMUM_CONTENT)
    {
      g_assert_cmpint (child_size, ==, EXPECTED_BOX_SIZE);
      g_assert_cmpint (max_size, ==, EXPECTED_MAX_SIZE);
    }
}


static void
min_content_width_overlay_automatic (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, TRUE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT);
}

static void
min_content_height_overlay_automatic (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, TRUE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT);
}

static void
max_content_width_overlay_automatic (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, TRUE, GTK_POLICY_AUTOMATIC, MAXIMUM_CONTENT);
}

static void
max_content_height_overlay_automatic (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, TRUE, GTK_POLICY_AUTOMATIC, MAXIMUM_CONTENT);
}

static void
min_max_content_width_overlay_automatic (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, TRUE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
min_max_content_height_overlay_automatic (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, TRUE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


static void
min_content_width_fixed_automatic (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, FALSE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT);
}

static void
min_content_height_fixed_automatic (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, FALSE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT);
}

static void
max_content_width_fixed_automatic (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, FALSE, GTK_POLICY_AUTOMATIC, MAXIMUM_CONTENT);
}

static void
max_content_height_fixed_automatic (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, FALSE, GTK_POLICY_AUTOMATIC, MAXIMUM_CONTENT);
}

static void
min_max_content_width_fixed_automatic (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, FALSE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
min_max_content_height_fixed_automatic (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, FALSE, GTK_POLICY_AUTOMATIC, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


static void
min_content_width_overlay_always (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, TRUE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT);
}

static void
min_content_height_overlay_always (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, TRUE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT);
}

static void
max_content_width_overlay_always (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, TRUE, GTK_POLICY_ALWAYS, MAXIMUM_CONTENT);
}

static void
max_content_height_overlay_always (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, TRUE, GTK_POLICY_ALWAYS, MAXIMUM_CONTENT);
}

static void
min_max_content_width_overlay_always (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, TRUE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
min_max_content_height_overlay_always (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, TRUE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


static void
min_content_width_fixed_always (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, FALSE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT);
}

static void
min_content_height_fixed_always (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, FALSE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT);
}

static void
max_content_width_fixed_always (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, FALSE, GTK_POLICY_ALWAYS, MAXIMUM_CONTENT);
}

static void
max_content_height_fixed_always (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, FALSE, GTK_POLICY_ALWAYS, MAXIMUM_CONTENT);
}

static void
min_max_content_width_fixed_always (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, FALSE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
min_max_content_height_fixed_always (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, FALSE, GTK_POLICY_ALWAYS, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/sizing/scrolledwindow/min_content_width_overlay_automatic", min_content_width_overlay_automatic);
  g_test_add_func ("/sizing/scrolledwindow/min_content_height_overlay_automatic", min_content_height_overlay_automatic);
  g_test_add_func ("/sizing/scrolledwindow/max_content_width_overlay_automatic", max_content_width_overlay_automatic);
  g_test_add_func ("/sizing/scrolledwindow/max_content_height_overlay_automatic", max_content_height_overlay_automatic);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_width_overlay_automatic", min_max_content_width_overlay_automatic);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_height_overlay_automatic", min_max_content_height_overlay_automatic);

  g_test_add_func ("/sizing/scrolledwindow/min_content_width_fixed_automatic", min_content_width_fixed_automatic);
  g_test_add_func ("/sizing/scrolledwindow/min_content_height_fixed_automatic", min_content_height_fixed_automatic);
  g_test_add_func ("/sizing/scrolledwindow/max_content_width_fixed_automatic", max_content_width_fixed_automatic);
  g_test_add_func ("/sizing/scrolledwindow/max_content_height_fixed_automatic", max_content_height_fixed_automatic);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_width_fixed_automatic", min_max_content_width_fixed_automatic);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_height_fixed_automatic", min_max_content_height_fixed_automatic);

  g_test_add_func ("/sizing/scrolledwindow/min_content_width_overlay_always", min_content_width_overlay_always);
  g_test_add_func ("/sizing/scrolledwindow/min_content_height_overlay_always", min_content_height_overlay_always);
  g_test_add_func ("/sizing/scrolledwindow/max_content_width_overlay_always", max_content_width_overlay_always);
  g_test_add_func ("/sizing/scrolledwindow/max_content_height_overlay_always", max_content_height_overlay_always);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_width_overlay_always", min_max_content_width_overlay_always);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_height_overlay_always", min_max_content_height_overlay_always);

  g_test_add_func ("/sizing/scrolledwindow/min_content_width_fixed_always", min_content_width_fixed_always);
  g_test_add_func ("/sizing/scrolledwindow/min_content_height_fixed_always", min_content_height_fixed_always);
  g_test_add_func ("/sizing/scrolledwindow/max_content_width_fixed_always", max_content_width_fixed_always);
  g_test_add_func ("/sizing/scrolledwindow/max_content_height_fixed_always", max_content_height_fixed_always);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_width_fixed_always", min_max_content_width_fixed_always);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_height_fixed_always", min_max_content_height_fixed_always);

  return g_test_run ();
}
