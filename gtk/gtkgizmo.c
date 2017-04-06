
#include "gtkgizmoprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscustomgadgetprivate.h"


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

  gtk_css_gadget_get_preferred_size (self->gadget,
                                     orientation,
                                     for_size,
                                     minimum, natural,
                                     minimum_baseline, natural_baseline);
}

static void
gtk_gizmo_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkGizmo *self = GTK_GIZMO (widget);
  GtkAllocation clip;

  GTK_WIDGET_CLASS (gtk_gizmo_parent_class)->size_allocate (widget, allocation);

  gtk_css_gadget_allocate (self->gadget,
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

  gtk_css_gadget_snapshot (self->gadget, snapshot);
}

static void
gtk_gizmo_measure_contents (GtkCssGadget   *gadget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline,
                            gpointer        user_data)
{
  GtkGizmo *self = GTK_GIZMO (gtk_css_gadget_get_owner (gadget));

  if (self->measure_func)
    self->measure_func (self, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_gizmo_allocate_contents (GtkCssGadget        *gadget,
                             const GtkAllocation *allocation,
                             int                  baseline,
                             GtkAllocation       *out_clip,
                             gpointer             user_data)

{
  GtkGizmo *self = GTK_GIZMO (gtk_css_gadget_get_owner (gadget));

  if (self->allocate_func)
    self->allocate_func (self,
                         allocation,
                         baseline,
                         out_clip);
}

static gboolean
gtk_gizmo_snapshot_contents (GtkCssGadget *gadget,
                             GtkSnapshot  *snapshot,
                             int           x,
                             int           y,
                             int           width,
                             int           height,
                             gpointer      user_data)
{
  GtkGizmo *self = GTK_GIZMO (gtk_css_gadget_get_owner (gadget));

  if (self->snapshot_func)
    return self->snapshot_func (self, snapshot);

  return FALSE;
}

static void
gtk_gizmo_finalize (GObject *obj)
{
  GtkGizmo *self = GTK_GIZMO (obj);

  g_clear_object (&self->gadget);

  G_OBJECT_CLASS (gtk_gizmo_parent_class)->finalize (obj);
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
}

static void
gtk_gizmo_init (GtkGizmo *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->gadget = gtk_css_custom_gadget_new_for_node (gtk_widget_get_css_node (GTK_WIDGET (self)),
                                                     GTK_WIDGET (self),
                                                     gtk_gizmo_measure_contents,
                                                     gtk_gizmo_allocate_contents,
                                                     gtk_gizmo_snapshot_contents,
                                                     NULL,
                                                     NULL);
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
