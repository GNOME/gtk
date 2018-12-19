#include "config.h"

#include "gtkboxlayout.h"

#include "gtkorientable.h"
#include "gtksizerequest.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkboxlayout
 * @Title: GtkBoxLayout
 * @Short_description: Layout manager for placing all children in a single row or column
 *
 * ...
 */

struct _GtkBoxLayout
{
  GtkLayoutManager parent_instance;

  gboolean homogeneous;
  guint spacing;
  GtkOrientation orientation;
};

G_DEFINE_TYPE_WITH_CODE (GtkBoxLayout, gtk_box_layout, GTK_TYPE_LAYOUT_MANAGER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

enum {
  PROP_HOMOGENEOUS = 1,
  PROP_SPACING,

  PROP_ORIENTATION,
  N_PROPS = PROP_ORIENTATION
};

static GParamSpec *box_layout_props[N_PROPS];

static void
gtk_box_layout_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_HOMOGENEOUS:
      gtk_box_layout_set_homogeneous (box_layout, g_value_get_boolean (value));
      break;

    case PROP_SPACING:
      gtk_box_layout_set_spacing (box_layout, g_value_get_int (value));
      break;

    case PROP_ORIENTATION:
      gtk_orientable_set_orientation (GTK_ORIENTABLE (box_layout),
                                      g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROP_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_box_layout_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, box_layout->homogeneous);
      break;

    case PROP_SPACING:
      g_value_set_int (value, box_layout->spacing);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, box_layout->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROP_ID (gobject, prop_id, pspec);
    }
}

static GtkSizeRequestMode
gtk_box_layout_get_request_mode (GtkLayoutManager *layout_manager,
                                 GtkWidget        *widget)
{
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (layout_manager);

  return box_layout->orientation == GTK_ORIENTATION_HORIZONTAL
    ? GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT
    : GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_box_layout_compute_size (GtkBoxLayout *box_layout,
                             GtkWidget    *widget,
                             int           for_size,
                             int          *minimum,
                             int          *natural)
{
  GtkWidget *child;
  int n_visible_children = 0;
  int required_min = 0, required_nat = 0;
  int largest_min = 0, largest_nat = 0;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min = 0;
      int child_nat = 0;

      if (!_gtk_widget_get_visible (child))
        continue;

      gtk_widget_measure (child, box_layout->orientation,
                          for_size,
                          &child_min, &child_nat,
                          NULL, NULL);

      largest_min = MAX (largest_min, child_min);
      largest_nat = MAX (largest_nat, child_nat);

      required_min += child_min;
      required_nat += child_nat;

      n_visible_children += 1;
    }

  if (n_visible_children > 0)
    {
      if (box_layout->homogeneous)
        {
          required_min = largest_min * n_visible_children;
          required_nat = largest_nat * n_visible_children;
        }

      required_min += (n_visible_children - 1) * spacing;
      required_nat += (n_visible_children - 1) * spacing;
    }

  *minimum = required_min;
  *natural = required_nat;
}

static void
gtk_box_layout_compute_opposite_size (GtkBoxLayout *box_layout,
                                      GtkWidget    *widget,
                                      int           for_size,
                                      int          *minimum,
                                      int          *natural,
                                      int          *min_baseline,
                                      int          *nat_baseline)
{
}

static void
gtk_box_layout_measure (GtkLayoutManager *layout_manager,
                        GtkWidget        *widget,
                        GtkOrientation    orientation,
                        int               for_size,
                        int              *minimum,
                        int              *natural,
                        int              *min_baseline,
                        int              *nat_baseline)
{
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (layout_manager);

  if (box_layout->orientation != orientation)
    {
      gtk_box_layout_compute_opposite_size (box_layout, widget, for_size,
                                            minimum, natural,
                                            min_baseline, nat_baseline);
    }
  else
    {
      gtk_box_layout_compute_size (box_layout, widget, for_size,
                                   minimum, natural);
    }
}

static void
gtk_box_layout_allocate (GtkLayoutManager *layout_manager,
                         GtkWidget        *widget,
                         int               width,
                         int               height,
                         int               baseline)
{
}

static void
gtk_box_layout_class_init (GtkBoxLayoutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER (klass);

  gobject_class->set_property = gtk_box_layout_set_property;
  gobject_class->get_property = gtk_box_layout_get_property;

  layout_manager_class->get_request_mode = gtk_box_layout_get_request_mode;
  layout_manager_class->measure = gtk_box_layout_measure;
  layout_manager_class->allocate = gtk_box_layout_allocate;

  box_layout_props[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous", "Homogeneous", "Distribute space homogeneously",
                          FALSE,
                          GTK_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY);

  box_layout_props[PROP_SPACING] =
    g_param_spec_int ("spacing", "Spacing", "Spacing between widgets",
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE |
                      G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, box_layout_props);
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");
}

static void
gtk_box_layout_init (GtkBoxLayout *self)
{
  self->homogeneous = FALSE;
  self->spacing = 0;
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
}

GtkLayoutManager *
gtk_box_layout_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_BOX_LAYOUT,
                       "orientation", orientation,
                       NULL);
}

void
gtk_box_layout_set_homogeneous (GtkBoxLayout *box_layout,
                                gboolean      homogeneous)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  if (box_layout->homogeneous == (!!homogeneous))
    return;

  box_layout->homogeneous = !!homogeneous;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_HOMOGENEOUS]);
}

gboolean
gtk_box_layout_get_homogeneous (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), FALSE);

  return box_layout->homogeneous;
}

void
gtk_box_layout_set_spacing (GtkBoxLayout *box_layout,
                            guint         spacing)
{
  g_return_if_fail (GTK_IS_BOX_LAYOUT (box_layout));

  if (box_layout->spacing == spacing)
    return;

  box_layout->spacing = spacing;

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (box_layout));
  g_object_notify_by_pspec (G_OBJECT (box_layout), box_layout_props[PROP_SPACING]);
}

guint
gtk_box_layout_get_spacing (GtkBoxLayout *box_layout)
{
  g_return_val_if_fail (GTK_IS_BOX_LAYOUT (box_layout), 0);

  return box_layout->spacing;
}
