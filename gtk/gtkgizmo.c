
#include "gtkgizmoprivate.h"
#include "gtkwidgetprivate.h"


G_DEFINE_TYPE (GtkGizmo, gtk_gizmo, GTK_TYPE_WIDGET);

static void
gtk_gizmo_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->measure_func)
    self->measure_func (self, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_gizmo_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkGizmo *self = GTK_GIZMO (widget);
  GtkAllocation clip = *allocation;

  if (self->allocate_func)
    self->allocate_func (self,
                         allocation,
                         gtk_widget_get_allocated_baseline (widget),
                         &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_gizmo_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->snapshot_func)
    self->snapshot_func (self, snapshot);
  else
    GTK_WIDGET_CLASS (gtk_gizmo_parent_class)->snapshot (widget, snapshot);
}

static void
gtk_gizmo_class_init (GtkGizmoClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_gizmo_measure;
  widget_class->size_allocate = gtk_gizmo_size_allocate;
  widget_class->snapshot = gtk_gizmo_snapshot;
}

static void
gtk_gizmo_init (GtkGizmo *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

GtkWidget *
gtk_gizmo_new (const char              *css_name,
               GtkGizmoMeasureFunc  measure_func,
               GtkGizmoAllocateFunc allocate_func,
               GtkGizmoSnapshotFunc snapshot_func)
{
  GtkGizmo *gizmo = GTK_GIZMO (g_object_new (GTK_TYPE_GIZMO,
                                             "css-name", css_name,
                                             NULL));

  gizmo->measure_func  = measure_func;
  gizmo->allocate_func = allocate_func;
  gizmo->snapshot_func = snapshot_func;

  return GTK_WIDGET (gizmo);
}
