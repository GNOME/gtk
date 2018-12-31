
#include<gtk/gtk.h>

typedef struct _GtkShrink      GtkShrink;
typedef struct _GtkShrinkClass GtkShrinkClass;

#define GTK_TYPE_SHRINK           (gtk_shrink_get_type ())
#define GTK_SHRINK(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_SHRINK, GtkShrink))
#define GTK_SHRINK_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_SHRINK, GtkShrinkClass))
struct _GtkShrink
{
  GtkWidget parent_instance;

  GtkWidget *child;
};

struct _GtkShrinkClass
{
  GtkWidgetClass parent_class;
};

GType gtk_shrink_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE(GtkShrink, gtk_shrink, GTK_TYPE_WIDGET);


static void
gtk_shrink_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GtkShrink *self = (GtkShrink *)widget;

  *minimum = 0;

  gtk_widget_measure (self->child, orientation, for_size, NULL, natural, NULL, NULL);
}

static void
gtk_shrink_size_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  GtkShrink *self = (GtkShrink *)widget;
  int child_width;
  int child_height;
  float scale_x;
  float scale_y;
  graphene_matrix_t transform;

  gtk_widget_measure (self->child, GTK_ORIENTATION_HORIZONTAL, -1, &child_width, NULL, NULL, NULL);
  gtk_widget_measure (self->child, GTK_ORIENTATION_VERTICAL, -1, &child_height, NULL, NULL, NULL);

  if (width < child_width)
    scale_x = (float)width / (float)child_width;
  else
    scale_x = 1.0f;

  if (height < child_height)
    scale_y = (float)height / (float)child_height;
  else
    scale_y = 1.0f;

  graphene_matrix_init_scale (&transform, scale_x, scale_y, 1.0f);

  gtk_widget_size_allocate_transformed (self->child,
                                        MAX (width, child_width),
                                        MAX (height, child_height),
                                        -1, &transform);
}

static void
gtk_shrink_finalize (GObject *object)
{
  GtkShrink *self = (GtkShrink *)object;

  gtk_widget_unparent (self->child);

  G_OBJECT_CLASS (gtk_shrink_parent_class)->finalize (object);
}

static void
gtk_shrink_init (GtkShrink *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);
}

static void
gtk_shrink_class_init (GtkShrinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shrink_finalize;

  widget_class->measure = gtk_shrink_measure;
  widget_class->size_allocate = gtk_shrink_size_allocate;
}

static GtkWidget *
gtk_shrink_new (GtkWidget *child)
{
  GtkShrink *s = GTK_SHRINK (g_object_new (GTK_TYPE_SHRINK, NULL));

  s->child = child;
  gtk_widget_set_parent (child, GTK_WIDGET (s));

  return GTK_WIDGET (s);
}



/* Stub definition of MyTextView which is used in the
 * widget-factory.ui file. We just need this so the
 * test keeps working
 */
typedef struct
{
  GtkTextView tv;
} MyTextView;

typedef GtkTextViewClass MyTextViewClass;

G_DEFINE_TYPE (MyTextView, my_text_view, GTK_TYPE_TEXT_VIEW)

static void
my_text_view_init (MyTextView *tv) {}

static void
my_text_view_class_init (MyTextViewClass *tv_class) {}

/* Copied from tests/scrolling-performance.c */
GtkWidget *
create_widget_factory_content (void)
{
  GError *error = NULL;
  GtkBuilder *builder;
  GtkWidget *result;

  g_type_ensure (my_text_view_get_type ());
  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder,
                             "../demos/widget-factory/widget-factory.ui",
                             &error);
  if (error != NULL)
    g_error ("Failed to create widgets: %s", error->message);

  result = GTK_WIDGET (gtk_builder_get_object (builder, "box1"));
  g_object_ref (result);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (result)),
                        result);
  g_object_unref (builder);

  return result;
}



int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *shrink;
  GtkWidget *to_shrink;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  to_shrink = create_widget_factory_content ();
  g_object_set (G_OBJECT (to_shrink), "margin", 0, NULL);
  shrink = gtk_shrink_new (to_shrink);

  gtk_container_add (GTK_CONTAINER (window), shrink);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
