/**
 * SECTION:gtkgridlayoutchild
 * @Title: GtkGridLayoutChild
 * @Short_description: Layout properties for children of GtkGridLayout
 *
 * GtkGridLayoutChild is a #GtkLayoutChild class used to store layout
 * properties that apply to children of a #GtkWidget using the #GtkGridLayout
 * layout manager.
 *
 * You can retrieve a GtkGridLayout instance bound to a #GtkWidget by
 * using gtk_layout_manager_get_layout_child().
 */

#include "config.h"

#include "gtkgridlayoutprivate.h"

G_DEFINE_TYPE (GtkGridLayoutChild, gtk_grid_layout_child, GTK_TYPE_LAYOUT_CHILD)

enum {
  PROP_LEFT_ATTACH = 1,
  PROP_TOP_ATTACH,
  PROP_COLUMN_SPAN,
  PROP_ROW_SPAN,

  N_PROPERTIES
};

static GParamSpec *layout_child_props[N_PROPERTIES];

static void
gtk_grid_layout_child_get_property (GObject    *gobject,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkGridLayoutChild *self = GTK_GRID_LAYOUT_CHILD (gobject);

  switch (property_id)
    {
    case PROP_LEFT_ATTACH:
      g_value_set_int (value, CHILD_LEFT (grid_child));
      break;

    case PROP_TOP_ATTACH:
      g_value_set_int (value, CHILD_TOP (grid_child));
      break;

    case PROP_COLUMN_SPAN:
      g_value_set_int (value, CHILD_COL_SPAN (grid_child));
      break;

    case PROP_ROW_SPAN:
      g_value_set_int (value, CHILD_ROW_SPAN (grid_child));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_child_set_property (GObject      *gobject,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkGridLayoutChild *self = GTK_GRID_LAYOUT_CHILD (gobject);

  switch (property_id)
    {
    case PROP_LEFT_ATTACH:
      CHILD_LEFT (grid_child) = g_value_get_int (value);
      break;

    case PROP_TOP_ATTACH:
      CHILD_TOP (grid_child) = g_value_get_int (value);
      break;

    case PROP_COLUMN_SPAN:
      CHILD_COL_SPAN (grid_child) = g_value_get_int (value);
      break;

    case PROP_ROW_SPAN:
      CHILD_ROW_SPAN (grid_child) = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
      break;
    }

  gtk_layout_manager_layout_changed (gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (self)));
}

static void
gtk_grid_layout_child_class_init (GtkGridLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_grid_layout_child_set_property;
  gobject_class->get_property = gtk_grid_layout_child_get_property;

  layout_child_props[PROP_LEFT_ATTACH] =
    g_param_spec_int ("left-attach",
                      P_("Left attachment"),
                      P_("The column number to attach the left side of the child to"),
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE);

  layout_child_props[PROP_TOP_ATTACH] =
    g_param_spec_int ("top-attach",
                      P_("Top attachment"),
                      P_("The row number to attach the top side of a child widget to"),
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE);

  layout_child_props[PROP_COLUMN_SPAN] =
    g_param_spec_int ("column-span",
                      P_("Column Span"),
                      P_("The number of columns that a child spans"),
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE);

  layout_child_props[PROP_ROW_SPAN] =
    g_param_spec_int ("row-span",
                      P_("Row Span"),
                      P_("The number of rows that a child spans"),
                      1, G_MAXINT, 1,
                      GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, layout_child_props);
}

static void
gtk_grid_layout_child_init (GtkGridLayoutChild *self)
{
  CHILD_LEFT (self) = 0;
  CHILD_TOP (self) = 0;
  CHILD_COLUMN_SPAN (self) = 1;
  CHILD_ROW_SPAN (self) = 1;
}

int
gtk_grid_layout_child_get_left_attach (GtkGridLayoutChild *grid_child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (grid_child), 0);

  return CHILD_LEFT (grid_child);
}

int
gtk_grid_layout_child_get_top_attach (GtkGridLayoutChild *grid_child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (grid_child), 0);

  return CHILD_TOP (grid_child);
}

int
gtk_grid_layout_child_get_width (GtkGridLayoutChild *grid_child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (grid_child), 0);

  return CHILD_WIDTH (grid_child);
}

int
gtk_grid_layout_child_get_height (GtkGridLayoutChild *grid_child)
{
  g_return_val_if_fail (GTK_IS_GRID_LAYOUT_CHILD (grid_child), 0);

  return CHILD_HEIGHT (grid_child);
}
