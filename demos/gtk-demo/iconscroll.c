/* Benchmark/Scrolling
 *
 * This demo scrolls a view with various content.
 */

#include <gtk/gtk.h>

static guint tick_cb;
static GtkAdjustment *hadjustment;
static GtkAdjustment *vadjustment;
static GtkWidget *window = NULL;
static GtkWidget *scrolledwindow;
static int selected;

#define N_WIDGET_TYPES 4


static int hincrement = 5;
static int vincrement = 5;

static gboolean
scroll_cb (GtkWidget *widget,
           GdkFrameClock *frame_clock,
           gpointer data)
{
  double value;

  value = gtk_adjustment_get_value (vadjustment);
  if (value + vincrement <= gtk_adjustment_get_lower (vadjustment) ||
     (value + vincrement >= gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment)))
    vincrement = - vincrement;

  gtk_adjustment_set_value (vadjustment, value + vincrement);

  value = gtk_adjustment_get_value (hadjustment);
  if (value + hincrement <= gtk_adjustment_get_lower (hadjustment) ||
     (value + hincrement >= gtk_adjustment_get_upper (hadjustment) - gtk_adjustment_get_page_size (hadjustment)))
    hincrement = - hincrement;

  gtk_adjustment_set_value (hadjustment, value + hincrement);

  return G_SOURCE_CONTINUE;
}

extern GtkWidget *create_icon (void);

static void
populate_icons (void)
{
  GtkWidget *grid;
  int top, left;

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  for (top = 0; top < 100; top++)
    for (left = 0; left < 15; left++)
      gtk_grid_attach (GTK_GRID (grid), create_icon (), left, top, 1, 1);

  hincrement = 0;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
		                  GTK_POLICY_NEVER,
		                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), grid);
}

static char *content;
static gsize content_len;

extern void fontify (GtkTextBuffer *buffer);

static void
populate_text (gboolean hilight)
{
  GtkWidget *textview;
  GtkTextBuffer *buffer;

  if (!content)
    {
      GBytes *bytes;

      bytes = g_resources_lookup_data ("/sources/font_features.c", 0, NULL);
      content = g_bytes_unref_to_data (bytes, &content_len);
    }

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, content, (int)content_len);

  if (hilight)
    fontify (buffer);

  textview = gtk_text_view_new ();
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  hincrement = 0;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
		                  GTK_POLICY_NEVER,
		                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), textview);
}

static void
populate_image (void)
{
  GtkWidget *image;

  if (!content)
    {
      GBytes *bytes;

      bytes = g_resources_lookup_data ("/sources/font_features.c", 0, NULL);
      content = g_bytes_unref_to_data (bytes, &content_len);
    }

  image = gtk_picture_new_for_resource ("/sliding_puzzle/portland-rose.jpg");
  gtk_picture_set_can_shrink (GTK_PICTURE (image), FALSE);

  hincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
		                  GTK_POLICY_AUTOMATIC,
		                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), image);
}

static void
set_widget_type (int type)
{
  if (tick_cb)
    gtk_widget_remove_tick_callback (window, tick_cb);

  if (gtk_bin_get_child (GTK_BIN (scrolledwindow)))
    gtk_container_remove (GTK_CONTAINER (scrolledwindow),
                          gtk_bin_get_child (GTK_BIN (scrolledwindow)));

  selected = type;

  switch (selected)
    {
    case 0:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling icons");
      populate_icons ();
      break;

    case 1:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling plain text");
      populate_text (FALSE);
      break;

    case 2:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling complex text");
      populate_text (TRUE);
      break;

    case 3:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a big image");
      populate_image ();
      break;

    default:
      g_assert_not_reached ();
    }

  tick_cb = gtk_widget_add_tick_callback (window, scroll_cb, NULL, NULL);
}

void
iconscroll_next_clicked_cb (GtkButton *source,
                            gpointer   user_data)
{
  int new_index;

  if (selected + 1 >= N_WIDGET_TYPES)
    new_index = 0;
  else
    new_index = selected + 1;
 

  set_widget_type (new_index);
}

void
iconscroll_prev_clicked_cb (GtkButton *source,
                            gpointer   user_data)
{
  int new_index;

  if (selected - 1 < 0)
    new_index = N_WIDGET_TYPES - 1;
  else
    new_index = selected - 1;

  set_widget_type (new_index);
}

GtkWidget *
do_iconscroll (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/iconscroll/iconscroll.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      scrolledwindow = GTK_WIDGET (gtk_builder_get_object (builder, "scrolledwindow"));
      gtk_widget_realize (window);
      hadjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "hadjustment"));
      vadjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "vadjustment"));
      set_widget_type (0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
