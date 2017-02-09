#include <gtk/gtk.h>



typedef struct _GtkFocusWidget      GtkFocusWidget;
typedef struct _GtkFocusWidgetClass GtkFocusWidgetClass;

#define GTK_TYPE_FOCUS_WIDGET           (gtk_focus_widget_get_type ())
#define GTK_FOCUS_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_FOCUS_WIDGET, GtkFocusWidget))
#define GTK_FOCUS_WIDGET_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_FOCUS_WIDGET, GtkFocusWidgetClass))
#define GTK_IS_FOCUS_WIDGET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_TYPE_FOCUS_WIDGET))
#define GTK_IS_FOCUS_WIDGET_CLASS(cls)   (G_TYPE_CHECK_CLASS_TYPE(cls, GTK_TYPE_FOCUS_WIDGET))
#define GTK_FOCUS_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(obj, GTK_TYPE_FOCUS_WIDGET, GtkFocusWidgetClass))

#define SPACING 30

struct _GtkFocusWidget
{
  GtkWidget parent_instance;

  union {
    struct {
      GtkWidget *child1;
      GtkWidget *child2;
      GtkWidget *child3;
      GtkWidget *child4;
    };
    GtkWidget* children[4];
  };
};

struct _GtkFocusWidgetClass
{
  GtkWidgetClass parent_class;
};

GType gtk_focus_widget_get_type (void) G_GNUC_CONST;


G_DEFINE_TYPE(GtkFocusWidget, gtk_focus_widget, GTK_TYPE_WIDGET)

static void
gtk_focus_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GtkFocusWidget *self = GTK_FOCUS_WIDGET (widget);
  int child_width  = (allocation->width  - (3 * SPACING)) / 2;
  int child_height = (allocation->height - (3 * SPACING)) / 2;
  GtkAllocation child_alloc;

  child_alloc.x = SPACING + allocation->x;
  child_alloc.y = SPACING + allocation->y;
  child_alloc.width = child_width;
  child_alloc.height = child_height;

  gtk_widget_size_allocate (self->child1, &child_alloc);

  child_alloc.x += SPACING + child_width;

  gtk_widget_size_allocate (self->child2, &child_alloc);

  child_alloc.y += SPACING + child_height;

  gtk_widget_size_allocate (self->child4, &child_alloc);

  child_alloc.x -= SPACING + child_width;

  gtk_widget_size_allocate (self->child3, &child_alloc);
}

static void
gtk_focus_widget_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkFocusWidget *self = GTK_FOCUS_WIDGET (widget);
  int min, nat;
  int i;

  *minimum = 0;
  *natural = 0;

  for (i = 0; i < 4; i ++)
    {
      gtk_widget_measure (self->children[i], orientation, for_size,
                          &min, &nat, NULL, NULL);

      *minimum = MAX (*minimum, min);
      *natural = MAX (*natural, nat);
    }

  *minimum *= 2;
  *natural *= 2;

  *minimum += SPACING * 3;
  *natural += SPACING * 3;
}

static void
gtk_focus_widget_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  GtkFocusWidget *self = GTK_FOCUS_WIDGET (widget);

  gtk_widget_snapshot_child (widget, self->child1, snapshot);
  gtk_widget_snapshot_child (widget, self->child2, snapshot);
  gtk_widget_snapshot_child (widget, self->child3, snapshot);
  gtk_widget_snapshot_child (widget, self->child4, snapshot);
}

static void
gtk_focus_widget_finalize (GObject *object)
{
  GtkFocusWidget *self = GTK_FOCUS_WIDGET (object);

  gtk_widget_unparent (self->child1);
  gtk_widget_unparent (self->child2);
  gtk_widget_unparent (self->child3);
  gtk_widget_unparent (self->child4);

  G_OBJECT_CLASS (gtk_focus_widget_parent_class)->finalize (object);
}

static void
gtk_focus_widget_init (GtkFocusWidget *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->child1 = gtk_button_new_with_label ("1");
  gtk_widget_set_parent (self->child1, GTK_WIDGET (self));
  self->child2 = gtk_button_new_with_label ("2");
  gtk_widget_set_parent (self->child2, GTK_WIDGET (self));
  self->child3 = gtk_button_new_with_label ("3");
  gtk_widget_set_parent (self->child3, GTK_WIDGET (self));
  self->child4 = gtk_button_new_with_label ("4");
  gtk_widget_set_parent (self->child4, GTK_WIDGET (self));
}

static void
gtk_focus_widget_class_init (GtkFocusWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_focus_widget_finalize;

  widget_class->snapshot = gtk_focus_widget_snapshot;
  widget_class->measure = gtk_focus_widget_measure;
  widget_class->size_allocate = gtk_focus_widget_size_allocate;
}

int
main()
{
  GtkWidget *window;
  GtkWidget *widget;
  gtk_init ();


  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  widget = g_object_new (GTK_TYPE_FOCUS_WIDGET, NULL);

  gtk_container_add (GTK_CONTAINER (window), widget);
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (window);

  gtk_main ();
}
