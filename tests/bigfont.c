#include <gtk/gtk.h>

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())
G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static void
demo_widget_init (DemoWidget *self)
{
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  PangoLayout *layout;
  int width, height;
  int pwidth, pheight;
  PangoFontDescription *desc;
  int size;
  double scale;
  int x, y;
  GtkStyleContext *context;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "Best Aa");

  pango_layout_get_pixel_size (layout, &pwidth, &pheight);
  desc = pango_font_description_copy_static (pango_context_get_font_description (pango_layout_get_context (layout)));
  size = pango_font_description_get_size (desc);

  scale = MIN (width / (double)pwidth, height / (double)pheight);

  pango_font_description_set_size (desc, size * scale * 0.5);
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);

  pango_layout_get_pixel_size (layout, &pwidth, &pheight);

  x = floor ((width - pwidth) / 2);
  y = floor ((height - pheight) / 2);

  context = gtk_widget_get_style_context (widget);

  gtk_snapshot_render_layout (snapshot, context, x, y, layout);

  g_object_unref (layout);
}

static void
demo_widget_dispose (GObject *object)
{
  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;

  widget_class->snapshot = demo_widget_snapshot;
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (DEMO_TYPE_WIDGET, NULL);
}

static const char css[] =
  "* {\n"
  "  font-family: Cantarell;\n"
  "  font-weight: 520;\n"
  "}";

int
main (int argc, char *argv[])
{
  GtkCssProvider *style;
  GtkWidget *window;

  gtk_init ();

  style = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (style, css, strlen (css));
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (style),
                                              800);

  window = gtk_window_new ();

  gtk_window_set_child (GTK_WINDOW (window), demo_widget_new ());

  gtk_widget_show (window);

  while (1)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
