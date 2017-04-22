
#include "gtkcenterboxprivate.h"

G_DEFINE_TYPE (GtkCenterBox, gtk_center_box, GTK_TYPE_WIDGET);


static void
gtk_center_box_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
  int min, nat, min_baseline, nat_baseline;

  gtk_widget_measure (self->start_widget,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  if (self->center_widget)
    {
      gtk_widget_measure (self->center_widget,
                          orientation,
                          for_size,
                          &min, &nat,
                          &min_baseline, &nat_baseline);

      /* XXX How are baselines even handled? */

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *minimum = *minimum + min;
          *natural = *natural + nat;
        }
      else /* GTK_ORIENTATION_VERTICAL */
        {
          *minimum = MAX (*minimum, min);
          *natural = MAX (*minimum, nat);
        }
    }

  gtk_widget_measure (self->end_widget,
                      orientation,
                      for_size,
                      &min, &nat,
                      &min_baseline, &nat_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = *minimum + min;
      *natural = *natural + nat;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      *minimum = MAX (*minimum, min);
      *natural = MAX (*minimum, nat);
    }

}

static void
gtk_center_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
  GtkAllocation child_allocation;
  GtkAllocation clip = *allocation;
  GtkAllocation child_clip;
  int start_size, end_size;
  int min, nat;

  GTK_WIDGET_CLASS (gtk_center_box_parent_class)->size_allocate (widget, allocation);


  // TODO: Allocate natural sizes if possible?

  /* Start Box */
  gtk_widget_measure (self->start_widget, GTK_ORIENTATION_HORIZONTAL,
                      allocation->height,
                      &min, &nat, NULL, NULL);
  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y;
  child_allocation.width = min;
  child_allocation.height = allocation->height;

  gtk_widget_size_allocate (self->start_widget, &child_allocation);
  gtk_widget_get_clip (self->start_widget, &child_clip);
  gdk_rectangle_union (&clip, &clip, &child_clip);
  start_size = child_allocation.width;


  /* End Box */
  gtk_widget_measure (self->end_widget, GTK_ORIENTATION_HORIZONTAL,
                      allocation->height,
                      &min, &nat, NULL, NULL);
  child_allocation.x = allocation->x + allocation->width - min;
  child_allocation.width = min;

  gtk_widget_size_allocate (self->end_widget, &child_allocation);
  gtk_widget_get_clip (self->end_widget, &child_clip);
  gdk_rectangle_union (&clip, &clip, &child_clip);
  end_size = child_allocation.width;

  /* Center Widget */
  if (self->center_widget)
    {
      gtk_widget_measure (self->center_widget, GTK_ORIENTATION_HORIZONTAL,
                          allocation->height,
                          &min, &nat, NULL, NULL);

      child_allocation.x = (allocation->width / 2) - (min / 2);

      /* Push in from start/end */
      if (start_size > child_allocation.x)
        child_allocation.x = start_size;
      else if (allocation->width - end_size < child_allocation.x + min)
        child_allocation.x = allocation->width - min - end_size;

      child_allocation.x += allocation->x;
      child_allocation.width = min;
      gtk_widget_size_allocate (self->center_widget, &child_allocation);
      gtk_widget_get_clip (self->center_widget, &child_clip);
      gdk_rectangle_union (&clip, &clip, &child_clip);
    }

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_center_box_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);

  gtk_widget_snapshot_child (widget, self->start_widget, snapshot);

  if (self->center_widget)
    gtk_widget_snapshot_child (widget, self->center_widget, snapshot);

  gtk_widget_snapshot_child (widget, self->end_widget, snapshot);
}

static void
gtk_center_box_class_init (GtkCenterBoxClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_center_box_measure;
  widget_class->size_allocate = gtk_center_box_size_allocate;
  widget_class->snapshot = gtk_center_box_snapshot;
}

static void
gtk_center_box_init (GtkCenterBox *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->start_widget = NULL;
  self->center_widget = NULL;
  self->end_widget = NULL;
}

GtkWidget *
gtk_center_box_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_CENTER_BOX, NULL));
}

void
gtk_center_box_set_start_widget (GtkCenterBox *self,
                                 GtkWidget    *child)
{
  if (self->start_widget)
    gtk_widget_unparent (self->start_widget);

  self->start_widget = child;
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_center_box_set_center_widget (GtkCenterBox *self,
                                 GtkWidget    *child)
{
  if (self->center_widget)
    gtk_widget_unparent (self->center_widget);

  self->center_widget = child;
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_center_box_set_end_widget (GtkCenterBox *self,
                               GtkWidget    *child)
{
  if (self->end_widget)
    gtk_widget_unparent (self->end_widget);

  self->end_widget = child;
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));
}

GtkWidget *
gtk_center_box_get_start_widget (GtkCenterBox *self)
{
  return self->start_widget;
}

GtkWidget *
gtk_center_box_get_center_widget (GtkCenterBox *self)
{
  return self->center_widget;
}

GtkWidget *
gtk_center_box_get_end_widget (GtkCenterBox *self)
{
  return self->end_widget;
}
