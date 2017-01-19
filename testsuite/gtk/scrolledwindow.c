#include <gtk/gtk.h>

#define MIN_SIZE 150
#define MAX_SIZE 300
#define BOX_SIZE 600

typedef enum
{
  MINIMUM_CONTENT = 1 << 0,
  MAXIMUM_CONTENT = 1 << 1
} TestProperty;

static void
test_size (GtkOrientation orientation,
           TestProperty   prop)
{
  GtkWidget *scrolledwindow, *box;
  int size, child_size;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), box);

  /* Testing the content-width property */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (prop & MINIMUM_CONTENT)
        {
          gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolledwindow), MIN_SIZE);

          gtk_widget_get_preferred_width (scrolledwindow, &size, NULL);

          g_assert_cmpint (size, ==, MIN_SIZE);
        }

      if (prop & MAXIMUM_CONTENT)
        {
          gtk_scrolled_window_set_max_content_width (GTK_SCROLLED_WINDOW (scrolledwindow), MAX_SIZE);
          gtk_widget_set_size_request (box, BOX_SIZE, -1);

          /*
           * Here, the content is purposely bigger than the scrolled window,
           * so it should grow up to max-content-width.
           */
          gtk_widget_get_preferred_width (scrolledwindow, NULL, &size);
          gtk_widget_get_preferred_width (box, &child_size, NULL);

          g_assert_cmpint (child_size, ==, BOX_SIZE);
          g_assert_cmpint (size, ==, MAX_SIZE);
        }
    }
  /* Testing the content-height property */
  else
    {
      if (prop & MINIMUM_CONTENT)
        {
          gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), MIN_SIZE);

          gtk_widget_get_preferred_height (scrolledwindow, &size, NULL);

          g_assert_cmpint (size, ==, MIN_SIZE);
        }

      if (prop & MAXIMUM_CONTENT)
        {
          gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), MAX_SIZE);
          gtk_widget_set_size_request (box, -1, BOX_SIZE);

          /*
           * Here, the content is purposely bigger than the scrolled window,
           * so it should grow up to max-content-height.
           */
          gtk_widget_get_preferred_height (scrolledwindow, NULL, &size);
          gtk_widget_get_preferred_height (box, &child_size, NULL);

          g_assert_cmpint (child_size, ==, BOX_SIZE);
          g_assert_cmpint (size, ==, MAX_SIZE);
        }
    }
}


static void
min_content_width (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT);
}

static void
min_content_height (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT);
}

static void
max_content_width (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, MAXIMUM_CONTENT);
}

static void
max_content_height (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, MAXIMUM_CONTENT);
}

static void
min_max_content_width (void)
{
  test_size (GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
min_max_content_height (void)
{
  test_size (GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

int
main (int argc, char **argv)
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/sizing/scrolledwindow/min_content_width", min_content_width);
  g_test_add_func ("/sizing/scrolledwindow/min_content_height", min_content_height);

  g_test_add_func ("/sizing/scrolledwindow/max_content_width", max_content_width);
  g_test_add_func ("/sizing/scrolledwindow/max_content_height", max_content_height);

  g_test_add_func ("/sizing/scrolledwindow/min_max_content_width", min_max_content_width);
  g_test_add_func ("/sizing/scrolledwindow/min_max_content_height", min_max_content_height);

  return g_test_run ();
}
