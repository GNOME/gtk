#include <gtk/gtk.h>
#include <gtk/css/gtkcssparserprivate.h>
#include <gsk/gskroundedrectprivate.h>

#define TEST_TYPE_WIDGET (test_widget_get_type ())
G_DECLARE_FINAL_TYPE (TestWidget, test_widget, TEST, WIDGET, GtkWidget)

struct _TestWidget
{
  GtkWidget parent_instance;

  GskRoundedRect rect1;
  GskRoundedRect rect2;
  GskRoundedRect rect3;
  GskRoundedRectIntersection result;
};

struct _TestWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (TestWidget, test_widget, GTK_TYPE_WIDGET)

static void
test_widget_init (TestWidget *self)
{
}

static void
test_widget_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  TestWidget *self = TEST_WIDGET (widget);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    *minimum = *natural = MAX (self->rect1.bounds.origin.x + self->rect1.bounds.size.width,
                               self->rect2.bounds.origin.x + self->rect2.bounds.size.width);
  else
    *minimum = *natural = MAX (self->rect1.bounds.origin.y + self->rect1.bounds.size.height,
                               self->rect2.bounds.origin.y + self->rect2.bounds.size.height);
}

static void
test_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  TestWidget *self = TEST_WIDGET (widget);
  float widths[4] = { 1, 1, 1, 1 };
  GdkRGBA colors1[4];
  GdkRGBA colors2[4];
  GdkRGBA colors3[4];
  GskRoundedRect rect3;

  gdk_rgba_parse (&colors1[0], "red");
  colors1[1] = colors1[2] = colors1[3] = colors1[0];

  gdk_rgba_parse (&colors2[0], "blue");
  colors2[1] = colors2[2] = colors2[3] = colors2[0];

  gdk_rgba_parse (&colors3[0], "magenta");
  colors3[1] = colors3[2] = colors3[3] = colors3[0];

  gtk_snapshot_append_border (snapshot, &self->rect1, widths, colors1);
  gtk_snapshot_append_border (snapshot, &self->rect2, widths, colors2);

  switch (gsk_rounded_rect_intersect (&self->rect1, &self->rect2, &rect3))
    {
    case GSK_INTERSECTION_NONEMPTY:
      gtk_snapshot_append_border (snapshot, &rect3, widths, colors3);
      break;
    default:
      ;
    }
}


static void
test_widget_class_init (TestWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->snapshot = test_widget_snapshot;
  widget_class->measure = test_widget_measure;
}

static GtkWidget *
test_widget_new (void)
{
  return g_object_new (TEST_TYPE_WIDGET, NULL);
}

static void
update_intersection (TestWidget *self)
{
  self->result = gsk_rounded_rect_intersect (&self->rect1, &self->rect2, &self->rect3);
}

static void
test_widget_set_rect1 (TestWidget     *self,
                       GskRoundedRect *rect)
{
  self->rect1 = *rect;
  update_intersection (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
test_widget_set_rect2 (TestWidget     *self,
                       GskRoundedRect *rect)
{
  self->rect2 = *rect;
  update_intersection (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static gboolean
parse_rect (GtkCssParser    *parser,
            graphene_rect_t *out_rect)
{
  double numbers[4];

  if (!gtk_css_parser_consume_number (parser, &numbers[0]) ||
      !gtk_css_parser_consume_number (parser, &numbers[1]) ||
      !gtk_css_parser_consume_number (parser, &numbers[2]) ||
      !gtk_css_parser_consume_number (parser, &numbers[3]))
    return FALSE;

  graphene_rect_init (out_rect, numbers[0], numbers[1], numbers[2], numbers[3]);

  return TRUE;
}

static gboolean
parse_rounded_rect (GtkCssParser   *parser,
                    GskRoundedRect *out_rect)
{
  graphene_rect_t r;
  graphene_size_t corners[4];
  double d;
  guint i;

  if (!parse_rect (parser, &r))
    return FALSE;

  if (!gtk_css_parser_try_delim (parser, '/'))
    {
      gsk_rounded_rect_init_from_rect (out_rect, &r, 0);
      return TRUE;
    }

  for (i = 0; i < 4; i++)
    {
      if (!gtk_css_parser_has_number (parser))
        break;
      if (!gtk_css_parser_consume_number (parser, &d))
        return FALSE;
      corners[i].width = d;
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      return FALSE;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases
   */
  for (; i < 4; i++)
    corners[i].width = corners[(i - 1) >> 1].width;
  if (gtk_css_parser_try_delim (parser, '/'))
    {
      gtk_css_parser_consume_token (parser);

      for (i = 0; i < 4; i++)
        {
          if (!gtk_css_parser_has_number (parser))
            break;
          if (!gtk_css_parser_consume_number (parser, &d))
            return FALSE;
          corners[i].height = d;
        }

      if (i == 0)
        {
          gtk_css_parser_error_syntax (parser, "Expected a number");
          return FALSE;
        }

      for (; i < 4; i++)
        corners[i].height = corners[(i - 1) >> 1].height;
    }
  else
    {
      for (i = 0; i < 4; i++)
        corners[i].height = corners[i].width;
    }

  gsk_rounded_rect_init (out_rect, &r, &corners[0], &corners[1], &corners[2], &corners[3]);

  return TRUE;
}

static GtkWidget *label;

static void
update_label (GtkLabel *label,
              GskRoundedRectIntersection result)
{
  const char *labels[] = {
    "Empty", "Not empty", "Not representable", "Who knows"
  };

  gtk_label_set_label (label, labels[result]);
}

static void
activate1_cb (GtkEntry *entry, TestWidget *test)
{
  GtkCssParser *parser;
  const char *text;
  GBytes *bytes;
  GskRoundedRect rect;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  bytes = g_bytes_new_static (text, strlen (text) + 1);
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  if (parse_rounded_rect (parser, &rect))
    {
      test_widget_set_rect1 (test, &rect);
      update_label (GTK_LABEL (label), test->result);
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);
}

static void
activate2_cb (GtkEntry *entry, TestWidget *test)
{
  GtkCssParser *parser;
  const char *text;
  GBytes *bytes;
  GskRoundedRect rect;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  bytes = g_bytes_new_static (text, strlen (text) + 1);
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  if (parse_rounded_rect (parser, &rect))
    {
      test_widget_set_rect2 (test, &rect);
      update_label (GTK_LABEL (label), test->result);
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *grid;
  GtkWidget *entry1;
  GtkWidget *entry2;
  GtkWidget *test;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  grid = gtk_grid_new ();
  gtk_box_append (GTK_BOX (box), grid);

  test = test_widget_new ();
  gtk_widget_set_hexpand (test, TRUE);
  gtk_widget_set_vexpand (test, TRUE);
  gtk_widget_set_halign (test, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (test, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), test);

  entry1 = gtk_entry_new ();
  g_signal_connect (entry1, "activate", G_CALLBACK (activate1_cb), test);
  gtk_grid_attach (GTK_GRID (grid), entry1, 0, 0, 1, 1);

  entry2 = gtk_entry_new ();
  g_signal_connect (entry2, "activate", G_CALLBACK (activate2_cb), test);
  gtk_grid_attach (GTK_GRID (grid), entry2, 0, 1, 1, 1);

  label = gtk_label_new ("");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, FALSE);

  return 0;
}
