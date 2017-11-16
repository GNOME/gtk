#include <gtk/gtk.h>


typedef struct _GtkNodeView      GtkNodeView;
typedef struct _GtkNodeViewClass GtkNodeViewClass;

#define GTK_TYPE_NODE_VIEW           (gtk_node_view_get_type ())
#define GTK_NODE_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_NODE_VIEW, GtkNodeView))
#define GTK_NODE_VIEW_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_NODE_VIEW, GtkNodeViewClass))
struct _GtkNodeView
{
  GtkWidget parent_instance;

  GskRenderNode *node;
};

struct _GtkNodeViewClass
{
  GtkWidgetClass parent_class;
};

GType gtk_node_view_get_type (void) G_GNUC_CONST;


G_DEFINE_TYPE(GtkNodeView, gtk_node_view, GTK_TYPE_WIDGET)

static void
gtk_node_view_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkNodeView *self = GTK_NODE_VIEW (widget);

  if (self->node != NULL)
    {
      gtk_snapshot_push_clip (snapshot,
                              &GRAPHENE_RECT_INIT (
                                0, 0,
                                gtk_widget_get_width (widget), gtk_widget_get_height (widget)),
                              "nodeview clip");
      gtk_snapshot_append_node (snapshot, self->node);
      gtk_snapshot_pop (snapshot);
    }
}

static void
gtk_node_view_finalize (GObject *object)
{
  GtkNodeView *self = GTK_NODE_VIEW (object);

  if (self->node)
    gsk_render_node_unref (self->node);

  G_OBJECT_CLASS (gtk_node_view_parent_class)->finalize (object);
}

static void
gtk_node_view_init (GtkNodeView *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

static void
gtk_node_view_class_init (GtkNodeViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_node_view_finalize;

  widget_class->snapshot = gtk_node_view_snapshot;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *nodeview;
  char *contents;
  gsize len;
  GError *error = NULL;
  GBytes *bytes;
  graphene_rect_t node_bounds;

  if (argc != 2)
    {
      printf ("Usage: showrendernode NODEFILE\n");
      return 0;
    }

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  nodeview = g_object_new (GTK_TYPE_NODE_VIEW, NULL);

  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_container_add (GTK_CONTAINER (window), nodeview);

  g_file_get_contents (argv[1], &contents, &len, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      return -1;
    }

  bytes = g_bytes_new_take (contents, len);
  GTK_NODE_VIEW (nodeview)->node = gsk_render_node_deserialize (bytes, &error);
  g_bytes_unref (bytes);

  if (GTK_NODE_VIEW (nodeview)->node == NULL)
    {
      g_test_message ("Invalid node file: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return -1;
    }

  gsk_render_node_get_bounds (GTK_NODE_VIEW (nodeview)->node, &node_bounds);
  gtk_window_resize (GTK_WINDOW (window), 600, 500);//node_bounds.size.width, node_bounds.size.height);

  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
