#include <gtk/gtk.h>


typedef struct
{
  GtkWidget parent_instance;
} DemoWidget;

typedef struct
{
  GtkWidgetClass parent_class;
} DemoWidgetClass;

GType demo_widget_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static void
demo_widget_init (DemoWidget *demo)
{
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GdkRGBA red, green, yellow, blue;
  float w, h;
  GskPathBuilder *builder;
  GskPath *path;

  gdk_rgba_parse (&red, "red");
  gdk_rgba_parse (&green, "green");
  gdk_rgba_parse (&yellow, "yellow");
  gdk_rgba_parse (&blue, "blue");

  w = gtk_widget_get_width (widget) / 2.0;
  h = gtk_widget_get_height (widget) / 2.0;

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, 10, 10);
  gsk_path_builder_curve_to (builder, 100, 10, 110, 20, 110, 30);
  gsk_path_builder_curve_to (builder, 80, 30, 100, 60, 80, 60);
  gsk_path_builder_line_to (builder, 120, 100);
  gsk_path_builder_curve_to (builder, 110, 110, 80, 120, 30, 70);
  gsk_path_builder_close (builder);
  path = gsk_path_builder_free_to_path (builder);

  gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
  gsk_path_unref (path);

  gtk_snapshot_append_color (snapshot, &red,
                             &GRAPHENE_RECT_INIT(0, 0, w, h));
  gtk_snapshot_append_color (snapshot, &green,
                             &GRAPHENE_RECT_INIT(w, 0, w, h));
  gtk_snapshot_append_color (snapshot, &yellow,
                             &GRAPHENE_RECT_INIT(0, h, w, h));
  gtk_snapshot_append_color (snapshot, &blue,
                             &GRAPHENE_RECT_INIT(w, h, w, h));

  gtk_snapshot_pop (snapshot);
}

static void
demo_widget_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum_size,
                     int            *natural_size,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  *minimum_size = 100;
  *natural_size = 200;
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->snapshot = demo_widget_snapshot;
  widget_class->measure = demo_widget_measure;
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (demo_widget_get_type (), NULL);
}

int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *demo;

  gtk_init ();

  window = GTK_WINDOW (gtk_window_new ());

  demo = demo_widget_new ();

  gtk_window_set_child (window, demo);

  gtk_window_present (window);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
