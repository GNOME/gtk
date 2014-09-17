#include <gtk/gtk.h>


static void     da_realize       (GtkWidget     *widget);
static void     da_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation);
static gboolean da_draw          (GtkWidget     *widget,
                                  cairo_t       *cr);

typedef GtkDrawingArea DArea;
typedef GtkDrawingAreaClass DAreaClass;

G_DEFINE_TYPE (DArea, da, GTK_TYPE_WIDGET)

static void
da_class_init (DAreaClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->realize = da_realize;
  widget_class->size_allocate = da_size_allocate;
  widget_class->draw = da_draw;
}

static void
da_init (DArea *darea)
{
  gtk_widget_set_has_window (GTK_WIDGET (darea), TRUE);
}

GtkWidget*
da_new (void)
{
  return g_object_new (da_get_type (), NULL);
}

static void
da_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_SUBSURFACE;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_register_window (widget, window);
  gtk_widget_set_window (widget, window);
}

static void
da_size_allocate (GtkWidget     *widget,
                  GtkAllocation *allocation)
{
  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);
}

static gboolean
da_draw (GtkWidget *widget,
         cairo_t   *cr)
{
  cairo_set_source_rgb (cr, 1.0, 0.0, 0.0); 
  cairo_paint (cr);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label, *box, *widget;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (window), box);
  label = gtk_label_new ("Test test");
  gtk_container_add (GTK_CONTAINER (box), label);
  widget = da_new ();
  gtk_widget_set_size_request (widget, 100, 100);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
