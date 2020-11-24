#include <gtk/gtk.h>

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())
G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;
  GskPath *path;
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
  int width, height;
  GskStroke *stroke;

  if (!self->path)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  stroke = gsk_stroke_new (1.0);
  gtk_snapshot_push_stroke (snapshot, self->path, stroke);
  gsk_stroke_free (stroke);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA){ 0, 0, 0, 1},
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);
}

static void
demo_widget_dispose (GObject *object)
{
  DemoWidget *self = DEMO_WIDGET (object);

  g_clear_pointer (&self->path, gsk_path_unref);

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

static void
demo_widget_set_path (DemoWidget *self,
                      GskPath    *path)
{
  g_clear_pointer (&self->path, gsk_path_unref);
  self->path = gsk_path_ref (path);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
activate (GtkEntry *entry,
          DemoWidget *demo)
{
  GskPath *path;

  path = gsk_path_from_string (gtk_editable_get_text (GTK_EDITABLE (entry)));
  if (path)
    {
      demo_widget_set_path (demo, path);
      gsk_path_unref (path);
    }
}

static void
init_demo (DemoWidget  *demo,
           GtkEditable *editable)
{
  GskPathBuilder *builder;
  GskPath *path;
  char *string;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (150, 150), 100);
  gsk_path_builder_add_rect (builder, 100, 100, 100, 100);
  path = gsk_path_builder_free_to_path (builder);

  demo_widget_set_path (demo, path);

  string = gsk_path_to_string (path);
  gtk_editable_set_text (editable, string);
  g_free (string);
  gsk_path_unref (path);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *demo, *entry;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  demo = demo_widget_new ();

  gtk_widget_set_hexpand (demo, TRUE);
  gtk_widget_set_vexpand (demo, TRUE);
  gtk_box_append (GTK_BOX (box), demo);

  entry = gtk_entry_new ();
  g_signal_connect (entry, "activate", G_CALLBACK (activate), demo);

  init_demo (DEMO_WIDGET (demo), GTK_EDITABLE (entry));

  gtk_box_append (GTK_BOX (box), entry);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
