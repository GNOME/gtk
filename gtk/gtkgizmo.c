
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
gtk_gizmo_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->allocate_func)
    self->allocate_func (self, width, height, baseline);
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

static gboolean
gtk_gizmo_contains (GtkWidget *widget,
                    double     x,
                    double     y)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (self->contains_func)
    return self->contains_func (self, x, y);
  else
    return GTK_WIDGET_CLASS (gtk_gizmo_parent_class)->contains (widget, x, y);
}

static void
gtk_gizmo_finalize (GObject *object)
{
  GtkGizmo *self = GTK_GIZMO (object);
  GtkWidget *widget;

  widget = _gtk_widget_get_first_child (GTK_WIDGET (self));
  while (widget != NULL)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (widget);

      gtk_widget_unparent (widget);

      widget = next;
    }

  G_OBJECT_CLASS (gtk_gizmo_parent_class)->finalize (object);
}

static void
gtk_gizmo_class_init (GtkGizmoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_gizmo_finalize;

  widget_class->measure = gtk_gizmo_measure;
  widget_class->size_allocate = gtk_gizmo_size_allocate;
  widget_class->snapshot = gtk_gizmo_snapshot;
  widget_class->contains = gtk_gizmo_contains;
}

static void
gtk_gizmo_init (GtkGizmo *self)
{
}

GtkWidget *
gtk_gizmo_new (const char              *css_name,
               GtkGizmoMeasureFunc  measure_func,
               GtkGizmoAllocateFunc allocate_func,
               GtkGizmoSnapshotFunc snapshot_func,
               GtkGizmoContainsFunc contains_func)
{
  GtkGizmo *gizmo = GTK_GIZMO (g_object_new (GTK_TYPE_GIZMO,
                                             "css-name", css_name,
                                             NULL));

  gizmo->measure_func  = measure_func;
  gizmo->allocate_func = allocate_func;
  gizmo->snapshot_func = snapshot_func;
  gizmo->contains_func = contains_func;

  return GTK_WIDGET (gizmo);
}
