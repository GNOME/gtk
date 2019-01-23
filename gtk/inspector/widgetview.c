
#include "widgetview.h"
#include "gtklabel.h"
#include "gtkpicture.h"
#include "gtkbox.h"

G_DEFINE_TYPE (GtkWidgetView, gtk_widget_view, GTK_TYPE_WIDGET);

static void
gtk_widget_view_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkWidgetView *self = GTK_WIDGET_VIEW (widget);

  gtk_widget_measure (self->box, orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_widget_view_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  GtkWidgetView *self = GTK_WIDGET_VIEW (widget);

  gtk_widget_size_allocate (self->box,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, -1);
}

static void
gtk_widget_view_class_init (GtkWidgetViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_widget_view_measure;
  widget_class->size_allocate = gtk_widget_view_size_allocate;
}


static void
gtk_widget_view_init (GtkWidgetView *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);

  self->typename_label = gtk_label_new ("");
  self->paintable_picture = gtk_picture_new ();
  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  gtk_container_add (GTK_CONTAINER (self->box), self->typename_label);
  gtk_container_add (GTK_CONTAINER (self->box), self->paintable_picture);

  gtk_widget_set_parent (self->box, GTK_WIDGET (self));
}


GtkWidget *
gtk_widget_view_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_WIDGET_VIEW, NULL);
}

void
gtk_widget_view_set_inspected_widget (GtkWidgetView *self,
                                      GtkWidget     *inspected)
{
  char typename_buffer[512];
  GdkPaintable *paintable;

  g_set_object (&self->inspected, inspected);

  if (!self->inspected)
    {
      gtk_label_set_label (GTK_LABEL (self->typename_label), "NULL");
      return;
    }

  g_snprintf (typename_buffer, sizeof (typename_buffer),
              "%s", G_OBJECT_TYPE_NAME (inspected));
  gtk_label_set_label (GTK_LABEL (self->typename_label), typename_buffer);

  paintable = gtk_widget_paintable_new (inspected);
  gtk_picture_set_paintable (GTK_PICTURE (self->paintable_picture), paintable);
  g_object_unref (paintable);
}
