#include "config.h"

#include "gtkgridlayoutprivate.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkenums.h"
#include "gtkintl.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

#include <string.h>

/**
 * SECTION:gtkgridlayout
 * @Short_description: Pack widgets in rows and columns
 * @Title: GtkGridLayout
 * @See_also: #GtkBoxLayout
 *
 * GtkGridLayout is a layout manager which arranges its child widgets in
 * rows and columns, with arbitrary positions and horizontal/vertical spans.
 *
 * Children can span multiple rows or columns. The behaviour of GtkGridLayout
 * when several children occupy the same grid cell is undefined.
 *
 * If you do not specify the attach points for each widget, GtkGridLayout
 * behaves like a #GtkBoxLayout, and will lay out children in a single row
 * or column, using the the #GtkOrientable:orientation property. However, if
 * all you want is a single row or column, then #GtkBoxLayout is the preferred
 * layout manager.
 */

typedef struct _GridRowProperties       GridRowProperties;
typedef struct _GridLineData            GridLineData;
typedef struct _GridLine                GridLine;
typedef struct _GridLines               GridLines;

struct _GridRowProperties
{
  int row;
  GtkBaselinePosition baseline_position;
};

/* A GtkGridLineData struct contains row/column specific parts
 * of the grid.
 */
struct _GridLineData
{
  gint16 spacing;
  guint homogeneous : 1;
};

#define ROWS(layout)    (&(layout)->linedata[GTK_ORIENTATION_HORIZONTAL])
#define COLUMNS(layout) (&(layout)->linedata[GTK_ORIENTATION_VERTICAL])

/* A GridLine struct represents a single row or column
 * during size requests
 */
struct _GridLine
{
  int minimum;
  int natural;
  int minimum_above;
  int minimum_below;
  int natural_above;
  int natural_below;

  int position;
  int allocation;
  int allocated_baseline;

  guint need_expand : 1;
  guint expand      : 1;
  guint empty       : 1;
};

struct _GridLines
{
  GridLine *lines;
  int min, max;
};

struct _GtkGridRequest
{
  GtkGridLayout *grid;

  GridLines lines[2];
};

struct _GtkGridLayout
{
  GtkLayoutManager parent_instance;

  GList *row_properties;

  GtkOrientation orientation;
  int baseline_row;

  GridLineData linedata[2];
};

enum
{
  PROP_ROW_SPACING = 1,
  PROP_COLUMN_SPACING,
  PROP_ROW_HOMOGENEOUS,
  PROP_COLUMN_HOMOGENEOUS,
  PROP_BASELINE_ROW,

  N_PROPERTIES,

  /* from GtkOrientable */
  PROP_ORIENTATION
};

static const GridRowProperties grid_row_properties_default = {
  0,
  GTK_BASELINE_POSITION_CENTER
};

static GParamSpec *grid_layout_props[N_PROPERTIES];

G_DEFINE_TYPE_WITH_CODE (GtkGridLayout, gtk_grid_layout, GTK_TYPE_LAYOUT_MANAGER,
  G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_grid_layout_create_layout_child (GtkLayoutManager *manager,
                                     GtkWidget        *child)
{
  return g_object_new (GTK_TYPE_GRID_LAYOUT_CHILD,
                       "layout-manager", manager,
                       "child-widget", child,
                       NULL);
}

static void
gtk_grid_layout_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (gtk_grid_layout_parent_class)->finalize (gobject);
}

static void
gtk_grid_layout_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;

    case PROP_ROW_SPACING:
      g_value_set_int (value, COLUMNS (self)->spacing);
      break;

    case PROP_COLUMN_SPACING:
      g_value_set_int (value, ROWS (self)->spacing);
      break;

    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, COLUMNS (self)->homogeneous);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, ROWS (self)->homogeneous);
      break;

    case PROP_BASELINE_ROW:
      g_value_set_int (value, self->baseline_row);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_set_orientation (GtkGridLayout  *self,
                                 GtkOrientation  orientation)
{
  GtkLayoutManager *layout_manager = GTK_LAYOUT_MANAGER (self);
  GtkWidget *widget;

  if (self->orientation == orientation)
    return;

  self->orientation = orientation;

  widget = gtk_layout_manager_get_widget (GTK_LAYOUT_MANAGER (self));
  if (widget != NULL && GTK_IS_ORIENTABLE (widget))
    _gtk_orientable_set_style_classes (GTK_ORIENTABLE (widget));

  gtk_layout_manager_layout_changed (layout_manager);
}

static void
gtk_grid_layout_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkGridLayout *self = GTK_GRID_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_grid_layout_set_orientation (self, g_value_get_enum (value));
      break;

    case PROP_ROW_SPACING:
      gtk_grid_layout_set_row_spacing (self, g_value_get_int (value));
      break;

    case PROP_COLUMN_SPACING:
      gtk_grid_layout_set_column_spacing (self, g_value_get_int (value));
      break;

    case PROP_ROW_HOMOGENEOUS:
      gtk_grid_layout_set_row_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      gtk_grid_layout_set_column_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_BASELINE_ROW:
      gtk_grid_layout_set_baseline_row (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_grid_layout_class_init (GtkGridLayoutClass *klass)
{
  GtkLayoutManagerClass *manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  manager_class->create_layout_child = gtk_grid_layout_create_layout_child;

  gobject_class->set_property = gtk_grid_layout_set_property;
  gobject_class->get_property = gtk_grid_layout_get_property;

  g_object_class_install_properties (gobject_class, N_PROPERTIES, grid_layout_props);
}

static void
gtk_grid_layout_init (GtkGridLayout *self)
{
}

GtkLayoutManager *
gtk_grid_layout_new (void)
{
  return g_object_new (GTK_TYPE_GRID_LAYOUT, NULL);
}

void
gtk_grid_layout_set_row_homogeneous (GtkGridLayout *grid_layout,
                                     gboolean       homogeneous)
{
}

gboolean
gtk_grid_layout_get_row_homogeneous (GtkGridLayout *grid_layout)
{
}

void
gtk_grid_layout_set_row_spacing (GtkGridLayout *grid_layout,
                                 guint          spacing)
{
}

guint
gtk_grid_layout_get_row_spacing (GtkGridLayout *grid_layout)
{
}

void
gtk_grid_layout_set_column_homogeneous (GtkGridLayout *grid_layout,
                                        gboolean       homogeneous)
{
}

gboolean
gtk_grid_layout_get_column_homogeneous (GtkGridLayout *grid_layout)
{
}

void
gtk_grid_layout_set_column_spacing (GtkGridLayout *grid_layout,
                                    guint          spacing)
{
}

guint
gtk_grid_layout_get_column_spacing (GtkGridLayout *grid_layout)
{
}

void
gtk_grid_layout_set_row_baseline_position (GtkGridLayout       *grid_layout,
                                           int                  row,
                                           GtkBaselinePosition  pos)
{
}

GtkBaselinePosition
gtk_grid_layout_get_row_baseline_position (GtkGridLayout *grid_layout,
                                           int            row)
{
}

void
gtk_grid_layout_set_baseline_row (GtkGridLayout *grid_layout,
                                  int            row)
{
}

int
gtk_grid_layout_get_baseline_row (GtkGridLayout *grid_layout)
{
}
