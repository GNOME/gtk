#include <gtk/gtk.h>

const char *css =
"resizewidget {"
"  background-color: yellow;"
"}"
"resizewidget button:first-child {"
"  border-top-left-radius: 50%;"
"}"
"resizewidget button:last-child {"
"  border-bottom-right-radius: 50%;"
"}";


typedef struct _GtkResizeWidget      GtkResizeWidget;
typedef struct _GtkResizeWidgetClass GtkResizeWidgetClass;

#define GTK_TYPE_RESIZE_WIDGET           (gtk_resize_widget_get_type ())
#define GTK_RESIZE_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_RESIZE_WIDGET, GtkResizeWidget))
#define GTK_RESIZE_WIDGET_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_RESIZE_WIDGET, GtkResizeWidgetClass))
#define GTK_IS_RESIZE_WIDGET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_TYPE_RESIZE_WIDGET))
#define GTK_IS_RESIZE_WIDGET_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE(cls, GTK_TYPE_RESIZE_WIDGET))
#define GTK_RESIZE_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(obj, GTK_TYPE_RESIZE_WIDGET, GtkResizeWidgetClass))

struct _GtkResizeWidget
{
  GtkWidget parent_instance;

  GtkWidget *buttons[2];
};

struct _GtkResizeWidgetClass
{
  GtkWidgetClass parent_class;
};

GType gtk_resize_widget_get_type (void) G_GNUC_CONST;


G_DEFINE_TYPE(GtkResizeWidget, gtk_resize_widget, GTK_TYPE_WIDGET)


static void
gtk_resize_widget_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
  GtkResizeWidget *self = GTK_RESIZE_WIDGET (widget);
  int i;

  /* Just add everything up */

  *minimum = 0;
  *natural = 0;

  for (i = 0; i < 2; i ++)
    {
      int m, n;

      gtk_widget_measure (self->buttons[i], orientation, for_size,
                          &m, &n, minimum_baseline, natural_baseline);

      /* Ignore minimum size. You know why. */
      if (i == 0)
        *minimum += m;

      *natural += n;
    }
}


static void
gtk_resize_widget_size_allocate (GtkWidget           *widget,
                                 const GtkAllocation *allocation,
                                 int                  baseline,
                                 GtkAllocation       *out_clip)
{
  static int k;
  GtkResizeWidget *self = GTK_RESIZE_WIDGET (widget);
  GtkAllocation child_alloc;
  GtkAllocation clip;
  int m, n;
  int remaining_width = allocation->width;

  g_message ("%s: %i", __FUNCTION__, k++);


  gtk_widget_measure (self->buttons[0], GTK_ORIENTATION_HORIZONTAL, -1,
                      &m, &n, NULL, NULL);

  child_alloc.x = 0;
  child_alloc.y = 0;
  child_alloc.width = m;
  child_alloc.height = allocation->height;
  gtk_widget_size_allocate (self->buttons[0], &child_alloc, -1, &clip);

  remaining_width -= child_alloc.width;

  gtk_widget_measure (self->buttons[1], GTK_ORIENTATION_HORIZONTAL, -1,
                      &m, &n, NULL, NULL);

  if (remaining_width < m)
    {
      gtk_widget_hide (self->buttons[1]);
    }
  else
    {
      gtk_widget_show (self->buttons[1]);
      gtk_widget_measure (self->buttons[1], GTK_ORIENTATION_HORIZONTAL, -1,
                          &m, &n, NULL, NULL);

      /* Still? */
      if (remaining_width > m)
        {
          child_alloc.x += child_alloc.width;
          child_alloc.width = m;
          gtk_widget_size_allocate (self->buttons[1], &child_alloc, -1, &clip);
        }
      else
        {
          /* Shame. */
          gtk_widget_hide (self->buttons[1]);
        }

    }

}


static void
gtk_resize_widget_init (GtkResizeWidget *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

  self->buttons[0] = gtk_button_new_with_label ("First");
  self->buttons[1] = gtk_button_new_with_label ("Second");

  gtk_widget_set_parent (self->buttons[0], GTK_WIDGET (self));
  gtk_widget_set_parent (self->buttons[1], GTK_WIDGET (self));
}

static void
gtk_resize_widget_class_init (GtkResizeWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_resize_widget_measure;
  widget_class->size_allocate = gtk_resize_widget_size_allocate;

  gtk_widget_class_set_css_name (widget_class, "resizewidget");
}


int
main()
{
  GtkWidget *window;
  GtkWidget *widget;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  widget = g_object_new (GTK_TYPE_RESIZE_WIDGET, NULL);

  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

  gtk_container_add (GTK_CONTAINER (window), widget);
  g_signal_connect (window, "close-request", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (window);

  gtk_main ();
}
