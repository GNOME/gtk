
#include<gtk/gtk.h>

typedef struct _GtkFlip      GtkFlip;
typedef struct _GtkFlipClass GtkFlipClass;

#define GTK_TYPE_FLIP           (gtk_flip_get_type ())
#define GTK_FLIP(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_FLIP, GtkFlip))
#define GTK_FLIP_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_FLIP, GtkFlipClass))
struct _GtkFlip
{
  GtkWidget parent_instance;

  GtkWidget *child;
  guint flipped : 1;
};

struct _GtkFlipClass
{
  GtkWidgetClass parent_class;
};

GType gtk_flip_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE(GtkFlip, gtk_flip, GTK_TYPE_WIDGET);

#define OPPOSITE_ORIENTATION(o) (1 - o)

static void
gtk_flip_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GtkFlip *self = (GtkFlip *)widget;

  if (self->flipped)
    gtk_widget_measure (self->child, OPPOSITE_ORIENTATION (orientation), for_size,
                        minimum, natural, NULL, NULL);
  else
    gtk_widget_measure (self->child, orientation, for_size, minimum, natural, NULL, NULL);

}

static void
gtk_flip_size_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  GtkFlip *self = (GtkFlip *)widget;
  int child_width;
  int child_height;
  graphene_matrix_t transform;

  gtk_widget_measure (self->child, GTK_ORIENTATION_HORIZONTAL, -1, &child_width, NULL, NULL, NULL);
  gtk_widget_measure (self->child, GTK_ORIENTATION_VERTICAL, -1, &child_height, NULL, NULL, NULL);

  if (self->flipped)
    {
      graphene_matrix_init_rotate (&transform, 90, graphene_vec3_z_axis ());
      graphene_matrix_translate (&transform,
                                 &GRAPHENE_POINT3D_INIT (child_height, 0, 0));

      /*gtk_widget_size_allocate_transformed (self->child,*/
                                            /*child_width,*/
                                            /*child_height,*/
                                            /*-1, &transform);*/
      /*return;*/
    }
  else
    {
      graphene_matrix_init_identity (&transform);
    }

  gtk_widget_size_allocate_transformed (self->child,
                                        child_width,
                                        child_height,
                                        /*MAX (width, child_width),*/
                                        /*MAX (height, child_height),*/
                                        -1, &transform);
}

static void
gtk_flip_finalize (GObject *object)
{
  GtkFlip *self = (GtkFlip *)object;

  gtk_widget_unparent (self->child);

  G_OBJECT_CLASS (gtk_flip_parent_class)->finalize (object);
}

static void
gtk_flip_init (GtkFlip *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);
}

static void
gtk_flip_class_init (GtkFlipClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_flip_finalize;

  widget_class->measure = gtk_flip_measure;
  widget_class->size_allocate = gtk_flip_size_allocate;
}

static GtkWidget *
gtk_flip_new (GtkWidget *child)
{
  GtkFlip *s = GTK_FLIP (g_object_new (GTK_TYPE_FLIP, NULL));

  s->child = child;
  gtk_widget_set_parent (child, GTK_WIDGET (s));

  return GTK_WIDGET (s);
}

static void
gtk_flip_flip (GtkFlip *self)
{
  self->flipped = !self->flipped;
  gtk_widget_queue_resize (GTK_WIDGET (self));
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


static void
flip_button_clicked_cb (GtkButton *source,
                        gpointer   user_data)
{
  GtkFlip *flip = user_data;

  gtk_flip_flip (flip);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *flip;
  GtkWidget *to_flip;
  GtkWidget *hb;
  GtkWidget *flip_button;


  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hb = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), hb);
  flip_button = gtk_button_new_with_label ("Flip");
  gtk_container_add (GTK_CONTAINER (hb), flip_button);
  to_flip = create_widget_factory_content ();
  g_object_set (G_OBJECT (to_flip), "margin", 0, NULL);
  flip = gtk_flip_new (to_flip);
  g_signal_connect (flip_button, "clicked", G_CALLBACK (flip_button_clicked_cb), flip);

  gtk_container_add (GTK_CONTAINER (window), flip);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
