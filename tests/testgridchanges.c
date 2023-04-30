#include <gtk/gtk.h>

#define CONTENT_TYPE_WIDGET (content_widget_get_type ())
G_DECLARE_FINAL_TYPE (ContentWidget, content_widget, CONTENT, WIDGET, GtkWidget)

struct _ContentWidget
{
  GtkWidget parent_instance;

  int pos;
  GskRenderNode *content[10];

  int width, height;
};

struct _ContentWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (ContentWidget, content_widget, GTK_TYPE_WIDGET)

static void
content_widget_init (ContentWidget *self)
{
}

static void
content_widget_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_content,
                         int            *natural_content)
{
  ContentWidget *self = CONTENT_WIDGET (widget);

  *minimum_content = *natural_content = -1;
  if (orientation == GTK_ORIENTATION_VERTICAL)
    *minimum = *natural = self->height;
   else
    *minimum = *natural = self->width;
}

static void
content_widget_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  ContentWidget *self = CONTENT_WIDGET (widget);

  gtk_snapshot_append_node (snapshot, self->content[self->pos]);
}

static void
content_widget_realize (GtkWidget *widget)
{
  ContentWidget *self = CONTENT_WIDGET (widget);
  PangoLayout *layout;

  GTK_WIDGET_CLASS (content_widget_parent_class)->realize (widget);

  layout = gtk_widget_create_pango_layout (widget, "");

  self->width = self->height = 0;

  for (unsigned int i = 0; i < 10; i++)
    {
      char text[20];
      GtkSnapshot *snapshot;
      graphene_rect_t bounds;

      g_snprintf (text, sizeof (text), "%u", i);
      pango_layout_set_text (layout, text, -1);

      snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0., 0., 0., 1. });
      self->content[i] = gtk_snapshot_free_to_node (snapshot);

      gsk_render_node_get_bounds (self->content[i], &bounds);
      self->width = MAX (self->width, ceilf (bounds.origin.x + bounds.size.width));
      self->height = MAX (self->height, ceilf (bounds.origin.y + bounds.size.height));
    }

  g_object_unref (layout);
}

static void
content_widget_unrealize (GtkWidget *widget)
{
  ContentWidget *self = CONTENT_WIDGET (widget);

  for (unsigned int i =0; i < 10; i++)
    g_clear_pointer (&self->content[i], gsk_render_node_unref);

  GTK_WIDGET_CLASS (content_widget_parent_class)->unrealize (widget);
}

static void
content_widget_class_init (ContentWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->snapshot = content_widget_snapshot;
  widget_class->measure = content_widget_measure;
  widget_class->realize = content_widget_realize;
  widget_class->unrealize = content_widget_unrealize;

  gtk_widget_class_set_css_name (widget_class, "content");
}

static GtkWidget *
content_widget_new (void)
{
  return g_object_new (CONTENT_TYPE_WIDGET, NULL);
}

static void
content_widget_set_pos (ContentWidget *self,
                        int            pos)
{
  g_return_if_fail (CONTENT_IS_WIDGET (self));
  g_return_if_fail (0 <= pos && pos < 10);

  self->pos = pos;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static GtkWidget *cells[100][100];

static gboolean
change_content (GtkWidget     *widget,
                GdkFrameClock *clock,
                gpointer       user_data)
{
  for (unsigned int i = 0; i < 100; i++)
    for (unsigned int j = 0; j < 100; j++)
      content_widget_set_pos (CONTENT_WIDGET (cells[i][j]), g_random_int_range (0, 9));

  return G_SOURCE_CONTINUE;
}

static const char css[] =
"content {\n"
"  background: pink;\n"
"  border: 1px solid black;\n"
"}";

int
main (int argc, char *argv[])
{
  GtkWidget *window, *sw, *grid;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              1000);

  window = gtk_window_new ();

  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), sw);

  grid = gtk_grid_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), grid);

  for (unsigned int i = 0; i < 100; i++)
    for (unsigned int j = 0; j < 100; j++)
      {
        GtkWidget *child;

        child = content_widget_new ();
        gtk_grid_attach (GTK_GRID (grid), child, i, j, 1, 1);
        cells[i][j] = child;
      }

  gtk_widget_add_tick_callback (window, change_content, NULL, NULL);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, FALSE);

  return 0;
}
