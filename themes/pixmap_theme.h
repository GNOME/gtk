#include <gtk/gtk.h>
#include <gdk_imlib.h>

/* internals */

typedef struct
  {
    guint refcount;
    GList *img_list;
  }
ThemeData;

enum
  {
    TOKEN_IMAGE = G_TOKEN_LAST + 1,
    TOKEN_FUNCTION,
    TOKEN_FILE,
    TOKEN_STRETCH,
    TOKEN_RECOLORABLE,
    TOKEN_BORDER,
    TOKEN_DETAIL,
    TOKEN_STATE,
    TOKEN_SHADOW,
    TOKEN_GAP_SIDE,
    TOKEN_GAP_FILE,
    TOKEN_GAP_BORDER,
    TOKEN_GAP_START_FILE,
    TOKEN_GAP_START_BORDER,
    TOKEN_GAP_END_FILE,
    TOKEN_GAP_END_BORDER,
    TOKEN_OVERLAY_FILE,
    TOKEN_OVERLAY_BORDER,
    TOKEN_OVERLAY_STRETCH,
    TOKEN_ARROW_DIRECTION,
    TOKEN_D_HLINE,
    TOKEN_D_VLINE,
    TOKEN_D_SHADOW,
    TOKEN_D_POLYGON,
    TOKEN_D_ARROW,
    TOKEN_D_DIAMOND,
    TOKEN_D_OVAL,
    TOKEN_D_STRING,
    TOKEN_D_BOX,
    TOKEN_D_FLAT_BOX,
    TOKEN_D_CHECK,
    TOKEN_D_OPTION,
    TOKEN_D_CROSS,
    TOKEN_D_RAMP,
    TOKEN_D_TAB,
    TOKEN_D_SHADOW_GAP,
    TOKEN_D_BOX_GAP,
    TOKEN_D_EXTENSION,
    TOKEN_D_FOCUS,
    TOKEN_D_SLIDER,
    TOKEN_D_ENTRY,
    TOKEN_D_HANDLE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_TOP,
    TOKEN_UP,
    TOKEN_BOTTOM,
    TOKEN_DOWN,
    TOKEN_LEFT,
    TOKEN_RIGHT,
    TOKEN_NORMAL,
    TOKEN_ACTIVE,
    TOKEN_PRELIGHT,
    TOKEN_SELECTED,
    TOKEN_INSENSITIVE,
    TOKEN_NONE,
    TOKEN_IN,
    TOKEN_OUT,
    TOKEN_ETCHED_IN,
    TOKEN_ETCHED_OUT,
    TOKEN_ORIENTATION,
    TOKEN_HORIZONTAL,
    TOKEN_VERTICAL,
  };

struct theme_image
  {
    guint               refcount;

    guint               function;
    gchar               recolorable;
    gchar              *detail;
    gchar              *file;
    GdkImlibBorder      border;
    gchar               stretch;
    gchar              *overlay_file;
    GdkImlibBorder      overlay_border;
    gchar               overlay_stretch;
    gchar              *gap_file;
    GdkImlibBorder      gap_border;

    gchar              *gap_start_file;
    GdkImlibBorder      gap_start_border;
    gchar              *gap_end_file;
    GdkImlibBorder      gap_end_border;
    
    gchar               __gap_side;
    gint                gap_side;
    gchar               __orientation;
    GtkOrientation      orientation;
    gchar               __state;
    GtkStateType        state;
    gchar               __shadow;
    GtkShadowType       shadow;
    gchar               __arrow_direction;
    GtkArrowType        arrow_direction;
  };

