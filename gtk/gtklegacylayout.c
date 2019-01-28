/*< private >
 * SECTION:gtklegacylayout
 * @Title: GtkLegacyLayout
 * @Short_description: A legacy layout manager
 *
 * #GtkLegacyLayout is a convenience type meant to be used as a transition
 * mechanism between #GtkContainers implementing a layout policy, and
 * #GtkLayoutManager classes.
 *
 * A #GtkLegacyLayout uses closures matching to the old #GtkWidget virtual
 * functions for size negotiation, to ease the porting towards the
 * corresponding #GtkLayoutManager virtual functions.
 */

#include "config.h"

#include "gtklegacylayoutprivate.h"

struct _GtkLegacyLayout
{
  GtkLayoutManager parent_instance;

  GtkLegacyRequestModeFunc request_mode_func;
  GtkLegacyMeasureFunc measure_func;
  GtkLegacyAllocateFunc allocate_func;
};

G_DEFINE_TYPE (GtkLegacyLayout, gtk_legacy_layout, GTK_TYPE_LAYOUT_MANAGER)

static GtkSizeRequestMode
gtk_legacy_layout_get_request_mode (GtkLayoutManager *manager,
                                    GtkWidget        *widget)
{
  GtkLegacyLayout *self = GTK_LEGACY_LAYOUT (manager);

  if (self->request_mode_func != NULL)
    return self->request_mode_func (widget);

  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_legacy_layout_measure (GtkLayoutManager *manager,
                           GtkWidget        *widget,
                           GtkOrientation    orientation,
                           int               for_size,
                           int              *minimum_p,
                           int              *natural_p,
                           int              *minimum_baseline_p,
                           int              *natural_baseline_p)
{
  GtkLegacyLayout *self = GTK_LEGACY_LAYOUT (manager);
  int minimum = 0, natural = 0;
  int minimum_baseline = -1, natural_baseline = -1;

  if (self->measure_func != NULL)
    self->measure_func (widget, orientation, for_size,
                        &minimum, &natural,
                        &minimum_baseline, &natural_baseline);

  if (minimum_p != NULL)
    *minimum_p = minimum;
  if (natural_p != NULL)
    *natural_p = natural;

  if (minimum_baseline_p != NULL)
    *minimum_baseline_p = minimum_baseline;
  if (natural_baseline_p != NULL)
    *natural_baseline_p = natural_baseline;
}

static void
gtk_legacy_layout_allocate (GtkLayoutManager *manager,
                            GtkWidget        *widget,
                            int               width,
                            int               height,
                            int               baseline)
{
  GtkLegacyLayout *self = GTK_LEGACY_LAYOUT (manager);

  if (self->allocate_func != NULL)
    self->allocate_func (widget, width, height, baseline);
}

static void
gtk_legacy_layout_class_init (GtkLegacyLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_request_mode = gtk_legacy_layout_get_request_mode;
  layout_class->measure = gtk_legacy_layout_measure;
  layout_class->allocate = gtk_legacy_layout_allocate;
}

static void
gtk_legacy_layout_init (GtkLegacyLayout *self)
{
}

/*< private >
 * gtk_legacy_layout_new:
 * @request_mode: (nullable): a function to retrieve
 *   the #GtkSizeRequestMode of the widget using the layout
 * @measure: (nullable): a fucntion to measure the widget
 *   using the layout
 * @allocate: (nullable): a function to allocate the children
 *   of the widget using the layout
 *
 * Creates a new legacy layout manager.
 *
 * Legacy layout managers map to the old #GtkWidget size negotiation
 * virtual functions, and are meant to be used during the transition
 * from layout containers to layout manager delegates.
 *
 * Returns: (transfer full): the newly created #GtkLegacyLayout
 */
GtkLayoutManager *
gtk_legacy_layout_new (GtkLegacyRequestModeFunc request_mode,
                       GtkLegacyMeasureFunc measure,
                       GtkLegacyAllocateFunc allocate)
{
  GtkLegacyLayout *self = g_object_new (GTK_TYPE_LEGACY_LAYOUT, NULL);

  self->request_mode_func = request_mode;
  self->measure_func = measure;
  self->allocate_func = allocate;

  return GTK_LAYOUT_MANAGER (self);
}
